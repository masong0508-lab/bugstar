// pc_snative.inc: the SPACE view (stars, sun, planet and moons) at the window's own resolution (NATIVE 3D VIEWS). Included at the end of the game's main.c.
// space_frame draws the background as always; right after the moons it calls pc_sn_native, which makes a second, window-size picture of the same background from
// the game's own camera (br/bu/bf), star list, sun and the planet_fill calls it just made (recorded by pc_sn_rec): stars as soft dots and streaks at their exact
// sub-pixel place, the sun as a smooth disc with glow, planets and moons shaded per window pixel from the game's own sphere tables. It only READS the game.
// pc_sncomp.inc (in pc_hw.c) shows it where the page still holds what was drawn at that moment (ships, rocks, station, HUD, cockpit stay the game's own).
#include <stdlib.h>
typedef struct { int w,h,hn; uint32_t *buf; long f[2]; uint8_t snap[2][240*128]; uint8_t ok[256]; int sunx,suny; uint8_t snap2[2][240*128]; uint8_t *msk; long f2[2]; uint32_t *ibuf; uint8_t foot[2][240*128], fval[2][240*128]; long ff[2]; uint8_t snapc[2][240*128]; long fc[2]; int cpmode[2]; } PcSNat;
extern double pc_now_ms(void); extern void pc_vrec(int,double,double); extern double pc_cpu_ms(void); extern PcSNat pc_sn; extern int pc_nat_w, pc_nat_h; extern int pc_v3d_classic(void), pc_v3d_ultra(void); extern long pc_frames(void);
typedef void (*PcParFn3)(void*,int,int); extern void pc_par_chunk(int total, int chunk, PcParFn3 fn, void *ctx);
typedef struct { int cx,cy,r,Lx,Ly,Lz,rot; const u8 *noise; } SnCall;
static SnCall sn_calls[40]; static int sn_nc, sn_rec; static uint32_t sn_pal[256];
static void pc_sn_begin(void){ sn_nc=0; sn_rec = pc_nat_w>240 && pc_nat_h>160 && !pc_v3d_classic(); }
static void pc_sn_rec(int cx,int cy,int r,int Lx,int Ly,int Lz,int rot,const u8 *noise){ if(!sn_rec || sn_nc>=40) return; SnCall *c=&sn_calls[sn_nc++]; c->cx=cx; c->cy=cy; c->r=r; c->Lx=Lx; c->Ly=Ly; c->Lz=Lz; c->rot=rot; c->noise=noise; }
typedef struct { int W,H; double sx,sy; SnCall *c; uint32_t *buf; } SnCtx;
static inline uint32_t sn_mix(uint32_t a, uint32_t b, int f){ if(f<0) f=0; if(f>256) f=256; uint32_t rb=((a&0xFF00FF)*(256-f)+(b&0xFF00FF)*f)>>8, g=((a&0xFF00)*(256-f)+(b&0xFF00)*f)>>8; return (rb&0xFF00FF)|(g&0xFF00); }
// a soft round dot / thick segment with coverage, over what is there
static void sn_seg(uint32_t *buf, int W, int H, double x0, double y0, double x1, double y1, double rad, uint32_t col, double a0, double a1){
    double lo_x=(x0<x1?x0:x1)-rad-1, hi_x=(x0<x1?x1:x0)+rad+1, lo_y=(y0<y1?y0:y1)-rad-1, hi_y=(y0<y1?y1:y0)+rad+1;
    int xa=(int)lo_x, xb=(int)hi_x+1, ya=(int)lo_y, yb=(int)hi_y+1; if(xa<0) xa=0; if(ya<0) ya=0; if(xb>W) xb=W; if(yb>H) yb=H;
    double dx=x1-x0, dy=y1-y0, l2=dx*dx+dy*dy;
    for(int Y=ya;Y<yb;Y++) for(int X=xa;X<xb;X++){ double px=X+0.5-x0, py=Y+0.5-y0, t= l2>1e-9 ? (px*dx+py*dy)/l2 : 0; if(t<0) t=0; if(t>1) t=1;
        double ex=px-t*dx, ey=py-t*dy, d=__builtin_sqrt(ex*ex+ey*ey); double a=(1.0-d/(rad+0.5)); if(a<=0) continue; a*=a0+(a1-a0)*t; if(a>1) a=1;
        uint32_t *p=&buf[(size_t)Y*W+X]; *p=sn_mix(*p,col,(int)(a*256)); }
}
static void sn_rows(void *vc, int lo, int hi){
    SnCtx *k=(SnCtx*)vc; SnCall *c=k->c; int W=k->W; double sx=k->sx, sy=k->sy; double R=c->r+0.5, cxp=c->cx+0.5, cyp=c->cy+0.5;
    int y0=(int)((cyp-R-1)*sy); if(y0<0) y0=0; int x0=(int)((cxp-R-1)*sx); if(x0<0) x0=0; int x1=(int)((cxp+R+1)*sx)+1; if(x1>W) x1=W;
    static const double dummy=0; (void)dummy;
    for(int Y=y0+lo;Y<y0+hi;Y++){ double gy=(Y+0.5)/sy, ny=(gy-cyp)/R;
        for(int X=x0;X<x1;X++){ double gx=(X+0.5)/sx, nx=(gx-cxp)/R, q=nx*nx+ny*ny; if(q>=1.0) continue; double d=__builtin_sqrt(q);
            double a=(1.0-d)*R*sx+0.5; if(a<=0) continue; if(a>1) a=1;
            double kx=(nx+1)*64-0.5, ky=(ny+1)*64-0.5; int kx0=(int)__builtin_floor(kx), ky0=(int)__builtin_floor(ky); double fx=kx-kx0, fy=ky-ky0;
            double sh=0, nzs=0, lits=0;
            for(int j=0;j<4;j++){ int xx=kx0+(j&1), yy=ky0+(j>>1); double w=((j&1)?fx:1-fx)*((j>>1)?fy:1-fy); if(xx<0) xx=0; if(xx>127) xx=127; if(yy<0) yy=0; if(yy>127) yy=127;
                int kk=(yy<<7)|xx, nz=sph_z[kk]; double s; int lit;
                if(nz<10){ s=-1; lit=0; } else {
                    int nx_=(xx-64)*4, ny_=(64-yy)*4; lit=(nx_*c->Lx + ny_*c->Ly - nz*c->Lz)>>14; int v=sph_v[kk], u=sph_u[kk]; int t=c->noise[(v<<7)|((u+c->rot)&127)];
                    int base = t>=8 ? 50+t*4 : 14+t*3; if(v<4 || v>59) base=100+t;
                    int cl=(pl_noise+16384)[(v<<7)|((u+c->rot+(int)(vbl_count>>5)+40-c->rot)&127)]; (void)cl;
                    int crot=c->rot+(int)(vbl_count>>5)+40; cl=(pl_noise+16384)[(v<<7)|((u+crot)&127)];
                    if(cl>10) base += ((cl-9)*(124-base))>>3;
                    int dd=lit+40; if(dd<0) dd=0; if(dd>256) dd=256; s=((base*dd)>>8)+4; if(s>127) s=127; }
                sh+=w*s; nzs+=w*nz; lits+=w*lit; }
            uint32_t col;
            if(sh<0 || nzs<10) col=sn_pal[48]; else {
                double lv=sh*113.0/128.0/16.0; if(lv<0) lv=0; if(lv>7) lv=7; int i=(int)lv; if(i>6) i=6; col=sn_mix(sn_pal[40+i],sn_pal[41+i],(int)((lv-i)*256));
                if(nzs<90 && lits>-50){ double hz=(90-nzs)/64.0; col=sn_mix(col,sn_pal[48],(int)(hz*256)); } }
            uint32_t *p=&k->buf[(size_t)Y*W+X]; *p = a>=1 ? col : sn_mix(*p,col,(int)(a*256)); } }
}
static void pc_sn_native(volatile unsigned short *pg, int vpx, int vpy, int stk){
    int W=pc_nat_w, H=pc_nat_h; pc_sn.w=0; if(W<=240 || H<=160 || pc_v3d_classic()) return; double sn_t0=pc_now_ms(), sn_c0=pc_cpu_ms();
    int hn=(128*H+159)/160; if(hn>H) hn=H; static unsigned long bcap; unsigned long need=(unsigned long)W*hn; if(need>bcap){ free(pc_sn.buf); free(pc_sn.msk); free(pc_sn.ibuf); pc_sn.buf=malloc(need*4); pc_sn.ibuf=malloc(need*4); pc_sn.msk=malloc(need); bcap=need; } memset(pc_sn.msk,0,need); memset(pc_sn.ibuf,0,need*4);
    for(int i=0;i<256;i++){ u16 c=BG_PAL[i]; int r=c&31, g=(c>>5)&31, b=(c>>10)&31; sn_pal[i]=(uint32_t)(r<<3|r>>2)<<16|(uint32_t)(g<<3|g>>2)<<8|(uint32_t)(b<<3|b>>2); }
    double sx=W/240.0, sy=H/160.0; uint32_t *buf=pc_sn.buf; uint32_t bk=sn_pal[0]; for(size_t i=0;i<need;i++) buf[i]=bk;
    pc_sg_space_back(buf,W,hn,sx,sy);          // nebulae and a far galaxy behind the stars (pc_sprgen_game.inc)
    // stars: exact sub-pixel places, soft dots, streaks when fast
    for(int i=0;i<96;i++){ s16 *d=&star_dir[i*3]; s32 z=(d[0]*bf[0]+d[1]*bf[1]+d[2]*bf[2])>>14; if(z<2000) continue;
        s32 x=(d[0]*br[0]+d[1]*br[1]+d[2]*br[2])>>14, y=(d[0]*bu[0]+d[1]*bu[1]+d[2]*bu[2])>>14;
        double fx=120.5+(double)x*FOCS/z, fy=VCY+0.5-(double)y*FOCS/z; int ci=i<20?5 : i<50?4 : 3; uint32_t col=sn_pal[ci]; double rad=(i<20?0.55:i<50?0.45:0.38)*sx+0.3;
        double tw=0.85+0.15*__builtin_cos(vbl_count*0.05+i*1.7);
        if(stk && i<70){ double ex=fx-((fx-vpx-0.5)*stk/256.0), ey=fy-((fy-vpy-0.5)*stk/256.0); sn_seg(buf,W,hn,fx*sx,fy*sy,ex*sx,ey*sy,rad*0.8,col,1.0,0.35); }
        else sn_seg(buf,W,hn,fx*sx,fy*sy,fx*sx,fy*sy,rad,col,tw,tw); }
    // the sun
    pc_sn.sunx=-999; pc_sn.suny=-999;
    { s32 z=(sun[0]*bf[0]+sun[1]*bf[1]+sun[2]*bf[2])>>14;
      if(z>2000){ s32 x=(sun[0]*br[0]+sun[1]*br[1]+sun[2]*bu[0]*0+sun[2]*br[2]*0)>>14; x=(sun[0]*br[0]+sun[1]*br[1]+sun[2]*br[2])>>14; s32 y=(sun[0]*bu[0]+sun[1]*bu[1]+sun[2]*bu[2])>>14;
          int gx=120+x*FOCS/z, gy=VCY-y*FOCS/z; pc_sn.sunx=gx; pc_sn.suny=gy; double px=(gx+0.5)*sx, py=(gy+0.5)*sy;
          int ex=(int)(18*sx)+2, ey=(int)(18*sy)+2;
          for(int Y=(int)py-ey;Y<=(int)py+ey;Y++){ if(Y<0||Y>=hn) continue; for(int X=(int)px-ex;X<=(int)px+ex;X++){ if(X<0||X>=W) continue;
              double dx=(X+0.5-px)/sx, dy=(Y+0.5-py)/sy, d=__builtin_sqrt(dx*dx+dy*dy); uint32_t *p=&buf[(size_t)Y*W+X];
              if(d>12.3){ double g=1.0-(d-12.3)/5.7; if(g>0){ g=g*g*0.4; *p=sn_mix(*p,sn_pal[50],(int)(g*256)); } continue; }
              uint32_t col = d<4.5 ? sn_pal[49] : d<7.5 ? sn_mix(sn_pal[49],sn_pal[50],(int)((d-4.5)/3*256)) : d<10.5 ? sn_mix(sn_pal[50],sn_pal[51],(int)((d-7.5)/3*256)) : sn_pal[51];
              double a=(12.3-d)*sx; if(a>1) a=1; *p=sn_mix(*p,col,(int)(a*256)); } } } }
    // planets and moons, in the order the game drew them
    for(int i=0;i<sn_nc;i++){ SnCall *c=&sn_calls[i]; SnCtx k; k.W=W; k.H=hn; k.sx=sx; k.sy=sy; k.c=c; k.buf=buf;
        int y0=(int)((c->cy+0.5-c->r-1.5)*sy); if(y0<0) y0=0; int y1=(int)((c->cy+0.5+c->r+1.5)*sy)+1; if(y1>hn) y1=hn; if(y1<=y0) continue;
        pc_par_chunk(y1-y0,8,sn_rows,&k); pc_sg_space_planet(buf,W,hn,sx,sy,c,i); }
    memset(pc_sn.ok,0,256); static const unsigned char okv[]={0,3,4,5,40,41,42,43,44,45,46,47,48,49,50}; for(unsigned i=0;i<sizeof okv;i++) pc_sn.ok[okv[i]]=1;
    int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0; pc_sn.w=W; pc_sn.h=H; pc_sn.hn=hn; memcpy(pc_sn.snap[t],(const void*)pg,240*128); pc_sn.f[t]=pc_frames(); pc_vrec(2,pc_now_ms()-sn_t0,pc_cpu_ms()-sn_c0);
}

