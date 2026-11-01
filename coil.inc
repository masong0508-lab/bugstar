// ================================================================= THE STORMCLOCK COIL: a ship part that flies faster than light
// This file: the state, the speed ladder and the maths (wide integers, saturating: nothing here can overflow). coil2.inc: the quest, the rare drop, the
// glitch show, the screens and the save block. Cheats (debug.inc) are plain RAM flags that are never saved: dbg_coil lends you the coil for this
// session, dbg_sm multiplies your speed, dbg_force makes the rare roll certain. A lent coil is never written to a save (see shx_save).
//
// THE LADDER (multiples of the speed of light, c):  LIGHT 1,  KILO 1,000,  MEGA 1 million,  GIGA 1 billion,  TERA 1 trillion,  LIMIT 13 trillion.
// The limit is the biggest figure we found for the comic Flash: about 13 trillion times c for the Chongjin evacuation in JLA #89 (a fan/journalist
// estimate: 2.48 quintillion miles a second, from slashfilm.com). We could not find a documented "attosecond run" speed; the attosecond line in
// the comics (Superman #709) is about what he can perceive, not how fast he runs. The cheats go past it: 13 trillion x 1 million = 1.3e19 still fits in u64.
#define NCT 6
static const u64 coil_mult[NCT+1]={0ULL,1ULL,1000ULL,1000000ULL,1000000000ULL,1000000000000ULL,13000000000000ULL};
static const char *const coil_tn[NCT+1]={"OFF","LIGHT","KILO","MEGA","GIGA","TERA","LIMIT"};
EW static u8 coil_own, coil_warned, coil_qstep, coil_tier; EW static u32 coil_rolls, coil_wins, coil_best;   // saved (coil2.inc): owned, warning seen, quest step, tier picked now (not saved); rolls tried, fleet battles won, best glitch tier reached
EW static u64 coil_cur;                                      // the speed the coil is making now, in whole c (0 = not engaged); ramps up and down
static int dbg_coil=0, dbg_sm=0, dbg_force=0, dbg_sshow=0, dbg_qdone=0;   // the cheats: coil lent for the session, speed step, force the rare roll, speed readout
static const u32 dbg_sm_v[6]={1,2,10,100,1000,1000000}; static const char *const dbg_sm_nm[6]={"OFF","X2","X10","X100","X1,000","X1 MILLION"};
static int coil_avail(void){ return coil_own || dbg_coil; }
static int coil_equipped(void){ return shpx.module==MD_COIL && coil_avail(); }
static int coil_cheated(void){ return dbg_coil || dbg_sm || dbg_force; }
static u64 sat_mul(u64 a, u64 b){ return (b && a > 0xFFFFFFFFFFFFFFFFULL/b) ? 0xFFFFFFFFFFFFFFFFULL : a*b; }
static u64 sat_add(u64 a, u64 b){ return a+b<a ? 0xFFFFFFFFFFFFFFFFULL : a+b; }
// the speed in whole multiples of c (and, below 1 c, in thousandths). spd is the ship's speed number; top its top speed. A ship at full throttle makes 0.6 c.
static u64 coil_total_c(int spd, int top, int *milli){
    u64 m = dbg_sm_v[dbg_sm%6]; *milli=0;
    if(coil_cur>=1) return sat_mul(coil_cur,m);
    u64 sub = (u64)(spd<0?0:spd)*600u/(u64)(top<1?1:top); sub*=m; *milli=(int)(sub%1000); return sub/1000;
}
static int coil_gt(u64 w){ return w>=10000000000000000ULL?7 : w>=10000000000000ULL?6 : w>=1000000000000ULL?5 : w>=1000000000ULL?4 : w>=1000000ULL?3 : w>=1000ULL?2 : w>=1ULL?1 : 0; }   // the glitch tier for a speed
static void coil_num(char *b, u64 v){                        // "950", "12,345", "13 TRILLION", "1.3 QUINTILLION"
    static const char *const nm[5]={"MILLION","BILLION","TRILLION","QUADRILLION","QUINTILLION"}; b[0]=0;
    if(v<1000000ULL){ if(v>=1000){ ncat(b,(int)(v/1000)); scat(b,","); int r=(int)(v%1000); if(r<100) scat(b,"0"); if(r<10) scat(b,"0"); ncat(b,r); } else ncat(b,(int)v); return; }
    u64 d=1000000ULL; int k=0; while(k<4 && v/d>=1000){ d*=1000; k++; }
    ncat(b,(int)(v/d)); u64 fr=(v%d)/(d/10); if(fr){ scat(b,"."); ncat(b,(int)fr); } scat(b," "); scat(b,nm[k]); }
