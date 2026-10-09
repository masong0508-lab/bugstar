// BUGSTAR prototype: a pill bug on a turning planet, and a colour 3D dungeon underneath.
#include "assets.h"
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef signed short s16; typedef signed char s8; typedef int s32; typedef unsigned long long u64;
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
static int opt_autosave=1, opt_shake=1, opt_easy=0, opt_diff=1, opt_cam=0, opt_minimap=1, opt_nums=1, opt_hints=1, opt_arrow=1;   // easy = (opt_diff==0)
static int cur_slot=0, last_slot=0;   // the slot you are playing, and the one you used last
static int in_game=0;
static int sys_req=0, cluster_no=0;
static int in_menu=0; static char menu_msg[40];   // inside a menu, messages go on a line in the menu instead of over it                 // set by the SYSTEM page: 1 = load cur_slot, 2 = quit to the title

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
    if(sc==2 && !(x&1)){                                   // fast path: big text on an even column writes whole pixel pairs
        u16 cc=c|(c<<8);
        for(;*s;s++){ u16 g = font[(u8)*s & 127];
            for(int k=0;k<15;k++) if(g & (1<<(14-k))){ volatile u16 *p = pg + ((y+(k/3)*2)*120 + ((x+(k%3)*2)>>1)); p[0]=cc; p[120]=cc; }
            x += 8; }
        return x; }
    if(sc==1){
        for(;*s;s++){ u16 g = font[(u8)*s & 127];
            for(int r=0;r<5;r++){ int bits=(g>>(12-r*3))&7; if(!bits) continue; volatile u16 *row = pg + (y+r)*120;
                for(int k=0;k<3;k++) if(bits&(4>>k)){ int xx=x+k; volatile u16 *p=row+(xx>>1); if(xx&1) *p=(*p&0x00FF)|(c<<8); else *p=(*p&0xFF00)|c; } }
            x += 4; }
        return x; }
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

static volatile int hdma_on=0;
// the galaxy and your ship's hold (used by the planet and the ship menu too)
#define NSYS 32
typedef struct { u8 x,y,kind,eco; } Sys;            // kind 0..5 = story zone, 6 = uncharted world
EW static Sys sys[NSYS];
static int cur_sys=0, fuel=70, hull=100, shield=40, shield_lv=1, laser_lv=1, cargo_cap=10, ant_king=0, cleared_mask=0, zones_open=1, visits=0;
static u8 cargo[8];
static const char *const eco_name[4]={"FARMING","MINING","TECH","HIVE"};
static const char *const goods[8]={"FOOD","NECTAR","SILK","AMBER","CRYSTALS","MACHINES","MEDICINE","RELICS"};
static int cargo_n(void){ int n=0; for(int i=0;i<8;i++) n+=cargo[i]; return n; }
#include "sound.inc"
#include "game.inc"
#include "save.inc"
static void title_restore(volatile u16 *pg, int y0, int y1); static void twinkle(volatile u16 *pg, int t);
#include "ui.inc"
static int launch_flag=0, zone_card=0, arrow_ship=0;
static void story_boss_beaten(void); static int hive_here(void); static int hive_boss_floor(void); static void hive_kill(void);

#define VISPG ((volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0xA000:0)))   // the page on screen right now
#define IWRAM_DATA
#define TMR() (*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16))
static u16 dim15(u16 c, int k){ int r=(c&31)*k/16, g=((c>>5)&31)*k/16, b=((c>>10)&31)*k/16; return r|(g<<5)|(b<<10); }
#include "planet.inc"

#include "dungeon.inc"
#include "space.inc"

static void hud_now(void){ if(scene==2) return; if(scene==0) planet_hud(); else dungeon_hud(); }
static void say(const char *t){
    if(in_menu){ int n=0; while(t[n] && n<39){ menu_msg[n]=t[n]; n++; } menu_msg[n]=0; return; }
    if(scene==0){ show_msg(t); return; }
    int n=0; while(t[n] && n<39){ msg[n]=t[n]; n++; } msg[n]=0; msg_timer=24;
}
static void compass(volatile u16 *pg, s32 dirx, s32 diry){
    s32 dx=tgt_x-px_, dy=tgt_y-py_;
    s32 f=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16), r=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16);
    int ax= r>0 ? 1 : -1, ay= f>0 ? -1 : 1;
    s32 af=f<0?-f:f, ar=r<0?-r:r;
    if(ar*5<af*2) ax=0; else if(af*5<ar*2) ay=0;
    int cx=112, cy=46;
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
    int c0=86, c1=117, y0=20, y1=31;          // the compass sits at the top now, out of the way of your jaws
    u16 rim=C_GOLD|(C_GOLD<<8), in=(R_CHEST+3)|((R_CHEST+3)<<8);
    for(int c=c0;c<=c1;c++){ pg[y0*120+c]=rim; pg[y1*120+c]=rim; for(int y=y0+1;y<y1;y++) pg[y*120+c]=in; }
    for(int y=y0;y<=y1;y++){ pg[y*120+c0]=rim; pg[y*120+c1]=rim; }
    static const char *const lab[8]={"E","","S","","W","","N",""};
    for(int k=0;k<8;k++){ int a=k*128, off=((a-pang+512)&1023)-512; if(off<-232||off>232) continue;
        int x=204+off*28/232;
        if(lab[k][0]) text(pg,x-1,y0+3,lab[k],0,1); else for(int y=y0+4;y<y0+8;y++) pix(pg,x,y,R_CHEST+1); }
    for(int y=y0+1;y<y1;y++) pix(pg,204,y,C_RED);
    // the spell you have ready
    if(spell_known[spell_sel]){ char b[20]; scpy(b,spell_name[spell_sel]); text(pg,4,21,b, mp>=spell_cost[spell_sel]?C_CYAN:C_DGREY,1); }
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
    int boss=o->kind==K_BOSS, hive = boss && endless && hive_boss_floor();
    if(o->kind>=0 && o->kind<11) st_kills[o->kind]++;
    int xpv = (4+dlv()*2+depth_no) * (o->kind==K_BEETLE?2 : o->kind==K_WASP?3 : boss?10 : 1) / (o->kind==K_WASP?2:1);
    if(o->champ) xpv*=3;
    earn(boss ? 100+depth_no*10 : 6+dlv()*4+(o->champ?40:0));
    if(q_type==Q_HUNT){ q_have++; if(q_have>=q_need && !q_done){ q_done=1; say("QUEST DONE! GO BACK TO YOUR SHIP"); } }
    if(o->champ==2 && q_type==Q_CHAMP){ q_done=1; say("QUEST TARGET SLAIN!"); }
    sfx(9); debris(o->x,o->y,0x6000,boss?16:10,3,o->kvx,o->kvy); burst(o->x,o->y,0x6000,4,1);
    int ilvl=dlv()*3+depth_no;
    if(boss){ music(1); if(!endless && !zone_cleared) zone_card=1+zone;
        if(endless && hive_boss_floor()){ hive_kill(); say("OVERLORD DOWN! THE HIVE DIES"); xpv+=200+dlv()*20; } else if(endless) say("GUARDIAN BEATEN!"); else if(zone_cleared){ if(tiers[zone]<9) tiers[zone]++; st_tiers++; char tb[40]; scpy(tb,"TIER "); ncat(tb,tiers[zone]); scat(tb," CLEARED! LOOT GETS BETTER"); say(tb); }
              else { zone_cleared=1; story_boss_beaten(); say(zone==5?"THE ANT KING FALLS! THE GALAXY OPENS":"BOSS BEATEN! NEXT STAR OPEN"); } xpv+=30+dlv()*10; }
    gain_xp(xpv);
    Gear g; int drop=0;
    int uq_chance = hive ? 1000 : boss ? 300 : o->champ ? 50 : 4;          // in 1000
    if(rndi(1000) < uq_chance){ g=roll_unique(ilvl); drop=1; st_uniq++; say("A UNIQUE ITEM DROPPED!"); sfx(6); }
    else if(boss || o->champ){ g=roll_gear(rndi(3)==0?IT_CHARM:rndi(2)?IT_THORN:IT_SHELL, ilvl+4, 40); drop=1; }
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


