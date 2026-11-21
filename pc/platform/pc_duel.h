#ifndef BUGSTAR_PC_DUEL_H
#define BUGSTAR_PC_DUEL_H
/* LAN-only friendly fire sparring, isolated from the real hull/save state. */
#include <stdint.h>
struct PcNetShip; /* forward declaration avoids a circular public header */
typedef struct PcDuel {
    uint8_t host_hp, guest_hp, host_wins, guest_wins;
    uint32_t knocked_at;
} PcDuel;
void pc_duel_reset(PcDuel *d);
void pc_duel_step(PcDuel *d, uint32_t tick);
int pc_duel_aim(const struct PcNetShip *attacker, const struct PcNetShip *target);
int pc_duel_fire(PcDuel *d, uint8_t shooter, const struct PcNetShip *attacker,
                 const struct PcNetShip *target, uint32_t tick);
#endif
