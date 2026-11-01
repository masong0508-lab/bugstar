// pc_content.c: rule-made content for the PC port (see pc_content.h). Pure functions of the address seed: same address, same result, always.
#include <string.h>
#include <stdio.h>
#include "pc_content.h"

static uint64_t mix64(uint64_t z){ z += 0x9E3779B97F4A7C15ull; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
static uint64_t fold(uint64_t h, const uint32_t *w, int n, uint64_t salt){ for(int i = 0; i < n; i++) h = mix64(h ^ ((uint64_t)w[i] << 7) ^ ((uint64_t)i * 0x100000001B3ull) ^ salt); return h; }
uint64_t uni_seed(const uint32_t *uni){ return fold(0x1234ABCD5678EF01ull, uni, UNI_WORDS, 0x55); }
uint64_t addr_seed(const Addr *a){
    uint64_t h = uni_seed(a->uni);
    h = fold(h, a->cx, AX_WORDS, 0xA1); h = fold(h, a->cy, AX_WORDS, 0xB2);
    return mix64(h ^ ((uint64_t)a->star << 8) ^ a->planet);
}
// a tiny stream of random numbers from one seed
typedef struct { uint64_t s; } Rng;
static uint32_t rnd(Rng *r){ r->s = mix64(r->s); return (uint32_t)(r->s >> 16); }
static uint32_t rndn(Rng *r, uint32_t n){ return rnd(r) % n; }

// ------------------------------------------------------------------ planet traits
// category, name, value (gravity % or day hours), temp change, danger, loot, price, life
typedef struct { uint8_t cat; const char *name; int16_t val, dt; int8_t dg, lt, pr, lf; } Trait;
#define G TC_GRAVITY
#define D TC_DAY
#define A TC_AIR
#define W TC_WEATHER
#define S TC_SURFACE
#define K TC_SKY
#define L TC_LIFE
#define N TC_ANCIENT
#define P TC_POWER
static const Trait tr[] = {
 {G,"FEATHER GRAVITY",40,0,-1,0,0,0},{G,"LIGHT GRAVITY",70,0,0,0,0,0},{G,"EARTHLIKE GRAVITY",100,0,0,0,0,0},{G,"HEAVY GRAVITY",160,0,1,0,0,0},{G,"CRUSHING GRAVITY",280,0,2,1,0,-1},
 {D,"TIDALLY LOCKED",0,0,1,0,0,0},{D,"FAST SPIN",6,0,0,0,0,0},{D,"EVEN DAYS",24,0,0,0,0,1},{D,"SLOW SPIN",300,0,0,0,0,0},{D,"LONG NIGHT",2000,-25,1,1,0,-1},
 {A,"THIN AIR",0,-10,0,0,0,-1},{A,"THICK AIR",0,10,0,0,0,0},{A,"AMBER HAZE",0,0,0,0,0,0},{A,"GLOW AURORA",0,0,0,1,0,0},{A,"ACID CLOUDS",0,5,1,0,0,-1},{A,"FRESH BREEZE",0,0,-1,0,0,1},{A,"DUST VEIL",0,5,0,0,0,0},
 {W,"CLEAR SKIES",0,0,0,0,0,0},{W,"STORMY",0,0,1,0,0,0},{W,"ASH FALL",0,10,1,0,0,-1},{W,"METEOR SHOWERS",0,0,1,2,0,0},{W,"ACID RAIN",0,0,1,0,0,-1},{W,"SNOWFALL",-20,0,0,0,0,0},{W,"SANDSTORMS",5,1,0,0,0,0},{W,"ENDLESS FOG",0,0,0,0,0,0},
 {S,"OCEAN WORLD",0,-5,0,0,0,1},{S,"ARCHIPELAGO",0,0,0,0,0,1},{S,"GLASS DUNES",0,15,0,1,0,0},{S,"CRYSTAL FIELDS",0,0,0,2,1,0},{S,"FUNGUS FORESTS",0,0,0,0,0,2},{S,"LAVA SEAS",0,60,2,1,0,-1},
 {S,"ICE SHEET",0,-50,0,0,0,-1},{S,"CANYON LANDS",0,0,0,1,0,0},{S,"FLOATING ISLES",0,0,0,1,0,0},{S,"SALT FLATS",0,10,0,0,0,-1},{S,"GIANT TREES",0,0,0,0,0,2},{S,"BONE DESERT",0,10,1,1,0,0},
 {K,"RINGS",0,0,0,0,0,0},{K,"MANY MOONS",0,0,0,0,0,0},{K,"TWIN SUNS",0,15,0,0,0,0},{K,"RED GIANT SKY",0,25,1,0,0,0},{K,"DEAD SUN",0,-80,1,1,0,-2},{K,"COMET TRAIL",0,0,0,1,0,0},{K,"NEARBY NEBULA",0,0,0,1,0,0},
 {L,"HIVE WORLD",0,0,2,0,0,2},{L,"GHOST WORLD",0,0,0,1,0,-3},{L,"BUG GARDEN",0,0,-1,0,0,2},{L,"GIANT BUGS",0,0,2,1,0,0},{L,"TINY BUGS",0,0,0,0,0,1},{L,"PREDATOR WORLD",0,0,3,1,0,0},{L,"PEACEFUL WORLD",0,0,-2,0,0,1},{L,"MIGRATING HERDS",0,0,0,0,0,2},
 {N,"RUINS",0,0,0,2,0,0},{N,"ANCIENT MACHINES",0,0,1,3,0,0},{N,"BURIED SHIP",0,0,0,2,1,0},{N,"SINGING STONES",0,0,0,1,0,0},{N,"MONUMENTS",0,0,0,1,1,0},{N,"NOTHING OLD",0,0,0,0,0,0},
 {P,"LEY LINES",0,0,0,1,0,0},{P,"SPELL STORMS",0,0,2,1,0,0},{P,"DEAD MAGIC",0,0,-1,0,0,0},{P,"VOLCANIC",0,20,1,1,0,0},{P,"GEYSERS",0,5,0,0,0,0},{P,"MAGNETIC STORMS",0,0,1,0,0,0},{P,"QUIET GROUND",0,0,0,0,0,0},
};
#undef G
#undef D
#undef A
#undef W
#undef S
#undef K
#undef L
#undef N
#undef P
#define NTR ((int)(sizeof tr / sizeof tr[0]))
_Static_assert(sizeof tr / sizeof tr[0] <= 128, "conf[][] is too small for this many traits");
static const char *const cat_nm[NCAT] = { "GRAVITY", "DAY", "AIR", "WEATHER", "SURFACE", "SKY", "LIFE", "ANCIENT", "POWER" };
int trait_count(void){ return NTR; }
const char *trait_name(int id){ return id >= 0 && id < NTR ? tr[id].name : "?"; }
int trait_cat(int id){ return tr[id].cat; }
const char *cat_name(int c){ return c >= 0 && c < NCAT ? cat_nm[c] : "?"; }

static int tid(const char *n){ for(int i = 0; i < NTR; i++) if(!strcmp(tr[i].name, n)) return i; return -1; }
// impossible pairs (names, so the table reads as sentences)
static const char *const bad[][2] = {
 {"DEAD SUN","TIDALLY LOCKED"},{"DEAD SUN","OCEAN WORLD"},{"DEAD SUN","LAVA SEAS"},{"DEAD SUN","FUNGUS FORESTS"},{"DEAD SUN","GIANT TREES"},{"DEAD SUN","BUG GARDEN"},{"DEAD SUN","MIGRATING HERDS"},{"DEAD SUN","FRESH BREEZE"},{"DEAD SUN","TINY BUGS"},
 {"GHOST WORLD","FUNGUS FORESTS"},{"GHOST WORLD","GIANT TREES"},{"GHOST WORLD","FRESH BREEZE"},
 {"LAVA SEAS","SNOWFALL"},{"LAVA SEAS","FRESH BREEZE"},{"LAVA SEAS","LONG NIGHT"},
 {"ICE SHEET","ASH FALL"},{"ICE SHEET","SANDSTORMS"},{"ICE SHEET","RED GIANT SKY"},{"ICE SHEET","TWIN SUNS"},
 {"OCEAN WORLD","SANDSTORMS"},{"OCEAN WORLD","DUST VEIL"},
 {"THIN AIR","STORMY"},{"THIN AIR","ENDLESS FOG"},{"THIN AIR","THICK AIR"},
 {"CRUSHING GRAVITY","FLOATING ISLES"},{"CRUSHING GRAVITY","GIANT TREES"},{"CRUSHING GRAVITY","GIANT BUGS"},
 {"FEATHER GRAVITY","FRESH BREEZE"},{"FEATHER GRAVITY","STORMY"},
 {"SNOWFALL","RED GIANT SKY"},{"VOLCANIC","SNOWFALL"},{"METEOR SHOWERS","THICK AIR"},
};
static uint8_t conf[128][128]; static int conf_ready;
static void conf_init(void){
    memset(conf, 0, sizeof conf);
    for(unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++){ int a = tid(bad[i][0]), b = tid(bad[i][1]); if(a >= 0 && b >= 0) conf[a][b] = conf[b][a] = 1; }
    conf_ready = 1;
}
int traits_conflict(int a, int b){ if(!conf_ready) conf_init(); return a == b || tr[a].cat == tr[b].cat || conf[a][b]; }

void planet_make(uint64_t seed, PlanetInfo *p){
    Rng r = { seed ^ 0x504C414E45545453ull }; uint8_t order[NCAT]; int want, k;
    memset(p, 0, sizeof *p);
    for(k = 0; k < NCAT; k++) order[k] = k;
    for(k = NCAT - 1; k > 0; k--){ int j = rndn(&r, k + 1); uint8_t t = order[k]; order[k] = order[j]; order[j] = t; }
    want = 3 + (int)rndn(&r, 4);                                           // 3 to 6
    p->gravity_pct = 100; p->day_hours = 24; p->temp_c = 15;
    for(k = 0; k < NCAT && p->n < want; k++){
        int cat = order[k], first = -1, cnt = 0, pick, tries;
        for(int i = 0; i < NTR; i++) if(tr[i].cat == cat){ if(first < 0) first = i; cnt++; }
        for(tries = 0; tries < 8; tries++){
            int id = first + (int)rndn(&r, cnt), ok = 1;
            for(int j = 0; j < p->n; j++) if(traits_conflict(id, p->id[j])) ok = 0;
            if(ok){ pick = id; p->id[p->n++] = (uint8_t)pick;
                if(cat == TC_GRAVITY) p->gravity_pct = tr[pick].val; else if(cat == TC_DAY) p->day_hours = tr[pick].val;
                p->temp_c += tr[pick].dt; p->danger += tr[pick].dg; p->loot += tr[pick].lt; p->price += tr[pick].pr; p->life += tr[pick].lf;
                break; }
        }
    }
    if(p->danger < -3) p->danger = -3;
    if(p->danger > 8) p->danger = 8;
    if(p->life < -3) p->life = -3;
    if(p->life > 6) p->life = 6;
}
void planet_name(uint64_t seed, char *out){
    static const char *const a[] = {"KO","VA","ZE","MI","TOR","SYL","BRA","NU","QUI","DRA","ELO","FAR","GOR","HAL","IX","JUN"};
    static const char *const b[] = {"RA","LU","NE","SA","KO","VI","MON","TAR","LEX","DAN","PHI","ZUN"};
    static const char *const c[] = {"X","N","S","TH","R","M","DE","K"};
    Rng r = { seed ^ 0x4E414D4553ull };
    int n = 2 + (int)rndn(&r, 2);
    out[0] = 0; strcat(out, a[rndn(&r, 16)]); strcat(out, b[rndn(&r, 12)]); if(n == 3) strcat(out, c[rndn(&r, 8)]);
    sprintf(out + strlen(out), "-%d", 1 + (int)rndn(&r, 19));
}

// ------------------------------------------------------------------ universe themes
static const Theme th[] = {
 {"LOW GRAVITY","ALL GRAVITY IS HALF. JUMPS ARE HIGH."},{"HEAVY HOLLOW","GRAVITY IS 1.5X. YOU MOVE SLOWER."},
 {"NO MAGIC","SPELLS DO NOTHING HERE. BITES HIT HARDER."},{"WILD MAGIC","SPELLS COST HALF AND SOMETIMES MISFIRE."},
 {"HUGE BUGS","EVERY MONSTER IS BIGGER AND SLOWER."},{"TINY BUGS","MONSTERS ARE SMALL AND COME IN SWARMS."},
 {"ENDLESS NIGHT","THE SUN NEVER RISES. GLOWING THINGS ARE EVERYWHERE."},{"ENDLESS DAY","NO NIGHT. HEAT IS A DANGER."},
 {"DOUBLE GOLD","GOLD DROPS ARE DOUBLED, AND SO ARE PRICES."},{"THIN PURSES","HALF THE GOLD, HALF THE PRICES."},
 {"GLASS BUGS","EVERYONE HITS TWICE AS HARD BUT HAS HALF THE HEALTH."},{"HUNGER FREE","NOBODY GETS HUNGRY."},
 {"STARVING","FOOD IS SCARCE. EVERYTHING HAS TO BE COOKED."},{"MIRROR WORLD","LEFT AND RIGHT ARE SWAPPED ON MAPS."},
 {"SHORT DAYS","A DAY LASTS FOUR MINUTES."},{"LONG ROADS","DISTANCES ARE DOUBLED. FUEL IS CHEAP."},
 {"SHORT ROADS","DISTANCES ARE HALVED. FUEL IS DEAR."},{"RUINED SKY","OLD WAR WRECKS DRIFT EVERYWHERE. SALVAGE IS RICH."},
 {"QUIET STARS","NO PIRATES AND NO FLEETS. TRADE IS SLOW."},{"PIRATE KINGDOM","PIRATES EVERYWHERE. BOUNTIES PAY DOUBLE."},
 {"LUCKY STARS","RARE DROPS ARE TWICE AS LIKELY."},{"HARD WORLD","ALL FOES HAVE 1.5X HEALTH. XP IS DOUBLED."},
 {"SOFT WORLD","ALL FOES HAVE HALF HEALTH. XP IS HALVED."},{"LADYBUG LANDS","LADYBUG BUDDIES HEAL YOU WHEN THEY BITE."},
};
// themes in the same group cannot be picked together (0 = no group)
static const uint8_t grp[] = {1,1, 2,2, 3,3, 4,4, 5,5, 6, 7,7, 0, 4, 8,8, 0,0,0, 0, 9,9, 0};
int theme_count(void){ return (int)(sizeof th / sizeof th[0]); }
const Theme *theme_get(int id){ return &th[id]; }
void universe_themes(uint64_t useed, int out[2]){
    Rng r = { useed ^ 0x5448454D45ull }; int n = theme_count();
    out[0] = (int)rndn(&r, n);
    for(int t = 0; t < 32; t++){ int b = (int)rndn(&r, n); if(b != out[0] && !(grp[b] && grp[b] == grp[out[0]])){ out[1] = b; return; } }
    out[1] = (out[0] + 5) % n;
}
