// pc_laws.inc: the PC-only UNIVERSE LAWS inside the game. It is #included at the end of the generated main.c (like pc_info.inc), so it can call the game's
// own functions. The law table, the model and the text live in pc/laws (plain C, no game code); the game only meets them through pc/laws/lawx.h.
//   * HOOKS: the generated game calls the pc_* functions below at the places build.sh patches in (planet counts, hell and weird odds, light years per
//     cluster, voyage hours, warp fuel, the planet's size, gravity, day, weather...). At the default laws every one returns the game's own number.
//   * PC LAB (pause, WORLD page, B): WORLD FILE, UNIVERSE LAWS (the editor), HOW THIS WORKS, CHEATS.
//   * The laws of a world are saved beside the .sav, one file per save slot: <name>.laws0, .laws1, .laws2 (see pc/README.md).
#include "laws/lawx.h"
static int pc_lw_clock;                                         // frames the laws screens have been open (blinks)

// ---------------------------------------------------------------- the place, as the model wants it
static int pc_lw_dist_log2(void){                               // how many bits away from home (the east and north cluster numbers)
    int top = 0; for(int i = 0; i < 10; i++) if(clus[i]){ int b = 32 - __builtin_clz(clus[i]); int v = (i % 5) * 32 + b; if(v > top) top = v; }
    return top; }
static void pc_laws_env_for(PlwEnv *e, int sys_i, int pl, int moon, int story){
    memset(e, 0, sizeof *e); for(int i = 0; i < UW && i < 8; i++) e->uni[i] = uni_w[i];
    e->gal = gal_hash(); e->star = sys_i >= 0 && sys_i < NSYS ? sys_id(sys_i) : 0; e->seed_a = e->star ^ (pl ? hmix((u32)(pl * 8 + moon) * 0x9E3779B1u + 0x5EEDu) : 0) ^ (moon ? 0x4D4F4F4Eu : 0);
    e->seed_b = wd.tseed; e->seed_c = wd.cseed;
    e->story = story || !endless || sys_i < 0 || sys_i >= NSYS || sys[sys_i].kind < 6;
    e->planet = pl; e->moon = moon; e->nplan = 1; e->nmoon = 0;
    if(!e->story){ e->nplan = sys_nplan(sys_i); e->nmoon = sys_nmoon(sys_i, pl); }
    e->psz = wd.psz; e->grav = wd.grav; e->cls = wd.cls; e->hour = wd.hour; e->weather = wd.weather; e->snow = wd.snow; e->sand = wd.sand; e->city = wd.city; e->ruins = wd.ruins;
    e->fauna = wd.fauna; e->beetles = wd.beetles; e->rough = wd.rough; e->hs = wd.hs; e->mood = wd.mood; e->water = water_lv; e->snowl = wd.snowl; e->hell = hellw();
    e->dist_log2 = pc_lw_dist_log2();
}
static void pc_laws_env(PlwEnv *e, int story){ pc_laws_env_for(e, cur_sys, cur_pl, cur_moon, story); }

// ---------------------------------------------------------------- hooks (build.sh puts the calls in)
static int pc_nplan(int i, int orig){
    if(plw_is_default() || sys[i].kind < 6 || is_hive(i)) return orig;
    static u32 k_star, k_gal, k_h; static int k_o = -1, k_v;
    u32 st = sys_id(i), gh = gal_hash(); uint64_t h = plw_hash(); u32 hh = (u32)h ^ (u32)(h >> 32);
    if(k_o == orig && k_star == st && k_gal == gh && k_h == hh) return k_v;
    PlwEnv e; pc_laws_env_for(&e, i, 0, 0, 0); e.nplan = orig; int v = plw_nplan(&e, orig);
    k_star = st; k_gal = gh; k_h = hh; k_o = orig; k_v = v; return v; }
static int pc_nmoon(int i, int p, int orig){
    if(plw_is_default() || sys[i].kind < 6 || is_hive(i)) return orig;
    PlwEnv e; pc_laws_env_for(&e, i, p, 0, 0); e.nplan = 1; e.nmoon = orig; return plw_nmoon(&e, orig); }
static int pc_dense(u32 roll, int odds){ return plw_dense(roll, odds); }
static int pc_hell(int roll100, int hp){ return plw_hell_hit(roll100, hp); }
static int pc_weird(u32 f){ return plw_weird_hit(f); }
static int pc_uly(void){ return plw_uly(); }
static int pc_galbits(void){ return plw_galbits(); }
static int pc_warp(int c){ return plw_warp_cost(c); }
static u32 pc_hours(u32 h){ return plw_hours(h); }
static int pc_danger(int i){
    if(plw_is_default() || i < 0 || i >= NSYS || sys[i].kind < 6) return 0;
    PlwEnv e; pc_laws_env_for(&e, i, cur_pl, 0, 0); e.psz = 2; e.grav = 1; e.hour = 0; e.hs = 3; e.snowl = 200; e.water = 50; PlwApply a; plw_apply(&e, &a); return a.danger_add; }
static int pc_water(int w){
    if(pc_water_pct == 100) return w; int v = w * pc_water_pct / 100; return v < 0 ? 0 : v > 95 ? 95 : v; }

