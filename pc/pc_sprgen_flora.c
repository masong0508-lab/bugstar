// pc_sprgen_flora.c: plants and trees from the flora catalogue (pc/content/flora.h): 32 growth forms x 16 leaf shapes x 16 flowers x 16 fruits x 12 barks x 16 colours
// (x geometry from the id's hash). Trees are L-systems (recursive branching with a spread, a ratio and a depth taken from the id), crowns are clumps of shaded
// discs with leaf marks of the leaf kind, flowers and fruits are small motifs of their own kind. See pc_sprgen.h.
#include "pc_sprgen_i.h"
#include "content/flora.h"
#include <stdio.h>

typedef struct { float x, y, r; } Tip;
typedef struct { SgCv cv; Flora f; SgRng r; SgRamp lf, lf2, bk, fl, fl2, fr; Tip tip[120]; int nt; uint32_t seed; int glow; int dry; float bx0, bx1, by0; } Pl;
#define CV (&p->cv)
static uint32_t c555(uint16_t c){ int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31; return sg_rgb(r << 3 | r >> 2, g << 3 | g >> 2, b << 3 | b >> 2); }
static const uint32_t bark_rgb[FL_NBARK] = { 0xA98C6B, 0x6B4A32, 0x6E6A4A, 0xE0DACB, 0x9A6B3C, 0xB4B9C2, 0x5C6B3F, 0x4F3328, 0x5A2E22, 0x7A7266, 0x8A6D4F, 0xC9B796 };
static const uint32_t fruit_rgb[FL_NFRUIT] = { 0xB02050, 0x7A4A28, 0x6E8A3A, 0xF2F2EA, 0xC83030, 0xE08A20, 0xF0A070, 0x7A5A3A, 0x5E9A3E, 0xD8B848, 0x6A3A6A, 0x8A8A3A, 0xE8E0B0, 0x60F0E0, 0x4AA860, 0 };

// ---- leaves (16 kinds): one leaf at (x,y) pointing at angle a, size s
static void leaf1(Pl *p, float x, float y, float a, float s, const SgRamp *rp, int kind){
    float ca = cosf(a), sa = sinf(a), ex = x + ca * s, ey = y + sa * s; SgCv *cv = CV;
    switch(kind){
    case 0: for(int i = -1; i <= 1; i++){ float b = a + i * 0.45f; cv_flat(cv, x, y, x + cosf(b) * s * 1.1f, y + sinf(b) * s * 1.1f, 0.55f, rp->c[3], 1); } cv_dot(cv, x, y, 0.5f, rp->c[1], 1); break;
    case 1: cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, s * 0.6f, s * 0.33f, a, rp, SH_SPHERE); cv_flat(cv, x, y, ex, ey, 0.3f, rp->c[0], 0.7f); break;
    case 2: cv_curve(cv, x, y, x + ca * s, y + sa * s - s * 0.2f, x + ca * s * 1.6f, y + sa * s + s * 0.9f, 0.55f, 0.15f, rp, SH_FLAT); break;
    case 3: for(int i = -2; i <= 2; i++){ float b = a + i * 0.4f; cv_tri(cv, x, y, x + cosf(b - 0.16f) * s * 1.2f, y + sinf(b - 0.16f) * s * 1.2f, x + cosf(b + 0.16f) * s * 1.2f, y + sinf(b + 0.16f) * s * 1.2f, rp, 0.75f, 0.35f); } break;
    case 4: { float nx = -sa * s * 0.28f, ny = ca * s * 0.28f; cv_ell(cv, x + ca * s * 0.4f + nx, y + sa * s * 0.4f + ny, s * 0.34f, s * 0.34f, 0, rp, SH_SPHERE); cv_ell(cv, x + ca * s * 0.4f - nx, y + sa * s * 0.4f - ny, s * 0.34f, s * 0.34f, 0, rp, SH_SPHERE);
        cv_tri(cv, x + ca * s * 0.4f + nx * 1.3f, y + sa * s * 0.4f + ny * 1.3f, x + ca * s * 0.4f - nx * 1.3f, y + sa * s * 0.4f - ny * 1.3f, x + ca * s * 0.05f, y + sa * s * 0.05f, rp, 0.6f, 0.45f); break; }
    case 5: for(int i = -1; i <= 1; i++){ float b = a + i * 0.7f, r = i ? 0.28f : 0.38f; cv_ell(cv, x + cosf(b) * s * 0.55f, y + sinf(b) * s * 0.55f, s * r * 1.3f, s * r, b, rp, SH_SPHERE); } break;
    case 6: for(int i = 0; i < 3; i++){ float t = 0.25f + i * 0.28f; cv_ell(cv, x + ca * s * t * 1.4f, y + sa * s * t * 1.4f, s * 0.22f, s * 0.3f, a + 1.57f, rp, SH_SPHERE); } break;
    case 7: cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, s * 0.55f, s * 0.3f, a, rp, SH_FLATLIT); cv_dot(cv, x + ca * s * 0.35f - sa * s * 0.1f, y + sa * s * 0.35f + ca * s * 0.1f, s * 0.1f, 0xFFFFFF, 0.9f); break;
    case 8: cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, s * 0.5f, s * 0.35f, a, rp, SH_SPHERE); for(int i = 0; i < 5; i++){ float b = a + i * 1.26f; cv_dot(cv, x + ca * s * 0.5f + cosf(b) * s * 0.5f, y + sa * s * 0.5f + sinf(b) * s * 0.36f, 0.4f, rp->c[4], 0.8f); } break;
    case 9: { float cx = x, cy = y, rr = s * 0.45f, ang = a; for(int i = 0; i < 5; i++){ float nx = cx + cosf(ang) * rr, ny = cy + sinf(ang) * rr; cv_cap(cv, cx, cy, 0.5f - i * 0.07f, nx, ny, 0.5f - i * 0.08f, rp, SH_FLAT); cx = nx; cy = ny; rr *= 0.8f; ang += 0.9f; } break; }
    case 10: cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, s * 0.62f, s * 0.42f, a, rp, SH_FLATLIT); cv_flat(cv, x, y, ex + ca * s * 0.1f, ey + sa * s * 0.1f, 0.25f, rp->c[1], 0.8f); break;
    case 11: for(int i = 0; i < 6; i++){ float b = a + (i - 2.5f) * 0.5f; cv_tri(cv, x, y, x + cosf(b - 0.1f) * s * 0.9f, y + sinf(b - 0.1f) * s * 0.9f, x + cosf(b + 0.1f) * s * 0.9f, y + sinf(b + 0.1f) * s * 0.9f, rp, 0.8f, 0.4f); } break;
    case 12: cv_curve(cv, x, y, x + ca * s * 0.6f, y + sa * s * 0.6f - s * 0.3f, x + ca * s * 1.3f, y + sa * s * 1.3f + s * 0.2f, 0.4f, 0.15f, rp, SH_FLAT);
        for(int i = 1; i < 5; i++){ float t = i / 5.0f, bx = x + ca * s * 1.3f * t, by = y + sa * s * 1.3f * t - s * 0.3f * sinf(t * 3.14f) * 0.6f; cv_cap(cv, bx, by, 0.3f, bx - sa * s * 0.35f, by + ca * s * 0.35f, 0.1f, rp, SH_FLAT); cv_cap(cv, bx, by, 0.3f, bx + sa * s * 0.35f, by - ca * s * 0.35f, 0.1f, rp, SH_FLAT); } break;
    case 13: cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f + s * 0.12f, s * 0.38f, s * 0.3f, 0, rp, SH_SPHERE); cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f - s * 0.05f, s * 0.38f, s * 0.12f, 0, rp, SH_FLATLIT); break;
    case 14: cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, s * 0.36f, s * 0.36f, 0, rp, SH_SPHERE); cv_ell(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, s * 0.2f, s * 0.2f, 0, rp, SH_VERT); break;
    default: for(int i = 0; i < 7; i++){ float b = i * 0.9f; cv_dot(cv, x + ca * s * 0.5f + cosf(b) * s * 0.34f, y + sa * s * 0.5f + sinf(b) * s * 0.34f, 0.45f, rp->c[3], 1); } cv_dot(cv, x + ca * s * 0.5f, y + sa * s * 0.5f, 0.4f, rp->c[1], 1); break;
    }
}
static const SgRamp *pickl(Pl *p, float lit){ return lit > 0.55f ? &p->lf2 : &p->lf; }
// a crown clump: a shaded mass with leaf marks of the plant's leaf kind on it
static void clump(Pl *p, float x, float y, float R, int marks){
    int lk = p->f.leaf;
    if(lk != 2 && lk != 0 && lk != 12) cv_ell(CV, x, y, R, R * 0.88f, 0, &p->lf, SH_SPHERE);
    else cv_ell(CV, x, y, R * 0.8f, R * 0.7f, 0, &p->lf, SH_SPHERE);
    for(int i = 0; i < marks; i++){ float a = sg_rr(&p->r, 0, SG_TAU), d = sqrtf(sg_rf(&p->r)) * R * 0.85f, lx = x + cosf(a) * d, ly = y + sinf(a) * d * 0.9f; float lit = 0.5f - (ly - y) / R * 0.4f + sg_rs(&p->r) * 0.15f;
        float ang = sg_rr(&p->r, -SG_PI, SG_PI); if(lk == 2) ang = 1.2f + sg_rs(&p->r) * 0.5f; leaf1(p, lx, ly, ang, R * (lk == 0 ? 0.55f : lk == 6 || lk == 14 ? 0.4f : 0.5f), pickl(p, lit), lk); }
}