// ---------------------------------------------------------------- weight and bodies: nothing in the dungeon is a ghost
// You speed up and slow down, bump into walls, and can't walk through things. Hits knock monsters back
// (into walls and into each other), the game freezes for a blink when a bite lands, and the view lunges and recoils.
#define PRAD 0x3000
static int combo=0, combo_t=0, dodge_t=0, dodge_dir=0, tapL_t=0, tapR_t=0, dodge_msg_t=0;
static s32 pvx=0, pvy=0; static int hitstop=0, kick_t=0, bump_cd=0, pknock_t=0, moved_real=0;
static s32 body_r(DObj *o){ s32 r; switch(o->kind){ case K_SPIDER: r=0x3000; break; case K_BEETLE: r=0x4000; break; case K_BOSS: r=0x6800; break; case K_BAT: r=0x2400; break;
    case K_WASP: r=0x2800; break; case K_BARREL: r=o->sub?0x3800:0x3200; break; case K_CAGE: r=0x3000; break; default: return 0; } return o->champ ? r*5/4 : r; }
static int body_m(DObj *o){ int m; switch(o->kind){ case K_SPIDER: m=3; break; case K_BEETLE: m=8; break; case K_BOSS: m=40; break; case K_BAT: m=1; break;
    case K_WASP: m=2; break; case K_BARREL: m=o->sub?9:5; break; default: m=999; } return o->champ ? m*2 : m; }
