// mine.c: see mine.h
#include "mine.h"
#include "sim_core.h"
#include <string.h>
#include <stdio.h>

enum { MP_SEAM = 120, MP_LUCK, MP_CREAK, MP_CAVE, MP_SEV, MP_GEM, MP_DEATH, MP_KILLS };
#define AUTO_BRACE 2          // the miners prop the roof this far by themselves (4 wood each), when the colony has the wood
// (MN_SAFE_RISK, mine.h) the colony works the deepest level whose daily creak risk is at most this (per mille); deeper takes your braces
#define MN_REFILL_DAYS 120    // every so often the settlers find new veins in the dry levels
#define WOOD_KEEP 12          // workshops and the mine never take the last of the wood

static const char *const WN[MNW_N] = { "SMELTER", "SMITHY", "LOOM" };
static const char *const WM[MNW_N] = { "2 ORE + 1 WOOD = 1 BAR", "2 BARS + 1 WOOD = 1 TOOL", "2 WOOD = 1 CLOTH" };
const char *mn_ws_name(int w){ return w >= 0 && w < MNW_N ? WN[w] : "?"; }
const char *mn_ws_make(int w){ return w >= 0 && w < MNW_N ? WM[w] : "?"; }
int mn_ws_rung(int w){ return w == MNW_SMITHY ? COLR_MINE : COLR_SHOPS; }
int mn_ws_max(uint16_t mask){ return 1 + ((mask >> COLR_MARKET) & 1) + ((mask >> COLR_MUSEUM) & 1); }
int mn_ws_cost(const Mine *m, int w){ return w >= 0 && w < MNW_N ? 15 + 15 * m->wl[w] : 0; }
int mn_zcap(uint16_t mask){
    if(!(mask & (1u << COLR_MINE))) return 0;
    int z = 4; if(mask & (1u << COLR_WALLS)) z += 4; if(mask & (1u << COLR_MARKET)) z += 7;
    return z;
}
int mn_dig_need(int z){ return 4 + 2 * z; }
int mn_dig_wood(int z){ return 6 + 2 * z; }
int mn_seam0(uint32_t town, int z){ return 150 + 40 * z + (int)sim_rngn(town, (uint32_t)z, MP_SEAM, 0, 100); }
int mn_ore_per(int z){ return 1 + z / 8; }
int mn_game_ore(int z, uint32_t salt){
    if(z <= 4) return 4 + (int)(salt % 2u);                   // IRON, COPPER
    if(z <= 6) return 6;                                      // ICE QUARTZ
    if(z <= 8) return 7 + (int)(salt % 3u);                   // SULFUR, OBSIDIAN, CALCITE
    if(z <= 12) return 10;                                    // SCRAP ALLOY
    return salt % 3u == 0 ? 11 : 10;                          // VOIDSTEEL (rare) or SCRAP ALLOY
}

static uint32_t sat(uint32_t a, uint32_t b){ uint64_t s = (uint64_t)a + b; return s > MN_STORE_MAX ? MN_STORE_MAX : (uint32_t)s; }
static int tools_eff(const Mine *m){ return m->tools > 6 ? 6 : (int)m->tools; }

void mn_init(Mine *m, uint32_t town, uint32_t day){ memset(m, 0, sizeof *m); m->town = town; m->last_day = day; }
int mn_miners(const SimColony *c){ int n = 0; for(int i = 0; i < c->nset; i++){ char nm[16]; int j; col_settler(c, i, nm, &j); if(j == COLJ_MINER) n++; } return n; }
int mn_staff(const SimColony *c, int w){
    int n = 0; for(int i = 0; i < c->nset; i++){ char nm[16]; int j; col_settler(c, i, nm, &j);
        if(w == MNW_SMELTER ? j == COLJ_MINER : w == MNW_SMITHY ? j == COLJ_BUILDER : (j == COLJ_COOK || j == COLJ_TRADER)) n++; }
    return n;
}

