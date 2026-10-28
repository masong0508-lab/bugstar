#ifndef DBGFPS
#define DBGFPS 0
#endif
// BUGSTAR prototype: a pill bug on a turning planet, and a colour 3D dungeon underneath.
#include "assets.h"
#include "logo.h"
typedef unsigned char u8; typedef unsigned short u16; typedef unsigned int u32; typedef signed short s16; typedef signed char s8; typedef int s32; typedef unsigned long long u64; typedef long long s64;
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
static int film_mode=0;                               // the debug menu's HIDE THE HUD: no HUD anywhere, only the world
#ifdef TEST_FILM
#define FILM 1                                        // (a build for filming the trailer: always)
#else
#define FILM film_mode
#endif
// ---- the debug menu (the Konami code on the title screen): what it asks of the game
static u32 g_w0,g_w1,g_w2,pp[6],pq;
#ifdef SHOW_PROF
#define PPT(i) do{ u32 t_=TMR(); pp[i]=t_-pq; pq=t_; }while(0)
#else
#define PPT(i) do{}while(0)
#endif
static int dbg_cls=-1, dbg_weird=-1, dbg_hour=-1, dbg_weather=-1, dbg_god=0, dbg_fps=DBGFPS;
// ---- the kart races' hardware ground (m7.inc): on, and the first screen row the ground can use (the rows above come from page 0)
static int m7_on=0, m7_strip=160, m7_snap=0; static s32 m7_cb_snap=0; static void m7_pix(int x, int y, int c); static void m7_span(int y, int x0, int x1, int c); IWRAM static void m7_irq(void); static void m7_after_vsync(void);

void *memset(void *d, int c, unsigned n){ u8 *p=d; while(n--) *p++=c; return d; }
void *memcpy(void *d, const void *s, unsigned n){ u8 *p=d; const u8 *q=s; while(n--) *p++=*q++; return d; }

static u32 keys, keys_old;
static u32 rng_s = 0x1234567;
static u32 rnd(void){ rng_s ^= rng_s<<13; rng_s ^= rng_s>>17; rng_s ^= rng_s<<5; return rng_s; }
static int rndi(int n){ return (int)(rnd()%(u32)n); }
static void read_keys(void){ keys_old=keys; keys = ~REG_KEYINPUT & 0x3FF; }
#define HELD(k) (keys&(k))
#define HIT(k) ((keys&(k)) && !(keys_old&(k)))
static int auto_run=0, tapU_t=0, run_stuck=0, run_hint=0;   // AUTO-RUN in dungeons: double-tap UP, and you keep running until you stop it
static void vsync(void){ while(REG_VCOUNT>=160); while(REG_VCOUNT<160); }
static void vsync_flip(void){ if(REG_VCOUNT>=160) return; while(REG_VCOUNT<160); }   // dungeon only (it runs on real time): late but still inside the blank = flip now, no tearing, no lost frame
static int isqrt(u32 n){ u32 r=0, b=1u<<30; while(b>n) b>>=2; while(b){ if(n>=r+b){ n-=r+b; r=(r>>1)+b; } else r>>=1; b>>=2; } return (int)r; }
#define S64_MAX 0x7FFFFFFFFFFFFFFFLL
static s64 sadd(s64 a, s64 b){ if(b>0){ if(a>S64_MAX-b) return S64_MAX; } else if(b<0){ if(a<-S64_MAX-1-b) return -S64_MAX-1; } return a+b; }
static s64 smul(s64 a, s64 b){ if(a<=0||b<=0) return 0; u64 ua=(u64)a, ub=(u64)b; u32 ah=(u32)(ua>>32), al=(u32)ua, bh=(u32)(ub>>32), bl=(u32)ub;
    if(ah && bh) return S64_MAX; u64 hi=(u64)ah*bl+(u64)al*bh; if(hi>>31) return S64_MAX;
    u64 lo=(u64)al*bl, r=(hi<<32)+lo; if(r<lo || r>(u64)S64_MAX) return S64_MAX; return (s64)r; }
static s64 sdiv(s64 n, u32 d){ int neg = n<0; u64 un = neg ? (u64)(-(n+1))+1 : (u64)n; u32 hi=(u32)(un>>32), lo=(u32)un, qh=hi/d, r=hi%d, ql=0;
    for(int i=31;i>=0;i--){ u32 top=r>>31; r=(r<<1)|((lo>>i)&1); ql<<=1; if(top||r>=d){ r-=d; ql|=1; } }
    u64 q=((u64)qh<<32)|ql; return neg ? -(s64)q : (s64)q; }
static int clampi(s64 v){ return v>1000000000LL ? 1000000000 : v<-1000000000LL ? -1000000000 : (int)v; }
static int ratio(s64 a, s64 b, int scale){ if(b<=0||a<=0) return 0; if(a>=b) return scale; while(b>0x3FFFFFFLL){ a>>=1; b>>=1; } if(b<1) b=1; return (int)(((u32)a*(u32)scale)/(u32)b); }
static int sinx(int a){ return sin_tab[a&255]; }
static int cosx(int a){ return sin_tab[(a+64)&255]; }

// ---------------------------------------------------------------- shared state
static s64 hearts=5, gold=0; static int crew=0, frames=0, planet_no=0, depth_no=0, invuln=0;
static volatile u16 *page;         // the page we draw into (mode 4)
static int page_back=0;
// options, save slots and menu requests (see save.inc)
static volatile int opt_music=2;   // 0 off, 1 low, 2 full
static volatile int opt_sfx=1;
static int opt_autosave=1, opt_shake=1, opt_easy=0, opt_diff=1, opt_insane=0, opt_cam=0, opt_minimap=1, opt_nums=1, opt_hints=1, opt_arrow=1, opt_baby=0, opt_hunger=1, opt_faintloss=1, opt_xpm=0, opt_goldm=0;   // easy = (opt_diff==0)
static int cur_slot=0, last_slot=0;   // the slot you are playing, and the one you used last
static int in_game=0;
static int sys_req=0, cluster_no=0;                 // cluster_no: how many intergalactic jumps you have made (it sets the danger)
// where you are: a universe of 2^155 x 2^155 clusters of 32 stars (see space.inc), and after the story any of 2^128 universes (uni_w; galaxy_seed is the value the generators mix).
// The address arrays below are bigger than that: they are what older saves used (a 320-bit galaxy number and 1024 cluster words).
// u_settle folds any old address into today's universe, so every word above clus[0..9] is zero in play. The upper words live in spare SRAM (see save.inc),
// and a world's land and parts are hashed from the whole address (see gen_terrain and make_world).
#define NCW0 32                                                   // the cluster words the older saves and the hash walk know about
#define NCW 32                                                    // cluster words kept in EWRAM (saves only store NCW0 of them; the old 1024 wasted 4 KB of EWRAM)
_Static_assert(NCW>=NCW0, "clus must hold every word a save stores");
static u32 galaxy_no=0; static u32 clus[NCW] __attribute__((section(".ewram")));
static u32 clus_f=0, clus_up=0;                                   // the upper cluster words folded into one, and whether any of them is set (see clus_refold)
#define NGH 9
static u32 galaxy_hi[NGH];                  // the galaxy number is 320 bits: galaxy_no is the low word, galaxy_hi the nine words above it (all zero = the old 32-bit galaxies)
static u32 relic_bits=0; static int weird_seen=0, relic_gold=0;      // the legendary relics you own (one bit each), weird worlds visited
static u32 hmix(u32 h);
static u32 gal_fold(void){ u32 f = (galaxy_hi[0]|galaxy_hi[1]|galaxy_hi[2]) ? hmix(galaxy_hi[0]^hmix(galaxy_hi[1]^hmix(galaxy_hi[2]^0x68E31DA4u))) : 0;   // the upper galaxy words folded into one; 0 for the old galaxies, so their worlds do not change
    for(int i=3;i<NGH;i++) if(galaxy_hi[i]) f=hmix(f^galaxy_hi[i]^(u32)i*0x27D4EB2Fu); return f; }
static int home_cluster(void){ u32 o=galaxy_no; for(int i=0;i<NGH;i++) o|=galaxy_hi[i]; for(int i=0;i<NCW0;i++) o|=clus[i]; return !(o|clus_up); }
static u32 aword(int i, int star){ i%=NCW0+2; return i==0 ? (galaxy_no ^ gal_fold() ^ clus_f) : i==NCW0+1 ? (u32)star : clus[i-1]; }
static u32 hmix(u32 h){ h^=h>>16; h*=0x85EBCA6Bu; h^=h>>13; h*=0xC2B2AE35u; h^=h>>16; return h; }
static u32 amix(u32 k, int star){ u32 h=0x811C9DC5u ^ k*0x9E3779B1u; for(int i=0;i<NCW0+2;i++) h=hmix(h ^ aword(i,star) ^ (u32)i*0x27D4EB2Fu); return h; }
static void clus_refold(void){ u32 f=0, up=0; for(int i=NCW0;i<NCW;i++) if(clus[i]){ up|=clus[i]; f=hmix(f^clus[i]^(u32)i*0x27D4EB2Fu); } clus_f=f; clus_up=up; }   // call after any change to the cluster words
static int in_menu=0; static char menu_msg[40];   // inside a menu, messages go on a line in the menu instead of over it                 // set by the SYSTEM page: 1 = load cur_slot, 2 = quit to the title

