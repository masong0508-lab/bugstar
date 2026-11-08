// pc_sprgen_crit.c: creatures. 66 body plans (6 classes x 11) are drawn by recipes (pc_sprgen_crit.c beasts and fliers, pc_sprgen_crit2.c the rest);
// the variant (size 4 x diet 3 x habitat 4 x behaviour 4) changes proportions, teeth, ground, company; the individual (gift 16, colouring 16, rank 8, mutation 16)
// adds auras, colours, patterns, scars, crowns and odd parts. Everything is a pure function of the numbers.
#include "pc_sprgen_crit.h"
#include <stdlib.h>

// ------------------------------------------------------------------ shared parts
void cr_eye(Cr *c, float x, float y, float r){
    SgCv *cv = &c->cv; r *= c->es; if(!c->copy && c->er == 0){ c->ex = x; c->ey = y; c->er = r; }
    int cave = c->hab == 2, hunt = c->diet == 1;
    if(cave) cv_glow(cv, x, y, r * 2.4f, c->glow, 0.45f);
    cv_ell(cv, x, y, r, r * 1.08f, 0, &c->bn, SH_FLATLIT);
    if(cave) cv_dot(cv, x, y, r * 0.85f, c->glow, 0.85f);
    if(hunt) cv_ell(cv, x + r * 0.1f, y, r * 0.34f, r * 0.92f, 0, &c->dk, SH_FLAT); else cv_dot(cv, x + r * 0.15f, y + r * 0.05f, r * 0.58f, 0x181420, 1);
    cv_dot(cv, x - r * 0.25f, y - r * 0.3f, r * 0.24f, 0xFFFFFF, 1);
    if(hunt || c->beh == 1) cv_flat(cv, x - r * 1.3f, y - r * 1.35f, x + r * 1.15f, y - r * 0.6f, r * 0.34f, sg_scale(c->b.c[0], 0.6f), 1);
}
void cr_mouth(Cr *c, float x, float y, float len, float dir){
    SgCv *cv = &c->cv; cv_flat(cv, x, y, x + dir * len, y + (c->beh == 1 ? -0.4f : 0.5f) * dir, 0.45f, 0x2A1418, 1);
    if(c->diet == 1){ cv_tri(cv, x + dir * len * 0.2f, y, x + dir * len * 0.2f + 1.0f * dir, y, x + dir * len * 0.2f + 0.4f * dir, y + 1.9f, &c->bn, 0.95f, 0.8f); cv_tri(cv, x + dir * len * 0.7f, y, x + dir * len * 0.7f + 0.9f * dir, y, x + dir * len * 0.7f + 0.3f * dir, y + 1.7f, &c->bn, 0.95f, 0.8f); }
}
void cr_leg(Cr *c, float x0, float y0, float x1, float y1, float th, int back, int digi){
    SgCv *cv = &c->cv; const SgRamp *rp = back ? &c->dk : &c->b; float mx = (x0 + x1) * 0.5f + (digi ? 2.4f : -0.6f), my = (y0 + y1) * 0.5f;
    cv_cap(cv, x0, y0, th, mx, my, th * 0.82f, rp, SH_SPHERE); cv_cap(cv, mx, my, th * 0.82f, x1, y1, th * 0.68f, rp, SH_SPHERE);
    cv_ell(cv, x1 + th * 0.55f, y1 + 0.2f, th * 1.25f, th * 0.62f, 0, back ? &c->dk : &c->ac, SH_SPHERE);
    if(c->diet == 1 && !back) for(int i = 0; i < 3; i++) cv_tri(cv, x1 + th * (1.2f + i * 0.35f), y1 - 0.3f + i * 0.2f, x1 + th * (1.2f + i * 0.35f), y1 + 0.9f + i * 0.2f, x1 + th * (2.0f + i * 0.35f), y1 + 0.5f + i * 0.2f, &c->bn, 0.9f, 0.6f);
}
void cr_ear(Cr *c, float x, float y, float s, int type, float dir){
    SgCv *cv = &c->cv; s *= c->ek;
    switch(type){
    case 1: cv_tri(cv, x - 1.6f * s, y, x + 1.6f * s, y, x + dir * 0.6f * s, y - 5.5f * s, &c->b, 0.8f, 0.4f); cv_tri(cv, x - 0.7f * s, y - 0.3f * s, x + 0.7f * s, y - 0.3f * s, x + dir * 0.4f * s, y - 3.4f * s, &c->bl, 0.7f, 0.6f); break;
    case 2: cv_cap(cv, x, y, 1.5f * s, x - dir * 2.2f * s, y + 6.5f * s, 1.3f * s, &c->ac, SH_SPHERE); break;
    case 3: cv_ell(cv, x, y - 1.2f * s, 2.3f * s, 2.3f * s, 0, &c->b, SH_SPHERE); cv_ell(cv, x, y - 1.0f * s, 1.3f * s, 1.3f * s, 0, &c->bl, SH_FLATLIT); break;
    case 4: cv_ell(cv, x - dir * 3.2f * s, y + 1.5f * s, 5.5f * s, 6.5f * s, 0, &c->ac, SH_SPHERE); cv_ell(cv, x - dir * 3.0f * s, y + 1.8f * s, 3.4f * s, 4.4f * s, 0, &c->bl, SH_FLATLIT); break;
    default: break; }
}
void cr_horn(Cr *c, float x, float y, float s, int type, float dir){
    SgCv *cv = &c->cv;
    switch(type){
    case 1: for(int i = 0; i < 3; i++){ float ox = i * 2.0f * dir; cv_curve(cv, x - ox * 0.3f, y, x - ox - 1.5f * dir, y - 5 * s, x - ox + 0.5f * dir, y - (8 + i * 1.5f) * s, 0.8f, 0.35f, &c->bn, SH_SPHERE); }
        cv_curve(cv, x + 0.5f, y, x + 2.8f * dir, y - 4 * s, x + 2.2f * dir, y - 7.5f * s, 0.7f, 0.3f, &c->bn, SH_SPHERE); cv_curve(cv, x + 2.4f * dir, y - 4.2f * s, x + 4.5f * dir, y - 5 * s, x + 5.0f * dir, y - 8 * s, 0.5f, 0.25f, &c->bn, SH_SPHERE); break;
    case 2: cv_curve(cv, x, y, x + 4 * dir * s, y - 4.5f * s, x - 0.5f * dir, y - 8 * s, 1.5f, 0.3f, &c->bn, SH_SPHERE); break;
    case 3: cv_curve(cv, x, y, x + 5 * dir * s, y - 2.5f * s, x + 4 * dir * s, y - 7.5f * s, 2.3f, 1.1f, &c->ac, SH_SPHERE); cv_flat(cv, x + 3.5f * dir * s, y - 4 * s, x + 4.5f * dir * s, y - 5.6f * s, 0.25f, 0x40201A, 0.7f); break;
    case 4: { float xy[8] = { x - 1.4f, y, x + 1.4f, y, x + 1.0f * dir, y - 8 * s, x - 0.6f * dir, y - 6 * s }; cv_poly(cv, xy, 4, &c->bn, 0.95f, 0.4f); cv_glow(cv, x, y - 4 * s, 4, 0xC8F4FF, 0.35f); break; }
    case 5: cv_curve(cv, x, y, x + 3.5f * dir, y - 0.5f, x + 4.5f * dir, y - 5 * s, 1.3f, 0.3f, &c->bn, SH_SPHERE); cv_glow(cv, x + 4.5f * dir, y - 5 * s, 3.5f, c->glow, 0.6f); cv_dot(cv, x + 4.4f * dir, y - 4.6f * s, 0.9f, 0xFFF4B0, 1); break;
    default: break; }
}

