// shipr.h: the PC ship renderer. Draws ships, beams, explosions and big hulls at the window's own resolution, for any number of ships.
// It only draws: it never changes the game. The game (pc/pc_ships.inc) hands it the camera and the ships every frame; other PC code
// (for example the colossal warships) can hand it more ships, big meshes and whole armadas through the same calls. See pc/shipr/README.md.
#ifndef SHIPR_H
#define SHIPR_H
#include <stdint.h>
enum { SR_CLASSIC = 0, SR_NATIVE = 1, SR_ULTRA = 2 };
enum { SR_FIGHTER = 0, SR_WING = 1, SR_HEAVY = 2, SR_KINDS };
typedef struct {
    float p[3];            // where (world units, the game's space units)
    float f[3], u[3];      // forward and up (unit vectors)
    float size;            // half length in world units (the game's fighters are about 110)
    uint32_t col;          // hull colour 0xRRGGBB
    uint8_t kind, team;    // SR_FIGHTER..; team 0 friend, 1 foe
    uint8_t firing;        // 1 = its gun is hot this frame (muzzle flash)
    float hit;             // 0..1: shield shimmer (recently hit)
    float thrust;          // 0..1: engine glow
    uint8_t far;           // 1 = scenery far away: drawn only over empty sky (never over planets, the station or the HUD)
} SrShip;
typedef struct {           // a mesh for big hulls (warships): triangles in model units, x right, y up, z forward
    const float *v; int nv; const uint16_t *tri; int nt; const uint32_t *tcol;   // tcol: per-triangle colour (or NULL: one colour)
} SrMesh;
// one frame: the camera (eye position, right/up/forward unit vectors), the projection (focal length, centre, all in GAME pixels of a 240x160
// screen) and the view's bottom (game pixels): that is how the game itself projects (space.inc sproj), so ships land exactly where the game puts them.
void sr_begin(int mode, const float eye[3], const float r[3], const float u[3], const float f[3], float focal, float cx, float cy, float view_bottom, const float sun[3]);
void sr_ship(const SrShip *s);
void sr_ships(const SrShip *s, int n);                                     // many at once (an armada)
void sr_beam(const float a[3], const float b[3], uint32_t col, float width);
void sr_boom(const float p[3], float radius, float t, uint32_t seed);      // an explosion, t = 0 (start) .. 1 (gone): fireball, shock ring, debris
void sr_mesh(const SrMesh *m, const float p[3], const float f[3], const float u[3], float scale, uint32_t col);   // a big hull
void sr_armada(int n, uint32_t seed, long frame, float dist);             // a distant battle of n ships (decoration, from a seed and the frame number)
int  sr_count(void);
void sr_commit(int tag);                                                   // the frame is complete: it becomes the one sr_render draws (tag: which screen page it belongs to)
int  sr_shown_tag(void);
int  sr_select(int tag);                                                   // draw the frame committed with this tag (returns 0 if there is none)                                                   // the tag of the frame on show, -1 = none yet
void sr_on_frame(void (*fn)(void));                                        // a hook: called every space frame after the game's ships went in, to add more (warships, armadas)
void sr_run_hooks(void);
// draw it all into a W x H picture (0x00RRGGBB). mask: 240x160 bytes, 1 = the game's own picture may be covered here (NULL = everywhere).
// fade: the screen fade (-16 darker .. +16 brighter). Returns the number of ships drawn.
int  sr_render(uint32_t *out, int W, int H, const uint8_t *mask, int fade);
double sr_last_ms(void);                                                   // how long the last sr_render took
#endif
