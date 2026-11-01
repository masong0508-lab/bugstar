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
#endif
