// pc_content.h: rule-made content for the PC port (planet traits, stats, universe themes, peoples, cities, creatures, names).
// Everything comes from an address or a seed: nothing is stored, every name is built on demand from word lists (parts x parts).
// The address is 2048 bits for the universe and 1240 bits per axis (x and y): UNI_WORDS and AX_WORDS 32-bit words.
#ifndef PC_CONTENT_H
#define PC_CONTENT_H
#include <stdint.h>
#define UNI_WORDS 64            // 2048 bits
#define AX_WORDS  39            // 1240 bits (38.75 words, so 39)
typedef struct { uint32_t uni[UNI_WORDS]; uint32_t cx[AX_WORDS], cy[AX_WORDS]; uint8_t star, planet; } Addr;

uint64_t addr_seed(const Addr *a);                 // hashes EVERY word of the address (the old GBA addresses sit in the low words)
uint64_t uni_seed(const uint32_t *uni);            // the universe number alone

// ---- planet traits. 65 classic traits (TR_BASE) x 16 flavours (TR_VARIANTS) = 1,040 traits.
// trait id = flavour * 65 + classic id, so ids 0..64 are the plain classic traits (flavour 0) and keep their old numbers.
// A planet has 3 to 6 traits, at most one per category, and no impossible pair (the rules are on the classic traits, so a flavour never breaks them).
enum { TC_GRAVITY, TC_DAY, TC_AIR, TC_WEATHER, TC_SURFACE, TC_SKY, TC_LIFE, TC_ANCIENT, TC_POWER, NCAT };
#define TR_BASE      65
#define TR_VARIANTS  16
#define MAX_TR       6
// ---- stats: 7 families x 16 zones = 112 numbers for every planet
#define N_FAM   7         // GRAVITY (percent of earth), TEMPERATURE (C), DAYLIGHT (percent of the day), DANGER, LOOT, PRICES, LIFE
#define N_ZONE  16        // lowlands, highlands, shores, open sea, deep sea, ice caps, deserts, forests, cities, ruins, caves, swamps, plains, volcanoes, sky, orbit
#define N_STAT  (N_FAM * N_ZONE)
#define MAX_NATIVE 6
#define MAX_PEOPLE 3
typedef struct {
    uint8_t n; uint16_t id[MAX_TR];                                  // the traits (ids 0..1039)
    int16_t gravity_pct, day_hours, temp_c; int8_t danger, loot, price, life;     // the 7 whole-planet numbers (day_hours is the day length)
    int16_t stat[N_STAT];                                            // stat[family * 16 + zone]
    uint8_t nbeing, npeople; uint16_t being[MAX_NATIVE], people[MAX_PEOPLE]; int16_t city;   // native creature kinds, peoples, a city kind (-1 = none)
    uint16_t legend_kind; uint8_t legend_gift, legend_colour, legend_rank, legend_mut;       // one famous native creature (an individual)
} PlanetInfo;
int  trait_count(void);                            // 1040
int  trait_base_count(void);                       // 65
int  trait_base(int id);                           // the classic trait behind an id
int  trait_variant(int id);                        // 0..15 (0 = plain)
const char *trait_name(int id);                    // built on demand into one of 16 rotating buffers: copy it if you keep it. Use trait_name_to for your own buffer
void trait_name_to(int id, char *out, int n);
int  trait_cat(int id);
void trait_num(int id, int *val, int *dt, int *dg, int *lt, int *pr, int *lf, int *light, int *cboost6);   // the numbers behind a trait
int  trait_look_only(int id);                      // 1 when the trait changes no number (COSMETIC in the meaning layer)
const char *cat_name(int c);
int  traits_conflict(int a, int b);                // 1 when two traits cannot be on one planet (same category, same classic trait, or a forbidden pair)
void planet_make(uint64_t seed, PlanetInfo *p);
void planet_from_traits(const uint16_t *ids, int n, PlanetInfo *p);   // the same sums as planet_make, for traits you picked yourself (the planet creator)
void planet_name(uint64_t seed, char *out);        // e.g. "KORAX-7"
int  stat_count(void);                             // 112
const char *stat_name(int s);                      // e.g. "DANGER IN RUINS" (rotating buffers like trait_name)
const char *fam_name(int f);
const char *fam_unit(int f);                       // "PCT", "C", "PCT", "", "", "", ""
const char *zone_name(int z);

// ---- universe themes: each universe has 2 rule changes
typedef struct { const char *name, *rule; } Theme;
int  theme_count(void);
const Theme *theme_get(int id);
void universe_themes(uint64_t useed, int out[2]);
int  theme_pair_ok(int a, int b);                  // 1 when two different themes can share a universe

// ---- peoples: 32 stocks x 16 ways of life = 512 kinds of people
#define N_STOCK 32
#define N_WAY   16
#define N_PEOPLE (N_STOCK * N_WAY)
typedef struct { char name[56]; char about1[72], about2[72]; int8_t might, wit, charm, craft, temper; int16_t lifespan; } People;
int  people_count(void);
void people_get(int id, People *out);              // id = way * 32 + stock

// ---- city kinds: 16 forms x 16 characters = 256 kinds of city
#define N_FORM  16
#define N_CHAR  16
#define N_CITY  (N_FORM * N_CHAR)
typedef struct { char name[56]; char about1[72], about2[72]; int16_t pop, walls, trade, wonder; } CityKind;
int  city_count(void);
void city_get(int id, CityKind *out);              // id = character * 16 + form

// ---- creatures: 66 body plans x 192 variants (size 4 x diet 3 x habitat 4 x behaviour 4) = 12,672 kinds,
// and every kind comes in 16 gifts x 16 colourings x 8 ages/ranks x 16 mutations = 32,768 individuals (415,236,096 in all).
#define N_PLAN    66
#define N_VARIANT 192
#define N_KIND    (N_PLAN * N_VARIANT)
#define N_GIFT 16
#define N_COLOUR 16
#define N_RANK 8
#define N_MUT 16
#define N_INDIV_PER_KIND (N_GIFT * N_COLOUR * N_RANK * N_MUT)
#define N_CSTAT 7          // health, speed, bite, guard, wit, taming, lifespan
typedef struct { char name[72]; char about1[72], about2[72]; int16_t stat[N_CSTAT]; uint8_t plan, cls, size, diet, habitat, behav, rarity; } Creature;
typedef struct { uint16_t kind; uint8_t gift, colour, rank, mut; } Indiv;
typedef struct { char name[160]; char about1[72], about2[72], about3[72]; int16_t stat[N_CSTAT]; uint8_t rarity; } CreatureInd;
int  creature_plan_count(void);                    // 66
int  creature_kind_count(void);                    // 12672
const char *creature_stat_name(int s);
const char *creature_class_name(int c);            // 6 classes
const char *rarity_name(int r);                    // COMMON .. MYTHIC (6)
void creature_kind(int id, Creature *out);         // id = variant * 66 + plan
void creature_indiv(const Indiv *iv, CreatureInd *out);
void indiv_from_seed(uint64_t seed, Indiv *iv);    // any individual from a seed
// ---- created planets (pc/creator): an address whose universe words 0..3 are the marker CREATOR_MARK, a check word and the recipe seed is a planet made by hand.
// addr_seed() returns that recipe seed itself, so the game lands on exactly the planet the creator showed.
#define CREATOR_MARK 0x43524541u
#endif
