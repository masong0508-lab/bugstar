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
// space: slow, starry arpeggios
static const u8 mel_space[64]={69,H,72,H,76,H,72,H, 69,H,72,H,76,H,79,H, 77,H,74,H,69,H,74,H, 77,H,H,H,76,H,74,H,
                               72,H,76,H,79,H,76,H, 72,H,76,H,79,H,84,H, 83,H,79,H,76,H,74,H, 76,H,H,H,H,H,_,_};
static const u8 bas_space[64]={45,H,H,H,H,H,H,H, 45,H,H,H,52,H,H,H, 50,H,H,H,H,H,H,H, 50,H,H,H,57,H,H,H,
                               48,H,H,H,H,H,H,H, 48,H,H,H,55,H,H,H, 43,H,H,H,H,H,H,H, 40,H,H,H,47,H,H,H};
// the boss: fast and tense
static const u8 mel_boss[64]={64,H,67,_,64,H,71,_, 70,H,67,_,64,H,62,H, 64,H,67,_,64,H,72,_, 71,H,67,_,66,H,H,_,
                              76,H,74,H,72,H,71,H, 72,H,71,H,69,H,67,H, 69,H,71,H,72,H,74,H, 75,H,H,H,71,H,H,_};
static const u8 bas_boss[64]={40,_,40,_,52,_,40,_, 40,_,40,_,52,_,40,_, 36,_,36,_,48,_,36,_, 35,_,35,_,47,_,35,_,
                              40,_,40,_,52,_,40,_, 38,_,38,_,50,_,38,_, 36,_,36,_,48,_,36,_, 35,_,35,_,47,_,47,_};
// the ending: warm and slow
static const u8 mel_end[64]={72,H,H,H,74,H,76,H, 79,H,H,H,76,H,H,H, 77,H,H,H,76,H,74,H, 72,H,H,H,H,H,_,_,
                             69,H,H,H,72,H,74,H, 76,H,H,H,79,H,H,H, 77,H,76,H,74,H,71,H, 72,H,H,H,H,H,H,_};
static const u8 bas_end[64]={48,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 48,H,H,H,55,H,H,H,
                             45,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 50,H,H,H,43,H,H,H, 48,H,H,H,H,H,H,H};
// the races: quick and bouncy
static const u8 mel_race[64]={72,H,76,H,79,H,76,H, 74,H,77,H,81,H,77,H, 76,H,79,H,84,H,83,81, 79,H,H,H,_,_,79,81,
                              84,H,83,H,81,H,79,H, 77,H,76,H,74,H,72,H, 74,H,76,H,77,H,79,H, 76,H,74,H,72,H,H,_};
static const u8 bas_race[64]={48,_,48,_,55,_,48,_, 50,_,50,_,57,_,50,_, 52,_,52,_,59,_,52,_, 55,_,55,_,43,_,55,_,
                              53,_,53,_,60,_,53,_, 52,_,52,_,59,_,52,_, 50,_,50,_,57,_,50,_, 55,_,55,_,43,_,47,_};
