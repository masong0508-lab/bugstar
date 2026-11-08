// pc_sprgen_build.c: buildings, whole towns and signs. A MODULAR KIT: wall materials (wood, stone, adobe, brick, metal, glass, ice, reed, concrete) by climate and
// technology, window shapes, doors, roof types, and 16 architectural FORMS (terrace, canal, stilt, cave, treetop, cliff nest, fort, market, tents, spires, harbour,
// raft, hive, dome, caravan, stone circle) from the city kind, plus 16 CHARACTER decorations (lanterns, flags, ivy, bunting, gilding ...). Colours come from the
// people who built it (stock hue, way of life accent), so the same town kind looks different for different peoples. See pc_sprgen.h.
#include "pc_sprgen_i.h"
#include <stdio.h>

enum { M_WOOD, M_STONE, M_ADOBE, M_BRICK, M_METAL, M_GLASS, M_ICE, M_REED, M_CONC };
typedef struct { SgCv cv; SgBStyle st; SgRng r; SgRamp wall, wall2, roof, trim, wood, glass, stone, acc, water; int mat, night, skyl; uint32_t hue_c; } Bd;
#define CV (&b->cv)
static const uint8_t mat_tab[SGC_N][6] = { { M_WOOD, M_WOOD, M_BRICK, M_CONC, M_METAL, M_GLASS }, { M_ADOBE, M_ADOBE, M_ADOBE, M_ADOBE, M_CONC, M_GLASS }, { M_ICE, M_WOOD, M_STONE, M_BRICK, M_METAL, M_GLASS },
    { M_REED, M_WOOD, M_WOOD, M_BRICK, M_METAL, M_GLASS }, { M_STONE, M_STONE, M_STONE, M_CONC, M_METAL, M_METAL }, { M_REED, M_WOOD, M_STONE, M_BRICK, M_CONC, M_GLASS } };
static const char *const form_nm[16] = { "TERRACE", "CANAL", "STILT", "CAVE", "TREETOP", "CLIFF", "FORT", "MARKET", "TENTS", "SPIRES", "HARBOUR", "RAFTS", "HIVE", "DOME", "CARAVAN", "STONES" };

static uint32_t matcol(int m, float hue){
    switch(m){ case M_WOOD: return sg_hsv(0.07f + hue * 0.03f, 0.5f, 0.52f); case M_STONE: return sg_hsv(hue * 0.2f + 0.08f, 0.12f, 0.62f); case M_ADOBE: return sg_hsv(0.07f + hue * 0.03f, 0.38f, 0.80f); case M_BRICK: return sg_hsv(0.02f + hue * 0.03f, 0.58f, 0.60f);
        case M_METAL: return sg_hsv(0.58f + hue * 0.04f, 0.22f, 0.62f); case M_GLASS: return sg_hsv(0.52f + hue * 0.1f, 0.45f, 0.62f); case M_ICE: return sg_hsv(0.52f, 0.14f, 0.93f); case M_REED: return sg_hsv(0.13f + hue * 0.03f, 0.45f, 0.70f); default: return sg_hsv(0.1f + hue * 0.05f, 0.08f, 0.80f); }
}
static void bpal(Bd *b){
    const SgBStyle *st = &b->st; float hue = fmodf(st->people_stock * 0.381966f + st->people_way * 0.0625f, 1.0f), ah = fmodf(st->people_way * 0.1375f + st->people_stock * 0.17f, 1.0f);
    int m = mat_tab[st->climate % SGC_N][st->tech > 5 ? 5 : st->tech]; int f = st->form; if(f == 3 || f == 15 || f == 5) m = M_STONE; if(f == 12) m = M_ADOBE; if(f == 0 && m == M_WOOD) m = M_ADOBE; if(f == 13) m = M_GLASS; if(f == 2 || f == 11) m = (m == M_ADOBE) ? M_REED : (m >= M_METAL ? M_METAL : M_WOOD);
    b->mat = m; uint32_t w = sg_mixc(matcol(m, hue), sg_hsv(hue, 0.45f, 0.72f), 0.28f);
    float ch = 0; (void)ch; int c = st->chr;
    if(c == 4) w = sg_mixc(w, 0x888888, 0.5f); if(c == 6) w = sg_scale(w, 0.78f); if(c == 7) w = sg_mixc(w, 0xFFFFFF, 0.25f); if(c == 11) w = sg_mixc(sg_scale(w, 0.6f), 0x6040A0, 0.35f);
    sg_ramp(&b->wall, w); sg_ramp(&b->wall2, sg_mixc(w, sg_hsv(hue + 0.08f, 0.4f, 0.75f), 0.4f));
    uint32_t rf; static const uint32_t rcol[6] = { 0xA84A32, 0x4A5260, 0xC8A850, 0x4E7A3A, 0x8A8E98, 0xE0C060 }; int rk = (st->people_way + st->people_stock / 3) % 6; if(m == M_REED || (st->tech == 0 && m == M_WOOD)) rk = 2; if(m >= M_METAL && m != M_ICE) rk = 4; if(st->climate == SGC_COLD) rk = (rk & 1) ? 1 : 4; if(st->climate == SGC_VOLCANIC) rk = 1;
    rf = sg_mixc(rcol[rk], sg_hsv(ah, 0.5f, 0.55f), 0.25f); if(c == 3) rf = 0xE6BC40; if(c == 4) rf = sg_mixc(rf, 0x808080, 0.5f); if(c == 7) rf = sg_mixc(rf, 0xFFFFFF, 0.2f); if(c == 11) rf = 0x3A3050;
    sg_ramp(&b->roof, rf); sg_ramp(&b->trim, c == 3 ? 0xF0D060 : sg_mixc(0xE8E0D0, w, 0.25f)); sg_ramp(&b->wood, sg_hsv(0.07f, 0.55f, 0.36f)); sg_ramp(&b->stone, sg_hsv(0.1f, 0.1f, 0.58f));
    sg_ramp(&b->acc, sg_hsv(ah, 0.75f, 0.88f)); sg_ramp(&b->water, 0x3F86B8); b->hue_c = sg_hsv(ah, 0.75f, 0.88f);
    b->night = st->hour == 2; uint32_t gl = b->night || c == 11 ? (c == 11 ? 0xA0FF90 : 0xFFD878) : (m == M_GLASS ? 0x9AD8F0 : 0x5C86A8); sg_ramp(&b->glass, gl);
}
// ---- the kit
static void texrect(Bd *b, float x0, float y0, float x1, float y1, int mat, const SgRamp *rp){
    SgCv *cv = CV; cv_rect(cv, x0, y0, x1, y1, rp, 0.72f, 0.40f); uint32_t dk = rp->c[0]; float w = x1 - x0, h = y1 - y0; (void)w;
    switch(mat){
    case M_WOOD: for(float x = x0 + 2.4f; x < x1 - 0.5f; x += 2.4f) cv_flat(cv, x, y0, x, y1, 0.3f, dk, 0.55f); break;
    case M_STONE: case M_BRICK: { float rh = mat == M_BRICK ? 2.2f : 3.2f, rw = mat == M_BRICK ? 4.4f : 6.0f; int row = 0; for(float y = y0 + rh; y < y1 - 0.4f; y += rh, row++){ cv_flat(cv, x0, y, x1, y, 0.28f, dk, 0.55f); for(float x = x0 + (row & 1) * rw * 0.5f + rw; x < x1; x += rw) cv_flat(cv, x, y - rh, x, y, 0.28f, dk, 0.55f); } break; }
    case M_ADOBE: cv_pattern(cv, 3, rp->c[1], 1.4f, 0.55f, b->st.seed, x0, y0, x1, y1); break;
    case M_METAL: case M_CONC: for(float y = y0 + 6; y < y1 - 1; y += 6) cv_flat(cv, x0, y, x1, y, 0.3f, dk, 0.6f); for(float y = y0 + 6; y < y1 - 1; y += 6) for(float x = x0 + 1.5f; x < x1 - 1; x += 4.5f) cv_dot(cv, x, y - 2.5f, 0.28f, rp->c[4], 0.8f); break;
    case M_GLASS: for(float x = x0 + 4; x < x1; x += 4) cv_flat(cv, x, y0, x, y1, 0.3f, rp->c[1], 0.7f); for(float y = y0 + 5; y < y1; y += 5) cv_flat(cv, x0, y, x1, y, 0.3f, rp->c[1], 0.7f); cv_flat(cv, x0 + 1, y1 - 1, x0 + w * 0.5f, y0 + 1, 0.9f, 0xFFFFFF, 0.25f); break;
    case M_ICE: for(int i = 0; i < 5; i++){ float x = x0 + 2 + (i * 7) % (int)(w < 4 ? 4 : w - 2), y = y0 + 2 + (i * 11) % (int)(h < 4 ? 4 : h - 2); cv_flat(cv, x - 1.2f, y, x + 1.2f, y, 0.4f, 0xFFFFFF, 0.9f); cv_flat(cv, x, y - 1.2f, x, y + 1.2f, 0.4f, 0xFFFFFF, 0.9f); } cv_pattern(cv, 7, rp->c[3], 5.0f, 0.2f, b->st.seed, x0, y0, x1, y1); break;
    case M_REED: for(float y = y0 + 2; y < y1; y += 2) cv_flat(cv, x0, y, x1, y, 0.3f, dk, 0.45f); for(float x = x0 + 2; x < x1; x += 3) cv_flat(cv, x, y0, x, y1, 0.25f, rp->c[4], 0.45f); break;
    }
    cv_flat(cv, x0, y1 - 0.4f, x1, y1 - 0.4f, 0.8f, dk, 0.35f);
}
static void win(Bd *b, float x, float y, float w, float h, int shape, int lit){
    SgCv *cv = CV; SgRamp g = b->glass; if(lit) sg_ramp(&g, b->st.chr == 11 ? 0xA0FF90 : 0xFFD070); cv_rect(cv, x - 0.5f, y - 0.5f, x + w + 0.5f, y + h + 0.5f, &b->wood, 0.3f, 0.2f);
    switch(shape){
    case 1: cv_ell(cv, x + w / 2, y, w / 2 + 0.5f, w / 2 + 0.5f, 0, &b->wood, SH_FLAT); cv_rect(cv, x, y, x + w, y + h, &g, 0.9f, 0.5f); cv_ell(cv, x + w / 2, y, w / 2, w / 2, 0, &g, SH_VERT); break;
    case 2: cv_ell(cv, x + w / 2, y + h / 2, w / 2 + 0.6f, h / 2 + 0.6f, 0, &b->wood, SH_FLAT); cv_ell(cv, x + w / 2, y + h / 2, w / 2, h / 2, 0, &g, SH_VERT); break;
    case 3: cv_rect(cv, x + w * 0.3f, y, x + w * 0.7f, y + h, &g, 0.9f, 0.5f); break;
    case 5: cv_rect(cv, x, y, x + w, y + h, &g, 0.9f, 0.5f); cv_rect(cv, x - w * 0.45f, y, x, y + h, &b->acc, 0.6f, 0.4f); cv_rect(cv, x + w, y, x + w * 1.45f, y + h, &b->acc, 0.6f, 0.4f); break;
    default: cv_rect(cv, x, y, x + w, y + h, &g, 0.95f, 0.5f); if(w > 2.5f && shape != 4){ cv_flat(cv, x + w / 2, y, x + w / 2, y + h, 0.35f, b->wood.c[1], 0.9f); if(h > 3.2f) cv_flat(cv, x, y + h / 2, x + w, y + h / 2, 0.35f, b->wood.c[1], 0.9f); } }
    if(!lit && !b->night) cv_flat(cv, x + 0.4f, y + h - 0.6f, x + w * 0.6f, y + 0.5f, 0.5f, 0xFFFFFF, 0.28f);
}
static void door(Bd *b, float x, float y, float w, float h, int arched){
    SgCv *cv = CV; cv_rect(cv, x - 0.5f, y, x + w + 0.5f, y + h, &b->trim, 0.7f, 0.4f); if(arched){ cv_ell(cv, x + w / 2, y, w / 2 + 0.5f, w / 2 + 0.5f, 0, &b->trim, SH_FLAT); cv_ell(cv, x + w / 2, y, w / 2, w / 2, 0, &b->acc, SH_VERT); } cv_rect(cv, x, y, x + w, y + h, &b->acc, 0.7f, 0.35f); cv_dot(cv, x + w * 0.78f, y + h * 0.55f, 0.45f, 0xF0D060, 1); }
