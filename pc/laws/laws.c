// laws.c: the LAWS table, editing, presets, files and number words. See laws.h.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "laws.h"

const char *const lw_cat_name[LC_N] = { "UNIVERSE", "GALAXY", "STARS", "PLANETS", "LIFE", "TIME" };
const char *const lw_gtype_name[GT_N] = { "SPIRAL", "BARRED SPIRAL", "RING", "DWARF", "GIANT ELLIPTICAL", "LENS", "IRREGULAR", "MERGER" };
const char *const lw_sclass_name[SC_N] = { "BLUE GIANT (O)", "BLUE-WHITE (B)", "WHITE (A)", "YELLOW-WHITE (F)", "YELLOW (G)", "ORANGE (K)", "RED DWARF (M)", "WHITE DWARF", "RED GIANT", "NEUTRON STAR" };

#define LF_ADV_ LF_ADV
const LawDef lw_defs[LW_N] = {
#define A LF_ADV
#define N LF_NUM
#define V LF_VARY
#define X(id,cat,name,desc,unit,mn,mx,df,st,bg,xmn,xmx,dec,fl) { #id, name, desc, unit, cat, dec, (uint8_t)(fl), mn, mx, df, st, bg, xmn, xmx },
#include "laws_def.h"
#undef X
#undef A
#undef N
#undef V
};
int lw_count(void){ return LW_N; }

uint64_t lw_mix(uint64_t x){ x += 0x9E3779B97F4A7C15ull; x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull; x = (x ^ (x >> 27)) * 0x94D049BB133111EBull; return x ^ (x >> 31); }

// ---------------------------------------------------------------------------------------------------- editing
void lw_default(LawSet *s){ memset(s, 0, sizeof *s); for(int i = 0; i < LW_N; i++) s->v[i] = lw_defs[i].def; }
int lw_find(const char *key){ for(int i = 0; i < LW_N; i++) if(!strcmp(lw_defs[i].key, key)) return i; return -1; }
int lw_is_edited(const LawSet *s, int id){ return s->v[id] != lw_defs[id].def; }
int lw_edited_count(const LawSet *s){ int n = 0; for(int i = 0; i < LW_N; i++) n += lw_is_edited(s, i); return n; }
int lw_is_default(const LawSet *s){ return lw_edited_count(s) == 0; }
int lw_clamp_one(int id, int32_t v, int cheat){
    const LawDef *d = &lw_defs[id]; int32_t lo = cheat ? d->xmin : d->min, hi = cheat ? d->xmax : d->max;
    if(v < lo) v = lo; if(v > hi) v = hi; return v;
}
void lw_clamp_all(LawSet *s, int cheat){ for(int i = 0; i < LW_N; i++) s->v[i] = lw_clamp_one(i, s->v[i], cheat); }
int lw_set(LawSet *s, int id, int32_t v, int cheat){
    v = lw_clamp_one(id, v, cheat); s->v[id] = v;
    if(v < lw_defs[id].min || v > lw_defs[id].max) s->cheated = 1;
    return v;
}
int lw_step(LawSet *s, int id, int dir, int big, int cheat){
    const LawDef *d = &lw_defs[id]; int32_t st = big ? d->big : d->step; if(st < 1) st = 1;
    int32_t v = s->v[id]; int32_t nv;
    if(dir > 0){ nv = v + st; if(!big && v < d->def && nv > d->def) nv = d->def; }
    else { nv = v - st; if(!big && v > d->def && nv < d->def) nv = d->def; }              // a small step always lands on the normal value
    if(big && ((v < d->def && nv > d->def) || (v > d->def && nv < d->def))) nv = d->def;   // ... and so does a big one, so "NORMAL" is easy to find
    return lw_set(s, id, nv, cheat);
}
void lw_reset_one(LawSet *s, int id){ s->v[id] = lw_defs[id].def; }
void lw_reset_all(LawSet *s){ int ch = s->cheated; lw_default(s); s->cheated = ch; }       // (the cheated mark stays: a world that was cheated is still marked)
uint64_t lw_hash(const LawSet *s){ uint64_t h = 0x4C41575321ull; for(int i = 0; i < LW_N; i++) h = lw_mix(h ^ ((uint64_t)(uint32_t)s->v[i] << 8) ^ (uint64_t)i); return h; }
int lw_visible(int id, int advanced){ return advanced || !(lw_defs[id].flags & LF_ADV); }

