// sy_design.h: the PC SHIP EDITOR's design (plain C, no game code): parts, stats, warnings, the small byte recipe,
// the share code, undo/redo, presets and the dice. The screen is shipyard_ui.inc; the game hooks are in pc/build.sh.
// Everything here is integer maths with clamps, so any design (even random bytes) gives sane numbers.
#ifndef SY_DESIGN_H
#define SY_DESIGN_H
#include <stdint.h>

#define SY_NAME 12
#define SY_MAXSEC 8          // hull sections, nose to tail
#define SY_MAXGUN 6          // weapon / turret mounts
#define SY_UNIT_MAX 15       // a section's length, width and height are 1..15 grid units
#define SY_VERSION 1

enum { SH_ROUND, SH_BOX, SH_WEDGE, SH_FLAT, SY_NSHAPE };                         // section shapes
enum { NO_POINT, NO_BLUNT, NO_FORK, NO_RAM, SY_NNOSE };
enum { WK_NONE, WK_STRAIGHT, WK_SWEPT, WK_DELTA, WK_X, WK_FORWARD, WK_GULL, SY_NWING };
enum { EK_ION, EK_PLASMA, EK_FUSION, EK_WARP, SY_NENG };
enum { GK_NONE, GK_LASER, GK_CANNON, GK_MISSILE, GK_BEAM, GK_FLAK, SY_NGUN };
enum { GP_TOP, GP_LEFT, GP_RIGHT, GP_BELOW, GP_NOSE, SY_NGPOS };                 // where a mount sits (LEFT/RIGHT: both sides when symmetry is on)
enum { CP_BUBBLE, CP_SLIT, CP_DOME, CP_VISOR, CP_BRIDGE, SY_NCOCK };
enum { PT_PLAIN, PT_STRIPE, PT_TWOTONE, PT_CHECKS, PT_TIGER, PT_SPLIT, PT_EDGE, PT_DOTS, SY_NPAT };
enum { FI_GLOSS, FI_MATTE, FI_METAL, FI_PEARL, SY_NFIN };

typedef struct { uint8_t len, wid, hgt, shape, armour; } SySec;                  // armour: plates 0..5 on this section
typedef struct { uint8_t kind, size, sec, pos; } SyGun;                          // size 0..2, sec = which section it sits on
typedef struct {
    char name[SY_NAME + 1];
    uint8_t nsec; SySec sec[SY_MAXSEC];
    uint8_t sym;                                                                 // symmetry: 1 = mirrored left/right
    uint8_t nose;
    uint8_t wing, wspan, wsweep, wsec, wtilt, fins;                              // wings: kind, span 0..15, sweep 0..15, section, tilt 0..2, fins 0..2
    uint8_t eng, neng, esize, reactor, tanks, radiators;                         // engines kind, count 1..6, size 0..3; reactor 0..5; fuel tanks 0..6; radiators 0..5
    uint8_t ngun; SyGun gun[SY_MAXGUN];
    uint8_t shield, cockpit, bays, cargo, quarters;                              // shield 0..5, hangar bays 0..4, cargo pods 0..8, crew quarters 0..6
    uint16_t paint, trim, glow, light;                                           // colours, 15-bit (GBA style: r | g<<5 | b<<10)
    uint8_t pattern, finish;
    uint8_t decal[8], emblem[8];                                                 // 8x8 one-bit pictures drawn in the editor
    uint16_t emcol;                                                              // the emblem's colour
    uint8_t ingame;                                                              // 1 = the game uses this design for your ship
} SyDesign;

// ---- stats
enum { ST_SPEED, ST_TURN, ST_ARMOUR, ST_SHIELD, ST_FIRE, ST_RANGE, ST_HOLD, ST_FUEL, ST_CREW, ST_COST, ST_THREAT, SY_NSTAT };
enum { WN_POWER = 1, WN_HEAT = 2, WN_HEAVY = 4, WN_CREW = 8, WN_NOGUN = 16, WN_FUEL = 32, WN_SLOW = 64 };
typedef struct {
    int32_t v[SY_NSTAT];                                                         // each 0..9999
    int32_t mass, power_gen, power_use, heat, cooling, thrust, crew_need, beds;
    int warn;                                                                    // WN_ bits
} SyStats;
extern const char *const sy_stat_name[SY_NSTAT];
extern const int sy_stat_max[SY_NSTAT];                                          // for the bars
void sy_stats(const SyDesign *d, SyStats *s);
int  sy_warn_text(int warn, int i, char *out, int n);                            // the i-th warning in plain words; 0 when there are no more
// the numbers the game itself uses (pc/build.sh hooks): top speed 28..90, turn % 40..170, damage taken % 40..130, extra laser 0..24,
// extra hold 0..60, extra fuel 0..200, push 4..96 per frame in sixteenths
typedef struct { int speed, turn, armour, gun, hold, fuel, accel, damp; } SyGame;
void sy_game(const SyDesign *d, SyGame *g);

// ---- one part's cost line (the help panel shows it): mass, power (minus = makes power), heat, cost, crew
typedef struct { int mass, power, heat, cost, crew; } SyPart;

