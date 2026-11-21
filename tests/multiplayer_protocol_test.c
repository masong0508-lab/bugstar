#include "../net/session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int payload_length(int type) {
    static const unsigned char lens[10]={0,5,1,3,8,4,0,2,0,24};
    return lens[type];
}
static void round_trip(int type, unsigned tick) {
    BsNetPacket orig, parsed;
    uint8_t encoded[BSNET_MAX_FRAME];
    int i, n;
    memset(&orig, 0, sizeof(orig));
    memset(&parsed, 0x5A, sizeof(parsed));
    orig.type = (uint8_t)type;
    orig.sequence = (uint8_t)(tick * 43u);
    orig.tick = (uint16_t)tick;
    orig.length = (uint8_t)payload_length(type);
    for (i=0;i<orig.length;i++) orig.payload[i]=(uint8_t)(tick+i*19u);
    n=bsnet_pack(&orig,encoded,(uint8_t)sizeof(encoded));
    assert(n==8+orig.length);
    assert(encoded[0]==BSNET_MAGIC && encoded[1]==BSNET_VERSION);
    assert(encoded[4]==(uint8_t)tick && encoded[5]==(uint8_t)(tick>>8));
    assert(bsnet_unpack(&parsed,encoded,(uint8_t)n)==0);
    assert(parsed.type==orig.type && parsed.sequence==orig.sequence);
    assert(parsed.tick==orig.tick && parsed.length==orig.length);
    assert(memcmp(parsed.payload,orig.payload,BSNET_MAX_PAYLOAD)==0);
    if (n>8) {
        BsNetPacket before=parsed;
        encoded[7]^=1;
        assert(bsnet_unpack(&parsed,encoded,(uint8_t)n)==BSNET_E_CRC);
        assert(memcmp(&before,&parsed,sizeof(parsed))==0);
        encoded[7]^=1;
    }
    encoded[1]^=1;
    assert(bsnet_unpack(&parsed,encoded,(uint8_t)n)==BSNET_E_VERSION);
    encoded[1]^=1;
    encoded[n-1]^=1;
    assert(bsnet_unpack(&parsed,encoded,(uint8_t)n)==BSNET_E_CRC);
    encoded[n-1]^=1;
    encoded[2]=255;
    assert(bsnet_unpack(&parsed,encoded,(uint8_t)n)==BSNET_E_TYPE);
}
int main(void) {
    BsNetPacket pkt,parsed;
    uint8_t b[BSNET_MAX_FRAME]={0}, zero[BSNET_MAX_FRAME]={0};
    int t;
    assert(bsnet_pack(0,b,sizeof(b))==BSNET_E_ARGS);
    assert(bsnet_unpack(0,b,sizeof(b))==BSNET_E_ARGS);
    assert(bsnet_unpack(&parsed,0,8)==BSNET_E_ARGS);
    assert(bsnet_unpack(&parsed,b,7)==BSNET_E_LENGTH);
    memset(&pkt,0,sizeof(pkt));
    pkt.type=BSNET_INPUT; pkt.length=3;
    assert(bsnet_pack(&pkt,b,8)==BSNET_E_LENGTH);
    pkt.length=255;
    assert(bsnet_pack(&pkt,b,sizeof(b))==BSNET_E_LENGTH);
    pkt.length=3;
    assert(bsnet_pack(&pkt,b,sizeof(b))==11);
    b[6]=4;
    assert(bsnet_unpack(&parsed,b,11)==BSNET_E_LENGTH);
    assert(memcmp(b,zero,1)!=0);
    for (t=1;t<=9;t++) {
        unsigned i;
        for (i=0;i<10000u;i++) round_trip(t,i*17u);
    }
    puts("Bugstar multiplayer packet contract: 9 types, 90,000 round trips, corruption/version/length checks PASS");
    return 0;
}
