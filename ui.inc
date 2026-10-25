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
        case IT_FISH: scpy(b, g->sub==0 ? "FOOD +40" : g->sub==4 ? "WORTH A LOT OF GOLD" : g->sub>=5 ? "RARE! EAT FOR A POWER-UP OR SELL" : "FEED IT TO BUDDY"); break;
    }
    if(g->it==IT_THORN||g->it==IT_SHELL||g->it==IT_CHARM){
        const char *const *pt = g->it==IT_SHELL ? apre_txt : wpre_txt;
        if(g->pre){ scat(b,". "); scat(b,pt[g->pre]); }
        if(g->suf){ scat(b,". "); scat(b,suff_txt[g->suf]); }
    }
}
// what would change if you wore this: damage, defence, health, mana, crit, speed, gold
typedef struct { int dmg, def, hp, mp, crit, spd, gold; } Delta;
static Delta gear_delta(const Gear *g){
    Delta d={0,0,0,0,0,0,0}; if(!is_gear(g)) return d;
    Gear *slot = g->it==IT_THORN ? &eqw : g->it==IT_SHELL ? &eqa : &eqc; Gear old=*slot; s64 oh=hearts, om=mp;
    s64 b_dmg=sadd(d_dmg,(s64)d_fire+d_venom), b_def=d_def, b_hp=maxh, b_mp=maxmp; int b_crit=d_crit, b_spd=d_spd, b_gold=gold_find;
    *slot=*g; recalc();
    d.dmg=clampi(sadd(sadd(d_dmg,(s64)d_fire+d_venom),-b_dmg)); d.def=clampi(sadd(d_def,-b_def)); d.hp=clampi(sadd(maxh,-b_hp)); d.mp=clampi(sadd(maxmp,-b_mp)); d.crit=d_crit-b_crit; d.spd=d_spd-b_spd; d.gold=gold_find-b_gold;
    *slot=old; recalc(); hearts=oh; mp=om; return d;
}
static int delta_text(const Gear *g, char *b){     // "DMG +2  DEF -1" in b; returns 1 better, -1 worse, 0 same/none
    b[0]=0; if(!is_gear(g)) return 0; Delta d=gear_delta(g); int sc=0;
    struct { const char *n; int v; } f[7]={{"DMG",d.dmg},{"DEF",d.def},{"HP",d.hp},{"MP",d.mp},{"CRIT",d.crit},{"SPD",d.spd},{"GOLD",d.gold}};
    for(int i=0;i<7;i++) if(f[i].v){ if(b[0]) scat(b,"  "); scat(b,f[i].n); scat(b,f[i].v>0?" +":" "); ncat(b,f[i].v); sc+=f[i].v>0?1:-1; }
    if(!b[0]) scpy(b,"SAME AS NOW");
    return sc>0?1:sc<0?-1:0;
}
static void vbar(volatile u16 *pg, int x, int y, int w, s64 val, s64 max, int col, const char *lab){     // a labelled glossy bar with the numbers on it
    ui_bar(pg,x,y,w,7,val,max,col); char b[16]; b[0]=0; ncat(b,val); scat(b,"/"); ncat(b,max);
    text(pg,x-textlen(lab)*4-3,y+1,lab,U_SOFT,1); text(pg,x+w-textlen(b)*4-2,y+2,b,U_EDGE,1); text(pg,x+w-textlen(b)*4-3,y+1,b,C_WHITE,1);
}
static void box(volatile u16 *pg, int x, int y, int w, int h){ ui_panel(pg,x,y,w,h); }      // (every box is a raised card now)

static void card(volatile u16 *pg, const char *title, const char *const *ln, int n);
static const char *const bring_nm[4]={"FISH","BERRIES","SEEDS","POTIONS"}; static const u8 bring_it[4]={IT_FISH,IT_BERRY,IT_SEED,IT_POTION};
static int q_counted(int t){ return t==Q_HUNT||t==Q_RESCUE||t==Q_SHARDS||t==Q_BRING||t==Q_GOODS; }
static int quest_held(void){ int n=0; if(q_type==Q_BRING){ for(int k=0;k<bag_n;k++) if(bag[k].it==bring_it[q_floor&3]) n++; } else if(q_type==Q_GOODS) n=cargo[q_floor&7]; return n; }
static void quest_refresh(void){ if(q_type==Q_BRING||q_type==Q_GOODS){ q_have=quest_held(); q_done = q_have>=q_need; } }   // fetch quests are done the moment you carry the goods
static void quest_take(void){ if(q_type==Q_BRING){ int left=q_need; for(int k=bag_n-1;k>=0&&left>0;k--) if(bag[k].it==bring_it[q_floor&3]){ drop_at(k); left--; } } else if(q_type==Q_GOODS) cargo[q_floor&7]-=q_need; }
// ---------------------------------------------------------------- the story, told by Bolt
// Bolt is the tin droid that rolls after you. He does not know what he is. The data cores in the drone outposts are his memory.
static const char *const bolt_mem[8][4]={
    {"I WAS A SHIP.","I WAS CALLED THE DAWN.","I REMEMBER THE DARK, AND THE STARS,","AND SOMEONE SMALL WHO SANG."},
    {"I CARRIED SEEDS.","A GARDEN. A POND. A LAWN AT NIGHT.","I PLANTED THEM ONE BY ONE.","YOUR WORLDS. I MADE THEM."},
    {"MY CREW WERE BUGS.","SMALL. STUBBORN. BRAVE.","THEY SLEPT IN MY HEART ON THE LONG WAY.","ONE OF THEM LOOKED LIKE YOU."},
    {"WE FELL.","THE ANTHILL CAUGHT US.","THE ANTS WERE KIND AT FIRST.","THEN THEY WERE HUNGRY."},
    {"THE KING OPENED MY HEART.","HE BUILT A MOUTH AROUND IT.","HE FED IT SHIPS SO IT WOULD GROW.","THAT MOUTH IS THE TECHNOBEAST."},
    {"EVERY SHIP IT EATS, IT GROWS.","IT IS EATING MY MEMORY TOO.","THAT IS WHY I FORGET.","THAT IS WHY I AM SMALL."},
    {"THERE IS A WORD THAT OPENS MY HEART.","THE KING NEVER FOUND IT.","IT IS MY NAME.","MY REAL NAME."},
    {"I KNOW MY NAME NOW.","TAKE ME TO THE BEAST.","I WILL SAY IT. ITS HEART WILL OPEN.","I AM NOT AFRAID. I HAVE YOU."}};
static void bolt_card(volatile u16 *pg, const char *const *ln, int n){        // a card with Bolt's little face
    oam_hide_all(); set_affine(31,128,0,0,128);
    shadow_oam[0].a0=((40)&255)|(1<<8)|(1<<9); shadow_oam[0].a1=((20)&511)|(31<<9)|(1<<14); shadow_oam[0].a2=T_BOLT;
    oam_flush(); card(pg,"BOLT",ln,n); oam_hide_all(); oam_flush();
}
static void bolt_story(volatile u16 *pg, int beat){                            // beats: 0 the start, 1..6 after each zone's boss, 10 the beast dies
    switch(beat){
    case 0: { static const char *const t[6]={"A TIN CAN ROLLS UP BEHIND YOU.","IT BLINKS. IT RATTLES. IT SPEAKS.","","\"I DON'T KNOW WHAT I AM.","I KNOW THAT I AM YOURS.","THE ANTS TOOK YOUR FRIENDS. DOWN. GO DOWN.\""}; bolt_card(pg,t,6); break; }
    case 1: { static const char *const t[5]={"\"THE QUEEN KEPT A MAP.","THE ANTS ARE TAKING BUGS TO THE ANTHILL.","EVERY ONE OF THEM, TO DIG.","DIG FOR WHAT? I DON'T KNOW.","BUT MY WHEELS SHAKE WHEN I THINK OF IT.\""}; bolt_card(pg,t,5); break; }
    case 2: { static const char *const t[5]={"\"I DREAMED LAST NIGHT. DROIDS DON'T DREAM.","I DREAMED OF WATER, AND OF FALLING.","I THINK I HAD A SHIP ONCE.","OR I WAS ONE.","DON'T LAUGH.\""}; bolt_card(pg,t,5); break; }
    case 3: { static const char *const t[5]={"\"THAT MACHINE IN THE DARK.","THE ONE THAT ATE THE PIRATES.","IT LOOKED AT ME.","IT LOOKED AT ME LIKE IT KNEW ME.","I THINK IT DOES.\""}; bolt_card(pg,t,5); break; }
    case 4: { static const char *const t[5]={"\"THE ANTS DIG FOR METAL. OLD METAL.","THEY BUILD OUTPOSTS AROUND WHAT THEY FIND.","I FELT ONE HUMMING. IT HUMMED MY TUNE.","THE ONE I HUM WHEN YOU SLEEP.","FIND THE OUTPOSTS. PLEASE.\""}; bolt_card(pg,t,5); break; }
    case 5: { static const char *const t[5]={"\"THE BEES SING AN OLD SONG.","A STAR FELL, THEY SAY. LONG AGO.","IT CARRIED EVERY WORLD INSIDE IT.","THEY SING IT TO THEIR CHILDREN.","WHY DO I KNOW ALL THE WORDS?\""}; bolt_card(pg,t,5); break; }
    case 6: { static const char *const t[6]={"THE ANT KING, DYING, LAUGHS.","\"YOU CAME FOR YOUR FRIENDS?","THE DAWN CAME FOR EVERYTHING.","I ONLY OPENED ITS HEART.\"","FAR AWAY, SOMETHING WAKES.","BOLT GOES VERY QUIET."}; bolt_card(pg,t,6); break; }
    case 10: { static const char *const t[6]={"THE MOUTH FALLS APART.","INSIDE IT, A SHIP. OLD. WHOLE. SINGING.","BOLT'S VOICE COMES FROM EVERYWHERE AT ONCE.","\"I REMEMBER ALL OF IT.","THANK YOU FOR CARRYING ME.","NOW LET ME CARRY YOU. ALL OF YOU. HOME.\""}; bolt_card(pg,t,6); break; }
    }
}
// what Bolt says when you talk to him at the ship: the next thing to do, and what he remembers
static void bolt_talk(volatile u16 *pg){
    const char *t[6]; int n=0; static char l1[48], l2[48];
    if(!endless && !zone_cleared){ t[n++]="\"THE DOOR IS THE WAY DOWN.\""; t[n++]="\"THE ARROW KNOWS. FOLLOW IT.\""; t[n++]="\"THE BOSS SITS AT THE BOTTOM.\""; }
    else if(!endless && zone<5){ scpy(l1,"\"THIS WORLD IS FREE. NEXT IS "); scat(l1,tint_name[zone+1]); scat(l1,".\""); t[n++]=l1; t[n++]="\"PICK AUTOPILOT AND I'LL FLY US.\""; if(zone>=1) t[n++]="\"OR GO DOWN AGAIN. IT'S HARDER. IT PAYS.\""; }
    else if(!ant_king){ t[n++]="\"THE ANT KING WAITS BELOW.\""; t[n++]="\"YOUR FRIENDS ARE THERE. ALL OF THEM.\""; t[n++]="\"I'M SCARED. LET'S GO ANYWAY.\""; }
    else if(beast_stage>=3 && data_cores<8){ scpy(l1,"\"THE OUTPOSTS HOLD MY MEMORY. "); ncat(l1,data_cores); scat(l1," OF 8.\""); t[n++]=l1; t[n++]="\"RED MARKS ON THE MAP. HATCHES ON THE GROUND.\""; t[n++]="\"BRING ME BACK TO MYSELF.\""; }
    else if(beast_stage>=3){ t[n++]="\"I KNOW MY NAME. THE DAWN.\""; t[n++]="\"FLY INTO THE BEAST. SHOOT ITS CORE.\""; t[n++]="\"I WILL DO THE REST.\""; }
    else { t[n++]="\"THE GALAXY IS OPEN. GO ANYWHERE.\""; t[n++]="\"THE BEAST IS OUT THERE. SO ARE ITS OUTPOSTS.\""; }
    if(data_cores>0){ int k=data_cores-1; if(k>7) k=7; scpy(l2,"\"I REMEMBER: "); scat(l2,bolt_mem[k][0]); scat(l2,"\""); t[n++]=l2; }
    if(base_mode==0 && n<6) t[n++]="";
    bolt_card(pg,t,n);
}
// ---------------------------------------------------------------- story cards: a few short lines, then A
static void card(volatile u16 *pg, const char *title, const char *const *ln, int n){
    int h=34+n*11+14, y0=88-h/2;
    ui_panel(pg,10,y0,220,h); rect(pg,12,y0+2,216,13,U_RAISE); rect(pg,12,y0+2,216,1,U_SHINE); rect(pg,12,y0+15,216,1,U_EDGE);   // a title band
    { int tw=textlen(title)*4; rect(pg,120-tw/2-8,y0+7,3,3,U_ACCENT); rect(pg,120+tw/2+5,y0+7,3,3,U_ACCENT); stext(pg,120-tw/2,y0+6,title,C_GOLD); }
    for(int i=0;i<n;i++) stext(pg,120-textlen(ln[i])*2,y0+21+i*11,ln[i],C_WHITE);
    wait_release();
    for(int t=0;;t++){ vsync(); read_keys(); if(HIT(K_A)||HIT(K_START)) break;
        if((t&31)==0){ rect(pg,96,y0+h-13,48,11,U_PANEL); if(t&32) ui_hints(pg,104,y0+h-10,"A: OK",C_WHITE,1); else ui_hints(pg,104,y0+h-10,"A: OK",U_SOFT,1); } }
    sfx(2); wait_release();
}
// ---------------------------------------------------------------- the colour picker: 16 presets, or mix your own with red, green and blue sliders
static void swatch(volatile u16 *pg, int x, int y, int w, int h, u16 c){ BG_PAL[239]=c; rect(pg,x,y,w,h,239); }      // one BG palette slot per swatch
static u16 pick_colour(volatile u16 *pg, const char *what, u16 cur){
    int sel=0, zone=0, dirty=1, r=cur&31, g=(cur>>5)&31, b=(cur>>10)&31, rep=0;                  // zone 0: presets, 1: sliders
    for(int i=0;i<NPRESET;i++) if(preset_col[i]==cur) sel=i;
    wait_release();
    while(1){
        u16 mix = zone ? (u16)(r|(g<<5)|(b<<10)) : preset_col[sel];
        if(dirty){ dirty=0; char t[40];
            panel(pg,12,20,216,136,what);
            text(pg,20,37,"PRESETS",C_GREY,1);
            for(int i=0;i<NPRESET;i++){ int x=20+(i&7)*14, y=46+(i>>3)*16; BG_PAL[224+i]=preset_col[i]; rect(pg,x,y,11,11,224+i);
                if(zone==0 && i==sel){ frame1(pg,x-2,y-2,15,15,C_GOLD); frame1(pg,x-1,y-1,13,13,0); } }
            text(pg,20,82,zone==0?preset_name[sel]:"CUSTOM MIX",C_WHITE,1);
            text(pg,140,37,"MIX YOUR OWN",C_GREY,1);
            static const char *const sl[3]={"RED","GREEN","BLUE"}; int vals[3]={r,g,b}; static const int sc[3]={C_RED,C_GREEN,C_CYAN};
            for(int k=0;k<3;k++){ int y=47+k*12; text(pg,140,y,sl[k],(zone==1&&sel==k)?C_WHITE:C_GREY,1); rect(pg,166,y+1,64,5,C_DGREY); rect(pg,166,y+1,vals[k]*2+2,5,sc[k]);
                if(zone==1&&sel==k){ rect(pg,166+vals[k]*2,y-1,3,9,C_WHITE); } }
            swatch(pg,140,84,88,22,mix); frame1(pg,140,84,88,22,6);
            scpy(t,"R"); ncat(t,r); scat(t," G"); ncat(t,g); scat(t," B"); ncat(t,b); text(pg,144,110,t,C_GREY,1);
            ctext(pg,120,126, zone==0 ? "LEFT/RIGHT/UP/DOWN: PICK   A: USE IT" : "LEFT/RIGHT: SLIDE   UP/DOWN: NEXT   A: USE IT",C_CYAN);
            ui_chint(pg,120,138,"START: SWITCH PRESETS / MIX    B: KEEP THE OLD ONE",C_DGREY);
        }
        vsync(); read_keys();
        if(HIT(K_START)){ zone^=1; if(zone==1){ r=mix&31; g=(mix>>5)&31; b=(mix>>10)&31; sel=0; } dirty=1; sfx(7); }
        if(zone==0){
            if(HIT(K_LEFT)){ sel=(sel+NPRESET-1)%NPRESET; dirty=1; sfx(7); } if(HIT(K_RIGHT)){ sel=(sel+1)%NPRESET; dirty=1; sfx(7); }
            if(HIT(K_UP)||HIT(K_DOWN)){ sel^=8; dirty=1; sfx(7); }
        } else {
            if(HIT(K_UP)){ sel=(sel+2)%3; dirty=1; } if(HIT(K_DOWN)){ sel=(sel+1)%3; dirty=1; }
            int d=0; if(HELD(K_LEFT)||HELD(K_RIGHT)){ rep++; if(rep==1 || (rep>12 && (rep&1))) d = HELD(K_RIGHT)?1:-1; } else rep=0;
            if(d){ int *v = sel==0?&r : sel==1?&g : &b; *v+=d; if(*v<0) *v=0; if(*v>31) *v=31; dirty=1; }
        }
        if(HIT(K_A)){ sfx(2); wait_release(); return mix; }
        if(HIT(K_B)){ wait_release(); return cur; }
    }
}
// ---------------------------------------------------------------- make your bug: parts, shapes and colours, with a live preview
// ---------------------------------------------------------------- the humanoid's look is saved on its own, next to the slot's other blocks (slot + 0xB40)
// (it used to sit at +0xA00, on top of the Technobeast's block: saving the look wiped the beast's progress. Old saves are still read from there.)
#define HUM_OFF 0xB40
typedef struct { u32 magic; u8 style, height, build, made; u16 col[NHC]; u16 pad; u32 sum; } HumSave;
_Static_assert(__builtin_offsetof(HumSave,sum)==sizeof(HumSave)-4, "HumSave: the sum must be the last 4 bytes");
_Static_assert(HUM_OFF+sizeof(HumSave) <= META_OFF, "the humanoid block must end before the play-time block");
static void hum_save(void){ HumSave s; memset(&s,0,sizeof s); s.magic=0x4D554846u; s.style=hum.style; s.height=hum.height; s.build=hum.build; s.made=hum.made; for(int i=0;i<NHC;i++) s.col[i]=hum.col[i];
    s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+HUM_OFF,&s,sizeof s); lookx_save(); }
