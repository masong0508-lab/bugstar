// giant_test.c: command line driver for the tests in pc/giant/test.sh (the Python reference compares against what it prints).
#define _POSIX_C_SOURCE 200809L
#include "giant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int prog(gu64 d, gu64 t, void *u){ (void)u; static int last = -1; int p = (int)(d * 100 / t); if(p != last && p % 10 == 0){ fprintf(stderr, "  %d%%\n", p); } last = p; return 0; }
int main(int argc, char **argv){
    if(argc < 2){ fprintf(stderr, "usage: giant_test cmd ...\n"); return 2; }
    const char *c = argv[1];
    if(!strcmp(c, "tokens")){ char b[64]; for(int v = 0; v < 1000; v++){ gcw_tok(v, 0, b); printf("%d %s ", v, b); gcw_tok(v, 1, b); printf("%s ", b); gen_chunk(v, b); printf("%s\n", b); } return 0; }
    if(!strcmp(c, "place")){ static char b[1 << 16]; for(int i = 2; i < argc; i++){ gu64 n = gcw_place(argv[i], b, sizeof b); printf("%s %llu %s\n", argv[i], n, n < sizeof b ? b : "(long)"); } return 0; }
    if(!strcmp(c, "en")){ char b[600]; for(int i = 2; i < argc; i++){ gen_u64(strtoull(argv[i], 0, 10), b, sizeof b); printf("%s\n", b); } return 0; }
    if(!strcmp(c, "sha")){ FILE *f = fopen(argv[2], "rb"); GiSha s; char hx[65]; static unsigned char b[1 << 16]; size_t n; gsha_init(&s); while((n = fread(b, 1, sizeof b, f)) > 0) gsha_add(&s, b, n); gsha_end(&s, hx); printf("%s\n", hx); return 0; }
    if(!strcmp(c, "ds")){ char q[200], o[200]; int r; gds_div3(argv[2], q, &r); gds_sub1(argv[2], o); printf("%s %d %s\n", q, r, o); return 0; }
    if(argc > 2 && !strcmp(argv[2], "-d")){ gi_set_dir(argv[3]); argv += 2; argc -= 2; }
    int which = (argc > 2 && !strcmp(argv[2], "count")) ? GI_COUNT : GI_POW;
    if(!strcmp(c, "read")){                       // read count|pow FILE : lines "pos n"
        if(gi_open(which)){ fprintf(stderr, "not ready\n"); return 1; }
        FILE *f = fopen(argv[3], "r"); unsigned long long p; int n; static char b[1 << 20];
        while(fscanf(f, "%llu %d", &p, &n) == 2){ int g = gi_read(which, p, n, b); printf("%.*s\n", g, b); }
        return 0; }
    if(!strcmp(c, "export")){ if(gi_open(which)){ fprintf(stderr, "not ready\n"); return 1; } char hx[65]; gu64 by; double t = now(); int rc = gi_export(which, argv[3], prog, 0, hx, &by); printf("%d %llu %s %.1fs\n", rc, by, hx, now() - t); return rc; }
    if(!strcmp(c, "seek")){                       // seek count|pow N : N random reads of 600 digits, time
        if(gi_open(which)){ fprintf(stderr, "not ready\n"); return 1; } int N = atoi(argv[3]); static char b[700]; unsigned long long x = 88172645463325252ull; double t = now(); unsigned long long sum = 0;
        for(int i = 0; i < N; i++){ x ^= x << 13; x ^= x >> 7; x ^= x << 17; gi_read(which, x % (gi_digits(which) - 600), 600, b); sum += (unsigned char)b[0]; }
        double dt = now() - t; printf("%d seeks of 600 digits in %.3f s = %.1f microseconds each (%llu)\n", N, dt, dt / N * 1e6, sum); return 0; }
    if(!strcmp(c, "v3")){                         // v3 -d DIR cmd...
        double t = now(); if(gv3_open()){ fprintf(stderr, "v3 not ready\n"); return 1; } fprintf(stderr, "v3 open %.1fs\n", now() - t);
        const char *s = argv[2];
        if(!strcmp(s, "total")){ printf("%llu %llu\n", gv3_parts(), gv3_chars()); return 0; }
        if(!strcmp(s, "read")){ FILE *f = fopen(argv[3], "r"); unsigned long long p; int n; static char b[1 << 20]; while(fscanf(f, "%llu %d", &p, &n) == 2){ int g = gv3_read(p, n, b); printf("%.*s\n", g, b); } return 0; }
        if(!strcmp(s, "info")){ for(int i = 3; i < argc; i++){ V3Info v; gu64 p = strtoull(argv[i], 0, 10); if(gv3_info(p, &v)){ printf("%llu none\n", p); continue; } printf("%llu j=%llu start=%llu len=%llu off=%llu coef=%d a=%d nch=%llu pl=%llu seg=%d chunk=%llu\n", p, v.j, v.start, v.len, v.off, v.coef, v.a, v.nchunks, v.placelen, v.seg, v.chunk); } return 0; }
        if(!strcmp(s, "term")){ for(int i = 3; i < argc; i++){ gu64 j = strtoull(argv[i], 0, 10); printf("%llu start=%llu len=%llu\n", j, gv3_term_start(j), gv3_term_len(j)); } return 0; }
        if(!strcmp(s, "seek")){ int N = atoi(argv[3]); static char b[700]; unsigned long long x = 88172645463325252ull; double t0 = now(); unsigned long long sum = 0;
            for(int i = 0; i < N; i++){ x ^= x << 13; x ^= x >> 7; x ^= x << 17; gv3_read(x % (gv3_chars() - 700), 600, b); sum += (unsigned char)b[0]; }
            double dt = now() - t0; printf("%d v3 seeks of 600 letters in %.3f s = %.2f ms each (%llu)\n", N, dt, dt / N * 1e3, sum); return 0; }
        if(!strcmp(s, "export")){ char hx[65]; int rc = gv3_export(argv[3], strtoull(argv[4], 0, 10), strtoull(argv[5], 0, 10), 0, 0, hx); printf("%d %s\n", rc, hx); return rc; }
    }
    fprintf(stderr, "unknown command\n"); return 2;
}