int mn_risk(const Mine *m, const SimColony *c, int z, int diff){
    if(z < 1 || z > MN_ZMAX) return 0;
    int r = 4 + 3 * z - 2 * m->brace[z] - tools_eff(m) / 2 + mn_miners(c) / 4;
    if(diff == COLD_CALM) r /= 2;
    return r < 1 ? 1 : r;
}
int mn_cave_chance(const Mine *m, int z){ int p = 70 - 8 * (z >= 1 && z <= MN_ZMAX ? m->brace[z] : 0); return p < 10 ? 10 : p; }

int mn_auto_level(const Mine *m, const SimColony *c, int diff){
    int best = 0, low = 0, lowr = 1 << 30;
    for(int z = 1; z <= m->depth; z++){ if(m->zst[z] != MNZ_OPEN || !m->seam[z]) continue; int r = mn_risk(m, c, z, diff);
        if(r < lowr){ lowr = r; low = z; } if(r <= MN_SAFE_RISK) best = z; }
    return best ? best : low;
}

static void ev(MnAway *aw, int kind, uint32_t a, uint32_t b, uint32_t day){
    if(!aw) return;
    if(aw->nev == 8){ memmove(aw->ev, aw->ev + 1, sizeof aw->ev[0] * 7); aw->nev = 7; }
    aw->ev[aw->nev].kind = (uint8_t)kind; aw->ev[aw->nev].a = a; aw->ev[aw->nev].b = b; aw->ev[aw->nev].day = day; aw->nev++;
}
static void open_level(Mine *m, int z, uint32_t day, MnAway *aw){
    m->zst[z] = MNZ_OPEN; m->seam[z] = (uint16_t)mn_seam0(m->town, z); m->brace[z] = 0; m->depth = (uint8_t)z; if(z > m->deepest) m->deepest = (uint8_t)z;
    m->digprog = 0; m->digging = 0; if(aw) aw->dug++; ev(aw, MNE_DUG, (uint32_t)z, 0, day);
}
static void cave_in(Mine *m, SimColony *c, int z, uint32_t day, int diff, ColAway *caw, MnAway *aw){
    int major = z >= 3 && sim_rngn(m->town, day, MP_SEV, (uint64_t)z, 100) < 35;
    m->zst[z] = MNZ_CAVED; m->clear = 0; m->brace[z] = (uint8_t)(m->brace[z] / 3); m->cave_ins = sat(m->cave_ins, 1);
    if(aw) aw->cave_ins++;
    ev(aw, MNE_CAVE, (uint32_t)z, (uint32_t)major, day);
    if(major){
        m->hurt_days = 5;
        if(diff != COLD_CALM && sim_rngn(m->town, day, MP_DEATH, (uint64_t)z, 100) < 60){
            uint32_t n = 1u + (diff == COLD_HARSH && z >= 8 ? 1u : 0u);
            int k = col_lose(c, n, day, caw); if(k){ if(aw) aw->died += (uint32_t)k; ev(aw, MNE_DIED, (uint32_t)k, (uint32_t)z, day); } }
    }
}
static int clear_need(const Mine *m, int z){ (void)m; return z >= 3 ? 5 : 3; }

