// gpow.inc (included by giant.c): the digits of 2^(2^32) worked out exactly on the spot, with no big file.
//
// The real "compressed form" of the number is its definition, 2^(2^32). Any stretch of its 1,292,913,987 digits is worked out from it:
//   * a stretch in the first half: the HEAD method. 2 is squared 32 times keeping only the top P limbs (base 100,000) of each square, rounding down.
//     That gives a number L with L x 100000^e <= 2^(2^32). The error is bounded: after t rounded squarings the relative error is at most
//     2^(t+1) / 100000^(P-1), so 2^(2^32) < (L + 100000^3) x 100000^e. A digit is used only when L and L + 100000^3 agree on it (PROVED exact),
//     and the top 5 x 4 guard digits are never used.
//   * a stretch in the second half: the TAIL method. 2 is squared 32 times modulo 100000^T (keep the low T limbs): exact integer arithmetic.
//   * squaring: number-theoretic transforms modulo two primes (3 x 2^30 + 1 and 13 x 2^28 + 1, Montgomery arithmetic), joined by the
//     Chinese remainder theorem; the two primes run on two threads. The biggest square (129 million limbs) uses transforms of 2^28 points.
//   * every block of 1,000,000 digits worked out is checked against GP_BLK (gpow_blocks.h: SHA-256 of each block, made from the digit
//     file that Python's decimal module (libmpdec) built and that was checked modulo 10 primes). A block that does not match is not used.
// Digits are kept in up to two BANDS in memory (block aligned, at most 120 million digits each, 10 bits per 3 digits: at most 100 MB in all),
// and optionally one band on disk (the FAST CACHE, 240 million digits around the middle, 100,000,000 bytes, built only when asked).
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#include "gpow_blocks.h"
typedef uint32_t gp32; typedef gu64 gp64;
#define GPB 100000u
#define GP_SPLIT 646000000ull                  // digits before this: HEAD method; from here: TAIL method (both fit transforms of 2^28)
#define GP_BLKD 1000000ull
typedef struct { gp32 p, pinv, r2, g; } GpPrime;
static GpPrime gpp[2] = {{3221225473u, 0, 0, 5}, {3489660929u, 0, 0, 3}};
static gp64 gp_powm(gp64 a, gp64 e, gp64 m){ gp64 r = 1; a %= m; while(e){ if(e & 1) r = r * a % m; a = a * a % m; e >>= 1; } return r; }   // (m < 2^32)
static void gp_init(void){
    static int done; if(done) return;
    for(int i = 0; i < 2; i++){ gp32 p = gpp[i].p, x = p; for(int k = 0; k < 5; k++) x *= 2u - p * x; gpp[i].pinv = x; gp64 r = ((gp64)1 << 32) % p; gpp[i].r2 = (gp32)(r * r % p); }
    done = 1;
}
static inline gp32 gp_red(gp64 t, gp32 p, gp32 pinv){ gp32 m = (gp32)t * pinv; gp64 mp = (gp64)m * p; gp32 th = (gp32)(t >> 32), mh = (gp32)(mp >> 32); return th >= mh ? th - mh : th - mh + p; }
static inline gp32 gp_mul(gp32 a, gp32 b, gp32 p, gp32 pinv){ return gp_red((gp64)a * b, p, pinv); }
static inline gp32 gp_add(gp32 a, gp32 b, gp32 p){ gp64 s = (gp64)a + b; return (gp32)(s >= p ? s - p : s); }
static inline gp32 gp_sub(gp32 a, gp32 b, gp32 p){ return a >= b ? a - b : (gp32)((gp64)a + p - b); }

