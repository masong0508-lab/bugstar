// sim_ui.inc: the side game's screens: clock chip, who-is-this card, the quick menu, JOURNAL, PEOPLE and GOD POWERS.
// (Included by sim_glue.inc. All text is capital letters, short lines, like the rest of the game.)

static void sim_when(const SimEvent *e, char *b){ int day=(int)(e->tick/SIM_DAY_TICKS), hr=(int)((e->tick%SIM_DAY_TICKS)*24/SIM_DAY_TICKS); b[0]=0; scat(b,"D"); ncat(b,day+1); scat(b," "); if(hr<10) scat(b,"0"); ncat(b,hr); scat(b,"H"); }

static void sim_hud(volatile u16 *pg){
    if(!sim_is_on() || !sim_have_w || FILM || kr_mode) return;
    int hr=sim_hour(&sim_w); char t[40]; t[0]=0; scat(t,"D"); ncat(t,sim_day(&sim_w)+1); scat(t," "); if(hr<10) scat(t,"0"); ncat(t,hr); scat(t,"H ");
    scat(t,sim_weather_name(sim_w.weather)); if(sim_w.env_on){ scat(t," "); scat(t,sim_season_name(sim_season(&sim_w))); }
    int day = hr>=6 && hr<19;
    rect(pg,3,151,4,4,day?C_GOLD:C_CYAN); stext(pg,10,150,t,C_WHITE);
}

static int sim_talk(void *po_obj){
    if(!sim_is_on() || !sim_have_w) return 0;
    int idx=(int)((PObj*)po_obj-po), k=-1; for(int i=0;i<sim_w.nfolk;i++) if(sim_fpo[i]==idx) k=i;
    if(k<0) return 0;
    const SimFolk *f=&sim_w.folk[k]; char l0[44], l1[44], l2[44];
    scpy(l0,sim_job_name(f->job)); scat(l0,". "); scat(l0,sim_task_name(f->task)); scat(l0,". "); scat(l0,sim_trait_text(f)); scat(l0,".");
    sim_status_text(f,l1,sizeof l1); if(f->mate>=0 && f->mate<sim_w.nfolk){ int n1=0; while(l1[n1]) n1++; if(n1<26){ scat(l1," LOVES "); scat(l1,sim_w.folk[f->mate].name); } }
    scpy(l2,"IT IS "); { int hr=sim_hour(&sim_w); if(hr<10) scat(l2,"0"); ncat(l2,hr); } scat(l2,"H. "); scat(l2,sim_weather_name(sim_w.weather)); scat(l2,".");
    int fr=sim_talk_to(&sim_w,k); int shown = fr>=0 ? 1 : 0; if(fr<0) fr=sim_w.friend[k];
    char l3[44]; scpy(l3, fr ? "FRIENDSHIP " : "WE HAVE NOT MET BEFORE."); if(fr){ ncat(l3,fr); scat(l3,"/100"); if(shown) scat(l3," +"); }
    const char *ln[4]={l0,l1,l2,l3}; card(VISPG,f->name,ln,4);
    return 0;                                                  // the old talk (gifts, gossip) goes on after the card
}

