// ================================================================= THE STORMCLOCK COIL, part 2: the quest, the rare drop, the save block, the controls, the glitch show
// (the state and the speed maths are in coil.inc)
// HOW YOU GET IT, in two locks that must BOTH open:
//   1. THE QUEST (8 steps, kept in the quest log: START > EXTRAS > STORMCLOCK COIL). Every step reads a number the game already keeps, so it can never get stuck.
//   2. THE DROP. When the quest is done, any enemy you shoot down in a fleet battle has a 1 in 10,000 chance to drop the coil, but ONLY IF ALL SIX of these
//      are true at the same moment (coil_cond_mask): five enemies already down in this battle, far from home, in an asteroid belt, shield gone and
//      hull at 25 or less, fuel at 10% or less, flying at full speed. coil_drop_roll() is the one place that decides, and a test checks it.
#define NQS 8
static const char *const cq_nm[NQS]={"THE THRONE","EVERY MEMORY","THE BEAST FALLS","FLEET ADMIRAL","THE FAST DRIVES","THE LONG ROAD","THE DEEP","COLLECTOR"};
static const char *const cq_ask[NQS]={"BEAT THE ANT KING","BOLT HAS ALL 8 DATA CORES","DEFEAT THE FULL TECHNOBEAST ONCE","WIN 10 FLEET BATTLES","WARP DRIVE MK8 AND FOLD DRIVE MK3","MAKE 100 TRIPS BETWEEN STARS","REACH DUNGEON FLOOR 60","FIND 12 UNIQUE ITEMS"};
static int cq_prog(int i, int *h, int *n){                  // how far step i is: have / need. Returns 1 when done
    int v=0, g=1;
    switch(i){
    case 0: v=ant_king?1:0; g=1; break;
    case 1: v=(int)data_cores; g=8; break;
    case 2: v=beast_kills>0?1:0; g=1; break;
    case 3: v=coil_wins>1000?1000:(int)coil_wins; g=10; break;
    case 4: v=(warp_lv>=8)+(fold_lv>=3); g=2; break;
    case 5: v=tv_trips>1000?1000:(int)tv_trips; g=100; break;
    case 6: v=(int)best_depth; g=60; break;
    default: v=(int)st_uniq; g=12; break; }
    if(v>g){ v=g; } if(v<0){ v=0; } *h=v; *n=g; return v>=g; }
static int coil_quest_done(void){ return coil_qstep>=NQS || dbg_qdone; }
// the six conditions for the drop, one bit each (bit set = true right now)
static const char *const cd_hint[6]={"A FLEET FALLS AROUND YOU, FIVE DOWN","FAR FROM HOME, OUT IN THE WILD GALAXY","AMONG THE ROCKS OF A BELT","SHIELD GONE, HULL NEARLY DEAD","YOUR TANK NEARLY EMPTY","FLYING AT FULL SPEED"};
static int coil_cond_mask(void){
    int m=0;
    if(battle_on && battle_kills>=5) m|=1;
    if(!home_cluster()) m|=2;
    if(is_belt()) m|=4;
    if(shield<=0 && hull<=25) m|=8;
    if(fuel*10<=fuel_max()) m|=16;
    if(speed*10>=ship_top_speed()*9) m|=32;
    return m; }
// the one place that decides: the quest must be done, you must not own it, all six conditions must hold, and then 1 in 10,000 (or certain with the cheat)
static int coil_drop_roll(int quest_done, int mask, int own, u32 r, int force){
    if(!quest_done || own || mask!=63) return 0;
    return force || (r%10000u)==0; }
// ---------------------------------------------------------------- the save block (slot + 0xC50, in the gap after the ship parts)
#define CO_OFF 0xC50
typedef struct { u32 magic; u8 own, qstep, warned, pad0; u32 rolls, wins, best;
                 u8 maxtier, r1; u16 limit_s; u32 passes, pstars, pplanets; u8 pad[4]; u32 sum; } CoilSave;   // (the newer fields sit in what used to be 20 spare zero bytes: an older save has zeros there, so it still loads, and the sum is the same)
_Static_assert(SHX_OFF+16 <= CO_OFF && CO_OFF+sizeof(CoilSave) <= 0xD00, "the coil block must sit between the ship parts and the kart blocks");
static void coil_reset(void){ coil_own=coil_warned=coil_qstep=coil_tier=coil_maxtier=0; coil_rolls=coil_wins=coil_best=0; coil_limit_s=0; coil_passes=coil_pstars=coil_pplanets=0; coil_cur=cw_u64(0); pt_reset(); }
static void coil_save(void){ CoilSave s; memset(&s,0,sizeof s); s.magic=0x4C494F43u; s.own=coil_own; s.qstep=coil_qstep; s.warned=coil_warned; s.rolls=coil_rolls; s.wins=coil_wins; s.best=coil_best;
    s.maxtier=coil_maxtier; s.limit_s=coil_limit_s; s.passes=coil_passes; s.pstars=coil_pstars; s.pplanets=coil_pplanets;
    s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+CO_OFF,&s,sizeof s); }
static void coil_load(void){ CoilSave s; sram_read(slot_base(cur_slot)+CO_OFF,&s,sizeof s); coil_reset();
    if(s.magic==0x4C494F43u && s.sum==fnv(&s,sizeof s-4)){ coil_own=s.own&1; coil_qstep=s.qstep>NQS?NQS:s.qstep; coil_warned=s.warned&1; coil_rolls=s.rolls; coil_wins=s.wins; coil_best=s.best>8?8:s.best;
        coil_maxtier=s.maxtier>NCT?NCT:s.maxtier; coil_limit_s=s.limit_s; coil_passes=s.passes; coil_pstars=s.pstars; coil_pplanets=s.pplanets; }
    if(shpx.module==MD_COIL && !coil_own){ shpx.module=0; ship_build_model(); } }                // (a lent coil, or a damaged save: it is not on your ship)