void mn_day(Mine *m, SimColony *c, uint32_t day, int diff, ColAway *caw, MnAway *aw){
    if(aw) aw->days++;
    m->last_day = day;
    if(c->state != COLS_ALIVE || !(c->mask & (1u << COLR_TENT)) || !c->nset) return;
    uint32_t wood_floor = WOOD_KEEP;
    // ---- the mine
    if((c->mask & (1u << COLR_MINE)) && c->bld[COLB_MINE] >= 1){
        if(!m->depth) open_level(m, 1, day, aw);
        int zcap = mn_zcap(c->mask), miners = mn_miners(c), eff = miners;
        if(m->hurt_days){ m->hurt_days--; eff = 0; }
        if(m->safe) m->warn = 0;
        if(m->warn){                                         // yesterday the roof creaked: today it holds or comes down
            int z = m->work; m->warn = 0;
            if(z >= 1 && z <= m->depth && m->zst[z] == MNZ_OPEN){
                if((int)sim_rngn(m->town, day, MP_CAVE, (uint64_t)z, 100) < mn_cave_chance(m, z)) cave_in(m, c, z, day, diff, caw, aw);
                else ev(aw, MNE_HELD, (uint32_t)z, 0, day); }
        }
        { int bad = m->work < 1 || m->work > m->depth || m->zst[m->work] == MNZ_DRY;      // the colony picks the work level unless you pinned a good one
          if(!m->pin || bad){ int a = mn_auto_level(m, c, diff);
              if(a && a != m->work){ m->work = (uint8_t)a; m->clear = 0; } else if(!a && bad) m->work = m->depth;
              if(m->pin && bad) m->pin = 0; } }
        if(m->work < 1) m->work = 1;
        int z = m->work;
        if(eff > 0 && m->zst[z] == MNZ_OPEN && m->brace[z] < AUTO_BRACE && c->wood >= wood_floor + 4){ c->wood -= 4; m->brace[z]++; }
        if(m->zst[z] == MNZ_CAVED){                          // clear the rubble first
            if(eff > 0){ m->clear = (uint8_t)(m->clear + eff > 250 ? 250 : m->clear + eff);
                if(m->clear >= clear_need(m, z)){ m->zst[z] = m->seam[z] ? MNZ_OPEN : MNZ_DRY; m->clear = 0; ev(aw, MNE_CLEARED, (uint32_t)z, 0, day); } }
        } else if(m->zst[z] == MNZ_OPEN && eff > 0 && m->seam[z]){
            int tb = tools_eff(m); uint32_t got = 0, gem = 0;
            for(int k = 0; k < eff; k++){
                uint32_t y = 1u + (uint32_t)z / 6u + (uint32_t)tb / 4u + sim_rngn(m->town, (uint32_t)k, MP_LUCK, day, 2);
                got += y;
                if(z >= 5 && sim_rngn(m->town, (uint32_t)k, MP_GEM, day, (uint32_t)(24 - z)) == 0) gem++; }
            if(got > m->seam[z]) got = m->seam[z];
            m->seam[z] = (uint16_t)(m->seam[z] - got); m->mined = sat(m->mined, got);
            c->ore = sat(c->ore, got * (uint32_t)mn_ore_per(z)); m->gems = sat(m->gems, gem);
            if(aw){ aw->ore = sat(aw->ore, got * (uint32_t)mn_ore_per(z)); aw->gems = sat(aw->gems, gem); }
            if(!m->seam[z]){ m->zst[z] = MNZ_DRY; ev(aw, MNE_DRY, (uint32_t)z, 0, day); }
            if(m->tools && day % 12 == 0) m->tools--;        // tools wear out
            // the creak: a chance a working level starts to give way. You get a day's warning.
            if(!m->safe && m->zst[z] == MNZ_OPEN && (int)sim_rngn(m->town, day, MP_CREAK, (uint64_t)z, 1000) < mn_risk(m, c, z, diff)){ m->warn = 1; ev(aw, MNE_CREAK, (uint32_t)z, 0, day); }
        }
        // digging the next level: when the work level is half empty (or you started it), and the rules allow a deeper one
        if(m->depth < zcap){
            int zw = m->work; int half = zw >= 1 && zw <= m->depth && m->zst[zw] == MNZ_OPEN && (int)m->seam[zw] * 2 <= mn_seam0(m->town, zw);
            int none = 1; for(int q = 1; q <= m->depth; q++) if(m->zst[q] == MNZ_OPEN && m->seam[q]) none = 0;
            if(half || none) m->digging = 1;
            if(m->digging && eff > 0){
                m->digprog = (uint16_t)(m->digprog + (uint32_t)eff + (uint32_t)(c->bld[COLB_MINE] > 1 ? c->bld[COLB_MINE] - 1 : 0));
                int nz = m->depth + 1;
                if(m->digprog >= mn_dig_need(nz) && c->wood >= wood_floor + (uint32_t)mn_dig_wood(nz)){ c->wood -= (uint32_t)mn_dig_wood(nz); open_level(m, nz, day, aw); }
            }
        }
    }
    if(day % MN_REFILL_DAYS == 0 && m->depth){ for(int q = 1; q <= m->depth; q++) if(m->zst[q] == MNZ_DRY){ m->zst[q] = MNZ_OPEN; m->seam[q] = (uint16_t)(mn_seam0(m->town, q) / 4); } }
    // ---- the workshops. The colony raises a first smelter, loom and smithy by itself once it has wood to spare; later levels are yours.
    if((c->mask & (1u << COLR_SHOPS)) && !m->wl[MNW_SMELTER] && c->wood >= 50){ c->wood -= 15; m->wl[MNW_SMELTER] = 1; if(aw) aw->ws_built++; ev(aw, MNE_WS, MNW_SMELTER, 1, day); }
    if((c->mask & (1u << COLR_SHOPS)) && m->wl[MNW_SMELTER] && !m->wl[MNW_LOOM] && c->nset >= 4 && c->wood >= 70){ c->wood -= 15; m->wl[MNW_LOOM] = 1; if(aw) aw->ws_built++; ev(aw, MNE_WS, MNW_LOOM, 1, day); }
    if((c->mask & (1u << COLR_MINE)) && m->wl[MNW_SMELTER] && !m->wl[MNW_SMITHY] && m->bars >= 6 && c->wood >= 50){ c->wood -= 15; m->wl[MNW_SMITHY] = 1; if(aw) aw->ws_built++; ev(aw, MNE_WS, MNW_SMITHY, 1, day); }
    { uint32_t spare = c->wood > wood_floor ? c->wood - wood_floor : 0;
      if(m->wl[MNW_SMELTER]){ uint32_t cap = (uint32_t)m->wl[MNW_SMELTER] + (mn_staff(c, MNW_SMELTER) > 0 ? 1u : 0u), u = cap;
          if(u > c->ore / 2) u = c->ore / 2; if(u > spare) u = spare;
          c->ore -= 2 * u; c->wood -= u; spare -= u; m->bars = sat(m->bars, u); if(aw) aw->bars = sat(aw->bars, u); }
      if(m->wl[MNW_SMITHY]){ uint32_t cap = (uint32_t)m->wl[MNW_SMITHY] + (mn_staff(c, MNW_SMITHY) > 0 ? 1u : 0u), u = cap;
          if(u > m->bars / 2) u = m->bars / 2; if(u > spare) u = spare; if(m->tools + u > 30u) u = m->tools >= 30u ? 0u : 30u - m->tools;
          m->bars -= 2 * u; c->wood -= u; spare -= u; m->tools = sat(m->tools, u); if(aw) aw->tools = sat(aw->tools, u); }
      if(m->wl[MNW_LOOM]){ uint32_t cap = (uint32_t)m->wl[MNW_LOOM] + (mn_staff(c, MNW_LOOM) > 0 ? 1u : 0u), u = cap;
          if(u > spare / 2) u = spare / 2; if(m->cloth + u > 60u) u = m->cloth >= 60u ? 0u : 60u - m->cloth;
          c->wood -= 2 * u; m->cloth = sat(m->cloth, u); if(aw) aw->cloth = sat(aw->cloth, u); } }
    if(m->scn && !m->safe && !(m->scn_done >> m->scn & 1) && mn_scn_check(m, c, m->scn)){        // the scenario you chose is done: paid once
        int pay = mn_scn_pay(c, m->scn); m->scn_done |= (uint8_t)(1u << m->scn); c->reward_due = c->reward_due + (uint64_t)pay > COL_GOLD_MAX ? COL_GOLD_MAX : c->reward_due + (uint64_t)pay;
        if(aw) aw->goals++; ev(aw, MNE_GOAL, (uint32_t)m->scn, (uint32_t)pay, day); }
    if(day % 7 == 0 && m->cloth){                            // a week: the settlers wear cloth through, and are the better for it
        uint32_t need = ((uint32_t)c->nset + 3u) / 4u;
        if(m->cloth >= need){ m->cloth -= need; if(c->morale < 100) c->morale = (uint8_t)(c->morale + 3 > 100 ? 100 : c->morale + 3); if(c->health < 100) c->health++; } }
}

