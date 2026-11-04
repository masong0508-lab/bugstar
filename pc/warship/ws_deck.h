// ws_deck.h: the PROCEDURAL INTERIOR of a warship (plain C, deterministic, no drawing).
// The whole craft is a tree: the root is the whole hull, every node splits into 8 sectors (2 x 2 x 2), down to DECKS (a 64 x 64 cell floor
// with rooms, corridors, doors, crew and props). Nothing is stored: a node is made from the craft seed and its path when you go there, so a
// universe hulk (about 80 levels deep) costs no more memory than a frigate (0 levels: the craft is one deck). Every subsystem has a fixed path,
// so it can always be reached: go down the sectors the map marks, then through the deck to its room.
#ifndef WS_DECK_H
#define WS_DECK_H
#include "ws.h"

#define WD_W 64
#define WD_MAXROOM 40
#define WD_MAXPROP 480
#define WD_MAXNPC 48
#define WD_LEAF_M 400.0                  // a deck block is about 400 m across in a big craft (smaller craft: the whole craft is one deck)

enum { WDC_SOLID, WDC_OPEN, WDC_DOOR, WDC_LOCK, WDC_WINDOW, WDC_PIT, WDC_VENT, WDC_TRAM };   // cell kinds (TRAM: the platform edge you leave from)
enum { WR_CORRIDOR, WR_HANGAR, WR_REACTOR, WR_CONTROL, WR_QUARTERS, WR_MESS, WR_LOUNGE, WR_GARDEN, WR_WORKSHOP, WR_TRENCH, WR_TRAM,
       WR_SHIELDGEN, WR_TURRET, WR_BRIDGE, WR_SECURITY, WR_NKIND };
enum { WP_FIGHTER, WP_CONSOLE, WP_BUNK, WP_TABLE, WP_PLANT, WP_CRATE, WP_CORE, WP_DOME, WP_GUN, WP_TRAMCAR, WP_LAMP, WP_PIPE, WP_SOFA,
       WP_BENCH, WP_KEY, WP_SCHEMA, WP_CHARGE, WP_NKIND };
enum { WN_CREW, WN_GUARD, WN_OFFICER };
enum { WA_SLEEP, WA_EAT, WA_CHAT, WA_WORK, WA_WALK, WA_FLEE, WA_HUNT, WA_DEAD };
enum { WB_MOLECULE, WB_MICRO, WB_SMALL, WB_NORMAL, WB_VAST, WB_COSMIC };

typedef struct { uint8_t kind, room, wtex, ftex, ctex, flags; int8_t floor, ceil; } WdCell;      // heights in half metres (eye at 3.2)
#define WF_LIGHT 1      // a light strip in the ceiling
#define WF_RAIL 2       // a catwalk edge
#define WF_TARGET 4     // the room of something to destroy
typedef struct { uint8_t kind, x0, y0, x1, y1, sub, locked, key; int8_t floor, ceil; uint8_t r, g, b; char name[24]; } WdRoom;   // sub: subsystem index or 255
typedef struct { uint8_t type, a, room; float x, y, z, w, h; } WdProp;
typedef struct { uint8_t kind, act, home, alive, dir, seen; float x, y, tx, ty, hp, cool, chat; int spot[4][2]; } WdNpc;

typedef struct {
    uint32_t seed, leaf; int band, cult, depth; double cell_m;     // cell_m: metres across one cell
    WdCell cell[WD_W*WD_W]; uint8_t lr[WD_W*WD_W], lg[WD_W*WD_W], lb[WD_W*WD_W];   // light per cell
    int nroom; WdRoom room[WD_MAXROOM];
    int nprop; WdProp prop[WD_MAXPROP];
    int nnpc; WdNpc npc[WD_MAXNPC];
    int sx, sy; float sdir;              // where you start
    int tram_room, nsubs, subs[WS_MAXSUB];
    char name[40];
    int alarm; double clock;             // the ship's clock (hours, 0..24): it decides what the crew are doing
} WdDeck;

int  wd_levels(const WsCraft *c);                       // how many levels of sectors above the decks (0: the craft is one deck)
uint32_t wd_root(const WsCraft *c);
uint32_t wd_child(uint32_t node, int child, int level);
int  wd_sub_child(const WsCraft *c, int sub, int level);     // the child (0..7) at this level on the way to a subsystem
int  wd_band(const WsCraft *c);
void wd_level_name(const WsCraft *c, int level, uint32_t h, char *out, int cap);   // "MOON SECTOR KETH 4"
double wd_level_m(const WsCraft *c, int level);         // how big a node of this level is (m)
// the subsystems in the deck at this path (path[0..depth-1]); returns how many, list in subs
int  wd_subs_at(const WsCraft *c, const uint8_t *path, int depth, int *subs);
void wd_make(WdDeck *d, const WsCraft *c, uint32_t leaf, const int *subs, int nsubs, int from_hangar, int depth);

// ---- ROOM MIX: let a ship design (pc/shipyard sy_rooms) say which rooms and props a deck has, instead of the builder's own random mix.
// The fields are the same as the shipyard's SyRooms (same order, same meaning), so the glue is a field-by-field copy; this header does not
// include the shipyard. The rooms: reactor, hangar, quarters, bridge, shieldgen, workshop, mess are wanted (1) or not (0); turret is how many
// turret rooms (0..6). The props: bunks, crates, guns, fighters are how many to put in the matching rooms (bunks in QUARTERS, crates in
// HANGAR / WORKSHOP / SECURITY, guns in TURRET rooms, fighters in HANGAR). A big count gets more rooms (a hangar per 10 fighters, a
// quarters per 12 bunks), so the number you ask for is the number you get while there is floor for it.
// The craft's own subsystem rooms (the targets: sub != 255) are never changed, and a target reactor, bridge or shield generator counts as the design's own. Same craft, leaf and mix: the same deck. wd_make is wd_make_mix with no mix, and its decks are exactly what they always were.
typedef struct { uint8_t reactor, hangar, quarters, bridge, turret, shieldgen, workshop, mess; uint8_t bunks, crates, guns, fighters; } WdMix;
void wd_make_mix(WdDeck *d, const WsCraft *c, uint32_t leaf, const int *subs, int nsubs, int from_hangar, int depth, const WdMix *mix);
int  wd_count_props(const WdDeck *d, int type);         // props of this type in rooms that are NOT subsystem targets (what a mix controls)
int  wd_count_rooms(const WdDeck *d, int kind);         // rooms of this kind that are NOT subsystem targets
const char *wd_room_kind(int k, int band);              // the room kind's name at this scale ("REACTOR SHAFT", "ATOM CORE" ...)
int  wd_walkable(const WdDeck *d, int x, int y, int have_key);
// the shortest walk (in cells) from (x0,y0) to any cell of room r; -1 if there is none. have_key: locked doors open; vents: 1 allowed, 0 not
int  wd_path_len(const WdDeck *d, int x0, int y0, int r, int have_key, int vents);
int  wd_next_step(const WdDeck *d, int x0, int y0, int tx, int ty, int have_key, int *nx, int *ny);   // the first step toward a cell
void wd_tick(WdDeck *d, double dt, float px, float py, int *hurt);   // the crew live: routines, chatting, sleeping; alarms; guards shoot (hurt += damage)
int  wd_room_at(const WdDeck *d, float x, float y);
const char *wd_act_name(int a);
#endif
