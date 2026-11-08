// pc_missions.inc: WARSHIP MISSIONS and random WARSHIP ENCOUNTERS in the PC build (included at the end of pc_warship.inc).
// The rules are in pc/warship/ms.c (plain C, tested). This file is the glue: the mission board (space menu, station, town, outpost),
// the journal, map marks, saving (in the .ws file), pay through the cost breakdown, and the encounters (planet and dungeon).
#include "warship/ms.h"
#define PCM_MAXACT 6
static WsMission pcm_act[PCM_MAXACT]; static int pcm_nact=0; static u32 pcm_done[64]; static int pcm_ndone=0; static int pcm_count=0;
static int pcm_escort=0; static double pcm_dw=0;
static int pcm_enc_mode=1;                                     // WARSHIP ENCOUNTERS: 0 OFF, 1 RARE (default), 2 NORMAL, 3 COMMON
static const char *const pcm_enc_nm[4]={"OFF","RARE","NORMAL","COMMON"};
// BALANCE (patch 17; the measuring program is pc/warship/bal.c, the numbers are in pc/warship/BALANCE.md; keep both in step)
#define PCM_QUIET_MIN 25        // after a severe threat or a boarding party, no new warship rolls for this many game minutes
#define PCM_BUSTER_MIN 5        // a planet-buster, strike or troop window is never shorter than this many minutes
#define PCM_DEFEND_K 4          // a DEFEND mission's raid on the colony is 6 + this x class + bonus/5
static const int PCM_P_CAP[4]={0,20,45,70};   // the most a minute can roll (thousandths), however high the danger or level
static int pcm_quiet_until=0;
typedef struct { int on; u32 seed; int z; u32 sys, gal; int pl, type, cd_end, expire, strikes, next; } PcmEnc;
static PcmEnc pcm_en; static char pcm_q[3][48]; static int pcm_qn=0;
static void pcm_aft_reset(void);
static void pcm_reset(void){ pcm_aft_reset(); pcm_quiet_until=0; pcm_enc_mode=1; pcm_nact=0; pcm_ndone=0; pcm_count=0; pcm_note[0]=0; pcm_escort=0; memset(&pcm_en,0,sizeof pcm_en); pcm_qn=0; }
// ---------------------------------------------------------------- save (inside the .ws file; an old file simply has no such lines)
static void pcm_burnt_save(FILE *f); static void pcm_burnt_add(u32 k);
static void pcm_save(FILE *f){ char b[300]; fprintf(f,"menc=%d\nmcount=%d\n",pcm_enc_mode,pcm_count);
    for(int i=0;i<pcm_nact;i++){ ms_pack(&pcm_act[i],b,sizeof b); fprintf(f,"mis=%s\n",b); }
    for(int i=0;i<pcm_ndone;i++) fprintf(f,"mdone=%u\n",pcm_done[i]);
    pcm_burnt_save(f); }
static int pcm_load_line(const char *ln){ unsigned u; int v;
    if(!strncmp(ln,"mis=",4)){ if(pcm_nact<PCM_MAXACT && ms_unpack(ln+4,&pcm_act[pcm_nact])) { pcm_act[pcm_nact].state=1; pcm_nact++; } return 1; }
    if(sscanf(ln,"mdone=%u",&u)==1){ if(pcm_ndone<64) pcm_done[pcm_ndone++]=u; return 1; }
    if(sscanf(ln,"burnt=%u",&u)==1){ pcm_burnt_add(u); return 1; }
    if(sscanf(ln,"mcount=%d",&v)==1){ pcm_count=v; return 1; }
    if(sscanf(ln,"menc=%d",&v)==1){ pcm_enc_mode=v<0||v>3?1:v; return 1; } return 0; }
// ---------------------------------------------------------------- places
static int pcm_zplayer(void){ return pcw_zcap()*80/100+sys_dlv(cur_sys)*10; }
static void pcm_place(MsPlace *pl, int kind){ memset(pl,0,sizeof *pl); pl->kind=kind; pl->gal=gal_hash();
    pl->seed=hmix(sys_id(cur_sys)*0x9E3779B1u ^ pl->gal ^ (u32)(kind==MP_TOWN?cur_pl*131+cur_moon+1:0)*0x632BE5ABu ^ (u32)(kind+1)*0x2545F491u ^ 0xB0A8Du);
    pl->danger=sys_dlv(cur_sys)+(int)(level>400?100:level/4); pl->zplayer=pcm_zplayer(); pl->theme=sys[cur_sys].eco*3+(cur_pl%3);
    pl->law=plw_edited_count()>0; pl->cheat_world=plw_cheated(); pl->epoch=pcm_count/4;
    for(int i=0;i<NSYS;i++) if(i!=cur_sys) pl->pool[pl->npool++]=sys_id(i); }
static int pcm_find_sys(u32 id){ for(int i=0;i<NSYS;i++) if(sys_id(i)==id) return i; return -1; }
static int pcm_is_done(u32 id){ for(int i=0;i<pcm_ndone;i++) if(pcm_done[i]==id) return 1; return 0; }
static int pcm_is_act(u32 id){ for(int i=0;i<pcm_nact;i++) if(pcm_act[i].id==id) return 1; return 0; }
static void pcm_gal_name(u32 h, char *b){ if(!h){ scpy(b,"THE BUG WAY"); return; } u32 ws=wseed; name_of(h^gsk(108),b); wseed=ws; scat(b," GALAXY"); }
static int pcm_at_target(const WsMission *m){ if(m->gal!=gal_hash()) return 0; return m->tsys ? sys_id(cur_sys)==m->tsys : cur_sys==m->tidx; }
static int pcm_here(void){ for(int i=0;i<pcm_nact;i++) if(pcm_at_target(&pcm_act[i])) return i; return -1; }
static int pcm_match(const WsCraft *c, int inside){ for(int i=0;i<pcm_nact;i++){ const WsMission *m=&pcm_act[i]; if(m->cseed==c->seed && m->z==c->z && (inside<0 || ms_inside(m->kind)==inside) && pcm_at_target(m)) return i; } return -1; }
static void pcm_prune(void){ for(int i=0;i<pcm_nact;i++) if(pcm_act[i].dl_abs>0 && visits>pcm_act[i].dl_abs){ space_say("A MISSION EXPIRED"); pcm_act[i]=pcm_act[--pcm_nact]; i--; } }
static int pcm_match_k(const WsCraft *c){ int i=pcm_match(c,-1); return i>=0?pcm_act[i].kind:-1; }
static int pcm_obj_kind(void){ int mi=pcm_match(&pcw_f.c,1); return mi>=0 && !pcm_act[mi].prog ? pcm_act[mi].objsub : -1; }
// ---------------------------------------------------------------- encounters (see below): one open craft near here
static void pcm_enc_offer(WsMission *m){ memset(m,0,sizeof *m); m->id=pcm_en.seed^0x77u; m->cseed=pcm_en.seed; m->z=pcm_en.z; m->kind=MK_DESTROY; m->place=MP_SPACE; m->slot=9; m->gal=pcm_en.gal; m->tsys=pcm_en.sys; m->plan=pcm_en.pl>0?pcm_en.pl:1; m->bonus=20+(pcm_en.type?30:0); m->faction=(uint8_t)(pcm_en.seed%MS_NFAC); m->tidx=-1; }
static int pcm_enc_here(void){ return pcm_en.on && pcm_en.gal==gal_hash() && pcm_en.sys==sys_id(cur_sys); }
static int pcm_offers(int place, WsMission *o){ int n=0; if(pcm_enc_here() && !pcm_is_act(pcm_en.seed^0x77u)) pcm_enc_offer(&o[n++]);
    MsPlace pl; pcm_place(&pl,place); for(int s=0;s<=MS_SLOTS;s++){ if(s==MS_SLOTS && !pl.cheat_world) continue; WsMission m; ms_offer(&m,&pl,s); if(pcm_is_done(m.id)||pcm_is_act(m.id)) continue; o[n++]=m; }
    { const char *ek=getenv("BUGSTAR_MS_KIND");                          // (tests/mission.sh) the first job on the station board is one of this kind, at this star, of this size
      if(ek && place==MP_STATION){ int k=atoi(ek); for(int sl=100;sl<256;sl++){ WsMission m; ms_offer(&m,&pl,sl); if(m.kind!=k || pcm_is_done(m.id) || pcm_is_act(m.id)) continue;
            const char *ez=getenv("BUGSTAR_MS_Z"), *ed=getenv("BUGSTAR_MS_DL"); m.far=0; m.dream=0; m.gal=gal_hash(); m.tsys=sys_id(cur_sys); m.tidx=-1; m.z=ez?atoi(ez):200; m.dl_rel=ed?atoi(ed):0;
            memmove(o+1,o,sizeof(WsMission)*(size_t)n); o[0]=m; n++; break; } } }
    return n; }