// ---- flowers (16 kinds) and fruits (16 kinds), small, at (x,y) with size s
static void flower1(Pl *p, float x, float y, float s, int kind, const SgRamp *fr, uint32_t core){
    SgCv *cv = CV;
    switch(kind){
    case 0: cv_ell(cv, x, y + s * 0.3f, s * 0.7f, s * 0.9f, 0, fr, SH_SPHERE); cv_ell(cv, x, y + s * 1.1f, s * 0.8f, s * 0.22f, 0, fr, SH_VERT); break;               // bell
    case 1: { float xy[20]; for(int i = 0; i < 10; i++){ float r = (i & 1) ? s * 0.45f : s * 1.15f, a = -SG_PI / 2 + i * SG_TAU / 10; xy[i * 2] = x + cosf(a) * r; xy[i * 2 + 1] = y + sinf(a) * r; } cv_poly(cv, xy, 10, fr, 0.8f, 0.4f); cv_dot(cv, x, y, s * 0.25f, core, 1); break; }   // star
    case 2: cv_ell(cv, x, y + s * 0.2f, s * 0.9f, s * 0.8f, 0, fr, SH_SPHERE); cv_ell(cv, x, y - s * 0.3f, s * 0.9f, s * 0.35f, 0, fr, SH_FLATLIT); cv_dot(cv, x, y - s * 0.3f, s * 0.2f, core, 1); break;   // cup
    case 3: for(int i = 0; i < 5; i++) cv_dot(cv, x + (i & 1) * 0.3f, y - s * 0.4f * i + s * 0.8f, s * (0.5f - i * 0.05f), fr->c[2 + (i & 1)], 1); break;                                // spike
    case 4: for(int i = -2; i <= 2; i++) cv_cap(cv, x, y + s * 0.5f, s * 0.2f, x + i * s * 0.45f, y - s * (1.0f - fabsf(i) * 0.15f), s * 0.35f, fr, SH_FLAT); break;                       // plume
    case 5: for(int i = 0; i < 4; i++){ float a = i * SG_PI / 2; cv_ell(cv, x + cosf(a) * s * 0.6f, y + sinf(a) * s * 0.6f, s * 0.55f, s * 0.3f, a, fr, SH_SPHERE); } cv_dot(cv, x, y, s * 0.28f, core, 1); break;   // cross
    case 6: for(int i = 0; i < 8; i++){ float a = i * SG_TAU / 8; cv_ell(cv, x + cosf(a) * s * 0.7f, y + sinf(a) * s * 0.7f, s * 0.4f, s * 0.2f, a, fr, SH_SPHERE); } cv_dot(cv, x, y, s * 0.35f, core, 1); break;   // wheel
    case 7: for(int i = 0; i < 7; i++){ float a = i * 2.4f; cv_dot(cv, x + cosf(a) * s * 0.7f * (0.3f + i * 0.1f), y + sinf(a) * s * 0.7f * (0.3f + i * 0.1f), s * 0.25f, fr->c[1 + (i % 3)], 0.9f); } break;   // spores
    case 8: cv_dot(cv, x, y - s * 0.4f, s * 0.35f, fr->c[2], 1); for(int i = -2; i <= 2; i++) cv_cap(cv, x, y - s * 0.3f, s * 0.12f, x + i * s * 0.28f, y + s * 1.0f, s * 0.06f, fr, SH_FLAT); break;   // tassel
    case 9: cv_ell(cv, x, y, s * 0.8f, s * 0.8f, 0, fr, SH_SPHERE); cv_dot(cv, x - s * 0.25f, y - s * 0.28f, s * 0.2f, 0xFFFFFF, 0.5f); break;                                          // orb
    case 10: cv_tri(cv, x - s * 0.9f, y - s * 0.8f, x + s * 0.9f, y - s * 0.8f, x, y + s * 0.8f, fr, 0.8f, 0.4f); cv_ell(cv, x, y - s * 0.8f, s * 0.9f, s * 0.25f, 0, fr, SH_FLATLIT); cv_dot(cv, x, y - s * 0.8f, s * 0.2f, core, 1); break;   // trumpet
    case 11: cv_glow(cv, x, y, s * 1.8f, fr->c[4], 0.5f); cv_ell(cv, x, y, s * 0.55f, s * 0.8f, 0, fr, SH_SPHERE); cv_dot(cv, x, y + s * 0.1f, s * 0.3f, 0xFFF4C0, 0.9f); break;      // lantern
    case 12: cv_curve(cv, x - s, y + s * 0.3f, x - s * 0.2f, y - s * 1.1f, x + s * 0.4f, y, s * 0.35f, s * 0.35f, fr, SH_VERT); cv_curve(cv, x + s * 0.4f, y, x + s * 0.9f, y + s * 0.7f, x + s, y - s * 0.2f, s * 0.3f, s * 0.12f, fr, SH_VERT); break;   // ribbon
    case 13: for(int i = 0; i < 6; i++){ float a = i * SG_TAU / 6; cv_ell(cv, x + cosf(a) * s * 0.8f, y + sinf(a) * s * 0.8f, s * 0.45f, s * 0.25f, a, fr, SH_SPHERE); } for(int i = 0; i < 5; i++){ float a = 0.3f + i * SG_TAU / 5; cv_ell(cv, x + cosf(a) * s * 0.4f, y + sinf(a) * s * 0.4f, s * 0.3f, s * 0.17f, a, &p->fl2, SH_SPHERE); } cv_dot(cv, x, y, s * 0.2f, core, 1); break;   // double
    case 14: cv_ell(cv, x, y, s * 0.9f, s * 0.9f, 0, fr, SH_SPHERE); cv_ell(cv, x + s * 0.4f, y - s * 0.1f, s * 0.75f, s * 0.8f, 0, &p->lf, SH_FLATLIT); cv_ell(cv, x + s * 0.4f, y - s * 0.1f, s * 0.75f, s * 0.8f, 0, &p->lf, SH_FLATLIT); break;     // moon (crescent: a bite of the leaf colour)
    default: cv_cap(cv, x, y + s, s * 0.08f, x, y - s * 0.9f, s * 0.08f, fr, SH_FLAT); for(int i = 0; i < 6; i++){ float t = i / 6.0f; cv_cap(cv, x, y + s * 0.8f - t * s * 1.6f, s * 0.1f, x - s * (0.8f - t * 0.5f), y + s * 0.4f - t * s * 1.6f, s * 0.05f, fr, SH_FLAT); cv_cap(cv, x, y + s * 0.8f - t * s * 1.6f, s * 0.1f, x + s * (0.8f - t * 0.5f), y + s * 0.4f - t * s * 1.6f, s * 0.05f, fr, SH_FLAT); } break;   // feather
    }
}
static void fruit1(Pl *p, float x, float y, float s, int kind){
    SgCv *cv = CV; uint32_t base = fruit_rgb[kind & 15]; if(kind == 0) base = sg_mixc(base, p->f.flower_rgb ? c555(p->f.flower_rgb) : base, 0.35f); if(kind == 15 || !base) return; SgRamp r; sg_ramp(&r, base);
    switch(kind){
    case 0: for(int i = 0; i < 3; i++) cv_ell(cv, x + (i - 1) * s * 0.6f, y + (i & 1) * s * 0.4f, s * 0.38f, s * 0.38f, 0, &r, SH_SPHERE); break;
    case 1: cv_ell(cv, x, y, s * 0.5f, s * 0.65f, 0, &r, SH_SPHERE); cv_ell(cv, x, y - s * 0.45f, s * 0.5f, s * 0.25f, 0, &p->bk, SH_SPHERE); break;
    case 2: cv_curve(cv, x - s * 0.4f, y - s, x + s * 0.5f, y, x - s * 0.1f, y + s, s * 0.3f, s * 0.15f, &r, SH_SPHERE); break;
    case 3: cv_ell(cv, x, y, s * 0.7f, s * 0.7f, 0, &r, SH_FLATLIT); for(int i = 0; i < 5; i++){ float a = i * 1.26f; cv_dot(cv, x + cosf(a) * s * 0.8f, y + sinf(a) * s * 0.8f, s * 0.2f, 0xFFFFFF, 0.8f); } break;
    case 4: cv_ell(cv, x, y, s * 0.65f, s * 0.6f, 0, &r, SH_SPHERE); cv_flat(cv, x, y - s * 0.5f, x + s * 0.1f, y - s * 0.9f, s * 0.14f, p->bk.c[1], 1); break;
    case 5: cv_ell(cv, x, y + s * 0.3f, s * 0.7f, s * 0.6f, 0, &r, SH_SPHERE); cv_ell(cv, x, y - s * 0.35f, s * 0.35f, s * 0.4f, 0, &r, SH_SPHERE); break;
    case 6: cv_ell(cv, x, y, s * 0.6f, s * 0.6f, 0, &r, SH_SPHERE); cv_flat(cv, x, y - s * 0.55f, x, y + s * 0.5f, s * 0.08f, r.c[1], 0.8f); break;
    case 7: { cv_ell(cv, x, y, s * 0.45f, s * 0.8f, 0, &r, SH_SPHERE); for(int i = 0; i < 3; i++) cv_flat(cv, x - s * 0.4f, y - s * 0.4f + i * s * 0.4f, x + s * 0.4f, y - s * 0.1f + i * s * 0.4f, s * 0.1f, r.c[0], 0.9f); break; }
    case 8: cv_curve(cv, x - s * 0.3f, y - s * 0.8f, x + s * 0.5f, y, x, y + s * 0.8f, s * 0.2f, s * 0.12f, &r, SH_SPHERE); break;
    case 9: cv_cap(cv, x, y + s * 0.8f, s * 0.07f, x, y - s * 0.6f, s * 0.07f, &r, SH_FLAT); for(int i = 0; i < 5; i++) cv_ell(cv, x + (i & 1 ? s * 0.22f : -s * 0.22f), y - s * 0.5f + i * s * 0.22f, s * 0.14f, s * 0.26f, i & 1 ? 0.4f : -0.4f, &r, SH_FLAT); break;
    case 10: cv_ell(cv, x, y, s * 0.55f, s * 0.7f, 0, &r, SH_SPHERE); cv_dot(cv, x, y - s * 0.6f, s * 0.12f, r.c[0], 1); break;
    case 11: cv_ell(cv, x, y, s * 0.6f, s * 0.6f, 0, &r, SH_SPHERE); for(int i = 0; i < 7; i++){ float a = i * 0.9f; cv_tri(cv, x + cosf(a - 0.2f) * s * 0.55f, y + sinf(a - 0.2f) * s * 0.55f, x + cosf(a + 0.2f) * s * 0.55f, y + sinf(a + 0.2f) * s * 0.55f, x + cosf(a) * s * 0.95f, y + sinf(a) * s * 0.95f, &r, 0.5f, 0.3f); } break;
    case 12: for(int i = 0; i < 4; i++) cv_ell(cv, x + (i % 2 - 0.5f) * s * 0.7f, y + (i / 2 - 0.5f) * s * 0.7f, s * 0.28f, s * 0.28f, 0, &r, SH_SPHERE); break;
    case 13: cv_glow(cv, x, y, s * 1.7f, base, 0.55f); cv_ell(cv, x, y, s * 0.5f, s * 0.5f, 0, &r, SH_SPHERE); cv_dot(cv, x - s * 0.1f, y - s * 0.1f, s * 0.2f, 0xFFFFFF, 0.9f); break;
    default: cv_ell(cv, x, y, s * 0.9f, s * 0.75f, 0, &r, SH_SPHERE); for(int i = -1; i <= 1; i++) cv_flat(cv, x + i * s * 0.35f, y - s * 0.7f, x + i * s * 0.28f, y + s * 0.65f, s * 0.1f, r.c[0], 0.8f); break;
    }
}
static void blade(Pl *p, float x, float y, float a, float len, float w, float bend, const SgRamp *rp){
    float x1 = x + cosf(a) * len, y1 = y + sinf(a) * len, mx = (x + x1) / 2 + sinf(a) * bend, my = (y + y1) / 2 - cosf(a) * bend; cv_curve(CV, x, y, mx, my, x1 + sinf(a) * bend * 0.6f, y1 - cosf(a) * bend * 0.6f, w, w * 0.2f, rp, SH_FLAT);
}
static void tipc(Pl *p, float x, float y, float r){ if(p->nt < 120){ p->tip[p->nt].x = x; p->tip[p->nt].y = y; p->tip[p->nt].r = r; p->nt++; } }
static void branch(Pl *p, float x, float y, float a, float len, float w, int d, float spread, float ratio, int nch, float droop, float cr){
    float x1 = x + cosf(a) * len, y1 = y + sinf(a) * len; if(p->dry){ if(x1 - cr < p->bx0) p->bx0 = x1 - cr; if(x1 + cr > p->bx1) p->bx1 = x1 + cr; if(y1 - cr < p->by0) p->by0 = y1 - cr; } else cv_cap(CV, x, y, w * 0.5f, x1, y1, w * 0.34f, &p->bk, SH_SPHERE);
    if(d <= 0){ if(!p->dry) tipc(p, x1, y1, cr); return; }
    for(int i = 0; i < nch; i++){ float t = nch > 1 ? (i / (float)(nch - 1)) * 2 - 1 : 0; float da = spread * t + sg_rs(&p->r) * 0.18f; float na = a + da; if(droop) na += droop * (cosf(na) > 0 ? 1 : -1) * 0.25f;
        branch(p, x1, y1, na, len * ratio * sg_rr(&p->r, 0.88f, 1.12f), w * 0.66f, d - 1, spread, ratio, nch, droop, cr); }
    if(d >= 2 && nch == 2 && sg_rf(&p->r) < 0.5f) branch(p, x1 - cosf(a) * len * 0.3f, y1 - sinf(a) * len * 0.3f, a + (sg_rf(&p->r) < 0.5f ? 1 : -1) * 1.0f, len * ratio * 0.7f, w * 0.4f, d - 2, spread, ratio, nch, droop, cr);
}
static void crowns(Pl *p, float mk){
    for(int i = 1; i < p->nt; i++){ Tip t = p->tip[i]; int j = i - 1; while(j >= 0 && p->tip[j].y < t.y){ p->tip[j + 1] = p->tip[j]; j--; } p->tip[j + 1] = t; }
    for(int i = 0; i < p->nt; i++){ Tip *t = &p->tip[i]; clump(p, t->x, t->y, t->r, (int)(mk * (t->r / 5.0f)) + 2);
        if(p->f.flower != 7 && sg_rf(&p->r) < 0.55f) flower1(p, t->x + sg_rs(&p->r) * t->r * 0.6f, t->y + sg_rs(&p->r) * t->r * 0.5f, 1.5f, p->f.flower, &p->fl, 0xFFE070);
        if(p->f.fruit != 15 && sg_rf(&p->r) < 0.4f) fruit1(p, t->x + sg_rs(&p->r) * t->r * 0.7f, t->y + t->r * 0.35f, 1.7f, p->f.fruit); }
}
static void barkpat(Pl *p, float x0, float y0, float x1, float y1){
    uint32_t dk = p->bk.c[0], lt = p->bk.c[4]; SgCv *cv = CV;
    switch(p->f.bark){ case 1: cv_pattern(cv, 0, dk, 1.6f, 0.45f, p->seed, x0, y0, x1, y1); break; case 2: cv_pattern(cv, 5, dk, 1.6f, 0.4f, p->seed, x0, y0, x1, y1); break; case 3: cv_pattern(cv, 4, dk, 2.2f, 0.5f, p->seed, x0, y0, x1, y1); break;
        case 4: cv_pattern(cv, 3, dk, 1.2f, 0.5f, p->seed, x0, y0, x1, y1); break; case 6: cv_pattern(cv, 2, 0x4A8A3A, 3.0f, 0.7f, p->seed, x0, y0, x1, y1); break; case 9: cv_pattern(cv, 1, dk, 2.6f, 0.45f, p->seed, x0, y0, x1, y1); break;
        case 10: cv_pattern(cv, 4, lt, 3.0f, 0.4f, p->seed, x0, y0, x1, y1); break; case 11: cv_pattern(cv, 3, lt, 1.4f, 0.5f, p->seed, x0, y0, x1, y1); break; case 8: cv_pattern(cv, 0, lt, 5.0f, 0.3f, p->seed, x0, y0, x1, y1); break; default: break; }
}
static void thorns(Pl *p, float x, float y, float h, int n, float dir){ for(int i = 0; i < n; i++){ float yy = y - h * (i + 0.5f) / n, side = (i & 1) ? 1.0f : -1.0f; cv_tri(CV, x, yy - 0.5f, x, yy + 0.5f, x + side * 2.0f * dir, yy - 0.3f, &p->bk, 0.3f, 0.2f); } }
static void ground(Pl *p, float w, float a){ cv_shadow(CV, 32, 60.5f, w, 2.2f, a); }

