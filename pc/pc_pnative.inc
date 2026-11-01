// pc_pnative.inc: the planet (on foot) at the window's own resolution (NATIVE 3D VIEWS). Included at the end of the game's main.c. After render_planet it makes a
// second picture of the same view: one ray per window column, the game's own height map, colour map, camera, step table, curve and palette, but sampled
// smoothly (bilinear heights and colours, 3 samples per game step, sky as a continuous gradient, stars as soft dots, sun and moons as smooth discs).
// It only READS the game. CLASSIC (or no window size) turns it off. pc_pncomp.inc (in pc_hw.c) shows it where the page still holds what render_planet wrote.
#include <stdlib.h>
typedef struct { int w,h,ya; uint32_t *buf; long f[2]; uint8_t snap[2][240*160]; } PcNat;
extern PcNat pc_pn; extern int pc_nat_w, pc_nat_h; extern int pc_v3d_classic(void); extern long pc_frames(void);
typedef void (*PcParFn2)(void*,int,int); extern void pc_par_chunk(int total, int chunk, PcParFn2 fn, void *ctx);
static int pn_active; static float *pn_top;           // per window column: the first terrain row (sky above it)
static uint32_t pn_tex[256*256], pn_pal[256];
typedef struct { int W,H,ya; double sy; double camx,camy; int ck,csh,fk; s32 csub; uint32_t *buf; } PnCtx;
static inline uint32_t pn_mix(uint32_t a, uint32_t b, int f){ uint32_t rb=((a&0xFF00FF)*(256-f)+(b&0xFF00FF)*f)>>8, g=((a&0xFF00)*(256-f)+(b&0xFF00)*f)>>8; return (rb&0xFF00FF)|(g&0xFF00); }
static inline uint32_t pn_bil(double u, double v){ u-=0.5; v-=0.5; double fu=__builtin_floor(u), fv=__builtin_floor(v); int x0=((int)fu)&255, y0=((int)fv)&255, x1=(x0+1)&255, y1=(y0+1)&255; int wx=(int)((u-fu)*256), wy=(int)((v-fv)*256);
    return pn_mix(pn_mix(pn_tex[y0*256+x0],pn_tex[y0*256+x1],wx), pn_mix(pn_tex[y1*256+x0],pn_tex[y1*256+x1],wx), wy); }
static inline double pn_hgt(double u, double v){ u-=0.5; v-=0.5; double fu=__builtin_floor(u), fv=__builtin_floor(v); int x0=((int)fu)&HMASK, y0=((int)fv)&HMASK, x1=(x0+1)&HMASK, y1=(y0+1)&HMASK; double a=u-fu, b=v-fv;
    return (hfine[y0*HW+x0]*(1-a)+hfine[y0*HW+x1]*a)*(1-b)+(hfine[y1*HW+x0]*(1-a)+hfine[y1*HW+x1]*a)*b; }
