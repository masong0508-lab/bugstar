#include "../pc/platform/pc_net.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    PcNetShip a={0,0,0,17,0,0},b={0,0,3000,17,512,0};
    PcDuel duel;
    pc_duel_reset(&duel);
    assert(duel.host_hp==100 && duel.guest_hp==100);
    assert(pc_duel_aim(&a,&b) && pc_duel_aim(&b,&a));
    b.world=18; assert(!pc_duel_aim(&a,&b)); b.world=17;
    b.x=4000; assert(!pc_duel_aim(&a,&b)); b.x=0;
    b.z=-3000; assert(!pc_duel_aim(&a,&b)); b.z=3000;
    b.z=12000; assert(!pc_duel_aim(&a,&b)); b.z=3000;
    assert(!pc_duel_fire(&duel,3,&a,&b,10));
    for(unsigned i=0;i<5;i++)assert(pc_duel_fire(&duel,0,&a,&b,11u+i*11u));
    assert(duel.guest_hp==0 && duel.host_wins==1);
    assert(!pc_duel_fire(&duel,1,&b,&a,90));
    pc_duel_step(&duel,100);assert(duel.guest_hp==0);
    pc_duel_step(&duel,175);assert(duel.guest_hp==100 && duel.host_hp==100);
    for(unsigned i=0;i<5;i++)assert(pc_duel_fire(&duel,1,&b,&a,200u+i*11u));
    assert(duel.host_hp==0 && duel.guest_wins==1);
    pc_duel_step(&duel,365);
    assert(duel.host_hp==100 && duel.guest_hp==100);
    puts("LAN sparring: aligned/missed/behind/out-of-world shots, host HP, both victories and respawn PASS");
    return 0;
}
