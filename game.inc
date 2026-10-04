// ================================================================= ZONES, BUGS, ITEMS
// zone 0..5: The Garden .. The Anthill. Zone z has z+3 dungeon floors; the last one has a boss.
static int zone=0, zone_cleared=0, maxh=5, food=100, bugs_freed=0, scene=0;
static int level, xp, atk_bonus;
static void scpy(char *d, const char *s); static void scat(char *d, const char *s); static void gain_xp(int n);
static int have_bug[6];
enum { BUG_PILL, BUG_LADY, BUG_FIRE, BUG_BEE, BUG_CRICKET, BUG_ANT };
static const char *const bug_name[6]={"PILL BUG","LADYBUG","FIREFLY","BEE","CRICKET","RUNAWAY ANT"};
static const char *const bug_power[6]={"+10 GOLD","+1 HEART","LIGHTS THE DARK","STINGS ATTACKERS","FASTER LEGS","POINTS TO STAIRS"};
// ---- endless galaxy (after the Ant King)
static int endless=0, world_no=0, loot=0, best_depth=0; static u32 galaxy_seed=1;
static int dlv(void){ int d = endless ? 6 + world_no/2 : zone; return d>30?30:d; }
static int zone_floors(void){ return endless ? 9999 : zone+3; }
static void earn(int g){ if(endless && scene==1) loot+=g; else gold+=g; }
static u16 w_tint[6], w_mon[8]; static u8 w_styles[3]; static char w_name[24];
static u32 wseed;
static int wr(int n){ wseed = wseed*1103515245u + 12345u; return (int)((wseed>>16)%(u32)n); }
static u16 rgb15(int r,int g,int b){ if(r>255)r=255; if(g>255)g=255; if(b>255)b=255; return (r>>3)|((g>>3)<<5)|((b>>3)<<10); }
static void make_world(void){
    static const char *const syl[16]={"ZA","KOR","MI","LUX","PO","BRA","NU","TIK","VO","RAN","SPI","GLO","QUA","FEN","DOR","ULM"};
    static const char *const suf[6]={""," PRIME"," II"," MAJOR"," MINOR"," X"};
    wseed = galaxy_seed ^ ((u32)world_no*2654435761u);
    w_name[0]=0; int n=2+wr(2); for(int i=0;i<n;i++) scat(w_name,syl[wr(16)]); scat(w_name,suf[wr(6)]);
    int r=40+wr(216), g=40+wr(216), b=40+wr(216); int k=wr(3); if(k==0) r=230; else if(k==1) g=230; else b=230;
    static const u8 mul[5]={8,30,55,80,100};
    for(int i=0;i<5;i++){ int m=mul[i]; int w = i==4 ? 120 : (i==3?30:0); w_tint[i]=rgb15(r*m/100+w, g*m/100+w, b*m/100+w); }
    w_tint[5]=rgb15(r*45/100,g*45/100,b*45/100);
    int mr=40+wr(216), mg=40+wr(216), mb=40+wr(216);
    for(int i=0;i<7;i++){ int m=6+i*16; w_mon[i]=rgb15(mr*m/100+(i>4?(i-4)*40:0), mg*m/100+(i>4?(i-4)*40:0), mb*m/100+(i>4?(i-4)*40:0)); }
    w_mon[7] = wr(2) ? rgb15(255,240,110) : rgb15(120,255,230);
    for(int i=0;i<3;i++) w_styles[i]=wr(5);
}
static const char *world_name(void){ return endless ? w_name : tint_name[zone]; }

