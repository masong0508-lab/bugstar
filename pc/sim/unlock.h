// unlock.h: PROGRESSIVE DISCLOSURE of the life sim (patch 47). Plain C, no game types, no floating point, no globals. Header only (static inline).
//
// The life sim (townsfolk with needs, jobs and friends; the colony; fame, the mayor and the armada) is a big thing. It must not be thrown at a new player.
// So every part of it is a FEATURE with one bit in a flag word, and a menu, a list row, a HUD piece or a line of a card is drawn only when its bit is set.
//   - A feature is EARNED by something the player did or met (a trigger below): the first town, the first talk, the first gold, a freed bug, fame 350 ...
//   - The first 10 minutes of play only open the BASIC features (clock, journal, people, jobs, needs, friends, weather). The rest waits.
//   - When a feature opens, a short TIP card says what it is (simple words, short lines; a big-text option). A TIPS page keeps them to re-read.
//     A "!" stays on the row until the player opens it.
//   - SHOW ALL (a tester's switch): OFF = only what was earned, SHAPES = locked menus show as ???, ALL = everything.
// Nothing in here reads or writes the game: the glue (sim_glue.inc) fills UlFacts and calls ul_update. The flags are saved in the .sim file (chunk UNLK, version 1).
#ifndef UNLOCK_H
#define UNLOCK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ---------------------------------------------------------------- the features (one bit each)
enum {
    UF_CLOCK,      // the clock chip on the screen (time of day only)            first time in a town
    UF_JOURNAL,    // JOURNAL in the side menu                                     the town has some news
    UF_MEET,       // PEOPLE in the side menu, the name card                       first talk to a bug
    UF_JOBS,       // jobs and what they do now (card and list)                    second talk
    UF_NEEDS,      // hunger, rest, mood, sickness                                 third talk, or a bug got hungry or tired
    UF_FRIEND,     // the friendship meter and what friends do                     friendship 10
    UF_LETTER,     // letters from friends                                         the first letter
    UF_ASKS,       // requests for help                                            the first request
    UF_GIFT,       // gifts from friends                                           the first gift
    UF_LOVE,       // couples                                                      met a bug who loves another
    UF_MONEY,      // gold in the life sim (taxes, ships)                          first gold earned
    UF_HOME,       // homes in the colony (houses)                                 the HOUSES rung opened
    UF_COLONY,     // COLONY in the side menu and on the ship                      first bug freed
    UF_SHOPS,      // WORKSHOPS: smelter, smithy, loom (crafting)                  the SHOPS rung opened
    UF_MINE,       // THE MINE                                                     the MINE rung opened
    UF_CALLS,      // the colony's calls (goals on dungeon floors)                 saw a call
    UF_RAID,       // raids and defence                                            a raid is on its way
    UF_WEATHER,    // weather on the clock                                         first weather
    UF_SEASON,     // season on the clock                                          first new season
    UF_FAME,       // fame ranks and the hint for the next menu                    fame 50
    UF_LEVEL,      // MY LEVEL in the side menu                                    level 10
    UF_GODS,       // GOD POWERS                                                   fame 350
    UF_SANDBOX,    // SANDBOX                                                      fame 700
    UF_MAYOR,      // TOWN HALL: taxes and a feast                                 a town named you mayor
    UF_ARMADA,     // ARMADA: ships                                                mayor, fame 1200, gold
    UF_WORLDS,     // settled worlds                                               the first world
    UF_GALAXY,     // crossing galaxies                                            the first galaxy
    UF_N
};
#define UL_ALL        ((1u << UF_N) - 1u)
#define UL_EARLY_TICKS 6000u       // the first 10 minutes of play on a planet with a town (sim ticks, 10 a second)
#define UL_FAME_KNOWN   50
#define UL_FAME_HERO    350
#define UL_FAME_CHAMP   700
#define UL_FAME_LEGEND  1200
#define UL_LEVEL_OPEN   10

