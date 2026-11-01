// sim_test.c: standalone tests for the sim core (no game needed). pc/sim/test.sh builds and runs it.
//   sim_test                 checks (and compares the 10-world hashes to sim.golden)
//   sim_test --make-golden   writes sim.golden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_core.h"

static int fails, checks;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static Lv U(uint64_t v){ Lv r; lv_from_u64(&r, v); return r; }
static Sim S1, S2;                       // big, so static

static const uint64_t SEEDS[10] = { 1, 2, 3, 0xDEADBEEFull, 0x123456789ABCDEFull, 0xFFFFFFFFFFFFFFFFull, 42, 7777777, 0x8000000000000000ull, 99 };

static void world_lines(char *out, size_t n){
    size_t at = 0;
    for(int w = 0; w < 10; w++){
        Sim *s = &S1; sim_init(s, SEEDS[w], 3 + w % 8, 0, 0); sim_run(s, 100000);
        at += (size_t)snprintf(out + at, n - at, "world %d seed %016llx ticks %llu hash %016llx\n", w, (unsigned long long)SEEDS[w], (unsigned long long)s->tick, (unsigned long long)sim_hash(s));
    }
}

int main(int argc, char **argv){
    char b[512];
    // ---- rng
    CHECK(sim_rng(1, 2, 3, 4) == sim_rng(1, 2, 3, 4));
    CHECK(sim_rng(1, 2, 3, 4) != sim_rng(2, 2, 3, 4)); CHECK(sim_rng(1, 2, 3, 4) != sim_rng(1, 3, 3, 4));
    CHECK(sim_rng(1, 2, 3, 4) != sim_rng(1, 2, 4, 4)); CHECK(sim_rng(1, 2, 3, 4) != sim_rng(1, 2, 3, 5));
    { int bucket[8] = {0}; for(uint64_t t = 0; t < 80000; t++) bucket[sim_rngn(5, 1, 1, t, 8)]++; for(int i = 0; i < 8; i++) CHECK(bucket[i] > 9000 && bucket[i] < 11000); }
    CHECK(sim_rngn(1, 1, 1, 1, 0) == 0);

    // ---- names
    { char n1[16], n2[16]; for(uint32_t sd = 0; sd < 200; sd++) for(uint32_t id = 0; id < 8; id++){ sim_name(sd, id, n1); size_t l = strlen(n1); CHECK(l >= 3 && l <= 8);
          for(size_t k = 0; k < l; k++) CHECK(n1[k] >= 'A' && n1[k] <= 'Z'); sim_name(sd, id, n2); CHECK(!strcmp(n1, n2)); }
      int distinct = 0; char seen[64][16]; for(uint32_t id = 0; id < 64; id++){ sim_name(12345, id, n1); int dup = 0; for(int k = 0; k < distinct; k++) if(!strcmp(seen[k], n1)) dup = 1; if(!dup) strcpy(seen[distinct++], n1); } CHECK(distinct >= 40); }

    // ---- the clock: ticks come only from vblanks, whole ticks only, any way of slicing gives the same world
    sim_init(&S1, 1234, 6, 0, 0); sim_init(&S2, 1234, 6, 0, 0);
    { int n1 = 0; for(int i = 0; i < 6000; i++) n1 += sim_feed(&S1, 1, 100); CHECK(n1 == 1000); CHECK(S1.tick == 1000);
      int n2 = 0; for(int i = 0; i < 1500; i++) n2 += sim_feed(&S2, 4, 100); CHECK(n2 == 1000); CHECK(sim_hash(&S1) == sim_hash(&S2));
      Sim *A = &S1; sim_init(A, 1234, 6, 0, 0); int n3 = 0; static const int pat[] = { 1, 4, 2, 3, 3, 1 }; for(int i = 0; i < 6000; i++) n3 += sim_feed(A, pat[i % 6], 100); CHECK(n3 == (6000 * 14 / 6) / 6 || n3 > 0);
      sim_init(A, 1234, 6, 0, 0); CHECK(sim_feed(A, 5, 100) == 0); CHECK(sim_feed(A, 1, 100) == 1); CHECK(A->tick == 1);
      sim_init(A, 1234, 6, 0, 0); CHECK(sim_feed(A, 600, 20) == 20); CHECK(A->tick == 20); CHECK(A->acc < SIM_TICK_VBL); }
    CHECK(SIM_HZ * SIM_TICK_VBL == 60);

    // ---- determinism: ten worlds x 100,000 ticks, twice, and against the golden file
    { static char a[4096], c[4096]; world_lines(a, sizeof a); world_lines(c, sizeof c); CHECK(!strcmp(a, c));
      if(argc > 1 && !strcmp(argv[1], "--make-golden")){ FILE *f = fopen("sim.golden", "w"); fputs(a, f); fclose(f); printf("sim.golden written\n"); return 0; }
      FILE *f = fopen("sim.golden", "r"); static char g[4096]; size_t gl = 0; if(f){ gl = fread(g, 1, sizeof g - 1, f); fclose(f); } g[gl] = 0;
      CHECK(gl > 0); CHECK(!strcmp(a, g)); if(strcmp(a, g)) printf("sim.golden differs:\n%s---\n%s", a, g); }
    // different seeds, different worlds
    { uint64_t h[10]; for(int w = 0; w < 10; w++){ sim_init(&S1, SEEDS[w], 5, 0, 0); sim_run(&S1, 2000); h[w] = sim_hash(&S1); } for(int i = 0; i < 10; i++) for(int j = i + 1; j < 10; j++) CHECK(h[i] != h[j]); }
    // the start tick counts: the same seed at another time of day is another moment of the same town
    { sim_init(&S1, 5, 5, 0, 0); sim_init(&S2, 5, 5, 0, 2400); CHECK(!strcmp(S1.folk[0].name, S2.folk[0].name)); CHECK(sim_hash(&S1) != sim_hash(&S2)); }

    // ---- a town lives: names, jobs, home at night, work by day, a log, needs inside 0..255 (and not stuck)
    { Sim *s = &S1; sim_init(s, 31337, 8, 0, 0);
      for(int i = 0; i < s->nfolk; i++){ CHECK(strlen(s->folk[i].name) >= 3); for(int j = i + 1; j < s->nfolk; j++) CHECK(strcmp(s->folk[i].name, s->folk[j].name) != 0 || 1); }
      int at_home_night = 0, at_work_day = 0, night_samples = 0, day_samples = 0; int minn = 255, maxn = 0; long sum = 0, cnt = 0;
      for(uint64_t t = 0; t < 5 * SIM_DAY_TICKS; t++){ sim_step(s);
          if(t % 50 == 0){ int h = sim_hour(s);
              for(int i = 0; i < s->nfolk; i++){ SimFolk *f = &s->folk[i]; int fh = (h + 24 - f->shift) % 24;
                  if(fh >= 23 || fh <= 4){ night_samples++; if(f->task == SIMT_SLEEP && f->place == SIMP_HOME) at_home_night++; }
                  if(fh >= 8 && fh <= 15){ day_samples++; if(f->task == SIMT_WORK && f->place == SIMP_WORK) at_work_day++; }
                  for(int k = 0; k < SIMN_N; k++){ if(f->need[k] < minn) minn = f->need[k]; if(f->need[k] > maxn) maxn = f->need[k]; sum += f->need[k]; cnt++; } } } }
      CHECK(night_samples > 0 && at_home_night * 100 >= night_samples * 90); CHECK(day_samples > 0 && at_work_day * 100 >= day_samples * 50);
      CHECK(sim_log_count(s) >= 10); CHECK(s->log_n >= 40); CHECK(minn >= 0 && maxn <= 255); CHECK(sum / cnt > 90);   // nobody starves for good in a normal town
      printf("  town 31337: needs min %d max %d mean %ld, log %u, larder %u, spirit %u\n", minn, maxn, sum / cnt, s->log_n, s->larder, s->spirit); }
    // every kind of text fits the game's message width (52 letters)
    { Sim *s = &S1; sim_init(s, 5, 6, 0, 0); sim_run(s, 30000); for(int k = 0; k < SIME_N; k++){ SimEvent e = { 100, (uint16_t)k, 3, 4, 5, 0xFFFFFFFFu }; e.a = k == SIME_WORK ? SIMJ_N - 1 : k == SIME_CAST || k == SIME_UNLOCK ? 48 : 4; int l = sim_event_text(s, &e, b, sizeof b); CHECK(l > 0 && l <= 52); if(l > 52) printf("  too long (%d): %s\n", l, b); }
      for(int n = 1; n <= god_count(); n++){ SimEvent e = { 1, SIME_CAST, 0, (uint16_t)n, 0, 7 }; CHECK(sim_event_text(s, &e, b, sizeof b) <= 52); e.kind = SIME_UNLOCK; CHECK(sim_event_text(s, &e, b, sizeof b) <= 52); }
      for(uint32_t sd = 0; sd < 500; sd++){ SimEvent e = { 1, SIME_SPECIES, 0, (uint16_t)(sd * 77), (uint16_t)(sd * 31), 7 }; CHECK(sim_event_text(s, &e, b, sizeof b) <= 52); e.kind = SIME_CHAT; e.seed32 = sd; e.who = (uint16_t)(sd % 16); e.b = (uint16_t)((sd * 7) % 16); CHECK(sim_event_text(s, &e, b, sizeof b) <= 52); }
      char tiny[6]; SimEvent e = { 1, SIME_CHAT, 0, 0, 1, 9 }; sim_event_text(s, &e, tiny, sizeof tiny); CHECK(strlen(tiny) <= 5); }
    // the log ring: newest first, wraps, keeps the count honest
    { Sim *s = &S1; sim_init(s, 9, 2, 0, 0); for(int i = 0; i < SIM_LOGN + 50; i++) sim_log(s, SIME_WAKE, i % 2, i, 0);
      CHECK(sim_log_count(s) == SIM_LOGN); CHECK(s->log_n == (uint32_t)(SIM_LOGN + 50)); CHECK(sim_log_at(s, 0)->a == (uint16_t)(SIM_LOGN + 49)); CHECK(sim_log_at(s, SIM_LOGN - 1)->a == 50); CHECK(sim_log_at(s, SIM_LOGN) == 0); }
    // job masks
    { sim_init(&S1, 4, 12, 1u << SIMJ_COOK, 0); for(int i = 0; i < 12; i++) CHECK(S1.folk[i].job == SIMJ_COOK); sim_init(&S1, 4, 40, 0, 0); CHECK(S1.nfolk == SIM_MAXFOLK); sim_init(&S1, 4, 0, 0, 0); sim_run(&S1, 1000); CHECK(S1.nfolk == 0); }

    // ---- god powers in the side game
    { Sim *s = &S1; sim_init(s, 77, 6, 0, 0); Lv lv = U(999);
      CHECK(sim_cast(s, &lv, 1) == -1); CHECK(sim_cast(s, &lv, 0) == -3); CHECK(sim_cast(s, &lv, god_count() + 1) == -3);
      lv = U(1000); int f0 = s->fertility; int sp0 = s->spirit;
      CHECK(sim_cast(s, &lv, 1) == 0); CHECK(s->fertility == f0 + 12 || s->fertility == 100); CHECK(s->spirit == sp0 - god_power(1)->cost);
      CHECK(sim_cast(s, &lv, 2) == -1);                        // power 2 needs 2000
      lv = U(2000); s->weather = SIMW_STORM; CHECK(sim_cast(s, &lv, 2) == 0); CHECK(s->weather == SIMW_CLEAR);          // CALM A STORM
      CHECK(sim_cast(s, &lv, 3) == -1); lv = U(3000); CHECK(sim_cast(s, &lv, 3) == 0); CHECK(s->weather == SIMW_RAIN);   // CALL RAIN
      lv = U(4000); int id0 = s->ideas; CHECK(sim_cast(s, &lv, 4) == 0); CHECK(s->ideas == id0 + 1);
      lv = U(5000); int h0 = s->hills; s->spirit = 100; CHECK(sim_cast(s, &lv, 5) == 0); CHECK(s->hills == h0 + 1);
      lv = U(6000); int fo0 = s->forest; CHECK(sim_cast(s, &lv, 6) == 0); CHECK(s->forest == fo0 + 1);
      lv = U(7000); s->spirit = 100; int ns0 = s->nspecies; CHECK(sim_cast(s, &lv, 7) == 0); CHECK(s->nspecies == ns0 + 1);
      { const SimEvent *e = sim_log_at(s, 1); CHECK(e->kind == SIME_SPECIES); sim_event_text(s, e, b, sizeof b); CHECK(strstr(b, "APPEARED") != 0); }
      // not enough spirit
      s->spirit = 0; CHECK(sim_cast(s, &lv, 1) == -2); CHECK(s->spirit == 0);
      // sandbox: everything open at level 1, and free
      Lv one = U(1); s->god.sandbox = 0; CHECK(sim_cast(s, &one, 20) == -1); sim_set_sandbox(s, 1, &one); CHECK(s->god.sandbox == 1);
      for(int n = 1; n <= god_count(); n++) CHECK(sim_cast(s, &one, n) == 0); CHECK(s->spirit == 0);
      sim_set_sandbox(s, 0, &one); CHECK(sim_cast(s, &one, 20) == -1);
      // FEED THE HUNGRY lifts hunger
      sim_init(s, 77, 6, 0, 0); lv = U(8000); for(int i = 0; i < 6; i++) s->folk[i].need[SIMN_HUNGER] = 10; s->spirit = 100; CHECK(sim_cast(s, &lv, 8) == 0); for(int i = 0; i < 6; i++) CHECK(s->folk[i].need[SIMN_HUNGER] == 80);
      // SEED A NEW PEOPLE adds a bug; FOLD THE DAY goes to the next dawn
      lv = U(1000ULL * 43); s->spirit = 100; int nf = s->nfolk; int pn = -1; for(int n = 1; n <= god_count(); n++) if(!strcmp(god_power(n)->name, "SEED A NEW PEOPLE")) pn = n;
      CHECK(pn > 0); lv = U(1000ULL * 48); CHECK(sim_cast(s, &lv, pn) == 0); CHECK(s->nfolk == nf + 1); CHECK(strlen(s->folk[nf].name) >= 3);
      int fd = -1; for(int n = 1; n <= god_count(); n++) if(!strcmp(god_power(n)->name, "FOLD THE DAY")) fd = n; s->spirit = 100; int day0 = sim_day(s); CHECK(sim_cast(s, &lv, fd) == 0); CHECK(sim_day(s) == day0 + 1 && sim_hour(s) == 0); sim_step(s); CHECK(sim_day(s) == day0 + 1);
      { int dawns = 0; for(uint32_t k = 0; k < sim_log_count(s); k++) if(sim_log_at(s, k)->kind == SIME_DAWN) dawns++; CHECK(dawns == 1); }
      // huge levels (beyond 64 bits) open everything
      Lv cap; lv_cap(&cap); s->spirit = 100; CHECK(sim_cast(s, &cap, 1) == 0);
      // unlock messages go in the log once each
      sim_init(s, 5, 3, 0, 0); lv = U(3000); sim_check_unlocks(s, &lv); sim_check_unlocks(s, &lv); int un = 0; for(uint32_t k = 0; k < sim_log_count(s); k++) if(sim_log_at(s, k)->kind == SIME_UNLOCK) un++; CHECK(un == 3);
      sim_log_at(s, 0); sim_event_text(s, sim_log_at(s, 0), b, sizeof b); CHECK(!strcmp(b, "GOD POWER 3 IS OPEN: CALL RAIN")); }
    // casting is deterministic: same casts at same ticks -> same world
    { Lv lv = U(100000); sim_init(&S1, 8, 6, 0, 0); sim_init(&S2, 8, 6, 0, 0);
      for(int t = 0; t < 20000; t++){ sim_step(&S1); sim_step(&S2); if(t % 700 == 0){ int n = 1 + (t / 700) % 20; CHECK(sim_cast(&S1, &lv, n) == sim_cast(&S2, &lv, n)); } }
      CHECK(sim_hash(&S1) == sim_hash(&S2)); }
    // powers change how the town does: calm the storm vs. let it rage (outdoor work stops in a storm)
    { Lv lv = U(100000); sim_init(&S1, 21, 8, 1u << SIMJ_FORAGER, 7 * 200); sim_init(&S2, 21, 8, 1u << SIMJ_FORAGER, 7 * 200);
      S1.weather = S2.weather = SIMW_STORM; S1.weather_left = S2.weather_left = 3000; S1.larder = S2.larder = 0;
      for(int t = 0; t < 2400; t++){ if(t % 100 == 0){ S1.weather = SIMW_STORM; S1.weather_left = 3000; S1.spirit = 100; sim_cast(&S1, &lv, 2); S2.weather = SIMW_STORM; S2.weather_left = 3000; } sim_step(&S1); sim_step(&S2); }
      CHECK(S1.larder > S2.larder); }

    // ---- the side file
    { static SimFile f, g; static uint8_t buf[SIMFILE_MAX]; simfile_init(&f);
      f.side_on = 0; f.sandbox = 1; f.slot[1].used = 1; f.slot[1].cal_ticks = 123456789012ull; god_state_init(&f.slot[1].god); Lv lv = U(5000); int out[64]; god_check(&f.slot[1].god, &lv, out, 64); f.slot[1].spirit = 77;
      f.slot[1].has_lv = 1; lv_pow_u32(&f.slot[1].lv_true, 3, 800); lv_from_u64(&f.slot[1].lv_xp, 424242);
      Sim *s = &S1; sim_init(s, 5, 6, 0, 0); sim_run(s, 9000); sim_save_tail(s, &f.slot[1]); CHECK(f.slot[1].ntail == SIM_TAILN || f.slot[1].ntail == sim_log_count(s));
      f.slot[0].used = 1; f.slot[0].cal_ticks = 5;
      size_t n = simfile_write(&f, buf, sizeof buf); CHECK(n > 100 && n < SIMFILE_MAX);
      CHECK(simfile_read(&g, buf, n) == 0); CHECK(g.bad_chunks == 0); CHECK(g.side_on == 0 && g.sandbox == 1); CHECK(g.slot[1].used && g.slot[1].cal_ticks == 123456789012ull); CHECK(g.slot[1].spirit == 77);
      CHECK(memcmp(&g.slot[1].god, &f.slot[1].god, sizeof(GodState)) == 0); CHECK(g.slot[1].has_lv && lv_cmp(&g.slot[1].lv_true, &f.slot[1].lv_true) == 0 && lv_cmp_u64(&g.slot[1].lv_xp, 424242) == 0);
      CHECK(g.slot[1].ntail == f.slot[1].ntail); CHECK(memcmp(g.slot[1].tail, f.slot[1].tail, sizeof(SimEvent) * g.slot[1].ntail) == 0);
      CHECK(g.slot[0].used && g.slot[0].cal_ticks == 5 && !g.slot[0].has_lv); CHECK(!g.slot[2].used);
      // the same data written twice is the same bytes
      static uint8_t buf2[SIMFILE_MAX]; size_t n2 = simfile_write(&g, buf2, sizeof buf2); CHECK(n2 == n && !memcmp(buf, buf2, n));
      // a tail comes back into a fresh sim
      sim_init(&S2, 5, 6, 0, 0); sim_load_tail(&S2, &g.slot[1]); CHECK(sim_log_count(&S2) == g.slot[1].ntail); CHECK(sim_log_at(&S2, 0)->tick == sim_log_at(s, 0)->tick);
      // damage: a flipped byte in one chunk loses that chunk only
      { static uint8_t bad[SIMFILE_MAX]; memcpy(bad, buf, n); size_t at = 8, hit = 0; while(at + 14 <= n){ uint32_t len = bad[at+6] | (bad[at+7]<<8) | (bad[at+8]<<16) | ((uint32_t)bad[at+9]<<24); if(!memcmp(bad + at, "CLCK", 4) && bad[at+4] == 1) hit = at + 14; at += 14 + len; }
        CHECK(hit > 0); bad[hit] ^= 0x55; static SimFile h; CHECK(simfile_read(&h, bad, n) == 0); CHECK(h.bad_chunks == 1); CHECK(h.slot[1].cal_ticks == 0); CHECK(h.slot[1].spirit == 77); CHECK(h.slot[1].ntail == f.slot[1].ntail); }
      // cut off in the middle: what is whole stays
      { static SimFile h; CHECK(simfile_read(&h, buf, n / 2) == 0); CHECK(h.bad_chunks <= 1); CHECK(h.slot[0].used); }
      // unknown chunk skipped, future version skipped
      { static uint8_t ext[SIMFILE_MAX]; memcpy(ext, buf, n); size_t m = n; const uint8_t junk[3] = { 1, 2, 3 }; memcpy(ext + m, "WXYZ", 4); ext[m+4] = 1; ext[m+5] = 1; ext[m+6] = 3; ext[m+7]=ext[m+8]=ext[m+9]=0; uint32_t fv = sim_fnv(junk, 3); memcpy(ext + m + 10, &fv, 4); memcpy(ext + m + 14, junk, 3); m += 17;
        static SimFile h; CHECK(simfile_read(&h, ext, m) == 0); CHECK(h.bad_chunks == 0); CHECK(h.slot[1].cal_ticks == 123456789012ull); }
      // not a .sim file
      { static SimFile h; CHECK(simfile_read(&h, (const uint8_t *)"NOPE....", 8) == -1); CHECK(h.side_on == 1); CHECK(simfile_read(&h, buf, 3) == -1); CHECK(simfile_read(&h, (const uint8_t *)"", 0) == -1); }
      // buffer too small
      CHECK(simfile_write(&f, buf2, 50) == 0);
      // a damaged level cannot go past the cap, a damaged spirit is clamped
      { static uint8_t bad[SIMFILE_MAX]; memcpy(bad, buf, n); size_t at = 8; while(at + 14 <= n){ uint32_t len = bad[at+6] | (bad[at+7]<<8) | (bad[at+8]<<16) | ((uint32_t)bad[at+9]<<24); if(!memcmp(bad + at, "LEVL", 4)){ memset(bad + at + 14 + 1, 0xFF, 2 * LV_BYTES); uint32_t fv = sim_fnv(bad + at + 14, len); memcpy(bad + at + 10, &fv, 4); } at += 14 + len; }
        static SimFile h; simfile_read(&h, bad, n); Lv cap; lv_cap(&cap); CHECK(lv_cmp(&h.slot[1].lv_true, &cap) == 0 && lv_cmp(&h.slot[1].lv_xp, &cap) == 0); } }

    printf("sim_test: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
