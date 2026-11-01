// lawx.c: the flat face of the laws for the game (see lawx.h). One law set: the world being played.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "laws.h"
#include "lawx.h"

static LawSet g_set; static int g_init;
#define UNDO_N 24
static LawSet g_undo[UNDO_N]; static int g_undo_n;
static void need(void){ if(!g_init){ lw_default(&g_set); g_init = 1; } }

int plw_count(void){ return LW_N; }
int plw_cat_n(void){ return LC_N; }
const char *plw_cat_name(int c){ return lw_cat_name[c < 0 ? 0 : c >= LC_N ? LC_N - 1 : c]; }
int plw_cat(int id){ return lw_defs[id].cat; }
const char *plw_key(int id){ return lw_defs[id].key; }
const char *plw_name(int id){ return lw_defs[id].name; }
const char *plw_desc(int id){ return lw_defs[id].desc; }
const char *plw_unit(int id){ return lw_defs[id].unit; }
int plw_find(const char *key){ return lw_find(key); }
int plw_visible(int id, int adv){ return lw_visible(id, adv); }
int plw_match(int id, const char *q){ return lw_match(id, q); }
int plw_is_edited(int id){ need(); return lw_is_edited(&g_set, id); }
int plw_edited_count(void){ need(); return lw_edited_count(&g_set); }
int plw_is_default(void){ need(); return lw_is_default(&g_set); }
int plw_value(int id){ need(); return g_set.v[id]; }
void plw_value_text(int id, int adv, char *out, int n){ need(); lw_value_text(&g_set, id, adv, out, n); }
int plw_step(int id, int dir, int big, int cheat){ need(); return lw_step(&g_set, id, dir, big, cheat); }
int plw_setv(int id, int v, int cheat){ need(); return lw_set(&g_set, id, v, cheat); }
void plw_reset_one(int id){ need(); lw_reset_one(&g_set, id); }
void plw_reset_all(void){ need(); int ch = g_set.cheated; lw_reset_all(&g_set); g_set.cheated = ch; }       // (a world that was cheated stays marked)
int plw_cheated(void){ need(); return g_set.cheated; }
void plw_set_cheated(int on){ need(); g_set.cheated = on ? 1 : 0; }
int plw_in_sensible(int id){ need(); return g_set.v[id] >= lw_defs[id].min && g_set.v[id] <= lw_defs[id].max; }
int plw_preset_count(void){ return lw_preset_count(); }
const char *plw_preset_name(int i){ return lw_preset_name(i); }
const char *plw_preset_desc(int i){ return lw_preset_desc(i); }
void plw_preset_apply(int i){ need(); int ch = g_set.cheated; lw_preset_apply(&g_set, i); g_set.cheated = ch; }
void plw_undo_push(void){ need(); if(g_undo_n == UNDO_N){ memmove(&g_undo[0], &g_undo[1], (UNDO_N - 1) * sizeof g_undo[0]); g_undo_n--; } g_undo[g_undo_n++] = g_set; }
int  plw_undo_pop(void){ need(); if(!g_undo_n) return 0; int ch = g_set.cheated; g_set = g_undo[--g_undo_n]; if(ch) g_set.cheated = 1; return 1; }
int  plw_undo_depth(void){ return g_undo_n; }

void plw_slot_path(const char *sav, int slot, char *out, int n){
    char base[600]; snprintf(base, sizeof base, "%s", sav && *sav ? sav : "bugstar.sav"); size_t L = strlen(base);
    if(L > 4 && !strcmp(base + L - 4, ".sav")) base[L - 4] = 0;
    snprintf(out, n, "%s.laws%d", base, slot);
}
int plw_load(const char *path){ need(); g_undo_n = 0; return lw_load(&g_set, path); }
int plw_save(const char *path){ need(); if(lw_is_default(&g_set) && !g_set.cheated){ remove(path); return 1; } return lw_save(&g_set, path); }

