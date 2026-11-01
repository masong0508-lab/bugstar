// giant.c: see giant.h and pc/giant/README.md. Plain C99, no libraries beyond the C library.
#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#ifdef _WIN32_CROSS
#define readlink(a,b,c) (-1)
#endif
#include "giant.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// ================================================================= names
static const char *const SM[10] = {"NI","MI","BI","TRI","QUADRI","QUINTI","SEXTI","SEPTI","OCTI","NONI"};
static const char *const UN[10] = {"","UN","DUO","TRE","QUATTUOR","QUINQUA","SE","SEPTE","OCTO","NOVE"};
static const char *const TE[10] = {"","DECI","VIGINTI","TRIGINTA","QUADRAGINTA","QUINQUAGINTA","SEXAGINTA","SEPTUAGINTA","OCTOGINTA","NONAGINTA"};
static const char *const HU[10] = {"","CENTI","DUCENTI","TRECENTI","QUADRINGENTI","QUINGENTI","SESCENTI","SEPTINGENTI","OCTINGENTI","NONGENTI"};
static const unsigned char TM[10] = {0,1,2|4,1|4,1|4,1|4,1,1,2|8,0};          // N=1 M=2 S=4 X=8: which letter the unit before it may need
static const unsigned char HM[10] = {0,1|8,1,1|4,1|4,1|4,1,1,2|8,0};

int gcw_prefix(int v, char *o){
    o[0] = 0;
    if(v < 10){ strcpy(o, SM[v]); return (int)strlen(o); }
    int u = v % 10, t = (v / 10) % 10, h = v / 100; int m = 0;
    if(u){
        m = t ? TM[t] : HM[h]; strcpy(o, UN[u]);
        if(u == 3 && (m & 12)) strcat(o, "S");
        if(u == 6){ if(m & 4) strcat(o, "S"); else if(m & 8) strcat(o, "X"); }
        if(u == 7 || u == 9){ if(m & 2) strcat(o, "M"); else if(m & 1) strcat(o, "N"); }
    }
    strcat(o, TE[t]); strcat(o, HU[h]);
    return (int)strlen(o);
}
int gcw_tok(int v, int last, char *o){
    int n = gcw_prefix(v, o);
    if(n && (o[n-1] == 'I' || o[n-1] == 'A')) n--;
    strcpy(o + n, last ? "ILLION" : "ILLI");
    return n + (last ? 6 : 4);
}
static unsigned char TLEN[1000];
static int tl_ready;
static void tl_init(void){ if(tl_ready) return; char b[48]; for(int v = 0; v < 1000; v++) TLEN[v] = (unsigned char)gcw_tok(v, 0, b); tl_ready = 1; }
int gcw_toklen(int v){ tl_init(); return TLEN[v]; }

gu64 gcw_place(const char *m, char *out, int cap){
    int d = (int)strlen(m); gu64 total = 0; int w = 0; char b[48];
    while(d > 1 && m[0] == '0'){ m++; d--; }
    if(d == 1 && m[0] == '0'){ const char *s = "THOUSAND"; if(cap > 0){ strncpy(out, s, cap - 1); out[cap-1] = 0; } return 8; }
    int lead = d % 3 ? d % 3 : 3; int p = 0;
    for(int ci = 0; p < d; ci++){
        int len = ci == 0 ? lead : 3, v = 0; for(int i = 0; i < len; i++) v = v * 10 + (m[p + i] - '0'); p += len;
        int n = gcw_tok(v, p >= d, b); total += n;
        for(int i = 0; i < n && w < cap - 1; i++) out[w++] = b[i];
    }
    if(cap > 0) out[w] = 0;
    return total;
}
gu64 gcw_place_len(const char *m){ char t[4]; return gcw_place(m, t, 1); }

int gcw_parts(int v, char p[3][24], char d[3][28]){
    int n = 0;
    if(v < 10){ strcpy(p[0], SM[v]); strcpy(d[0], "THE DIGIT "); d[0][10] = (char)('0' + v); d[0][11] = 0; return 1; }
    int u = v % 10, t = (v / 10) % 10, h = v / 100, m = 0; char tmp[24];
    if(u){
        m = t ? TM[t] : HM[h]; strcpy(tmp, UN[u]);
        if(u == 3 && (m & 12)) strcat(tmp, "S");
        if(u == 6){ if(m & 4) strcat(tmp, "S"); else if(m & 8) strcat(tmp, "X"); }
        if(u == 7 || u == 9){ if(m & 2) strcat(tmp, "M"); else if(m & 1) strcat(tmp, "N"); }
        strcpy(p[n], tmp); strcpy(d[n], "ONES: "); d[n][6] = (char)('0' + u); d[n][7] = 0; n++;
    }
    if(t){ strcpy(p[n], TE[t]); strcpy(d[n], "TENS: "); d[n][6] = (char)('0' + t); d[n][7] = '0'; d[n][8] = 0; n++; }
    if(h){ strcpy(p[n], HU[h]); strcpy(d[n], "HUNDREDS: "); d[n][10] = (char)('0' + h); d[n][11] = '0'; d[n][12] = '0'; d[n][13] = 0; n++; }
    return n;
}

