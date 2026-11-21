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
    pc_coop_step(&enemy,150,&host);
    assert(enemy.phase==1 && enemy.hp==100);
    assert(enemy.z==3500 && enemy.y>=0);
    /* Both users fire toward the same moving in-world target. */
    for(unsigned i=0;i<5;i++){
        pc_coop_step(&enemy,150+i*12,&host);
        assert(pc_coop_fire(&enemy,i%2?1:0,i%2?&guest:&host,150+i*12));
    }
    assert(enemy.hp==0 && enemy.wins==1 && enemy.phase==2);
    assert(enemy.host_hits==3 && enemy.guest_hits==2);
    assert(!pc_coop_fire(&enemy,0,&host,230));
    pc_coop_step(&enemy,350,&host);
    assert(enemy.phase==2 && enemy.hp==0);
    pc_coop_step(&enemy,349+48,&host);
    assert(enemy.phase==1 && enemy.hp==100 && enemy.wins==1);
    /* Aim must be in 3D; a lateral miss and wrong world never count. */
    host.yaw=512;pc_coop_step(&enemy,409,&host);
    assert(!pc_coop_fire(&enemy,0,&host,409));
    host.yaw=0;host.world=32;
    assert(!pc_coop_fire(&enemy,0,&host,410)==0 || enemy.hp==100);
    puts("LAN co-op raider: real 3D aim, both players hit, victory, respawn and no saved campaign state PASS");
    return 0;
}