#undef _
#undef H
typedef struct { const u8 *mel, *bas; u8 tempo, vol; } Track;
// 0 planet, 1 dungeon, 2 title, 3 space, 4 boss, 5 ending, 6 kart race, 7 the final lap (faster)
// The eight tunes above are the THEMES. The engine below plays each theme as the first verse, then goes on with new verses, choruses and bridges
// it makes from the world's seed (key, scale, chords, rhythm), changes key every lap, and adds layers (arpeggio, drums, drips, heartbeat) as the game asks.
static const Track tracks[8]={{mel_planet,bas_planet,9,0xA},{mel_dun,bas_dun,12,0x8},{mel_title,bas_title,8,0xB},{mel_space,bas_space,11,0x9},{mel_boss,bas_boss,6,0xA},{mel_end,bas_end,12,0xA},{mel_race,bas_race,7,0xA},{mel_race,bas_race,5,0xB}};
// per tune: melody centre note, bass style, drums (0 only in fights, 1 always, 2 only in choruses), scales allowed (bit per scale), arpeggio layer on in verses, ambience (drips)
typedef struct { u8 base, bass, drum, modes, arp, amb; } MCfg;
static const MCfg mcfg[8]={ {72,1,0,0x35,0,0}, {69,0,0,0x0E,0,1}, {72,0,2,0x11,1,0}, {69,0,0,0x34,1,0}, {64,2,1,0x4A,0,0}, {72,0,0,0x11,1,0}, {72,2,1,0x21,0,0}, {72,2,1,0x21,1,0} };
// scales: 0 major 1 minor 2 dorian 3 phrygian 4 lydian 5 mixolydian 6 harmonic minor
static const u8 mscale[7][7]={{0,2,4,5,7,9,11},{0,2,3,5,7,8,10},{0,2,3,5,7,9,10},{0,1,3,5,7,8,10},{0,2,4,6,7,9,11},{0,2,4,5,7,9,10},{0,2,3,5,7,8,11}};
static const u8 mprog[8][4]={{0,4,5,3},{0,5,3,4},{5,3,0,4},{0,3,4,3},{0,2,5,4},{0,6,5,4},{0,3,6,4},{0,5,2,6}};      // one chord (a scale degree) per bar
static const u16 mrhy[12]={0x0001,0x0101,0x0011,0x1001, 0x1111,0x1151,0x1415,0x4545, 0x5555,0x5D5D,0x5557,0x7777};   // which of the 16 steps get a note: sparse, medium, busy
// form (28 bars, about a minute): 0 intro, 1 theme verse, 2 new verse, 3 chorus, 4 bridge, 5 outro. Every second time round the theme verse is a new one.
static const u8 mform[8]={0,1,2,3,1,4,3,5};
static const u8 mbars[6]={2,4,4,4,4,2};
static const s8 mdyn[6]={-3,0,0,2,-1,-2};
static const u8 menv[6]={3,1,1,2,2,3};
static const u8 mduty[6]={2,2,1,2,0,2};
static const s8 mkoff[4]={0,2,5,7};       // the key climbs a little each time round
static const s8 mstp[6]={-2,-1,-1,1,1,2};
// stingers: a few bars of melody and bass that cut in over the music
static const u8 stg_mel[2][16]={{72,1,76,1,79,1,84,1,1,1,1,1,0,0,0,0},{72,1,1,1,68,1,1,1,65,1,1,1,60,1,1,1}};
static const u8 stg_bas[2][16]={{48,1,1,1,55,1,1,1,60,1,1,1,0,0,0,0},{48,1,1,1,44,1,1,1,41,1,1,1,36,1,1,1}};
typedef struct { u32 seed; s8 key; u8 mode, tadj, sec, bar, cyc, stg, stg_i, mv, fade_out, low, night, amb, drumsf, kind, lock;
                 s8 pend, mforce, md, prg, drm, hrt; u8 pv, pc, pb, hide; u16 rm; u32 bseed; } MusSt;
static MusSt mus;
static volatile int mus_track=-1, mus_step=0, mus_t=0;
static volatile u8 mus_hold1=0, mus_hold4=0; static volatile u16 mus_fight=0;
static volatile int vbl_count=0;
static void mus_gather(int n, int full);          // (game.inc) read the world: seed, scale, hour, weather, danger
__attribute__((noinline,used)) static void mus_gen(void);
static void (*volatile mus_gen_p)(void)=mus_gen;     // (called through a pointer: the interrupt runs from fast RAM and this lives in the cartridge)

IWRAM static void music_tick(void){
    vbl_count++;
    if(mus_hold1) mus_hold1--; if(mus_hold4) mus_hold4--; if(mus_fight) mus_fight--;
    if(mus_track<0 || opt_music==0) return;
    if(--mus_t>0) return;
    mus_gen_p();
}
IWRAM static void irq_handler(void){
    u16 f=REG_IF;
    if(f&1){ music_tick(); m7_irq(); rumble_tick(); }
    REG_IF=f; *(volatile u16*)0x03007FF8 |= f;
}
static u32 mh(u32 a, u32 b){ a ^= b*0x9E3779B1u; a ^= a>>15; a *= 0x85EBCA77u; a ^= a>>13; a *= 0xC2B2AE3Du; a ^= a>>16; return a; }
static int mdeg(int d, const MCfg *c, int tr){        // a scale degree (any octave) -> a note number
    int i=((d%7)+7)%7, o=(d-i)/7; int n=c->base + tr + mscale[mus.mode%7][i] + 12*o; return n<50?50:n>98?98:n; }
