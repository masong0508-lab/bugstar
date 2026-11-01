// colony.c: see colony.h
#include "colony.h"
#include "sim_core.h"
#include <string.h>
#include <stdio.h>

static const char *const RN[COLR_N] = { "TOWN", "TENT", "FIELDS", "HOUSES", "WORKSHOPS", "MINE", "WALLS", "MARKET HALL", "MUSEUM" };
static const char *const RH[COLR_N] = { "LAND ON A PLANET", "FREE A CAGED BUG", "CLEAR A DOOR DUNGEON", "BEAT A STORY BOSS", "CLEAR A DRONE OUTPOST",
                                        "GET A PICKAXE", "CLEAN OUT A HIVE", "BEAT THE ANT KING", "FIND A RELIC" };
static const char *const RG[COLR_N] = { "", "OR MAKE A FRIEND IN TOWN, OR LEVEL 10", "OR STORE 60 FOOD, OR 2 BUILDINGS", "OR 3 BABIES BORN, OR 2 FIELDS",
                                        "OR STORE 100 WOOD, OR 2 HOUSES", "OR 6 BUILDINGS, OR 2 YEARS", "OR BEAT RAIDERS, OR 6 SETTLERS",
                                        "OR STORE 40 GOODS, OR 10 SETTLERS", "OR 10 YEARS, OR BEAT 3 RAIDS" };
static const char *const RC[COLR_N] = { "", "A TENT! 2 SETTLERS ARE WAITING.", "FIELDS AND A WELL! 4 SETTLERS.", "HOUSES! YOUR BUG HOME IS READY.",
                                        "WORKSHOPS! THE KITCHEN IS OPEN.", "THE COLONY MINE IS OPEN!", "WALLS AND A GUARD POST!", "A MARKET HALL! 20 SETTLERS.", "A MUSEUM FOR YOUR RELICS!" };
static const char *const JN[COLJ_N] = { "FORAGER", "FARMER", "BUILDER", "COOK", "MINER", "GUARD", "TRADER" };
static const char *const BN[COLB_N] = { "HUT", "FIELD", "WELL", "STORE", "HOUSE", "KITCHEN", "MINE", "WALL", "TOWER", "MARKET", "MUSEUM" };
static const uint8_t BR[COLB_N] = { COLR_TENT, COLR_FIELDS, COLR_FIELDS, COLR_FIELDS, COLR_HOUSES, COLR_SHOPS, COLR_MINE, COLR_WALLS, COLR_WALLS, COLR_MARKET, COLR_MUSEUM };
static const uint8_t CAP[COLR_N] = { 0, 2, 4, 6, 8, 10, 12, 20, 20 };
static const uint8_t ARRIVE[COLR_N] = { 0, 2, 2, 1, 1, 1, 1, 2, 0 };     // settlers who come when a rung opens (the rest are born)
enum { RP_JOB = 77, RP_LUCK, RP_BIRTH, RP_RAID, RP_RAIDSZ, RP_SICK };

const char *col_rung_name(int r){ return r >= 0 && r < COLR_N ? RN[r] : "?"; }
const char *col_rung_how(int r){ return r >= 0 && r < COLR_N ? RH[r] : "?"; }
const char *col_rung_goal(int r){ return r >= 0 && r < COLR_N ? RG[r] : ""; }
const char *col_rung_card(int r){ return r >= 0 && r < COLR_N ? RC[r] : ""; }
const char *col_job_name(int j){ return j >= 0 && j < COLJ_N ? JN[j] : "?"; }
const char *col_bld_name(int b){ return b >= 0 && b < COLB_N ? BN[b] : "?"; }
int col_bld_rung(int b){ return b >= 0 && b < COLB_N ? BR[b] : COLR_N; }

