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
static Mine *sim_mine(void);

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
static int sim_gifts_got, sim_helped_n;   // friend gifts received, requests helped (the tests read them)
static int sim_dump_gap=600;                           // ticks between test dumps (BUGSTAR_SIM_DUMP_EVERY)
static int sim_h_mine, sim_h_age, sim_h_done;          // test hooks: BUGSTAR_SIM_MINE=1 (open the mine rungs and give the colony a mine), BUGSTAR_SIM_AGE=days (the colony catches up that many days)
static int sim_a_up;                    // 1 once A has been seen up on this planet: keeps a key still held from the title or a menu from opening the quick menu

static int sim_is_on(void){
    if(sim_env_side<0){ const char *e=sim_io_env("BUGSTAR_SIDE"); sim_env_side = !(e && (e[0]=='0' || e[0]=='o' || e[0]=='O' || e[0]=='n' || e[0]=='N')); }
    return sim_env_side && xo_v(XO_SIDE);
}

// ---------------------------------------------------------------- patch 42: STANDING (standing.h): the menus open with fame; a CHAMPION in a town becomes its mayor; the mayor can raise an armada
static Standing *sim_st(void){ return &sim_f.slot[cur_slot%SIM_SLOTS].stand; }
static u64 sim_unit(void){ return 20u+(u64)(level>100 ? 100 : level<0 ? 0 : level); }          // the game's gold unit (the same one the friends' gifts use)
static u64 sim_lead_level(void){ return level<1 ? 1u : (u64)level; }
static u32 sim_town32(void);