// ---- the 32 forms
static void f_tree(Pl *p, int form){
    SgRng *r = &p->r; float cx = 32, by = 59; int lk = p->f.leaf;
    if(form == 24){                                                 // SPIRE: conifer tiers
        int tiers = 5 + sg_ri(r, 3); float w0 = sg_rr(r, 12, 17), h = sg_rr(r, 46, 52);
        cv_cap(CV, cx, by, 1.8f, cx, by - 12, 1.4f, &p->bk, SH_SPHERE); barkpat(p, 29, by - 14, 35, by);
        for(int i = 0; i < tiers; i++){ float t = (float)i / tiers, y1 = by - 6 - t * h * 0.88f, w = w0 * (1 - t * 0.85f) + 2, th = h / tiers * 1.7f;
            float xy[10]; xy[0] = cx - w; xy[1] = y1; xy[2] = cx - w * 0.4f; xy[3] = y1 - th * 0.15f; xy[4] = cx; xy[5] = y1 - th; xy[6] = cx + w * 0.4f; xy[7] = y1 - th * 0.15f; xy[8] = cx + w; xy[9] = y1;
            cv_poly(CV, xy, 5, &p->lf, 0.85f, 0.3f); for(int k = 0; k < 4; k++){ float lx = cx + sg_rs(r) * w * 0.8f, ly = y1 - sg_rf(r) * th * 0.5f; leaf1(p, lx, ly, sg_rr(r, 0.3f, 2.8f), 2.4f, &p->lf2, lk); } if(sg_rf(r) < 0.5f) fruit1(p, cx + sg_rs(r) * w * 0.6f, y1 + 0.5f, 1.6f, p->f.fruit); }
        cv_tri(CV, cx - 1.4f, by - h * 0.88f - 5, cx + 1.4f, by - h * 0.88f - 5, cx, by - h * 0.88f - 12, &p->lf2, 0.9f, 0.5f); return; }
    if(form == 23){                                                 // CANOPY: tall bare trunk, a flat umbrella of layered crowns
        float th = sg_rr(r, 30, 38), tw = sg_rr(r, 3.5f, 5.0f); cv_cap(CV, cx, by, tw * 0.6f, cx + sg_rs(r) * 2, by - th, tw * 0.4f, &p->bk, SH_SPHERE); barkpat(p, 26, by - th, 38, by);
        int nb = 4 + sg_ri(r, 3); for(int i = 0; i < nb; i++){ float a = -SG_PI / 2 + (i / (float)(nb - 1) * 2 - 1) * 1.15f, l = sg_rr(r, 14, 20); float x1 = cx + cosf(a) * l, y1 = by - th + sinf(a) * l * 0.55f + 4; cv_cap(CV, cx, by - th + 2, 1.4f, x1, y1, 0.8f, &p->bk, SH_SPHERE); tipc(p, x1, y1 - 2, sg_rr(r, 7, 9.5f)); }
        tipc(p, cx, by - th - 3, 9); crowns(p, 7); ground(p, 12, 0.3f); return; }
    if(form == 11){                                                 // PALM: a curved trunk and a ring of fronds
        float lean = sg_rs(r) * 9, th = sg_rr(r, 34, 42); float tx_ = cx + lean, ty_ = by - th; cv_curve(CV, cx, by, cx - lean * 0.3f, by - th * 0.55f, tx_, ty_, 2.4f, 1.5f, &p->bk, SH_SPHERE);
        for(int i = 0; i < 7; i++){ float t = i / 7.0f; cv_flat(CV, cx + lean * t * t * 1.0f - 2.2f, by - th * t, cx + lean * t * t * 1.0f + 2.2f, by - th * t, 0.6f, p->bk.c[0], 0.7f); }
        int nf = 7 + sg_ri(r, 3); for(int i = 0; i < nf; i++){ float a = -SG_PI + (i + 0.5f) * SG_PI / nf + sg_rs(r) * 0.1f; float l = sg_rr(r, 14, 20);
            float ex = tx_ + cosf(a) * l, ey = ty_ + sinf(a) * l * 0.5f + l * 0.38f * fabsf(cosf(a)), qx = tx_ + cosf(a) * l * 0.45f, qy = ty_ + sinf(a) * l * 0.9f - 2;
            cv_curve(CV, tx_, ty_, qx, qy, ex, ey, 0.9f, 0.3f, &p->lf, SH_FLAT);
            for(int k = 1; k < 9; k++){ float u = k / 9.0f, bx = (1 - u) * (1 - u) * tx_ + 2 * u * (1 - u) * qx + u * u * ex, by2 = (1 - u) * (1 - u) * ty_ + 2 * u * (1 - u) * qy + u * u * ey;
                float tdx = 2 * (1 - u) * (qx - tx_) + 2 * u * (ex - qx), tdy = 2 * (1 - u) * (qy - ty_) + 2 * u * (ey - qy), ta = atan2f(tdy, tdx), ll = 6.5f * (1 - u * 0.55f);
                for(int sd = -1; sd <= 1; sd += 2){ float la = ta + sd * 1.0f + 0.45f * (cosf(ta + sd * 1.0f) < 0 ? -1 : 1) * 0; cv_cap(CV, bx, by2, 0.75f, bx + cosf(la) * ll, by2 + sinf(la) * ll + ll * 0.35f, 0.15f, sd > 0 ? &p->lf2 : &p->lf, SH_FLAT); } } }
        if(p->f.fruit != 15) for(int i = 0; i < 3; i++) fruit1(p, tx_ + (i - 1) * 2.3f, ty_ + 3.2f, 2.0f, p->f.fruit); if(p->f.flower != 7) flower1(p, tx_, ty_ - 2, 2.2f, p->f.flower, &p->fl, 0xFFE070); ground(p, 10, 0.3f); return; }
    if(form == 31){                                                 // WILLOW: a short trunk, arches, hanging curtains
        cv_cap(CV, cx, by, 3.2f, cx + sg_rs(r) * 2, by - 20, 2.2f, &p->bk, SH_SPHERE); barkpat(p, 27, by - 22, 37, by);
        int nb = 4 + sg_ri(r, 2); for(int i = 0; i < nb; i++){ float side = (i & 1) ? 1 : -1, l = sg_rr(r, 12, 20), a = -SG_PI / 2 + side * sg_rr(r, 0.5f, 1.2f); float x1 = cx + cosf(a) * l, y1 = by - 20 + sinf(a) * l;
            cv_curve(CV, cx, by - 19, (cx + x1) / 2, y1 - 3, x1, y1 + 2, 1.4f, 0.7f, &p->bk, SH_SPHERE); tipc(p, x1, y1, 5.5f);
            for(int s2 = 0; s2 < 5; s2++){ float sx = x1 + (s2 - 2) * 2.0f, sl = sg_rr(r, 14, 26); cv_curve(CV, sx, y1 + 1, sx + side * 2, y1 + sl * 0.5f, sx + side * 1.0f, y1 + sl, 0.6f, 0.2f, &p->lf, SH_FLAT); leaf1(p, sx + side, y1 + sl * 0.8f, 1.6f, 3, &p->lf2, lk); } }
        crowns(p, 4); ground(p, 12, 0.3f); return; }
    if(form == 25){                                                 // STILT-TREE: prop roots on arches, a round crown
        int nr = 3 + sg_ri(r, 3); for(int i = 0; i < nr; i++){ float t = (i / (float)(nr - 1)) * 2 - 1, x1 = cx + t * 15; cv_curve(CV, cx + t * 2.5f, by - 24, cx + t * 9, by - 18, x1, by, 1.6f, 0.6f, &p->bk, SH_SPHERE); }
        cv_cap(CV, cx, by - 20, 2.6f, cx + sg_rs(r) * 2, by - 36, 2.2f, &p->bk, SH_SPHERE); barkpat(p, 28, by - 36, 36, by - 18);
        for(int i = 0; i < 6; i++){ float a = -SG_PI / 2 + (i / 5.0f * 2 - 1) * 1.1f, l = sg_rr(r, 9, 14); float x1 = cx + cosf(a) * l, y1 = by - 36 + sinf(a) * l * 0.7f; cv_cap(CV, cx, by - 35, 1.2f, x1, y1, 0.8f, &p->bk, SH_SPHERE); tipc(p, x1, y1 - 1, sg_rr(r, 5.5f, 8)); }
        crowns(p, 6); ground(p, 16, 0.25f); return; }
    // TREE (10) and the rest: a recursive branching
    { float th = sg_rr(r, 17, 24), tw = sg_rr(r, 3.6f, 5.4f), spread = sg_rr(r, 0.42f, 0.95f), ratio = sg_rr(r, 0.66f, 0.8f); int nch = sg_ri(r, 3) == 0 ? 3 : 2, depth = nch == 3 ? 3 : 4; float lean = sg_rs(r) * 0.12f, cr = sg_rr(r, 5.0f, 8.0f) * (nch == 3 ? 0.8f : 1);
        float droop = (p->f.leaf == 2 || p->f.leaf == 12) ? 0.6f : 0; cv_shadow(CV, 32, 60.5f, 13, 2.4f, 0.28f);
        { SgRng keep = p->r; p->dry = 1; p->bx0 = 99; p->bx1 = -99; p->by0 = 99; branch(p, cx, by, -SG_PI / 2 + lean, th, tw, depth, spread, ratio, nch, droop, cr); p->dry = 0; p->r = keep;
          float hw = fmaxf(cx - p->bx0, p->bx1 - cx), hh = by - p->by0, k = fminf(fminf(27.0f / hw, 56.0f / hh), 1.0f); if(k < 0.55f) k = 0.55f; th *= k; cr *= 0.5f + 0.5f * k; }
        branch(p, cx, by, -SG_PI / 2 + lean, th, tw, depth, spread, ratio, nch, droop, cr); barkpat(p, 16, 4, 48, by); if(p->f.bark == 7) thorns(p, cx + tw * 0.4f, by, th * 0.8f, 5, 1);
        crowns(p, 6); }
}
static void f_bush(Pl *p, int form){
    SgRng *r = &p->r; float cx = 32, by = 58; cv_shadow(CV, 32, 60.5f, 17, 2.4f, 0.28f);
    if(form == 9){                                                  // BRAMBLE: arching canes with thorns
        int n = 5 + sg_ri(r, 3); for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, x1 = cx + t * 22, h = sg_rr(r, 24, 42) * (1 - fabsf(t) * 0.35f); cv_curve(CV, cx + t * 3, by, cx + t * 6, by - h * 1.2f, x1, by - h * 0.5f, 1.3f, 0.7f, &p->bk, SH_SPHERE);
            for(int k = 0; k < 4; k++){ float u = (k + 1) / 5.0f, px = cx + t * 3 + (x1 - cx - t * 3) * u, py = by - h * (0.9f * sinf(u * 2.0f)) * 0.9f; clump(p, px, py, 4.0f, 3); if(sg_rf(r) < 0.4f) fruit1(p, px + 2, py + 2, 1.8f, p->f.fruit); } }
        return; }
    if(form == 22){                                                 // THORNBUSH: angular, spiky
        int n = 6 + sg_ri(r, 4); for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, x0 = cx, y0 = by, a = -SG_PI / 2 + t * 1.2f, l = sg_rr(r, 8, 15);
            for(int k = 0; k < 3; k++){ float na = a + sg_rs(r) * 0.7f, x1 = x0 + cosf(na) * l, y1 = y0 + sinf(na) * l * 0.9f; cv_cap(CV, x0, y0, 1.0f - k * 0.2f, x1, y1, 0.8f - k * 0.2f, &p->bk, SH_FLAT); cv_tri(CV, x1 - 0.5f, y1, x1 + 0.5f, y1, x1 + cosf(na + 1.2f) * 3, y1 + sinf(na + 1.2f) * 3, &p->bk, 0.2f, 0.2f); leaf1(p, x1, y1, na, 3.0f, &p->lf, p->f.leaf); x0 = x1; y0 = y1; l *= 0.8f; } }
        return; }
    int blobs = form == 7 ? 4 : 6; float W = form == 7 ? 14 : 18, H = form == 7 ? 13 : 20;
    for(int i = 0; i < blobs; i++){ float t = (i / (float)(blobs - 1)) * 2 - 1, x = cx + t * W * 0.8f + sg_rs(r) * 2, y = by - H * (0.35f + 0.65f * (1 - fabsf(t)) * sg_rr(r, 0.6f, 1.0f)), R = sg_rr(r, 6.5f, 9.5f) * (form == 7 ? 0.85f : 1.0f); tipc(p, x, y, R); }
    cv_cap(CV, cx, by, 1.2f, cx, by - 8, 1.0f, &p->bk, SH_SPHERE); crowns(p, 6);
}
static void f_cactus(Pl *p){
    SgRng *r = &p->r; float cx = 32, by = 58; int kind = sg_ri(r, 4); uint32_t g = p->lf.c[2]; SgRamp rp = p->lf; cv_shadow(CV, 32, 60.5f, 12, 2.2f, 0.3f);
    if(kind == 0){ float h = sg_rr(r, 36, 46), w = 4.8f; cv_cap(CV, cx, by, w, cx, by - h, w * 0.85f, &rp, SH_SPHERE); int na = 1 + sg_ri(r, 3); for(int i = 0; i < na; i++){ float s = (i & 1) ? 1 : -1, yy = by - h * (0.35f + 0.18f * i), ah = sg_rr(r, 12, 18); cv_cap(CV, cx, yy, 2.2f, cx + s * 11, yy, 2.4f, &rp, SH_SPHERE); cv_cap(CV, cx + s * 11, yy, 2.4f, cx + s * 11, yy - ah, 2.0f, &rp, SH_SPHERE); } }
    else if(kind == 1){ float rr = sg_rr(r, 13, 17); cv_ell(CV, cx, by - rr * 0.9f, rr * 0.9f, rr, 0, &rp, SH_SPHERE); for(int i = -2; i <= 2; i++) cv_curve(CV, cx + i * rr * 0.34f, by - 1, cx + i * rr * 0.5f, by - rr, cx + i * rr * 0.28f, by - rr * 1.9f + 1, 0.4f, 0.25f, &p->lf, SH_FLAT); }
    else if(kind == 2){ for(int i = 0; i < 3; i++){ float x = cx + (i - 1) * 10, h = sg_rr(r, 18, 36); cv_cap(CV, x, by, 4.0f, x + sg_rs(r) * 1.5f, by - h, 3.4f, &rp, SH_SPHERE); } }
    else { float x = cx, y = by - 6; for(int i = 0; i < 4; i++){ float rx = 9 - i * 1.2f, ry = 7 - i * 0.7f; cv_ell(CV, x, y, rx, ry, sg_rs(r) * 0.4f, &rp, SH_SPHERE); x += sg_rs(r) * 7; y -= ry * 1.5f; } }
    cv_pattern(CV, 6, 0xFFF4C0, 1.5f, 0.9f, p->seed, 4, 4, 60, 60); if(p->f.flower != 7){ flower1(p, cx, by - (kind == 0 ? 46 : kind == 1 ? 33 : kind == 2 ? 30 : 36), 3.0f, p->f.flower, &p->fl, 0xFFE070); }
    (void)g;
}
static void f_succ(Pl *p){ SgRng *r = &p->r; float cx = 32, by = 52; int n = 12 + sg_ri(r, 8); cv_shadow(CV, 32, 56.5f, 14, 2.2f, 0.28f); for(int ring = 0; ring < 3; ring++) for(int i = 0; i < n - ring * 3; i++){ float a = i * SG_TAU / (n - ring * 3) + ring * 0.4f, l = (14 - ring * 4) + sg_rs(r); float ex = cx + cosf(a) * l, ey = by - 4 - ring * 2 + sinf(a) * l * 0.4f - (1 - fabsf(cosf(a))) * (4 + ring * 4); float pw = 3.6f - ring * 0.5f; cv_ell(CV, (cx + ex) / 2, (by + ey) / 2 - 1 - ring * 2, sg_len(ex - cx, ey - by) * 0.55f, pw, atan2f(ey - by, ex - cx), ring == 0 ? &p->lf : &p->lf2, SH_SPHERE); }
    if(p->f.flower != 7) flower1(p, cx, by - 12, 3, p->f.flower, &p->fl, 0xFFE070); }
