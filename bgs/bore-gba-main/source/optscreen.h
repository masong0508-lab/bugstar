// optscreen.h - the OPTIONS screen (settingsScreen). Eight pages, every row is one choice or one action.
//
//   L / R  change page        UP / DOWN  pick a row        LEFT / RIGHT (or A)  change it
//   SELECT  put the row back to its normal value           B or START  back (everything is saved)
//   A gold dot on the right of a row means it is not at its normal value.
//
// HOW TO ADD AN OPTION: add it to opts.h (see the steps there), give it a label list below and an XR(...) row in the page it
// belongs to. HOW TO ADD A PAGE: add a row table, a line in optPages[] and raise NOPG.
// This file is included from main.c at the old SETTINGS screen's place: it needs presetNm/presetOf/setPreset/setDefaults/autoTune/
// measureDraw/capLevel/costCache from there, slots.h for the confirm menus, and tmStart()/R_TM2CNT.
enum { OR_VAR, OR_XO, OR_JB, OR_PRESET, OR_ACT };   // a plain u8 variable, an xo[] option, the jukebox mode, the preset, an action
enum { OA_TUNE, OA_LIFESAVE, OA_LIFEERASE, OA_ROOMERASE, OA_SLOTSERASE, OA_ALLERASE, OA_SRAMTEST, OA_RESET, OA_BTNTEST };
typedef struct { u8 kind, idx, n, def; u8*v; const char*nm; const char* const* lab; const char*d0; const char*d1; } OptRow;
#define VR(var,n,def,nm,lab,d0,d1) {OR_VAR,0,n,def,&var,nm,lab,d0,d1}
#define XR(i,nm,lab,d0,d1) {OR_XO,i,0,0,0,nm,lab,d0,d1}
#define AR(a,nm,d0,d1) {OR_ACT,a,0,0,0,nm,0,d0,d1}
static u8 gInPlay;   // 1 while the life game runs (some actions only make sense, or are only safe, in one place)

static const char* const lbFps[4]={"60 FPS","30 FPS","20 FPS","15 FPS"}, *const lbWalls[3]={"FULL","CUTAWAY","LOW"}, *const lbPat[2]={"PLAIN","PATTERNS"},
    *const lbOnOff[2]={"OFF","ON"}, *const lbShow[3]={"OFF","FPS","DETAIL"}, *const lbWarn[2]={"ON","OFF"}, *const lbRom[2]={"FAST","SAFE"},
    *const lbCam[4]={"OFF","OVER 10000","OVER 5000","OVER 2000"}, *const lbHud[3]={"FULL","SLIM","OFF"};
static const char* const lbNeed[5]={"OFF","SLOW","NORMAL","FAST","BRUTAL"}, *const lbHunger[4]={"OFF","SLOW","NORMAL","FAST"},
    *const lbDay[5]={"3 MIN","6 MIN","12 MIN","24 MIN","STOPPED"}, *const lbBills[4]={"NONE","HALF","NORMAL","DOUBLE"},
    *const lbQuota[4]={"EASY","NORMAL","HARD","INSANE"}, *const lbScore[4]={"X0.5","X1","X2","X3"},
    *const lbCombo[4]={"1.5 SEC","2.5 SEC","4 SEC","6 SEC"}, *const lbSpeed[4]={"80 %","100 %","125 %","150 %"},
    *const lbDays[10]={"1 DAY","2 DAYS","3 DAYS","5 DAYS","7 DAYS","10 DAYS","14 DAYS","21 DAYS","30 DAYS","60 DAYS"},
    *const lbDaysA[11]={"1 DAY","2 DAYS","3 DAYS","5 DAYS","7 DAYS","10 DAYS","14 DAYS","21 DAYS","30 DAYS","60 DAYS","FOREVER"}, *const lbAging[4]={"OFF","SLOW","NORMAL","FAST"}, *const lbHurt[3]={"NORMAL","GENTLE","NO DEATH"}, *const lbBubble[3]={"OFF","URGENT","ALL"}, *const lbShown[2]={"HIDDEN","SHOWN"};
