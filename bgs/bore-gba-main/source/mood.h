// mood.h - BORE mood meters. No art, no allocation, integer only. Two meters, both 0..100:
//   FUN    fast and fickle. Tricks, grinds, air time and ramps fill it; it drains on its own (boredom, and the longer nothing
//          fun happens the faster it drains). Fun is what the game is about.
//   HAPPY  slow, "how life is going". It chases a target made from comfort (fed, bladder ok) and fun, falls faster than it
//          rises, and takes instant knocks from pain and accidents.
// Include AFTER the life globals (lfood, lbl, lgrind, lairF, lskate, lsp, lnote, lnoteT) and BEFORE hurt().
//
// HOW TO ADD A MECHANIC
//  1. one-off thing (landed a trick, ate, got hurt): add a name to MoodEv, a row to moodTab (fun, happy change in meter
//     points, same order), then call moodEvent(M_X) where it happens. moodEventN(M_X,n) multiplies the row by n (clamped to 8).
//  2. something that goes on every step (grinding, hunger, standing near a thing you like): add it to moodTick().
//  3. something the mood should CHANGE (speed, score, what you can do): put it in an effect function at the bottom
//     (moodTop, moodPts) and call that where the game uses the number. Use moodState() for anything that needs "how does
//     the skater feel": faces, sounds, animations, dialogue.
// Meters are kept x256 internally so slow drifts work at 60 steps/s. Not saved to SRAM yet (see moodReset).
//
// MOOD (tune here only)
#define MOOD_ONE        256   // 1 meter point
#define MOOD_FUN_START  50
#define MOOD_HAP_START  60
#define MOOD_FUN_DECAY  4     // fun lost per step (1/256 pt): 4 = 1 pt per second, 100 to 0 in ~100 s of nothing
#define MOOD_BORED_AFTER 900  // steps (15 s) without anything fun before boredom doubles the drain
#define MOOD_CRUISE     3     // fun regained per step while skating fast (cancels most of the drain, fills nothing)
#define MOOD_GRIND      12    // fun per step while grinding (~2.8 pt/s)
#define MOOD_AIRTIME    20    // fun per step while airborne (~4.7 pt/s), only after MOOD_AIR_MIN steps in the air
#define MOOD_AIR_MIN    10
#define MOOD_HAP_UP     3     // happy rises towards its target this fast per step (1 pt per 1.4 s)
#define MOOD_HAP_DOWN   6     // ...and falls twice as fast
#define MOOD_W_COMFORT  60    // % of the happy target that comes from needs (the rest from fun)
#define MOOD_SAD        25    // happy below this = SAD (slower top speed)
#define MOOD_BORED      20    // fun below this = BORED (fewer points)
#define MOOD_STOKED     80    // fun at/above this (and happy >= 60) = STOKED (more points, a touch faster)

enum { M_TRICK, M_COMBO, M_GRIND_ON, M_LAUNCH, M_GOT_BOARD, M_EAT, M_RELIEVE, M_SLEEP, M_SHOWER, M_SOFA, M_WANT, M_SKILL, M_PAY, M_PROMO,      // good
       M_BAIL, M_HURT, M_HURT_BIG, M_BUMP, M_ACCIDENT, M_FAINT, M_DIE, M_FEAR, M_PASSOUT, M_BROKE, M_DEMOTE, M_N };        // bad