// ---------------------------------------------------------------- patch 47: PROGRESSIVE DISCLOSURE (unlock.h): menus, rows and HUD pieces show only what the player has found
static Unlock *sim_ul(void){ return &sim_f.slot[cur_slot%SIM_SLOTS].unlock; }
static int sim_ls_legacy;
static int sim_ls_h_show=-1, sim_ls_h_tips=-1, sim_ls_h_pop=-1, sim_ls_h_pause=-1, sim_ls_h_big=-1, sim_ls_h_early=1;       // test hooks (BUGSTAR_LS_*), read once
static int sim_ls_digit(const char *e, int dflt){ return e && e[0]>='0' && e[0]<='9' ? e[0]-'0' : dflt; }
static void sim_ls_hooks(void){
    static int done; if(done) return; done=1;
    const char *e=sim_io_env("BUGSTAR_LS_ALL"); if(e && e[0]=='1'){ sim_ls_h_show=ULSA_ALL; sim_ls_h_tips=0; sim_ls_legacy=1; }       // the checks from before patch 47: everything inside open, no tip cards, the side menu lists what fame opened (as it did)
    sim_ls_h_show=sim_ls_digit(sim_io_env("BUGSTAR_LS_SHOW"),sim_ls_h_show); sim_ls_h_tips=sim_ls_digit(sim_io_env("BUGSTAR_LS_TIPS"),sim_ls_h_tips);
    sim_ls_h_pop=sim_ls_digit(sim_io_env("BUGSTAR_LS_POP"),-1); sim_ls_h_pause=sim_ls_digit(sim_io_env("BUGSTAR_LS_PAUSE"),-1); sim_ls_h_big=sim_ls_digit(sim_io_env("BUGSTAR_LS_BIG"),-1);
    sim_ls_h_early=sim_ls_digit(sim_io_env("BUGSTAR_LS_EARLY"),1);
}
static void sim_ls_opts(UlOpts *o){
    sim_ls_hooks();
    o->show_all=(uint8_t)xo_i(XO_LSALL); o->tips=(uint8_t)xo_i(XO_LSTIPS); o->level=(uint8_t)xo_i(XO_LSPOP); o->autopause=(uint8_t)xo_i(XO_LSPAUSE); o->big=(uint8_t)xo_i(XO_LSBIG);
    if(sim_ls_h_show>=0 && sim_ls_h_show<=2) o->show_all=(uint8_t)sim_ls_h_show;
    if(sim_ls_h_tips>=0) o->tips=(uint8_t)(sim_ls_h_tips!=0); if(sim_ls_h_pop>=0 && sim_ls_h_pop<=2) o->level=(uint8_t)sim_ls_h_pop;
    if(sim_ls_h_pause>=0) o->autopause=(uint8_t)(sim_ls_h_pause!=0); if(sim_ls_h_big>=0) o->big=(uint8_t)(sim_ls_h_big!=0);
}
static int sim_ls_vis(int ft){ UlOpts o; sim_ls_opts(&o); return sim_f_ok ? ul_vis(sim_ul(),ft,o.show_all) : (o.show_all>=ULSA_ALL ? ULV_SHOWN : ULV_HIDDEN); }
static int sim_ls_on(int ft){ return sim_ls_vis(ft)==ULV_SHOWN; }                   // is this part of the life sim drawn?
static int sim_qm_n, sim_qm_id[SQ_N], sim_qm_sil[SQ_N]; static void sim_qm_build(int all); static const char *sim_qm_name(int sq);      // the side menu's rows (sim_ui.inc)
static s64 sim_gold_prev=-1;
static int sim_ls_cards, sim_ls_toasts, sim_ls_summaries, sim_tip_wait;             // tip cards, one-line tips and crowd summaries shown (the tests read them); frames to wait before the next card
static void sim_ls_facts(UlFacts *f){
    memset(f,0,sizeof *f); const Standing *st=sim_st();
    f->fame=fame; f->level=level; f->town=sim_have_w && sim_w.nfolk>0; f->log_n=(int)sim_log_count(&sim_w);
    for(int k=0;k<sim_w.nfolk;k++) if(sim_w.friend[k]>f->friend_max) f->friend_max=sim_w.friend[k];
    if(!ul_has(sim_ul(),UF_LETTER)) for(u32 b=0;b<sim_log_count(&sim_w);b++) if(sim_log_at(&sim_w,b)->kind==SIME_LETTER){ f->letters=1; break; }
    f->day=sim_day(&sim_w); f->season_on=sim_w.env_on; f->freed=bugs_freed; f->mayor=stand_is_mayor(st); f->worlds_total=st->worlds_total; f->galaxies=st->galaxies;
    if(f->town){ SimColony *co=sim_colony(); f->homes=(co->mask>>COLR_HOUSES)&1; f->shops=(co->mask>>COLR_SHOPS)&1; f->mine=(co->mask>>COLR_MINE)&1; f->raid=co->raid_eta>0; }
}
static uint32_t sim_ls_update(void){                                               // what the game knows now -> the flags. No tips are shown here.
    if(!sim_have_w) return 0;
    Unlock *u=sim_ul(); UlFacts f;
    if(sim_gold_prev>=0 && gold>sim_gold_prev) ul_note(u,ULC_GOLD); sim_gold_prev=gold;       // the first gold you earn
    if(sim_w.weather!=SIMW_CLEAR) ul_note(u,ULC_WEATHER);
    for(int k=0;k<sim_w.nfolk;k++) if(sim_w.folk[k].warned) { ul_note(u,ULC_NEEDY); break; }  // somebody got hungry or tired
    sim_ls_facts(&f); return ul_update(u,&f, sim_ls_h_early && sim_cal<UL_EARLY_TICKS);
}
// a tip as a card: normal size uses the game's own card; BIG TEXT draws 8-pixel letters, 26 to a line
static void sim_tip_card(int ft, int big){
    const char *ln[4]; int n=ul_tip_lines(ft,ln); char t[24]; scpy(t,"NEW: "); scat(t,ul_defs[ft].name);
    oam_hide_all(); oam_flush();
    if(!big){ card(VISPG,t,ln,n); return; }
    int sh=shd_begin(); volatile u16 *pg = sh ? VISPG : VISPG; int h=26+n*14+16, y0=80-h/2; if(y0<2) y0=2;
    ui_panel(pg,6,y0,228,h); rect(pg,8,y0+2,224,15,U_RAISE); rect(pg,8,y0+2,224,1,U_SHINE); rect(pg,8,y0+17,224,1,U_EDGE);
    { int tw=textlen(t)*8; int x=120-tw/2; x&=~1; text(pg,x+1,y0+3,t,0,2); text(pg,x,y0+2,t,C_GOLD,2); }
    for(int i=0;i<n;i++){ int w=textlen(ln[i])*8, x=(120-w/2)&~1; text(pg,x,y0+22+i*14,ln[i],C_WHITE,2); }
    wait_release();
    for(int tt=0;;tt++){ vsync(); read_keys(); if(HIT(K_A)||HIT(K_START)||HIT(K_B)) break;
        if((tt&31)==0){ rect(pg,92,y0+h-14,56,11,U_PANEL); if(tt&32) ui_hints(pg,100,y0+h-12,"A: OK",C_WHITE,1); else ui_hints(pg,100,y0+h-12,"A: OK",U_SOFT,1); } }
    sfx(2); wait_release(); shd_end(sh);
}
static void sim_ls_deliver(void){                                                  // one tip at a time, never in a menu, never a pile
    Unlock *u=sim_ul(); UlOpts o; sim_ls_opts(&o);
    if(!ul_pending(u)) return;
    if(!o.tips || o.show_all>=ULSA_ALL){ ul_told_all(u); return; }                  // TIPS off (or everything shown): nothing to tell; the ! marks stay
    if(sim_tip_wait>0){ sim_tip_wait-=30; return; }
    if((in_menu&0xFFFF) || FILM || kr_mode) return;
    if(ul_flood(u)){                              // a crowd (an old save, a cheat): one card, the ! marks show the rest
        char l0[44]; l0[0]=0; ncat(l0,ul_popcount(ul_pending(u))); scat(l0," NEW THINGS ARE OPEN.");
        const char *ln[3]={l0,"LOOK FOR THE ! MARKS.","READ MORE IN TIPS."};
        ul_told_all(u); sim_ls_summaries++; sim_a_up=0;
        if(o.autopause){ oam_hide_all(); oam_flush(); card(VISPG,"NEW: LOTS",ln,3); p_last=vbl_count; sim_tip_wait=1800; } else { say("MANY NEW THINGS. SEE TIPS"); sfx(6); sim_tip_wait=300; }
        return; }
    int ft=ul_next_tip(u), st=ul_tip_style(&o,ft);
    ul_told(u,ft);
    if(st==ULT_CARD){ sim_ls_cards++; sim_a_up=0; sim_tip_card(ft,o.big); p_last=vbl_count; if(ul_feat_sq(ft)<0) ul_open(u,ft); sim_tip_wait=1200; }     // (a card you read is not new any more, unless it has its own menu row)
    else if(st==ULT_TOAST){ char b[44]; scpy(b,"NEW: "); scat(b,ul_defs[ft].name); scat(b,". SEE TIPS"); say(b); sfx(6); sim_ls_toasts++; sim_tip_wait=150; }
}
static void sim_ls_tick(void){ sim_ls_update(); sim_ls_deliver(); }
static void sim_ls_hud_dump(char *t, size_t n){ char a[300],b[300],c[300]; Unlock *u=sim_ul(); ul_names(u->have,a,sizeof a); ul_names(ul_pending(u),b,sizeof b); ul_names(ul_fresh(u),c,sizeof c);
    scpy(t,"unlock have "); ncat(t,u->have); scat(t," told "); ncat(t,u->told); scat(t," opened "); ncat(t,u->opened); scat(t," cards "); ncat(t,sim_ls_cards); scat(t," toasts "); ncat(t,sim_ls_toasts); scat(t," crowds "); ncat(t,sim_ls_summaries);
    scat(t," names "); scat(t,a[0]?a:"-"); scat(t," fresh "); scat(t,c[0]?c:"-"); (void)b; (void)n; }

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
    sim_f_ok=1; sim_have_w=0; sim_seen_log=0; sim_dump_at=0; sim_big_act=0; sim_gold_prev=-1; sim_tip_wait=0;
    SimSlot *sl=&sim_f.slot[cur_slot%SIM_SLOTS]; sim_cal=sl->used ? sl->cal_ticks : 0;
    if(level>=SIM_BIGSTART){                           // the character is in the big range: take the true level from the side file
        Lv v; if(level>=LEVEL_CAP && sl->has_lv && lv_cmp_u64(&sl->lv_true,(u64)LEVEL_CAP)>=0){ v=sl->lv_true; sim_big_set(&v); sim_big_bank=sl->lv_xp; }
        else { lv_from_u64(&v,(u64)level); sim_big_set(&v); }
    }
}
static void sim_file_ensure(void){                       // the .sim file in memory (the title screen has not loaded it yet)
    if(sim_f_ok) return; static u8 buf[SIMFILE_MAX]; size_t n=sim_io_load(buf,sizeof buf);
    if(!n || simfile_read(&sim_f,buf,n)<0) simfile_init(&sim_f);
    sim_f_ok=1;
}
static void sim_file_flush(void){ if(!pc_sav_path()) return; static u8 buf[SIMFILE_MAX]; size_t n=simfile_write(&sim_f,buf,sizeof buf); if(n) sim_io_store(buf,n); }
static void sim_reset(void){                             // a new game in this slot: its side game starts empty, the other two slots keep theirs (this used to wipe all three)
    sim_f_ok=0; sim_file_ensure(); simfile_clear_slot(&sim_f,cur_slot%SIM_SLOTS);
    sim_have_w=0; sim_cal=0; sim_seen_log=0; sim_dump_at=0; sim_big_act=0; lv_zero(&sim_big_lv); lv_zero(&sim_big_bank); sim_gold_prev=-1; sim_tip_wait=0; }
