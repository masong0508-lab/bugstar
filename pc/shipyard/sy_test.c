// sy_test.c: the ship editor's own tests (no game code): recipe and share code round trips, determinism, stats bounds,
// a random-edit fuzz (every row, every action, random bytes) that must never crash or leave a bad design, and the game numbers.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sy_design.h"
#include "sy_files.h"
static int fails;
#define CHECK(c, ...) do { if(!(c)){ printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while(0)
static void bounds(const SyDesign *d, const char *what){
    SyStats s; sy_stats(d, &s);
    for(int i = 0; i < SY_NSTAT; i++) CHECK(s.v[i] >= 0 && s.v[i] <= 9999, "%s: stat %s = %d", what, sy_stat_name[i], s.v[i]);
    CHECK(s.mass > 0 && s.mass < 100000 && s.power_gen >= 0 && s.heat >= 0, "%s: mass %d gen %d heat %d", what, s.mass, s.power_gen, s.heat);
    SyGame g; sy_game(d, &g);
    CHECK(g.speed >= 28 && g.speed <= 90 && g.turn >= 40 && g.turn <= 170 && g.armour >= 40 && g.armour <= 130 && g.gun >= 0 && g.gun <= 24 && g.hold >= 0 && g.hold <= 60 && g.fuel >= 0 && g.fuel <= 200 && g.accel >= 4 && g.accel <= 96, "%s: game numbers out of range", what);
    static SyMesh m; sy_mesh(d, &m); CHECK(m.nv > 0 && m.nv <= SY_MAXV && m.nf > 0 && m.nf <= SY_MAXF, "%s: mesh %d %d", what, m.nv, m.nf);
    for(int i = 0; i < m.nf; i++) for(int k = 0; k < m.f[i].n; k++) CHECK(m.f[i].v[k] < m.nv, "%s: face points past the end", what);
}
static void trip(const SyDesign *d, const char *what){
    uint8_t b[SY_RECIPE_MAX]; int n = sy_encode(d, b, sizeof b); CHECK(n > 0 && n <= SY_RECIPE_MAX, "%s: encode %d", what, n);
    SyDesign e; CHECK(sy_decode(b, n, &e), "%s: decode", what); SyDesign f = *d; sy_fix(&f);
    uint8_t b2[SY_RECIPE_MAX]; int n2 = sy_encode(&e, b2, sizeof b2); CHECK(n == n2 && !memcmp(b, b2, n), "%s: recipe round trip differs", what);
    CHECK(!strcmp(e.name, f.name) && e.nsec == f.nsec && e.ngun == f.ngun && e.paint == f.paint && !memcmp(e.emblem, f.emblem, 8) && !memcmp(e.sec, f.sec, sizeof e.sec) && !memcmp(e.gun, f.gun, sizeof e.gun), "%s: fields differ after round trip", what);
    char code[400]; CHECK(sy_code(d, code, sizeof code), "%s: code", what); SyDesign g; CHECK(sy_uncode(code, &g) && sy_same(&g, d), "%s: share code round trip (%s)", what, code);
    for(char *c = code + 5; *c; c++) CHECK((*c >= 'A' && *c <= 'Z') || (*c >= '2' && *c <= '9'), "%s: odd letter in code", what);
    char lc[400]; strcpy(lc, code); for(char *c = lc; *c; c++) if(*c >= 'A' && *c <= 'Z') *c += 32; SyDesign h; CHECK(sy_uncode(lc, &h) && sy_same(&h, d), "%s: lower-case code", what);
    if(strlen(code) > 8){ code[7] = code[7] == 'A' ? 'B' : 'A'; SyDesign k; CHECK(!sy_uncode(code, &k) || sy_same(&k, d), "%s: a changed code was accepted as a different ship", what); }
}
int main(void){
    SyDesign d; sy_default(&d); bounds(&d, "default"); trip(&d, "default");
    { uint8_t b[SY_RECIPE_MAX]; int n = sy_encode(&d, b, sizeof b); printf("default recipe: %d bytes\n", n); CHECK(n < 100, "recipe too big %d", n); }
    for(int i = 0; i < SY_NPRESET; i++){ SyDesign p = d; sy_preset(&p, i); char w[40]; snprintf(w, sizeof w, "preset %s", sy_preset_name[i]); bounds(&p, w); trip(&p, w);
        SyStats s; sy_stats(&p, &s); char code[400]; sy_code(&p, code, sizeof code);
        printf("%-12s speed %3d turn %3d armour %3d shield %3d fire %3d range %3d hold %3d fuel %3d crew %2d cost %4d threat %4d warn %d  %d chars\n", sy_preset_name[i], s.v[0], s.v[1], s.v[2], s.v[3], s.v[4], s.v[5], s.v[6], s.v[7], s.v[8], s.v[9], s.v[10], s.warn, (int)strlen(code)); }
    // determinism: the dice from the same seed give the same ship, and hashes are stable
    { uint64_t a = 77, b = 77; SyDesign x = d, y = d; for(int i = 0; i < 50; i++){ sy_random(&x, &a); sy_random(&y, &b); CHECK(sy_same(&x, &y) && sy_hash(&x) == sy_hash(&y), "dice not deterministic"); bounds(&x, "dice"); trip(&x, "dice"); } }
    // the dice make sensible ships (mostly without warnings about power or heat)
    { uint64_t r = 5; int bad = 0; for(int i = 0; i < 200; i++){ SyDesign x = d; sy_random(&x, &r); SyStats s; sy_stats(&x, &s); if(s.warn & (SYWN_POWER | SYWN_HEAT)) bad++; } printf("dice: %d of 200 with power or heat warnings\n", bad); CHECK(bad < 40, "dice too often unbalanced"); }
    // random bytes: decode must not crash; random structs: fix makes them valid
    { uint64_t r = 99; for(int i = 0; i < 20000; i++){ uint8_t b[SY_RECIPE_MAX]; int n = 6 + (int)(sy_rng(&r) % (SY_RECIPE_MAX - 6)); for(int k = 0; k < n; k++) b[k] = (uint8_t)sy_rng(&r); if(i & 1){ b[0] = 0x53; b[1] = 1; }
        SyDesign x; if(sy_decode(b, n, &x)){ bounds(&x, "random bytes"); } }
      for(int i = 0; i < 3000; i++){ SyDesign x; uint8_t *p = (uint8_t*)&x; for(size_t k = 0; k < sizeof x; k++) p[k] = (uint8_t)sy_rng(&r); sy_fix(&x); bounds(&x, "random struct"); trip(&x, "random struct"); }
      for(int i = 0; i < 2000; i++){ char c[200]; int n = 5 + (int)(sy_rng(&r) % 150); strcpy(c, "SHIP-"); for(int k = 5; k < n; k++) c[k] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"[sy_rng(&r) % 32]; c[n] = 0; SyDesign x; if(sy_uncode(c, &x)) bounds(&x, "random code"); } }
    // random-edit fuzz through the editor's own rows: 200k edits, undo/redo, all actions
    { uint64_t r = 1234; SyDesign x = d; SyCursor c = {0, 0, 1, 0}; static SyHist h; sy_hist_init(&h, &x); long edits = 0;
      for(int it = 0; it < 200000; it++){ int tab = (int)(sy_rng(&r) % SY_NTAB), ids[64]; c.advanced = (int)(sy_rng(&r) & 1); int n = sy_rows(tab, &x, &c, ids); CHECK(n > 0 && n < 64, "rows %d", n); int id = ids[sy_rng(&r) % n];
          char v[64]; sy_row_value(id, &x, &c, v, sizeof v); const char *l = sy_row_label(id), *hl = sy_row_help(id); CHECK(l[0] && strlen(hl) <= 60, "row %d label/help (%s)", id, hl); SyPart p; sy_row_part(id, &x, &c, &p);
          int ch; int k = (int)(sy_rng(&r) % 10);
          if(k < 7) ch = sy_change(&x, &c, id, (int)(sy_rng(&r) % 5) - 2); else if(k < 8) ch = sy_action(&x, &c, id);
          else if(k < 9){ ch = 0; sy_hist_undo(&h, &x); } else { ch = 0; sy_hist_redo(&h, &x); }
          if(ch){ sy_hist_push(&h, &x); edits++; } sy_cursor_fix(&x, &c);
          CHECK(x.nsec >= 1 && x.nsec <= SY_MAXSEC && x.ngun <= SY_MAXGUN && c.sec >= 0 && c.sec < x.nsec && (x.ngun == 0 || (c.gun >= 0 && c.gun < x.ngun)), "edit left a bad design (id %d)", id);
          if(c.sec >= x.nsec) c.sec = x.nsec - 1; if(c.gun >= x.ngun) c.gun = x.ngun ? x.ngun - 1 : 0;
          if((it & 255) == 0){ bounds(&x, "fuzz"); trip(&x, "fuzz"); }
          if(fails > 20) break; }
      printf("fuzz: 200000 random edits (%ld changed the ship), no crash\n", edits); }
    // undo / redo
    { SyDesign x = d; SyHist *h = malloc(sizeof *h); sy_hist_init(h, &x); SyCursor c = {0, 0, 0, 0}; sy_change(&x, &c, SYR_LEN, 1); sy_hist_push(h, &x); sy_change(&x, &c, SYR_WING, 1); sy_hist_push(h, &x);
      SyDesign y; CHECK(sy_hist_undo(h, &y) && y.wing == d.wing && y.sec[0].len == d.sec[0].len + 1, "undo 1"); CHECK(sy_hist_undo(h, &y) && sy_same(&y, &d), "undo 2"); CHECK(!sy_hist_undo(h, &y), "undo past start");
      CHECK(sy_hist_redo(h, &y) && sy_hist_redo(h, &y) && sy_same(&y, &x), "redo"); free(h); }
    // stats move the right way: armour plates make it tougher and slower; more engines faster; more reactor fixes underpowered
    { SyDesign a = d, b = d; SyCursor c = {0, 0, 0, 0}; for(int i = 0; i < 5; i++) sy_change(&b, &c, SYR_ARMALL, 1); SyStats sa, sb; sy_stats(&a, &sa); sy_stats(&b, &sb);
      CHECK(sb.v[SYST_ARMOUR] > sa.v[SYST_ARMOUR] && sb.v[SYST_SPEED] < sa.v[SYST_SPEED] && sb.mass > sa.mass, "armour trade-off"); SyGame ga, gb; sy_game(&a, &ga); sy_game(&b, &gb); CHECK(gb.armour < ga.armour && gb.speed <= ga.speed, "game: armour plates (dmg %d->%d speed %d->%d)", ga.armour, gb.armour, ga.speed, gb.speed);
      SyDesign e = d; e.neng = 6; e.reactor = 5; SyStats se; sy_stats(&e, &se); CHECK(se.v[SYST_SPEED] > sa.v[SYST_SPEED], "more engines faster");
      SyDesign u = d; u.ngun = 6; for(int i = 0; i < 6; i++) u.gun[i] = (SyGun){SYGK_BEAM, 2, 0, SYGP_TOP}; u.reactor = 0; SyStats su; sy_stats(&u, &su); CHECK(su.warn & SYWN_POWER && su.warn & SYWN_HEAT, "beams with no reactor warn");
      char w[100]; CHECK(sy_warn_text(su.warn, 0, w, sizeof w) && strstr(w, "UNDERPOWERED"), "warning words"); }
    // files next to a save
    { const char *sv = "/tmp/sy_test_dir/x.sav"; system("rm -rf /tmp/sy_test_dir; mkdir -p /tmp/sy_test_dir"); SyDesign x = d, y; sy_preset(&x, 3); strcpy(x.name, "TEST SHIP");
      CHECK(!sy_slot_load(sv, 0, &y), "old save: no design"); CHECK(sy_slot_save(sv, 0, &x) && sy_slot_load(sv, 0, &y) && sy_same(&x, &y), "slot file"); sy_slot_copy(sv, 0, 2); CHECK(sy_slot_load(sv, 2, &y), "slot copy"); sy_slot_erase(sv, 0); CHECK(!sy_slot_load(sv, 0, &y), "slot erase");
      CHECK(sy_lib_save(sv, &x), "lib save"); char nm[SY_LIBMAX][SY_NAME + 1]; int n = sy_lib_list(sv, nm, SY_LIBMAX); CHECK(n == 1 && !strcmp(nm[0], "TEST SHIP"), "lib list %d", n); CHECK(sy_lib_load(sv, nm[0], &y) && sy_same(&x, &y), "lib load");
      char code[400], back[400]; sy_code(&x, code, sizeof code); CHECK(sy_code_write(sv, code) && sy_code_read(sv, back, sizeof back) && !strcmp(code, back), "code file");
      FILE *f = fopen("/tmp/sy_test_dir/x.slot0.ship", "wb"); fputs("garbage", f); fclose(f); CHECK(!sy_slot_load(sv, 0, &y), "a broken file is refused"); }
    if(fails){ printf("sy_test: %d FAILED\n", fails); return 1; }
    printf("sy_test: all good\n"); return 0;
}