static void hum_load(void){ HumSave s; sram_read(slot_base(cur_slot)+HUM_OFF,&s,sizeof s);
    if(s.magic!=0x4D554846u || s.sum!=fnv(&s,sizeof s-4)) sram_read(slot_base(cur_slot)+0xA00,&s,sizeof s);        // an older save kept it at +0xA00
    if(s.magic!=0x4D554846u || s.sum!=fnv(&s,sizeof s-4)){ hum.style=0; hum.height=3; hum.build=1; hum.made=0; return; }
    hum.style=s.style&1; hum.height=s.height%5; hum.build=s.build%3; hum.made=s.made; for(int i=0;i<NHC;i++) hum.col[i]=s.col[i]; }
// ---------------------------------------------------------------- the humanoid creator: shown when a weird world turns you into one, and at your ship's mirror
static void hum_palette(void); static int hum_oam(int slot, int cx, int feet, int z, int facing, int lean, int sq, int shr, int sw, int tool);
// ================================================================= THE CHARACTER CREATOR (one screen for bugs and humanoids)
// UP/DOWN pick a row. LEFT/RIGHT change it (colour rows browse the presets). A mixes your own colour or types the name.
// L locks a row. R builds a matching palette from the colour you're on. SELECT surprises you (locked rows stay).
// B steps back through your last 8 changes. START is done. Ready-made LOOKS give you a place to start.
typedef struct { const char *nm, *nm2; u8 kind, n; u8 *v; const char *const *opt; u16 *c; u8 *dep; const char *const *opt2; } CRow;     // kind 0 name, 1 looks, 2 choice, 3 colour. opt2: a furry's names
typedef struct { const char *nm; u8 style, height, build, hair, face, beard, top, bot, shoes, hat, back; u16 col[9]; } CHumP;   // skin, hair, shoes, shirt, pants, belt, eyes, hat, back
typedef struct { const char *nm; u8 shell, shape, fin, pat, eye, mouth, ant, legs, legst, horn, wings, tail, hat; u16 col[10]; } CBugP;   // shell, head, eye, antenna, legs, marks, horn, wings, hat, outline
#define NCHP 16
static const CHumP cc_hp[NCHP]={
 {"EXPLORER",0,3,1, 0,0,0,4,0,1,6,3, {C15(224,172,130),C15(90,56,30),C15(100,70,40),C15(80,130,70),C15(120,100,70),C15(170,130,50),C15(60,120,60),C15(140,90,40),C15(90,120,60)}},
 {"KNIGHT",0,3,2, 0,4,3,8,0,1,7,1, {C15(235,190,150),C15(200,200,215),C15(70,70,80),C15(150,155,175),C15(60,65,90),C15(220,170,40),C15(70,110,200),C15(170,175,195),C15(190,40,50)}},
 {"ROGUE",0,2,0, 3,7,1,5,0,1,0,4, {C15(200,150,110),C15(30,28,36),C15(30,28,30),C15(60,50,70),C15(40,36,48),C15(150,40,50),C15(220,180,40),C15(60,50,70),C15(150,40,50)}},
 {"MAGE",0,2,0, 3,0,3,6,0,4,3,1, {C15(240,205,175),C15(235,235,245),C15(90,60,120),C15(110,60,180),C15(70,40,120),C15(250,220,60),C15(120,200,250),C15(90,50,170),C15(60,30,110)}},
 {"PUNK",0,1,1, 5,6,0,2,4,2,0,0, {C15(230,180,150),C15(240,100,180),C15(20,20,28),C15(30,30,36),C15(60,90,150),C15(250,220,60),C15(100,240,160),C15(30,30,36),C15(30,30,36)}},
 {"SPACER",0,3,1, 0,0,0,0,0,1,7,5, {C15(200,160,130),C15(50,40,30),C15(200,200,215),C15(240,240,250),C15(230,230,240),C15(240,130,30),C15(70,150,230),C15(240,240,250),C15(150,155,175)}},
 {"FOX",1,2,1, 8,2,0,4,5,3,0,4, {C15(230,120,40),C15(250,240,230),C15(250,240,230),C15(60,100,170),C15(70,70,90),C15(220,170,40),C15(40,40,50),C15(60,100,170),C15(220,60,60)}},
 {"WOLF",1,3,2, 0,4,5,9,0,1,0,0, {C15(120,126,140),C15(70,74,86),C15(220,222,230),C15(150,50,50),C15(50,50,60),C15(190,150,60),C15(240,200,60),C15(80,80,90),C15(80,80,90)}},
 {"BUNNY",1,1,0, 3,1,1,7,5,3,9,0, {C15(245,240,245),C15(250,170,200),C15(250,200,215),C15(240,130,180),C15(110,150,230),C15(250,230,120),C15(200,60,100),C15(250,120,170),C15(250,230,120)}},
 {"SPRITE",0,0,0, 7,2,1,7,5,4,10,2, {C15(250,220,200),C15(120,230,140),C15(250,250,120),C15(250,150,90),C15(90,190,170),C15(250,250,250),C15(120,60,200),C15(250,240,120),C15(200,240,255)}},
 {"ROYAL",0,3,1, 6,0,0,6,0,0,4,1, {C15(150,100,70),C15(30,24,20),C15(120,40,40),C15(120,40,150),C15(80,30,100),C15(250,210,60),C15(60,40,20),C15(250,210,60),C15(180,30,40)}},
 {"PIRATE",0,3,1, 3,7,3,9,3,1,2,0, {C15(220,170,130),C15(60,30,20),C15(40,30,20),C15(240,240,240),C15(60,60,90),C15(170,40,40),C15(40,30,20),C15(30,30,30),C15(170,40,40)}},
 {"ATHLETE",0,3,1, 4,0,1,2,1,2,8,0, {C15(190,130,90),C15(240,200,80),C15(240,80,60),C15(40,120,230),C15(30,30,40),C15(240,240,240),C15(60,120,60),C15(240,80,60),C15(40,120,230)}},
 {"CAT",1,1,0, 4,1,0,1,1,3,0,0, {C15(60,60,70),C15(60,60,70),C15(250,190,200),C15(240,200,60),C15(60,90,160),C15(240,240,240),C15(120,230,90),C15(240,200,60),C15(60,90,160)}},
 {"BEAR",1,4,2, 7,0,0,3,5,3,0,0, {C15(130,90,50),C15(110,70,40),C15(70,50,30),C15(200,60,60),C15(110,70,40),C15(240,200,80),C15(30,20,10),C15(200,60,60),C15(200,60,60)}},
 {"ANGEL",0,2,0, 9,2,1,6,0,4,10,2, {C15(250,225,200),C15(250,230,140),C15(230,200,150),C15(250,250,255),C15(240,240,250),C15(250,220,100),C15(100,160,240),C15(250,230,120),C15(250,250,255)}}};
