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
// options, save slots and menu requests (see save.inc)
static volatile int opt_music=2;   // 0 off, 1 low, 2 full
static volatile int opt_sfx=1;
static int opt_autosave=1, opt_shake=1;
static int cur_slot=0, last_slot=0;   // the slot you are playing, and the one you used last
static int in_game=0;
static int sys_req=0;                 // set by the SYSTEM page: 1 = load cur_slot, 2 = quit to the title

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
#define EW __attribute__((section(".ewram")))
EW static Obj shadow_oam[128];
static void oam_hide_all(void){ for(int i=0;i<128;i++){ shadow_oam[i].a0 = 0x0200; shadow_oam[i].a1=0; shadow_oam[i].a2=0; } }
static void oam_flush(void){ volatile u16 *o=OAM; for(int i=0;i<128;i++){ o[i*4]=shadow_oam[i].a0; o[i*4+1]=shadow_oam[i].a1; o[i*4+2]=shadow_oam[i].a2; } }
static void set_affine(int m, int pa, int pb, int pc, int pd){ volatile u16 *o=OAM; o[m*16+3]=pa; o[m*16+7]=pb; o[m*16+11]=pc; o[m*16+15]=pd; }

#include "sound.inc"
#include "game.inc"
#include "save.inc"
#include "ui.inc"
static int launch_flag=0;
static void story_boss_beaten(void);

// ================================================================= THE PLANET
enum { O_DOOR, O_SHIP, O_CAGE, O_SLUG, O_PLANT, O_PEBBLE, O_TUFT };
typedef struct { s32 x,y,z; u8 kind, alive; } PObj;
#define NPO 110
EW static PObj po[NPO];
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
        if(dot_p0(o)>16300 && !invuln){ hurt(3+dlv()*2); if(invuln>20) show_msg("OUCH! A SLUG"); }
    }
    // A: use what is next to you
    if(HIT(K_A)){
        int best=-1; s32 bd=16150;
        for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind<=O_SLUG){ s32 d=dot_p0(&po[i]); if(d>bd){ bd=d; best=i; } }
        if(best<0) show_msg("NOTHING HERE");
        else switch(po[best].kind){
            case O_DOOR: enter_dungeon_flag=1; sfx(5); break;
            case O_SHIP:
                if(!shop_menu()){ planet_draw_bg(); planet_hud(); save_game(); show_msg(opt_autosave?"GAME SAVED":""); return; }
                save_game(); launch_flag=1; return;
            case O_CAGE: po[best].alive=0; free_bug(rndi(8)<2?BUG_PILL:rndi(6)); break;
            case O_SLUG: po[best].alive=0; gold+=5; show_msg("SQUASH! +5 GOLD"); gain_xp(3+dlv()); sfx(9); break;
        }
    }
    bag_input(); tick_timers(); hunger_tick(240); regen_tick(120);
    if(HIT(K_START)){ oam_hide_all(); oam_flush(); char_screen(VRAM); planet_draw_bg(); planet_hud(); msg_timer=0; return; }
    escape_flag=call_flag=blast_flag=0; cast_flag=-1;
    if(hearts<=0){ hearts=maxh; food=60; gold/=2; bag_n=0; bag_sel=0; show_msg("YOU FAINTED. YOU LOST YOUR BAG"); planet_hud(); }
    if(msg_timer && --msg_timer==0) show_msg("");
    if(frames%60==0) planet_hud();
    // sprites: project, scale, sort by height on screen
    EW static u8 order[NPO]; EW static s16 sy_[NPO]; int n=0;
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
#include "space.inc"