// ---- a small scrolling list screen (like the MORE OPTIONS screen): rows are made by row(i,text,&colour)
static void sim_rows(volatile u16 *pg, const char *title, int n, void (*row)(int,char*,int*), void (*foot)(int,char*,int*), const char *hint, int *sel, int (*act)(int)){
    int top=0, dirty=1; if(*sel>=n) *sel=0; wait_release();
    while(1){
        if(*sel<top) top=*sel; if(*sel>=top+10) top=*sel-9;
        if(dirty){ dirty=0; char b[56]; int c;
            panel(pg,12,20,216,136,title);
            for(int r=0;r<10 && top+r<n;r++){ int i=top+r, y=37+r*9; c=C_GREY; row(i,b,&c); b[50]=0;
                if(i==*sel){ rect(pg,18,y-2,204,10,6); if(c==C_GREY) c=C_WHITE; }
                text(pg,22,y,b,c,1); }
            if(top>0) text(pg,216,37,"^",C_CYAN,1); if(top+10<n) text(pg,216,118,"v",C_CYAN,1);
            rect(pg,18,128,204,1,C_DGREY);
            if(foot){ char f1[56], f2[56]; int c1=C_GREY, c2=C_GREY; foot(*sel,f1,&c1); foot(-1-*sel,f2,&c2); f1[52]=0; f2[52]=0; text(pg,20,131,f1,c1,1); text(pg,20,139,f2,c2,1); }
            ctext(pg,120,148,hint,C_CYAN);
        }
        vsync(); read_keys(); if(keys!=keys_old) dirty=1;
        if(n){ if(HIT(K_UP)){ *sel=(*sel+n-1)%n; sfx(7); } if(HIT(K_DOWN)){ *sel=(*sel+1)%n; sfx(7); }
               if(HIT(K_L)){ *sel=*sel>=10 ? *sel-10 : 0; sfx(7); } if(HIT(K_R)){ *sel=*sel+10<n ? *sel+10 : n-1; sfx(7); } }
        if(act && HIT(K_A)){ if(act(*sel)==1){ wait_release(); return; } dirty=1; }
        if(HIT(K_B)||HIT(K_START)){ wait_release(); return; }
    }
}

// ---- JOURNAL: what happened, newest first
static void sim_jr_row(int i, char *b, int *c){
    const SimEvent *e=sim_log_at(&sim_w,(u32)i); if(!e){ b[0]=0; return; }
    char t[60], w[16]; sim_when(e,w); sim_event_text(&sim_w,e,t,sizeof t); scpy(b,w); while(textlen(b)<8) scat(b," "); scat(b,t);
    *c = e->kind==SIME_UNLOCK||e->kind==SIME_CAST ? C_GOLD : e->kind==SIME_HUNGRY||e->kind==SIME_TIRED||e->kind==SIME_EMPTY ? C_RED : e->kind==SIME_WEATHER ? C_CYAN : C_GREY;
}
static void sim_journal(volatile u16 *pg){
    int n=(int)sim_log_count(&sim_w), sel=0; char t[40]; scpy(t,"JOURNAL  "); ncat(t,n); scat(t," ENTRIES");
    if(!n){ const char *ln[2]={"NOTHING HAS HAPPENED YET.","COME BACK IN A MINUTE."}; card(pg,"JOURNAL",ln,2); return; }
    sim_rows(pg,t,n,sim_jr_row,0,"UP/DOWN: READ  L/R: PAGE  B: BACK",&sel,0);
}

// ---- PEOPLE: who lives here and how they are
static void sim_pp_row(int i, char *b, int *c){
    const SimFolk *f=&sim_w.folk[i]; scpy(b,f->name); while(textlen(b)<10) scat(b," "); scat(b,sim_job_name(f->job)); while(textlen(b)<26) scat(b," "); scat(b,sim_task_name(f->task));
    *c = sim_mood(f)>170 ? C_GREEN : sim_mood(f)>100 ? C_WHITE : C_RED;
}
static void sim_pp_foot(int i, char *b, int *c){
    b[0]=0; if(i>=0){ const SimFolk *f=&sim_w.folk[i]; sim_status_text(f,b,52); *c=C_WHITE; }
    else { const SimFolk *f=&sim_w.folk[-1-i]; scpy(b,"FOOD "); ncat(b,f->need[0]); scat(b,"  REST "); ncat(b,f->need[1]); scat(b,"  FRIENDS "); ncat(b,f->need[2]); scat(b,"  FUN "); ncat(b,f->need[3]); *c=C_GREY; }
}
static void sim_people(volatile u16 *pg){
    int sel=0; if(!sim_w.nfolk){ const char *ln[2]={"NOBODY LIVES HERE.","TRY A TOWN."}; card(pg,"PEOPLE",ln,2); return; }
    char t[32]; scpy(t,"PEOPLE  "); ncat(t,sim_w.nfolk);
    sim_rows(pg,t,sim_w.nfolk,sim_pp_row,sim_pp_foot,"UP/DOWN: PICK  B: BACK",&sel,0);
}

