// shipyard_ui.inc: the PC SHIP EDITOR screen (PC only). #included at the end of the generated main.c, so it can use the game's drawing.
// It replaces the hangar on the PC (make_ship -> pc_sy_screen; the old hangar is one row away in FILE). Tabs: SHIP HULL WINGS POWER GUNS
// GUARD ROOMS PAINT BADGE FILE. UP DOWN pick, LEFT RIGHT change, A opens/does, L R tab, SELECT tap: surprise me, hold SELECT + arrows: turn
// the view, START or right click: undo, B: done. Mouse: click tabs and rows, wheel scrolls rows / zooms the view, drag the view to turn it,
// drag sections in the HULL plan to stretch them. The design is saved per save slot as a small recipe (sy_files.h) and the game's ship
// numbers come from it through pc_sy_stat (hooks in pc/build.sh). No design file = the game's own ship, so old saves play as before.
#include "sy_design.h"
#include "sy_files.h"
double sin(double), cos(double), sqrt(double);

enum { SYP_PAINT = 168, SYP_TRIM = 176, SYP_METAL = 184, SYP_GLOW = 192, SYP_GLASS = 200, SYP_GUN = 208, SYP_EMB = 216, SYP_HI = 224, SYP_SW = 232 };
#define SY_PX 119
#define SY_PY 13
#define SY_PW 120
#define SY_PH 60
static SyDesign sy_d, sy_d0; static SyHist sy_h; static SyCursor sy_c; static SyMesh sy_m; static SyStats sy_s, sy_s0;
static int sy_init, sy_tab, sy_row[SY_NTAB], sy_top[SY_NTAB], sy_note_t, sy_hold[4], sy_drag, sy_dx0, sy_dy0, sy_dv0, sy_dv1;
static char sy_note[64]; static u32 sy_clock; static uint64_t sy_rngs = 0x5EEDB0A7ull; static int sy_yaw = 300, sy_pitch = 40, sy_zoom = 100, sy_spin = 1, sy_mesh_ok;
static u16 sy_savepal[256];

// ---------------------------------------------------------------- the game hooks (pc/build.sh renames the ship.inc stat functions to *_g)
static int pc_sy_slot = -1, pc_sy_on, pc_sy_foes; static SyGame pc_sy_gm; static SyDesign pc_sy_des;
typedef struct { int ok; float v[SY_MAXV * 3]; uint16_t tri[SY_MAXF * 6]; uint32_t tcol[SY_MAXF * 2]; SrMesh m; float scale; } PcSyMesh;
static PcSyMesh pc_sy_fm[12], pc_sy_mc; static int pc_sy_mc_ok; static SyMesh pc_sy_tmp;
static uint32_t sy_rgb15(u16 c, int k, int fin){ extern u16 sy_shade_p(u16, int, int); u16 q = sy_shade_p(c, k, fin); int r = q & 31, g = (q >> 5) & 31, b = (q >> 10) & 31; return (uint32_t)(r << 3 | r >> 2) << 16 | (uint32_t)(g << 3 | g >> 2) << 8 | (uint32_t)(b << 3 | b >> 2); }
static void sy_srmesh(const SyDesign *d, PcSyMesh *o){      // the design as triangles for the PC ship renderer (x right, y up, z forward: our y and z are flipped)
    sy_mesh(d, &pc_sy_tmp); const SyMesh *m = &pc_sy_tmp; int nt = 0;
    for(int i = 0; i < m->nv; i++){ o->v[i * 3] = m->v[i].x; o->v[i * 3 + 1] = -m->v[i].y; o->v[i * 3 + 2] = -m->v[i].z; }
    uint32_t cl[7] = { sy_rgb15(d->paint, 5, d->finish), sy_rgb15(d->trim, 5, d->finish), sy_rgb15(0x4A52, 5, 2), sy_rgb15(d->glow, 6, 0), sy_rgb15(d->light, 5, 0), sy_rgb15(0x2108, 5, 2), sy_rgb15(d->emcol, 5, 0) };
    for(int i = 0; i < m->nf && nt + 2 <= SY_MAXF * 2; i++){ const SyF *f = &m->f[i]; o->tri[nt * 3] = f->v[0]; o->tri[nt * 3 + 1] = f->v[1]; o->tri[nt * 3 + 2] = f->v[2]; o->tcol[nt++] = cl[f->col % 7];
        if(f->n == 4){ o->tri[nt * 3] = f->v[0]; o->tri[nt * 3 + 1] = f->v[2]; o->tri[nt * 3 + 2] = f->v[3]; o->tcol[nt++] = cl[f->col % 7]; } }
    o->m.v = o->v; o->m.nv = m->nv; o->m.tri = o->tri; o->m.nt = nt; o->m.tcol = o->tcol; o->scale = 1.f / (m->rad > 8 ? m->rad : 8); o->ok = 1;
}
static void pc_sy_refresh(void){
    if(pc_sy_slot == cur_slot) return; pc_sy_slot = cur_slot; pc_sy_on = 0; SyDesign d;
    if(pc_sav_path() && sy_slot_load(pc_sav_path(), cur_slot, &d) && d.ingame){ sy_game(&d, &pc_sy_gm); pc_sy_on = 1; pc_sy_des = d; pc_sy_foes = d.foes; pc_sy_mc_ok = 0; for(int i = 0; i < 12; i++) pc_sy_fm[i].ok = 0; }
}
static int pc_sy_stat(int k, int v){            // k: 0 armour, 1 top speed, 2 turn, 3 damp, 4 gun, 5 hold, 6 fuel, 7 push
    pc_sy_refresh(); if(!pc_sy_on) return v; SyGame *g = &pc_sy_gm; int r;
    switch(k){
    case 0: r = g->armour - hull_plates * 3; return r < 25 ? 25 : r;                       // the smelter's hull plates still help
    case 1: r = g->speed + sp_n[0] * 2; return r > 90 ? 90 : r;                            // engine tunes still help
    case 2: return g->turn; case 3: return g->damp;
    case 4: return g->gun + sp_n[1] * 2;                                                   // laser lenses still help
    case 5: return g->hold; case 6: return g->fuel;
    case 7: r = g->accel + sp_n[0] * 2; return r > 96 ? 96 : r; }
    return v;
}
static void pc_sy_slot_erase(int s){ if(pc_sav_path()) sy_slot_erase(pc_sav_path(), s); pc_sy_slot = -1; }
static void pc_sy_slot_copy(int a, int b){ if(pc_sav_path()) sy_slot_copy(pc_sav_path(), a, b); pc_sy_slot = -1; }
static void sy_to_look(const SyDesign *d){          // the game's own ship model (space chase view, GBA style) gets the nearest look
    int tl = 0, mw = 0, cnt[SY_NSHAPE] = {0}; for(int i = 0; i < d->nsec; i++){ tl += d->sec[i].len; if(d->sec[i].wid > mw) mw = d->sec[i].wid; cnt[d->sec[i].shape] += d->sec[i].len; }
    int sh = 0; for(int i = 1; i < SY_NSHAPE; i++) if(cnt[i] > cnt[sh]) sh = i;
    ship.hull = sh == SYSH_BOX ? (mw >= 9 ? HL_TWIN : HL_BOX) : sh == SYSH_WEDGE ? HL_WEDGE : sh == SYSH_FLAT ? (mw >= 8 ? HL_SAUCER : HL_MANTA) : (tl >= 18 && mw <= 3 ? HL_NEEDLE : d->nsec >= 5 ? HL_CAPSULE : HL_DART);
    ship.len = tl < 12 ? 0 : tl < 22 ? 1 : 2; ship.wid = mw < 4 ? 0 : mw < 8 ? 1 : 2; ship.nose = d->nose == SYNO_POINT ? 0 : d->nose == SYNO_FORK ? 2 : 1;
    ship.wings = d->wing; ship.wsize = d->wspan < 5 ? 0 : d->wspan < 10 ? 1 : 2; ship.wpos = d->nsec < 2 ? 1 : d->wsec * 3 / d->nsec; if(ship.wpos > 2) ship.wpos = 2; ship.dihed = d->wtilt; ship.fins = d->fins;
    ship.engines = d->neng - 1 > 3 ? 3 : d->neng - 1; ship.esize = d->esize > 2 ? 2 : d->esize;
    int ng = 0, side = 0, top = 0, nose = 0, big = 0; for(int i = 0; i < d->ngun; i++){ const SyGun *g = &d->gun[i]; if(!g->kind) continue; ng++; if(g->pos == SYGP_LEFT || g->pos == SYGP_RIGHT) side++; if(g->pos == SYGP_TOP) top++; if(g->pos == SYGP_NOSE) nose++; if(g->kind == SYGK_CANNON && g->size == 2) big++; }
    ship.cannon = !ng ? CN_NONE : ng >= 4 ? CN_QUAD : big ? CN_BIG : top ? CN_TURRET : side ? (d->sym ? CN_WING : CN_TWIN) : CN_NOSE;
    ship.cockpit = d->cockpit == SYCP_BRIDGE ? CK_DOME : d->cockpit;
    int mis = 0; for(int i = 0; i < d->ngun; i++) if(d->gun[i].kind == SYGK_MISSILE) mis = 1;
    ship.pods = d->tanks >= 3 ? 3 : mis ? 1 : d->cargo >= 3 ? 2 : 0;
    ship.decal = d->pattern == SYPT_STRIPE ? SD_STRIPE : d->pattern == SYPT_TIGER ? SD_TEETH : d->pattern == SYPT_CHECKS ? SD_PANELS : d->pattern == SYPT_EDGE ? SD_LINES : d->pattern == SYPT_SPLIT ? SD_CHEVRON : SD_NONE;
    ship.paint = d->paint; ship.trim = d->trim; ship.glow = d->glow;
    if(shpx.module != MD_COIL){ int a = 0; for(int i = 0; i < d->nsec; i++) a += d->sec[i].armour; shpx.module = a >= 2 * d->nsec ? MD_ARMOUR : d->cargo >= 4 ? MD_RACK : d->shield >= 3 ? MD_DISH : MD_NONE; }
    shpx.scheme = d->pattern == SYPT_TWOTONE ? SC_TWO : SC_CLASSIC; shpx.finish = d->finish; shpx.topper = d->bays ? TP_MASTS : TP_NONE;
    char nm[9]; int k = 0; for(; k < 8 && d->name[k]; k++) nm[k] = d->name[k]; nm[k] = 0; while(k > 0 && nm[k - 1] == ' ') nm[--k] = 0; if(nm[0]) scpy(ship.name, nm);
    ship_build_model();
}

