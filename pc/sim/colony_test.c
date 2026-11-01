// colony_test.c: the colony (Phases 4 and 5): ladder (nonlinear), settlers, days, care and failure, raids and rewards, hands-on play, catch-up, the file.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static uint8_t buf[SIMFILE_MAX]; static SimFile F, G;
static const ColFlags NOF;
// runs a colony for `days` days on a difficulty; feats[d] (if given) are action-game masks that open on day d. Goals are checked every day.
static void run(SimColony *c, uint32_t from, uint32_t days, int diff, const uint16_t *feat_at, const uint16_t *feat_mask, int nfeat){
    for(uint32_t d = from; d < from + days; d++){
        uint16_t m = 1; for(int k = 0; k < nfeat; k++) if(d >= feat_at[k]) m |= feat_mask[k];
        col_update(c, (uint16_t)(m | col_goal_mask(c, &NOF, d, 28)));
        col_day_ex(c, d, 28, diff, 0);
    }
}
int main(void){
    ColFlags f; memset(&f, 0, sizeof f);
    // ---- the ladder: action routes
    CHECK(col_mask(&f) == 1); CHECK(col_cap(1) == 0); CHECK(col_next_rung(1) == COLR_TENT); CHECK(!strcmp(col_rung_how(COLR_TENT), "FREE A CAGED BUG"));
    f.bugs_freed = 1; CHECK(col_mask(&f) == (1 | 1 << COLR_TENT)); f.best_depth = 3; CHECK(col_mask(&f) & 1 << COLR_FIELDS); f.best_depth = 0; f.zone_cleared = 1; CHECK(col_mask(&f) & 1 << COLR_FIELDS);
    f.bosses = 1; f.outposts = 1; f.ore_tool = 1; f.hives = 1; f.ant_king = 1; f.relics = 1; CHECK(col_mask(&f) == 0x1FF); CHECK(col_cap(0x1FF) == 20);
    CHECK(col_cap(1 | 1 << COLR_FIELDS) == 0);
    // ---- the ladder: colony-goal routes (nonlinear)
    { SimColony c; col_init(&c, 5, 0); ColFlags g = NOF; g.friends = 1; CHECK(col_goal_mask(&c, &g, 0, 28) & 1 << COLR_TENT); g.friends = 0; g.level = 10; CHECK(col_goal_mask(&c, &g, 0, 28) & 1 << COLR_TENT);
      CHECK(col_goal_mask(&c, &NOF, 0, 28) == 1); col_update(&c, 0x3); c.food = 60; CHECK(col_goal_mask(&c, &NOF, 0, 28) & 1 << COLR_FIELDS); CHECK(!(col_goal_mask(&c, &NOF, 0, 28) & 1 << COLR_MUSEUM));
      CHECK(col_goal_mask(&c, &NOF, 28 * 10, 28) & 1 << COLR_MUSEUM); }
    // ---- a colony: locked, then the tent with 2 settlers and one card; a rung never closes, never told twice
    SimColony c; col_init(&c, 0xABCD, 10); CHECK(col_update(&c, 1) == 0 && c.nset == 0);
    CHECK(col_update(&c, 1 | 1 << COLR_FIELDS) == 0);
    uint16_t nw = col_update(&c, 1 | 1 << COLR_TENT); CHECK(nw == (1 << COLR_TENT | 1 << COLR_FIELDS)); CHECK(c.nset == 4);
    CHECK(col_update(&c, 1 | 1 << COLR_TENT) == 0); CHECK(c.mask & 1 << COLR_FIELDS);
    char nm[16], nm2[16]; int j, j2; col_settler(&c, 0, nm, &j); col_settler(&c, 0, nm2, &j2); CHECK(!strcmp(nm, nm2) && j == j2 && j == COLJ_BUILDER);
    // ---- unattended: three different unlock orders (and no feats at all) all reach the full colony on NORMAL
    { static const uint16_t at1[] = { 0, 30, 60, 90, 120, 150, 180, 210 }, m1[] = { 2, 4, 8, 16, 32, 64, 128, 256 };     // in ladder order
      static const uint16_t at2[] = { 0, 30, 60, 90, 120, 150, 180, 210 }, m2[] = { 2, 256, 128, 64, 32, 16, 8, 4 };   // backwards
      static const uint16_t at3[] = { 0, 400 }, m3[] = { 2, 64 };                                                       // only the tent, a hive much later
      const uint16_t *ats[4] = { at1, at2, at3, at3 }, *ms[4] = { m1, m2, m3, m3 }; int ns[4] = { 8, 8, 2, 1 };
      for(int o = 0; o < 4; o++){ SimColony k; col_init(&k, 0x1000u + (uint32_t)o * 31u, 0); k.danger = 2; run(&k, 0, 28 * 40, COLD_NORMAL, ats[o], ms[o], ns[o]);
          printf("  order %d: mask %03x settlers %u buildings %d state %u raids %u/%u\n", o + 1, k.mask, k.nset, col_built_total(&k), k.state, k.raids_won, k.raids_lost);
          CHECK(k.state == COLS_ALIVE); CHECK(k.mask == 0x1FF); CHECK(k.nset >= 10); CHECK(col_built_total(&k) >= 15); } }
    // ---- hands-on changes outcomes, deterministically
    { SimColony a, b, a2; col_init(&a, 77, 0); col_update(&a, 0x7); b = a;
      for(uint32_t d = 1; d <= 60; d++){ col_help_build(&a); col_day(&a, d); col_day(&b, d); }
      a2 = b; (void)a2; SimColony a3; col_init(&a3, 77, 0); col_update(&a3, 0x7); for(uint32_t d = 1; d <= 60; d++){ col_help_build(&a3); col_day(&a3, d); }
      CHECK(col_built_total(&a) > col_built_total(&b)); CHECK(!memcmp(&a, &a3, sizeof a));
      SimColony p; col_init(&p, 78, 0); col_update(&p, 0x7); CHECK(col_set_plan(&p, COLB_WELL, 13)); CHECK(!col_set_plan(&p, COLB_MUSEUM, 1));
      for(uint32_t d = 1; d <= 30 && !p.bld[COLB_WELL]; d++){ col_day(&p, d); }
      CHECK(p.bld[COLB_WELL] == 1 && p.grid[13] == COLB_WELL + 1); CHECK(!col_set_plan(&p, COLB_HUT, 13));
      SimColony q; col_init(&q, 79, 0); col_update(&q, 0x3); CHECK(col_set_job(&q, 1, COLJ_BUILDER)); col_settler(&q, 1, nm, &j); CHECK(j == COLJ_BUILDER); CHECK(col_set_job(&q, 1, -1)); col_settler(&q, 1, nm, &j); CHECK(j == COLJ_FORAGER);
      CHECK(!col_set_job(&q, 5, COLJ_COOK)); }
    // ---- neglect: no food (everyone mining, nothing in the store) kills on NORMAL and HARSH, never on CALM
    for(int diff = 0; diff < 3; diff++){ SimColony k; col_init(&k, 300, 0); col_update(&k, 0x1 | 1 << COLR_TENT | 1 << COLR_FIELDS | 1 << COLR_MINE); k.food = 0;
        for(int i = 0; i < k.nset; i++) col_set_job(&k, i, COLJ_MINER);       // the player sends everyone down the mine and never feeds them
        int n0 = k.nset;
        ColAway aw; memset(&aw, 0, sizeof aw); int warned = 0; for(uint32_t d = 1; d <= 120; d++){ col_day_ex(&k, d, 28, diff, &aw); }
        for(int e = 0; e < aw.nev; e++) if(aw.ev[e].kind == COLE_DIED || aw.ev[e].kind == COLE_RUINS || aw.ev[e].kind == COLE_SICK) warned = 1;
        if(diff == COLD_CALM) CHECK(k.state == COLS_ALIVE && k.nset == n0); else CHECK(k.state == COLS_RUINS && k.nset == 0 && (int)k.died == n0);
        CHECK(warned); }
    // a food warning comes days before the food runs out
    { SimColony k; col_init(&k, 301, 0); col_update(&k, 0x3); for(int i = 0; i < k.nset; i++) col_set_job(&k, i, COLJ_BUILDER); k.food = 6;
      ColAway aw; memset(&aw, 0, sizeof aw); for(uint32_t d = 1; d <= 6; d++) col_day_ex(&k, d, 28, COLD_NORMAL, &aw);
      int w = 0; for(int e = 0; e < aw.nev; e++) if(aw.ev[e].kind == COLE_FOODWARN){ char t[48]; col_event_text(&aw.ev[e], t, sizeof t); CHECK(strstr(t, "FOOD RUNS OUT IN")); w = 1; } CHECK(w); }
    // ---- raids: warnings, a big unrepelled raid ruins the colony; the planet goes only on the extreme path (and never a story world, never on CALM)
    { SimColony k; col_init(&k, 400, 0); col_update(&k, 0x3); k.raid_size = 80; k.raid_eta = 4; ColAway aw; memset(&aw, 0, sizeof aw);
      for(uint32_t d = 1; d <= 4; d++) col_day_ex(&k, d, 28, COLD_NORMAL, &aw);
      int warn = 0; for(int e = 0; e < aw.nev; e++) if(aw.ev[e].kind == COLE_RAIDWARN) warn++; CHECK(warn == 2);   // 3 days and 1 day before
      CHECK(k.state == COLS_RUINS && k.raids_lost == 1); CHECK(col_resettle(&k, 5) && k.state == COLS_ALIVE && k.nset == 2); }
    { int states[3][2]; for(int diff = 0; diff < 3; diff++) for(int sz = 0; sz < 2; sz++){ SimColony k; col_init(&k, 401, 0); col_update(&k, 0x1FF); k.world_ok = 1;
          k.raid_size = (uint16_t)(sz ? 300 : 70); k.raid_eta = 1; col_day_ex(&k, 1, 28, diff, 0); states[diff][sz] = k.state; }
      CHECK(states[COLD_CALM][0] == COLS_ALIVE && states[COLD_CALM][1] == COLS_ALIVE);
      CHECK(states[COLD_NORMAL][0] == COLS_RUINS && states[COLD_NORMAL][1] == COLS_DEAD_WORLD);
      CHECK(states[COLD_HARSH][0] == COLS_DEAD_WORLD && states[COLD_HARSH][1] == COLS_DEAD_WORLD);
      SimColony k; col_init(&k, 402, 0); col_update(&k, 0x1FF); k.world_ok = 0; k.raid_size = 5000; k.raid_eta = 1; col_day_ex(&k, 1, 28, COLD_HARSH, 0); CHECK(k.state == COLS_RUINS);  // a story world is never destroyed
      CHECK(!col_resettle(&(SimColony){ .state = COLS_DEAD_WORLD }, 1)); }
    // ---- rewards: grow steeply with size and danger, bonuses, saturate, paid once, deterministic
    { uint64_t prev = 0; for(uint32_t s = 1; s < 400; s++){ uint64_t r = col_reward(s, 3, 1, 1, 0); CHECK(r > prev); if(s > 1) CHECK(r >= prev + prev / s); prev = r; }
      CHECK(col_reward(12, 3, 1, 1, 0) == col_reward(12, 3, 1, 1, 0)); CHECK(col_reward(12, 5, 0, 0, 0) > col_reward(12, 3, 0, 0, 0));
      CHECK(col_reward(12, 3, 1, 1, 1) == 2 * col_reward(12, 3, 1, 1, 0)); CHECK(col_reward(12, 3, 1, 0, 0) > col_reward(12, 3, 0, 0, 0));
      CHECK(col_reward(0xFFFFFFFFu, 9, 1, 1, 1) == COL_GOLD_MAX);
      char t[64]; ColEvent e = { COLE_RAIDWON, 12, (uint32_t)col_reward(12, 3, 1, 1, 0), 0 }; col_event_text(&e, t, sizeof t); printf("  %s\n", t);
      SimColony k; col_init(&k, 500, 0); col_update(&k, 0x1FF); k.nset = 20; k.bld[COLB_WALL] = 6; k.raid_size = 12; k.raid_eta = 1; col_day_ex(&k, 1, 28, COLD_NORMAL, 0);
      CHECK(k.raids_won == 1 && k.reward_due > 0); ColGift g; uint64_t due = k.reward_due; CHECK(col_collect(&k, 0, &g) && (uint64_t)g.gold >= due && g.reward == due); CHECK(k.reward_due == 0);
      col_collect(&k, 0, &g); CHECK(g.reward == 0);                                        // paid once
      k.reward_due = COL_GOLD_MAX - 5; k.raid_size = 200; k.raid_eta = 1; k.bld[COLB_WALL] = 9; k.bld[COLB_TOWER] = 9; k.god_used = 1; col_day_ex(&k, 2, 28, COLD_CALM, 0); CHECK(k.reward_due == COL_GOLD_MAX); }
    // ---- days and catch-up: the long catch-up equals stepping day by day, on each difficulty
    for(int s = 0; s < 30; s++){ SimColony x, y; col_init(&x, 1000u + (uint32_t)s * 7919u, 3); col_update(&x, (uint16_t)(0x3 | (s & 1 ? 1 << COLR_FIELDS : 0) | (s & 2 ? 1 << COLR_SHOPS : 0) | (s & 4 ? 1 << COLR_MINE | 1 << COLR_MARKET : 0)));
        int diff = s % 3; y = x; uint32_t to = 3 + 5 + (uint32_t)s * 23; ColAway aw; col_catchup_d(&x, to, 28, diff, &aw); for(uint32_t d = 4; d <= to; d++) col_catchup_d(&y, d, 28, diff, 0);   // one long catch-up = one day at a time
        CHECK(!memcmp(&x, &y, sizeof x)); }
    { SimColony a1, a2; col_init(&a1, 0xBEEF, 0); col_update(&a1, 0x1FF); a1.world_ok = 1; a2 = a1; ColAway aw, aw2;
      clock_t t0 = clock(); int nd = col_catchup_d(&a1, 1000000u, 28, COLD_NORMAL, &aw); double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
      col_catchup_d(&a2, 1000000u, 28, COLD_NORMAL, &aw2); CHECK(nd == 2800 && aw.years == 100 && !memcmp(&a1, &a2, sizeof a1) && !memcmp(&aw, &aw2, sizeof aw));
      printf("  100 years (2800 days): %.2f ms, state %u settlers %u born %u died %u raids %u/%u reward %llu\n", ms, a1.state, a1.nset, aw.born, aw.died, aw.raids_won, aw.raids_lost, (unsigned long long)aw.reward);
      CHECK(ms < 500); CHECK(a1.food <= COL_STORE_MAX);
      SimColony a3; col_init(&a3, 0xBEEF, 0); col_update(&a3, 0x1FF); ColAway aw3; CHECK(col_catchup_d(&a3, 5000000u, col_year_days(10), COLD_CALM, &aw3) == 4000 && aw3.years == 100 && a3.state == COLS_ALIVE);
      char lines[8][48]; int nl = col_away_text(&aw, &a1, lines, 8); CHECK(nl >= 4); for(int i = 0; i < nl; i++){ CHECK(strlen(lines[i]) <= 40); printf("    %s\n", lines[i]); }
      // the file: all of it comes back
      simfile_init(&F); F.slot[1].used = 1; F.slot[1].cols.n = 2; F.slot[1].cols.c[0] = a1; F.slot[1].cols.c[1] = a3;
      size_t n = simfile_write(&F, buf, sizeof buf); CHECK(n > 0); CHECK(simfile_read(&G, buf, n) == 0 && G.bad_chunks == 0);
      CHECK(G.slot[1].cols.n == 2 && !memcmp(&G.slot[1].cols.c[0], &a1, sizeof a1) && !memcmp(&G.slot[1].cols.c[1], &a3, sizeof a3)); }
    // ---- old files (Phase 4: 34-byte records, Phase 5: 42) still load
    { uint8_t old[1 + 34] = { 1 }; old[1] = 7; old[5] = 3; old[9] = 2; ColList O; CHECK(col_list_read(&O, old, sizeof old) == 0 && O.c[0].town == 7 && O.c[0].nset == 2 && O.c[0].state == COLS_ALIVE);
      uint8_t o2[1 + 42] = { 1 }; o2[1] = 8; o2[5] = 3; o2[9] = 2; o2[35] = 5; CHECK(col_list_read(&O, o2, sizeof o2) == 0 && O.c[0].born == 5);
      uint8_t bad[1 + COL_REC_BYTES] = { 2 }; CHECK(col_list_read(&O, bad, sizeof bad) == -1); }
    // ---- list: find or make; the oldest visit goes when full
    { ColList L; memset(&L, 0, sizeof L); for(uint32_t t = 0; t < COL_MAX; t++) col_find(&L, 100 + t, 1, t + 1); L.c[5].last_day = 0;
      col_find(&L, 999, 1, 50); CHECK(L.n == COL_MAX && !col_find(&L, 105, 0, 0) && col_find(&L, 999, 0, 0)); }
    // ---- evacuation
    { SimColony a, b; col_init(&a, 1, 0); col_init(&b, 2, 0); col_update(&a, 0x7); col_update(&b, 0x1FF); int bn = b.nset; CHECK(col_evacuate(&a, &b) && a.state == COLS_RUINS && b.nset == bn + 4); }
    // ---- collecting: locked gives nothing; food fills the bar 2 for 1; goods and ore sold; houses give a nap
    { SimColony k; col_init(&k, 9, 0); ColGift g; CHECK(col_collect(&k, 50, &g) == 0);
      col_update(&k, 0x3); k.food = 41; k.goods = 3; k.ore = 2; CHECK(col_collect(&k, 100, &g) == 1); CHECK(g.food_pts == 20 && k.food == 1 && g.gold == 21 && !g.heal);
      col_update(&k, 0x9); CHECK(col_collect(&k, 0, &g) == 1 && g.heal == 1); }
    // ---- the map, the invasion hook, burnt worlds stay
    { SimColony a; col_init(&a, 77, 0); col_update(&a, 0x1FF); a.wood = 100; int p0 = a.progress; a.progress = 0; CHECK(col_carry_wood(&a, 3) && (a.grid[3] || (a.plan_cell == 3 && a.progress > 0))); (void)p0;
      CHECK(!col_carry_wood(&a, 99)); a.grid[5] = 1; CHECK(!col_carry_wood(&a, 5));
      SimColony b; col_init(&b, 78, 0); CHECK(col_invade(&b, 5, COLD_NORMAL, 1, 0, 0) == 0);
      col_update(&b, 0x1FF); b.world_ok = 1; b.bld[COLB_TOWER] = 9; uint64_t rw = 0; int r = col_invade(&b, 2, COLD_NORMAL, 5, 0, &rw); CHECK(r == 1 && rw > 0 && b.reward_due >= rw && b.raids_won == 1);
      SimColony d; col_init(&d, 79, 0); col_update(&d, 0x1FF); d.world_ok = 1; CHECK(col_invade(&d, 5000, COLD_HARSH, 5, 0, &rw) == -1 && col_burnt(&d) && !col_resettle(&d, 9) && !col_invade(&d, 5, COLD_NORMAL, 6, 0, 0));
      ColList L; memset(&L, 0, sizeof L); for(uint32_t q = 0; q < COL_MAX; q++) col_find(&L, 200 + q, 1, q + 5); L.c[7].state = COLS_DEAD_WORLD; L.c[7].last_day = 0; col_find(&L, 999, 1, 90); CHECK(col_find(&L, 207, 0, 0) && col_find(&L, 999, 0, 0));
      col_update(&d, 0xFF); col_catchup_d(&d, 500, 28, COLD_NORMAL, 0); CHECK(col_burnt(&d) && d.nset == 0); }
    char t[300]; CHECK(col_dump(&c, t, sizeof t) > 20);
    printf("colony_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
