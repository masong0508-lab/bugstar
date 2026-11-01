// pc_content.h: rule-made content for the PC port (planet traits, universe themes, names). Everything comes from an address, nothing is stored.
// The address is 2048 bits for the universe and 1240 bits per axis (x and y): UNI_WORDS and AX_WORDS 32-bit words.
#ifndef PC_CONTENT_H
#define PC_CONTENT_H
#include <stdint.h>
#define UNI_WORDS 64            // 2048 bits
#define AX_WORDS  39            // 1240 bits (38.75 words, so 39)
typedef struct { uint32_t uni[UNI_WORDS]; uint32_t cx[AX_WORDS], cy[AX_WORDS]; uint8_t star, planet; } Addr;

uint64_t addr_seed(const Addr *a);                 // hashes EVERY word of the address (the old GBA addresses sit in the low words)
uint64_t uni_seed(const uint32_t *uni);            // the universe number alone

// ---- planet traits: 3 to 6 per planet, at most one per category, no impossible pairs
enum { TC_GRAVITY, TC_DAY, TC_AIR, TC_WEATHER, TC_SURFACE, TC_SKY, TC_LIFE, TC_ANCIENT, TC_POWER, NCAT };
#define MAX_TR 6
typedef struct { uint8_t n, id[MAX_TR]; int16_t gravity_pct, day_hours, temp_c; int8_t danger, loot, price, life; } PlanetInfo;
int  trait_count(void);
const char *trait_name(int id);
int  trait_cat(int id);
const char *cat_name(int c);
int  traits_conflict(int a, int b);                // 1 when two traits cannot be on one planet
void planet_make(uint64_t seed, PlanetInfo *p);
void planet_name(uint64_t seed, char *out);        // e.g. "KORAX-7"

// ---- universe themes: each universe has 2 rule changes
typedef struct { const char *name, *rule; } Theme;
int  theme_count(void);
const Theme *theme_get(int id);
void universe_themes(uint64_t useed, int out[2]);
#endif