enum { ULW_MAJOR, ULW_NORMAL, ULW_MINOR };           // how loud a tip is (see POPUP LEVEL)
typedef struct {
    const char *key, *name;        // key: for the test dump. name: the menu row and the tip title
    uint8_t weight, basic, shape;  // basic: may open in the first 10 minutes. shape: a locked ??? shape is shown in SHAPES mode
    uint32_t needs;                // features that must be open first
    const char *tip[4];            // up to four lines, 26 letters at most (they fit big text)
} UlDef;
#define UB(f) (1u << (f))
static const UlDef ul_defs[UF_N] = {
 [UF_CLOCK]   = { "clock",   "CLOCK",       ULW_MINOR,  1, 0, 0,                      { "A CLOCK SHOWS THE TIME.", "BUGS EAT, WORK AND SLEEP", "BY THE HOUR. PRESS A ON", "EMPTY GROUND FOR THE MENU." } },
 [UF_JOURNAL] = { "journal", "JOURNAL",     ULW_NORMAL, 1, 1, UB(UF_CLOCK),           { "THE JOURNAL KEEPS NEWS", "OF THE TOWN.", "OPEN IT FROM THE MENU.", 0 } },
 [UF_MEET]    = { "meet",    "PEOPLE",      ULW_MAJOR,  1, 1, UB(UF_CLOCK),           { "YOU MET A BUG OF THE TOWN.", "TALK TO EACH ONCE A DAY.", "THE PEOPLE LIST IS OPEN.", 0 } },
 [UF_JOBS]    = { "jobs",    "JOBS",        ULW_NORMAL, 1, 0, UB(UF_MEET),            { "EVERY BUG HAS A JOB.", "COOKS, CLERKS, GUARDS.", "SEE IT WHEN YOU TALK.", 0 } },
 [UF_NEEDS]   = { "needs",   "NEEDS",       ULW_NORMAL, 1, 0, UB(UF_MEET),            { "BUGS GET HUNGRY AND TIRED.", "A HAPPY BUG WORKS WELL.", "SEE IT IN THE PEOPLE LIST.", 0 } },
 [UF_FRIEND]  = { "friend",  "FRIENDS",     ULW_NORMAL, 1, 0, UB(UF_MEET),            { "TALK TO A BUG EVERY DAY.", "FRIENDS WRITE LETTERS", "AND LEND A HAND.", 0 } },
 [UF_LETTER]  = { "letter",  "LETTERS",     ULW_MINOR,  0, 0, UB(UF_FRIEND),          { "A FRIEND WROTE TO YOU.", "READ IT IN THE JOURNAL.", 0, 0 } },
 [UF_ASKS]    = { "asks",    "REQUESTS",    ULW_NORMAL, 0, 0, UB(UF_MEET),          { "SOME BUGS NEED HELP.", "FOOD, A LISTENING EAR,", "OR PEACE. HELPING MAKES", "FRIENDS." } },
 [UF_GIFT]    = { "gift",    "GIFTS",       ULW_MINOR,  0, 0, UB(UF_FRIEND),          { "FRIENDS SEND GIFTS.", "FOOD, GOLD OR ORE.", "THEY FIT THEIR JOB.", 0 } },
 [UF_LOVE]    = { "love",    "LOVE",        ULW_MINOR,  0, 0, UB(UF_MEET),            { "TWO BUGS ARE IN LOVE.", "LOVE KEEPS THEM CALM.", 0, 0 } },
 [UF_MONEY]   = { "money",   "GOLD",        ULW_NORMAL, 0, 0, 0,                      { "YOU EARNED GOLD!", "GOLD PAYS FOR TAXES,", "FEASTS AND SHIPS LATER.", 0 } },
 [UF_HOME]    = { "home",    "HOMES",       ULW_NORMAL, 0, 0, UB(UF_COLONY),          { "THE COLONY HAS HOMES.", "BUGS SLEEP THERE.", "YOU CAN NAP THERE TOO.", 0 } },
 [UF_COLONY]  = { "colony",  "COLONY",      ULW_MAJOR,  0, 1, 0,                      { "YOU FREED A BUG!", "IT SETTLES IN YOUR COLONY.", "OPEN IT FROM THE MENU.", "OR FROM YOUR SHIP." } },
 [UF_SHOPS]   = { "shops",   "WORKSHOPS",   ULW_NORMAL, 0, 0, UB(UF_COLONY),          { "SMELT ORE INTO BARS.", "MAKE TOOLS AND CLOTH.", "OPEN IT IN THE COLONY.", 0 } },
 [UF_MINE]    = { "mine",    "THE MINE",    ULW_NORMAL, 0, 0, UB(UF_COLONY),          { "MINERS DIG DOWN.", "BRACE THE ROOF IN TIME.", "GEMS COME UP.", 0 } },
 [UF_CALLS]   = { "calls",   "CALLS",       ULW_NORMAL, 0, 0, UB(UF_COLONY),          { "THE COLONY ASKS FOR HELP.", "DO THE GOAL ON THIS", "FLOOR TO ANSWER.", 0 } },
 [UF_RAID]    = { "raid",    "RAIDS",       ULW_NORMAL, 0, 0, UB(UF_COLONY),          { "A RAID IS COMING.", "WALLS AND TOWERS HELP.", "STAND GUARD TO HELP.", 0 } },
 [UF_WEATHER] = { "weather", "WEATHER",     ULW_MINOR,  1, 0, UB(UF_CLOCK),           { "WEATHER CHANGES THE DAY.", "IT SHOWS ON THE CLOCK.", 0, 0 } },
 [UF_SEASON]  = { "season",  "SEASONS",     ULW_MINOR,  0, 0, UB(UF_WEATHER),         { "A NEW SEASON BEGAN.", "EACH ONE LASTS 7 DAYS.", 0, 0 } },
 [UF_FAME]    = { "fame",    "FAME",        ULW_NORMAL, 0, 0, 0,                      { "YOUR NAME IS KNOWN.", "FAME OPENS MORE MENUS.", "IT HINTS AT THE NEXT MENU.", 0 } },
 [UF_LEVEL]   = { "level",   "MY LEVEL",    ULW_MINOR,  0, 1, 0,                      { "SEE YOUR LEVEL IN FULL.", "IT CAN GET VERY BIG.", 0, 0 } },
 [UF_GODS]    = { "gods",    "GOD POWERS",  ULW_MAJOR,  0, 1, UB(UF_FAME),            { "THEY SAY YOU MOVE THE SKY.", "CAST POWERS WITH SPIRIT.", "MORE OPEN WITH LEVEL.", 0 } },
 [UF_SANDBOX] = { "sandbox", "SANDBOX",     ULW_NORMAL, 0, 1, UB(UF_GODS),            { "A CHAMPION MAY PLAY.", "ALL POWERS OPEN. NO COST.", 0, 0 } },
 [UF_MAYOR]   = { "mayor",   "TOWN HALL",   ULW_MAJOR,  0, 1, UB(UF_FAME),            { "A TOWN NAMED YOU MAYOR!", "COLLECT TAXES.", "THROW A FEAST.", 0 } },
 [UF_ARMADA]  = { "armada",  "ARMADA",      ULW_MAJOR,  0, 1, UB(UF_MAYOR) | UB(UF_MONEY), { "THE SKY IS YOURS TO FILL.", "BUY SHIPS WITH GOLD.", "SHIPS SETTLE NEW WORLDS.", 0 } },
 [UF_WORLDS]  = { "worlds",  "WORLDS",      ULW_NORMAL, 0, 0, UB(UF_ARMADA),          { "A WORLD IS YOURS.", "SETTLED WORLDS SEND GOLD", "EVERY DAY.", 0 } },
 [UF_GALAXY]  = { "galaxy",  "GALAXIES",    ULW_NORMAL, 0, 0, UB(UF_WORLDS),          { "A GALAXY IS FULL.", "CROSS TO THE NEXT ONE.", 0, 0 } },
};