enum { IT_NONE, IT_BERRY, IT_SEED, IT_SHELL, IT_THORN, IT_CRUMB, IT_WARP, IT_GEM };
static const char *const gem_name[4]={"RUBY","SAPPHIRE","EMERALD","DIAMOND"};
static const char *const gem_use[4]={"HEALS ALL","+XP","FULL BELLY","BITE +1 FOREVER"};
static const char *const berry_col[8]={"RED","BLUE","GOLD","BLACK","WHITE","PINK","GREEN","PURPLE"};
enum { BE_FOOD, BE_HEAL, BE_POISON, BE_SPEED, BE_SIGHT, BE_DIZZY, BE_STRONG, BE_GOLD };
static const char *const berry_eff[8]={"FOOD","HEAL","POISON","SPEED","SIGHT","DIZZY","STRONG","GOLD"};
static const char *const seed_look[6]={"ROUND","LONG","SPIKY","FLAT","TINY","HAIRY"};
enum { SE_STUN, SE_FIND, SE_ESCAPE, SE_CALL, SE_BLAST, SE_HEAL };
static const char *const seed_eff[6]={"STUN","FINDER","ESCAPE","CALLER","BLAST","HEALER"};
static const char *const shell_name[5]={"ACORN SHELL","BEETLE SHELL","CRYSTAL SHELL","STICKY SHELL","GOLDEN SHELL"};
static const char *const thorn_name[5]={"ROSE THORN","WASP STING","GLASS NEEDLE","DARK THORN","KING FANG"};
static u8 berry_map[8], seed_map[6], berry_known[8], seed_known[6];
static u8 bag_it[8], bag_sub[8]; static int bag_n=0, bag_sel=0;
static int shell=-1, thorn=-1;
static int speed_t=0, strong_t=0, dizzy_t=0, stun_t=0, sight=0, hunger_t=0;
static int escape_flag=0, call_flag=0, blast_flag=0;

static void shuffle(u8 *a, int n){ for(int i=0;i<n;i++) a[i]=i; for(int i=n-1;i>0;i--){ int j=rndi(i+1); u8 t=a[i]; a[i]=a[j]; a[j]=t; } }
static void new_game(void){
    shuffle(berry_map,8); shuffle(seed_map,6);
    for(int i=0;i<8;i++) berry_known[i]=0; for(int i=0;i<6;i++) seed_known[i]=0;
    for(int i=0;i<6;i++) have_bug[i]=0;
    zone=0; zone_cleared=0; maxh=5; level=1; xp=0; atk_bonus=0; hearts=5; food=100; gold=0; bugs_freed=0; bag_n=0; bag_sel=0; shell=-1; thorn=-1;
    bag_it[bag_n]=IT_CRUMB; bag_sub[bag_n]=0; bag_n++;
}

static void scpy(char *d, const char *s){ while(*s) *d++=*s++; *d=0; }
static void scat(char *d, const char *s){ while(*d) d++; scpy(d,s); }
static void item_name(int it, int sub, char *b){
    b[0]=0;
    switch(it){
        case IT_BERRY: scpy(b,berry_col[sub]); scat(b," BERRY"); if(berry_known[sub]){ scat(b," "); scat(b,berry_eff[berry_map[sub]]); } break;
        case IT_SEED:  scpy(b,seed_look[sub]); scat(b," SEED"); if(seed_known[sub]){ scat(b," "); scat(b,seed_eff[seed_map[sub]]); } break;
        case IT_SHELL: scpy(b, sub==4 ? "GOLDEN SHELL" : "SHELL ?"); break;
        case IT_THORN: scpy(b, sub==4 ? "KING FANG" : "THORN ?"); break;
        case IT_WARP: scpy(b,"WARP CRYSTAL"); break;
        case IT_GEM: scpy(b,gem_name[sub&3]); break;
        case IT_CRUMB: scpy(b,"CRUMB"); break;
        default: scpy(b,"EMPTY BAG");
    }
}

