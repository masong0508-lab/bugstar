// lawx.h: the flat face of the universe laws for the game (plain ints and char buffers; no names that clash with the game's own).
// The game's code (pc/pc_laws.inc, #included at the end of the generated main.c) calls only these. The state is one law set (the world you are playing).
#ifndef LAWX_H
#define LAWX_H
#include <stdint.h>
#define PLW_CATS 6
typedef struct {                       // the place and the planet's own DNA (the game fills it in)
    uint32_t uni[8];                   // the universe number (256 bits, low word first)
    uint32_t gal, star, seed_a, seed_b, seed_c;
    int story, planet, nplan, moon, nmoon;
    int psz, grav, cls, hour, weather, snow, sand, city, ruins, fauna, beetles, rough, hs, mood, water, snowl;
    int hell, dist_log2;
} PlwEnv;
typedef struct {                       // what the game changes (see LwApply in laws.h); -1 = keep
    int changed, size_q, k16, tw, psz, grav, slow, zerog;
    int hour, weather, snow, sand, cls, city, ruins, fauna, beetles, rough, hs, mood, snowl;
    int water_pct, aurora, meteor, badair, hunger_pct, day_pct, danger_add, node_add[4];
} PlwApply;
#define PLW_LINES 17
#define PLW_W 60
#define PLW_PAGES 9
typedef struct { char title[32]; int n; char t[PLW_LINES][PLW_W]; uint8_t col[PLW_LINES]; } PlwPage;

// ---- the table and editing
int  plw_count(void);                                           // how many laws
int  plw_cat_n(void);                                           // 6 categories
const char *plw_cat_name(int c);
int  plw_cat(int id);
const char *plw_key(int id), *plw_name(int id), *plw_desc(int id), *plw_unit(int id);
int  plw_visible(int id, int advanced);
int  plw_match(int id, const char *query);                      // upper case words
int  plw_is_edited(int id);
int  plw_edited_count(void);
int  plw_is_default(void);
int  plw_value(int id);
void plw_value_text(int id, int advanced, char *out, int n);
int  plw_step(int id, int dir, int big, int cheat);             // returns the new value
int  plw_setv(int id, int v, int cheat);                        // sets (clamped); returns what is stored
void plw_reset_one(int id);
void plw_reset_all(void);
int  plw_cheated(void);
void plw_set_cheated(int on);
int  plw_in_sensible(int id);                                   // the value is inside the normal range
// ---- presets
int  plw_preset_count(void);
const char *plw_preset_name(int i), *plw_preset_desc(int i);
void plw_preset_apply(int i);
// ---- undo (a stack of the last 24 states)
void plw_undo_push(void);
int  plw_undo_pop(void);                                        // 1 when something was undone
int  plw_undo_depth(void);
// ---- files (one per save slot: <sav>.laws<slot>)
void plw_slot_path(const char *sav, int slot, char *out, int n);
int  plw_load(const char *path);                                // 1 ok; missing file = defaults, returns 0
int  plw_save(const char *path);                                // a default set deletes the file
void plw_file_remove(const char *path);
void plw_file_copy(const char *from, const char *to);           // a missing source deletes the target
// ---- the model
void plw_apply(const PlwEnv *e, PlwApply *a);
int  plw_nplan(const PlwEnv *e, int orig);
int  plw_nmoon(const PlwEnv *e, int orig);
int  plw_dense(uint32_t roll, int odds);
int  plw_hell_hit(int roll100, int hp);
int  plw_weird_hit(uint32_t f);
int  plw_uly(void);
int  plw_galbits(void);
uint32_t plw_hours(uint32_t hours);
int  plw_warp_cost(int cost);
int  plw_time_pct(void);
int  plw_explain(const PlwEnv *e, PlwPage *pages);              // HOW THIS WORKS: returns the number of pages
// live effect lines for the law on the cursor: a few real numbers of this place under the current laws
int  plw_live(const PlwEnv *e, int id, char lines[][PLW_W], int max);
// a number of this place by name (cheats, tests)
double plw_out(const PlwEnv *e, const char *key);
void plw_fmt_big(double v, char *out, int n);
uint64_t plw_hash(void);
#endif
