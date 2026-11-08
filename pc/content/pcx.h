// pcx.h: a flat, plain-types face for pc_content (the game's own translation unit has names like trait_name, so it must not include pc_content.h).
// The game gives its address; this gives back finished pages of text (what the planet you stand on is made of), already worded and cut to fit.
#ifndef PCX_H
#define PCX_H
#include <stdint.h>
#define PCX_LINES 18          // lines per page
#define PCX_PAGES 10
#define PCX_W     64          // letters per line, with the end mark
enum { PX_WHITE, PX_GOLD, PX_GREY, PX_CYAN, PX_GREEN, PX_RED };       // line colours (the game maps them to its own)
typedef struct { char title[24]; int n; char t[PCX_LINES][PCX_W]; uint8_t col[PCX_LINES]; } PcxPage;
typedef struct { char planet[32]; int npages; PcxPage page[PCX_PAGES]; uint64_t seed; } PcxWorld;
// Extra address bits ABOVE what the game's own address holds (it has 256 bits of universe and 155 per axis; the PC content library reads 2048 and 1240).
// All zero in normal play. The seed viewer can set them. uni_hi: words 8..63 (56 words), ax_hi: words 5..38 (34 words) for each axis.
#define PCX_UNI_HI 56
#define PCX_AX_HI  34
void pcx_hi_set(const uint32_t *uni_hi, const uint32_t *cx_hi, const uint32_t *cy_hi);   // NULL = leave that part as it is
void pcx_hi_clear(void);
// Folds the extra bits into the game's own words (a hash), so two addresses that differ only high up still make different game worlds. Nothing changes when the extra bits are zero.
void pcx_fold(uint32_t *uni8, uint32_t *cx5, uint32_t *cy5);
// Makes the pages for the place at: universe (8 words), cluster x and y (5 words each), star 0..31, planet 0..15, moon 0..7.
// applied = 1 when the game really uses the traits for this world (gravity, daylight, weather: see pcx_effects), 0 for the story worlds (traits are lore only).
void pcx_world(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, int applied, PcxWorld *out);
// What the traits change in the game's own world: out[0] gravity step 0..3 (0.35 / 0.70 / 1.05 / 1.40 g), out[1] daylight 0 day, 1 dusk, 2 night (-1 = leave as it is),
// out[2] weather 0 clear, 1 rain, 2 snow, 3 dust, 4 embers, 5 ash (-1 = leave as it is).
void pcx_effects(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, int out[3]);
// ---- the MEANING LAYER (pc/meaning): what the planet's whole make-up adds up to, and what the game does with it (endless-galaxy planets only)
typedef struct { int hunger_pct, gold_pct, fauna_add, beetle_add, speed_add, hour, weather, node[4], sky_r, sky_g, sky_b, lift; int lvl[32]; } PcxMeaning;
void pcx_meaning(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, PcxMeaning *out);
void pcx_law_clear(void);                 // the edited universe laws count as items in the profile: clear, then set each edited law (id, value)
void pcx_law_set(int id, int value);
void pcx_means_line(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, char *out, int n);   // one short line: "MEANS: WARM AND LOUD."
void pcx_env(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, int *day_hours, int *temp_c);   // the planet's day length (hours, 0 = tidally locked) and warmth
void pcx_gods(int n);                     // how many god powers are open (they count as items too)
void pcx_counts(int *traits, int *stats, int *peoples, int *cities, int *creature_kinds, int *themes);
// ---- what a planet LOOKS like (pc_sprgen): the ids of the content that stands on it
typedef struct { uint32_t flora[4]; int nflora; uint16_t being[8]; int nbeing; uint16_t people[3]; int npeople; int city; int temp_c, wet, sea, danger;
                 uint8_t legend_gift, legend_colour, legend_rank, legend_mut; uint16_t legend_kind; uint64_t seed; } PcxVis;
void pcx_vis(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, PcxVis *out);
void pcx_vis_seed(uint64_t seed, int temp_c, int wet, int sea, PcxVis *out);      // a planet that has no address (the story worlds): the same things from a seed and a climate
#endif