// ---------------------------------------------------------------- pay, fame, removal
static void pcm_say(const char *s); static void pcm_hell_begin(int quiet), pcm_raid_lost_world(int cls);
static void pcm_finish(int mi, const WsPay *p, const WsCraft *c, int pay){
    WsMission m=pcm_act[mi]; snprintf(pcm_note,sizeof pcm_note,"%s +%d PERCENT",ms_kind_name(m.kind),m.bonus);
    if(pcm_auto){ double at=ws_attack((double)level,laser_lv,ship_gun(),tech_lv); fprintf(stderr,"mp %ld finish kind=%d bonus=%d inside=%d seed=%u z=%d attack=%.17g out=%.17g,%lld reward=%.17g,%lld pay=%d\n",(long)pc_frames(),m.kind,m.bonus,ms_inside(m.kind),c->seed,c->z,at,
        pcw_f.kill.out_dmg[0].m,(long long)pcw_f.kill.out_dmg[0].e,p->reward.m,(long long)p->reward.e,pay); }
    if(pay){ pcw_pay(p->reward); pcw_kills++; if(wn_cmp(p->reward,pcw_best)>0) pcw_best=p->reward; if(p->cheat){ pcw_cheated=1; plw_set_cheated(1); } }
    if(m.kind==MK_DEFEND){ unsigned long long rw=0; int r=sim_invasion((unsigned)(6+c->cls*PCM_DEFEND_K+m.bonus/5),&rw);      // a DEFEND mission brings a real raid on this planet's colony
        if(r>0){ char b[48]; snprintf(b,sizeof b,"COLONY RAID WON: +%llu GOLD AT THE COLONY",rw); pcm_say(b); if(pcm_auto) fprintf(stderr,"mp %ld defend raid won reward %llu\n",(long)pc_frames(),rw); }
        else if(r<0){ pcm_say("THE COLONY LOST THE RAID. SEE THE COLONY"); if(pcm_auto) fprintf(stderr,"mp %ld defend raid LOST\n",(long)pc_frames());
            if(sim_planet_burnt()) pcm_raid_lost_world(c->cls); }          // a harsh raid that ends the planet: the same burnt world as a planet-buster (hell look from the next landing)
        else if(pcm_auto) fprintf(stderr,"mp %ld defend: no colony here\n",(long)pc_frames()); }
    fame+=15+c->cls*4+(m.dream?20:0); gain_xp(100+c->cls*200); pcm_count++; if(pcm_ndone<64) pcm_done[pcm_ndone++]=m.id; else { memmove(pcm_done,pcm_done+1,63*sizeof(u32)); pcm_done[63]=m.id; }
    pcm_act[mi]=pcm_act[--pcm_nact]; }
static int pcm_obj_done(void){ int mi=pcm_match(&pcw_f.c,1); return mi>=0 && pcm_act[mi].prog; }
// the ALLIED craft of an ESCORT job on this star: the convoy's flagship (smaller than the warship, always with a hangar). BUGSTAR_WS_ALLY=Z gives one anywhere (tests).
static int pcm_ally_craft(WsCraft *c){
    const char *ea=getenv("BUGSTAR_WS_ALLY"); u32 sd=0; int z=0;
    for(int i=0;i<pcm_nact;i++){ const WsMission *m=&pcm_act[i]; if(m->kind==MK_ESCORT && pcm_at_target(m)){ sd=m->cseed^0xA11E5u; z=m->z-100; break; } }
    if(!sd && ea){ sd=0xA11E5u; z=atoi(ea); }
    if(!sd) return 0; if(z<150) z=150;
    for(u32 k=0;k<64;k++){ ws_make(c,sd+k*0x9E3779B1u,z,0); for(int j=0;j<c->nsub;j++) if(c->sub[j].kind==WSS_HANGAR && c->sub[j].alive) return 1; }
    return 0; }
static void pcm_enc_clear(void){ pcm_en.on=0; pcm_en.type=0; }
static void pcm_on_win(WsPay *p){                                  // called by the kill: a mission for this craft changes the pay
    if(pcm_en.on && pcm_en.seed==pcw_f.c.seed && pcm_en.z==pcw_f.c.z) pcm_enc_clear();
    int mi=pcm_match(&pcw_f.c,0); if(mi<0) return;
    ms_pay(&pcw_f.c,&pcw_f.kill,pcw_f.ship.attack,&pcm_act[mi],p); pcm_finish(mi,p,&pcw_f.c,0); }
static void pcm_on_sab(int s){ int mi=pcm_match(&pcw_f.c,1);
    if(mi>=0 && pcm_act[mi].kind==MK_SABOTAGE && pcw_f.c.sub[s].kind==pcm_act[mi].objsub){ pcm_act[mi].prog=1; space_say("OBJECTIVE DONE"); }
    if(pcm_en.on && pcm_en.type==1 && pcm_en.seed==pcw_f.c.seed && pcw_f.c.sub[s].kind==WSS_TURRET){ pcm_en.type=0; pcm_q[0][0]=0; space_say("ITS PLANET WEAPON IS DEAD"); } }
static void pcm_collect(void){                                      // leaving a craft: inside jobs that are done get paid
    for(int i=0;i<pcm_nact;i++){ WsMission *m=&pcm_act[i]; if(!m->prog || m->cseed!=pcw_f.c.seed || m->z!=pcw_f.c.z) continue;
        WsPay p; ms_pay(&pcw_f.c,NULL,ws_attack((double)level,laser_lv,ship_gun(),tech_lv),m,&p); WsCraft c=pcw_f.c; pcm_finish(i,&p,&c,1); pcw_last=p; pcw_last_ok=1;
        pc_ws_breakdown((volatile u16*)GBAA(0x06000000),&c,&p); pcm_note[0]=0; build_space_palette(); REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); BG_PAL[57]=rgb15(14,16,30); BG_PAL[58]=rgb15(60,70,120);
        do_save(); pc_ws_slot_save(cur_slot); i--; } }
static void pcm_dwell(int target_room, const char **prompt, char *msg, int *msg_t){          // standing in the objective room
    int mi=pcm_match(&pcw_f.c,1);
    if(mi>=0 && !pcm_act[mi].prog && pcm_act[mi].kind!=MK_SABOTAGE && target_room>=0 && pcw_deck.room[target_room].sub!=255 && pcw_f.c.sub[pcw_deck.room[target_room].sub].kind==pcm_act[mi].objsub){
        pcm_dw+=1/60.0; *prompt=pcm_act[mi].kind==MK_STEAL?"STAY: COPYING THE PLANS":pcm_act[mi].kind==MK_RESCUE?"STAY: FREEING THE CAPTIVES":"STAY: SCANNING THE CORE";
        if(pcm_dw>=3){ pcm_act[mi].prog=1; scpy(msg,"OBJECTIVE DONE: GET TO A HANGAR, START"); *msg_t=240; sfx(6); } } else pcm_dw=0; }
// ---------------------------------------------------------------- screens
static void pcm_target_text(const WsMission *m, char *b){
    char nm[32];
    if(m->gal!=gal_hash()){ pcm_gal_name(m->gal,nm); scpy(b,nm); scat(b,", STAR "); ncat(b,m->tidx+1); return; }
    int i = m->tsys ? pcm_find_sys(m->tsys) : m->tidx; if(i<0||i>=NSYS){ scpy(b,"A STAR IN THE CLUSTER YOU LEFT"); return; }
    sys_name(i,nm); scpy(b,nm); scat(b,", PLANET "); ncat(b,m->plan); }