static char coil_toast_s[44]; static int coil_toast_t=0;
static void coil_toast(const char *t){ scpy(coil_toast_s,t); coil_toast_s[42]=0; coil_toast_t=210; }
static void coil_toast_draw(volatile u16 *pg){ if(coil_toast_t>0){ coil_toast_t--; int w=textlen(coil_toast_s)*4+8; rect(pg,120-w/2,138,w,11,U_BG0); rect(pg,120-w/2,138,w,1,C_GOLD); text(pg,124-w/2,141,coil_toast_s,C_GOLD,1); } }
static void coil_win(void){ if(coil_wins<100000) coil_wins++; }            // a fleet battle won (space.inc)
static void coil_tick(void){                                // every frame of the game: watch the quest
    static int t=0; t++;
    if(coil_toast_t>0 && scene==0){ coil_toast_draw(VISPG); }
    if(forge_on || (t&31)) return;
    while(coil_qstep<NQS){ int h,n; if(!cq_prog(coil_qstep,&h,&n)) break; char b[44]; scpy(b,"QUEST: "); scat(b,cq_nm[coil_qstep]); scat(b," DONE"); coil_qstep++;
        if(coil_qstep>=NQS){ scpy(b,"THE COIL CAN NOW FALL: SEE EXTRAS"); } coil_toast(b); sfx(6); coil_save(); }
}
// ---------------------------------------------------------------- the glitch engine's own random numbers (never the game's: a glitch must not change the world)
static u32 fx_s=0x1D872B41u;
static u32 fx_rnd(void){ fx_s^=fx_s<<13; fx_s^=fx_s>>17; fx_s^=fx_s<<5; return fx_s; }
static int fx_safe(void){ return xo_i(XO_FSAFE); }           // FLASH-SAFE MODE (options): no strobing, no noise, slow colour changes, small shakes
static int fx_off(void){ return xo_i(XO_GLITCH); }           // GLITCHES: ON / OFF (options)
static void coil_found(void){                                // the drop: it is yours (and written to the cartridge at once, then with every save)
    coil_own=1; coil_save(); sfx(6);
    static const char *const t[5]={"A WRECK BREAKS OPEN. INSIDE: A COIL.","IT IS HUMMING. IT IS ALREADY LATE.","THE STORMCLOCK COIL","FIT IT AT THE HANGAR: SHIP, MODULE.","L+R ENGAGES IT. R OR SELECT SLOWS DOWN."};
    story_card("YOU FOUND THE STORMCLOCK COIL",t,5); }
static int coil_try_drop(void){                              // an enemy you shot down in a fleet battle (space.inc)
    int m=coil_cond_mask(); if(!coil_quest_done() || coil_own || m!=63) return 0;
    if(coil_rolls<0xFFFFFFF0u) coil_rolls++;
    if(!coil_drop_roll(1,m,0,fx_rnd(),dbg_force)) return 0;
    coil_found(); return 1; }
static u32 coil_hours(u32 hours, const u32 *t10){            // a star trip with the coil on the ship takes the time the flight really takes at that speed (never longer than the jump)
    if(!coil_equipped() || !coil_tier) return hours;
    for(int i=1;i<LYW;i++) if(t10[i]) return hours;
    CW M=cw_mulu(coil_mw[coil_tier%(NCT+1)],dbg_sm_v[dbg_sm%6]); if(cw_zero(M)) return hours;
    u64 h = M.e ? 0 : (u64)t10[0]*8766u/10u/M.m; return h<hours ? (u32)h : hours; }       // (a speed above 10^19 c: the trip takes under an hour)
// ---------------------------------------------------------------- controls: engage, step up, drop out (called every flying frame from space_frame)
static int coil_pd(void){ s32 a=shx>>6, b=shy>>6, c=shz>>6; return isqrt((u32)(a*a+b*b+c*c))<<6; }
static void coil_cut(const char *why){                      // back to normal flight, at once. Far out, space snaps back to near the planet.
    if(!cw_zero(coil_cur) && coil_pd()>26000){ s32 pd=coil_pd(); shx=(s32)((s64)shx*14000/pd); shy=(s32)((s64)shy*14000/pd); shz=(s32)((s64)shz*14000/pd); }
    coil_cur=cw_u64(0); coil_tier=0; svx_=svy_=svz_=0; speed=0; pt_reset(); if(why && why[0]) space_say(why); }
static void coil_warn(void);
static void coil_ctl(void){
    static int rhold=0;
    if(HIT(K_START) && !in_lair && (dbg_sshow>0 || (coil_equipped() && (coil_tier || !cw_zero(coil_cur))))){          // START: the SPEED DETAILS pages (coil3.inc)
        coil_start_eaten=1; coil_detail((volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0xA000:0))); }
    if(!coil_equipped() || in_lair){ if(!cw_zero(coil_cur)||coil_tier){ coil_cut(in_lair?"COIL OFF: NOT IN THE LAIR":0); } coil_tier=0; return; }
#ifdef TEST_COILTOUR
    { static int tf=0; tf++; int tt=tf/TEST_COILTOUR; coil_tier = tt<1 ? 0 : tt>NCT ? NCT : tt; }   // (a test tour: a new tier every TEST_COILTOUR frames)
