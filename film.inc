// ================================================================= FILM: the trailer's last shot (test builds only: -DTEST_LAUNCH)
// Sunrise over a planet. Your ship lifts off the night side, climbs past the sun and flies straight into the camera.
// It is drawn slowly and carefully (no hurry: it is filmed, not played). After each picture a counter at FILM_FLAG goes up,
// and the recorder (rec: "recf") takes exactly one frame per picture, however long the picture took.
#define FILM_FLAG (*(volatile u32*)0x0203FFF8)            // (past the end of the game's EWRAM)
#define FP_STAR 1          // 1..15   stars
#define FP_SUN 16          // 16..31  sunlight, dark red to white
#define FP_GLOW 32         // 32..47  engine light, dark blue to white
#define FP_ATM 48          // 48..63  the air, deep blue to white
#define FP_LAND 64         // 64..111 6 kinds of ground x 8 lights
#define FP_CLOUD 112       // 112..119
#define FP_CITY 120        // 120..127 city lights on the night side
#define FP_DUST 128        // 128..143 dust and smoke
#define FP_STREAK 144      // 144..159 the lens's blue streak
#define FY0 92             // the planet's first row
#ifndef FL_HULL
#define FL_HULL HL_DART
#endif
#ifndef FL_WINGS
#define FL_WINGS WG_X
#endif
#ifndef FL_ENG
#define FL_ENG 3
#endif
#ifndef FL_DIHED
#define FL_DIHED 1
#endif
#ifndef FL_PAINT
#define FL_PAINT 186,194,212
#endif
#ifndef FL_TRIM
#define FL_TRIM 250,104,36
#endif
#ifndef FL_IGN
#define FL_IGN 56
#endif
#ifndef FL_LIFT
#define FL_LIFT 80
#endif
#ifndef FL_END
#define FL_END 250         // the frame the ship reaches the camera
#endif
#ifndef FL_TOTAL
#define FL_TOTAL 430
#endif
static const u8 fl_bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
static int fl_pcx=140, fl_pcy=332, fl_R=232, fl_sunx=170, fl_suny=99;
static u16 *fl_u, *fl_v; static s8 *fl_lit; static u8 *fl_nz;   // per pixel of rows FY0..159 (in the planet maps' memory: nothing else runs)
static float ff_sqrt(float x){ if(x<=0) return 0; float r = x>1 ? x : 1; for(int i=0;i<14;i++) r=0.5f*(r+x/r); return r; }
static float ff_atan(float x){ float x2=x*x; return x*(0.99986604f + x2*(-0.33029950f + x2*(0.18014100f + x2*(-0.08513300f + x2*0.02083510f)))); }
static float ff_atan2(float y, float x){ float ay=y<0?-y:y, ax=x<0?-x:x, a; if(ax==0 && ay==0) return 0;
    a = ax>=ay ? ff_atan(ay/ax) : 1.5707963f-ff_atan(ax/ay); if(x<0) a=3.1415927f-a; return y<0?-a:a; }
static u16 fl_mix(u16 a, u16 b, int k){ int r=((a&31)*(16-k)+(b&31)*k)>>4, g=(((a>>5)&31)*(16-k)+((b>>5)&31)*k)>>4, bl=(((a>>10)&31)*(16-k)+((b>>10)&31)*k)>>4; return r|(g<<5)|(bl<<10); }
static void fl_ramp(int at, int n, const u16 *key, int nk){       // n colours through nk key colours
    for(int i=0;i<n;i++){ int f=i*(nk-1)*16/(n-1), a=f>>4, fr=f&15; if(a>=nk-1){ a=nk-2; fr=16; } BG_PAL[at+i]=fl_mix(key[a],key[a+1],fr); } }
