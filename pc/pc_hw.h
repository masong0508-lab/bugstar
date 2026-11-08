// pc_hw.h: the "fake GameBoy Advance" the PC build runs on. See pc/README.md.
#ifndef PC_HW_H
#define PC_HW_H
#include <stdint.h>
#include "platform/pc_gbaaddr.h"
uint32_t pc_tmr(void);                           // the fake TM0+TM1 cascade: CPU cycles (16.78 MHz) since start, 280896 a frame
void pc_tick_poll(void), pc_tick_step(void);   // frame-log counters (patch 43): key polls and dungeon logic steps
uint16_t pc_vcount(void);            // replaces reading REG_VCOUNT: every read moves the beam one line on (228 lines = one frame)
void pc_dma_sad(uintptr_t a), pc_dma_dad(uintptr_t a);   // the DMA-3 source / destination, full width (patch 25)
void pc_dma3(uint32_t cnt);          // replaces the write that starts DMA channel 3 (copies/fills now, like the real chip does)
extern void (*pc_irq_fn)(void);      // the game's interrupt handler (installed by sound_init)
static inline uint32_t pc_sp(void){ return GBAA(0x03007E00u); }   // the stack probes: pretend the stack is at the top of IWRAM
int  pc_hw_init(const char *sav_path);
void pc_hw_finish(void);             // writes the save file
void pc_script(const char *path, const char *outprefix, long max_frames);
long pc_frames(void);
void pc_render(uint8_t *rgb);                       // 240x160 RGB of what the screen shows now
int  pc_write_ppm(const char *path, int scale);     // the same, scaled up with sharp pixels
uint64_t pc_hash_screen(void);
uint64_t pc_hash_sram(void);
extern int pc_vid3d;                 // 3D views: 0 AUTO, 1 NATIVE (smooth, made at window size), 2 CLASSIC (the GBA look)
extern int pc_time_pct;              // game speed in percent (100 = normal)
const char *pc_sav_path(void);       // the save file path, or NULL
extern int pc_mouse_x, pc_mouse_y, pc_mouse_b, pc_mouse_wheel;   // the mouse in the 240x160 picture (x = -1 when it is outside), buttons bit0 left bit1 right, wheel steps (the creator clears it)
extern void (*pc_ovl_fn)(uint32_t *out, int w, int h);   // set: a PC-made 3D view shows wherever the game's picture is empty (backdrop)

// kart race NATIVE extras (pc_kartnat_r.inc / pc_kartnat.inc): banking, hills, soft shadows. The game hands over its state each frame; the window picture uses it.
int pc_kn_on(void);
void pc_kn_frame(const short *rowz, int active, int pi256, int heading, int curvahead, int elev, unsigned seed, int knp, int nsh, const short *shx, const short *shy, const short *shw, int pcx, int pcy, int pcw);
#endif