// ---------------------------------------------------------------- mode 4 text
static void pix(volatile u16 *pg, int x, int y, int c){
    if(m7_on){ m7_pix(x,y,c); return; }
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
    if(sc==2 && !(x&1)){                                   // fast path: big text on an even column writes whole pixel pairs (row by row: no dividing)
        u16 cc=c|(c<<8);
        for(;*s;s++){ u16 g = font[(u8)*s & 127];
            if(g){ volatile u16 *row = pg + y*120 + (x>>1);
                for(int r=0;r<5;r++, row+=240){ int bits=(g>>(12-r*3))&7; if(!bits) continue;
                    if(bits&4){ row[0]=cc; row[120]=cc; } if(bits&2){ row[1]=cc; row[121]=cc; } if(bits&1){ row[2]=cc; row[122]=cc; } } }
            x += 8; }
        return x; }
    if(sc==1){                                             // small text: each glyph row touches two pixel pairs at most
        for(;*s;s++){ u16 g = font[(u8)*s & 127];
            if(g){ volatile u16 *row = pg + y*120 + (x>>1);
                if(x&1){ for(int r=0;r<5;r++, row+=120){ int bits=(g>>(12-r*3))&7; if(!bits) continue;
                    if(bits&4) row[0]=(row[0]&0x00FF)|(c<<8);
                    if(bits&2){ if(bits&1) row[1]=c|(c<<8); else row[1]=(row[1]&0xFF00)|c; } else if(bits&1) row[1]=(row[1]&0x00FF)|(c<<8); } }
                else { for(int r=0;r<5;r++, row+=120){ int bits=(g>>(12-r*3))&7; if(!bits) continue;
                    if(bits&4){ if(bits&2) row[0]=c|(c<<8); else row[0]=(row[0]&0xFF00)|c; } else if(bits&2) row[0]=(row[0]&0x00FF)|(c<<8);
                    if(bits&1) row[1]=(row[1]&0xFF00)|c; } } }
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
static u16 cargo[8];
static const char *const eco_name[4]={"FARMING","MINING","TECH","HIVE"};
static const char *const goods[8]={"FOOD","NECTAR","SILK","AMBER","CRYSTALS","MACHINES","MEDICINE","RELICS"};
static u32 sys_id(int i); static int is_hive(int i);
static int cargo_n(void){ int n=0; for(int i=0;i<8;i++) n+=cargo[i]; return n; }
#define VISPG ((volatile u16*)(0x06000000 + (((REG_DISPCNT&0x10) && !m7_on)?0xA000:0)))   // the page on screen right now (in a kart race, always the first)
// ================================================================= MORE OPTIONS (OPTIONS screen, press SELECT)
// Every extra option is a row in XOT. Its setting is stored as (index XOR default), so a zero byte (a fresh game, or EWRAM at power on)
// always means "the normal setting". xo_v(XO_...) is the number the game uses; xo_i(XO_...) is which choice is picked.
#define NXO 48
typedef struct { const char *name, *help; const char *const *lab; const s32 *val; u8 n, def, grp; } XOpt;
#define XL(id,...) static const char *const id##_l[]={__VA_ARGS__};
#define XV(id,...) static const s32 id##_v[]={__VA_ARGS__};
#define XR(nm,hp,id,df,gr) { nm, hp, id##_l, id##_v, (u8)(sizeof(id##_v)/sizeof(id##_v[0])), df, gr }
enum { XG_COMBAT, XG_ENEMY, XG_SURV, XG_MOVE, XG_REWARD, XG_EXTRA, NXG };
static const char *const xo_gname[NXG]={"COMBAT","ENEMIES","SURVIVAL","MOVEMENT","REWARDS","EXTRAS"};
enum { XO_DMGIN, XO_DMGOUT, XO_CRIT, XO_BITESPD, XO_BITERNG, XO_KNOCK, XO_HITSTOP, XO_CRUNCH, XO_VAMP, XO_SPELL, XO_INVT, XO_GOD,
       XO_ESPEED, XO_EHP, XO_ECOUNT, XO_EAWARE, XO_ESTR, XO_BOSSES,
       XO_HUNGER, XO_REGEN, XO_HEAL, XO_MANA, XO_MAXH, XO_FUEL,
       XO_MOVE, XO_DODGEI, XO_DODGED, XO_FLOORS,
       XO_XP, XO_GOLD, XO_DROPS, XO_UNIQUE, XO_XPNEED,
       XO_VOL, XO_FPS, XO_MSGT, XO_BUDDY, XO_GFX, NXOPT };
XL(xl_dmgin,"NONE","1/4","1/2","NORMAL","X2","X4","X10")           XV(xl_dmgin,0,25,50,100,200,400,1000)
XL(xl_dmgout,"1/4","1/2","NORMAL","X2","X5","X10","X100","X1000")  XV(xl_dmgout,25,50,100,200,500,1000,10000,100000)
XL(xl_crit,"NORMAL","+10%","+25%","+50%","ALWAYS")                   XV(xl_crit,0,10,25,50,100)
XL(xl_bspd,"X1/2","X3/4","NORMAL","X1.5","X2","X3")                 XV(xl_bspd,50,75,100,150,200,300)
XL(xl_brng,"SHORT","NORMAL","LONG","VERY LONG","HUGE")              XV(xl_brng,50,100,150,200,400)
XL(xl_knock,"NONE","1/2","NORMAL","X2","X4")                        XV(xl_knock,0,50,100,200,400)
XL(xl_hstop,"NONE","SHORT","NORMAL","LONG")                         XV(xl_hstop,0,50,100,200)
XL(xl_crunch,"EVERY BITE","EVERY 2ND","EVERY 3RD","EVERY 5TH","EVERY 7TH") XV(xl_crunch,0,1,2,4,6)
XL(xl_vamp,"OFF","1","2","5","10","50")                             XV(xl_vamp,0,1,2,5,10,50)
XL(xl_spell,"FREE","1/4","1/2","NORMAL")                            XV(xl_spell,0,25,50,100)
XL(xl_invt,"SHORT","NORMAL","LONG","2 SEC","5 SEC")                 XV(xl_invt,15,30,60,120,300)
XL(xl_gfx,"FAST","BALANCED","SMOOTH")                               XV(xl_gfx,0,1,2)
XL(xl_yn,"OFF","ON")                                                XV(xl_yn,0,1)
XL(xl_espd,"FROZEN","1/2","3/4","NORMAL","X1.5","X2","X3")          XV(xl_espd,0,50,75,100,150,200,300)
XL(xl_bud,"1","2","3","5","10")                                   XV(xl_bud,1,2,3,5,10)
XL(xl_ehp,"1/4","1/2","NORMAL","X2","X5","X10")                     XV(xl_ehp,25,50,100,200,500,1000)
XL(xl_ecnt,"NONE","1/4","1/2","NORMAL","X2","X3","X5","X7","X10")  XV(xl_ecnt,0,25,50,100,200,300,500,700,1000)
XL(xl_eawr,"SHORT","NORMAL","LONG","VERY LONG","EVERYWHERE")        XV(xl_eawr,50,100,200,400,1000)
XL(xl_estr,"1/4","1/2","NORMAL","X1.5","X2","X5")                   XV(xl_estr,25,50,100,150,200,500)
XL(xl_boss,"NONE","+1","+2","+4","+8","+15")                        XV(xl_boss,0,1,2,4,8,15)
XL(xl_hung,"X1/4","X1/2","NORMAL","X2","X4")                        XV(xl_hung,25,50,100,200,400)
XL(xl_regn,"NORMAL","X2","X4","X10")                                XV(xl_regn,100,200,400,1000)
XL(xl_heal,"OFF","1","2","5","10","25")                             XV(xl_heal,0,1,2,5,10,25)
XL(xl_mana,"NORMAL","X2","X5","X10")                                XV(xl_mana,1,2,5,10)
XL(xl_maxh,"NORMAL","X1.5","X2","X5","X10","X100")                  XV(xl_maxh,100,150,200,500,1000,10000)
XL(xl_fuel,"FREE","1/4","1/2","NORMAL","X2")                        XV(xl_fuel,0,25,50,100,200)
XL(xl_move,"1/2","3/4","NORMAL","X1.25","X1.5","X2","X3")           XV(xl_move,50,75,100,125,150,200,300)
XL(xl_dgi,"SHORT","NORMAL","LONG","VERY LONG")                      XV(xl_dgi,7,14,30,60)
XL(xl_dgd,"SHORT","NORMAL","FAR","VERY FAR")                        XV(xl_dgd,50,100,150,200)
XL(xl_flr,"1/10","1/4","1/2","NORMAL","X2","X5")                    XV(xl_flr,10,25,50,100,200,500)
XL(xl_big,"X1","X2","X5","X10","X25","X100")                        XV(xl_big,1,2,5,10,25,100)
XL(xl_drop,"NONE","1/6","NORMAL","1/2","2/3","ALWAYS")              XV(xl_drop,0,50,100,150,200,300)
XL(xl_uniq,"NORMAL","X2","X5","X10","X50","X250")                   XV(xl_uniq,100,200,500,1000,5000,25000)
XL(xl_xpn,"1/10","1/4","1/2","NORMAL","X2","X5")                    XV(xl_xpn,10,25,50,100,200,500)
XL(xl_vol,"100%","85%","70%","55%","40%","30%","15%","MUTE")        XV(xl_vol,0,1,2,3,4,5,6,7)
XL(xl_msgt,"SHORT","NORMAL","LONG","VERY LONG")                     XV(xl_msgt,50,100,200,400)
static const XOpt XOT[NXOPT]={
  [XO_DMGIN]  =XR("DAMAGE YOU TAKE","HOW MUCH EVERY BITE AND STING HURTS YOU. NONE = YOU CANNOT BE HURT",xl_dmgin,3,XG_COMBAT),
  [XO_DMGOUT] =XR("DAMAGE YOU DEAL","HOW HARD YOUR BITE HITS. X1000 ONE-SHOTS ALMOST EVERYTHING",xl_dmgout,2,XG_COMBAT),
  [XO_CRIT]   =XR("CRIT CHANCE","EXTRA PERCENT CHANCE FOR A CRITICAL BITE ON TOP OF YOUR GEAR",xl_crit,0,XG_COMBAT),
  [XO_BITESPD]=XR("BITE SPEED","HOW FAST YOU CAN BITE AGAIN",xl_bspd,2,XG_COMBAT),
  [XO_BITERNG]=XR("BITE REACH","HOW FAR IN FRONT OF YOU A BITE LANDS",xl_brng,1,XG_COMBAT),
  [XO_KNOCK]  =XR("KNOCKBACK","HOW FAR YOUR BITES SHOVE MONSTERS BACK",xl_knock,2,XG_COMBAT),
  [XO_HITSTOP]=XR("HIT FREEZE","THE TINY FREEZE WHEN A BITE LANDS. NONE FEELS SNAPPY",xl_hstop,2,XG_COMBAT),
  [XO_CRUNCH] =XR("CRUNCH COMBO","HOW OFTEN YOUR DOUBLE-POWER CRUNCH BITE COMES",xl_crunch,2,XG_COMBAT),
  [XO_VAMP]   =XR("HEAL ON BITE","HEARTS YOU GET BACK WITH EVERY BITE (ADDS TO YOUR GEAR)",xl_vamp,0,XG_COMBAT),
  [XO_SPELL]  =XR("SPELL COST","HOW MUCH MANA SPELLS USE",xl_spell,3,XG_COMBAT),
  [XO_INVT]   =XR("SAFE TIME AFTER HIT","HOW LONG YOU CANNOT BE HIT AFTER GETTING HURT",xl_invt,1,XG_COMBAT),
  [XO_GOD]    =XR("GOD MODE","FULL HEARTS, FOOD, MANA AND SHIELD ALL THE TIME",xl_yn,0,XG_COMBAT),
  [XO_ESPEED] =XR("MONSTER SPEED","HOW FAST MONSTERS WALK. FROZEN = THEY STAND STILL",xl_espd,3,XG_ENEMY),
  [XO_EHP]    =XR("MONSTER HEALTH","HEALTH OF EVERY NEW MONSTER AND BOSS (NEXT FLOOR)",xl_ehp,2,XG_ENEMY),
  [XO_ECOUNT] =XR("MONSTER COUNT","HOW MANY MONSTERS FILL EACH ROOM (NEXT FLOOR)",xl_ecnt,3,XG_ENEMY),
  [XO_EAWARE] =XR("MONSTER SIGHT","HOW FAR AWAY MONSTERS NOTICE YOU AND COME",xl_eawr,1,XG_ENEMY),
  [XO_ESTR]   =XR("MONSTER STRENGTH","HOW HARD MONSTERS HIT, BEFORE YOUR ARMOUR",xl_estr,2,XG_ENEMY),
  [XO_BUDDY]  =XR("LADYBUG BUDDIES","HOW MANY BUDDIES FIGHT BY YOUR SIDE IN DUNGEONS. UP TO 10",xl_bud,0,XG_ENEMY),
  [XO_BOSSES] =XR("EXTRA BOSSES","MORE BOSSES AT THE BOTTOM OF EVERY ZONE (NEXT FLOOR)",xl_boss,0,XG_ENEMY),
  [XO_HUNGER] =XR("HUNGER SPEED","HOW FAST YOU GET HUNGRY (IF HUNGER IS ON)",xl_hung,2,XG_SURV),
  [XO_REGEN]  =XR("HEAL TIMER","HOW OFTEN SLOW HEALING AND MANA TICK",xl_regn,0,XG_SURV),
  [XO_HEAL]   =XR("HEARTS PER TICK","HEARTS YOU HEAL ON EVERY HEAL TICK",xl_heal,0,XG_SURV),
  [XO_MANA]   =XR("MANA REGEN","HOW MUCH MANA COMES BACK ON EVERY HEAL TICK",xl_mana,0,XG_SURV),
  [XO_MAXH]   =XR("MAX HEARTS","MULTIPLIES YOUR MAXIMUM HEARTS",xl_maxh,0,XG_SURV),
  [XO_FUEL]   =XR("FUEL USE","HOW MUCH FUEL SHIP TRAVEL BURNS. FREE = NEVER LOSE ANY",xl_fuel,3,XG_SURV),
  [XO_MOVE]   =XR("WALK SPEED","HOW FAST YOU RUN AROUND ON FOOT",xl_move,2,XG_MOVE),
  [XO_DODGEI] =XR("DODGE SAFE TIME","HOW LONG YOU CANNOT BE HIT DURING A DODGE DASH",xl_dgi,1,XG_MOVE),
  [XO_DODGED] =XR("DODGE DISTANCE","HOW FAR A DOUBLE-TAP L OR R DASH GOES",xl_dgd,1,XG_MOVE),
  [XO_FLOORS] =XR("DUNGEON DEPTH","HOW MANY FLOORS BEFORE EACH ZONE BOSS",xl_flr,3,XG_MOVE),
  [XO_XP]     =XR("XP GAIN","MULTIPLIES ALL XP YOU EARN (ON TOP OF THE OPTION BEFORE)",xl_big,0,XG_REWARD),
  [XO_GOLD]   =XR("GOLD FIND","MULTIPLIES ALL GOLD YOU EARN (ON TOP OF THE OPTION BEFORE)",xl_big,0,XG_REWARD),
  [XO_DROPS]  =XR("LOOT DROPS","HOW OFTEN MONSTERS DROP GEAR",xl_drop,2,XG_REWARD),
  [XO_UNIQUE] =XR("UNIQUE ITEM CHANCE","HOW OFTEN THE RARE UNIQUE ITEMS DROP",xl_uniq,0,XG_REWARD),
  [XO_XPNEED] =XR("XP TO LEVEL UP","HOW MUCH XP EACH LEVEL NEEDS",xl_xpn,3,XG_REWARD),
  [XO_VOL]    =XR("MASTER VOLUME","HOW LOUD EVERYTHING IS. MUTE TURNS ALL SOUND OFF",xl_vol,0,XG_EXTRA),
  [XO_GFX]    =XR("DUNGEON LOOK","FAST IS CHUNKIER BUT RUNS MUCH SMOOTHER",xl_gfx,1,XG_EXTRA),
  [XO_FPS]    =XR("FPS COUNTER","SHOWS FRAMES PER SECOND IN THE CORNER",xl_yn,0,XG_EXTRA),
  [XO_MSGT]   =XR("MESSAGE TIME","HOW LONG POP-UP MESSAGES STAY ON SCREEN",xl_msgt,1,XG_EXTRA),
};
_Static_assert(NXOPT<=NXO, "the extra options must fit the NXO bytes");
static u8 xo[NXO] __attribute__((section(".ewram")));
static int xo_i(int o){ int i=xo[o]^XOT[o].def; return i<XOT[o].n ? i : XOT[o].def; }
static int xo_v(int o){ return XOT[o].val[xo_i(o)]; }
static u16 xo_snd(void){ int i=xo_i(XO_VOL); if(i>=7) return 0; { int l=7-i; return (u16)(0xFF00|(l<<4)|l); } }   // the sound chip's master volume (and mute)
static int xo_hp(int h){ long long v=(long long)h*xo_v(XO_EHP)/100; if(v<1) v=1; if(v>32000) v=32000; return (int)v; }   // monster health is a 16-bit number
#define NBUD 9                                                   // the extra Buddies (LADYBUG BUDDIES): they bite, they never get hurt
typedef struct { s32 x, y; s16 cool; u8 flash, pad; } Bud;
static Bud bud[NBUD]; static s32 bud_dd[NBUD];
static int nbud(void){
#ifdef TEST_BUD
    return NBUD;
#endif
    return xo_v(XO_BUDDY)-1; }
static int xo_n(int c){ int v=xo_v(XO_ECOUNT); if(v==100) return c; c=c*v/100; if(v && c<1) c=1; return c; }
static int xo_fuel(int n){ return (n*xo_v(XO_FUEL)+99)/100; }

#include "sound.inc"
static void ore_reset(void); static void ore_save(void); static void ore_load(void);   // (ore.inc, after save.inc)
#include "game.inc"
#include "ship.inc"
#include "save.inc"
#include "ore.inc"
static void title_restore(volatile u16 *pg, int y0, int y1); static void twinkle(volatile u16 *pg, int t);
static int beast_stage;
#include "ui.inc"
#include "hangar.inc"
static int launch_flag=0, zone_card=0, arrow_ship=0, base_card=0;
static void story_boss_beaten(void); static int hive_here(void); static int hive_boss_floor(void); static void hive_kill(void);

#define IWRAM_DATA
#define TMR() (*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16))
static u16 dim15(u16 c, int k){ int r=(c&31)*k/16, g=((c>>5)&31)*k/16, b=((c>>10)&31)*k/16; return r|(g<<5)|(b<<10); }
#include "planet.inc"

#include "dungeon.inc"
#include "terrace.inc"
#include "space.inc"
#include "forge.inc"
#include "kart.inc"
#include "kart2.inc"
#include "kart3.inc"
#include "sizes.inc"
#include "deep.inc"
#include "extras.inc"
#include "debug.inc"
#ifdef TEST_LAUNCH
#include "film.inc"
#endif

static void hud_now(void){ if(scene==2) return; if(scene==0) planet_hud(); else dungeon_hud(); }
static void say(const char *t){
    if(in_menu){ int n=0; while(t[n] && n<39){ menu_msg[n]=t[n]; n++; } menu_msg[n]=0; return; }
    if(scene==0){ show_msg(t); return; }
    int n=0; while(t[n] && n<39){ msg[n]=t[n]; n++; } msg[n]=0; msg_timer=24*xo_v(XO_MSGT)/100;
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
static void bar(volatile u16 *pg, int c0, s64 val, s64 max, int col){
    int h=34, f = ratio(val,max,h); if(f>h) f=h; if(f<0) f=0;
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
    switch(o->kind){ case K_BEETLE: b=b*9/5; break; case K_BAT: b=b*7/10+1; break; case K_BOSS: b=b*5/2; break; case K_SCORP: b=b*3/2; break; case K_GHOST: b=b*4/5; break; case K_SLIME: b = o->sub ? b/2+1 : b*3/4; break; case K_CENTI: b=b*3/5+1; break; case K_GOLEM: b=b*2; break; case K_DRONE: b=b*9/10; break; case K_MOTH: b=b/2+1; break; case K_SHROOM: b=b*4/5; break; case K_CRAB: b=b*5/3; break; case K_WISP: b=b*7/10; break; }
    if(nm_of(o->kind)) b=b*nm_of(o->kind)->dmm/8;
    if(o->champ) b=b*3/2;
    if(fl_ev==FE_CURSED) b=b*3/2;
    { int sv=xo_v(XO_ESTR); if(sv!=100){ b=(int)((long long)b*sv/100); if(b<1) b=1; } }
    return b;
}
static void kill_mon(DObj *o){
    if(nm_has(o,NF_SPLIT) && !o->sub){ for(int s=0;s<2 && ndo<NDO;s++){ DObj *c=&dob[ndo++]; *c=*o; c->sub=1; c->champ=0; c->venom=0; c->hp=c->maxhp=(s16)(o->maxhp/2>0?o->maxhp/2:1);   // flatworms, puffballs, geodes, zombugs: two smaller ones
            c->x=o->x+(s?0x2400:-0x2400); c->y=o->y+(s?-0x1000:0x1000); if(solid(c->x,c->y)){ c->x=o->x; c->y=o->y; } c->flash=6; c->stag=14; c->wind=0; }
        say("IT SPLITS IN TWO!"); }
    if(nm_has(o,NF_BOOM)){ burst(o->x,o->y,0x6000,12,0); sfx(3); if(dist8(px_-o->x,py_-o->y)<0x18000){ hurt(mon_dmg(o)*2); hurt_flash=8; say("BOOM! IT BLEW UP"); } }   // stinkbugs, snowballs, scrapbots, cinders
    if(o->kind==K_SKELE && !o->ph){ o->ph=1; o->hp=o->maxhp/2+1; o->flash=8; o->stag=12; o->venom=0; say("THE BONES RISE AGAIN!"); sfx(14); return; }   // skeletons get up once
    fl_kill(o);
    int boss=o->kind==K_BOSS, hive = boss && endless && hive_boss_floor(); int nbo=0; if(boss) for(int i=0;i<ndo;i++) if(&dob[i]!=o && dob[i].alive && dob[i].kind==K_BOSS) nbo++;
    if(o->kind>=0 && o->kind<11) st_kills[o->kind]++;
    if(o->kind==K_CORE){                                                              // the outpost dies: Bolt gets a memory back, the beast loses a feeding node
        o->alive=0; sfx(9); debris(o->x,o->y,0x8000,16,3,0,0); burst(o->x,o->y,0x8000,12,2); if(opt_shake) shake_t=8;
        base_mask |= 1u<<(cur_sys&31); outposts_done++; if(data_cores<8) data_cores++;
        scrap+=8; earn(150+outposts_done*60); gain_xp(40+dlv()*8);
        b3_max = b3_max*96/100; if(b3_hp>b3_max) b3_hp=b3_max; if(beast_stage>=3){ beast_max=b3_max; if(beast_hp>beast_max) beast_hp=beast_max; }
        base_card=1; escape_flag=1; say("THE CORE SHATTERS! BOLT: I'LL BEAM US OUT"); hud_now(); return; }
    int xpv = (4+dlv()*2+depth_no) * (o->kind==K_BEETLE?2 : o->kind==K_WASP?3 : boss?10 : 1) / (o->kind==K_WASP?2:1);
    if(o->kind==K_SCORP||o->kind==K_GHOST) xpv=xpv*2; else if(o->kind==K_TOAD) xpv=xpv*3/2; else if(o->kind==K_SLIME) xpv = o->sub ? xpv/2+1 : xpv*3/2; else if(o->kind==K_GOLEM) xpv=xpv*4; else if(o->kind==K_DRONE) xpv=xpv*2; else if(o->kind==K_SHROOM) xpv=xpv*3/2; else if(o->kind==K_CRAB) xpv=xpv*3; else if(o->kind==K_WISP) xpv=xpv*2;
    if(nm_of(o->kind)) xpv=xpv*nm_of(o->kind)->xpm/8;
    if(o->champ) xpv*=3;
    earn(boss ? 100+depth_no*10 : 6+dlv()*4+(o->champ?40:0));
    if(!forge_on && q_type==Q_HUNT){ q_have++; if(q_have>=q_need && !q_done){ q_done=1; say("QUEST DONE! GO BACK TO YOUR SHIP"); } }
    if(o->champ==2 && q_type==Q_CHAMP){ q_done=1; say("QUEST TARGET SLAIN!"); }
    if(!forge_on && boss && q_type==Q_BOSS && !q_done && q_zone==zone && q_world==world_no){ q_done=1; say("BOSS QUEST DONE! GO TO YOUR SHIP"); }
    sfx(9); debris(o->x,o->y,0x6000,boss?16:10,3,o->kvx,o->kvy); burst(o->x,o->y,0x6000,4,1);
    if(o->kind==K_SLIME && !o->sub){                                                  // a slime splits into two small ones
        for(int k=0;k<2 && ndo<NDO;k++){ DObj *b=&dob[ndo++]; memset(b,0,sizeof(DObj)); b->kind=K_SLIME; b->sub=1; b->alive=1; b->hp=b->maxhp=o->maxhp/3+1; b->x=o->x+((k?1:-1)<<14); b->y=o->y+(1<<14); if(solid(b->x,b->y)){ b->x=o->x; b->y=o->y; } b->kvx=(k?1:-1)*0x1400; b->kvy=0x0800; } }
    int ilvl=dlv()*3+depth_no;
    if(boss && nbo){ char t[32]; scpy(t,"BOSS DOWN! "); ncat(t,nbo); scat(t," LEFT"); say(t); xpv+=30+dlv()*10; }
    else if(boss){ music(1); if(!endless||forge_on){ tgt_x=up_x; tgt_y=up_y; } if(!endless && !zone_cleared) zone_card=1+zone;      // the way out is back up the stairs: point the arrow there
       
        if(endless && hive_boss_floor()){ hive_kill(); say("OVERLORD DOWN! THE HIVE DIES"); xpv+=200+dlv()*20; } else if(endless && cur_moon){ int mt=moon_tier(); char mb[40]; scpy(mb,"MOON GUARDIAN DOWN! +"); ncat(mb,200*mt); scat(mb,"G"); say(mb); earn(200*mt); } else if(endless) say("GUARDIAN BEATEN!"); else if(zone_cleared){ if(tiers[TZ]<(opt_diff==5?53:9)) tiers[TZ]++; st_tiers++; char tb[40]; scpy(tb,"TIER "); ncat(tb,tiers[TZ]); scat(tb," CLEARED! LOOT GETS BETTER"); say(tb); }
              else { zone_cleared=1; story_boss_beaten(); say(zone==5?"THE ANT KING FALLS! THE GALAXY OPENS":"BOSS BEATEN! NEXT STAR OPEN"); } xpv+=30+dlv()*10; }
    if(boss && mboss){ say("THE LABYRINTH FALLS SILENT"); earn(150+dlv()*30); xpv+=40+dlv()*8; }
    if(boss && !endless && !forge_on && zone==5 && opt_diff==4 && !opt_insane){ opt_insane=1; settings_save(); say("INSANE-SECT UNLOCKED! SEE OPTIONS"); sfx(6); }
    gain_xp(xpv);
    Gear g; int drop=0;
    int uq_chance = hive ? 1000 : boss ? (mboss?600:(endless&&cur_moon)?450:300) : o->champ ? 50 : 4;          // in 1000
    if(rndi(1000) < uq_chance*xo_v(XO_UNIQUE)/100){ g=roll_unique(ilvl); drop=1; st_uniq++; say("A UNIQUE ITEM DROPPED!"); sfx(6); }
    else if(boss || o->champ){ g=roll_gear(rndi(3)==0?IT_CHARM:rndi(2)?IT_THORN:IT_SHELL, ilvl+4, 40); drop=1; }
    else if(rndi(300) < xo_v(XO_DROPS)){ g=roll_drop(ilvl,0); drop=1; }
    s32 x=o->x, y=o->y;
    o->alive=0;
    if(drop){ memset(o,0,sizeof(DObj)); o->kind=K_ITEM; o->alive=1; o->x=x; o->y=y; o->it=g.it; o->sub=g.sub; o->rar=g.rar; o->pre=g.pre; o->suf=g.suf; o->lvl=g.lvl; }
    hud_now();
}
static void hit_mon(DObj *o, int dmg, int crit){
    if(nm_has(o,NF_ARMOR)){ dmg=(dmg+1)/2; if(dmg<1) dmg=1; }                         // rockbacks: half damage
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
    case K_SLIME: r=o->sub?0x2400:0x3800; break; case K_SCORP: r=0x3000; break; case K_GHOST: r=0x2800; break; case K_TOAD: r=0x3400; break; case K_ANT: r=0x2C00; break; case K_CENTI: r=0x2800; break; case K_GOLEM: r=0x5800; break; case K_DRONE: r=0x2800; break; case K_MOTH: r=0x2400; break; case K_SHROOM: r=0x3400; break; case K_CRAB: r=0x4800; break; case K_WISP: r=0x2800; break;
    case K_WASP: r=0x2800; break; case K_BARREL: r=o->sub?0x3800:0x3200; break; case K_CAGE: r=0x3000; break; case K_CORE: r=0x5000; break; default: if(nm_of(o->kind)){ r=nm_of(o->kind)->rad; break; } return 0; } return o->champ ? r*5/4 : r; }
static int body_m(DObj *o){ int m; switch(o->kind){ case K_SPIDER: m=3; break; case K_BEETLE: m=8; break; case K_BOSS: m=40; break; case K_BAT: m=1; break;
    case K_SLIME: m=o->sub?2:4; break; case K_SCORP: m=3; break; case K_GHOST: m=1; break; case K_TOAD: m=5; break; case K_ANT: m=3; break; case K_CENTI: m=2; break; case K_GOLEM: m=20; break; case K_DRONE: m=1; break; case K_MOTH: m=1; break; case K_SHROOM: m=6; break; case K_CRAB: m=12; break; case K_WISP: m=1; break;
    case K_WASP: m=2; break; case K_BARREL: m=o->sub?9:5; break; default: m = nm_of(o->kind) ? nm_of(o->kind)->mass : 999; } return o->champ ? m*2 : m; }
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
    s32 kb = power*5/body_m(o); kb=kb*xo_v(XO_KNOCK)/100; if(kb>0x3400) kb=0x3400;
    o->kvx += (s32)(((long long)dx*kb)>>16); o->kvy += (s32)(((long long)dy*kb)>>16);
}
static void player_move(s32 tvx, s32 tvy){
    if(!dodge_t){ int mv=xo_v(XO_MOVE); if(mv!=100){ tvx=tvx*mv/100; tvy=tvy*mv/100; } }
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
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; if(iabs(o->x-px_)>0x40000 || iabs(o->y-py_)>0x40000) continue; s32 r=body_r(o); if(!r) continue; s32 nx,ny;
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
            if(is_mon(o->kind)){ int dm=2+clampi(sdiv(level,2))+hit/0x800; add_fnum(o->x,o->y,dm,C_GOLD); o->hp-=dm; o->flash=6; if(o->hp<=0){ kill_mon(o); continue; } } }
        s32 sp=iabs(o->kvx)+iabs(o->kvy);
        if(sp>0x2000 && is_mon(o->kind) && !invuln && iabs(o->x-px_)<r+PRAD+0x2000 && iabs(o->y-py_)<r+PRAD+0x2000){
            s64 before=hearts; hurt(mon_dmg(o)*3/2); if(hearts<before){ hurt_flash=8; hitstop=5; kick_t=8; pknock_t=14; combo=0; pvx+=o->kvx; pvy+=o->kvy; say("RAMMED!"); } o->kvx/=4; o->kvy/=4; }
        if(sp>0x1200) for(int j=0;j<ndo;j++){ DObj *b=&dob[j]; if(j==i||!b->alive||!is_mon(b->kind)) continue;
            s32 rr=(r?r:0x2000)+body_r(b); if(iabs(b->x-o->x)>=rr||iabs(b->y-o->y)>=rr) continue;
            int dm=3+clampi(sdiv(level,2)); b->kvx+=o->kvx/2; b->kvy+=o->kvy/2; b->stag=4; o->kvx/=3; o->kvy/=3; thump(1);
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
    if(tx<0||ty<0||tx>=MW||ty>=MW) return 0; return (fmat[ty*MW+tx]==FL_WATER||fmat[ty*MW+tx]==FL_SHALLOW) && dmap[ty*MW+tx]==D_FLOOR;
}
static int spark_t=0, spark_c=60, spark_y=80;
// ---- fishing 2: everything here runs every 60th of a second, so it answers the instant you press
// angler rank (from fish caught): quicker bites, longer bite window, stronger reel, better odds
// states: 0 idle, 1 waiting (nibbles first), 2 bite (press A), 3 the fight (hold A to reel, LET GO when it surges)
static int fish_buf=0, fish_rc=0, fish_f=0, fish_hp=0, fish_hpm=1, fish_ten=0, fish_sg=0, fish_sgw=0, fish_streak=0, fish_pool=0, fish_sig=0;
static u8 pool_left[10];
static const char *const rank_nm[6]={"NOVICE","ANGLER","SKILLED","EXPERT","MASTER","LEGEND"};
static int angler(void){ return st_fish>=120?5 : st_fish>=70?4 : st_fish>=35?3 : st_fish>=15?2 : st_fish>=5?1 : 0; }
static int pool_at(s32 dirx, s32 diry){
    s32 x=px_+((dirx*13)>>4), y=py_+((diry*13)>>4); int tx=x>>16, ty=y>>16; int r=rid[ty*MW+tx]; return r>=10 ? 0 : r; }
static int pool_stock(int p){                         // every pool holds a few fish; deep grotto pools hold more
    int sg=(depth_no*131+nrooms*17+rooms[0].x*7+rooms[0].y*3+rooms[1].x*11+rooms[1].y*5)&0xFFFF; sg|=0x10000;
    if(sg!=fish_sig){ fish_sig=sg; for(int i=0;i<10;i++) pool_left[i]=255; fish_streak=0; }
    if(pool_left[p]==255) pool_left[p]=(u8)(4+rndi(4)+(rooms[p].style==ST_GROTTO?3:0));
    return pool_left[p]; }
static int fish_roll(int rb){
    int r=rndi(1000);
    if(r<3+rb/40) return 7; if(r<12+rb/10) return 6; if(r<35+rb/5) return 5;
    r=rndi(100); return r<45?0 : r<62?1 : r<79?2 : r<88?3 : 4; }
static void fish_catch(void){
    int f=fish_f, rk=angler();
    Gear g={IT_FISH,(u8)f,(u8)(f>=5?R_RARE:f>=3?R_MAGIC:R_NORMAL),0,0,0};
    if(!add_gear(g)){ fish_state=0; fish_rc=30; return; }
    st_fish++; fish_streak++; if(pool_left[fish_pool]>0 && pool_left[fish_pool]!=255) pool_left[fish_pool]--;
    ach_check(); sfx(f>=5?6:2); fish_state=0; fish_rc=8;
    char b[40];
    if(f>=5){ scpy(b, f==7?"LEGENDARY! ":"RARE CATCH! "); scat(b,fish_name[f]); scat(b,"!"); say(b); msg_timer=80; if(opt_shake) shake_t=6; }
    else if(angler()>rk){ scpy(b,"ANGLER RANK: "); scat(b,rank_nm[angler()]); scat(b,"!"); say(b); msg_timer=70; sfx(6); }
    else if(fish_streak>=3 && !(fish_streak%3)){ int gv=6+dlv()*4+depth_no*2+fish_streak*2; earn(gv); scpy(b,"CATCH STREAK X"); ncat(b,fish_streak); scat(b," +"); ncat(b,gv); scat(b,"G"); say(b); sfx(7); }
    if(rndi(100)<5){ int gv=10+dlv()*6+depth_no*3; earn(gv); scpy(b,"A POUCH ON THE LINE! +"); ncat(b,gv); scat(b,"G"); say(b); sfx(7); }
    else if(rndi(100)<2) add_item(IT_GEM,rndi(4));
    if(pool_left[fish_pool]==0) { say("THE POOL IS FISHED OUT"); msg_timer=60; }
}
// returns 1 when fishing used this step's A press
static int fish_step(s32 dirx, s32 diry, int blocked, int foe){
    int rk=angler();
    if(fish_rc) fish_rc--;
    if(fish_state && (foe || hurt_flash)){ fish_state=0; say("A MONSTER! LINE CUT"); fish_rc=20; return 0; }
    int wet=water_ahead(dirx,diry);
    if(fish_state && !wet){ fish_state=0; fish_rc=6; say("YOU REEL IN"); return 0; }   // you walked or turned away from the water
    if(fish_state==0){
        if(blocked || !wet || fish_rc) return 0;
        if(!HIT(K_A)) return 0;                                   // a fresh press casts: holding A no longer re-casts by itself
        int p=pool_at(dirx,diry); fish_pool=p;
        if(pool_stock(p)==0){ if(HIT(K_A)){ say("FISHED OUT. TRY ANOTHER POOL"); sfx(11); } return HIT(K_A); }
        fish_state=1; fish_buf=0; fish_t=40+rndi(100)-rk*8; if(fish_t<28) fish_t=28; sfx(8); say("CAST! WAIT FOR THE BITE"); return 1;
    }
    if(fish_state==1){
        if(HIT(K_A)){ fish_buf=14; }                                 // a press while waiting is kept for 14 steps: if the bite lands inside it, you hook at once
        if(fish_buf) fish_buf--;
        if(fish_t==30 || fish_t==18) sfx(11);
        if(--fish_t<=0){ fish_state=2; fish_t=72+rk*6; sfx(8); if(opt_shake) shake_t=2;
            int rb=rk*8 + (depth_no<20?depth_no*2:40) + (rooms[fish_pool].style==ST_GROTTO?40:0) + (fish_streak<5?fish_streak*10:50) + (mark_on(MK_LUCKY)?30:0);
            fish_f=fish_roll(rb);
            if(fish_buf) goto hook; }
        return 1;
    }
    if(fish_state==2){
        if(HIT(K_A)){ hook:
            { static const u16 hp[8]={90,110,120,150,170,200,240,300}; fish_hpm=fish_hp=hp[fish_f]*10; }
            fish_state=3; fish_ten=250; fish_sg=0; fish_sgw=60+rndi(50)-fish_f*4; sfx(2); return 1; }
        if(--fish_t<=0){ fish_state=0; fish_streak=0; fish_rc=10; say("IT GOT AWAY"); sfx(11); }
        return 1;
    }
    // state 3: the fight. hold A to reel. when it surges (bar flashes) let go or the line snaps
    int held=HELD(K_A)!=0;
    if(fish_sg>0){ fish_sg--; if(!fish_sg) fish_sgw=55+rndi(55)-fish_f*4; }
    else if(--fish_sgw<=0){ fish_sg=24+fish_f*3; }
    if(held){ fish_ten += fish_sg?26:9; if(!fish_sg) fish_hp -= 12+rk*2; }
    else { fish_ten -= 14; if(fish_ten<0) fish_ten=0; fish_hp += fish_sg?3:1; if(fish_hp>fish_hpm) fish_hp=fish_hpm; }
    if(fish_ten>=1000){ fish_state=0; fish_streak=0; fish_rc=30; say("THE LINE SNAPPED!"); sfx(4); if(opt_shake) shake_t=3; return 1; }
    if(fish_hp<=0) fish_catch();
    return 1;
}
static void fish_draw(volatile u16 *pg){
    int by = 118 + (sinI[(frames*20)&255]>>12);
    if(fish_state==1 && fish_t<30) by = 118 + ((frames&2)?3:-1);
    else if(fish_state==2) by = 124;
    else if(fish_state==3) by = 108 + (fish_hp*14/fish_hpm) + ((fish_sg&&(frames&1))?2:0);
    for(int y=100;y<by;y+=2) pg[y*120+66]=(R_STONE+6)|((R_STONE+6)<<8);
    for(int y=by;y<by+4;y++){ int c = fish_state==2 ? ((frames&2)?C_RED:216) : 216; pg[y*120+66]=c|(c<<8); pg[y*120+67]=c|(c<<8); }
    if(fish_state==1 && fish_t<30 && (frames&4)) text(pg,120-textlen("NIBBLE...")*2,44,"NIBBLE...",C_CYAN,1);
    if(fish_state==2) text(pg,120-textlen("BITE! PRESS A")*2,44,"BITE! PRESS A",(frames&2)?C_RED:C_WHITE,1);
    if(fish_state==3){
        int ten=fish_ten>1000?1000:fish_ten;
        rect(pg,10,50,8,82,C_DGREY); int hh=ten*80/1000; int col = ten<600?C_CYAN : ten<850?C_GOLD : C_RED;
        rect(pg,11,51+80-hh,6,hh,col);
        rect(pg,80,34,80,4,C_DGREY); rect(pg,80,34,(fish_hpm-fish_hp)*80/fish_hpm,4,C_GOLD);
        if(fish_sg) text(pg,120-8*2,44,"LET GO!",(frames&2)?C_RED:C_WHITE,1);
        else if(fish_sgw<14 && (frames&2)) text(pg,120-1*2,44,"!",C_RED,1);
        else text(pg,120-6*2,44,"REEL!",C_CYAN,1);
    }
}

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
static DObj *named_p=0; static u32 dhud_sig=0; static int lock_ahead=0;
// is there something to bite right in front of you? (same box the bite uses)
static int foe_ahead(s32 dirx, s32 diry){
    const s32 rb_=xo_v(XO_BITERNG);
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue;
        s32 dx=o->x-px_, dy=o->y-py_; if(iabs(dx)>0x40000||iabs(dy)>0x40000) continue; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16);
        s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(side<0) side=-side;
        s32 rng_=(o->kind==K_BOSS?0x22000:0x1A000)*rb_/100;
        if(fwd>0 && fwd<rng_ && side<0xE000) return 1; }
    return 0;
}
static void do_bite(s32 dirx, s32 diry){
    atk_len=(14-d_haste)*2/3; if(atk_len<4) atk_len=4; atk_len=atk_len*100/xo_v(XO_BITESPD); if(atk_len<2) atk_len=2; atk_t=atk_len;        // was 14 (min 6): a bite every 0.9 s was slow
    int crit; int pw=bite_power(&crit); sfx(0);
    int landed=0, killed=0, finisher = combo>=xo_v(XO_CRUNCH);          // every third bite in a row is a CRUNCH: double power, big shove
    if(finisher){ pw*=2; atk_t=atk_len+3; }
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!(is_mon(o->kind)||o->kind==K_BARREL)) continue;
        s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16);
        s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(side<0) side=-side;
        s32 rng_=(o->kind==K_BOSS?0x22000:0x1A000)*xo_v(XO_BITERNG)/100;
        if(fwd>0 && fwd<rng_ && side<0xE000){
            landed=1; knock(o,dirx,diry,(finisher?0x4000:crit?0x1800:0x0C00)*(strong_t?3:2)/2);
            if(o->kind==K_BARREL){ o->flash=6; sfx(13); debris(o->x,o->y,0x4000,3,2,dirx>>4,diry>>4); if(--o->hp<=0) barrel_break(o); continue; }
            int p2=pw; if(o->stag || o->wind){ p2=p2*3/2; }      // a staggered or winding-up foe takes extra
            o->stag = o->kind==K_BOSS ? (crit||finisher?3:0) : (finisher?8:crit?5:3); o->wind=0;
            hit_mon(o,p2,crit||finisher); if(!o->alive) killed=1; }
    }
    if(landed){ hitstop = (finisher ? 8 : killed ? 7 : crit ? 6 : 3)*xo_v(XO_HITSTOP)/100; if((crit||finisher) && opt_shake) shake_t=2;
        if(finisher){ combo=0; say("CRUNCH!"); sfx(12); } else { combo++; combo_t=50; } }
    else if(finisher) combo=0;
}
static int dungeon_frame_(void);
static int dungeon_frame(void){ u32 t=TMR(); int r=dungeon_frame_(); prof[5]=TMR()-t; return r; }
static int dungeon_frame_(void){   // returns 1 to go back up, 2 when you faint, 3 when you warp out
    int freeze = hitstop>0; if(freeze){ hitstop--; wtick=0; }      // a blink of stillness when a hit lands
    if(wtick){ tick_timers(); hunger_tick(40); regen_tick(45); map_reveal(); }
    bag_input();
    if(!skip_draw){ int f=frames; if((f&127)==0) dungeon_palette(); int fl=(sin_tab[(f*29)&255]+sin_tab[(f*47+80)&255])>>12;
      for(int i=0;i<8;i++){ int k=i+(fl>2?1:fl<-2?-1:0); if(k<0)k=0; if(k>7)k=7; BG_PAL[R_FLAME+i]=bg_pal[R_FLAME+k]; }
      for(int i=2;i<7;i++){ int k=i+((((f>>2)+i)&3)==0); BG_PAL[R_WATER+i]=bg_pal[R_WATER+(k>7?7:k)]; BG_PAL[R_CRYSTAL+i]=bg_pal[R_CRYSTAL+((((f>>3)+i*3)%5)==0 && i<6 ? i+1 : i)]; } }
    if(HIT(K_START)){ oam_hide_all(); oam_flush(); char_screen(VISPG); dungeon_palette(); dungeon_hud_both(); }
    int kl=K_LEFT, kr=K_RIGHT, ku=K_UP, kd=K_DOWN, ksl=K_L, ksr=K_R;
    if(dizzy_t){ kl=K_RIGHT; kr=K_LEFT; ku=K_DOWN; kd=K_UP; ksl=K_R; ksr=K_L; }
    if(HELD(K_SEL)){ kl=kr=ku=kd=ksl=ksr=0; }          // SELECT held: d-pad and L/R pick items and spells instead
    if(HELD(kl)||HELD(kr)){ if(turn_held<TURN_RAMP) turn_held++; } else turn_held=0;
    int ts = TURN_MIN + (((TURN_MAX-TURN_MIN)*turn_held)>>4);
    if(HELD(kl)) pang=(pang-ts)&1023;
    if(HELD(kr)) pang=(pang+ts)&1023;
#ifdef TEST_AIM
    { int best=-1; s32 bd=0x7FFFFFFF; for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue; s32 dx=(o->x-px_)>>8, dy=(o->y-py_)>>8, d=dx*dx+dy*dy; if(d>=bd) continue;
          int seen=1; for(int k=1;k<16 && seen;k++) if(solid(px_+(o->x-px_)/16*k, py_+(o->y-py_)/16*k)) seen=0;      // (only monsters you can see)
          if(seen){ bd=d; best=i; } }   // (for filming: face the nearest monster...
      if(best>=0 && bd<(TEST_AIM)*(TEST_AIM)){ s32 dx=(dob[best].x-px_)>>8, dy=(dob[best].y-py_)>>8; int ba=0; s32 bv=-0x7FFFFFFF; for(int a=0;a<256;a++){ s32 v=cosx(a)*dx+sinx(a)*dy; if(v>bv){ bv=v; ba=a; } }
        int diff=((ba*4-pang+512)&1023)-512; if(diff>20) diff=20; if(diff<-20) diff=-20; pang=(pang+diff)&1023; }
#ifdef TEST_AUTOWALK
      else { int bests=-99999, besta=pang;                                               // ...and when none is near, walk on down the most open way)
          for(int da=-320; da<=320; da+=32){ int a=(pang+da)&1023; s32 ex=cosx(a>>2)<<2, ey=sinx(a>>2)<<2; int d=1;
              for(; d<48; d++){ s32 x=px_+(s32)(((long long)ex*d*0x2000)>>16), y=py_+(s32)(((long long)ey*d*0x2000)>>16); if(box_solid(x,y,PRAD)) break; }
              int sc=d*8-(da<0?-da:da)/8; if(sc>bests){ bests=sc; besta=a; } }
          int diff=((besta-pang+512)&1023)-512; if(diff>10) diff=10; if(diff<-10) diff=-10; pang=(pang+diff)&1023; keys|=K_UP; }
#endif
    }
