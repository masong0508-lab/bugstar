// pc_sprgen_critx.c: the creature engine: the 16 colourings, the habitat ground, the 16 gifts, 8 ranks, 16 mutations, and sg_creature itself.
#include "pc_sprgen_crit.h"
#include <stdlib.h>
#include <stdio.h>

static const struct { uint32_t base, acc; uint8_t pat; uint32_t pc; } colt[16] = {
 {0x5E8F3E, 0xA8C858, 0, 0},        {0xC0442A, 0xF09030, 3, 0x601810}, {0x5A74A0, 0xA8C8E8, 0, 0},        {0xE4DCC8, 0xA89878, 0, 0},
 {0x3C3844, 0x7A5A98, 0, 0},        {0xE0A830, 0x8A5A18, 1, 0x7A4810}, {0xE890A8, 0xFFE0E8, 0, 0},        {0x7A4A8E, 0xD090E0, 0, 0},
 {0xC8A870, 0x7A5A38, 3, 0x7A5A38}, {0x3A9A9A, 0x1E5A66, 0, 0x14464E}, {0x8C9096, 0x5A5E66, 2, 0x50545C}, {0xC4CCD6, 0x7C8AA0, 0, 0},
 {0xB8683A, 0x6A3A1E, 1, 0x4AB098}, {0xD8E4F0, 0xB090D8, 2, 0xE8A0D0}, {0x1E2652, 0x5A6AD0, 6, 0xFFF0A0}, {0xF08A38, 0xE8508A, 4, 0xFFC860} };
static const char *const colour_pat_name[8] = { "STRIPES", "SPOTS", "DAPPLES", "SPECKLE", "BANDS", "SCALES", "STARS", "CHECKS" };