static void pcm_dir_text(const WsMission *m, char *b){ b[0]=0; if(m->gal!=gal_hash()){ scpy(b,"IN ANOTHER GALAXY: TRAVEL THERE"); return; }
    int i = m->tsys ? pcm_find_sys(m->tsys) : m->tidx; if(i<0||i>=NSYS) return; if(i==cur_sys){ scpy(b,"THE CRAFT IS HERE: HOLD R, PRESS SELECT"); return; }
    int dx=sys[i].x-sys[cur_sys].x, dy=sys[i].y-sys[cur_sys].y; scpy(b,"ON THE MAP: "); ncat(b,sys_dist(i,cur_sys)); scat(b," UNITS "); if(dy<-3) scat(b,"NORTH"); else if(dy>3) scat(b,"SOUTH"); if(dx<-3) scat(b,"WEST"); else if(dx>3) scat(b,"EAST"); }
static void pc_ms_map(volatile u16 *pg){ for(int k=0;k<pcm_nact;k++){ const WsMission *m=&pcm_act[k]; if(m->gal!=gal_hash()) continue; int i=m->tsys?pcm_find_sys(m->tsys):m->tidx; if(i<0||i>=NSYS) continue;
        int x=sys[i].x, y=sys[i].y; line2(pg,x-5,y,x,y-5,C_CYAN); line2(pg,x,y-5,x+5,y,C_CYAN); line2(pg,x+5,y,x,y+5,C_CYAN); line2(pg,x,y+5,x-5,y,C_CYAN); } }
static void pcm_detail(volatile u16 *pg, const WsMission *m, int journal, int conf){
    WsCraft *c=malloc(sizeof(WsCraft)); ws_make(c,m->cseed,m->z,m->cheat); char b[96], g[32], v[48], w[48]; double at=ws_attack((double)level,laser_lv,ship_gun(),tech_lv);
    ui_bg(pg,0,160); ui_panel(pg,4,4,232,152); ui_head(pg,10,8,220,journal?"MISSION JOURNAL":"MISSION BRIEFING",C_GOLD); int y=20;
    ms_giver(m,g,sizeof g); scpy(b,"FROM "); scat(b,g); scat(b," OF "); scat(b,ms_faction_name(m->faction)); text(pg,10,y,b,C_WHITE,1); y+=9;
    scpy(b,ms_kind_name(m->kind)); scat(b,": "); scat(b,ms_kind_about(m->kind)); text(pg,10,y,b,C_GOLD,1); if(m->dream) rtext(pg,230,y-9,"DREAM JOB",C_RED); y+=9;
    for(int r=0;r<3;r++){ ms_line(m,c->name,r,b,sizeof b); text(pg,10,y,b,C_GREY,1); y+=8; }
    ws_len_text(c->z,v,sizeof v); scpy(b,c->name); scat(b," (");  scat(b,ws_class_name(c->cls)); scat(b,", "); scat(b,v); scat(b,")"); text(pg,10,y,b,C_WHITE,1); y+=9;
    WsThreat t; ws_threat(c,at,pcw_ehp(),wing_n,pcw_kits,fuel_max(),&t); scpy(b,"THREAT: "); scat(b,t.word); text(pg,10,y,b,t.tier>=4?C_RED:t.tier>=2?C_GOLD:C_GREEN,1);
    { WsKill k; memset(&k,0,sizeof k); k.attack=at; k.out_dmg[0]=c->hp_hull; WsPay p; ms_pay(c,&k,at,m,&p); wn_text(p.reward,v,sizeof v,0); snprintf(b,sizeof b,"PAYS %s G (BONUS %d PERCENT)",v,m->bonus); rtext(pg,230,y,b,C_GOLD); if(m->cheat) rtext(pg,230,y+8,"CHEATED SIZE: MARKED",C_RED); } y+=9;
    { WN a=wn(t.need_attack); wn_text(a,w,sizeof w,1); snprintf(b,sizeof b,"PREPARE: ATTACK %s TOUGH %d WING %d KITS %d",w,t.need_ehp,t.need_wing,t.need_kits); text(pg,10,y,b,C_GREY,1); y+=9; }
    pcm_target_text(m,b); { char t2[96]; scpy(t2,"TARGET: "); scat(t2,b); text(pg,10,y,t2,C_CYAN,1); y+=8; } pcm_dir_text(m,b); if(b[0]){ text(pg,10,y,b,C_CYAN,1); y+=8; }
    if(m->dl_rel||m->dl_abs){ int left = m->dl_abs ? m->dl_abs-(int)visits : m->dl_rel; snprintf(b,sizeof b,"DEADLINE: %d STAR JUMPS",left); text(pg,10,y,b,left<4?C_RED:C_GREY,1); } else text(pg,10,y,"NO DEADLINE",C_GREY,1);
    if(m->prog) rtext(pg,230,y,"OBJECTIVE DONE: LEAVE THE CRAFT",C_GREEN);
    if(journal) ui_foot(pg, conf?"PRESS SELECT AGAIN TO ABANDON IT":"SELECT: ABANDON","B: BACK"); else ui_foot(pg,pcm_nact>=PCM_MAXACT?"JOURNAL FULL (6)":"A: ACCEPT","B: BACK");
    free(c); }
static void pc_ms_board(volatile u16 *pg, int place){
    oam_hide_all(); oam_flush(); pcm_prune(); int tab=0, sel=0, det=0, dirty=1, conf=0; WsMission off[MS_SLOTS+3]; int noff=pcm_offers(place,off); WsCraft *cs=malloc(sizeof(WsCraft)*(MS_SLOTS+3));
    int outp = has_base(cur_sys) || ((base_mask>>(cur_sys&31))&1); wait_release();
    while(1){
        if(dirty){ dirty=0; for(int i=0;i<noff;i++) ws_make(&cs[i],off[i].cseed,off[i].z,off[i].cheat);
            if(det){ pcm_detail(pg, tab?&pcm_act[sel]:&off[sel], tab, conf); }
            else { ui_bg(pg,0,160); ui_panel(pg,4,4,232,152); char b[64], v[40]; scpy(b,tab?"MISSION JOURNAL":"MISSION BOARD: "); if(!tab) scat(b,ms_place_name(place)); ui_head(pg,10,8,220,b,C_GOLD);
                text(pg,10,18,"BOARD",tab==0?C_WHITE:C_DGREY,1); text(pg,50,18,"JOURNAL",tab==1?C_WHITE:C_DGREY,1); snprintf(b,sizeof b,"ACTIVE %d OF %d   DONE %d",pcm_nact,PCM_MAXACT,pcm_count); rtext(pg,230,18,b,C_GREY);
                int n = tab?pcm_nact:noff; if(sel>=n) sel=n?n-1:0;
                for(int i=0;i<n;i++){ int y=30+i*11; const WsMission *m = tab?&pcm_act[i]:&off[i]; WsCraft *c = tab?NULL:&cs[i]; WsCraft tmp; if(tab){ ws_make(&tmp,m->cseed,m->z,m->cheat); c=&tmp; }
                    if(i==sel) rect(pg,8,y-2,224,11,6); text(pg,14,y,ms_kind_name(m->kind),i==sel?C_WHITE:C_GREY,1); text(pg,62,y,ws_class_name(c->cls),i==sel?C_WHITE:C_GREY,1); ws_len_text(c->z,v,sizeof v); text(pg,126,y,v,C_GREY,1);
                    if(m->dream) text(pg,164,y,"DREAM",C_RED,1); else if(m->far) text(pg,164,y,"FAR",C_CYAN,1); else if(m->slot==9) text(pg,164,y,"OPEN",C_GREEN,1);
                    WsThreat t; ws_threat(c,ws_attack((double)level,laser_lv,ship_gun(),tech_lv),pcw_ehp(),wing_n,pcw_kits,fuel_max(),&t); rtext(pg,230,y,t.word,t.tier>=4?C_RED:t.tier>=2?C_GOLD:C_GREEN); }
                if(!n) text(pg,14,34,tab?"NO ACTIVE MISSIONS. TAKE ONE FROM A BOARD.":"NOTHING POSTED. COME BACK AFTER A MISSION.",C_GREY,1);
                if(pcm_en.on){ char e[48]; snprintf(e,sizeof e,"ENCOUNTERS: %s",pcm_enc_nm[pcm_enc_mode]); text(pg,10,142,e,C_DGREY,1); }
                ui_foot(pg, tab?"A: SEE  L/R: BOARD":(outp&&place!=MP_SPACE?"A: SEE  L/R: JOURNAL  START: OUTPOST":"A: SEE  L/R: JOURNAL"),"B: BACK"); } }
        vsync(); read_keys(); int n = tab?pcm_nact:noff;
        if(HIT(K_B)){ if(det){ det=0; conf=0; dirty=1; continue; } break; }
        if(!det){ if(n && HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; sfx(7); } if(n && HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; sfx(7); }
            if(HIT(K_L)||HIT(K_R)){ tab^=1; sel=0; dirty=1; sfx(7); }
            if(HIT(K_START) && outp && place!=MP_SPACE && !tab){ place = place==MP_OUTPOST ? MP_STATION : MP_OUTPOST; noff=pcm_offers(place,off); sel=0; dirty=1; sfx(7); }
            if(HIT(K_A) && n){ det=1; dirty=1; sfx(2); } }
        else if(!tab && HIT(K_A)){ if(pcm_nact>=PCM_MAXACT){ sfx(0); continue; } WsMission m=off[sel]; m.state=1; m.dl_abs = m.dl_rel ? (int)visits+m.dl_rel : 0; if(m.slot==9){ m.tsys=pcm_en.sys; m.gal=pcm_en.gal; }
            pcm_act[pcm_nact++]=m; noff=pcm_offers(place,off); det=0; sel=0; dirty=1; sfx(6); do_save(); pc_ws_slot_save(cur_slot); }
        else if(tab && HIT(K_SEL)){ if(!conf){ conf=1; dirty=1; } else { pcm_act[sel]=pcm_act[--pcm_nact]; conf=0; det=0; sel=0; dirty=1; sfx(4); do_save(); pc_ws_slot_save(cur_slot); } } }
    free(cs); wait_release(); }