static void f_toad(Pl *p){ SgRng *r = &p->r; int n = 1 + sg_ri(r, 4); cv_shadow(CV, 32, 60.5f, 15, 2.2f, 0.3f); uint32_t cap = p->f.flower_rgb ? c555(p->f.flower_rgb) : 0xCC3333; if(p->f.flower == 7 || 1) cap = sg_mixc(c555(p->f.leaf_rgb), cap, 0.65f); SgRamp cr, stem; sg_ramp(&cr, cap); sg_ramp(&stem, 0xE8DCC0);
    for(int i = n - 1; i >= 0; i--){ float x = 32 + (i - (n - 1) / 2.0f) * 15 + sg_rs(r) * 2, h = sg_rr(r, 16, 34) * (1 - i * 0.12f), R = sg_rr(r, 8, 13); if(n == 1){ h = sg_rr(r, 24, 36); R = sg_rr(r, 11, 15); }
        if(p->f.quirk == 0) cv_glow(CV, x, 58 - h, R * 1.8f, cr.c[4], 0.5f); cv_cap(CV, x, 59, 2.8f, x + sg_rs(r), 58 - h, 2.1f, &stem, SH_SPHERE); cv_ell(CV, x, 58 - h, R, R * 0.62f, 0, &cr, SH_SPHERE);
        cv_ell(CV, x, 58 - h + R * 0.2f, R * 0.95f, R * 0.18f, 0, &stem, SH_FLATLIT); cv_pattern(CV, 1, 0xF4EEDD, 1.2f + R * 0.1f, 0.85f, p->seed + i, x - R, 58 - h - R * 0.7f, x + R, 58 - h + R * 0.1f); } }
