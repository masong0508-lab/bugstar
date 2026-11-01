// pc_psg.c: the GBA's four-voice sound chip (2 square waves, 1 wave table, 1 noise), as a small software synth.
// The game's music and effects only use these four voices, so this is what plays them. No SDL in here: pc_snd() takes the
// register writes the game makes, pc_psg_render() makes stereo samples. Headless tests can write a WAV with it.
#include <stdint.h>
#include <string.h>
#include "pc_plat.h"

#define RATE PC_AUDIO_RATE
typedef struct {
    int on, dac;              // channel running; DAC (initial volume or envelope direction non-zero)
    double ph;                // phase 0..1
    int vol, envdir, envper, envt;     // envelope: volume 0..15, +1/-1/0, step length (in 1/64 s), counter
    int len, lenen;           // length counter (in 1/256 s ticks), enable
    int duty;
    int freq;                 // 11-bit register value
    int sweep_t, sweep_per, sweep_shift, sweep_dir;
    uint16_t lfsr; int nshift, ndiv, n7; double nacc; int nout;
} Ch;
static Ch c[4];
static uint16_t R[0x28];        // register shadow, index = offset/2 from 0x60
static uint8_t wave[2][32]; static int wbank_play, wbank_dim, wen;
static int master_on=1;
static int seq_fs; static double seq_acc;          // frame sequencer, in samples
static double hp_l, hp_r;       // output smoothing
static double hp_l_o, hp_r_o; static double dc_xl, dc_yl, dc_xr, dc_yr;   // DC blocker (the real chip's output is AC coupled)

void pc_psg_reset(void){ memset(c,0,sizeof c); memset(R,0,sizeof R); memset(wave,0,sizeof wave); wbank_play=0; seq_acc=0; c[3].lfsr=0x7FFF; master_on=1; hp_l=hp_r=0; }

static void trigger(int i){
    Ch *h=&c[i]; h->on=1; h->ph=0;
    if(i==0||i==1){ uint16_t e=R[i==0?1:4]; h->vol=e>>12; h->envdir=(e>>11)&1?1:-1; h->envper=(e>>8)&7; h->envt=h->envper; h->duty=(e>>6)&3;
        h->dac = (e&0xF800)!=0; if(!(h->lenen)) {} h->len=64-(e&63); if(!h->dac) h->on=0; }
    if(i==0){ uint16_t s=R[0]; h->sweep_shift=s&7; h->sweep_dir=(s>>3)&1; h->sweep_per=(s>>4)&7; h->sweep_t=h->sweep_per?h->sweep_per:8; if(h->sweep_shift&&0){} }
    if(i==2){ uint16_t w=R[0x12/2]; h->len=256-(w&255); h->dac=(R[0x10/2]>>7)&1; if(!h->dac) h->on=0; h->vol=0; }
    if(i==3){ uint16_t e=R[0x18/2]; h->vol=e>>12; h->envdir=(e>>11)&1?1:-1; h->envper=(e>>8)&7; h->envt=h->envper; h->len=64-(e&63); h->dac=(e&0xF800)!=0; if(!h->dac) h->on=0;
        h->lfsr=h->n7?0x7F:0x7FFF; h->nacc=0; h->nout=1; }
}
void pc_snd(unsigned off, unsigned v){
    v&=0xFFFF;
    if(off>=0x30 && off<0x40){ int i=(off-0x30)>>1; int bank=wbank_dim?0:(1-wbank_play);       // wave RAM: 16 bytes = 32 four-bit samples, high nibble first
        wave[bank][i*4]=(v>>4)&15; wave[bank][i*4+1]=v&15; wave[bank][i*4+2]=(v>>12)&15; wave[bank][i*4+3]=(v>>8)&15; return; }
    if(off>=0x28) return;
    R[off/2]=(uint16_t)v;
    switch(off){
    case 0x00: c[0].sweep_shift=v&7; c[0].sweep_dir=(v>>3)&1; c[0].sweep_per=(v>>4)&7; break;
    case 0x02: c[0].duty=(v>>6)&3; if(!(v&0xF800)){ c[0].dac=0; c[0].on=0; } break;
    case 0x04: c[0].freq=v&2047; c[0].lenen=(v>>14)&1; if(v&0x8000) trigger(0); break;
    case 0x08: c[1].duty=(v>>6)&3; if(!(v&0xF800)){ c[1].dac=0; c[1].on=0; } break;
    case 0x0C: c[1].freq=v&2047; c[1].lenen=(v>>14)&1; if(v&0x8000) trigger(1); break;
    case 0x10: wbank_dim=(v>>5)&1; wbank_play=(v>>6)&1; wen=(v>>7)&1; c[2].dac=wen; if(!wen) c[2].on=0; break;
    case 0x12: break;
    case 0x14: c[2].freq=v&2047; c[2].lenen=(v>>14)&1; if(v&0x8000) trigger(2); break;
    case 0x18: if(!(v&0xF800)){ c[3].dac=0; c[3].on=0; } break;
    case 0x1C: c[3].ndiv=v&7; c[3].n7=(v>>3)&1; c[3].nshift=(v>>4)&15; c[3].lenen=(v>>14)&1; if(v&0x8000) trigger(3); break;
    case 0x24: master_on=(v>>7)&1; if(!master_on){ for(int i=0;i<4;i++) c[i].on=0; } break;
    }
}
// one tick of the frame sequencer pieces
static void tick_len(void){ for(int i=0;i<4;i++){ Ch *h=&c[i]; if(h->on && h->lenen && h->len>0){ if(--h->len==0) h->on=0; } } }
static void tick_env(void){ int ids[3]={0,1,3}; for(int k=0;k<3;k++){ Ch *h=&c[ids[k]]; if(!h->on||!h->envper) continue; if(--h->envt<=0){ h->envt=h->envper; int nv=h->vol+h->envdir; if(nv>=0&&nv<=15) h->vol=nv; } } }
static void tick_sweep(void){ Ch *h=&c[0]; if(!h->on||!h->sweep_per) return; if(--h->sweep_t<=0){ h->sweep_t=h->sweep_per; if(h->sweep_shift){ int d=h->freq>>h->sweep_shift; int nf=h->sweep_dir?h->freq-d:h->freq+d; if(nf>2047) h->on=0; else if(nf>=0) h->freq=nf; } } }
static const double dutyw[4]={0.125,0.25,0.5,0.75};

