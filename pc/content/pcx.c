// pcx.c: pages of text for the PC game's world screen, made from pc_content (see pcx.h).
#include <stdio.h>
#include <string.h>
#include "pc_content.h"
#include "pcx.h"

static uint32_t hi_uni[PCX_UNI_HI], hi_cx[PCX_AX_HI], hi_cy[PCX_AX_HI];
void pcx_hi_set(const uint32_t *u, const uint32_t *x, const uint32_t *y){
    if(u){ memcpy(hi_uni, u, sizeof hi_uni); }
    if(x){ memcpy(hi_cx, x, sizeof hi_cx); }
    if(y){ memcpy(hi_cy, y, sizeof hi_cy); }
}
void pcx_hi_clear(void){ memset(hi_uni, 0, sizeof hi_uni); memset(hi_cx, 0, sizeof hi_cx); memset(hi_cy, 0, sizeof hi_cy); }
static uint64_t fold64(const uint32_t *w, int n, uint64_t salt){
    uint64_t h = 0x9E3779B97F4A7C15ull ^ salt;
    for(int i = 0; i < n; i++){ h ^= w[i] + 0x9E3779B9u + (h << 6) + (h >> 2); h *= 0xBF58476D1CE4E5B9ull; h ^= h >> 29; }
    return h;
}
static int any(const uint32_t *w, int n){ for(int i = 0; i < n; i++) if(w[i]) return 1; return 0; }
void pcx_fold(uint32_t *uni8, uint32_t *cx5, uint32_t *cy5){
    if(any(hi_uni, PCX_UNI_HI)){ for(int i = 0; i < 4; i++){ uint64_t h = fold64(hi_uni, PCX_UNI_HI, 0x55 + i); uni8[2 * i] ^= (uint32_t)h; uni8[2 * i + 1] ^= (uint32_t)(h >> 32); } }
    if(any(hi_cx, PCX_AX_HI)){ uint64_t h = fold64(hi_cx, PCX_AX_HI, 0xA1), g = fold64(hi_cx, PCX_AX_HI, 0xA2); cx5[0] ^= (uint32_t)h; cx5[1] ^= (uint32_t)(h >> 32); cx5[2] ^= (uint32_t)g; cx5[3] ^= (uint32_t)(g >> 32); cx5[4] ^= (uint32_t)fold64(hi_cx, PCX_AX_HI, 0xA3) & 0x7FFFFFFu; }
    if(any(hi_cy, PCX_AX_HI)){ uint64_t h = fold64(hi_cy, PCX_AX_HI, 0xB1), g = fold64(hi_cy, PCX_AX_HI, 0xB2); cy5[0] ^= (uint32_t)h; cy5[1] ^= (uint32_t)(h >> 32); cy5[2] ^= (uint32_t)g; cy5[3] ^= (uint32_t)(g >> 32); cy5[4] ^= (uint32_t)fold64(hi_cy, PCX_AX_HI, 0xB3) & 0x7FFFFFFu; }
}
void pcx_counts(int *a, int *b, int *c, int *d, int *e, int *f){ *a = trait_count(); *b = stat_count(); *c = people_count(); *d = city_count(); *e = creature_kind_count(); *f = theme_count(); }