// ---------------------------------------------------------------- random encounters
static void pcm_say(const char *s){ if(pcm_qn<3){ snprintf(pcm_q[pcm_qn],48,"%s",s); pcm_qn++; } }
// ---------------------------------------------------------------- AFTERMATH of a severe warship (NORMAL/COMMON only): HELLSCAPE, SEALED ENTRANCE, BURNING-SURFACE ESCAPE, FLOOR INVASIONS, BOARDING PARTY
// Nothing here touches the save except the stated death penalty (20 percent gold, cargo, hearts). The burnt-world list lives in the .ws file; an old file has none.
// Story worlds only burn for this visit (they heal on the next landing); endless-galaxy worlds stay burnt (hell look, colony in ruins: resettle later).
static void sim_burn_world(void);
typedef struct { int warn, fire, fire0, cls, ship, pod, sealed, dig, alt, resc, prev_scene, dmg_done, inv_d0, strand, board_sp, board_floor, sec, tst, hell, terr, alt_ser; u32 inv_mask, inv_sp; } PcmAft;
static int pcm_floor_serial=0;                                   // counts dungeon floors made (hook in dungeon.inc): a second stair belongs to one floor only
static void pcm_floor_new(void){ pcm_floor_serial++; }
static PcmAft pcm_aft; static u32 pcm_burnt[32]; static int pcm_nburnt=0;
static void pcm_aft_reset(void){ memset(&pcm_aft,0,sizeof pcm_aft); pcm_aft.alt=-1; pcm_nburnt=0; }
static void pcm_log(const char *fmt, int a, int b){ if(!getenv("BUGSTAR_AFT_LOG")) return; fprintf(stderr,"aft "); fprintf(stderr,fmt,a,b); fprintf(stderr,"\n"); }
static u32 pcm_pkey(void){ return hmix(sys_id(cur_sys)*0x9E3779B1u ^ gal_hash() ^ (u32)(cur_pl*131+cur_moon+1)*0x632BE5ABu ^ 0xB0A7u); }
static int pcm_burnt_here(void){ if(!endless) return 0; u32 k=pcm_pkey(); for(int i=0;i<pcm_nburnt;i++) if(pcm_burnt[i]==k) return 1; return 0; }
static int pcm_burnt_cls(void){ return (int)((pcm_pkey()>>7)%6u); }
static void pcm_burnt_add(u32 k){ for(int i=0;i<pcm_nburnt;i++) if(pcm_burnt[i]==k) return; if(pcm_nburnt<32) pcm_burnt[pcm_nburnt++]=k; }
static void pcm_burnt_save(FILE *f){ for(int i=0;i<pcm_nburnt;i++) fprintf(f,"burnt=%u\n",pcm_burnt[i]); }
// --- the numbers a player can prepare: armour, the SHIELD spell, a firefly friend, potions
static int pcm_fire_res(void){ int r=(int)(d_def>100?100:d_def)/2; if(shield_t>0) r+=20; if(have_bug[BUG_FIRE]) r+=15; return r>85?85:r; }
static s64 pcm_dps(void){ PcmAft *a=&pcm_aft; int el=a->fire0-a->fire, ramp=100+(el/1800)*10; if(ramp>250) ramp=250;
    s64 d = sdiv(smul(maxh,(s64)(10+a->cls*2)*ramp*(100-pcm_fire_res())),10000000); if(d<1) d=1; if(opt_baby) d=1; return d; }
static int pcm_potions(void){ int n=0; for(int i=0;i<bag_n;i++) if(bag[i].it==IT_POTION) n++; return n; }
static int pcm_drink(void){ for(int i=0;i<bag_n;i++) if(bag[i].it==IT_POTION){ drop_at(i); hearts=sadd(hearts,sdiv(maxh,3)); if(hearts>maxh) hearts=maxh; pcm_say("A POTION KEEPS YOU ALIVE"); return 1; } return 0; }
static int pcm_survive_s(void){ s64 tot=sadd(hearts,smul(pcm_potions(),sdiv(maxh,3))), d=pcm_dps(); s64 s=sdiv(tot,d); return s>999?999:(int)s; }
#define PCM_SHX ((64<<16)-(3<<16))
#define PCM_SHY ((64<<16)+(1<<16))
static int pcm_ship_dist(void){ int dx=wrapd(ppx-PCM_SHX)>>16, dy=wrapd(ppy-PCM_SHY)>>16; if(dx<0) dx=-dx; if(dy<0) dy=-dy; return dx>dy ? dx+dy/2 : dy+dx/2; }
static const char *pcm_ship_word(void){ return pcm_aft.ship==0?"INTACT":pcm_aft.ship==1?"DAMAGED":pcm_aft.pod?"WRECK, POD":wing_n>0?"WRECK, WING":"WRECK"; }
static void pcm_survive_say(void){ char b[48]; snprintf(b,sizeof b,"%d SEC TO LIVE. SHIP %d M, %s",pcm_survive_s(),pcm_ship_dist(),pcm_ship_word()); pcm_say(b); }
static void pcm_hud(volatile u16 *pg){ PcmAft *a=&pcm_aft; if(scene!=0 || a->fire<=0 || in_menu) return; char b[64];
    snprintf(b,sizeof b,"THE PLANET BURNS  LIVE %d S",pcm_survive_s()); stext(pg,40,46,b,C_RED);
    snprintf(b,sizeof b,"SHIP %d M %s  RESIST %d PCT  POTIONS %d",pcm_ship_dist(),pcm_ship_word(),pcm_fire_res(),pcm_potions()); stext(pg,40,55,b,C_GOLD);
    if(a->resc>0){ snprintf(b,sizeof b,"PICKUP IN %d S",(240-a->resc)/60+1); stext(pg,40,64,b,C_GREEN); } }
