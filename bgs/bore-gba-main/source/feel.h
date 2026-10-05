// feel.h - BORE skate "game feel" core (THPS3-GBA style isometric skating). No art, no allocation, integer only.
// Include AFTER the life globals (lfx..lcN) and BEFORE lifeStep(). Units: spd 16 = 1 old lsp unit; angles 256 = 360 deg
// (angF has 4 extra fraction bits); vz keeps the old 8.8 px/step scale. Every tuning number is in the FEEL block below.
//
// FEEL (tune here only)
#define F_TOP     400   // top speed (old lsp 25)
#define F_PUSHDIV 5     // push: spd += (TOP-spd)>>5 + 1  -> ~1.2 s to 90% speed
#define F_COAST   7     // coast: spd -= 1 + spd>>7
#define F_BRAKE   4     // brake: spd -= 5 + spd>>4
#define F_TURN_LO 56    // turn rate at standstill (1/16 unit/frame, ~300 deg/s)
#define F_TURN_HI 28    // ...at top speed (~150 deg/s)
#define F_TURN_ACC 10   // turn rate ramp per frame (steering has weight, no snapping)
#define F_GRIP    2     // velocity chases heading by 1/4 per frame (1<<2): slight slide in turns
#define F_BUF     6     // jump input buffer (frames before landing)
#define F_COY     5     // coyote time (frames after leaving a ledge)
#define F_OLLIE   0x2C0 // ollie base vz (+spd/2): tap = hop, hold = full
#define F_CUT     0x240 // releasing B early caps upward speed here (variable jump height)
#define F_SPIN_MAX 6    // air spin rate (256/360 units per frame)
#define F_WALK    8     // on foot: walk speed (1/256 tile per step, adult; the life stage scales it). Was 5 = 1.2 tiles/s
#define F_RUN     16    // on foot: run speed with B. Was 10
#define F_WACC    2     // on foot: velocity closes 1/F_WACC of the gap to the target each step (was 3): quick start AND quick stop
#define F_WKICK   2     // on foot: from standstill the first step already moves at target/F_WKICK (was 1 unit = under a pixel, felt dead)
#define F_LAND_TOL 36   // land clean within +-36 (~50 deg) of a 180 multiple; +12 while grabbing
static const short sinQ[65]={0,6,13,19,25,31,38,44,50,56,62,68,74,80,86,92,98,104,109,115,121,126,132,137,142,147,152,157,162,167,172,177,181,185,190,194,198,202,206,209,213,216,220,223,226,229,231,234,237,239,241,243,245,247,248,250,251,252,253,254,255,255,256,256,256};
static inline int fsin(int a){ int s=1; a&=255; if(a>=128){ a-=128; s=-1; } if(a>64) a=128-a; return s*sinQ[a]; }
static inline int fcos(int a){ return fsin(a+64); }
static inline int fabsi(int v){ return v<0?-v:v; }
typedef struct { int angF, turn, spd, fvx, fvy, rx, ry, buf, coy, jh, spin, spinV, grab; } Feel;
static Feel F;
static void feelReset(int hd16){ F.angF=hd16<<8; F.turn=F.spd=F.fvx=F.fvy=F.rx=F.ry=F.buf=F.coy=F.jh=F.spin=F.spinV=F.grab=0; }
static void feelSync(void){   // main.c code that zeroes/reduces lsp (bumps, bails, stuns, death) wins over our speed
    if(lsp<(F.spd>>4)){ int o=F.spd; F.spd=lsp<<4; if(o){ F.fvx=F.fvx*F.spd/o; F.fvy=F.fvy*F.spd/o; } if(!F.spd) F.rx=F.ry=0; }
}
static void feelTick(int ongr,int pressB){   // coyote + jump buffer bookkeeping
    if(ongr) F.coy=F_COY; else if(F.coy>0) F.coy--;
    if(F.buf>0) F.buf--;
    if(pressB) F.buf=F_BUF;
}
static void feelSteer(u16 k){
    int dir=((k&K_RIGHT)?1:0)-((k&K_LEFT)?1:0);
    int mx=F_TURN_LO-(F.spd*(F_TURN_LO-F_TURN_HI)>>9), tg=dir*mx, d=tg-F.turn;
    int acc=dir?F_TURN_ACC:F_TURN_ACC+4; if(d>acc) d=acc; if(d<-acc) d=-acc;
    F.turn+=d; F.angF=(F.angF+F.turn)&4095;
}
static void feelPush(u16 k,int rail){
    int top=moodTop(F_TOP);                             // mood: SAD drags the top speed, STOKED adds a touch
    if(k&K_A){ if(F.spd<top) F.spd+=((top-F.spd)>>F_PUSHDIV)+1; }
    else { F.spd-=1+(F.spd>>F_COAST); }
    if(k&K_DOWN) F.spd-=F_BRAKE+1+(F.spd>>4);
    F.spd-=(fabsi(F.turn)*F.spd)>>14;                  // carving costs a little speed
    if(rail&&F.spd<192) F.spd=192;                     // rails keep you rolling
    if(F.spd<0) F.spd=0;
}
static inline int feelOllie(void){ F.jh=1; F.buf=0; F.coy=0; return F_OLLIE+(F.spd>>1); }
static void feelAir(u16 k,u16 pr,int nearGround){      // spin ramps up, A grabs, B flips; variable jump height
    int dir=((k&K_RIGHT)?1:0)-((k&K_LEFT)?1:0);
    if(dir){ F.spinV+=dir; if(F.spinV>F_SPIN_MAX) F.spinV=F_SPIN_MAX; if(F.spinV<-F_SPIN_MAX) F.spinV=-F_SPIN_MAX; }
    else if(F.spinV>0) F.spinV--; else if(F.spinV<0) F.spinV++;
    F.spin+=F.spinV;
    if(k&K_A){ if(F.grab<600) F.grab++; }
    if(F.jh&&lvz>F_CUT&&!(k&K_B)) lvz=F_CUT;           // let go of B early = short hop
    if(pr&K_B){ if(nearGround&&lvz<0) F.buf=F_BUF; else if(!lflip){ lflip=1; lnote=(k&K_UP)?"HEELFLIP":"KICKFLIP"; lnoteT=40; } }
}
static int feelClean(void){ int a=fabsi(F.spin), h=(a+64)>>7, o=fabsi(a-h*128); return o<=F_LAND_TOL+(F.grab>12?12:0); }
static int feelHalfTurns(void){ return (fabsi(F.spin)+64)>>7; }
static int feelGrabPts(void){ int g=F.grab>>2; return g>150?150:g; }
static void feelLandReset(void){ F.spin=F.spinV=F.grab=0; F.jh=0; }
static void feelVel(void){   // velocity chases heading*speed (grip), exact sub-unit movement via remainder
    int a=F.angF>>4, tx=(fcos(a)*F.spd)>>8, ty=(fsin(a)*F.spd)>>8;
    F.fvx+=(tx-F.fvx)/(1<<F_GRIP); F.fvy+=(ty-F.fvy)/(1<<F_GRIP);
    F.rx+=F.fvx; F.ry+=F.fvy; lvx=F.rx/16; lvy=F.ry/16; F.rx-=lvx*16; F.ry-=lvy*16;
    lsp=F.spd>>4; lhd=((a+8)>>4)&15;
}
static inline int wEase(int e){ int s=e/F_WACC; return s?s:(e>0)-(e<0); }   // ease step, never stalls below 1 unit
static void feelWalk(u16 k,u16 pr,int ongr){   // on foot: eased accel instead of instant speed, hop with buffer + coyote
    int ux=((k&K_RIGHT)?1:0)-((k&K_LEFT)?1:0), uy=((k&K_DOWN)?1:0)-((k&K_UP)?1:0);
    int dx=ux+uy, dy=uy-ux, spd=(k&K_B)?F_RUN:F_WALK; spd=spd*stSpd[stage]/100; if(spd<2) spd=2; if(ux&&uy) spd=(spd*3)/4;   // the life stage scales the pace
    int tx=dx*spd, ty=dy*spd;
    if((tx||ty)&&!lvx&&!lvy){ lvx=tx/F_WKICK; lvy=ty/F_WKICK; }   // standing start: visible movement on the very first step
    int ex=tx-lvx, ey=ty-lvy;
    lvx+=wEase(ex); lvy+=wEase(ey);
    lsp=(dx||dy)?spd:0;
    if(dx||dy){ int h=hdT[(dy>0)-(dy<0)+1][(dx>0)-(dx<0)+1]; if(h>=0) lhd=h; }
    if(pr&K_A) F.buf=F_BUF;
    if(F.buf>0&&(ongr||(F.coy>0&&lvz<=0))){ lvz=0x300; F.buf=0; F.coy=0; }
}