typedef struct { Mine *m; MnAway *aw; ColAway *caw; int diff; } MnCtx;
static void mn_after(SimColony *c, uint32_t day, void *ud){ MnCtx *x = (MnCtx *)ud; mn_day(x->m, c, day, x->diff, x->caw, x->aw); }
int mn_catchup(SimColony *c, Mine *m, uint32_t today, uint32_t year_days, int diff, ColAway *caw, MnAway *aw){
    if(aw) memset(aw, 0, sizeof *aw);
    MnCtx x = { m, aw, caw, diff };
    int n = col_catchup_hook(c, today, year_days, diff, caw, mn_after, &x);
    if(n > 0 && aw) aw->days = (uint32_t)n;
    m->last_day = c->last_day;
    return n;
}

// ---- by hand
int mn_build_ws(Mine *m, SimColony *c, int w){
    if(w < 0 || w >= MNW_N || c->state != COLS_ALIVE || !(c->mask & (1u << mn_ws_rung(w)))) return 0;
    if(m->wl[w] >= mn_ws_max(c->mask)) return 0;
    uint32_t wood = (uint32_t)mn_ws_cost(m, w), bars = (w == MNW_SMITHY ? 2u * m->wl[w] : 0u);
    if(c->wood < wood || m->bars < bars) return 0;
    c->wood -= wood; m->bars -= bars; m->wl[w]++; return 1;
}
int mn_brace(Mine *m, SimColony *c, int strong){
    int z = m->work; if(c->state != COLS_ALIVE || z < 1 || z > m->depth || m->zst[z] != MNZ_OPEN || m->brace[z] >= 9) return 0;
    if(strong){ if(c->wood < 2 || m->bars < 1) return 0; c->wood -= 2; m->bars--; m->brace[z] = (uint8_t)(m->brace[z] + 3 > 9 ? 9 : m->brace[z] + 3); }
    else { if(c->wood < 4) return 0; c->wood -= 4; m->brace[z]++; }
    return 1;
}
int mn_dig_help(Mine *m, SimColony *c){
    if(c->state != COLS_ALIVE || !m->depth || m->depth >= mn_zcap(c->mask)) return 0;
    m->digging = 1; m->digprog = (uint16_t)(m->digprog + 4);
    int nz = m->depth + 1; if(m->digprog >= mn_dig_need(nz) && c->wood >= (uint32_t)mn_dig_wood(nz)){ c->wood -= (uint32_t)mn_dig_wood(nz); open_level(m, nz, m->last_day, 0); }
    return 1;
}
int mn_clear_help(Mine *m, SimColony *c){
    int z = m->work; if(c->state != COLS_ALIVE || z < 1 || z > m->depth || m->zst[z] != MNZ_CAVED) return 0;
    m->clear = (uint8_t)(m->clear + 3); if(m->clear >= clear_need(m, z)){ m->zst[z] = m->seam[z] ? MNZ_OPEN : MNZ_DRY; m->clear = 0; } return 1;
}
int mn_set_work(Mine *m, int z){
    if(z == 0){ m->pin = 0; return 1; }
    if(z < 1 || z > m->depth || m->zst[z] == MNZ_SOLID || m->zst[z] == MNZ_DRY) return 0;
    m->work = (uint8_t)z; m->pin = 1; m->clear = 0; return 1;
}
int mn_collect(Mine *m, MnGift *g){
    memset(g, 0, sizeof *g);
    uint32_t take = m->gems > MN_TAKE_GEMS ? MN_TAKE_GEMS : m->gems;
    g->bars = m->bars; g->tools = m->tools; g->cloth = m->cloth; g->gems = take;
    g->gold = (int64_t)m->bars * 9 + (int64_t)m->tools * 30 + (int64_t)m->cloth * 10 + (int64_t)take * 60;
    m->bars = 0; m->tools = 0; m->cloth = 0; m->gems -= take;
    g->ore_n = (int)take; g->ore_kind = take ? mn_game_ore(m->deepest, m->mined) : 0;
    return g->gold != 0;
}

