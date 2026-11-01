// laws.h: the PC build's UNIVERSE LAWS. 146 named numbers (gravity strength, speed of light, galaxy shapes, star mix, planet size, life chance, day length...)
// that really drive how the universe, its galaxies, stars and planets are made. Plain C, no game code, no libm: the same answer on every machine.
//   * a LawSet holds one value per law (a whole number); the default set is the game as it always was (every hook is a no-op at the defaults)
//   * the MODEL turns a LawSet and a place (galaxy, star, planet, plus the game's own planet DNA) into physical numbers (lw_eval) and into
//     the changes the game applies (lw_apply). Every number is worked out as "how the law set changes the default", so default laws change nothing.
//   * lw_explain writes the HOW THIS WORKS pages as plain text with the current numbers filled in.
#ifndef LAWS_H
#define LAWS_H
#include <stdint.h>
#include <stddef.h>

enum { LC_UNIVERSE, LC_GALAXY, LC_STARS, LC_PLANETS, LC_LIFE, LC_TIME, LC_N };
#define LF_ADV  1       // only shown with ADVANCED on
#define LF_NUM  2       // always shown as a number, never as "X2" or "NORMAL"
#define LF_ENUM 4       // the value is an index into a list of names
#define LF_SIZE 8       // the value is quarter-steps of doubling (PLANET SIZE)
#define LF_VARY 16      // UNIVERSE VARIETY may bend it from one universe to the next

enum {
#define X(id,cat,name,desc,unit,mn,mx,df,st,bg,xmn,xmx,dec,fl) LW_##id,
#include "laws_def.h"
#undef X
    LW_N };
#define LW_MAXLAWS 192

typedef struct { const char *key, *name, *desc, *unit; uint8_t cat, dec, flags; int32_t min, max, def, step, big, xmin, xmax; } LawDef;
extern const LawDef lw_defs[LW_N];
extern const char *const lw_cat_name[LC_N];

typedef struct { int32_t v[LW_N]; int cheated; } LawSet;      // cheated: 1 once anything outside the sensible range or any cheat was used in this world

// ---- the table and editing
int  lw_count(void);                                             // 146
void lw_default(LawSet *s);
int  lw_find(const char *key);                                   // law id from its key ("U_GRAV"), or -1
int  lw_is_default(const LawSet *s);
int  lw_is_edited(const LawSet *s, int id);
int  lw_edited_count(const LawSet *s);
int  lw_clamp_one(int id, int32_t v, int cheat);                 // the value inside the sensible range (cheat = 0) or the wide range (cheat = 1)
void lw_clamp_all(LawSet *s, int cheat);                         // clamps every law; with cheat = 0 it also marks nothing. Use lw_set to mark cheats
int  lw_set(LawSet *s, int id, int32_t v, int cheat);            // sets (clamped); returns the value that was stored. Going outside the sensible range sets s->cheated
int  lw_step(LawSet *s, int id, int dir, int big, int cheat);    // one step up (dir > 0) or down; big uses the big step; returns the new value
void lw_reset_one(LawSet *s, int id);
void lw_reset_all(LawSet *s);
uint64_t lw_hash(const LawSet *s);                               // a hash of all values (0-th set = a fixed number)
int  lw_match(int id, const char *query);                        // 1 when the law's name or text holds every word of the query (upper case)
int  lw_visible(int id, int advanced);                           // is the law shown in the simple list?
const char *lw_enum_name(int id, int v);                         // for LF_ENUM laws
// value text: simple mode gives NORMAL, OFF, X0.5, X2... (numbers for LF_NUM laws); advanced gives the number with its unit
void lw_value_text(const LawSet *s, int id, int advanced, char *out, int n);
void lw_raw_text(int id, int32_t v, char *out, int n);           // number and unit only
// ---- presets (plain words first)
int  lw_preset_count(void);
const char *lw_preset_name(int i);
const char *lw_preset_desc(int i);
void lw_preset_apply(LawSet *s, int i);                          // starts from the defaults, then sets what the preset names
// ---- files (text, one "KEY=VALUE" per edited law, plus cheated=0|1)
int  lw_save(const LawSet *s, const char *path);
int  lw_load(LawSet *s, const char *path);                       // 1 ok. A missing file gives the defaults and returns 0
int  lw_to_text(const LawSet *s, char *out, int n);              // the same text in a buffer
int  lw_from_text(LawSet *s, const char *text);

// ---- the universe may differ from universe to universe (UNIVERSE VARIETY)
void lw_effective(const LawSet *s, uint64_t useed, LawSet *out);

// ---- the model
enum { GT_SPIRAL, GT_BARRED, GT_RING, GT_DWARF, GT_ELLIP, GT_LENS, GT_IRREG, GT_MERGE, GT_N };
enum { SC_O, SC_B, SC_A, SC_F, SC_G, SC_K, SC_M, SC_WD, SC_GIANT, SC_NS, SC_N };
extern const char *const lw_gtype_name[GT_N], *const lw_sclass_name[SC_N];