// ---------------------------------------------------------------- colours
static u16 sy_shade(u16 c, int k, int fin);
static u16 sy_shade(u16 c, int k, int fin){                // k 0..7 dark to light
    int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31; int lo = fin == SYFI_MATTE ? 45 : fin == SYFI_METAL ? 25 : 35, hi = fin == SYFI_MATTE ? 105 : fin == SYFI_METAL ? 135 : 120;
    int f = lo + (hi - lo) * k / 7; r = r * f / 100; g = g * f / 100; b = b * f / 100;
    if(k == 7 && (fin == SYFI_GLOSS || fin == SYFI_METAL)){ r = (r + 31) / 2; g = (g + 31) / 2; b = (b + 31) / 2; }
    if(fin == SYFI_PEARL){ r = (r * 3 + 24) / 4; g = (g * 3 + 22) / 4; b = (b * 3 + 28) / 4; }
    if(r > 31) r = 31; if(g > 31) g = 31; if(b > 31) b = 31; return (u16)(r | (g << 5) | (b << 10));
}
u16 sy_shade_p(u16 c, int k, int fin){ return sy_shade(c, k, fin); }
static void sy_pal(void){
    const SyDesign *d = &sy_d;
    for(int k = 0; k < 8; k++){ BG_PAL[SYP_PAINT + k] = sy_shade(d->paint, k, d->finish); BG_PAL[SYP_TRIM + k] = sy_shade(d->trim, k, d->finish); BG_PAL[SYP_METAL + k] = sy_shade(rgb15(120, 124, 136), k, SYFI_METAL);
        BG_PAL[SYP_GLOW + k] = sy_shade(d->glow, 4 + k / 2, SYFI_GLOSS); BG_PAL[SYP_GLASS + k] = sy_shade(d->light, 2 + k * 5 / 7, SYFI_GLOSS); BG_PAL[SYP_GUN + k] = sy_shade(rgb15(70, 72, 82), k, SYFI_METAL);
        BG_PAL[SYP_EMB + k] = sy_shade(d->emcol, 3 + k * 4 / 7, SYFI_GLOSS); BG_PAL[SYP_HI + k] = sy_shade(rgb15(120, 230, 255), 2 + k * 5 / 7, SYFI_GLOSS); }
}