static void fl_palette(void){
    BG_PAL[0]=0;
    for(int i=0;i<15;i++){ int v=40+i*15; BG_PAL[FP_STAR+i] = (i%5==4) ? rgb15(v,v*9/10,v*7/10) : (i%5==2) ? rgb15(v*8/10,v*9/10,v) : rgb15(v,v,v); }
    { u16 s[6]={rgb15(30,6,10),rgb15(120,30,20),rgb15(230,110,40),rgb15(255,200,90),rgb15(255,245,200),rgb15(255,255,255)}; fl_ramp(FP_SUN,16,s,6); }
    { u16 g[5]={rgb15(10,20,60),rgb15(30,80,200),rgb15(60,170,255),rgb15(170,235,255),rgb15(255,255,255)}; fl_ramp(FP_GLOW,16,g,5); }
    { u16 a[5]={rgb15(4,10,40),rgb15(20,50,140),rgb15(60,130,230),rgb15(160,210,255),rgb15(255,250,235)}; fl_ramp(FP_ATM,16,a,5); }
    { static const u8 band[6][3]={{14,40,96},{26,74,150},{40,140,170},{120,150,70},{60,110,46},{150,140,120}};   // deep sea, sea, shallows, lowland, forest, mountains
      for(int b=0;b<6;b++) for(int l=0;l<8;l++){ int r=band[b][0], g=band[b][1], bl=band[b][2];
          int k = l==0 ? 6 : 10+l*22;                                                          // night is not quite black: a little blue
          int R=r*k/164, G=g*k/164, B=bl*k/164 + (l<2 ? 10-l*4 : 0); BG_PAL[FP_LAND+b*8+l]=rgb15(R,G,B); } }
    for(int l=0;l<8;l++){ int v = l==0 ? 18 : l==1 ? 46 : 70+l*26; BG_PAL[FP_CLOUD+l]=rgb15(v,v,v+(l<3?14:4)); }
    { u16 c[4]={rgb15(255,190,80),rgb15(255,150,50),rgb15(255,230,140),rgb15(200,120,40)}; for(int i=0;i<8;i++) BG_PAL[FP_CITY+i]=c[i&3]; }
    { u16 d[4]={rgb15(10,10,14),rgb15(70,60,60),rgb15(160,140,120),rgb15(240,220,190)}; fl_ramp(FP_DUST,16,d,4); }
    { u16 s[4]={rgb15(4,10,30),rgb15(20,60,160),rgb15(90,180,255),rgb15(220,250,255)}; fl_ramp(FP_STREAK,16,s,4); }
}
// ---- the planet: worked out once per pixel (where on the globe, how lit, how near the edge), then each frame only turns the map
static int fl_band(int t){ return t<64 ? 0 : t<112 ? 1 : t<128 ? 2 : t<150 ? 3 : t<198 ? 4 : 5; }
static void fl_planet_init(void){
    fl_u=(u16*)cmap; fl_v=fl_u+(160-FY0)*240; fl_lit=(s8*)hfine; fl_nz=(u8*)hfine+(160-FY0)*240;
    float R=(float)fl_R, cT=0.2588f, sT=0.9659f;                                   // the globe is tipped 75 degrees: its tropics run along the top edge
    float Lx=0.30f, Ly=0.42f, Lz=-0.86f;                                            // the sun: behind the planet, up and to the right
    float sx0=(float)fl_sunx;
    for(int y=FY0;y<160;y++) for(int x=0;x<240;x++){ int i=(y-FY0)*240+x;
        float dx=((float)x+0.5f-(float)fl_pcx)/R, dy=((float)fl_pcy-((float)y+0.5f))/R, d2=dx*dx+dy*dy;
        if(d2>=1.0f){                                                               // outside: the glow of the air above the edge
            float h=(ff_sqrt(d2)-1.0f)*R;                                           // pixels above the edge
            float ax=((float)x-sx0); if(ax<0) ax=-ax; float near=1.0f-ax/150.0f; if(near<0) near=0;   // brighter toward the sun
            float k = h<1 ? 1.0f : 1.0f/(1.0f+(h-1.0f)*(h-1.0f)*(0.10f+0.25f*(1.0f-near)));
            float I = k*(0.30f+0.70f*near*near);
            int lv=(int)(I*15.0f*16.0f); if(lv>255) lv=255;                        // 0..255: 4 bits of level, 4 of dither
            fl_u[i]=0xFFFF; fl_v[i]=(u16)lv; fl_lit[i]=0; fl_nz[i]=0; continue; }
        float nz=ff_sqrt(1.0f-d2);
        float py=dy*cT - nz*sT, pz=dy*sT + nz*cT, px=dx;
        float lat=ff_atan2(py, ff_sqrt(1.0f-py*py)), lon=ff_atan2(px,pz);
        float u=(lon/6.2831853f+0.5f)*128.0f, v=(lat/3.1415927f+0.5f)*63.0f; if(v<0) v=0; if(v>62.99f) v=62.99f;
        fl_u[i]=(u16)(int)(u*256.0f); fl_v[i]=(u16)(int)(v*256.0f);
        float lit=dx*Lx+dy*Ly+nz*Lz; int li=(int)(lit*127.0f); if(li>127) li=127; if(li<-127) li=-127; fl_lit[i]=(s8)li;
        float hx=Lx, hy=Ly, hz=Lz+1.0f, hl=ff_sqrt(hx*hx+hy*hy+hz*hz); float nh=(dx*hx+dy*hy+nz*hz)/hl; if(nh<0) nh=0;   // the sun's glint off the sea
        float sp=nh*nh; sp=sp*sp; sp=sp*sp; sp=sp*sp; sp*=sp;                       // (nh to the 32nd)
        int n4=(int)(nz*15.0f+0.5f), s4=(int)(sp*15.0f); if(s4>15) s4=15; fl_nz[i]=(u8)((n4<<4)|s4); }
}
static int fl_tex(const u8 *n, u32 u, u32 v){                       // the 128x64 map, smoothly in between its squares: 0..240
    int u0=(u>>8)&127, u1=(u0+1)&127, v0=v>>8, v1=v0<63?v0+1:63, fu=u&255, fv=v&255;
    int a=n[(v0<<7)|u0], b=n[(v0<<7)|u1], c=n[(v1<<7)|u0], d=n[(v1<<7)|u1];
    int top=a*(256-fu)+b*fu, bot=c*(256-fu)+d*fu; return (top*(256-fv)+bot*fv)>>12; }