#endif
    s32 dirx=cosx(pang>>2)<<2, diry=sinx(pang>>2)<<2;
    s32 plx=-(diry*43/65), ply=dirx*43/65;
    // AUTO-RUN (the "RUN! DOWN STOPS" hint shows only once per session): double-tap UP and you run forward by yourself, a bit faster, with your thumb free: you can still steer (LEFT/RIGHT), strafe, bite (A) and use items (B).
    // It stops by itself when you tap DOWN, hit a wall, meet a monster, get hurt, start fishing, open a menu or change floor.
    if(!dizzy_t && !HELD(K_SEL) && HIT(ku)){ if(tapU_t && !auto_run){ auto_run=1; run_stuck=0; sfx(11); if(!run_hint){ run_hint=1; say("RUN!  DOWN STOPS"); } } tapU_t=14; }
    if(tapU_t) tapU_t--;
    if(auto_run && (HELD(kd) || HELD(K_SEL) || HIT(K_START) || hurt_flash || dizzy_t || stun_t || fish_state)) auto_run=0;
    if(auto_run){ if(moved_real) run_stuck=0; else if(wtick && ++run_stuck>6) auto_run=0; }          // a wall: stop, do not keep grinding on it
    s32 spf = (HELD(ku) || auto_run) ? WALK_FWD : HELD(kd) ? -WALK_BACK : 0;
    s32 sps = HELD(ksr) ? WALK_SIDE : HELD(ksl) ? -WALK_SIDE : 0;
    if(spf && sps){ spf=spf*3/4; sps=sps*3/4; }          // diagonals are not faster
    spf = spf*(100+d_spd+(endless&&cur_moon?15:0)+have_bug[BUG_CRICKET]*25)/100; sps = sps*(100+d_spd+(endless&&cur_moon?15:0)+have_bug[BUG_CRICKET]*25)/100;
    if(speed_t){ spf=spf*8/5; sps=sps*8/5; }
    if(auto_run){ spf=spf*7/5; sps=sps*7/5; }
    int sp = (spf||sps);
    s32 tvx=0, tvy=0;
    if(sp){ fish_state=0; tvx=(s32)((((long long)dirx*spf)-((long long)diry*sps))>>16); tvy=(s32)((((long long)diry*spf)+((long long)dirx*sps))>>16); }
    // double-tap L or R: a quick dodge dash to that side. you can't be hit while you dash
    if(!dizzy_t && !HELD(K_SEL)){ if(HIT(K_L)){ if(tapL_t && !dodge_t){ dodge_t=9; dodge_dir=-1; invuln=xo_v(XO_DODGEI); sfx(11); dodge_msg_t=30; } tapL_t=14; }
                                  if(HIT(K_R)){ if(tapR_t && !dodge_t){ dodge_t=9; dodge_dir=1; invuln=xo_v(XO_DODGEI); sfx(11); dodge_msg_t=30; } tapR_t=14; } }
    if(tapL_t) tapL_t--; if(tapR_t) tapR_t--; if(dodge_msg_t) dodge_msg_t--;
    if(dodge_t){ dodge_t--; s32 ds=(0x1400*xo_v(XO_DODGED)/100)*dodge_dir; tvx=(s32)((-(long long)diry*ds)>>16); tvy=(s32)(((long long)dirx*ds)>>16); pvx=tvx; pvy=tvy; }
    if(wtick && combo_t && --combo_t==0) combo=0;
    if(!freeze){ player_move(tvx,tvy); bodies_tick(); knock_tick(); if(kick_t) kick_t--; if(bump_cd) bump_cd--; if(pknock_t) pknock_t--; }
    if(wtick) mon_bodies();
    mv_now = sp;
    if(wtick && atk_t) atk_t--;
    if(escape_flag){ escape_flag=0; return 3; }
    // ---- spells
    if(cast_flag>=0){ int s=cast_flag; cast_flag=-1;
        if(!spell_known[s]) say("FIND A SPELL BOOK FIRST");
        else if(mp<spell_cost[s]*xo_v(XO_SPELL)/100){ say("NOT ENOUGH MANA"); }
        else { mp-=spell_cost[s]*xo_v(XO_SPELL)/100;
            if(s==SP_FIRE){ fire_proj(0, px_+(dirx>>2), py_+(diry>>2), 0x7000, (dirx*3)>>5, (diry*3)>>5, clampi(sadd(sadd(6,smul(st_mag,2)),level))); sfx(3); }
            else if(s==SP_SPARK){ int best=-1; s32 bd=0x60000;
                for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue; s32 d=dist8(o->x-px_,o->y-py_); if(d<bd){ bd=d; best=i; } }
                if(best>=0){ DObj *o=&dob[best]; hit_mon(o, clampi(sadd(sadd(5,smul(st_mag,2)),level)), 0); burst(o->x,o->y,0x8000,8,2); spark_t=6;
                    s32 det=(s32)((((long long)plx*diry)-((long long)dirx*ply))>>16); s32 sx=o->x-px_, sy=o->y-py_;
                    s32 tY=(s32)((((long long)-ply*sx)+((long long)plx*sy))/det), tX=(s32)((((long long)diry*sx)-((long long)dirx*sy))/det);
                    if(tY>0x2000){ spark_c=60+(s32)(((long long)tX*60)/tY); spark_y=hz; } else { spark_c=60; spark_y=60; } }
                else say("NOTHING TO ZAP");
                sfx(10); }
            else if(s==SP_HEAL){ hearts+=10+st_mag*3; if(hearts>maxh) hearts=maxh; burst(px_+(dirx>>1),py_+(diry>>1),0x4000,10,1); sfx(6); say("HEALED"); }
            else if(s==SP_SHIELD){ shield_t=900; say("SHIELD UP"); sfx(6); }
            else { escape_flag=1; portal_depth=depth_no; say("A PORTAL OPENS!"); sfx(5); }          // PORTAL: home to the surface; the door brings you back to this floor
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
#ifdef TEST_ZOO
    { static int zg=-1; int g=(int)((vbl_count/160)%10); stun_t=50; hearts=maxh;     // (test: the new monsters, five at a time, lined up in front of you)
      if(g!=zg){ zg=g; int k=0; for(int i=0;i<ndo && k<5;i++){ DObj *o=&dob[i]; if(!o->alive || !is_mon(o->kind)) continue; o->kind=(s8)(K_EARWIG+g*5+k); o->sub=0; o->champ=0;
          s32 lat=(k-2)*0xB000; o->x=px_+(s32)(((long long)dirx*0x24000)>>16)-(s32)(((long long)diry*lat)>>16); o->y=py_+(s32)(((long long)diry*0x24000)>>16)+(s32)(((long long)dirx*lat)>>16); k++; } }
      for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(o->alive && is_mon(o->kind) && (o->kind<K_EARWIG+zg*5 || o->kind>=K_EARWIG+zg*5+5)) o->x=o->y=0x10000; } }
