// lvxp.h: experience and levels for the PC build's big level (Lv). Same rule as the game's xp_next():
//     xp needed to go from level L to L+1  =  (18 x L x L + 12) x pct / 100       (pct is the game's XP NEEDED option, 100 = normal)
// but exact for levels up to 64^256, and it can pay out many levels at once without looping.
//   cost of n levels from L  =  floor( (3 x (g(L+n-1) - g(L-1)) + 12 n) x pct / 100 ),   g(m) = m (m+1) (2m+1)
// (the percent is applied to the whole sum; at 100 it is exactly the sum of the one-level costs)
#ifndef LVXP_H
#define LVXP_H
#include "lv.h"
// xp to go up ONE level from L (saturates at 64^256)
void lvxp_need(const Lv *level, unsigned pct, Lv *out);
// Total xp for n levels from L. Returns 1 if that is too big to count (then out = 64^256 and it is more than anyone can have).
int  lvxp_cost(const Lv *level, const Lv *n, unsigned pct, Lv *out);
// Adds `add` xp to the bank and pays out every level it can. *gained = how many levels went up. Returns 1 when the level is at 64^256.
// At the top the bank is emptied (like the game at its cap).
int  lvxp_gain(Lv *level, Lv *bank, const Lv *add, unsigned pct, Lv *gained);
// bank / need as a number 0..scale (for the bar). need is lvxp_need(level).
unsigned lvxp_ratio(const Lv *bank, const Lv *need, unsigned scale);
#endif