#define NCBP 18
static const CBugP cc_bp[NCBP]={
 {"CLASSIC",1,0,0,0,0,0,0,1,0,0,0,0,0, {C15(92,108,148),C15(70,84,120),C15(240,240,250),C15(14,16,28),C15(60,70,100),C15(200,200,215),C15(220,170,40),C15(190,225,255),C15(210,60,70),C15(14,16,28)}},
 {"LADYBUG",0,1,0,2,1,1,0,1,0,0,2,0,0, {C15(210,40,50),C15(20,20,28),C15(240,240,250),C15(20,20,28),C15(30,30,40),C15(20,20,28),C15(220,170,40),C15(230,235,255),C15(210,60,70),C15(14,14,20)}},
 {"WASP",2,0,0,1,0,2,1,1,0,0,1,1,0, {C15(250,210,50),C15(30,28,24),C15(230,60,40),C15(30,28,24),C15(40,36,30),C15(30,28,24),C15(220,170,40),C15(220,240,255),C15(30,28,24),C15(20,18,14)}},
 {"STAG",2,0,2,0,0,3,2,2,1,0,0,0,0, {C15(110,60,30),C15(70,40,20),C15(240,240,250),C15(40,24,12),C15(80,44,22),C15(150,100,50),C15(220,180,90),C15(200,180,150),C15(150,100,50),C15(30,16,8)}},
 {"FIREFLY",2,0,1,3,5,0,1,1,0,0,1,0,0, {C15(40,36,60),C15(250,150,40),C15(255,255,200),C15(250,150,40),C15(60,50,80),C15(250,230,70),C15(220,170,40),C15(255,250,190),C15(250,150,40),C15(14,12,24)}},
 {"MOTH",1,0,1,8,1,0,3,0,2,0,3,0,0, {C15(210,200,215),C15(120,100,130),C15(40,30,50),C15(240,220,250),C15(150,130,170),C15(150,130,170),C15(220,170,40),C15(200,190,170),C15(150,130,170),C15(50,40,60)}},
 {"BUTTERFLY",0,0,3,0,1,1,4,1,0,0,3,0,0, {C15(40,40,60),C15(30,30,40),C15(240,240,250),C15(30,30,40),C15(30,30,40),C15(250,250,250),C15(220,170,40),C15(250,140,40),C15(210,60,70),C15(14,14,20)}},
 {"RHINO",2,1,2,0,0,0,1,1,1,3,0,0,0, {C15(90,60,40),C15(60,40,26),C15(240,240,250),C15(40,26,16),C15(60,40,26),C15(150,110,80),C15(170,120,70),C15(200,180,150),C15(150,110,80),C15(24,14,8)}},
 {"UNICORN",1,0,3,11,5,1,0,1,0,4,5,3,0, {C15(245,245,255),C15(250,220,235),C15(120,90,220),C15(250,180,220),C15(240,200,230),C15(250,160,210),C15(250,220,120),C15(200,230,255),C15(250,160,210),C15(120,90,160)}},
 {"KNIGHT",1,1,2,0,4,2,1,1,3,0,0,0,10, {C15(170,175,195),C15(120,125,145),C15(250,250,250),C15(40,40,50),C15(80,80,95),C15(190,40,50),C15(220,170,40),C15(190,225,255),C15(200,200,215),C15(20,20,30)}},
 {"PIRATE",1,0,0,1,7,1,2,1,3,0,0,4,2, {C15(240,240,240),C15(210,170,130),C15(250,250,250),C15(40,30,20),C15(40,30,20),C15(190,40,40),C15(220,170,40),C15(190,225,255),C15(30,30,30),C15(20,14,10)}},
 {"ROYAL",1,0,3,9,1,1,4,1,0,0,0,0,4, {C15(120,40,150),C15(250,210,170),C15(60,40,20),C15(250,210,60),C15(80,30,100),C15(250,210,60),C15(250,210,60),C15(190,225,255),C15(250,210,60),C15(40,14,50)}},
 {"PARTY",0,0,0,6,2,1,5,1,0,0,1,0,5, {C15(80,200,240),C15(250,200,90),C15(250,250,250),C15(240,80,160),C15(80,90,200),C15(250,240,80),C15(220,170,40),C15(250,160,220),C15(240,80,160),C15(20,20,40)}},
 {"CHEF",1,0,1,0,0,1,0,1,0,0,0,0,9, {C15(240,140,60),C15(250,210,170),C15(40,30,20),C15(40,30,20),C15(60,40,30),C15(250,250,250),C15(220,170,40),C15(190,225,255),C15(250,250,250),C15(40,26,16)}},
 {"DEMON",2,3,2,8,4,4,6,2,2,2,4,1,0, {C15(40,20,30),C15(200,40,40),C15(250,220,60),C15(200,40,40),C15(40,20,30),C15(200,40,40),C15(240,230,210),C15(120,30,40),C15(200,40,40),C15(10,4,8)}},
 {"GHOST",1,1,1,0,6,0,7,0,0,0,5,0,7, {C15(220,225,240),C15(245,245,255),C15(90,100,200),C15(245,245,255),C15(200,200,220),C15(245,245,255),C15(220,170,40),C15(220,235,255),C15(250,230,120),C15(120,130,170)}},
 {"ROBOT",1,2,2,4,7,2,4,2,1,0,0,0,10, {C15(150,160,170),C15(110,120,130),C15(80,240,250),C15(80,240,250),C15(90,95,105),C15(80,240,250),C15(220,170,40),C15(190,225,255),C15(170,175,185),C15(30,34,40)}},
 {"VIKING",2,4,0,3,4,0,1,2,3,0,0,0,11, {C15(150,90,50),C15(240,200,120),C15(70,120,200),C15(120,80,40),C15(100,70,40),C15(220,200,120),C15(240,230,210),C15(190,225,255),C15(170,175,195),C15(40,24,10)}}};
static void cc_hsv(u16 c, int *h, int *s, int *v){
    int r=(c&31)<<3, g=((c>>5)&31)<<3, b=((c>>10)&31)<<3, mx=r>g?r:g, mn=r<g?r:g; if(b>mx) mx=b; if(b<mn) mn=b;
    int d=mx-mn, hh=0; *v=mx; *s=mx?d*255/mx:0;
    if(d){ if(mx==r) hh=60*(g-b)/d; else if(mx==g) hh=120+60*(b-r)/d; else hh=240+60*(r-g)/d; if(hh<0) hh+=360; } *h=hh; }
static u16 cc_rgb(int h, int s, int v){
    h%=360; if(h<0) h+=360; if(s>255) s=255; if(v>255) v=255;
    int i=h/60, f=(h%60)*255/60, p=v*(255-s)/255, q=v*(255-s*f/255)/255, t=v*(255-s*(255-f)/255)/255, r, g, b;
    switch(i){ case 0: r=v;g=t;b=p; break; case 1: r=q;g=v;b=p; break; case 2: r=p;g=v;b=t; break; case 3: r=p;g=q;b=v; break; case 4: r=t;g=p;b=v; break; default: r=v;g=p;b=q; }
    return (u16)((r>>3)|((g>>3)<<5)|((b>>3)<<10)); }
static const char *const cc_sch_nm[9]={"ANALOGOUS","COMPLEMENT","TRIAD","SPLIT","MONO","PASTEL","NEON","EARTH","NIGHT"};
static const s16 cc_dh[9][6]={{30,-30,60,-60,15,-15},{180,0,180,0,180,0},{120,240,120,240,0,180},{150,210,150,210,180,0},{0,0,0,0,0,0},{0,40,80,120,200,300},{0,60,120,180,240,300},{25,35,95,15,45,200},{250,270,230,300,210,50}};
static const u8 cc_ss[9][6]={{200,200,170,170,220,220},{210,160,230,120,190,255},{220,220,170,170,255,140},{200,220,180,160,230,120},{230,200,170,140,110,80},{70,80,60,70,90,60},{255,255,255,255,255,255},{160,140,120,180,100,70},{200,180,220,150,240,200}};
static const u8 cc_vv[9][6]={{210,160,230,140,190,120},{220,150,120,230,170,90},{220,200,150,130,100,240},{220,150,230,120,190,90},{90,130,170,210,240,255},{250,245,250,240,250,245},{255,255,255,255,255,255},{110,150,130,90,190,160},{60,90,50,110,70,230}};
static int cc_pidx(u16 c){ for(int i=0;i<NPRESET;i++) if(preset_col[i]==c) return i; return -1; }
static void cc_scheme(CRow *R, int nr, int a, u16 lock, int sch){        // paint every unlocked colour row to go with the colour in row a
    int h0,s0,v0; cc_hsv(*R[a].c,&h0,&s0,&v0); int k=0;
    for(int i=0;i<nr;i++) if(R[i].kind==3 && i!=a && !((lock>>i)&1)){ int sl=k%6; k++;
        *R[i].c = cc_rgb(sch>=7 ? cc_dh[sch][sl] : h0+cc_dh[sch][sl], cc_ss[sch][sl], cc_vv[sch][sl]); } }
