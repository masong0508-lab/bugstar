// ================================================================= THE STORMCLOCK COIL: a ship part that flies faster than light
// This file: the state, the speed ladder and the maths (wide integers, saturating: nothing here can overflow). coil2.inc: the quest, the rare drop, the
// glitch show, the screens and the save block. Cheats (debug.inc) are plain RAM flags that are never saved: dbg_coil lends you the coil for this
// session, dbg_sm multiplies your speed, dbg_force makes the rare roll certain. A lent coil is never written to a save (see shx_save).
//
// THE LADDER (multiples of the speed of light, c):  LIGHT 1,  KILO 1,000,  MEGA 1 million,  GIGA 1 billion,  TERA 1 trillion,  LIMIT 13 trillion,
//   STORM 2.3759 x 10^43 (the figure the player asked for; written 23759 x 10^39). STORM does not fit in 64 bits, so every speed is a wide number (coilnum.inc).
// LIMIT is the biggest figure we found for the comic Flash: about 13 trillion times c for the Chongjin evacuation in JLA #89 (a fan/journalist
// estimate: 2.48 quintillion miles a second, from slashfilm.com). We could not find a documented "attosecond run" speed; the attosecond line in
// the comics (Superman #709) is about what he can perceive, not how fast he runs. STORM (2.3759 x 10^43 c) is beyond every figure we found: the Moon is
// then about one Planck time away. The cheats go past it: STORM x 1 million = 2.3759 x 10^49.
// GATING: L+R steps up one tier at a time like always, but the last step (LIMIT to STORM) opens only after you have held LIMIT for 30 seconds in all
// (coil_limit_s, saved). A cheat-lent coil skips that lock (and does not count the seconds). The glitch show: tiers 1 to 6 as before (6 = LIMIT, the
// heavy fake screens), 7 = STORM has the reality-break sequence, 8 = beyond STORM (cheats only).
#include "coilnum.inc"
#define NCT 7
#define COIL_GATE_S 30                                          // seconds held at LIMIT before STORM opens
static const CW coil_mw[NCT+1]={{0,0},{1,0},{1000,0},{1000000,0},{1000000000ULL,0},{1000000000000ULL,0},{13000000000000ULL,0},{2375900000ULL,34}};   // the speed of each tier in c (STORM = 23759 x 10^39)
static const char *const coil_tn[NCT+1]={"OFF","LIGHT","KILO","MEGA","GIGA","TERA","LIMIT","STORM"};
EW static u8 coil_own, coil_warned, coil_qstep, coil_tier, coil_maxtier; EW static u32 coil_rolls, coil_wins, coil_best; EW static u16 coil_limit_s;   // saved (coil2.inc): owned, warning seen, quest step, (tier picked now: not saved); rolls tried, fleet battles won, best glitch tier reached; the highest tier ever engaged; seconds held at LIMIT
EW static u32 coil_passes, coil_pstars, coil_pplanets;       // saved: things passed through, in all, of them stars, of them planets (coil3.inc)
EW static CW coil_cur;                                       // the speed the coil is making now, in whole c (0 = not engaged); ramps up and down
static int dbg_coil=0, dbg_sm=0, dbg_force=0, dbg_sshow=0, dbg_qdone=0;   // the cheats: coil lent for the session, speed step, force the rare roll, speed readout
static const u32 dbg_sm_v[6]={1,2,10,100,1000,1000000}; static const char *const dbg_sm_nm[6]={"OFF","X2","X10","X100","X1,000","X1 MILLION"};
static int coil_storm_open(void){ return dbg_coil || coil_limit_s>=COIL_GATE_S; }      // the last step (LIMIT to STORM): opens after 30 s held at LIMIT; a cheat-lent coil skips the lock
static int coil_avail(void){ return coil_own || dbg_coil; }
static int coil_equipped(void){ return shpx.module==MD_COIL && coil_avail(); }
static int coil_cheated(void){ return dbg_coil || dbg_sm || dbg_force; }
static u64 sat_mul(u64 a, u64 b){ return (b && a > 0xFFFFFFFFFFFFFFFFULL/b) ? 0xFFFFFFFFFFFFFFFFULL : a*b; }
static u64 sat_add(u64 a, u64 b){ return a+b<a ? 0xFFFFFFFFFFFFFFFFULL : a+b; }
// the speed in whole multiples of c (and, below 1 c, in thousandths). spd is the ship's speed number; top its top speed. A ship at full throttle makes 0.6 c.
static CW coil_total_c(int spd, int top, int *milli){
    u32 m = dbg_sm_v[dbg_sm%6]; *milli=0;
    if(!cw_zero(coil_cur)) return cw_mulu(coil_cur,m);
    u64 sub = (u64)(spd<0?0:spd)*600u/(u64)(top<1?1:top); sub*=m; *milli=(int)(sub%1000); return cw_u64(sub/1000);
}
static int coil_gt(CW w){                                     // the glitch tier for a speed: 1 to 6 up to LIMIT and beyond it, 7 = STORM, 8 = beyond STORM (cheats)
    if(cw_zero(w)) return 0;
    int c=cw_cmp(w,coil_mw[NCT]); if(c>0) return 8; if(c==0) return 7;
    int d=cw_decades(w); return d>=13?6 : d>=12?5 : d>=9?4 : d>=6?3 : d>=3?2 : 1; }