static volatile int gp_cancel;
typedef struct { gi_progress_fn pr; void *ud; gp64 done, total; } GpProg;
static GpProg *gp_curP;                                                      // progress of the computation running now (reported by the main thread)
static int gp_tick(GpProg *P, gp64 extra){ if(!P || !P->pr) return 0; if(P->pr(P->done + extra, P->total ? P->total : 1, P->ud)) gp_cancel = 1; return gp_cancel; }
// forward transform (natural order in, bit-reversed out) and inverse (bit-reversed in, natural out), values in Montgomery form
static void gp_ntt(gp32 *a, gp64 n, int pi, int inv, volatile gp64 *prog){
    gp32 p = gpp[pi].p, pinv = gpp[pi].pinv, r2 = gpp[pi].r2; int lg = 0; while(((gp64)1 << lg) < n) lg++;
    gp32 one = gp_red(r2, p, pinv);                                           // R mod p
    gp64 g = gp_powm(gpp[pi].g, (p - 1) >> lg, p); if(inv) g = gp_powm(g, p - 2, p);   // a root of order n
    gp32 wn[64]; gp64 wp[64]; { gp64 w = g; for(int s = lg; s >= 1; s--){ wp[s] = w; wn[s] = gp_red((gp64)(gp32)w * r2, p, pinv); w = w * w % p; } }   // order 2^s
    // stages s <= sb work inside blocks of 2^sb points (they fit the cache): those are done block by block, all of them at once.
    // twiddle tables: small stages from one table (made once), big stages a segment of GP_SEG at a time (no long chain of multiplications)
    enum { GP_SB = 14, GP_SEG = 4096 };
    int sb = lg < GP_SB ? lg : GP_SB; gp64 bs = (gp64)1 << sb;
    gp32 *tws = malloc(((size_t)1 << sb) * 4 + 4); if(!tws){ gp_cancel = 1; return; }
    for(int s = 1; s <= sb; s++){ gp32 *t = tws + ((1 << (s - 1)) - 1), w = one; for(gp64 j = 0; j < ((gp64)1 << (s - 1)); j++){ t[j] = w; w = gp_mul(w, wn[s], p, pinv); } }
    gp32 tw[GP_SEG];
    #define GP_BIG(s_) do { int s = (s_); gp64 len = (gp64)1 << (s - 1); \
        for(gp64 j0 = 0; j0 < len; j0 += GP_SEG){ gp64 m = len - j0 < GP_SEG ? len - j0 : GP_SEG; \
            gp32 w = gp_red((gp64)(gp32)gp_powm(wp[s], j0, p) * r2, p, pinv); for(gp64 j = 0; j < m; j++){ tw[j] = w; w = gp_mul(w, wn[s], p, pinv); } \
            for(gp64 i = 0; i < n; i += 2 * len){ gp32 *x = a + i + j0, *y = x + len; \
                if(!inv) for(gp64 j = 0; j < m; j++){ gp32 u = x[j], v = y[j]; x[j] = gp_add(u, v, p); y[j] = gp_mul(gp_sub(u, v, p), tw[j], p, pinv); } \
                else for(gp64 j = 0; j < m; j++){ gp32 u = x[j], v = gp_mul(y[j], tw[j], p, pinv); x[j] = gp_add(u, v, p); y[j] = gp_sub(u, v, p); } } } \
        if(prog){ *prog += n; if(pi == 0 && gp_curP) gp_tick(gp_curP, *prog); } } while(0)
    #define GP_SMALL(blk) do { gp32 *b_ = (blk); \
        for(int st = 0; st < sb; st++){ int s = inv ? st + 1 : sb - st; gp64 len = (gp64)1 << (s - 1); const gp32 *t = tws + (len - 1); \
            for(gp64 i = 0; i < bs; i += 2 * len){ gp32 *x = b_ + i, *y = x + len; \
                if(!inv) for(gp64 j = 0; j < len; j++){ gp32 u = x[j], v = y[j]; x[j] = gp_add(u, v, p); y[j] = gp_mul(gp_sub(u, v, p), t[j], p, pinv); } \
                else for(gp64 j = 0; j < len; j++){ gp32 u = x[j], v = gp_mul(y[j], t[j], p, pinv); x[j] = gp_add(u, v, p); y[j] = gp_sub(u, v, p); } } } } while(0)
    if(!inv) for(int sg = lg; sg > sb && !gp_cancel; sg--) GP_BIG(sg);
    for(gp64 b = 0; b < n && !gp_cancel; b += bs){
        GP_SMALL(a + b);
        if(prog && ((b / bs) & 1023) == 1023){ *prog += 1024 * bs * (gp64)sb; if(pi == 0 && gp_curP) gp_tick(gp_curP, *prog); }
    }
    if(prog) *prog += ((n / bs) & 1023) * bs * (gp64)sb;
    if(inv) for(int sg = sb + 1; sg <= lg && !gp_cancel; sg++) GP_BIG(sg);
    #undef GP_BIG
    #undef GP_SMALL
    free(tws);
}
typedef struct { gp32 *buf; gp64 n; const gp32 *a; gp64 na; int pi; volatile gp64 prog; } GpJob;
static void gp_job(GpJob *J){
    gp32 p = gpp[J->pi].p, pinv = gpp[J->pi].pinv, r2 = gpp[J->pi].r2;
    for(gp64 i = 0; i < J->na; i++) J->buf[i] = gp_red((gp64)J->a[i] * r2, p, pinv);
    memset(J->buf + J->na, 0, (size_t)(J->n - J->na) * 4);
    gp_ntt(J->buf, J->n, J->pi, 0, &J->prog); if(gp_cancel) return;
    for(gp64 i = 0; i < J->n; i++) J->buf[i] = gp_mul(J->buf[i], J->buf[i], p, pinv);
    gp_ntt(J->buf, J->n, J->pi, 1, &J->prog); if(gp_cancel) return;
    gp32 ninv = (gp32)gp_powm(J->n % p, p - 2, p);                             // plain n^-1: red(A.R x ninv) = A / n
    for(gp64 i = 0; i < J->n; i++) J->buf[i] = gp_red((gp64)J->buf[i] * ninv, p, pinv);
}
#ifdef _WIN32
static DWORD WINAPI gp_thread(LPVOID v){ gp_job((GpJob *)v); return 0; }
#else
static void *gp_thread(void *v){ gp_job((GpJob *)v); return 0; }
#endif