static void sim_slot_erase(int s){ if(s<0 || s>=SIM_SLOTS) return; sim_file_ensure(); simfile_clear_slot(&sim_f,s); sim_file_flush(); }      // the slot picker erased a save: its side game goes too
static void sim_slot_copy(int from, int to){ if(from<0 || from>=SIM_SLOTS || to<0 || to>=SIM_SLOTS) return; sim_file_ensure(); simfile_copy_slot(&sim_f,from,to); sim_file_flush(); }

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
    sim_have_w=1; sim_seen_log=sim_w.log_n; sim_dump_at=sim_w.tick+sim_dump_gap; sim_a_up=0;
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
    e=sim_io_env("BUGSTAR_SET_FAME"); if(e && *e){ int v=0; for(int i=0;e[i]>='0' && e[i]<='9' && i<9;i++) v=v*10+(e[i]-'0'); fame=v; }     // patch 42, a test hook: start with this fame
    e=sim_io_env("BUGSTAR_SIM_DUMP_EVERY"); if(e && *e){ int v=0; for(int i=0;e[i]>='0' && e[i]<='9' && i<6;i++) v=v*10+(e[i]-'0'); if(v>=10){ sim_dump_gap=v; if(sim_have_w) sim_dump_at=sim_w.tick+v; } }
    e=sim_io_env("BUGSTAR_SIM_MINE"); sim_h_mine = e && e[0]=='1';
    e=sim_io_env("BUGSTAR_SIM_AGE"); if(e && *e){ int v=0; for(int i=0;e[i]>='0' && e[i]<='9' && i<6;i++) v=v*10+(e[i]-'0'); sim_h_age=v; }
    e=sim_io_env("BUGSTAR_SET_FREED"); if(e && *e) bugs_freed=e[0]-'0';     // a test hook: bugs freed so far (0..9)
    e=sim_io_env("BUGSTAR_SANDBOX"); if(e && e[0]=='1'){ Lv l; sim_level_lv(&l); sim_f.sandbox=1; if(sim_have_w) sim_set_sandbox(&sim_w,1,&l); }
}
static void sim_unlock_msgs(void){
    Lv l; sim_level_lv(&l); sim_check_unlocks(&sim_w,&l);
    int c=0, last=0;
    for(u32 b=sim_w.log_n-sim_seen_log; b>0; b--){ const SimEvent *e=sim_log_at(&sim_w,b-1); if(e && e->kind==SIME_UNLOCK){ c++; last=e->a; } }
    for(u32 b=sim_w.log_n-sim_seen_log; b>0; b--){ const SimEvent *e=sim_log_at(&sim_w,b-1); if(e && e->kind==SIME_LETTER){ char t[60]; sim_event_text(&sim_w,e,t,sizeof t); say(t); sfx(6); sim_said_letters++; break; } }
    for(u32 b=sim_w.log_n-sim_seen_log; b>0; b--){ const SimEvent *e=sim_log_at(&sim_w,b-1);                // patch 15: a friend's gift reaches your bag
        if(e && e->kind==SIME_GIFT){ int a=e->b;
            if(e->a==SIMG_FOOD) eat(a); else if(e->a==SIMG_GOLD) gold=sadd(gold,(s64)a*(20+(level>100?100:(s64)level))); else if(e->a==SIMG_HEAL){ hearts=maxh; mp=maxmp; } else if(e->a==SIMG_ORE) ore_add(0,a);
            char t[60]; sim_event_text(&sim_w,e,t,sizeof t); say(t); sfx(6); sim_gifts_got++; break; } }
    sim_seen_log=sim_w.log_n;
    if(!sim_ls_on(UF_GODS)) c=0;                                               // patch 47: GOD POWERS is not found yet: no news of them (the journal still keeps the line)
    if(c>3){ char b[40]; b[0]=0; ncat(b,c); scat(b," NEW GOD POWERS ARE OPEN"); say(b); sfx(6); }
    else if(c>0){ char b[40]; scpy(b,"NEW POWER "); ncat(b,last); scat(b,": "); scat(b,god_power(last)->name); say(b); sfx(6); }
}
static void sim_dump_all(void){
    const char *path=sim_io_env("BUGSTAR_SIM_DUMP"); if(!path) return;
    static char buf[4096]; sim_dump(&sim_w,buf,sizeof buf); sim_io_append(path,buf);
    char t[96]; for(int k=0;k<sim_w.nfolk;k++){ scpy(t,"near "); ncat(t,k); scat(t," "); ncat(t,sim_dist10(k)); scat(t,"\n"); sim_io_append(path,t); }
    { int nl=0; for(u32 b=0;b<sim_log_count(&sim_w);b++) if(sim_log_at(&sim_w,b)->kind==SIME_LETTER) nl++;
      scpy(t,"friends"); for(int k=0;k<sim_w.nfolk;k++){ scat(t," "); ncat(t,sim_w.friend[k]); } scat(t," letters "); ncat(t,nl); scat(t," shown "); ncat(t,sim_said_letters); scat(t,"\n"); sim_io_append(path,t); }
    { static char mb[300]; mn_dump(sim_mine(),mb,sizeof mb); sim_io_append(path,mb); }
    { static char cb[200]; col_dump(sim_colony(),cb,sizeof cb); sim_io_append(path,cb); scpy(t,"colcards "); ncat(t,sim_col_cards); scat(t," gifts "); ncat(t,sim_col_gifts); scat(t," fgifts "); ncat(t,sim_gifts_got); scat(t," helped "); ncat(t,sim_helped_n); { SimTies ti; sim_ties(&sim_w,&ti); scat(t," ties "); ncat(t,ti.guard); scat(t,"/"); ncat(t,ti.food); scat(t,"/"); ncat(t,ti.goods); scat(t," worry "); ncat(t,sim_w.worry); scat(t," aid "); ncat(t,sim_w.aid); } scat(t," gold "); ncat(t,gold); scat(t," food "); ncat(t,food); scat(t,"\n"); sim_io_append(path,t); }
    { Lv l; sim_level_lv(&l); char d[LV_DEC_MAX]; lv_to_dec(&l,d,sizeof d); scpy(t,"level "); scat(t,d); scat(t," powers "); ncat(t,god_unlocked_count(&l,sim_w.god.sandbox)); scat(t,"\n"); sim_io_append(path,t); }
    { const Standing *st=sim_st(); scpy(t,"stand fame "); ncat(t,fame); scat(t," open "); ncat(t,stand_open_mask(st,fame)); scat(t," mayor "); ncat(t,stand_is_mayor(st)); scat(t," ships "); ncat(t,(s64)st->ships); scat(t," worlds "); ncat(t,(s64)st->worlds_total); scat(t," galaxies "); ncat(t,st->galaxies); scat(t,"\n"); sim_io_append(path,t); }
    { sim_ls_hud_dump(t,sizeof t); scat(t," qm "); sim_qm_build(0); ncat(t,sim_qm_n); scat(t," rows "); for(int i=0;i<sim_qm_n;i++){ if(i) scat(t,","); scat(t,sim_qm_sil[i]?"???":sim_qm_name(sim_qm_id[i])); } scat(t,"\n"); sim_io_append(path,t); }
    sim_io_append(path,"--\n");
}
// ---------------------------------------------------------------- the colony (Phase 4): the action game's milestones open its rungs
static char sim_away[6][48]; static int sim_away_n, sim_away_new;   // the WHILE YOU WERE AWAY summary of the last catch-up
static u32 sim_town32(void){ return (u32)sim_mix64(sim_w.seed ^ 0xC0105EEDull); }
static SimColony *sim_colony(void){ return col_find(&sim_f.slot[cur_slot%SIM_SLOTS].cols, sim_town32(), 1, (u32)sim_day(&sim_w)); }
static Mine *sim_mine(void){ SimColony *co=sim_colony(); return mn_find(&sim_f.slot[cur_slot%SIM_SLOTS].mines, sim_town32(), 1, co->last_day); }   // Phase 6: this colony's workshops and mine (mine.h)
static void sim_colflags(ColFlags *f){
    f->bugs_freed=bugs_freed; f->zone_cleared=zone_cleared; f->best_depth=best_depth; f->bosses=__builtin_popcount((unsigned)cleared_mask);
    f->outposts=outposts_done; f->ore_tool=ore_tool; f->hives=hive_done; f->ant_king=ant_king; f->relics=relic_bits!=0;
    f->friends=0; for(int k=0;k<sim_w.nfolk;k++) if(sim_w.friend[k]>=25) f->friends++;
    f->level = level>=10 ? 10 : (int)level;
}
static int sim_invasion(unsigned size, unsigned long long *reward){          // warship DEFEND missions: a real raid on this planet's colony now. 1 won, -1 lost, 0 none
    if(!sim_is_on()) return 0;
    if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind();
    SimColony *co=sim_colony(); ColAway aw; memset(&aw,0,sizeof aw); uint64_t r=0; sim_f.difficulty=(u8)xo_i(XO_SIDEDIF);       // (patch 45: aw was never cleared: the first DEFEND job with a colony crashed in ev())
    if(sim_f.difficulty==COLD_NORMAL){ unsigned cap=(unsigned)(col_defence(co)*6/5+2); if(size>cap) size=cap; }   // patch 17: on NORMAL a DEFEND raid is never far beyond what the colony can hold (CALM always holds, HARSH keeps the full size)
    int res=col_invade(co,size,sim_f.difficulty,(u32)sim_day(&sim_w),&aw,&r); if(reward) *reward=r;
    return res;
}
static int sim_planet_burnt(void){ if(!sim_is_on()) return 0; if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind(); return col_burnt(sim_colony()); }
static void sim_burn_world(void){                       // a warship burnt this planet: the colony is dead (RUINS, so it can be resettled later); story worlds are never ended for good
    if(!sim_is_on()) return;
    u64 ws=sim_world_seed(); if(!sim_have_w || ws!=sim_w.seed) sim_bind();
    SimColony *co=sim_colony(); co->world_ok = pc_applied ? 1 : 0; col_burn(co); }              // an endless-galaxy world is burnt for good (DEAD_WORLD), a story world is only ruined (it heals; settle again)
