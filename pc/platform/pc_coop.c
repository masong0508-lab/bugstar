/* BUGSTAR LAN co-op vertical slice: both pilots attack one shared raider.
 * Only the host calls step/fire. Its 3D pose, HP and victories replicate
 * to the guest; none of these values can alter campaign HP, loot or saves.
 * Fixed memory and integer-only movement (except existing aim geometry).
 */
#include "pc_net.h"
#include <string.h>
void pc_coop_reset(PcCoop *c){if(c)memset(c,0,sizeof *c);}
void pc_coop_step(PcCoop *c,uint32_t frame,const PcNetShip *host){
    if(!c || !host)return;
    if(!c->phase){
        c->anchor_x=host->x;c->anchor_y=host->y;
        c->anchor_z=host->z;
        c->hp=100;c->phase=1;c->down_tick=0;
    }
    if(c->phase==2){
        if(frame-c->down_tick>=150u){c->hp=100;c->phase=1;c->flash=0;}
        else return;
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
}
int pc_coop_fire(PcCoop *c,unsigned shooter,const PcNetShip *a,
                 uint32_t frame){
    if(!c || !a || shooter>1 || c->phase!=1 || !c->hp)return 0;
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
