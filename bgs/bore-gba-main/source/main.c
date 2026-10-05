// BORE - GBA voxel creature creator (tech demo)
// Build space: 6 wide (X) x 4 long (Z) x 8 high (Y). Mode 3, no libraries.
// EYE and MOUTH are 2D sprites painted straight onto the front (+Z) face of the voxel under the cursor.
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;

#define REG_DISPCNT (*(volatile u16*)0x04000000)
#define REG_VCOUNT  (*(volatile u16*)0x04000006)
#define REG_KEYINPUT (*(volatile u16*)0x04000130)
#define REG_DMA3SAD (*(volatile u32*)0x040000D4)
#define REG_DMA3DAD (*(volatile u32*)0x040000D8)
#define REG_DMA3CNT (*(volatile u32*)0x040000DC)
#define VRAM_ADDR 0x06000000u
#define EWRAM_BSS __attribute__((section(".sbss"), aligned(4)))
// Hot loops run as ARM code from IWRAM (32-bit, zero-wait bus) instead of Thumb from the 16-bit ROM bus.
#define IWRAM_CODE __attribute__((section(".iwram"), target("arm"), long_call))
#define REG_WAITCNT (*(volatile u16*)0x04000204)

#define SW 240
#define SH 160
#define W 6
#define D 4
#define H 8
// Voxel size: change CA (and CC) to resize everything; the starter model, sprites and limbs all scale with them.
#define CA 8    // cube half width (was 10)
#define CB (CA/2)   // cube half height of top face
#define CC 8    // cube side height (was 10)
#define HUG ((CA*3+5)/10)   // limb inset toward the torso (was 3 at CA 10)
#define OX 50
#define OY 96
#define PANEL_X 124

enum { K_A=1, K_B=2, K_SEL=4, K_START=8, K_RIGHT=16, K_LEFT=32, K_UP=64, K_DOWN=128, K_R=256, K_L=512 };

#define RGB(r,g,b) ((u16)((r)|((g)<<5)|((b)<<10)))
static u16 fb[SW*SH] EWRAM_BSS;
#define SFX_MAX 124000   // RAM for the decoded sound effect (also used as the title backdrop before the game starts)
static u8 sfxRam[SFX_MAX] EWRAM_BSS;
#define tfb ((u16*)sfxRam)   // pre-rendered title backdrop (only needed while the title screen shows)

// ---------- settings (kept in SRAM; the SETTINGS screen edits them) ----------
static u8 sFps=1;    // frame rate: 0 = 60, 1 = 30, 2 = 20, 3 = 15 frames per second (game speed stays the same)
static u8 sWall=1;   // walls: 0 full height, 1 cutaway (walls in front drop low), 2 all low
static u8 sWp=1;     // wallpaper patterns on
static u8 sFl=1;     // floor patterns on
static u8 sSnd=1;    // sound on
static u8 sShad=1;   // shadows under the player
static u8 sHud=0;    // on-screen info: 0 full, 1 slim, 2 off
static u8 sRom=0;    // ROM waits: 0 fast (3/1 + prefetch), 1 safe (power-on default, for fussy flash carts)
static u8 sUnlock=0;  // 1 = the Konami code was entered on the title screen: START+SELECT in the creator swaps creator screens
static u8 sClassic=0; // 1 = the secret classic creature screen (toggled with UP UP DOWN DOWN in the creator)
static u8 sNoWarn=0; // 1 = hide the TOO SLOW FOR THIS FRAME RATE warning in settings
static u8 sShow=0;   // performance counter: 0 off, 1 fps, 2 fps + load
static int cview;    // room view while the action cam spins (0..3, quarter turns); always 0 in the editor
static int lcN, lcPts, lcT, lcBank, lcBankT, lcamPend, lcamF;   // combo chain: tricks, points, time left, banked total + display time, cam queued, cam frame
static u8 sCam=1;    // action cam after a big combo: 0 off, 1 over 10000, 2 over 5000, 3 over 2000
static const int camThr[4]={0,10000,5000,2000};
#define CAM_LEN 84    // action cam length in game steps (1.4 s)
#define CAM_ZOOM 62   // zoom in by 256/(256-62) = 1.3x
static int lloadV;   // work per drawn frame as a percent of its time budget (PERFORMANCE INFO: DETAIL)
#define NWP 14       // wallpapers
#define NFL 14       // floors
#include "opts.h"   // extended options (xo[]): gameplay, input, audio, HUD and room options; also defines GOLD (the accent colour)


// ---------- palette ----------
// Colour rows of the creature creator: 8 swatches each. Swatch 0 of every row is the starter creature's colour.
#define NSW 8
static const u16 skinTones[NSW] = { RGB(30,23,17), RGB(24,16,10), RGB(19,12,7), RGB(13,8,5), RGB(14,26,10), RGB(10,19,29), RGB(22,13,27), RGB(31,17,19) };
static const u16 hairTones[NSW] = { RGB(5,3,2), RGB(14,8,4), RGB(27,21,6), RGB(28,8,4), RGB(21,21,22), RGB(10,22,12), RGB(8,12,28), RGB(30,14,22) };
static const u16 topTones[NSW]  = { RGB(8,20,22), RGB(28,8,6), RGB(30,24,6), RGB(10,24,8), RGB(8,10,26), RGB(22,10,26), RGB(30,30,30), RGB(5,5,8) };
static const u16 botTones[NSW]  = { RGB(8,9,20), RGB(5,5,8), RGB(18,12,6), RGB(14,15,16), RGB(24,20,12), RGB(8,16,8), RGB(26,6,6), RGB(30,30,30) };
// The look: one number per choice in the creature creator. 0 everywhere = the starter creature.
enum { LK_SHAPE, LK_SKIN, LK_EYES, LK_MOUTH, LK_EARS, LK_HSTYLE, LK_HCOL, LK_TOP, LK_BOT, LK_N };
static u8 look[LK_N];
// ---- life stages ----  BABY (cannot be steered, walks about by itself), CHILD, TEEN, ADULT, ELDER (the last stage, slower and stooped). The creator's room to build in is smaller
// when young and grows with the age: a box of stBW x stBD x stBH blocks centred on the floor, a biggest block size and a list of looks
// each part picker may use. The adult box is the whole 6x4x8 space, so every old person and save is an ADULT.
enum { AG_BABY, AG_CHILD, AG_TEEN, AG_ADULT, AG_ELDER, AG_N };
static u8 stage=AG_ADULT;   // current life stage
static u8 ageDays;          // game days lived in this stage (grows the creature when it reaches the days set on the OPTIONS > AGES page, saved with the person)
static const char* const stageNm[AG_N]={"BABY","CHILD","TEEN","ADULT","ELDER"};
static const u8 stBW[AG_N]={4,4,6,6,6}, stBD[AG_N]={4,4,4,4,4}, stBH[AG_N]={5,6,7,8,7};   // build box (width is always even: parts mirror around its centre)
static const u8 stMaxSz[AG_N]={2,2,3,3,3};          // biggest block size S/M/L the builder offers
static const u8 stLegs[AG_N]={0,1,2,3,2};   // (an elder is stooped: a block lower than an adult)           // leg blocks showing under the torso before the shape trims them
static const u8 stSpd[AG_N]={50,80,95,100,70};       // walking speed in percent
// allowed looks per stage: bit n set = option n may be picked. Shape: AVERAGE BROAD BIG-HEAD STUBBY. Ears: NONE SMALL BIG. Hair: CROP BOWL LONG BALD.
static const u8 stMaskShape[AG_N]={12,13,13,15,15}, stMaskEars[AG_N]={3,7,7,7,7}, stMaskHair[AG_N]={9,11,15,15,15};
static const u8 stSwatches[AG_N]={4,6,8,8,8};       // how many colours of each row are on offer
#define BX0 ((W-stBW[stage])/2)
static u16 base[9+NWP], sT[9+NWP], sL[9+NWP], sR[9+NWP];   // slots 1..8 = body colours, 9.. = wallpaper average colours
static u16 dL[4], dR[4];   // face-sprite palette (k w r s) pre-shaded for the left / right cube face
#define EDGE RGB(3,2,5)
#define SKY  RGB(20,26,31)
#define PANEL RGB(5,6,9)

static inline __attribute__((always_inline)) u16 shade(u16 c, int n) {
    int r=c&31, g=(c>>5)&31, b=(c>>10)&31;
    return RGB(r*n/16, g*n/16, b*n/16);
}
// ---------- wallpapers & floors: 8x8 texels, each char 0-3 picks one of the 4 colours ----------
typedef struct { const char*nm; u16 c[4]; const char*p[8]; } Tex;
#define PN_FLAT  "00000000","00000000","00000000","00000000","00000000","00000000","00000000","00000000"
#define PN_NOISE "01000200","00020010","20001000","00100020","01000100","00200001","10002000","00010200"
#define PN_CONC  "00000100","02000000","00001000","00000020","00100000","00000002","20000000","00020100"
#define PN_TILE  "11111111","12001200","10001000","10001000","11111111","12001200","10001000","10001000"
#define PN_STEEL "00000000","01110000","00220000","00000000","00000000","00000111","00000022","00000000"
#define PN_HAZ   "00110011","10011001","11001100","01100110","00110011","10011001","11001100","01100110"
// Wallpaper tiles repeat every block (8 px), so they line up across a whole wall. Colours: house first, factory after.
static const Tex wpTex[NWP]={
 {"TEAL PAINT",{RGB(8,20,22),0,0,0},{PN_FLAT}},
 {"FLORAL",{RGB(28,26,20),RGB(26,12,16),RGB(10,20,9),RGB(30,26,8)},{"00000000","00100000","01310020","00102200","00000000","00000100","20001310","02200100"}},
 {"PEACH STRIPE",{RGB(30,23,18),RGB(31,29,24),RGB(26,16,15),0},{"11102000","11102000","11102000","11102000","11102000","11102000","11102000","11102000"}},
 {"MEMPHIS",{RGB(30,30,28),RGB(4,22,22),RGB(29,9,18),RGB(31,27,5)},{"01000010","10100101","00000000","00022000","00222200","00000030","30000000","00000000"}},
 {"WOOD PANEL",{RGB(18,11,5),RGB(11,6,3),RGB(22,14,7),0},{"10201000","10001020","10021002","10001000","10201020","10001000","10021002","10001000"}},
 {"DIAMONDS",{RGB(20,13,17),RGB(25,18,21),RGB(14,8,12),0},{"00010000","00101000","01000100","10020010","01000100","00101000","00010000","00000000"}},
 {"GINGHAM",{RGB(29,29,29),RGB(18,22,29),RGB(10,15,26),0},{"22112211","22112211","11001100","11001100","22112211","22112211","11001100","11001100"}},
 {"CORRUGATED",{RGB(15,17,18),RGB(22,24,25),RGB(9,11,12),0},{"10201020","10201020","10201020","10201020","10201020","10201020","10201020","10201020"}},
 {"RED BRICK",{RGB(20,8,6),RGB(22,20,18),RGB(14,5,4),0},{"00010001","02010201","00010001","11111111","01000100","01020102","01000100","11111111"}},
 {"CINDER BLOCK",{RGB(17,17,17),RGB(11,11,11),RGB(21,21,20),0},{"10000000","10200000","10000020","11111111","00001000","02001000","00001020","11111111"}},
 {"HAZARD",{RGB(30,25,2),RGB(4,4,5),0,0},{PN_HAZ}},
 {"GREEN TILE",{RGB(10,20,14),RGB(22,24,22),RGB(14,25,18),0},{PN_TILE}},
 {"STEEL PLATE",{RGB(14,16,18),RGB(22,24,26),RGB(8,9,11),0},{PN_STEEL}},
 {"CONCRETE",{RGB(16,16,15),RGB(19,19,18),RGB(12,12,11),0},{PN_CONC}},
};
// Floor textures are mapped onto each iso tile in tile space (a = along +x, b = along +y).
static const Tex flTex[NFL]={
 {"TAN CHECK",{RGB(26,21,14),0,0,0},{PN_FLAT}},
 {"BEIGE CARPET",{RGB(24,21,16),RGB(21,18,13),RGB(27,24,19),0},{PN_NOISE}},
 {"TEAL CARPET",{RGB(6,16,16),RGB(4,12,13),RGB(8,19,19),0},{PN_NOISE}},
 {"CHECKER LINO",{RGB(29,29,28),RGB(5,5,8),0,0},{"00001111","00001111","00001111","00001111","11110000","11110000","11110000","11110000"}},
 {"WOOD PLANKS",{RGB(20,13,6),RGB(12,7,3),RGB(23,16,8),0},{"11111111","10020000","10000200","10002000","11111111","02001000","00201000","00001002"}},
 {"PINK TILE",{RGB(28,18,20),RGB(30,28,27),RGB(30,22,23),0},{PN_TILE}},
 {"BLUE TILE",{RGB(9,19,26),RGB(28,29,30),RGB(14,24,29),0},{PN_TILE}},
 {"CONCRETE",{RGB(15,15,14),RGB(18,18,17),RGB(12,12,11),0},{PN_CONC}},
 {"STEEL PLATE",{RGB(12,14,16),RGB(20,22,24),RGB(7,8,10),0},{PN_STEEL}},
 {"METAL GRATE",{RGB(4,5,7),RGB(17,18,20),RGB(10,11,13),0},{"11111111","10001000","10201020","10001000","11111111","10001000","10201020","10001000"}},
 {"HAZARD",{RGB(30,25,2),RGB(4,4,5),0,0},{"00000000","00000000","11111111","11111111","00000000","00000000","11111111","11111111"}},
 {"GREEN LINO",{RGB(12,19,10),RGB(14,22,12),RGB(10,16,8),0},{PN_NOISE}},
 {"OIL STAINED",{RGB(14,14,13),RGB(8,8,9),RGB(11,11,11),0},{"00000000","00022000","00211200","00022100","00002000","00000000","00000000","00000000"}},
 {"RED TILE",{RGB(22,8,5),RGB(14,6,4),RGB(25,11,7),0},{PN_TILE}},
};
static const u8 flVs[NFL]={14,15,15,16,14,15,15,15,15,15,15,15,15,15};   // shade (of 16) for the odd tiles of a checkerboard of tiles
static u16 wpAvg[NWP];   // average colour of each wallpaper: wall tops and the "wallpaper off" look


static void setColors(void) {
    base[1]=skinTones[look[LK_SKIN]]; base[2]=RGB(31,31,31); base[3]=RGB(3,3,6);
    base[4]=RGB(29,12,16);    base[5]=hairTones[look[LK_HCOL]];
    base[6]=topTones[look[LK_TOP]]; base[7]=botTones[look[LK_BOT]]; base[8]=RGB(31,30,16);
    for (int i=1;i<9;i++){ sT[i]=base[i]; sL[i]=shade(base[i],12); sR[i]=shade(base[i],9); }
    for (int i=0;i<NWP;i++){ int s=9+i; base[s]=wpAvg[i]; sT[s]=base[s]; sL[s]=shade(base[s],12); sR[s]=shade(base[s],9); }
    u16 dc[4]={ base[3], base[2], base[4], shade(base[1],11) };   // k dark, w white, r red, s lid shadow
    for (int i=0;i<4;i++){ dL[i]=shade(dc[i],12); dR[i]=shade(dc[i],9); }
}

// ---------- drawing ----------
static inline __attribute__((always_inline)) void px(int x,int y,u16 c){ if((unsigned)x<SW && (unsigned)y<SH) fb[y*SW+x]=c; }
IWRAM_CODE static void vline(int x,int y0,int y1,u16 c){
    if((unsigned)x>=SW) return; if(y0<0)y0=0; if(y1>=SH)y1=SH-1;
    for(;y0<=y1;y0++) fb[y0*SW+x]=c;
}
IWRAM_CODE static void rect(int x,int y,int w,int h,u16 c){ for(int j=0;j<h;j++)for(int i=0;i<w;i++)px(x+i,y+j,c); }
IWRAM_CODE static void line(int x0,int y0,int x1,int y1,u16 c){
    int dx=x1>x0?x1-x0:x0-x1, dy=y1>y0?y0-y1:y1-y0, sx=x0<x1?1:-1, sy=y0<y1?1:-1, e=dx+dy;
    for(;;){ px(x0,y0,c); if(x0==x1&&y0==y1)break; int e2=2*e;
        if(e2>=dy){e+=dy;x0+=sx;} if(e2<=dx){e+=dx;y0+=sy;} }
}

// Smooth proportional font (generated by tools/make_font.py from assets/font): 9 coverage steps blended over the screen.
#include "fontdata.h"
static int fIdx(char ch){ const char*p=FNT_CHARS; for(int i=0;*p;p++,i++) if(*p==ch) return i; return -1; }
// width in pixels of a string at scale sc (1 = small, 2 = medium, 3+ = large)
static int tw(const char*s,int sc){
    const u8*adv=sc<=1?fa_s:sc==2?fa_m:fa_l; int sp=sc<=1?FSP_s:sc==2?FSP_m:FSP_l, w=0;
    for(;*s;s++){ int i=fIdx(*s); w+=(i<0)?sp:adv[i]; } return w;
}
IWRAM_CODE static int text(int x,int y,const char*s,u16 c,int sc){
    const u32*fo=sc<=1?fo_s:sc==2?fo_m:fo_l; const u8*fw=sc<=1?fw_s:sc==2?fw_m:fw_l, *fa=sc<=1?fa_s:sc==2?fa_m:fa_l, *fp=sc<=1?fp_s:sc==2?fp_m:fp_l;
    int fh=sc<=1?FH_s:sc==2?FH_m:FH_l, sp=sc<=1?FSP_s:sc==2?FSP_m:FSP_l;
    int cr=c&31, cg=(c>>5)&31, cb=(c>>10)&31;
    for(;*s;s++){
        int i=fIdx(*s); if(i<0){ x+=sp; continue; }
        int w=fw[i]; const u8*g=fp+fo[i];
        for(int r=0;r<fh;r++){ int yy=y+r; if((unsigned)yy>=SH){ g+=w; continue; }
            u16*d=&fb[yy*SW];
            for(int q=0;q<w;q++){ int a=g[q]; if(!a) continue; int xx=x+q; if((unsigned)xx>=SW) continue;
                if(a>=8){ d[xx]=c; continue; }
                u16 b=d[xx]; int ia=8-a;
                int R=((b&31)*ia+cr*a)>>3, G=(((b>>5)&31)*ia+cg*a)>>3, B=(((b>>10)&31)*ia+cb*a)>>3;
                d[xx]=(u16)(R|(G<<5)|(B<<10)); }
            g+=w; }
        x+=fa[i];
    }
    return x;
}
static void num(int x,int y,int n,u16 c){ char s[2]={(char)('0'+n),0}; text(x,y,s,c,1); }

static inline __attribute__((always_inline)) u16 lite(u16 c,int n){
    int r=(c&31)*n/16, g=((c>>5)&31)*n/16, b=((c>>10)&31)*n/16;
    if(r>31)r=31; if(g>31)g=31; if(b>31)b=31; return RGB(r,g,b);
}
// Soft voxel: tonal outline only on the silhouette (no seams between joined blocks), lit rim, shaded base.
// shape 0 block, 1 slim limb, 2 hand, 3 leg. f: 1 block above, 2 block below, 16/32 coplanar neighbour at left/right edge.
static const u8 rTab[4]={CA,CA*7/10,CA/2,CA*4/5};   // block, slim limb, hand, leg half widths
static u8 hhT[4][CA+1];   // hhT[shape][|t|] = (r/2)*(r-|t|)/r, filled once in initTables (no division in the hot loop)
IWRAM_CODE static void cube(int sx,int sy,int ci,int shape,int f){
    int r=rTab[shape], ch=shape==2?CC*7/10:CC; const u8*hhp=hhT[shape];
    if(shape) f&=3;
    u16 T=sT[ci], L=sL[ci], R=sR[ci], eT=shade(T,9), eL=shade(L,9), eR=shade(R,9);
    for(int t=-r;t<=r;t++){
        int at=t<0?-t:t, hh=hhp[at], x=sx+t, yt=sy+hh, yb=yt+ch-1;
        u16 sc=t<0?L:R, ec=t<0?eL:eR;
        vline(x,yt,yb,sc);
        vline(x,sy-hh,sy+hh,T);
        if(!(f&1)){ px(x,yt+1,lite(sc,19)); px(x,sy-hh,eT); }
        if(!(f&2)){ px(x,yb-1,shade(sc,13)); px(x,yb,ec); }
        if((t==-r&&!(f&16))||(t==r&&!(f&32))) vline(x,sy-hh,yb,ec);
    }
}

// ---------- textured walls and floors ----------
static u16 wpTab[NWP][2][8][8] EWRAM_BSS;              // [wallpaper][0 left face / 1 right face][column][row], pre-shaded
static u16 flTab[NFL][2][2*CB+1][2*CA+1] EWRAM_BSS;    // [floor][odd tile][row][column] pre-sampled onto the iso diamond (row-major: drawn as horizontal spans)
static u8 rowHW[CB+1];   // rowHW[|y|] = half width of the diamond on that row
static u16 flFlat[NFL][2];                             // plain-colour fallback ("floor patterns off")
static u16 avgTex(const Tex*t){
    int r=0,g=0,b=0;
    for(int v=0;v<8;v++)for(int u=0;u<8;u++){ u16 c=t->c[t->p[v][u]-'0']; r+=c&31; g+=(c>>5)&31; b+=(c>>10)&31; }
    return RGB(r/64,g/64,b/64);
}
static void bakeTex(void){   // needs hhT (filled by initTables)
    for(int w=0;w<NWP;w++){
        const Tex*t=&wpTex[w]; wpAvg[w]=avgTex(t);
        for(int f=0;f<2;f++)for(int u=0;u<8;u++)for(int v=0;v<8;v++) wpTab[w][f][u][v]=shade(t->c[t->p[v][u]-'0'],f?9:12);
    }
    for(int fl=0;fl<NFL;fl++){
        const Tex*t=&flTex[fl]; u16 av=avgTex(t); int vs=flVs[fl];
        flFlat[fl][0]=av; flFlat[fl][1]=shade(av,vs);
        for(int var=0;var<2;var++)for(int tt=-CA;tt<=CA;tt++){
            int hh=hhT[0][tt<0?-tt:tt];
            for(int y=-hh;y<=hh;y++){
                int X=tt*CB+y*CA+CA*CB, Y=y*CA-tt*CB+CA*CB;      // tile-space position, 0..2*CA*CB
                int ta=X*8/(2*CA*CB), tb=Y*8/(2*CA*CB);
                if(ta<0)ta=0; if(ta>7)ta=7; if(tb<0)tb=0; if(tb>7)tb=7;
                u16 c=t->c[t->p[tb][ta]-'0']; if(var) c=shade(c,vs);
                flTab[fl][var][y+CB][tt+CA]=c;
            }
        }
    }
}
// One wall block with its wallpaper on both faces. Same silhouette and outline as cube(); f as for cube().
// The baseboard / crown lines come from cube's own edge rows, so they stay visible over the pattern.
IWRAM_CODE static void wallBlock(int sx,int sy,int wp,int f){
    const u8*hhp=hhT[0]; int sl=9+wp;
    u16 T=sT[sl], eT=shade(T,9), eL=shade(sL[sl],9), eR=shade(sR[sl],9);
    for(int t=-CA;t<=CA;t++){
        int x=sx+t; if((unsigned)x>=SW) continue;
        int hh=hhp[t<0?-t:t], yt=sy+hh, yb=yt+CC-1;
        const u16*col=wpTab[wp][t<0?0:1][t<0?t+CA:(t&7)];
        u16 ec=t<0?eL:eR;
        for(int y=yt,v=0;y<=yb;y++,v++) if((unsigned)y<SH) fb[y*SW+x]=col[v];
        vline(x,sy-hh,sy+hh,T);
        if(!(f&1)){ px(x,yt+1,lite(col[1],19)); px(x,sy-hh,eT); }
        if(!(f&2)){ px(x,yb-1,shade(col[6],13)); px(x,yb,ec); }
        if((t==-CA&&!(f&16))||(t==CA&&!(f&32))) vline(x,sy-hh,yb,ec);
    }
}
// One floor tile: copy the pre-sampled columns. tex = flTab[floor][odd][0][0].
IWRAM_CODE static void floorTile(int sx,int sy,const u16*tex){   // one scanline span per row instead of one call per column
    for(int ry=-CB;ry<=CB;ry++,tex+=2*CA+1){
        int y=sy+ry; if((unsigned)y>=SH) continue;
        int hw=rowHW[ry<0?-ry:ry], x0=sx-hw, x1=sx+hw; const u16*sp=tex+(CA-hw);
        if(x0<0){ sp-=x0; x0=0; } if(x1>=SW) x1=SW-1;
        u16*d=&fb[y*SW+x0]; u16*e=&fb[y*SW+x1];
        while(d<=e) *d++=*sp++;
    }
}

IWRAM_CODE static void tileTop(int sx,int sy,u16 c){
    for(int ry=-CB;ry<=CB;ry++){
        int y=sy+ry; if((unsigned)y>=SH) continue;
        int hw=rowHW[ry<0?-ry:ry], x0=sx-hw, x1=sx+hw;
        if(x0<0) x0=0; if(x1>=SW) x1=SW-1;
        u16*d=&fb[y*SW+x0]; u16*e=&fb[y*SW+x1];
        while(d<=e) *d++=c;
    }
}


