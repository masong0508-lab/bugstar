// BUGSTAR prototype: a pill bug on a turning planet, and a colour 3D dungeon underneath.
#include "assets.h"
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef signed short s16; typedef signed char s8; typedef int s32;
#define REG_DISPCNT  (*(volatile u16*)0x04000000)
#define REG_VCOUNT   (*(volatile u16*)0x04000006)
#define REG_BG2CNT   (*(volatile u16*)0x0400000C)
#define REG_BG2PA    (*(volatile s16*)0x04000020)
#define REG_BG2PD    (*(volatile s16*)0x04000026)
#define REG_BLDCNT   (*(volatile u16*)0x04000050)
#define REG_BLDY     (*(volatile u16*)0x04000054)
#define REG_KEYINPUT (*(volatile u16*)0x04000130)
#define REG_WAITCNT  (*(volatile u16*)0x04000204)
#define BG_PAL  ((volatile u16*)0x05000000)
#define OBJ_PAL ((volatile u16*)0x05000200)
#define VRAM    ((volatile u16*)0x06000000)
#define OBJ_VRAM ((volatile u16*)0x06014000)
#define OAM     ((volatile u16*)0x07000000)
#define K_A 1
#define K_B 2
#define K_SEL 4
#define K_START 8
#define K_RIGHT 16
#define K_LEFT 32
#define K_UP 64
#define K_DOWN 128
#define K_R 256
#define K_L 512
#define IWRAM __attribute__((section(".iwram"), long_call, target("arm"), noinline))

void *memset(void *d, int c, unsigned n){ u8 *p=d; while(n--) *p++=c; return d; }
void *memcpy(void *d, const void *s, unsigned n){ u8 *p=d; const u8 *q=s; while(n--) *p++=*q++; return d; }

static u32 keys, keys_old;
static u32 rng_s = 0x1234567;
static u32 rnd(void){ rng_s ^= rng_s<<13; rng_s ^= rng_s>>17; rng_s ^= rng_s<<5; return rng_s; }
static int rndi(int n){ return (int)(rnd()%(u32)n); }
static void read_keys(void){ keys_old=keys; keys = ~REG_KEYINPUT & 0x3FF; }
#define HELD(k) (keys&(k))
#define HIT(k) ((keys&(k)) && !(keys_old&(k)))
static void vsync(void){ while(REG_VCOUNT>=160); while(REG_VCOUNT<160); }
static int isqrt(u32 n){ u32 r=0, b=1u<<30; while(b>n) b>>=2; while(b){ if(n>=r+b){ n-=r+b; r=(r>>1)+b; } else r>>=1; b>>=2; } return (int)r; }
static int sinx(int a){ return sin_tab[a&255]; }
static int cosx(int a){ return sin_tab[(a+64)&255]; }

// ---------------------------------------------------------------- shared state
static int hearts=5, gold=0, crew=0, frames=0, planet_no=0, depth_no=0, invuln=0;
static volatile u16 *page;         // the page we draw into (mode 4)
static int page_back=0;

// ---------------------------------------------------------------- mode 4 text
static void pix(volatile u16 *pg, int x, int y, int c){
    volatile u16 *p = pg + ((y*240+x)>>1);
    if(x&1) *p = (*p&0x00FF)|(c<<8); else *p = (*p&0xFF00)|c;
}
static void rect(volatile u16 *pg, int x, int y, int w, int h, int c){
    for(int j=0;j<h;j++){ int i=0;
        if(x&1){ pix(pg,x,y+j,c); i=1; }
        volatile u16 *p = pg + (((y+j)*240+x+i)>>1); u16 cc=c|(c<<8);
        for(; i+1<w; i+=2) *p++=cc;
        if(i<w) pix(pg,x+i,y+j,c); }
}
static int text(volatile u16 *pg, int x, int y, const char *s, int c, int sc){
    for(;*s;s++){
        u16 g = font[(u8)*s & 127];
        for(int k=0;k<15;k++) if(g & (1<<(14-k))) rect(pg, x+(k%3)*sc, y+(k/3)*sc, sc, sc, c);
        x += 4*sc;
    }
    return x;
}
static void num(char *b, int v, int digits){ for(int i=digits-1;i>=0;i--){ b[i]='0'+v%10; v/=10; } b[digits]=0; }
static int textlen(const char *s){ int n=0; while(*s++) n++; return n; }

