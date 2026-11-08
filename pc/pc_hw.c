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
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include "pc_hw.h"
#include "shipr/shipr.h"
#include "platform/pc_plat.h"
#include <math.h>
#include <time.h>

void (*pc_irq_fn)(void);
int pc_ships_mode=-1, pc_armada_n=0;   // the ship renderer: -1 off (the game draws its own ships), 0 CLASSIC, 1 NATIVE, 2 ULTRA; ships in the far battle (PC_SHIPS / PC_ARMADA for tests)
int pc_nat_w, pc_nat_h, pc_dn_w, pc_dn_h, pc_dn_sc=1; uint32_t *pc_dn_buf, *pc_dn_spl; int pc_dn_spl_on; long pc_dn_f[2]={-100,-100}; uint8_t pc_dn_snap[2][240*160], pc_dn_snap2[2][240*160];   // the native dungeon (pc_dnative.inc): the picture, its size, the page snapshot it belongs to
static uint8_t sr_snap[2][240*128]; static long sr_snap_f[2]={-100,-100};
void pc_sr_snapshot(const volatile void *pg){ int t=((uintptr_t)pg)>=GBAA(0x0600A000u) ? 1 : 0; memcpy(sr_snap[t],(const void*)pg,240*128); sr_snap_f[t]=pc_frames(); }
// the ship editor's preview: the page as it will be shown, except the preview rectangle which holds what it looked like before its text went on (so text stays on top of the native ship)
void pc_sr_snapshot_rect(const volatile void *pg, const volatile void *back, int x0, int y0, int w, int h){ int t=((uintptr_t)pg)>=GBAA(0x0600A000u) ? 1 : 0; memcpy(sr_snap[t],(const void*)pg,240*128);
    for(int y=y0;y<y0+h && y<128;y++) for(int x=x0;x<x0+w && x<240;x++) sr_snap[t][y*240+x]=((const volatile uint8_t*)back)[y*240+x]; sr_snap_f[t]=pc_frames(); }
void pc_sr_forget(void){ sr_snap_f[0]=sr_snap_f[1]=-100; }      // leaving a screen: whatever the ship renderer last kept must not show on the next one
int pc_vid3d;                   // 3D views: 0 AUTO, 1 NATIVE, 2 CLASSIC (set by the window front end; PC_V3D=0..2 in the environment for tests)

#define EWRAM GBAA(0x02000000u)
#define IWRAM GBAA(0x03000000u)
#define IOREG GBAA(0x04000000u)
#define PALRAM GBAA(0x05000000u)
#define VRAMB GBAA(0x06000000u)
#define OAMB  GBAA(0x07000000u)
#define GPIOB GBAA(0x08000000u)
#define SRAMB GBAA(0x0E000000u)
#define R16(a) (*(volatile uint16_t*)(uintptr_t)(a))
#define R32(a) (*(volatile uint32_t*)(uintptr_t)(a))

static const char *g_sav;
static long g_frame, g_max_frames=-1;
static int g_line;
static int g_window;            // 1 = a window drives the game (keys, picture, sound, pacing come from pc/platform), 0 = headless script mode
void pc_set_window_mode(int on){ g_window=on; }
int  pc_window_mode(void){ return g_window; }

unsigned pc_map_fail_addr; unsigned long pc_map_fail_err;       // Windows: which fake-GBA address could not be mapped (shown in the error box and the log)
static int map_at(uint32_t a, size_t n){
#ifdef _WIN32
    { void *p=VirtualAlloc((void*)(uintptr_t)a,n,MEM_COMMIT,PAGE_READWRITE);   /* the launcher (pc_main.c) reserved the range before any DLL could take it: commit it */
      if(!p||(uintptr_t)p!=a) p=VirtualAlloc((void*)(uintptr_t)a,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); if(!p||(uintptr_t)p!=a){ pc_map_fail_addr=a; pc_map_fail_err=GetLastError(); fprintf(stderr,"pc_hw: cannot map %08x (windows error %lu)\n",a,pc_map_fail_err); { MEMORY_BASIC_INFORMATION mi; if(VirtualQuery((void*)(uintptr_t)a,&mi,sizeof mi)) fprintf(stderr,"  there: base %p alloc-base %p size %lx state %lx type %lx\n",mi.BaseAddress,mi.AllocationBase,(unsigned long)mi.RegionSize,(unsigned long)mi.State,(unsigned long)mi.Type); } return 0; } return 1; }
#else
    void *p = mmap((void*)(uintptr_t)a, n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE, -1, 0);
    if(p!=MAP_FAILED && (uintptr_t)p!=a){ munmap(p,n); p=MAP_FAILED; }      // an old kernel ignores MAP_FIXED_NOREPLACE and may give another address: do not keep it
    if(p==MAP_FAILED){ fprintf(stderr,"pc_hw: cannot map %08x (%zu bytes): %s\n",a,n,strerror(errno));
        // patch 35: say what is in the way (the lines of /proc/self/maps that overlap the range), so a log is enough to pick another PC_GBA_TOP
        FILE *m=fopen("/proc/self/maps","r"); if(m){ char l[320]; int k=0; while(k<12 && fgets(l,sizeof l,m)){ unsigned long lo=0,hi=0; if(sscanf(l,"%lx-%lx",&lo,&hi)==2 && lo<(unsigned long)a+n && hi>(unsigned long)a){ fprintf(stderr,"  in the way: %s",l); k++; } } fclose(m); }
        return 0; }
    return 1;
#endif
}
static uint64_t g_saved_hash;
uint64_t pc_hash_sram(void);
int pc_hw_init(const char *sav_path){
    g_sav = sav_path;
    if(getenv("PC_NATIVE") && getenv("PC_SCALE")){ int sc=atoi(getenv("PC_SCALE")); if(sc>1){ pc_nat_w=240*sc; pc_nat_h=160*sc; } } if(getenv("PC_SHIPS")) pc_ships_mode=atoi(getenv("PC_SHIPS")); if(getenv("PC_ARMADA")) pc_armada_n=atoi(getenv("PC_ARMADA"));
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
// the game's DMA source / destination writes go through pc_dma_sad / pc_dma_dad (build.sh, patch 25) so a pointer above 4 GB keeps its top half.
// The low 32 bits also go into the I/O register, so a write that does not use them (a plain 0x040000D4 = ... store) still works: if the register no longer
// matches the last full value, the register wins.
static uintptr_t g_sad, g_dad;
void pc_dma_sad(uintptr_t a){ g_sad=a; R32(IOREG+0xD4)=(uint32_t)a; }
void pc_dma_dad(uintptr_t a){ g_dad=a; R32(IOREG+0xD8)=(uint32_t)a; }
void pc_dma3(uint32_t v){
    uintptr_t sad=R32(IOREG+0xD4), dad=R32(IOREG+0xD8); uint32_t cnt=v&0xFFFF, ctl=v>>16;
    if((uint32_t)g_sad==(uint32_t)sad) sad=g_sad;
    if((uint32_t)g_dad==(uint32_t)dad) dad=g_dad;
    if(!(ctl&0x8000)) return;
    if(((ctl>>12)&3)!=0) return;                              // only "start now"; the timed kinds (HBlank/VBlank) are not done here
    if(!cnt) cnt=0x10000;
    if(sad<0x1000 || dad<0x1000){ fprintf(stderr,"pc_hw: DMA with bad address %llx -> %llx\n",(unsigned long long)sad,(unsigned long long)dad); return; }
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
void (*pc_ovl_fn)(uint32_t *out, int w, int h);      // a PC-made 3D view under the game's picture (the warship interiors): shown wherever the game draws nothing
static uint32_t ovl240[240*160]; static uint32_t *nv_ovl;
static void compose(void){
    build_lines(); if(pc_ovl_fn) pc_ovl_fn(ovl240,240,160);
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
        if(pc_ovl_fn && tl==2 && !semi){ uint32_t c=ovl240[y*240+x]; out=(uint16_t)((c>>19&31)|((c>>11&31)<<5)|((c>>3&31)<<10)); }
        screen[y][x]=out;
    } }
}
void pc_render(uint8_t *rgb){
    compose();
    for(int y=0;y<160;y++) for(int x=0;x<240;x++){ uint16_t c=screen[y][x]; int r=c&31, g=(c>>5)&31, b=(c>>10)&31; uint8_t *p=rgb+(y*240+x)*3;
        p[0]=(uint8_t)(r<<3|r>>2); p[1]=(uint8_t)(g<<3|g>>2); p[2]=(uint8_t)(b<<3|b>>2); }
}
double pc_now_ms(void); void pc_vrec(int,double,double); double pc_cpu_ms(void); extern double pc_vms[4], pc_vmin[4], pc_vcpu[4], pc_vcmin[4];
int pc_write_ppm(const char *path, int scale){
    if(getenv("PC_NATIVE")){                                   // test: make the picture with the native path (pc_compose_native) at scale x 240x160
        int W=240*scale, H=160*scale; uint32_t *b=malloc((size_t)W*H*4); pc_compose_native(b,W,H,atoi(getenv("PC_NATIVE"))>1);
        if(getenv("PC_NBENCH")){ int n=atoi(getenv("PC_NBENCH")); double mn=1e9,sum=0,wmn=1e9,wsum=0; for(int i=0;i<n;i++){ struct timespec a,c; double w0=pc_now_ms(); clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&a); pc_compose_native(b,W,H,1); clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&c); double ww=pc_now_ms()-w0; if(ww<wmn) wmn=ww; wsum+=ww; double ms=(c.tv_sec-a.tv_sec)*1e3+(c.tv_nsec-a.tv_nsec)/1e6; if(ms<mn) mn=ms; sum+=ms; } fprintf(stderr,"native compose %dx%d: wall best %.2f avg %.2f ms, cpu best %.2f avg %.2f ms (all threads) over %d (frame %ld); native pass in the game frame (best wall / best cpu ms): dungeon %.2f/%.2f planet %.2f/%.2f space %.2f/%.2f\n",W,H,wmn,wsum/n,mn,sum/n,n,g_frame,pc_vmin[0]<1e8?pc_vmin[0]:0,pc_vcmin[0]<1e8?pc_vcmin[0]:0,pc_vmin[1]<1e8?pc_vmin[1]:0,pc_vcmin[1]<1e8?pc_vcmin[1]:0,pc_vmin[2]<1e8?pc_vmin[2]:0,pc_vcmin[2]<1e8?pc_vcmin[2]:0); }   // (test: time the whole native picture, and the native passes of the game frame)
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

