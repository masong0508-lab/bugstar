// ================================================================= RUMBLE
// The motor of a rumble cartridge (the Drill Dozer kind) hangs off the cartridge's GPIO port: pin 3 of the data byte at 0x080000C4.
// Plain ROM ignores writes there, so on a cartridge or emulator without a motor this does nothing at all.
// It is a small engine: every game event is a PATTERN (a peak strength, a length in frames, a shape and a class), the strength is smoothed into
// a duty cycle (the motor is a slow thing: it is pulsed on and off, 60 times a second) and the whole thing ticks from the vblank interrupt,
// so a long load or a save can never leave the motor stuck on. Events are also filtered by class, so you can keep only the big ones.
//   shapes: 0 flat, 1 dies away (hits), 2 swells (stairs, charging), 3 pulses (fanfares, quakes)   classes: 0 small (pickups, steps), 1 fights, 2 big (hurt, crashes, quakes)
#define GP_DATA (*(volatile u16*)0x080000C4)
#define GP_DIR  (*(volatile u16*)0x080000C6)
#define GP_CTL  (*(volatile u16*)0x080000C8)
typedef struct { u8 amp, len, cls, shape; } RPat;
enum { RP_BITE, RP_HIT, RP_PICK, RP_FIRE, RP_HURT, RP_STAIRS, RP_FANFARE, RP_COIN, RP_SPLASH, RP_DIE, RP_SPARK, RP_STEP, RP_THUD, RP_CRUNCH,
       RP_QUAKE, RP_BOSS, RP_TEST, RP_CRASH, RP_CATCH, NRP };
static const RPat rum_pat[NRP]={
    [RP_BITE]={90,3,1,1},  [RP_HIT]={150,5,1,1},   [RP_PICK]={45,2,0,0},   [RP_FIRE]={110,8,1,1},  [RP_HURT]={255,14,2,1},
    [RP_STAIRS]={120,9,2,2},[RP_FANFARE]={150,30,0,3},[RP_COIN]={35,1,0,0}, [RP_SPLASH]={55,5,0,1}, [RP_DIE]={190,8,1,1},
    [RP_SPARK]={100,3,1,1},[RP_STEP]={28,1,0,0},   [RP_THUD]={220,8,2,1},  [RP_CRUNCH]={140,5,1,1},
    [RP_QUAKE]={210,70,2,3},[RP_BOSS]={255,26,2,1},[RP_TEST]={220,24,2,3}, [RP_CRASH]={255,20,2,1}, [RP_CATCH]={170,22,1,3} };
static const u8 rum_of_sfx[14]={RP_BITE,RP_HIT,RP_PICK,RP_FIRE,RP_HURT,RP_STAIRS,RP_FANFARE,RP_COIN,RP_SPLASH,RP_DIE,RP_SPARK,RP_STEP,RP_THUD,RP_CRUNCH};   // sfx(n) -> pattern
static const u8 rum_pct[6]={0,30,60,100,150,220};           // OFF, LOW, MEDIUM, HIGH, MAX, EXTREME: how strong everything is
static volatile u8 rum_id, rum_len, rum_tot, rum_hold; static u8 rum_acc, rum_on, rum_ready;
static void rumble_stop(void){ rum_len=0; rum_hold=0; rum_acc=0; if(rum_on){ rum_on=0; GP_DATA=0; } }
static void rumble(int id){                  // fire a pattern. A new one takes over only if it is at least as strong as what is left of the old one
    int lvl=xo_i(XO_RUMBLE); if(!lvl || id<0 || id>=NRP) return;
    if(rum_pat[id].cls < xo_i(XO_RUMEV)) return;                      // filtered out: ALL EVENTS (0), FIGHTS AND BIG (1), BIG ONLY (2)
    const RPat *p=&rum_pat[id];
    if(rum_len){ const RPat *q=&rum_pat[rum_id]; if(q->amp*rum_len/(rum_tot?rum_tot:1) > p->amp) return; }
    rum_id=(u8)id; rum_tot=p->len; rum_len=p->len; }
static void rumble_sfx(int n){ if(n>=0 && n<14) rumble(rum_of_sfx[n]); }
static void rum_hum(int amp){ if(xo_i(XO_RUMHUM) && amp>rum_hold) rum_hold=(u8)amp; }   // a steady rumble that lasts while you keep calling it every frame (engines, drifts)
static void rumble_tick(void){                   // once per frame, from the vblank interrupt
    int lvl=xo_i(XO_RUMBLE);
    if(!lvl){ if(rum_on||rum_len||rum_hold) rumble_stop(); return; }
    if(!rum_ready){ rum_ready=1; GP_CTL=1; GP_DIR=8; GP_DATA=0; }       // the port: writable, pin 3 an output
    int out=0;
    if(rum_len){ const RPat *p=&rum_pat[rum_id]; int el=rum_tot-rum_len, e=p->amp;
        if(p->shape==1) e=p->amp*rum_len/rum_tot;
        else if(p->shape==2) e=p->amp*(el+1)/rum_tot;
        else if(p->shape==3) e=((el>>1)&1) ? p->amp/4 : p->amp;
        rum_len--; out=e; }
    if(rum_hold){ if(rum_hold>out) out=rum_hold; rum_hold=0; }
    out=out*rum_pct[lvl]/100; if(out>255) out=255;
    if(out>0) out=64+out*191/255;                 // a motor will not turn on a thin pulse: squeeze the range up out of the dead zone
    int on=0; if(out>0){ int a=rum_acc+out; on=a>=256; rum_acc=(u8)(a&255); } else rum_acc=0;     // a duty cycle: the carry out of the accumulator says when the motor is on
    if(out>=250) on=1;
    if(on!=rum_on){ rum_on=(u8)on; GP_DATA=on?8:0; } }