// ---------------------------------------------------------------- the live 3D preview
typedef struct { float z; short i; } SyOrd;
static int sy_ordcmp(const void *a, const void *b){ float x = ((const SyOrd*)a)->z, y = ((const SyOrd*)b)->z; return x < y ? 1 : x > y ? -1 : 0; }
static void sy_draw3d(volatile u16 *pg, int x0, int y0, int w, int h, int hilite){
    if(!sy_mesh_ok){ sy_mesh(&sy_d, &sy_m); sy_mesh_ok = 1; }
    static float X[SY_MAXV], Y[SY_MAXV], Z[SY_MAXV]; static SyOrd ord[SY_MAXF];
    float ya = sy_yaw * 6.2831853f / 1024.f, pa = sy_pitch * 6.2831853f / 1024.f, cy = (float)cos(ya), sy_ = (float)sin(ya), cp = (float)cos(pa), sp = (float)sin(pa);
    float sc = (h * 0.62f) / (float)(sy_m.rad > 8 ? sy_m.rad : 8) * sy_zoom / 100.f; float cx = x0 + w * 0.5f, ccy = y0 + h * 0.5f;
    for(int i = 0; i < sy_m.nv; i++){ float x = sy_m.v[i].x, y = sy_m.v[i].y, z = sy_m.v[i].z; float x1 = x * cy + z * sy_, z1 = -x * sy_ + z * cy; float y2 = y * cp - z1 * sp, z2 = y * sp + z1 * cp;
        float f = 1.f + z2 / (sy_m.rad * 7.f + 1.f) * -1.f; if(f < 0.5f) f = 0.5f; X[i] = cx + x1 * sc * f; Y[i] = ccy + y2 * sc * f; Z[i] = z2; }
    int n = 0; for(int i = 0; i < sy_m.nf; i++){ const SyF *f = &sy_m.f[i]; float z = 0; for(int k = 0; k < f->n; k++) z += Z[f->v[k]]; ord[n].z = z / f->n; ord[n].i = (short)i; n++; }
    qsort(ord, n, sizeof ord[0], sy_ordcmp);
    static const int base[7] = {SYP_PAINT, SYP_TRIM, SYP_METAL, SYP_GLOW, SYP_GLASS, SYP_GUN, SYP_EMB};
    for(int o = 0; o < n; o++){ const SyF *f = &sy_m.f[ord[o].i]; int px[4], py[4];
        for(int k = 0; k < f->n; k++){ px[k] = (int)X[f->v[k]]; py[k] = (int)Y[f->v[k]]; }
        // light: the face's normal in view space (unit), lit from the upper left front
        float ax = X[f->v[1]] - X[f->v[0]], ay = Y[f->v[1]] - Y[f->v[0]], az = Z[f->v[1]] - Z[f->v[0]], bx = X[f->v[2]] - X[f->v[0]], by = Y[f->v[2]] - Y[f->v[0]], bz = Z[f->v[2]] - Z[f->v[0]];
        float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx, L = (float)sqrt(nx * nx + ny * ny + nz * nz) + 1e-6f; float l = (-0.45f * nx - 0.65f * ny - 0.6f * nz) / L; if(l < 0) l = -l;
        int sh = (int)(l * 7.4f); if(sh > 7) sh = 7; if(f->col == 3) sh = 4 + (int)((sy_clock >> 2) & 3);
        int c = base[f->col % 7] + sh; if(hilite >= 0 && f->sec == hilite && f->col <= 2 && ((sy_clock >> 4) & 1)) c = SYP_HI + sh;
        polyfill(pg, px, py, f->n, c, x0 + 1, y0 + 1, x0 + w - 1, y0 + h - 1); }
    if(sy_d.shield){ int r = (int)(sy_m.rad * sc * 1.08f); for(int k = 0; k < 64; k += (6 - sy_d.shield)){ int a = (k * 16 + (int)sy_clock * 2) & 1023; float t = a * 6.2831853f / 1024.f; int x = (int)(cx + (float)cos(t) * r), y = (int)(ccy + (float)sin(t) * r * 0.55f);
        if(x > x0 && x < x0 + w - 1 && y > y0 && y < y0 + h - 1) pix(pg, x, y, SYP_GLASS + 6); } }
}
// the HULL plan: sections seen from above on a grid; click to pick, drag right/left to change length, up/down for width
#define SY_BX 121
#define SY_BY 84
#define SY_BW 116
#define SY_BH 44
static int sy_plan_u(void){ int L = 0, W = 1; for(int i = 0; i < sy_d.nsec; i++){ L += sy_d.sec[i].len; if(sy_d.sec[i].wid > W) W = sy_d.sec[i].wid; } int u = (SY_BW - 6) / (L ? L : 1); int v = (SY_BH - 6) / W; if(v < u) u = v; return u < 1 ? 1 : u > 8 ? 8 : u; }
static int sy_plan_hit(int mx, int my){ int u = sy_plan_u(), x = SY_BX + 3, cy = SY_BY + SY_BH / 2; for(int i = 0; i < sy_d.nsec; i++){ int w = sy_d.sec[i].len * u, hh = sy_d.sec[i].wid * u / 2 + 1; if(mx >= x && mx < x + w && my >= cy - hh - 3 && my <= cy + hh + 3) return i; x += w; } return -1; }
static void sy_plan(volatile u16 *pg){
    ui_well(pg, SY_BX, SY_BY, SY_BW, SY_BH); int u = sy_plan_u(), cy = SY_BY + SY_BH / 2;
    for(int x = SY_BX + 3; x < SY_BX + SY_BW - 2; x += u * (sy_c.snap ? 2 : 1)) rect(pg, x, SY_BY + 2, 1, SY_BH - 4, U_PANEL);
    for(int y = cy % u; y < SY_BY + SY_BH - 2; y += u) if(y > SY_BY + 1) rect(pg, SY_BX + 2, y, SY_BW - 4, 1, U_PANEL);
    int x = SY_BX + 3;
    for(int i = 0; i < sy_d.nsec; i++){ const SySec *s = &sy_d.sec[i]; int w = s->len * u, hh = s->wid * u / 2; if(hh < 1) hh = 1; int on = i == sy_c.sec;
        rect(pg, x, cy - hh, w, hh * 2, on ? SYP_HI + 4 : SYP_PAINT + 3 + s->armour / 2); frame1(pg, x, cy - hh, w, hh * 2, on ? C_WHITE : SYP_PAINT + 6);
        if(s->shape == SYSH_BOX) rect(pg, x + 1, cy - hh + 1, w - 2, 1, SYP_TRIM + 5); if(s->shape == SYSH_WEDGE) rect(pg, x + 1, cy, w - 2, 1, SYP_TRIM + 5);
        char b[4] = {(char)('1' + i), 0}; if(w >= 6) text(pg, x + w / 2 - 1, cy - 2, b, on ? C_GOLD : C_WHITE, 1);
        if(i == sy_d.wsec && sy_d.wing){ rect(pg, x + w / 4, cy - hh - 3, w / 2, 2, SYP_TRIM + 6); rect(pg, x + w / 4, cy + hh + 1, w / 2, 2, SYP_TRIM + 6); }
        x += w; }
}
// ---------------------------------------------------------------- the stats panel: 11 bars, mass, power and heat
static void sy_stats_panel(volatile u16 *pg, int x0, int y0, int compact){
    ui_well(pg, x0, y0, 118, compact ? 9 : 56); char b[24];
    if(compact){ snprintf(b, sizeof b, "SPD %d ARM %d FIRE %d", (int)sy_s.v[SYST_SPEED], (int)sy_s.v[SYST_ARMOUR], (int)sy_s.v[SYST_FIRE]); text(pg, x0 + 3, y0 + 2, b, C_WHITE, 1);
        snprintf(b, sizeof b, "T%d", (int)sy_s.v[SYST_THREAT]); rtext(pg, x0 + 116, y0 + 2, b, C_GOLD); return; }
    static const char *const ab[SY_NSTAT] = {"SPEED","TURN","ARMOR","SHLD","FIRE","RANGE","HOLD","FUEL","CREW","COST","THRT"};
    for(int i = 0; i < SY_NSTAT + 1; i++){ int col = i / 6, row = i % 6, x = x0 + 2 + col * 58, y = y0 + 2 + row * 8;
        if(i == SY_NSTAT){ snprintf(b, sizeof b, "MASS %d", (int)sy_s.mass); text(pg, x, y, b, U_SOFT, 1); continue; }
        int v = sy_s.v[i], o = sy_s0.v[i], better = (i == SYST_COST || i == SYST_CREW) ? v < o : v > o, worse = (i == SYST_COST || i == SYST_CREW) ? v > o : v < o;
        text(pg, x, y, ab[i], i == SYST_THREAT ? C_GOLD : U_SOFT, 1); ui_bar(pg, x + 22, y + 1, 18, 4, v, sy_stat_max[i], i == SYST_THREAT ? C_RED : C_CYAN);
        snprintf(b, sizeof b, "%d", v); rtext(pg, x + 56, y, b, better ? C_GREEN : worse ? C_RED : C_WHITE); }
    int y = y0 + 50; snprintf(b, sizeof b, "POWER %d/%d", (int)sy_s.power_use, (int)sy_s.power_gen); text(pg, x0 + 2, y, b, sy_s.warn & SYWN_POWER ? C_RED : C_GREEN, 1);
    snprintf(b, sizeof b, "HEAT %d/%d", (int)sy_s.heat, (int)sy_s.cooling); text(pg, x0 + 60, y, b, sy_s.warn & SYWN_HEAT ? C_RED : C_GREEN, 1);
}
// ---------------------------------------------------------------- the whole screen
static void sy_say(const char *s){ snprintf(sy_note, sizeof sy_note, "%s", s); sy_note_t = 150; }
static void sy_commit(void){ sy_fix(&sy_d); sy_cursor_fix(&sy_d, &sy_c); sy_hist_push(&sy_h, &sy_d); sy_stats(&sy_d, &sy_s); sy_mesh_ok = 0; sy_pal(); }
static int sy_colrow(int id){ return id == SYR_PAINTC || id == SYR_TRIMC || id == SYR_GLOWC || id == SYR_LIGHTC || id == SYR_EMCOL; }
static u16 *sy_colptr(int id){ return id == SYR_PAINTC ? &sy_d.paint : id == SYR_TRIMC ? &sy_d.trim : id == SYR_GLOWC ? &sy_d.glow : id == SYR_LIGHTC ? &sy_d.light : &sy_d.emcol; }
static void sy_bits(volatile u16 *pg, int x, int y, const u8 *b, int px, int c, int bg){ for(int r = 0; r < 8; r++) for(int k = 0; k < 8; k++) rect(pg, x + k * px, y + r * px, px, px, (b[r] >> (7 - k)) & 1 ? c : bg); }
static void sy_draw(volatile u16 *pg, int *rows, int nrows){
    ui_bg(pg, 0, 160);
    for(int i = 0; i < SY_NTAB; i++){ int x = i * 24, on = i == sy_tab; rect(pg, x, 0, 23, 11, on ? U_SEL : U_BG0); if(on) rect(pg, x, 10, 23, 1, U_ACCENT);
        text(pg, x + 12 - textlen(sy_tab_name[i]) * 2, 3, sy_tab_name[i], on ? C_GOLD : U_SOFT, 1); }
    ui_panel(pg, 1, 13, 116, 117); int top = sy_top[sy_tab], sel = sy_row[sy_tab];
    for(int i = 0; i < 12 && top + i < nrows; i++){ int id = rows[top + i], y = 17 + i * 9, on = top + i == sel; char v[40];
        if(on) ui_sel(pg, 3, y - 2, 112, 9, U_SEL);
        text(pg, 6, y, sy_row_label(id), on ? C_WHITE : sy_row_is_value(id) ? C_GREY : C_CYAN, 1);
        if(sy_colrow(id)){ BG_PAL[SYP_SW + i] = *sy_colptr(id); rect(pg, 92, y - 1, 20, 7, SYP_SW + i); frame1(pg, 92, y - 1, 20, 7, on ? C_WHITE : C_DGREY); }
        else if(id == SYR_EMBLEM || id == SYR_DECAL){ BG_PAL[SYP_SW + i] = id == SYR_EMBLEM ? sy_d.emcol : sy_d.trim; for(int r = 0; r < 8; r += 1) for(int k = 0; k < 8; k++) if((id == SYR_EMBLEM ? sy_d.emblem : sy_d.decal)[r] >> (7 - k) & 1) pix(pg, 104 + k, y - 2 + r, SYP_SW + i); }
        else { sy_row_value(id, &sy_d, &sy_c, v, sizeof v); v[14] = 0; if(v[0]){ char t[24]; if(sy_row_is_value(id) && on) snprintf(t, sizeof t, "<%s>", v); else snprintf(t, sizeof t, "%s", v); rtext(pg, 114, y, t, on ? C_GOLD : C_WHITE); } } }
    if(nrows > 12){ int bh = 104 * 12 / nrows; rect(pg, 115, 16 + 104 * top / nrows, 1, bh, U_ACCENT); }
    { char m[20]; snprintf(m, sizeof m, "%s", sy_c.advanced ? "ADVANCED" : "SIMPLE"); text(pg, 6, 123, m, sy_c.advanced ? C_GOLD : U_SOFT, 1); rtext(pg, 114, 123, sy_d.name, C_GOLD); }
    ui_well(pg, SY_PX, SY_PY, SY_PW, SY_PH);
    if(sy_tab == SYT_HULL){ sy_stats_panel(pg, 120, 74, 1); sy_plan(pg); }
    else if(sy_tab == SYT_BADGE){ ui_well(pg, 120, 74, 118, 56); text(pg, 124, 77, "EMBLEM", U_SOFT, 1); sy_bits(pg, 124, 85, sy_d.emblem, 5, SYP_EMB + 5, U_TROUGH); text(pg, 182, 77, "DECAL", U_SOFT, 1); sy_bits(pg, 182, 85, sy_d.decal, 5, SYP_TRIM + 5, U_TROUGH); }
    else sy_stats_panel(pg, 120, 74, 0);
    ui_panel(pg, 1, 131, 238, 18);
    { int id = rows[sel]; char ln[64];
      if(sy_note_t > 0) text(pg, 5, 134, sy_note, C_GOLD, 1); else { snprintf(ln, sizeof ln, "%s", sy_row_help(id)); ln[58] = 0; text(pg, 5, 134, ln, U_SOFT, 1); }
      if(sy_s.warn){ int nw = 0; char w[80]; while(sy_warn_text(sy_s.warn, nw, w, sizeof w)) nw++; sy_warn_text(sy_s.warn, (int)(sy_clock / 150) % (nw ? nw : 1), w, sizeof w); w[58] = 0; text(pg, 5, 141, w, C_RED, 1); }
      else { SyPart p; if(sy_row_part(id, &sy_d, &sy_c, &p)){ snprintf(ln, sizeof ln, "THIS PART: MASS %d  POWER %d  HEAT %d  COST %d  CREW %d", p.mass, p.power, p.heat, p.cost, p.crew); ln[58] = 0; text(pg, 5, 141, ln, C_GREEN, 1); }
             else text(pg, 5, 141, "ALL PARTS WORK TOGETHER. NO WARNINGS", C_GREEN, 1); } }
    ui_foot(pg, "A: PICK  SEL: DICE  START: UNDO/DONE", "L R: TAB  B: DONE");
}
static void sy_preview(volatile u16 *pg){
    volatile u16 *bp = (volatile u16*)(0x06000000 + ((REG_DISPCNT & 0x10) ? 0 : 0xA000));
    ui_well(bp, SY_PX, SY_PY, SY_PW, SY_PH);
    for(int k = 0; k < 12; k++){ int x = SY_PX + 2 + ((k * 37 + sy_clock / 6) % (SY_PW - 4)), y = SY_PY + 2 + (k * 53) % (SY_PH - 4); pix(bp, x, y, (k & 3) ? U_RAISE : U_SOFT); }
    sy_draw3d(bp, SY_PX, SY_PY, SY_PW, SY_PH, (sy_tab == SYT_HULL || sy_tab == SYT_GUARD) ? sy_c.sec : -1);
    { char b[24]; snprintf(b, sizeof b, "THREAT %d", (int)sy_s.v[SYST_THREAT]); text(bp, SY_PX + 3, SY_PY + 3, b, C_GOLD, 1); if(sy_s.warn) rtext(bp, SY_PX + SY_PW - 3, SY_PY + 3, "!", C_RED);
      text(bp, SY_PX + 3, SY_PY + SY_PH - 8, sy_spin ? "DRAG: TURN  WHEEL: ZOOM" : "SEL HELD: TURN", U_RAISE, 1); }
    vsync();
    for(int y = SY_PY; y < SY_PY + SY_PH; y++){ volatile u16 *s = bp + y * 120 + SY_PX / 2, *t = pg + y * 120 + SY_PX / 2; for(int x = 0; x < (SY_PW + 1) / 2; x++) t[x] = s[x]; }
}
// ---------------------------------------------------------------- small pop-up editors
static int sy_name_edit(volatile u16 *pg){
    static const char chs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.'"; char nm[SY_NAME + 1]; snprintf(nm, sizeof nm, "%s", sy_d.name); int cx = 0, cy = 0, dirty = 1, hh[4] = {0, 0, 0, 0}; wait_release();
    while(1){ int L = (int)strlen(nm);
        if(dirty){ dirty = 0; ui_bg(pg, 0, 160); ui_panel(pg, 10, 8, 220, 132); ui_head(pg, 16, 12, 208, "NAME YOUR SHIP", C_GOLD); ui_well(pg, 16, 24, 208, 14); text(pg, 22, 28, nm, C_WHITE, 1); if((sy_clock / 20) & 1) rect(pg, 22 + L * 4, 28, 3, 5, C_GOLD);
            for(int i = 0; i < 44; i++){ int x = 18 + (i % 11) * 19, y = 46 + (i / 11) * 16, on = i == cy * 11 + cx; if(on) ui_sel(pg, x - 1, y - 1, 19, 13, U_SEL);
                const char *tx; char t[2] = {0, 0}; if(i < 40){ t[0] = chs[i]; tx = t; if(chs[i] == ' ') tx = "SP"; } else tx = i == 40 ? "DEL" : i == 41 ? "OK" : i == 42 ? "DICE" : "ESC"; text(pg, x + 9 - textlen(tx) * 2, y + 3, tx, on ? C_GOLD : C_WHITE, 1); }
            ui_foot(pg, "A: TYPE  B: DELETE  SEL: RANDOM", "START: OK"); }
        vsync(); read_keys(); sy_clock++; if((sy_clock % 20) == 0) dirty = 1;
        if(pc_lw_rep(K_RIGHT, &hh[0])){ cx = (cx + 1) % 11; dirty = 1; } if(pc_lw_rep(K_LEFT, &hh[1])){ cx = (cx + 10) % 11; dirty = 1; }
        if(pc_lw_rep(K_DOWN, &hh[2])){ cy = (cy + 1) % 4; dirty = 1; } if(pc_lw_rep(K_UP, &hh[3])){ cy = (cy + 3) % 4; dirty = 1; }
        { static int mb_old; int mb = pc_mouse_b, click = (mb & 1) && !(mb_old & 1); mb_old = mb; if(click && pc_mouse_x >= 18 && pc_mouse_y >= 46){ int ix = (pc_mouse_x - 18) / 19, iy = (pc_mouse_y - 46) / 16; if(ix < 11 && iy < 4){ cx = ix; cy = iy; dirty = 1; keys |= K_A; keys_old &= ~K_A; } } }
        int i = cy * 11 + cx;
        if(HIT(K_A)){ dirty = 1; sfx(2); if(i < 40){ if(L < SY_NAME){ nm[L] = chs[i]; nm[L + 1] = 0; } } else if(i == 40){ if(L) nm[L - 1] = 0; } else if(i == 41){ snprintf(sy_d.name, sizeof sy_d.name, "%s", nm); wait_release(); return 1; }
            else if(i == 42){ SyDesign t = sy_d; sy_change(&t, &sy_c, SYR_NAME, 1); snprintf(nm, sizeof nm, "%s", t.name); } else { wait_release(); return 0; } }
        if(HIT(K_B)){ if(L){ nm[L - 1] = 0; dirty = 1; } else { wait_release(); return 0; } }
        if(HIT(K_SEL)){ SyDesign t = sy_d; sy_change(&t, &sy_c, SYR_NAME, 1); snprintf(nm, sizeof nm, "%s", t.name); dirty = 1; sfx(6); }
        if(HIT(K_START)){ snprintf(sy_d.name, sizeof sy_d.name, "%s", nm); wait_release(); return 1; }
    }
}
static int sy_pixel_edit(volatile u16 *pg, u8 *bits, const char *title, int col){
    u8 b[8]; memcpy(b, bits, 8); int cx = 3, cy = 3, dirty = 1, hh[4] = {0, 0, 0, 0}, paint = -1; wait_release();
    while(1){
        if(dirty){ dirty = 0; ui_bg(pg, 0, 160); ui_panel(pg, 10, 8, 220, 132); ui_head(pg, 16, 12, 208, title, C_GOLD);
            ui_well(pg, 20, 24, 100, 100); for(int r = 0; r < 8; r++) for(int k = 0; k < 8; k++){ int on = (b[r] >> (7 - k)) & 1; rect(pg, 22 + k * 12, 26 + r * 12, 11, 11, on ? col : U_TROUGH); }
            frame1(pg, 21 + cx * 12, 25 + cy * 12, 13, 13, C_GOLD);
            text(pg, 132, 30, "BIG", U_SOFT, 1); sy_bits(pg, 132, 40, b, 6, col, U_TROUGH); text(pg, 190, 30, "SMALL", U_SOFT, 1); sy_bits(pg, 196, 44, b, 1, col, U_TROUGH);
            text(pg, 132, 98, "A: PAINT A SQUARE", C_WHITE, 1); text(pg, 132, 106, "SEL: CLEAR  R: FLIP", C_WHITE, 1); text(pg, 132, 114, "MOUSE: CLICK, DRAG", C_WHITE, 1);
            ui_foot(pg, "A: PAINT  START: OK", "B: CANCEL"); }
        vsync(); read_keys(); sy_clock++;
        if(pc_lw_rep(K_RIGHT, &hh[0])){ cx = (cx + 1) & 7; dirty = 1; } if(pc_lw_rep(K_LEFT, &hh[1])){ cx = (cx + 7) & 7; dirty = 1; }
        if(pc_lw_rep(K_DOWN, &hh[2])){ cy = (cy + 1) & 7; dirty = 1; } if(pc_lw_rep(K_UP, &hh[3])){ cy = (cy + 7) & 7; dirty = 1; }
        if(HIT(K_A)){ b[cy] ^= (u8)(0x80 >> cx); dirty = 1; sfx(7); }
        { int mb = pc_mouse_b, mx = pc_mouse_x - 22, my = pc_mouse_y - 26; if((mb & 1) && mx >= 0 && my >= 0 && mx < 96 && my < 96){ cx = mx / 12; cy = my / 12; int on = (b[cy] >> (7 - cx)) & 1; if(paint < 0) paint = !on; if(on != paint){ b[cy] ^= (u8)(0x80 >> cx); } dirty = 1; } if(!(mb & 1)) paint = -1; }
        if(HIT(K_SEL)){ memset(b, 0, 8); dirty = 1; sfx(4); }
        if(HIT(K_R)){ for(int r = 0; r < 8; r++){ u8 v = b[r], o = 0; for(int k = 0; k < 8; k++) if(v >> k & 1) o |= (u8)(0x80 >> k); b[r] = o; } dirty = 1; }
        if(HIT(K_START)){ memcpy(bits, b, 8); wait_release(); return 1; }
        if(HIT(K_B)){ wait_release(); return 0; }
    }
}
static int sy_list(volatile u16 *pg, const char *title, char names[][SY_NAME + 1], int n){
    int sel = 0, dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; ui_bg(pg, 0, 160); ui_panel(pg, 10, 6, 220, 140); ui_head(pg, 16, 10, 208, title, C_GOLD); int top = sel > 12 ? sel - 12 : 0;
            for(int i = 0; i < 13 && top + i < n; i++){ int y = 22 + i * 9; if(top + i == sel) ui_sel(pg, 14, y - 1, 212, 9, U_SEL); text(pg, 20, y, names[top + i], top + i == sel ? C_GOLD : C_WHITE, 1); }
            ui_foot(pg, "A: OPEN", "B: BACK"); }
        vsync(); read_keys(); if(HIT(K_UP)){ sel = (sel + n - 1) % n; dirty = 1; } if(HIT(K_DOWN)){ sel = (sel + 1) % n; dirty = 1; }
        if(HIT(K_A)){ wait_release(); return sel; } if(HIT(K_B)){ wait_release(); return -1; }
    }
}
// A on a row. *leave: 1 done, 2 the old hangar
static int sy_open(volatile u16 *pg, int id, int *leave){
    const char *sv = pc_sav_path(); int ch = 0;
    if(sy_colrow(id)){ u16 *c = sy_colptr(id); u16 n = pick_colour(pg, sy_row_label(id), *c); if(n != *c){ *c = n; ch = 1; } return ch; }
    switch(id){
    case SYR_DICE: sy_random(&sy_d, &sy_rngs); sy_say("A NEW SHIP. START OR RIGHT CLICK TAKES IT BACK"); return 1;
    case SYR_UNDO: if(sy_hist_undo(&sy_h, &sy_d)) sy_say("UNDONE"); else sy_say("NOTHING TO UNDO"); sy_cursor_fix(&sy_d, &sy_c); sy_mesh_ok = 0; return 0;
    case SYR_REDO: if(sy_hist_redo(&sy_h, &sy_d)) sy_say("REDONE"); else sy_say("NOTHING TO REDO"); sy_cursor_fix(&sy_d, &sy_c); sy_mesh_ok = 0; return 0;
    case SYR_NAME: return sy_name_edit(pg);
    case SYR_EMBLEM: return sy_pixel_edit(pg, sy_d.emblem, "DRAW YOUR EMBLEM", SYP_EMB + 5);
    case SYR_DECAL: return sy_pixel_edit(pg, sy_d.decal, "DRAW THE WING DECAL", SYP_TRIM + 5);
    case SYR_SAVE: if(sv && sy_lib_save(sv, &sy_d)) sy_say("SAVED NEXT TO YOUR SAVE FILE"); else sy_say("COULD NOT SAVE"); return 0;
    case SYR_LOAD: { static char nm[SY_LIBMAX][SY_NAME + 1]; int n = sv ? sy_lib_list(sv, nm, SY_LIBMAX) : 0; if(!n){ sy_say("NO SAVED DESIGNS YET"); return 0; }
        int k = sy_list(pg, "SAVED DESIGNS", nm, n); if(k >= 0){ SyDesign t; if(sy_lib_load(sv, nm[k], &t)){ t.ingame = sy_d.ingame; sy_d = t; sy_say("DESIGN LOADED"); return 1; } sy_say("THAT FILE IS DAMAGED"); } return 0; }
    case SYR_CODE: { char code[400]; if(sy_code(&sy_d, code, sizeof code)){ if(sv) sy_code_write(sv, code); printf("shipyard: share code %s\n", code); sy_say("CODE WRITTEN TO THE .SHIPCODE FILE"); } return 0; }
    case SYR_IMPORT: { char code[400]; SyDesign t; if(sy_code_read(sv, code, sizeof code) && sy_uncode(code, &t)){ t.ingame = sy_d.ingame; sy_d = t; sy_say("CODE READ. THIS IS THAT SHIP"); return 1; } sy_say("NO GOOD CODE IN THE .SHIPCODE FILE"); return 0; }
    case SYR_CLASSIC: *leave = 2; return 0;
    case SYR_DONE: *leave = 1; return 0;
    case SYR_PRESET: sy_say("LEFT RIGHT: PICK A READY SHIP"); return 0;
    default: if(sy_row_is_value(id)) return sy_change(&sy_d, &sy_c, id, 1); return sy_action(&sy_d, &sy_c, id);
    }
}

