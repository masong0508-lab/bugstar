// ================================================================= YOUR SHIP: built from parts, drawn as a 3D wireframe, and the parts change how it flies
typedef struct { char name[9]; u8 hull, wings, wsize, engines, fins, cannon, cockpit, len; u16 paint, trim; u8 wid, nose, wpos, dihed, esize, pods, decal, pad2; u16 glow; } ShipLook;
static ShipLook ship;
enum { HL_DART, HL_SAUCER, HL_BOX, HL_WEDGE, HL_NEEDLE, NHULL };
enum { WG_NONE, WG_STRAIGHT, WG_SWEPT, WG_DELTA, WG_X, NWING };
static const char *const hull_name[NHULL]={"DART","SAUCER","CARGO BOX","WEDGE","NEEDLE"};
static const char *const wing_name[NWING]={"NONE","STRAIGHT","SWEPT","DELTA","X-WINGS"};
static const char *const wsize_name[3]={"SMALL","MEDIUM","LARGE"};
static const char *const fins_name[3]={"NONE","ONE FIN","TWIN FINS"};
static const char *const cannon_name[4]={"NONE","NOSE GUN","WING GUNS","BIG CANNON"};
static const char *const cockpit_name[2]={"BUBBLE","SLIT"};
static const char *const len_name[3]={"SHORT","NORMAL","LONG"}, *const wid_name[3]={"SLIM","NORMAL","WIDE"}, *const nose_name[3]={"POINT","BLUNT","FORK"};
static const char *const wpos_name[3]={"FORWARD","MIDDLE","BACK"}, *const dihed_name[3]={"DOWN","FLAT","UP"}, *const esize_name[3]={"SMALL","NORMAL","HUGE"};
static const char *const pods_name[4]={"NONE","MISSILE PODS","SCOOPS","FUEL TANKS"}, *const decal_name[4]={"NONE","RACING STRIPE","GLOW LINES","SPIKES"};
static void ship_default(void){ scpy(ship.name,"MOTH"); ship.hull=HL_DART; ship.wings=WG_SWEPT; ship.wsize=1; ship.engines=1; ship.fins=1; ship.cannon=1; ship.cockpit=0; ship.paint=rgb15(150,156,176); ship.trim=rgb15(232,80,72);
    ship.len=1; ship.wid=1; ship.nose=0; ship.wpos=1; ship.dihed=1; ship.esize=1; ship.pods=0; ship.decal=0; ship.glow=rgb15(110,200,255); }
