// pc_sprgen_i.h: the drawing kit shared by the generators (pc_sprgen*.c). Not for the game: it only includes pc_sprgen.h.
// A canvas draws shapes given in DESIGN units (a 64 x 64 grid by default) with anti-aliased edges, colour RAMPS (dark .. light) and light from the upper left.
#ifndef PC_SPRGEN_I_H
#define PC_SPRGEN_I_H
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "pc_sprgen.h"

typedef struct { uint64_t s; } SgRng;
static inline uint64_t sg_m64(uint64_t z){ z += 0x9E3779B97F4A7C15ull; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
static inline SgRng sg_rng(uint64_t a, uint64_t b){ SgRng r; r.s = sg_m64(a * 0xD6E8FEB86659FD93ull + sg_m64(b + 0x51ED27)); return r; }
static inline uint32_t sg_rn(SgRng *r){ r->s += 0x9E3779B97F4A7C15ull; uint64_t z = r->s; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return (uint32_t)((z ^ (z >> 31)) >> 16); }
static inline float sg_rf(SgRng *r){ return (float)(sg_rn(r) & 0xFFFF) * (1.0f / 65536.0f); }               // 0 .. 1
static inline float sg_rr(SgRng *r, float a, float b){ return a + (b - a) * sg_rf(r); }
static inline int   sg_ri(SgRng *r, int n){ return n <= 1 ? 0 : (int)(sg_rn(r) % (uint32_t)n); }
static inline float sg_rs(SgRng *r){ return sg_rf(r) * 2.0f - 1.0f; }                                       // -1 .. 1
static inline uint32_t sg_h2(uint32_t a, uint32_t b){ return (uint32_t)(sg_m64(((uint64_t)a << 32) | b) >> 20); }
static inline float sg_clampf(float v, float lo, float hi){ return v < lo ? lo : v > hi ? hi : v; }
static inline float sg_lerp(float a, float b, float t){ return a + (b - a) * t; }
static inline float sg_smooth(float t){ t = sg_clampf(t, 0, 1); return t * t * (3 - 2 * t); }
#define SG_PI 3.14159265f
#define SG_TAU 6.2831853f

// ---- colours: 0xRRGGBB
static inline uint32_t sg_rgb(int r, int g, int b){ r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b; return (uint32_t)(r << 16 | g << 8 | b); }
static inline uint32_t sg_mixc(uint32_t a, uint32_t b, float t){
    t = sg_clampf(t, 0, 1); int ar = a >> 16 & 255, ag = a >> 8 & 255, ab = a & 255, br = b >> 16 & 255, bg = b >> 8 & 255, bb = b & 255;
    return sg_rgb((int)(ar + (br - ar) * t + 0.5f), (int)(ag + (bg - ag) * t + 0.5f), (int)(ab + (bb - ab) * t + 0.5f)); }
uint32_t sg_hsv(float h, float s, float v);
void     sg_tohsv(uint32_t c, float *h, float *s, float *v);
static inline uint32_t sg_scale(uint32_t c, float k){ return sg_rgb((int)((c >> 16 & 255) * k), (int)((c >> 8 & 255) * k), (int)((c & 255) * k)); }
typedef struct { uint32_t c[5]; } SgRamp;                      // dark .. light
void     sg_ramp(SgRamp *r, uint32_t base);                    // shadows lean cool and dark, lights lean warm and pale
uint32_t sg_rampc(const SgRamp *r, float lit, int post);       // lit 0..1; post = number of flat steps (0 = smooth)
uint32_t sg_gray(float v);

// ---- noise
float sg_vnoise(float x, float y, uint32_t seed);              // value noise 0..1
float sg_fbm(float x, float y, uint32_t seed, int oct);        // 0..1
float sg_wnoise(float x, float y, uint32_t seed);              // cellular (distance to the nearest point), 0..~1

// ---- the canvas
enum { SH_FLAT, SH_SPHERE, SH_VERT, SH_HORZ, SH_FLATLIT };     // how a shape is lit
typedef struct SgCv { SgImg *im; float s; float k, ox, oy; int post; float lx, ly, lz; float alpha; uint32_t *snap; } SgCv;       // pixel = (design * k + o) * s
void cv_init(SgCv *cv, SgImg *im, float design);
void cv_view(SgCv *cv, float ox, float oy, float k);
void cv_snap(SgCv *cv);                                        // from now on patterns only touch pixels changed since this call (when the picture already has a background)
void cv_unsnap(SgCv *cv);           // draw the next shapes inside a box: design (0..64) -> (ox..ox+64k)
void cv_ell(SgCv *cv, float cx, float cy, float rx, float ry, float rot, const SgRamp *rp, int sh);
void cv_cap(SgCv *cv, float x0, float y0, float r0, float x1, float y1, float r1, const SgRamp *rp, int sh);
void cv_poly(SgCv *cv, const float *xy, int n, const SgRamp *rp, float lit0, float lit1);       // star-convex polygon (fan from its centre)
void cv_rect(SgCv *cv, float x0, float y0, float x1, float y1, const SgRamp *rp, float lit0, float lit1);   // lit0 at the top, lit1 at the bottom
void cv_rrect(SgCv *cv, float x0, float y0, float x1, float y1, float r, const SgRamp *rp, int sh);
void cv_tri(SgCv *cv, float x0, float y0, float x1, float y1, float x2, float y2, const SgRamp *rp, float lit0, float lit1);
void cv_curve(SgCv *cv, float x0, float y0, float cx, float cy, float x1, float y1, float r0, float r1, const SgRamp *rp, int sh);   // a stroke along a quadratic curve
void cv_glow(SgCv *cv, float x, float y, float r, uint32_t col, float alpha);                  // a soft round light
void cv_dot(SgCv *cv, float x, float y, float r, uint32_t col, float alpha);                   // a flat anti-aliased dot
void cv_flat(SgCv *cv, float x0, float y0, float x1, float y1, float w, uint32_t col, float alpha);   // a flat line of width w
void cv_text(SgCv *cv, float x, float y, const char *s, float px, uint32_t col);                   // the game's 3x5 letters, px = design units per letter pixel (x,y: top left)
float cv_textw(const char *s, float px);
// patterns over what is already drawn (only on pixels with alpha): 0 stripes, 1 spots, 2 dapples, 3 speckle, 4 bands, 5 scales, 6 stars, 7 checks
void cv_pattern(SgCv *cv, int kind, uint32_t col, float scale, float amt, uint32_t seed, float x0, float y0, float x1, float y1);
void cv_outline(SgCv *cv, uint32_t col, float strength);       // a dark contour around everything drawn (1 pixel thick, thicker on big images)
void cv_mirror(SgCv *cv);                                      // copy the left half onto the right, flipped
void cv_shadow(SgCv *cv, float cx, float cy, float rx, float ry, float alpha);   // a soft ground shadow under the sprite
void cv_fade(SgCv *cv, float y0, float y1, float a0, float a1);                  // multiply the alpha by a vertical ramp

static inline float sg_len(float x, float y){ return sqrtf(x * x + y * y); }
// a few shared tables
extern const uint16_t sg_font35[64];                           // 3x5 glyphs for ASCII 32..95
uint32_t sg_pal_people(int stock, SgRng *r, uint32_t out[6]);  // (pc_sprgen_life.c) a few harmonious colours for a people
#endif
