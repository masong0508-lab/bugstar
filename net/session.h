#ifndef BUGSTAR_NET_SESSION_H
#define BUGSTAR_NET_SESSION_H
/* Multiplayer protocol v1: transport-independent, allocation-free packets.
 * Used by opt-in PC/Android LAN space combat, not yet by handheld cables.
 * Fixed byte order and explicit lengths: never send native C structs.
 * CRC8 detects accidental corruption; it is NOT a security/authentication MAC.
 */
#include <stdint.h>

#define BSNET_MAGIC 0xB7u
#define BSNET_VERSION 1u
#define BSNET_MAX_PAYLOAD 24u
#define BSNET_HEADER_BYTES 7u
#define BSNET_MAX_FRAME (BSNET_HEADER_BYTES + BSNET_MAX_PAYLOAD + 1u)
enum BsNetType {
    BSNET_HELLO = 1, BSNET_READY = 2, BSNET_INPUT = 3,
    BSNET_STATE = 4, BSNET_EVENT = 5, BSNET_ACK = 6,
    BSNET_PING = 7, BSNET_BYE = 8, BSNET_RAIDER = 9
};
enum BsNetError {
    BSNET_E_ARGS = -1, BSNET_E_LENGTH = -2, BSNET_E_HEADER = -3,
    BSNET_E_VERSION = -4, BSNET_E_TYPE = -5, BSNET_E_CRC = -6
};
typedef struct BsNetPacket {
    uint8_t type;       /* BsNetType */
    uint8_t sequence;   /* wraps at 255, match ACK window */
    uint16_t tick;      /* wraps at 65535; callers compare modularly */
    uint8_t length;     /* fixed for every v1 packet type */
    uint8_t payload[BSNET_MAX_PAYLOAD];
} BsNetPacket;

/* Success: pack returns wire byte count, unpack returns 0.
 * Failure: negative BsNetError; unpack never partially modifies *out.
 * Exact v1 payload lengths: HELLO=5 READY=1 INPUT=3 STATE=8
 * EVENT=4 ACK=0 PING=2 BYE=0 RAIDER=16 (opt-in LAN co-op). Higher-level handlers validate values,
 * session identifiers, ownership and replay windows.
 */
int bsnet_pack(const BsNetPacket *pkt, uint8_t *out, uint8_t capacity);
int bsnet_unpack(BsNetPacket *out, const uint8_t *data, uint8_t count);
#endif