static uint32_t sat_add(uint32_t a, uint32_t b){ uint64_t s = (uint64_t)a + b; return s > COL_STORE_MAX ? COL_STORE_MAX : (uint32_t)s; }
static uint64_t sat_add64(uint64_t a, uint64_t b){ uint64_t s = a + b; return s < a || s > COL_GOLD_MAX ? COL_GOLD_MAX : s; }
static uint64_t sat_mul64(uint64_t a, uint64_t b){ if(a && b > COL_GOLD_MAX / a) return COL_GOLD_MAX; return a * b; }

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
int col_built_total(const SimColony *c){ int n = 0; for(int b = 0; b < COLB_N; b++) n += c->bld[b]; return n; }
uint16_t col_goal_mask(const SimColony *c, const ColFlags *f, uint32_t today, uint32_t year_days){
    uint16_t m = 1; uint32_t age = today > c->founded ? (today - c->founded) / (year_days ? year_days : COL_YEAR_DAYS) : 0;
    if(f && (f->friends >= 1 || f->level >= 10)) m |= 1u << COLR_TENT;
    if(!(c->mask & (1u << COLR_TENT)) || c->state != COLS_ALIVE) return m;   // the other goals need a living colony
    if(c->food >= 60 || col_built_total(c) >= 2) m |= 1u << COLR_FIELDS;
    if(c->born >= 3 || c->bld[COLB_FIELD] >= 2) m |= 1u << COLR_HOUSES;
    if(c->wood >= 100 || c->bld[COLB_HOUSE] >= 2) m |= 1u << COLR_SHOPS;
    if(col_built_total(c) >= 6 || age >= 2) m |= 1u << COLR_MINE;
    if(c->raids_won >= 1 || c->nset >= 6) m |= 1u << COLR_WALLS;
    if(c->goods >= 40 || c->nset >= 10) m |= 1u << COLR_MARKET;
    if(age >= 10 || c->raids_won >= 3) m |= 1u << COLR_MUSEUM;
    return m;
}
int col_cap(uint16_t mask){ if(!(mask & (1u << COLR_TENT))) return 0; int c = 0; for(int r = 1; r < COLR_N; r++) if(mask & (1u << r)) c = CAP[r] > c ? CAP[r] : c; return c; }
int col_buildings(uint16_t mask){ if(!(mask & (1u << COLR_TENT))) return 0; int n = 0; for(int b = 0; b < COLB_N; b++) if(mask & (1u << BR[b])) n++; return n; }
int col_next_rung(uint16_t mask){ for(int r = 1; r < COLR_N; r++) if(!(mask & (1u << r))) return r; return 0; }
int col_bld_cost(const SimColony *c, int b){ return 10 + 6 * (b >= 0 && b < COLB_N ? c->bld[b] : 0) + (b == COLB_MUSEUM || b == COLB_MARKET ? 30 : 0); }
static int grid_free(const SimColony *c){ int n = 0; for(int i = 0; i < COL_GRID_W * COL_GRID_H; i++) if(!c->grid[i]) n++; return n; }
int col_bld_ok(const SimColony *c, int b){ return b >= 0 && b < COLB_N && (c->mask >> BR[b] & 1) && c->bld[b] < 9 && grid_free(c) > 0 && c->state == COLS_ALIVE; }
int col_defence(const SimColony *c){
    int d = c->nset + c->bld[COLB_WALL] * 5 + c->bld[COLB_TOWER] * 4;
    for(int i = 0; i < c->nset; i++){ char nm[16]; int j; col_settler(c, i, nm, &j); if(j == COLJ_GUARD) d += 3; }
    if(c->help_days) d += 10;
    if(c->god_used) d += 15;
    return d;
}

void col_init(SimColony *c, uint32_t town, uint32_t day){
    memset(c, 0, sizeof *c); c->town = town; c->mask = 1; c->told = 1; c->founded = day; c->last_day = day;
    c->health = 80; c->morale = 70; c->plan = COLB_HUT; c->plan_cell = 0xFF; c->danger = 2;
    memset(c->job, 0xFF, sizeof c->job);
}

uint16_t col_update(SimColony *c, uint16_t mask){
    if(c->state != COLS_ALIVE) return 0;
    uint16_t before = c->mask; c->mask |= mask | 1;
    uint16_t opened = (uint16_t)(c->mask & ~before);
    int cap = col_cap(c->mask);
    if(c->mask & (1u << COLR_TENT)){
        if(opened & (1u << COLR_TENT)) c->founded = c->last_day;
        if(opened & (1u << COLR_TENT)) opened = c->mask;           // the tent: everyone waiting for the rungs already open arrives now
        for(int r = 1; r < COLR_N; r++) if(opened & (1u << r)){ int n = c->nset + ARRIVE[r]; if(r == COLR_TENT && n < 2) n = 2; c->nset = (uint8_t)(n > cap ? cap : n); }
    }
    uint16_t nw = (uint16_t)(c->mask & ~c->told);
    if(!(c->mask & (1u << COLR_TENT))) nw = 0;
    c->told |= nw; return nw;
}

void col_settler(const SimColony *c, int i, char *name, int *job){
    sim_name(c->town ^ 0x5E771E5u, (uint32_t)i, name);
    if(i >= 0 && i < COL_MAXSET && c->job[i] < COLJ_N){ *job = c->job[i]; return; }
    int allowed[COLJ_N], na = 0; allowed[na++] = COLJ_FORAGER;
    if(c->mask & (1u << COLR_FIELDS)) allowed[na++] = COLJ_FARMER;
    if(c->mask & (1u << COLR_TENT))   allowed[na++] = COLJ_BUILDER;
    if(c->mask & (1u << COLR_SHOPS))  allowed[na++] = COLJ_COOK;
    if(c->mask & (1u << COLR_MINE))   allowed[na++] = COLJ_MINER;
    if(c->mask & (1u << COLR_WALLS))  allowed[na++] = COLJ_GUARD;
    if(c->mask & (1u << COLR_MARKET)) allowed[na++] = COLJ_TRADER;
    if(i >= 1 && (c->hungry_days || c->food < c->nset * 4u)){ *job = (c->mask & (1u << COLR_FIELDS)) ? COLJ_FARMER : COLJ_FORAGER; return; }   // hungry: hands to the fields
    *job = i == 0 ? COLJ_BUILDER : i == 1 ? COLJ_FORAGER : allowed[sim_rngn(c->town, (uint32_t)i, RP_JOB, 0, (uint32_t)na)];
}

