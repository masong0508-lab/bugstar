// pc_sprgen.c: the core of the procedural sprite generators (see pc_sprgen.h): images, colour ramps, noise, a canvas with anti-aliased shapes,
// patterns, contours, the 3x5 letters, scaled drawing into a window picture and the cache of finished sprites. No game code in here.
#include "pc_sprgen_i.h"
#include <stdio.h>

// ---------------------------------------------------------------- images
SgImg *sg_img_new(int w, int h){ SgImg *im = (SgImg *)malloc(sizeof *im); if(!im) return 0; im->w = w; im->h = h; im->px = (uint32_t *)calloc((size_t)w * h, 4); if(!im->px){ free(im); return 0; } return im; }
void sg_img_free(SgImg *im){ if(!im) return; free(im->px); free(im); }
void sg_img_clear(SgImg *im){ memset(im->px, 0, (size_t)im->w * im->h * 4); }
uint64_t sg_img_hash(const SgImg *im){ uint64_t h = 1469598103934665603ull; h ^= (uint64_t)im->w << 16 | (uint64_t)im->h; h *= 1099511628211ull;
    for(long i = 0, n = (long)im->w * im->h; i < n; i++){ h ^= im->px[i]; h *= 1099511628211ull; h ^= h >> 29; } return h; }
int sg_img_diff(const SgImg *a, const SgImg *b){
    if(a->w != b->w || a->h != b->h) return 1000; long n = (long)a->w * a->h, tot = 0;
    for(long i = 0; i < n; i++){ uint32_t p = a->px[i], q = b->px[i]; int da = (int)(p >> 24) - (int)(q >> 24); if(da < 0) da = -da;
        int dr = (int)(p >> 16 & 255) - (int)(q >> 16 & 255), dg = (int)(p >> 8 & 255) - (int)(q >> 8 & 255), db = (int)(p & 255) - (int)(q & 255); if(dr < 0) dr = -dr; if(dg < 0) dg = -dg; if(db < 0) db = -db;
        int wgt = ((int)(p >> 24) + (int)(q >> 24)) > 0 ? 1 : 0; tot += da * 2 + (wgt ? (dr + dg + db) / 3 : 0); }
    return (int)(tot * 1000L / (n * 255L * 2));
}
int sg_img_cover(const SgImg *im){ int c = 0; for(long i = 0, n = (long)im->w * im->h; i < n; i++) if((im->px[i] >> 24) >= 128) c++; return c; }

// ---------------------------------------------------------------- colour
uint32_t sg_gray(float v){ int g = (int)(sg_clampf(v, 0, 1) * 255 + 0.5f); return sg_rgb(g, g, g); }
void sg_tohsv(uint32_t c, float *h, float *s, float *v){
    float r = (c >> 16 & 255) / 255.0f, g = (c >> 8 & 255) / 255.0f, b = (c & 255) / 255.0f;
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b), d = mx - mn;
    *v = mx; *s = mx > 0 ? d / mx : 0; float hh = 0;
    if(d > 1e-6f){ if(mx == r) hh = (g - b) / d + (g < b ? 6 : 0); else if(mx == g) hh = (b - r) / d + 2; else hh = (r - g) / d + 4; hh /= 6; }
    *h = hh;
}
uint32_t sg_hsv(float h, float s, float v){
    h = h - floorf(h); s = sg_clampf(s, 0, 1); v = sg_clampf(v, 0, 1);
    float hh = h * 6; int i = (int)hh; float f = hh - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f)), r, g, b;
    switch(i % 6){ case 0: r = v; g = t; b = p; break; case 1: r = q; g = v; b = p; break; case 2: r = p; g = v; b = t; break; case 3: r = p; g = q; b = v; break; case 4: r = t; g = p; b = v; break; default: r = v; g = p; b = q; }
    return sg_rgb((int)(r * 255 + 0.5f), (int)(g * 255 + 0.5f), (int)(b * 255 + 0.5f));
}
static float hue_toward(float h, float target, float amt){ float d = target - h; d -= floorf(d + 0.5f); return h + d * amt; }
void sg_ramp(SgRamp *r, uint32_t base){
    float h, s, v; sg_tohsv(base, &h, &s, &v);
    r->c[0] = sg_hsv(hue_toward(h, 0.72f, 0.14f), sg_clampf(s * 1.10f + 0.04f, 0, 1), v * 0.33f);
    r->c[1] = sg_hsv(hue_toward(h, 0.72f, 0.07f), sg_clampf(s * 1.06f, 0, 1), v * 0.60f);
    r->c[2] = base;
    r->c[3] = sg_hsv(hue_toward(h, 0.12f, 0.05f), s * 0.92f, v * 1.10f + 0.05f);
    r->c[4] = sg_hsv(hue_toward(h, 0.12f, 0.09f), s * 0.62f, v * 1.20f + 0.14f);
}
uint32_t sg_rampc(const SgRamp *r, float lit, int post){
    lit = sg_clampf(lit, 0, 1); float p;
    if(post > 1){ int q = (int)(lit * post); if(q >= post) q = post - 1; p = q * (4.0f / (post - 1)); } else p = lit * 4;
    int i = (int)p; if(i > 3) i = 3; return sg_mixc(r->c[i], r->c[i + 1], p - i);
}

// ---------------------------------------------------------------- noise
static inline float lat(int x, int y, uint32_t seed){ uint32_t h = (uint32_t)x * 0x9E3779B1u ^ (uint32_t)y * 0x85EBCA77u ^ seed * 0xC2B2AE3Du; h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15; return (h & 0xFFFFFF) * (1.0f / 16777216.0f); }
float sg_vnoise(float x, float y, uint32_t seed){
    float fx = floorf(x), fy = floorf(y); int ix = (int)fx, iy = (int)fy; float u = x - fx, v = y - fy; u = u * u * (3 - 2 * u); v = v * v * (3 - 2 * v);
    return sg_lerp(sg_lerp(lat(ix, iy, seed), lat(ix + 1, iy, seed), u), sg_lerp(lat(ix, iy + 1, seed), lat(ix + 1, iy + 1, seed), u), v);
}
float sg_fbm(float x, float y, uint32_t seed, int oct){ float a = 0.5f, t = 0, n = 0; for(int i = 0; i < oct; i++){ t += a * sg_vnoise(x, y, seed + i * 101); n += a; x *= 2.03f; y *= 2.03f; a *= 0.5f; } return t / n; }
float sg_wnoise(float x, float y, uint32_t seed){
    int ix = (int)floorf(x), iy = (int)floorf(y); float best = 9;
    for(int j = -1; j <= 1; j++) for(int i = -1; i <= 1; i++){ int cx = ix + i, cy = iy + j; float px = cx + lat(cx, cy, seed), py = cy + lat(cx, cy, seed + 77); float d = sg_len(x - px, y - py); if(d < best) best = d; }
    return best;
}

