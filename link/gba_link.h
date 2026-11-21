#ifndef BUGSTAR_GBA_LINK_H
#define BUGSTAR_GBA_LINK_H
/* GBA 16-bit multiplayer cable semantic protocol. Not GBC serial or PC UDP.
 * Six 16-bit words, little-endian CRC16; no framebuffer or SRAM access.
 * Preflight only until actual GBA cable transport and mission pass tests.
 */
#include <stdint.h>
#define GBAL_VERSION 1u
#define GBAL_MAGIC 0xB701u
#define GBAL_WORDS 6u
#define GBAL_TIMEOUT 120u
#define GBAL_MODE_DUEL 1u
#define GBAL_MODE_COOP 2u
#define GBAL_FLAG_MODE 3u
#define GBAL_FLAG_HELLO 4u
#define GBAL_FLAG_BYE 8u
#define GBAL_FLAG_ACTIVE 64u
#define GBAL_FLAG_READY 128u
typedef struct GbaLinkPacket {
    uint32_t seed;
    uint16_t tick;
    uint8_t seq,flags;
} GbaLinkPacket;
typedef struct GbaLinkState {
    uint32_t seed;
    uint16_t last_rx;
    uint8_t role,mode,tx_seq,peer_seq,seen_seq,connected;
    uint8_t local_ready,peer_ready,local_active,peer_active,reconnecting;
} GbaLinkState;
int gbal_pack(uint16_t out[GBAL_WORDS],const GbaLinkPacket *p);
int gbal_unpack(GbaLinkPacket *p,const uint16_t in[GBAL_WORDS]);
void gbal_init(GbaLinkState *s,uint8_t role,uint8_t mode,uint32_t seed);
void gbal_set_ready(GbaLinkState *s,uint8_t ready);
void gbal_set_active(GbaLinkState *s,uint8_t active);
void gbal_timeout(GbaLinkState *s,uint16_t tick);
void gbal_make(GbaLinkState *s,GbaLinkPacket *p,uint16_t tick);
int gbal_receive(GbaLinkState *s,const GbaLinkPacket *p,uint16_t now);
uint8_t gbal_confirmed(const GbaLinkState *s);
uint8_t gbal_can_play(const GbaLinkState *s);
#endif