static const char* const lbVol4[4]={"FULL","HALF","QUARTER","OFF"}, *const lbVol3[3]={"FULL","HALF","QUARTER"},
    *const lbBtn[4]={"NORMAL","A B SWAPPED","L R SWAPPED","BOTH SWAPPED"}, *const lbRep[3]={"SLOW","NORMAL","FAST"},
    *const lbClock[3]={"24 HOUR","12 HOUR","HIDDEN"}, *const lbToast[3]={"SHORT","NORMAL","LONG"},
    *const lbCont[3]={"ROOM","ROOM+PERSON","ALL THREE"};

static const OptRow pgVideo[]={
 {OR_PRESET,0,0,0,0,"PRESET",0,"LOOKS BALANCED SPEED BATTERY  ONE TAP SETUP","CHANGING ANYTHING BELOW MAKES IT CUSTOM"},
 AR(OA_TUNE,"AUTO TUNE","PRESS A  TESTS YOUR SCREEN AND PICKS THE","PRETTIEST PRESET THAT STAYS SMOOTH"),
 VR(sFps,4,1,"FRAME RATE",lbFps,"HOW OFTEN THE PICTURE REDRAWS","LOWER IS FASTER  THE GAME KEEPS ITS PACE"),
 VR(sWall,3,1,"WALLS",lbWalls,"FULL SHOWS EVERY WALL  CUTAWAY LOWERS THE","WALLS IN FRONT  LOW DRAWS THEM ALL SHORT"),
 VR(sWp,2,1,"WALLPAPER",lbPat,"PATTERNED OR PLAIN COLOUR WALLS","PLAIN IS QUICKER TO DRAW"),
 VR(sFl,2,1,"FLOORS",lbPat,"PATTERNED OR PLAIN COLOUR FLOORS","PLAIN IS QUICKER TO DRAW"),
 VR(sShad,2,1,"SHADOWS",lbOnOff,"THE DARK SPOT UNDER YOUR FEET","OFF SAVES A LITTLE DRAWING"),
 VR(sShow,3,0,"PERFORMANCE INFO",lbShow,"SHOWS FPS WHILE YOU PLAY  DETAIL ALSO SHOWS","LOAD  100 MEANS A FRAME IS JUST FITTING"),
 VR(sNoWarn,2,0,"SPEED WARNING",lbWarn,"ON SHOWS TOO SLOW WHEN THE PICTURE","CANT KEEP UP  OFF HIDES THE WARNING"),
 VR(sRom,2,0,"ROM SPEED",lbRom,"FAST IS RIGHT FOR MOST CARTS AND EMULATORS","SAFE IF A FLASH CART FREEZES OR GLITCHES"),
};
static const OptRow pgPlay[]={
 XR(XO_NEED,"NEEDS",lbNeed,"HOW FAST REST CLEAN AND COMFY RUN DOWN","OFF FREEZES THEM  BRUTAL IS TWICE AS FAST"),
 XR(XO_HUNGER,"FOOD AND WC",lbHunger,"HOW FAST HUNGER AND THE BLADDER BUILD","OFF MEANS NO ACCIDENTS AND NO FAINTING"),
 XR(XO_DAY,"DAY LENGTH",lbDay,"REAL MINUTES IN ONE GAME DAY  STOPPED","FREEZES THE CLOCK  SLEEP NO LONGER SKIPS TIME"),
 XR(XO_JOB,"CAREER",lbOnOff,"OFF REMOVES SHIFTS QUOTAS PAY AND BILLS","A FREE PLAY LIFE WITH NO WORK"),
 XR(XO_QUOTA,"JOB QUOTA",lbQuota,"TRICK POINTS NEEDED IN A SHIFT","EASY 60  NORMAL 100  HARD 150  INSANE 200"),
 XR(XO_BILLS,"BILLS",lbBills,"WHAT THE BILL AT MIDNIGHT COSTS","NONE  HALF  NORMAL  OR DOUBLE"),
 XR(XO_SCORE,"SCORE",lbScore,"MULTIPLIES EVERY TRICK AND GRIND SCORE","QUOTAS AND PAY FOLLOW THE SCORE"),
 XR(XO_COMBO,"COMBO WINDOW",lbCombo,"TIME YOU HAVE TO LAND THE NEXT TRICK","BEFORE THE CHAIN IS BANKED"),
 XR(XO_SPEED,"TOP SPEED",lbSpeed,"HOW FAST YOU WALK RUN AND SKATE","80 TO 150 % OF NORMAL"),
 XR(XO_MOODFX,"MOOD EFFECTS",lbOnOff,"SAD SLOWS YOU  STOKED SPEEDS YOU UP AND MOOD","CHANGES TRICK POINTS  OFF IGNORES MOOD"),
 XR(XO_HURT,"HURT",lbHurt,"GENTLE HALVES FALL DAMAGE","NO DEATH MEANS A FALL CAN NEVER KILL"),
 XR(XO_AUTOSAVE,"AUTO SAVE LIFE",lbOnOff,"SAVES AT MIDNIGHT PAYDAY AND THE PAUSE MENU","OFF  ONLY SLOTS AND SAVE LIFE NOW SAVE IT"),
};
static const OptRow pgAges[]={
 XR(XO_AGING,"AGING",lbAging,"OFF STAYS AT THE AGE YOU PICKED  SLOW DOUBLES","EVERY STAGE BELOW  FAST HALVES THEM"),
 XR(XO_AGEB,"BABY LASTS",lbDays,"GAME DAYS AS A BABY  THE ONE STAGE YOU","CANNOT STEER"),
 XR(XO_AGEC,"CHILD LASTS",lbDays,"GAME DAYS AS A CHILD","BEFORE GROWING INTO A TEEN"),
 XR(XO_AGET,"TEEN LASTS",lbDays,"GAME DAYS AS A TEEN","THE CAREER STARTS AT THIS STAGE"),
 XR(XO_AGEA,"ADULT LASTS",lbDaysA,"GAME DAYS AS AN ADULT BEFORE BECOMING AN","ELDER  FOREVER NEVER GROWS OLD"),
};
static const OptRow pgAudio[]={
 VR(sSnd,2,1,"SOUND",lbOnOff,"SOUND OFF SKIPS SOUND DECODING","SAVES A LITTLE SPEED AND BATTERY"),
 XR(XO_SFX,"SFX VOLUME",lbVol3,"LOUDNESS OF GRUNTS BONKS AND CRIES","QUARTER IS THE QUIETEST"),
 XR(XO_MUS,"MUSIC VOLUME",lbVol4,"THE TITLE MUSIC AND THE JUKEBOX","OFF SILENCES THEM"),
 XR(XO_GAMEMUS,"GAME MUSIC",lbOnOff,"JUKEBOX SONGS IN SHUFFLED ORDER WHILE YOU PLAY","MIXING COSTS SPEED  SOUND EFFECTS DUCK IT"),
 XR(XO_TITLEMUS,"TITLE MUSIC",lbOnOff,"PLAY THE DIPPER MAN ON THE TITLE SCREEN","OFF KEEPS THE TITLE QUIET"),
 {OR_JB,0,0,0,0,"JUKEBOX MODE",jbModeNm,"SHUFFLE PLAYS YOUR SAVED RANDOM SONG ORDER","IN ORDER  OR  REPEAT ONE SONG  OPEN FROM MENU"},
};
static const OptRow pgInput[]={
 XR(XO_BTN,"BUTTONS",lbBtn,"SWAP A AND B  OR L AND R  ON EVERY SCREEN","USE BUTTON TEST BELOW TO CHECK IT"),
 XR(XO_REPEAT,"CURSOR REPEAT",lbRep,"HOW FAST THE EDITOR CURSOR REPEATS WHEN","YOU HOLD THE D PAD"),
 AR(OA_BTNTEST,"BUTTON TEST","SHOWS WHICH BUTTONS THE GAME SEES","PRESS A  LEAVE WITH SELECT AND START"),
};
static const OptRow pgHud[]={
 VR(sHud,3,0,"INFO ON SCREEN",lbHud,"FULL SHOWS ALL  SLIM KEEPS SCORE AND BARS","OFF HIDES ALL OF IT  ALERTS STILL SHOW"),
 XR(XO_CLOCK,"CLOCK",lbClock,"HOW THE GAME CLOCK IS SHOWN","24 HOUR  12 HOUR  OR HIDDEN"),
 XR(XO_BUBBLE,"THOUGHT BUBBLE",lbBubble,"THE BUBBLE OVER YOUR HEAD","URGENT SHOWS ONLY NEEDS  ALL ADDS WANTS"),
 XR(XO_WANTS,"WANTS AND FEARS",lbShown,"THE GREEN AND RED LINES UNDER THE BARS","THEY STILL COUNT WHEN HIDDEN"),
 VR(sCam,4,1,"ACTION CAM",lbCam,"AFTER A BIG COMBO THE CAMERA ZOOMS AND SPINS","ALL 4 VIEWS  PICK HOW BIG A COMBO TRIGGERS IT"),
 XR(XO_ACCENT,"ACCENT COLOUR",accentNm,"COLOUR OF MENUS HEADINGS AND HUD NUMBERS","SEE IT CHANGE RIGHT HERE"),
 XR(XO_TOAST,"MESSAGE TIME",lbToast,"HOW LONG POP UP MESSAGES STAY ON SCREEN","SHORT  NORMAL  LONG"),
};
static const OptRow pgRooms[]={
 XR(XO_MINI,"EDITOR MINIMAP",lbOnOff,"THE SMALL MAP IN THE ROOM BUILDER","OFF GIVES A CLEARER VIEW"),
 XR(XO_EDSAVE,"SAVE ON EXIT",lbOnOff,"LEAVING THE ROOM BUILDER SAVES THE ROOM","OFF  USE SAVE MAP WHEN YOU WANT IT"),
 XR(XO_RESETASK,"ASK BEFORE RESET",lbOnOff,"ASK FIRST WHEN YOU CHOOSE RESET MAP","OFF RESETS AT ONCE"),
 XR(XO_SLOTCONT,"SLOTS SAVE",lbCont,"WHAT A SLOT STORES  THE ROOM  ROOM AND","PERSON  OR ROOM PERSON AND LIFE"),
 XR(XO_SLOTCONF,"ASK IN SLOTS",lbOnOff,"ASK BEFORE OVERWRITING LOADING OR DELETING","OFF  DOES IT AT ONCE"),
 XR(XO_SLOTSYNC,"SAVE MAP TO SLOT",lbOnOff,"SAVE MAP IN THE ROOM BUILDER ALSO WRITES","THE ACTIVE SLOT"),
 XR(XO_SLOTBOOT,"BOOT LOADS PERSON",lbOnOff,"AT POWER ON THE CREATURE OF THE ACTIVE","SLOT COMES BACK  THE ROOM IS NOT CHANGED"),
};
static const OptRow pgData[]={
 AR(OA_LIFESAVE,"SAVE LIFE NOW","WRITES THE CURRENT LIFE TO SAVE MEMORY","USE IT FROM THE PAUSE MENU WHILE PLAYING"),
 AR(OA_LIFEERASE,"ERASE LIFE","DELETES THE SAVED LIFE  CASH JOB AND CLOCK","THE NEXT PLAY STARTS A NEW ONE"),
 AR(OA_ROOMERASE,"ERASE SAVED MAP","DELETES THE AUTO SAVED MAP  SLOTS STAY","THE DEFAULT MAP RETURNS AT NEXT POWER ON"),
 AR(OA_SLOTSERASE,"ERASE ALL SLOTS","DELETES EVERY ROOM SLOT","THIS CANNOT BE UNDONE"),
 AR(OA_ALLERASE,"ERASE EVERYTHING","WIPES ALL SAVE MEMORY  MAP SLOTS LIFE","OPTIONS AND JUKEBOX  MAIN MENU ONLY"),
 AR(OA_SRAMTEST,"SAVE MEMORY TEST","CHECKS THAT THIS CART OR EMULATOR KEEPS","SAVES  A GOOD FIRST TEST ON NEW HARDWARE"),
 AR(OA_RESET,"RESET ALL OPTIONS","PUTS EVERY OPTION BACK TO NORMAL","PRESS A"),
};
typedef struct { const char*nm; const OptRow*r; u8 n; } OptPage;
#define NOPG 8
#define PG(nm,t) {nm,t,(u8)(sizeof(t)/sizeof(t[0]))}
static const OptPage optPages[NOPG]={ PG("VIDEO",pgVideo), PG("PLAY",pgPlay), PG("AGES",pgAges), PG("AUDIO",pgAudio), PG("INPUT",pgInput), PG("HUD",pgHud), PG("ROOMS",pgRooms), PG("DATA",pgData) };
static int opPage; static u8 opSel[NOPG];

