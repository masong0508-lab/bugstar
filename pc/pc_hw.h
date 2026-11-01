// pc_hw.h: the "fake GameBoy Advance" the PC build runs on. See pc/README.md.
#ifndef PC_HW_H
#define PC_HW_H
#include <stdint.h>
uint16_t pc_vcount(void);            // replaces reading REG_VCOUNT: every read moves the beam one line on (228 lines = one frame)
void pc_dma3(uint32_t cnt);          // replaces the write that starts DMA channel 3 (copies/fills now, like the real chip does)
extern void (*pc_irq_fn)(void);      // the game's interrupt handler (installed by sound_init)
static inline uint32_t pc_sp(void){ return 0x03007E00u; }   // the stack probes: pretend the stack is at the top of IWRAM
int  pc_hw_init(const char *sav_path);
void pc_hw_finish(void);             // writes the save file
void pc_script(const char *path, const char *outprefix, long max_frames);
long pc_frames(void);
void pc_render(uint8_t *rgb);                       // 240x160 RGB of what the screen shows now
int  pc_write_ppm(const char *path, int scale);     // the same, scaled up with sharp pixels
uint64_t pc_hash_screen(void);
uint64_t pc_hash_sram(void);
#endif
