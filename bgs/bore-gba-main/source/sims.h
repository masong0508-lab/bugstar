// sims.h - BORE life-sim layer, in the spirit of The Sims 2 on a handheld. Integer only, no allocation, no art files.
// Include AFTER mood.h and the life globals (lfood, lbl, lstun, lsp, lgrind, lnote, lnoteT, lnear, lscore, lifeMap) and BEFORE lifeStep.
//
//  NEEDS     REST (energy), CLEAN (hygiene), COMFY (comfort) and ROOM (the look of the place you stand in) next to FOOD and WC (main.c).
//            0..100, they drain by themselves. BED (S) sleeps, SHOWER (H) washes, SOFA (C) sits: stand next to it and press R, A/B/R gets you up.
//            ROOM rises when you stand among furniture (fridge, toilet, bed, shower, sofa) and sags in an empty place.
//            Everything feeds the HAPPY meter (mood.h -> simsComfort()).
//  CLOCK     60 steps = 4 game minutes... see SIM_STEPS_MIN. A week is MON..SUN, the game starts MON 08:00. Sleeping runs the clock fast.
//  CAREER    Pro skater, Mon-Fri 09:00-17:00. Trick points you score during the shift count towards the day's QUOTA. At 17:00 you are paid:
//            full quota = full pay (and a step towards promotion), half quota = half pay, less = nothing and a strike. 3 good days = promotion,
//            3 strikes = demotion. Bills are taken at midnight; if you cannot pay, that is a fear coming true.
//  SKILL     SKATING skill 0..5, trained by tricks, combos and grinds; each level adds 8% to trick points.
//  WANTS     two wants and one fear float next to the needs. Meeting a want pays aspiration points and a mood lift; a fear costs points.
//            A want is only offered if the room has what it needs (no bed, no nap want).
//  ASPIRATION  points -> BRONZE / SILVER / GOLD / PLATINUM. Each level slows the needs down (platinum: half speed).
//  THOUGHT BUBBLE  over the head: an urgent need if there is one, otherwise a want.
//  SAVING    needs, cash, aspiration, clock, job, skill are kept in SRAM (SIM_OFF). See simsSave() / simsLoad().
//
// HOW TO ADD A WANT: add a SE_ name, a row in simWants (name, event, aspiration points, required furniture mask), and call
//   simEvent(SE_X) where it happens (or map a mood event to it in simsMood). A FEAR is the same in simFears.
// HOW TO ADD A NEED: a variable (saved in simsSave/simsLoad), a rate in simsTick, a use in simBegin, a bar in simsHud.
//
// TUNING
#define SIM_STEPS_MIN  15     // logic steps per game minute (60/s): 15 = a game day is 6 real minutes
#define SIM_RATE_NRG   7      // energy lost per step, 1/1024 pt (7 = 1 pt per 2.4 s, 100 to 0 in ~4 minutes)
#define SIM_RATE_HYG   5
#define SIM_RATE_COM   6
#define SIM_GAIN_SLEEP 175    // energy gained per step while sleeping, 1/1024 pt (~10 s for a full night)
#define SIM_GAIN_WASH  400    // hygiene per step in the shower (~4 s)
#define SIM_GAIN_SIT   300    // comfort per step on the sofa (~5.5 s)
#define SIM_LOW        20     // a need below this shows in the thought bubble
#define SIM_SLEEPY_TOP 20     // % of top speed lost when ENERGY is below SIM_LOW
#define SIM_WANT_GAP   240    // steps before an empty want slot is refilled
#define SIM_NIGHT_FROM 1320   // 22:00 ... 06:00: awake costs energy 50% faster, sleep restores 25% faster
#define SIM_NIGHT_TO   360
#define SIM_WORK_FROM  540    // 09:00
#define SIM_WORK_TO    1020   // 17:00
#define SIM_QUOTA0     600    // trick points needed per shift at job level 0 ...
#define SIM_QUOTA_LVL  500    // ... plus this per level
#define SIM_PAY0       70     // pay for a full shift at level 0 ...
#define SIM_PAY_LVL    40     // ... plus this per level (+30 when you score double the quota)
#define SIM_BILLS      40     // taken every midnight
#define SIM_CASH0      200    // starting cash
#define SIM_ROOM_R     5      // ROOM looks this many tiles around the skater
static const short simLvlAt[4]={40,120,260,450};          // aspiration points for BRONZE, SILVER, GOLD, PLATINUM
static const char* const simLvlNm[5]={"NONE","BRONZE","SILVER","GOLD","PLATINUM"};
static const unsigned char simDecayPct[5]={100,90,80,70,50}; // need decay at each aspiration level
static const char* const simJobNm[6]={"NEWBIE","AMATEUR","SPONSORED","PRO","TEAM RIDER","LEGEND"};
static const short simSkillAt[5]={12,35,70,120,200};      // skill points for skill levels 1..5
static const char* const simDayNm[7]={"MON","TUE","WED","THU","FRI","SAT","SUN"};

