// lv.c: see lv.h
#include "lv.h"
#include <string.h>

typedef unsigned __int128 u128;
#define CAPLIMB 24                       // 2^1536 = bit 0 of limb 24

void lv_zero(Lv *r){ memset(r, 0, sizeof *r); }
void lv_from_u64(Lv *r, uint64_t v){ lv_zero(r); r->w[0] = v; }
void lv_cap(Lv *r){ lv_zero(r); r->w[CAPLIMB] = 1; }
int lv_is_zero(const Lv *a){ for(int i = 0; i < LV_LIMBS; i++) if(a->w[i]) return 0; return 1; }
int lv_is_cap(const Lv *a){ return a->w[CAPLIMB] > 1 || (a->w[CAPLIMB] == 1); }
// (lv_is_cap: true when the value is exactly 64^256; values above cannot exist after any operation)
static int over_cap(const Lv *a){ if(a->w[CAPLIMB] > 1) return 1; if(a->w[CAPLIMB] == 1) for(int i = 0; i < CAPLIMB; i++) if(a->w[i]) return 1; return 0; }
static int clamp(Lv *r){ if(over_cap(r)){ lv_cap(r); return 1; } return 0; }

int lv_cmp(const Lv *a, const Lv *b){
    for(int i = LV_LIMBS - 1; i >= 0; i--){ if(a->w[i] != b->w[i]) return a->w[i] < b->w[i] ? -1 : 1; }
    return 0;
}
int lv_cmp_u64(const Lv *a, uint64_t v){
    for(int i = LV_LIMBS - 1; i >= 1; i--) if(a->w[i]) return 1;
    return a->w[0] < v ? -1 : a->w[0] > v ? 1 : 0;
}
int lv_bits(const Lv *a){
    for(int i = LV_LIMBS - 1; i >= 0; i--) if(a->w[i]) return i * 64 + 64 - __builtin_clzll(a->w[i]);
    return 0;
}
int lv_fits_u64(const Lv *a){ for(int i = 1; i < LV_LIMBS; i++) if(a->w[i]) return 0; return 1; }
uint64_t lv_to_u64(const Lv *a){ return lv_fits_u64(a) ? a->w[0] : UINT64_MAX; }

