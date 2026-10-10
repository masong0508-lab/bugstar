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
// what would change if you wore this: damage, defence, health, mana, crit, speed, gold
typedef struct { int dmg, def, hp, mp, crit, spd, gold; } Delta;
static Delta gear_delta(const Gear *g){
    Delta d={0,0,0,0,0,0,0}; if(!is_gear(g)) return d;
    Gear *slot = g->it==IT_THORN ? &eqw : g->it==IT_SHELL ? &eqa : &eqc; Gear old=*slot; int oh=hearts, om=mp;
    int b_dmg=d_dmg+d_fire+d_venom, b_def=d_def, b_hp=maxh, b_mp=maxmp, b_crit=d_crit, b_spd=d_spd, b_gold=gold_find;
    *slot=*g; recalc();
    d.dmg=d_dmg+d_fire+d_venom-b_dmg; d.def=d_def-b_def; d.hp=maxh-b_hp; d.mp=maxmp-b_mp; d.crit=d_crit-b_crit; d.spd=d_spd-b_spd; d.gold=gold_find-b_gold;
    *slot=old; recalc(); hearts=oh; mp=om; return d;
}
static int delta_text(const Gear *g, char *b){     // "DMG +2  DEF -1" in b; returns 1 better, -1 worse, 0 same/none
    b[0]=0; if(!is_gear(g)) return 0; Delta d=gear_delta(g); int sc=0;
    struct { const char *n; int v; } f[7]={{"DMG",d.dmg},{"DEF",d.def},{"HP",d.hp},{"MP",d.mp},{"CRIT",d.crit},{"SPD",d.spd},{"GOLD",d.gold}};
    for(int i=0;i<7;i++) if(f[i].v){ if(b[0]) scat(b,"  "); scat(b,f[i].n); scat(b,f[i].v>0?" +":" "); ncat(b,f[i].v); sc+=f[i].v>0?1:-1; }
    if(!b[0]) scpy(b,"SAME AS NOW");
    return sc>0?1:sc<0?-1:0;
}
static void vbar(volatile u16 *pg, int x, int y, int w, int val, int max, int col, const char *lab){     // a labelled bar with the numbers on it
    rect(pg,x,y,w,7,C_DGREY); int f = max>0 ? val*(w-2)/max : 0; if(f>w-2) f=w-2; if(f<0) f=0; if(f) rect(pg,x+1,y+1,f,5,col);
    frame1(pg,x,y,w,7,6); char b[16]; b[0]=0; ncat(b,val); scat(b,"/"); ncat(b,max);
    text(pg,x-textlen(lab)*4-3,y+1,lab,C_GREY,1); text(pg,x+w-textlen(b)*4-2,y+1,b,C_WHITE,1);
}
static void box(volatile u16 *pg, int x, int y, int w, int h){ rect(pg,x,y,w,h,0); rect(pg,x,y,w,1,6); rect(pg,x,y+h-1,w,1,6); rect(pg,x,y,1,h,6); rect(pg,x+w-1,y,1,h,6); }

