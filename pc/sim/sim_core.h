// sim_core.h: the side game's simulation (DEPTH_PLAN sections 3 and 4, Phase 1). Plain C, no game types, no floating point, no globals.
//
//  - Fixed step: SIM_HZ = 10 ticks a second. In the game one tick = 6 vblanks (sim_feed).
//  - Deterministic: every random number is sim_rng(world_seed, id, purpose, tick), a stateless hash. It never touches the game's rnd().
//  - Everything about a town is a function of the planet seed and the tick count. Only what the player did is saved (sim_file below).
//  - Names that could clash with the game start with sim_ or SIM_. (The game's own names are never used here.)
#ifndef SIM_CORE_H
#define SIM_CORE_H
#include <stdint.h>
#include <stddef.h>
#include "lv.h"
#include "god.h"
#include "colony.h"
#include "mine.h"
#include "standing.h"

#define SIM_HZ         10
#define SIM_TICK_VBL   6            // one tick is 6 vblanks of the 60 Hz screen
#define SIM_DAY_TICKS  4800         // Phase 1: a day is 8 minutes. (Phase 2 makes it depend on the planet.)
#define SIM_SEASON_DAYS 7           // Phase 2: 7 days to a season, 4 seasons
#define SIM_FRMAX 64
#define SIM_MAXFOLK    16
#define SIM_LOGN       4096         // event ring
#define SIM_TAILN      256          // events kept in the save file
#define SIM_NAMELEN    10
#define SIM_SLOTS      3
#define SIM_MAXSPECIES 8

uint64_t sim_mix64(uint64_t x);
uint64_t sim_rng(uint64_t world_seed, uint32_t id, uint32_t purpose, uint64_t tick);
uint32_t sim_rngn(uint64_t world_seed, uint32_t id, uint32_t purpose, uint64_t tick, uint32_t n);   // 0 .. n-1

enum { SIMJ_STALL, SIMJ_COOK, SIMJ_CLERK, SIMJ_RACER, SIMJ_FORAGER, SIMJ_MINER, SIMJ_GUARD, SIMJ_N };   // jobs
enum { SIMT_SLEEP, SIMT_EAT, SIMT_WORK, SIMT_LEISURE, SIMT_SOCIAL, SIMT_N };                              // what a bug is doing
enum { SIMP_HOME, SIMP_EATERY, SIMP_WORK, SIMP_PLAZA, SIMP_N };                                           // where (the game maps these to map spots)
enum { SIMN_HUNGER, SIMN_REST, SIMN_SOCIAL, SIMN_FUN, SIMN_N };                                           // needs: 0 bad .. 255 good
enum { SIMW_CLEAR, SIMW_CLOUD, SIMW_RAIN, SIMW_STORM, SIMW_SNOW, SIMW_N };                                           // weather

enum { SIME_NONE, SIME_WAKE, SIME_SLEEP, SIME_WORK, SIME_EAT, SIME_CHAT, SIME_HUNGRY, SIME_TIRED, SIME_WEATHER, SIME_DAWN, SIME_CAST,
       SIME_UNLOCK, SIME_EMPTY, SIME_HARVEST, SIME_NEWFOLK, SIME_SPECIES, SIME_LEGEND, SIME_IDEA, SIME_SANDBOX, SIME_FRIEND, SIME_LETTER, SIME_COLONY,
       SIME_BOND, SIME_RIVAL, SIME_COUPLE, SIME_BREAKUP, SIME_QUARREL, SIME_SICK, SIME_WELL, SIME_BREAK, SIME_CARE,
       SIME_GIFT, SIME_HELP, SIME_WORRY, SIME_STAND, SIME_N };   // v2 life: who/b = the two bugs   // SIME_COLONY: a = colony event kind, b = its first number, seed32 = its second

typedef struct { uint32_t tick; uint16_t kind, who, a, b; uint32_t seed32; } SimEvent;     // 16 bytes

typedef struct {
    char name[SIM_NAMELEN + 1];
    uint8_t job, home, task, place, shift;      // shift: 0..2 hours later bedtime (night owls)
    uint8_t need[SIMN_N];
    uint8_t warned;                             // bit 0: said hungry, bit 1: said tired (so the log does not repeat itself)
    uint8_t skill;
    uint16_t pal;                               // who it chatted with last
    // life v2 (all derived from the seed or rebuilt as the town runs; nothing here is saved to the .sim file)
    uint8_t trait[4];                           // 0..255: sociable, diligent, hearty, calm (made from the seed: the same bug is always the same person)
    uint8_t sick;                               // days of illness left
    uint8_t stress;                             // 0..255, slow: bad needs raise it, good days and sleep lower it
    int8_t  mate;                               // the bug it is a couple with, or -1
    int8_t  bond[SIM_MAXFOLK];                  // how it feels about each other bug: -100 rival .. +100 dear friend
} SimFolk;
enum { SIMTR_SOCIABLE, SIMTR_DILIGENT, SIMTR_HEARTY, SIMTR_CALM };