static void roof_gable(Bd *b, float x0, float x1, float yb, float h, float over){
    SgCv *cv = CV; float xy[6] = { x0 - over, yb, (x0 + x1) / 2, yb - h, x1 + over, yb }; cv_poly(cv, xy, 3, &b->roof, 0.75f, 0.35f);
    int rows = (int)(h / 2.2f); for(int i = 1; i <= rows; i++){ float t = i / (float)(rows + 1), yy = yb - h * t, hw = ((x1 - x0) / 2 + over) * (1 - t); cv_flat(cv, (x0 + x1) / 2 - hw, yy, (x0 + x1) / 2 + hw, yy, 0.3f, b->roof.c[0], 0.6f); }
    cv_flat(cv, x0 - over, yb, (x0 + x1) / 2, yb - h, 0.7f, b->roof.c[4], 0.5f);
}
static void roof_flat(Bd *b, float x0, float x1, float yb, int crenel){
    SgCv *cv = CV; cv_rect(cv, x0 - 0.8f, yb - 1.6f, x1 + 0.8f, yb, &b->trim, 0.85f, 0.4f);
    if(crenel){ for(float x = x0 - 0.6f; x < x1 - 1; x += 3.4f) cv_rect(cv, x, yb - 3.4f, x + 1.9f, yb - 1.5f, &b->wall, 0.8f, 0.45f); }
}
static void roof_dome(Bd *b, float cx, float yb, float r, int onion){
    SgCv *cv = CV; cv_ell(cv, cx, yb, r, r * 0.85f, 0, &b->roof, SH_SPHERE); cv_rect(cv, cx - r - 1, yb, cx + r + 1, yb + 1.5f, &b->wall, 0.5f, 0.3f);
    for(int i = -2; i <= 2; i++) cv_curve(cv, cx + i * r * 0.18f, yb, cx + i * r * 0.5f, yb - r * 0.5f, cx + i * r * 0.1f, yb - r * 0.85f, 0.28f, 0.2f, &b->roof, SH_FLAT);
    if(onion) { cv_tri(cv, cx - r * 0.3f, yb - r * 0.65f, cx + r * 0.3f, yb - r * 0.65f, cx, yb - r * 1.6f, &b->roof, 0.9f, 0.4f); cv_dot(cv, cx, yb - r * 1.65f, 0.7f, 0xF0D060, 1); }
}
static void roof_cone(Bd *b, float cx, float yb, float w, float h, int striped){
    SgCv *cv = CV; float xy[6] = { cx - w, yb, cx, yb - h, cx + w, yb }; cv_poly(cv, xy, 3, &b->roof, 0.8f, 0.3f);
    if(striped) for(int i = -2; i <= 2; i++) cv_flat(cv, cx + i * w * 0.38f, yb, cx + i * w * 0.08f, yb - h * 0.95f, 0.9f, b->acc.c[2], 0.55f);
}
static void roof_pagoda(Bd *b, float cx, float yb, float w, int tiers){
    SgCv *cv = CV; for(int i = 0; i < tiers; i++){ float ww = w * (1 - i * 0.22f), y = yb - i * 7; float xy[8] = { cx - ww - 3, y - 1, cx - ww, y - 4.5f, cx + ww, y - 4.5f, cx + ww + 3, y - 1 }; cv_poly(cv, xy, 4, &b->roof, 0.8f, 0.3f); cv_flat(cv, cx - ww - 3, y - 1, cx + ww + 3, y - 1, 0.7f, b->roof.c[0], 0.9f); if(i < tiers - 1) cv_rect(cv, cx - ww * 0.62f, y - 7, cx + ww * 0.62f, y - 4.4f, &b->wall, 0.7f, 0.35f); }
    cv_flat(cv, cx, yb - tiers * 7 + 2.2f, cx, yb - tiers * 7 - 4, 0.7f, b->trim.c[2], 1);
}
static void roof_thatch(Bd *b, float cx, float yb, float w, float h){
    SgCv *cv = CV; cv_ell(cv, cx, yb, w + 1.5f, h, 0, &b->roof, SH_SPHERE); cv_rect(cv, cx - w - 1.5f, yb, cx + w + 1.5f, yb + 2.5f, &b->roof, 0.5f, 0.3f);
    for(int i = -4; i <= 4; i++) cv_flat(cv, cx + i * w * 0.2f, yb + 1, cx + i * w * 0.14f, yb - h * 0.9f, 0.3f, b->roof.c[0], 0.5f);
}
static void flag(Bd *b, float x, float y, float h, float w, uint32_t col, int swallow){
    SgCv *cv = CV; cv_flat(cv, x, y, x, y - h, 0.55f, 0x60503C, 1); SgRamp r; sg_ramp(&r, col); float xy[8] = { x, y - h, x + w, y - h + 0.8f, swallow ? x + w * 0.7f : x + w, y - h + w * 0.28f, x + w, y - h + w * 0.55f }; if(swallow){ xy[4] = x + w; xy[5] = y - h + w * 0.55f; xy[6] = x + w * 0.7f; xy[7] = y - h + w * 0.3f; xy[2] = x + w; xy[3] = y - h + 0.2f; cv_poly(cv, xy, 4, &r, 0.85f, 0.5f); }
    else { float q[8] = { x, y - h, x + w, y - h + 0.6f, x + w, y - h + w * 0.6f, x, y - h + w * 0.55f }; cv_poly(cv, q, 4, &r, 0.85f, 0.45f); }
}
static void smoke(Bd *b, float x, float y){ SgRng *r = &b->r; for(int i = 0; i < 4; i++) cv_glow(CV, x + sinf(i * 1.4f) * 2 + i * 0.7f, y - i * 3.6f, 2.6f + i * 0.9f, 0xE8E8E8, 0.45f - i * 0.08f); (void)r; }
static void lantern(Bd *b, float x, float y){ SgCv *cv = CV; cv_flat(cv, x, y - 3, x, y, 0.3f, 0x302820, 1); cv_glow(cv, x, y + 1.6f, 4.5f, 0xFFC860, b->night ? 0.7f : 0.35f); cv_ell(cv, x, y + 1.6f, 1.0f, 1.4f, 0, &b->acc, SH_SPHERE); cv_dot(cv, x, y + 1.7f, 0.6f, 0xFFF0B0, 1); }
static void ground_band(Bd *b, float y, uint32_t col){ if(b->st.flags & 1) return; SgRamp g; sg_ramp(&g, col); cv_rect(CV, -2, y, 66, 66, &g, 0.7f, 0.35f); }
static void water_band(Bd *b, float y){ if(b->st.flags & 1) return; SgCv *cv = CV; cv_rect(cv, -2, y, 66, 66, &b->water, 0.75f, 0.3f); for(int i = 0; i < 5; i++){ float x = 4 + i * 13 + (i & 1) * 3; cv_flat(cv, x, y + 2.5f + (i & 1) * 2.5f, x + 6, y + 2.5f + (i & 1) * 2.5f, 0.5f, 0xD0F0FF, 0.5f); } }