#endif
    rhold = (HELD(K_R) && !HELD(K_L)) ? rhold+1 : 0;
    int chord = (HIT(K_L) && HELD(K_R)) || (HIT(K_R) && HELD(K_L));
    if(chord){
        if(!coil_warned){ coil_warn(); chord=0; }
        else if(fuel<=0) space_say("COIL NEEDS FUEL");
        else if(coil_pd()<PR*4 || dbg_sshow<0) space_say("TOO CLOSE TO THE PLANET FOR THE COIL");
        else if(coil_tier==NCT-1 && !coil_storm_open()){            // the last step is locked until LIMIT has been held for 30 seconds
            char b[40]; scpy(b,"STORM LOCKED: HOLD LIMIT "); ncat(b,coil_limit_s); scat(b,"/"); ncat(b,COIL_GATE_S); scat(b," S"); space_say(b); sfx(4); }
        else { if(coil_tier<NCT) coil_tier++; if(coil_tier>coil_maxtier){ coil_maxtier=coil_tier; }
            char b[40]; scpy(b,"COIL "); scat(b,coil_tn[coil_tier]); scat(b,": "); cn_wfmt(b+textlen(b),coil_mw[coil_tier]); scat(b," X LIGHT"); space_say(b); sfx(3); } }
    if(coil_tier && (HIT(K_SEL) || rhold>=3)){ if(HIT(K_SEL)) coil_sel_eaten=1; coil_cut("COIL OFF. SLOWING DOWN"); rhold=0; }
    else if(coil_tier){ s32 sdx=shx-stx_, sdy=shy-sty_, sdz=shz-stz_; s32 sd=isqrt((u32)((sdx>>6)*(sdx>>6)+(sdy>>6)*(sdy>>6)+(sdz>>6)*(sdz>>6)))<<6;
        s32 pdn=coil_pd();
        if(fuel<=0) coil_cut("COIL OFF: OUT OF FUEL");
        else if(!cw_zero(coil_cur) && pdn<PR*4+speed*24){ coil_cut("COIL OFF: THE PLANET IS CLOSE"); }
        else if(!cw_zero(coil_cur) && sd<2600+speed*24){ coil_cut("COIL OFF: THE STATION IS CLOSE"); }
        else if(hull<=0) coil_cut(0);
        else if(sd<2600 && cw_cmp(coil_cur,cw_u64(2))<0) { /* parked at the station: it does not start */ coil_tier=0; space_say("TOO CLOSE TO THE STATION FOR THE COIL"); } }
    CW tgt = coil_tier ? coil_mw[coil_tier] : cw_u64(0);
    if(cw_cmp(coil_cur,tgt)<0){                                  // speeding up: a twelfth more every frame (halves of it beyond LIMIT, so the push to STORM takes about 3 seconds)
        CW st = cw_add(cw_divu(coil_cur, cw_cmp(coil_cur,coil_mw[6])<0 ? 12 : 2), cw_u64(1)); coil_cur=cw_add(coil_cur,st); if(cw_cmp(coil_cur,tgt)>0) coil_cur=tgt; }
    else if(cw_cmp(coil_cur,tgt)>0){ CW d=cw_add(cw_divu(coil_cur,4),cw_u64(1)); coil_cur = cw_cmp(coil_cur,d)>0 ? cw_sub(coil_cur,d) : cw_u64(0); if(cw_cmp(coil_cur,tgt)<0) coil_cur=tgt; }
    if(!cw_zero(coil_cur) && !coil_cheated() && coil_tier==6 && cw_cmp(coil_cur,coil_mw[6])>=0){          // seconds held at LIMIT (the key to STORM)
        static int lf=0; if(++lf>=60){ lf=0; if(coil_limit_s<65535){ coil_limit_s++;
            if(coil_limit_s==COIL_GATE_S){ coil_toast("STORM UNLOCKED: ONE MORE L+R"); sfx(6); coil_save(); } } } }
    if(!cw_zero(coil_cur)){ crash_cd=2; if((frames&63)==0 && !dbg_fuel){ fuel-=xo_fuel(1); if(fuel<0) fuel=0; } }
}
static s32 coil_vmax(s32 vmax){                              // the engaged coil: velocity swings round to the nose and the top speed grows (4x at LIGHT to 10x at STORM)
    if(cw_zero(coil_cur)) return vmax;
    int fr = cw_ratio256(coil_cur, coil_mw[coil_tier%(NCT+1)]); if(fr<16) fr=16;
    s32 t = vmax + (s32)((s64)vmax*(2+coil_tier)*fr/256);
    s32 vx=(bf[0]*t)>>14, vy=(bf[1]*t)>>14, vz=(bf[2]*t)>>14; svx_+=(vx-svx_)/8; svy_+=(vy-svy_)/8; svz_+=(vz-svz_)/8; return t; }
static void coil_bounds(void){                               // the picture maths holds up to about 130,000 from the star: stay inside 90,000
    const s32 B=90000; if(shx>B){ shx=B; svx_=0; } if(shx<-B){ shx=-B; svx_=0; } if(shy>B){ shy=B; svy_=0; } if(shy<-B){ shy=-B; svy_=0; } if(shz>B){ shz=B; svz_=0; } if(shz<-B){ shz=-B; svz_=0; } }
// ---------------------------------------------------------------- the first-time warning
static void coil_warn(void){
    volatile u16 *pg=(volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0xA000:0));
    rect(pg,0,0,240,160,U_BG0); rect(pg,4,4,232,152,U_PANEL); frame1(pg,4,4,232,152,C_GOLD); frame1(pg,6,6,228,148,C_RED);
    ctext(pg,120,11,"WARNING: THE COIL BREAKS THE PICTURE",C_GOLD);
    static const char *const l[10]={"THE FASTER YOU GO, THE MORE THE SCREEN GOES WRONG:","STAR STREAKS, COLOUR SHIFTS, WOBBLES AND TEARS,","SCRAMBLED NUMBERS, FLASHES, A TILTED OR MIRRORED","WORLD, GARBAGE THAT SNAPS BACK, AND FAKE ERROR,","CRASH AND SAVE-CORRUPTED SCREENS. THE MUSIC WARPS.","","ALL OF IT IS A SHOW. NOTHING TOUCHES YOUR REAL SAVE","OR YOUR GAME. IT NEVER FREEZES. SELECT OR R SLOWS","YOU DOWN AT ANY MOMENT AND IT ALL STOPS.","FLASHING LIGHTS CAN HURT SOME PEOPLE."};
    for(int i=0;i<10;i++) ctext(pg,120,24+i*9,l[i],i==9?C_RED:C_WHITE);
    rect(pg,10,118,220,1,U_RAISE);
    ctext(pg,120,124,"A: TURN ON FLASH-SAFE MODE  (LESS STROBING)",C_CYAN);
    ctext(pg,120,134,"B: LEAVE IT OFF     START: GLITCHES OFF",C_CYAN);
    ctext(pg,120,144,"(BOTH CAN BE CHANGED LATER IN OPTIONS)",U_SOFT);
    wait_release(); int t=0, ch=0;
    while(t<3600){ vsync(); read_keys(); t++; if(HIT(K_A)){ ch=1; break; } if(HIT(K_B)||HIT(K_SEL)) break; if(HIT(K_START)){ ch=2; break; } }   // (a timeout: it never waits for ever)
    if(ch==1) xo[XO_FSAFE]=(u8)(1^XOT[XO_FSAFE].def);
    if(ch==2) xo[XO_GLITCH]=(u8)(1^XOT[XO_GLITCH].def);
    if(ch) settings_save();
    coil_warned=1; coil_save(); wait_release(); sfx(2);
}
// ---------------------------------------------------------------- the glitch show
// Everything below draws on the page that is about to be shown (after the world is drawn), or writes a few palette entries, the BG2 matrix, the music's pitch
// or some spare sprites. It reads nothing that matters and writes nothing that is saved. coil_fx_end() puts every one of those things back.
static int fx_gt=0, fx_t=0, fx_ev=0, fx_evt=0, fx_cool=0, fx_dirty=0, fx_mir=0, fx_tearT=0, fx_tearY=0, fx_tearD=0, fx_nsp=0, fx_pal=0;
EW static u16 fx_row[120];
#define FX_BG2(pa,pb,pc,pd,x,y) do { *(volatile s16*)0x04000020=(s16)(pa); *(volatile s16*)0x04000022=(s16)(pb); *(volatile s16*)0x04000024=(s16)(pc); *(volatile s16*)0x04000026=(s16)(pd); *(volatile u32*)0x04000028=(u32)(x); *(volatile u32*)0x0400002C=(u32)(y); } while(0)
static void coil_fx_end(void){
    if(fx_dirty){
        FX_BG2(256,0,0,256,0,0);                             // the picture, upright and unscaled again
        if(fx_dirty&2){ build_space_palette(); }             // the colours
        if(fx_nsp){ for(int i=120;i<128;i++) *(volatile u16*)(0x07000000+i*8)=0x200; fx_nsp=0; }   // the junk sprites (the last 8 slots)
        fx_pw=0; }
    fx_dirty=0; fx_gt=0; fx_ev=0; fx_evt=0; fx_mir=0; fx_tearT=0; fx_pal=0; pt_stretch=pt_str_prev=256; }