static void ev(ColAway *aw, int kind, uint32_t a, uint32_t b, uint32_t day){
    if(!aw) return;
    if(aw->nev == 12){ memmove(aw->ev, aw->ev + 1, sizeof aw->ev[0] * 11); aw->nev = 11; }
    aw->ev[aw->nev].kind = (uint8_t)kind; aw->ev[aw->nev].a = a; aw->ev[aw->nev].b = b; aw->ev[aw->nev].day = day; aw->nev++;
}
static int shelter_of(const SimColony *c){ return 2 + c->bld[COLB_HUT] * 2 + c->bld[COLB_HOUSE] * 4; }
static void auto_plan(SimColony *c){                        // the colony picks what to build when the player has not: what it lacks most
    int want = -1;
    if(c->nset >= shelter_of(c)) want = col_bld_ok(c, COLB_HOUSE) ? COLB_HOUSE : COLB_HUT;
    else if(c->raid_eta && col_bld_ok(c, COLB_WALL)) want = COLB_WALL;
    else if(c->food < 20u + c->nset * 2u && col_bld_ok(c, COLB_FIELD)) want = COLB_FIELD;
    else if(c->health < 60 && col_bld_ok(c, COLB_WELL) && c->bld[COLB_WELL] < 2) want = COLB_WELL;
    if(want >= 0 && col_bld_ok(c, want)){ c->plan = (uint8_t)want; c->plan_cell = 0xFF; c->progress = 0; return; }
    int best = -1; for(int b = 0; b < COLB_N; b++) if(col_bld_ok(c, b) && (best < 0 || c->bld[b] < c->bld[best])) best = b;
    c->plan = (uint8_t)(best < 0 ? COLB_HUT : best); c->plan_cell = 0xFF; c->progress = 0;
}
static void finish_building(SimColony *c, uint32_t day, ColAway *aw){
    int b = c->plan, cell = c->plan_cell;
    if(cell >= COL_GRID_W * COL_GRID_H || c->grid[cell]){ cell = -1; for(int i = 0; i < COL_GRID_W * COL_GRID_H; i++) if(!c->grid[i]){ cell = i; break; } }
    if(cell < 0) return;
    c->grid[cell] = (uint8_t)(b + 1); c->bld[b]++; ev(aw, COLE_BUILT, (uint32_t)b, (uint32_t)cell, day); if(aw) aw->nbuilt++;
    auto_plan(c);
}
static void kill(SimColony *c, uint32_t n, uint32_t day, ColAway *aw){
    if(n > c->nset) n = c->nset;
    if(!n) return;
    c->nset = (uint8_t)(c->nset - n); c->died = sat_add(c->died, n); if(aw) aw->died = sat_add(aw->died, n); ev(aw, COLE_DIED, n, c->nset, day);
    for(int i = c->nset; i < COL_MAXSET; i++) c->job[i] = 0xFF;
    if(!c->nset){ c->state = COLS_RUINS; c->raid_eta = 0; ev(aw, COLE_RUINS, 0, 0, day); if(aw) aw->ruined = 1; }
}
uint64_t col_reward(uint32_t size, int danger, int no_deaths, int no_god, int brink){
    if(danger < 0) danger = 0;
    if(danger > 9) danger = 9;
    uint64_t s = size, g = sat_mul64(sat_mul64(s, s), (uint64_t)(100 + 50 * danger));     // super-linear in the horde's size
    if(size > 50) g = sat_mul64(g, s / 50 + 1);                                            // and steeper still for the great hordes
    if(no_deaths) g = sat_add64(g, g / 4);
    if(no_god) g = sat_add64(g, g / 4);
    if(brink) g = sat_mul64(g, 2);
    return g;
}
static int g_catchup;          // set while col_catchup_d runs: battles fought while you were away pay half
static void raid_fight(SimColony *c, uint32_t day, int diff, ColAway *aw, int catchup){
    uint32_t size = c->raid_size, def = (uint32_t)col_defence(c); c->raid_eta = 0;
    int brink = c->health < 30 || c->nset <= 2;
    if(def >= size || (diff == COLD_CALM && c->nset)){                                   // repelled (on CALM the colony always holds)
        uint32_t lost = (def >= size + size / 2 || diff == COLD_CALM) ? 0 : (size + size / 2 - def) / 6; if(lost >= c->nset) lost = c->nset ? c->nset - 1u : 0;
        if(lost) kill(c, lost, day, aw);
        uint64_t r = col_reward(size, c->danger, lost == 0, !c->god_used, brink); if(catchup) r /= 2;     // battles fought while you were away pay half
        c->reward_due = sat_add64(c->reward_due, r); c->raids_won++; if(aw){ aw->raids_won++; aw->reward = sat_add64(aw->reward, r); }
        ev(aw, COLE_RAIDWON, size, (uint32_t)(r > 0xFFFFFFFFull ? 0xFFFFFFFFu : r), day); c->morale = (uint8_t)(c->morale > 85 ? 100 : c->morale + 15);
    } else {
        uint32_t lost = (size - def) / 2 + 1; c->raids_lost++; if(aw) aw->raids_lost++;
        ev(aw, COLE_RAIDLOST, size, lost, day);
        int burn = (int)((size - def) / 8);
        for(int b = COLB_N - 1; b >= 0 && burn > 0; b--) while(c->bld[b] && burn > 0){        // raiders burn buildings
            c->bld[b]--; burn--; for(int i = COL_GRID_W * COL_GRID_H - 1; i >= 0; i--) if(c->grid[i] == b + 1){ c->grid[i] = 0; break; } }
        c->food /= 2; c->goods /= 2; c->morale = c->morale > 30 ? c->morale - 30 : 0;
        int was_dying = c->nset <= lost || c->health < 20;
        kill(c, lost, day, aw);
        uint32_t limit = diff == COLD_HARSH ? 60 : 200;                                    // the extreme path: the planet itself is lost
        if(c->world_ok && diff != COLD_CALM && size >= limit && (was_dying || diff == COLD_HARSH)){
            c->state = COLS_DEAD_WORLD; c->nset = 0; c->raid_eta = 0; c->food = c->wood = c->goods = 0; c->hungry_days = 0; ev(aw, COLE_WORLDEND, size, 0, day); if(aw) aw->world_end = 1; }
    }
    c->god_used = 0;
}
void col_day_ex(SimColony *c, uint32_t day, uint32_t year_days, int diff, ColAway *aw){
    if(c->state != COLS_ALIVE || !(c->mask & (1u << COLR_TENT))){ c->last_day = day; return; }
    if(!year_days) year_days = COL_YEAR_DAYS;
    uint32_t mood = c->morale >= 40 ? 1 : 0;                                          // unhappy settlers work less
    uint32_t wood_in = 0;
    for(int i = 0; i < c->nset; i++){
        char nm[16]; int j; col_settler(c, i, nm, &j);
        uint32_t luck = sim_rngn(c->town, (uint32_t)i, RP_LUCK, day, 4);
        switch(j){
        case COLJ_FORAGER: c->food = sat_add(c->food, 2 + luck / 2 + mood); break;
        case COLJ_FARMER:  c->food = sat_add(c->food, 3 + luck / 2 + (uint32_t)c->bld[COLB_FIELD] * 2 + mood); break;
        case COLJ_BUILDER: wood_in += 2 + luck / 2 + mood; break;
        case COLJ_COOK:    if(c->food >= 3u + c->nset * 2u){ c->food -= 3; c->goods = sat_add(c->goods, 2 + (uint32_t)c->bld[COLB_KITCHEN]); } break;
        case COLJ_MINER:   c->ore = sat_add(c->ore, 1 + luck / 2 + (uint32_t)c->bld[COLB_MINE]); break;
        case COLJ_GUARD:   break;
        case COLJ_TRADER:  if(c->goods >= 2){ c->goods -= 2; c->ore = sat_add(c->ore, 1); c->wood = sat_add(c->wood, 2 + (uint32_t)c->bld[COLB_MARKET]); } break;
        }
    }
    c->wood = sat_add(c->wood, wood_in);
    if(c->help_days) c->help_days--;
    // building: wood from the store goes into the plan
    if(!col_bld_ok(c, c->plan)) auto_plan(c);
    if(col_bld_ok(c, c->plan)){ uint32_t need = (uint32_t)col_bld_cost(c, c->plan), have = c->progress, put = need > have ? need - have : 0;
        if(put > c->wood) put = c->wood;
        if(put > wood_in + 4) put = wood_in + 4;
        c->wood -= put; c->progress = (uint16_t)(c->progress + put); if(c->progress >= need) finish_building(c, day, aw); }
    // care: food, shelter, health, morale
    uint32_t eat = (c->nset + 1u) / 2u;
    if(c->food >= eat){ c->food -= eat; c->hungry_days = 0; } else { c->food = 0; c->hungry_days++; }
    int shelter = shelter_of(c);
    if(c->hungry_days){ c->health = c->health > 6 ? c->health - 6 : 0; c->morale = c->morale > 4 ? c->morale - 4 : 0; }
    else if(c->health < 100) c->health = (uint8_t)(c->health + 1 + (c->bld[COLB_WELL] ? 1 : 0));
    if(c->health > 100) c->health = 100;
    if(c->nset > shelter){ c->morale = c->morale > 2 ? c->morale - 2 : 0; if(day % 7 == 0) ev(aw, COLE_CROWDED, (uint32_t)c->nset, (uint32_t)shelter, day); }
    else if(c->morale < 100) c->morale++;
    if(!c->hungry_days && eat && c->food < eat * 3 && day % 3 == 0) ev(aw, COLE_FOODWARN, c->food / eat + 1, 0, day);
    if(c->health < 30 && sim_rngn(c->town, 0, RP_SICK, day, 10) < 2){                   // sickness
        if(diff != COLD_CALM) kill(c, 1, day, aw); else ev(aw, COLE_SICK, 0, 0, day); }
    if(c->state != COLS_ALIVE){ c->last_day = day; return; }
    // raids: the first a year after the tent; warnings 12, 3 and 1 days before
    uint32_t age = day > c->founded ? day - c->founded : 0;
    if(c->raid_eta){
        c->raid_eta--; if(c->raid_eta == 3 || c->raid_eta == 1) ev(aw, COLE_RAIDWARN, c->raid_size, c->raid_eta, day);
        if(!c->raid_eta) raid_fight(c, day, diff, aw, g_catchup);
    } else if(age >= year_days && sim_rngn(c->town, 0, RP_RAID, day, 30) == 0){
        uint32_t yrs = age / year_days;
        c->raid_size = (uint16_t)(2 + (yrs > 40 ? 40 : yrs) / 2 + c->danger + sim_rngn(c->town, 1, RP_RAIDSZ, day, 4u + c->danger * 2u));
        c->raid_eta = 12; ev(aw, COLE_RAIDWARN, c->raid_size, 12, day);
    }
    if(c->state != COLS_ALIVE){ c->last_day = day; return; }
    // a year ends: babies (when fed and housed); settlers grow to what the rungs allow
    if(day % year_days == 0 && c->nset){
        int cap = col_cap(c->mask); uint32_t b = (c->hungry_days || c->nset > shelter) ? 0 : 1 + sim_rngn(c->town, day / year_days, RP_BIRTH, 0, 1u + c->nset / 3u);
        c->born = sat_add(c->born, b); if(aw) aw->born = sat_add(aw->born, b);
        uint32_t n = c->nset + b; c->nset = (uint8_t)(n > (uint32_t)cap ? (uint32_t)cap : n); if(b) ev(aw, COLE_BORN, b, c->nset, day);
    }
    c->last_day = day;
}
void col_day(SimColony *c, uint32_t day){ col_day_ex(c, day, COL_YEAR_DAYS, COLD_NORMAL, 0); }
uint32_t col_year_days(int season_days){ return season_days > 0 ? (uint32_t)season_days * 4u : COL_YEAR_DAYS; }
int col_catchup_d(SimColony *c, uint32_t today, uint32_t year_days, int diff, ColAway *aw){
    if(aw){ memset(aw, 0, sizeof *aw); aw->built0 = aw->built1 = (uint32_t)col_built_total(c); aw->settlers = c->nset; }
    if(today <= c->last_day) return 0;
    if(!year_days) year_days = COL_YEAR_DAYS;
    uint64_t span = (uint64_t)COL_CATCHUP_YEARS * year_days;
    uint32_t from = c->last_day + 1; if((uint64_t)today - from + 1 > span) from = (uint32_t)(today - span + 1);
    int n = 0; g_catchup = 1; for(uint32_t d = from; d <= today; d++){ col_day_ex(c, d, year_days, diff, aw); n++; } g_catchup = 0;
    c->last_day = today;
    if(aw){ aw->days = (uint32_t)n; aw->years = (uint32_t)n / year_days; aw->settlers = c->nset; aw->built1 = (uint32_t)col_built_total(c); }
    return n;
}
int col_catchup_ex(SimColony *c, uint32_t today, uint32_t year_days, ColAway *aw){ return col_catchup_d(c, today, year_days, COLD_NORMAL, aw); }
int col_catchup(SimColony *c, uint32_t today){ return col_catchup_d(c, today, COL_YEAR_DAYS, COLD_NORMAL, 0); }

