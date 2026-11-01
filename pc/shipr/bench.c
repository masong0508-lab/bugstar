// bench.c: the ship renderer on its own: N ships in a battle (a close group, a middle cloud, a far armada), each mode, timed, and a picture.
//   cc -O2 -o out/srbench shipr/bench.c shipr/shipr.c -lm && out/srbench W H prefix
#include "shipr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
typedef struct { void (*fn)(void*, int, int); void *ctx; int lo, hi; } Job;
static void *runj(void *u){ Job *j = u; j->fn(j->ctx, j->lo, j->hi); return 0; }
static void par(int total, int chunk, void (*fn)(void*, int, int), void *ctx){       // a plain parallel-for for the bench: one thread per chunk
    pthread_t th[64]; Job jb[64]; int n = 0; for(int a = 0; a < total && n < 64; a += chunk, n++){ jb[n].fn = fn; jb[n].ctx = ctx; jb[n].lo = a; jb[n].hi = a + chunk > total ? total : a + chunk; pthread_create(&th[n], 0, runj, &jb[n]); }
    for(int i = 0; i < n; i++) pthread_join(th[i], 0); }
static void backdrop(uint32_t *o, int W, int H){ for(int y = 0; y < H; y++) for(int x = 0; x < W; x++){ uint32_t h = (uint32_t)(x * 7919 + y * 104729); h ^= h >> 13; h *= 0x5bd1e995; h ^= h >> 15;
    int b = 10 + y * 20 / H; o[y * W + x] = (h % 900 == 0) ? 0xC0C8FF : (uint32_t)(b / 2) << 16 | (uint32_t)(b / 2) << 8 | (uint32_t)b; } }
static void scene(int mode, int n, long frame){
    float eye[3] = {0, 0, 0}, r[3] = {1, 0, 0}, u[3] = {0, 0.94f, 0.34f}, f[3] = {0, -0.34f, 0.94f}, sun[3] = {-0.5f, 0.6f, -0.6f};
    sr_begin(mode, eye, r, u, f, 160, 120, 64, 128, sun);
    int near = n < 40 ? n : 40, mid = n / 10 < 400 ? n / 10 : 400; if(mid < 0) mid = 0;
    for(int i = 0; i < near; i++){ SrShip s; memset(&s, 0, sizeof s); int team = i & 1; float a = i * 2.4f;     // a close dogfight
        s.p[0] = cosf(a) * (300 + i * 40) + (team ? 250 : -250); s.p[1] = sinf(a * 1.7f) * 220; s.p[2] = 900 + i * 90;
        float fx = team ? -1 : 1, fz = sinf(a); float l = sqrtf(fx*fx + fz*fz); s.f[0] = fx / l; s.f[2] = fz / l; s.u[1] = 1;
        s.size = (i % 9 == 4) ? 330 : 110; s.kind = (i % 9 == 4) ? SR_HEAVY : team ? SR_FIGHTER : SR_WING; s.team = team; s.col = team ? 0xD84A3A : 0x4A9AF0;
        s.thrust = 0.8f; s.firing = (i % 5) == 0; s.hit = (i % 7) == 3 ? 0.8f : 0; sr_ship(&s);
        if(s.firing){ float b[3] = { s.p[0] + s.f[0] * 1500, s.p[1], s.p[2] + s.f[2] * 1500 + 400 }; sr_beam(s.p, b, team ? 0xFF6040 : 0x60C0FF, 1); } }
    for(int i = 0; i < 4 && i < near; i++){ float p[3] = { -400.f + i * 300, -100.f + i * 60, 1600.f + i * 500 }; sr_boom(p, 120, (i + 1) * 0.2f, 77 + i); }
    sr_armada(mid, 1234, frame, 6000); sr_armada(n - near - mid, 99, frame, 30000); sr_commit(0);
}
int main(int argc, char **argv){
    int W = argc > 1 ? atoi(argv[1]) : 1280, H = argc > 2 ? atoi(argv[2]) : 720; const char *pre = argc > 3 ? argv[3] : "sr";
    int nt = argc > 4 ? atoi(argv[4]) : 1; if(nt > 1) sr_set_par(par, nt);
    uint32_t *o = malloc((size_t)W * H * 4), *bg = malloc((size_t)W * H * 4); backdrop(bg, W, H);
    static const char *mn[3] = {"CLASSIC", "NATIVE", "ULTRA"}; int ns[4] = {100, 1000, 10000, 100000};
    for(int m = 0; m < 3; m++) for(int k = 0; k < 4; k++){ double sum = 0, mx = 0; int drawn = 0, reps = 12;
        for(int rep = 0; rep < reps; rep++){ memcpy(o, bg, (size_t)W * H * 4); scene(m, ns[k], rep * 3); drawn = sr_render(o, W, H, NULL, 0); double t = sr_last_ms(); if(rep){ sum += t; if(t > mx) mx = t; } }
        printf("%-7s %6d ships  %dx%d  avg %6.2f ms  worst %6.2f ms  (drawn %d)\n", mn[m], ns[k], W, H, sum / (reps - 1), mx, drawn);
        if(k < 3){ char fn[300]; snprintf(fn, sizeof fn, "%s_%s_%d.ppm", pre, mn[m], ns[k]); FILE *f = fopen(fn, "wb"); fprintf(f, "P6 %d %d 255\n", W, H);
            for(size_t i = 0; i < (size_t)W * H; i++){ unsigned char p[3] = { o[i] >> 16, o[i] >> 8, o[i] }; fwrite(p, 1, 3, f); } fclose(f); } }
    return 0;
}