static const char *const ON[20] = {"ZERO","ONE","TWO","THREE","FOUR","FIVE","SIX","SEVEN","EIGHT","NINE","TEN","ELEVEN","TWELVE","THIRTEEN","FOURTEEN","FIFTEEN","SIXTEEN","SEVENTEEN","EIGHTEEN","NINETEEN"};
static const char *const TY[10] = {"","","TWENTY","THIRTY","FORTY","FIFTY","SIXTY","SEVENTY","EIGHTY","NINETY"};
int gen_chunk(int v, char *o){
    o[0] = 0; int h = v / 100, r = v % 100;
    if(h){ strcat(o, ON[h]); strcat(o, " HUNDRED"); if(r) strcat(o, " "); }
    if(r || !h){
        if(r < 20) strcat(o, ON[r]);
        else { strcat(o, TY[r / 10]); if(r % 10){ strcat(o, "-"); strcat(o, ON[r % 10]); } }
    }
    return (int)strlen(o);
}
static unsigned char ENLEN[1000]; static int en_ready;
int gen_chunklen(int v){ if(!en_ready){ char b[64]; for(int i = 0; i < 1000; i++) ENLEN[i] = (unsigned char)gen_chunk(i, b); en_ready = 1; } return ENLEN[v]; }
void gen_u64(gu64 v, char *o, int cap){
    static const char *const SC[7] = {"","THOUSAND","MILLION","BILLION","TRILLION","QUADRILLION","QUINTILLION"};
    char t[600]; t[0] = 0;
    if(v == 0) strcpy(t, "ZERO");
    else {
        int g[7], n = 0; while(v){ g[n++] = (int)(v % 1000); v /= 1000; }
        for(int i = n - 1; i >= 0; i--) if(g[i]){ char b[64]; gen_chunk(g[i], b); if(t[0]) strcat(t, " "); strcat(t, b); if(i){ strcat(t, " "); strcat(t, SC[i]); } }
    }
    strncpy(o, t, cap - 1); o[cap - 1] = 0;
}

// ---- decimal strings
void gds_div3(const char *s, char *q, int *r){
    int rem = 0, k = 0, n = (int)strlen(s);
    for(int i = 0; i < n; i++){ rem = rem * 10 + (s[i] - '0'); int dg = rem / 3; rem %= 3; if(k || dg) q[k++] = (char)('0' + dg); }
    if(!k) q[k++] = '0'; q[k] = 0; if(r) *r = rem;
}
void gds_sub1(const char *s, char *o){
    int n = (int)strlen(s); strcpy(o, s); int i = n - 1;
    while(i >= 0 && o[i] == '0'){ o[i] = '9'; i--; }
    if(i >= 0) o[i]--;
    int z = 0; while(o[z] == '0' && o[z+1]) z++; if(z) memmove(o, o + z, strlen(o + z) + 1);
}
int gds_cmp(const char *a, const char *b){
    while(*a == '0' && a[1]) a++; while(*b == '0' && b[1]) b++;
    int la = (int)strlen(a), lb = (int)strlen(b); if(la != lb) return la < lb ? -1 : 1; int c = strcmp(a, b); return c < 0 ? -1 : c > 0;
}
void gds_comma(const char *s, char *o, int cap){
    int n = (int)strlen(s), k = 0;
    for(int i = 0; i < n && k < cap - 2; i++){ if(i && (n - i) % 3 == 0) o[k++] = ','; o[k++] = s[i]; }
    o[k] = 0;
}