static int box_solid(s32 x, s32 y, s32 r){ return solid(x-r,y-r)||solid(x+r,y-r)||solid(x-r,y+r)||solid(x+r,y+r); }
static s32 iabs(s32 v){ return v<0?-v:v; }
// push two round bodies apart; the heavier one moves less. a wall stops a body, and then the other one gets the whole push
static int separate(s32 *ax, s32 *ay, s32 ar, int am, s32 awr, s32 *bx, s32 *by, s32 br, int bm, s32 bwr, s32 *nx, s32 *ny){
    s32 dx=*ax-*bx, dy=*ay-*by, rr=ar+br;
    if(dx>=rr||dx<=-rr||dy>=rr||dy<=-rr) return 0;
    s32 l=isqrt((u32)((dx>>6)*(dx>>6)+(dy>>6)*(dy>>6)))<<6;
    if(l>=rr) return 0;
    if(l<0x100){ dx=0x100; dy=(frames&1)?0x80:-0x80; l=0x100; }
    s32 ux=(s32)(((long long)dx<<16)/l), uy=(s32)(((long long)dy<<16)/l);
    s32 ov=rr-l; s32 pa=(s32)((long long)ov*bm/(am+bm)), pb=ov-pa;
    s32 ax2=*ax+(s32)(((long long)ux*pa)>>16), ay2=*ay+(s32)(((long long)uy*pa)>>16);
    int amov = !box_solid(ax2,ay2,awr);
    if(!amov) pb=ov;
    s32 bx2=*bx-(s32)(((long long)ux*pb)>>16), by2=*by-(s32)(((long long)uy*pb)>>16);
    int bmov = !box_solid(bx2,by2,bwr);
    if(amov){ if(!bmov){ ax2=*ax+(s32)(((long long)ux*ov)>>16); ay2=*ay+(s32)(((long long)uy*ov)>>16); if(box_solid(ax2,ay2,awr)){ ax2=*ax; ay2=*ay; } } *ax=ax2; *ay=ay2; }
    if(bmov){ *bx=bx2; *by=by2; }
    *nx=ux; *ny=uy; return 1;
}
static void thump(int big){ if(!bump_cd){ sfx(big?12:11); bump_cd=big?6:18; } kick_t = big?6:3; if(big && opt_shake) shake_t=2; }
static void barrel_break(DObj *o){
    s32 x=o->x, y=o->y; st_barrels++; ach_check(); debris(x,y,0x5000,12,2,o->kvx,o->kvy); sfx(13);
    int r=rndi(100); Gear g={0,0,0,0,0,0};
    if(r<30){ int gv=3+dlv()*3+depth_no; earn(gv); add_fnum(x,y,gv,C_GOLD); o->alive=0; }
    else if(r<48) g.it=IT_CRUMB; else if(r<60){ g.it=IT_BERRY; g.sub=rndi(8); } else if(r<66) g=roll_drop(dlv()*3+depth_no,10);
    else o->alive=0;
    if(g.it){ memset(o,0,sizeof(DObj)); o->kind=K_ITEM; o->alive=1; o->x=x; o->y=y; o->it=g.it; o->sub=g.sub; o->rar=g.rar; o->pre=g.pre; o->suf=g.suf; o->lvl=g.lvl; }
}
static void knock(DObj *o, s32 dx, s32 dy, s32 power){       // dx,dy: a direction (16.16, length about 1)
    s32 kb = power*5/body_m(o); if(kb>0x3400) kb=0x3400;
    o->kvx += (s32)(((long long)dx*kb)>>16); o->kvy += (s32)(((long long)dy*kb)>>16);
}
static void player_move(s32 tvx, s32 tvy){
    if(pknock_t){ pvx+=(tvx-pvx)>>4; pvy+=(tvy-pvy)>>4; }
    else if(tvx||tvy){ pvx+=(tvx-pvx)>>2; pvy+=(tvy-pvy)>>2; }      // speeding up takes a moment...
    else { pvx-=pvx>>1; pvy-=pvy>>1; }                              // ...and stopping is quick, with a little weight
    if(iabs(pvx)<0x60) pvx=0; if(iabs(pvy)<0x60) pvy=0;
    s32 r = box_solid(px_,py_,PRAD) ? 0x1000 : PRAD, ox=px_, oy=py_;
    s32 nx=px_+pvx; if(!box_solid(nx,py_,r)) px_=nx; else { s32 v=iabs(pvx); if(v>0x500) thump(v>0x1000); pvx=0; }
    s32 ny=py_+pvy; if(!box_solid(px_,ny,r)) py_=ny; else { s32 v=iabs(pvy); if(v>0x500) thump(v>0x1000); pvy=0; }
    moved_real = iabs(px_-ox)+iabs(py_-oy) > 0x200;
}
static void bodies_tick(void){          // you against everything solid, every 60th
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; s32 r=body_r(o); if(!r) continue; s32 nx,ny;
        s32 me = is_mon(o->kind) ? 0x7800 : PRAD;          // monsters stop at biting distance, not in your face
        if(separate(&px_,&py_,me,5,PRAD,&o->x,&o->y,r,body_m(o),r>>1,&nx,&ny)){
            s32 vn=(s32)(((long long)pvx*nx+(long long)pvy*ny)>>16);
            if(vn<0){ pvx-=(s32)(((long long)nx*vn)>>16); pvy-=(s32)(((long long)ny*vn)>>16); } } }
}
static void mon_bodies(void){           // monsters, barrels and Buddy shove each other (15 times a second, near you)
    int pet_here = !pet_away_t && !pet_down_t;
    for(int i=0;i<ndo;i++){ DObj *a=&dob[i]; if(!a->alive) continue; s32 ra=body_r(a); if(!ra) continue;
        if(iabs(a->x-px_)>0x70000 || iabs(a->y-py_)>0x70000) continue; s32 nx,ny;
        if(pet_here && a->kind!=K_CAGE) separate(&pet_x,&pet_y,0x2800,1,0x1400,&a->x,&a->y,ra,body_m(a),ra>>1,&nx,&ny);
        for(int j=i+1;j<ndo;j++){ DObj *b=&dob[j]; if(!b->alive) continue; s32 rb=body_r(b); if(!rb) continue;
            separate(&a->x,&a->y,ra,body_m(a),ra>>1,&b->x,&b->y,rb,body_m(b),rb>>1,&nx,&ny); } }
}
static void knock_tick(void){           // things that were hit slide, slam into walls, and bowl into each other
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive || !(o->kvx||o->kvy)) continue;
        s32 r=body_r(o), wr = r ? r>>1 : 0x1800, hit=0;
        s32 nx=o->x+o->kvx; if(!box_solid(nx,o->y,wr)) o->x=nx; else { hit=iabs(o->kvx); o->kvx=-o->kvx/3; }
        s32 ny=o->y+o->kvy; if(!box_solid(o->x,ny,wr)){ o->y=ny; } else { s32 h=iabs(o->kvy); if(h>hit) hit=h; o->kvy=-o->kvy/3; }
        if(hit>0xC00){ burst(o->x,o->y,0x3000,4,1); thump(1);
            if(o->kind==K_BARREL){ barrel_break(o); continue; }
            if(is_mon(o->kind)){ int dm=2+level/2+hit/0x800; add_fnum(o->x,o->y,dm,C_GOLD); o->hp-=dm; o->flash=6; if(o->hp<=0){ kill_mon(o); continue; } } }
        s32 sp=iabs(o->kvx)+iabs(o->kvy);
        if(sp>0x2000 && is_mon(o->kind) && !invuln && iabs(o->x-px_)<r+PRAD+0x2000 && iabs(o->y-py_)<r+PRAD+0x2000){
            int before=hearts; hurt(mon_dmg(o)*3/2); if(hearts<before){ hurt_flash=8; hitstop=5; kick_t=8; pknock_t=14; combo=0; pvx+=o->kvx; pvy+=o->kvy; say("RAMMED!"); } o->kvx/=4; o->kvy/=4; }
        if(sp>0x1200) for(int j=0;j<ndo;j++){ DObj *b=&dob[j]; if(j==i||!b->alive||!is_mon(b->kind)) continue;
            s32 rr=(r?r:0x2000)+body_r(b); if(iabs(b->x-o->x)>=rr||iabs(b->y-o->y)>=rr) continue;
            int dm=3+level/2; b->kvx+=o->kvx/2; b->kvy+=o->kvy/2; b->stag=4; o->kvx/=3; o->kvy/=3; thump(1);
            hit_mon(b,dm,0); if(o->kind==K_BARREL){ barrel_break(o); } else if(is_mon(o->kind)) hit_mon(o,dm/2+1,0);
            break; }
        if(!o->alive) continue;
        o->kvx=o->kvx*3/4; o->kvy=o->kvy*3/4; if(iabs(o->kvx)<0x80) o->kvx=0; if(iabs(o->kvy)<0x80) o->kvy=0;
    }
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
// Your own movement runs every 60th of a second. Everything else in the dungeon (monsters, hunger, cooldowns,
// timers, your pet) ticks once every WDIV of those, which is the pace the game was balanced for (about 15 a second).
#define WDIV       4
#define WALK_FWD   0x0700      // per 60th: about 1.6 tiles a second
#define WALK_BACK  0x0500
#define WALK_SIDE  0x0580      // strafing
#define TURN_MIN   4           // a tap turns finely...
#define TURN_MAX   9           // ...holding speeds up to this
#define TURN_RAMP  16          // 60ths of holding to reach TURN_MAX

