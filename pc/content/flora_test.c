// flora_test.c: tests for the flora catalogue. Build: gcc -O2 -I. flora_test.c flora.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flora.h"
static int bad;
#define CHECK(c, ...) do{ if(!(c)){ bad++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } }while(0)
static int cmp(const void *a, const void *b){ return strcmp((const char*)a, (const char*)b); }
static int fontsafe(const char *s){ for(; *s; s++){ char c = *s; if(!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == ',' || c == '\'' || c == ':' || c == '(' || c == ')' || c == '-')) return 0; } return 1; }
int main(void){
    // 1. the catalogue is a bijection: 4,000,000 entries of one seed hit 4,000,000 different plants
    { static uint8_t seen[FL_TOTAL / 8]; int dup = 0; const uint32_t N = 4000000;
      for(uint32_t n = 0; n < N; n++){ uint32_t id = flora_catalogue(0xB16B00B5ull, n); if(id >= FL_TOTAL || (seen[id >> 3] >> (id & 7) & 1)) dup++; seen[id >> 3] |= 1 << (id & 7); }
      CHECK(!dup, "catalogue repeats a plant %d times", dup); printf("catalogue: %u entries, %d repeats\n", N, dup); }
    // 2. names are unique in a big sample (three seeds x 400,000 plants), exactly (sorted and compared)
    { const int per = 400000, seeds = 3; char (*nm)[40] = malloc((size_t)per * seeds * 40); int k = 0; Flora f;
      for(int s = 0; s < seeds; s++) for(int n = 0; n < per; n++){ flora_make(flora_catalogue(1000 + s * 7919, n), &f); strcpy(nm[k++], f.name); }
      // names of the same plant repeat between seeds (a plant is the same on every planet), so test each seed on its own
      int dups = 0; for(int s = 0; s < seeds; s++){ qsort(nm + s * per, per, 40, cmp); for(int n = 1; n < per; n++) if(!strcmp(nm[s * per + n], nm[s * per + n - 1])) dups++; }
      CHECK(!dups, "%d duplicate names", dups); printf("names: %d plants per seed, %d duplicate names\n", per, dups); free(nm); }
    // 3. name -> id -> name round trip, and a name finds its plant
    { int ok = 0, n = 200000; for(int i = 0; i < n; i++){ Flora f, g; uint32_t id = (uint32_t)(((uint64_t)i * 2654435761ull) & (FL_TOTAL - 1)); flora_make(id, &f);
        uint32_t back = flora_find(f.name); flora_make(back, &g); if(back == id && !strcmp(f.name, g.name)) ok++; }
      CHECK(ok == n, "find round trip %d of %d", ok, n); printf("find: %d of %d names found their plant\n", ok, n); }
    // 4. font-safe text, lengths, every trait value shows up
    { static uint32_t cnt[10][32]; int unsafe = 0, longest = 0; Flora f; char ab[400]; int maxab = 0;
      for(uint32_t n = 0; n < 300000; n++){ flora_make(flora_catalogue(5, n), &f); flora_about(&f, ab, sizeof ab);
        if(!fontsafe(f.name) || !fontsafe(ab)) unsafe++;
        if((int)strlen(f.name) > longest) longest = (int)strlen(f.name); if((int)strlen(ab) > maxab) maxab = (int)strlen(ab);
        cnt[0][f.form]++; cnt[1][f.leaf]++; cnt[2][f.flower]++; cnt[3][f.fruit]++; cnt[4][f.bark]++; cnt[5][f.colour]++; cnt[6][f.habitat]++; cnt[7][f.size]++; cnt[8][f.use]++; cnt[9][f.quirk]++;
        CHECK(f.o2 <= 15 && f.food <= 15 && f.shelter <= 15 && f.medicine <= 15 && f.fuel <= 15, "value out of range"); }
      CHECK(!unsafe, "%d texts use letters the font lacks", unsafe); CHECK(longest <= 30, "name too long %d", longest);
      static const int lim[10] = {FL_NFORM, FL_NLEAF, FL_NFLOWER, FL_NFRUIT, FL_NBARK, FL_NCOLOUR, FL_NHAB, FL_NSIZE, FL_NUSE, FL_NQUIRK};
      for(int d = 0; d < 10; d++) for(int v = 0; v < lim[d]; v++) CHECK(cnt[d][v] > 0, "trait %d value %d never appears", d, v);
      printf("text: font-safe, longest name %d, longest about %d; every trait value appears\n", longest, maxab); }
    // 5. determinism: the same plant twice is byte for byte the same
    { Flora a, b; int same = 1; for(uint32_t n = 0; n < 50000; n++){ flora_make(flora_catalogue(77, n), &a); flora_make(flora_catalogue(77, n), &b); if(memcmp(&a, &b, sizeof a)) same = 0; }
      CHECK(same, "flora_make is not deterministic"); printf("determinism: same\n"); }
    // 6. a planet gets the plants it can grow
    { FloraEnv envs[4] = { {15, 55, 60, 1, 100}, {-30, 40, 20, 2, 100}, {45, 10, 0, 1, 100}, {20, 70, 80, 0, 10} }; Flora out[400];
      for(int e = 0; e < 4; e++){ int g = flora_for_planet(123 + e, &envs[e], 300, out); uint32_t m = flora_habitats(&envs[e]); int wrong = 0;
        for(int i = 0; i < g; i++) if(!(m >> out[i].habitat & 1)) wrong++;
        CHECK(g == 300 && !wrong, "planet %d: got %d plants, %d in the wrong place", e, g, wrong);
        int g2 = flora_for_planet(123 + e, &envs[e], 300, out + 0); CHECK(g2 == g, "planet pick changes"); printf("planet climate %d: %d plants, habitat mask %04X\n", e, g, m); } }
    printf("kinds by traits: %llu, named plants: %u\n", (unsigned long long)FL_KINDS, FL_TOTAL);
    printf(bad ? "FLORA TESTS FAILED (%d)\n" : "FLORA TESTS PASSED\n", bad);
    return bad != 0;
}