// ------------------------------------------------------------------ beasts (plans 0..10): four legs, a head on a neck, ears, horns, a tail, a coat
typedef struct { float brx, bry, legl, legt, nx, ny, hr, sn; int ear, horn, tail, cover, snt, digi; } BQ;
static const BQ bq[11] = {
 {15, 9,   7, 2.6f, 13,  1,  5.0f, 3, 1, 0, 5, 1, 0, 0}, {17, 7,   8, 2.0f, 15, -1, 4.6f, 7, 2, 0, 1, 0, 1, 0}, {13, 6,  17, 1.6f, 10, -11, 3.6f, 4, 1, 1, 0, 6, 0, 1},
 {15, 9,   6, 2.8f, 14,  2,  6.0f, 5, 1, 5, 4, 5, 2, 0}, {18, 12, 11, 5.0f, 15,  2,  6.5f, 9, 4, 0, 1, 4, 4, 0}, { 9, 6,   6, 2.2f,  8, -2,  5.0f, 3, 3, 0, 2, 0, 0, 0},
 {17, 12,  9, 4.4f, 14, -2,  7.0f, 4, 3, 0, 3, 7, 0, 0}, {11, 5,  26, 1.3f, 11, -15, 3.2f, 4, 1, 0, 4, 0, 0, 1}, {15, 11,  6, 2.4f, 12,  0,  5.0f, 3, 2, 2, 3, 2, 0, 0},
 {16, 10,  9, 3.4f, 14, -3,  6.0f, 4, 1, 3, 1, 8, 0, 0}, {17, 12,  5, 3.5f, 17, -4,  4.0f, 2, 0, 0, 3, 3, 0, 0} };
