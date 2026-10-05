// ================================================================= SCREENS: character, shops, quest board, title, saving

// what a thing does, in a few words
static const char *const wpre_txt[7]={"","+DAMAGE","++DAMAGE","FIRE DAMAGE","POISON","+25% GOLD","DRINKS HEALTH"};
static const char *const apre_txt[7]={"","+DEFENCE","++DEFENCE","HURTS ATTACKERS","FASTER","HEALS SLOWLY","MORE LIGHT"};
static const char *const suff_txt[8]={"","+HEALTH","+MANA","+DAMAGE","+CRITS","+GOLD","FASTER BITES","STINGS ATTACKERS"};
static void describe(const Gear *g, char *b){
    b[0]=0;
    if(g->rar==R_UNIQUE && is_gear(g)){ scpy(b,"UNIQUE. "); scat(b,uq_desc[g->sub%NUNIQ]); return; }
    switch(g->it){
        case IT_THORN: scpy(b,"DAMAGE "); ncat(b,weapon_base(g)); if(g->sub==3) scat(b,". CURSED"); break;
        case IT_SHELL: scpy(b,"DEFENCE "); ncat(b,armor_base(g)); if(g->sub==3) scat(b,". STICKY"); break;
        case IT_CHARM: scpy(b,"A LUCKY CHARM"); break;
        case IT_BERRY: scpy(b, berry_known[g->sub] ? "YOU KNOW THIS BERRY" : "WHAT DOES IT DO? TRY IT"); break;
        case IT_SEED: scpy(b, seed_known[g->sub] ? "YOU KNOW THIS SEED" : "WHAT DOES IT DO? TRY IT"); break;
        case IT_CRUMB: scpy(b,"FOOD +40"); break;
        case IT_POTION: scpy(b, g->sub ? "FILLS YOUR MANA" : "GIVES HEALTH BACK"); break;
        case IT_WARP: scpy(b,"PORTAL OUT. COME BACK LATER"); break;
        case IT_GEM: scpy(b,gem_use[g->sub&3]); scat(b," OR SELL IT"); break;
        case IT_BOOK: scpy(b,"TEACHES A SPELL"); break;
        case IT_FISH: scpy(b, g->sub==0 ? "FOOD +40" : g->sub==4 ? "WORTH A LOT OF GOLD" : "FEED IT TO BUDDY"); break;
    }
    if(g->it==IT_THORN||g->it==IT_SHELL||g->it==IT_CHARM){
        const char *const *pt = g->it==IT_SHELL ? apre_txt : wpre_txt;
        if(g->pre){ scat(b,". "); scat(b,pt[g->pre]); }
        if(g->suf){ scat(b,". "); scat(b,suff_txt[g->suf]); }
    }
}
static void box(volatile u16 *pg, int x, int y, int w, int h){ rect(pg,x,y,w,h,0); rect(pg,x,y,w,1,6); rect(pg,x,y+h-1,w,1,6); rect(pg,x,y,1,h,6); rect(pg,x+w-1,y,1,h,6); }