static u8* rowVar(const OptRow*r){ return r->kind==OR_XO?&xo[r->idx]: r->kind==OR_JB?&sJb: r->v; }
static int rowN(const OptRow*r){ return r->kind==OR_XO?xoCnt[r->idx]: r->kind==OR_JB?3: r->n; }
static int rowDef(const OptRow*r){ return r->kind==OR_XO?xoDef[r->idx]: r->kind==OR_JB?0: r->def; }
static int rowChanged(const OptRow*r){
    if(r->kind==OR_PRESET) return presetOf()!=1;
    if(r->kind==OR_ACT) return 0;
    return *rowVar(r)!=rowDef(r);
}
static int rowCostKey(const OptRow*r){ u8*v=r->v; return r->kind==OR_PRESET||(r->kind==OR_VAR&&(v==&sWall||v==&sWp||v==&sFl||v==&sShad)); }   // rows that change how much a frame costs
static void rowSet(const OptRow*r,int val){   // one place for the side effects of a change
    u8*v=rowVar(r); if(!v) return;
    if(r->kind==OR_JB){ jbSetMode(val); return; }
    *v=(u8)val; if(v==&sSnd&&!sSnd) sfxStop(); if(v==&sRom) applyRom();
}
static void rowChange(const OptRow*r,int d){
    sTunedMsg=0;
    if(r->kind==OR_PRESET){ int p=presetOf(); p=(p==4)?(d>0?0:3):(p+d+4)%4; setPreset(p); return; }
    int n=rowN(r); rowSet(r,(*rowVar(r)+d+n)%n);
}
static void rowReset(const OptRow*r){ sTunedMsg=0; if(r->kind==OR_PRESET) setPreset(1); else if(r->kind!=OR_ACT) rowSet(r,rowDef(r)); }
static const char* rowVal(const OptRow*r){
    if(r->kind==OR_PRESET) return presetNm[presetOf()];
    if(r->kind==OR_ACT) return "PRESS A";
    return r->lab[*rowVar(r)];
}
static int rowHeat(const OptRow*r){   // VIDEO page colours: 0 light (green), 1 medium (yellow), 2 heavy (red), 3 neutral
    if(r->kind!=OR_VAR) return 3; u8*v=r->v;
    if(v==&sFps) return sFps==0?2:sFps==1?1:0;
    if(v==&sWall) return sWall==0?2:sWall==1?1:0;
    if(v==&sWp) return sWp?1:0;
    if(v==&sFl) return sFl?1:0;
    if(v==&sShad) return sShad?1:0;
    if(v==&sRom) return sRom?1:0;
    if(v==&sShow) return sShow==2?1:0;
    return 3;
}
static s16 costCache[24];   // draw cost per walls/wallpaper/floors/shadows combo, 0 = not measured yet
static inline int costKey(void){ return sWall*8+sWp*4+sFl*2+sShad; }