static void f_fern(Pl *p){ SgRng *r = &p->r; int n = 7 + sg_ri(r, 4); cv_shadow(CV, 32, 60.5f, 15, 2.2f, 0.28f); for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, a = -SG_PI / 2 + t * 1.25f, l = sg_rr(r, 22, 34) * (1 - fabsf(t) * 0.25f);
        float x1 = 32 + cosf(a) * l, y1 = 58 + sinf(a) * l, mx = 32 + cosf(a) * l * 0.5f, my = 58 + sinf(a) * l * 0.5f - 4; cv_curve(CV, 32, 58, mx, my, x1 + t * 4, y1 + 5 + fabsf(t) * 5, 0.8f, 0.25f, &p->bk, SH_FLAT);
        for(int k = 1; k < 9; k++){ float u = k / 9.0f, px = (1 - u) * (1 - u) * 32 + 2 * u * (1 - u) * mx + u * u * (x1 + t * 4), py = (1 - u) * (1 - u) * 58 + 2 * u * (1 - u) * my + u * u * (y1 + 5 + fabsf(t) * 5), ll = 5.0f * (1 - u * 0.7f);
            cv_cap(CV, px, py, 0.55f, px - ll * 0.9f, py + ll * 0.4f, 0.15f, &p->lf, SH_FLAT); cv_cap(CV, px, py, 0.55f, px + ll * 0.9f, py + ll * 0.4f, 0.15f, &p->lf2, SH_FLAT); } } }
