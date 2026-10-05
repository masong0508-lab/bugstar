// ================================================================= THE RPG: zones, stats, gear, magic, pet, quests
// zone 0..5: The Garden .. The Anthill. Zone z has z+3 dungeon floors; the last one has a boss.
static int zone=0, zone_cleared=0, maxh=30, food=100, bugs_freed=0, scene=0;
static int level, xp;
static void scpy(char *d, const char *s); static void scat(char *d, const char *s); static void gain_xp(int n);
static void hud_now(void); static void say(const char *s); static void show_msg(const char *s); static void sfx(int n);
static int have_bug[6];
enum { BUG_PILL, BUG_LADY, BUG_FIRE, BUG_BEE, BUG_CRICKET, BUG_ANT };
static const char *const bug_name[6]={"PILL BUG","LADYBUG","FIREFLY","BEE","CRICKET","RUNAWAY ANT"};
static const char *const bug_power[6]={"+10 GOLD","+5 HEALTH","LIGHTS THE DARK","STINGS ATTACKERS","FASTER LEGS","POINTS TO STAIRS"};
// ---- endless galaxy (after the Ant King)
static int endless=0, world_no=0, loot=0, best_depth=0, w_dlv=6; static u32 galaxy_seed=1;
static int dlv(void){ int d = endless ? w_dlv : zone; return d>30?30:d; }
static int zone_floors(void){ return endless ? 9999 : zone+3; }
static int gold_find=100;
static void earn(int g){ g = g*gold_find/100; if(endless && scene==1) loot+=g; else gold+=g; }
static u16 w_tint[6], w_mon[8]; static u8 w_styles[3]; static char w_name[24];
static u32 wseed;
static int wr(int n){ wseed = wseed*1103515245u + 12345u; return (int)((wseed>>16)%(u32)n); }
static u16 rgb15(int r,int g,int b){ if(r>255)r=255; if(g>255)g=255; if(b>255)b=255; return (r>>3)|((g>>3)<<5)|((b>>3)<<10); }
static void name_of(u32 wn, char *b){
    static const char *const syl[16]={"ZA","KOR","MI","LUX","PO","BRA","NU","TIK","VO","RAN","SPI","GLO","QUA","FEN","DOR","ULM"};
    static const char *const suf[6]={""," PRIME"," II"," MAJOR"," MINOR"," X"};
    wseed = galaxy_seed ^ (wn*2654435761u);
    b[0]=0; int n=2+wr(2); for(int i=0;i<n;i++) scat(b,syl[wr(16)]); scat(b,suf[wr(6)]);
}
static void make_world(void){
    name_of((u32)world_no,w_name);
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

// ---------------------------------------------------------------- items
enum { IT_NONE, IT_BERRY, IT_SEED, IT_SHELL, IT_THORN, IT_CRUMB, IT_WARP, IT_GEM, IT_CHARM, IT_BOOK, IT_FISH, IT_POTION, IT_QUEST };
enum { R_NORMAL, R_MAGIC, R_RARE };
static const char *const gem_name[4]={"RUBY","SAPPHIRE","EMERALD","DIAMOND"};
static const char *const gem_use[4]={"HEALS ALL","+XP","FULL BELLY","+2 STRENGTH"};
static const char *const berry_col[8]={"RED","BLUE","GOLD","BLACK","WHITE","PINK","GREEN","PURPLE"};
enum { BE_FOOD, BE_HEAL, BE_POISON, BE_SPEED, BE_SIGHT, BE_DIZZY, BE_STRONG, BE_GOLD };
static const char *const berry_eff[8]={"FOOD","HEAL","POISON","SPEED","SIGHT","DIZZY","STRONG","GOLD"};
static const char *const seed_look[6]={"ROUND","LONG","SPIKY","FLAT","TINY","HAIRY"};
enum { SE_STUN, SE_FIND, SE_ESCAPE, SE_CALL, SE_BLAST, SE_HEAL };
static const char *const seed_eff[6]={"STUN","FINDER","ESCAPE","CALLER","BLAST","HEALER"};
static const char *const shell_name[5]={"ACORN SHELL","BEETLE SHELL","CRYSTAL SHELL","STICKY SHELL","GOLDEN SHELL"};
static const char *const thorn_name[5]={"ROSE THORN","WASP STING","GLASS NEEDLE","DARK THORN","KING FANG"};
static const char *const charm_name[4]={"AMBER CHARM","JADE CHARM","BONE CHARM","OPAL CHARM"};
static const char *const fish_name[5]={"SILVER FISH","FIRE FISH","STONE FISH","KING FISH","GOLD FISH"};
static const char *const potion_name[2]={"HEALTH POTION","MANA POTION"};
enum { SP_FIRE, SP_SPARK, SP_HEAL, SP_SHIELD, NSPELL };
static const char *const spell_name[NSPELL]={"FIREBALL","SPARK","HEAL","SHIELD"};
static const u8 spell_cost[NSPELL]={4,6,8,10};
// prefixes and suffixes, like the magic items in Fate
static const char *const wpre[7]={"","SHARP","CRUEL","FLAMING","VENOM","LUCKY","VAMPIRE"};
static const char *const apre[7]={"","STURDY","THICK","SPIKY","SWIFT","BLESSED","GLOWING"};
static const char *const suff[8]={"","OF LIFE","OF MAGIC","OF POWER","OF THE FOX","OF GREED","OF HASTE","OF THE BEE"};

typedef struct { u8 it, sub, rar, pre, suf, lvl; } Gear;
#define BAGMAX 12
static Gear bag[BAGMAX]; static int bag_n=0, bag_sel=0;
static Gear eqw, eqa, eqc;           // what you bite with, what you wear, your charm
static u8 berry_map[8], seed_map[6], berry_known[8], seed_known[6];

// ---------------------------------------------------------------- your bug: stats and everything worked out from them
static int st_str, st_dex, st_vit, st_mag, stat_pts;
static int mp, maxmp, fame;
static int d_dmg, d_def, d_crit, d_spd, d_regen, d_reflect, d_vamp, d_haste, d_sting, d_fire, d_light, d_venom;
static u8 spell_known[NSPELL]; static int spell_sel=0;
static int shake_t=0, bob_ph=0;
static int speed_t=0, strong_t=0, dizzy_t=0, stun_t=0, sight=0, hunger_t=0, shield_t=0, regen_t=0;
static int escape_flag=0, call_flag=0, blast_flag=0, cast_flag=-1;
static int portal_depth=0;

static void scpy(char *d, const char *s){ while(*s) *d++=*s++; *d=0; }
static void scat(char *d, const char *s){ while(*d) d++; scpy(d,s); }
static void ncat(char *d, int v){ char t[8]; int n=0; if(v<0){ scat(d,"-"); v=-v; } do { t[n++]='0'+v%10; v/=10; } while(v && n<7); char r[8]; for(int i=0;i<n;i++) r[i]=t[n-1-i]; r[n]=0; scat(d,r); }

static int weapon_base(const Gear *g){ static const u8 b[5]={2,4,6,5,10}; return g->it==IT_THORN ? b[g->sub]+g->lvl/2 : 0; }
static int armor_base(const Gear *g){ static const u8 b[5]={2,4,6,5,10}; return g->it==IT_SHELL ? b[g->sub]+g->lvl/3 : 0; }
static void gear_affix(const Gear *g){
    if(!g->it) return;
    int wpn = g->it==IT_THORN, arm = g->it==IT_SHELL, v = 1 + g->lvl/4 + (g->rar==R_RARE);
    if(wpn) switch(g->pre){ case 1: d_dmg+=2*v; break; case 2: d_dmg+=4*v; break; case 3: d_fire+=3*v; break; case 4: d_venom+=2*v; break; case 5: gold_find+=25; break; case 6: d_vamp+=1; break; }
    else if(arm) switch(g->pre){ case 1: d_def+=2*v; break; case 2: d_def+=4*v; break; case 3: d_reflect+=3*v; break; case 4: d_spd+=12; break; case 5: d_regen+=1; break; case 6: d_light+=12; break; }
    else switch(g->pre){ case 1: d_dmg+=v; break; case 2: d_dmg+=2*v; break; case 3: d_fire+=v*2; break; case 4: d_venom+=v; break; case 5: gold_find+=20; break; case 6: d_vamp+=1; break; }
    switch(g->suf){ case 1: maxh+=6*v; break; case 2: maxmp+=5*v; break; case 3: d_dmg+=2*v; break; case 4: d_crit+=8; break; case 5: gold_find+=40; break; case 6: d_haste+=3; break; case 7: d_sting+=3*v; break; }
}
static void recalc(void){
    gold_find=100; d_dmg=0; d_def=0; d_crit=0; d_spd=0; d_regen=0; d_reflect=0; d_vamp=0; d_haste=0; d_sting=0; d_fire=0; d_light=0; d_venom=0;
    maxh = 20 + st_vit*3 + level*2 + have_bug[BUG_LADY]*5;
    maxmp = 5 + st_mag*3;
    gear_affix(&eqw); gear_affix(&eqa); gear_affix(&eqc);
    d_dmg += 2 + st_str/2 + weapon_base(&eqw);
    d_def += st_vit/4 + armor_base(&eqa);
    d_crit += st_dex;
    d_haste += st_dex/6;
    d_sting += have_bug[BUG_BEE]*2;
    if(eqa.it==IT_SHELL && eqa.sub==3) d_spd -= 15;        // the sticky shell slows you
    if(hearts>maxh) hearts=maxh; if(mp>maxmp) mp=maxmp;
}
static void shuffle(u8 *a, int n){ for(int i=0;i<n;i++) a[i]=i; for(int i=n-1;i>0;i--){ int j=rndi(i+1); u8 t=a[i]; a[i]=a[j]; a[j]=t; } }

// ---------------------------------------------------------------- the pet (like Fate's dog): it follows you, fights, and changes when fed fish
static int pet_hp, pet_maxhp, pet_form, pet_form_t, pet_away_t, pet_down_t, pet_cool, pet_sold_n, pet_sold_g;
static const char *const pet_form_name[4]={"BUDDY","FIRE BEETLE","STONE BEETLE","STAG KING"};
static int pet_dmg(void){ static const u8 m[4]={2,4,3,6}; return (2 + level/2) * m[pet_form] / 2; }

// ---------------------------------------------------------------- quests and fame
enum { Q_NONE, Q_HUNT, Q_CHAMP, Q_FETCH, Q_RESCUE };
static int q_type, q_need, q_have, q_floor, q_done, q_gold, q_fame, q_zone, q_world; static char q_name[24];
static const char *fame_rank(void){
    return fame>=1200?"BUG LEGEND": fame>=700?"CHAMPION": fame>=350?"HERO": fame>=150?"BRAVE BUG": fame>=50?"KNOWN BUG":"NOBODY";
}

static void new_game(void){
    shuffle(berry_map,8); shuffle(seed_map,6);
    for(int i=0;i<8;i++) berry_known[i]=0; for(int i=0;i<6;i++) seed_known[i]=0;
    for(int i=0;i<6;i++) have_bug[i]=0;
    for(int i=0;i<NSPELL;i++) spell_known[i]=0; spell_known[SP_FIRE]=1; spell_sel=0;
    zone=0; zone_cleared=0; level=1; xp=0; food=100; gold=0; bugs_freed=0; bag_n=0; bag_sel=0; fame=0; portal_depth=0;
    endless=0; world_no=0; loot=0; best_depth=0;
    st_str=6; st_dex=5; st_vit=6; st_mag=5; stat_pts=0;
    eqw=(Gear){IT_THORN,0,R_NORMAL,0,0,1}; eqa=(Gear){0,0,0,0,0,0}; eqc=(Gear){0,0,0,0,0,0};
    recalc(); hearts=maxh; mp=maxmp;
    pet_form=0; pet_form_t=0; pet_away_t=0; pet_down_t=0; pet_maxhp=30; pet_hp=pet_maxhp;
    q_type=Q_NONE;
    bag[bag_n++]=(Gear){IT_CRUMB,0,0,0,0,0};
    bag[bag_n++]=(Gear){IT_POTION,0,0,0,0,0};
}

static void gear_name(const Gear *g, char *b){
    b[0]=0;
    int it=g->it, sub=g->sub;
    switch(it){
        case IT_BERRY: scpy(b,berry_col[sub]); scat(b," BERRY"); if(berry_known[sub]){ scat(b," "); scat(b,berry_eff[berry_map[sub]]); } break;
        case IT_SEED:  scpy(b,seed_look[sub]); scat(b," SEED"); if(seed_known[sub]){ scat(b," "); scat(b,seed_eff[seed_map[sub]]); } break;
        case IT_SHELL: case IT_THORN: case IT_CHARM: {
            const char *pre = it==IT_THORN ? wpre[g->pre] : it==IT_SHELL ? apre[g->pre] : wpre[g->pre];
            if(g->pre){ scpy(b,pre); scat(b," "); }
            scat(b, it==IT_THORN ? thorn_name[sub] : it==IT_SHELL ? shell_name[sub] : charm_name[sub&3]);
            if(g->suf){ scat(b," "); scat(b,suff[g->suf]); }
            break; }
        case IT_WARP: scpy(b,"WARP CRYSTAL"); break;
        case IT_GEM: scpy(b,gem_name[sub&3]); break;
        case IT_CRUMB: scpy(b,"CRUMB"); break;
        case IT_BOOK: scpy(b,"BOOK: "); scat(b,spell_name[sub%NSPELL]); break;
        case IT_FISH: scpy(b,fish_name[sub%5]); break;
        case IT_POTION: scpy(b,potion_name[sub&1]); break;
        case IT_QUEST: scpy(b,"LOST TREASURE"); break;
        default: scpy(b,"NOTHING");
    }
}
static int rar_color(const Gear *g){ return g->rar==R_RARE ? C_GOLD : g->rar==R_MAGIC ? C_CYAN : C_WHITE; }

// make a random piece of gear for this depth (drops, shops, rewards)
static Gear roll_gear(int kind, int ilvl, int luck){
    Gear g; g.it=kind; g.lvl=ilvl>99?99:ilvl; g.pre=0; g.suf=0;
    int r=rndi(100)+luck; g.rar = r>=95 ? R_RARE : r>=70 ? R_MAGIC : R_NORMAL;
    if(kind==IT_CHARM && g.rar==R_NORMAL) g.rar=R_MAGIC;
    if(kind==IT_THORN){ int t=rndi(100)+ilvl; g.sub = t>110?4 : t>75?2 : t>45?1 : (rndi(5)==0?3:0); }
    else if(kind==IT_SHELL){ int t=rndi(100)+ilvl; g.sub = t>110?4 : t>75?2 : t>45?1 : (rndi(5)==0?3:0); }
    else g.sub=rndi(4);
    if(g.rar==R_MAGIC){ if(rndi(2)) g.pre=1+rndi(6); else g.suf=1+rndi(7); }
    if(g.rar==R_RARE){ g.pre=1+rndi(6); g.suf=1+rndi(7); }
    return g;
}
static Gear roll_drop(int ilvl, int luck){
    int r=rndi(100);
    if(r<16) return roll_gear(IT_THORN,ilvl,luck);
    if(r<32) return roll_gear(IT_SHELL,ilvl,luck);
    if(r<40) return roll_gear(IT_CHARM,ilvl,luck);
    Gear g={0,0,0,0,0,0};
    if(r<55){ g.it=IT_BERRY; g.sub=rndi(8); }
    else if(r<66){ g.it=IT_POTION; g.sub=rndi(2); }
    else if(r<76){ g.it=IT_SEED; g.sub=rndi(6); }
    else if(r<84){ g.it=IT_CRUMB; }
    else if(r<88){ g.it=IT_BOOK; g.sub=rndi(NSPELL); }
    else { g.it=IT_GEM; int t=rndi(10)+ilvl/6; g.sub = t<5?0 : t<8?1 : t<11?2 : 3; }
    return g;
}
static int item_value(const Gear *g){
    switch(g->it){
        case IT_GEM: { static const u8 v[4]={40,60,90,150}; return v[g->sub&3] + 5*dlv(); }
        case IT_SHELL: case IT_THORN: case IT_CHARM: { int v=12+g->lvl*3+(g->sub==4?150:0); return g->rar==R_RARE?v*4 : g->rar==R_MAGIC?v*2 : v; }
        case IT_WARP: return 60; case IT_SEED: return 8; case IT_BERRY: return 5; case IT_BOOK: return 80; case IT_POTION: return 12;
        case IT_FISH: { static const u8 v[5]={10,40,40,90,120}; return v[g->sub%5]; }
        default: return 2;
    }
}

// ---------------------------------------------------------------- the status bar
static void draw_hud(volatile u16 *pg, const char *l1){
    rect(pg,0,0,240,17,0); rect(pg,0,17,240,1,6);
    char b[32]; b[0]=0;
    scpy(b,"HP "); ncat(b,hearts<0?0:hearts); scat(b,"/"); ncat(b,maxh); text(pg,3,3,b,C_RED,1);
    b[0]=0; scpy(b,"MP "); ncat(b,mp); scat(b,"/"); ncat(b,maxmp); text(pg,3,10,b,C_CYAN,1);
    rect(pg,52,4,30,3,C_DGREY); rect(pg,52,4,(hearts>0?hearts:0)*30/(maxh?maxh:1),3,C_RED);
    rect(pg,52,11,30,3,C_DGREY); rect(pg,52,11,food*30/100,3, food>25?C_GREEN:C_RED);
    { char lv[8]="LV"; ncat(lv,level); text(pg,86,3,lv,C_GOLD,1); }
    { int xb = xp*16/((6*level*level+4)*3); if(xb>16) xb=16; rect(pg,86,11,16,2,C_DGREY); rect(pg,86,11,xb,2,C_GOLD); }
    text(pg,106,3,l1,C_WHITE,1);
    if(bag_n){ char nm[40]; gear_name(&bag[bag_sel],nm); b[0]='0'+(bag_sel+1)%10; b[1]=' '; b[2]=0; scat(b,nm); b[25]=0; text(pg,106,10,b,rar_color(&bag[bag_sel]),1); }
    else text(pg,106,10,"BAG EMPTY",C_GREY,1);
    if(endless && scene==1){ text(pg,206,3,"+",C_GOLD,1); num(b,loot,4); text(pg,212,3,b,C_GOLD,1); }
    else { text(pg,206,3,"G",C_GOLD,1); num(b,gold>9999?9999:gold,4); text(pg,212,3,b,C_WHITE,1); }
    text(pg,206,10,"*",C_RED,1); num(b,bugs_freed,2); text(pg,212,10,b,C_WHITE,1);
    if(stat_pts){ text(pg,226,10,"+",C_GREEN,1); }
}

// ---------------------------------------------------------------- freeing bugs
static void free_bug(int t){
    bugs_freed++; have_bug[t]++;
    if(t==BUG_PILL) earn(10);
    recalc(); if(t==BUG_LADY) hearts=maxh;
    char b[40]; scpy(b,"FREED A "); scat(b,bug_name[t]); say(b); sfx(2);
    if(q_type==Q_RESCUE){ q_have++; if(q_have>=q_need) q_done=1; }
    hud_now();
}

// ---------------------------------------------------------------- getting hurt, hunger
static void hurt(int n){
    if(invuln) return;
    int def = d_def + (shield_t ? 5+st_mag/2 : 0);
    n -= def/2; if(n<1) n=1;
    if(rndi(100) < def){ say("BLOCKED!"); invuln=15; return; }
    hearts-=n; invuln=30; shake_t=10; sfx(4); hud_now();
}
static void hunger_tick(int every){
    if(++hunger_t < every) return; hunger_t=0;
    if(food>0){ food--; if(food==20) say("YOU ARE HUNGRY"); if(food%5==0) hud_now(); }
    else { hearts-=1+maxh/20; say("STARVING! EAT SOMETHING"); hud_now(); }
}
static void regen_tick(int every){          // slow healing and mana
    if(++regen_t < every) return; regen_t=0;
    int ch=0;
    if(mp<maxmp){ mp+=1+st_mag/12; if(mp>maxmp) mp=maxmp; ch=1; }
    if(d_regen && hearts<maxh){ hearts+=d_regen; if(hearts>maxh) hearts=maxh; ch=1; }
    if(ch) hud_now();
}

// ---------------------------------------------------------------- the bag
static int add_gear(Gear g){
    if(g.it==IT_QUEST){ q_done=1; say("FOUND THE LOST TREASURE!"); sfx(2); return 1; }
    if(bag_n>=BAGMAX){ say("YOUR BAG IS FULL"); return 0; }
    bag[bag_n++]=g;
    char b[48], nm[40]; gear_name(&g,nm); scpy(b,"GOT "); scat(b,nm); b[39]=0; say(b); sfx(2); hud_now(); return 1;
}
static int add_item(int it, int sub){ Gear g={it,sub,0,0,0,0}; return add_gear(g); }
static void drop_at(int i){ for(int k=i;k<bag_n-1;k++) bag[k]=bag[k+1]; bag_n--; if(bag_sel>=bag_n) bag_sel=bag_n?bag_n-1:0; }
static void drop_sel(void){ drop_at(bag_sel); }
static void eat(int amount){ food+=amount; if(food>100) food=100; }
static int feed_pet(int f);
static int equip(int i){        // wear or wield bag item i; what you had goes back in the bag
    Gear g=bag[i], *slot = g.it==IT_THORN ? &eqw : g.it==IT_SHELL ? &eqa : &eqc;
    if(slot==&eqa && eqa.it==IT_SHELL && eqa.sub==3){ say("THE STICKY SHELL WON'T COME OFF"); return 0; }
    Gear old=*slot; *slot=g; drop_at(i);
    if(old.it && bag_n<BAGMAX) bag[bag_n++]=old;
    char b[48], nm[40]; gear_name(&g,nm); scpy(b,"NOW USING "); scat(b,nm); b[39]=0; say(b);
    recalc(); return 1;
}
static void use_index(int i){
    if(i<0 || i>=bag_n){ say("YOUR BAG IS EMPTY"); return; }
    Gear g=bag[i]; int it=g.it, sub=g.sub; char b[48];
    switch(it){
    case IT_CRUMB: eat(40); say("CRUNCH. FOOD +40"); break;
    case IT_POTION: if(sub==0){ hearts+=maxh/2+10; if(hearts>maxh) hearts=maxh; say("GLUG. HEALTH BACK"); } else { mp=maxmp; say("GLUG. MANA FULL"); } break;
    case IT_BERRY: { int e=berry_map[sub]; berry_known[sub]=1; scpy(b,berry_col[sub]); scat(b," BERRY: ");
        switch(e){
            case BE_FOOD: eat(60); scat(b,"FOOD!"); break;
            case BE_HEAL: hearts=maxh; scat(b,"HEALED!"); break;
            case BE_POISON: hearts-=maxh/5; scat(b,"POISON!"); break;
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
            case SE_ESCAPE: if(scene==0){ say("NOTHING TO ESCAPE"); return; } escape_flag=1; scat(b,"ESCAPE!"); break;
            case SE_CALL: call_flag=1; scat(b,"CALLS A BUG"); break;
            case SE_BLAST: blast_flag=1; scat(b,"BLAST!"); break;
            default: hearts+=maxh/3; if(hearts>maxh) hearts=maxh; scat(b,"HEALTH +"); break;
        }
        say(b); break; }
    case IT_SHELL: case IT_THORN: case IT_CHARM: equip(i); hud_now(); return;
    case IT_WARP:
        if(scene==0){ say("KEEP IT FOR A DUNGEON"); return; }
        escape_flag=1; portal_depth=depth_no; say("A PORTAL OPENS!"); break;
    case IT_GEM: { int t=sub&3; scpy(b,gem_name[t]); scat(b,": "); scat(b,gem_use[t]);
        if(t==0) hearts=maxh; else if(t==1) gain_xp(20+depth_no*3); else if(t==2) food=100; else { st_str+=2; recalc(); }
        say(b); break; }
    case IT_BOOK: if(spell_known[sub]){ say("YOU KNOW THAT SPELL"); return; } spell_known[sub]=1; spell_sel=sub; scpy(b,"LEARNED "); scat(b,spell_name[sub]); say(b); break;
    case IT_FISH: if(!feed_pet(sub)) return; break;
    }
    drop_at(i); hud_now();
}
static void use_item(void){ use_index(bag_sel); }
static int sel_held=0, sel_used=0;
static void bag_input(void){
    if(HELD(K_SEL)){                         // hold SELECT + L/R to pick a spell
        if(HIT(K_SEL)){ sel_held=1; sel_used=0; }
        if(HIT(K_L)||HIT(K_R)){ sel_used=1; for(int k=0;k<NSPELL;k++){ spell_sel=(spell_sel+(HIT(K_R)?1:NSPELL-1))%NSPELL; if(spell_known[spell_sel]) break; }
            char b[32]; scpy(b,"SPELL: "); scat(b,spell_name[spell_sel]); say(b); }
        return;
    }
    if(sel_held){ sel_held=0; if(!sel_used) cast_flag=spell_sel; }
    if(HIT(K_L) && bag_n){ bag_sel=(bag_sel+bag_n-1)%bag_n; hud_now(); }
    if(HIT(K_R) && bag_n){ bag_sel=(bag_sel+1)%bag_n; hud_now(); }
    if(HIT(K_B)) use_item();
}
static int bite_power(int *crit){
    int p = d_dmg + d_fire + d_venom;
    if(strong_t) p*=2;
    *crit = rndi(100) < d_crit; if(*crit) p*=2;
    if(eqw.it==IT_THORN && eqw.sub==3 && rndi(6)==0){ say("THE DARK THORN BITES YOU"); hearts-=2; hud_now(); }
    if(d_vamp && hearts<maxh){ hearts+=d_vamp; }
    return p;
}
static void tick_timers(void){ if(speed_t) speed_t--; if(strong_t) strong_t--; if(dizzy_t) dizzy_t--; if(stun_t) stun_t--; if(shield_t) shield_t--;
    if(pet_form_t && --pet_form_t==0){ say("BUDDY CHANGES BACK"); pet_form=0; } }

// ---------------------------------------------------------------- experience and levels
static int xp_next(void){ return (6*level*level+4)*3; }
static void gain_xp(int n){
    xp+=n;
    while(xp>=xp_next()){
        xp-=xp_next(); level++; stat_pts+=5;
        recalc(); hearts=maxh; mp=maxmp; pet_maxhp+=4; pet_hp=pet_maxhp;
        char b[40]; scpy(b,"LEVEL UP! LV "); ncat(b,level); scat(b,". START: SPEND POINTS"); say(b); sfx(6);
    }
    hud_now();
}

// ---------------------------------------------------------------- feeding Buddy
static int feed_pet(int f){
    if(f==0){ eat(40); say("YUM. FOOD +40"); return 1; }
    if(f==4){ say("TOO PRETTY TO EAT. SELL IT!"); return 0; }
    if(pet_away_t || pet_down_t){ say("BUDDY ISN'T HERE"); return 0; }
    pet_form=f; pet_form_t = f==3 ? 2400 : 1600; pet_hp=pet_maxhp*(f==2?2:1);
    char b[40]; scpy(b,"BUDDY BECOMES A "); scat(b,pet_form_name[f]); say(b); sfx(6);
    return 1;
}
