#ifndef BUGSTAR_PC_NET_H
#define BUGSTAR_PC_NET_H
/* Experimental LAN ship-presence transport. PC + Android only.
 * Protocol bytes come from net/session.h; no saves, combat or rewards.
 * Fixed memory, nonblocking sockets, one peer maximum.
 */
#include <stdint.h>
#include <stddef.h>
typedef struct PcNetShip {
    int32_t x,y,z;       /* shared space.inc world-space coordinates */
    uint32_t world;      /* same star-system ID required on both peers */
    uint16_t yaw;        /* 0..1023 */
    int16_t pitch;       /* camera inclination */
} PcNetShip;
typedef struct PcNetSession {
    uintptr_t sock;
    uint32_t peer_addr;  /* IPv4 in network byte order */
    uint32_t world;
    uint32_t last_rx, last_send, last_hello;
    uint16_t peer_port, local_port;
    uint8_t role, connected, received, seq;
    PcNetShip other;
} PcNetSession;
int pcnet_host(PcNetSession *s, uint16_t port);
int pcnet_join(PcNetSession *s, const char *numeric_ipv4, uint16_t port);
void pcnet_step(PcNetSession *s, uint32_t frame, const PcNetShip *local);
int pcnet_remote(const PcNetSession *s, uint32_t frame, PcNetShip *out);
void pcnet_close(PcNetSession *s);

/* One optional game instance per process. No-op unless host/join is enabled. */
int pc_mp_host(uint16_t port);
int pc_mp_join(const char *ip, uint16_t port);
void pc_mp_submit(const PcNetShip *local);
void pc_mp_frame(uint32_t frame);
int pc_mp_remote(uint32_t frame, PcNetShip *out);
void pc_mp_close(void);
#endif
