// sim_ui.inc: the side game's screens: clock chip, who-is-this card, the quick menu, JOURNAL, PEOPLE and GOD POWERS.
// (Included by sim_glue.inc. All text is capital letters, short lines, like the rest of the game.)

static void sim_when(const SimEvent *e, char *b){ int day=(int)(e->tick/SIM_DAY_TICKS), hr=(int)((e->tick%SIM_DAY_TICKS)*24/SIM_DAY_TICKS); b[0]=0; scat(b,"D"); ncat(b,day+1); scat(b," "); if(hr<10) scat(b,"0"); ncat(b,hr); scat(b,"H"); }

static void sim_hud(volatile u16 *pg){
    if(!sim_is_on() || !sim_have_w || FILM || kr_mode) return;
    int hr=sim_hour(&sim_w); char t[24]; t[0]=0; scat(t,"D"); ncat(t,sim_day(&sim_w)+1); scat(t," "); if(hr<10) scat(t,"0"); ncat(t,hr); scat(t,"H ");
    scat(t,sim_weather_name(sim_w.weather));
    int day = hr>=6 && hr<19;
    rect(pg,3,151,4,4,day?C_GOLD:C_CYAN); stext(pg,10,150,t,C_WHITE);
}

static int sim_talk(void *po_obj){
    if(!sim_is_on() || !sim_have_w) return 0;
    int idx=(int)((PObj*)po_obj-po), k=-1; for(int i=0;i<sim_w.nfolk;i++) if(sim_fpo[i]==idx) k=i;
    if(k<0) return 0;
    const SimFolk *f=&sim_w.folk[k]; char l0[44], l1[44], l2[44];
    scpy(l0,sim_job_name(f->job)); scat(l0,". "); scat(l0,sim_task_name(f->task)); scat(l0,".");
    sim_status_text(f,l1,sizeof l1);
    scpy(l2,"IT IS "); { int hr=sim_hour(&sim_w); if(hr<10) scat(l2,"0"); ncat(l2,hr); } scat(l2,"H. "); scat(l2,sim_weather_name(sim_w.weather)); scat(l2,".");
    const char *ln[3]={l0,l1,l2}; card(VISPG,f->name,ln,3);
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
    static const char *const nm[5]={"JOURNAL","PEOPLE","GOD POWERS","SANDBOX","MY LEVEL"}; scpy(b,nm[i]); *c=C_GREY;
    if(i==2){ Lv l; sim_level_lv(&l); scat(b,"  "); ncat(b,god_unlocked_count(&l,sim_w.god.sandbox)); scat(b,"/"); ncat(b,god_count()); }
    if(i==3){ scat(b,sim_w.god.sandbox ? ": ON. ALL POWERS OPEN" : ": OFF"); }
}
static void sim_qm_foot(int i, char *b, int *c){
    static const char *const hp[5]={"WHAT HAPPENED HERE, NEWEST FIRST","WHO LIVES HERE AND HOW THEY FEEL","POWERS OPEN BY LEVEL: 1 POWER PER 1,000 LEVELS","PLAY WITH EVERY POWER OPEN. NO LEVEL NEEDED","YOUR LEVEL IN FULL"};
    if(i>=0){ scpy(b,hp[i]); *c=C_GREY; } else { b[0]=0; }
}
static void sim_level_full(void);
static int sim_qm_act(int i){
    if(i==0) sim_journal(VISPG); else if(i==1) sim_people(VISPG); else if(i==2) sim_gods(VISPG);
    else if(i==3){ Lv l; sim_level_lv(&l); sim_f.sandbox=!sim_f.sandbox; sim_set_sandbox(&sim_w,sim_f.sandbox,&l); sim_seen_log=sim_w.log_n; sfx(2); }
    else { sim_level_full(); }
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
    static int sel=0; sim_rows(pg,"SIDE GAME",5,sim_qm_row,sim_qm_foot,"A: OPEN  B: BACK",&sel,sim_qm_act);
    in_menu--; return 1;
}

static void sim_lab(volatile u16 *pg){                     // PC LAB > SIDE GAME
    if(!sim_is_on()){ const char *ln[3]={"THE SIDE GAME IS OFF.","TURN SIDE GAME ON IN THE OPTIONS","(MORE OPTIONS)."}; card(pg,"SIDE GAME",ln,3); return; }
    if(!sim_have_w || sim_world_seed()!=sim_w.seed) sim_bind();
    static int sel=0; in_menu++; sim_rows(pg,"SIDE GAME",5,sim_qm_row,sim_qm_foot,"A: OPEN  B: BACK",&sel,sim_qm_act); in_menu--;
}