// ---- making and fixing
void sy_default(SyDesign *d);
void sy_fix(SyDesign *d);                                                        // clamp every field into range (any bytes become a valid design)
#define SY_NPRESET 10
extern const char *const sy_preset_name[SY_NPRESET];
void sy_preset(SyDesign *d, int i);                                              // keeps the name
void sy_random(SyDesign *d, uint64_t *rng);
void sy_balance(SyDesign *d);                                                    // add reactor, radiators, beds, tanks until the warnings go (where it can)                                      // SURPRISE ME: a sensible random ship (keeps the name)
uint64_t sy_rng(uint64_t *s);

// ---- the recipe: a few dozen bytes. Round trip: decode(encode(d)) == fixed d
#define SY_RECIPE_MAX 160
int  sy_encode(const SyDesign *d, uint8_t *out, int max);                        // bytes written, 0 on failure
int  sy_decode(const uint8_t *in, int n, SyDesign *d);                           // 1 ok (checksum and version), 0 bad
int  sy_code(const SyDesign *d, char *out, int n);                               // share code: "SHIP-" then letters and digits (no 0 O 1 I)
int  sy_uncode(const char *code, SyDesign *d);
int  sy_same(const SyDesign *a, const SyDesign *b);                              // same after fixing (compares the recipes)
uint32_t sy_hash(const SyDesign *d);

// ---- undo / redo
#define SY_HIST 48
typedef struct { SyDesign h[SY_HIST]; int n, at; } SyHist;
void sy_hist_init(SyHist *h, const SyDesign *d);
void sy_hist_push(SyHist *h, const SyDesign *d);                                 // after a change (does nothing when nothing changed)
int  sy_hist_undo(SyHist *h, SyDesign *d);
int  sy_hist_redo(SyHist *h, SyDesign *d);

// ---- the editor's rows: every edit goes through sy_change, so the fuzz test and the screen do exactly the same thing
enum { TB_SHIP, TB_HULL, TB_WINGS, TB_POWER, TB_GUNS, TB_GUARD, TB_ROOMS, TB_PAINT, TB_BADGE, TB_FILE, SY_NTAB };
extern const char *const sy_tab_name[SY_NTAB];
enum {
    R_PRESET, R_DICE, R_MODE, R_SNAP, R_INGAME, R_UNDO, R_REDO,
    R_SEC, R_LEN, R_WID, R_HGT, R_SHAPE, R_NOSE, R_SYM, R_ADDSEC, R_COPYSEC, R_DELSEC, R_STRETCH,
    R_WING, R_WSPAN, R_WSWEEP, R_WSEC, R_WTILT, R_FINS,
    R_ENG, R_NENG, R_ESIZE, R_REACTOR, R_TANKS, R_RADS,
    R_GUN, R_GKIND, R_GSIZE, R_GSEC, R_GPOS, R_ADDGUN, R_COPYGUN, R_DELGUN,
    R_SHIELD, R_ARMALL, R_ARM0, R_ARM_END = R_ARM0 + SY_MAXSEC - 1,
    R_COCK, R_BAYS, R_CARGO, R_QUART,
    R_PAINTC, R_TRIMC, R_GLOWC, R_LIGHTC, R_PATTERN, R_FINISH,
    R_NAME, R_EMBLEM, R_EMCOL, R_DECAL,
    R_SAVE, R_LOAD, R_CODE, R_IMPORT, R_CLASSIC, R_DONE,
    SY_NROW
};
typedef struct { int sec, gun, advanced, snap; } SyCursor;                       // editor state the rows depend on
int  sy_rows(int tab, const SyDesign *d, const SyCursor *c, int *ids);           // the rows of a tab (simple mode shows fewer)
const char *sy_row_label(int id);
const char *sy_row_help(int id);                                                 // one short line, plain words
void sy_row_value(int id, const SyDesign *d, const SyCursor *c, char *out, int n);
int  sy_row_part(int id, const SyDesign *d, const SyCursor *c, SyPart *p);       // the part this row edits, with its costs (0 = none)
int  sy_row_is_value(int id);                                                    // LEFT RIGHT change it (else A does something)
int  sy_change(SyDesign *d, SyCursor *c, int id, int delta);                     // LEFT RIGHT (delta -1/1, or +-4 for a big step); 1 = changed
int  sy_action(SyDesign *d, SyCursor *c, int id);
void sy_cursor_fix(const SyDesign *d, SyCursor *c);                              // after undo, presets or loads                                // A on add/copy/remove rows; 1 = changed
extern const uint16_t sy_swatch[32];                                             // the colour steps for LEFT RIGHT on colour rows

// ---- the 3D model for the preview (and for any renderer that wants it): points and faces in model units, nose at -z
#define SY_MAXV 520
#define SY_MAXF 460
typedef struct { int16_t x, y, z; } SyV;
typedef struct { uint16_t v[4]; uint8_t n, col, two; int8_t sec; } SyF;          // col: 0 paint, 1 trim, 2 metal, 3 glow, 4 glass, 5 gun, 6 emblem
typedef struct { SyV v[SY_MAXV]; SyF f[SY_MAXF]; int nv, nf; int len, rad; } SyMesh;
void sy_mesh(const SyDesign *d, SyMesh *m);
#endif