// ---------------------------------------------------------------- fades
static void fade(int out){
    REG_BLDCNT = 0x3F | (3<<6);
    for(int i=0;i<=16;i++){ REG_BLDY = out? i : 16-i; vsync(); }
    if(!out) REG_BLDCNT = 0;
}

// ---------------------------------------------------------------- OAM helpers
typedef struct { u16 a0,a1,a2,pad; } Obj;
static Obj shadow_oam[128];
static void oam_hide_all(void){ for(int i=0;i<128;i++){ shadow_oam[i].a0 = 0x0200; shadow_oam[i].a1=0; shadow_oam[i].a2=0; } }
static void oam_flush(void){ volatile u16 *o=OAM; for(int i=0;i<128;i++){ o[i*4]=shadow_oam[i].a0; o[i*4+1]=shadow_oam[i].a1; o[i*4+2]=shadow_oam[i].a2; } }
static void set_affine(int m, int pa, int pb, int pc, int pd){ volatile u16 *o=OAM; o[m*16+3]=pa; o[m*16+7]=pb; o[m*16+11]=pc; o[m*16+15]=pd; }

#include "game.inc"

// ================================================================= THE PLANET
enum { O_DOOR, O_SHIP, O_CAGE, O_SLUG, O_PLANT, O_PEBBLE, O_TUFT };
typedef struct { s32 x,y,z; u8 kind, alive; } PObj;
#define NPO 110
static PObj po[NPO];
static const s32 P0x=-565, P0y=-10027, P0z=12943;      // where the bug stands on the ball (2.14)
static const s32 AXx=0, AXy=12945, AXz=10027;          // axis for walking sideways
static int facing=1, msg_timer=0;
#define cur_tint zone
static char msg[40];

static void rot_axis(PObj *o, s32 ax, s32 ay, s32 az, int a){   // Rodrigues, 2.14 fixed
    s32 c=cosx(a), s=sinx(a);
    s32 dot=(ax*o->x + ay*o->y + az*o->z)>>14;
    s32 cx_=(ay*o->z - az*o->y)>>14, cy_=(az*o->x - ax*o->z)>>14, cz_=(ax*o->y - ay*o->x)>>14;
    s32 k=((16384-c)*dot)>>14;
    s32 nx=((o->x*c)>>14) + ((cx_*s)>>14) + ((ax*k)>>14);
    s32 ny=((o->y*c)>>14) + ((cy_*s)>>14) + ((ay*k)>>14);
    s32 nz=((o->z*c)>>14) + ((cz_*s)>>14) + ((az*k)>>14);
    o->x=nx; o->y=ny; o->z=nz;
}
static void renorm(PObj *o){ s32 l=isqrt((u32)(o->x*o->x + o->y*o->y + o->z*o->z)); if(l>0){ o->x=o->x*16384/l; o->y=o->y*16384/l; o->z=o->z*16384/l; } }
static void rand_dir(PObj *o){
    do { o->x=rndi(32768)-16384; o->y=rndi(32768)-16384; o->z=rndi(32768)-16384; } while(o->x*o->x+o->y*o->y+o->z*o->z > 16384*16384 || o->x*o->x+o->y*o->y+o->z*o->z < 2000*2000);
    renorm(o);
}
static s32 dot_p0(PObj *o){ return (o->x*P0x + o->y*P0y + o->z*P0z)>>14; }

