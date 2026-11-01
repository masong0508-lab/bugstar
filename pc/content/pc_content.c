// pc_content.c: rule-made content for the PC port (see pc_content.h). Pure functions of the address seed: same address, same result, always.
#include <string.h>
#include <stdio.h>
#include "pc_content.h"
#include <stdlib.h>

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
// the 15 flavours of each category: flavour 0 is the plain classic trait, flavours 1..15 put one of these words in front ("SALTY LAVA SEAS", "HUMMING MONUMENTS").
static const char *const flav[NCAT][15] = {
 {"STEADY","SWAYING","SPRINGY","PATCHY","RISING","SINKING","TIDAL","GENTLE","STUBBORN","PULSING","WARPED","LOPSIDED","HUMMING","DRIFTING","SLOW-WAVE"},                     // GRAVITY
 {"WOBBLY","LAZY","RAGGED","BRIGHT","DIM","COPPER","PALE","SHARP","LONG-DUSK","SHORT-DAWN","DOUBLE","STUTTERING","GOLDEN","STEEL","QUIET"},                          // DAY
 {"SWEET","SOUR","SPICY","SMOKY","CHILLY","WARM","SILKEN","RUSTY","MINTY","SALTY","HUMMING","SPARKLING","MUSKY","STILL","RESIN-SCENTED"},                           // AIR
 {"GENTLE","VIOLENT","SEASONAL","SUDDEN","PATCHY","SILVER","RED","LATE-DAY","DAWN","NIGHT-ONLY","TWIN-CELL","RARE","DAILY","ROLLING","HEAVY-LAID"},                  // WEATHER
 {"PURPLE","SALTY","MOSSY","SHINING","JAGGED","HOLLOW","TERRACED","SCARRED","MISTY","EMERALD","CHALKY","STRIPED","CRACKED","VELVET","SMOOTH"},                      // SURFACE
 {"PINK","SILVER","BLUE","DISTANT","LOW","BRIGHT","FADED","DOUBLE","SLOW","TILTED","VEILED","HUGE","SMALL","RUSTY","PEARL"},                                         // SKY
 {"SHY","BOLD","NOISY","SLEEPY","STRIPED","GLOWING","SWIFT","STUBBORN","TAME","WILD","SINGING","SECRET","GENTLE","CLEVER","FURRY"},                                  // LIFE
 {"BROKEN","BURIED","GLASSY","MOSS-GROWN","SUNKEN","HUMMING","SEALED","SCORCHED","TOWERING","WHISPERING","TILTED","GILDED","HOLLOW","FORGOTTEN","TINY"},             // ANCIENT
 {"FAINT","STRONG","FLICKERING","SLOW","WANDERING","COLD","WARM","SINGING","TANGLED","BRIGHT","SEASONAL","SECRET","STEADY","DEEP","RESTLESS"},                      // POWER
};
_Static_assert(NTR == TR_BASE, "TR_BASE must equal the number of classic traits");
#define NTRV (TR_BASE * TR_VARIANTS)
static const char *const cat_nm[NCAT] = { "GRAVITY", "DAY", "AIR", "WEATHER", "SURFACE", "SKY", "LIFE", "ANCIENT", "POWER" };
int trait_count(void){ return NTRV; }
int trait_base_count(void){ return NTR; }
int trait_base(int id){ return id % TR_BASE; }
int trait_variant(int id){ return id / TR_BASE; }
int trait_cat(int id){ return tr[id % TR_BASE].cat; }
const char *cat_name(int c){ return c >= 0 && c < NCAT ? cat_nm[c] : "?"; }
void trait_name_to(int id, char *out, int n){
    if(id < 0 || id >= NTRV){ snprintf(out, n, "?"); return; }
    int b = id % TR_BASE, v = id / TR_BASE;
    if(v) snprintf(out, n, "%s %s", flav[tr[b].cat][v - 1], tr[b].name); else snprintf(out, n, "%s", tr[b].name);
}
static char ring[16][96]; static int ring_i;
static char *ring_next(void){ ring_i = (ring_i + 1) & 15; return ring[ring_i]; }
const char *trait_name(int id){ char *o = ring_next(); trait_name_to(id, o, 96); return o; }

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
// what each classic trait does to the chances of a kind of native creature (class 0 beasts, 1 fliers, 2 swimmers, 3 crawlers, 4 spirits, 5 monsters) and to the daylight
static const struct { const char *n; uint8_t cls; int8_t w; } boost[] = {
 {"OCEAN WORLD",2,4},{"ARCHIPELAGO",2,3},{"ICE SHEET",2,1},{"BUG GARDEN",3,4},{"HIVE WORLD",3,4},{"GIANT BUGS",3,3},{"TINY BUGS",3,3},{"FUNGUS FORESTS",3,2},
 {"FLOATING ISLES",1,4},{"FRESH BREEZE",1,2},{"GLOW AURORA",1,1},{"THIN AIR",1,1},{"GHOST WORLD",4,5},{"RUINS",4,1},{"SINGING STONES",4,2},{"SPELL STORMS",4,2},{"LEY LINES",4,2},{"NEARBY NEBULA",4,1},
 {"PREDATOR WORLD",5,4},{"VOLCANIC",5,2},{"LAVA SEAS",5,2},{"ANCIENT MACHINES",5,2},{"CRUSHING GRAVITY",5,1},{"METEOR SHOWERS",5,1},{"DEAD SUN",4,2},
 {"MIGRATING HERDS",0,4},{"PEACEFUL WORLD",0,2},{"GIANT TREES",0,2},{"EVEN DAYS",0,1},{"CANYON LANDS",0,1},{"SALT FLATS",0,1},
};
static const struct { const char *n; int8_t d; } lightmod[] = {
 {"LONG NIGHT",-35},{"DEAD SUN",-45},{"TWIN SUNS",25},{"RED GIANT SKY",15},{"AMBER HAZE",-10},{"DUST VEIL",-10},{"ENDLESS FOG",-15},{"STORMY",-10},{"GLOW AURORA",5},
 {"NEARBY NEBULA",5},{"CLEAR SKIES",10},{"ASH FALL",-15},{"SNOWFALL",-5},{"ACID CLOUDS",-10},{"THICK AIR",-5},{"THIN AIR",5},{"RINGS",-3},{"LAVA SEAS",8},
};
static uint8_t conf[128][128]; static int8_t cboost[TR_BASE][6], lmod[TR_BASE]; static int conf_ready;
static void conf_init(void){
    memset(conf, 0, sizeof conf); memset(cboost, 0, sizeof cboost); memset(lmod, 0, sizeof lmod);
    for(unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++){ int a = tid(bad[i][0]), b = tid(bad[i][1]); if(a >= 0 && b >= 0) conf[a][b] = conf[b][a] = 1; }
    for(unsigned i = 0; i < sizeof boost / sizeof boost[0]; i++){ int a = tid(boost[i].n); if(a >= 0) cboost[a][boost[i].cls] = boost[i].w; }
    for(unsigned i = 0; i < sizeof lightmod / sizeof lightmod[0]; i++){ int a = tid(lightmod[i].n); if(a >= 0) lmod[a] = lightmod[i].d; }
    conf_ready = 1;
}
int traits_conflict(int a, int b){ if(!conf_ready) conf_init(); a %= TR_BASE; b %= TR_BASE; return a == b || tr[a].cat == tr[b].cat || conf[a][b]; }

