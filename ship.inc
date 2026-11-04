// ================================================================= YOUR SHIP: built from parts, drawn as a 3D wireframe, and the parts change how it flies
typedef struct { char name[9]; u8 hull, wings, wsize, engines, fins, cannon, cockpit, len; u16 paint, trim; u8 wid, nose, wpos, dihed, esize, pods, decal, pad2; u16 glow; } ShipLook;
static ShipLook ship;
// the newer parts live in their own block on the cartridge (slot+0xC40, see save.inc), so every old save still loads: a ship that never saw that block has all of these at 0
typedef struct { u8 scheme, finish, module, topper, pad[4]; } ShipX;
static ShipX shpx;
enum { HL_DART, HL_SAUCER, HL_BOX, HL_WEDGE, HL_NEEDLE, HL_MANTA, HL_TWIN, HL_CAPSULE, NHULL };
enum { WG_NONE, WG_STRAIGHT, WG_SWEPT, WG_DELTA, WG_X, WG_FORWARD, WG_GULL, NWING };
enum { CN_NONE, CN_NOSE, CN_WING, CN_BIG, CN_TWIN, CN_TURRET, CN_QUAD, NCANNON };
enum { CK_BUBBLE, CK_SLIT, CK_DOME, CK_VISOR, CK_NONE, NCOCK };
enum { SD_NONE, SD_STRIPE, SD_LINES, SD_SPIKES, SD_CHEVRON, SD_PANELS, SD_TEETH, NDECAL };
enum { MD_NONE, MD_ARMOUR, MD_RACK, MD_GYRO, MD_DISH, MD_BOOST, MD_COIL, NMODULE };   // MD_COIL: the STORMCLOCK COIL (coil.inc): only when you own it
enum { TP_NONE, TP_ANTENNA, TP_MASTS, TP_BEACON, NTOPPER };
enum { SC_CLASSIC, SC_TWO, SC_INVERT, SC_MONO, NSCHEME };
enum { FN_GLOSS, FN_MATTE, FN_METAL, FN_PEARL, NFINISH };
static const char *const hull_name[NHULL]={"DART","SAUCER","CARGO BOX","WEDGE","NEEDLE","MANTA","TWIN HULL","CAPSULE"};
static const char *const wing_name[NWING]={"NONE","STRAIGHT","SWEPT","DELTA","X-WINGS","FORWARD","GULL"};
static const char *const wsize_name[3]={"SMALL","MEDIUM","LARGE"};
static const char *const fins_name[3]={"NONE","ONE FIN","TWIN FINS"};
static const char *const cannon_name[NCANNON]={"NONE","NOSE GUN","WING GUNS","BIG CANNON","TWIN LASERS","TURRET","QUAD GUNS"};
static const char *const cockpit_name[NCOCK]={"BUBBLE","SLIT","DOME","VISOR","NONE"};
static const char *const len_name[3]={"SHORT","NORMAL","LONG"}, *const wid_name[3]={"SLIM","NORMAL","WIDE"}, *const nose_name[3]={"POINT","BLUNT","FORK"};
static const char *const wpos_name[3]={"FORWARD","MIDDLE","BACK"}, *const dihed_name[3]={"DOWN","FLAT","UP"}, *const esize_name[3]={"SMALL","NORMAL","HUGE"};
static const char *const pods_name[4]={"NONE","MISSILE PODS","SCOOPS","FUEL TANKS"}, *const decal_name[NDECAL]={"NONE","RACING STRIPE","GLOW LINES","SPIKES","CHEVRONS","PANELS","SHARK TEETH"};
static const char *const module_name[NMODULE]={"NONE","ARMOUR PLATES","CARGO RACK","GYRO RING","TARGET DISH","BOOSTERS","STORMCLOCK COIL"}, *const topper_name[NTOPPER]={"NONE","ANTENNA","TWIN MASTS","BEACON"};
static const char *const scheme_name[NSCHEME]={"CLASSIC","TWO-TONE","INVERTED","ONE COLOUR"}, *const finish_name[NFINISH]={"GLOSS","MATTE","METAL","PEARL"};
static void ship_default(void){ scpy(ship.name,"MOTH"); ship.hull=HL_DART; ship.wings=WG_SWEPT; ship.wsize=1; ship.engines=1; ship.fins=1; ship.cannon=1; ship.cockpit=0; ship.paint=rgb15(150,156,176); ship.trim=rgb15(232,80,72);
    ship.len=1; ship.wid=1; ship.nose=0; ship.wpos=1; ship.dihed=1; ship.esize=1; ship.pods=0; ship.decal=0; ship.glow=rgb15(110,200,255); shpx.scheme=shpx.finish=shpx.module=shpx.topper=0; }
