// cr_test.c: tests for the planet creator (recipe, seed, share code, files, undo, footprint). Build: see test.sh
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cr_recipe.h"
#include "cr_bio.h"
#include "cr_addr.h"
#include "../content/flora.h"
static int bad;
#define CHECK(c, ...) do{ if(!(c)){ bad++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } }while(0)
static int fontsafe(const char *s){ for(; *s; s++){ char c = *s; if(!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '.' || c == ',' || c == '\'' || c == ':' || c == '(' || c == ')' || c == '-')) return 0; } return 1; }
static uint64_t fnv(const void *d, size_t n, uint64_t h){ const uint8_t *p = d; while(n--){ h ^= *p++; h *= 1099511628211ull; } return h; }
static int cmpu64(const void *a, const void *b){ uint64_t x = *(const uint64_t*)a, y = *(const uint64_t*)b; return x < y ? -1 : x > y; }

int main(void){
    uint64_t rng = 0xC0FFEEull; Recipe r;
    // ---- 1. recipe -> seed
    { cr_default(&r, 0); uint64_t s1 = cr_seed(&r), s2 = cr_seed(&r); CHECK(s1 == s2, "seed changes between calls");
      Recipe c = r; CHECK(cr_seed(&c) == s1, "a copy has another seed");
      int n = 0, same = 0;
#define FLD(f) do{ Recipe t = r; t.f = (__typeof__(t.f))(t.f + 1); cr_fix(&t); n++; if(cr_seed(&t) == s1) { same++; printf("  field %s does not change the seed\n", #f); } }while(0)
      FLD(cls); FLD(size); FLD(sea); FLD(land); FLD(rough); FLD(hour); FLD(weather); FLD(sky); FLD(ground); FLD(salt); FLD(body); FLD(limbs); FLD(height); FLD(calling); FLD(cstyle); FLD(diet); FLD(temper); FLD(pop); FLD(tech); FLD(cover); FLD(div);
      { Recipe t = r; t.climate++; n++; if(cr_seed(&t) == s1) same++; } { Recipe t = r; t.col[1]++; n++; if(cr_seed(&t) == s1) same++; } { Recipe t = r; t.gift[2] = (uint8_t)((t.gift[2] + 5) % 24); n++; if(cr_seed(&t) == s1) same++; }
      { Recipe t = r; t.name[0] = 'Z'; n++; if(cr_seed(&t) == s1) same++; } { Recipe t = r; t.cre[0].amount = (uint8_t)(t.cre[0].amount % 9 + 1); n++; if(cr_seed(&t) == s1) same++; } { Recipe t = r; t.theme[0] = 3; n++; if(cr_seed(&t) == s1) same++; }
      CHECK(!same, "%d of %d single changes left the seed alone", same, n);
      // many recipes -> many different seeds
      const int N = 200000; uint64_t *sd = malloc(N * 8); for(int i = 0; i < N; i++){ cr_randomize(&r, -1, &rng); sd[i] = cr_seed(&r); } qsort(sd, N, 8, cmpu64);
      int dup = 0; for(int i = 1; i < N; i++) if(sd[i] == sd[i - 1]) dup++; CHECK(!dup, "%d seed collisions in %d random recipes", dup, N); free(sd);
      printf("seed: deterministic, all %d single-field changes move it, %d random recipes -> %d collisions\n", n, N, dup);
      // the world: same recipe, same settings; land follows the salt, not the people
      CrWorld a, b; cr_default(&r, 1); cr_world(&r, &a); cr_world(&r, &b); CHECK(!memcmp(&a, &b, sizeof a), "cr_world is not deterministic");
      Recipe p2 = r; p2.col[0] ^= 3; p2.gift[0] = (uint8_t)((p2.gift[0] + 1) % 24); p2.tech = (uint8_t)((p2.tech + 1) & 7); cr_world(&p2, &b); CHECK(a.tseed == b.tseed, "editing the people reshaped the land");
      p2 = r; p2.salt++; cr_world(&p2, &b); CHECK(a.tseed != b.tseed, "the salt does not change the land");
      printf("world: deterministic; the land comes from the salt only\n"); }
    // ---- 2. address hook
    { cr_default(&r, 2); Addr ad; cr_addr(&r, &ad); CHECK(cr_addr_is_created(&ad), "created address not recognised"); CHECK(addr_seed(&ad) == cr_seed(&r), "addr_seed does not return the recipe seed");
      Addr plain; memset(&plain, 0, sizeof plain); plain.uni[0] = 1; CHECK(!cr_addr_is_created(&plain) && addr_seed(&plain) != 0, "plain address hit the hook");
      ad.uni[2] ^= 1; CHECK(!cr_addr_is_created(&ad), "a broken created address is still accepted"); printf("address: a created planet is an address and addr_seed gives its recipe seed back\n"); }
    // ---- 3. share code
    { int N = 30000, ok = 0, longest = 0; char code[400]; uint64_t s = 99;
      for(int i = 0; i < N; i++){ if(i < 4) cr_default(&r, (uint32_t)i); else cr_randomize(&r, -1, &s);
        int L = cr_encode(&r, code, sizeof code); if(L > longest) longest = L; if(!L){ printf("  encode failed\n"); continue; }
        Recipe d; if(cr_decode(code, &d) && cr_equal(&r, &d) && cr_seed(&r) == cr_seed(&d)) ok++; else if(bad < 5) { printf("  mismatch for %s\n", code); bad++; } }
      CHECK(ok == N, "share code round trip %d of %d", ok, N);
      cr_default(&r, 0); int L = cr_encode(&r, code, sizeof code); printf("code of the default: %s (%d letters)\n", code, L);
      // typing mistakes: lower case, no dashes, a wrong letter, a cut
      char t[400]; Recipe d; int i2 = 0; for(char *p = code; *p; p++) if(*p != '-') t[i2++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p + 32) : *p; t[i2] = 0;
      CHECK(cr_decode(t, &d) && cr_equal(&r, &d), "lower case without dashes does not decode");
      int rejected = 0, tried = 0; for(int k = 5; k < L; k++){ if(code[k] == '-') continue; strcpy(t, code); t[k] = t[k] == 'A' ? 'B' : 'A'; tried++; if(!cr_decode(t, &d) || !cr_equal(&r, &d)) rejected++; }
      CHECK(rejected == tried, "a wrong letter was accepted %d times of %d", tried - rejected, tried);
      strcpy(t, code); t[L - 7] = 0; CHECK(!cr_decode(t, &d), "a cut code was accepted"); CHECK(!cr_decode("hello", &d) && !cr_decode("", &d) && !cr_decode("PL1-", &d), "rubbish was accepted");
      printf("share code: %d random recipes round-trip, longest code %d letters, %d of %d single typos caught\n", ok, longest, rejected, tried); }
    // ---- 4. files
    { const char *path = "/tmp/cr_test.crp"; int ok = 0; uint64_t s = 7; for(int i = 0; i < 300; i++){ cr_randomize(&r, -1, &s); if(!cr_save(&r, path)) break; Recipe d; if(cr_load(&d, path) && cr_equal(&r, &d)) ok++; }
      CHECK(ok == 300, "file round trip %d of 300", ok); Recipe d; CHECK(!cr_load(&d, "/tmp/cr_no_such_file.crp"), "loaded a missing file"); printf("files: %d of 300 recipes save and load back equal\n", ok); }
    // ---- 5. undo and redo
    { CrHist h; cr_default(&r, 0); cr_hist_init(&h, &r); Recipe orig = r; Recipe cur = r;
      cur.sea = 10; cr_hist_push(&h, &cur); cur.cls = 4; cr_hist_push(&h, &cur); cr_hist_push(&h, &cur);          // (the second push of the same recipe changes nothing)
      CHECK(h.n == 3, "history has %d entries, want 3", h.n);
      Recipe x; CHECK(cr_hist_undo(&h, &x) && x.cls == orig.cls && x.sea == 10, "undo 1"); CHECK(cr_hist_undo(&h, &x) && cr_equal(&x, &orig), "undo 2"); CHECK(!cr_hist_undo(&h, &x), "undo past the start");
      CHECK(cr_hist_redo(&h, &x) && x.sea == 10, "redo 1"); cur = x; cur.size = 5; cr_hist_push(&h, &cur); CHECK(!cr_hist_redo(&h, &x), "redo after a new edit should be gone");
      for(int i = 0; i < 200; i++){ cur.sea = (uint8_t)(i % 100); cur.land = (uint8_t)(i & 7); cur.salt = (uint32_t)i; cr_hist_push(&h, &cur); } CHECK(h.n <= CR_HIST && h.at == h.n - 1, "history overflow");
      int back = 0; while(cr_hist_undo(&h, &x)) back++; CHECK(back == CR_HIST - 1, "can undo %d steps, want %d", back, CR_HIST - 1); printf("undo/redo: ok, %d steps kept\n", CR_HIST - 1); }
    // ---- 6. randomise one section changes only that section
    { uint64_t s = 5; int bad0 = bad; for(int sec = 0; sec < CR_NSECT; sec++) for(int k = 0; k < 40; k++){ cr_randomize(&r, -1, &s); Recipe before = r; cr_randomize(&r, sec, &s);
          int w = memcmp(&before.cls, &r.cls, offsetof(Recipe, trait) - offsetof(Recipe, cls)) != 0 || before.salt != r.salt, t = memcmp(before.trait, r.trait, sizeof r.trait) != 0 || before.theme[0] != r.theme[0],
              p = memcmp(&before.body, &r.body, offsetof(Recipe, cover) - offsetof(Recipe, body)) != 0, l = before.cover != r.cover || before.ncre != r.ncre || before.npin != r.npin, nm = strcmp(before.name, r.name) != 0;
          if(sec != CRS_WORLD && w) { bad++; printf("FAIL: section %d changed the world\n", sec); } if(sec != CRS_TRAITS && t) { bad++; printf("FAIL: section %d changed traits\n", sec); }
          if(sec != CRS_PEOPLE && p) { bad++; printf("FAIL: section %d changed the people\n", sec); } if(sec != CRS_LIFE && l) { bad++; printf("FAIL: section %d changed the life\n", sec); }
          if(sec != CRS_NAME && nm) { bad++; printf("FAIL: section %d changed the name\n", sec); } if(bad > bad0 + 3) break; } printf("randomise: each section touches only its own fields\n"); }
    // ---- 7. footprint
    { BioReport a, b; int ok = 1; uint64_t h = 1469598103934665603ull; uint64_t s = 11;
      for(int i = 0; i < 2000; i++){ if(i < 4) cr_default(&r, (uint32_t)i); else cr_randomize(&r, -1, &s);
          cr_bio(&r, &a); cr_bio(&r, &b); if(memcmp(&a, &b, sizeof a)){ ok = 0; break; }
          CHECK(a.health >= 0 && a.health <= 100 && a.grade >= 'A' && a.grade <= 'F', "health %d grade %c", a.health, a.grade);
          CHECK(a.o2_pct >= 0 && a.o2_pct <= 40 && a.forest_lost_pct >= 0 && a.forest_lost_pct <= 100 && a.smoke >= 0 && a.smoke <= 100, "range");
          for(int k = 0; k < a.nwarn; k++) CHECK(fontsafe(a.warn[k]) && strlen(a.warn[k]) <= 40, "warning text %s", a.warn[k]);
          for(int k = 0; k < a.n_links; k++) CHECK(fontsafe(a.link[k].who) && fontsafe(a.link[k].what), "link text");
          if(i < 4){ h = fnv(&a.health, sizeof a.health, h); h = fnv(&a.o2_pct, sizeof a.o2_pct, h); h = fnv(&a.n_flora, sizeof a.n_flora, h); h = fnv(a.traj, sizeof a.traj, h); } }
      CHECK(ok, "footprint is not stable (two runs differ)"); printf("footprint: stable over 2000 recipes; fingerprint of the 4 examples %016llx\n", (unsigned long long)h);
      // sense: more people -> more smoke, farms and forest lost; no plants -> bad; a balanced example beats an unbalanced one
      BioReport lo, hi; cr_default(&r, 0); r.pop = 8; cr_bio(&r, &lo); r.pop = 14; cr_bio(&r, &hi);
      CHECK(hi.smoke >= lo.smoke && hi.farm_pct >= lo.farm_pct && hi.forest_lost_pct >= lo.forest_lost_pct && hi.health <= lo.health, "more people should not make it healthier");
      cr_default(&r, 0); cr_bio(&r, &a); Recipe z = r; z.div = 0; z.npin = 0; z.cover = 0; cr_bio(&z, &b); CHECK(b.health < a.health, "no plants should be worse");
      Recipe g = r; g.cre[0].amount = 9; g.cre[1].amount = 9; g.cre[2].amount = 9; cr_bio(&g, &b); CHECK(b.lvl_use[0] > a.lvl_use[0], "more plant eaters should eat more");
      Recipe c2 = r; c2.pop = 15; c2.tech = 5; c2.height = 7; cr_bio(&c2, &b); CHECK(b.grade >= 'D' && b.nwarn > 0, "a crowded planet of giants should get a bad grade and warnings (got %c)", b.grade);
      cr_default(&r, 2); cr_bio(&r, &a); CHECK(a.n_flora >= 300 && a.n_species > 300, "a lush planet should have 300 kinds of plant (has %d)", a.n_flora);
      printf("footprint: more people means more smoke and less health; the lush example holds %d kinds of plant, %d species\n", a.n_flora, a.n_species); }
    // ---- 8. all words are font-safe
    { int unsafe = 0; for(int i = 0; i < 16; i++) unsafe += !fontsafe(cr_cls_name[i]) + !fontsafe(cr_body_name[i]) + !fontsafe(cr_calling_name[i]) + !fontsafe(cr_cstyle_name[i]);
      for(int i = 0; i < 24; i++) unsafe += !fontsafe(cr_gift_name[i]); for(int i = 0; i < cr_trait_count(); i++) unsafe += !fontsafe(cr_trait_name(i)); for(int i = 0; i < cr_theme_count(); i++) unsafe += !fontsafe(cr_theme_name(i));
      for(int i = 0; i < cr_species_count(); i++){ CrSpecies sp; cr_species(i, &sp); unsafe += !fontsafe(sp.name) + !fontsafe(sp.eats); }
      for(int i = 0; i < 8; i++) unsafe += !fontsafe(cr_tech_name[i]) + !fontsafe(cr_temper_name[i]); for(int i = 0; i < 6; i++) unsafe += !fontsafe(cr_diet_name[i]) + !fontsafe(cr_size_name[i]);
      CHECK(!unsafe, "%d words use letters the font lacks", unsafe); printf("words: all font-safe (%d traits, %d themes, %d creature kinds)\n", cr_trait_count(), cr_theme_count(), cr_species_count()); }
    printf(bad ? "CREATOR TESTS FAILED (%d)\n" : "CREATOR TESTS PASSED\n", bad);
    return bad != 0;
}