// ---------------------------------------------------------------- what the player has done (counted when it happens) and what is true now
enum { ULC_TALK, ULC_ASK, ULC_MATE, ULC_GIFT, ULC_CALL, ULC_GOLD, ULC_WEATHER, ULC_NEEDY, ULC_N };     // counters, each stops at 255
typedef struct {
    uint8_t  used;                  // 1 once a UNLK chunk was read or the glue touched it
    uint8_t  cnt[ULC_N];
    uint32_t have;                  // bit f: feature f is open
    uint32_t told;                  // bit f: its tip was handled (shown, or skipped by the settings)
    uint32_t opened;                // bit f: the player opened its row / read its tip (the ! goes away)
} Unlock;
typedef struct {                    // filled by the glue every half second
    int64_t fame, level;
    int     town;                   // bugs live on this planet
    int     log_n;                  // news in the journal
    int     friend_max;             // best friendship (0..100)
    int     letters;                // letters in the journal
    int     day;                    // sim day
    int     season_on;              // this planet has seasons
    int     freed;                  // bugs freed
    int     homes, shops, mine;     // colony rungs open: HOUSES, SHOPS, MINE
    int     raid;                   // a raid is on its way
    int     mayor;
    uint64_t worlds_total; uint32_t galaxies;
} UlFacts;
typedef struct { uint8_t show_all, tips, level, autopause, big; } UlOpts;       // the LIFE SIM options: show_all 0 OFF 1 SHAPES 2 ALL; tips 0/1; level 0 FEW 1 NORMAL 2 MANY; autopause 0/1; big 0/1
enum { ULSA_OFF, ULSA_SHAPES, ULSA_ALL };

