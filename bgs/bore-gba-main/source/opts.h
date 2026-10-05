// opts.h - the extended options. Every option here is one small number in xo[], so adding one is cheap:
//
//   1. add a name to the XO_ enum (ALWAYS at the end: saves are stored by position),
//   2. add its choice count to xoCnt[] and its default to xoDef[] (same position),
//   3. read xo[XO_X] (or a helper below) where the game uses it,
//   4. add a row for it in the page tables in optscreen.h (XR(...) line plus a label list).
//
// The values are saved to SRAM by optsSave() (main.c) next to the old settings block, and loaded with a range check on
// every value, so an older save (fewer options) just gets the defaults for the new ones.
// Needs before it: u8 / u16 (stdint) and RGB().
enum {
    // PLAY
    XO_NEED,      // how fast REST / CLEAN / COMFY drain: OFF SLOW NORMAL FAST BRUTAL
    XO_DAY,       // length of a game day: 3 / 6 / 12 / 24 real minutes, or the clock stopped
    XO_JOB,       // career on / off (off = no shifts, no quota, no bills)
    XO_BILLS,     // bills taken at midnight: NONE HALF NORMAL DOUBLE
    XO_QUOTA,     // trick-point quota of a shift: EASY NORMAL HARD INSANE
    XO_SCORE,     // trick points multiplier: X0.5 X1 X2 X3
    XO_COMBO,     // how long a combo chain waits for the next trick
    XO_MOODFX,    // SAD / STOKED / BORED change speed and points
    XO_HURT,      // NORMAL, GENTLE (half damage), NO DEATH (a bad fall can never kill)
    XO_HUNGER,    // FOOD and WC speed: OFF SLOW NORMAL FAST
    XO_WANTS,     // wants and fears shown in the HUD
    XO_BUBBLE,    // thought bubble: OFF, URGENT needs only, ALL
    XO_AUTOSAVE,  // the life saves itself (midnight, payday, pause menu, leaving)
    XO_SPEED,     // top speed 80 / 100 / 125 / 150 %
    // INPUT
    XO_BTN,       // NORMAL, A/B swapped, L/R swapped, both
    XO_REPEAT,    // D-pad auto-repeat of the editor cursor: SLOW NORMAL FAST
    // AUDIO
    XO_MUS,       // music volume: FULL HALF QUARTER OFF
    XO_SFX,       // sound effect volume: FULL HALF QUARTER
    XO_TITLEMUS,  // music on the title screen
    // HUD
    XO_CLOCK,     // 24 HOUR, 12 HOUR, HIDDEN
    XO_ACCENT,    // accent colour of menus and the HUD
    XO_TOAST,     // how long messages stay up: SHORT NORMAL LONG
    // ROOMS (editor and slots)
    XO_MINI,      // minimap in the editor
    XO_EDSAVE,    // leaving the editor saves the room
    XO_RESETASK,  // ask before RESET MAP
    XO_SLOTSYNC,  // SAVE MAP also writes into the active slot
    XO_SLOTCONF,  // ask before overwriting, loading over or deleting a slot
    XO_SLOTCONT,  // what a slot saves: ROOM, ROOM + PERSON, ALL (room, person, life)
    XO_SLOTBOOT,  // at power on, the person of the active slot is loaded (the creature is not kept anywhere else)
    XO_AGING,     // how fast the life stages pass: OFF SLOW NORMAL FAST (slow doubles the days of every stage, fast halves them)
    XO_AGEB,      // OPTIONS > AGES: days as a BABY  (see oStageDays)
    XO_AGEC,      // days as a CHILD
    XO_AGET,      // days as a TEEN
    XO_AGEA,      // days as an ADULT (the last choice is FOREVER: never grows old)
    XO_GAMEMUS,   // AUDIO: jukebox songs in their shuffled order while you play (off / on)
    XO_N
};
static const u8 xoCnt[]={ 5,5,2,4,4,4,4,2,3,4,2,3,2,4,   4,3,   4,3,2,   3,6,3,   2,2,2,2,2,3,2, 4,10,10,10,11, 2 };
static const u8 xoDef[]={ 2,1,1,2,1,1,1,1,0,2,1,2,1,1,   0,1,   0,0,1,   0,0,1,   1,1,1,0,1,2,1, 2,1,2,2,4, 0 };
_Static_assert(sizeof(xoCnt)==XO_N&&sizeof(xoDef)==XO_N,"xoCnt / xoDef must have one entry per XO_ name");
static u8 xo[XO_N];
static void optsDefaults(void){ for(int i=0;i<XO_N;i++) xo[i]=xoDef[i]; }

// ---- accent colour (the GOLD of menus, HUD numbers and the editor) ----
#define NACC 6
static const u16 accentTab[NACC]={ RGB(31,26,6), RGB(12,28,16), RGB(14,24,31), RGB(31,16,24), RGB(31,18,4), RGB(24,18,31) };
static const char* const accentNm[NACC]={"GOLD","MINT","SKY","PINK","ORANGE","LILAC"};

// ---- helpers: the number each choice stands for ----
static inline int oNeedPct(void){ static const u8 t[5]={0,50,100,150,200}; return t[xo[XO_NEED]]; }
static inline int oStepsMin(void){ static const u8 t[5]={7,15,30,60,0}; return t[xo[XO_DAY]]; }   // logic steps per game minute, 0 = stopped
static inline int oStageDays(int st){   // game days a stage lasts before the next one (0 = for ever); ELDER is the last stage and has no row
    static const u8 t[10]={1,2,3,5,7,10,14,21,30,60};
    if(st<0||st>3) return 0; int i=xo[XO_AGEB+st]; return i<10?t[i]:0; }
static inline int oBillsPct(void){ static const u8 t[4]={0,50,100,200}; return t[xo[XO_BILLS]]; }
static inline int oQuotaPct(void){ static const u8 t[4]={60,100,150,200}; return t[xo[XO_QUOTA]]; }
static inline int oScorePct(void){ static const u16 t[4]={50,100,200,300}; return t[xo[XO_SCORE]]; }
static inline int oComboLen(void){ static const u16 t[4]={90,150,240,360}; return t[xo[XO_COMBO]]; }   // steps (60 per second)
static inline int oSpeedPct(void){ static const u8 t[4]={80,100,125,150}; return t[xo[XO_SPEED]]; }
static inline int oFoodEvery(void){ static const u16 t[4]={0,240,120,60}; return t[xo[XO_HUNGER]]; }  // steps per FOOD point, 0 = never
static inline int oWcEvery(void){ static const u16 t[4]={0,200,100,50}; return t[xo[XO_HUNGER]]; }
static inline int oToastLen(void){ static const u8 t[3]={25,45,90}; return t[xo[XO_TOAST]]; }
static inline int oRepDelay(void){ static const u8 t[3]={22,14,8}; return t[xo[XO_REPEAT]]; }       // frames held before the cursor repeats
static inline int oRepMask(void){ static const u8 t[3]={7,3,1}; return t[xo[XO_REPEAT]]; }          // repeats when (held & mask) == 0
static inline int oMusShift(void){ static const u8 t[4]={0,1,2,8}; return t[xo[XO_MUS]]; }          // 8 = silent
static inline int oSfxShift(void){ return xo[XO_SFX]; }                                               // 0 full, 1 half, 2 quarter
#define GOLD (accentTab[xo[XO_ACCENT]])