// ---------------------------------------------------------------- the canvas
void cv_init(SgCv *cv, SgImg *im, float design){ cv->im = im; cv->alpha = 1; cv->snap = 0; cv->s = im->w / design; cv->k = 1; cv->ox = cv->oy = 0; cv->post = 5; float lx = -0.45f, ly = -0.55f, lz = 0.70f, n = sqrtf(lx * lx + ly * ly + lz * lz); cv->lx = lx / n; cv->ly = ly / n; cv->lz = lz / n; }
void cv_view(SgCv *cv, float ox, float oy, float k){ cv->ox = ox; cv->oy = oy; cv->k = k; }
void cv_snap(SgCv *cv){ cv_unsnap(cv); size_t n = (size_t)cv->im->w * cv->im->h; cv->snap = (uint32_t *)malloc(n * 4); if(cv->snap) memcpy(cv->snap, cv->im->px, n * 4); }
void cv_unsnap(SgCv *cv){ free(cv->snap); cv->snap = 0; }
static inline float tx(const SgCv *cv, float x){ return (x * cv->k + cv->ox) * cv->s; }
static inline float ty(const SgCv *cv, float y){ return (y * cv->k + cv->oy) * cv->s; }
static inline void plot(SgImg *im, int x, int y, float cov, uint32_t col, float alpha){
    if((unsigned)x >= (unsigned)im->w || (unsigned)y >= (unsigned)im->h) return;
    float ca = cov * alpha; if(ca <= 0.003f) return; if(ca > 1) ca = 1;
    uint32_t d = im->px[(size_t)y * im->w + x]; float da = (d >> 24) / 255.0f, oa = ca + da * (1 - ca);
    float wd = da * (1 - ca) / oa, wc = ca / oa;
    int r = (int)((col >> 16 & 255) * wc + (d >> 16 & 255) * wd + 0.5f), g = (int)((col >> 8 & 255) * wc + (d >> 8 & 255) * wd + 0.5f), b = (int)((col & 255) * wc + (d & 255) * wd + 0.5f);
    int a = (int)(oa * 255 + 0.5f); if(a > 255) a = 255; im->px[(size_t)y * im->w + x] = (uint32_t)a << 24 | (uint32_t)sg_rgb(r, g, b);
}
static inline float shade_n(const SgCv *cv, float nx, float ny){ float q = nx * nx + ny * ny; if(q > 1){ float k = 1 / sqrtf(q); nx *= k; ny *= k; q = 1; } float nz = sqrtf(1 - q); float d = nx * cv->lx + ny * cv->ly + nz * cv->lz; return d * 0.62f + 0.40f; }

