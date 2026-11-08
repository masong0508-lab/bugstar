// mine.h: the colony's WORKSHOPS and MINE (side game Phase 6, DEPTH_PLAN sections 1.2 and 7). Plain C, no game types, no floating point.
//
// A layer ON TOP of the colony (colony.h): it reads and writes the colony's stores (ore, wood) and its settlers' jobs, and keeps its own record
// (Mine) per colony, saved in the .sim file as chunk MINE. col_day_ex is NOT changed: a colony with no mine layer plays exactly as before
// (colony_test, sim.golden). The glue runs mn_day after each colony day (mn_catchup does it for a whole stretch of days).
//
// THE LADDER (nonlinear: the colony's rung mask decides, however each rung was opened)
//   WORKSHOPS rung  -> SMELTER and LOOM (level 1), and the colony builds a first smelter by itself once it has the wood
//   MINE rung       -> the shaft (needs a MINE building on the colony map), levels Z1..Z4, and the SMITHY
//   WALLS rung      -> Z5..Z8   (a guard post keeps the deep tunnels safe)
//   MARKET rung     -> Z9..Z15  (a market hall to sell what the deep gives)
//   MARKET, MUSEUM  -> every workshop may grow one more level each (levels 1..3)
//
// PRODUCTION CHAINS (a workshop makes `level (+1 when a settler of its trade works there)` units a day)
//   SMELTER (miners)             2 ore + 1 wood  -> 1 bar
//   SMITHY  (builders)           2 bars + 1 wood -> 1 tool            tools make digging faster and the roof safer; they wear out
//   LOOM    (cooks and traders)  2 wood           -> 1 cloth           cloth keeps settlers warm (health and morale) every week
//   Workshops never take the last 12 wood: houses and fields still get built.
//
// THE MINE (z-levels). Miners dig the level they work. Each level holds a seam; ore found there counts for more the deeper it lies, and from
// Z5 down they find gems. A level runs dry; the colony digs the next one down when the work level is half empty (miner-days and wood).
// CAVE-INS ARE THE RISK: every day a working level has a chance (per mille) of CREAKING, which warns you one day ahead; the next day it
// caves in unless the braces hold (more braces, better odds). A cave-in blocks the level until the rubble is cleared; a big one (deep levels)
// hurts the miners (no work for 5 days) and on NORMAL and HARSH may kill one. It never kills the last settler and never ends a colony, and
// on CALM nobody dies. You can brace a level, dig, clear rubble, pick the work level, all by hand; the mine runs by itself without you.
#ifndef MINE_H
#define MINE_H
#include "colony.h"

#define MN_ZMAX 15
#define MN_STORE_MAX 999999u
enum { MNW_SMELTER, MNW_SMITHY, MNW_LOOM, MNW_N };                 // workshops
enum { MNZ_SOLID, MNZ_OPEN, MNZ_CAVED, MNZ_DRY };                  // a level: not dug, worked, caved in, empty
enum { MNE_NONE, MNE_DUG, MNE_CREAK, MNE_CAVE, MNE_HELD, MNE_DRY, MNE_CLEARED, MNE_DIED, MNE_WS, MNE_GOAL, MNE_N };

typedef struct {
    uint32_t town;                       // the colony this belongs to (SimColony.town)
    uint8_t  wl[MNW_N];                  // workshop levels 0..3
    uint8_t  depth, work;                // levels dug (0 = no shaft yet); the level the miners work
    uint8_t  pin, digging;               // pin: you chose the work level (else the colony picks the deepest safe one); digging: the next level is being dug
    uint8_t  warn, hurt_days, clear;     // the roof creaks (a cave-in is due tomorrow); days the miners are hurt; rubble cleared so far on the work level
    uint16_t digprog;                    // miner-days put into the next level
    uint8_t  zst[MN_ZMAX + 1];           // MNZ_* per level (index 0 unused)
    uint8_t  brace[MN_ZMAX + 1];         // supports 0..9
    uint16_t seam[MN_ZMAX + 1];          // ore left in each level's seam
    uint32_t bars, tools, cloth, gems;   // stocks
    uint32_t cave_ins, mined, last_day;  // totals; the last day this layer ran
    uint8_t  deepest;
    uint8_t  scn, scn_done, safe;        // REPLAY LAYER: the scenario you chose (0 none), the scenarios finished (bit n), SANDBOX (1 = no creaks, no cave-ins, and no scenario pay)
} Mine;

typedef struct { uint8_t kind; uint32_t a, b, day; } MnEvent;
typedef struct {                         // what happened over a stretch of days
    uint32_t days, ore, bars, tools, cloth, gems, cave_ins, died, dug, ws_built, goals;
    int nev; MnEvent ev[8];
} MnAway;