// ---- what the parts do
static int ship_mass(void); static int ship_thrust(void);
static int ship_armour(void){ static const u8 a[NHULL]={100,90,70,95,130,110,85,80}; int v=a[ship.hull%NHULL]; if(shpx.module==MD_ARMOUR) v-=15; v-=hull_plates*3; { int d=v-20; if(d>ship_rk[SK_ARMOUR]) d=ship_rk[SK_ARMOUR]; if(d>0) v-=d; } return v; }           // % of damage taken
static int ship_top_speed(void){ int t=30+ship_thrust()*110/ship_mass(), top=90+ship_rk[SK_SPEED]/2; if(t>top) t=top; if(t<28) t=28; return t; }     // (SPEED ranks raise the top too, to 110)
static int ship_turn(void){ static const u8 w[NWING]={75,100,115,130,120,138,108}; static const s8 z[3]={10,0,-10}; int t=w[ship.wings%NWING]+z[ship.wsize%3]; if(shpx.module==MD_GYRO) t+=18; t = t*14/(8+ship_mass()/3); t+=ship_rk[SK_TURN]*2; return t<40?40:t; }   // % turn rate: heavy ships turn slowly
static int ship_damp(void){ return ship.fins==0 ? 3 : ship.fins==1 ? 4 : 6; }                                  // how fast turning settles
static int ship_gun(void){ static const u8 g[NCANNON]={0,2,4,8,5,3,9}; return g[ship.cannon%NCANNON] + (shpx.module==MD_DISH ? 3 : 0) + sp_n[1]*2 + ship_rk[SK_LASER]; }                           // extra laser damage
static int ship_hold_bonus(void){ static const u8 h[NHULL]={0,4,8,0,0,2,10,6}; return h[ship.hull%NHULL] + ship.len*2 + ship.wid*2 + (shpx.module==MD_RACK ? 10 : 0) + ship_rk[SK_HOLD]; }
static int ship_mass(void){ static const u8 hm[NHULL]={10,11,16,9,7,8,15,13}, cm[NCANNON]={0,1,2,3,2,2,5}, mm[NMODULE]={0,4,2,3,1,3,2}; return hm[ship.hull%NHULL] + ship.len*2 + ship.wid*2 + (ship.wings?1+ship.wsize:0) + (ship.engines+1)*(1+ship.esize) + cm[ship.cannon%NCANNON] + (ship.pods?2:0) + mm[shpx.module%NMODULE]; }
static int ship_thrust(void){ static const u8 es[3]={7,10,14}; return (ship.engines+1)*es[ship.esize] + (shpx.module==MD_BOOST ? 8 : 0) + sp_n[0]*4 + ship_rk[SK_SPEED]*2; }
static int ship_fuel_bonus(void){ return (ship.pods==3 ? 30 : 0) + (ship.hull==HL_CAPSULE ? 20 : 0) + ship_rk[SK_FUEL]*2; }                         // fuel tanks and the capsule's big belly: a bigger tank
static int ship_accel16(void){ int a=ship_thrust()*24/ship_mass(); if(a<4) a=4; if(a>96) a=96; return a; }        // per-frame push, in 1/16
// ---- the model: vertices in 1/100 of a cell, x right, y up, z forward. faces are flat-shaded polygons (3-8 corners), edges are drawn dark on top
#define SV_MAX 128
static s16 sh_eng[4][4]; static int sh_en=0, sh_rad=80, sh_flame=0;     // engine rear centres (x, y, z, radius) for the flames; the model's radius; flame level 0 (off) to 3
#define SE_MAX 200
#define SF_MAX 110
EW static s16 shv[SV_MAX*3]; EW static u8 she[SE_MAX*3]; static int snv, sne, snf;
typedef struct { u8 n, grp, two; u8 v[8]; } Face;          // grp 0 paint, 1 trim. two: bit 0 visible from both sides (wings, fins), bit 1 in the front half of the hull (for the two-tone paint scheme)
EW static Face shf[SF_MAX];
EW static s16 shn[SF_MAX*3];                 // per face: unit normal in model space, 256 = 1.0
static s16 sh_mz[2][3]; static int sh_mzn=2;                               // where the guns' muzzles are in the model (x, y, z): the chase view fires the lasers from here
EW static u8 sh_ord[SF_MAX]; static int sh_ord_n=-1;   // the draw order is kept from frame to frame: it barely changes, so re-sorting it is almost free
EW static u32 sh_rt[256]; static int sh_rt_ok=0;       // 65536/i, so lines and polygon edges need no division
EW static s16 sh_l[160], sh_r[160];                    // polygon fill: the left and right end of every row
static void sh_rt_init(void){ sh_rt[0]=0; for(int i=1;i<256;i++) sh_rt[i]=(65536+i/2)/i; sh_rt_ok=1; }
static int sv_add(int x,int y,int z){ if(snv>=SV_MAX) return snv-1; shv[snv*3]=x; shv[snv*3+1]=y; shv[snv*3+2]=z; return snv++; }
static void se_add(int a,int b,int c){ if(sne>=SE_MAX) return; she[sne*3]=a; she[sne*3+1]=b; she[sne*3+2]=c; sne++; }
static void sf_add(int grp,int two,int n,int a,int b,int c,int d,int e,int f,int g,int h){ if(snf>=SF_MAX) return; Face *F=&shf[snf++]; F->n=n; F->grp=grp; F->two=two; int vv[8]={a,b,c,d,e,f,g,h}; for(int i=0;i<8;i++) F->v[i]=vv[i]; }
#define TRI(g,t,a,b,c) sf_add(g,t,3,a,b,c,0,0,0,0,0)
#define QUAD(g,t,a,b,c,d) sf_add(g,t,4,a,b,c,d,0,0,0,0)
static int ring4(int w,int h,int z){ int i=snv; sv_add(w,0,z); sv_add(0,h,z); sv_add(-w,0,z); sv_add(0,-h,z); for(int k=0;k<4;k++) se_add(i+k,i+((k+1)&3),0); return i; }
static int ring8(int r,int z,int c){ int i=snv; for(int k=0;k<8;k++){ int a=k*32; sv_add((cosx(a)*r)>>14, (sinx(a)*r)>>14, z); } for(int k=0;k<8;k++) se_add(i+k,i+((k+1)&7),c); return i; }
static void join(int a,int b,int n,int c){ for(int k=0;k<n;k++) se_add(a+k,b+k,c); }
static void tube(int a,int b,int n,int grp){ for(int k=0;k<n;k++){ int k2=(k+1)%n; QUAD(grp,0,a+k,a+k2,b+k2,b+k); } }      // quads between two rings (front ring a, back ring b)
static void fan(int tip,int r,int n,int grp,int rev){ for(int k=0;k<n;k++){ int k2=(k+1)%n; if(rev) TRI(grp,0,tip,r+k2,r+k); else TRI(grp,0,tip,r+k,r+k2); } }
static int room(int v,int f){ return snv+v<=SV_MAX && snf+f<=SF_MAX && sne+v<=SE_MAX; }      // the model buffers are small (the GBA's work RAM is full), so the newer parts only fit in if there is room
static void sbox(int cx,int cy,int cz,int dx,int dy,int dz,int grp){ int p=snv; for(int k=0;k<8;k++) sv_add(cx+((k&1)?dx:-dx), cy+((k&2)?dy:-dy), cz+((k&4)?dz:-dz));      // a little block: centre and half sizes
    QUAD(grp,0,p+0,p+1,p+3,p+2); QUAD(grp,0,p+4,p+5,p+7,p+6); QUAD(grp,0,p+0,p+1,p+5,p+4); QUAD(grp,0,p+2,p+3,p+7,p+6); QUAD(grp,0,p+0,p+2,p+6,p+4); QUAD(grp,0,p+1,p+3,p+7,p+5); }
