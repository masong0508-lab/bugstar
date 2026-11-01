// shipr.c: the PC ship renderer (see shipr.h). Everything here is made from numbers: a few tiny parametric meshes, colours worked out at run
// time, glows and explosions from formulas. No image files.
//
// How it stays fast with thousands of ships:
//  * one pass transforms every ship's centre once (structure of arrays), culls what is behind or off screen, and works out its size on screen;
//  * level of detail by size on screen: under ~1.5 px a dot (an icon of a few pixels), under ~7 px a flat arrow (one triangle), above that a
//    lit mesh. In a crowd most ships are dots, and a dot costs a few pixel writes;
//  * meshes go through one shared depth buffer (correct overlap, no sorting), glows and beams are added after;
//  * CLASSIC draws the same lists the GBA way: wireframes and 2x2 marks on the game's own 240x128 grid, scaled up sharp.
#include "shipr.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
static double now_ms(void){ return clock() * 1000.0 / CLOCKS_PER_SEC; }
#else
static double now_ms(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
#endif

typedef struct { float a[3], b[3]; uint32_t col; float w; int far; } Beam; static int beam_far;
typedef struct { float p[3], r, t; uint32_t seed; } Boom;
typedef struct { const SrMesh *m; float p[3], f[3], u[3], s; uint32_t col; } MeshI;
static SrShip *S; static int nS, capS;
static Beam *Bm; static int nB, capB;
static Boom *Bo; static int nBo, capBo;
static MeshI Mi[64]; static int nM;
static int mode; static float E[3], R[3], U[3], F[3], FOC, CX, CY, VB, SUN[3];
static double last_ms;
static void (*PAR)(int, int, void (*)(void*, int, int), void*); static int NTH = 1;
void sr_set_par(void (*par)(int, int, void (*)(void*, int, int), void*), int nthreads){ PAR = par; NTH = nthreads < 1 ? 1 : nthreads; }
static __thread int YL, YH;                       // this thread's band of rows
// the lists being built (sr_begin .. sr_commit) and the lists on show (what sr_render draws): the game draws the next frame while the last one is shown
typedef struct { SrShip *S; int nS, capS; Beam *Bm; int nB, capB; Boom *Bo; int nBo, capBo; MeshI Mi[64]; int nM; int mode, tag; float E[3], R[3], U[3], F[3], FOC, CX, CY, VB, SUN[3]; } Set;
static Set shownv[2]; static int have_v[2], sel_tag; 
#define shown shownv[sel_tag]
#define have_shown have_v[sel_tag]
static void (*hooks[8])(void); static int nhooks;
void sr_on_frame(void (*fn)(void)){ for(int i = 0; i < nhooks; i++) if(hooks[i] == fn) return; if(nhooks < 8) hooks[nhooks++] = fn; }
void sr_run_hooks(void){ for(int i = 0; i < nhooks; i++) hooks[i](); }
static void cp_arr(void **dst, int *cap, const void *src, int n, size_t sz){ if(n > *cap){ *cap = n + 256; *dst = realloc(*dst, *cap * sz); } if(n) memcpy(*dst, src, n * sz); }
void sr_commit(int tag){
    sel_tag = tag & 1;
    cp_arr((void**)&shown.S, &shown.capS, S, nS, sizeof(SrShip)); shown.nS = nS; cp_arr((void**)&shown.Bm, &shown.capB, Bm, nB, sizeof(Beam)); shown.nB = nB;
    cp_arr((void**)&shown.Bo, &shown.capBo, Bo, nBo, sizeof(Boom)); shown.nBo = nBo; memcpy(shown.Mi, Mi, sizeof Mi); shown.nM = nM;
    shown.mode = mode; shown.tag = tag; memcpy(shown.E, E, 12); memcpy(shown.R, R, 12); memcpy(shown.U, U, 12); memcpy(shown.F, F, 12); memcpy(shown.SUN, SUN, 12);
    shown.FOC = FOC; shown.CX = CX; shown.CY = CY; shown.VB = VB; have_shown = 1; }
int sr_shown_tag(void){ return have_shown ? shown.tag : -1; }
int sr_select(int tag){ sel_tag = tag & 1; return have_shown; }
static void use_shown(void){   // sr_render draws the committed lists: swap them into the working names for the draw
    S = shown.S; nS = shown.nS; Bm = shown.Bm; nB = shown.nB; Bo = shown.Bo; nBo = shown.nBo; memcpy(Mi, shown.Mi, sizeof Mi); nM = shown.nM; mode = shown.mode;
    memcpy(E, shown.E, 12); memcpy(R, shown.R, 12); memcpy(U, shown.U, 12); memcpy(F, shown.F, 12); memcpy(SUN, shown.SUN, 12); FOC = shown.FOC; CX = shown.CX; CY = shown.CY; VB = shown.VB; }

#define GROW(arr, n, cap, T) do { if((n) >= (cap)){ (cap) = (cap) ? (cap) * 2 : 256; arr = (T*)realloc(arr, sizeof(T) * (cap)); } } while(0)
void sr_begin(int m, const float eye[3], const float r[3], const float u[3], const float f[3], float focal, float cx, float cy, float vb, const float sun[3]){
    mode = m; memcpy(E, eye, 12); memcpy(R, r, 12); memcpy(U, u, 12); memcpy(F, f, 12); FOC = focal; CX = cx; CY = cy; VB = vb;
    float l = sqrtf(sun[0]*sun[0] + sun[1]*sun[1] + sun[2]*sun[2]); if(l < 1e-6f) l = 1; for(int k = 0; k < 3; k++) SUN[k] = sun[k] / l;
    nS = nB = nBo = nM = 0;
}
void sr_ship(const SrShip *s){ GROW(S, nS, capS, SrShip); S[nS++] = *s; }
void sr_ships(const SrShip *s, int n){ for(int i = 0; i < n; i++) sr_ship(&s[i]); }
void sr_beam(const float a[3], const float b[3], uint32_t col, float w){ GROW(Bm, nB, capB, Beam); Beam *q = &Bm[nB++]; memcpy(q->a, a, 12); memcpy(q->b, b, 12); q->col = col; q->w = w; q->far = beam_far; }
void sr_boom(const float p[3], float r, float t, uint32_t seed){ GROW(Bo, nBo, capBo, Boom); Boom *q = &Bo[nBo++]; memcpy(q->p, p, 12); q->r = r; q->t = t; q->seed = seed; }
void sr_mesh(const SrMesh *m, const float p[3], const float f[3], const float u[3], float s, uint32_t col){ if(nM >= 64) return; MeshI *q = &Mi[nM++]; q->m = m; memcpy(q->p, p, 12); memcpy(q->f, f, 12); memcpy(q->u, u, 12); q->s = s; q->col = col; }
int sr_count(void){ return nS; }
double sr_last_ms(void){ return last_ms; }

static uint32_t hsh(uint32_t x){ x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static float frnd(uint32_t *s){ *s = hsh(*s + 0x9E3779B9u); return (*s & 0xFFFFFF) / 16777216.0f; }
// a distant battle: two fleets in two clouds facing each other, ships weaving; positions come only from (seed, i, frame)
void sr_armada(int n, uint32_t seed, long frame, float dist){
    float c[3]; for(int k = 0; k < 3; k++) c[k] = E[k] + F[k] * dist;
    for(int i = 0; i < n; i++){
        uint32_t s = hsh(seed ^ (uint32_t)i * 2654435761u); int team = i & 1;
        float a = frnd(&s) * 6.2832f, b = (frnd(&s) - 0.5f) * 1.2f, rr = dist * 0.22f * sqrtf(frnd(&s));
        float side = team ? 1.f : -1.f, t = frame * (0.004f + frnd(&s) * 0.004f) + frnd(&s) * 6.2832f;
        float lx = side * dist * 0.18f + cosf(a) * rr * 0.6f + sinf(t) * 120, ly = sinf(b) * rr * 0.8f + cosf(t * 1.3f) * 80, lz = sinf(a) * rr * 0.5f;
        SrShip q; memset(&q, 0, sizeof q);
        for(int k = 0; k < 3; k++) q.p[k] = c[k] + R[k] * lx + U[k] * ly + F[k] * lz;
        float fx = -side + cosf(t) * 0.4f, fz = sinf(t) * 0.6f, l = sqrtf(fx*fx + fz*fz); fx /= l; fz /= l;
        for(int k = 0; k < 3; k++){ q.f[k] = R[k] * fx + F[k] * fz; q.u[k] = U[k]; }
        int heavy = (s & 31) == 0; q.kind = heavy ? SR_HEAVY : team ? SR_FIGHTER : SR_WING; q.team = (uint8_t)team; q.size = heavy ? 320 : 110;
        q.col = team ? 0xD84A3A : 0x4A9AF0; q.thrust = 0.7f; q.far = 1; q.firing = ((frame + i) % 47) < 3;
        sr_ship(&q);
        if(q.firing && i + 2 < n && (s & 15) == 0){ float tg[3]; for(int k = 0; k < 3; k++) tg[k] = q.p[k] + q.f[k] * 900 + R[k] * side * -400; beam_far = 1; sr_beam(q.p, tg, team ? 0xFF6040 : 0x60C0FF, 1.0f); beam_far = 0; }
    }
}

// ---- the meshes: parametric, a handful of points each (x right, y up, z forward, unit = the ship's half length)
typedef struct { int nv, nt; float v[24][3]; uint8_t t[40][3]; float eng[2][3]; int ne; } Mesh;
static Mesh mesh[SR_KINDS][2]; static int mesh_ok;            // [kind][0 = NATIVE, 1 = ULTRA (more parts)]
static void m_v(Mesh *m, float x, float y, float z){ m->v[m->nv][0] = x; m->v[m->nv][1] = y; m->v[m->nv][2] = z; m->nv++; }
static void m_t(Mesh *m, int a, int b, int c){ m->t[m->nt][0] = a; m->t[m->nt][1] = b; m->t[m->nt][2] = c; m->nt++; }
static void m_tri2(Mesh *m, int a, int b, int c){ m_t(m, a, b, c); m_t(m, a, c, b); }
static void build_meshes(void){
    for(int q = 0; q < 2; q++){
        // the fighter: the game's own shape (nose, two wing tips, fin, belly, tail), made solid
        Mesh *m = &mesh[SR_FIGHTER][q]; memset(m, 0, sizeof *m);
        m_v(m, 0, 0, 1); m_v(m, -1, 0, -.45f); m_v(m, 1, 0, -.45f); m_v(m, 0, .32f, -.45f); m_v(m, 0, -.16f, -.45f); m_v(m, 0, 0, -.64f);
        m_t(m, 0, 3, 1); m_t(m, 0, 2, 3); m_t(m, 0, 1, 4); m_t(m, 0, 4, 2); m_t(m, 1, 3, 5); m_t(m, 3, 2, 5); m_t(m, 1, 5, 4); m_t(m, 4, 5, 2);
        if(q){ int a = m->nv; m_v(m, -.55f, 0, -.2f); m_v(m, -.6f, .12f, -.55f); m_v(m, -.5f, 0, -.6f); m_tri2(m, a, a + 1, a + 2);
               a = m->nv; m_v(m, .55f, 0, -.2f); m_v(m, .6f, .12f, -.55f); m_v(m, .5f, 0, -.6f); m_tri2(m, a, a + 1, a + 2);
               a = m->nv; m_v(m, 0, .12f, .45f); m_v(m, -.08f, .06f, .2f); m_v(m, .08f, .06f, .2f); m_tri2(m, a, a + 1, a + 2); }   // wing fins and a canopy
        m->ne = 1; m->eng[0][0] = 0; m->eng[0][1] = 0; m->eng[0][2] = -.66f;
        // the wingman: twin booms, a centre pod
        m = &mesh[SR_WING][q]; memset(m, 0, sizeof *m);
        m_v(m, 0, 0, .9f); m_v(m, -.3f, 0, -.3f); m_v(m, .3f, 0, -.3f); m_v(m, 0, .25f, -.2f); m_v(m, 0, -.2f, -.3f);
        m_t(m, 0, 3, 1); m_t(m, 0, 2, 3); m_t(m, 0, 1, 4); m_t(m, 0, 4, 2); m_t(m, 1, 3, 2); m_t(m, 1, 2, 4);
        for(int s = -1; s <= 1; s += 2){ int a = m->nv; m_v(m, s * .75f, 0, .4f); m_v(m, s * .95f, 0, -.7f); m_v(m, s * .55f, .1f, -.7f); m_v(m, s * .2f, 0, -.2f);
            m_tri2(m, a, a + 1, a + 2); m_tri2(m, a, a + 2, a + 3); }
        m->ne = 2; m->eng[0][0] = -.75f; m->eng[0][2] = -.72f; m->eng[1][0] = .75f; m->eng[1][2] = -.72f;
        if(q){ int a = m->nv; m_v(m, 0, .2f, .3f); m_v(m, -.1f, .1f, 0); m_v(m, .1f, .1f, 0); m_tri2(m, a, a + 1, a + 2); }
        // the heavy: a long box hull with a bridge
        m = &mesh[SR_HEAVY][q]; memset(m, 0, sizeof *m);
        float w = .22f, h = .16f;
        m_v(m, 0, 0, 1); for(int i = 0; i < 4; i++){ float x = (i & 1) ? w : -w, y = (i & 2) ? h : -h; m_v(m, x, y, .4f); } for(int i = 0; i < 4; i++){ float x = (i & 1) ? w : -w, y = (i & 2) ? h : -h; m_v(m, x, y, -1); }
        m_t(m, 0, 3, 4); m_t(m, 0, 4, 2); m_t(m, 0, 2, 1); m_t(m, 0, 1, 3);
        m_t(m, 3, 7, 8); m_t(m, 3, 8, 4); m_t(m, 4, 8, 6); m_t(m, 4, 6, 2); m_t(m, 2, 6, 5); m_t(m, 2, 5, 1); m_t(m, 1, 5, 7); m_t(m, 1, 7, 3); m_t(m, 5, 6, 8); m_t(m, 5, 8, 7);
        if(q){ int a = m->nv; m_v(m, 0, h + .14f, -.5f); m_v(m, -.1f, h, -.3f); m_v(m, .1f, h, -.3f); m_v(m, 0, h, -.8f); m_tri2(m, a, a + 1, a + 2); m_tri2(m, a, a + 3, a + 1); m_tri2(m, a, a + 2, a + 3); }
        m->ne = 2; m->eng[0][0] = -.12f; m->eng[0][2] = -1.02f; m->eng[1][0] = .12f; m->eng[1][2] = -1.02f;
    }
    mesh_ok = 1;
}

// ---- the target
static uint32_t *O; static int W, H; static float *Z; static size_t zcap; static const uint8_t *MK; static float GX, GY;   // GX/GY: game px per output px
static int VBp;                                                                                                              // the view's bottom in output px
static int bgonly;                                                                                                           // drawing a far (scenery) ship: only over empty sky
static inline int ok_px(int x, int y){ if(!MK) return 1; int gx = (int)(x * GX), gy = (int)(y * GY); int m = MK[gy * 240 + gx]; return bgonly ? m == 2 : m != 0; }
static inline void add_px(int x, int y, uint32_t c, float a){         // additive light
    if(x < 0 || y < YL || x >= W || y >= YH || !ok_px(x, y)) return; uint32_t *p = &O[y * W + x];
    int r = (*p >> 16 & 255) + (int)((c >> 16 & 255) * a), g = (*p >> 8 & 255) + (int)((c >> 8 & 255) * a), b = (*p & 255) + (int)((c & 255) * a);
    *p = (uint32_t)(r > 255 ? 255 : r) << 16 | (uint32_t)(g > 255 ? 255 : g) << 8 | (uint32_t)(b > 255 ? 255 : b);
}
static inline void put_z(int x, int y, float z, uint32_t c){
    if(x < 0 || y < YL || x >= W || y >= YH) return; size_t i = (size_t)y * W + x; if(z >= Z[i] || !ok_px(x, y)) return; Z[i] = z; O[i] = c;
}
static int proj(const float w[3], float *sx, float *sy, float *cz){   // world -> output pixels; 0 = behind
    float d[3] = { w[0] - E[0], w[1] - E[1], w[2] - E[2] };
    float x = d[0]*R[0] + d[1]*R[1] + d[2]*R[2], y = d[0]*U[0] + d[1]*U[1] + d[2]*U[2], z = d[0]*F[0] + d[1]*F[1] + d[2]*F[2];
    if(z < 60) return 0; *sx = (CX + x * FOC / z) / GX; *sy = (CY - y * FOC / z) / GY; *cz = z; return 1;
}
static uint32_t shade(uint32_t c, float k){ int r = (int)((c >> 16 & 255) * k), g = (int)((c >> 8 & 255) * k), b = (int)((c & 255) * k); r = r > 255 ? 255 : r < 0 ? 0 : r; g = g > 255 ? 255 : g < 0 ? 0 : g; b = b > 255 ? 255 : b < 0 ? 0 : b; return (uint32_t)r << 16 | (uint32_t)g << 8 | b; }
static void tri(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2, float z2, uint32_t c){
    float A = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0); if(fabsf(A) < 1e-6f) return;
    int bx0 = (int)floorf(fminf(x0, fminf(x1, x2))), bx1 = (int)ceilf(fmaxf(x0, fmaxf(x1, x2))), by0 = (int)floorf(fminf(y0, fminf(y1, y2))), by1 = (int)ceilf(fmaxf(y0, fmaxf(y1, y2)));
    if(bx0 < 0) bx0 = 0; if(by0 < YL) by0 = YL; if(bx1 > W - 1) bx1 = W - 1; if(by1 > YH - 1) by1 = YH - 1; if(bx0 > bx1 || by0 > by1) return;
    float iA = 1.0f / A;
    for(int y = by0; y <= by1; y++){ float py = y + 0.5f;
        for(int x = bx0; x <= bx1; x++){ float px = x + 0.5f;
            float w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) * iA, w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) * iA, w2 = 1 - w0 - w1;
            if(w0 < 0 || w1 < 0 || w2 < 0) continue; put_z(x, y, w0 * z0 + w1 * z1 + w2 * z2, c); } }
}
static void glow(float x, float y, float rad, uint32_t c, float a){   // a soft round light, added
    if(rad < 0.7f){ add_px((int)x, (int)y, c, a); return; }
    int r = (int)ceilf(rad * 2); if(y + r < YL || y - r >= YH) return; float i2 = 1.0f / (rad * rad);
    for(int dy = -r; dy <= r; dy++) for(int dx = -r; dx <= r; dx++){ float d2 = (dx*dx + dy*dy) * i2; if(d2 > 4) continue; float k = a / (1 + d2 * 3) - a * 0.077f; if(k > 0) add_px((int)x + dx, (int)y + dy, c, k); }
}
static void lineadd(float x0, float y0, float x1, float y1, uint32_t col, float w, float a){   // a glowing line: one pass over its box, light falls off with the distance to the segment
    if(w <= 1.0f){                                                       // thin: a plain line with soft sides
        float dx = x1 - x0, dy = y1 - y0; int n = (int)fmaxf(fabsf(dx), fabsf(dy)) + 1; if(n > 8000) n = 8000; int xm = fabsf(dx) >= fabsf(dy);
        for(int i = 0; i <= n; i++){ float t = (float)i / n, x = x0 + dx * t, y = y0 + dy * t; add_px((int)x, (int)y, col, a); if(xm){ add_px((int)x, (int)y - 1, col, a * .3f); add_px((int)x, (int)y + 1, col, a * .3f); } else { add_px((int)x - 1, (int)y, col, a * .3f); add_px((int)x + 1, (int)y, col, a * .3f); } }
        return; }
    float r = w; float ext = r * 2 + 1;
    int bx0 = (int)floorf(fminf(x0, x1) - ext), bx1 = (int)ceilf(fmaxf(x0, x1) + ext), by0 = (int)floorf(fminf(y0, y1) - ext), by1 = (int)ceilf(fmaxf(y0, y1) + ext);
    if(bx0 < 0) bx0 = 0; if(by0 < YL) by0 = YL; if(bx1 > W - 1) bx1 = W - 1; if(by1 > YH - 1) by1 = YH - 1; if(bx0 > bx1 || by0 > by1) return;
    float dx = x1 - x0, dy = y1 - y0, L2 = dx*dx + dy*dy + 1e-6f, i2 = 1.0f / (r * r); int xm = fabsf(dx) >= fabsf(dy);
    int o0 = xm ? bx0 : by0, o1 = xm ? bx1 : by1;
    for(int o = o0; o <= o1; o++){                                       // walk along the long axis; across it only the band near the line
        float c = xm ? (dx != 0 ? y0 + dy * ((o + .5f - x0) / dx) : y0) : (dy != 0 ? x0 + dx * ((o + .5f - y0) / dy) : x0);
        float slope = xm ? fabsf(dy / (dx + 1e-6f)) : fabsf(dx / (dy + 1e-6f)); int band = (int)(ext * (1 + slope)) + 1;
        int q0 = (int)c - band, q1 = (int)c + band, lo = xm ? by0 : bx0, hi = xm ? by1 : bx1; if(q0 < lo) q0 = lo; if(q1 > hi) q1 = hi;
        for(int q = q0; q <= q1; q++){ int x = xm ? o : q, y = xm ? q : o; float px = x + .5f - x0, py = y + .5f - y0; float t = (px*dx + py*dy) / L2; t = t < 0 ? 0 : t > 1 ? 1 : t;
            float ex = px - dx*t, ey = py - dy*t, d2 = (ex*ex + ey*ey) * i2; if(d2 > 4) continue; float k = a / (1 + d2 * 3) - a * 0.077f; if(k > 0) add_px(x, y, col, k); } }
}