// furniture the room has (simsScan) and what a want needs
enum { SR_FRIDGE=1, SR_TOILET=2, SR_BED=4, SR_SHOWER=8, SR_SOFA=16, SR_RAIL=32, SR_RAMP=64 };
// things that happen (wants and fears are both made of these)
enum { SE_EAT, SE_PEE, SE_SLEEP, SE_SHOWER, SE_SOFA, SE_TRICK, SE_COMBO, SE_GRIND, SE_AIR, SE_STOKED,
       SE_SHIFT, SE_PROMO, SE_SKILL, SE_ROOM,
       SE_BAIL, SE_HURT, SE_ACCIDENT, SE_FAINT, SE_PASSOUT, SE_BROKE, SE_DEMOTE, SE_N };
typedef struct { const char* name; unsigned char ev, pts, req; } SimWish;
static const SimWish simWants[]={
    {"HAVE A SNACK",  SE_EAT,   10, SR_FRIDGE},
    {"USE THE WC",    SE_PEE,    8, SR_TOILET},
    {"TAKE A NAP",    SE_SLEEP, 15, SR_BED},
    {"GET CLEAN",     SE_SHOWER,12, SR_SHOWER},
    {"SIT ON SOFA",   SE_SOFA,  10, SR_SOFA},
    {"LAND A TRICK",  SE_TRICK, 10, 0},
    {"TRICK COMBO",   SE_COMBO, 25, 0},
    {"GRIND A RAIL",  SE_GRIND, 15, SR_RAIL},
    {"GET AIR",       SE_AIR,   15, SR_RAMP},
    {"FEEL STOKED",   SE_STOKED,20, 0},
    {"FINISH A SHIFT",SE_SHIFT, 20, 0},
    {"GET PROMOTED",  SE_PROMO, 35, 0},
    {"LEARN A SKILL", SE_SKILL, 25, 0},
    {"NICE ROOM",     SE_ROOM,  15, SR_BED|SR_SOFA},
};
static const SimWish simFears[]={
    {"BAILING",       SE_BAIL,     10, 0},
    {"AN ACCIDENT",   SE_ACCIDENT, 15, 0},
    {"PASSING OUT",   SE_PASSOUT,  15, 0},
    {"GETTING HURT",  SE_HURT,     10, 0},
    {"FAINTING",      SE_FAINT,    15, 0},
    {"BEING BROKE",   SE_BROKE,    15, 0},
    {"DEMOTION",      SE_DEMOTE,   20, 0},
};
#define SIM_NW ((int)(sizeof(simWants)/sizeof(simWants[0])))
#define SIM_NF ((int)(sizeof(simFears)/sizeof(simFears[0])))