// --- the death penalty (the only thing that changes the save): wake at the nearest station
static void pcm_hell_death(void){ PcmAft *a=&pcm_aft; s64 g0=gold; gold -= gold/5; for(int i=0;i<8;i++) cargo[i]=0; hearts = maxh/3>0 ? maxh/3 : 1; if(hearts>maxh) hearts=maxh;
    a->fire=0; a->warn=0; a->sealed=0; a->resc=0; pcm_qn=0; pcm_say("THE FIRE TAKES YOU. YOU WAKE AT A STATION"); pcm_say("LOST 20 PERCENT OF YOUR GOLD AND ALL CARGO");
    pcm_log("death gold %d -> %d",(int)g0,(int)gold); launch_flag=1; if(scene==1) escape_flag=1; do_save(); pc_ws_slot_save(cur_slot); }
// --- COLLAPSING ENTRANCE: rubble seals the stairs up (floors count from 1: depth_no). Ways out: DIG (hold A on the stairs), ANOTHER STAIR (appears on floor 1), a PORTAL (spell/scroll)
static void pcm_seal_roll(int chance){ PcmAft *a=&pcm_aft; if(scene!=1 || a->sealed || base_mode || forge_on) return;
    u32 h=hmix(pcm_pkey()^(u32)pcm_clk*0x2545F491u^0x5EA1u); if((int)(h%100u)>=chance) return;
    a->sealed=1; a->dig=0; a->alt=-1; pcm_say("THE ENTRANCE COLLAPSES: RUBBLE SEALS IT"); pcm_say("DIG OUT (HOLD A), FIND A STAIR, OR PORTAL"); pcm_log("sealed %d %d",depth_no,0); }
static int pcm_cell(void){ return (int)(py_>>16)*MW+(int)(px_>>16); }
// the stairs up that lead OUT to the surface: in the endless galaxy the up stairs of ANY floor do (the game's own rule: "the way out"), in a story world only the ones on floor 1
static int pcm_up_surface(void){ return endless || forge_on || base_mode || depth_no<2; }
static void pcm_ensure_alt(void){ PcmAft *a=&pcm_aft; if(!a->sealed || !pcm_up_surface() || nrooms<2) return;
    if(a->alt>=0 && a->alt_ser==pcm_floor_serial && dmap[a->alt]==D_UP) return;                  // this floor already has its second stair (an old floor's cell number means nothing here)
    a->alt=-1; int upc=(int)(up_y>>16)*MW+(int)(up_x>>16);
    for(int t=0;t<nrooms;t++){ int ri=1+(int)((hmix(pcm_pkey()^0xA17u^(u32)depth_no*0x9E3779B1u)+(u32)t)%(u32)(nrooms-1)); int c=room_floor_cell(&rooms[ri]);
        if(c!=upc && dmap[c]==D_FLOOR){ dmap[c]=D_UP; add_light(c%MW,c/MW,40); fcode[c]=7; a->alt=c; a->alt_ser=pcm_floor_serial; pcm_say("ANOTHER STAIR UP IS OPEN IN THIS FLOOR"); pcm_log("alt %d %d",c,upc); pcm_log("alt_depth %d %d",depth_no,0); return; } } }
static int pcm_count_it(int it){ int n=0; for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].it==it && is_mon(dob[i].kind)) n++; return n; }
static int pcm_up_blocked(int surface){ PcmAft *a=&pcm_aft; char b[48]; int n;
    if(depth_no<32 && ((a->inv_mask>>depth_no)&1u) && (n=pcm_count_it(99))>0){ snprintf(b,sizeof b,"TROOPS HOLD THE STAIRS: %d LEFT",n); say(b); msg_timer=60; pcm_log("blocked troops %d",n,0); return 1; }
    if(surface && a->sealed && pcm_cell()!=a->alt){ say("RUBBLE: HOLD A TO DIG OUT"); msg_timer=60; pcm_log("blocked rubble %d",0,0); return 1; }
    return 0; }
// --- troops and boarders: real monsters of the game, marked in DObj.it (99 troops, 98 boarders)
static void pcm_spawn(int kind, int n, int it, int champ, int mult, int room0){
    int base_hp=6+dlv()*5+depth_no*2;
    for(int k=0;k<n && ndo<NDO-28;k++){ DObj *o=&dob[ndo++]; memset(o,0,sizeof *o); int ri = room0||nrooms<2 ? 0 : 1+(int)(hmix(pcm_pkey()^(u32)(k*977+depth_no*31))%(u32)(nrooms-1)); int c=room_floor_cell(&rooms[ri]);
        long long hp=(long long)xo_hp(kind_hp(kind,base_hp))*mult; if(hp>30000) hp=30000; o->kind=kind; o->alive=1; o->hp=o->maxhp=(s16)hp; o->it=it; o->champ=champ; o->cname=k&7;
        o->x=((c%MW)<<16)+0x8000; o->y=((c/MW)<<16)+0x8000; } }
static void pcm_inv_wave(int cls, int w){ PcmAft *a=&pcm_aft; int f=a->inv_d0-w; char b[48];
    if(f<1){ pcm_en.type=0; return; } if(f<32) a->inv_mask|=1u<<f; snprintf(b,sizeof b,f==a->inv_d0?"TROOPS HIT YOUR FLOOR (B%d)":"TROOPS HIT FLOOR B%d, ABOVE YOU",f); pcm_say(b); pcm_log("wave floor %d cls %d",f,cls); }
// --- THE HELLSCAPE
// patch 45: the burning world gets its FULL hell terrain in the same visit (no need to leave and land again): an endless-galaxy world is made again with its hell class
// (make_world, exactly what a later landing does, so the ground you see now is the ground you will land on), a story world gets the hell DNA by hand. You keep your place on the ground.
static void pcm_hell_dna(int c){                                  // the hell part of make_world's class switch, for a story world (its DNA comes from story_dna)
    wd.cls=(u8)c; wd.snowl=200; wd.hs=3; wd.snow=0; wd.sand=0;
    switch(c){ case PC_INFERNO: wd.style=5; wd.rough=3; wd.hour=1; wd.weather=4; wd.hs=5; break; case PC_BRIMSTONE: wd.style=1; wd.rough=3; wd.hour=1; wd.weather=3; wd.hs=4; break;
        case PC_OBSIDIAN: wd.style=6; wd.rough=3; wd.hour=2; wd.weather=0; wd.hs=6; break; case PC_BONE: wd.style=2; wd.rough=2; wd.hour=1; wd.weather=5; wd.sand=1; wd.hs=4; break;
        case PC_BLOODSEA: wd.style=3; wd.rough=2; wd.hour=2; wd.weather=0; wd.hs=5; break; default: wd.style=4; wd.rough=3; wd.hour=2; wd.weather=2; wd.snow=1; wd.snowl=70; wd.hs=5; break; }
    wd.mood=MOOD_HATE; wd.city=0; wd.csize=0; wd.ruins=0; wd.fauna=5; wd.beetles=3; }
static void pcm_hell_terrain(void){
    PcmAft *a=&pcm_aft; if(scene!=0 || a->terr) return; a->terr=1;
    s32 sx=ppx, sy=ppy; int c=PC_INFERNO+pcm_burnt_cls(), sv=dbg_cls;
    if(endless && !cur_rock){ dbg_cls=c; make_world(); dbg_cls=sv; } else pcm_hell_dna(c);
    new_planet(); planet_enter(); ppx=sx; ppy=sy; cam_h=ground_h(ppx,ppy)+CAM_UP; planet_hud(); p_last=vbl_count;
    pcm_log("hell terrain cls %d (%d)",c,endless); }
static void pcm_hell_begin(int quiet){
    PcmAft *a=&pcm_aft; int cls=a->cls; if(a->fire>0) return;
    u32 h=hmix(pcm_pkey()^(u32)cls*0x9E3779B1u^0x4E11u); int ar=ship_armour(); int ex=ar>40?ar-40:0; int dthr=5+ex/4, pp=85-ex/3; int r=(int)(h%100u);   // ar = percent of damage taken (100 none): the ship is lost 5..20 percent of the time, a pod waits 85..65 percent (patch 17: the scale was upside down)
    a->fire=a->fire0=3600*(3+cls/2); a->sec=0; a->resc=0; a->dmg_done=0; a->strand=0; a->ship = r<dthr?2 : r<dthr+35?1 : 0; a->pod=(int)((h>>8)%100u)<pp;
    { const char *e=getenv("BUGSTAR_AFT_SHIP"); if(e) a->ship=atoi(e); e=getenv("BUGSTAR_AFT_POD"); if(e) a->pod=atoi(e); }
    if(endless) pcm_burnt_add(pcm_pkey());
    sim_burn_world();
    a->hell=1; if(scene==0) pcm_hell_terrain();
    pcm_log("hell ship %d pod %d",a->ship,a->pod);
    if(!quiet){ pcm_qn=0; pcm_say("THE PLANET BURNS"); if(scene==0) pcm_survive_say(); else { pcm_say("THE SURFACE IS FIRE: WAIT OR PREPARE"); }
        if(scene==1) pcm_seal_roll(30+cls*5); }
    do_save(); pc_ws_slot_save(cur_slot); }