static void add(PcxPage *p, int col, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void add(PcxPage *p, int col, const char *fmt, ...){
    if(p->n >= PCX_LINES) return;
    va_list ap; va_start(ap, fmt); vsnprintf(p->t[p->n], PCX_W, fmt, ap); va_end(ap); p->col[p->n++] = (uint8_t)col;
}
static void addwrap(PcxPage *p, int col, const char *s, int w){      // words cut to lines of at most w letters
    char line[PCX_W]; int n = 0;
    while(*s){
        const char *e = s; while(*e && *e != ' ') e++;
        int len = (int)(e - s);
        if(n && n + 1 + len > w){ line[n] = 0; add(p, col, "%s", line); n = 0; }
        if(n) line[n++] = ' ';
        memcpy(line + n, s, len); n += len; s = e; while(*s == ' ') s++;
    }
    if(n){ line[n] = 0; add(p, col, "%s", line); }
}
static const char *commas(long v, char *b){ char t[24]; int n = snprintf(t, sizeof t, "%ld", v), j = 0; for(int i = 0; i < n; i++){ if(i && (n - i) % 3 == 0) b[j++] = ','; b[j++] = t[i]; } b[j] = 0; return b; }
static int rcol(int r){ return r >= 4 ? PX_RED : r >= 3 ? PX_GOLD : r >= 2 ? PX_CYAN : PX_WHITE; }
static void title(PcxPage *p, const char *t){ memset(p, 0, sizeof *p); snprintf(p->title, sizeof p->title, "%s", t); }

static void make_addr(Addr *a, const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon){
    memset(a, 0, sizeof *a); memcpy(a->uni, uni8, 8 * 4); memcpy(a->uni + 8, hi_uni, sizeof hi_uni);
    memcpy(a->cx, cx5, 5 * 4); memcpy(a->cx + 5, hi_cx, sizeof hi_cx); memcpy(a->cy, cy5, 5 * 4); memcpy(a->cy + 5, hi_cy, sizeof hi_cy);
    a->star = (uint8_t)star; a->planet = (uint8_t)((planet & 15) | ((moon & 15) << 4));
}
void pcx_effects(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, int out[3]){
    static Addr a; PlanetInfo pi; make_addr(&a, uni8, cx5, cy5, star, planet, moon); planet_make(addr_seed(&a), &pi);
    out[0] = pi.gravity_pct <= 52 ? 0 : pi.gravity_pct <= 87 ? 1 : pi.gravity_pct <= 122 ? 2 : 3; out[1] = -1; out[2] = -1;
    for(int i = 0; i < pi.n; i++){ const char *b = trait_name(trait_base(pi.id[i]));
        if(!strcmp(b, "LONG NIGHT") || !strcmp(b, "DEAD SUN")) out[1] = 2; else if(!strcmp(b, "TIDALLY LOCKED") || !strcmp(b, "RED GIANT SKY")) out[1] = 1;
        else if(!strcmp(b, "CLEAR SKIES")) out[2] = 0; else if(!strcmp(b, "STORMY") || !strcmp(b, "ACID RAIN")) out[2] = 1; else if(!strcmp(b, "SNOWFALL")) out[2] = 2;
        else if(!strcmp(b, "SANDSTORMS") || !strcmp(b, "ENDLESS FOG")) out[2] = 3; else if(!strcmp(b, "ASH FALL")) out[2] = 5; }
}
void pcx_world(const uint32_t *uni8, const uint32_t *cx5, const uint32_t *cy5, int star, int planet, int moon, int applied, PcxWorld *o){
    static Addr a; PlanetInfo pi; int th[2]; char nm[32]; int i, z, f;
    memset(o, 0, sizeof *o); make_addr(&a, uni8, cx5, cy5, star, planet, moon);
    uint64_t seed = addr_seed(&a), useed = uni_seed(a.uni);
    planet_make(seed, &pi); planet_name(seed, nm); universe_themes(useed, th);
    o->seed = seed; snprintf(o->planet, sizeof o->planet, "%s", nm); o->npages = 6;
    // ---- page 0: the place, the universe's two rule changes, the whole-planet numbers
    { PcxPage *p = &o->page[0]; title(p, "THIS PLACE");
      add(p, PX_GOLD, "PLANET %s    STAR %d  PLANET %d  MOON %d", nm, star, planet + 1, moon);
      add(p, PX_GREY, "ADDRESS SEED %016llX", (unsigned long long)seed);
      add(p, PX_GREY, "READ FROM A 2048-BIT UNIVERSE AND TWO 1240-BIT SIDES");
      add(p, PX_GREY, "UNIVERSE SEED %016llX", (unsigned long long)useed);
      add(p, PX_CYAN, "THIS UNIVERSE HAS TWO RULE CHANGES:");
      for(i = 0; i < 2; i++){ add(p, PX_GOLD, "%d. %s", i + 1, theme_get(th[i])->name); addwrap(p, PX_WHITE, theme_get(th[i])->rule, 56); }
      add(p, PX_CYAN, "THE WHOLE PLANET:");
      add(p, PX_WHITE, "GRAVITY %d PCT   DAY %d HOURS   %d C", pi.gravity_pct, pi.day_hours, pi.temp_c);
      add(p, PX_WHITE, "DANGER %+d   LOOT %+d   PRICES %+d   LIFE %+d", pi.danger, pi.loot, pi.price, pi.life);
      add(p, PX_GREY, applied ? "THE GAME USES THESE TRAITS FOR GRAVITY, LIGHT, WEATHER." : "STORY WORLD: THE STORY'S OWN RULES. TRAITS ARE LORE ONLY."); }
    // ---- page 1: the traits
    { PcxPage *p = &o->page[1]; char t[96], cb[24]; title(p, "TRAITS"); add(p, PX_CYAN, "%d TRAITS ON THIS PLANET (OF %s)", pi.n, commas(trait_count(), cb));
      for(i = 0; i < pi.n; i++){ trait_name_to(pi.id[i], t, sizeof t); add(p, PX_GOLD, "%s", t); add(p, PX_GREY, "  %s, FLAVOUR %d OF 16", cat_name(trait_cat(pi.id[i])), trait_variant(pi.id[i]) + 1); }
      add(p, PX_GREY, "1,040 TRAITS = 65 CLASSIC TRAITS X 16 FLAVOURS"); }
    // ---- page 2: the 112 stats, zones down and families across
    { PcxPage *p = &o->page[2]; char row[PCX_W]; static const char *const ab[N_FAM] = {"GRAV", "TEMP", "LITE", "DNGR", "LOOT", "PRIC", "LIFE"};
      title(p, "STATS"); snprintf(row, sizeof row, "%-12s", "ZONE"); for(f = 0; f < N_FAM; f++) { char c[8]; snprintf(c, sizeof c, "%6s", ab[f]); strcat(row, c); } add(p, PX_CYAN, "%s", row);
      for(z = 0; z < N_ZONE; z++){ snprintf(row, sizeof row, "%-12s", zone_name(z)); for(f = 0; f < N_FAM; f++){ char c[8]; snprintf(c, sizeof c, "%6d", pi.stat[f * N_ZONE + z]); strcat(row, c); } add(p, z & 1 ? PX_GREY : PX_WHITE, "%s", row); }
      add(p, PX_GREY, "112 STATS = 7 X 16. GRAV AND LITE ARE PERCENT."); }
    // ---- page 3: peoples and the city
    { PcxPage *p = &o->page[3]; title(p, "PEOPLE AND CITY");
      if(!pi.npeople) add(p, PX_GREY, "NO ONE LIVES HERE.");
      for(i = 0; i < pi.npeople; i++){ People pe; people_get(pi.people[i], &pe); add(p, PX_GOLD, "%s", pe.name); add(p, PX_WHITE, "%s", pe.about1); add(p, PX_GREY, "%s", pe.about2);
          add(p, PX_CYAN, "MIGHT %d WIT %d CHARM %d CRAFT %d TEMPER %d LIFE %d", pe.might, pe.wit, pe.charm, pe.craft, pe.temper, pe.lifespan); }
      if(pi.city >= 0){ CityKind c; city_get(pi.city, &c); add(p, PX_GOLD, "CITY: %s", c.name); add(p, PX_WHITE, "%s", c.about1); add(p, PX_GREY, "%s", c.about2); add(p, PX_CYAN, "SIZE %d WALLS %d TRADE %d WONDER %d", c.pop, c.walls, c.trade, c.wonder); }
      else if(pi.npeople) add(p, PX_GREY, "NO CITY. THEY LIVE IN SMALL CAMPS."); }
    // ---- page 4: native creatures
    { PcxPage *p = &o->page[4]; char cb[24]; title(p, "CREATURES"); add(p, PX_CYAN, "%d NATIVE KINDS (OF %s)", pi.nbeing, commas(creature_kind_count(), cb));
      for(i = 0; i < pi.nbeing; i++){ Creature c; creature_kind(pi.being[i], &c); add(p, rcol(c.rarity), "%s", c.name);
          add(p, PX_GREY, "  %s, %s. HP %d SPD %d BITE %d GRD %d WIT %d", creature_class_name(c.cls), rarity_name(c.rarity), c.stat[0], c.stat[1], c.stat[2], c.stat[3], c.stat[4]); }
      add(p, PX_GREY, "12,672 KINDS = 66 BODY PLANS X 192 VARIANTS"); }
    // ---- page 5: the famous one
    { PcxPage *p = &o->page[5]; Indiv iv = { pi.legend_kind, pi.legend_gift, pi.legend_colour, pi.legend_rank, pi.legend_mut }; CreatureInd c; title(p, "FAMOUS CREATURE");
      creature_indiv(&iv, &c); add(p, PX_CYAN, "THE MOST FAMOUS CREATURE OF THIS WORLD:"); addwrap(p, rcol(c.rarity), c.name, 56);
      add(p, PX_GOLD, "%s", rarity_name(c.rarity)); add(p, PX_WHITE, "%s", c.about1); add(p, PX_WHITE, "%s", c.about2); add(p, PX_GREY, "%s", c.about3);
      for(i = 0; i < N_CSTAT; i++) add(p, PX_CYAN, "%-9s %d", creature_stat_name(i), c.stat[i]);
      add(p, PX_GREY, "EACH KIND HAS 32,768 INDIVIDUALS: 415,236,096 IN ALL."); }
}