static void planet_palette(void){
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    for(int i=0;i<6;i++) BG_PAL[16+i]= endless ? w_tint[i] : tint_pal[zone*6+i];
    for(int i=0;i<16;i++) OBJ_PAL[i]=obj_pal[i];
}
static void planet_draw_bg(void){
    volatile u16 *pg=VRAM; const u16 *src=(const u16*)planet_img;
    for(int i=0;i<240*160/2;i++) pg[i]=src[i];
}
static void planet_hud(void){
    char b[24]; scpy(b,world_name()); if(zone_cleared && !endless) scat(b," *");
    draw_hud(VRAM, b);
}
static void show_msg(const char *s){
    volatile u16 *pg=VRAM; const u16 *src=(const u16*)planet_img;
    for(int i=22*120;i<34*120;i++) pg[i]=src[i];
    int n=0; while(s[n] && n<39){ msg[n]=s[n]; n++; } msg[n]=0;
    if(n){ text(pg,120-n*4,24,msg,C_WHITE,2); msg_timer=150; }
}
static void new_planet(void){
    int k=0;
    po[k].kind=O_SHIP; po[k].x=P0x; po[k].y=P0y; po[k].z=P0z; po[k].alive=1;
    rot_axis(&po[k],AXx,AXy,AXz,-6); k++;                                  // ship parked just left of you
    po[k].kind=O_DOOR; po[k].alive=1; po[k].x=P0x; po[k].y=P0y; po[k].z=P0z; rot_axis(&po[k],16384,0,0,30); rot_axis(&po[k],AXx,AXy,AXz,8); k++;
    for(int i=0;i<2;i++){ po[k].kind=O_CAGE; po[k].alive=1; rand_dir(&po[k]); k++; }
    for(int i=0;i<2;i++){ po[k].kind=O_SLUG; po[k].alive=1; do rand_dir(&po[k]); while(dot_p0(&po[k])>8000); k++; }
    for(int i=0;i<30;i++){ po[k].kind=O_PLANT; po[k].alive=1; rand_dir(&po[k]); k++; }
    while(k<NPO){ po[k].kind = (k&1)?O_PEBBLE:O_TUFT; po[k].alive=1; rand_dir(&po[k]); k++; }
}
static const u16 tile_of[]={T_DOOR,T_SHIP,T_CAGE,T_SLUG,T_PLANT,T_PEBBLE,T_TUFT};
static const u8 shape_of[]={0,0,1,1,0,0,0}, size_of[]={1,1,0,0,0,0,0}, w_of[]={16,16,16,16,8,8,8}, h_of[]={16,16,8,8,8,8,8};