// ================================================================= SHA-256
static const unsigned K256[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void gsha_block(GiSha *s, const unsigned char *p){
    unsigned w[64], a, b, c, d, e, f, g, h;
    for(int i = 0; i < 16; i++) w[i] = (unsigned)p[4*i] << 24 | (unsigned)p[4*i+1] << 16 | (unsigned)p[4*i+2] << 8 | p[4*i+3];
    for(int i = 16; i < 64; i++){ unsigned s0 = ROR(w[i-15],7) ^ ROR(w[i-15],18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2],17) ^ ROR(w[i-2],19) ^ (w[i-2] >> 10); w[i] = w[i-16] + s0 + w[i-7] + s1; }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for(int i = 0; i < 64; i++){
        unsigned S1 = ROR(e,6) ^ ROR(e,11) ^ ROR(e,25), ch = (e & f) ^ (~e & g), t1 = h + S1 + ch + K256[i] + w[i];
        unsigned S0 = ROR(a,2) ^ ROR(a,13) ^ ROR(a,22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
void gsha_init(GiSha *s){ static const unsigned i0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; memcpy(s->h, i0, sizeof i0); s->len = 0; s->nb = 0; }
void gsha_add(GiSha *s, const void *dv, gu64 n){
    const unsigned char *d = dv; s->len += n;
    if(s->nb){ while(n && s->nb < 64){ s->buf[s->nb++] = *d++; n--; } if(s->nb == 64){ gsha_block(s, s->buf); s->nb = 0; } }
    while(n >= 64){ gsha_block(s, d); d += 64; n -= 64; }
    while(n){ s->buf[s->nb++] = *d++; n--; }
}
void gsha_end(GiSha *s, char hex[65]){
    gu64 bits = s->len * 8; unsigned char pad[72]; int pn = (s->nb < 56) ? 56 - s->nb : 120 - s->nb; memset(pad, 0, sizeof pad); pad[0] = 0x80;
    gu64 keep = s->len; gsha_add(s, pad, pn); for(int i = 0; i < 8; i++) pad[i] = (unsigned char)(bits >> (56 - 8*i)); gsha_add(s, pad, 8); s->len = keep;
    for(int i = 0; i < 8; i++) sprintf(hex + 8*i, "%08x", s->h[i]);
    hex[64] = 0;
}
void gsha256_buf(const unsigned char *d, gu64 n, unsigned char out[32]){ GiSha s; char hx[65]; gsha_init(&s); gsha_add(&s, d, n); gsha_end(&s, hx); for(int i = 0; i < 32; i++){ unsigned v; sscanf(hx + 2*i, "%2x", &v); out[i] = (unsigned char)v; } }

// ================================================================= where the files are
static char g_dir[600];
#include "gpow.inc"
static FILE *g_pf; static int g_pstate;       // 0 not tried, 1 open, 2 failed (missing), 3 wrong
static const char *const FN[GI_N] = {"pow2_2e32.bcd", "(none: worked out live)"};
const char *gi_file(int which){ return FN[which]; }
const char *gi_title(int which){ return which == GI_POW ? "2 TO THE POWER 2^32" : "THE COUNTING NUMBER"; }
gu64 gi_digits(int which){ return which == GI_POW ? GI_POW_DIGITS : GI_COUNT_DIGITS; }
static int file_exists(const char *p){ struct stat st; return stat(p, &st) == 0 && S_ISREG(st.st_mode); }
void gi_set_dir(const char *d){ if(g_pf){ fclose(g_pf); g_pf = 0; } g_pstate = 0; if(d){ strncpy(g_dir, d, sizeof g_dir - 1); g_dir[sizeof g_dir - 1] = 0; } else g_dir[0] = 0; }
const char *gi_dir(void){ return g_dir; }
static void find_dir(void){
    if(g_dir[0]) return;
    char c[700]; const char *e = getenv("BUGSTAR_GIANT_DIR");
    if(e && *e){ snprintf(c, sizeof c, "%s/%s", e, FN[0]); if(file_exists(c)){ strncpy(g_dir, e, sizeof g_dir - 1); return; } }
    char exe[600]; ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if(n > 0){ exe[n] = 0; char *sl = strrchr(exe, '/'); if(sl){ *sl = 0; snprintf(c, sizeof c, "%s/giant/cache/%s", exe, FN[0]); if(file_exists(c)){ snprintf(g_dir, sizeof g_dir, "%s/giant/cache", exe); return; }
        snprintf(c, sizeof c, "%s/../pc/giant/cache/%s", exe, FN[0]); if(file_exists(c)){ snprintf(g_dir, sizeof g_dir, "%s/../pc/giant/cache", exe); return; } } }
    snprintf(c, sizeof c, "giant/cache/%s", FN[0]); if(file_exists(c)){ strcpy(g_dir, "giant/cache"); return; }
    snprintf(c, sizeof c, "pc/giant/cache/%s", FN[0]); if(file_exists(c)){ strcpy(g_dir, "pc/giant/cache"); return; }
    if(e && *e){ strncpy(g_dir, e, sizeof g_dir - 1); return; }
    // no digit file anywhere: the small files (FAST CACHE, V3 index) go to <program folder>/giant/cache
    if(n > 0){ snprintf(g_dir, sizeof g_dir, "%s/giant/cache", exe); }
}
static void gi_mkdir(void){ char cmd[700]; if(!g_dir[0]) find_dir(); snprintf(cmd, sizeof cmd, "mkdir -p '%s' 2>/dev/null", g_dir); if(system(cmd)){} }
int gi_open(int which){
    if(which == GI_COUNT) return 0;
    if(g_pstate == 1) return 0;
    if(g_pstate == 2 || g_pstate == 3) g_pstate = 0;           // (try again: the file may have been built meanwhile)
    // the digits are always there: worked out on the spot (gpow.inc). A full digit file (pow2_2e32.bcd, made by mkpow.py: used by the tests
    // and by developers) or the FAST CACHE band file are used when present.
    find_dir(); g_pstate = 1; gi_cache_open();
    if(!g_dir[0]) return 0;
    char p[700]; snprintf(p, sizeof p, "%s/%s", g_dir, FN[0]);
    FILE *f = fopen(p, "rb"); if(!f) return 0;
    unsigned char h[64]; if(fread(h, 1, 64, f) != 64 || memcmp(h, "BSGPOW2\0", 8)){ fclose(f); return 0; }
    gu64 e, dg; memcpy(&e, h + 8, 8); memcpy(&dg, h + 16, 8);
    fseeko(f, 0, SEEK_END); gu64 sz = (gu64)ftello(f);
    if(e != GI_POW_E || dg != GI_POW_DIGITS || sz != 64 + (GI_POW_DIGITS + 1) / 2){ fclose(f); return 0; }
    g_pf = f; return 0;
}
int gi_ready(int which){ if(which == GI_POW && g_pstate != 1) gi_open(GI_POW); return which == GI_COUNT ? 1 : g_pstate == 1; }

static int count_read(gu64 pos, int n, char *out){
    gu64 cum = 0, first = 1, cnt = 9; int d = 1, got = 0;
    for(; d <= 9; d++, first *= 10, cnt *= 10){ gu64 dg = cnt * (gu64)d; if(pos < cum + dg) break; cum += dg; }
    if(d > 9) return 0;
    gu64 idx = pos - cum, num = first + idx / (gu64)d; int off = (int)(idx % (gu64)d);
    while(got < n && num <= GI_COUNT_LAST){
        char t[16]; int dl = 0; gu64 x = num; while(x){ t[dl++] = (char)('0' + x % 10); x /= 10; }
        for(int i = dl - 1 - off; i >= 0 && got < n; i--) out[got++] = t[i];
        off = 0; num++;
    }
    return got;
}
static int pow_read_file(gu64 pos, int n, char *out);
static int pow_read(gu64 pos, int n, char *out){
    if(pos >= GI_POW_DIGITS) return 0;
    if(pos + (gu64)n > GI_POW_DIGITS) n = (int)(GI_POW_DIGITS - pos);
    if(g_pf) return pow_read_file(pos, n, out);
    int got = 0;
    while(got < n){
        gu64 q = pos + (gu64)got; int k = gp_read_disk(q, n - got, out + got);
        if(!k) for(int b = 0; b < GP_NBAND && !k; b++) if(gp_have(&gp_band[b], q)){ k = gp_read_band(&gp_band[b], q, n - got, out + got); gp_band[b].used = ++gp_clock; }
        if(!k){                                                                 // not kept anywhere: work it out now (into the band used longest ago)
            int v = 0; for(int b = 1; b < GP_NBAND; b++) if(gp_band[b].used < gp_band[v].used) v = b;
            gu64 b0, bn; gp_plan(q, &b0, &bn);
            if(gp_make(&gp_band[v], b0, bn, gp_hook, gp_hook_ud)) break;
            continue;
        }
        got += k;
    }
    return got;
}
static int pow_read_file(gu64 pos, int n, char *out){
    if(!g_pf || pos >= GI_POW_DIGITS) return 0;
    if(pos + (gu64)n > GI_POW_DIGITS) n = (int)(GI_POW_DIGITS - pos);
    gu64 b0 = pos >> 1, b1 = (pos + (gu64)n - 1) >> 1; int nb = (int)(b1 - b0 + 1);
    unsigned char *buf = malloc((size_t)nb); if(!buf) return 0;
    fseeko(g_pf, (off_t)(64 + b0), SEEK_SET);
    if(fread(buf, 1, (size_t)nb, g_pf) != (size_t)nb){ free(buf); return 0; }
    for(int i = 0; i < n; i++){ gu64 p = pos + (gu64)i; unsigned char b = buf[(p >> 1) - b0]; out[i] = (char)('0' + ((p & 1) ? (b & 15) : (b >> 4))); }
    free(buf); return n;
}
int gi_read(int which, gu64 pos, int n, char *out){
    int got = which == GI_COUNT ? count_read(pos, n, out) : pow_read(pos, n, out);
    for(int i = got; i < n; i++) out[i] = 0;
    return got;
}

// ================================================================= export
typedef struct { FILE *f; GiSha sh; gu64 pos, wrote; int err; char *o; } GiExp;
static int gi_exp_emit(const char *d, gu64 n, void *ud){
    GiExp *E = ud; int k = 0;
    for(gu64 i = 0; i < n; i++){ E->o[k++] = d[i]; E->pos++; if(E->pos % 100 == 0 || E->pos == GI_POW_DIGITS) E->o[k++] = '\n'; }
    if(fwrite(E->o, 1, (size_t)k, E->f) != (size_t)k){ E->err = 1; return 1; }
    gsha_add(&E->sh, E->o, (gu64)k); E->wrote += (gu64)k; return 0;
}
int gi_export(int which, const char *path, gi_progress_fn pr, void *ud, char hex[65], gu64 *bytes){
    if(!gi_ready(which)) return 3;
    if(which == GI_POW && !g_pf){                                   // work the whole number out in two passes (head half, tail half), straight to the file
        FILE *f = fopen(path, "wb"); if(!f) return 2;
        GiExp E; memset(&E, 0, sizeof E); E.f = f; E.o = malloc((1 << 16) * 2 + 4); gsha_init(&E.sh);
        int rc = gp_window(0, GP_SPLIT, gi_exp_emit, &E, pr, ud);
        if(!rc) rc = gp_window(GP_SPLIT, GI_POW_DIGITS - GP_SPLIT, gi_exp_emit, &E, pr, ud);
        if(fclose(f)) E.err = 1; free(E.o);
        if(hex) gsha_end(&E.sh, hex); if(bytes) *bytes = E.wrote;
        return E.err ? 2 : rc == 1 ? 1 : rc ? 2 : 0;
    }
    FILE *f = fopen(path, "wb"); if(!f) return 2;
    gu64 total = gi_digits(which), pos = 0, wrote = 0; GiSha sh; gsha_init(&sh);
    enum { CH = 1 << 20 };                                       // (a multiple of 100? no: lines are cut by position, below)
    char *in = malloc(CH), *o = malloc(CH + CH / 100 + 4); int rc = 0;
    if(!in || !o){ fclose(f); free(in); free(o); return 2; }
    while(pos < total){
        int want = CH; if(pos + (gu64)want > total) want = (int)(total - pos);
        int got = gi_read(which, pos, want, in); if(got != want){ rc = 2; break; }
        int k = 0;
        for(int i = 0; i < got; i++){ o[k++] = in[i]; gu64 p = pos + (gu64)i + 1; if(p % 100 == 0 || p == total) o[k++] = '\n'; }
        if(fwrite(o, 1, (size_t)k, f) != (size_t)k){ rc = 2; break; }
        gsha_add(&sh, o, (gu64)k); wrote += (gu64)k; pos += (gu64)got;
        if(pr && pr(pos, total, ud)){ rc = 1; break; }
    }
    if(fclose(f)) rc = rc ? rc : 2;
    free(in); free(o);
    if(hex) gsha_end(&sh, hex);
    if(bytes) *bytes = wrote;
    return rc;
}

// ================================================================= BUGSTARILLION v3
#define B3 65536
static gu64 L3, TOT3, *Pblk, *Ablk[4], nblk3, nablk[4];
static int lv3[4];                                  // the first chunk of n(j) for a = 1, 2, 3
static int gv3_state;
static unsigned h32(gu64 q){ unsigned h = (unsigned)(q * 2654435761ull); h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
int gv3_coef(gu64 j){ return 100 + (int)(h32(j) % 900u); }
gu64 gv3_parts(void){ return L3; }
gu64 gv3_chars(void){ return TOT3; }
int gv3_ready(void){ return gv3_state == 1; }
static gu64 kmax(int a){ return (L3 - (gu64)a) / 3; }

static int dg(gu64 pos){ char c; gi_read(GI_POW, pos, 1, &c); return c - '0'; }
// C_a(k): the letters of the first k triples of alignment a (triple i = the digits a+3i .. a+3i+2)
static gu64 cval(int a, gu64 k){
    gu64 blk = k / B3, c = Ablk[a][blk], i0 = blk * B3, n = k - i0;
    if(!n) return c;
    char *buf = malloc((size_t)(3 * n)); gi_read(GI_POW, (gu64)a + 3 * i0, (int)(3 * n), buf);
    for(gu64 i = 0; i < n; i++) c += TLEN[(buf[3*i] - '0') * 100 + (buf[3*i+1] - '0') * 10 + (buf[3*i+2] - '0')];
    free(buf); return c;
}
static gu64 wterm(int coef, int a, gu64 C){ return (gu64)gen_chunklen(coef) + 1 + TLEN[lv3[a]] + C + 2 + 2; }

static int idx_path(char *p, int n){ snprintf(p, n, "%s/pow2_2e32.v3idx", gi_dir()); return 0; }
static int gv3_build(void){
    tl_init(); gen_chunklen(0);
    gu64 nb = (L3 + B3 - 1) / B3; Pblk = calloc(nb + 1, 8);
    for(int a = 1; a <= 3; a++){ nablk[a] = kmax(a) / B3 + 1; Ablk[a] = calloc(nablk[a] + 1, 8); }
    char *buf = malloc(B3 + 8); gu64 C[4] = {0, 0, 0, 0}, P = 0;
    for(gu64 b = 0; b < nb; b++){
        gu64 j0 = b * B3 + 1, j1 = j0 + B3 - 1; if(j1 > L3) j1 = L3;
        gi_read(GI_POW, j0, (int)(j1 - j0 + 3), buf);                 // digits g[j0 .. j1+2] (0-based: g[j] is digit number j+1)
        for(gu64 j = j0; j <= j1; j++){
            int a = (int)((j - 1) % 3) + 1; gu64 k = (j - (gu64)a) / 3;
            if(k % B3 == 0) Ablk[a][k / B3] = C[a];
            P += wterm(gv3_coef(j), a, C[a]);
            if(j + 2 < L3){ const char *t = buf + (j - j0); C[a] += TLEN[(t[0] - '0') * 100 + (t[1] - '0') * 10 + (t[2] - '0')]; }
        }
        Pblk[b + 1] = P;
        if((b & 1023) == 0 && getenv("GIANT_VERBOSE")) fprintf(stderr, "v3 index %llu / %llu\n", b, nb);
    }
    free(buf); nblk3 = nb; TOT3 = P - 1; return 0;
}
// the same index from the digits as one stream (worked out on the spot in two passes: no digit file needed)
typedef struct { gu64 i, j, P, C[4]; char d3[3]; gu64 nb; gi_progress_fn pr; void *ud; } V3S;
static void v3s_term(V3S *S, gu64 j, int add){
    int a = (int)((j - 1) % 3) + 1; gu64 k = (j - (gu64)a) / 3;
    if(k % B3 == 0) Ablk[a][k / B3] = S->C[a];
    S->P += wterm(gv3_coef(j), a, S->C[a]);
    if(add) S->C[a] += TLEN[(S->d3[0] - '0') * 100 + (S->d3[1] - '0') * 10 + (S->d3[2] - '0')];
    if(j % B3 == 0 || j == L3) Pblk[(j - 1) / B3 + 1] = S->P;
}
static int v3s_emit(const char *d, gu64 n, void *ud){
    V3S *S = ud;
    for(gu64 q = 0; q < n; q++){ S->d3[0] = S->d3[1]; S->d3[1] = S->d3[2]; S->d3[2] = d[q]; if(S->i >= 3) v3s_term(S, S->i - 2, 1); S->i++; }
    return 0;
}
int gv3_build_index(gi_progress_fn pr, void *ud){
    if(gv3_state == 1) return 0;
    if(g_pstate != 1) gi_open(GI_POW);
    tl_init(); gen_chunklen(0); L3 = GI_POW_DIGITS;
    char c[3]; for(int a = 1; a <= 3; a++){ int v = 0; gi_read(GI_POW, 0, 3, c); for(int i = 0; i < a; i++) v = v * 10 + c[i] - '0'; lv3[a] = v; }
    gu64 nb = (L3 + B3 - 1) / B3; free(Pblk); Pblk = calloc(nb + 1, 8);
    for(int a = 1; a <= 3; a++){ nablk[a] = kmax(a) / B3 + 1; free(Ablk[a]); Ablk[a] = calloc(nablk[a] + 1, 8); }
    V3S S; memset(&S, 0, sizeof S);
    int rc = gp_window(0, GP_SPLIT, v3s_emit, &S, pr, ud);
    if(!rc) rc = gp_window(GP_SPLIT, L3 - GP_SPLIT, v3s_emit, &S, pr, ud);
    if(rc || S.i != L3) return rc ? rc : 3;
    for(gu64 j = L3 - 2; j <= L3; j++) v3s_term(&S, j, 0);
    nblk3 = nb; TOT3 = S.P - 1;
    gi_mkdir(); char p[700]; idx_path(p, sizeof p); FILE *f = fopen(p, "wb");
    if(f){ gu64 hd[4] = {0x3356494458ull, L3, B3, nblk3}; fwrite(hd, 8, 4, f); fwrite(Pblk, 8, nblk3 + 1, f); for(int a = 1; a <= 3; a++) fwrite(Ablk[a], 8, nablk[a] + 1, f); fclose(f); }
    gv3_state = 1; return 0;
}
int gv3_have_index(void){ if(gv3_state == 1 || g_pf) return 1; if(g_pstate != 1) gi_open(GI_POW); char p[700]; idx_path(p, sizeof p); return file_exists(p); }
int gv3_open(void){
    if(gv3_state == 1) return 0;
    if(gi_open(GI_POW)) return 1;
    tl_init(); gen_chunklen(0); L3 = GI_POW_DIGITS;
    char c[3]; for(int a = 1; a <= 3; a++){ int v = 0; for(int i = 0; i < a; i++){ gi_read(GI_POW, (gu64)i, 1, c); v = v * 10 + c[0] - '0'; } lv3[a] = v; }
    char p[700]; idx_path(p, sizeof p); FILE *f = fopen(p, "rb"); int ok = 0;
    if(f){
        gu64 hd[4]; gu64 nb = (L3 + B3 - 1) / B3;
        if(fread(hd, 8, 4, f) == 4 && hd[0] == 0x3356494458ull && hd[1] == L3 && hd[2] == B3 && hd[3] == nb){
            Pblk = malloc((nb + 1) * 8); ok = fread(Pblk, 8, nb + 1, f) == nb + 1;
            for(int a = 1; a <= 3 && ok; a++){ nablk[a] = kmax(a) / B3 + 1; Ablk[a] = malloc((nablk[a] + 1) * 8); ok = fread(Ablk[a], 8, nablk[a] + 1, f) == nablk[a] + 1; }
            if(ok){ nblk3 = nb; TOT3 = Pblk[nb] - 1; }
        }
        fclose(f);
        if(!ok){ free(Pblk); Pblk = 0; for(int a = 1; a <= 3; a++){ free(Ablk[a]); Ablk[a] = 0; } }
    }
    if(!ok && !g_pf) return 1;                                     // no index yet: gv3_build_index() makes it (a few minutes, 0.7 MB)
    if(!ok){
        gv3_build();
        f = fopen(p, "wb");
        if(f){ gu64 hd[4] = {0x3356494458ull, L3, B3, nblk3}; fwrite(hd, 8, 4, f); fwrite(Pblk, 8, nblk3 + 1, f); for(int a = 1; a <= 3; a++) fwrite(Ablk[a], 8, nablk[a] + 1, f); fclose(f); }
    }
    gv3_state = 1; return 0;
}

// the scan inside one block of terms: from term j0 = b*B3+1 with P = Pblk[b]. stops at the term where P(j) >= Y (Y > 0) or at jmax.
static void gv3_scan(gu64 b, gu64 Y, gu64 jmax, gu64 *jout, gu64 *Pbefore, gu64 *Wout){
    gu64 j0 = b * B3 + 1, j1 = j0 + B3 - 1; if(j1 > L3) j1 = L3; if(j1 > jmax) j1 = jmax;
    gu64 Cs[4] = {0, 0, 0, 0}, P = Pblk[b];
    for(int a = 1; a <= 3; a++){
        gu64 ja = j0 + (gu64)(((a - 1) - (int)((j0 - 1) % 3) + 3) % 3);
        if(ja <= L3) Cs[a] = cval(a, (ja - (gu64)a) / 3);
    }
    char *buf = malloc(B3 + 8); gi_read(GI_POW, j0, (int)(j1 - j0 + 3), buf);
    gu64 j, W = 0;
    for(j = j0; j <= j1; j++){
        int a = (int)((j - 1) % 3) + 1; W = wterm(gv3_coef(j), a, Cs[a]);
        if(Y && P + W >= Y) break;
        if(j == j1) break;
        P += W;
        if(j + 2 < L3){ const char *t = buf + (j - j0); Cs[a] += TLEN[(t[0] - '0') * 100 + (t[1] - '0') * 10 + (t[2] - '0')]; }
    }
    free(buf); *jout = j; *Pbefore = P; *Wout = W;
}
static gu64 p_before(gu64 j, gu64 *Wj){ gu64 jj, P, W; gv3_scan((j - 1) / B3, 0, j, &jj, &P, &W); if(Wj) *Wj = W; return P; }   // P(j-1)
gu64 gv3_term_len(gu64 j){ gu64 W; p_before(j, &W); return j == 1 ? W - 1 : W; }
gu64 gv3_term_start(gu64 j){ gu64 W; gu64 Pj = p_before(j, &W) + W; return Pblk[nblk3] - Pj; }
int gv3_chunk(gu64 j, gu64 ci){
    int a = (int)((j - 1) % 3) + 1; if(ci == 0) return lv3[a];
    char t[3]; gi_read(GI_POW, (gu64)a + 3 * (ci - 1), 3, t); return (t[0] - '0') * 100 + (t[1] - '0') * 10 + (t[2] - '0');
}
static void locate(gu64 pos, gu64 *jo, gu64 *start, gu64 *W) {
    gu64 Y = Pblk[nblk3] - pos, lo = 0, hi = nblk3 - 1;           // block b with Pblk[b] < Y <= Pblk[b+1]
    while(lo < hi){ gu64 mid = (lo + hi) / 2; if(Pblk[mid + 1] >= Y) hi = mid; else lo = mid + 1; }
    gu64 j, Pb, Wj; gv3_scan(lo, Y, L3, &j, &Pb, &Wj);
    *jo = j; *W = Wj; *start = Pblk[nblk3] - (Pb + Wj);
}
static gu64 place_len(gu64 j){ int a = (int)((j - 1) % 3) + 1; gu64 k = (j - (gu64)a) / 3; return TLEN[lv3[a]] + cval(a, k) + 2; }
// which chunk holds letter off2 of the -illion name of term j; also the letter's place inside that chunk's text
static void find_chunk(gu64 j, gu64 off2, gu64 *ci, gu64 *toff){
    int a = (int)((j - 1) % 3) + 1; gu64 k = (j - (gu64)a) / 3;
    gu64 t0 = TLEN[lv3[a]] + (k == 0 ? 2 : 0);
    if(off2 < t0){ *ci = 0; *toff = off2; return; }
    gu64 r = off2 - t0, lo = 0, hi = nablk[a] - 1;
    if(hi > (k ? (k - 1) / B3 : 0)) hi = k ? (k - 1) / B3 : 0;
    while(lo < hi){ gu64 mid = (lo + hi + 1) / 2; if(Ablk[a][mid] <= r) lo = mid; else hi = mid - 1; }
    gu64 i = lo * B3, c = Ablk[a][lo], cnt = k - i; if(cnt > B3) cnt = B3;
    char *buf = malloc((size_t)(3 * cnt + 3)); gi_read(GI_POW, (gu64)a + 3 * i, (int)(3 * cnt), buf);
    gu64 q;
    for(q = 0; q < cnt; q++){
        int len = TLEN[(buf[3*q] - '0') * 100 + (buf[3*q+1] - '0') * 10 + (buf[3*q+2] - '0')]; if(i + q == k - 1) len += 2;
        if(r < c + (gu64)len) break;
        c += (gu64)len;
    }
    free(buf); *ci = i + q + 1; *toff = r - c;
}
int gv3_info(gu64 pos, V3Info *vi){
    if(!gv3_ready() || pos >= TOT3) return 1;
    gu64 j, st, W; locate(pos, &j, &st, &W);
    memset(vi, 0, sizeof *vi); vi->j = j; vi->start = st; vi->len = j == 1 ? W - 1 : W; vi->off = pos - st; vi->coef = gv3_coef(j);
    int a = (int)((j - 1) % 3) + 1; vi->a = a; vi->nchunks = 1 + (j - (gu64)a) / 3; vi->placelen = place_len(j);
    gu64 cl = (gu64)gen_chunklen(vi->coef);
    if(vi->off < cl) vi->seg = 0; else if(vi->off == cl) vi->seg = 1; else if(vi->off < cl + 1 + vi->placelen){ vi->seg = 2; gu64 toff; find_chunk(j, vi->off - cl - 1, &vi->chunk, &toff); } else vi->seg = 3;
    return 0;
}
// write up to n letters of term j from letter off; returns how many were written
static int emit_term(gu64 j, gu64 off, int n, char *out){
    int w = 0; char cw[64]; int coef = gv3_coef(j); int cl = gen_chunk(coef, cw);
    int a = (int)((j - 1) % 3) + 1; gu64 k = (j - (gu64)a) / 3;
    while(off < (gu64)cl && w < n) out[w++] = cw[off++];
    if(w == n) return w;
    if(off == (gu64)cl){ out[w++] = ' '; off++; if(w == n) return w; }
    gu64 off2 = off - (gu64)cl - 1, ci, toff; find_chunk(j, off2, &ci, &toff);
    if(ci <= k){
        char tb[64]; char *tri = 0; gu64 tcnt = 0, tbase = 0;
        while(ci <= k && w < n){
            int v;
            if(ci == 0) v = lv3[a];
            else {
                if(!tri || ci - 1 >= tbase + tcnt){ free(tri); tbase = ci - 1; tcnt = k - tbase; if(tcnt > 256) tcnt = 256; tri = malloc((size_t)(3 * tcnt)); gi_read(GI_POW, (gu64)a + 3 * tbase, (int)(3 * tcnt), tri); }
                const char *t = tri + 3 * (ci - 1 - tbase); v = (t[0] - '0') * 100 + (t[1] - '0') * 10 + (t[2] - '0');
            }
            int tl = gcw_tok(v, ci == k, tb);
            for(gu64 i = toff; i < (gu64)tl && w < n; i++) out[w++] = tb[i];
            toff = 0; ci++;
        }
        free(tri);
        if(w == n) return w;
        off = 0;
    } else off = off2 + (gu64)cl + 1 - ((gu64)cl + 1 + place_len(j));       // (we start inside the separator)
    const char *sep = j > 1 ? ", " : "."; int sl = j > 1 ? 2 : 1;
    while(off < (gu64)sl && w < n){ out[w++] = sep[off]; off++; }
    return w;
}
int gv3_read(gu64 pos, int n, char *out){
    if(!gv3_ready() || pos >= TOT3) return 0;
    if(pos + (gu64)n > TOT3) n = (int)(TOT3 - pos);
    gu64 j, st, W; locate(pos, &j, &st, &W); gu64 off = pos - st; int w = 0;
    while(w < n && j >= 1){
        w += emit_term(j, off, n - w, out + w); off = 0;
        if(j == 1) break; j--;
    }
    return w;
}
int gv3_export(const char *path, gu64 start, gu64 n, gi_progress_fn pr, void *ud, char hex[65]){
    if(!gv3_ready()) return 3;
    if(start >= TOT3) return 2; if(n > TOT3 - start) n = TOT3 - start;
    FILE *f = fopen(path, "wb"); if(!f) return 2;
    enum { CH = 1 << 20 }; char *b = malloc(CH); GiSha sh; gsha_init(&sh); gu64 done = 0; int rc = 0;
    while(done < n){
        int want = n - done > CH ? CH : (int)(n - done); int got = gv3_read(start + done, want, b);
        if(got != want){ rc = 2; break; }
        if(fwrite(b, 1, (size_t)got, f) != (size_t)got){ rc = 2; break; }
        gsha_add(&sh, b, (gu64)got); done += (gu64)got;
        if(pr && pr(done, n, ud)){ rc = 1; break; }
    }
    if(fclose(f)) rc = rc ? rc : 2; free(b); if(hex) gsha_end(&sh, hex);
    return rc;
}

// ================================================================= on-the-spot digits of 2^(2^32): settings, the FAST CACHE, a direct window (tests)
void gi_set_hook(gi_progress_fn pr, void *ud){ gp_hook = pr; gp_hook_ud = ud; }
const char *gi_error(void){ return gp_err; }
int gi_mode(void){ if(g_pstate != 1) gi_open(GI_POW); return g_pf ? GI_MODE_FILE : gp_diskf ? GI_MODE_CACHE : GI_MODE_LIVE; }
int gi_have(gu64 pos){ if(g_pf) return 1; if(gp_diskf && pos >= gp_disk.b0 && pos < gp_disk.b0 + gp_disk.n) return 1; for(int b = 0; b < GP_NBAND; b++) if(gp_have(&gp_band[b], pos)) return 1; return 0; }
gu64 gi_ram_bytes(void){ gu64 t = 0; for(int b = 0; b < GP_NBAND; b++) if(gp_band[b].pk) t += gp_pkbytes(gp_band[b].n); return t; }
static void gi_cache_path(char *p, int n){ const char *d = gi_dir(); if(!d[0]){ find_dir(); d = gi_dir(); } snprintf(p, n, "%s/pow2_2e32.band", d[0] ? d : "."); }
#define GI_CACHE_B0 (GP_SPLIT - 120 * GP_BLKD)
#define GI_CACHE_N (240 * GP_BLKD)
gu64 gi_cache_bytes(void){ return 64 + gp_pkbytes(GI_CACHE_N); }
int gi_cache_open(void){
    if(gp_diskf) return 0;
    char p[760]; gi_cache_path(p, sizeof p); FILE *f = fopen(p, "rb"); if(!f) return 1;
    unsigned char h[64]; gu64 b0, n; if(fread(h, 1, 64, f) != 64 || memcmp(h, "BSGBAND1", 8)){ fclose(f); return 2; }
    memcpy(&b0, h + 8, 8); memcpy(&n, h + 16, 8); fseeko(f, 0, SEEK_END);
    if(b0 != GI_CACHE_B0 || n != GI_CACHE_N || (gu64)ftello(f) != gi_cache_bytes()){ fclose(f); return 2; }
    gp_disk.b0 = b0; gp_disk.n = n; gp_diskf = f; return 0;
}
// build the FAST CACHE: the 240,000,000 digits around the middle (where working out is dearest), checked block by block, 100 MB
int gi_cache_build(gi_progress_fn pr, void *ud){
    if(g_pstate != 1) gi_open(GI_POW);
    char p[760], tp[780]; gi_cache_path(p, sizeof p); snprintf(tp, sizeof tp, "%s.part", p);
    gi_mkdir();
    GpBand L = {0}, R = {0}; int rc = gp_make(&L, GI_CACHE_B0, GI_CACHE_N / 2, pr, ud);
    if(!rc) rc = gp_make(&R, GP_SPLIT, GI_CACHE_N / 2, pr, ud);
    if(rc){ free(L.pk); free(R.pk); return rc; }
    FILE *f = fopen(tp, "wb"); if(!f){ free(L.pk); free(R.pk); return 5; }
    unsigned char h[64]; memset(h, 0, 64); memcpy(h, "BSGBAND1", 8); gu64 b0 = GI_CACHE_B0, n = GI_CACHE_N; memcpy(h + 8, &b0, 8); memcpy(h + 16, &n, 8);
    fwrite(h, 1, 64, f);
    unsigned char *pk = calloc((size_t)gp_pkbytes(n), 1); int okw = pk != 0;
    if(pk){ for(gu64 t = 0; t < n / 3; t++){ gu64 q = t * 3; const GpBand *B = q < GI_CACHE_N / 2 ? &L : &R; gu64 r = q - (B == &L ? 0 : GI_CACHE_N / 2); gp_put3(pk, t, gp_get3(B->pk, r / 3)); }   // (both halves are multiples of 3)
        okw = fwrite(pk, 1, (size_t)gp_pkbytes(n), f) == (size_t)gp_pkbytes(n); }
    free(pk); free(L.pk); free(R.pk);
    if(fclose(f) || !okw){ remove(tp); return 5; }
    if(rename(tp, p)){ remove(tp); return 5; }
    return gi_cache_open() ? 5 : 0;
}
int gi_cache_delete(void){ char p[760]; gi_cache_path(p, sizeof p); if(gp_diskf){ fclose(gp_diskf); gp_diskf = 0; } gp_disk.n = 0; return remove(p) == 0 ? 0 : 1; }
int gi_cache_present(void){ return gp_diskf != 0; }
int gi_window(gu64 pos, gu64 n, int (*emit)(const char *d, gu64 n, void *ud), void *ud, gi_progress_fn pr, void *pud){ return gp_window(pos, n, emit, ud, pr, pud); }
void gi_forget(void){ for(int b = 0; b < GP_NBAND; b++){ free(gp_band[b].pk); gp_band[b].pk = 0; gp_band[b].n = 0; } }
void gi_use_file(int on){ if(!on && g_pf){ fclose(g_pf); g_pf = 0; } }
