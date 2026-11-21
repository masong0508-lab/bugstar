#ifndef BUGSTAR_GBA_SIO_H
#define BUGSTAR_GBA_SIO_H
/* Optional GBA 16-bit multiplayer cable backend. NOT auto-compiled into
 * the solo ROM. Integrate the serial IRQ hook and two linked emulator
 * instances before enabling an in-game multiplayer entry.
 */
#include "gba_link.h"
void gba_sio_begin(uint8_t role,uint8_t mode,uint32_t seed);
void gba_sio_enable_irq(void); /* only AFTER routing IRQ_SERIAL to gba_sio_irq */
void gba_sio_irq(void); /* minimal ISR callback: copy hardware word and return */
void gba_sio_tick(uint16_t tick); /* call once at fixed logical VBlank */
void gba_sio_ready(uint8_t ready);
const GbaLinkState *gba_sio_session(void);
void gba_sio_end(void);
#endif