#endif
#ifdef TEST_SPLITBOOM
    if(hearts<maxh/2) hearts=maxh;
    { char b[24]; static int mx=0; int n=0; for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].sub) n++; if(n>mx) mx=n; scpy(b,"SPLITS "); ncat(b,mx); scat(b," NDO "); ncat(b,ndo); say(b); }
#endif
    // ---- the other Buddies: each goes for the monster nearest to it, or keeps its place in a ring round you
    if(wtick) for(int k=0;k<nbud();k++){ Bud *b=&bud[k]; int tg=-1; s32 best=0x48000;
        for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue; if(dist8(o->x-px_,o->y-py_)>0x50000) continue; s32 d=dist8(o->x-b->x,o->y-b->y); if(d<best){ best=d; tg=i; } }
        s32 tx,ty; if(tg>=0){ int a=(k*256/NBUD)&255; tx=dob[tg].x+((cosx(a)*0x0C00)>>14); ty=dob[tg].y+((sinx(a)*0x0C00)>>14); }
        else { s32 side=(k&1)?1:-1, lat=side*(0x5000+(k>>1)*0x4000), fwd=0x1C000+(k>>1)*0x6000; tx=px_+(s32)(((long long)dirx*fwd)>>16)-(s32)(((long long)diry*lat)>>16); ty=py_+(s32)(((long long)diry*fwd)>>16)+(s32)(((long long)dirx*lat)>>16); }   // a V at your sides
        s32 dx=tx-b->x, dy=ty-b->y, d=dist8(dx,dy), st = d>0x20000 ? 0x2000 : d>0x10000 ? 0x1400 : 0x0900;   // (far behind: they hurry)
        if(d>(tg>=0?0x7000:0x3000)){ s32 sx=dx>0x800?st:dx<-0x800?-st:0, sy=dy>0x800?st:dy<-0x800?-st:0; if(!solid(b->x+sx*3,b->y)) b->x+=sx; if(!solid(b->x,b->y+sy*3)) b->y+=sy; }
        if(dist8(px_-b->x,py_-b->y)>0x80000){ b->x=px_; b->y=py_; }
        if(b->cool) b->cool--; if(b->flash) b->flash--;
        if(tg>=0 && best<0xC000 && !b->cool){ hit_mon(&dob[tg], pet_dmg(), 0); b->cool=(s16)(20+(k&3)); b->flash=4; } }
    // ---- monsters, pickups, cages
    s32 named_d=0x60000; if(wtick){ near_cage_p=-1; named_p=0; }
    for(int i=0;wtick && i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; if(o->flash) o->flash--;
        s32 dx=px_-o->x, dy=py_-o->y, d=dist8(dx,dy);
        if(o->kind==K_CORE){ if(d<named_d){ named_p=o; named_d=d; } if(blast_flag && d<0x30000) hit_mon(o,clampi(sadd(8,smul(level,2))),0); continue; }
        if(is_mon(o->kind)){
            const NMon *nm=nm_of(o->kind);
            int boss=o->kind==K_BOSS, bat=o->kind==K_BAT||o->kind==K_GHOST||o->kind==K_MOTH||o->kind==K_WISP, wasp=(nm&&(nm->fl&NF_SHOOT))||o->kind==K_WASP||o->kind==K_TOAD||o->kind==K_DRONE||o->kind==K_SHROOM||o->kind==K_WISP, toad=o->kind==K_TOAD, drone=o->kind==K_DRONE, shroom=o->kind==K_SHROOM, wisp=o->kind==K_WISP;   // ghosts move like bats; toads keep their distance and shoot like wasps
            s32 contact = 0x7800 + body_r(o), reach = contact + 0x2800;     // they walk up until they touch you, then bite
            if((o->champ||boss) && d<named_d){ named_p=o; named_d=d; }
            if(o->venom){ o->venom--; if((o->venom%15)==0){ int vd=1+d_venom/3; o->hp-=vd; add_fnum(o->x,o->y,vd,C_GREEN); if(o->hp<=0){ kill_mon(o); continue; } } }
            if(blast_flag && d<0x30000){ hit_mon(o, clampi(sadd(8,smul(level,2))), 0); if(!o->alive) continue; }
            // go after whoever is closer: you or Buddy
            s32 pdx=pet_x-o->x, pdy=pet_y-o->y, pd = pet_here ? dist8(pdx,pdy) : 0x7FFFFFFF;
            int at_pet = pd < d;
            s32 tdx = at_pet?pdx:dx, tdy = at_pet?pdy:dy, td = at_pet?pd:d;
            if(at_pet){ contact = 0x2800 + body_r(o); reach = contact + 0x2800; }
            if(o->stag) o->stag--;
            if(mboss && boss){                                  // the labyrinth boss: it sleeps, then it hunts you down the corridors
                if(hunt_t){ if(--hunt_t==0){ say("IT HAS YOUR SCENT"); sfx(14); } }
                else if(!stun_t && !o->stag && !o->wind && td>contact+0x800) hunt_move(o, 0x0580+tier_now()*0x30); }
            else if(!stun_t && !o->stag && !o->wind && td<0x70000*xo_v(XO_EAWARE)/100){
                s32 st = nm ? (s32)nm->stp : boss?0x0500 : bat?(0x0700+((frames+i*9)%24<12?0x400:0)) : o->kind==K_BEETLE?0x0380 : o->kind==K_SLIME?(o->sub?0x0480:0x0300) : o->kind==K_SCORP?0x0780 : toad?0x0420 : o->kind==K_ANT?0x0660 : o->kind==K_CENTI?0x0800 : o->kind==K_GOLEM?0x0240 : drone?0x0500 : shroom?0x0300 : o->kind==K_CRAB?0x0340 : 0x0600; st=st*xo_v(XO_ESPEED)/100;
                int away = wasp && td<0x18000, keep = wasp && td<0x40000 && td>=0x18000;
                if(!keep && (td>contact+0x800 || away)){
                    s32 stx=(tdx>0?st:-st), sty=(tdy>0?st:-st); if(away){ stx=-stx; sty=-sty; }
                    if(!solid(o->x+stx*3,o->y)) o->x+=stx; if(!solid(o->x,o->y+sty*3)) o->y+=sty; }
            }
            if(o->cool) o->cool--;
            if(wasp && !stun_t && !o->cool && d<0x60000){        // wasps shoot stingers
                s32 l=isqrt((u32)((dx>>8)*(dx>>8)+(dy>>8)*(dy>>8))); if(l<1) l=1;
                if(toad) fire_proj(1, o->x, o->y, 0x6000, (dx>>8)*0x0B00/l, (dy>>8)*0x0B00/l, mon_dmg(o)*3/2), o->cool=85+rndi(40);
                else if(shroom) fire_proj(1, o->x, o->y, 0x6000, (dx>>8)*0x0900/l, (dy>>8)*0x0900/l, mon_dmg(o)*2), o->cool=95+rndi(40);
                else if(nm) fire_proj(1, o->x, o->y, 0x7000, (dx>>8)*nm->pspd/l, (dy>>8)*nm->pspd/l, mon_dmg(o)*nm->pdm/4), o->cool=nm->pcool+rndi(30);
                else if(wisp) fire_proj(1, o->x, o->y, 0x8000, (dx>>8)*0x1C00/l, (dy>>8)*0x1C00/l, mon_dmg(o)), o->cool=35+rndi(25);
                else if(drone) fire_proj(1, o->x, o->y, 0x8000, (dx>>8)*0x1800/l, (dy>>8)*0x1800/l, mon_dmg(o)), o->cool=40+rndi(20);
                else fire_proj(1, o->x, o->y, 0x8000, (dx>>8)*0x1400/l, (dy>>8)*0x1400/l, mon_dmg(o)), o->cool=55+rndi(30); }
            if(o->wind){                                    // winding up: it stands still, then snaps
                if(--o->wind==0){
                    if(td<reach+0x3000){
                        if(td>0x100){ o->kvx += (s32)(((long long)tdx*(boss?0x1400:0x0C00))/td); o->kvy += (s32)(((long long)tdy*(boss?0x1400:0x0C00))/td); }   // it lunges
                        if(at_pet){ pet_hp-=mon_dmg(o)/2; petobj.flash=6; }
                        else { if(invuln && dodge_msg_t){ say("DODGED!"); st_dodges++; gain_xp(1); }
                               else if(!invuln){ s64 before=hearts; hurt(mon_dmg(o)); if(hearts<before){ hurt_flash=8; hitstop=5; kick_t=7; pknock_t=12; combo=0;
                                   s32 kp = boss?0x2400 : o->kind==K_GOLEM?0x2000 : o->kind==K_CRAB?0x1C00 : o->kind==K_BEETLE?0x1A00 : 0x1000; if(d>0x100){ pvx+=(s32)(((long long)dx*kp)/d); pvy+=(s32)(((long long)dy*kp)/d); } } }
                               if(nm && (nm->fl&NF_LEECH) && !invuln){ o->hp+=mon_dmg(o)/2; if(o->hp>o->maxhp) o->hp=o->maxhp; }   // leeches and reapers drink
                               int ret=d_reflect+d_sting; if(ret){ hit_mon(o,ret,0); if(!o->alive) continue; } }
                    } else { o->stag=3; }                      // it snapped at empty air and is open for a moment
                }
            }
            else if(!wasp && !stun_t && !o->stag && td<reach && !o->cool){ o->cool=boss?34:46; o->wind = nm?nm->wind : boss?3:o->kind==K_BEETLE?6:o->kind==K_SCORP||o->kind==K_CENTI?3:o->kind==K_SLIME?5:o->kind==K_GOLEM?9 : o->kind==K_CRAB?7 : 4; sfx(14); }
            // beetles charge from a distance: a long wind-up, then they barrel at you. into a wall, and they are stunned
            else if((o->kind==K_BEETLE||o->kind==K_SCORP||(nm&&(nm->fl&NF_CHARGE))) && !stun_t && !o->stag && !o->cool && td>0x18000 && td<0x50000 && rndi(o->kind==K_SCORP?55:40)==0){ o->cool=70; o->wind=8; o->lunge=1; sfx(14); }
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
        else if(dist8(px_-p->x,py_-p->y)<0x7000){ p->alive=0; s64 before=hearts; hurt(p->dmg); if(hearts<before){ hurt_flash=8; kick_t=4; pknock_t=6; pvx+=p->vx>>1; pvy+=p->vy>>1; } }
    }
    // ---- A: open a cage, take the stairs, fish, or bite
    int here=dmap[(py_>>16)*MW+(px_>>16)];
    { s32 ax=px_+((dirx*13)>>4), ay=py_+((diry*13)>>4); int tx=ax>>16, ty=ay>>16;
      if(tx>=0&&ty>=0&&tx<MW&&ty<MW && dmap[ty*MW+tx]==D_LOCK){ lock_ahead=1;
          if(HIT(K_A)||(sp && (frames&31)==0)){ int ki=-1; for(int k=0;k<bag_n;k++) if(bag[k].it==IT_KEY){ ki=k; break; }
              if(ki>=0){ dmap[ty*MW+tx]=D_FLOOR; wtex[ty*MW+tx]=WT_STONE; drop_at(ki); say("KEYCARD ACCEPTED. THE DOOR SLIDES OPEN"); sfx(5); hud_now(); build_light(); }
              else say("LOCKED. THE DRONES KEEP A KEYCARD SOMEWHERE"); } }
      else lock_ahead=0; }
    if(here!=D_UP) stair_ok=1;           // you arrive standing on the up stairs: step off once before they work
    int foe = foe_ahead(dirx,diry);
    if(auto_run && foe) auto_run=0;                                  // something to bite right ahead: stop
    int fish_used = fish_step(dirx,diry, foe || lock_ahead || near_cage>=0 || (here==D_UP&&stair_ok) || here==D_DOWN, foe);
    if(fish_used){ }
    else if(lock_ahead && HIT(K_A)){ }
    else if(HELD(K_A) && foe){ if(!atk_t) do_bite(dirx,diry); }          // something in front always gets bitten, and holding A keeps biting
    else if(HIT(K_A)){
        if(near_cage>=0){ dob[near_cage].alive=0; free_bug(dob[near_cage].sub); }
        else if(here==D_UP && stair_ok) return 1;
        else if(here==D_DOWN){ fl_leave(); wacc=WDIV-1; fade(1); dungeon_enter(); dungeon_checkpoint(); music(boss_floor()?4:1); pvx=pvy=0; hitstop=kick_t=pknock_t=0; fade(0); return 0; }
        else if(!atk_t) do_bite(dirx,diry);
    }
    if(hearts<=0) return 2;
    // ---- things that count down or drift: one tick per game step, drawn or not
    if(wtick){
        if(sp && moved_real){ int o_=bob_ph; bob_ph += (speed_t||auto_run)?17:12; if(((o_+64)^(bob_ph+64))&128) sfx(11); }
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
    if(terr_on) render_terrace(pg,dirx,diry,plx,ply); else render_view(pg,dirx,diry,plx,ply);
    u32 tq=TMR(); pq=tq;
    EW static u8 ord[NDO+1]; EW static s32 dd[NDO+1]; int n=0;
    for(int i=0;i<ndo;i++) if(dob[i].alive){ s32 dx=dob[i].x-px_, dy=dob[i].y-py_; dd[i]=(dx>>8)*(dx>>8)+(dy>>8)*(dy>>8); if(dd[i]<12845056) ord[n++]=i; }   // (things more than 14 cells away are not drawn or sorted: keeps big crowds fast)
    if(pet_here){ petobj.kind=K_PET; petobj.alive=1; petobj.x=pet_x; petobj.y=pet_y; s32 dx=pet_x-px_, dy=pet_y-py_; dd[NDO]=(dx>>8)*(dx>>8)+(dy>>8)*(dy>>8); ord[n++]=NDO; }
    for(int k=0;k<nbud() && n<NDO+1;k++){ s32 dx=bud[k].x-px_, dy=bud[k].y-py_; bud_dd[k]=(dx>>8)*(dx>>8)+(dy>>8)*(dy>>8); if(bud_dd[k]<12845056) ord[n++]=(u8)(NDO+1+k); }   // (the other Buddies sort in too)
    #define ODD(t) ((t)>NDO ? bud_dd[(t)-NDO-1] : dd[t])
    for(int i=1;i<n;i++){ u8 t=ord[i]; int j=i; while(j>0 && ODD(ord[j-1])<ODD(t)){ ord[j]=ord[j-1]; j--; } ord[j]=t; }
    for(int i=0;i<n;i++){ int t=ord[i]; if(t>NDO){ static DObj bo; Bud *b=&bud[t-NDO-1]; bo=petobj; bo.kind=K_PET; bo.alive=1; bo.x=b->x; bo.y=b->y; bo.flash=0; draw_sprite(pg,&bo,dirx,diry,plx,ply); }
        else draw_sprite(pg, t==NDO ? &petobj : &dob[t], dirx,diry,plx,ply); }
    for(int i=0;i<NPROJ;i++){ Proj *p=&proj[i]; if(!p->alive) continue;
        if(p->kind==0) draw_glow(pg,p->x,p->y,p->h,0x2400 + (sinI[(frames*40)&255]>>6),R_FLAME+7,R_FLAME+4,dirx,diry,plx,ply);
        else draw_glow(pg,p->x,p->y,p->h,0x0C00,R_CHEST+7,R_CHEST+5,dirx,diry,plx,ply); }
    PPT(0); parts_draw(pg,dirx,diry,plx,ply); PPT(1);
    if(!FILM) draw_fnums(pg,dirx,diry,plx,ply);
    px_=cam_px; py_=cam_py;
    if(spark_t){ int c=60, y=140; for(int k=0;k<12;k++){ int c2=60+(spark_c-60)*(k+1)/12+(k<11?rndi(5)-2:0), y2=140+(spark_y-140)*(k+1)/12;
            int steps=(y-y2)>0?(y-y2):1; for(int s=0;s<=steps;s+=2){ int cc=c+(c2-c)*s/steps, yy=y-s; if(cc>=0&&cc<120&&yy>=18&&yy<160){ pg[yy*120+cc]=(C_WHITE)|(C_WHITE<<8); if(cc+1<120) pg[yy*120+cc+1]=(R_CRYSTAL+7)|((R_CRYSTAL+7)<<8);} }
            c=c2; y=y2; } }
    if(fish_state) fish_draw(pg);
    if(FILM){} else { if(auto_run && (frames&32)) text(pg,4,34,"RUN",C_CYAN,1);        // a small blinking reminder, out of the way
    if(combo){ char cb[12]; scpy(cb,"COMBO X"); ncat(cb,combo+1); text(pg,120-textlen(cb)*2,100,cb,combo>=2?C_GOLD:C_WHITE,1); }
    if(dodge_msg_t) text(pg,120-5*2,108,"DODGE",C_CYAN,1); }
#ifdef TEST_FOE_DBG
    { char db[40]; scpy(db,"ATK "); ncat(db,atk_t); scat(db," FOE "); ncat(db,foe); scat(db," CB "); ncat(db,combo); scat(db," CT "); ncat(db,combo_t); scat(db," HS "); ncat(db,hitstop); text(pg,4,60,db,C_WHITE,1);
      int dl=0; for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive||!is_mon(o->kind)) continue; s32 dx=o->x-px_, dy=o->y-py_; s32 fwd=(s32)(((long long)dx*dirx+(long long)dy*diry)>>16); s32 side=(s32)(((long long)dx*(-diry)+(long long)dy*dirx)>>16); if(fwd>0x40000||fwd<-0x40000||side>0x40000||side<-0x40000) continue; db[0]=0; scpy(db,"K"); ncat(db,o->kind); scat(db," F"); ncat(db,fwd>>8); scat(db," S"); ncat(db,side>>8); scat(db," W"); ncat(db,o->wind); scat(db," ST"); ncat(db,o->stag); text(pg,4,68+dl*7,db,C_WHITE,1); if(++dl>2) break; } }