void cv_ell(SgCv *cv, float cx, float cy, float rx, float ry, float rot, const SgRamp *rp, int sh){
    float k = cv->k * cv->s; cx = tx(cv, cx); cy = ty(cv, cy); rx *= k; ry *= k; if(rx < 0.3f) rx = 0.3f; if(ry < 0.3f) ry = 0.3f;
    float ca = cosf(rot), sa = sinf(rot), R = (rx > ry ? rx : ry) + 1.5f, mn = rx < ry ? rx : ry; SgImg *im = cv->im;
    int x0 = (int)floorf(cx - R), x1 = (int)ceilf(cx + R), y0 = (int)floorf(cy - R), y1 = (int)ceilf(cy + R); if(x0 < 0) x0 = 0; if(y0 < 0) y0 = 0; if(x1 > im->w) x1 = im->w; if(y1 > im->h) y1 = im->h;
    for(int y = y0; y < y1; y++) for(int x = x0; x < x1; x++){
        float dx = x + 0.5f - cx, dy = y + 0.5f - cy, u = (dx * ca + dy * sa) / rx, v = (-dx * sa + dy * ca) / ry, q = sqrtf(u * u + v * v);
        float cov = 0.5f - (q - 1) * mn; if(cov <= 0) continue; if(cov > 1) cov = 1; float lit;
        switch(sh){ case SH_SPHERE: { float sx = u * ca - v * sa, sy = u * sa + v * ca; lit = shade_n(cv, sx, sy); break; }
            case SH_VERT: lit = 0.88f - 0.62f * (dy / R * 0.5f + 0.5f); break; case SH_HORZ: lit = 0.88f - 0.62f * (dx / R * 0.5f + 0.5f); break; case SH_FLATLIT: lit = 0.6f; break; default: lit = 0.5f; }
        plot(im, x, y, cov, sg_rampc(rp, lit, cv->post), cv->alpha);
    }
}
void cv_cap(SgCv *cv, float x0f, float y0f, float r0, float x1f, float y1f, float r1, const SgRamp *rp, int sh){
    float k = cv->k * cv->s; float ax = tx(cv, x0f), ay = ty(cv, y0f), bx = tx(cv, x1f), by = ty(cv, y1f); r0 *= k; r1 *= k; if(r0 < 0.25f) r0 = 0.25f; if(r1 < 0.25f) r1 = 0.25f;
    float R = (r0 > r1 ? r0 : r1) + 1.5f, dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy; SgImg *im = cv->im;
    int xa = (int)floorf((ax < bx ? ax : bx) - R), xb = (int)ceilf((ax < bx ? bx : ax) + R), ya = (int)floorf((ay < by ? ay : by) - R), yb = (int)ceilf((ay < by ? by : ay) + R);
    if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){
        float px = x + 0.5f - ax, py = y + 0.5f - ay, t = l2 > 1e-6f ? (px * dx + py * dy) / l2 : 0; t = sg_clampf(t, 0, 1);
        float ex = px - t * dx, ey = py - t * dy, d = sqrtf(ex * ex + ey * ey), r = r0 + (r1 - r0) * t; float cov = 0.5f - (d - r); if(cov <= 0) continue; if(cov > 1) cov = 1;
        float lit; if(sh == SH_FLAT) lit = 0.5f; else if(sh == SH_VERT) lit = 0.88f - 0.62f * (sg_clampf(ey / r, -1, 1) * 0.5f + 0.5f); else if(sh == SH_FLATLIT) lit = 0.6f; else lit = shade_n(cv, ex / r, ey / r);
        plot(im, x, y, cov, sg_rampc(rp, lit, cv->post), cv->alpha);
    }
}
void cv_tri(SgCv *cv, float x0, float y0, float x1, float y1, float x2, float y2, const SgRamp *rp, float lit0, float lit1){
    float X[3] = { tx(cv, x0), tx(cv, x1), tx(cv, x2) }, Y[3] = { ty(cv, y0), ty(cv, y1), ty(cv, y2) }; SgImg *im = cv->im;
    float area = (X[1] - X[0]) * (Y[2] - Y[0]) - (X[2] - X[0]) * (Y[1] - Y[0]); if(fabsf(area) < 1e-4f) return; float sg = area > 0 ? 1.0f : -1.0f;
    float mnx = X[0], mxx = X[0], mny = Y[0], mxy = Y[0]; for(int i = 1; i < 3; i++){ if(X[i] < mnx) mnx = X[i]; if(X[i] > mxx) mxx = X[i]; if(Y[i] < mny) mny = Y[i]; if(Y[i] > mxy) mxy = Y[i]; }
    int xa = (int)floorf(mnx - 1), xb = (int)ceilf(mxx + 1), ya = (int)floorf(mny - 1), yb = (int)ceilf(mxy + 1); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    float el[3], nx[3], ny[3]; for(int i = 0; i < 3; i++){ int j = (i + 1) % 3; float ex = X[j] - X[i], ey = Y[j] - Y[i]; el[i] = sqrtf(ex * ex + ey * ey); if(el[i] < 1e-5f) el[i] = 1e-5f; nx[i] = -ey / el[i] * sg; ny[i] = ex / el[i] * sg; }
    float hh = mxy - mny; if(hh < 1) hh = 1;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){
        float px = x + 0.5f, py = y + 0.5f, d = 1e9f; for(int i = 0; i < 3; i++){ float e = (px - X[i]) * nx[i] + (py - Y[i]) * ny[i]; if(e < d) d = e; }
        float cov = 0.5f + d; if(cov <= 0) continue; if(cov > 1) cov = 1; float t = (py - mny) / hh; plot(im, x, y, cov, sg_rampc(rp, lit0 + (lit1 - lit0) * t, cv->post), cv->alpha);
    }
}
void cv_poly(SgCv *cv, const float *xy, int n, const SgRamp *rp, float lit0, float lit1){
    if(n < 3) return; float cx = 0, cy = 0; for(int i = 0; i < n; i++){ cx += xy[i * 2]; cy += xy[i * 2 + 1]; } cx /= n; cy /= n;
    float X[64], Y[64]; if(n > 62) n = 62; float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
    for(int i = 0; i < n; i++){ X[i] = tx(cv, xy[i * 2]); Y[i] = ty(cv, xy[i * 2 + 1]); if(X[i] < mnx) mnx = X[i]; if(X[i] > mxx) mxx = X[i]; if(Y[i] < mny) mny = Y[i]; if(Y[i] > mxy) mxy = Y[i]; }
    float CX = tx(cv, cx), CY = ty(cv, cy); SgImg *im = cv->im;
    int xa = (int)floorf(mnx - 1), xb = (int)ceilf(mxx + 1), ya = (int)floorf(mny - 1), yb = (int)ceilf(mxy + 1); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    float hh = mxy - mny; if(hh < 1) hh = 1;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){
        float px = x + 0.5f, py = y + 0.5f, tot = 0;
        for(int i = 0; i < n; i++){ int j = (i + 1) % n; float ax = X[i], ay = Y[i], bx = X[j], by = Y[j];
            float area = (bx - ax) * (CY - ay) - (CX - ax) * (by - ay); if(fabsf(area) < 1e-4f) continue; float sg = area > 0 ? 1.0f : -1.0f;
            float xs[3] = { ax, bx, CX }, ys[3] = { ay, by, CY }, d = 1e9f;
            for(int e = 0; e < 3; e++){ int f = (e + 1) % 3; float ex = xs[f] - xs[e], ey = ys[f] - ys[e], l = sqrtf(ex * ex + ey * ey); if(l < 1e-5f){ d = -1; break; } float v = ((px - xs[e]) * (-ey) + (py - ys[e]) * ex) / l * sg; if(v < d) d = v; }
            float c = 0.5f + d; if(c > 0) tot += c > 1 ? 1 : c; }
        if(tot <= 0) continue; if(tot > 1) tot = 1; plot(im, x, y, tot, sg_rampc(rp, lit0 + (lit1 - lit0) * ((py - mny) / hh), cv->post), cv->alpha);
    }
}
void cv_rect(SgCv *cv, float x0, float y0, float x1, float y1, const SgRamp *rp, float lit0, float lit1){
    float X0 = tx(cv, x0), X1 = tx(cv, x1), Y0 = ty(cv, y0), Y1 = ty(cv, y1); if(X1 < X0){ float t = X0; X0 = X1; X1 = t; } if(Y1 < Y0){ float t = Y0; Y0 = Y1; Y1 = t; }
    SgImg *im = cv->im; int xa = (int)floorf(X0), xb = (int)ceilf(X1), ya = (int)floorf(Y0), yb = (int)ceilf(Y1); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    float hh = Y1 - Y0; if(hh < 1e-3f) hh = 1e-3f;
    for(int y = ya; y < yb; y++){ float cy = (y + 1 < Y1 ? y + 1 : Y1) - (y > Y0 ? y : Y0); if(cy <= 0) continue; uint32_t col = sg_rampc(rp, lit0 + (lit1 - lit0) * sg_clampf((y + 0.5f - Y0) / hh, 0, 1), cv->post);
        for(int x = xa; x < xb; x++){ float cx = (x + 1 < X1 ? x + 1 : X1) - (x > X0 ? x : X0); if(cx <= 0) continue; plot(im, x, y, cx * cy, col, cv->alpha); } }
}
void cv_rrect(SgCv *cv, float x0, float y0, float x1, float y1, float r, const SgRamp *rp, int sh){
    float k = cv->k * cv->s; float X0 = tx(cv, x0), X1 = tx(cv, x1), Y0 = ty(cv, y0), Y1 = ty(cv, y1), R = r * k; SgImg *im = cv->im;
    float hw = (X1 - X0) / 2, hh = (Y1 - Y0) / 2, cx = (X0 + X1) / 2, cy = (Y0 + Y1) / 2; if(R > hw) R = hw; if(R > hh) R = hh;
    int xa = (int)floorf(X0 - 1), xb = (int)ceilf(X1 + 1), ya = (int)floorf(Y0 - 1), yb = (int)ceilf(Y1 + 1); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){
        float px = fabsf(x + 0.5f - cx) - (hw - R), py = fabsf(y + 0.5f - cy) - (hh - R), ox = px > 0 ? px : 0, oy = py > 0 ? py : 0, d = sqrtf(ox * ox + oy * oy) + (px > py ? (px < 0 ? px : 0) : (py < 0 ? py : 0)) - R;
        float cov = 0.5f - d; if(cov <= 0) continue; if(cov > 1) cov = 1; float lit;
        if(sh == SH_SPHERE) lit = shade_n(cv, (x + 0.5f - cx) / (hw + 0.01f), (y + 0.5f - cy) / (hh + 0.01f)); else if(sh == SH_VERT) lit = 0.88f - 0.62f * ((y + 0.5f - Y0) / (2 * hh)); else if(sh == SH_HORZ) lit = 0.88f - 0.62f * ((x + 0.5f - X0) / (2 * hw)); else lit = 0.5f;
        plot(im, x, y, cov, sg_rampc(rp, lit, cv->post), cv->alpha);
    }
}
void cv_curve(SgCv *cv, float x0, float y0, float cx, float cy, float x1, float y1, float r0, float r1, const SgRamp *rp, int sh){
    float len = sg_len(cx - x0, cy - y0) + sg_len(x1 - cx, y1 - cy); int n = (int)(len * cv->k * cv->s / 2.0f) + 3; if(n > 40) n = 40;
    float px = x0, py = y0, pr = r0;
    for(int i = 1; i <= n; i++){ float t = (float)i / n, u = 1 - t; float x = u * u * x0 + 2 * u * t * cx + t * t * x1, y = u * u * y0 + 2 * u * t * cy + t * t * y1, r = r0 + (r1 - r0) * t; cv_cap(cv, px, py, pr, x, y, r, rp, sh); px = x; py = y; pr = r; }
}
void cv_glow(SgCv *cv, float x, float y, float r, uint32_t col, float alpha){
    float k = cv->k * cv->s; float cx = tx(cv, x), cy = ty(cv, y), R = r * k; SgImg *im = cv->im; if(R < 0.5f) return;
    int xa = (int)floorf(cx - R), xb = (int)ceilf(cx + R), ya = (int)floorf(cy - R), yb = (int)ceilf(cy + R); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int yy = ya; yy < yb; yy++) for(int xx = xa; xx < xb; xx++){ float d = sg_len(xx + 0.5f - cx, yy + 0.5f - cy) / R; if(d >= 1) continue; float a = (1 - d); a = a * a * (3 - 2 * a); plot(im, xx, yy, a, col, alpha * cv->alpha); }
}
void cv_dot(SgCv *cv, float x, float y, float r, uint32_t col, float alpha){
    float k = cv->k * cv->s; float cx = tx(cv, x), cy = ty(cv, y), R = r * k; if(R < 0.35f) R = 0.35f; SgImg *im = cv->im;
    int xa = (int)floorf(cx - R - 1), xb = (int)ceilf(cx + R + 1), ya = (int)floorf(cy - R - 1), yb = (int)ceilf(cy + R + 1); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int yy = ya; yy < yb; yy++) for(int xx = xa; xx < xb; xx++){ float d = sg_len(xx + 0.5f - cx, yy + 0.5f - cy); float c = 0.5f - (d - R); if(c <= 0) continue; plot(im, xx, yy, c > 1 ? 1 : c, col, alpha * cv->alpha); }
}
void cv_flat(SgCv *cv, float x0, float y0, float x1, float y1, float w, uint32_t col, float alpha){
    float k = cv->k * cv->s; float ax = tx(cv, x0), ay = ty(cv, y0), bx = tx(cv, x1), by = ty(cv, y1), R = w * k * 0.5f; if(R < 0.3f) R = 0.3f; SgImg *im = cv->im;
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy; int xa = (int)floorf((ax < bx ? ax : bx) - R - 1), xb = (int)ceilf((ax < bx ? bx : ax) + R + 1), ya = (int)floorf((ay < by ? ay : by) - R - 1), yb = (int)ceilf((ay < by ? by : ay) + R + 1);
    if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){ float px = x + 0.5f - ax, py = y + 0.5f - ay, t = l2 > 1e-6f ? sg_clampf((px * dx + py * dy) / l2, 0, 1) : 0; float ex = px - t * dx, ey = py - t * dy; float c = 0.5f - (sqrtf(ex * ex + ey * ey) - R); if(c > 0) plot(im, x, y, c > 1 ? 1 : c, col, alpha * cv->alpha); }
}
// ---- the 3x5 letters (the game's own font, ASCII 32..95)
const uint16_t sg_font35[64] = { 0,9346,23040,24445,0,21157,0,9216,10530,8778,21973,1488,20,448,2,4772,31599,11415,29671,29647,23497,31183,31215,29257,31727,31695,1040,0,5393,3640,17492,29378,
    0,31725,27566,31015,27502,31143,31140,31087,23533,29847,4719,23469,18727,24557,27501,31599,31716,31609,27565,31183,29842,23407,23402,23421,23213,23506,29351,0,0,0,10752,0 };