static int skip_draw=0, wtick=1, turn_held=0, hud_f=0, dt_last=0, wacc=0, mv_now=0, near_cage_p=-1, atk_len=9;
static DObj *named_p=0; static u32 dhud_sig=0;
// is there something to bite right in front of you? (same box the bite uses)
static int foe_ahead(s32 dirx, s32 diry){
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue;
        s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16);
        s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(side<0) side=-side;
        s32 rng_=o->kind==K_BOSS?0x22000:0x1A000;
        if(fwd>0 && fwd<rng_ && side<0xE000) return 1; }
    return 0;
}
static void do_bite(s32 dirx, s32 diry){
    atk_len=(14-d_haste)*2/3; if(atk_len<4) atk_len=4; atk_t=atk_len;        // was 14 (min 6): a bite every 0.9 s was slow
    int crit; int pw=bite_power(&crit); sfx(0);
    int landed=0, killed=0, finisher = combo>=2;          // every third bite in a row is a CRUNCH: double power, big shove
    if(finisher){ pw*=2; atk_t=atk_len+3; }
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!(is_mon(o->kind)||o->kind==K_BARREL)) continue;
        s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16);
        s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(side<0) side=-side;
        s32 rng_=o->kind==K_BOSS?0x22000:0x1A000;
        if(fwd>0 && fwd<rng_ && side<0xE000){
            landed=1; knock(o,dirx,diry,(finisher?0x4000:crit?0x1800:0x0C00)*(strong_t?3:2)/2);
            if(o->kind==K_BARREL){ o->flash=6; sfx(13); debris(o->x,o->y,0x4000,3,2,dirx>>4,diry>>4); if(--o->hp<=0) barrel_break(o); continue; }
            int p2=pw; if(o->stag || o->wind){ p2=p2*3/2; }      // a staggered or winding-up foe takes extra
            o->stag = o->kind==K_BOSS ? (crit||finisher?3:0) : (finisher?8:crit?5:3); o->wind=0;
            hit_mon(o,p2,crit||finisher); if(!o->alive) killed=1; }
    }
    if(landed){ hitstop = finisher ? 8 : killed ? 7 : crit ? 6 : 3; if((crit||finisher) && opt_shake) shake_t=2;
        if(finisher){ combo=0; say("CRUNCH!"); sfx(12); } else { combo++; combo_t=50; } }
    else if(finisher) combo=0;
}
static int dungeon_frame_(void);
static int dungeon_frame(void){ u32 t=TMR(); int r=dungeon_frame_(); prof[5]=TMR()-t; return r; }
static int dungeon_frame_(void){   // returns 1 to go back up, 2 when you faint, 3 when you warp out
    int freeze = hitstop>0; if(freeze){ hitstop--; wtick=0; }      // a blink of stillness when a hit lands
    if(wtick){ tick_timers(); hunger_tick(40); regen_tick(45); map_reveal(); }
    bag_input();
    if(!skip_draw){ int f=frames; int fl=(sin_tab[(f*29)&255]+sin_tab[(f*47+80)&255])>>12;
      for(int i=0;i<8;i++){ int k=i+(fl>2?1:fl<-2?-1:0); if(k<0)k=0; if(k>7)k=7; BG_PAL[R_FLAME+i]=bg_pal[R_FLAME+k]; }
      for(int i=2;i<7;i++){ int k=i+((((f>>2)+i)&3)==0); BG_PAL[R_WATER+i]=bg_pal[R_WATER+(k>7?7:k)]; BG_PAL[R_CRYSTAL+i]=bg_pal[R_CRYSTAL+((((f>>3)+i*3)%5)==0 && i<6 ? i+1 : i)]; } }
    if(HIT(K_START)){ oam_hide_all(); oam_flush(); char_screen(VISPG); dungeon_hud_both(); }
    int kl=K_LEFT, kr=K_RIGHT, ku=K_UP, kd=K_DOWN, ksl=K_L, ksr=K_R;
    if(dizzy_t){ kl=K_RIGHT; kr=K_LEFT; ku=K_DOWN; kd=K_UP; ksl=K_R; ksr=K_L; }
    if(HELD(K_SEL)){ kl=kr=ku=kd=ksl=ksr=0; }          // SELECT held: d-pad and L/R pick items and spells instead
    if(HELD(kl)||HELD(kr)){ if(turn_held<TURN_RAMP) turn_held++; } else turn_held=0;
    int ts = TURN_MIN + (((TURN_MAX-TURN_MIN)*turn_held)>>4);
    if(HELD(kl)) pang=(pang-ts)&1023;
    if(HELD(kr)) pang=(pang+ts)&1023;
    s32 dirx=cosx(pang>>2)<<2, diry=sinx(pang>>2)<<2;
    s32 plx=-(diry*43/65), ply=dirx*43/65;
    s32 spf = HELD(ku) ? WALK_FWD : HELD(kd) ? -WALK_BACK : 0;
    s32 sps = HELD(ksr) ? WALK_SIDE : HELD(ksl) ? -WALK_SIDE : 0;
    if(spf && sps){ spf=spf*3/4; sps=sps*3/4; }          // diagonals are not faster
    spf = spf*(100+d_spd+have_bug[BUG_CRICKET]*25)/100; sps = sps*(100+d_spd+have_bug[BUG_CRICKET]*25)/100;
    if(speed_t){ spf=spf*8/5; sps=sps*8/5; }
    int sp = (spf||sps);
    s32 tvx=0, tvy=0;
    if(sp){ fish_state=0; tvx=(s32)((((long long)dirx*spf)-((long long)diry*sps))>>16); tvy=(s32)((((long long)diry*spf)+((long long)dirx*sps))>>16); }
    // double-tap L or R: a quick dodge dash to that side. you can't be hit while you dash
    if(!dizzy_t && !HELD(K_SEL)){ if(HIT(K_L)){ if(tapL_t && !dodge_t){ dodge_t=9; dodge_dir=-1; invuln=14; sfx(11); dodge_msg_t=30; } tapL_t=14; }
                                  if(HIT(K_R)){ if(tapR_t && !dodge_t){ dodge_t=9; dodge_dir=1; invuln=14; sfx(11); dodge_msg_t=30; } tapR_t=14; } }
    if(tapL_t) tapL_t--; if(tapR_t) tapR_t--; if(dodge_msg_t) dodge_msg_t--;
    if(dodge_t){ dodge_t--; s32 ds=0x1400*dodge_dir; tvx=(s32)((-(long long)diry*ds)>>16); tvy=(s32)(((long long)dirx*ds)>>16); pvx=tvx; pvy=tvy; }
    if(wtick && combo_t && --combo_t==0) combo=0;
    if(!freeze){ player_move(tvx,tvy); bodies_tick(); knock_tick(); if(kick_t) kick_t--; if(bump_cd) bump_cd--; if(pknock_t) pknock_t--; }
    if(wtick) mon_bodies();
    mv_now = sp;
    if(wtick && atk_t) atk_t--;
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
    if(wtick && pet_here){
        int tg=-1; s32 best=0x48000;
        for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue;
            if(dist8(o->x-px_,o->y-py_)>0x50000) continue; s32 d=dist8(o->x-pet_x,o->y-pet_y); if(d<best){ best=d; tg=i; } }
        s32 tx,ty;
        if(tg>=0){ tx=dob[tg].x-(diry*3/8); ty=dob[tg].y+(dirx*3/8); if(solid(tx,ty)){ tx=dob[tg].x+(diry*3/8); ty=dob[tg].y-(dirx*3/8); } } else {      /* Buddy goes for the flank, so it doesn't block your bite */ tx=px_-((dirx*14)>>4)+((-diry)>>2); ty=py_-((diry*14)>>4)+(dirx>>2); }
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
    if(wtick && pet_down_t && --pet_down_t==0){ pet_hp=pet_maxhp/2; pet_x=px_; pet_y=py_; say("BUDDY IS BACK!"); }
    // ---- monsters, pickups, cages
    s32 named_d=0x60000; if(wtick){ near_cage_p=-1; named_p=0; }
    for(int i=0;wtick && i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; if(o->flash) o->flash--;
        s32 dx=px_-o->x, dy=py_-o->y, d=dist8(dx,dy);
        if(is_mon(o->kind)){
            int boss=o->kind==K_BOSS, bat=o->kind==K_BAT, wasp=o->kind==K_WASP;
            s32 contact = 0x7800 + body_r(o), reach = contact + 0x2800;     // they walk up until they touch you, then bite
            if((o->champ||boss) && d<named_d){ named_p=o; named_d=d; }
            if(o->venom){ o->venom--; if((o->venom%15)==0){ int vd=1+d_venom/3; o->hp-=vd; add_fnum(o->x,o->y,vd,C_GREEN); if(o->hp<=0){ kill_mon(o); continue; } } }
            if(blast_flag && d<0x30000){ hit_mon(o, 8+level*2, 0); if(!o->alive) continue; }
            // go after whoever is closer: you or Buddy
            s32 pdx=pet_x-o->x, pdy=pet_y-o->y, pd = pet_here ? dist8(pdx,pdy) : 0x7FFFFFFF;
            int at_pet = pd < d;
            s32 tdx = at_pet?pdx:dx, tdy = at_pet?pdy:dy, td = at_pet?pd:d;
            if(at_pet){ contact = 0x2800 + body_r(o); reach = contact + 0x2800; }
            if(o->stag) o->stag--;
            if(!stun_t && !o->stag && !o->wind && td<0x70000){
                s32 st = boss?0x0500 : bat?(0x0700+((frames+i*9)%24<12?0x400:0)) : o->kind==K_BEETLE?0x0380 : 0x0600;
                int away = wasp && td<0x18000, keep = wasp && td<0x40000 && td>=0x18000;
                if(!keep && (td>contact+0x800 || away)){
                    s32 stx=(tdx>0?st:-st), sty=(tdy>0?st:-st); if(away){ stx=-stx; sty=-sty; }
                    if(!solid(o->x+stx*3,o->y)) o->x+=stx; if(!solid(o->x,o->y+sty*3)) o->y+=sty; }
            }
            if(o->cool) o->cool--;
            if(wasp && !stun_t && !o->cool && d<0x60000){        // wasps shoot stingers
                s32 l=isqrt((u32)((dx>>8)*(dx>>8)+(dy>>8)*(dy>>8))); if(l<1) l=1;
                fire_proj(1, o->x, o->y, 0x8000, (dx>>8)*0x1400/l, (dy>>8)*0x1400/l, mon_dmg(o)); o->cool=55+rndi(30); }
            if(o->wind){                                    // winding up: it stands still, then snaps
                if(--o->wind==0){
                    if(td<reach+0x3000){
                        if(td>0x100){ o->kvx += (s32)(((long long)tdx*(boss?0x1400:0x0C00))/td); o->kvy += (s32)(((long long)tdy*(boss?0x1400:0x0C00))/td); }   // it lunges
                        if(at_pet){ pet_hp-=mon_dmg(o)/2; petobj.flash=6; }
                        else { if(invuln && dodge_msg_t){ say("DODGED!"); st_dodges++; gain_xp(1); }
                               else if(!invuln){ int before=hearts; hurt(mon_dmg(o)); if(hearts<before){ hurt_flash=8; hitstop=5; kick_t=7; pknock_t=12; combo=0;
                                   s32 kp = boss?0x2400 : o->kind==K_BEETLE?0x1A00 : 0x1000; if(d>0x100){ pvx+=(s32)(((long long)dx*kp)/d); pvy+=(s32)(((long long)dy*kp)/d); } } }
                               int ret=d_reflect+d_sting; if(ret){ hit_mon(o,ret,0); if(!o->alive) continue; } }
                    } else { o->stag=3; }                      // it snapped at empty air and is open for a moment
                }
            }
            else if(!wasp && !stun_t && !o->stag && td<reach && !o->cool){ o->cool=boss?34:46; o->wind = boss?3:o->kind==K_BEETLE?6:4; sfx(14); }
            // beetles charge from a distance: a long wind-up, then they barrel at you. into a wall, and they are stunned
            else if(o->kind==K_BEETLE && !stun_t && !o->stag && !o->cool && td>0x18000 && td<0x50000 && rndi(40)==0){ o->cool=70; o->wind=8; o->lunge=1; sfx(14); }
            if(o->lunge && !o->wind){ o->lunge=0; if(td>0x100){ o->kvx=(s32)(((long long)tdx*0x3000)/td); o->kvy=(s32)(((long long)tdy*0x3000)/td); } }
            // the boss calls her brood at half health, and gets angry
            if(boss && !o->ph && o->hp<o->maxhp/2){ o->ph=1; say("THE QUEEN CALLS HER BROOD!"); sfx(9);
                for(int k=0;k<2 && ndo<NDO;k++){ DObj *b=&dob[ndo++]; memset(b,0,sizeof(DObj)); b->kind=K_SPIDER; b->alive=1; b->hp=b->maxhp=6+dlv()*4; b->x=o->x+((k?1:-1)<<16); b->y=o->y+(1<<15); if(solid(b->x,b->y)){ b->x=o->x; b->y=o->y; } } }
        } else if(o->kind==K_CHEST){ if(d<0x9000){ o->alive=0; earn(10+rndi(20)+dlv()*8+depth_no*3); say("TREASURE!"); sfx(7);
                Gear g=roll_drop(dlv()*3+depth_no+2,15); add_gear(g); hud_now(); } }
        else if(o->kind==K_ITEM){ if(d<0x9000 && (bag_n<BAGMAX || o->it==IT_QUEST)){ Gear g={o->it,o->sub,o->rar,o->pre,o->suf,o->lvl}; if(add_gear(g)) o->alive=0; } }
        else if(o->kind==K_CAGE){ if(d<0xC000) near_cage_p=i; }
    }
    int near_cage=near_cage_p; DObj *named=named_p;
    if(near_cage>=0 && !(dob[near_cage].alive && dob[near_cage].kind==K_CAGE)) near_cage=-1;
    if(named && !named->alive) named=0;
    if(wtick) blast_flag=0;
    if(call_flag){ call_flag=0; for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].kind==K_CAGE){ dob[i].alive=0; free_bug(dob[i].sub); break; } }
    // ---- fireballs and stingers
    for(int i=0;wtick && i<NPROJ;i++){ Proj *p=&proj[i]; if(!p->alive) continue;
        p->x+=p->vx; p->y+=p->vy; if(--p->life<=0 || solid(p->x,p->y)){ p->alive=0; burst(p->x-p->vx,p->y-p->vy,p->h,p->kind?4:10,0); continue; }
        if(p->kind==0){ if(frames&1) burst(p->x,p->y,p->h,1,0);
            for(int k=0;k<ndo;k++){ DObj *o=&dob[k]; if(!o->alive||!(is_mon(o->kind)||o->kind==K_BARREL)) continue;
                if(dist8(o->x-p->x,o->y-p->y)<0x9000){ o->kvx+=p->vx; o->kvy+=p->vy; p->alive=0; burst(p->x,p->y,p->h,12,0); hitstop=2;
                    if(o->kind==K_BARREL) barrel_break(o); else hit_mon(o,p->dmg,0); break; } } }
        else if(dist8(px_-p->x,py_-p->y)<0x7000){ p->alive=0; int before=hearts; hurt(p->dmg); if(hearts<before){ hurt_flash=8; kick_t=4; pknock_t=6; pvx+=p->vx>>1; pvy+=p->vy>>1; } }
    }
    // ---- A: open a cage, take the stairs, fish, or bite
    int here=dmap[(py_>>16)*MW+(px_>>16)];
    if(here!=D_UP) stair_ok=1;           // you arrive standing on the up stairs: step off once before they work
    if(wtick && fish_state==1 && --fish_t<=0){ fish_state=2; fish_t=14; sfx(8); say("! A BITE ! PRESS A"); }
    else if(wtick && fish_state==2 && --fish_t<=0){ fish_state=0; say("IT GOT AWAY"); }
    int foe = foe_ahead(dirx,diry);
    if(HIT(K_A) && fish_state==2){ fish_state=0; int r=rndi(100), f = r<45?0 : r<62?1 : r<79?2 : r<88?3 : 4; add_item(IT_FISH,f); st_fish++; ach_check(); sfx(8); }
    else if(HELD(K_A) && foe){ if(!atk_t) do_bite(dirx,diry); }          // something in front always gets bitten, and holding A keeps biting
    else if(HIT(K_A)){
        if(near_cage>=0){ dob[near_cage].alive=0; free_bug(dob[near_cage].sub); }
        else if(here==D_UP && stair_ok) return 1;
        else if(here==D_DOWN){ wacc=WDIV-1; fade(1); dungeon_enter(); music(boss_floor()?4:1); pvx=pvy=0; hitstop=kick_t=pknock_t=0; fade(0); return 0; }
        else if(fish_state==1){ fish_state=0; say("YOU REEL IN"); }
        else if(water_ahead(dirx,diry)){ fish_state=1; fish_t=25+rndi(50); say("FISHING... WAIT FOR A BITE"); sfx(8); }
        else if(!atk_t) do_bite(dirx,diry);
    }
    if(hearts<=0) return 2;
    // ---- things that count down or drift: one tick per game step, drawn or not
    if(wtick){
        if(sp && moved_real){ int o_=bob_ph; bob_ph += speed_t?17:12; if(((o_+64)^(bob_ph+64))&128) sfx(11); }
        if(shake_t) shake_t--;
        if(msg_timer) msg_timer--;
        if(hurt_flash) hurt_flash--;
        if(spark_t) spark_t--;
        parts_update(); fnums_update();
    }
    if(skip_draw) return 0;
    // ---- draw into the back page
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
    hz = 89 + (sinI[bob_ph&255]>>13) + (shake_t ? ((shake_t&1)?3:-3) : 0) + (kick_t>>1);
    s32 cam_px=px_, cam_py=py_;
    { s32 amt = (atk_t*2>atk_len ? 0x1000 : 0) - kick_t*0x500, jit = (shake_t && opt_shake) ? ((frames&1)?0x800:-0x800) : 0;   // lunge, recoil, shake
      if(amt||jit){ s32 cx=px_+(s32)(((long long)dirx*amt)>>16)-(s32)(((long long)diry*jit)>>16), cy=py_+(s32)(((long long)diry*amt)>>16)+(s32)(((long long)dirx*jit)>>16);
          if(!box_solid(cx,cy,0x1000)){ px_=cx; py_=cy; } } }
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
    px_=cam_px; py_=cam_py;
    if(spark_t){ int c=60, y=140; for(int k=0;k<12;k++){ int c2=60+(spark_c-60)*(k+1)/12+(k<11?rndi(5)-2:0), y2=140+(spark_y-140)*(k+1)/12;
            int steps=(y-y2)>0?(y-y2):1; for(int s=0;s<=steps;s+=2){ int cc=c+(c2-c)*s/steps, yy=y-s; if(cc>=0&&cc<120&&yy>=18&&yy<160){ pg[yy*120+cc]=(C_WHITE)|(C_WHITE<<8); if(cc+1<120) pg[yy*120+cc+1]=(R_CRYSTAL+7)|((R_CRYSTAL+7)<<8);} }
            c=c2; y=y2; } }
    if(fish_state){ int by = 118 + (fish_state==2 ? 4 : (sinI[(frames*20)&255]>>12)); for(int y=100;y<by;y+=2) pg[y*120+66]=(R_STONE+6)|((R_STONE+6)<<8);
        for(int y=by;y<by+4;y++){ pg[y*120+66]=216|(216<<8); pg[y*120+67]=216|(216<<8); } }
    if(combo){ char cb[12]; scpy(cb,"COMBO X"); ncat(cb,combo+1); text(pg,120-textlen(cb)*2,100,cb,combo>=2?C_GOLD:C_WHITE,1); }
    if(dodge_msg_t) text(pg,120-5*2,108,"DODGE",C_CYAN,1);