// ---------- face sprites ----------
// One cell = 9 x 8 px of art on a cube face. Wider sprites span cells: width = 10*cells-1 (the seam column is art too).
// Palette: k dark, w white, r red, s lid shadow (skin), . clear
#define B9  "........."
#define B19 "..................."
static const char* const aHalf[8] ={ B9, "..sssss..", ".kkkkkkk.", ".wwkkkww.", "..wkkkw..", "...www...", B9, B9 };
static const char* const aRound[8]={ B9, "..kkkkk..", ".kwwwwwk.", ".kwkkkwk.", ".kwkkkwk.", ".kwwwwwk.", "..kkkkk..", B9 };
static const char* const aHappy[8]={ B9, B9, "...kkk...", "..k...k..", ".k.....k.", B9, B9, B9 };
static const char* const mFlat[8] ={ B19, B19, B19, ".....kkkkkkkkk.....", B19, B19, B19, B19 };
static const char* const mSmile[8]={ B19, B19, "...k...........k...", "....k.........k....", ".....kkkkkkkkk.....", B19, B19, B19 };
static const char* const mOh[8]   ={ B19, ".......kkkkk.......", "......krrrrrk......", "......krrrrrk......", ".......kkkkk.......", B19, B19, B19 };
typedef struct { const char*name; u8 wc; const char* const*art; } Spr;   // wc = width in cells
static const Spr spr[6]={ {"HALF",1,aHalf},{"ROUND",1,aRound},{"HAPPY",1,aHappy},
                          {"FLAT",2,mFlat},{"SMILE",2,mSmile},{"OH",2,mOh} };
static int sty[2];                     // chosen style per kind: 0 = eye, 1 = mouth
#define SPRID(k) ((k)*3+sty[k])

// code (0 = none): bits 0-2 sprite+1, 3-5 cell column, 6-7 cell row (from top), 8-9 size-1, 10 mirrored
// face: 0 = left cube face (+Z seen from view 0), 1 = right cube face (+Z seen from view 3)
IWRAM_CODE static void drawDeco(int sx,int sy,u16 code,int face,int tint){
    const Spr*sp=&spr[(code&7)-1];
    int ci=(code>>3)&7, cj=(code>>6)&3, sz=((code>>8)&3)+1, fl=(code>>10)&1, aw=10*sp->wc-1;   // aw = art width in chars
    int wp=CA*sp->wc*sz-1, hp=CC*sz-2;   // footprint size in px (scales with the voxel size)
    const u16*pal=face?dR:dL;
    int lc0=ci?-1:0, lr0=cj?-1:0, axT[CA+1];
    for(int lc=lc0;lc<CA-1;lc++){ int ax=((ci*CA+lc)*aw)/wp; if(ax>aw-1) ax=aw-1; if(fl) ax=aw-1-ax; axT[lc+1]=ax; }
    for(int lr=lr0;lr<CC-2;lr++){
        int ay=((cj*CC+lr)*8)/hp; if(ay>7) ay=7;
        const char*row=sp->art[ay];
        for(int lc=lc0;lc<CA-1;lc++){
            char c=row[axT[lc+1]]; if(c=='.') continue;
            u16 col=tint?(face?sR[8]:sL[8]):pal[c=='k'?0:c=='w'?1:c=='r'?2:3];
            if(!face) px(sx-CA+1+lc, sy+((1+lc)>>1)+1+lr, col);
            else      px(sx+1+lc,    sy+((CA-1-lc)>>1)+1+lr, col);
        }
    }
}

static int cx,cy,cz,part,size;
#define OXC 60
#define OYC 121
static int view=0;   // 0..3 = 90 degree turns
static int noGrid=0;   // sprite baking draws the character without the floor grid
static void rotUW(int u,int w,int*ru,int*rw){
    switch(view){ case 0:*ru=u;*rw=w;break; case 1:*ru=-w;*rw=u;break; case 2:*ru=-u;*rw=-w;break; default:*ru=w;*rw=-u; }
}
// u,w = doubled grid coords relative to the build-space centre
static void projC(int u,int w,int yy,int*ox,int*oy){
    int a,b; rotUW(u,w,&a,&b); *ox=OXC+(a-b)*CA/2; *oy=OYC+(a+b)*CB/2-yy*CC;
}
// Wedge blocks (hair): the block's own diamond, with some top corners lowered by a full block height, so the top face slopes.
// shape 4..11: 4 edges (slope towards +x +z -x -z) and 4 corners (only the outer corner drops: ++ -+ -- +-). Slopes are given in grid space and
// turned with the view like everything else, so a wedge keeps pointing the same way as the creature spins.
static const u8 wMask[8]={10,12,5,3,8,4,1,2};   // bit k = grid corner (k&1 ? +x : -x, k&2 ? +z : -z)
IWRAM_CODE static void wedgeCube(int sx,int sy,int ci,int shape,int f){
    int m=wMask[shape-4], o[4]={0,0,0,0};   // lowered corners as seen on screen: N, E, S, W
    for(int k=0;k<4;k++) if(m>>k&1){
        int a,b; rotUW((k&1)?1:-1,(k&2)?1:-1,&a,&b); int dx=a-b, dy=a+b;
        o[dx>0?1:dx<0?3:dy>0?2:0]=CC;
    }
    const u8*hhp=hhT[0];
    u16 T=sT[ci], L=sL[ci], R=sR[ci], eT=shade(T,9), eL=shade(L,9), eR=shade(R,9);
    int left=o[3]+o[2], right=o[1]+o[0];
    u16 S2=shade(T,left>right?13:left<right?11:12);   // the slope: a little darker than a flat top, tilted towards the light on the left
    for(int t=-CA;t<=CA;t++){
        int x=sx+t, hh=hhp[t<0?-t:t], k=t<0?t+CA:t, u0,u1,l0,l1;
        if(t<0){ u0=o[3];u1=o[0];l0=o[3];l1=o[2]; } else { u0=o[0];u1=o[1];l0=o[2];l1=o[1]; }
        int yu=sy-hh+u0+(u1-u0)*k/CA, yl=sy+hh+l0+(l1-l0)*k/CA, yb=sy+hh+CC-1;
        if(yl>yb) yl=yb;
        u16 sc=t<0?L:R, ec=t<0?eL:eR;
        if(yl<yb) vline(x,yl,yb,sc);
        if(yl>=yu) vline(x,yu,yl,S2);
        px(x,yu,eT);
        if(yl<yb-1){ px(x,yl+1,lite(sc,19)); } else px(x,yl,ec);
        if(yb>yl+2) px(x,yb-1,shade(sc,13));
        px(x,yb,ec);
        if(t==-CA||t==CA){ if(!(f&(t<0?16:32))) vline(x,yu,yb,ec); }
    }
}
static void moveView(int sx,int sz){   // screen-relative step -> grid step
    int gx,gz;
    switch(view){ case 0:gx=sx;gz=sz;break; case 1:gx=sz;gz=-sx;break; case 2:gx=-sx;gz=-sz;break; default:gx=-sz;gz=sx; }
    cx+=gx; cz+=gz;
}

static u8 vox[H][D][W], ghost[H][D][W];
static u16 dec[H][D][W], gdec[H][D][W];   // face sprites per voxel (+Z face) and their cursor preview
static int gAny;


// ---------- parts ----------
typedef struct { const char*name; u8 n,mirror,w,h,d; const u8 (*c)[4]; u8 dk; } Part;   // dk: 0 voxel part, 1 eye sprite, 2 mouth sprite
static const u8 cHead[][4]={{0,0,0,1},{1,0,0,1},{0,1,0,1},{1,1,0,1},{0,0,1,1},{1,0,1,1},{0,1,1,1},{1,1,1,1}};
static const u8 cTorso[][4]={{0,0,0,6},{1,0,0,6},{0,1,0,6},{1,1,0,6},{0,0,1,6},{1,0,1,6},{0,1,1,6},{1,1,1,6}};
static const u8 cArm[][4]={{0,0,0,1|(2<<4)},{0,1,0,1|(1<<4)},{0,2,0,6|(1<<4)}};   // hand, forearm, sleeve (high nibble = shape)
static const u8 cLeg[][4]={{0,0,0,7|(3<<4)},{0,1,0,7|(3<<4)},{0,2,0,7|(3<<4)}};
static const u8 cEar[][4]={{0,0,0,1},{0,1,0,1}};
static const u8 cHair[][4]={{0,0,0,5},{1,0,0,5},{0,0,1,5},{1,0,1,5},{0,1,0,5},{1,1,1,5}};
#define NPARTS 8
static const Part parts[NPARTS]={
 {"HEAD",8,0,2,2,2,cHead,0},{"TORSO",8,0,2,2,2,cTorso,0},{"ARM",3,1,1,3,1,cArm,0},{"LEG",3,1,1,3,1,cLeg,0},
 {"EYE",0,1,1,1,1,0,1},{"MOUTH",0,0,2,1,1,0,2},{"EAR",2,1,1,2,1,cEar,0},{"HAIR",6,0,2,2,2,cHair,0}};

// ---------- state ----------


static void apply(int x0,int y0,int z0,int flip,int act,int pi,int s){
    const Part*p=&parts[pi];
    for(int i=0;i<p->n;i++)
      for(int a=0;a<s;a++)for(int b=0;b<s;b++)for(int c=0;c<s;c++){
        int X=p->c[i][0]*s+a; if(flip) X=p->w*s-1-X;
        int x=x0+X, y=y0+p->c[i][1]*s+b, z=z0+p->c[i][2]*s+c;
        if(x<BX0||x>=BX0+stBW[stage]||y<0||y>=stBH[stage]||z<0||z>=stBD[stage]) continue;   // outside this life stage's build box
        if(act==0) ghost[y][z][x]=1; else if(act==1) vox[y][z][x]=p->c[i][3]; else { vox[y][z][x]=0; dec[y][z][x]=0; }
      }
}
// Sprites snap to the front-most solid voxel in the cursor's column, so Z does not matter.
static int snapZ(int x,int y){ for(int z=D-1;z>=0;z--) if(vox[y][z][x]) return z; return -1; }
static void applyDeco(int x0,int y0,int act,int pi,int s,int flip){
    int id=SPRID(parts[pi].dk-1), fw=spr[id].wc*s;   // footprint: fw cells wide, s cells tall
    for(int j=0;j<s;j++)for(int i=0;i<fw;i++){
        int x=x0+i, y=y0+s-1-j;
        if(x<BX0||x>=BX0+stBW[stage]||y<0||y>=stBH[stage]) continue;
        if(act==2){ for(int z=0;z<D;z++) dec[y][z][x]=0; continue; }
        int z=snapZ(x,y); if(z<0) continue;
        u16 code=(u16)((id+1)|(i<<3)|(j<<6)|((s-1)<<8)|(flip<<10));
        if(act==0){ gdec[y][z][x]=code; gAny=1; } else dec[y][z][x]=code;
    }
}
static void doDeco(int act,int pi,int s,int x,int y){
    int fw=spr[SPRID(parts[pi].dk-1)].wc*s;
    applyDeco(x,y,act,pi,s,0);
    if(parts[pi].mirror){ int mx=W-x-fw; if(mx!=x) applyDeco(mx,y,act,pi,s,1); }
}
static void doPart(int act,int pi,int s,int x,int y,int z){
    if(parts[pi].dk){ doDeco(act,pi,s,x,y); return; }
    apply(x,y,z,0,act,pi,s);
    if(parts[pi].mirror){ int mx=W-x-parts[pi].w*s; if(mx!=x) apply(mx,y,z,1,act,pi,s); }
}
static void clampCursor(void){
    if(part>=NPARTS) return;   // "GO LIVE LIFE!" entry has no cursor
    const Part*p=&parts[part];
    if(size>stMaxSz[stage]) size=stMaxSz[stage];
    int mx=BX0+stBW[stage]-p->w*size, my=stBH[stage]-p->h*size, mz=stBD[stage]-p->d*size;
    if(mx<BX0)mx=BX0; if(my<0)my=0; if(mz<0)mz=0;
    if(cx>mx)cx=mx; if(cy>my)cy=my; if(cz>mz)cz=mz;
    if(cx<BX0)cx=BX0; if(cy<0)cy=0; if(cz<0)cz=0;
}
// ---------- the look -> blocks ----------
// The creature creator never asks for blocks: it asks for a look (shape, ears, hair style...) and this turns it into the 6x4x8 model.
// Skin, hair, top and bottom colours are only palette slots (setColors), eye and mouth styles only re-skin the face sprites (restyle),
// so those never touch the blocks. Shape, ears and hair style rebuild the whole model (the pickers ask first if you built by hand).
static int custom;   // 1 once the block builder has placed or erased something by hand
static void bodyPlan(int*L,int*T,int*hs){   // legs showing, torso blocks showing, head scale: the body that fits this stage's box
    int sh=look[LK_SHAPE]; *hs=(sh==2)?2:1;
    int l=stLegs[stage], t=2, ht=2*(*hs);
    if(sh>=2&&l>0) l--;                                   // BIG HEAD and STUBBY: legs one block shorter
    while(l+t+ht>stBH[stage]&&l>0) l--;                   // too tall for the box: shorten the legs, then the torso
    if(l+t+ht>stBH[stage]) t=1;
    *L=l; *T=t;
}
static void headBox(int*hx,int*hy,int*hz,int*hs){   // where the head sits (and how many blocks per head cell) for each body shape
    int L,T; bodyPlan(&L,&T,hs);
    *hx=BX0+(stBW[stage]-2*(*hs))/2; *hy=L+T; *hz=(stBD[stage]-2*(*hs))/2;
}
static void hairW(int x,int y,int z,int xp,int xm,int zp,int zm){   // one hair block; the flags say which sides slope away (grid space)
    if(x<BX0||x>=BX0+stBW[stage]||y<0||y>=stBH[stage]||z<0||z>=stBD[stage]) return;
    int nx=xp+xm, nz=zp+zm, m=0;
    if(nx+nz==1) m=xp?10:zp?12:xm?5:3;
    else if(nx==1&&nz==1) m=1<<((xp?1:0)+(zp?2:0));
    for(int i=0;i<8;i++) if(wMask[i]==m&&m){ vox[y][z][x]=(u8)(5|((4+i)<<4)); return; }
    vox[y][z][x]=5;
}
static void buildLook(void){
    for(int y=0;y<H;y++)for(int z=0;z<D;z++)for(int x=0;x<W;x++){ vox[y][z][x]=0; dec[y][z][x]=0; }
    sty[0]=look[LK_EYES]; sty[1]=look[LK_MOUTH];
    int L,T,hs0; bodyPlan(&L,&T,&hs0);
    int tx=BX0+(stBW[stage]-2)/2, ty=L+T-2;                      // torso: left column and bottom row (the bottom row is clipped when T is 1)
    if(look[LK_SHAPE]==1&&stage==AG_ADULT){                      // BROAD: torso and legs two blocks wider each side
        doPart(1,3,1,1,0,1); doPart(1,3,1,2,0,1);
        doPart(1,1,1,1,3,1); doPart(1,1,1,3,3,1); doPart(1,2,1,0,2,1);
    } else {                                                     // AVERAGE, BIG HEAD, STUBBY: legs (clipped to the stage), torso, arms
        doPart(1,3,1,tx,L-3,1); doPart(1,1,1,tx,ty,1); doPart(1,2,1,tx-1,ty-1,1);
    }
    int hx,hy,hz,hs; headBox(&hx,&hy,&hz,&hs);
    int hw=2*hs, hh=2*hs, hd=2*hs;
    doPart(1,0,hs,hx,hy,hz);                                     // head
    int es=look[LK_EARS];                                        // ears: 0 none, 1 small (1x2), 2 big (2x4)
    if(es) doPart(1,6,es,hx-es,hy+(es==1?(hh-2)/2:0),hz+(es==1?hd/2:0));
    int st=look[LK_HSTYLE], top=(stBH[stage]-(hy+hh)>=1)?hy+hh:hy+hh-1;    // hair: a cap on the head, or in place of its top layer when the head touches the ceiling
    if(st!=3){
        // every style starts with the same dome: the outer edges of the cap are wedges that slope away, down to the head's top
        for(int z=hz;z<hz+hd;z++)for(int x=hx;x<hx+hw;x++) hairW(x,top,z,x==hx+hw-1,x==hx,z==hz+hd-1,z==hz);
        // BOWL: full blocks down both sides (the dome slopes down onto them, so the profile stays smooth)
        if(st==1) for(int z=hz;z<hz+hd;z++)for(int y=top-1;y>=top-2&&y>=0;y--){ hairW(hx-1,y,z,0,0,0,0); hairW(hx+hw,y,z,0,0,0,0); }
        // LONG: full blocks down the back, from the dome to below the neck
        if(st==2){ int z0=hz>0?hz-1:hz; for(int x=hx;x<hx+hw;x++)for(int y=hy-1;y<top;y++) hairW(x,y,z0,0,0,0,0); }
    }
    if(hs==1){ doPart(1,4,1,hx,hy+1,0); doPart(1,5,1,hx,hy,0); }          // eyes on the top row of the face, mouth on the bottom row
    else     { doPart(1,4,2,hx,hy+1,0); doPart(1,5,1,hx+1,hy,0); }        // big head: big eyes, mouth still one block
    custom=0;
}
static void restyle(int kind){   // change the style of every eye (0) or mouth (1) sprite already on the creature, built by hand or not
    for(int y=0;y<H;y++)for(int z=0;z<D;z++)for(int x=0;x<W;x++){
        u16 c=dec[y][z][x]; if(!c) continue;
        if(((c&7)-1)/3==kind) dec[y][z][x]=(u16)((c&~7)|(kind*3+sty[kind]+1)); }
}
// ---- changing the stage ----
static int maskPick(int mask,int v,int n){ for(int i=0;i<n;i++){ int j=(v+i)%n; if(mask>>j&1) return j; } return 0; }   // the option at or after v that is allowed
static void fixLook(void){   // pull every choice into what this stage offers
    look[LK_SHAPE]=(u8)maskPick(stMaskShape[stage],look[LK_SHAPE],4);
    look[LK_EARS]=(u8)maskPick(stMaskEars[stage],look[LK_EARS],3);
    look[LK_HSTYLE]=(u8)maskPick(stMaskHair[stage],look[LK_HSTYLE],4);
    static const u8 sw[4]={LK_SKIN,LK_HCOL,LK_TOP,LK_BOT};
    for(int i=0;i<4;i++) if(look[sw[i]]>=stSwatches[stage]) look[sw[i]]=(u8)(look[sw[i]]%stSwatches[stage]);
}
static void clipCustom(void){   // hand-built blocks outside the stage's box are cut off
    for(int y=0;y<H;y++)for(int z=0;z<D;z++)for(int x=0;x<W;x++)
        if(x<BX0||x>=BX0+stBW[stage]||y>=stBH[stage]||z>=stBD[stage]){ vox[y][z][x]=0; dec[y][z][x]=0; }
}
static void ageSave(void);
static void setStage(int n){   // new stage: the look is fitted to it; a look-built creature is rebuilt, hand-built blocks stay (cut to size)
    stage=(u8)n; ageDays=0; fixLook(); if(size>stMaxSz[stage]) size=stMaxSz[stage];
    if(custom) clipCustom(); else buildLook();
    setColors(); ageSave();
}
#define AGE_OFF 12416   // SRAM: 'A' 'G', stage, days in the stage, checksum (the creature itself is only kept in room slots, so its growth is remembered here)
static void ageSave(void){ volatile u8*m=(volatile u8*)0x0E000000+AGE_OFF; m[0]='A'; m[1]='G'; m[2]=stage; m[3]=ageDays; m[4]=(u8)(0x47+stage+ageDays); }
static void ageLoad(void){   // at power on, after the person came back from its slot: the grown-up stage wins over the stage the slot was saved at
    volatile u8*m=(volatile u8*)0x0E000000+AGE_OFF;
    if(m[0]!='A'||m[1]!='G'||m[2]>=AG_N||m[4]!=(u8)(0x47+m[2]+m[3])) return;
    ageDays=m[3];
    if(m[2]!=stage){ stage=m[2]; fixLook(); if(custom) clipCustom(); else buildLook(); setColors(); }
}
static int gGrow;   // set at midnight when the creature has lived long enough in its stage: the life loop grows it (setStage) and re-bakes its sprites
static inline int ojob(void){ return xo[XO_JOB]&&stage>=AG_TEEN&&stage<AG_ELDER; }   // career: shifts, quota and bills for teens and adults (an elder is retired)
static void starter(void){
    cx=2;cy=0;cz=1;part=0;size=1;
    buildLook();   // the starter creature is look 0 everywhere: legs, torso, arms, head, eyes, mouth and hair
}

// ---------- scene ----------
static int solid(int x,int y,int z){ return x>=0&&x<W&&y>=0&&y<H&&z>=0&&z<D&&vox[y][z][x]; }
static const signed char dA[4][2]={{1,0},{0,-1},{-1,0},{0,1}}, dB[4][2]={{0,1},{1,0},{0,-1},{-1,0}};   // screen +a / +b in grid x,z per view
// The screen is two independent regions: the 3D scene (columns 0..PANEL_X-1) and the side panel.
// Both are cleared with 32-bit stores, and the panel is only redrawn when something it shows changed.
#define SCENE_W (PANEL_X/2)
#define ROW_W   (SW/2)
IWRAM_CODE static void fillCols(int w0,int w1,u16 c){
    u32 v=c|((u32)c<<16), *row=(u32*)fb;
    for(int y=0;y<SH;y++,row+=ROW_W) for(int w=w0;w<w1;w++) row[w]=v;
}
static u8 ord[4][W*D];   // per view: cells (x | z<<4) sorted back to front, so the draw loop needs no search
static void initTables(void){
    for(int sh=0;sh<4;sh++){ int r=rTab[sh]; for(int at=0;at<=r;at++) hhT[sh][at]=(u8)((r/2)*(r-at)/r); }
    for(int a=0;a<=CB;a++){ int w=0; for(int at=0;at<=CA;at++) if(hhT[0][at]>=a) w=at; rowHW[a]=(u8)w; }
    bakeTex();
    int sv=view;
    for(int v=0;v<4;v++){
        view=v; int key[W*D], n=0;
        for(int z=0;z<D;z++)for(int x=0;x<W;x++){
            int ru,rw; rotUW(2*x+1-W,2*z+1-D,&ru,&rw);
            int k=((ru+rw+8)<<8)|(z<<4)|x, j=n++;
            while(j>0&&key[j-1]>k){ key[j]=key[j-1]; j--; }
            key[j]=k;
        }
        for(int i=0;i<n;i++) ord[v][i]=(u8)(key[i]&0xFF);
    }
    view=sv;
}
// The creature creator's stage: a little house room built from the game's own wallpaper and floors, so the creature stands in the
// same kind of place it will live in. Floor tile (tx,ty) lines up with build cell x=tx, z=ty-1, so the creature stands on it exactly.
static int stageOn;   // 1: draw the stage under the creature; 0: the plain sky and build grid (block builder, game sprite baking)
#define ST_N 6                       // floor tiles per side
#define ST_WH 6                      // wall height in blocks
#define ST_WP 2                      // PEACH STRIPE
#define ST_Y0 (OYC-ST_N*CB)          // screen y of the floor's back corner
static void stageWall(int tx,int ty,int j32){   // one wall cell (tx or ty is -1); j32/16 = coplanar neighbour flags
    int sx=OXC+(tx-ty)*CA, sy=ST_Y0+(tx+ty+1)*CB;
    for(int j=1;j<=ST_WH;j++){
        int f=(j<ST_WH?1:0)|(j>1?2:0)|j32;
        if(sWp) wallBlock(sx,sy-j*CC,ST_WP,f); else cube(sx,sy-j*CC,9+ST_WP,0,f);
    }
}
static void drawStage(void){
    for(int y=0;y<SH;y++){ u16 c=RGB(3+y/45,4+y/34,10+y/16); u32 v=c|((u32)c<<16), *row=(u32*)fb+y*ROW_W; for(int w=0;w<SCENE_W;w++) row[w]=v; }
    for(int ty=0;ty<ST_N;ty++)for(int tx=0;tx<ST_N;tx++){
        int sx=OXC+(tx-ty)*CA, sy=ST_Y0+(tx+ty+1)*CB, v=(tx^ty)&1;
        int fl=(tx>=1&&tx<=ST_N-2&&ty>=1&&ty<=ST_N-2)?2:4;   // TEAL CARPET rug on WOOD PLANKS
        if(sFl) floorTile(sx,sy,&flTab[fl][v][0][0]); else tileTop(sx,sy,flFlat[fl][v]);
    }
    for(int k=-1;k<ST_N;k++){   // back to front: the corner first, then the two walls moving toward the viewer
        stageWall(-1,k,k>=0?32:0);
        if(k>=0) stageWall(k,-1,16);
    }
}
IWRAM_CODE static void drawScene(int blink){
    if(stageOn&&!noGrid) drawStage(); else fillCols(0,SCENE_W,SKY);
    // floor grid
    u16 gc=RGB(13,18,22); int a,b,c,d;
    if(!noGrid&&!stageOn) for(int i=BX0;i<=BX0+stBW[stage];i++){ projC(2*i-W,-D,0,&a,&b); projC(2*i-W,2*stBD[stage]-D,0,&c,&d); line(a,b,c,d,gc); }   // the grid shows only this stage's box
    if(!noGrid&&!stageOn) for(int j=0;j<=stBD[stage];j++){ projC(2*BX0-W,2*j-D,0,&a,&b); projC(2*(BX0+stBW[stage])-W,2*j-D,0,&c,&d); line(a,b,c,d,gc); }
    int fv=view==0?0:view==3?1:-1;   // which cube face shows the +Z (front) face, -1 = turned away
    // voxels (back to front)
    for(int y=0;y<H;y++)for(int i=0;i<W*D;i++){
        int x=ord[view][i]&15, z=ord[view][i]>>4;
        int raw=vox[y][z][x], ci=raw&15, shape=raw>>4;
        if(ghost[y][z][x]&&blink) ci=8;
        if(gdec[y][z][x]&&blink&&fv<0) ci=8;   // face turned away: flag the target voxel instead
        if(!ci) continue;
        int u=2*x+1-W, w=2*z+1-D;
        int sx,sy; projC(u,w,y+1,&sx,&sy);   // top-face centre
        if(shape==1||shape==2){ int sg=u<0?1:-1, a2,b2; rotUW(sg,0,&a2,&b2); sx+=HUG*(a2-b2); sy+=(HUG*(a2+b2))/2; }   // hug the torso
        int f=(solid(x,y+1,z)?1:0)|(solid(x,y-1,z)?2:0)
             |(solid(x-dA[view][0],y,z-dA[view][1])?16:0)|(solid(x-dB[view][0],y,z-dB[view][1])?32:0);
        if(shape>=4) wedgeCube(sx,sy,ci,shape,f); else cube(sx,sy,ci,shape,f);
        u16 dc=dec[y][z][x]; int tint=0;
        if(gdec[y][z][x]&&blink){ dc=gdec[y][z][x]; tint=1; }
        if(dc&&fv>=0) drawDeco(sx,sy,dc,fv,tint);
    }
}
static void vsync(void){ while(REG_VCOUNT>=160); while(REG_VCOUNT<160); }
static void present(void){
    vsync();
    REG_DMA3SAD=(u32)(uintptr_t)fb; REG_DMA3DAD=VRAM_ADDR;
    REG_DMA3CNT=(SW*SH/2)|0x84000000u;
}