// ---- GOD POWERS
static int sim_god_sel, sim_gd_noted=-1;
static void sim_gd_row(int i, char *b, int *c){
    int n=i+1; Lv l; sim_level_lv(&l); const GodPower *p=god_power(n); int open=god_unlocked(&l,n,sim_w.god.sandbox);
    b[0]=0; if(n<10) scat(b," "); ncat(b,n); scat(b," "); scat(b,p->name); while(textlen(b)<30) scat(b," ");
    if(open){ scat(b,"COST "); ncat(b,sim_w.god.sandbox ? 0 : p->cost); *c = sim_w.god.sandbox || sim_w.spirit>=p->cost ? C_GREEN : C_RED; }
    else { Lv need; char t[24]; god_need_level(n,&need); lv_compact(&need,t,8); scat(b,"LV "); scat(b,t); *c=C_DGREY; }
}
static void sim_gd_foot(int i, char *b, int *c){
    if(i>=0){ const GodPower *p=god_power(i+1); scpy(b,p->line); *c=C_WHITE; return; }
    int n=-1-i+1; Lv l; sim_level_lv(&l); b[0]=0; *c=C_GREY;
    if(menu_msg[0] && sim_gd_noted==n-1){ scpy(b,menu_msg); *c=C_GOLD; }
    else if(god_unlocked(&l,n,sim_w.god.sandbox)) scpy(b,"OPEN. A: CAST IT");
    else god_locked_text(n,&l,b,52);
}
static int sim_gd_act(int i){
    Lv l; sim_level_lv(&l); int n=i+1, r=sim_cast(&sim_w,&l,n); const GodPower *p=god_power(n); menu_msg[0]=0; sim_gd_noted=i;
    if(r==0){ scpy(menu_msg,"CAST: "); scat(menu_msg,p->name); sfx(6); }
    else if(r==-1){ god_locked_text(n,&l,menu_msg,40); sfx(4); }
    else if(r==-2){ scpy(menu_msg,"NOT ENOUGH SPIRIT. WAIT A WHILE"); sfx(4); }
    return 0;
}
static void sim_gods(volatile u16 *pg){
    Lv l; sim_level_lv(&l); char t[48], d[LV_DEC_MAX]; lv_compact(&l,d,10);
    int open=god_unlocked_count(&l,sim_w.god.sandbox);
    scpy(t,"GOD POWERS "); ncat(t,open); scat(t,"/"); ncat(t,god_count()); scat(t,"  SPIRIT "); ncat(t,sim_w.spirit);
    menu_msg[0]=0; sim_gd_noted=-1; in_menu++;
    sim_rows(pg,t,god_count(),sim_gd_row,sim_gd_foot,"A: CAST  L/R: PAGE  B: BACK",&sim_god_sel,sim_gd_act);
    in_menu--; menu_msg[0]=0;
}

