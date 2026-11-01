// lv_test.c: tests for Lv (lv.c). Build + run: pc/sim/test.sh
//   lv_test                 the built-in checks
//   lv_test --batch         reads lines "op a b" (decimal numbers) from stdin, prints "result sat" (used by test.sh against Python's big integers)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv.h"

static int fails, checks;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static void D(const Lv *a, char *b){ lv_to_dec(a, b, LV_DEC_MAX); }
static Lv P(const char *s){ Lv r; lv_from_dec(&r, s); return r; }
static Lv Uf(uint64_t v){ Lv r; lv_from_u64(&r, v); return r; }
#define U(v) (Uf(v))
#define UP(v) (&(Lv){ .w = { (v) } })
static int is_dec(const Lv *a, const char *s){ char b[LV_DEC_MAX]; D(a, b); return !strcmp(b, s); }

static int batch(void){
    char op[16], a[LV_DEC_MAX + 8], b[LV_DEC_MAX + 8];
    while(scanf("%15s %s %s", op, a, b) == 3){
        Lv x = P(a), y = P(b), r; int sat = 0; char out[LV_DEC_MAX];
        if(!strcmp(op, "add")) sat = lv_add(&r, &x, &y);
        else if(!strcmp(op, "sub")) sat = lv_sub(&r, &x, &y);
        else if(!strcmp(op, "mul")) sat = lv_mul(&r, &x, &y);
        else if(!strcmp(op, "muls")) sat = lv_mul_small(&r, &x, (uint32_t)strtoul(b, 0, 10));
        else if(!strcmp(op, "divs")){ uint32_t m = lv_div_small(&r, &x, (uint32_t)strtoul(b, 0, 10)); sprintf(out, "%u", m); D(&r, a); printf("%s %s\n", a, out); continue; }
        else if(!strcmp(op, "cmp")){ printf("%d 0\n", lv_cmp(&x, &y)); continue; }
        else if(!strcmp(op, "shl")) sat = lv_shl(&r, &x, (unsigned)strtoul(b, 0, 10));
        else if(!strcmp(op, "shr")){ lv_shr(&r, &x, (unsigned)strtoul(b, 0, 10)); }
        else if(!strcmp(op, "b64")){ char t[2048]; lv_to_b64(&x, t, sizeof t); Lv z; lv_from_b64(&z, t); D(&z, out); printf("%s 0\n", out); continue; }
        else if(!strcmp(op, "compact")){ char t[64]; lv_compact(&x, t, (size_t)strtoul(b, 0, 10)); printf("%s 0\n", t); continue; }
        else { printf("?\n"); continue; }
        D(&r, out); printf("%s %d\n", out, sat);
    }
    return 0;
}