// Copy only the scene columns (blink-only redraws leave the panel untouched).
static void presentScene(void){
    vsync();
    for(int y=0;y<SH;y++){
        REG_DMA3SAD=(u32)(uintptr_t)(fb+y*SW); REG_DMA3DAD=VRAM_ADDR+(u32)(y*SW*2); REG_DMA3CNT=SCENE_W|0x84000000u;
    }
}
// Row-by-row DMA of a rectangle (32-bit columns w0..w1-1, rows y0..y1-1): src buffer -> dst base address.
static void dmaRows(const u16*src,u32 dst,int w0,int w1,int y0,int y1){
    for(int y=y0;y<y1;y++){
        int o=y*SW+w0*2;
        REG_DMA3SAD=(u32)(uintptr_t)(src+o); REG_DMA3DAD=dst+(u32)(o*2); REG_DMA3CNT=(u32)(w1-w0)|0x84000000u;
    }
}

// ---------- title screen ----------
#include "titleimg.h"
static void buildTitle(void){
    for(int y=0;y<80;y++)for(int x=0;x<120;x++){
        char c=titleArt[y][x]; u16 col=titlePal[c<='9'?c-'0':c-'a'+10];
        u16*o=&fb[(y*2)*SW+x*2]; o[0]=o[1]=o[SW]=o[SW+1]=col;
    }
    for(int y=118;y<SH;y++)for(int x=0;x<SW;x++){ u16 c=fb[y*SW+x]; fb[y*SW+x]=shade(c,7); }   // dim strip for the prompt
    u16 ink=RGB(4,3,6), gold=RGB(31,27,6), grn=RGB(12,28,8);
    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++) text(10+dx,10+dy,"BORE",ink,5);   // outline
    text(10,10,"BORE",gold,5);
    text(12,40,"A VOXEL LIFE SIM",ink,1); text(11,39,"A VOXEL LIFE SIM",grn,1);
    text(14,126,"PUFF PUFF PASS THE CONTROLLER",RGB(16,22,12),1);
    for(int i=0;i<SW*SH;i++) tfb[i]=fb[i];
}
static void smoke(int frame){
    static const signed char wob[16]={0,1,2,3,3,3,2,1,0,-1,-2,-3,-3,-3,-2,-1};
    for(int i=0;i<9;i++){
        int t=(frame+i*14)&127;
        int x=164+wob[(t/4+i*5)&15]+t/5, y=82-t*7/8, r=1+t/24;
        for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){
            if(dx*dx+dy*dy>r*r+1) continue;
            if(t>64&&((x+dx+y+dy)&1)) continue;       // fade out by dithering
            if(t>100&&((x+dx)&1)) continue;
            px(x+dx,y+dy,RGB(27,28,29));
        }
    }
}
#define SM_W0 76   // smoke stays inside columns 152..203, rows 0..89 (checked over its whole 128-frame loop)
#define SM_W1 102
#define SM_Y1 90
#define TX_W0 47    // "PRESS START" box
#define TX_W1 70
#define TX_Y0 141
#define TX_Y1 147
// ---------- LIFE MODE: fixed isometric "sim" room + Tony-Hawk-style skating (placeholder) ----------
// Pick "GO LIVE LIFE!" in the part list and press A. SELECT+START returns to the editor.
// Controls: D-pad L/R steer (grounded) or spin (airborne) | hold A push | D-pad down brake | B ollie / kickflip in air
// Land spins in half-turns (180/360) for points, a bad angle is a bail. Land on a yellow rail to grind it.
typedef int32_t s32;
static void lifeMode(int ed);
static void mapEditor(void);
#define MW 40
#define MH 40    // keep MH == MW: the 4-way action cam rotates the square map
static int camX, camY, camSnap=1, camLastV;   // camera offset in px; the map's top corner is drawn at (120-camX, 24-camY)
#define LOX (120-camX)   // screen x of the map's top corner
#define LOY (24-camY)
#define SPW 32   // baked at half size so the skater is ~2 tiles tall in the room
#define SPH 44
#define SPX0 (OXC-32)
#define SPY0 (OYC-80)   // capture window top; feet sit at row 40 of the half-size sprite
#define MAPNAME "THE MAN BASE"   // name of the (placeholder) map
// w = low wall, W = wall, # = 2-block crate, = = grind rail, . = floor (the default map is built by mapGen below)
static const short cosT[16]={256,237,181,98,0,-98,-181,-237,-256,-237,-181,-98,0,98,181,237};   // sin(a)=cosT[(a+12)&15]
static u16 spr4[4][SPW*SPH] EWRAM_BSS;
static s32 lfx,lfy,lz,lvz,lvx,lvy; static int lskate, lhave, lfr;   // lskate: 0 on foot, 1 skateboard; lhave: picked up the board
static u8 floorMap[MH][MW] EWRAM_BSS, wallMap[MH][MW] EWRAM_BSS;   // floor style and wallpaper per tile
static int lfpsV;   // measured frames per second (shown when SHOW FPS is on)
static char lifeMap[MH][MW+1] EWRAM_BSS;   // the room being played / edited (starts as mapDef, or the copy saved in SRAM)
static int bdx=10, bdy=4, spx=3, spy=6;   // skateboard tile and spawn tile, found by mapScan (B and P tiles)
#define BDX bdx
#define BDY bdy
static int lsp,lhd,lspin,lflip,lgrind,lscore,lstun,lairF,lpts,lnoteT; static const char*lnote;

static int lfood, lbl, lnear;   // hunger (100 = full), bladder (100 = bursting), what is in reach (1 fridge, 2 toilet)
static int lmaxz, lplay, ldead, lbumpCd;   // peak height this jump, air sound played, dead, bump cooldown
#include "mood.h"   // FUN + HAPPY meters: moodEvent(), moodTick(), moodTop(), moodPts()
#include "sims.h"   // life-sim layer: energy/hygiene/comfort, wants and fears, aspiration. simsTick(), simBegin(), simsHud()

// ---------- sound effects: 4-bit IMA-ADPCM @ 6554 Hz, decoded on the fly into RAM, played by Direct Sound A (DMA1 + Timer0) ----------
// source/sfx/*.adp (made by tools/encode_sfx.py) are baked into the ROM with .incbin; paths are relative to the project root.
// Timer1 counts Timer0 overflows = samples played, so a clip stops exactly at its end whatever the frame rate is.
#define R_SNDCNT_L (*(volatile u16*)0x04000080)
#define R_SNDCNT_H (*(volatile u16*)0x04000082)
#define R_SNDCNT_X (*(volatile u16*)0x04000084)
#define R_DMA1SAD (*(volatile u32*)0x040000BC)
#define R_DMA1DAD (*(volatile u32*)0x040000C0)
#define R_DMA1CNT (*(volatile u32*)0x040000C4)
#define R_TM0D    (*(volatile u16*)0x04000100)
#define R_TM0CNT  (*(volatile u16*)0x04000102)
#define R_TM1D    (*(volatile u16*)0x04000104)
#define R_TM1CNT  (*(volatile u16*)0x04000106)
#define SFX_TIMER (65536-2560)   // 16777216/2560 = 6553.6 Hz
__asm__(".pushsection .rodata\n.balign 4\n"
 ".global sfx_hit\nsfx_hit:\n.incbin \"source/sfx/hit.adp\"\n.balign 4\n"
 ".global sfx_gasp\nsfx_gasp:\n.incbin \"source/sfx/gasp.adp\"\n.balign 4\n"
 ".global sfx_scream\nsfx_scream:\n.incbin \"source/sfx/scream.adp\"\n.balign 4\n"
 ".global sfx_cry\nsfx_cry:\n.incbin \"source/sfx/cry.adp\"\n.balign 4\n"
 ".global sfx_groan\nsfx_groan:\n.incbin \"source/sfx/groan.adp\"\n.balign 4\n"
 ".global sfx_instant\nsfx_instant:\n.incbin \"source/sfx/instant.adp\"\n.balign 4\n"
 ".popsection\n");
extern const u8 sfx_hit[],sfx_gasp[],sfx_scream[],sfx_cry[],sfx_groan[],sfx_instant[];
enum { SFX_BONK, SFX_HIT, SFX_GASP, SFX_SCREAM, SFX_CRY, SFX_GROAN, SFX_NEARLY, SFX_DEATH, SFX_INSTANT, SFX_N };
// effects that share a source file share one blob in the ROM
static const u8* const sfxTab[SFX_N]={ sfx_hit,sfx_hit,sfx_gasp,sfx_scream,sfx_cry,sfx_groan,sfx_scream,sfx_scream,sfx_instant };
static const u16 stepT[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const signed char idxT[8]={-1,-1,-1,-1,2,4,6,8};
static const u8 *ssrc; static u32 sn, sdone, swraps; static int spred, sidx, sfxOn; static u16 slast;
// GAME MUSIC (jukebox songs while playing) shares Timer0 and the sound FIFOs with the sound effects, so an effect ducks the music: the music
// stops feeding the DMA (mDucked) while the effect plays, and sfxStop() hands the speakers back when the effect is over.
static volatile int mDucked; static int gMusic;   // mDucked: music paused for an effect; gMusic: game music is switched on right now
static void musResume(void);
static void sfxStop(void){ R_DMA1CNT=0; R_TM0CNT=0; R_TM1CNT=0; sfxOn=0; if(mDucked){ mDucked=0; if(gMusic) musResume(); } }
#define SM_W0 76   // smoke stays inside columns 152..203, rows 0..89 (checked over its whole 128-frame loop)
#define SM_W1 102
#define SM_Y1 90
#define TX_W0 47    // "PRESS START" box
#define TX_W1 70
#define TX_Y0 141
#define TX_Y1 147
// ---------- tracker songs: note-based XM player (tools/xm2gba.py converts the .xm songs listed in songs.h) ----------
// A song is stored as notes (pattern/row/channel events, each with its own volume) plus small instrument samples (8-bit,
// band-limited and down-sampled in the converter). A 16-voice stereo mixer (each note has a pan bus, see tools/xm2gba.py) with linear interpolation renders 304 samples per frame
// into Direct Sound A (left, DMA1) and B (right, DMA2), both on Timer0, restarted every vblank. The title song plays its intro once, then orders loop.. repeat
// (loop = per-song loop order); the voices are never cut at the loop jump, so the last notes ring into the first ones.
typedef int8_t s8; typedef int16_t s16;
typedef struct {            // one converted tracker song (generated into musicdata.h)
    const u8*order; const u16*rows; const u32*patOff; const u32*ev;   // order list, rows per pattern, pattern start in ev, note events
    const u32*step; const u32*len; const s8*const*data;               // per instrument: 96 note steps, sample length, sample data
    const u8*busL; const u8*busR;                                     // 7 pan buses: left / right gain (128 = 1.0), balanced per song by the converter
    int nord, loop, rowN, rfr;                                        // orders in the song, loop order, samples per row (+ fraction/256)
} XmSong;
#include "musicdata.h"
#define R_DMA2SAD (*(volatile u32*)0x040000C8)
#define R_DMA2DAD (*(volatile u32*)0x040000CC)
#define R_DMA2CNT (*(volatile u32*)0x040000D0)
#define MUS_N 304   // samples per frame at 18157 Hz (924 cycles each = exactly one frame)
#define MUS_VOICES 16   // tracker channels the mixer can play at once
typedef struct { const s8*d; u32 pos,step,len; int vl,vr; } MVoice;   // vl / vr = note volume x left / right pan-bus gain
static MVoice mvc[MUS_VOICES];
// STEREO: Direct Sound A plays the left buffers, Direct Sound B the right ones; both are fed by Timer0 and restarted together at vblank.
static s8 mbufL[2][MUS_N] __attribute__((aligned(4))), mbufR[2][MUS_N] __attribute__((aligned(4)));
static s16 maccL[MUS_N], maccR[MUS_N];
static s8 mDly[256]; static int mDp, mLp;   // pseudo-stereo for streamed songs: 256-sample (14 ms) delay line + a low-pass state that keeps the bass centred
static int mOrd, mRow, mLeft, mFrac; static volatile int mCur, mOn, mFilled; static const XmSong*mSong;
static volatile int mGain=256, mGainT=256;   // music loudness 256 = full; mGain glides to mGainT a little every frame (half while a menu is open)
static int mKind, aTail; static volatile int mLaps, mDone;   // mKind 0 = tracker song, 1 = streamed ADPCM; mLaps = times the tracker song has wrapped; mDone = ADPCM song finished
static int aSlow, aPrv, aPh; static const u8 *aSrc; static u32 aN, aPos; static int aPred, aIdx;   // ADPCM stream: data, sample count, position, decoder state
static void musTrigger(void){
    const XmSong*s=mSong; const u32*e=&s->ev[s->patOff[s->order[mOrd]]];
    for(int r=0;r<mRow;r++) e+=1+*e;
    int n=*e++;
    while(n--){ u32 w=*e++; int ch=w&15, in=(w>>4)&31, nt=(w>>9)&127, vol=(w>>16)&127;   // channel, instrument, note, voice volume
        int bus=(w>>23)&7; if(bus>6) bus=3;   // pan bus 0 = hard left .. 3 = centre .. 6 = hard right
        MVoice*v=&mvc[ch]; v->d=s->data[in]; v->pos=0; v->step=s->step[in*96+nt]; v->len=s->len[in]<<16; v->vl=vol*s->busL[bus]; v->vr=vol*s->busR[bus]; }
}
IWRAM_CODE static void musMix(s8*outL,s8*outR){
    int done=0;
    while(done<MUS_N){
        if(mLeft==0){ musTrigger(); mFrac+=mSong->rfr; mLeft=mSong->rowN+(mFrac>>8); mFrac&=255;
            if(++mRow>=mSong->rows[mSong->order[mOrd]]){ mRow=0; if(++mOrd>=mSong->nord){ mOrd=mSong->loop; mLaps++; } } }
        int n=MUS_N-done; if(n>mLeft) n=mLeft;
        s16*a=maccL+done; s16*b=maccR+done;
        for(int i=0;i<n;i++){ a[i]=0; b[i]=0; }
        for(int vi=0;vi<MUS_VOICES;vi++){ MVoice*v=&mvc[vi]; if(!v->d) continue;
            u32 pos=v->pos, st=v->step, len=v->len; const s8*d=v->d; int vl=v->vl, vr=v->vr, i=0;
            for(;i<n;i++){
                if(pos>=len){ v->d=0; break; }
                int ix=(int)(pos>>16), fr=(int)((pos>>8)&255), x0=d[ix], x1=d[ix+1];
                int x=x0*256+(x1-x0)*fr;                      // one interpolated sample, 16-bit scale
                a[i]=(s16)(a[i]+((x*vl)>>21)); b[i]=(s16)(b[i]+((x*vr)>>21)); pos+=st; }   // (>>21 = the old >>14 with the 1/128 bus gain folded in)
            v->pos=pos; }
        mLeft-=n; done+=n;
    }
    for(int i=0;i<MUS_N;i++){ int x=maccL[i]>>2, y=maccR[i]>>2;
        outL[i]=(s8)(x>127?127:x<-128?-128:x); outR[i]=(s8)(y>127?127:y<-128?-128:y); }
}
// Streamed ADPCM song (source/music/*.adp from tools/encode_song.py): 4-bit IMA-ADPCM, 18157 Hz, so one frame = 304 samples.
// Same format as the sound effects: u32 sample count, then nibbles (low first). Decoded straight into the DMA buffer.
IWRAM_CODE static void adpMix(s8*out,s8*outR){
    // bit 31 of the sample count = song stored at 2/3 rate (12105 Hz): every 2 stored samples become 3 output samples (linear interpolation)
    u32 p=aPos, e=aN; int pred=aPred, idx=aIdx, i=0; const u8*d=aSrc;
    int prv=aPrv, ph=aPh;
    for(;i<MUS_N;i++){
        if(aSlow){
            ph+=2;
            while(ph>=3&&p<e){ ph-=3; prv=pred;
                int v=d[p>>1]; v=(p&1)?(v>>4):(v&15); p++;
                int step=stepT[idx], diff=step>>3;
                if(v&1) diff+=step>>2; if(v&2) diff+=step>>1; if(v&4) diff+=step;
                pred+=(v&8)?-diff:diff; if(pred>32767) pred=32767; if(pred<-32768) pred=-32768;
                idx+=idxT[v&7]; if(idx<0) idx=0; if(idx>88) idx=88; }
            if(p>=e&&ph>=3) break;
            out[i]=(s8)((prv+(((pred-prv)*ph)/3))>>8);
        } else {
            if(p>=e) break;
            int v=d[p>>1]; v=(p&1)?(v>>4):(v&15); p++;
            int step=stepT[idx], diff=step>>3;
            if(v&1) diff+=step>>2; if(v&2) diff+=step>>1; if(v&4) diff+=step;
            pred+=(v&8)?-diff:diff; if(pred>32767) pred=32767; if(pred<-32768) pred=-32768;
            idx+=idxT[v&7]; if(idx<0) idx=0; if(idx>88) idx=88;
            out[i]=(s8)(pred>>8);
        }
    }
    for(;i<MUS_N;i++) out[i]=0;
    aPos=p; aPred=pred; aIdx=idx; aPrv=prv; aPh=ph;
    // Pseudo-stereo (complementary comb): L = 0.75x + 0.5z, R = 0.75x - 0.5z, where z is the high part of x delayed by 14 ms. L+R is exactly the
    // original mono signal (so it also sounds right on the GBA's mono speaker); the ears get different comb patterns = width.
    // The one-pole low-pass is subtracted from z so bass and kick stay in the middle.
    int dp=mDp, lp=mLp;
    for(int k=0;k<MUS_N;k++){ int x=out[k], z=mDly[dp]; mDly[dp]=(s8)x; dp=(dp+1)&255;
        lp+=(z*16-lp)>>3; int h=z-(lp>>4);                 // lp holds the low-passed delayed signal x16
        int l=(x*12+h*8)>>4, r=(x*12-h*8)>>4;
        out[k]=(s8)(l>127?127:l<-128?-128:l); outR[k]=(s8)(r>127?127:r<-128?-128:r); }
    mDp=dp; mLp=lp;
    if(p>=e&&i<MUS_N&&++aTail>=3) mDone=1;   // 2 buffers are in flight, so wait for the last real samples to be heard
}
IWRAM_CODE static void musMixAny(int b){
    if(mKind) adpMix(mbufL[b],mbufR[b]); else musMix(mbufL[b],mbufR[b]);
    int sh=oMusShift();   // MUSIC VOLUME option: full, half, quarter, off
    if(sh>=8){ for(int i=0;i<MUS_N;i++){ mbufL[b][i]=0; mbufR[b][i]=0; } }
    else if(sh){ for(int i=0;i<MUS_N;i++){ mbufL[b][i]=(s8)(mbufL[b][i]>>sh); mbufR[b][i]=(s8)(mbufR[b][i]>>sh); } }
    if(mGain!=mGainT){ int g=mGain+((mGainT>mGain)?16:-16); if((mGainT>mGain)?g>mGainT:g<mGainT) g=mGainT; mGain=g; }   // fade: 16 steps of 1/16 per frame
    if(mGain<256){ int g=mGain; for(int i=0;i<MUS_N;i++){ mbufL[b][i]=(s8)((mbufL[b][i]*g)>>8); mbufR[b][i]=(s8)((mbufR[b][i]*g)>>8); } }
}
// ---- Audio is driven by interrupts, NOT by the main loop ----
// Old design: the main loop mixed one buffer per frame right after vsync. Any frame whose drawing ran long (jukebox list
// redraw, equalizer...) missed the next vblank, so the DMA ran dry (crackle) and the song fell a frame behind (timing lag).
// Now: VBlank IRQ (line 160) only restarts the sound DMA on the buffer that is already filled (a few dozen cycles);
// VCount IRQ (line 0) mixes the next buffer. The song clock is therefore locked to the hardware, whatever the main loop does.
#define R_IE  (*(volatile u16*)0x04000200)
#define R_IF  (*(volatile u16*)0x04000202)
#define R_IME (*(volatile u16*)0x04000208)
#define R_DISPSTAT (*(volatile u16*)0x04000004)
#define R_IRQVEC (*(volatile u32*)0x03007FFC)
u32 irqStack[256] __attribute__((aligned(8)));   // private IRQ stack (the BIOS one is only 160 bytes)
extern void irqEntry(void);
__asm__(".pushsection .iwram,\"ax\",%progbits\n.arm\n.align 2\n.global irqEntry\nirqEntry:\n"
        "  push {r4-r11,lr}\n  mov r4,sp\n  ldr r0,=irqStack+1024\n  mov sp,r0\n  bl irqMain\n  mov sp,r4\n  pop {r4-r11,lr}\n  bx lr\n"
        ".ltorg\n.popsection\n");
__attribute__((used)) IWRAM_CODE void irqMain(void){
    u16 f=R_IF;
    if(f&1){   // vblank: start the buffer that was filled last frame, in step with the screen
        R_IF=1;
        if(mOn){
            if(!mFilled) mCur^=1;                      // (mix overran: replay the last buffer rather than a half-filled one)
            R_DMA1CNT=0; R_DMA2CNT=0; R_TM0CNT=0;
            R_DMA1SAD=(u32)(uintptr_t)mbufL[mCur]; R_DMA1DAD=0x040000A0u;   // left  -> Direct Sound A
            R_DMA2SAD=(u32)(uintptr_t)mbufR[mCur]; R_DMA2DAD=0x040000A4u;   // right -> Direct Sound B
            R_DMA1CNT=0xB6400000u; R_DMA2CNT=0xB6400000u;                   // enable, FIFO timing, repeat, 32-bit, fixed dest
            R_TM0D=(u16)(65536-924); R_TM0CNT=0x80;
            mCur^=1; mFilled=0;                        // mCur is now the idle buffer
        }
    }
    if(f&4){   // line 0: render the idle buffer, it plays at the next vblank
        R_IF=4;
        if(mOn&&!mFilled){ musMixAny(mCur); mFilled=1; }
    }
}
static void irqOff(void){ R_IME=0; R_IE=0; R_DISPSTAT=0; R_IF=0xFFFF; }
// Start a song: kind 0 = the tracker song xm, kind 1 = the ADPCM data in adp.
static void musBegin(int kind,const u8*adp,const XmSong*xm){
    irqOff(); mOn=0; R_DMA1CNT=0; R_DMA2CNT=0; R_TM0CNT=0;
    sfxStop();
    for(int i=0;i<MUS_VOICES;i++) mvc[i].d=0;
    mOrd=0; mRow=0; mLeft=0; mFrac=0; mCur=0; mFilled=0; mLaps=0; mDone=0; aTail=0; mKind=kind; mSong=xm; mDp=0; mLp=0; for(int i=0;i<256;i++) mDly[i]=0;
    if(kind){ aSrc=adp+4; aN=*(const u32*)adp; aSlow=(int)(aN>>31); aN&=0x7FFFFFFFu; aPrv=0; aPh=0; aPos=0; aPred=0; aIdx=0; }
    musMixAny(0);   // buffer 0 is primed here and plays at the first vblank; the line-0 IRQ then renders buffer 1
    mFilled=1; mOn=1;
    R_SNDCNT_X=0x80; R_SNDCNT_L=0; R_SNDCNT_H=0x9A0C;   // stereo: Direct Sound A -> left only, B -> right only, both 100%, Timer0, FIFOs reset
    R_IRQVEC=(u32)(uintptr_t)irqEntry;
    R_DISPSTAT=0x0028;                // vblank IRQ (bit 3) + vcount IRQ (bit 5) at line 0
    R_IF=0xFFFF; R_IE=5; R_IME=1;
}
static void musStart(void){ musBegin(0,0,&xm_the_dipper_man); }   // the title music
static void musKick(void){}   // (kept so old call sites still compile: the interrupts do this now)
static void musFill(void){}
static void musStop(void){ irqOff(); if(!mOn) return; mOn=0; R_DMA1CNT=0; R_DMA2CNT=0; R_TM0CNT=0; R_SNDCNT_H=0; }
static void musResume(void){ R_SNDCNT_X=0x80; R_SNDCNT_L=0; R_SNDCNT_H=0x9A0C; mOn=1; }   // after an effect: the next vblank restarts the music DMA
// ---------- jukebox song table: built from source/songs.h (edit that file, not this) ----------
// Pass 1 bakes every .adp into the ROM, pass 2 declares the data, pass 3 builds the table.
#define SONG_XM(id,n,f)
#define SONG_ADP(id,n,f) ".global jbs_" #id "\njbs_" #id ":\n.incbin \"" f "\"\n.balign 4\n"
__asm__(".pushsection .rodata\n.balign 4\n"
#include "songs.h"
".popsection\n");
#undef SONG_XM
#undef SONG_ADP
#define SONG_XM(id,n,f)
#define SONG_ADP(id,n,f) extern const u8 jbs_##id[];
#include "songs.h"
#undef SONG_XM
#undef SONG_ADP
typedef struct { const char*name; const u8*adp; const XmSong*xm; } Song;   // adp = 0 means a tracker song (xm)
#define SONG_XM(id,n,f) {n,0,&xm_##id},
#define SONG_ADP(id,n,f) {n,jbs_##id,0},
static const Song songs[]={
#include "songs.h"
};
#undef SONG_XM
#undef SONG_ADP
#define NSONGS ((int)(sizeof(songs)/sizeof(songs[0])))
_Static_assert(sizeof(songs)/sizeof(songs[0])<=32,"the jukebox holds at most 32 songs (see source/songs.h)");
// ---------- debug code: UP UP DOWN DOWN LEFT LEFT RIGHT B A START on the title screen ----------
// Reveals the PLACEHOLDER test tunes in the jukebox for this session (they are hidden otherwise).
static u8 dbgOn;
static void settingsSave(void);
static const u16 konSeq[11]={K_UP,K_UP,K_DOWN,K_DOWN,K_LEFT,K_RIGHT,K_LEFT,K_RIGHT,K_B,K_A,K_START};   // UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A START
static u8 konMsg;   // 1 = the code just locked the classic creator, 2 = unlocked (main shows a toast once the title is gone)
static const u16 dbgSeq[10]={K_UP,K_UP,K_DOWN,K_DOWN,K_LEFT,K_LEFT,K_RIGHT,K_B,K_A,K_START};
static int titleScreen(void){
    buildTitle();                          // leaves the finished backdrop in both fb and tfb
    vsync(); dmaRows(fb,VRAM_ADDR,0,ROW_W,0,SH);
    int shown=0, frame, dbgI=0, konI=0; u16 dbgPrev=(u16)(~REG_KEYINPUT)&0x3FF; if(xo[XO_TITLEMUS]) musStart();
    for(frame=0;;frame++){
        u16 dk=(u16)(~REG_KEYINPUT)&0x3FF, dp=dk&(u16)~dbgPrev; dbgPrev=dk;
        if(dp){   // a fresh button press: right next key of the code, or start over
            if(dp==dbgSeq[dbgI]){ if(++dbgI==10){ dbgOn=1; dbgI=0; } }
            else dbgI=(dp==dbgSeq[0])?1:0;
            if(dp==konSeq[konI]){ if(++konI==11){ konI=0; sUnlock^=1; if(!sUnlock) sClassic=0; settingsSave(); konMsg=1+sUnlock; } }
            else konI=(dp==konSeq[0])?1:0;
        }
        if(dk&K_START) break;
        dmaRows(tfb,(u32)(uintptr_t)fb,SM_W0,SM_W1,0,SM_Y1);   // wipe last frame's smoke only
        smoke(frame);
        int on=(frame>>4)&1, tx=(on!=shown);
        if(tx){ dmaRows(tfb,(u32)(uintptr_t)fb,TX_W0,TX_W1,TX_Y0,TX_Y1); if(on) text(94,141,"PRESS START",RGB(31,31,31),1); shown=on; }
        vsync(); musKick();
        dmaRows(fb,VRAM_ADDR,SM_W0,SM_W1,0,SM_Y1);
        if(tx) dmaRows(fb,VRAM_ADDR,TX_W0,TX_W1,TX_Y0,TX_Y1);
        musFill();
    }
    musStop();
    while((~REG_KEYINPUT)&K_START) vsync();   // wait for release so START doesn't also change size
    return frame;   // how long the player sat on the title: stirs the random seed
}

IWRAM_CODE static void sfxDecode(int cnt){   // decode the next cnt samples into sfxRam (signed 8-bit)
    u32 i=sdone, e=sdone+(u32)cnt; if(e>sn) e=sn;
    int pred=spred, idx=sidx; signed char*out=(signed char*)sfxRam;
    for(;i<e;i++){
        int v=ssrc[i>>1]; v=(i&1)?(v>>4):(v&15);
        int step=stepT[idx], diff=step>>3;
        if(v&1) diff+=step>>2; if(v&2) diff+=step>>1; if(v&4) diff+=step;
        pred+=(v&8)?-diff:diff; if(pred>32767) pred=32767; if(pred<-32768) pred=-32768;
        idx+=idxT[v&7]; if(idx<0) idx=0; if(idx>88) idx=88;
        out[i]=(signed char)(pred>>(8+oSfxShift()));   // SFX VOLUME option
    }
    spred=pred; sidx=idx; sdone=e;
    if(sdone>=sn) for(int k=0;k<256;k++) out[sn+k]=0;   // silence after the end, so the DMA read-ahead plays nothing
}
static void sfxPlay(int id){   // a new sound replaces whatever is playing
    sfxStop(); if(!sSnd) return;
    if(gMusic&&mOn){ mOn=0; mDucked=1; R_DMA2CNT=0; }   // game music: pause it for the effect
    const u8*b=sfxTab[id]; sn=*(const u32*)b; if(sn>SFX_MAX-256) sn=SFX_MAX-256;
    ssrc=b+4; sdone=0; spred=0; sidx=0;
    sfxDecode(1024);                             // a head start; sfxTick decodes the rest while it plays
    R_SNDCNT_X=0x80; R_SNDCNT_L=0;
    R_SNDCNT_H=0x0B04;                           // Direct Sound A: 100% vol, L+R, Timer0, reset FIFO
    R_DMA1SAD=(u32)(uintptr_t)sfxRam; R_DMA1DAD=0x040000A0u;
    R_DMA1CNT=0xB6400000u;                       // enable, FIFO timing, repeat, 32-bit, fixed dest
    R_TM0D=SFX_TIMER; R_TM0CNT=0x80;
    swraps=0; slast=0; R_TM1D=0; R_TM1CNT=0x84;  // Timer1 counts Timer0 overflows (samples played)
    sfxOn=1;
}
static void sfxTick(void){   // call once per frame
    if(!sfxOn) return;
    u16 t=R_TM1D; if(t<slast) swraps++; slast=t;
    if(sdone<sn) sfxDecode(512);
    if(swraps*65536u+t>=sn) sfxStop();
}
static u32 lrng=12345;
static int rnd8(void){ lrng=lrng*1664525u+1013904223u; return (int)(lrng>>24); }

// Getting hurt. sev grows with fall height, speed and a bad landing. kind: 0 clean landing, 1 bail, 2 wall hit.
static void die(int snd){ moodEvent(M_DIE); ldead=1; lstun=2; lsp=0; lgrind=0; sfxPlay(snd); lnote="YOU DIED"; lnoteT=0x7fff; }
static void hurt(int sev,int kind){
    if(xo[XO_HURT]==1) sev/=2; else if(xo[XO_HURT]==2&&sev>=30) sev=29;   // HURT option: GENTLE halves it, NO DEATH keeps a fall survivable
    if(sev>=30) moodEvent(M_HURT_BIG); else if(sev>=18) moodEvent(M_HURT); else if(kind==2) moodEvent(M_BUMP);   // (40+ is death: die() logs it)
    if(sev>=40) die(SFX_INSTANT);                                                                   // instant death
    else if(sev>=30){                                                                                // life or death
        if(rnd8()<128){ lstun=240; lsp=0; lgrind=0; sfxPlay(SFX_NEARLY); lnote="CLOSE CALL"; lnoteT=120; }
        else die(SFX_DEATH);
    }
    else if(sev>=18){ lstun=150; lsp=0; lgrind=0; sfxPlay(SFX_GROAN); lnote="OW"; lnoteT=90; }     // groaning, struggling up
    else if(kind==1){ lstun=60; sfxPlay(SFX_CRY); }                                                  // minor bail: crying
    else if(kind==2){ lstun=20; sfxPlay(SFX_HIT); lnote="OOF"; lnoteT=30; }                          // grunts and hits
}

#include "ramps.h"
static int tileH(int tx,int ty){   // surface height in px (ramps: their highest point). Grind height is 6: rails, ledges and benches
    if(tx<0||ty<0||tx>=MW||ty>=MH) return 99;
    char c=lifeMap[ty][tx];
    return (c=='#'||c=='F'||c=='W'||c=='H')?2*CC: (c=='X'||c=='Y')?10: (c=='w'||c=='T'||c=='S'||c=='C'||c=='O')?CC: (c=='='||c=='L'||c=='N'||c=='Z'||c=='K'||c=='J')?6: (c=='M')?3: isKicker(c)?KICKER_H: isLaunch(c)?LAUNCH_H: isQPipe(c)?qpH[7]: 0;   // pack 2: X funbox 10, Y trash can 10, O barrel 8, Z planter / K table / J jersey grind at 6, M manual pad 3
}
static int surfH(s32 fx,s32 fy){   // surface height at an exact position (1/256 tiles): same as tileH, but ramps slope
    int tx=(int)(fx>>8), ty=(int)(fy>>8); if(tx<0||ty<0||tx>=MW||ty>=MH) return 99;
    char c=lifeMap[ty][tx]; return isRamp(c)?rampH(c,(int)fx,(int)fy):tileH(tx,ty);
}
static void bakeSprites(void){   // render the built character once per view (4 turns), then just blit it
    int sv=view; noGrid=1;
    for(int y=0;y<H;y++)for(int z=0;z<D;z++)for(int x=0;x<W;x++){ ghost[y][z][x]=0; gdec[y][z][x]=0; }
    for(int v=0;v<4;v++){
        view=v; drawScene(0);
        for(int y=0;y<SPH;y++)for(int x=0;x<SPW;x++) spr4[v][y*SPW+x]=fb[(SPY0+y*2)*SW+SPX0+x*2];
    }
    noGrid=0; view=sv;
}
IWRAM_CODE static void blit(const u16*s,int x0,int y0){
    for(int y=0;y<SPH;y++){ int yy=y0+y; if((unsigned)yy>=SH) continue;
        for(int x=0;x<SPW;x++){ u16 c=s[y*SPW+x]; if(c!=SKY) px(x0+x,yy,c); } }
}
static int numText(int x,int y,int n,u16 c){
    char b[10]; int i=9; b[i]=0; if(n<=0) b[--i]='0';
    while(n>0&&i>0){ int q=n/10; b[--i]=(char)('0'+n-q*10); n=q; }
    return text(x,y,b+i,c,1);
}
// ---------- map data: reset / scan / save ----------
// lifeMap = what stands on each tile, floorMap = floor style under it, wallMap = wallpaper on it (for wall tiles).
enum { T_ROOM, T_WALL, T_FLOOR, T_ITEM, T_ERASE, NTOOL };
static int eTool, eAct, eAx, eAy, eFl, eWp, eOb;   // editor: tool, rectangle anchor set?, anchor tile, chosen floor / wallpaper / item
#define NOBJ 25
#define OB_LAUNCH 17   // launch ramp turns like the kicker: '9'..'<'
#define OB_KICKER 10   // palette slots whose char carries a turn (+eRot): kicker '1'..'4', quarter pipe '5'..'8'
#define OB_QPIPE 11
static int eRot;   // editor: which way the next ramp faces (0 S, 1 E, 2 N, 3 W)
static const char palCh[NOBJ]={'.','w','W','#','=','F','T','D','B','P','1','5','L','N','S','H','C','9','X','O','Y','Z','K','J','M'};
static const char* const palNm[NOBJ]={"CLEAR","LOW WALL","WALL","CRATE","RAIL","FRIDGE","TOILET","DOOR","BOARD","SPAWN","KICKER","Q PIPE","LEDGE","BENCH","BED","SHOWER","SOFA","LAUNCH","FUNBOX","BARREL","TRASH CAN","PLANTER","PICNIC","JERSEY","MANUAL PAD"};
static const u16 palCol[NOBJ]={RGB(26,21,14),RGB(8,20,22),RGB(10,22,24),RGB(8,9,20),RGB(31,30,16),RGB(31,31,31),RGB(30,28,18),RGB(14,9,5),RGB(26,10,6),RGB(28,10,8),RGB(24,17,9),RGB(27,19,11),RGB(20,20,22),RGB(25,18,9),RGB(10,14,28),RGB(22,28,30),RGB(26,18,9),RGB(8,14,24),RGB(18,16,24),RGB(24,6,5),RGB(12,18,14),RGB(20,10,6),RGB(25,18,9),RGB(22,22,24),RGB(30,26,5)};
static signed char palLut[256]; static u8 palLutOk;   // tile char -> palette slot (or -1), built on first use: palIdx() runs for every tile of the minimap, so it must be O(1) even with 100+ items
static int palIdx(char c){
    if(!palLutOk){ for(int i=0;i<256;i++) palLut[i]=-1; for(int i=NOBJ-1;i>=0;i--) palLut[(u8)palCh[i]]=(signed char)i;
        for(int r=0;r<4;r++){ palLut[(u8)('1'+r)]=OB_KICKER; palLut[(u8)('5'+r)]=OB_QPIPE; palLut[(u8)('9'+r)]=OB_LAUNCH; } palLutOk=1; }
    return palLut[(u8)c];
}
static char edObjCh(void){ char c=palCh[eOb]; return (eOb==OB_KICKER||eOb==OB_QPIPE||eOb==OB_LAUNCH)?(char)(c+eRot):c; }   // the char the ITEM tool places
// ---- default big map: house (top left), factory (top right), rail park (bottom), roads of concrete between ----
static void gBox(int x0,int y0,int x1,int y1,int fl){ for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) floorMap[y][x]=(u8)fl; }
static void gRoom(int x0,int y0,int x1,int y1,int fl,int wp){   // walled room with a floor
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){ floorMap[y][x]=(u8)fl;
        if(x==x0||x==x1||y==y0||y==y1){ lifeMap[y][x]='W'; wallMap[y][x]=(u8)wp; } } }
