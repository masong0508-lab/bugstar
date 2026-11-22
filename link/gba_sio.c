/* GBA cable preflight: true 16-bit MULTIPLAYER SIO register mode.
 * Different from GBC 8-bit serial. One word/2 logical ticks (~30Hz).
 * Compiled separately from the solo ROM until IRQ + actual emulator tests.
 * NO heap, framebuffers, save mutation, sound timer or busy-waiting. */
#include "gba_sio.h"
#define REG16(a) (*(volatile uint16_t *)(a))
#define SIO_MULTI0 REG16(0x04000120u)
#define SIO_MULTI1 REG16(0x04000122u)
#define SIO_CNT    REG16(0x04000128u)
#define SIO_SEND   REG16(0x0400012Au)
#define SIO_RCNT   REG16(0x04000134u)
#define SIO_IE     REG16(0x04000200u)
#define SIO_IF     REG16(0x04000202u)
#define SIO_MULTI_MODE 0x2000u
#define SIO_START_BIT 0x0080u
#define SIO_ERROR 0x0040u
#define SIO_INTERRUPT 0x4000u
#define SIO_IRQ_MASK 0x0080u
static GbaLinkState state;
static uint16_t tx[GBAL_WORDS],rx[GBAL_WORDS];
static uint16_t previous_rcnt,previous_cnt,previous_ie;
static uint16_t last_tx_tick;
static uint8_t on,role,tx_i,rx_n;
static volatile uint8_t word_pending,irq_error;
static volatile uint16_t irq_word;

static void prepare(uint16_t tick){
    GbaLinkPacket pkt;
    gbal_make(&state,&pkt,tick);
    (void)gbal_pack(tx,&pkt);
    tx_i=0;
}
void gba_sio_begin(uint8_t master,uint8_t mode,uint32_t seed){
    if(on || (master!=1 && master!=2))return;
    gbal_init(&state,master,mode,seed);
    if(!state.role)return;
    role=master;on=1;tx_i=rx_n=0;
    last_tx_tick=0;word_pending=irq_error=0;
    previous_rcnt=SIO_RCNT;previous_cnt=SIO_CNT;previous_ie=SIO_IE;
    SIO_RCNT=0; /* select Normal/Multiplayer rather than GPIO mode */
    SIO_CNT=(uint16_t)(SIO_MULTI_MODE|3u); /* 115,200 baud; IRQ not yet armed */
    prepare(0);
    SIO_SEND=tx[0];
}
void gba_sio_enable_irq(void){
    if(!on)return;
    /* Caller MUST first route IRQ_SERIAL in the game's existing IRQ handler.
     * Never hijack the timer/audio/VBlank IRQ or global IME. */
    SIO_IF=SIO_IRQ_MASK; /* acknowledge any leftover serial completion */
    SIO_IE|=SIO_IRQ_MASK;
    SIO_CNT|=SIO_INTERRUPT;
}
void gba_sio_irq(void){
    uint16_t cnt;
    if(!on)return;
    cnt=SIO_CNT;
    if(cnt&SIO_ERROR){irq_error=1;return;}
    /* SIOCNT ID bits 4..5 select master(0) or first slave(1).
     * The remote word is in the opposite player's MULTI register. */
    irq_word=((cnt>>4)&3u)?SIO_MULTI0:SIO_MULTI1;
    word_pending=1;
}
static void accept_word(uint16_t w,uint16_t tick){
    GbaLinkPacket packet;
    uint8_t i,n;
    if(rx_n<GBAL_WORDS)rx[rx_n++]=w;
    if(rx_n!=GBAL_WORDS)return;
    if(gbal_unpack(&packet,rx)==0){
        (void)gbal_receive(&state,&packet,tick);
        rx_n=0;return;
    }
    for(i=1;i<GBAL_WORDS;i++)if(rx[i]==GBAL_MAGIC)break;
    n=(uint8_t)(GBAL_WORDS-i);
    for(i=0;i<n;i++)rx[i]=rx[GBAL_WORDS-n+i];
    rx_n=n;
}
void gba_sio_tick(uint16_t tick){
    uint16_t w=0;uint8_t has_word=0;
    if(!on)return;
    gbal_timeout(&state,tick);
    if(irq_error){
        irq_error=0;rx_n=0;prepare(tick);
    }
    /* 16-bit stores are naturally atomic on ARM7TDMI. Clear availability
     * after taking its copy; only one transfer starts per two ticks. */
    if(word_pending){
        w=irq_word;word_pending=0;has_word=1;
    }
    if(has_word){
        if(w!=0xFFFFu)accept_word(w,tick);
        else rx_n=0; /* no first slave connected; never trust as a packet */
        if(++tx_i==GBAL_WORDS)prepare(tick);
    }
    SIO_SEND=tx[tx_i];
    if(role==1 && !(SIO_CNT&SIO_START_BIT) &&
       (uint16_t)(tick-last_tx_tick)>=2u){
        last_tx_tick=tick;
        SIO_CNT|=SIO_START_BIT; /* only the physical master supplies clock */
    }
}
void gba_sio_ready(uint8_t ready){
    if(on)gbal_set_ready(&state,ready);
}
const GbaLinkState *gba_sio_session(void){
    return on?&state:0;
}
void gba_sio_end(void){
    if(!on)return;
    SIO_CNT=previous_cnt;
    SIO_RCNT=previous_rcnt;
    SIO_IE=previous_ie;
    on=0;word_pending=irq_error=0;rx_n=tx_i=0;
    gbal_init(&state,0,0,0);
}