static int enter_dungeon_flag=0;
static void planet_frame(void){
    int moved=0;
    if(HELD(K_LEFT)){ for(int i=0;i<NPO;i++) rot_axis(&po[i],AXx,AXy,AXz,1); facing=0; moved=1; }
    if(HELD(K_RIGHT)){ for(int i=0;i<NPO;i++) rot_axis(&po[i],AXx,AXy,AXz,-1); facing=1; moved=1; }
    if(HELD(K_UP)){ for(int i=0;i<NPO;i++) rot_axis(&po[i],16384,0,0,-1); moved=1; }
    if(HELD(K_DOWN)){ for(int i=0;i<NPO;i++) rot_axis(&po[i],16384,0,0,1); moved=1; }
    renorm(&po[frames%NPO]);
    // slugs crawl toward you
    for(int i=0;i<NPO;i++) if(po[i].kind==O_SLUG && po[i].alive){
        PObj *o=&po[i]; if(dot_p0(o)>9000){ o->x+=(P0x-o->x)>>7; o->y+=(P0y-o->y)>>7; o->z+=(P0z-o->z)>>7; renorm(o); }
        if(dot_p0(o)>16300 && !invuln){ hurt(1); if(invuln>30) show_msg("OUCH! A SLUG"); }
    }
    // A: use what is next to you
    if(HIT(K_A)){
        int best=-1; s32 bd=16150;
        for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind<=O_SLUG){ s32 d=dot_p0(&po[i]); if(d>bd){ bd=d; best=i; } }
        if(best<0) show_msg("NOTHING HERE");
        else switch(po[best].kind){
            case O_DOOR: enter_dungeon_flag=1; break;
            case O_SHIP:
                if(!shop_menu()){ planet_draw_bg(); planet_hud(); if(!zone_cleared) show_msg("BEAT THE BOSS BELOW TO FLY ON"); return; }
                if(endless){ world_no++; make_world(); }
                else if(zone==NTINT-1){ endless=1; world_no=1; galaxy_seed=rnd()|1; make_world(); }
                else { zone++; zone_cleared=0; }
                fade(1); new_planet(); planet_palette(); planet_draw_bg(); planet_hud();
                show_msg(endless && world_no==1 ? "THE ENDLESS GALAXY OPENS" : world_name()); fade(0); return;
            case O_CAGE: po[best].alive=0; free_bug(rndi(8)<2?BUG_PILL:rndi(6)); break;
            case O_SLUG: po[best].alive=0; gold+=5; show_msg("SQUASH! +5 GOLD"); gain_xp(2); break;
        }
    }
    bag_input(); tick_timers(); hunger_tick(240);
    if(HIT(K_START)){ stats_screen(VRAM); planet_draw_bg(); planet_hud(); msg_timer=0; return; }
    escape_flag=call_flag=blast_flag=0;
    if(hearts<=0){ hearts=maxh; food=60; gold/=2; bag_n=0; bag_sel=0; show_msg("YOU FAINTED. YOU LOST YOUR BAG"); planet_hud(); }
    if(msg_timer && --msg_timer==0) show_msg("");
    if(frames%60==0) planet_hud();
    // sprites: project, scale, sort by height on screen
    static u8 order[NPO]; static s16 sy_[NPO]; int n=0;
    for(int i=0;i<NPO;i++){ if(!po[i].alive || po[i].z<300) continue; s32 sy=292+((232*po[i].y)>>14); if(sy>170) continue; sy_[i]=sy; order[n++]=i; }
    for(int i=1;i<n;i++){ u8 t=order[i]; int j=i; while(j>0 && sy_[order[j-1]]<sy_[t]){ order[j]=order[j-1]; j--; } order[j]=t; }
    for(int m=0;m<30;m++){ int s=40+m*7; int inv=16384/s; set_affine(m, inv, 0, 0, inv); }   // scale s/64 .. (pa=256*64/s)
    oam_hide_all();
    // the player, always in front
    int bob = moved ? ((frames>>3)&1) : 0;
    int pvis = !(invuln && (frames&4));
    set_affine(31, facing? -128:128, 0, 0, 128);        // 2x, flipped when facing right
    if(pvis){ shadow_oam[0].a0 = ((150-16-16-bob)&255) | (1<<8)|(1<<9) | (1<<14);
              shadow_oam[0].a1 = ((120-32)&511) | (31<<9) | (2<<14);
              shadow_oam[0].a2 = T_BUGSIDE; }
    int slot=1;
    for(int k=0;k<n && slot<128;k++){
        PObj *o=&po[order[k]];
        int sx=128+((232*o->x)>>14), sy=sy_[order[k]];
        int sc=26+((o->z*102)>>14);                      // scale in 1/64: rim small, front big
        int m=(sc-40)/7; if(m<0) m=0; if(m>29) m=29; sc=40+m*7;
        int w=w_of[o->kind], h=h_of[o->kind];
        int cy=sy - (sc*h)/128;                           // centre so the bottom touches the ground
        int ox=sx-w, oy=cy-h;
        if(ox<-64||ox>240||oy<-64||oy>160) continue;
        shadow_oam[slot].a0 = (oy&255) | (1<<8)|(1<<9) | (shape_of[o->kind]<<14);
        shadow_oam[slot].a1 = (ox&511) | (m<<9) | (size_of[o->kind]<<14);
        shadow_oam[slot].a2 = tile_of[o->kind];
        slot++;
    }
}
static void planet_enter(void){
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    planet_palette(); planet_draw_bg(); planet_hud();
    volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i];
}

#include "dungeon.inc"

static void hud_now(void){ if(scene==0) planet_hud(); else dungeon_hud(); }
static void say(const char *t){
    if(scene==0){ show_msg(t); return; }
    int n=0; while(t[n] && n<39){ msg[n]=t[n]; n++; } msg[n]=0; msg_timer=45;
}
static void compass(volatile u16 *pg, s32 dirx, s32 diry){
    s32 dx=tgt_x-px_, dy=tgt_y-py_;
    s32 f=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16), r=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16);
    // r>0 means to the right on screen
    int ax= r>0 ? 1 : -1, ay= f>0 ? -1 : 1;
    s32 af=f<0?-f:f, ar=r<0?-r:r;
    if(ar*5<af*2) ax=0; else if(af*5<ar*2) ay=0;
    int cx=112, cy=30;          // in 2-pixel columns
    for(int k=-3;k<=3;k++){ int x=cx+ax*k, y=cy+ay*k*2; for(int j=0;j<2;j++) pg[(y+j)*120+x]=C_CYAN|(C_CYAN<<8); }
    int hx=cx+ax*4, hy=cy+ay*8; for(int j=-2;j<=2;j++) for(int i=-1;i<=1;i++){ int y=hy+j, x=hx+i; if(x>=0&&x<120&&y>=18&&y<160) pg[y*120+x]=C_WHITE|(C_WHITE<<8); }
}