// ---- the design DRAWN in space: your ship in the chase view, and allied / enemy ships built from the same parts (when FOE SHIPS is on)
static int pc_sy_player(int cx, int cy, int sc, int yaw_, int pitch_){
    pc_sy_refresh(); if(!pc_sy_on) return 0; if(!pc_sy_mc_ok){ sy_srmesh(&pc_sy_des, &pc_sy_mc); pc_sy_mc_ok = 1; }
    float rc = cosx(ship_roll >> 2) / 16384.0f, rs = sinx(ship_roll >> 2) / 16384.0f, cyw = cosx(yaw_ >> 2) / 16384.0f, syw = sinx(yaw_ >> 2) / 16384.0f, cp = cosx(pitch_ >> 2) / 16384.0f, sp = sinx(pitch_ >> 2) / 16384.0f;
    static float px[SY_MAXV], py[SY_MAXV], pz[SY_MAXV]; PcSyMesh *o = &pc_sy_mc; float k = (float)sh_rad * o->scale;
    for(int i = 0; i < o->m.nv; i++){ float x = o->v[i * 3] * k, y = o->v[i * 3 + 1] * k, z = o->v[i * 3 + 2] * k;
        float rx = x * cyw + z * syw, rz = -x * syw + z * cyw, ry = y * cp + rz * sp; rz = -y * sp + rz * cp;
        float d = 400 + rz; if(d < 60) d = 60; float f = sc * 25600.0f / d / 16384.0f, X = rx * f, Y = -ry * f; float X2 = X * rc - Y * rs, Y2 = X * rs + Y * rc; px[i] = X2 + cx; py[i] = Y2 + cy; pz[i] = rz; }
    static int ord[SY_MAXF * 2]; static float zz[SY_MAXF * 2]; int n = o->m.nt; for(int t = 0; t < n; t++){ ord[t] = t; zz[t] = (pz[o->tri[t * 3]] + pz[o->tri[t * 3 + 1]] + pz[o->tri[t * 3 + 2]]) / 3; }
    for(int a = 1; a < n; a++){ int v = ord[a]; float z = zz[v]; int b = a - 1; while(b >= 0 && zz[ord[b]] < z){ ord[b + 1] = ord[b]; b--; } ord[b + 1] = v; }
    for(int q = 0; q < n; q++){ int t = ord[q]; int a = o->tri[t * 3], b = o->tri[t * 3 + 1], c = o->tri[t * 3 + 2];
        float ux = o->v[b * 3] - o->v[a * 3], uy = o->v[b * 3 + 1] - o->v[a * 3 + 1], uz = o->v[b * 3 + 2] - o->v[a * 3 + 2], vx = o->v[c * 3] - o->v[a * 3], vy = o->v[c * 3 + 1] - o->v[a * 3 + 1], vz = o->v[c * 3 + 2] - o->v[a * 3 + 2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx, L = sqrtf(nx * nx + ny * ny + nz * nz) + 1e-6f; nx /= L; ny /= L; nz /= L;
        float mx = nx * cyw + nz * syw, mz0 = -nx * syw + nz * cyw, my = ny * cp + mz0 * sp, mz = -ny * sp + mz0 * cp; float l = (-mx * 0.35f + my * 0.7f - mz * 0.55f); if(l < 0) l = -l * 0.5f; if(l > 1) l = 1;
        uint32_t c0 = o->tcol[t]; int R = (int)((c0 >> 16 & 255) * (0.45f + 0.65f * l)), G = (int)((c0 >> 8 & 255) * (0.45f + 0.65f * l)), B = (int)((c0 & 255) * (0.45f + 0.65f * l)); if(R > 255) R = 255; if(G > 255) G = 255; if(B > 255) B = 255;
        float X[3] = { px[a], px[b], px[c] }, Y[3] = { py[a], py[b], py[c] }; sr_poly2d(X, Y, 3, 2.0f + zz[t] / 1000.0f, (uint32_t)(R << 16 | G << 8 | B)); }
    if(sh_flame) for(int e = 0; e < sh_en; e++){                                                                    // the game's flame (two crossed cones per engine, as ship.inc sh_flames) at window resolution
        float ex = sh_eng[e][0], ey = sh_eng[e][1], bz = sh_eng[e][2] + 2, R = sh_eng[e][3]; int len = (16 + (int)R * 4) * (3 + sh_flame) / 4; len = len * (10 + ((frames * 3 + e) & 3)) / 12;
        #define PCFP(ox,oy,oz,X_,Y_,Z_) { float x_ = (ox), y_ = (oy), z_ = (oz); float rx = x_ * cyw + z_ * syw, rz = -x_ * syw + z_ * cyw, ry = y_ * cp + rz * sp; rz = -y_ * sp + rz * cp; float d = 400 + rz; if(d < 60) d = 60; float f = sc * 25600.0f / d / 16384.0f, X = rx * f, Y = -ry * f; X_ = X * rc - Y * rs + cx; Y_ = X * rs + Y * rc + cy; Z_ = rz; }
        float bX,bY,bZ,tX,tY,tZ,aX,aY,aZ,cX,cY,cZ,dX,dY,dZ,fX,fY,fZ;
        PCFP(ex,ey,bz,bX,bY,bZ) PCFP(ex,ey,bz-len,tX,tY,tZ) PCFP(ex+R,ey,bz,aX,aY,aZ) PCFP(ex-R,ey,bz,cX,cY,cZ) PCFP(ex,ey+R,bz,dX,dY,dZ) PCFP(ex,ey-R,bz,fX,fY,fZ)
        #undef PCFP
        float dep = tZ >= bZ ? 3.5f : 1.4f; uint32_t oc = sy_rgb15(pc_sy_des.glow, 6 + ((frames + e) & 1), 0), ic = 0xFFF0D0;
        float mX = bX + (tX - bX) * 0.6f, mY = bY + (tY - bY) * 0.6f, X3[3], Y3[3];
        X3[0]=aX; Y3[0]=aY; X3[1]=cX; Y3[1]=cY; X3[2]=tX; Y3[2]=tY; sr_poly2d(X3,Y3,3,dep,oc);
        X3[0]=dX; Y3[0]=dY; X3[1]=fX; Y3[1]=fY; sr_poly2d(X3,Y3,3,dep,oc);
        X3[0]=bX+(aX-bX)/2; Y3[0]=bY+(aY-bY)/2; X3[1]=bX+(cX-bX)/2; Y3[1]=bY+(cY-bY)/2; X3[2]=mX; Y3[2]=mY; sr_poly2d(X3,Y3,3,dep-0.1f,ic);
        X3[0]=bX+(dX-bX)/2; Y3[0]=bY+(dY-bY)/2; X3[1]=bX+(fX-bX)/2; Y3[1]=bY+(fY-bY)/2; sr_poly2d(X3,Y3,3,dep-0.1f,ic); }
    if(sh_flame) for(int e = 0; e < sh_en; e++){ float ex = sh_eng[e][0], ey = sh_eng[e][1], ez = sh_eng[e][2] - 8; float rx = ex * cyw + ez * syw, rz = -ex * syw + ez * cyw, ry = ey * cp + rz * sp; rz = -ey * sp + rz * cp; float d = 400 + rz; if(d < 60) d = 60; float f = sc * 25600.0f / d / 16384.0f, X = rx * f, Y = -ry * f;
        float gx = X * rc - Y * rs + cx, gy = X * rs + Y * rc + cy, r = sh_eng[e][3] * f * 0.9f + 1; sr_glow2d(gx, gy, r * (0.8f + 0.25f * sh_flame), sy_rgb15(pc_sy_des.glow, 6, 0), 0.8f); sr_glow2d(gx, gy, r * 0.45f, 0xFFFFFF, 0.6f); }
    sr_commit(pc_sr_tag); return 1;
}
static int pc_sy_foe(int i, SrShip *s){      // 1 = drawn here as a mesh (an allied or enemy ship from the part grammar)
    pc_sy_refresh(); if(!pc_sy_on || !pc_sy_foes) return 0; int team = s->team ? 1 : 0, slot = team * 6 + (i % 6); PcSyMesh *o = &pc_sy_fm[slot];
    if(!o->ok){ SyDesign d; sy_foe(&d, (uint32_t)(i % 6) * 2654435761u + (uint32_t)cur_sys * 40503u, team, space_danger() > 9 ? 9 : space_danger() < 0 ? 0 : space_danger()); sy_srmesh(&d, o); }
    sr_mesh(&o->m, s->p, s->f, s->u, s->size * o->scale * 1.2f, s->col); return 1; }
// the test hook: BUGSTAR_SY_TEST=PRESET[,foes] BUGSTAR_SY_AT=frames: after that many frames in the game, give this slot that preset design and fly into space
static void pc_sy_autotest(void){
    static int done = 0; if(done || !in_game) return; const char *e = getenv("BUGSTAR_SY_TEST"); if(!e){ done = 1; return; }
    const char *at = getenv("BUGSTAR_SY_AT"); if(frames < (at ? atoi(at) : 0)) return; done = 1; int p = 0, f = 0; sscanf(e, "%d,%d", &p, &f);
    SyDesign d; sy_default(&d); sy_preset(&d, p); d.foes = f; if(pc_sav_path()) sy_slot_save(pc_sav_path(), cur_slot, &d); pc_sy_slot = -1; pc_sy_refresh(); if(pc_sy_on) sy_to_look(&d);
    if(scene != 2){ oam_hide_all(); oam_flush(); scene = 2; page_back = 1; space_enter(1); music(3); }
    if(getenv("BUGSTAR_SY_PIRATES")) spawn_pirates(atoi(getenv("BUGSTAR_SY_PIRATES")));
}
static int make_ship_g(volatile u16 *pg, int first);
static int pc_sy_screen(volatile u16 *pg, int first){
    int rows[64], nrows, leave = 0, dirty = 1, selp = 0, selused = 0; u16 dispcnt = REG_DISPCNT;
    for(int i = 0; i < 256; i++) sy_savepal[i] = BG_PAL[i];
    if(!sy_init || sy_c.sec < 0){ sy_default(&sy_d); sy_init = 1; }
    int had = 0; { SyDesign t; if(pc_sav_path() && sy_slot_load(pc_sav_path(), cur_slot, &t)){ sy_d = t; had = 1; } else if(!first){ sy_default(&sy_d); char nm[9]; scpy(nm, ship.name); snprintf(sy_d.name, sizeof sy_d.name, "%s", nm); sy_fix(&sy_d); } }
    if(getenv("PC_SHIP_PRESET")){ sy_preset(&sy_d, atoi(getenv("PC_SHIP_PRESET"))); }
    sy_d0 = sy_d; int start_hash = (int)sy_hash(&sy_d); sy_hist_init(&sy_h, &sy_d); sy_cursor_fix(&sy_d, &sy_c); sy_stats(&sy_d, &sy_s); sy_s0 = sy_s; sy_mesh_ok = 0;
    REG_DISPCNT = 4 | (1 << 10); oam_hide_all(); oam_flush(); sy_pal(); wait_release();
    while(!leave){
        nrows = sy_rows(sy_tab, &sy_d, &sy_c, rows); if(sy_row[sy_tab] >= nrows) sy_row[sy_tab] = nrows - 1; if(sy_row[sy_tab] < 0) sy_row[sy_tab] = 0;
        { int s = sy_row[sy_tab], *t = &sy_top[sy_tab]; if(s < *t) *t = s; if(s > *t + 11) *t = s - 11; if(*t < 0) *t = 0; }
        if(dirty){ dirty = 0; sy_pal(); sy_draw(pg, rows, nrows); }
        sy_preview(pg); read_keys(); sy_clock++; frames++; if(sy_spin && !sy_drag) sy_yaw = (sy_yaw + 2) & 1023; if(sy_note_t > 0 && --sy_note_t == 0) dirty = 1; if(sy_s.warn && sy_clock % 150 == 0) dirty = 1;
        int hr = pc_lw_rep(K_RIGHT, &sy_hold[0]), hl = pc_lw_rep(K_LEFT, &sy_hold[1]), hu = pc_lw_rep(K_UP, &sy_hold[2]), hd = pc_lw_rep(K_DOWN, &sy_hold[3]);
        int id = rows[sy_row[sy_tab]], changed = 0, mopen = 0, mundo = 0;
        // ---- the mouse
        { static int mb_old; int mb = pc_mouse_b, mx = pc_mouse_x, my = pc_mouse_y, click = (mb & 1) && !(mb_old & 1), rclick = (mb & 2) && !(mb_old & 2); mb_old = mb; int wh = pc_mouse_wheel; pc_mouse_wheel = 0;
          if(rclick) mundo = 1;
          if(click && mx >= 0){
              if(my < 12){ int t = mx / 24; if(t >= 0 && t < SY_NTAB && t != sy_tab){ sy_tab = t; dirty = 1; sfx(7); } }
              else if(mx < 117 && my >= 15 && my < 15 + 12 * 9){ int r2 = sy_top[sy_tab] + (my - 15) / 9; if(r2 < nrows){ if(r2 == sy_row[sy_tab]) mopen = 1; else { sy_row[sy_tab] = r2; dirty = 1; sfx(7); } } }
              else if(mx >= SY_PX && my >= SY_PY && my < SY_PY + SY_PH){ sy_drag = 1; sy_dx0 = mx; sy_dy0 = my; sy_dv0 = sy_yaw; sy_dv1 = sy_pitch; }
              else if(sy_tab == SYT_HULL && mx >= SY_BX && my >= SY_BY && my < SY_BY + SY_BH){ int s = sy_plan_hit(mx, my); if(s >= 0){ sy_c.sec = s; sy_drag = 2; sy_dx0 = mx; sy_dy0 = my; sy_dv0 = sy_d.sec[s].len; sy_dv1 = sy_d.sec[s].wid; dirty = 1; sfx(7); } } }
          if(!(mb & 1) && sy_drag){ if(sy_drag == 2) changed = 1; sy_drag = 0; }
          if(sy_drag == 1 && mx >= 0){ sy_yaw = (sy_dv0 + (mx - sy_dx0) * 8) & 1023; sy_pitch = sy_dv1 + (my - sy_dy0) * 6; if(sy_pitch > 250) sy_pitch = 250; if(sy_pitch < -250) sy_pitch = -250; }
          if(sy_drag == 2 && mx >= 0){ int u = sy_plan_u(), st = sy_c.snap ? 2 : 1; int dl = (mx - sy_dx0) / u, dw = -(my - sy_dy0) * 2 / u; dl = dl / st * st; dw = dw / st * st;
              int nl = sy_dv0 + dl, nw = sy_dv1 + dw; if(nl < 1) nl = 1; if(nl > SY_UNIT_MAX) nl = SY_UNIT_MAX; if(nw < 1) nw = 1; if(nw > SY_UNIT_MAX) nw = SY_UNIT_MAX;
              if(nl != sy_d.sec[sy_c.sec].len || nw != sy_d.sec[sy_c.sec].wid){ sy_d.sec[sy_c.sec].len = (u8)nl; sy_d.sec[sy_c.sec].wid = (u8)nw; sy_stats(&sy_d, &sy_s); sy_mesh_ok = 0; dirty = 1; } }
          if(wh && mx >= 0){ if(mx >= SY_PX && my < SY_PY + SY_PH){ sy_zoom += wh * 10; if(sy_zoom < 40) sy_zoom = 40; if(sy_zoom > 250) sy_zoom = 250; }
              else if(mx < 117){ int r2 = sy_row[sy_tab] - wh; if(r2 >= 0 && r2 < nrows){ sy_row[sy_tab] = r2; dirty = 1; } } } }
        // ---- SELECT: tap = surprise me, hold + arrows = turn the view
        { int sh = HELD(K_SEL); if(sh && !selp) selused = 0;
          if(sh){ if(HELD(K_LEFT)){ sy_yaw = (sy_yaw - 10) & 1023; selused = 1; } if(HELD(K_RIGHT)){ sy_yaw = (sy_yaw + 10) & 1023; selused = 1; }
                  if(HELD(K_UP)){ if(sy_pitch < 250) sy_pitch += 6; selused = 1; } if(HELD(K_DOWN)){ if(sy_pitch > -250) sy_pitch -= 6; selused = 1; } if(selused) sy_spin = 0; }
          if(!sh && selp && !selused){ sy_random(&sy_d, &sy_rngs); changed = 1; sfx(6); sy_say("SURPRISE! START OR RIGHT CLICK TAKES IT BACK"); }
          selp = sh; if(sh){ continue; } }
        if(HIT(K_L)){ sy_tab = (sy_tab + SY_NTAB - 1) % SY_NTAB; dirty = 1; sfx(7); } if(HIT(K_R)){ sy_tab = (sy_tab + 1) % SY_NTAB; dirty = 1; sfx(7); }
        if(hu){ sy_row[sy_tab] = (sy_row[sy_tab] + nrows - 1) % nrows; dirty = 1; sfx(7); } if(hd){ sy_row[sy_tab] = (sy_row[sy_tab] + 1) % nrows; dirty = 1; sfx(7); }
        if((hr || hl) && sy_row_is_value(id)){ int ok = sy_change(&sy_d, &sy_c, id, hr ? (hr == 2 ? 4 : 1) : (hl == 2 ? -4 : -1)); changed |= ok; dirty = 1; if(ok) sfx(7); sy_cursor_fix(&sy_d, &sy_c); if(id == SYR_MODE || id == SYR_SEC || id == SYR_GUN || id == SYR_SNAP) sy_mesh_ok = 0; }
        if(HIT(K_A) || mopen){ int lv = 0; changed |= sy_open(pg, id, &lv); if(lv) leave = lv; dirty = 1; sfx(2); sy_pal(); }
        if(HIT(K_START) && sy_h.at == 0) leave = 1;
        else if(HIT(K_START) || mundo){ if(sy_hist_undo(&sy_h, &sy_d)){ sy_say("UNDONE (SHIP TAB: REDO)"); sy_cursor_fix(&sy_d, &sy_c); sy_stats(&sy_d, &sy_s); sy_mesh_ok = 0; } else sy_say("NOTHING TO UNDO"); dirty = 1; sfx(4); }
        if(changed){ sy_commit(); dirty = 1; }
        if(HIT(K_B)) leave = 1;
    }
    int r = 1;
    if(leave == 2){ sy_d.ingame = 0; for(int i = 0; i < 256; i++) BG_PAL[i] = sy_savepal[i]; REG_DISPCNT = dispcnt; r = make_ship_g(pg, first); }
    int touched = had || (int)sy_hash(&sy_d) != start_hash || getenv("PC_SHIP_PRESET");
    if(touched && leave != 2 && sy_d.ingame) sy_to_look(&sy_d);
    sy_fix(&sy_d); if(touched && pc_sav_path()) sy_slot_save(pc_sav_path(), cur_slot, &sy_d); pc_sy_slot = -1;
    if(leave != 2){ for(int i = 0; i < 256; i++) BG_PAL[i] = sy_savepal[i]; REG_DISPCNT = dispcnt; }
    if(getenv("PC_SY_PRINT")){ int v[6]; ship_stats(v); printf("shipyard: %s in game %d: top speed %d turn %d damage taken %d laser %d hold %d fuel %d push %d\n", sy_d.name, sy_d.ingame, ship_top_speed(), ship_turn(), ship_armour(), ship_gun(), ship_hold_bonus(), ship_fuel_bonus(), ship_accel16()); (void)v; }
    wait_release(); return r;
}
