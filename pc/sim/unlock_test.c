// unlock_test.c: standalone tests for unlock.h (patch 47): the feature registry, every trigger, the first-10-minutes rule, prerequisites,
// what is visible (OFF / SHAPES / ALL), the ! markers, tip styles for every option, the save record and its chunk in the .sim file, and the
// slot helpers. pc/sim/test.sh builds and runs it. No game needed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_core.h"

static int fails, checks;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static uint64_t rs = 0xBEEF123ull;
static uint64_t rnd(void){ rs = rs * 6364136223846793005ull + 1442695040888963407ull; return rs >> 11; }
#define BIT(m) (1u << (m))

static uint32_t closure(int ft){ uint32_t m = 0, add = UB(ft); while(add & ~m){ m |= add; add = 0; for(int i = 0; i < UF_N; i++) if(m >> i & 1) add |= ul_defs[i].needs; } return m & ~UB(ft); }   // all prerequisites, deep
static UlFacts zero(void){ UlFacts f; memset(&f, 0, sizeof f); return f; }

int main(void){
    // ---- the registry itself
    CHECK(UF_N <= 32 && UL_ALL == (UF_N == 32 ? 0xFFFFFFFFu : BIT(UF_N) - 1));
    for(int a = 0; a < UF_N; a++){
        const UlDef *d = &ul_defs[a]; CHECK(d->key && d->name && d->key[0] && d->name[0]);
        for(int b = a + 1; b < UF_N; b++) CHECK(strcmp(d->key, ul_defs[b].key) && strcmp(d->name, ul_defs[b].name));
        CHECK(!(d->needs & BIT(a)));                                                     // not its own prerequisite
        CHECK(!(closure(a) & BIT(a)));                                                   // no cycle
        if(d->basic) for(int i = 0; i < UF_N; i++) if(closure(a) >> i & 1) CHECK(ul_defs[i].basic);   // a basic feature only needs basic ones
        const char *ln[4]; int n = ul_tip_lines(a, ln); CHECK(n >= 2 && n <= 4);          // a tip is 2 to 4 short lines
        for(int i = 0; i < n; i++){ CHECK(strlen(ln[i]) <= 26); for(const char *c = ln[i]; *c; c++) CHECK((*c >= 'A' && *c <= 'Z') || *c == ' ' || *c == '.' || *c == ',' || *c == '!' || (*c >= '0' && *c <= '9')); }
        CHECK(strlen(d->name) <= 12);                                                    // "NEW: " + name fits a big-text title
        CHECK(d->weight <= ULW_MINOR);
    }
    CHECK(ul_tip_lines(-1, (const char **)0) == 0 && ul_tip_lines(UF_N, (const char **)0) == 0);
    CHECK(UL_FAME_KNOWN == stand_rank_fame(STR_KNOWN) && UL_FAME_HERO == stand_rank_fame(STR_HERO) && UL_FAME_CHAMP == stand_rank_fame(STR_CHAMPION) && UL_FAME_LEGEND == stand_rank_fame(STR_LEGEND));   // the same ranks the standing ladder uses
    { int basics = 0; for(int a = 0; a < UF_N; a++) basics += ul_defs[a].basic; CHECK(basics >= 5 && basics <= 8); }                   // the first 10 minutes reveal a handful, not everything
    CHECK(ul_defs[UF_COLONY].shape && ul_defs[UF_GODS].shape && ul_defs[UF_MAYOR].shape && ul_defs[UF_ARMADA].shape && !ul_defs[UF_JOBS].shape && !ul_defs[UF_RAID].shape);

    // ---- a new game: nothing is open, nothing is shown, no tip is waiting
    { Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero();
      CHECK(ul_update(&u, &f, 0) == 0 && u.have == 0 && ul_fresh(&u) == 0 && ul_pending(&u) == 0 && ul_next_tip(&u) == -1);
      for(int a = 0; a < UF_N; a++){ CHECK(ul_vis(&u, a, ULSA_OFF) == ULV_HIDDEN); CHECK(ul_vis(&u, a, ULSA_ALL) == ULV_SHOWN); CHECK(!ul_has(&u, a)); }
      for(int s = 0; s < SQ_N; s++){ CHECK(ul_sq_vis(&u, s, ULSA_OFF) == ULV_HIDDEN); CHECK(ul_sq_vis(&u, s, ULSA_ALL) == ULV_SHOWN); } }

    // ---- every trigger on its own: false just before, true at the edge (prerequisites are opened first and the tip state is fresh)
    typedef struct { int ft; void (*off)(Unlock *, UlFacts *); void (*on)(Unlock *, UlFacts *); } Tr;
#define TR(ft, offc, onc) { ft, ({ void fo(Unlock *u, UlFacts *f){ (void)u; (void)f; offc; } fo; }), ({ void fn(Unlock *u, UlFacts *f){ (void)u; (void)f; onc; } fn; }) }
    Tr tr[] = {
      TR(UF_CLOCK,   f->town = 0,                      f->town = 1),
      TR(UF_JOURNAL, f->log_n = 1,                     f->log_n = 2),
      TR(UF_MEET,    u->cnt[ULC_TALK] = 0,             u->cnt[ULC_TALK] = 1),
      TR(UF_JOBS,    u->cnt[ULC_TALK] = 1,             u->cnt[ULC_TALK] = 2),
      TR(UF_NEEDS,   u->cnt[ULC_TALK] = 2,             u->cnt[ULC_TALK] = 3),
      TR(UF_FRIEND,  f->friend_max = 9,                f->friend_max = 10),
      TR(UF_LETTER,  f->letters = 0,                   f->letters = 1),
      TR(UF_ASKS,    u->cnt[ULC_ASK] = 0,              u->cnt[ULC_ASK] = 1),
      TR(UF_GIFT,    u->cnt[ULC_GIFT] = 0,             u->cnt[ULC_GIFT] = 1),
      TR(UF_LOVE,    u->cnt[ULC_MATE] = 0,             u->cnt[ULC_MATE] = 1),
      TR(UF_MONEY,   u->cnt[ULC_GOLD] = 0,             u->cnt[ULC_GOLD] = 1),
      TR(UF_HOME,    f->homes = 0,                     f->homes = 1),
      TR(UF_COLONY,  f->freed = 0,                     f->freed = 1),
      TR(UF_SHOPS,   f->shops = 0,                     f->shops = 1),
      TR(UF_MINE,    f->mine = 0,                      f->mine = 1),
      TR(UF_CALLS,   u->cnt[ULC_CALL] = 0,             u->cnt[ULC_CALL] = 1),
      TR(UF_RAID,    f->raid = 0,                      f->raid = 1),
      TR(UF_WEATHER, f->day = 1,                       f->day = 2),
      TR(UF_SEASON,  (f->season_on = 1, f->day = 6),   (f->season_on = 1, f->day = 7)),
      TR(UF_FAME,    f->fame = 49,                     f->fame = 50),
      TR(UF_LEVEL,   f->level = 9,                     f->level = 10),
      TR(UF_GODS,    f->fame = 349,                    f->fame = 350),
      TR(UF_SANDBOX, f->fame = 699,                    f->fame = 700),
      TR(UF_MAYOR,   f->mayor = 0,                     f->mayor = 1),
      TR(UF_ARMADA,  (f->mayor = 1, f->fame = 1199),   (f->mayor = 1, f->fame = 1200)),
      TR(UF_WORLDS,  f->worlds_total = 0,              f->worlds_total = 1),
      TR(UF_GALAXY,  f->galaxies = 0,                  f->galaxies = 1),
    };
    CHECK((int)(sizeof tr / sizeof tr[0]) == UF_N);                                      // every feature has a trigger test
    for(unsigned i = 0; i < sizeof tr / sizeof tr[0]; i++){
        int ft = tr[i].ft; CHECK(ft == (int)i);
        Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero(); u.have = closure(ft);
        tr[i].off(&u, &f); CHECK(!ul_trigger(&u, &f, ft)); ul_update(&u, &f, 0); CHECK(!ul_has(&u, ft));     // (a neighbour may open on the way: the edge of this one is what counts)
        tr[i].on(&u, &f);  CHECK(ul_trigger(&u, &f, ft));  CHECK(ul_update(&u, &f, 0) & UB(ft)); CHECK(ul_has(&u, ft));
        CHECK(ul_update(&u, &f, 0) == 0);                                                // and only once
        tr[i].off(&u, &f); CHECK(ul_has(&u, ft)); ul_update(&u, &f, 0); CHECK(ul_has(&u, ft));  // features never close again
        // the prerequisites hold it back
        if(ul_defs[ft].needs){ Unlock v; memset(&v, 0, sizeof v); UlFacts g = zero(); tr[i].on(&v, &g); ul_update(&v, &g, 0); CHECK(!ul_has(&v, ft) || (v.have & ul_defs[ft].needs) == ul_defs[ft].needs); }
    }
    // ARMADA needs the mayor AND gold; the mayor needs fame to be known
    { Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero(); f.mayor = 1; f.fame = 5000;
      uint32_t got = ul_update(&u, &f, 0); CHECK(got & BIT(UF_MAYOR)); CHECK(got & BIT(UF_FAME)); CHECK(!(got & BIT(UF_ARMADA)));       // no gold yet
      ul_note(&u, ULC_GOLD); got = ul_update(&u, &f, 0); CHECK((got & BIT(UF_MONEY)) && (u.have & BIT(UF_ARMADA))); }
    // prerequisites open in the same call, whatever the table order
    { Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero(); f.town = 1; f.log_n = 9; f.freed = 3; f.homes = f.shops = f.mine = 1; f.raid = 1; f.fame = 100000; f.level = 99; f.mayor = 1; f.worlds_total = 5; f.galaxies = 2; f.friend_max = 100; f.letters = 3; f.day = 30; f.season_on = 1;
      for(int c = 0; c < ULC_N; c++) u.cnt[c] = 9;
      CHECK(ul_update(&u, &f, 0) == UL_ALL && u.have == UL_ALL); }

    // ---- the first 10 minutes: only basic features open, the rest waits and then opens (nothing is lost)
    { Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero(); f.town = 1; f.log_n = 9; f.freed = 3; f.homes = f.shops = f.mine = 1; f.raid = 1; f.fame = 100000; f.level = 99; f.mayor = 1; f.worlds_total = 5; f.galaxies = 2; f.friend_max = 100; f.letters = 3; f.day = 30; f.season_on = 1;
      for(int c = 0; c < ULC_N; c++) u.cnt[c] = 9;
      uint32_t got = ul_update(&u, &f, 1), basic = 0; for(int a = 0; a < UF_N; a++) if(ul_defs[a].basic) basic |= BIT(a);
      CHECK(got == basic && u.have == basic);
      CHECK(!(u.have & (BIT(UF_COLONY) | BIT(UF_GODS) | BIT(UF_MAYOR) | BIT(UF_ARMADA) | BIT(UF_MONEY))));
      CHECK(ul_update(&u, &f, 1) == 0);
      CHECK(ul_update(&u, &f, 0) == (UL_ALL & ~basic) && u.have == UL_ALL); }
    { Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero(); f.town = 1; f.log_n = 2; ul_note(&u, ULC_TALK);                  // a gentle start: the town, the journal, the first talk
      CHECK(ul_update(&u, &f, 1) == (BIT(UF_CLOCK) | BIT(UF_JOURNAL) | BIT(UF_MEET))); CHECK(ul_popcount(u.have) == 3); }

    // ---- what is drawn: OFF hides, SHAPES shows ??? for menus only, ALL shows everything; the ! markers
    { Unlock u; memset(&u, 0, sizeof u); u.have = BIT(UF_CLOCK) | BIT(UF_MEET);
      CHECK(ul_vis(&u, UF_MEET, ULSA_OFF) == ULV_SHOWN && ul_shown(&u, UF_CLOCK, ULSA_OFF));
      CHECK(ul_vis(&u, UF_GODS, ULSA_OFF) == ULV_HIDDEN && ul_vis(&u, UF_GODS, ULSA_SHAPES) == ULV_SHAPE && ul_vis(&u, UF_GODS, ULSA_ALL) == ULV_SHOWN);
      CHECK(ul_vis(&u, UF_JOBS, ULSA_SHAPES) == ULV_HIDDEN);                          // a detail is never a silhouette
      CHECK(ul_vis(&u, UF_MEET, ULSA_SHAPES) == ULV_SHOWN);
      CHECK(ul_vis(&u, -1, ULSA_ALL) == ULV_HIDDEN && ul_vis(&u, UF_N, ULSA_ALL) == ULV_HIDDEN);
      CHECK(ul_sq_vis(&u, SQ_PEOPLE, ULSA_OFF) == ULV_SHOWN && ul_sq_vis(&u, SQ_JOURNAL, ULSA_OFF) == ULV_HIDDEN && ul_sq_vis(&u, SQ_JOURNAL, ULSA_SHAPES) == ULV_SHAPE);
      CHECK(ul_sq_vis(&u, SQ_TIPS, ULSA_OFF) == ULV_SHOWN);                           // as soon as one thing is found there is a tip to read
      CHECK(ul_sq_vis(&u, SQ_TIPS, ULSA_SHAPES) == ULV_SHOWN);
      for(int s = 0; s < SQ_TIPS; s++) CHECK(ul_feat_sq(ul_sq_feat(s)) == s);          // rows and features map both ways
      CHECK(ul_feat_sq(UF_JOBS) == -1 && ul_sq_feat(SQ_TIPS) == -1 && ul_sq_feat(-4) == -1 && ul_sq_vis(&u, 99, ULSA_ALL) == ULV_HIDDEN);
      CHECK(ul_fresh(&u) == (BIT(UF_CLOCK) | BIT(UF_MEET)));
      ul_open(&u, UF_MEET); CHECK(ul_fresh(&u) == BIT(UF_CLOCK)); ul_open(&u, UF_GODS); CHECK(ul_fresh(&u) == BIT(UF_CLOCK) && !(u.opened & BIT(UF_GODS)));    // you cannot open what is not found
      ul_open(&u, UF_CLOCK); CHECK(ul_fresh(&u) == 0); ul_open(&u, -1); ul_open(&u, 99); CHECK(u.opened == (BIT(UF_CLOCK) | BIT(UF_MEET))); }

    // ---- tips: the style for every combination of the options
    { UlOpts o; ul_opts_default(&o); CHECK(o.show_all == ULSA_OFF && o.tips == 1 && o.level == 1 && o.autopause == 1 && o.big == 0);
      for(int ft = 0; ft < UF_N; ft++){
        int w = ul_defs[ft].weight;
        for(int tips = 0; tips < 2; tips++) for(int lvl = 0; lvl < 3; lvl++) for(int ap = 0; ap < 2; ap++) for(int sa = 0; sa < 3; sa++){
            o.tips = (uint8_t)tips; o.level = (uint8_t)lvl; o.autopause = (uint8_t)ap; o.show_all = (uint8_t)sa;
            int st = ul_tip_style(&o, ft), want;
            if(!tips || sa == ULSA_ALL) want = ULT_NONE;
            else { int card = lvl == 2 || (lvl == 1 && w <= ULW_NORMAL) || (lvl == 0 && w == ULW_MAJOR); want = card && ap ? ULT_CARD : ULT_TOAST; }
            CHECK(st == want);
        } }
      ul_opts_default(&o); o.level = 0; CHECK(ul_tip_style(&o, UF_COLONY) == ULT_CARD && ul_tip_style(&o, UF_JOBS) == ULT_TOAST && ul_tip_style(&o, UF_CLOCK) == ULT_TOAST);   // FEW: only the big ones stop the game
      o.level = 2; CHECK(ul_tip_style(&o, UF_CLOCK) == ULT_CARD);                                                                                            // MANY: all of them
      o.level = 1; CHECK(ul_tip_style(&o, UF_JOBS) == ULT_CARD && ul_tip_style(&o, UF_LETTER) == ULT_TOAST);                                                 // NORMAL
      o.autopause = 0; CHECK(ul_tip_style(&o, UF_COLONY) == ULT_TOAST);                                                                                      // no auto-pause: never a card
      CHECK(ul_tip_style(&o, -1) == ULT_NONE && ul_tip_style(&o, UF_N) == ULT_NONE); }
    // the order tips are told in, the summary for a crowd, told is remembered
    { Unlock u; memset(&u, 0, sizeof u); u.have = BIT(UF_LETTER) | BIT(UF_JOBS) | BIT(UF_COLONY);
      CHECK(ul_pending(&u) == u.have && !ul_flood(&u));
      CHECK(ul_next_tip(&u) == UF_COLONY);                                              // major first
      ul_told(&u, UF_COLONY); CHECK(ul_next_tip(&u) == UF_JOBS);                       // then normal
      ul_told(&u, UF_JOBS); CHECK(ul_next_tip(&u) == UF_LETTER); ul_told(&u, UF_LETTER); CHECK(ul_next_tip(&u) == -1 && ul_pending(&u) == 0);
      u.have = BIT(UF_CLOCK) | BIT(UF_JOURNAL) | BIT(UF_MEET); u.told = 0; CHECK(!ul_flood(&u)); u.have |= BIT(UF_JOBS); CHECK(ul_flood(&u));                 // more than 3 at once: a summary
      ul_told_all(&u); CHECK(ul_pending(&u) == 0 && !ul_flood(&u)); ul_told(&u, -1); ul_told(&u, 77); CHECK(u.told == u.have); }

    // ---- counters stop at 255; wild numbers are safe
    { Unlock u; memset(&u, 0, sizeof u); for(int i = 0; i < 400; i++) ul_note(&u, ULC_TALK); CHECK(u.cnt[ULC_TALK] == 255); ul_note(&u, -1); ul_note(&u, ULC_N); CHECK(u.used); }

    // ---- the save record
    { Unlock u; memset(&u, 0, sizeof u); u.used = 1; u.have = 0x155AA55u & UL_ALL; u.told = u.have & 0xF0F0F0Fu; u.opened = u.have & 0x3333333u; for(int c = 0; c < ULC_N; c++) u.cnt[c] = (uint8_t)(c * 31 + 1);
      uint8_t b[64]; CHECK(ul_write(&u, b, 20) == 0); CHECK(ul_write(&u, b, sizeof b) == UL_BYTES);
      Unlock v; memset(&v, 0xAA, sizeof v); CHECK(ul_read(&v, b, UL_BYTES) == 0);
      CHECK(v.used && v.have == u.have && v.told == u.told && v.opened == u.opened && !memcmp(v.cnt, u.cnt, sizeof u.cnt));
      CHECK(ul_read(&v, b, UL_BYTES - 1) < 0 && ul_read(&v, b, UL_BYTES + 1) < 0 && ul_read(&v, b, 0) < 0);
      b[0] = 2; CHECK(ul_read(&v, b, UL_BYTES) < 0); b[0] = 1;                          // a later version is ignored, not misread
      // told and opened can only be a part of have; bits beyond the registry are dropped
      ul_p32(b + 9, BIT(UF_CLOCK) | 0x80000000u); ul_p32(b + 13, 0xFFFFFFFFu); ul_p32(b + 17, 0xFFFFFFFFu);
      CHECK(ul_read(&v, b, UL_BYTES) == 0 && v.have == BIT(UF_CLOCK) && v.told == BIT(UF_CLOCK) && v.opened == BIT(UF_CLOCK));
      char nm[128]; ul_names(BIT(UF_CLOCK) | BIT(UF_GODS), nm, sizeof nm); CHECK(!strcmp(nm, "clock,gods"));
      ul_names(UL_ALL, nm, 20); CHECK(strlen(nm) == 19); ul_names(0, nm, sizeof nm); CHECK(nm[0] == 0); CHECK(ul_names(1, nm, 0) == 0);
      for(int it = 0; it < 3000; it++){ uint8_t g[UL_BYTES]; for(int i = 0; i < UL_BYTES; i++) g[i] = (uint8_t)rnd(); g[0] = 1; Unlock w; if(ul_read(&w, g, UL_BYTES) == 0){ CHECK((w.have & ~UL_ALL) == 0 && (w.told & ~w.have) == 0 && (w.opened & ~w.have) == 0); ul_update(&w, &(UlFacts){0}, 0); } } }

    // ---- the .sim file: each slot has its own flags; unknown or damaged chunks are skipped; a new game in a slot keeps the others
    { SimFile f; simfile_init(&f);
      for(int s = 0; s < 3; s++){ f.slot[s].used = 1; f.slot[s].unlock.used = 1; f.slot[s].unlock.have = BIT(UF_CLOCK) | (s ? BIT(UF_COLONY) : 0) | (s == 2 ? BIT(UF_GODS) : 0); f.slot[s].unlock.told = BIT(UF_CLOCK); f.slot[s].unlock.cnt[ULC_TALK] = (uint8_t)(s + 3); }
      static uint8_t buf[SIMFILE_MAX]; size_t n = simfile_write(&f, buf, sizeof buf); CHECK(n > 0);
      static SimFile g; CHECK(simfile_read(&g, buf, n) == 0 && g.bad_chunks == 0);
      for(int s = 0; s < 3; s++){ CHECK(g.slot[s].unlock.used && g.slot[s].unlock.have == f.slot[s].unlock.have && g.slot[s].unlock.told == BIT(UF_CLOCK) && g.slot[s].unlock.cnt[ULC_TALK] == s + 3); }
      // a slot that never found anything writes no chunk and reads back as nothing
      f.slot[1].unlock.used = 0; f.slot[1].unlock.have = 0; n = simfile_write(&f, buf, sizeof buf); simfile_read(&g, buf, n); CHECK(!g.slot[1].unlock.used && g.slot[1].unlock.have == 0 && g.slot[2].unlock.have == f.slot[2].unlock.have);
      // a damaged chunk is skipped and counted
      f.slot[1].unlock.used = 1; f.slot[1].unlock.have = BIT(UF_MEET); n = simfile_write(&f, buf, sizeof buf);
      for(size_t i = 8; i + 14 < n; i++) if(!memcmp(buf + i, "UNLK", 4) && buf[i + 4] == 1){ buf[i + 14 + 9] ^= 0x40; break; }
      simfile_read(&g, buf, n); CHECK(g.bad_chunks == 1 && g.slot[1].unlock.have == 0 && g.slot[0].unlock.have == f.slot[0].unlock.have);
      // a wrong-sized chunk (an older or newer layout) is skipped
      { SimFile h; simfile_init(&h); h.slot[0].used = 1; h.slot[0].unlock.used = 1; h.slot[0].unlock.have = BIT(UF_MEET); n = simfile_write(&h, buf, sizeof buf);
        for(size_t i = 8; i + 14 < n; i++) if(!memcmp(buf + i, "UNLK", 4)){ buf[i + 5] = 2; break; }                          // chunk version 2: skipped silently
        simfile_read(&g, buf, n); CHECK(g.slot[0].unlock.have == 0 && g.bad_chunks == 0); }
      // simfile_clear_slot: a new game in slot 1 empties slot 1 only
      simfile_init(&f); for(int s = 0; s < 3; s++){ f.slot[s].used = 1; f.slot[s].unlock.used = 1; f.slot[s].unlock.have = BIT(UF_MEET); f.slot[s].cal_ticks = 1000 + s; f.slot[s].stand.used = 1; f.slot[s].stand.mayor_town = 77; }
      simfile_clear_slot(&f, 1); CHECK(!f.slot[1].used && !f.slot[1].unlock.have && !f.slot[1].stand.used && f.slot[1].cal_ticks == 0);
      CHECK(f.slot[0].used && f.slot[0].unlock.have && f.slot[0].cal_ticks == 1000 && f.slot[2].used && f.slot[2].cal_ticks == 1002 && f.slot[2].stand.mayor_town == 77);
      simfile_clear_slot(&f, -1); simfile_clear_slot(&f, 3); CHECK(f.slot[0].used && f.slot[2].used);
      CHECK(f.slot[1].god.sandbox == 0 && f.slot[1].spirit == 0);
      simfile_copy_slot(&f, 0, 1); CHECK(f.slot[1].used && f.slot[1].unlock.have == BIT(UF_MEET) && f.slot[1].cal_ticks == 1000 && f.slot[1].stand.mayor_town == 77);
      f.slot[0].unlock.have = 0; CHECK(f.slot[1].unlock.have == BIT(UF_MEET));                  // a copy, not a link
      simfile_copy_slot(&f, 2, 2); simfile_copy_slot(&f, -1, 2); simfile_copy_slot(&f, 0, 9); CHECK(f.slot[2].used);
    }

    // ---- a first-time player's story, step by step (the same facts the game gathers): what opens when, in order
    { Unlock u; memset(&u, 0, sizeof u); UlFacts f = zero(); char log[400]; log[0] = 0; uint64_t cal = 0; int order[UF_N], no = 0;
      #define STEP(txt, code) do{ code; uint32_t nw = ul_update(&u, &f, cal < UL_EARLY_TICKS); for(int a = 0; a < UF_N; a++) if(nw >> a & 1) order[no++] = a; (void)txt; }while(0)
      STEP("land with no town", f.town = 0);                                    CHECK(u.have == 0);
      STEP("walk into town", f.town = 1; cal = 10);                             CHECK(u.have == BIT(UF_CLOCK));
      STEP("a minute of news", f.log_n = 3; cal = 600);                         CHECK(u.have == (BIT(UF_CLOCK) | BIT(UF_JOURNAL)));
      STEP("first talk", ul_note(&u, ULC_TALK); cal = 900);                     CHECK(ul_has(&u, UF_MEET) && !ul_has(&u, UF_JOBS));
      STEP("second talk", ul_note(&u, ULC_TALK));                               CHECK(ul_has(&u, UF_JOBS) && !ul_has(&u, UF_NEEDS));
      STEP("free a bug in minute 3", f.freed = 1);                              CHECK(!ul_has(&u, UF_COLONY));                // waits for minute 10
      STEP("gold in minute 4", ul_note(&u, ULC_GOLD); cal = 2400);              CHECK(!ul_has(&u, UF_MONEY));
      STEP("fame 400 early", f.fame = 400);                                     CHECK(!ul_has(&u, UF_GODS) && !ul_has(&u, UF_FAME));
      STEP("minute 10", cal = UL_EARLY_TICKS);                                  CHECK(ul_has(&u, UF_COLONY) && ul_has(&u, UF_MONEY) && ul_has(&u, UF_FAME) && ul_has(&u, UF_GODS) && !ul_has(&u, UF_MAYOR));
      STEP("fame 700 and two bugs", f.fame = 700; f.mayor = 1);                 CHECK(ul_has(&u, UF_SANDBOX) && ul_has(&u, UF_MAYOR) && !ul_has(&u, UF_ARMADA));
      STEP("fame 1200", f.fame = 1200);                                         CHECK(ul_has(&u, UF_ARMADA) && !ul_has(&u, UF_WORLDS));
      STEP("a world", f.worlds_total = 1);                                      CHECK(ul_has(&u, UF_WORLDS) && !ul_has(&u, UF_GALAXY));
      (void)log; CHECK(no == ul_popcount(u.have));
      // the order each feature opened in never breaks a prerequisite
      uint32_t seen = 0; for(int i = 0; i < no; i++){ CHECK((seen & ul_defs[order[i]].needs) == ul_defs[order[i]].needs || (u.have & ul_defs[order[i]].needs) == ul_defs[order[i]].needs); seen |= BIT(order[i]); }
      CHECK(order[0] == UF_CLOCK); }

    printf("unlock_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
