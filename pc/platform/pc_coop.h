#ifndef BUGSTAR_PC_COOP_H
#define BUGSTAR_PC_COOP_H
/* One tiny host-controlled shared 3D raider, session only. No save access. */
#include <stdint.h>
struct PcNetShip;
typedef struct PcCoop {
    int32_t anchor_x,anchor_y,anchor_z;
    int32_t x,y,z;
    uint32_t down_tick,last_attack_tick;
    uint8_t hp,wins,losses,phase,flash,host_hits,guest_hits;
    uint8_t host_hp,guest_hp,beam,beam_target;
    uint8_t charge,charge_target; /* a short, visible lock-on before firing */
} PcCoop;
void pc_coop_reset(PcCoop *c);
void pc_coop_step(PcCoop *c,uint32_t frame,const struct PcNetShip *host,
                  const struct PcNetShip *guest);
int pc_coop_fire(PcCoop *c,unsigned shooter,const struct PcNetShip *attacker,
                 uint32_t frame);
#endif