#endif
    if(FILM){} else { if(sight || have_bug[BUG_ANT] || stairs_seen()) compass(pg,dirx,diry);
    if(opt_minimap) minimap(pg); }
    if(dbg_fps || xo_i(XO_FPS)){ static int lv=0; int d=vbl_count-lv; lv=vbl_count; char b[12]; scpy(b,"FPS "); ncat(b, d>0 ? (60+d/2)/d : 60); text(pg,206,40,b,C_WHITE,1);
#ifdef TEST_CYC
 { static u32 lt=0; u32 nw=TMR(); char c2[16]; c2[0]=0; ncat(c2,(g_w2-g_w0)/1000); scat(c2," ");ncat(c2,(nw-g_w0)/1000); text(pg,150,52,c2,C_WHITE,1); lt=nw; }
#endif
 }
#ifdef SHOW_PROF
    { char pb[48]; pb[0]=0; ncat(pb,prof[0]); scat(pb," "); ncat(pb,prof[3]); scat(pb," "); ncat(pb,prof[1]); scat(pb," "); ncat(pb,prof[2]); scat(pb," "); ncat(pb,prof[7]); text(pg,4,96,pb,C_WHITE,1); pb[0]=0; for(int q=0;q<4;q++){ ncat(pb,pp[q]); scat(pb," "); } text(pg,4,104,pb,C_WHITE,1); }