typedef struct {
    uint64_t seed, tick;
    uint32_t day_ticks;
    uint8_t  acc;                               // vblanks not yet turned into a tick
    int      nfolk;
    SimFolk  folk[SIM_MAXFOLK];
    // the land (god powers act here)
    uint8_t  fertility, water, shelter, comfort, health, luck, pests, forest, hills, birds, ideas, legends, nspecies, ore;
    uint8_t  weather; uint16_t weather_left;
    uint32_t larder;
    uint16_t spirit;                            // 0..SIM_SPIRIT_MAX. Powers cost spirit.
    uint32_t species_seed[SIM_MAXSPECIES];
    GodState god;
    uint32_t log_n, log_head;                   // total events ever, ring head
    SimEvent log[SIM_LOGN];
    uint8_t friend[SIM_MAXFOLK];                // Phase 3: how much each townsperson likes you, 0..100
    uint32_t talked[SIM_MAXFOLK];               // the day (+1) you last talked to each
    uint8_t env_on, locked; int16_t temp_c;      // Phase 2: set by sim_set_env (the planet's day length and warmth)
    uint32_t day_seen;                          // the day new_day last ran for
    uint32_t steps_done;                        // for tests
    // patch 15 TIES (runtime only: not in sim_hash, not in the save; the glue sets them again every few seconds)
    uint32_t helped[SIM_MAXFOLK];               // the day (+1) you last helped each townsperson
    uint8_t  worry, aid;                        // from the colony: 0..2 how much the town fears for it; 0..2 how well it feeds the town
} Sim;
#define SIM_SPIRIT_MAX 100

// jobmask: bit j set when the town can employ job j (the game passes what buildings it has). 0 means "everything".
void sim_init(Sim *s, uint64_t world_seed, int nfolk, uint32_t jobmask, uint64_t start_tick);
void sim_step(Sim *s);                          // exactly one tick
int  sim_feed(Sim *s, int vblanks, int maxsteps);   // adds vblanks; runs whole ticks (at most maxsteps); returns how many ran
void sim_run(Sim *s, uint64_t ticks);           // tests: n ticks
uint64_t sim_hash(const Sim *s);                // one number for the whole state (log included)

int  sim_hour(const Sim *s);                    // 0..23
int  sim_day(const Sim *s);
// Phase 2: the planet's day. day_hours from the planet (0 = tidally locked: no day, a fixed sun); day length is 4800 x hours / 24 ticks, clamped to 1200..12000.
void sim_set_env(Sim *s, int day_hours, int temp_c);
int  sim_season(const Sim *s);                  // 0 spring, 1 summer, 2 autumn, 3 winter
const char *sim_season_name(int se);
int  sim_sun_hour(const Sim *s);                // the sun's hour (12 always on a tidally locked planet)                     // 0 based
const char *sim_task_name(int t);
const char *sim_job_name(int j);
const char *sim_weather_name(int w);
void sim_name(uint32_t seed32, uint32_t id, char *out);        // a bug's name (3 to 8 letters)
void sim_species_name(uint32_t seed, char *out);
int  sim_mood(const SimFolk *f);                // 0..255: the average of the needs, less stress and illness
const char *sim_trait_text(const SimFolk *f);   // "SHY", "CHEERFUL", "LAZY", "TIRELESS", "TOUGH", "FRAGILE", "CALM", "TENSE" or "EVEN": the trait that stands out most
int  sim_bond(const Sim *s, int a, int b);      // -100..100 (0 for no such bug)
int  sim_best_pal(const Sim *s, int k);         // the bug k likes most (>= 20), or -1
void sim_status_text(const SimFolk *f, char *out, size_t n);  // "TIRED. HUNGRY." or "FINE."

