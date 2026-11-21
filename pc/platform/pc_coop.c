/* BUGSTAR PC/Android LAN co-op contract: three host-owned mission waves.
 * Wave 1: a moving raider. Waves 2/3: raider plus an independent flanking
 * drone. Defeat all active enemies before the team is knocked out.
 * None of the encounter state can change campaign HP, XP, loot or saves.
 * No heap allocations; integer-only simulation, existing 3D aim helper.
 */
#include "pc_net.h"
#include <string.h>

void pc_coop_reset(PcCoop *c){if(c)memset(c,0,sizeof *c);}

static void coop_next(PcCoop *c,uint32_t frame){
    c->phase=1;c->hp=100;
    c->wing_hp=c->wave ? (uint8_t)(c->wave==1?60:80) : 0;
    c->host_hp=100;c->guest_hp=100;
    c->flash=0;c->beam=0;c->charge=0;
    c->charge_source=0;c->beam_source=0;
    c->last_attack_tick=frame;
}
void pc_coop_step(PcCoop *c,uint32_t frame,const PcNetShip *host,
                  const PcNetShip *guest){
    if(!c || !host)return;
    if(!c->phase){
        c->anchor_x=host->x;c->anchor_y=host->y;
        c->anchor_z=host->z;
        c->wave=0;c->down_tick=0;
        coop_next(c,frame);
    }
    if(c->phase==2 || c->phase==3 || c->phase==4){
        uint32_t hold=c->phase==4?300u:150u;
        if(frame-c->down_tick<hold)return;
        /* Every cleared wave unlocks a harder objective, but defeat
         * restarts the contract. Three victories complete the mission. */
        if(c->phase==2 && c->wave<2)c->wave++;
        else c->wave=0;
        coop_next(c,frame);
    }
    /* Same world-space reference for all peers; each enemy has its own
     * deterministic patrol and independently destructible hit points. */
    uint32_t w=frame%240u;
    int32_t sweep=(int32_t)(w<120u?w:240u-w)*4-240;
    uint32_t v=frame%180u;
    int32_t bob=(int32_t)(v<90u?v:180u-v)-45;
    c->x=c->anchor_x+sweep;
    c->y=c->anchor_y+120+bob*2;
    c->z=c->anchor_z+3500;
    c->wing_x=c->anchor_x+900-sweep/2;
    c->wing_y=c->anchor_y+40-bob;
    c->wing_z=c->anchor_z+3350;
    if(c->flash)c->flash--;
    if(c->beam)c->beam--;
    /* Telegraph a single, host-authoritative shot at a time. The flank
     * drone alternates with the raider from wave 2 onward. The marked
     * player has 14 actual game ticks to dodge out of the firing lane. */
    if(c->charge){
        if(!--c->charge){
            unsigned target=c->charge_target;
            const PcNetShip *p=target?guest:host;
            uint8_t *hp=target?&c->guest_hp:&c->host_hp;
            int32_t ex=c->charge_source?c->wing_x:c->x;
            int32_t ey=c->charge_source?c->wing_y:c->y;
            int32_t ez=c->charge_source?c->wing_z:c->z;
            int enemy_alive=c->charge_source?c->wing_hp:c->hp;
            c->last_attack_tick=frame;
            if(enemy_alive && p && *hp && p->world==host->world){
                int64_t dx=(int64_t)p->x-ex;
                int64_t dy=(int64_t)p->y-ey;
                int64_t dz=(int64_t)ez-p->z;
                int64_t r=300+dz/20;
                if(dz>250 && dz<9000 &&
                   dx>-r && dx<r && dy>-r && dy<r &&
                   dx*dx+dy*dy<=r*r){
                    *hp=*hp<=10?0:(uint8_t)(*hp-10);
                    c->beam=8;c->beam_target=(uint8_t)target;
                    c->beam_source=c->charge_source;
                }
            }
            if(!c->host_hp && !c->guest_hp){
                c->phase=3;c->down_tick=frame;c->beam=0;
                if(c->losses<255)c->losses++;
            }
        }
    }else if(frame-c->last_attack_tick>=35u &&
             (c->host_hp || c->guest_hp)){
        c->charge=14;
        c->charge_target^=1u;
        c->charge_source=(uint8_t)
            (c->wing_hp && (!c->hp || (frame/35u&1u)));
    }
}
int pc_coop_fire(PcCoop *c,unsigned shooter,const PcNetShip *a,
                 uint32_t frame){
    if(!c || !a || shooter>1 || c->phase!=1 ||
       !(c->hp || c->wing_hp) ||
       !(shooter?c->guest_hp:c->host_hp))return 0;
    PcNetShip enemy={c->x,c->y,c->z,a->world,0,0};
    uint8_t *target=0;
    if(c->hp && pc_duel_aim(a,&enemy))target=&c->hp;
    if(!target && c->wing_hp){
        enemy.x=c->wing_x;enemy.y=c->wing_y;enemy.z=c->wing_z;
        if(pc_duel_aim(a,&enemy))target=&c->wing_hp;
    }
    if(!target)return 0;
    uint8_t *hits=shooter?&c->guest_hits:&c->host_hits;
    if(*hits<255)(*hits)++;
    c->flash=10;
    *target=*target<=20?0:(uint8_t)(*target-20);
    if(!c->hp && !c->wing_hp){
        c->down_tick=frame?frame:1;
        if(c->wins<255)c->wins++;
        if(c->wave==2){
            c->phase=4; /* complete: hold mission-success screen */
            if(c->completed<31)c->completed++;
            c->charge=0;c->beam=0;
        }else c->phase=2; /* cleared wave: next enemies after 150 ticks */
    }
    return 1;
}