static void ramps(Cr *c, int col){
    uint32_t base = colt[col].base, acc = colt[col].acc; float h, s, v; sg_tohsv(base, &h, &s, &v); float dh = ((int)(sg_h2((uint32_t)c->plan, 17) % 9) - 4) * 0.006f; h += dh; if(h < 0) h += 1; if(h >= 1) h -= 1; base = sg_hsv(h, s, v);
    if(c->rank == 4) base = sg_mixc(base, 0xB4B4B8, 0.3f);
    sg_ramp(&c->b, base); sg_ramp(&c->bl, sg_mixc(base, 0xFFF4E0, 0.42f)); sg_ramp(&c->ac, acc); sg_ramp(&c->dk, sg_scale(base, 0.55f)); sg_ramp(&c->bn, 0xEADFC4);
    if(col == 11){ for(int i = 0; i < 5; i++){ int d = i - 2; c->b.c[i] = sg_mixc(c->b.c[i], d < 0 ? 0x303848 : 0xFFFFFF, 0.18f * (d < 0 ? -d : d)); } }
    c->glow = col == 1 || col == 15 ? 0xFFA040 : col == 2 || col == 14 || col == 13 ? 0x70C8FF : col == 7 ? 0xE080FF : col == 0 || col == 9 ? 0x90FF90 : 0xFFE070;
    c->eyec = 0xFFFFFF;
}
static void habitat(Cr *c){
    SgCv *cv = &c->cv; SgRamp g, w, st; SgRng r = sg_rng((uint64_t)c->plan * 31 + 5, c->hab + 7);
    switch(c->hab){
    case 0: sg_ramp(&w, 0x3E6A5E); cv_ell(cv, 32, CG + 1, 27, 4.5f, 0, &w, SH_SPHERE); cv_ell(cv, 28, CG + 0.6f, 13, 1.6f, 0, &w, SH_FLATLIT); cv_flat(cv, 12, CG + 1, 22, CG + 0.4f, 0.3f, 0xA8D8C0, 0.6f);
        for(int i = 0; i < 4; i++){ float x = i < 2 ? 8 + i * 4 : 50 + (i - 2) * 4; for(int j = 0; j < 3; j++) cv_curve(cv, x + j * 1.2f, CG + 1, x + j * 1.2f + (j - 1) * 1.5f, CG - 8, x + j * 1.8f + (j - 1) * 2.4f, CG - 13 - (i & 1) * 3 - j, 0.6f, 0.2f, &c->dk, SH_FLAT); cv_ell(cv, x + 1.2f, CG - 9, 1.0f, 3.0f, 0, &c->bn, SH_FLAT); cv_ell(cv, x + 1.2f, CG - 9, 0.9f, 2.8f, 0, &st, SH_FLAT); }
        cv_ell(cv, 41, CG + 2, 2.6f, 1.2f, 0, &c->ac, SH_FLATLIT); break;
    case 1: sg_ramp(&g, 0x6A8A44); sg_ramp(&st, 0x8E8A84); cv_ell(cv, 32, CG + 1.5f, 27, 4, 0, &g, SH_SPHERE); cv_ell(cv, 52, CG, 5, 3.4f, 0.2f, &st, SH_SPHERE); cv_ell(cv, 56, CG + 1, 3.4f, 2.4f, 0, &st, SH_SPHERE); cv_ell(cv, 10, CG + 0.6f, 3.6f, 2.5f, 0, &st, SH_SPHERE);
        for(int i = 0; i < 7; i++){ float x = 6 + i * 8 + sg_rr(&r, -2, 2); for(int j = -1; j <= 1; j++) cv_flat(cv, x, CG + 2, x + j * 1.3f, CG - 2.5f - (j == 0), 0.35f, 0x9ABA5A, 0.95f); } for(int i = 0; i < 3; i++) cv_dot(cv, 15 + i * 17, CG + 2, 0.8f, i & 1 ? 0xF0D060 : 0xF080A0, 1); break;
    case 2: sg_ramp(&g, 0x3A3846); sg_ramp(&st, 0x666470); cv_ell(cv, 32, CG + 1.5f, 27, 4, 0, &g, SH_SPHERE); for(int i = 0; i < 5; i++){ float x = i < 3 ? 6 + i * 5 : 46 + (i - 3) * 7; float hh = 7 + (i * 5 % 7); cv_tri(cv, x - 2.4f, CG + 1.5f, x + 2.4f, CG + 1.5f, x + 0.3f, CG - hh, i & 1 ? &c->ac : &st, 0.9f, 0.3f); }
        for(int i = 0; i < 3; i++){ cv_glow(cv, 8 + i * 3, CG - 5, 3.2f, c->glow, 0.35f); cv_dot(cv, 8.2f + i * 3, CG - 5, 0.7f, 0xFFFFFF, 0.9f); } cv_ell(cv, 54, CG + 1, 2.2f, 1.4f, 0, &st, SH_SPHERE); break;
    default: sg_ramp(&g, 0xE8D8A0); sg_ramp(&st, 0x7ABCCF); cv_ell(cv, 32, CG + 1.5f, 27, 4.2f, 0, &g, SH_SPHERE); cv_ell(cv, 10, CG + 4, 14, 2, 0, &st, SH_FLATLIT); cv_flat(cv, 4, CG + 3.3f, 17, CG + 3.0f, 0.4f, 0xFFFFFF, 0.9f);
        { float sx = 52, sy = CG + 0.5f; for(int i = -3; i <= 3; i++) cv_tri(cv, sx + i * 1.4f - 0.7f, sy, sx + i * 1.4f + 0.7f, sy, sx + i * 2.2f, sy - 5 + (i < 0 ? -i : i) * 0.7f, i & 1 ? &c->ac : &c->bn, 0.9f, 0.4f); } cv_dot(cv, 7, CG + 0.4f, 0.9f, 0xD0C090, 1); cv_dot(cv, 45, CG + 2, 0.7f, 0xB8A870, 1); break; }
}
// ---- the gift: a sign of it around the creature
static void gift_back(Cr *c){
    SgCv *cv = &c->cv; float cx = c->bx ? c->bx : 32, cy = c->by ? c->by : 36; (void)cx; (void)cy; int g = c->gift; if(g < 0 || g > 15) return;
    switch(g){
    case 3: cv_glow(cv, 32, 36, 26, 0xA0A8B8, 0.18f); break; case 6: cv_glow(cv, 32, 34, 28, 0x80FFA0, 0.3f); break; case 11: cv_glow(cv, 32, 34, 28, 0xFFE070, 0.4f); break; case 14: cv_glow(cv, 32, 34, 26, 0xFFD850, 0.3f); break;
    case 15: cv_glow(cv, 32, 40, 24, 0xFF7020, 0.28f); break; case 12: cv_glow(cv, 32, 34, 26, 0xB090FF, 0.25f); break; case 0: cv_glow(cv, 32, 30, 26, 0x6080FF, 0.2f); break;
    case 2: for(int i = 0; i < 3; i++) cv_ell(cv, 32, CG + 0.8f, 11 + i * 6, 1.5f + i * 0.7f, 0, &c->bn, SH_FLAT); break; case 4: case 9: case 1: case 5: case 7: case 8: case 10: case 13: default: break; }
}
static void star4(Cr *c, float x, float y, float r, uint32_t col){ SgCv *cv = &c->cv; cv_flat(cv, x - r, y, x + r, y, r * 0.22f, col, 1); cv_flat(cv, x, y - r, x, y + r, r * 0.22f, col, 1); cv_dot(cv, x, y, r * 0.35f, 0xFFFFFF, 1); }
static void gift_front(Cr *c){
    SgCv *cv = &c->cv; int g = c->gift; if(g < 0 || g > 15) return; float ex = c->ex ? c->ex : 36, ey = c->ey ? c->ey : 24, top = c->by - c->bry - 6; if(top < 6) top = 6; float tx = c->bx ? c->bx : 32;
    switch(g){
    case 0: cv_glow(cv, ex, ey, 4.0f, 0x70A0FF, 0.7f); cv_dot(cv, ex, ey, c->er * 0.45f, 0xE0F0FF, 1); cv_flat(cv, 8, 8, 8, 8, 0.1f, 0, 0); cv_dot(cv, 52, 9, 4.5f, 0xE8EEFF, 1); cv_dot(cv, 54, 8, 4, 0x26304A, 1); cv_dot(cv, 52, 9, 4.5f, 0xE8EEFF, 0); break;
    case 1: for(int i = 0; i < 4; i++) cv_curve(cv, tx - 6 + i * 4, top, tx - 8 + i * 5, top + 10, tx - 10 + i * 7, c->by, 0.18f, 0.12f, &c->bn, SH_FLAT); break;
    case 2: cv_flat(cv, 10, CG + 2, 54, CG + 2, 0.25f, 0xFFFFFF, 0.6f); break;
    case 3: for(int i = 0; i < 4; i++){ float xy[10]; for(int j = 0; j < 5; j++){ float a = j * SG_TAU / 5 + i; xy[j * 2] = tx - 8 + i * 5 + cosf(a) * 2.4f; xy[j * 2 + 1] = c->by - c->bry * 0.3f + (i % 2) * 4 + sinf(a) * 2.2f; } SgRamp st; sg_ramp(&st, 0x9498A4); cv_poly(cv, xy, 5, &st, 0.85f, 0.4f); } break;
    case 4: for(int i = 0; i < 3; i++){ float x = tx + 10 + i * 4, y = top - 2 - i * 3; cv_dot(cv, x, y + 3, 1.3f, 0xFFE8A0, 1); cv_flat(cv, x + 1.2f, y + 3, x + 1.2f, y - 2, 0.35f, 0xFFE8A0, 1); cv_flat(cv, x + 1.2f, y - 2, x + 3, y - 1, 0.35f, 0xFFE8A0, 1); } break;
    case 5: for(int i = 0; i < 4; i++) cv_flat(cv, 4 + i * 2, c->by - 6 + i * 4, 14 + i * 2, c->by - 6 + i * 4, 0.35f, 0xFFFFFF, 0.55f - i * 0.1f); break;
    case 6: cv_flat(cv, tx + 12, top, tx + 12, top + 7, 1.5f, 0x40E070, 1); cv_flat(cv, tx + 8.5f, top + 3.5f, tx + 15.5f, top + 3.5f, 1.5f, 0x40E070, 1); cv_flat(cv, tx + 12, top, tx + 12, top + 7, 0.5f, 0xE8FFE8, 1); cv_flat(cv, tx + 8.5f, top + 3.5f, tx + 15.5f, top + 3.5f, 0.5f, 0xE8FFE8, 1); break;
    case 7: cv_ell(cv, tx + 12, top, 5, 3, 0, &c->bn, SH_SPHERE); cv_ell(cv, tx + 9, top + 1, 3.5f, 2.4f, 0, &c->bn, SH_SPHERE); cv_flat(cv, tx + 12, top + 3, tx + 10.5f, top + 6, 0.5f, 0xFFE040, 1); cv_flat(cv, tx + 10.5f, top + 6, tx + 12.6f, top + 6, 0.5f, 0xFFE040, 1); cv_flat(cv, tx + 12.6f, top + 6, tx + 11, top + 9, 0.5f, 0xFFE040, 1); break;
    case 8: cv_rrect(cv, tx + 6, top - 4, tx + 18, top + 4, 2, &c->bn, SH_FLATLIT); cv_tri(cv, tx + 8, top + 3, tx + 11, top + 3, tx + 7, top + 8, &c->bn, 0.9f, 0.7f); cv_text(cv, tx + 10.2f, top - 2.4f, "?", 1.1f, 0x403028); break;
    case 9: for(int i = 0; i < 2; i++){ cv_ell(cv, 9 + i * 0, CG - 2 - i * 5, 3.5f, 2.5f, 0, &c->dk, SH_SPHERE); } cv_flat(cv, 54, CG, 54, CG - 18, 0.35f, 0x5A4030, 1); cv_tri(cv, 54, CG - 18, 54, CG - 12, 60, CG - 15, &c->ac, 0.9f, 0.4f); cv_dot(cv, 56, CG - 15, 0.8f, 0xFFE070, 1); break;
    case 10: for(int i = 0; i < 6; i++){ float x = 8 + i * 9 + sinf(i * 2.0f) * 2; cv_flat(cv, x, CG + 1.5f, x, CG - 2.5f, 0.3f, 0x4E8A38, 1); for(int j = 0; j < 5; j++){ float a = j * SG_TAU / 5; cv_dot(cv, x + cosf(a) * 1.1f, CG - 3.4f + sinf(a) * 1.1f, 0.8f, i & 1 ? 0xF080B0 : 0xFFE060, 1); } cv_dot(cv, x, CG - 3.4f, 0.55f, 0xFFF4C0, 1); } break;
    case 11: cv_glow(cv, tx, c->by, 12, 0xFFF0A0, 0.28f); for(int i = 0; i < 5; i++) star4(c, tx + sinf(i * 2.6f) * 16, c->by - 8 + cosf(i * 1.9f) * 14, 1.5f, 0xFFF0A0); break;
    case 12: cv_glow(cv, tx + 14, c->by, 8, 0xB090FF, 0.4f); for(int i = 0; i < 6; i++) cv_dot(cv, tx + 10 + (i % 3) * 4.5f, c->by - 8 + (i / 3) * 6 + (i % 2) * 2, 0.7f, 0xD8C8FF, 0.9f); break;
    case 13: for(int i = 0; i < 3; i++) star4(c, 8 + i * 6, CG - 4 - (i & 1) * 3, 1.3f, 0xFFE070); cv_ell(cv, 54, top + 2, 3.2f, 3.2f, 0, &c->ac, SH_SPHERE); cv_dot(cv, 54, top + 2, 2.6f, 0xF0C020, 1); cv_dot(cv, 54, top + 2, 1.8f, 0xFFE880, 1); cv_flat(cv, 54, top + 0.6f, 54, top + 3.4f, 0.4f, 0xA07810, 1); break;
    case 14: cv_flat(cv, tx + 13, top + 6, tx + 13, top + 11, 0.4f, 0x2A7A38, 1); for(int i = 0; i < 4; i++){ float a = i * SG_PI / 2 + 0.78f; cv_ell(cv, tx + 13 + cosf(a) * 1.8f, top + 3.5f + sinf(a) * 1.8f, 1.9f, 1.9f, 0, &c->bn, 0); cv_dot(cv, tx + 13 + cosf(a) * 1.8f, top + 3.5f + sinf(a) * 1.8f, 1.7f, 0x40C060, 1); } star4(c, tx + 20, top - 3, 1.6f, 0xFFE070); break;
    case 15: for(int i = 0; i < 5; i++) cv_dot(cv, tx - 14 + i * 3.4f + sinf(i * 3.0f) * 2, c->by - c->bry - 3 - ((i * 7) % 5) * 2.2f, 0.7f, i & 1 ? 0xFF8030 : 0xFFD060, 0.9f);
        { float fx = tx - c->brx - 3, fy = c->by - 2; cv_ell(cv, fx, fy, 2.6f, 4.2f, 0.4f, &c->ac, SH_SPHERE); cv_glow(cv, fx, fy, 6, 0xFF8030, 0.55f); cv_tri(cv, fx - 2.2f, fy + 1.5f, fx + 2.2f, fy + 1.5f, fx + 0.2f, fy - 6, &c->ac, 0.85f, 0.4f); cv_tri(cv, fx - 1.2f, fy + 1.5f, fx + 1.2f, fy + 1.5f, fx + 0.1f, fy - 3, &c->bn, 0.95f, 0.8f); } break;
    default: break; }
}
// ---- mutations (drawn after the body)
static void mut_fx(Cr *c){
    SgCv *cv = &c->cv; float ex = c->ex ? c->ex : 38, ey = c->ey ? c->ey : 26, er = c->er ? c->er : 1.6f; float bx = c->bx, by = c->by, brx = c->brx, bry = c->bry; int m = c->mut;
    switch(m){
    case 1: cv_dot(cv, ex - er * 0.4f, ey - er * 2.7f, er * 0.55f, 0xFFFFFF, 1); cv_dot(cv, ex - er * 0.4f, ey - er * 2.7f, er * 0.3f, 0x181420, 1); cv_dot(cv, ex + er * 1.9f, ey - er * 2.3f, er * 0.5f, 0xFFFFFF, 1); cv_dot(cv, ex + er * 1.9f, ey - er * 2.3f, er * 0.28f, 0x181420, 1); break;
    case 4: for(int i = 0; i < 9; i++){ float a = SG_PI + i * SG_PI / 8, x = bx + cosf(a) * brx * 0.95f, y = by + sinf(a) * bry * 0.95f; float dx = cosf(a), dy = sinf(a); cv_tri(cv, x - dy * 0.9f, y + dx * 0.9f, x + dy * 0.9f, y - dx * 0.9f, x + dx * 4.2f, y + dy * 4.2f, &c->bl, 0.95f, 0.4f); } break;
    case 6: cr_horn(c, ex - er * 0.2f, ey - er * 3.2f, 1.1f, 5, 1); break;
    case 7: cv_curve(cv, bx - brx + 1, by, bx - brx - 8, by - 6, bx - brx - 7, by - 14, 1.2f, 0.3f, &c->ac, SH_SPHERE); cv_curve(cv, bx - brx + 1, by + 1, bx - brx - 8, by + 4, bx - brx - 12, by - 3, 1.2f, 0.3f, &c->ac, SH_SPHERE); break;
    case 8: for(int i = 0; i < 4; i++) cv_flat(cv, bx - brx * 0.45f + i * brx * 0.3f, by - bry * 0.4f, bx - brx * 0.45f + i * brx * 0.3f + 0.7f, by + bry * 0.45f, 0.4f, 0xF4EEDC, 0.65f); cv_flat(cv, bx - brx * 0.55f, by, bx + brx * 0.5f, by, 0.4f, 0xF4EEDC, 0.65f); break;
    case 9: for(int sd = -1; sd <= 1; sd += 2){ float xy[6] = { bx + sd * 1, by - bry, bx + sd * 11, by - bry - 11, bx + sd * 9, by - bry + 1 }; cv_poly(cv, xy, 3, &c->ac, 0.9f, 0.4f); for(int i = 0; i < 3; i++) cv_flat(cv, bx + sd * (3 + i * 2.6f), by - bry - 1 - i * 2.6f, bx + sd * (4 + i * 2.1f), by - bry + 0.4f, 0.4f, 0xFFFFFF, 0.7f); } break;
    case 10: cv_tri(cv, ex - er * 2, ey - er * 3, ex + er * 2, ey - er * 3, ex, ey - er * 6, &c->ac, 0.9f, 0.4f); cv_rect(cv, ex - er * 2, ey - er * 3, ex + er * 2, ey - er * 2.3f, &c->ac, 0.7f, 0.5f); { SgRamp g; sg_ramp(&g, 0xF0C020); cv_tri(cv, ex - er * 1.6f, ey - er * 2.5f, ex + er * 1.6f, ey - er * 2.5f, ex, ey - er * 5.4f, &g, 0.95f, 0.5f); cv_rect(cv, ex - er * 1.7f, ey - er * 2.9f, ex + er * 1.7f, ey - er * 2.3f, &g, 0.9f, 0.6f); for(int i = -1; i <= 1; i++) cv_dot(cv, ex + i * er * 1.2f, ey - er * 3.9f, er * 0.35f, 0xFF4060, 1); } break;
    case 11: { float rx = bx - brx - 4, ry = by - 3; for(int i = 0; i < 12; i++){ float a0 = i * SG_TAU / 12, a1 = (i + 1) * SG_TAU / 12; cv_flat(cv, rx + cosf(a0) * 3.8f, ry + sinf(a0) * 3.8f, rx + cosf(a1) * 3.8f, ry + sinf(a1) * 3.8f, 1.1f, c->ac.c[2], 1); } cv_flat(cv, rx + 3.8f, ry, bx - brx + 1, by, 1.1f, c->ac.c[2], 1); } break;
    case 12: for(int i = 0; i < 2; i++){ float x = ex + er * (4.5f + i * 2.2f); cv_curve(cv, x, ey + er * 1.4f - 2.5f - i, x + 1.6f, ey + er * 1.4f, x, ey + er * 1.4f + 2.5f + i, 0.35f, 0.35f, &c->bn, SH_FLAT); } break;
    case 13: for(int i = 0; i < 7; i++){ float x = ex + er * 5 + i * 2.4f, y = ey + er * 2.2f + sinf(i * 2.3f) * 2.4f + i * 0.5f; cv_glow(cv, x, y, 1.9f + i * 0.2f, 0xD8F4FF, 0.45f); cv_dot(cv, x, y, 0.55f + (i % 3) * 0.2f, 0xFFFFFF, 0.95f); } break;
    case 14: cv_ell(cv, ex, ey + er * 3.4f, er * 2.6f, er * 1.4f, 0, &(SgRamp){ { 0x2A4A22, 0x3E7A32, 0x5EA044, 0x88C060, 0xB0E080 } }, SH_SPHERE); for(int i = 0; i < 4; i++) cv_curve(cv, ex - er * 1.6f + i * er * 1.1f, ey + er * 3.8f, ex - er * 1.6f + i * er * 1.1f + 0.5f, ey + er * 5.5f, ex - er * 1.6f + i * er * 1.1f - 0.3f, ey + er * 6.8f, 0.45f, 0.2f, &(SgRamp){ { 0x2A4A22, 0x3E7A32, 0x5EA044, 0x88C060, 0xB0E080 } }, SH_FLAT); break;
    case 15: star4(c, ex + er * 3.2f, ey + er * 2.4f, 2.0f, 0xFFFFFF); star4(c, ex + er * 5.2f, ey + er * 1.4f, 1.3f, 0xD8F4FF); break;
    default: break; }
}
static void rank_fx(Cr *c){
    SgCv *cv = &c->cv; float ex = c->ex ? c->ex : 38, ey = c->ey ? c->ey : 26, er = c->er ? c->er : 1.6f;
    switch(c->rank){
    case 3: cv_flat(cv, ex - er * 1.4f, ey - er * 2.4f, ex + er * 1.1f, ey + er * 2.6f, 0.5f, 0xE05A50, 1); cv_flat(cv, ex - er * 1.2f, ey - er * 2.4f, ex + er * 1.3f, ey + er * 2.6f, 0.15f, 0xFFE0D8, 0.9f); break;
    case 4: for(int i = 0; i < 3; i++){ cv_flat(cv, ex + er * 2.2f, ey + er * (1.7f + i * 0.5f), ex + er * 5.4f, ey + er * (1.5f + i * 1.0f), 0.28f, 0xF4F4F4, 0.95f); } cv_curve(cv, ex - er * 1.8f, ey - er * 1.8f, ex - er * 0.5f, ey - er * 3.2f, ex + er * 1.4f, ey - er * 2.6f, 0.4f, 0.4f, &c->bn, SH_FLAT); break;
    case 5: { SgRamp g; sg_ramp(&g, 0xE0B030); cv_curve(cv, ex - er * 3.2f, ey + er * 1.6f, ex - er * 2.4f, ey + er * 4.6f, ex + er * 0.4f, ey + er * 4.6f, 1.0f, 1.0f, &g, SH_SPHERE); cv_dot(cv, ex - er * 1.6f, ey + er * 4.0f, er * 0.8f, 0x40C0FF, 1); cv_dot(cv, ex - er * 1.8f, ey + er * 3.8f, er * 0.28f, 0xFFFFFF, 1); } break;
    case 6: { cv_glow(cv, 32, 34, 30, 0xFFD060, 0.22f); cv_curve(cv, ex - er * 2.4f, ey + er * 3.6f, ex - er * 7, ey + er * 5, ex - er * 11, ey + er * 1.4f, 1.0f, 0.5f, &(SgRamp){ { 0x701818, 0xA02828, 0xD04040, 0xE86A60, 0xFF9A90 } }, SH_VERT); cv_curve(cv, ex - er * 2.4f, ey + er * 3.4f, ex - er * 0.4f, ey + er * 4.6f, ex + er * 1.0f, ey + er * 3.4f, 1.0f, 1.0f, &(SgRamp){ { 0x701818, 0xA02828, 0xD04040, 0xE86A60, 0xFF9A90 } }, SH_VERT); star4(c, 9, 12, 2.4f, 0xFFE070); star4(c, 55, 17, 1.8f, 0xFFE070); } break;
    case 7: { cv_ell(cv, ex - 1, ey - er * 4.6f, 4.4f, 1.2f, 0, &(SgRamp){ { 0xA08020, 0xD0A830, 0xF0D050, 0xFFEC90, 0xFFFFFF } }, SH_SPHERE); cv_glow(cv, ex - 1, ey - er * 4.6f, 7, 0xFFE880, 0.4f);
        for(int i = 0; i < 4; i++){ float x = c->bx - c->brx * 0.5f + i * c->brx * 0.35f, y = c->by - c->bry * 0.3f + (i & 1) * 3; cv_flat(cv, x - 1.2f, y, x + 1.2f, y, 0.32f, 0x90F0FF, 0.9f); cv_flat(cv, x, y - 1.2f, x, y + 1.2f, 0.32f, 0x90F0FF, 0.9f); cv_flat(cv, x - 1, y - 1, x + 1, y + 1, 0.25f, 0x90F0FF, 0.7f); } } break;
    default: break; }
}
static void glass(Cr *c){ SgImg *im = c->cv.im; for(int i = 0; i < im->w * im->h; i++){ uint32_t p = im->px[i]; int a = p >> 24; if(!a) continue; uint32_t q = sg_mixc(p & 0xFFFFFF, 0xDDF4FF, 0.32f); a = (int)(a * 0.68f); im->px[i] = (uint32_t)a << 24 | q; } }