// ---------------------------------------------------------------- the status bar
static void draw_hud(volatile u16 *pg, const char *l1){
    rect(pg,0,0,240,17,0); rect(pg,0,17,240,1,6);
    char b[24]; int s=(frames/60)%3600; num(b,s/60,2); b[2]=':'; num(b+3,s%60,2);
    text(pg,4,4,b,C_WHITE,2);
    for(int i=0;i<maxh;i++){ int x=48+i*7, c = i<hearts?C_RED:C_DRED;
        for(int j=0;j<6;j++){ int w = j<2 ? j*2+3 : (5-j)*2+1; rect(pg,x+3-w/2,3+j,w,1,c); } }
    rect(pg,48,12,40,3,C_DGREY); rect(pg,48,12,food*40/100,3, food>25?C_GREEN:C_RED);
    { char lv[6]="L"; num(lv+1,level,2); text(pg,92,11,lv,C_GOLD,1); }
    text(pg,108,3,l1,C_WHITE,1);
    if(bag_n){ char nm[24]; item_name(bag_it[bag_sel],bag_sub[bag_sel],nm); b[0]='0'+bag_sel+1; b[1]='/'; b[2]='0'+bag_n; b[3]=' '; b[4]=0; scat(b,nm); text(pg,108,10,b,C_CYAN,1); }
    else text(pg,108,10,"BAG EMPTY",C_GREY,1);
    if(endless && scene==1){ text(pg,206,3,"+",C_GOLD,1); num(b,loot,4); text(pg,212,3,b,C_GOLD,1); }
    else { text(pg,206,3,"G",C_GOLD,1); num(b,gold,4); text(pg,212,3,b,C_WHITE,1); }
    text(pg,206,10,"*",C_RED,1); num(b,bugs_freed,2); text(pg,212,10,b,C_WHITE,1);
}
static void hud_now(void);     // redraws the bar for the scene we are in
static void say(const char *s);  // a message in the middle of the screen
static void show_msg(const char *s);

// ---------------------------------------------------------------- freeing bugs
static void free_bug(int t){
    bugs_freed++; have_bug[t]++;
    if(t==BUG_PILL) earn(10);
    if(t==BUG_LADY && maxh<8){ maxh++; hearts=maxh; }
    char b[40]; scpy(b,"FREED A "); scat(b,bug_name[t]); say(b);
    hud_now();
}

// ---------------------------------------------------------------- getting hurt, hunger
static void hurt(int n){
    if(invuln) return;
    if(shell>=0){ static const u8 blk[5]={2,3,4,3,5}; if(rndi(6)<blk[shell]){ say("YOUR SHELL BLOCKED IT"); invuln=20; return; } }
    hearts-=n; invuln=40; hud_now();
}
static void hunger_tick(int every){
    if(++hunger_t < every) return; hunger_t=0;
    if(food>0){ food--; if(food==20) say("YOU ARE HUNGRY"); if(food%5==0) hud_now(); }
    else { hearts--; say("STARVING! EAT SOMETHING"); hud_now(); }
}

