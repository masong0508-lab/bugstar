// sim_glue.inc: the side game and the big level, inside the game's own file. build.sh appends this to pc/gen/main.c.
// It is the ONLY place that sees the game's variables. The logic lives in sim_core.c, god.c, lv.c, lvxp.c (plain C, tested alone).
//
// Rules (DEPTH_PLAN section 3.4):
//   - nothing here calls rnd(), rndi() or wr(): the side game has its own random numbers (sim_rng), so worlds and saves never change;
//   - with the SIDE GAME option OFF (or BUGSTAR_SIDE=0) every sim_* hook returns at once and the action game is byte for byte the old one;
//   - the side game's own save is <sav>.sim. The 32 KB .sav is never touched.
#include "sim/sim_core.h"
#include "sim/lvxp.h"
#include "sim/sim_io.h"
#include "content/pcx.h"
static SimColony *sim_colony(void);

// ---------------------------------------------------------------- state
static Sim sim_w;                       // the town of the planet you are on (rebuilt when the planet changes)
static SimFile sim_f;                   // the .sim file in memory (all three slots)
static int sim_have_w, sim_f_ok, sim_env_done, sim_env_side=-1;
static u64 sim_cal;                     // sim ticks played in this slot
static u32 sim_seen_log;                // events already looked at for unlock messages
static u64 sim_dump_at;                 // next tick for the test dump
static int sim_fpo[SIM_MAXFOLK];        // which po[] each townsfolk is
static s32 sim_fx[SIM_MAXFOLK], sim_fy[SIM_MAXFOLK];       // where it stands (16.16), kept here so the old random walk cannot move it
static s32 sim_tgx[SIM_MAXFOLK][SIMP_N], sim_tgy[SIM_MAXFOLK][SIMP_N];   // where each logical place is, per bug
static u16 sim_stuck[SIM_MAXFOLK];
static int sim_nhome;
static int sim_said_letters, sim_col_cards, sim_col_gifts;   // letter pop-ups and colony cards shown (the tests read them)            // letter pop-ups shown (the tests read it)
static int sim_a_up;                    // 1 once A has been seen up on this planet: keeps a key still held from the title or a menu from opening the quick menu

static int sim_is_on(void){
    if(sim_env_side<0){ const char *e=sim_io_env("BUGSTAR_SIDE"); sim_env_side = !(e && (e[0]=='0' || e[0]=='o' || e[0]=='O' || e[0]=='n' || e[0]=='N')); }
    return sim_env_side && xo_v(XO_SIDE);
}

// ---------------------------------------------------------------- the big level (Lv): beyond the game's 64-bit level, up to 64^256
// The game's `level` (s64) is the low part: it is min(true level, LEVEL_CAP) and LEVEL_CAP is the s64 limit on the PC build.
// From level 500,000,000 up (where 18 x L x L stops fitting in 64 bits) experience is counted here, in Lv, instead of by the game's loop.
#define SIM_BIGSTART 500000000LL
static Lv sim_big_lv, sim_big_bank;     // the true level and the xp bank (both valid when sim_big_act)
static int sim_big_act;