float cv_textw(const char *s, float px){ int n = (int)strlen(s); return n ? (n * 4 - 1) * px : 0; }
static void fillrect(SgCv *cv, float x0, float y0, float x1, float y1, uint32_t col, float alpha){
    float X0 = tx(cv, x0), X1 = tx(cv, x1), Y0 = ty(cv, y0), Y1 = ty(cv, y1); SgImg *im = cv->im; int xa = (int)floorf(X0), xb = (int)ceilf(X1), ya = (int)floorf(Y0), yb = (int)ceilf(Y1);
    if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int y = ya; y < yb; y++){ float cy = (y + 1 < Y1 ? y + 1 : Y1) - (y > Y0 ? y : Y0); if(cy <= 0) continue; for(int x = xa; x < xb; x++){ float cx = (x + 1 < X1 ? x + 1 : X1) - (x > X0 ? x : X0); if(cx > 0) plot(im, x, y, cx * cy, col, alpha * cv->alpha); } }
}
void cv_text(SgCv *cv, float x, float y, const char *s, float px, uint32_t col){
    for(; *s; s++, x += 4 * px){ int ch = (unsigned char)*s; if(ch >= 'a' && ch <= 'z') ch -= 32; if(ch < 32 || ch > 95) continue; uint16_t g = sg_font35[ch - 32]; if(!g) continue;
        for(int r = 0; r < 5; r++){ int bits = (g >> (12 - r * 3)) & 7; for(int c = 0; c < 3; c++) if(bits & (4 >> c)) fillrect(cv, x + c * px, y + r * px, x + (c + 1) * px, y + (r + 1) * px, col, 1); } }
}
// ---- patterns, contour, mirror, shadow
void cv_pattern(SgCv *cv, int kind, uint32_t col, float scale, float amt, uint32_t seed, float x0, float y0, float x1, float y1){
    SgImg *im = cv->im; float s = cv->s * cv->k; if(scale < 0.2f) scale = 0.2f;
    int xa = (int)floorf(tx(cv, x0)), xb = (int)ceilf(tx(cv, x1)), ya = (int)floorf(ty(cv, y0)), yb = (int)ceilf(ty(cv, y1)); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){
        uint32_t d = im->px[(size_t)y * im->w + x]; if((d >> 24) < 100) continue; if(cv->snap && cv->snap[(size_t)y * im->w + x] == d) continue; float u = ((x + 0.5f) / cv->s - cv->ox) / cv->k, v = ((y + 0.5f) / cv->s - cv->oy) / cv->k, f = 0; (void)s;
        switch(kind){
        case 0: { float t = (u * 0.80f + v * 0.60f) / scale + (sg_vnoise(u * 0.3f, v * 0.3f, seed) - 0.5f) * 0.9f; t -= floorf(t); f = sg_smooth((0.5f - fabsf(t - 0.35f)) * 14.0f * scale / 2.0f + 0.5f) * (t < 0.62f ? 1 : 0); break; }
        case 1: { float dd = sg_wnoise(u / scale, v / scale, seed); f = sg_smooth((0.30f - dd) * 6.0f * scale + 0.5f); break; }
        case 2: f = sg_smooth((sg_fbm(u / scale, v / scale, seed, 3) - 0.55f) * 9.0f); break;
        case 3: f = lat((int)floorf(u * 3.0f / scale), (int)floorf(v * 3.0f / scale), seed) < 0.2f ? 1.0f : 0.0f; break;
        case 4: { float t = v / scale; t -= floorf(t); f = t < 0.45f ? 1.0f : 0.0f; break; }
        case 5: { float uu = u / scale, vv = v / scale; int row = (int)floorf(vv); uu += (row & 1) * 0.5f; float fx = uu - floorf(uu) - 0.5f, fy = vv - floorf(vv); float dd = sg_len(fx, fy - 0.55f); f = sg_smooth((dd - 0.46f) * 8 + 0.5f) * (dd < 0.62f ? 1.0f : 0.0f); break; }
        case 6: { float dd = sg_wnoise(u / scale, v / scale, seed); f = dd < 0.12f ? 1.0f : 0.0f; break; }
        default: { int c = ((int)floorf(u / scale) + (int)floorf(v / scale)) & 1; f = (float)c; }
        }
        if(f > 0.01f){ float a = f * amt; uint32_t c = sg_mixc(d & 0xFFFFFF, col, a); im->px[(size_t)y * im->w + x] = (d & 0xFF000000u) | c; }
    }
}
void cv_outline(SgCv *cv, uint32_t col, float strength){
    SgImg *im = cv->im; int w = im->w, h = im->h, t = (int)(w / 64.0f + 0.4f); if(t < 1) t = 1; if(t > 3) t = 3; uint8_t *al = (uint8_t *)malloc((size_t)w * h); if(!al) return;
    for(long i = 0, n = (long)w * h; i < n; i++) al[i] = (uint8_t)(im->px[i] >> 24);
    for(int y = 0; y < h; y++) for(int x = 0; x < w; x++){
        int a0 = al[y * w + x]; if(a0 >= 250) continue; int mx = 0;
        for(int dy = -t; dy <= t; dy++) for(int dx = -t; dx <= t; dx++){ if(!dx && !dy) continue; if(dx * dx + dy * dy > t * t + 1) continue; int xx = x + dx, yy = y + dy; if(xx < 0 || yy < 0 || xx >= w || yy >= h) continue; int a = al[yy * w + xx]; if(a > mx) mx = a; }
        if(mx < 128) continue; float oa = strength * (mx / 255.0f) * (1 - a0 / 255.0f); if(oa <= 0.01f) continue;
        uint32_t d = im->px[y * w + x]; float ea = (d >> 24) / 255.0f, ra = ea + oa * (1 - ea); float we = ea / ra, wo = oa * (1 - ea) / ra;
        int r = (int)((d >> 16 & 255) * we + (col >> 16 & 255) * wo + 0.5f), g = (int)((d >> 8 & 255) * we + (col >> 8 & 255) * wo + 0.5f), b = (int)((d & 255) * we + (col & 255) * wo + 0.5f);
        im->px[y * w + x] = (uint32_t)((int)(ra * 255 + 0.5f)) << 24 | sg_rgb(r, g, b);
    }
    free(al);
}
void cv_mirror(SgCv *cv){ SgImg *im = cv->im; for(int y = 0; y < im->h; y++) for(int x = 0; x < im->w / 2; x++) im->px[(size_t)y * im->w + (im->w - 1 - x)] = im->px[(size_t)y * im->w + x]; }
void cv_shadow(SgCv *cv, float cx, float cy, float rx, float ry, float alpha){
    float k = cv->k * cv->s; float X = tx(cv, cx), Y = ty(cv, cy), RX = rx * k, RY = ry * k; SgImg *im = cv->im; if(RX < 1 || RY < 0.5f) return;
    int xa = (int)floorf(X - RX), xb = (int)ceilf(X + RX), ya = (int)floorf(Y - RY), yb = (int)ceilf(Y + RY); if(xa < 0) xa = 0; if(ya < 0) ya = 0; if(xb > im->w) xb = im->w; if(yb > im->h) yb = im->h;
    for(int y = ya; y < yb; y++) for(int x = xa; x < xb; x++){ float d = sg_len((x + 0.5f - X) / RX, (y + 0.5f - Y) / RY); if(d >= 1) continue; float a = 1 - d; plot(im, x, y, a * a * (3 - 2 * a), 0x000000, alpha * cv->alpha); }
}
void cv_fade(SgCv *cv, float y0, float y1, float a0, float a1){
    SgImg *im = cv->im; float Y0 = ty(cv, y0), Y1 = ty(cv, y1); for(int y = 0; y < im->h; y++){ float t = Y1 > Y0 ? sg_clampf((y + 0.5f - Y0) / (Y1 - Y0), 0, 1) : 1, f = a0 + (a1 - a0) * t;
        for(int x = 0; x < im->w; x++){ uint32_t d = im->px[(size_t)y * im->w + x]; int a = (int)((d >> 24) * f + 0.5f); im->px[(size_t)y * im->w + x] = (d & 0xFFFFFF) | (uint32_t)(a < 0 ? 0 : a > 255 ? 255 : a) << 24; } }
}