static int LX(int v){ static const u8 f[3]={80,100,125}; return v*f[ship.len]/100; }     // the hull stretches with its length
static int WX(int v){ static const u8 f[3]={75,100,130}; return v*f[ship.wid]/100; }     // ...and widens
static void ship_build_model(void){
    snv=0; sne=0; snf=0; sh_en=0; int hw=WX(22), hh=12, tail=LX(-55), nose=LX(70);
    switch(ship.hull){
        case HL_DART: { int r1=ring4(WX(22),12,LX(20)), r2=ring4(WX(18),10,LX(-40)), r3=ring4(WX(8),6,LX(-60)); join(r1,r2,4,0); join(r2,r3,4,0);
            if(ship.nose==0){ int n=sv_add(0,0,nose); for(int k=0;k<4;k++) se_add(n,r1+k,0); fan(n,r1,4,0,0); }
            else if(ship.nose==1){ int r0=ring4(WX(12),7,nose-10); join(r0,r1,4,0); tube(r0,r1,4,0); sf_add(0,0,4,r0,r0+1,r0+2,r0+3,0,0,0,0); }
            else { for(int sx=-1;sx<=1;sx+=2){ int n=sv_add(sx*WX(14),0,nose+6); se_add(n,r1+(sx>0?0:2),0); se_add(n,r1+1,0); se_add(n,r1+3,0); TRI(0,1,n,r1+1,r1+(sx>0?0:2)); TRI(0,1,n,r1+(sx>0?0:2),r1+3); } sf_add(0,1,4,r1,r1+1,r1+2,r1+3,0,0,0,0); }
            tube(r1,r2,4,0); tube(r2,r3,4,0); sf_add(0,0,4,r3+3,r3+2,r3+1,r3,0,0,0,0); hw=WX(22); hh=12; tail=LX(-60); break; }
        case HL_SAUCER: { int r1=ring8(WX(44),0,0), top=sv_add(0,16,0), bot=sv_add(0,-10,0); for(int k=0;k<8;k+=2){ se_add(r1+k,top,0); se_add(r1+k,bot,0); }
            for(int i=0;i<8;i++) if(r1+i<snv){ int yy=shv[(r1+i)*3+1]; shv[(r1+i)*3+1]=0; shv[(r1+i)*3+2]=(s16)LX(yy); }          // lay the ring flat: round in x and z
            fan(top,r1,8,0,1); fan(bot,r1,8,0,0); hw=WX(44); hh=10; tail=LX(-30); nose=LX(46); break; }
        case HL_BOX: { int a=snv; int W=WX(28), B=LX(-50), F=LX(40); sv_add(-W,-14,B); sv_add(W,-14,B); sv_add(W,14,B); sv_add(-W,14,B); sv_add(-W,-14,F); sv_add(W,-14,F); sv_add(W,14,F); sv_add(-W,14,F);
            static const u8 be[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; for(int k=0;k<12;k++) se_add(a+be[k][0],a+be[k][1],0);
            QUAD(0,0,a+0,a+3,a+2,a+1); QUAD(0,0,a+0,a+1,a+5,a+4); QUAD(0,0,a+1,a+2,a+6,a+5); QUAD(0,0,a+2,a+3,a+7,a+6); QUAD(0,0,a+3,a+0,a+4,a+7);
            if(ship.nose==1){ QUAD(0,0,a+4,a+5,a+6,a+7); } else { int n=sv_add(0,0,LX(62)); for(int k=4;k<8;k++) se_add(n,a+k,0); TRI(0,0,n,a+4,a+5); TRI(0,0,n,a+5,a+6); TRI(0,0,n,a+6,a+7); TRI(0,0,n,a+7,a+4); }
            hw=W; hh=14; tail=B; nose=LX(62); break; }
        case HL_WEDGE: { int n=sv_add(0,0,nose); int a=snv; int W=WX(46), B=LX(-50); sv_add(-W,-6,B); sv_add(W,-6,B); sv_add(W,6,B); sv_add(-W,6,B); for(int k=0;k<4;k++){ se_add(n,a+k,0); se_add(a+k,a+((k+1)&3),0); }
            TRI(0,0,n,a+0,a+1); TRI(0,0,n,a+1,a+2); TRI(0,0,n,a+2,a+3); TRI(0,0,n,a+3,a+0); QUAD(0,0,a+0,a+3,a+2,a+1); hw=WX(20); hh=6; tail=B; break; }
        case HL_MANTA: { int r1=ring4(WX(46),7,LX(12)), r2=ring4(WX(30),11,LX(-24)), r3=ring4(WX(12),6,LX(-52)); int n=sv_add(0,0,LX(70));       // a wide, flat diamond
            join(r1,r2,4,0); join(r2,r3,4,0); fan(n,r1,4,0,0); tube(r1,r2,4,0); tube(r2,r3,4,0); sf_add(0,0,4,r3+3,r3+2,r3+1,r3,0,0,0,0); hw=WX(30); hh=7; tail=LX(-52); nose=LX(70); break; }
        case HL_TWIN: { int X0=WX(26);                                                                                                            // two slim hulls and a bridge between them
            for(int sx=-1;sx<=1;sx+=2){ int a1=ring4(7,9,LX(34)), a2=ring4(8,10,LX(-30)), a3=ring4(6,7,LX(-58)); for(int i=0;i<12;i++) shv[(a1+i)*3]+=sx*X0;
                int n=sv_add(sx*X0,0,LX(70)); fan(n,a1,4,0,0); tube(a1,a2,4,0); tube(a2,a3,4,0); sf_add(0,0,4,a3+3,a3+2,a3+1,a3,0,0,0,0); }
            sbox(0,0,LX(18),X0,3,LX(10),1); sbox(0,0,LX(-32),X0,3,LX(20),1); hw=X0+8; hh=4; tail=LX(-58); nose=LX(36); break; }
        case HL_CAPSULE: { int n=sv_add(0,0,LX(72)); int c1=ring8(12,LX(44),0), c2=ring8(17,LX(10),0), c3=ring8(16,LX(-26),0), c4=ring8(10,LX(-52),0);        // a pill: rounded all the way round
            for(int i=0;i<32;i++) if(c1+i<snv) shv[(c1+i)*3]=(s16)WX(shv[(c1+i)*3]);
            fan(n,c1,8,0,0); tube(c1,c2,8,0); tube(c2,c3,8,0); tube(c3,c4,8,0); sf_add(0,0,8,c4+7,c4+6,c4+5,c4+4,c4+3,c4+2,c4+1,c4); hw=WX(17); hh=16; tail=LX(-52); nose=LX(72); break; }
        default: { int n=sv_add(0,0,LX(92)); int r1=ring4(WX(10),10,LX(10)), r2=ring4(WX(7),7,LX(-70)); for(int k=0;k<4;k++) se_add(n,r1+k,0); join(r1,r2,4,0);
            fan(n,r1,4,0,0); tube(r1,r2,4,0); sf_add(0,0,4,r2+3,r2+2,r2+1,r2,0,0,0,0); hw=WX(10); hh=10; tail=LX(-70); nose=LX(92); break; }
    }
    int hullv=snv, zmid; { int lo=9999, hi=-9999; for(int i=0;i<hullv;i++){ int z=shv[i*3+2]; if(z<lo) lo=z; if(z>hi) hi=z; } zmid=(lo+hi)/2; }   // the middle of the hull, for the two-tone paint
    int s = 22+ship.wsize*16, wz = ship.wpos==0 ? 25 : ship.wpos==1 ? 0 : -20, dy = (ship.dihed-1)*s/3;
    if(ship.wings==WG_X){ for(int q=0;q<4;q++){ int sx=(q&1)?1:-1, sy=(q&2)?1:-1; int a=sv_add(sx*hw/2,sy*hh/2,wz), b=sv_add(sx*(hw/2+s),sy*(hh/2+s),wz-25), c=sv_add(sx*hw/2,sy*hh/2,wz-30); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); } }
    else if(ship.wings){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,0,wz+10), b=sv_add(sx*hw,0,wz-35), c, d;
            if(ship.wings==WG_STRAIGHT){ c=sv_add(sx*(hw+s),dy,wz-35); d=sv_add(sx*(hw+s),dy,wz); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); QUAD(1,1,a,d,c,b); }
            else if(ship.wings==WG_SWEPT){ c=sv_add(sx*(hw+s),dy,wz-35-s/2); d=sv_add(sx*(hw+s),dy,wz-15-s/2); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); QUAD(1,1,a,d,c,b); }
            else if(ship.wings==WG_FORWARD){ int sh=s/2+12; c=sv_add(sx*(hw+s),dy,wz-20+sh); d=sv_add(sx*(hw+s),dy,wz+10+sh); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); QUAD(1,1,a,d,c,b); }
            else if(ship.wings==WG_GULL){ int m1=sv_add(sx*(hw+s/2),dy+s/3,wz-5-s/6), m2=sv_add(sx*(hw+s/2),dy+s/3,wz-38-s/6); c=sv_add(sx*(hw+s),dy-s/6,wz-30-s/3); d=sv_add(sx*(hw+s),dy-s/6,wz-8-s/3);
                QUAD(1,1,a,m1,m2,b); QUAD(1,1,m1,d,c,m2); }
            else { c=sv_add(sx*(hw+s),dy,wz-45); se_add(a,c,1); se_add(c,b,1); TRI(1,1,a,c,b); }
            if(ship.pods==1){ int px0=sx*(hw+s*2/3); int p=snv; sv_add(px0-4,dy-3,wz-30); sv_add(px0+4,dy-3,wz-30); sv_add(px0+4,dy-3,wz+2); sv_add(px0-4,dy-3,wz+2); sv_add(px0-4,dy-9,wz-30); sv_add(px0+4,dy-9,wz-30); sv_add(px0+4,dy-9,wz+2); sv_add(px0-4,dy-9,wz+2);
                QUAD(1,0,p+0,p+1,p+2,p+3); QUAD(1,0,p+4,p+7,p+6,p+5); QUAD(1,0,p+0,p+4,p+5,p+1); QUAD(1,0,p+2,p+6,p+7,p+3); QUAD(1,0,p+1,p+5,p+6,p+2); QUAD(1,0,p+3,p+7,p+4,p+0); for(int k=0;k<4;k++){ se_add(p+k,p+((k+1)&3),1); se_add(p+4+k,p+4+((k+1)&3),1); } } } }
    if(ship.pods==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,hh/2,20), b=sv_add(sx*(hw+10),0,0), c=sv_add(sx*hw,-hh/2,-20); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); } }
    if(ship.pods==3){ for(int sx=-1;sx<=1;sx+=2){ int r=ring4(7,7,10), r2=ring4(7,7,-40); for(int i=0;i<8;i++) if(r+i<snv) shv[(r+i)*3]+=sx*(hw+8); join(r,r2,4,1); tube(r,r2,4,1); sf_add(1,0,4,r,r+1,r+2,r+3,0,0,0,0); sf_add(1,0,4,r2+3,r2+2,r2+1,r2,0,0,0,0); } }
    { static const u8 er[3]={5,7,10}; int R=er[ship.esize];
      for(int k=0;k<=ship.engines;k++){ int n=ship.engines+1; int ex = n==1?0 : n==2 ? (k?hw*3/4:-hw*3/4) : n==3 ? (k==0?-hw:k==1?0:hw) : (k==0?-hw:k==1?-hw/3:k==2?hw/3:hw); int ey = (n==3 && k==1) ? hh/2 : 0;
        if(k<4){ sh_eng[k][0]=(s16)ex; sh_eng[k][1]=(s16)ey; sh_eng[k][2]=(s16)(tail-10-R); sh_eng[k][3]=(s16)R; sh_en=k+1; }
        int r=ring8(R,tail,1), r2=ring8(R*5/7,tail-10-R,1); join(r,r2,8,1); for(int i=0;i<16;i++){ if(r+i<snv){ shv[(r+i)*3]+=ex; shv[(r+i)*3+1]+=ey; } }
        tube(r,r2,8,1); sf_add(2,0,8,r2+7,r2+6,r2+5,r2+4,r2+3,r2+2,r2+1,r2); } }
    if(ship.fins==1){ int a=sv_add(0,hh,LX(-15)), b=sv_add(0,hh+26,LX(-45)), c=sv_add(0,hh,LX(-48)); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); }
    else if(ship.fins==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw*3/4,hh/2,LX(-15)), b=sv_add(sx*hw,hh/2+22,LX(-45)), c=sv_add(sx*hw*3/4,hh/2,LX(-48)); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); } }
    sh_mzn=2; sh_mz[0][0]=(s16)(-hw/2); sh_mz[1][0]=(s16)(hw/2); sh_mz[0][1]=sh_mz[1][1]=0; sh_mz[0][2]=sh_mz[1][2]=(s16)nose;       // no cannon: a pair of bolts from the nose
    if(ship.cannon==CN_NOSE){ int a=sv_add(0,-hh/2,nose-10), b=sv_add(0,-hh/2,nose+22); se_add(a,b,5); sh_mzn=1; sh_mz[0][0]=0; sh_mz[0][1]=(s16)(-hh/2); sh_mz[0][2]=(s16)(nose+22); }
    else if(ship.cannon==CN_WING){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*(hw+s/2),dy-4,wz-20), b=sv_add(sx*(hw+s/2),dy-4,wz+30); se_add(a,b,5); sh_mz[sx>0][0]=(s16)(sx*(hw+s/2)); sh_mz[sx>0][1]=(s16)(dy-4); sh_mz[sx>0][2]=(s16)(wz+30); } }
    else if(ship.cannon==CN_BIG){ sh_mz[0][0]=-6; sh_mz[1][0]=6; sh_mz[0][1]=sh_mz[1][1]=(s16)(-hh-4); sh_mz[0][2]=sh_mz[1][2]=(s16)(nose+10); int a=sv_add(-6,-hh-4,-20), b=sv_add(-6,-hh-4,nose+10), c=sv_add(6,-hh-4,-20), d=sv_add(6,-hh-4,nose+10); se_add(a,b,1); se_add(c,d,1); se_add(b,d,1); se_add(a,c,1); QUAD(1,1,a,b,d,c); }
    else if((ship.cannon==CN_TWIN || ship.cannon==CN_QUAD) && room(8,0)){ int gx=hw/3+3; for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*gx,-hh/3,nose-16), b=sv_add(sx*gx,-hh/3,nose+16); se_add(a,b,5); sh_mz[sx>0][0]=(s16)(sx*gx); sh_mz[sx>0][1]=(s16)(-hh/3); sh_mz[sx>0][2]=(s16)(nose+16); }
        if(ship.cannon==CN_QUAD){ for(int sx=-1;sx<=1;sx+=2){ int gx2=hw+4, a=sv_add(sx*gx2,-hh/3,wz-22), b=sv_add(sx*gx2,-hh/3,wz+30); se_add(a,b,5); sh_mz[sx>0][0]=(s16)(sx*gx2); sh_mz[sx>0][1]=(s16)(-hh/3); sh_mz[sx>0][2]=(s16)(wz+30); } } }   // quad: the side guns fire the bolts
    else if(ship.cannon==CN_TURRET && room(10,6)){ sbox(0,hh+3,LX(-18),5,3,LX(8),1); int a=sv_add(0,hh+4,LX(-14)), b=sv_add(0,hh+4,LX(22)); se_add(a,b,5); sh_mzn=1; sh_mz[0][0]=0; sh_mz[0][1]=(s16)(hh+4); sh_mz[0][2]=(s16)LX(22); }
    if(ship.cockpit==CK_BUBBLE){ int a=sv_add(0,hh,LX(20)), b=sv_add(0,hh+9,LX(10)), c=sv_add(0,hh,0); se_add(a,b,2); se_add(b,c,2); TRI(3,1,a,b,c); }
    else if(ship.cockpit==CK_SLIT){ int a=sv_add(-hw/2,hh-1,LX(20)), b=sv_add(hw/2,hh-1,LX(20)); se_add(a,b,6); }
    else if(ship.cockpit==CK_DOME && room(5,4)){ int b0=sv_add(-hw/2,hh,LX(26)), b1=sv_add(hw/2,hh,LX(26)), b2=sv_add(hw/2,hh,LX(-4)), b3=sv_add(-hw/2,hh,LX(-4)), ap=sv_add(0,hh+13,LX(10)); TRI(3,1,b0,b1,ap); TRI(3,1,b1,b2,ap); TRI(3,1,b2,b3,ap); TRI(3,1,b3,b0,ap); }
    else if(ship.cockpit==CK_VISOR && room(4,1)){ int a=sv_add(-hw*2/3,hh-1,LX(30)), b=sv_add(hw*2/3,hh-1,LX(30)), c=sv_add(hw/3,hh+5,LX(12)), d=sv_add(-hw/3,hh+5,LX(12)); QUAD(3,1,a,b,c,d); }
    if(room(16,0)){ int nl=1, lx0=0, ly=hh+1, zs=nose-8; if(ship.hull==HL_TWIN){ nl=2; lx0=WX(26); ly=11; zs=LX(62); }          // the twin hull has a lane on each hull
      for(int ln=0;ln<nl;ln++){ int bx = nl==1 ? 0 : (ln ? lx0 : -lx0), cw=hw/2+2; if(cw>12) cw=12;
        if(ship.decal==SD_STRIPE){ int a=sv_add(bx,ly,zs), b=sv_add(bx,ly,tail+4); se_add(a,b,5); int c=sv_add(bx-4,ly,zs-4), d=sv_add(bx-4,ly,tail+4); se_add(c,d,5); }
        else if(ship.decal==SD_SPIKES){ for(int k=0;k<3;k++){ int z=tail+20+k*LX(22); int a=sv_add(bx,ly-1,z), b=sv_add(bx,ly+9,z-6); se_add(a,b,5); } }
        else if(ship.decal==SD_CHEVRON){ for(int k=0;k<3;k++){ int z0=zs-4-k*LX(14); int ap=sv_add(bx,ly,z0), l=sv_add(bx-cw,ly,z0-9), r=sv_add(bx+cw,ly,z0-9); se_add(ap,l,5); se_add(ap,r,5); } }
        else if(ship.decal==SD_PANELS){ for(int k=0;k<4;k++){ int z=zs-6-k*LX(14); int a=sv_add(bx-cw,ly,z), b=sv_add(bx+cw,ly,z); se_add(a,b,4); } int a=sv_add(bx,ly,zs), b=sv_add(bx,ly,tail+4); se_add(a,b,4); }
        else if(ship.decal==SD_TEETH){ int pv=-1; for(int k=0;k<7;k++){ int v=sv_add(bx-cw+k*(2*cw)/6, ly, zs-(k&1?12:3)); if(pv>=0) se_add(pv,v,5); pv=v; } } }
      if(ship.decal==SD_LINES){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,2,nose-15), b=sv_add(sx*hw,2,tail+6); se_add(a,b,6); } } }
    if(shpx.module==MD_ARMOUR && room(16,12)){ int ph=hh*2/3; if(ph<5) ph=5; for(int sx=-1;sx<=1;sx+=2) sbox(sx*(hw+3),0,tail+LX(34),3,ph,LX(14),1); }                      // armour plates on both flanks
    else if(shpx.module==MD_RACK && room(8,6)){ sbox(0,hh+2,tail+LX(34),WX(11),3,LX(15),1); }                                                                         // a cargo rack on the back
    else if((shpx.module==MD_GYRO||shpx.module==MD_COIL) && room(20,8)){ int Ro=(hw>hh?hw:hh)+(shpx.module==MD_COIL?18:12); if(Ro>64) Ro=64; int Ri=Ro-5, o=snv; for(int k=0;k<8;k++){ int a=k*32; sv_add((cosx(a)*Ro)>>14,(sinx(a)*Ro)>>14,LX(-8)); }
        int in=snv; for(int k=0;k<8;k++){ int a=k*32; sv_add((cosx(a)*Ri)>>14,(sinx(a)*Ri)>>14,LX(-8)); } for(int k=0;k<8;k++) QUAD(2,1,o+k,o+((k+1)&7),in+((k+1)&7),in+k);
        for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*(hw>Ri-2?Ri-2:hw),0,LX(-8)), b=sv_add(sx*Ri,0,LX(-8)); se_add(a,b,5); } }                                  // a glowing gyro ring round the hull
    else if(shpx.module==MD_DISH && room(10,1)){ int cz=LX(-10), cy=hh+12, v[6]; int a0=sv_add(0,hh,cz), a1=sv_add(0,cy,cz); se_add(a0,a1,5);
        for(int k=0;k<6;k++){ int a=k*43; v[k]=sv_add((cosx(a)*9)>>14, cy+(((sinx(a)*9)>>14)*7/10), cz-(((sinx(a)*9)>>14)*7/10)); } sf_add(1,1,6,v[0],v[1],v[2],v[3],v[4],v[5],0,0); }   // a tilted dish on a mast
    else if(shpx.module==MD_BOOST && room(24,10)){ for(int sx=-1;sx<=1;sx+=2){ int bx=sx*(hw/2>6?hw/2:6), by=hh+3; int r1=ring4(4,4,tail+LX(34)), r2=ring4(4,4,tail-4); for(int i=0;i<8;i++){ shv[(r1+i)*3]+=bx; shv[(r1+i)*3+1]+=by; }
        tube(r1,r2,4,1); sf_add(2,0,4,r2+3,r2+2,r2+1,r2,0,0,0,0); if(sh_en<4){ sh_eng[sh_en][0]=(s16)bx; sh_eng[sh_en][1]=(s16)by; sh_eng[sh_en][2]=(s16)(tail-6); sh_eng[sh_en][3]=4; sh_en++; } } }          // two booster nacelles with flames
    if(shpx.topper==TP_ANTENNA && room(4,0)){ int a=sv_add(0,hh,LX(-28)), b=sv_add(0,hh+26,LX(-34)), c=sv_add(-3,hh+22,LX(-34)), d=sv_add(3,hh+22,LX(-34)); se_add(a,b,5); se_add(c,d,5); }
    else if(shpx.topper==TP_MASTS && room(8,0)){ for(int sx=-1;sx<=1;sx+=2){ int mh=sx>0?30:20, mx=sx*(hw/3+2); int a=sv_add(mx,hh,LX(-30)), b=sv_add(mx,hh+mh,LX(-34)), c=sv_add(mx-3,hh+mh-4,LX(-34)), d=sv_add(mx+3,hh+mh-4,LX(-34)); se_add(a,b,5); se_add(c,d,5); } }
    else if(shpx.topper==TP_BEACON && room(8,8)){ int z=LX(-26), y=hh+10; int st=sv_add(0,hh,z), e0=sv_add(4,y,z), e1=sv_add(-4,y,z), e2=sv_add(0,y+4,z), e3=sv_add(0,y-4,z), e4=sv_add(0,y,z+4), e5=sv_add(0,y,z-4); se_add(st,e3,5);
        TRI(3,1,e2,e0,e4); TRI(3,1,e2,e4,e1); TRI(3,1,e2,e1,e5); TRI(3,1,e2,e5,e0); TRI(3,1,e3,e4,e0); TRI(3,1,e3,e1,e4); TRI(3,1,e3,e5,e1); TRI(3,1,e3,e0,e5); }
    for(int f=0;f<snf;f++){ Face *F=&shf[f]; int z=0; for(int k=0;k<F->n;k++) z+=shv[F->v[k]*3+2]; if(z/F->n > zmid) F->two|=2; }      // front half or back half (the two-tone paint)
    for(int f=0;f<snf;f++){                                                       // each face's unit normal (256 = 1.0), worked out once here instead of every frame
        const Face *F=&shf[f]; const s16 *A=&shv[F->v[0]*3], *B=&shv[F->v[1]*3], *C=&shv[F->v[2]*3];
        s32 ux=B[0]-A[0], uy=B[1]-A[1], uz=B[2]-A[2], vx=C[0]-A[0], vy=C[1]-A[1], vz=C[2]-A[2];
        s32 nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        s32 len=isqrt((u32)((nx>>4)*(nx>>4)+(ny>>4)*(ny>>4)+(nz>>4)*(nz>>4)))<<4;
        if(len<1){ shn[f*3]=shn[f*3+1]=shn[f*3+2]=0; } else { shn[f*3]=(s16)(nx*256/len); shn[f*3+1]=(s16)(ny*256/len); shn[f*3+2]=(s16)(nz*256/len); }
    }
    { s32 m=1; for(int i=0;i<snv;i++){ s32 x=shv[i*3], y=shv[i*3+1], z=shv[i*3+2], d2=x*x+y*y+z*z; if(d2>m) m=d2; } sh_rad=isqrt((u32)m); if(sh_rad<20) sh_rad=20; }     // how big it is: the hangar zooms to fit
    sh_ord_n=-1;                                                                  // the face order is rebuilt on the next draw
}
// ---- drawing it: solid, lit, with dark outlines. paint shades live in BG palette 200..207, trim in 208..215
#define SHP_PAINT 200
#define SHP_TRIM 208
#define SHP_GLOW 216
static u16 sh_mix(u16 c, int r2, int g2, int b2, int w){ int r=c&31, g=(c>>5)&31, b=(c>>10)&31; r+=(r2-r)*w/16; g+=(g2-g)*w/16; b+=(b2-b)*w/16; return (u16)(r|(g<<5)|(b<<10)); }
static u16 sh_lit(u16 c, int boost, int w, int r2, int g2, int b2){                // a lit shade: brighten the paint's OWN colour (never past the brightest channel hitting full, so the hue holds), then a small tinted glint on top
    int r=c&31, g=(c>>5)&31, b=(c>>10)&31, m=r>g?r:g; if(b>m) m=b; int k=boost; if(m>0 && m*k>31*16) k=31*16/m; if(k<16) k=16;
    r=r*k/16; g=g*k/16; b=b*k/16; r+=(r2-r)*w/16; g+=(g2-g)*w/16; b+=(b2-b)*w/16; if(r>31) r=31; if(g>31) g=31; if(b>31) b=31; return (u16)(r|(g<<5)|(b<<10)); }