static int sim_big_over(void){ return sim_big_act && level>=LEVEL_CAP && lv_cmp_u64(&sim_big_lv,(u64)LEVEL_CAP)>0; }
static void sim_level_lv(Lv *o){ if(sim_big_act && level>=LEVEL_CAP) *o=sim_big_lv; else lv_from_u64(o, level<1 ? 1 : (u64)level); }
static void sim_big_set(const Lv *v){                       // sets the character's level from an Lv (used by the load, the test hook)
    if(lv_cmp_u64(v,(u64)LEVEL_CAP)>=0){ level=LEVEL_CAP; sim_big_lv=*v; }
    else { level=(s64)lv_to_u64(v); if(level<1) level=1; lv_from_u64(&sim_big_lv,(u64)level); }
    sim_big_act = level>=SIM_BIGSTART; lv_zero(&sim_big_bank); xp=0;
}
static unsigned sim_xp_pct(void){ return (unsigned)xo_v(XO_XPNEED); }
static int sim_big_xp(void){
    if(level<SIM_BIGSTART){ sim_big_act=0; return 0; }
    if(!sim_big_act){ sim_big_act=1; lv_from_u64(&sim_big_lv,(u64)level); lv_zero(&sim_big_bank); }
    if(xp>0){ Lv a; lv_from_u64(&a,(u64)xp); lv_add(&sim_big_bank,&sim_big_bank,&a); xp=0; }
    if(level<LEVEL_CAP) lv_from_u64(&sim_big_lv,(u64)level);
    Lv z, g; lv_zero(&z); lvxp_gain(&sim_big_lv,&sim_big_bank,&z,sim_xp_pct(),&g);
    if(!lv_is_zero(&g)){
        if(lv_cmp_u64(&sim_big_lv,(u64)LEVEL_CAP)>=0) level=LEVEL_CAP; else level=(s64)lv_to_u64(&sim_big_lv);
        Lv t; lv_mul_small(&t,&g,7); u64 v=lv_to_u64(&t); if(v>(u64)S64_MAX) v=(u64)S64_MAX; stat_pts=sadd(stat_pts,(s64)v);       // about 7 points a level: 5, +5 every 5th, +10 every 10th
        recalc(); hearts=maxh; mp=maxmp; { u64 gg=lv_to_u64(&g); if(gg>100000) gg=100000; if(pet_maxhp<1000000000) pet_maxhp+=(int)gg*4; } pet_hp=pet_maxhp;
        char b[40]; scpy(b, lv_cmp_u64(&g,1)==0 ? "LEVEL UP! LV " : "SURGE! LV "); { char c[24]; lv_compact(&sim_big_lv,c,14); scat(b,c); } say(b); sfx(6);
    }
    return 1;
}
static void sim_lv_fix(char *lv){ if(sim_big_over()) lv_compact(&sim_big_lv,lv,5); }            // the top bar has room for 5 letters
static void sim_ncat_level(char *d){ if(sim_big_over()){ char c[24]; lv_compact(&sim_big_lv,c,10); scat(d,c); } else ncat(d,level); }
static long long sim_xpv(long long v){ if(!sim_big_act) return v; Lv n; lvxp_need(&sim_big_lv,sim_xp_pct(),&n); return (long long)lvxp_ratio(&sim_big_bank,&n,1000); }
static long long sim_xpm(long long v){ return sim_big_act ? 1000 : v; }

// ---------------------------------------------------------------- the world seed of the side game
static u64 sim_world_seed(void){
    u64 a=((u64)wd.tseed<<32)|wd.cseed, b=((u64)body_salt()<<32)|sys_id(cur_sys), c=((u64)gsk(1)<<32)|(u32)galaxy_seed;
    return sim_mix64(sim_mix64(a) ^ sim_mix64(b+1) ^ sim_mix64(c+2));
}

