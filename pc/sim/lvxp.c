// lvxp.c: see lvxp.h
#include "lvxp.h"

static unsigned fixpct(unsigned pct){ return pct ? pct : 1; }

// floor(S x pct / 100), exactly; 1 if it is more than 64^256
static int pct_of(const Lv *S, unsigned pct, Lv *out){
    if(pct == 100){ *out = *S; return 0; }
    Lv q, a, b; uint32_t r = lv_div_small(&q, S, 100); int s = 0;
    s |= lv_mul_small(&a, &q, pct);
    lv_from_u64(&b, (uint64_t)r * pct / 100); s |= lv_add(&a, &a, &b);
    *out = a; return s;
}
// cost of n levels from L:  18 n L^2 + 18 L n (n-1) + 3 n (n-1) (2n-1) + 12 n   (every term is positive, so if one overflows the total does)
int lvxp_cost(const Lv *level, const Lv *n, unsigned pct, Lv *out){
    Lv t1, t2, t3, t4, nm1, a, b, S; int s = 0; pct = fixpct(pct);
    if(lv_is_zero(n)){ lv_zero(out); return 0; }
    lv_sub(&nm1, n, &(Lv){ .w = { 1 } });
    s |= lv_mul(&a, level, level); s |= lv_mul(&a, &a, n); s |= lv_mul_small(&t1, &a, 18);
    s |= lv_mul(&a, level, n); s |= lv_mul(&a, &a, &nm1); s |= lv_mul_small(&t2, &a, 18);
    s |= lv_mul(&a, n, &nm1); s |= lv_mul_small(&b, n, 2); lv_sub(&b, &b, &(Lv){ .w = { 1 } }); s |= lv_mul(&a, &a, &b); s |= lv_mul_small(&t3, &a, 3);
    s |= lv_mul_small(&t4, n, 12);
    s |= lv_add(&S, &t1, &t2); s |= lv_add(&S, &S, &t3); s |= lv_add(&S, &S, &t4);
    if(!s) s |= pct_of(&S, pct, &S);
    if(s){ lv_cap(out); return 1; }
    *out = S; return 0;
}
void lvxp_need(const Lv *level, unsigned pct, Lv *out){
    Lv one; lv_from_u64(&one, 1); lvxp_cost(level, &one, pct, out);          // (too big counts as the top: 64^256)
}
int lvxp_gain(Lv *level, Lv *bank, const Lv *add, unsigned pct, Lv *gained){
    Lv room, lo, hi, mid, d, c; lv_zero(gained);
    lv_add(bank, bank, add);
    Lv cap; lv_cap(&cap);
    if(lv_cmp(level, &cap) >= 0){ *level = cap; lv_zero(bank); return 1; }
    lv_sub(&room, &cap, level);                                // most levels it could still gain
    lv_zero(&lo); hi = room;
    while(lv_cmp(&lo, &hi) < 0){                               // the largest n with cost(n) <= bank
        lv_sub(&d, &hi, &lo); lv_add_u64(&d, &d, 1); lv_shr(&d, &d, 1); lv_add(&mid, &lo, &d);
        int over = lvxp_cost(level, &mid, pct, &c);
        if(!over && lv_cmp(&c, bank) <= 0) lo = mid;
        else { lv_sub(&hi, &mid, &(Lv){ .w = { 1 } }); }
    }
    if(lv_is_zero(&lo) && lv_cmp(bank, &cap) == 0){                 // a level that costs more than the biggest possible bank is paid by a full bank
        Lv one; lv_from_u64(&one, 1);
        if(lvxp_cost(level, &one, pct, &c)){ lv_add(level, level, &one); lv_zero(bank); *gained = one; }
    }
    if(!lv_is_zero(&lo)){
        lvxp_cost(level, &lo, pct, &c); lv_sub(bank, bank, &c);
        lv_add(level, level, &lo); *gained = lo;
    }
    if(lv_cmp(level, &cap) >= 0){ *level = cap; lv_zero(bank); return 1; }
    return 0;
}
unsigned lvxp_ratio(const Lv *bank, const Lv *need, unsigned scale){
    int bn = lv_bits(need), sh = bn > 30 ? bn - 30 : 0; Lv a, b;
    lv_shr(&a, bank, (unsigned)sh); lv_shr(&b, need, (unsigned)sh);
    uint64_t x = lv_to_u64(&a), y = lv_to_u64(&b);
    if(!y) return 0;
    uint64_t r = x * scale / y; return r > scale ? scale : (unsigned)r;
}
