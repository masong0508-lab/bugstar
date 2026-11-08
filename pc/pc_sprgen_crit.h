// pc_sprgen_crit.h: shared parts of the creature generators (pc_sprgen_crit*.c). Internal.
#ifndef PC_SPRGEN_CRIT_H
#define PC_SPRGEN_CRIT_H
#include "pc_sprgen_i.h"
// A creature being drawn: its ramps, its variant numbers (size 0..3, diet 0 graze 1 hunt 2 scavenge, habitat, behaviour) and its individual numbers.
typedef struct Cr { SgCv cv; SgRng r; SgRamp b, bl, ac, dk, bn; uint32_t glow, eyec; int plan, size, diet, hab, beh, mut, rank, gift, col, front, copy; float hs, es, ll, ek, t, bx, by, brx, bry, ex, ey, er; } Cr;
#define CG 57.0f                                   // the ground line of the design grid
void cr_eye(Cr *c, float x, float y, float r);     // an eye: white, pupil, highlight, brow by temper; glows in caves
void cr_mouth(Cr *c, float x, float y, float len, float dir);   // a mouth line (fangs on hunters)
void cr_leg(Cr *c, float x0, float y0, float x1, float y1, float th, int back, int digi);   // a two-part leg ending in a foot (claws on hunters); back = far side, darker
void cr_ear(Cr *c, float x, float y, float s, int type, float dir);
void cr_horn(Cr *c, float x, float y, float s, int type, float dir);
void cr_beast(Cr *c, int k);     // k = plan - 0 (0..10)
void cr_flier(Cr *c, int k);
void cr_swim(Cr *c, int k);
void cr_crawl(Cr *c, int k);
void cr_spirit(Cr *c, int k);
void cr_monster(Cr *c, int k);
#endif