static void commas(uint64_t v, char *o){ char t[32]; int n = 0, k = 0; do { if(k && k % 3 == 0) t[n++] = ','; t[n++] = (char)('0' + v % 10); v /= 10; k++; } while(v); for(int i = 0; i < n; i++) o[i] = t[n - 1 - i]; o[n] = 0; }
int col_event_text(const ColEvent *e, char *o, size_t n){
    char g[32]; int k = 0;
    switch(e->kind){
    case COLE_BUILT: k = snprintf(o, n, "A %s WAS BUILT", col_bld_name((int)e->a)); break;
    case COLE_BORN: k = snprintf(o, n, "%u BABIES WERE BORN", e->a); break;
    case COLE_DIED: k = snprintf(o, n, "%u SETTLERS DIED. %u LEFT", e->a, e->b); break;
    case COLE_FOODWARN: k = snprintf(o, n, "FOOD RUNS OUT IN %u DAYS", e->a); break;
    case COLE_RAIDWARN: k = snprintf(o, n, "A HORDE OF %u GATHERS: %u DAYS AWAY", e->a, e->b); break;
    case COLE_RAIDWON: commas(e->b, g); k = snprintf(o, n, "RAID OF %u REPELLED! +%sG", e->a, g); break;
    case COLE_RAIDLOST: k = snprintf(o, n, "RAIDERS (%u) BROKE IN", e->a); break;
    case COLE_RUINS: k = snprintf(o, n, "THE COLONY IS IN RUINS"); break;
    case COLE_WORLDEND: k = snprintf(o, n, "THE PLANET WAS DESTROYED"); break;
    case COLE_SICK: k = snprintf(o, n, "SICKNESS IN THE COLONY"); break;
    case COLE_CROWDED: k = snprintf(o, n, "TOO CROWDED: BUILD HOUSES"); break;
    case COLE_RUNG: k = snprintf(o, n, "%s OPENED", col_rung_name((int)e->a)); break;
    default: k = snprintf(o, n, "..."); }
    return k < 0 ? 0 : k;
}
int col_away_text(const ColAway *a, const SimColony *c, char lines[][48], int max){
    int n = 0; if(max < 1 || !a->days) return 0;
    #define L(...) do{ if(n < max){ snprintf(lines[n], 48, __VA_ARGS__); n++; } }while(0)
    if(a->years) L("%u YEARS WENT BY", a->years); else L("%u DAYS WENT BY", a->days);
    if(a->world_end) L("THE PLANET WAS DESTROYED");
    else if(a->ruined) L("THE COLONY FELL INTO RUINS");
    L("%u BORN, %u DIED. %u LIVE HERE", a->born, a->died, c->nset);
    if(a->built1 != a->built0) L("BUILDINGS %u -> %u", a->built0, a->built1);
    if(a->raids_won || a->raids_lost){ char g[32]; commas(a->reward, g); L("RAIDS %u WON %u LOST +%sG", a->raids_won, a->raids_lost, g); }
    L("FOOD %u WOOD %u ORE %u", c->food, c->wood, c->ore);
    for(int i = a->nev - 1; i >= 0 && n < max; i--){ int k = a->ev[i].kind; if(k == COLE_RAIDWON || k == COLE_RAIDLOST || k == COLE_RUINS || k == COLE_WORLDEND){ char t[48]; col_event_text(&a->ev[i], t, sizeof t); L("%s", t); } }
    #undef L
    return n;
}