static void gLine(int x0,int y0,int x1,int y1,char c,int wp){ for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){ lifeMap[y][x]=c; wallMap[y][x]=(u8)wp; } }
static void gPut(int x,int y,char c){ lifeMap[y][x]=c; }
static void gFree(int x,int y,char c){ if(x>=0&&y>=0&&x<MW&&y<MH&&lifeMap[y][x]=='.') lifeMap[y][x]=c; }   // put only onto empty floor
static void mapGen(void){
    for(int y=0;y<MH;y++){ for(int x=0;x<MW;x++){ lifeMap[y][x]='.'; floorMap[y][x]=7; wallMap[y][x]=0; } lifeMap[y][MW]=0; }
    gLine(0,0,MW-1,0,'w',13); gLine(0,MH-1,MW-1,MH-1,'w',13); gLine(0,0,0,MH-1,'w',13); gLine(MW-1,0,MW-1,MH-1,'w',13);   // low wall round the edge
    // HOUSE: peach wallpaper, beige carpet, lino kitchen, pink-tile bathroom
    gRoom(2,2,17,17,1,2); gBox(11,11,16,16,3);
    gRoom(2,2,9,9,5,11); gPut(6,9,'D'); gPut(3,3,'T');
    gPut(9,17,'D'); gPut(17,13,'D');
    gPut(16,11,'F'); gPut(16,12,'F'); gPut(12,4,'#'); gPut(13,4,'#'); gPut(12,5,'#'); gPut(13,5,'#');
    gPut(5,12,'P'); gPut(7,14,'B');
    gPut(8,3,'H'); gPut(4,16,'S'); gPut(3,11,'C');                     // shower (bathroom), bed and sofa (lounge)
    // FACTORY: red brick, steel plate, oil-stained and hazard lanes, grate corner, crates and a rail
    gRoom(22,2,37,19,8,8); gBox(23,10,36,11,10); gBox(23,14,27,18,9); gBox(30,3,36,8,12);
    gPut(29,19,'D'); gPut(22,10,'D'); gPut(37,10,'D');
    gLine(24,13,29,13,'=',8);
    gPut(25,4,'#'); gPut(26,4,'#'); gPut(25,5,'#'); gPut(26,5,'#'); gPut(31,15,'#'); gPut(32,15,'#'); gPut(31,16,'#'); gPut(32,16,'#'); gPut(34,5,'#'); gPut(34,6,'#');
    // RAIL PARK: oil-stained skate lanes, long rails, crate boxes. The middle (x 13-26, y 22-35) is a plaza (an old 14x14 saved room lands here)
    gBox(2,22,37,37,7); gBox(2,28,37,29,12);
    gLine(3,24,10,24,'=',0); gLine(3,31,10,31,'=',0); gLine(3,35,10,35,'=',0);
    gLine(29,24,36,24,'=',0); gLine(29,31,36,31,'=',0); gLine(29,35,36,35,'=',0);
    gLine(16,28,23,28,'=',0); gLine(16,33,23,33,'=',0);
    gPut(5,26,'#'); gPut(6,26,'#'); gPut(5,27,'#'); gPut(6,27,'#'); gPut(8,33,'#'); gPut(9,33,'#'); gPut(8,34,'#'); gPut(9,34,'#');
    gPut(31,26,'#'); gPut(32,26,'#'); gPut(31,27,'#'); gPut(32,27,'#'); gPut(34,33,'#'); gPut(35,33,'#'); gPut(34,34,'#'); gPut(35,34,'#');
    gPut(14,24,'#'); gPut(15,24,'#'); gPut(14,25,'#'); gPut(15,25,'#'); gPut(24,25,'#'); gPut(25,25,'#'); gPut(24,26,'#'); gPut(25,26,'#');
    gLine(12,21,27,21,'w',13);
    gPut(16,30,'4'); gPut(21,30,'2');                                  // two kickers facing each other: a gap jump
    gPut(17,22,'5'); gPut(18,22,'5');                                  // quarter pipes (face south) in front of the plaza wall
    gPut(13,33,'L'); gPut(14,33,'L'); gPut(15,33,'L'); gPut(25,33,'N'); gPut(26,33,'N');   // ledge and bench to grind
    // SKATE PACK 2 (only onto empty floor, so nothing above is overwritten): funbox with two launch ramps, barrels, jersey barriers, planters, picnic table, trash cans, manual pad
    gFree(18,30,'X'); gFree(19,30,'X'); gFree(18,31,'9'); gFree(19,31,'9');
    gFree(13,31,'O'); gFree(13,32,'O'); gFree(14,31,'O'); gFree(26,30,'O'); gFree(26,31,'O');
    for(int x=16;x<=21;x++) gFree(x,35,'J');
    gFree(13,22,'Z'); gFree(14,22,'Z'); gFree(25,22,'Z'); gFree(26,22,'Z');
    gFree(21,24,'K'); gFree(13,23,'Y'); gFree(26,23,'Y');
    for(int x=17;x<=20;x++) gFree(x,26,'M');
}
static void mapReset(void){ mapGen(); }
static void mapScan(void){   // find the skateboard (B) and the spawn point (P); fall back to sane defaults
    int fx=-1, fy=-1; bdx=bdy=spx=spy=-1;
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){ char c=lifeMap[y][x];
        if(c=='B'){ bdx=x; bdy=y; } if(c=='P'){ spx=x; spy=y; }
        if(fx<0&&c=='.'){ fx=x; fy=y; } }
    if(spx<0){ if(fx<0){ lifeMap[1][1]='P'; fx=fy=1; } spx=fx; spy=fy; }
}
#define SRAM_BASE ((volatile u8*)0x0E000000)
static const char sramTag[] __attribute__((used)) = "SRAM_V113";   // tells emulators / flash carts to give the game battery saves
#define MSZ (MW*MH)
#define SET_OFF 8192            // settings live here now (the big map takes bytes 0..4802)
#define OMW 14                  // old 14x14 saves
#define OMSZ (OMW*OMW)
#define LEG_X 13                // an old save is copied into the plaza at (13,22)
#define LEG_Y 22
#include "jukebox.h"   // playlist logic: shuffled order lives in SRAM at JB_OFF (12288), the mode is a setting
// Songs named PLACEHOLDER... are hidden from the jukebox unless the title-screen debug code was entered (dbgOn).
static int isDbgSong(int i){ const char*n=songs[i].name, *p="PLACEHOLDER"; while(*p){ if(*n++!=*p++) return 0; } return 1; }
static void jbSetup(void){   // build the list of songs the jukebox shows, then load / make the playlist order
    int n=0; for(int i=0;i<NSONGS&&n<JB_MAX;i++) if(songs[i].xm!=&xm_the_dipper_man&&(dbgOn||!isDbgSong(i))) jbMap[n++]=(u8)i;   // THE DIPPER MAN is the title music only: never listed
    jbInit(n);
}
// SRAM layout: 0..2 "BM3", then MSZ bytes each of tiles, floors, wallpapers. Settings at SET_OFF (see settingsSave).
// Old "BM1" / "BM2" saves (14x14, settings at 640) still load: the room is placed into the plaza of the new default map.
static void mapSave(void){ volatile u8*m=SRAM_BASE; m[0]='B'; m[1]='M'; m[2]='3';
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){ int i=y*MW+x; m[3+i]=(u8)lifeMap[y][x]; m[3+MSZ+i]=floorMap[y][x]; m[3+2*MSZ+i]=wallMap[y][x]; } }
static int mapSaved(void){ volatile u8*m=SRAM_BASE; if(m[0]!='B'||m[1]!='M'||m[2]!='3') return 0;
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){ int i=y*MW+x;
        if(m[3+i]!=(u8)lifeMap[y][x]||m[3+MSZ+i]!=floorMap[y][x]||m[3+2*MSZ+i]!=wallMap[y][x]) return 0; }
    return 1; }
static int mapLoad(void){   // returns 1 if a valid saved map was loaded
    volatile u8*m=SRAM_BASE;
    if(m[0]!='B'||m[1]!='M') return 0;
    if(m[2]=='3'){
        for(int i=0;i<MSZ;i++){ if(palIdx((char)m[3+i])<0||m[3+MSZ+i]>=NFL||m[3+2*MSZ+i]>=NWP) return 0; }
        for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){ int i=y*MW+x;
            lifeMap[y][x]=(char)m[3+i]; floorMap[y][x]=m[3+MSZ+i]; wallMap[y][x]=m[3+2*MSZ+i]; }
        return 1; }
    if(m[2]!='1'&&m[2]!='2') return 0;
    int v2=(m[2]=='2');
    for(int i=0;i<OMSZ;i++){ if(palIdx((char)m[3+i])<0) return 0; if(v2&&(m[3+OMSZ+i]>=NFL||m[3+2*OMSZ+i]>=NWP)) return 0; }
    mapReset();
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++) if(lifeMap[y][x]=='B'||lifeMap[y][x]=='P') lifeMap[y][x]='.';   // the old room brings its own
    for(int y=0;y<OMW;y++)for(int x=0;x<OMW;x++){ int i=y*OMW+x, X=LEG_X+x, Y=LEG_Y+y;
        lifeMap[Y][X]=(char)m[3+i]; floorMap[Y][X]=v2?m[3+OMSZ+i]:0; wallMap[Y][X]=v2?m[3+2*OMSZ+i]:0; }
    return 1; }
static void mapPlace(int x,int y,char c){
    if(c=='B'||c=='P'){ for(int j=0;j<MH;j++)for(int i=0;i<MW;i++) if(lifeMap[j][i]==c) lifeMap[j][i]='.'; }
    lifeMap[y][x]=c; if(c=='w'||c=='W') wallMap[y][x]=(u8)eWp; }
// extended options (opts.h): one byte each at OPT_OFF, 'X' 'O', count, values, checksum. A save with fewer options (older game) leaves the new ones at their defaults.
#define OPT_OFF 8448
static void optsSave(void){
    volatile u8*m=SRAM_BASE+OPT_OFF; unsigned sum=0x3C;
    for(int i=0;i<XO_N;i++){ m[3+i]=xo[i]; sum+=xo[i]; }
    m[2]=XO_N; sum+=XO_N; m[3+XO_N]=(u8)sum; m[0]='X'; m[1]='O';
}
static void optsLoad(void){
    volatile u8*m=SRAM_BASE+OPT_OFF; optsDefaults();
    if(m[0]!='X'||m[1]!='O'||m[2]==0||m[2]>XO_N) return;
    int n=m[2]; unsigned sum=0x3C+n; for(int i=0;i<n;i++) sum+=m[3+i];
    if(m[3+n]!=(u8)sum) return;                                       // damaged: keep the defaults
    for(int i=0;i<n;i++) if(m[3+i]<xoCnt[i]) xo[i]=m[3+i];            // every value range checked
}
// settings (SRAM offset 8192)
static void settingsSave(void){ optsSave();
   volatile u8*m=SRAM_BASE+SET_OFF; m[0]='S'; m[1]='2'; m[2]=sFps; m[3]=sWall; m[4]=sWp; m[5]=sFl; m[6]=sSnd; m[7]=sShow; m[8]=sShad; m[9]=sHud; m[10]=sRom; m[11]=sCam; m[12]=sJb; m[13]=sNoWarn; m[14]=sClassic; m[15]=sUnlock; }