// ---- the 16 forms. Each draws on the 64 x 64 design grid, ground at y = 58.
static void fm_box(Bd *b, float x0, float x1, float yb, int floors, int rk, int win_shape, int lit_pct){   // a plain block of floors with windows; rk = roof kind
    SgCv *cv = CV; SgRng *r = &b->r; float fh = 7.5f, ht = floors * fh; texrect(b, x0, yb - ht, x1, yb, b->mat, &b->wall);
    for(int f = 0; f < floors; f++){ float y = yb - (f + 1) * fh + 1.8f; int n = (int)((x1 - x0) / 6.5f); if(n < 1) n = 1; float sp = (x1 - x0) / n; for(int i = 0; i < n; i++){ if(f == 0 && i == n / 2){ door(b, x0 + sp * (i + 0.5f) - 1.6f, yb - 6.2f, 3.2f, 6.2f, win_shape == 1); continue; } int lit = (int)(sg_rf(r) * 100) < lit_pct; win(b, x0 + sp * (i + 0.5f) - 1.3f, y, 2.6f, 3.6f, win_shape, lit); if(b->st.chr == 15 && f > 0) cv_rect(cv, x0 + sp * (i + 0.5f) - 1.9f, y + 3.8f, x0 + sp * (i + 0.5f) + 1.9f, y + 4.8f, &b->wood, 0.6f, 0.3f); } if(f) cv_flat(cv, x0, yb - f * fh, x1, yb - f * fh, 0.4f, b->wall.c[0], 0.6f); }
    switch(rk){ case 0: roof_gable(b, x0, x1, yb - ht, (x1 - x0) * 0.5f + 1, 1.5f); break; case 1: roof_flat(b, x0, x1, yb - ht, b->st.form == 6); break; case 2: roof_dome(b, (x0 + x1) / 2, yb - ht, (x1 - x0) / 2, b->st.people_way & 1); break;
        case 3: roof_cone(b, (x0 + x1) / 2, yb - ht, (x1 - x0) / 2 + 1.5f, (x1 - x0) * 0.95f, 0); break; case 4: roof_pagoda(b, (x0 + x1) / 2, yb - ht, (x1 - x0) / 2, 2); break; default: roof_thatch(b, (x0 + x1) / 2, yb - ht, (x1 - x0) / 2, 6.0f); }
}
static int rkind(Bd *b){ int t = b->st.tech, m = b->mat; if(t >= 4) return 1; if(m == M_REED || t == 0) return 5; if(b->st.climate == SGC_ARID) return (b->st.people_way & 1) ? 2 : 1; if(b->st.climate == SGC_COLD) return 0; return (b->st.people_stock + b->st.people_way) % 5 == 0 ? 4 : (t >= 3 ? 1 : 0); }
static void form_terrace(Bd *b){ SgCv *cv = CV; int tiers = 3 + (b->st.seed & 1); float w = 54, yb = 58; ground_band(b, 57, 0x6A8A4A);
    for(int i = 0; i < tiers; i++){ float h = 11, x0 = 32 - w / 2, x1 = 32 + w / 2; texrect(b, x0, yb - h, x1, yb, b->mat == M_WOOD ? M_ADOBE : b->mat, &b->wall); roof_flat(b, x0, x1, yb - h, 0); SgRamp gr; sg_ramp(&gr, 0x5C9A48); cv_rect(cv, x0 + 1, yb - h - 3.2f, x1 - 1, yb - h - 1.5f, &gr, 0.8f, 0.4f);
        for(int k = 0; k < 3 + (tiers - i); k++){ float x = x0 + 3 + sg_rf(&b->r) * (w - 6); cv_ell(cv, x, yb - h - 3.6f, 1.6f, 1.3f, 0, &gr, SH_SPHERE); }
        int n = (int)(w / 7); for(int k = 0; k < n; k++) { if(i == tiers - 1 && k == n / 2) door(b, x0 + (k + 0.5f) * w / n - 1.5f, yb - 7, 3, 7, 1); else win(b, x0 + (k + 0.5f) * w / n - 1.2f, yb - 8.5f, 2.4f, 3.4f, 1, sg_rf(&b->r) < 0.3f); }
        yb -= h + 2; w -= 12; }
    cv_rect(cv, 52, 57 - tiers * 13, 55, 58, &b->stone, 0.6f, 0.3f); }