// ---- hands-on
int col_set_job(SimColony *c, int i, int job){ if(i < 0 || i >= c->nset || job >= COLJ_N) return 0; c->job[i] = job < 0 ? 0xFF : (uint8_t)job; return 1; }
int col_set_plan(SimColony *c, int b, int cell){
    if(!col_bld_ok(c, b)) return 0;
    if(cell >= 0 && (cell >= COL_GRID_W * COL_GRID_H || c->grid[cell])) return 0;
    if(b != c->plan) c->progress = 0;
    c->plan = (uint8_t)b; c->plan_cell = cell < 0 ? 0xFF : (uint8_t)cell; return 1;
}
int col_help_build(SimColony *c){
    if(c->state != COLS_ALIVE || !(c->mask & (1u << COLR_TENT))) return 0;
    if(!col_bld_ok(c, c->plan)) auto_plan(c);
    if(!col_bld_ok(c, c->plan)) return 0;
    uint32_t add = c->wood >= 15 ? 15 : 5; if(c->wood >= 15) c->wood -= 15;
    c->progress = (uint16_t)(c->progress + add); if(c->progress >= (uint32_t)col_bld_cost(c, c->plan)) finish_building(c, c->last_day, 0);
    return 1;
}
int col_carry_food(SimColony *c, int amount){ if(c->state != COLS_ALIVE || amount <= 0) return 0; c->food = sat_add(c->food, (uint32_t)amount * 2); c->hungry_days = 0; return 1; }
int col_help_guard(SimColony *c){ if(c->state != COLS_ALIVE) return 0; c->help_days = 3; return 1; }
int col_god_help(SimColony *c, int kind){
    if(c->state != COLS_ALIVE) return 0;
    if(kind == 0){ c->food = sat_add(c->food, 60); c->hungry_days = 0; }
    else if(kind == 1){ c->health = 100; c->morale = (uint8_t)(c->morale > 80 ? 100 : c->morale + 20); }
    else c->god_used = 1;
    return 1;
}
int col_resettle(SimColony *c, uint32_t day){
    if(c->state != COLS_RUINS) return 0;
    c->state = COLS_ALIVE; c->nset = 2; c->health = 70; c->morale = 60; c->hungry_days = 0; c->raid_eta = 0; c->founded = day; c->last_day = day;
    memset(c->job, 0xFF, sizeof c->job); return 1;
}
int col_carry_wood(SimColony *c, int cell){            // the map: you carry wood to a cell and build there
    if(c->state != COLS_ALIVE || !(c->mask & (1u << COLR_TENT)) || cell < 0 || cell >= COL_GRID_W * COL_GRID_H || c->grid[cell]) return 0;
    if(!col_bld_ok(c, c->plan)) auto_plan(c);
    if(!col_bld_ok(c, c->plan)) return 0;
    c->plan_cell = (uint8_t)cell;
    uint32_t add = 4; if(c->wood >= 8){ c->wood -= 8; add = 12; }
    c->progress = (uint16_t)(c->progress + add); if(c->progress >= (uint32_t)col_bld_cost(c, c->plan)) finish_building(c, c->last_day, 0);
    return 1;
}
int col_burnt(const SimColony *c){ return c->state == COLS_DEAD_WORLD; }
int col_invade(SimColony *c, uint32_t size, int diff, uint32_t day, ColAway *aw, uint64_t *reward){   // a real raid now (warship DEFEND missions)
    if(reward) *reward = 0;
    if(c->state != COLS_ALIVE || !(c->mask & (1u << COLR_TENT)) || !c->nset) return 0;
    if(size < 1) size = 1; if(size > 60000) size = 60000;
    uint64_t before = c->reward_due, won = c->raids_won;
    c->raid_size = (uint16_t)size; raid_fight(c, day, diff, aw, 0);
    if(reward) *reward = c->reward_due - before;
    return c->raids_won > won ? 1 : -1;
}
int col_evacuate(SimColony *from, SimColony *to){
    if(from == to || from->state != COLS_ALIVE || to->state != COLS_ALIVE || !(to->mask & (1u << COLR_TENT))) return 0;
    int room = col_cap(to->mask) - to->nset; if(room <= 0) return 0;
    int n = from->nset < room ? from->nset : room; from->nset = (uint8_t)(from->nset - n); to->nset = (uint8_t)(to->nset + n);
    if(!from->nset) from->state = COLS_RUINS;
    return n > 0;
}