static void bar(volatile u16 *pg, int c0, int val, int max, int col){
    int h=34, f = max>0 ? val*h/max : 0; if(f>h) f=h; if(f<0) f=0;
    for(int y=0;y<h+2;y++){ int yy=124+y; u16 e=6|(6<<8); pg[yy*120+c0]=e; pg[yy*120+c0+2]=e; }
    for(int y=0;y<h;y++){ int yy=125+y; int ci = (h-y<=f) ? col : 0; pg[yy*120+c0+1]=ci|(ci<<8); }
}
static void hud3d(volatile u16 *pg){
    bar(pg,1,hearts,maxh,C_RED); bar(pg,4,food,100,C_GREEN); bar(pg,7,xp,xp_next(),C_CYAN);
    int c0=86, c1=117, y0=146, y1=157;
    u16 rim=C_GOLD|(C_GOLD<<8), in=(R_CHEST+3)|((R_CHEST+3)<<8), dk=(R_CHEST+1)|((R_CHEST+1)<<8);
    for(int c=c0;c<=c1;c++){ pg[y0*120+c]=rim; pg[y1*120+c]=rim; for(int y=y0+1;y<y1;y++) pg[y*120+c]=in; }
    for(int y=y0;y<=y1;y++){ pg[y*120+c0]=rim; pg[y*120+c1]=rim; }
    static const char *const lab[8]={"E","","S","","W","","N",""};
    for(int k=0;k<8;k++){ int a=k*128, off=((a-pang+512)&1023)-512; if(off<-232||off>232) continue;
        int x=204+off*28/232;
        if(lab[k][0]) text(pg,x-1,y0+3,lab[k],0,1); else for(int y=y0+4;y<y0+8;y++) pix(pg,x,y,R_CHEST+1); }
    for(int y=y0+1;y<y1;y++) pix(pg,204,y,C_RED);
    (void)dk;
}
static int dungeon_frame_(void);
static int dungeon_frame(void){ u32 t=TMR(); int r=dungeon_frame_(); prof[5]=TMR()-t; return r; }
static int dungeon_frame_(void){   // returns 1 to go back up, 2 when you faint
    tick_timers(); hunger_tick(40); bag_input();
    { int f=frames; int fl=(sin_tab[(f*29)&255]+sin_tab[(f*47+80)&255])>>12;      // -8..8
      for(int i=0;i<8;i++){ int k=i+(fl>2?1:fl<-2?-1:0); if(k<0)k=0; if(k>7)k=7; BG_PAL[R_FLAME+i]=bg_pal[R_FLAME+k]; }
      for(int i=2;i<7;i++){ int k=i+((((f>>2)+i)&3)==0); BG_PAL[R_WATER+i]=bg_pal[R_WATER+(k>7?7:k)]; BG_PAL[R_CRYSTAL+i]=bg_pal[R_CRYSTAL+((((f>>3)+i*3)%5)==0 && i<6 ? i+1 : i)]; } }
    if(HIT(K_START)){ stats_screen((volatile u16*)(0x06000000 + (page_back?0:0xA000))); dungeon_hud(); }
    int kl=K_LEFT, kr=K_RIGHT, ku=K_UP, kd=K_DOWN;
    if(dizzy_t){ kl=K_RIGHT; kr=K_LEFT; ku=K_DOWN; kd=K_UP; }
    if(HELD(kl)) pang=(pang-12)&1023;
    if(HELD(kr)) pang=(pang+12)&1023;
    s32 dirx=cosx(pang>>2)<<2, diry=sinx(pang>>2)<<2;                 // 16.16
    s32 plx=-(diry*43/65), ply=dirx*43/65;                             // camera plane
    s32 sp = HELD(ku) ? 0x0D00 : HELD(kd) ? -0x0900 : 0;
    if(have_bug[BUG_CRICKET]) sp=sp*5/4;
    if(speed_t) sp=sp*8/5;
    if(sp){
        s32 nx=px_+(s32)(((long long)dirx*sp)>>16), ny=py_+(s32)(((long long)diry*sp)>>16);
        s32 mx = nx + (sp>0? (dirx>0?0x4000:-0x4000) : (dirx>0?-0x4000:0x4000));
        s32 my = ny + (sp>0? (diry>0?0x4000:-0x4000) : (diry>0?-0x4000:0x4000));
        if(!solid(mx,py_)) px_=nx;
        if(!solid(px_,my)) py_=ny;
    }
    if(atk_t) atk_t--;
    if(escape_flag){ escape_flag=0; return 3; }
    // monsters, pickups, cages
    int near_cage=-1;
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; if(o->flash) o->flash--;
        s32 dx=px_-o->x, dy=py_-o->y; s32 adx=dx<0?-dx:dx, ady=dy<0?-dy:dy; s32 d = adx>ady ? adx+ady/2 : ady+adx/2;
        if(o->kind==K_SPIDER || o->kind==K_BOSS || o->kind==K_BAT){
            int boss=o->kind==K_BOSS, bat=o->kind==K_BAT; s32 reach = boss?0x14000:0xB000;
            if(blast_flag && d<0x30000){ o->hp-=3; o->flash=8; }
            if(!stun_t && d<0x70000 && d>reach-0x2000){ s32 st=boss?0x0500:bat?(0x0700+((frames+i*9)%24<12?0x400:0)):0x0600; s32 stx=(dx>0?st:-st), sty=(dy>0?st:-st);
                if(!solid(o->x+stx*3,o->y)) o->x+=stx; if(!solid(o->x,o->y+sty*3)) o->y+=sty; }
            if(o->cool) o->cool--;
            if(!stun_t && d<reach && !o->cool){ o->cool=boss?40:50; if(!invuln){ hurt(boss?2:1); if(invuln>=40) hurt_flash=8; }
                if(have_bug[BUG_BEE]){ o->hp--; o->flash=8; } }
            if(o->hp<=0){ o->alive=0; earn(boss ? 100+depth_no*10 : 8+dlv()*4);
                if(boss){ if(endless) say("GUARDIAN BEATEN!"); else { zone_cleared=1; say("BOSS BEATEN! ZONE CLEARED"); } gain_xp(30+dlv()*10); }
                else { gain_xp(2+dlv()+depth_no/2);
                       if(rndi(3)==0){ int r=rndi(10), it = r<5?IT_BERRY : r<7?IT_CRUMB : r<9?IT_SEED : IT_THORN;
                           o->kind=K_ITEM; o->alive=1; o->sub=(it<<4)|(it==IT_BERRY?rndi(8):it==IT_SEED?rndi(6):it==IT_THORN?rndi(4):0); } }
                hud_now(); }
        } else if(o->kind==K_CHEST){ if(d<0x9000){ o->alive=0; earn(10+rndi(20)+dlv()*8+depth_no*3); say("TREASURE!"); if(endless && rndi(2)) add_item(IT_GEM,rndi(4)); else if(rndi(2)) add_item(IT_SEED+rndi(3),rndi(4)); hud_now(); } }
        else if(o->kind==K_ITEM){ if(d<0x9000 && bag_n<8){ o->alive=0; add_item(o->sub>>4,o->sub&15); } }
        else if(o->kind==K_CAGE){ if(d<0xC000) near_cage=i; }
    }
    blast_flag=0;
    if(call_flag){ call_flag=0; for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].kind==K_CAGE){ dob[i].alive=0; free_bug(dob[i].sub); break; } }
    // A: free a bug, or bite whatever is in front of you
    int here=dmap[(py_>>16)*MW+(px_>>16)];
    if(HIT(K_A)){
        if(near_cage>=0){ dob[near_cage].alive=0; free_bug(dob[near_cage].sub); }
        else if(here==D_UP) return 1;
        else if(here==D_DOWN){ fade(1); dungeon_enter(); fade(0); return 0; }
        else if(!atk_t){
            atk_t=14; int pw=bite_power();
            for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||(o->kind!=K_SPIDER&&o->kind!=K_BOSS&&o->kind!=K_BAT)) continue;
                s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16);
                s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(side<0) side=-side;
                s32 rng_=o->kind==K_BOSS?0x1C000:0x14000;
                if(fwd>0 && fwd<rng_ && side<0xC000){ o->flash=8; if(o->kind!=K_BOSS){ o->x+=dirx>>2; o->y+=diry>>2; } o->hp-=pw; }
            }
        }
    }
    if(hearts<=0) return 2;
    // draw into the back page
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
    render_view(pg,dirx,diry,plx,ply);
    static u8 ord[NDO]; static s32 dd[NDO]; int n=0;
    for(int i=0;i<ndo;i++) if(dob[i].alive){ s32 dx=dob[i].x-px_, dy=dob[i].y-py_; dd[i]=(dx>>8)*(dx>>8)+(dy>>8)*(dy>>8); ord[n++]=i; }
    for(int i=1;i<n;i++){ u8 t=ord[i]; int j=i; while(j>0 && dd[ord[j-1]]<dd[t]){ ord[j]=ord[j-1]; j--; } ord[j]=t; }
    for(int i=0;i<n;i++) draw_sprite(pg,&dob[ord[i]],dirx,diry,plx,ply);
    if(sight || have_bug[BUG_ANT]) compass(pg,dirx,diry);
    hud3d(pg);
    if(msg_timer){ msg_timer--; text(pg,120-textlen(msg)*4,24,msg,C_WHITE,2); }
    else if(near_cage>=0) text(pg,120-14*4,24,"A: OPEN CAGE",C_WHITE,2);
    else if(here==D_UP||here==D_DOWN){ const char *m = here==D_UP? "A: CLIMB UP" : "A: GO DEEPER"; text(pg,120-textlen(m)*4,24,m,C_WHITE,2); }
    if(hurt_flash){ hurt_flash--; for(int y=18;y<22;y++) for(int x=0;x<120;x++) pg[y*120+x]=C_RED|(C_RED<<8); }
    if(frames%60==0) dungeon_hud();
    oam_hide_all();
    int bob = (sp ? ((frames>>3)&1) : 0) + (atk_t>7 ? 6 : 0);
    if(!(invuln && (frames&4))){
        shadow_oam[0].a0 = ((98-bob)&255);
        shadow_oam[0].a1 = ((120-32)&511) | (3<<14);
        shadow_oam[0].a2 = T_BACK;
    }
    return 0;
}

