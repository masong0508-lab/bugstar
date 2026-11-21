/* Actual localhost two-socket transport test (no SDL/game needed).
 * Runs a host and a client with independent movement and world IDs.
 */
#include "../pc/platform/pc_net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int abs_i(int x){return x<0?-x:x;}
int main(void){
    PcNetSession host, client;
    PcNetShip ah={1000,-2000,-8000,0xA0B0C0D0u,32,4};
    PcNetShip bc={-4000,3500,-10000,0xA0B0C0D0u,900,-48};
    PcNetShip got;
    memset(&host,0,sizeof host);memset(&client,0,sizeof client);
    assert(pcnet_host(&host,0));
    assert(host.local_port>0);
    assert(pcnet_join(&client,"127.0.0.1",host.local_port));
    assert(!pcnet_remote(&host,0,&got));
    for(unsigned t=1;t<120;t++){
        ah.x+=8;bc.z+=16;
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
        pcnet_step(&client,t,&bc);
    }
    assert(host.connected && client.connected);
    assert(pcnet_remote(&host,119,&got));
    assert(got.world==bc.world && abs_i((int)(got.x-bc.x))<=32);
    assert(got.y==bc.y);
    assert(abs_i((int)(got.z-bc.z))<=32);
    assert(abs_i((int)got.yaw-(int)bc.yaw)<=4);
    assert(pcnet_remote(&client,119,&got));
    assert(abs_i((int)(got.x-ah.x))<=32);
    /* A client in another star must not join. Reconnection requires world match. */
    pcnet_close(&client);
    assert(pcnet_join(&client,"127.0.0.1",host.local_port));
    bc.world=0x12345678u;
    for(unsigned t=121;t<400;t++){
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
    }
    assert(!host.connected && !client.connected);
    bc.world=ah.world;
    for(unsigned t=401;t<540;t++){
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
        pcnet_step(&client,t,&bc);
    }
    assert(host.connected && client.connected);
    assert(pcnet_remote(&client,539,&got));
    pcnet_close(&client);
    pcnet_step(&host,541,&ah);
    assert(!host.connected && !pcnet_remote(&host,541,&got));
    pcnet_close(&host);
    puts("LAN space prototype: 2 UDP sockets, world match, bidirectional ship movement, disconnect/rejoin PASS");
    return 0;
}