static void hud_now(void){ if(scene==0) planet_hud(); else dungeon_hud(); }
static void say(const char *t){
    if(scene==0){ show_msg(t); return; }
    int n=0; while(t[n] && n<39){ msg[n]=t[n]; n++; } msg[n]=0; msg_timer=100;
}
static void compass(volatile u16 *pg, s32 dirx, s32 diry){
    s32 dx=tgt_x-px_, dy=tgt_y-py_;
    s32 f=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16), r=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16);
    int ax= r>0 ? 1 : -1, ay= f>0 ? -1 : 1;
    s32 af=f<0?-f:f, ar=r<0?-r:r;
    if(ar*5<af*2) ax=0; else if(af*5<ar*2) ay=0;
    int cx=112, cy=30;
    for(int k=-3;k<=3;k++){ int x=cx+ax*k, y=cy+ay*k*2; for(int j=0;j<2;j++) pg[(y+j)*120+x]=C_CYAN|(C_CYAN<<8); }
    int hx=cx+ax*4, hy=cy+ay*8; for(int j=-2;j<=2;j++) for(int i=-1;i<=1;i++){ int y=hy+j, x=hx+i; if(x>=0&&x<120&&y>=18&&y<160) pg[y*120+x]=C_WHITE|(C_WHITE<<8); }
}
static void bar(volatile u16 *pg, int c0, int val, int max, int col){
    int h=34, f = max>0 ? val*h/max : 0; if(f>h) f=h; if(f<0) f=0;
    for(int y=0;y<h+2;y++){ int yy=124+y; u16 e=6|(6<<8); pg[yy*120+c0]=e; pg[yy*120+c0+2]=e; }
    for(int y=0;y<h;y++){ int yy=125+y; int ci = (h-y<=f) ? col : 0; pg[yy*120+c0+1]=ci|(ci<<8); }
}
static void hud3d(volatile u16 *pg){
    bar(pg,1,hearts,maxh,C_RED); bar(pg,4,mp,maxmp,C_CYAN); bar(pg,7,food,100,C_GREEN);
    int c0=86, c1=117, y0=146, y1=157;
    u16 rim=C_GOLD|(C_GOLD<<8), in=(R_CHEST+3)|((R_CHEST+3)<<8);
    for(int c=c0;c<=c1;c++){ pg[y0*120+c]=rim; pg[y1*120+c]=rim; for(int y=y0+1;y<y1;y++) pg[y*120+c]=in; }
    for(int y=y0;y<=y1;y++){ pg[y*120+c0]=rim; pg[y*120+c1]=rim; }
    static const char *const lab[8]={"E","","S","","W","","N",""};
    for(int k=0;k<8;k++){ int a=k*128, off=((a-pang+512)&1023)-512; if(off<-232||off>232) continue;
        int x=204+off*28/232;
        if(lab[k][0]) text(pg,x-1,y0+3,lab[k],0,1); else for(int y=y0+4;y<y0+8;y++) pix(pg,x,y,R_CHEST+1); }
    for(int y=y0+1;y<y1;y++) pix(pg,204,y,C_RED);
    // the spell you have ready
    if(spell_known[spell_sel]){ char b[20]; scpy(b,spell_name[spell_sel]); text(pg,24,150,b, mp>=spell_cost[spell_sel]?C_CYAN:C_DGREY,1); }
}

// ---------------------------------------------------------------- combat
static s32 dist8(s32 dx, s32 dy){ s32 adx=dx<0?-dx:dx, ady=dy<0?-dy:dy; return adx>ady ? adx+ady/2 : ady+adx/2; }
static int mon_dmg(DObj *o){
    int b=3+dlv()*2+depth_no/2;
    switch(o->kind){ case K_BEETLE: b=b*9/5; break; case K_BAT: b=b*7/10+1; break; case K_BOSS: b=b*5/2; break; }
    if(o->champ) b=b*3/2;
    return b;
}
static void kill_mon(DObj *o){
    int boss=o->kind==K_BOSS;
    int xpv = (4+dlv()*2+depth_no) * (o->kind==K_BEETLE?2 : o->kind==K_WASP?3 : boss?10 : 1) / (o->kind==K_WASP?2:1);
    if(o->champ) xpv*=3;
    earn(boss ? 100+depth_no*10 : 6+dlv()*4+(o->champ?40:0));
    if(q_type==Q_HUNT){ q_have++; if(q_have>=q_need && !q_done){ q_done=1; say("QUEST DONE! GO BACK TO YOUR SHIP"); } }
    if(o->champ==2 && q_type==Q_CHAMP){ q_done=1; say("QUEST TARGET SLAIN!"); }
    sfx(9); burst(o->x,o->y,0x6000,10,1);
    int ilvl=dlv()*3+depth_no;
    if(boss){ if(endless) say("GUARDIAN BEATEN!"); else { zone_cleared=1; story_boss_beaten(); say(zone==5?"THE ANT KING FALLS! THE GALAXY OPENS":"BOSS BEATEN! NEXT STAR OPEN"); } xpv+=30+dlv()*10; }
    gain_xp(xpv);
    Gear g; int drop=0;
    if(boss || o->champ){ g=roll_gear(rndi(3)==0?IT_CHARM:rndi(2)?IT_THORN:IT_SHELL, ilvl+4, 40); drop=1; }
    else if(rndi(3)==0){ g=roll_drop(ilvl,0); drop=1; }
    s32 x=o->x, y=o->y;
    o->alive=0;
    if(drop){ memset(o,0,sizeof(DObj)); o->kind=K_ITEM; o->alive=1; o->x=x; o->y=y; o->it=g.it; o->sub=g.sub; o->rar=g.rar; o->pre=g.pre; o->suf=g.suf; o->lvl=g.lvl; }
    hud_now();
}
static void hit_mon(DObj *o, int dmg, int crit){
    o->hp-=dmg; o->flash=6; add_fnum(o->x,o->y,dmg,crit?C_GOLD:C_WHITE); sfx(1);
    if(d_fire) burst(o->x,o->y,0x6000,4,0);
    if(d_venom) o->venom=90;
    if(o->hp<=0) kill_mon(o);
}

