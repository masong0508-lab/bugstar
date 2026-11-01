// cr_recipe.h: the PLANET CREATOR's recipe (what you design) and everything that is a pure function of it.
// Plain C, no game code: the creator screen (creator_ui.inc), the tests and the headless tools all use this.
//   Recipe -> cr_seed()   a 64-bit planet seed (the same recipe always gives the same seed)
//   Recipe -> cr_world()  the settings the game's planet generator needs (cr_world is what the game hook copies)
//   Recipe <-> share code (text), Recipe <-> .crp file (text), undo/redo (CrHist), randomise per section.
#ifndef CR_RECIPE_H
#define CR_RECIPE_H
#include <stdint.h>

#define CR_NAME    16          // letters in a planet name
#define CR_NCAT    9           // trait categories (gravity, day, air, weather, surface, sky, life, ancient, power)
#define CR_NONE    0xFFFF      // "no trait in this category"
#define CR_MAXCRE  10          // creature groups you can place
#define CR_MAXPIN  8           // flora you can pin by hand
#define CR_NSWATCH 32          // colour swatches

typedef struct { uint16_t species; uint8_t amount; } CrCre;                  // species = a creature kind (0..12671, see creature_kind in pc_content), amount 1..9: how many (0 = none)
typedef struct { uint32_t id; uint8_t amount; } CrPin;               // a pinned plant (flora id) and how much of it (1..9)

typedef struct {
    char name[CR_NAME + 1];
    // ---- the planet
    uint8_t cls;                 // 0..15: temperate, mountain, meadow, ocean, desert, ice, jungle, death, city, paradise, inferno, brimstone, obsidian, bone, blood sea, frozen hell
    uint8_t size;                // 0..5 tiny .. giant
    uint8_t sea;                 // 0..100 percent of the surface under water
    uint8_t land;                // 0..7 shape of the land (hills, mesas, dunes, islands, craters, canyons, ridges, plains)
    uint8_t rough;               // 0..3
    uint8_t hour;                // 0 day, 1 dusk, 2 night (the light you land in)
    uint8_t weather;             // 0 clear, 1 rain, 2 snow, 3 dust
    uint8_t sky;                 // swatch of the sky
    uint8_t ground;              // swatch of the ground, 0 = the class colour
    int8_t  climate;             // -8..8: five degrees each way
    uint16_t trait[CR_NCAT];     // one trait id per category or CR_NONE
    uint8_t theme[2];            // universe themes (255 = none)
    uint32_t salt;               // roll this to make a different land from the same recipe
    // ---- the people
    uint8_t body, limbs, height; // body shape 0..15, limbs 0..5 (2,4,6,8,10,12), height 0..7
    uint8_t col[3];              // swatches: skin, marks, clothes
    uint8_t gift[3];             // three gifts, 0..23, all different
    uint8_t calling;             // 0..15
    uint8_t cstyle;              // city style 0..15
    uint8_t diet;                // 0..5
    uint8_t temper;              // 0..7
    uint8_t pop;                 // 0..15 how many (1,000 .. 90 billion)
    uint8_t tech;                // 0..7
    // ---- the life
    uint8_t cover;               // 0..100: how much of the land the plants try to cover
    uint8_t div;                 // 0..9: how many kinds of plant (0 .. 400)
    uint8_t ncre; CrCre cre[CR_MAXCRE];
    uint8_t npin; CrPin pin[CR_MAXPIN];
} Recipe;

typedef struct { int gravity_pct, day_hours, temp_c, light, danger, life; } CrEnv;     // what the traits and the class add up to

enum { CRS_WORLD, CRS_TRAITS, CRS_PEOPLE, CRS_LIFE, CRS_NAME, CR_NSECT };

// ---- names of things (all font-safe)
extern const char *const cr_cls_name[16];
extern const char *const cr_size_name[6];
extern const char *const cr_land_name[8];
extern const char *const cr_hour_name[3];
extern const char *const cr_weather_name[4];
extern const char *const cr_body_name[16];
extern const char *const cr_limb_name[6];
extern const char *const cr_gift_name[24];
extern const char *const cr_calling_name[16];
extern const char *const cr_cstyle_name[16];
extern const char *const cr_diet_name[6];
extern const char *const cr_temper_name[8];
extern const char *const cr_tech_name[8];
extern const char *const cr_div_name[10];
uint16_t cr_swatch(int i);                         // RGB555 of swatch i (0..31)
int      cr_height_cm(int h);
double   cr_pop_count(int p);                      // people in a pop step
void cr_people_name(const Recipe *r, char *out);   // what your people are called (made from the recipe)

// ---- the recipe
void cr_default(Recipe *r, uint32_t n);            // a starting recipe (n picks one of several examples)
void cr_fix(Recipe *r);                            // clamp every field and drop clashing traits (call after any edit)
uint64_t cr_seed(const Recipe *r);                 // the planet seed
void cr_env(const Recipe *r, CrEnv *e);            // traits and class added up
void cr_randomize(Recipe *r, int section, uint64_t *rng);   // randomise one section (CRS_*) or all of them (section = -1)
int  cr_trait_count(void);
const char *cr_trait_name(int id);
int  cr_trait_cat(int id);
int  cr_trait_ok(const Recipe *r, int cat, int id);   // 1 when id fits with the other chosen traits
int  cr_theme_count(void);
const char *cr_theme_name(int id);
int  cr_theme_ok(const Recipe *r, int slot, int id);

// ---- what the game's planet generator needs
typedef struct {
    uint8_t cls, psz, style, rough, hour, weather, grav, snow, sand, city, csize, ruins, fauna, flora[4], cliffs, twotone;
    uint8_t water;                      // water_lv
    uint16_t tint[6], sky, bld, road;   // RGB555 ground ramp, sky, buildings, roads
    uint32_t tseed, cseed;
    char name[24];
} CrWorld;
void cr_world(const Recipe *r, CrWorld *w);

// ---- an address for it: a reserved universe whose planet seed IS the recipe seed (see CR_UNI_MARK in pc_content.c)
#define CR_UNI_MARK 0x43524541u            // "CREA"
// (cr_addr() is in cr_addr.c because it needs pc_content.h)

// ---- share code and files
int  cr_encode(const Recipe *r, char *out, int cap);       // "BG1-XXXXX-XXXXX-..." returns its length, or 0 when it does not fit
int  cr_decode(const char *code, Recipe *r);                // 1 = ok, 0 = not a code (wrong checksum, bad letters)
int  cr_save(const Recipe *r, const char *path);            // text file, one "key value" per line; 1 = ok
int  cr_load(Recipe *r, const char *path);                  // 1 = ok
int  cr_equal(const Recipe *a, const Recipe *b);

// ---- undo and redo
#define CR_HIST 64
typedef struct { Recipe snap[CR_HIST]; int n, at; } CrHist;     // snap[at] is the present
void cr_hist_init(CrHist *h, const Recipe *r);
void cr_hist_push(CrHist *h, const Recipe *r);                  // after a change (does nothing when the recipe did not change)
int  cr_hist_undo(CrHist *h, Recipe *r);                        // 1 when it went back
int  cr_hist_redo(CrHist *h, Recipe *r);
#endif