// ---- the replay layer
static const char *const SN[MNS_N] = { "NONE", "FEED 20", "REACH LEVEL 8", "SURVIVE 3 RAIDS", "MAKE 10 TOOLS", "CLOTH FOR ALL", "TWENTY GEMS" };
static const char *const SA[MNS_N] = { "NO SCENARIO. THE COLONY JUST LIVES", "GROW THE COLONY TO 20 SETTLERS", "DIG DOWN TO LEVEL 8", "BEAT THREE RAIDS", "HOLD 10 TOOLS IN STOCK", "HOLD 30 CLOTH IN STOCK", "HOLD 20 GEMS IN STOCK" };
const char *mn_scn_name(int s){ return s >= 0 && s < MNS_N ? SN[s] : "?"; }
const char *mn_scn_about(int s){ return s >= 0 && s < MNS_N ? SA[s] : "?"; }
int mn_scn_pay(const SimColony *c, int s){ return s > 0 && s < MNS_N ? 150 * s * (1 + c->danger) : 0; }
int mn_scn_check(const Mine *m, const SimColony *c, int s){
    switch(s){
    case MNS_FEED20: return c->nset >= 20;
    case MNS_DEEP8: return m->deepest >= 8;
    case MNS_RAIDS3: return c->raids_won >= 3;
    case MNS_TOOLS10: return m->tools >= 10;
    case MNS_CLOTH30: return m->cloth >= 30;
    case MNS_GEMS20: return m->gems >= 20;
    default: return 0; }
}
int mn_scn_set(Mine *m, int s){ if(s < 0 || s >= MNS_N || m->scn == s) return 0; m->scn = (uint8_t)s; return 1; }
int mn_scn_next(Mine *m){ int s = m->scn; for(int k = 0; k < MNS_N; k++){ s = (s + 1) % MNS_N; if(s == 0 || !(m->scn_done >> s & 1)) break; } m->scn = (uint8_t)s; return s; }
void mn_set_safe(Mine *m, int on){ m->safe = on ? 1 : 0; if(m->safe) m->warn = 0; }

