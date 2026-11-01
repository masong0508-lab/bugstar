// colony.h: the colony side game (DEPTH_PLAN sections 1, 4, 5, 12; Phases 4 and 5). Plain C, no game types, no floating point.
//
// A SIDE game: it never reads or writes the action game's save. The only things that flow into the action game are what the player takes on the
// COLONY screen (food, gold, a nap) and invasion rewards (gold), both through col_collect, and only with the side game on.
//
// NONLINEAR LADDER. Each rung opens by ANY of its routes: an action-game feat, or a colony goal. Colony goals come by themselves with time,
// so every order of play leads to a full colony (colony_test proves 3 orders). The player also chooses which building goes up next, and where.
//
//  Rung      Action-game route           or colony goals
//   TENT     free a caged bug            make a friend in town (25), or reach level 10
//   FIELDS   clear a door dungeon        store 60 food, or 2 buildings
//   HOUSES   beat a story boss           3 babies born, or 2 fields
//   SHOPS    clear a drone outpost       store 100 wood, or 2 houses
//   MINE     own a pickaxe               6 buildings, or 2 years old
//   WALLS    clean out a hive            win a fight against raiders, or 6 settlers
//   MARKET   beat the Ant King           store 40 goods, or 10 settlers
//   MUSEUM   hold a relic                10 years old, or 3 raids beaten
//
// CARE AND FAILURE (difficulty CALM / NORMAL / HARSH). Settlers need food, shelter (houses), health (wells) and morale. Hunger and sickness kill
// settlers on NORMAL and HARSH, never on CALM. Raiders come with warnings (12, 3 and 1 days ahead). A raid stronger than the colony's defence kills
// settlers and burns buildings; a colony with nobody left becomes RUINS (resettle them later). Only an extreme loss destroys the whole planet:
// on HARSH a lost raid of 60+, on NORMAL a lost raid of 200+ on a colony already in ruins or dying, and never on a story world or on CALM.
// Beating a raid pays gold, growing steeply with its size and the planet's danger (col_reward).
#ifndef COLONY_H
#define COLONY_H
#include <stdint.h>
#include <stddef.h>

enum { COLR_TENT = 1, COLR_FIELDS, COLR_HOUSES, COLR_SHOPS, COLR_MINE, COLR_WALLS, COLR_MARKET, COLR_MUSEUM, COLR_N };
enum { COLJ_FORAGER, COLJ_FARMER, COLJ_BUILDER, COLJ_COOK, COLJ_MINER, COLJ_GUARD, COLJ_TRADER, COLJ_N };
enum { COLB_HUT, COLB_FIELD, COLB_WELL, COLB_STORE, COLB_HOUSE, COLB_KITCHEN, COLB_MINE, COLB_WALL, COLB_TOWER, COLB_MARKET, COLB_MUSEUM, COLB_N };
enum { COLS_ALIVE, COLS_RUINS, COLS_DEAD_WORLD };                // a colony's state
enum { COLD_CALM, COLD_NORMAL, COLD_HARSH };                      // side-game difficulty
enum { COLE_NONE, COLE_RUNG, COLE_BUILT, COLE_BORN, COLE_DIED, COLE_FOODWARN, COLE_RAIDWARN, COLE_RAIDWON, COLE_RAIDLOST, COLE_RUINS,
       COLE_WORLDEND, COLE_SICK, COLE_CROWDED, COLE_N };          // colony events (the journal, the away summary)

#define COL_MAXSET 20
#define COL_MAX    32            // colonies a save slot keeps
#define COL_GRID_W 8
#define COL_GRID_H 6
#define COL_CATCHUP_YEARS 100    // a colony runs by itself while you are away: up to ten decades of its planet's years
#define COL_YEAR_DAYS 28         // a year when the planet gives none: 4 seasons of 7 days
#define COL_STORE_MAX 999999999u // stores saturate here
#define COL_GOLD_MAX  0x7FFFFFFFFFFFFFFFull

typedef struct { int bugs_freed, zone_cleared, best_depth, bosses, outposts, ore_tool, hives, ant_king, relics, friends, level; } ColFlags;
typedef struct {
    uint32_t town;               // which town (a hash of the planet's side-game seed)
    uint16_t mask, told;         // rungs open (bit r = rung r), rungs whose card was shown
    uint8_t  nset, state, danger, world_ok;   // settlers; COLS_*; the planet's danger 0..9; 1 = an endless-galaxy planet (can be destroyed on the extreme path)
    uint32_t food, wood, ore, goods;
    uint32_t founded, last_day, born, died;
    uint8_t  bld[COLB_N];        // buildings of each kind
    uint8_t  grid[COL_GRID_W * COL_GRID_H];   // the colony map: 0 empty, else building kind + 1
    uint8_t  plan, plan_cell;    // what the builders raise next (COLB_*), and where (0xFF = first free cell)
    uint16_t progress;           // wood put into the planned building so far
    uint8_t  health, morale;     // 0..100
    uint16_t hungry_days;
    uint16_t raid_eta, raid_size; // days until the raid (0 = none coming), its size
    uint8_t  job[COL_MAXSET];    // 0xFF = the colony picks; else the job the player gave
    uint8_t  help_days, god_used; // days of the player's help left; a god power helped in the coming fight
    uint32_t raids_won, raids_lost;
    uint64_t reward_due, reward_total;   // gold owed to the action game; all gold ever paid
} SimColony;