// the rest of the space view's wireframes (dust, station, rocks) at window resolution: drawn onto the same picture as the background (anti-aliased), and a mask
// marks where the game's blocky copy is hidden. Called right after the game drew them (a second page copy is kept: pc_sn.snap2).
static void sn_mseg(uint8_t *m, int W, int H, double x0, double y0, double x1, double y1, double rad){
    int xa=(int)((x0<x1?x0:x1)-rad-1), xb=(int)((x0<x1?x1:x0)+rad+1)+1, ya=(int)((y0<y1?y0:y1)-rad-1), yb=(int)((y0<y1?y1:y0)+rad+1)+1; if(xa<0) xa=0; if(ya<0) ya=0; if(xb>W) xb=W; if(yb>H) yb=H;
    double dx=x1-x0, dy=y1-y0, l2=dx*dx+dy*dy;
    for(int Y=ya;Y<yb;Y++) for(int X=xa;X<xb;X++){ double px=X+0.5-x0, py=Y+0.5-y0, t= l2>1e-9 ? (px*dx+py*dy)/l2 : 0; if(t<0) t=0; if(t>1) t=1; double ex=px-t*dx, ey=py-t*dy; if(ex*ex+ey*ey<=rad*rad) m[(size_t)Y*W+X]=1; }
}
static int so_proj(s32 x,s32 y,s32 z,double *fx,double *fy){ s32 cx,cy,cz; if(!cam3(x,y,z,&cx,&cy,&cz)) return 0; double X=120.5+(double)cx*FOCS/cz, Y=VCY+0.5-(double)cy*FOCS/cz; if(X<-4000||X>4000||Y<-4000||Y>4000) return 0; *fx=X; *fy=Y; return 1; }
static void so_line(double x0,double y0,double x1,double y1,int ci){
    if(!pc_sn.w) return; int W=pc_sn.w, H=pc_sn.hn; double sx=W/240.0, sy=pc_nat_h/160.0;
    sn_seg(pc_sn.buf,W,H,x0*sx,y0*sy,x1*sx,y1*sy,0.5*sx+0.15,sn_pal[ci],1.0,1.0); sn_mseg(pc_sn.msk,W,H,x0*sx,y0*sy,x1*sx,y1*sy,1.1*sx+0.5);
}
static void so_wire(const s32 *v,int nv,const u8 *e,int ne,int c){ double X[16],Y[16]; u8 ok[16]; for(int i=0;i<nv;i++) ok[i]=so_proj(v[i*3],v[i*3+1],v[i*3+2],&X[i],&Y[i]);
    for(int i=0;i<ne;i++){ int a=e[i*2], b=e[i*2+1]; if(ok[a]&&ok[b]) so_line(X[a],Y[a],X[b],Y[b],c); } }