static inline void ul_opts_default(UlOpts *o){ o->show_all = ULSA_OFF; o->tips = 1; o->level = 1; o->autopause = 1; o->big = 0; }
static inline int ul_has(const Unlock *u, int f){ return f >= 0 && f < UF_N && (u->have >> f & 1); }
static inline void ul_note(Unlock *u, int c){ if(c >= 0 && c < ULC_N && u->cnt[c] < 255) u->cnt[c]++; u->used = 1; }

// is feature f's trigger true right now? (ignores the first-10-minutes rule and the prerequisites; ul_update adds them)
static inline int ul_trigger(const Unlock *u, const UlFacts *f, int ft){
    switch(ft){
    case UF_CLOCK:   return f->town > 0;
    case UF_JOURNAL: return f->log_n >= 2;
    case UF_MEET:    return u->cnt[ULC_TALK] >= 1;
    case UF_JOBS:    return u->cnt[ULC_TALK] >= 2;
    case UF_NEEDS:   return u->cnt[ULC_TALK] >= 3 || u->cnt[ULC_NEEDY] >= 1;
    case UF_FRIEND:  return f->friend_max >= 10;
    case UF_LETTER:  return f->letters >= 1;
    case UF_ASKS:    return u->cnt[ULC_ASK] >= 1;
    case UF_GIFT:    return u->cnt[ULC_GIFT] >= 1;
    case UF_LOVE:    return u->cnt[ULC_MATE] >= 1;
    case UF_MONEY:   return u->cnt[ULC_GOLD] >= 1;
    case UF_HOME:    return f->homes > 0;
    case UF_COLONY:  return f->freed >= 1;
    case UF_SHOPS:   return f->shops > 0;
    case UF_MINE:    return f->mine > 0;
    case UF_CALLS:   return u->cnt[ULC_CALL] >= 1;
    case UF_RAID:    return f->raid > 0;
    case UF_WEATHER: return u->cnt[ULC_WEATHER] >= 1 || f->day >= 2;
    case UF_SEASON:  return f->season_on && f->day >= 7;
    case UF_FAME:    return f->fame >= UL_FAME_KNOWN;
    case UF_LEVEL:   return f->level >= UL_LEVEL_OPEN;
    case UF_GODS:    return f->fame >= UL_FAME_HERO;
    case UF_SANDBOX: return f->fame >= UL_FAME_CHAMP;
    case UF_MAYOR:   return f->mayor > 0;
    case UF_ARMADA:  return f->mayor > 0 && f->fame >= UL_FAME_LEGEND;
    case UF_WORLDS:  return f->worlds_total >= 1;
    case UF_GALAXY:  return f->galaxies >= 1;
    }
    return 0;
}
// opens every feature whose trigger is true and whose prerequisites are open. early = still in the first 10 minutes: only basic features may open.
// Returns the mask of the features that opened now. Features never close again.
static inline uint32_t ul_update(Unlock *u, const UlFacts *f, int early){
    uint32_t before = u->have;
    for(int pass = 0; pass < UF_N; pass++){                      // prerequisites may open in the same call, in any table order
        uint32_t add = 0;
        for(int ft = 0; ft < UF_N; ft++){
            if(u->have >> ft & 1) continue;
            const UlDef *d = &ul_defs[ft];
            if(early && !d->basic) continue;
            if((u->have & d->needs) != d->needs) continue;
            if(ul_trigger(u, f, ft)) add |= UB(ft);
        }
        if(!add) break;
        u->have |= add;
    }
    u->used = 1; return u->have & ~before;
}
// ---------------------------------------------------------------- what is drawn
enum { ULV_HIDDEN, ULV_SHAPE, ULV_SHOWN };
static inline int ul_vis(const Unlock *u, int ft, int show_all){
    if(ft < 0 || ft >= UF_N) return ULV_HIDDEN;
    if(show_all >= ULSA_ALL || (u->have >> ft & 1)) return ULV_SHOWN;
    if(show_all == ULSA_SHAPES && ul_defs[ft].shape) return ULV_SHAPE;
    return ULV_HIDDEN;
}
static inline int ul_shown(const Unlock *u, int ft, int show_all){ return ul_vis(u, ft, show_all) == ULV_SHOWN; }
static inline uint32_t ul_fresh(const Unlock *u){ return u->have & ~u->opened; }           // the ! markers
static inline void ul_open(Unlock *u, int ft){ if(ft >= 0 && ft < UF_N && (u->have >> ft & 1)){ u->opened |= UB(ft); u->used = 1; } }
static inline int ul_popcount(uint32_t v){ int n = 0; while(v){ n += (int)(v & 1u); v >>= 1; } return n; }