void pc_psg_render(int16_t *out, int n){      // n stereo frames
    uint16_t cnt_l=R[0x20/2], cnt_h=R[0x22/2];
    int vr=cnt_l&7, vl=(cnt_l>>4)&7; int er=(cnt_l>>8)&15, el=(cnt_l>>12)&15;
    int psgv=cnt_h&3; double pm = psgv==0?0.25: psgv==1?0.5: psgv==2?1.0:0.0;
    for(int s=0;s<n;s++){
        // 512 Hz sequencer: length 256 Hz, sweep 128 Hz, envelope 64 Hz
        seq_acc+=1.0; if(seq_acc>=RATE/512.0){ seq_acc-=RATE/512.0; seq_fs=(seq_fs+1)&7; if((seq_fs&1)==0) tick_len(); if(seq_fs==2||seq_fs==6) tick_sweep(); if(seq_fs==7) tick_env(); }
        double ch[4]={0,0,0,0};
        if(master_on){
        for(int i=0;i<2;i++){ Ch *h=&c[i]; if(!h->on) continue; double f=131072.0/(2048-h->freq); double dt=f/RATE; double acc=0;
            for(int k=0;k<4;k++){ h->ph+=dt/4; if(h->ph>=1) h->ph-=1; acc+= h->ph<dutyw[h->duty]?1.0:-1.0; }
            ch[i]=acc/4*(h->vol/15.0); }
        { Ch *h=&c[2]; if(h->on && wen){ double f=2097152.0/(2048-h->freq)/32.0*(wbank_dim?0.5:1.0); double dt=f/RATE; double acc=0; int vs=(R[0x12/2]>>13)&3; int f75=(R[0x12/2]>>15)&1;
              double vm = f75?0.75: vs==0?0: vs==1?1.0: vs==2?0.5:0.25;
              for(int k=0;k<4;k++){ h->ph+=dt/4; if(h->ph>=1) h->ph-=1; int idx=(int)(h->ph*(wbank_dim?64:32)); int bank=wbank_play; if(wbank_dim){ bank=idx>=32?1-wbank_play:wbank_play; idx&=31; } acc+=(wave[bank][idx]-7.5)/7.5; }
              ch[2]=acc/4*vm; } }
        { Ch *h=&c[3]; if(h->on){ double r=h->ndiv?h->ndiv:0.5; double f=524288.0/r/(double)(1<<(h->nshift+1)); double dt=f/RATE; double acc=0;
            for(int k=0;k<4;k++){ h->nacc+=dt/4; while(h->nacc>=1){ h->nacc-=1; int x=(h->lfsr^(h->lfsr>>1))&1; h->lfsr=(h->lfsr>>1)|(x<<(h->n7?6:14)); h->nout=(h->lfsr&1)?-1:1; } acc+=h->nout; }
            ch[3]=acc/4*(h->vol/15.0); } }
        }
        double L=0,Rr=0; for(int i=0;i<4;i++){ if((el>>i)&1) L+=ch[i]; if((er>>i)&1) Rr+=ch[i]; }
        L*=pm*(vl+1)/8.0*0.45; Rr*=pm*(vr+1)/8.0*0.45;
        hp_l+=(L-hp_l)*0.55; hp_r+=(Rr-hp_r)*0.55;                 // a gentle low-pass: takes the harshest fizz off the naive squares
        { double yl=hp_l-dc_xl+0.9985*dc_yl; dc_xl=hp_l; dc_yl=yl; double yr=hp_r-dc_xr+0.9985*dc_yr; dc_xr=hp_r; dc_yr=yr; hp_l_o=yl; hp_r_o=yr; }
        double a=hp_l_o, b=hp_r_o; if(a>1)a=1; if(a<-1)a=-1; if(b>1)b=1; if(b<-1)b=-1;
        out[s*2]=(int16_t)(a*32000); out[s*2+1]=(int16_t)(b*32000);
    }
}