IWRAM static void fx_shift(volatile u16 *row, int d){        // rotate one row of the picture by d halfwords (2 pixels each)
    if(d==0) return;
    if(d<0){ d+=120; }
    if(d<=0||d>=120) return;
    for(int i=0;i<120-d;i++) fx_row[i+d]=row[i];
    for(int i=120-d;i<120;i++) fx_row[i+d-120]=row[i];
    for(int i=0;i<120;i++) row[i]=fx_row[i]; }
static void fx_hex(char *b, u32 v){ int n=textlen(b); for(int i=7;i>=0;i--){ int x=(v>>(i*4))&15; b[n++]=(char)(x<10?'0'+x:'A'+x-10); } b[n]=0; }
static void fx_bars(volatile u16 *pg, int n, int maxw, int cols){  // n coloured bars across the view
    for(int i=0;i<n;i++){ int y=fx_rnd()%128, h=1+fx_rnd()%6; if(y+h>128) h=128-y; rect(pg,fx_rnd()%(240-maxw+1),y,maxw/2+fx_rnd()%(maxw/2+1),h,(int)(49+fx_rnd()%cols)); } }
static void fx_noise(volatile u16 *pg, int y0, int y1, int dim){   // the world as garbage (dim: flash-safe, a dark, calm mosaic that changes only every 40 frames, never noise)
    if(dim){ u32 ph=(u32)(fx_t/40)*0x9E3779B1u;
        for(int y=y0;y<y1;y++){ volatile u16 *r=pg+y*120; for(int i=0;i<120;i++){ u32 h=((u32)(i>>2)*0x85EBCA6Bu)^((u32)(y>>3)*0xC2B2AE35u)^ph; h^=h>>15; h*=0x2C1B3C6Du; h^=h>>13; u16 c=(u16)((h&1)?U_BG1:U_BG0); r[i]=(u16)(c|(c<<8)); } }
        return; }
    for(int y=y0;y<y1;y++){ volatile u16 *r=pg+y*120; for(int i=0;i<120;i++){ r[i]=(u16)fx_rnd(); } } }