// ---------------------------------------------------------------------------------------------------- number words
void lw_fmt_sci(double v, char *out, int n){
    if(v == 0){ snprintf(out, n, "0"); return; }
    int neg = v < 0; if(neg) v = -v; int e = 0;
    while(v >= 10.0 && e < 300){ v /= 10.0; e++; } while(v < 1.0 && e > -300){ v *= 10.0; e--; }
    int m = (int)(v * 10.0 + 0.5); if(m >= 100){ m = 10; e++; }                 // one decimal: 6.4
    snprintf(out, n, "%s%d.%dE%d", neg ? "-" : "", m / 10, m % 10, e);
}
static const char *const bigname[] = { "", " THOUSAND", " MILLION", " BILLION", " TRILLION", " QUADRILLION" };
void lw_fmt_big(double v, char *out, int n){
    if(v < 0){ char t[48]; lw_fmt_big(-v, t, sizeof t); snprintf(out, n, "-%s", t); return; }
    if(v < 1000){ if(v >= 100) snprintf(out, n, "%d", (int)(v + 0.5)); else if(v >= 10) snprintf(out, n, "%.1f", v); else if(v >= 1) snprintf(out, n, "%.2f", v); else if(v >= 0.001) snprintf(out, n, "%.3f", v); else if(v == 0) snprintf(out, n, "0"); else lw_fmt_sci(v, out, n); return; }
    if(v < 1e6){ long long k = (long long)(v + 0.5); snprintf(out, n, "%lld,%03lld", k / 1000, k % 1000); return; }
    if(v >= 1e18){ lw_fmt_sci(v, out, n); return; }
    int g = 0; double x = v; while(x >= 1000.0 && g < 5){ x /= 1000.0; g++; }
    if(x >= 100) snprintf(out, n, "%d%s", (int)(x + 0.5), bigname[g]); else if(x >= 10) snprintf(out, n, "%.1f%s", x, bigname[g]); else snprintf(out, n, "%.2f%s", x, bigname[g]);
}
void lw_fmt_time(double s, char *out, int n){
    char t[48];
    if(s < 1e-3){ snprintf(out, n, "NO TIME AT ALL"); return; }
    if(s < 60){ lw_fmt_big(s, t, sizeof t); snprintf(out, n, "%s SECONDS", t); return; }
    if(s < 3600 * 2){ lw_fmt_big(s / 60.0, t, sizeof t); snprintf(out, n, "%s MINUTES", t); return; }
    if(s < 86400.0 * 2){ lw_fmt_big(s / 3600.0, t, sizeof t); snprintf(out, n, "%s HOURS", t); return; }
    if(s < 86400.0 * 365.25 * 2){ lw_fmt_big(s / 86400.0, t, sizeof t); snprintf(out, n, "%s DAYS", t); return; }
    lw_fmt_big(s / (86400.0 * 365.25), t, sizeof t); snprintf(out, n, "%s YEARS", t);
}
void lw_fmt_dist(double m, char *out, int n){
    char t[48]; const double LY = 9.4607e15;
    if(m < 0.01){ lw_fmt_big(m * 1000.0, t, sizeof t); snprintf(out, n, "%s MM", t); return; }
    if(m < 1.0){ lw_fmt_big(m * 100.0, t, sizeof t); snprintf(out, n, "%s CM", t); return; }
    if(m < 1000){ lw_fmt_big(m, t, sizeof t); snprintf(out, n, "%s M", t); return; }
    if(m < LY * 0.1){ lw_fmt_big(m / 1000.0, t, sizeof t); snprintf(out, n, "%s KM", t); return; }
    lw_fmt_big(m / LY, t, sizeof t); snprintf(out, n, "%s LIGHT YEARS", t);
}