// ---- the kart ground at native size (NATIVE 3D view). The game's own line tables say, for every game line, where on the map window (page 1) the ground
// under that line starts and how far one pixel steps. Here those are interpolated between lines (so the road is smooth in perspective at any
// window height, no 160-step ladder), the map is filtered (bilinear close up, averaged taps far away), the distance haze is continuous, and the
// sky's one-colour-per-row bands are blended into a smooth gradient. Nothing here touches the game's state: it is only how the picture is made.
typedef struct { double pa,pc,rx,ry,fog; int pg,bg,ground,nl; } RowP;
static uint32_t pg888[2][160][240]; static uint16_t skym[160]; static int skyok[160];
static void nv_row(int Y, double sy, RowP *r){
    double lc=(Y+0.5)*sy-0.5; int l0=(int)floor(lc); if(l0<0) l0=0; if(l0>159) l0=159; int l1=l0+1>159?159:l0+1; double f=lc-l0; if(f<0) f=0;
    int nl=(int)((Y+0.5)*sy); if(nl>159) nl=159; if(nl<0) nl=0; const LineSt *N=&lst[nl], *A=&lst[l0], *B=&lst[l1];
    r->ground = ((N->dc&0x410)==0x410) && N->pa!=0;
    if(!(((A->dc&B->dc)&0x410)==0x410) || A->pa==0 || B->pa==0){ A=B=N; f=0; }
    r->nl=nl; r->bg=(N->dc>>10)&1; r->pg=(N->dc>>4)&1;
    r->pa=(A->pa*(1-f)+B->pa*f)/256.0; r->pc=(A->pc*(1-f)+B->pc*f)/256.0; r->rx=(A->rx*(1-f)+B->rx*f)/256.0; r->ry=(A->ry*(1-f)+B->ry*f)/256.0;
    r->fog=-1; uint16_t bc=N->bldcnt;
    if(((bc>>6)&3)==1 && (bc&4) && (bc&0x2000)){ double eb=(((A->bldalpha>>8)&31)*(1-f)+((B->bldalpha>>8)&31)*f)/16.0; r->fog= eb>1?1:eb; }
}
static inline uint32_t mix888(uint32_t a, uint32_t b, double t){
    int ar=a>>16&255, ag=a>>8&255, ab=a&255, br=b>>16&255, bg=b>>8&255, bb=b&255;
    return (uint32_t)(int)(ar+(br-ar)*t+0.5)<<16 | (uint32_t)(int)(ag+(bg-ag)*t+0.5)<<8 | (uint32_t)(int)(ab+(bb-ab)*t+0.5);
}
static void nv_ground_prep(uint16_t bd){
    uint8_t lut5[32]; for(int i=0;i<32;i++) lut5[i]=(uint8_t)(i<<3|i>>2);
    for(int p=0;p<2;p++) for(int y=0;y<160;y++) for(int x=0;x<240;x++){ uint16_t c= p ? bgres2[y][x] : bgres[y][x];
        pg888[p][y][x]=(uint32_t)lut5[c&31]<<16 | (uint32_t)lut5[(c>>5)&31]<<8 | lut5[(c>>10)&31]; }
    for(int y=0;y<160;y++){ skyok[y]=0; if(y<19 || (lst[y].dc&0x10)) continue;               // the sky rows: the colour most of the row has (a vote), if it is more than half
        int cand=0, cnt=0; for(int x=0;x<240;x++){ uint16_t c=bgres[y][x]; if(cnt==0){ cand=c; cnt=1; } else if(c==cand) cnt++; else cnt--; }
        int n=0; for(int x=0;x<240;x++) n+= bgres[y][x]==cand; if(n>=140){ skym[y]=(uint16_t)cand; skyok[y]=1; } }
}
static inline uint32_t nv_bil(int pg, float u, float v, float iF){      // "sharp bilinear" (u,v >= 0 texels; iF = 1 / footprint): flat inside a texel, blended over one output pixel at its edges
    u=u<0?0:u; v=v<0?0:v; int x0=(int)u, y0=(int)v; float fu=u-x0, fv=v-y0;
    float tx=(fu-0.5f)*iF+0.5f, ty=(fv-0.5f)*iF+0.5f; tx=tx<0?0:tx>1?1:tx; ty=ty<0?0:ty>1?1:ty;
    int ax=fu<0.5f?x0-1:x0+1, ay=fv<0.5f?y0-1:y0+1; float wx = fu<0.5f ? 1-tx : tx, wy = fv<0.5f ? 1-ty : ty;
    int X0=x0>239?239:x0, Y0=y0>159?159:y0, X1=ax<0?0:ax>239?239:ax, Y1=ay<0?0:ay>159?159:ay;
    int wxi=(int)(wx*256), wyi=(int)(wy*256);
    if(!wxi && !wyi) return pg888[pg][Y0][X0];
    uint32_t a=pg888[pg][Y0][X0], b=pg888[pg][Y0][X1], c=pg888[pg][Y1][X0], d=pg888[pg][Y1][X1];
    uint32_t rb0=(a&0xFF00FF)*(256-wxi)+(b&0xFF00FF)*wxi, g0=(a&0xFF00)*(256-wxi)+(b&0xFF00)*wxi, rb1=(c&0xFF00FF)*(256-wxi)+(d&0xFF00FF)*wxi, g1=(c&0xFF00)*(256-wxi)+(d&0xFF00)*wxi;
    rb0=(rb0>>8)&0xFF00FF; rb1=(rb1>>8)&0xFF00FF; g0=(g0>>8)&0xFF00; g1=(g1>>8)&0xFF00;
    uint32_t rb=(((rb0*(256-wyi)+rb1*wyi)>>8)&0xFF00FF), g=(((g0*(256-wyi)+g1*wyi)>>8)&0xFF00); return rb|g;
}
static inline uint32_t nv_grain(uint32_t c, float u, float v, float F){   // close up the map is only one texel per cell: a faint fixed grain, finer than the texel, keeps the ground from looking like smooth plastic
    if(F>=0.5) return c; double k=(0.5-F)/0.5; int iu=(int)(u*5), iv=(int)(v*5); uint32_t h=(uint32_t)iu*2654435761u ^ (uint32_t)iv*2246822519u; h^=h>>15; h*=2246822519u; h^=h>>13;
    int d=(int)((h&255)-128)*(int)(k*9)/128;          // +-9 at the closest
    int r=(c>>16&255)+d, g=(c>>8&255)+d, b=(c&255)+d; r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b; return (uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b;
}

// ---- a small pool of worker threads: the picture's rows are shared out (each row only depends on the frame, so any split gives the same picture)
#include <pthread.h>
#ifndef _WIN32
#include <unistd.h>
#endif
typedef void (*ParFn)(void*,int,int);
static struct { pthread_t th[15]; int n, inited; pthread_mutex_t m; pthread_cond_t go, done; ParFn fn; void *ctx; int total, next, gen, left, chunk; } PP;
static int par_take(int *lo, int *hi){ pthread_mutex_lock(&PP.m); int a=PP.next; if(a>=PP.total){ pthread_mutex_unlock(&PP.m); return 0; } int b=a+PP.chunk; if(b>PP.total) b=PP.total; PP.next=b; pthread_mutex_unlock(&PP.m); *lo=a; *hi=b; return 1; }
static void *par_worker(void *u){ (void)u; int seen=0;
    for(;;){ pthread_mutex_lock(&PP.m); while(PP.gen==seen) pthread_cond_wait(&PP.go,&PP.m); seen=PP.gen; pthread_mutex_unlock(&PP.m);
        int lo,hi; while(par_take(&lo,&hi)) PP.fn(PP.ctx,lo,hi);
        pthread_mutex_lock(&PP.m); if(--PP.left==0) pthread_cond_signal(&PP.done); pthread_mutex_unlock(&PP.m); } return 0; }
static int pc_ncpu(void){
#ifdef _WIN32
    SYSTEM_INFO si; GetSystemInfo(&si); return (int)si.dwNumberOfProcessors;
#else
    long n=sysconf(_SC_NPROCESSORS_ONLN); return n>0?(int)n:1;
#endif
}
static void pc_par_init(void);
void pc_par_chunk(int total, int chunk, ParFn fn, void *ctx);
void pc_par(int total, ParFn fn, void *ctx){ pc_par_chunk(total,8,fn,ctx); }
int pc_par_threads(void){ pc_par_init(); return PP.n+1; }
static void pc_par_init(void){
    if(!PP.inited){ PP.inited=1; int n=pc_ncpu()-1; const char *e=getenv("PC_THREADS"); if(e) n=atoi(e)-1; if(n<0) n=0; if(n>15) n=15;
        pthread_mutex_init(&PP.m,0); pthread_cond_init(&PP.go,0); pthread_cond_init(&PP.done,0);
        for(int i=0;i<n;i++) if(pthread_create(&PP.th[i],0,par_worker,0)==0) PP.n++; } }
void pc_par_chunk(int total, int chunk, ParFn fn, void *ctx){
    pc_par_init();
    if(!PP.n || total<16){ fn(ctx,0,total); return; }
    pthread_mutex_lock(&PP.m); PP.chunk=chunk<1?1:chunk; PP.fn=fn; PP.ctx=ctx; PP.total=total; PP.next=0; PP.left=PP.n; PP.gen++; pthread_cond_broadcast(&PP.go); pthread_mutex_unlock(&PP.m);
    int lo,hi; while(par_take(&lo,&hi)) fn(ctx,lo,hi);
    pthread_mutex_lock(&PP.m); while(PP.left>0) pthread_cond_wait(&PP.done,&PP.m); pthread_mutex_unlock(&PP.m);
}
static int nv_v3(void); int pc_v3d_classic(void); extern int pc_nat_w;
#include "pc_kartnat_r.inc"
typedef struct { int dmaln, nat3, have_bg, W, H, bgprio, axis, bw, bh, ev, bmode, ea, eb; double sx, sy; uint16_t bldcnt, bldalpha, bd; uint8_t lut5[32]; int *tabx, *taby; int8_t *wx, *wy; int32_t rx, ry; int pa, pb, pc, pd; uint32_t *out; } NvCtx;
static void nv_rows(void *vc, int lo, int hi){
    NvCtx *c=(NvCtx*)vc; __typeof__(c->dmaln) dmaln=c->dmaln; __typeof__(c->nat3) nat3=c->nat3; __typeof__(c->have_bg) have_bg=c->have_bg; __typeof__(c->W) W=c->W; __typeof__(c->H) H=c->H; __typeof__(c->bgprio) bgprio=c->bgprio; __typeof__(c->axis) axis=c->axis; __typeof__(c->bw) bw=c->bw; __typeof__(c->bh) bh=c->bh; __typeof__(c->ev) ev=c->ev; __typeof__(c->bmode) bmode=c->bmode; __typeof__(c->ea) ea=c->ea; __typeof__(c->eb) eb=c->eb; __typeof__(c->sx) sx=c->sx; __typeof__(c->sy) sy=c->sy; __typeof__(c->bldcnt) bldcnt=c->bldcnt; __typeof__(c->bldalpha) bldalpha=c->bldalpha; __typeof__(c->bd) bd=c->bd; __typeof__(c->tabx) tabx=c->tabx; __typeof__(c->taby) taby=c->taby; __typeof__(c->wx) wx=c->wx; __typeof__(c->wy) wy=c->wy; __typeof__(c->rx) rx=c->rx; __typeof__(c->ry) ry=c->ry; __typeof__(c->pa) pa=c->pa; __typeof__(c->pb) pb=c->pb; __typeof__(c->pc) pc=c->pc; __typeof__(c->pd) pd=c->pd; __typeof__(c->out) out=c->out; const uint8_t *lut5=c->lut5; (void)nat3;
    #define XRGB(c) ((uint32_t)lut5[(c)&31]<<16 | (uint32_t)lut5[((c)>>5)&31]<<8 | lut5[((c)>>10)&31])
    static const int bit[3]={2,4,5};
    for(int Y=lo;Y<hi;Y++){
        double rpa=0,rpc=0,rrx=0,rry=0; int rpg=0, rbg=have_bg; RowP R0,R1; int nfx=1,nfy=1; double fog=-1, FP=1; float iFP=1;
        if(dmaln){                                               // this output row: the table row(s) of the game line(s) it lies over
            nv_row(Y,sy,&R0); rbg=R0.bg; rpg=R0.pg; rpa=R0.pa; rpc=R0.pc; rrx=R0.rx; rry=R0.ry;
            { const LineSt *N=&lst[R0.nl]; bldcnt=N->bldcnt; bldalpha=N->bldalpha; ev=N->bldy&31; if(ev>16) ev=16; bmode=(bldcnt>>6)&3; ea=bldalpha&31; eb=(bldalpha>>8)&31; if(ea>16)ea=16; if(eb>16)eb=16; }
            if(nat3 && R0.ground){ fog=R0.fog; nv_row(Y+1<H?Y+1:Y,sy,&R1); double g=W*0.5*sx;
                double du=(R1.rx+R1.pa*g)-(R0.rx+R0.pa*g), dv=(R1.ry+R1.pc*g)-(R0.ry+R0.pc*g); double fy=sqrt(du*du+dv*dv), fx=sqrt(rpa*rpa+rpc*rpc)*sx;
                FP=fx>fy?fx:fy; if(FP<0.0625) FP=0.0625; iFP=(float)(1.0/FP); nfx=(int)ceil(fx-0.25); nfy=(int)ceil(fy-0.25); if(nfx<1)nfx=1; if(nfx>3)nfx=3; if(nfy<1)nfy=1; if(nfy>3)nfy=3; if(nfx*nfy>2){ if(nfx>=nfy){ nfx=2; nfy=1; } else { nfy=2; nfx=1; } } }
        }
        uint32_t bd888=(uint32_t)lut5[bd&31]<<16|(uint32_t)lut5[(bd>>5)&31]<<8|lut5[(bd>>10)&31]; int fog256=fog>0?(int)(fog*256+0.5):0;   // (the haze colour and strength, once per row, in integers)
        double gx0=0.5*sx-0.5, ustep=rpa*sx, vstep=rpc*sx, ubase=rrx+rpa*gx0, vbase=rry+rpc*gx0;
        for(int X=0;X<W;X++){
            if(nv_op[(size_t)Y*W+X]&0x40) continue;
            int have=0; uint16_t bc=bd;
            uint32_t bc888=0; int sm=0;
            if(dmaln){
                int kn_hit = kn_on && (kn_buf[(size_t)Y*W+X]>>31);
                if(nat3){
                    if(kn_hit){ uint32_t c=kn_buf[(size_t)Y*W+X]&0xFFFFFF; have=1; bc888=c; sm=1; size_t q=(size_t)Y*W+X; if((nv_op[q]&7)>=4 && ((kn_buf[q]&0x40000000u) || !(bmode>=2))){ out[q]=c; continue; } bc=(uint16_t)((c>>3&31)|((c>>11&31)<<5)|((c>>19&31)<<10)); }
                    else if(rbg && R0.ground){ double g=(X+0.5)*sx-0.5; double u0=ubase+ustep*X, v0=vbase+vstep*X;
                        if(u0>=0&&v0>=0&&u0<240&&v0<160){ have=1; uint32_t c;
                            if(nfx==1&&nfy==1){ c=nv_bil(1,(float)u0,(float)v0,iFP); if(FP<0.5) c=nv_grain(c,(float)u0,(float)v0,(float)FP); }
                            else { double du=rpa*sx/nfx, dv=rpc*sx/nfx, ex=(R1.rx+R1.pa*g-(rrx+rpa*g))/nfy, ey=(R1.ry+R1.pc*g-(rry+rpc*g))/nfy; unsigned sr=0,sg=0,sb=0; int n=0;
                                for(int j=0;j<nfy;j++) for(int i=0;i<nfx;i++){ double ox=i-(nfx-1)*0.5, oy=j-(nfy-1)*0.5; uint32_t t=nv_bil(1,(float)(u0+du*ox+ex*oy),(float)(v0+dv*ox+ey*oy),1.0/(FP/(nfx>nfy?nfx:nfy))); sr+=t>>16&255; sg+=t>>8&255; sb+=t&255; n++; }
                                c=(sr/n)<<16|(sg/n)<<8|(sb/n); }
                            if(fog256){ uint32_t rb=((c&0xFF00FF)*(256-fog256)+(bd888&0xFF00FF)*fog256)>>8, gg=((c&0xFF00)*(256-fog256)+(bd888&0xFF00)*fog256)>>8; c=(rb&0xFF00FF)|(gg&0xFF00); }
                            bc888=c; sm=1; { size_t q=(size_t)Y*W+X; if((nv_op[q]&7)>=4){ out[q]=c; continue; } } bc=(uint16_t)((c>>3&31)|((c>>11&31)<<5)|((c>>19&31)<<10)); } }
                    else if(rbg){ int tx=(int)((X+0.5)*sx), nl=R0.nl; if(tx>239) tx=239; have=rpg? bghave2[nl][tx] : bghave[nl][tx]; bc=rpg? bgres2[nl][tx] : bgres[nl][tx];
                        if(!rpg && skyok[nl] && bgres[nl][tx]==skym[nl]){ double p=(Y+0.5)*sy-0.5-nl; int nb=nl+(p>0?1:-1); double w=p<0?-p:p; if(w>1) w=1;
                            if(nb>=19 && nb<160 && skyok[nb]){ uint16_t a=skym[nl], b2=skym[nb]; uint32_t ca=(uint32_t)lut5[a&31]<<16|(uint32_t)lut5[(a>>5)&31]<<8|lut5[(a>>10)&31], cb=(uint32_t)lut5[b2&31]<<16|(uint32_t)lut5[(b2>>5)&31]<<8|lut5[(b2>>10)&31];
                                bc888=mix888(ca,cb,w); sm=1; { size_t q=(size_t)Y*W+X; if((nv_op[q]&7)>=4 && !(bmode>=2)){ out[q]=bc888; continue; } } } } }
                }
                else if(rbg){ double gx=X*sx; int tx=(int)floor(rrx+rpa*gx), ty=(int)floor(rry+rpc*gx);
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
            out[q]= (sm && tl==0 && !semi) ? bc888 : (nv_ovl && tl==2 && !semi) ? nv_ovl[q] : XRGB(res);
        }
    }
}
double pc_now_ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
double pc_vms[4], pc_vmin[4]={1e9,1e9,1e9,1e9}, pc_vcpu[4], pc_vcmin[4]={1e9,1e9,1e9,1e9}; static long pc_vn[4];
double pc_cpu_ms(void){ struct timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t); return t.tv_sec*1e3+t.tv_nsec/1e6; }
// ---- QUALITY (VIDEO > QUALITY): LOW, MEDIUM or HIGH, or AUTO: the native 3D views (dungeon 0, planet 1) trade detail for speed by themselves when the frame costs too much.
// HIGH = the full picture. MEDIUM = cheaper shading (nearest texels in the dungeon, far ground every other row and far steps sampled once on the planet). LOW = MEDIUM and the
// picture made at half size each way, shown with each pixel repeated. AUTO steps down when the average cost of a frame (the 3D pass plus the compose, wall time) is over the budget
// (PC_QBUDGET ms, default 15), and back up only when the next level up is estimated to fit well inside it, waiting longer each time a step up had to be taken back.
int pc_quality;                  // 0 AUTO, 1 LOW, 2 MEDIUM, 3 HIGH (set by the window front end; PC_QUALITY=0..3 in the environment forces one)
static int pc_ql[4]={3,3,3,3}; static double pc_vlast[4]; static long pc_vfr[4]={-100,-100,-100,-100};
static int pc_q_env(void){ static int env=-2; if(env==-2){ const char *e=getenv("PC_QUALITY"); env=e?atoi(e):-1; } return env; }
int pc_q_get(int id){ int q=pc_quality, e=pc_q_env(); if(e>=0) q=e; if(q>=1 && q<=3) return q; return pc_ql[id&3]; }
int pc_q_auto(void){ int e=pc_q_env(); return (e>=0?e:pc_quality)==0; }
int pc_q_cur(void){ int id = g_frame-pc_vfr[1]<=2 ? 1 : g_frame-pc_vfr[0]<=2 ? 0 : -1; return id<0 ? 3 : pc_q_get(id); }      // the level of the view on screen (for the FPS line)
static void pc_q_tick(double comp_ms){
    if(!pc_q_auto()) return; static double budget=-1; if(budget<0){ const char *e=getenv("PC_QBUDGET"); budget=e?atof(e):15.0; if(budget<2) budget=15.0; }
    static double ema[4]; static int n[4], cool[4], good[4], upwait[4]={120,120,120,120}; static long lastup[4];
    for(int id=0;id<2;id++){ if(g_frame-pc_vfr[id]>1) { n[id]=0; continue; } double cost=pc_vlast[id]+comp_ms; ema[id]= n[id]++ ? ema[id]*0.88+cost*0.12 : cost;
        if(cool[id]>0){ cool[id]--; continue; } int L=pc_ql[id];
        if(ema[id]>budget && L>1 && n[id]>8){ pc_ql[id]=L-1; ema[id]*= L==3 ? 0.70 : 0.40; cool[id]=40; good[id]=0; if(g_frame-lastup[id]<400 && upwait[id]<1920) upwait[id]*=2; continue; }       // too slow: one level down
        double ratio = L==1 ? 2.5 : 1.45;
        if(L<3 && ema[id]*ratio<budget*0.85){ if(++good[id]>=upwait[id]){ pc_ql[id]=L+1; ema[id]*=ratio; cool[id]=40; good[id]=0; lastup[id]=g_frame; } } else good[id]=0; }
}
void pc_vrec(int id,double ms,double cpu){ pc_vlast[id&3]=ms; pc_vfr[id&3]=g_frame;  pc_vms[id]=pc_vn[id]?pc_vms[id]*0.95+ms*0.05:ms; pc_vcpu[id]=pc_vn[id]?pc_vcpu[id]*0.95+cpu*0.05:cpu; pc_vn[id]++; if(ms<pc_vmin[id]) pc_vmin[id]=ms; if(cpu<pc_vcmin[id]) pc_vcmin[id]=cpu; }       // the game-frame cost of the native 3D passes (0 dungeon, 1 planet, 2 space): wall and CPU (all threads) time, a running average and the best frame (shown by PC_NBENCH)
static double pc_dn_last_ms; void pc_dn_report(double ms,int W,int H){ pc_dn_last_ms=ms; static int on=-1; static double acc; static int n; if(on<0) on=getenv("PC_TIMING")!=0; if(!on) return; acc+=ms; if(++n==200){ fprintf(stderr,"dungeon native %dx%d: %.2f ms per frame\n",W,H,acc/n); n=0; acc=0; } }
int pc_v3d_classic(void), pc_v3d_ultra(void);
static int nv_v3(void){ int v3=pc_vid3d; static int env=-2; if(env==-2){ const char *e=getenv("PC_V3D"); env=e?atoi(e):-1; } if(env>=0) v3=env; return v3; }
int pc_v3d_ultra(void){ return nv_v3()==3 && pc_ships_mode!=0; }                  // 3D VIEWS = ULTRA: the native views plus the extras (smoothed edges, bloom, glow); PC_V3D=3 in the environment for tests
int pc_v3d_classic(void){ return nv_v3()==2 || pc_ships_mode==0; }   // CLASSIC in either switch (3D VIEWS, or SHIPS: the one the ship renderer uses) keeps the GBA picture
static int spr_tex(int tile,int tx,int ty,int w,int h,int one_d,int is8,int pal,uint16_t *col){    // one texel of a sprite: 0 = transparent
    if(tx<0||ty<0||tx>=w||ty>=h) return 0; int units=is8?2:1; int t=tile + (ty>>3)*(one_d ? (w>>3)*units : 32) + (tx>>3)*units;
    uint32_t addr=VRAMB+0x10000+(uint32_t)t*32 + (ty&7)*(is8?8:4) + ((tx&7)>>(is8?0:1)); if(addr>=VRAMB+0x18000) return 0;
    int idx=*(volatile uint8_t*)(uintptr_t)addr; if(!is8){ idx = (tx&1) ? idx>>4 : idx&15; } if(!idx) return 0;
    *col=rgbpix(R16(PALRAM+0x200 + 2*(is8? idx : pal*16+idx))); return 1; }
typedef struct { int W, H, one_d, sprsm; double sx, sy; } SpCtx;
static uint16_t sp_tex[128][64*64]; static uint8_t sp_ok[128];       // every sprite's texels decoded once a picture (colour | 0x8000, 0 = clear): the row workers only read it
static void nv_sprprep(int one_d){
    static const uint8_t dim[3][4][2]={{{8,8},{16,16},{32,32},{64,64}},{{16,8},{32,8},{32,16},{64,32}},{{8,16},{8,32},{16,32},{32,64}}};
    for(int i=0;i<128;i++){ sp_ok[i]=0; uint32_t o=OAMB+i*8; uint16_t a0=R16(o), a1=R16(o+2), a2=R16(o+4); int am=(a0>>8)&3, gm=(a0>>10)&3; if(am==2||gm==2) continue; int shape=(a0>>14)&3, size=(a1>>14)&3; if(shape==3) continue;
        int w=dim[shape][size][0], h=dim[shape][size][1], is8=(a0>>13)&1, pal=(a2>>12)&15, tile=a2&1023; int y0=a0&255; if(y0>=160) y0-=256; if(y0+h*2<-60 || y0>220) continue;
        uint16_t *d=sp_tex[i]; for(int ty=0;ty<h;ty++) for(int tx=0;tx<w;tx++){ uint16_t col; d[ty*w+tx]=spr_tex(tile,tx,ty,w,h,one_d,is8,pal,&col) ? (uint16_t)(col|0x8000) : 0; } sp_ok[i]=1; if(getenv("PC_SPRDBG")&&g_frame==3140) fprintf(stderr,"spr %d: %dx%d aff%d dbl%d at %d,%d prio %d\n",i,w,h,(am&1),am==3,a1&511,y0,(a2>>10)&3); } }
static void nv_sprites(void *vc, int lo, int hi){
    SpCtx *c=(SpCtx*)vc; int W=c->W, H=c->H, one_d=c->one_d, sprsm=c->sprsm; double sx=c->sx, sy=c->sy; (void)H;

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
            double kox=0, koy=0; if(kn_on && y0+bh/2.0>=KN.g0-2){ double fx,fy; kn_fwd(x0+bw/2.0,y0+bh/2.0,&fx,&fy); kox=fx-(x0+bw/2.0); koy=fy-(y0+bh/2.0); }
            double x0d=x0+kox, y0d=y0+koy;
            int ox0=(int)floor(x0d/sx), ox1=(int)ceil((x0d+bw)/sx), oy0=(int)floor(y0d/sy), oy1=(int)ceil((y0d+bh)/sy);
            if(ox0<0)ox0=0; if(oy0<lo)oy0=lo; if(ox1>W)ox1=W; if(oy1>hi)oy1=hi; if(oy0>=oy1) continue;
            if(!sp_ok[i]) continue; const uint16_t *tcache=sp_tex[i];
            for(int Y=oy0;Y<oy1;Y++){
                double gy=Y*sy;                                  // the top edge of this output pixel, in game pixels
                int xa=ox0, xb=ox1;
                if(aff){ double dyy=gy-y0d-bh/2.0, A0=pa*sx, B0=pa*(-x0d-bw/2.0)+pb*dyy+w/2.0, C0=pc*sx, D0=pc*(-x0d-bw/2.0)+pd*dyy+h/2.0;      // a turned or scaled sprite: only the stretch of this row that lands inside its picture (the box of a double-size one is mostly empty)
                    for(int k2=0;k2<2;k2++){ double A=k2?C0:A0, B=k2?D0:B0, n=k2?h:w; if(A>-1e-9 && A<1e-9){ if(B<0||B>=n){ xa=xb; break; } continue; } double t1=(0-B)/A, t2=(n-B)/A, lo2=t1<t2?t1:t2, hi2=t1<t2?t2:t1; if(lo2<-1e6) lo2=-1e6; if(hi2>1e6) hi2=1e6; if(lo2>1e6||hi2<-1e6){ xa=xb; break; } int l2=(int)floor(lo2)-1, h2=(int)ceil(hi2)+2; if(l2>xa) xa=l2; if(h2<xb) xb=h2; } }
                for(int X=xa;X<xb;X++){
                    double gx=X*sx; int tx,ty;
                    if(aff){ double dx=gx-x0d-bw/2.0, dy=gy-y0d-bh/2.0; double fx=pa*dx+pb*dy+w/2.0, fy=pc*dx+pd*dy+h/2.0;
                             if(fx<0||fy<0) continue; tx=(int)fx; ty=(int)fy; if(tx>=w||ty>=h) continue; }
                    else { int ix=(int)floor(gx-x0d), iy=(int)floor(gy-y0d); if(ix<0||iy<0||ix>=bw||iy>=bh) continue; tx=hf?w-1-ix:ix; ty=vf?h-1-iy:iy; }
                    size_t q=(size_t)Y*W+X;
                    if((nv_op[q]&7)<4 && (nv_op[q]&3)<=prio) continue;
                    uint16_t tcv=tcache[ty*w+tx]; if(!tcv) continue;
                    nv_oc[q]=tcv&0x7FFF; nv_op[q]=(uint8_t)(prio | (gm==1?16:0));
                    if(aff && sprsm){                                      // a scaled or turned sprite: the colour is a bilinear mix of the opaque texels around, the edge stays as it was
                        double ux=pa*(gx-x0d-bw/2.0)+pb*(gy-y0d-bh/2.0)+w/2.0-0.5, uy=pc*(gx-x0d-bw/2.0)+pd*(gy-y0d-bh/2.0)+h/2.0-0.5; int ix=(int)floor(ux), iy=(int)floor(uy); double ax=ux-ix, ay=uy-iy;
                        double wsum=0, r=0, g2=0, b2=0; for(int jy=0;jy<2;jy++) for(int jx=0;jx<2;jx++){ int qx=ix+jx, qy=iy+jy; if(qx<0||qy<0||qx>=w||qy>=h) continue; uint16_t cc=tcache[qy*w+qx]; if(!cc) continue; double wt=(jx?ax:1-ax)*(jy?ay:1-ay); wsum+=wt; r+=wt*(cc&31); g2+=wt*((cc>>5)&31); b2+=wt*((cc>>10)&31); }
                        if(wsum>0.01) nv_oc[q]=(uint16_t)((int)(r/wsum+0.5) | (int)(g2/wsum+0.5)<<5 | (int)(b2/wsum+0.5)<<10); }
                }
            }
        }
    }
static int bl_y0, bl_y1;                                       // the rows of the picture a native 3D view made this frame (ULTRA: the bloom works on these only)
// ---- ULTRA bloom: bright parts of the 3D picture (suns, lamps, sparks, lava, hit flashes) spill a soft glow. Bright-pass into a 1/B picture (every 2nd pixel is looked at), a box blur both ways, added back
// with bilinear sampling, only where the blurred picture holds light (most of a frame holds none).
typedef struct { uint32_t *out; int W,H,y0,y1,B,bw,bh; float *a,*b; uint8_t *nz; } BlCtx;
static void bl_down(void *vc,int lo,int hi){ BlCtx *c=(BlCtx*)vc; int B=c->B; for(int by=lo;by<hi;by++) for(int bx=0;bx<c->bw;bx++){ float r=0,g=0,bl=0; int n=0; for(int j=0;j<B;j+=2){ int Y=c->y0+by*B+j; if(Y>=c->y1) break; const uint32_t *p=c->out+(size_t)Y*c->W+bx*B; for(int i=0;i<B;i+=2){ if(bx*B+i>=c->W) break; n++; uint32_t v=p[i]; int R=v>>16&255, G=v>>8&255, Bl=v&255, l=(R*3+G*6+Bl)/10; if(l>185){ float w=(l-185)/70.0f; r+=R*w; g+=G*w; bl+=Bl*w; } } } float *d=c->a+3*((size_t)by*c->bw+bx); float k=n?1.0f/n:0; d[0]=r*k; d[1]=g*k; d[2]=bl*k; } }
static void bl_h(void *vc,int lo,int hi){ BlCtx *c=(BlCtx*)vc; int bw=c->bw; for(int y=lo;y<hi;y++) for(int x=0;x<bw;x++){ float s[3]={0,0,0}; for(int i=-3;i<=3;i++){ int xx=x+i; if(xx<0||xx>=bw) continue; const float *q=c->a+3*((size_t)y*bw+xx); float w=4-(i<0?-i:i); s[0]+=q[0]*w; s[1]+=q[1]*w; s[2]+=q[2]*w; } float *d=c->b+3*((size_t)y*bw+x); d[0]=s[0]/16; d[1]=s[1]/16; d[2]=s[2]/16; } }
static void bl_v(void *vc,int lo,int hi){ BlCtx *c=(BlCtx*)vc; int bw=c->bw, bh=c->bh; for(int y=lo;y<hi;y++) for(int x=0;x<bw;x++){ float s[3]={0,0,0}; for(int i=-3;i<=3;i++){ int yy=y+i; if(yy<0||yy>=bh) continue; const float *q=c->b+3*((size_t)yy*bw+x); float w=4-(i<0?-i:i); s[0]+=q[0]*w; s[1]+=q[1]*w; s[2]+=q[2]*w; } float *d=c->a+3*((size_t)y*bw+x); d[0]=s[0]/16; d[1]=s[1]/16; d[2]=s[2]/16; c->nz[(size_t)y*bw+x]=(s[0]+s[1]+s[2])>48.0f; } }
static void bl_up(void *vc,int lo,int hi){ BlCtx *c=(BlCtx*)vc; int B=c->B, bw=c->bw, bh=c->bh; for(int Y=c->y0+lo;Y<c->y0+hi;Y++){ float fy=(Y-c->y0+0.5f)/B-0.5f; int y0=(int)__builtin_floorf(fy); float ty=fy-y0; int ya=y0<0?0:y0, yb=y0+1>=bh?bh-1:y0+1; if(ya>=bh) ya=bh-1; uint32_t *o=c->out+(size_t)Y*c->W;
    for(int bx=0;bx<bw;bx++){ int xa0=bx-1<0?0:bx-1, xb0=bx+1>=bw?bw-1:bx+1; if(!(c->nz[(size_t)ya*bw+xa0]|c->nz[(size_t)ya*bw+bx]|c->nz[(size_t)ya*bw+xb0]|c->nz[(size_t)yb*bw+xa0]|c->nz[(size_t)yb*bw+bx]|c->nz[(size_t)yb*bw+xb0])) continue;
      int X1=(bx+1)*B; if(X1>c->W) X1=c->W; for(int X=bx*B;X<X1;X++){ float fx=(X+0.5f)/B-0.5f; int x0=(int)__builtin_floorf(fx); float tx=fx-x0; int xa=x0<0?0:x0, xb=x0+1>=bw?bw-1:x0+1; if(xa>=bw) xa=bw-1; const float *p00=c->a+3*((size_t)ya*bw+xa), *p01=c->a+3*((size_t)ya*bw+xb), *p10=c->a+3*((size_t)yb*bw+xa), *p11=c->a+3*((size_t)yb*bw+xb); int add[3];
        for(int k=0;k<3;k++){ float v=(p00[k]*(1-tx)+p01[k]*tx)*(1-ty)+(p10[k]*(1-tx)+p11[k]*tx)*ty; add[k]=(int)(v*1.2f); }
        if(!(add[0]|add[1]|add[2])) continue; uint32_t v=o[X]; int r=(v>>16&255)+add[0], g=(v>>8&255)+add[1], b=(v&255)+add[2]; if(r>255) r=255; if(g>255) g=255; if(b>255) b=255; o[X]=(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; } } } }
static void pc_bloom(uint32_t *out,int W,int H,int y0,int y1){
    if(y1>H) y1=H; if(y0<0) y0=0; if(y1-y0<16 || W<480) return; int B=W/200; if(B<4) B=4; int bw=(W+B-1)/B, bh=(y1-y0+B-1)/B; static float *fa,*fb; static uint8_t *fz; static size_t cap; size_t n=(size_t)bw*bh*3; if(n>cap){ free(fa); free(fb); free(fz); fa=malloc(n*4); fb=malloc(n*4); fz=malloc((size_t)bw*bh); cap=n; }
    BlCtx c={out,W,H,y0,y1,B,bw,bh,fa,fb,fz}; pc_par_chunk(bh,4,bl_down,&c); pc_par_chunk(bh,8,bl_h,&c); pc_par_chunk(bh,8,bl_v,&c); pc_par_chunk(y1-y0,16,bl_up,&c); }
#include "pc_pncomp.inc"
#include "pc_sncomp.inc"
// the native dungeon (pc_dnative.inc), shown wherever the page still holds what the 3D pass wrote. Its picture may be smaller than the window (the quality levels): f = window / picture.
typedef struct { uint32_t *out; int W,H,f,Y0,Y0i,fadeop,ev; const uint8_t *pgb,*sn1,*sn2; const uint32_t *spl; double sx,sy; uint32_t pal[256]; } DcCC;
static void dc_rows(void *vc,int lo,int hi){
    DcCC *c=(DcCC*)vc; int W=c->W, f=c->f, Wi=pc_dn_w, fadeop=c->fadeop, ev=c->ev; uint32_t *out=c->out; const uint32_t *pal=c->pal;
    for(int Y=c->Y0+lo;Y<c->Y0+hi;Y++){ int gy=(int)((Y+0.5)*c->sy); if(gy>159) gy=159; if(gy<18) continue;
        int Yi=Y/f; if(Yi<c->Y0i) Yi=c->Y0i; size_t ro=(size_t)(Yi-c->Y0i)*Wi; const uint32_t *src=pc_dn_buf+ro; const uint32_t *sp=c->spl?c->spl+ro:0;
        for(int X=0;X<W;X++){ int gx=(int)((X+0.5)*c->sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X; int Xi=f==1?X:X/f;
            if((nv_op[q]&7)<4) continue;
            uint32_t v=src[Xi], sv=sp?sp[Xi]:0;
            if(sv){ if(c->pgb[gi]!=c->sn2[gi]) continue; if(sv&0x80000000u) v=sv; else if(v&0x20000000u) continue; }         // a thing drawn natively: its own pixel (or the world behind it), while the page still holds what the sprite pass left
            else if(c->pgb[gi]!=c->sn1[gi] || (v&0x20000000u)) continue;
            uint32_t col; if((v&0x90000000u)==0x90000000u) col=v&0xFFFFFF;          // a generated creature's own colour (pc_dnative.inc)
            else { uint32_t a=pal[v&255], b=pal[(v>>8)&255]; int fr=(v>>16)&255; uint32_t rb=((a&0xFF00FF)*(256-fr)+(b&0xFF00FF)*fr)>>8, g=((a&0xFF00)*(256-fr)+(b&0xFF00)*fr)>>8; col=(rb&0xFF00FF)|(g&0xFF00); }
            if(fadeop==2){ uint32_t r2=(col>>16&255)+((255-(col>>16&255))*ev>>4), g2=(col>>8&255)+((255-(col>>8&255))*ev>>4), b2=(col&255)+((255-(col&255))*ev>>4); col=r2<<16|g2<<8|b2; }
            else if(fadeop==3){ uint32_t r2=(col>>16&255)*(16-ev)>>4, g2=(col>>8&255)*(16-ev)>>4, b2=(col&255)*(16-ev)>>4; col=r2<<16|g2<<8|b2; }
            out[q]=col; nv_op[q]|=0x40; } }
}
static void dn_compose(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    if(!(pc_dn_w>0 && pc_dn_h>0 && pc_dn_sc>=1 && pc_dn_w==(W+pc_dn_sc-1)/pc_dn_sc && pc_dn_h==(H+pc_dn_sc-1)/pc_dn_sc && pc_dn_buf && !pc_v3d_classic())) return;
    int t=(dc>>4)&1; if(g_frame-pc_dn_f[t]>4) return;
    static DcCC c; c.out=out; c.W=W; c.H=H; c.f=pc_dn_sc; c.Y0=(18*H+159)/160; c.Y0i=(18*pc_dn_h+159)/160; c.pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0));
    for(int i=0;i<256;i++){ uint16_t cc=R16(PALRAM+i*2); c.pal[i]=(uint32_t)((cc&31)<<3|(cc&31)>>2)<<16 | (uint32_t)(((cc>>5)&31)<<3|((cc>>5)&31)>>2)<<8 | (uint32_t)(((cc>>10)&31)<<3|((cc>>10)&31)>>2); }
    c.fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0; c.ev=ev; bl_y0=c.Y0; bl_y1=H; c.spl=pc_dn_spl_on?pc_dn_spl:0; c.sn1=pc_dn_snap[t]; c.sn2=pc_dn_snap2[t]; c.sx=240.0/W; c.sy=160.0/H;
    pc_par_chunk(H-c.Y0,16,dc_rows,&c);
}
void pc_compose_native(uint32_t *out, int W, int H, int smooth){
    double q_t0=pc_now_ms();
    if(W<240||H<160){ W=240; H=160; }
    if(W>240 && H>160){ pc_nat_w=W; pc_nat_h=H; }       // (the game's own native renderers, e.g. the dungeon, make their picture at this size)
    build_lines();
    size_t need=(size_t)W*H; if(need>nv_cap){ free(nv_oc); free(nv_op); nv_oc=malloc(need*2); nv_op=malloc(need); nv_cap=need; }
    if(W+H>nv_fcap){ free(nv_fx); nv_fcap=W+H; nv_fx=malloc(sizeof(float)*(W+H)); } nv_fy=nv_fx+W;      // (nv_fy must follow every new W: after a resize it used to point past the end of the buffer)
    uint16_t dc=R16(IOREG), mode=dc&7; int one_d=(dc>>6)&1;
    uint16_t bd=rgbpix(R16(PALRAM));
    uint8_t lut5[32]; for(int i=0;i<32;i++) lut5[i]=(uint8_t)(i<<3|i>>2);
    #define XRGB(c) ((uint32_t)lut5[(c)&31]<<16 | (uint32_t)lut5[((c)>>5)&31]<<8 | lut5[((c)>>10)&31])
    if(dc&0x80){ for(size_t i=0;i<need;i++) out[i]=0xFFFFFF; return; }
    { static uint32_t *ovl; static size_t ovl_cap; nv_ovl=0; if(pc_ovl_fn){ if(need>ovl_cap){ free(ovl); ovl=malloc(need*4); ovl_cap=need; } pc_ovl_fn(ovl,W,H); nv_ovl=ovl; } }
    const double sx=240.0/W, sy=160.0/H;                       // game pixels per output pixel
    bl_y0=bl_y1=0;
    struct timespec T0,T1,T2,T3,T4; clock_gettime(CLOCK_MONOTONIC,&T0);
    // ---- sprites
    kn_prep(W,H,R16(IOREG)&7); memset(nv_op,4,need); int sprsm = nv_v3()!=2 && lst_dma && W>240;
    if((dc>>12)&1){ SpCtx c={W,H,one_d,sprsm,sx,sy}; nv_sprprep(one_d); pc_par(H,nv_sprites,&c); }
    clock_gettime(CLOCK_MONOTONIC,&T1);
    // ---- bitmap layer + blend
    uint16_t bldcnt=R16(IOREG+0x50), bldalpha=R16(IOREG+0x52); int ev=R16(IOREG+0x54)&31; if(ev>16) ev=16;
    int bmode=(bldcnt>>6)&3; int ea=bldalpha&31, eb=(bldalpha>>8)&31; if(ea>16)ea=16; if(eb>16)eb=16;
    int bgprio=R16(IOREG+0x0C)&3; int bg2_on=(dc>>10)&1;
    int16_t pa=(int16_t)R16(IOREG+0x20), pb=(int16_t)R16(IOREG+0x22), pc=(int16_t)R16(IOREG+0x24), pd=(int16_t)R16(IOREG+0x26);
    int32_t rx=(int32_t)(R32(IOREG+0x28)<<4)>>4, ry=(int32_t)(R32(IOREG+0x2C)<<4)>>4;
    uint32_t base = VRAMB + ((dc&0x10) ? 0xA000 : 0);
    int have_bg = bg2_on && mode>=3 && mode<=5, bw=240, bh=160;
    if(have_bg) nv_resolve_bg(mode,base,dc,&bw,&bh,bd);
    int v3=nv_v3();
    int dmaln = have_bg && lst_dma && mode==4, nat3 = dmaln && v3!=2;
    if(dmaln){                                                   // both pages: each line says which one it shows
        for(int y=0;y<160;y++) for(int x=0;x<240;x++){ int i0=*(volatile uint8_t*)(uintptr_t)(VRAMB+y*240+x), i1=*(volatile uint8_t*)(uintptr_t)(VRAMB+0xA000+y*240+x);
            bghave[y][x]=i0!=0; bgres[y][x]=i0?rgbpix(R16(PALRAM+i0*2)):bd; bghave2[y][x]=i1!=0; bgres2[y][x]=i1?rgbpix(R16(PALRAM+i1*2)):bd; }
        if(nat3) nv_ground_prep(bd); if(nat3 && kn_on) kn_pass(W,H,sx,sy,bd); else kn_on=0; }
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
    clock_gettime(CLOCK_MONOTONIC,&T2);
    if(mode==4 && !lst_dma){ dn_compose(out,W,H,dc,bmode,ev,bldcnt); pn_compose(out,W,H,dc,bmode,ev,bldcnt); sn_compose(out,W,H,dc,bmode,ev,bldcnt); }      // the native 3D pictures first: the bitmap pass skips every pixel they show (nv_op bit 0x40)
    { NvCtx c; c.dmaln=dmaln; c.nat3=nat3; c.have_bg=have_bg; c.W=W; c.H=H; c.bgprio=bgprio; c.axis=axis; c.bw=bw; c.bh=bh; c.ev=ev; c.bmode=bmode; c.ea=ea; c.eb=eb; c.sx=sx; c.sy=sy; c.bldcnt=bldcnt; c.bldalpha=bldalpha; c.bd=bd; c.tabx=tabx; c.taby=taby; c.wx=wx; c.wy=wy; c.rx=rx; c.ry=ry; c.pa=pa; c.pb=pb; c.pc=pc; c.pd=pd; c.out=out; memcpy(c.lut5,lut5,32); pc_par(H,nv_rows,&c); }
    free(tabx); free(wx);
    clock_gettime(CLOCK_MONOTONIC,&T3);
    if(getenv("PC_TIMING")){ double a=(T1.tv_sec-T0.tv_sec)*1e3+(T1.tv_nsec-T0.tv_nsec)/1e6, b=(T2.tv_sec-T1.tv_sec)*1e3+(T2.tv_nsec-T1.tv_nsec)/1e6, c=(T3.tv_sec-T2.tv_sec)*1e3+(T3.tv_nsec-T2.tv_nsec)/1e6; fprintf(stderr,"compose %dx%d: sprites %.2f prep %.2f main %.2f ms\n",W,H,a,b,c); }
    if(pc_ships_mode>=0 && mode==4 && !(bmode>=2 && ev>=12)){                 // the ship renderer, under whatever the game drew after its ships (cockpit, HUD, sparks)
        int t=(dc>>4)&1; if(getenv("SR_DBG")) fprintf(stderr,"sr: page %d tag %d snapf %ld/%ld frame %ld n=%d\n",t,sr_shown_tag(),sr_snap_f[0],sr_snap_f[1],g_frame,sr_count());
        if(sr_select(t) && g_frame-sr_snap_f[t]<=4){
            static uint8_t mk[240*160]; int same=0; const uint8_t *pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0));
            for(int i=0;i<240*128;i++){ int e= pgb[i]==sr_snap[t][i] || sn_foot_still(t,i,pgb[i]); same+=e; mk[i]= e ? (pgb[i]==0 ? 2 : 1) : 0; } memset(mk+240*128,0,240*32);
            if(same>240*128*6/10){ sr_set_par(pc_par_chunk,pc_par_threads()); sr_render(out,W,H,mk,0); } } }
    if(mode==4 && !lst_dma) sn_items(out,W,H,dc,bmode,ev,bldcnt);
    if(mode==4 && !lst_dma) sn_cockpit(out,W,H,dc,bmode,ev,bldcnt);
    if(bl_y1>bl_y0 && pc_v3d_ultra() && !(bmode>=2 && ev>0)) pc_bloom(out,W,H,bl_y0,bl_y1);
    pc_q_tick(pc_now_ms()-q_t0);
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
static unsigned long g_vc_reads, g_tk_poll, g_tk_step; static uint64_t g_cyc;
void pc_tick_poll(void){ g_tk_poll++; }
void pc_tick_step(void){ g_tk_step++; }          // (FRAMELOG, env PC_FRAMELOG=file: one line per vblank = frame number, ms clock, REG_VCOUNT reads, key polls, dungeon steps, timer cycles)
static void vblank(void){
    { static int fl=-1; static FILE *ff; static unsigned long ls_reads; if(fl<0){ const char *e=getenv("PC_FRAMELOG"); fl=0; if(e&&*e){ ff=fopen(e,"a"); fl=ff!=0; if(ff) fprintf(ff,"# run\n"); } }
      if(fl){ fprintf(ff,"%ld %.3f %lu %lu %lu %llu\n",(long)g_frame+1,pc_now_ms(),g_vc_reads,g_tk_poll,g_tk_step,(unsigned long long)g_cyc); ls_reads=g_vc_reads; } (void)ls_reads; }
    g_vc_reads=0; g_tk_poll=0; g_tk_step=0;
    { static double lt, sum; static int on=-1, nf; static FILE *lf;             // FRAME LOG: put an empty file slowlog.on next to the exe; slowlog.txt then lists every frame over 33 ms (and an average line every 300 frames)
      if(on<0){ FILE *t=fopen("slowlog.on","rb"); on=t!=0; if(t) fclose(t); if(on) lf=fopen("slowlog.txt","a"); if(!lf) on=0; if(on) fprintf(lf,"--- start, window %dx%d\n",pc_nat_w,pc_nat_h); }
      if(on){ double nw=pc_now_ms(); if(lt){ double d=nw-lt; sum+=d; if(d>33) { fprintf(lf,"frame %ld: %.0f ms (dungeon renderer %.1f ms)\n",(long)g_frame,d,pc_dn_last_ms); fflush(lf); } if(++nf==300){ fprintf(lf,"frame %ld: average %.1f ms over 300 frames\n",(long)g_frame,sum/300); fflush(lf); nf=0; sum=0; } } lt=nw; } }
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
// g_cyc: CPU cycles since start, as the beam counts them: one REG_VCOUNT read = one line = 1232 cycles, 228 lines = one frame = 280896 cycles (the GBA's 59.7275 Hz)
uint32_t pc_tmr(void){ return (uint32_t)g_cyc; }
uint16_t pc_vcount(void){
    g_vc_reads++; g_cyc+=1232; R16(IOREG+0x100)=(uint16_t)g_cyc; R16(IOREG+0x104)=(uint16_t)(g_cyc>>16);      // (TM0 / TM1 counters, for code that reads the registers itself)
    if(++g_line>=228) g_line=0;
    uint16_t ds=R16(IOREG+4); ds=(uint16_t)((ds&~7)|((g_line>=160&&g_line<227)?1:0)); R16(IOREG+4)=ds;
    R16(IOREG+6)=(uint16_t)g_line;
    if(g_line==160) vblank();
    return (uint16_t)g_line;
}

int pc_mouse_x = -1, pc_mouse_y = -1, pc_mouse_b, pc_mouse_wheel;
