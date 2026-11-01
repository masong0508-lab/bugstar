// sy_design.c: see sy_design.h. Plain C (stdint, string, stdio for snprintf only).
#include "sy_design.h"
#include <string.h>
#include <stdio.h>

#define CL(v, a, b) ((v) < (a) ? (a) : (v) > (b) ? (b) : (v))
static uint16_t C15(int r, int g, int b){ return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)); }

const char *const sy_tab_name[SY_NTAB] = {"SHIP","HULL","WINGS","POWER","GUNS","GUARD","ROOMS","PAINT","BADGE","FILE"};
const char *const sy_stat_name[SY_NSTAT] = {"SPEED","TURN","ARMOUR","SHIELD","FIRE","RANGE","HOLD","FUEL","CREW","COST","THREAT"};
const int sy_stat_max[SY_NSTAT] = {100, 200, 600, 125, 300, 120, 120, 300, 60, 3000, 1000};
static const char *const shape_nm[SY_NSHAPE] = {"ROUND","BOX","WEDGE","FLAT"};
static const char *const nose_nm[SY_NNOSE] = {"POINT","BLUNT","FORK","RAM"};
static const char *const wing_nm[SY_NWING] = {"NONE","STRAIGHT","SWEPT","DELTA","X-WINGS","FORWARD","GULL"};
static const char *const eng_nm[SY_NENG] = {"ION","PLASMA","FUSION","WARP"};
static const char *const gun_nm[SY_NGUN] = {"EMPTY","LASER","CANNON","MISSILES","BEAM","FLAK"};
static const char *const gpos_nm[SY_NGPOS] = {"TOP","LEFT","RIGHT","BELOW","NOSE"};
static const char *const cock_nm[SY_NCOCK] = {"BUBBLE","SLIT","DOME","VISOR","BRIDGE"};
static const char *const pat_nm[SY_NPAT] = {"PLAIN","STRIPE","TWO-TONE","CHECKS","TIGER","SPLIT","EDGES","DOTS"};
static const char *const fin_nm[SY_NFIN] = {"GLOSS","MATTE","METAL","PEARL"};
static const char *const size_nm[4] = {"SMALL","MEDIUM","LARGE","HUGE"};
static const char *const tilt_nm[3] = {"DOWN","FLAT","UP"};

const uint16_t sy_swatch[32] = {
    0x5A8E, 0x4A52, 0x7FFF, 0x2108, 0x0C63, 0x001F, 0x0010, 0x01DF, 0x03FF, 0x03E0, 0x0200, 0x7FE0, 0x7C00, 0x4000, 0x7C1F, 0x4010,
    0x3DEF, 0x6318, 0x56B5, 0x1CE7, 0x2D7F, 0x4B3F, 0x2F6A, 0x7EA5, 0x7DAD, 0x5D4A, 0x15B5, 0x02B5, 0x456A, 0x6F7B, 0x2A1F, 0x7F18 };

// ------------------------------------------------------------------ part tables: mass, power, heat, cost, crew
static const uint8_t eng_thr[SY_NENG] = {10, 16, 22, 30}, eng_pow[SY_NENG] = {1, 3, 5, 8}, eng_heat[SY_NENG] = {1, 3, 4, 7}, eng_mass[SY_NENG] = {2, 3, 5, 6}, eng_cost[SY_NENG] = {10, 25, 45, 80};
static const uint8_t gun_dmg[SY_NGUN] = {0, 4, 7, 10, 12, 3}, gun_rng[SY_NGUN] = {0, 6, 4, 9, 7, 3}, gun_pow[SY_NGUN] = {0, 2, 1, 1, 6, 1}, gun_heat[SY_NGUN] = {0, 2, 3, 1, 6, 1},
                     gun_mass[SY_NGUN] = {0, 1, 3, 2, 3, 2}, gun_cost[SY_NGUN] = {0, 15, 20, 35, 60, 15}, gun_crew[SY_NGUN] = {0, 0, 1, 1, 1, 1};
static const uint8_t wing_turn[SY_NWING] = {0, 25, 32, 38, 34, 44, 30};

static int sec_vol(const SySec *s){ int v = s->len * s->wid * s->hgt; if(s->shape == SYSH_FLAT) v /= 2; else if(s->shape == SYSH_WEDGE) v = v * 3 / 4; else if(s->shape == SYSH_ROUND) v = v * 4 / 5; return v; }
static int sec_area(const SySec *s){ return s->len * (s->wid + s->hgt); }
static int gun_mult(const SyDesign *d, const SyGun *g){ return (d->sym && (g->pos == SYGP_LEFT || g->pos == SYGP_RIGHT)) ? 2 : 1; }

static void part_sec(const SyDesign *d, int i, SyPart *p){ const SySec *s = &d->sec[i]; int v = sec_vol(s); p->mass = 2 + v / 12; p->power = 0; p->heat = 0; p->cost = 5 + v / 6; p->crew = 0; }
static void part_arm(const SyDesign *d, int i, SyPart *p){ const SySec *s = &d->sec[i]; memset(p, 0, sizeof *p); p->mass = s->armour * sec_area(s) / 20; p->cost = s->armour * sec_area(s) / 4; }
static void part_eng(const SyDesign *d, SyPart *p){ int k = d->eng, m = d->neng * (2 + d->esize); p->mass = eng_mass[k] * m / 2; p->power = eng_pow[k] * m / 2; p->heat = eng_heat[k] * m / 2; p->cost = eng_cost[k] * m / 2; p->crew = d->neng / 2; }
static void part_gun(const SyDesign *d, int i, SyPart *p){ const SyGun *g = &d->gun[i]; int m = (1 + g->size) * gun_mult(d, g); p->mass = gun_mass[g->kind] * m; p->power = gun_pow[g->kind] * m; p->heat = gun_heat[g->kind] * m; p->cost = gun_cost[g->kind] * m; p->crew = gun_crew[g->kind] * gun_mult(d, g); }
static void part_wing(const SyDesign *d, SyPart *p){ memset(p, 0, sizeof *p); if(d->wing == SYWK_NONE) return; int a = (d->wspan + 2) * (4 + d->wsweep / 3) * (d->wing == SYWK_X ? 2 : 1); p->mass = 1 + a / 10; p->cost = 5 + a / 3; }
static void part_reactor(const SyDesign *d, SyPart *p){ p->mass = 3 + d->reactor * 4; p->power = -(12 + d->reactor * 10); p->heat = d->reactor * 2; p->cost = d->reactor * 40; p->crew = d->reactor / 2; }

void sy_stats(const SyDesign *d0, SyStats *s){
    SyDesign dd = *d0; sy_fix(&dd); const SyDesign *d = &dd; SyPart p; memset(s, 0, sizeof *s);
    int32_t mass = 0, gen = 0, use = 0, heat = 0, cost = 0, crew = 1, vol = 0, armour = 0, fire = 0, range = 0, beds = 2, cool = 12, thirst, shield, turnb = 0, hold, fuel;
    for(int i = 0; i < d->nsec; i++){ part_sec(d, i, &p); mass += p.mass; cost += p.cost; vol += sec_vol(&d->sec[i]);
        part_arm(d, i, &p); mass += p.mass; cost += p.cost; armour += d->sec[i].armour * sec_area(&d->sec[i]) / 6; if(d->sec[i].shape == SYSH_BOX) armour += d->sec[i].len * 2; }
    armour += vol / 40; beds += vol / 150;
    if(d->nose == SYNO_RAM){ armour += 20; mass += 4; cost += 15; } else if(d->nose == SYNO_POINT) turnb += 4;
    part_wing(d, &p); mass += p.mass; cost += p.cost;
    if(d->wing){ turnb += wing_turn[d->wing] * (d->wspan + 4) / 8; cool += (d->wspan + 2) * (4 + d->wsweep / 3) / 12; }
    mass += d->fins * 2; cost += d->fins * 5; cool += d->fins * 3; turnb += d->fins * 3;
    part_eng(d, &p); mass += p.mass; use += p.power; heat += p.heat; cost += p.cost; crew += p.crew;
    int32_t thrust = eng_thr[d->eng] * d->neng * (2 + d->esize) / 2; thirst = p.power;
    part_reactor(d, &p); mass += p.mass; gen += -p.power; heat += p.heat; cost += p.cost; crew += p.crew;
    mass += d->tanks * 2; cost += d->tanks * 8; mass += d->radiators * 2; cost += d->radiators * 12; cool += d->radiators * 6;
    int guns = 0;
    for(int i = 0; i < d->ngun; i++){ const SyGun *g = &d->gun[i]; if(!g->kind) continue; part_gun(d, i, &p); mass += p.mass; use += p.power; heat += p.heat; cost += p.cost; crew += p.crew;
        fire += gun_dmg[g->kind] * (1 + g->size) * gun_mult(d, g); int r = gun_rng[g->kind] * 10 + g->size * 3; if(r > range) range = r; if(g->kind == SYGK_FLAK) armour += 5 * (1 + g->size); guns++; }
    shield = d->shield * 25; use += d->shield * 3; heat += d->shield; mass += d->shield * 2; cost += d->shield * 40; crew += d->shield / 2;
    switch(d->cockpit){ case SYCP_BUBBLE: turnb += 5; break; case SYCP_SLIT: armour += 10; break; case SYCP_DOME: range += 5; break; case SYCP_VISOR: range += 10; break; case SYCP_BRIDGE: beds += 4; fire += 2; mass += 4; cost += 30; break; }
    fire += d->bays * 6; mass += d->bays * 5; crew += d->bays * 3; use += d->bays; cost += d->bays * 50; if(d->bays && range < 80) range = 80;
    hold = 10 + d->cargo * 8 + vol / 100; mass += d->cargo * 2; cost += d->cargo * 10;
    beds += d->quarters * 4; mass += d->quarters * 2; cost += d->quarters * 10;
    fuel = 60 + d->tanks * 20 + vol / 80;
    // the trade-offs: too little power or too much heat weakens guns, shields and engines
    int warn = 0, pf = 100, hf = 100;
    if(use > gen){ warn |= SYWN_POWER; pf = (int)(gen * 100 / (use ? use : 1)); }
    if(heat > cool){ warn |= SYWN_HEAT; hf = (int)(cool * 100 / heat); }
    int32_t thr_e = thrust * (pf < 100 ? (100 + pf) / 2 : 100) / 100;
    int32_t speed = 12 + thr_e * 80 / (mass + 10); speed = CL(speed, 0, 100);
    int32_t turn = (40 + turnb) * 60 / (mass / 2 + 30); turn = turn * 100 / 55; turn = CL(turn, 0, 200);
    fire = fire * pf / 100 * hf / 100; shield = shield * pf / 100;
    if(crew > beds){ warn |= SYWN_CREW; fire = fire * 3 / 4; }
    if(speed < 25) warn |= SYWN_HEAVY;
    if(!guns && !d->bays) warn |= SYWN_NOGUN;
    if(fuel < thirst * 5) warn |= SYWN_FUEL;
    int32_t threat = fire * (range + 20) / 40 + armour / 3 + shield / 2 + speed + turn / 4 + d->bays * 10;
    s->v[SYST_SPEED] = speed; s->v[SYST_TURN] = turn; s->v[SYST_ARMOUR] = CL(armour, 0, 9999); s->v[SYST_SHIELD] = CL(shield, 0, 9999); s->v[SYST_FIRE] = CL(fire, 0, 9999);
    s->v[SYST_RANGE] = CL(range, 0, 9999); s->v[SYST_HOLD] = CL(hold, 0, 9999); s->v[SYST_FUEL] = CL(fuel, 0, 9999); s->v[SYST_CREW] = CL(crew, 0, 9999); s->v[SYST_COST] = CL(cost, 0, 9999); s->v[SYST_THREAT] = CL(threat, 0, 9999);
    s->mass = mass; s->power_gen = gen; s->power_use = use; s->heat = heat; s->cooling = cool; s->thrust = thrust; s->crew_need = crew; s->beds = beds; s->warn = warn;
}