// ---------------------------------------------------------------------------------------------------- value text
static const char *const gshape[8] = { "SPIRAL", "BARRED SPIRAL", "RING", "DWARF", "GIANT ELLIPTICAL", "LENS", "IRREGULAR", "MERGER" };
static const char *const sshape[10] = { "BLUE GIANT", "BLUE-WHITE", "WHITE", "YELLOW-WHITE", "YELLOW", "ORANGE", "RED DWARF", "WHITE DWARF", "RED GIANT", "NEUTRON STAR" };
const char *lw_enum_name(int id, int v){
    if(id == LW_G_HOME) return gshape[(v < 0 ? 0 : v > 7 ? 7 : v)];
    if(id == LW_S_HOME) return sshape[(v < 0 ? 0 : v > 9 ? 9 : v)];
    return "";
}
static void size_text(int32_t q, char *o, int n){                         // 2^(q/4), said in words
    double f = lw_pw2(q / 4.0); char t[48];
    if(q == 0){ snprintf(o, n, "NORMAL"); return; }
    if(f >= 1) { lw_fmt_big(f, t, sizeof t); snprintf(o, n, "X%s", t); }
    else { lw_fmt_big(1.0 / f, t, sizeof t); snprintf(o, n, "1/%s", t); }
}
void lw_raw_text(int id, int32_t v, char *o, int n){
    const LawDef *d = &lw_defs[id];
    if(d->flags & LF_ENUM){ snprintf(o, n, "%s", lw_enum_name(id, v)); return; }
    if(d->flags & LF_SIZE){ size_text(v, o, n); return; }
    if(d->dec == 1) snprintf(o, n, "%d.%d %s", v / 10, (v < 0 ? -v : v) % 10, d->unit);
    else snprintf(o, n, "%d %s", v, d->unit);
}
void lw_value_text(const LawSet *s, int id, int advanced, char *o, int n){
    const LawDef *d = &lw_defs[id]; int32_t v = s->v[id];
    if(d->flags & LF_ENUM){ snprintf(o, n, "%s", lw_enum_name(id, v)); return; }
    if(d->flags & LF_SIZE){ size_text(v, o, n); return; }
    if(advanced || (d->flags & LF_NUM) || d->def == 0){ lw_raw_text(id, v, o, n); return; }
    if(v == d->def){ snprintf(o, n, "NORMAL"); return; }
    if(v == 0){ snprintf(o, n, "OFF"); return; }
    double r = (double)v / (double)d->def;
    if(r >= 10) snprintf(o, n, "X%d", (int)(r + 0.5)); else if(r >= 2) snprintf(o, n, "X%.1f", r); else snprintf(o, n, "X%.2f", r);
    // trim a trailing zero: X2.0 -> X2
    char *dot = strchr(o, '.'); if(dot){ int L = (int)strlen(o); while(L > 0 && o[L - 1] == '0') o[--L] = 0; if(o[L - 1] == '.') o[L - 1] = 0; }
}

// ---------------------------------------------------------------------------------------------------- search
static int has_word(const char *hay, const char *w){ int L = (int)strlen(w); if(!L) return 1; for(const char *p = hay; *p; p++) if(!strncmp(p, w, L)) return 1; return 0; }
int lw_match(int id, const char *q){
    char buf[64]; snprintf(buf, sizeof buf, "%s", q ? q : ""); int ok = 1;
    for(char *t = strtok(buf, " "); t; t = strtok(0, " ")) if(!(has_word(lw_defs[id].name, t) || has_word(lw_defs[id].desc, t))) ok = 0;
    return ok;
}