static void card(volatile u16 *pg, const char *title, const char *const *ln, int n);
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
    box(pg,10,y0,220,h); rect(pg,11,y0+1,218,12,6);
    text(pg,120-textlen(title)*2,y0+4,title,C_GOLD,1);
    for(int i=0;i<n;i++) text(pg,120-textlen(ln[i])*2,y0+20+i*11,ln[i],C_WHITE,1);
    wait_release();
    for(int t=0;;t++){ vsync(); read_keys(); if(HIT(K_A)||HIT(K_START)) break;
        text(pg,104,y0+h-11,"A: OK",(t&32)?C_CYAN:C_DGREY,1); }
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
            ctext(pg,120,138,"START: SWITCH PRESETS / MIX    B: KEEP THE OLD ONE",C_DGREY);
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
static int make_bug(volatile u16 *pg, int first){
    enum { R_NAME, R_SHELL, R_PAT, R_MARK, R_HEAD, R_EYES, R_ANT, R_LEGS, R_HORN, NROWS };
    static const char *const rows[NROWS]={"NAME","SHELL","PATTERN","MARKINGS","HEAD","EYES","ANTENNAE","LEGS","HORN"};
    static const char *const shell_n[3]={"SHORT","NORMAL","LONG"}, *const pat_n[5]={"PLAIN","STRIPED","SPOTTED","BANDED","CHECKED"}, *const eye_n[2]={"SMALL","BIG"}, *const ant_n[3]={"CURLED","STRAIGHT","DROOPY"}, *const legs_n[3]={"FOUR","SIX","EIGHT"}, *const horn_n[2]={"NONE","HORN"};
    static const s8 col_of[NROWS]={-1,LC_SHELL,-1,LC_MARK,LC_HEAD,LC_EYE,LC_ANT,LC_LEGS,LC_HORN};
    int sel=0, dirty=1, editing=0, cur=0, t=0; char nm[9]; scpy(nm,look.name); int n=textlen(nm); if(n<1){ scpy(nm,"PIP"); n=3; }
    BugLook before=look;
    wait_release(); bug_build();
    while(1){
        if(dirty){ dirty=0; char b[40];
            panel(pg,12,20,216,136, first ? "MAKE YOUR BUG" : "THE MIRROR");
            for(int i=0;i<NROWS;i++){ int y=37+i*10; if(i==sel) rect(pg,16,y-2,124,10,6);
                text(pg,20,y,rows[i], i==sel?C_WHITE:C_GREY,1);
                const char *opt=0;
                switch(i){ case R_NAME: break; case R_SHELL: opt=shell_n[look.shell]; break; case R_PAT: opt=pat_n[look.pat]; break; case R_EYES: opt=eye_n[look.eye]; break;
                           case R_ANT: opt=ant_n[look.ant]; break; case R_LEGS: opt=legs_n[look.legs]; break; case R_HORN: opt=horn_n[look.horn]; break; default: break; }
                if(i==R_NAME){ for(int k=0;k<8;k++){ char ch[2]={ k<n?nm[k]:'_',0 }; int c = (editing && k==cur) ? C_GOLD : k<n ? C_WHITE : C_DGREY; text(pg,64+k*6,y,ch,c,1); if(editing && k==cur && (t&16)) rect(pg,64+k*6,y+6,4,1,C_GOLD); } }
                else if(opt){ scpy(b,"<"); scat(b,opt); scat(b,">"); text(pg,64,y,b,i==sel?C_GOLD:C_GREY,1); }
                if(col_of[i]>=0 && !(i==R_HORN && !look.horn) && !(i==R_MARK && !look.pat)){ BG_PAL[224+i]=look.col[col_of[i]]; rect(pg,122,y-1,12,8,224+i); frame1(pg,122,y-1,12,8,i==sel?C_WHITE:C_DGREY); } }
            rect(pg,146,38,74,62,1); frame1(pg,146,38,74,62,C_DGREY);
            { char nb[12]; scpy(nb,nm); ctext(pg,183,92,nb,C_GOLD); }
            const char *tip = editing ? "UP/DOWN: LETTER  LEFT/RIGHT: MOVE  A: DONE" : sel==R_NAME ? "A: TYPE A NAME" : col_of[sel]>=0 ? "LEFT/RIGHT: SHAPE   A: ITS COLOUR" : "LEFT/RIGHT: CHANGE IT";
            ctext(pg,120,128,tip,C_GREY);
            ctext(pg,120,138, first ? "SELECT: SURPRISE ME        START: ALL DONE" : "SELECT: SURPRISE ME   START: DONE   B: UNDO ALL", C_CYAN);
            if(first) ctext(pg,120,147,"YOU CAN CHANGE THIS LATER AT YOUR SHIP'S MIRROR",C_DGREY);
        }
        t++; int wf=(t>>3)&1;
        set_affine(31, -128, 0, 0, 128);
        shadow_oam[0].a0=((70-32-wf)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[0].a1=((183-32)&511)|(31<<9)|(2<<14); shadow_oam[0].a2=bug_tile(wf)|BUG_BANK;
        shadow_oam[1].a0=((70-2)&255)|(1<<14); shadow_oam[1].a1=((183-8)&511); shadow_oam[1].a2=T_SHADOW;
        vsync(); oam_flush(); read_keys(); if(editing && (t&15)==0) dirty=1;
        if(editing){
            if(HIT(K_UP)||HIT(K_DOWN)){ if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } else { char c=nm[cur]; c = HIT(K_UP) ? (c=='Z'?' ':c==' '?'A':c+1) : (c=='A'?' ':c==' '?'Z':c-1); nm[cur]=c; } sfx(7); dirty=1; }
            if(HIT(K_RIGHT) && cur<7){ cur++; if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } dirty=1; }
            if(HIT(K_LEFT) && cur>0){ cur--; dirty=1; }
            if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ while(n>0 && nm[n-1]==' ') n--; nm[n]=0; if(n==0){ scpy(nm,"PIP"); n=3; } editing=0; dirty=1; sfx(2); }
            continue; }
        if(HIT(K_UP)){ sel=(sel+NROWS-1)%NROWS; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%NROWS; dirty=1; sfx(7); }
        int d = HIT(K_RIGHT)?1 : HIT(K_LEFT)?-1 : 0;
        if(d){ int ch=1; switch(sel){ case R_SHELL: look.shell=(look.shell+d+3)%3; break; case R_PAT: look.pat=(look.pat+d+5)%5; break; case R_EYES: look.eye^=1; break;
                 case R_ANT: look.ant=(look.ant+d+3)%3; break; case R_LEGS: look.legs=(look.legs+d+3)%3; break; case R_HORN: look.horn^=1; break; default: ch=0; }
               if(ch){ bug_build(); dirty=1; sfx(2); } }
        if(HIT(K_A)){ if(sel==R_NAME){ editing=1; cur=0; dirty=1; }
            else if(col_of[sel]>=0){ oam_hide_all(); oam_flush(); look.col[col_of[sel]]=pick_colour(pg,rows[sel],look.col[col_of[sel]]); bug_build(); dirty=1; }
            else { d=1; if(sel==R_PAT) look.pat=(look.pat+1)%5; bug_build(); dirty=1; sfx(2); } }
        if(HIT(K_SEL)){ look.shell=rndi(3); look.pat=rndi(5); look.eye=rndi(2); look.ant=rndi(3); look.legs=rndi(3); look.horn=rndi(3)==0;
            look.col[LC_SHELL]=preset_col[rndi(NPRESET)]; look.col[LC_HEAD]=preset_col[rndi(NPRESET)]; look.col[LC_MARK]=preset_col[rndi(NPRESET)]; look.col[LC_EYE]=preset_col[rndi(NPRESET)]; look.col[LC_HORN]=preset_col[rndi(NPRESET)];
            bug_build(); dirty=1; sfx(6); }
        if(HIT(K_START)) break;
        if(HIT(K_B)){ if(first){ look=before; bug_build(); wait_release(); oam_hide_all(); oam_flush(); return 0; } look=before; scpy(nm,before.name); n=textlen(nm); bug_build(); dirty=1; sfx(4); }
    }
    scpy(look.name,nm); sfx(6); wait_release(); oam_hide_all(); oam_flush(); return 1;
}
// how to play: lively pages with button badges, over the title planet, with your bug walking along
typedef struct { const char *btn; const char *txt; } HelpLine;
static const HelpLine help_pg[5][7]={
  {{"D-PAD","WALK OVER THE HILLS"},{"SEL","SWITCH 3RD / 1ST PERSON VIEW"},{"A","DOOR, SHIP, CAGE, SLUG, PLANT, ORE"},{"B","USE THE PICKED ITEM"},{"L R","PICK AN ITEM (1ST PERSON: SIDESTEP)"},{"SEL+LR","ARROW: DOOR, SHIP OR NEAREST ORE"},{"START","YOUR BUG, BAG, MAP AND SAVING"}},
  {{"UP DN","WALK FORWARD AND BACK"},{"LT RT","TURN (TAP A LITTLE, HOLD FOR MORE)"},{"L R","STEP SIDEWAYS. TAP TWICE TO DODGE"},{"A","BITE. HOLD IT. THREE IN A ROW = CRUNCH"},{"A","OPEN CAGES, TAKE STAIRS, FISH IN POOLS"},{"SEL","CAST A SPELL. HOLD + L/R PICKS THE SPELL"},{"START","THE MAP OF THIS FLOOR"}},
  {{"D-PAD","STEER THE SHIP"},{"B","THRUST.  R: BRAKE"},{"A","SHOOT PIRATES AND ROCKS"},{"A","NEAR THE PLANET: LAND.  AT THE BOX: DOCK"},{"SEL","THE GALAXY MAP. PICK A STAR, A: JUMP"},{"","JUMPS BURN FUEL. BUY FUEL AT STATIONS"},{"","STATIONS BUY THE GOODS YOU MINE"}},
  {{"!","A RED ! MEANS A MONSTER IS ABOUT TO BITE"},{"","STEP BACK OR DODGE, THEN BITE IT"},{"","BEETLES CHARGE. SIDESTEP: THEY HIT THE WALL"},{"","BUDDY FIGHTS BESIDE YOU. FEED HIM FISH"},{"","EAT CRUMBS WHEN THE GREEN BAR IS LOW"},{"","FAINTING LOSES YOUR BAG (NOT ON EASY)"},{"","SAVE ON A PLANET: START > SYSTEM"}},
  {{"1","FOLLOW THE ARROW TO THE DOOR. GO DOWN"},{"2","BEAT THE BOSS AT THE BOTTOM"},{"3","AT YOUR SHIP: AUTOPILOT TO THE NEXT STAR"},{"","MINE ORE. SELL IT, OR UPGRADE GEAR"},{"","DRONE OUTPOSTS: FIND THE KEYCARD, SMASH THE CORE"},{"","EACH CORE GIVES BOLT A MEMORY BACK"},{"","TALK TO BOLT AT YOUR SHIP IF YOU ARE LOST"}}};