int sy_warn_text(int warn, int i, char *out, int n){
    static const char *const t[7] = {
        "UNDERPOWERED: THE REACTOR CANNOT FEED IT ALL. GUNS AND SHIELDS ARE WEAK",
        "OVERHEATING: ADD RADIATORS OR FINS, OR FIT FEWER BEAMS",
        "TOO HEAVY: IT WILL CRAWL. ADD ENGINES OR TAKE OFF ARMOUR",
        "NOT ENOUGH BEDS: ADD CREW QUARTERS OR THE GUNS FIRE SLOWER",
        "NO GUNS AND NO HANGAR: THIS SHIP CANNOT FIGHT",
        "SMALL FUEL TANK FOR THESE ENGINES: ADD FUEL TANKS",
        "" };
    int k = 0; for(int b = 0; b < 6; b++) if(warn & (1 << b)){ if(k == i){ snprintf(out, n, "%s", t[b]); return 1; } k++; }
    if(n) out[0] = 0; return 0;
}

void sy_game(const SyDesign *d, SyGame *g){
    SyStats s; sy_stats(d, &s);
    g->speed = CL(28 + s.v[SYST_SPEED] * 62 / 80, 28, 90);
    g->turn = CL(40 + s.v[SYST_TURN] * 130 / 200, 40, 170);
    g->armour = CL(115 - (s.v[SYST_ARMOUR] + s.v[SYST_SHIELD]) / 4, 40, 130);
    g->gun = CL(s.v[SYST_FIRE] / 2, 0, 24);
    g->hold = CL((s.v[SYST_HOLD] - 10) / 2, 0, 60);
    g->fuel = CL(s.v[SYST_FUEL] - 60, 0, 200);
    g->accel = CL(s.v[SYST_SPEED] * 24 / 55, 4, 96);
    g->damp = d->fins == 0 ? 3 : d->fins == 1 ? 4 : 6;
}