static void f_grass(Pl *p, int form){
    SgRng *r = &p->r; int n = form == 2 ? 16 : form == 21 ? 10 : form == 3 ? 9 : form == 28 ? 14 : 8; float h0 = form == 3 ? 40 : form == 2 ? 30 : form == 28 ? 34 : form == 21 ? 20 : 26; cv_shadow(CV, 32, 60.5f, 12, 2.0f, 0.25f);
    for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, a = -SG_PI / 2 + t * (form == 3 ? 0.35f : 0.8f) + sg_rs(r) * 0.12f, l = sg_rr(r, h0 * 0.6f, h0), bend = sg_rs(r) * (form == 28 ? 9 : 5); float x0 = 32 + t * 7, ex = x0 + cosf(a) * l, ey = 59 + sinf(a) * l;
        blade(p, x0, 59, a, l, form == 3 ? 1.0f : 0.95f, bend, i & 1 ? &p->lf : &p->lf2);
        if((form == 3 || form == 2) && (i % 3 == 0)){ if(form == 3){ cv_ell(CV, ex + bend * 0.3f, ey, 1.5f, 3.6f, 0, &p->bk, SH_SPHERE); } else for(int k = 0; k < 4; k++) cv_ell(CV, ex + bend * 0.3f + (k & 1 ? 0.8f : -0.8f), ey + k * 1.4f, 0.7f, 1.2f, 0.3f, &p->fl, SH_FLAT); }
        if(form == 21 && i == n / 2 && p->f.flower != 7) flower1(p, ex, ey - 2, 2.2f, p->f.flower, &p->fl, 0xFFE070); }
}
static void f_herb(Pl *p, int form){
    SgRng *r = &p->r; int n = form == 19 ? 5 : 4 + sg_ri(r, 3); cv_shadow(CV, 32, 60.5f, 11, 2.0f, 0.25f); if(form == 19) cv_ell(CV, 32, 56, 5.5f, 5.0f, 0, &p->bk, SH_SPHERE);
    for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, a = -SG_PI / 2 + t * 0.9f, l = form == 19 ? sg_rr(r, 24, 34) : sg_rr(r, 20, 34); float x0 = 32 + t * 3, ex = x0 + cosf(a) * l, ey = 58 + sinf(a) * l;
        if(form == 19){ blade(p, 32 + t * 4, 56, a, l, 1.5f, sg_rs(r) * 4, i & 1 ? &p->lf : &p->lf2); } else { cv_cap(CV, x0, 58, 0.7f, ex, ey, 0.5f, &p->lf, SH_FLAT); for(int k = 1; k < 4; k++){ float u = k / 4.0f; leaf1(p, x0 + (ex - x0) * u, 58 + (ey - 58) * u, a + (k & 1 ? 1.1f : -1.1f), 5.0f * (1 - u * 0.4f), &p->lf2, p->f.leaf); } leaf1(p, ex, ey, a, 4, &p->lf2, p->f.leaf);
            if(p->f.flower != 7) flower1(p, ex, ey - 1.5f, 2.3f, p->f.flower, &p->fl, 0xFFE070); if(p->f.fruit != 15 && (i & 1)) fruit1(p, x0 + (ex - x0) * 0.4f + 2, 58 + (ey - 58) * 0.4f, 1.6f, p->f.fruit); } }
    if(form == 19 && p->f.flower != 7) flower1(p, 32, 26 + sg_ri(r, 6), 5.5f, p->f.flower, &p->fl, 0xFFE070);
}
static void f_mat(Pl *p, int form){ SgRng *r = &p->r; cv_shadow(CV, 32, 56.5f, 20, 2.2f, 0.28f); if(form == 1){ cv_ell(CV, 32, 52, 15, 7, 0, &p->bk, SH_SPHERE); cv_ell(CV, 32, 55, 19, 3.2f, 0, &p->bk, SH_VERT); }
    int n = form == 0 ? 11 : 9; for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, x = 32 + t * 18 + sg_rs(r) * 2, R = sg_rr(r, 5, 9) * (1 - fabsf(t) * 0.3f); cv_ell(CV, x, 54 - (1 - fabsf(t)) * 4 + sg_rs(r) * 1.5f - (form == 1 ? 4 : 0), R, R * 0.7f, 0, form == 1 ? &p->lf2 : &p->lf, SH_SPHERE); }
    cv_pattern(CV, form == 1 ? 2 : 3, p->lf.c[4], 1.6f, 0.7f, p->seed, 4, 36, 60, 60);
    if(form == 20){ for(int i = 0; i < 6; i++){ float x = 32 + sg_rs(r) * 18, y = 48 + sg_rs(r) * 5; if(p->f.flower != 7) flower1(p, x, y, 1.7f, p->f.flower, &p->fl, 0xFFE070); } } }
