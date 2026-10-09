// ================================================================= YOUR SHIP: built from parts, drawn as a 3D wireframe, and the parts change how it flies
typedef struct { char name[9]; u8 hull, wings, wsize, engines, fins, cannon, cockpit, pad; u16 paint, trim; } ShipLook;
static ShipLook ship;
enum { HL_DART, HL_SAUCER, HL_BOX, HL_WEDGE, HL_NEEDLE, NHULL };
enum { WG_NONE, WG_STRAIGHT, WG_SWEPT, WG_DELTA, WG_X, NWING };
static const char *const hull_name[NHULL]={"DART","SAUCER","CARGO BOX","WEDGE","NEEDLE"};
static const char *const wing_name[NWING]={"NONE","STRAIGHT","SWEPT","DELTA","X-WINGS"};
static const char *const wsize_name[3]={"SMALL","MEDIUM","LARGE"};
static const char *const fins_name[3]={"NONE","ONE FIN","TWIN FINS"};
static const char *const cannon_name[4]={"NONE","NOSE GUN","WING GUNS","BIG CANNON"};
static const char *const cockpit_name[2]={"BUBBLE","SLIT"};
static void ship_default(void){ scpy(ship.name,"MOTH"); ship.hull=HL_DART; ship.wings=WG_SWEPT; ship.wsize=1; ship.engines=1; ship.fins=1; ship.cannon=1; ship.cockpit=0; ship.paint=rgb15(150,156,176); ship.trim=rgb15(232,80,72); }
// ---- what the parts do
static int ship_armour(void){ static const u8 a[NHULL]={100,90,70,95,130}; return a[ship.hull]; }           // % of damage taken
static int ship_top_speed(void){ static const u8 s[4]={44,52,62,74}; return s[ship.engines&3]; }
static int ship_turn(void){ static const u8 w[NWING]={75,100,115,130,120}; static const s8 z[3]={10,0,-10}; return w[ship.wings]+z[ship.wsize]; }   // % turn rate
static int ship_damp(void){ return ship.fins==0 ? 3 : ship.fins==1 ? 4 : 6; }                                  // how fast turning settles
static int ship_gun(void){ static const u8 g[4]={0,2,4,8}; return g[ship.cannon&3]; }                           // extra laser damage
static int ship_hold_bonus(void){ return ship.hull==HL_BOX ? 8 : ship.hull==HL_SAUCER ? 4 : 0; }
// ---- the model: vertices in 1/100 of a cell, x right, y up, z forward. edges carry a colour (0 paint, 1 trim)
#define SV_MAX 64
#define SE_MAX 110
EW static s16 shv[SV_MAX*3]; EW static u8 she[SE_MAX*3]; static int snv, sne;
static int sv_add(int x,int y,int z){ if(snv>=SV_MAX) return snv-1; shv[snv*3]=x; shv[snv*3+1]=y; shv[snv*3+2]=z; return snv++; }
static void se_add(int a,int b,int c){ if(sne>=SE_MAX) return; she[sne*3]=a; she[sne*3+1]=b; she[sne*3+2]=c; sne++; }
static int ring4(int w,int h,int z){ int i=snv; sv_add(w,0,z); sv_add(0,h,z); sv_add(-w,0,z); sv_add(0,-h,z); for(int k=0;k<4;k++) se_add(i+k,i+((k+1)&3),0); return i; }
static int ring8(int r,int z,int c){ int i=snv; for(int k=0;k<8;k++){ int a=k*32; sv_add((cosx(a)*r)>>14, (sinx(a)*r)>>14, z); } for(int k=0;k<8;k++) se_add(i+k,i+((k+1)&7),c); return i; }
static void join(int a,int b,int n,int c){ for(int k=0;k<n;k++) se_add(a+k,b+k,c); }
static void ship_build_model(void){
    snv=0; sne=0; int hw=22, hh=12, tail=-55, nose=70;
    switch(ship.hull){
        case HL_DART: { int n=sv_add(0,0,nose); int r1=ring4(22,12,20), r2=ring4(18,10,-40), r3=ring4(8,6,-60); for(int k=0;k<4;k++) se_add(n,r1+k,0); join(r1,r2,4,0); join(r2,r3,4,0); hw=22; hh=12; tail=-60; break; }
        case HL_SAUCER: { int r1=ring8(46,0,0), top=sv_add(0,16,0), bot=sv_add(0,-10,0); for(int k=0;k<8;k+=2){ se_add(r1+k,top,0); se_add(r1+k,bot,0); } hw=46; hh=10; tail=-40; nose=46; break; }
        case HL_BOX: { int a=snv; sv_add(-28,-14,-50); sv_add(28,-14,-50); sv_add(28,14,-50); sv_add(-28,14,-50); sv_add(-28,-14,40); sv_add(28,-14,40); sv_add(28,14,40); sv_add(-28,14,40);
            static const u8 be[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}}; for(int k=0;k<12;k++) se_add(a+be[k][0],a+be[k][1],0);
            int n=sv_add(0,0,62); for(int k=4;k<8;k++) se_add(n,a+k,0); hw=28; hh=14; tail=-50; break; }
        case HL_WEDGE: { int n=sv_add(0,0,nose); int a=snv; sv_add(-46,-6,-50); sv_add(46,-6,-50); sv_add(46,6,-50); sv_add(-46,6,-50); for(int k=0;k<4;k++){ se_add(n,a+k,0); se_add(a+k,a+((k+1)&3),0); } hw=20; hh=6; tail=-50; break; }
        default: { int n=sv_add(0,0,92); int r1=ring4(10,10,10), r2=ring4(7,7,-70); for(int k=0;k<4;k++) se_add(n,r1+k,0); join(r1,r2,4,0); hw=10; hh=10; tail=-70; break; }
    }
    int s = 22+ship.wsize*16;
    if(ship.wings==WG_X){ for(int q=0;q<4;q++){ int sx=(q&1)?1:-1, sy=(q&2)?1:-1; int a=sv_add(sx*hw/2,sy*hh/2,0), b=sv_add(sx*(hw/2+s),sy*(hh/2+s),-25), c=sv_add(sx*hw/2,sy*hh/2,-30); se_add(a,b,1); se_add(b,c,1); } }
    else if(ship.wings){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw,0,10), b=sv_add(sx*hw,0,-35), c, d;
            if(ship.wings==WG_STRAIGHT){ c=sv_add(sx*(hw+s),0,-35); d=sv_add(sx*(hw+s),0,0); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); }
            else if(ship.wings==WG_SWEPT){ c=sv_add(sx*(hw+s),0,-35-s/2); d=sv_add(sx*(hw+s),0,-15-s/2); se_add(a,d,1); se_add(d,c,1); se_add(c,b,1); }
            else { c=sv_add(sx*(hw+s),0,-45); se_add(a,c,1); se_add(c,b,1); } } }
    for(int k=0;k<=ship.engines;k++){ int n=ship.engines+1; int x = n==1 ? 0 : (k-(n-1)*128/256)*0; (void)x; int ex = n==1?0 : n==2 ? (k?hw*3/4:-hw*3/4) : (k==0?-hw:k==1?0:hw); int ey = n==3 && k==1 ? hh/2 : 0;
        int r=ring8(7,tail,1), r2=ring8(5,tail-10,1); join(r,r2,8,1); for(int i=0;i<16;i++){ shv[(r+i)*3]+=ex; shv[(r+i)*3+1]+=ey; } }
    if(ship.fins==1){ int a=sv_add(0,hh,-15), b=sv_add(0,hh+26,-45), c=sv_add(0,hh,-48); se_add(a,b,1); se_add(b,c,1); }
    else if(ship.fins==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*hw*3/4,hh/2,-15), b=sv_add(sx*hw,hh/2+22,-45), c=sv_add(sx*hw*3/4,hh/2,-48); se_add(a,b,1); se_add(b,c,1); } }
    if(ship.cannon==1){ int a=sv_add(0,-hh/2,nose-10), b=sv_add(0,-hh/2,nose+22); se_add(a,b,1); }
    else if(ship.cannon==2){ for(int sx=-1;sx<=1;sx+=2){ int a=sv_add(sx*(hw+s/2),-4,-20), b=sv_add(sx*(hw+s/2),-4,30); se_add(a,b,1); } }
    else if(ship.cannon==3){ int a=sv_add(-6,-hh-4,-20), b=sv_add(-6,-hh-4,nose+10), c=sv_add(6,-hh-4,-20), d=sv_add(6,-hh-4,nose+10); se_add(a,b,1); se_add(c,d,1); se_add(b,d,1); se_add(a,c,1); }
    if(ship.cockpit==0){ int a=sv_add(0,hh,20), b=sv_add(0,hh+9,10), c=sv_add(0,hh,0); se_add(a,b,1); se_add(b,c,1); }
    else { int a=sv_add(-hw/2,hh-1,20), b=sv_add(hw/2,hh-1,20); se_add(a,b,1); }
}
// ---- drawing it: a clipped line into any page, and a little camera for the preview
static void linep(volatile u16 *pg, int x0, int y0, int x1, int y1, int c, int cx0, int cy0, int cx1, int cy1){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(n>300) return;
    if(!n){ if(x0>=cx0&&x0<cx1&&y0>=cy0&&y0<cy1) pix(pg,x0,y0,c); return; }
    s32 fx=x0<<16, fy=y0<<16, sx=(dx<<16)/n, sy=(dy<<16)/n;
    for(int i=0;i<=n;i++){ int x=(fx+0x8000)>>16, y=(fy+0x8000)>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1) pix(pg,x,y,c); fx+=sx; fy+=sy; }
}
// the ship turned by yaw and tilted by pitch (0..1023), seen from a distance, centred at (cx,cy) with scale sc (1/256 px per unit)
static void ship_draw(volatile u16 *pg, int cx, int cy, int sc, int yaw_, int pitch_, int cpaint, int ctrim, int cx0,int cy0,int cx1,int cy1){
    static s16 px[SV_MAX], py[SV_MAX];
    s32 cy_=cosx(yaw_>>2), sy_=sinx(yaw_>>2), cp=cosx(pitch_>>2), sp=sinx(pitch_>>2);
    for(int i=0;i<snv;i++){ s32 x=shv[i*3], y=shv[i*3+1], z=shv[i*3+2];
        s32 rx=(x*cy_ + z*sy_)>>14, rz=(-x*sy_ + z*cy_)>>14;                      // turn
        s32 ry=(y*cp - rz*sp)>>14; rz=(y*sp + rz*cp)>>14;                           // tilt
        s32 d = 400 + rz;  if(d<60) d=60;                                            // a little perspective
        px[i]=cx + (rx*sc*400/d)/256; py[i]=cy - (ry*sc*400/d)/256; }
    for(int i=0;i<sne;i++){ int a=she[i*3], b=she[i*3+1]; linep(pg,px[a],py[a],px[b],py[b], she[i*3+2]?ctrim:cpaint, cx0,cy0,cx1,cy1); }
}
