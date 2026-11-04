// standing.h: how the world sees you (patch 42). Plain C, no game types, no floating point, no globals. Header only (static inline), so no build list changes.
//
// The side game does not open all at once. What you can do grows with your FAME (the action game's own fame, the same ranks the HUD shows):
//
//   fame     rank        opens                                  what it says about the scale of the game
//   0        NOBODY      JOURNAL, MY LEVEL                      one bug, one town
//   50       KNOWN BUG   PEOPLE                                 the town knows your name
//   150      BRAVE BUG   COLONY                                 settlers trust you
//   350      HERO        GOD POWERS                             (they still open by level, one per 1,000 levels)
//   700      CHAMPION    SANDBOX, and the world APPOINTS YOU MAYOR of the planet you stand on (it needs a town: 2 bugs or more)
//                        -> TOWN HALL: taxes, a feast
//   1200     BUG LEGEND  (as mayor) ARMADA: ships bought with gold, as many as your LEVEL can command; with them you settle other
//                        worlds, and when a galaxy is full you cross to the next one
//
// Nothing is thrown at you: a menu that is not open is not in the list. One line under the list says what the next one needs.
// Fame, gold and level come in as plain numbers; nothing in here reads or writes the action game.
#ifndef STANDING_H
#define STANDING_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

enum { STR_NOBODY, STR_KNOWN, STR_BRAVE, STR_HERO, STR_CHAMPION, STR_LEGEND, STR_N };          // ranks (same thresholds as the game's fame_rank)
enum { STM_JOURNAL, STM_PEOPLE, STM_GODS, STM_SANDBOX, STM_LEVEL, STM_COLONY, STM_HALL, STM_ARMADA, STM_N };   // menus, in the order of the list

#define ST_DAY_TICKS     4800      // tax days are 8 minutes of play on any planet (the sim's standard day)
#define ST_TAX_DAYS_MAX  30        // taxes pile up for at most this many days
#define ST_GALAXY_WORLDS 12        // worlds settled before the way to the next galaxy opens
#define ST_NAME_MAX      15
#define ST_BYTES         80        // the save record: 4 flags, 4 town, 16 name, 4 x u64, 4 x u32, 1 x u64

typedef struct {
    uint8_t  used;                      // 0 = nothing was ever filed for this slot (an old save): the glue adopts the menus you already earned, silently
    uint8_t  flags;                     // bit 0: the mayor's ceremony card was shown, bit 1: the armada card was shown
    uint16_t told;                      // bit m: menu m was announced
    uint32_t mayor_town;                // which planet appointed you (the town hash); 0 = nobody has
    char     home[ST_NAME_MAX + 1];     // its name
    uint64_t mayor_cal;                 // the tick you were appointed
    uint64_t tax_cal;                   // the tick the taxes were last collected
    uint64_t ships;                     // the armada
    uint64_t worlds_total;              // worlds settled, all galaxies
    uint32_t worlds;                    // worlds settled in the galaxy you are working on
    uint32_t galaxies;                  // galaxies crossed (the one you start in is 0)
    uint32_t tax_rate;                  // gold a day, as of the last visit to the home town
    uint32_t feasts;                    // feasts thrown
    uint64_t gold_spent;                // for the record
} Standing;

// ---------------------------------------------------------------- ranks and menus
static inline int32_t stand_rank_fame(int r){ static const int32_t t[STR_N] = { 0, 50, 150, 350, 700, 1200 }; return r < 0 ? 0 : r >= STR_N ? t[STR_N - 1] : t[r]; }
static inline int stand_rank(int64_t fame){ int r = 0; for(int i = 1; i < STR_N; i++) if(fame >= stand_rank_fame(i)) r = i; return r; }
static inline const char *stand_rank_name(int r){ static const char *const n[STR_N] = { "NOBODY", "KNOWN BUG", "BRAVE BUG", "HERO", "CHAMPION", "BUG LEGEND" }; return r < 0 || r >= STR_N ? "?" : n[r]; }
static inline const char *stand_menu_name(int m){ static const char *const n[STM_N] = { "JOURNAL", "PEOPLE", "GOD POWERS", "SANDBOX", "MY LEVEL", "COLONY", "TOWN HALL", "ARMADA" }; return m < 0 || m >= STM_N ? "?" : n[m]; }
static inline int stand_menu_rank(int m){ static const int8_t r[STM_N] = { STR_NOBODY, STR_KNOWN, STR_HERO, STR_CHAMPION, STR_NOBODY, STR_BRAVE, STR_CHAMPION, STR_LEGEND }; return m < 0 || m >= STM_N ? STR_N : r[m]; }
static inline int stand_is_mayor(const Standing *s){ return s->mayor_town != 0; }