static void f_vine(Pl *p, int form){
    SgRng *r = &p->r; cv_shadow(CV, 32, 60.5f, 11, 2.0f, 0.25f);
    if(form == 5){ cv_cap(CV, 32, 60, 2.0f, 32, 6, 1.7f, &p->bk, SH_SPHERE); cv_tri(CV, 24, 8, 40, 8, 32, 3, &p->bk, 0.4f, 0.2f); float prev = 32, py = 58; for(int i = 0; i < 12; i++){ float y = 58 - i * 4.3f, x = 32 + sinf(i * 1.15f + p->seed) * 8.5f; cv_curve(CV, prev, py, (prev + x) / 2 + 3, (py + y) / 2, x, y, 0.8f, 0.8f, &p->lf, SH_SPHERE); leaf1(p, x, y, (i & 1) ? 0 : SG_PI, 6.0f, i & 1 ? &p->lf2 : &p->lf, p->f.leaf); if(i % 3 == 1 && p->f.flower != 7) flower1(p, x + (i & 1 ? 4 : -4), y + 1, 2.1f, p->f.flower, &p->fl, 0xFFE070); if(i % 4 == 2 && p->f.fruit != 15) fruit1(p, x + (i & 1 ? -3 : 3), y + 3, 1.8f, p->f.fruit); prev = x; py = y; } return; }
    if(form == 6){ for(int k = 0; k < 4; k++){ float y = 59 - k * 1.5f, x = 32, dir = (k & 1) ? 1 : -1; for(int i = 0; i < 8; i++){ float nx = x + dir * sg_rr(r, 3, 5), ny = y + sinf(i * 1.3f + k) * 3 - 1.2f; cv_cap(CV, x, y, 0.7f, nx, ny, 0.7f, &p->lf, SH_FLAT); leaf1(p, nx, ny, (i & 1 ? -1 : 1) * 1.2f - 1.57f, 5, i & 1 ? &p->lf2 : &p->lf, p->f.leaf); if(i == 3 && p->f.flower != 7) flower1(p, nx, ny - 3, 2.0f, p->f.flower, &p->fl, 0xFFE070); x = nx; y = ny; } } return; }
    // CLIMBER (29): a weathered stone post with a woody vine winding up it
    { SgRamp rock; sg_ramp(&rock, 0x8A8680); float xy[10] = { 26, 60, 27.5f, 22, 32, 14, 36.5f, 22, 38, 60 }; cv_poly(CV, xy, 5, &rock, 0.8f, 0.3f); cv_pattern(CV, 5, rock.c[0], 4.0f, 0.4f, p->seed, 20, 10, 44, 60); float px = 32, py = 59;
      for(int i = 0; i < 14; i++){ float y = 59 - i * 3.3f, x = 32 + sinf(i * 0.95f + p->seed) * 9, rr = 1.5f - i * 0.04f; cv_curve(CV, px, py, (px + x) / 2 + 2, (py + y) / 2, x, y, rr, rr * 0.95f, &p->lf, SH_SPHERE); for(int k = 0; k < 2; k++) leaf1(p, x, y, k ? 0.4f : 2.7f, 6, k ? &p->lf2 : &p->lf, p->f.leaf); if(i % 3 == 1 && p->f.flower != 7) flower1(p, x + 2, y - 2, 2.3f, p->f.flower, &p->fl, 0xFFE070); px = x; py = y; } }
}
static void f_water(Pl *p, int form){
    SgRng *r = &p->r; SgRamp wat; sg_ramp(&wat, 0x3A7AB0);
    if(form == 15){ cv_ell(CV, 32, 52, 29, 9, 0, &wat, SH_VERT); cv_ell(CV, 32, 49, 27, 6, 0, &wat, SH_FLATLIT); for(int i = 0; i < 4; i++){ float x = 14 + i * 12 + sg_rs(r) * 3, y = 48 + (i & 1) * 6, R = sg_rr(r, 7, 10); cv_ell(CV, x, y, R, R * 0.38f, 0, &p->lf, SH_SPHERE); cv_tri(CV, x, y, x + R, y - 1, x + R, y + 1, &wat, 0.8f, 0.8f); if(i == 1 && p->f.flower != 7) flower1(p, x, y - 4, 5, p->f.flower, &p->fl, 0xFFE070); } return; }
    // FLOATER (26): tethered balloon bladders
    cv_shadow(CV, 32, 60.5f, 9, 1.8f, 0.2f); for(int i = 0; i < 3; i++){ float x = 32 + (i - 1) * 14 + sg_rs(r) * 3, y = 18 + sg_rr(r, 0, 12) + (i == 1 ? -6 : 0), R = sg_rr(r, 8, 12); cv_curve(CV, 32 + (i - 1) * 4, 60, x, 40, x, y + R, 0.4f, 0.3f, &p->bk, SH_FLAT); cv_ell(CV, x, y, R, R * 1.1f, 0, &p->lf2, SH_SPHERE); cv_dot(CV, x - R * 0.35f, y - R * 0.4f, R * 0.25f, 0xFFFFFF, 0.7f); cv_pattern(CV, 0, p->lf.c[1], 3.5f, 0.3f, p->seed + i, x - R, y - R, x + R, y + R); }
}
static void f_orch(Pl *p){ SgRng *r = &p->r; cv_shadow(CV, 32, 60.5f, 11, 2.0f, 0.25f); for(int i = 0; i < 4; i++){ float a = -SG_PI / 2 + (i - 1.5f) * 0.55f; blade(p, 32 + (i - 1.5f) * 2, 59, a, sg_rr(r, 16, 22), 2.2f, sg_rs(r) * 5, i & 1 ? &p->lf : &p->lf2); }
    float px = 31, py = 56; int nb = 5 + sg_ri(r, 3); for(int i = 0; i < nb; i++){ float t = (i + 1.0f) / nb, x = 31 + t * 15 + sinf(t * 3) * 2, y = 56 - sinf(t * 2.2f) * 40; cv_cap(CV, px, py, 0.6f, x, y, 0.5f, &p->lf, SH_FLAT); if(p->f.flower != 7) flower1(p, x + 2.5f, y + 2, 3.0f - t * 0.8f, p->f.flower, &p->fl, 0xFFE070); px = x; py = y; } }
static void f_kelp(Pl *p, int form){
    SgRng *r = &p->r; 
    int n = form == 28 ? 9 : 5 + sg_ri(r, 3); { SgRamp sand; sg_ramp(&sand, 0xC8B488); cv_ell(CV, 32, 60, 23, 4, 0, &sand, SH_SPHERE); }
    for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, x0 = 32 + t * 16, h = form == 28 ? sg_rr(r, 20, 36) : sg_rr(r, 38, 58), sw = sg_rs(r) * 9; float px = x0, py = 60;
        for(int k = 1; k <= 8; k++){ float u = k / 8.0f, x = x0 + sinf(u * 4 + i + p->seed) * 4 + sw * u, y = 60 - h * u; cv_cap(CV, px, py, form == 28 ? 0.7f : 1.4f * (1 - u * 0.5f), x, y, form == 28 ? 0.5f : 1.2f * (1 - u * 0.5f), k & 1 ? &p->lf : &p->lf2, SH_SPHERE); if(form != 28){ leaf1(p, x, y, k & 1 ? 0 : SG_PI, 7, &p->lf2, 2); if(k % 3 == 0) cv_ell(CV, x + (k & 1 ? 2 : -2), y - 1, 1.5f, 1.8f, 0, &p->lf2, SH_SPHERE); } px = x; py = y; } }
}
static void f_bamboo(Pl *p){ SgRng *r = &p->r; cv_shadow(CV, 32, 60.5f, 13, 2.0f, 0.25f); int n = 3 + sg_ri(r, 3); for(int i = 0; i < n; i++){ float x = 32 + (i - (n - 1) / 2.0f) * 8 + sg_rs(r) * 1.5f, h = sg_rr(r, 40, 56), w = sg_rr(r, 2.0f, 2.8f); int seg = 5 + sg_ri(r, 3); float lean = sg_rs(r) * 4;
        for(int k = 0; k < seg; k++){ float y0 = 60 - h * k / seg, y1 = 60 - h * (k + 1) / seg, xa = x + lean * k / seg, xb = x + lean * (k + 1) / seg; cv_cap(CV, xa, y0, w, xb, y1 + 0.4f, w, &p->lf, SH_SPHERE); cv_ell(CV, xb, y1, w * 1.15f, 0.7f, 0, &p->lf2, SH_FLATLIT); if(k >= 1 && k % 2 == 1) for(int s2 = 0; s2 < 2; s2++){ float a = s2 ? -0.35f : -SG_PI + 0.35f; leaf1(p, xb, y1, a, 9, &p->lf2, 1); } } } }