#endif
    PPT(2);
    if(FILM){} else { hud3d(pg); fl_draw(pg);
    if(named){ char b[32]; b[0]=0; if(named->kind==K_CORE) scpy(b,"OUTPOST CORE"); else if(named->kind==K_BOSS) scpy(b, endless?(hive_boss_floor()?"HIVE OVERLORD":"GUARDIAN"):"THE QUEEN"); else if(named->champ==2) scpy(b,"QUEST TARGET"); else { scpy(b,champ_pre[named->cname&7]); scat(b," "); scat(b,kind_name[named->kind]); }
        int w=textlen(b)*4; text(pg,120-w/2,36,b,C_RED,1); int hw2 = named->hp*40/(named->maxhp?named->maxhp:1); for(int c=40;c<80;c++){ int ix=(c-40)<hw2/2*1+0 ? C_RED : C_DRED; if(c-40<hw2) ix=C_RED; pg[43*120+c]=ix|(ix<<8); } }
    if(msg_timer){ text(pg,120-textlen(msg)*4,24,msg,C_WHITE,2); }
    else if(!opt_hints){}
    else if(lock_ahead) text(pg,120-14*4,24,"A: USE KEYCARD",C_GOLD,2);
    else if(near_cage>=0) text(pg,120-12*4,24,"A: OPEN CAGE",C_WHITE,2);
    else if((here==D_UP && stair_ok)||here==D_DOWN){ const char *m2 = here==D_UP? "A: CLIMB UP" : "A: GO DEEPER"; text(pg,120-textlen(m2)*4,24,m2,C_WHITE,2); }
    else if(fish_state==1) text(pg,120-8*4,24,"WAITING...",C_CYAN,2);
    else if(fish_state==2) text(pg,120-6*4,24,"PRESS A!",(frames&2)?C_RED:C_WHITE,2);
    else if(fish_state==3) text(pg,120-8*4,24,"HOLD A TO REEL",C_CYAN,2);
    else if(water_ahead(dirx,diry)){ char fb[24]; int p=pool_at(dirx,diry); int sk=pool_stock(p); scpy(fb,"A: FISH "); if(sk){ scat(fb,"("); ncat(fb,sk); scat(fb,")"); } else scat(fb,"(EMPTY)"); text(pg,120-textlen(fb)*4,24,fb,C_CYAN,2); }
    if(shield_t && (frames&16)){ for(int c=0;c<120;c+=6){ pg[18*120+c]=(R_CRYSTAL+6)|((R_CRYSTAL+6)<<8); pg[159*120+c]=(R_CRYSTAL+6)|((R_CRYSTAL+6)<<8); } }
    if(hurt_flash){ for(int y=18;y<22;y++) for(int x=0;x<120;x++) pg[y*120+x]=C_RED|(C_RED<<8); } }
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
    PPT(3); prof[7]=TMR()-tq;
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
    enum { TI_CONT, TI_NEW, TI_LOAD, TI_HELP, TI_OPT, TI_EXTRA };
    static const char *const tn[7]={"CONTINUE","NEW GAME","LOAD GAME","HOW TO PLAY","OPTIONS","EXTRAS"};
    volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i];
    bug_build();
    hdma_on=0; title_paint(pg);
    REG_BLDCNT = 0x3F | (3<<6); REG_BLDY = 16;
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    music(2);
    int sel=0, blink=0, dirty=1, full=1, n=0, cs=-1, kwu=0; int it[7]; int kon=0;
    wait_release();
    int fading_in=1;
    while(1){
        if(full){ full=0; title_paint(pg); slots_refresh(); cs=first_good_slot();
            kwu=kw_unlocked(); n=0; if(cs>=0) it[n++]=TI_CONT; it[n++]=TI_NEW; if(cs>=0) it[n++]=TI_LOAD; it[n++]=TI_HELP; it[n++]=TI_OPT; it[n++]=TI_EXTRA;
            if(sel>=n) sel=0; dirty=1; }
        if(dirty){ dirty=0; char b[40];
            title_restore(pg,56,146);
            if(!FILM){ ui_panel(pg,46,58,148,n*9+22); rect(pg,50,62+n*9+1,140,1,U_RAISE); }      // the menu sits on a card
            for(int i=0;i<n && !FILM;i++){ int y=63+i*9; const char *nm=tn[it[i]]; int x=120-textlen(nm)*2;
                if(i==sel){ ui_sel(pg,60,y-2,120,9,U_SEL); stext(pg,x,y,nm,C_GOLD); rect(pg,66,y+1,3,3,C_GOLD); rect(pg,171,y+1,3,3,C_GOLD); }
                else stext(pg,x,y,nm,U_SOFT); }
            int y=62+n*9+5; b[0]=0;
            if(it[sel]==TI_CONT){ const SlotInfo *o=&sinf[cs]; char w[28]; slot_where(o,w); scpy(b,"SLOT "); ncat(b,cs+1); scat(b,"  LV "); ncat(b,o->level); scat(b,"  "); scat(b,w); }
            else if(it[sel]==TI_NEW) scpy(b,"START A NEW ADVENTURE");
            else if(it[sel]==TI_LOAD) scpy(b,"PICK ONE OF YOUR 3 SAVE SLOTS");
            else if(it[sel]==TI_HELP) scpy(b,"THE BUTTONS, AND TIPS");
            else if(it[sel]==TI_EXTRA) scpy(b, kwu ? "KART WORLD AND HOW BIG IS IT?" : "UNLOCKABLES: KART WORLD AND MORE");
            else scpy(b,"SOUND, AUTOSAVE AND MORE");
            b[34]=0; if(!FILM) stext(pg,120-textlen(b)*2,y,b,C_CYAN);
        }
        oam_hide_all(); set_affine(31,128,0,0,128);
        bug_obj(0,200,150-16-((blink>>3)&1),31,128); shadow_oam[0].a2=bug_tile((blink>>4)&1)|BUG_BANK;
        vsync(); oam_flush(); read_keys(); blink++; rng_s += blink;
        if(fading_in){ fading_in=0; fade(0); }
        twinkle(pg,blink);
        { static const u16 code[10]={K_UP,K_UP,K_DOWN,K_DOWN,K_LEFT,K_RIGHT,K_LEFT,K_RIGHT,K_B,K_A}; u16 k=0;        // the Konami code opens the debug menu
          for(int i=0;i<10;i++) if(HIT(code[i])) k=code[i];
          if(k){ if(k==code[kon]) kon++; else kon = (k==K_UP) ? 1 : 0;
              if(kon==10){ kon=0; sfx(6); oam_hide_all(); oam_flush(); int r=dbg_menu(pg); full=1; if(r) return r; wait_release(); continue; } } }
        if(HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; sfx(7); }
        if(HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; sfx(7); }
        if(HIT(K_A)||HIT(K_START)){
            int a=it[sel]; sfx(2);
            if(a==TI_CONT){ cur_slot=cs; return 1; }
            if(a==TI_NEW){ int s2=slot_picker(pg,SP_NEW,0); if(s2>=0){ cur_slot=s2; oam_hide_all(); oam_flush(); if(make_bug(pg,1) && make_ship(pg,1)) return 0; } full=1; }
            if(a==TI_LOAD){ int s2=slot_picker(pg,SP_LOAD,0); if(s2>=0){ cur_slot=s2; return 1; } full=1; }
            if(a==TI_OPT){ oam_hide_all(); oam_flush(); options_screen(pg); full=1; }
            if(a==TI_HELP){ oam_hide_all(); oam_flush(); help_screen(pg); full=1; }
            if(a==TI_EXTRA){ oam_hide_all(); oam_flush(); if(title_extras(pg,cs)==2) return 2; full=1; }
            wait_release();
        }
    }
}

