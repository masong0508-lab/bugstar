/* Two real UDP sockets, negotiated opt-in co-op mode, host target state. */
#include "../pc/platform/pc_net.h"
#include "../net/session.h"
#include <arpa/inet.h>
#include <sys/socket.h>
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
    /* A delayed, CRC-valid guest INPUT must never deliver a new hit
     * after the host has already consumed newer input packet ticks. */
    {
        BsNetPacket old={0}; uint8_t bytes[BSNET_MAX_FRAME];
        struct sockaddr_in dst={0};
        dst.sin_family=AF_INET;dst.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        dst.sin_port=htons(host.local_port);
        old.type=BSNET_INPUT;old.tick=120;old.length=3;old.payload[0]=1;
        int n=bsnet_pack(&old,bytes,sizeof bytes);assert(n==11);
        assert(sendto((int)client.sock,bytes,(size_t)n,0,
                      (struct sockaddr*)&dst,sizeof dst)==n);
        pcnet_step(&host,150,&h);
        assert(host.coop_enemy.hp==100 && host.coop_enemy.guest_hits==0);
    }
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
    assert(enemy.wave==1 && enemy.wing_hp==60 && enemy.completed==0);
    int32_t wing_host_x=enemy.wing_x,wing_host_z=enemy.wing_z;
    assert(pcnet_raider(&client,419,&enemy) && enemy.hp==100 && enemy.wins==1);
    assert(enemy.wave==1 && enemy.wing_hp==60 && enemy.completed==0);
    assert(enemy.wing_x-wing_host_x<=16 && enemy.wing_x-wing_host_x>=-16);
    assert(enemy.wing_z-wing_host_z<=16 && enemy.wing_z-wing_host_z>=-16);
    pcnet_close(&client);pcnet_step(&host,421,&h);
    assert(!pcnet_raider(&host,421,&enemy));
    pcnet_close(&host);
    puts("LAN co-op UDP: mode gate, joint damage, next-wave drone 3D pose/HP replication and respawn PASS");
    return 0;
}