static void f_lichen(Pl *p){ SgRng *r = &p->r; cv_shadow(CV, 32, 60.5f, 18, 2.2f, 0.3f); SgRamp rock; sg_ramp(&rock, 0x7C7A78); cv_ell(CV, 32, 50, 17, 11, 0, &rock, SH_SPHERE); cv_ell(CV, 20, 55, 9, 6, 0, &rock, SH_SPHERE); cv_ell(CV, 44, 56, 8, 5, 0, &rock, SH_SPHERE);
    for(int i = 0; i < 10; i++){ float x = 32 + sg_rs(r) * 14, y = 48 + sg_rs(r) * 8; cv_ell(CV, x, y, sg_rr(r, 2.5f, 5), sg_rr(r, 1.8f, 3.5f), sg_rs(r), i & 1 ? &p->lf : &p->lf2, SH_SPHERE); cv_dot(CV, x + 1, y, 0.8f, p->lf.c[4], 1); } }
static void f_glasswort(Pl *p){ SgRng *r = &p->r; cv_shadow(CV, 32, 60.5f, 12, 2.0f, 0.25f); int n = 4 + sg_ri(r, 3); for(int i = 0; i < n; i++){ float t = (i / (float)(n - 1)) * 2 - 1, x = 32 + t * 12, y = 59, a = -SG_PI / 2 + t * 0.4f; for(int k = 0; k < 7; k++){ float l = 5.2f - k * 0.35f, nx = x + cosf(a) * l, ny = y + sinf(a) * l; cv_cap(CV, x, y, 1.8f - k * 0.1f, nx, ny, 1.7f - k * 0.1f, k & 1 ? &p->lf : &p->lf2, SH_SPHERE); cv_ell(CV, nx, ny, 2.0f, 0.5f, a + 1.57f, &p->lf2, SH_FLATLIT);
            if(k > 1 && k < 6 && (k & 1)){ float b = a + (i & 1 ? 0.9f : -0.9f); cv_cap(CV, nx, ny, 1.0f, nx + cosf(b) * 4, ny + sinf(b) * 4, 0.9f, &p->lf, SH_SPHERE); } x = nx; y = ny; a += sg_rs(r) * 0.25f; } } }

static void quirks(Pl *p){
    SgRng *r = &p->r; SgCv *cv = CV; (void)r;
    switch(p->f.quirk){
    case 0: for(int i = 0; i < 5; i++) cv_glow(cv, 14 + (i * 37 + p->seed) % 36, 20 + (i * 53 + p->seed / 3) % 30, 6, p->lf.c[4], 0.18f); break;                       // glows at night
    case 10: for(int i = 0; i < 7; i++){ float x = 12 + sg_rf(&p->r) * 40, y = 8 + sg_rf(&p->r) * 48; cv_flat(cv, x - 1, y, x + 1, y, 0.4f, 0xFFFFFF, 0.9f); cv_flat(cv, x, y - 1, x, y + 1, 0.4f, 0xFFFFFF, 0.9f); } break;   // sparkles in frost
    case 1: case 14: cv_text(cv, 46, 6, "~", 1.0f, 0xFFFFFF); cv_dot(cv, 50, 9, 1.0f, 0xFFFFFF, 0.8f); cv_flat(cv, 51, 9, 51, 3, 0.5f, 0xFFFFFF, 0.8f); break;       // hums, sings (a note)
    case 6: for(int i = 0; i < 3; i++) cv_dot(cv, 12 + sg_rf(&p->r) * 40, 14 + sg_rf(&p->r) * 30, 0.7f, 0x303030, 1); break;                                                    // draws bugs
    case 11: cv_flat(cv, 50, 8, 50, 14, 1.2f, 0x60E080, 0.9f); cv_flat(cv, 47, 11, 53, 11, 1.2f, 0x60E080, 0.9f); break;                                                     // heals
    case 12: for(int i = 0; i < 5; i++) cv_dot(cv, 16 + sg_rf(&p->r) * 32, 20 + sg_rf(&p->r) * 34, 0.9f, 0x7A2A8A, 0.55f); break;                                            // poison
    case 7: cv_glow(cv, 32, 32, 30, 0xFFA060, 0.10f); break;                                                                                                                  // warms the air
    case 15: for(int i = 0; i < 3; i++) leaf1(p, 10 + i * 18, 56 - i * 2, 1.0f, 3, &p->lf2, 1); break;                                                                          // drops its leaves
    default: break; }
}
void sg_flora(SgImg *im, uint32_t id, uint32_t var){
    Pl *p = (Pl *)calloc(1, sizeof *p); if(!p) return; flora_make(id, &p->f); p->seed = (uint32_t)(sg_m64(id * 0x9E3779B97F4A7C15ull + var) >> 24); p->r = sg_rng(id, var + 11);
    cv_init(CV, im, 64); CV->post = 6; uint32_t lc = c555(p->f.leaf_rgb), fc = c555(p->f.flower_rgb);
    { float h, s, v; sg_tohsv(lc, &h, &s, &v); s = sg_clampf(s * 0.78f, 0, 1); v = sg_clampf(v * 0.88f + 0.1f, 0, 1); lc = sg_hsv(h, s, v); }                 // (a little softer than the raw catalogue colour)
    sg_ramp(&p->lf, lc); { float h, s, v; sg_tohsv(lc, &h, &s, &v); sg_ramp(&p->lf2, sg_hsv(h + 0.03f, s * 0.9f, sg_clampf(v * 1.15f + 0.04f, 0, 1))); }
    if(p->f.quirk == 8){ float h, s, v; sg_tohsv(lc, &h, &s, &v); sg_ramp(&p->lf2, sg_hsv(h + 0.28f, s, v)); }                                                 // changes colour by day
    sg_ramp(&p->fl, fc); { float h, s, v; sg_tohsv(fc, &h, &s, &v); sg_ramp(&p->fl2, sg_hsv(h + 0.5f, s, v)); }
    sg_ramp(&p->bk, bark_rgb[p->f.bark % FL_NBARK]); int form = p->f.form;
    switch(form){
    case 10: case 23: case 24: case 11: case 31: case 25: f_tree(p, form); break;
    case 7: case 8: case 9: case 22: f_bush(p, form); break;
    case 12: f_cactus(p); break; case 13: f_succ(p); break; case 14: f_toad(p); break;
    case 4: f_fern(p); break; case 2: case 3: case 21: f_grass(p, form); break;
    case 17: f_orch(p); break; case 18: case 19: f_herb(p, form); break;
    case 0: case 1: case 20: if(form == 1) f_lichen(p); else f_mat(p, form); break;
    case 5: case 6: case 29: f_vine(p, form); break;
    case 15: case 28: f_kelp(p, form); break; case 16: f_water(p, 15); break; case 26: f_water(p, 26); break;
    case 27: f_glasswort(p); break; case 30: f_bamboo(p); break;
    default: f_herb(p, 18); break; }
    quirks(p);
    cv_outline(CV, sg_scale(p->lf.c[0], 0.55f), 0.75f);
    free(p);
}
int sg_flora_tree(uint32_t id){ static const uint8_t tf[8] = { 10, 23, 24, 11, 31, 25, 10, 24 }; uint32_t h = (uint32_t)(sg_m64(id + 0x7733) >> 20); uint32_t giv = (uint32_t)(sg_m64(id * 31 + 5) >> 30) & 0x3FFFF, adj = h & 63; return (int)(adj | (uint32_t)tf[id & 7] << 6 | giv << 11); }
void sg_flora_label(uint32_t id, char *out, int n){ Flora f; flora_make(id, &f); snprintf(out, n, "%s", f.name); }