static void settingsLoad(void){ volatile u8*m=SRAM_BASE+SET_OFF;
    if(m[0]!='S'){ volatile u8*o=SRAM_BASE; if(o[0]=='B'&&o[1]=='M'&&o[2]!='3') m=SRAM_BASE+640; else return; }   // old saves kept settings at 640
    if(m[0]!='S') return;
    if(m[1]=='1'){ if(m[2]>2||m[3]>2||m[4]>1||m[5]>1||m[6]>1||m[7]>1) return;   // older save: fewer settings
        sFps=m[2]; sWall=m[3]; sWp=m[4]; sFl=m[5]; sSnd=m[6]; sShow=m[7]; return; }
    if(m[1]!='2'||m[2]>3||m[3]>2||m[4]>1||m[5]>1||m[6]>1||m[7]>2||m[8]>1||m[9]>2||m[10]>1) return;
    sCam=(m[11]<=3)?m[11]:1; sJb=(m[12]<=2)?m[12]:0; sNoWarn=(m[13]==1)?1:0; sClassic=(m[14]==1)?1:0; sUnlock=(m[15]==1)?1:0; if(!sUnlock) sClassic=0;
    sFps=m[2]; sWall=m[3]; sWp=m[4]; sFl=m[5]; sSnd=m[6]; sShow=m[7]; sShad=m[8]; sHud=m[9]; sRom=m[10]; }

// ---------- small UI kit: one menu style, one help style, one toast ----------
#define DIMC RGB(18,20,22)
#define WHITE RGB(31,31,31)
static u16 keyNow(void){   // BUTTONS option: A/B and L/R can be swapped here, so every screen sees the swapped keys
    u16 k=(u16)(~REG_KEYINPUT)&0x3FF; int b=xo[XO_BTN];
    if(b&1){ u16 a=k&K_A, c=k&K_B; k=(u16)((k&~(K_A|K_B))|(a?K_B:0)|(c?K_A:0)); }
    if(b&2){ u16 l=k&K_L, r=k&K_R; k=(u16)((k&~(K_L|K_R))|(l?K_R:0)|(r?K_L:0)); }
    return k;
}
static void box(int x,int y,int w,int h){ rect(x-1,y-1,w+2,h+2,GOLD); rect(x,y,w,h,RGB(3,4,7)); }
static int menu(const char*title,const char*const*it,int n){   // UP/DOWN + A to choose, B or START to cancel (returns -1)
    int sel=0, w=116, h=26+n*10, x=(SW-w)/2, y=(SH-h)/2; u16 prev=keyNow();
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k;
        if(pr&K_DOWN) sel=(sel+1)%n;
        if(pr&K_UP) sel=(sel+n-1)%n;
        if(pr&K_A) return sel;
        if(pr&(K_B|K_START)) return -1;
        box(x,y,w,h); text(x+6,y+5,title,GOLD,1);
        for(int i=0;i<n;i++){ int yy=y+16+i*10;
            if(i==sel){ rect(x+3,yy-2,w-6,9,RGB(6,16,8)); text(x+6,yy,">",WHITE,1); }
            text(x+13,yy,it[i],i==sel?WHITE:DIMC,1); }
        text(x+6,y+h-9,"A OK  B BACK",RGB(12,14,16),1);
        present();
    }
}
static void helpScreen(const char*title,const char*const*ln,int n){   // lines starting with > are headings
    u16 prev=keyNow();
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k;
        if(pr&(K_A|K_B|K_START)) return;
        box(3,1,234,157); text(14,10,title,GOLD,1);
        for(int i=0;i<n;i++){ const char*l=ln[i]; if(l[0]=='>') text(14,22+i*8,l+1,GOLD,1); else text(18,22+i*8,l,WHITE,1); }
        text(14,144,"PRESS A TO CLOSE",DIMC,1);
        present();
    }
}
static void toast(const char*msg){ int w=tw(msg,1)+16;
    box((SW-w)/2,66,w,22); text((SW-w)/2+8,74,msg,WHITE,1); for(int i=0,n=oToastLen();i<n;i++){ present(); } }
static const char* const lifeHelp[16]={">ON FOOT","DPAD WALK  B RUN  A HOP","L GET ON THE BOARD","R USE FRIDGE TOILET BED SHOWER SOFA",">ON THE BOARD","A PUSH  DPAD STEER  B OLLIE","IN AIR DPAD SPINS  B KICKFLIP",">KEEP YOURSELF GOING","FOOD WC REST CLEAN COMFY ROOM BARS","SLEEP IN BED  A OR B GETS YOU UP",">WORK  MON TO FRI 9 TO 5","TRICK POINTS BEAT THE QUOTA FOR PAY","3 GOOD DAYS PROMOTE  3 BAD DEMOTE",">WANTS AND FEARS","MEET WANTS TO RISE  AVOID FEARS","START OPENS THE MENU"};

static const char* const creatureHelp[15]={">PICK YOUR LOOK","L R CHANGE TAB   UP DOWN PICK A ROW","LEFT RIGHT CHANGE IT  A ALSO STEPS","SELECT TURNS THE CREATURE ROUND",">THE TABS","1 BODY  SHAPE AND SKIN","2 FACE  EYES MOUTH AND EARS","3 HAIR  4 CLOTHES  COLOURS AND STYLE","5 BUILD  PLACE EVERY BLOCK YOURSELF",">FINISH","START JUMPS TO THE DONE TAB","GO LIVE LIFE PLAYS YOUR CREATURE",">BLOCK BUILDER","DPAD AND L R MOVE  A PLACE  B ERASE","SELECT+START BACK TO THE TABS"};
static const char* const mapHelp[12]={">BUILD A ROOM","ROOM TOOL  A CORNER  A BUILDS","WALL TOOL  A START  A DRAWS A LINE","FLOOR TOOL  A CORNER  A FILLS","ITEM TOOL  PLACE SINGLE TILES","ERASE TOOL  A CORNER  A CLEARS",">STYLES","L R PICK FLOOR OR ITEM","SEL+L R PICK WALLPAPER","SELECT TAP NEXT TOOL  B CANCELS",">KEEP IT","START OPENS PLAY TEST AND SAVE"};

// ---------- settings screen ----------
static void drawRoom(int ed);
// Timer2 (65536 Hz) is the clock for pacing, the speed meter and the load counter.
#define R_TM2D   (*(volatile u16*)0x04000108)
#define R_TM2CNT (*(volatile u16*)0x0400010A)
#define TICKS_FRAME 1097   // 65536 / 59.7275 Hz
static void tmStart(void){ R_TM2CNT=0; R_TM2D=0; R_TM2CNT=0x82; }
// graphics fields per preset: fps wall wallpaper floors shadows hud
static const u8 presetTab[4][6]={ {0,0,1,1,1,0}, {1,1,1,1,1,0}, {1,2,0,0,0,1}, {2,2,0,0,0,1} };
static const char* const presetNm[5]={"LOOKS","BALANCED","SPEED","BATTERY","CUSTOM"};
static int sCost, sTunedMsg;   // measured cost of drawing one frame (timer ticks); 1 = just auto-tuned
static int presetOf(void){
    for(int p=0;p<4;p++){ const u8*t=presetTab[p];
        if(sFps==t[0]&&sWall==t[1]&&sWp==t[2]&&sFl==t[3]&&sShad==t[4]&&sHud==t[5]) return p; }
    return 4;
}
static void setPreset(int p){ const u8*t=presetTab[p]; sFps=t[0]; sWall=t[1]; sWp=t[2]; sFl=t[3]; sShad=t[4]; sHud=t[5]; }
static void applyRom(void){ REG_WAITCNT=sRom?0x0000:0x4317; }
static void setDefaults(void){ setPreset(1); sCam=1; sSnd=1; sRom=0; sShow=0; sNoWarn=0; applyRom(); jbSetMode(0); }
// Time to draw the room once (timer ticks), averaged over 3 draws. Uses the editor view so it never touches the game state.
static int measureDraw(void){
    drawRoom(1); u16 t0=R_TM2D;
    for(int i=0;i<3;i++) drawRoom(1);
    return (int)(u16)(R_TM2D-t0)/3;
}
static int capLevel(void){   // how many 60 Hz frames one picture really needs: 1 = holds 60 FPS ... 4 = 15 FPS (logic and copy get ~30%)
    int c=sCost+TICKS_FRAME*3/10;
    return c<=TICKS_FRAME?1: c<=2*TICKS_FRAME?2: c<=3*TICKS_FRAME?3: 4;
}
static void autoTune(void){
    int p; for(p=0;p<4;p++){ setPreset(p); sCost=measureDraw(); if(capLevel()<=sFps+1) break; }
    if(p==4){ setPreset(3); sCost=measureDraw(); }
    sTunedMsg=1;
}
#include "slots.h"      // ROOM SLOTS: named saves of the room, the person and the life (header made ready for houses)
#include "optscreen.h"  // OPTIONS: seven pages of settings (replaces the old SETTINGS screen; settingsScreen() keeps its name)

static const signed char hdT[3][3]={{10,12,14},{8,-1,0},{6,4,2}};   // [sign dy+1][sign dx+1] -> heading (16 steps), -1 = keep
#include "feel.h"
static void lifeInit(void){
    mapScan(); bakeSprites(); camSnap=1;
    lfx=spx*256+128; lfy=spy*256+128; lz=lvz=0; lsp=0; lhd=0; lspin=0; lflip=0; lgrind=0; lscore=0; lstun=0; lairF=0; lpts=0; lnoteT=0; lnote=""; lskate=0; lhave=(bdx<0); lfr=0; lvx=lvy=0; ldead=0; lmaxz=0; lplay=0; lbumpCd=0; lfood=100; lbl=0; lnear=0; moodReset(); simsReset(); sfxStop(); feelReset(0);
}
static int rampAvg, rampOn;   // px/step (8.8) the skater has been climbing a ramp, smoothed (heights are whole px, so single steps are lumpy); rampOn = rode a ramp last step
// BABY: cannot be steered. A caretaker keeps the needs up and the baby toddles about by itself: stops now and then, picks a new way
// every second or two, and turns round when it walks into something.
static const char* const growNote[AG_N]={"","NOW A CHILD","NOW A TEEN","NOW AN ADULT","NOW AN ELDER"};
static u16 babyPad(void){
    static const u16 dm[9]={0,K_RIGHT,K_LEFT,K_UP,K_DOWN,K_RIGHT|K_DOWN,K_LEFT|K_DOWN,K_RIGHT|K_UP,K_LEFT|K_UP};
    static int t, dir, still; static s32 ox, oy;
    if(lfx==ox&&lfy==oy&&dir) still++; else still=0;
    ox=lfx; oy=lfy;
    if(--t<=0||still>10){ int r=rnd8(); dir=(r&3)==0?0:1+((r>>2)&7); t=40+(rnd8()&63); still=0; }
    return dm[dir];
}
static void lifeStep(u16 k,u16 pr,int fr){
    if(stage==AG_BABY&&!ldead){ k=babyPad(); pr=0; }   // uncontrollable stage: the pad is ignored (the pause menu still works)
    int fh=surfH(lfx,lfy)<<8;
    if(ldead){   // dead: frozen until A
        lstun=2;
        if(pr&K_A){ ldead=0; lstun=0; lfx=spx*256+128; lfy=spy*256+128; lz=0; lvz=0; lskate=0; lsp=0; lgrind=0; lspin=0; lflip=0; lairF=0; lmaxz=0; lplay=0; lnoteT=0; lfood=100; lbl=0; moodReset(); simsRespawn(); sfxStop(); feelReset(0); }
    }
    if(lstun>0){ lstun--; lsp=0; lvx=lvy=0; }
    else {
        if((pr&K_L)&&!lhave){ lnote="FIND A BOARD"; lnoteT=40; }
        if((pr&K_L)&&lhave&&lz<=fh&&stage>=AG_CHILD){   // L: swap between on-foot (walk/run) and skateboard
            lskate=!lskate; lsp=0; lgrind=0; lspin=0; lflip=0; feelReset(lhd); lnote=lskate?"SKATE":"ON FOOT"; lnoteT=40;
        }
        feelSync(); feelTick(lz<=fh,(pr&K_B)!=0&&lskate&&(lz<=fh||F.coy>0));
        if(lskate){
            if(lz<=fh||(F.coy>0&&lvz<=0)){                     // on the ground (or a rail), incl. coyote frames
                feelSteer(k); feelPush(k,lgrind);
                if(F.buf>0){ lvz=feelOllie(); lgrind=0; }      // ollie (buffered, variable height)
            } else feelAir(k,pr,(lz-fh)<(8<<8));               // airborne
            feelVel(); lspin=F.spin>>4;
        } else {
            feelWalk(k,pr,lz<=fh);                             // D-pad relative to screen, B = run, A = hop
        }
    }
    int zp=(int)(lz>>8);
    s32 nx=lfx+lvx, ny=lfy+lvy;   // move per axis so walls slide
    int bump=0, sp0b=lsp;
    int tol=isRamp(lifeMap[lfy>>8][lfx>>8])?F_RAMP_TOL:3;   // a ramp climbs a few px per step without being a wall
    if(surfH(nx,lfy)<=zp+tol) lfx=nx; else bump=1;
    if(surfH(lfx,ny)<=zp+tol) lfy=ny; else bump=1;
    if(bump){
        lsp=(lsp*2)/3;
        if(lbumpCd==0&&sp0b>=(lskate?12:10)){ lbumpCd=40; if(lskate) hurt(sp0b+(rnd8()>>4),2); else sfxPlay(SFX_BONK); }   // skating into a wall hurts, running into one bonks
    }
    if(lbumpCd>0) lbumpCd--;
    fh=surfH(lfx,lfy)<<8;
    int wasOn=rampOn, onRamp=lskate&&isRamp(lifeMap[lfy>>8][lfx>>8]); rampOn=0;
    if(lz<=fh&&onRamp){ int rise=lz<fh?(int)(fh-lz):0; rampAvg=(rampAvg*3+rise)>>2; rampOn=1; }   // riding a ramp: remember how fast we are climbing
    if(lz<fh){ lz=fh; if(lvz<0) lvz=0; }
    else if(lz>fh&&wasOn&&!onRamp&&lskate&&lvz<=0&&(lz-fh)<(16<<8)){   // rolled off the lip: launch with the climb speed
        int v=rampAvg*F_RAMP_BOOST; if(v>F_RAMP_MAX) v=F_RAMP_MAX; if(v>0){ lvz=v; lnote="AIR"; lnoteT=20; moodEvent(M_LAUNCH); } }
    if(!rampOn) rampAvg=0;
    if(lz>fh||lvz>0){ lz+=lvz; lvz-=0x40; if(lz<=fh&&lvz<=0){ lz=fh; lvz=0; } }   // gravity
    int air=lz>fh;
    if(air){
        int zz=(int)(lz>>8); if(zz>lmaxz) lmaxz=zz;
        if(!lplay&&lvz<0){ int hi=lmaxz-(int)(fh>>8);
            if(hi>=34){ sfxPlay(SFX_SCREAM); lplay=1; }                 // falling from way up
            else if(!feelClean()&&hi>=10){ sfxPlay(SFX_GASP); lplay=1; }   // landing is going wrong
        }
    }
    if(lairF&&!air){                                   // just landed
        int pts=feelHalfTurns()*180+(lflip?100:0)+feelGrabPts();
        int drop=lmaxz-(int)(lz>>8), sp0=lsp, bail=!feelClean();
        if(bail){ lnote="BAIL"; lnoteT=60; lsp=0; lstun=45; lgrind=0; moodEvent(M_BAIL); }
        else{
            if(pts){ pts=moodPts(pts); lscore+=pts; lpts=pts; lnote="NICE"; lnoteT=60; lcN++; lcPts+=pts; lcT=oComboLen(); moodEvent(M_TRICK); }
            if(lskate&&tileH(lfx>>8,lfy>>8)==6){ lgrind=1; lnote="GRIND"; lnoteT=30; lcN++; lcT=oComboLen(); moodEvent(M_GRIND_ON); }
        }
        if(bail) hurt(drop/2+sp0+(rnd8()>>5),1);        // bad landing: harder/faster/higher = worse
        else if(drop>24) hurt(drop-24+(rnd8()>>5),0);   // big drops hurt even landed clean
        lspin=0; lflip=0; feelLandReset();
    }
    if(!air){ lmaxz=(int)(lz>>8); lplay=0; }
    lairF=air;
    if(lgrind){ if(air||tileH(lfx>>8,lfy>>8)!=6) lgrind=0; else if((fr&3)==0){ lscore+=3; lnote="GRIND"; lnoteT=10; lcPts+=3; lcT=oComboLen(); } }
    if(!lhave&&lz<(8<<8)&&(lfx>>8)==BDX&&(lfy>>8)==BDY){ lhave=1; lnote="GOT A SKATEBOARD"; lnoteT=90; moodEvent(M_GOT_BOARD); }   // walk over it to pick it up
    if(!ldead){   // needs: hunger and bladder, then how they (and the skating) make the skater feel
        if(stage==AG_BABY){ if(lfood<70) lfood=70; if(lbl>30) lbl=30; if(sNrg<60) sNrg=60; if(sHyg<60) sHyg=60; if(sCom<60) sCom=60; }   // looked after
        moodTick(); simsTick(pr,(int)(lfx>>8),(int)(lfy>>8));
        if(gGrow){ gGrow=0; setStage(stage+1); bakeSprites(); lnote=growNote[stage]; lnoteT=120; lstun=lstun>30?lstun:30; lsp=0; }
        { int fe=oFoodEvery(), we=oWcEvery();   // FOOD AND WC option
          if(fe&&lfr%fe==0&&lfood>0) lfood--;
          if(we&&lfr%we==0&&lbl<100) lbl++; }
        if(lfood==0&&lfr%300==0){ lfood=15; lstun=120; lsp=0; lgrind=0; sfxPlay(SFX_GROAN); lnote="FAINTED FROM HUNGER"; lnoteT=90; moodEvent(M_FAINT); }
        if(lbl>=100){ lbl=0; lstun=90; lsp=0; lgrind=0; lscore=lscore>100?lscore-100:0; sfxPlay(SFX_CRY); lnote="ACCIDENT"; lnoteT=90; moodEvent(M_ACCIDENT); }
        int nf=0, nt=0, nb=0, nh=0, nc=0;
        for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ int tx=(lfx>>8)+dx, ty=(lfy>>8)+dy; if(tx<0||ty<0||tx>=MW||ty>=MH) continue;
            char c=lifeMap[ty][tx]; if(c=='F') nf=1; if(c=='T') nt=1; if(c=='S') nb=1; if(c=='H') nh=1; if(c=='C') nc=1; }
        lnear=nf?1:(nt?2:(nb?3:(nh?4:(nc?5:0))));   // 1 fridge, 2 toilet, 3 bed, 4 shower, 5 sofa
        if((pr&K_R)&&lnear&&lstun<=0&&lz<=fh){
            if(lnear==1){   // fridge: eat
                if(lfood>=95){ lnote="FULL"; lnoteT=40; }
                else { lfood+=35; if(lfood>100) lfood=100; lbl+=10; if(lbl>99) lbl=99; lstun=30; lsp=0; lnote="YUM"; lnoteT=50; moodEvent(M_EAT); }
            } else if(lnear>=3){ simBegin(lnear);   // bed / shower / sofa (sims.h)
            } else {        // toilet: relieve yourself
                if(lbl<15){ lnote="LATER"; lnoteT=40; }
                else { lbl=0; lstun=70; lsp=0; lgrind=0; lnote="AHH"; lnoteT=60; moodEvent(M_RELIEVE); }
            }
        }
    }
    if(lcN>0){
        if(lstun>0||ldead){ lcN=0; lcPts=0; lcT=0; }                      // a bail or hit loses the chain
        else if(!air&&!lgrind&&--lcT<=0){                                  // chain over: bank the multiplier bonus
            int tot=lcPts*lcN; if(lcN>=2) lscore+=lcPts*(lcN-1);
            lcBank=tot; lcBankT=120;
            if(lcN>=2&&sCam&&tot>camThr[sCam]) lcamPend=1;
            if(lcN>=2) moodEventN(M_COMBO,lcN-1);
            lcN=0; lcPts=0;
        }
    }
    if(lcBankT>0) lcBankT--;
    lfr++;
    if(lnoteT>0) lnoteT--;
    sfxTick();
}
static int ecx=6, ecy=6, efr;   // map editor cursor (tile) and frame counter
// The room can be viewed from 4 sides (action cam). (rx,ry) are screen-space tile coords for the current view, (tx,ty) the real map tile.
static void rotXY(int rx,int ry,int*tx,int*ty){
    switch(cview){ case 0:*tx=rx;*ty=ry;break; case 1:*tx=ry;*ty=MW-1-rx;break; case 2:*tx=MW-1-rx;*ty=MH-1-ry;break; default:*tx=MH-1-ry;*ty=rx; }
}
static void rotPos(s32 x,s32 y,s32*rx,s32*ry){   // same for a position in 1/256 tiles
    switch(cview){ case 0:*rx=x;*ry=y;break; case 1:*rx=MH*256-y;*ry=x;break; case 2:*rx=MW*256-x;*ry=MH*256-y;break; default:*rx=y;*ry=MW*256-x; }
}
static char cellAt(int rx,int ry){ int tx,ty; rotXY(rx,ry,&tx,&ty); return lifeMap[ty][tx]; }
static int wpAt(int rx,int ry){ int tx,ty; rotXY(rx,ry,&tx,&ty); return wallMap[ty][tx]; }
static int flAt(int rx,int ry){ int tx,ty; rotXY(rx,ry,&tx,&ty); return floorMap[ty][tx]; }
static int isWallCh(char c){ return c=='w'||c=='W'; }
// ---- camera: follows the player (play) or the cursor (editor); only the tiles on screen are drawn ----
static void camClamp(int ed){
    int xl=120-MH*CA, xh=120+MW*CA-SW, yl=24-(ed?20:0), yh=24+(MW+MH)*CB-SH+(ed?20:0);
    if(camX<xl) camX=xl; if(camX>xh) camX=xh; if(camY<yl) camY=yl; if(camY>yh) camY=yh;
}
static void camFollow(int snap){   // keep the skater near the middle of the screen, eased so it stays steady
    s32 rfx,rfy; rotPos(lfx+lvx*(lskate?14:10),lfy+lvy*(lskate?14:10),&rfx,&rfy);   // look ahead of the skater (a little less on foot)
    int ox=camX, oy=camY; camX=(int)((rfx-rfy)>>5); camY=(int)((rfx+rfy)>>6)-76; camClamp(0);
    int tx=camX, ty=camY; camX=ox; camY=oy;
    if(cview!=camLastV){ camLastV=cview; snap=1; }
    if(snap){ camX=tx; camY=ty; return; }
    int ease=lskate?4:3, dx=tx-camX, dy=ty-camY, sx=dx/ease, sy=dy/ease;   // on foot the camera catches up a bit faster
    if(!sx) sx=(dx>0)-(dx<0); if(!sy) sy=(dy>0)-(dy<0);
    camX+=sx; camY+=sy;
}
static void bandRows(int*s0,int*s1){   // diagonals (tx+ty) that can touch the screen
    int lo=(-LOY-CB-10)/CB-1, hi=(SH+2*CC+CB-LOY)/CB+1;
    if(lo<0) lo=0; if(hi>MW+MH-2) hi=MW+MH-2; *s0=lo; *s1=hi;
}
static void bandCols(int s,int*a,int*b){   // tx range of diagonal s that falls on screen
    int kmin=(-2*CA-LOX)/CA-1, kmax=(SW+2*CA-LOX)/CA+1;
    int lo=(s+kmin)>>1, hi=(s+kmax+1)>>1, mn=s-(MH-1), mx=s<MW-1?s:MW-1;
    if(mn<0) mn=0; if(lo<mn) lo=mn; if(hi>mx) hi=mx; *a=lo; *b=hi;
}
static int tileOpen(int x,int y){   // in the map and not a wall / crate / fridge
    if(x<0||y<0||x>=MW||y>=MH) return 0;
    char c=cellAt(x,y); return !(c=='w'||c=='W'||c=='#'||c=='F'); }
static int wallH(int tx,int ty){    // wall blocks DRAWN for this tile (the player still bumps into the full height)
    if(cellAt(tx,ty)=='w'||sWall==2) return 1;
    if(sWall==1&&(tileOpen(tx-1,ty)||tileOpen(tx,ty-1)||tileOpen(tx-1,ty-1))) return 1;   // faces the camera: cut it down
    return 2;
}
static int wallJoin(int tx,int ty,int j,int wp){   // neighbour wall with the same wallpaper that reaches block j
    if(tx<0||ty<0||tx>=MW||ty>=MH||!isWallCh(cellAt(tx,ty))) return 0;
    return wallH(tx,ty)>=j&&wpAt(tx,ty)==wp; }
