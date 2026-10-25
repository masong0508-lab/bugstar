// ================================================================= EXTRAS: one page of the pause screen for everything the game can unlock
// Every line is one thing that really sits behind a flag: UNLOCKED or LOCKED, and when it is locked, what you need to do (plain words).
// A opens it from here when that is safe (HOW BIG IS IT?, the options for INSANE-SECT, the Kart Club on a planet); otherwise it says where to go.
// Nothing here is saved: every line is worked out from flags the game already keeps, so old saves just work.
enum { EX_BIG, EX_KARTW, EX_KCLS, EX_INSANE, EX_STARS, EX_FAR, EX_FORGE, EX_TECH, EX_SHAPE, EX_CHART, EX_MAP, EX_FOLD };   // (EX_N lines, see ui.inc)
#define EX_VIS 8                                                        // lines on show at once
static const char *const ex_name[EX_N]={"HOW BIG IS IT?","KART WORLD","KART CLASSES","INSANE-SECT","STORY STARS","FAR WORLDS","DUNGEON FORGE","TECH CORES","OTHER SHAPES","LONG-RANGE CHART","MAP COMPUTER","FOLD DRIVE"};

static int ex_kart_world(void){ KWSave keep=kw; int u=kw_unlocked(); kw=keep; return u; }       // (the same check as the title screen; it must not disturb the Kart World data in memory)
static int ex_shapes(void){ int n=0; for(int k=0;k<3;k++) n+=(forms_open>>k)&1; return n; }
// is line i open? When it is, tag gets a few words to show beside UNLOCKED; when not, req gets what you need (at most 26 letters, so "LOCKED: " + req fits a message line)
static int ex_info(int i, char *req, char *tag){
    req[0]=0; tag[0]=0;
    switch(i){
    case EX_BIG:    if(big_unlocked()){ scpy(tag,"A: OPEN IT"); return 1; } scpy(req,"BEAT THE ANT KING FIRST"); return 0;
    case EX_KARTW:  if(ex_kart_world()){ scpy(tag,"FROM THE TITLE SCREEN"); return 1; } scpy(req,"BEAT THE GAME FIRST"); return 0;
    case EX_KCLS:   if(kcls_open>=4){ scpy(tag,"ALL 4 OPEN"); return 1; }
                    scpy(req, kcls_open<=1 ? "WIN A LARVA CUP" : kcls_open==2 ? "WIN A PUPA CUP" : "WIN AN IMAGO CUP"); return 0;
    case EX_INSANE: if(opt_insane){ scpy(tag,"A: OPTIONS"); return 1; } scpy(req,"BEAT ANT KING ON NIGHTMARE"); return 0;
    case EX_STARS:  if(zones_open>=6){ scpy(tag,"ALL 6 OPEN"); return 1; } scpy(req,"BEAT THE ZONE "); ncat(req,zones_open<1?1:zones_open); scat(req," BOSS"); return 0;
    case EX_FAR:    if(ant_king){ scpy(tag,"ON THE GALAXY MAP"); return 1; } scpy(req,"BEAT THE ANT KING FIRST"); return 0;
    case EX_FORGE:  if(ant_king){ scpy(tag,"AT YOUR SHIP"); return 1; } scpy(req,"BEAT THE ANT KING FIRST"); return 0;
    case EX_TECH:   if(beast_stage>=1){ scpy(tag,"AT A STATION"); return 1; } scpy(req,"BEAT THE BABY TECHNOBEAST"); return 0;
    case EX_SHAPE:  if(forms_open>1){ scpy(tag,"AT YOUR SHIP"); return 1; } scpy(req,"VISIT WEIRD WORLDS"); return 0;
    case EX_CHART:  if(ant_king){ scpy(tag,"ON THE GALAXY MAP"); return 1; } scpy(req,"BEAT THE ANT KING FIRST"); return 0;
    case EX_MAP:    if(map_lv>0){ scpy(tag,"MK"); ncat(tag,map_lv); scat(tag," OF 5"); return 1; }
                    scpy(req, ant_king ? "BUY MK1 AT A STATION" : "BEAT THE ANT KING FIRST"); return 0;
    default:        if(fold_lv>0){ scpy(tag,"MK"); ncat(tag,fold_lv); scat(tag," OF 6"); return 1; }
                    if(!ant_king) scpy(req,"BEAT THE ANT KING FIRST"); else if(warp_lv<1) scpy(req,"NEEDS WARP DRIVE MK1 FIRST"); else scpy(req,"BUY MK1 AT A STATION"); return 0;
    }
}
// two short lines about line i: what it is, and how to get to it (b1, b2 hold 56)
static void ex_lines(int i, int open, char *b1, char *b2){
    b1[0]=b2[0]=0;
    switch(i){
    case EX_BIG:    scpy(b1,"THE HUGE NUMBERS IN THIS GAME"); scpy(b2, open ? "PRESS A TO OPEN IT NOW" : "ONCE OPEN: PRESS A HERE TO OPEN IT"); break;
    case EX_KARTW:  scpy(b1,"THE KART RACING GAME, ALL ON ITS OWN"); scpy(b2, open ? "START IT FROM THE TITLE SCREEN" : "ONCE OPEN: START IT FROM THE TITLE SCREEN"); break;
    case EX_KCLS:   scpy(b1,"OPEN: "); for(int k=0;k<4 && k<kcls_open;k++){ if(k) scat(b1,", "); scat(b1,kcls_name[k]); }
                    scpy(b2,"A: KART CLUB. CUPS ARE RUN AT A RACE POST"); break;
    case EX_INSANE: scpy(b1,"THE HARDEST DIFFICULTY"); scpy(b2, open ? "PRESS A FOR OPTIONS, THEN PICK DIFFICULTY" : "ONCE OPEN: PICK IT IN OPTIONS"); break;
    case EX_STARS:  scpy(b1,"STAR WORLDS OPEN: "); ncat(b1,zones_open>6?6:zones_open); scat(b1," OF 6"); scpy(b2,"FLY TO THEM ON THE GALAXY MAP"); break;
    case EX_FAR:    scpy(b1,"THE ENDLESS GALAXY BEYOND THE STORY"); scpy(b2,"FLY TO THEM ON THE GALAXY MAP"); break;
    case EX_FORGE:  scpy(b1,"MAKE YOUR OWN DUNGEON RUNS"); scpy(b2,"GO TO YOUR SHIP: DUNGEON FORGE"); break;
    case EX_TECH:   scpy(b1,"EACH TECH CORE MAKES YOUR LASER 4X"); scpy(b2,"BUY THEM AT A STATION'S SHIPYARD"); break;
    case EX_SHAPE:  scpy(b1,"SHAPES YOU HAVE BEEN: "); ncat(b1,ex_shapes()); scat(b1," OF 3"); scpy(b2,"SWITCH AT YOUR SHIP: YOUR SHAPE"); break;
    case EX_CHART:  scpy(b1,"SEE FAR PLACES AND FOLD THERE"); scpy(b2,"ON THE GALAXY MAP: START, THEN L"); break;
    case EX_MAP:    scpy(b1,"THE CHART SEES FARTHER AT EACH LEVEL"); scpy(b2,"BUY IT AT A STATION: NAV TAB"); break;
    default:        scpy(b1,"FOLD FARTHER OUT AT EACH LEVEL"); scpy(b2,"BUY IT AT A STATION: NAV TAB"); break;
    }
}
static const u8 ex_tick[7][2]={{0,2},{1,3},{2,4},{3,3},{4,2},{5,1},{6,0}};     // a tick (green) and a cross (red), drawn pixel by pixel, each a little bold
static void ex_mark(volatile u16 *pg, int x, int y, int open){
    if(open){ for(int k=0;k<7;k++){ pix(pg,x+ex_tick[k][0],y+ex_tick[k][1],C_GREEN); pix(pg,x+ex_tick[k][0]+1,y+ex_tick[k][1],C_GREEN); } }
    else for(int k=0;k<5;k++){ pix(pg,x+k,y+k,C_RED); pix(pg,x+k+1,y+k,C_RED); pix(pg,x+4-k,y+k,C_RED); pix(pg,x+5-k,y+k,C_RED); } }
