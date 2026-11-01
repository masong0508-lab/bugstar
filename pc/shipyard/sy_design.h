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
#define SY_VERSION 1                 // (the foes bit was added before any file was shared: still version 1)

enum { SYSH_ROUND, SYSH_BOX, SYSH_WEDGE, SYSH_FLAT, SY_NSHAPE };                         // section shapes
enum { SYNO_POINT, SYNO_BLUNT, SYNO_FORK, SYNO_RAM, SY_NNOSE };
enum { SYWK_NONE, SYWK_STRAIGHT, SYWK_SWEPT, SYWK_DELTA, SYWK_X, SYWK_FORWARD, SYWK_GULL, SY_NWING };
enum { SYEK_ION, SYEK_PLASMA, SYEK_FUSION, SYEK_WARP, SY_NENG };
enum { SYGK_NONE, SYGK_LASER, SYGK_CANNON, SYGK_MISSILE, SYGK_BEAM, SYGK_FLAK, SY_NGUN };
enum { SYGP_TOP, SYGP_LEFT, SYGP_RIGHT, SYGP_BELOW, SYGP_NOSE, SY_NGPOS };                 // where a mount sits (LEFT/RIGHT: both sides when symmetry is on)
enum { SYCP_BUBBLE, SYCP_SLIT, SYCP_DOME, SYCP_VISOR, SYCP_BRIDGE, SY_NCOCK };
enum { SYPT_PLAIN, SYPT_STRIPE, SYPT_TWOTONE, SYPT_CHECKS, SYPT_TIGER, SYPT_SPLIT, SYPT_EDGE, SYPT_DOTS, SY_NPAT };
enum { SYFI_GLOSS, SYFI_MATTE, SYFI_METAL, SYFI_PEARL, SY_NFIN };

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
    uint8_t foes;                                                                // 1 = allied and enemy ships in space are built from the same parts (see sy_foe)
    uint8_t ingame;                                                              // 1 = the game uses this design for your ship
} SyDesign;

// ---- stats
enum { SYST_SPEED, SYST_TURN, SYST_ARMOUR, SYST_SHIELD, SYST_FIRE, SYST_RANGE, SYST_HOLD, SYST_FUEL, SYST_CREW, SYST_COST, SYST_THREAT, SY_NSTAT };
enum { SYWN_POWER = 1, SYWN_HEAT = 2, SYWN_HEAVY = 4, SYWN_CREW = 8, SYWN_NOGUN = 16, SYWN_FUEL = 32, SYWN_SLOW = 64 };
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
void sy_foe(SyDesign *d, uint32_t seed, int team, int level);                  // an allied (team 0) or enemy (1) design from a seed: the same part grammar, balanced; level 0..9 = stronger
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
enum { SYT_SHIP, SYT_HULL, SYT_WINGS, SYT_POWER, SYT_GUNS, SYT_GUARD, SYT_ROOMS, SYT_PAINT, SYT_BADGE, SYT_FILE, SY_NTAB };
extern const char *const sy_tab_name[SY_NTAB];
enum {
    SYR_PRESET, SYR_DICE, SYR_MODE, SYR_SNAP, SYR_INGAME, SYR_FOES, SYR_UNDO, SYR_REDO,
    SYR_SEC, SYR_LEN, SYR_WID, SYR_HGT, SYR_SHAPE, SYR_NOSE, SYR_SYM, SYR_ADDSEC, SYR_COPYSEC, SYR_DELSEC, SYR_STRETCH,
    SYR_WING, SYR_WSPAN, SYR_WSWEEP, SYR_WSEC, SYR_WTILT, SYR_FINS,
    SYR_ENG, SYR_NENG, SYR_ESIZE, SYR_REACTOR, SYR_TANKS, SYR_RADS,
    SYR_GUN, SYR_GKIND, SYR_GSIZE, SYR_GSEC, SYR_GPOS, SYR_ADDGUN, SYR_COPYGUN, SYR_DELGUN,
    SYR_SHIELD, SYR_ARMALL, SYR_ARM0, SYR_ARM_END = SYR_ARM0 + SY_MAXSEC - 1,
    SYR_COCK, SYR_BAYS, SYR_CARGO, SYR_QUART,
    SYR_PAINTC, SYR_TRIMC, SYR_GLOWC, SYR_LIGHTC, SYR_PATTERN, SYR_FINISH,
    SYR_NAME, SYR_EMBLEM, SYR_EMCOL, SYR_DECAL,
    SYR_SAVE, SYR_LOAD, SYR_CODE, SYR_IMPORT, SYR_CLASSIC, SYR_DONE,
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

// ---- the INTERIOR hook (boarding): what rooms a design has, in the warship interior's room kinds (pc/warship/ws_deck.h WR_*, WP_* props).
// pc/warship's wd_make can take this list instead of its own mix: one entry per room kind, how many rooms, and how many props of the
// kinds that belong to the parts (guns in turrets, crates in cargo, bunks in crew rooms). Not wired into wd_make yet: that file is the
// warship worker's. A design's own ship (no craft seed) is one deck.
typedef struct { uint8_t reactor, hangar, quarters, bridge, turret, shieldgen, workshop, mess; uint8_t bunks, crates, guns, fighters; } SyRooms;
void sy_rooms(const SyDesign *d, SyRooms *r);

// ---- the 3D model for the preview (and for any renderer that wants it): points and faces in model units, nose at -z
#define SY_MAXV 520
#define SY_MAXF 460
typedef struct { int16_t x, y, z; } SyV;
typedef struct { uint16_t v[4]; uint8_t n, col, two; int8_t sec; } SyF;          // col: 0 paint, 1 trim, 2 metal, 3 glow, 4 glass, 5 gun, 6 emblem
typedef struct { SyV v[SY_MAXV]; SyF f[SY_MAXF]; int nv, nf; int len, rad; } SyMesh;
void sy_mesh(const SyDesign *d, SyMesh *m);
#endif