// ---- state ----
static int sNrg, sHyg, sCom, sRoom;          // needs, 0..100
static int simCrN, simCrH, simCrC;           // fractional need changes, 1/1024 pt
static int simAct, simActT;                  // activity: 0 none, 1 sleep, 2 wash, 3 sit; steps left
static int simAsp, simLvl, simDone;          // aspiration points, level 0..4, wants met
static int simW[2], simF;                    // slot contents (index into the tables, -1 empty)
static int simSlotT[3];                      // refill countdown per slot (w0, w1, fear)
static int simHave, simPrevStoked;           // SR_ mask of furniture in the room, was STOKED last step
static unsigned simRng=12345u;
static const char* simQ; static int simQT;   // a note waiting for the note line to be free
static int simT;                             // steps since reset (drives the bubble)
static int simMoney, simDay, simMin, simClkCr;   // cash, days since the start (0 = MON), minute of day, step counter towards a minute
static int jobLvl, jobGood, jobBad, shiftPts, simLastScore, simNiceRoom;   // job level 0..5, good days towards promotion, strikes, points this shift
static int skillPts, skillLvl;               // SKATING skill
static char simClk[16], simMsg[20];          // formatted clock text, note buffer

static int simRnd(void){ simRng=simRng*1664525u+1013904223u; return (int)(simRng>>24); }
static void simQueue(const char* s){ simQ=s; simQT=240; }

static void simsScan(void){   // what does this map have?
    simHave=0;
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){ char c=lifeMap[y][x];
        if(c=='F') simHave|=SR_FRIDGE; else if(c=='T') simHave|=SR_TOILET; else if(c=='S') simHave|=SR_BED;
        else if(c=='H') simHave|=SR_SHOWER; else if(c=='C') simHave|=SR_SOFA;
        else if(c=='='||c=='L'||c=='N'||c=='Z'||c=='K'||c=='J') simHave|=SR_RAIL; else if((c>='1'&&c<='<')) simHave|=SR_RAMP; }
}
static int simPick(const SimWish* tab,int n,int avoid1,int avoid2){   // a random wish the room can satisfy, not already on show
    for(int t=0;t<40;t++){ int i=simRnd()%n; if(i==avoid1||i==avoid2) continue; if((tab[i].req&simHave)!=tab[i].req) continue; return i; }
    return -1;
}
static void simFill(void){
    if(simW[0]<0&&simSlotT[0]<=0) simW[0]=simPick(simWants,SIM_NW,simW[1],-1);
    if(simW[1]<0&&simSlotT[1]<=0) simW[1]=simPick(simWants,SIM_NW,simW[0],-1);
    if(simF<0&&simSlotT[2]<=0)    simF=simPick(simFears,SIM_NF,-1,-1);
}

// ---- saving (SRAM at SIM_OFF; main.c's map is 0..4802, settings 8192, jukebox 12288) ----
#ifndef SIM_SRAM
#define SIM_OFF 16384
#define SIM_SRAM ((volatile unsigned char*)0x0E000000+SIM_OFF)
#endif
static void simPut16(volatile unsigned char*m,int i,int v){ m[i]=(unsigned char)(v&255); m[i+1]=(unsigned char)((v>>8)&255); }
static int  simGet16(volatile unsigned char*m,int i){ return m[i]|(m[i+1]<<8); }
#define SIM_BLOCK 24   // bytes of one saved life (also stored inside a room slot, see slots.h)
static void simsPack(volatile unsigned char*m){   // write the life into any 24 byte buffer
    unsigned sum=0x5A;
    m[0]='S'; m[1]='I'; m[2]='M'; m[3]='2';
    m[4]=(unsigned char)sNrg; m[5]=(unsigned char)sHyg; m[6]=(unsigned char)sCom; m[7]=(unsigned char)sRoom;
    simPut16(m,8,simMoney); simPut16(m,10,simAsp); simPut16(m,12,simDone); simPut16(m,14,simDay); simPut16(m,16,simMin);
    m[18]=(unsigned char)jobLvl; m[19]=(unsigned char)jobGood; m[20]=(unsigned char)jobBad; simPut16(m,21,skillPts);
    for(int i=4;i<=22;i++) sum+=m[i];
    m[23]=(unsigned char)sum;
}
static void simsSaveNow(void){ simsPack(SIM_SRAM); }
static void simsSave(void){ if(xo[XO_AUTOSAVE]) simsSaveNow(); }   // AUTO SAVE LIFE option: off = only slots / SAVE LIFE NOW write it
static int simsCheck(volatile unsigned char*m){   // 1 = the 24 byte buffer holds a valid life (nothing is changed)
    unsigned sum=0x5A;
    if(m[0]!='S'||m[1]!='I'||m[2]!='M'||m[3]!='2') return 0;
    for(int i=4;i<=22;i++) sum+=m[i];
    if(m[23]!=(unsigned char)sum) return 0;
    if(m[4]>100||m[5]>100||m[6]>100||m[7]>100||m[18]>5||simGet16(m,16)>=1440) return 0;
    return 1;
}
static int simsUnpack(volatile unsigned char*m){   // 1 = a valid life was read from the 24 byte buffer
    if(!simsCheck(m)) return 0;
    sNrg=m[4]; sHyg=m[5]; sCom=m[6]; sRoom=m[7];
    simMoney=simGet16(m,8); simAsp=simGet16(m,10); simDone=simGet16(m,12); simDay=simGet16(m,14); simMin=simGet16(m,16);
    jobLvl=m[18]; jobGood=m[19]; jobBad=m[20]; skillPts=simGet16(m,21);
    return 1;
}
static int simsLoad(void){ return simsUnpack(SIM_SRAM); }   // 1 = loaded a valid save
static void simLvlCalc(void){ int l=0; for(int i=0;i<4;i++) if(simAsp>=simLvlAt[i]) l=i+1; simLvl=l; }
static void simSkillCalc(void){ int l=0; for(int i=0;i<5;i++) if(skillPts>=simSkillAt[i]) l=i+1; skillLvl=l; }