// ---- CLASSIC: the game's own look on the game's own grid (240 x 128), lines and 2x2 marks in flat colours, then scaled up sharp
static uint32_t cg[240 * 160]; static uint8_t cgs[240 * 160];
static int cfar; static void cpx(int x, int y, uint32_t c){ if(x < 0 || y < 0 || x >= 240 || y >= (int)VB) return; int q = y * 240 + x; if(cfar && cgs[q] == 1) return; cg[q] = c; cgs[q] = cfar ? 2 : 1; }
static void cline(int x0, int y0, int x1, int y1, uint32_t c){
    int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, n = 0;
    for(;;){ cpx(x0, y0, c); if((x0 == x1 && y0 == y1) || ++n > 600) break; int e2 = 2 * e; if(e2 >= dy){ e += dy; x0 += sx; } if(e2 <= dx){ e += dx; y0 += sy; } }
}
static int gproj(const float w[3], int *x, int *y, float *z){ float d[3] = { w[0]-E[0], w[1]-E[1], w[2]-E[2] }; float cx = d[0]*R[0]+d[1]*R[1]+d[2]*R[2], cy = d[0]*U[0]+d[1]*U[1]+d[2]*U[2], cz = d[0]*F[0]+d[1]*F[1]+d[2]*F[2];
    if(cz <= 20) return 0; *x = (int)lrintf(CX + cx * FOC / cz); *y = (int)lrintf(CY - cy * FOC / cz); *z = cz; return 1; }