// bit m is set when menu m is open: the fame is there and, for the TOWN HALL and the ARMADA, a town has named you mayor
static inline uint16_t stand_open_mask(const Standing *s, int64_t fame){
    int r = stand_rank(fame); uint16_t m = 0;
    for(int i = 0; i < STM_N; i++){
        if(r < stand_menu_rank(i)) continue;
        if((i == STM_HALL || i == STM_ARMADA) && !stand_is_mayor(s)) continue;
        m |= (uint16_t)(1u << i);
    }
    return m;
}
// the menu to hint at under the list: the open-next one with the lowest fame need (table order breaks ties); -1 when everything is open
static inline int stand_next_menu(uint16_t open_mask){
    int best = -1;
    for(int i = 0; i < STM_N; i++){ if(open_mask >> i & 1) continue; if(best < 0 || stand_menu_rank(i) < stand_menu_rank(best)) best = i; }
    return best;
}
static inline size_t st_cat(char *o, size_t n, size_t at, const char *t){ while(*t && at + 1 < n) o[at++] = *t++; if(n) o[at < n ? at : n - 1] = 0; return at; }
static inline size_t st_catn(char *o, size_t n, size_t at, uint64_t v){ char t[24]; int k = 0; if(!v) t[k++] = '0'; while(v){ t[k++] = (char)('0' + v % 10); v /= 10; } char u[24]; for(int i = 0; i < k; i++) u[i] = t[k - 1 - i]; u[k] = 0; return st_cat(o, n, at, u); }
// "FAME 150 (BRAVE BUG)" or what else is missing
static inline void stand_lock_text(const Standing *s, int m, int64_t fame, char *o, size_t n){
    size_t at = 0; if(!n) return; o[0] = 0;
    if(stand_rank(fame) < stand_menu_rank(m)){ at = st_cat(o, n, at, "FAME "); at = st_catn(o, n, at, (uint64_t)stand_rank_fame(stand_menu_rank(m))); at = st_cat(o, n, at, " ("); at = st_cat(o, n, at, stand_rank_name(stand_menu_rank(m))); st_cat(o, n, at, ")"); return; }
    if((m == STM_HALL || m == STM_ARMADA) && !stand_is_mayor(s)) st_cat(o, n, 0, "A TOWN MUST NAME YOU MAYOR FIRST");
}
static inline const char *stand_menu_card(int m){      // the line shown when a menu opens
    switch(m){
    case STM_PEOPLE:  return "THE TOWN KNOWS YOUR NAME. PEOPLE IS OPEN";
    case STM_COLONY:  return "SETTLERS TRUST YOU NOW. COLONY IS OPEN";
    case STM_GODS:    return "THEY SAY YOU MOVE THE SKY. GOD POWERS ARE OPEN";
    case STM_SANDBOX: return "A CHAMPION MAY PLAY. SANDBOX IS OPEN";
    case STM_HALL:    return "THE WORLD NAMES YOU MAYOR! TOWN HALL IS OPEN";
    case STM_ARMADA:  return "THE SKY IS YOURS TO FILL. ARMADA IS OPEN";
    default:          return "A NEW MENU IS OPEN";
    }
}