// ---------------------------------------------------------------- the .sim file
static void sim_store_slot(void){                       // memory only: what the slot has learned so far
    SimSlot *sl=&sim_f.slot[cur_slot%SIM_SLOTS];
    sl->used=1; sl->cal_ticks=sim_cal;
    if(sim_have_w){ sim_save_friends(&sim_w,sl); sl->god=sim_w.god; sl->spirit=sim_w.spirit; sim_save_tail(&sim_w,sl); }
    if(sim_big_act && level>=LEVEL_CAP){ sl->has_lv=1; sl->lv_true=sim_big_lv; sl->lv_xp=sim_big_bank; } else sl->has_lv=0;
}
static void sim_save(void){
    sim_a_up=0;                                          // (a save comes with every landing: the key that landed us may still be held)
    if(!pc_sav_path()) return;
    if(!sim_is_on() && !sim_big_act) return;            // OFF and nothing to remember: no file at all
    sim_store_slot();
    static u8 buf[SIMFILE_MAX]; size_t n=simfile_write(&sim_f,buf,sizeof buf);
    if(n) sim_io_store(buf,n);
}
static void sim_load(void){
    static u8 buf[SIMFILE_MAX]; size_t n=sim_io_load(buf,sizeof buf);
    if(!n || simfile_read(&sim_f,buf,n)<0) simfile_init(&sim_f);
    sim_f_ok=1; sim_have_w=0; sim_seen_log=0; sim_dump_at=0; sim_big_act=0;
    SimSlot *sl=&sim_f.slot[cur_slot%SIM_SLOTS]; sim_cal=sl->used ? sl->cal_ticks : 0;
    if(level>=SIM_BIGSTART){                           // the character is in the big range: take the true level from the side file
        Lv v; if(level>=LEVEL_CAP && sl->has_lv && lv_cmp_u64(&sl->lv_true,(u64)LEVEL_CAP)>=0){ v=sl->lv_true; sim_big_set(&v); sim_big_bank=sl->lv_xp; }
        else { lv_from_u64(&v,(u64)level); sim_big_set(&v); }
    }
}
static void sim_reset(void){ simfile_init(&sim_f); sim_f_ok=1; sim_have_w=0; sim_cal=0; sim_seen_log=0; sim_dump_at=0; sim_big_act=0; lv_zero(&sim_big_lv); lv_zero(&sim_big_bank); }

