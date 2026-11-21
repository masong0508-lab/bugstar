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
#include "pc_duel.h"
#include "pc_coop.h"
typedef struct PcNetSession {
    uintptr_t sock;
    uint32_t peer_addr;  /* IPv4 in network byte order */
    uint32_t world;
    uint32_t last_rx, last_send, last_hello;
    uint16_t peer_port, local_port;
    uint8_t role, connected, received, seq;
    PcNetShip other;
    PcDuel duel;                 /* session-only sparring HP and score */
    PcCoop coop_enemy;           /* host-authored shared star-system raider */
    uint32_t last_host_shot_tick, last_guest_shot_tick;
    uint16_t last_state_tick, last_event_tick, last_raider_tick; /* reordering */
    uint8_t local_shots, host_seen_shots, guest_seen_shots;
    uint8_t have_guest_fire, event_seen, state_seen;
    uint8_t coop, raider_seen;  /* 1: co-op against one shared raider, 0: sparring */
} PcNetSession;
int pcnet_host(PcNetSession *s, uint16_t port);
int pcnet_join(PcNetSession *s, const char *numeric_ipv4, uint16_t port);
void pcnet_step(PcNetSession *s, uint32_t frame, const PcNetShip *local);
int pcnet_remote(const PcNetSession *s, uint32_t frame, PcNetShip *out);
void pcnet_close(PcNetSession *s);
void pcnet_fire(PcNetSession *s);
void pcnet_set_coop(PcNetSession *s,int enabled);
int pcnet_raider(const PcNetSession *s,uint32_t frame,PcCoop *out);
int pcnet_duel(const PcNetSession *s, uint8_t *host_hp, uint8_t *guest_hp,
               uint8_t *host_wins, uint8_t *guest_wins);

/* One optional game instance per process. No-op unless host/join is enabled. */
int pc_mp_host(uint16_t port);
int pc_mp_join(const char *ip, uint16_t port);
void pc_mp_submit(const PcNetShip *local);
void pc_mp_frame(uint32_t frame);
int pc_mp_remote(uint32_t frame, PcNetShip *out);
void pc_mp_close(void);
void pc_mp_set_coop(int enabled);
int pc_mp_coop_mode(void);
int pc_mp_raider(uint32_t frame,PcCoop *out);
void pc_mp_fired(void);
int pc_mp_role(void); /* 1 host, 2 guest, 0 disabled */
int pc_mp_duel(uint8_t *host_hp, uint8_t *guest_hp, uint8_t *host_wins, uint8_t *guest_wins);
#endif
