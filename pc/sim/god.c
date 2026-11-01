// god.c: see god.h
#include "god.h"
#include <string.h>

#define G(nm, ln, e, a, c) { nm, ln, GE_##e, a, c }
static const GodPower T[] = {
    G("BLESS A FIELD",       "THE SOIL OF ONE FIELD GETS RICHER",                 FERTILE,  12, 4),
    G("CALM A STORM",        "THE STORM STOPS AND THE SKY CLEARS",                CLEAR,     0, 4),
    G("CALL RAIN",           "A SOFT RAIN FALLS FOR A WHILE",                     RAIN,      0, 4),
    G("SPARK AN IDEA",       "SOMEONE HAS A GOOD NEW IDEA",                       IDEA,      1, 5),
    G("RAISE A HILL",        "THE GROUND RISES INTO A SHELTERING HILL",           HILL,      1, 6),
    G("PLANT A FOREST",      "TREES GROW AND THE SOIL IMPROVES",                  FOREST,    1, 6),
    G("SEED A SPECIES",      "A NEW KIND OF CREATURE APPEARS",                    SPECIES,   1, 8),
    G("FEED THE HUNGRY",     "EVERYONE FEELS WELL FED",                           FEED,     70, 5),
    G("LET THEM REST",       "EVERYONE FEELS RESTED",                             REST,     70, 5),
    G("STIR A FRIENDSHIP",   "PEOPLE WANT TO TALK TO EACH OTHER",                 SOCIAL,   70, 5),
    G("TELL A JOKE",         "EVERYONE LAUGHS AT THE SAME TIME",                  FUN,      70, 5),
    G("FILL A WELL",         "THE WELLS FILL WITH CLEAN WATER",                   WATER,    10, 5),
    G("MEND A ROOF",         "ROOFS AND WALLS BECOME SOUND AGAIN",                SHELTER,   8, 5),
    G("LIGHT A HEARTH",      "A WARM FIRE MAKES HOMES COZY",                      COMFORT,   8, 5),
    G("HEAL A SPRAIN",       "ACHES AND SPRAINS FADE AWAY",                       HEALTH,   10, 5),
    G("LEND SOME LUCK",      "LUCK TURNS A LITTLE IN YOUR FAVOUR",                LUCK,      6, 6),
    G("TEACH A KNACK",       "SOMEONE LEARNS A USEFUL TRICK",                     SKILL,     1, 6),
    G("SHOO THE PESTS",      "SLUGS AND BEETLES GO AWAY FOR NOW",                 PESTS,    80, 5),
    G("CALL THE BIRDS",      "BIRDS COME AND SING IN THE TREES",                  BIRDS,     1, 5),
    G("GROW A BERRY BUSH",   "A BUSH OF SWEET BERRIES SPROUTS",                   FOOD,     20, 5),
    G("PART THE CLOUDS",     "THE CLOUDS OPEN AND THE SUN SHINES",                CLEAR,     0, 5),
    G("WAKE A SPRING",       "WATER BUBBLES UP FROM THE GROUND",                  WATER,    14, 6),
    G("DEEPEN THE SOIL",     "THE EARTH BECOMES DEEP AND DARK",                   FERTILE,  18, 7),
    G("DRAW A RIVER",        "A RIVER FINDS ITS WAY ACROSS THE LAND",             WATER,    20, 8),
    G("SING AT DAWN",        "A SONG IN THE AIR LIFTS EVERY MOOD",                MOOD,      8, 5),
    G("BRING A HERD",        "A HERD OF GRAZERS WALKS IN FROM THE HILLS",         SPECIES,   1, 9),
    G("GROW A GARDEN",       "FLOWERS BLOOM AND BEES GET BUSY",                   FOOD,     35, 7),
    G("SOFTEN THE WIND",     "THE WIND DROPS TO A GENTLE BREEZE",                 MOOD,     10, 6),
    G("BRIGHTEN THE STARS",  "THE NIGHT SKY SHINES. NIGHTS FEEL SAFER",           MOOD,     12, 7),
    G("SHARE A DREAM",       "THE SAME KIND DREAM VISITS EVERYONE",               MOOD,     16, 8),
    G("BUILD A BRIDGE",      "A BRIDGE JOINS TWO SIDES OF THE LAND",              SHELTER,  14, 8),
    G("SHAPE A VALLEY",      "A VALLEY OPENS BETWEEN TWO HILLS",                  HILL,      2, 9),
    G("FIND A SEAM OF ORE",  "A RICH SEAM OF ORE SHOWS IN THE ROCK",              ORE,      12, 9),
    G("SWEETEN THE FRUIT",   "ALL THE FRUIT TURNS SWEET AND BIG",                 FOOD,     50, 8),
    G("STILL THE QUAKE",     "THE GROUND STOPS SHAKING",                          SHELTER,  10, 8),
    G("TURN THE TIDE",       "THE SEA GIVES UP FISH AND DRIFTWOOD",               FOOD,     60, 9),
    G("QUICKEN THE HARVEST", "CROPS RIPEN IN A SINGLE NIGHT",                     FERTILE,  30, 10),
    G("WARM A COLD NIGHT",   "THE COLD LEAVES AND EVERYONE SLEEPS WELL",          REST,    100, 8),
    G("NAME A PLACE",        "A HILL OR POND GETS A NAME AND A LEGEND",   LEGEND,    1, 9),
    G("SPIN A TALE",         "A STORY IS TOLD AND IT TURNS TRUE",                 LEGEND,    1, 10),
    G("GROW A GLASS WOOD",   "A WOOD OF SHINING CRYSTAL TREES RISES",             FOREST,    3, 12),
    G("SEED A NEW PEOPLE",   "A FAMILY OF NEWCOMERS ARRIVES AND SETTLES",         PEOPLE,    1, 14),
    G("OPEN A GREAT SPRING", "A GREAT SPRING FEEDS FIELDS FAR AND WIDE",          FERTILE,  40, 12),
    G("CROWN A FESTIVAL",    "A FESTIVAL FILLS THE TOWN WITH JOY",                FUN,     100, 12),
    G("FOLD THE DAY",        "THE NIGHT PASSES AT ONCE AND THE SUN COMES UP",     DAY,       0, 10),
    G("WIPE AWAY A SORROW",  "OLD GRIEVES AND SOUR MOODS ARE LIFTED",             MOOD,     30, 12),
    G("MAKE A LUCKY YEAR",   "EVERYTHING GOES RIGHT FOR A LONG TIME",             LUCK,     20, 14),
    G("BEGIN A NEW AGE",     "A NEW AGE OF PEACE AND PLENTY DAWNS",               LEGEND,    3, 16),
};
#define NT ((int)(sizeof T / sizeof T[0]))
_Static_assert(NT >= 40, "the plan asks for at least 40 god powers");
_Static_assert(NT <= GOD_MAX, "the save format holds at most GOD_MAX powers");