// ---------------------------------------------------------------- the mayor
static inline int stand_can_appoint(const Standing *s, int64_t fame, int folk){ return !stand_is_mayor(s) && fame >= stand_rank_fame(STR_CHAMPION) && folk >= 2; }
static inline void stand_appoint(Standing *s, uint32_t town, const char *name, uint64_t cal){
    s->mayor_town = town ? town : 1; s->mayor_cal = cal; s->tax_cal = cal; int i = 0;
    for(; name && name[i] && i < ST_NAME_MAX; i++) s->home[i] = name[i]; s->home[i] = 0;
}
// gold a day from the home town: its bugs, a little more when they are happy (mood 0..255). unit is the game's gold unit (20 + level up to 100).
static inline uint32_t stand_rate(int folk, int mood, uint64_t unit){
    if(folk < 0) folk = 0; if(mood < 0) mood = 0; if(mood > 255) mood = 255;
    uint64_t v = (uint64_t)folk * (uint64_t)(1 + mood / 128) * unit / 2; return v > 0xFFFFFFu ? 0xFFFFFFu : (uint32_t)v;
}
static inline uint64_t stand_tribute(const Standing *s, uint64_t unit){ return s->worlds_total * unit * 2; }     // each settled world sends this much a day
static inline uint32_t stand_tax_days(const Standing *s, uint64_t cal){ if(!stand_is_mayor(s) || cal <= s->tax_cal) return 0; uint64_t d = (cal - s->tax_cal) / ST_DAY_TICKS; return d > ST_TAX_DAYS_MAX ? ST_TAX_DAYS_MAX : (uint32_t)d; }
static inline uint64_t stand_tax_due(const Standing *s, uint64_t cal, uint64_t unit){ return (uint64_t)stand_tax_days(s, cal) * ((uint64_t)s->tax_rate + stand_tribute(s, unit)); }
static inline uint64_t stand_tax_take(Standing *s, uint64_t cal, uint64_t unit){      // the gold; the clock restarts (the days over the cap are lost: collect now and then)
    uint64_t g = stand_tax_due(s, cal, unit); if(g) s->tax_cal = cal; return g;
}
static inline uint64_t stand_feast_cost(uint64_t unit){ return unit * 30; }

// ---------------------------------------------------------------- the armada
static inline uint64_t st_sat_add(uint64_t a, uint64_t b){ return a + b < a ? UINT64_MAX : a + b; }
static inline uint64_t st_sat_mul(uint64_t a, uint64_t b){ if(a && b > UINT64_MAX / a) return UINT64_MAX; return a * b; }
static inline uint64_t stand_command(uint64_t level){ return level < 5 ? 5 : level; }             // most ships you may lead: one a level (5 at the least)
static inline uint64_t stand_ship_cost(const Standing *s, uint64_t unit){ return st_sat_mul(unit, st_sat_add(10, s->ships / 4)); }
static inline const char *stand_fleet_name(uint64_t n){
    return n >= 1000000 ? "STAR ARMADA" : n >= 10000 ? "GRAND ARMADA" : n >= 1000 ? "ARMADA" : n >= 100 ? "FLEET" : n >= 10 ? "SQUADRON" : n >= 1 ? "ESCORT" : "NO SHIPS";
}
static inline int stand_fleet_tier(uint64_t n){ return n >= 1000000 ? 6 : n >= 10000 ? 5 : n >= 1000 ? 4 : n >= 100 ? 3 : n >= 10 ? 2 : n >= 1 ? 1 : 0; }
// buy up to want ships: stops at the command limit or when the gold runs out. Returns how many; *gold is reduced by the exact price.
static inline uint64_t stand_buy(Standing *s, int64_t *gold, uint64_t level, uint64_t unit, uint64_t want){
    uint64_t cmd = stand_command(level), got = 0;
    while(got < want && s->ships < cmd){
        uint64_t c = stand_ship_cost(s, unit); if(*gold < 0 || (uint64_t)*gold < c || c > (uint64_t)INT64_MAX) break;
        *gold -= (int64_t)c; s->ships++; got++; s->gold_spent = st_sat_add(s->gold_spent, c);
    }
    return got;
}
static inline uint64_t stand_buy_price(const Standing *s, uint64_t level, uint64_t unit, uint64_t want, uint64_t *count){    // what buying 'want' would cost, and how many it would be, if gold did not matter
    Standing t = *s; uint64_t cmd = stand_command(level), sum = 0, n = 0;
    while(n < want && t.ships < cmd){ sum = st_sat_add(sum, stand_ship_cost(&t, unit)); t.ships++; n++; }
    if(count) *count = n; return sum;
}