static void drawWall(int tx,int ty,int sx,int sy){
    int h=wallH(tx,ty), wp=wpAt(tx,ty);
    for(int j=1;j<=h;j++){
        int f=(j<h?1:0)|(j>1?2:0)|(wallJoin(tx-1,ty,j,wp)?16:0)|(wallJoin(tx,ty-1,j,wp)?32:0);
        if(sWp) wallBlock(sx,sy-j*CC,wp,f); else cube(sx,sy-j*CC,9+wp,0,f);
    }
}
static void tileMark(int tx,int ty,u16 cc){   // diamond outline on a tile (editor cursor / preview)
    char c=lifeMap[ty][tx]; int hgt=isWallCh(c)?wallH(tx,ty)*CC:tileH(tx,ty);
    int sx=LOX+(tx-ty)*CA, sy=LOY+(tx+ty+1)*CB-hgt;
    if(sx<-CA-1||sx>SW+CA||sy<-CB-2||sy>SH+CB+2) return;   // off screen
    for(int t=-CA;t<=CA;t++){ int at=t<0?-t:t, hh=hhT[0][at]; px(sx+t,sy-hh,cc); px(sx+t,sy-hh-1,cc); px(sx+t,sy+hh,cc); px(sx+t,sy+hh+1,cc); }
}
static void eRect(int*x0,int*y0,int*x1,int*y1){   // anchor..cursor as an ordered rectangle; the WALL tool snaps to a straight line
    int ax=eAx, ay=eAy, bx=ecx, by=ecy;
    if(eTool==T_WALL){ int dx=bx>ax?bx-ax:ax-bx, dy=by>ay?by-ay:ay-by; if(dx>=dy) by=ay; else bx=ax; }
    *x0=ax<bx?ax:bx; *x1=ax<bx?bx:ax; *y0=ay<by?ay:by; *y1=ay<by?by:ay;
}
#include "items.h"
static int lpsx, lpsy;   // where the player is on screen (zoom centre)
static void drawRoom(int ed){   // the room, drawn back to front; ed=1: editor view (no player, markers + cursor)
    fillCols(0,ROW_W,RGB(4,5,8));
    int s0,s1; bandRows(&s0,&s1);
    for(int s=s0;s<=s1;s++){ int a,b; bandCols(s,&a,&b);
        for(int tx=a;tx<=b;tx++){ int ty=s-tx; char c=cellAt(tx,ty); if(c=='w'||c=='W'||c=='#') continue;
            int sx=LOX+(tx-ty)*CA, sy=LOY+(tx+ty+1)*CB;
            { int fl=flAt(tx,ty), v=(tx^ty)&1; if(sFl) floorTile(sx,sy,&flTab[fl][v][0][0]); else tileTop(sx,sy,flFlat[fl][v]); } } }
    s32 rfx,rfy; rotPos(lfx,lfy,&rfx,&rfy);
    int ss=(int)((rfx>>8)+(rfy>>8)), psx=LOX+(int)((rfx-rfy)>>5), psy=LOY+(int)((rfx+rfy)>>6);
    lpsx=psx; lpsy=psy-20;
    for(int s=s0;s<=s1;s++){ int a,b; bandCols(s,&a,&b);
        for(int tx=a;tx<=b;tx++){ int ty=s-tx;
            char c=cellAt(tx,ty); int sx=LOX+(tx-ty)*CA, sy=LOY+(tx+ty+1)*CB;
            if(c=='w'||c=='W') drawWall(tx,ty,sx,sy);
            int ox,oy; rotXY(tx,ty,&ox,&oy);
            if(c=='#'||c=='F'||c=='T'||c=='='||c=='D'||c=='L'||c=='N'||c=='S'||c=='H'||c=='C'||c=='X'||c=='O'||c=='Y'||c=='Z'||c=='K'||c=='J'||c=='M'||isRamp(c)) drawItemTile(c,sx,sy,ox,oy);
            if((ed&&c=='B')||(!ed&&!lhave&&ox==BDX&&oy==BDY)) blitItem(V_BOARD,sx,sy-(ed?0:((lfr>>4)&1)));   // the skateboard pickup, bobbing
            if(ed&&c=='P') drawSpawn(sx,sy+1);   // little person = spawn
        }
        if(!ed&&s==ss){
            int fhp=surfH(lfx,lfy), zp=(int)(lz>>8), vsel=((lhd+lspin+66+4*cview)>>2)&3;
            if(sShad){ rect(psx-3,psy-fhp-1,7,2,RGB(10,8,5)); rect(psx-1,psy-fhp-2,3,4,RGB(10,8,5)); }   // shadow
            if(lskate){ rect(psx-6,psy-zp-1,12,2,RGB(26,10,6)); rect(psx-5,psy-zp+1,2,2,RGB(3,3,6)); rect(psx+3,psy-zp+1,2,2,RGB(3,3,6)); }   // board under the feet
            blit(spr4[vsel],psx-16,psy-40-zp);
        }
    }
    if(ed){
        if(eAct&&eTool!=T_ITEM){   // preview of what the next A will build
            int x0,y0,x1,y1; eRect(&x0,&y0,&x1,&y1);
            u16 pc=eTool==T_ERASE?RGB(31,10,8):RGB(10,28,10);
            for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
                if(eTool==T_ROOM&&x!=x0&&x!=x1&&y!=y0&&y!=y1) continue;   // a room only outlines its walls
                tileMark(x,y,pc);
            }
        }
        tileMark(ecx,ecy,(efr&8)?WHITE:GOLD);   // blinking diamond on the tile under the cursor
    }
}

// Camera zoom: scale the finished picture up around (cx,cy) in place. zk = 256 / zoom. Source pixels are always nearer the
// centre than their destination, so working outward from the centre never reads a pixel that was already overwritten.
static short zxm[SW], zym[SH];
IWRAM_CODE static void zoomFb(int cx,int cy,int zk){
    for(int x=0;x<SW;x++) zxm[x]=(short)(cx+(((x-cx)*zk)>>8));
    for(int y=0;y<SH;y++) zym[y]=(short)(cy+(((y-cy)*zk)>>8));
    for(int pass=0;pass<2;pass++){
        int y0=pass?0:SH-1, y1=pass?cy:cy-1, st=pass?1:-1;
        for(int y=y0;y!=y1;y+=st){
            u16*d=fb+y*SW; const u16*s=fb+zym[y]*SW;
            for(int x=SW-1;x>=cx;x--) d[x]=s[zxm[x]];
            for(int x=0;x<cx;x++) d[x]=s[zxm[x]];
        }
    }
}
// a 7x7 face for the mood state (SAD, BORED, OK, HAPPY, STOKED); 'y' = skin, 'k' = features
static const char* const faceArt[5][7]={
 {".yyyyy.","yyyyyyy","yykykyy","yyyyyyy","yykkkyy","ykyyyky",".yyyyy."},   // sad
 {".yyyyy.","yyyyyyy","ykkykky","yyyyyyy","yykkkyy","yyyyyyy",".yyyyy."},   // bored: half-shut eyes, flat mouth
 {".yyyyy.","yyyyyyy","yykykyy","yyyyyyy","yykkkyy","yyyyyyy",".yyyyy."},   // ok
 {".yyyyy.","yyyyyyy","yykykyy","yyyyyyy","ykyyyky","yykkkyy",".yyyyy."},   // happy
 {".yyyyy.","yyyyyyy","ykkykky","yyyyyyy","ykkkkky","yykkkyy",".yyyyy."} }; // stoked
static void drawFace(int x,int y,int st){
    static const u16 skin[5]={RGB(14,18,28),RGB(22,22,20),RGB(30,26,8),RGB(26,30,10),RGB(31,20,6)};
    for(int j=0;j<7;j++)for(int i=0;i<7;i++){ char c=faceArt[st][j][i]; if(c=='.') continue; px(x+i,y+j,c=='k'?RGB(4,3,6):skin[st]); }
}
static void lifeDraw(void){
    camFollow(camSnap||lcamF>0); camSnap=0;
    drawRoom(0);
    if(lcamF>0){   // action cam: ease in, spin through all 4 views, ease out
        int f=lcamF, z=f<12?f:(f>CAM_LEN-12?CAM_LEN-f:12);   // 0..12 zoom amount
        if(z>0){ int cx=lpsx<0?0:lpsx>=SW?SW-1:lpsx, cy=lpsy<0?0:lpsy>=SH?SH-1:lpsy; zoomFb(cx,cy,256-z*(CAM_ZOOM)/12); }
    }
    u16 gold=GOLD, dim=DIMC, hint=RGB(12,14,16);
    if(!lcamF&&!ldead&&sHud<2) simsBubble(lpsx,lpsy-16,RGB(6,6,10));   // thought bubble over the head (sims.h)
    if(sHud<2){
        simsClockDraw(150,2,dim,gold);
        numText(text(2,2,"SCORE",dim,1)+3,2,lscore,gold);
        text(150,10,"FOOD",dim,1); rect(180,10,lfood/2,5,lfood<20?RGB(28,8,6):RGB(10,24,8));
        text(150,18,"WC",dim,1); rect(180,18,lbl/2,5,lbl>80?RGB(28,8,6):RGB(26,22,6));
        text(150,26,"FUN",dim,1); rect(180,26,moodFunPct()/2,5,moodFunPct()<MOOD_BORED?RGB(28,8,6):RGB(8,22,28));
        text(150,34,"HAPPY",dim,1); rect(180,34,moodHapPct()/2,5,moodHapPct()<MOOD_SAD?RGB(28,8,6):RGB(28,13,19));
        drawFace(150,42,moodState()); text(160,43,moodStName[moodState()],gold,1);
        simsHud(150,52,dim,gold);
    }
    if(sHud==0){
        text(2,10,"SPEED",dim,1); rect(24,10,lsp,5,RGB(8,24,10));
        text(2,139,MAPNAME,RGB(14,16,18),1);
        text(60,2,lskate?"SKATE":(lsp>5?"RUN":"WALK"),gold,1);
        text(2,146,lskate?"A PUSH B OLLIE DPAD STEER L WALK":(lhave?"DPAD WALK B RUN A HOP L SKATE":"DPAD WALK B RUN A HOP FIND A BOARD"),hint,1);
        text(2,153,"START MENU",hint,1);
    }
    if(lcamF>0){ rect(56,126,128,32,PANEL); text(72,130,"BIG COMBO",GOLD,2); numText(120-tw("0",1)*3/2,145,lcBank,WHITE); }   // banner at the bottom so it never covers the skater
    else if(lcN>0&&sHud<2){ int x=text(2,32,"COMBO X",GOLD,1)+1; x=numText(x,32,lcN,WHITE)+4; numText(x,32,lcPts*lcN,gold); }
    else if(lcBankT>0&&sHud<2){ numText(text(2,32,"COMBO",GOLD,1)+3,32,lcBank,WHITE); }
    if(lnear&&!ldead&&!lcamF) text(2,132,simAct?"A OR B GET UP":(lnear==1?"R OPEN FRIDGE":lnear==2?"R USE TOILET":lnear==3?"R SLEEP IN BED":lnear==4?"R TAKE A SHOWER":"R SIT ON SOFA"),gold,1);   // prompts and alerts always show
    if(ldead) text(2,25,"PRESS A TO RESPAWN",RGB(31,12,8),1);
    if(lnoteT>0){ text(2,18,lnote,RGB(31,31,31),1); if(lpts&&lnote[0]=='N'){ numText(text(2,25,"+",gold,1)+1,25,lpts,gold); } }
    if(sShow){   // performance counter, bottom right
        numText(text(184,153,"FPS",dim,1)+3,153,lfpsV,gold);
        if(sShow==2){ numText(text(184,146,"LOAD",dim,1)+3,146,lloadV,lloadV>=100?RGB(30,10,8):gold); }
    }
}
static void camStep(int steps,u16 k,u16 pr){   // action cam: the game holds still while the camera swings round the room
    lcamF+=steps;
    if(((k&K_SEL)&&(pr&K_SEL))||lcamF>=CAM_LEN){ lcamF=0; cview=0; lcBankT=120; }
    else { int f=lcamF; cview=(f<6||f>=60)?0:(f-6)/18+1; if(cview>3) cview=0; }
}
static int gToMenu;   // set when the player picks MAIN MENU in the pause menu, so every screen above returns to it
static const char* const lifeItems[7]={"RESUME","HOW TO PLAY","OPTIONS","ROOM SLOTS","EDIT MAP","NEW LIFE","MAIN MENU"};
static const char* const lifeItemsEd[4]={"RESUME","HOW TO PLAY","OPTIONS","BACK TO EDITOR"};
// Timer2 (65536 Hz) is the clock (defined with the settings). The game logic always runs at 60 steps per second; the
// frame rate setting only says how often the picture is redrawn, so lower rates save work without slowing the game.
static const char* const yesNoLife[2]={"NO","YES ERASE IT"};
// ---- game music: the jukebox songs in their shuffled order while you play (OPTIONS > AUDIO > GAME MUSIC) ----
static int gmPos;
static void gmPlay(void){   // start the song in playlist slot gmPos (always the shuffled order, whatever the jukebox mode is)
    const Song*sg=&songs[jbMap[jbOrd[gmPos]]]; musBegin(sg->adp?1:0,sg->adp,sg->xm);
}
static void gmStart(void){
    if(gMusic||!xo[XO_GAMEMUS]||!sSnd||jbN<=0) return;
    gmPos=(rnd8()*jbN)>>8; if(gmPos>=jbN) gmPos=0;
    gMusic=1; mDucked=0; mGain=mGainT=256; gmPlay();
}
static void gmStop(void){ mGain=mGainT=256; if(!gMusic) return; gMusic=0; mDucked=0; musStop(); }
static void gmSync(void){ if(xo[XO_GAMEMUS]&&sSnd) gmStart(); else gmStop(); }   // after the pause menu: the option or SOUND may have changed
static void gmTick(void){   // once per frame: when the song is over, the next one in the shuffle
    if(!gMusic||mDucked||sfxOn) return;
    if(mKind?mDone:mLaps>=1){ gmPos=(gmPos+1)%jbN; gmPlay(); }
}
static void lifeModeRun(int ed);
static void lifeMode(int ed){ gInPlay=1; lifeModeRun(ed); gInPlay=0; }   // gInPlay: some option actions are only allowed while playing / only outside it
static void lifeModeRun(int ed){   // ed=1: test play started from the map editor
    lifeInit(); lcamF=0; cview=0; lcN=lcPts=lcT=lcBank=lcBankT=lcamPend=0; u16 prev=keyNow(); gmStart();
    tmStart(); u16 tl=R_TM2D; int acc=0, fpsN=0, fr=0; u32 fpsT=0, workT=0; lfpsV=0; lloadV=0;
    for(;;){
        int need=(sFps+1)*TICKS_FRAME-100;
        for(;;){ u16 now=R_TM2D, dt=(u16)(now-tl); tl=now; acc+=dt; fpsT+=dt; if(acc>=need) break; vsync(); }
        u16 w0=R_TM2D;
        int steps=(acc+110)/TICKS_FRAME; if(steps>6){ steps=6; acc=0; } else acc-=steps*TICKS_FRAME;
        u16 k=keyNow(), pr=k&~prev; prev=k;
        if((k&K_SEL)&&(k&K_START)) break;
        if(pr&K_START){   // pause menu
            mGainT=128; sfxStop(); simsSave();   // the music fades to half while a menu is open   // the pause menu is also a save point
            int c=menu("PAUSED",ed?lifeItemsEd:lifeItems,ed?4:7);
            if(c==1) helpScreen("HOW TO PLAY",lifeHelp,16);
            else if(c==2) settingsScreen();
            else if(c==3&&!ed){ simsSaveNow(); if(slotScreen()) lifeInit(); }   // a slot was loaded: start again in the loaded room (the life was written first, so nothing is lost)
            else if(c==4&&!ed){ mapEditor(); lifeInit(); }
            else if(c==5&&!ed){ if(menu("START A NEW LIFE",yesNoLife,2)==1){ simsNewLife(); moodReset(); lscore=0; simLastScore=0; lnote="NEW LIFE"; lnoteT=60; } }
            else if((c==3&&ed)||c==6){ if(c==6) gToMenu=1; break; }
            mGainT=256; gmSync(); prev=keyNow(); tmStart(); tl=R_TM2D; acc=0; lcamF=0; cview=0; continue;
        }
        if(lcamF>0) camStep(steps,k,pr);
        else {
            for(int s=0;s<steps;s++) lifeStep(k,s?0:pr,fr++);   // catch up if a frame took long; button presses count once
            if(lcamPend){ lcamPend=0; if(sCam){ lcamF=1; cview=0; } }
        }
        gmTick(); lifeDraw(); workT+=(u16)(R_TM2D-w0); present();
        fpsN++; if(fpsT>=65536){ lfpsV=fpsN; lloadV=(int)(workT/(u32)fpsN*100/(u32)((sFps+1)*TICKS_FRAME)); workT=0; fpsN=0; fpsT-=65536; }
    }
    simsSave(); R_TM2CNT=0; gmStop(); sfxStop(); lcamF=0; cview=0;   // leaving the life game saves it
    while((~REG_KEYINPUT)&0x3FF) vsync();   // wait for release so the caller doesn't see the exit keys
}


// ---------- map editor ----------
// Tools: ROOM (two corners -> walls + floor + a door), WALL (a straight line), FLOOR (fill an area), ITEM (single tiles), ERASE (clear an area).
static const char* const mapItems[7]={"PLAY TEST","SAVE MAP","ROOM SLOTS","OPTIONS","RESET MAP","HOW TO EDIT","BACK"};
static const char* const yesNo[2]={"NO","YES RESET"};
static const char* const toolNm[NTOOL]={"ROOM","WALL","FLOOR","ITEM","ERASE"};
static const char* const toolHint[NTOOL][2]={
 {"A CORNER  A AGAIN BUILDS THE ROOM  B CANCEL","L R FLOOR  SEL+L R WALLPAPER  SEL TOOL"},
 {"A START  A AGAIN DRAWS A WALL  B CANCEL","L R WALLPAPER  SEL TOOL  START MENU"},
 {"A CORNER  A AGAIN FILLS THE AREA  B CANCEL","L R FLOOR  SEL TOOL  START MENU"},
 {"A PLACE  B ERASE  HOLD AND MOVE TO PAINT","L R ITEM  SEL+A TURN RAMP  SEL TOOL"},
 {"A CORNER  A AGAIN CLEARS THE AREA  B CANCEL","SEL TOOL  START MENU"} };
static void texSwatch(const Tex*t,int x,int y){   // the 8x8 pattern itself, 1:1
    rect(x-1,y-1,10,10,WHITE);
    for(int v=0;v<8;v++)for(int u=0;u<8;u++) px(x+u,y+v,t->c[t->p[v][u]-'0']);
}
static int eApply(void){   // second A of ROOM / WALL / FLOOR / ERASE. 0 = refused
    int x0,y0,x1,y1; eRect(&x0,&y0,&x1,&y1);
    if(eTool==T_ROOM){
        if(x1-x0<2||y1-y0<2) return 0;   // needs at least 3 x 3
        for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
            floorMap[y][x]=(u8)eFl;
            if(x==x0||x==x1||y==y0||y==y1){ lifeMap[y][x]='W'; wallMap[y][x]=(u8)eWp; }
            else if(isWallCh(lifeMap[y][x])) lifeMap[y][x]='.';   // old walls inside are cleared, furniture stays
        }
        lifeMap[y1][(x0+x1)/2]='D';   // doorway in the front wall; move or remove it with the ITEM tool
    } else for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
        if(eTool==T_WALL){ lifeMap[y][x]='W'; wallMap[y][x]=(u8)eWp; }
        else if(eTool==T_FLOOR) floorMap[y][x]=(u8)eFl;
        else { lifeMap[y][x]='.'; floorMap[y][x]=0; wallMap[y][x]=0; }
    }
    return 1;
}
static void drawEditorHud(const char*msg){
    int x=2;
    for(int i=0;i<NTOOL;i++){ int w=tw(toolNm[i],1)+4;
        rect(x,1,w,8,i==eTool?GOLD:RGB(3,4,7)); text(x+2,1,toolNm[i],i==eTool?RGB(4,3,6):DIMC,1); x+=w+1; }
    if(msg[0]) text(2,11,msg,WHITE,1);
    else if(eTool!=T_ITEM){
        if(eAct){ int x0,y0,x1,y1; eRect(&x0,&y0,&x1,&y1); int w=x1-x0+1, h=y1-y0+1;
            if(eTool==T_WALL){ numText(text(2,11,"LENGTH",GOLD,1)+3,11,w+h-1,WHITE); }
            else { int xx=numText(text(2,11,"SIZE",GOLD,1)+3,11,w,WHITE)+3; xx=text(xx,11,"X",GOLD,1)+3; numText(xx,11,h,WHITE); } }
        else text(2,11,"PICK A START POINT",GOLD,1);
    }
    if(eTool==T_ITEM){
        { int vis=18, first=eOb-9; if(first<0) first=0; if(first>NOBJ-vis) first=NOBJ-vis;   // a window of the palette that follows the cursor
          for(int j=0;j<vis;j++){ int i=first+j, xx=2+j*11; rect(xx,136,10,9,i==eOb?WHITE:RGB(3,4,7)); rect(xx+1,137,8,7,palCol[i]); } }
        { static const char*const faceNm[4]={"FACES S","FACES E","FACES N","FACES W"};
          int xx=text(2,127,palNm[eOb],WHITE,1)+4; if(eOb==OB_KICKER||eOb==OB_QPIPE||eOb==OB_LAUNCH) text(xx,127,faceNm[eRot],GOLD,1); }
        if(eOb>=3){ rect(204,114,34,36,RGB(4,5,8)); tileTop(221,141,RGB(14,14,18));   // preview of the picked item
            switch(eOb){ case 3:blitItem(V_CRATE,221,141);break; case 4:blitItem(V_RAILU,221,141);break; case 5:blitItem(V_FRIDGE,221,141);break;
                case 6:blitItem(V_TOILET,221,141);break; case 7:blitItem(V_DOOR,221,141);break; case 8:blitItem(V_BOARD,221,141);break;
                case OB_KICKER:blitItem(V_KICKER+((eRot-cview)&3),221,141);break; case OB_QPIPE:blitItem(V_QPIPE+((eRot-cview)&3),221,141);break;
                case 12:blitItem(V_LEDGEU,221,141);break; case 13:blitItem(V_BENCHU,221,141);break;
                case OB_LAUNCH:blitItem(V_LAUNCH+((eRot-cview)&3),221,141);break; case 18:blitItem(V_FUNBOX,221,141);break; case 19:blitItem(V_BARREL,221,141);break;
                case 20:blitItem(V_TRASH,221,141);break; case 21:blitItem(V_PLANTER,221,141);break; case 22:blitItem(V_PICNIC,221,141);break;
                case 23:blitItem(V_JERSEYU,221,141);break; case 24:blitItem(V_MPAD,221,141);break;
                case 14:blitItem(V_BED,221,141);break; case 15:blitItem(V_SHOWER,221,141);break; case 16:blitItem(V_SOFA,221,141);break; default:drawSpawn(221,142); } }
        if(eOb==1||eOb==2){ texSwatch(&wpTex[eWp],212,137); }
    } else if(eTool!=T_ERASE){
        if(eTool!=T_WALL){ text(2,139,"FLOOR",DIMC,1); texSwatch(&flTex[eFl],24,137); text(36,139,flTex[eFl].nm,WHITE,1); }
        if(eTool!=T_FLOOR){ text(100,139,"WALL",DIMC,1); texSwatch(&wpTex[eWp],118,137); text(130,139,wpTex[eWp].nm,WHITE,1); }
    } else text(2,139,"CLEARS WALLS ITEMS AND FLOORS",DIMC,1);
    text(2,147,toolHint[eTool][0],RGB(12,14,16),1); text(2,153,toolHint[eTool][1],RGB(12,14,16),1);
}
static void edCamSnap(void){ camX=(ecx-ecy)*CA; camY=24+(ecx+ecy+1)*CB-80; camClamp(1); }
static int edCamStep(void){   // dead-zone camera: the view only scrolls when the cursor nears the edge of the screen
    int sx=LOX+(ecx-ecy)*CA, sy=LOY+(ecx+ecy+1)*CB, dx=0, dy=0;
    if(sx<76) dx=sx-76; else if(sx>164) dx=sx-164;
    if(sy<48) dy=sy-48; else if(sy>112) dy=sy-112;
    if(!dx&&!dy) return 0;
    if(dx>10) dx=10; if(dx<-10) dx=-10; if(dy>5) dy=5; if(dy<-5) dy=-5;
    int ox=camX, oy=camY; camX+=dx; camY+=dy; camClamp(1);
    return camX!=ox||camY!=oy;
}
static void mmPx(int x,int y,u16 c){ if((unsigned)x<MW&&(unsigned)y<MH) px(SW-MW-3+x,2+y,c); }
static void miniMap(void){   // whole map at 1 px per tile, top right: colours by tile, the camera's view outlined, cursor blinking
    int X0=SW-MW-3, Y0=2;
    rect(X0-1,Y0-1,MW+2,MH+2,RGB(3,4,7));
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){
        char c=lifeMap[y][x]; u16 col;
        if(c=='.') col=shade(flFlat[floorMap[y][x]][0],10);
        else { int i=palIdx(c); col=i>=0?palCol[i]:0; }
        px(X0+x,Y0+y,col);
    }
    int ca=(120-LOX)/CA, cb=(80-LOY)/CB-1;   // screen centre as (tx-ty, tx+ty); the screen is a tilted box on the map
    for(int t=-15;t<=15;t++){ int a=ca+t, b=cb-20; mmPx((a+b)>>1,(b-a)>>1,RGB(20,22,24)); b=cb+20; mmPx((a+b)>>1,(b-a)>>1,RGB(20,22,24)); }
    for(int t=-20;t<=20;t++){ int b=cb+t, a=ca-15; mmPx((a+b)>>1,(b-a)>>1,RGB(20,22,24)); a=ca+15; mmPx((a+b)>>1,(b-a)>>1,RGB(20,22,24)); }
    u16 cc=(efr&8)?WHITE:GOLD;
    mmPx(ecx,ecy,cc); mmPx(ecx-1,ecy,cc); mmPx(ecx+1,ecy,cc); mmPx(ecx,ecy-1,cc); mmPx(ecx,ecy+1,cc);
}
static void mapEditor(void){
    int hold[4]={0}, comboUsed=0, dirty=1, lastBl=-1, msgT=0; const char*msg=""; u16 prev=keyNow();
    static const u16 dirK[4]={K_RIGHT,K_LEFT,K_UP,K_DOWN};
    eAct=0; edCamSnap();
    for(efr=0;;efr++){
        u16 k=keyNow(), pr=k&~prev, rel=prev&~k; prev=k;
        int tr[4];
        for(int i=0;i<4;i++){ hold[i]=(k&dirK[i])?hold[i]+1:0; tr[i]=(hold[i]==1)||(hold[i]>oRepDelay()&&(hold[i]&oRepMask())==0); }
        int ux=tr[0]-tr[1], uy=tr[3]-tr[2];
        if(ux||uy){   // screen-relative like walking: up = away from the camera
            int dx=ux+uy, dy=uy-ux; dx=(dx>0)-(dx<0); dy=(dy>0)-(dy<0);
            ecx+=dx; ecy+=dy; if(ecx<0)ecx=0; if(ecy<0)ecy=0; if(ecx>=MW)ecx=MW-1; if(ecy>=MH)ecy=MH-1;
            if(eTool==T_ITEM){ if(k&K_A) mapPlace(ecx,ecy,edObjCh()); else if(k&K_B) mapPlace(ecx,ecy,'.'); }
            dirty=1;
        }
        if(pr|rel) dirty=1;
        if(pr&(K_L|K_R)){
            int d=(pr&K_R)?1:-1;
            if(k&K_SEL){ eWp=(eWp+d+NWP)%NWP; comboUsed=1; }
            else if(eTool==T_ITEM) eOb=(eOb+d+NOBJ)%NOBJ;
            else if(eTool==T_WALL) eWp=(eWp+d+NWP)%NWP;
            else if(eTool!=T_ERASE) eFl=(eFl+d+NFL)%NFL;
        }
        if(rel&K_SEL){ if(!comboUsed){ eTool=(eTool+1)%NTOOL; eAct=0; } comboUsed=0; }
        if(pr&K_A){
            if(eTool==T_ITEM&&(k&K_SEL)){ eRot=(eRot+1)&3; comboUsed=1; msg="TURNED"; msgT=20; }   // SEL+A: turn the next ramp
            else if(eTool==T_ITEM) mapPlace(ecx,ecy,edObjCh());
            else if(!eAct){ eAct=1; eAx=ecx; eAy=ecy; }
            else if(eApply()){ eAct=0; msg=eTool==T_ROOM?"ROOM BUILT":eTool==T_WALL?"WALL BUILT":eTool==T_FLOOR?"FLOOR LAID":"CLEARED"; msgT=70; }
            else { msg="ROOM NEEDS 3 X 3 OR BIGGER"; msgT=70; }
        }
        if(pr&K_B){ if(eAct) eAct=0; else mapPlace(ecx,ecy,'.'); }
        if(pr&K_START){
            int c=menu("MAP MENU",mapItems,7);
            if(c==0){ mapScan(); lifeMode(1); }
            else if(c==1){ mapSave();
                if(!mapSaved()) toast("SAVE NOT SUPPORTED HERE");
                else if(xo[XO_SLOTSYNC]&&slotSyncActive()) toast("MAP AND SLOT SAVED");   // MAP SAVE TO SLOT option
                else toast("MAP SAVED"); }
            else if(c==2) slotScreen();
            else if(c==3) settingsScreen();
            else if(c==4){ if(!xo[XO_RESETASK]||menu("RESET THE MAP",yesNo,2)==1){ mapReset(); eAct=0; toast("MAP RESET"); } }
            else if(c==5) helpScreen("HOW TO EDIT",mapHelp,12);
            else if(c==6){ if(xo[XO_EDSAVE]) mapSave(); break; }
            prev=keyNow(); edCamSnap(); dirty=1; continue;
        }
        if(edCamStep()) dirty=1;
        if(msgT>0&&--msgT==0){ msg=""; dirty=1; }
        int bl=(efr>>3)&1;   // the editor only redraws when something changed or the cursor blinks
        if(dirty||bl!=lastBl){
            drawRoom(1); drawEditorHud(msgT>0?msg:""); if(xo[XO_MINI]) miniMap();
            present(); dirty=0; lastBl=bl;
        } else vsync();
    }
    while((~REG_KEYINPUT)&0x3FF) vsync();
}


