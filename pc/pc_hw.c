// pc_hw.c: a fake GBA for the PC build.
//  * the memory map is real: EWRAM/IWRAM/IO/PAL/VRAM/OAM/GPIO/SRAM are mapped at their GBA addresses, so the game's own
//    "*(volatile u16*)0x06000000" code runs untouched.
//  * time is a counter, not a clock: each read of VCOUNT moves the beam one line on. 228 reads = 1 frame. On line 160 the
//    vblank "happens": keys for the next frame are set, the game's interrupt handler runs, shots are taken.
//  * the picture: a software copy of the GBA's mode 3/4/5 bitmap layer + sprites (regular and rotating) + the fade/blend unit.
// What is NOT here (see README): per-scanline DMA tricks (the kart's ground), windows, mosaic, tile backgrounds, sound output.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "pc_hw.h"
#include "platform/pc_plat.h"
#include <math.h>

void (*pc_irq_fn)(void);

#define EWRAM 0x02000000u
#define IWRAM 0x03000000u
#define IOREG 0x04000000u
#define PALRAM 0x05000000u
#define VRAMB 0x06000000u
#define OAMB  0x07000000u
#define GPIOB 0x08000000u
#define SRAMB 0x0E000000u
#define R16(a) (*(volatile uint16_t*)(uintptr_t)(a))
#define R32(a) (*(volatile uint32_t*)(uintptr_t)(a))

static const char *g_sav;
static long g_frame, g_max_frames=-1;
static int g_line;
static int g_window;            // 1 = a window drives the game (keys, picture, sound, pacing come from pc/platform), 0 = headless script mode
void pc_set_window_mode(int on){ g_window=on; }
int  pc_window_mode(void){ return g_window; }

static int map_at(uint32_t a, size_t n){
#ifdef _WIN32
    { void *p=VirtualAlloc((void*)(uintptr_t)a,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); if(!p||(uintptr_t)p!=a){ fprintf(stderr,"pc_hw: cannot map %08x\n",a); return 0; } return 1; }
#else
    void *p = mmap((void*)(uintptr_t)a, n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);
    if(p==MAP_FAILED || (uintptr_t)p!=a){ fprintf(stderr,"pc_hw: cannot map %08x (%zu bytes)\n",a,n); return 0; }
    return 1;
#endif
}
static uint64_t g_saved_hash;
uint64_t pc_hash_sram(void);
int pc_hw_init(const char *sav_path){
    g_sav = sav_path;
    if(!map_at(EWRAM,0x40000) || !map_at(IWRAM,0x8000) || !map_at(IOREG,0x1000) || !map_at(PALRAM,0x1000) ||
       !map_at(VRAMB,0x20000) || !map_at(OAMB,0x1000) || !map_at(GPIOB,0x10000) || !map_at(SRAMB,0x10000)) return 0;
    R16(IOREG+0x130)=0x03FF;                                  // no key pressed
    R16(IOREG+0x20)=0x100; R16(IOREG+0x26)=0x100;             // BG2 starts unscaled (the same start the emulator the game was tested on gives it)
    memset((void*)(uintptr_t)SRAMB,0xFF,0x8000);              // blank SRAM reads 0xFF, like a fresh cartridge
    if(sav_path){ FILE *f=fopen(sav_path,"rb"); if(f){ size_t n=fread((void*)(uintptr_t)SRAMB,1,0x8000,f); (void)n; fclose(f); } }
    g_saved_hash=pc_hash_sram();
    pc_psg_reset();
    return 1;
}
static int save_write(const char *path){            // write to a temp file, then swap it in: a crash while saving never leaves half a save
    char tmp[1100]; snprintf(tmp,sizeof tmp,"%s.tmp",path);
    FILE *f=fopen(tmp,"wb"); if(!f) return 0;
    size_t n=fwrite((void*)(uintptr_t)SRAMB,1,0x8000,f); int ok = n==0x8000 && fflush(f)==0; fclose(f);
    if(!ok){ remove(tmp); return 0; }
    remove(path); if(rename(tmp,path)!=0){ remove(tmp); return 0; }
    return 1;
}
void pc_set_save_path(const char *path){ g_sav=path; }
int pc_time_pct=100;                                    // the game speed in percent (UNIVERSE LAWS: GAME SPEED); only the window front end paces by it
const char *pc_sav_path(void){ return g_sav; }          // the save file, for the files kept beside it (laws, recipes)
void pc_save_flush(int force){
    if(!g_sav) return;
    uint64_t h=pc_hash_sram();
    if(!force && h==g_saved_hash) return;
    if(save_write(g_sav)) g_saved_hash=h;
}
void pc_hw_finish(void){
    if(!g_sav) return;
    pc_save_flush(1);
}
void pc_quit_now(void){ pc_hw_finish(); fflush(stdout); exit(0); }

