// colony_test.c: Phase 4 (unlock ladder, tent, settlers, colony save). Plain C, no game.
#include <stdio.h>
#include <string.h>
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
    SimColony e = c; CHECK(col_catchup(&e, 59) == 0); CHECK(col_catchup(&e, 100) == COL_CATCHUP_DAYS); CHECK(e.last_day == 100); CHECK(col_catchup(&e, 101) == 1);
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
    { uint8_t r[1 + COL_REC_BYTES]; ColList M; M.n = 1; col_init(&M.c[0], 7, 0); M.c[0].mask = 0x3; M.c[0].nset = 2; col_list_write(&M, r, sizeof r); r[1 + 8] = 200;
      ColList N; CHECK(col_list_read(&N, r, sizeof r) == 0 && N.c[0].nset == 2); r[0] = 2; CHECK(col_list_read(&N, r, sizeof r) == -1); }
    char t[200]; CHECK(col_dump(&c, t, sizeof t) > 20);
    printf("colony_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