// ---------------------------------------------------------------- the bag
static int add_item(int it, int sub){
    if(bag_n>=8){ say("YOUR BAG IS FULL"); return 0; }
    bag_it[bag_n]=it; bag_sub[bag_n]=sub; bag_n++;
    char b[40], nm[24]; item_name(it,sub,nm); scpy(b,"GOT "); scat(b,nm); say(b); hud_now(); return 1;
}
static int item_value(int it,int sub){
    switch(it){ case IT_GEM: { static const u8 v[4]={40,60,90,150}; return v[sub&3] + 5*dlv(); }
                case IT_SHELL: case IT_THORN: return sub==4 ? 200 : 20;
                case IT_WARP: return 60; case IT_SEED: return 8; case IT_BERRY: return 5; default: return 2; }
}
static void drop_sel(void){ for(int i=bag_sel;i<bag_n-1;i++){ bag_it[i]=bag_it[i+1]; bag_sub[i]=bag_sub[i+1]; } bag_n--; if(bag_sel>=bag_n) bag_sel=bag_n?bag_n-1:0; }
static void eat(int amount){ food+=amount; if(food>100) food=100; }
static void use_item(void){
    if(!bag_n){ say("YOUR BAG IS EMPTY"); return; }
    int it=bag_it[bag_sel], sub=bag_sub[bag_sel]; char b[40];
    switch(it){
    case IT_CRUMB: eat(40); say("CRUNCH. FOOD +40"); break;
    case IT_BERRY: { int e=berry_map[sub]; berry_known[sub]=1; scpy(b,berry_col[sub]); scat(b," BERRY: ");
        switch(e){
            case BE_FOOD: eat(60); scat(b,"FOOD!"); break;
            case BE_HEAL: hearts=maxh; scat(b,"HEALED!"); break;
            case BE_POISON: hearts--; scat(b,"POISON!"); break;
            case BE_SPEED: speed_t=240; scat(b,"SPEED!"); break;
            case BE_SIGHT: sight=1; scat(b,"YOU SEE THE WAY"); break;
            case BE_DIZZY: dizzy_t=200; scat(b,"DIZZY!"); break;
            case BE_STRONG: strong_t=300; scat(b,"STRONG BITE!"); break;
            default: earn(30); scat(b,"+30 GOLD"); break;
        }
        eat(5); say(b); break; }
    case IT_SEED: { int e=seed_map[sub]; seed_known[sub]=1; scpy(b,seed_look[sub]); scat(b," SEED: ");
        switch(e){
            case SE_STUN: stun_t=200; scat(b,"MONSTERS FREEZE"); break;
            case SE_FIND: sight=1; scat(b,"FINDS THE STAIRS"); break;
            case SE_ESCAPE: escape_flag=1; scat(b,"ESCAPE!"); break;
            case SE_CALL: call_flag=1; scat(b,"CALLS A BUG"); break;
            case SE_BLAST: blast_flag=1; scat(b,"BLAST!"); break;
            default: hearts+=2; if(hearts>maxh) hearts=maxh; scat(b,"+2 HEARTS"); break;
        }
        say(b); break; }
    case IT_SHELL:
        if(shell==3){ say("THE STICKY SHELL WON'T COME OFF"); return; }
        shell=sub; scpy(b,"WEARING "); scat(b,shell_name[sub]); if(sub==3) scat(b,". STUCK!"); say(b); break;
    case IT_THORN:
        thorn=sub; scpy(b,"BITING WITH "); scat(b,thorn_name[sub]); say(b); break;
    case IT_WARP:
        if(scene==0){ say("KEEP IT FOR A DUNGEON"); return; }
        escape_flag=1; say("WARP!"); break;
    case IT_GEM: { int t=sub&3; scpy(b,gem_name[t]); scat(b,": "); scat(b,gem_use[t]);
        if(t==0) hearts=maxh; else if(t==1) gain_xp(20+depth_no); else if(t==2) food=100; else atk_bonus++;
        say(b); break; }
    }
    drop_sel(); hud_now();
}
static void bag_input(void){
    if(HIT(K_L) && bag_n){ bag_sel=(bag_sel+bag_n-1)%bag_n; hud_now(); }
    if(HIT(K_R) && bag_n){ bag_sel=(bag_sel+1)%bag_n; hud_now(); }
    if(HIT(K_B)) use_item();
}
static int bite_power(void){
    static const u8 tp[5]={1,2,3,2,5};
    int p = 1 + atk_bonus + (thorn>=0 ? tp[thorn] : 0);
    if(strong_t) p*=2;
    if(thorn==3 && rndi(6)==0){ say("THE DARK THORN BITES YOU"); hearts--; hud_now(); }
    return p;
}
static void tick_timers(void){ if(speed_t) speed_t--; if(strong_t) strong_t--; if(dizzy_t) dizzy_t--; if(stun_t) stun_t--; }