int lv_add(Lv *r, const Lv *a, const Lv *b){
    uint64_t c = 0; Lv t;
    for(int i = 0; i < LV_LIMBS; i++){ u128 s = (u128)a->w[i] + b->w[i] + c; t.w[i] = (uint64_t)s; c = (uint64_t)(s >> 64); }
    *r = t; return clamp(r);
}
int lv_add_u64(Lv *r, const Lv *a, uint64_t v){ Lv b; lv_from_u64(&b, v); return lv_add(r, a, &b); }
int lv_sub(Lv *r, const Lv *a, const Lv *b){
    if(lv_cmp(a, b) <= 0){ int hit = lv_cmp(a, b) < 0; lv_zero(r); return hit; }
    uint64_t br = 0; Lv t;
    for(int i = 0; i < LV_LIMBS; i++){ u128 d = (u128)a->w[i] - b->w[i] - br; t.w[i] = (uint64_t)d; br = (uint64_t)(d >> 64) & 1; }
    *r = t; return 0;
}
int lv_mul(Lv *r, const Lv *a, const Lv *b){
    uint64_t p[2 * LV_LIMBS]; memset(p, 0, sizeof p);
    for(int i = 0; i < LV_LIMBS; i++){
        if(!a->w[i]) continue;
        uint64_t c = 0;
        for(int j = 0; j < LV_LIMBS; j++){ u128 t = (u128)a->w[i] * b->w[j] + p[i + j] + c; p[i + j] = (uint64_t)t; c = (uint64_t)(t >> 64); }
        p[i + LV_LIMBS] += c;
    }
    int over = 0;
    for(int i = LV_LIMBS; i < 2 * LV_LIMBS; i++) if(p[i]) over = 1;
    Lv t; memcpy(t.w, p, sizeof t.w);
    if(over){ lv_cap(r); return 1; }
    *r = t; return clamp(r);
}
int lv_mul_small(Lv *r, const Lv *a, uint32_t m){
    uint64_t c = 0; Lv t;
    for(int i = 0; i < LV_LIMBS; i++){ u128 s = (u128)a->w[i] * m + c; t.w[i] = (uint64_t)s; c = (uint64_t)(s >> 64); }
    if(c){ lv_cap(r); return 1; }
    *r = t; return clamp(r);
}
uint32_t lv_div_small(Lv *q, const Lv *a, uint32_t d){
    if(!d){ *q = *a; return 0; }
    uint64_t rem = 0; Lv t;
    for(int i = LV_LIMBS - 1; i >= 0; i--){ u128 cur = ((u128)rem << 64) | a->w[i]; t.w[i] = (uint64_t)(cur / d); rem = (uint64_t)(cur % d); }
    *q = t; return (uint32_t)rem;
}
int lv_shl(Lv *r, const Lv *a, unsigned bits){
    if(lv_is_zero(a)){ lv_zero(r); return 0; }
    if(bits > 2048 || (int)bits + lv_bits(a) > LV_CAP_BITS + 1){ lv_cap(r); return 1; }
    unsigned ls = bits / 64, bs = bits % 64; Lv t; lv_zero(&t);
    for(int i = LV_LIMBS - 1; i >= 0; i--){
        int src = i - (int)ls; if(src < 0) continue;
        uint64_t v = a->w[src] << bs; if(bs && src > 0) v |= a->w[src - 1] >> (64 - bs);
        t.w[i] = v;
    }
    *r = t; return clamp(r);
}
void lv_shr(Lv *r, const Lv *a, unsigned bits){
    unsigned ls = bits / 64, bs = bits % 64; Lv t; lv_zero(&t);
    for(unsigned i = 0; i + ls < LV_LIMBS; i++){
        uint64_t v = a->w[i + ls] >> bs; if(bs && i + ls + 1 < LV_LIMBS) v |= a->w[i + ls + 1] << (64 - bs);
        t.w[i] = v;
    }
    *r = t;
}
int lv_pow_u32(Lv *r, uint32_t base, uint32_t e){
    Lv acc, b; lv_from_u64(&acc, 1); lv_from_u64(&b, base); int sat = 0;
    while(e){
        if(e & 1) sat |= lv_mul(&acc, &acc, &b);
        e >>= 1;
        if(e) sat |= lv_mul(&b, &b, &b);
    }
    *r = acc; return sat;
}

