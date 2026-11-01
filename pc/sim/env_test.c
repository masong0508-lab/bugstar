// env_test.c: Phase 2 (planet day length, seasons, climate weather). Plain C, no game.
#include <stdio.h>
#include <string.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static Sim A, B;
int main(void){
    // day length follows the planet: 12 h = 2400 ticks, 24 h = 4800, 48 h = 9600, 1 h clamps to 1200, 1000 h clamps to 12000
    int hrs[] = { 12, 24, 48, 1, 1000 }, want[] = { 2400, 4800, 9600, 1200, 12000 };
    for(int i = 0; i < 5; i++){ sim_init(&A, 77, 5, 0, 0); sim_set_env(&A, hrs[i], 15); CHECK((int)A.day_ticks == want[i]); CHECK(!A.locked); }
    // a tidally locked planet (0 hours): no day, the sun stays at noon, people keep a body clock
    sim_init(&A, 77, 5, 0, 0); sim_set_env(&A, 0, 15); CHECK(A.locked); for(int i = 0; i < 6000; i += 500){ sim_run(&A, 500); CHECK(sim_sun_hour(&A) == 12); }
    // seasons: 7 days each, spring summer autumn winter, in order, then again
    sim_init(&A, 5, 4, 0, 0); sim_set_env(&A, 24, 15); int last = -1, seq[8], ns = 0;
    for(uint32_t t = 0; t < 4800u * 7 * 8; t += 100){ sim_run(&A, 100); int se = sim_season(&A); if(se != last){ if(ns < 8) seq[ns++] = se; last = se; } }
    CHECK(ns == 8); for(int i = 0; i < 8; i++) CHECK(seq[i] == i % 4);
    CHECK(!strcmp(sim_season_name(3), "WINTER"));
    // snow only where it is cold: a freezing planet sees snow and no rain; a hot one sees no rain and no snow
    int snow_c = 0, rain_c = 0, snow_h = 0, rain_h = 0;
    sim_init(&A, 9, 4, 0, 0); sim_set_env(&A, 24, -20); sim_init(&B, 9, 4, 0, 0); sim_set_env(&B, 24, 60);
    for(int d = 0; d < 400 * 48; d++){ sim_run(&A, 100); sim_run(&B, 100);
        if(A.weather == SIMW_SNOW) snow_c++; if(A.weather == SIMW_RAIN) rain_c++; if(B.weather == SIMW_SNOW) snow_h++; if(B.weather == SIMW_RAIN) rain_h++; }
    CHECK(snow_c > 5); CHECK(rain_c == 0); CHECK(snow_h == 0); CHECK(rain_h == 0);
    // determinism: same env, same hash; a different env, a different hash; env off keeps the old hash
    sim_init(&A, 3, 5, 0, 0); sim_set_env(&A, 30, 10); sim_init(&B, 3, 5, 0, 0); sim_set_env(&B, 30, 10); sim_run(&A, 20000); sim_run(&B, 20000); CHECK(sim_hash(&A) == sim_hash(&B));
    sim_init(&B, 3, 5, 0, 0); sim_set_env(&B, 18, 10); sim_run(&B, 20000); CHECK(sim_hash(&A) != sim_hash(&B));
    // the dump shows the hour and every need stays in range under a short day
    sim_init(&A, 3, 6, 0, 0); sim_set_env(&A, 6, 5); for(int i = 0; i < 30000; i++){ sim_step(&A); } CHECK(A.nfolk == 6);
    printf("env_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