// ---------------------------------------------------------------- DMA channel 3 (immediate copies and fills)
void pc_dma3(uint32_t v){
    uint32_t sad=R32(IOREG+0xD4), dad=R32(IOREG+0xD8), cnt=v&0xFFFF, ctl=v>>16;
    if(!(ctl&0x8000)) return;
    if(((ctl>>12)&3)!=0) return;                              // only "start now"; the timed kinds (HBlank/VBlank) are not done here
    if(!cnt) cnt=0x10000;
    if(sad<0x1000 || dad<0x1000){ fprintf(stderr,"pc_hw: DMA with bad address %08x -> %08x\n",sad,dad); return; }
    int word=(ctl>>10)&1, dc=(ctl>>5)&3, sc=(ctl>>7)&3, st=word?4:2;
    for(uint32_t i=0;i<cnt;i++){
        if(word) *(volatile uint32_t*)(uintptr_t)dad = *(volatile uint32_t*)(uintptr_t)sad;
        else     *(volatile uint16_t*)(uintptr_t)dad = *(volatile uint16_t*)(uintptr_t)sad;
        sad += sc==0||sc==3 ? st : sc==1 ? -st : 0;
        dad += dc==0||dc==3 ? st : dc==1 ? -st : 0;
    }
}

// ---------------------------------------------------------------- the picture
static uint16_t rgbpix(uint16_t c){ return c&0x7FFF; }
static uint16_t bright(uint16_t c, int ev, int up){
    int r=c&31, g=(c>>5)&31, b=(c>>10)&31;
    if(up){ r+=(31-r)*ev/16; g+=(31-g)*ev/16; b+=(31-b)*ev/16; } else { r-=r*ev/16; g-=g*ev/16; b-=b*ev/16; }
    return (uint16_t)(r|(g<<5)|(b<<10));
}
static uint16_t alpha(uint16_t a, uint16_t b, int ea, int eb){
    int r=((a&31)*ea+(b&31)*eb)>>4, g=(((a>>5)&31)*ea+((b>>5)&31)*eb)>>4, bl=(((a>>10)&31)*ea+((b>>10)&31)*eb)>>4;
    if(r>31)r=31; if(g>31)g=31; if(bl>31)bl=31; return (uint16_t)(r|(g<<5)|(bl<<10));
}
static uint16_t objc[160][240]; static uint8_t objp[160][240];   // sprite pixel colour; priority (4 = none); bit 4 set = semi-transparent
static void draw_sprites(int one_d){
    static const uint8_t dim[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
    memset(objp,4,sizeof objp);
    for(int i=127;i>=0;i--){
        uint32_t o=OAMB+i*8; uint16_t a0=R16(o), a1=R16(o+2), a2=R16(o+4);
        int am=(a0>>8)&3; if(am==2) continue;
        int gm=(a0>>10)&3; if(gm==2) continue;                    // (object window sprites are not drawn)
        int shape=(a0>>14)&3, size=(a1>>14)&3; if(shape==3) continue;
        int w=dim[shape][size][0], h=dim[shape][size][1];
        int aff=am&1, dbl=am==3; int bw=dbl?w*2:w, bh=dbl?h*2:h;
        int y0=a0&255; if(y0>=160) y0-=256; int x0=a1&511; if(x0>=240) x0-=512;
        int is8=(a0>>13)&1, prio=(a2>>10)&3, pal=(a2>>12)&15, tile=a2&1023;
        int pa=0x100,pb=0,pc=0,pd=0x100;
        if(aff){ int g=(a1>>9)&31; uint32_t p=OAMB+g*32; pa=(int16_t)R16(p+6); pb=(int16_t)R16(p+14); pc=(int16_t)R16(p+22); pd=(int16_t)R16(p+30); }
        int hf=!aff&&((a1>>12)&1), vf=!aff&&((a1>>13)&1);
        for(int sy=0;sy<bh;sy++){ int py=y0+sy; if(py<0||py>=160) continue;
            for(int sx=0;sx<bw;sx++){ int px=x0+sx; if(px<0||px>=240) continue;
                int tx,ty;
                if(aff){ int dx=sx-bw/2, dy=sy-bh/2; tx=((pa*dx+pb*dy)>>8)+w/2; ty=((pc*dx+pd*dy)>>8)+h/2; if(tx<0||ty<0||tx>=w||ty>=h) continue; }
                else { tx=hf?w-1-sx:sx; ty=vf?h-1-sy:sy; }
                int units=is8?2:1; int t=tile + (ty>>3)*(one_d ? (w>>3)*units : 32) + (tx>>3)*units;
                uint32_t addr=VRAMB+0x10000+(uint32_t)t*32 + (ty&7)*(is8?8:4) + ((tx&7)>>(is8?0:1));
                if(addr>=VRAMB+0x18000) continue;
                int idx=*(volatile uint8_t*)(uintptr_t)addr; if(!is8){ idx = (tx&1) ? idx>>4 : idx&15; } if(!idx) continue;
                if((objp[py][px]&7)<4 && (objp[py][px]&3)<=prio) continue;      // a lower-numbered sprite of equal or better priority is there
                uint16_t c = R16(PALRAM+0x200 + 2*(is8? idx : pal*16+idx));
                objc[py][px]=rgbpix(c); objp[py][px]=(uint8_t)(prio | (gm==1?16:0));
            } }
    }
}

// ---------------------------------------------------------------- the picture, line by line (HBlank DMA)
// The kart races' ground is drawn by the chip itself: at every HBlank three DMA channels copy a table row into DISPCNT, BLDALPHA and BG2's affine
// registers. A real chip does that while it scans; here the picture is made once a frame, so build_lines() plays the scan: it copies the I/O block,
// and for each of the 160 lines records the registers the line is drawn with, then runs the HBlank transfers (into the copy, never into the real
// registers). The bitmap layer's internal reference point is reloaded whenever BG2X/BG2Y is written, else it moves on by PB/PD each line.
typedef struct { uint16_t dc, bldcnt, bldalpha, bldy; int16_t pa,pb,pc,pd; int32_t rx,ry; } LineSt;
static LineSt lst[160]; static int lst_dma;
static int32_t sx28(uint32_t v){ return (int32_t)(v<<4)>>4; }
static void build_lines(void){
    uint8_t io[0x60]; memcpy(io,(void*)(uintptr_t)IOREG,0x60);
    #define I16(o) (*(uint16_t*)(io+(o)))
    #define I32(o) (*(uint32_t*)(io+(o)))
    struct { uint32_t sad,dad,dad0,cnt,ctl; int on; } ch[4]; int any=0;
    for(int i=0;i<4;i++){ uint32_t b=IOREG+0xB0+12*i; uint32_t ctl=R16(b+10); ch[i].ctl=ctl; ch[i].on=(ctl&0x8000) && ((ctl>>12)&3)==2;
        ch[i].sad=R32(b); ch[i].dad=ch[i].dad0=R32(b+4); ch[i].cnt=R16(b+8); if(!ch[i].cnt) ch[i].cnt= i==3 ? 0x10000 : 0x4000; if(ch[i].on) any++; }
    lst_dma=any>0;
    int32_t ix=sx28(I32(0x28)), iy=sx28(I32(0x2C));
    for(int y=0;y<160;y++){
        LineSt *L=&lst[y]; L->dc=I16(0); L->bldcnt=I16(0x50); L->bldalpha=I16(0x52); L->bldy=I16(0x54);
        L->pa=(int16_t)I16(0x20); L->pb=(int16_t)I16(0x22); L->pc=(int16_t)I16(0x24); L->pd=(int16_t)I16(0x26); L->rx=ix; L->ry=iy;
        ix+=L->pb; iy+=L->pd;
        if(!any || y>=159) continue;
        for(int i=0;i<4;i++){ if(!ch[i].on) continue;
            int word=(ch[i].ctl>>10)&1, dc=(ch[i].ctl>>5)&3, sc=(ch[i].ctl>>7)&3, st=word?4:2;
            for(uint32_t n=0;n<ch[i].cnt;n++){
                uint32_t sa=ch[i].sad, da=ch[i].dad;
                if(sa>=0x1000 && da>=IOREG && da<IOREG+0x60-(word?3:1)){
                    uint32_t v= word ? *(volatile uint32_t*)(uintptr_t)sa : *(volatile uint16_t*)(uintptr_t)sa; uint32_t o=(da-IOREG)&~(uint32_t)(st-1);
                    if(word) I32(o)=v; else I16(o)=(uint16_t)v;
                    if(o>=0x28 && o<0x2C) ix=sx28(I32(0x28)); if(o>=0x2C && o<0x30) iy=sx28(I32(0x2C));
                }
                ch[i].sad += sc==0||sc==3 ? st : sc==1 ? -st : 0;
                ch[i].dad += dc==0||dc==3 ? st : dc==1 ? -st : 0;
            }
            if(dc==3) ch[i].dad=ch[i].dad0;
            if(!((ch[i].ctl>>9)&1)) ch[i].on=0;
        }
    }
    #undef I16
    #undef I32
}
static uint16_t screen[160][240];
static void compose(void){
    build_lines();
    uint16_t dc=R16(IOREG), mode=dc&7; int one_d=(dc>>6)&1;
    uint16_t bd=rgbpix(R16(PALRAM));
    if(dc&0x80){ for(int y=0;y<160;y++) for(int x=0;x<240;x++) screen[y][x]=0x7FFF; return; }      // forced blank = white
    int obj_on=(dc>>12)&1, bg2_on=(dc>>10)&1;
    if(obj_on) draw_sprites(one_d); else memset(objp,4,sizeof objp);
    int bgprio=R16(IOREG+0x0C)&3;
    for(int y=0;y<160;y++){ const LineSt *L=&lst[y]; uint16_t ldc=L->dc; int lmode=ldc&7, lbg2=(ldc>>10)&1;
      uint16_t bldcnt=L->bldcnt, bldalpha=L->bldalpha; int ev=L->bldy&31; if(ev>16) ev=16;
      int bmode=(bldcnt>>6)&3; int ea=bldalpha&31, eb=(bldalpha>>8)&31; if(ea>16)ea=16; if(eb>16)eb=16;
      int pa=L->pa, pc=L->pc; int32_t rx=L->rx, ry=L->ry; uint32_t base = VRAMB + ((ldc&0x10) ? 0xA000 : 0);
      for(int x=0;x<240;x++){
        int have=0; uint16_t bc=bd;
        if(bg2_on && lbg2 && lmode>=3 && lmode<=5){ mode=lmode; dc=(dc&~0x10)|(ldc&0x10);
            int tx=(int)((rx + pa*x)>>8), ty=(int)((ry + pc*x)>>8);
            int bw = mode==5?160:240, bh = mode==5?128:160;
            if(tx>=0&&ty>=0&&tx<bw&&ty<bh){
                if(mode==4){ int idx=*(volatile uint8_t*)(uintptr_t)(base+ty*240+tx); if(idx){ bc=rgbpix(R16(PALRAM+idx*2)); have=1; } else { bc=bd; } }
                else { bc=rgbpix(R16(VRAMB + (mode==5&&(dc&0x10)?0xA000:0) + (ty*bw+tx)*2)); have=1; }
            }
        }
        // layers: 0 = BG2, 1 = sprite, 2 = backdrop
        uint16_t top=bd, second=bd; int tl=2, sl=2; int o=(objp[y][x]&7)<4;
        int oprio = o ? (objp[y][x]&3) : 9; int semi = o && (objp[y][x]&16);
        if(o && have){ if(oprio<=bgprio){ top=objc[y][x]; tl=1; second=bc; sl=0; } else { top=bc; tl=0; second=objc[y][x]; sl=1; } }
        else if(o){ top=objc[y][x]; tl=1; second=bd; sl=2; }
        else if(have){ top=bc; tl=0; second=bd; sl=2; }
        static const int bit[3]={2,4,5};
        int t1=(bldcnt>>bit[tl])&1, t2=(bldcnt>>(8+bit[sl]))&1;
        uint16_t out=top;
        if(semi && tl==1 && t2) out=alpha(top,second,ea,eb);
        else if(bmode==1 && t1 && t2) out=alpha(top,second,ea,eb);
        else if(bmode==2 && t1) out=bright(top,ev,1);
        else if(bmode==3 && t1) out=bright(top,ev,0);
        screen[y][x]=out;
    } }
}
void pc_render(uint8_t *rgb){
    compose();
    for(int y=0;y<160;y++) for(int x=0;x<240;x++){ uint16_t c=screen[y][x]; int r=c&31, g=(c>>5)&31, b=(c>>10)&31; uint8_t *p=rgb+(y*240+x)*3;
        p[0]=(uint8_t)(r<<3|r>>2); p[1]=(uint8_t)(g<<3|g>>2); p[2]=(uint8_t)(b<<3|b>>2); }
}
int pc_write_ppm(const char *path, int scale){
    if(getenv("PC_NATIVE")){                                   // test: make the picture with the native path (pc_compose_native) at scale x 240x160
        int W=240*scale, H=160*scale; uint32_t *b=malloc((size_t)W*H*4); pc_compose_native(b,W,H,atoi(getenv("PC_NATIVE"))>1);
        FILE *f=fopen(path,"wb"); if(!f){ free(b); return 0; } fprintf(f,"P6 %d %d 255\n",W,H);
        for(size_t i=0;i<(size_t)W*H;i++){ uint8_t p[3]={(uint8_t)(b[i]>>16),(uint8_t)(b[i]>>8),(uint8_t)b[i]}; fwrite(p,1,3,f); } fclose(f); free(b); return 1; }
    static uint8_t rgb[240*160*3]; pc_render(rgb);
    FILE *f=fopen(path,"wb"); if(!f) return 0;
    fprintf(f,"P6 %d %d 255\n",240*scale,160*scale);
    for(int y=0;y<160*scale;y++) for(int x=0;x<240*scale;x++) fwrite(rgb+((y/scale)*240+(x/scale))*3,1,3,f);
    fclose(f); return 1;
}

// ---------------------------------------------------------------- the picture, made at the window's own size
// pc_compose_native() does what compose() does, but for every pixel of a w x h picture instead of 240 x 160: the bitmap layer is sampled with
// its affine matrix per output pixel, sprites (also the rotating ones) are drawn from the sprite table at that size, and the blend unit runs on
// the output pixels. So rotation, zoom and sprite edges are made at the real resolution, not made small and stretched. The 3D views are still
// made by the game's own code at its own grid (see NATIVE_PLAN.md); this does not invent detail they do not have.
// w = 240*k, h = 160*k gives sharp, even pixels. Other sizes give uneven pixel widths unless smooth=1, which blends the bitmap layer's texel
// edges over one output pixel ("sharp bilinear": flat inside a texel, soft only at the edge).
static uint16_t *nv_oc; static uint8_t *nv_op; static size_t nv_cap;
static uint16_t bgres[160][240]; static uint8_t bghave[160][240];
static uint16_t bgres2[160][240]; static uint8_t bghave2[160][240];   // page 1, for the line-by-line kart ground
static float *nv_fx, *nv_fy; static int nv_fcap;
static void nv_resolve_bg(int mode, uint32_t base, uint16_t dc, int *bw, int *bh, uint16_t bd){
    *bw = mode==5?160:240; *bh = mode==5?128:160;
    for(int y=0;y<*bh;y++) for(int x=0;x<*bw;x++){
        if(mode==4){ int idx=*(volatile uint8_t*)(uintptr_t)(base+y*240+x); bghave[y][x]=idx!=0; bgres[y][x]= idx? rgbpix(R16(PALRAM+idx*2)) : bd; }
        else { bgres[y][x]=rgbpix(R16(VRAMB + (mode==5&&(dc&0x10)?0xA000:0) + (y*(*bw)+x)*2)); bghave[y][x]=1; }
    }
}
static uint16_t mix15(uint16_t a, uint16_t b, int t){      // t = 0..16: weight of b
    int r=((a&31)*(16-t)+(b&31)*t+8)>>4, g=(((a>>5)&31)*(16-t)+((b>>5)&31)*t+8)>>4, bl=(((a>>10)&31)*(16-t)+((b>>10)&31)*t+8)>>4;
    return (uint16_t)(r|(g<<5)|(bl<<10));
}
void pc_compose_native(uint32_t *out, int W, int H, int smooth){
    if(W<240||H<160){ W=240; H=160; }
    build_lines();
    size_t need=(size_t)W*H; if(need>nv_cap){ free(nv_oc); free(nv_op); nv_oc=malloc(need*2); nv_op=malloc(need); nv_cap=need; }
    if(W+H>nv_fcap){ free(nv_fx); free(nv_fy); nv_fcap=W+H; nv_fx=malloc(sizeof(float)*(W+H)); nv_fy=nv_fx+W; }
    uint16_t dc=R16(IOREG), mode=dc&7; int one_d=(dc>>6)&1;
    uint16_t bd=rgbpix(R16(PALRAM));
    uint8_t lut5[32]; for(int i=0;i<32;i++) lut5[i]=(uint8_t)(i<<3|i>>2);
    #define XRGB(c) ((uint32_t)lut5[(c)&31]<<16 | (uint32_t)lut5[((c)>>5)&31]<<8 | lut5[((c)>>10)&31])
    if(dc&0x80){ for(size_t i=0;i<need;i++) out[i]=0xFFFFFF; return; }
    const double sx=240.0/W, sy=160.0/H;                       // game pixels per output pixel
    // ---- sprites
    memset(nv_op,4,need);
    if((dc>>12)&1){
        static const uint8_t dim[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
        for(int i=0;i<128;i++){                                  // lowest number first: on a tie of priority, the lower sprite number stays on top (as on the chip)
            uint32_t o=OAMB+i*8; uint16_t a0=R16(o), a1=R16(o+2), a2=R16(o+4);
            int am=(a0>>8)&3; if(am==2) continue;
            int gm=(a0>>10)&3; if(gm==2) continue;
            int shape=(a0>>14)&3, size=(a1>>14)&3; if(shape==3) continue;
            int w=dim[shape][size][0], h=dim[shape][size][1];
            int aff=am&1, dbl=am==3; int bw=dbl?w*2:w, bh=dbl?h*2:h;
            int y0=a0&255; if(y0>=160) y0-=256; int x0=a1&511; if(x0>=240) x0-=512;
            int is8=(a0>>13)&1, prio=(a2>>10)&3, pal=(a2>>12)&15, tile=a2&1023;
            double pa=1,pb=0,pc=0,pd=1;
            if(aff){ int g=(a1>>9)&31; uint32_t p=OAMB+g*32; pa=(int16_t)R16(p+6)/256.0; pb=(int16_t)R16(p+14)/256.0; pc=(int16_t)R16(p+22)/256.0; pd=(int16_t)R16(p+30)/256.0; }
            int hf=!aff&&((a1>>12)&1), vf=!aff&&((a1>>13)&1);
            int ox0=(int)floor(x0/sx), ox1=(int)ceil((x0+bw)/sx), oy0=(int)floor(y0/sy), oy1=(int)ceil((y0+bh)/sy);
            if(ox0<0)ox0=0; if(oy0<0)oy0=0; if(ox1>W)ox1=W; if(oy1>H)oy1=H;
            int units=is8?2:1;
            for(int Y=oy0;Y<oy1;Y++){
                double gy=Y*sy;                                  // the top edge of this output pixel, in game pixels
                for(int X=ox0;X<ox1;X++){
                    double gx=X*sx; int tx,ty;
                    if(aff){ double dx=gx-x0-bw/2.0, dy=gy-y0-bh/2.0; double fx=pa*dx+pb*dy+w/2.0, fy=pc*dx+pd*dy+h/2.0;
                             if(fx<0||fy<0) continue; tx=(int)fx; ty=(int)fy; if(tx>=w||ty>=h) continue; }
                    else { int ix=(int)floor(gx-x0), iy=(int)floor(gy-y0); if(ix<0||iy<0||ix>=bw||iy>=bh) continue; tx=hf?w-1-ix:ix; ty=vf?h-1-iy:iy; }
                    size_t q=(size_t)Y*W+X;
                    if((nv_op[q]&7)<4 && (nv_op[q]&3)<=prio) continue;
                    int t=tile + (ty>>3)*(one_d ? (w>>3)*units : 32) + (tx>>3)*units;
                    uint32_t addr=VRAMB+0x10000+(uint32_t)t*32 + (ty&7)*(is8?8:4) + ((tx&7)>>(is8?0:1));
                    if(addr>=VRAMB+0x18000) continue;
                    int idx=*(volatile uint8_t*)(uintptr_t)addr; if(!is8){ idx = (tx&1) ? idx>>4 : idx&15; } if(!idx) continue;
                    nv_oc[q]=rgbpix(R16(PALRAM+0x200 + 2*(is8? idx : pal*16+idx))); nv_op[q]=(uint8_t)(prio | (gm==1?16:0));
                }
            }
        }
    }
    // ---- bitmap layer + blend
    uint16_t bldcnt=R16(IOREG+0x50), bldalpha=R16(IOREG+0x52); int ev=R16(IOREG+0x54)&31; if(ev>16) ev=16;
    int bmode=(bldcnt>>6)&3; int ea=bldalpha&31, eb=(bldalpha>>8)&31; if(ea>16)ea=16; if(eb>16)eb=16;
    int bgprio=R16(IOREG+0x0C)&3; int bg2_on=(dc>>10)&1;
    int16_t pa=(int16_t)R16(IOREG+0x20), pb=(int16_t)R16(IOREG+0x22), pc=(int16_t)R16(IOREG+0x24), pd=(int16_t)R16(IOREG+0x26);
    int32_t rx=(int32_t)(R32(IOREG+0x28)<<4)>>4, ry=(int32_t)(R32(IOREG+0x2C)<<4)>>4;
    uint32_t base = VRAMB + ((dc&0x10) ? 0xA000 : 0);
    int have_bg = bg2_on && mode>=3 && mode<=5, bw=240, bh=160;
    if(have_bg) nv_resolve_bg(mode,base,dc,&bw,&bh,bd);
    int dmaln = have_bg && lst_dma && mode==4;
    if(dmaln){                                                   // both pages: each line says which one it shows
        for(int y=0;y<160;y++) for(int x=0;x<240;x++){ int i0=*(volatile uint8_t*)(uintptr_t)(VRAMB+y*240+x), i1=*(volatile uint8_t*)(uintptr_t)(VRAMB+0xA000+y*240+x);
            bghave[y][x]=i0!=0; bgres[y][x]=i0?rgbpix(R16(PALRAM+i0*2)):bd; bghave2[y][x]=i1!=0; bgres2[y][x]=i1?rgbpix(R16(PALRAM+i1*2)):bd; } }
    int axis = (pb==0 && pc==0) && !dmaln;                                // no rotation: the bitmap's x depends on X only, y on Y only
    int *tabx=0,*taby=0; int8_t *wx=0,*wy=0;
    if(have_bg && axis){
        tabx=malloc(sizeof(int)*(W+H)); taby=tabx+W; wx=malloc(W+H); wy=wx+W;
        double ts_x = fabs(pa)/256.0, ts_y = fabs(pd)/256.0;      // texels per game pixel
        for(int X=0;X<W;X++){ double p=rx/256.0 + (pa/256.0)*(X*sx); double fl=floor(p); tabx[X]=(int)fl; double f=p-fl, d=0;
            if(smooth){ double k = ts_x>0 ? 1.0/(ts_x*sx) : 1; d=(f-0.5)*k; d=d<-0.5?-0.5:d>0.5?0.5:d; }   // (k: output pixels across one texel)
            wx[X]=(int8_t)lrint(d*16); }
        for(int Y=0;Y<H;Y++){ double p=ry/256.0 + (pd/256.0)*(Y*sy); double fl=floor(p); taby[Y]=(int)fl; double f=p-fl, d=0;
            if(smooth){ double k = ts_y>0 ? 1.0/(ts_y*sy) : 1; d=(f-0.5)*k; d=d<-0.5?-0.5:d>0.5?0.5:d; }
            wy[Y]=(int8_t)lrint(d*16); }
    }
    static const int bit[3]={2,4,5};
    for(int Y=0;Y<H;Y++){
        double rpa=0,rpc=0,rrx=0,rry=0; int rpg=0, rbg=have_bg;
        if(dmaln){                                               // this output row: the table row(s) of the game line(s) it lies over
            double lc=(Y+0.5)*sy-0.5; int l0=(int)floor(lc); if(l0<0) l0=0; if(l0>159) l0=159; int l1=l0+1>159?159:l0+1; double f=lc-l0; if(f<0) f=0;
            int nl=(int)((Y+0.5)*sy); if(nl>159) nl=159; const LineSt *N=&lst[nl], *A=&lst[l0], *B=&lst[l1];
            if(!(((A->dc&B->dc)&0x410)==0x410) || A->pa==0 || B->pa==0) { A=B=N; f=0; }          // (only between two ground lines: the sky and the HUD are never mixed in)
            rbg=(N->dc>>10)&1; rpg=(N->dc>>4)&1;
            rpa=(A->pa*(1-f)+B->pa*f)/256.0; rpc=(A->pc*(1-f)+B->pc*f)/256.0; rrx=(A->rx*(1-f)+B->rx*f)/256.0; rry=(A->ry*(1-f)+B->ry*f)/256.0;
            bldcnt=N->bldcnt; bldalpha=N->bldalpha; ev=N->bldy&31; if(ev>16) ev=16; bmode=(bldcnt>>6)&3; ea=bldalpha&31; eb=(bldalpha>>8)&31; if(ea>16)ea=16; if(eb>16)eb=16;
        }
        for(int X=0;X<W;X++){
            int have=0; uint16_t bc=bd;
            if(dmaln){
                if(rbg){ double gx=X*sx; int tx=(int)floor(rrx+rpa*gx), ty=(int)floor(rry+rpc*gx);
                    if(tx>=0&&ty>=0&&tx<240&&ty<160){ if(rpg){ have=bghave2[ty][tx]; bc=bgres2[ty][tx]; } else { have=bghave[ty][tx]; bc=bgres[ty][tx]; } } }
            }
            else if(have_bg){
                int tx,ty; int wxx=0,wyy=0;
                if(axis){ tx=tabx[X]; ty=taby[Y]; wxx=wx[X]; wyy=wy[Y]; }
                else { double gx=X*sx, gy=Y*sy; tx=(int)floor((rx + pa*gx + pb*gy)/256.0); ty=(int)floor((ry + pc*gx + pd*gy)/256.0); }
                if(tx>=0&&ty>=0&&tx<bw&&ty<bh){
                    have=bghave[ty][tx]; bc=bgres[ty][tx];
                    if(have && (wxx|wyy)){                       // an edge between texels: soften over this one output pixel (wxx, wyy: -8..8 = which neighbour, how much)
                        int tx2=tx+(wxx<0?-1:1), ty2=ty+(wyy<0?-1:1); int ax=wxx<0?-wxx:wxx, ay=wyy<0?-wyy:wyy;
                        int okx=tx2>=0&&tx2<bw, oky=ty2>=0&&ty2<bh;
                        uint16_t c00=bc, c10=okx&&bghave[ty][tx2]?bgres[ty][tx2]:bc, c01=oky&&bghave[ty2][tx]?bgres[ty2][tx]:bc, c11=okx&&oky&&bghave[ty2][tx2]?bgres[ty2][tx2]:bc;
                        uint16_t t0=mix15(c00,c10,ax), t1=mix15(c01,c11,ax); bc=mix15(t0,t1,ay);
                    }
                }
            }
            size_t q=(size_t)Y*W+X; uint8_t op=nv_op[q];
            uint16_t top=bd, second=bd; int tl=2, sl=2; int o=(op&7)<4;
            int oprio = o ? (op&3) : 9; int semi = o && (op&16);
            if(o && have){ if(oprio<=bgprio){ top=nv_oc[q]; tl=1; second=bc; sl=0; } else { top=bc; tl=0; second=nv_oc[q]; sl=1; } }
            else if(o){ top=nv_oc[q]; tl=1; second=bd; sl=2; }
            else if(have){ top=bc; tl=0; second=bd; sl=2; }
            uint16_t res=top;
            if(bmode|semi){
                int t1=(bldcnt>>bit[tl])&1, t2=(bldcnt>>(8+bit[sl]))&1;
                if(semi && tl==1 && t2) res=alpha(top,second,ea,eb);
                else if(bmode==1 && t1 && t2) res=alpha(top,second,ea,eb);
                else if(bmode==2 && t1) res=bright(top,ev,1);
                else if(bmode==3 && t1) res=bright(top,ev,0);
            }
            out[q]=XRGB(res);
        }
    }
    free(tabx); free(wx);
}
void pc_compose_240(uint32_t *out){ pc_compose_native(out,240,160,0); }

static uint64_t fnv(const void *d, size_t n, uint64_t h){ const uint8_t *p=d; while(n--){ h^=*p++; h*=1099511628211ull; } return h; }
uint64_t pc_hash_screen(void){ compose(); return fnv(screen,sizeof screen,1469598103934665603ull); }
uint64_t pc_hash_sram(void){ return fnv((void*)(uintptr_t)SRAMB,0x8000,1469598103934665603ull); }

// ---------------------------------------------------------------- the key script (same format as tools/harness: "<from> <to> keys", "shot <frame>")
typedef struct { long a,b; uint16_t k; } Ev;
static Ev ev[8192]; static int nev; static long shots[8192]; static int nshot; static char outpre[512]="pc_out";
static uint16_t keybits(const char *s){ uint16_t k=0; for(;*s;s++) switch(*s){
    case 'A':k|=1;break; case 'B':k|=2;break; case 's':k|=4;break; case 'S':k|=8;break; case 'r':k|=16;break; case 'l':k|=32;break;
    case 'U':k|=64;break; case 'D':k|=128;break; case 'R':k|=256;break; case 'L':k|=512;break; } return k; }
void pc_script(const char *path, const char *outprefix, long max_frames){
    if(outprefix) snprintf(outpre,sizeof outpre,"%s",outprefix);
    g_max_frames=max_frames;
    if(!path) return;
    FILE *f=fopen(path,"r"); if(!f){ fprintf(stderr,"pc_hw: no script %s\n",path); return; }
    char line[256]; long last=0;
    while(fgets(line,sizeof line,f)){
        long a,b; char ks[64];
        if(sscanf(line,"shot %ld",&a)==1){ if(nshot<8192) shots[nshot++]=a; if(a>last) last=a; continue; }
        if(sscanf(line,"%ld %ld %63s",&a,&b,ks)==3 && nev<8192){ ev[nev].a=a; ev[nev].b=b; ev[nev].k=keybits(ks); nev++; if(b>last) last=b; }
    }
    fclose(f);
    if(g_max_frames<0) g_max_frames=last+1;
}
long pc_frames(void){ return g_frame; }
int pc_motor(void){ return (R16(GPIOB+0xC4)&8)!=0; }      // the rumble cartridge's motor pin (rumble.inc)
int pc_script_keys(uint16_t *k){ if(!nev) return 0; uint16_t down=0; for(int i=0;i<nev;i++) if(g_frame>=ev[i].a && g_frame<=ev[i].b) down|=ev[i].k; *k=down; return 1; }
void pc_set_keys(uint16_t down){ R16(IOREG+0x130)=(uint16_t)(~down & 0x3FF); }
#ifdef PC_SDL
static void irq_now(void){ if((R16(IOREG+0x200)&1) && R16(IOREG+0x208)&1 && pc_irq_fn){ R16(IOREG+0x202)|=1; pc_irq_fn(); R16(IOREG+0x202)&=~1; } }
#endif
static void vblank(void){
    g_frame++;
    for(int i=0;i<nshot;i++) if(shots[i]==g_frame){ char n[600]; snprintf(n,sizeof n,"%s_%06ld.ppm",outpre,g_frame); pc_write_ppm(n,getenv("PC_SCALE")?atoi(getenv("PC_SCALE")):1);
        printf("shot %ld screen=%016llx\n",g_frame,(unsigned long long)pc_hash_screen()); }
#ifdef PC_SDL
    if(g_window){ irq_now(); plat_frame(); return; }       // window mode: the game's interrupt (music), then the platform shows the picture, plays the sound, waits for the next 60th, reads the keys
#endif
    if(g_max_frames>=0 && g_frame>=g_max_frames){
        if(getenv("PC_DEBUG")){ int nz0=0,nz1=0; for(int i=0;i<0x9600;i++){ nz0+=*(volatile uint8_t*)(uintptr_t)(VRAMB+i)!=0; nz1+=*(volatile uint8_t*)(uintptr_t)(VRAMB+0xA000+i)!=0; }
            printf("dbg DISPCNT=%04x BLDCNT=%04x BLDALPHA=%04x BLDY=%04x BG2CNT=%04x PA=%04x PD=%04x nz0=%d nz1=%d pal1=%04x IE=%04x IME=%04x\n",R16(IOREG),R16(IOREG+0x50),R16(IOREG+0x52),R16(IOREG+0x54),R16(IOREG+0xC),R16(IOREG+0x20),R16(IOREG+0x26),nz0,nz1,R16(PALRAM+2),R16(IOREG+0x200),R16(IOREG+0x208)); } printf("end frame=%ld screen=%016llx sram=%016llx\n",g_frame,(unsigned long long)pc_hash_screen(),(unsigned long long)pc_hash_sram()); pc_hw_finish(); fflush(stdout); exit(0); }
    uint16_t down=0; for(int i=0;i<nev;i++) if(g_frame>=ev[i].a && g_frame<=ev[i].b) down|=ev[i].k;
    R16(IOREG+0x130)=(uint16_t)(~down & 0x3FF);
    if((R16(IOREG+0x200)&1) && R16(IOREG+0x208)&1 && pc_irq_fn){ R16(IOREG+0x202)|=1; pc_irq_fn(); R16(IOREG+0x202)&=~1; }
}
uint16_t pc_vcount(void){
    if(++g_line>=228) g_line=0;
    uint16_t ds=R16(IOREG+4); ds=(uint16_t)((ds&~7)|((g_line>=160&&g_line<227)?1:0)); R16(IOREG+4)=ds;
    R16(IOREG+6)=(uint16_t)g_line;
    if(g_line==160) vblank();
    return (uint16_t)g_line;
}

int pc_mouse_x = -1, pc_mouse_y = -1, pc_mouse_b, pc_mouse_wheel;