// ---- the things the game draws AFTER the wireframes (cargo cans, sparks, the cockpit's laser bolts, the crosshair, and in the chase view the bolts): drawn here into a
// separate layer (pc_sn.ibuf: premultiplied colour, alpha in the top byte; alpha 0 with colour = pure glow), at the window's own resolution. The game's own blocky copy is
// found exactly by letting the game's own dot2/line2 draw the same things onto a scratch page (so_scr): pc_sn.foot says which game pixels they cover and pc_sn.fval what they wrote
// there. The compositor shows the window picture's background in those pixels while the page still holds that value (so_foot_still), and puts this layer over everything
// (ships included) afterwards. Anything the game draws later on top (HUD, text, canopy) changes the page there, and the layer is not shown.
static volatile u16 so_scr[120*128];
static void so_foot_begin(void){ memset((void*)so_scr,0,sizeof so_scr); }
static void so_foot_end(int t){ const uint8_t *b=(const uint8_t*)so_scr; for(int i=0;i<240*128;i++) if(b[i]){ pc_sn.foot[t][i]=1; pc_sn.fval[t][i]=b[i]; } }
static inline void io_over(uint32_t *p, uint32_t col, double a){ if(a<=0) return; if(a>1) a=1; uint32_t d=*p; int ia=(int)(a*256);
    int da=d>>24, dr=d>>16&255, dg=d>>8&255, db=d&255; int k=256-ia;
    int ra=(ia*255+da*k)>>8; int rr=((col>>16&255)*ia+dr*k)>>8, gg=((col>>8&255)*ia+dg*k)>>8, bb=((col&255)*ia+db*k)>>8; if(ra>255) ra=255; *p=(uint32_t)ra<<24|(uint32_t)rr<<16|(uint32_t)gg<<8|(uint32_t)bb; }