static void pcm_raid_lost_world(int cls){ pcm_aft.cls=cls; pcm_say("THE WORLD IS LOST: IT BURNS FOR GOOD"); pcm_hell_begin(1); }          // a harsh raid that ends the planet: the same burnt world as a planet-buster (the hell look from the next landing)
static void pcm_test_env(void){ const char *e=getenv("BUGSTAR_AFT_GOLD"); if(e) gold=atoll(e); e=getenv("BUGSTAR_AFT_WING"); if(e) wing_n=atoi(e); e=getenv("BUGSTAR_AFT_HP"); if(e){ maxh=hearts=atoll(e); } }
static void pcm_test_hell(void){ pcm_test_env(); pcm_aft.cls=7; pcm_hell_begin(0); }
static void pcm_hell_warn(int cls){ PcmAft *a=&pcm_aft; if(a->warn>0 || a->fire>0) return; a->cls=cls; a->warn=600+(int)(((u32)pcm_clk*2654435761u>>8)%17401u); { char b[64]; int sc=a->warn/60; snprintf(b,sizeof b,"FIRESTORM IN %d MIN %d SEC: LEAVE OR TAKE COVER",sc/60,sc%60); pcm_say(b); } }
static void pcm_planet_lost(int cls){ pcm_aft.cls=cls; pcm_en.type=0; pcm_en.on=0; pcm_say(endless?"THE PLANET-BUSTER FIRES! THE WORLD BURNS":"THE PLANET-BUSTER FIRES! A STORY WORLD HEALS LATER"); pcm_hell_begin(1); pcm_say("THE PLANET BURNS"); }
static void pcm_fire_end(void){ PcmAft *a=&pcm_aft; int stranded=a->ship==2 && scene==0; a->fire=0; a->resc=0; pcm_say("THE FIRES DIE DOWN: THE WORLD IS ASH"); pcm_log("fire end stranded %d",stranded,0);
    if(stranded){ pcm_say("A RESCUE SHIP FINDS YOU"); launch_flag=1; } a->ship=0; }
static void pcm_fire_tick(void){
    PcmAft *a=&pcm_aft; if(a->fire<=0) return;
    if(a->ship==2) for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==O_SHIP) po[i].alive=0;
    if(launch_flag){ if(a->ship==1 && !a->dmg_done){ a->dmg_done=1; fuel -= fuel/3; if(hearts>maxh/10+1) hearts-=maxh/10; pcm_say("THE HULL IS HOLED: LIFT-OFF COSTS FUEL"); pcm_log("damaged launch %d",1,0); } return; }
    a->fire--;
    if(a->ship==2){
        if(a->pod || wing_n>0){ if(pcm_ship_dist()<=5){ if(++a->resc==1) pcm_say(a->pod?"ESCAPE POD: STAY CLOSE":"A WINGMAN IS COMING: STAY CLOSE");
                if(a->resc>=240){ pcm_say(a->pod?"THE POD LIFTS YOU OFF":"YOUR WINGMAN PICKS YOU UP"); launch_flag=1; pcm_log("rescued pod %d",a->pod,0); return; } } else a->resc=0; }
        else if(!a->strand){ a->strand=1; pcm_say("NO SHIP, NO POD, NO WINGMAN: STRANDED"); } }
    if(++a->sec>=60){ a->sec=0; s64 d=pcm_dps(); hearts-=d; if(a->fire%600<60) pcm_log("hp %d dps %d",(int)hearts,(int)d); sfx(4); shake_t=opt_shake?10:0;
        while(hearts<=0){ if(!pcm_drink()){ pcm_hell_death(); return; } }
        if(hearts<maxh/3) pcm_drink();
        hud_now(); if(a->fire%600<60) pcm_survive_say(); }
    if(a->fire<=0) pcm_fire_end(); }
// --- one call per frame (from pcm_tick, only on a planet or in a dungeon)
static void pcm_test_dun(void){ PcmAft *a=&pcm_aft; const char *t=getenv("BUGSTAR_AFT_DUN"); if(!t) return; a->tst++; if(getenv("BUGSTAR_AFT_TOPUP")) hearts=maxh;
    if(a->tst==120){ pcm_enc_mode=3; a->cls=7; pcm_test_env();
        if(!strcmp(t,"seal")) pcm_seal_roll(100);
        else if(!strcmp(t,"inv")){ pcm_en.on=1; pcm_en.seed=77; pcm_en.z=400; pcm_en.gal=gal_hash(); pcm_en.sys=sys_id(cur_sys); pcm_en.type=3; pcm_en.cd_end=pcm_clk+20*3600; pcm_en.expire=pcm_clk+40*3600; pcm_en.next=pcm_clk+60; pcm_en.strikes=0; }
        else if(!strcmp(t,"board")){ pcm_en.on=1; pcm_en.seed=78; pcm_en.z=400; pcm_en.gal=gal_hash(); pcm_en.sys=sys_id(cur_sys); pcm_en.type=4; pcm_en.cd_end=pcm_clk+3*3600; pcm_en.expire=pcm_clk+40*3600; pcm_en.next=pcm_clk+60; pcm_en.strikes=0; a->board_sp=0; } }
    { static int alt_done=0; if(a->tst<2) alt_done=0; if(getenv("BUGSTAR_AFT_ALT") && a->alt>=0 && a->sealed && !alt_done && a->alt_ser==pcm_floor_serial){ alt_done=1; px_=((a->alt%MW)<<16)+0x8000; py_=((a->alt/MW)<<16)+0x8000; stair_ok=1; pcm_log("tele_alt depth %d",depth_no,0); } }
    if(a->tst==900 && !strcmp(t,"inv")) pcm_log("up_blocked %d",pcm_up_blocked(1),0);
    if(a->tst==300 && !strcmp(t,"seal")) pcm_log("blocked_at_start %d",pcm_up_blocked(1),0); }
