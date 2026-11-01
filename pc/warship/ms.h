// ms.h: WARSHIP MISSIONS (plain C, deterministic). A place (the space menu, a station, a town, an outpost) offers missions made from its
// own seed. The target craft is made from the mission's seed, so it is the same craft every time. Pay = the ONE reward formula
// (ws_pay) times the mission's share, plus the mission bonus. No game code here; the glue is in pc/pc_warship.inc.
#ifndef MS_H
#define MS_H
#include "ws.h"
enum { MK_DESTROY, MK_SABOTAGE, MK_STEAL, MK_RESCUE, MK_ESCORT, MK_DEFEND, MK_SURVEY, MK_N };
enum { MP_SPACE, MP_STATION, MP_TOWN, MP_OUTPOST, MP_N };
#define MS_NSYS 32                      // stars in a cluster (the game's NSYS)
#define MS_SLOTS 5                      // missions on a board (a sixth, the CHEAT CONTRACT, only in a cheated world)
#define MS_NFAC 12
typedef struct {                        // what the glue tells us about the place
    uint32_t seed, gal;                 // the place's own seed; the galaxy key
    int kind, danger, zplayer, theme, law, cheat_world, epoch;
    uint32_t pool[MS_NSYS]; int npool;  // system ids in this cluster a mission may point at
} MsPlace;
typedef struct {
    uint32_t id, cseed; int z, cheat;   // identity; the target craft (seed, size, cheated mark)
    uint8_t kind, place, dream, far, objsub, faction, slot, state;   // state: 0 offered, 1 active
    uint8_t prog;                       // the inside objective is done (it is paid when you leave the craft)
    int bonus, dl_rel, dl_abs;          // bonus percent, days to do it (0 none), the jump count it ends at (set on accept)
    uint32_t gal, tsys; int tidx, plan; // target galaxy key, target system id (0 = far galaxy), its star number, planet 1..8
} WsMission;
const char *ms_kind_name(int k);        // "DESTROY" ...
const char *ms_kind_about(int k);       // one line
const char *ms_place_name(int p);
const char *ms_faction_name(int f);
double ms_share(int kind);              // share of the full pay (1 for kills, less for inside jobs)
int  ms_inside(int kind);               // 1 if the job is done on board
void ms_offer(WsMission *m, const MsPlace *pl, int slot);
void ms_giver(const WsMission *m, char *out, int cap);
void ms_line(const WsMission *m, const char *craft, int row, char *out, int cap);   // briefing rows 0..2
void ms_pay(const WsCraft *c, const WsKill *k, double attack, const WsMission *m, WsPay *out);   // k = NULL: an inside job
int  ms_pack(const WsMission *m, char *out, int cap);
int  ms_unpack(const char *s, WsMission *m);
#endif