int main(int argc, char **argv){
    if(argc > 1 && !strcmp(argv[1], "--batch")) return batch();
    char b[LV_DEC_MAX + 8]; Lv a, c, cap, one = U(1), zero; lv_zero(&zero); lv_cap(&cap);

    // --- basics
    CHECK(lv_is_zero(&zero)); CHECK(!lv_is_zero(&one)); CHECK(lv_cmp(&zero, &one) < 0); CHECK(lv_cmp(&one, &zero) > 0); CHECK(lv_cmp(&one, &one) == 0);
    CHECK(sizeof(Lv) == 200); CHECK(LV_LIMBS * 64 >= LV_CAP_BITS + 1);
    CHECK(lv_bits(&zero) == 0); CHECK(lv_bits(&one) == 1); CHECK(lv_bits(&cap) == 1537);
    a = U(UINT64_MAX); CHECK(lv_bits(&a) == 64); CHECK(lv_fits_u64(&a)); CHECK(lv_to_u64(&a) == UINT64_MAX);
    lv_add_u64(&c, &a, 1); CHECK(!lv_fits_u64(&c)); CHECK(is_dec(&c, "18446744073709551616")); CHECK(lv_bits(&c) == 65);
    CHECK(lv_cmp_u64(&c, UINT64_MAX) > 0); CHECK(lv_cmp_u64(&a, UINT64_MAX) == 0); CHECK(lv_cmp_u64(&zero, 0) == 0); CHECK(lv_cmp_u64(&one, 2) < 0);

    // --- the cap is exactly 64^256 = 2^1536
    Lv p; int sat = lv_pow_u32(&p, 64, 256); CHECK(!sat); CHECK(lv_cmp(&p, &cap) == 0);
    sat = lv_pow_u32(&p, 2, 1536); CHECK(!sat); CHECK(lv_cmp(&p, &cap) == 0);
    sat = lv_pow_u32(&p, 64, 255); CHECK(!sat); CHECK(lv_cmp(&p, &cap) < 0); CHECK(lv_bits(&p) == 1531);
    sat = lv_pow_u32(&p, 64, 257); CHECK(sat); CHECK(lv_cmp(&p, &cap) == 0);
    sat = lv_pow_u32(&p, 2, 1537); CHECK(sat); CHECK(lv_cmp(&p, &cap) == 0);
    sat = lv_pow_u32(&p, 10, 462); CHECK(!sat); CHECK(lv_cmp(&p, &cap) < 0);      // 10^462 < 2^1536 < 10^463
    sat = lv_pow_u32(&p, 10, 463); CHECK(sat);
    lv_to_dec(&cap, b, sizeof b); CHECK(strlen(b) == 463); CHECK(!strcmp(b, "2410312426921032588580116606028314112912093247945688951359675039065257391591803200669085024107346049663448766280888004787862416978794958324969612987890774651455213339381625224770782077917681499676845543137387820057597345857904599109461387122099507964997815641342300677629473355281617428411794163967785870370368969109221591943054232011562758450080579587850900993714892283476646631181515063804873375182260506246992837898705971012525843324401232986857004760339316736"));
    CHECK(b[462] == '6');
    { Lv back = P(b); CHECK(lv_cmp(&back, &cap) == 0); CHECK(lv_from_dec(&back, b) == 0); }

    // saturating add: cap + 1, cap + cap, (cap-1) + 1 exact, (cap-1) + 2 saturated
    Lv capm1; lv_sub(&capm1, &cap, &one);
    CHECK(lv_add(&c, &cap, &one) == 1 && lv_cmp(&c, &cap) == 0);
    CHECK(lv_add(&c, &cap, &cap) == 1 && lv_cmp(&c, &cap) == 0);
    CHECK(lv_add(&c, &capm1, &one) == 0 && lv_cmp(&c, &cap) == 0);          // landing exactly on the cap is not a saturation
    CHECK(lv_add(&c, &capm1, UP(2)) == 1 && lv_cmp(&c, &cap) == 0);
    CHECK(lv_add(&c, &capm1, &zero) == 0 && lv_cmp(&c, &capm1) == 0);
    CHECK(lv_is_cap(&cap)); CHECK(!lv_is_cap(&capm1));
    lv_add_u64(&c, &capm1, UINT64_MAX); CHECK(lv_cmp(&c, &cap) == 0);

    // saturating multiply
    CHECK(lv_mul(&c, &cap, UP(2)) == 1 && lv_cmp(&c, &cap) == 0);
    CHECK(lv_mul(&c, &cap, &cap) == 1 && lv_cmp(&c, &cap) == 0);
    CHECK(lv_mul(&c, &cap, &zero) == 0 && lv_is_zero(&c));
    CHECK(lv_mul(&c, &cap, &one) == 0 && lv_cmp(&c, &cap) == 0);
    { Lv h; lv_pow_u32(&h, 2, 768); Lv h2 = h; CHECK(lv_mul(&c, &h, &h2) == 0 && lv_cmp(&c, &cap) == 0);          // 2^768 * 2^768 = cap exactly
      Lv h3; lv_pow_u32(&h3, 2, 769); CHECK(lv_mul(&c, &h3, &h2) == 1 && lv_cmp(&c, &cap) == 0);
      Lv h4; lv_pow_u32(&h4, 2, 767); CHECK(lv_mul(&c, &h4, &h2) == 0 && lv_cmp(&c, &cap) < 0);
      Lv m3 = U(3); CHECK(lv_mul(&c, &h, &m3) == 0 && lv_cmp(&c, &cap) < 0); }
    CHECK(lv_mul_small(&c, &cap, 2) == 1 && lv_cmp(&c, &cap) == 0);
    CHECK(lv_mul_small(&c, &capm1, 1) == 0 && lv_cmp(&c, &capm1) == 0);
    { Lv half; lv_pow_u32(&half, 2, 1535); CHECK(lv_mul_small(&c, &half, 2) == 0 && lv_cmp(&c, &cap) == 0); CHECK(lv_mul_small(&c, &half, 3) == 1 && lv_cmp(&c, &cap) == 0); }
    { Lv x = U(UINT64_MAX); lv_mul(&c, &x, &x); CHECK(is_dec(&c, "340282366920938463426481119284349108225")); }   // (2^64-1)^2
    // 64^255 * 64 lands on the cap, *65 saturates
    { Lv q; lv_pow_u32(&q, 64, 255); CHECK(lv_mul_small(&c, &q, 64) == 0 && lv_cmp(&c, &cap) == 0); CHECK(lv_mul_small(&c, &q, 65) == 1 && lv_cmp(&c, &cap) == 0); }

    // subtraction
    CHECK(lv_sub(&c, &one, UP(2)) == 1 && lv_is_zero(&c));
    CHECK(lv_sub(&c, UP(5), UP(5)) == 0 && lv_is_zero(&c));
    CHECK(lv_sub(&c, &cap, &cap) == 0 && lv_is_zero(&c));
    CHECK(lv_sub(&c, &cap, &one) == 0 && lv_cmp(&c, &capm1) == 0);
    { Lv x; lv_pow_u32(&x, 2, 64); lv_sub(&c, &x, &one); CHECK(lv_to_u64(&c) == UINT64_MAX && lv_fits_u64(&c)); }

    // division by a small number
    { uint32_t r = lv_div_small(&c, &cap, 64); CHECK(r == 0); Lv q255; lv_pow_u32(&q255, 64, 255); CHECK(lv_cmp(&c, &q255) == 0);
      r = lv_div_small(&c, &cap, 3); CHECK(r == 1);      // 2^1536 mod 3 = 1 (even power of 2)
      Lv back; lv_mul_small(&back, &c, 3); lv_add_u64(&back, &back, r); CHECK(lv_cmp(&back, &cap) == 0);
      r = lv_div_small(&c, &cap, 4294967295u); Lv back2; lv_mul_small(&back2, &c, 4294967295u); lv_add_u64(&back2, &back2, r); CHECK(lv_cmp(&back2, &cap) == 0); CHECK(r < 4294967295u);
      r = lv_div_small(&c, UP(1000), 7); CHECK(r == 6 && lv_to_u64(&c) == 142);
      r = lv_div_small(&c, &zero, 5); CHECK(r == 0 && lv_is_zero(&c));
      r = lv_div_small(&c, UP(UINT64_MAX), 10); CHECK(r == 5 && lv_to_u64(&c) == 1844674407370955161ULL); }

    // shifts
    { Lv x; lv_shl(&x, &one, 1536); CHECK(lv_cmp(&x, &cap) == 0); CHECK(lv_shl(&x, &one, 1537) == 1 && lv_cmp(&x, &cap) == 0);
      lv_shl(&x, &one, 70); lv_shr(&c, &x, 70); CHECK(lv_cmp(&c, &one) == 0); lv_shr(&c, &cap, 1536); CHECK(lv_cmp(&c, &one) == 0);
      lv_shr(&c, &cap, 1537); CHECK(lv_is_zero(&c)); CHECK(lv_shl(&x, &zero, 5000) == 0 && lv_is_zero(&x)); }

    // decimal known values
    { Lv x; lv_pow_u32(&x, 2, 64); CHECK(is_dec(&x, "18446744073709551616")); lv_pow_u32(&x, 2, 128); CHECK(is_dec(&x, "340282366920938463463374607431768211456"));
      lv_pow_u32(&x, 10, 100); lv_to_dec(&x, b, sizeof b); CHECK(strlen(b) == 101 && b[0] == '1' && strspn(b + 1, "0") == 100);
      lv_pow_u32(&x, 2, 256); CHECK(is_dec(&x, "115792089237316195423570985008687907853269984665640564039457584007913129639936"));
      lv_pow_u32(&x, 64, 4); CHECK(is_dec(&x, "16777216")); lv_pow_u32(&x, 64, 10); CHECK(is_dec(&x, "1152921504606846976"));
      CHECK(is_dec(&zero, "0")); CHECK(is_dec(&one, "1")); a = U(1000000000); CHECK(is_dec(&a, "1000000000")); a = U(999999999); CHECK(is_dec(&a, "999999999"));
      a = P("1,000,000,000,000"); CHECK(is_dec(&a, "1000000000000")); a = P("0000012"); CHECK(is_dec(&a, "12"));
      a = P("123456789012345678901234567890"); CHECK(is_dec(&a, "123456789012345678901234567890"));
      CHECK(lv_from_dec(&a, "") == -1 && lv_is_zero(&a)); CHECK(lv_from_dec(&a, "12x") == -1); CHECK(lv_from_dec(&a, "-5") == -1);
      CHECK(lv_from_dec(&a, "99999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999") == 1);
      CHECK(lv_cmp(&a, &cap) == 0); }
    // exactly the cap text: ok, one more: saturated
    { char t[LV_DEC_MAX + 8]; lv_to_dec(&cap, t, sizeof t); CHECK(lv_from_dec(&a, t) == 0 && lv_cmp(&a, &cap) == 0);
      t[strlen(t) - 1] = '7'; CHECK(lv_from_dec(&a, t) == 1 && lv_cmp(&a, &cap) == 0);          // the cap's last digit is 6: 7 is one past it
    }

    // base 64
    { uint8_t d[LV_B64_MAX + 1]; int n = lv_b64_digits(&cap, d); CHECK(n == 257); CHECK(d[0] == 1); int z = 1; for(int i = 1; i < n; i++) if(d[i]) z = 0; CHECK(z);
      n = lv_b64_digits(&capm1, d); CHECK(n == 256); z = 1; for(int i = 0; i < n; i++) if(d[i] != 63) z = 0; CHECK(z);
      n = lv_b64_digits(&zero, d); CHECK(n == 1 && d[0] == 0);
      a = U(64); n = lv_b64_digits(&a, d); CHECK(n == 2 && d[0] == 1 && d[1] == 0);
      a = U(63); n = lv_b64_digits(&a, d); CHECK(n == 1 && d[0] == 63);
      a = U(64ULL * 64 * 7 + 64 * 5 + 3); n = lv_b64_digits(&a, d); CHECK(n == 3 && d[0] == 7 && d[1] == 5 && d[2] == 3);
      char t[2048]; a = U(64ULL * 64 * 7 + 64 * 5 + 3); lv_to_b64(&a, t, sizeof t); CHECK(!strcmp(t, "7.5.3"));
      lv_to_b64(&zero, t, sizeof t); CHECK(!strcmp(t, "0")); lv_to_b64(&cap, t, sizeof t); CHECK(!strncmp(t, "1.0.0.0", 7)); CHECK(strlen(t) == 1 + 256 * 2);
      lv_from_b64(&a, "7.5.3"); CHECK(lv_to_u64(&a) == 64ULL * 64 * 7 + 64 * 5 + 3);
      lv_to_b64(&cap, t, sizeof t); CHECK(lv_from_b64(&a, t) == 0 && lv_cmp(&a, &cap) == 0);
      lv_to_b64(&capm1, t, sizeof t); CHECK(lv_from_b64(&a, t) == 0 && lv_cmp(&a, &capm1) == 0);
      CHECK(lv_from_b64(&a, "64") == -1); CHECK(lv_from_b64(&a, "1..2") == -1); CHECK(lv_from_b64(&a, "") == -1); CHECK(lv_from_b64(&a, "x") == -1);
      CHECK(lv_from_b64(&a, "2.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0") == 1); CHECK(lv_cmp(&a, &cap) == 0);
      // a round trip through every power of 64
      for(int e = 0; e <= 256; e++){ Lv q, r2; lv_pow_u32(&q, 64, e); lv_to_b64(&q, t, sizeof t); CHECK(lv_from_b64(&r2, t) == 0 && lv_cmp(&q, &r2) == 0);
          lv_to_dec(&q, b, sizeof b); CHECK(lv_from_dec(&r2, b) == 0 && lv_cmp(&q, &r2) == 0); lv_b64_digits(&q, d); CHECK(lv_b64_digits(&q, d) == e + 1); } }

    // compact display
    { char t[40]; a = U(0); lv_compact(&a, t, 8); CHECK(!strcmp(t, "0")); a = U(12345); lv_compact(&a, t, 8); CHECK(!strcmp(t, "12345"));
      a = U(12345678); lv_compact(&a, t, 8); CHECK(!strcmp(t, "12345678")); a = U(123456789); lv_compact(&a, t, 8); CHECK(!strcmp(t, "1.2345E8"));
      lv_compact(&cap, t, 8); CHECK(!strcmp(t, "2.41E462")); CHECK(strlen(t) <= 8);
      lv_compact(&cap, t, 6); CHECK(strlen(t) <= 6); CHECK(!strcmp(t, "2E462") || strlen(t) <= 6);
      lv_compact(&cap, t, 5); CHECK(strlen(t) <= 5);
      for(int e = 0; e <= 256; e++){ Lv q; lv_pow_u32(&q, 64, e); for(size_t m = 5; m <= 12; m++){ lv_compact(&q, t, m); CHECK(strlen(t) <= m); } }
      a = P("1000000000000000000000"); lv_compact(&a, t, 8); CHECK(!strcmp(t, "1.000E21")); }

    // bytes (the .sim save format)
    { uint8_t by[LV_BYTES]; Lv x; lv_pow_u32(&x, 3, 900); lv_to_bytes(&x, by); Lv y; lv_from_bytes(&y, by); CHECK(lv_cmp(&x, &y) == 0);
      lv_to_bytes(&cap, by); lv_from_bytes(&y, by); CHECK(lv_cmp(&y, &cap) == 0);
      memset(by, 0xFF, sizeof by); lv_from_bytes(&y, by); CHECK(lv_cmp(&y, &cap) == 0); }       // damaged data cannot go past the cap

    // a long chain: 100000 additions of a growing step, saturates and stays put
    { Lv x = U(1); int hit = 0; for(int i = 0; i < 100000 && !hit; i++){ lv_mul_small(&x, &x, 3); hit |= lv_is_cap(&x); } CHECK(hit); lv_add(&c, &x, &x); CHECK(lv_cmp(&c, &cap) == 0); }

    printf("lv_test: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