static const char *const help_ttl[5]={"ON A PLANET","IN A DUNGEON","IN SPACE","STAYING ALIVE","YOUR QUEST"};
static int badge_col(const char *b){ if(!b[0]) return -1; if(b[0]=='A'&&!b[1]) return C_RED; if(b[0]=='B'&&!b[1]) return C_CYAN; if(b[0]=='!') return C_RED; if(b[0]>='1'&&b[0]<='3') return C_GOLD; if(b[0]=='S') return C_GOLD; if(b[0]=='D') return C_GREEN; return C_GREY; }
static void help_screen(volatile u16 *pg){
    int p=0, dirty=1, t=0; wait_release(); bug_build();
    while(1){
        if(dirty){ dirty=0;
            title_restore(pg,18,160);
            rect(pg,8,22,224,126,0); frame1(pg,8,22,224,126,6); rect(pg,9,23,222,12,C_DGREY); rect(pg,9,35,222,1,6);
            char b[24]; scpy(b,"HOW TO PLAY  "); ncat(b,p+1); scat(b,"/5"); text(pg,14,26,b,C_GREY,1);
            rtext(pg,226,26,help_ttl[p],C_GOLD);
            for(int i=0;i<7;i++){ const HelpLine *h=&help_pg[p][i]; int y=41+i*13; int c=badge_col(h->btn);
                if(c>=0){ int w=textlen(h->btn)*4+6; rect(pg,14,y-2,w,9,c); text(pg,17,y,h->btn,0,1); }
                else { rect(pg,16,y+2,3,2,C_DGREY); }
                text(pg,50,y,h->txt,C_WHITE,1); }
            for(int k=0;k<5;k++) rect(pg,106+k*7,140,4,3, k==p?C_GOLD:C_DGREY);
            text(pg,14,140,"< >: PAGE",C_CYAN,1); rtext(pg,226,140,"B: BACK",C_CYAN); }
        t++; int wf=(t>>3)&1; int bx=((t>>1)%300)-40;                    // the bug trots along the bottom
        set_affine(31, -128, 0, 0, 128);
        shadow_oam[0].a0=((150-32-wf)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[0].a1=((bx-32)&511)|(31<<9)|(2<<14); shadow_oam[0].a2=bug_tile(wf)|BUG_BANK;
        vsync(); oam_flush(); read_keys(); twinkle(pg,t);
        if(HIT(K_RIGHT)||HIT(K_R)||HIT(K_A)){ if(p<4){ p++; dirty=1; sfx(7); } else if(HIT(K_A)) break; }
        if(HIT(K_LEFT)||HIT(K_L)){ if(p>0){ p--; dirty=1; sfx(7); } }
        if(HIT(K_B)||HIT(K_START)) break;
    }
    oam_hide_all(); oam_flush(); wait_release();
}
// the end of the story: the Ant King is beaten
static void credits(void){
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
        if(scene!=0){ sys_say("YOU CAN ONLY SAVE ON THE SURFACE",C_RED); sfx(0); return; }
        if(cur==1){ int t=slot_picker(pg,SP_SAVE,0); if(t<0) return; cur_slot=t; }
        do_save(); scpy(b,"SAVED TO SLOT "); ncat(b,cur_slot+1); sys_say(b,C_GREEN); sfx(6);
    }
    else if(cur==2){ int t=slot_picker(pg,SP_LOAD,0); if(t>=0){ cur_slot=t; sys_req=1; } }
    else if(cur==3) options_screen(pg);
    else if(confirm(pg,"QUIT TO THE TITLE?","UNSAVED PROGRESS IS LOST")) sys_req=2;
}