static gp64 gp_npow(gp64 m){ gp64 n = 1; while(n < m) n <<= 1; return n; }
static gp64 gp_cost(gp64 na){ gp64 n = gp_npow(2 * na); int lg = 0; while(((gp64)1 << lg) < n) lg++; return 2 * n * (gp64)lg; }   // per prime, both transforms

// a := a^2 (na limbs). Returns the new limb count, 0 when cancelled or out of memory. *a is reallocated.
static gp64 gp_square(gp32 **pa, gp64 na, GpProg *P){
    gp_init(); gp32 *a = *pa;
    if(na <= 64){                                                            // small: schoolbook
        gp64 *t = calloc(2 * na + 1, 8); if(!t) return 0;
        for(gp64 i = 0; i < na; i++) for(gp64 j = 0; j < na; j++) t[i + j] += (gp64)a[i] * a[j];
        gp64 c = 0, m = 2 * na; gp32 *o = malloc((size_t)(m + 1) * 4); if(!o){ free(t); return 0; }
        for(gp64 i = 0; i < m; i++){ gp64 v = t[i] + c; o[i] = (gp32)(v % GPB); c = v / GPB; } o[m] = (gp32)c; m++;
        while(m > 1 && !o[m - 1]) m--; free(t); free(a); *pa = o; return m;
    }
    gp64 n = gp_npow(2 * na), cost = gp_cost(na);
    gp32 *b1 = malloc((size_t)n * 4), *b2 = malloc((size_t)n * 4);
    if(!b1 || !b2){ free(b1); free(b2); return 0; }
    GpJob J1 = {b1, n, a, na, 0, 0}, J2 = {b2, n, a, na, 1, 0};
#ifdef _WIN32
    HANDLE th = CreateThread(0, 0, gp_thread, &J2, 0, 0); int threaded = th != 0;
#else
    pthread_t th; int threaded = pthread_create(&th, 0, gp_thread, &J2) == 0;
#endif
    if(!threaded) gp_job(&J2);
    // run prime 1 here, a level at a time is too fine to report; report from the other thread's counter while this one works
    gp_curP = P; gp_job(&J1); gp_curP = 0;
#ifdef _WIN32
    if(threaded){ WaitForSingleObject(th, INFINITE); CloseHandle(th); }
#else
    if(threaded) pthread_join(th, 0);
#endif
    if(P) P->done += cost;
    if(gp_cancel || gp_tick(P, 0)){ free(b1); free(b2); return 0; }
    gp32 p1 = gpp[0].p, p2 = gpp[1].p; gp64 inv = gp_powm(p1 % p2, p2 - 2, p2), c = 0, m = 2 * na;
    for(gp64 i = 0; i < m; i++){
        gp64 x1 = b1[i], x2 = b2[i], t = x2 >= x1 ? x2 - x1 : x2 + p2 - x1; t = t * inv % p2;
        gp64 v = x1 + (gp64)p1 * t + c; b1[i] = (gp32)(v % GPB); c = v / GPB;
    }
    free(b2);
    gp32 *o = realloc(b1, (size_t)(m + 2) * 4); if(!o) o = b1;
    while(c){ o[m++] = (gp32)(c % GPB); c /= GPB; }
    while(m > 1 && !o[m - 1]) m--;
    free(a); *pa = o; return m;
}
static int gp_ndig(gp32 v){ int d = 1; while(v >= 10){ v /= 10; d++; } return d; }