static void cc_set8(CRow *R, int nr, u16 lock, u8 *p, int val){ for(int i=0;i<nr;i++) if(R[i].v==p){ if(!((lock>>i)&1)) *p=(u8)val; return; } *p=(u8)val; }
static void cc_set16(CRow *R, int nr, u16 lock, u16 *p, u16 val){ for(int i=0;i<nr;i++) if(R[i].c==p){ if(!((lock>>i)&1)) *p=val; return; } *p=val; }
static void cc_name(volatile u16 *pg, char *nm){
    char b[9]; scpy(b,nm); int n=textlen(b), cur=0, t=0; if(n<1){ scpy(b,"PIP"); n=3; }
    oam_hide_all(); oam_flush(); wait_release();
    while(1){
        box(pg,36,52,168,56); rect(pg,37,53,166,12,6); ctext(pg,120,56,"WHAT IS YOUR NAME?",C_GOLD);
        for(int k=0;k<8;k++){ char ch[2]={ k<n?b[k]:'_',0 }; int c = k==cur ? C_GOLD : k<n ? C_WHITE : C_DGREY; text(pg,80+k*10,72,ch,c,2); if(k==cur && (t&16)) rect(pg,80+k*10,89,8,1,C_GOLD); }
        ui_chint(pg,120,94,"UP/DOWN: LETTER   LEFT/RIGHT: MOVE   A: OK",C_GREY);
        vsync(); read_keys(); t++;
        if(HIT(K_UP)||HIT(K_DOWN)){ if(cur>=n){ b[cur]='A'; n=cur+1; b[n]=0; } else { char c=b[cur]; c = HIT(K_UP) ? (c=='Z'?' ':c==' '?'A':c+1) : (c=='A'?' ':c==' '?'Z':c-1); b[cur]=c; } sfx(7); }
        if(HIT(K_RIGHT) && cur<7){ cur++; if(cur>=n){ b[cur]='A'; n=cur+1; b[n]=0; } }
        if(HIT(K_LEFT) && cur>0) cur--;
        if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ while(n>0 && b[n-1]==' ') n--; b[n]=0; if(n==0){ scpy(b,"PIP"); } scpy(nm,b); sfx(2); wait_release(); return; }
    }
}
typedef struct { BugLook l; BugX b; Hum h; HumX x; } CCSnap;                      // everything the creator can change (for undo)
static void cc_take(CCSnap *s){ s->l=look; s->b=bugx; s->h=hum; s->x=humx; }
static void cc_put(const CCSnap *s){ look=s->l; bugx=s->b; hum=s->h; humx=s->x; }
static int cc_run(volatile u16 *pg, int human, int first){
    static const char *const body_n[2]={"HUMAN","FURRY"}, *const ht_n[5]={"TINY","SHORT","AVERAGE","TALL","TOWERING"}, *const bd_n[3]={"SLIM","NORMAL","BROAD"},
        *const hair_n[10]={"SHORT","BALD","SPIKY","LONG","PONYTAIL","MOHAWK","AFRO","BUN","BOB","CURLY"}, *const ears_n[10]={"POINTY","ROUND","FLOPPY","BUNNY","CAT","HORNS","FLUFFY","BEAR","LONG","NONE"},
        *const face_n[8]={"DOTS","BIG","HAPPY","SLEEPY","ANGRY","GLASSES","SHADES","EYEPATCH"}, *const beard_n[6]={"NONE","SMILE","MOUSTACHE","BEARD","GOATEE","FANGS"},
        *const top_n[10]={"SHIRT","T-SHIRT","TANK TOP","VEST","JACKET","HOODIE","ROBE","DRESS","ARMOUR","STRIPES"}, *const bot_n[6]={"TROUSERS","SHORTS","SKIRT","BAGGY","STRIPED","NONE"},
        *const shoe_n[5]={"SHOES","BOOTS","TRAINERS","BARE FEET","SANDALS"}, *const hhat_n[12]={"NONE","CAP","TOP HAT","WIZARD","CROWN","BEANIE","COWBOY","HELMET","HEADBAND","BOW","HALO","PARTY HAT"},
        *const back_n[6]={"NONE","CAPE","WINGS","BACKPACK","SCARF","JETPACK"},
        *const shell_n[3]={"SHORT","NORMAL","LONG"}, *const shape_n[5]={"ROUND","DOME","FLAT","SPIKY","HUMPED"}, *const fin_n[4]={"SHINY","MATTE","METAL","PEARL"},
        *const pat_n[12]={"PLAIN","STRIPED","SPOTTED","BANDED","CHECKED","ZIGZAG","DOTTED","DIAMONDS","TIGER","HALVES","SPECKLED","RINGS"},
        *const eye_n[8]={"SMALL","BIG","GOOGLY","SLEEPY","ANGRY","STARRY","ONE BIG EYE","SHADES"}, *const mouth_n[6]={"NONE","SMILE","JAWS","PINCERS","FANGS","TONGUE"},
        *const ant_n[8]={"CURLED","STRAIGHT","DROOPY","FEATHERY","CLUBBED","ZIGZAG","LONG WHIPS","NONE"}, *const legs_n[3]={"FOUR","SIX","EIGHT"}, *const legst_n[4]={"THIN","CHUNKY","HAIRY","BOOTS"},
        *const horn_n[6]={"NONE","HORN","TWIN HORNS","RHINO","UNICORN","ANTLERS"}, *const wing_n[6]={"NONE","LITTLE","BEETLE","BUTTERFLY","BAT","FAIRY"}, *const tail_n[5]={"NONE","STINGER","SPIKES","CURLY","LONG"},
        *const hat_n[12]={"NONE","CAP","TOP HAT","BOW","CROWN","PARTY HAT","FLOWER","HALO","BEANIE","CHEF","HELMET","VIKING"};
    CRow R[40]; int nr=0;
    #define CR(a,b,c,d,e,f,g,h) R[nr++]=(CRow){a,b,c,d,e,f,g,h,0}
    CR("NAME",0,0,0,0,0,0,0); CR("LOOKS",0,1,0,0,0,0,0);
    if(human){
        CR("BODY",0,2,2,&hum.style,body_n,0,0); CR("HEIGHT",0,2,5,&hum.height,ht_n,0,0); CR("BUILD",0,2,3,&hum.build,bd_n,0,0);
        CR("HAIR","EARS",2,10,&humx.hair,hair_n,0,0); R[nr-1].opt2=ears_n; CR("FACE",0,2,8,&humx.face,face_n,0,0); CR("MOUTH",0,2,6,&humx.beard,beard_n,0,0);
        CR("TOP",0,2,10,&humx.top,top_n,0,0); CR("BOTTOM",0,2,6,&humx.bot,bot_n,0,0); CR("SHOES","FEET",2,5,&humx.shoes,shoe_n,0,0); CR("HAT",0,2,12,&humx.hat,hhat_n,0,0); CR("BACK",0,2,6,&humx.back,back_n,0,0);
        CR("SKIN","FUR",3,0,0,0,&hum.col[HC_SKIN],0); CR("HAIR","EARS+TAIL",3,0,0,0,&hum.col[HC_HAIR],0); CR("SHOES","MUZZLE",3,0,0,0,&hum.col[HC_ACC],0);
        CR("SHIRT",0,3,0,0,0,&hum.col[HC_SHIRT],0); CR("PANTS",0,3,0,0,0,&hum.col[HC_PANTS],0); CR("BELT",0,3,0,0,0,&hum.col[HC_BELT],0); CR("EYES",0,3,0,0,0,&hum.col[HC_EYES],0);
        CR("HAT COL",0,3,0,0,0,&humx.c_hat,&humx.hat); CR("BACK COL",0,3,0,0,0,&humx.c_back,&humx.back);
    } else {
        CR("LENGTH",0,2,3,&look.shell,shell_n,0,0); CR("SHAPE",0,2,5,&bugx.shape,shape_n,0,0); CR("FINISH",0,2,4,&bugx.finish,fin_n,0,0); CR("PATTERN",0,2,12,&look.pat,pat_n,0,0);
        CR("EYES",0,2,8,&look.eye,eye_n,0,0); CR("MOUTH",0,2,6,&bugx.mouth,mouth_n,0,0); CR("ANTENNAE",0,2,8,&look.ant,ant_n,0,0); CR("LEGS",0,2,3,&look.legs,legs_n,0,0); CR("LEG STYLE",0,2,4,&bugx.legst,legst_n,0,0);
        CR("HORN",0,2,6,&look.horn,horn_n,0,0); CR("WINGS",0,2,6,&bugx.wings,wing_n,0,0); CR("TAIL",0,2,5,&bugx.tail,tail_n,0,0); CR("HAT",0,2,12,&bugx.hat,hat_n,0,0);
        CR("SHELL",0,3,0,0,0,&look.col[LC_SHELL],0); CR("MARKINGS",0,3,0,0,0,&look.col[LC_MARK],&look.pat); CR("HEAD",0,3,0,0,0,&look.col[LC_HEAD],0);
        CR("EYE COL",0,3,0,0,0,&look.col[LC_EYE],0); CR("ANT COL",0,3,0,0,0,&look.col[LC_ANT],0); CR("LEG COL",0,3,0,0,0,&look.col[LC_LEGS],0); CR("HORN COL",0,3,0,0,0,&look.col[LC_HORN],0);
        CR("WING COL",0,3,0,0,0,&look.col[LC_WING],&bugx.wings); CR("HAT COL",0,3,0,0,0,&bugx.c_hat,0); CR("OUTLINE",0,3,0,0,0,&bugx.c_line,0);
    }
    int np = human ? NCHP : NCBP;
    char nm[9]; scpy(nm,look.name); if(textlen(nm)<1) scpy(nm,"PIP");
    EW static CCSnap before, hist[8]; char bnm[9]; cc_take(&before); scpy(bnm,nm);
    char hnm[8][9]; int hn=0, hp=0;
    #define PUSH() do{ cc_take(&hist[hp]); scpy(hnm[hp],nm); hp=(hp+1)&7; if(hn<8) hn++; }while(0)
    #define APPLY() do{ if(human){ hum_palette(); hum_build(); } else bug_build(); }while(0)
    int sel=0, top=0, dirty=1, t=0, pi=-1, sch=-1, rep=0, note_t=0; u16 lock=0; char note[32]; note[0]=0;
    wait_release(); oam_hide_all(); oam_flush();
    if(human){ if(!hum.made) hum_from_look(); hum.made=1; hum_palette(); hum_build(); } else bug_build();
    while(1){
        if(dirty){ dirty=0; char b[40];
            panel(pg,12,20,216,136, human ? (first?"YOU ARE A HUMANOID NOW":"THE MIRROR: HUMANOID") : (first?"MAKE YOUR BUG":"THE MIRROR"));
            if(sel<top) top=sel; if(sel>=top+8) top=sel-7;
            for(int j=0;j<8 && top+j<nr;j++){ int i=top+j, y=36+j*9; CRow *r=&R[i]; int on=!r->dep || *r->dep;
                if(i==sel) rect(pg,16,y-2,116,9,6);
                text(pg,20,y,(human&&hum.style&&r->nm2)?r->nm2:r->nm, i==sel?C_WHITE:on?C_GREY:C_DGREY,1);
                int vc = i==sel?C_GOLD:C_GREY;
                if(r->kind==0) text(pg,64,y,nm,vc,1);
                else if(r->kind==1){ scpy(b,"<"); scat(b, pi>=0 ? (human?cc_hp[pi].nm:cc_bp[pi].nm) : "READY-MADE"); scat(b,">"); text(pg,64,y,b,vc,1); }
                else if(r->kind==2){ scpy(b,"<"); scat(b,(human&&hum.style&&r->opt2)?r->opt2[*r->v%r->n]:r->opt[*r->v%r->n]); scat(b,">"); text(pg,64,y,b,on?vc:C_DGREY,1); }
                else if(on){ BG_PAL[224+j]=*r->c; rect(pg,64,y-1,24,8,224+j); frame1(pg,64,y-1,24,8,i==sel?C_WHITE:C_DGREY); int pc=cc_pidx(*r->c); text(pg,92,y,pc>=0?preset_name[pc]:"CUSTOM",i==sel?C_WHITE:C_GREY,1); }
                else text(pg,64,y,"(OFF)",C_DGREY,1);
                if(i>=1) rect(pg,124,y,5,5,((lock>>i)&1)?C_GOLD:C_DGREY); }
            if(top>0) text(pg,134,36,"^",C_CYAN,1); if(top+8<nr) text(pg,134,100,"V",C_CYAN,1);
            rect(pg,146,38,74,62,1); frame1(pg,146,38,74,62,C_DGREY);
            if(human) for(int k=0;k<5;k++) rect(pg,149,94-k*13,4,2,k==hum.height?C_GOLD:C_DGREY);
            ctext(pg,183,104,nm,C_GOLD);
            CRow *cr=&R[sel]; int on=!cr->dep || *cr->dep;
            ctext(pg,120,113, cr->kind==0 ? "A: TYPE A NAME" : cr->kind==1 ? "LEFT/RIGHT: TRY A READY-MADE LOOK" : cr->kind==2 ? "LEFT/RIGHT: CHANGE IT" : on ? "LEFT/RIGHT: BROWSE   A: MIX YOUR OWN" : "TURN ITS PART ON FIRST", C_GREY);
            ctext(pg,120,122,"L: LOCK  R: MATCH COLOURS  SEL: SURPRISE", C_CYAN);
            if(hn){ scpy(b,"B: UNDO ("); ncat(b,hn); scat(b,")   START: ALL DONE"); } else scpy(b, first ? "B: CANCEL   START: ALL DONE" : "B: BACK   START: ALL DONE");
            ctext(pg,120,131,b,C_CYAN);
            if(note_t) ctext(pg,120,140,note,C_GOLD); else if(first) ctext(pg,120,140,"CHANGE THIS ANYTIME AT YOUR SHIP'S MIRROR",C_DGREY); }
        t++; int facing=(t/150)&1;
        if(human){ int n=hum_oam(0,183,95-((t>>5)&1),1,facing,0,0,0,0,-1); shadow_oam[n].a0=((95-2)&255)|(1<<14); shadow_oam[n].a1=((183-8)&511); shadow_oam[n].a2=T_SHADOW; }
        else { int wf=(t>>3)&1; set_affine(31, facing?170:-170, 0, 0, 170);
            bug_obj(0,183,76-wf,31,170); shadow_oam[0].a2=bug_tile(wf)|BUG_BANK;
            shadow_oam[1].a0=((88-2)&255)|(1<<14); shadow_oam[1].a1=((183-8)&511); shadow_oam[1].a2=T_SHADOW; }
        vsync(); oam_flush(); read_keys();
        if(note_t && --note_t==0) dirty=1;
        CRow *cr=&R[sel]; int on=!cr->dep || *cr->dep;
        if(HIT(K_UP)){ sel=(sel+nr-1)%nr; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%nr; dirty=1; sfx(7); }
        int d=0, fresh=0; if(HELD(K_LEFT)||HELD(K_RIGHT)){ rep++; fresh=(rep==1); if(fresh || (rep>14 && !(rep&3) && (cr->kind==3||cr->kind==1))) d=HELD(K_RIGHT)?1:-1; } else rep=0;
        if(HIT(K_A) && cr->kind==2) d=1, fresh=1;
        if(d){
            if(cr->kind==2){ PUSH(); *cr->v=(u8)((*cr->v+d+cr->n)%cr->n); pi=-1; APPLY(); dirty=1; sfx(2); }
            else if(cr->kind==3 && on){ if(fresh) PUSH(); int p=cc_pidx(*cr->c); p = p<0 ? (d>0?0:NPRESET-1) : (p+d+NPRESET)%NPRESET; *cr->c=preset_col[p]; pi=-1; APPLY(); dirty=1; sfx(7); }
            else if(cr->kind==1){ if(fresh) PUSH(); pi = pi<0 ? (d>0?0:np-1) : (pi+d+np)%np;
                if(human){ const CHumP *p=&cc_hp[pi]; cc_set8(R,nr,lock,&hum.style,p->style); cc_set8(R,nr,lock,&hum.height,p->height); cc_set8(R,nr,lock,&hum.build,p->build);
                    cc_set8(R,nr,lock,&humx.hair,p->hair); cc_set8(R,nr,lock,&humx.face,p->face); cc_set8(R,nr,lock,&humx.beard,p->beard); cc_set8(R,nr,lock,&humx.top,p->top); cc_set8(R,nr,lock,&humx.bot,p->bot); cc_set8(R,nr,lock,&humx.shoes,p->shoes); cc_set8(R,nr,lock,&humx.hat,p->hat); cc_set8(R,nr,lock,&humx.back,p->back);
                    for(int k=0;k<7;k++) cc_set16(R,nr,lock,&hum.col[k],p->col[k]); cc_set16(R,nr,lock,&humx.c_hat,p->col[7]); cc_set16(R,nr,lock,&humx.c_back,p->col[8]); }
                else { const CBugP *p=&cc_bp[pi]; cc_set8(R,nr,lock,&look.shell,p->shell); cc_set8(R,nr,lock,&bugx.shape,p->shape); cc_set8(R,nr,lock,&bugx.finish,p->fin); cc_set8(R,nr,lock,&look.pat,p->pat);
                    cc_set8(R,nr,lock,&look.eye,p->eye); cc_set8(R,nr,lock,&bugx.mouth,p->mouth); cc_set8(R,nr,lock,&look.ant,p->ant); cc_set8(R,nr,lock,&look.legs,p->legs); cc_set8(R,nr,lock,&bugx.legst,p->legst);
                    cc_set8(R,nr,lock,&look.horn,p->horn); cc_set8(R,nr,lock,&bugx.wings,p->wings); cc_set8(R,nr,lock,&bugx.tail,p->tail); cc_set8(R,nr,lock,&bugx.hat,p->hat);
                    for(int k=0;k<8;k++) cc_set16(R,nr,lock,&look.col[k],p->col[k]); cc_set16(R,nr,lock,&bugx.c_hat,p->col[8]); cc_set16(R,nr,lock,&bugx.c_line,p->col[9]); }
                APPLY(); dirty=1; sfx(6); }
        }
        if(HIT(K_A)){
            if(cr->kind==0){ PUSH(); cc_name(pg,nm); dirty=1; }
            else if(cr->kind==3 && on){ u16 old=*cr->c; PUSH(); oam_hide_all(); oam_flush(); *cr->c=pick_colour(pg,(human&&hum.style&&cr->nm2)?cr->nm2:cr->nm,old);
                if(*cr->c==old){ hp=(hp+7)&7; hn--; } else { pi=-1; APPLY(); } dirty=1; } }
        if(HIT(K_L) && sel>=1){ lock^=(u16)(1<<sel); dirty=1; sfx(7); scpy(note,((lock>>sel)&1)?"LOCKED: SURPRISE AND LOOKS SKIP IT":"UNLOCKED"); note_t=60; }
        if(HIT(K_R)){ int a=-1; if(cr->kind==3) a=sel; else for(int i=0;i<nr;i++) if(R[i].kind==3){ a=i; break; }
            if(a>=0){ sch=(sch+1)%9; PUSH(); cc_scheme(R,nr,a,lock,sch); pi=-1; APPLY(); scpy(note,"PALETTE: "); scat(note,cc_sch_nm[sch]); note_t=70; dirty=1; sfx(6); } }
        if(HIT(K_SEL)){ PUSH(); for(int i=0;i<nr;i++) if(R[i].kind==2 && !((lock>>i)&1)) *R[i].v=(u8)rndi(R[i].n);
            int a=-1; for(int i=0;i<nr;i++) if(R[i].kind==3 && !((lock>>i)&1)){ a=i; break; }
            if(a>=0){ *R[a].c=cc_rgb(rndi(360),140+rndi(116),120+rndi(136)); cc_scheme(R,nr,a,lock,rndi(9)); }
            pi=-1; APPLY(); scpy(note,"SURPRISE!"); note_t=50; dirty=1; sfx(6); }
        if(HIT(K_START)) break;
        if(HIT(K_B)){
            if(hn>0){ hp=(hp+7)&7; hn--; cc_put(&hist[hp]); scpy(nm,hnm[hp]); pi=-1; APPLY(); scpy(note,"UNDONE"); note_t=40; dirty=1; sfx(4); }
            else { cc_put(&before); scpy(nm,bnm); if(human){ hum_palette(); hum_build(); form_palette(); } else bug_build(); wait_release(); oam_hide_all(); oam_flush(); return 0; } }
    }
    scpy(look.name,nm);
    if(human){ hum.made=1; hum_save(); form_palette(); }
    sfx(6); wait_release(); oam_hide_all(); oam_flush(); return 1;
}
static int human_creator(volatile u16 *pg, int first){ return cc_run(pg,1,first); }
static int make_bug(volatile u16 *pg, int first){ return cc_run(pg,0,first); }
// how to play: lively pages with button badges, over the title planet, with your bug walking along
typedef struct { const char *btn; const char *txt; } HelpLine;
static const HelpLine help_pg[5][7]={
  {{"D-PAD","WALK OVER THE HILLS"},{"SEL","TAP: SWITCH 3RD / 1ST PERSON VIEW"},{"A","DOOR, SHIP, CAGE, SLUG, PLANT, ORE"},{"B","USE THE PICKED ITEM"},{"L R","PICK AN ITEM (1ST PERSON: SIDESTEP)"},{"SEL+LR","ARROW: DOOR, SHIP OR NEAREST ORE"},{"SEL+LT RT","3RD PERSON: TURN THE CAMERA"}},
  {{"UP DN","WALK FORWARD AND BACK"},{"LT RT","TURN (TAP A LITTLE, HOLD FOR MORE)"},{"L R","STEP SIDEWAYS. TAP TWICE TO DODGE"},{"A","BITE. HOLD IT. THREE IN A ROW = CRUNCH"},{"A","OPEN CAGES, TAKE STAIRS, FISH IN POOLS"},{"SEL","CAST A SPELL. HOLD + L/R PICKS THE SPELL"},{"START","THE MAP OF THIS FLOOR"}},
  {{"D-PAD","STEER THE SHIP"},{"B","THRUST.  R: BRAKE"},{"A","SHOOT PIRATES AND ROCKS"},{"A","NEAR THE PLANET: LAND.  AT THE BOX: DOCK"},{"SEL","THE GALAXY MAP. PICK A STAR, A: JUMP"},{"","JUMPS BURN FUEL. BUY FUEL AT STATIONS"},{"","STATIONS BUY THE GOODS YOU MINE"}},
  {{"!","A RED ! MEANS A MONSTER IS ABOUT TO BITE"},{"","STEP BACK OR DODGE, THEN BITE IT"},{"","BEETLES CHARGE. SIDESTEP: THEY HIT THE WALL"},{"","BUDDY FIGHTS BESIDE YOU. FEED HIM FISH"},{"","EAT CRUMBS WHEN THE GREEN BAR IS LOW"},{"","FAINTING LOSES YOUR BAG (NOT ON EASY)"},{"","SAVE: START > SYSTEM (DUNGEONS TOO)"}},
  {{"1","FOLLOW THE ARROW TO THE DOOR. GO DOWN"},{"2","BEAT THE BOSS AT THE BOTTOM"},{"3","AT YOUR SHIP: AUTOPILOT TO THE NEXT STAR"},{"","MINE ORE. SELL IT, OR UPGRADE GEAR"},{"","DRONE OUTPOSTS: FIND THE KEYCARD, SMASH THE CORE"},{"","EACH CORE GIVES BOLT A MEMORY BACK"},{"","BEAT A BOSS, THEN GO BACK DOWN ITS DOOR: NEW TIER"}}};