// ------------------------------------------------------------------ stats: 7 families x 16 zones
static const char *const fam_nm[N_FAM] = {"GRAVITY","TEMPERATURE","DAYLIGHT","DANGER","LOOT","PRICES","LIFE"};
static const char *const fam_un[N_FAM] = {"PCT","C","PCT","","","",""};
static const char *const zone_nm[N_ZONE] = {"LOWLANDS","HIGHLANDS","SHORES","OPEN SEA","DEEP SEA","ICE CAPS","DESERTS","FORESTS","CITIES","RUINS","CAVES","SWAMPS","PLAINS","VOLCANOES","THE SKY","ORBIT"};
// how each zone shifts each family: gravity (percent points), temperature (C), daylight (percent points), danger, loot, prices, life
static const int8_t zmod[N_ZONE][N_FAM] = {
 {0, 2, 0, 0, 0, 0, 1},   {-4, -8, 5, 1, 1, 0, -1},  {0, -2, 0, 0, 1, 0, 1},  {0, -4, 5, 1, 0, 0, 1},
 {2, -12, -40, 2, 1, 0, 0}, {1, -35, -10, 0, 0, 1, -2}, {0, 15, 10, 1, 0, 1, -2}, {0, -2, -15, 0, 0, 0, 2},
 {0, 3, 0, -1, 0, 0, 0},  {0, 0, -5, 1, 2, 1, -1},   {0, -6, -100, 1, 1, 0, 0}, {0, 3, -20, 1, 0, 0, 2},
 {0, 0, 5, 0, 0, 0, 1},   {1, 40, -10, 2, 1, 0, -2}, {-3, -10, 20, 1, 0, 0, 0}, {-90, -60, 30, 1, 0, 1, -3},
};
int stat_count(void){ return N_STAT; }
const char *fam_name(int f){ return f >= 0 && f < N_FAM ? fam_nm[f] : "?"; }
const char *fam_unit(int f){ return f >= 0 && f < N_FAM ? fam_un[f] : ""; }
const char *zone_name(int z){ return z >= 0 && z < N_ZONE ? zone_nm[z] : "?"; }
const char *stat_name(int s){ char *o = ring_next(); if(s < 0 || s >= N_STAT) return "?"; snprintf(o, 96, "%s IN %s", fam_nm[s / N_ZONE], zone_nm[s % N_ZONE]); return o; }
static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }
static void make_stats(PlanetInfo *p, Rng *r){
    static const int unit[N_FAM] = {5, 3, 4, 1, 1, 1, 1}, jit[N_FAM] = {2, 2, 3, 1, 1, 1, 1};
    int base[N_FAM], lt = 50;
    for(int k = 0; k < p->n; k++) lt += lmod[p->id[k] % TR_BASE];
    base[0] = p->gravity_pct; base[1] = p->temp_c; base[2] = clampi(lt, 0, 100); base[3] = p->danger; base[4] = p->loot; base[5] = p->price; base[6] = p->life;
    for(int f = 0; f < N_FAM; f++) for(int z = 0; z < N_ZONE; z++){
        int j = (int)rndn(r, 3) - 1;                                           // a little local weather in every number
        p->stat[f * N_ZONE + z] = (int16_t)(base[f] + zmod[z][f] + j * jit[f]);
    }
    for(int k = 0; k < p->n; k++){                                             // a flavour pushes one stat in one zone
        int v = p->id[k] / TR_BASE, c = tr[p->id[k] % TR_BASE].cat; if(!v) continue;
        int f = (v + c) % N_FAM, z = (v * 3 + c) & 15, up = ((v ^ c) & 1) ? 1 : -1;
        p->stat[f * N_ZONE + z] += (int16_t)(up * unit[f]);
    }
    for(int z = 0; z < N_ZONE; z++){
        int16_t *g = &p->stat[0 * N_ZONE + z]; if(*g < 0) *g = 0;
        int16_t *l = &p->stat[2 * N_ZONE + z]; *l = (int16_t)clampi(*l, 0, 100);
        for(int f = 3; f < N_FAM; f++){ int16_t *s = &p->stat[f * N_ZONE + z]; *s = (int16_t)clampi(*s, f == 3 ? -5 : -3, f == 3 ? 12 : 9); }
    }
}