static void drawOptions(void){
    static const u16 heat[4]={ RGB(12,28,10), RGB(31,26,6), RGB(30,10,8), RGB(22,24,26) };
    fillCols(0,ROW_W,RGB(3,4,8));
    box(4,2,232,156);
    for(int i=0;i<NOPG;i++){ int x=4+i*29;   // the page tabs
        if(i==opPage){ rect(x,4,28,10,GOLD); text(x+2,6,optPages[i].nm,RGB(4,3,6),1); } else text(x+2,6,optPages[i].nm,DIMC,1); }
    rect(6,15,228,1,RGB(10,12,16));
    const OptPage*pg=&optPages[opPage]; int sel=opSel[opPage], y0=19, vis=11;
    if(opPage==0){   // the speed meter: how much of the frame the picture needs. 60 / 30 / 20 marks show which frame rate it can hold.
        int cap=capLevel(), want=sFps+1, fill=sCost*100/(3*TICKS_FRAME); if(fill>100) fill=100;
        u16 mc=cap==1?heat[0]:cap<=2?heat[1]:heat[2];
        text(12,19,"DRAW COST",DIMC,1);
        rect(60,19,100,5,RGB(8,10,14)); rect(60,19,fill,5,mc);
        static const int mk[3]={23,57,90}; static const char* const ml[3]={"60","30","20"};
        for(int i=0;i<3;i++){ rect(60+mk[i],18,1,7,WHITE); text(60+mk[i]-3,26,ml[i],DIMC,1); }
        if(cap>want){ if(!sNoWarn){ text(168,19,"TOO SLOW FOR",heat[2],1); text(168,26,"THIS FRAME RATE",heat[2],1); } }
        else { text(168,19,cap==1?"HOLDS 60 FPS":cap==2?"HOLDS 30 FPS":cap==3?"HOLDS 20 FPS":"HOLDS 15 FPS",heat[0],1); text(168,26,sTunedMsg?"TUNED FOR YOU":"SMOOTH",sTunedMsg?heat[0]:DIMC,1); }
        y0=37; vis=9;
    }
    int top=sel-vis/2; if(top>pg->n-vis) top=pg->n-vis; if(top<0) top=0;
    for(int n=0;n<vis&&top+n<pg->n;n++){
        int i=top+n, y=y0+n*9; const OptRow*r=&pg->r[i];
        if(i==sel){ rect(8,y-2,212,9,RGB(6,16,8)); text(12,y,">",WHITE,1); }
        text(20,y,r->nm,i==sel?WHITE:DIMC,1);
        int ch=rowChanged(r); u16 vc=i==sel?WHITE:DIMC;
        if(opPage==0&&rowHeat(r)<3) vc=heat[rowHeat(r)];
        else if(r->kind==OR_ACT){ int a=r->idx; vc=(a==OA_LIFEERASE||a==OA_ROOMERASE||a==OA_SLOTSERASE||a==OA_ALLERASE)?heat[2]:(i==sel?GOLD:DIMC); }
        else if(r->kind==OR_XO&&r->idx==XO_ACCENT) vc=GOLD;
        else if(ch) vc=GOLD;
        text(124,y,rowVal(r),vc,1);
        if(ch) rect(214,y+1,3,3,GOLD);
    }
    if(top>0){ for(int k=0;k<3;k++) rect(227-k,y0+k,1+2*k,1,GOLD); }                              // more rows above
    if(top+vis<pg->n){ for(int k=0;k<3;k++) rect(227-k,y0+vis*9-4+(2-k),1+2*k,1,GOLD); }        // more rows below
    text(12,122,pg->r[sel].d0,WHITE,1); text(12,129,pg->r[sel].d1,DIMC,1);
    text(12,139,"L R PAGE  UP DOWN ROW  LEFT RIGHT CHANGE",RGB(14,16,20),1);
    if(opPage==0){ text(12,148,"GREEN FAST",heat[0],1); text(68,148,"YELLOW MID",heat[1],1); text(124,148,"RED SLOW",heat[2],1); text(172,148,"B BACK",RGB(12,14,16),1); }
    else text(12,148,"SELECT RESETS ROW  B BACK  DOT = CHANGED",RGB(12,14,16),1);
}

