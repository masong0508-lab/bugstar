// laws_test.c: tests for the LAWS table, editing, files and the model. Run pc/laws/test.sh.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "laws.h"

static int fails;
#define CHECK(c, ...) do{ if(!(c)){ fails++; printf("FAIL %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); if(fails > 30) exit(1); } }while(0)
static uint64_t rs = 88172645463325252ull;
static uint64_t rnd(void){ rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static int rndi(int n){ return (int)(rnd() % (uint64_t)n); }
static int font_ok(const char *s){ for(; *s; s++){ char c = *s; if(!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr(" .,':()-/%?!+<>=^*#", c))) return 0; } return 1; }

static void rand_env(LwEnv *e){
    memset(e, 0, sizeof *e); e->useed = rnd(); e->gal = (rnd() & 3) ? (uint32_t)rnd() | 1 : 0; e->star = (uint32_t)rnd();
    e->seed_a = (uint32_t)rnd(); e->seed_b = (uint32_t)rnd(); e->seed_c = (uint32_t)rnd(); e->story = rndi(8) == 0;
    e->nplan = 1 + rndi(16); e->planet = rndi(e->nplan); e->nmoon = rndi(5); e->moon = (rndi(4) == 0 && e->nmoon) ? 1 + rndi(e->nmoon) : 0;
    e->psz = e->moon ? rndi(2) : rndi(6); e->grav = rndi(4); e->cls = rndi(16); e->hour = rndi(3); e->weather = rndi(6); e->snow = rndi(2); e->sand = rndi(2);
    e->city = rndi(2); e->ruins = rndi(2); e->fauna = rndi(6); e->beetles = rndi(4); e->rough = rndi(4); e->hs = rndi(8); e->mood = rndi(3); e->water = rndi(101); e->snowl = 50 + rndi(200); e->hell = e->cls >= 10;
    e->dist_clusters_log2 = rndi(150);
}
static void rand_laws(LawSet *s, int cheat){
    lw_default(s);
    int mode = rndi(4);
    for(int i = 0; i < LW_N; i++){
        if(mode == 0 && rndi(4)) continue;                                          // a few laws only
        const LawDef *d = &lw_defs[i]; int32_t lo = cheat ? d->xmin : d->min, hi = cheat ? d->xmax : d->max; int32_t v;
        int k = rndi(6); if(k == 0) v = lo; else if(k == 1) v = hi; else if(k == 2) v = d->def; else v = lo + (int32_t)(rnd() % (uint64_t)(hi - lo + 1));
        lw_set(s, i, v, cheat);
    }
}
static int finite_ok(const LwResult *r){ for(int i = 0; i < LO_N; i++) if(!isfinite(r->o[i])) return 0; return 1; }
static int apply_ok(const LwApply *a, const char **why){
#define BAD(c, w) if(c){ *why = w; return 0; }
    BAD(a->tw != 8 && a->tw != 16 && a->tw != 32 && a->tw != 64 && a->tw != 128, "tw");
    BAD(a->k16 < 0 || a->k16 > 4000, "k16") BAD(a->psz < 0 || a->psz > 5, "psz") BAD(a->grav < 0 || a->grav > 3, "grav") BAD(a->slow < 0 || a->slow > 0x1000, "slow") BAD(a->zerog < 0 || a->zerog > 8, "zerog")
    BAD(a->hour < -1 || a->hour > 2, "hour") BAD(a->weather < -1 || a->weather > 5, "weather") BAD(a->cls < -1 || a->cls > 15, "cls") BAD(a->fauna < -1 || a->fauna > 5, "fauna") BAD(a->beetles < -1 || a->beetles > 3, "beetles")
    BAD(a->rough < -1 || a->rough > 3, "rough") BAD(a->hs < -1 || a->hs > 12, "hs") BAD(a->mood < -1 || a->mood > 2, "mood") BAD(a->snowl < -1 || a->snowl > 255, "snowl")
    BAD(a->water_pct < 0 || a->water_pct > 400, "water") BAD(a->hunger_pct < 0 || a->hunger_pct > 900, "hunger") BAD(a->danger_add < -4 || a->danger_add > 12, "danger")
    BAD(a->aurora < 0 || a->aurora > 25 || a->meteor < 0 || a->meteor > 25 || a->badair < 0 || a->badair > 25, "traits")
    for(int i = 0; i < 4; i++) BAD(a->node_add[i] < -3 || a->node_add[i] > 8, "nodes")
    return 1;
}