static const char *const help_ttl[5]={"ON A PLANET","IN A DUNGEON","IN SPACE","STAYING ALIVE","YOUR QUEST"};
static int badge_col(const char *b){ if(!b[0]) return -1; if(b[0]=='A'&&!b[1]) return C_RED; if(b[0]=='B'&&!b[1]) return C_CYAN; if(b[0]=='!') return C_RED; if(b[0]>='1'&&b[0]<='3') return C_GOLD; if(b[0]=='S') return C_GOLD; if(b[0]=='D') return C_GREEN; return C_GREY; }
static void help_screen(volatile u16 *pg){
    int p=0, dirty=1, t=0; wait_release(); bug_build();
    while(1){
        if(dirty){ dirty=0;
            title_restore(pg,18,160);
            ui_panel(pg,8,22,224,126); rect(pg,10,24,220,11,U_RAISE); rect(pg,10,24,220,1,U_SHINE); rect(pg,10,35,220,1,U_EDGE); rect(pg,13,27,2,5,U_ACCENT);
            char b[24]; scpy(b,"HOW TO PLAY  "); ncat(b,p+1); scat(b,"/5"); stext(pg,18,27,b,U_SOFT);
            { int w=textlen(help_ttl[p])*4; stext(pg,226-w,27,help_ttl[p],C_GOLD); }
            for(int i=0;i<7;i++){ const HelpLine *h=&help_pg[p][i]; int y=41+i*13; int c=badge_col(h->btn);
                if(c>=0){ int w=textlen(h->btn)*4+6; rect(pg,14,y-2,w,9,c); text(pg,17,y,h->btn,0,1); }
                else { rect(pg,16,y+2,3,2,C_DGREY); }
                text(pg,50,y,h->txt,C_WHITE,1); }
            for(int k=0;k<5;k++) rect(pg,106+k*7,140,4,3, k==p?C_GOLD:C_DGREY);
            text(pg,14,140,"< >: PAGE",C_CYAN,1); rtext(pg,226,140,"B: BACK",C_CYAN); }
        t++; int wf=(t>>3)&1; int bx=((t>>1)%300)-40;                    // the bug trots along the bottom
        set_affine(31, -128, 0, 0, 128);
        bug_obj(0,bx,150-16-wf,31,128); shadow_oam[0].a2=bug_tile(wf)|BUG_BANK;
        vsync(); oam_flush(); read_keys(); twinkle(pg,t);
        if(HIT(K_RIGHT)||HIT(K_R)||HIT(K_A)){ if(p<4){ p++; dirty=1; sfx(7); } else if(HIT(K_A)) break; }
        if(HIT(K_LEFT)||HIT(K_L)){ if(p>0){ p--; dirty=1; sfx(7); } }
        if(HIT(K_B)||HIT(K_START)) break;
    }
    oam_hide_all(); oam_flush(); wait_release();
}
// the end of the story: the Ant King is beaten
static void credits(void){
    kw_unlock();                                                                        // the game is beaten: Kart World opens
    volatile u16 *pg=VRAM; music(5);
    rect(pg,0,0,240,160,0);
    for(int k=0;k<70;k++){ int x=rndi(240), y=rndi(160); pix(pg,x,y,k&3?C_DGREY:C_WHITE); }
    text(pg,120-7*6+1,9,"THE END",C_DGREY,3); text(pg,120-7*6,8,"THE END",C_GOLD,3);
    char b[48];
    char l1[40]; scpy(l1,bug_name); scat(l1," FREED THE BUGS.");
    const char *ln[6]={"...OF THE BEGINNING.",l1,"THE ANT KING IS BEATEN. THE DAWN STIRS.","BUGSTAR","A GAME BY DIPPINN PRODUCTIONS","MADE WITH CLAUDE (AI)"};
    static const u8 ly[6]={28,42,50,64,72,80};
    for(int i=0;i<6;i++){ int c = i==3?C_GOLD : i==0?C_GREY : C_WHITE; text(pg,120-textlen(ln[i])*2,ly[i],ln[i],c,1); }
    scpy(b,"LEVEL "); ncat(b,level); scat(b,"   BUGS FREED "); ncat(b,bugs_freed); scat(b,"   FAME "); ncat(b,fame); text(pg,120-textlen(b)*2,94,b,C_CYAN,1);
    scpy(b,"TIME PLAYED "); { char t[16]; fmt_time(t,play_secs()); scat(b,t); } text(pg,120-textlen(b)*2,102,b,C_CYAN,1);
    text(pg,120-22*2,116,"THANK YOU FOR PLAYING!",C_GREEN,1);
    text(pg,120-31*2,128,"NOW THE ENDLESS GALAXY IS OPEN.",C_WHITE,1);
    text(pg,120-31*2,136,"AND THE TECHNOBEAST IS AWAKE...",C_RED,1);
    fade(0); wait_release();
    for(int t=0;;t++){ vsync(); read_keys(); if(t>90 && (HIT(K_A)||HIT(K_START))) break;
        if(t>90) text(pg,120-16*2,150,"A: KEEP PLAYING",(t&32)?C_CYAN:C_DGREY,1); }
    wait_release(); fade(1); music(0);
}

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
        if(scene==2){ sys_say("NO SAVING IN SPACE",C_RED); sfx(0); return; }
        if(scene==1 && forge_on){ sys_say("NO SAVING IN A FORGE RUN",C_RED); sfx(0); return; }
        if(scene==1 && base_mode){ sys_say("NO SAVING IN AN OUTPOST",C_RED); sfx(0); return; }
        if(cur==1){ int t=slot_picker(pg,SP_SAVE,0); if(t<0) return; cur_slot=t; }
        if(scene==1){ int pd=portal_depth; portal_depth=depth_no; do_save(); portal_depth=pd; }      // in a dungeon: the save wakes you on the surface with the door open to this floor
        else do_save();
        scpy(b,"SAVED TO SLOT "); ncat(b,cur_slot+1); if(scene==1){ scat(b," AT FLOOR "); ncat(b,depth_no); }
        sys_say(b,C_GREEN); sfx(6);
        if(scene==1 && endless && loot>0) sys_say("SAVED. LOOT IS NOT KEPT",C_GOLD);
    }
    else if(cur==2){ int t=slot_picker(pg,SP_LOAD,0); if(t>=0){ cur_slot=t; sys_req=1; } }
    else if(cur==3) options_screen(pg);
    else if(confirm(pg,"QUIT TO THE TITLE?","UNSAVED PROGRESS IS LOST")) sys_req=2;
}

static void map_page(volatile u16 *pg); static void size_screen(volatile u16 *pg, int from_title); static int big_unlocked(void);
#define EX_N 12                                                  // lines on the EXTRAS page (extras.inc)
static void extras_page(volatile u16 *pg, int cur, int *top, const char **foot); static int extras_press(volatile u16 *pg, int cur);
// ---------------------------------------------------------------- the pause screen: nine pages with picture tabs, your bug in a frame, where you are, what you have done
static void world_page(volatile u16 *pg, int y); static void relic_shelf(volatile u16 *pg, int y); static int fav_add_here(void); static int me_oam(int slot, int cx, int feet, int frame);
static const u8 tab_icon[9][8]={
    {0x24,0x18,0x3C,0xBD,0x7E,0xBD,0x3C,0x42},   // BUG: a little beetle
    {0x7E,0xFF,0xFF,0xFF,0x7E,0x7E,0x3C,0x18},   // GEAR: a shield
    {0x18,0x18,0xFF,0x7E,0x3C,0x7E,0x66,0x42},   // MAGIC: a star
    {0xFE,0x82,0xBA,0x82,0xBA,0x82,0xFE,0x7C},   // QUEST: a note
    {0x3C,0x5A,0x99,0xFF,0x99,0x5A,0x3C,0x00},   // WORLD: a globe
    {0xFF,0xFF,0x7E,0x3C,0x18,0x18,0x3C,0x7E},   // FEATS: a cup
    {0xFE,0x92,0x92,0xFE,0x92,0x92,0xFE,0x00},   // MAP: a grid
    {0xFC,0x8A,0x8A,0xFE,0x82,0xBA,0xBA,0xFE},   // SYSTEM: a save disk
    {0x3C,0x42,0x42,0xFF,0xFF,0xE7,0xE7,0xFF}};  // EXTRAS: a padlock