static void buttonTest(void){
    static const char* const nm[10]={"A","B","SELECT","START","RIGHT","LEFT","UP","DOWN","R","L"};   // same order as the K_ bits
    while((~REG_KEYINPUT)&0x3FF) vsync();
    for(;;){
        u16 k=keyNow(); if((k&K_SEL)&&(k&K_START)) break;
        fillCols(0,ROW_W,RGB(3,4,8)); box(4,2,232,156); text(12,7,"BUTTON TEST",GOLD,1); text(12,18,"THE KEYS THE GAME SEES",DIMC,1);
        for(int i=0;i<10;i++){ int x=14+(i%5)*44, y=40+(i/5)*30, on=(k>>i)&1;
            rect(x,y,40,20,on?GOLD:RGB(5,6,10)); text(x+4,y+7,nm[i],on?RGB(4,3,6):DIMC,1); }
        text(12,112,"PRESS SELECT AND START TOGETHER TO LEAVE",WHITE,1);
        present();
    }
    while((~REG_KEYINPUT)&0x3FF) vsync();
}
static void optAction(int a,int*remeasure){
    switch(a){
        case OA_TUNE: autoTune(); costCache[costKey()]=(s16)sCost; *remeasure=0; break;
        case OA_RESET: if(menu("RESET ALL OPTIONS",slYesNo,2)==1){ optsDefaults(); setDefaults(); sTunedMsg=0; *remeasure=1; toast("OPTIONS RESET"); } break;
        case OA_BTNTEST: buttonTest(); break;
        case OA_LIFESAVE:
            if(!gInPlay) toast("USE THIS FROM THE PAUSE MENU");
            else { simsSaveNow(); toast("LIFE SAVED"); } break;
        case OA_LIFEERASE: if(menu("ERASE THE SAVED LIFE",slYesNo,2)==1){ SIM_SRAM[0]=0; toast("SAVED LIFE ERASED"); } break;
        case OA_ROOMERASE: if(menu("ERASE THE SAVED MAP",slYesNo,2)==1){ SRAM_BASE[0]=0; toast("SAVED MAP ERASED"); } break;
        case OA_SLOTSERASE: if(menu("ERASE ALL ROOM SLOTS",slYesNo,2)==1){ slEraseAll(); toast("ALL SLOTS ERASED"); } break;
        case OA_ALLERASE:
            if(gInPlay){ toast("USE THIS FROM THE MAIN MENU"); break; }
            if(menu("ERASE ALL SAVE MEMORY",slYesNo,2)==1 && menu("REALLY ERASE EVERYTHING",slYesNo,2)==1){
                volatile u8*m=SRAM_BASE; for(int i=0;i<32768;i++) m[i]=0;
                optsDefaults(); setDefaults(); settingsSave(); mapReset(); for(int i=0;i<LK_N;i++) look[i]=0; stage=AG_ADULT; ageDays=0; starter(); setColors(); jbSetup(); *remeasure=1;
                toast("EVERYTHING ERASED"); }
            break;
        case OA_SRAMTEST:{
            volatile u8*m=SRAM_BASE+SRAM_TEST; int ok=1;
            for(int i=0;i<16;i++) m[i]=(u8)(i*37+0x5A);
            for(int i=0;i<16;i++) if(m[i]!=(u8)(i*37+0x5A)) ok=0;
            for(int i=0;i<16;i++) m[i]=0;
            toast(ok?"SAVE MEMORY WORKS":"NO SAVE MEMORY HERE"); } break;
    }
}