// ---- what the parts do
static int ship_mass(void); static int ship_thrust(void);
static int ship_armour(void){ static const u8 a[NHULL]={100,90,70,95,130}; return a[ship.hull]; }           // % of damage taken
static int ship_top_speed(void){ int t=30+ship_thrust()*110/ship_mass(); if(t>90) t=90; if(t<28) t=28; return t; }
static int ship_turn(void){ static const u8 w[NWING]={75,100,115,130,120}; static const s8 z[3]={10,0,-10}; int t=w[ship.wings]+z[ship.wsize]; t = t*14/(8+ship_mass()/3); return t<40?40:t; }   // % turn rate: heavy ships turn slowly
static int ship_damp(void){ return ship.fins==0 ? 3 : ship.fins==1 ? 4 : 6; }                                  // how fast turning settles
static int ship_gun(void){ static const u8 g[4]={0,2,4,8}; return g[ship.cannon&3]; }                           // extra laser damage
static int ship_hold_bonus(void){ return (ship.hull==HL_BOX ? 8 : ship.hull==HL_SAUCER ? 4 : 0) + (ship.pods==3 ? 0 : 0) + ship.len*2 + ship.wid*2; }
static int ship_mass(void){ static const u8 hm[NHULL]={10,11,16,9,7}; return hm[ship.hull] + ship.len*2 + ship.wid*2 + (ship.wings?1+ship.wsize:0) + (ship.engines+1)*(1+ship.esize) + ship.cannon + (ship.pods?2:0); }
static int ship_thrust(void){ static const u8 es[3]={7,10,14}; return (ship.engines+1)*es[ship.esize]; }
static int ship_fuel_bonus(void){ return ship.pods==3 ? 30 : 0; }                                                  // fuel tanks: a bigger tank
static int ship_accel16(void){ int a=ship_thrust()*24/ship_mass(); if(a<4) a=4; if(a>96) a=96; return a; }        // per-frame push, in 1/16
// ---- the model: vertices in 1/100 of a cell, x right, y up, z forward. faces are flat-shaded polygons (3-8 corners), edges are drawn dark on top
#define SV_MAX 128
#define SE_MAX 200
#define SF_MAX 110
EW static s16 shv[SV_MAX*3]; EW static u8 she[SE_MAX*3]; static int snv, sne, snf;
typedef struct { u8 n, grp, two; u8 v[8]; } Face;          // grp 0 paint, 1 trim. two: visible from both sides (wings, fins)
EW static Face shf[SF_MAX];
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
static int LX(int v){ static const u8 f[3]={80,100,125}; return v*f[ship.len]/100; }     // the hull stretches with its length
static int WX(int v){ static const u8 f[3]={75,100,130}; return v*f[ship.wid]/100; }     // ...and widens
static void ship_build_model(void){
    snv=0; sne=0; snf=0; int hw=WX(22), hh=12, tail=LX(-55), nose=LX(70);
    switch(ship.hull){
        case HL_DART: { int r1=ring4(WX(22),12,LX(20)), r2=ring4(WX(18),10,LX(-40)), r3=ring4(WX(8),6,LX(-60)); join(r1,r2,4,0); join(r2,r3,4,0);
            if(ship.nose==0){ int n=sv_add(0,0,nose); for(int k=0;k<4;k++) se_add(n,r1+k,0); fan(n,r1,4,0,0); }
            else if(ship.nose==1){ int r0=ring4(WX(12),7,nose-10); join(r0,r1,4,0); tube(r0,r1,4,0); sf_add(0,0,4,r0,r0+1,r0+2,r0+3,0,0,0,0); }
            else { for(int sx=-1;sx<=1;sx+=2){ int n=sv_add(sx*WX(14),0,nose+6); se_add(n,r1+(sx>0?0:2),0); se_add(n,r1+1,0); se_add(n,r1+3,0); TRI(0,1,n,r1+1,r1+(sx>0?0:2)); TRI(0,1,n,r1+(sx>0?0:2),r1+3); } sf_add(0,1,4,r1,r1+1,r1+2,r1+3,0,0,0,0); }
            tube(r1,r2,4,0); tube(r2,r3,4,0); sf_add(0,0,4,r3+3,r3+2,r3+1,r3,0,0,0,0); hw=WX(22); hh=12; tail=LX(-60); break; }
        case HL_SAUCER: { int r1=ring8(WX(46),0,0), top=sv_add(0,16,0), bot=sv_add(0,-10,0); for(int k=0;k<8;k+=2){ se_add(r1+k,top,0); se_add(r1+k,bot,0); }
            for(int i=0;i<8;i++) if(r1+i<snv) shv[(r1+i)*3+2]=LX(shv[(r1+i)*3+2]);
            fan(top,r1,8,0,1); fan(bot,r1,8,0,0); hw=WX(46); hh=10; tail=LX(-40); nose=LX(46); break; }
        case HL_BOX: { int a=snv; int W=WX(28), B=LX(-50), F=LX(40); sv_add(-W,-14,B); sv_add(W,-14,B); sv_add(W,14,B); sv_add(-W,14,B); sv_add(-W,-14,F); sv_add(W,-14,F); sv_add(W,14,F); sv_add(-W,14,F);
            static const u8 be[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; for(int k=0;k<12;k++) se_add(a+be[k][0],a+be[k][1],0);
            QUAD(0,0,a+0,a+3,a+2,a+1); QUAD(0,0,a+0,a+1,a+5,a+4); QUAD(0,0,a+1,a+2,a+6,a+5); QUAD(0,0,a+2,a+3,a+7,a+6); QUAD(0,0,a+3,a+0,a+4,a+7);
            if(ship.nose==1){ QUAD(0,0,a+4,a+5,a+6,a+7); } else { int n=sv_add(0,0,LX(62)); for(int k=4;k<8;k++) se_add(n,a+k,0); TRI(0,0,n,a+4,a+5); TRI(0,0,n,a+5,a+6); TRI(0,0,n,a+6,a+7); TRI(0,0,n,a+7,a+4); }
            hw=W; hh=14; tail=B; nose=LX(62); break; }
        case HL_WEDGE: { int n=sv_add(0,0,nose); int a=snv; int W=WX(46), B=LX(-50); sv_add(-W,-6,B); sv_add(W,-6,B); sv_add(W,6,B); sv_add(-W,6,B); for(int k=0;k<4;k++){ se_add(n,a+k,0); se_add(a+k,a+((k+1)&3),0); }
            TRI(0,0,n,a+0,a+1); TRI(0,0,n,a+1,a+2); TRI(0,0,n,a+2,a+3); TRI(0,0,n,a+3,a+0); QUAD(0,0,a+0,a+3,a+2,a+1); hw=WX(20); hh=6; tail=B; break; }
        default: { int n=sv_add(0,0,LX(92)); int r1=ring4(WX(10),10,LX(10)), r2=ring4(WX(7),7,LX(-70)); for(int k=0;k<4;k++) se_add(n,r1+k,0); join(r1,r2,4,0);
            fan(n,r1,4,0,0); tube(r1,r2,4,0); sf_add(0,0,4,r2+3,r2+2,r2+1,r2,0,0,0,0); hw=WX(10); hh=10; tail=LX(-70); nose=LX(92); break; }
    }
    int s = 22+ship.wsize*16, wz = ship.wpos==0 ? 25 : ship.wpos==1 ? 0 : -20, dy = (ship.dihed-1)*s/3;
    if(ship.wings==WG_X){ for(int q=0;q<4;q++){ int sx=(q&1)?1:-1, sy=(q&2)?1:-1; int a=sv_add(sx*hw/2,sy*hh/2,wz), b=sv_add(sx*(hw/2+s),sy*(hh/2+s),wz-25), c=sv_add(sx*hw/2,sy*hh/2,wz-30); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); } }
    else if(ship.wings){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,0,wz+10), b=sv_add(sx*hw,0,wz-35), c, d;
            if(ship.wings==WG_STRAIGHT){ c=sv_add(sx*(hw+s),dy,wz-35); d=sv_add(sx*(hw+s),dy,wz); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); QUAD(1,1,a,d,c,b); }
            else if(ship.wings==WG_SWEPT){ c=sv_add(sx*(hw+s),dy,wz-35-s/2); d=sv_add(sx*(hw+s),dy,wz-15-s/2); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); QUAD(1,1,a,d,c,b); }
            else { c=sv_add(sx*(hw+s),dy,wz-45); se_add(a,c,1); se_add(c,b,1); TRI(1,1,a,c,b); }
            if(ship.pods==1){ int px0=sx*(hw+s*2/3); int p=snv; sv_add(px0-4,dy-3,wz-30); sv_add(px0+4,dy-3,wz-30); sv_add(px0+4,dy-3,wz+2); sv_add(px0-4,dy-3,wz+2); sv_add(px0-4,dy-9,wz-30); sv_add(px0+4,dy-9,wz-30); sv_add(px0+4,dy-9,wz+2); sv_add(px0-4,dy-9,wz+2);
                QUAD(1,0,p+0,p+1,p+2,p+3); QUAD(1,0,p+4,p+7,p+6,p+5); QUAD(1,0,p+0,p+4,p+5,p+1); QUAD(1,0,p+2,p+6,p+7,p+3); QUAD(1,0,p+1,p+5,p+6,p+2); QUAD(1,0,p+3,p+7,p+4,p+0); for(int k=0;k<4;k++){ se_add(p+k,p+((k+1)&3),1); se_add(p+4+k,p+4+((k+1)&3),1); } } } }
    if(ship.pods==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,hh/2,20), b=sv_add(sx*(hw+10),0,0), c=sv_add(sx*hw,-hh/2,-20); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); } }
    if(ship.pods==3){ for(int sx=-1;sx<=1;sx+=2){ int r=ring4(7,7,10), r2=ring4(7,7,-40); for(int i=0;i<8;i++) if(r+i<snv) shv[(r+i)*3]+=sx*(hw+8); join(r,r2,4,1); tube(r,r2,4,1); sf_add(1,0,4,r,r+1,r+2,r+3,0,0,0,0); sf_add(1,0,4,r2+3,r2+2,r2+1,r2,0,0,0,0); } }
    { static const u8 er[3]={5,7,10}; int R=er[ship.esize];
      for(int k=0;k<=ship.engines;k++){ int n=ship.engines+1; int ex = n==1?0 : n==2 ? (k?hw*3/4:-hw*3/4) : n==3 ? (k==0?-hw:k==1?0:hw) : (k==0?-hw:k==1?-hw/3:k==2?hw/3:hw); int ey = (n==3 && k==1) ? hh/2 : 0;
        int r=ring8(R,tail,1), r2=ring8(R*5/7,tail-10-R,1); join(r,r2,8,1); for(int i=0;i<16;i++){ if(r+i<snv){ shv[(r+i)*3]+=ex; shv[(r+i)*3+1]+=ey; } }
        tube(r,r2,8,1); sf_add(2,0,8,r2+7,r2+6,r2+5,r2+4,r2+3,r2+2,r2+1,r2); } }
    if(ship.fins==1){ int a=sv_add(0,hh,LX(-15)), b=sv_add(0,hh+26,LX(-45)), c=sv_add(0,hh,LX(-48)); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); }
    else if(ship.fins==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw*3/4,hh/2,LX(-15)), b=sv_add(sx*hw,hh/2+22,LX(-45)), c=sv_add(sx*hw*3/4,hh/2,LX(-48)); se_add(a,b,1); se_add(b,c,1); TRI(1,1,a,b,c); } }
    if(ship.cannon==1){ int a=sv_add(0,-hh/2,nose-10), b=sv_add(0,-hh/2,nose+22); se_add(a,b,1); }
    else if(ship.cannon==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*(hw+s/2),dy-4,wz-20), b=sv_add(sx*(hw+s/2),dy-4,wz+30); se_add(a,b,1); } }
    else if(ship.cannon==3){ int a=sv_add(-6,-hh-4,-20), b=sv_add(-6,-hh-4,nose+10), c=sv_add(6,-hh-4,-20), d=sv_add(6,-hh-4,nose+10); se_add(a,b,1); se_add(c,d,1); se_add(b,d,1); se_add(a,c,1); QUAD(1,1,a,b,d,c); }
    if(ship.cockpit==0){ int a=sv_add(0,hh,LX(20)), b=sv_add(0,hh+9,LX(10)), c=sv_add(0,hh,0); se_add(a,b,2); se_add(b,c,2); TRI(2,1,a,b,c); }
    else { int a=sv_add(-hw/2,hh-1,LX(20)), b=sv_add(hw/2,hh-1,LX(20)); se_add(a,b,2); }
    if(ship.decal==1){ int a=sv_add(0,hh+1,nose-8), b=sv_add(0,hh+1,tail+4); se_add(a,b,1); int c=sv_add(-4,hh+1,nose-12), d=sv_add(-4,hh+1,tail+4); se_add(c,d,1); }
    else if(ship.decal==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,2,nose-15), b=sv_add(sx*hw,2,tail+6); se_add(a,b,2); } }
    else if(ship.decal==3){ for(int k=0;k<3;k++){ int z=tail+20+k*LX(22); int a=sv_add(0,hh,z), b=sv_add(0,hh+10,z-6); se_add(a,b,1); } }
}
// ---- drawing it: solid, lit, with dark outlines. paint shades live in BG palette 200..207, trim in 208..215
#define SHP_PAINT 200
#define SHP_TRIM 208
#define SHP_GLOW 216
static void ship_ramps(void){ for(int k=0;k<8;k++){ BG_PAL[SHP_PAINT+k] = k<4 ? shade15(ship.paint,6+k*3) : lift15(ship.paint,(k-4)*4); BG_PAL[SHP_TRIM+k] = k<4 ? shade15(ship.trim,6+k*3) : lift15(ship.trim,(k-4)*4); BG_PAL[SHP_GLOW+k] = k<3 ? shade15(ship.glow,10+k*2) : lift15(ship.glow,(k-3)*3); } }
static void linep(volatile u16 *pg, int x0, int y0, int x1, int y1, int c, int cx0, int cy0, int cx1, int cy1){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(n>300) return;
    if(!n){ if(x0>=cx0&&x0<cx1&&y0>=cy0&&y0<cy1) pix(pg,x0,y0,c); return; }
    s32 fx=x0<<16, fy=y0<<16, sx=(dx<<16)/n, sy=(dy<<16)/n;
    for(int i=0;i<=n;i++){ int x=(fx+0x8000)>>16, y=(fy+0x8000)>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1) pix(pg,x,y,c); fx+=sx; fy+=sy; }
}
static void polyfill(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1){
    int ymin=py[0], ymax=py[0]; for(int i=1;i<n;i++){ if(py[i]<ymin) ymin=py[i]; if(py[i]>ymax) ymax=py[i]; }
    if(ymin<cy0) ymin=cy0; if(ymax>=cy1) ymax=cy1-1;
    for(int y=ymin;y<=ymax;y++){ int xl=9999, xr=-9999;
        for(int i=0;i<n;i++){ int j=(i+1)%n; int ya=py[i], yb=py[j]; if(ya==yb) continue; if((y<ya && y<yb) || (y>ya && y>yb)) continue;
            int x = px[i] + (y-ya)*(px[j]-px[i])/(yb-ya); if(x<xl) xl=x; if(x>xr) xr=x; }
        if(xl>xr) continue; if(xl<cx0) xl=cx0; if(xr>=cx1) xr=cx1-1; if(xl>xr) continue;
        rect(pg,xl,y,xr-xl+1,1,c); }
}
static int ship_roll=0;                                                        // extra bank applied on screen (the chase camera)
static void ship_draw(volatile u16 *pg, int cx, int cy, int sc, int yaw_, int pitch_, int cpaint, int ctrim, int cx0,int cy0,int cx1,int cy1){
    static s16 px[SV_MAX], py[SV_MAX]; static s16 rx_[SV_MAX], ry_[SV_MAX], rz_[SV_MAX];
    (void)cpaint; (void)ctrim; ship_ramps(); s32 rc=cosx(ship_roll>>2), rs=sinx(ship_roll>>2);
    s32 cy_=cosx(yaw_>>2), sy_=sinx(yaw_>>2), cp=cosx(pitch_>>2), sp=sinx(pitch_>>2);
    for(int i=0;i<snv;i++){ s32 x=shv[i*3], y=shv[i*3+1], z=shv[i*3+2];
        s32 rx=(x*cy_ + z*sy_)>>14, rz=(-x*sy_ + z*cy_)>>14;
        s32 ry=(y*cp - rz*sp)>>14; rz=(y*sp + rz*cp)>>14;
        rx_[i]=rx; ry_[i]=ry; rz_[i]=rz;
        s32 d = 400 + rz; if(d<60) d=60;
        { s32 X=(rx*sc*400/d)/256, Y=-(ry*sc*400/d)/256; s32 X2=(X*rc - Y*rs)>>14, Y2=(X*rs + Y*rc)>>14; X=cx+X2; Y=cy+Y2; if(X<-2000) X=-2000; if(X>2000) X=2000; if(Y<-2000) Y=-2000; if(Y>2000) Y=2000; px[i]=X; py[i]=Y; } }
    // sort faces far to near
    static u8 ord[SF_MAX]; static s16 fz[SF_MAX];
    for(int f=0;f<snf;f++){ Face *F=&shf[f]; int z=0; for(int k=0;k<F->n;k++) z+=rz_[F->v[k]]; fz[f]=z/F->n; ord[f]=f; }
    for(int i=1;i<snf;i++){ u8 t=ord[i]; int j=i; while(j>0 && fz[ord[j-1]]>fz[t]){ ord[j]=ord[j-1]; j--; } ord[j]=t; }
    for(int o=0;o<snf;o++){ Face *F=&shf[ord[o]]; int a=F->v[0], b=F->v[1], c=F->v[2];
        // facing: screen-space winding
        int area=(px[b]-px[a])*(py[c]-py[a]) - (px[c]-px[a])*(py[b]-py[a]);
        // light: the face normal (in camera space) against a lamp up-front-left
        s32 ux=rx_[b]-rx_[a], uy=ry_[b]-ry_[a], uz=rz_[b]-rz_[a], vx=rx_[c]-rx_[a], vy=ry_[c]-ry_[a], vz=rz_[c]-rz_[a];
        s32 nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
        s32 len=isqrt((u32)((nx>>4)*(nx>>4)+(ny>>4)*(ny>>4)+(nz>>4)*(nz>>4)))<<4; if(len<1) len=1;
        s32 dot=(-nx*5 + ny*11 - nz*8)/len;                                   // light from (-0.35, 0.75, -0.55), scaled by 16
        if(area>=0) dot=-dot;
        int sh = 3 + dot*4/15; if(sh<0) sh=0; if(sh>7) sh=7;
        int X[8], Y[8]; for(int k=0;k<F->n;k++){ X[k]=px[F->v[k]]; Y[k]=py[F->v[k]]; }
        int base = F->grp==2 ? SHP_GLOW : F->grp ? SHP_TRIM : SHP_PAINT; if(F->grp==2) sh = 5+((frames>>2)&1)*2;      // engines glow and flicker
        polyfill(pg,X,Y,F->n,base+sh,cx0,cy0,cx1,cy1); }
    for(int i=0;i<sne;i++){ int a=she[i*3], b=she[i*3+1], g=she[i*3+2]; linep(pg,px[a],py[a],px[b],py[b], g==2?SHP_GLOW+6 : g?SHP_TRIM:SHP_PAINT, cx0,cy0,cx1,cy1); }
}