// ---------------------------------------------------------------- RPG: experience and levels
static int xp_next(void){ return 6*level*level+4; }
static void gain_xp(int n){
    xp+=n;
    while(xp>=xp_next()){
        xp-=xp_next(); level++; atk_bonus++;
        if((level&1)==0 && maxh<8) maxh++;
        hearts=maxh;
        char b[32], n2[4]; scpy(b,"LEVEL UP! NOW LV "); num(n2,level,2); scat(b,n2); say(b);
    }
    hud_now();
}

// ---------------------------------------------------------------- stats screen (START)
static void wait_release(void){ do { vsync(); read_keys(); } while(keys); }
static void stats_screen(volatile u16 *pg){
    rect(pg,0,18,240,142,0);
    char b[48], n2[6]; int y=22;
    text(pg,8,y,"BUGSTAR",C_GOLD,2); text(pg,72,y+3,"YOUR BUG",C_GREY,1); y+=16;
    scpy(b,"LEVEL "); num(n2,level,2); scat(b,n2); scat(b,"   XP "); num(n2,xp,3); scat(b,n2); scat(b,"/"); num(n2,xp_next(),3); scat(b,n2); text(pg,8,y,b,C_WHITE,1); y+=8;
    scpy(b,"BITE "); num(n2,1+atk_bonus+(thorn>=0?(thorn==0?1:thorn==1?2:thorn==2?3:thorn==3?2:5):0),2); scat(b,n2); scat(b,"   HEARTS "); num(n2,hearts,1); scat(b,n2); scat(b,"/"); num(n2,maxh,1); scat(b,n2); scat(b,"   FOOD "); num(n2,food,3); scat(b,n2); text(pg,8,y,b,C_WHITE,1); y+=8;
    scpy(b,"SHELL: "); scat(b, shell>=0?shell_name[shell]:"NONE"); scat(b,"   THORN: "); scat(b, thorn>=0?thorn_name[thorn]:"NONE"); text(pg,8,y,b,C_WHITE,1); y+=8;
    scpy(b,"GOLD "); num(n2,gold,4); scat(b,n2); scat(b,"   BUGS FREED "); num(n2,bugs_freed,2); scat(b,n2); text(pg,8,y,b,C_WHITE,1); y+=8;
    if(endless){ scpy(b,"ENDLESS GALAXY  WORLD "); num(n2,world_no,3); scat(b,n2); scat(b,"  DEEPEST B"); num(n2,best_depth,3); scat(b,n2); text(pg,8,y,b,C_CYAN,1); }
    y+=4;
    text(pg,8,y,"BUG FRIENDS",C_GOLD,1); y+=8;
    int any=0;
    for(int t=0;t<6;t++) if(have_bug[t]){ any=1; scpy(b,bug_name[t]); scat(b," X"); num(n2,have_bug[t],1); scat(b,n2); scat(b," - "); scat(b,bug_power[t]); text(pg,8,y,b,C_WHITE,1); y+=7; }
    if(!any){ text(pg,8,y,"NONE YET. OPEN CAGES TO FREE BUGS",C_GREY,1); y+=7; }
    y+=5; text(pg,8,y,"WHAT YOU HAVE LEARNED",C_GOLD,1); y+=8;
    for(int i=0;i<8;i++){ scpy(b,berry_col[i]); scat(b,":"); scat(b, berry_known[i]?berry_eff[berry_map[i]]:"?"); text(pg,8+(i&3)*58,y+(i>>2)*7,b,berry_known[i]?C_WHITE:C_GREY,1); }
    y+=16;
    for(int i=0;i<6;i++){ scpy(b,seed_look[i]); scat(b,":"); scat(b, seed_known[i]?seed_eff[seed_map[i]]:"?"); text(pg,8+(i%3)*76,y+(i/3)*7,b,seed_known[i]?C_WHITE:C_GREY,1); }
    text(pg,8,150,"START: BACK   L/R: PICK ITEM   B: USE",C_CYAN,1);
    wait_release();
    do { vsync(); read_keys(); } while(!HIT(K_START));
    wait_release();
}

