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
    if(!e->story){ e->nplan = sys_nplan_g(sys_i); e->nmoon = sys_nmoon_g(sys_i, pl); }   // (the game's own counts: the wrappers call this)
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
    { int keep = pc_tw / 32; for(int i = 0; i < NPO; i++) if(po[i].alive && (po[i].kind == O_SLUG || po[i].kind == O_BEETLE || po[i].kind == O_CRIT)){ if(keep > 0) keep--; else po[i].alive = 0; } }     // (no room for things that bite on a pebble)
    cam_h = ground_h(ppx, ppy) + CAM_UP;
}

// ---------------------------------------------------------------- the laws of a world are kept beside the save file, one file per slot
static void pc_laws_file(int slot, char *out, int n){ const char *sv = pc_sav_path(); if(!sv){ out[0] = 0; return; } plw_slot_path(sv, slot, out, n); }
static void pc_laws_slot_load(int s){ char p[640]; pc_laws_file(s, p, sizeof p); if(p[0]) plw_load(p); else plw_reset_all(); pc_time_pct = plw_time_pct(); }
static void pc_laws_slot_save(int s){ char p[640]; pc_laws_file(s, p, sizeof p); if(p[0]) plw_save(p); }
static void pc_laws_slot_new(int s){ char p[640]; plw_reset_all(); plw_set_cheated(0); pc_laws_file(s, p, sizeof p); if(p[0]) plw_file_remove(p); pc_time_pct = 100; }
static void pc_laws_slot_erase(int s){ char p[640]; pc_laws_file(s, p, sizeof p); if(p[0]) plw_file_remove(p); }
static void pc_laws_slot_copy(int a, int b){ char pa[640], pb[640]; pc_laws_file(a, pa, sizeof pa); pc_laws_file(b, pb, sizeof pb); if(pa[0] && pb[0]) plw_file_copy(pa, pb); }

// ================================================================= THE SCREENS
// ---- little helpers
static int pc_lw_col(int c){ return c == 1 ? C_GOLD : c == 2 ? C_GREY : c == 3 ? C_CYAN : c == 4 ? C_GREEN : c == 5 ? C_RED : C_WHITE; }
static void pc_lw_fit(char *o, const char *s, int n){ int i = 0; while(s[i] && i < n){ o[i] = s[i]; i++; } o[i] = 0; }
static int pc_lw_wrap(const char *s, int w, char lines[][60], int max){     // word wrap into at most max lines of w letters
    int n = 0, L = 0; char cur[60]; cur[0] = 0; char word[60]; int wl = 0;
    for(int i = 0;; i++){
        char c = s[i];
        if(c == ' ' || c == 0){
            if(wl){ word[wl] = 0; if(L && L + 1 + wl > w){ if(n < max){ scpy(lines[n], cur); n++; } cur[0] = 0; L = 0; } if(L){ cur[L++] = ' '; cur[L] = 0; } for(int k = 0; k <= wl; k++) cur[L + k] = word[k]; L += wl; wl = 0; }
            if(c == 0) break;
        } else if(wl < 58) word[wl++] = c;
    }
    if(L && n < max){ scpy(lines[n], cur); n++; }
    return n;
}
static int pc_lw_rep(int key, int *hold){                       // press once, then repeat while held: returns 0 no, 1 a step, 2 a big step (held a long time)
    if(!HELD(key)){ *hold = 0; return 0; }
    int h = (*hold)++; if(h == 0) return 1; if(h >= 24 && h % 4 == 0) return h >= 100 ? 2 : 1; return 0; }
static void pc_lw_status(char *b){                              // "NORMAL", "3 CHANGED", "CHEATED"
    int n = plw_edited_count(); if(plw_cheated()) scpy(b, "CHEATED"); else if(!n) scpy(b, "ALL NORMAL"); else { b[0] = 0; ncat(b, n); scat(b, n == 1 ? " CHANGED" : " CHANGED"); } }
static int pc_lw_confirm(volatile u16 *pg, const char *a, const char *b){ int r = confirm(pg, a, b); return r; }

// ---- HOW THIS WORKS
static void pc_how_screen(volatile u16 *pg){
    static PlwPage pages[PLW_PAGES]; PlwEnv e; pc_laws_env(&e, 0); int np = plw_explain(&e, pages), page = 0, dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); PlwPage *q = &pages[page]; char b[48];
            ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143);
            scpy(b, "HOW THIS WORKS: "); scat(b, q->title); ui_head(pg, 9, 8, 222, b, C_GOLD);
            { char pn[16]; pn[0] = 0; ncat(pn, page + 1); scat(pn, "/"); ncat(pn, np); rtext(pg, 231, 8, pn, U_SOFT); }
            for(int i = 0; i < q->n; i++) text(pg, 9, 19 + i * 7, q->t[i], pc_lw_col(q->col[i]), 1);
            ui_foot(pg, "L R: PAGE", "B: BACK"); }
        vsync(); read_keys(); pc_lw_clock++;
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_L) || HIT(K_LEFT)){ page = (page + np - 1) % np; dirty = 1; sfx(7); }
        if(HIT(K_R) || HIT(K_RIGHT) || HIT(K_A)){ page = (page + 1) % np; dirty = 1; sfx(7); }
    }
    wait_release();
}

