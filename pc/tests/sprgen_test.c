// sprgen_test.c: the procedural sprites (pc/pc_sprgen*.c), standalone. Checks, for EVERY generator in the gallery list:
//   DETERMINISM  the same id gives the same picture (hash), also after other pictures were made in between, and at a second size it is again its own picture
//   DIFFERENCE   different ids give different pictures: of 40 ids, no two are (nearly) equal, and the average difference is above a floor
//   NOT EMPTY    every picture draws something (not all clear, not the whole square)
//   SIZES        8..256 px square draw without a crash or a write outside (run it under -fsanitize=address,undefined too: tests/sprgen.sh does)
//   CACHE        sg_cached gives the same picture twice (a hit), a different size or id a different one, the list never grows past its limit, and a flood of ids still answers
//   BLIT         flip mirrors, fog moves colours toward the fog colour, clipping keeps inside the picture
//   WORLD        the same planet seed and class gives the same disc, and a turn of the planet changes it
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../pc_sprgen.h"
static int fails;
#define CHECK(c, ...) do { if(!(c)){ fprintf(stderr, "sprgen: FAIL " __VA_ARGS__); fprintf(stderr, "\n"); fails++; } } while(0)
static int seen(const SgImg *im){ int n = 0; for(int i = 0; i < im->w * im->h; i++) if((im->px[i] >> 24) >= 16) n++; return n; }     // pixels with any real colour (soft glows count)
static uint64_t hash_of(int gen, uint32_t id, int size){ const SgGen *g = &sg_gens[gen]; int w = size, h = size * g->h / g->w; SgImg *im = sg_img_new(w, h); g->draw(im, id); uint64_t x = sg_img_hash(im); sg_img_free(im); return x; }
static void disc_make(SgImg *im, void *a){ struct { SgPlanetSpec sp; float r; } *g = a; sg_planet(im, &g->sp, g->r, -0.5f, 0.4f, 0.7f); }
int main(void){
    int total_pairs = 0, near_pairs = 0;
    for(int gi = 0; gi < sg_ngens; gi++){ const SgGen *g = &sg_gens[gi]; int W = 64, H = 64 * g->h / g->w; enum { N = 40 };
        SgImg *im[N]; uint32_t ids[N]; for(int i = 0; i < N; i++){ ids[i] = (uint32_t)(((uint64_t)i * 2654435761u + gi * 97u) % g->nids); if(i < 8) ids[i] = (uint32_t)i % g->nids; }
        for(int i = 0; i < N; i++){ im[i] = sg_img_new(W, H); g->draw(im[i], ids[i]); }
        // determinism: draw again (other pictures were made in between)
        for(int i = 0; i < N; i += 3){ SgImg *b = sg_img_new(W, H); g->draw(b, ids[i]); CHECK(sg_img_hash(b) == sg_img_hash(im[i]), "%s id %u is not the same picture twice", g->name, ids[i]); sg_img_free(b); }
        // not empty, not solid
        for(int i = 0; i < N; i++){ int cv = seen(im[i]); CHECK(cv > W * H / 150, "%s id %u draws almost nothing (%d px)", g->name, ids[i], cv); }
        // difference: pairs of different ids
        long sum = 0; int pairs = 0, same = 0;
        for(int i = 0; i < N; i++) for(int j = i + 1; j < N; j++){ if(ids[i] == ids[j]) continue; int d = sg_img_diff(im[i], im[j]); sum += d; pairs++; if(d < 3){ same++; if(same <= 3) fprintf(stderr, "sprgen: note: %s ids %u and %u differ by only %d\n", g->name, ids[i], ids[j], d); } }
        total_pairs += pairs; near_pairs += same; double avg = pairs ? (double)sum / pairs : 0;
        CHECK(same <= pairs / 40, "%s: %d of %d pairs of ids look the same", g->name, same, pairs);
        CHECK(avg > 8.0, "%s: ids differ by only %.1f per mille on average", g->name, avg);
        printf("%-10s %5u ids  avg difference %5.1f  near-equal pairs %d of %d\n", g->name, g->nids, avg, same, pairs);
        for(int i = 0; i < N; i++) sg_img_free(im[i]);
        // sizes
        static const int sz[] = { 8, 16, 24, 33, 64, 97, 128, 256 };
        for(unsigned k = 0; k < sizeof sz / sizeof sz[0]; k++){ int w = sz[k], h = sz[k] * g->h / g->w; if(h < 1) h = 1; SgImg *b = sg_img_new(w, h); g->draw(b, (uint32_t)(k * 7919u) % g->nids); CHECK(seen(b) > 0, "%s draws nothing at %d px", g->name, w); sg_img_free(b); }
        // the same id at two sizes is the same picture (a hash of the 64 px one is not the 32 px one, but the covered area should be close)
        if(strcmp(g->name, "WEATHER")) { SgImg *a = sg_img_new(32, 32 * g->h / g->w), *b = sg_img_new(128, 128 * g->h / g->w); g->draw(a, ids[5]); g->draw(b, ids[5]); double ca = (double)seen(a) / (a->w * a->h), cb = (double)seen(b) / (b->w * b->h);
          CHECK(ca < cb * 1.6 + 0.02 && cb < ca * 1.6 + 0.02, "%s covers %.2f at 32 px but %.2f at 128 px (not the same picture)", g->name, ca, cb); sg_img_free(a); sg_img_free(b); }
        // labels exist
        { char l[64]; l[0] = 0; g->label(ids[3], l, sizeof l); CHECK(l[0], "%s has no label for id %u", g->name, ids[3]); }
    }
    // the same hash from two separate calls of the whole list, in two orders
    { uint64_t a[64]; for(int gi = 0; gi < sg_ngens && gi < 64; gi++) a[gi] = hash_of(gi, 11, 48); for(int gi = sg_ngens - 1; gi >= 0; gi--) CHECK(hash_of(gi, 11, 48) == a[gi], "%s: the hash depends on what was drawn before", sg_gens[gi].name); }
    // the cache
    { sg_cache_clear(); const SgImg *a = sg_cached(0, 5, 64), *b = sg_cached(0, 5, 64), *c = sg_cached(0, 6, 64), *d = sg_cached(0, 5, 32); int hits, miss, live; sg_cache_stats(&hits, &miss, &live);
      CHECK(a && a == b, "cache: a repeated request is not the same picture"); CHECK(c != a && d != a, "cache: a different id or size gave the same picture"); CHECK(hits >= 1 && miss == 3, "cache: %d hits %d misses", hits, miss);
      uint64_t h0 = sg_img_hash(a); for(int i = 0; i < 700; i++) (void)sg_cached(i % sg_ngens, (uint32_t)i * 37u, 24); sg_cache_stats(&hits, &miss, &live); CHECK(live <= 192, "cache: %d pictures live (limit 192)", live);
      const SgImg *e = sg_cached(0, 5, 64); CHECK(e && sg_img_hash(e) == h0, "cache: the picture came back different after it was dropped"); sg_cache_clear(); }
    // blit
    { SgImg *s = sg_img_new(8, 8); for(int i = 0; i < 64; i++) s->px[i] = (i % 8) < 4 ? 0xFFFF0000u : 0x000000FFu | 0xFF000000u; static uint32_t d1[16 * 16], d2[16 * 16];
      SgBlit b = { 4, 4, 8, 8, 0, 0, 0, 0, 0 }; sg_blit(d1, 16, 16, s, &b); b.flags = 1; sg_blit(d2, 16, 16, s, &b);
      CHECK((d1[8 * 16 + 5] >> 16 & 255) > 200 && (d2[8 * 16 + 5] & 255) > 200, "blit: flip does not mirror"); CHECK(d1[0] == 0 && d1[15 * 16 + 15] == 0, "blit: wrote outside its box");
      memset(d1, 0, sizeof d1); b.flags = 0; b.fog = 255; b.fogc = 0x00FF00; sg_blit(d1, 16, 16, s, &b); CHECK((d1[8 * 16 + 5] >> 8 & 255) > 240 && (d1[8 * 16 + 5] >> 16 & 255) < 10, "blit: full fog is not the fog colour");
      memset(d1, 0, sizeof d1); b.fog = 0; b.x = -20; b.y = 100; sg_blit(d1, 16, 16, s, &b); CHECK(d1[0] == 0, "blit: off-screen drew"); b.x = -4; b.y = -4; b.w = 40; b.h = 40; sg_blit(d1, 16, 16, s, &b); CHECK(d1[8 * 16 + 8] != 0, "blit: a big sprite over the picture drew nothing"); sg_img_free(s); }
    // planets: the same seed the same disc, a turn changes it, the spec classes all draw
    { SgPlanetSpec p; sg_planet_spec(&p, 12345, 0); SgImg *a = sg_img_new(96, 96), *b = sg_img_new(96, 96), *c = sg_img_new(96, 96); sg_planet(a, &p, 0.1f, -0.6f, 0.4f, 0.7f); sg_planet(b, &p, 0.1f, -0.6f, 0.4f, 0.7f); sg_planet(c, &p, 0.35f, -0.6f, 0.4f, 0.7f);
      CHECK(sg_img_hash(a) == sg_img_hash(b), "planet: not the same twice"); CHECK(sg_img_diff(a, c) > 3, "planet: a turn does not change it"); sg_img_free(a); sg_img_free(b); sg_img_free(c);
      for(int cls = 0; cls < SG_NCLS; cls++){ sg_planet_spec(&p, 777 + cls, cls); SgImg *x = sg_img_new(64, 64); sg_planet(x, &p, 0.2f, 0.5f, 0.3f, 0.8f); CHECK(sg_img_cover(x) > 800, "planet class %d draws almost nothing", cls); sg_img_free(x); } }
    // the byte cap: a 256 px picture is 256 KB, so with a 2 MB cap only a few stay; trim gives everything back
    { sg_cache_clear(); sg_cache_set_cap_mb(2); for(int i = 0; i < 40; i++) (void)sg_cached(0, (uint32_t)i * 13u, 256); CHECK(sg_cache_bytes() <= (2L << 20), "cache cap: %ld bytes held (cap 2 MB)", sg_cache_bytes());
      int live; sg_cache_stats(0, 0, &live); CHECK(live >= 2 && live <= 8, "cache cap: %d big pictures live", live); sg_cache_trim(); CHECK(sg_cache_bytes() == 0, "cache trim: %ld bytes left", sg_cache_bytes()); sg_cache_set_cap_mb(32); sg_cache_clear(); }
    // the background worker: a queued picture arrives, and equals the one made at once (also a planet disc, whose surface map the worker makes in its own small cache)
    { CHECK(sg_async_start(), "async: no worker"); sg_cache_clear(); const SgImg *a = 0; for(long i = 0; i < 20000000L && !a; i++) a = sg_cached_try(5, 77, 64);
      CHECK(a != 0, "async: the picture never came"); if(a){ CHECK(sg_img_hash(a) == hash_of(5, 77, 64), "async: the picture differs from the one made at once"); }
      for(int k = 0; k < 8; k++) (void)sg_cached_try(k, 3u + (uint32_t)k, 48); const SgImg *b = sg_cached(3, 3u + 3u, 48); CHECK(b != 0 && sg_img_hash(b) == hash_of(3, 6, 48), "async: waiting for a queued picture gave another one");
      SgPlanetSpec p; sg_planet_spec(&p, 4242, -1); SgImg *ref = sg_img_new(80, 80); sg_planet(ref, &p, 0.3f, -0.5f, 0.4f, 0.7f); const SgImg *d = 0; struct { SgPlanetSpec sp; float r; } arg = { p, 0.3f };
      for(long i = 0; i < 20000000L && !d; i++) d = sg_cached_ex_try(0xABCDu, 80, 80, disc_make, &arg, (int)sizeof arg); CHECK(d && sg_img_hash(d) == sg_img_hash(ref), "async: a planet disc made by the worker differs"); sg_img_free(ref); sg_cache_clear(); }
    sg_cache_clear();
    printf("sprgen: %d generators, %d pairs compared, %d look alike, %d failures\n", sg_ngens, total_pairs, near_pairs, fails);
    return fails ? 1 : 0;
}