int col_collect(SimColony *c, int hunger_room, ColGift *g){
    memset(g, 0, sizeof *g);
    g->reward = c->reward_due; c->reward_total = sat_add64(c->reward_total, c->reward_due); c->reward_due = 0;
    if(c->state != COLS_ALIVE || !(c->mask & (1u << COLR_TENT))){ g->gold = (int64_t)g->reward; return g->reward != 0; }
    if(hunger_room < 0) hunger_room = 0;
    uint32_t want = (uint32_t)hunger_room * 2; if(want > c->food) want = c->food; want &= ~1u;
    g->food_used = want; g->food_pts = (int)(want / 2); c->food -= want;
    g->goods_sold = c->goods; g->ore_sold = c->ore;
    g->gold = (int64_t)sat_add64(g->reward, (uint64_t)c->goods * 5 + (uint64_t)c->ore * 3); c->goods = 0; c->ore = 0;
    g->heal = (c->mask >> COLR_HOUSES) & 1;
    return g->food_pts || g->gold || g->heal;
}
size_t col_dump(const SimColony *c, char *o, size_t n){
    int k = snprintf(o, n, "colony town %08x mask %04x told %04x settlers %u built %d food %u wood %u ore %u goods %u day %u born %u died %u state %u health %u morale %u raid %u/%u won %u lost %u due %llu\n",
                     c->town, c->mask, c->told, c->nset, col_built_total(c), c->food, c->wood, c->ore, c->goods, c->last_day, c->born, c->died, c->state,
                     c->health, c->morale, c->raid_size, c->raid_eta, c->raids_won, c->raids_lost, (unsigned long long)c->reward_due);
    return k < 0 ? 0 : (size_t)k;
}