static void simsDefaults(void){   // a brand new life (nothing is written to SRAM)
    sNrg=100; sHyg=100; sCom=80; sRoom=40; simMoney=SIM_CASH0; simAsp=0; simDone=0; simDay=0; simMin=480;
    jobLvl=0; jobGood=0; jobBad=0; skillPts=0;
}
static void simsTransient(void){
    simCrN=simCrH=simCrC=0; simAct=simActT=0; simW[0]=simW[1]=simF=-1; simSlotT[0]=simSlotT[1]=simSlotT[2]=0;
    simPrevStoked=0; simQ=0; simQT=0; simT=0; simClkCr=0; shiftPts=0; simLastScore=lscore; simNiceRoom=0;
    simLvlCalc(); simSkillCalc(); simsScan(); simFill();
}
static void simsReset(void){   // entering the life game: pick up the saved life if there is one
    simsDefaults(); simsLoad(); simsTransient();
}
static void simsRespawn(void){   // after dying: the needs come back, the life (cash, job, skill, aspiration, clock) goes on
    sNrg=60; sHyg=60; sCom=60; simAct=simActT=0; simPrevStoked=0; simQ=0; shiftPts=0; simLastScore=lscore;
}
static void simsNewLife(void){   // pause menu: NEW LIFE
    simsDefaults(); simsTransient(); simsSaveNow();
}