// ---- HEAD: the first nd digits. Out: L (little-endian limbs), its limb count, and how many top digits are proved (>= nd, or 0 on failure)
static gp64 gp_head_cost(gp64 nd){ gp64 P = nd / 5 + 6, n = 1, c = 0; double lim = 2; for(int k = 0; k < 32; k++){ gp64 m = (gp64)(lim * 0.30103 / 5) + 2; if(m > P) m = P; if(m > 64) c += gp_cost(m); lim *= 2; (void)n; } return c; }
static int gp_head(gp64 nd, gp32 **Lo, gp64 *nLo, GpProg *P){
    gp64 Pl = nd / 5 + 6; gp32 *L = malloc(8); gp64 nL = 1; L[0] = 2; int t = 0;
    for(int k = 0; k < 32; k++){
        nL = gp_square(&L, nL, P); if(!nL){ free(L); return 1; }
        if(nL > Pl){ gp64 s = nL - Pl; memmove(L, L + s, (size_t)Pl * 4); nL = Pl; t++; gp32 *r = realloc(L, (size_t)Pl * 4); if(r) L = r; }
    }
    (void)t;                                                                 // t <= 32: error < 100000 x 2^33 < 100000^3 units of the last limb
    *Lo = L; *nLo = nL; return 0;
}
// ---- TAIL: 2^(2^32) mod 100000^T
static gp64 gp_tail_cost(gp64 d){ gp64 T = (d + 4) / 5 + 1, c = 0; double lim = 2; for(int k = 0; k < 32; k++){ gp64 m = (gp64)(lim * 0.30103 / 5) + 2; if(m > T) m = T; if(m > 64) c += gp_cost(m); lim *= 2; } return c; }
static int gp_tail(gp64 d, gp32 **Ao, gp64 *nAo, GpProg *P){
    gp64 T = (d + 4) / 5 + 1; gp32 *A = malloc(8); gp64 nA = 1; A[0] = 2;
    for(int k = 0; k < 32; k++){
        nA = gp_square(&A, nA, P); if(!nA){ free(A); return 1; }
        if(nA > T){ nA = T; gp32 *r = realloc(A, (size_t)T * 4); if(r) A = r; while(nA > 1 && !A[nA - 1]) nA--; }
    }
    *Ao = A; *nAo = T; if(nA < T){ gp32 *B = realloc(A, (size_t)T * 4); if(!B){ free(A); return 2; } memset(B + nA, 0, (size_t)(T - nA) * 4); *Ao = B; }
    return 0;
}

// ---- digits [pos, pos+n) through emit(digits, count, ud) in order. 0 ok, 1 cancelled, 2 out of memory, 3 could not prove (never seen)
typedef int (*gp_emit_fn)(const char *d, gp64 n, void *ud);
static int gp_emit_limbs_desc(const gp32 *L, gp64 nL, gp64 skip, gp64 n, gp_emit_fn emit, void *ud){
    // the decimal digits of L from the top (top limb without leading zeros, the others 5 digits each), from digit skip on, n digits
    enum { CH = 1 << 16 }; static char buf[CH + 8]; int k = 0; int td = gp_ndig(L[nL - 1]);
    gp64 li; int q0;
    if(skip < (gp64)td){ li = nL - 1; q0 = (int)skip; } else { li = nL - 2 - (skip - (gp64)td) / 5; q0 = (int)((skip - (gp64)td) % 5); }
    for(;; li--){
        char t[5]; gp32 v = L[li]; int w = li == nL - 1 ? td : 5; for(int q = w - 1; q >= 0; q--){ t[q] = (char)('0' + v % 10); v /= 10; }
        for(int q = q0; q < w && n; q++){ buf[k++] = t[q]; n--; if(k == CH){ if(emit(buf, (gp64)k, ud)) return 1; k = 0; } }
        q0 = 0; if(!n || li == 0) break;
    }
    if(k && emit(buf, (gp64)k, ud)) return 1;
    return 0;
}
static int gp_window(gp64 pos, gp64 n, gp_emit_fn emit, void *ud, gi_progress_fn pr, void *pud){
    gp64 D = GI_POW_DIGITS; if(pos >= D) return 0; if(pos + n > D) n = D - pos;
    gp64 e1 = pos + n < GP_SPLIT ? pos + n : GP_SPLIT, s2 = pos > GP_SPLIT ? pos : GP_SPLIT;
    GpProg P = {pr, pud, 0, 0}; gp_cancel = 0;
    if(pos < e1) P.total += gp_head_cost(e1);
    if(s2 < pos + n) P.total += gp_tail_cost(D - s2);
    if(pos < e1){
        gp32 *L; gp64 nL; int rc = gp_head(e1, &L, &nL, &P); if(rc) return gp_cancel ? 1 : 2;
        // proof: L <= X / 100000^e < L + 100000^3. The carry of +100000^3 reaches limb k at most:
        gp64 k = 3; while(k < nL && L[k] == GPB - 1) k++;
        gp64 lend = (gp64)gp_ndig(L[nL - 1]) + 5 * (nL - 1);                // digits in L
        gp64 safe = lend - 5 * (k + 1);                                       // digits from the top that the error cannot touch
        if(e1 > safe){ free(L); return 3; }
        rc = gp_emit_limbs_desc(L, nL, pos, e1 - pos, emit, ud); free(L); if(rc) return 1;
    }
    if(s2 < pos + n){
        gp64 d = D - s2; gp32 *A; gp64 nA; int rc = gp_tail(d, &A, &nA, &P); if(rc) return gp_cancel ? 1 : 2;
        // digits from the right: X mod 100000^nA as 5 x nA digits (with leading zeros); digit pos from the left is index D-1-pos from the right
        gp64 tot = 5 * nA, skip = tot - (D - s2);                              // digits of A (zero-padded) above digit s2
        gp32 *A2 = realloc(A, (size_t)(nA + 1) * 4); if(!A2){ free(A); return 2; } A = A2; A[nA] = 1;   // a leading 1, then the zero-padded limbs
        gp64 from = pos > s2 ? pos : s2;
        rc = gp_emit_limbs_desc(A, nA + 1, 1 + skip + (from - s2), pos + n - from, emit, ud); free(A); if(rc) return 1;
    }
    return 0;
}