// ---------------------------------------------------------------- drawing a sprite into a window picture
void sg_blit(uint32_t *dst, int dw, int dh, const SgImg *src, const SgBlit *b){
    if(!src || b->w < 0.5f || b->h < 0.5f) return;
    int xa = (int)floorf(b->x), xb = (int)ceilf(b->x + b->w), ya = (int)floorf(b->y), yb = (int)ceilf(b->y + b->h);
    if(xa < 0) xa = 0; if(xb > dw) xb = dw; if(ya < 0) ya = 0; if(yb > dh) yb = dh; if(b->clip_y1 > b->clip_y0){ if(ya < b->clip_y0) ya = b->clip_y0; if(yb > b->clip_y1) yb = b->clip_y1; }
    float kx = src->w / b->w, ky = src->h / b->h; int mini = (kx > 1.7f || ky > 1.7f), fl = b->flags & 1; int sw = src->w, sh = src->h; const uint32_t *sp = src->px;
    if(!mini){                                                  // magnifying or about 1:1: one bilinear tap in 8-bit fixed point (the common case in the game)
        int fogk = b->fog > 0 ? (b->fog > 255 ? 256 : b->fog + (b->fog >> 7)) : 0, fr = b->fogc >> 16 & 255, fg = b->fogc >> 8 & 255, fb = b->fogc & 255, add = b->flags & 4;
        for(int Y = ya; Y < yb; Y++){ float vv = (Y + 0.5f - b->y) * ky - 0.5f; int iy = (int)floorf(vv); int fy = (int)((vv - iy) * 256.0f); uint32_t *drow = dst + (size_t)Y * dw;
            for(int X = xa; X < xb; X++){ float uu = (X + 0.5f - b->x) * kx; if(fl) uu = sw - uu; uu -= 0.5f; int ix = (int)floorf(uu); int fx = (int)((uu - ix) * 256.0f);
                int wa[4], n = 0; uint32_t px4[4]; int x0 = ix, y0 = iy;
                for(int j = 0; j < 4; j++){ int xx = x0 + (j & 1), yy = y0 + (j >> 1); wa[j] = 0; px4[j] = 0; if(xx < 0 || yy < 0 || xx >= sw || yy >= sh) continue; uint32_t p = sp[yy * sw + xx]; int a = p >> 24; if(!a) continue; int w = ((j & 1) ? fx : 256 - fx) * ((j >> 1) ? fy : 256 - fy) >> 8; wa[j] = w * a; px4[j] = p; n++; }
                if(!n) continue; int sa = wa[0] + wa[1] + wa[2] + wa[3]; if(sa < 256 * 3) continue;
                int r = 0, g = 0, bl = 0; for(int j = 0; j < 4; j++){ if(!wa[j]) continue; r += (int)(px4[j] >> 16 & 255) * wa[j]; g += (int)(px4[j] >> 8 & 255) * wa[j]; bl += (int)(px4[j] & 255) * wa[j]; } r /= sa; g /= sa; bl /= sa;
                int al = sa >> 8; if(al > 256) al = 256;                       // 0..256: the coverage
                if(fogk){ r += ((fr - r) * fogk) >> 8; g += ((fg - g) * fogk) >> 8; bl += ((fb - bl) * fogk) >> 8; }
                uint32_t d = drow[X]; int dr = d >> 16 & 255, dg = d >> 8 & 255, db = d & 255;
                if(add){ dr += (r * al) >> 8; dg += (g * al) >> 8; db += (bl * al) >> 8; if(dr > 255) dr = 255; if(dg > 255) dg = 255; if(db > 255) db = 255; }
                else { dr += ((r - dr) * al) >> 8; dg += ((g - dg) * al) >> 8; db += ((bl - db) * al) >> 8; }
                drow[X] = (uint32_t)dr << 16 | (uint32_t)dg << 8 | (uint32_t)db; } }
        return; }
    for(int Y = ya; Y < yb; Y++){ float v0 = (Y + 0.5f - b->y) * ky;
        for(int X = xa; X < xb; X++){ float u0 = (X + 0.5f - b->x) * kx; if(fl) u0 = sw - u0;
            float accr = 0, accg = 0, accb = 0, acca = 0; int taps = mini ? 4 : 1;
            for(int t = 0; t < taps; t++){ float u = u0, v = v0; if(mini){ u += ((t & 1) - 0.5f) * kx * 0.5f; v += ((t >> 1) - 0.5f) * ky * 0.5f; }
                u -= 0.5f; v -= 0.5f; int ix = (int)floorf(u), iy = (int)floorf(v); float fx = u - ix, fy = v - iy;
                for(int j = 0; j < 4; j++){ int xx = ix + (j & 1), yy = iy + (j >> 1); if(xx < 0 || yy < 0 || xx >= sw || yy >= sh) continue; float wgt = ((j & 1) ? fx : 1 - fx) * ((j >> 1) ? fy : 1 - fy); uint32_t p = sp[yy * sw + xx]; float pa = (p >> 24) * (1.0f / 255.0f) * wgt;
                    acca += pa; accr += (p >> 16 & 255) * pa; accg += (p >> 8 & 255) * pa; accb += (p & 255) * pa; } }
            acca /= taps; if(acca < 0.01f) continue; accr = accr / taps / acca; accg = accg / taps / acca; accb = accb / taps / acca;
            if(b->fog > 0){ float f = b->fog / 255.0f; accr += ((b->fogc >> 16 & 255) - accr) * f; accg += ((b->fogc >> 8 & 255) - accg) * f; accb += ((b->fogc & 255) - accb) * f; }
            uint32_t d = dst[(size_t)Y * dw + X]; float dr = d >> 16 & 255, dg = d >> 8 & 255, db = d & 255; if(acca > 1) acca = 1;
            if(b->flags & 4){ dr += accr * acca; dg += accg * acca; db += accb * acca; if(dr > 255) dr = 255; if(dg > 255) dg = 255; if(db > 255) db = 255; }
            else { dr += (accr - dr) * acca; dg += (accg - dg) * acca; db += (accb - db) * acca; }
            dst[(size_t)Y * dw + X] = (uint32_t)(int)(dr + 0.5f) << 16 | (uint32_t)(int)(dg + 0.5f) << 8 | (uint32_t)(int)(db + 0.5f); } }
}

