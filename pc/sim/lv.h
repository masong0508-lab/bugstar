// lv.h: Lv, the PC build's character level number. Unsigned, saturating, exactly 64^256 (= 2^1536) at the top.
//
// 64^256 needs 1537 bits, so an Lv has 25 limbs of 64 bits (1600 bits): 1536 bits of level plus guard room.
// Every operation that would go past 64^256 gives exactly 64^256 (and says so in its return value).
// Plain C99 + unsigned __int128. No floating point, no allocation, no globals. Same answer on every machine.
#ifndef LV_H
#define LV_H
#include <stdint.h>
#include <stddef.h>

#define LV_LIMBS   25
#define LV_CAP_BITS 1536            // the cap is 2^LV_CAP_BITS = 64^256
#define LV_DEC_MAX 470              // buffer size that always holds the decimal text (64^256 has 463 digits) plus the 0
#define LV_B64_MAX 257              // most base-64 digits an Lv has (64^256 is a 1 and 256 zeros)

typedef struct { uint64_t w[LV_LIMBS]; } Lv;     // little endian limbs

void lv_zero(Lv *r);
void lv_from_u64(Lv *r, uint64_t v);
void lv_cap(Lv *r);                                // r = 64^256
int  lv_is_zero(const Lv *a);
int  lv_is_cap(const Lv *a);
int  lv_cmp(const Lv *a, const Lv *b);            // -1, 0, 1
int  lv_cmp_u64(const Lv *a, uint64_t v);
int  lv_bits(const Lv *a);                         // number of bits (0 for zero)
uint64_t lv_to_u64(const Lv *a);                   // the value, or UINT64_MAX when it is bigger
int  lv_fits_u64(const Lv *a);

// Each returns 1 if the result hit the top (r is then exactly 64^256), else 0. For sub, 1 means it hit the bottom (r = 0).
int  lv_add(Lv *r, const Lv *a, const Lv *b);
int  lv_add_u64(Lv *r, const Lv *a, uint64_t v);
int  lv_sub(Lv *r, const Lv *a, const Lv *b);     // saturates at 0
int  lv_mul(Lv *r, const Lv *a, const Lv *b);
int  lv_mul_small(Lv *r, const Lv *a, uint32_t m); // multiply by a small number
uint32_t lv_div_small(Lv *q, const Lv *a, uint32_t d);   // q = a / d; returns a % d. d must not be 0 (then q = a, returns 0)
int  lv_shl(Lv *r, const Lv *a, unsigned bits);   // times 2^bits, saturating
void lv_shr(Lv *r, const Lv *a, unsigned bits);
int  lv_pow_u32(Lv *r, uint32_t base, uint32_t e); // base^e, saturating (lv_pow_u32(r,64,256) is the cap)

// Text. Decimal is exact. Base 64 is the digits of the number in base 64, most significant first, each written as its value 0..63.
size_t lv_to_dec(const Lv *a, char *buf, size_t n);   // returns length; buf needs LV_DEC_MAX to be safe
int  lv_from_dec(Lv *r, const char *s);               // 0 ok, 1 ok but saturated, -1 bad text (r = 0). Commas, spaces and _ are skipped.
int  lv_b64_digits(const Lv *a, uint8_t *dig);        // fills dig[] (up to LV_B64_MAX), most significant first; returns the count (zero has 1 digit: 0)
size_t lv_to_b64(const Lv *a, char *buf, size_t n);   // "1.0.0.0": digit values joined with dots; returns length
int  lv_from_b64(Lv *r, const char *s);               // reads that text back. 0 ok, 1 saturated, -1 bad

// Compact text for the HUD: whole number when it is short, else "1.23E461". Never longer than maxlen characters (maxlen >= 5).
size_t lv_compact(const Lv *a, char *buf, size_t maxlen);
// Save format: 200 bytes, little endian, fixed.
#define LV_BYTES 200
void lv_to_bytes(const Lv *a, uint8_t *out);
void lv_from_bytes(Lv *r, const uint8_t *in);         // anything above 64^256 becomes 64^256

#endif