static u16 sh_tone(u16 c, int k){                                                  // 0 deep cool shadow ... 4 the colour itself ... 7 the brightest lit shade. The lit end keeps the paint's colour: a blue hull gets a lighter blue, not cream
    static const u8 bo[4][3]={{18,21,24},{17,18,20},{18,20,22},{17,19,21}}, gw[4][3]={{1,2,4},{0,0,1},{2,5,9},{3,5,8}};   // per finish (gloss, matte, metal, pearl): how much brighter, how much glint
    if(shpx.finish==FN_MATTE){ if(k<4) return sh_mix(shade15(c,7+k*3),3,4,10,4-k); if(k==4) return c; return sh_lit(c,bo[1][k-5],gw[1][k-5],31,30,27); }       // soft: shallow shadows, hardly a glint
    if(shpx.finish==FN_METAL){ if(k<4) return sh_mix(shade15(c,3+k*3),2,4,12,(4-k)*2); if(k==4) return c; return sh_lit(c,bo[2][k-5],gw[2][k-5],31,31,31); }   // deep shadows, a white-ish glint on the top shade
    if(shpx.finish==FN_PEARL){ if(k<4) return sh_mix(shade15(c,8+k*2),14,6,20,(4-k)*3/2+2); if(k==4) return lift15(c,3); return sh_lit(c,bo[3][k-5],gw[3][k-5],22,31,31); }   // violet shadows, a cyan sheen
    if(k<4) return sh_mix(shade15(c,5+k*3),3,4,10,(4-k)*3/2); if(k==4) return c; return sh_lit(c,bo[0][k-5],gw[0][k-5],31,30,27); }