// ================================================================= bands: digits kept in memory, 10 bits for 3 digits
typedef struct { gp64 b0, n; unsigned char *pk; gp64 used; } GpBand;
#define GP_NBAND 2
static GpBand gp_band[GP_NBAND]; static gp64 gp_clock;
static GpBand gp_disk; static FILE *gp_diskf;                              // the optional FAST CACHE on disk (read in place)
static gi_progress_fn gp_hook; static void *gp_hook_ud;
static char gp_err[160];
static inline void gp_put3(unsigned char *pk, gp64 t, unsigned v){ gp64 b = t * 10; for(int i = 0; i < 10; i++, b++){ if(v & (1u << (9 - i))) pk[b >> 3] |= (unsigned char)(0x80 >> (b & 7)); } }
static inline unsigned gp_get3(const unsigned char *pk, gp64 t){ gp64 b = t * 10, by = b >> 3; unsigned w = ((unsigned)pk[by] << 16) | ((unsigned)pk[by + 1] << 8) | pk[by + 2]; return (w >> (14 - (b & 7))) & 1023u; }
static gp64 gp_pkbytes(gp64 n){ return ((n + 2) / 3 * 10 + 7) / 8 + 3; }
typedef struct { unsigned char *pk; gp64 i; char tri[3]; int nt; GiSha sh; gp64 blk; int bad; gp64 b0; } GpFill;
static int gp_fill_emit(const char *d, gp64 n, void *ud){
    GpFill *F = ud;
    for(gp64 k = 0; k < n; k++){
        F->tri[F->nt++] = d[k];
        if(F->nt == 3){ gp_put3(F->pk, F->i / 3, (unsigned)((F->tri[0] - '0') * 100 + (F->tri[1] - '0') * 10 + (F->tri[2] - '0'))); F->nt = 0; }
        F->i++;
    }
    return 0;
}
static int gp_check_band(const GpBand *B){                                  // every whole block in the band against GP_BLK
    char *buf = malloc(GP_BLKD); if(!buf) return 0; int ok = 1;
    for(gp64 s = 0; s < B->n && ok; s += GP_BLKD){
        gp64 g = B->b0 + s; if(g % GP_BLKD) continue; gp64 m = B->n - s < GP_BLKD ? B->n - s : GP_BLKD;
        if(m < GP_BLKD && g + m != GI_POW_DIGITS) continue;                   // only whole blocks (or the real last one)
        for(gp64 i = 0; i < m; i++){ gp64 q = s + i; unsigned v = gp_get3(B->pk, q / 3); int r = (int)(q % 3); buf[i] = (char)('0' + (r == 0 ? v / 100 : r == 1 ? v / 10 % 10 : v % 10)); }
        unsigned char h[32]; gsha256_buf((unsigned char *)buf, m, h); gp64 hv = 0; for(int i = 0; i < 8; i++) hv = hv << 8 | h[i];
        if(hv != GP_BLK[g / GP_BLKD]){ ok = 0; snprintf(gp_err, sizeof gp_err, "BLOCK %llu DOES NOT MATCH THE REFERENCE", (unsigned long long)(g / GP_BLKD)); }
    }
    free(buf); return ok;
}
// work out the band [b0, b0+n) (block aligned, one side of GP_SPLIT). 0 ok
static int gp_make(GpBand *B, gp64 b0, gp64 n, gi_progress_fn pr, void *ud){
    unsigned char *pk = calloc((size_t)gp_pkbytes(n), 1); if(!pk) return 2;
    GpFill F; memset(&F, 0, sizeof F); F.pk = pk; F.b0 = b0;
    int rc = gp_window(b0, n, gp_fill_emit, &F, pr, ud);
    if(F.nt){ while(F.nt < 3) F.tri[F.nt++] = '0'; gp_put3(pk, F.i / 3, (unsigned)((F.tri[0] - '0') * 100 + (F.tri[1] - '0') * 10 + (F.tri[2] - '0'))); }
    if(rc || F.i != n){ free(pk); if(!rc) snprintf(gp_err, sizeof gp_err, "SHORT RESULT"); return rc ? rc : 3; }
    GpBand nb = {b0, n, pk, ++gp_clock};
    if(!gp_check_band(&nb)){ free(pk); return 4; }
    free(B->pk); *B = nb; return 0;
}
// the band to make for a digit: block aligned, on one side of the split, size grows with how deep it is (deeper = dearer to make, so keep more)
static void gp_plan(gp64 pos, gp64 *b0, gp64 *n){
    gp64 D = GI_POW_DIGITS, lo = pos < GP_SPLIT ? 0 : GP_SPLIT, hi = pos < GP_SPLIT ? GP_SPLIT : D;
    gp64 depth = pos < GP_SPLIT ? pos : D - pos, len = depth / 2;
    if(len < 2 * GP_BLKD) len = 2 * GP_BLKD; if(len > 120 * GP_BLKD) len = 120 * GP_BLKD;
    gp64 s = pos > len / 2 ? pos - len / 2 : 0; s -= s % GP_BLKD; if(s < lo) s = lo;
    gp64 e = s + len; if(e > hi){ e = hi; s = e > len ? e - len : 0; s -= s % GP_BLKD; if(s < lo) s = lo; }
    *b0 = s; *n = e - s;
}
static int gp_have(const GpBand *B, gp64 pos){ return B->pk && pos >= B->b0 && pos < B->b0 + B->n; }
static int gp_read_band(const GpBand *B, gp64 pos, int n, char *out){
    int got = 0;
    while(got < n && gp_have(B, pos)){ gp64 q = pos - B->b0; unsigned v = gp_get3(B->pk, q / 3); int r = (int)(q % 3); out[got++] = (char)('0' + (r == 0 ? v / 100 : r == 1 ? v / 10 % 10 : v % 10)); pos++; }
    return got;
}
static int gp_read_disk(gp64 pos, int n, char *out){
    if(!gp_diskf || pos < gp_disk.b0 || pos >= gp_disk.b0 + gp_disk.n) return 0;
    if(pos + (gp64)n > gp_disk.b0 + gp_disk.n) n = (int)(gp_disk.b0 + gp_disk.n - pos);
    gp64 q0 = pos - gp_disk.b0, t0 = q0 / 3, t1 = (q0 + (gp64)n - 1) / 3, by0 = t0 * 10 / 8, by1 = (t1 * 10 + 9) / 8 + 1;
    unsigned char *b = calloc((size_t)(by1 - by0 + 4), 1); if(!b) return 0;
    fseeko(gp_diskf, (off_t)(64 + by0), SEEK_SET); size_t r = fread(b, 1, (size_t)(by1 - by0 + 1), gp_diskf); (void)r;
    for(int i = 0; i < n; i++){ gp64 q = q0 + (gp64)i, t = q / 3, bb = t * 10 - by0 * 8, by = bb >> 3; unsigned w = ((unsigned)b[by] << 16) | ((unsigned)b[by + 1] << 8) | b[by + 2]; unsigned v = (w >> (14 - (bb & 7))) & 1023u; int rr = (int)(q % 3); out[i] = (char)('0' + (rr == 0 ? v / 100 : rr == 1 ? v / 10 % 10 : v % 10)); }
    free(b); return n;
}