static void pcm_aft_tick(void){
    PcmAft *a=&pcm_aft; int ps=a->prev_scene; a->prev_scene=scene;
    if(a->warn>0){ a->warn--; if(a->warn==18000||a->warn==10800||a->warn==3600) pcm_say(a->warn==3600?"FIRESTORM IN 1 MINUTE":a->warn==10800?"FIRESTORM IN 3 MINUTES":"FIRESTORM IN 5 MINUTES"); else if(a->warn==1800) pcm_say("FIRESTORM IN 30 SECONDS"); else if(a->warn==600) pcm_say("FIRESTORM IN 10 SECONDS"); else if(a->warn==300) pcm_say("FIRESTORM IN 5 SECONDS"); if(!a->warn) pcm_hell_begin(0); }
    if(scene==0){
        if(ps==1){ pcm_log("exit dungeon sealed %d",a->sealed,0); a->sealed=0; a->alt=-1; a->inv_mask=a->inv_sp=0; a->board_sp=0; if(pcm_en.type==4) pcm_en.type=0; if(a->hell && !a->terr) pcm_hell_terrain(); if(a->fire>0){ pcm_say("YOU ARE ON THE BURNING SURFACE"); pcm_survive_say(); pcm_log("arrived burning %d",pcm_survive_s(),0); } }
        pcm_fire_tick(); return; }
    // ---- scene 1: a dungeon
    if(a->fire>0 && --a->fire<=0) pcm_fire_end();
    pcm_test_dun();
    if(a->sealed){ pcm_ensure_alt();
        if(pcm_up_surface() && pcm_cell()==((int)(up_y>>16)*MW+(int)(up_x>>16)) && HELD(K_A)){ int need=(14-(int)(st_str>200?200:st_str)/20)*60; if(need<300) need=300;
            if(++a->dig%60==0){ char b[40]; snprintf(b,sizeof b,"DIGGING %d OF %d",a->dig/60,need/60); say(b); msg_timer=60; }
            if(a->dig>=need){ a->sealed=0; a->dig=0; pcm_say("YOU DIG THROUGH: THE WAY UP IS OPEN"); pcm_log("dug %d",depth_no,0); } } else a->dig=0; }
    if(depth_no<32 && ((a->inv_mask>>depth_no)&1u) && !((a->inv_sp>>depth_no)&1u)){ a->inv_sp|=1u<<depth_no; pcm_spawn(K_BULLETANT,3+a->cls/2>8?8:3+a->cls/2,99,0,1,1); pcm_say("TROOPS ON THIS FLOOR HOLD THE STAIRS"); pcm_log("troops floor %d n %d",depth_no,pcm_count_it(99)); }
    if(pcm_en.type==4 && pcm_en.on){
        if(!a->board_sp){ a->board_sp=1; a->board_floor=depth_no; int n=3+a->cls/2>8?8:3+a->cls/2; pcm_spawn(K_SENTRY,n,98,1,3,0); pcm_spawn(K_SENTRY,1,98,1,6,0); char b[48]; snprintf(b,sizeof b,"BOARDING PARTY: %d FIGHTERS AND A LEADER",n); pcm_say(b); pcm_log("boarders %d",pcm_count_it(98),0); }
        else if(depth_no==a->board_floor && pcm_count_it(98)==0){ pcm_en.type=0; earn(100+a->cls*60); pcm_say("BOARDING PARTY DEFEATED: SALVAGE GOLD"); pcm_log("boarders beaten %d",1,0); } } }
static void pcm_aft_away(void){ PcmAft *a=&pcm_aft; if(a->warn>0){ a->warn=0; pcm_hell_begin(1); } a->hell=0; a->terr=0; a->fire=0; a->resc=0; a->sealed=0; a->alt=-1; a->inv_mask=a->inv_sp=0; a->prev_scene=scene; }
static void pcm_enc_roll(void){
    static const int P[4]={0,8,22,55};                                   // chance per game minute, in thousandths, before danger and level
    int minute=pcm_clk/3600; u32 h=hmix(sys_id(cur_sys)*0x9E3779B1u ^ gal_hash() ^ (u32)(cur_pl*131+cur_moon)*0x632BE5ABu ^ (u32)minute*0x2545F491u ^ 0xE1C0u);
    int dg=sys_dlv(cur_sys); int p=P[pcm_enc_mode]*(100+dg*8+(level>2000?200:(int)level/10))/100; if(p>PCM_P_CAP[pcm_enc_mode]) p=PCM_P_CAP[pcm_enc_mode]; if((int)(h%1000u)>=p) return;
    int zp=pcm_zplayer(); int z=zp-300+(int)((h>>10)%450u); if(z<150) z=150; if(z>WS_ZMAX_FAIR) z=WS_ZMAX_FAIR;
    pcm_en.on=1; pcm_en.seed=hmix(h^1u)|1; pcm_en.z=z; pcm_en.sys=sys_id(cur_sys); pcm_en.gal=gal_hash(); pcm_en.pl=cur_pl+1; pcm_en.type=0; pcm_en.expire=pcm_clk+36000; pcm_en.next=pcm_clk+900; pcm_en.strikes=0; pcm_aft.board_sp=0; pcm_aft.inv_mask=pcm_aft.inv_sp=0;
    WsCraft *c=malloc(sizeof(WsCraft)); ws_make(c,pcm_en.seed,z,0); WsThreat t; ws_threat(c,ws_attack((double)level,laser_lv,ship_gun(),tech_lv),pcw_ehp(),wing_n,pcw_kits,fuel_max(),&t);
    char b[64], l[24]; ws_len_text(z,l,sizeof l); pcm_qn=0;
    pcm_say(scene==1?"THE CEILING SHAKES: A WARSHIP IS OVERHEAD":"A WARSHIP DROPS OUT OF WARP"); snprintf(b,sizeof b,"%s, %s, THREAT: %s",c->name,l,t.word); pcm_say(b);
    int severe = pcm_enc_mode>=2 && c->cls>=WC_CITADEL;
    if(severe){ { unsigned sr=(h>>20)%6u; pcm_en.type = sr==0 ? 1 : sr<3 ? 2 : 3; } int mins=15-c->cls; if(mins<PCM_BUSTER_MIN) mins=PCM_BUSTER_MIN; if(mins>12) mins=12;
        if(pcm_en.type==1){ pcm_en.cd_end=pcm_clk+mins*3600; snprintf(b,sizeof b,"PLANET-BUSTER: %d MIN TO LEAVE OR STOP IT",mins); }
        else if(pcm_en.type==2){ pcm_en.cd_end=pcm_clk+mins*3600; snprintf(b,sizeof b,"ORBITAL STRIKES COMING: TAKE COVER"); }
        else { pcm_en.cd_end=pcm_clk+mins*3600; snprintf(b,sizeof b,scene==1?"TROOPS LAND ON THE UPPER FLOORS":"TROOP DROPS ON THE SURFACE"); }
        pcm_say(b); }
    else if(scene==1 && pcm_enc_mode>=2 && (h>>24)%4==0){ pcm_say("A BOARDING PARTY IS IN THE DUNGEON"); pcm_en.type=4; pcm_en.cd_end=pcm_clk+3*3600; }
    else pcm_say("IT SHOWS ON THE MISSION BOARD AND SCANNER");
    if(pcm_en.type) pcm_quiet_until=pcm_clk+PCM_QUIET_MIN*3600;
    free(c); do_save(); pc_ws_slot_save(cur_slot); }
static void pcm_tick(void){
    if(pcm_enc_mode==0 && !pcm_en.on && !pcm_aft.fire && !pcm_aft.warn) return;
    pcm_clk++;
    if(pcm_en.on && (pcm_en.gal!=gal_hash() || pcm_en.sys!=sys_id(cur_sys) || pcm_clk>pcm_en.expire)){ if(pcm_en.type==1 && pcm_en.gal==gal_hash() && pcm_en.sys==sys_id(cur_sys) && scene<2) {} else { pcm_enc_clear(); } }
    if(scene>1) { if(scene==2 && pcm_en.type==1) pcm_en.type=0; pcm_aft_away(); return; }
    if(in_lair||base_mode||beast_fight||kr_mode||in_menu||pc_ws_on) return;
    if(msg_timer==0 && pcm_qn>0){ show_msg(pcm_q[0]); for(int i=1;i<pcm_qn;i++) memcpy(pcm_q[i-1],pcm_q[i],48); pcm_qn--; }
    pcm_aft_tick();
    if(!pcm_en.on){ if(pcm_enc_mode && (pcm_clk%3600==0 || pcm_enc_force) && hearts>0 && (pcm_clk>=pcm_quiet_until || pcm_enc_force)){ pcm_enc_force=0; pcm_enc_roll(); } return; }
    if(pcm_en.type && pcm_clk>=pcm_en.next){
        int left=(pcm_en.cd_end-pcm_clk)/60; char b[48]; WsCraft *c=malloc(sizeof(WsCraft)); ws_make(c,pcm_en.seed,pcm_en.z,0); int cls=c->cls; free(c);
        if(pcm_en.type==1){ snprintf(b,sizeof b,"PLANET-BUSTER: %d MIN %d SEC LEFT",left/60,left%60); pcm_say(b); pcm_en.next=pcm_clk+(left>60?1800:600); if(pcm_clk>=pcm_en.cd_end) pcm_planet_lost(cls); }
        else if(pcm_en.type==2){ if(pcm_en.strikes%2==0){ pcm_say("WARNING: ORBITAL STRIKE IN 5 SECONDS"); pcm_en.next=pcm_clk+300; } else { hurt(4+cls*2); pcm_say("THE STRIKE HITS! BUILDINGS BURN"); pcm_en.next=pcm_clk+1500; } pcm_en.strikes++; if(scene==1) pcm_seal_roll(15+cls*3); if(pcm_en.strikes>=8 || pcm_clk>=pcm_en.cd_end){ pcm_en.type=0; if(pcm_enc_mode==3 || cls>=WC_MOON) pcm_hell_warn(cls); } }
        else if(pcm_en.type==4){ pcm_en.next=pcm_clk+600; if(!pcm_aft.board_sp && pcm_clk>=pcm_en.cd_end) pcm_en.type=0; }
        else if(pcm_en.type==3 && scene==1){ if(pcm_en.strikes%2==0){ if(pcm_en.strikes==0) pcm_aft.inv_d0=depth_no; pcm_aft.cls=cls; pcm_say("TROOPS ARE COMING DOWN THE STAIRS: BRACE"); pcm_en.next=pcm_clk+300; } else { pcm_inv_wave(cls,pcm_en.strikes/2); pcm_en.next=pcm_clk+1500; } pcm_en.strikes++; if(pcm_en.strikes>=12 || pcm_clk>=pcm_en.cd_end) pcm_en.type=0; }
        else { if(pcm_en.strikes%2==0){ pcm_say(scene==1?"TROOPS HIT THE FLOOR ABOVE: BRACE":"TROOPS ARE LANDING: BRACE"); pcm_en.next=pcm_clk+300; } else { hurt(3+cls); pcm_say("A TROOP WAVE REACHES YOU"); pcm_en.next=pcm_clk+1500; } pcm_en.strikes++; if(pcm_en.strikes>=10 || pcm_clk>=pcm_en.cd_end) pcm_en.type=0; } } }