// ---------------------------------------------------------------- fishing
static int fish_state=0, fish_t=0;
static int water_ahead(s32 dirx, s32 diry){
    s32 x=px_+((dirx*13)>>4), y=py_+((diry*13)>>4); int tx=x>>16, ty=y>>16;
    if(tx<0||ty<0||tx>=MW||ty>=MW) return 0; return fmat[ty*MW+tx]==FL_WATER && dmap[ty*MW+tx]==D_FLOOR;
}
static int spark_t=0, spark_c=60, spark_y=80;

// The 3D view takes several 60ths of a second to draw. The game itself always runs 60 steps a second:
// run_game() runs as many steps as time has passed and draws once (skip_draw=1 on the catch-up steps).
#define WALK_FWD   0x0F00      // was 0x0D00
#define WALK_BACK  0x0A00      // was 0x0900
#define TURN_MIN   10          // a tap turns finely...
#define TURN_MAX   22          // ...holding speeds up to this (was a flat 12)
#define TURN_RAMP  16          // steps of holding to reach TURN_MAX
static int skip_draw=0, turn_held=0, hud_f=0, dt_last=0;
static int dungeon_frame_(void);
static int dungeon_frame(void){ u32 t=TMR(); int r=dungeon_frame_(); prof[5]=TMR()-t; return r; }
static int dungeon_frame_(void){   // returns 1 to go back up, 2 when you faint, 3 when you warp out
    tick_timers(); hunger_tick(40); regen_tick(45); bag_input();
    if(!skip_draw){ int f=frames; int fl=(sin_tab[(f*29)&255]+sin_tab[(f*47+80)&255])>>12;
      for(int i=0;i<8;i++){ int k=i+(fl>2?1:fl<-2?-1:0); if(k<0)k=0; if(k>7)k=7; BG_PAL[R_FLAME+i]=bg_pal[R_FLAME+k]; }
      for(int i=2;i<7;i++){ int k=i+((((f>>2)+i)&3)==0); BG_PAL[R_WATER+i]=bg_pal[R_WATER+(k>7?7:k)]; BG_PAL[R_CRYSTAL+i]=bg_pal[R_CRYSTAL+((((f>>3)+i*3)%5)==0 && i<6 ? i+1 : i)]; } }
    if(HIT(K_START)){ oam_hide_all(); oam_flush(); char_screen((volatile u16*)(0x06000000 + (page_back?0:0xA000))); dungeon_hud(); }
    int kl=K_LEFT, kr=K_RIGHT, ku=K_UP, kd=K_DOWN;
    if(dizzy_t){ kl=K_RIGHT; kr=K_LEFT; ku=K_DOWN; kd=K_UP; }
    if(HELD(kl)||HELD(kr)){ if(turn_held<TURN_RAMP) turn_held++; } else turn_held=0;
    int ts = TURN_MIN + (((TURN_MAX-TURN_MIN)*turn_held)>>4);
    if(HELD(kl)) pang=(pang-ts)&1023;
    if(HELD(kr)) pang=(pang+ts)&1023;
    s32 dirx=cosx(pang>>2)<<2, diry=sinx(pang>>2)<<2;
    s32 plx=-(diry*43/65), ply=dirx*43/65;
    s32 sp = HELD(ku) ? WALK_FWD : HELD(kd) ? -WALK_BACK : 0;
    sp = sp*(100+d_spd+have_bug[BUG_CRICKET]*25)/100;
    if(speed_t) sp=sp*8/5;
    if(sp){
        fish_state=0;
        s32 nx=px_+(s32)(((long long)dirx*sp)>>16), ny=py_+(s32)(((long long)diry*sp)>>16);
        s32 mx = nx + (sp>0? (dirx>0?0x4000:-0x4000) : (dirx>0?-0x4000:0x4000));
        s32 my = ny + (sp>0? (diry>0?0x4000:-0x4000) : (diry>0?-0x4000:0x4000));
        if(!solid(mx,py_)) px_=nx;
        if(!solid(px_,my)) py_=ny;
    }
    if(atk_t) atk_t--;
    if(escape_flag){ escape_flag=0; return 3; }
    // ---- spells
    if(cast_flag>=0){ int s=cast_flag; cast_flag=-1;
        if(!spell_known[s]) say("FIND A SPELL BOOK FIRST");
        else if(mp<spell_cost[s]){ say("NOT ENOUGH MANA"); }
        else { mp-=spell_cost[s];
            if(s==SP_FIRE){ fire_proj(0, px_+(dirx>>2), py_+(diry>>2), 0x7000, (dirx*3)>>5, (diry*3)>>5, 6+st_mag*2+level); sfx(3); }
            else if(s==SP_SPARK){ int best=-1; s32 bd=0x60000;
                for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue; s32 d=dist8(o->x-px_,o->y-py_); if(d<bd){ bd=d; best=i; } }
                if(best>=0){ DObj *o=&dob[best]; hit_mon(o, 5+st_mag*2+level, 0); burst(o->x,o->y,0x8000,8,2); spark_t=6;
                    s32 det=(s32)((((long long)plx*diry)-((long long)dirx*ply))>>16); s32 sx=o->x-px_, sy=o->y-py_;
                    s32 tY=(s32)((((long long)-ply*sx)+((long long)plx*sy))/det), tX=(s32)((((long long)diry*sx)-((long long)dirx*sy))/det);
                    if(tY>0x2000){ spark_c=60+(s32)(((long long)tX*60)/tY); spark_y=hz; } else { spark_c=60; spark_y=60; } }
                else say("NOTHING TO ZAP");
                sfx(10); }
            else if(s==SP_HEAL){ hearts+=10+st_mag*3; if(hearts>maxh) hearts=maxh; burst(px_+(dirx>>1),py_+(diry>>1),0x4000,10,1); sfx(6); say("HEALED"); }
            else { shield_t=900; say("SHIELD UP"); sfx(6); }
            hud_now(); }
    }
    // ---- Buddy, your pet
    int pet_here = !pet_away_t && !pet_down_t;
    if(pet_here){
        int tg=-1; s32 best=0x48000;
        for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue;
            if(dist8(o->x-px_,o->y-py_)>0x50000) continue; s32 d=dist8(o->x-pet_x,o->y-pet_y); if(d<best){ best=d; tg=i; } }
        s32 tx,ty;
        if(tg>=0){ tx=dob[tg].x; ty=dob[tg].y; } else { tx=px_-((dirx*14)>>4)+((-diry)>>2); ty=py_-((diry*14)>>4)+(dirx>>2); }
        s32 dx=tx-pet_x, dy=ty-pet_y, d=dist8(dx,dy);
        s32 st = pet_form==3?0x0B00 : pet_form==2?0x0700 : 0x0900;
        if(d > (tg>=0?0x9000:0x5000)){
            s32 stx = dx>0x800?st : dx<-0x800?-st : 0, sty = dy>0x800?st : dy<-0x800?-st : 0;
            if(!solid(pet_x+stx*3,pet_y)) pet_x+=stx; if(!solid(pet_x,pet_y+sty*3)) pet_y+=sty;
        }
        if(dist8(px_-pet_x,py_-pet_y)>0x80000){ pet_x=px_-(dirx>>1); pet_y=py_-(diry>>1); if(solid(pet_x,pet_y)){ pet_x=px_; pet_y=py_; } }
        if(pet_cool) pet_cool--;
        if(petobj.flash) petobj.flash--;
        if(tg>=0 && best<0xC000 && !pet_cool){ hit_mon(&dob[tg], pet_dmg(), 0); pet_cool = pet_form==1?12:20; if(pet_form==1) burst(dob[tg].x,dob[tg].y,0x5000,5,0); }
        if(pet_hp<=0){ pet_down_t=900; say("BUDDY IS HURT. HE'LL BE BACK"); }
    }
    if(pet_down_t && --pet_down_t==0){ pet_hp=pet_maxhp/2; pet_x=px_; pet_y=py_; say("BUDDY IS BACK!"); }
    // ---- monsters, pickups, cages
    int near_cage=-1; DObj *named=0; s32 named_d=0x60000;
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; if(o->flash) o->flash--;
        s32 dx=px_-o->x, dy=py_-o->y, d=dist8(dx,dy);
        if(is_mon(o->kind)){
            int boss=o->kind==K_BOSS, bat=o->kind==K_BAT, wasp=o->kind==K_WASP;
            s32 reach = boss?0x14000 : o->kind==K_BEETLE?0xC000 : 0xB000;
            if((o->champ||boss) && d<named_d){ named=o; named_d=d; }
            if(o->venom){ o->venom--; if((o->venom%15)==0){ int vd=1+d_venom/3; o->hp-=vd; add_fnum(o->x,o->y,vd,C_GREEN); if(o->hp<=0){ kill_mon(o); continue; } } }
            if(blast_flag && d<0x30000){ hit_mon(o, 8+level*2, 0); if(!o->alive) continue; }
            // go after whoever is closer: you or Buddy
            s32 pdx=pet_x-o->x, pdy=pet_y-o->y, pd = pet_here ? dist8(pdx,pdy) : 0x7FFFFFFF;
            int at_pet = pd < d;
            s32 tdx = at_pet?pdx:dx, tdy = at_pet?pdy:dy, td = at_pet?pd:d;
            if(!stun_t && td<0x70000){
                s32 st = boss?0x0500 : bat?(0x0700+((frames+i*9)%24<12?0x400:0)) : o->kind==K_BEETLE?0x0380 : 0x0600;
                int away = wasp && td<0x18000, keep = wasp && td<0x40000 && td>=0x18000;
                if(!keep && (td>reach-0x2000 || away)){
                    s32 stx=(tdx>0?st:-st), sty=(tdy>0?st:-st); if(away){ stx=-stx; sty=-sty; }
                    if(!solid(o->x+stx*3,o->y)) o->x+=stx; if(!solid(o->x,o->y+sty*3)) o->y+=sty; }
            }
            if(o->cool) o->cool--;
            if(wasp && !stun_t && !o->cool && d<0x60000){        // wasps shoot stingers
                s32 l=isqrt((u32)((dx>>8)*(dx>>8)+(dy>>8)*(dy>>8))); if(l<1) l=1;
                fire_proj(1, o->x, o->y, 0x8000, (dx>>8)*0x1400/l, (dy>>8)*0x1400/l, mon_dmg(o)); o->cool=55+rndi(30); }
            if(!wasp && !stun_t && td<reach && !o->cool){
                o->cool=boss?40:50;
                if(at_pet){ pet_hp-=mon_dmg(o)/2; petobj.flash=6; }
                else { if(!invuln){ int before=hearts; hurt(mon_dmg(o)); if(hearts<before) hurt_flash=8; }
                       int ret=d_reflect+d_sting; if(ret){ hit_mon(o,ret,0); if(!o->alive) continue; } }
            }
        } else if(o->kind==K_CHEST){ if(d<0x9000){ o->alive=0; earn(10+rndi(20)+dlv()*8+depth_no*3); say("TREASURE!"); sfx(7);
                Gear g=roll_drop(dlv()*3+depth_no+2,15); add_gear(g); hud_now(); } }
        else if(o->kind==K_ITEM){ if(d<0x9000 && (bag_n<BAGMAX || o->it==IT_QUEST)){ Gear g={o->it,o->sub,o->rar,o->pre,o->suf,o->lvl}; if(add_gear(g)) o->alive=0; } }
        else if(o->kind==K_CAGE){ if(d<0xC000) near_cage=i; }
    }
    blast_flag=0;
    if(call_flag){ call_flag=0; for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].kind==K_CAGE){ dob[i].alive=0; free_bug(dob[i].sub); break; } }
    // ---- fireballs and stingers
    for(int i=0;i<NPROJ;i++){ Proj *p=&proj[i]; if(!p->alive) continue;
        p->x+=p->vx; p->y+=p->vy; if(--p->life<=0 || solid(p->x,p->y)){ p->alive=0; burst(p->x-p->vx,p->y-p->vy,p->h,p->kind?4:10,0); continue; }
        if(p->kind==0){ if(frames&1) burst(p->x,p->y,p->h,1,0);
            for(int k=0;k<ndo;k++){ DObj *o=&dob[k]; if(!o->alive||!is_mon(o->kind)) continue;
                if(dist8(o->x-p->x,o->y-p->y)<0x9000){ hit_mon(o,p->dmg,0); p->alive=0; burst(p->x,p->y,p->h,12,0); break; } } }
        else if(dist8(px_-p->x,py_-p->y)<0x7000){ p->alive=0; int before=hearts; hurt(p->dmg); if(hearts<before) hurt_flash=8; }
    }
    // ---- A: open a cage, take the stairs, fish, or bite
    int here=dmap[(py_>>16)*MW+(px_>>16)];
    if(fish_state==1 && --fish_t<=0){ fish_state=2; fish_t=14; sfx(8); say("! A BITE ! PRESS A"); }
    else if(fish_state==2 && --fish_t<=0){ fish_state=0; say("IT GOT AWAY"); }
    if(HIT(K_A)){
        if(fish_state==2){ fish_state=0; int r=rndi(100), f = r<45?0 : r<62?1 : r<79?2 : r<88?3 : 4; add_item(IT_FISH,f); sfx(8); }
        else if(near_cage>=0){ dob[near_cage].alive=0; free_bug(dob[near_cage].sub); }
        else if(here==D_UP) return 1;
        else if(here==D_DOWN){ fade(1); dungeon_enter(); fade(0); return 0; }
        else if(fish_state==1){ fish_state=0; say("YOU REEL IN"); }
        else if(water_ahead(dirx,diry)){ fish_state=1; fish_t=25+rndi(50); say("FISHING... WAIT FOR A BITE"); sfx(8); }
        else if(!atk_t){
            atk_t=14-d_haste; if(atk_t<6) atk_t=6; int crit; int pw=bite_power(&crit); sfx(0);
            for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue;
                s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16);
                s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(side<0) side=-side;
                s32 rng_=o->kind==K_BOSS?0x1C000:0x14000;
                if(fwd>0 && fwd<rng_ && side<0xC000){ if(o->kind!=K_BOSS && o->kind!=K_BEETLE && !o->champ){ o->x+=dirx>>2; o->y+=diry>>2; } hit_mon(o,pw,crit); }
            }
        }
    }
    if(hearts<=0) return 2;
    // ---- things that count down or drift: one tick per game step, drawn or not
    if(sp){ bob_ph += speed_t?14:10; }
    if(shake_t) shake_t--;
    if(msg_timer) msg_timer--;
    if(hurt_flash) hurt_flash--;
    if(spark_t) spark_t--;
    parts_update(); fnums_update();
    if(skip_draw) return 0;
    // ---- draw into the back page
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
    hz = 89 + (sinI[bob_ph&255]>>13) + (shake_t ? ((shake_t&1)?3:-3) : 0);
    render_view(pg,dirx,diry,plx,ply);
    static u8 ord[NDO+1]; static s32 dd[NDO+1]; int n=0;
    for(int i=0;i<ndo;i++) if(dob[i].alive){ s32 dx=dob[i].x-px_, dy=dob[i].y-py_; dd[i]=(dx>>8)*(dx>>8)+(dy>>8)*(dy>>8); ord[n++]=i; }
    if(pet_here){ petobj.kind=K_PET; petobj.alive=1; petobj.x=pet_x; petobj.y=pet_y; s32 dx=pet_x-px_, dy=pet_y-py_; dd[NDO]=(dx>>8)*(dx>>8)+(dy>>8)*(dy>>8); ord[n++]=NDO; }
    for(int i=1;i<n;i++){ u8 t=ord[i]; int j=i; while(j>0 && dd[ord[j-1]]<dd[t]){ ord[j]=ord[j-1]; j--; } ord[j]=t; }
    for(int i=0;i<n;i++) draw_sprite(pg, ord[i]==NDO ? &petobj : &dob[ord[i]], dirx,diry,plx,ply);
    for(int i=0;i<NPROJ;i++){ Proj *p=&proj[i]; if(!p->alive) continue;
        if(p->kind==0) draw_glow(pg,p->x,p->y,p->h,0x2400 + (sinI[(frames*40)&255]>>6),R_FLAME+7,R_FLAME+4,dirx,diry,plx,ply);
        else draw_glow(pg,p->x,p->y,p->h,0x0C00,R_CHEST+7,R_CHEST+5,dirx,diry,plx,ply); }
    parts_draw(pg,dirx,diry,plx,ply);
    draw_fnums(pg,dirx,diry,plx,ply);
    if(spark_t){ int c=60, y=140; for(int k=0;k<12;k++){ int c2=60+(spark_c-60)*(k+1)/12+(k<11?rndi(5)-2:0), y2=140+(spark_y-140)*(k+1)/12;
            int steps=(y-y2)>0?(y-y2):1; for(int s=0;s<=steps;s+=2){ int cc=c+(c2-c)*s/steps, yy=y-s; if(cc>=0&&cc<120&&yy>=18&&yy<160){ pg[yy*120+cc]=(C_WHITE)|(C_WHITE<<8); if(cc+1<120) pg[yy*120+cc+1]=(R_CRYSTAL+7)|((R_CRYSTAL+7)<<8);} }
            c=c2; y=y2; } }
    if(fish_state){ int by = 118 + (fish_state==2 ? 4 : (sinI[(frames*20)&255]>>12)); for(int y=100;y<by;y+=2) pg[y*120+66]=(R_STONE+6)|((R_STONE+6)<<8);
        for(int y=by;y<by+4;y++){ pg[y*120+66]=216|(216<<8); pg[y*120+67]=216|(216<<8); } }
    if(sight || have_bug[BUG_ANT]) compass(pg,dirx,diry);
    hud3d(pg);
    if(named){ char b[32]; b[0]=0; if(named->kind==K_BOSS) scpy(b, endless?"GUARDIAN":"THE QUEEN"); else if(named->champ==2) scpy(b,"QUEST TARGET"); else { scpy(b,champ_pre[named->cname&7]); scat(b," "); scat(b,kind_name[named->kind]); }
        int w=textlen(b)*4; text(pg,120-w/2,36,b,C_RED,1); int hw2 = named->hp*40/(named->maxhp?named->maxhp:1); for(int c=40;c<80;c++){ int ix=(c-40)<hw2/2*1+0 ? C_RED : C_DRED; if(c-40<hw2) ix=C_RED; pg[43*120+c]=ix|(ix<<8); } }
    if(msg_timer){ text(pg,120-textlen(msg)*4,24,msg,C_WHITE,2); }
    else if(near_cage>=0) text(pg,120-12*4,24,"A: OPEN CAGE",C_WHITE,2);
    else if(here==D_UP||here==D_DOWN){ const char *m2 = here==D_UP? "A: CLIMB UP" : "A: GO DEEPER"; text(pg,120-textlen(m2)*4,24,m2,C_WHITE,2); }
    else if(!fish_state && water_ahead(dirx,diry)) text(pg,120-9*4,24,"A: FISH",C_CYAN,2);
    if(shield_t && (frames&16)){ for(int c=0;c<120;c+=6){ pg[18*120+c]=(R_CRYSTAL+6)|((R_CRYSTAL+6)<<8); pg[159*120+c]=(R_CRYSTAL+6)|((R_CRYSTAL+6)<<8); } }
    if(hurt_flash){ for(int y=18;y<22;y++) for(int x=0;x<120;x++) pg[y*120+x]=C_RED|(C_RED<<8); }
    if(frames-hud_f>=60){ hud_f=frames; dungeon_hud(); }
    oam_hide_all();
    int bob = (sp ? ((frames>>3)&1) : 0) + (atk_t>7 ? 6 : 0);
    if(!(invuln && (frames&4))){
        shadow_oam[0].a0 = ((98-bob)&255);
        shadow_oam[0].a1 = ((120-32)&511) | (3<<14);
        shadow_oam[0].a2 = T_BACK;
    }
    return 0;
}