static void form_canal(Bd *b){ SgCv *cv = CV; water_band(b, 52); int n = 3; float x = 3;
    for(int i = 0; i < n; i++){ float w = sg_rr(&b->r, 14, 19), h = sg_rr(&b->r, 26, 42); texrect(b, x, 52 - h, x + w, 52, b->mat, i & 1 ? &b->wall2 : &b->wall); float yb = 52 - h; int fl = (int)(h / 8);
        for(int f = 0; f < fl; f++) for(int k = 0; k < 2; k++) win(b, x + 2.5f + k * (w - 7), 52 - (f + 1) * 8 + 2, 3.2f, 4.4f, 1, sg_rf(&b->r) < 0.3f);
        if(i == 1) { float xy[10] = { x - 1, yb, x + 1, yb - 3, x + w / 2, yb - 7, x + w - 1, yb - 3, x + w + 1, yb }; cv_poly(cv, xy, 5, &b->roof, 0.75f, 0.35f); } else roof_gable(b, x, x + w, yb, w * 0.55f, 1.2f);
        cv_ell(cv, x + w / 2, 50, 2.6f, 3.4f, 0, &b->wood, SH_VERT); cv_rect(cv, x + w / 2 - 2.6f, 50, x + w / 2 + 2.6f, 52, &b->water, 0.5f, 0.4f); x += w + 1.2f; }
    cv_ell(cv, 44, 55.5f, 9, 2.2f, 0, &b->wood, SH_SPHERE); cv_flat(cv, 44, 54, 44, 44, 0.5f, 0x504030, 1); flag(b, 44, 46, 5, 5, 0xE04030, 0); }
static void form_stilt(Bd *b){ SgCv *cv = CV; water_band(b, 50); float x0 = 14, x1 = 50, yp = 38;
    for(int i = 0; i < 4; i++){ float x = x0 + 2 + i * 10.5f; cv_cap(cv, x, yp, 1.3f, x + sg_rs(&b->r) * 0.5f, 60, 1.3f, &b->wood, SH_SPHERE); } cv_flat(cv, x0 + 3, yp + 2, x0 + 14, 56, 0.6f, b->wood.c[1], 0.9f); cv_flat(cv, x0 + 14, yp + 2, x0 + 3, 56, 0.6f, b->wood.c[1], 0.9f);
    cv_rect(cv, x0 - 2, yp - 1, x1 + 2, yp + 2, &b->wood, 0.7f, 0.3f); texrect(b, x0 + 2, yp - 13, x1 - 6, yp - 1, b->mat == M_ADOBE ? M_REED : b->mat, &b->wall); win(b, x0 + 6, yp - 10, 4, 4, 0, 1); door(b, x0 + 17, yp - 9, 4, 8, 0);
    if(b->mat == M_REED || b->st.tech == 0) roof_thatch(b, (x0 + x1 - 4) / 2, yp - 13, (x1 - x0 - 8) / 2 + 2, 9); else roof_gable(b, x0 + 2, x1 - 6, yp - 13, 10, 2);
    cv_flat(cv, x1 - 4, yp, x1 + 2, 56, 0.5f, b->wood.c[2], 1); for(int k = 0; k < 4; k++) cv_flat(cv, x1 - 4 + k * 1.5f, yp + 1 + k * 4.5f, x1 - 2.5f + k * 1.5f, yp + 1 + k * 4.5f, 0.4f, b->wood.c[3], 1); }
static void form_cave(Bd *b){ SgCv *cv = CV; float xy[16]; for(int i = 0; i < 8; i++){ float a = -SG_PI + i * SG_PI / 7, rr = 31 + sg_rs(&b->r) * 3.5f; xy[i * 2] = 32 + cosf(a) * rr; xy[i * 2 + 1] = 59 + sinf(a) * rr * 0.92f; }
    SgRamp rock; sg_ramp(&rock, sg_mixc(0x8A8076, b->wall.c[2], 0.25f)); cv_poly(cv, xy, 8, &rock, 0.85f, 0.3f); cv_pattern(cv, 2, rock.c[0], 5, 0.5f, b->st.seed, 0, 0, 64, 64); cv_pattern(cv, 1, rock.c[1], 3, 0.4f, b->st.seed + 5, 0, 0, 64, 64);
    for(int i = 0; i < 4; i++){ float x = 14 + i * 11 + sg_rs(&b->r) * 2, y = 54 - (i & 1) * 12 - (i == 1 || i == 2 ? 10 : 0); door(b, x, y - 6, 4, 6, 1); if(i != 2) win(b, x + 7, y - 14, 3, 3, 2, sg_rf(&b->r) < 0.5f); }
    for(int k = 0; k < 6; k++) cv_flat(cv, 30 + k * 1.6f, 58 - k * 2.2f, 36 + k * 1.6f, 58 - k * 2.2f, 0.6f, rock.c[4], 0.9f); lantern(b, 12, 40); }
static void form_treetop(Bd *b){ SgCv *cv = CV; SgRamp lf; sg_ramp(&lf, 0x3E7A3A); cv_cap(cv, 32, 60, 5, 31, 6, 3.6f, &b->wood, SH_SPHERE); cv_pattern(cv, 0, b->wood.c[0], 1.8f, 0.5f, b->st.seed, 24, 0, 40, 64); for(int i = 0; i < 6; i++) cv_ell(cv, 14 + i * 7.5f, 10 + (i & 1) * 5, 9, 7, 0, &lf, SH_SPHERE);
    for(int k = 0; k < 2; k++){ float y = 28 + k * 18, side = k ? -1.0f : 1.0f, x0 = 32 + side * 3, x1 = 32 + side * 22; cv_rect(cv, fminf(x0, x1), y, fmaxf(x0, x1), y + 2, &b->wood, 0.7f, 0.3f); texrect(b, fminf(x0, x1) + 3, y - 10, fmaxf(x0, x1) - 1, y, M_WOOD, &b->wall); float cx = (x0 + x1) / 2 + side * 1; roof_gable(b, fminf(x0, x1) + 3, fmaxf(x0, x1) - 1, y - 10, 7, 1.5f); win(b, cx - 1.5f, y - 7, 3, 3.5f, 0, 1); lantern(b, x1 - side * 2, y + 2);
        cv_curve(cv, x1, y + 1, x1 + side * 3, y + 9, x1 + side * 1, y + 16, 0.3f, 0.3f, &b->wood, SH_FLAT); }
    cv_flat(cv, 32 + 24, 29, 32 - 24, 47, 0.5f, b->wood.c[2], 1); }
static void form_cliff(Bd *b){ SgCv *cv = CV; SgRamp rock; sg_ramp(&rock, sg_mixc(0x8E8478, b->wall.c[2], 0.2f)); float xy[12] = { 40, 0, 64, 0, 64, 64, 36, 64, 44, 30, 36, 14 }; cv_poly(cv, xy, 6, &rock, 0.85f, 0.35f); cv_pattern(cv, 0, rock.c[0], 4, 0.4f, b->st.seed, 30, 0, 64, 64); cv_pattern(cv, 2, rock.c[1], 5, 0.4f, b->st.seed, 30, 0, 64, 64);
    for(int i = 0; i < 3; i++){ float y = 14 + i * 15, x = 14 + (i & 1) * 6; cv_rect(cv, x, y + 9, 42 - i * 2, y + 11, &b->wood, 0.7f, 0.3f); texrect(b, x + 2, y, 41 - i * 2, y + 9, M_WOOD, &b->wall); win(b, x + 5, y + 2, 3, 4, 0, sg_rf(&b->r) < 0.5f); door(b, 33 - i * 2, y + 2.5f, 3, 6.5f, 0); roof_gable(b, x + 2, 41 - i * 2, y, 5, 1.5f); for(int k = 0; k < 4; k++) cv_flat(cv, x + 3 + k * 4, y + 9, x + 3 + k * 4, y + 12, 0.35f, b->wood.c[1], 1); }
    cv_flat(cv, 20, 0, 20, 38, 0.3f, 0x504030, 1); cv_rrect(cv, 17.5f, 38, 22.5f, 43, 0.8f, &b->wood, SH_VERT); }