// ---- the quick menu: A on empty ground
static void sim_qm_row(int i, char *b, int *c){
    static const char *const nm[6]={"JOURNAL","PEOPLE","GOD POWERS","SANDBOX","MY LEVEL","COLONY"}; scpy(b,nm[i]); *c=C_GREY;
    if(i==2){ Lv l; sim_level_lv(&l); scat(b,"  "); ncat(b,god_unlocked_count(&l,sim_w.god.sandbox)); scat(b,"/"); ncat(b,god_count()); }
    if(i==3){ scat(b,sim_w.god.sandbox ? ": ON. ALL POWERS OPEN" : ": OFF"); }
}
static void sim_qm_foot(int i, char *b, int *c){
    static const char *const hp[6]={"WHAT HAPPENED HERE, NEWEST FIRST","WHO LIVES HERE AND HOW THEY FEEL","POWERS OPEN BY LEVEL: 1 POWER PER 1,000 LEVELS","PLAY WITH EVERY POWER OPEN. NO LEVEL NEEDED","YOUR LEVEL IN FULL","YOUR SETTLERS. THEY GROW AS YOU PLAY THE ACTION GAME"};
    if(i>=0){ scpy(b,hp[i]); *c=C_GREY; } else { b[0]=0; }
}
static void sim_level_full(void);
// ---- COLONY (Phase 4): the unlock ladder and the settlers
static SimColony *sim_colony(void);
#define SIM_CO_ACT 6
static void sim_colony_map(volatile u16 *pg);
static void sim_co_row(int i, char *b, int *c){
    SimColony *co=sim_colony(); int r=i+1;
    if(r<COLR_N){ int open=(co->mask>>r)&1; scpy(b,open?"+ ":"- "); scat(b,col_rung_name(r)); while(textlen(b)<16) scat(b," "); scat(b,open?"OPEN":col_rung_how(r)); if(!open){ scat(b," "); scat(b,col_rung_goal(r)); } *c=open?C_GREEN:C_DGREY; return; }
    int k=i-(COLR_N-1);
    if(k<SIM_CO_ACT){ static const char *const dn[3]={"CALM","NORMAL","HARSH"};
        *c=C_GOLD;
        if(k==0){ scpy(b,"> OPEN THE COLONY MAP"); }
        else if(k==1){ scpy(b,"> DIFFICULTY: "); scat(b,dn[sim_f.difficulty%3]); }
        else if(k==2){ scpy(b,"> BUILD NEXT: "); scat(b,col_bld_name(co->plan)); }
        else if(k==3){ scpy(b,"> CARRY WOOD AND BUILD"); }
        else if(k==4){ scpy(b,"> BRING 10 FOOD FROM MY BAG"); }
        else { scpy(b,"> STAND GUARD 3 DAYS"); }
        return; }
    k-=SIM_CO_ACT; char nm[16]; int j; col_settler(co,k,nm,&j); scpy(b,"  "); scat(b,nm); while(textlen(b)<14) scat(b," "); scat(b,col_job_name(j)); *c=C_WHITE;
}
static void sim_co_foot(int i, char *b, int *c){
    SimColony *co=sim_colony(); *c=C_GREY;
    if(i>=0){ scpy(b,"FOOD "); ncat(b,co->food); scat(b,"  WOOD "); ncat(b,co->wood); scat(b,"  ORE "); ncat(b,co->ore); scat(b,"  GOODS "); ncat(b,co->goods); *c=C_WHITE; }
    else if(menu_msg[0]){ scpy(b,menu_msg); *c=C_GREEN; }
    else if(co->state==COLS_DEAD_WORLD){ scpy(b,"BURNT FOR GOOD. SETTLE ANOTHER PLANET"); *c=C_RED; }
    else if(co->state==COLS_RUINS){ scpy(b,"RUINS. A: SETTLE AGAIN"); *c=C_RED; }
    else if(co->raid_eta){ scpy(b,"RAID OF "); ncat(b,co->raid_size); scat(b," IN "); ncat(b,co->raid_eta); scat(b," DAYS. DEFENCE "); ncat(b,col_defence(co)); *c=C_RED; }
    else { scpy(b,"SETTLERS "); ncat(b,co->nset); scat(b,"/"); ncat(b,col_cap(co->mask)); scat(b,"  BUILDINGS "); ncat(b,col_built_total(co)); scat(b,"/"); ncat(b,col_buildings(co->mask)); }
}
// Phase 5: A in the colony takes what it made: food for your hunger bar, goods and ore sold for gold, a nap at your bug home (HOUSES)
static int sim_co_act(int i){
    SimColony *co=sim_colony(); ColGift g; menu_msg[0]=0; int k=i-(COLR_N-1);
    if(i>=COLR_N-1){
        if(k>=SIM_CO_ACT){ int s=k-SIM_CO_ACT; int j=co->job[s]==0xFF ? -1 : co->job[s]; j++; if(j>=COLJ_N) j=-1; col_set_job(co,s,j); scpy(menu_msg,"JOB SET"); sfx(2); return 0; }
        int ok=0;
        if(k==0){ sim_colony_map(VISPG); return 0; }
        if(k==1){ sim_f.difficulty=(u8)((sim_f.difficulty+1)%3); xo[XO_SIDEDIF]=(u8)(sim_f.difficulty^XOT[XO_SIDEDIF].def); xo_save(); ok=1; }
        else if(k==2){ int b=co->plan; for(int t=0;t<COLB_N;t++){ b=(b+1)%COLB_N; if(col_set_plan(co,b,-1)){ ok=1; break; } } }
        else if(k==3) ok=col_help_build(co);
        else if(k==4){ if(food>=20){ food-=10; ok=col_carry_food(co,10); } else scpy(menu_msg,"YOUR BAG HAS TOO LITTLE FOOD"); }
        else ok=col_help_guard(co);
        if(co->state==COLS_RUINS) ok=col_resettle(co,(u32)sim_day(&sim_w));
        if(ok){ sfx(2); if(!menu_msg[0]) scpy(menu_msg,"DONE"); } else { sfx(4); if(!menu_msg[0]) scpy(menu_msg,"NOT NOW"); }
        return 0; }
    if(!col_collect(co,100-food,&g)){ scpy(menu_msg,"NOTHING TO TAKE YET. COME BACK LATER"); sfx(4); return 0; }
    if(g.food_pts) eat(g.food_pts); if(g.gold) gold=sadd(gold,(s64)g.gold); if(g.heal){ hearts=maxh; mp=maxmp; }
    scpy(menu_msg,"TAKEN:"); if(g.food_pts){ scat(menu_msg," FOOD +"); ncat(menu_msg,g.food_pts); } if(g.gold){ scat(menu_msg," GOLD +"); ncat(menu_msg,g.gold); } if(g.heal) scat(menu_msg," A NAP");
    sim_col_gifts++; sfx(6); return 0;
}

