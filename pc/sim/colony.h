// colony.h: the colony side game, Phase 4 (DEPTH_PLAN sections 1.1, 1.2): the ActRaiser-style unlock ladder and the first settlers.
// Plain C, no game types. The game fills ColFlags from its own variables (sim_glue.inc); everything else is a function of the town seed and the days.
//
//  Rung (bit)  Action-game milestone                 Unlocks in the colony
//   1  TENT    free a caged bug                      a tent and 2 settlers
//   2  FIELDS  clear a door dungeon / depth 3        fields, a well, a stockpile; 4 settlers
//   3  HOUSES  beat a story boss                     houses and your bug home; 6 settlers
//   4  SHOPS   clear a drone outpost                 workshops (kitchen, loom); 8 settlers
//   5  MINE    own a pickaxe (ore tool 1+)           the colony mine; 10 settlers
//   6  WALLS   clean a hive                          guard post and walls; 12 settlers
//   7  MARKET  beat the Ant King                     market hall; 20 settlers
//   8  MUSEUM  hold a relic                          museum
// Rung 0 (land anywhere: townsfolk and the journal) is always open.
#ifndef COLONY_H
#define COLONY_H
#include <stdint.h>
#include <stddef.h>

enum { COLR_TENT = 1, COLR_FIELDS, COLR_HOUSES, COLR_SHOPS, COLR_MINE, COLR_WALLS, COLR_MARKET, COLR_MUSEUM, COLR_N };
enum { COLJ_FORAGER, COLJ_FARMER, COLJ_BUILDER, COLJ_COOK, COLJ_MINER, COLJ_GUARD, COLJ_TRADER, COLJ_N };
#define COL_MAXSET 20
#define COL_MAX    32           // colonies a save slot keeps
#define COL_CATCHUP_YEARS 100   // a colony runs by itself while you are away: up to ten decades of its planet's years
#define COL_YEAR_DAYS 28        // a year when the planet gives none: 4 seasons of 7 days (Phase 2)
#define COL_STORE_MAX 999999999u   // stores saturate here

typedef struct { int bugs_freed, zone_cleared, best_depth, bosses, outposts, ore_tool, hives, ant_king, relics; } ColFlags;
typedef struct {
    uint32_t town;              // which town (a hash of the planet's side-game seed)
    uint16_t mask, told;        // rungs open (bit r = rung r), rungs whose card was shown
    uint8_t  nset, built;       // settlers living here, buildings finished
    uint32_t food, wood, ore, goods;
    uint32_t founded, last_day; // the day the tent went up, the last day the colony was run for
    uint32_t born;              // babies born here, ever (saturating)
} SimColony;

uint16_t col_mask(const ColFlags *f);                    // which rungs the action game has opened (bit 0 always set)
const char *col_rung_name(int r);                        // "TENT"
const char *col_rung_how(int r);                         // "FREE A CAGED BUG"
const char *col_rung_card(int r);                        // "A TENT! 2 SETTLERS ARE WAITING."
const char *col_job_name(int j);
int  col_cap(uint16_t mask);                             // settlers allowed by the open rungs
int  col_buildings(uint16_t mask);                       // buildings the open rungs allow
int  col_next_rung(uint16_t mask);                       // the lowest rung still shut (0 when all are open)
void col_init(SimColony *c, uint32_t town, uint32_t day);
// Opens rungs from the mask (never closes one), fills settlers up to the cap. Returns the rungs that opened now and were not told yet; marks them told.
uint16_t col_update(SimColony *c, uint16_t mask);
void col_day(SimColony *c, uint32_t day);                // one day of work (deterministic; uses only the town and the day)
int  col_catchup(SimColony *c, uint32_t today);          // runs the days since last_day (at most COL_CATCHUP_YEARS of 28 days); returns how many ran
// While you were away: the days since the last visit, up to COL_CATCHUP_YEARS planet years, worked day by day with the jobs worked out once
// (the same result as calling col_day for each day: colony_test proves it). year_days = col_year_days(the planet's season length).
typedef struct { uint32_t days, years, born, settlers, built0, nbuilt, built_day[4]; uint32_t food0, wood0, ore0, goods0; } ColAway;
uint32_t col_year_days(int season_days);
int  col_catchup_ex(SimColony *c, uint32_t today, uint32_t year_days, ColAway *aw);
int  col_away_text(const ColAway *a, const SimColony *c, char lines[][48], int max);   // the WHILE YOU WERE AWAY summary, short lines; returns how many
void col_settler(const SimColony *c, int i, char *name, int *job);   // the i-th settler's name (from the town) and job
// Phase 5: what the colony sends to the action game. hunger_room = how much the player's food bar can still take (0..100).
// Takes food for the bar (2 colony food = 1 point), sells goods (5 gold each) and ore (3 gold each). heal = 1 when the bug home (HOUSES) is open.
typedef struct { int food_pts, gold, heal; uint32_t food_used, goods_sold, ore_sold; } ColGift;
int  col_collect(SimColony *c, int hunger_room, ColGift *g);     // 1 if anything was given
size_t col_dump(const SimColony *c, char *out, size_t n);

// the colonies of one save slot (kept in the .sim file, chunk COLN)
typedef struct { uint8_t n; SimColony c[COL_MAX]; } ColList;
SimColony *col_find(ColList *l, uint32_t town, int make, uint32_t day);   // finds (or makes, dropping the oldest when full) the colony of this town
size_t col_list_write(const ColList *l, uint8_t *out, size_t cap);       // bytes written
int    col_list_read(ColList *l, const uint8_t *in, size_t n);           // 0 ok, -1 bad
#define COL_REC_BYTES 42
#endif
