// meaning_test.c: tests and the coverage report of the MEANING LAYER. Build and run with ./test.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "meaning.h"
#include "../content/flora.h"
#include "../laws/laws.h"

static int fails;
#define CHECK(c, ...) do{ if(!(c)){ if(fails < 40){ printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } fails++; } }while(0)
static uint64_t rs = 88172645463325252ull;
static uint32_t r32(void){ rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 8); }
static const char *okset = " ',():-.^0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static int dead_n, items_n; static int reach[MN_NCH];

static void check_item(int cls, uint32_t id, long *tally){
    MnRef r = {(uint8_t)cls, id, 0}; MnItem it;
    if(!mn_item(&r, &it)){ CHECK(0, "class %d id %u does not build", cls, id); return; }
    items_n++;
    int rule = 0, look = 0, any = 0;
    for(int i = 0; i < it.n; i++){ if(it.ch[i] < MN_RULE_CH) rule++; else look++; if(it.amt[i]) any = 1; reach[it.ch[i]] = 1; }
    if(!it.tags || !it.n || !any){ dead_n++; if(dead_n < 10) printf("DEAD: %s %u (tags %x, effects %d)\n", mn_class_name(cls), id, it.tags, it.n); }
    CHECK(it.role <= MN_BOTH, "role of %d/%u", cls, id);
    CHECK(it.role == MN_PURPOSE ? (rule > 0 && look == 0) : it.role == MN_COSMETIC ? (look > 0 && rule == 0) : (rule > 0 && look > 0), "role %s but %d rule + %d look effects: class %d id %u", mn_role_name(it.role), rule, look, cls, id);
    if(tally) tally[it.role]++;
    if((id & 63) == 7 || cls != MC_CREATURE){ char m[200]; mn_means(&it, m, sizeof m); CHECK(strspn(m, okset) == strlen(m) && strlen(m) > 10, "MEANS text of %d/%u: '%s'", cls, id, m); }
}
int main(int argc, char **argv){
    int quick = argc > 1 && !strcmp(argv[1], "quick");
    CHECK(mn_selfcheck() == 0, "table lengths");
    CHECK(MN_NCH == 32 && MN_RULE_CH == 25 && MN_NTAG == 24, "32 channels");
    // ---- every item of every enumerable class builds; roles match their effects; every item has tags and effects (no dead parameter)
    long top[MC_N][3]; memset(top, 0, sizeof top);
    for(int c = 0; c < MC_N; c++){
        if(c == MC_FLORA) continue;
        int n = mn_class_size(c); CHECK(n > 0, "class %d is empty", c);
        for(int id = 0; id < n; id++) check_item(c, (uint32_t)id, top[c]);
    }
    for(int k = 0; k < (quick ? 20000 : 300000); k++) check_item(MC_FLORA, r32() & (FL_TOTAL - 1), top[MC_FLORA]);
    // counts of the headline numbers
    CHECK(mn_class_size(MC_TRAIT) == 1040 && mn_class_size(MC_STAT) == 112 && mn_class_size(MC_THEME) == 24 && mn_class_size(MC_PEOPLE) == 512 && mn_class_size(MC_CITY) == 256 && mn_class_size(MC_CREATURE) == 12672 && mn_class_size(MC_LAW) == 146, "class sizes");
    // ---- the classic traits that change no number are exactly the COSMETIC base traits
    for(int b = 0; b < TR_BASE; b++){ MnRef r = {MC_TBASE, (uint32_t)b, 0}; MnItem it; mn_item(&r, &it); CHECK((it.role == MN_COSMETIC) == trait_look_only(b), "base trait %d %s: cosmetic %d but look_only %d", b, trait_name(b), it.role == MN_COSMETIC, trait_look_only(b));
        for(int v = 0; v < TR_VARIANTS; v++){ MnRef t = {MC_TRAIT, (uint32_t)(v * TR_BASE + b), 0}; MnItem ti; mn_item(&t, &ti); CHECK(ti.role == it.role, "trait %d role differs from its base", v * TR_BASE + b); } }
    // ---- role mix
    static const int heads[] = {MC_TRAIT, MC_STAT, MC_THEME, MC_PEOPLE, MC_CITY, MC_CREATURE, MC_GIFT, MC_CCOL, MC_RANK, MC_MUT, MC_LAW, MC_GOD};
    long tot[3] = {0, 0, 0}, all = 0;
    printf("ROLE MIX (purpose / cosmetic / both)\n");
    for(unsigned h = 0; h < sizeof heads / sizeof heads[0]; h++){ int c = heads[h]; long s = top[c][0] + top[c][1] + top[c][2];
        printf("  %-10s %6ld   %5.1f%% %5.1f%% %5.1f%%\n", mn_class_name(c), s, 100.0 * top[c][0] / s, 100.0 * top[c][1] / s, 100.0 * top[c][2] / s); for(int r = 0; r < 3; r++) tot[r] += top[c][r]; all += s; }
    printf("  %-10s %6ld   %5.1f%% %5.1f%% %5.1f%%\n", "ALL", all, 100.0 * tot[0] / all, 100.0 * tot[1] / all, 100.0 * tot[2] / all);
    { long s = top[MC_FLORA][0] + top[MC_FLORA][1] + top[MC_FLORA][2]; printf("  %-10s sample %6ld   %5.1f%% %5.1f%% %5.1f%%\n", "PLANT", s, 100.0 * top[MC_FLORA][0] / s, 100.0 * top[MC_FLORA][1] / s, 100.0 * top[MC_FLORA][2] / s); }
    CHECK(100.0 * tot[0] / all > 56 && 100.0 * tot[0] / all < 64, "purpose share %.1f", 100.0 * tot[0] / all);
    CHECK(100.0 * tot[1] / all > 21 && 100.0 * tot[1] / all < 29, "cosmetic share %.1f", 100.0 * tot[1] / all);
    CHECK(100.0 * tot[2] / all > 11 && 100.0 * tot[2] / all < 19, "both share %.1f", 100.0 * tot[2] / all);
    // ---- every channel is reachable by some item
    int nreach = 0; for(int c = 0; c < MN_NCH; c++){ nreach += reach[c]; CHECK(reach[c], "channel %s is reached by no item", mn_ch_name(c)); }
    printf("CHANNELS reached: %d of %d\n", nreach, MN_NCH);
    // ---- every one of the 112 stats is read by a rule (PURPOSE or BOTH) or marked cosmetic, and a read stat moves the profile
    { int read = 0, cos = 0;
      for(int s = 0; s < N_STAT; s++){ MnRef a[1] = {{MC_STAT, (uint32_t)s, 0}}, b[1] = {{MC_STAT, (uint32_t)s, 200}}; MnItem it; mn_item(&a[0], &it); MnProfile pa, pb; mn_resolve(a, 1, &pa, 0); mn_resolve(b, 1, &pb, 0);
        int moved = 0; for(int c = 0; c < MN_RULE_CH; c++) if(pa.raw[c] != pb.raw[c]) moved = 1;
        if(it.role == MN_COSMETIC){ cos++; CHECK(!moved, "cosmetic stat %d moves a rule channel", s); } else { read++; CHECK(moved, "stat %d (%s) is read by no rule", s, stat_name(s)); } }
      printf("STATS: %d read by rules (purpose or both), %d cosmetic, 0 dead\n", read, cos); CHECK(read + cos == 112, "112 stats"); }
    // ---- synergy and clash work, and an unrelated pair does neither
    { MnRef sea[2] = {{MC_TBASE, 25, 0}, {MC_PEOPLE, 10, 0}}; MnProfile p; mn_resolve(sea, 2, &p, 1); CHECK(p.syn_n > 0 && p.tag_syn[0] > 0, "OCEAN WORLD + TIDESINGERS should agree on SEA");
      MnRef bad[2] = {{MC_TBASE, 25, 0}, {MC_TBASE, 36, 0}}; mn_resolve(bad, 2, &p, 1); CHECK(p.clash_n > 0, "OCEAN WORLD vs BONE DESERT should clash"); }
    // ---- 1,000,000 random combinations: no overflow (built with the overflow trap), same answer twice
    long N = quick ? 50000 : 1000000, nasty = 0; uint64_t acc = 0; int maxraw = 0;
    static MnRef refs[200];
    for(long k = 0; k < N; k++){
        int n = 1 + (int)(r32() % ((k & 15) == 0 ? 200 : 40));
        for(int i = 0; i < n; i++){
            int c = (int)(r32() % MC_N); if(r32() % 3 == 0) c = (int)(r32() % 9);
            uint32_t sz = c == MC_FLORA ? FL_TOTAL : (uint32_t)mn_class_size(c);
            refs[i].cls = (uint8_t)c; refs[i].id = r32() % sz; refs[i].val = (int32_t)(r32() % 800) - 300;
            if(c == MC_LAW){ const LawDef *d = &lw_defs[refs[i].id]; refs[i].val = d->xmin + (int32_t)(r32() % (uint32_t)(d->xmax - d->xmin + 1)); }
            if((k & 255) == 255 && c == MC_STAT) refs[i].val = (int32_t)0x7fffffff;               // silly numbers
        }
        MnProfile a, b; mn_resolve(refs, n, &a, (k & 7) == 0); mn_resolve(refs, n, &b, (k & 7) == 0);
        if(mn_profile_hash(&a) != mn_profile_hash(&b) || memcmp(&a, &b, sizeof a)) CHECK(0, "combination %ld is not deterministic", k);
        for(int c = 0; c < MN_NCH; c++){ int v = a.raw[c] < 0 ? -a.raw[c] : a.raw[c]; if(v > maxraw) maxraw = v; if(v > 100000 || a.lvl[c] > 100 || a.lvl[c] < -100) nasty++; }
        acc ^= mn_profile_hash(&a) + (uint64_t)k;
    }
    printf("COMBINATIONS: %ld random sets resolved twice, same every time, biggest channel total %d (cap 100000), out of range: %ld, check %016llx\n", N, maxraw, nasty, (unsigned long long)acc);
    CHECK(nasty == 0, "out of range channel values");
    // ---- hooks stay in their ranges
    { MnProfile p; MnHooks h; for(int i = 0; i < MN_NCH; i++){ p.lvl[i] = (int8_t)((i & 1) ? 100 : -100); } memset(p.tag_n, 0, sizeof p.tag_n); mn_hooks(&p, &h); CHECK(h.hunger_pct >= 40 && h.hunger_pct <= 160 && h.gold_pct >= 50 && h.gold_pct <= 200 && h.fauna_add >= -2 && h.fauna_add <= 2, "hook ranges"); }
    printf("COVERAGE: %d items checked, DEAD PARAMETERS: %d\n", items_n, dead_n);
    CHECK(dead_n == 0, "dead parameters");
    if(fails){ printf("%d FAILURES\n", fails); return 1; }
    printf("MEANING OK\n"); return 0;
}