static int render_classic(void){
    memset(cgs, 0, sizeof cgs); int drawn = 0;
    static const float mdl[6][3] = {{0,0,110},{-110,0,-50},{110,0,-50},{0,35,-50},{0,-18,-50},{0,0,-70}};
    static const uint8_t pe[] = {0,1,0,2,0,3,0,4,1,3,2,3,1,4,2,4,3,5,4,5,1,5,2,5}, pel[] = {0,1,0,2,1,5,2,5,3,5};
    for(int i = 0; i < nB; i++){ int ax, ay, bx, by; float z; cfar = Bm[i].far; if(gproj(Bm[i].a, &ax, &ay, &z) && gproj(Bm[i].b, &bx, &by, &z)) cline(ax, ay, bx, by, Bm[i].col); }
    for(int i = 0; i < nS; i++){ const SrShip *s = &S[i]; int sx, sy; float z; cfar = s->far; if(!gproj(s->p, &sx, &sy, &z)) continue;
        float rp = s->size * 1.3f * FOC / z; if(sx + rp < 0 || sx - rp > 239 || sy + rp < 0 || sy - rp > VB) continue; drawn++;
        if(rp < 3.5f){ cpx(sx, sy, s->col); if(nS < 600 || rp > 2){ cpx(sx - 1, sy, s->col); cpx(sx + 1, sy, s->col); cpx(sx, sy - 1, s->col); } continue; }   // the game's far mark (a crowd: one pixel)   // the game's far mark
        float r3[3] = { s->f[1]*s->u[2] - s->f[2]*s->u[1], s->f[2]*s->u[0] - s->f[0]*s->u[2], s->f[0]*s->u[1] - s->f[1]*s->u[0] };
        int vx[6], vy[6], okv[6]; float k = s->size / 110.0f;
        for(int v = 0; v < 6; v++){ float w[3]; for(int a = 0; a < 3; a++) w[a] = s->p[a] + k * (-mdl[v][0] * r3[a] + mdl[v][1] * s->u[a] + mdl[v][2] * s->f[a]); okv[v] = gproj(w, &vx[v], &vy[v], &z); }
        const uint8_t *e = rp < 9 ? pel : pe; int ne = rp < 9 ? 5 : 12;
        for(int j = 0; j < ne; j++){ int a = e[j*2], b = e[j*2+1]; if(okv[a] && okv[b]) cline(vx[a], vy[a], vx[b], vy[b], s->col); } }
    cfar = 0;
    for(int i = 0; i < nBo; i++){ int sx, sy; float z; if(!gproj(Bo[i].p, &sx, &sy, &z)) continue; uint32_t sd = Bo[i].seed; int n = 10;
        for(int j = 0; j < n; j++){ float a = frnd(&sd) * 6.2832f, d = Bo[i].r * Bo[i].t * (0.4f + frnd(&sd)) * FOC / z; cpx(sx + (int)(cosf(a) * d), sy + (int)(sinf(a) * d), Bo[i].t < .4f ? 0xFFFFC0 : 0xFF8030); } }
    for(int y = 0; y < VBp; y++){ int gy = (int)(y * GY); if(gy >= 160) break; for(int x = 0; x < W; x++){ int gx = (int)(x * GX), q = gy * 240 + gx; if(cgs[q] && (!MK || MK[q] >= cgs[q])) O[y * W + x] = cg[q]; } }
    return drawn;
}