// the side menu (the quick menu): its rows, in order. TIPS is last.
enum { SQ_JOURNAL, SQ_PEOPLE, SQ_GODS, SQ_SANDBOX, SQ_LEVEL, SQ_COLONY, SQ_HALL, SQ_ARMADA, SQ_TIPS, SQ_N };
static inline int ul_sq_feat(int sq){ static const int8_t m[SQ_N] = { UF_JOURNAL, UF_MEET, UF_GODS, UF_SANDBOX, UF_LEVEL, UF_COLONY, UF_MAYOR, UF_ARMADA, -1 }; return sq < 0 || sq >= SQ_N ? -1 : m[sq]; }
static inline int ul_feat_sq(int ft){ for(int s = 0; s < SQ_N; s++) if(ul_sq_feat(s) == ft) return s; return -1; }
static inline int ul_sq_vis(const Unlock *u, int sq, int show_all){
    if(sq == SQ_TIPS) return show_all >= ULSA_ALL || u->have ? ULV_SHOWN : ULV_HIDDEN;
    return ul_vis(u, ul_sq_feat(sq), show_all);
}
// ---------------------------------------------------------------- tips
enum { ULT_NONE, ULT_TOAST, ULT_CARD };
static inline int ul_tip_style(const UlOpts *o, int ft){
    if(ft < 0 || ft >= UF_N || !o->tips || o->show_all >= ULSA_ALL) return ULT_NONE;
    int w = ul_defs[ft].weight, card = o->level >= 2 || (o->level == 1 && w <= ULW_NORMAL) || (o->level == 0 && w == ULW_MAJOR);
    return card && o->autopause ? ULT_CARD : ULT_TOAST;
}
static inline uint32_t ul_pending(const Unlock *u){ return u->have & ~u->told; }                // opened but not yet told
static inline int ul_next_tip(const Unlock *u){                                                 // the loudest first, then table order
    uint32_t p = ul_pending(u); for(int w = ULW_MAJOR; w <= ULW_MINOR; w++) for(int ft = 0; ft < UF_N; ft++) if((p >> ft & 1) && ul_defs[ft].weight == w) return ft;
    return -1;
}
static inline void ul_told(Unlock *u, int ft){ if(ft >= 0 && ft < UF_N){ u->told |= UB(ft); u->used = 1; } }
static inline void ul_told_all(Unlock *u){ u->told = u->have; u->used = 1; }
static inline int ul_tip_lines(int ft, const char **ln){ int n = 0; if(ft < 0 || ft >= UF_N) return 0; for(int i = 0; i < 4; i++) if(ul_defs[ft].tip[i]) ln[n++] = ul_defs[ft].tip[i]; return n; }
static inline int ul_flood(const Unlock *u){ return ul_popcount(ul_pending(u)) > 3; }          // many at once (an old save, a cheat): one summary card instead

// ---------------------------------------------------------------- the save record (chunk UNLK, version 1): 1 version, 8 counters, have, told, opened
#define UL_BYTES 21
static inline void ul_p32(uint8_t *b, uint32_t v){ b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }
static inline uint32_t ul_g32(const uint8_t *b){ return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static inline size_t ul_write(const Unlock *u, uint8_t *o, size_t cap){
    if(cap < UL_BYTES) return 0; o[0] = 1; for(int i = 0; i < ULC_N; i++) o[1 + i] = u->cnt[i];
    ul_p32(o + 9, u->have & UL_ALL); ul_p32(o + 13, u->told & u->have & UL_ALL); ul_p32(o + 17, u->opened & u->have & UL_ALL); return UL_BYTES;
}
static inline int ul_read(Unlock *u, const uint8_t *in, size_t n){                              // 0 ok, -1 wrong size or version
    if(n != UL_BYTES || in[0] != 1) return -1; memset(u, 0, sizeof *u); u->used = 1;
    for(int i = 0; i < ULC_N; i++) u->cnt[i] = in[1 + i];
    u->have = ul_g32(in + 9) & UL_ALL; u->told = ul_g32(in + 13) & u->have; u->opened = ul_g32(in + 17) & u->have; return 0;
}
// a feature name for the dump: "clock,journal"
static inline size_t ul_names(uint32_t m, char *o, size_t n){
    size_t at = 0; if(!n) return 0; o[0] = 0;
    for(int ft = 0; ft < UF_N; ft++) if(m >> ft & 1){ const char *k = ul_defs[ft].key; if(at && at + 1 < n) o[at++] = ','; while(*k && at + 1 < n) o[at++] = *k++; o[at] = 0; }
    return at;
}
#endif
