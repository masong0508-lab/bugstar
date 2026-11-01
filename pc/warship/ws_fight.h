// ws_fight.h: the warship fight rules (plain C, deterministic, no drawing). The game (pc/pc_warship.inc) drives it once a frame with
// your keys; the bot tests drive it with a simple pilot. Everything a colossal craft does to make a cheap win impossible is here:
// regenerating shield layers, fighter waves, point defence, repair crews, phases and an enraged last phase, sections in order,
// heat, ammo and fuel on your side, no safe spot (the turrets cover every angle; only speed and fuel let you dodge).
#ifndef WS_FIGHT_H
#define WS_FIGHT_H
#include "ws.h"

typedef struct {                         // your ship as you bring it into the fight (the preparation)
    double attack;                       // levels' worth (ws_attack)
    int hull_max, shield_max, armour;    // armour = percent of damage you take (100 = none stopped, 50 = half)
    int fuel_max, wing, kits;            // fuel, wingmen, repair kits (each gives back 40 hull)
    int ammo[WW_N];                      // shots of each weapon (laser and railgun: -1 = no limit; 0 = not fitted)
} WsShip;

typedef struct { int fire, weapon, boost, retreat, kit, target; } WsInput;   // target: -1 the hull section, -2 fighters, 0.. a subsystem

enum { WF_ON, WF_WON, WF_LOST, WF_FLED };
typedef struct {
    WsCraft c;                           // the craft (sections and subsystems change as it is hurt)
    WsShip ship;
    double hull, shield, fuel, heat, t, since_hit, since_my_hit, fire_cd, kit_cd, fighter_t, sh_back_t, wing_hp[10];
    int overheat, wing, kits, ammo[WW_N];
    int layers_up; WN lay_hp;            // shield layers still up, and the HP of the one on top
    int sec, phase, fighters, state, nsab;
    int turrets0, hangars0, gens0, reacs0;
    WsKill kill;
    WN out_total;                        // hull damage you did from outside (repairs can undo it: this is what you did, not what is left)
    unsigned shots, hits, intercepted, waves, repairs;
    char msg[48]; int msg_t;             // the last thing worth saying ("SHIELD LAYER 2 DOWN")
    uint32_t rng;
} WsFight;

void wf_start(WsFight *f, const WsCraft *c, const WsShip *s);
void wf_tick(WsFight *f, const WsInput *in, double dt);
int  wf_alive(const WsFight *f, int kind);           // subsystems of a kind still working
void wf_sabotage(WsFight *f, int sub, int precise);  // a subsystem blown up from the inside (boarding)
void wf_away(WsFight *f, double seconds);            // you left: the crews repair. Short trips keep destroyed sections; long ones lose everything
WN   wf_hull_left(const WsFight *f);
double wf_heat_max(void);
double wf_rate(int w);                               // shots a second
void wf_bot(const WsFight *f, WsInput *in, int clever);   // a simple pilot for the tests (clever: manages heat, kits, fighters)
const char *wf_phase_name(int p);                    // SHIELDS, OUTER HULL, INNER HULL, ENRAGED CORE
#endif