// ---------- creature creator ----------
// Pick a look from numbered tabs (like a character creator), or open the block builder to place every block by hand.
// L R change tab | UP DOWN pick a row | LEFT RIGHT change it | SELECT turns the creature | START jumps to DONE | B leaves.
enum { TB_BODY, TB_FACE, TB_HAIR, TB_CLOTHES, TB_BUILD, TB_DONE, NTAB };
enum { RK_PICK, RK_SWATCH, RK_ACT };                 // a row picks from named options, picks a colour, or is a button
enum { AC_BUILD, AC_PLAY, AC_MAP, AC_MENU };
typedef struct { const char*lab,*sub; u8 kind,id,n; } Row;   // sub = second line of a button
static const char* const tabNm[NTAB]={"BODY","FACE","HAIR","CLOTHES","BUILD","DONE"};
static const char* const shapeNm[4]={"AVERAGE","BROAD","BIG HEAD","STUBBY"};
static const char* const eyeNm[3]={"SLEEPY","ROUND","HAPPY"};
static const char* const mouthNm[3]={"FLAT","SMILE","OH"};
static const char* const earNm[3]={"NONE","SMALL","BIG"};
static const char* const hairNm[4]={"CROP","BOWL","LONG","BALD"};
#define LK_AGE LK_N   // the AGE row is not part of look[]: it picks the life stage
static const char* const* const lookNm[LK_N+1]={shapeNm,0,eyeNm,mouthNm,earNm,hairNm,0,0,0,stageNm};
static const u16* const lookCol[LK_N+1]={0,skinTones,0,0,0,0,hairTones,topTones,botTones,0};
static const Row tabRow[NTAB][3]={
  {{"AGE",0,RK_PICK,LK_AGE,AG_N},{"SHAPE",0,RK_PICK,LK_SHAPE,4},{"SKIN",0,RK_SWATCH,LK_SKIN,NSW}},
  {{"EYES",0,RK_PICK,LK_EYES,3},{"MOUTH",0,RK_PICK,LK_MOUTH,3},{"EARS",0,RK_PICK,LK_EARS,3}},
  {{"STYLE",0,RK_PICK,LK_HSTYLE,4},{"COLOUR",0,RK_SWATCH,LK_HCOL,NSW},{0}},
  {{"TOP",0,RK_SWATCH,LK_TOP,NSW},{"BOTTOM",0,RK_SWATCH,LK_BOT,NSW},{0}},
  {{"OPEN BUILDER","BY HAND",RK_ACT,AC_BUILD,0},{0},{0}},
  {{"GO LIVE LIFE!","PLAY IT NOW",RK_ACT,AC_PLAY,0},{"EDIT MAP","BUILD ROOMS",RK_ACT,AC_MAP,0},{"MAIN MENU","LOOK IS KEPT",RK_ACT,AC_MENU,0}} };
static const u8 tabN[NTAB]={3,3,2,2,1,3};

// layout (the panel is x 124..239): tabs down the left edge, the card of rows beside them, key legend under both
#define TBX 128
#define TBW 18
#define TBH 17
#define TBP 20
#define TBY 7
#define CDX 149
#define CDY 5
#define CDW 91
#define CDH 124
#define RW0 (CDY+26)      // first row
#define RHT 19            // row pitch
#define CARD   RGB(8,11,21)
#define CARDED RGB(14,17,30)
#define FOCUS  RGB(6,16,8)
#define GOLD2  RGB(14,11,3)

static void roundRect(int x,int y,int w,int h,u16 c){ rect(x+1,y,w-2,h,c); rect(x,y+1,w,h-2,c); }
static void tri(int x,int y,int dir,u16 c){   // dir 0 left, 1 right (3 wide, 5 tall); 2 up, 3 down (5 wide, 3 tall); x,y = top left
    if(dir<2){ for(int r=0;r<5;r++){ int hf=2-(r<2?2-r:r-2); if(dir==0) rect(x+2-hf,y+r,hf+1,1,c); else rect(x,y+r,hf+1,1,c); } }
    else for(int r=0;r<3;r++){ if(dir==2) rect(x+2-r,y+r,2*r+1,1,c); else rect(x+r,y+r,5-2*r,1,c); }
}
static int kcap(int x,int y,const char*t){   // a little key cap with a label in it; returns the x after it
    int w=tw(t,1)+5; rect(x,y-2,w,10,RGB(22,18,5)); rect(x+1,y-1,w-2,8,RGB(8,9,15)); text(x+3,y,t,GOLD,1); return x+w+2;
}
static int kcapAr(int x,int y,int vert){   // a key cap showing two arrows: up/down or left/right
    int w=vert?17:14; rect(x,y-2,w,10,RGB(22,18,5)); rect(x+1,y-1,w-2,8,RGB(8,9,15));
    if(vert){ tri(x+3,y+1,2,GOLD); tri(x+9,y+2,3,GOLD); } else { tri(x+3,y,0,GOLD); tri(x+8,y,1,GOLD); }
    return x+w+2;
}
static int klab(int x,int y,const char*t){ return text(x,y,t,RGB(20,22,26),1)+6; }
static void disc(int x0,int y0,int r,u16 c){ for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++) if(dx*dx+dy*dy<=r*r) px(x0+dx,y0+dy,c); }

static const char* const iconArt[5][9]={
  {"...###...","...###...","...###...","..#####..",".#.###.#.",".#.###.#.","...#.#...","...#.#...","...#.#..."},   // body
  {"..#####..",".#.....#.","#.#...#.#","#.#...#.#","#.......#","#.#...#.#","#..###..#",".#.....#.","..#####.."},   // face
  {"..#####..",".#######.","#########","##.....##","#.......#","#.......#",".#.....#.","..#...#..","........."},   // hair
  {".##...##.","####.####","#########","#.#####.#","..#####..","..#####..","..#####..","..#####..","........."},   // clothes
  {"....#....","..#####..","..#####..","..#####..","..#####..","..#####..","....#....",".........","........."} };  // (build is drawn with lines)
static void drawIcon(int x,int y,int id,u16 c){
    if(id==TB_BUILD){ line(x+4,y,x+8,y+2,c); line(x+8,y+2,x+8,y+6,c); line(x+8,y+6,x+4,y+8,c); line(x+4,y+8,x,y+6,c); line(x,y+6,x,y+2,c); line(x,y+2,x+4,y,c);
        line(x+4,y+4,x,y+2,c); line(x+4,y+4,x+8,y+2,c); line(x+4,y+4,x+4,y+8,c); return; }
    if(id==TB_DONE){ line(x,y+4,x+3,y+7,c); line(x+3,y+7,x+9,y+1,c); line(x,y+3,x+3,y+6,c); line(x+3,y+6,x+9,y,c); return; }
    for(int r=0;r<9;r++)for(int q=0;q<9;q++) if(iconArt[id][r][q]=='#') px(x+q,y+r,c);
}
static void panelBg(void){
    for(int y=0;y<SH;y++){ u16 c=RGB(2+y/55,3+y/38,9+y/14); u32 v=c|((u32)c<<16), *row=(u32*)fb+y*ROW_W; for(int w=SCENE_W;w<ROW_W;w++) row[w]=v; }
    rect(PANEL_X,0,2,SH,GOLD2);
}
static void drawTabs(int tab){
    roundRect(CDX-1,CDY-1,CDW+2,CDH+2,CARDED); roundRect(CDX,CDY,CDW,CDH,CARD);   // the card first, tabs on top of its edge
    for(int i=0;i<NTAB;i++){
        int y=TBY+i*TBP, sel=(i==tab); u16 ink=sel?WHITE:RGB(14,17,22);
        if(sel){ roundRect(TBX-1,y-1,CDX-TBX+3,TBH+2,GOLD); roundRect(TBX,y,CDX-TBX+3,TBH,CARD); rect(CDX-1,y,3,TBH,CARD); }   // open into the card
        else   { roundRect(TBX,y,TBW,TBH,RGB(4,6,13)); roundRect(TBX+1,y+1,TBW-2,TBH-2,RGB(9,12,22)); }
        if(i==TB_DONE) drawIcon(TBX+4,y+4,TB_DONE,sel?RGB(14,30,12):RGB(8,18,8));
        else { char d[2]={(char)('1'+i),0}; text(TBX+(TBW-tw(d,2))/2,y+2,d,ink,2); }
    }
}
static const char* const lookName(int id,int v){ return lookNm[id][v]; }
static int lkAllowed(int id,int v){   // may this stage pick option v of row id?
    switch(id){
      case LK_SHAPE: return stMaskShape[stage]>>v&1;
      case LK_EARS:  return stMaskEars[stage]>>v&1;
      case LK_HSTYLE:return stMaskHair[stage]>>v&1;
      case LK_SKIN: case LK_HCOL: case LK_TOP: case LK_BOT: return v<stSwatches[stage];
      default: return 1;
    }
}
static int lkCount(int id,int n,int*rank){   // options on offer, and the 1-based place of the current one among them
    int c=0, cur=id==LK_AGE?stage:look[id]; *rank=1;
    for(int v=0;v<n;v++) if(lkAllowed(id,v)){ if(v==cur) *rank=c+1; c++; }
    return c;
}
static void drawRowSet(int tab,int sel){
    if(tab==TB_BUILD){
        const char*t[]={"PLACE BLOCKS BY","HAND FOR A ONE","OF A KIND LOOK.","","CHANGING SHAPE,","EARS OR HAIR","STYLE LATER ASKS","BEFORE REPLACING","YOUR BLOCKS."};
        for(int i=0;i<9;i++) text(CDX+6,RW0+i*7,t[i],i<3?WHITE:DIMC,1);
    }
    for(int i=0;i<tabN[tab];i++){
        const Row*r=&tabRow[tab][i]; int y=RW0+i*RHT, f=(i==sel);
        if(tab==TB_BUILD) y=RW0+9*7+4;
        if(r->kind==RK_ACT){
            rect(CDX+3,y-2,CDW-6,17,f?FOCUS:RGB(5,8,16)); if(f){ rect(CDX+3,y-2,2,17,GOLD); }
            text(CDX+9,y,r->lab,f?GOLD:WHITE,1); text(CDX+9,y+8,r->sub,f?WHITE:DIMC,1); continue;
        }
        if(f){ rect(CDX+3,y-2,CDW-6,RHT-1,FOCUS); rect(CDX+3,y-2,2,RHT-1,GOLD); }
        text(CDX+9,y,r->lab,f?WHITE:DIMC,1);
        int cur=r->id==LK_AGE?stage:look[r->id], rk, cnt=lkCount(r->id,r->n,&rk);
        { char b[4]={(char)('0'+rk),'/',(char)('0'+cnt),0}; text(CDX+CDW-6-tw(b,1),y,b,f?DIMC:RGB(10,12,16),1); }
        if(r->kind==RK_PICK){
            const char*nm=lookName(r->id,cur); int mx=CDX+CDW/2;
            tri(CDX+9,y+9,0,f?GOLD:RGB(10,12,16)); tri(CDX+CDW-12,y+9,1,f?GOLD:RGB(10,12,16));
            text(mx-tw(nm,1)/2,y+9,nm,f?WHITE:DIMC,1);
        } else {
            const u16*pal=lookCol[r->id];
            for(int q=0;q<cnt;q++){
                int x=CDX+8+q*10, on=(q==look[r->id]);
                if(on){ rect(x-1,y+8,11,11,f?WHITE:RGB(16,18,22)); }
                rect(x,y+9,9,9,pal[q]);
            }
        }
    }
}
static void drawCreatorPanel(int tab,int sel){
    panelBg(); drawTabs(tab);
    drawIcon(CDX+6,CDY+6,tab,GOLD); text(CDX+20,CDY+4,tabNm[tab],GOLD,2);
    rect(CDX+5,CDY+20,CDW-10,1,GOLD2);
    drawRowSet(tab,sel);
    int act=(tabRow[tab][sel].kind==RK_ACT), x;
    x=kcap(128,132,"L"); x=kcap(x,132,"R"); x=klab(x,132,"TABS"); x=kcapAr(x,132,1); klab(x,132,"ROW");
    x=act?kcap(128,142,"A"):kcapAr(128,142,0); klab(x,142,act?"CHOOSE":"CHANGE");
    x=kcap(128,152,"START"); x=klab(x,152,"DONE"); x=kcap(x,152,"B"); klab(x,152,"BACK");
}
static void drawDial(void){   // the creature's compass: the needle points the way it faces on screen (view 0 = down-left, then clockwise)
    static const signed char ddx[4]={-1,-1,1,1}, ddy[4]={1,-1,-1,1};
    int x0=18, y0=143;
    disc(x0,y0,12,GOLD2); disc(x0,y0,11,RGB(4,6,12));
    for(int i=0;i<4;i++) rect(x0+ddx[i]*7-1,y0+ddy[i]*7-1,2,2,RGB(10,12,18));
    int tx=x0+ddx[view&3]*7, ty=y0+ddy[view&3]*7;
    line(x0,y0,tx,ty,GOLD); rect(tx-1,ty-1,3,3,GOLD); rect(x0-1,y0-1,3,3,WHITE);
    int x=kcap(34,152,"SELECT"); klab(x,152,"TURN");
}
static void drawCreatorScene(void){
    stageOn=1; drawScene(0);
    text(7,6,"MAKE CREATURE",RGB(3,3,6),1); text(6,5,"MAKE CREATURE",GOLD,1);
    drawDial();
}

// ---- tab actions ----
static int confirmRebuild(void){ static const char* const it[2]={"YES  REBUILD","NO  KEEP BLOCKS"}; return menu("REPLACE YOUR BLOCKS?",it,2)==0; }
static void lookStep(int id,int n,int d){
    if(id==LK_AGE){   // the life stage: a look-built creature is rebuilt, hand-built blocks stay but are cut to a smaller box
        int ns=(stage+d+AG_N)%AG_N;
        if(custom&&ns<stage){ static const char* const it[2]={"YES  CUT THEM","NO  KEEP AGE"}; if(menu("CUT BLOCKS TO FIT?",it,2)!=0) return; }
        setStage(ns); return;
    }
    int nv=look[id];
    for(int t=0;t<n;t++){ nv=(nv+d+n)%n; if(lkAllowed(id,nv)) break; }   // skip what this stage cannot have
    if(nv==look[id]) return;
    if((id==LK_SHAPE||id==LK_EARS||id==LK_HSTYLE)&&custom&&!confirmRebuild()) return;   // declined: keep the hand-built blocks
    look[id]=(u8)nv;
    switch(id){
      case LK_SKIN: case LK_HCOL: case LK_TOP: case LK_BOT: setColors(); break;
      case LK_EYES:  sty[0]=nv; restyle(0); break;
      case LK_MOUTH: sty[1]=nv; restyle(1); break;
      default: buildLook(); break;
    }
}

// ---- the block builder (the original free editor, with a legend you can read) ----
static void drawBuildPanel(void){
    panelBg();
    text(130,4,"BLOCK BUILDER",GOLD,1); text(236-tw(stageNm[stage],1),4,stageNm[stage],WHITE,1);
    for(int i=0;i<NPARTS;i++){
        int y=14+i*7;
        if(i==part){ rect(128,y-1,108,7,FOCUS); rect(128,y-1,2,7,GOLD); }
        text(134,y,parts[i].name,i==part?WHITE:DIMC,1);
        if(i==part&&parts[i].dk) text(190,y,spr[SPRID(parts[i].dk-1)].name,GOLD,1);
    }
    text(130,74,"SIZE",DIMC,1);
    const char*sn[3]={"S","M","L"};
    for(int i=0;i<3;i++){ int x=156+i*14, ok=i<stMaxSz[stage]; rect(x,73,11,9,i==size-1?FOCUS:RGB(2,3,5)); text(x+4,74,sn[i],i==size-1?WHITE:ok?DIMC:RGB(6,7,9),1); }
    text(130,85,"X",DIMC,1); num(136,85,cx,WHITE); text(148,85,"Y",DIMC,1); num(154,85,cy,WHITE);
    text(166,85,"Z",DIMC,1); num(172,85,cz,(part<NPARTS&&parts[part].dk)?RGB(12,14,16):WHITE);   // sprites ignore Z
    int x;
    x=kcap(128,98,"DPAD"); x=klab(x,98,"MOVE"); x=kcap(x,98,"L"); x=kcap(x,98,"R"); klab(x,98,"LIFT");
    x=kcap(128,108,"A"); x=klab(x,108,"PLACE"); x=kcap(x,108,"B"); klab(x,108,"ERASE");
    x=kcap(128,118,"SELECT"); klab(x,118,"NEXT PART");
    x=kcap(128,128,"START"); klab(x,128,"SIZE S M L");
    x=kcap(128,138,"SEL"); x=kcapAr(x,138,0); klab(x,138,"TURN");
    x=kcap(128,148,"SEL"); x=kcap(x,148,"START"); klab(x,148,"BACK");
}
static void blockBuilder(void){
    int hx,hy,hz,hs; headBox(&hx,&hy,&hz,&hs);
    part=0; size=hs; cx=hx; cy=hy; cz=hz; view=0; stageOn=0;
    u16 prev=keyNow(); int hold[10]={0}, frame=0, dirty=1, lastBlink=-1, comboUsed=0;
    for(;;){
        u16 k=keyNow(), pressed=k&~prev, released=prev&~k; prev=k;
        for(int i=0;i<10;i++) hold[i]=(k>>i&1)?hold[i]+1:0;
        #define TRIG(m,i) ((pressed&(m))||(hold[i]>14&&(hold[i]&3)==0))
        int sel=k&K_SEL;
        if(sel){
            if(pressed&K_START){ break; }                                                  // SELECT+START: back to the pickers
            if(pressed&K_R){ part=(part+1)%NPARTS; comboUsed=1; dirty=1; }
            if(pressed&K_L){ part=(part+NPARTS-1)%NPARTS; comboUsed=1; dirty=1; }
            if(pressed&K_RIGHT){ view=(view+1)&3; comboUsed=1; dirty=1; }
            if(pressed&K_LEFT){ view=(view+3)&3; comboUsed=1; dirty=1; }
        } else {
            if(TRIG(K_RIGHT,4)){moveView(1,0);dirty=1;} if(TRIG(K_LEFT,5)){moveView(-1,0);dirty=1;}
            if(TRIG(K_UP,6)){moveView(0,-1);dirty=1;}     if(TRIG(K_DOWN,7)){moveView(0,1);dirty=1;}
            if(TRIG(K_R,8)){cy++;dirty=1;}      if(TRIG(K_L,9)){cy--;dirty=1;}
            if(pressed&K_A){ doPart(1,part,size,cx,cy,cz); custom=1; dirty=1; }
            if(pressed&K_B){ doPart(2,part,size,cx,cy,cz); custom=1; dirty=1; }
            if(pressed&K_START){ size=size%stMaxSz[stage]+1; dirty=1; }
        }
        if(released&K_SEL){ if(!comboUsed){ part=(part+1)%NPARTS; dirty=1; } comboUsed=0; }
        clampCursor();
        if(dirty) frame=16;   // restart blink with the ghost visible
        int blink=(frame>>4)&1;
        if(dirty||blink!=lastBlink){
            for(int y=0;y<H;y++)for(int z=0;z<D;z++)for(int x=0;x<W;x++){ ghost[y][z][x]=0; gdec[y][z][x]=0; }
            gAny=0;
            doPart(0,part,size,cx,cy,cz);
            if(parts[part].dk&&!gAny) ghost[cy][cz][cx]=1;   // nothing solid under the cursor: show a marker cube
            drawScene(blink); if(dirty){ drawBuildPanel(); present(); } else presentScene();   // blink-only: scene columns only
            dirty=0; lastBlink=blink;
        } else vsync();
        frame++;
    }
    view=0;
}

// ---------- the secret classic creator ----------
// Title screen: UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A START unlocks (or locks again) the classic creature screen.
// Creator: once unlocked, pressing START and SELECT together swaps between the new creator and the classic one. Remembered in SRAM.
static int comboSS(u16 k,u16 pressed){ return (k&K_START)&&(k&K_SEL)&&(pressed&(K_START|K_SEL)); }
#define NENT (NPARTS+4)   // classic list: the parts, then AGE, GO LIVE LIFE, EDIT MAP, MAIN MENU