// ---------------------------------------------------------------- binding the town of this planet to the objects the game made
static int sim_wlk(s32 x, s32 y){ int c=cmap[((y>>15)&HMASK)*HW+((x>>15)&HMASK)]; return c==74||c==75||c==80||c==81; }   // same ground test as town.inc
static void sim_bind(void){
    if(!sim_f_ok){ simfile_init(&sim_f); sim_f_ok=1; }
    if(sim_have_w) sim_store_slot();                    // keep what the last town learned
    int nl=0, mask=0, homes[SIM_MAXFOLK*2], mk=-1, cn=-1, of=-1, rc=-1; sim_nhome=0;
    for(int i=0;i<NPO;i++){ if(!po[i].alive) continue;
        if(po[i].kind==O_LOCAL && nl<SIM_MAXFOLK) sim_fpo[nl++]=i;
        else if(po[i].kind==O_HOME && sim_nhome<SIM_MAXFOLK*2) homes[sim_nhome++]=i;
        else if(po[i].kind==O_MARKET && mk<0) mk=i; else if(po[i].kind==O_CANTINA && cn<0) cn=i;
        else if(po[i].kind==O_OFFICE && of<0) of=i; else if(po[i].kind==O_RACE && rc<0) rc=i; }
    if(mk>=0) mask|=1<<SIMJ_STALL; if(cn>=0) mask|=1<<SIMJ_COOK; if(of>=0) mask|=1<<SIMJ_CLERK; if(rc>=0) mask|=1<<SIMJ_RACER;
    mask|=(1<<SIMJ_FORAGER)|(1<<SIMJ_MINER)|(1<<SIMJ_GUARD);
    SimSlot *sl=&sim_f.slot[cur_slot%SIM_SLOTS];
    sim_init(&sim_w, sim_world_seed(), nl, (uint32_t)mask, sim_cal);
    if(pc_applied){ u32 cx[5], cy[5]; int dh, tc; for(int i=0;i<5;i++){ cx[i]=clus[i]; cy[i]=clus[UAX+i]; } pcx_env(uni_w,cx,cy,cur_sys,cur_pl,cur_moon,&dh,&tc); sim_set_env(&sim_w,dh,tc); }   // endless-galaxy planets: their own day and climate
    if(sl->used){ sim_w.spirit=sl->spirit; sim_w.god=sl->god; sim_load_tail(&sim_w,sl); sim_load_friends(&sim_w,sl); }
    sim_w.god.sandbox=sim_f.sandbox;
    sim_have_w=1; sim_seen_log=sim_w.log_n; sim_dump_at=sim_w.tick+600; sim_a_up=0;
    s32 px = city_r ? city_cx<<16 : 64<<16, py = city_r ? city_cy<<16 : 64<<16;
    for(int k=0;k<nl;k++){
        PObj *o=&po[sim_fpo[k]]; sim_fx[k]=o->x; sim_fy[k]=o->y; sim_stuck[k]=0;
        int a=(k*53)&255; s32 ox=cosx(a)*8, oy=sinx(a)*8;                          // a spot on a ring round the plaza, one per bug
        sim_tgx[k][SIMP_PLAZA]=px+ox; sim_tgy[k][SIMP_PLAZA]=py+oy;
        int h = sim_nhome ? homes[k%sim_nhome] : -1;
        sim_tgx[k][SIMP_HOME]= h>=0 ? po[h].x : px+ox*2; sim_tgy[k][SIMP_HOME]= h>=0 ? po[h].y+(1<<16) : py+oy*2;
        int wk=-1; switch(sim_w.folk[k].job){ case SIMJ_STALL: wk=mk; break; case SIMJ_COOK: wk=cn; break; case SIMJ_CLERK: wk=of; break; case SIMJ_RACER: wk=rc; break; }
        sim_tgx[k][SIMP_WORK]= wk>=0 ? po[wk].x : px+ox*3/2; sim_tgy[k][SIMP_WORK]= wk>=0 ? po[wk].y+(1<<16) : py+oy*3/2;
        int ea = cn>=0 ? cn : mk;
        sim_tgx[k][SIMP_EATERY]= ea>=0 ? po[ea].x : px+ox; sim_tgy[k][SIMP_EATERY]= ea>=0 ? po[ea].y+(1<<16) : py+oy;
    }
}
static int sim_dist10(int k){                            // how far bug k is from where it should be, in tenths of a cell
    const SimFolk *f=&sim_w.folk[k]; s32 dx=wrapd(sim_tgx[k][f->place]-sim_fx[k]), dy=wrapd(sim_tgy[k][f->place]-sim_fy[k]);
    s32 d=(dx<0?-dx:dx)+(dy<0?-dy:dy); return (int)(d>>12)*10>>4;
}
static void sim_walk(int n){                            // each frame: walk every bug towards where its day says it should be
    for(int k=0;k<sim_w.nfolk && k<SIM_MAXFOLK;k++){
        PObj *o=&po[sim_fpo[k]]; if(!o->alive) continue; const SimFolk *f=&sim_w.folk[k];
        s32 dx=wrapd(sim_tgx[k][f->place]-sim_fx[k]), dy=wrapd(sim_tgy[k][f->place]-sim_fy[k]); s32 adx=dx<0?-dx:dx, ady=dy<0?-dy:dy;
        if(adx+ady>0x18000){                              // not there yet (1.5 cells): one step along the longer way, else along the other
            s32 st=0x500*n; int moved=0;
            for(int pass=0;pass<2 && !moved;pass++){
                int useX = pass==0 ? adx>=ady : adx<ady; s32 d = useX ? dx : dy; if(d==0) continue;
                s32 nx=sim_fx[k]+(useX ? (d>0?st:-st) : 0), ny=sim_fy[k]+(useX ? 0 : (d>0?st:-st));
                if(sim_wlk(nx,ny)){ sim_fx[k]=nx&((TW<<16)-1); sim_fy[k]=ny&((TW<<16)-1); moved=1; } }
            if(moved) sim_stuck[k]=0;
            else if(++sim_stuck[k]>180 && adx+ady>0x30000){ sim_fx[k]=sim_tgx[k][SIMP_PLAZA]; sim_fy[k]=sim_tgy[k][SIMP_PLAZA]; sim_stuck[k]=0; }   // stuck behind a wall: back to the plaza
        }
        o->x=sim_fx[k]; o->y=sim_fy[k];                   // (this overrules the old random walk of life_tick)
    }
}

