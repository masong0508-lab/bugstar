// god_test.c: tests for the god powers table and the level thresholds (god.c)
#include <stdio.h>
#include <string.h>
#include "god.h"

static int fails, checks;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static Lv U(uint64_t v){ Lv r; lv_from_u64(&r, v); return r; }

int main(void){
    int P = god_count();
    CHECK(P >= 40); CHECK(GOD_STEP_LV == 1000);
    // table quality: names/lines present, capital letters only, fit the screen, names unique, nothing copied from a game or wiki
    for(int n = 1; n <= P; n++){
        const GodPower *g = god_power(n);
        CHECK(g && g->name && g->line);
        size_t ln = strlen(g->name), ll = strlen(g->line);
        CHECK(ln >= 5 && ln <= 22); CHECK(ll >= 12 && ll <= 46);
        for(size_t i = 0; i < ln; i++) CHECK((g->name[i] >= 'A' && g->name[i] <= 'Z') || g->name[i] == ' ');
        for(size_t i = 0; i < ll; i++) CHECK((g->line[i] >= 'A' && g->line[i] <= 'Z') || g->line[i] == ' ' || g->line[i] == '.' || g->line[i] == ',');
        CHECK(g->eff < GE_NEFFECT); CHECK(g->cost >= 1 && g->cost <= 30);
        for(int m = n + 1; m <= P; m++) CHECK(strcmp(g->name, god_power(m)->name) != 0);
    }
    CHECK(god_power(0) == 0); CHECK(god_power(P + 1) == 0); CHECK(god_power(-1) == 0);
    // the named examples from the plan exist
    const char *must[] = { "BLESS A FIELD", "CALM A STORM", "CALL RAIN", "SPARK AN IDEA", "RAISE A HILL", "PLANT A FOREST", "SEED A SPECIES" };
    for(unsigned k = 0; k < sizeof must / sizeof *must; k++){ int found = 0; for(int n = 1; n <= P; n++) if(!strcmp(god_power(n)->name, must[k])) found = 1; CHECK(found); }

    // thresholds are exact: power n opens at 1000 x n
    for(int n = 1; n <= P; n++){
        uint64_t need = (uint64_t)GOD_STEP_LV * n; Lv a = U(need - 1), b = U(need), c = U(need + 1), nd;
        CHECK(!god_unlocked(&a, n, 0)); CHECK(god_unlocked(&b, n, 0)); CHECK(god_unlocked(&c, n, 0));
        god_need_level(n, &nd); CHECK(lv_cmp_u64(&nd, need) == 0);
    }
    Lv l;
    l = U(999);  CHECK(god_unlocked_count(&l, 0) == 0); CHECK(!god_unlocked(&l, 1, 0));
    l = U(1000); CHECK(god_unlocked_count(&l, 0) == 1); CHECK(god_unlocked(&l, 1, 0)); CHECK(!god_unlocked(&l, 2, 0));
    l = U(1999); CHECK(god_unlocked_count(&l, 0) == 1); CHECK(!god_unlocked(&l, 2, 0));
    l = U(2000); CHECK(god_unlocked_count(&l, 0) == 2); CHECK(god_unlocked(&l, 2, 0));
    l = U(0);    CHECK(god_unlocked_count(&l, 0) == 0);
    l = U(1000ULL * P - 1); CHECK(god_unlocked_count(&l, 0) == P - 1);
    l = U(1000ULL * P);     CHECK(god_unlocked_count(&l, 0) == P);
    l = U(UINT64_MAX);      CHECK(god_unlocked_count(&l, 0) == P);
    { Lv cap; lv_cap(&cap); CHECK(god_unlocked_count(&cap, 0) == P); Lv one = U(1); CHECK(god_unlocked_count(&one, 0) == 0); }
    // numbers outside the table never open, even in sandbox
    l = U(UINT64_MAX); CHECK(!god_unlocked(&l, 0, 0)); CHECK(!god_unlocked(&l, P + 1, 0)); CHECK(!god_unlocked(&l, P + 1, 1)); CHECK(!god_unlocked(&l, -3, 1));
    // sandbox opens every power at level 1
    l = U(1); CHECK(god_unlocked_count(&l, 1) == P); for(int n = 1; n <= P; n++) CHECK(god_unlocked(&l, n, 1));

    // announcements: each unlock is told once, in order
    { GodState g; god_state_init(&g); int out[8]; Lv lv = U(999);
      CHECK(god_check(&g, &lv, out, 8) == 0);
      lv = U(1000); CHECK(god_check(&g, &lv, out, 8) == 1 && out[0] == 1);
      CHECK(god_check(&g, &lv, out, 8) == 0);                              // told once
      lv = U(1999); CHECK(god_check(&g, &lv, out, 8) == 0);
      lv = U(2000); CHECK(god_check(&g, &lv, out, 8) == 1 && out[0] == 2);
      lv = U(5000); CHECK(god_check(&g, &lv, out, 8) == 3 && out[0] == 3 && out[1] == 4 && out[2] == 5);
      lv = U(1000ULL * P); int c = god_check(&g, &lv, out, 8); CHECK(c == 8);          // capped by max: the rest come next call
      c = god_check(&g, &lv, out, 8); CHECK(c == 8);
      int total = 5 + 8 + 8; while((c = god_check(&g, &lv, out, 8)) > 0) total += c; CHECK(total == P);
      // save and load keeps what was told
      uint8_t by[GOD_STATE_BYTES]; god_state_to_bytes(&g, by); GodState h; god_state_init(&h); god_state_from_bytes(&h, by);
      CHECK(memcmp(&g, &h, sizeof g) == 0); CHECK(god_check(&h, &lv, out, 8) == 0);
      // half-told state
      GodState k; god_state_init(&k); lv = U(3000); god_check(&k, &lv, out, 8); god_state_to_bytes(&k, by); GodState k2; god_state_from_bytes(&k2, by);
      CHECK(god_check(&k2, &lv, out, 8) == 0); lv = U(4000); CHECK(god_check(&k2, &lv, out, 8) == 1 && out[0] == 4);
      // damaged bytes cannot make powers past the table "told"
      memset(by, 0xFF, sizeof by); GodState d; god_state_from_bytes(&d, by); for(int n = P + 1; n <= GOD_MAX; n++) CHECK(!((d.seen[(n - 1) >> 3] >> ((n - 1) & 7)) & 1)); CHECK(d.sandbox == 1);
      // sandbox: no spam, all told at once
      GodState s; god_state_init(&s); s.sandbox = 1; lv = U(1); CHECK(god_check(&s, &lv, out, 8) == 0); s.sandbox = 0; CHECK(god_check(&s, &lv, out, 8) == 0); }

    // texts
    { char b[96]; Lv lv = U(500); god_locked_text(3, &lv, b, sizeof b); CHECK(!strcmp(b, "LOCKED. NEEDS LEVEL 3000"));
      lv = U(3000); god_locked_text(3, &lv, b, sizeof b); CHECK(!strcmp(b, "OPEN"));
      god_unlock_text(3, b, sizeof b); CHECK(!strcmp(b, "GOD POWER 3 IS OPEN: CALL RAIN"));
      god_unlock_text(99999, b, sizeof b); CHECK(b[0] == 0);
      god_locked_text(P, &lv, b, sizeof b); char want[64]; sprintf(want, "LOCKED. NEEDS LEVEL %d", 1000 * P); CHECK(!strcmp(b, want));
      char tiny[8]; god_unlock_text(1, tiny, sizeof tiny); CHECK(strlen(tiny) == 7); }

    // determinism: same inputs, same answers, any order
    { int a1 = 0, a2 = 0; for(uint64_t v = 0; v < 60000; v += 137){ Lv x = U(v); a1 += god_unlocked_count(&x, 0); } for(uint64_t v = 0; v < 60000; v += 137){ Lv x = U(v); a2 += god_unlocked_count(&x, 0); } CHECK(a1 == a2 && a1 > 0); }

    printf("god_test: %d powers, %d checks, %d failed\n", P, checks, fails);
    return fails != 0;
}