// ---- text
int mn_event_text(const MnEvent *e, char *o, size_t n){
    int k = 0;
    switch(e->kind){
    case MNE_DUG: k = snprintf(o, n, "LEVEL %u OPENED", e->a); break;
    case MNE_CREAK: k = snprintf(o, n, "THE ROOF CREAKS AT LEVEL %u", e->a); break;
    case MNE_CAVE: k = snprintf(o, n, e->b ? "BIG CAVE-IN AT LEVEL %u" : "CAVE-IN AT LEVEL %u", e->a); break;
    case MNE_HELD: k = snprintf(o, n, "THE BRACES HELD AT LEVEL %u", e->a); break;
    case MNE_DRY: k = snprintf(o, n, "LEVEL %u RAN DRY", e->a); break;
    case MNE_CLEARED: k = snprintf(o, n, "LEVEL %u RUBBLE CLEARED", e->a); break;
    case MNE_DIED: k = snprintf(o, n, "%u MINER%s LOST AT LEVEL %u", e->a, e->a == 1 ? "" : "S", e->b); break;
    case MNE_GOAL: k = snprintf(o, n, "SCENARIO DONE: %s +%uG", mn_scn_name((int)e->a), e->b); break;
    case MNE_WS: k = snprintf(o, n, "A %s WAS BUILT", mn_ws_name((int)e->a)); break;
    default: k = snprintf(o, n, "..."); }
    return k < 0 ? 0 : k;
}
int mn_away_text(const MnAway *a, const Mine *m, char lines[][48], int max){
    int n = 0; if(max < 1 || !a->days) return 0;
    #define L(...) do{ if(n < max){ snprintf(lines[n], 48, __VA_ARGS__); n++; } }while(0)
    if(m->depth) L("MINE: LEVEL %u OF %u DUG. +%u ORE", (unsigned)m->depth, (unsigned)MN_ZMAX, a->ore);
    if(a->bars || a->tools || a->cloth) L("MADE %u BARS %u TOOLS %u CLOTH", a->bars, a->tools, a->cloth);
    if(a->gems) L("FOUND %u GEMS", a->gems);
    if(a->cave_ins) L("%u CAVE-INS, %u MINERS LOST", a->cave_ins, a->died);
    if(a->goals) L("SCENARIO DONE: %s", mn_scn_name(m->scn));
    #undef L
    return n;
}
void mn_level_text(const Mine *m, const SimColony *c, int z, int diff, char *o, size_t n){
    if(z < 1 || z > MN_ZMAX){ if(n) o[0] = 0; return; }
    const char *s = m->zst[z] == MNZ_OPEN ? "OPEN" : m->zst[z] == MNZ_CAVED ? "CAVED IN" : m->zst[z] == MNZ_DRY ? "DRY" : "SOLID";
    if(m->zst[z] == MNZ_SOLID) snprintf(o, n, "LV%d %s", z, z == m->depth + 1 && z <= mn_zcap(c->mask) ? "NEXT TO DIG" : "ROCK: NEEDS A HIGHER RUNG");
    else if(m->zst[z] == MNZ_CAVED) snprintf(o, n, "LV%d %s RUBBLE %u/%d", z, s, (unsigned)(m->work == z ? m->clear : 0), clear_need(m, z));
    else snprintf(o, n, "LV%d %s%s SEAM %u BRACE %u RISK %d", z, s, m->work == z ? " *" : "", (unsigned)m->seam[z], (unsigned)m->brace[z], mn_risk(m, c, z, diff));
}
void mn_status_text(const Mine *m, char *o, size_t n){
    if(m->warn) snprintf(o, n, "CREAKING! BRACE LEVEL %u NOW", (unsigned)m->work);
    else if(m->hurt_days) snprintf(o, n, "MINERS HURT: %u DAYS OF REST", (unsigned)m->hurt_days);
    else if(m->depth) snprintf(o, n, "LEVEL %u OF %u IN WORK", (unsigned)m->work, (unsigned)m->depth);
    else snprintf(o, n, "NO SHAFT YET: BUILD A MINE");
}
size_t mn_dump(const Mine *m, char *o, size_t n){
    int k = snprintf(o, n, "mine town %08x depth %u work %u ws %u/%u/%u bars %u tools %u cloth %u gems %u mined %u caveins %u warn %u hurt %u scn %u safe %u day %u\n",
                     m->town, m->depth, m->work, m->wl[0], m->wl[1], m->wl[2], m->bars, m->tools, m->cloth, m->gems, m->mined, m->cave_ins, m->warn, m->hurt_days, m->scn, m->safe, m->last_day);
    return k < 0 ? 0 : (size_t)k;
}