// Phase 3: friends and letters. Talking to somebody (once a day each) makes them like you more; at 25, 50, 75 and 100 they write you a letter.
// Returns the new friendship (0..100), or -1 when you already talked today / no such person.
int  sim_talk_to(Sim *s, int k);
int  sim_letter_text(const Sim *s, int who, int step, char *out, size_t n);
void sim_log(Sim *s, int kind, int who, int a, int b);
int  sim_event_text(const Sim *s, const SimEvent *e, char *out, size_t n);   // returns length
const SimEvent *sim_log_at(const Sim *s, uint32_t back);     // 0 = newest. Null past the end.
uint32_t sim_log_count(const Sim *s);          // how many are still in the ring

// God powers in the side game. level is the player's level; sandbox opens everything and costs nothing.
// Returns 0 ok, -1 locked, -2 not enough spirit, -3 no such power.
int  sim_cast(Sim *s, const Lv *level, int n);
void sim_check_unlocks(Sim *s, const Lv *level);   // logs one message per newly opened power (and calls god_check)
void sim_set_sandbox(Sim *s, int on, const Lv *level);

// The text dump the tests read: one fact per line. Returns the length written.
size_t sim_dump(const Sim *s, char *out, size_t n);

// ---------------------------------------------------------------- the side file (<sav>.sim)
// Header "BSIM" + version, then chunks: tag[4], slot u8, version u8, length u32, fnv u32, data. Unknown tags and bad chunks are skipped.
typedef struct {
    uint8_t  used;
    uint64_t cal_ticks;                         // sim ticks played in this slot (the calendar)
    GodState god; uint16_t spirit;
    Lv       lv_true, lv_xp;                    // the big level (see pc/sim/BIGLEVEL in README.md): used only above the s64 range
    uint8_t  has_lv;
    uint8_t nfr; struct { uint32_t town; uint8_t who, val; } fr[SIM_FRMAX];   // Phase 3: friendships, by town (seed hash) and townsperson
    ColList  cols;                              // Phase 4: the colonies of this slot (chunk COLN)
    MineList mines;                             // Phase 6: the workshops and mine of each colony (chunk MINE; see mine.h)
    Standing stand;                             // patch 42: how the world sees you: mayor, armada, worlds (chunk STND; see standing.h)
    uint16_t ntail; SimEvent tail[SIM_TAILN];   // the newest events, oldest first
} SimSlot;
typedef struct { uint8_t side_on; uint8_t sandbox; uint8_t difficulty; SimSlot slot[SIM_SLOTS]; int bad_chunks; } SimFile;
void   simfile_init(SimFile *f);
size_t simfile_write(const SimFile *f, uint8_t *buf, size_t cap);     // bytes written (0 if buf too small)
int    simfile_read(SimFile *f, const uint8_t *buf, size_t n);        // 0 ok, -1 not a .sim file (f stays empty). Bad chunks are counted in f->bad_chunks.
#define SIMFILE_MAX (64 * 1024)
uint32_t sim_fnv(const void *p, size_t n);
void   sim_save_friends(const Sim *s, SimSlot *sl);
void   sim_load_friends(Sim *s, const SimSlot *sl);
void   sim_save_tail(const Sim *s, SimSlot *sl);
void   sim_load_tail(Sim *s, const SimSlot *sl);


// ---------------------------------------------------------------- patch 15: TIES
// Friends give gifts that fit their job; sick/stressed/feuding bugs ask for help and helping pays in friendship;
// friends (50+) lend their trade to the colony; the colony's trouble stresses the town and its surplus feeds it.
enum { SIMG_NONE, SIMG_FOOD, SIMG_GOLD, SIMG_HEAL, SIMG_ORE, SIMG_N };
typedef struct { uint8_t kind; uint16_t amt; } SimGift;      // GOLD: amt in units (the game scales). FOOD: hunger points. ORE: pieces.
int  sim_gift(const Sim *s, int who, int step, SimGift *g);
const char *sim_gift_name(int kind);
enum { SIMA_NONE, SIMA_MEAL, SIMA_LISTEN, SIMA_PEACE, SIMA_N };
int  sim_ask(const Sim *s, int k, int *with);
void sim_ask_text(int ask, char *out, size_t n);
int  sim_ask_need(int ask);                                  // friendship needed before you can help (0, 10, 25)
int  sim_help(Sim *s, int k, int ask);                       // 1 done, 0 nothing to do / already helped today, -1 not close enough yet
typedef struct { uint8_t guard, food, wood, ore, goods, mood, couples, friends, sick, asks; } SimTies;   // guard..goods 0..6
void sim_ties(const Sim *s, SimTies *t);
void sim_ties_text(const SimTies *t, char *out, size_t n);
void sim_set_outside(Sim *s, int worry, int aid);
const char *sim_perk_text(const Sim *s, int k);

#endif