static int creatorNew(void){   // returns 1 when the secret code switched screens, 0 when leaving
    int tab=0, rs[NTAB]={0}, dirty=3, hold[10]={0}; u16 prev=keyNow();
    for(;;){
        u16 k=keyNow(), pressed=k&~prev; prev=k;
        for(int i=0;i<10;i++) hold[i]=(k>>i&1)?hold[i]+1:0;
        if(sUnlock&&comboSS(k,pressed)){ sClassic=1; settingsSave(); stageOn=0; return 1; }
        if(pressed&K_R){ tab=(tab+1)%NTAB; dirty|=2; }
        if(pressed&K_L){ tab=(tab+NTAB-1)%NTAB; dirty|=2; }
        if(pressed&K_DOWN){ rs[tab]=(rs[tab]+1)%tabN[tab]; dirty|=2; }
        if(pressed&K_UP){ rs[tab]=(rs[tab]+tabN[tab]-1)%tabN[tab]; dirty|=2; }
        if(pressed&K_SEL){ view=(view+1)&3; dirty=3; }
        if(pressed&K_START){ tab=TB_DONE; rs[tab]=0; dirty|=2; }
        if(pressed&K_B){ stageOn=0; return 0; }
        const Row*r=&tabRow[tab][rs[tab]];
        int d=TRIG(K_RIGHT,4)?1:TRIG(K_LEFT,5)?-1:0;
        if(r->kind==RK_ACT){
            if(pressed&K_A){
                switch(r->id){
                    case AC_BUILD: blockBuilder(); break;
                    case AC_PLAY:  lifeMode(0); if(gToMenu){ stageOn=0; return 0; } break;
                    case AC_MAP:   mapEditor(); break;
                    default:       stageOn=0; return 0;   // MAIN MENU
                }
                prev=keyNow(); for(int i=0;i<10;i++) hold[i]=0; dirty=3;
            }
        } else if(d||(pressed&K_A)){
            lookStep(r->id,r->n,d?d:1); prev=keyNow(); for(int i=0;i<10;i++) hold[i]=0; dirty=3;
        }
        if(dirty){
            if(dirty&1) drawCreatorScene();
            drawCreatorPanel(tab,rs[tab]);
            present(); dirty=0;
        } else vsync();
    }
}

IWRAM_CODE static void drawClassicPanel(void){
    fillCols(SCENE_W,ROW_W,PANEL);
    text(130,5,"BORE",RGB(31,26,6),2);
    text(130,17,"VOXEL DEMO",RGB(14,16,18),1);
    for(int i=0;i<NENT;i++){
        int y=26+i*6, go=(i>=NPARTS);
        if(i==part){ rect(128,y-1,108,6,go?RGB(16,10,2):RGB(6,16,8)); text(130,y,">",RGB(31,31,31),1); }
        text(137,y,go?(i==NPARTS?"AGE":i==NPARTS+1?"GO LIVE LIFE!":i==NPARTS+2?"EDIT MAP":"MAIN MENU"):parts[i].name,i==part?(go?RGB(31,26,6):RGB(31,31,31)):(go?RGB(24,20,6):RGB(18,20,22)),1);
        if(i==NPARTS) text(190,y,stageNm[stage],RGB(31,26,6),1);
        if(i==part&&!go&&parts[i].dk) text(190,y,spr[SPRID(parts[i].dk-1)].name,RGB(31,26,6),1);
    }
    text(130,101,"SIZE",RGB(18,20,22),1);
    const char*sn[3]={"S","M","L"};
    for(int i=0;i<3;i++){
        int x=156+i*16; rect(x,99,12,9,i==size-1?RGB(6,16,8):RGB(2,3,5));
        text(x+4,101,sn[i],i<stMaxSz[stage]?RGB(31,31,31):RGB(8,9,11),1);
    }
    text(130,113,"X",RGB(18,20,22),1); num(136,113,cx,RGB(31,31,31));
    text(148,113,"Y",RGB(18,20,22),1); num(154,113,cy,RGB(31,31,31));
    text(166,113,"Z",RGB(18,20,22),1); num(172,113,cz,(part<NPARTS&&parts[part].dk)?RGB(12,14,16):RGB(31,31,31));   // sprites ignore Z
    u16 hc=RGB(12,14,16);
    if(part==NPARTS){ text(130,123,"A OR LEFT RIGHT",RGB(31,26,6),1); text(130,131,"CHANGES THE AGE",RGB(31,26,6),1); text(130,141,"SELECT NEXT ENTRY",hc,1); }
    else if(part>=NPARTS){ text(130,123,"PRESS A TO OPEN",RGB(31,26,6),1); text(130,131,"SELECT NEXT ENTRY",hc,1); }
    else {
    text(130,123,"DPAD X Z  L R UP DN",hc,1); text(130,129,"A PLACE B ERASE",hc,1);
    text(130,135,"START SIZE  SEL PART",hc,1); text(130,141,"SEL+UP DN FACE",hc,1);
    text(130,147,"SEL+A SKIN  B HAIR",hc,1); text(130,153,"SEL+L R TURN VIEW",hc,1);
    }
}


// ---------- the classic creature screen (the original editor, kept as a secret) ----------
static int creatorClassic(void){   // returns 1 when the secret code switched screens, 0 when leaving
    stageOn=0;
    u16 prev=keyNow(); int hold[10]={0}, frame=0, dirty=1, lastBlink=-1, comboUsed=(keyNow()&K_SEL)?1:0;
    for(;;){
        u16 k=keyNow(), pressed=k&~prev, released=prev&~k; prev=k;
        for(int i=0;i<10;i++) hold[i]=(k>>i&1)?hold[i]+1:0;
        #define TRIG(m,i) ((pressed&(m))||(hold[i]>14&&(hold[i]&3)==0))
        if(sUnlock&&comboSS(k,pressed)){ sClassic=0; settingsSave(); return 1; }
        int sel=k&K_SEL;
        if(sel){
            if(pressed&K_A){ look[LK_SKIN]=(look[LK_SKIN]+1)%NSW; setColors(); comboUsed=1; dirty=1; }
            if(pressed&K_B){ look[LK_HCOL]=(look[LK_HCOL]+1)%NSW; setColors(); comboUsed=1; dirty=1; }
            if(pressed&K_R){ part=(part+1)%NENT; comboUsed=1; dirty=1; }
            if(pressed&K_L){ part=(part+NENT-1)%NENT; comboUsed=1; dirty=1; }
            if(pressed&K_RIGHT){ view=(view+1)&3; comboUsed=1; dirty=1; }
            if(pressed&K_LEFT){ view=(view+3)&3; comboUsed=1; dirty=1; }
            if(pressed&(K_UP|K_DOWN)){
                comboUsed=1;
                if(part<NPARTS&&parts[part].dk){ int kd=parts[part].dk-1; sty[kd]=(sty[kd]+((pressed&K_UP)?1:2))%3; look[LK_EYES]=sty[0]%3; look[LK_MOUTH]=sty[1]%3; dirty=1; }
            }
        } else {
            if(part==NPARTS){   // the AGE entry: LEFT / RIGHT step the life stage backwards / forwards, A steps forwards
                if(pressed&(K_LEFT|K_RIGHT|K_A)){ lookStep(LK_AGE,AG_N,(pressed&K_LEFT)?-1:1); prev=keyNow(); dirty=1; }
            } else {
            if(TRIG(K_RIGHT,4)){moveView(1,0);dirty=1;} if(TRIG(K_LEFT,5)){moveView(-1,0);dirty=1;}
            if(TRIG(K_UP,6)){moveView(0,-1);dirty=1;}     if(TRIG(K_DOWN,7)){moveView(0,1);dirty=1;}
            if(TRIG(K_R,8)){cy++;dirty=1;}      if(TRIG(K_L,9)){cy--;dirty=1;}
            }
            if((pressed&K_A)&&part!=NPARTS){
                if(part==NPARTS+1){ lifeMode(0); if(gToMenu) return 0; }
                else if(part==NPARTS+2) mapEditor();
                else if(part==NPARTS+3) return 0;   // MAIN MENU
                else { doPart(1,part,size,cx,cy,cz); custom=1; }
                prev=keyNow(); dirty=1;
            }
            if(pressed&K_B){ if(part<NPARTS){ doPart(2,part,size,cx,cy,cz); custom=1; } dirty=1; }
        }
        if(released&K_SEL){ if(!comboUsed){ part=(part+1)%NENT; dirty=1; } comboUsed=0; }
        if(pressed&K_START){ size=size%stMaxSz[stage]+1; dirty=1; }
        clampCursor();
        if(dirty) frame=16;   // restart blink with the ghost visible
        int blink=(frame>>4)&1;
        if(dirty||blink!=lastBlink){
            for(int y=0;y<H;y++)for(int z=0;z<D;z++)for(int x=0;x<W;x++){ ghost[y][z][x]=0; gdec[y][z][x]=0; }
            gAny=0;
            if(part<NPARTS){ doPart(0,part,size,cx,cy,cz);
            if(parts[part].dk&&!gAny) ghost[cy][cz][cx]=1; }   // nothing solid under the cursor: show a marker cube
            drawScene(blink); if(dirty){ drawClassicPanel(); present(); } else presentScene();   // blink-only: scene columns only
            dirty=0; lastBlink=blink;
        } else vsync();
        frame++;
    }
}


static void creatureEditor(void){ for(;;){ int sw=(sUnlock&&sClassic)?creatorClassic():creatorNew(); if(!sw) break; } }   // main menu entry: the new creator, or the classic one once the code has been entered

// ---------- jukebox ----------
// Listen to the songs in source/songs.h. The playlist (jukebox.h) is shuffled once and the shuffle is saved, so the song set
// keeps the same random order every time the game starts. SELECT re-rolls it. Audio uses the same Direct Sound B player as the
// title music. That player is refilled once per frame, so the screen only redraws small regions (one per frame, drawn one
// frame and copied to the display the next) to keep every frame short enough for the sound.
// (Music while playing the game would need a vblank interrupt, because game frames can run long: not done yet.)
#define JB_BG RGB(3,4,8)
#define JB_LY 50                 // list region starts at this row
#define JB_ROWS 8                // visible list rows, 9 px each
#define JB_LH (JB_ROWS*9+4)
static int jbPlaying, jbMsgT; static const char*jbMsg;
static void fillBox(int x0,int x1,int y0,int y1,u16 c){   // x0, x1 must be even (32-bit stores)
    u32 v=c|((u32)c<<16);
    for(int y=y0;y<y1;y++){ u32*row=(u32*)fb+y*ROW_W; for(int w=x0>>1;w<(x1>>1);w++) row[w]=v; }
}
static int numAt(int x,int y,int n,u16 c){ return numText(x,y,n,c); }
static void jbStartSlot(int slot){   // play playlist slot (remembered in SRAM so the playlist carries on after a reboot)
    jbPos=slot; jbSave();
    if(!sSnd){ jbPlaying=0; return; }
    const Song*sg=&songs[jbSong(slot)];
    musBegin(sg->adp?1:0,sg->adp,sg->xm); jbPlaying=1;
}
// Look: dark navy gradient with gold side rails, readable plain text (no jitter or fading), a segmented equalizer, a pill for the
// mode, a green NOW tag, a progress bar with a playhead, banded list rows, and key-cap hints at the bottom.
#define JB_GOLD2 RGB(14,11,3)
#define JB_GREEN RGB(14,30,12)
#define JB_TXT   RGB(22,24,28)
static u8 jbEq[12];
static void jbFill(int y0,int y1){   // gradient background + gold rails for rows y0..y1
    for(int y=y0;y<y1;y++) fillBox(0,SW,y,y+1,RGB(2+y/55,3+y/38,9+y/14));
    rect(0,y0,2,y1-y0,JB_GOLD2); rect(SW-2,y0,2,y1-y0,JB_GOLD2);
}
static void jbNote(int x,int y,u16 c){ rect(x,y+8,4,3,c); rect(x+3,y,1,9,c); rect(x+4,y,3,1,c); rect(x+6,y+1,1,3,c); }   // a little eighth note
static void jbHead(void){   // title, equalizer, mode, now playing, progress bar
    jbFill(0,JB_LY);
    text(9,6,"JUKEBOX",RGB(10,7,1),2); int tx=text(8,5,"JUKEBOX",GOLD,2);
    jbNote(tx+6,4,JB_GOLD2); jbNote(tx+14,6,GOLD);
    for(int b=0;b<12;b++){                     // segmented equalizer: green, then yellow, then red at the top
        int e=jbEq[b], t=jbPlaying?2+((rnd8()*16)>>8):1;
        e=(t>e)?t:(e>3?e-3:(e>1?e-1:1)); jbEq[b]=(u8)e;
        int x=150+b*7;
        if(!jbPlaying){ rect(x,20,5,1,RGB(8,10,16)); continue; }
        for(int k=0;k*3<e;k++){ int sh=(e-k*3>3)?2:(e-k*3>2?2:e-k*3); if(sh<1) sh=1;
            u16 c=k<3?RGB(8,26,8):k<4?RGB(28,26,5):RGB(30,9,6); rect(x,19-k*3-(sh-1),5,sh,c); }
    }
    int mx=text(8,22,"MODE",DIMC,1)+3; int vw=tw(jbModeNm[sJb],1);
    rect(mx,21,vw+7,9,RGB(14,10,2)); rect(mx,21,vw+7,1,RGB(24,19,5)); text(mx+3,22,jbModeNm[sJb],GOLD,1);
    if(jbMsgT>0) text(140,22,jbMsg,WHITE,1);
    else { int x=text(140,22,"TRACK",DIMC,1)+3; x=numAt(x,22,jbPos+1,WHITE); x=text(x+3,22,"OF",DIMC,1); numAt(x+3,22,jbN,WHITE); }
    rect(8,31,22,9,RGB(6,16,8)); text(11,32,"NOW",JB_GREEN,1);
    if(!sSnd) text(36,32,"SOUND IS OFF IN SETTINGS",RGB(30,10,8),1);
    else text(36,32,songs[jbSong(jbPos)].name,WHITE,1);
    int pct=!mOn?0:mKind?(int)(aPos/(aN/100+1)):mOrd*100/mSong->nord; if(pct>100) pct=100;
    int fw=pct*162/100; u16 gd=jbPlaying?RGB(8,22,8):RGB(10,12,18), gl=jbPlaying?RGB(14,30,12):RGB(14,16,22);
    rect(8,44,162,5,RGB(7,9,15)); rect(8,44,fw,5,gd); rect(8,44,fw,2,gl);
    if(fw>0&&jbPlaying) rect(8+fw-1,42,3,9,WHITE);
    if(jbPlaying){ for(int k=0;k<4;k++) rect(178+k,43+k,1,7-2*k,JB_GREEN); text(187,43,"PLAYING",JB_GREEN,1); }
    else { rect(178,44,5,5,RGB(14,16,22)); text(187,43,"STOPPED",RGB(14,16,22),1); }
    rect(8,49,224,1,JB_GOLD2);
}
static void jbList(int cur){   // the playlist in play order: triangle = cursor, bars = playing
    jbFill(JB_LY,JB_LY+JB_LH);
    int top=cur-JB_ROWS/2; if(top>jbN-JB_ROWS) top=jbN-JB_ROWS; if(top<0) top=0;
    for(int r=0;r<JB_ROWS&&top+r<jbN;r++){
        int slot=top+r, y=JB_LY+3+r*9, sel=(slot==cur), pl=(slot==jbPos&&jbPlaying);
        if(sel){ fillBox(6,228,y-2,y+7,RGB(7,18,9)); fillBox(8,228,y-2,y-1,RGB(12,26,13)); rect(6,y-2,2,9,GOLD); }
        else if(r&1) fillBox(6,228,y-2,y+7,RGB(5,7,16));
        if(sel){ rect(10,y,1,5,WHITE); rect(11,y+1,1,3,WHITE); rect(12,y+2,1,1,WHITE); }
        if(pl){ rect(15,y+3,1,3,GOLD); rect(17,y+1,1,5,GOLD); rect(19,y+4,1,2,GOLD); }
        numAt(24,y,slot+1,sel?GOLD:RGB(14,16,20));
        text(38,y,songs[jbSong(slot)].name,sel?WHITE:pl?JB_GREEN:JB_TXT,1);
    }
    if(jbN>JB_ROWS){   // scroll bar
        int th=JB_ROWS*9*JB_ROWS/jbN, ty=JB_LY+3+(JB_ROWS*9-th)*top/(jbN-JB_ROWS);
        rect(233,JB_LY+3,2,JB_ROWS*9,RGB(8,10,16)); rect(233,ty,2,th,GOLD);
    }
}
static int jbKey(int x,int y,const char*k){   // a little key cap, returns the x after it
    int w=tw(k,1)+5; rect(x,y-2,w,10,RGB(22,18,5)); rect(x+1,y-1,w-2,8,RGB(8,9,15)); text(x+3,y,k,GOLD,1); return x+w+2;
}
static int jbLab(int x,int y,const char*t){ return text(x,y,t,RGB(20,22,26),1)+7; }
static void jbFoot(void){
    int x=8; x=jbKey(x,130,"UP"); x=jbKey(x,130,"DOWN"); x=jbLab(x,130,"PICK"); x=jbKey(x,130,"A"); x=jbLab(x,130,"PLAY");
    x=jbKey(x,130,"L"); x=jbKey(x,130,"R"); jbLab(x,130,"PREV NEXT");
    x=8; x=jbKey(x,140,"START"); x=jbLab(x,140,"STOP OR PLAY"); x=jbKey(x,140,"SELECT"); jbLab(x,140,"RESHUFFLE");
    x=8; x=jbKey(x,150,"LEFT"); x=jbKey(x,150,"RIGHT"); x=jbLab(x,150,"MODE"); x=jbKey(x,150,"B"); jbLab(x,150,"BACK");
}
static void jukeboxScreen(void){
    int cur=jbPos, dH=1, dL=1, pend=0, fr=0; u16 prev=keyNow();
    jbMsgT=0; jbPlaying=0;
    if(sSnd) jbStartSlot(jbPos);                          // opening the jukebox starts the song the playlist is on
    jbFill(0,SH);
    jbHead(); jbList(cur); jbFoot();
    vsync(); dmaRows(fb,VRAM_ADDR,0,ROW_W,0,SH); dH=dL=0;
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k; fr++;
        if(pr&K_B) break;
        if(pr&K_DOWN){ cur=(cur+1)%jbN; dL=1; }
        if(pr&K_UP){ cur=(cur+jbN-1)%jbN; dL=1; }
        if(pr&K_A){ jbStartSlot(cur); dH=dL=1; }
        if(pr&K_R){ cur=(jbPos+1)%jbN; jbStartSlot(cur); dH=dL=1; }
        if(pr&K_L){ cur=(jbPos+jbN-1)%jbN; jbStartSlot(cur); dH=dL=1; }
        if(pr&K_START){ if(jbPlaying){ musStop(); jbPlaying=0; } else jbStartSlot(cur); dH=dL=1; }
        if(pr&(K_LEFT|K_RIGHT)){ int cs=jbSong(cur); jbSetMode((sJb+((pr&K_RIGHT)?1:2))%3); cur=jbSlotOf(cs); dH=dL=1; }
        if(pr&K_SEL){ lrng^=(u32)fr*2654435761u; jbReshuffle(); cur=jbPos; jbMsg="NEW ORDER SAVED"; jbMsgT=90; dH=dL=1; }
        if(jbPlaying&&(mKind?mDone:mLaps>=1)){ jbStartSlot(sJb==2?jbPos:(jbPos+1)%jbN); cur=jbPos; dH=dL=1; }   // song over: next one
        if(jbMsgT>0&&--jbMsgT==0) dH=1;
        if(jbPlaying&&(fr&3)==0) dH=1;                    // bar + equalizer
        int drew=0;   // draw at most ONE region per frame into fb (only when none is waiting to be copied)...
        if(!pend){ if(dH){ jbHead(); drew=1; dH=0; } else if(dL){ jbList(cur); drew=2; dL=0; } }
        vsync(); musKick();
        if(pend==1) dmaRows(fb,VRAM_ADDR,0,ROW_W,0,JB_LY);                  // ...and copy it to the screen the frame after
        else if(pend==2) dmaRows(fb,VRAM_ADDR,0,ROW_W,JB_LY,JB_LY+JB_LH);
        pend=drew;
        musFill();
    }
    musStop(); jbPlaying=0; settingsSave();
    while(keyNow()) vsync();
}

// ---------- main menu ----------
static const char* const mmName[7]={"PLAY","MAKE CREATURE","BUILD ROOM","ROOM SLOTS","JUKEBOX","OPTIONS","HOW TO PLAY"};
static const char* const mmDesc[7]={"WALK AND SKATE AROUND YOUR ROOM","DESIGN YOUR OWN VOXEL CHARACTER","BUILD WALLS AND LAY FLOORS AND WALLPAPER","SAVE AND LOAD ROOMS  PEOPLE AND LIVES","LISTEN  PICK  OR SHUFFLE THE SONGS","SPEED  GAMEPLAY  SOUND  BUTTONS AND MORE","LEARN THE CONTROLS"};
static const char* const guideItems[6]={"PLAYING","MAKE CREATURE","BUILD ROOMS","JUKEBOX","ROOM SLOTS","OPTIONS"};
static const char* const jbHelp[10]={">LISTEN","UP DOWN PICK A SONG  A PLAY IT","L R PREVIOUS OR NEXT SONG","START STOPS OR PLAYS AGAIN",">ORDER","LEFT RIGHT SHUFFLE  IN ORDER  REPEAT ONE","SELECT MAKES A NEW SHUFFLE  SAVED","THE SHUFFLE STAYS THE SAME EVERY BOOT",">LEAVE","B GOES BACK TO THE MENU"};
static void drawMainMenu(int sel){
    for(int y=0;y<SH;y++){ u16 c=RGB(2+y/50,3+y/36,9+y/13); u32 v=c|((u32)c<<16), *row=(u32*)fb+y*ROW_W; for(int w=0;w<ROW_W;w++) row[w]=v; }
    u16 ink=RGB(4,3,6);
    for(int dy=-2;dy<=2;dy+=2)for(int dx=-2;dx<=2;dx+=2) text(14+dx,8+dy,"BORE",ink,5);
    text(14,8,"BORE",GOLD,5);
    text(16,38,"A VOXEL LIFE SIM",RGB(12,28,8),1);
    // a little pile of voxels (back to front)
    { int ox=206, oy=92;
      cube(ox,oy,1,0,1); cube(ox,oy-CC,4,0,3); cube(ox,oy-2*CC,8,0,2);
      cube(ox+CA,oy+CB,6,0,0); cube(ox-CA,oy+CB,7,0,0); cube(ox,oy+2*CB,2,0,0); }
    for(int i=0;i<7;i++){
        int y=47+i*12;
        if(i==sel){ rect(10,y-2,150,12,RGB(6,16,8)); rect(10,y-2,2,12,GOLD); text(16,y,">",WHITE,2); }
        text(28,y,mmName[i],i==sel?WHITE:DIMC,2);
    }
    rect(0,134,SW,26,PANEL);
    text(8,139,mmDesc[sel],WHITE,1); text(8,150,"UP DOWN CHOOSE  A OK",RGB(12,14,16),1);
}
static void mainMenu(void){
    int sel=0, dirty=1; u16 prev=keyNow();
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k;
        if(pr&K_DOWN){ sel=(sel+1)%7; dirty=1; }
        if(pr&K_UP){ sel=(sel+6)%7; dirty=1; }
        if(pr&(K_A|K_START)){
            if(sel==0) lifeMode(0);
            else if(sel==1) creatureEditor();
            else if(sel==2) mapEditor();
            else if(sel==3) slotScreen();
            else if(sel==4) jukeboxScreen();
            else if(sel==5) settingsScreen();
            else { int g=menu("HOW TO PLAY",guideItems,6);
                   if(g==0) helpScreen("PLAYING",lifeHelp,16); else if(g==1) helpScreen("MAKE CREATURE",creatureHelp,15); else if(g==2) helpScreen("BUILD ROOMS",mapHelp,12); else if(g==3) helpScreen("JUKEBOX",jbHelp,10); else if(g==4) helpScreen("ROOM SLOTS",slotHelp,11); else if(g==5) helpScreen("OPTIONS",optHelp,11); }
            gToMenu=0; prev=keyNow(); dirty=1; continue;
        }
        if(dirty){ drawMainMenu(sel); present(); dirty=0; } else vsync();
    }
}

int main(void){
    REG_WAITCNT=0x4317;  // ROM 3/1 waits + prefetch (power-on default is 4/2, no prefetch)
    REG_DISPCNT=0x0403;  // mode 3, BG2 on
    initTables(); setColors(); settingsLoad(); optsLoad(); applyRom();
    lrng^=(u32)titleScreen()*2654435761u;   // time spent on the title seeds the random numbers (first shuffle)
    if(konMsg) toast(konMsg==2?"CLASSIC CREATOR UNLOCKED":"CLASSIC CREATOR LOCKED");
    jbSetup();                              // load the saved shuffled order (or make a new one), placeholders hidden
    starter();
    mapReset(); mapLoad();   // default room, or the one saved to SRAM
    slotBoot();              // BOOT LOADS PERSON option: the creature of the active room slot
    ageLoad();               // ...grown to the stage it had reached
    mainMenu();
    return 0;
}