static void fx_text_scatter(volatile u16 *pg, const char *s, int c, int amp){ int x=120-textlen(s)*2+(int)(fx_rnd()%(2*amp+1))-amp, y=60+(int)(fx_rnd()%(2*amp+1))-amp; text(pg,x+1,y+1,s,0,1); text(pg,x,y,s,c,1); }
static void fx_event(volatile u16 *pg, int gt, int S){
    char b[48];
    switch(fx_ev){
    case 1: {                                                // MEMORY ERROR: a screen of hex, a few moments
        rect(pg,0,0,240,160,S?57:0); scpy(b,"MEMORY ERROR AT 0X"); fx_hex(b,0x02000000u+((fx_rnd()&0x3FFFFu)&~3u)); text(pg,6,6,b,C_RED,1);
        for(int r=0;r<14;r++){ scpy(b,"0X"); fx_hex(b,0x02000000u+((fx_rnd()&0x3FFFFu)&~3u)); scat(b,"  "); fx_hex(b,fx_rnd()); scat(b," "); fx_hex(b,fx_rnd()); text(pg,6,20+r*8,b,r&1?C_GREEN:C_GREY,1); }
        text(pg,6,140,"CHECKSUM FAILED. (NOT REAL)",C_GOLD,1);
        if(++fx_evt>(S?30:14)){ fx_ev=0; } break; }
    case 2: {                                                // FAKE CRASH: blue screen, then it goes away
        rect(pg,0,0,240,160,57); rect(pg,0,0,240,10,C_WHITE); text(pg,6,2,"BUGSTAR.GBA",U_BG0,1);
        scpy(b,"FATAL ERROR 0X"); fx_hex(b,0xC0DE0000u|(fx_rnd()&0xFFFF)); text(pg,8,18,b,C_WHITE,1);
        text(pg,8,30,"THE SPACE-TIME CHECKSUM FAILED.",C_WHITE,1); text(pg,8,40,"THE GAME HAS STOPPED.",C_WHITE,1);
        text(pg,8,56,"DUMPING MEMORY",C_GREY,1); rect(pg,8,66,160,5,U_TROUGH); rect(pg,8,66,160*fx_evt/(S?150:90),5,C_GREEN);
        for(int r=0;r<5;r++){ scpy(b,"  "); fx_hex(b,fx_rnd()); scat(b,"  "); fx_hex(b,fx_rnd()); text(pg,8,80+r*9,b,C_GREY,1); }
        text(pg,8,138,"IT IS A SHOW. YOU ARE STILL FLYING.",C_GOLD,1); text(pg,8,148,"R OR SELECT: SLOW DOWN",C_CYAN,1);
        if(++fx_evt>(S?150:90)){ fx_ev=0; } break; }
    case 3: {                                                // FAKE SAVE SCARE: the real save is never touched, and it says so at the end
        int t=fx_evt; rect(pg,10,40,220,64,57); frame1(pg,10,40,220,64,t<110?C_RED:C_GREEN);
        if(t<60){ text(pg,20,48,"SAVE DATA CORRUPTED",C_RED,1); text(pg,20,60,"SLOT 1: CHECKSUM MISMATCH",C_WHITE,1); scpy(b,"ERROR 0X"); fx_hex(b,0x5A170000u|(fx_rnd()&0xFFFF)); text(pg,20,72,b,C_GREY,1); }
        else if(t<110){ text(pg,20,48,"TRYING TO REBUILD THE SAVE...",C_GOLD,1); rect(pg,20,64,200,6,U_TROUGH); rect(pg,20,64,200*(t-60)/50,6,C_GOLD); text(pg,20,76,"DO NOT TURN OFF THE POWER",C_WHITE,1); }
        else { text(pg,20,48,"JUST KIDDING.",C_GREEN,1); text(pg,20,60,"YOUR SAVE WAS NEVER TOUCHED.",C_WHITE,1); text(pg,20,72,"THE COIL ONLY PRETENDS.",C_GREY,1); }
        if(++fx_evt>(S?190:160)){ fx_ev=0; } break; }
    case 4: {                                                // THE WORLD AS GARBAGE, then it snaps back
        if(S) fx_noise(pg,0,128,1); else fx_noise(pg,0,128,0);
        fx_text_scatter(pg,"WHERE DID THE STARS GO",C_WHITE,6);
        if(++fx_evt>(S?26:12)){ fx_ev=0; } break; }
    case 5: {                                                // REALITY BREAK: the full sequence, 6 seconds
        int t=fx_evt;
        if(t<90){ u32 keep=fx_s; if(S) fx_s=((u32)(fx_t/45)*0x9E3779B1u)|1u;      // (flash-safe: the same few bars and shifts for 45 frames, never a flicker)
            fx_bars(pg,S?3:10+t/6,200,8); for(int k=0;k<(S?2:4);k++) fx_shift(pg+((fx_rnd()%128)*120),(int)(fx_rnd()%(t/3+3))-(t/6)); fx_text_scatter(pg,"REALITY INTEGRITY FAILING",C_RED,t/12); if(S) fx_s=keep; }
        else if(t<180){ if(S) fx_noise(pg,0,128,1); else fx_noise(pg,(t&1)?0:64,(t&1)?64:128,0);
            static const char *const c[4]={"NOTHING IS WHERE YOU LEFT IT","THE STARS ARE LISTENING","IS THIS STILL THE SAME GAME","DO NOT LOOK AT THE SUN"}; fx_text_scatter(pg,c[((t-90)/23)&3],C_WHITE,3); }
        else if(t<240){ int bar=(t-180)*64/50; if(bar>64) bar=64; rect(pg,0,0,240,bar,0); rect(pg,0,160-bar,240,bar,0); if(t>=230) rect(pg,0,79,240,2,C_WHITE); fx_pw=-60-(t-180)*8; }
        else if(t<300){ rect(pg,0,0,240,160,0); if(t>248) ctext(pg,120,60,"SPACE: NOT FOUND",C_RED); if(t>262) ctext(pg,120,72,"TIME: NOT FOUND",C_RED); if(t>278) ctext(pg,120,84,"SPEED: ???",C_RED); fx_pw=-400; }
        else { int up=(t-300)*160/30; if(up>160) up=160; rect(pg,0,up,240,160-up,0); if(t>335) ctext(pg,120,140,"...AND WE ARE BACK. (NOTHING WAS TOUCHED)",C_GOLD); fx_pw=0; }
        if(++fx_evt>=360){ fx_ev=0; fx_pw=0; fx_cool=(S?1500:900); coil_toast("REALITY RESTORED. THE SAVE IS SAFE."); } break; }
    default: break; }
    (void)gt;
}
static void coil_fx_post(volatile u16 *pg){
    int milli; CW w=coil_total_c(speed,ship_top_speed(),&milli);
    int show = coil_equipped() || dbg_sm || dbg_sshow>0;
    int gt = show ? coil_gt(w) : 0; int gt_all=gt; if(fx_off()) gt=0; if(gt>8) gt=8;
    if(show){ if(gt_all>0) fx_t++; else fx_t=0; pt_frame(w,gt_all); coil_hud(pg,w,milli,gt_all); }
    coil_toast_draw(pg);
    if(gt_all>0 && pt_kind && pt_vis()>0){ pt_draw(pg,gt_all,fx_safe()); if(pt_stretch!=256) fx_dirty|=1; }       // (what you are passing through: coil3.inc; it stays when only the glitches are off)
    if(gt<=0){ if(pt_stretch!=256 || pt_str_prev!=256){ FX_BG2(256,0,0,pt_stretch,0,(80<<8)-pt_stretch*80); fx_dirty|=1; } else if(fx_dirty||fx_gt) coil_fx_end(); pt_str_prev=pt_stretch; fx_gt=0; return; }
    fx_gt=gt; if((u32)gt>coil_best) coil_best=(u32)gt;
    int S=fx_safe(); fx_dirty|=1; if(fx_cool>0) fx_cool--;
#ifdef TEST_COILEV
    if(!fx_ev && fx_t==TEST_COILT){ fx_ev=TEST_COILEV; fx_evt=0; }
#endif
    // ---- tier 1+: star streaks, red and blue shift (inner end blue, outer end red)
    { int n = 8+gt*2; for(int i=0;i<n;i++){ int a=(int)(fx_rnd()&255), r0=12+(int)(fx_rnd()%30), r1=r0+10+gt*8+(int)(fx_rnd()%24); s32 ca=cosx(a), sa=sinx(a);
        int x0=120+(ca*r0>>14), y0=VCY+(sa*r0>>14)*3/4, x1=120+(ca*r1>>14), y1=VCY+(sa*r1>>14)*3/4, xm=120+(ca*((r0+r1)/2)>>14), ym=VCY+(sa*((r0+r1)/2)>>14)*3/4;
        line2(pg,x0,y0,xm,ym,55); line2(pg,xm,ym,x1,y1,gt>=2?53:54); } }
    // ---- tier 2+: the colours cycle; the lines of the picture wobble
    if(gt>=2){ int every = S ? 48 : (gt>=4 ? 1 : 3); if((fx_t%every)==0){ u16 t0=BG_PAL[49]; for(int i=49;i<56;i++) BG_PAL[i]=BG_PAL[i+1]; BG_PAL[56]=t0; fx_dirty|=2; }
        int amp = (gt==2?1 : gt==3?2 : gt==4?4 : gt==5?7 : 12); if(S) amp=(amp+2)/3;
        int step = gt>=4 ? 3 : 4; for(int y=(fx_t&3);y<128;y+=step){ int d=(sinx(y*5+fx_t*11)*amp)>>14; if(d) fx_shift(pg+y*120,d); } }
    // ---- tier 3+: tiles of the picture are copied over each other; bands go blocky; junk sprites; the HUD digits scramble (in coil_hud)
    if(gt>=3){ int nb = S ? gt-1 : gt*3; for(int i=0;i<nb;i++){ int sx=(int)(fx_rnd()%112), sy=(int)(fx_rnd()%120), dx=(int)(fx_rnd()%112), dy=(int)(fx_rnd()%120);
            for(int r=0;r<8;r++) for(int c=0;c<4;c++) pg[(dy+r)*120+dx+c]=pg[(sy+r)*120+sx+c]; }
        if(!S || (fx_t&15)==0){ int y0=(int)(fx_rnd()%110); for(int y=y0;y<y0+16;y+=2){ volatile u16 *r=pg+y*120; for(int i=0;i<120;i++){ u16 v=r[i]&0xFF; r[i]=(u16)(v|(v<<8)); } for(int i=0;i<120;i++) r[120+i]=r[i]; } }
        if(!S){ fx_nsp = gt-1>8?8:gt-1; for(int i=0;i<8;i++){ volatile u16 *o=(volatile u16*)(0x07000000+(120+i)*8);
                if(i<fx_nsp){ o[0]=(u16)((fx_rnd()%140)|0); o[1]=(u16)((fx_rnd()%232)|(1<<14)); o[2]=(u16)((512+fx_rnd()%500)|((fx_rnd()&15)<<12)); } else o[0]=0x200; } fx_dirty|=1; } }
    // ---- tier 4+: the world tilts, mirrors and zooms; bands tear sideways; the music's pitch wobbles; a memory-error screen
    if(gt>=4){ int tilt = (sinx(fx_t*3)*(S?10:(gt>=6?60:30)))>>14, zoom = 256+((sinx(fx_t*5+40)*(S?6:(gt>=6?50:24)))>>14);
        if(!S && (fx_t%90)==0 && (fx_rnd()&1)) fx_mir=1;
        if(!S && (fx_t%90)==45) fx_mir=0;
        if(S && (fx_t%240)==0) fx_mir=!fx_mir;    // (flash-safe: at most one flip every 4 seconds)
        if(fx_ev>=1 && fx_ev<=3){ tilt=0; zoom=256; fx_mir=0; }                                             // (the error and save screens stay upright so they can be read)
        s32 ca=cosx(tilt)*zoom>>14, sa=sinx(tilt)*zoom>>14; int pa=fx_mir?-ca:ca, pb=sa, pc=-sa, pd=ca*pt_stretch>>8;
        s32 X=(120<<8)-(pa*120+pb*80), Y=(80<<8)-(pc*120+pd*80); FX_BG2(pa,pb,pc,pd,X,Y);
        if(fx_tearT>0){ fx_tearT--; for(int y=fx_tearY;y<fx_tearY+14 && y<128;y++) fx_shift(pg+y*120,fx_tearD); } else if((fx_rnd()&15)==0){ fx_tearT=S?6:4; fx_tearY=(int)(fx_rnd()%110); fx_tearD=(int)(fx_rnd()%61)-30; if(S) fx_tearD/=3; }
        fx_pw = fx_ev==5 ? fx_pw : (sinx(fx_t*(3+gt))*(S?gt*4:gt*18))>>14; fx_dirty|=1; }
    // ---- tiers 4 to 7: the fake screens (only one at a time, and a pause after each); the reality break is STORM's alone
    if(fx_ev==0 && fx_cool<=0){
        unsigned r=fx_rnd()%(S?900u:420u);
        if(gt>=7 && r<3){ fx_ev=5; fx_evt=0; }
        else if(gt>=5 && r<6){ fx_ev=4; fx_evt=0; fx_cool=S?500:240; }
        else if(gt>=5 && r<8){ fx_ev=3; fx_evt=0; fx_cool=S?700:420; }
        else if(gt>=5 && r<10){ fx_ev=2; fx_evt=0; fx_cool=S?700:420; }
        else if(gt>=4 && r<14){ fx_ev=1; fx_evt=0; fx_cool=S?400:200; } }
    if(fx_ev==5 && gt<7) { fx_ev=0; fx_pw=0; }
    if(fx_ev) fx_event(pg,gt,S);
    if(gt<4 && (pt_stretch!=256 || pt_str_prev!=256)){ FX_BG2(256,0,0,pt_stretch,0,(80<<8)-pt_stretch*80); }     // (tides stretch the picture even when the glitch show does not tilt it)
    pt_str_prev=pt_stretch;
    if(gt>=8 && !S){ fx_bars(pg,12,230,10); }               // beyond STORM (cheats): colour bars on top of everything
}
// ---------------------------------------------------------------- fuzz: every tier, every pass-through setting, normal and flash-safe, with random keys, through the real flight code
// 4 pass-through modes x 2 (flash-safe off/on) x 9 speeds (off, the 7 tiers, STORM x 1 million) = 72 runs of frames_per_run frames. Checks: the real cartridge is
// never changed (-1), a pass-through never takes the hull below 10 (-2), the speed readout and all five detail pages draw without trouble.
static int fz_pass=0, fz_kinds=0, fz_heat=0, fz_hmin=100, fz_gmax=0;       // what the last fuzz saw: passes started, which kinds (bits), highest heat, lowest hull, highest glitch tier
static int coil_fuzz(int frames_per_run){
    int ok=0, bad=0; u32 sk=keys, so=keys_old; u8 sw=coil_warned; u8 st=coil_tier, so_=coil_own, sm_=shpx.module, xs=xo[XO_FSAFE], xp=xo[XO_PASS], xd=xo[XO_PDMG], xv=xo[XO_PVIS]; CW sc=coil_cur;
    int shl=hull, shs=shield; u16 sls=coil_limit_s; u32 spa=coil_passes, sps=coil_pstars, spp=coil_pplanets;
    u32 h0=fnv((const void*)SRAM,0x8000); coil_own=1; dbg_coil=1; shpx.module=MD_COIL; coil_warned=1; int run=0;
    for(int pm=0;pm<4;pm++) for(int sf=0;sf<2;sf++) for(int tt=0;tt<=NCT+1;tt++,run++){
        xo[XO_FSAFE]=(u8)(sf^XOT[XO_FSAFE].def); xo[XO_PASS]=(u8)(pm^XOT[XO_PASS].def); xo[XO_PDMG]=(u8)((run%4)^XOT[XO_PDMG].def); xo[XO_PVIS]=(u8)(((run/3)%3)^XOT[XO_PVIS].def);
        coil_fx_end(); pt_reset(); dbg_sm=0; coil_tier=0; coil_cur=cw_u64(0); shx=0; shy=0; shz=-PR*14; svx_=svy_=svz_=0; hull=100; shield=shield_max(); fuel=fuel_max();
        for(int f=0;f<frames_per_run;f++){
            keys_old=keys; keys=(u32)(fx_rnd()&0x3FF); keys&=~(K_SEL|K_START); if((fx_rnd()&15)) keys&=~K_R; if((fx_rnd()&255)==0) keys|=K_SEL;       // random keys (SELECT now and then drops out)
            if(tt>=1 && !coil_tier && coil_pd()>=PR*4){ coil_tier=(u8)(tt>NCT?NCT:tt); if(run&1) coil_cur=coil_mw[coil_tier]; }       // (half the runs start at full speed, half ramp up)
            if(tt>NCT) dbg_sm=5;                                                                                              // STORM x 1 million
            if(coil_tier || dbg_sm){ fuel=fuel_max(); }
            int hb=hull;
            int r=space_frame();
            if(r==0) coil_fx_post((volatile u16*)(0x06000000 + (page_back?0xA000:0))); else { coil_fx_end(); space_enter(0); }
            if(pt_kind && pt_t==1){ fz_pass++; fz_kinds|=1<<pt_kind; } if(pt_heat>fz_heat) fz_heat=pt_heat; if(!cw_zero(coil_cur) && hull<fz_hmin) fz_hmin=hull; if(fx_gt>fz_gmax) fz_gmax=fx_gt;
            if(!cw_zero(coil_cur) && hull<10 && hb>=10) bad|=2;                                                                  // a pass-through must never take the hull below 10
            if(hull<10){ hull=100; }
            if((f&63)==31){ int mi; CW w=coil_total_c(speed,ship_top_speed(),&mi); volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0)); cd_draw(pg,(f>>6)%5,w,mi); }   // (a detail page: the five of them in turn)
            vsync(); REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1;
            ok++; }
        coil_fx_end(); }
    dbg_sm=0; dbg_coil=0; coil_tier=st; coil_cur=sc; keys=sk; keys_old=so; coil_warned=sw; coil_own=so_; shpx.module=sm_; xo[XO_FSAFE]=xs; xo[XO_PASS]=xp; xo[XO_PDMG]=xd; xo[XO_PVIS]=xv; ship_build_model();
    hull=shl; shield=shs; coil_limit_s=sls; coil_passes=spa; coil_pstars=sps; coil_pplanets=spp; pt_reset();
    if(fnv((const void*)SRAM,0x8000)!=h0) return -1;                                          // (nothing in the cartridge may change)
    if(bad) return -2;
    return ok; }
