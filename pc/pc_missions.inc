// pc_missions.inc: WARSHIP MISSIONS and random WARSHIP ENCOUNTERS in the PC build (included at the end of pc_warship.inc).
// The rules are in pc/warship/ms.c (plain C, tested). This file is the glue: the mission board (space menu, station, town, outpost),
// the journal, map marks, saving (in the .ws file), pay through the cost breakdown, and the encounters (planet and dungeon).
#include "warship/ms.h"
#define PCM_MAXACT 6
static WsMission pcm_act[PCM_MAXACT]; static int pcm_nact=0; static u32 pcm_done[64]; static int pcm_ndone=0; static int pcm_count=0;
static int pcm_escort=0; static double pcm_dw=0;
static int pcm_enc_mode=1;                                     // WARSHIP ENCOUNTERS: 0 OFF, 1 RARE (default), 2 NORMAL, 3 COMMON
static const char *const pcm_enc_nm[4]={"OFF","RARE","NORMAL","COMMON"};
typedef struct { int on; u32 seed; int z; u32 sys, gal; int pl, type, cd_end, expire, strikes, next; } PcmEnc;
static PcmEnc pcm_en; static char pcm_q[3][48]; static int pcm_qn=0;
static void pcm_reset(void){ pcm_enc_mode=1; pcm_nact=0; pcm_ndone=0; pcm_count=0; pcm_note[0]=0; pcm_escort=0; memset(&pcm_en,0,sizeof pcm_en); pcm_qn=0; }
// ---------------------------------------------------------------- save (inside the .ws file; an old file simply has no such lines)
static void pcm_save(FILE *f){ char b[300]; fprintf(f,"menc=%d\nmcount=%d\n",pcm_enc_mode,pcm_count);
    for(int i=0;i<pcm_nact;i++){ ms_pack(&pcm_act[i],b,sizeof b); fprintf(f,"mis=%s\n",b); }
    for(int i=0;i<pcm_ndone;i++) fprintf(f,"mdone=%u\n",pcm_done[i]); }
static int pcm_load_line(const char *ln){ unsigned u; int v;
    if(!strncmp(ln,"mis=",4)){ if(pcm_nact<PCM_MAXACT && ms_unpack(ln+4,&pcm_act[pcm_nact])) { pcm_act[pcm_nact].state=1; pcm_nact++; } return 1; }
    if(sscanf(ln,"mdone=%u",&u)==1){ if(pcm_ndone<64) pcm_done[pcm_ndone++]=u; return 1; }
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
    MsPlace pl; pcm_place(&pl,place); for(int s=0;s<=MS_SLOTS;s++){ if(s==MS_SLOTS && !pl.cheat_world) continue; WsMission m; ms_offer(&m,&pl,s); if(pcm_is_done(m.id)||pcm_is_act(m.id)) continue; o[n++]=m; } return n; }
// ---------------------------------------------------------------- pay, fame, removal
static void pcm_finish(int mi, const WsPay *p, const WsCraft *c, int pay){
    WsMission m=pcm_act[mi]; snprintf(pcm_note,sizeof pcm_note,"%s +%d PERCENT",ms_kind_name(m.kind),m.bonus);
    if(pay){ pcw_pay(p->reward); pcw_kills++; if(wn_cmp(p->reward,pcw_best)>0) pcw_best=p->reward; if(p->cheat){ pcw_cheated=1; plw_set_cheated(1); } }
    fame+=15+c->cls*4+(m.dream?20:0); gain_xp(100+c->cls*200); pcm_count++; if(pcm_ndone<64) pcm_done[pcm_ndone++]=m.id; else { memmove(pcm_done,pcm_done+1,63*sizeof(u32)); pcm_done[63]=m.id; }
    pcm_act[mi]=pcm_act[--pcm_nact]; }
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
        pc_ws_breakdown((volatile u16*)0x06000000,&c,&p); pcm_note[0]=0; build_space_palette(); REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); BG_PAL[57]=rgb15(14,16,30); BG_PAL[58]=rgb15(60,70,120);
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
static void pcm_enc_roll(void){
    static const int P[4]={0,8,22,55};                                   // chance per game minute, in thousandths, before danger and level
    int minute=pcm_clk/3600; u32 h=hmix(sys_id(cur_sys)*0x9E3779B1u ^ gal_hash() ^ (u32)(cur_pl*131+cur_moon)*0x632BE5ABu ^ (u32)minute*0x2545F491u ^ 0xE1C0u);
    int dg=sys_dlv(cur_sys); int p=P[pcm_enc_mode]*(100+dg*8+(level>2000?200:(int)level/10))/100; if((int)(h%1000u)>=p) return;
    int zp=pcm_zplayer(); int z=zp-300+(int)((h>>10)%450u); if(z<150) z=150; if(z>WS_ZMAX_FAIR) z=WS_ZMAX_FAIR;
    pcm_en.on=1; pcm_en.seed=hmix(h^1u)|1; pcm_en.z=z; pcm_en.sys=sys_id(cur_sys); pcm_en.gal=gal_hash(); pcm_en.pl=cur_pl+1; pcm_en.type=0; pcm_en.expire=pcm_clk+36000; pcm_en.next=pcm_clk+900; pcm_en.strikes=0;
    WsCraft *c=malloc(sizeof(WsCraft)); ws_make(c,pcm_en.seed,z,0); WsThreat t; ws_threat(c,ws_attack((double)level,laser_lv,ship_gun(),tech_lv),pcw_ehp(),wing_n,pcw_kits,fuel_max(),&t);
    char b[64], l[24]; ws_len_text(z,l,sizeof l); pcm_qn=0;
    pcm_say(scene==1?"THE CEILING SHAKES: A WARSHIP IS OVERHEAD":"A WARSHIP DROPS OUT OF WARP"); snprintf(b,sizeof b,"%s, %s, THREAT: %s",c->name,l,t.word); pcm_say(b);
    int severe = pcm_enc_mode>=2 && c->cls>=WC_CITADEL;
    if(severe){ pcm_en.type=1+(int)((h>>20)%3u); int mins=15-c->cls; if(mins<3) mins=3; if(mins>12) mins=12;
        if(pcm_en.type==1){ pcm_en.cd_end=pcm_clk+mins*3600; snprintf(b,sizeof b,"PLANET-BUSTER: %d MIN TO LEAVE OR STOP IT",mins); }
        else if(pcm_en.type==2){ pcm_en.cd_end=pcm_clk+mins*3600; snprintf(b,sizeof b,"ORBITAL STRIKES COMING: TAKE COVER"); }
        else { pcm_en.cd_end=pcm_clk+mins*3600; snprintf(b,sizeof b,scene==1?"TROOPS LAND ON THE UPPER FLOORS":"TROOP DROPS ON THE SURFACE"); }
        pcm_say(b); }
    else if(scene==1 && pcm_enc_mode>=2 && (h>>24)%4==0){ pcm_say("A BOARDING PARTY IS IN THE DUNGEON"); pcm_en.type=3; pcm_en.cd_end=pcm_clk+3*3600; }
    else pcm_say("IT SHOWS ON THE MISSION BOARD AND SCANNER");
    free(c); do_save(); pc_ws_slot_save(cur_slot); }