static inline void io_add(uint32_t *p, uint32_t col, double f){ if(f<=0) return; int i=(int)(f*256); uint32_t d=*p; int r=(d>>16&255)+((col>>16&255)*i>>8), g=(d>>8&255)+((col>>8&255)*i>>8), b=(d&255)+((col&255)*i>>8); if(r>255) r=255; if(g>255) g=255; if(b>255) b=255; *p=(d&0xFF000000u)|(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; }
// a line (hw = half thickness, native pixels) with an anti-aliased edge, over the layer; gl/ga: an additive glow that many pixels wide (ULTRA)
static void io_line(double x0,double y0,double x1,double y1,double hw,uint32_t col,double alpha,double gl,double ga){
    int W=pc_sn.w, H=pc_sn.hn; double ex=hw+gl+1.5; int xa=(int)((x0<x1?x0:x1)-ex), xb=(int)((x0<x1?x1:x0)+ex)+1, ya=(int)((y0<y1?y0:y1)-ex), yb=(int)((y0<y1?y1:y0)+ex)+1; if(xa<0) xa=0; if(ya<0) ya=0; if(xb>W) xb=W; if(yb>H) yb=H;
    double dx=x1-x0, dy=y1-y0, l2=dx*dx+dy*dy;
    for(int Y=ya;Y<yb;Y++) for(int X=xa;X<xb;X++){ double px=X+0.5-x0, py=Y+0.5-y0, t= l2>1e-9 ? (px*dx+py*dy)/l2 : 0; if(t<0) t=0; if(t>1) t=1; double qx=px-t*dx, qy=py-t*dy, d=__builtin_sqrt(qx*qx+qy*qy);
        uint32_t *p=&pc_sn.ibuf[(size_t)Y*W+X]; if(gl>0 && d<hw+gl){ double g=1.0-(d-hw>0?d-hw:0)/gl; io_add(p,col,g*g*ga); } double c=hw-d+0.5; if(c>0) io_over(p,col,(c>1?1:c)*alpha); } }
static void io_dot(double cx,double cy,double r,uint32_t col,double alpha,double gl,double ga){ io_line(cx,cy,cx,cy,r,col,alpha,gl,ga); }
static void so_markline(double x0,double y0,double x1,double y1,double rad){ sn_mseg(pc_sn.msk,pc_sn.w,pc_sn.hn,x0,y0,x1,y1,rad); }
static void so_items(int t){
    int W=pc_sn.w; double sx=W/240.0, sy=pc_nat_h/160.0; int ultra=pc_v3d_ultra(); (void)W;
    memset(pc_sn.foot[t],0,240*128); memset(pc_sn.fval[t],0,240*128); so_foot_begin(); pc_sn.ff[t]=pc_frames();
    // cargo cans: a turning diamond (the game's four lines), with a pulse and, in ULTRA, a glow
    for(int k=0;k<4;k++) if(cans[k].alive){ s32 cx,cy,cz; if(!cam3(cans[k].x,cans[k].y,cans[k].z,&cx,&cy,&cz)) continue; int sx_,sy_; s32 z; if(!sproj(cans[k].x,cans[k].y,cans[k].z,&sx_,&sy_,&z)) continue;
        int r=(int)(60*FOCS/z)+1; line2(so_scr,sx_-r,sy_,sx_,sy_-r,50); line2(so_scr,sx_,sy_-r,sx_+r,sy_,50); line2(so_scr,sx_+r,sy_,sx_,sy_+r,50); line2(so_scr,sx_,sy_+r,sx_-r,sy_,50);
        double fx=120.5+(double)cx*FOCS/cz, fy=VCY+0.5-(double)cy*FOCS/cz, R=(60.0*FOCS/cz+0.6), pul=0.75+0.25*__builtin_sin(vbl_count*0.12+k*2.0); uint32_t col=sn_pal[50], hot=sn_pal[49];
        double hw=0.30*sx+0.4; double X=fx*sx, Y=fy*sy, RX=R*sx, RY=R*sy;
        io_line(X-RX,Y,X,Y-RY,hw,col,1.0,ultra?1.6*sx:0,0.45*pul); io_line(X,Y-RY,X+RX,Y,hw,col,1.0,ultra?1.6*sx:0,0.45*pul); io_line(X+RX,Y,X,Y+RY,hw,col,1.0,ultra?1.6*sx:0,0.45*pul); io_line(X,Y+RY,X-RX,Y,hw,col,1.0,ultra?1.6*sx:0,0.45*pul);
        io_dot(X,Y,0.42*sx*pul+0.5,hot,0.9,ultra?3.0*sx:0,0.5);                                                    // a bright core: the thing to scoop
        so_markline(X-RX,Y,X,Y-RY,hw+ (ultra?1.6*sx:0)+1); so_markline(X,Y-RY,X+RX,Y,hw+(ultra?1.6*sx:0)+1); so_markline(X+RX,Y,X,Y+RY,hw+(ultra?1.6*sx:0)+1); so_markline(X,Y+RY,X-RX,Y,hw+(ultra?1.6*sx:0)+1); }
    // sparks: a bright head and a short streak behind it (where it was), fading out with its life
    for(int i=0;i<NSPP;i++){ SPart *p=&spp[i]; if(p->life<=0) continue; int sx_,sy_; if(!sproj(p->x,p->y,p->z,&sx_,&sy_,0)) continue; dot2(so_scr,sx_,sy_,p->life>12?49:p->col); dot2(so_scr,sx_+1,sy_,p->col);
        double fx,fy,tx,ty; if(!so_proj(p->x,p->y,p->z,&fx,&fy)) continue; fx+=0.5; double a=p->life>12?1.0:p->life/12.0; uint32_t col=sn_pal[p->col], hot=sn_pal[49];
        if(so_proj(p->x-p->vx*2,p->y-p->vy*2,p->z-p->vz*2,&tx,&ty)){ tx+=0.5; double dd=(tx-fx)*(tx-fx)+(ty-fy)*(ty-fy); if(dd<110) io_line(fx*sx,fy*sy,tx*sx,ty*sy,0.2*sx+0.3,col,0.5*a,0,0); }
        io_dot(fx*sx,fy*sy,0.55*sx+0.4,p->life>12?hot:col,a,ultra?3.2*sx:0,0.5*a); so_markline(fx*sx,fy*sy,fx*sx,fy*sy,2.2*sx+(ultra?3.2*sx:0)); }
    // the cockpit view's laser bolts, from the bottom corners to the crosshair
    int lz=laser_t; if(lz && !chase_cam){ line2(so_scr,30,127,120,VCY,52); line2(so_scr,210,127,120,VCY,52); double a=lz/4.0; if(a>1) a=1; uint32_t col=sn_pal[52], hot=sn_pal[54];
        for(int s=0;s<2;s++){ double x0=(s?210:30)+0.5, y0=127.5, x1=120.5, y1=VCY+0.5; io_line(x0*sx,y0*sy,x1*sx,y1*sy,0.45*sx+0.5,col,0.9*a,ultra?2.5*sx:0,0.4*a); io_line(x0*sx,y0*sy,x1*sx,y1*sy,0.15*sx+0.4,hot,a,0,0);
            so_markline(x0*sx,y0*sy,x1*sx,y1*sy,0.45*sx+0.5+(ultra?2.5*sx:0)+1); } }
    // the crosshair: four short arms and, in ULTRA, a ring and a dot
    if(!FILM){ for(int k=4;k<9;k++){ dot2(so_scr,120+k,VCY,52); dot2(so_scr,120-k,VCY,52); dot2(so_scr,120,VCY+k,52); dot2(so_scr,120,VCY-k,52); }
        double cx=120.5*sx, cy=(VCY+0.5)*sy; uint32_t col=sn_pal[52]; double hw=0.17*sx+0.5;
        io_line(124.0*sx,cy,129.0*sx,cy,hw,col,1.0,ultra?1.4*sx:0,0.4); io_line(112.0*sx,cy,117.0*sx,cy,hw,col,1.0,ultra?1.4*sx:0,0.4);
        io_line(cx,(VCY-8.0)*sy,cx,(VCY-3.0)*sy,hw,col,1.0,ultra?1.4*sx:0,0.4); io_line(cx,(VCY+4.0)*sy,cx,(VCY+9.0)*sy,hw,col,1.0,ultra?1.4*sx:0,0.4);
        if(ultra){ io_dot(cx,cy,0.28*sx+0.6,col,0.9,1.6*sx,0.5); }
        so_markline(112.0*sx,cy,129.0*sx,cy,hw+(ultra?1.4*sx:0)+1); so_markline(cx,(VCY-8.0)*sy,cx,(VCY+9.0)*sy,hw+(ultra?1.4*sx:0)+1); }
    so_foot_end(t);
}
// the chase view's bolts (the game flies them from the ship's guns to the crosshair): recorded where the game draws them, drawn by pc_so_late right after the chase ship
static int sob_n; static int sob[8][4];
static void pc_so_bolt(int tx,int ty,int hx,int hy){ if(!pc_sn.w || sob_n>=8) return; sob[sob_n][0]=tx; sob[sob_n][1]=ty; sob[sob_n][2]=hx; sob[sob_n][3]=hy; sob_n++; }
static void pc_so_late(volatile unsigned short *pg){
    if(!pc_sn.w || !pc_sn.msk){ sob_n=0; return; } int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0; double sx=pc_sn.w/240.0, sy=pc_nat_h/160.0; int ultra=pc_v3d_ultra(); so_foot_begin();
    for(int n=0;n<sob_n;n++){ int tx=sob[n][0], ty=sob[n][1], hx=sob[n][2], hy=sob[n][3];
        line2(so_scr,tx,ty,hx,hy,52); line2(so_scr,tx+1,ty,hx+1,hy,52); line2(so_scr,(tx+hx)/2,(ty+hy)/2,hx,hy,54);
        double x0=tx+1.0, y0=ty+0.5, x1=hx+1.0, y1=hy+0.5; uint32_t col=sn_pal[52], hot=sn_pal[54];
        io_line(x0*sx,y0*sy,x1*sx,y1*sy,0.5*sx+0.4,col,0.92,ultra?2.5*sx:0,0.45); io_line((x0+x1)*0.5*sx,(y0+y1)*0.5*sy,x1*sx,y1*sy,0.22*sx+0.4,hot,1.0,0,0);
        so_markline(x0*sx,y0*sy,x1*sx,y1*sy,0.5*sx+0.4+(ultra?2.5*sx:0)+1); }
    sob_n=0; const uint8_t *b=(const uint8_t*)so_scr; for(int i=0;i<240*128;i++) if(b[i]){ pc_sn.foot[t][i]=1; pc_sn.fval[t][i]=b[i]; }
}
static void pc_so_native(volatile unsigned short *pg){
    if(!pc_sn.w || !pc_sn.msk) return; int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0; memcpy(pc_sn.snap2[t],(const void*)pg,240*128); pc_sn.f2[t]=pc_frames();
    for(int i=0;i<20;i++){ s32 *d=&dust[i*3]; double X,Y,X2,Y2; if(so_proj(d[0],d[1],d[2],&X,&Y)){
        if(speed>14 && so_proj(d[0]-((bf[0]*speed*4)>>14),d[1]-((bf[1]*speed*4)>>14),d[2]-((bf[2]*speed*4)>>14),&X2,&Y2)) so_line(X,Y,X2,Y2,56); else so_line(X,Y,X,Y,56); } }
    if(!in_lair){
    { s32 v[12*3]; int a=vbl_count*3; s32 S=420, c_=cosx(a>>2), s_=sinx(a>>2);
      static const s8 cube[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
      for(int i=0;i<8;i++){ s32 x=cube[i][0]*S, z=cube[i][2]*S; v[i*3]=stx_+((x*c_-z*s_)>>14); v[i*3+1]=sty_+cube[i][1]*S; v[i*3+2]=stz_+((x*s_+z*c_)>>14); }
      static const s8 slot[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
      for(int i=0;i<4;i++){ s32 x=slot[i][0]*140, z=-S; v[(8+i)*3]=stx_+((x*c_-z*s_)>>14); v[(8+i)*3+1]=sty_+slot[i][1]*70; v[(8+i)*3+2]=stz_+((x*s_+z*c_)>>14); }
      static const u8 ce[]={0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7}; static const u8 se[]={8,9,9,10,10,11,11,8};
      so_wire(v,12,ce,12,54); so_wire(v,12,se,4,(vbl_count&16)?55:56); }
    for(int i=0;i<nast;i++){ Ast *a=&ast[i]; if(!a->alive) continue;
        static const s8 oc[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        s32 v[6*3]; int ang=(vbl_count*2+a->seed*4)&1023; s32 c=cosx(ang>>2), s_=sinx(ang>>2);
        for(int k=0;k<6;k++){ int rr=a->r*(10+((a->seed>>k)&3)*2)/14; s32 x=oc[k][0]*rr, y=oc[k][1]*rr*3/4, z=oc[k][2]*rr;
            v[k*3]=a->x+((x*c-z*s_)>>14); v[k*3+1]=a->y+y; v[k*3+2]=a->z+((x*s_+z*c)>>14); }
        static const u8 ae[]={0,2,0,3,0,4,0,5,1,2,1,3,1,4,1,5,2,4,2,5,3,4,3,5};
        so_wire(v,6,ae,12,a->flash?49 : sys[cur_sys].kind==6 ? rock_col[rock_type_of(rock_h(i))-1] : (a->seed&1)?56:51); }
    }
    so_items(t);
}

// ---- the cockpit frame (the canopy round the view, the dash with the scanner): called right after the game drew it. The page is copied (pc_sn.snapc) so the compositor can tell what the game drew
// after the frame (text, messages) from the frame itself; cpmode 1 = canopy and dash (cockpit view), 0 = dash only (chase view or filming).
static void pc_so_cockpit(volatile unsigned short *pg){
    if(!pc_sn.w) return; int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0; memcpy(pc_sn.snapc[t],(const void*)pg,240*128); pc_sn.fc[t]=pc_frames(); pc_sn.cpmode[t]=(!chase_cam && !FILM)?1:0; }
