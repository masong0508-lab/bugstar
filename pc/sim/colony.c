// colony.c: see colony.h
#include "colony.h"
#include "sim_core.h"
#include <string.h>
#include <stdio.h>

static const char *const RN[COLR_N] = { "TOWN", "TENT", "FIELDS", "HOUSES", "WORKSHOPS", "MINE", "WALLS", "MARKET HALL", "MUSEUM" };
static const char *const RH[COLR_N] = { "LAND ON A PLANET", "FREE A CAGED BUG", "CLEAR A DOOR DUNGEON", "BEAT A STORY BOSS", "CLEAR A DRONE OUTPOST",
                                      "GET A PICKAXE", "CLEAN OUT A HIVE", "BEAT THE ANT KING", "FIND A RELIC" };
static const char *const RC[COLR_N] = { "", "A TENT! 2 SETTLERS ARE WAITING.", "FIELDS AND A WELL! 4 SETTLERS.", "HOUSES! YOUR BUG HOME IS READY.",
                                      "WORKSHOPS! THE KITCHEN IS OPEN.", "THE COLONY MINE IS OPEN!", "WALLS AND A GUARD POST!", "A MARKET HALL! 20 SETTLERS.", "A MUSEUM FOR YOUR RELICS!" };
static const char *const JN[COLJ_N] = { "FORAGER", "FARMER", "BUILDER", "COOK", "MINER", "GUARD", "TRADER" };
static const uint8_t CAP[COLR_N] = { 0, 2, 4, 6, 8, 10, 12, 20, 20 };

const char *col_rung_name(int r){ return r >= 0 && r < COLR_N ? RN[r] : "?"; }
const char *col_rung_how(int r){ return r >= 0 && r < COLR_N ? RH[r] : "?"; }
const char *col_rung_card(int r){ return r >= 0 && r < COLR_N ? RC[r] : ""; }
const char *col_job_name(int j){ return j >= 0 && j < COLJ_N ? JN[j] : "?"; }

uint16_t col_mask(const ColFlags *f){
    uint16_t m = 1;
    if(f->bugs_freed >= 1) m |= 1u << COLR_TENT;
    if(f->zone_cleared || f->best_depth >= 3) m |= 1u << COLR_FIELDS;
    if(f->bosses >= 1) m |= 1u << COLR_HOUSES;
    if(f->outposts >= 1) m |= 1u << COLR_SHOPS;
    if(f->ore_tool >= 1) m |= 1u << COLR_MINE;
    if(f->hives >= 1) m |= 1u << COLR_WALLS;
    if(f->ant_king) m |= 1u << COLR_MARKET;
    if(f->relics) m |= 1u << COLR_MUSEUM;
    return m;
}
// The ladder: a rung counts only when the tent is up (rung 1). Settlers follow the highest open rung.
int col_cap(uint16_t mask){ if(!(mask & (1u << COLR_TENT))) return 0; int c = 0; for(int r = 1; r < COLR_N; r++) if(mask & (1u << r)) c = CAP[r] > c ? CAP[r] : c; return c; }
int col_buildings(uint16_t mask){ if(!(mask & (1u << COLR_TENT))) return 0; int b = 0; for(int r = 1; r < COLR_N; r++) if(mask & (1u << r)) b += r == COLR_FIELDS ? 3 : 1; return b; }
int col_next_rung(uint16_t mask){ for(int r = 1; r < COLR_N; r++) if(!(mask & (1u << r))) return r; return 0; }

void col_init(SimColony *c, uint32_t town, uint32_t day){ memset(c, 0, sizeof *c); c->town = town; c->mask = 1; c->told = 1; c->founded = day; c->last_day = day; }

uint16_t col_update(SimColony *c, uint16_t mask){
    c->mask |= mask | 1;
    int cap = col_cap(c->mask); if(c->nset < cap) c->nset = (uint8_t)cap;
    uint16_t nw = (uint16_t)(c->mask & ~c->told);
    if(!(c->mask & (1u << COLR_TENT))) nw = 0;                  // nothing to tell before the tent: the rest waits for it
    c->told |= nw; return nw;
}

void col_settler(const SimColony *c, int i, char *name, int *job){
    sim_name(c->town ^ 0x5E771E5u, (uint32_t)i, name);
    int allowed[COLJ_N], na = 0; allowed[na++] = COLJ_FORAGER;
    if(c->mask & (1u << COLR_FIELDS)) allowed[na++] = COLJ_FARMER;
    if(c->mask & (1u << COLR_TENT))   allowed[na++] = COLJ_BUILDER;
    if(c->mask & (1u << COLR_SHOPS))  allowed[na++] = COLJ_COOK;
    if(c->mask & (1u << COLR_MINE))   allowed[na++] = COLJ_MINER;
    if(c->mask & (1u << COLR_WALLS))  allowed[na++] = COLJ_GUARD;
    if(c->mask & (1u << COLR_MARKET)) allowed[na++] = COLJ_TRADER;
    *job = i == 0 ? COLJ_BUILDER : i == 1 ? COLJ_FORAGER : allowed[sim_rngn(c->town, (uint32_t)i, 77, 0, (uint32_t)na)];
}