static void settingsScreen(void){   // the OPTIONS screen (the name stays so every caller keeps working)
    int dirty=1, remeasure=1, idle=0; u16 prev=keyNow(); tmStart(); sTunedMsg=0;
    for(int i=0;i<24;i++) costCache[i]=0;
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k;
        if(pr&(K_L|K_R)){ opPage=(opPage+((pr&K_R)?1:NOPG-1))%NOPG; dirty=1; if(opPage==0){ int c=costCache[costKey()]; if(c){ sCost=c; remeasure=0; } else remeasure=1; } }
        const OptPage*pg=&optPages[opPage]; u8*sel=&opSel[opPage];
        if(pr&K_DOWN){ *sel=(u8)((*sel+1)%pg->n); dirty=1; }
        if(pr&K_UP){ *sel=(u8)((*sel+pg->n-1)%pg->n); dirty=1; }
        const OptRow*r=&pg->r[*sel];
        if(r->kind==OR_ACT){ if(pr&K_A){ optAction(r->idx,&remeasure); prev=keyNow(); dirty=1; } }
        else {
            int d=((pr&K_RIGHT)?1:0)-((pr&K_LEFT)?1:0); if(pr&K_A) d=1;
            if(d) rowChange(r,d); else if(pr&K_SEL) { rowReset(r); d=1; }
            if(d){ dirty=1; idle=0;
                if(rowCostKey(r)){ int c=costCache[costKey()]; if(c){ sCost=c; remeasure=0; } else remeasure=1; } }
        }
        if(pr&(K_B|K_START)){ settingsSave(); R_TM2CNT=0; return; }
        if(dirty){ drawOptions(); present(); dirty=0; idle=0; }
        else { vsync(); if(remeasure&&opPage==0&&++idle>=20){ sCost=measureDraw(); costCache[costKey()]=(s16)sCost; remeasure=0; dirty=1; } }
    }
}
static const char* const optHelp[12]={">OPTIONS",">PAGES  L AND R","VIDEO  SPEED AND LOOKS   PLAY  THE LIFE SIM","AGES  HOW LONG EACH LIFE STAGE LASTS",">ROWS","UP DOWN PICK  LEFT RIGHT CHANGE","SELECT PUTS A ROW BACK TO NORMAL","A GOLD DOT MARKS A CHANGED ROW",">SAFE TO TRY","RESET ALL OPTIONS IS ON THE DATA PAGE","B OR START GOES BACK AND SAVES",""};