// ---- names and rules
const char *mn_ws_name(int w);                          // "SMELTER"
const char *mn_ws_make(int w);                          // "2 ORE + 1 WOOD = 1 BAR"
int  mn_ws_rung(int w);                                 // the colony rung a workshop needs
int  mn_ws_max(uint16_t mask);                          // highest workshop level the open rungs allow (1..3)
int  mn_ws_cost(const Mine *m, int w);                  // wood to build the next level
int  mn_zcap(uint16_t mask);                            // deepest level the open rungs allow (0 = no mine)
int  mn_dig_need(int z);                                // miner-days to dig level z
int  mn_dig_wood(int z);                                // wood to open level z
int  mn_seam0(uint32_t town, int z);                    // how much ore level z holds when opened
int  mn_ore_per(int z);                                 // colony ore per unit dug at level z (deeper is richer)
#define MN_SAFE_RISK 24
int  mn_risk(const Mine *m, const SimColony *c, int z, int diff);   // per mille, the chance a working level starts to creak today
int  mn_cave_chance(const Mine *m, int z);              // percent, the chance a creaking level caves in tomorrow
int  mn_game_ore(int z, uint32_t salt);                 // which game ore a gem is: an index into the game's ore_def (ore.inc): 4..11 by depth

// ---- the record
void mn_init(Mine *m, uint32_t town, uint32_t day);
int  mn_miners(const SimColony *c);                     // settlers whose job is MINER
int  mn_staff(const SimColony *c, int w);               // settlers of the workshop's trade
int  mn_auto_level(const Mine *m, const SimColony *c, int diff);   // the level the colony would work on its own
void mn_day(Mine *m, SimColony *c, uint32_t day, int diff, ColAway *caw, MnAway *aw);
int  mn_catchup(SimColony *c, Mine *m, uint32_t today, uint32_t year_days, int diff, ColAway *caw, MnAway *aw);   // colony days and mine days together (as col_catchup_d)

// ---- by hand (each returns 1 when it did something)
int  mn_build_ws(Mine *m, SimColony *c, int w);         // build or upgrade a workshop from the colony's wood (smithy upgrades also take bars)
int  mn_brace(Mine *m, SimColony *c, int strong);       // prop the work level: 4 wood for +1, or 2 wood and a bar for +3
int  mn_dig_help(Mine *m, SimColony *c);                // you dig: +4 miner-days toward the next level (starts the dig)
int  mn_clear_help(Mine *m, SimColony *c);              // you clear rubble on the work level (+3)
int  mn_set_work(Mine *m, int z);                       // pick the work level (an open level); z 0 lets the colony pick again
typedef struct { int64_t gold; uint32_t bars, tools, cloth, gems; int ore_kind, ore_n; } MnGift;
int  mn_collect(Mine *m, MnGift *g);                    // you take the bars, tools, cloth and gems: gold, and gems as ore for your bag (at most MN_TAKE_GEMS a time)
#define MN_TAKE_GEMS 6

// ---- REPLAY LAYER (DEPTH_PLAN 6.2 and phase 11, a small part): SCENARIOS and a SANDBOX switch, per colony, opt-in, never forced.
// Pick one scenario at a time. Its goal is checked every day; the first time it holds, it pays gold into the colony's reward (taken with A like the raid gold)
// and tells you once. Each scenario pays once per colony. SANDBOX takes the cave-ins away (a calm way to dig down and look) and also the scenario pay.
enum { MNS_NONE, MNS_FEED20, MNS_DEEP8, MNS_RAIDS3, MNS_TOOLS10, MNS_CLOTH30, MNS_GEMS20, MNS_N };
const char *mn_scn_name(int s);                         // "REACH LEVEL 8"
const char *mn_scn_about(int s);                        // "DIG DOWN TO LEVEL 8"
int  mn_scn_pay(const SimColony *c, int s);             // gold the scenario pays (more on a dangerous planet)
int  mn_scn_check(const Mine *m, const SimColony *c, int s);   // 1 when the goal holds now
int  mn_scn_set(Mine *m, int s);                        // choose a scenario (0 none): 1 when it changed
int  mn_scn_next(Mine *m);                              // the next scenario you have not finished (a ring through 0 = none): returns it
void mn_set_safe(Mine *m, int on);

// ---- text
int  mn_event_text(const MnEvent *e, char *out, size_t n);
int  mn_away_text(const MnAway *a, const Mine *m, char lines[][48], int max);
void mn_level_text(const Mine *m, const SimColony *c, int z, int diff, char *out, size_t n);   // "Z3 OPEN SEAM 44 BRACE 2 RISK 9"
void mn_status_text(const Mine *m, char *out, size_t n);                                         // "CREAKING! BRACE LEVEL 5" or "LEVEL 5 OF 8 IN WORK"
size_t mn_dump(const Mine *m, char *out, size_t n);

// ---- the file (chunk MINE): one byte count, then records of MN_REC_BYTES
typedef struct { uint8_t n; Mine m[COL_MAX]; } MineList;
Mine *mn_find(MineList *l, uint32_t town, int make, uint32_t day);
size_t mn_list_write(const MineList *l, uint8_t *out, size_t cap);
int    mn_list_read(MineList *l, const uint8_t *in, size_t n);
#define MN_REC_BYTES 112
#endif