// ---------------------------------------------------------------- the quest log (START > EXTRAS > STORMCLOCK COIL). A shows the next page, B leaves.
static void coil_log(volatile u16 *pg){
    int pgn=0, dirty=1, t=0; wait_release();
    while(1){
        if(dirty){ dirty=0; rect(pg,0,0,240,160,U_BG0); ui_bg(pg,0,160); ui_panel(pg,4,4,232,152); ui_head(pg,9,8,200,"THE STORMCLOCK COIL",C_UNIQ);
            char b[64]; int y=20;
            if(coil_own){ rtext(pg,231,8,"OWNED",C_GREEN); }
            else if(dbg_coil){ rtext(pg,231,8,"CHEAT",C_GOLD); }
            else rtext(pg,231,8,coil_quest_done()?"QUEST DONE":"LOCKED",coil_quest_done()?C_GOLD:C_RED);
            if(pgn==0){
                text(pg,10,y,"A SHIP PART THAT BREAKS THE SPEED OF LIGHT. IT IS NOT",C_WHITE,1); y+=8; text(pg,10,y,"SOLD. TWO LOCKS: FINISH THE QUEST, THEN WAIT FOR A DROP.",C_WHITE,1); y+=11;
                int cur=coil_qstep; for(int i=0;i<NQS;i++){ int h=0,n=1; int dn=i<cur; if(i==cur) cq_prog(i,&h,&n);
                    ex_mark(pg,10,y,dn); text(pg,22,y,i<=cur ? cq_nm[i] : "???", dn?C_GREEN : i==cur?C_WHITE:C_GREY,1);
                    if(i==cur && !coil_quest_done()){ scpy(b,cq_ask[i]); text(pg,22,y+8,b,C_GOLD,1); scpy(b,""); ncat(b,h); scat(b,"/"); ncat(b,n); rtext(pg,231,y,b,C_CYAN); y+=8; }
                    y+=9; }
                if(coil_quest_done()) text(pg,10,y+2,"QUEST DONE! NOW THE HARD PART. A: SEE THE HINTS",C_GOLD,1);
                else text(pg,10,y+2,"EACH STEP OPENS WHEN YOU DO IT. NO SPOILERS YET.",C_GREY,1);
            } else {
                if(!coil_quest_done()){ text(pg,10,y,"FINISH THE QUEST TO SEE WHAT IT WANTS.",C_GREY,1); y+=12; text(pg,10,y,"IT WILL NEVER FALL BEFORE THEN.",C_GREY,1); }
                else {
                    text(pg,10,y,coil_own?"YOU HAVE IT. IT FELL WHEN ALL SIX WERE TRUE:":"AN ENEMY IN A FLEET BATTLE CAN DROP IT, ONE SHOT IN",C_WHITE,1); y+=8;
                    if(!coil_own){ text(pg,10,y,"10,000, BUT ONLY WHEN ALL SIX HOLD AT THE SAME TIME:",C_WHITE,1); } y+=11;
                    int m=coil_cond_mask(); for(int i=0;i<6;i++){ ex_mark(pg,10,y,(m>>i)&1); text(pg,22,y,cd_hint[i],(m>>i)&1?C_GREEN:C_GREY,1); y+=10; }
                    y+=2; scpy(b,"TRIES SO FAR: "); ncat(b,coil_rolls); text(pg,10,y,b,C_CYAN,1); y+=9;
                    text(pg,10,y,coil_own?"FIT IT: SHIP, HANGAR, MODULE. L+R ENGAGES.":"GREEN = TRUE RIGHT NOW. HOLD ALL SIX AND KEEP SHOOTING.",C_GOLD,1); }
            }
            ui_lhint(pg,120,146,pgn==0?"A: HINTS   B: BACK":"A: THE QUEST   B: BACK",C_CYAN); }
        t++; vsync(); read_keys();
        if(HIT(K_A)){ pgn^=1; dirty=1; sfx(7); }
        if(HIT(K_B)||HIT(K_START)) break;
    }
    wait_release(); }