static void map_page(volatile u16 *pg);
static void char_screen(volatile u16 *pg){
    int page=scene==1?6:0, cur=0, dirty=1; sys_msg[0]=0; in_menu++; menu_msg[0]=0;
    static const char *const tabs[7]={"BUG","GEAR","MAGIC","QUEST","FEATS","SYSTEM","MAP"};
    static const char *const stn[4]={"STRENGTH","DEXTERITY","VITALITY","MAGIC"};
    static const char *const std[4]={"BITE DAMAGE","CRITS, FASTER BITES","HEALTH, DEFENCE","MANA, SPELL POWER"};
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64];
            rect(pg,0,18,240,142,0); rect(pg,0,18,240,12,C_DGREY); rect(pg,0,30,240,1,6);
            for(int t=0;t<7;t++){ int x=6+t*34; if(t==page) rect(pg,x-3,18,textlen(tabs[t])*4+5,12,0); text(pg,x,21,tabs[t], t==page?C_GOLD:C_GREY, 1); }
            int y=35;
            if(page==0){
                scpy(b,bug_name); scat(b,"  LEVEL "); ncat(b,level); text(pg,8,y,b,C_GOLD,1);
                scpy(b,fame_rank()); scat(b,"  FAME "); ncat(b,fame); rtext(pg,232,y,b,C_GOLD); y+=9;
                vbar(pg,34,y,86,hearts,maxh,C_RED,"HP"); vbar(pg,152,y,80,mp,maxmp,C_CYAN,"MP"); y+=9;
                vbar(pg,34,y,86,food,100,food>25?C_GREEN:C_RED,"FOOD"); vbar(pg,152,y,80,xp,xp_next(),C_GOLD,"XP"); y+=12;
                int *sv[4]={&st_str,&st_dex,&st_vit,&st_mag};
                rect(pg,4,y-2,118,36,1);
                for(int i=0;i<4;i++){ if(cur==i) rect(pg,4,y-1,118,8,6); if(cur==i) text(pg,6,y,">",C_CYAN,1); text(pg,12,y,stn[i],cur==i?C_WHITE:C_GREY,1); b[0]=0; ncat(b,*sv[i]); text(pg,58,y,b,C_WHITE,1);
                    for(int k=0;k<10;k++) rect(pg,72+k*4,y+1,3,4, k<*sv[i]? (i==0?C_RED:i==1?C_GOLD:i==2?C_GREEN:C_CYAN) : C_DGREY); y+=8; }
                y+=2; if(stat_pts){ scpy(b,"A: ADD A POINT ("); ncat(b,stat_pts); scat(b," LEFT)"); text(pg,8,y,b,C_GREEN,1); } else text(pg,8,y,"LEVEL UP TO GET MORE POINTS",C_DGREY,1);
                int yy=y-38; text(pg,130,yy,std[cur],C_GREY,1); yy+=10;
                scpy(b,"DAMAGE  "); ncat(b,d_dmg+d_fire+d_venom); text(pg,130,yy,b,C_WHITE,1); scpy(b,"DEFENCE "); ncat(b,d_def); text(pg,186,yy,b,C_WHITE,1); yy+=8;
                scpy(b,"CRIT "); ncat(b,d_crit); scat(b,"%"); text(pg,130,yy,b,C_WHITE,1); scpy(b,"GOLD "); ncat(b,gold_find); scat(b,"%"); text(pg,186,yy,b,C_WHITE,1); yy+=8;
                if(d_spd){ scpy(b,"SPEED "); ncat(b,d_spd>0?d_spd:-d_spd); scat(b,"%"); text(pg,130,yy,b,d_spd>0?C_GREEN:C_RED,1); }
                if(d_haste){ scpy(b,"BITES +"); ncat(b,d_haste); text(pg,186,yy,b,C_GREEN,1); } yy+=8;
                y+=12; rect(pg,4,y-2,232,1,6); y+=3;
                scpy(b,"BUDDY LV"); ncat(b,pet_lv+1); scat(b,": "); scat(b,pet_form_name[pet_form]); text(pg,8,y,b,C_CYAN,1);
                if(pet_away_t) rtext(pg,232,y,"AWAY SELLING",C_GREY); else if(pet_down_t) rtext(pg,232,y,"RESTING",C_GREY); else { int pm=pet_maxhp*(pet_form==2?2:1); hbar(pg,152,y+2,80,pet_hp,pm,C_GREEN); } y+=10;
                text(pg,8,y,"BUG FRIENDS",C_GOLD,1); y+=8; int any=0, x=8;
                for(int t=0;t<6;t++) if(have_bug[t]){ any=1; scpy(b,friend_name[t]); scat(b," X"); ncat(b,have_bug[t]); text(pg,x,y,b,C_WHITE,1); x+=76; if(x>200){ x=8; y+=7; } }
                if(!any) text(pg,8,y,"NONE YET. OPEN CAGES TO FREE BUGS",C_DGREY,1);
            } else if(page==1){
                const Gear *eq[3]={&eqw,&eqa,&eqc}; static const char *const lab[3]={"BITE","WEAR","CHARM"};
                for(int i=0;i<3;i++){ int x=8+i*78; rect(pg,x-2,y-2,74,18,1); frame1(pg,x-2,y-2,74,18,C_DGREY); text(pg,x,y,lab[i],C_GOLD,1);
                    if(eq[i]->it){ gear_name(eq[i],b); b[17]=0; text(pg,x,y+8,b,rar_color(eq[i]),1); } else text(pg,x,y+8,"NOTHING",C_DGREY,1); }
                y+=20; scpy(b,"YOUR BAG  "); ncat(b,bag_n); scat(b,"/"); ncat(b,BAGMAX); text(pg,8,y,b,C_GOLD,1); y+=8;
                int y0=y;
                for(int i=0;i<bag_n;i++){ int col=i/6, yy=y0+(i%6)*8, x=8+col*116;
                    if(cur==i){ rect(pg,x-4,yy-1,114,8,6); text(pg,x-3,yy,">",C_CYAN,1); }
                    gear_name(&bag[i],b); b[26]=0; text(pg,x+3,yy,b,rar_color(&bag[i]),1);
                    if(is_gear(&bag[i])){ Delta d=gear_delta(&bag[i]); int v=d.dmg+d.def+d.hp/4; if(v) text(pg,x+100,yy,v>0?"+":"-",v>0?C_GREEN:C_RED,1); } }
                if(!bag_n) text(pg,10,y0,"EMPTY. PICK THINGS UP IN DUNGEONS",C_DGREY,1);
                y=y0+50; rect(pg,4,y-2,232,1,6);
                if(bag_n){ describe(&bag[cur],b); b[56]=0; text(pg,8,y+1,b,C_WHITE,1);
                    int sc=delta_text(&bag[cur],b); b[56]=0; text(pg,8,y+9,b, sc>0?C_GREEN:sc<0?C_RED:C_GREY,1);
                    scpy(b,"WORTH "); ncat(b,item_value(&bag[cur])); scat(b,"G");
                    if(is_gear(&bag[cur])){ scat(b,"  SCRAP "); ncat(b,scrap_of(bag[cur].rar)); scat(b,"  KEEP +"); ncat(b,keep_points(bag[cur].rar)); }
                    rtext(pg,232,y+17,b,C_GOLD); }
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
            } else if(page==4){
                text(pg,8,y,"FEATS",C_GOLD,1); { int rn=0; for(u32 r=relic_bits;r;r>>=1) rn+=r&1; scpy(b,"RELICS "); ncat(b,rn); scat(b,"/16  WEIRD WORLDS "); ncat(b,weird_seen); text(pg,40,y,b,rn?C_UNIQ:C_DGREY,1); } { int dn=0; for(int i=0;i<NACH;i++) dn+=(ach_done>>i)&1; scpy(b,""); ncat(b,dn); scat(b,"/"); ncat(b,NACH); scat(b," DONE"); rtext(pg,232,y,b,C_GREY); } y+=9;
                for(int i=0;i<NACH;i++){ int done=(ach_done>>i)&1, hv=ach_have(i); if(hv>ach[i].goal) hv=ach[i].goal; int yy=y+i*8;
                    text(pg,8,yy,ach[i].name,done?C_GOLD:C_WHITE,1); text(pg,76,yy,ach[i].what,done?C_GREY:C_DGREY,1);
                    if(done) text(pg,180,yy,"DONE",C_GREEN,1); else { hbar(pg,180,yy+2,26,hv,ach[i].goal,C_CYAN); scpy(b,""); ncat(b,hv); scat(b,"/"); ncat(b,ach[i].goal); text(pg,209,yy,b,C_DGREY,1); } }
            } else if(page==6){ map_page(pg);
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
            if(menu_msg[0]){ char mm[40]; scpy(mm,menu_msg); mm[34]=0; text(pg,8,154,mm,C_GREEN,1); }
        }
        vsync(); read_keys();
        if(HIT(K_START)) break;
        if(HIT(K_L)){ page=(page+6)%7; cur=0; dirty=1; sys_msg[0]=0; }
        if(HIT(K_R)){ page=(page+1)%7; cur=0; dirty=1; sys_msg[0]=0; }
        int nmax = page==0?4 : page==1?bag_n : page==2?NSPELL : page==5?5 : 0;
        if(nmax){ if(HIT(K_UP)){ cur=(cur+nmax-1)%nmax; dirty=1; } if(HIT(K_DOWN)){ cur=(cur+1)%nmax; dirty=1; }
            if(page==1){ if(HIT(K_RIGHT) && cur+6<nmax){ cur+=6; dirty=1; } if(HIT(K_LEFT) && cur>=6){ cur-=6; dirty=1; } } }
        if(HIT(K_A)){
            dirty=1;
            if(page==0 && stat_pts){ int *sv[4]={&st_str,&st_dex,&st_vit,&st_mag}; (*sv[cur])++; stat_pts--; int oldmax=maxh; recalc(); hearts+=maxh-oldmax; sfx(2); }
            if(page==1 && bag_n){ use_index(cur); if(cur>=bag_n && cur) cur--; }
            if(page==2 && spell_known[cur]) spell_sel=cur;
            if(page==5){ sys_action(pg,cur); if(sys_req) break; }
        }
        if(HIT(K_B) && page==1 && bag_n){
            if(is_gear(&bag[cur])){ int n=scrap_of(bag[cur].rar); scrap+=n; char m[32]; scpy(m,"SCRAPPED: +"); ncat(m,n); scat(m," TECH SCRAP"); drop_at(cur); say(m); sfx(13); }
            else drop_at(cur);
            if(cur>=bag_n && cur) cur--; dirty=1; }
        if(HIT(K_SEL) && page==1){ send_pet_to_sell(); dirty=1; }
        if(HIT(K_UP)||HIT(K_DOWN)||HIT(K_L)||HIT(K_R)) menu_msg[0]=0;
    }
    in_menu--; wait_release();
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
static void (*list_desc)(int,char*,int*)=0;      // an optional second line about the picked row
static int list_pick(volatile u16 *pg, const char *title, int n, void (*row)(int,char*,int*), int *sel, const char *help){
    int dirty=1;
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64]; int c;
            box(pg,12,20,216,136); text(pg,20,24,title,C_GOLD,1);
            char g[16]="G"; ncat(g,gold); text(pg,190,24,g,C_GOLD,1);
            for(int i=0;i<n;i++){ c=C_GREY; row(i,b,&c); b[48]=0; if(i==*sel){ text(pg,16,34+i*8,">",C_CYAN,1); if(c==C_GREY) c=C_WHITE; } text(pg,22,34+i*8,b,c,1); }
            text(pg,20,148,help,C_CYAN,1);
            if(list_desc && n){ int c2=C_GREY; list_desc(*sel,b,&c2); b[52]=0; text(pg,20,124,b,c2,1); }
            if(menu_msg[0]) text(pg,20,139,menu_msg,C_GREEN,1); else if(list_desc && n){ int c2=C_GREY; list_desc(-1-*sel,b,&c2); b[52]=0; text(pg,20,132,b,c2,1); }
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
            for(int i=0;i<n && y<136;i++, y+=8){
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
            for(int i=0;i<n && y<118;i++,y+=8){ Gear *g=items[i]; if(i==sel) text(pg,16,y,">",C_CYAN,1);
                if(!g->it){ text(pg,22,y, i==0?"BITE: NOTHING":i==1?"WEAR: NOTHING":"CHARM: NOTHING",C_DGREY,1); continue; }
                scpy(b, i<3 ? (i==0?"BITE: ":i==1?"WEAR: ":"CHARM: ") : "BAG: "); char nm[40]; gear_name(g,nm); nm[26]=0; scat(b,nm); scat(b," +"); ncat(b,g->lvl); text(pg,22,y,b,rar_color(g),1); }
            Gear *g=items[sel];
            if(g->it){ int gd=wb_good(g); scpy(b,"UPGRADE: "); ncat(b,wb_cost_gold(g)); scat(b,"G + 2 "); scat(b,goods[gd]); scat(b,"   HAVE "); ncat(b,cargo[gd]);
                int ok = gold>=wb_cost_gold(g) && cargo[gd]>=2 && g->lvl<97; text(pg,20,124,b,ok?C_GREEN:C_RED,1);
                Gear t=*g; t.lvl+=2; Delta d=gear_delta(&t); Gear old=*g; *g=t; recalc(); *g=old; recalc();
                scpy(b,"NEXT: "); if(g->it==IT_THORN){ scat(b,"DAMAGE "); ncat(b,weapon_base(g)); scat(b," -> "); ncat(b,weapon_base(&t)); } else if(g->it==IT_SHELL){ scat(b,"DEFENCE "); ncat(b,armor_base(g)); scat(b," -> "); ncat(b,armor_base(&t)); } else { scat(b,"STRONGER CHARM EFFECTS"); } (void)d;
                text(pg,20,132,b,C_WHITE,1); }
            if(menu_msg[0]) text(pg,20,140,menu_msg,C_GREEN,1);
            text(pg,20,148,"A: UPGRADE  B: BACK   MINE GOODS ON PLANETS",C_CYAN,1); }
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
static int shop_menu(void){   // returns 1 to fly on
    volatile u16 *pg=VISPG;
    oam_hide_all(); oam_flush();
    int sel=0; in_menu++; menu_msg[0]=0;
    while(1){
        const char *main_opt[13]={ "LAUNCH INTO SPACE","SUPPLIES","GEAR SHOP","SELL","QUEST BOARD","HEAL YOU AND BUDDY  25G","IDENTIFY BERRY/SEED 30G","TROPHY CASE: KEEP OR SCRAP","WORKBENCH: UPGRADE GEAR","MIRROR: CHANGE YOUR LOOK","HANGAR: REDESIGN YOUR SHIP","TALK TO BOLT","LEAVE"};
#define NSHOP 13
        int can_fly = 1, next_ok = !endless && zone_cleared && zone<5 && cluster_no==0; char nxt[40];
        if(next_ok){ scpy(nxt,"AUTOPILOT TO "); scat(nxt,tint_name[zone+1]); main_opt[0]=nxt; }
        int dirty=1; wait_release();
        while(1){
            if(dirty){ dirty=0; box(pg,12,20,216,136); text(pg,20,24,"YOUR SHIP",C_GOLD,1); char g[16]="G"; ncat(g,gold); text(pg,190,24,g,C_GOLD,1);
                for(int i=0;i<NSHOP;i++){ int c=(i==0&&!can_fly)?C_DGREY:(i==sel?C_WHITE:(i==0&&next_ok)?C_GREEN:(i==11)?C_CYAN:C_GREY); if(i==sel) text(pg,16,34+i*8,">",C_CYAN,1); text(pg,22,34+i*8,main_opt[i],c,1); }
                if(next_ok) rtext(pg,224,44,"HOLD A: FLY YOURSELF",C_DGREY);
                { char hb[40]; scpy(hb,ship.name); scat(hb,"  HOLD "); ncat(hb,cargo_n()); scat(hb,"/"); ncat(hb,cargo_cap+ship_hold_bonus()); rtext(pg,224,34,hb,C_CYAN); }
                if(q_type!=Q_NONE && q_done) text(pg,130,70,"! QUEST DONE",C_GREEN,1);
                if(menu_msg[0]) text(pg,20,139,menu_msg,C_GREEN,1);
                text(pg,20,148,"A: CHOOSE  B: LEAVE",C_CYAN,1); }
            vsync(); read_keys(); if(keys!=keys_old) dirty=1;
            if(HIT(K_UP)){ sel=(sel+NSHOP-1)%NSHOP; menu_msg[0]=0; } if(HIT(K_DOWN)){ sel=(sel+1)%NSHOP; menu_msg[0]=0; }
            if(HIT(K_B)){ in_menu--; wait_release(); return 0; }
            if(HIT(K_A)) break;
        }
        if(sel==12){ in_menu--; wait_release(); return 0; }
        if(sel==11){ bolt_talk(pg); box(pg,12,20,216,136); continue; }
        if(sel==10){ make_ship(pg,0); box(pg,12,20,216,136); continue; }
        if(sel==9){ make_bug(pg,0); box(pg,12,20,216,136); continue; }
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