// ---------------------------------------------------------------- the cache (least recently used, with a size cap in bytes)
// Two limits: at most SG_CACHE pictures and at most cap_ bytes of pixels (default 32 MB, 12 MB on Android; BUGSTAR_SG_CACHE_MB or sg_cache_set_cap_mb change it).
// Only the thread that plays (the "main" thread) touches this list. The optional worker thread (sg_async_start) makes pictures for it in the background:
// a picture that is not there yet is queued and the caller gets 0 (draw the game's own sprite, or nothing, for a frame or two); finished pictures are put into the list
// by the main thread at its next cache call, so no picture is ever freed while somebody draws it. Without the worker (the default, and always in the tests unless
// BUGSTAR_SG_ASYNC=1) every call makes its picture at once, exactly as before.
#define SG_CACHE 192
#define SG_QMAX 256
#ifdef PC_ANDROID
#define SG_CAP_DEFAULT_MB 12
#else
#define SG_CAP_DEFAULT_MB 32
#endif
#include <pthread.h>
typedef struct { uint64_t key; SgImg *im; uint32_t stamp; int pending; } SgEnt;
static SgEnt cache_[SG_CACHE]; static uint32_t cache_clock; static int c_hits, c_miss, c_dropped; static long c_bytes, c_cap = -1, c_peak;
typedef struct { uint64_t key; int w, h; void (*make)(SgImg *, void *); unsigned char arg[128]; int argn; SgImg *out; int state; } SgJob;      // state 0 free, 1 queued, 2 running, 3 done
static SgJob q_[SG_QMAX]; static pthread_mutex_t q_m = PTHREAD_MUTEX_INITIALIZER; static pthread_cond_t q_c = PTHREAD_COND_INITIALIZER, q_d = PTHREAD_COND_INITIALIZER; static int q_on, q_run, q_gen_ms_max; static int q_nq;
static __thread int tls_worker;
static long cap_bytes(void){ if(c_cap < 0){ long mb = SG_CAP_DEFAULT_MB; const char *e = getenv("BUGSTAR_SG_CACHE_MB"); if(e && atoi(e) > 0) mb = atoi(e); c_cap = mb << 20; } return c_cap; }
void sg_cache_set_cap_mb(int mb){ c_cap = (long)(mb < 1 ? 1 : mb) << 20; }
long sg_cache_bytes(void){ return c_bytes; }
long sg_cache_peak(void){ return c_peak; }
static void ent_drop(SgEnt *e){ if(e->im){ c_bytes -= (long)e->im->w * e->im->h * 4; sg_img_free(e->im); e->im = 0; } e->pending = 0; e->key = 0; }
static void evict_to(long limit, const SgEnt *keep){        // drop the least recently used finished pictures until the pixels fit
    while(c_bytes > limit){ int old = -1; uint32_t os = 0xFFFFFFFFu; for(int i = 0; i < SG_CACHE; i++) if(cache_[i].im && !cache_[i].pending && &cache_[i] != keep && cache_[i].stamp < os){ os = cache_[i].stamp; old = i; }
        if(old < 0) break; ent_drop(&cache_[old]); c_dropped++; }
}
void sg_cache_trim(void){ evict_to(0, 0); }                                               // low memory: give everything back that is not being made
// a finished background picture goes into its slot (the slot was reserved when the job was queued)
static void pump(void){
    if(!q_on) return; pthread_mutex_lock(&q_m);
    for(int j = 0; j < SG_QMAX; j++){ SgJob *b = &q_[j]; if(b->state != 3) continue; SgEnt *e = 0;
        for(int i = 0; i < SG_CACHE; i++) if(cache_[i].pending && cache_[i].key == b->key){ e = &cache_[i]; break; }
        if(e && b->out){ e->im = b->out; e->pending = 0; c_bytes += (long)b->w * b->h * 4; if(c_bytes > c_peak) c_peak = c_bytes; } else { if(b->out) sg_img_free(b->out); if(e) ent_drop(e); }
        b->out = 0; b->state = 0; q_nq--; }
    pthread_mutex_unlock(&q_m);
    evict_to(cap_bytes(), 0);
}
static void *worker(void *a){ (void)a; tls_worker = 1;
    for(;;){ pthread_mutex_lock(&q_m); SgJob *b = 0; for(;;){ int best = -1; for(int j = 0; j < SG_QMAX; j++) if(q_[j].state == 1){ best = j; break; } if(best >= 0){ b = &q_[best]; break; } pthread_cond_wait(&q_c, &q_m); }
        b->state = 2; pthread_mutex_unlock(&q_m);
        SgImg *im = sg_img_new(b->w, b->h); if(im) b->make(im, b->argn ? (void *)b->arg : 0);
        pthread_mutex_lock(&q_m); b->out = im; b->state = 3; pthread_cond_broadcast(&q_d); pthread_mutex_unlock(&q_m); }
    return 0; }