// ---------------------------------------------------------------- the character screen (START): four pages
static int send_pet_to_sell(void){
    if(pet_away_t||pet_down_t){ say("BUDDY ISN'T HERE"); return 0; }
    int n=0, g=0;
    for(int i=0;i<bag_n;){ int it=bag[i].it; if(((it==IT_THORN||it==IT_SHELL||it==IT_CHARM) && bag[i].rar!=R_UNIQUE)||it==IT_GEM||(it==IT_FISH&&bag[i].sub==4)){ g+=item_value(&bag[i]); n++; drop_at(i); } else i++; }
    if(!n){ say("NOTHING FOR BUDDY TO SELL"); return 0; }
    pet_sold_n=n; pet_sold_g=g; pet_away_t=900; say("BUDDY RUNS OFF TO SELL YOUR GEAR"); return 1;
}
// ---------------------------------------------------------------- the SYSTEM page: save, load, options, quit
static char sys_msg[40]; static int sys_col=C_GREEN;
static void sys_say(const char *t, int c){ scpy(sys_msg,t); sys_col=c; }
static void sys_action(volatile u16 *pg, int cur){
    char b[40];
    if(cur==0 || cur==1){
        if(scene!=0){ sys_say("YOU CAN ONLY SAVE ON THE SURFACE",C_RED); sfx(0); return; }
        if(cur==1){ int t=slot_picker(pg,SP_SAVE,0); if(t<0) return; cur_slot=t; }
        do_save(); scpy(b,"SAVED TO SLOT "); ncat(b,cur_slot+1); sys_say(b,C_GREEN); sfx(6);
    }
    else if(cur==2){ int t=slot_picker(pg,SP_LOAD,0); if(t>=0){ cur_slot=t; sys_req=1; } }
    else if(cur==3) options_screen(pg);
    else if(confirm(pg,"QUIT TO THE TITLE?","UNSAVED PROGRESS IS LOST")) sys_req=2;
}