// ---- the file
static void p16(uint8_t *p, uint32_t v){ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *p, uint32_t v){ p16(p, v); p16(p + 2, v >> 16); }
static uint32_t g16(const uint8_t *p){ return p[0] | (uint32_t)p[1] << 8; }
static uint32_t g32(const uint8_t *p){ return g16(p) | g16(p + 2) << 16; }
Mine *mn_find(MineList *l, uint32_t town, int make, uint32_t day){
    for(int i = 0; i < l->n; i++) if(l->m[i].town == town) return &l->m[i];
    if(!make) return 0;
    int at = l->n;
    if(l->n >= COL_MAX){ at = 0; for(int i = 1; i < l->n; i++) if(l->m[i].last_day < l->m[at].last_day) at = i; }
    else l->n++;
    mn_init(&l->m[at], town, day); return &l->m[at];
}
size_t mn_list_write(const MineList *l, uint8_t *o, size_t cap){
    size_t need = 1 + (size_t)l->n * MN_REC_BYTES; if(cap < need) return 0;
    memset(o, 0, need); o[0] = l->n;
    for(int i = 0; i < l->n; i++){ const Mine *m = &l->m[i]; uint8_t *q = o + 1 + i * MN_REC_BYTES;
        p32(q, m->town); q[4] = m->wl[0]; q[5] = m->wl[1]; q[6] = m->wl[2]; q[7] = m->depth; q[8] = m->work; q[9] = (uint8_t)(m->pin | m->digging << 1);
        q[10] = m->warn; q[11] = m->hurt_days; q[12] = m->clear; q[13] = m->deepest; q[109] = m->scn; q[110] = m->scn_done; q[111] = m->safe; p16(q + 14, m->digprog);
        memcpy(q + 16, m->zst, 16); memcpy(q + 32, m->brace, 16); for(int z = 0; z < 16; z++) p16(q + 48 + z * 2, m->seam[z]);
        p32(q + 80, m->bars); p32(q + 84, m->tools); p32(q + 88, m->cloth); p32(q + 92, m->gems); p32(q + 96, m->cave_ins); p32(q + 100, m->mined); p32(q + 104, m->last_day); }
    return need;
}
static uint32_t cl(uint32_t v){ return v > MN_STORE_MAX ? MN_STORE_MAX : v; }
int mn_list_read(MineList *l, const uint8_t *in, size_t n){
    memset(l, 0, sizeof *l);
    if(n < 1 || in[0] > COL_MAX || n != 1 + (size_t)in[0] * MN_REC_BYTES) return -1;
    l->n = in[0];
    for(int i = 0; i < l->n; i++){ Mine *m = &l->m[i]; const uint8_t *q = in + 1 + i * MN_REC_BYTES; mn_init(m, g32(q), 0);
        for(int w = 0; w < MNW_N; w++) m->wl[w] = q[4 + w] > 3 ? 3 : q[4 + w];
        m->depth = q[7] > MN_ZMAX ? MN_ZMAX : q[7]; m->work = q[8] > m->depth ? m->depth : q[8]; m->pin = q[9] & 1; m->digging = (q[9] >> 1) & 1;
        m->warn = q[10] ? 1 : 0; m->hurt_days = q[11] > 5 ? 5 : q[11]; m->clear = q[12]; m->deepest = q[13] > MN_ZMAX ? MN_ZMAX : q[13]; m->digprog = (uint16_t)g16(q + 14);
        for(int z = 1; z <= MN_ZMAX; z++){ m->zst[z] = q[16 + z] > MNZ_DRY ? MNZ_SOLID : q[16 + z]; m->brace[z] = q[32 + z] > 9 ? 9 : q[32 + z]; m->seam[z] = (uint16_t)g16(q + 48 + z * 2);
            if(z > m->depth){ m->zst[z] = MNZ_SOLID; m->seam[z] = 0; m->brace[z] = 0; } }
        if(m->deepest < m->depth) m->deepest = m->depth;
        m->scn = q[109] < MNS_N ? q[109] : 0; m->scn_done = (uint8_t)(q[110] & ((1u << MNS_N) - 2u)); m->safe = q[111] ? 1 : 0;
        m->bars = cl(g32(q + 80)); m->tools = cl(g32(q + 84)); m->cloth = cl(g32(q + 88)); m->gems = cl(g32(q + 92)); m->cave_ins = cl(g32(q + 96)); m->mined = cl(g32(q + 100)); m->last_day = g32(q + 104);
        if(m->warn && (!m->work || m->zst[m->work] != MNZ_OPEN)) m->warn = 0; }
    return 0;
}