// ------------------------------------------------------------------ planets
static void make_natives(PlanetInfo *p, Rng *r);
void planet_make(uint64_t seed, PlanetInfo *p){
    Rng r = { seed ^ 0x504C414E45545453ull }; uint8_t order[NCAT]; int want, k;
    if(!conf_ready) conf_init();
    memset(p, 0, sizeof *p);
    for(k = 0; k < NCAT; k++) order[k] = k;
    for(k = NCAT - 1; k > 0; k--){ int j = rndn(&r, k + 1); uint8_t t = order[k]; order[k] = order[j]; order[j] = t; }
    want = 3 + (int)rndn(&r, 4);                                           // 3 to 6
    p->gravity_pct = 100; p->day_hours = 24; p->temp_c = 15;
    for(k = 0; k < NCAT && p->n < want; k++){
        int cat = order[k], first = -1, cnt = 0, tries;
        for(int i = 0; i < NTR; i++) if(tr[i].cat == cat){ if(first < 0) first = i; cnt++; }
        for(tries = 0; tries < 8; tries++){
            int b = first + (int)rndn(&r, cnt), ok = 1, v = (int)rndn(&r, TR_VARIANTS), id = v * TR_BASE + b;
            for(int j = 0; j < p->n; j++) if(traits_conflict(b, p->id[j])) ok = 0;
            if(ok){ p->id[p->n++] = (uint16_t)id;
                if(cat == TC_GRAVITY) p->gravity_pct = tr[b].val; else if(cat == TC_DAY) p->day_hours = tr[b].val;
                p->temp_c += tr[b].dt; p->danger += tr[b].dg; p->loot += tr[b].lt; p->price += tr[b].pr; p->life += tr[b].lf;
                break; }
        }
    }
    if(p->danger < -3) p->danger = -3;
    if(p->danger > 8) p->danger = 8;
    if(p->life < -3) p->life = -3;
    if(p->life > 6) p->life = 6;
    make_stats(p, &r);
    make_natives(p, &r);
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
int theme_pair_ok(int a, int b){ return a != b && !(grp[a] && grp[a] == grp[b]); }
void universe_themes(uint64_t useed, int out[2]){
    Rng r = { useed ^ 0x5448454D45ull }; int n = theme_count();
    out[0] = (int)rndn(&r, n);
    for(int t = 0; t < 32; t++){ int b = (int)rndn(&r, n); if(theme_pair_ok(b, out[0])){ out[1] = b; return; } }
    out[1] = (out[0] + 5) % n;
}

#include "pc_beings.inc"