// ================================================================= starting, running and leaving a game
static void arrive_on_planet(void);
static void landing_screen(int welcome){                   // while a world is being built: a quiet card with its name, instead of a black screen
    volatile u16 *pg=(volatile u16*)0x06000000;
    REG_BLDCNT=0x3F|(3<<6); REG_BLDY=16;
    for(int i=0;i<120*160;i++) pg[i]=0;
    for(int i=1;i<6;i++) BG_PAL[i]=bg_pal[i]; for(int i=30;i<39;i++) BG_PAL[i]=bg_pal[i]; BG_PAL[0]=0;
    { u32 r=0x1234567u+(u32)cur_sys*77; for(int k=0;k<70;k++){ r=r*1664525u+1013904223u; int x=(r>>8)%240, y=8+((r>>20)%150); pix(pg,x,y,(k%7)==0?5:(k&1)?3:2); } }
    const char *nm=world_name(); int L=textlen(nm);
    const char *top = welcome ? "WELCOME BACK TO" : "LANDING ON";
    text(pg,120-textlen(top)*2,62,top,C_GREY,1);
    if(L<=28) text(pg,120-L*4,72,nm,C_GOLD,2); else text(pg,120-L*2,74,nm,C_GOLD,1);
    { const char *c = pc_name[wd.cls]; text(pg,120-textlen(c)*2,90,c,C_CYAN,1); }
    text(pg,120-11*2,140,"PLEASE WAIT",C_DGREY,1);
    REG_DISPCNT = 4 | (1<<10);
    for(int i=16;i>=0;i-=4){ REG_BLDY=i; vsync(); } REG_BLDCNT=0;
}
static void start_game(int cont){       // fades out, sets the world up from cur_slot (or fresh) and fades in on the planet
    rng_s ^= vbl_count*2654435761u;
    fade(1);
    gen_cluster();
    if(!(cont && load_game(1))){ cont=0; new_game(); story_dna(0); cam_fp=opt_cam; planet_tables(); gen_cluster(); load_ext_defaults(); play_reset(0); do_save(); }
#ifdef TEST_WORLD
    endless=1; world_no=TEST_WORLD; make_world();
#endif
#ifdef TEST_KW
    kw_unlock(); kart_world();
#endif
#ifdef TEST_SAVE
    {   // set something in every save block, save, wipe it all, load, and check each value came back
        int old_ok; { SaveTv t; memset(&t,0,sizeof t); t.magic=0x4C565254u; t.ly[5]=4242; t.trips=3; t.sum=fnv(&t,sizeof t-4); sram_write(slot_base(cur_slot)+TV_OFF,&t,sizeof t);   // a travel log from the last version
            SaveTv2 z; memset(&z,0,sizeof z); sram_write(slot_base(cur_slot)+TV2_OFF,&z,sizeof z); tv_load(); old_ok = tv_ly[5]==4242 && tv_trips==3; }
        beast_stage=3; tech_lv=4; beast_kills=2; hive_done=2; wing_n=7; relic_bits=0x5; mark_perm=0x3; weird_seen=9;
        form=FORM_HUMAN; form_t=0; forms_open=7; tools_owned=0x15; tool_held=2; hum.style=1; hum.height=4; hum.build=2; hum.made=1; hum.col[0]=0x1234;
        clus[0]=777; clus[5]=999; fuel_tank=3; warp_lv=2; gold=4321; tiers[2]=3; data_cores=5; scrap=33; cargo[3]=4; fuel=55; tr_n[1]=6; bugx.hat=5; bugx.wings=3; humx.top=7; humx.c_hat=0x1357; look.pat=9; tv_trips=12; tv_ly[7]=0x7654321; tv_ly[0]=5; tv_best[6]=77;
        do_save();
        beast_stage=0; tech_lv=0; beast_kills=0; hive_done=0; wing_n=0; relic_bits=0; mark_perm=0; weird_seen=0; form=0; forms_open=1; tools_owned=0; tool_held=0;
        hum.style=0; hum.height=0; hum.build=0; hum.made=0; hum.col[0]=0; clus[0]=0; clus[5]=0; fuel_tank=0; warp_lv=0; gold=0; tiers[2]=0; data_cores=0; scrap=0; cargo[3]=0; fuel=0; tr_n[1]=0; bugx.hat=0; bugx.wings=0; humx.top=0; humx.c_hat=0; look.pat=0; tv_trips=0; tv_ly[7]=0; tv_ly[0]=0; tv_best[6]=0;
        int ok=load_game(1);
        struct { const char *n; int got, want; } c[]={{"LOAD",ok,1},{"BEAST STAGE",beast_stage,3},{"TECH LEVEL",tech_lv,4},{"BEAST KILLS",beast_kills,2},{"HIVES",hive_done,2},{"WINGMEN",wing_n,7},
            {"RELICS",(int)relic_bits,5},{"MARKS",(int)mark_perm,3},{"WEIRD SEEN",weird_seen,9},{"FORM",form,FORM_HUMAN},{"TOOLS",(int)tools_owned,0x15},{"HUM STYLE",hum.style,1},{"HUM HEIGHT",hum.height,4},
            {"HUM COLOUR",hum.col[0],0x1234},{"EAST WORD",(int)clus[0],777},{"NORTH WORD",(int)clus[5],999},{"FUEL TANK",fuel_tank,3},{"WARP DRIVE",warp_lv,2},
            {"GOLD",(int)gold,4321},{"TIER",tiers[2],3},{"DATA CORES",data_cores,5},{"SCRAP",scrap,33},{"CARGO",cargo[3],4},{"FUEL",fuel,55},{"TROPHIES",tr_n[1],6},{"BUG HAT",bugx.hat,5},{"BUG WINGS",bugx.wings,3},{"HUM TOP",humx.top,7},{"HUM HAT COL",humx.c_hat,0x1357},{"PATTERN",look.pat,9},{"TRIPS",(int)tv_trips,12},{"LY TOP WORD",(int)tv_ly[7],0x7654321},{"LY LOW WORD",(int)tv_ly[0],5},{"LONGEST TRIP",(int)tv_best[6],77},{"OLD TRAVEL LOG",old_ok,1}};
        volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10); REG_BLDCNT=0; REG_BLDY=0; for(int i=0;i<120*160;i++) pg[i]=0; for(int i=30;i<39;i++) BG_PAL[i]=bg_pal[i];
        int n=sizeof c/sizeof c[0], bad=0;
        for(int i=0;i<n;i++){ int good=c[i].got==c[i].want; bad+=!good; char b[40]; scpy(b,c[i].n); scat(b,good?" OK":" BAD "); if(!good) ncat(b,c[i].got); text(pg,4+(i/19)*120,4+(i%19)*8,b,good?C_GREEN:C_RED,1); }
        text(pg,124,150,bad?"SOME FAILED":"ALL PASSED",bad?C_RED:C_GREEN,1);
        while(1) vsync();
    }
#endif

    if(dbg_start()) return;                                                              // (the debug menu's GO)
#ifdef TEST_DEEP
    ant_king=1; zones_open=6; cleared_mask=63; galaxy_no=4000000000u; for(int w=0;w<NCW;w++) clus[w]=hmix(w*77+TEST_DEEP); u_settle(); gen_cluster(); cur_sys=7+TEST_DEEP%20; fade(0); arrive_on_planet();
#ifdef TEST_FORM
    form=TEST_FORM; forms_open=7; tools_owned=0xFF; tool_held=1; form_body=TEST_BODY; form_col=rgb15(200,80,200); recalc(); form_palette();
#endif
#ifdef TEST_WARP
    warp_lv=TEST_WARP; fuel_tank=8; fuel=fuel_max();
#endif
#ifdef TEST_MAP
    scene=2; page_back=1; space_enter(1); galaxy_map();
#endif
#ifdef TEST_FOLD
    map_lv=TEST_FOLD&7; fold_lv=(TEST_FOLD>>3)&7; warp_lv=8; fuel_tank=8; fuel=fuel_max(); gold=999999; scene=2; page_back=1; space_enter(1); shx=PR*3; shy=0; shz=0;
#ifdef TEST_FOLDGO
    dp_fold(TEST_FOLDGO); page_back=1; REG_DISPCNT&=~(1<<4); wait_release(); galaxy_map();
#else
    { volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0:0xA000)); int rr=deep_chart(pg); (void)rr; page_back=1; REG_DISPCNT&=~(1<<4); galaxy_map(); }
#endif
#endif
#ifdef TEST_FAVUNI
    fav_add_here(); uni_set32(5); gen_cluster();                                  // a favourite here, then off to universe 5
#endif
#ifdef TEST_UNI
    scene=2; page_back=1; space_enter(1); shx=PR*3; shy=0; shz=0; galaxy_map();      // out in space, away from the planet: try a universe jump
#endif
    return;
#endif
#ifdef TEST_FLEET
    fade(0); arrive_on_planet(); scene=2; page_back=1; wing_n=TEST_FLEET; space_enter(0); if(TEST_FLEET) start_battle(50); return;
#endif
#ifdef TEST_ARRIVE
    cur_sys=TEST_ARRIVE; fade(0); arrive_on_planet(); return;
#endif
#ifdef TEST_SPACE
    fade(0); scene=2; page_back=1; space_enter(1); music(3); return;                  // (filming: straight up into space)
#endif
#ifdef TEST_BEAST
    ant_king=1; zones_open=6; cleared_mask=63; fade(0); scene=2; page_back=1; beast_defaults(); beast_stage=2; tech_lv=3; lair_enter(); return;   // (filming: the 1 trillion HP Technobeast)
#endif
    scene=0; launch_flag=0; enter_dungeon_flag=0; forge_on=0; fish_state=0; msg_timer=0; invuln=0; shake_t=0; page_back=0; loot=0; depth_no=0;
    speed_t=strong_t=dizzy_t=stun_t=shield_t=0;
    restock(); arrow_ship = zone_cleared && !endless;
    landing_screen(cont);
    new_planet(); planet_enter(); show_msg(cont ? "WELCOME BACK" : "");
    fade(1); REG_DISPCNT &= ~(1<<4); page_back=0; planet_frame(); oam_hide_all(); oam_flush(); fade(0);      // the planet is on screen behind the first cards
    music(0);
    if(!cont && !dbg.go){ static const char *const t[6]={"YOU ARE A LITTLE PILL BUG.","THE ANT KING HAS CAGED YOUR BUG FRIENDS.","","FOLLOW THE YELLOW ARROW TO THE DOOR.","GO DOWN. BEAT THE BOSS AT THE BOTTOM.","THEN FLY YOUR SHIP TO THE NEXT STAR."};
        card(VISPG,"BUGSTAR",t,6); bolt_story(VISPG,0); planet_hud(); show_msg("THE GARDEN: FIND THE DOOR"); }
#ifdef TEST_DUNGEON
    enter_dungeon_flag=1;
#endif
    dbg_start2();
#ifdef TEST_CREDITS
    fade(1); credits(); planet_enter(); fade(0);
#endif
#ifdef TEST_BOSS
    zone=TEST_BOSS; portal_depth=zone_floors(); st_str=300; recalc(); hearts=maxh;
#ifdef TEST_BSTR
    st_str=TEST_BSTR; st_vit=300; recalc(); hearts=maxh;                          // (for filming: a longer fight you don't lose)
#endif
#endif
#ifdef TEST_ZONE
    zone=TEST_ZONE; portal_depth=2+TEST_ZONE; st_str=300; st_vit=300; recalc(); hearts=maxh;     // (filming: a floor in another zone)
#endif
#ifdef TEST_KART
    k_cls=1; k_type=0; k_trk=TEST_KART%kt_count(); k_rec=-1; kt_official(k_trk,&kt);
#ifdef TEST_KLAPS
    kt.laps=TEST_KLAPS;
#endif
    kart_go(NRC,-1,0,1);
#endif
#ifdef TEST_KJUMP
    if(nkj){ int p=(kjl[TEST_KJUMP%nkj]-26+knp)%knp; ppx=kp_cx(p); ppy=kp_cy(p); heading=kp_dir(p); heading_rays(); cam_h=ground_h(ppx,ppy)+CAM_UP; rc[0].pi=(s16)p; for(int r=1;r<nrc;r++){ rc[r].x=kp_cx((p+knp-30)%knp); rc[r].y=kp_cy((p+knp-30)%knp); } }
#endif
#ifdef TEST_KBR
    if(nkb){ int pc=kbs[0]+KBR_S, p=(pc-TEST_KBR+knp)%knp;
#ifdef TEST_KBRL
      for(int i=0;i<knp;i++){ int d=i-pc; if(d<0) d=-d; if(d<40 || d>knp-40) continue; int dx=kwrap8(kpx[i]-kpx[pc]), dy=kwrap8(kpy[i]-kpy[pc]); if(dx*dx+dy*dy<=8){ p=(i-TEST_KBR+knp)%knp; break; } }
#endif
      ppx=kp_cx(p); ppy=kp_cy(p); khd=heading=kp_dir(p); heading_rays(); cam_h=ground_h(ppx,ppy)+CAM_UP; rc[0].pi=(s16)p; for(int r=1;r<nrc;r++){ rc[r].x=kp_cx((p+knp-30-r*3)%knp); rc[r].y=kp_cy((p+knp-30-r*3)%knp); rc[r].pi=(s16)((p+knp-30-r*3)%knp); } }
