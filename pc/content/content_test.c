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

static int cs_first[NCAT], cs_n[NCAT], cs_pick[MAX_TR + 1];
// every valid set of 3..MAX_TR classic traits (at most one per category, no clash), each trait weighted by `mult` flavours: sum of mult^n
static unsigned long long count_sets(int cat, int n, unsigned long long mult){
    unsigned long long t = 0;
    if(cat == NCAT) return n >= 3 ? 1 : 0;
    t += count_sets(cat + 1, n, mult);                            // nothing from this category
    if(n < MAX_TR) for(int ch = 0; ch < cs_n[cat]; ch++){
        int id = cs_first[cat] + ch, ok = 1;
        for(int j = 0; j < n; j++) if(traits_conflict(id, cs_pick[j])){ ok = 0; break; }
        if(!ok) continue;
        cs_pick[n] = id; t += mult * count_sets(cat + 1, n + 1, mult);
    }
    return t;
}
static int cmp_str(const void *a, const void *b){ return strcmp((const char *)a, (const char *)b); }
static const char *okset = " ',():-.^0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static void text_ok(const char *s, int maxlen, const char *what, int id){ CHECK(s[0] && (int)strlen(s) <= maxlen && strspn(s, okset) == strlen(s), "%s %d: bad text '%s'", what, id, s); }
// all strings of a list must be different (sort a copy and compare neighbours)
static int dup_count(char (*names)[96], int n){ qsort(names, n, 96, cmp_str); int d = 0; for(int i = 1; i < n; i++) if(!strcmp(names[i], names[i - 1])) d++; return d; }
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

    // ---- planets: traits are valid, repeatable, and every one of the 1,040 traits shows up
    { static long seen[TR_BASE * TR_VARIANTS]; long N = 1000000, bad = 0, hist[8] = {0}, nat = 0, ppl = 0, cit = 0; PlanetInfo p, q;
      for(long i = 0; i < N; i++){
          uint64_t s = ((uint64_t)r32() << 32) | r32(); planet_make(s, &p); planet_make(s, &q);
          if(memcmp(&p, &q, sizeof p)){ bad++; continue; }
          if(p.n < 3 || p.n > MAX_TR){ bad++; continue; }
          hist[p.n]++;
          for(int a = 0; a < p.n; a++){ if(p.id[a] >= TR_BASE * TR_VARIANTS){ bad++; continue; } seen[p.id[a]]++; for(int b = a + 1; b < p.n; b++) if(traits_conflict(p.id[a], p.id[b])) bad++; }
          if(p.nbeing < 1 || p.nbeing > MAX_NATIVE || p.npeople > MAX_PEOPLE) bad++;
          for(int a = 0; a < p.nbeing; a++){ if(p.being[a] >= N_KIND) bad++; for(int b = a + 1; b < p.nbeing; b++) if(p.being[a] % N_PLAN == p.being[b] % N_PLAN) bad++; }
          for(int a = 0; a < p.npeople; a++) if(p.people[a] >= N_PEOPLE) bad++;
          if(p.city >= N_CITY || (p.city >= 0 && !p.npeople)) bad++;
          if(p.legend_kind != p.being[0]) bad++;
          for(int z = 0; z < N_ZONE; z++){ if(p.stat[0 * N_ZONE + z] < 0 || p.stat[2 * N_ZONE + z] < 0 || p.stat[2 * N_ZONE + z] > 100) bad++; for(int f = 3; f < N_FAM; f++) if(p.stat[f * N_ZONE + z] < -5 || p.stat[f * N_ZONE + z] > 12) bad++; }
          nat += p.nbeing; ppl += p.npeople; cit += p.city >= 0;
      }
      CHECK(!bad, "%ld bad planets of %ld", bad, N);
      int tc = trait_count(), never = 0; long mn = N, mx = 0;
      for(int i = 0; i < tc; i++){ if(!seen[i]) never++; if(seen[i] < mn) mn = seen[i]; if(seen[i] > mx) mx = seen[i]; }
      CHECK(!never, "%d traits never appeared", never);
      printf("planets: %ld made, %d traits, trait counts %ld..%ld, sizes 3:%ld 4:%ld 5:%ld 6:%ld, %ld bad; natives avg %.2f creatures, %.2f peoples, %.1f%% with a city\n", N, tc, mn, mx, hist[3], hist[4], hist[5], hist[6], bad, (double)nat / N, (double)ppl / N, 100.0 * cit / N); }

    // ---- the counts, the names and the texts (every list is built from parts, so these checks scale with the counts)
    { static char names[N_KIND][96]; int d;
      CHECK(trait_count() == 1040 && trait_base_count() == 65 && TR_BASE * TR_VARIANTS == 1040, "traits: %d (want 1040 = 65 x 16)", trait_count());
      CHECK(stat_count() == 112 && N_FAM * N_ZONE == 112, "stats: %d (want 112 = 7 x 16)", stat_count());
      CHECK(people_count() == 512 && city_count() == 256, "peoples %d, cities %d (want 512 and 256)", people_count(), city_count());
      CHECK(creature_plan_count() == 66 && creature_kind_count() == 12672 && N_KIND == 66 * 192, "creature kinds %d (want 66 plans x 192 = 12672)", creature_kind_count());
      CHECK((unsigned long long)N_KIND * N_INDIV_PER_KIND == 415236096ull, "individuals %llu", (unsigned long long)N_KIND * N_INDIV_PER_KIND);
      CHECK(192 == 16 * 12, "192 is 16 x 12");
      for(int i = 0; i < trait_count(); i++){ strcpy(names[i], trait_name(i)); text_ok(names[i], 40, "trait", i); }
      d = dup_count(names, trait_count()); CHECK(!d, "%d duplicate trait names", d);
      for(int i = 0; i < stat_count(); i++){ strcpy(names[i], stat_name(i)); text_ok(names[i], 40, "stat", i); }
      d = dup_count(names, stat_count()); CHECK(!d, "%d duplicate stat names", d);
      for(int i = 0; i < people_count(); i++){ People pe; people_get(i, &pe); strcpy(names[i], pe.name); text_ok(pe.name, 40, "people", i); text_ok(pe.about1, 58, "people about1", i); text_ok(pe.about2, 58, "people about2", i);
          CHECK(pe.might >= 1 && pe.might <= 7 && pe.wit >= 1 && pe.charm >= 1 && pe.craft >= 1 && pe.temper >= 1 && pe.lifespan > 0, "people %d stats", i); }
      d = dup_count(names, people_count()); CHECK(!d, "%d duplicate people names", d);
      for(int i = 0; i < city_count(); i++){ CityKind c; city_get(i, &c); strcpy(names[i], c.name); text_ok(c.name, 40, "city", i); text_ok(c.about1, 58, "city about1", i); text_ok(c.about2, 58, "city about2", i);
          CHECK(c.pop >= 1 && c.pop <= 9 && c.walls >= 0 && c.trade >= 0 && c.wonder >= 0, "city %d stats", i); }
      d = dup_count(names, city_count()); CHECK(!d, "%d duplicate city names", d);
      { int rar[6] = {0}; for(int i = 0; i < creature_kind_count(); i++){ Creature c, c2; creature_kind(i, &c); creature_kind(i, &c2);
            CHECK(!memcmp(&c, &c2, sizeof c), "creature kind %d not repeatable", i);
            strcpy(names[i], c.name); text_ok(c.name, 60, "creature", i); text_ok(c.about1, 58, "creature about1", i); text_ok(c.about2, 58, "creature about2", i);
            for(int s = 0; s < N_CSTAT; s++) CHECK(c.stat[s] >= 0, "creature %d stat %d", i, s);
            CHECK(c.stat[0] >= 1 && c.stat[1] >= 1 && c.stat[2] >= 1, "creature %d stats are not positive", i);
            rar[c.rarity]++; }
        d = dup_count(names, creature_kind_count()); CHECK(!d, "%d duplicate creature kind names", d);
        printf("creature kinds by rarity (kinds): C%d U%d R%d E%d L%d M%d\n", rar[0], rar[1], rar[2], rar[3], rar[4], rar[5]); }
      // individuals: 200,000 random ones; the same four numbers always make the same name and the same name always means the same four numbers
      { static char (*iname)[168]; static Indiv iv[200000]; int n = 200000, dd = 0, rr[6] = {0};
        iname = malloc((size_t)n * 168);
        for(int i = 0; i < n; i++){ CreatureInd a, b; indiv_from_seed(((uint64_t)r32() << 32) | r32(), &iv[i]); creature_indiv(&iv[i], &a); creature_indiv(&iv[i], &b);
            CHECK(!memcmp(&a, &b, sizeof a), "individual %d not repeatable", i); snprintf(iname[i], 168, "%s", a.name); text_ok(a.name, 150, "individual", i); text_ok(a.about3, 70, "individual about3", i); rr[a.rarity]++;
            for(int s = 0; s < N_CSTAT; s++) CHECK(a.stat[s] >= 0, "individual %d stat %d", i, s); }
        // sort indices by name and check equal names have equal individuals
        { int *ix = malloc(sizeof(int) * n); for(int i = 0; i < n; i++) ix[i] = i;
          for(int gap = n / 2; gap > 0; gap /= 2) for(int i = gap; i < n; i++){ int t = ix[i], j = i; while(j >= gap && strcmp(iname[ix[j - gap]], iname[t]) > 0){ ix[j] = ix[j - gap]; j -= gap; } ix[j] = t; }
          for(int i = 1; i < n; i++) if(!strcmp(iname[ix[i]], iname[ix[i - 1]]) && memcmp(&iv[ix[i]], &iv[ix[i - 1]], sizeof(Indiv))) dd++;
          free(ix); }
        CHECK(!dd, "%d different individuals share a name", dd);
        printf("individuals: %d sampled, names clash %d, by rarity C%d U%d R%d E%d L%d M%d\n", n, dd, rr[0], rr[1], rr[2], rr[3], rr[4], rr[5]);
        free(iname); }
      // the forbidden-pair rules sit on the classic traits: a flavour never changes them
      { long diff = 0, forb = 0; for(int a = 0; a < trait_count(); a += 7) for(int b = 0; b < trait_count(); b++){ int c1 = traits_conflict(a, b), c0 = traits_conflict(a % TR_BASE, b % TR_BASE); if(c1 != c0) diff++; }
        for(int a = 0; a < TR_BASE; a++) for(int b = a + 1; b < TR_BASE; b++) if(traits_conflict(a, b) && trait_cat(a) != trait_cat(b)) forb++;
        CHECK(!diff, "%ld flavoured pairs differ from the classic rule", diff); CHECK(forb == 31, "forbidden pairs across categories: %ld (want 31)", forb);
        printf("forbidden pairs: %ld across categories (classic rule), flavours change none\n", forb); }
      printf("counts: %d traits (%d x %d), %d stats (%d x %d), %d peoples (%d x %d), %d cities (%d x %d), %d creature kinds (%d x %d), %llu individuals\n",
             trait_count(), TR_BASE, TR_VARIANTS, stat_count(), N_FAM, N_ZONE, people_count(), N_STOCK, N_WAY, city_count(), N_FORM, N_CHAR, creature_kind_count(), N_PLAN, N_VARIANT, (unsigned long long)N_KIND * N_INDIV_PER_KIND); }

    // ---- universe themes
    { static long seen[64]; long N = 200000, bad = 0; int out[2];
      for(long i = 0; i < N; i++){ uint32_t uni[UNI_WORDS]; for(int k = 0; k < UNI_WORDS; k++) uni[k] = r32(); universe_themes(uni_seed(uni), out);
          if(out[0] == out[1]) bad++;
          seen[out[0]]++; seen[out[1]]++; }
      int never = 0; for(int i = 0; i < theme_count(); i++) if(!seen[i]) never++;
      CHECK(!bad && !never, "themes: %ld equal pairs, %d never picked", bad, never);
      printf("themes: %d kinds, %ld universes, %ld bad\n", theme_count(), N, bad); }

    // ---- the numbers on the HOW BIG IS IT? pages (pc/facts/09_pc.py and 10_pc16.py): counted here so the pages stay true
    { unsigned long long cnt, cnt16;
      for(int c = 0; c < NCAT; c++){ cs_first[c] = -1; cs_n[c] = 0; }
      for(int i = 0; i < trait_base_count(); i++){ int c = trait_cat(i); if(cs_first[c] < 0) cs_first[c] = i; cs_n[c]++; }
      // every valid set of 3 to MAX_TR traits, at most one per category, no clashing pair; with 16 flavours each trait counts 16 times
      cnt = count_sets(0, 0, 1); cnt16 = count_sets(0, 0, TR_VARIANTS);
      CHECK(cnt == 10780270ull, "classic trait recipes %llu, the HOW BIG page says 10780270", cnt);
      CHECK(cnt16 == 144586870865920ull, "flavoured trait recipes %llu, the HOW BIG page says 144586870865920", cnt16);
      int tn = theme_count(), pairs = 0; for(int a = 0; a < tn; a++) for(int b = a + 1; b < tn; b++) if(theme_pair_ok(a, b)) pairs++;
      CHECK(pairs == 266, "valid theme pairs %d, the HOW BIG page says 266", pairs);
      CHECK(trait_count() == 1040 && tn == 24, "counts: %d traits, %d themes (the HOW BIG pages say 1040 and 24)", trait_count(), tn);
      printf("how-big pages: %llu classic trait recipes, %llu flavoured recipes, %d theme pairs, %d traits, %d themes\n", cnt, cnt16, pairs, trait_count(), tn); }

    printf(fails ? "FAILED (%d)\n" : "ALL OK\n", fails);
    return fails != 0;
}