// what the laws do to the planet being made (called at the end of make_world, and for the six story worlds at the end of story_dna)
static void pc_laws_dna(int story){
    pc_tw = 128; pc_k16 = -1; pc_slow = 0; pc_day_pct = 100; pc_hun_pct = 100; pc_water_pct = 100; for(int i = 0; i < 4; i++) pc_node[i] = 0;
    pc_time_pct = plw_time_pct();
    if(plw_is_default() || cur_rock || kw_mode || forge_on) return;
    PlwEnv e; pc_laws_env(&e, story); PlwApply a; plw_apply(&e, &a);
    if(!a.changed) return;
    if(a.size_q) { wd.psz = (u8)a.psz; pc_k16 = a.k16; pc_tw = a.tw; }
    wd.grav = (u8)a.grav; pc_slow = a.slow; if(a.zerog && wlv[MW_ZEROG] < a.zerog) wlv[MW_ZEROG] = (u8)a.zerog;
    pc_day_pct = a.day_pct;
    if(e.story) return;                                           // the six story worlds follow only size and gravity
    if(a.hour >= 0) wd.hour = (u8)a.hour; if(a.weather >= 0) wd.weather = (u8)a.weather; if(a.snow >= 0) wd.snow = (u8)a.snow; if(a.sand >= 0) wd.sand = (u8)a.sand;
    if(a.cls >= 0 && !hellw()) wd.cls = (u8)a.cls;
    if(a.city >= 0){ wd.city = (u8)a.city; wd.csize = a.city ? (u8)((wd.cseed >> 8) & 3) : 0; } if(a.ruins >= 0) wd.ruins = (u8)a.ruins;
    if(a.fauna >= 0) wd.fauna = (u8)a.fauna; if(a.beetles >= 0) wd.beetles = (u8)a.beetles; if(a.rough >= 0) wd.rough = (u8)a.rough; if(a.hs >= 0) wd.hs = (u8)a.hs;
    if(a.mood >= 0) wd.mood = (u8)a.mood; if(a.snowl >= 0) wd.snowl = (u8)a.snowl;
    if(pc_tw < 128){ wd.city = 0; wd.csize = 0; wd.ruins = 0; }  // a pebble has no room for a town
    if(a.aurora && wlv[MW_AURORA] < a.aurora) wlv[MW_AURORA] = (u8)a.aurora; if(a.meteor && wlv[MW_METEOR] < a.meteor) wlv[MW_METEOR] = (u8)a.meteor; if(a.badair && wlv[MW_BADAIR] < a.badair) wlv[MW_BADAIR] = (u8)a.badair;
    pc_hun_pct = a.hunger_pct; pc_water_pct = a.water_pct; for(int i = 0; i < 4; i++) pc_node[i] = a.node_add[i];
}

// ---- tiny worlds: the walkable world repeats every pc_tw cells. The 128 x 128 ground is cut and cross-faded into a tile and repeated.
static void pc_tiny_hmap(void){
    int P = pc_tw; if(P >= 128 || P < 2) return;
    static u8 t[64 * 64]; int o = 64 - P;
    #define PCA(u,v) ((int)hmap[(((o + (v)) & 127) * 128) + ((o + (u)) & 127)])
    for(int y = 0; y < P; y++) for(int x = 0; x < P; x++){
        int u = x, v = y; int val = (PCA(u + P, v + P) * (P - u) * (P - v) + PCA(u, v + P) * u * (P - v) + PCA(u + P, v) * (P - u) * v + PCA(u, v) * u * v) / (P * P);
        t[y * P + x] = (u8)(val > 255 ? 255 : val); }
    #undef PCA
    for(int y = 0; y < 128; y++) for(int x = 0; x < 128; x++) hmap[y * 128 + x] = t[(y & (P - 1)) * P + (x & (P - 1))];
}
static void pc_tiny_fix(void){                                  // objects of a tiny world: wrapped into it, and only one in each cell
    if(pc_tw >= 128) return; s32 m = (pc_tw << 16) - 1; ppx &= m; ppy &= m;
    for(int i = 0; i < NPO; i++) if(po[i].alive){ po[i].x &= m; po[i].y &= m; }
    for(int i = 1; i < NPO; i++) if(po[i].alive && po[i].kind > O_CAGE){
        for(int j = 0; j < i; j++) if(po[j].alive && (po[j].x >> 16) == (po[i].x >> 16) && (po[j].y >> 16) == (po[i].y >> 16)){ po[i].alive = 0; break; } }
    cam_h = ground_h(ppx, ppy) + CAM_UP;
}

// ---------------------------------------------------------------- the laws of a world are kept beside the save file, one file per slot
static void pc_laws_file(int slot, char *out, int n){ const char *sv = pc_sav_path(); if(!sv){ out[0] = 0; return; } plw_slot_path(sv, slot, out, n); }
static void pc_laws_slot_load(int s){ char p[640]; pc_laws_file(s, p, sizeof p); if(p[0]) plw_load(p); else plw_reset_all(); pc_time_pct = plw_time_pct(); }
static void pc_laws_slot_save(int s){ char p[640]; pc_laws_file(s, p, sizeof p); if(p[0]) plw_save(p); }
static void pc_laws_slot_new(int s){ char p[640]; plw_reset_all(); plw_set_cheated(0); pc_laws_file(s, p, sizeof p); if(p[0]) plw_file_remove(p); pc_time_pct = 100; }
static void pc_laws_slot_erase(int s){ char p[640]; pc_laws_file(s, p, sizeof p); if(p[0]) plw_file_remove(p); }
static void pc_laws_slot_copy(int a, int b){ char pa[640], pb[640]; pc_laws_file(a, pa, sizeof pa); pc_laws_file(b, pb, sizeof pb); if(pa[0] && pb[0]) plw_file_copy(pa, pb); }

// ---------------------------------------------------------------- PC LAB (stub until the screens below)
static void pc_lab_screen(volatile u16 *pg){ pc_world_screen(pg); }
