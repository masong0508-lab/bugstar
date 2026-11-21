/* Shared 3D co-op encounter: one host-owned enemy, no campaign state. */
#include "../pc/platform/pc_net.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    PcNetShip host={0,0,0,31,0,0};
    PcNetShip guest={192,0,1720,31,0,0};
    PcCoop enemy;
    pc_coop_reset(&enemy);
    assert(enemy.phase==0);
    pc_coop_step(&enemy,150,&host,&guest);
    assert(enemy.phase==1 && enemy.hp==100);
    assert(enemy.z==3500 && enemy.y>=0);
    /* Both users fire toward the same moving in-world target. */
    for(unsigned i=0;i<5;i++){
        pc_coop_step(&enemy,150+i*12,&host,&guest);
        assert(pc_coop_fire(&enemy,i%2?1:0,i%2?&guest:&host,150+i*12));
    }
    assert(enemy.hp==0 && enemy.wins==1 && enemy.phase==2);
    assert(enemy.host_hits==3 && enemy.guest_hits==2);
    assert(enemy.host_hp<=100 && enemy.guest_hp<=100);
    assert(enemy.losses==0);
    assert(!pc_coop_fire(&enemy,0,&host,230));
    pc_coop_step(&enemy,300,&host,&guest);
    assert(enemy.phase==2 && enemy.hp==0);
    pc_coop_step(&enemy,397,&host,&guest);
    assert(enemy.phase==1 && enemy.hp==100 && enemy.wins==1);
    /* Aim must be in 3D; a lateral miss and wrong world never count. */
    host.yaw=512;pc_coop_step(&enemy,409,&host,&guest);
    assert(!pc_coop_fire(&enemy,0,&host,409));
    host.yaw=0;host.y=4000;
    assert(!pc_coop_fire(&enemy,0,&host,410));
    assert(enemy.hp==100);
    /* When both pilots remain in the firing lane without defeating the
     * raider, its deterministic attacks end the round, not the campaign. */
    pc_coop_reset(&enemy);
    host.y=0;guest.y=0;host.yaw=0;
    for(unsigned t=1;t<2000 && enemy.phase!=3;t++)
        pc_coop_step(&enemy,t,&host,&guest);
    assert(enemy.phase==3 && enemy.losses==1);
    assert(enemy.host_hp==0 && enemy.guest_hp==0);
    assert(enemy.wins==0);
    /* New combat affordance: a laser target is announced first; lateral
     * dodging during the countdown must prevent damage to both pilots. */
    pc_coop_reset(&enemy);host.x=0;host.y=0;
    guest.x=192;guest.y=0;
    unsigned lock=0, fire=0;
    for(unsigned t=1;t<180;t++){
        pc_coop_step(&enemy,t,&host,&guest);
        if(enemy.charge && !lock){
            lock=t;
            if(enemy.charge_target)guest.x=1500;
            else host.x=1500;
        }
        if(lock && !enemy.charge && t>lock){fire=t;break;}
    }
    assert(lock && fire && fire>lock+5);
    assert(enemy.host_hp==100 && enemy.guest_hp==100);
    assert(enemy.phase==1 && enemy.wins==0 && enemy.losses==0);
    puts("LAN co-op raider: real 3D aim, both players hit, victory, respawn and no saved campaign state PASS");
    return 0;
}