static void form_fort(Bd *b){ SgCv *cv = CV; texrect(b, 3, 30, 61, 58, M_STONE, &b->stone); roof_flat(b, 3, 61, 30, 1); for(int s = 0; s < 2; s++){ float cx = s ? 55 : 9; texrect(b, cx - 6, 18, cx + 6, 58, M_STONE, &b->stone); cv_rect(cv, cx - 7, 17, cx + 7, 19.5f, &b->trim, 0.8f, 0.4f); roof_cone(b, cx, 17, 8, 14, 1); win(b, cx - 1, 28, 2, 5, 3, 1); }
    texrect(b, 22, 14, 42, 58, M_STONE, &b->stone); roof_flat(b, 22, 42, 14, 1); cv_rect(cv, 24, 6, 40, 14, &b->roof, 0.7f, 0.4f); roof_gable(b, 24, 40, 6, 7, 1.5f); cv_rect(cv, 25, 40, 39, 58, &b->wood, 0.5f, 0.3f); cv_ell(cv, 32, 40, 7, 7, 0, &b->wood, SH_VERT); cv_rect(cv, 25, 40, 39, 58, &b->wood, 0.5f, 0.25f);
    for(float x = 26.5f; x < 39; x += 3) cv_flat(cv, x, 34, x, 58, 0.35f, b->wood.c[0], 0.8f); cv_flat(cv, 25, 49, 39, 49, 0.6f, 0x302820, 0.9f); win(b, 30, 22, 4, 5, 3, 1); flag(b, 32, 6, 9, 8, b->hue_c, 1); cv_dot(cv, 32, 30, 1.2f, 0xF0D060, 1); }
static void form_market(Bd *b){ SgCv *cv = CV; ground_band(b, 56, 0xA89868); static const uint32_t cl[6] = { 0xD84A3A, 0x3A78D0, 0xE0B030, 0x3AA860, 0xB04AB0, 0xE07830 };
    for(int i = 0; i < 3; i++){ float x = 3 + i * 20, y = 58, w = 18; cv_cap(cv, x + 1, y, 0.8f, x + 1, y - 26, 0.8f, &b->wood, SH_FLAT); cv_cap(cv, x + w - 1, y, 0.8f, x + w - 1, y - 26, 0.8f, &b->wood, SH_FLAT); SgRamp a; sg_ramp(&a, cl[(b->st.seed + i * 2 + b->st.people_way) % 6]);
        float xy[8] = { x - 1, y - 26, x + w + 1, y - 26, x + w + 3, y - 18, x - 3, y - 18 }; cv_poly(cv, xy, 4, &a, 0.8f, 0.4f); for(int k = 0; k < 4; k++) cv_flat(cv, x - 1 + k * 5.5f, y - 26, x - 3 + k * 5.5f + 2, y - 18, 1.2f, 0xF4F0E8, 0.6f); for(int k = 0; k < 6; k++) cv_ell(cv, x - 3 + k * 4.4f + 1.4f, y - 17.4f, 1.5f, 1.2f, 0, &a, SH_FLAT);
        cv_rect(cv, x + 1, y - 10, x + w - 1, y - 6, &b->wood, 0.7f, 0.3f); for(int k = 0; k < 5; k++){ SgRamp g; sg_ramp(&g, cl[(k + i) % 6]); cv_ell(cv, x + 3 + k * 3, y - 11.5f, 1.4f, 1.4f, 0, &g, SH_SPHERE); } cv_rect(cv, x + 2, y - 6, x + 6, y, &b->wood, 0.6f, 0.3f); cv_rect(cv, x + w - 7, y - 4, x + w - 2, y, &b->wood, 0.6f, 0.3f); }
    flag(b, 32, 31, 8, 7, b->hue_c, 1); }
static void form_tents(Bd *b){ SgCv *cv = CV; ground_band(b, 56, 0xC8B080); int n = 2 + (b->st.seed & 1); for(int i = n - 1; i >= 0; i--){ float cx = n == 2 ? 20 + i * 26 : 12 + i * 20, w = 12 + (i == 1 ? 3 : 0), h = 26 + sg_rs(&b->r) * 3; roof_cone(b, cx, 58, w, h, 1);
        float xy[6] = { cx - 3.2f, 58, cx, 58 - h * 0.45f, cx + 3.2f, 58 }; SgRamp dk; sg_ramp(&dk, 0x20180E); cv_poly(cv, xy, 3, &dk, 0.6f, 0.3f); cv_flat(cv, cx - 3.4f, 58, cx, 58 - h * 0.48f, 0.6f, b->roof.c[4], 0.9f); cv_flat(cv, cx, 58 - h, cx, 58 - h - 5, 0.5f, 0x504030, 1); flag(b, cx, 58 - h - 3, 3, 5, b->hue_c, 1); }
    cv_ell(cv, 32, 57.5f, 4, 1.4f, 0, &b->wood, SH_SPHERE); cv_glow(cv, 32, 54, 5, 0xFFA040, 0.7f); cv_tri(cv, 30.5f, 57, 33.5f, 57, 32, 51, &b->acc, 0.9f, 0.5f); }
static void form_spires(Bd *b){ SgCv *cv = CV; int n = 3 + (b->st.seed & 1); static const float hs[5] = { 48, 36, 54, 42, 30 }; float xs[5] = { 11, 25, 39, 52, 6 }; for(int i = n - 1; i >= 0; i--){ float x = xs[i], h = hs[(i + b->st.seed) % 5], w = 4.5f + (i & 1) * 1.5f;
        float xy[8] = { x - w, 58, x - w * 0.55f, 58 - h, x + w * 0.55f, 58 - h, x + w, 58 }; cv_poly(cv, xy, 4, i & 1 ? &b->wall2 : &b->wall, 0.8f, 0.35f); cv_tri(cv, x - w * 0.6f, 58 - h, x + w * 0.6f, 58 - h, x, 58 - h - 9, &b->roof, 0.9f, 0.5f); cv_flat(cv, x, 58 - h - 9, x, 58 - h - 14, 0.4f, 0xC0C0C0, 1); cv_dot(cv, x, 58 - h - 14, 0.7f, 0xFF4040, 1);
        for(float y = 54; y > 58 - h + 3; y -= 4.2f){ int lit = sg_rf(&b->r) < 0.55f; cv_rect(cv, x - 1, y - 2, x + 1, y, lit ? &(SgRamp){ { 0x806020, 0xC09030, 0xFFD070, 0xFFE8A0, 0xFFF4D0 } } : &b->glass, 0.9f, 0.5f); } }
    for(int i = 0; i < n - 1; i++){ float y = 58 - hs[(i + b->st.seed) % 5] * 0.55f; cv_cap(cv, xs[i] + 2, y, 0.9f, xs[i + 1] - 2, y + 1.5f, 0.9f, &b->glass, SH_SPHERE); } }
static void form_harbour(Bd *b){ SgCv *cv = CV; water_band(b, 49); cv_rect(cv, 2, 45, 62, 49, &b->wood, 0.75f, 0.3f); for(int i = 0; i < 6; i++) cv_flat(cv, 4 + i * 10, 45, 4 + i * 10, 49, 0.4f, b->wood.c[0], 0.9f); texrect(b, 4, 24, 36, 45, b->mat, &b->wall); roof_gable(b, 4, 36, 24, 9, 1.5f); door(b, 16, 33, 8, 12, 0); win(b, 7, 28, 3, 3.5f, 0, 0); win(b, 28, 28, 3, 3.5f, 0, 1);
    cv_cap(cv, 50, 45, 1.6f, 50, 18, 1.2f, &b->wood, SH_SPHERE); cv_cap(cv, 50, 20, 1.0f, 38, 24, 0.8f, &b->wood, SH_SPHERE); cv_flat(cv, 39, 24, 39, 34, 0.3f, 0x302820, 1); cv_rect(cv, 37, 34, 41, 38, &b->acc, 0.7f, 0.4f); for(int i = 0; i < 3; i++) cv_rrect(cv, 43 + i * 5, 39, 47 + i * 5, 45, 1.2f, &b->wood, SH_HORZ); cv_ell(cv, 56, 51, 5, 1.8f, 0, &b->wood, SH_SPHERE); flag(b, 56, 50, 7, 5, b->hue_c, 0); }
