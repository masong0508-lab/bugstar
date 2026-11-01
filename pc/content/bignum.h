// bignum.h: a small fixed-size unsigned big number for the PC port's huge addresses and distances (no library).
// 32-bit words, least significant first. BN_W words = 5632 bits: room for the biggest trip (about 2^5336 light years).
#ifndef BIGNUM_H
#define BIGNUM_H
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define BN_W 176
typedef struct { uint32_t w[BN_W]; } Big;

static inline void bn_zero(Big *a){ memset(a, 0, sizeof *a); }
static inline void bn_set(Big *a, uint32_t v){ bn_zero(a); a->w[0] = v; }
static inline int  bn_cmp(const Big *a, const Big *b){ for(int i = BN_W - 1; i >= 0; i--) if(a->w[i] != b->w[i]) return a->w[i] > b->w[i] ? 1 : -1; return 0; }
static inline int  bn_bits(const Big *a){ for(int i = BN_W - 1; i >= 0; i--) if(a->w[i]){ int b = 32; uint32_t v = a->w[i]; while(!(v & 0x80000000u)){ v <<= 1; b--; } return i * 32 + b; } return 0; }
static inline void bn_mul_u32(Big *a, uint32_t m){ uint64_t c = 0; for(int i = 0; i < BN_W; i++){ uint64_t t = (uint64_t)a->w[i] * m + c; a->w[i] = (uint32_t)t; c = t >> 32; } }
static inline void bn_add(Big *a, const Big *b){ uint64_t c = 0; for(int i = 0; i < BN_W; i++){ uint64_t t = (uint64_t)a->w[i] + b->w[i] + c; a->w[i] = (uint32_t)t; c = t >> 32; } }
static inline void bn_shl(Big *a, int n){                       // a <<= n bits (bits shifted past the top are lost)
    int ws = n / 32, bs = n % 32;
    for(int i = BN_W - 1; i >= 0; i--){
        uint32_t lo = i - ws >= 0 ? a->w[i - ws] : 0, lo2 = i - ws - 1 >= 0 ? a->w[i - ws - 1] : 0;
        a->w[i] = bs ? (lo << bs) | (lo2 >> (32 - bs)) : lo;
    }
}
static inline void bn_pow2(Big *a, int n){ bn_set(a, 1); bn_shl(a, n); }
static inline void bn_mul(Big *r, const Big *a, const Big *b){  // r = a * b (needs a*b to fit; the high part is dropped)
    Big t; bn_zero(&t);
    for(int i = 0; i < BN_W; i++){ uint64_t c = 0; if(!a->w[i]) continue;
        for(int j = 0; i + j < BN_W; j++){ uint64_t x = (uint64_t)a->w[i] * b->w[j] + t.w[i + j] + c; t.w[i + j] = (uint32_t)x; c = x >> 32; } }
    *r = t;
}
static inline uint32_t bn_divmod_u32(Big *a, uint32_t d){ uint64_t r = 0; for(int i = BN_W - 1; i >= 0; i--){ uint64_t t = (r << 32) | a->w[i]; a->w[i] = (uint32_t)(t / d); r = t % d; } return (uint32_t)r; }
static inline int  bn_is_zero(const Big *a){ for(int i = 0; i < BN_W; i++) if(a->w[i]) return 0; return 1; }
// decimal digits (most significant first) into out; returns the digit count
static inline int bn_to_dec(const Big *a, char *out, int cap){
    Big t = *a; char tmp[BN_W * 10 + 2]; int n = 0;
    if(bn_is_zero(&t)){ out[0] = '0'; out[1] = 0; return 1; }
    while(!bn_is_zero(&t) && n < (int)sizeof tmp - 10){ uint32_t r = bn_divmod_u32(&t, 1000000000u); for(int k = 0; k < 9; k++){ tmp[n++] = '0' + r % 10; r /= 10; } }
    while(n > 1 && tmp[n - 1] == '0') n--;
    if(n >= cap) n = cap - 1;
    for(int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0; return n;
}
// scientific form like 3.232E+616 (4 significant digits)
static inline void bn_sci(const Big *a, char *out){
    char d[BN_W * 10 + 2]; int n = bn_to_dec(a, d, sizeof d);
    if(n <= 4){ snprintf(out, 24, "%.4s", d); return; }
    int m = (d[0] - '0') * 1000 + (d[1] - '0') * 100 + (d[2] - '0') * 10 + (d[3] - '0'), e = n - 1;   // round on the 5th digit
    if(d[4] >= '5') m++;
    if(m == 10000){ m = 1000; e++; }
    snprintf(out, 24, "%d.%03dE+%d", m / 1000, m % 1000, e);
}
#endif