// ---- the COLONY MAP (the 8 x 6 grid): walk the cursor, pick what to build and where, carry wood to the site (Keflings style). Optional: the colony runs without it.
static void sim_cell_name(const SimColony *co, int cell, char *b){
    int g=co->grid[cell]; if(g) { scpy(b,col_bld_name(g-1)); return; }
    scpy(b, co->state==COLS_DEAD_WORLD ? "BURNT GROUND" : (co->plan_cell==cell ? "BUILDING SITE" : "EMPTY GROUND"));
}
static void sim_colony_map(volatile u16 *pg){
    SimColony *co=sim_colony(); static const char L[COLB_N+1]="HFWSOKM|T$U";
    int cx=0, cy=0, carry=0, tick=0, dirty=1; char msg[44]; msg[0]=0; wait_release(); in_menu++;
    while(1){
        co=sim_colony();
        if(dirty || !(tick&15)){ dirty=0;
            ui_bg(pg,0,160); panel(pg,4,6,232,150,co->state==COLS_ALIVE?"COLONY MAP":"COLONY MAP: NO ONE LIVES HERE");
            for(int y=0;y<COL_GRID_H;y++) for(int x=0;x<COL_GRID_W;x++){ int i=y*COL_GRID_W+x, g=co->grid[i], px=10+x*20, py=24+y*14;
                rect(pg,px,py,19,13,g?(co->state==COLS_DEAD_WORLD?C_DGREY:6):C_DGREY);
                if(!g && co->plan_cell==i && co->state==COLS_ALIVE) rect(pg,px+1,py+1,17,11,C_GOLD);
                if(g){ char t[2]={L[(g-1)<COLB_N?g-1:COLB_N],0}; text(pg,px+7,py+3,t,co->state==COLS_DEAD_WORLD?C_RED:C_WHITE,1); }
                else rect(pg,px+8,py+5,3,3,co->state==COLS_DEAD_WORLD?C_RED:C_GREY); }
            if(co->state==COLS_ALIVE){ int occ[COL_GRID_W*COL_GRID_H], no=0; for(int i=0;i<COL_GRID_W*COL_GRID_H;i++) if(co->grid[i]) occ[no++]=i;
                for(int k=0;k<co->nset && k<COL_MAXSET;k++){ int i=no?occ[(k+(tick>>6))%no]:0; rect(pg,10+(i%COL_GRID_W)*20+2+(k%4)*4,24+(i/COL_GRID_W)*14+9,3,3,C_GREEN); } }
            { int px=10+cx*20, py=24+cy*14; rect(pg,px-1,py-1,21,1,C_CYAN); rect(pg,px-1,py+13,21,1,C_CYAN); rect(pg,px-1,py-1,1,15,C_CYAN); rect(pg,px+19,py-1,1,15,C_CYAN);
              rect(pg,px+5,py-5,9,4,carry?C_GOLD:C_CYAN); }                                  // you: gold while you carry wood
            char b[44]; int cell=cy*COL_GRID_W+cx;
            text(pg,174,26,"BUILD",C_GOLD,1); text(pg,174,35,col_bld_name(co->plan),C_WHITE,1);
            scpy(b,"WOOD "); ncat(b,col_bld_cost(co,co->plan)); text(pg,174,44,b,C_GREY,1);
            scpy(b,"DONE "); ncat(b,co->progress); text(pg,174,53,b,C_GREY,1);
            scpy(b,"STORE "); ncat(b,co->wood); text(pg,174,66,b,C_GREY,1);
            scpy(b,"FOLK "); ncat(b,co->nset); text(pg,174,75,b,C_GREY,1);
            scpy(b,"YOU: "); scat(b,carry?"WOOD":"EMPTY"); text(pg,174,88,b,carry?C_GOLD:C_GREY,1);
            sim_cell_name(co,cell,b); text(pg,10,112,b,C_WHITE,1);
            if(co->state==COLS_DEAD_WORLD) text(pg,10,121,"THIS PLANET IS BURNT FOR GOOD. SETTLE ANOTHER WORLD",C_RED,1);
            else if(co->state==COLS_RUINS) text(pg,10,121,"RUINS. OPEN THE COLONY LIST AND SETTLE AGAIN",C_RED,1);
            else if(co->raid_eta){ scpy(b,"RAID OF "); ncat(b,co->raid_size); scat(b," IN "); ncat(b,co->raid_eta); scat(b," DAYS. DEFENCE "); ncat(b,col_defence(co)); text(pg,10,121,b,C_RED,1); }
            else text(pg,10,121,"A ON A STORE: LIFT WOOD. A ON GROUND: BUILD THERE",C_GREY,1);
            if(msg[0]) text(pg,10,130,msg,C_GREEN,1);
            ctext(pg,120,146,"L/R: BUILDING  A: DO  B: BACK",C_CYAN);
        }
        vsync(); read_keys(); tick++; if(keys!=keys_old) dirty=1;
        if(HIT(K_B)||HIT(K_START)) break;
        if(HIT(K_LEFT)&&cx>0){ cx--; sfx(7); } if(HIT(K_RIGHT)&&cx<COL_GRID_W-1){ cx++; sfx(7); }
        if(HIT(K_UP)&&cy>0){ cy--; sfx(7); } if(HIT(K_DOWN)&&cy<COL_GRID_H-1){ cy++; sfx(7); }
        if(HIT(K_L)||HIT(K_R)){ int d=HIT(K_R)?1:COLB_N-1, b=co->plan; msg[0]=0; for(int t=0;t<COLB_N;t++){ b=(b+d)%COLB_N; if(col_set_plan(co,b,-1)){ break; } } sfx(7); }
        if(HIT(K_A)){ int cell=cy*COL_GRID_W+cx, g=co->grid[cell]; msg[0]=0;
            if(co->state!=COLS_ALIVE){ sfx(4); scpy(msg,"NO ONE IS HERE TO WORK"); }
            else if(!g){ if(carry){ if(col_carry_wood(co,cell)){ carry=0; scpy(msg,"YOU PUT THE WOOD DOWN"); sfx(2); } else { scpy(msg,"NOTHING CAN BE BUILT NOW"); sfx(4); } }
                         else if(col_set_plan(co,co->plan,cell)){ scpy(msg,"SITE CHOSEN. CARRY WOOD HERE"); sfx(2); } else { scpy(msg,"CANNOT BUILD THAT YET"); sfx(4); } }
            else if(g-1==COLB_STORE||g-1==COLB_HUT){ if(carry){ carry=0; scpy(msg,"YOU PUT IT BACK"); } else if(co->wood>0){ carry=1; scpy(msg,"YOU LIFT SOME WOOD"); sfx(2); } else { scpy(msg,"THE STORE HAS NO WOOD"); sfx(4); } }
            else if(g-1==COLB_TOWER||g-1==COLB_WALL){ if(col_help_guard(co)){ scpy(msg,"YOU STAND GUARD: DEFENCE UP"); sfx(2); } }
            else { scpy(msg,"WORKERS USE THIS"); } }
    }
    in_menu--; wait_release();
}
static void sim_colony_screen(volatile u16 *pg){
    SimColony *co=sim_colony();
    if(!(co->mask&(1u<<COLR_TENT))){ char l1[44]; scpy(l1,"LOCKED: "); scat(l1,col_rung_how(COLR_TENT)); const char *ln[3]={l1,"THE COLONY GROWS AS YOU PLAY.","EACH BIG WIN OPENS A NEW PART."}; card(pg,"COLONY",ln,3); return; }
    if(sim_away_new && sim_away_n){ const char *ln[6]; for(int i=0;i<sim_away_n;i++) ln[i]=sim_away[i]; card(pg,"WHILE YOU WERE AWAY",ln,sim_away_n); sim_away_new=0; }
    int sel=0; char t[32]; scpy(t,"COLONY  "); ncat(t,co->nset); scat(t," SETTLERS");
    menu_msg[0]=0; in_menu++; sim_rows(pg,t,COLR_N-1+SIM_CO_ACT+co->nset,sim_co_row,sim_co_foot,"A: TAKE/DO  B: BACK",&sel,sim_co_act); in_menu--; menu_msg[0]=0;
}
static int sim_qm_act(int i){
    if(i==0) sim_journal(VISPG); else if(i==1) sim_people(VISPG); else if(i==2) sim_gods(VISPG);
    else if(i==3){ Lv l; sim_level_lv(&l); sim_f.sandbox=!sim_f.sandbox; sim_set_sandbox(&sim_w,sim_f.sandbox,&l); sim_seen_log=sim_w.log_n; sfx(2); }
    else if(i==4) sim_level_full();
    else sim_colony_screen(VISPG);
    return 0;
}
static void sim_level_full(void){                          // the level in all its digits, in lines of 36
    Lv l; sim_level_lv(&l); static char d[LV_DEC_MAX]; lv_to_dec(&l,d,sizeof d); int len=0; while(d[len]) len++;
    char c[24]; lv_compact(&l,c,12); char l0[44]; scpy(l0,"LEVEL "); scat(l0,c); char l1[44]; scpy(l1,""); ncat(l1,len); scat(l1," DIGITS. BASE 64: "); { uint8_t dg[LV_B64_MAX+1]; ncat(l1,lv_b64_digits(&l,dg)); } scat(l1," DIGITS");
    char l2[40]="", l3[40]=""; if(len<=36){ scpy(l2,d); } else { for(int i=0;i<36;i++){ l2[i]=d[i]; } l2[36]='.'; l2[37]='.'; l2[38]=0; scpy(l3,"(THE REST IS TOO LONG FOR THE SCREEN)"); }
    const char *ln[4]={l0,l1,l2,l3}; card(VISPG,"MY LEVEL",ln,l3[0]?4:3);
}
static int sim_quick(void){
    if(!sim_is_on() || !sim_a_up) return 0;
    sim_a_up=0;
    if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind();
    volatile u16 *pg=VISPG; oam_hide_all(); oam_flush(); in_menu++;
    static int sel=0; sim_rows(pg,"SIDE GAME",6,sim_qm_row,sim_qm_foot,"A: OPEN  B: BACK",&sel,sim_qm_act);
    in_menu--; return 1;
}

static void sim_lab(volatile u16 *pg){                     // PC LAB > SIDE GAME
    if(!sim_is_on()){ const char *ln[3]={"THE SIDE GAME IS OFF.","TURN SIDE GAME ON IN THE OPTIONS","(MORE OPTIONS)."}; card(pg,"SIDE GAME",ln,3); return; }
    if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind();
    static int sel=0; in_menu++; sim_rows(pg,"SIDE GAME",6,sim_qm_row,sim_qm_foot,"A: OPEN  B: BACK",&sel,sim_qm_act); in_menu--;
}

// the ship's COLONY row (GO tab, only with the side game on)
static void sim_ship_colony(volatile u16 *pg){ if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind(); sim_colony_screen(pg); }