static void form_raft(Bd *b){ SgCv *cv = CV; water_band(b, 46); for(int i = 0; i < 8; i++) cv_cap(cv, 6 + i * 7, 46, 2.0f, 6 + i * 7, 46 + 0.01f, 2.0f, &b->wood, SH_SPHERE); cv_rect(cv, 4, 42, 60, 47, &b->wood, 0.75f, 0.3f); for(int i = 0; i < 10; i++) cv_flat(cv, 4 + i * 6, 42, 4 + i * 6, 47, 0.4f, b->wood.c[0], 0.9f);
    texrect(b, 10, 28, 34, 42, b->mat == M_ADOBE ? M_REED : b->mat, &b->wall); roof_thatch(b, 22, 28, 14, 6); door(b, 15, 34, 4, 8, 0); win(b, 24, 32, 4, 4, 0, 1);
    cv_cap(cv, 46, 42, 0.9f, 46, 10, 0.7f, &b->wood, SH_SPHERE); float xy[6] = { 46.5f, 12, 46.5f, 38, 60, 38 }; SgRamp sl; sg_ramp(&sl, 0xE8E0C8); cv_poly(cv, xy, 3, &sl, 0.9f, 0.5f); cv_flat(cv, 46.5f, 22, 56, 22, 0.4f, b->acc.c[2], 0.8f); flag(b, 46, 12, 4, 5, b->hue_c, 1); }
static void form_hive(Bd *b){ SgCv *cv = CV; ground_band(b, 57, 0x9A8A60); int rows = 4; for(int r = 0; r < rows; r++){ int n = rows - r + 1; float cw = 12, y = 58 - r * 11.5f; for(int i = 0; i < n; i++){ float x = 32 + (i - (n - 1) / 2.0f) * (cw + 1.5f); cv_ell(cv, x, y - 5.8f, cw / 2 + 0.6f, 6.4f, 0, (i + r) & 1 ? &b->wall2 : &b->wall, SH_SPHERE); cv_rect(cv, x - cw / 2 - 0.6f, y - 5.8f, x + cw / 2 + 0.6f, y, (i + r) & 1 ? &b->wall2 : &b->wall, 0.6f, 0.35f);
            SgRamp dk; sg_ramp(&dk, 0x181008); cv_rect(cv, x - 1.7f, y - 4.2f, x + 1.7f, y, &dk, 0.4f, 0.3f); cv_ell(cv, x, y - 4.2f, 1.7f, 1.7f, 0, &dk, SH_FLAT); if(sg_rf(&b->r) < 0.4f) cv_rect(cv, x - 1.0f, y - 3.0f, x + 1.0f, y, &b->glass, 0.9f, 0.6f); cv_dot(cv, x, y - 8.3f, 0.7f, b->wall.c[0], 0.9f); cv_flat(cv, x - cw / 2 + 0.4f, y - 0.5f, x + cw / 2 - 0.4f, y - 0.5f, 0.3f, b->wall.c[0], 0.5f); } }
    cv_pattern(cv, 5, b->wall.c[0], 2.4f, 0.28f, b->st.seed, 0, 0, 64, 64); }
static void form_dome(Bd *b){ SgCv *cv = CV; ground_band(b, 57, 0x7A9A6A); SgRamp gl; sg_ramp(&gl, 0xA8E0F0); for(int i = 0; i < 3; i++){ float x = 18 + i * 14, h = 14 + (i == 1 ? 8 : 0) + sg_rr(&b->r, 0, 4); texrect(b, x - 5, 57 - h, x + 5, 57, M_CONC, &b->wall); for(float y = 55; y > 57 - h + 2; y -= 4) win(b, x - 3, y - 2, 6, 2.2f, 4, sg_rf(&b->r) < 0.5f); }
    cv->alpha = 0.34f; cv_ell(cv, 32, 57, 29, 33, 0, &gl, SH_SPHERE); cv->alpha = 0.8f; for(int i = -3; i <= 3; i++) cv_curve(cv, 32 + i * 9.3f, 57, 32 + i * 5, 40, 32 + i * 0.9f, 24, 0.35f, 0.3f, &gl, SH_FLAT); for(int k = 1; k < 4; k++) cv_flat(cv, 32 - 29 * (1 - k * 0.24f), 57 - 33 * k * 0.28f, 32 + 29 * (1 - k * 0.24f), 57 - 33 * k * 0.28f, 0.3f, gl.c[4], 0.8f);
    cv->alpha = 1; }
static void form_caravan(Bd *b){ SgCv *cv = CV; ground_band(b, 57, 0xC4AA78); cv_rect(cv, 8, 40, 56, 47, &b->wood, 0.75f, 0.3f); roof_dome(b, 32, 40, 18, 0); cv_ell(cv, 32, 40, 18, 15, 0, &b->roof, SH_SPHERE); for(int i = -3; i <= 3; i++) cv_flat(cv, 32 + i * 5, 40, 32 + i * 3, 27, 0.8f, b->acc.c[2], 0.45f); door(b, 29, 33, 6, 8, 1);
    for(int s = 0; s < 2; s++){ float cx = s ? 46 : 18; cv_ell(cv, cx, 51, 7.5f, 7.5f, 0, &b->wood, SH_SPHERE); cv_ell(cv, cx, 51, 5.2f, 5.2f, 0, &b->wall, SH_VERT); for(int k = 0; k < 4; k++){ float a = k * SG_PI / 4; cv_flat(cv, cx + cosf(a) * 6.8f, 51 + sinf(a) * 6.8f, cx - cosf(a) * 6.8f, 51 - sinf(a) * 6.8f, 0.6f, b->wood.c[1], 1); } cv_dot(cv, cx, 51, 1.1f, 0xF0D060, 1); }
    cv_flat(cv, 56, 44, 63, 49, 0.9f, b->wood.c[2], 1); lantern(b, 10, 43); flag(b, 32, 25, 6, 6, b->hue_c, 1); }
static void form_stones(Bd *b){ SgCv *cv = CV; ground_band(b, 56, 0x7AA05A); SgRamp g; sg_ramp(&g, 0x8C8A84); for(int i = 0; i < 3; i++){ float x = 10 + i * 17, h = 24 + (i == 1 ? 4 : 0) + sg_rr(&b->r, -3, 3); float xy[10] = { x - 4.5f, 58, x - 3.6f, 58 - h * 0.7f, x - 2, 58 - h, x + 3, 58 - h + 1.5f, x + 4.6f, 58 }; cv_poly(cv, xy, 5, &g, 0.85f, 0.3f); cv_pattern(cv, 2, g.c[0], 3, 0.5f, b->st.seed + i, x - 6, 0, x + 6, 60); cv_pattern(cv, 2, 0x5A8A3A, 3, 0.7f, b->st.seed + 9 + i, x - 6, 58 - h * 0.35f, x + 6, 60); cv_flat(cv, x - 1.6f, 58 - h * 0.6f, x + 1.2f, 58 - h * 0.45f, 0.5f, g.c[0], 0.8f); }
    float xy[8] = { 6, 33, 26, 31, 27.5f, 35.5f, 5, 37.5f }; cv_poly(cv, xy, 4, &g, 0.9f, 0.3f); roof_thatch(b, 50, 46, 9, 7); texrect(b, 41, 46, 59, 57, M_REED, &b->wall); door(b, 46, 48, 4, 9, 1); roof_thatch(b, 50, 46, 10, 7); }
typedef void (*FormFn)(Bd *);
static const FormFn forms_fn[16] = { form_terrace, form_canal, form_stilt, form_cave, form_treetop, form_cliff, form_fort, form_market, form_tents, form_spires, form_harbour, form_raft, form_hive, form_dome, form_caravan, form_stones };