static void icon8(volatile u16 *pg, int x, int y, const u8 *ic, int col){ for(int r=0;r<8;r++) for(int c=0;c<8;c++) if(ic[r]&(0x80>>c)) pix(pg,x+c,y+r,col); }
#define HI 255                                                 // the selection bar's colour: it gently pulses while the pause screen is open
static int pause_last[2]={0,6};                                // the page you were on last time, on the surface and down in a dungeon
static void char_screen(volatile u16 *pg){
    static const u8 ord[9]={0,1,2,3,7,4,8,6,5};                // page numbers in tab order (7 = WORLD, 8 = EXTRAS; pause_last keeps page numbers, so older orders still work)
    static const char *const tabn[9]={"BUG","GEAR","MAGIC","QUEST","WORLD","FEATS","EXTRAS","MAP","SYSTEM"};
    static const u8 tabc[9]={C_GREEN,C_CYAN,C_UNIQ,C_GOLD,C_CYAN,C_GOLD,C_UNIQ,C_GREEN,C_WHITE};
    int ti=0; { int want=pause_last[scene==1]; for(int i=0;i<9;i++) if(ord[i]==want) ti=i; }
    int page=ord[ti], cur=0, dirty=1, sub=0, tick=0, extop=0; sys_msg[0]=0; in_menu++; menu_msg[0]=0;
    static const char *const stn[4]={"STRENGTH","DEXTERITY","VITALITY","MAGIC"};
    static const char *const std[4]={"BITE DAMAGE","CRITS, FASTER BITES","HEALTH, DEFENCE","MANA, SPELL POWER"};
    bug_build(); form_palette();
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64];
            // ---- the backdrop and the tab bar: a picture and a name for every page; the open tab is a raised card
            ui_bg(pg,18,149); rect(pg,0,18,240,17,U_BG0); rect(pg,0,35,240,1,U_RAISE);
            for(int t=0;t<9;t++){ int x=t*240/9, on=t==ti;                  // nine tabs, 26 or 27 dots apart
                if(on){ rect(pg,x,19,26,17,U_PANEL); rect(pg,x,20,1,16,U_RAISE); rect(pg,x+25,20,1,16,U_EDGE); rect(pg,x+1,19,24,2,tabc[t]); rect(pg,x+1,21,24,1,U_SHINE); }
                icon8(pg,x+9,22,tab_icon[t], on?tabc[t]:U_GLOW);
                ctext(pg,x+13,30,tabn[t], on?C_GOLD:U_GLOW); }
            const char *foot=""; int y=38;
            if(page==0){
                // ---- BUG: you, your bars, your stats, your friends
                ui_panel(pg,4,38,122,48);
                stext(pg,10,42,bug_name,C_GOLD); { char lv[12]="LV "; ncat(lv,level); int w=textlen(lv)*4+6; rect(pg,120-w,41,w,8,U_RAISE); rect(pg,120-w,41,w,1,U_SHINE); text(pg,123-w,43,lv,C_WHITE,1); }
                vbar(pg,30,51,90,hearts,maxh,C_RED,"HP");
                vbar(pg,30,59,90,mp,maxmp,C_CYAN,"MP");
                vbar(pg,30,67,90,food,100,food>25?C_GREEN:C_RED,"FOOD");
                vbar(pg,30,75,90,xp,xp_next(),C_GOLD,"XP");
                // the portrait: a sunken window with a floor and a shadow (your sprite stands in it every frame, below)
                ui_panel(pg,128,38,108,48); ui_well(pg,132,42,100,40);
                for(int yy=66;yy<81;yy++) rect(pg,133,yy,98,1, yy<70?U_INK:yy<74?U_BG0:U_BG1);
                for(int k=0;k<5;k++){ int w=34-k*6; rect(pg,182-w/2,79+(k>>1),w,1,U_EDGE); }
                { const char *fn = form==FORM_CREATURE ? body_name[form_body] : form_name[form]; text(pg,136,45,fn,U_GLOW,1); }
                // stats
                ui_panel(pg,4,88,122,40); ui_head(pg,9,91,112,"STATS",C_GREEN);
                s64 *sv[4]={&st_str,&st_dex,&st_vit,&st_mag};
                for(int i=0;i<4;i++){ int yy=99+i*7; if(cur==i) ui_sel(pg,7,yy-1,116,7,HI);
                    text(pg,12,yy,stn[i],cur==i?C_WHITE:U_SOFT,1); b[0]=0; ncat(b,*sv[i]); text(pg,58,yy,b,C_WHITE,1);
                    int pc = i==0?C_RED:i==1?C_GOLD:i==2?C_GREEN:C_CYAN; for(int k=0;k<10;k++){ int on=k<*sv[i]; rect(pg,74+k*4,yy,3,5, on?pc:U_TROUGH); if(on) rect(pg,74+k*4,yy,3,1,ui_glint(pc)); } }
                // power
                ui_panel(pg,128,88,108,40); ui_head(pg,133,91,98,"POWER",C_RED);
                { int yy=99;
                  text(pg,134,yy,"DMG",U_SOFT,1); b[0]=0; ncat(b,d_dmg+d_fire+d_venom); text(pg,150,yy,b,C_WHITE,1); text(pg,184,yy,"DEF",U_SOFT,1); b[0]=0; ncat(b,d_def); text(pg,200,yy,b,C_WHITE,1); yy+=7;
                  text(pg,134,yy,"CRIT",U_SOFT,1); b[0]=0; ncat(b,d_crit); scat(b,"%"); text(pg,154,yy,b,C_WHITE,1); text(pg,184,yy,"GOLD",U_SOFT,1); b[0]=0; ncat(b,gold_find); scat(b,"%"); text(pg,204,yy,b,C_WHITE,1); yy+=7;
                  if(d_spd){ scpy(b,"SPD "); scat(b,d_spd>0?"+":"-"); ncat(b,d_spd>0?d_spd:-d_spd); scat(b,"%"); text(pg,134,yy,b,d_spd>0?C_GREEN:C_RED,1); }
                  if(d_haste){ scpy(b,"BITES +"); ncat(b,d_haste); text(pg,184,yy,b,C_GREEN,1); } yy+=7;
                  scpy(b,fame_rank()); scat(b," * FAME "); ncat(b,fame); b[25]=0; text(pg,134,yy,b,C_GOLD,1); }
                // Buddy and your friends
                ui_panel(pg,4,130,232,18);
                scpy(b,"BUDDY LV"); ncat(b,pet_lv+1); scat(b," "); scat(b,pet_form_name[pet_form]); text(pg,10,133,b,C_CYAN,1);
                if(pet_away_t) rtext(pg,232,133,"AWAY SELLING",U_SOFT); else if(pet_down_t) rtext(pg,232,133,"RESTING",U_SOFT); else { int pm=pet_maxhp*(pet_form==2?2:1); ui_bar(pg,150,133,82,5,pet_hp,pm,C_GREEN); }
                { int any=0, x=10; for(int t=0;t<6;t++) if(have_bug[t]){ any=1; scpy(b,friend_name[t]); scat(b," X"); ncat(b,have_bug[t]); if(x+textlen(b)*4>232) break; text(pg,x,141,b,C_WHITE,1); x+=textlen(b)*4+8; }
                  if(!any) text(pg,10,141,"NO BUG FRIENDS YET. OPEN CAGES TO FREE THEM",C_DGREY,1); }
                if(stat_pts){ static char ft[32]; scpy(ft,"A: ADD A POINT ("); ncat(ft,stat_pts); scat(ft," LEFT)"); foot=ft; }
            } else if(page==1){
                const Gear *eq[3]={&eqw,&eqa,&eqc}; static const char *const lab[3]={"BITE","WEAR","CHARM"}; static const u8 lc[3]={C_RED,C_CYAN,C_UNIQ};
                for(int i=0;i<3;i++){ int x=4+i*78; ui_panel(pg,x,38,76,22); rect(pg,x+5,42,2,5,lc[i]); text(pg,x+10,42,lab[i],U_SOFT,1);
                    if(eq[i]->it){ gear_name(eq[i],b); b[17]=0; text(pg,x+5,51,b,rar_color(eq[i]),1); } else text(pg,x+5,51,"NOTHING",C_DGREY,1); }
                ui_panel(pg,4,62,232,58);
                scpy(b,"YOUR BAG"); ui_head(pg,9,65,222,b,C_CYAN); scpy(b,""); ncat(b,bag_n); scat(b,"/"); ncat(b,BAGMAX); if(bag_n>12){ scat(b,"  PAGE "); ncat(b,cur/12+1); scat(b,"/"); ncat(b,(bag_n+11)/12); }
                { int w=textlen(b)*4+6; rect(pg,231-w,64,w,8,U_PANEL); text(pg,234-w,65,b,U_SOFT,1); }
                int y0=74;
                for(int i=(cur/12)*12;i<bag_n && i<(cur/12)*12+12;i++){ int k12=i%12; int col=k12/6, yy=y0+(k12%6)*7, x=10+col*114;
                    if(cur==i) ui_sel(pg,x-3,yy-1,112,7,HI);
                    gear_name(&bag[i],b); b[25]=0; text(pg,x+2,yy,b,rar_color(&bag[i]),1);
                    if(is_gear(&bag[i])){ Delta d=gear_delta(&bag[i]); int v=d.dmg+d.def+d.hp/4; if(v){ int c=v>0?C_GREEN:C_RED; rect(pg,x+101,yy+2,5,1,c); if(v>0) rect(pg,x+103,yy,1,5,c); } } }
                if(!bag_n) text(pg,12,y0+2,"EMPTY. PICK THINGS UP IN DUNGEONS",C_DGREY,1);
                ui_panel(pg,4,122,232,26);
                if(bag_n){ describe(&bag[cur],b); b[56]=0; text(pg,10,125,b,C_WHITE,1);
                    int sc=delta_text(&bag[cur],b); b[40]=0; text(pg,10,133,b, sc>0?C_GREEN:sc<0?C_RED:U_SOFT,1);
                    scpy(b,"WORTH "); ncat(b,item_value(&bag[cur])); scat(b,"G");
                    if(is_gear(&bag[cur])){ scat(b,"  SCRAP "); ncat(b,scrap_of(bag[cur].rar)); }
                    rtext(pg,231,140,b,C_GOLD); }
                foot = bag_n && is_gear(&bag[cur]) ? "A: WEAR  B: SCRAP  SEL: SELL" : bag_n ? "A: USE  B: DROP  SEL: SELL" : "";
            } else if(page==2){
                ui_panel(pg,4,38,232,60); ui_head(pg,9,41,150,"SPELLS",C_UNIQ);
                scpy(b,"MANA "); ncat(b,mp); scat(b,"/"); ncat(b,maxmp); scat(b,"  POWER "); ncat(b,st_mag); rtext(pg,231,41,b,C_CYAN);
                static const char *const sd[NSPELL]={"A BALL OF FIRE FLIES AHEAD","LIGHTNING HITS THE NEAREST FOE","HEALS YOU","HARDER TO HURT FOR A WHILE","PORTAL HOME. THE DOOR RETURNS"};
                y=51; for(int i=0;i<NSPELL;i++){ if(cur==i) ui_sel(pg,7,y-1,226,7,HI);
                    if(i==spell_sel){ rect(pg,11,y+1,3,3,C_GOLD); }
                    text(pg,17,y,spell_name[i], spell_known[i] ? (i==spell_sel?C_GOLD:C_WHITE) : C_DGREY,1);
                    b[0]=0; ncat(b,spell_cost[i]); scat(b," MP"); text(pg,76,y,b,spell_known[i]?C_CYAN:C_DGREY,1);
                    text(pg,108,y, spell_known[i]?sd[i]:"FIND ITS BOOK",spell_known[i]?U_SOFT:C_DGREY,1); y+=9; }
                ui_panel(pg,4,100,232,32); ui_head(pg,9,103,222,"HOW TO CAST",C_CYAN);
                ui_hints(pg,12,113,"SEL: CAST IT, IN A DUNGEON",C_WHITE,1);
                ui_hints(pg,12,123,"SEL+L/R: CHANGE SPELL",C_WHITE,1);
                foot="A: MAKE IT YOUR SPELL";
            } else if(page==3){
                quest_refresh();
                ui_panel(pg,4,38,232,30); ui_head(pg,9,41,140,"QUEST",C_GOLD);
                scpy(b,"FAME "); ncat(b,fame); scat(b,"  "); scat(b,fame_rank()); rtext(pg,231,41,b,C_GOLD);
                if(q_type==Q_NONE) text(pg,12,51,"NONE. VISIT YOUR SHIP'S QUEST BOARD.",C_DGREY,1);
                else { text(pg,12,51,q_name,C_WHITE,1); scpy(b, q_done ? "DONE! GO BACK TO YOUR SHIP" : "IN PROGRESS"); text(pg,12,59,b,q_done?C_GREEN:U_SOFT,1);
                    if(!q_done && q_counted(q_type)){ ui_bar(pg,130,59,70,5,q_have,q_need,C_GOLD); scpy(b,""); ncat(b,q_have); scat(b,"/"); ncat(b,q_need); text(pg,204,59,b,C_WHITE,1); } }
                ui_panel(pg,4,70,232,78); y=73;
                if(endless){ scpy(b,"ENDLESS GALAXY  WORLD "); ncat(b,world_no); scat(b,"  DEEPEST B"); ncat(b,best_depth); text(pg,12,y,b,C_CYAN,1); y+=10; }
                ui_head(pg,9,y,222,"WHAT YOU HAVE LEARNED",C_GREEN); y+=10;
                text(pg,12,y,"BERRIES",U_GLOW,1); y+=8;
                for(int i=0;i<8;i++){ scpy(b,berry_col[i]); scat(b,":"); scat(b, berry_known[i]?berry_eff[berry_map[i]]:"?"); text(pg,12+(i&3)*56,y+(i>>2)*7,b,berry_known[i]?C_WHITE:C_DGREY,1); }
                y+=17; text(pg,12,y,"SEEDS",U_GLOW,1); y+=8;
                for(int i=0;i<6;i++){ scpy(b,seed_look[i]); scat(b,":"); scat(b, seed_known[i]?seed_eff[seed_map[i]]:"?"); text(pg,12+(i%3)*74,y+(i/3)*7,b,seed_known[i]?C_WHITE:C_DGREY,1); }
            } else if(page==4){
                ui_panel(pg,4,37,232,111); y=40;
                if(sub){ relic_shelf(pg,y); foot="A: SHOW YOUR FEATS"; }
                else {
                    ui_head(pg,9,y,150,"FEATS",C_GOLD); { int dn=0; for(int i=0;i<NACH;i++) dn+=(ach_done>>i)&1; scpy(b,""); ncat(b,dn); scat(b,"/"); ncat(b,NACH); scat(b," DONE"); rtext(pg,231,y,b,U_SOFT); } y+=9;
                    for(int i=0;i<NACH;i++){ int done=(ach_done>>i)&1, hv=ach_have(i); if(hv>ach[i].goal) hv=ach[i].goal; int yy=y+i*7;
                        if(done){ rect(pg,9,yy+1,3,3,C_GOLD); } else rect(pg,9,yy+1,3,3,U_TROUGH);
                        text(pg,15,yy,ach[i].name,done?C_GOLD:C_WHITE,1); text(pg,80,yy,ach[i].what,done?U_SOFT:C_DGREY,1);
                        if(done) text(pg,190,yy,"DONE",C_GREEN,1); else { hbar(pg,184,yy+1,24,hv,ach[i].goal,C_CYAN); scpy(b,""); ncat(b,hv); scat(b,"/"); ncat(b,ach[i].goal); text(pg,211,yy,b,C_DGREY,1); } }
                    foot="A: SHOW YOUR RELICS"; }
            } else if(page==6){ if(scene!=1) ui_panel(pg,4,37,232,30); map_page(pg);
            } else if(page==8){ ui_panel(pg,4,37,232,111); extras_page(pg,cur,&extop,&foot);
            } else if(page==7){ ui_panel(pg,4,37,232,111); world_page(pg,41); foot = big_unlocked() ? "A: FAVOURITE  SEL: HOW BIG IS IT?" : "A: SAVE AS A FAVOURITE";
            } else {
                static const char *const sn[5]={"SAVE GAME","SAVE TO ANOTHER SLOT","LOAD GAME","OPTIONS","QUIT TO TITLE"};
                ui_panel(pg,4,38,232,72);
                scpy(b,"SLOT "); ncat(b,cur_slot+1); ui_head(pg,9,41,60,b,C_CYAN);
                fmt_time(b,play_secs()); scat(b," PLAYED"); text(pg,80,41,b,U_SOFT,1);
                text(pg,172,41,opt_autosave?"AUTOSAVE ON":"AUTOSAVE OFF",opt_autosave?C_GREEN:C_DGREY,1); y=51;
                for(int i=0;i<5;i++){
                    int ok = !(i<2 && (scene==2 || (scene==1 && (forge_on || base_mode))));
                    if(cur==i) ui_sel(pg,7,y-2,226,10,HI);
                    text(pg,14,y,sn[i],!ok?C_DGREY: cur==i?C_WHITE:U_SOFT,1);
                    if(i==0){ scpy(b,"SLOT "); ncat(b,cur_slot+1); text(pg,190,y,b,ok?C_CYAN:C_DGREY,1); }
                    y+=11;
                }
                if(scene==2 || (scene==1 && (forge_on || base_mode))) text(pg,12,y-2,"YOU CAN'T SAVE HERE. TRY ON A PLANET",C_DGREY,1);
                // ---- your records
                ui_panel(pg,4,112,232,36); ui_head(pg,9,115,222,"YOUR RECORDS",C_GOLD); y=125;
                { s64 kills=0; for(int i=0;i<11;i++) kills+=st_kills[i];
                  scpy(b,"PLANETS "); ncat(b,st_planets); scat(b,"   FLOORS "); ncat(b,st_floors); scat(b,"   MONSTERS "); ncat(b,kills); text(pg,12,y,b,C_WHITE,1); y+=8;
                  scpy(b,"FISH "); ncat(b,st_fish); scat(b,"   ORE "); ncat(b,st_mined); scat(b,"   DODGES "); ncat(b,st_dodges); scat(b,"   WEIRD "); ncat(b,weird_seen); text(pg,12,y,b,C_WHITE,1); }
                foot="A: CHOOSE";
            }
            ui_foot(pg,foot,"L R: PAGE  START: BACK");
            if(page==5 && sys_msg[0]){ char sm[40]; scpy(sm,sys_msg); sm[34]=0; rect(pg,0,150,150,10,U_BG0); text(pg,6,152,sm,sys_col,1); }
            if(menu_msg[0]){ char mm[40]; scpy(mm,menu_msg); mm[34]=0; rect(pg,0,150,150,10,U_BG0); text(pg,6,152,mm,C_GREEN,1); }
        }
        // ---- every frame: the selection bar breathes, and your bug stands in its frame on the BUG page
        tick++; { int s=sinx((tick*5)&255); int t=((s+16384)*16)>>15; if(t>16) t=16; BG_PAL[HI]=rgb15(24+t*2,34+t*3,64+t*5); }
        oam_hide_all(); if(page==0) me_oam(0,182,76,(tick>>4)&1); oam_flush();
        vsync(); read_keys();
        if(HIT(K_START)) break;
        if(HIT(K_L)){ ti=(ti+8)%9; page=ord[ti]; cur=0; sub=0; dirty=1; sys_msg[0]=0; sfx(7); }
        if(HIT(K_R)){ ti=(ti+1)%9; page=ord[ti]; cur=0; sub=0; dirty=1; sys_msg[0]=0; sfx(7); }
        int nmax = page==0?4 : page==1?bag_n : page==2?NSPELL : page==5?5 : page==8?EX_N : 0;
        if(nmax){ if(HIT(K_UP)){ cur=(cur+nmax-1)%nmax; dirty=1; } if(HIT(K_DOWN)){ cur=(cur+1)%nmax; dirty=1; }
            if(page==1){ if(HIT(K_RIGHT) && cur+6<nmax){ cur+=6; dirty=1; } if(HIT(K_LEFT) && cur>=6){ cur-=6; dirty=1; } } }
        if(HIT(K_A)){
            dirty=1;
            if(page==0 && stat_pts){ s64 *sv[4]={&st_str,&st_dex,&st_vit,&st_mag}; (*sv[cur])=sadd(*sv[cur],1); stat_pts--; s64 oldmax=maxh; recalc(); hearts=sadd(hearts,sadd(maxh,-oldmax)); sfx(2); }
            if(page==1 && bag_n){ use_index(cur); if(cur>=bag_n && cur) cur--; }
            if(page==2 && spell_known[cur]) spell_sel=cur;
            if(page==4){ sub^=1; sfx(2); }
            if(page==7){ int r=fav_add_here(); scpy(menu_msg, r>=0 ? "SAVED TO YOUR FAVOURITES" : r==-2 ? "IT IS ALREADY A FAVOURITE" : r==-3 ? "31 UNIVERSES IN USE" : "FAVOURITES ARE FULL (256)"); if(r>=0) sfx(6); }
            if(page==5){ sys_action(pg,cur); if(sys_req) break; }
            if(page==8){ if(extras_press(pg,cur)) break; }
        }
        if(HIT(K_B) && page==1 && bag_n){
            if(is_gear(&bag[cur])){ int n=scrap_of(bag[cur].rar); scrap+=n; char m[32]; scpy(m,"SCRAPPED: +"); ncat(m,n); scat(m," TECH SCRAP"); drop_at(cur); say(m); sfx(13); }
            else drop_at(cur);
            if(cur>=bag_n && cur) cur--; dirty=1; }
        if(HIT(K_SEL) && page==1){ send_pet_to_sell(); dirty=1; }
        if(HIT(K_SEL) && page==7){ if(big_unlocked()){ oam_hide_all(); oam_flush(); size_screen(pg,0); hud_now(); } else scpy(menu_msg,"LOCKED: BEAT THE ANT KING FIRST"); dirty=1; }
        if(HIT(K_UP)||HIT(K_DOWN)||HIT(K_L)||HIT(K_R)) menu_msg[0]=0;
    }
    pause_last[scene==1]=page; BG_PAL[HI]=bg_pal[HI];
    oam_hide_all(); oam_flush();
    in_menu--; wait_release();
}