// ---- wants, fears, aspiration ----
static void simSetLvl(void){
    int old=simLvl; simLvlCalc();
    if(simLvl>old){ simQueue("ASPIRATION UP"); moodEvent(M_WANT); }
}
static void simEvent(int ev);
// mood.h calls this from every moodEvent: the game events that wants, fears and skill care about
static void simSkillAdd(int n){
    int old=skillLvl; skillPts+=n; simSkillCalc();
    if(skillLvl>old){ simQueue("SKILL UP"); moodEvent(M_SKILL); simEvent(SE_SKILL); }
}
static void simsMood(int ev,int n){
    switch(ev){
        case M_TRICK: simEvent(SE_TRICK); simSkillAdd(1); break;
        case M_COMBO: if(n>=2) simEvent(SE_COMBO); simSkillAdd(1); break;   // n = tricks - 1
        case M_GRIND_ON: simEvent(SE_GRIND); simSkillAdd(1); break;    case M_LAUNCH: simEvent(SE_AIR); break;
        case M_EAT: simEvent(SE_EAT); break;           case M_RELIEVE: simEvent(SE_PEE); break;
        case M_BAIL: simEvent(SE_BAIL); break;         case M_HURT: case M_HURT_BIG: simEvent(SE_HURT); break;
        case M_ACCIDENT: simEvent(SE_ACCIDENT); break; case M_FAINT: simEvent(SE_FAINT); break;
        case M_PASSOUT: simEvent(SE_PASSOUT); break;
        default: break;   // M_SLEEP/M_SHOWER/M_SOFA/M_WANT/M_FEAR/M_SKILL/M_PAY/M_PROMO/M_BROKE/M_DEMOTE are raised by sims.h itself
    }
}
// something happened: pay a want, or let a fear come true
static void simEvent(int ev){
    for(int s=0;s<2;s++) if(simW[s]>=0&&simWants[simW[s]].ev==ev){
        simAsp+=simWants[simW[s]].pts; simDone++; simW[s]=-1; simSlotT[s]=SIM_WANT_GAP; moodEvent(M_WANT); simQueue("WANT MET"); simSetLvl(); }
    if(simF>=0&&simFears[simF].ev==ev){
        simAsp-=simFears[simF].pts; if(simAsp<0) simAsp=0; simF=-1; simSlotT[2]=SIM_WANT_GAP*2; moodEvent(M_FEAR); simQueue("FEAR CAME TRUE"); simLvlCalc(); }
}
static inline int simMin3(int a,int b,int c){ return a<b?(a<c?a:c):(b<c?b:c); }
static int simsWorst(void){ return simMin3(sNrg,sHyg,sCom); }                 // lowest of the three activity needs
static int simsComfort(void){ return (sNrg*30+sHyg*25+sCom*25+sRoom*20)/100; } // blended, used by the HAPPY target in mood.h
static int simsTop(int top){ if(sNrg<SIM_LOW) top-=top*SIM_SLEEPY_TOP/100; return top*stSpd[stage]/100; }   // too tired: slower
static int simsPts(int pts){ return pts+pts*skillLvl*8/100; }                  // SKATING skill: +8% trick points per level
static const char* simsAlert(void){   // most urgent need, or 0
    if(lbl>80) return "WC";
    if(lfood<SIM_LOW) return "EAT";
    if(sNrg<SIM_LOW) return "ZZZ";
    if(sHyg<SIM_LOW) return "STINKY";
    if(sCom<SIM_LOW) return "SIT";
    return 0;
}

