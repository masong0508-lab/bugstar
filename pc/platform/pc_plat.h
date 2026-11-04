// pc_plat.h: what the fake GBA (pc_hw.c) and the platform layer (pc/platform/*.c) say to each other.
#ifndef PC_PLAT_H
#define PC_PLAT_H
#include <stdint.h>
#include "pc_gbaaddr.h"
#define PC_AUDIO_RATE 48000
// --- sound chip (pc_psg.c)
void pc_snd(unsigned off, unsigned v);          // a write to the sound registers: off = offset from 0x04000060 (0x30..0x3F = wave RAM)
void pc_psg_reset(void);
void pc_psg_render(int16_t *out, int n);        // n stereo frames
// --- the window front end (pc_sdl.c); only built with SDL (PC_SDL). pc_hw.c calls these in window mode.
int  plat_open(int argc, char **argv);          // 0 = ok
void plat_frame(void);                          // called at every vblank (game frame): present, poll, pace; sets keys with pc_set_keys
void plat_close(void);
// --- pc_hw.c, for the platform layer
void pc_set_keys(uint16_t down);                // GBA key bits down now (bit0 A, 1 B, 2 select, 3 start, 4 right, 5 left, 6 up, 7 down, 8 R, 9 L)
void pc_compose_native(uint32_t *xrgb, int w, int h, int smooth);   // the picture made straight at w x h pixels (sprites, rotation, fades at that size)
void pc_compose_240(uint32_t *xrgb);                                 // the plain 240x160 picture
void pc_set_save_path(const char *path);
void pc_save_flush(int force);                  // write SRAM to the save file if it changed (or force)
void pc_quit_now(void);                         // save and exit
uint16_t pc_font_glyph(int ch);                 // the game's own 3x5 font (from pc_view.inc), 15 bits, MSB = top-left
#endif