typedef struct { signed char fun, hap; } MoodRow;
static const MoodRow moodTab[M_N]={
    { 5, 1},   // M_TRICK       landed a clean trick (spin / flip / grab)
    { 8, 2},   // M_COMBO       a chain banked (moodEventN with the trick count)
    { 2, 0},   // M_GRIND_ON    locked onto a rail / ledge / bench
    { 3, 0},   // M_LAUNCH      popped off a ramp lip
    {10, 6},   // M_GOT_BOARD   found the skateboard
    { 2,10},   // M_EAT         ate at the fridge
    { 1, 8},   // M_RELIEVE     used the toilet
    { 0, 8},   // M_SLEEP       got up from bed
    { 2, 8},   // M_SHOWER      finished a shower
    { 3, 5},   // M_SOFA        got up from the sofa
    { 4, 8},   // M_WANT        met a want (sims.h)
    { 6, 6},   // M_SKILL       skill level up (sims.h)
    { 4, 8},   // M_PAY         shift paid (sims.h)
    { 8,15},   // M_PROMO       promoted (sims.h)
    {-6,-4},   // M_BAIL        bad landing
    {-4,-6},   // M_HURT        hurt badly enough to groan (OW)
    {-6,-12},  // M_HURT_BIG    close call
    {-2,-1},   // M_BUMP        ran into a wall
    {-8,-15},  // M_ACCIDENT    bladder let go
    {-6,-12},  // M_FAINT       fainted from hunger
    {-10,-25}, // M_DIE
    {-3,-7},   // M_FEAR        a fear came true (sims.h)
    {-6,-12},  // M_PASSOUT     fell asleep on their feet (sims.h)
    {-4,-10},  // M_BROKE       bills could not be paid (sims.h)
    {-6,-15},  // M_DEMOTE      demoted (sims.h)
};
enum { MS_SAD, MS_BORED, MS_OK, MS_HAPPY, MS_STOKED };
static int moodFun, moodHap, moodIdle, moodAir, moodSt;   // meters x256, steps since anything fun, steps airborne, last announced state
static const char* const moodStName[5]={"SAD","BORED","OK","HAPPY","STOKED"};
static int simsComfort(void); static int simsTop(int top); static void simsMood(int ev,int n); static int simsPts(int pts);   // sims.h (included after this file)
static inline int moodClamp(int v){ return v<0?0:v>100*MOOD_ONE?100*MOOD_ONE:v; }
static inline int moodFunPct(void){ return moodFun/MOOD_ONE; }
static inline int moodHapPct(void){ return moodHap/MOOD_ONE; }
static int moodState(void){
    int f=moodFunPct(), h=moodHapPct();
    if(h<MOOD_SAD) return MS_SAD;
    if(f<MOOD_BORED) return MS_BORED;
    if(f>=MOOD_STOKED&&h>=60) return MS_STOKED;
    return h>=60?MS_HAPPY:MS_OK;
}
static void moodReset(void){ moodFun=MOOD_FUN_START*MOOD_ONE; moodHap=MOOD_HAP_START*MOOD_ONE; moodIdle=moodAir=0; moodSt=moodState(); }
static void moodEventN(int ev,int n){
    if(n<1) n=1;
    if(n>8) n=8;
    moodFun=moodClamp(moodFun+moodTab[ev].fun*MOOD_ONE*n); moodHap=moodClamp(moodHap+moodTab[ev].hap*MOOD_ONE*n);
    if(moodTab[ev].fun>0) moodIdle=0;   // something fun happened: boredom starts over
    simsMood(ev,n);                     // sims.h: tell the wants and fears about it
}
static inline void moodEvent(int ev){ moodEventN(ev,1); }
static void moodTick(void){   // once per logic step while alive
    moodIdle++;
    int dec=MOOD_FUN_DECAY*(moodIdle>MOOD_BORED_AFTER?2:1);
    if(lskate&&lsp>=12) dec-=MOOD_CRUISE;                              // cruising: boredom creeps instead of running
    if(lgrind){ dec-=MOOD_GRIND; moodIdle=0; }
    if(lairF){ if(++moodAir>MOOD_AIR_MIN){ dec-=MOOD_AIRTIME; moodIdle=0; } } else moodAir=0;
    moodFun=moodClamp(moodFun-dec);
    int comfort=lfood<100-lbl?lfood:100-lbl;                            // 0..100: worst of hunger and bladder
    { int sc=simsComfort()+20; if(sc>100) sc=100; if(sc<comfort) comfort=sc; }   // ...and the sims.h needs (energy, hygiene, comfort), with some slack
    int target=(comfort*MOOD_W_COMFORT+moodFunPct()*(100-MOOD_W_COMFORT))/100;
    int t=target*MOOD_ONE;
    if(moodHap<t){ moodHap+=MOOD_HAP_UP; if(moodHap>t) moodHap=t; } else if(moodHap>t){ moodHap-=MOOD_HAP_DOWN; if(moodHap<t) moodHap=t; }
    moodHap=moodClamp(moodHap);
    int st=moodState();
    if(st!=moodSt){   // say it out loud when the skater slips into (or reaches) an extreme
        if((st==MS_SAD||st==MS_BORED||st==MS_STOKED)&&lnoteT<=0){ lnote=moodStName[st]; lnoteT=60; }
        moodSt=st;
    }
}
// ---- effects: what the mood does to the game ----
static int moodTop(int top){    // top speed: SAD drags 15%, STOKED adds 6%, and being worn out (sims.h) slows you down
    int s=moodState(); if(xo[XO_MOODFX]) top=s==MS_SAD?top-top*15/100: s==MS_STOKED?top+top*6/100: top;   // MOOD EFFECTS option
    return simsTop(top)*oSpeedPct()/100;   // TOP SPEED option
}
static int moodPts(int pts){    // trick points: STOKED +25%, BORED -25%
    int s=moodState(); if(xo[XO_MOODFX]) pts=s==MS_STOKED?pts+pts/4: s==MS_BORED?pts-pts/4: pts;
    pts=simsPts(pts)*oScorePct()/100; return pts;   // SCORE option   // then the skill bonus (sims.h)
}
