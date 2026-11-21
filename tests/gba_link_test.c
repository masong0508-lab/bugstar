/* GBA 16-bit SIO packet codec and state tests, not an emulator claim. */
#include "../link/gba_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void exchange(GbaLinkState *from,GbaLinkState *to,uint16_t t){
    GbaLinkPacket p,got;uint16_t bytes[GBAL_WORDS];
    gbal_make(from,&p,t);assert(!gbal_pack(bytes,&p));
    assert(!gbal_unpack(&got,bytes));
    assert(gbal_receive(to,&got,t)==1);
}
int main(void){
    uint16_t words[GBAL_WORDS],bad[GBAL_WORDS];
    GbaLinkPacket p={0},got={0},old;
    for(uint32_t t=0;t<20000;t++){
        p.seed=0x6F4Au^t;p.tick=(uint16_t)t;p.seq=(uint8_t)t;
        p.flags=GBAL_MODE_COOP|((t&1)?GBAL_FLAG_READY:0u);
        assert(!gbal_pack(words,&p));assert(!gbal_unpack(&got,words));
        assert(got.seed==p.seed && got.seq==p.seq &&
               got.tick==p.tick && got.flags==p.flags);
        old=got;memcpy(bad,words,sizeof bad);
        bad[4]^=0x4000u;
        assert(gbal_unpack(&got,bad)==-3);
        assert(!memcmp(&old,&got,sizeof got));
        bad[0]=0;
        assert(gbal_unpack(&got,bad)==-2);
    }
    GbaLinkState host,guest;
    gbal_init(&host,1,GBAL_MODE_COOP,0xBEEF2026u);
    gbal_init(&guest,2,GBAL_MODE_COOP,0xBEEF2026u);
    exchange(&host,&guest,1);exchange(&guest,&host,2);
    assert(host.connected && guest.connected);
    gbal_set_ready(&host,1);exchange(&host,&guest,3);
    assert(!gbal_can_play(&host) && !gbal_can_play(&guest));
    gbal_set_ready(&guest,1);exchange(&guest,&host,4);
    assert(gbal_can_play(&host) && gbal_can_play(&guest));
    gbal_set_active(&guest,0);exchange(&guest,&host,5);
    assert(!gbal_can_play(&host) && !gbal_can_play(&guest));
    gbal_set_active(&guest,1);exchange(&guest,&host,6);
    assert(gbal_can_play(&host) && gbal_can_play(&guest));
    gbal_timeout(&guest,127);gbal_timeout(&host,127);
    assert(!host.connected && !guest.connected);
    assert(!host.local_ready && !guest.local_ready);
    exchange(&host,&guest,128);exchange(&guest,&host,129);
    assert(host.connected && guest.connected && !gbal_can_play(&host));
    gbal_set_ready(&host,1);gbal_set_ready(&guest,1);
    exchange(&host,&guest,130);exchange(&guest,&host,131);
    assert(gbal_can_play(&host) && gbal_can_play(&guest));
    puts("GBA SIO codec: 20000 round trips and dual Ready, pause, timeout, rejoin PASS");
    return 0;
}
