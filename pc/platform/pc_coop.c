/* BUGSTAR LAN co-op vertical slice: both pilots attack one shared raider.
 * Only the host calls step/fire. Its 3D pose, HP and victories replicate
 * to the guest; none of these values can alter campaign HP, loot or saves.
 * Fixed memory and integer-only movement (except existing aim geometry).
 */
#include "pc_net.h"
#include <string.h>
void pc_coop_reset(PcCoop *c){if(c)memset(c,0,sizeof *c);}
void pc_coop_step(PcCoop *c,uint32_t frame,const PcNetShip *host,
                  const PcNetShip *guest){
    if(!c || !host)return;
    if(!c->phase){
        c->anchor_x=host->x;c->anchor_y=host->y;
        c->anchor_z=host->z;
        c->hp=100;c->host_hp=100;c->guest_hp=100;
        c->phase=1;c->down_tick=0;c->last_attack_tick=frame;
    }
    if(c->phase==2 || c->phase==3){
        if(frame-c->down_tick>=150u){
            c->hp=100;c->host_hp=100;c->guest_hp=100;
            c->phase=1;c->flash=0;c->beam=0;
            c->last_attack_tick=frame;
        }else return;
    }
    /* A readable patrol, not an invisible static crosshair target. */
    uint32_t w=frame%240u;
    int32_t sweep=(int32_t)(w<120u?w:240u-w)*4-240;
    uint32_t v=frame%180u;
    int32_t bob=(int32_t)(v<90u?v:180u-v)-45;
    c->x=c->anchor_x+sweep;
    c->y=c->anchor_y+120+bob*2;
    c->z=c->anchor_z+3500;
    if(c->flash)c->flash--;
    if(c->beam)c->beam--;
    /* The raider fires a readable straight-ahead shot about every half
     * second. Both players share the host's decision and temporary shields.
     * Sidestepping the lane avoids damage; there is no save-file access. */
    if(frame-c->last_attack_tick>=30u){
        c->last_attack_tick=frame;
        unsigned target=(frame/30u)&1u;
        const PcNetShip *p=target?guest:host;
        uint8_t *hp=target?&c->guest_hp:&c->host_hp;
        if(p && *hp && p->world==host->world){
            int64_t dx=(int64_t)p->x-c->x;
            int64_t dy=(int64_t)p->y-c->y;
            int64_t dz=(int64_t)c->z-p->z;
            int64_t r=300+dz/20;
            if(dz>250 && dz<9000 && dx*dx+dy*dy<=r*r){
                *hp=*hp<=10?0:(uint8_t)(*hp-10);
                c->beam=8;c->beam_target=(uint8_t)target;
            }
        }
        if(!c->host_hp && !c->guest_hp){
            c->phase=3;c->down_tick=frame;c->beam=0;
            if(c->losses<255)c->losses++;
        }
    }
}
int pc_coop_fire(PcCoop *c,unsigned shooter,const PcNetShip *a,
                 uint32_t frame){
    if(!c || !a || shooter>1 || c->phase!=1 || !c->hp ||
       !(shooter?c->guest_hp:c->host_hp))return 0;
    PcNetShip enemy={c->x,c->y,c->z,a->world,0,0};
    if(!pc_duel_aim(a,&enemy))return 0;
    uint8_t *hits=shooter?&c->guest_hits:&c->host_hits;
    if(*hits<255)(*hits)++;
    c->flash=10;
    if(c->hp<=20){
        c->hp=0;c->phase=2;c->down_tick=frame?frame:1;
        if(c->wins<255)c->wins++;
    }else c->hp-=20;
    return 1;
}