// ---- clock and career ----
static int simWorkday(void){ return (simDay%7)<5; }
static int simInShift(void){ return ojob()&&simWorkday()&&simMin>=SIM_WORK_FROM&&simMin<SIM_WORK_TO; }
static int simQuota(void){ return (SIM_QUOTA0+SIM_QUOTA_LVL*jobLvl)*oQuotaPct()/100; }
static int simIsNight(void){ return simMin>=SIM_NIGHT_FROM||simMin<SIM_NIGHT_TO; }
static void simMsgPay(const char* pre,int n){   // "PAID 110" into simMsg
    int i=0; for(;pre[i]&&i<10;i++) simMsg[i]=pre[i];
    char d[8]; int k=0; if(n==0) d[k++]='0'; while(n>0&&k<7){ d[k++]=(char)('0'+n%10); n/=10; }
    while(k>0) simMsg[i++]=d[--k]; simMsg[i]=0;
}
static void simShiftEnd(void){   // 17:00 on a workday
    int q=simQuota(), p=shiftPts, pay=0;
    if(p>=q){ pay=SIM_PAY0+SIM_PAY_LVL*jobLvl+(p>=2*q?30:0); jobBad=0;
        if(++jobGood>=3){ jobGood=0; if(jobLvl<5){ jobLvl++; moodEvent(M_PROMO); simEvent(SE_PROMO); simQueue("PROMOTED"); } } }
    else if(p>=q/2){ pay=(SIM_PAY0+SIM_PAY_LVL*jobLvl)/2; }
    else { if(++jobBad>=3){ jobBad=0; jobGood=0; if(jobLvl>0){ jobLvl--; moodEvent(M_DEMOTE); simEvent(SE_DEMOTE); simQueue("DEMOTED"); } } }
    if(pay>0){ simMoney+=pay; if(simMoney>9999) simMoney=9999; moodEvent(M_PAY); simMsgPay(p>=q?"SHIFT PAID ":"HALF PAY ",pay); simQueue(simMsg); }
    else if(!simQ) simQueue("NO PAY TODAY");
    if(p>=q/2) simEvent(SE_SHIFT);
    shiftPts=0; simsSave();
}
static void ageTick(void){   // once per game day: each stage lasts the days set on OPTIONS > AGES (the AGING option scales them); the life loop does the growing
    static const u8 pct[4]={0,200,100,50};
    if(stage>=AG_ELDER||!xo[XO_AGING]||!oStageDays(stage)) return;   // an elder is the last stage; OFF or FOREVER never grows
    int need=oStageDays(stage)*pct[xo[XO_AGING]]/100; if(need<1) need=1;
    if(++ageDays>=need){ gGrow=1; simQueue("BIRTHDAY"); } else ageSave();
}
static void simMinute(void){   // once per game minute
    simMin++;
    if(simMin>=1440){   // midnight: new day, bills, autosave
        simMin=0; simDay++; if(simDay>30000) simDay=0;
        ageTick();
        int bill=ojob()?SIM_BILLS*oBillsPct()/100:0;   // no career = no bills; BILLS option scales them
        if(bill>0){ if(simMoney>=bill) simMoney-=bill;
            else { simMoney=0; moodEvent(M_BROKE); simEvent(SE_BROKE); simQueue("BILLS UNPAID"); } }
        simsSave();
    }
    if(ojob()&&simMin==SIM_WORK_FROM-60&&simWorkday()&&!simQ) simQueue("WORK AT 9");
    if(ojob()&&simMin==SIM_WORK_FROM&&simWorkday()){ shiftPts=0; if(!simQ) simQueue("SHIFT STARTS"); }
    if(ojob()&&simMin==SIM_WORK_TO&&simWorkday()) simShiftEnd();
}
static const char* simsClock(void){   // "MON 14:05"
    int h=simMin/60, m=simMin%60, i=0; const char*d=simDayNm[simDay%7];
    simClk[i++]=d[0]; simClk[i++]=d[1]; simClk[i++]=d[2]; simClk[i++]=' ';
    if(xo[XO_CLOCK]==1){ int h12=h%12; if(h12==0) h12=12; if(h12>=10) simClk[i++]='1'; simClk[i++]=(char)('0'+h12%10); }   // CLOCK option: 12 HOUR
    else { simClk[i++]=(char)('0'+h/10); simClk[i++]=(char)('0'+h%10); }
    simClk[i++]=':'; simClk[i++]=(char)('0'+m/10); simClk[i++]=(char)('0'+m%10);
    if(xo[XO_CLOCK]==1) simClk[i++]=(h>=12)?'P':'A';
    simClk[i]=0;
    return simClk;
}

// ---- ROOM need: what is around the skater ----
static void simRoomTick(int tx,int ty){
    int kinds=0, items=0;
    for(int y=ty-SIM_ROOM_R;y<=ty+SIM_ROOM_R;y++)for(int x=tx-SIM_ROOM_R;x<=tx+SIM_ROOM_R;x++){
        if(x<0||y<0||x>=MW||y>=MH) continue; char c=lifeMap[y][x]; int b=0;
        if(c=='F') b=1; else if(c=='T') b=2; else if(c=='S') b=4; else if(c=='H') b=8; else if(c=='C') b=16;
        if(b){ kinds|=b; items++; } }
    int k=0; for(int b=1;b<32;b<<=1) if(kinds&b) k++;
    int target=k*16+(items>5?5:items)*4; if(target>100) target=100;
    if(target>sRoom){ sRoom+=2; if(sRoom>target) sRoom=target; }
    else if(target<sRoom&&(simT%90)<30) sRoom--;       // sags slowly
    if(sRoom<0) sRoom=0;
    if(sRoom>=80){ if(!simNiceRoom){ simNiceRoom=1; simEvent(SE_ROOM); } } else if(sRoom<70) simNiceRoom=0;
}

