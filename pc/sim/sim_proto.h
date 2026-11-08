// sim_proto.h: prototypes for the hooks build.sh puts into the generated game copy (pc/gen/main.c). Only plain C types here: it is included
// before the game defines its own. The bodies are in sim_glue.inc (appended to the same file).
#ifndef SIM_PROTO_H
#define SIM_PROTO_H
static void sim_frame(int n);                     // once a frame on a planet (n = vblanks since the last frame)
static int  sim_quick(void);                      // A on empty ground: the quick menu. 1 = it was shown
static int  sim_talk(void *po_obj);               // A on a local: shows who it is. Always 0: the old talk goes on after it
static void sim_hud(volatile unsigned short *pg); // the clock chip
static void sim_save(void);                       // with every save
static void sim_load(void);                       // after a save was loaded
static void sim_reset(void);                      // a new game
static int  sim_big_xp(void);                     // 1 when the big level (Lv) is in charge of experience
static void sim_lv_fix(char *lv);                 // the top bar: swap the number for the compact big level
static void sim_ncat_level(char *d);              // appends the level (compact when it is beyond 64 bits)
static long long sim_xpv(long long v), sim_xpm(long long v);   // xp bar values: unchanged, or per-mille when the big level is on
static void sim_lab(volatile unsigned short *pg);   // PC LAB > SIDE GAME: the side-game menu from anywhere
static int  sim_open_gods(void);                 // how many god powers are open at this level (0 with the side game off)
static void pc_land_note(void);                   // the MEANS line after landing
static void sim_ship_colony(volatile unsigned short *pg);   // the ship menu's COLONY row
static int  sim_is_on(void);
static void sim_ship_tabs(void);                 // patch 47: the GO tab lists COLONY only once the colony is found
static void sim_slot_erase(int s), sim_slot_copy(int from, int to);   // patch 47: a save slot erased or copied takes its side game along
static int  sim_invasion(unsigned size, unsigned long long *reward);   // a warship DEFEND mission raids this planet's colony for real: 1 won, -1 lost, 0 no colony
static int  sim_planet_burnt(void);              // 1 if this planet was destroyed (kept in the colony records for good)
static int  sim_dn_call(void);                   // patch 15: 1 if the colony has a call for this dungeon floor
static void sim_dn_reset(void), sim_dn_pick(void), sim_dn_mark(void), sim_dn_leave(void);   // new floor / the floor event is chosen / after the floor is built / taking the stairs
static void sim_dn_done(int fe);                 // a floor goal was met (FE_ELITE, FE_QUIET) or is checked on leaving (FE_SPEED, FE_NEST)
static void sim_dn_draw(volatile unsigned short *pg);   // the COLONY CALLS line under the goal line
#endif