// ---------------------------------------------------------------- the title screen: CONTINUE / NEW GAME / LOAD GAME / OPTIONS
static void title_restore(volatile u16 *pg, int y0, int y1){ const u16 *src=(const u16*)planet_img; for(int i=y0*120;i<y1*120;i++) pg[i]=src[i]; }
static void title_paint(volatile u16 *pg){
    int z=zone, e=endless; zone=0; endless=0; planet_palette(); planet_draw_bg(); zone=z; endless=e;
    rect(pg,0,0,240,18,0);
    text(pg,66,26,"BUGSTAR",C_DGREY,4); text(pg,64,24,"BUGSTAR",C_GOLD,4);
    stext(pg,68,50,"A LITTLE BUG. A BIG GALAXY.",C_WHITE);
}
static void twinkle(volatile u16 *pg, int t){        // a few stars blink in the side margins
    const u8 *img=(const u8*)planet_img;
    for(int k=0;k<14;k++){
        int x=(k*37+11)%48; if(k&1) x+=192;
        int y=21+((k*53+9)%86);
        int o=img[y*240+x]; if(o>3) continue;
        int ph=((t>>3)+k*5)&15;
        pix(pg,x,y, ph==0 ? 5 : (ph==1||ph==15) ? 4 : o);
    }
}
// returns 1 to continue or load the game in cur_slot, 0 to start a new one there
static int title_screen(void){
    volatile u16 *pg=VRAM;
    enum { TI_CONT, TI_NEW, TI_LOAD, TI_OPT };
    static const char *const tn[4]={"CONTINUE","NEW GAME","LOAD GAME","OPTIONS"};
    volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i];
    for(int i=0;i<16;i++) OBJ_PAL[i]=obj_pal[i];
    title_paint(pg);
    REG_BLDCNT = 0x3F | (3<<6); REG_BLDY = 16;
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    music(2);
    int sel=0, blink=0, dirty=1, full=1, n=0, cs=-1; int it[4];
    wait_release();
    int fading_in=1;
    while(1){
        if(full){ full=0; title_paint(pg); slots_refresh(); cs=first_good_slot();
            n=0; if(cs>=0) it[n++]=TI_CONT; it[n++]=TI_NEW; if(cs>=0) it[n++]=TI_LOAD; it[n++]=TI_OPT;
            if(sel>=n) sel=0; dirty=1; }
        if(dirty){ dirty=0; char b[40];
            title_restore(pg,56,118);
            for(int i=0;i<n;i++){ int y=62+i*11; const char *nm=tn[it[i]]; int x=120-textlen(nm)*2;
                if(i==sel){ rect(pg,66,y-2,108,10,6); text(pg,x,y,nm,C_GOLD,1); text(pg,70,y,">",C_GOLD,1); text(pg,166,y,"<",C_GOLD,1); }
                else stext(pg,x,y,nm,C_WHITE); }
            int y=62+n*11+3; b[0]=0;
            if(it[sel]==TI_CONT){ const SlotInfo *o=&sinf[cs]; char w[28]; slot_where(o,w); scpy(b,"SLOT "); ncat(b,cs+1); scat(b,"  LV "); ncat(b,o->level); scat(b,"  "); scat(b,w); }
            else if(it[sel]==TI_NEW) scpy(b,"START A NEW ADVENTURE");
            else if(it[sel]==TI_LOAD) scpy(b,"PICK ONE OF YOUR 3 SAVE SLOTS");
            else scpy(b,"SOUND, AUTOSAVE AND MORE");
            b[34]=0; stext(pg,120-textlen(b)*2,y,b,C_CYAN);
        }
        oam_hide_all(); set_affine(31,128,0,0,128);
        shadow_oam[0].a0=((150-32-((blink>>3)&1))&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[0].a1=((120-32)&511)|(31<<9)|(2<<14); shadow_oam[0].a2=T_BUGSIDE;
        vsync(); oam_flush(); read_keys(); blink++; rng_s += blink;
        if(fading_in){ fading_in=0; fade(0); }
        twinkle(pg,blink);
        if(HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; sfx(7); }
        if(HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; sfx(7); }
        if(HIT(K_A)||HIT(K_START)){
            int a=it[sel]; sfx(2);
            if(a==TI_CONT){ cur_slot=cs; return 1; }
            if(a==TI_NEW){ int s2=slot_picker(pg,SP_NEW,0); if(s2>=0){ cur_slot=s2; return 0; } full=1; }
            if(a==TI_LOAD){ int s2=slot_picker(pg,SP_LOAD,0); if(s2>=0){ cur_slot=s2; return 1; } full=1; }
            if(a==TI_OPT){ options_screen(pg); full=1; }
            wait_release();
        }
    }
}