// ---------------------------------------------------------------- once a frame
static void sim_env_once(void){
    if(sim_env_done) return; sim_env_done=1;
    const char *e=sim_io_env("BUGSTAR_SET_LEVEL");      // a test hook: start with this level (a decimal number, up to 64^256)
    if(e && *e){ Lv v; if(lv_from_dec(&v,e)>=0 && !lv_is_zero(&v)){ sim_big_set(&v); recalc(); hearts=maxh; mp=maxmp; } }
    e=sim_io_env("BUGSTAR_SET_FREED"); if(e && *e) bugs_freed=e[0]-'0';     // a test hook: bugs freed so far (0..9)
    e=sim_io_env("BUGSTAR_SANDBOX"); if(e && e[0]=='1'){ Lv l; sim_level_lv(&l); sim_f.sandbox=1; if(sim_have_w) sim_set_sandbox(&sim_w,1,&l); }
}
static void sim_unlock_msgs(void){
    Lv l; sim_level_lv(&l); sim_check_unlocks(&sim_w,&l);
    int c=0, last=0;
    for(u32 b=sim_w.log_n-sim_seen_log; b>0; b--){ const SimEvent *e=sim_log_at(&sim_w,b-1); if(e && e->kind==SIME_UNLOCK){ c++; last=e->a; } }
    for(u32 b=sim_w.log_n-sim_seen_log; b>0; b--){ const SimEvent *e=sim_log_at(&sim_w,b-1); if(e && e->kind==SIME_LETTER){ char t[60]; sim_event_text(&sim_w,e,t,sizeof t); say(t); sfx(6); sim_said_letters++; break; } }
    sim_seen_log=sim_w.log_n;
    if(c>3){ char b[40]; b[0]=0; ncat(b,c); scat(b," NEW GOD POWERS ARE OPEN"); say(b); sfx(6); }
    else if(c>0){ char b[40]; scpy(b,"NEW POWER "); ncat(b,last); scat(b,": "); scat(b,god_power(last)->name); say(b); sfx(6); }
}
static void sim_dump_all(void){
    const char *path=sim_io_env("BUGSTAR_SIM_DUMP"); if(!path) return;
    static char buf[4096]; sim_dump(&sim_w,buf,sizeof buf); sim_io_append(path,buf);
    char t[96]; for(int k=0;k<sim_w.nfolk;k++){ scpy(t,"near "); ncat(t,k); scat(t," "); ncat(t,sim_dist10(k)); scat(t,"\n"); sim_io_append(path,t); }
    { int nl=0; for(u32 b=0;b<sim_log_count(&sim_w);b++) if(sim_log_at(&sim_w,b)->kind==SIME_LETTER) nl++;
      scpy(t,"friends"); for(int k=0;k<sim_w.nfolk;k++){ scat(t," "); ncat(t,sim_w.friend[k]); } scat(t," letters "); ncat(t,nl); scat(t," shown "); ncat(t,sim_said_letters); scat(t,"\n"); sim_io_append(path,t); }
    { static char cb[200]; col_dump(sim_colony(),cb,sizeof cb); sim_io_append(path,cb); scpy(t,"colcards "); ncat(t,sim_col_cards); scat(t," gifts "); ncat(t,sim_col_gifts); scat(t," gold "); ncat(t,gold); scat(t," food "); ncat(t,food); scat(t,"\n"); sim_io_append(path,t); }
    { Lv l; sim_level_lv(&l); char d[LV_DEC_MAX]; lv_to_dec(&l,d,sizeof d); scpy(t,"level "); scat(t,d); scat(t," powers "); ncat(t,god_unlocked_count(&l,sim_w.god.sandbox)); scat(t,"\n"); sim_io_append(path,t); }
    sim_io_append(path,"--\n");
}
// ---------------------------------------------------------------- the colony (Phase 4): the action game's milestones open its rungs
static char sim_away[6][48]; static int sim_away_n, sim_away_new;   // the WHILE YOU WERE AWAY summary of the last catch-up
static u32 sim_town32(void){ return (u32)sim_mix64(sim_w.seed ^ 0xC0105EEDull); }
static SimColony *sim_colony(void){ return col_find(&sim_f.slot[cur_slot%SIM_SLOTS].cols, sim_town32(), 1, (u32)sim_day(&sim_w)); }
static void sim_colflags(ColFlags *f){
    f->bugs_freed=bugs_freed; f->zone_cleared=zone_cleared; f->best_depth=best_depth; f->bosses=__builtin_popcount((unsigned)cleared_mask);
    f->outposts=outposts_done; f->ore_tool=ore_tool; f->hives=hive_done; f->ant_king=ant_king; f->relics=relic_bits!=0;
    f->friends=0; for(int k=0;k<sim_w.nfolk;k++) if(sim_w.friend[k]>=25) f->friends++;
    f->level = level>=10 ? 10 : (int)level;
}
static void sim_burn_world(void){                       // a warship burnt this planet: the colony is dead (RUINS, so it can be resettled later); story worlds are never ended for good
    if(!sim_is_on()) return;
    u64 ws=sim_world_seed(); if(!sim_have_w || ws!=sim_w.seed) sim_bind();
    SimColony *co=sim_colony(); co->nset=0; co->state=COLS_RUINS; co->raid_eta=0; }