#endif
#ifdef TEST_TOPMAP
    { volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10); for(int y=0;y<160;y++) for(int x=0;x<120;x++){ int tx=(x*2*256/240+((ppx>>15)&255)-60)&255, ty=(y*256/240*2/2+((ppy>>15)&255)-80)&255; int c=cmap[ty*HW+tx]; pg[y*120+x]=c|(c<<8); }
      while(1) vsync(); }
#endif
#ifdef TEST_JSTAT
    { volatile u16 *pg=VISPG; rect(pg,0,0,240,160,1); int n=kt_count(), y=4; char b[64];
      for(int t=0;t<n && y<150;t++){ kt_official(t,&kt); kt_unpaint(); kt_build(); static u8 wet[KP_MAX]; for(int i=0;i<knp;i++) wet[i]=(hm_bil(kp_cx(i),kp_cy(i))>>8)<water_lv; kt_paint(); int gaps=0, bridge=0; for(int i=0;i<knp;i++){ if(kpf[i]&KPF_GAP) gaps++; }
        for(int i=0;i<knp;i++){ if(!(kpf[i]&KPF_GAP) && wet[i]) bridge++; }
        scpy(b,"T"); ncat(b,t); scat(b," N "); ncat(b,knp); scat(b," J "); ncat(b,nkj); scat(b," GAP "); ncat(b,gaps); scat(b," BR "); ncat(b,bridge); scat(b," F "); ncat(b,kt.flags); scat(b," L "); ncat(b,kt_len); scat(b," LP "); ncat(b,nkl); if(nkl){ scat(b," @"); ncat(b,klp[0]); } stext(pg,4,y,b,C_WHITE); y+=8; }
      kt_unpaint(); while(1) vsync(); }
#endif
#ifdef TEST_TOPJ
    { volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10); int j=TEST_TOPJ%(nkj?nkj:1), cx=kpx[kjl[j]], cy=kpy[kjl[j]];
      for(int y=0;y<160;y++) for(int x=0;x<240;x++){ int tx=(cx-30+x/4)&255, ty=(cy-20+y/4)&255; pix(pg,x,y,cmap[ty*HW+tx]); }
      for(int i=0;i<knp;i++){ int dx=kwrap8(kpx[i]-cx)+30, dy=kwrap8(kpy[i]-cy)+20; if(dx<0||dx>=60||dy<0||dy>=40) continue; int c = (kpf[i]&KPF_GAP)?C_RED:(kpf[i]&KPF_RAMP)?C_GOLD:C_WHITE; pix(pg,dx*4+1,dy*4+1,c); pix(pg,dx*4+2,dy*4+1,c); pix(pg,dx*4+1,dy*4+2,c); pix(pg,dx*4+2,dy*4+2,c); }
      { char b[64]; int a=kjl[j]+1, e=kjr[j]-1; scpy(b,"J "); ncat(b,kjl[j]); scat(b,"-"); ncat(b,kjr[j]); scat(b," B1 "); ncat(b,kbend(a-9,a)); scat(b," B2 "); ncat(b,kbend(a,e+10)); scat(b," B3 "); ncat(b,kbend(a-9,e+10)); scat(b," N "); ncat(b,nkj); stext(pg,2,2,b,C_WHITE);
        scpy(b,"D "); for(int i=a-9;i<=e+10;i+=3){ ncat(b,kp_dir(i)); scat(b," "); } stext(pg,2,10,b,C_WHITE);
        scpy(b,"F "); for(int i=a-9;i<=e+10;i++){ ncat(b,kpf[i]); } stext(pg,2,18,b,C_WHITE); }
      while(1) vsync(); }
#endif
#ifdef TEST_CC
    if(TEST_CC) human_creator(VISPG,0); else make_bug(VISPG,0); if(TEST_CC){ form=FORM_HUMAN; recalc(); form_palette(); } planet_hud();
#endif
#ifdef TEST_KMENU
    kart_menu(1);
#endif
#ifdef TEST_GARAGE
    gold=99999; kgarage(VISPG); planet_hud();
#endif
}
static void arrive_on_planet(void){      // after landing (or the autopilot): set the surface up
    g_phz=66;
    land_on_current(); st_planets++; ach_check(); portal_depth=0; scene=0; msg_timer=0;
    landing_screen(0);
    new_planet(); planet_enter(); arrow_ship=0; planet_hud(); restock(); save_game(); fade(1);
    char b[40]; scpy(b,world_name()); if(!endless && !zone_cleared) scat(b,": FIND THE DOOR"); show_msg(b); oam_hide_all(); oam_flush(); music(0);
    { REG_DISPCNT &= ~(1<<4); page_back=0; planet_frame(); oam_hide_all(); oam_flush(); fade(0); planet_card(VISPG); p_last=vbl_count; }
    if(hum_req){ hum_req=0; human_creator(VISPG,1); planet_hud(); save_game(); p_last=vbl_count; }       // a weird world just made you a humanoid: look in the mirror
}
static void pet_away_tick(void){
    if(pet_away_t && --pet_away_t==0){ gold+=pet_sold_g; char b[40]; scpy(b,"BUDDY SOLD "); ncat(b,pet_sold_n); scat(b," THINGS: +"); ncat(b,pet_sold_g); scat(b,"G"); say(b); sfx(7); hud_now(); pet_x=px_; pet_y=py_; }
}
// plays until the player chooses QUIT TO TITLE
static void run_game(void){
    while(1){
        read_keys(); if(scene!=1){ frames++; if(invuln) invuln--; }
        if(dbg_god || xo_i(XO_GOD)){ hearts=maxh; food=100; mp=maxmp; hull=100; shield=20+shield_lv*20; pet_hp=pet_maxhp; }
        if(dbg_fps || xo_i(XO_FPS)){ static int lv=0; int d=vbl_count-lv; lv=vbl_count; char b[12]; scpy(b,"FPS "); ncat(b, d>0 ? (60+d/2)/d : 60); text(VISPG,208,150,b,C_WHITE,1); }
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
            vsync(); if(m7_on){ oam_flush(); m7_after_vsync(); } else { REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1; oam_flush(); }
            if(launch_flag==2){ launch_flag=0; fade(1); oam_hide_all(); oam_flush();
                int d=sys_dist(cur_sys,zone+1); char fn[24], tn[24]; sys_name(cur_sys,fn); sys_name(zone+1,tn); u32 t10[LYW]={(u32)d};
                fuel = d>fuel ? 0 : fuel-d; cur_sys=zone+1; cur_pl=cur_moon=0; visits++;
                scene=2; page_back=1; build_space_palette(); REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); REG_BLDY=0; voyage(fn,tn,t10,(u32)d*24/10+1,0);
                arrive_on_planet(); fade(0); continue; }
            if(launch_flag){ launch_flag=0; fade(1); oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(3); fade(0); continue; }
            if(enter_dungeon_flag){ int fgs=(enter_dungeon_flag==3); if(enter_dungeon_flag==2) base_mode=1; enter_dungeon_flag=0; save_game(); fade(1); oam_hide_all(); oam_flush();
                if(fgs) forge_begin();
                depth_no=0; if(portal_depth>0 && !base_mode){ depth_no=portal_depth-1; portal_depth=0; }
                tier_lock = (!endless && zone_cleared) ? tiers[TZ]+1 : 0;
                scene=1; dungeon_enter();
                if(tier_lock>0){ char tb[40]; scpy(tb,"TIER "); ncat(tb,tier_lock+1); scat(tb,"! MORE FLOORS, TOUGHER BOSS"); say(tb); }
#ifdef TEST_FOE
                for(int k=0;k<2 && ndo<NDO;k++){ DObj *b=&dob[ndo++]; memset(b,0,sizeof(DObj)); b->kind=k?K_BEETLE:K_SPIDER; b->alive=1; b->hp=b->maxhp=k?40:20; b->x=px_+(cosx(pang>>2)<<(k?4:3)); b->y=py_+(sinx(pang>>2)<<(k?4:3)); }
#endif
#ifdef TEST_BOSS
                for(int i=0;i<ndo;i++) if(dob[i].kind==K_BOSS){ dob[i].x=px_+(cosx(pang>>2)<<3); dob[i].y=py_+(sinx(pang>>2)<<3); }
#endif
                pvx=pvy=0; hitstop=kick_t=pknock_t=0; page_back=1; music(boss_floor()?4:1); fade(0); wacc=WDIV-1; dt_last=vbl_count; }
        } else {
            int r=0;
            g_w0=TMR(); int n = vbl_count - dt_last; dt_last = vbl_count;          // 60ths of a second since the last picture
            if(n>=4 && n<=12){ if(!gq_adapt){ gq_adapt=1; ad_t=150; } else ad_t=150; } else if(ad_t>0 && --ad_t==0) gq_adapt=0;
            if(n>12) n = (n>30) ? 1 : 12;                              // after a menu or a load, don't fast-forward
            if(n<1) n=1;
            for(int i=0;i<n;i++){
                wtick = (++wacc>=WDIV);
                if(wtick){ wacc=0; frames++; if(invuln) invuln--; pet_away_tick(); }
                if(i) keys_old=keys;
                skip_draw = (i<n-1);
                if(i==n-1) g_w2=TMR();
                r=dungeon_frame();
                if(r) break;
            }
            skip_draw=0; wtick=1; g_w1=TMR();
            vsync_flip();
            if(scene==1){ REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1; }
            oam_flush();
            if(r){ fade(1); oam_hide_all(); oam_flush(); scene=0; tier_lock=-1; msg_timer=0; fish_state=0;
                   char mb[40];
                   if(forge_on) forge_end(r,mb);
                   else if(r==2){ recalc(); hearts=maxh; food=60; int nolose=!opt_faintloss||opt_baby; if(!endless && !nolose) gold=sdiv(gold,2); loot=0; if(!opt_easy && !nolose){ bag_n=0; bag_sel=0; } portal_depth=0; scpy(mb, (opt_easy||nolose) ? "YOU FAINTED. BUDDY DRAGGED YOU OUT" : endless ? "YOU FAINTED. LOOT AND BAG LOST" : "YOU FAINTED. YOU LOST YOUR BAG"); }
                   else if(endless){ s64 bonus = sdiv(smul(loot,100+depth_no*15),100); gold=sadd(gold,bonus); loot=0; scpy(mb,"ESCAPED B"); ncat(mb,depth_no); scat(mb,": +"); ncat(mb,bonus); scat(mb,"G"); }
                   else if(base_mode && base_card) scpy(mb,"THE OUTPOST IS DEAD");
                   else if(base_mode) scpy(mb,"BACK ON THE SURFACE");
                   else if(r==3 && portal_depth) scpy(mb,"PORTAL OPEN: THE DOOR TAKES YOU BACK");
                   else scpy(mb, zone_cleared?"ZONE CLEARED! GO TO YOUR SHIP":"BACK ON THE SURFACE");
                   if(base_mode){ base_mode=0; portal_depth=0; for(int k=0;k<bag_n;k++) if(bag[k].it==IT_KEY){ drop_at(k); break; } }
                   restock(); save_game();
                   planet_enter(); arrow_ship = zone_cleared && !endless; show_msg(mb); oam_flush(); music(0); fade(0);
                   if(base_card){ base_card=0; int k=data_cores-1; if(k<0) k=0; if(k>7) k=7; bolt_card(VISPG,bolt_mem[k],4);
                       if(data_cores>=8){ static const char *const t[4]={"\"THAT WAS THE LAST OF ME.\"","\"I AM THE DAWN. I KNOW IT NOW.\"","\"FIND THE BEAST. L ON THE GALAXY MAP.\"","\"SHOOT ITS CORE AND I WILL OPEN IT.\""}; bolt_card(VISPG,t,4); }
                       planet_hud(); }
                   if(zone_card){ int z=zone_card-1; zone_card=0;
                       if(z==5){ bolt_story(VISPG,6); fade(1); credits(); planet_enter(); fade(0); }
                       else { static const char *const t[6]={"A NEW STAR IS OPEN.","","1. GO TO YOUR SHIP. FOLLOW THE ARROW.","2. PICK LAUNCH INTO SPACE.","3. PRESS SELECT FOR THE GALAXY MAP.","4. PICK THE NEW STAR AND PRESS A."};
                           card(VISPG,"BOSS BEATEN! ZONE CLEARED",t,6); bolt_story(VISPG,1+z); }
                       planet_hud(); show_msg("GO TO YOUR SHIP"); } }
        }
    }
}

int main(void){
#ifdef TEST_STACK
    { extern u32 __bss_end; u32 *p=&__bss_end; u32 sp_now; __asm__ volatile("mov %0, sp":"=r"(sp_now)); while((u32)p<sp_now-0x100) *p++=0xA5A5A5A5u; }   // (testing: paint the free stack so its deepest use can be seen)
#endif
    REG_DISPCNT = 0x80;
    REG_WAITCNT = 0x4317;
#ifndef NO_LOGO
    logo_play();
#endif                                             // the DIPPINN PRODUCTIONS boot logo (logo.c, the same one BORE plays)
    { volatile u16 *io=(volatile u16*)0x04000000; for(int r=0x08/2;r<0x20/2;r++) io[r]=0; for(int r=0x40/2;r<0x56/2;r++) io[r]=0; }   // undo its layers, scroll, windows and blend
    { volatile u16 *v=(volatile u16*)0x06000000; for(int i=0;i<0x18000/2;i++) v[i]=0; } REG_DISPCNT = 0x80;
    *(volatile u16*)0x04000100=0; *(volatile u16*)0x04000104=0; *(volatile u16*)0x04000106=0x84; *(volatile u16*)0x04000102=0x80;
    sound_init();
    rng_s ^= 0x9E3779B9;
    settings_load(); music_apply();
#ifdef TEST_KW
    kw_defaults(1); kw.coins=50000; kw_store(); new_game(); bug_look_default(); humx_default(); kart_world();
#endif
#ifdef TEST_UNLOCK
    kw_unlock();
#endif
#ifdef TEST_FILM
    opt_hints=0; opt_arrow=0; opt_minimap=0; opt_nums=0;
#endif
#ifdef TEST_SHIPS
    film_ships();
#endif
#ifdef TEST_LAUNCH
    film_launch();
#endif
    while(1){
        in_game=0; sys_req=0; scene=0;
        cur_slot=last_slot;
        new_game(); bug_look_default(); humx_default(); ship_default(); ship_build_model();
        int cont=title_screen();
        if(cont==3) cont = (dbg.go==DG_CONT);                                            // the debug menu: a new game, or the slot it picked
        if(cont==2){ fade(1); kart_world(); oam_hide_all(); oam_flush(); music(-1); REG_BLDCNT=0; REG_BLDY=0; REG_DISPCNT=0x80; continue; }   // Kart World, then back to the title
        start_game(cont);
        in_game=1;
        run_game();
        fade(1); oam_hide_all(); oam_flush(); music(-1);
        REG_BLDCNT=0; REG_BLDY=0; REG_DISPCNT=0x80;      // dark, until the title is painted
    }
}