// ---------------------------------------------------------------- worlds and galaxies
static inline uint64_t stand_world_ships(const Standing *s){ return 8 + 6 * (uint64_t)s->worlds; }                  // ships the next world needs (they stay yours)
static inline uint64_t stand_world_cost(const Standing *s, uint64_t unit){ uint64_t n = (uint64_t)s->worlds + 1, g = s->galaxies > 20 ? 20 : s->galaxies; return st_sat_mul(st_sat_mul(unit * 120, n * n), (uint64_t)1 << g); }
static inline uint64_t stand_galaxy_ships(const Standing *s){ uint32_t g = s->galaxies > 20 ? 20 : s->galaxies; return (uint64_t)100 << (2 * g > 40 ? 40 : 2 * g); }
static inline uint64_t stand_galaxy_cost(const Standing *s, uint64_t unit){ uint32_t g = s->galaxies > 20 ? 20 : s->galaxies; uint64_t p = 1; for(uint32_t i = 0; i < g; i++) p = st_sat_mul(p, 3); return st_sat_mul(unit * 40000, p); }
static inline int stand_galaxy_open(const Standing *s){ return s->worlds >= ST_GALAXY_WORLDS; }
// 0 ok, -1 not enough ships, -2 not enough gold, -3 the galaxy is full (cross to the next one), -4 not an armada yet
static inline int stand_settle(Standing *s, int64_t *gold, int32_t *fame_gain, uint64_t unit){
    if(s->ships == 0 || !stand_is_mayor(s)) return -4;
    if(stand_galaxy_open(s)) return -3;
    if(s->ships < stand_world_ships(s)) return -1;
    uint64_t c = stand_world_cost(s, unit); if(*gold < 0 || (uint64_t)*gold < c || c > (uint64_t)INT64_MAX) return -2;
    *gold -= (int64_t)c; s->gold_spent = st_sat_add(s->gold_spent, c); s->worlds++; s->worlds_total++; if(fame_gain) *fame_gain = 20 + 5 * (int32_t)(s->galaxies > 10 ? 10 : s->galaxies);
    return 0;
}
static inline int stand_cross(Standing *s, int64_t *gold, int32_t *fame_gain, uint64_t unit){
    if(!stand_galaxy_open(s)) return -3;
    if(s->ships < stand_galaxy_ships(s)) return -1;
    uint64_t c = stand_galaxy_cost(s, unit); if(*gold < 0 || (uint64_t)*gold < c || c > (uint64_t)INT64_MAX) return -2;
    *gold -= (int64_t)c; s->gold_spent = st_sat_add(s->gold_spent, c); s->worlds = 0; s->galaxies++; if(fame_gain) *fame_gain = 500;
    return 0;
}
// the name of the n-th world you settled (n counts from 0 over all galaxies): the same every time, from the home town's hash
static inline uint64_t st_mix(uint64_t x){ x += 0x9E3779B97F4A7C15ull; x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull; x = (x ^ (x >> 27)) * 0x94D049BB133111EBull; return x ^ (x >> 31); }
static inline void stand_world_name(const Standing *s, uint64_t n, char *o, size_t cap){
    static const char *const cv[] = { "B", "K", "D", "G", "L", "M", "N", "R", "S", "T", "V", "Z", "BR", "KR", "ZH", "TH" };
    static const char *const vw[] = { "A", "E", "I", "O", "U", "AU", "OO", "EA" };
    uint64_t h = st_mix(((uint64_t)s->mayor_town << 20) ^ n ^ 0x57A7D1A6ull); size_t at = 0; int syl = 2 + (int)(h % 2); h >>= 3; if(!cap) return; o[0] = 0;
    for(int i = 0; i < syl; i++){ at = st_cat(o, cap, at, cv[h % 16]); h >>= 4; at = st_cat(o, cap, at, vw[h % 8]); h >>= 3; }
    if(n >= 9 && (h & 1)){ at = st_cat(o, cap, at, " "); at = st_catn(o, cap, at, 2 + (h >> 1) % 8); }
}
// one line about how big this gets, by how far you are
static inline const char *stand_scale_line(const Standing *s){
    if(!stand_is_mayor(s)) return "ONE TOWN. THE SKY HOLDS A LOT MORE.";
    if(s->galaxies >= 3) return "BUGS NOW RULE GALAXIES. THEY ARE STILL VERY SMALL.";
    if(s->galaxies >= 1) return "ONE GALAXY DOWN. THE SKY HAS BILLIONS MORE.";
    if(s->worlds >= ST_GALAXY_WORLDS) return "A GALAXY HAS HUNDREDS OF BILLIONS OF STARS. START AT ONE.";
    if(s->worlds >= 1) return "ONE WORLD IS YOURS. THE GALAXY HAS BILLIONS.";
    if(s->ships >= 1000) return "A THOUSAND SHIPS. THE SKY STILL HAS MORE ROOM.";
    if(s->ships >= 1) return "A SHIP IS A START. THE SKY IS BIG.";
    return "YOU RUN A PLANET. THERE ARE MILLIONS OF THEM.";
}