int main(void){
    // ---------------------------------------------------------------- the table
    printf("laws: %d\n", lw_count());
    CHECK(lw_count() >= 100 && lw_count() <= 200, "law count %d", lw_count());
    int percat[LC_N] = { 0 }, nadv = 0, nsimple = 0;
    for(int i = 0; i < LW_N; i++){
        const LawDef *d = &lw_defs[i];
        CHECK(d->min <= d->def && d->def <= d->max, "%s default %d outside %d..%d", d->key, d->def, d->min, d->max);
        CHECK(d->xmin <= d->min && d->max <= d->xmax, "%s cheat range %d..%d does not hold %d..%d", d->key, d->xmin, d->xmax, d->min, d->max);
        CHECK(d->step >= 1 && d->big >= d->step, "%s steps %d %d", d->key, d->step, d->big);
        CHECK(strlen(d->name) <= 26, "%s name too long: %s", d->key, d->name);
        CHECK(strlen(d->desc) >= 20 && strlen(d->desc) <= 116, "%s description is %d long", d->key, (int)strlen(d->desc));
        CHECK(font_ok(d->name) && font_ok(d->desc) && font_ok(d->unit), "%s has a letter the font lacks", d->key);
        CHECK(d->cat < LC_N, "%s category", d->key);
        percat[d->cat]++; if(d->flags & LF_ADV) nadv++; else nsimple++;
        for(int j = 0; j < i; j++) CHECK(strcmp(d->key, lw_defs[j].key) && strcmp(d->name, lw_defs[j].name), "duplicate %s", d->key);
        CHECK(lw_find(d->key) == i, "find %s", d->key);
        if(d->flags & LF_ENUM) for(int v = d->min; v <= d->max; v++) CHECK(lw_enum_name(i, v)[0] && font_ok(lw_enum_name(i, v)), "%s enum %d", d->key, v);
    }
    for(int c = 0; c < LC_N; c++){ printf("  %-9s %d laws\n", lw_cat_name[c], percat[c]); CHECK(percat[c] >= 10, "category %s has only %d laws", lw_cat_name[c], percat[c]); }
    printf("  simple list %d laws, ADVANCED adds %d\n", nsimple, nadv);
    CHECK(nsimple >= 30 && nsimple <= 60, "the simple list should be short (%d)", nsimple);
    { LawSet d; lw_default(&d); CHECK(lw_is_default(&d) && lw_edited_count(&d) == 0, "default set"); }

    // ---------------------------------------------------------------- clamping and stepping
    for(int it = 0; it < 20000; it++){
        int id = rndi(LW_N); const LawDef *d = &lw_defs[id]; LawSet s; lw_default(&s);
        int32_t v = (int32_t)(rnd() % 2000001) - 1000000;
        int32_t a = lw_set(&s, id, v, 0); CHECK(a >= d->min && a <= d->max, "%s clamp %d -> %d", d->key, v, a);
        CHECK(!s.cheated, "inside the sensible range is not a cheat");
        int32_t b = lw_set(&s, id, v, 1); CHECK(b >= d->xmin && b <= d->xmax, "%s cheat clamp %d -> %d", d->key, v, b);
        CHECK(s.cheated == (b < d->min || b > d->max), "%s cheated flag %d for %d", d->key, s.cheated, b);
    }
    for(int id = 0; id < LW_N; id++){                                                // walk from the bottom to the top by single steps: it must reach the top, and pass NORMAL
        const LawDef *d = &lw_defs[id]; LawSet s; lw_default(&s); lw_set(&s, id, d->min, 0); int hit = s.v[id] == d->def, n = 0;
        while(s.v[id] < d->max && n < 100000){ int32_t o = s.v[id]; lw_step(&s, id, 1, 0, 0); n++; if(s.v[id] == d->def) hit = 1; CHECK(s.v[id] > o, "%s stuck going up at %d", d->key, o); }
        CHECK(s.v[id] == d->max && hit, "%s: up-walk ended at %d, passed normal %d", d->key, s.v[id], hit);
        hit = s.v[id] == d->def; n = 0;
        while(s.v[id] > d->min && n < 100000){ int32_t o = s.v[id]; lw_step(&s, id, -1, 1, 0); n++; if(s.v[id] == d->def) hit = 1; CHECK(s.v[id] < o, "%s stuck going down", d->key); }
        CHECK(s.v[id] == d->min && hit, "%s: big down-walk ended at %d", d->key, s.v[id]);
        lw_reset_one(&s, id); CHECK(s.v[id] == d->def, "reset one");
    }
    { LawSet s; rand_laws(&s, 1); s.cheated = 1; lw_reset_all(&s); CHECK(lw_edited_count(&s) == 0 && s.cheated == 1, "reset all keeps the cheated mark"); }

    // ---------------------------------------------------------------- text, files, search
    for(int it = 0; it < 500; it++){
        LawSet s, t; rand_laws(&s, it & 1); char buf[8192]; int n = lw_to_text(&s, buf, sizeof buf); CHECK(n > 10 && n < (int)sizeof buf - 100, "text size %d", n);
        CHECK(lw_from_text(&t, buf), "text parse"); CHECK(!memcmp(t.v, s.v, sizeof s.v) && t.cheated == s.cheated, "text round trip (cheated %d %d)", s.cheated, t.cheated);
        CHECK(lw_hash(&s) == lw_hash(&t), "hash round trip");
    }
    { LawSet s, t; rand_laws(&s, 1); s.cheated = 1; CHECK(lw_save(&s, "/tmp/laws_test.laws"), "save"); CHECK(lw_load(&t, "/tmp/laws_test.laws") && t.cheated == 1 && !memcmp(t.v, s.v, sizeof s.v), "file round trip keeps the cheat flag");
      lw_default(&s); lw_save(&s, "/tmp/laws_test.laws"); CHECK(lw_load(&t, "/tmp/laws_test.laws") && t.cheated == 0 && lw_is_default(&t), "a normal file stays normal");
      CHECK(!lw_load(&t, "/tmp/does/not/exist.laws") && lw_is_default(&t), "a missing file gives the defaults"); }
    { LawSet t; CHECK(!lw_from_text(&t, "garbage") && lw_is_default(&t), "garbage text gives the defaults"); CHECK(lw_from_text(&t, "BUGSTAR LAWS 1\ncheated=0\nU_GRAV=99999999\nNOPE=3\nU_LIGHT=abc\n") && t.v[LW_U_GRAV] == lw_defs[LW_U_GRAV].xmax, "a wild file is clamped"); }
    { int n = 0; for(int i = 0; i < LW_N; i++) n += lw_match(i, "GRAVITY"); CHECK(n >= 5, "search GRAVITY finds %d", n); n = 0; for(int i = 0; i < LW_N; i++) n += lw_match(i, "WATER"); CHECK(n >= 3, "search WATER finds %d", n);
      CHECK(lw_match(LW_P_SIZE, "PLANET SIZE") && !lw_match(LW_P_SIZE, "ZZZZ"), "search words"); }
    for(int id = 0; id < LW_N; id++){ LawSet s; lw_default(&s); char t[64]; for(int adv = 0; adv < 2; adv++){ lw_value_text(&s, id, adv, t, sizeof t); CHECK(t[0] && font_ok(t), "%s value text '%s'", lw_defs[id].key, t); }
        lw_set(&s, id, lw_defs[id].max, 0); lw_value_text(&s, id, 0, t, sizeof t); CHECK(t[0] && font_ok(t), "%s max text '%s'", lw_defs[id].key, t);
        lw_set(&s, id, lw_defs[id].xmax, 1); lw_value_text(&s, id, 1, t, sizeof t); CHECK(t[0] && font_ok(t), "%s cheat max text '%s'", lw_defs[id].key, t); }
    // presets
    CHECK(lw_preset_count() >= 12, "presets %d", lw_preset_count());
    for(int p = 0; p < lw_preset_count(); p++){
        LawSet s; lw_default(&s); lw_preset_apply(&s, p); CHECK(font_ok(lw_preset_name(p)) && font_ok(lw_preset_desc(p)) && strlen(lw_preset_name(p)) <= 20 && strlen(lw_preset_desc(p)) <= 116, "preset %d text", p);
        CHECK(!s.cheated, "preset %s must stay inside the sensible range", lw_preset_name(p));
        CHECK(p == 0 ? lw_is_default(&s) : !lw_is_default(&s), "preset %s edited count %d", lw_preset_name(p), lw_edited_count(&s));
    }

    // ---------------------------------------------------------------- determinism: the same laws and place give the same numbers
    for(int it = 0; it < 2000; it++){
        LawSet s; rand_laws(&s, it & 1); LwEnv e; rand_env(&e); LwResult a, b; lw_eval(&s, &e, &a); lw_eval(&s, &e, &b);
        CHECK(!memcmp(a.o, b.o, sizeof a.o), "lw_eval is not repeatable"); LwApply x, y; lw_apply(&s, &e, &x); lw_apply(&s, &e, &y); CHECK(!memcmp(&x, &y, sizeof x), "lw_apply is not repeatable");
        CHECK(lw_nplan(&s, &e, 3) == lw_nplan(&s, &e, 3) && lw_nmoon(&s, &e, 2) == lw_nmoon(&s, &e, 2), "counts repeat");
    }
    { // different laws make different universes; different universe numbers differ under UNIVERSE VARIETY
      LawSet s, t; lw_default(&s); t = s; lw_set(&t, LW_U_GRAV, 200, 0); LwEnv e; rand_env(&e); e.story = 0; LwResult a, b; lw_eval(&s, &e, &a); lw_eval(&t, &e, &b); CHECK(memcmp(a.o, b.o, sizeof a.o), "gravity changes the numbers");
      CHECK(lw_hash(&s) != lw_hash(&t), "hash differs");
      lw_set(&s, LW_U_VARIETY, 80, 0); LawSet u1, u2; lw_effective(&s, 1, &u1); lw_effective(&s, 2, &u2); CHECK(memcmp(u1.v, u2.v, sizeof u1.v), "variety makes two universes differ");
      LawSet u3; lw_effective(&s, 1, &u3); CHECK(!memcmp(u1.v, u3.v, sizeof u1.v), "variety is repeatable");
      lw_default(&s); lw_effective(&s, 5, &u1); CHECK(!memcmp(u1.v, s.v, sizeof s.v), "no variety, no change"); }

    // ---------------------------------------------------------------- the default laws change nothing
    for(int it = 0; it < 20000; it++){
        LawSet s; lw_default(&s); LwEnv e; rand_env(&e); LwApply a; lw_apply(&s, &e, &a);
        static const int ks[6] = { 12, 7, 4, 3, 2, 1 };
        CHECK(!a.changed && a.size_q == 0 && a.tw == 128 && a.k16 == 16 * ks[e.psz] && a.psz == e.psz && a.grav == e.grav && !a.slow && !a.zerog, "defaults must not change the planet (changed %d)", a.changed);
        CHECK(a.hour == -1 && a.weather == -1 && a.cls == -1 && a.city == -1 && a.fauna == -1 && a.hs == -1 && a.water_pct == 100 && a.hunger_pct == 100, "defaults must not change the DNA");
        int np = 1 + rndi(8), nm = rndi(5); CHECK(lw_nplan(&s, &e, np) == np && lw_nmoon(&s, &e, nm) == nm, "default counts");
        uint32_t roll = (uint32_t)rnd(); CHECK(lw_dense(&s, roll, 12) == (roll % 12u == 0), "default crowd"); int r100 = rndi(100), hp = 8 + rndi(17); CHECK(lw_hell_hit(&s, r100, hp) == (r100 < hp), "default hell");
        CHECK(lw_weird_hit(&s, roll) == (roll % 111u == 0), "default weird"); uint32_t h = (uint32_t)rnd() % 100000; CHECK(lw_hours(&s, h) == h, "default hours"); int cost = 20 + rndi(500); CHECK(lw_warp_cost(&s, cost) == cost, "default fuel");
        CHECK(lw_uly(&s) == 25 && lw_galbits(&s) == 20 && lw_time_pct(&s) == 100, "default constants");
    }

    // ---------------------------------------------------------------- every law does something
    {
        LwEnv probe[48]; for(int i = 0; i < 48; i++){ rand_env(&probe[i]); probe[i].story = 0; probe[i].moon = 0; probe[i].hell = 0; if(probe[i].cls >= 10) probe[i].cls = i % 10; if(i % 4 == 0) probe[i].gal = 0; if(i >= 40) probe[i].story = 1; }
        int dead = 0;
        for(int id = 0; id < LW_N; id++){
            int moved = 0; const LawDef *d = &lw_defs[id];
            for(int ctx = 0; ctx < 10 && !moved; ctx++){                                          // ctx 0: as it is; 1..8: the home galaxy has shape ctx-1; 9: the story stars are red dwarfs, white dwarfs...
            for(int side = 0; side < 4 && !moved; side++){
                LawSet s; lw_default(&s); int32_t v = side == 0 ? d->min : side == 1 ? d->max : side == 2 ? d->xmin : d->xmax; if(v == d->def) continue; lw_set(&s, id, v, 1);
                LawSet dflt; lw_default(&dflt);
                if(ctx >= 1 && ctx <= 8 && id != LW_G_HOME){ s.v[LW_G_HOME] = dflt.v[LW_G_HOME] = ctx - 1; }
                if(ctx == 9 && id != LW_S_HOME){ s.v[LW_S_HOME] = dflt.v[LW_S_HOME] = 6; }
                for(int i = 0; i < 48 && !moved; i++){
                    LwResult a, b; lw_eval(&dflt, &probe[i], &a); lw_eval(&s, &probe[i], &b);
                    for(int k = 0; k < LO_N; k++) if(a.o[k] != b.o[k] || a.gtype != b.gtype || a.sclass != b.sclass){ moved = 1; break; }
                    LwApply x; lw_apply(&s, &probe[i], &x); if(x.changed || x.hunger_pct != 100 || x.danger_add) moved = 1;
                    if(lw_nplan(&s, &probe[i], 4) != 4 || lw_nmoon(&s, &probe[i], 2) != 2) moved = 1;
                    uint32_t roll = (uint32_t)rnd(); if(lw_dense(&s, roll, 12) != (roll % 12u == 0) || lw_weird_hit(&s, roll) != (roll % 111u == 0) || lw_hell_hit(&s, 5, 8) != 1 || lw_hours(&s, 1000) != 1000 || lw_warp_cost(&s, 100) != 100) moved = 1;
                    if(lw_uly(&s) != 25 || lw_galbits(&s) != 20 || lw_time_pct(&s) != 100) moved = 1;
                    if(id == LW_U_VARIETY){ LawSet q; lw_effective(&s, probe[i].useed, &q); if(memcmp(q.v, s.v, sizeof q.v)) moved = 1; }
                    if(id == LW_G_HOME || id == LW_S_HOME) { LwResult a2 = a; (void)a2; }
                }
            } }
            if(!moved){ dead++; printf("  law with no effect found: %s\n", d->key); }
        }
        CHECK(dead == 0, "%d laws change nothing", dead);
        if(!dead) printf("every one of the %d laws moves at least one number\n", LW_N);
    }

    // ---------------------------------------------------------------- extremes: 100,000 random law sets and places, no overflow, no crash
    { long bad = 0; int maxtw[6] = { 0 }; double minr = 1e300, maxr = 0; const char *why = "";
      for(int it = 0; it < 100000; it++){
        LawSet s; rand_laws(&s, it % 3 != 0); LwEnv e; rand_env(&e); LwResult r; LwApply a; lw_eval(&s, &e, &r); lw_apply(&s, &e, &a);
        if(!finite_ok(&r)){ bad++; if(bad < 4) printf("  non-finite output at iteration %d\n", it); }
        if(!apply_ok(&a, &why)){ bad++; if(bad < 4) printf("  bad apply field %s at iteration %d\n", why, it); }
        int np = lw_nplan(&s, &e, 1 + rndi(8)), nm = lw_nmoon(&s, &e, rndi(5)); if(np < 1 || np > 16 || nm < 0 || nm > 4 || (e.story && 0)){ bad++; }
        if(lw_uly(&s) < 1 || lw_galbits(&s) < 4 || lw_galbits(&s) > 40 || lw_warp_cost(&s, 100) < 1 || lw_hours(&s, 100000) < 1) bad++;
        if(r.o[LO_P_RADIUS] < minr) minr = r.o[LO_P_RADIUS]; if(r.o[LO_P_RADIUS] > maxr) maxr = r.o[LO_P_RADIUS];
        maxtw[a.tw == 8 ? 0 : a.tw == 16 ? 1 : a.tw == 32 ? 2 : a.tw == 64 ? 3 : 4]++;
        if(it % 5000 == 0){ LxPage P[LX_PAGES]; int n = lw_explain(&s, &e, P); if(n != LX_PAGES) bad++;
            for(int p = 0; p < n; p++){ if(P[p].n < 3) bad++; for(int l = 0; l < P[p].n; l++){ if(!font_ok(P[p].t[l]) || strlen(P[p].t[l]) > 57){ bad++; if(bad < 6) printf("  text line problem: %s\n", P[p].t[l]); } } } }
      }
      printf("100000 random law sets (sensible and cheat ranges) and places: radius from %.3g m to %.3g m; worlds that repeat every 8/16/32/64 cells: %d/%d/%d/%d, normal 128: %d\n", minr, maxr, maxtw[0], maxtw[1], maxtw[2], maxtw[3], maxtw[4]);
      CHECK(bad == 0, "%ld bad results", bad); }

    // ---------------------------------------------------------------- planet size from pebble to giant
    {
        LawSet s; lw_default(&s); LwEnv e; rand_env(&e); e.story = 0; e.moon = 0; e.psz = 2; e.grav = 1; e.cls = 0; e.gal = 0;
        double last = -1; int prev_tw = 0; (void)prev_tw;
        for(int q = lw_defs[LW_P_SIZE].xmin; q <= lw_defs[LW_P_SIZE].xmax; q += 4){
            lw_default(&s); lw_set(&s, LW_P_SIZE, q, 1); lw_set(&s, LW_P_SPREAD, 0, 0); LwResult r; lw_eval(&s, &e, &r); LwApply a; lw_apply(&s, &e, &a);
            CHECK(r.o[LO_P_RADIUS] > last, "radius must grow with the size law (q=%d)", q); last = r.o[LO_P_RADIUS];
            CHECK(isfinite(r.o[LO_P_CIRC]) && isfinite(r.o[LO_P_WALK]) && isfinite(r.o[LO_P_HORIZON]), "size numbers finite");
            CHECK(a.size_q == q || q == 0, "size_q %d vs %d", a.size_q, q);
            if(q == -112) CHECK(a.tw == 8 && r.o[LO_P_CIRC] < 8, "a pebble is a few steps round (%.2f, tw %d)", r.o[LO_P_CIRC], a.tw);
            if(q == 0) CHECK(a.k16 == 64 && a.tw == 128, "normal size keeps the game's horizon");
            if(q >= 100) CHECK(a.k16 <= 4, "giant worlds are nearly flat (k16 %d)", a.k16);
        }
        printf("planet radius runs from %.3g m (a pebble) to %.3g m (bigger than any star)\n", 0.0 + 6.371e6 * lw_pw2(-32), 6.371e6 * lw_pw2(44));
    }
    // ---------------------------------------------------------------- number words
    { char b[64]; lw_fmt_big(1234567, b, sizeof b); CHECK(!strcmp(b, "1.23 MILLION"), "fmt_big %s", b); lw_fmt_big(999, b, sizeof b); CHECK(!strcmp(b, "999"), "fmt_big 999 %s", b);
      lw_fmt_big(4.5e30, b, sizeof b); CHECK(!strcmp(b, "4.5E30"), "fmt_big sci %s", b); lw_fmt_time(86400.0 * 800, b, sizeof b); CHECK(strstr(b, "YEARS"), "fmt_time %s", b);
      lw_fmt_dist(0.05, b, sizeof b); CHECK(!strcmp(b, "5.00 CM"), "fmt_dist %s", b);
      CHECK(fabs(lw_pw2(10) - 1024) < 1e-9 && fabs(lw_pw2(-3) - 0.125) < 1e-12 && fabs(lw_lg2(1024) - 10) < 1e-9 && fabs(lw_lg2(0.125) + 3) < 1e-12 && fabs(lw_pw2(0.5) - 1.41421356237) < 1e-9, "pw2/lg2"); }
    printf(fails ? "LAWS TESTS: %d FAILED\n" : "LAWS TESTS: ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