static int sim_mine_news, sim_mine_said;             // mine news said aloud (the tests read it)
static void sim_colony_tick(void){
    sim_f.difficulty=(u8)xo_i(XO_SIDEDIF);                              // the main Options setting is the one that counts
    ColFlags fl; sim_colflags(&fl); SimColony *co=sim_colony(); u32 today=(u32)sim_day(&sim_w)+(u32)sim_h_age, yd=col_year_days(SIM_SEASON_DAYS);
    co->world_ok = pc_applied ? 1 : 0;                                  // only endless-galaxy planets can ever be destroyed; story worlds never
    if(!co->danger || co->danger==2) co->danger = (u8)(1 + (sim_town32()>>9)%6);
    { SimTies ti; sim_ties(&sim_w,&ti); col_set_ties(co,ti.guard*6,ti.food*2,ti.wood*2,ti.ore,ti.goods,ti.mood);      // patch 15: friends lend their trade to the colony...
      int wy, ad; col_outside(co,&wy,&ad); sim_set_outside(&sim_w,wy,ad); }                                         // ...and the colony's trouble or plenty reaches the town
    u16 hk = sim_h_mine ? (u16)((1u<<COLR_TENT)|(1u<<COLR_FIELDS)|(1u<<COLR_HOUSES)|(1u<<COLR_SHOPS)|(1u<<COLR_MINE)|(1u<<COLR_WALLS)|(1u<<COLR_MARKET)) : 0;     // test hook BUGSTAR_SIM_MINE=1: rungs, a mine, miners and wood
    u16 nw=col_update(co,(u16)(col_mask(&fl)|col_goal_mask(co,&fl,today,yd)|hk));
    if(sim_h_mine && !sim_h_done && (co->mask&(1u<<COLR_TENT)) && co->nset){ sim_h_done=1; co->bld[COLB_MINE]=1; co->grid[5]=COLB_MINE+1; co->wood=400; co->food=500; for(int i=0;i<co->nset && i<6;i++) co->job[i]= (u8)(i<4 ? COLJ_MINER : i==4 ? COLJ_BUILDER : COLJ_COOK); }
    { ColAway aw; MnAway maw; Mine *mi=sim_mine(); int nd=mn_catchup(co,mi,today,yd,sim_f.difficulty,&aw,&maw);       // Phase 6: the colony's days and its mine's days together
      if(nd>=2 && (co->mask&(1u<<COLR_TENT))){ sim_away_n=col_away_text(&aw,co,sim_away,6); sim_away_n+=mn_away_text(&maw,mi,sim_away+sim_away_n,6-sim_away_n); sim_away_new=1;     // read on the COLONY screen; one line now
          char b[40]; scpy(b,"COLONY: "); if(aw.years){ ncat(b,aw.years); scat(b," YEARS PASSED"); } else { ncat(b,aw.days); scat(b," DAYS PASSED"); } if(!nw){ say(b); } }
      for(int i=aw.nev-1;i>=0;i--){ int k=aw.ev[i].kind;                  // warnings in advance, and bad news, as they come
          if(k==COLE_RAIDWARN||k==COLE_FOODWARN||k==COLE_RAIDLOST||k==COLE_RUINS||k==COLE_WORLDEND||k==COLE_SICK){ char t[52]; col_event_text(&aw.ev[i],t,sizeof t); say(t); sfx(4); sim_mine_news++; break; } }
      if(!sim_mine_news) for(int i=maw.nev-1;i>=0;i--){ int k=maw.ev[i].kind;                              // the mine's news: a creak is a day's warning, a cave-in is bad news
          if(k==MNE_CREAK||k==MNE_CAVE||k==MNE_DIED||k==MNE_GOAL){ char t[52]; mn_event_text(&maw.ev[i],t,sizeof t); say(t); sfx(4); sim_mine_said++; break; } }
      sim_mine_news=0; }
    if(nw){ int top=0; for(int r=1;r<COLR_N;r++) if(nw&(1u<<r)) top=r; say(col_rung_card(top)); sfx(6); sim_col_cards++; }
}
static void sim_stand_tick(void){
    Standing *st=sim_st(); if(!st->used){ st->used=1; st->told=(uint16_t)stand_open_mask(st,fame); }      // an old save: the menus you already earned are yours, with no fanfare
    if(stand_can_appoint(st,fame,sim_w.nfolk)){ stand_appoint(st,sim_town32(),world_name(),sim_cal); sim_log(&sim_w,SIME_STAND,0,2,0); }
    uint16_t m=stand_open_mask(st,fame), nw=(uint16_t)(m&~st->told);              // patch 47: the TIP cards (unlock.h) announce menus now; here only the journal line is kept
    for(int i=0;i<STM_N;i++) if(nw>>i&1){ if(i!=STM_HALL) sim_log(&sim_w,SIME_STAND,0,1,i); }
    st->told|=nw;
    if(stand_is_mayor(st) && sim_town32()==st->mayor_town && sim_w.nfolk>0){        // the home town pays: its bugs, more when they are happy
        int sum=0; for(int k=0;k<sim_w.nfolk;k++) sum+=sim_mood(&sim_w.folk[k]);
        st->tax_rate=stand_rate(sim_w.nfolk,sum/sim_w.nfolk,sim_unit()); }
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
    if(frames%30==0){ sim_unlock_msgs(); sim_colony_tick(); sim_stand_tick(); sim_ls_tick(); }
    if(sim_dump_at && sim_w.tick>=sim_dump_at){ sim_dump_at=sim_w.tick+sim_dump_gap; sim_dump_all(); }
}

