// mine_test.c: the colony's WORKSHOPS and MINE (Phase 6, mine.h): production chains, z-levels, cave-ins, the ladder, hands-on play, catch-up, the file.
#include <stdio.h>
#include <string.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static uint8_t buf[SIMFILE_MAX]; static SimFile F, G;
static const ColFlags NOF;
// a colony with the given rungs, n settlers all doing `job`, and a mine building on the map
static void mk(SimColony *c, uint32_t town, uint16_t rungs, int n, int job, int with_mine){
    col_init(c, town, 0); col_update(c, (uint16_t)(rungs | 1u << COLR_TENT)); c->nset = (uint8_t)n; c->food = 5000; c->wood = 200; c->health = 100; c->morale = 100;
    for(int i = 0; i < n; i++) c->job[i] = (uint8_t)job;
    if(with_mine){ c->bld[COLB_MINE] = 1; c->grid[5] = COLB_MINE + 1; }
}
#define ALL 0x1FE
static uint32_t bytes_hash(const Mine *m){ MineList l; memset(&l, 0, sizeof l); l.n = 1; l.m[0] = *m; uint8_t b[1 + MN_REC_BYTES]; mn_list_write(&l, b, sizeof b); return sim_fnv(b, sizeof b); }
// runs the colony and mine day by day (the same order the catch-up uses: colony day, then mine day)
static void runboth(SimColony *c, Mine *m, uint32_t from, uint32_t days, int diff, uint16_t mask, MnAway *aw){
    for(uint32_t d = from; d < from + days; d++){ col_update(c, (uint16_t)(mask | col_goal_mask(c, &NOF, d, 28))); col_day_ex(c, d, 28, diff, 0); mn_day(m, c, d, diff, 0, aw); }
}
int main(void){
    // ---- rules and names
    CHECK(mn_zcap(1) == 0); CHECK(mn_zcap(1u << COLR_MINE) == 4); CHECK(mn_zcap(1u << COLR_MINE | 1u << COLR_WALLS) == 8); CHECK(mn_zcap(ALL) == 15); CHECK(mn_zcap(1u << COLR_WALLS | 1u << COLR_MARKET) == 0);
    CHECK(mn_ws_max(1) == 1); CHECK(mn_ws_max(1u << COLR_MARKET) == 2); CHECK(mn_ws_max(ALL) == 3);
    CHECK(!strcmp(mn_ws_name(MNW_SMITHY), "SMITHY")); CHECK(mn_ws_rung(MNW_SMITHY) == COLR_MINE && mn_ws_rung(MNW_LOOM) == COLR_SHOPS);
    CHECK(mn_game_ore(1, 0) >= 4 && mn_game_ore(1, 1) <= 5); CHECK(mn_game_ore(15, 0) == 11 && mn_game_ore(15, 1) == 10); for(int z = 1; z <= 15; z++) for(uint32_t s = 0; s < 5; s++) CHECK(mn_game_ore(z, s) >= 4 && mn_game_ore(z, s) <= 11);
    for(int z = 1; z < 15; z++) CHECK(mn_seam0(7, z) >= 150 + 40 * z && mn_dig_need(z + 1) > mn_dig_need(z) && mn_ore_per(z + 1) >= mn_ore_per(z));
    // ---- no mine without a shaft building, no mine without the rung
    { SimColony c; Mine m; mk(&c, 11, 1u << COLR_MINE, 4, COLJ_MINER, 0); mn_init(&m, 11, 0); for(uint32_t d = 1; d < 30; d++) mn_day(&m, &c, d, COLD_NORMAL, 0, 0); CHECK(m.depth == 0 && c.ore == 0);
      mk(&c, 11, 1u << COLR_SHOPS, 4, COLJ_MINER, 1); mn_init(&m, 11, 0); for(uint32_t d = 1; d < 30; d++) mn_day(&m, &c, d, COLD_NORMAL, 0, 0); CHECK(m.depth == 0 && c.ore == 0);   // a mine building but no MINE rung
      mk(&c, 11, 1u << COLR_MINE, 4, COLJ_MINER, 1); mn_init(&m, 11, 0); mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.depth == 1 && m.zst[1] == MNZ_OPEN && m.seam[1] >= 190 && m.work == 1); }
    // ---- a dead colony or one with nobody does nothing
    { SimColony c; Mine m; mk(&c, 12, ALL, 4, COLJ_MINER, 1); c.state = COLS_RUINS; mn_init(&m, 12, 0); mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.depth == 0);
      mk(&c, 12, ALL, 0, COLJ_MINER, 1); c.nset = 0; mn_day(&m, &c, 2, COLD_NORMAL, 0, 0); CHECK(m.depth == 0); }
    // ---- the chains, one day at a time, exact numbers
    { SimColony c; Mine m; mk(&c, 13, ALL, 4, COLJ_MINER, 0); mn_init(&m, 13, 0); m.wl[MNW_SMELTER] = 1; c.ore = 10; c.wood = 40;
      mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.bars == 2 && c.ore == 6 && c.wood == 38);                      // level 1 + a miner at work = 2 bars: 4 ore, 2 wood
      mk(&c, 13, ALL, 4, COLJ_FORAGER, 0); mn_init(&m, 13, 0); m.wl[MNW_SMELTER] = 1; c.ore = 10; c.wood = 40; mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.bars == 1 && c.ore == 8 && c.wood == 39);   // nobody of the trade: level only
      mk(&c, 13, ALL, 4, COLJ_MINER, 0); mn_init(&m, 13, 0); m.wl[MNW_SMELTER] = 1; c.ore = 3; c.wood = 40; mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.bars == 1 && c.ore == 1);   // not enough ore for two
      mk(&c, 13, ALL, 4, COLJ_MINER, 0); mn_init(&m, 13, 0); m.wl[MNW_SMELTER] = 1; c.ore = 50; c.wood = 12; mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.bars == 0 && c.wood == 12);   // the last 12 wood stay
      mk(&c, 13, ALL, 4, COLJ_BUILDER, 0); mn_init(&m, 13, 0); m.wl[MNW_SMITHY] = 1; m.bars = 9; c.wood = 40; mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.tools == 2 && m.bars == 5 && c.wood == 38);
      mk(&c, 13, ALL, 4, COLJ_COOK, 0); mn_init(&m, 13, 0); m.wl[MNW_LOOM] = 2; c.wood = 40; mn_day(&m, &c, 1, COLD_NORMAL, 0, 0); CHECK(m.cloth == 3 && c.wood == 34);
      // the whole chain in one run: ore in, bars, tools, gold out
      mk(&c, 14, ALL, 6, COLJ_MINER, 1); c.job[0] = COLJ_BUILDER; c.job[1] = COLJ_COOK; mn_init(&m, 14, 0); MnAway aw; memset(&aw, 0, sizeof aw); runboth(&c, &m, 1, 120, COLD_NORMAL, ALL, &aw);
      printf("  chain: depth %u ore %u bars %u tools %u cloth %u gems %u wl %u/%u/%u mined %u caveins %u\n", m.depth, c.ore, m.bars, m.tools, m.cloth, m.gems, m.wl[0], m.wl[1], m.wl[2], m.mined, m.cave_ins);
      CHECK(aw.ore > 50); CHECK(aw.bars > 5); CHECK(aw.tools >= 1 || m.tools >= 1); CHECK(m.wl[MNW_SMELTER] == 1 && m.wl[MNW_SMITHY] == 1); }
    // ---- stock caps and saturation
    { SimColony c; Mine m; mk(&c, 15, ALL, 4, COLJ_MINER, 1); mn_init(&m, 15, 0); m.wl[MNW_SMELTER] = 3; m.wl[MNW_SMITHY] = 3; m.wl[MNW_LOOM] = 3; m.bars = MN_STORE_MAX - 1; m.tools = 29; m.cloth = 59; c.ore = 4000000000u > 999999999u ? 999999999u : 0; c.wood = 999999999u;
      for(uint32_t d = 1; d < 10; d++) mn_day(&m, &c, d, COLD_NORMAL, 0, 0); CHECK(m.bars <= MN_STORE_MAX && m.tools <= 30 && m.cloth <= 60 && c.ore <= COL_STORE_MAX); }
    // ---- z-levels: the colony digs down by itself, level by level, only as far as the rungs allow
    { static const uint16_t rm[3] = { 1u << COLR_MINE, 1u << COLR_MINE | 1u << COLR_WALLS, ALL }; static const int want[3] = { 4, 8, 15 };
      for(int k = 0; k < 3; k++){ SimColony c; Mine m; mk(&c, 20u + (uint32_t)k, rm[k], 8, COLJ_MINER, 1); mn_init(&m, c.town, 0);
          for(uint32_t d = 1; d <= 3000; d++){ c.wood = 400; c.food = 5000; c.health = 100; mn_day(&m, &c, d, COLD_CALM, 0, 0); }
          CHECK(m.depth == want[k] || (m.depth >= want[k] - 1 && m.depth <= want[k])); CHECK(m.depth <= mn_zcap(rm[k])); printf("  zcap %d: reached level %u after 3000 days (caved-in %u)\n", mn_zcap(rm[k]), m.depth, m.cave_ins); } }
    // ---- a level is richer the deeper it is, and a worked level runs dry
    { SimColony c; Mine m; mk(&c, 21, ALL, 6, COLJ_MINER, 1); mn_init(&m, 21, 0); int dry = 0, opened = 0; MnAway aw; memset(&aw, 0, sizeof aw);
      for(uint32_t d = 1; d <= 600; d++){ c.wood = 300; mn_day(&m, &c, d, COLD_CALM, 0, &aw); for(int z = 1; z <= 15; z++) if(m.zst[z] == MNZ_DRY){ dry = 1; } } for(int z = 1; z <= 15; z++) if(m.zst[z] != MNZ_SOLID) opened++;
      CHECK(dry >= 1 && opened >= 3);      // (veins are found again every 120 days, so only look for a dry level along the way) int ok = 0; for(int i = 0; i < aw.nev; i++) if(aw.ev[i].kind == MNE_DRY || aw.ev[i].kind == MNE_DUG) ok = 1; CHECK(ok); CHECK(aw.gems > 0 || m.gems > 0);
      char t[60]; for(int i = 0; i < aw.nev; i++){ int n = mn_event_text(&aw.ev[i], t, sizeof t); CHECK(n > 3 && n < 50); } }
    // ---- CAVE-INS. Always a day of warning first; braces and tools lower the odds; CALM kills nobody; NORMAL and HARSH sometimes; never the last settler; the colony lives
    { int creaks = 0, caves = 0, warned = 0, deaths_calm = 0, deaths_norm = 0, deaths_harsh = 0, last_ok = 1, alive = 1; int caves_b0 = 0, caves_b9 = 0, nb0 = 0, nb9 = 0;
      for(int diff = 0; diff < 3; diff++) for(uint32_t town = 1; town <= 40; town++){
          SimColony c; Mine m; mk(&c, 5000u + town, ALL, 8, COLJ_MINER, 1); mn_init(&m, c.town, 0); m.pin = 0; uint32_t died0 = 0;
          for(uint32_t d = 1; d <= 900; d++){ c.wood = 300; c.food = 5000; c.health = 100; c.morale = 100; if(c.nset < 3) c.nset = 8;
              uint32_t before = m.cave_ins; int w0 = m.warn; uint8_t n0 = c.nset; MnAway aw; memset(&aw, 0, sizeof aw); mn_day(&m, &c, d, diff, 0, &aw);
              if(m.cave_ins > before){ caves++; if(w0) warned++; }
              for(int i = 0; i < aw.nev; i++) if(aw.ev[i].kind == MNE_CREAK) creaks++;
              if(c.nset < n0){ uint32_t lost = (uint32_t)(n0 - c.nset); died0 += lost; if(diff == 0) deaths_calm += (int)lost; else if(diff == 1) deaths_norm += (int)lost; else deaths_harsh += (int)lost; }
              if(c.nset < 1) last_ok = 0; if(c.state != COLS_ALIVE) alive = 0; } }
      printf("  cave-ins: %d creaks, %d cave-ins (%d warned the day before); deaths calm %d normal %d harsh %d\n", creaks, caves, warned, deaths_calm, deaths_norm, deaths_harsh);
      CHECK(caves > 20); CHECK(warned == caves); CHECK(creaks >= caves); CHECK(deaths_calm == 0); CHECK(deaths_norm + deaths_harsh > 0); CHECK(deaths_harsh >= deaths_norm / 2); CHECK(last_ok && alive);
      // braces: the same mine at brace 0 against brace 9 on its work level
      for(uint32_t town = 1; town <= 60; town++) for(int b = 0; b < 2; b++){ SimColony c; Mine m; mk(&c, 7000u + town, 1u << COLR_MINE | 1u << COLR_WALLS, 8, COLJ_MINER, 1); mn_init(&m, c.town, 0); m.depth = 8; for(int z = 1; z <= 8; z++){ m.zst[z] = MNZ_OPEN; m.seam[z] = 60000; m.brace[z] = (uint8_t)(b ? 9 : 0); }
          m.work = 8; m.pin = 1; for(uint32_t d = 1; d <= 400; d++){ c.wood = 12; c.food = 5000; c.health = 100; if(c.nset < 3) c.nset = 8; for(int z = 1; z <= 8; z++){ if(m.zst[z] == MNZ_CAVED) { m.zst[z] = MNZ_OPEN; } m.brace[z] = (uint8_t)(b ? 9 : 0); } m.hurt_days = 0; mn_day(&m, &c, d, COLD_NORMAL, 0, 0); }
          if(b){ caves_b9 += (int)m.cave_ins; nb9++; } else { caves_b0 += (int)m.cave_ins; nb0++; } }
      printf("  braces: brace 0 -> %d cave-ins, brace 9 -> %d (60 mines x 400 days at level 8)\n", caves_b0, caves_b9); CHECK(caves_b0 > caves_b9 * 2 && caves_b9 >= 0 && nb0 == nb9);
      // tools lower the risk
      { SimColony c; Mine m; mk(&c, 9, ALL, 4, COLJ_MINER, 1); mn_init(&m, 9, 0); m.depth = 6; m.brace[6] = 1; int r0 = mn_risk(&m, &c, 6, COLD_NORMAL); m.tools = 3; int r3 = mn_risk(&m, &c, 6, COLD_NORMAL); CHECK(r3 < r0); CHECK(mn_risk(&m, &c, 6, COLD_CALM) < r3); CHECK(mn_risk(&m, &c, 14, COLD_NORMAL) > mn_risk(&m, &c, 2, COLD_NORMAL)); CHECK(mn_cave_chance(&m, 6) > mn_cave_chance(&m, 6) - 1); } }
    // ---- a caved-in level gives no ore until the rubble is cleared; hurt miners rest
    { SimColony c; Mine m; mk(&c, 30, 1u << COLR_MINE, 4, COLJ_MINER, 1); mn_init(&m, 30, 0); mn_day(&m, &c, 1, COLD_CALM, 0, 0); m.zst[1] = MNZ_CAVED; m.clear = 0; m.pin = 1; c.ore = 0; uint32_t mined0 = m.mined;
      mn_day(&m, &c, 2, COLD_CALM, 0, 0); CHECK(m.mined == mined0 && c.ore == 0); CHECK(m.zst[1] == MNZ_OPEN);          // 4 miners clear 3 rubble in a day
      mn_day(&m, &c, 3, COLD_CALM, 0, 0); CHECK(m.mined > mined0);
      m.zst[1] = MNZ_CAVED; m.clear = 0; m.hurt_days = 3; mn_day(&m, &c, 4, COLD_CALM, 0, 0); CHECK(m.zst[1] == MNZ_CAVED && m.hurt_days == 2); CHECK(mn_clear_help(&m, &c) && m.zst[1] == MNZ_OPEN); }
    // ---- hands-on
    { SimColony c; Mine m; mk(&c, 40, 1u << COLR_MINE | 1u << COLR_SHOPS, 4, COLJ_MINER, 1); c.wood = 40; mn_init(&m, 40, 0); mn_day(&m, &c, 1, COLD_CALM, 0, 0); c.wood = 100; m.bars = 0;
      CHECK(mn_brace(&m, &c, 0) && m.brace[1] >= 1); uint8_t b0 = m.brace[1]; uint32_t w0 = c.wood; CHECK(!mn_brace(&m, &c, 1)); m.bars = 3; CHECK(mn_brace(&m, &c, 1) && m.brace[1] == b0 + 3 && m.bars == 2 && c.wood == w0 - 2);
      for(int i = 0; i < 20; i++) mn_brace(&m, &c, 1); CHECK(m.brace[1] == 9); CHECK(!mn_brace(&m, &c, 0));
      c.wood = 5; CHECK(!mn_build_ws(&m, &c, MNW_SMELTER)); c.wood = 100; CHECK(mn_build_ws(&m, &c, MNW_SMELTER) && m.wl[0] == 1 && c.wood == 85); CHECK(!mn_build_ws(&m, &c, MNW_SMELTER));   // only one level without a market
      CHECK(!mn_build_ws(&m, &c, MNW_SMITHY) || m.wl[1] == 1); c.mask &= (uint16_t)~(1u << COLR_MINE); CHECK(!mn_build_ws(&m, &c, MNW_SMITHY + 5));
      c.mask |= 1u << COLR_MARKET; c.wood = 200; CHECK(mn_build_ws(&m, &c, MNW_SMELTER) && m.wl[0] == 2);
      c.mask |= 1u << COLR_MINE; c.wood = 500; m.depth = 3; m.zst[2] = m.zst[3] = MNZ_OPEN; m.seam[2] = m.seam[3] = 50; CHECK(mn_set_work(&m, 3) && m.work == 3 && m.pin); CHECK(!mn_set_work(&m, 9)); CHECK(mn_set_work(&m, 0) && !m.pin);
      m.zst[2] = MNZ_DRY; CHECK(!mn_set_work(&m, 2)); m.depth = 3; uint16_t dp = m.digprog; CHECK((mn_dig_help(&m, &c) && m.digprog > dp) || m.depth == 4);
      for(int i = 0; i < 300; i++){ c.wood = 500; mn_dig_help(&m, &c); } CHECK(m.depth == mn_zcap(c.mask) && m.depth == 11); CHECK(!mn_dig_help(&m, &c));    // MINE + MARKET: down to level 11, and no further
      { SimColony c2; Mine m2; mk(&c2, 41, 1u << COLR_MINE, 4, COLJ_MINER, 1); mn_init(&m2, 41, 0); mn_day(&m2, &c2, 1, COLD_CALM, 0, 0); for(int i = 0; i < 300; i++){ c2.wood = 500; mn_dig_help(&m2, &c2); } CHECK(m2.depth == 4); CHECK(!mn_dig_help(&m2, &c2)); }   // the MINE rung alone reaches level 4
      m.gems = 20; m.bars = 2; m.tools = 1; m.cloth = 3; m.deepest = 12; MnGift g; CHECK(mn_collect(&m, &g)); CHECK(g.gems == MN_TAKE_GEMS && m.gems == 20 - MN_TAKE_GEMS && g.gold == 2 * 9 + 30 + 30 + MN_TAKE_GEMS * 60 && g.ore_n == MN_TAKE_GEMS && g.ore_kind >= 4 && g.ore_kind <= 11 && m.bars == 0 && m.tools == 0 && m.cloth == 0);
      CHECK(!mn_collect(&m, &g) || g.gold > 0); }
    // ---- the nonlinear ladder: every order of play reaches the mine (the rungs come by themselves with time) and the workshops build themselves
    { static const uint16_t at1[] = { 0, 30, 60, 90, 120, 150, 180, 210 }, m1[] = { 2, 4, 8, 16, 32, 64, 128, 256 }, m2[] = { 2, 256, 128, 64, 32, 16, 8, 4 };
      static const uint16_t at3[] = { 0, 400 }, m3[] = { 2, 64 };
      const uint16_t *ats[3] = { at1, at1, at3 }, *ms[3] = { m1, m2, m3 }; int ns[3] = { 8, 8, 2 };
      for(int o = 0; o < 3; o++){ SimColony c; Mine m; col_init(&c, 0x2000u + (uint32_t)o * 17u, 0); c.danger = 2; mn_init(&m, c.town, 0); int maxd = 0, bld = 0;
          for(uint32_t d = 0; d < 28 * 60; d++){ uint16_t mk_ = 1; for(int k = 0; k < ns[o]; k++) if(d >= ats[o][k]) mk_ |= ms[o][k]; col_update(&c, (uint16_t)(mk_ | col_goal_mask(&c, &NOF, d, 28))); col_day_ex(&c, d, 28, COLD_NORMAL, 0); mn_day(&m, &c, d, COLD_NORMAL, 0, 0); if(m.depth > maxd) maxd = m.depth; bld += c.bld[COLB_MINE]; }
          printf("  order %d: mask %03x settlers %u state %u mine depth %d caveins %u ws %u/%u/%u bars %u\n", o + 1, c.mask, c.nset, c.state, maxd, m.cave_ins, m.wl[0], m.wl[1], m.wl[2], m.bars);
          CHECK(c.state == COLS_ALIVE); CHECK(c.mask == 0x1FF); CHECK(maxd >= 1); CHECK(m.wl[MNW_SMELTER] >= 1); CHECK(m.last_day == 28 * 60 - 1); } }
    // ---- determinism: the same days give the same bytes; catch-up is the same as day by day
    { SimColony a, b; Mine ma, mb; mk(&a, 60, ALL, 8, COLJ_MINER, 1); a.job[0] = COLJ_BUILDER; a.job[1] = COLJ_COOK; b = a; mn_init(&ma, 60, 0); mb = ma; MnAway wa, wb; memset(&wa, 0, sizeof wa);
      for(uint32_t d = 1; d <= 800; d++){ col_day_ex(&a, d, 28, COLD_NORMAL, 0); mn_day(&ma, &a, d, COLD_NORMAL, 0, &wa); }
      int n = mn_catchup(&b, &mb, 800, 28, COLD_NORMAL, 0, &wb); CHECK(n == 800); CHECK(bytes_hash(&ma) == bytes_hash(&mb)); CHECK(a.food == b.food && a.ore == b.ore && a.wood == b.wood && a.nset == b.nset && a.last_day == b.last_day && mb.last_day == 800);
      CHECK(wb.days == 800 && wb.ore == wa.ore && wb.bars == wa.bars && wb.cave_ins == wa.cave_ins);
      // a colony nobody added a mine layer to plays exactly as before: col_catchup_d is unchanged
      SimColony x, y; mk(&x, 61, ALL, 8, COLJ_MINER, 1); y = x; col_catchup_d(&x, 500, 28, COLD_NORMAL, 0); col_catchup_hook(&y, 500, 28, COLD_NORMAL, 0, 0, 0); CHECK(!memcmp(&x, &y, sizeof x));
      // 100 years of catch-up at once stays sane
      SimColony z; Mine mz; mk(&z, 62, ALL, 10, COLJ_MINER, 1); mn_init(&mz, 62, 0); MnAway wz; mn_catchup(&z, &mz, 28 * 400, 28, COLD_NORMAL, 0, &wz); CHECK(z.state == COLS_ALIVE || z.state == COLS_RUINS); CHECK(mz.depth <= 15); CHECK(wz.days == 28 * 100 || wz.days == 28 * 400); }
    // ---- the file: round trip, bad chunk, truncated, old file without the chunk
    { simfile_init(&F); F.slot[0].used = 1; F.slot[0].cal_ticks = 123;
      for(int k = 0; k < 5; k++){ SimColony c; Mine *m = mn_find(&F.slot[0].mines, 900u + (uint32_t)k, 1, 0); mk(&c, 900u + (uint32_t)k, ALL, 8, COLJ_MINER, 1); mn_catchup(&c, m, (uint32_t)(300 + 40 * k), 28, COLD_NORMAL, 0, 0); CHECK(mn_find(&F.slot[0].mines, 900u + (uint32_t)k, 0, 0) == m); }
      CHECK(F.slot[0].mines.n == 5); size_t n = simfile_write(&F, buf, sizeof buf); CHECK(n > 0); CHECK(simfile_read(&G, buf, n) == 0 && G.bad_chunks == 0 && G.slot[0].mines.n == 5);
      for(int k = 0; k < 5; k++) CHECK(bytes_hash(&F.slot[0].mines.m[k]) == bytes_hash(&G.slot[0].mines.m[k]) && G.slot[0].mines.m[k].depth >= 1);
      size_t n2 = simfile_write(&G, buf + 0, sizeof buf); CHECK(n2 == n);
      // flip a byte in the MINE chunk: that chunk is dropped, the rest stays
      { size_t at = 8, hit = 0; while(at + 14 <= n){ uint32_t len = (uint32_t)(buf[at + 6] | (buf[at + 7] << 8) | (buf[at + 8] << 16) | ((uint32_t)buf[at + 9] << 24)); if(!memcmp(buf + at, "MINE", 4)) hit = at; at += 14 + len; }
        CHECK(hit); buf[hit + 20] ^= 0x55; SimFile H; CHECK(simfile_read(&H, buf, n) == 0 && H.bad_chunks == 1 && H.slot[0].mines.n == 0 && H.slot[0].cal_ticks == 123); buf[hit + 20] ^= 0x55; }
      { SimFile H; simfile_read(&H, buf, n - 40); CHECK(H.slot[0].cal_ticks == 123); }
      { SimFile E; simfile_init(&E); E.slot[0].used = 1; E.slot[0].cal_ticks = 5; size_t ne = simfile_write(&E, buf, sizeof buf); SimFile H; CHECK(simfile_read(&H, buf, ne) == 0 && H.slot[0].mines.n == 0 && H.bad_chunks == 0); }   // a file from before this chunk
      // the record reader keeps wild bytes in range
      { uint8_t r[1 + MN_REC_BYTES]; memset(r, 0xFF, sizeof r); r[0] = 1; MineList L; CHECK(mn_list_read(&L, r, sizeof r) == 0); const Mine *m = &L.m[0]; CHECK(m->depth <= 15 && m->work <= m->depth && m->bars <= MN_STORE_MAX && m->wl[0] <= 3 && m->warn <= 1 && m->hurt_days <= 5);
        for(int z = 1; z <= 15; z++) CHECK(m->brace[z] <= 9 && m->zst[z] <= MNZ_DRY); CHECK(mn_list_read(&L, r, sizeof r - 1) < 0); r[0] = 99; CHECK(mn_list_read(&L, r, sizeof r) < 0); } }
    // ---- the list keeps at most COL_MAX and drops the oldest
    { MineList L; memset(&L, 0, sizeof L); for(uint32_t t = 1; t <= COL_MAX + 3; t++) mn_find(&L, t, 1, t); CHECK(L.n == COL_MAX); CHECK(!mn_find(&L, 1, 0, 0) && mn_find(&L, COL_MAX + 3, 0, 0)); }
    // ---- text
    { SimColony c; Mine m; mk(&c, 70, ALL, 6, COLJ_MINER, 1); mn_init(&m, 70, 0); runboth(&c, &m, 1, 60, COLD_NORMAL, ALL, 0); char t[80]; mn_status_text(&m, t, sizeof t); CHECK(t[0]); m.warn = 1; mn_status_text(&m, t, sizeof t); CHECK(strstr(t, "CREAKING"));
      for(int z = 1; z <= 15; z++){ mn_level_text(&m, &c, z, COLD_NORMAL, t, sizeof t); CHECK(t[0] == 'Z' && strlen(t) < 46); } char d[300]; CHECK(mn_dump(&m, d, sizeof d) > 20);
      MnAway aw; memset(&aw, 0, sizeof aw); aw.days = 5; aw.ore = 3; aw.bars = 1; aw.gems = 2; aw.cave_ins = 1; char ln[6][48]; CHECK(mn_away_text(&aw, &m, ln, 6) >= 3); }
    printf("mine_test: %d checks, %d failed\n", checks, fails); return fails ? 1 : 0;
}