static inline uint32_t pn_sky(double yg){ double t=(yg-18)*8.0/(PHZ+30-18)-0.5; if(t<0) t=0; if(t>7) t=7; int i=(int)t; if(i>6) i=6; return pn_mix(pn_pal[SKY_B+i],pn_pal[SKY_B+i+1],(int)((t-i)*256)); }
static void pn_cols(void *vc, int lo, int hi){
    PnCtx *c=(PnCtx*)vc; int W=c->W, H=c->H, ya=c->ya; double sy=c->sy; uint32_t hz=pn_pal[SKY_B+7];
    for(int X=lo;X<hi;X++){
        double gc=(X+0.5)*120.0/W-0.5; double rd=((2*gc-119)*c->fk)/120.0; double vx=(hsn+hcs*rd/256.0)/256.0, vy=(hcs-hsn*rd/256.0)/256.0;
        int rtop=H; double ytopf=160, zl=0; int first=1; double zprev=stp_z[0];
        int total=g_nst*3-2;
        for(int j=0;j<total && rtop>ya;j++){
            int i=j/3, s=j%3; double zf = s? stp_z[i]+(stp_z[i+1]-stp_z[i])*s/3.0 : stp_z[i]; s32 z=(s32)zf; if(z<16) z=16;
            double zc=z/256.0, mx=c->camx+vx*zc, my=c->camy+vy*zc; double h=pn_hgt(mx*2,my*2); if(g_wob){ h+=wob64[(i*3+(int)gc)&63]; if(h<0) h=0; if(h>255) h=255; }
            s32 curve=((((z*z)>>14)*c->ck)>>c->csh)-c->csub; double yi=PHZ+((double)(cam_h+curve)-h*g_hs)*FOCP/z;
            if(yi>=ytopf){ zprev=z; continue; }
            double e=yi*sy; int r0=(int)__builtin_ceil(e-0.5); if(r0<ya) r0=ya; if(r0>=rtop){ ytopf=yi; zprev=z; continue; }
            double zref; if(first){ zref= yi>PHZ+1 ? z*(yi-PHZ)/(160-PHZ) : zprev; if(zref<16) zref=16; ytopf=160; first=0; } else zref=zl;
            int ti=((int)__builtin_floor(my*2)&HMASK)*HW+((int)__builtin_floor(mx*2)&HMASK); int ix=cmap[ti]; int bld=ix>=BLD_B && ix<BLD_B+8; int sp=(int)((ytopf-yi));
            double iz0=1.0/z, iz1=1.0/zref; double span=ytopf-yi; if(span<1e-6) span=1e-6;
            double hazek=(double)j/total; if(hazek>1) hazek=1;
            for(int r=r0;r<rtop;r++){ double yg=(r+0.5)/sy; uint32_t col;
                if(bld){ int rr=(int)((yg-yi)); uint32_t base=pn_pal[ix];
                    if(sp>3){ if(rr==0) col=pn_pal[76+((ix-BLD_B)>>1)]; else if((ix&1) && (rr&3)>=2 && (((int)gc)&3)!=3) col=pn_pal[72+(((rr*3+(int)gc)&7)==0)]; else col=base; } else col=base; }
                else { double t=(yg-yi)/span; if(t>1) t=1; double izz=iz0+(iz1-iz0)*t; double zz=1.0/izz; double zcc=zz/256.0; double px=c->camx+vx*zcc, py=c->camy+vy*zcc;
                    col=pn_bil(px*2,py*2); int ci=cmap[(((int)__builtin_floor(py*2))&HMASK)*HW+(((int)__builtin_floor(px*2))&HMASK)];
                    if(ci<GND_B){ double w=__builtin_sin(px*47+frames*0.06)*__builtin_sin(py*53-frames*0.05); if(w>0.96 && zcc<20) col=pn_mix(col,0xFFFFFF,(int)((w-0.96)*25*128)); }
                    else if(ci>GND_B && ci<GND_B+15 && zcc<14){ uint32_t hh=(uint32_t)((int)__builtin_floor(px*24))*2654435761u ^ (uint32_t)((int)__builtin_floor(py*24))*2246822519u; hh^=hh>>15; hh*=2246822519u; hh^=hh>>13; int d=(int)(hh&15)-8; d=d*(int)(14-zcc)/14; 
                        int rr_=((col>>16)&255)+d, gg_=((col>>8)&255)+d, bb_=(col&255)+d; rr_=rr_<0?0:rr_>255?255:rr_; gg_=gg_<0?0:gg_>255?255:gg_; bb_=bb_<0?0:bb_>255?255:bb_; col=rr_<<16|gg_<<8|bb_; }
                    double hz_=hazek*0.35; if(g_fog){ double fz=(double)(j-(g_nst-6)*3)/18.0; if(fz>0){ hz_+= fz>1?1:fz; if(hz_>1) hz_=1; } }
                    col=pn_mix(col,hz,(int)(hz_*256)); }
                c->buf[(size_t)(r-ya)*W+X]=col; }
            rtop=r0; ytopf=yi; zl=z; zprev=z; }
        for(int r=ya;r<rtop;r++) c->buf[(size_t)(r-ya)*W+X]=pn_sky((r+0.5)/sy);
        pn_top[X]=(float)rtop; }
}
static void pc_pn_native(volatile u16 *pg){
    int W=pc_nat_w, H=pc_nat_h; pn_active=0; pc_pn.w=0; if(W<=240 || H<=160 || pc_v3d_classic()) return;
    static int cap; static unsigned long bcap; if(W>cap){ free(pn_top); pn_top=malloc(sizeof(float)*W); cap=W; }
    int ya=(18*H+159)/160; unsigned long need=(unsigned long)W*(H-ya); if(need>bcap){ free(pc_pn.buf); pc_pn.buf=malloc(need*4); bcap=need; }
    for(int i=0;i<256;i++){ u16 c=BG_PAL[i]; int r=c&31, g=(c>>5)&31, b=(c>>10)&31; pn_pal[i]=(uint32_t)(r<<3|r>>2)<<16|(uint32_t)(g<<3|g>>2)<<8|(uint32_t)(b<<3|b>>2); }
    for(int i=0;i<HW*HW;i++) pn_tex[i]=pn_pal[cmap[i]];
    s32 cb=cam_back(); PnCtx c; c.W=W; c.H=H; c.ya=ya; c.sy=H/160.0; c.camx=(ppx-((hsn*cb)>>8))/65536.0; c.camy=(ppy-((hcs*cb)>>8))/65536.0; c.ck=curv_k(); c.csh=curv_sh(); c.fk=fov_k(); c.csub=tiny_sub(); c.buf=pc_pn.buf;
    pc_par_chunk(W,16,pn_cols,&c);
    // stars: soft dots on the sky rows only
    { double hor=heading*536/1024.0; double sx=W/240.0, sy=H/160.0;
      for(int k=0;k<88;k++){ double x=__builtin_fmod(k*83+17-hor+536*4,536); if(x>=120) continue; int y=19+((k*31+5)%50); double px=(x*2+1)*sx, py=(y+0.5)*sy; double tw=0.7+0.3*__builtin_cos(frames*0.1+k*2.3); uint32_t sc=pn_pal[((frames>>3)+k)&7?4:5]; double rad=0.55*sy+0.5;
          for(int Y=(int)(py-rad-1);Y<=(int)(py+rad+1);Y++){ if(Y<ya||Y>=H) continue; for(int X=(int)(px-rad-1);X<=(int)(px+rad+1);X++){ if(X<0||X>=W||Y>=pn_top[X]-0.5f) continue;
              double d=__builtin_sqrt((X+0.5-px)*(X+0.5-px)+(Y+0.5-py)*(Y+0.5-py)); double a=(1.0-d/(rad+0.5))*tw; if(a<=0) continue; uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X]; *p=pn_mix(*p,sc,(int)(a*255)); } } } }
    int t=((uintptr_t)pg)>=0x0600A000u?1:0; pc_pn.w=W; pc_pn.h=H; pc_pn.ya=ya; memcpy(pc_pn.snap[t],(const void*)pg,240*160); pc_pn.f[t]=pc_frames(); pn_active=1;
}
// the sun and the moons (sky_disc): smooth discs, drawn into the picture only over the sky; the game's own blocky disc is not drawn
static int pc_pn_disc(int cx, int cy, int rc, int dd, int lit, int dark, int sunmode){
    if(!pn_active) return 0; int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double sx=W/240.0, sy=H/160.0; double px=(cx*2+1)*sx, py=(cy+0.5)*sy, R=2.0*rc*sx, Ry=2.0*rc*sy;
    double glow=sunmode?R*2.2:0; double ex=R+glow, ey=Ry+glow*sy/sx; int ad=dd<0?-dd:dd; double f=ad/60.0, psi=3.14159265*(1-f), Lx=(dd>0?-1:1)*__builtin_sin(psi), Lz=__builtin_cos(psi);
    for(int Y=(int)(py-ey);Y<=(int)(py+ey)+1;Y++){ if(Y<ya||Y>=H) continue; for(int X=(int)(px-ex);X<=(int)(px+ex)+1;X++){ if(X<0||X>=W||Y>=pc_pn.h) continue; if(Y>=pn_top[X]-0.5f) continue;
        double nx=(X+0.5-px)/R, ny=(Y+0.5-py)/Ry, q=nx*nx+ny*ny; uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X];
        if(sunmode){ double d=__builtin_sqrt(q); if(d>1){ double g=1.0-(d-1)/(ex/R-1); if(g>0){ g=g*g*0.55; *p=pn_mix(*p,pn_pal[89],(int)(g*255)); } continue; }
            uint32_t col= d<0.55? pn_mix(pn_pal[94],pn_pal[89],(int)(d/0.55*256)) : pn_mix(pn_pal[89],pn_pal[88],(int)((d-0.55)/0.45*256)); double a=(1-d)*R/1.0; if(a>1) a=1; *p=pn_mix(*p,col,(int)(a*255)); }
        else { if(q>=1.0) continue; double nz=__builtin_sqrt(1-q), d=nx*Lx+nz*Lz; double s=d*4+0.5; if(s<0) s=0; if(s>1) s=1; uint32_t col=pn_mix(pn_pal[dark],pn_pal[lit],(int)(s*256)); double a=(1-__builtin_sqrt(q))*R; if(a>1) a=1; *p=pn_mix(*p,col,(int)(a*255)); } } }
    return 1;
}