// ------------------------------------------------------------------ the creature
static void body(Cr *c){ int k = c->plan % 11; switch(c->plan / 11){ case 0: cr_beast(c, k); break; case 1: cr_flier(c, k); break; case 2: cr_swim(c, k); break; case 3: cr_crawl(c, k); break; case 4: cr_spirit(c, k); break; default: cr_monster(c, k); } }
static void diet_fx(Cr *c){
    SgCv *cv = &c->cv;
    if(c->diet == 0){ cv_flat(cv, 52, CG + 1, 52, CG - 4, 0.3f, 0x4E8A38, 1); cv_ell(cv, 50.5f, CG - 3.4f, 2.0f, 1.0f, -0.5f, &c->ac, SH_SPHERE); cv_ell(cv, 53.6f, CG - 4.4f, 2.0f, 1.0f, 0.5f, &c->ac, SH_SPHERE); }
    else if(c->diet == 2){ cv_flat(cv, 8, CG, 14, CG - 1.5f, 0.5f, 0xEAE0C8, 1); cv_dot(cv, 7.6f, CG + 0.1f, 1.0f, 0xEAE0C8, 1); cv_dot(cv, 14.4f, CG - 1.7f, 1.0f, 0xEAE0C8, 1); cv_dot(cv, 56, CG, 1.4f, 0x7A5A38, 1); }
    else { for(int i = 0; i < 3; i++) cv_tri(cv, 54 + i * 3 - 1.2f, CG + 1.8f, 54 + i * 3 + 1.2f, CG + 1.8f, 54 + i * 3, CG + 0.2f, &c->bn, 0.9f, 0.5f); }
}
static void temper_fx(Cr *c){
    SgCv *cv = &c->cv; float ex = c->ex ? c->ex : 38, ey = c->ey ? c->ey : 26, er = c->er ? c->er : 1.6f;
    switch(c->beh){
    case 0: cv_ell(cv, ex + er * 3, ey - er * 3.4f, 1.0f, 1.5f, 0, &(SgRamp){ { 0x3A6AB0, 0x5A9AE0, 0x80C0F8, 0xB0E0FF, 0xE8F8FF } }, SH_SPHERE); break;
    case 1: for(int i = 0; i < 2; i++){ float x = ex + er * 3.2f, y = ey - er * 4; cv_flat(cv, x + i * 1.6f, y, x + i * 1.6f - 0.8f, y - 3, 0.5f, 0xE04A3A, 1); } break;
    case 3: for(int i = 0; i < 4; i++) cv_ell(cv, 54 - i * 7, CG + 3.6f, 0.8f, 0.5f, 0, &c->dk, SH_FLAT); break;
    default: break; }
}
void sg_creature(SgImg *im, const SgCrit *s){
    Cr *c = (Cr *)calloc(1, sizeof *c); if(!c) return; int kind = s->kind % 12672; c->plan = kind % 66; int v = kind / 66; c->beh = v % 4; c->hab = (v / 4) % 4; c->diet = (v / 16) % 3; c->size = v / 48;
    c->gift = s->gift; c->col = s->colour & 15; c->rank = s->rank & 7; c->mut = s->mut & 15; c->r = sg_rng(s->seed + 11, (uint64_t)kind * 2654435761u + s->colour * 977u);
    cv_init(&c->cv, im, 64); c->cv.post = 5; SgCv *cv = &c->cv; ramps(c, c->col);
    static const float szk[4] = { 0.64f, 0.8f, 0.94f, 1.0f }, rkk[8] = { 0.66f, 0.84f, 0.96f, 1.0f, 0.96f, 1.0f, 1.0f, 1.0f };
    float k = szk[c->size] * rkk[c->rank]; c->hs = 1.0f + (c->size == 0 ? 0.28f : c->size == 1 ? 0.12f : c->size == 3 ? -0.05f : 0) + (c->rank == 0 ? 0.22f : c->rank == 1 ? 0.1f : 0); c->es = 1.0f + (c->size == 0 ? 0.25f : 0) + (c->rank == 0 ? 0.25f : 0) + (c->beh == 0 ? 0.1f : 0) + (c->hab == 2 ? 0.15f : 0);
    c->ll = c->mut == 3 ? 1.35f : 1.0f; c->ek = c->mut == 5 ? 1.8f : 1.0f; if(c->diet == 2) c->hs *= 1.04f; if(c->mut == 3) k *= 0.9f;
    if(s->scene) habitat(c);
    float k0 = k; if(c->beh == 2) k *= 0.8f;
    cv_view(cv, 32 * (1 - k) , CG * (1 - k), k); gift_back(c); cv_snap(cv);
    if(c->beh == 2){ for(int i = 0; i < 2; i++){ float kk = k0 * 0.52f, dx = i ? 20.0f : -19.0f; Cr cc = *c; cc.copy = 1; cc.es *= 0.9f; cv_view(&cc.cv, 32 * (1 - kk) + dx, CG * (1 - kk) - (i ? 1.5f : 3.5f), kk); body(&cc); } cv_view(cv, 32 * (1 - k), CG * (1 - k), k); }
    body(c);
    if(colt[c->col].pat) cv_pattern(cv, colt[c->col].pat, colt[c->col].pc, colt[c->col].pat == 5 ? 4.0f : 2.8f, colt[c->col].pat == 6 ? 0.9f : 0.55f, (uint32_t)c->col * 77 + c->plan, 0, 0, 64, 64);
    cv_unsnap(cv); mut_fx(c); rank_fx(c); diet_fx(c); temper_fx(c); gift_front(c); if(c->mut == 2) glass(c);
    cv_outline(cv, 0x1C1410, 0.85f); free(c);
}
void sg_creature_kind(SgImg *im, int kind, uint32_t seed){ SgCrit s; memset(&s, 0, sizeof s); s.kind = (uint16_t)(kind % 12672); s.gift = 255; s.colour = (uint8_t)(sg_m64(seed * 7 + kind) & 15); s.rank = 2; s.mut = 0; s.scene = 1; s.seed = seed; sg_creature(im, &s); }
const char *sg_colour_pattern_name(int col){ return colt[col & 15].pat ? colour_pat_name[colt[col & 15].pat] : "PLAIN"; }