static int mbassn(int d, int tr12){ int n=36+tr12+mscale[mus.mode%7][((d%7)+7)%7]; return n<30?30:n>66?66:n; }
static void mus_start(int n){
    MusSt *m=&mus; const MCfg *c=&mcfg[n&7]; int i,k,cnt=0;
    mus_track=n; m->mforce=-1; m->tadj=0; m->drumsf=0; m->lock=0; m->seed=mh(n+1,vbl_count>>3); m->stg=0; m->fade_out=0;
    mus_gather(n,1);
    for(i=0;i<7;i++) if((c->modes>>i)&1) cnt++;
    k=cnt? (int)((m->seed>>8)%cnt) : 0; m->mode=0;
    for(i=0;i<7;i++) if((c->modes>>i)&1){ if(!k){ m->mode=i; break; } k--; }
    if(m->mforce>=0) m->mode=m->mforce%7;
    m->key = m->lock ? 0 : (s8)((m->seed>>16)%12); if(m->key>6) m->key-=12;
    m->pv=m->seed&7; m->pc=(m->seed>>4)&7; m->pb=(m->seed>>8)&7; if(m->pb==m->pc) m->pb=(m->pb+3)&7; if(m->pv==m->pc) m->pv=(m->pv+5)&7;
    m->sec=0; m->bar=0; m->cyc=0; m->md=0; m->kind=0; m->rm=0; m->bseed=0; m->prg=0;
    mus_step=0; mus_t=1;
}
static void music(int n){
    u16 ime=REG_IME; REG_IME=0;
    if(n>7) n=0;
    if(n<0){ mus_track=-1; mus.pend=-1; mus.fade_out=0; mus.stg=0; SND(0x08)=0; SND(0x12)=0; }
    else if(mus_track==n){ mus.fade_out=0; }
    else if(mus_track<0 || opt_music==0){ mus_start(n); mus.mv=4; }
    else if(mus_track==6 && n==7){ mus_track=7; mus.fade_out=0; }     // the final lap: same tune, faster, and it does not start over
    else { mus.pend=n; mus.fade_out=1; mus_t=1; }
    REG_IME=ime;
}
static void music_sting(int k){                      // 0 victory, 1 defeat
    u16 ime=REG_IME; REG_IME=0;
    if(opt_music!=0 && mus_track>=0 && (k==0||k==1)){ mus.stg=(u8)(k+1); mus.stg_i=0; mus_t=1; }
    REG_IME=ime;
}
// ONE STEP OF THE MUSIC (a 16th note; 16 of them make a bar). Runs from the vblank interrupt but sits in the cartridge: about 100 cycles a step, nothing per frame.
__attribute__((noinline)) static void mus_gen(void){
    MusSt *m=&mus; int n=mus_track; if(n<0||n>7){ mus_track=-1; return; }
    const MCfg *c=&mcfg[n]; const Track *t=&tracks[n];
    int s=mus_step&15, lay=xo_i(XO_MUSL), classic=(xo_i(XO_MUSVAR)==0), bal=xo_i(XO_MUSBAL);
    int ob=(opt_music==1);
    if(m->stg){                                            // a stinger over the top
        int i=m->stg_i&15, k=(m->stg-1)&1; int mn=stg_mel[k][i], bn=stg_bas[k][i];
        if(mn>1){ SND(0x08)=(0xC<<12)|(2<<8)|(2<<6); SND(0x0C)=0x8000|sq_freq[mn]; } else if(mn==0) SND(0x08)=0;
        if(bn>1){ SND(0x12)=((ob?2:1)<<13); SND(0x14)=0x8000|wv_freq[bn]; } else if(bn==0) SND(0x12)=0;
        if(++m->stg_i>=16){ m->stg=0; m->mv=2; SND(0x08)=0; SND(0x12)=0; mus_step=0; mus_t=8; return; }
        mus_t=5; return; }
    if(m->fade_out){ if(m->mv>3) m->mv-=3; else { int p=m->pend; m->fade_out=0; m->mv=0;
            if(p<0||p>7){ mus_track=-1; SND(0x08)=0; SND(0x12)=0; return; }
            mus_start(p); m->mv=2; n=mus_track; c=&mcfg[n]; t=&tracks[n]; s=0; } }
    else if(m->mv<16){ m->mv+=2; if(m->mv>16) m->mv=16; }
    if(s==0){                                              // ---- the start of a bar: work out what this bar is
        mus_gather(n,0);
        int kd=mform[m->sec&7]; if(kd==1 && (m->cyc&1)) kd=2; if(classic) kd=1;
        m->kind=(u8)kd;
        int pr = kd==3 ? m->pc : kd==4 ? m->pb : m->pv;
        m->prg = (s8)mprog[pr&7][m->bar&3];
        { u32 bs=mh(m->seed^(u32)(kd*0x1234567), (u32)(m->bar&1)); if((m->bar&3)==3) bs=mh(bs,m->cyc); m->bseed=mh(bs, m->sec); }
        { int grp = kd==3 ? 2 : (kd==2||kd==4) ? 1 : 0; if(kd==3 && (m->bseed&3)==0) grp=1; m->rm=mrhy[grp*4+(int)((m->bseed>>4)&3)]; }
        { int lo = kd==3 ? 2 : kd==4 ? -3 : -2; int d=m->prg; while(d<lo+2) d+=7; while(d>lo+6) d-=7; m->md=(s8)d; }
        m->hide = (u8)(m->kind==1 ? 1 : 0);
    }
    int kd=m->kind, tr12=(((m->key+mkoff[m->cyc&3])%12)+12)%12;
    int tr = m->key+mkoff[m->cyc&3]; tr = ((tr+6)%12+12)%12-6;       // the theme verse is moved by this many notes
    if(classic) tr=0;
    int tempo=t->tempo; if(!classic){ tempo=t->tempo+m->tadj+(m->night?1:0)-(mus_fight?1:0); if(tempo<4) tempo=4; if(tempo>16) tempo=16; }
    int vol=t->vol + (classic?0:mdyn[kd%6]) - (m->low?2:0); if(ob) vol>>=1;
    vol=(vol*m->mv)>>4; if(bal==1) vol+=1; else if(bal==2) vol-=2; if(vol<0) vol=0; if(vol>15) vol=15;
    int bcode = (m->mv==0) ? 0 : (m->mv<6 ? 3 : m->mv<11 ? 2 : (ob?2:1)); if(bal==1 && bcode>0 && bcode<3) bcode++; if(bal==2 && bcode>0) bcode=1;
    int d=m->prg, mn=0, bn=0;
    if(kd==1){                                                     // the theme: note for note, only moved to the world's key
        int ix=(m->bar&3)*16+s; mn=t->mel[ix&63]; bn=t->bas[ix&63];
        if(mn>1){ mn+=tr; if(mn<1) mn=1; if(mn>120) mn=120; } if(bn>1){ bn+=tr; if(bn<1) bn=1; if(bn>110) bn=110; }
    } else {                                                       // made up on the spot from the world's seed
        if((m->rm>>s)&1){
            u32 h=mh(m->bseed,(u32)s); int md=m->md;
            if((s&3)==0){ int x=d+2*(int)(h%3); while(x-md>3) x-=7; while(md-x>3) x+=7; md=x; }
            else md+=mstp[(h>>8)%6];
            { int lo = kd==3 ? 2 : kd==4 ? -3 : -2; if(md>lo+9) md=lo+7; if(md<lo) md=lo+2; }
            m->md=(s8)md; mn=mdeg(md,c,(int)(((m->key+mkoff[m->cyc&3])+6)%12)-6 +0);
        }
        int bs=c->bass; if(kd==0||kd==5) bs=3;
        if(bs==0){ if(s==0) bn=mbassn(d,tr12); else if(s==8 && kd==3) bn=mbassn(d+4,tr12); }
        else if(bs==1){ if(s==0) bn=mbassn(d,tr12); else if(s==8) bn=mbassn(d+4,tr12); }
        else if(bs==2){ if((s&3)==0) bn=mbassn(d,tr12); else if((s&3)==2) bn=mbassn(d,tr12)+12; }
        else { if(s==0) bn=mbassn(d,tr12); }
        if(bn>1) bn=bn<2?2:bn;
    }
    if(mn==0 && kd!=1) { /* a held note rings on */ }
    else if(mn==0) SND(0x08)=0;
    else if(mn>1 && vol>0){ SND(0x08)=(vol<<12)|((kd==1||classic?1:menv[kd%6])<<8)|((kd==1||classic?2:mduty[kd%6])<<6); SND(0x0C)=0x8000|sq_freq[mn&127]; }
    if(bn==0 && kd==1) SND(0x12)=0;
    else if(bn>1 && bcode>0){ SND(0x12)=(bcode<<13); SND(0x14)=0x8000|wv_freq[bn&127]; }
    else if(bcode==0) SND(0x12)=0;
    if(!classic && lay){
        // arpeggio layer on channel 1 (steps the sound effects need are left alone)
        if((lay&2) && !mus_hold1 && kd!=1 && kd!=0 && (kd==3 || c->arp) && !(s&1)){
            int ti=((s>>1)+m->bar)&3; static const u8 ap[4]={0,2,4,2}; int dg=d+ap[ti]+7*((s>>3)&1)*0+7; int an=mdeg(dg,c,tr);
            int av=(5*m->mv)>>4; if(ob) av>>=1; if(av>0){ SND(0x00)=0; SND(0x02)=(av<<12)|(1<<8)|(1<<6); SND(0x04)=0x8000|sq_freq[an&127]; } }
        // drums on channel 4: fights, the boss and the race, and the choruses of some tunes
        int dr = (lay&1) && !mus_hold4 && (mus_fight || c->drum==1 || (c->drum==2 && kd==3) || m->drumsf);
        if(dr){
            int dv; int hit=0;
            if(s==0||s==8){ dv=0xB; SND(0x18)=(((dv*m->mv)>>4)<<12)|(2<<8); SND(0x1C)=0x8000|(7<<4)|4; hit=1; }
            else if(s==4||s==12){ dv=0x9; SND(0x18)=(((dv*m->mv)>>4)<<12)|(1<<8); SND(0x1C)=0x8000|(3<<4)|2; hit=1; }
            else if((s&1)==0 && ((s&2) || n>=4 || mus_fight>200)){ dv=0x5; SND(0x18)=(((dv*m->mv)>>4)<<12)|(1<<8); SND(0x1C)=0x8000|(1<<4)|(1<<3)|1; hit=1; }
            (void)hit;
        } else if((lay&1) && m->low && !mus_hold4 && (((s&7)==0) || ((s&7)==3))){        // low on hearts: a heartbeat
            SND(0x18)=((((s&7)==0?0x9:0x6)*m->mv>>4)<<12)|(3<<8); SND(0x1C)=0x8000|(7<<4)|5; }
        // drips: cave water, rain, the sea
        if((lay&4) && m->amb && !mus_hold1 && !((lay&2) && kd!=1 && kd!=0 && (kd==3 || c->arp)) ){
            u32 h=mh(m->seed+(u32)m->cyc*997u, (u32)(m->bar*16+s)); if((h&63) < (u32)(m->amb*3)){
                int dn=mdeg((int)((h>>8)%7)+14,c,tr); int av=(5*m->mv)>>4; if(av>0){ SND(0x00)=0; SND(0x02)=(av<<12)|(3<<8)|(2<<6); SND(0x04)=0x8000|sq_freq[dn&127]; } } }
    }
    mus_t=tempo;
    if(++s>=16){ s=0;                                          // the next bar
        int bl=classic ? 4 : mbars[mform[m->sec&7]%6];
        if(++m->bar>=bl){ m->bar=0;
            if(classic){ }
            else if(++m->sec>=8){ m->sec=1; m->cyc++; }
        }
    }
    mus_step=s;
}
static void sound_init(void){
    SND(0x24)=0x80;                 // master on
    SND(0x20)=xo_snd();               // all four channels, both speakers, full volume
    SND(0x22)=0x0002;               // chip volume 100%
    // channel 3: a soft bass wave
    SND(0x10)=0x40;                 // write bank 0 while bank 1 plays
    static const u16 wave[8]={0x0123,0x4567,0x89AB,0xCDEF,0xFEDC,0xBA98,0x7654,0x3210};
    for(int i=0;i<8;i++) *(volatile u16*)(0x04000090+i*2)=wave[i];
    SND(0x10)=0x80;                 // play bank 0
    *(volatile u32*)0x03007FFC=(u32)irq_handler;
    REG_DISPSTAT |= 1<<3; REG_IE=1; REG_IME=1;
}
// little noises: 0 bite, 1 hit, 2 pick up, 3 fireball, 4 hurt, 5 stairs, 6 fanfare, 7 coin, 8 splash, 9 monster dies, 10 spark,
// 11 footstep, 12 thud (walls, slams, crashes), 13 crunch (wood breaking)
static void sfx(int n){
    rumble_sfx(n);                      // (the motor does not care whether the sound is on)
    if(n>=0 && n<16){ if((0x14FE>>n)&1) mus_hold1=18; if((0x7F09>>n)&1) mus_hold4=10; if((0x420B>>n)&1) mus_fight=360; }
    if(!opt_sfx) return;
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
        case 11: SND(0x18)=(0x3<<12)|(1<<8); SND(0x1C)=0x8000|(6<<4)|5; break;
        case 12: SND(0x18)=(0xD<<12)|(2<<8); SND(0x1C)=0x8000|(7<<4)|6; SND(0x00)=(2<<4)|(1<<3)|4; SND(0x02)=(0xA<<12)|(2<<8)|(2<<6); SND(0x04)=0x8000|900; break;
        case 13: SND(0x18)=(0xF<<12)|(3<<8); SND(0x1C)=0x8000|(4<<4)|(1<<3)|2; break;
        case 14: SND(0x18)=(0x6<<12)|(2<<8); SND(0x1C)=0x8000|(2<<4)|(1<<3)|1; break;      // a hiss: something winds up to bite
        case 10: SND(0x18)=(0xF<<12)|(1<<8); SND(0x1C)=0x8000|(1<<4)|0; SND(0x00)=(1<<4)|(1<<3)|1; SND(0x02)=(0xE<<12)|(1<<8)|(0<<6); SND(0x04)=0x8000|1980; break;
    }
}
