#ifndef BUGSTAR_PC_COOP_H
#define BUGSTAR_PC_COOP_H
/* Three-wave host-owned LAN mission, with a second drone in later waves.
 * All progress is session-only; no persistent campaign or save access. */
#include <stdint.h>
struct PcNetShip;
typedef struct PcCoop {
    int32_t anchor_x,anchor_y,anchor_z;
    int32_t x,y,z;
    int32_t wing_x,wing_y,wing_z; /* second, independently hittable 3D enemy */
    uint32_t down_tick,last_attack_tick;
    uint8_t hp,wins,losses,phase,flash,host_hits,guest_hits;
    uint8_t host_hp,guest_hp,beam,beam_target;
    uint8_t charge,charge_target; /* a short, visible lock-on before firing */
    uint8_t wing_hp,wave,completed; /* waves 0..2, then session-only clear */
} PcCoop;
void pc_coop_reset(PcCoop *c);
void pc_coop_step(PcCoop *c,uint32_t frame,const struct PcNetShip *host,
                  const struct PcNetShip *guest);
int pc_coop_fire(PcCoop *c,unsigned shooter,const struct PcNetShip *attacker,
                 uint32_t frame);
#endif
