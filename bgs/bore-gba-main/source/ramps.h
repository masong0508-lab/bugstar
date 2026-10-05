// ramps.h - kicker ramps and quarter pipes: which tile chars they are and how high the surface is at any point inside the tile.
// Kicker = '1'..'4', quarter pipe = '5'..'8'; the digit is the side the ramp FACES (its low entry side): 0 S(+y) 1 E(+x) 2 N(-y) 3 W(-x),
// the same numbering as itemFacing(). The lip is on the opposite side. Include BEFORE tileH().
// RAMP TUNING (see lifeStep): F_RAMP_BOOST turns the climb speed (8.8 px/step) into launch speed, F_RAMP_MAX caps it.
#include "rampdata.h"
#define F_RAMP_BOOST 4      // launch vz = (smoothed px climbed per step, 8.8) * this
#define F_RAMP_MAX   0x300  // never launch harder than this: ~18 px above the lip (an ollie is ~0x388). Keep lip + this under 34 px or landings scream
#define F_RAMP_TOL   6      // px a ramp surface may rise between two steps before it counts as a wall (flat ground uses 3)
static inline int isKicker(char c){ return c>='1'&&c<='4'; }
static inline int isQPipe(char c){ return c>='5'&&c<='8'; }
static inline int isLaunch(char c){ return c>='9'&&c<='<'; }   // launch ramp (pack 2): '9' ':' ';' '<' = faces S E N W
static inline int isRamp(char c){ return (c>='1'&&c<='8')||isLaunch(c); }
static int rampH(char c,int fx,int fy){   // surface height (px) at a position inside the tile (fx,fy in 1/256 tile)
    int d=isKicker(c)?c-'1':isQPipe(c)?c-'5':c-'9', t;      // t 0..255 = how far from the low (entry) edge towards the lip
    switch(d){ case 0: t=255-(fy&255); break; case 1: t=255-(fx&255); break; case 2: t=fy&255; break; default: t=fx&255; }
    return isKicker(c)?(t*(KICKER_H+1))>>8:isLaunch(c)?(t*(LAUNCH_H+1))>>8:qpH[t>>5];
}
