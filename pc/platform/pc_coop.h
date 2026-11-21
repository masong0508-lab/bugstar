#ifndef BUGSTAR_PC_COOP_H
#define BUGSTAR_PC_COOP_H
/* One tiny host-controlled shared 3D raider, session only. No save access. */
#include <stdint.h>
struct PcNetShip;
typedef struct PcCoop {
    int32_t anchor_x,anchor_y,anchor_z;
    int32_t x,y,z;
    uint32_t down_tick;
    uint8_t hp,wins,phase,flash,host_hits,guest_hits;
} PcCoop;
void pc_coop_reset(PcCoop *c);
void pc_coop_step(PcCoop *c,uint32_t frame,const struct PcNetShip *host);
int pc_coop_fire(PcCoop *c,unsigned shooter,const struct PcNetShip *attacker,
                 uint32_t frame);
#endif