// ---- THE LAWS EDITOR (cheat = 1: the wide range, from the CHEATS screen)
static int pc_lv_list[200];
static int pc_lv_build(int tab, int adv, const char *q, int *rows_are_presets){      // the ids on show; -1 = the last row (reset all)
    int n = 0; *rows_are_presets = 0;
    if(q && q[0]){ for(int i = 0; i < plw_count(); i++) if(plw_match(i, q)) pc_lv_list[n++] = i; }
    else if(tab == 0){ *rows_are_presets = 1; for(int i = 0; i < plw_preset_count(); i++) pc_lv_list[n++] = i; }
    else { for(int i = 0; i < plw_count(); i++) if(plw_cat(i) == tab - 1 && plw_visible(i, adv)) pc_lv_list[n++] = i; }
    pc_lv_list[n++] = -1; return n; }
static void pc_laws_apply_side_effects(void){ pc_time_pct = plw_time_pct(); }
static int pc_laws_editor(volatile u16 *pg, int cheat){        // returns 1 when the player asked to regenerate the planet now
    static const char *const tabn[7] = { "PRESETS", "UNIVERSE", "GALAXY", "STARS", "PLANETS", "LIFE", "TIME" };
    int tab = 0, sel = 0, top = 0, adv = 0, dirty = 1, focus = 1, hl = 0, hr = 0, menu = 0, msel = 0, regen = 0, mode = 0;       // focus 0 = the tab bar, 1 = the list; mode 1 = typing a search
    char q[20]; q[0] = 0; int qn = 0, qc = 0; int isp = 1; int n; char note[48]; note[0] = 0; int note_t = 0;
    char live[2][PLW_W]; int nlive = 0, live_id = -2, live_h = 0; PlwEnv env; pc_laws_env(&env, 0);
    static const int MAXR = 9;
    wait_release(); n = pc_lv_build(tab, adv, q, &isp);
    while(1){
        if(sel >= n) sel = n - 1; if(sel < 0) sel = 0; if(sel < top) top = sel; if(sel >= top + MAXR) top = sel - MAXR + 1; if(top < 0) top = 0;
        int cur = (n && pc_lv_list[sel] >= 0) ? pc_lv_list[sel] : -1;
        if(!isp && cur >= 0){ uint64_t hh = plw_hash(); int h2 = (int)(hh ^ (hh >> 32)); if(live_id != cur || live_h != h2){ live_id = cur; live_h = h2; nlive = plw_live(&env, cur, live, 2); dirty = 1; } }
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); char b[64];
            ui_bg(pg, 0, 160);
            scpy(b, cheat ? "CHEATS: SET ANY LAW" : "UNIVERSE LAWS"); ui_head(pg, 6, 4, 150, b, cheat ? C_RED : C_GOLD);
            { char st[24]; pc_lw_status(st); rtext(pg, 234, 4, st, plw_cheated() ? C_RED : plw_edited_count() ? C_GOLD : C_GREEN); }
            for(int t = 0; t < 7; t++){ int x = 4 + t * 33; int on = (t == tab && !(q[0]));
                if(on){ ui_sel(pg, x, 13, 32, 10, focus == 0 ? 6 : U_RAISE); }
                else rect(pg, x + 1, 13, 30, 10, U_PANEL);
                ctext(pg, x + 16, 16, tabn[t], on ? C_WHITE : C_GREY); }
            ui_panel(pg, 4, 25, 232, 87);
            for(int r = 0; r < MAXR; r++){ int i = top + r; if(i >= n) break; int id = pc_lv_list[i]; int y = 28 + r * 9; int on = (i == sel && focus == 1);
                if(on) rect(pg, 7, y - 1, 220, 9, 6);
                if(id < 0){ text(pg, 12, y, "BACK TO NORMAL (RESET EVERYTHING)", on ? C_WHITE : C_GREY, 1); continue; }
                if(isp){ text(pg, 12, y, plw_preset_name(id), on ? C_WHITE : C_GREY, 1); continue; }
                char v[40]; plw_value_text(id, adv, v, sizeof v); int ed = plw_is_edited(id);
                if(ed) text(pg, 8, y, "*", C_GOLD, 1);
                { char nm[28]; pc_lw_fit(nm, plw_name(id), 24); text(pg, 14, y, nm, on ? C_WHITE : ed ? C_GOLD : C_GREY, 1); }
                { char vb[48]; vb[0] = 0; if(on) scat(vb, "< "); scat(vb, v); if(on) scat(vb, " >"); int col = !plw_in_sensible(id) ? C_RED : ed ? C_GOLD : on ? C_WHITE : C_GREY; rtext(pg, 224, y, vb, col); } }
            if(top > 0) text(pg, 229, 29, "^", C_CYAN, 1); if(top + MAXR < n) text(pg, 229, 101, "v", C_CYAN, 1);
            ui_panel(pg, 4, 113, 232, 35);
            if(isp && cur < 0 && n > 0 && pc_lv_list[sel] < 0){ text(pg, 9, 117, "PUTS EVERY LAW BACK TO NORMAL. YOU CAN UNDO IT (START).", C_GREY, 1); }
            else if(isp && n > 0){ char ls[3][60]; int k = pc_lw_wrap(plw_preset_desc(pc_lv_list[sel]), 56, ls, 3); for(int i = 0; i < k; i++) text(pg, 9, 117 + i * 7, ls[i], C_GREY, 1); text(pg, 9, 138, "A: USE THIS PRESET", C_CYAN, 1); }
            else if(cur >= 0){ char ls[3][60]; int k = pc_lw_wrap(plw_desc(cur), 56, ls, 3); if(k > 2) k = 2; for(int i = 0; i < k; i++) text(pg, 9, 116 + i * 7, ls[i], C_GREY, 1);
                for(int i = 0; i < nlive && i < 2; i++) text(pg, 9, 131 + i * 7, live[i], C_CYAN, 1); if(!nlive && !k) text(pg, 9, 131, "", C_CYAN, 1);
                { char u[40]; u[0] = 0; scpy(u, plw_unit(cur)); if(!nlive){ char nb[60]; scpy(nb, "UNIT: "); scat(nb, u); text(pg, 9, 131, nb, C_DGREY, 1); } } }
            else if(n > 0){ text(pg, 9, 117, "PUTS EVERY LAW BACK TO NORMAL. YOU CAN UNDO IT (START).", C_GREY, 1); }
            if(note_t > 0){ rect(pg, 4, 100, 232, 11, U_BG0); ctext(pg, 120, 103, note, C_GOLD); }
            if(q[0] || mode){ rect(pg, 4, 100, 232, 11, U_BG0); char sb[40]; scpy(sb, "SEARCH: "); scat(sb, q); if(mode){ int L = textlen(sb); sb[L] = qc == 26 ? ' ' : (char)('A' + qc); sb[L + 1] = '_'; sb[L + 2] = 0; } text(pg, 8, 103, sb, C_CYAN, 1); }
            if(mode) ui_foot(pg, "UP/DOWN: LETTER  RIGHT: ADD  LEFT: DELETE", "A: DONE");
            else if(focus == 0) ui_foot(pg, "LEFT/RIGHT: TAB  DOWN: LIST", "START: MENU  B: BACK");
            else if(isp) ui_foot(pg, "A: USE  L R: TAB", "SEL: FIND  START: MENU  B: BACK");
            else ui_foot(pg, "LEFT/RIGHT: CHANGE  A: NORMAL", "START: MENU  B: BACK");
            if(menu){
                static const char *const mn[7] = { "ADVANCED NUMBERS", "UNDO LAST CHANGE", "SEARCH BY WORD", "MAKE THIS PLANET AGAIN NOW", "RESET EVERYTHING", "HOW THIS WORKS", "CLOSE THIS MENU" };
                panel(pg, 50, 30, 140, 92, "MENU");
                for(int i = 0; i < 7; i++){ int y = 47 + i * 10; if(i == msel) rect(pg, 54, y - 1, 132, 10, 6);
                    char mb[40]; scpy(mb, mn[i]); if(i == 0){ scat(mb, adv ? ": ON" : ": OFF"); } if(i == 1 && plw_undo_depth()){ scat(mb, " ("); ncat(mb, plw_undo_depth()); scat(mb, ")"); }
                    text(pg, 58, y, mb, i == msel ? C_WHITE : C_GREY, 1); } }
        }
        vsync(); read_keys(); pc_lw_clock++; if(note_t > 0){ note_t--; if(!note_t) dirty = 1; }
        // ---- the menu
        if(menu){
            if(HIT(K_UP)){ msel = (msel + 6) % 7; sfx(7); dirty = 1; } if(HIT(K_DOWN)){ msel = (msel + 1) % 7; sfx(7); dirty = 1; }
            if(HIT(K_B) || HIT(K_START)){ menu = 0; dirty = 1; wait_release(); continue; }
            if(HIT(K_A) || ((msel == 0) && (HIT(K_LEFT) || HIT(K_RIGHT)))){ dirty = 1;
                if(msel == 0){ adv ^= 1; sfx(2); n = pc_lv_build(tab, adv, q, &isp); }
                else if(msel == 1){ if(plw_undo_pop()){ sfx(4); scpy(note, "UNDONE"); pc_laws_apply_side_effects(); } else { scpy(note, "NOTHING TO UNDO"); sfx(0); } note_t = 60; menu = 0; }
                else if(msel == 2){ menu = 0; mode = 1; qc = 0; focus = 1; }
                else if(msel == 3){ if(scene != 0){ scpy(note, "GO TO A PLANET FIRST"); sfx(0); note_t = 90; menu = 0; } else { menu = 0; if(pc_lw_confirm(pg, "SAVE AND MAKE THIS PLANET AGAIN?", "WITH THE NEW LAWS")) { regen = 1; break; } } }
                else if(msel == 4){ menu = 0; if(plw_edited_count() && pc_lw_confirm(pg, "PUT EVERY LAW BACK TO NORMAL?", "YOU CAN UNDO THIS")){ plw_undo_push(); plw_reset_all(); pc_laws_apply_side_effects(); scpy(note, "EVERYTHING IS NORMAL AGAIN"); note_t = 90; sfx(4); } }
                else if(msel == 5){ menu = 0; pc_how_screen(pg); }
                else menu = 0;
                wait_release(); dirty = 1; }
            continue;
        }
        // ---- typing a search
        if(mode){
            if(HIT(K_UP)){ qc = (qc + 26) % 27; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ qc = (qc + 1) % 27; dirty = 1; sfx(7); }
            if(HIT(K_RIGHT) && qn < 16){ q[qn++] = qc == 26 ? ' ' : (char)('A' + qc); q[qn] = 0; n = pc_lv_build(tab, adv, q, &isp); sel = 0; top = 0; dirty = 1; sfx(2); }
            if(HIT(K_LEFT) && qn > 0){ q[--qn] = 0; n = pc_lv_build(tab, adv, q, &isp); sel = 0; top = 0; dirty = 1; sfx(4); }
            if(HIT(K_A)){ mode = 0; while(qn && q[qn - 1] == ' ') q[--qn] = 0; n = pc_lv_build(tab, adv, q, &isp); dirty = 1; wait_release(); }
            if(HIT(K_B) || HIT(K_START)){ mode = 0; q[0] = 0; qn = 0; n = pc_lv_build(tab, adv, q, &isp); sel = 0; top = 0; dirty = 1; wait_release(); }
            continue;
        }
        if(HIT(K_START)){ menu = 1; msel = 0; dirty = 1; sfx(7); continue; }
        if(HIT(K_SEL)){ mode = 1; qc = 0; focus = 1; dirty = 1; sfx(7); continue; }
        if(HIT(K_B)){ if(q[0]){ q[0] = 0; qn = 0; n = pc_lv_build(tab, adv, q, &isp); sel = 0; top = 0; dirty = 1; sfx(4); wait_release(); continue; } break; }
        // ---- moving about: UP from the first row goes to the tabs, DOWN from the tabs goes back to the list
        if(focus == 0){
            if(HIT(K_DOWN)){ focus = 1; dirty = 1; sfx(7); }
            if(HIT(K_LEFT) || HIT(K_L)){ tab = (tab + 6) % 7; sel = 0; top = 0; n = pc_lv_build(tab, adv, q, &isp); dirty = 1; sfx(7); }
            if(HIT(K_RIGHT) || HIT(K_R)){ tab = (tab + 1) % 7; sel = 0; top = 0; n = pc_lv_build(tab, adv, q, &isp); dirty = 1; sfx(7); }
            continue; }
        if(HIT(K_UP)){ if(sel == 0 && !q[0]) focus = 0; else if(sel > 0) sel--; else sel = n - 1; dirty = 1; sfx(7); }
        if(HIT(K_DOWN)){ sel = (sel + 1) % n; dirty = 1; sfx(7); }
        if(!q[0]){ if(HIT(K_L)){ tab = (tab + 6) % 7; sel = 0; top = 0; n = pc_lv_build(tab, adv, q, &isp); dirty = 1; sfx(7); continue; } if(HIT(K_R)){ tab = (tab + 1) % 7; sel = 0; top = 0; n = pc_lv_build(tab, adv, q, &isp); dirty = 1; sfx(7); continue; } }
        if(n && pc_lv_list[sel] < 0){                                 // the last row: A puts everything back to normal
            if(HIT(K_A)){ if(plw_edited_count()){ if(pc_lw_confirm(pg, "PUT EVERY LAW BACK TO NORMAL?", "YOU CAN UNDO THIS (START)")){ plw_undo_push(); plw_reset_all(); pc_laws_apply_side_effects(); scpy(note, "EVERYTHING IS NORMAL AGAIN"); note_t = 90; sfx(4); } } else { scpy(note, "IT ALL IS NORMAL ALREADY"); note_t = 60; } dirty = 1; wait_release(); }
            continue; }
        if(isp){                                                      // a preset: A uses it
            if(HIT(K_A)){ plw_undo_push(); plw_preset_apply(pc_lv_list[sel]); pc_laws_apply_side_effects(); scpy(note, "DONE. NEW PLANETS FOLLOW IT. START: UNDO"); note_t = 120; sfx(6); dirty = 1; }
            continue; }
        { int id = pc_lv_list[sel]; int ra = pc_lw_rep(K_RIGHT, &hr), la = pc_lw_rep(K_LEFT, &hl); int big = HELD(K_L) || HELD(K_R);
          if(HIT(K_R) && 0) big = 1;
          int dir = ra ? 1 : la ? -1 : 0; int bg = ra == 2 || la == 2 || (dir && big);
          if(dir){ if((ra == 1 && hr == 1) || (la == 1 && hl == 1)) plw_undo_push(); plw_step(id, dir, bg, cheat); pc_laws_apply_side_effects(); dirty = 1; sfx(2); }
          if(HIT(K_A) && plw_is_edited(id)){ plw_undo_push(); plw_reset_one(id); pc_laws_apply_side_effects(); dirty = 1; sfx(4); } }
    }
    pc_laws_slot_save(cur_slot); wait_release(); return regen;
}