static uint32_t sat_add(uint32_t a, uint32_t b){ uint32_t s = a + b; return s < a || s > COL_STORE_MAX ? COL_STORE_MAX : s; }
static void jobs_of(const SimColony *c, uint8_t *jobs){ for(int i = 0; i < c->nset; i++){ char nm[16]; int j; col_settler(c, i, nm, &j); jobs[i] = (uint8_t)j; } }
// One day of work. jobs[] is the settlers' jobs (they depend only on the town, the rungs and the settler, so a long catch-up works them out once).
static void day_with(SimColony *c, uint32_t day, const uint8_t *jobs, uint32_t year_days, ColAway *aw){
    if(!(c->mask & (1u << COLR_TENT))) { c->last_day = day; return; }
    int bmax = col_buildings(c->mask);
    for(int i = 0; i < c->nset; i++){
        uint32_t luck = sim_rngn(c->town, (uint32_t)i, 78, day, 4);
        switch(jobs[i]){
        case COLJ_FORAGER: c->food = sat_add(c->food, 2 + luck / 2); break;
        case COLJ_FARMER:  c->food = sat_add(c->food, 4 + luck); break;
        case COLJ_BUILDER: c->wood = sat_add(c->wood, 2 + luck / 2);
            if(c->built < bmax && c->wood >= 10u * (c->built + 1)){ c->wood -= 10u * (c->built + 1); c->built++; if(aw && aw->nbuilt < 4) aw->built_day[aw->nbuilt++] = day; } break;
        case COLJ_COOK:    if(c->food >= 3){ c->food -= 3; c->goods = sat_add(c->goods, 2); } break;
        case COLJ_MINER:   c->ore = sat_add(c->ore, 1 + luck / 2); break;
        case COLJ_GUARD:   break;
        case COLJ_TRADER:  if(c->goods >= 2){ c->goods -= 2; c->ore = sat_add(c->ore, 1); c->wood = sat_add(c->wood, 2); } break;
        }
    }
    uint32_t eat = (c->nset + 1u) / 2u; c->food = c->food > eat ? c->food - eat : 0;             // everybody eats
    if(year_days && day % year_days == 0 && c->nset){                                 // a year ends: babies are born, the old retire (the colony keeps its size)
        uint32_t b = sim_rngn(c->town, day / year_days, 79, 0, 1u + c->nset / 4u) + (c->food >= 10u * c->nset ? 1u : 0u);
        c->born = sat_add(c->born, b); if(aw) aw->born = sat_add(aw->born, b);
    }
    c->last_day = day;
}
void col_day(SimColony *c, uint32_t day){ uint8_t jobs[COL_MAXSET]; jobs_of(c, jobs); day_with(c, day, jobs, COL_YEAR_DAYS, 0); }
uint32_t col_year_days(int season_days){ return season_days > 0 ? (uint32_t)season_days * 4u : COL_YEAR_DAYS; }
int col_catchup_ex(SimColony *c, uint32_t today, uint32_t year_days, ColAway *aw){
    if(aw){ memset(aw, 0, sizeof *aw); aw->food0 = c->food; aw->wood0 = c->wood; aw->ore0 = c->ore; aw->goods0 = c->goods; aw->built0 = c->built; }
    if(today <= c->last_day) return 0;
    if(!year_days) year_days = COL_YEAR_DAYS;
    uint64_t span = (uint64_t)COL_CATCHUP_YEARS * year_days;
    uint32_t from = c->last_day + 1; if((uint64_t)today - from + 1 > span) from = (uint32_t)(today - span + 1);
    uint8_t jobs[COL_MAXSET]; jobs_of(c, jobs);
    int n = 0; for(uint32_t d = from; d <= today; d++){ day_with(c, d, jobs, year_days, aw); n++; }
    c->last_day = today;
    if(aw){ aw->days = (uint32_t)n; aw->years = (uint32_t)n / year_days; aw->settlers = c->nset; }
    return n;
}
int col_catchup(SimColony *c, uint32_t today){ return col_catchup_ex(c, today, COL_YEAR_DAYS, 0); }
int col_away_text(const ColAway *a, const SimColony *c, char lines[][48], int max){
    int n = 0; if(max < 1 || !a->days) return 0;
    #define L(...) do{ if(n < max){ snprintf(lines[n], 48, __VA_ARGS__); n++; } }while(0)
    if(a->years) L("%u YEARS AND %u DAYS WENT BY", a->years, a->days % (a->years ? a->days / a->years : 1)); else L("%u DAYS WENT BY", a->days);
    L("%u BORN. %u SETTLERS LIVE HERE", a->born, a->settlers);
    if(c->built != a->built0) L("BUILDINGS %u -> %u", a->built0, c->built);
    L("FOOD %u  WOOD %u  ORE %u  GOODS %u", c->food, c->wood, c->ore, c->goods);
    for(int i = 0; i < a->nbuilt && n < max; i++) L("DAY %u: A BUILDING WAS FINISHED", a->built_day[i]);
    #undef L
    return n;
}
size_t col_dump(const SimColony *c, char *o, size_t n){
    int k = snprintf(o, n, "colony town %08x mask %04x told %04x settlers %u built %u food %u wood %u ore %u goods %u day %u born %u\n",
                     c->town, c->mask, c->told, c->nset, c->built, c->food, c->wood, c->ore, c->goods, c->last_day, c->born);
    return k < 0 ? 0 : (size_t)k;
}