int sg_async_start(void){
    if(q_on) return 1; const char *e = getenv("BUGSTAR_SG_ASYNC"); if(e && e[0] == '0') return 0;
    pthread_attr_t at; pthread_t th; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 8u << 20); pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&th, &at, worker, 0)) return 0; q_on = 1; return 1; }
int sg_async_on(void){ return q_on; }
void sg_async_stats(int *queued, int *dropped){ if(queued) *queued = q_nq; if(dropped) *dropped = c_dropped; }
// the worker's own little cache (a planet's surface map is made by sg_planet through sg_cached_ex, and must not touch the main list)
typedef struct { uint64_t key; SgImg *im; uint32_t stamp; } WEnt;
static __thread WEnt wc_[6]; static __thread uint32_t wc_clock;
static const SgImg *wcache_get(uint64_t key, int w, int h, void (*mk)(SgImg *, void *), void *arg){
    int old = 0; wc_clock++; for(int i = 0; i < 6; i++){ if(wc_[i].im && wc_[i].key == key){ wc_[i].stamp = wc_clock; return wc_[i].im; } if(!wc_[i].im || wc_[i].stamp < wc_[old].stamp) old = i; }
    sg_img_free(wc_[old].im); wc_[old].im = sg_img_new(w, h); if(!wc_[old].im) return 0; wc_[old].key = key; wc_[old].stamp = wc_clock; mk(wc_[old].im, arg); return wc_[old].im; }