// ------------------------------------------------------------------ making and fixing
uint64_t sy_rng(uint64_t *s){ *s = *s * 6364136223846793005ull + 1442695040888963407ull; return *s >> 33; }
static int okch(char c){ return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' || c == '\''; }
void sy_fix(SyDesign *d){
    d->name[SY_NAME] = 0; int L = 0; for(int i = 0; i < SY_NAME && d->name[i]; i++){ char c = d->name[i]; if(c >= 'a' && c <= 'z') c -= 32; d->name[i] = okch(c) ? c : ' '; L = i + 1; }
    for(int i = L; i <= SY_NAME; i++) d->name[i] = 0;
    while(L > 0 && d->name[L - 1] == ' ') d->name[--L] = 0;
    if(!d->name[0]) strcpy(d->name, "MOTH");
    d->nsec = CL(d->nsec, 1, SY_MAXSEC);
    for(int i = 0; i < SY_MAXSEC; i++){ SySec *s = &d->sec[i]; if(i >= d->nsec){ memset(s, 0, sizeof *s); continue; }
        s->len = CL(s->len, 1, SY_UNIT_MAX); s->wid = CL(s->wid, 1, SY_UNIT_MAX); s->hgt = CL(s->hgt, 1, SY_UNIT_MAX); s->shape %= SY_NSHAPE; s->armour = CL(s->armour, 0, 5); }
    d->sym = d->sym ? 1 : 0; d->ingame = d->ingame ? 1 : 0; d->nose %= SY_NNOSE;
    d->wing %= SY_NWING; d->wspan = CL(d->wspan, 0, 15); d->wsweep = CL(d->wsweep, 0, 15); d->wsec = d->wsec % d->nsec; d->wtilt %= 3; d->fins %= 3;
    d->eng %= SY_NENG; d->neng = CL(d->neng, 1, 6); d->esize %= 4; d->reactor = CL(d->reactor, 0, 5); d->tanks = CL(d->tanks, 0, 6); d->radiators = CL(d->radiators, 0, 5);
    d->ngun = CL(d->ngun, 0, SY_MAXGUN);
    for(int i = 0; i < SY_MAXGUN; i++){ SyGun *g = &d->gun[i]; if(i >= d->ngun){ memset(g, 0, sizeof *g); continue; } g->kind %= SY_NGUN; g->size %= 3; g->sec %= d->nsec; g->pos %= SY_NGPOS; }
    d->shield = CL(d->shield, 0, 5); d->cockpit %= SY_NCOCK; d->bays = CL(d->bays, 0, 4); d->cargo = CL(d->cargo, 0, 8); d->quarters = CL(d->quarters, 0, 6);
    d->paint &= 0x7FFF; d->trim &= 0x7FFF; d->glow &= 0x7FFF; d->light &= 0x7FFF; d->emcol &= 0x7FFF; d->pattern %= SY_NPAT; d->finish %= SY_NFIN;
}
void sy_default(SyDesign *d){
    memset(d, 0, sizeof *d); strcpy(d->name, "MOTH");
    d->nsec = 3; d->sec[0] = (SySec){4, 4, 3, SYSH_ROUND, 0}; d->sec[1] = (SySec){6, 6, 4, SYSH_ROUND, 1}; d->sec[2] = (SySec){4, 5, 4, SYSH_ROUND, 1};
    d->sym = 1; d->nose = SYNO_POINT; d->wing = SYWK_SWEPT; d->wspan = 6; d->wsweep = 6; d->wsec = 1; d->wtilt = 1; d->fins = 1;
    d->eng = SYEK_PLASMA; d->neng = 2; d->esize = 1; d->reactor = 1; d->tanks = 1; d->radiators = 1;
    d->ngun = 1; d->gun[0] = (SyGun){SYGK_LASER, 0, 0, SYGP_NOSE};
    d->shield = 1; d->cockpit = SYCP_BUBBLE; d->bays = 0; d->cargo = 1; d->quarters = 1;
    d->paint = C15(150, 156, 176); d->trim = C15(232, 80, 72); d->glow = C15(90, 200, 255); d->light = C15(255, 230, 140); d->emcol = C15(255, 210, 60);
    d->pattern = SYPT_STRIPE; d->finish = SYFI_GLOSS; d->ingame = 1;
    static const uint8_t em[8] = {0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x24, 0x42}; memcpy(d->emblem, em, 8);
    static const uint8_t dc[8] = {0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81}; memcpy(d->decal, dc, 8);
}
static void tame(SyDesign *d);
const char *const sy_preset_name[SY_NPRESET] = {"MOTH","DART","HAULER","GUNBOAT","INTERCEPTOR","CARRIER","TANK","EXPLORER","CORVETTE","STARHAMMER"};
void sy_preset(SyDesign *d, int i){
    char nm[SY_NAME + 1]; memcpy(nm, d->name, sizeof nm); int ig = d->ingame; uint8_t em[8], dc[8]; memcpy(em, d->emblem, 8); memcpy(dc, d->decal, 8);
    sy_default(d); i = ((i % SY_NPRESET) + SY_NPRESET) % SY_NPRESET;
    switch(i){
    case 1: d->nsec = 3; d->sec[0] = (SySec){5, 2, 2, SYSH_ROUND, 0}; d->sec[1] = (SySec){5, 3, 2, SYSH_WEDGE, 0}; d->sec[2] = (SySec){3, 3, 2, SYSH_FLAT, 0}; d->wing = SYWK_DELTA; d->wspan = 7; d->wsweep = 10;
        d->eng = SYEK_FUSION; d->neng = 1; d->esize = 1; d->reactor = 1; d->shield = 0; d->quarters = 0; d->cargo = 0; d->paint = C15(220, 220, 230); d->trim = C15(40, 120, 255); d->pattern = SYPT_EDGE; break;
    case 2: d->nsec = 4; for(int k = 0; k < 4; k++) d->sec[k] = (SySec){5, 9, 7, SYSH_BOX, 1}; d->sec[0].wid = 6; d->nose = SYNO_BLUNT; d->wing = SYWK_STRAIGHT; d->wspan = 3; d->wsweep = 0; d->wsec = 2;
        d->eng = SYEK_ION; d->neng = 4; d->esize = 2; d->cargo = 8; d->quarters = 2; d->tanks = 4; d->reactor = 2; d->radiators = 2; d->paint = C15(200, 150, 60); d->trim = C15(60, 60, 70); d->pattern = SYPT_CHECKS; d->cockpit = SYCP_BRIDGE; break;
    case 3: d->nsec = 3; d->sec[0] = (SySec){4, 5, 4, SYSH_BOX, 2}; d->sec[1] = (SySec){6, 7, 5, SYSH_BOX, 3}; d->sec[2] = (SySec){4, 6, 5, SYSH_BOX, 2}; d->nose = SYNO_RAM; d->wing = SYWK_NONE; d->fins = 2;
        d->eng = SYEK_FUSION; d->neng = 2; d->esize = 2; d->reactor = 4; d->radiators = 4; d->shield = 2; d->quarters = 3; d->ngun = 4;
        d->gun[0] = (SyGun){SYGK_CANNON, 2, 0, SYGP_NOSE}; d->gun[1] = (SyGun){SYGK_LASER, 1, 1, SYGP_LEFT}; d->gun[2] = (SyGun){SYGK_MISSILE, 1, 1, SYGP_TOP}; d->gun[3] = (SyGun){SYGK_FLAK, 0, 2, SYGP_BELOW};
        d->paint = C15(80, 100, 70); d->trim = C15(200, 180, 60); d->pattern = SYPT_TIGER; d->cockpit = SYCP_SLIT; break;
    case 4: d->nsec = 2; d->sec[0] = (SySec){7, 2, 2, SYSH_ROUND, 0}; d->sec[1] = (SySec){5, 3, 2, SYSH_ROUND, 0}; d->wing = SYWK_FORWARD; d->wspan = 8; d->wsweep = 8; d->wsec = 1; d->fins = 2;
        d->eng = SYEK_WARP; d->neng = 1; d->esize = 1; d->reactor = 1; d->radiators = 0; d->shield = 0; d->quarters = 0; d->cargo = 0; d->tanks = 0; d->ngun = 1; d->gun[0] = (SyGun){SYGK_LASER, 1, 0, SYGP_LEFT};
        d->paint = C15(30, 30, 40); d->trim = C15(255, 60, 40); d->glow = C15(255, 120, 40); d->pattern = SYPT_STRIPE; d->cockpit = SYCP_VISOR; break;
    case 5: d->nsec = 5; for(int k = 0; k < 5; k++) d->sec[k] = (SySec){6, 10, 5, SYSH_FLAT, 1}; d->sec[0] = (SySec){5, 6, 4, SYSH_WEDGE, 1}; d->nose = SYNO_BLUNT; d->wing = SYWK_STRAIGHT; d->wspan = 4; d->wsec = 3;
        d->eng = SYEK_FUSION; d->neng = 3; d->esize = 2; d->reactor = 4; d->radiators = 3; d->bays = 4; d->shield = 3; d->quarters = 6; d->ngun = 2; d->gun[0] = (SyGun){SYGK_FLAK, 1, 1, SYGP_LEFT}; d->gun[1] = (SyGun){SYGK_FLAK, 1, 3, SYGP_TOP};
        d->paint = C15(120, 130, 150); d->trim = C15(240, 240, 255); d->pattern = SYPT_SPLIT; d->cockpit = SYCP_BRIDGE; break;
    case 6: d->nsec = 2; d->sec[0] = (SySec){5, 7, 6, SYSH_BOX, 5}; d->sec[1] = (SySec){5, 8, 6, SYSH_BOX, 5}; d->nose = SYNO_RAM; d->wing = SYWK_NONE; d->eng = SYEK_ION; d->neng = 3; d->esize = 3; d->reactor = 3; d->radiators = 2;
        d->shield = 4; d->ngun = 1; d->gun[0] = (SyGun){SYGK_CANNON, 2, 0, SYGP_TOP}; d->paint = C15(90, 90, 100); d->trim = C15(200, 60, 40); d->pattern = SYPT_PLAIN; d->finish = SYFI_METAL; d->cockpit = SYCP_SLIT; break;
    case 7: d->nsec = 4; d->sec[0] = (SySec){4, 4, 4, SYSH_ROUND, 0}; d->sec[1] = (SySec){5, 5, 5, SYSH_ROUND, 0}; d->sec[2] = (SySec){5, 5, 5, SYSH_ROUND, 0}; d->sec[3] = (SySec){3, 4, 4, SYSH_ROUND, 0};
        d->wing = SYWK_GULL; d->wspan = 9; d->wsec = 2; d->eng = SYEK_PLASMA; d->neng = 2; d->esize = 2; d->tanks = 6; d->cargo = 4; d->quarters = 2; d->reactor = 2; d->cockpit = SYCP_DOME;
        d->paint = C15(240, 240, 235); d->trim = C15(40, 160, 120); d->pattern = SYPT_TWOTONE; d->finish = SYFI_PEARL; break;
    case 8: d->nsec = 4; d->sec[0] = (SySec){5, 4, 3, SYSH_WEDGE, 1}; d->sec[1] = (SySec){6, 6, 4, SYSH_WEDGE, 2}; d->sec[2] = (SySec){6, 6, 4, SYSH_BOX, 2}; d->sec[3] = (SySec){4, 5, 4, SYSH_BOX, 1};
        d->wing = SYWK_SWEPT; d->wspan = 5; d->wsweep = 9; d->wsec = 2; d->eng = SYEK_FUSION; d->neng = 2; d->esize = 2; d->reactor = 3; d->radiators = 2; d->shield = 2; d->quarters = 3; d->ngun = 3;
        d->gun[0] = (SyGun){SYGK_BEAM, 0, 0, SYGP_NOSE}; d->gun[1] = (SyGun){SYGK_LASER, 1, 2, SYGP_RIGHT}; d->gun[2] = (SyGun){SYGK_MISSILE, 0, 1, SYGP_TOP}; d->paint = C15(60, 70, 120); d->trim = C15(255, 200, 60); break;
    case 9: d->nsec = 6; d->sec[0] = (SySec){6, 6, 5, SYSH_WEDGE, 3}; for(int k = 1; k < 6; k++) d->sec[k] = (SySec){7, 10, 7, SYSH_BOX, 3}; d->sec[5].wid = 8; d->nose = SYNO_RAM; d->wing = SYWK_X; d->wspan = 5; d->wsec = 4;
        d->eng = SYEK_WARP; d->neng = 6; d->esize = 3; d->reactor = 5; d->radiators = 5; d->shield = 5; d->bays = 2; d->quarters = 6; d->ngun = 6; d->fins = 2;
        d->gun[0] = (SyGun){SYGK_BEAM, 2, 0, SYGP_NOSE}; d->gun[1] = (SyGun){SYGK_CANNON, 2, 1, SYGP_TOP}; d->gun[2] = (SyGun){SYGK_MISSILE, 2, 2, SYGP_LEFT}; d->gun[3] = (SyGun){SYGK_LASER, 2, 3, SYGP_RIGHT}; d->gun[4] = (SyGun){SYGK_FLAK, 1, 4, SYGP_BELOW}; d->gun[5] = (SyGun){SYGK_CANNON, 1, 5, SYGP_TOP};
        d->paint = C15(40, 40, 50); d->trim = C15(220, 30, 30); d->glow = C15(255, 60, 60); d->pattern = SYPT_TIGER; d->finish = SYFI_METAL; d->cockpit = SYCP_BRIDGE; break;
    default: break; }
    memcpy(d->name, nm, sizeof nm); d->ingame = ig; memcpy(d->emblem, em, 8); memcpy(d->decal, dc, 8); tame(d);
}
void sy_balance(SyDesign *d){       // enough reactor, cooling, beds and fuel for the parts (a fix-up loop; it never removes parts)
    sy_fix(d);
    for(int k = 0; k < 40; k++){ SyStats s; sy_stats(d, &s); if(s.warn & SYWN_POWER && d->reactor < 5) d->reactor++; else if(s.warn & SYWN_HEAT && d->radiators < 5) d->radiators++; else if(s.warn & SYWN_HEAT && d->fins < 2) d->fins++;
        else if(s.warn & SYWN_CREW && d->quarters < 6) d->quarters++; else if(s.warn & SYWN_FUEL && d->tanks < 6) d->tanks++; else if(s.warn & SYWN_HEAVY && d->neng < 6) d->neng++; else break; }
}
static void tame(SyDesign *d){      // the dice and presets: if it still runs hot or short of power, make the hungriest parts smaller
    sy_balance(d);
    for(int k = 0; k < 40; k++){ SyStats s; sy_stats(d, &s); if(!(s.warn & (SYWN_POWER | SYWN_HEAT))) break;
        int bi = -1, bw = 0; for(int i = 0; i < d->ngun; i++){ int w = (gun_pow[d->gun[i].kind] + gun_heat[d->gun[i].kind]) * (1 + d->gun[i].size); if(w > bw){ bw = w; bi = i; } }
        if(bi >= 0 && d->gun[bi].size) d->gun[bi].size--; else if(bi >= 0 && d->gun[bi].kind == SYGK_BEAM) d->gun[bi].kind = SYGK_LASER;
        else if(d->esize > 0) d->esize--; else if(d->eng > 0) d->eng--; else if(d->shield > 0) d->shield--; else if(d->neng > 1) d->neng--; else break;
        sy_balance(d); }
}
void sy_random(SyDesign *d, uint64_t *r){
    sy_preset(d, (int)(sy_rng(r) % SY_NPRESET));
    #define RN(n) ((int)(sy_rng(r) % (uint64_t)(n)))
    d->nsec = 1 + RN(5); int w = 2 + RN(7), h = 2 + RN(5);
    for(int i = 0; i < d->nsec; i++){ d->sec[i].len = 3 + RN(6); d->sec[i].wid = CL(w + RN(3) - 1, 1, 15); d->sec[i].hgt = CL(h + RN(3) - 1, 1, 15); d->sec[i].shape = i && RN(3) ? d->sec[i - 1].shape : RN(SY_NSHAPE); d->sec[i].armour = RN(4); }
    d->nose = RN(SY_NNOSE); d->wing = RN(SY_NWING); d->wspan = 2 + RN(9); d->wsweep = RN(14); d->wsec = RN(d->nsec); d->wtilt = RN(3); d->fins = RN(3);
    d->eng = RN(SY_NENG); d->neng = 1 + RN(4); d->esize = RN(4); d->tanks = RN(4); d->radiators = RN(4);
    d->ngun = 1 + RN(4); for(int i = 0; i < d->ngun; i++) d->gun[i] = (SyGun){(uint8_t)(1 + RN(SY_NGUN - 1)), (uint8_t)RN(3), (uint8_t)RN(d->nsec), (uint8_t)RN(SY_NGPOS)};
    d->shield = RN(4); d->cockpit = RN(SY_NCOCK); d->bays = RN(5) == 0 ? 1 + RN(2) : 0; d->cargo = RN(5); d->pattern = RN(SY_NPAT); d->finish = RN(SY_NFIN);
    d->paint = sy_swatch[RN(32)]; d->trim = sy_swatch[RN(32)]; d->glow = sy_swatch[RN(32)]; d->light = sy_swatch[RN(32)];
    #undef RN
    tame(d);
}