int god_count(void){ return NT; }
const GodPower *god_power(int n){ return n >= 1 && n <= NT ? &T[n - 1] : 0; }
void god_need_level(int n, Lv *out){
    Lv a; lv_from_u64(&a, (uint64_t)(n < 0 ? 0 : n)); lv_mul_small(out, &a, GOD_STEP_LV);
}
int god_unlocked(const Lv *level, int n, int sandbox){
    if(n < 1 || n > NT) return 0;
    if(sandbox) return 1;
    Lv need; god_need_level(n, &need);
    return lv_cmp(level, &need) >= 0;
}
int god_unlocked_count(const Lv *level, int sandbox){
    int c = 0; for(int n = 1; n <= NT; n++) if(god_unlocked(level, n, sandbox)) c++;
    return c;
}

void god_state_init(GodState *g){ memset(g, 0, sizeof *g); }
static int seen(const GodState *g, int n){ return (g->seen[(n - 1) >> 3] >> ((n - 1) & 7)) & 1; }
static void mark(GodState *g, int n){ g->seen[(n - 1) >> 3] |= (uint8_t)(1u << ((n - 1) & 7)); }
int god_check(GodState *g, const Lv *level, int *out, int max){
    int c = 0;
    if(g->sandbox){ for(int n = 1; n <= NT; n++) mark(g, n); return 0; }
    for(int n = 1; n <= NT; n++){
        if(seen(g, n) || !god_unlocked(level, n, 0)) continue;
        if(c >= max) break;                          // no room: leave the rest to the next call
        mark(g, n); out[c++] = n;
    }
    return c;
}
static size_t put(char *b, size_t o, size_t sz, const char *s){ while(*s && o + 1 < sz) b[o++] = *s++; b[o] = 0; return o; }
static void u64txt(char *t, uint64_t v){ char r[24]; int l = 0; do { r[l++] = (char)('0' + v % 10); v /= 10; } while(v); for(int i = 0; i < l; i++) t[i] = r[l - 1 - i]; t[l] = 0; }
void god_unlock_text(int n, char *buf, size_t sz){
    const GodPower *p = god_power(n); char t[24]; size_t o = 0; if(!sz) return; buf[0] = 0;
    if(!p) return;
    u64txt(t, (uint64_t)n); o = put(buf, o, sz, "GOD POWER "); o = put(buf, o, sz, t); o = put(buf, o, sz, " IS OPEN: "); put(buf, o, sz, p->name);
}
void god_locked_text(int n, const Lv *level, char *buf, size_t sz){
    Lv need; char t[LV_DEC_MAX]; size_t o = 0; if(!sz) return; buf[0] = 0;
    if(!god_power(n)) return;
    if(god_unlocked(level, n, 0)){ put(buf, 0, sz, "OPEN"); return; }
    god_need_level(n, &need); lv_to_dec(&need, t, sizeof t);
    o = put(buf, o, sz, "LOCKED. NEEDS LEVEL "); put(buf, o, sz, t);
}
void god_state_to_bytes(const GodState *g, uint8_t *out){ memcpy(out, g->seen, GOD_MAX / 8); out[GOD_MAX / 8] = g->sandbox ? 1 : 0; }
void god_state_from_bytes(GodState *g, const uint8_t *in){
    memcpy(g->seen, in, GOD_MAX / 8); g->sandbox = in[GOD_MAX / 8] ? 1 : 0;
    for(int n = NT + 1; n <= GOD_MAX; n++) g->seen[(n - 1) >> 3] &= (uint8_t)~(1u << ((n - 1) & 7));
}