// a check of the cartridge block (the tests call it): save, forget, load; then a damaged block must load as "nothing". Returns 0 when all is well.
__attribute__((unused)) static int coil_sramtest(void){
    u8 o=coil_own, q=coil_qstep, w=coil_warned, mt=coil_maxtier; u32 r=coil_rolls, wi=coil_wins, b=coil_best, pa=coil_passes, ps=coil_pstars, pp=coil_pplanets; u16 ls=coil_limit_s; int bad=0;
    coil_own=1; coil_qstep=5; coil_warned=1; coil_rolls=12345; coil_wins=77; coil_best=8; coil_maxtier=7; coil_limit_s=31; coil_passes=99999; coil_pstars=4321; coil_pplanets=765; coil_save();
    coil_reset(); coil_load(); if(!(coil_own==1 && coil_qstep==5 && coil_warned==1 && coil_rolls==12345 && coil_wins==77 && coil_best==8 && coil_maxtier==7 && coil_limit_s==31 && coil_passes==99999 && coil_pstars==4321 && coil_pplanets==765)) bad|=1;
    SRAM[slot_base(cur_slot)+CO_OFF+5]^=0x55;                                  // damage one byte
    coil_load(); if(coil_own || coil_qstep || coil_rolls || coil_limit_s) bad|=2;
    { XSet cur; sram_read(XS_OFF,&cur,sizeof cur); u8 sx[NXO]; memcpy(sx,xo,NXO);                                   // the options block: an older save (48 options) must load, and the new ones round-trip
      struct { u32 magic; u8 v[XO_OLDN]; u32 sum; } ob; memset(&ob,0,sizeof ob); ob.magic=0x584F5053; for(int i=0;i<XO_OLDN;i++) ob.v[i]=(u8)(XOT[i].n>1 ? (1^XOT[i].def) : XOT[i].def);
      ob.sum=fnv(&ob,sizeof ob-4); sram_write(XS_OFF,&ob,sizeof ob); memset(xo,0,NXO); xo_load();
      for(int i=0;i<XO_OLDN;i++) if(xo[i]!=ob.v[i]) bad|=8;
      for(int i=XO_OLDN;i<NXO;i++) if(xo[i]) bad|=8;
      xo[XO_PASS]=(u8)(2^XOT[XO_PASS].def); xo[XO_PDMG]=(u8)(3^XOT[XO_PDMG].def); xo[XO_PVIS]=(u8)(1^XOT[XO_PVIS].def); xo_save(); memset(xo,0,NXO); xo_load();
      if(!(pt_mode()==2 && xo_i(XO_PDMG)==3 && xo_i(XO_PVIS)==1 && xo[XO_FSAFE]==ob.v[XO_FSAFE])) bad|=16;
      sram_write(XS_OFF,&cur,sizeof cur); memcpy(xo,sx,NXO); }
    SRAM[slot_base(cur_slot)+CO_OFF+5]^=0x55;
    { typedef struct { u32 magic; u8 own, qstep, warned, pad0; u32 rolls, wins, best; u8 pad[20]; u32 sum; } OldCoilSave;     // a save from before STORM existed: the same bytes with zeros where the new fields are
      OldCoilSave os; memset(&os,0,sizeof os); os.magic=0x4C494F43u; os.own=1; os.qstep=8; os.warned=1; os.rolls=555; os.wins=12; os.best=6; os.sum=fnv(&os,sizeof os-4);
      _Static_assert(sizeof(OldCoilSave)==sizeof(CoilSave), "the coil block keeps its size");
      sram_write(slot_base(cur_slot)+CO_OFF,&os,sizeof os); coil_reset(); coil_load();
      if(!(coil_own==1 && coil_qstep==8 && coil_warned==1 && coil_rolls==555 && coil_wins==12 && coil_best==6 && coil_maxtier==0 && coil_limit_s==0 && coil_passes==0)) bad|=4; }
    coil_own=o; coil_qstep=q; coil_warned=w; coil_rolls=r; coil_wins=wi; coil_best=b; coil_maxtier=mt; coil_limit_s=ls; coil_passes=pa; coil_pstars=ps; coil_pplanets=pp; coil_save(); return bad; }