SimColony *col_find(ColList *l, uint32_t town, int make, uint32_t day){
    for(int i = 0; i < l->n; i++) if(l->c[i].town == town) return &l->c[i];
    if(!make) return 0;
    int at = l->n;
    if(l->n >= COL_MAX){ at = 0; for(int i = 1; i < l->n; i++){ int bi = l->c[i].state == COLS_DEAD_WORLD, ba = l->c[at].state == COLS_DEAD_WORLD;     // burnt worlds are kept for good: they go last
            if(bi < ba || (bi == ba && l->c[i].last_day < l->c[at].last_day)) at = i; } }
    else l->n++;
    col_init(&l->c[at], town, day); return &l->c[at];
}
// ---- the file: one byte count, then records of COL_REC_BYTES (little endian). Older records (34 and 42 bytes, Phases 4 and 5) still load.
static void p16(uint8_t *p, uint32_t v){ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *p, uint32_t v){ p16(p, v); p16(p + 2, v >> 16); }
static void p64(uint8_t *p, uint64_t v){ p32(p, (uint32_t)v); p32(p + 4, (uint32_t)(v >> 32)); }
static uint32_t g16(const uint8_t *p){ return p[0] | (uint32_t)p[1] << 8; }
static uint32_t g32(const uint8_t *p){ return g16(p) | g16(p + 2) << 16; }
static uint64_t g64(const uint8_t *p){ return g32(p) | (uint64_t)g32(p + 4) << 32; }
static uint32_t cl(uint32_t v){ return v > COL_STORE_MAX ? COL_STORE_MAX : v; }
size_t col_list_write(const ColList *l, uint8_t *o, size_t cap){
    size_t need = 1 + (size_t)l->n * COL_REC_BYTES; if(cap < need) return 0;
    memset(o, 0, need); o[0] = l->n;
    for(int i = 0; i < l->n; i++){ const SimColony *c = &l->c[i]; uint8_t *q = o + 1 + i * COL_REC_BYTES;
        p32(q, c->town); p16(q + 4, c->mask); p16(q + 6, c->told); q[8] = c->nset; q[9] = c->state; q[10] = c->danger; q[11] = c->world_ok;
        p32(q + 12, c->food); p32(q + 16, c->wood); p32(q + 20, c->ore); p32(q + 24, c->goods);
        p32(q + 28, c->founded); p32(q + 32, c->last_day); p32(q + 36, c->born); p32(q + 40, c->died);
        memcpy(q + 44, c->bld, COLB_N); memcpy(q + 55, c->grid, COL_GRID_W * COL_GRID_H);
        q[103] = c->plan; q[104] = c->plan_cell; p16(q + 105, c->progress); q[107] = c->health; q[108] = c->morale; p16(q + 109, c->hungry_days);
        p16(q + 111, c->raid_eta); p16(q + 113, c->raid_size); memcpy(q + 115, c->job, COL_MAXSET); q[135] = c->help_days; q[136] = c->god_used;
        p32(q + 137, c->raids_won); p32(q + 141, c->raids_lost); p64(q + 145, c->reward_due); p64(q + 153, c->reward_total); }
    return need;
}
int col_list_read(ColList *l, const uint8_t *in, size_t n){
    memset(l, 0, sizeof *l);
    if(n < 1 || in[0] > COL_MAX) return -1;
    size_t rb = n == 1 + (size_t)in[0] * COL_REC_BYTES ? COL_REC_BYTES : n == 1 + (size_t)in[0] * 42 ? 42 : n == 1 + (size_t)in[0] * 34 ? 34 : 0;
    if(!rb) return -1;
    l->n = in[0];
    for(int i = 0; i < l->n; i++){ SimColony *c = &l->c[i]; const uint8_t *q = in + 1 + i * rb;
        col_init(c, g32(q), 0); c->mask = (uint16_t)((g16(q + 4) & ((1u << COLR_N) - 1)) | 1); c->told = (uint16_t)(g16(q + 6) & c->mask);
        if(rb < COL_REC_BYTES){                                 // an older record: the stores and days sit elsewhere
            c->nset = q[8]; c->food = cl(g32(q + 10)); c->wood = cl(g32(q + 14)); c->ore = cl(g32(q + 18)); c->goods = cl(g32(q + 22));
            c->founded = g32(q + 26); c->last_day = g32(q + 30); if(rb >= 38) c->born = cl(g32(q + 34));
        } else {
            c->nset = q[8]; c->state = q[9] > COLS_DEAD_WORLD ? COLS_RUINS : q[9]; c->danger = q[10] > 9 ? 9 : q[10]; c->world_ok = q[11] ? 1 : 0;
            c->food = cl(g32(q + 12)); c->wood = cl(g32(q + 16)); c->ore = cl(g32(q + 20)); c->goods = cl(g32(q + 24));
            c->founded = g32(q + 28); c->last_day = g32(q + 32); c->born = cl(g32(q + 36)); c->died = cl(g32(q + 40));
            for(int b = 0; b < COLB_N; b++) c->bld[b] = q[44 + b] > 9 ? 9 : q[44 + b];
            for(int k = 0; k < COL_GRID_W * COL_GRID_H; k++) c->grid[k] = q[55 + k] > COLB_N ? 0 : q[55 + k];
            c->plan = q[103] < COLB_N ? q[103] : COLB_HUT; c->plan_cell = q[104]; c->progress = (uint16_t)g16(q + 105);
            c->health = q[107] > 100 ? 100 : q[107]; c->morale = q[108] > 100 ? 100 : q[108]; c->hungry_days = (uint16_t)g16(q + 109);
            c->raid_eta = (uint16_t)g16(q + 111); c->raid_size = (uint16_t)g16(q + 113); memcpy(c->job, q + 115, COL_MAXSET);
            for(int k = 0; k < COL_MAXSET; k++) if(c->job[k] >= COLJ_N) c->job[k] = 0xFF;
            c->help_days = q[135]; c->god_used = q[136] ? 1 : 0; c->raids_won = g32(q + 137); c->raids_lost = g32(q + 141);
            c->reward_due = g64(q + 145); c->reward_total = g64(q + 153);
            if(c->reward_due > COL_GOLD_MAX) c->reward_due = COL_GOLD_MAX;
            if(c->reward_total > COL_GOLD_MAX) c->reward_total = COL_GOLD_MAX;
        }
        if(c->nset > COL_MAXSET) c->nset = COL_MAXSET;
        if(c->nset > col_cap(c->mask)) c->nset = (uint8_t)col_cap(c->mask);
        if(c->state == COLS_ALIVE && !c->nset && (c->mask & (1u << COLR_TENT))) c->state = COLS_RUINS;
    }
    return 0;
}
