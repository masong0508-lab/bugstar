// standing_test.c: standalone tests for standing.h (patch 42): the fame ladder, menus that open one at a time, the mayor, taxes, the armada,
// worlds and galaxies, the save record and its chunk in the .sim file. pc/sim/test.sh builds and runs it. No game needed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_core.h"

static int fails, checks;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static int popc(unsigned v){ int n = 0; while(v){ n += v & 1; v >>= 1; } return n; }
static uint64_t rs = 0x1234567ull;
static uint64_t rnd(void){ rs = rs * 6364136223846793005ull + 1442695040888963407ull; return rs >> 11; }
#define BIT(m) (1u << (m))

int main(void){
    Standing s; memset(&s, 0, sizeof s);

    // ---- ranks: the game's own thresholds and names
    { static const int64_t f[] = { 0, 49, 50, 149, 150, 349, 350, 699, 700, 1199, 1200, 1000000, INT64_MAX };
      static const int r[] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5 };
      for(unsigned i = 0; i < sizeof f / sizeof f[0]; i++) CHECK(stand_rank(f[i]) == r[i]);
      CHECK(stand_rank(-5) == 0);
      CHECK(!strcmp(stand_rank_name(0), "NOBODY") && !strcmp(stand_rank_name(1), "KNOWN BUG") && !strcmp(stand_rank_name(2), "BRAVE BUG") && !strcmp(stand_rank_name(3), "HERO") && !strcmp(stand_rank_name(4), "CHAMPION") && !strcmp(stand_rank_name(5), "BUG LEGEND")); }

    // ---- menus: two at the start, one more per rank. Nothing is thrown at you.
    CHECK(stand_open_mask(&s, 0) == (BIT(STM_JOURNAL) | BIT(STM_LEVEL)));
    CHECK(popc(stand_open_mask(&s, 0)) == 2);
    CHECK(stand_open_mask(&s, 49) == stand_open_mask(&s, 0));
    CHECK(stand_open_mask(&s, 50) == (BIT(STM_JOURNAL) | BIT(STM_LEVEL) | BIT(STM_PEOPLE)));
    CHECK((stand_open_mask(&s, 150) & BIT(STM_COLONY)) && !(stand_open_mask(&s, 149) & BIT(STM_COLONY)));
    CHECK((stand_open_mask(&s, 350) & BIT(STM_GODS)) && !(stand_open_mask(&s, 349) & BIT(STM_GODS)));
    CHECK((stand_open_mask(&s, 700) & BIT(STM_SANDBOX)) && !(stand_open_mask(&s, 699) & BIT(STM_SANDBOX)));
    CHECK(!(stand_open_mask(&s, 5000) & BIT(STM_HALL)) && !(stand_open_mask(&s, 5000) & BIT(STM_ARMADA)));      // no mayor: no hall, no armada, however famous
    // the order of the list is the order they were written in the game before the ladder (tests and keys depend on it)
    CHECK(STM_JOURNAL == 0 && STM_PEOPLE == 1 && STM_GODS == 2 && STM_SANDBOX == 3 && STM_LEVEL == 4 && STM_COLONY == 5);
    { uint16_t prev = 0; for(int64_t f = 0; f <= 1500; f++){ uint16_t m = stand_open_mask(&s, f); CHECK((m & prev) == prev); prev = m; } }    // fame never closes a menu
    CHECK(stand_next_menu(stand_open_mask(&s, 0)) == STM_PEOPLE);
    CHECK(stand_next_menu(stand_open_mask(&s, 60)) == STM_COLONY);
    CHECK(stand_next_menu(stand_open_mask(&s, 200)) == STM_GODS);
    CHECK(stand_next_menu(stand_open_mask(&s, 400)) == STM_SANDBOX || stand_next_menu(stand_open_mask(&s, 400)) == STM_HALL);
    CHECK(stand_next_menu(0xFF) == -1);
    { char t[64]; stand_lock_text(&s, STM_COLONY, 0, t, sizeof t); CHECK(!strcmp(t, "FAME 150 (BRAVE BUG)"));
      stand_lock_text(&s, STM_GODS, 10, t, sizeof t); CHECK(!strcmp(t, "FAME 350 (HERO)"));
      stand_lock_text(&s, STM_HALL, 800, t, sizeof t); CHECK(strstr(t, "MAYOR") != NULL);
      stand_lock_text(&s, STM_ARMADA, 100, t, sizeof t); CHECK(strstr(t, "1200") != NULL);
      stand_lock_text(&s, STM_PEOPLE, 100, t, sizeof t); CHECK(t[0] == 0);                                       // open: nothing to say
      stand_lock_text(&s, STM_COLONY, 0, t, 5); CHECK(strlen(t) == 4); }                                         // short buffer: cut, never overrun
    for(int m = 0; m < STM_N; m++){ CHECK(stand_menu_name(m)[0] && stand_menu_card(m)[0]); CHECK(strlen(stand_menu_card(m)) <= 52); }

    // ---- the mayor: a CHAMPION in a real town (2 bugs or more), once
    CHECK(!stand_can_appoint(&s, 699, 8)); CHECK(!stand_can_appoint(&s, 700, 1)); CHECK(!stand_can_appoint(&s, 700, 0)); CHECK(stand_can_appoint(&s, 700, 2));
    stand_appoint(&s, 0xABCDEF01u, "A VERY LONG PLANET NAME INDEED", 5000);
    CHECK(stand_is_mayor(&s) && strlen(s.home) == ST_NAME_MAX && s.mayor_cal == 5000 && s.tax_cal == 5000);
    CHECK(!stand_can_appoint(&s, 5000, 12));
    CHECK(stand_open_mask(&s, 700) & BIT(STM_HALL)); CHECK(!(stand_open_mask(&s, 1199) & BIT(STM_ARMADA))); CHECK(stand_open_mask(&s, 1200) == (BIT(STM_N) - 1));
    { Standing z; memset(&z, 0, sizeof z); stand_appoint(&z, 0, "X", 0); CHECK(stand_is_mayor(&z)); }           // a town hash of 0 still counts

    // ---- taxes
    CHECK(stand_rate(8, 200, 50) == 8 * 2 * 50 / 2); CHECK(stand_rate(8, 100, 50) == 8 * 1 * 50 / 2); CHECK(stand_rate(0, 255, 50) == 0); CHECK(stand_rate(-3, -4, 50) == 0);
    CHECK(stand_rate(16, 255, 120) < 0x1000000u); CHECK(stand_rate(1000000, 255, 1000000000ull) == 0xFFFFFFu);
    s.tax_rate = 400;
    CHECK(stand_tax_days(&s, 5000) == 0); CHECK(stand_tax_days(&s, 5000 + ST_DAY_TICKS - 1) == 0); CHECK(stand_tax_days(&s, 5000 + ST_DAY_TICKS) == 1);
    CHECK(stand_tax_days(&s, 5000 + 100 * (uint64_t)ST_DAY_TICKS) == ST_TAX_DAYS_MAX); CHECK(stand_tax_days(&s, 10) == 0);                  // piles up to a cap; a clock behind the tax clock gives nothing
    CHECK(stand_tax_due(&s, 5000 + 3 * (uint64_t)ST_DAY_TICKS, 50) == 1200);
    { uint64_t g = stand_tax_take(&s, 5000 + 3 * (uint64_t)ST_DAY_TICKS, 50); CHECK(g == 1200); CHECK(stand_tax_take(&s, 5000 + 3 * (uint64_t)ST_DAY_TICKS, 50) == 0); CHECK(s.tax_cal == 5000 + 3 * (uint64_t)ST_DAY_TICKS); }
    { Standing z; memset(&z, 0, sizeof z); z.tax_rate = 99; CHECK(stand_tax_due(&z, 1000000, 50) == 0); }     // no mayor, no taxes
    s.worlds_total = 5; CHECK(stand_tribute(&s, 50) == 500); s.tax_cal = 0; CHECK(stand_tax_due(&s, 2 * (uint64_t)ST_DAY_TICKS, 50) == 2 * (400 + 500)); s.worlds_total = 0;
    CHECK(stand_feast_cost(50) == 1500);

    // ---- the armada: a ship costs more as the fleet grows; gold and your level limit it
    memset(&s, 0, sizeof s); stand_appoint(&s, 77, "HOME", 0);
    CHECK(stand_ship_cost(&s, 50) == 500); s.ships = 4; CHECK(stand_ship_cost(&s, 50) == 550); s.ships = 400; CHECK(stand_ship_cost(&s, 50) == 5500); s.ships = 0;
    CHECK(stand_command(0) == 5 && stand_command(3) == 5 && stand_command(5) == 5 && stand_command(1000) == 1000);
    { int64_t gold = 100000; uint64_t got = stand_buy(&s, &gold, 1000, 50, 10);
      uint64_t sum = 0; for(int i = 0; i < 10; i++) sum += 50 * (10 + (uint64_t)i / 4);
      CHECK(got == 10 && s.ships == 10 && gold == 100000 - (int64_t)sum && s.gold_spent == sum); }
    { Standing t = s; uint64_t cnt = 0, price = stand_buy_price(&t, 1000, 50, 5, &cnt); int64_t gold = (int64_t)price; CHECK(cnt == 5); CHECK(stand_buy(&t, &gold, 1000, 50, 5) == 5 && gold == 0);
      t = s; gold = (int64_t)price - 1; CHECK(stand_buy(&t, &gold, 1000, 50, 5) == 4 && gold >= 0 && gold < 700); }       // one gold short: one ship fewer, and the change stays
    { Standing t; memset(&t, 0, sizeof t); int64_t gold = INT64_MAX; CHECK(stand_buy(&t, &gold, 20, 50, 1000) == 20 && t.ships == 20); CHECK(stand_buy(&t, &gold, 20, 50, 1000) == 0); }   // level 20 leads 20 ships
    { Standing t; memset(&t, 0, sizeof t); int64_t gold = 499; CHECK(stand_buy(&t, &gold, 1000, 50, 1) == 0 && gold == 499 && t.ships == 0); gold = -5; CHECK(stand_buy(&t, &gold, 1000, 50, 1) == 0); }
    { Standing t; memset(&t, 0, sizeof t); t.ships = UINT64_MAX - 1; CHECK(stand_ship_cost(&t, UINT64_MAX / 2) == UINT64_MAX); uint64_t c = 0; stand_buy_price(&t, UINT64_MAX, 1, 5, &c); CHECK(c == 1); }   // saturates, never wraps
    CHECK(!strcmp(stand_fleet_name(0), "NO SHIPS") && !strcmp(stand_fleet_name(1), "ESCORT") && !strcmp(stand_fleet_name(10), "SQUADRON") && !strcmp(stand_fleet_name(100), "FLEET") && !strcmp(stand_fleet_name(1000), "ARMADA") && !strcmp(stand_fleet_name(10000), "GRAND ARMADA") && !strcmp(stand_fleet_name(1000000), "STAR ARMADA"));
    { uint64_t pts[] = { 0, 1, 9, 10, 99, 100, 999, 1000, 9999, 10000, 999999, 1000000 }; int want[] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6 }; for(unsigned i = 0; i < sizeof pts / sizeof pts[0]; i++) CHECK(stand_fleet_tier(pts[i]) == want[i]); }

    // ---- worlds and galaxies
    memset(&s, 0, sizeof s); stand_appoint(&s, 77, "HOME", 0);
    { int64_t gold = INT64_MAX / 4; int32_t fg = 0;
      CHECK(stand_settle(&s, &gold, &fg, 50) == -4);                                                           // no ships: not an armada yet
      s.ships = 7; CHECK(stand_settle(&s, &gold, &fg, 50) == -1);                                              // 8 ships for the first world
      s.ships = 8; { int64_t poor = 100; CHECK(stand_settle(&s, &poor, &fg, 50) == -2 && poor == 100 && s.worlds == 0); }
      CHECK(stand_settle(&s, &gold, &fg, 50) == 0 && s.worlds == 1 && s.worlds_total == 1 && fg == 20); CHECK(gold == INT64_MAX / 4 - (int64_t)(50 * 120 * 1));
      CHECK(stand_world_ships(&s) == 14); CHECK(stand_world_cost(&s, 50) == 50ull * 120 * 4);
      uint64_t pc = 0; for(int n = 0; n < 12; n++){ s.worlds = (uint32_t)n; uint64_t c = stand_world_cost(&s, 50); CHECK(c > pc); pc = c; } s.worlds = 1;
      s.ships = 1000; for(int n = 1; n < 12; n++) CHECK(stand_settle(&s, &gold, &fg, 50) == 0);
      CHECK(s.worlds == 12 && s.worlds_total == 12 && stand_galaxy_open(&s)); CHECK(stand_settle(&s, &gold, &fg, 50) == -3 && s.worlds == 12);            // a galaxy holds 12
      s.ships = 99; CHECK(stand_cross(&s, &gold, &fg, 50) == -1);
      s.ships = 100; { int64_t poor = 1000; CHECK(stand_cross(&s, &poor, &fg, 50) == -2); }
      int64_t g0 = gold; CHECK(stand_cross(&s, &gold, &fg, 50) == 0 && s.worlds == 0 && s.galaxies == 1 && s.worlds_total == 12 && fg == 500 && g0 - gold == 50 * 40000);
      CHECK(stand_galaxy_ships(&s) == 400); CHECK(!stand_galaxy_open(&s)); CHECK(stand_cross(&s, &gold, &fg, 50) == -3);       // a new galaxy is empty: no second jump
      CHECK(stand_world_cost(&s, 50) == 2 * 50ull * 120);                                                      // the second galaxy costs double
      CHECK(stand_galaxy_cost(&s, 50) == 3 * 50ull * 40000); }
    { Standing t; memset(&t, 0, sizeof t); t.galaxies = 1000; CHECK(stand_galaxy_ships(&t) > 0 && stand_world_cost(&t, 120) > 0 && stand_galaxy_cost(&t, 120) > 0); }      // far past any real game: still finite
    for(int g = 0; g < 25; g++){ Standing t; memset(&t, 0, sizeof t); t.galaxies = (uint32_t)g; if(g) CHECK(stand_galaxy_ships(&t) >= 100 && stand_galaxy_cost(&t, 120) >= 40000 * 120ull); }

    // ---- world names: the same every time, readable, and mostly different
    { char a[32], b[32]; int distinct = 0; static char seen[300][32];
      for(uint64_t n = 0; n < 300; n++){ stand_world_name(&s, n, a, sizeof a); stand_world_name(&s, n, b, sizeof b); CHECK(!strcmp(a, b)); size_t l = strlen(a); CHECK(l >= 4 && l <= 14);
          for(size_t k = 0; k < l; k++) CHECK((a[k] >= 'A' && a[k] <= 'Z') || a[k] == ' ' || (a[k] >= '0' && a[k] <= '9'));
          int dup = 0; for(int k = 0; k < distinct; k++) if(!strcmp(seen[k], a)) dup = 1; if(!dup) strcpy(seen[distinct++], a); }
      CHECK(distinct >= 250);
      Standing t = s; t.mayor_town ^= 0x55; stand_world_name(&s, 3, a, sizeof a); stand_world_name(&t, 3, b, sizeof b); CHECK(strcmp(a, b) != 0);
      char tiny[3]; stand_world_name(&s, 7, tiny, sizeof tiny); CHECK(strlen(tiny) <= 2); }
    for(int i = 0; i < 6; i++) CHECK(strlen(stand_scale_line(&s)) <= 56);
    { Standing t; memset(&t, 0, sizeof t); CHECK(!strcmp(stand_scale_line(&t), "ONE TOWN. THE SKY HOLDS A LOT MORE.")); }

    // ---- the save record
    { uint8_t buf[256]; Standing a, b; size_t n;
      memset(&a, 0, sizeof a); a.used = 1; a.flags = 3; a.told = 0x7F; a.mayor_town = 0xDEADBEEFu; strcpy(a.home, "GRUNDLE"); a.mayor_cal = 123456789012ull; a.tax_cal = 123456789999ull; a.ships = 1234567890123ull;
      a.worlds_total = 25; a.worlds = 7; a.galaxies = 2; a.tax_rate = 4321; a.feasts = 9; a.gold_spent = 987654321987ull;
      n = stand_write(&a, buf, sizeof buf); CHECK(n == ST_BYTES); CHECK(stand_write(&a, buf, ST_BYTES - 1) == 0);
      CHECK(stand_read(&b, buf, n) == 0); CHECK(!memcmp(&a, &b, sizeof a) || (b.used == a.used && b.flags == a.flags && b.told == a.told && b.mayor_town == a.mayor_town && !strcmp(a.home, b.home) && b.mayor_cal == a.mayor_cal && b.tax_cal == a.tax_cal && b.ships == a.ships && b.worlds_total == a.worlds_total && b.worlds == a.worlds && b.galaxies == a.galaxies && b.tax_rate == a.tax_rate && b.feasts == a.feasts && b.gold_spent == a.gold_spent));
      CHECK(stand_read(&b, buf, n - 1) == -1); CHECK(stand_read(&b, buf, n + 1) == -1); CHECK(stand_read(&b, buf, 0) == -1);
      // a damaged record cannot hand out more than the rules allow
      { Standing c = a; c.worlds = 9999; c.told = 0xFFFF; c.tax_cal = 1; n = stand_write(&c, buf, sizeof buf); stand_read(&b, buf, n); CHECK(b.worlds == ST_GALAXY_WORLDS); CHECK(b.told < BIT(STM_N)); CHECK(b.tax_cal >= b.mayor_cal); }
      { Standing c = a; c.mayor_town = 0; n = stand_write(&c, buf, sizeof buf); stand_read(&b, buf, n); CHECK(b.ships == 0 && b.worlds == 0 && b.worlds_total == 0 && b.galaxies == 0); }     // no mayor, no armada
      { Standing c = a; c.home[2] = 3; n = stand_write(&c, buf, sizeof buf); stand_read(&b, buf, n); CHECK(b.home[2] == '?'); }
      // fuzz: random states in, the same states out
      for(int it = 0; it < 3000; it++){ Standing r; memset(&r, 0, sizeof r); r.used = 1; r.flags = (uint8_t)(rnd() & 3); r.told = (uint16_t)(rnd() & (BIT(STM_N) - 1)); r.mayor_town = (uint32_t)rnd() | 1;
          int l = (int)(rnd() % 16); for(int k = 0; k < l; k++) r.home[k] = (char)('A' + rnd() % 26); r.mayor_cal = rnd() % 100000000; r.tax_cal = r.mayor_cal + rnd() % 100000; r.ships = rnd() * rnd(); r.worlds = (uint32_t)(rnd() % 13); r.galaxies = (uint32_t)(rnd() % 50); r.worlds_total = r.worlds + r.galaxies * 12ull;
          r.tax_rate = (uint32_t)rnd(); r.feasts = (uint32_t)rnd(); r.gold_spent = rnd() * rnd(); n = stand_write(&r, buf, sizeof buf); CHECK(n == ST_BYTES); CHECK(stand_read(&b, buf, n) == 0);
          CHECK(b.told == r.told && b.mayor_town == r.mayor_town && !strcmp(b.home, r.home) && b.ships == r.ships && b.worlds == r.worlds && b.galaxies == r.galaxies && b.worlds_total == r.worlds_total && b.tax_rate == r.tax_rate && b.gold_spent == r.gold_spent && b.tax_cal == r.tax_cal); } }

    // ---- in the .sim file: its own chunk, per slot; an old file has none; a damaged chunk is skipped and the rest survives
    { static SimFile f, g; static uint8_t buf[SIMFILE_MAX]; simfile_init(&f);
      f.slot[1].used = 1; f.slot[1].cal_ticks = 777; f.slot[1].stand.used = 1; f.slot[1].stand.told = 0x13; stand_appoint(&f.slot[1].stand, 4242, "ZORBIX", 100); f.slot[1].stand.ships = 321; f.slot[1].stand.worlds = 3; f.slot[1].stand.worlds_total = 15; f.slot[1].stand.galaxies = 1;
      f.slot[0].used = 1; f.slot[0].cal_ticks = 5;                                                              // a slot with no standing
      size_t n = simfile_write(&f, buf, sizeof buf); CHECK(n > 0);
      CHECK(simfile_read(&g, buf, n) == 0 && g.bad_chunks == 0);
      CHECK(g.slot[1].stand.used && g.slot[1].stand.ships == 321 && g.slot[1].stand.worlds == 3 && g.slot[1].stand.galaxies == 1 && g.slot[1].stand.mayor_town == 4242 && !strcmp(g.slot[1].stand.home, "ZORBIX") && g.slot[1].cal_ticks == 777);
      CHECK(!g.slot[0].stand.used && g.slot[0].cal_ticks == 5 && !g.slot[2].used);
      // find the STND chunk and flip a byte inside it
      { size_t at = 8; size_t hit = 0; while(at + 14 <= n){ uint32_t len = (uint32_t)(buf[at + 6] | (buf[at + 7] << 8) | (buf[at + 8] << 16) | ((uint32_t)buf[at + 9] << 24)); if(!memcmp(buf + at, "STND", 4)) hit = at; at += 14 + len; }
        CHECK(hit > 0); static uint8_t bad[SIMFILE_MAX]; memcpy(bad, buf, n); bad[hit + 14 + 20] ^= 0x40;
        CHECK(simfile_read(&g, bad, n) == 0 && g.bad_chunks == 1 && !g.slot[1].stand.used && g.slot[1].cal_ticks == 777); }                                  // only the bad chunk is lost
      // a chunk of the wrong length is refused, not half-read
      { static uint8_t cut[SIMFILE_MAX]; memcpy(cut, buf, n); size_t at = 8, hit = 0; while(at + 14 <= n){ uint32_t len = (uint32_t)(buf[at + 6] | (buf[at + 7] << 8) | (buf[at + 8] << 16) | ((uint32_t)buf[at + 9] << 24)); if(!memcmp(buf + at, "STND", 4)) hit = at; at += 14 + len; }
        cut[hit + 4] = 9; CHECK(simfile_read(&g, cut, n) == 0 && !g.slot[1].stand.used); }                         // slot 9 does not exist: skipped quietly
      // the older file shape (no STND at all) reads as before
      { static SimFile h; simfile_init(&h); h.slot[0].used = 1; h.slot[0].cal_ticks = 9; size_t m = simfile_write(&h, buf, sizeof buf); CHECK(simfile_read(&g, buf, m) == 0 && g.slot[0].cal_ticks == 9 && !g.slot[0].stand.used); } }

    // ---- the journal line
    { static Sim w; sim_init(&w, 5, 4, 0, 0); SimEvent e; char t[80]; memset(&e, 0, sizeof e);
      e.kind = SIME_STAND; e.a = 1; e.b = STM_PEOPLE; sim_event_text(&w, &e, t, sizeof t); CHECK(!strcmp(t, "PEOPLE IS OPEN TO YOU"));
      e.a = 2; sim_event_text(&w, &e, t, sizeof t); CHECK(strstr(t, "MAYOR") != NULL);
      e.a = 3; e.b = 4; sim_event_text(&w, &e, t, sizeof t); CHECK(strstr(t, "ARMADA") != NULL);
      e.a = 4; e.b = 3; sim_event_text(&w, &e, t, sizeof t); CHECK(strstr(t, "WORLD 3") != NULL);
      e.a = 5; e.b = 1; sim_event_text(&w, &e, t, sizeof t); CHECK(strstr(t, "GALAXY 2") != NULL);
      e.a = 99; sim_event_text(&w, &e, t, sizeof t); CHECK(t[0]);
      uint64_t h1 = sim_hash(&w); sim_log(&w, SIME_STAND, 0, 2, 0); CHECK(sim_hash(&w) != h1); }              // it is a real event: it lands in the log

    // ---- a journey: fame and gold grow, the menus follow, the armada is reachable. Prints how long the taxes alone would take (see TUNING.md).
    { Standing j; memset(&j, 0, sizeof j); int64_t fame = 0, gold = 0; uint64_t level = 1000, unit = 120, cal = 0; int opened[STM_N]; int when[STM_N]; int mayor_day = -1;
      for(int i = 0; i < STM_N; i++){ opened[i] = 0; when[i] = -1; }
      int day = 0; int first_world = -1, galaxy_day = -1, ships100 = -1;
      for(day = 0; day < 40000 && galaxy_day < 0; day++){
          cal = (uint64_t)day * ST_DAY_TICKS;
          if(day < 200) fame += 8; else if(day < 1000) fame += 2;                                                     // a player who keeps adventuring early, then eases off
          if(stand_can_appoint(&j, fame, 8)){ stand_appoint(&j, 99, "HOME", cal); mayor_day = day; }
          uint16_t m = stand_open_mask(&j, fame); for(int i = 0; i < STM_N; i++) if((m >> i & 1) && !opened[i]){ opened[i] = 1; when[i] = day; }
          if(stand_is_mayor(&j)){ j.tax_rate = stand_rate(8, 190, unit); gold += (int64_t)stand_tax_take(&j, cal + ST_DAY_TICKS, unit); j.tax_cal = cal + ST_DAY_TICKS; }
          if(m & BIT(STM_ARMADA)){
              int32_t fg = 0; uint64_t need = stand_galaxy_open(&j) ? stand_galaxy_ships(&j) : stand_world_ships(&j);             // a sensible captain buys what the next step asks for
              if(j.ships < need) stand_buy(&j, &gold, level, unit, need - j.ships);
              while(stand_settle(&j, &gold, &fg, unit) == 0){ fame += fg; if(first_world < 0) first_world = day; if(j.worlds < ST_GALAXY_WORLDS && j.ships < stand_world_ships(&j)) break; }
              if(stand_cross(&j, &gold, &fg, unit) == 0){ fame += fg; galaxy_day = day; } }
          if(j.ships >= 100 && ships100 < 0) ships100 = day; }
      CHECK(opened[STM_JOURNAL] && when[STM_JOURNAL] == 0 && when[STM_LEVEL] == 0);
      CHECK(when[STM_PEOPLE] > 0 && when[STM_PEOPLE] <= when[STM_COLONY] && when[STM_COLONY] <= when[STM_GODS] && when[STM_GODS] <= when[STM_SANDBOX]);        // they come in order
      CHECK(mayor_day >= when[STM_SANDBOX] && when[STM_HALL] == mayor_day && when[STM_ARMADA] >= when[STM_HALL]);
      CHECK(first_world > 0 && galaxy_day > first_world);
      printf("journey (level 1000, 8 happy bugs, fame +8 a day for 200 days then +2, taxes are the only gold): people day %d, colony %d, god powers %d, mayor %d, armada %d, first world %d, 100 ships %d, first galaxy crossed day %d\n",
             when[STM_PEOPLE], when[STM_COLONY], when[STM_GODS], mayor_day, when[STM_ARMADA], first_world, ships100, galaxy_day); }

    printf("standing_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