static void ship_ramps(void){ for(int k=0;k<8;k++){ BG_PAL[SHP_PAINT+k] = sh_tone(ship.paint,k); BG_PAL[SHP_TRIM+k] = sh_tone(ship.trim,k); BG_PAL[SHP_GLOW+k] = k<3 ? shade15(ship.glow,10+k*2) : lift15(ship.glow,(k-3)*3); } }static const u8 *sh_occ=0;                                                     // on a planet: the terrain's top row per column (every 8th byte), so hills and canyon walls hide what is behind them. 0 = nothing hides it
static void linep(volatile u16 *pg, int x0, int y0, int x1, int y1, int c, int cx0, int cy0, int cx1, int cy1){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(n>300) return;
    if(m7_on){ s32 fx=(x0<<16)+0x8000, fy=(y0<<16)+0x8000, sx=n?(dx<<16)/n:0, sy=n?(dy<<16)/n:0;         // (a kart race: each pixel finds its own way to the screen)
        for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1) m7_pix(x,y,c); fx+=sx; fy+=sy; } return; }
    if(!n){ if(x0>=cx0&&x0<cx1&&y0>=cy0&&y0<cy1) pix(pg,x0,y0,c); return; }
    s32 sx, sy; if(n<256){ sx=dx*(s32)sh_rt[n]; sy=dy*(s32)sh_rt[n]; } else { sx=(dx<<16)/n; sy=(dy<<16)/n; }
    s32 fx=(x0<<16)+0x8000, fy=(y0<<16)+0x8000; u16 lo=(u16)c, hi=(u16)(c<<8);
    if(sh_occ){ for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1 && sh_occ[(x>>1)<<3]>y){ volatile u16 *p=pg+((y*240+x)>>1); if(x&1) *p=(*p&0x00FF)|hi; else *p=(*p&0xFF00)|lo; } fx+=sx; fy+=sy; } return; }   // (on a planet: not through a hill)
    if(x0>=cx0&&x0<cx1&&x1>=cx0&&x1<cx1&&y0>=cy0&&y0<cy1&&y1>=cy0&&y1<cy1){        // both ends inside: no clipping per pixel
        for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; volatile u16 *p=pg+((y*240+x)>>1); if(x&1) *p=(*p&0x00FF)|hi; else *p=(*p&0xFF00)|lo; fx+=sx; fy+=sy; }
        return; }
    for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1){ volatile u16 *p=pg+((y*240+x)>>1); if(x&1) *p=(*p&0x00FF)|hi; else *p=(*p&0xFF00)|lo; } fx+=sx; fy+=sy; }
}
static int sh_df=0;                                                               // ship faces: 0..3 = how far from palette entry c towards c+1 (a 2x2 ordered dither). 0 for everything else
static void hspan(volatile u16 *pg, int y, int x0, int x1, int c){                 // a row of pixels, x0..x1 inclusive, written two at a time
    if(m7_on){ m7_span(y,x0,x1,c+(sh_df>>1)); return; }
    volatile u16 *row=pg+y*120; u16 cc;
    if(!sh_df) cc=(u16)(c|(c<<8)); else { int f=sh_df, t0=(y&1)?3:0, t1=(y&1)?1:2; cc=(u16)((c+(f>t0)) | ((c+(f>t1))<<8)); }
    if(x0&1){ volatile u16 *p=row+(x0>>1); *p=(*p&0x00FF)|(cc&0xFF00); x0++; }
    if(x1>=x0 && !(x1&1)){ volatile u16 *p=row+(x1>>1); *p=(*p&0xFF00)|(cc&0x00FF); x1--; }
    for(int k=x0>>1, e=x1>>1; k<=e && x0<=x1; k++) row[k]=cc;
}
#ifdef TEST_FPS
static u32 dbg_pc=0, dbg_rows=0, dbg_px=0, dbg_pt=0, dbg_kt=0, dbg_kn=0;
#endif
static void polyfill_(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1);
static void polyfill(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1){
#ifdef TEST_FPS
    u32 t0=*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16); polyfill_(pg,px,py,n,c,cx0,cy0,cx1,cy1); dbg_pt+=(*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16))-t0;
}
static void polyfill_(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1){
#endif
    int ymin=py[0], ymax=py[0]; for(int i=1;i<n;i++){ if(py[i]<ymin) ymin=py[i]; if(py[i]>ymax) ymax=py[i]; }
    if(ymin<cy0) ymin=cy0; if(ymax>=cy1) ymax=cy1-1; if(ymin>ymax) return;
    for(int y=ymin;y<=ymax;y++){ sh_l[y]=32767; sh_r[y]=-32768; }
    for(int i=0;i<n;i++){ int j=i+1; if(j==n) j=0; int x0=px[i], y0=py[i], x1=px[j], y1=py[j];      // walk each edge once, noting where it crosses every row
        if(y0==y1){ if(y0>=ymin && y0<=ymax){ int a=x0<x1?x0:x1, b=x0<x1?x1:x0; if(a<sh_l[y0]) sh_l[y0]=a; if(b>sh_r[y0]) sh_r[y0]=b; } continue; }
        if(y0>y1){ int t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
        int ys=y0<ymin?ymin:y0, ye=y1>ymax?ymax:y1; if(ys>ye) continue;
        s32 dy=y1-y0, dx=x1-x0, sl = dy<256 ? dx*(s32)sh_rt[dy] : (dx<<16)/dy;
        s32 x=(x0<<16)+0x8000+sl*(ys-y0);
        for(int y=ys;y<=ye;y++){ int xi=x>>16; if(xi<sh_l[y]) sh_l[y]=xi; if(xi>sh_r[y]) sh_r[y]=xi; x+=sl; } }
#ifdef TEST_FPS
    dbg_pc++;
#endif
    for(int y=ymin;y<=ymax;y++){ int xl=sh_l[y], xr=sh_r[y]; if(xl>xr) continue; if(xl<cx0) xl=cx0; if(xr>=cx1) xr=cx1-1; if(xl>xr) continue;
#ifdef TEST_FPS
        dbg_rows++; dbg_px+=xr-xl+1;
#endif
        if(sh_occ){ int x=xl;                                                       // on a planet: columns a nearer hill covers from this row down are skipped
            while(x<=xr){ while(x<=xr && sh_occ[(x>>1)<<3]<=y) x++; if(x>xr) break; int s0=x; while(x<=xr && sh_occ[(x>>1)<<3]>y) x++; hspan(pg,y,s0,x-1,c); }
            continue; }
        hspan(pg,y,xl,xr,c); }
}
static int ship_roll=0;                                                        // extra bank applied on screen (the chase camera)
static s32 shp_c[10];                                                       // this draw's turn: yaw cos/sin, pitch cos/sin, roll cos/sin, rolled?, centre x, scale, centre y
static void sh_pt(int x, int y, int z, int *X, int *Y, int *Z){                // one model point to the screen (the same maths as the vertex loop in ship_draw)
    s32 rx=(x*shp_c[0] + z*shp_c[1])>>14, rz=(-x*shp_c[1] + z*shp_c[0])>>14; s32 ry=(y*shp_c[2] + rz*shp_c[3])>>14; rz=(-y*shp_c[3] + rz*shp_c[2])>>14;
    s32 d=400+rz; if(d<60) d=60; s32 f=(shp_c[8]*25600)/d, xx=(rx*f)>>14, yy=-((ry*f)>>14);
    if(shp_c[6]){ s32 x2=(xx*shp_c[4]-yy*shp_c[5])>>14, y2=(xx*shp_c[5]+yy*shp_c[4])>>14; xx=x2; yy=y2; }
    xx+=shp_c[7]; yy+=shp_c[9]; if(xx<-2000) xx=-2000; if(xx>2000) xx=2000; if(yy<-2000) yy=-2000; if(yy>2000) yy=2000; *X=xx; *Y=yy; *Z=rz;
}
static void sh_flames(volatile u16 *pg, int early, int cx0, int cy0, int cx1, int cy1){      // a hot flame out of every engine: a crossed pair of cones, white-hot in the middle
    for(int e=0;e<sh_en;e++){ int ex=sh_eng[e][0], ey=sh_eng[e][1], bz=sh_eng[e][2]+2, R=sh_eng[e][3];
        int len=(16+R*4)*(3+sh_flame)/4; len=len*(10+((frames*3+e)&3))/12;
        int bX,bY,bZ,tX,tY,tZ,aX,aY,aZ,cX,cY,cZ,dX,dY,dZ,fX,fY,fZ;
        sh_pt(ex,ey,bz,&bX,&bY,&bZ); sh_pt(ex,ey,bz-len,&tX,&tY,&tZ);
        if((tZ>=bZ)!=early) continue;                                           // the tip is farther than the nozzle: the hull hides it, so it goes under the faces
        sh_pt(ex+R,ey,bz,&aX,&aY,&aZ); sh_pt(ex-R,ey,bz,&cX,&cY,&cZ); sh_pt(ex,ey+R,bz,&dX,&dY,&dZ); sh_pt(ex,ey-R,bz,&fX,&fY,&fZ);
        int px[3], py[3], oc=SHP_GLOW+4+((frames+e)&1), mX=bX+(tX-bX)*3/5, mY=bY+(tY-bY)*3/5; sh_df=0;
        px[0]=aX; py[0]=aY; px[1]=cX; py[1]=cY; px[2]=tX; py[2]=tY; polyfill(pg,px,py,3,oc,cx0,cy0,cx1,cy1);
        px[0]=dX; py[0]=dY; px[1]=fX; py[1]=fY; polyfill(pg,px,py,3,oc,cx0,cy0,cx1,cy1);
        px[0]=bX+(aX-bX)/2; py[0]=bY+(aY-bY)/2; px[1]=bX+(cX-bX)/2; py[1]=bY+(cY-bY)/2; px[2]=mX; py[2]=mY; polyfill(pg,px,py,3,SHP_GLOW+7,cx0,cy0,cx1,cy1);
        px[0]=bX+(dX-bX)/2; py[0]=bY+(dY-bY)/2; px[1]=bX+(fX-bX)/2; py[1]=bY+(fY-bY)/2; polyfill(pg,px,py,3,SHP_GLOW+7,cx0,cy0,cx1,cy1); }
}
static void ship_draw(volatile u16 *pg, int cx, int cy, int sc, int yaw_, int pitch_, int cpaint, int ctrim, int cx0,int cy0,int cx1,int cy1){
    EW static s16 px[SV_MAX], py[SV_MAX], rz_[SV_MAX]; EW static s16 fz[SF_MAX];
    (void)cpaint; (void)ctrim; if(!sh_rt_ok) sh_rt_init(); ship_ramps(); s32 rc=cosx(ship_roll>>2), rs=sinx(ship_roll>>2); int rolled = !(rs==0 && rc>0);
    s32 cy_=cosx(yaw_>>2), sy_=sinx(yaw_>>2), cp=cosx(pitch_>>2), sp=sinx(pitch_>>2);
    shp_c[0]=cy_; shp_c[1]=sy_; shp_c[2]=cp; shp_c[3]=sp; shp_c[4]=rc; shp_c[5]=rs; shp_c[6]=rolled; shp_c[7]=cx; shp_c[8]=sc; shp_c[9]=cy;
    for(int i=0;i<snv;i++){ s32 x=shv[i*3], y=shv[i*3+1], z=shv[i*3+2];
        s32 rx=(x*cy_ + z*sy_)>>14, rz=(-x*sy_ + z*cy_)>>14;
        s32 ry=(y*cp + rz*sp)>>14; rz=(-y*sp + rz*cp)>>14;                      // (a turn about the sideways axis: a positive pitch_ looks DOWN on the ship from above, so its nose rises up the screen)
        rz_[i]=(s16)rz;
        s32 d = 400 + rz; if(d<60) d=60;
        s32 f=(sc*25600)/d;                                                     // one division per vertex: the scale at this depth
        s32 X=(rx*f)>>14, Y=-((ry*f)>>14);
        if(rolled){ s32 X2=(X*rc - Y*rs)>>14, Y2=(X*rs + Y*rc)>>14; X=X2; Y=Y2; }
        X+=cx; Y+=cy; if(X<-2000) X=-2000; if(X>2000) X=2000; if(Y<-2000) Y=-2000; if(Y>2000) Y=2000; px[i]=(s16)X; py[i]=(s16)Y; }
    if(sh_flame) sh_flames(pg,1,cx0,cy0,cx1,cy1);                               // flames pointing away from you go under the hull
    if(sh_ord_n!=snf){ for(int i=0;i<snf;i++) sh_ord[i]=(u8)i; sh_ord_n=snf; }       // last frame's order is the starting point: almost always a few swaps
    { static const u8 rn[9]={0,0,0,85,64,51,43,37,32};                          // 256/n
      for(int f=0;f<snf;f++){ const Face *F=&shf[f]; int z=0; for(int k=0;k<F->n;k++) z+=rz_[F->v[k]]; fz[f]=(s16)((z*rn[F->n])>>8); } }
    for(int i=1;i<snf;i++){ u8 t=sh_ord[i]; int zt=fz[t], j=i; while(j>0 && fz[sh_ord[j-1]]<zt){ sh_ord[j]=sh_ord[j-1]; j--; } sh_ord[j]=t; }      // far first (a bigger z is farther away), so near faces cover far ones
    for(int o=0;o<snf;o++){ int fi=sh_ord[o]; const Face *F=&shf[fi]; int a=F->v[0], b=F->v[1], c=F->v[2];
        int area=(px[b]-px[a])*(py[c]-py[a]) - (px[c]-px[a])*(py[b]-py[a]);        // facing: screen-space winding
        s32 mx=shn[fi*3], my=shn[fi*3+1], mz=shn[fi*3+2];                          // the stored normal, turned the same way as the ship
        s32 nx=(mx*cy_ + mz*sy_)>>14, nz0=(-mx*sy_ + mz*cy_)>>14;
        s32 ny=(my*cp + nz0*sp)>>14, nz=(-my*sp + nz0*cp)>>14;
        if(nz>0){ nx=-nx; ny=-ny; nz=-nz; }                                        // light the side that faces you, whichever way the stored normal points (a hull seen from above is lit on top, never on its belly)
        s32 dot=(-nx*5 + ny*11 - nz*8)>>8;                                         // light from (-0.35, 0.75, -0.55), scaled by 16
        int X[8], Y[8]; for(int k=0;k<F->n;k++){ X[k]=px[F->v[k]]; Y[k]=py[F->v[k]]; }
        int base, L;                                                               // L: 0..28 = the 8 palette shades with 4 dither steps between each
        if(F->grp==2){ base=SHP_GLOW; L=20+((frames>>2)&1)*8; }                    // engine glow flickers
        else { L=8+dot+(dot>>3); if(dot>9) L+=(dot-9)*2;                          // ambient + light, and a glint where a face points at the light
            L-=fz[fi]>>5;                                                          // far parts a little darker, near parts a little brighter
            if(F->grp==3){ base=SHP_GLOW; L+=6; } else { int g=F->grp; if(shpx.scheme==SC_TWO){ if(!g && (F->two&2)) g=1; } else if(shpx.scheme==SC_INVERT) g^=1; else if(shpx.scheme==SC_MONO) g=0; base = g ? SHP_TRIM : SHP_PAINT; }      // the canopy is glass: lit, never flickering. the paint scheme says which faces wear the trim
            if(L<0) L=0; if(L>28) L=28; }
        int lv=(L+1)>>1, sh=lv>>1; sh_df=(lv&1)?2:0; if(sh>=7){ sh=7; sh_df=0; }          // rounded to 15 steps: a flat shade, or a 50% checkerboard between two
        polyfill(pg,X,Y,F->n,base+sh,cx0,cy0,cx1,cy1); sh_df=0;
        { int ink = F->grp>=2 ? SHP_GLOW+1 : base;                                 // each face is outlined as it is drawn, so a line never shows through the hull
          for(int k=0;k<F->n;k++){ int k2=k+1==F->n?0:k+1; linep(pg,X[k],Y[k],X[k2],Y[k2],ink,cx0,cy0,cx1,cy1); } } }
    for(int i=0;i<sne;i++){ int g=she[i*3+2]; if(!(g&4)) continue; int a=she[i*3], b=she[i*3+1]; if(rz_[a]+rz_[b]>16) continue;      // loose lines (stripes, barrels, slit): only on the side facing you
        g&=3; linep(pg,px[a],py[a],px[b],py[b], g==2?SHP_GLOW+6 : g?SHP_TRIM+5:SHP_PAINT+1, cx0,cy0,cx1,cy1); }
    if(sh_flame) sh_flames(pg,0,cx0,cy0,cx1,cy1);                               // flames pointing at you go over it
}
static void ship_proj(int mx, int my, int mz, int cx, int cy, int sc, int yaw_, int pitch_, int *ox, int *oy){   // where a point of the model lands on the screen (the same maths as ship_draw)
    s32 rc=cosx(ship_roll>>2), rs=sinx(ship_roll>>2), cy_=cosx(yaw_>>2), sy_=sinx(yaw_>>2), cp=cosx(pitch_>>2), sp=sinx(pitch_>>2);
    s32 rx=(mx*cy_ + mz*sy_)>>14, rz=(-mx*sy_ + mz*cy_)>>14, ry=(my*cp + rz*sp)>>14; rz=(-my*sp + rz*cp)>>14;
    s32 d=400+rz; if(d<60) d=60; s32 f=(sc*25600)/d, X=(rx*f)>>14, Y=-((ry*f)>>14);
    if(!(rs==0 && rc>0)){ s32 X2=(X*rc - Y*rs)>>14, Y2=(X*rs + Y*rc)>>14; X=X2; Y=Y2; }
    *ox=(int)(X+cx); *oy=(int)(Y+cy);
}
// ---- ready-made ships (the hangar's PRESET row). They set every part and colour but keep your ship's name
#define RGBC(r,g,b) (u16)((((r)>>3)&31)|((((g)>>3)&31)<<5)|((((b)>>3)&31)<<10))
typedef struct { const char *nm; u8 f[19]; u16 paint, trim, glow; } ShipPre;     // f: hull, wings, wing size, engines, fins, cannon, cockpit, length, width, nose, wing spot, wing tilt, engine size, pods, decal, scheme, finish, module, topper
#define NSHP 10
static const ShipPre ship_pre[NSHP]={
    {"MOTH",   {HL_DART,   WG_SWEPT,   1,1,1,CN_NOSE,  CK_BUBBLE,1,1,0,1,1,1,0,SD_NONE,   SC_CLASSIC,FN_GLOSS,MD_NONE,  TP_NONE},   RGBC(150,156,176),RGBC(232,80,72),RGBC(110,200,255)},
    {"RACER",  {HL_NEEDLE, WG_DELTA,   0,2,1,CN_NOSE,  CK_SLIT,  2,0,0,2,1,2,0,SD_STRIPE, SC_TWO,    FN_METAL,MD_BOOST, TP_NONE},   RGBC(235,235,240),RGBC(225,40,50),RGBC(120,230,255)},
    {"HAULER", {HL_BOX,    WG_STRAIGHT,0,1,2,CN_NOSE,  CK_VISOR, 2,2,1,2,1,1,3,SD_PANELS, SC_CLASSIC,FN_MATTE,MD_RACK,  TP_NONE},   RGBC(200,160,70), RGBC(90,90,100),RGBC(255,190,90)},
    {"GUNSHIP",{HL_WEDGE,  WG_STRAIGHT,2,1,0,CN_QUAD,  CK_DOME,  1,2,0,1,0,1,1,SD_TEETH,  SC_INVERT, FN_METAL,MD_ARMOUR,TP_NONE},   RGBC(85,100,70),  RGBC(230,200,60),RGBC(255,120,60)},
    {"UFO",    {HL_SAUCER, WG_NONE,    0,0,0,CN_TURRET,CK_DOME,  1,1,0,1,1,0,2,SD_LINES,  SC_TWO,    FN_PEARL,MD_GYRO,  TP_BEACON}, RGBC(190,230,200),RGBC(160,90,220),RGBC(150,255,200)},
    {"MANTA",  {HL_MANTA,  WG_GULL,    1,1,0,CN_TWIN,  CK_VISOR, 1,1,0,1,1,1,0,SD_CHEVRON,SC_TWO,    FN_GLOSS,MD_DISH,  TP_NONE},   RGBC(60,120,190), RGBC(240,240,250),RGBC(100,220,255)},
    {"TWINS",  {HL_TWIN,   WG_FORWARD, 1,1,2,CN_TURRET,CK_BUBBLE,1,1,0,1,1,1,0,SD_STRIPE, SC_CLASSIC,FN_GLOSS,MD_NONE,  TP_MASTS},  RGBC(225,120,50), RGBC(50,55,70),RGBC(255,200,90)},
    {"PILL",   {HL_CAPSULE,WG_STRAIGHT,0,1,1,CN_NOSE,  CK_DOME,  1,1,0,1,1,1,0,SD_STRIPE, SC_MONO,   FN_PEARL,MD_BOOST, TP_ANTENNA},RGBC(240,200,120),RGBC(200,90,90),RGBC(255,170,100)},
    {"STINGER",{HL_DART,   WG_FORWARD, 2,1,2,CN_TWIN,  CK_SLIT,  2,0,2,1,2,1,0,SD_SPIKES, SC_TWO,    FN_GLOSS,MD_NONE,  TP_NONE},   RGBC(250,215,60), RGBC(30,30,35),RGBC(255,230,120)},
    {"SCOUT",  {HL_NEEDLE, WG_X,       1,1,0,CN_NOSE,  CK_BUBBLE,2,0,0,1,1,0,0,SD_LINES,  SC_CLASSIC,FN_MATTE,MD_DISH,  TP_ANTENNA},RGBC(90,170,120), RGBC(230,230,120),RGBC(120,255,170)},
};
static void ship_apply_preset(int i){ const ShipPre *q=&ship_pre[i]; const u8 *f=q->f;
    ship.hull=f[0]; ship.wings=f[1]; ship.wsize=f[2]; ship.engines=f[3]; ship.fins=f[4]; ship.cannon=f[5]; ship.cockpit=f[6]; ship.len=f[7]; ship.wid=f[8]; ship.nose=f[9]; ship.wpos=f[10]; ship.dihed=f[11]; ship.esize=f[12]; ship.pods=f[13]; ship.decal=f[14];
    shpx.scheme=f[15]; shpx.finish=f[16]; shpx.module=f[17]; shpx.topper=f[18]; ship.paint=q->paint; ship.trim=q->trim; ship.glow=q->glow; ship_build_model(); }
// ---- the numbers the hangar and the ship menu show: speed, turn, armour (higher is tougher), guns, hold, fuel
static void ship_stats(int *v){ v[0]=ship_top_speed(); v[1]=ship_turn(); v[2]=200-ship_armour(); v[3]=ship_gun(); v[4]=ship_hold_bonus(); v[5]=ship_fuel_bonus()/10; }