static void pcm_planet_lost(void){
    int story=!endless; pcm_en.type=0; pcm_en.on=0;
    if(story){ hearts = hearts>2 ? hearts/2 : 1; pcm_say("A STORY WORLD CANNOT DIE: EVACUATE"); }
    else { gold -= gold/5; for(int i=0;i<8;i++) cargo[i]=0; hearts = hearts>2 ? hearts*2/3 : 1; pcm_say("THE PLANET IS DESTROYED. YOU LIVE"); }
    pcm_say("A STATION PICKED YOU UP. LOST GOLD AND CARGO"); launch_flag=1; do_save(); pc_ws_slot_save(cur_slot); }
static void pcm_tick(void){
    if(pcm_enc_mode==0 && !pcm_en.on) return;
    pcm_clk++;
    if(pcm_en.on && (pcm_en.gal!=gal_hash() || pcm_en.sys!=sys_id(cur_sys) || pcm_clk>pcm_en.expire)){ if(pcm_en.type==1 && pcm_en.gal==gal_hash() && pcm_en.sys==sys_id(cur_sys) && scene<2) {} else { pcm_enc_clear(); } }
    if(scene>1) { if(scene==2 && pcm_en.type==1) pcm_en.type=0; return; }
    if(in_lair||base_mode||beast_fight||kr_mode||in_menu||pc_ws_on) return;
    if(msg_timer==0 && pcm_qn>0){ show_msg(pcm_q[0]); for(int i=1;i<pcm_qn;i++) memcpy(pcm_q[i-1],pcm_q[i],48); pcm_qn--; }
    if(!pcm_en.on){ if(pcm_enc_mode && (pcm_clk%3600==0 || pcm_enc_force) && hearts>0){ pcm_enc_force=0; pcm_enc_roll(); } return; }
    if(pcm_en.type && pcm_clk>=pcm_en.next){
        int left=(pcm_en.cd_end-pcm_clk)/60; char b[48]; WsCraft *c=malloc(sizeof(WsCraft)); ws_make(c,pcm_en.seed,pcm_en.z,0); int cls=c->cls; free(c);
        if(pcm_en.type==1){ snprintf(b,sizeof b,"PLANET-BUSTER: %d MIN %d SEC LEFT",left/60,left%60); pcm_say(b); pcm_en.next=pcm_clk+(left>60?1800:600); if(pcm_clk>=pcm_en.cd_end) pcm_planet_lost(); }
        else if(pcm_en.type==2){ if(pcm_en.strikes%2==0){ pcm_say("WARNING: ORBITAL STRIKE IN 5 SECONDS"); pcm_en.next=pcm_clk+300; } else { hurt(4+cls*2); pcm_say("THE STRIKE HITS! BUILDINGS BURN"); pcm_en.next=pcm_clk+1500; } pcm_en.strikes++; if(pcm_en.strikes>=8 || pcm_clk>=pcm_en.cd_end) pcm_en.type=0; }
        else { if(pcm_en.strikes%2==0){ pcm_say(scene==1?"TROOPS HIT THE FLOOR ABOVE: BRACE":"TROOPS ARE LANDING: BRACE"); pcm_en.next=pcm_clk+300; } else { hurt(3+cls); pcm_say("A TROOP WAVE REACHES YOU"); pcm_en.next=pcm_clk+1500; } pcm_en.strikes++; if(pcm_en.strikes>=10 || pcm_clk>=pcm_en.cd_end) pcm_en.type=0; } } }
static void pcm_fight_tick(void){ if(pcm_en.on && pcm_en.type==1 && pcm_en.seed==pcw_f.c.seed && pcw_f.layers_up==0){ pcm_en.type=0; space_say("SHIELDS DOWN: ITS PLANET WEAPON FAILED"); } }
static int pcm_contact(int i, u32 *seed, int *z){ if(i==1){ int mi=pcm_here(); if(mi>=0){ *seed=pcm_act[mi].cseed; *z=pcm_act[mi].z; return 1+pcm_act[mi].cheat; } }
    if(i==2 && pcm_enc_here()){ *seed=pcm_en.seed; *z=pcm_en.z; return 1; } return 0; }