// ---- CHEATS
static const char *const pc_ch_size_n[] = { "A PEBBLE", "A HOUSE", "AN ASTEROID", "A MOON", "EARTH", "JUPITER", "THE SUN", "A GIANT STAR", "A SOLAR SYSTEM", "A LIGHT YEAR", "THE BIGGEST" };
static const int pc_ch_size_v[] = { -113, -77, -37, -7, 0, 14, 27, 68, 82, 122, 176 };
static void pc_ch_tp(volatile u16 *pg, int *go);                  // the teleport screen
static int pc_ch_on;                                              // the CHEATS switch (for this session; the mark on the world is kept)
static int pc_cheats_screen(volatile u16 *pg){                    // returns 1 when the game must be reloaded (regenerate / teleport)
    enum { CH_SW, CH_LAWS, CH_SIZE, CH_SPEED, CH_LEVEL, CH_GOLD, CH_FILL, CH_TP, CH_REGEN, CH_N };
    static const char *const nm[CH_N] = { "CHEATS", "SET ANY LAW", "JUMP TO A PLANET SIZE", "GAME SPEED", "YOUR LEVEL", "GIVE GOLD", "FILL HEALTH, FOOD AND FUEL", "TELEPORT BY ADDRESS", "MAKE THIS PLANET AGAIN" };
    static const char *const hp[CH_N] = { "OFF: NOTHING HERE CAN CHANGE YOUR GAME. ON: THE WORLD IS MARKED CHEATED (IT IS KEPT IN ITS LAWS FILE).", "ANY LAW, FAR BEYOND THE SENSIBLE RANGE. THE GAME MAY GET VERY STRANGE. UNDO IS IN THE MENU.", "LEFT/RIGHT PICKS A SIZE, A SETS IT. NEW PLANETS ARE THIS BIG. PEBBLES ARE A FEW STEPS ROUND.", "HOW FAST THE WHOLE GAME RUNS, FROM A CRAWL TO A BLUR.", "LEFT/RIGHT: ONE LEVEL. HOLD IT FOR BIG JUMPS. YOUR STATS FOLLOW.", "LEFT/RIGHT PICKS HOW MUCH, A GIVES IT.", "A FILLS YOUR HEARTS, MANA, FOOD AND SHIP FUEL.", "TYPE A PLACE IN THE UNIVERSE AND GO THERE (ENDLESS GALAXY ONLY).", "SAVES, THEN MAKES THIS PLANET AGAIN WITH THE LAWS YOU HAVE NOW." };
    static const char *const gn[5] = { "1,000", "1 MILLION", "1 BILLION", "1 TRILLION", "1 QUADRILLION" }; static const s64 gv[5] = { 1000LL, 1000000LL, 1000000000LL, 1000000000000LL, 1000000000000000LL };
    int sel = 0, top = 0, dirty = 1, hl = 0, hr = 0, szi = 4, gi = 1, ret = 0; char note[56]; note[0] = 0; int note_t = 0;
    wait_release();
    while(1){
        if(sel < top) top = sel; if(sel >= top + 8) top = sel - 7;
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); char b[64];
            ui_bg(pg, 0, 160); rect(pg, 0, 0, 240, 12, 0x70 & 0 ? 0 : C_DGREY); rect(pg, 0, 12, 240, 1, C_RED);
            text(pg, 6, 3, "CHEATS", C_RED, 1); text(pg, 40, 3, plw_cheated() ? "THIS WORLD IS MARKED CHEATED" : "THIS WORLD HAS NEVER BEEN CHEATED", plw_cheated() ? C_RED : C_GREY, 1);
            ui_panel(pg, 4, 16, 232, 84);
            for(int r = 0; r < 8; r++){ int i = top + r; if(i >= CH_N) break; int y = 20 + r * 10, on = (i == sel); if(on) rect(pg, 7, y - 1, 220, 10, 6);
                text(pg, 12, y, nm[i], on ? C_WHITE : (i != CH_SW && !pc_ch_on ? C_DGREY : C_GREY), 1);
                b[0] = 0;
                if(i == CH_SW) scpy(b, pc_ch_on ? "< ON >" : "< OFF >");
                else if(i == CH_SIZE){ scpy(b, "< "); scat(b, pc_ch_size_n[szi]); scat(b, " >"); }
                else if(i == CH_SPEED){ plw_value_text(plw_find("T_SPEED"), 1, b + 0, 20); { char t2[40]; scpy(t2, "< "); scat(t2, b); scat(t2, " >"); scpy(b, t2); } }
                else if(i == CH_LEVEL){ scpy(b, "< "); ncat(b, level); scat(b, " >"); }
                else if(i == CH_GOLD){ scpy(b, "< "); scat(b, gn[gi]); scat(b, " >"); }
                if(b[0]) rtext(pg, 224, y, b, on ? C_GOLD : C_GREY); }
            if(top > 0) text(pg, 229, 21, "^", C_CYAN, 1); if(top + 8 < CH_N) text(pg, 229, 91, "v", C_CYAN, 1);
            ui_panel(pg, 4, 102, 232, 45);
            { char ls[4][60]; int k = pc_lw_wrap(hp[sel], 56, ls, 3); for(int i = 0; i < k; i++) text(pg, 9, 106 + i * 7, ls[i], C_GREY, 1); }
            if(!pc_ch_on && sel != CH_SW) text(pg, 9, 130, "CHEATS ARE OFF. GO UP TO THE SWITCH AND PRESS A", C_RED, 1);
            else if(note_t > 0) text(pg, 9, 130, note, C_GOLD, 1);
            ui_foot(pg, "A: DO IT  LEFT/RIGHT: CHANGE", "B: BACK"); }
        vsync(); read_keys(); pc_lw_clock++; if(note_t > 0){ note_t--; if(!note_t) dirty = 1; }
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_UP)){ sel = (sel + CH_N - 1) % CH_N; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ sel = (sel + 1) % CH_N; dirty = 1; sfx(7); }
        int ra = pc_lw_rep(K_RIGHT, &hr), la = pc_lw_rep(K_LEFT, &hl), dir = ra ? 1 : la ? -1 : 0, fast = ra == 2 || la == 2; int A = HIT(K_A);
        int on = pc_ch_on;
        if(sel == CH_SW && (A || (dir && ((ra == 1 && hr == 1) || (la == 1 && hl == 1))))){
            if(!on){ if(plw_cheated() || pc_lw_confirm(pg, "TURN CHEATS ON?", "THIS WORLD IS MARKED CHEATED")){ pc_ch_on = 1; plw_set_cheated(1); pc_laws_slot_save(cur_slot); scpy(note, "CHEATS ARE ON"); note_t = 90; sfx(6); } }
            else { pc_ch_on = 0; scpy(note, "CHEATS ARE OFF. THE MARK STAYS ON THIS WORLD"); note_t = 120; sfx(4); }
            dirty = 1; wait_release(); continue; }
        if(!on){ if(A || dir){ if(note_t <= 0){ sfx(0); } dirty = 1; } continue; }
        if(sel == CH_LAWS && A){ int rg = pc_laws_editor(pg, 1); dirty = 1; if(rg){ do_save(); sys_req = 1; ret = 1; break; } }
        if(sel == CH_SIZE){ if(dir && ((ra == 1 && hr == 1) || (la == 1 && hl == 1))){ szi = (szi + dir + 11) % 11; dirty = 1; sfx(7); }
            if(A){ plw_undo_push(); plw_setv(plw_find("P_SIZE"), pc_ch_size_v[szi], 1); plw_set_cheated(1); pc_laws_slot_save(cur_slot); scpy(note, "SIZE SET. NEW PLANETS ARE THAT BIG"); note_t = 120; sfx(6); dirty = 1; } }
        if(sel == CH_SPEED && dir){ int id = plw_find("T_SPEED"); if((ra == 1 && hr == 1) || (la == 1 && hl == 1) || fast){ plw_undo_push(); plw_step(id, dir, fast, 1); plw_set_cheated(1); pc_laws_apply_side_effects(); dirty = 1; sfx(2); } }
        if(sel == CH_LEVEL && dir){ if((ra == 1 && hr < 24) || ra == 2 || (la == 1 && hl < 24) || la == 2 || (ra == 1 || la == 1)){
                s64 st = 1; int h = ra ? hr : hl; if(h >= 100) st = level / 4 + 1; else if(h >= 40) st = level / 20 + 1; s64 nl = dir > 0 ? sadd(level, st) : level - st; if(nl < 1) nl = 1; if(nl > LEVEL_CAP) nl = LEVEL_CAP;
                if(nl != level){ if(nl > level) stat_pts = sadd(stat_pts, smul(nl - level, 5)); level = nl; xp = 0; recalc(); hearts = maxh; mp = maxmp; hud_now(); dirty = 1; } } }
        if(sel == CH_GOLD){ if(dir && ((ra == 1 && hr == 1) || (la == 1 && hl == 1))){ gi = (gi + dir + 5) % 5; dirty = 1; sfx(7); }
            if(A){ gold = sadd(gold, gv[gi]); hud_now(); scpy(note, "GOLD GIVEN"); note_t = 90; sfx(6); dirty = 1; } }
        if(sel == CH_FILL && A){ recalc(); hearts = maxh; mp = maxmp; food = 100; pet_hp = pet_maxhp; fuel = fuel_max(); hull = 100; hud_now(); scpy(note, "ALL FULL"); note_t = 90; sfx(6); dirty = 1; }
        if(sel == CH_TP && A){ int go = 0; pc_ch_tp(pg, &go); dirty = 1; if(go){ ret = 1; break; } }
        if(sel == CH_REGEN && A){ if(scene != 0){ scpy(note, "GO TO A PLANET FIRST"); note_t = 90; sfx(0); dirty = 1; } else if(pc_lw_confirm(pg, "SAVE AND MAKE THIS PLANET AGAIN?", "WITH THE LAWS YOU HAVE NOW")){ do_save(); sys_req = 1; ret = 1; break; } else dirty = 1; }
    }
    pc_laws_slot_save(cur_slot); wait_release(); return ret;
}
// the teleport screen: two 39-digit hex numbers (east and north, in clusters) and a star number
static void pc_ch_tp(volatile u16 *pg, int *go){
    u32 a[UAX], b[UAX]; ax_get(0, a); ax_get(1, b); int star = cur_sys; if(star < 6 || star >= NSYS) star = 6; int row = 0, col = 38, dirty = 1, edit = 0;      // col counts from the left digit (38) down to 0
    char note[56]; note[0] = 0; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143);
            ui_head(pg, 9, 8, 222, "TELEPORT BY ADDRESS", C_RED);
            text(pg, 9, 20, "AN ADDRESS IS HOW FAR EAST AND NORTH OF HOME THE CLUSTER IS,", C_GREY, 1); text(pg, 9, 27, "COUNTED IN CLUSTERS, IN BASE 16. THEN WHICH STAR (6 TO 31).", C_GREY, 1);
            for(int r = 0; r < 2; r++){ u32 *w = r ? b : a; char hx[48]; for(int d = 0; d < 39; d++){ int nib = (w[(38 - d) / 8] >> (((38 - d) % 8) * 4)) & 15; hx[d] = (char)(nib < 10 ? '0' + nib : 'A' + nib - 10); } hx[39] = 0;
                int y = 42 + r * 22; text(pg, 9, y - 8, r ? "NORTH" : "EAST", row == r ? C_WHITE : C_GREY, 1);
                if(row == r) rect(pg, 8 + col * 0 + (38 - col) * 6 - 0, y - 1, 6, 9, 6);
                for(int d = 0; d < 39; d++){ char c1[2]; c1[0] = hx[d]; c1[1] = 0; text(pg, 9 + d * 5 + (d / 8) * 0, y, c1, row == r && d == 38 - col ? C_WHITE : C_GOLD, 1); } }
            { char sb[40]; scpy(sb, "STAR NUMBER: < "); ncat(sb, star); scat(sb, " >"); text(pg, 9, 92, sb, row == 2 ? C_WHITE : C_GREY, 1); if(row == 2) rect(pg, 7, 90, 2, 9, C_GOLD); }
            text(pg, 9, 108, "GO THERE NOW", row == 3 ? C_GREEN : C_GREY, 1); if(row == 3) rect(pg, 7, 106, 2, 9, C_GOLD);
            if(note[0]) text(pg, 9, 124, note, C_GOLD, 1);
            if(edit) ui_foot(pg, "UP/DOWN: CHANGE THE DIGIT  LEFT/RIGHT: MOVE", "A: DONE"); else ui_foot(pg, "UP/DOWN: ROW  A: EDIT OR GO", "SEL: ZERO  B: BACK"); }
        vsync(); read_keys(); pc_lw_clock++;
        if(edit){ u32 *w = row ? b : a; int sh = (col % 8) * 4, wi = col / 8; u32 nib = (w[wi] >> sh) & 15;
            if(HIT(K_UP)){ nib = (nib + 1) & 15; w[wi] = (w[wi] & ~(15u << sh)) | (nib << sh); w[UAX - 1] &= UTOP; dirty = 1; sfx(2); }
            if(HIT(K_DOWN)){ nib = (nib + 15) & 15; w[wi] = (w[wi] & ~(15u << sh)) | (nib << sh); w[UAX - 1] &= UTOP; dirty = 1; sfx(2); }
            if(HIT(K_LEFT)){ col = col < 38 ? col + 1 : 0; dirty = 1; sfx(7); } if(HIT(K_RIGHT)){ col = col > 0 ? col - 1 : 38; dirty = 1; sfx(7); }
            if(HIT(K_L)){ col = col + 8 > 38 ? 38 : col + 8; dirty = 1; sfx(7); } if(HIT(K_R)){ col = col - 8 < 0 ? 0 : col - 8; dirty = 1; sfx(7); }
            if(HIT(K_A) || HIT(K_B)){ edit = 0; dirty = 1; sfx(4); }
            continue; }
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_UP)){ row = (row + 3) % 4; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ row = (row + 1) % 4; dirty = 1; sfx(7); }
        if(row < 2){ u32 *w = row ? b : a;
            if(HIT(K_SEL)){ for(int i = 0; i < UAX; i++) w[i] = 0; dirty = 1; sfx(4); }
            if(HIT(K_A)){ edit = 1; dirty = 1; sfx(2); } }
        if(row == 2){ if(HIT(K_LEFT)){ star = star > 6 ? star - 1 : NSYS - 1; dirty = 1; sfx(7); } if(HIT(K_RIGHT)){ star = star < NSYS - 1 ? star + 1 : 6; dirty = 1; sfx(7); } }
        if(row == 3 && HIT(K_A)){
            if(!endless){ scpy(note, "ONLY IN THE ENDLESS GALAXY"); sfx(0); dirty = 1; }
            else if(scene != 0){ scpy(note, "GO TO A PLANET FIRST"); sfx(0); dirty = 1; }
            else { plw_set_cheated(1); ax_put(0, a); ax_put(1, b); dp_ix = 0; u_settle(); gen_cluster(); cur_sys = star; cur_pl = cur_moon = 0; do_save(); sys_req = 1; *go = 1; break; } }
    }
    wait_release();
}