static void pcm_fight_tick(void){ if(pcm_en.on && pcm_en.type==1 && pcm_en.seed==pcw_f.c.seed && pcw_f.layers_up==0){ pcm_en.type=0; space_say("SHIELDS DOWN: ITS PLANET WEAPON FAILED"); } }
static int pcm_contact(int i, u32 *seed, int *z){ if(i==1){ int mi=pcm_here(); if(mi>=0){ *seed=pcm_act[mi].cseed; *z=pcm_act[mi].z; return 1+pcm_act[mi].cheat; } }
    if(i==2 && pcm_enc_here()){ *seed=pcm_en.seed; *z=pcm_en.z; return 1; } return 0; }
static u32 pcm_test_setup(int z){ WsMission o[8]; pcm_offers(MP_STATION,o); WsMission m=o[0]; m.kind=MK_DESTROY; m.state=1; m.gal=gal_hash(); m.tsys=sys_id(cur_sys); m.z=z; m.cseed=0; pcm_act[pcm_nact++]=m; return 0; }
// ---------------------------------------------------------------- THE MISSION PLAYTHROUGH (tests/mission.sh): the hook BUGSTAR_WS_TEST=<mode> with BUGSTAR_MS_AUTO=1 plays a whole job by script keys
//   mplay    the station board (the keys accept the first job: BUGSTAR_MS_KIND/Z/DL pick its kind, size and deadline), then space, the SCANNER (the keys engage the job's craft), then the fight:
//            jobs done from outside get the shields and hull shot away by the hook, jobs done inside: the hook drops the shield layers and the keys fly (B thrust, R brake), board (R + A), work inside
//            (the hook only walks you home once the objective is done); the pay comes through the cost breakdown (a key closes it). Every step says "mp <script frame> ..." on stderr.
//   maccept  only the board.   mresume  CONTINUE from a save made after maccept: the job in the journal is played.   mcheck  prints the loaded state.   mexpire  jumps the star counter past the deadline.
static int mp_st=0, mp_wait=0, mp_wait_n=30, mp_n=0, mp_kind=-1, mp_fight_done=0; static int mp_last_nact=0;
#define MPL(...) do{ fprintf(stderr,"mp %ld ",(long)pc_frames()); fprintf(stderr,__VA_ARGS__); fputc('\n',stderr); }while(0)
static void mp_state(const char *tag){ char w[48]; pcw_wallet(w,sizeof w);
    MPL("%s gold=%lld wallet=%s vault=%.15g,%lld dust=%.6f kills=%u nact=%d ndone=%d count=%d fame=%d xp=%lld hull=%d",tag,(long long)gold,w,pcw_vault.m,(long long)pcw_vault.e,pcw_dust,pcw_kills,pcm_nact,pcm_ndone,pcm_count,(int)fame,(long long)xp,hull); }
static void mp_journal(void){ for(int i=0;i<pcm_nact;i++){ char b[300]; ms_pack(&pcm_act[i],b,sizeof b); MPL("journal %d: %s",i,b); } for(int i=0;i<pcm_ndone;i++) MPL("done %d: %u",i,pcm_done[i]); }
static void pcm_mp_start(const char *mode){
    pcm_auto=1; { const char *lv=getenv("BUGSTAR_WS_LEVEL"); if(lv) level=atoll(lv); } { const char *en=getenv("BUGSTAR_MS_ENC"); pcm_enc_mode = en ? atoi(en)&3 : 0; }
    mp_state("start"); mp_journal();
    if(!strcmp(mode,"mscan")){ if(scene!=2){ oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(3); } mp_wait=0; mp_kind=-1; mp_st=1; return; }          // (tests/walk.sh) space and the scanner: the keys pick a row
    if(!strcmp(mode,"mcheck")) return;
    if(!strcmp(mode,"mexpire")){ visits+=60; pcm_prune(); mp_state("after 60 jumps"); mp_journal(); return; }
    if(strcmp(mode,"mresume")){
        pc_ms_board((volatile u16*)GBAA(0x06000000),MP_STATION); mp_state("board closed"); mp_journal();
        if(pcm_nact>0) MPL("accepted kind=%s dl_abs=%d visits=%d",ms_kind_name(pcm_act[0].kind),pcm_act[0].dl_abs,(int)visits);
        if(!strcmp(mode,"maccept")) return; }
    if(pcm_nact<1){ MPL("NO JOB: nothing to play"); return; }
    mp_kind=pcm_act[0].kind; mp_last_nact=pcm_nact; mp_fight_done=0; mp_n=0;
    oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(3);
    mp_wait=0; mp_st=1; }
static void pcm_mp_tick(void){
    if(!mp_st) return; mp_n++;
    if(mp_st==1){ if(scene!=2 || ++mp_wait<mp_wait_n) return;
        MPL("scanner opens (kind %s)",mp_kind>=0?ms_kind_name(mp_kind):"none"); pc_ws_scan(); MPL("scanner closed, fight on=%d",pc_ws_on);
        if(!pc_ws_on || mp_kind<0){ mp_st=0; return; }
        MPL("engaged %s z=%d class %s wing=%d escort=%d",pcw_f.c.name,pcw_f.c.z,ws_class_name(pcw_f.c.cls),pcw_f.ship.wing,pcm_escort); mp_st=2; mp_wait=0; return; }
    if(mp_st==2){
        if(!pc_ws_on){ MPL("fight over"); mp_st=3; mp_wait=0; return; }
        hull=100; if(fuel<fuel_max()/2) fuel=fuel_max();
        if(!ms_inside(mp_kind)){                                              // from outside: shoot it down (the same loop as the 'kill' test mode)
            if(!mp_fight_done){ mp_fight_done=1; MPL("shooting"); while(pcw_f.state==WF_ON && pcw_f.shots<2000000u){ pcw_f.fire_cd=0; pcw_f.overheat=0; pcw_f.heat=0; wf_fire(&pcw_f,0,-1); } if(pcw_f.state==WF_WON) pcw_victory(); } return; }
        if(!mp_fight_done){ mp_fight_done=1; pcw_f.layers_up=0; pcw_f.lay_hp=wn(0); MPL("shield layers dropped (the keys fly and board)"); }
        if(mp_n%30==0){ s32 pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4; MPL("fly pd=%d (board < %d) speed=%d board_ok=%d hull=%d",(int)pd,PCW_DS*9/10,speed,pcw_board_ok,hull); }
        if(pcm_nact<mp_last_nact){ MPL("job paid out inside"); mp_st=3; mp_wait=0; }
        return; }
    if(mp_st==3){ if(++mp_wait<90) return; if(pc_ws_on){ pc_ws_sel(); }
        mp_state("final"); mp_journal(); mp_st=0; }
}