// ---- using furniture ----
// kind: 3 bed, 4 shower, 5 sofa (the lnear values). Returns 1 if the skater started.
static int simBegin(int kind){
    if(kind==3){ if(sNrg>=95){ lnote="NOT TIRED"; lnoteT=40; return 0; } simAct=1; simActT=900; lnote="ZZZ"; lnoteT=40; }
    else if(kind==4){ if(sHyg>=95){ lnote="ALREADY CLEAN"; lnoteT=40; return 0; } simAct=2; simActT=420; lnote="SPLASH"; lnoteT=40; }
    else if(kind==5){ if(sCom>=95){ lnote="COMFY ALREADY"; lnoteT=40; return 0; } simAct=3; simActT=480; lnote="AHH SOFA"; lnoteT=40; }
    else return 0;
    lsp=0; lgrind=0; lstun=2; return 1;
}
static void simEnd(void){
    if(simAct==1){ moodEvent(M_SLEEP); simEvent(SE_SLEEP); }
    else if(simAct==2){ moodEvent(M_SHOWER); simEvent(SE_SHOWER); }
    else if(simAct==3){ moodEvent(M_SOFA); simEvent(SE_SOFA); }
    simAct=0; simActT=0;
}
// once per logic step while alive. pr = keys pressed this step, tx,ty = the tile the skater stands on.
static void simsTick(unsigned pr,int tx,int ty){
    simT++;
    int pct=simDecayPct[simLvl]*oNeedPct()/100, night=simIsNight();
    if(simAct==0){   // needs drain only when not being refilled
        simCrN+=SIM_RATE_NRG*pct/100*(night?3:2)/2; simCrH+=SIM_RATE_HYG*pct/100; simCrC+=SIM_RATE_COM*pct/100;
        while(simCrN>=1024){ simCrN-=1024; if(sNrg>0) sNrg--; }
        while(simCrH>=1024){ simCrH-=1024; if(sHyg>0) sHyg--; }
        while(simCrC>=1024){ simCrC-=1024; if(sCom>0) sCom--; }
    } else {
        int *n=simAct==1?&sNrg:simAct==2?&sHyg:&sCom, *cr=simAct==1?&simCrN:simAct==2?&simCrH:&simCrC;
        int g=simAct==1?SIM_GAIN_SLEEP:simAct==2?SIM_GAIN_WASH:SIM_GAIN_SIT;
        if(simAct==1&&night) g=g*5/4;
        *cr+=g; while(*cr>=1024){ *cr-=1024; if(*n<100) (*n)++; }
        lstun=lstun>2?lstun:2; lsp=0; lgrind=0;       // stay put while busy
        simActT--;
        if(*n>=100||simActT<=0||(pr&(K_A|K_B|K_R))) simEnd();
    }
    // the clock: sleeping runs it one game minute per step
    int spm=oStepsMin();   // DAY LENGTH option (0 = the clock is stopped)
    if(spm>0){ simClkCr+=(simAct==1)?spm:1; while(simClkCr>=spm){ simClkCr-=spm; simMinute(); } }
    // the day's quota counts trick points scored during the shift
    if(lscore>simLastScore&&simInShift()) shiftPts+=lscore-simLastScore;
    simLastScore=lscore;
    if(simT%30==0) simRoomTick(tx,ty);
    if(sNrg==0&&simAct==0){ sNrg=25; lstun=300; lsp=0; lgrind=0; lnote="PASSED OUT"; lnoteT=90; moodEvent(M_PASSOUT); }   // like the old FAINT, from tiredness
    int st=moodState()==MS_STOKED; if(st&&!simPrevStoked) simEvent(SE_STOKED); simPrevStoked=st;
    for(int s=0;s<3;s++) if(simSlotT[s]>0) simSlotT[s]--;
    simFill();
    if(simQ){ if(lnoteT<=0){ lnote=simQ; lnoteT=60; simQ=0; } else if(--simQT<=0) simQ=0; }
}