static void env(const PlwEnv *e, LwEnv *o){
    memset(o, 0, sizeof *o); uint64_t u = 0x1234567ull; for(int i = 0; i < 8; i++) u = lw_mix(u ^ e->uni[i] ^ ((uint64_t)i << 40)); o->useed = u;
    o->gal = e->gal; o->star = e->star; o->seed_a = e->seed_a; o->seed_b = e->seed_b; o->seed_c = e->seed_c;
    o->story = e->story; o->planet = e->planet; o->nplan = e->nplan; o->moon = e->moon; o->nmoon = e->nmoon;
    o->psz = e->psz; o->grav = e->grav; o->cls = e->cls; o->hour = e->hour; o->weather = e->weather; o->snow = e->snow; o->sand = e->sand; o->city = e->city; o->ruins = e->ruins;
    o->fauna = e->fauna; o->beetles = e->beetles; o->rough = e->rough; o->hs = e->hs; o->mood = e->mood; o->water = e->water; o->snowl = e->snowl; o->hell = e->hell; o->dist_clusters_log2 = e->dist_log2;
}
void plw_apply(const PlwEnv *e, PlwApply *a){
    LwEnv x; env(e, &x); LwApply y; need(); lw_apply(&g_set, &x, &y);
    a->changed = y.changed; a->size_q = y.size_q; a->k16 = y.k16; a->tw = y.tw; a->psz = y.psz; a->grav = y.grav; a->slow = y.slow; a->zerog = y.zerog;
    a->hour = y.hour; a->weather = y.weather; a->snow = y.snow; a->sand = y.sand; a->cls = y.cls; a->city = y.city; a->ruins = y.ruins; a->fauna = y.fauna; a->beetles = y.beetles;
    a->rough = y.rough; a->hs = y.hs; a->mood = y.mood; a->snowl = y.snowl; a->water_pct = y.water_pct; a->aurora = y.aurora; a->meteor = y.meteor; a->badair = y.badair;
    a->hunger_pct = y.hunger_pct; a->day_pct = y.day_pct; a->danger_add = y.danger_add; for(int i = 0; i < 4; i++) a->node_add[i] = y.node_add[i];
}
int plw_nplan(const PlwEnv *e, int orig){ need(); if(lw_is_default(&g_set)) return orig; LwEnv x; env(e, &x); return lw_nplan(&g_set, &x, orig); }
int plw_nmoon(const PlwEnv *e, int orig){ need(); if(lw_is_default(&g_set)) return orig; LwEnv x; env(e, &x); return lw_nmoon(&g_set, &x, orig); }
int plw_dense(uint32_t roll, int odds){ need(); return lw_dense(&g_set, roll, odds); }
int plw_hell_hit(int roll100, int hp){ need(); return lw_hell_hit(&g_set, roll100, hp); }
int plw_weird_hit(uint32_t f){ need(); return lw_weird_hit(&g_set, f); }
int plw_uly(void){ need(); return lw_uly(&g_set); }
int plw_galbits(void){ need(); return lw_galbits(&g_set); }
uint32_t plw_hours(uint32_t h){ need(); return lw_hours(&g_set, h); }
int plw_warp_cost(int c){ need(); return lw_warp_cost(&g_set, c); }
int plw_time_pct(void){ need(); return lw_time_pct(&g_set); }
int plw_explain(const PlwEnv *e, PlwPage *pages){
    static LxPage px[LX_PAGES]; LwEnv x; env(e, &x); need(); int n = lw_explain(&g_set, &x, px);
    for(int i = 0; i < n; i++){ memcpy(pages[i].title, px[i].title, 32); pages[i].n = px[i].n; memcpy(pages[i].t, px[i].t, sizeof pages[i].t); memcpy(pages[i].col, px[i].col, sizeof pages[i].col); }
    return n;
}
void plw_fmt_big(double v, char *out, int n){ lw_fmt_big(v, out, n); }
uint64_t plw_hash(void){ need(); return lw_hash(&g_set); }
double plw_out(const PlwEnv *e, const char *key){
    LwEnv x; env(e, &x); LwResult r; need(); lw_eval(&g_set, &x, &r);
    for(int k = 0; k < LO_N; k++) if(!strcmp(lw_out_name[k], key)) return r.o[k]; return 0;
}

// the numbers this one law moves most at this place: "TEMPERATURE: NOW 41 (WITHOUT THIS LAW 15) C"
int plw_live(const PlwEnv *e, int id, char lines[][PLW_W], int max){
    need(); LwEnv x; env(e, &x); LawSet base = g_set; base.v[id] = lw_defs[id].def;
    LwResult a, b; lw_eval(&g_set, &x, &a); lw_eval(&base, &x, &b);
    int idx[LO_N], n = 0; double sc[LO_N];
    for(int k = 0; k < LO_N; k++){
        double p = a.o[k], q = b.o[k]; if(p == q) continue;
        double den = (p < 0 ? -p : p) + (q < 0 ? -q : q) + 1e-12, d = (p - q) / den; if(d < 0) d = -d; if(d < 0.0005) continue;
        int j = n++; while(j > 0 && sc[j - 1] < d){ sc[j] = sc[j - 1]; idx[j] = idx[j - 1]; j--; } sc[j] = d; idx[j] = k;
    }
    int m = 0;
    for(int i = 0; i < n && m < max; i++){
        int k = idx[i]; char va[32], vb[32], t[200]; lw_fmt_big(a.o[k], va, sizeof va); lw_fmt_big(b.o[k], vb, sizeof vb);
        int L = snprintf(t, sizeof t, "%s: %s (NORMAL %s)", lw_out_name[k], va, vb);
        const char *u = lw_out_unit[k]; int ul = (int)strlen(u), room = 57 - L - 1;
        if(ul && room >= 3){ while(ul > room){ while(ul > 0 && u[ul - 1] != ' ') ul--; if(ul > 0) ul--; } if(ul > 0) snprintf(t + L, sizeof t - L, " %.*s", ul, u); }
        snprintf(lines[m++], PLW_W, "%.57s", t);
    }
    return m;
}

void plw_file_remove(const char *path){ remove(path); }
void plw_file_copy(const char *from, const char *to){
    FILE *f = fopen(from, "rb"); if(!f){ remove(to); return; } FILE *g = fopen(to, "wb");
    if(g){ char buf[4096]; size_t k; while((k = fread(buf, 1, sizeof buf, f)) > 0) fwrite(buf, 1, k, g); fclose(g); } fclose(f);
}

int plw_setk(const char *kv, int cheat){
    char k[40]; int i = 0; while(kv[i] && kv[i] != '=' && i < 39){ k[i] = kv[i]; i++; } k[i] = 0; if(kv[i] != '=') return 0;
    int id = lw_find(k); if(id < 0) return 0; need(); lw_set(&g_set, id, atoi(kv + i + 1), cheat); return 1;
}