static void coil_fmt(char *b, CW w, int milli){              // the speed, exact: "0.42 X LIGHT", "950 X LIGHT", "13,000,000,000,000 X LIGHT", "2.3759 X 10^43 X LIGHT"
    if(cw_zero(w)){ scpy(b,"0."); int h=milli/10; if(h<10){ scat(b,"0"); } ncat(b,h); scat(b," X LIGHT"); return; }
    b[0]=0; cn_wfmt(b,w); scat(b," X LIGHT"); }
static CS coil_speed_cs(CW w, int milli){ return !cw_zero(w) ? cs_from_cw(w) : cs_make((u64)milli,-3); }   // the speed as a number for the derived figures
static const char *const coil_trip_nm[CT_N+1]={"THE MOON","THE SUN","PROXIMA","THE MILKY WAY","ANDROMEDA","THE UNIVERSE","THE GAME MAP","THE PLANET"};
static void coil_trip(char *b, int which, u32 dist_units, CS c){      // the time to a place at speed c: 0..6 the list in coilnum.inc, 7 = the planet here (1 game unit = 2,000 km)
    if(which==CT_N){ cn_time(b, cs_div(cs_div(cs_u((u64)dist_units*2000000ULL),cs_u(299792458ULL)), c.m ? c : cs_u(1))); if(!c.m) scpy(b,"NEVER"); }
    else cn_trip_text(b,which,c); }
static int pt_stretch=256, pt_str_prev=256;                   // the picture's vertical stretch (256 = none) from tides, and last frame's (coil3.inc, drawn by coil_fx_post)
EW static u8 pt_kind, pt_sub, pt_peak, pt_heat, pt_hit; EW static u16 pt_t, pt_len, pt_next; EW static u32 pt_seed;   // the pass-through model's state (coil3.inc)
static int pt_mode(void), pt_vis(void), pt_dmg(void); static void cd_draw(volatile u16 *pg, int pgn, CW w, int milli);
static int coil_quest_done(void);
static void coil_save(void); static void coil_load(void); static void coil_fx_post(volatile u16 *pg); static void coil_fx_end(void); static int coil_try_drop(void);
static void coil_tick(void); static void coil_log(volatile u16 *pg); static void coil_win(void); static u32 coil_hours(u32 hours, const u32 *t10);
static void coil_ctl(void); static s32 coil_vmax(s32 vmax); static void coil_bounds(void); static void coil_toast_draw(volatile u16 *pg); static int coil_sel_eaten=0, coil_start_eaten=0;
static void coil_hud(volatile u16 *pg, CW w, int milli, int gt, int minimal); static int hud_on=0; static s32 hud_m[6]; static void hud_set(s32 pa, s32 pb, s32 pc, s32 pd, s32 X, s32 Y){ hud_m[0]=pa; hud_m[1]=pb; hud_m[2]=pc; hud_m[3]=pd; hud_m[4]=X; hud_m[5]=Y; hud_on=1; } static int coil_detail(volatile u16 *pg); static void pt_reset(void); static void pt_frame(CW w, int gt); static void pt_draw(volatile u16 *pg, int gt, int S);