// ---------------------------------------------------------------------------------------------------- presets
typedef struct { const char *key; int32_t v; } PV;
typedef struct { const char *name, *desc; PV pv[24]; } Preset;
static const Preset presets[] = {
 { "NORMAL", "THE GAME AS IT ALWAYS WAS. EVERY LAW AT ITS NORMAL VALUE.", { { 0 } } },
 { "TINY WORLDS", "PLANETS SHRINK TO BOULDERS AND PEBBLES YOU WALK ROUND IN A FEW STEPS. LOW GRAVITY, THIN AIR.", { { "P_SIZE", -80 }, { "P_SPREAD", 60 }, { "P_MOONS", 150 } } },
 { "PEBBLE WORLDS", "THE SMALLEST WORLDS THERE ARE: A BUG WALKS ROUND ONE IN EIGHT STEPS AND SEES ITS OWN BACK.", { { "P_SIZE", -112 }, { "P_SPREAD", 0 } } },
 { "GIANT WORLDS", "PLANETS GROW TO THOUSANDS OF TIMES THE SIZE OF THE EARTH. THE LAND GOES ON AND ON, FLAT TO THE HORIZON.", { { "P_SIZE", 48 }, { "P_SPREAD", 120 } } },
 { "HIGH GRAVITY", "EVERYTHING PULLS HARDER. HEAVY PLANETS, SLOW WALKING, SHORT YEARS, SQUAT CREATURES.", { { "U_GRAV", 250 }, { "P_DENS", 160 }, { "L_CSIZE", 60 } } },
 { "LOW GRAVITY", "EVERYTHING PULLS WEAKLY. FLOATY BOUNCING, TALL MOUNTAINS, LONG YEARS, GIANT CREATURES.", { { "U_GRAV", 40 }, { "P_DENS", 70 }, { "P_MOUNT", 250 }, { "L_CSIZE", 250 } } },
 { "DEAD UNIVERSE", "AN OLD, COLD UNIVERSE: FEW NEW STARS, DEAD STARS EVERYWHERE, LIFE IS RARE.", { { "U_AGE", 380 }, { "S_SFR", 10 }, { "S_WD", 400 }, { "S_NEUT", 60 }, { "S_YOUNG", 5 }, { "L_CHANCE", 20 }, { "P_PERSTAR", 60 } } },
 { "STAR NURSERY", "A YOUNG UNIVERSE BURSTING WITH NEW BLUE STARS, DUST AND DANGEROUS RADIATION.", { { "U_AGE", 40 }, { "S_SFR", 700 }, { "S_O", 60 }, { "S_B", 120 }, { "S_YOUNG", 95 }, { "G_GAS", 250 }, { "G_DUST", 300 }, { "G_SUPERN", 400 } } },
 { "CROWDED GALAXIES", "PLANETS AND MOONS EVERYWHERE. EVERY STAR HAS A FAMILY OF WORLDS.", { { "P_PERSTAR", 300 }, { "P_MOONS", 250 }, { "G_CROWD", 500 }, { "P_MAXPL", 16 } } },
 { "LONELY SPACE", "BIG GAPS AND EMPTY STARS. FEW PLANETS, LONG TRIPS, LONELY WORLDS.", { { "U_SPACING", 120 }, { "P_PERSTAR", 30 }, { "P_MOONS", 20 }, { "G_CROWD", 0 }, { "P_ROGUE", 25 } } },
 { "SLOW LIGHT", "THE SPEED LIMIT DROPS TO A TENTH. EVERY TRIP TAKES TEN TIMES AS LONG.", { { "U_LIGHT", 10 } } },
 { "FAST LIGHT", "THE SPEED LIMIT IS TEN TIMES HIGHER. TRIPS ARE OVER IN A BLINK.", { { "U_LIGHT", 1000 } } },
 { "HOT UNIVERSE", "BRIGHT STARS AND THICK AIR: DESERTS, BOILING SEAS AND ROASTED WORLDS.", { { "S_BRIGHT", 220 }, { "P_GREEN", 220 }, { "P_WATER", 50 }, { "P_HELL", 250 } } },
 { "ICY UNIVERSE", "DIM STARS AND SHINY PLANETS: ICE AND SNOW ALMOST EVERYWHERE.", { { "S_BRIGHT", 45 }, { "P_ALBEDO", 150 }, { "P_ICE", 220 }, { "P_GREEN", 40 } } },
 { "WATER WORLDS", "OCEANS ON EVERY WORLD, STORMS, RAIN AND FEW ISLANDS.", { { "P_WATER", 240 }, { "P_CLOUD", 200 }, { "P_STORM", 200 }, { "P_DUNE", 20 } } },
 { "SPIRALS ONLY", "EVERY GALAXY IS A BIG, BEAUTIFUL SPIRAL WITH MANY ARMS.", { { "G_SPIRAL", 100 }, { "G_BARRED", 60 }, { "G_RING", 0 }, { "G_DWARF", 0 }, { "G_ELLIP", 0 }, { "G_LENT", 0 }, { "G_IRREG", 0 }, { "G_MERGE", 0 }, { "G_ARMS", 6 } } },
 { "GALAXY CRASH", "GALAXIES RUN INTO EACH OTHER AND BLAST OUT NEW STARS. WILD AND DANGEROUS.", { { "G_MERGE", 60 }, { "G_RING", 30 }, { "G_IRREG", 30 }, { "G_COLLIDE", 400 }, { "G_BH", 800 }, { "G_BHJET", 400 } } },
 { "STRANGE LIFE", "LIFE EVERYWHERE, FAST CHANGING AND WEIRD. MANY STRANGE WORLDS.", { { "L_CHANCE", 400 }, { "L_EVOLVE", 400 }, { "L_WEIRD", 600 }, { "L_TOLER", 150 }, { "U_COSMIC", 500 } } },
 { "FAST CLOCKS", "SHORT DAYS AND YEARS, FAST WEATHER. EVERYTHING HAPPENS QUICKLY.", { { "T_DAY", 25 }, { "T_ORBIT", 300 }, { "T_WEATHER", 400 }, { "T_SPEED", 200 } } },
 { "SLOW CLOCKS", "LONG DAYS AND YEARS. NIGHTS LAST FOR DAYS AND SUMMERS FOR YEARS.", { { "T_DAY", 900 }, { "T_ORBIT", 40 }, { "T_WEATHER", 30 }, { "T_SEASON", 300 } } },
};
int lw_preset_count(void){ return (int)(sizeof presets / sizeof presets[0]); }
const char *lw_preset_name(int i){ return presets[i].name; }
const char *lw_preset_desc(int i){ return presets[i].desc; }
void lw_preset_apply(LawSet *s, int i){
    int ch = s->cheated; lw_default(s); s->cheated = ch;
    for(int k = 0; k < 24 && presets[i].pv[k].key; k++){ int id = lw_find(presets[i].pv[k].key); if(id >= 0) lw_set(s, id, presets[i].pv[k].v, 0); }
}