#ifdef TEST_FOE
    { char db[40]; scpy(db,"ATK "); ncat(db,atk_t); scat(db," FOE "); ncat(db,foe); scat(db," CB "); ncat(db,combo); scat(db," CT "); ncat(db,combo_t); scat(db," HS "); ncat(db,hitstop); text(pg,4,60,db,C_WHITE,1);
      int dl=0; for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue; s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16); s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(fwd>0x40000||fwd<-0x40000||side>0x40000||side<-0x40000) continue; db[0]=0; scpy(db,"K"); ncat(db,o->kind); scat(db," F"); ncat(db,fwd>>8); scat(db," S"); ncat(db,side>>8); scat(db," W"); ncat(db,o->wind); scat(db," ST"); ncat(db,o->stag); text(pg,4,68+dl*7,db,C_WHITE,1); if(++dl>2) break; } }
#endif
    if(sight || have_bug[BUG_ANT] || stairs_seen()) compass(pg,dirx,diry);
    if(opt_minimap) minimap(pg);
    hud3d(pg);
    if(named){ char b[32]; b[0]=0; if(named->kind==K_BOSS) scpy(b, endless?(hive_boss_floor()?"HIVE OVERLORD":"GUARDIAN"):"THE QUEEN"); else if(named->champ==2) scpy(b,"QUEST TARGET"); else { scpy(b,champ_pre[named->cname&7]); scat(b," "); scat(b,kind_name[named->kind]); }
        int w=textlen(b)*4; text(pg,120-w/2,36,b,C_RED,1); int hw2 = named->hp*40/(named->maxhp?named->maxhp:1); for(int c=40;c<80;c++){ int ix=(c-40)<hw2/2*1+0 ? C_RED : C_DRED; if(c-40<hw2) ix=C_RED; pg[43*120+c]=ix|(ix<<8); } }
    if(msg_timer){ text(pg,120-textlen(msg)*4,24,msg,C_WHITE,2); }
    else if(!opt_hints){}
    else if(near_cage>=0) text(pg,120-12*4,24,"A: OPEN CAGE",C_WHITE,2);
    else if((here==D_UP && stair_ok)||here==D_DOWN){ const char *m2 = here==D_UP? "A: CLIMB UP" : "A: GO DEEPER"; text(pg,120-textlen(m2)*4,24,m2,C_WHITE,2); }
    else if(!fish_state && water_ahead(dirx,diry)) text(pg,120-9*4,24,"A: FISH",C_CYAN,2);
    if(shield_t && (frames&16)){ for(int c=0;c<120;c+=6){ pg[18*120+c]=(R_CRYSTAL+6)|((R_CRYSTAL+6)<<8); pg[159*120+c]=(R_CRYSTAL+6)|((R_CRYSTAL+6)<<8); } }
    if(hurt_flash){ for(int y=18;y<22;y++) for(int x=0;x<120;x++) pg[y*120+x]=C_RED|(C_RED<<8); }
    if(frames-hud_f>=15){ hud_f=frames; u32 hs=hud_sig(); if(hs!=dhud_sig){ dhud_sig=hs; dungeon_hud(); } }
    if(hud_dirty){ char lb[40]; dungeon_label(lb); draw_hud(pg,lb); hud_dirty--; }
    oam_hide_all();
    // your jaws: they sway as you walk, snap shut when you bite, and jolt when you are hit
    if(!(invuln && (frames&4))){
        int p = atk_t ? (atk_len-atk_t)*64/atk_len : 64;               // 0..64 through the bite
        int shut = p<26 ? p*64/26 : (64-p)*64/38;  if(!atk_t) shut=0;  // 0 open .. 64 closed
        int sway = (sinI[bob_ph&255]>>14) * (moved_real?3:1) + (sin_tab[(vbl_count*2)&255]>>13);   // walking sway, breathing
        int ang = (sway - shut*9/64) & 255;
        s32 c=cosx(ang)>>6, sn=sinx(ang)>>6;
        set_affine(30, c, -sn, sn, c); set_affine(29, -c, -sn, -sn, c);
        int dy = (sinI[(bob_ph*2)&255]>>13)*(moved_real?1:0) + kick_t*2 - shut*12/64 + 2, dx = shut*26/64;   // they slide in and up to snap shut
        shadow_oam[0].a0 = ((136-64+dy)&255) | (1<<8) | (1<<9); shadow_oam[0].a1 = ((52-64+dx)&511) | (30<<9) | (3<<14); shadow_oam[0].a2 = T_JAW;
        shadow_oam[1].a0 = ((136-64+dy)&255) | (1<<8) | (1<<9); shadow_oam[1].a1 = ((188-64-dx)&511) | (29<<9) | (3<<14); shadow_oam[1].a2 = T_JAW;
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
    enum { TI_CONT, TI_NEW, TI_LOAD, TI_HELP, TI_OPT };
    static const char *const tn[5]={"CONTINUE","NEW GAME","LOAD GAME","HOW TO PLAY","OPTIONS"};
    volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i];
    bug_build();
    hdma_on=0; title_paint(pg);
    REG_BLDCNT = 0x3F | (3<<6); REG_BLDY = 16;
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    music(2);
    int sel=0, blink=0, dirty=1, full=1, n=0, cs=-1; int it[5];
    wait_release();
    int fading_in=1;
    while(1){
        if(full){ full=0; title_paint(pg); slots_refresh(); cs=first_good_slot();
            n=0; if(cs>=0) it[n++]=TI_CONT; it[n++]=TI_NEW; if(cs>=0) it[n++]=TI_LOAD; it[n++]=TI_HELP; it[n++]=TI_OPT;
            if(sel>=n) sel=0; dirty=1; }
        if(dirty){ dirty=0; char b[40];
            title_restore(pg,56,128);
            for(int i=0;i<n;i++){ int y=62+i*11; const char *nm=tn[it[i]]; int x=120-textlen(nm)*2;
                if(i==sel){ rect(pg,66,y-2,108,10,6); text(pg,x,y,nm,C_GOLD,1); text(pg,70,y,">",C_GOLD,1); text(pg,166,y,"<",C_GOLD,1); }
                else stext(pg,x,y,nm,C_WHITE); }
            int y=62+n*11+3; b[0]=0;
            if(it[sel]==TI_CONT){ const SlotInfo *o=&sinf[cs]; char w[28]; slot_where(o,w); scpy(b,"SLOT "); ncat(b,cs+1); scat(b,"  LV "); ncat(b,o->level); scat(b,"  "); scat(b,w); }
            else if(it[sel]==TI_NEW) scpy(b,"START A NEW ADVENTURE");
            else if(it[sel]==TI_LOAD) scpy(b,"PICK ONE OF YOUR 3 SAVE SLOTS");
            else if(it[sel]==TI_HELP) scpy(b,"THE BUTTONS, AND TIPS");
            else scpy(b,"SOUND, AUTOSAVE AND MORE");
            b[34]=0; stext(pg,120-textlen(b)*2,y,b,C_CYAN);
        }
        oam_hide_all(); set_affine(31,128,0,0,128);
        shadow_oam[0].a0=((150-32-((blink>>3)&1))&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[0].a1=((120-32)&511)|(31<<9)|(2<<14); shadow_oam[0].a2=bug_tile((blink>>4)&1)|BUG_BANK;
        vsync(); oam_flush(); read_keys(); blink++; rng_s += blink;
        if(fading_in){ fading_in=0; fade(0); }
        twinkle(pg,blink);
        if(HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; sfx(7); }
        if(HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; sfx(7); }
        if(HIT(K_A)||HIT(K_START)){
            int a=it[sel]; sfx(2);
            if(a==TI_CONT){ cur_slot=cs; return 1; }
            if(a==TI_NEW){ int s2=slot_picker(pg,SP_NEW,0); if(s2>=0){ cur_slot=s2; oam_hide_all(); oam_flush(); if(make_bug(pg,1)) return 0; } full=1; }
            if(a==TI_LOAD){ int s2=slot_picker(pg,SP_LOAD,0); if(s2>=0){ cur_slot=s2; return 1; } full=1; }
            if(a==TI_OPT){ oam_hide_all(); oam_flush(); options_screen(pg); full=1; }
            if(a==TI_HELP){ oam_hide_all(); oam_flush(); help_screen(pg); full=1; }
            wait_release();
        }
    }
}