static void pc_giant_hub(volatile u16 *pg);
// ---- THE LAB: the hub on the WORLD page
static void pc_lab_screen(volatile u16 *pg){
    enum { LB_FILE, LB_LAWS, LB_HOW, LB_GIANT, LB_CHEAT, LB_CREATE, LB_SIDE, LB_N };
    static const char *const nm[LB_N] = { "PLANET FILE", "UNIVERSE LAWS", "HOW THIS WORKS", "GIANT NUMBERS", "CHEATS", "PLANET CREATOR", "SIDE GAME" };
    static const char *const ds[LB_N] = { "EVERYTHING THE GAME KNOWS ABOUT THIS PLANET: ITS TRAITS, STATS, PEOPLES, CITY AND CREATURES.", "CHANGE THE NUMBERS THAT RUN THE UNIVERSE: GRAVITY, STARS, PLANET SIZES FROM PEBBLES TO GIANTS, LIFE, TIME.", "WHY THIS UNIVERSE, GALAXY, STAR AND PLANET ARE THE WAY THEY ARE, WITH THEIR REAL NUMBERS.", "A NUMBER WITH OVER A BILLION DIGITS TO READ IN FULL, THE 6-QUINTILLION-LETTER NAME OF BUGSTARILLION V3, A NAMER AND A SCALE LADDER.", "WIDE-OPEN POWERS: ANY LAW, ANY SIZE, TIME SPEED, LEVEL, GOLD, TELEPORT. MARKS THE WORLD AS CHEATED.", "MAKE YOUR OWN PLANET AND PEOPLE, SEE ITS BIOFOOTPRINT LIVE, AND LAND ON IT.", "THE TOWN'S JOURNAL AND PEOPLE, THE GOD POWERS (ONE OPENS EVERY 1,000 LEVELS), SANDBOX AND YOUR BIG LEVEL." };
    int sel = 0, dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); char b[40]; ui_bg(pg, 0, 160); ui_panel(pg, 24, 14, 192, 126);
            ui_head(pg, 30, 19, 180, "PC LAB", C_GOLD); pc_lw_status(b); rtext(pg, 210, 19, b, plw_cheated() ? C_RED : plw_edited_count() ? C_GOLD : C_GREEN);
            for(int i = 0; i < LB_N; i++){ int y = 30 + i * 10; if(i == sel) rect(pg, 30, y - 2, 180, 10, 6); text(pg, 38, y, nm[i], i == LB_CHEAT ? (i == sel ? C_RED : C_RED) : i == sel ? C_WHITE : C_GREY, 1);
                if(i == LB_LAWS){ scpy(b, plw_edited_count() ? "EDITED" : "NORMAL"); rtext(pg, 204, y, b, plw_edited_count() ? C_GOLD : C_DGREY); } }
            { char ls[4][60]; int k = pc_lw_wrap(ds[sel], 44, ls, 4); for(int i = 0; i < k; i++) text(pg, 32, 104 + i * 7, ls[i], C_GREY, 1); }
            ui_foot(pg, "UP/DOWN: PICK  A: OPEN", "B: BACK"); }
        vsync(); read_keys(); pc_lw_clock++;
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_UP)){ sel = (sel + LB_N - 1) % LB_N; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ sel = (sel + 1) % LB_N; dirty = 1; sfx(7); }
        if(HIT(K_A)){ sfx(2); int rg = 0;
            if(sel == LB_FILE) pc_world_screen(pg);
            else if(sel == LB_LAWS) rg = pc_laws_editor(pg, 0);
            else if(sel == LB_HOW) pc_how_screen(pg);
            else if(sel == LB_CREATE){ pc_creator_screen(pg, 1); if(sys_req) rg = 1; }
            else if(sel == LB_GIANT) pc_giant_hub(pg);
            else if(sel == LB_SIDE) sim_lab(pg);
            else rg = pc_cheats_screen(pg);
            dirty = 1; if(rg){ if(!sys_req){ do_save(); sys_req = 1; } break; } wait_release(); }
    }
    pc_laws_slot_save(cur_slot); wait_release();
}