// ---- drawing: HUD column and the bubble over the head. x,y = top left of the column ----
static void simsBar(int x,int y,const char* nm,int v,u16 dim){
    text(x,y,nm,dim,1);
    rect(x+30,y,v/2,5,v<SIM_LOW?RGB(28,8,6):RGB(14,22,10));
}
static int simsNum(int x,int y,int n,u16 c){   // number text with no dependence on main.c's numText order
    char b[8]; int k=0; if(n<=0){ b[k++]='0'; } else { char t[8]; int j=0; while(n>0&&j<7){ t[j++]=(char)('0'+n%10); n/=10; } while(j>0) b[k++]=t[--j]; } b[k]=0;
    return text(x,y,b,c,1);
}
static void simsClockDraw(int x,int y,u16 dim,u16 gold){
    if(xo[XO_CLOCK]==2) return;   // CLOCK: HIDDEN
    text(x,y,simsClock(),simInShift()?gold:dim,1);
    if(simIsNight()) text(x+54,y,"NIGHT",RGB(14,16,26),1);
}
static void simsHud(int x,int y,u16 dim,u16 gold){
    simsBar(x,y,"REST",sNrg,dim); simsBar(x,y+8,"CLEAN",sHyg,dim); simsBar(x,y+16,"COMFY",sCom,dim);
    text(x,y+24,"ROOM",dim,1); rect(x+30,y+24,sRoom/2,5,RGB(20,16,26));
    // aspiration bar: progress to the next level
    int lo=simLvl==0?0:simLvlAt[simLvl-1], hi=simLvl>=4?simLvlAt[3]:simLvlAt[simLvl];
    int w=simLvl>=4?50:(simAsp-lo)*50/(hi-lo); if(w<0) w=0; if(w>50) w=50;
    text(x,y+33,simLvlNm[simLvl],gold,1); if(stage<AG_ADULT) text(x+54,y+33,stageNm[stage],dim,1); rect(x,y+41,50,3,RGB(4,5,8)); rect(x,y+41,w,3,gold);
    // cash, job and skill, shift status
    int nx=text(x,y+46,"CASH ",dim,1); simsNum(nx,y+46,simMoney,gold);
    nx=text(x,y+54,simJobNm[jobLvl],RGB(22,24,28),1)+4; nx=text(nx,y+54,"SK",dim,1)+2; simsNum(nx,y+54,skillLvl,gold);
    if(simInShift()){ nx=text(x,y+62,"WORK ",gold,1); nx=simsNum(nx,y+62,shiftPts,gold); nx=text(nx,y+62,"/",dim,1); simsNum(nx,y+62,simQuota(),dim); }
    else text(x,y+62,!ojob()?"NO JOB":simWorkday()&&simMin<SIM_WORK_FROM?"WORK AT 9":"OFF DUTY",dim,1);
    // wants (green marker) and the fear (red marker)
    for(int s=0;s<2&&xo[XO_WANTS];s++) if(simW[s]>=0){ rect(x,y+72+s*8,3,5,RGB(10,26,10)); text(x+6,y+72+s*8,simWants[simW[s]].name,RGB(22,28,22),1); }
    if(simF>=0&&xo[XO_WANTS]){ rect(x,y+88,3,5,RGB(28,8,6)); text(x+6,y+88,simFears[simF].name,RGB(30,18,16),1); }
}
static void simsBubble(int cx,int cy,u16 ink){   // cx = centre, cy = bottom of the bubble
    const char* t=simsAlert(); u16 edge;
    if(!xo[XO_BUBBLE]) return;   // THOUGHT BUBBLE: OFF
    if(!t){ if(xo[XO_BUBBLE]<2) return; int s=(simT/240)&1; if(simW[s]<0) s^=1; if(simW[s]<0) return; t=simWants[simW[s]].name; edge=RGB(24,31,24); }
    else edge=RGB(31,22,20);
    int w=tw(t,1)+6, x=cx-w/2; if(x<1) x=1; if(x+w>239) x=239-w;
    rect(x,cy-9,w,9,edge); rect(x+1,cy-8,w-2,7,RGB(31,31,31)); px(cx-1,cy,edge); px(cx,cy,edge); px(cx,cy+1,edge);
    text(x+3,cy-7,t,ink,1);
}
