// ws_view.h: the first-person view inside a warship, drawn straight at any size (the PC window's own resolution, or 240 x 160).
// A grid renderer with real heights per cell: tall halls next to low corridors, door lintels, deep reactor shafts with catwalks,
// windows to space; smooth per-cell light, fog; props and crew are small solid models (boxes, cylinders, spheres) ray-cast per pixel.
#ifndef WS_VIEW_H
#define WS_VIEW_H
#include "ws_deck.h"
typedef struct { float x, y, z; int on; } WvMark;               // the objective marker (cell coordinates, z in half metres)
// out: W x H pixels 0x00RRGGBB. hurt: 0..1 red flash. charges: planted charges (prop list is the deck's; these are extra blinking ones)
void wv_render(uint32_t *out, int W, int H, const WdDeck *d, float px, float py, float ang, double t, const WvMark *mark, float hurt);
#endif