// look a picture up; make = how to draw it, arg/argn = its numbers (copied for the worker, at most 128 bytes). try_async: queue it instead of waiting for it
static const SgImg *cache_get(uint64_t key, int w, int h, void (*mk)(SgImg *, void *), void *arg, int argn, int try_async){
    if(tls_worker) return wcache_get(key, w, h, mk, arg);
    pump(); cache_clock++; int free_i = -1, old_i = -1; uint32_t old_s = 0xFFFFFFFFu;
    for(int i = 0; i < SG_CACHE; i++){ if(cache_[i].key == key && (cache_[i].im || cache_[i].pending)){ cache_[i].stamp = cache_clock; if(cache_[i].im){ c_hits++; return cache_[i].im; } if(try_async) return 0; while(cache_[i].pending && q_on){ pthread_mutex_lock(&q_m); int any = 0; for(int j = 0; j < SG_QMAX; j++) if(q_[j].state == 3) any = 1; if(!any) pthread_cond_wait(&q_d, &q_m); pthread_mutex_unlock(&q_m); pump(); } return cache_[i].im; }
        if(!cache_[i].im && !cache_[i].pending){ if(free_i < 0) free_i = i; } else if(cache_[i].im && cache_[i].stamp < old_s){ old_s = cache_[i].stamp; old_i = i; } }
    int slot = free_i >= 0 ? free_i : old_i; if(slot < 0) return 0; ent_drop(&cache_[slot]); c_miss++;
    long need = (long)w * h * 4; if(!try_async || !q_on || argn > 128 || argn < 0){
        evict_to(cap_bytes() - need, 0);
        SgImg *im = sg_img_new(w, h); if(!im) return 0; cache_[slot].im = im; cache_[slot].key = key; cache_[slot].stamp = cache_clock; c_bytes += need; if(c_bytes > c_peak) c_peak = c_bytes;      // (taken before it is drawn: a generator may use the cache itself and must not be given this slot)
        mk(im, arg); return im; }
    pthread_mutex_lock(&q_m); int jb = -1; for(int j = 0; j < SG_QMAX; j++) if(q_[j].state == 0){ jb = j; break; }
    if(jb < 0){ pthread_mutex_unlock(&q_m); return 0; }                                    // the queue is full: ask again next frame
    SgJob *b = &q_[jb]; b->key = key; b->w = w; b->h = h; b->make = mk; b->argn = argn; if(argn) memcpy(b->arg, arg, (size_t)argn); b->out = 0; b->state = 1; q_nq++; pthread_cond_signal(&q_c); pthread_mutex_unlock(&q_m);
    cache_[slot].key = key; cache_[slot].pending = 1; cache_[slot].stamp = cache_clock; return 0;
}
typedef struct { int gen; uint32_t id; } MkArg;
static void mk_gen(SgImg *im, void *a){ MkArg *m = (MkArg *)a; sg_gens[m->gen].draw(im, m->id); }
static uint64_t gen_key(int gen, uint32_t id, int size){ return sg_m64(((uint64_t)gen << 40) ^ ((uint64_t)id << 8) ^ (uint64_t)size) | 1; }
static int gen_dims(int gen, int size, int *w, int *h){ if(gen < 0 || gen >= sg_ngens) return 0; if(size < 8) size = 8; if(size > 256) size = 256; const SgGen *g = &sg_gens[gen]; *w = size; *h = (int)((long)size * g->h / g->w); if(*h < 1) *h = 1; return 1; }
const SgImg *sg_cached(int gen, uint32_t id, int size){ int w, h; if(!gen_dims(gen, size, &w, &h)) return 0; MkArg a = { gen, id }; return cache_get(gen_key(gen, id, w), w, h, mk_gen, &a, 0, 0); }
const SgImg *sg_cached_try(int gen, uint32_t id, int size){ int w, h; if(!gen_dims(gen, size, &w, &h)) return 0; MkArg a = { gen, id }; return cache_get(gen_key(gen, id, w), w, h, mk_gen, &a, (int)sizeof a, 1); }
static uint64_t ex_key(uint32_t key, int w, int h){ return sg_m64(0xE000000000000000ull ^ ((uint64_t)key << 12) ^ (uint64_t)w * 7919u ^ (uint64_t)h); }
const SgImg *sg_cached_ex(uint32_t key, int w, int h, void (*make)(SgImg *, void *), void *arg){ return cache_get(ex_key(key, w, h), w, h, make, arg, 0, 0); }
const SgImg *sg_cached_ex_try(uint32_t key, int w, int h, void (*make)(SgImg *, void *), const void *arg, int argn){ return cache_get(ex_key(key, w, h), w, h, make, (void *)arg, argn, 1); }
void sg_cache_stats(int *hits, int *misses, int *live){ if(hits) *hits = c_hits; if(misses) *misses = c_miss; if(live){ int n = 0; for(int i = 0; i < SG_CACHE; i++) if(cache_[i].im) n++; *live = n; } }
void sg_cache_clear(void){ pump(); for(int i = 0; i < SG_CACHE; i++) if(!cache_[i].pending) ent_drop(&cache_[i]); c_hits = c_miss = c_dropped = 0; c_peak = c_bytes; }
