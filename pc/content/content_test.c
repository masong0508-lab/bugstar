// content_test.c: checks for bignum.h and pc_content.c. Build and run with ./test.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bignum.h"
#include "pc_content.h"

static int fails;
#define CHECK(c, ...) do{ if(!(c)){ printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); fails++; } }while(0)
static uint64_t rs = 88172645463325252ull;
static uint32_t r32(void){ rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 8); }
static void sci_is(const Big *b, const char *want, const char *what){ char s[32]; bn_sci(b, s); CHECK(!strcmp(s, want), "%s: got %s, want %s", what, s, want); }

int main(void){
    // ---- the scale numbers in pc/README.md (8x bits)
    Big u, ax, cl, t, w, tmp;
    bn_pow2(&u, 2048);               sci_is(&u, "3.232E+616", "universes (2^2048)");
    bn_pow2(&cl, 2480);              sci_is(&cl, "3.584E+746", "clusters per universe (2^2480)");
    bn_pow2(&t, 2440);               sci_is(&t, "3.260E+734", "galaxies per universe");
    t = cl; bn_mul_u32(&t, 32);      sci_is(&t, "1.147E+748", "stars per universe");
    bn_mul_u32(&t, 16);              sci_is(&t, "1.835E+749", "planets per universe (max)");
    bn_mul(&tmp, &t, &u);            sci_is(&tmp, "5.930E+1365", "planets, all universes");
    bn_pow2(&ax, 1240); w = ax; bn_mul_u32(&w, 750);   sci_is(&w, "1.420E+376", "width of one universe (ly)");
    bn_mul(&tmp, &u, &u); bn_mul(&tmp, &tmp, &w);      sci_is(&tmp, "1.483E+1609", "trip to the neighbour universe (ly)");
    { Big a, b; bn_set(&a, 0xFFFFFFFFu); bn_set(&b, 1); bn_add(&a, &b); CHECK(a.w[0] == 0 && a.w[1] == 1, "carry"); }
    { Big a; bn_pow2(&a, 100); CHECK(bn_bits(&a) == 101, "bits of 2^100 = %d", bn_bits(&a)); char d[64]; bn_to_dec(&a, d, sizeof d); CHECK(!strcmp(d, "1267650600228229401496703205376"), "2^100 decimal %s", d); }
    printf("bignum: %s\n", fails ? "FAILED" : "ok");

    // ---- the address hashes every word
    { Addr a; memset(&a, 0, sizeof a); for(int i = 0; i < UNI_WORDS; i++) a.uni[i] = r32(); for(int i = 0; i < AX_WORDS; i++){ a.cx[i] = r32(); a.cy[i] = r32(); }
      uint64_t base = addr_seed(&a); long bits = 0, n = 0; int stuck = 0;
      for(int i = 0; i < UNI_WORDS + 2 * AX_WORDS; i++) for(int b = 0; b < 32; b++){
          Addr c = a; uint32_t *wp = i < UNI_WORDS ? &c.uni[i] : i < UNI_WORDS + AX_WORDS ? &c.cx[i - UNI_WORDS] : &c.cy[i - UNI_WORDS - AX_WORDS];
          *wp ^= 1u << b; uint64_t d = addr_seed(&c) ^ base; if(!d) stuck++; bits += __builtin_popcountll(d); n++; }
      CHECK(!stuck, "%d single-bit changes did not change the seed", stuck);
      CHECK(bits / n > 28 && bits / n < 36, "avalanche average %ld bits of 64", bits / n);
      printf("address: %ld bit flips over %d words, average %ld/64 bits changed, %d stuck\n", n, UNI_WORDS + 2 * AX_WORDS, bits / n, stuck); }

    // ---- planets: traits are valid, repeatable, and every trait shows up
    { static long seen[128]; long N = 1000000, bad = 0, hist[8] = {0}; PlanetInfo p, q;
      for(long i = 0; i < N; i++){
          uint64_t s = ((uint64_t)r32() << 32) | r32(); planet_make(s, &p); planet_make(s, &q);
          if(memcmp(&p, &q, sizeof p)){ bad++; continue; }
          if(p.n < 3 || p.n > MAX_TR){ bad++; continue; }
          hist[p.n]++;
          for(int a = 0; a < p.n; a++){ seen[p.id[a]]++; for(int b = a + 1; b < p.n; b++) if(traits_conflict(p.id[a], p.id[b])) bad++; }
      }
      CHECK(!bad, "%ld bad planets of %ld", bad, N);
      int tc = trait_count(), never = 0; long mn = N, mx = 0;
      for(int i = 0; i < tc; i++){ if(!seen[i]) never++; if(seen[i] < mn) mn = seen[i]; if(seen[i] > mx) mx = seen[i]; }
      CHECK(!never, "%d traits never appeared", never);
      printf("planets: %ld made, %d traits, trait counts %ld..%ld, sizes 3:%ld 4:%ld 5:%ld 6:%ld, %ld bad\n", N, tc, mn, mx, hist[3], hist[4], hist[5], hist[6], bad); }

    // ---- universe themes
    { static long seen[64]; long N = 200000, bad = 0; int out[2];
      for(long i = 0; i < N; i++){ uint32_t uni[UNI_WORDS]; for(int k = 0; k < UNI_WORDS; k++) uni[k] = r32(); universe_themes(uni_seed(uni), out);
          if(out[0] == out[1]) bad++;
          seen[out[0]]++; seen[out[1]]++; }
      int never = 0; for(int i = 0; i < theme_count(); i++) if(!seen[i]) never++;
      CHECK(!bad && !never, "themes: %ld equal pairs, %d never picked", bad, never);
      printf("themes: %d kinds, %ld universes, %ld bad\n", theme_count(), N, bad); }

    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