static void fl_planet(volatile u16 *pg, int tq){
    const u8 *gnd=pl_noise+(1<<13), *cld=pl_noise+16384;
    u32 rot=(u32)(tq*7/4), crot=(u32)(tq*13/4)+40*256;                                        // in 1/256 of a square
    for(int y=FY0;y<160;y++){ volatile u16 *row=pg+y*120; const u8 *bz=fl_bayer+(y&3)*4;
        for(int x=0;x<240;x+=2){ u16 two=0;
            for(int h=0;h<2;h++){ int xx=x+h, i=(y-FY0)*240+xx, c;
                if(fl_u[i]==0xFFFF){ int lv=fl_v[i]; int l=(lv+bz[xx&3])>>4; if(l>15) l=15;    // the air above the edge: only where it is brighter than the sky
                    int cur = h ? (row[x>>1]>>8) : (row[x>>1]&255); int curb = cur>=FP_SUN && cur<FP_SUN+16 ? cur-FP_SUN : cur>=FP_STAR && cur<FP_STAR+15 ? 1 : 0;
                    c = (l>0 && l>=curb) ? FP_ATM+l : cur; }
                else {
                    u32 u=fl_u[i]+rot, v=fl_v[i]; int tt=fl_tex(gnd,u,v), cl=fl_tex(cld,fl_u[i]+crot,v);
                    int li=fl_lit[i], n4=fl_nz[i]>>4, s4=fl_nz[i]&15, b=bz[xx&3];
                    int dl=li+10; if(dl<0) dl=0; if(dl>127) dl=127; int lv=(dl*112/127+b)>>4; if(lv>7) lv=7;
                    int band=fl_band(tt); c=FP_LAND+band*8+lv;
                    if(band<=2 && s4 && lv>0 && n4<=6 && (s4*3+lv*2)>12+b) c=FP_ATM+12+(s4>>2);                    // sunlight glinting off the water
                    if(lv==0 && band>=3 && band<=4){ u32 hh=hmix(((u>>6)&511)*977u ^ ((v>>6)*131071u) ^ 0x51u); if(hh%13==0) c=FP_CITY+(hh>>8)%4; }   // city lights at night
                    if(cl>150){ int a=(cl-150)>>2; if(a>b){ int cl_l = lv; c=FP_CLOUD+cl_l; } }
                    if(n4<5 && li>-60){ int hz=(5-n4)*3+(li>0?3:0); if(hz>b){ int al=li+70; if(al<0) al=0; int k=al*13/197+2; if(k>15) k=15; c=FP_ATM+k; } }   // haze at the rim
                }
                two |= (u16)(c<<(h*8)); }
            row[x>>1]=two; } }
}
// ---- sky: stars and the sun
#define NFS 230
static u8 fs_x[NFS], fs_y[NFS], fs_b[NFS], fs_p[NFS];
static void fl_stars_init(void){ u32 h=12345; for(int i=0;i<NFS;i++){ h=hmix(h+i*0x9E3779B9u); fs_x[i]=h%240; fs_y[i]=(h>>8)%150; fs_b[i]= ((h>>16)&7)==0 ? 14 : ((h>>16)&3)==0 ? 9 : 2+((h>>20)%6); fs_p[i]=(h>>24); } }
static void fl_stars(volatile u16 *pg, int tq){
    for(int i=0;i<NFS;i++){ int b=fs_b[i]; int tw=(sinx(((tq*3>>2)+fs_p[i]*5)&255)>>12); b+=tw; if(b<1) b=1; if(b>14) b=14;
        pix(pg,fs_x[i],fs_y[i],FP_STAR+b);
        if(fs_b[i]>=14){ int c=FP_STAR+(b>4?b-5:1); if(fs_x[i]>0) pix(pg,fs_x[i]-1,fs_y[i],c); if(fs_x[i]<239) pix(pg,fs_x[i]+1,fs_y[i],c); if(fs_y[i]>0) pix(pg,fs_x[i],fs_y[i]-1,c); pix(pg,fs_x[i],fs_y[i]+1,c); } }
}
static u8 fl_suntab[180];                                                                   // sunlight by distance from the sun's middle: 4.4 fixed
static void fl_sun_init(void){ for(int r=0;r<180;r++){ float x = r<6 ? 0 : (float)(r-6)/16.0f; float e=1.0f/(1.0f+x+x*x*0.5f+x*x*x*0.1667f);   // ~e^-x
        float v = r<6 ? 255.0f : 236.0f*e - 6.0f; if(v<0) v=0; if(v>255) v=255; fl_suntab[r]=(u8)(int)v; } }
