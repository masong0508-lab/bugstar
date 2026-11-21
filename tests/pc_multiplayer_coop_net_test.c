/* Two real UDP sockets, negotiated opt-in co-op mode, host target state. */
#include "../pc/platform/pc_net.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    PcNetSession host,client;PcCoop enemy;
    PcNetShip h={0,0,0,0x49u,0,0},g={192,0,1720,0x49u,0,0};
    assert(pcnet_host(&host,0));pcnet_set_coop(&host,1);
    assert(pcnet_join(&client,"127.0.0.1",host.local_port));
    /* Ordinary sparring clients must NOT join a co-op host. */
    for(unsigned t=1;t<100;t++){
        pcnet_step(&client,t,&g);pcnet_step(&host,t,&h);
    }
    assert(!host.connected && !client.connected);
    pcnet_close(&client);
    assert(pcnet_join(&client,"127.0.0.1",host.local_port));
    pcnet_set_coop(&client,1);
    int guest_saw_lock=0;
    for(unsigned t=101;t<150;t++){
        pcnet_step(&client,t,&g);pcnet_step(&host,t,&h);
        pcnet_step(&client,t,&g);
        PcCoop peer_seen,host_seen;
        if(pcnet_raider(&client,t,&peer_seen) && peer_seen.charge){
            assert(pcnet_raider(&host,t,&host_seen));
            assert(peer_seen.charge_target==host_seen.charge_target);
            guest_saw_lock=1;
        }
    }
    assert(host.connected && client.connected);
    assert(guest_saw_lock);
    assert(pcnet_raider(&host,149,&enemy) && enemy.hp==100);
    assert(pcnet_raider(&client,149,&enemy) && enemy.hp==100);
    assert(enemy.z==3504 || enemy.z==3488 || enemy.z==3500);
    for(unsigned t=150;t<240;t++){
        if(t==155||t==179||t==203)pcnet_fire(&host);
        if(t==167||t==191)pcnet_fire(&client);
        pcnet_step(&client,t,&g);pcnet_step(&host,t,&h);
        pcnet_step(&client,t,&g);
    }
    assert(pcnet_raider(&host,239,&enemy));
    assert(enemy.hp==0 && enemy.phase==2 && enemy.wins==1);
    assert(enemy.host_hits==3 && enemy.guest_hits==2);
    uint8_t p1hp=enemy.host_hp,p2hp=enemy.guest_hp;
    assert(pcnet_raider(&client,239,&enemy));
    assert(enemy.host_hp==p1hp && enemy.guest_hp==p2hp);
    assert(enemy.hp==0 && enemy.phase==2 && enemy.wins==1);
    assert(enemy.host_hits==3 && enemy.guest_hits==2);
    for(unsigned t=240;t<420;t++){
        pcnet_step(&client,t,&g);pcnet_step(&host,t,&h);
        pcnet_step(&client,t,&g);
    }
    assert(pcnet_raider(&host,419,&enemy) && enemy.hp==100 && enemy.wins==1);
    assert(pcnet_raider(&client,419,&enemy) && enemy.hp==100 && enemy.wins==1);
    pcnet_close(&client);pcnet_step(&host,421,&h);
    assert(!pcnet_raider(&host,421,&enemy));
    pcnet_close(&host);
    puts("LAN co-op UDP: mode gate, two pilots damaging one raider, replicated HP and victory, respawn PASS");
    return 0;
}
