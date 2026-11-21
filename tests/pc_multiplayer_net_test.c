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
    assert(abs_i((int)(got.y-bc.y))<=16); /* 16-unit wire quantization */
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
    /* Two independent players now enter an aligned training engagement. */
    ah.x=0;ah.y=0;ah.z=0;ah.yaw=0;ah.pitch=0;
    bc.x=0;bc.y=0;bc.z=3000;bc.yaw=512;bc.pitch=0;
    for(unsigned t=540;t<560;t++){
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
        pcnet_step(&client,t,&bc);
    }
    {uint8_t hh,gh,hw,gw;
      assert(pcnet_duel(&client,&hh,&gh,&hw,&gw));
      assert(hh==100 && gh==100 && hw==0 && gw==0);}
    for(unsigned t=560;t<630;t++){
        if(t==565||t==577||t==589||t==601||t==613)pcnet_fire(&host);
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
        pcnet_step(&client,t,&bc);
    }
    {uint8_t hh,gh,hw,gw;
      assert(pcnet_duel(&host,&hh,&gh,&hw,&gw));
      assert(hh==100 && gh==0 && hw==1 && gw==0);
      assert(pcnet_duel(&client,&hh,&gh,&hw,&gw));
      assert(hh==100 && gh==0 && hw==1 && gw==0);}
    for(unsigned t=630;t<750;t++){
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
        pcnet_step(&client,t,&bc);
    }
    for(unsigned t=750;t<820;t++){
        if(t==756||t==768||t==780||t==792||t==804)pcnet_fire(&client);
        pcnet_step(&client,t,&bc);
        pcnet_step(&host,t,&ah);
        pcnet_step(&client,t,&bc);
    }
    {uint8_t hh,gh,hw,gw;
      assert(pcnet_duel(&host,&hh,&gh,&hw,&gw));
      assert(hh==0 && gh==100 && hw==1 && gw==1);
      assert(pcnet_duel(&client,&hh,&gh,&hw,&gw));
      assert(hh==0 && gh==100 && hw==1 && gw==1);}
    pcnet_close(&client);
    pcnet_step(&host,821,&ah);
    assert(!host.connected && !pcnet_remote(&host,821,&got));
    pcnet_close(&host);
    puts("LAN combat: 2 UDP sockets, world match, rejoin, host/client hits, authoritative HP, both victories PASS");
    return 0;
}