typedef struct {                       // the place and the game's own DNA of the planet there (all from the game's generator; tests make them up)
    uint64_t useed;                    // the universe
    uint32_t gal;                      // gal_hash(): 0 = the home galaxy
    uint32_t star;                     // sys_id(): the star
    uint32_t seed_a, seed_b, seed_c;   // the planet's three seeds
    int story;                         // 1 = one of the six story worlds (only size and gravity follow the laws there)
    int planet, nplan, moon, nmoon;    // which planet of the star (0..), how many, which moon (0 = the planet itself), how many moons it has
    int psz, grav, cls, hour, weather, snow, sand, city, ruins, fauna, beetles, rough, hs, mood, water, snowl;   // WorldDNA, and the water level
    int hell;                          // 1 when the world is one of the six hells
    int dist_clusters_log2;            // how many bits away from home (0 = home cluster); sets the danger a little
} LwEnv;

enum {
#define LO(id,name,unit) LO_##id,
#include "laws_out.h"
#undef LO
    LO_N };
typedef struct { double o[LO_N]; int gtype, sclass, nsuns, locked, rogue, gas, fate, hour_roll; double hid[6]; } LwResult;
extern const char *const lw_out_name[LO_N], *const lw_out_unit[LO_N];

void lw_eval(const LawSet *s, const LwEnv *e, LwResult *r);      // every physical number for this place under the (effective) laws

typedef struct {                       // what the game does to the planet (every field means "no change" at the default laws)
    int changed;                       // 1 if anything below differs from the game's own planet
    int size_q;                        // planet radius change in quarter-octaves against the DNA's size (0 = as generated)
    int k16;                           // horizon bend in sixteenths (the game's own is 16 x {12,7,4,3,2,1} by size class)
    int tw;                            // how many cells round the walkable world is: 128 normally, 64, 32, 16 or 8 on tiny worlds
    int psz;                           // the nearest of the game's six size classes (0 tiny .. 5 giant)
    int grav;                          // 0..3 the game's gravity step
    int slow;                          // walking speed lost to heavy gravity (the game's 0x1600 base speed minus this)
    int zerog;                         // 0, or 1..8: the game's ZERO-G trait level
    int hour, weather, snow, sand, cls, city, ruins, fauna, beetles, rough, hs, mood, snowl;   // new values for WorldDNA (-1 = keep)
    int water_pct;                     // water level multiplier in percent for new_planet (100 = keep)
    int aurora, meteor, badair;        // levels of the game's AURORA, METEORS and BAD AIR traits (0 = keep what the world has)
    int hunger_pct;                    // hunger speed in percent (100 = as is)
    int day_pct;                       // how fast the sky turns in percent (100 = as is): a long day turns the sun slowly
    int danger_add;                    // levels added to the world's danger
    int node_add[4];                   // extra resource nodes: tree, rock, web, flower
} LwApply;
void lw_apply(const LawSet *s, const LwEnv *e, LwApply *a);

// planet and moon counts, travel, fuel and chances (the game calls these with its own number; they return it unchanged at the default laws)
int  lw_nplan(const LawSet *s, const LwEnv *e, int orig);
int  lw_nmoon(const LawSet *s, const LwEnv *e, int orig);
int  lw_dense(const LawSet *s, uint32_t roll, int orig_odds);    // a crowded galaxy? roll is the game's hash; orig_odds is its 1 in N
int  lw_hell_hit(const LawSet *s, int roll100, int hp);          // the game's hell world test (roll100 < hp)
int  lw_weird_hit(const LawSet *s, uint32_t f);                  // the game's 1-in-111 weird world test
int  lw_uly(const LawSet *s);                                    // light years per cluster
int  lw_galbits(const LawSet *s);                                // galaxy block size: 2^N clusters a side
uint32_t lw_hours(const LawSet *s, uint32_t hours);              // a voyage's hours under the speed of light
int  lw_warp_cost(const LawSet *s, int cost);                    // fuel for a warp under the expansion and dark energy
int  lw_time_pct(const LawSet *s);                               // game speed in percent

// ---- HOW THIS WORKS: plain text pages with the numbers of this place
#define LX_PAGES 9
#define LX_LINES 17
#define LX_W 60
typedef struct { char title[32]; int n; char t[LX_LINES][LX_W]; uint8_t col[LX_LINES]; } LxPage;     // col: 0 white, 1 gold, 2 grey, 3 cyan, 4 green, 5 red
int  lw_explain(const LawSet *s, const LwEnv *e, LxPage *pages);   // fills up to LX_PAGES pages; returns how many
// number words (the same ones the screens use)
void lw_fmt_big(double v, char *out, int n);                     // 1,234 / 1.5 BILLION / 3.2E21
void lw_fmt_time(double seconds, char *out, int n);              // 3 HOURS / 12 DAYS / 4.5 BILLION YEARS
void lw_fmt_dist(double metres, char *out, int n);               // 6 CM / 40,000 KM / 3 LIGHT YEARS
void lw_fmt_sci(double v, char *out, int n);                     // 6.4E6
double lw_pw2(double x);                                         // 2^x without libm
double lw_lg2(double x);                                         // log2 without libm
uint64_t lw_mix(uint64_t x);                                     // the hash everything here uses
#endif