void cr_beast(Cr *c, int k){
    const BQ *q = &bq[k]; SgCv *cv = &c->cv; float ll = c->ll, legl = q->legl * ll, by = CG - legl - q->bry * 0.82f, bx = 30.5f - (q->nx > 12 ? 1.5f : 0), brx = q->brx, bry = q->bry; float hr = q->hr * c->hs;
    if(k == 4 || k == 6) bx -= 1;
    c->bx = bx; c->by = by; c->brx = brx; c->bry = bry;
    float hx = bx + q->nx, hy = by + q->ny - (hr - 5) * 0.3f; float tl = brx * (q->tail == 1 ? 1.3f : 0.9f);
    // far legs, tail, body, near legs
    float fl = brx * 0.62f, bl_ = brx * 0.5f; int digi = q->digi;
    cr_leg(c, bx + fl - 2, by + bry * 0.6f, bx + fl - 2.5f, CG, q->legt, 1, digi); cr_leg(c, bx - bl_ + 2, by + bry * 0.6f, bx - bl_ + 1.6f, CG, q->legt, 1, digi);
    switch(q->tail){ case 1: cv_curve(cv, bx - brx + 1, by - 1, bx - brx - tl * 0.6f, by + 2, bx - brx - tl * 0.7f, by + bry * 0.9f + 3, 1.7f, 0.5f, &c->b, SH_SPHERE); break;
        case 2: cv_ell(cv, bx - brx - 3, by - 2, 4, 6.5f, -0.5f, &c->b, SH_SPHERE); cv_ell(cv, bx - brx - 3.5f, by - 3, 2.2f, 4, -0.5f, &c->bl, SH_FLATLIT); break;
        case 3: cv_ell(cv, bx - brx + 0.5f, by - 1, 2.0f, 1.6f, 0, &c->b, SH_SPHERE); break;
        case 4: cv_curve(cv, bx - brx + 1, by - 2, bx - brx - 7, by - 6, bx - brx - 6, by - 11, 1.3f, 0.3f, &c->b, SH_SPHERE); cv_dot(cv, bx - brx - 6, by - 11.5f, 1.6f, c->ac.c[2], 1); break;
        case 5: for(int i = 0; i < 3; i++) cv_tri(cv, bx - brx + 1, by - 2 + i * 2, bx - brx + 1, by + i * 2 + 1.5f, bx - brx - 6 + i, by + i * 2 + 0.5f, &c->ac, 0.8f, 0.4f); break;
        default: break; }
    if(k == 7){ digi = 1; }
    cv_ell(cv, bx, by, brx, bry, 0, &c->b, SH_SPHERE);
    if(q->cover != 3 && q->cover != 8) cv_ell(cv, bx + 1, by + bry * 0.42f, brx * 0.78f, bry * 0.5f, 0, &c->bl, SH_FLATLIT);
    switch(q->cover){
    case 1: for(int i = 0; i < 6; i++){ float t = (i + 0.5f) / 6, x = bx - brx * 0.78f + t * brx * 1.56f, y = by - bry * sqrtf(1 - sg_clampf((x - bx) / brx, -1, 1) * sg_clampf((x - bx) / brx, -1, 1)) + 1.2f; cv_tri(cv, x - 1.7f, y, x + 1.7f, y, x - 0.6f, y - 5.5f - (i & 1) * 1.8f, &c->ac, 0.85f, 0.4f); } break;
    case 2: for(int i = 0; i < 9; i++){ float a = i * 0.7f, x = bx + cosf(a) * brx * (0.2f + 0.7f * (i % 3) / 2.0f), y = by + sinf(a * 1.3f) * bry * 0.6f - 1; cv_ell(cv, x, y, 3.8f, 3.6f, 0, &c->bl, SH_SPHERE); }
        for(int i = 0; i < 5; i++) cv_dot(cv, bx - brx * 0.6f + i * brx * 0.3f, by - bry * 0.4f + (i % 2) * 5, 1.0f, i & 1 ? 0xD03050 : 0x7A3AA0, 1); break;
    case 3: cv_ell(cv, bx, by - 1, brx * 1.02f, bry * 1.05f, 0, &c->ac, SH_SPHERE); cv_ell(cv, bx - 1, by - 2.5f, brx * 0.86f, bry * 0.82f, 0, &c->b, SH_SPHERE);
        { float sx = bx, sy = by - 1.5f; cv_tri(cv, sx - 1.5f, sy + 1.5f, sx + 1.5f, sy + 1.5f, sx + 1.5f, sy - 8, &c->bn, 0.95f, 0.5f); for(int i = 0; i < 9; i++){ float a = SG_PI + i * SG_PI / 8; cv_dot(cv, sx + cosf(a) * brx * 0.62f, sy + 1.5f + sinf(a) * bry * 0.62f, 0.55f, 0x2A2018, 1); } } break;
    case 4: for(int i = 0; i < 5; i++) cv_curve(cv, bx - brx * 0.7f + i * brx * 0.36f, by - bry * 0.5f, bx - brx * 0.7f + i * brx * 0.36f + 1.5f, by, bx - brx * 0.7f + i * brx * 0.36f - 0.5f, by + bry * 0.55f, 0.5f, 0.3f, &c->dk, SH_FLAT); break;
    case 5: for(int i = 0; i < 7; i++) cv_tri(cv, bx + brx * 0.1f + i * 1.8f - 4, by - bry + 1.2f + (i % 2) * 0.8f, bx + brx * 0.1f + i * 1.8f - 2.2f, by - bry + 1.2f, bx + brx * 0.1f + i * 1.8f - 3.4f, by - bry - 3.2f, &c->ac, 0.8f, 0.3f); break;
    case 6: for(int i = 0; i < 6; i++) cv_dot(cv, bx - brx * 0.5f + i * brx * 0.2f, by - bry * 0.2f + sinf(i * 2.1f) * bry * 0.4f, 0.6f, 0xFFF4C0, 0.95f); break;
    case 7: cv_ell(cv, bx - brx * 0.1f, by + 1, 4.5f, 5.5f, 0, &c->dk, SH_SPHERE); cv_ell(cv, bx - brx * 0.1f, by + 1.6f, 3.2f, 4.0f, 0, &c->dk, SH_FLAT); cv_ell(cv, bx - brx * 0.1f - 0.5f, by - 5.6f, 2.4f, 1.1f, 0.6f, &c->ac, SH_SPHERE); cv_dot(cv, bx - brx * 0.1f + 4, by - 1, 0.8f, c->ac.c[3], 1); break;
    case 8: cv_ell(cv, bx + brx * 0.35f, by - bry * 0.75f, brx * 0.45f, bry * 0.4f, 0, &c->b, SH_SPHERE); break;
    default: break; }
    cr_leg(c, bx + fl, by + bry * 0.6f, bx + fl + 0.5f, CG, q->legt, 0, digi); cr_leg(c, bx - bl_, by + bry * 0.6f, bx - bl_ - 0.6f, CG, q->legt, 0, digi);
    // neck and head
    float nkw = hr * (k == 7 ? 0.35f : 0.8f); cv_cap(cv, bx + brx * 0.62f, by - bry * 0.3f, bry * (k == 7 ? 0.3f : 0.7f), hx, hy, nkw, &c->b, SH_SPHERE);
    if(q->horn && q->horn != 5 && q->horn != 3) cr_horn(c, hx - hr * 0.1f, hy - hr * 0.8f, 1.0f, q->horn, 1);
    cr_ear(c, hx - hr * 0.35f, hy - hr * 0.7f, 1.0f, q->ear, 1);
    cv_ell(cv, hx, hy, hr, hr * 0.92f, 0, &c->b, SH_SPHERE);
    float sn = q->sn * (c->hs > 1.1f ? 0.8f : 1.0f), sx = hx + hr * 0.8f + sn * 0.45f, sy = hy + hr * 0.25f;
    if(q->snt == 4){ cv_curve(cv, hx + hr * 0.7f, hy + hr * 0.2f, hx + hr + 4, hy + hr * 0.9f, hx + hr + 4.5f, hy + hr + 6, hr * 0.6f, hr * 0.3f, &c->b, SH_SPHERE); sx = hx + hr + 4; sy = hy + hr + 3; }
    else { cv_ell(cv, sx, sy, sn * 0.65f + hr * 0.25f, hr * (q->snt == 1 ? 0.42f : 0.55f), 0, &c->bl, SH_SPHERE); cv_dot(cv, sx + sn * 0.5f, sy - hr * 0.2f, 0.7f, 0x2A1418, 1); }
    if(q->horn == 3 || q->horn == 5) cr_horn(c, hx + hr * (q->horn == 5 ? 0.9f : -0.1f), q->horn == 5 ? hy + hr * 0.5f : hy - hr * 0.8f, 1.0f, q->horn, 1);
    cr_eye(c, hx + hr * 0.2f, hy - hr * 0.25f, 1.7f); if(q->snt != 4) cr_mouth(c, sx - sn * 0.2f, sy + hr * 0.3f, sn * 0.8f, 1);
    if(q->horn == 4) cr_horn(c, hx, hy - hr * 0.9f, 1, 4, 1);
    // plan details
    if(k == 0) for(int i = 0; i < 3; i++) cv_tri(cv, hx - hr * 0.7f + i * 2, hy - hr * 0.8f, hx - hr * 0.7f + i * 2 + 1.6f, hy - hr * 0.8f, hx - hr * 0.7f + i * 2 + 0.2f, hy - hr * 0.8f - 3.2f, &c->ac, 0.8f, 0.4f);
    if(k == 1){ cv_dot(cv, hx + hr * 0.6f, hy + hr * 0.9f, 1.2f, 0x6A4A28, 0.9f); cv_dot(cv, bx - 3, by + bry * 0.7f, 1.6f, 0x5A4020, 0.9f); cv_dot(cv, bx + 4, by + bry * 0.9f, 1.1f, 0x5A4020, 0.9f); }
    if(k == 3) cv_glow(cv, sx + sn * 0.4f, sy + hr * 0.4f, 5, c->glow, 0.5f);
    if(k == 5){ cv_poly(cv, (float[]){ hx + hr * 1.4f + 3, hy + hr * 1.3f, hx + hr * 1.4f + 5, hy + hr * 1.3f + 1.5f, hx + hr * 1.4f + 3, hy + hr * 1.3f + 4, hx + hr * 1.4f + 1, hy + hr * 1.3f + 1.5f }, 4, &c->bn, 0.95f, 0.5f); cv_glow(cv, hx + hr * 1.4f + 3, hy + hr * 1.3f + 1.8f, 4, 0xB8F0FF, 0.5f); }
    if(k == 8) cv_dot(cv, hx - hr * 0.2f, hy - hr * 1.1f, 1.4f, 0x8A3AB0, 1);
    if(k == 6) cv_flat(cv, hx - 1, hy - hr * 0.1f - 1.2f, hx + 3, hy - hr * 0.1f - 1.2f, 0.7f, c->b.c[0], 1);
    if(k == 10){ cv_tri(cv, bx - 2, by - bry * 0.95f, bx + 2, by - bry * 0.95f, bx + 0.5f, by - bry * 0.95f - 5.5f, &c->bn, 0.95f, 0.6f); }
    (void)tl;
}