// the page, inside the card the caller drew at (4,37,232,111). top: the first line on show (kept by the caller). *foot: the hint for the bottom strip
static void extras_page(volatile u16 *pg, int cur, int *top, const char **foot){
    char b[56], b2[56], rq[40], tg[24]; int n_open=0;
    for(int i=0;i<EX_N;i++) n_open+=ex_info(i,rq,tg);
    if(cur<*top) *top=cur;
    if(cur>=*top+EX_VIS) *top=cur-EX_VIS+1;
    ui_head(pg,9,40,140,"EXTRAS",C_UNIQ);
    scpy(b,""); ncat(b,n_open); scat(b," OF "); ncat(b,EX_N); scat(b," UNLOCKED"); rtext(pg,231,40,b,n_open==EX_N?C_GREEN:U_SOFT);
    for(int r=0;r<EX_VIS;r++){ int i=*top+r, y=50+r*9; if(i>=EX_N) break;
        int open=ex_info(i,rq,tg);
        if(i==cur) ui_sel(pg,7,y-1,226,9,HI);
        ex_mark(pg,10,y,open);
        text(pg,22,y,ex_name[i], open || i==cur ? C_WHITE : C_GREY, 1);
        if(open){ text(pg,92,y,"UNLOCKED",C_GREEN,1); text(pg,130,y,tg,i==cur?C_WHITE:U_SOFT,1); }
        else { text(pg,92,y,"LOCKED",C_RED,1); text(pg,121,y,rq,C_GOLD,1); } }
    if(*top>0) text(pg,231,50,"^",C_WHITE,1);
    if(*top+EX_VIS<EX_N) text(pg,231,115,"v",C_WHITE,1);
    int open=ex_info(cur,rq,tg);
    rect(pg,9,123,222,1,U_RAISE);
    ex_lines(cur,open,b,b2);
    text(pg,10,126,b,C_WHITE,1); text(pg,10,135,b2,open?C_GREEN:U_SOFT,1);
    *foot = cur==EX_BIG ? (open?"A: OPEN IT":"A: WHAT YOU NEED") : cur==EX_KCLS ? "A: KART CLUB" : cur==EX_INSANE ? (open?"A: OPTIONS":"A: WHAT YOU NEED") : open ? "A: WHERE TO GO" : "A: WHAT YOU NEED";
}
// A on line cur. Returns 1 if the pause screen should close (the Kart Club is next, see planet.inc)
static int extras_press(volatile u16 *pg, int cur){
    char rq[40], tg[24], m[40]; int open=ex_info(cur,rq,tg);
    if(cur==EX_KCLS){                                                  // the Kart Club works from the start: only more classes are locked
        if(scene==0 && !kr_mode){ kart_req=1; return 1; }
        scpy(menu_msg,"THE KART CLUB IS AT YOUR SHIP"); return 0; }
    if(!open){ scpy(m,"LOCKED: "); scat(m,rq); scpy(menu_msg,m); sfx(12); return 0; }
    if(cur==EX_BIG){ oam_hide_all(); oam_flush(); size_screen(pg,0); hud_now(); return 0; }
    if(cur==EX_INSANE){ options_screen(pg); return 0; }
    sfx(2);
    switch(cur){
    case EX_KARTW:  scpy(menu_msg,"START IT FROM THE TITLE SCREEN"); break;
    case EX_STARS:
    case EX_FAR:    scpy(menu_msg,"FLY THERE ON THE GALAXY MAP"); break;
    case EX_FORGE:  scpy(menu_msg,"GO TO YOUR SHIP: DUNGEON FORGE"); break;
    case EX_TECH:   scpy(menu_msg,"BUY IT AT A STATION'S SHIPYARD"); break;
    case EX_SHAPE:  scpy(menu_msg,"AT YOUR SHIP: YOUR SHAPE"); break;
    case EX_CHART:  scpy(menu_msg,"GALAXY MAP: START, THEN L"); break;
    default:        scpy(menu_msg,"BUY UPGRADES AT A STATION: NAV TAB"); break;
    }
    return 0;
}