typedef struct { uint8_t kind; uint32_t a, b; uint32_t day; } ColEvent;
typedef struct {                 // what happened over a stretch of days (one tick, or a long catch-up)
    uint32_t days, years, born, died, settlers, built0, built1, nbuilt;
    uint32_t raids_won, raids_lost; uint64_t reward;
    uint8_t  ruined, world_end;
    int nev; ColEvent ev[12];    // the notable events, newest kept
} ColAway;

uint16_t col_mask(const ColFlags *f);                    // rungs opened by the action game (bit 0 always set)
uint16_t col_goal_mask(const SimColony *c, const ColFlags *f, uint32_t today, uint32_t year_days);   // rungs opened by colony goals
const char *col_rung_name(int r);
const char *col_rung_how(int r);                         // the action-game route, "FREE A CAGED BUG"
const char *col_rung_goal(int r);                        // the colony-goal routes, "OR MAKE A FRIEND IN TOWN"
const char *col_rung_card(int r);
const char *col_job_name(int j);
const char *col_bld_name(int b);
int  col_bld_rung(int b);                                // the rung a building kind needs
int  col_bld_cost(const SimColony *c, int b);            // wood
int  col_bld_ok(const SimColony *c, int b);              // 1 if it may be raised now
int  col_cap(uint16_t mask);                             // most settlers the open rungs allow
int  col_buildings(uint16_t mask);                       // (kept for old callers) how many kinds can be built
int  col_built_total(const SimColony *c);
int  col_defence(const SimColony *c);
int  col_next_rung(uint16_t mask);
void col_init(SimColony *c, uint32_t town, uint32_t day);
uint16_t col_update(SimColony *c, uint16_t mask);         // opens rungs (never closes one); new rungs bring settlers; returns rungs to tell
void col_settler(const SimColony *c, int i, char *name, int *job);
void col_day(SimColony *c, uint32_t day);                // one day on NORMAL with no record (tests)
void col_day_ex(SimColony *c, uint32_t day, uint32_t year_days, int diff, ColAway *aw);
uint32_t col_year_days(int season_days);
int  col_catchup(SimColony *c, uint32_t today);
int  col_catchup_ex(SimColony *c, uint32_t today, uint32_t year_days, ColAway *aw);   // NORMAL
int  col_catchup_d(SimColony *c, uint32_t today, uint32_t year_days, int diff, ColAway *aw);
int  col_away_text(const ColAway *a, const SimColony *c, char lines[][48], int max);
int  col_event_text(const ColEvent *e, char *out, size_t n);
uint64_t col_reward(uint32_t size, int danger, int no_deaths, int no_god, int brink);   // gold for beating a raid (saturating)

// hands-on (all optional; the colony runs without them). Each returns 1 when it did something.
int  col_set_job(SimColony *c, int i, int job);          // job -1: let the colony pick
int  col_set_plan(SimColony *c, int b, int cell);        // what to build next and where (cell -1: first free)
int  col_help_build(SimColony *c);                       // you carry wood and build: +15 progress (from the store if it has it, else your own sweat: +5)
int  col_carry_food(SimColony *c, int amount);           // you bring food from your bag: amount points (2 colony food each)
int  col_help_guard(SimColony *c);                       // you stand guard for 3 days: defence +10
int  col_god_help(SimColony *c, int kind);               // a god power: 0 feast (food), 1 heal, 2 shield (defence for the next fight)
int  col_carry_wood(SimColony *c, int cell);             // map: carry wood to an empty cell, plan the plan there, +12 progress (4 if the store is short)
int  col_burnt(const SimColony *c);                      // 1 for a destroyed planet: kept in the records for good, never resettled (settle another world)
int  col_invade(SimColony *c, uint32_t size, int diff, uint32_t day, ColAway *aw, uint64_t *reward);   // a real raid now: 1 won, -1 lost, 0 no colony to attack. The reward lands in reward_due
int  col_resettle(SimColony *c, uint32_t day);           // ruins become a tent with 2 settlers again
int  col_evacuate(SimColony *from, SimColony *to);       // settlers move to another colony (as many as it can house)

typedef struct { int food_pts, heal; int64_t gold; uint32_t food_used, goods_sold, ore_sold; uint64_t reward; } ColGift;
int  col_collect(SimColony *c, int hunger_room, ColGift *g);
size_t col_dump(const SimColony *c, char *out, size_t n);

typedef struct { uint8_t n; SimColony c[COL_MAX]; } ColList;
SimColony *col_find(ColList *l, uint32_t town, int make, uint32_t day);
size_t col_list_write(const ColList *l, uint8_t *out, size_t cap);
int    col_list_read(ColList *l, const uint8_t *in, size_t n);
#define COL_REC_BYTES 164
#endif
