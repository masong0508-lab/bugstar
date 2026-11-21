#include "gba_link.h"
#include <string.h>
/* CRC16-CCITT over the first 5 words as 10 little-endian bytes. */
static uint16_t crc_words(const uint16_t *w){
    uint16_t c=0xFFFFu;
    uint8_t k,i;
    for(k=0;k<5;k++){
        uint8_t b=(uint8_t)w[k];
        for(i=0;i<2;i++){
            uint8_t q;
            c^=(uint16_t)b<<8;
            for(q=0;q<8;q++)
                c=(uint16_t)((c<<1)^((c&0x8000u)?0x1021u:0u));
            b=(uint8_t)(w[k]>>8);
        }
    }
    return c;
}
int gbal_pack(uint16_t out[GBAL_WORDS],const GbaLinkPacket *p){
    if(!out || !p || (p->flags&GBAL_FLAG_MODE)<1 ||
       (p->flags&GBAL_FLAG_MODE)>2)return -1;
    out[0]=GBAL_MAGIC;
    out[1]=(uint16_t)(((uint16_t)p->seq<<8)|p->flags);
    out[2]=p->tick;
    out[3]=(uint16_t)p->seed;
    out[4]=(uint16_t)(p->seed>>16);
    out[5]=crc_words(out);return 0;
}
int gbal_unpack(GbaLinkPacket *p,const uint16_t in[GBAL_WORDS]){
    GbaLinkPacket next;
    if(!p || !in)return -1;
    if(in[0]!=GBAL_MAGIC)return -2;
    if(crc_words(in)!=in[5])return -3;
    next.seq=(uint8_t)(in[1]>>8);
    next.flags=(uint8_t)in[1];
    if((next.flags&GBAL_FLAG_MODE)<1 || 
       (next.flags&GBAL_FLAG_MODE)>2)return -4;
    next.tick=in[2];
    next.seed=(uint32_t)in[3]|((uint32_t)in[4]<<16);
    *p=next;return 0;
}
void gbal_init(GbaLinkState *s,uint8_t role,uint8_t mode,uint32_t seed){
    if(!s)return;
    memset(s,0,sizeof *s);
    if(role<1 || role>2 || mode<1 || mode>2)return;
    s->role=role;s->mode=mode;s->seed=seed;s->local_active=1;
}
void gbal_set_ready(GbaLinkState *s,uint8_t ready){
    if(s && s->role)s->local_ready=(uint8_t)(ready!=0);
}
void gbal_set_active(GbaLinkState *s,uint8_t active){
    if(s && s->role)s->local_active=(uint8_t)(active!=0);
}
static void drop_peer(GbaLinkState *s){
    s->connected=0;s->peer_ready=0;s->peer_active=0;
    s->local_ready=0;s->seen_seq=0;s->reconnecting=1;
}
void gbal_timeout(GbaLinkState *s,uint16_t now){
    if(s && s->role && s->connected &&
       (uint16_t)(now-s->last_rx)>GBAL_TIMEOUT)drop_peer(s);
}
void gbal_make(GbaLinkState *s,GbaLinkPacket *p,uint16_t tick){
    if(!s || !s->role || !p)return;
    p->seed=s->seed;p->tick=tick;p->seq=s->tx_seq++;
    p->flags=(uint8_t)(s->mode |
        (!s->connected?GBAL_FLAG_HELLO:0u) |
        (s->local_ready?GBAL_FLAG_READY:0u) |
        (s->local_active?GBAL_FLAG_ACTIVE:0u));
}
int gbal_receive(GbaLinkState *s,const GbaLinkPacket *p,uint16_t now){
    uint8_t delta;
    if(!s || !p || !s->role ||
       p->seed!=s->seed || (p->flags&GBAL_FLAG_MODE)!=s->mode)return -1;
    if(s->seen_seq){
        delta=(uint8_t)(p->seq-s->peer_seq);
        if(!delta || delta>127u)return -2;
    }
    s->seen_seq=1;s->peer_seq=p->seq;s->last_rx=now;
    if(p->flags&GBAL_FLAG_BYE){drop_peer(s);return 1;}
    if(p->flags&GBAL_FLAG_HELLO){
        if(!s->connected){
            s->connected=1;s->peer_ready=0;s->peer_active=0;
        }
        return 1;
    }
    /* The peer that received our HELLO may immediately answer with READY
     * rather than another HELLO. A valid same-seed reply also completes
     * handshake; rejecting it left the clock master waiting forever. */
    if(!s->connected){
        s->connected=1;s->peer_ready=0;s->peer_active=0;
    }
    s->peer_ready=(uint8_t)((p->flags&GBAL_FLAG_READY)!=0u);
    s->peer_active=(uint8_t)((p->flags&GBAL_FLAG_ACTIVE)!=0u);
    if(gbal_can_play(s))s->reconnecting=0;
    return 1;
}
uint8_t gbal_confirmed(const GbaLinkState *s){
    return (uint8_t)(s && s->connected && s->local_ready && s->peer_ready);
}
uint8_t gbal_can_play(const GbaLinkState *s){
    return (uint8_t)(gbal_confirmed(s) && s->local_active && s->peer_active);
}