// ---------------------------------------------------------------- the save record (chunk STND, version 1; the sim file wraps it)
static inline void st_p32(uint8_t *b, uint32_t v){ b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }
static inline void st_p64(uint8_t *b, uint64_t v){ st_p32(b, (uint32_t)v); st_p32(b + 4, (uint32_t)(v >> 32)); }
static inline uint32_t st_g32(const uint8_t *b){ return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static inline uint64_t st_g64(const uint8_t *b){ return st_g32(b) | ((uint64_t)st_g32(b + 4) << 32); }
static inline size_t stand_write(const Standing *s, uint8_t *o, size_t cap){
    if(cap < ST_BYTES) return 0; uint8_t *p = o;
    *p++ = s->used; *p++ = s->flags; *p++ = (uint8_t)s->told; *p++ = (uint8_t)(s->told >> 8);
    st_p32(p, s->mayor_town); p += 4; memcpy(p, s->home, ST_NAME_MAX + 1); p += ST_NAME_MAX + 1;
    st_p64(p, s->mayor_cal); p += 8; st_p64(p, s->tax_cal); p += 8; st_p64(p, s->ships); p += 8; st_p64(p, s->worlds_total); p += 8;
    st_p32(p, s->worlds); p += 4; st_p32(p, s->galaxies); p += 4; st_p32(p, s->tax_rate); p += 4; st_p32(p, s->feasts); p += 4;
    st_p64(p, s->gold_spent); p += 8;
    return (size_t)(p - o);
}
static inline int stand_read(Standing *s, const uint8_t *in, size_t n){          // 0 ok, -1 wrong size
    if(n != ST_BYTES) return -1; const uint8_t *p = in; memset(s, 0, sizeof *s);
    s->used = p[0] ? 1 : 0; s->flags = (uint8_t)(p[1] & 3); s->told = (uint16_t)(p[2] | (p[3] << 8)); s->told &= (uint16_t)((1u << STM_N) - 1); p += 4;
    s->mayor_town = st_g32(p); p += 4; memcpy(s->home, p, ST_NAME_MAX + 1); s->home[ST_NAME_MAX] = 0; p += ST_NAME_MAX + 1;
    for(int i = 0; s->home[i]; i++) if((uint8_t)s->home[i] < 32 || (uint8_t)s->home[i] > 126) s->home[i] = '?';
    s->mayor_cal = st_g64(p); p += 8; s->tax_cal = st_g64(p); p += 8; s->ships = st_g64(p); p += 8; s->worlds_total = st_g64(p); p += 8;
    s->worlds = st_g32(p); p += 4; s->galaxies = st_g32(p); p += 4; s->tax_rate = st_g32(p); p += 4; s->feasts = st_g32(p); p += 4;
    s->gold_spent = st_g64(p);
    if(s->worlds > ST_GALAXY_WORLDS) s->worlds = ST_GALAXY_WORLDS;                // a damaged file cannot give more worlds than a galaxy holds
    if(s->tax_cal < s->mayor_cal) s->tax_cal = s->mayor_cal;
    if(!s->mayor_town){ s->ships = 0; s->worlds = 0; s->worlds_total = 0; s->galaxies = 0; }     // no mayor, no armada
    return 0;
}
#endif
