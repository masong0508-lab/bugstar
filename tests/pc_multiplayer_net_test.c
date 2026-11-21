/* Actual localhost two-socket transport test (no SDL/game needed).
 * Runs a host and a client with independent movement and world IDs.
 */
#include "../pc/platform/pc_net.h"
#include "../net/session.h"
#include <arpa/inet.h>
#include <sys/socket.h>
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
    /* Inject valid but stale host packets after a knockout. The client
     * must reject UDP reordering rather than restore old HP or teleport. */
    {
        BsNetPacket old={0}; uint8_t wire[BSNET_MAX_FRAME];
        struct sockaddr_in dst={0};
        dst.sin_family=AF_INET;dst.sin_port=htons(client.local_port);
        dst.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        old.type=BSNET_EVENT;old.tick=500;old.length=4;
        old.payload[0]=100;old.payload[1]=100;
        int n=bsnet_pack(&old,wire,sizeof wire);assert(n==12);
        assert(sendto((int)host.sock,wire,(size_t)n,0,
                      (struct sockaddr*)&dst,sizeof dst)==n);
        old.type=BSNET_STATE;old.length=8;old.tick=500;
        old.payload[0]=0x71;old.payload[1]=0x02; /* stale x=10000 */
        n=bsnet_pack(&old,wire,sizeof wire);assert(n==16);
        assert(sendto((int)host.sock,wire,(size_t)n,0,
                      (struct sockaddr*)&dst,sizeof dst)==n);
        pcnet_step(&client,630,&bc);
        uint8_t hh,gh,hw,gw;
        assert(pcnet_duel(&client,&hh,&gh,&hw,&gw) && gh==0 && hw==1);
        assert(pcnet_remote(&client,630,&got) && abs_i(got.x-ah.x)<40);
        /* A valid checksum is not enough: reject short authoritative
         * scores and ship states with deliberately newer frame numbers. */
        old.type=BSNET_EVENT;old.tick=631;old.length=0;
        n=bsnet_pack(&old,wire,sizeof wire);assert(n==8);
        assert(sendto((int)host.sock,wire,(size_t)n,0,
                      (struct sockaddr*)&dst,sizeof dst)==n);
        old.type=BSNET_STATE;old.tick=631;old.length=0;
        n=bsnet_pack(&old,wire,sizeof wire);assert(n==8);
        assert(sendto((int)host.sock,wire,(size_t)n,0,
                      (struct sockaddr*)&dst,sizeof dst)==n);
        pcnet_step(&client,631,&bc);
        assert(pcnet_duel(&client,&hh,&gh,&hw,&gw) && gh==0 && hw==1);
        assert(pcnet_remote(&client,631,&got) && abs_i(got.x-ah.x)<40);
    }
    for(unsigned t=632;t<750;t++){
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