SimColony *col_find(ColList *l, uint32_t town, int make, uint32_t day){
    for(int i = 0; i < l->n; i++) if(l->c[i].town == town) return &l->c[i];
    if(!make) return 0;
    int at = l->n;
    if(l->n >= COL_MAX){ at = 0; for(int i = 1; i < l->n; i++) if(l->c[i].last_day < l->c[at].last_day) at = i; }   // full: the one visited longest ago goes
    else l->n++;
    col_init(&l->c[at], town, day); return &l->c[at];
}
static void p16(uint8_t *p, uint32_t v){ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *p, uint32_t v){ p16(p, v); p16(p + 2, v >> 16); }
static uint32_t g16(const uint8_t *p){ return p[0] | (uint32_t)p[1] << 8; }
static uint32_t g32(const uint8_t *p){ return g16(p) | g16(p + 2) << 16; }
size_t col_list_write(const ColList *l, uint8_t *o, size_t cap){
    size_t need = 1 + (size_t)l->n * COL_REC_BYTES; if(cap < need) return 0;
    o[0] = l->n;
    for(int i = 0; i < l->n; i++){ const SimColony *c = &l->c[i]; uint8_t *q = o + 1 + i * COL_REC_BYTES;
        p32(q, c->town); p16(q + 4, c->mask); p16(q + 6, c->told); q[8] = c->nset; q[9] = c->built;
        p32(q + 10, c->food); p32(q + 14, c->wood); p32(q + 18, c->ore); p32(q + 22, c->goods); p32(q + 26, c->founded); p32(q + 30, c->last_day); p32(q + 34, c->born); p32(q + 38, 0); }
    return need;
}
int col_list_read(ColList *l, const uint8_t *in, size_t n){
    memset(l, 0, sizeof *l);
    if(n < 1 || in[0] > COL_MAX) return -1;
    size_t rb = n == 1 + (size_t)in[0] * COL_REC_BYTES ? COL_REC_BYTES : n == 1 + (size_t)in[0] * 34 ? 34 : 0;     // 34: the first (Phase 4) records, without births
    if(!rb) return -1;
    l->n = in[0];
    for(int i = 0; i < l->n; i++){ SimColony *c = &l->c[i]; const uint8_t *q = in + 1 + i * rb; if(rb >= 38) c->born = g32(q + 34);
        c->town = g32(q); c->mask = (uint16_t)(g16(q + 4) & ((1u << COLR_N) - 1)) | 1; c->told = (uint16_t)(g16(q + 6) & c->mask);
        c->nset = q[8] > COL_MAXSET ? COL_MAXSET : q[8]; c->built = q[9];
        c->food = g32(q + 10) > COL_STORE_MAX ? COL_STORE_MAX : g32(q + 10); c->wood = g32(q + 14) > COL_STORE_MAX ? COL_STORE_MAX : g32(q + 14); c->ore = g32(q + 18) > COL_STORE_MAX ? COL_STORE_MAX : g32(q + 18); c->goods = g32(q + 22) > COL_STORE_MAX ? COL_STORE_MAX : g32(q + 22); c->founded = g32(q + 26); c->last_day = g32(q + 30);
        if(c->nset > col_cap(c->mask)) c->nset = (uint8_t)col_cap(c->mask);
        if(c->built > col_buildings(c->mask)) c->built = (uint8_t)col_buildings(c->mask); }
    return 0;
}

int col_collect(SimColony *c, int hunger_room, ColGift *g){
    memset(g, 0, sizeof *g);
    if(!(c->mask & (1u << COLR_TENT))) return 0;
    if(hunger_room < 0) hunger_room = 0;
    uint32_t want = (uint32_t)hunger_room * 2; if(want > c->food) want = c->food; want &= ~1u;
    g->food_used = want; g->food_pts = (int)(want / 2); c->food -= want;
    g->goods_sold = c->goods; g->ore_sold = c->ore; g->gold = (int)(c->goods * 5 + c->ore * 3); c->goods = 0; c->ore = 0;
    g->heal = (c->mask >> COLR_HOUSES) & 1;
    return g->food_pts || g->gold || g->heal;
}