// ================================================================= starting, running and leaving a game
static void start_game(int cont){       // fades out, sets the world up from cur_slot (or fresh) and fades in on the planet
    rng_s ^= vbl_count*2654435761u;
    fade(1);
    gen_cluster();
    if(!(cont && load_game(1))){ cont=0; new_game(); gen_cluster(); load_ext_defaults(); play_reset(0); do_save(); }
    scene=0; launch_flag=0; enter_dungeon_flag=0; fish_state=0; msg_timer=0; invuln=0; shake_t=0; page_back=0; loot=0; depth_no=0;
    speed_t=strong_t=dizzy_t=stun_t=shield_t=0;
    restock();
    planet_enter(); new_planet(); show_msg(cont ? "WELCOME BACK" : "THE GARDEN: FIND THE DOOR");
    oam_hide_all(); oam_flush(); fade(0);
    music(0);
}
static void pet_away_tick(void){
    if(pet_away_t && --pet_away_t==0){ gold+=pet_sold_g; char b[40]; scpy(b,"BUDDY SOLD "); ncat(b,pet_sold_n); scat(b," THINGS: +"); ncat(b,pet_sold_g); scat(b,"G"); say(b); sfx(7); hud_now(); pet_x=px_; pet_y=py_; }
}
// plays until the player chooses QUIT TO TITLE
static void run_game(void){
    while(1){
        read_keys(); frames++; if(invuln) invuln--;
        if(sys_req){ int rq=sys_req; sys_req=0; if(rq==2) return; start_game(1); continue; }
        rng_s += keys;
        pet_away_tick();
        if(scene==2){
            int r=space_frame();
            vsync();
            REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1;
            if(r==2) galaxy_map();
            else if(r==3){ station_menu(); save_game(); }
            else if(r==1){ fade(1); land_on_current(); portal_depth=0; scene=0; msg_timer=0;
                planet_enter(); new_planet(); planet_palette(); planet_draw_bg(); planet_hud(); restock(); save_game();
                show_msg(world_name()); oam_hide_all(); oam_flush(); music(0); fade(0); }
            continue;
        }
        if(scene==0){
            planet_frame();
            vsync(); oam_flush();
            if(launch_flag){ launch_flag=0; fade(1); oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(2); fade(0); continue; }
            if(enter_dungeon_flag){ enter_dungeon_flag=0; fade(1); oam_hide_all(); oam_flush();
                depth_no=0; if(portal_depth>0){ depth_no=portal_depth-1; portal_depth=0; }
                scene=1; dungeon_enter(); page_back=1; music(1); fade(0); }
        } else {
            int r=0;
            int n = vbl_count - dt_last; dt_last = vbl_count;          // 60ths of a second since the last picture
            if(n>6) n = (n>10) ? 1 : 6;                                // after a menu or a load, don't fast-forward
            if(n<1) n=1;
            for(int i=0;i<n;i++){
                if(i){ frames++; if(invuln) invuln--; pet_away_tick(); keys_old=keys; }
                skip_draw = (i<n-1);
                r=dungeon_frame();
                if(r) break;
            }
            skip_draw=0;
            vsync();
            if(scene==1){ REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1; }
            oam_flush();
            if(r){ fade(1); oam_hide_all(); oam_flush(); scene=0; msg_timer=0; fish_state=0;
                   char mb[40];
                   if(r==2){ recalc(); hearts=maxh; food=60; if(!endless) gold/=2; loot=0; bag_n=0; bag_sel=0; portal_depth=0; scpy(mb, endless ? "YOU FAINTED. LOOT AND BAG LOST" : "YOU FAINTED. YOU LOST YOUR BAG"); }
                   else if(endless){ int bonus = loot*(100+depth_no*15)/100; gold+=bonus; loot=0; scpy(mb,"ESCAPED B"); ncat(mb,depth_no); scat(mb,": +"); ncat(mb,bonus); scat(mb,"G"); }
                   else if(r==3 && portal_depth) scpy(mb,"PORTAL OPEN: THE DOOR TAKES YOU BACK");
                   else scpy(mb, zone_cleared?"ZONE CLEARED! GO TO YOUR SHIP":"BACK ON THE SURFACE");
                   restock(); save_game();
                   planet_enter(); show_msg(mb); oam_flush(); music(0); fade(0); }
        }
    }
}

int main(void){
    REG_DISPCNT = 0x80;
    REG_WAITCNT = 0x4317;
    *(volatile u16*)0x04000100=0; *(volatile u16*)0x04000104=0; *(volatile u16*)0x04000106=0x84; *(volatile u16*)0x04000102=0x80;
    sound_init();
    rng_s ^= 0x9E3779B9;
    settings_load(); music_apply();
    while(1){
        in_game=0; sys_req=0; scene=0;
        cur_slot=last_slot;
        new_game();
        int cont=title_screen();
        start_game(cont);
        in_game=1;
        run_game();
        fade(1); oam_hide_all(); oam_flush(); music(-1);
        REG_BLDCNT=0; REG_BLDY=0; REG_DISPCNT=0x80;      // dark, until the title is painted
    }
}