// ---------------------------------------------------------------- the ship's trading post
static int shop_menu(void){   // returns 1 to fly on
    const char *opt[9]={ endless?"WARP TO A NEW WORLD":"FLY TO THE NEXT ZONE","CRUMB          10G","MYSTERY BERRY  15G","MYSTERY SEED   20G","HEAL ALL       25G","IDENTIFY ITEM  30G","WARP CRYSTAL  120G","SELL PICKED ITEM","LEAVE"};
    int can_fly = endless || zone_cleared;
    int sel = can_fly?0:1;
    volatile u16 *pg=VRAM;
    oam_hide_all(); oam_flush();
    wait_release();
    int dirty=1;
    while(1){
        if(dirty){ dirty=0;
        rect(pg,36,20,168,92,0); rect(pg,36,20,168,1,6); rect(pg,36,111,168,1,6);
        text(pg,44,24,"SHIP - TRADING POST",C_GOLD,1);
        for(int i=0;i<9;i++){ int c = (i==0 && !can_fly) ? C_DGREY : (i==sel?C_WHITE:C_GREY);
            if(i==sel) text(pg,44,34+i*8,">",C_CYAN,1); text(pg,52,34+i*8,opt[i],c,1); }
        if(sel==7 && bag_n){ char b[40], nm[24], n2[6]; item_name(bag_it[bag_sel],bag_sub[bag_sel],nm); scpy(b,nm); scat(b," = "); num(n2,item_value(bag_it[bag_sel],bag_sub[bag_sel]),3); scat(b,n2); scat(b,"G");
            rect(pg,120,90,84,6,0); text(pg,120,90,b,C_CYAN,1); }
        }
        vsync(); read_keys();
        if(keys!=keys_old) dirty=1;
        if(HIT(K_UP)) sel=(sel+8)%9;
        if(HIT(K_DOWN)) sel=(sel+1)%9;
        if(HIT(K_L) && bag_n){ bag_sel=(bag_sel+bag_n-1)%bag_n; hud_now(); }
        if(HIT(K_R) && bag_n){ bag_sel=(bag_sel+1)%bag_n; hud_now(); }
        if(HIT(K_B) || (HIT(K_A) && sel==8)) break;
        if(HIT(K_A)){
            static const u8 cost[9]={0,10,15,20,25,30,120,0,0};
            if(sel==0){ if(can_fly){ wait_release(); return 1; } continue; }
            if(sel==7){ if(!bag_n){ show_msg("NOTHING TO SELL"); continue; } int v=item_value(bag_it[bag_sel],bag_sub[bag_sel]); gold+=v; drop_sel(); show_msg("SOLD!"); hud_now(); continue; }
            if(gold<cost[sel]){ show_msg("NOT ENOUGH GOLD"); continue; }
            int ok=1;
            if(sel==1) ok=add_item(IT_CRUMB,0);
            else if(sel==2) ok=add_item(IT_BERRY,rndi(8));
            else if(sel==3) ok=add_item(IT_SEED,rndi(6));
            else if(sel==4){ hearts=maxh; show_msg("ALL HEALED"); }
            else if(sel==5){
                ok=0;
                for(int k=0;k<bag_n && !ok;k++){ int i=(bag_sel+k)%bag_n;
                    if(bag_it[i]==IT_BERRY && !berry_known[bag_sub[i]]){ berry_known[bag_sub[i]]=1; ok=1; }
                    else if(bag_it[i]==IT_SEED && !seed_known[bag_sub[i]]){ seed_known[bag_sub[i]]=1; ok=1; } }
                show_msg(ok?"ITEM IDENTIFIED":"NOTHING TO IDENTIFY");
            }
            else if(sel==6) ok=add_item(IT_WARP,0);
            if(ok){ gold-=cost[sel]; hud_now(); }
        }
    }
    wait_release();
    return 0;
}