static void fl_sun(volatile u16 *pg, int tq){
    int sx=fl_sunx, sy=fl_suny;
    int pulse=(sinx((tq>>1)&255)>>11);                                                           // it breathes a little
    for(int y=sy-90;y<sy+60;y++){ if(y<0||y>159) continue; const u8 *bz=fl_bayer+(y&3)*4;
        for(int x=sx-150;x<sx+150;x++){ if(x<0||x>239) continue; int dx=x-sx, dy=y-sy, r=isqrt((u32)(dx*dx+dy*dy*3/2)); if(r>=180) continue;
            int I=fl_suntab[r]; if(!I) continue; I=I*(64+pulse)/64; int l=(I+bz[x&3])>>4; if(l>15) l=15; if(l<1) continue;
            int cur=(x&1)?(pg[(y*240+x)>>1]>>8):(pg[(y*240+x)>>1]&255); int curb = cur>=FP_SUN && cur<FP_SUN+16 ? cur-FP_SUN : (cur>=FP_STAR && cur<FP_STAR+15 ? 2 : 0);
            if(l>curb) pix(pg,x,y,FP_SUN+l); } }
    for(int k=0;k<12;k++){ int a=(k*256/12 + (tq>>4) + (k&1)*9)&255, len = (k%3==0) ? 150 : (k%3==1) ? 60 : 95; s32 cx=cosx(a), cy_=sinx(a);   // long thin rays, slowly turning
        for(int r=8;r<len;r++){ int x=sx+((cx*r)>>14), y=sy+((cy_*r)>>14); if(x<0||x>239||y<0||y>159) break;
            int l=12-r*12/len; if(((r+x)&3)==0) l--; if(l<1) continue; int cur=(x&1)?(pg[(y*240+x)>>1]>>8):(pg[(y*240+x)>>1]&255); int curb = cur>=FP_SUN && cur<FP_SUN+16 ? cur-FP_SUN : 0;
            if(l>curb) pix(pg,x,y,FP_SUN+l); } }
}
static void fl_streak(volatile u16 *pg, int sx, int sy, int len, int top){   // the lens's flat blue streak through a bright light
    for(int x=sx-len;x<=sx+len;x++){ if(x<0||x>239) continue; int ax=x-sx; if(ax<0) ax=-ax; int l=top-ax*top/len; if(l<1) continue;
        for(int dy=-1;dy<=1;dy++){ int y=sy+dy; if(y<0||y>159) continue; int ll = dy ? l-6 : l; if(((x+y)&1) && dy) ll--; if(ll<1) continue;
            int cur=(x&1)?(pg[(y*240+x)>>1]>>8):(pg[(y*240+x)>>1]&255); if(cur>=FP_SUN+12 && cur<FP_SUN+16) continue; pix(pg,x,y,FP_STREAK+(ll>15?15:ll)); } }
}
static void fl_glow(volatile u16 *pg, int cx, int cy, int r, int top){        // a round dithered glow in the engine colours
    if(r<1) r=1; if(r>90) r=90;
    for(int y=cy-r;y<=cy+r;y++){ if(y<0||y>159) continue; const u8 *bz=fl_bayer+(y&3)*4;
        for(int x=cx-r;x<=cx+r;x++){ if(x<0||x>239) continue; int dx=x-cx, dy=y-cy, d2=dx*dx+dy*dy; if(d2>r*r) continue;
            int f=256-d2*256/(r*r); f=f*f>>8; int l=(top*f+bz[x&3]*16)>>8; if(l<1) continue; if(l>15) l=15;
            int cur=(x&1)?(pg[(y*240+x)>>1]>>8):(pg[(y*240+x)>>1]&255); if(cur>=FP_GLOW && cur<FP_GLOW+16 && cur-FP_GLOW>=l) continue; pix(pg,x,y,FP_GLOW+l); } }
}
// ---- the ship, in proper perspective (ship_draw keeps it at arm's length; this lets it come right up to the lens)
#define FL_F 160
typedef struct { float x,y,z; } V3;
static V3 v3(float x,float y,float z){ V3 r={x,y,z}; return r; }
static V3 vadd(V3 a,V3 b){ return v3(a.x+b.x,a.y+b.y,a.z+b.z); } static V3 vsub(V3 a,V3 b){ return v3(a.x-b.x,a.y-b.y,a.z-b.z); }
static V3 vmul(V3 a,float k){ return v3(a.x*k,a.y*k,a.z*k); } static float vdot(V3 a,V3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static V3 vcross(V3 a,V3 b){ return v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static V3 vnorm(V3 a){ float l=ff_sqrt(vdot(a,a)); return l>0 ? vmul(a,1.0f/l) : a; }
static int fl_proj(V3 p, int *sx, int *sy){ if(p.z<4.0f) return 0; float X=120.0f+FL_F*p.x/p.z, Y=80.0f-FL_F*p.y/p.z;
    if(X<-1900) X=-1900; if(X>1900) X=1900; if(Y<-1900) Y=-1900; if(Y>1900) Y=1900; *sx=(int)X; *sy=(int)Y; return 1; }
static V3 fl_pos, fl_fwd, fl_up, fl_rt;                              // the ship: where, and which way its nose, roof and right wing point
static V3 fl_world(int vi){ const s16 *m=&shv[vi*3]; return vadd(fl_pos, vadd(vmul(fl_rt,(float)m[0]), vadd(vmul(fl_up,(float)m[1]), vmul(fl_fwd,(float)m[2])))); }
static void fl_ship(volatile u16 *pg){
    EW static s16 px[SV_MAX], py[SV_MAX]; static u8 ok[SV_MAX]; static float vz[SV_MAX]; static s16 fz[SF_MAX]; static u8 ord[SF_MAX];
    ship_ramps();
    for(int i=0;i<snv;i++){ V3 w=fl_world(i); int a,b; ok[i]=(u8)fl_proj(w,&a,&b); px[i]=(s16)a; py[i]=(s16)b; vz[i]=w.z; }
    for(int f=0;f<snf;f++){ const Face *F=&shf[f]; float z=0; for(int k=0;k<F->n;k++) z+=vz[F->v[k]]; fz[f]=(s16)(z/F->n); ord[f]=(u8)f; }
    for(int i=1;i<snf;i++){ u8 t=ord[i]; int j=i; while(j>0 && fz[ord[j-1]]<fz[t]){ ord[j]=ord[j-1]; j--; } ord[j]=t; }   // far first
    V3 L=vnorm(v3(-0.45f,0.65f,-0.62f));                                                  // a key light from the camera's top left
    for(int o=0;o<snf;o++){ int fi=ord[o]; const Face *F=&shf[fi]; int good=1; for(int k=0;k<F->n;k++) if(!ok[F->v[k]]) good=0; if(!good) continue;
        V3 n = vadd(vmul(fl_rt,(float)shn[fi*3]), vadd(vmul(fl_up,(float)shn[fi*3+1]), vmul(fl_fwd,(float)shn[fi*3+2]))); n=vmul(n,1.0f/256.0f);
        V3 c=fl_world(F->v[0]); if(vdot(n,c)>0) n=vmul(n,-1.0f);                           // turn it to face the camera (both-sided parts)
        float d=vdot(n,L); int sh=3+(int)(d*4.6f); if(sh<0) sh=0; if(sh>7) sh=7;
        int base = F->grp==2 ? SHP_GLOW : F->grp ? SHP_TRIM : SHP_PAINT; if(F->grp==2) sh=6+((frames>>1)&1);
        int X[8], Y[8]; for(int k=0;k<F->n;k++){ X[k]=px[F->v[k]]; Y[k]=py[F->v[k]]; } polyfill(pg,X,Y,F->n,base+sh,0,0,240,160); }
    for(int i=0;i<sne;i++){ int a=she[i*3], b=she[i*3+1], g=she[i*3+2]; if(!ok[a]||!ok[b]) continue; int dx=px[b]-px[a], dy=py[b]-py[a]; if(dx>290||dx<-290||dy>290||dy<-290) continue;
        linep(pg,px[a],py[a],px[b],py[b], g==2?SHP_GLOW+6 : g?SHP_TRIM+1:SHP_PAINT+1, 0,0,240,160); }
}
// ---- the flight: keyframes on screen (x, y) and in depth, from the launch pad up past the sun and into the lens
// (time is in 30ths of a second from lift-off; every 30th is drawn FL_SUB times, so the edit can slow any part of it right down)
#ifndef FL_SUB
#define FL_SUB 4
#endif
#define NFK 8
static const s16 fk_t[NFK] ={  0,  45,  90, 125, 155, 180, 200, 206};
static const s16 fk_sx[NFK]={104, 106, 128, 146, 132, 122, 120, 120};
static const s16 fk_sy[NFK]={121,  96,  55,  36,  52,  66,  76,  86};
static const s32 fk_z[NFK] ={26000,24500,17000,8000,2800,900,150,-60};
static V3 fk_p[NFK];
static void fl_keys(void){ for(int i=0;i<NFK;i++){ float z=(float)fk_z[i]; fk_p[i] = z>0 ? v3((fk_sx[i]-120)*z/FL_F, (80-fk_sy[i])*z/FL_F, z) : v3(0,-12.0f,z); } }
static V3 fl_path(float t){                                          // a smooth curve through the keys (Catmull-Rom, uneven spacing)
    if(t<=0) return fk_p[0]; if(t>=fk_t[NFK-1]) return fk_p[NFK-1];
    int i=0; while(i<NFK-2 && t>=fk_t[i+1]) i++;
    float t0=fk_t[i], t1=fk_t[i+1], dt=t1-t0, u=(t-t0)/dt;
    V3 m0 = i>0 ? vmul(vsub(fk_p[i+1],fk_p[i-1]),dt/(float)(fk_t[i+1]-fk_t[i-1])) : vsub(fk_p[1],fk_p[0]);
    V3 m1 = i+2<NFK ? vmul(vsub(fk_p[i+2],fk_p[i]),dt/(float)(fk_t[i+2]-fk_t[i])) : vsub(fk_p[i+1],fk_p[i]);
    if(i==0) m0=v3(0,0,0);                                           // (it starts from rest)
    float u2=u*u, u3=u2*u, h00=2*u3-3*u2+1, h10=u3-2*u2+u, h01=-2*u3+3*u2, h11=u3-u2;
    return vadd(vadd(vmul(fk_p[i],h00), vmul(m0,h10)), vadd(vmul(fk_p[i+1],h01), vmul(m1,h11))); }
#define NTR 160
static V3 ftr_p[NTR]; static float ftr_t[NTR]; static int ftr_n=0;    // the trail: where the engines have been, and when
#define NSPK 60
static V3 spk_p[NSPK], spk_v[NSPK]; static u8 spk_l[NSPK];
static void fl_engines(V3 *out, int *n){                             // where the engines' nozzles are (the glowing faces)
    *n=0; for(int f=0;f<snf && *n<4;f++) if(shf[f].grp==2 && shf[f].n==8){ V3 c=v3(0,0,0); for(int k=0;k<shf[f].n;k++) c=vadd(c,fl_world(shf[f].v[k])); out[(*n)++]=vmul(c,1.0f/shf[f].n); } }
static int fl_cur(volatile u16 *pg, int x, int y){ return (x&1)?(pg[(y*240+x)>>1]>>8):(pg[(y*240+x)>>1]&255); }
static void film_launch(void){
    ship_default(); scpy(ship.name,"DAWN");
    ship.hull=HL_DART; ship.wings=WG_SWEPT; ship.wsize=2; ship.engines=1; ship.esize=2; ship.fins=2; ship.cannon=2; ship.cockpit=0; ship.len=2; ship.wid=1; ship.nose=2; ship.wpos=1; ship.dihed=2; ship.pods=1; ship.decal=1;
    ship.paint=rgb15(FL_PAINT); ship.trim=rgb15(FL_TRIM); ship.glow=rgb15(110,210,255);
    ship_build_model(); sh_rt_init();
    REG_DISPCNT=4|(1<<10); REG_BLDCNT=0; REG_BLDY=0;
    fl_palette(); ship_ramps(); fl_stars_init(); fl_sun_init(); fl_planet_init(); fl_keys();
    const int lx=104, ly=121;                                         // the launch pad, far away on the night side
    int page=1; u32 n=0; float flast=-1;
    for(u32 pic=0;;pic++){
        float T=(float)pic/(float)FL_SUB; if(T>(float)FL_TOTAL) T=(float)FL_TOTAL; int tq=(int)(T*4.0f), t=(int)T;
        float tf=T-(float)FL_LIFT, tend=(float)fk_t[NFK-1]-0.5f;            // time since lift-off; the ship is gone after tend
        volatile u16 *pg=(volatile u16*)(0x06000000 + (page?0xA000:0));
        { static u32 zero=0; *(volatile u32*)0x040000D4=(u32)&zero; *(volatile u32*)0x040000D8=(u32)pg; *(volatile u32*)0x040000DC=(240*160/4)|(2<<23)|(1<<26)|(1u<<31); }
        fl_stars(pg,tq); fl_sun(pg,tq); fl_planet(pg,tq); frames=t;
        int live = tf>=0 && tf<tend;
        if(tf<tend){ float tt=tf<0?0:tf; fl_pos=fl_path(tt); V3 a=fl_path(tt+0.5f), b=fl_path(tt-0.5f<0?0:tt-0.5f);
            fl_fwd = tf<1.0f ? v3(0.05f,1,-0.2f) : vsub(a,b); fl_fwd=vnorm(fl_fwd);
            float fy=fl_fwd.y<0?-fl_fwd.y:fl_fwd.y; V3 ref=vnorm(vadd(vmul(v3(0,1,0),1.0f-fy), vmul(v3(0,0,-1),fy)));
            fl_rt=vnorm(vcross(ref,fl_fwd)); fl_up=vcross(fl_fwd,fl_rt);
            float roll = live ? 0.5f*(float)sinx((int)(tt*1.1f)&255)/16384.0f : 0;   // a slow bank on the way
            { float c=1.0f-roll*roll*0.5f; V3 r2=vadd(vmul(fl_rt,c), vmul(fl_up,roll)), u2=vadd(vmul(fl_up,c), vmul(fl_rt,-roll)); fl_rt=vnorm(r2); fl_up=vnorm(u2); } }
        V3 eng[4]; int ne=0; if(live) fl_engines(eng,&ne);
        V3 ec=v3(0,0,0); for(int k=0;k<ne;k++) ec=vadd(ec,eng[k]); if(ne) ec=vmul(ec,1.0f/ne);
        if(live && T-flast>=0.999f){ flast=T; if(ftr_n<NTR) ftr_n++; for(int k=ftr_n-1;k>0;k--){ ftr_p[k]=ftr_p[k-1]; ftr_t[k]=ftr_t[k-1]; } ftr_p[0]=ec; ftr_t[0]=T; }
        // the trail: bright and thin near the ship, wider and fading as it cools
        for(int k=ftr_n-1;k>=0;k--){ V3 p0=ftr_p[k], p1 = k ? ftr_p[k-1] : ec; if(!k && !live) continue;
            if(p0.z<400.0f || p1.z<400.0f) continue;
            int a,b,c,d; if(!fl_proj(p0,&a,&b) || !fl_proj(p1,&c,&d)) continue;
            float age=T-ftr_t[k]; int top = 15 - (int)(age/6.0f) - (p0.z<2500.0f ? (int)((2500.0f-p0.z)/180.0f) : 0); if(top<2) continue;
            int r = (int)(FL_F*(8.0f+age*1.6f)/p0.z); if(r>7) r=7;
            int steps = c-a; if(steps<0) steps=-steps; int s2=d-b; if(s2<0) s2=-s2; if(s2>steps) steps=s2; if(steps>80) steps=80; if(steps<1) steps=1;
            for(int q=0;q<steps;q++){ int x=a+(c-a)*q/steps, y=b+(d-b)*q/steps;
                if(r<=1){ if(x>=0&&x<240&&y>=0&&y<160){ int cur=fl_cur(pg,x,y); if(!(cur>=FP_GLOW && cur<FP_GLOW+16 && cur-FP_GLOW>=top)) pix(pg,x,y,FP_GLOW+top); } }
                else fl_glow(pg,x,y,r,top); } }
        if(T>=FL_IGN && tf<30){ float k=T-FL_IGN; int r=2+(int)(k/3.0f); if(r>10) r=10; if(tf>10) r=r*(int)(30-tf)/20+1; fl_glow(pg,lx,ly,r,15);   // ignition on the pad
            for(int q=0;q<18;q++){ int a=(q*256/18+(int)k)&255, rr=(int)(k*0.7f)+((q*7)&3); int x=lx+((cosx(a)*rr)>>14), y=ly+((sinx(a)*rr)>>15); int l=12-(int)(k/3.0f); if(l>1 && x>=0&&x<240&&y>=0&&y<160) pix(pg,x,y,FP_DUST+l); } }
        if(live){                                                           // the engines' light, then the ship over it
            float facing=vdot(fl_fwd,v3(0,0,-1));
            for(int k=0;k<ne;k++){ int a,b; if(!fl_proj(eng[k],&a,&b)) continue; float z=eng[k].z; int r=(int)(FL_F*(facing>0.5f?34.0f:40.0f)/z)+2; fl_glow(pg,a,b,r,facing>0.5f?12:15); fl_glow(pg,a,b,r/3+1,15); }
            fl_ship(pg);
            if(facing<0.6f) for(int k=0;k<ne;k++){ int a,b; if(!fl_proj(eng[k],&a,&b)) continue; V3 back=vsub(eng[k],vmul(fl_fwd,40.0f)); int c,d; if(!fl_proj(back,&c,&d)) continue;   // flames out of the back
                float z=eng[k].z; int r=(int)(FL_F*14.0f/z)+1; fl_glow(pg,(a+c)/2,(b+d)/2,r,14); }
            if(T-flast<0.01f) for(int q=0;q<NSPK;q++){ if(spk_l[q]==0 && ne){ spk_p[q]=eng[q%ne]; spk_v[q]=vadd(vmul(fl_fwd,-40.0f-(float)(q%7)*6.0f), v3((float)((int)(hmix(q*31+t)%41)-20),(float)((int)(hmix(q*17+t*3)%41)-20),(float)((int)(hmix(q*7+t*5)%41)-20))); spk_l[q]=6+q%9; break; } }
        }
        if(T-flast<0.01f || !live) for(int q=0;q<NSPK;q++) if(spk_l[q]){ spk_l[q]--; spk_p[q]=vadd(spk_p[q],spk_v[q]); }
        for(int q=0;q<NSPK;q++) if(spk_l[q]){ int a,b; if(fl_proj(spk_p[q],&a,&b) && a>=0&&a<240&&b>=0&&b<160) pix(pg,a,b,FP_GLOW+6+spk_l[q]); }
        // lens streaks: the sun's, and the engines' once they face us
        fl_streak(pg,fl_sunx,fl_suny,120,13);
        if(live && ne){ int a,b; if(fl_proj(ec,&a,&b)){ float z=ec.z; float facing=vdot(fl_fwd,v3(0,0,-1)); int top=(int)(4.0f+40000.0f/(z+2000.0f)); if(top>15) top=15; if(facing>0.3f && z<2600.0f && top>5) fl_streak(pg,a,b,24+(int)(120000.0f/(z+1500.0f)),top-(int)(z/400.0f)); } }
        vsync(); REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(page?(1<<4):0); page^=1; FILM_FLAG=++n;
    }
}
#ifdef TEST_SHIPS
// (for choosing the hero ship: each picture shows one design from the front, nose-on and from behind)
static void film_ships(void){
    static const u8 D[][13]={ // hull wings engines fins cannon pods decal wsize len wid nose dihed esize
        {HL_DART,WG_X,3,2,2,1,1,2,2,1,0,1,2},{HL_WEDGE,WG_DELTA,1,2,2,0,2,2,2,1,0,1,2},{HL_NEEDLE,WG_SWEPT,1,2,1,1,1,2,2,1,0,1,2},{HL_DART,WG_DELTA,2,2,3,0,1,2,2,1,0,1,2},
        {HL_WEDGE,WG_X,3,2,2,0,2,1,2,1,0,1,2},{HL_NEEDLE,WG_X,3,1,2,0,1,2,2,0,0,1,2},{HL_DART,WG_STRAIGHT,1,2,2,1,1,2,2,1,0,1,2},{HL_DART,WG_SWEPT,1,2,2,1,1,2,2,1,2,2,2}};
    REG_DISPCNT=4|(1<<10); fl_palette(); sh_rt_init(); int page=1; u32 n=0;
    for(int d=0;;d=(d+1)%8){
        ship_default(); ship.hull=D[d][0]; ship.wings=D[d][1]; ship.engines=D[d][2]; ship.fins=D[d][3]; ship.cannon=D[d][4]; ship.pods=D[d][5]; ship.decal=D[d][6]; ship.wsize=D[d][7]; ship.len=D[d][8]; ship.wid=D[d][9]; ship.nose=D[d][10]; ship.dihed=D[d][11]; ship.esize=D[d][12];
        ship.paint=rgb15(FL_PAINT); ship.trim=rgb15(FL_TRIM); ship.glow=rgb15(110,210,255); ship_build_model();
        for(int rep=0;rep<2;rep++){
            volatile u16 *pg=(volatile u16*)(0x06000000 + (page?0xA000:0)); for(int i=0;i<120*160;i++) pg[i]=0x0101*(FP_ATM+1);
            static const float P[2][4]={{0.25f,-0.35f,-1.0f,62},{-0.1f,-0.25f,-1.0f,178}};
            for(int k=0;k<2;k++){ float z=k?230.0f:300.0f; fl_pos=v3((P[k][3]-120.0f)*z/FL_F, (80.0f-80.0f)*z/FL_F, z); fl_fwd=vnorm(v3(P[k][0],P[k][1],P[k][2]));
                float fy=fl_fwd.y<0?-fl_fwd.y:fl_fwd.y; V3 ref=vnorm(vadd(vmul(v3(0,1,0),1.0f-fy), vmul(v3(0,0,-1),fy))); fl_rt=vnorm(vcross(ref,fl_fwd)); fl_up=vcross(fl_fwd,fl_rt);
                fl_ship(pg); }
            char b[8]; b[0]='A'+d; b[1]=0; text(pg,4,4,b,FP_SUN+15,2);
            vsync(); REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(page?(1<<4):0); page^=1; FILM_FLAG=++n; }
    }
}
#endif
