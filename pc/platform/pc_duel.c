/* Only the host executes this; guests receive host scores over EVENT.
 * Geometry uses the actual Bugstar space yaw/pitch coordinate convention.
 * No game-state or save-file pointer can be reached from this module.
 */
#include "pc_duel.h"
#include <math.h>
#include <string.h>
void pc_duel_reset(PcDuel *d){
    if(d){memset(d,0,sizeof *d);d->host_hp=100;d->guest_hp=100;}
}
int pc_duel_aim(const PcNetShip *a,const PcNetShip *b){
    if(!a || !b || a->world!=b->world)return 0;
    double dx=(double)b->x-a->x,dy=(double)b->y-a->y,dz=(double)b->z-a->z;
    double ds=dx*dx+dy*dy+dz*dz;
    if(ds<250.0*250.0 || ds>9500.0*9500.0)return 0;
    double yaw=(double)(a->yaw&1023u)*(6.2831853071795864769/1024.0);
    int16_t pitch=(int16_t)a->pitch;
    double pit=(double)pitch*(6.2831853071795864769/1024.0);
    double cp=cos(pit);
    double along=dx*sin(yaw)*cp+dy*sin(pit)+dz*cos(yaw)*cp;
    if(along<=0)return 0; /* behind the firing ship never counts */
    double radius=220.0+sqrt(ds)*0.055; /* forgiving 3D reticle tolerance */
    double miss=ds-along*along;
    return miss<=radius*radius;
}
int pc_duel_fire(PcDuel *d,uint8_t shooter,const PcNetShip *a,
                 const PcNetShip *b,uint32_t tick){
    if(!d || shooter>1 || d->knocked_at || !d->host_hp || !d->guest_hp ||
       !pc_duel_aim(a,b))return 0;
    uint8_t *target=shooter==0?&d->guest_hp:&d->host_hp;
    if(*target<=20){*target=0;d->knocked_at=tick?tick:1u;
        uint8_t *wins=shooter==0?&d->host_wins:&d->guest_wins;
        if(*wins<255)(*wins)++;
    }else *target-=20;
    return 1;
}
void pc_duel_step(PcDuel *d,uint32_t tick){
    if(d && d->knocked_at && tick-d->knocked_at>=120u){
        d->knocked_at=0;d->host_hp=100;d->guest_hp=100;
    }
}