// ================================================================= main loop
int main(void){
    REG_DISPCNT = 0x80;
    REG_WAITCNT = 0x4317;
    *(volatile u16*)0x04000100=0; *(volatile u16*)0x04000104=0; *(volatile u16*)0x04000106=0x84; *(volatile u16*)0x04000102=0x80;
    rng_s ^= 0x9E3779B9;
    new_game();
    planet_enter(); new_planet(); show_msg("THE GARDEN: FIND THE DOOR");
    oam_hide_all(); oam_flush();
    scene=0;
    while(1){
        read_keys(); frames++; if(invuln) invuln--;
        rng_s += keys;
        if(scene==0){
            planet_frame();
            vsync(); oam_flush();
            if(enter_dungeon_flag){ enter_dungeon_flag=0; fade(1); oam_hide_all(); oam_flush(); depth_no=0; scene=1; dungeon_enter(); page_back=1; fade(0); }
        } else {
            int r=dungeon_frame();
            vsync();
            if(scene==1){ REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1; }
            oam_flush();
            if(r){ fade(1); oam_hide_all(); oam_flush(); scene=0; msg_timer=0;
                   char mb[40];
                   if(r==2){ hearts=maxh; food=60; if(!endless) gold/=2; loot=0; bag_n=0; bag_sel=0; scpy(mb, endless ? "YOU FAINTED. LOOT AND BAG LOST" : "YOU FAINTED. YOU LOST YOUR BAG"); }
                   else if(endless){ int bonus = loot*(100+depth_no*15)/100; gold+=bonus; loot=0; char n2[6]; scpy(mb,"ESCAPED B"); num(n2,depth_no,depth_no>9?2:1); scat(mb,n2); scat(mb,": +"); num(n2,bonus,bonus>999?4:3); scat(mb,n2); scat(mb,"G"); }
                   else scpy(mb, r==3 ? "WARPED OUT!" : (zone_cleared?"ZONE CLEARED! GO TO YOUR SHIP":"BACK ON THE SURFACE"));
                   planet_enter(); show_msg(mb); oam_flush(); fade(0); }
        }
    }
}
