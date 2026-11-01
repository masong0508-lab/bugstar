// lvxp_test.c: tests for lvxp.c.   lvxp_test            built-in checks
//                                  lvxp_test --batch    lines "L X pct" (decimal) -> "newlevel newbank gained"  (compared with Python by test.sh)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvxp.h"

static int fails, checks;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static Lv P(const char *s){ Lv r; lv_from_dec(&r, s); return r; }
static Lv U(uint64_t v){ Lv r; lv_from_u64(&r, v); return r; }
static void D(const Lv *a, char *b){ lv_to_dec(a, b, LV_DEC_MAX); }

int main(int argc, char **argv){
    if(argc > 1 && !strcmp(argv[1], "--batch")){
        char a[LV_DEC_MAX + 8], b[LV_DEC_MAX + 8]; unsigned pct;
        while(scanf("%s %s %u", a, b, &pct) == 3){
            Lv L = P(a), X = P(b), z, g; lv_zero(&z); lvxp_gain(&L, &X, &z, pct, &g);
            char o1[LV_DEC_MAX], o2[LV_DEC_MAX], o3[LV_DEC_MAX]; D(&L, o1); D(&X, o2); D(&g, o3); printf("%s %s %s\n", o1, o2, o3);
        }
        return 0;
    }
    char s[LV_DEC_MAX]; Lv n;
    // one level costs 18 L^2 + 12 (same as the game's xp_next)
    { Lv L = U(1); lvxp_need(&L, 100, &n); CHECK(lv_cmp_u64(&n, 30) == 0); L = U(10); lvxp_need(&L, 100, &n); CHECK(lv_cmp_u64(&n, 1812) == 0);
      L = U(1000); lvxp_need(&L, 100, &n); CHECK(lv_cmp_u64(&n, 18000012ULL) == 0); L = U(1000); lvxp_need(&L, 50, &n); CHECK(lv_cmp_u64(&n, 9000006ULL) == 0);
      L = U(500000000ULL); lvxp_need(&L, 100, &n); CHECK(lv_cmp_u64(&n, 18ULL * 500000000ULL * 500000000ULL + 12) == 0);
      Lv cap; lv_cap(&cap); lvxp_need(&cap, 100, &n); CHECK(lv_cmp(&n, &cap) == 0); }
    // cost(1) = need; cost(n) matches adding up one at a time
    for(uint64_t L0 = 1; L0 < 40; L0 += 7) for(uint64_t k = 1; k < 30; k++){
        Lv L = U(L0), tot = U(0), one = U(1), cost; for(uint64_t i = 0; i < k; i++){ Lv Li = U(L0 + i), nd; lvxp_need(&Li, 100, &nd); lv_add(&tot, &tot, &nd); }
        Lv kk = U(k); CHECK(lvxp_cost(&L, &kk, 100, &cost) == 0); CHECK(lv_cmp(&cost, &tot) == 0); (void)one; }
    // gain: step by step against the plain loop for small numbers
    for(uint64_t L0 = 1; L0 < 60; L0 += 13) for(uint64_t X0 = 0; X0 < 200000; X0 += 7919){
        uint64_t l = L0, x = X0; while(x >= 18 * l * l + 12){ x -= 18 * l * l + 12; l++; }
        Lv L = U(L0), X = U(X0), z = U(0), g; lvxp_gain(&L, &X, &z, 100, &g); CHECK(lv_cmp_u64(&L, l) == 0); CHECK(lv_cmp_u64(&X, x) == 0); CHECK(lv_cmp_u64(&g, l - L0) == 0); }
    // gaining xp twice = once (splitting an amount does not change the result)
    { Lv L = U(777), X = U(0), a = U(123456789012ULL), b = U(987654321098ULL), z, g, sum; lvxp_gain(&L, &X, &a, 100, &g); lvxp_gain(&L, &X, &b, 100, &g);
      Lv L2 = U(777), X2 = U(0); lv_add(&sum, &a, &b); lvxp_gain(&L2, &X2, &sum, 100, &g); (void)z; CHECK(lv_cmp(&L, &L2) == 0 && lv_cmp(&X, &X2) == 0); }
    // exactly at the edge of a level: need-1 stays, need goes up
    { Lv L = U(1000), X = U(18000011ULL), z = U(0), g; lvxp_gain(&L, &X, &z, 100, &g); CHECK(lv_cmp_u64(&L, 1000) == 0 && lv_is_zero(&g));
      X = U(18000012ULL); lvxp_gain(&L, &X, &z, 100, &g); CHECK(lv_cmp_u64(&L, 1001) == 0 && lv_is_zero(&X) && lv_cmp_u64(&g, 1) == 0); }
    // the top: nothing goes past 64^256, and the bank is emptied there
    { Lv cap, L, X, z = U(0), g; lv_cap(&cap); L = cap; X = U(5); CHECK(lvxp_gain(&L, &X, &z, 100, &g) == 1 && lv_cmp(&L, &cap) == 0 && lv_is_zero(&X) && lv_is_zero(&g));
      Lv big; lv_sub(&big, &cap, &(Lv){ .w = { 1 } }); L = big; X = cap; CHECK(lvxp_gain(&L, &X, &z, 100, &g) == 1 && lv_cmp(&L, &cap) == 0 && lv_cmp_u64(&g, 1) == 0);         // the last level costs a full bank
      L = big; X = U(12345); CHECK(lvxp_gain(&L, &X, &z, 100, &g) == 0 && lv_cmp(&L, &big) == 0 && lv_is_zero(&g) && lv_cmp_u64(&X, 12345) == 0);   // anything less does not buy it
      L = U(1); X = cap; lvxp_gain(&L, &X, &z, 100, &g); CHECK(lv_cmp(&L, &cap) < 0); CHECK(lv_cmp(&L, &(Lv){ .w = { 1 } }) > 0); }       // 2^1536 xp from level 1 buys a lot, not everything
    // levels beyond 64 bits: pay out many levels in a blink, and the numbers agree with known decimals
    { Lv L = P("9223372036854775807"), X = P("100000000000000000000000000000000000000000"), z = U(0), g; lvxp_gain(&L, &X, &z, 100, &g); lv_to_dec(&L, s, sizeof s); printf("  from S64MAX with 1e41 xp: level %s\n", s); CHECK(lv_cmp_u64(&g, 0) > 0); }
    // ratio for the bar
    { Lv L = U(1000), nd, X; lvxp_need(&L, 100, &nd); X = U(0); CHECK(lvxp_ratio(&X, &nd, 1000) == 0); lv_div_small(&X, &nd, 2); CHECK(lvxp_ratio(&X, &nd, 1000) >= 499 && lvxp_ratio(&X, &nd, 1000) <= 500); CHECK(lvxp_ratio(&nd, &nd, 1000) == 1000);
      Lv huge; lv_pow_u32(&huge, 10, 300); lvxp_need(&huge, 100, &nd); lv_div_small(&X, &nd, 4); unsigned r = lvxp_ratio(&X, &nd, 1000); CHECK(r >= 249 && r <= 250); }
    printf("lvxp_test: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