static void sim_colony_tick(void){
    ColFlags fl; sim_colflags(&fl); SimColony *co=sim_colony(); u32 today=(u32)sim_day(&sim_w), yd=col_year_days(SIM_SEASON_DAYS);
    co->world_ok = pc_applied ? 1 : 0;                                  // only endless-galaxy planets can ever be destroyed; story worlds never
    if(!co->danger || co->danger==2) co->danger = (u8)(1 + (sim_town32()>>9)%6);
    u16 nw=col_update(co,(u16)(col_mask(&fl)|col_goal_mask(co,&fl,today,yd)));
    { ColAway aw; int nd=col_catchup_d(co,today,yd,sim_f.difficulty,&aw);
      if(nd>=2 && (co->mask&(1u<<COLR_TENT))){ sim_away_n=col_away_text(&aw,co,sim_away,6); sim_away_new=1;     // read on the COLONY screen; one line now
          char b[40]; scpy(b,"COLONY: "); if(aw.years){ ncat(b,aw.years); scat(b," YEARS PASSED"); } else { ncat(b,aw.days); scat(b," DAYS PASSED"); } if(!nw){ say(b); } }
      for(int i=aw.nev-1;i>=0;i--){ int k=aw.ev[i].kind;                  // warnings in advance, and bad news, as they come
          if(k==COLE_RAIDWARN||k==COLE_FOODWARN||k==COLE_RAIDLOST||k==COLE_RUINS||k==COLE_WORLDEND||k==COLE_SICK){ char t[52]; col_event_text(&aw.ev[i],t,sizeof t); say(t); sfx(4); break; } } }
    if(nw){ int top=0; for(int r=1;r<COLR_N;r++) if(nw&(1u<<r)) top=r; say(col_rung_card(top)); sfx(6); sim_col_cards++; }
}
static int sim_open_gods(void){ if(!sim_is_on()) return 0; Lv l; sim_level_lv(&l); return god_unlocked_count(&l,sim_f.sandbox); }
static void sim_frame(int n){
    pc_land_note();
    if(!sim_is_on()) return;
    u64 ws=sim_world_seed(); if(!sim_have_w || ws!=sim_w.seed) sim_bind();
    sim_env_once();
    if(!(keys&K_A)) sim_a_up=1;
    int steps=sim_feed(&sim_w,n,20); sim_cal+=(u64)steps;
    sim_walk(n);
    { static int nk=-2; if(nk==-2){ const char *e=sim_io_env("BUGSTAR_SIM_NEAR"); nk = e && *e ? e[0]-'0' : -1; }   // a test hook: keep the player beside townsperson k
      if(nk>=0 && nk<sim_w.nfolk){ ppx=sim_fx[nk]+0xC000; ppy=sim_fy[nk]; } }
    if(frames%30==0){ sim_unlock_msgs(); sim_colony_tick(); }
    if(sim_dump_at && sim_w.tick>=sim_dump_at){ sim_dump_at=sim_w.tick+600; sim_dump_all(); }
}

#include "sim_ui.inc"
