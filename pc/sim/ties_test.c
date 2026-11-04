// ties_test.c: patch 15 (gifts, requests, colony ties). Plain C.
#include <stdio.h>
#include <string.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
int main(void){
    Sim A, B; sim_init(&A, 77, 6, 0, 0);
    // gifts fit the job; a gift is logged once per friendship step
    SimGift g; for(int k = 0; k < 6; k++){ CHECK(sim_gift(&A, k, 1, &g) && g.kind != SIMG_NONE); }
    CHECK(!sim_gift(&A, 9, 1, &g) && !sim_gift(&A, 0, 5, &g));
    int gifts = 0; for(int d = 0; d < 30; d++){ sim_talk_to(&A, 2); sim_run(&A, SIM_DAY_TICKS); }
    for(uint32_t i = 0; i < sim_log_count(&A); i++) if(sim_log_at(&A, i)->kind == SIME_GIFT) gifts++;
    CHECK(gifts == 4);
    // requests: make bug 1 sick; you must feed it; once a day; friendship rises
    sim_init(&A, 77, 6, 0, 0); A.folk[1].sick = 4; int w;
    CHECK(sim_ask(&A, 1, &w) == SIMA_MEAL); int f0 = A.friend[1];
    CHECK(sim_help(&A, 1, SIMA_MEAL) == 1); CHECK(A.friend[1] > f0); CHECK(A.folk[1].sick < 4);
    CHECK(sim_help(&A, 1, SIMA_MEAL) == 0);                                   // not twice in a day
    CHECK(sim_help(&A, 2, SIMA_MEAL) == 0);                                   // nothing to do
    // listen needs a little friendship first
    sim_init(&A, 77, 6, 0, 0); A.folk[3].stress = 220; CHECK(sim_ask(&A, 3, &w) == SIMA_LISTEN);
    CHECK(sim_help(&A, 3, SIMA_LISTEN) == -1); sim_talk_to(&A, 3); CHECK(A.friend[3] >= 8);
    A.friend[3] = 12; CHECK(sim_help(&A, 3, SIMA_LISTEN) == 1); CHECK(A.folk[3].stress < 150);
    // peace: a feud is mended
    sim_init(&A, 77, 6, 0, 0); A.folk[0].bond[4] = A.folk[4].bond[0] = -60; A.friend[0] = 30; CHECK(sim_ask(&A, 0, &w) == SIMA_PEACE && w == 4);
    CHECK(sim_help(&A, 0, SIMA_PEACE) == 1); CHECK(A.folk[0].bond[4] > -39);
    // ties: friends lend their trade; none, nothing
    SimTies t; sim_init(&A, 77, 6, 0, 0); sim_ties(&A, &t); CHECK(t.friends == 0 && t.guard + t.food + t.wood + t.ore + t.goods == 0);
    for(int k = 0; k < 6; k++) A.friend[k] = 100; sim_ties(&A, &t); CHECK(t.friends == 6 && t.guard + t.food + t.wood + t.ore + t.goods > 0);
    char tx[80]; sim_ties_text(&t, tx, sizeof tx); CHECK(strlen(tx) > 5);
    // the colony's worry stresses the town; aid feeds it
    sim_init(&A, 5, 4, 0, 0); sim_init(&B, 5, 4, 0, 0); sim_set_outside(&B, 2, 2);
    sim_run(&A, SIM_DAY_TICKS * 3); sim_run(&B, SIM_DAY_TICKS * 3);
    int sa = 0, sb = 0; for(int k = 0; k < 4; k++){ sa += A.folk[k].stress; sb += B.folk[k].stress; } CHECK(sb > sa); CHECK(B.larder > A.larder);
    // determinism with ties
    sim_init(&A, 9, 5, 0, 0); sim_init(&B, 9, 5, 0, 0);
    for(int d = 0; d < 10; d++){ sim_talk_to(&A, d % 5); sim_talk_to(&B, d % 5); sim_set_outside(&A, d % 3, 1); sim_set_outside(&B, d % 3, 1); sim_run(&A, 3000); sim_run(&B, 3000); }
    CHECK(sim_hash(&A) == sim_hash(&B));
    // colony side
    SimColony c; col_init(&c, 1, 0); c.mask = 0x1ff; c.nset = 6; c.state = COLS_ALIVE; int d0 = col_defence(&c);
    col_set_ties(&c, 12, 4, 2, 1, 1, 80); CHECK(col_defence(&c) == d0 + 12);
    SimColony z = c; z.t_def = z.t_food = z.t_wood = z.t_ore = z.t_goods = z.t_mood = 0; z.food = c.food = 50; z.wood = c.wood = 50;
    col_day(&c, 5); col_day(&z, 5); CHECK(c.ore >= z.ore + 1 && c.goods >= z.goods + 1);
    c.raid_eta = 5; c.raid_size = 200; int wy, ad; col_outside(&c, &wy, &ad); CHECK(wy == 2);
    c.raid_eta = 0; c.food = 6 * 40; c.hungry_days = 0; col_outside(&c, &wy, &ad); CHECK(wy == 0 && ad == 2);
    // dungeon calls: trouble picks the call; clearing it pays once, then a rest
    { SimColony k; col_init(&k, 3, 0); k.mask = 0x1ff; k.nset = 6; k.state = COLS_ALIVE; k.food = 200; k.wood = 100; k.morale = 80; char tx2[56];
      CHECK(col_call(&k, 10) == COLC_NONE || col_call(&k, 10) == COLC_LOST);
      k.raid_eta = 8; k.raid_size = 90; CHECK(col_call(&k, 10) == COLC_RAID); uint16_t rs = k.raid_size;
      CHECK(col_call_done(&k, COLC_RAID, 5, 10, tx2, sizeof tx2) == 1 && k.raid_size < rs && k.help_days == 5 && strlen(tx2) > 3);
      CHECK(col_call(&k, 11) != COLC_RAID);                                               // resting: no call for 2 days
      k.raid_eta = 0; k.food = 3; k.call_cool = 0; CHECK(col_call(&k, 20) == COLC_FAMINE); uint32_t f1 = k.food; col_call_done(&k, COLC_FAMINE, 6, 20, 0, 0); CHECK(k.food == f1 + 44);
      k.food = 200; k.wood = 5; k.call_cool = 0; CHECK(col_call(&k, 30) == COLC_SUPPLY); uint32_t w1 = k.wood; col_call_done(&k, COLC_SUPPLY, 4, 30, 0, 0); CHECK(k.wood == w1 + 23);
      k.wood = 100; k.nset = 3; k.call_cool = 0; int lost = 0; for(uint32_t d = 0; d < 60; d += 3) if(col_call(&k, d) == COLC_LOST) lost++; CHECK(lost > 0 && lost < 20);
      uint8_t ns = k.nset; col_call_done(&k, COLC_LOST, 3, 90, 0, 0); CHECK(k.nset == ns + 1);
      k.state = COLS_RUINS; k.call_cool = 0; CHECK(col_call(&k, 50) == COLC_NONE && col_call_done(&k, COLC_RAID, 3, 50, 0, 0) == 0); }
    printf("ties_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