static void char_screen(volatile u16 *pg){
    int page=0, cur=0, dirty=1; sys_msg[0]=0;
    static const char *const tabs[5]={"BUG","GEAR","MAGIC","QUEST","SYSTEM"};
    static const char *const stn[4]={"STRENGTH","DEXTERITY","VITALITY","MAGIC"};
    static const char *const std[4]={"BITE DAMAGE","CRITS, FASTER BITES","HEALTH, DEFENCE","MANA, SPELL POWER"};
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64];
            rect(pg,0,18,240,142,0);
            for(int t=0;t<5;t++) text(pg,8+t*46,21,tabs[t], t==page?C_GOLD:C_DGREY, 1);
            rect(pg,8+page*46,28,textlen(tabs[page])*4,1,C_GOLD);
            int y=34;
            if(page==0){
                scpy(b,"LEVEL "); ncat(b,level); scat(b,"   XP "); ncat(b,xp); scat(b,"/"); ncat(b,xp_next()); text(pg,8,y,b,C_WHITE,1); y+=8;
                scpy(b,"FAME "); ncat(b,fame); scat(b,"  "); scat(b,fame_rank()); text(pg,8,y,b,C_GOLD,1); y+=11;
                int *sv[4]={&st_str,&st_dex,&st_vit,&st_mag};
                for(int i=0;i<4;i++){ if(cur==i) text(pg,4,y,">",C_CYAN,1); text(pg,10,y,stn[i],cur==i?C_WHITE:C_GREY,1); b[0]=0; ncat(b,*sv[i]); text(pg,58,y,b,C_WHITE,1); text(pg,76,y,std[i],C_DGREY,1); y+=8; }
                scpy(b,"POINTS TO SPEND: "); ncat(b,stat_pts); text(pg,10,y,b,stat_pts?C_GREEN:C_DGREY,1); if(stat_pts) text(pg,120,y,"A: ADD A POINT",C_GREEN,1); y+=11;
                scpy(b,"DAMAGE "); ncat(b,d_dmg+d_fire+d_venom); scat(b,"  DEFENCE "); ncat(b,d_def); scat(b,"  CRIT "); ncat(b,d_crit); scat(b,"%  GOLD "); ncat(b,gold_find); scat(b,"%"); text(pg,8,y,b,C_WHITE,1); y+=8;
                scpy(b,"HEALTH "); ncat(b,hearts); scat(b,"/"); ncat(b,maxh); scat(b,"  MANA "); ncat(b,mp); scat(b,"/"); ncat(b,maxmp); scat(b,"  FOOD "); ncat(b,food); text(pg,8,y,b,C_WHITE,1); y+=11;
                scpy(b,"PET: "); scat(b,pet_form_name[pet_form]); scat(b,"  HP "); ncat(b,pet_hp); scat(b,"/"); ncat(b,pet_maxhp*(pet_form==2?2:1));
                if(pet_away_t) scat(b,"  (AWAY SELLING)"); else if(pet_down_t) scat(b,"  (RESTING)");
                text(pg,8,y,b,C_CYAN,1); y+=11;
                text(pg,8,y,"BUG FRIENDS",C_GOLD,1); y+=8; int any=0, x=8;
                for(int t=0;t<6;t++) if(have_bug[t]){ any=1; scpy(b,bug_name[t]); scat(b," X"); ncat(b,have_bug[t]); text(pg,x,y,b,C_WHITE,1); x+=76; if(x>200){ x=8; y+=7; } }
                if(!any) text(pg,8,y,"NONE YET. OPEN CAGES TO FREE BUGS",C_DGREY,1);
            } else if(page==1){
                const Gear *eq[3]={&eqw,&eqa,&eqc}; static const char *const lab[3]={"BITE ","WEAR ","CHARM"};
                for(int i=0;i<3;i++){ text(pg,8,y,lab[i],C_GOLD,1); if(eq[i]->it){ gear_name(eq[i],b); b[44]=0; text(pg,32,y,b,rar_color(eq[i]),1); } else text(pg,32,y,"NOTHING",C_DGREY,1); y+=7; }
                y+=4; text(pg,8,y,"YOUR BAG",C_GOLD,1); y+=8;
                for(int i=0;i<bag_n;i++){ if(cur==i) text(pg,4,y,">",C_CYAN,1); gear_name(&bag[i],b); b[44]=0; text(pg,10,y,b,rar_color(&bag[i]),1); y+=7; }
                if(!bag_n) text(pg,10,y,"EMPTY",C_DGREY,1);
                if(bag_n){ describe(&bag[cur],b); b[56]=0; text(pg,8,139,b,C_WHITE,1); scpy(b,"WORTH "); ncat(b,item_value(&bag[cur])); scat(b,"G");
                    if(is_gear(&bag[cur])){ scat(b,"  SCRAP "); ncat(b,scrap_of(bag[cur].rar)); scat(b,"  KEEP +"); ncat(b,keep_points(bag[cur].rar)); }
                    text(pg,236-textlen(b)*4,131,b,C_GOLD,1); }
                text(pg,8,147, bag_n && is_gear(&bag[cur]) ? "A: WEAR  B: SCRAP IT  SELECT: BUDDY SELLS GEAR" : "A: USE  B: DROP  SELECT: BUDDY SELLS GEAR",C_CYAN,1);
            } else if(page==2){
                scpy(b,"MANA "); ncat(b,mp); scat(b,"/"); ncat(b,maxmp); scat(b,"   SPELL POWER "); ncat(b,st_mag); text(pg,8,y,b,C_CYAN,1); y+=12;
                static const char *const sd[NSPELL]={"A BALL OF FIRE FLIES AHEAD","LIGHTNING HITS THE NEAREST FOE","HEALS YOU","HARDER TO HURT FOR A WHILE"};
                for(int i=0;i<NSPELL;i++){ if(cur==i) text(pg,4,y,">",C_CYAN,1);
                    scpy(b,spell_name[i]); if(i==spell_sel) scat(b," *"); text(pg,10,y,b, spell_known[i] ? (i==spell_sel?C_GOLD:C_WHITE) : C_DGREY,1);
                    b[0]=0; ncat(b,spell_cost[i]); scat(b," MP"); text(pg,76,y,b,C_CYAN,1);
                    text(pg,104,y, spell_known[i]?sd[i]:"FIND ITS BOOK",C_DGREY,1); y+=9; }
                y+=6; text(pg,8,y,"IN A DUNGEON: TAP SELECT TO CAST.",C_WHITE,1); y+=8;
                text(pg,8,y,"HOLD SELECT + L OR R TO CHANGE SPELL.",C_WHITE,1); y+=8;
                text(pg,8,147,"A: MAKE IT YOUR SPELL",C_CYAN,1);
            } else if(page==3){
                text(pg,8,y,"QUEST",C_GOLD,1); y+=8;
                if(q_type==Q_NONE) text(pg,8,y,"NONE. VISIT YOUR SHIP'S QUEST BOARD.",C_DGREY,1);
                else { text(pg,8,y,q_name,C_WHITE,1); y+=7; scpy(b, q_done ? "DONE! GO BACK TO YOUR SHIP" : "IN PROGRESS"); if(!q_done && (q_type==Q_HUNT||q_type==Q_RESCUE)){ scat(b," "); ncat(b,q_have); scat(b,"/"); ncat(b,q_need); } text(pg,8,y,b,q_done?C_GREEN:C_WHITE,1); }
                y+=12; scpy(b,"FAME "); ncat(b,fame); scat(b,"  RANK: "); scat(b,fame_rank()); text(pg,8,y,b,C_GOLD,1); y+=12;
                if(endless){ scpy(b,"ENDLESS GALAXY  WORLD "); ncat(b,world_no); scat(b,"  DEEPEST B"); ncat(b,best_depth); text(pg,8,y,b,C_CYAN,1); y+=10; }
                text(pg,8,y,"WHAT YOU HAVE LEARNED",C_GOLD,1); y+=8;
                for(int i=0;i<8;i++){ scpy(b,berry_col[i]); scat(b,":"); scat(b, berry_known[i]?berry_eff[berry_map[i]]:"?"); text(pg,8+(i&3)*58,y+(i>>2)*7,b,berry_known[i]?C_WHITE:C_DGREY,1); }
                y+=16;
                for(int i=0;i<6;i++){ scpy(b,seed_look[i]); scat(b,":"); scat(b, seed_known[i]?seed_eff[seed_map[i]]:"?"); text(pg,8+(i%3)*76,y+(i/3)*7,b,seed_known[i]?C_WHITE:C_DGREY,1); }
            } else {
                static const char *const sn[5]={"SAVE GAME","SAVE TO ANOTHER SLOT","LOAD GAME","OPTIONS","QUIT TO TITLE"};
                scpy(b,"SLOT "); ncat(b,cur_slot+1); text(pg,8,y,b,C_CYAN,1);
                fmt_time(b,play_secs()); scat(b," PLAYED"); text(pg,60,y,b,C_GREY,1);
                text(pg,170,y,opt_autosave?"AUTOSAVE ON":"AUTOSAVE OFF",opt_autosave?C_GREEN:C_DGREY,1); y+=14;
                for(int i=0;i<5;i++){
                    int ok = !(i<2 && scene!=0);
                    if(cur==i) rect(pg,4,y-2,232,10,6);
                    text(pg,10,y,sn[i],!ok?C_DGREY: cur==i?C_WHITE:C_GREY,1);
                    if(i==0){ scpy(b,"-> SLOT "); ncat(b,cur_slot+1); text(pg,150,y,b,ok?C_CYAN:C_DGREY,1); }
                    y+=12;
                }
                if(scene!=0) text(pg,8,y+4,"SAVING IS ONLY POSSIBLE ON THE SURFACE",C_DGREY,1);
                if(sys_msg[0]) text(pg,8,134,sys_msg,sys_col,1);
                text(pg,8,147,"A: CHOOSE",C_CYAN,1);
            }
            text(pg,148,154,"L/R: PAGE  START: BACK",C_DGREY,1);
        }
        vsync(); read_keys();
        if(HIT(K_START)) break;
        if(HIT(K_L)){ page=(page+4)%5; cur=0; dirty=1; sys_msg[0]=0; }
        if(HIT(K_R)){ page=(page+1)%5; cur=0; dirty=1; sys_msg[0]=0; }
        int nmax = page==0?4 : page==1?bag_n : page==2?NSPELL : page==4?5 : 0;
        if(nmax){ if(HIT(K_UP)){ cur=(cur+nmax-1)%nmax; dirty=1; } if(HIT(K_DOWN)){ cur=(cur+1)%nmax; dirty=1; } }
        if(HIT(K_A)){
            dirty=1;
            if(page==0 && stat_pts){ int *sv[4]={&st_str,&st_dex,&st_vit,&st_mag}; (*sv[cur])++; stat_pts--; int oldmax=maxh; recalc(); hearts+=maxh-oldmax; sfx(2); }
            if(page==1 && bag_n){ use_index(cur); if(cur>=bag_n && cur) cur--; }
            if(page==2 && spell_known[cur]) spell_sel=cur;
            if(page==4){ sys_action(pg,cur); if(sys_req) break; }
        }
        if(HIT(K_B) && page==1 && bag_n){
            if(is_gear(&bag[cur])){ int n=scrap_of(bag[cur].rar); scrap+=n; char m[32]; scpy(m,"SCRAPPED: +"); ncat(m,n); scat(m," TECH SCRAP"); drop_at(cur); say(m); sfx(13); }
            else drop_at(cur);
            if(cur>=bag_n && cur) cur--; dirty=1; }
        if(HIT(K_SEL) && page==1){ send_pet_to_sell(); dirty=1; }
    }
    wait_release();
}