// ---------------------------------------------------------------- patch 15 CALLS: the colony's trouble sets the goal of a dungeon floor
static int sim_dn_cur, sim_dn_paid;                    // the call this floor carries (COLC_*); calls paid so far (the tests read it)
static int sim_dn_fe(int call){ return call==COLC_RAID ? FE_ELITE : call==COLC_FAMINE ? FE_NEST : call==COLC_SUPPLY ? FE_SPEED : call==COLC_LOST ? FE_QUIET : 0; }
static int sim_dn_call(void){
    if(!sim_is_on() || forge_on || base_mode) return 0;
    if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind();
    return col_call(sim_colony(),(u32)sim_day(&sim_w));
}
static void sim_dn_reset(void){ sim_dn_cur=0; }
static void sim_dn_pick(void){ int c=sim_dn_call(); sim_dn_cur=c; if(c) fl_ev=sim_dn_fe(c); }
static void sim_dn_mark(void){ if(sim_dn_cur!=COLC_LOST) return; for(int i=ndo-1;i>=0;i--) if(dob[i].kind==K_CAGE && dob[i].alive){ dob[i].champ=3; break; } }   // the last cage made is the settler's
static void sim_dn_done(int fe){
    if(!sim_dn_cur || fe!=sim_dn_fe(sim_dn_cur)) return;
    char b[48]; if(!col_call_done(sim_colony(),sim_dn_cur,depth_no,(u32)sim_day(&sim_w),b,sizeof b)) return;
    sim_dn_cur=0; sim_dn_paid++; say(b); sfx(6); msg_timer=70;
}
static void sim_dn_leave(void){
    if(!sim_dn_cur) return;
    if(fl_ev==FE_SPEED && !fl_done && (s32)(fl_end-vbl_count)>0) sim_dn_done(FE_SPEED);
    else if(fl_ev==FE_NEST && !fl_done){ int live=0; for(int i=0;i<ndo;i++) if(dob[i].alive && is_mon(dob[i].kind)) live++; if(!live){ fl_done=1; sim_dn_done(FE_NEST); } }
}
static void sim_dn_draw(volatile u16 *pg){
    if(!sim_dn_cur) return; char b[48]; scpy(b,"COLONY: "); { char t[32]; col_call_text(sim_dn_cur,t,sizeof t); scat(b,t); } text(pg,4,38,b,C_GOLD,1);
}

#include "sim_ui.inc"