static void decorate(Bd *b){
    SgCv *cv = CV; SgRng *r = &b->r; int c = b->st.chr; (void)r;
    switch(c){
    case 0: for(int i = 0; i < 3; i++){ float x = 12 + i * 20 + sg_rs(r) * 4, y = 6 + sg_rf(r) * 10; cv_dot(cv, x, y, 1.0f, 0xFFFFFF, 0.9f); cv_flat(cv, x + 0.9f, y, x + 0.9f, y - 5, 0.5f, 0xFFFFFF, 0.9f); cv_flat(cv, x + 0.9f, y - 5, x + 3, y - 4, 0.6f, 0xFFFFFF, 0.9f); } break;
    case 1: lantern(b, 8, 18); lantern(b, 54, 18); lantern(b, 31, 11); break;
    case 2: flag(b, 56, 12, 9, 8, 0xE0E0E0, 1); flag(b, 8, 14, 7, 7, b->hue_c, 1); for(int i = 0; i < 3; i++) cv_flat(cv, 14 + i * 14, 8 + i * 3, 22 + i * 14, 8 + i * 3, 0.5f, 0xFFFFFF, 0.55f); break;
    case 3: for(int i = 0; i < 5; i++) cv_glow(cv, 8 + i * 11, 12 + (i & 1) * 6, 2.4f, 0xFFE070, 0.45f); break;
    case 5: smoke(b, 52, 14); for(int i = 0; i < 3; i++) cv_dot(cv, 14 + i * 18, 56, 0.9f, 0x302820, 0.8f); break;
    case 6: cv_pattern(cv, 2, 0x4F7A3A, 3.5f, 0.6f, b->st.seed, 0, 40, 64, 64); for(int i = 0; i < 4; i++){ float x = 10 + sg_rf(r) * 44, y = 30 + sg_rf(r) * 26; cv_flat(cv, x, y, x + sg_rs(r) * 3, y + 4, 0.35f, 0x181410, 0.8f); cv_flat(cv, x + sg_rs(r) * 3, y + 4, x + sg_rs(r) * 4, y + 7, 0.3f, 0x181410, 0.7f); } break;
    case 7: for(int i = 0; i < 3; i++) cv_flat(cv, 6 + i * 26, 12, 6 + i * 26, 58, 0.5f, 0xC8A870, 0.5f); cv_flat(cv, 4, 22, 60, 22, 0.5f, 0xC8A870, 0.45f); cv_flat(cv, 4, 40, 60, 40, 0.5f, 0xC8A870, 0.45f); break;
    case 8: flag(b, 8, 30, 16, 14, b->hue_c, 1); cv_dot(cv, 12, 20, 1.5f, 0xFFFFFF, 0.9f); break;
    case 9: smoke(b, 48, 18); smoke(b, 18, 24); cv_ell(cv, 54, 8, 2.6f, 1.7f, 0, &b->wood, SH_SPHERE); cv_pattern(cv, 6, 0xFFF4C0, 0.8f, 0.9f, b->st.seed, 52, 6, 56, 10); break;
    case 10: { float y0 = 14; for(int i = 0; i < 10; i++){ float x0 = 2 + i * 6.2f, x1 = x0 + 6.2f, ya = y0 + sinf(i * 0.6f) * 0, yb = y0 + 3 + (i & 1); SgRamp g; sg_ramp(&g, i % 3 == 0 ? 0xE04040 : i % 3 == 1 ? 0xF0D040 : 0x40A0E0); cv_flat(cv, x0, y0 + (i & 1) * 0.5f, x1, y0 + ((i + 1) & 1) * 0.5f, 0.35f, 0x504030, 0.9f); cv_tri(cv, x0 + 0.6f, y0 + 0.2f, x1 - 0.6f, y0 + 0.2f, (x0 + x1) / 2, yb + 1.6f, &g, 0.9f, 0.5f); (void)ya; } break; }
    case 11: cv_glow(cv, 32, 40, 24, 0x80FF90, 0.12f); for(int i = 0; i < 3; i++){ float x = 12 + i * 20, y = 8 + (i & 1) * 6; cv_ell(cv, x, y, 2.4f, 1.0f, 0.3f, &b->wood, SH_FLAT); cv_tri(cv, x - 2, y - 0.5f, x - 5, y - 2.4f, x - 3.5f, y + 0.8f, &b->wood, 0.3f, 0.3f); cv_tri(cv, x + 2, y - 0.5f, x + 5, y - 2.4f, x + 3.5f, y + 0.8f, &b->wood, 0.3f, 0.3f); } break;
    case 12: { float x = 52, y = 10; cv_ell(cv, x, y, 3.4f, 3.4f, 0, &b->trim, SH_SPHERE); cv_dot(cv, x, y, 1.2f, b->wood.c[1], 1); for(int i = 0; i < 8; i++){ float a = i * SG_PI / 4; cv_flat(cv, x + cosf(a) * 3, y + sinf(a) * 3, x + cosf(a) * 4.6f, y + sinf(a) * 4.6f, 1.2f, b->trim.c[2], 1); } break; }
    case 13: for(int i = 0; i < 5; i++){ float x = 8 + sg_rf(r) * 48, y = 28 + sg_rf(r) * 28; cv_rect(cv, x, y, x + 5, y + 3.2f, &b->wood, 0.6f, 0.3f); cv_flat(cv, x, y + 1.6f, x + 5, y + 1.6f, 0.4f, b->wood.c[0], 0.8f); cv_dot(cv, x + 1, y + 0.8f, 0.3f, 0xB0B0B0, 1); } break;
    case 14: cv_pattern(cv, 2, 0x3E7A3A, 3.0f, 0.75f, b->st.seed + 3, 0, 20, 30, 64); cv_pattern(cv, 3, 0x70B050, 1.2f, 0.4f, b->st.seed, 0, 20, 30, 64); break;
    case 15: for(int i = 0; i < 8; i++){ float x = 6 + sg_rf(r) * 52, y = 40 + sg_rf(r) * 17; SgRamp f; sg_ramp(&f, i & 1 ? 0xF080B0 : 0xF0E060); cv_dot(cv, x, y, 1.0f, f.c[2], 1); cv_dot(cv, x, y, 0.4f, 0xFFF0A0, 1); cv_flat(cv, x, y + 1, x, y + 3, 0.3f, 0x3E7A3A, 1); } break;
    default: break; }
    if(b->st.tech >= 4 && b->st.form != 2 && b->st.form != 3){ cv_flat(cv, 58, 14, 58, 6, 0.4f, 0x909090, 1); cv_dot(cv, 58, 6, 0.6f, 0xFF3030, 1); }
    if(b->st.tech >= 5 && b->night) cv_glow(cv, 32, 30, 28, sg_hsv(fmodf(b->st.seed * 0.137f, 1), 0.8f, 1), 0.18f);
}
void sg_bstyle(SgBStyle *st, int city_id, int people_id, int climate, int tech, uint32_t seed){
    memset(st, 0, sizeof *st); st->form = (uint8_t)((city_id & 255) % 16); st->chr = (uint8_t)((city_id & 255) / 16); st->people_stock = (uint8_t)((people_id & 511) % 32); st->people_way = (uint8_t)((people_id & 511) / 32);
    st->climate = (uint8_t)(climate % SGC_N); st->tech = (uint8_t)(tech > 5 ? 5 : tech < 0 ? 0 : tech); st->floors = (uint8_t)(1 + (seed >> 3) % 3); st->hour = (uint8_t)((seed >> 9) % 3 == 2 ? 2 : (seed >> 9) % 3); st->seed = seed;
}
static void bd_init(Bd *b, SgImg *im, const SgBStyle *st, float design){ memset(b, 0, sizeof *b); b->st = *st; b->r = sg_rng(st->seed + 1, st->form * 977u + st->chr * 31u + st->people_stock * 7u); cv_init(CV, im, design); CV->post = 6; bpal(b); }
void sg_building(SgImg *im, const SgBStyle *st){
    Bd *b = (Bd *)calloc(1, sizeof *b); if(!b) return; bd_init(b, im, st, 64); SgCv *cv = CV;
    if(!(st->flags & 1)){ cv_shadow(cv, 32, 59.5f, 28, 2.6f, 0.30f); }
    forms_fn[st->form % 16](b); decorate(b);
    if(!(st->flags & 1)) cv_outline(cv, sg_scale(b->wall.c[0], 0.5f), 0.8f);
    free(b);
}
// ---- the sign: the town's name on a hanging board
static void sign_at(Bd *b, float cx, float ytop, const char *text, float maxw){
    SgCv *cv = CV; char t[24]; snprintf(t, sizeof t, "%s", text && text[0] ? text : "TOWN"); float px = 1.0f, w = cv_textw(t, px);
    while(w > maxw && px > 0.25f){ px -= 0.05f; w = cv_textw(t, px); } while(w < maxw * 0.55f && px < 1.9f){ px += 0.05f; w = cv_textw(t, px); }
    float bw = w + 6, bh = 5 * px + 5, x0 = cx - bw / 2, y0 = ytop; cv_flat(cv, x0 + 3, y0, x0 + 3, y0 - 6, 0.6f, 0x40382C, 1); cv_flat(cv, x0 + bw - 3, y0, x0 + bw - 3, y0 - 6, 0.6f, 0x40382C, 1); cv_flat(cv, x0 + 1.5f, y0 - 6, x0 + bw - 1.5f, y0 - 6, 0.9f, 0x40382C, 1);
    cv_rrect(cv, x0, y0, x0 + bw, y0 + bh, 1.4f, &b->wood, SH_VERT); cv_rrect(cv, x0 + 0.8f, y0 + 0.8f, x0 + bw - 0.8f, y0 + bh - 0.8f, 1.0f, &b->acc, SH_VERT); cv_flat(cv, x0 + 1.5f, y0 + bh - 0.2f, x0 + bw - 1.5f, y0 + bh - 0.2f, 0.4f, b->acc.c[0], 0.8f);
    cv_text(cv, cx - w / 2 + 0.25f * px, y0 + 2.5f + 0.3f * px, t, px, sg_scale(b->acc.c[0], 0.45f)); cv_text(cv, cx - w / 2, y0 + 2.5f, t, px, 0xFFF8E0);
}
void sg_sign(SgImg *im, const SgBStyle *st, const char *text){
    Bd *b = (Bd *)calloc(1, sizeof *b); if(!b) return; bd_init(b, im, st, 64); sign_at(b, 32, 24, text, 54); cv_outline(CV, 0x20180E, 0.8f); free(b);
}
// ---- the town: sky, hills, a row of its buildings, the ground, a sign
void sg_skyline(SgImg *im, const SgBStyle *st0, const char *name){
    Bd *b = (Bd *)calloc(1, sizeof *b); if(!b) return; SgBStyle st = *st0; st.flags |= 1; bd_init(b, im, &st, 128); SgCv *cv = CV; int hr = st.hour; int W = im->w, H = im->h;
    uint32_t top = hr == 0 ? 0x4F90E0 : hr == 1 ? 0x5A4A98 : 0x0A0E30, bot = hr == 0 ? 0xC8E4F8 : hr == 1 ? 0xF8A060 : 0x2A3868;
    if(st.climate == SGC_VOLCANIC){ top = hr == 2 ? 0x1A0808 : 0x6A3A2A; bot = hr == 2 ? 0x5A1A10 : 0xE8A050; } else if(st.climate == SGC_COLD && hr == 0) bot = 0xE8F0FF;
    for(int y = 0; y < H; y++){ uint32_t c = sg_mixc(top, bot, (float)y / (H * 0.9f)); for(int x = 0; x < W; x++) im->px[(size_t)y * W + x] = 0xFF000000u | c; }
    SgRng *r = &b->r; if(hr == 2) for(int i = 0; i < 40; i++) cv_dot(cv, sg_rf(r) * 128, sg_rf(r) * 36, 0.35f, 0xFFFFFF, 0.4f + sg_rf(r) * 0.6f);
    { float sx = 16 + sg_rf(r) * 96, sy = hr == 1 ? 38 : 12 + sg_rf(r) * 8; uint32_t sc = hr == 2 ? 0xE8E8F8 : hr == 1 ? 0xFFB070 : 0xFFF0B0; cv_glow(cv, sx, sy, 14, sc, 0.45f); cv_dot(cv, sx, sy, hr == 2 ? 3.2f : 4.2f, sc, 1); if(hr == 2) cv_dot(cv, sx + 1.4f, sy - 0.8f, 2.7f, top, 0.8f); }
    for(int layer = 0; layer < 2; layer++){ uint32_t hc = sg_mixc(sg_mixc(top, bot, 0.55f), layer ? 0x2E5A38 : 0x6A7C98, layer ? 0.55f : 0.5f); if(st.climate == SGC_ARID) hc = sg_mixc(hc, 0xC8A060, 0.5f); if(st.climate == SGC_COLD) hc = sg_mixc(hc, 0xE8F0FF, 0.6f); if(st.climate == SGC_VOLCANIC) hc = sg_mixc(hc, 0x201816, 0.6f);
        float xy[64]; int n = 20; for(int i = 0; i < n; i++){ float x = i * 128.0f / (n - 3) - 6; xy[i * 2] = x; xy[i * 2 + 1] = 46 - layer * 3 - sg_fbm(x * 0.07f, layer * 9.0f, st.seed, 3) * (layer ? 14 : 22); } xy[n * 2] = 136; xy[n * 2 + 1] = 70; xy[n * 2 + 2] = -8; xy[n * 2 + 3] = 70;
        SgRamp hr_; sg_ramp(&hr_, hc); cv_poly(cv, xy, n + 2, &hr_, 0.75f, 0.45f); }
    SgRamp gnd; sg_ramp(&gnd, st.climate == SGC_ARID ? 0xC8A468 : st.climate == SGC_COLD ? 0xE8EEF4 : st.climate == SGC_VOLCANIC ? 0x383030 : st.climate == SGC_SHORE ? 0xD8C890 : 0x5C8C48); cv_rect(cv, 0, 54, 128, 64, &gnd, 0.75f, 0.4f);
    int n = 4 + (st.form == 6 || st.form == 9 || st.form == 12 ? 1 : 0); if(st.form == 3 || st.form == 5 || st.form == 14 || st.form == 15 || st.form == 13) n = 3; float cw = 128.0f / n;
    for(int pass = 0; pass < 2; pass++) for(int i = 0; i < n; i++){ float far = (i * 5 + st.seed) % 3 == 0 ? 1.0f : 0.0f; if((int)far != pass) continue; SgBStyle bs = st; bs.seed = st.seed * 31u + i * 977u; bs.chr = (uint8_t)((st.chr + (i % 3 == 2 ? 0 : 0)) & 15); float k = (far ? 0.50f : 0.70f) + sg_rr(r, 0, 0.12f);
        Bd *c = (Bd *)calloc(1, sizeof *c); if(!c) continue; bd_init(c, im, &bs, 128); cv_view(&c->cv, cw * (i + 0.5f) - 32 * k + sg_rs(r) * 3, (far ? 49.0f : 57.0f) - 58 * k, k); c->night = b->night; cv_snap(&c->cv); if((i & 1) && st.form != 2 && st.form != 3 && st.form != 4 && st.form != 5 && st.form != 8 && st.form != 9 && st.form != 11 && st.form != 13){ bs.floors = (uint8_t)(2 + (bs.seed >> 5) % 3); fm_box(c, 14, 14 + 16 + (bs.seed % 8), 58, bs.floors, rkind(c), (bs.seed >> 3) % 2, 40); } else forms_fn[bs.form % 16](c); decorate(c); cv_unsnap(&c->cv); free(c); }
    cv_view(cv, 0, 0, 1); if(name && name[0]) sign_at(b, 34, 10, name, 56);
    free(b);
}
void sg_city_label(int city_id, int people_id, char *out, int n){
    int f = (city_id & 255) % 16, c = (city_id & 255) / 16; static const char *const chn[16] = { "SINGING", "LANTERN-LIT", "WINDY", "GOLDEN", "QUIET", "BUSY", "ANCIENT", "NEWBORN", "PROUD", "HUNGRY", "FESTIVE", "HAUNTED", "CLEVER", "ROUGH", "SECRET", "FLOWERING" };
    snprintf(out, n, "%s %s #%d", chn[c], form_nm[f], people_id & 511);
}