// ------------------------------------------------------------------ the recipe (a bit stream) and the share code
typedef struct { uint8_t *b; int max, bit; } BW;
static void put(BW *w, uint32_t v, int n){ for(int i = 0; i < n; i++){ int B = w->bit >> 3; if(B >= w->max){ w->bit++; continue; } if(!(w->bit & 7)) w->b[B] = 0; if(v >> i & 1) w->b[B] |= (uint8_t)(1 << (w->bit & 7)); w->bit++; } }
typedef struct { const uint8_t *b; int n, bit; } BR;
static uint32_t get(BR *r, int n){ uint32_t v = 0; for(int i = 0; i < n; i++){ int B = r->bit >> 3; if(B < r->n && (r->b[B] >> (r->bit & 7) & 1)) v |= 1u << i; r->bit++; } return v; }
static const char NAMECH[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.'";       // 40 letters: 6 bits each
static uint16_t crc16(const uint8_t *p, int n){ uint16_t c = 0xFFFF; for(int i = 0; i < n; i++){ c ^= (uint16_t)(p[i] << 8); for(int k = 0; k < 8; k++) c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1); } return c; }
static void bits(BW *w, BR *r, int dec, uint8_t *v, int n){ if(dec) *v = (uint8_t)get(r, n); else put(w, *v, n); }
static void bits16(BW *w, BR *r, int dec, uint16_t *v, int n){ if(dec) *v = (uint16_t)get(r, n); else put(w, *v, n); }
static void walk(SyDesign *d, BW *w, BR *r, int dec){                    // the one description of the layout, used both ways
    uint8_t L = 0; if(!dec) L = (uint8_t)strlen(d->name); bits(w, r, dec, &L, 4); if(L > SY_NAME) L = SY_NAME;
    for(int i = 0; i < L; i++){ uint8_t c = 0; if(!dec){ const char *p = strchr(NAMECH, d->name[i]); c = p ? (uint8_t)(p - NAMECH) : 0; } bits(w, r, dec, &c, 6); if(dec) d->name[i] = c < 40 ? NAMECH[c] : ' '; }
    if(dec) d->name[L] = 0;
    uint8_t ns = (uint8_t)(d->nsec - 1); bits(w, r, dec, &ns, 3); if(dec) d->nsec = (uint8_t)(ns + 1);
    for(int i = 0; i < d->nsec; i++){ SySec *s = &d->sec[i]; bits(w, r, dec, &s->len, 4); bits(w, r, dec, &s->wid, 4); bits(w, r, dec, &s->hgt, 4); bits(w, r, dec, &s->shape, 2); bits(w, r, dec, &s->armour, 3); }
    bits(w, r, dec, &d->sym, 1); bits(w, r, dec, &d->ingame, 1); bits(w, r, dec, &d->nose, 2);
    bits(w, r, dec, &d->wing, 3); bits(w, r, dec, &d->wspan, 4); bits(w, r, dec, &d->wsweep, 4); bits(w, r, dec, &d->wsec, 3); bits(w, r, dec, &d->wtilt, 2); bits(w, r, dec, &d->fins, 2);
    bits(w, r, dec, &d->eng, 2); bits(w, r, dec, &d->neng, 3); bits(w, r, dec, &d->esize, 2); bits(w, r, dec, &d->reactor, 3); bits(w, r, dec, &d->tanks, 3); bits(w, r, dec, &d->radiators, 3);
    bits(w, r, dec, &d->ngun, 3); if(dec && d->ngun > SY_MAXGUN) d->ngun = SY_MAXGUN;
    for(int i = 0; i < d->ngun; i++){ SyGun *g = &d->gun[i]; bits(w, r, dec, &g->kind, 3); bits(w, r, dec, &g->size, 2); bits(w, r, dec, &g->sec, 3); bits(w, r, dec, &g->pos, 3); }
    bits(w, r, dec, &d->shield, 3); bits(w, r, dec, &d->cockpit, 3); bits(w, r, dec, &d->bays, 3); bits(w, r, dec, &d->cargo, 4); bits(w, r, dec, &d->quarters, 3);
    bits16(w, r, dec, &d->paint, 15); bits16(w, r, dec, &d->trim, 15); bits16(w, r, dec, &d->glow, 15); bits16(w, r, dec, &d->light, 15); bits16(w, r, dec, &d->emcol, 15);
    bits(w, r, dec, &d->pattern, 3); bits(w, r, dec, &d->finish, 2);
    for(int i = 0; i < 8; i++) bits(w, r, dec, &d->emblem[i], 8);
    for(int i = 0; i < 8; i++) bits(w, r, dec, &d->decal[i], 8);
}
int sy_encode(const SyDesign *d0, uint8_t *out, int max){
    if(max < 8) return 0; SyDesign d = *d0; sy_fix(&d);
    out[0] = 0x53; out[1] = SY_VERSION; BW w = {out + 2, max - 4, 0}; walk(&d, &w, 0, 0);
    int n = 2 + (w.bit + 7) / 8; if(n + 2 > max) return 0;
    uint16_t c = crc16(out, n); out[n] = (uint8_t)(c & 255); out[n + 1] = (uint8_t)(c >> 8); return n + 2;
}
int sy_decode(const uint8_t *in, int n, SyDesign *d){
    if(n < 6 || n > SY_RECIPE_MAX || in[0] != 0x53 || in[1] < 1 || in[1] > SY_VERSION) return 0;
    uint16_t c = crc16(in, n - 2); if(in[n - 2] != (c & 255) || in[n - 1] != (c >> 8)) return 0;
    SyDesign t; sy_default(&t); memset(t.sec, 0, sizeof t.sec); memset(t.gun, 0, sizeof t.gun); memset(t.name, 0, sizeof t.name);
    BR r = {in + 2, n - 4, 0}; walk(&t, 0, &r, 1); sy_fix(&t); *d = t; return 1;
}
static const char B32[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
int sy_code(const SyDesign *d, char *out, int n){
    uint8_t b[SY_RECIPE_MAX]; int k = sy_encode(d, b, sizeof b); if(!k) return 0;
    int need = 5 + (k * 8 + 4) / 5 + 1; if(need > n) return 0;
    strcpy(out, "SHIP-"); int o = 5; uint32_t acc = 0; int nb = 0;
    for(int i = 0; i < k; i++){ acc |= (uint32_t)b[i] << nb; nb += 8; while(nb >= 5){ out[o++] = B32[acc & 31]; acc >>= 5; nb -= 5; } }
    if(nb) out[o++] = B32[acc & 31]; out[o] = 0; return o;
}
int sy_uncode(const char *code, SyDesign *d){
    if(!code) return 0; while(*code == ' ') code++;
    { const char *P = "SHIP-"; for(int i = 0; i < 5; i++){ char ch = code[i]; if(ch >= 'a' && ch <= 'z') ch -= 32; if(ch != P[i]) return 0; } } code += 5;
    uint8_t b[SY_RECIPE_MAX]; int k = 0; uint32_t acc = 0; int nb = 0;
    for(; *code; code++){ char ch = *code; if(ch >= 'a' && ch <= 'z') ch -= 32; if(ch == ' ' || ch == '-' || ch == '\n' || ch == '\r') continue; const char *p = strchr(B32, ch); if(!p || !ch) return 0;
        acc |= (uint32_t)(p - B32) << nb; nb += 5; if(nb >= 8){ if(k >= SY_RECIPE_MAX) return 0; b[k++] = (uint8_t)(acc & 255); acc >>= 8; nb -= 8; } }
    return sy_decode(b, k, d);
}
int sy_same(const SyDesign *a, const SyDesign *b){ uint8_t x[SY_RECIPE_MAX], y[SY_RECIPE_MAX]; int n = sy_encode(a, x, sizeof x), m = sy_encode(b, y, sizeof y); return n == m && !memcmp(x, y, n); }
uint32_t sy_hash(const SyDesign *d){ uint8_t x[SY_RECIPE_MAX]; int n = sy_encode(d, x, sizeof x); uint32_t h = 2166136261u; for(int i = 0; i < n; i++){ h ^= x[i]; h *= 16777619u; } return h; }

// ------------------------------------------------------------------ undo / redo
void sy_hist_init(SyHist *h, const SyDesign *d){ h->n = 1; h->at = 0; h->h[0] = *d; }
void sy_hist_push(SyHist *h, const SyDesign *d){
    if(h->n && sy_same(&h->h[h->at], d)) return;
    h->n = h->at + 1; if(h->n >= SY_HIST){ memmove(&h->h[0], &h->h[1], sizeof(SyDesign) * (SY_HIST - 1)); h->n = SY_HIST - 1; }
    h->h[h->n] = *d; h->at = h->n; h->n++;
}
int sy_hist_undo(SyHist *h, SyDesign *d){ if(h->at <= 0) return 0; h->at--; *d = h->h[h->at]; return 1; }
int sy_hist_redo(SyHist *h, SyDesign *d){ if(h->at + 1 >= h->n) return 0; h->at++; *d = h->h[h->at]; return 1; }

// ------------------------------------------------------------------ rows
int sy_rows(int tab, const SyDesign *d, const SyCursor *c, int *ids){
    int n = 0, a = c->advanced;
    #define R(x) ids[n++] = (x)
    switch(tab){
    case SYT_SHIP: R(SYR_PRESET); R(SYR_DICE); R(SYR_MODE); R(SYR_INGAME); R(SYR_UNDO); R(SYR_REDO); if(a) R(SYR_SNAP); break;
    case SYT_HULL: R(SYR_SEC); R(SYR_LEN); R(SYR_WID); R(SYR_HGT); R(SYR_SHAPE); R(SYR_NOSE); R(SYR_ADDSEC); R(SYR_DELSEC); R(SYR_SYM); if(a){ R(SYR_COPYSEC); R(SYR_STRETCH); } break;
    case SYT_WINGS: R(SYR_WING); R(SYR_WSPAN); R(SYR_WSWEEP); R(SYR_FINS); if(a){ R(SYR_WSEC); R(SYR_WTILT); } break;
    case SYT_POWER: R(SYR_ENG); R(SYR_NENG); R(SYR_ESIZE); R(SYR_REACTOR); R(SYR_RADS); R(SYR_TANKS); break;
    case SYT_GUNS: R(SYR_GUN); if(d->ngun){ R(SYR_GKIND); R(SYR_GSIZE); R(SYR_GPOS); } R(SYR_ADDGUN); if(d->ngun) R(SYR_DELGUN); if(a && d->ngun){ R(SYR_GSEC); R(SYR_COPYGUN); } break;
    case SYT_GUARD: R(SYR_SHIELD); R(SYR_ARMALL); if(a) for(int i = 0; i < d->nsec; i++) R(SYR_ARM0 + i); break;
    case SYT_ROOMS: R(SYR_COCK); R(SYR_BAYS); R(SYR_CARGO); R(SYR_QUART); break;
    case SYT_PAINT: R(SYR_PAINTC); R(SYR_TRIMC); R(SYR_GLOWC); R(SYR_PATTERN); if(a){ R(SYR_LIGHTC); R(SYR_FINISH); } break;
    case SYT_BADGE: R(SYR_NAME); R(SYR_EMBLEM); R(SYR_EMCOL); R(SYR_DECAL); break;
    case SYT_FILE: R(SYR_SAVE); R(SYR_LOAD); R(SYR_CODE); R(SYR_IMPORT); R(SYR_CLASSIC); R(SYR_DONE); break; }
    #undef R
    return n;
}
const char *sy_row_label(int id){
    static char armb[12];
    if(id >= SYR_ARM0 && id <= SYR_ARM_END){ snprintf(armb, sizeof armb, "PLATES %d", id - SYR_ARM0 + 1); return armb; }
    switch(id){
    case SYR_PRESET: return "START FROM"; case SYR_DICE: return "SURPRISE ME"; case SYR_MODE: return "MODE"; case SYR_SNAP: return "SNAP"; case SYR_INGAME: return "FLY IT"; case SYR_UNDO: return "UNDO"; case SYR_REDO: return "REDO";
    case SYR_SEC: return "SECTION"; case SYR_LEN: return "LENGTH"; case SYR_WID: return "WIDTH"; case SYR_HGT: return "HEIGHT"; case SYR_SHAPE: return "SHAPE"; case SYR_NOSE: return "NOSE"; case SYR_SYM: return "SYMMETRY";
    case SYR_ADDSEC: return "ADD SECTION"; case SYR_COPYSEC: return "COPY SECTION"; case SYR_DELSEC: return "REMOVE SECTION"; case SYR_STRETCH: return "STRETCH ALL";
    case SYR_WING: return "WINGS"; case SYR_WSPAN: return "SPAN"; case SYR_WSWEEP: return "SWEEP"; case SYR_WSEC: return "WING SPOT"; case SYR_WTILT: return "TILT"; case SYR_FINS: return "FINS";
    case SYR_ENG: return "ENGINE"; case SYR_NENG: return "ENGINES"; case SYR_ESIZE: return "ENGINE SIZE"; case SYR_REACTOR: return "REACTOR"; case SYR_TANKS: return "FUEL TANKS"; case SYR_RADS: return "RADIATORS";
    case SYR_GUN: return "MOUNT"; case SYR_GKIND: return "WEAPON"; case SYR_GSIZE: return "SIZE"; case SYR_GSEC: return "ON SECTION"; case SYR_GPOS: return "PLACE"; case SYR_ADDGUN: return "ADD MOUNT"; case SYR_COPYGUN: return "MIRROR COPY"; case SYR_DELGUN: return "REMOVE MOUNT";
    case SYR_SHIELD: return "SHIELD"; case SYR_ARMALL: return "ARMOUR";
    case SYR_COCK: return "COCKPIT"; case SYR_BAYS: return "HANGAR BAYS"; case SYR_CARGO: return "CARGO PODS"; case SYR_QUART: return "CREW ROOMS";
    case SYR_PAINTC: return "PAINT"; case SYR_TRIMC: return "TRIM"; case SYR_GLOWC: return "ENGINE GLOW"; case SYR_LIGHTC: return "LIGHTS"; case SYR_PATTERN: return "PATTERN"; case SYR_FINISH: return "FINISH";
    case SYR_NAME: return "NAME"; case SYR_EMBLEM: return "EMBLEM"; case SYR_EMCOL: return "EMBLEM COLOUR"; case SYR_DECAL: return "DECAL";
    case SYR_SAVE: return "SAVE DESIGN"; case SYR_LOAD: return "LOAD DESIGN"; case SYR_CODE: return "SHARE CODE"; case SYR_IMPORT: return "IMPORT CODE"; case SYR_CLASSIC: return "OLD HANGAR"; case SYR_DONE: return "DONE"; }
    return "";
}
const char *sy_row_help(int id){
    if(id >= SYR_ARM0 && id <= SYR_ARM_END) return "ARMOUR PLATES ON THIS SECTION ONLY. TOUGHER BUT HEAVIER";
    switch(id){
    case SYR_PRESET: return "A READY SHIP TO START FROM. KEEPS YOUR NAME AND BADGE";
    case SYR_DICE: return "A: ROLL A RANDOM SHIP THAT WORKS. UNDO BRINGS YOURS BACK";
    case SYR_MODE: return "SIMPLE SHOWS THE MAIN PARTS. ADVANCED SHOWS EVERY PART";
    case SYR_SNAP: return "SNAP ON: DRAGGING MOVES IN STEPS OF 2. OFF: STEPS OF 1";
    case SYR_INGAME: return "YES: YOU FLY THIS DESIGN IN SPACE. NO: THE OLD SHIP";
    case SYR_UNDO: return "A: TAKE BACK THE LAST CHANGE (ALSO START, RIGHT CLICK)"; case SYR_REDO: return "A: PUT BACK WHAT YOU TOOK BACK";
    case SYR_SEC: return "PICK A HULL SECTION. 1 IS THE NOSE. CLICK ONE IN THE PLAN";
    case SYR_LEN: return "LONGER: ROOMIER BUT HEAVIER. DRAG IN THE PLAN TOO";
    case SYR_WID: return "WIDER: MORE ROOM AND ARMOUR, MORE MASS";
    case SYR_HGT: return "TALLER: MORE ROOM, A BIGGER TARGET";
    case SYR_SHAPE: return "ROUND IS LIGHT. BOX IS TOUGH. WEDGE AND FLAT ARE LEAN";
    case SYR_NOSE: return "POINT TURNS BETTER. RAM ADDS ARMOUR. LOOKS CHANGE TOO";
    case SYR_SYM: return "ON: SIDE GUNS ARE MIRRORED, SO YOU GET TWO";
    case SYR_ADDSEC: return "A: ADD A SECTION BEHIND THIS ONE (8 AT MOST)";
    case SYR_COPYSEC: return "A: COPY THIS SECTION RIGHT BEHIND IT";
    case SYR_DELSEC: return "A: REMOVE THIS SECTION (1 IS ALWAYS KEPT)";
    case SYR_STRETCH: return "LEFT RIGHT: MAKE EVERY SECTION LONGER OR SHORTER";
    case SYR_WING: return "WINGS TURN THE SHIP AND SHED HEAT. NONE IS LIGHTEST";
    case SYR_WSPAN: return "WIDER WINGS TURN AND COOL BETTER BUT WEIGH MORE";
    case SYR_WSWEEP: return "HOW FAR BACK THE TIPS GO. MORE AREA, MORE COOLING";
    case SYR_WSEC: return "WHICH SECTION THE WINGS SIT ON"; case SYR_WTILT: return "WING TILT: LOOK ONLY";
    case SYR_FINS: return "FINS STEADY TURNS AND SHED A LITTLE HEAT";
    case SYR_ENG: return "ION IS LIGHT AND COOL. WARP IS FAST BUT HOT AND HUNGRY";
    case SYR_NENG: return "MORE ENGINES: MORE PUSH, MORE POWER AND HEAT";
    case SYR_ESIZE: return "BIGGER ENGINES PUSH HARDER AND WEIGH MORE";
    case SYR_REACTOR: return "MAKES POWER FOR ENGINES, GUNS AND SHIELDS. ADDS HEAT";
    case SYR_TANKS: return "MORE FUEL FOR LONG TRIPS. 20 EACH";
    case SYR_RADS: return "COOLING PLATES. THEY STOP OVERHEATING";
    case SYR_GUN: return "PICK A WEAPON MOUNT. ADD UP TO 6";
    case SYR_GKIND: return "LASER ALL ROUND. CANNON HITS HARD. MISSILES REACH FAR";
    case SYR_GSIZE: return "BIGGER GUNS HIT HARDER BUT NEED MORE POWER";
    case SYR_GSEC: return "WHICH SECTION THIS MOUNT SITS ON";
    case SYR_GPOS: return "TOP, SIDES, BELOW OR NOSE. SIDES DOUBLE WITH SYMMETRY";
    case SYR_ADDGUN: return "A: ADD A NEW WEAPON MOUNT"; case SYR_COPYGUN: return "A: COPY THIS MOUNT TO THE OTHER SIDE"; case SYR_DELGUN: return "A: REMOVE THIS MOUNT";
    case SYR_SHIELD: return "SHIELDS SOAK HITS FIRST. THEY NEED LOTS OF POWER";
    case SYR_ARMALL: return "ARMOUR PLATES ON EVERY SECTION. ADVANCED: ONE BY ONE";
    case SYR_COCK: return "SLIT IS SAFE. VISOR SEES FAR. BRIDGE HAS BEDS";
    case SYR_BAYS: return "FIGHTERS LAUNCH FROM BAYS. THEY NEED CREW AND ROOM";
    case SYR_CARGO: return "MORE HOLD FOR ORE AND GOODS. 8 EACH";
    case SYR_QUART: return "BEDS FOR THE CREW. GUNS AND BAYS NEED PEOPLE";
    case SYR_PAINTC: return "THE MAIN COLOUR. LEFT RIGHT: NEXT. A: MIX YOUR OWN";
    case SYR_TRIMC: return "THE SECOND COLOUR FOR STRIPES AND WINGS";
    case SYR_GLOWC: return "THE COLOUR OF THE ENGINE FLAMES";
    case SYR_LIGHTC: return "THE COLOUR OF THE WINDOWS AND RUNNING LIGHTS";
    case SYR_PATTERN: return "WHERE THE TRIM COLOUR GOES ON THE HULL";
    case SYR_FINISH: return "HOW THE PAINT CATCHES THE LIGHT";
    case SYR_NAME: return "A: TYPE A NAME (12 LETTERS). LEFT RIGHT: RANDOM NAME";
    case SYR_EMBLEM: return "A: DRAW YOUR EMBLEM ON AN 8 BY 8 GRID";
    case SYR_EMCOL: return "THE EMBLEM'S COLOUR";
    case SYR_DECAL: return "A: DRAW THE DECAL PAINTED ON THE WINGS";
    case SYR_SAVE: return "A: SAVE THIS DESIGN NEXT TO YOUR SAVE (A SMALL FILE)";
    case SYR_LOAD: return "A: OPEN A DESIGN YOU SAVED";
    case SYR_CODE: return "A: MAKE A SHARE CODE (WRITTEN TO THE .SHIPCODE FILE)";
    case SYR_IMPORT: return "A: READ A SHARE CODE FROM THE .SHIPCODE FILE";
    case SYR_CLASSIC: return "A: THE GAME'S OLD HANGAR SCREEN";
    case SYR_DONE: return "A: FINISH AND FLY. START ALSO UNDOES, B LEAVES"; }
    return "";
}
static int colidx(uint16_t c){ for(int i = 0; i < 32; i++) if(sy_swatch[i] == c) return i; return -1; }
void sy_row_value(int id, const SyDesign *d, const SyCursor *c, char *o, int n){
    const SySec *s = &d->sec[c->sec < d->nsec ? c->sec : 0]; const SyGun *g = &d->gun[c->gun < d->ngun ? c->gun : 0]; o[0] = 0;
    if(id >= SYR_ARM0 && id <= SYR_ARM_END){ snprintf(o, n, "%d", d->sec[id - SYR_ARM0].armour); return; }
    switch(id){
    case SYR_PRESET: snprintf(o, n, "PICK"); break; case SYR_MODE: snprintf(o, n, "%s", c->advanced ? "ADVANCED" : "SIMPLE"); break;
    case SYR_SNAP: snprintf(o, n, "%s", c->snap ? "ON" : "OFF"); break; case SYR_INGAME: snprintf(o, n, "%s", d->ingame ? "YES" : "NO"); break;
    case SYR_SEC: snprintf(o, n, "%d OF %d", c->sec + 1, d->nsec); break;
    case SYR_LEN: snprintf(o, n, "%d", s->len); break; case SYR_WID: snprintf(o, n, "%d", s->wid); break; case SYR_HGT: snprintf(o, n, "%d", s->hgt); break;
    case SYR_SHAPE: snprintf(o, n, "%s", shape_nm[s->shape % SY_NSHAPE]); break; case SYR_NOSE: snprintf(o, n, "%s", nose_nm[d->nose % SY_NNOSE]); break;
    case SYR_SYM: snprintf(o, n, "%s", d->sym ? "MIRRORED" : "OFF"); break;
    case SYR_STRETCH: { int t = 0; for(int i = 0; i < d->nsec; i++) t += d->sec[i].len; snprintf(o, n, "LENGTH %d", t); break; }
    case SYR_WING: snprintf(o, n, "%s", wing_nm[d->wing % SY_NWING]); break; case SYR_WSPAN: snprintf(o, n, "%d", d->wspan); break; case SYR_WSWEEP: snprintf(o, n, "%d", d->wsweep); break;
    case SYR_WSEC: snprintf(o, n, "SECTION %d", d->wsec + 1); break; case SYR_WTILT: snprintf(o, n, "%s", tilt_nm[d->wtilt % 3]); break; case SYR_FINS: snprintf(o, n, "%d", d->fins); break;
    case SYR_ENG: snprintf(o, n, "%s", eng_nm[d->eng % SY_NENG]); break; case SYR_NENG: snprintf(o, n, "%d", d->neng); break; case SYR_ESIZE: snprintf(o, n, "%s", size_nm[d->esize & 3]); break;
    case SYR_REACTOR: snprintf(o, n, "LEVEL %d", d->reactor); break; case SYR_TANKS: snprintf(o, n, "%d", d->tanks); break; case SYR_RADS: snprintf(o, n, "%d", d->radiators); break;
    case SYR_GUN: if(d->ngun) snprintf(o, n, "%d OF %d", c->gun + 1, d->ngun); else snprintf(o, n, "NONE"); break;
    case SYR_GKIND: snprintf(o, n, "%s", gun_nm[g->kind % SY_NGUN]); break; case SYR_GSIZE: snprintf(o, n, "%s", size_nm[g->size % 3]); break;
    case SYR_GSEC: snprintf(o, n, "SECTION %d", g->sec + 1); break;
    case SYR_GPOS: snprintf(o, n, "%s%s", gpos_nm[g->pos % SY_NGPOS], d->sym && (g->pos == SYGP_LEFT || g->pos == SYGP_RIGHT) ? " X2" : ""); break;
    case SYR_SHIELD: snprintf(o, n, "%d", d->shield); break;
    case SYR_ARMALL: { int a = d->sec[0].armour, same = 1; for(int i = 1; i < d->nsec; i++) if(d->sec[i].armour != a) same = 0; if(same) snprintf(o, n, "%d", a); else snprintf(o, n, "MIXED"); break; }
    case SYR_COCK: snprintf(o, n, "%s", cock_nm[d->cockpit % SY_NCOCK]); break; case SYR_BAYS: snprintf(o, n, "%d", d->bays); break; case SYR_CARGO: snprintf(o, n, "%d", d->cargo); break; case SYR_QUART: snprintf(o, n, "%d", d->quarters); break;
    case SYR_PAINTC: case SYR_TRIMC: case SYR_GLOWC: case SYR_LIGHTC: case SYR_EMCOL: { uint16_t v = id == SYR_PAINTC ? d->paint : id == SYR_TRIMC ? d->trim : id == SYR_GLOWC ? d->glow : id == SYR_LIGHTC ? d->light : d->emcol; int k = colidx(v); if(k >= 0) snprintf(o, n, "COLOUR %d", k + 1); else snprintf(o, n, "MIXED"); break; }
    case SYR_PATTERN: snprintf(o, n, "%s", pat_nm[d->pattern % SY_NPAT]); break; case SYR_FINISH: snprintf(o, n, "%s", fin_nm[d->finish % SY_NFIN]); break;
    case SYR_NAME: snprintf(o, n, "%s", d->name); break; case SYR_EMBLEM: case SYR_DECAL: snprintf(o, n, "DRAW"); break;
    default: break; }
}
int sy_row_is_value(int id){
    switch(id){ case SYR_DICE: case SYR_UNDO: case SYR_REDO: case SYR_ADDSEC: case SYR_COPYSEC: case SYR_DELSEC: case SYR_ADDGUN: case SYR_COPYGUN: case SYR_DELGUN: case SYR_EMBLEM: case SYR_DECAL:
        case SYR_SAVE: case SYR_LOAD: case SYR_CODE: case SYR_IMPORT: case SYR_CLASSIC: case SYR_DONE: return 0; }
    return 1;
}
int sy_row_part(int id, const SyDesign *d, const SyCursor *c, SyPart *p){
    memset(p, 0, sizeof *p); int s = c->sec < d->nsec ? c->sec : 0;
    if(id >= SYR_ARM0 && id <= SYR_ARM_END){ if(id - SYR_ARM0 >= d->nsec) return 0; part_arm(d, id - SYR_ARM0, p); return 1; }
    switch(id){
    case SYR_SEC: case SYR_LEN: case SYR_WID: case SYR_HGT: case SYR_SHAPE: case SYR_ADDSEC: case SYR_COPYSEC: case SYR_DELSEC: part_sec(d, s, p); return 1;
    case SYR_WING: case SYR_WSPAN: case SYR_WSWEEP: case SYR_WSEC: case SYR_WTILT: part_wing(d, p); return 1;
    case SYR_FINS: p->mass = d->fins * 2; p->heat = -d->fins * 3; p->cost = d->fins * 5; return 1;
    case SYR_ENG: case SYR_NENG: case SYR_ESIZE: part_eng(d, p); return 1;
    case SYR_REACTOR: part_reactor(d, p); return 1;
    case SYR_TANKS: p->mass = d->tanks * 2; p->cost = d->tanks * 8; return 1;
    case SYR_RADS: p->mass = d->radiators * 2; p->heat = -d->radiators * 6; p->cost = d->radiators * 12; return 1;
    case SYR_GUN: case SYR_GKIND: case SYR_GSIZE: case SYR_GSEC: case SYR_GPOS: case SYR_COPYGUN: case SYR_DELGUN: if(!d->ngun) return 0; part_gun(d, c->gun < d->ngun ? c->gun : 0, p); return 1;
    case SYR_SHIELD: p->mass = d->shield * 2; p->power = d->shield * 3; p->heat = d->shield; p->cost = d->shield * 40; p->crew = d->shield / 2; return 1;
    case SYR_ARMALL: for(int i = 0; i < d->nsec; i++){ SyPart q; part_arm(d, i, &q); p->mass += q.mass; p->cost += q.cost; } return 1;
    case SYR_BAYS: p->mass = d->bays * 5; p->crew = d->bays * 3; p->power = d->bays; p->cost = d->bays * 50; return 1;
    case SYR_CARGO: p->mass = d->cargo * 2; p->cost = d->cargo * 10; return 1;
    case SYR_QUART: p->mass = d->quarters * 2; p->cost = d->quarters * 10; p->crew = -d->quarters * 4; return 1;
    case SYR_COCK: p->crew = 1; p->mass = d->cockpit == SYCP_BRIDGE ? 4 : 0; p->cost = d->cockpit == SYCP_BRIDGE ? 30 : 0; return 1; }
    return 0;
}
void sy_cursor_fix(const SyDesign *d, SyCursor *c){ if(c->sec >= d->nsec) c->sec = d->nsec - 1; if(c->sec < 0) c->sec = 0; if(c->gun >= d->ngun) c->gun = d->ngun ? d->ngun - 1 : 0; if(c->gun < 0) c->gun = 0; }
static int cyc(int v, int d, int n){ return ((v + d) % n + n) % n; }
static int clampstep(uint8_t *v, int d, int lo, int hi){ int o = *v, t = CL(o + d, lo, hi); *v = (uint8_t)t; return t != o; }
static int colstep(uint16_t *v, int d){ int k = colidx(*v); k = k < 0 ? (d > 0 ? 0 : 31) : cyc(k, d > 0 ? 1 : -1, 32); *v = sy_swatch[k]; return 1; }
int sy_change(SyDesign *d, SyCursor *c, int id, int delta){
    if(!delta) return 0; int dir = delta > 0 ? 1 : -1, st = (delta > 1 || delta < -1) ? 4 : 1; if(c->sec >= d->nsec) c->sec = d->nsec - 1; if(c->sec < 0) c->sec = 0; if(c->gun >= d->ngun) c->gun = d->ngun ? d->ngun - 1 : 0; if(c->gun < 0) c->gun = 0;
    SySec *s = &d->sec[c->sec]; SyGun *g = &d->gun[c->gun]; int ch = 0, us = c->snap ? 2 : 1;
    if(id >= SYR_ARM0 && id <= SYR_ARM_END){ if(id - SYR_ARM0 < d->nsec) ch = clampstep(&d->sec[id - SYR_ARM0].armour, dir, 0, 5); return ch; }
    switch(id){
    case SYR_PRESET: { static int p; p = cyc(p, dir, SY_NPRESET); sy_preset(d, p); return 1; }
    case SYR_DICE: return 0;
    case SYR_MODE: c->advanced ^= 1; return 0; case SYR_SNAP: c->snap ^= 1; return 0;
    case SYR_INGAME: d->ingame ^= 1; return 1;
    case SYR_SEC: c->sec = cyc(c->sec, dir, d->nsec); return 0;
    case SYR_LEN: return clampstep(&s->len, dir * st * us, 1, SY_UNIT_MAX); case SYR_WID: return clampstep(&s->wid, dir * st * us, 1, SY_UNIT_MAX); case SYR_HGT: return clampstep(&s->hgt, dir * st * us, 1, SY_UNIT_MAX);
    case SYR_SHAPE: s->shape = (uint8_t)cyc(s->shape, dir, SY_NSHAPE); return 1; case SYR_NOSE: d->nose = (uint8_t)cyc(d->nose, dir, SY_NNOSE); return 1;
    case SYR_SYM: d->sym ^= 1; return 1;
    case SYR_STRETCH: for(int i = 0; i < d->nsec; i++) ch |= clampstep(&d->sec[i].len, dir, 1, SY_UNIT_MAX); return ch;
    case SYR_WING: d->wing = (uint8_t)cyc(d->wing, dir, SY_NWING); return 1; case SYR_WSPAN: return clampstep(&d->wspan, dir * st, 0, 15); case SYR_WSWEEP: return clampstep(&d->wsweep, dir * st, 0, 15);
    case SYR_WSEC: d->wsec = (uint8_t)cyc(d->wsec, dir, d->nsec); return 1; case SYR_WTILT: d->wtilt = (uint8_t)cyc(d->wtilt, dir, 3); return 1; case SYR_FINS: return clampstep(&d->fins, dir, 0, 2);
    case SYR_ENG: d->eng = (uint8_t)cyc(d->eng, dir, SY_NENG); return 1; case SYR_NENG: return clampstep(&d->neng, dir, 1, 6); case SYR_ESIZE: return clampstep(&d->esize, dir, 0, 3);
    case SYR_REACTOR: return clampstep(&d->reactor, dir, 0, 5); case SYR_TANKS: return clampstep(&d->tanks, dir, 0, 6); case SYR_RADS: return clampstep(&d->radiators, dir, 0, 5);
    case SYR_GUN: if(d->ngun) c->gun = cyc(c->gun, dir, d->ngun); return 0;
    case SYR_GKIND: if(!d->ngun) return 0; g->kind = (uint8_t)cyc(g->kind, dir, SY_NGUN); return 1;
    case SYR_GSIZE: if(!d->ngun) return 0; return clampstep(&g->size, dir, 0, 2);
    case SYR_GSEC: if(!d->ngun) return 0; g->sec = (uint8_t)cyc(g->sec, dir, d->nsec); return 1;
    case SYR_GPOS: if(!d->ngun) return 0; g->pos = (uint8_t)cyc(g->pos, dir, SY_NGPOS); return 1;
    case SYR_SHIELD: return clampstep(&d->shield, dir, 0, 5);
    case SYR_ARMALL: { int a = CL(d->sec[c->sec].armour + dir, 0, 5); for(int i = 0; i < d->nsec; i++){ if(d->sec[i].armour != a) ch = 1; d->sec[i].armour = (uint8_t)a; } return ch; }
    case SYR_COCK: d->cockpit = (uint8_t)cyc(d->cockpit, dir, SY_NCOCK); return 1;
    case SYR_BAYS: return clampstep(&d->bays, dir, 0, 4); case SYR_CARGO: return clampstep(&d->cargo, dir, 0, 8); case SYR_QUART: return clampstep(&d->quarters, dir, 0, 6);
    case SYR_PAINTC: return colstep(&d->paint, dir); case SYR_TRIMC: return colstep(&d->trim, dir); case SYR_GLOWC: return colstep(&d->glow, dir); case SYR_LIGHTC: return colstep(&d->light, dir); case SYR_EMCOL: return colstep(&d->emcol, dir);
    case SYR_PATTERN: d->pattern = (uint8_t)cyc(d->pattern, dir, SY_NPAT); return 1; case SYR_FINISH: d->finish = (uint8_t)cyc(d->finish, dir, SY_NFIN); return 1;
    case SYR_NAME: { static const char *const a[] = {"MOTH","WASP","HORNET","BEETLE","CRICKET","MANTIS","LOCUST","CICADA","FIREFLY","SCARAB","DRAGONFLY","STAG"}, *const b[] = {"","II","ONE","PRIME","RED","STAR","NOVA","MK 3"};
        static unsigned k = 7; k = k * 1103515245u + 12345u; snprintf(d->name, sizeof d->name, "%s%s%s", a[(k >> 8) % 12], ((k >> 16) & 1) ? " " : "", ((k >> 16) & 1) ? b[(k >> 20) % 8] : ""); sy_fix(d); return 1; }
    }
    return 0;
}
int sy_action(SyDesign *d, SyCursor *c, int id){
    if(c->sec >= d->nsec) c->sec = d->nsec - 1; if(c->sec < 0) c->sec = 0;
    switch(id){
    case SYR_ADDSEC: case SYR_COPYSEC: if(d->nsec >= SY_MAXSEC) return 0;
        for(int i = d->nsec; i > c->sec + 1; i--) d->sec[i] = d->sec[i - 1];
        d->sec[c->sec + 1] = d->sec[c->sec]; if(id == SYR_ADDSEC){ d->sec[c->sec + 1].len = 4; }
        for(int i = 0; i < d->ngun; i++) if(d->gun[i].sec > c->sec) d->gun[i].sec++;
        if(d->wsec > c->sec) d->wsec++;
        d->nsec++; c->sec++; return 1;
    case SYR_DELSEC: if(d->nsec <= 1) return 0;
        for(int i = c->sec; i < d->nsec - 1; i++) d->sec[i] = d->sec[i + 1];
        for(int i = 0; i < d->ngun; i++){ if(d->gun[i].sec > c->sec) d->gun[i].sec--; else if(d->gun[i].sec == c->sec && d->gun[i].sec >= d->nsec - 1) d->gun[i].sec = (uint8_t)(d->nsec - 2); }
        if(d->wsec > c->sec || d->wsec >= d->nsec - 1) d->wsec = d->wsec ? d->wsec - 1 : 0;
        d->nsec--; memset(&d->sec[d->nsec], 0, sizeof(SySec)); if(c->sec >= d->nsec) c->sec = d->nsec - 1; sy_fix(d); return 1;
    case SYR_ADDGUN: if(d->ngun >= SY_MAXGUN) return 0; d->gun[d->ngun] = (SyGun){SYGK_LASER, 0, (uint8_t)c->sec, SYGP_TOP}; c->gun = d->ngun++; return 1;
    case SYR_COPYGUN: if(!d->ngun || d->ngun >= SY_MAXGUN) return 0; { SyGun g = d->gun[c->gun]; if(g.pos == SYGP_LEFT) g.pos = SYGP_RIGHT; else if(g.pos == SYGP_RIGHT) g.pos = SYGP_LEFT; else if(g.pos == SYGP_TOP) g.pos = SYGP_BELOW; else if(g.pos == SYGP_BELOW) g.pos = SYGP_TOP; d->gun[d->ngun] = g; c->gun = d->ngun++; } return 1;
    case SYR_DELGUN: if(!d->ngun) return 0; for(int i = c->gun; i < d->ngun - 1; i++) d->gun[i] = d->gun[i + 1]; d->ngun--; memset(&d->gun[d->ngun], 0, sizeof(SyGun)); if(c->gun >= d->ngun) c->gun = d->ngun ? d->ngun - 1 : 0; return 1;
    }
    return 0;
}

// ------------------------------------------------------------------ the 3D model
static const int8_t RX[8] = {100, 71, 0, -71, -100, -71, 0, 71}, RY[8] = {0, 71, 100, 71, 0, -71, -100, -71};
static const int8_t BX[8] = {100, 100, 0, -100, -100, -100, 0, 100}, BY[8] = {0, 100, 100, 100, 0, -100, -100, -100};
static int addv(SyMesh *m, int x, int y, int z){ if(m->nv >= SY_MAXV) return m->nv - 1; m->v[m->nv] = (SyV){(int16_t)x, (int16_t)y, (int16_t)z}; return m->nv++; }
static void addf(SyMesh *m, int n, int a, int b, int c, int dd, int col, int two, int sec){ if(m->nf >= SY_MAXF || a < 0 || b < 0 || c < 0) return; SyF *f = &m->f[m->nf++]; f->n = (uint8_t)n; f->v[0] = (uint16_t)a; f->v[1] = (uint16_t)b; f->v[2] = (uint16_t)c; f->v[3] = (uint16_t)(n == 4 ? dd : c); f->col = (uint8_t)col; f->two = (uint8_t)two; f->sec = (int8_t)sec; }
static int ring(SyMesh *m, int w, int h, int shape, int z, int dy){
    int b = m->nv; if(m->nv + 8 > SY_MAXV) return -1;
    for(int k = 0; k < 8; k++){ int x, y;
        if(shape == SYSH_BOX){ x = BX[k] * w / 100; y = BY[k] * h / 100; }
        else { x = RX[k] * w / 100; y = RY[k] * h / 100; if(shape == SYSH_WEDGE && y > 0) x = x * 45 / 100; if(shape == SYSH_FLAT) y /= 2; }
        addv(m, x, y + dy, z); }
    return b;
}
static void box(SyMesh *m, int cx, int cy, int cz, int dx, int dy, int dz, int col, int sec){
    if(m->nv + 8 > SY_MAXV || m->nf + 6 > SY_MAXF) return; int p = m->nv;
    for(int k = 0; k < 8; k++) addv(m, cx + ((k & 1) ? dx : -dx), cy + ((k & 2) ? dy : -dy), cz + ((k & 4) ? dz : -dz));
    addf(m, 4, p + 0, p + 2, p + 3, p + 1, col, 0, sec); addf(m, 4, p + 4, p + 5, p + 7, p + 6, col, 0, sec);
    addf(m, 4, p + 0, p + 1, p + 5, p + 4, col, 0, sec); addf(m, 4, p + 2, p + 6, p + 7, p + 3, col, 0, sec);
    addf(m, 4, p + 0, p + 4, p + 6, p + 2, col, 0, sec); addf(m, 4, p + 1, p + 3, p + 7, p + 5, col, 0, sec);
}
static int patcol(const SyDesign *d, int sec, int k, int seg, int nseg){
    switch(d->pattern){
    case SYPT_STRIPE: return (k == 5 || k == 6) ? 1 : 0;              // y negative = top in screen terms is drawn as up
    case SYPT_TWOTONE: return (k >= 1 && k <= 3) ? 1 : 0;
    case SYPT_CHECKS: return ((seg + k) & 1);
    case SYPT_TIGER: return ((seg * 3 + k) % 4) == 0;
    case SYPT_SPLIT: return seg * 2 < nseg;
    case SYPT_EDGE: return k == 0 || k == 4;
    case SYPT_DOTS: return (seg % 2 == 0) && (k % 4 == 2);
    } (void)sec; return 0;
}
void sy_mesh(const SyDesign *d0, SyMesh *m){
    SyDesign dd = *d0; sy_fix(&dd); const SyDesign *d = &dd; m->nv = m->nf = 0;
    const int U = 6; int L = 0; for(int i = 0; i < d->nsec; i++) L += d->sec[i].len * U;
    int z0 = -L / 2, zs[SY_MAXSEC + 1]; zs[0] = z0; for(int i = 0; i < d->nsec; i++) zs[i + 1] = zs[i] + d->sec[i].len * U;
    // rings: front edge, then for every section its middle and its back edge (the edges are blends of the neighbours)
    int rg[2 * SY_MAXSEC + 2], rs[2 * SY_MAXSEC + 2], nr = 0;
    #define HW(i) (d->sec[i].wid * U / 2 + 1)
    #define HH(i) (d->sec[i].hgt * U / 2 + 1)
    rg[nr] = ring(m, HW(0) * 6 / 10, HH(0) * 6 / 10, d->sec[0].shape, zs[0], 0); rs[nr++] = 0;
    for(int i = 0; i < d->nsec; i++){
        rg[nr] = ring(m, HW(i), HH(i), d->sec[i].shape, (zs[i] + zs[i + 1]) / 2, 0); rs[nr++] = i;
        if(i + 1 < d->nsec) rg[nr] = ring(m, (HW(i) + HW(i + 1)) / 2, (HH(i) + HH(i + 1)) / 2, d->sec[i].armour >= d->sec[i + 1].armour ? d->sec[i].shape : d->sec[i + 1].shape, zs[i + 1], 0);
        else rg[nr] = ring(m, HW(i) * 8 / 10, HH(i) * 8 / 10, d->sec[i].shape, zs[i + 1], 0);
        rs[nr++] = i; }
    for(int r = 0; r + 1 < nr; r++){ if(rg[r] < 0 || rg[r + 1] < 0) break; int sec = rs[r + 1], arm = d->sec[sec].armour;
        for(int k = 0; k < 8; k++){ int k2 = (k + 1) & 7; int col = patcol(d, sec, k, r, nr - 1); if(arm >= 3 && (k == 0 || k == 4 || k == 3 || k == 7)) col = 2; addf(m, 4, rg[r] + k, rg[r] + k2, rg[r + 1] + k2, rg[r + 1] + k, col, 0, sec); } }
    // nose
    { int f = rg[0], len0 = d->sec[0].len * U; if(f >= 0){
        if(d->nose == SYNO_FORK){ int a = addv(m, HW(0) / 2, 0, zs[0] - len0 * 7 / 10), b = addv(m, -HW(0) / 2, 0, zs[0] - len0 * 7 / 10);
            for(int k = 0; k < 8; k++){ int k2 = (k + 1) & 7; int t = (k <= 1 || k >= 6) ? a : b; addf(m, 3, t, f + k2, f + k, 0, 0, 0, 0); } }
        else { int tz = d->nose == SYNO_POINT ? zs[0] - len0 * 9 / 10 : d->nose == SYNO_BLUNT ? zs[0] - len0 / 4 : zs[0] - len0 / 3;
            int t = addv(m, 0, 0, tz); for(int k = 0; k < 8; k++){ int k2 = (k + 1) & 7; addf(m, 3, t, f + k2, f + k, d->nose == SYNO_RAM ? 2 : 0, 0, 0, 0); } } } }
    // tail cap
    { int b = rg[nr - 1]; if(b >= 0){ int t = addv(m, 0, 0, zs[d->nsec] + 2); for(int k = 0; k < 8; k++){ int k2 = (k + 1) & 7; addf(m, 3, t, b + k, b + k2, 2, 0, 0, d->nsec - 1); } } }
    int last = d->nsec - 1;
    // wings (both sides), with the decal side shown in trim
    if(d->wing){ int s = d->wsec, za = zs[s], zb = zs[s + 1], w = HW(s), span = (d->wspan + 2) * U, sw = d->wsweep * U / 2, ty = d->wtilt == 0 ? span / 3 : d->wtilt == 2 ? -span / 3 : 0;
        int pairs = d->wing == SYWK_X ? 2 : 1;
        for(int pp = 0; pp < pairs; pp++){ int yo = pairs == 2 ? (pp ? span / 3 : -span / 3) : ty;
            for(int side = -1; side <= 1; side += 2){ int rx = side * (w - 1), tx = side * (w + span);
                int r0, r1, t0, t1, rz0 = za, rz1 = zb, tz0, tz1;
                switch(d->wing){
                case SYWK_STRAIGHT: tz0 = za + sw / 2; tz1 = zb - (zb - za) / 4 + sw / 2; break;
                case SYWK_DELTA: rz0 = za - (zb - za) / 2; tz0 = zb + sw / 2 - 4; tz1 = zb + sw / 2; break;
                case SYWK_FORWARD: tz0 = za - sw; tz1 = za - sw + (zb - za) / 2; break;
                case SYWK_GULL: { int mx = side * (w + span / 2), my = -span / 5, mz = (za + zb) / 2 + sw / 2;
                    int a = addv(m, rx, 0, za), b = addv(m, rx, 0, zb), c = addv(m, mx, my, mz + 6), e = addv(m, mx, my, mz - 6);
                    addf(m, 4, a, b, c, e, 1, 1, s);
                    int f2 = addv(m, tx, span / 6, mz + sw / 2 + 4), g2 = addv(m, tx, span / 6, mz + sw / 2 - 2);
                    addf(m, 4, e, c, f2, g2, 1, 1, s); continue; }
                default: tz0 = za + sw; tz1 = za + sw + (zb - za) / 3; break; }
                r0 = addv(m, rx, 0, rz0); r1 = addv(m, rx, 0, rz1); t1 = addv(m, tx, yo, tz1); t0 = addv(m, tx, yo, tz0);
                addf(m, 4, r0, r1, t1, t0, 1, 1, s); } } }
    // fins on the tail
    for(int k = 0; k < d->fins; k++){ int s = last, za = zs[s], zb = zs[s + 1], h = HH(s), x = d->fins == 2 ? (k ? HW(s) / 2 : -HW(s) / 2) : 0;
        int a = addv(m, x, -h + 1, za + (zb - za) / 3), b = addv(m, x, -h + 1, zb), c = addv(m, x, -h - 10, zb + 2), e = addv(m, x, -h - 10, zb - 4);
        addf(m, 4, a, b, c, e, 1, 1, s); }
    // radiators: thin plates off the back sides
    for(int k = 0; k < d->radiators; k++){ int s = last, z = zs[s] + 3 + k * 4, w = HW(s), side = (k & 1) ? 1 : -1;
        int a = addv(m, side * w, -2, z), b = addv(m, side * w, -2, z + 3), c = addv(m, side * (w + 8), -6, z + 3), e = addv(m, side * (w + 8), -6, z);
        addf(m, 4, a, b, c, e, 2, 1, s); }
    // engines: a row of nacelles at the tail with glowing backs
    { int s = last, w = HW(s), n = d->neng, r = 3 + d->esize * 2, z = zs[d->nsec] + r / 2;
        for(int k = 0; k < n; k++){ int x = n == 1 ? 0 : -w + 2 + (2 * w - 4) * k / (n - 1), y = (k & 1) && n > 3 ? -r : r / 3;
            box(m, x, y, z - r, r, r, r + 3, 2, s); int g = m->nv; addv(m, x - r + 1, y - r + 1, z + 4); addv(m, x + r - 1, y - r + 1, z + 4); addv(m, x + r - 1, y + r - 1, z + 4); addv(m, x - r + 1, y + r - 1, z + 4);
            addf(m, 4, g, g + 1, g + 2, g + 3, 3, 1, s); } }
    // weapons
    for(int i = 0; i < d->ngun; i++){ const SyGun *g = &d->gun[i]; if(!g->kind) continue; int s = g->sec, zc = (zs[s] + zs[s + 1]) / 2, w = HW(s), h = HH(s), bl = 6 + g->size * 4, bw = 1 + g->size;
        int sides = (g->pos == SYGP_LEFT || g->pos == SYGP_RIGHT) && d->sym ? 2 : 1;
        for(int k = 0; k < sides; k++){ int x = 0, y = 0, z = zc;
            switch(g->pos){ case SYGP_TOP: y = -h - bw; break; case SYGP_BELOW: y = h + bw; break; case SYGP_LEFT: x = (k ? 1 : -1) * (w + bw); break; case SYGP_RIGHT: x = (k ? -1 : 1) * (w + bw); break; case SYGP_NOSE: z = zs[0] - 2; y = HH(0) / 3; s = 0; break; }
            if(g->kind == SYGK_MISSILE) box(m, x, y, z, bw + 2, bw + 1, bl / 2 + 2, 2, s);
            else { box(m, x, y, z + 2, bw + 1, bw + 1, 3, 2, s); box(m, x, y, z - bl / 2, g->kind == SYGK_BEAM ? bw + 1 : bw, g->kind == SYGK_BEAM ? bw + 1 : bw, bl / 2 + 1, 5, s); } } }
    // cockpit
    { int s = d->nsec > 1 ? 0 : 0, h = HH(s), z = (zs[s] + zs[s + 1]) / 2, cw = 3 + d->sec[s].wid / 3;
        switch(d->cockpit){ case SYCP_BUBBLE: box(m, 0, -h, z, cw, 3, 6, 4, s); break; case SYCP_SLIT: box(m, 0, -h, z, cw + 2, 1, 3, 4, s); break;
        case SYCP_DOME: box(m, 0, -h - 1, z, cw + 1, 4, cw + 1, 4, s); break; case SYCP_VISOR: box(m, 0, -h + 2, zs[0] + 3, cw + 2, 2, 3, 4, s); break;
        case SYCP_BRIDGE: { int sb = d->nsec > 2 ? d->nsec / 2 : 0, hb = HH(sb), zb = (zs[sb] + zs[sb + 1]) / 2; box(m, 0, -hb - 4, zb, cw + 2, 4, 6, 2, sb); box(m, 0, -hb - 7, zb - 4, cw + 1, 1, 2, 4, sb); break; } } }
    // hangar bays, cargo pods, fuel tanks, the emblem plate
    for(int k = 0; k < d->bays; k++){ int s = (d->nsec * (k + 1)) / (d->bays + 1); if(s > last) s = last; int z = (zs[s] + zs[s + 1]) / 2; box(m, 0, HH(s), z, HW(s) * 2 / 3, 1, d->sec[s].len * U / 3, 2, s); }
    for(int k = 0; k < d->cargo; k++){ int s = d->nsec / 2, side = (k & 1) ? 1 : -1, z = zs[s] + 4 + (k / 2) * 7; if(z > zs[d->nsec]) break; box(m, side * (HW(s) + 3), 2, z, 3, 3, 3, 1, s); }
    for(int k = 0; k < d->tanks; k++){ int s = last, side = (k & 1) ? 1 : -1, z = zs[s] + 2 + (k / 2) * 5; box(m, side * HW(s) / 2, HH(s) + 2, z, 2, 2, 3, 2, s); }
    { int s = d->nsec / 2, h = HH(s), z = (zs[s] + zs[s + 1]) / 2; int a = addv(m, -4, -h - 1, z - 4), b = addv(m, 4, -h - 1, z - 4), c = addv(m, 4, -h - 1, z + 4), e = addv(m, -4, -h - 1, z + 4); addf(m, 4, a, b, c, e, 6, 1, s); }
    #undef HW
    #undef HH
    int rad = 1; for(int i = 0; i < m->nv; i++){ int x = m->v[i].x, y = m->v[i].y, z = m->v[i].z, q = x < 0 ? -x : x; if(y > q || -y > q) q = y < 0 ? -y : y; if(z > q || -z > q) q = z < 0 ? -z : z; if(q > rad) rad = q; }
    m->len = L; m->rad = rad;
}