// ---------------------------------------------------------------- text
size_t lv_to_dec(const Lv *a, char *buf, size_t n){
    char tmp[LV_DEC_MAX + 16]; size_t len = 0; Lv t = *a;
    if(lv_is_zero(&t)){ tmp[len++] = '0'; }
    while(!lv_is_zero(&t)){
        uint32_t rem = lv_div_small(&t, &t, 1000000000u);
        for(int k = 0; k < 9 && len < sizeof tmp; k++){ tmp[len++] = (char)('0' + rem % 10); rem /= 10; }
    }
    while(len > 1 && tmp[len - 1] == '0') len--;                  // leading zeros (they sit at the end of tmp)
    if(n == 0) return len;
    size_t o = 0; for(size_t i = len; i > 0 && o + 1 < n; i--) buf[o++] = tmp[i - 1];
    buf[o] = 0;
    return o;
}
int lv_from_dec(Lv *r, const char *s){
    Lv v; lv_zero(&v); int digits = 0, sat = 0;
    for(; *s; s++){
        char c = *s;
        if(c == ',' || c == ' ' || c == '_') continue;
        if(c < '0' || c > '9'){ lv_zero(r); return -1; }
        digits++;
        if(sat) continue;
        if(lv_mul_small(&v, &v, 10)) sat = 1;
        else if(lv_add_u64(&v, &v, (uint64_t)(c - '0'))) sat = 1;
    }
    if(!digits){ lv_zero(r); return -1; }
    *r = v; return sat;
}
static unsigned get6(const Lv *a, int idx){      // base-64 digit number idx (0 = least significant)
    int bit = idx * 6, limb = bit / 64, off = bit % 64;
    uint64_t v = limb < LV_LIMBS ? a->w[limb] >> off : 0;
    if(off > 58 && limb + 1 < LV_LIMBS) v |= a->w[limb + 1] << (64 - off);
    return (unsigned)(v & 63);
}
int lv_b64_digits(const Lv *a, uint8_t *dig){
    int nb = (lv_bits(a) + 5) / 6; if(nb < 1) nb = 1;
    for(int i = 0; i < nb; i++) dig[i] = (uint8_t)get6(a, nb - 1 - i);
    return nb;
}
size_t lv_to_b64(const Lv *a, char *buf, size_t n){
    uint8_t d[LV_B64_MAX + 1]; int c = lv_b64_digits(a, d); size_t o = 0;
    for(int i = 0; i < c; i++){
        char t[4]; int l = 0; unsigned v = d[i];
        if(v >= 10) t[l++] = (char)('0' + v / 10);
        t[l++] = (char)('0' + v % 10);
        if(i && o + 1 < n) buf[o++] = '.';
        for(int k = 0; k < l && o + 1 < n; k++) buf[o++] = t[k];
    }
    if(n) buf[o] = 0;
    return o;
}
int lv_from_b64(Lv *r, const char *s){
    Lv v; lv_zero(&v); int sat = 0, any = 0;
    while(*s){
        if(*s < '0' || *s > '9'){ lv_zero(r); return -1; }
        unsigned d = 0; int nd = 0;
        while(*s >= '0' && *s <= '9'){ d = d * 10 + (unsigned)(*s - '0'); s++; if(++nd > 3) { lv_zero(r); return -1; } }
        if(d > 63){ lv_zero(r); return -1; }
        if(*s == '.') s++; else if(*s){ lv_zero(r); return -1; }
        any = 1;
        if(!sat){ if(lv_mul_small(&v, &v, 64)) sat = 1; else if(lv_add_u64(&v, &v, d)) sat = 1; }
    }
    if(!any){ lv_zero(r); return -1; }
    *r = v; return sat;
}

size_t lv_compact(const Lv *a, char *buf, size_t maxlen){
    char d[LV_DEC_MAX]; size_t len = lv_to_dec(a, d, sizeof d);
    if(maxlen < 5) maxlen = 5;
    if(len <= maxlen){ memcpy(buf, d, len + 1); return len; }
    // d.dddE<exp>: exponent text first, then as many fraction digits as fit
    int exp = (int)len - 1; char e[8]; int el = 0, x = exp; char t[8];
    while(x){ t[el++] = (char)('0' + x % 10); x /= 10; }
    for(int i = 0; i < el; i++) e[i] = t[el - 1 - i];
    size_t fixed = 1 + 1 + el;                                  // "d" + "E" + exp digits
    int frac = (int)maxlen - (int)fixed - 1;                     // minus the '.'
    if(frac < 0) frac = 0;
    if(frac > (int)len - 1) frac = (int)len - 1;
    size_t o = 0; buf[o++] = d[0];
    if(frac > 0){ buf[o++] = '.'; for(int i = 0; i < frac; i++) buf[o++] = d[1 + i]; }
    buf[o++] = 'E'; for(int i = 0; i < el; i++) buf[o++] = e[i];
    buf[o] = 0; return o;
}

void lv_to_bytes(const Lv *a, uint8_t *out){
    for(int i = 0; i < LV_LIMBS; i++) for(int k = 0; k < 8; k++) out[i * 8 + k] = (uint8_t)(a->w[i] >> (8 * k));
}
void lv_from_bytes(Lv *r, const uint8_t *in){
    for(int i = 0; i < LV_LIMBS; i++){ uint64_t v = 0; for(int k = 0; k < 8; k++) v |= (uint64_t)in[i * 8 + k] << (8 * k); r->w[i] = v; }
    clamp(r);
}