// ---------------------------------------------------------------- the ship: shops, quest board
EW static Gear stock[6];
static void restock(void){ int il=dlv()*3+level; for(int i=0;i<6;i++) stock[i]=roll_gear(i<2?IT_THORN:i<4?IT_SHELL:IT_CHARM, il, 10); }
static void make_quest(void){
    q_type=1+rndi(4); q_have=0; q_done=0; q_zone=zone; q_world=world_no;
    int maxf = endless ? (best_depth>3?best_depth:3) : zone_floors();
    q_floor = 1+rndi(maxf);
    q_gold = 60 + dlv()*40 + q_floor*10; q_fame = 15 + dlv()*6;
    static const char *const cn[6]={"GRIMFANG","MOLDJAW","SKITTERMAW","VILEWING","THE PALE ONE","OLD CRUNCH"};
    char b[40];
    switch(q_type){
        case Q_HUNT: q_need=8+dlv()*2+rndi(6); scpy(b,"SLAY "); ncat(b,q_need); scat(b," MONSTERS"); break;
        case Q_CHAMP: scpy(b,"SLAY "); scat(b,cn[rndi(6)]); scat(b," ON B"); ncat(b,q_floor); q_gold+=60; q_fame+=10; break;
        case Q_FETCH: scpy(b,"FIND THE LOST TREASURE ON B"); ncat(b,q_floor); break;
        default: q_need=2+rndi(2); scpy(b,"FREE "); ncat(b,q_need); scat(b," CAGED BUGS"); break;
    }
    b[23]=0; scpy(q_name,b);
}
static int list_pick(volatile u16 *pg, const char *title, int n, void (*row)(int,char*,int*), int *sel, const char *help){
    int dirty=1;
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64]; int c;
            box(pg,12,20,216,136); text(pg,20,24,title,C_GOLD,1);
            char g[16]="G"; ncat(g,gold); text(pg,190,24,g,C_GOLD,1);
            for(int i=0;i<n;i++){ c=C_GREY; row(i,b,&c); b[48]=0; if(i==*sel){ text(pg,16,34+i*8,">",C_CYAN,1); if(c==C_GREY) c=C_WHITE; } text(pg,22,34+i*8,b,c,1); }
            text(pg,20,148,help,C_CYAN,1);
        }
        vsync(); read_keys();
        if(keys!=keys_old) dirty=1;
        if(n){ if(HIT(K_UP)) *sel=(*sel+n-1)%n; if(HIT(K_DOWN)) *sel=(*sel+1)%n; }
        if(HIT(K_B)) return -1;
        if(HIT(K_A)) return *sel;
    }
}
static const char *const sup_name[6]={"CRUMB","HEALTH POTION","MANA POTION","MYSTERY BERRY","MYSTERY SEED","WARP CRYSTAL"};
static const u8 sup_cost[6]={10,25,25,15,20,120};
static void sup_row(int i, char *b, int *c){ scpy(b,sup_name[i]); while(textlen(b)<16) scat(b," "); ncat(b,sup_cost[i]); scat(b,"G"); }
static void stock_row(int i, char *b, int *c){ if(!stock[i].it){ scpy(b,"SOLD"); *c=C_DGREY; return; } gear_name(&stock[i],b); b[34]=0; while(textlen(b)<35) scat(b," "); ncat(b,item_value(&stock[i])*2); scat(b,"G"); *c=rar_color(&stock[i]); }
static void sell_row(int i, char *b, int *c){ gear_name(&bag[i],b); b[34]=0; while(textlen(b)<35) scat(b," "); ncat(b,item_value(&bag[i])); scat(b,"G"); *c=rar_color(&bag[i]); }
// ---------------------------------------------------------------- the trophy case: keep it forever, or scrap it for tech
static void trophy_case(volatile u16 *pg){
    int sel=0, dirty=1; wait_release();
    while(1){
        int gi[BAGMAX], ng=0; for(int i=0;i<bag_n;i++) if(is_gear(&bag[i])) gi[ng++]=i;
        int uq[NUNIQ], nu=0; for(int k=0;k<NUNIQ;k++) if(tr_uq>>k&1) uq[nu++]=k;
        int n=ng+nu; if(sel>=n) sel=n?n-1:0;
        if(dirty){ dirty=0; char b[64];
            box(pg,12,20,216,136); text(pg,20,24,"TROPHY CASE",C_GOLD,1);
            scpy(b,"SCRAP "); ncat(b,scrap); text(pg,236-24-textlen(b)*4,24,b,C_CYAN,1);
            int tp=trophy_points(); scpy(b,"POINTS "); ncat(b,tp); scat(b,":  +"); ncat(b,tp/15); scat(b," DAMAGE  +"); ncat(b,tp/5); scat(b,"% GOLD  +"); ncat(b,tp/10); scat(b," HP");
            text(pg,20,34,b,C_WHITE,1);
            scpy(b,"KEPT "); ncat(b,tr_n[0]); scat(b," COMMON  "); ncat(b,tr_n[1]); scat(b," MAGIC  "); ncat(b,tr_n[2]); scat(b," RARE  "); ncat(b,tr_n[3]); scat(b," UNIQUE"); text(pg,20,42,b,C_GREY,1);
            int y=54;
            if(!n) text(pg,22,y,"NO GEAR IN YOUR BAG",C_DGREY,1);
            for(int i=0;i<n && y<136;i++, y+=8){
                if(i<ng){ Gear *g=&bag[gi[i]]; gear_name(g,b); b[30]=0; text(pg,22,y,b,rar_color(g),1);
                    scpy(b,"KEEP +"); ncat(b,keep_points(g->rar)); scat(b,"  SCRAP "); ncat(b,scrap_of(g->rar)); text(pg,236-24-textlen(b)*4,y,b,C_DGREY,1); }
                else { scpy(b,"IN CASE: "); scat(b,uq_name[uq[i-ng]]); text(pg,22,y,b,C_UNIQ,1); text(pg,180,y,"TAKE OUT",C_DGREY,1); }
                if(i==sel) text(pg,16,y,">",C_CYAN,1); }
            text(pg,20,140, sel<ng ? "A: KEEP IT FOREVER  B: SCRAP IT  START: BACK" : "A: TAKE IT OUT TO USE  START: BACK",C_CYAN,1);
            text(pg,20,148,"KEPT THINGS ARE SAFE, EVEN IF YOU FAINT",C_DGREY,1);
        }
        vsync(); read_keys(); if(keys!=keys_old) dirty=1;
        if(n){ if(HIT(K_UP)) sel=(sel+n-1)%n; if(HIT(K_DOWN)) sel=(sel+1)%n; }
        if(HIT(K_START) || (HIT(K_B) && sel>=ng)) break;
        if(HIT(K_A) && sel<ng){ Gear *g=&bag[gi[sel]]; int r=g->rar&3;
            if(r==R_UNIQUE && (tr_uq>>(g->sub%NUNIQ)&1)){ say("ONE OF THOSE IS ALREADY IN THE CASE"); continue; }
            tr_n[r]++; if(r==R_UNIQUE) tr_uq|=1<<(g->sub%NUNIQ); drop_at(gi[sel]); recalc(); sfx(6); say(r>=R_RARE?"A FINE TROPHY!":"KEPT IN THE CASE"); }
        else if(HIT(K_A) && sel>=ng){ int k=uq[sel-ng]; Gear g={uq_it[k],(u8)k,R_UNIQUE,0,0,(u8)(5+level>99?99:5+level)};
            if(add_gear(g)){ tr_uq&=~(1<<k); tr_n[3]--; recalc(); sfx(2); } }
        else if(HIT(K_B) && sel<ng){ Gear *g=&bag[gi[sel]]; int v=scrap_of(g->rar); scrap+=v; drop_at(gi[sel]); sfx(13); char m[32]; scpy(m,"+"); ncat(m,v); scat(m," TECH SCRAP"); say(m); }
    }
    wait_release(); hud_now();
}
static int shop_menu(void){   // returns 1 to fly on
    volatile u16 *pg=VRAM;
    oam_hide_all(); oam_flush();
    int sel=0;
    while(1){
        const char *main_opt[9]={ "LAUNCH INTO SPACE","SUPPLIES","GEAR SHOP","SELL","QUEST BOARD","HEAL YOU AND BUDDY  25G","IDENTIFY BERRY/SEED 30G","TROPHY CASE: KEEP OR SCRAP","LEAVE"};
        int can_fly = 1;
        int dirty=1; wait_release();
        while(1){
            if(dirty){ dirty=0; box(pg,12,20,216,136); text(pg,20,24,"YOUR SHIP",C_GOLD,1); char g[16]="G"; ncat(g,gold); text(pg,190,24,g,C_GOLD,1);
                for(int i=0;i<9;i++){ int c=(i==0&&!can_fly)?C_DGREY:(i==sel?C_WHITE:C_GREY); if(i==sel) text(pg,16,36+i*10,">",C_CYAN,1); text(pg,22,36+i*10,main_opt[i],c,1); }
                if(q_type!=Q_NONE && q_done) text(pg,120,76,"! QUEST DONE",C_GREEN,1);
                if(!can_fly) text(pg,20,124,"BEAT THE BOSS BELOW TO FLY ON",C_DGREY,1);
                text(pg,20,148,"A: CHOOSE  B: LEAVE",C_CYAN,1); }
            vsync(); read_keys(); if(keys!=keys_old) dirty=1;
            if(HIT(K_UP)) sel=(sel+8)%9; if(HIT(K_DOWN)) sel=(sel+1)%9;
            if(HIT(K_B)){ wait_release(); return 0; }
            if(HIT(K_A)) break;
        }
        if(sel==8){ wait_release(); return 0; }
        if(sel==7){ trophy_case(pg); continue; }
        if(sel==0){ if(can_fly){ wait_release(); return 1; } continue; }
        if(sel==1){ int s2=0; int r; while((r=list_pick(pg,"SUPPLIES",6,sup_row,&s2,"A: BUY  B: BACK"))>=0){
                if(gold<sup_cost[r]){ say("NOT ENOUGH GOLD"); continue; }
                Gear gg={0,0,0,0,0,0};
                if(r==0) gg.it=IT_CRUMB; else if(r<3){ gg.it=IT_POTION; gg.sub=r-1; } else if(r==3){ gg.it=IT_BERRY; gg.sub=rndi(8); } else if(r==4){ gg.it=IT_SEED; gg.sub=rndi(6); } else gg.it=IT_WARP;
                if(add_gear(gg)){ gold-=sup_cost[r]; hud_now(); sfx(7); } } }
        if(sel==2){ int s2=0; int r; while((r=list_pick(pg,"GEAR SHOP",6,stock_row,&s2,"A: BUY  B: BACK"))>=0){
                if(!stock[r].it) continue; int cost=item_value(&stock[r])*2; if(gold<cost){ say("NOT ENOUGH GOLD"); continue; }
                if(add_gear(stock[r])){ gold-=cost; stock[r].it=0; hud_now(); sfx(7); } } }
        if(sel==3){ int s2=0; int r; while(bag_n && (r=list_pick(pg,"SELL",bag_n,sell_row,&s2,"A: SELL  B: BACK"))>=0){
                gold+=item_value(&bag[r]); drop_at(r); if(s2>=bag_n && s2) s2--; hud_now(); sfx(7); } }
        if(sel==4){
            char b[48]; int dirty2=1; wait_release();
            if(q_type==Q_NONE) make_quest(), q_type=-q_type;     // an offer, not taken yet
            while(1){
                if(dirty2){ dirty2=0; box(pg,12,20,216,136); text(pg,20,24,"QUEST BOARD",C_GOLD,1);
                    int t = q_type<0 ? -q_type : q_type;
                    text(pg,20,40,q_name,C_WHITE,1);
                    scpy(b,"REWARD "); ncat(b,q_gold); scat(b,"G, "); ncat(b,q_fame); scat(b," FAME, A PRIZE"); text(pg,20,50,b,C_GOLD,1);
                    if(q_type<0) text(pg,20,70,"A: TAKE THIS QUEST   B: NOT NOW",C_CYAN,1);
                    else if(q_done) text(pg,20,70,"DONE! A: CLAIM YOUR REWARD",C_GREEN,1);
                    else { scpy(b,"IN PROGRESS"); if(t==Q_HUNT||t==Q_RESCUE){ scat(b," "); ncat(b,q_have); scat(b,"/"); ncat(b,q_need); } text(pg,20,70,b,C_WHITE,1); text(pg,20,80,"A: GIVE UP   B: BACK",C_CYAN,1); }
                    scpy(b,"YOUR FAME: "); ncat(b,fame); scat(b,"  "); scat(b,fame_rank()); text(pg,20,140,b,C_GOLD,1); }
                vsync(); read_keys(); if(keys!=keys_old) dirty2=1;
                if(HIT(K_B)){ if(q_type<0) q_type=Q_NONE; break; }
                if(HIT(K_A)){
                    if(q_type<0){ q_type=-q_type; say("QUEST TAKEN"); break; }
                    if(q_done){ gold+=q_gold; fame+=q_fame; Gear pz=roll_gear(rndi(3)==0?IT_CHARM:rndi(2)?IT_THORN:IT_SHELL, dlv()*3+level+2, 30); add_gear(pz); q_type=Q_NONE; say("REWARD CLAIMED!"); sfx(6); hud_now(); break; }
                    q_type=Q_NONE; say("QUEST DROPPED"); break;
                }
            }
        }
        if(sel==5){ if(gold<25) say("NOT ENOUGH GOLD"); else { gold-=25; hearts=maxh; mp=maxmp; pet_hp=pet_maxhp; pet_down_t=0; say("YOU AND BUDDY FEEL GREAT"); hud_now(); sfx(6); } }
        if(sel==6){ if(gold<30){ say("NOT ENOUGH GOLD"); continue; } int ok=0;
            for(int k=0;k<bag_n && !ok;k++){ if(bag[k].it==IT_BERRY && !berry_known[bag[k].sub]){ berry_known[bag[k].sub]=1; ok=1; } else if(bag[k].it==IT_SEED && !seed_known[bag[k].sub]){ seed_known[bag[k].sub]=1; ok=1; } }
            if(ok){ gold-=30; say("IDENTIFIED"); hud_now(); } else say("NOTHING TO IDENTIFY"); }
    }
}
