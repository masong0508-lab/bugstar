// ================================================================= SOUND: effects and chiptune music on the GBA's sound chip
#define SND(o) (*(volatile u16*)(0x04000060+(o)))
#define REG_IE   (*(volatile u16*)0x04000200)
#define REG_IF   (*(volatile u16*)0x04000202)
#define REG_IME  (*(volatile u16*)0x04000208)
#define REG_DISPSTAT (*(volatile u16*)0x04000004)
// note numbers (MIDI): 0 = rest, 1 = keep holding
#define _ 0
#define H 1
// the planet: bright and wandering
static const u8 mel_planet[64]={76,H,79,H,81,H,79,76, 74,H,72,H,74,H,H,_, 76,H,79,H,84,H,81,79, 76,H,H,_,_,_,_,_,
                                77,H,81,H,84,H,81,77, 76,H,74,H,72,H,H,_, 74,H,76,H,79,H,76,74, 72,H,H,H,_,_,_,_};
static const u8 bas_planet[64]={48,H,H,H,55,H,H,H, 45,H,H,H,52,H,H,H, 48,H,H,H,55,H,H,H, 43,H,H,H,50,H,H,H,
                                41,H,H,H,48,H,H,H, 43,H,H,H,50,H,H,H, 45,H,H,H,52,H,H,H, 43,H,H,H,47,H,H,H};
// the dungeon: slow, minor, a little spooky
static const u8 mel_dun[64]={69,H,H,_,72,H,_,_, 76,H,H,H,74,H,72,_, 71,H,H,_,72,H,74,_, 72,H,H,H,H,_,_,_,
                             _,_,64,H,65,H,_,_, 69,H,H,_,68,H,H,_, 71,H,_,_,69,H,_,68, 69,H,H,H,H,H,_,_};
static const u8 bas_dun[64]={45,H,H,H,H,H,H,H, 41,H,H,H,H,H,H,H, 43,H,H,H,H,H,H,H, 40,H,H,H,H,H,H,H,
                             45,H,H,H,H,H,H,H, 41,H,H,H,H,H,H,H, 44,H,H,H,H,H,H,H, 45,H,H,H,40,H,H,H};
// the title: a little fanfare
static const u8 mel_title[64]={72,H,76,H,79,H,84,H, H,H,83,H,79,H,76,H, 77,H,81,H,84,H,89,H, H,H,88,H,84,H,81,H,
                               79,H,H,H,77,H,76,H, 74,H,H,H,72,H,74,H, 76,H,H,H,79,H,H,H, 72,H,H,H,H,H,_,_};
static const u8 bas_title[64]={48,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 57,H,H,H,H,H,H,H,
                               55,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 55,H,H,H,H,H,H,H, 48,H,H,H,H,H,H,H};
#undef _
#undef H
typedef struct { const u8 *mel, *bas; u8 tempo, vol; } Track;
static const Track tracks[3]={{mel_planet,bas_planet,9,0xA},{mel_dun,bas_dun,12,0x8},{mel_title,bas_title,8,0xB}};
static volatile int mus_track=-1, mus_step=0, mus_t=0;
static volatile int vbl_count=0;

IWRAM static void music_tick(void){
    vbl_count++;
    if(mus_track<0) return;
    if(--mus_t>0) return;
    const Track *t=&tracks[mus_track]; mus_t=t->tempo;
    int m=t->mel[mus_step], b=t->bas[mus_step];
    if(m==0) SND(0x08)=0;                                            // channel 2 quiet
    else if(m>1){ SND(0x08)=(t->vol<<12)|(1<<8)|(2<<6); SND(0x0C)=0x8000|sq_freq[m]; }
    if(b==0) SND(0x12)=0;
    else if(b>1){ SND(0x12)=(1<<13); SND(0x14)=0x8000|wv_freq[b]; }
    mus_step=(mus_step+1)&63;
}
IWRAM static void irq_handler(void){
    u16 f=REG_IF;
    if(f&1) music_tick();
    REG_IF=f; *(volatile u16*)0x03007FF8 |= f;
}
static void music(int n){ if(mus_track==n) return; mus_track=n; mus_step=0; mus_t=1; if(n<0){ SND(0x08)=0; SND(0x12)=0; } }
static void sound_init(void){
    SND(0x24)=0x80;                 // master on
    SND(0x20)=0xFF77;               // all four channels, both speakers, full volume
    SND(0x22)=0x0002;               // chip volume 100%
    // channel 3: a soft bass wave
    SND(0x10)=0x40;                 // write bank 0 while bank 1 plays
    static const u16 wave[8]={0x0123,0x4567,0x89AB,0xCDEF,0xFEDC,0xBA98,0x7654,0x3210};
    for(int i=0;i<8;i++) *(volatile u16*)(0x04000090+i*2)=wave[i];
    SND(0x10)=0x80;                 // play bank 0
    *(volatile u32*)0x03007FFC=(u32)irq_handler;
    REG_DISPSTAT |= 1<<3; REG_IE=1; REG_IME=1;
}
// little noises: 0 bite, 1 hit, 2 pick up, 3 fireball, 4 hurt, 5 stairs, 6 fanfare, 7 coin, 8 splash, 9 monster dies, 10 spark
static void sfx(int n){
    switch(n){
        case 0: SND(0x18)=(0xA<<12)|(1<<8); SND(0x1C)=0x8000|(3<<4)|1; break;
        case 1: SND(0x00)=(2<<4)|(1<<3)|3; SND(0x02)=(0xC<<12)|(1<<8)|(2<<6); SND(0x04)=0x8000|1750; break;
        case 2: SND(0x00)=(1<<4)|2; SND(0x02)=(0xB<<12)|(2<<8)|(2<<6); SND(0x04)=0x8000|1800; break;
        case 3: SND(0x18)=(0xF<<12)|(3<<8); SND(0x1C)=0x8000|(5<<4)|3; SND(0x00)=(3<<4)|(1<<3)|2; SND(0x02)=(0x9<<12)|(3<<8)|(1<<6); SND(0x04)=0x8000|1400; break;
        case 4: SND(0x00)=(3<<4)|(1<<3)|2; SND(0x02)=(0xF<<12)|(2<<8)|(3<<6); SND(0x04)=0x8000|1100; break;
        case 5: SND(0x00)=(7<<4)|(1<<3)|1; SND(0x02)=(0xC<<12)|(5<<8)|(2<<6); SND(0x04)=0x8000|1700; break;
        case 6: SND(0x00)=(6<<4)|1; SND(0x02)=(0xD<<12)|(6<<8)|(2<<6); SND(0x04)=0x8000|1550; break;
        case 7: SND(0x00)=0; SND(0x02)=(0xA<<12)|(1<<8)|(2<<6); SND(0x04)=0x8000|1930; break;
        case 8: SND(0x18)=(0xB<<12)|(4<<8); SND(0x1C)=0x8000|(4<<4)|2; break;
        case 9: SND(0x18)=(0xE<<12)|(2<<8); SND(0x1C)=0x8000|(6<<4)|4; break;
        case 10: SND(0x18)=(0xF<<12)|(1<<8); SND(0x1C)=0x8000|(1<<4)|0; SND(0x00)=(1<<4)|(1<<3)|1; SND(0x02)=(0xE<<12)|(1<<8)|(0<<6); SND(0x04)=0x8000|1980; break;
    }
}