static void sr_band(void *vd, int lo, int hi){
    YL = lo; YH = hi; int drawn = 0; for(int y = lo; y < hi; y++) for(int x = 0; x < W; x++) Z[(size_t)y * W + x] = 1e30f;
    int ultra = mode == SR_ULTRA; float px_per = 1.0f / GX;
    float dotR = nS > 3000 ? 3.2f : nS > 800 ? 2.4f : 1.6f, arrR = nS > 3000 ? 11 : nS > 800 ? 9 : 7;   // a crowd: simpler shapes sooner (the far ones are tiny anyway)
    // big hulls first (they fill the depth buffer)
    for(int i = 0; i < nM; i++){ const MeshI *q = &Mi[i]; const SrMesh *m = q->m; float r3[3] = { q->f[1]*q->u[2]-q->f[2]*q->u[1], q->f[2]*q->u[0]-q->f[0]*q->u[2], q->f[0]*q->u[1]-q->f[1]*q->u[0] };
        for(int t = 0; t < m->nt; t++){ float P[3][3], sx[3], sy[3], sz[3]; int ok = 1;
            for(int k = 0; k < 3; k++){ const float *v = &m->v[m->tri[t*3+k]*3]; for(int a = 0; a < 3; a++) P[k][a] = q->p[a] + q->s * (v[0]*r3[a] + v[1]*q->u[a] + v[2]*q->f[a]); ok &= proj(P[k], &sx[k], &sy[k], &sz[k]); }
            if(!ok) continue; float e1[3], e2[3], n[3]; for(int a = 0; a < 3; a++){ e1[a] = P[1][a]-P[0][a]; e2[a] = P[2][a]-P[0][a]; }
            n[0] = e1[1]*e2[2]-e1[2]*e2[1]; n[1] = e1[2]*e2[0]-e1[0]*e2[2]; n[2] = e1[0]*e2[1]-e1[1]*e2[0]; float l = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]) + 1e-9f;
            float d = fabsf((n[0]*SUN[0]+n[1]*SUN[1]+n[2]*SUN[2]) / l); uint32_t c = m->tcol ? m->tcol[t] : q->col;
            tri(sx[0], sy[0], sz[0], sx[1], sy[1], sz[1], sx[2], sy[2], sz[2], shade(c, 0.35f + 0.85f * d)); } }
    // ships
    for(int i = 0; i < nS; i++){ const SrShip *s = &S[i]; float sx, sy, cz; bgonly = s->far; if(!proj(s->p, &sx, &sy, &cz)) continue;
        float rp = s->size * FOC / cz / GX; if(sx + rp * 1.5f < 0 || sx - rp * 1.5f > W || sy + rp * 1.5f < YL || sy - rp * 1.5f >= YH) continue; if((int)sy >= lo && (int)sy < hi) drawn++;
        int teamc = s->team ? 0xFF7050 : 0x70C8FF;
        if(rp < dotR){                                                      // a dot: two tones, brighter in the middle
            float a = rp < 0.6f ? 0.55f : 0.9f; add_px((int)sx, (int)sy, s->col, a + 0.3f); if(rp > 1.8f){ add_px((int)sx - 1, (int)sy, s->col, a * 0.6f); add_px((int)sx + 1, (int)sy, s->col, a * 0.6f); } if(ultra && rp > 0.9f){ add_px((int)sx + 1, (int)sy, s->col, a * 0.4f); add_px((int)sx, (int)sy + 1, s->col, a * 0.4f); }
            if(s->thrust > 0 && ultra) add_px((int)sx, (int)sy, teamc, 0.25f); continue; }
        float r3[3] = { s->f[1]*s->u[2]-s->f[2]*s->u[1], s->f[2]*s->u[0]-s->f[0]*s->u[2], s->f[0]*s->u[1]-s->f[1]*s->u[0] };
        if(rp < arrR){                                                         // an arrow: one lit triangle, nose to the two tails
            float P[3][3], X[3], Y[3], Zc[3]; float L = s->size; int ok = 1;
            for(int a = 0; a < 3; a++){ P[0][a] = s->p[a] + s->f[a] * L; P[1][a] = s->p[a] - s->f[a] * L * .5f - r3[a] * L * .8f; P[2][a] = s->p[a] - s->f[a] * L * .5f + r3[a] * L * .8f; }
            for(int k = 0; k < 3; k++) ok &= proj(P[k], &X[k], &Y[k], &Zc[k]);
            float d = fabsf(s->u[0]*SUN[0] + s->u[1]*SUN[1] + s->u[2]*SUN[2]);
            if(ok){ tri(X[0], Y[0], Zc[0], X[1], Y[1], Zc[1], X[2], Y[2], Zc[2], shade(s->col, 0.55f + 0.6f * d)); if(fabsf((X[1]-X[0])*(Y[2]-Y[0])-(X[2]-X[0])*(Y[1]-Y[0])) < 3) put_z((int)sx, (int)sy, cz, s->col); }
            if(s->thrust > 0 && rp > 3){ float tx, ty, tz; float tp[3]; for(int a = 0; a < 3; a++) tp[a] = s->p[a] - s->f[a] * L * .7f; if(proj(tp, &tx, &ty, &tz)) glow(tx, ty, rp * .25f + .5f, teamc, .5f * s->thrust); }
            continue; }
        const Mesh *m = &mesh[s->kind % SR_KINDS][ultra]; float Pw[24][3], X[24], Y[24], Zc[24]; int okv[24];
        for(int v = 0; v < m->nv; v++){ for(int a = 0; a < 3; a++) Pw[v][a] = s->p[a] + s->size * (m->v[v][0]*r3[a] + m->v[v][1]*s->u[a] + m->v[v][2]*s->f[a]); okv[v] = proj(Pw[v], &X[v], &Y[v], &Zc[v]); }
        for(int t = 0; t < m->nt; t++){ int a = m->t[t][0], b = m->t[t][1], c = m->t[t][2]; if(!okv[a] || !okv[b] || !okv[c]) continue;
            float e1[3], e2[3], n[3]; for(int k = 0; k < 3; k++){ e1[k] = Pw[b][k]-Pw[a][k]; e2[k] = Pw[c][k]-Pw[a][k]; }
            n[0] = e1[1]*e2[2]-e1[2]*e2[1]; n[1] = e1[2]*e2[0]-e1[0]*e2[2]; n[2] = e1[0]*e2[1]-e1[1]*e2[0]; float l = sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]) + 1e-9f;
            float vd[3] = { E[0]-Pw[a][0], E[1]-Pw[a][1], E[2]-Pw[a][2] }; if(n[0]*vd[0]+n[1]*vd[1]+n[2]*vd[2] < 0) continue;     // back face
            float d = (n[0]*SUN[0]+n[1]*SUN[1]+n[2]*SUN[2]) / l; float k = 0.32f + 0.75f * (d > 0 ? d : 0) + 0.12f * (d < 0 ? -d : 0);   // sun + a little bounce light
            if(ultra){ float vl = sqrtf(vd[0]*vd[0]+vd[1]*vd[1]+vd[2]*vd[2]) + 1e-6f; float hv[3] = { SUN[0] + vd[0]/vl, SUN[1] + vd[1]/vl, SUN[2] + vd[2]/vl }; float hl = sqrtf(hv[0]*hv[0]+hv[1]*hv[1]+hv[2]*hv[2]) + 1e-6f;
                float sp = (n[0]*hv[0]+n[1]*hv[1]+n[2]*hv[2]) / (l * hl); if(sp > 0){ sp *= sp; sp *= sp; sp *= sp; k += sp * 0.9f; } }
            uint32_t col = (t >= 8 && m == &mesh[SR_FIGHTER][1] && t >= m->nt - 2) ? 0x9FE8FF : s->col;          // the canopy is glass
            tri(X[a], Y[a], Zc[a], X[b], Y[b], Zc[b], X[c], Y[c], Zc[c], shade(col, k)); }
        for(int e = 0; e < m->ne; e++){ float ep[3]; for(int a = 0; a < 3; a++) ep[a] = s->p[a] + s->size * (m->eng[e][0]*r3[a] + m->eng[e][1]*s->u[a] + m->eng[e][2]*s->f[a]);
            float ex, ey, ez; if(!proj(ep, &ex, &ey, &ez)) continue; float gr = rp * (ultra ? .16f : .12f) + 1;
            glow(ex, ey, gr, teamc, .55f + .35f * s->thrust); glow(ex, ey, gr * .45f, 0xFFFFFF, .5f * s->thrust);
            if(ultra && !s->far){ float tp[3]; for(int a = 0; a < 3; a++) tp[a] = ep[a] - s->f[a] * s->size * (.6f + .5f * s->thrust); float tx, ty, tz; if(proj(tp, &tx, &ty, &tz)) lineadd(ex, ey, tx, ty, teamc, gr * .6f, .35f); } }
        if(s->firing){ float mp[3]; for(int a = 0; a < 3; a++) mp[a] = s->p[a] + s->f[a] * s->size * 1.05f; float mx, my, mz; if(proj(mp, &mx, &my, &mz)) glow(mx, my, rp * .18f + 1, 0xFFF0C0, .9f); }
        if(s->hit > 0){                                                     // shield: a shimmering ring of light round the ship
            int n = (int)(rp * 9) + 16; if(n > 600) n = 600; uint32_t sd = (uint32_t)(i * 977) ^ (uint32_t)(s->hit * 1000);
            for(int j = 0; j < n; j++){ float a = j * 6.2832f / n, wob = 1.15f + 0.05f * sinf(a * 7 + s->hit * 20); (void)sd;
                glow(sx + cosf(a) * rp * wob, sy + sinf(a) * rp * wob * .8f, ultra ? 1.0f : .7f, 0x80D0FF, .22f * s->hit); } } }
    bgonly = 0;
    // beams (after the hulls: light)
    for(int i = 0; i < nB; i++){ float ax, ay, az, bx, by, bz; bgonly = Bm[i].far; if(!proj(Bm[i].a, &ax, &ay, &az) || !proj(Bm[i].b, &bx, &by, &bz)) continue;
        float w = Bm[i].w * (ultra ? 9.f : 6.f) * FOC * 2 / (az + bz) / GX; if(w > 5) w = 5; if(w < .5f) w = .5f; lineadd(ax, ay, bx, by, Bm[i].col, w, ultra ? .6f : .8f); if(ultra) lineadd(ax, ay, bx, by, 0xFFFFFF, 1, .35f); }
    bgonly = 0;
    // explosions: fireball, shock ring, debris that flies out and cools
    for(int i = 0; i < nBo; i++){ const Boom *b = &Bo[i]; float sx, sy, cz; if(!proj(b->p, &sx, &sy, &cz)) continue; float rp = b->r * FOC / cz / GX, t = b->t;
        float fire = rp * (0.4f + 1.2f * t), a = 1 - t; glow(sx, sy, fire, t < .3f ? 0xFFF4C0 : 0xFF9030, a * 1.1f); glow(sx, sy, fire * .5f, 0xFFFFFF, a * a);
        if(ultra){ int n = (int)(rp * 4 * t) + 8; if(n > 300) n = 300; for(int j = 0; j < n; j++){ float an = j * 6.2832f / n; glow(sx + cosf(an) * rp * 2.2f * t, sy + sinf(an) * rp * 2.2f * t * .7f, 1, 0xA0C0FF, .25f * a); } }
        uint32_t sd = b->seed; int nd = ultra ? 24 : 12;
        for(int j = 0; j < nd; j++){ float an = frnd(&sd) * 6.2832f, sp = 0.6f + frnd(&sd) * 1.6f, d = rp * sp * 2.5f * t;
            float dx = sx + cosf(an) * d, dy = sy + sinf(an) * d * .8f + t * t * rp * .3f; uint32_t c = t < .5f ? 0xFFD080 : 0x806050;
            if(ultra){ float d0 = rp * sp * 2.5f * (t - .06f > 0 ? t - .06f : 0); lineadd(sx + cosf(an) * d0, sy + sinf(an) * d0 * .8f, dx, dy, c, 1, .5f * a); } else add_px((int)dx, (int)dy, c, a); } }
    __atomic_add_fetch((int*)vd, drawn, __ATOMIC_RELAXED);
}
static int sr_render_(uint32_t *out, int w, int h, const uint8_t *mask, int fade);
int sr_render(uint32_t *out, int w, int h, const uint8_t *mask, int fade){
    if(!have_shown) return 0;
    SrShip *s0 = S; int n0 = nS, c0 = capS; Beam *b0 = Bm; int nb0 = nB, cb0 = capB; Boom *o0 = Bo; int no0 = nBo, co0 = capBo; static MeshI mi0[64]; memcpy(mi0, Mi, sizeof Mi); int nm0 = nM, md0 = mode;
    float e0[3], r0[3], u0[3], f0[3], sn0[3], fo = FOC, cx = CX, cy = CY, vb = VB; memcpy(e0, E, 12); memcpy(r0, R, 12); memcpy(u0, U, 12); memcpy(f0, F, 12); memcpy(sn0, SUN, 12);
    use_shown(); int d = sr_render_(out, w, h, mask, fade);
    S = s0; nS = n0; capS = c0; Bm = b0; nB = nb0; capB = cb0; Bo = o0; nBo = no0; capBo = co0; memcpy(Mi, mi0, sizeof Mi); nM = nm0; mode = md0;
    memcpy(E, e0, 12); memcpy(R, r0, 12); memcpy(U, u0, 12); memcpy(F, f0, 12); memcpy(SUN, sn0, 12); FOC = fo; CX = cx; CY = cy; VB = vb; return d; }
static int sr_render_(uint32_t *out, int w, int h, const uint8_t *mask, int fade){
    double t0 = now_ms();
    O = out; W = w; H = h; MK = mask; GX = 240.0f / w; GY = 160.0f / h; VBp = (int)(VB / GY); if(VBp > H) VBp = H;
    if(!mesh_ok) build_meshes();
    int drawn = 0;
    YL = 0; YH = VBp;
    if(mode == SR_CLASSIC){ drawn = render_classic(); goto done; }
    { size_t need = (size_t)W * H; if(need > zcap){ free(Z); Z = (float*)malloc(need * sizeof(float)); zcap = need; } }
    if(PAR && NTH > 1 && W * VBp > 200000){ int ch = (VBp + NTH - 1) / NTH; PAR(VBp, ch, sr_band, &drawn); }
    else sr_band(&drawn, 0, VBp);
done:
    if(fade){ /* the screen fade also covers the ships (the compositor applies it to the whole picture already: nothing here) */ }
    last_ms = now_ms() - t0;
    return drawn;
}