// ------------------------------------------------------------------ fliers (plans 11..21)
static void wingfan(Cr *c, float x, float y, float dx, float dy, int n, float len, float r, const SgRamp *rp){      // n feathers fanned between two directions
    for(int i = 0; i < n; i++){ float t = n == 1 ? 0.5f : i / (float)(n - 1), a = atan2f(dy, dx) + (t - 0.5f) * 1.3f; cv_cap(&c->cv, x, y, r, x + cosf(a) * len, y + sinf(a) * len, r * 0.45f, rp, SH_SPHERE); } }
static void spots_eye(Cr *c, float x, float y, float r, uint32_t col){ cv_dot(&c->cv, x, y, r, col, 1); cv_dot(&c->cv, x, y, r * 0.62f, 0xFFF4D0, 1); cv_dot(&c->cv, x, y, r * 0.3f, 0x201420, 1); }
void cr_flier(Cr *c, int k){
    SgCv *cv = &c->cv; float hs = c->hs; c->front = 0;
    switch(k){
    case 0: { float cx = 32, cy = 36; c->bx = cx; c->by = cy; c->brx = 3; c->bry = 9;      // GLIMMERWING moth: front view, two soft wings, dust
        for(int sd = -1; sd <= 1; sd += 2){ cv_ell(cv, cx + sd * 12, cy - 6, 13, 9, sd * -0.5f, &c->ac, SH_SPHERE); cv_ell(cv, cx + sd * 9, cy + 7, 8, 6.5f, sd * 0.6f, &c->b, SH_SPHERE); spots_eye(c, cx + sd * 15, cy - 7, 3.2f, c->b.c[3]); for(int i = 0; i < 5; i++) cv_dot(cv, cx + sd * (6 + i * 3.4f), cy - 12 + (i % 2) * 3, 0.6f, 0xFFF4C0, 0.9f); }
        cv_ell(cv, cx, cy, 3.4f, 9, 0, &c->b, SH_SPHERE); cv_ell(cv, cx, cy - 11 * hs, 3.4f * hs, 3.4f * hs, 0, &c->bl, SH_SPHERE); c->front = 1;
        for(int sd = -1; sd <= 1; sd += 2) cv_curve(cv, cx + sd * 1.5f, cy - 13.5f * hs, cx + sd * 5, cy - 19, cx + sd * 7, cy - 21, 0.4f, 0.25f, &c->dk, SH_FLAT);
        cr_eye(c, cx - 1.6f, cy - 11 * hs, 1.3f); cr_eye(c, cx + 1.7f, cy - 11 * hs, 1.3f); for(int i = 0; i < 9; i++) cv_dot(cv, cx + sinf(i * 2.4f) * 9, cy + 12 + i * 1.5f, 0.55f, i & 1 ? 0xFFF4C0 : c->ac.c[3], 0.9f - i * 0.07f); break; }
    case 1: { float bx = 30, by = 36; c->bx = bx; c->by = by; c->brx = 11; c->bry = 10; cv_ell(cv, bx - 9, by + 6, 6, 2.2f, -0.35f, &c->ac, SH_SPHERE);      // KETTLEKITE: a round hawk that whistles
        cv_cap(cv, bx - 1, by + 8, 1.4f, bx - 1.5f, CG - 3, 1.1f, &c->bn, SH_SPHERE); cv_cap(cv, bx + 4, by + 8, 1.4f, bx + 4, CG - 3, 1.1f, &c->bn, SH_SPHERE);
        cv_ell(cv, bx, by, 11, 10, 0, &c->b, SH_SPHERE); cv_ell(cv, bx + 2, by + 3.5f, 7.5f, 6, 0, &c->bl, SH_FLATLIT); cv_ell(cv, bx - 3, by - 1, 8, 5.5f, -0.4f, &c->ac, SH_SPHERE); for(int i = 0; i < 4; i++) cv_flat(cv, bx - 8 + i * 2.6f, by + 1 + i * 1.6f, bx - 4 + i * 2.6f, by + 3 + i * 1.6f, 0.4f, c->ac.c[0], 0.9f);
        float hx = bx + 8, hy = by - 8; cv_ell(cv, hx, hy, 5.2f * hs, 5 * hs, 0, &c->b, SH_SPHERE); cv_tri(cv, hx + 4, hy - 0.5f, hx + 4, hy + 2.8f, hx + 8.8f, hy + 2.4f, &c->bn, 0.95f, 0.6f); cr_eye(c, hx + 1.5f, hy - 1, 1.5f);
        for(int i = 0; i < 3; i++) cv_curve(cv, hx + 7 + i * 0.5f, hy - 5 - i * 2, hx + 10 + i * 0.5f, hy - 7 - i * 2, hx + 7.5f + i * 1.0f, hy - 10 - i * 2, 0.5f, 0.3f, &c->bl, SH_FLAT); break; }
    case 2: { float bx = 31, by = 38; c->bx = bx; c->by = by; c->brx = 10; c->bry = 6; cv_ell(cv, bx - 10, by - 2, 6, 2, 0.3f, &c->b, SH_SPHERE);      // ROOKLET: a clever little crow with a button
        cv_cap(cv, bx - 1, by + 4, 1.1f, bx - 2, CG - 2, 0.9f, &c->dk, SH_FLAT); cv_cap(cv, bx + 3, by + 4, 1.1f, bx + 3, CG - 2, 0.9f, &c->dk, SH_FLAT); cv_flat(cv, bx - 4.5f, CG - 1, bx - 0.5f, CG - 1, 0.5f, 0x2A2018, 1); cv_flat(cv, bx + 1, CG - 1, bx + 5, CG - 1, 0.5f, 0x2A2018, 1);
        cv_ell(cv, bx, by, 11, 6.5f, -0.2f, &c->b, SH_SPHERE); cv_ell(cv, bx - 2, by - 1, 7, 4, -0.3f, &c->ac, SH_SPHERE); float hx = bx + 9, hy = by - 7; cv_ell(cv, hx, hy, 5 * hs, 4.6f * hs, 0, &c->b, SH_SPHERE);
        cv_tri(cv, hx + 3.5f, hy - 1.2f, hx + 3.5f, hy + 1.8f, hx + 10, hy + 0.6f, &c->dk, 0.9f, 0.5f); cr_eye(c, hx + 0.8f, hy - 1, 1.5f); cv_dot(cv, hx + 7.5f, hy + 2.8f, 1.6f, 0xE0C060, 1); cv_dot(cv, hx + 7.5f, hy + 2.8f, 0.5f, 0x504020, 1); break; }
    case 3: { float cx = 32, cy = 26; c->bx = cx; c->by = cy; c->brx = 9; c->bry = 4;       // SKYWHELK: a glider with a spiral shell hanging below
        cv_curve(cv, cx - 25, cy + 3, cx, cy - 9, cx + 25, cy + 3, 1.8f, 1.8f, &c->b, SH_SPHERE); cv_curve(cv, cx - 25, cy + 3, cx - 12, cy + 1, cx, cy + 4, 0.4f, 3, &c->ac, SH_VERT); cv_curve(cv, cx + 25, cy + 3, cx + 12, cy + 1, cx, cy + 4, 0.4f, 3, &c->ac, SH_VERT);
        cv_ell(cv, cx, cy + 3, 12, 4, 0, &c->bl, SH_SPHERE); cv_ell(cv, cx + 9, cy + 1, 4 * hs, 3.4f * hs, 0, &c->b, SH_SPHERE); for(int sd = 0; sd < 2; sd++){ cv_curve(cv, cx + 10, cy - 1, cx + 13 + sd * 2, cy - 6, cx + 14 + sd * 3, cy - 8, 0.4f, 0.3f, &c->dk, SH_FLAT); cv_dot(cv, cx + 14 + sd * 3, cy - 8, 1.2f, 0xFFFFFF, 1); cv_dot(cv, cx + 14.3f + sd * 3, cy - 8, 0.6f, 0x181420, 1); }
        c->ex = cx + 14; c->ey = cy - 8; c->er = 1; cv_ell(cv, cx - 2, cy + 15, 10, 9.5f, 0.3f, &c->ac, SH_SPHERE); for(int i = 0; i < 4; i++) cv_ell(cv, cx - 2 + i * 0.4f, cy + 15, 8.5f - i * 2.1f, 8 - i * 2.0f, 0.3f, i & 1 ? &c->bn : &c->ac, SH_SPHERE); cv_dot(cv, cx - 1, cy + 14.6f, 1.0f, c->dk.c[1], 1);
        cv_flat(cv, cx - 1, cy + 6, cx - 1, cy + 8, 1.2f, c->dk.c[2], 1); break; }
    case 4: { float cx = 32, cy = 34; c->bx = cx; c->by = cy; c->brx = 4; c->bry = 8;       // DUSTMOTH: grey, with page-shaped wings
        for(int sd = -1; sd <= 1; sd += 2){ for(int j = 0; j < 2; j++){ float xy[8] = { cx + sd * 3, cy - 3 + j * 8, cx + sd * 20, cy - 12 + j * 18, cx + sd * 22, cy - 2 + j * 18, cx + sd * 4, cy + 4 + j * 8 }; cv_poly(cv, xy, 4, j ? &c->b : &c->bl, 0.9f, 0.5f); for(int i = 0; i < 4; i++) cv_flat(cv, cx + sd * (7 + i * 0.5f), cy - 7 + j * 14 + i * 2.3f, cx + sd * (19 + i * 0.3f), cy - 9 + j * 14 + i * 2.1f, 0.3f, c->dk.c[1], 0.7f); } }
        cv_ell(cv, cx, cy + 1, 3.4f, 9, 0, &c->ac, SH_SPHERE); cv_ell(cv, cx, cy - 9.5f * hs, 3.6f * hs, 3.4f * hs, 0, &c->ac, SH_SPHERE); c->front = 1; cr_eye(c, cx - 1.7f, cy - 9.5f * hs, 1.3f); cr_eye(c, cx + 1.7f, cy - 9.5f * hs, 1.3f);
        for(int sd = -1; sd <= 1; sd += 2){ cv_curve(cv, cx + sd * 1.5f, cy - 12.5f * hs, cx + sd * 4, cy - 18, cx + sd * 7, cy - 19, 0.4f, 0.25f, &c->dk, SH_FLAT); for(int i = 0; i < 4; i++) cv_flat(cv, cx + sd * (3.5f + i * 0.9f), cy - 15 - i * 0.9f, cx + sd * (5.5f + i * 0.9f), cy - 14 - i * 0.2f, 0.3f, c->dk.c[2], 1); }
        for(int i = 0; i < 6; i++) cv_dot(cv, cx + sinf(i * 2.9f) * 10, cy + 14 + i * 1.6f, 0.6f, 0xD8D0C0, 0.8f - i * 0.1f); break; }
    case 5: { float bx = 30, by = 34; c->bx = bx; c->by = by; c->brx = 8; c->bry = 5;      // PAPERCRANE: folded planes on stilts
        cv_flat(cv, bx - 2, by + 4, bx - 3, CG - 1, 0.55f, 0x2A2018, 1); cv_flat(cv, bx + 2, by + 4, bx + 3, CG - 1, 0.55f, 0x2A2018, 1); cv_flat(cv, bx - 5, CG - 0.5f, bx - 1, CG - 0.5f, 0.45f, 0x2A2018, 1); cv_flat(cv, bx + 1, CG - 0.5f, bx + 5, CG - 0.5f, 0.45f, 0x2A2018, 1);
        { float xy[8] = { bx - 14, by + 1, bx - 2, by - 4, bx + 8, by + 1, bx - 2, by + 8 }; cv_poly(cv, xy, 4, &c->b, 0.95f, 0.4f); } cv_tri(cv, bx - 2, by - 4, bx - 10, by - 17, bx + 6, by - 3, &c->bl, 0.95f, 0.5f); cv_tri(cv, bx - 2, by - 4, bx - 12, by - 12, bx - 3, by - 14, &c->ac, 0.85f, 0.3f); cv_flat(cv, bx - 2, by - 4, bx + 1, by + 5, 0.3f, c->dk.c[1], 0.7f);
        cv_flat(cv, bx + 7, by + 1, bx + 12, by - 12, 1.1f, c->bl.c[2], 1); cv_flat(cv, bx + 12, by - 12, bx + 11, by - 15, 1.6f, c->b.c[3], 1); cv_tri(cv, bx + 12, by - 16, bx + 12, by - 13.5f, bx + 17.5f, by - 14.5f, &c->dk, 0.9f, 0.5f); cr_eye(c, bx + 11.2f, by - 15.2f, 1.1f); { float xy[6] = { bx - 14, by + 1, bx - 22, by + 8, bx - 11, by + 4 }; cv_poly(cv, xy, 3, &c->bl, 0.9f, 0.4f); } break; }
    case 6: { float cx = 32, cy = 40; c->bx = cx; c->by = cy; c->brx = 8; c->bry = 9;       // BELLFINCH: a bell with a bird on top
        cv_cap(cv, cx - 1.5f, cy + 8, 0.9f, cx - 2, CG - 1, 0.7f, &c->dk, SH_FLAT); cv_cap(cv, cx + 2.5f, cy + 8, 0.9f, cx + 3, CG - 1, 0.7f, &c->dk, SH_FLAT);
        cv_ell(cv, cx, cy, 8, 9, 0, &c->b, SH_SPHERE); { float xy[8] = { cx - 6, cy + 7, cx + 6, cy + 7, cx + 10, cy + 12, cx - 10, cy + 12 }; cv_poly(cv, xy, 4, &c->bn, 0.9f, 0.4f); } cv_dot(cv, cx, cy + 13.5f, 1.8f, 0xE8C060, 1); cv_ell(cv, cx + 1, cy + 2.5f, 5.5f, 5.5f, 0, &c->bl, SH_FLATLIT); cv_ell(cv, cx - 3, cy - 1, 4.5f, 6, -0.4f, &c->ac, SH_SPHERE);
        float hx = cx + 3, hy = cy - 10; cv_ell(cv, hx, hy, 5 * hs, 4.8f * hs, 0, &c->b, SH_SPHERE); cv_tri(cv, hx + 4, hy - 1, hx + 4, hy + 1.6f, hx + 8, hy + 0.5f, &c->bn, 0.95f, 0.6f); cr_eye(c, hx + 1.4f, hy - 0.8f, 1.5f);
        cv_flat(cv, hx - 5, hy - 5, hx - 8, hy - 8, 0.4f, c->dk.c[1], 0.9f); for(int i = 0; i < 3; i++){ cv_dot(cv, cx + 12 + i * 3, cy - 14 - i * 3 + (i & 1) * 2, 1.0f, 0xFFF0B0, 0.9f); cv_flat(cv, cx + 12.9f + i * 3, cy - 14 - i * 3 + (i & 1) * 2, cx + 12.9f + i * 3, cy - 18 - i * 3 + (i & 1) * 2, 0.35f, 0xFFF0B0, 0.9f); } break; }
    case 7: { float bx = 30, by = 36; c->bx = bx; c->by = by; c->brx = 7; c->bry = 5;      // STORMWREN: a tiny fighter in front of a gale
        cv_glow(cv, bx - 8, by, 18, 0x8AA0D0, 0.35f); for(int i = 0; i < 3; i++) cv_flat(cv, bx - 24, by - 8 + i * 8, bx - 12, by - 8 + i * 8 - 2, 0.7f, 0xDDE8FF, 0.7f);
        { float xy[8] = { bx - 3, by - 2, bx - 14, by - 15, bx - 18, by - 12, bx - 7, by + 1 }; cv_poly(cv, xy, 4, &c->b, 0.9f, 0.4f); } cv_tri(cv, bx - 6, by + 1, bx - 22, by + 9, bx - 16, by + 3, &c->ac, 0.9f, 0.4f); cv_tri(cv, bx - 6, by + 2, bx - 24, by + 2, bx - 12, by - 3, &c->ac, 0.8f, 0.4f);
        cv_cap(cv, bx, by + 4, 0.9f, bx - 1, CG - 8, 0.7f, &c->bn, SH_FLAT); cv_cap(cv, bx + 3, by + 4, 0.9f, bx + 3, CG - 8, 0.7f, &c->bn, SH_FLAT);
        cv_ell(cv, bx, by, 8, 5.5f, -0.1f, &c->b, SH_SPHERE); cv_ell(cv, bx + 1, by + 2, 5, 3, 0, &c->bl, SH_FLATLIT); { float xy[10] = { bx - 3, by - 3, bx - 9, by - 8, bx - 6, by - 8, bx - 12, by - 14, bx - 1, by - 6 }; cv_poly(cv, xy, 5, &c->ac, 0.9f, 0.5f); }
        float hx = bx + 7, hy = by - 4; cv_ell(cv, hx, hy, 4.2f * hs, 4 * hs, 0, &c->b, SH_SPHERE); cv_tri(cv, hx + 3, hy - 1, hx + 3, hy + 1.2f, hx + 7, hy + 0.2f, &c->bn, 0.95f, 0.6f); cr_eye(c, hx + 1, hy - 0.6f, 1.4f); cv_tri(cv, hx - 1, hy - 3.5f, hx + 2, hy - 3.5f, hx - 3, hy - 8, &c->ac, 0.9f, 0.4f);
        { float xy[6] = { bx + 14, by - 10, bx + 12, by - 5, bx + 16, by - 6 }; cv_poly(cv, xy, 3, &c->bl, 0.9f, 0.4f); } cv_flat(cv, bx + 14, by - 10, bx + 12.5f, by - 6.5f, 0.5f, 0xFFF08A, 1); cv_flat(cv, bx + 12.5f, by - 6.5f, bx + 15, by - 7, 0.5f, 0xFFF08A, 1); break; }
    case 8: { float cx = 32, cy = 34; c->bx = cx; c->by = cy; c->brx = 4; c->bry = 6;       // CINDERBAT: soot-coloured, hanging in chimneys
        for(int sd = -1; sd <= 1; sd += 2){ float xy[14] = { cx + sd * 2, cy - 4, cx + sd * 12, cy - 16, cx + sd * 19, cy - 8, cx + sd * 24, cy + 4, cx + sd * 17, cy + 2, cx + sd * 11, cy + 8, cx + sd * 3, cy + 6 }; cv_poly(cv, xy, 7, &c->b, 0.9f, 0.3f);
            cv_flat(cv, cx + sd * 3, cy - 3, cx + sd * 12, cy - 16, 0.55f, c->dk.c[0], 1); cv_flat(cv, cx + sd * 3, cy - 2, cx + sd * 19, cy - 8, 0.5f, c->dk.c[0], 1); cv_flat(cv, cx + sd * 3, cy - 1, cx + sd * 24, cy + 4, 0.5f, c->dk.c[0], 1); cv_flat(cv, cx + sd * 3, cy, cx + sd * 11, cy + 8, 0.5f, c->dk.c[0], 1); }
        cv_ell(cv, cx, cy + 1, 4, 7, 0, &c->b, SH_SPHERE); cv_ell(cv, cx, cy + 3, 2.6f, 4.5f, 0, &c->bl, SH_FLATLIT); cv_ell(cv, cx, cy - 8 * hs, 4 * hs, 3.8f * hs, 0, &c->b, SH_SPHERE); c->front = 1;
        for(int sd = -1; sd <= 1; sd += 2){ cv_tri(cv, cx + sd * 1.5f, cy - 10 * hs, cx + sd * 4.5f, cy - 10 * hs, cx + sd * 3.4f, cy - 17 * c->ek, &c->b, 0.85f, 0.4f); cv_tri(cv, cx + sd * 2.2f, cy - 11 * hs, cx + sd * 3.9f, cy - 11 * hs, cx + sd * 3.4f, cy - 15 * c->ek, &c->bl, 0.7f, 0.5f); }
        cr_eye(c, cx - 1.8f, cy - 8 * hs, 1.2f); cr_eye(c, cx + 1.8f, cy - 8 * hs, 1.2f); cv_flat(cv, cx - 0.9f, cy - 5.5f * hs, cx + 0.9f, cy - 5.5f * hs, 0.4f, 0x2A1418, 1);
        if(c->diet == 1) { cv_tri(cv, cx - 1.1f, cy - 5.4f * hs, cx - 0.3f, cy - 5.4f * hs, cx - 0.7f, cy - 3.8f * hs, &c->bn, 0.95f, 0.8f); cv_tri(cv, cx + 0.3f, cy - 5.4f * hs, cx + 1.1f, cy - 5.4f * hs, cx + 0.7f, cy - 3.8f * hs, &c->bn, 0.95f, 0.8f); }
        for(int i = 0; i < 8; i++) cv_dot(cv, cx + sinf(i * 2.2f) * 20, cy - 14 + cosf(i * 1.7f) * 14, 0.5f, i & 1 ? 0xFF8A30 : 0xFFD070, 0.85f); break; }
    case 9: { float cx = 32, cy = 36; c->bx = cx; c->by = cy; c->brx = 11; c->bry = 13;     // TASSELOWL: front view, silk tassels on the tufts
        cv_cap(cv, cx - 4, cy + 11, 1.2f, cx - 4, CG - 1, 1.0f, &c->bn, SH_FLAT); cv_cap(cv, cx + 4, cy + 11, 1.2f, cx + 4, CG - 1, 1.0f, &c->bn, SH_FLAT); for(int sd = -1; sd <= 1; sd += 2) for(int i = -1; i <= 1; i++) cv_tri(cv, cx + sd * 4 + i * 1.3f - 0.4f, CG - 1.6f, cx + sd * 4 + i * 1.3f + 0.4f, CG - 1.6f, cx + sd * 4 + i * 1.5f, CG + 0.6f, &c->bn, 0.9f, 0.5f);
        for(int sd = -1; sd <= 1; sd += 2) cv_ell(cv, cx + sd * 10, cy + 1, 4.5f, 11, sd * 0.2f, &c->ac, SH_SPHERE); cv_ell(cv, cx, cy, 11, 13, 0, &c->b, SH_SPHERE); cv_ell(cv, cx, cy + 5, 7, 7, 0, &c->bl, SH_FLATLIT);
        for(int i = 0; i < 4; i++) for(int j = -1; j <= 1; j++) cv_curve(cv, cx + j * 3.2f - 1, cy + 1.5f + i * 3, cx + j * 3.2f, cy + 3 + i * 3, cx + j * 3.2f + 1, cy + 1.5f + i * 3, 0.4f, 0.4f, &c->ac, SH_FLAT);
        float hy = cy - 9; cv_ell(cv, cx, hy, 9 * hs, 6.5f * hs, 0, &c->b, SH_SPHERE); for(int sd = -1; sd <= 1; sd += 2){ cv_tri(cv, cx + sd * 4, hy - 4 * hs, cx + sd * 8, hy - 2 * hs, cx + sd * 8.5f, hy - 10 * c->ek, &c->b, 0.85f, 0.4f); cv_curve(cv, cx + sd * 8.5f, hy - 10 * c->ek, cx + sd * 10, hy - 8, cx + sd * 9.5f, hy - 4, 0.4f, 1.1f, &c->bn, SH_FLAT); cv_dot(cv, cx + sd * 9.5f, hy - 3.6f, 1.2f, c->glow, 1); }
        cv_ell(cv, cx - 3.8f, hy, 3.6f * c->es, 3.6f * c->es, 0, &c->bl, SH_FLATLIT); cv_ell(cv, cx + 3.8f, hy, 3.6f * c->es, 3.6f * c->es, 0, &c->bl, SH_FLATLIT); c->front = 1; cr_eye(c, cx - 3.8f, hy, 2.0f); cr_eye(c, cx + 3.8f, hy, 2.0f); cv_tri(cv, cx - 1.3f, hy + 1.4f, cx + 1.3f, hy + 1.4f, cx, hy + 5, &c->bn, 0.95f, 0.6f); break; }
    default: { float cx = 32, cy = 31; c->bx = cx; c->by = cy; c->brx = 22; c->bry = 6;      // LOFTRAY: a flat sky ray over a puff of cloud
        cv_glow(cv, cx, cy + 14, 14, 0xFFFFFF, 0.28f); cv_ell(cv, cx - 8, cy + 16, 9, 3.4f, 0, &c->bl, SH_FLATLIT); cv_ell(cv, cx + 6, cy + 17, 10, 3.6f, 0, &c->bl, SH_FLATLIT);
        cv_curve(cv, cx, cy + 3, cx - 3, cy + 17, cx - 8, cy + 23, 1.2f, 0.3f, &c->dk, SH_FLAT); { float xy[10] = { cx - 28, cy + 2, cx - 10, cy - 8, cx + 6, cy - 9, cx + 28, cy + 1, cx, cy + 12 }; cv_poly(cv, xy, 5, &c->b, 0.95f, 0.35f); } { float xy[6] = { cx - 28, cy + 2, cx, cy + 12, cx + 28, cy + 1 }; (void)xy; }
        cv_ell(cv, cx, cy + 2, 7, 5, 0, &c->ac, SH_SPHERE); cv_curve(cv, cx - 26, cy + 2, cx - 14, cy + 7, cx - 2, cy + 11, 0.5f, 1.5f, &c->bl, SH_VERT); cv_curve(cv, cx + 26, cy + 1, cx + 14, cy + 6, cx + 2, cy + 11, 0.5f, 1.5f, &c->bl, SH_VERT); cr_eye(c, cx + 3, cy - 2, 1.4f); cr_eye(c, cx - 3, cy - 2, 1.4f); c->front = 1; for(int i = -2; i <= 2; i++) cv_dot(cv, cx + i * 3.2f, cy - 5.5f + (i & 1), 0.5f, c->dk.c[2], 0.9f); break; }
    }
}
