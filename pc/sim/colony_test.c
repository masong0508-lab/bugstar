// colony_test.c: Phase 4 (unlock ladder, tent, settlers, colony save). Plain C, no game.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static uint8_t buf[SIMFILE_MAX]; static SimFile F, G;
int main(void){
    ColFlags f; memset(&f, 0, sizeof f);
    CHECK(col_mask(&f) == 1); CHECK(col_cap(1) == 0); CHECK(col_next_rung(1) == COLR_TENT); CHECK(!strcmp(col_rung_how(COLR_TENT), "FREE A CAGED BUG"));
    // each milestone opens its own rung
    f.bugs_freed = 1; CHECK(col_mask(&f) == (1 | 1 << COLR_TENT)); f.best_depth = 3; CHECK(col_mask(&f) & 1 << COLR_FIELDS); f.best_depth = 0; f.zone_cleared = 1; CHECK(col_mask(&f) & 1 << COLR_FIELDS);
    f.bosses = 1; f.outposts = 1; f.ore_tool = 1; f.hives = 1; f.ant_king = 1; f.relics = 1; CHECK(col_mask(&f) == 0x1FF); CHECK(col_next_rung(0x1FF) == 0); CHECK(col_cap(0x1FF) == 20);
    // without the tent nothing counts
    CHECK(col_cap(1 | 1 << COLR_FIELDS) == 0); CHECK(col_buildings(1 | 1 << COLR_FIELDS) == 0);
    // a colony: locked, then the tent opens with 2 settlers and one card; more rungs, more settlers; the card is told once
    SimColony c; col_init(&c, 0xABCD, 10); CHECK(col_update(&c, 1) == 0 && c.nset == 0);
    CHECK(col_update(&c, 1 | 1 << COLR_FIELDS) == 0);                                   // fields wait for the tent
    uint16_t nw = col_update(&c, 1 | 1 << COLR_TENT | 1 << COLR_FIELDS); CHECK(nw == (1 << COLR_TENT | 1 << COLR_FIELDS)); CHECK(c.nset == 4);
    CHECK(col_update(&c, 1 | 1 << COLR_TENT) == 0); CHECK(c.mask & 1 << COLR_FIELDS);   // never closes, never tells twice
    char nm[16], nm2[16]; int j, j2; col_settler(&c, 0, nm, &j); col_settler(&c, 0, nm2, &j2); CHECK(!strcmp(nm, nm2) && j == j2 && j == COLJ_BUILDER); CHECK(strlen(nm) >= 2);
    // days of work: food rises, builders finish buildings, all deterministic
    SimColony d = c; for(uint32_t day = 11; day < 60; day++){ col_day(&c, day); col_day(&d, day); }
    CHECK(!memcmp(&c, &d, sizeof c)); CHECK(c.food > 0); CHECK(c.built >= 1 && c.built <= col_buildings(c.mask));
    // catch-up runs at most 7 days, and none twice
    SimColony e = c; CHECK(col_catchup(&e, 59) == 0); CHECK(col_catchup(&e, 100) == 41); CHECK(e.last_day == 100); CHECK(col_catchup(&e, 101) == 1);
    // the long catch-up equals stepping one day at a time (year ends and births included), for many short spans and split points
    for(int s = 0; s < 40; s++){ SimColony x, y; col_init(&x, 1000u + (uint32_t)s * 7919u, 3); col_update(&x, (uint16_t)(0x3 | (s & 1 ? 1 << COLR_FIELDS : 0) | (s & 2 ? 1 << COLR_SHOPS : 0) | (s & 4 ? 1 << COLR_MINE | 1 << COLR_MARKET : 0)));
        y = x; uint32_t to = 3 + 5 + (uint32_t)s * 9; ColAway aw; col_catchup_ex(&x, to, 28, &aw); for(uint32_t d = 4; d <= to; d++) col_day(&y, d);
        CHECK(!memcmp(&x, &y, sizeof x)); SimColony z; col_init(&z, 1000u + (uint32_t)s * 7919u, 3); z.mask = y.mask; z.told = y.told; z.nset = y.nset;
        col_catchup_ex(&z, 3 + to / 2, 28, 0); col_catchup_ex(&z, to, 28, 0); CHECK(!memcmp(&x, &z, sizeof x)); }
    // ten decades: 100 years of 28 days (and of a planet with 10-day seasons) run fast, are deterministic, saturate, and come back from the file
    { SimColony a1, a2; col_init(&a1, 0xBEEF, 0); col_update(&a1, 0x1FF); a2 = a1; ColAway aw, aw2;
      clock_t t0 = clock(); int nd = col_catchup_ex(&a1, 1000000u, 28, &aw); double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
      col_catchup_ex(&a2, 1000000u, 28, &aw2); CHECK(nd == 2800 && aw.years == 100 && !memcmp(&a1, &a2, sizeof a1) && !memcmp(&aw, &aw2, sizeof aw));
      CHECK(a1.built == col_buildings(a1.mask) && a1.born > 100 && a1.food <= COL_STORE_MAX && a1.ore <= COL_STORE_MAX);
      printf("  100 years (2800 days, 20 settlers): %.2f ms, born %u, food %u ore %u\n", ms, a1.born, a1.food, a1.ore); CHECK(ms < 200);
      SimColony a3; col_init(&a3, 0xBEEF, 0); col_update(&a3, 0x1FF); ColAway aw3; CHECK(col_catchup_ex(&a3, 5000000u, col_year_days(10), &aw3) == 4000 && aw3.years == 100);
      SimColony s1 = a1; s1.food = COL_STORE_MAX - 3; s1.last_day = 0; col_catchup_ex(&s1, 500, 28, 0); CHECK(s1.food <= COL_STORE_MAX);
      char lines[8][48]; int nl = col_away_text(&aw, &a1, lines, 8); CHECK(nl >= 4); for(int i = 0; i < nl; i++){ CHECK(strlen(lines[i]) <= 40); }
      printf("  %s / %s / %s\n", lines[0], lines[1], lines[2]);
      ColList L1; memset(&L1, 0, sizeof L1); L1.n = 1; L1.c[0] = a1; uint8_t rb[1 + COL_REC_BYTES]; CHECK(col_list_write(&L1, rb, sizeof rb) == sizeof rb);
      ColList L2; CHECK(col_list_read(&L2, rb, sizeof rb) == 0 && !memcmp(&L2.c[0], &a1, sizeof a1)); }
    // list: find or make, save and load through the .sim file
    simfile_init(&F); ColList *l = &F.slot[2].cols; F.slot[2].used = 1;
    SimColony *a = col_find(l, 1, 1, 5), *b = col_find(l, 2, 1, 6); CHECK(l->n == 2 && a != b && col_find(l, 1, 0, 0) == a && !col_find(l, 3, 0, 0));
    col_update(a, 0x3); for(int i = 0; i < 20; i++) col_day(a, 6 + i);
    size_t n = simfile_write(&F, buf, sizeof buf); CHECK(n > 0); CHECK(simfile_read(&G, buf, n) == 0); CHECK(G.bad_chunks == 0);
    CHECK(G.slot[2].cols.n == 2); CHECK(!memcmp(&G.slot[2].cols.c[0], a, sizeof *a)); CHECK(G.slot[2].cols.c[1].town == 2);
    // full list: the oldest visit goes
    ColList L; memset(&L, 0, sizeof L); for(uint32_t t = 0; t < COL_MAX; t++) col_find(&L, 100 + t, 1, t + 1); L.c[5].last_day = 0;
    col_find(&L, 999, 1, 50); CHECK(L.n == COL_MAX && !col_find(&L, 105, 0, 0) && col_find(&L, 999, 0, 0));
    // damage: a bad record count is refused; settlers past the cap are cut back
    { uint8_t old[1 + 34] = { 1 }; old[1] = 7; old[5] = 3; old[9] = 2; ColList O; CHECK(col_list_read(&O, old, sizeof old) == 0 && O.c[0].town == 7 && O.c[0].nset == 2 && O.c[0].born == 0);   // a Phase 4 file still loads
      uint8_t r[1 + COL_REC_BYTES]; ColList M; M.n = 1; col_init(&M.c[0], 7, 0); M.c[0].mask = 0x3; M.c[0].nset = 2; col_list_write(&M, r, sizeof r); r[1 + 8] = 200;
      ColList N; CHECK(col_list_read(&N, r, sizeof r) == 0 && N.c[0].nset == 2); r[0] = 2; CHECK(col_list_read(&N, r, sizeof r) == -1); }
    // Phase 5: collecting. Locked: nothing. Food fills the bar 2 for 1; goods and ore are sold; houses heal.
    { SimColony k; col_init(&k, 9, 0); ColGift g; CHECK(col_collect(&k, 50, &g) == 0);
      col_update(&k, 0x3); k.food = 41; k.goods = 3; k.ore = 2; CHECK(col_collect(&k, 100, &g) == 1); CHECK(g.food_pts == 20 && k.food == 1 && g.gold == 21 && !g.heal && k.goods == 0 && k.ore == 0);
      k.food = 100; CHECK(col_collect(&k, 10, &g) == 1 && g.food_pts == 10 && k.food == 80); CHECK(col_collect(&k, 0, &g) == 0);
      col_update(&k, 0x9); CHECK(col_collect(&k, 0, &g) == 1 && g.heal == 1); }
    char t[200]; CHECK(col_dump(&c, t, sizeof t) > 20);
    printf("colony_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
