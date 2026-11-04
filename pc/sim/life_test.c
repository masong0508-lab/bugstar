// life_test.c: patch 41, life v3 (food economy with seasons, jobs that matter, needs that steer free time, no task flapping). Plain C.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static Sim A, B;
static int changes(Sim *s, int bug, uint64_t ticks){ int n = 0, prev = s->folk[bug].task; for(uint64_t t = 0; t < ticks; t++){ sim_step(s); if(s->folk[bug].task != prev){ n++; prev = s->folk[bug].task; } } return n; }
static void all_job(Sim *s, int job){ for(int i = 0; i < s->nfolk; i++) s->folk[i].job = (uint8_t)job; }
int main(void){
    // 1. no flapping. A sick bug just under the old rest line used to flip between bed and plaza every few ticks; now it stays in bed until rested
    sim_init(&A, 11, 6, 0, 0); A.folk[0].shift = 0; A.tick = 14 * 200; A.day_seen = (uint32_t)sim_day(&A);
    A.folk[0].sick = 6; A.folk[0].need[SIMN_REST] = 159; A.folk[0].need[SIMN_HUNGER] = 220;
    CHECK(changes(&A, 0, 600) <= 3);
    // worn out (rest < 20): sleeps until properly rested (90), not until just over the line
    sim_init(&A, 12, 6, 0, 0); for(int i = 0; i < 6; i++) A.folk[i].shift = 0; A.tick = 9 * 200; A.day_seen = (uint32_t)sim_day(&A);
    A.folk[1].need[SIMN_REST] = 10; A.folk[1].need[SIMN_HUNGER] = 220;
    sim_run(&A, 50); CHECK(A.folk[1].task == SIMT_SLEEP);
    sim_run(&A, 250); CHECK(A.folk[1].task == SIMT_SLEEP && A.folk[1].need[SIMN_REST] < 90);
    sim_run(&A, 250); CHECK(A.folk[1].need[SIMN_REST] >= 80 && A.folk[1].task != SIMT_SLEEP);                  // up and about at about 90, then it drifts down a little again
    // hungry (below 50) with food in the larder: eats until full, one trip, no table/work/table/work
    sim_init(&A, 13, 6, 0, 0); for(int i = 0; i < 6; i++) A.folk[i].shift = 0; A.tick = 8 * 200 + 30; A.day_seen = (uint32_t)sim_day(&A); A.larder = 500;
    A.folk[2].need[SIMN_HUNGER] = 40; A.folk[2].need[SIMN_REST] = 200;
    { int n = changes(&A, 2, 300); CHECK(n <= 2); CHECK(A.folk[2].need[SIMN_HUNGER] >= 100); }                  // full at 130, then it drifts down
    // over many towns and 20 days: nobody changes task more than a normal day's schedule asks (about 9 times a day; 14 is a generous ceiling)
    { int worst = 0; for(int sd = 1; sd <= 12; sd++){ sim_init(&A, (uint64_t)sd * 313u, 8, 0, 0); sim_set_env(&A, 24, 10); int prev[SIM_MAXFOLK], n[SIM_MAXFOLK]; for(int i = 0; i < 8; i++){ prev[i] = A.folk[i].task; n[i] = 0; }
        for(uint64_t t = 0; t < 20ull * SIM_DAY_TICKS; t++){ sim_step(&A); for(int i = 0; i < 8; i++) if(A.folk[i].task != prev[i]){ n[i]++; prev[i] = A.folk[i].task; } }
        for(int i = 0; i < 8; i++) if(n[i] / 20 > worst) worst = n[i] / 20; }
      CHECK(worst <= 14); }
    // 2. free time follows the lower need (hour 20 is leisure time by the schedule)
    sim_init(&A, 14, 4, 0, 0); for(int i = 0; i < 4; i++){ A.folk[i].shift = 0; A.folk[i].trait[SIMTR_SOCIABLE] = 128; }
    A.tick = 20 * 200 + 5; A.day_seen = (uint32_t)sim_day(&A); A.folk[0].task = SIMT_WORK; A.folk[1].task = SIMT_WORK;
    A.folk[0].need[SIMN_SOCIAL] = 30; A.folk[0].need[SIMN_FUN] = 240; A.folk[1].need[SIMN_SOCIAL] = 240; A.folk[1].need[SIMN_FUN] = 30;
    for(int i = 0; i < 2; i++){ A.folk[i].need[SIMN_HUNGER] = 220; A.folk[i].need[SIMN_REST] = 220; }
    sim_step(&A); CHECK(A.folk[0].task == SIMT_SOCIAL && A.folk[1].task == SIMT_LEISURE);
    // 3. the season decides the harvest: the same towns, the same larder, three days of autumn against three days of winter
    { double au = 0, wi = 0; for(int sd = 1; sd <= 16; sd++){ for(int pass = 0; pass < 2; pass++){ sim_init(&A, (uint64_t)sd * 997u, 10, 0, 0); sim_set_env(&A, 24, 12);
            uint64_t st = (uint64_t)(pass ? 3 : 2) * SIM_SEASON_DAYS * SIM_DAY_TICKS; A.tick = st; A.day_seen = (uint32_t)sim_day(&A); sim_run(&A, SIM_DAY_TICKS); A.larder = 300; uint32_t l0 = A.larder; sim_run(&A, 3 * SIM_DAY_TICKS);
            if(pass) wi += (double)A.larder - l0; else au += (double)A.larder - l0; } }
      CHECK(sim_season(&A) == 3); CHECK(au / 16 > wi / 16 + 80); }
    // 4. the larder is a stock that matters: bounded, not a hoard, and the stock follows the year (spring 100, summer 120, autumn 140, winter 35 percent of a harvest)
    { uint32_t hi = 0; int empty = 0; for(int sd = 1; sd <= 20; sd++){ sim_init(&A, (uint64_t)sd * 6151u, 12, 0, 0); sim_set_env(&A, 24, 14); int hit0 = 0;
          for(uint64_t t = 0; t < 2ull * 4 * SIM_SEASON_DAYS * SIM_DAY_TICKS; t++){ sim_step(&A); if(A.larder > hi) hi = A.larder; if(t > 5ull * SIM_DAY_TICKS && A.larder == 0) hit0 = 1; } empty += hit0; }
      CHECK(hi < 2600); CHECK(empty >= 1 && empty <= 15); }                                    // some towns do run out in two years, most do not
    // 5. every job has an effect now
    // (a) foragers learn (their skill never grew before)
    sim_init(&A, 15, 6, 0, 0); { int fk = -1; for(int i = 0; i < 6; i++) if(A.folk[i].job == SIMJ_FORAGER) fk = i; CHECK(fk >= 0); sim_run(&A, 12 * SIM_DAY_TICKS); CHECK(A.folk[fk].skill > 0); }
    // (b) a cook on duty stretches the meals: the same hour, the same food, four bugs at the table
    { uint32_t used[2];
      for(int pass = 0; pass < 2; pass++){ sim_init(&A, 16, 4, 0, 0); all_job(&A, SIMJ_CLERK); if(pass){ A.folk[0].job = SIMJ_COOK; A.folk[1].job = SIMJ_COOK; }
          for(int i = 0; i < 4; i++){ A.folk[i].shift = 0; A.folk[i].need[SIMN_HUNGER] = 100; A.folk[i].need[SIMN_REST] = 220; A.folk[i].task = SIMT_EAT; }
          A.tick = 12 * 200 + 1; A.day_seen = (uint32_t)sim_day(&A); A.larder = 1000; sim_run(&A, 190); used[pass] = 1000 - A.larder; }
      CHECK(used[0] > used[1] + 5); }
    // (c) guards keep the pests down
    { double p0 = 0, p1 = 0; for(int sd = 1; sd <= 10; sd++){ for(int pass = 0; pass < 2; pass++){ sim_init(&A, (uint64_t)sd * 41u, 6, 0, 0); all_job(&A, SIMJ_CLERK); if(pass){ A.folk[0].job = SIMJ_GUARD; A.folk[1].job = SIMJ_GUARD; }
            A.pests = 60; sim_run(&A, 6 * SIM_DAY_TICKS); if(pass) p1 += A.pests; else p0 += A.pests; } }
      CHECK(p1 < p0 - 20); }
    // (d) a racer's show cheers the town up
    { double f0 = 0, f1 = 0; for(int sd = 1; sd <= 10; sd++){ for(int pass = 0; pass < 2; pass++){ sim_init(&A, (uint64_t)sd * 59u, 8, 0, 0); all_job(&A, SIMJ_CLERK); if(pass){ A.folk[0].job = SIMJ_RACER; A.folk[1].job = SIMJ_RACER; }
            sim_run(&A, 8 * SIM_DAY_TICKS); for(int i = 2; i < 8; i++){ if(pass) f1 += A.folk[i].need[SIMN_FUN]; else f0 += A.folk[i].need[SIMN_FUN]; } } }
      CHECK(f1 > f0 + 60); }
    // (e) stall keepers and clerks bring in food by trading; a town of idle bugs does not get it for free any more
    { double t0 = 0, t1 = 0; for(int sd = 1; sd <= 10; sd++){ for(int pass = 0; pass < 2; pass++){ sim_init(&A, (uint64_t)sd * 73u, 8, 0, 0); all_job(&A, pass ? SIMJ_CLERK : SIMJ_GUARD); A.larder = 400; sim_run(&A, 2 * SIM_DAY_TICKS); if(pass) t1 += A.larder; else t0 += A.larder; } }
      CHECK(t1 > t0 + 100); }
    // 6. a skilled bug at work counts for more than a green one (the same job, more skill, more done)
    { uint32_t g[2]; for(int pass = 0; pass < 2; pass++){ sim_init(&A, 17, 4, 0, 0); all_job(&A, SIMJ_CLERK); A.folk[0].job = SIMJ_FORAGER; A.folk[0].skill = pass ? 60 : 0; A.folk[0].shift = 0; A.tick = 9 * 200; A.day_seen = (uint32_t)sim_day(&A);
          for(int i = 0; i < 4; i++){ A.folk[i].need[SIMN_HUNGER] = 220; A.folk[i].need[SIMN_REST] = 220; } A.larder = 100; sim_run(&A, 6 * 200); g[pass] = A.larder; }
      CHECK(g[1] >= g[0]); }
    // 7. determinism and a clean state: the same seed gives the same hash; the larder never overflows
    sim_init(&A, 99, 12, 0, 0); sim_init(&B, 99, 12, 0, 0); sim_run(&A, 5 * SIM_DAY_TICKS); sim_run(&B, 5 * SIM_DAY_TICKS); CHECK(sim_hash(&A) == sim_hash(&B));
    A.larder = 9990; sim_run(&A, SIM_DAY_TICKS); CHECK(A.larder <= 9999);
    printf("life_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
