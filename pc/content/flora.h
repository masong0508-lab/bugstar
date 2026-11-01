// flora.h: the FLORA catalogue of the PC port. Nothing is stored: a plant is worked out from a number (its id) when asked.
//   plant id (29 bits) = colour word (6 bits) + growth form (5) + given name (18 bits = three 2-letter syllables)
//   so EVERY id has its own name, and no two ids share one: 536,870,912 named plants.
//   the rest of a plant (leaf, flower, fruit, bark, habitat, size, use, quirk) is a hash of its id, so a plant is the same plant on every planet.
//   a planet picks its own plants from the catalogue with flora_for_planet(): same seed and same climate, same plants.
// Text is font-safe for the game: A-Z 0-9 space . , ' : ( ) -
// Kinds of plant by traits: FL_KINDS = form 32 x leaf 16 x flower 16 x fruit 16 x bark 12 x colour 16 x habitat 14 x size 6 x use 10 x quirk 16.
#ifndef PC_FLORA_H
#define PC_FLORA_H
#include <stdint.h>

#define FL_ID_BITS 29
#define FL_TOTAL   (1u << FL_ID_BITS)          // named plants
enum { FL_NFORM = 32, FL_NLEAF = 16, FL_NFLOWER = 16, FL_NFRUIT = 16, FL_NBARK = 12, FL_NCOLOUR = 16, FL_NHAB = 14, FL_NSIZE = 6, FL_NUSE = 10, FL_NQUIRK = 16 };
#define FL_KINDS ((uint64_t)FL_NFORM * FL_NLEAF * FL_NFLOWER * FL_NFRUIT * FL_NBARK * FL_NCOLOUR * FL_NHAB * FL_NSIZE * FL_NUSE * FL_NQUIRK)

enum { FU_FOOD, FU_MEDICINE, FU_FUEL, FU_TIMBER, FU_FIBRE, FU_DYE, FU_SPICE, FU_AIR, FU_SOIL, FU_ORNAMENT };
enum { FH_MEADOW, FH_FOREST, FH_MARSH, FH_SHORE, FH_DUNE, FH_ROCK, FH_ICE, FH_CANYON, FH_ASH, FH_CAVE, FH_SLOPE, FH_TIDAL, FH_RAIN, FH_VOLCANIC };

typedef struct {
    uint32_t id;                                  // 0 .. FL_TOTAL-1
    uint8_t  form, leaf, flower, fruit, bark, colour, habitat, size, use, quirk, word;   // word: which of the 4 colour words (0..3)
    uint16_t leaf_rgb, flower_rgb;                // RGB555, ready for the game's palette
    uint16_t height_cm;
    uint16_t biomass;                             // tenths of kg per square metre when it covers the ground
    uint8_t  o2, food, shelter, medicine, fuel;   // 0..15 each: how much it gives
    uint8_t  woody, aquatic;                      // 1 = a tree or a bush with wood; 1 = lives in water
    char     name[40];
} Flora;

typedef struct { int temp_c, wet, sea, rough, light; } FloraEnv;     // a planet's climate: degrees C, wetness 0..100, sea % 0..100, roughness 0..3, light % (100 = Earth)

void flora_make(uint32_t id, Flora *f);                              // the plant with this id (id is taken modulo FL_TOTAL)
void flora_about(const Flora *f, char *out, int cap);                // a short text about it (font-safe, no line breaks)
uint32_t flora_catalogue(uint64_t seed, uint32_t n);                 // the n-th plant of a catalogue seed (a bijection: no plant twice in one catalogue)
uint32_t flora_find(const char *name);                               // the id of a plant by its name, or 0xFFFFFFFF (works for any name this catalogue can make)
uint32_t flora_habitats(const FloraEnv *e);                          // bit mask of the habitats this climate has (bit = FH_*)
int  flora_for_planet(uint64_t seed, const FloraEnv *e, int want, Flora *out);    // up to `want` plants that fit this climate, always the same for the same seed
const char *flora_form_name(int i);
const char *flora_use_name(int i);
const char *flora_habitat_name(int i);
const char *flora_size_name(int i);
const char *flora_colour_name(int i);
uint16_t flora_colour_rgb(int i);
#endif