// ---------------------------------------------------------------- the ship: shops, quest board
EW static Gear stock[6];
static void restock(void){ int il=dlv()*3+clampi(level); for(int i=0;i<6;i++) stock[i]=roll_gear(i<2?IT_THORN:i<4?IT_SHELL:IT_CHARM, il, 10); }
static void make_quest(void){
    q_type=1+rndi(NQ-1); if(q_type==Q_BOSS && endless) q_type=Q_HUNT; q_have=0; q_done=0; q_zone=zone; q_world=world_no;
    int maxf = endless ? (best_depth>3?best_depth:3) : zone_floors();
    q_floor = 1+rndi(maxf);
    q_gold = 60 + dlv()*40 + q_floor*10; q_fame = 15 + dlv()*6;
    static const char *const cn[6]={"GRIMFANG","MOLDJAW","SKITTERMAW","VILEWING","THE PALE ONE","OLD CRUNCH"};
    char b[40];
    switch(q_type){
        case Q_HUNT: q_need=8+dlv()*2+rndi(6); scpy(b,"SLAY "); ncat(b,q_need); scat(b," MONSTERS"); break;
        case Q_CHAMP: scpy(b,"SLAY "); scat(b,cn[rndi(6)]); scat(b," ON B"); ncat(b,q_floor); q_gold+=60; q_fame+=10; break;
        case Q_FETCH: scpy(b,"FIND THE LOST TREASURE ON B"); ncat(b,q_floor); break;
        case Q_BOSS: q_floor=zone_floors(); q_gold+=150; q_fame+=25; scpy(b,"SLAY THE BOSS ON B"); ncat(b,q_floor); break;        // the zone boss waits on the last floor
        case Q_SHARDS: q_need=2+rndi(2); if(q_need>q_floor) q_need=q_floor; q_gold+=q_need*15; scpy(b,"FIND "); ncat(b,q_need); scat(b," SHARDS B"); ncat(b,q_floor-q_need+1); scat(b,"-"); ncat(b,q_floor); break;
        case Q_BRING: q_floor=rndi(4); q_need=2+rndi(3); q_gold=60+dlv()*40+q_need*20; scpy(b,"BRING "); ncat(b,q_need); scat(b," "); scat(b,bring_nm[q_floor]); break;
        case Q_GOODS: q_floor=rndi(8); q_need=3+rndi(4); q_gold=60+dlv()*40+q_need*25; scpy(b,"BRING "); ncat(b,q_need); scat(b," "); scat(b,goods[q_floor]); break;
        default: q_need=2+rndi(2); scpy(b,"FREE "); ncat(b,q_need); scat(b," CAGED BUGS"); break;
    }
    b[23]=0; scpy(q_name,b);
}
static void (*list_desc)(int,char*,int*)=0;      // an optional second line about the picked row
static int list_pick(volatile u16 *pg, const char *title, int n, void (*row)(int,char*,int*), int *sel, const char *help){
    int dirty=1;
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64]; int c;
            panel(pg,12,20,216,136,title);
            { char g[16]=""; ncat(g,gold); scat(g,"G"); int gx=222-textlen(g)*4; rect(pg,gx-9,26,5,5,C_GOLD); rect(pg,gx-8,26,2,1,U_GLGOLD); stext(pg,gx,25,g,C_GOLD); }   // your gold, with a coin
            for(int i=0;i<n;i++){ c=C_GREY; row(i,b,&c); b[48]=0; if(i==*sel){ ui_sel(pg,16,36+i*8,208,8,U_SEL); if(c==C_GREY) c=C_WHITE; } text(pg,22,37+i*8,b,c,1); }
            rect(pg,14,141,212,1,U_RAISE); ui_hints(pg,20,145,help,U_SOFT,1);
            if(list_desc && n){ int c2=C_GREY; list_desc(*sel,b,&c2); b[52]=0; rect(pg,18,120,204,1,U_RAISE); text(pg,20,124,b,c2,1); }
            if(menu_msg[0]) text(pg,20,132,menu_msg,C_GREEN,1); else if(list_desc && n){ int c2=C_GREY; list_desc(-1-*sel,b,&c2); b[52]=0; text(pg,20,132,b,c2,1); }
        }
        vsync(); read_keys();
        if(keys!=keys_old) dirty=1;
        if(n){ if(HIT(K_UP)){ *sel=(*sel+n-1)%n; menu_msg[0]=0; } if(HIT(K_DOWN)){ *sel=(*sel+1)%n; menu_msg[0]=0; } }
        if(HIT(K_B)) return -1;
        if(HIT(K_A)) return *sel;
    }
}
static const char *const sup_name[6]={"CRUMB","HEALTH POTION","MANA POTION","MYSTERY BERRY","MYSTERY SEED","WARP CRYSTAL"};
static const u8 sup_cost[6]={10,25,25,15,20,120};
static void sup_row(int i, char *b, int *c){ scpy(b,sup_name[i]); while(textlen(b)<16) scat(b," "); ncat(b,sup_cost[i]); scat(b,"G"); }
static void gear_desc(const Gear *g, int which, char *b, int *c){       // which 0: what it does. 1: how it compares
    if(which==0){ if(!g->it){ b[0]=0; return; } describe(g,b); *c=C_WHITE; }
    else { int sc=delta_text(g,b); *c = sc>0?C_GREEN:sc<0?C_RED:C_GREY; if(!is_gear(g)) b[0]=0; }
}
static void stock_desc(int i, char *b, int *c){ if(i<0) gear_desc(&stock[-1-i],1,b,c); else gear_desc(&stock[i],0,b,c); }
static void sell_desc(int i, char *b, int *c){ if(i<0){ b[0]=0; } else gear_desc(&bag[i],0,b,c); }
static void sup_desc(int i, char *b, int *c){ static const char *const d[6]={"FOOD +40","HEALTH BACK","MANA BACK","A RANDOM BERRY. TRY IT TO LEARN IT","A RANDOM SEED. TRY IT TO LEARN IT","A PORTAL HOME FROM ANY DUNGEON FLOOR"}; if(i<0){ b[0]=0; return; } scpy(b,d[i]); *c=C_WHITE; }
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
            for(int i=(sel>10?sel-10:0);i<n && y<136;i++, y+=8){
                if(i<ng){ Gear *g=&bag[gi[i]]; gear_name(g,b); b[30]=0; text(pg,22,y,b,rar_color(g),1);
                    scpy(b,"KEEP +"); ncat(b,keep_points(g->rar)); scat(b,"  SCRAP "); ncat(b,scrap_of(g->rar)); text(pg,236-24-textlen(b)*4,y,b,C_DGREY,1); }
                else { scpy(b,"IN CASE: "); scat(b,uq_name[uq[i-ng]]); text(pg,22,y,b,C_UNIQ,1); text(pg,180,y,"TAKE OUT",C_DGREY,1); }
                if(i==sel) text(pg,16,y,">",C_CYAN,1); }
            text(pg,20,140, sel<ng ? "A: KEEP IT FOREVER  B: SCRAP IT  START: BACK" : "A: TAKE IT OUT TO USE  START: BACK",C_CYAN,1);
            if(menu_msg[0]) text(pg,20,148,menu_msg,C_GREEN,1); else text(pg,20,148,"KEPT THINGS ARE SAFE, EVEN IF YOU FAINT",C_DGREY,1);
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
// ---------------------------------------------------------------- the workbench: upgrade gear with gold and what you mined
static int wb_cost_gold(const Gear *g){ return 25 + g->lvl*12 + (g->rar==R_UNIQUE?200:g->rar==R_RARE?60:g->rar==R_MAGIC?20:0); }
static int wb_good(const Gear *g){ return g->it==IT_THORN ? 4 : g->it==IT_SHELL ? 3 : 2; }     // thorns want crystals, shells amber, charms silk
static void workbench(volatile u16 *pg){
    int sel=0, dirty=1; wait_release();
    while(1){
        Gear *items[3+BAGMAX]; int n=0; items[n++]=&eqw; items[n++]=&eqa; items[n++]=&eqc;
        for(int i=0;i<bag_n;i++) if(is_gear(&bag[i])) items[n++]=&bag[i];
        if(sel>=n) sel=n-1;
        if(dirty){ dirty=0; char b[64];
            box(pg,12,20,216,136); text(pg,20,24,"WORKBENCH",C_GOLD,1);
            scpy(b,"G"); ncat(b,gold); scat(b,"  AMBER "); ncat(b,cargo[3]); scat(b,"  CRYSTALS "); ncat(b,cargo[4]); scat(b,"  SILK "); ncat(b,cargo[2]); rtext(pg,224,24,b,C_CYAN);
            int y=36;
            for(int i=(sel>9?sel-9:0);i<n && y<118;i++,y+=8){ Gear *g=items[i]; if(i==sel) text(pg,16,y,">",C_CYAN,1);
                if(!g->it){ text(pg,22,y, i==0?"BITE: NOTHING":i==1?"WEAR: NOTHING":"CHARM: NOTHING",C_DGREY,1); continue; }
                scpy(b, i<3 ? (i==0?"BITE: ":i==1?"WEAR: ":"CHARM: ") : "BAG: "); char nm[40]; gear_name(g,nm); nm[26]=0; scat(b,nm); scat(b," +"); ncat(b,g->lvl); text(pg,22,y,b,rar_color(g),1); }
            Gear *g=items[sel];
            if(g->it){ int gd=wb_good(g); scpy(b,"UPGRADE: "); ncat(b,wb_cost_gold(g)); scat(b,"G + 2 "); scat(b,goods[gd]); scat(b,"   HAVE "); ncat(b,cargo[gd]);
                int ok = gold>=wb_cost_gold(g) && cargo[gd]>=2 && g->lvl<97; text(pg,20,124,b,ok?C_GREEN:C_RED,1);
                Gear t=*g; t.lvl+=2; Delta d=gear_delta(&t); Gear old=*g; *g=t; recalc(); *g=old; recalc();
                scpy(b,"NEXT: "); if(g->it==IT_THORN){ scat(b,"DAMAGE "); ncat(b,weapon_base(g)); scat(b," -> "); ncat(b,weapon_base(&t)); } else if(g->it==IT_SHELL){ scat(b,"DEFENCE "); ncat(b,armor_base(g)); scat(b," -> "); ncat(b,armor_base(&t)); } else { scat(b,"STRONGER CHARM EFFECTS"); } (void)d;
                text(pg,20,132,b,C_WHITE,1); }
            if(menu_msg[0]) text(pg,20,140,menu_msg,C_GREEN,1);
            ui_lhint(pg,20,148,"A: UPGRADE  B: BACK   MINE GOODS ON PLANETS",C_CYAN); }
        vsync(); read_keys(); if(keys!=keys_old) dirty=1;
        if(HIT(K_UP)){ sel=(sel+n-1)%n; menu_msg[0]=0; } if(HIT(K_DOWN)){ sel=(sel+1)%n; menu_msg[0]=0; }
        if(HIT(K_B)||HIT(K_START)) break;
        if(HIT(K_A)){ Gear *g=items[sel]; if(!g->it){ say("NOTHING THERE TO UPGRADE"); continue; } int gd=wb_good(g), c=wb_cost_gold(g);
            if(g->lvl>=97) say("IT CAN'T GET ANY BETTER"); else if(gold<c) say("NOT ENOUGH GOLD"); else if(cargo[gd]<2){ char m[40]; scpy(m,"YOU NEED 2 "); scat(m,goods[gd]); scat(m,". MINE SOME"); say(m); }
            else { gold-=c; cargo[gd]-=2; g->lvl+=2; recalc(); sfx(6); char m[40]; scpy(m,"UPGRADED TO +"); ncat(m,g->lvl); say(m); hud_now(); } }
    }
    wait_release();
}
static int make_ship(volatile u16 *pg, int first);
static int enter_dungeon_flag; static int forge_menu(volatile u16 *pg);
static int shop_menu(void){   // returns 1 to fly on
    volatile u16 *pg=VISPG;
    oam_hide_all(); oam_flush();
    int sel=0; in_menu++; menu_msg[0]=0;
    while(1){
        const char *main_opt[16]={ "LAUNCH INTO SPACE","SUPPLIES","GEAR SHOP","SELL","QUEST BOARD","HEAL YOU AND BUDDY  25G","IDENTIFY BERRY/SEED 30G","TROPHY CASE: KEEP OR SCRAP","WORKBENCH: UPGRADE GEAR","MIRROR: CHANGE YOUR LOOK","HANGAR: REDESIGN YOUR SHIP","TALK TO BOLT","YOUR SHAPE","DUNGEON FORGE","KART CLUB: RACE AND BUILD TRACKS","LEAVE"};
#define NSHOP 16
        int can_fly = 1, next_ok = !endless && zone_cleared && zone<5 && cluster_no==0; char nxt[40];
        if(next_ok){ scpy(nxt,"AUTOPILOT TO "); scat(nxt,tint_name[zone+1]); main_opt[0]=nxt; }
        int dirty=1; wait_release();
        while(1){
            if(dirty){ dirty=0; panel(pg,12,20,216,136,"YOUR SHIP"); { char g[16]=""; ncat(g,gold); scat(g,"G"); int gx=222-textlen(g)*4; rect(pg,gx-9,26,5,5,C_GOLD); rect(pg,gx-8,26,2,1,U_GLGOLD); stext(pg,gx,25,g,C_GOLD); }
                for(int i=0;i<NSHOP;i++){ int c=(i==0&&!can_fly)?C_DGREY:(i==sel?C_WHITE:(i==0&&next_ok)?C_GREEN:(i==11)?C_CYAN:C_GREY); if(i==sel) ui_sel(pg,16,36+i*6,120,7,U_SEL); text(pg,22,37+i*6,main_opt[i],c,1); if(i==12){ char fb[24]; scpy(fb,"("); scat(fb, form==FORM_CREATURE ? body_name[form_body] : form_name[form]); scat(fb,")"); text(pg,82,37+i*6,fb,C_CYAN,1); } }
                if(next_ok) rtext(pg,224,47,"HOLD A: FLY YOURSELF",C_DGREY);
                { char hb[40]; scpy(hb,ship.name); scat(hb,"  HOLD "); ncat(hb,cargo_n()); scat(hb,"/"); ncat(hb,cargo_cap+ship_hold_bonus()); rtext(pg,224,38,hb,C_CYAN); }
                if(q_type!=Q_NONE && q_done) text(pg,130,70,"! QUEST DONE",C_GREEN,1);
                if(menu_msg[0]) text(pg,20,134,menu_msg,C_GREEN,1);
                rect(pg,14,141,212,1,U_RAISE); ui_hints(pg,20,145,"A: CHOOSE  B: LEAVE",U_SOFT,1); }
            vsync(); read_keys(); if(keys!=keys_old) dirty=1;
            if(HIT(K_UP)){ sel=(sel+NSHOP-1)%NSHOP; menu_msg[0]=0; } if(HIT(K_DOWN)){ sel=(sel+1)%NSHOP; menu_msg[0]=0; }
            if(HIT(K_B)){ in_menu--; wait_release(); return 0; }
            if(HIT(K_A)) break;
        }
        if(sel==15){ in_menu--; wait_release(); return 0; }
        if(sel==14){ kart_req=1; in_menu--; wait_release(); return 0; }
        if(sel==13){ if(!ant_king){ say("BEAT THE ANT KING TO OPEN THE FORGE"); continue; }
            if(forge_menu(pg)){ enter_dungeon_flag=3; in_menu--; wait_release(); return 0; } box(pg,12,20,216,136); continue; }
        if(sel==12){ if(forms_open<=1){ say("A WEIRD WORLD MIGHT RESHAPE YOU ONE DAY"); continue; }      // switch between shapes you have been
            do form=(form+1)%3; while(!((forms_open>>form)&1)); form_t=0; recalc(); form_palette(); char b[40]; scpy(b,"YOU ARE NOW A "); scat(b,form==FORM_CREATURE?body_name[form_body]:form_name[form]); say(b); sfx(6); continue; }
        if(sel==11){ bolt_talk(pg); box(pg,12,20,216,136); continue; }
        if(sel==10){ make_ship(pg,0); box(pg,12,20,216,136); continue; }
        if(sel==9){ if(form==FORM_HUMAN) human_creator(pg,0); else make_bug(pg,0); box(pg,12,20,216,136); continue; }
        if(sel==8){ workbench(pg); continue; }
        if(sel==7){ trophy_case(pg); continue; }
        if(sel==0){ if(can_fly){ in_menu--; int held=0; for(int t=0;t<20 && HELD(K_A);t++){ vsync(); read_keys(); held=t; } wait_release(); return (next_ok && held<19) ? 2 : 1; } continue; }
        if(sel==1){ int s2=0; int r; list_desc=sup_desc; while((r=list_pick(pg,"SUPPLIES",6,sup_row,&s2,"A: BUY  B: BACK"))>=0){
                if(gold<sup_cost[r]){ say("NOT ENOUGH GOLD"); continue; }
                Gear gg={0,0,0,0,0,0};
                if(r==0) gg.it=IT_CRUMB; else if(r<3){ gg.it=IT_POTION; gg.sub=r-1; } else if(r==3){ gg.it=IT_BERRY; gg.sub=rndi(8); } else if(r==4){ gg.it=IT_SEED; gg.sub=rndi(6); } else gg.it=IT_WARP;
                if(add_gear(gg)){ gold-=sup_cost[r]; hud_now(); sfx(7); } } }
        if(sel==2){ int s2=0; int r; list_desc=stock_desc; while((r=list_pick(pg,"GEAR SHOP",6,stock_row,&s2,"A: BUY  B: BACK"))>=0){
                if(!stock[r].it) continue; int cost=item_value(&stock[r])*2; if(gold<cost){ say("NOT ENOUGH GOLD"); continue; }
                if(add_gear(stock[r])){ gold-=cost; stock[r].it=0; hud_now(); sfx(7); } } }
        if(sel==3){ int s2=0; int r; list_desc=sell_desc; while(bag_n && (r=list_pick(pg,"SELL",bag_n,sell_row,&s2,"A: SELL  B: BACK"))>=0){
                gold+=item_value(&bag[r]); drop_at(r); if(s2>=bag_n && s2) s2--; hud_now(); sfx(7); } }
        list_desc=0;
        if(sel==4){
            char b[48]; int dirty2=1; wait_release();
            if(q_type==Q_NONE) make_quest(), q_type=-q_type;     // an offer, not taken yet
            while(1){
                if(dirty2){ dirty2=0; quest_refresh(); box(pg,12,20,216,136); text(pg,20,24,"QUEST BOARD",C_GOLD,1);
                    int t = q_type<0 ? -q_type : q_type;
                    text(pg,20,40,q_name,C_WHITE,1);
                    scpy(b,"REWARD "); ncat(b,q_gold); scat(b,"G, "); ncat(b,q_fame); scat(b," FAME, A PRIZE"); text(pg,20,50,b,C_GOLD,1);
                    if(q_type<0) ui_lhint(pg,20,70,"A: TAKE THIS QUEST   B: NOT NOW",C_CYAN);
                    else if(q_done) text(pg,20,70,"DONE! A: CLAIM YOUR REWARD",C_GREEN,1);
                    else { scpy(b,"IN PROGRESS"); if(q_counted(t)){ scat(b," "); ncat(b,q_have); scat(b,"/"); ncat(b,q_need); } text(pg,20,70,b,C_WHITE,1); ui_lhint(pg,20,80,"A: GIVE UP   B: BACK",C_CYAN); }
                    scpy(b,"YOUR FAME: "); ncat(b,fame); scat(b,"  "); scat(b,fame_rank()); text(pg,20,140,b,C_GOLD,1); }
                vsync(); read_keys(); if(keys!=keys_old) dirty2=1;
                if(HIT(K_B)){ if(q_type<0) q_type=Q_NONE; break; }
                if(HIT(K_A)){
                    if(q_type<0){ q_type=-q_type; say("QUEST TAKEN"); save_game(); break; }
                    if(q_done){ quest_take(); gold=sadd(gold,q_gold); fame+=q_fame; Gear pz=roll_gear(rndi(3)==0?IT_CHARM:rndi(2)?IT_THORN:IT_SHELL, dlv()*3+clampi(level)+2, 30); add_gear(pz); q_type=Q_NONE; say("REWARD CLAIMED!"); sfx(6); save_game(); hud_now(); break; }
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
