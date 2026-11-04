// ws_mix_sy.h: the glue between the ship editor (pc/shipyard) and the warship interiors (pc/warship): a design's rooms and props as a WdMix.
// Header only, so ws_deck.c and sy_design.c stay apart (the warship tests build without the shipyard, the shipyard tests without the warships).
#ifndef WS_MIX_SY_H
#define WS_MIX_SY_H
#include "ws_deck.h"
#include "../shipyard/sy_design.h"
static inline void wd_mix_from_sy(const SyDesign *d, WdMix *m){
    SyRooms r; sy_rooms(d, &r);
    m->reactor = r.reactor; m->hangar = r.hangar; m->quarters = r.quarters; m->bridge = r.bridge; m->turret = r.turret; m->shieldgen = r.shieldgen; m->workshop = r.workshop; m->mess = r.mess;
    m->bunks = r.bunks; m->crates = r.crates; m->guns = r.guns; m->fighters = r.fighters;
}
#endif