// ================================================================= starting, running and leaving a game
static void start_game(int cont){       // fades out, sets the world up from cur_slot (or fresh) and fades in on the planet
    rng_s ^= vbl_count*2654435761u;
    fade(1);
    gen_cluster();
    if(!(cont && load_game(1))){ cont=0; new_game(); cam_fp=opt_cam; planet_tables(); gen_cluster(); load_ext_defaults(); play_reset(0); do_save(); }
    scene=0; launch_flag=0; enter_dungeon_flag=0; fish_state=0; msg_timer=0; invuln=0; shake_t=0; page_back=0; loot=0; depth_no=0;
    speed_t=strong_t=dizzy_t=stun_t=shield_t=0;
    restock(); arrow_ship = zone_cleared && !endless;
    new_planet(); planet_enter(); show_msg(cont ? "WELCOME BACK" : "THE GARDEN: FIND THE DOOR");
    oam_hide_all(); oam_flush(); fade(0);
    music(0);
    if(!cont){ static const char *const t[6]={"YOU ARE A LITTLE PILL BUG.","THE ANT KING HAS CAGED YOUR BUG FRIENDS.","","FOLLOW THE YELLOW ARROW TO THE DOOR.","GO DOWN. BEAT THE BOSS AT THE BOTTOM.","THEN FLY YOUR SHIP TO THE NEXT STAR."};
        card(VISPG,"BUGSTAR",t,6); planet_hud(); show_msg("THE GARDEN: FIND THE DOOR"); }
#ifdef TEST_DUNGEON
    enter_dungeon_flag=1;
#endif
#ifdef TEST_CREDITS
    fade(1); credits(); planet_enter(); fade(0);
#endif
#ifdef TEST_BOSS
    zone=TEST_BOSS; portal_depth=zone_floors(); st_str=300; recalc(); hearts=maxh;
#endif
}
static void arrive_on_planet(void){      // after landing (or the autopilot): set the surface up
    land_on_current(); st_planets++; ach_check(); portal_depth=0; scene=0; msg_timer=0;
    new_planet(); planet_enter(); arrow_ship=0; planet_hud(); restock(); save_game();
    char b[40]; scpy(b,world_name()); if(!endless && !zone_cleared) scat(b,": FIND THE DOOR"); show_msg(b); oam_hide_all(); oam_flush(); music(0);
}
static void pet_away_tick(void){
    if(pet_away_t && --pet_away_t==0){ gold+=pet_sold_g; char b[40]; scpy(b,"BUDDY SOLD "); ncat(b,pet_sold_n); scat(b," THINGS: +"); ncat(b,pet_sold_g); scat(b,"G"); say(b); sfx(7); hud_now(); pet_x=px_; pet_y=py_; }
}
// plays until the player chooses QUIT TO TITLE
static void run_game(void){
    while(1){
        read_keys(); if(scene!=1){ frames++; if(invuln) invuln--; }
        if(sys_req){ int rq=sys_req; sys_req=0; if(rq==2) return; start_game(1); continue; }
        rng_s += keys;
        if(scene!=1) pet_away_tick();
        if(scene==2){
            int r=space_frame();
            vsync();
            REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1;
            if(r==2) galaxy_map();
            else if(r==3){ station_menu(); save_game(); }
            else if(r==1){ fade(1); arrive_on_planet(); fade(0); }
            continue;
        }
        if(scene==0){
            planet_frame();
            vsync(); REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1; oam_flush();
            if(launch_flag==2){ launch_flag=0; fade(1); oam_hide_all(); oam_flush();
                int d=sys_dist(cur_sys,zone+1); fuel = d>fuel ? 0 : fuel-d; cur_sys=zone+1; visits++;
                scene=2; page_back=1; REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); REG_BLDY=0; hyperspace();
                arrive_on_planet(); fade(0); continue; }
            if(launch_flag){ launch_flag=0; fade(1); oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(3); fade(0); continue; }
            if(enter_dungeon_flag){ enter_dungeon_flag=0; fade(1); oam_hide_all(); oam_flush();
                depth_no=0; if(portal_depth>0){ depth_no=portal_depth-1; portal_depth=0; }
                scene=1; dungeon_enter();
#ifdef TEST_FOE
                for(int k=0;k<2 && ndo<NDO;k++){ DObj *b=&dob[ndo++]; memset(b,0,sizeof(DObj)); b->kind=k?K_BEETLE:K_SPIDER; b->alive=1; b->hp=b->maxhp=k?40:20; b->x=px_+(cosx(pang>>2)<<(k?4:3)); b->y=py_+(sinx(pang>>2)<<(k?4:3)); }
#endif
#ifdef TEST_BOSS
                for(int i=0;i<ndo;i++) if(dob[i].kind==K_BOSS){ dob[i].x=px_+(cosx(pang>>2)<<3); dob[i].y=py_+(sinx(pang>>2)<<3); }
#endif
                pvx=pvy=0; hitstop=kick_t=pknock_t=0; page_back=1; music(boss_floor()?4:1); fade(0); wacc=WDIV-1; dt_last=vbl_count; }
        } else {
            int r=0;
            int n = vbl_count - dt_last; dt_last = vbl_count;          // 60ths of a second since the last picture
            if(n>12) n = (n>30) ? 1 : 12;                              // after a menu or a load, don't fast-forward
            if(n<1) n=1;
            for(int i=0;i<n;i++){
                wtick = (++wacc>=WDIV);
                if(wtick){ wacc=0; frames++; if(invuln) invuln--; pet_away_tick(); }
                if(i) keys_old=keys;
                skip_draw = (i<n-1);
                r=dungeon_frame();
                if(r) break;
            }
            skip_draw=0; wtick=1;
            vsync();
            if(scene==1){ REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1; }
            oam_flush();
            if(r){ fade(1); oam_hide_all(); oam_flush(); scene=0; msg_timer=0; fish_state=0;
                   char mb[40];
                   if(r==2){ recalc(); hearts=maxh; food=60; if(!endless) gold/=2; loot=0; if(!opt_easy){ bag_n=0; bag_sel=0; } portal_depth=0; scpy(mb, opt_easy ? "YOU FAINTED. BUDDY DRAGGED YOU OUT" : endless ? "YOU FAINTED. LOOT AND BAG LOST" : "YOU FAINTED. YOU LOST YOUR BAG"); }
                   else if(endless){ int bonus = loot*(100+depth_no*15)/100; gold+=bonus; loot=0; scpy(mb,"ESCAPED B"); ncat(mb,depth_no); scat(mb,": +"); ncat(mb,bonus); scat(mb,"G"); }
                   else if(r==3 && portal_depth) scpy(mb,"PORTAL OPEN: THE DOOR TAKES YOU BACK");
                   else scpy(mb, zone_cleared?"ZONE CLEARED! GO TO YOUR SHIP":"BACK ON THE SURFACE");
                   restock(); save_game();
                   planet_enter(); arrow_ship = zone_cleared && !endless; show_msg(mb); oam_flush(); music(0); fade(0);
                   if(zone_card){ int z=zone_card-1; zone_card=0;
                       if(z==5){ fade(1); credits(); planet_enter(); fade(0); }
                       else { static const char *const t[6]={"A NEW STAR IS OPEN.","","1. GO TO YOUR SHIP. FOLLOW THE ARROW.","2. PICK LAUNCH INTO SPACE.","3. PRESS SELECT FOR THE GALAXY MAP.","4. PICK THE NEW STAR AND PRESS A."};
                           card(VISPG,"BOSS BEATEN! ZONE CLEARED",t,6); }
                       planet_hud(); show_msg("GO TO YOUR SHIP"); } }
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
        new_game(); bug_look_default();
        int cont=title_screen();
        start_game(cont);
        in_game=1;
        run_game();
        fade(1); oam_hide_all(); oam_flush(); music(-1);
        REG_BLDCNT=0; REG_BLDY=0; REG_DISPCNT=0x80;      // dark, until the title is painted
    }
}