// ---------------------------------------------------------------------------------------------------- files
int lw_to_text(const LawSet *s, char *o, int n){
    int L = snprintf(o, n, "BUGSTAR LAWS 1\ncheated=%d\n", s->cheated ? 1 : 0);
    for(int i = 0; i < LW_N && L < n - 40; i++) if(lw_is_edited(s, i)) L += snprintf(o + L, n - L, "%s=%d\n", lw_defs[i].key, s->v[i]);
    return L;
}
int lw_from_text(LawSet *s, const char *text){
    lw_default(s);
    if(strncmp(text, "BUGSTAR LAWS", 12)) return 0;
    const char *p = strchr(text, '\n'); if(!p) return 1; p++;
    while(*p){
        char key[40]; long v; int k = 0; while(*p && *p != '=' && *p != '\n' && k < 39) key[k++] = *p++; key[k] = 0;
        if(*p != '='){ while(*p && *p != '\n') p++; if(*p) p++; continue; }
        p++; v = strtol(p, 0, 10); while(*p && *p != '\n') p++; if(*p) p++;
        if(!strcmp(key, "cheated")){ s->cheated = v ? 1 : 0; continue; }
        int id = lw_find(key); if(id >= 0) s->v[id] = lw_clamp_one(id, (int32_t)v, 1);     // (a file may hold cheat values; the screen clamps again for normal play)
    }
    return 1;
}
int lw_save(const LawSet *s, const char *path){
    char buf[8192]; int n = lw_to_text(s, buf, sizeof buf); FILE *f = fopen(path, "wb"); if(!f) return 0;
    int ok = (int)fwrite(buf, 1, n, f) == n; fclose(f); return ok;
}
int lw_load(LawSet *s, const char *path){
    lw_default(s); FILE *f = fopen(path, "rb"); if(!f) return 0;
    char buf[8192]; int n = (int)fread(buf, 1, sizeof buf - 1, f); fclose(f); buf[n] = 0; return lw_from_text(s, buf);
}

// ---------------------------------------------------------------------------------------------------- UNIVERSE VARIETY
void lw_effective(const LawSet *s, uint64_t useed, LawSet *o){
    *o = *s; int var = s->v[LW_U_VARIETY]; if(var <= 0) return;
    for(int i = 0; i < LW_N; i++) if(lw_defs[i].flags & LF_VARY){
        uint64_t h = lw_mix(useed ^ (0x5641524Cull << 8) ^ (uint64_t)i * 0x9E3779B97F4A7C15ull);
        double u = (double)(h >> 11) / 9007199254740992.0;                    // 0..1
        double f = 1.0 + (var / 100.0) * (u - 0.5) * 1.2;                    // +-60 % at VARIETY 100
        double nv = (double)s->v[i] * f; int32_t q = (int32_t)(nv + (nv < 0 ? -0.5 : 0.5));
        if(s->v[i] == 0) q = 0;                                                // an OFF law stays off
        o->v[i] = lw_clamp_one(i, q, 1);
    }
}

// ---------------------------------------------------------------------------------------------------- 2^x and log2(x) without libm
double lw_pw2(double x){
    if(x > 1000) x = 1000; if(x < -1000) x = -1000;
    int i = (int)x; if((double)i > x) i--; double f = x - i;                    // x = i + f, 0 <= f < 1
    // 2^f = e^(f ln2) by a 12-term series (f ln2 < 0.7: the series is exact to double precision)
    double y = f * 0.69314718055994530942, t = 1, sum = 1; for(int k = 1; k < 14; k++){ t *= y / k; sum += t; }
    while(i > 0){ int s = i > 60 ? 60 : i; sum *= (double)(1ull << s); i -= s; }
    while(i < 0){ int s = -i > 60 ? 60 : -i; sum /= (double)(1ull << s); i += s; }
    return sum;
}
double lw_lg2(double x){
    if(x <= 0) return -1000;
    int e = 0; while(x >= 2.0){ x *= 0.5; e++; } while(x < 1.0){ x *= 2.0; e--; }
    double z = (x - 1.0) / (x + 1.0), z2 = z * z, t = z, sum = 0; for(int k = 1; k < 60; k += 2){ sum += t / k; t *= z2; }
    return e + 2.0 * sum / 0.69314718055994530942;
}