static void coil_fmt(char *b, u64 w, int milli){            // the speed, in friendly words: "0.42 C", "950 X LIGHT", "13 TRILLION X LIGHT"
    if(w==0){ scpy(b,"0."); int h=milli/10; if(h<10) scat(b,"0"); ncat(b,h); scat(b," X LIGHT"); return; }
    coil_num(b,w); scat(b," X LIGHT"); }
static const char *const coil_unit[7]={"SECONDS","MILLISECONDS","MICROSECONDS","NANOSECONDS","PICOSECONDS","FEMTOSECONDS","ATTOSECONDS"};
// how long n units (u = 0 seconds, 1 ms, 2 us ... 6 as) take at M times the speed of light, said simply. n is how long light itself would take.
static void coil_time(char *b, u64 n, int u, u64 M){
    if(M<1) M=1; b[0]=0;
    while(n/M==0 && u<6){ if(M>=1000) M/=1000; else n*=1000; u++; }                   // (a smaller unit until the number is at least 1)
    u64 v=n/M;
    if(v==0){ scpy(b,"LESS THAN AN ATTOSECOND"); return; }
    if(u==0 && v>=60){ u64 yr=v/31557600ULL; if(yr>=1){ coil_num(b,yr); scat(b,yr==1?" YEAR":" YEARS"); return; }
        u64 dy=v/86400; if(dy>=1){ ncat(b,(int)dy); scat(b,dy==1?" DAY":" DAYS"); return; }
        u64 hr=v/3600; if(hr>=1){ ncat(b,(int)hr); scat(b,hr==1?" HOUR":" HOURS"); return; }
        ncat(b,(int)(v/60)); scat(b," MINUTES"); return; }
    if(v<100 && n<0x0FFFFFFFFFFFFFFFULL){ u64 t=n*10/M; ncat(b,(int)(t/10)); if(t%10){ scat(b,"."); ncat(b,(int)(t%10)); } }
    else coil_num(b,v);
    scat(b," "); scat(b,coil_unit[u]); }
// a trip home to the planet (1 game unit = 2,000 km), the next star (4.2 light years) and across the galaxy (100,000 light years)
static void coil_trip(char *b, int which, u32 dist_units, u64 M){
    if(which==0){ u64 km=(u64)dist_units*2000u; coil_time(b, km*1000u/299792u, 1, M); }          // ms of light travel
    else if(which==1) coil_time(b, 132556800ULL, 0, M);                                          // 4.2 light years = 132,556,800 s
    else if(which==2) coil_time(b, 3155760000000ULL, 0, M);                                      // 100,000 light years
    else coil_time(b, 2934000000000000000ULL, 0, M); }                                           // 93 billion light years: the visible universe
static int coil_quest_done(void);
static void coil_save(void); static void coil_load(void); static void coil_fx_post(volatile u16 *pg); static void coil_fx_end(void); static int coil_try_drop(void);
static void coil_tick(void); static void coil_log(volatile u16 *pg); static void coil_win(void); static u32 coil_hours(u32 hours, const u32 *t10);
static void coil_ctl(void); static s32 coil_vmax(s32 vmax); static void coil_bounds(void); static void coil_toast_draw(volatile u16 *pg); static int coil_sel_eaten=0;
