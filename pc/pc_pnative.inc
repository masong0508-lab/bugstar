// pc_pnative.inc: the planet (on foot) at the window's own resolution (NATIVE 3D VIEWS). Included at the end of the game's main.c. After render_planet it makes a
// second picture of the same view: one ray per window column, the game's own height map, colour map, camera, step table, curve and palette, but sampled
// smoothly (bilinear heights and colours, 3 samples per game step, sky as a continuous gradient, stars as soft dots, sun and moons as smooth discs).
// It only READS the game. CLASSIC (or no window size) turns it off. pc_pncomp.inc (in pc_hw.c) shows it where the page still holds what render_planet wrote.
#include <stdlib.h>
typedef struct { int w,h,ya; uint32_t *buf; long f[2]; uint8_t snap[2][240*160]; } PcNat;
extern PcNat pc_pn; extern int pc_nat_w, pc_nat_h; extern int pc_v3d_classic(void); extern long pc_frames(void);
typedef void (*PcParFn2)(void*,int,int); extern void pc_par_chunk(int total, int chunk, PcParFn2 fn, void *ctx);
static int pn_active; static float *pn_top, *pn_ca, *pn_cb;           // per window column: the first terrain row (sky above it), and how much terrain covers the row above it (ca) and that row itself (cb)
extern double pc_now_ms(void); extern void pc_vrec(int,double); extern int pc_v3d_ultra(void); static void pc_pn_off(void){ pn_active=0; }
static uint32_t pn_tex[256*256], pn_pal[256];
typedef struct { int W,H,ya; double sy; double camx,camy; int ck,csh,fk; s32 csub; uint32_t *buf; } PnCtx;
static inline uint32_t pn_mix(uint32_t a, uint32_t b, int f){ uint32_t rb=((a&0xFF00FF)*(256-f)+(b&0xFF00FF)*f)>>8, g=((a&0xFF00)*(256-f)+(b&0xFF00)*f)>>8; return (rb&0xFF00FF)|(g&0xFF00); }
static inline double pn_sf(int X,int Y){ int r0=(int)pn_top[X]; if(Y<r0-1) return 1; if(Y==r0-1) return 1-pn_ca[X]; if(Y==r0) return 1-pn_cb[X]; return 0; }       // how much of this pixel is sky (the terrain edge row is shared)
static inline void pn_sky_over(int X,int Y,uint32_t col,double a){ if(X<0||X>=pc_pn.w||Y<pc_pn.ya||Y>=pc_pn.h||a<=0) return; a*=pn_sf(X,Y); if(a<=0) return; uint32_t *p=&pc_pn.buf[(size_t)(Y-pc_pn.ya)*pc_pn.w+X]; *p=pn_mix(*p,col,(int)(a>1?255:a*255)); }
static inline void pn_sky_add(int X,int Y,uint32_t col,double f){ if(X<0||X>=pc_pn.w||Y<pc_pn.ya||Y>=pc_pn.h||f<=0) return; f*=pn_sf(X,Y); if(f<=0) return; int i=(int)(f*256); uint32_t *p=&pc_pn.buf[(size_t)(Y-pc_pn.ya)*pc_pn.w+X], d=*p; int r=(d>>16&255)+((col>>16&255)*i>>8), g=(d>>8&255)+((col>>8&255)*i>>8), b=(d&255)+((col&255)*i>>8); if(r>255) r=255; if(g>255) g=255; if(b>255) b=255; *p=(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; }
static inline uint32_t pn_bil(double u, double v){ u-=0.5; v-=0.5; double fu=__builtin_floor(u), fv=__builtin_floor(v); int x0=((int)fu)&255, y0=((int)fv)&255, x1=(x0+1)&255, y1=(y0+1)&255; int wx=(int)((u-fu)*256), wy=(int)((v-fv)*256);
    return pn_mix(pn_mix(pn_tex[y0*256+x0],pn_tex[y0*256+x1],wx), pn_mix(pn_tex[y1*256+x0],pn_tex[y1*256+x1],wx), wy); }
static inline double pn_hgt(double u, double v){ u-=0.5; v-=0.5; double fu=__builtin_floor(u), fv=__builtin_floor(v); int x0=((int)fu)&HMASK, y0=((int)fv)&HMASK, x1=(x0+1)&HMASK, y1=(y0+1)&HMASK; double a=u-fu, b=v-fv;
    return (hfine[y0*HW+x0]*(1-a)+hfine[y0*HW+x1]*a)*(1-b)+(hfine[y1*HW+x0]*(1-a)+hfine[y1*HW+x1]*a)*b; }
// heights near a building are point-sampled, not blended: the blend turned every wall into a ramp one texel wide (a roof with sloping sides, like a hill). The classic
// view takes the nearest texel, so a wall is vertical and a roof flat; here the same rule applies wherever any of the four blend texels is a building.
static inline int pn_isb(int c){ return c>=BLD_B && c<BLD_B+8; }
static inline double pn_hgtb(double u, double v){
    double uu=u-0.5, vv=v-0.5, fu=__builtin_floor(uu), fv=__builtin_floor(vv); int x0=((int)fu)&HMASK, y0=((int)fv)&HMASK, x1=(x0+1)&HMASK, y1=(y0+1)&HMASK;
    if(pn_isb(cmap[y0*HW+x0])||pn_isb(cmap[y0*HW+x1])||pn_isb(cmap[y1*HW+x0])||pn_isb(cmap[y1*HW+x1]))
        return hfine[(((int)__builtin_floor(v))&HMASK)*HW+(((int)__builtin_floor(u))&HMASK)];
    return pn_hgt(u,v); }
static inline uint32_t pn_sky(double yg){ double t=(yg-18)*8.0/(PHZ+30-18)-0.5; if(t<0) t=0; if(t>7) t=7; int i=(int)t; if(i>6) i=6; return pn_mix(pn_pal[SKY_B+i],pn_pal[SKY_B+i+1],(int)((t-i)*256)); }
// a building's wall: floors every 32 units of real height (so they shrink with distance like real floors), a roof ledge, slab lines, punched windows or glass ribbons
// (by tint), some windows lit. hd = height below the roof edge; u = distance along the wall (mx+my: one of them is constant on any face).
static inline uint32_t pn_dim(uint32_t c, int f){ return pn_mix(c,0x000000,f); }
static inline uint32_t pn_lite(uint32_t c, int f){ return pn_mix(c,0xFFFFFF,f); }
static inline uint32_t pn_bld(int ix, double dy, int z, double u, double gc, int face, double hag, double sy){
    int t=(ix-BLD_B)>>1; uint32_t base=pn_pal[BLD_B+t*2], shd=pn_pal[BLD_B+t*2+1], roof=pn_pal[76+t];
    double hd=dy*z/(double)FOCP, pxf=32.0*FOCP/z*sy;                          // pxf = screen pixels per floor
    // each face has its own light: face 0 (x) is the lit side, face 1 (y) is in shade; walls get lighter toward the roof and darker at the street (contact shadow)
    int fdim = face ? 30 : 0; int ao = hag<40 ? (int)((40-hag)*0.9) : 0; if(ao>38) ao=38; int sky = hag>60 ? (int)((hag-60)*0.12) : 0; if(sky>22) sky=22;
    if(hd<5){ return face ? pn_dim(roof,12) : pn_lite(roof,16); }                // roof: a bright lip
    if(hd<5.8) return pn_dim(base,110);                                          // thin shadow line under the lip
    uint32_t wall=pn_dim(pn_lite(base,sky),fdim);
    int fl=(int)(hd/32.0); double fr=hd-fl*32.0; double uf=u-__builtin_floor(u); int uc=(int)__builtin_floor(u);
    uint32_t avg=pn_mix(wall,pn_dim(pn_pal[73],fdim),60);
    double kq=(pxf-2.5)/9.0; if(kq<0) kq=0; if(kq>1) kq=1; int kf=(int)(kq*256);   // detail fades out with distance instead of aliasing into stripes
    uint32_t col;
    if(hag<19){                                                                  // ground floor: a dark shopfront with glass between pillars, an awning ledge above it
        if(hag>=17.5) col=pn_dim(base,90);
        else { int pil = (uf<0.10 || uf>0.90 || (uf>0.47 && uf<0.53)); uint32_t hh0=(uint32_t)(uc*2246822519u+(uint32_t)t*977u); hh0^=hh0>>13; hh0*=2654435761u; hh0^=hh0>>15;
               if(pil) col=pn_dim(wall,40); else { int lit=(hh0&3)!=0; col= lit ? pn_mix(pn_pal[72],wall,90) : pn_dim(pn_pal[73],40); if(hag<3) col=pn_dim(col,50); } }
        return pn_dim(pn_mix(avg,col,kf),ao/2); }
    if(fr<2) col=pn_dim(shd,fdim);                                               // slab edge
    else {
        uint32_t hh=(uint32_t)(fl*2654435761u)^(uint32_t)((uc+t*7)*2246822519u); hh^=hh>>13; hh*=2246822519u; hh^=hh>>15;
        int glass=(t&1); int inwin = glass ? (fr>=7 && fr<24 && uf>0.06) : (fr>=9 && fr<24 && uf>=0.22 && uf<0.78);
        if(!inwin){ col = (!glass && fr>=24 && fr<27 && uf>=0.18 && uf<0.82) ? pn_lite(wall,14) : wall; }   // sill under a punched window
        else {
            int lit=(hh&3)==0; uint32_t g=lit?pn_pal[72]:pn_mix(pn_pal[73],pn_pal[72],(int)((hh>>8)&63));
            double wf=(fr-9)/15.0; if(wf<0) wf=0; if(wf>1) wf=1;
            if(!lit) g=pn_mix(g,pn_pal[72],(int)(wf*wf*70));                     // the pane reflects the sky toward its bottom edge
            if(glass){ if(uf<0.5) g=pn_mix(g,base,50); if(((int)(uf*8))==3) g=pn_dim(g,50); }     // ribbon glass: sheen and a mullion every eighth of a floor
            else if(uf>0.48 && uf<0.52) g=pn_dim(g,45);                         // a mullion down the middle of each punched window
            if(fr<10.5 && !lit) g=pn_dim(g,60);                                  // reveal shadow at the top of the opening
            if(face) g=pn_dim(g,22);
            col=g; }
    }
    col = pn_mix(avg,col,kf);
    return ao ? pn_dim(col,ao) : col; }
// the town's streets (palette 74 asphalt, 75 lane dash, 80/81 plaza and yards). Smooth colour sampling smeared them into the lots beside them and washed the lane dashes out, so
// near a street the colour map is sampled "sharp bilinear": the blend between two texels is squeezed into about one screen pixel, so edges stay crisp but never staircase. The
// asphalt gets a fine grain, and the paving beside a street gets a pale kerb line, so a street reads as a street and not as paint.
static int pn_roadT; static double pn_L;                                         // the city's tech level (0 huts .. 5 megacity), set once a frame
static inline int pn_stI(int ix){ return ix==74 || ix==75 || ix==80 || ix==81; }
static uint8_t pn_nst[HW*HW];                                                         // 1 where the texel or one of its four neighbours is street (made once a frame): the others skip the street tests
static uint32_t pn_gnd(double u, double v, double zcc){
    int tx=(int)__builtin_floor(u)&HMASK, ty=(int)__builtin_floor(v)&HMASK; if(!pn_nst[ty*HW+tx]) return pn_bil(u,v); int xr=(tx+1)&HMASK, xl=(tx-1)&HMASK, yd=(ty+1)&HMASK, yu=(ty-1)&HMASK;
    int c0=cmap[ty*HW+tx], cr=cmap[ty*HW+xr], cl=cmap[ty*HW+xl], cd=cmap[yd*HW+tx], cu=cmap[yu*HW+tx];
    double k=0.5*FOCP/zcc*(pc_pn.w/240.0); if(k<1.5) k=1.5; if(k>16) k=16;                              // screen pixels per texel here: the nearer, the sharper
    double uu=u-0.5, vv=v-0.5, fu=__builtin_floor(uu), fv=__builtin_floor(vv), a=uu-fu, b=vv-fv;
    a=0.5+(a-0.5)*k; if(a<0) a=0; if(a>1) a=1; b=0.5+(b-0.5)*k; if(b<0) b=0; if(b>1) b=1;
    int x0=((int)fu)&HMASK, y0=((int)fv)&HMASK, x1=(x0+1)&HMASK, y1=(y0+1)&HMASK, wx=(int)(a*256), wy=(int)(b*256);
    uint32_t col=pn_mix(pn_mix(pn_tex[y0*256+x0],pn_tex[y0*256+x1],wx), pn_mix(pn_tex[y1*256+x0],pn_tex[y1*256+x1],wx), wy);
    if(zcc<18){
        double fx=u-__builtin_floor(u), fy=v-__builtin_floor(v);
        if(c0==74 || c0==75){ uint32_t hh=(uint32_t)((int)__builtin_floor(u*6))*2654435761u ^ (uint32_t)((int)__builtin_floor(v*6))*2246822519u; hh^=hh>>15; hh*=2246822519u; hh^=hh>>13;
            static const u8 amp[6]={5,4,3,2,1,0}; int d=((int)(hh&7)-3)*amp[pn_roadT]*(int)(18-zcc)/36;  /* rough earth, smooth megacity */ int r=(int)(col>>16&255)+d, g=(int)(col>>8&255)+d, bl=(int)(col&255)+d; r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; bl=bl<0?0:bl>255?255:bl; col=(uint32_t)(r<<16|g<<8|bl); }
        else if((c0==80 || c0==81) && pn_roadT>=1){ int e = (cr==74&&1-fx<0.16) || (cl==74&&fx<0.16) || (cd==74&&1-fy<0.16) || (cu==74&&fy<0.16); if(e) col=pn_mix(col,0xFFFFFF,pn_roadT>=2?56:30); }   // kerb
    }
    if(zcc<40){                                                                           // paint and shadows, at any distance a street is visible
        double fx=u-__builtin_floor(u), fy=v-__builtin_floor(v);
        if(c0==75 && pn_roadT>=2) col=pn_mix(col,pn_roadT>=5?0x80F0FF:0xFFF4C8, pn_roadT>=5?110:150);   // lane paint
        if(c0==74 || c0==75){ int ke=(pn_isb(cr)&&1-fx<0.22) || (pn_isb(cl)&&fx<0.22) || (pn_isb(cd)&&1-fy<0.22) || (pn_isb(cu)&&fy<0.22);
            int yk=((cr==80||cr==81)&&1-fx<0.10) || ((cl==80||cl==81)&&fx<0.10) || ((cd==80||cd==81)&&1-fy<0.10) || ((cu==80||cu==81)&&fy<0.10);
            if(ke) col=pn_mix(col,0x000000,70); else if(yk) col=pn_mix(col,0x000000,36); }    // the street is shadowed under the walls and darker along the kerb
        else if(c0==80 || c0==81){ int ke=(pn_isb(cr)&&1-fx<0.30) || (pn_isb(cl)&&fx<0.30) || (pn_isb(cd)&&1-fy<0.30) || (pn_isb(cu)&&fy<0.30); if(ke) col=pn_mix(col,0x000000,64); }   // contact shadow at a building's foot
    }
    return col;
}
static void pn_cols(void *vc, int lo, int hi){
    PnCtx *c=(PnCtx*)vc; int W=c->W, H=c->H, ya=c->ya; double sy=c->sy; uint32_t hz=pn_pal[SKY_B+7];
    for(int X=lo;X<hi;X++){
        double gc=(X+0.5)*120.0/W-0.5; double rd=((2*gc-119)*c->fk)/120.0; double vx=(hsn+hcs*rd/256.0)/256.0, vy=(hcs-hsn*rd/256.0)/256.0;
        int rtop=H; double ytopf=160, zl=0; int first=1; double zprev=stp_z[0]; int ne=0; int er[256]; float eca[256], ecb[256];                // (the silhouette edges of this column, to anti-alias once the column is done)
        int total=g_nst*3-2;
        for(int j=0;j<total && rtop>ya;j++){
            int i=j/3, s=j%3; double zf = s? stp_z[i]+(stp_z[i+1]-stp_z[i])*s/3.0 : stp_z[i]; s32 z=(s32)zf; if(z<16) z=16;
            double zc=z/256.0, mx=c->camx+vx*zc, my=c->camy+vy*zc; double h=pn_hgtb(mx*2,my*2); if(g_wob){ h+=wob64[(i*3+(int)gc)&63]; if(h<0) h=0; if(h>255) h=255; }
            s32 curve=((((z*z)>>14)*c->ck)>>c->csh)-c->csub; double yi=PHZ+((double)(cam_h+curve)-h*g_hs)*FOCP/z;
            if(yi>=ytopf){ zprev=z; continue; }
            double e=yi*sy; int r0=(int)__builtin_ceil(e-0.5); int clamped=r0<ya; if(r0<ya) r0=ya; if(r0>=rtop){ ytopf=yi; zprev=z; continue; }
            double zref; if(first){ zref= yi>PHZ+1 ? z*(yi-PHZ)/(160-PHZ) : zprev; if(zref<16) zref=16; ytopf=160; first=0; } else zref=zl;
            int ti=((int)__builtin_floor(my*2)&HMASK)*HW+((int)__builtin_floor(mx*2)&HMASK); int ix=cmap[ti]; int bld=ix>=BLD_B && ix<BLD_B+8; int sp=(int)((ytopf-yi));
            double iz0=1.0/z, iz1=1.0/zref; double span=ytopf-yi; if(span<1e-6) span=1e-6; double ispan=1.0/span;
            int face=0; double ygr=0;
            if(bld){ int tx=(int)__builtin_floor(mx*2), ty=(int)__builtin_floor(my*2); int sxs=vx>0?-1:1, sys=vy>0?-1:1;
                int bx=pn_isb(cmap[(ty&HMASK)*HW+((tx+sxs)&HMASK)]), by=pn_isb(cmap[((ty+sys)&HMASK)*HW+(tx&HMASK)]);
                if(bx!=by) face = bx ? 1 : 0;                                  // the open side is the face we see (0: an x-facing wall, lit; 1: a y-facing wall, shaded)
                else { double fx=mx*2-tx, fy=my*2-ty, ex= vx>0?fx:1-fx, ey= vy>0?fy:1-fy; face = ex<ey ? 0 : 1; }
                ygr=PHZ+((double)(cam_h+curve)-pn_L*g_hs)*FOCP/z; }
            double hazek=(double)j/total; if(hazek>1) hazek=1;
            for(int r=r0;r<rtop;r++){ double yg=(r+0.5)/sy; uint32_t col;
                if(bld) { double hag=(ygr-yg)*z/(double)FOCP; col=pn_bld(ix,yg-yi,z,mx+my,gc,face,hag,sy); }
                else { double t=(yg-yi)*ispan; if(t>1) t=1; double izz=iz0+(iz1-iz0)*t; double zz=1.0/izz; double zcc=zz/256.0; double px=c->camx+vx*zcc, py=c->camy+vy*zcc;
                    col=pn_gnd(px*2,py*2,zcc); int ci=cmap[(((int)__builtin_floor(py*2))&HMASK)*HW+(((int)__builtin_floor(px*2))&HMASK)];
                    if(ci<GND_B && zcc<20){ double w=__builtin_sin(px*47+frames*0.06)*__builtin_sin(py*53-frames*0.05); if(w>0.96 && zcc<20) col=pn_mix(col,0xFFFFFF,(int)((w-0.96)*25*128)); }
                    else if(ci>GND_B && ci<GND_B+15 && zcc<14){ uint32_t hh=(uint32_t)((int)__builtin_floor(px*24))*2654435761u ^ (uint32_t)((int)__builtin_floor(py*24))*2246822519u; hh^=hh>>15; hh*=2246822519u; hh^=hh>>13; int d=(int)(hh&15)-8; d=d*(int)(14-zcc)/14; 
                        int rr_=((col>>16)&255)+d, gg_=((col>>8)&255)+d, bb_=(col&255)+d; rr_=rr_<0?0:rr_>255?255:rr_; gg_=gg_<0?0:gg_>255?255:gg_; bb_=bb_<0?0:bb_>255?255:bb_; col=rr_<<16|gg_<<8|bb_; }
                    double hz_=hazek*0.35; if(g_fog){ double fz=(double)(j-(g_nst-6)*3)/18.0; if(fz>0){ hz_+= fz>1?1:fz; if(hz_>1) hz_=1; } }
                    col=pn_mix(col,hz,(int)(hz_*256)); }
                c->buf[(size_t)(r-ya)*W+X]=col; }
            if(!clamped && r0>ya && ne<256){ double ca=r0-e, cb=r0+1-e; er[ne]=r0; eca[ne]=(float)(ca>0?ca:0); ecb[ne]=(float)(cb<1?cb:1); ne++; }
            rtop=r0; ytopf=yi; zl=z; zprev=z; }
        for(int r=ya;r<rtop;r++) c->buf[(size_t)(r-ya)*W+X]=pn_sky((r+0.5)/sy);
        for(int q=ne-1;q>=0;q--){ int r0=er[q]; uint32_t *p0=&c->buf[(size_t)(r0-ya)*W+X], *pa=&c->buf[(size_t)(r0-1-ya)*W+X]; uint32_t above=*pa, seg=*p0;     // farthest edge first: the row above the edge takes a share of the nearer surface, the edge row keeps the rest of it
            if(ecb[q]<1.0f) *p0=pn_mix(above,seg,(int)(ecb[q]*256)); if(eca[q]>0.0f) *pa=pn_mix(above,seg,(int)(eca[q]*256)); }
        pn_top[X]=(float)rtop; if(ne>0 && er[ne-1]==rtop){ pn_ca[X]=eca[ne-1]; pn_cb[X]=ecb[ne-1]; } else { pn_ca[X]=0; pn_cb[X]=1; } }
}
static void pc_pn_native(volatile u16 *pg){
    pn_roadT = city_r ? city_tech() : 0; pn_L = city_r ? hmap[city_cy*TW+city_cx] : 0;
    int W=pc_nat_w, H=pc_nat_h; pn_active=0; pc_pn.w=0; if(W<=240 || H<=160 || pc_v3d_classic()) return; double pn_t0=pc_now_ms();
    static int cap; static unsigned long bcap; if(W>cap){ free(pn_top); free(pn_ca); free(pn_cb); pn_top=malloc(sizeof(float)*W); pn_ca=malloc(sizeof(float)*W); pn_cb=malloc(sizeof(float)*W); cap=W; }
    int ya=(18*H+159)/160; unsigned long need=(unsigned long)W*(H-ya); if(need>bcap){ free(pc_pn.buf); pc_pn.buf=malloc(need*4); bcap=need; }
    for(int i=0;i<256;i++){ u16 c=BG_PAL[i]; int r=c&31, g=(c>>5)&31, b=(c>>10)&31; pn_pal[i]=(uint32_t)(r<<3|r>>2)<<16|(uint32_t)(g<<3|g>>2)<<8|(uint32_t)(b<<3|b>>2); }
    for(int i=0;i<HW*HW;i++) pn_tex[i]=pn_pal[cmap[i]];
    for(int y=0;y<HW;y++) for(int x=0;x<HW;x++){ int i=y*HW+x; pn_nst[i]=pn_stI(cmap[i])||pn_stI(cmap[y*HW+((x+1)&HMASK)])||pn_stI(cmap[y*HW+((x-1)&HMASK)])||pn_stI(cmap[((y+1)&HMASK)*HW+x])||pn_stI(cmap[((y-1)&HMASK)*HW+x]); }
    s32 cb=cam_back(); PnCtx c; c.W=W; c.H=H; c.ya=ya; c.sy=H/160.0; c.camx=(ppx-((hsn*cb)>>8))/65536.0; c.camy=(ppy-((hcs*cb)>>8))/65536.0; c.ck=curv_k(); c.csh=curv_sh(); c.fk=fov_k(); c.csub=tiny_sub(); c.buf=pc_pn.buf;
    pc_par_chunk(W,16,pn_cols,&c);
    // stars: soft dots on the sky rows only
    { double hor=heading*536/1024.0; double sx=W/240.0, sy=H/160.0;
      for(int k=0;k<88;k++){ double x=__builtin_fmod(k*83+17-hor+536*4,536); if(x>=120) continue; int y=19+((k*31+5)%50); double px=(x*2+1)*sx, py=(y+0.5)*sy; double tw=0.7+0.3*__builtin_cos(frames*0.1+k*2.3); uint32_t sc=pn_pal[((frames>>3)+k)&7?4:5]; double rad=0.55*sy+0.5;
          for(int Y=(int)(py-rad-1);Y<=(int)(py+rad+1);Y++){ if(Y<ya||Y>=H) continue; for(int X=(int)(px-rad-1);X<=(int)(px+rad+1);X++){ if(X<0||X>=W||Y>=pn_top[X]-0.5f) continue;
              double d=__builtin_sqrt((X+0.5-px)*(X+0.5-px)+(Y+0.5-py)*(Y+0.5-py)); double a=(1.0-d/(rad+0.5))*tw; if(a<=0) continue; uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X]; *p=pn_mix(*p,sc,(int)(a*255)); } } } }
    int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0; pc_pn.w=W; pc_pn.h=H; pc_pn.ya=ya; memcpy(pc_pn.snap[t],(const void*)pg,240*160); pc_pn.f[t]=pc_frames(); pn_active=1; pc_vrec(1,pc_now_ms()-pn_t0);
}
// the parked ship's shadow (ship_shadow in planet.inc works out where it lies: a dark patch under the hull and a paler, softer far end up or down the slope).
// Here it is drawn into the picture as a smooth, soft-edged, slightly cool shadow over the ground only; the game's own blocky copy is skipped. 1 = drawn here.
static int pc_pn_shadow(volatile unsigned short *pg, int cx, int cy, int rx0, int ry0, int tx, int ty, int rx1, int ry1, int hour){
    if(!pn_active) return 0; int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double sx=W/240.0, sy=H/160.0;
    double amax = hour==2 ? 0.34 : hour==1 ? 0.64 : 0.74;                                  // by moonlight it is faint
    double ax=cx+0.5, ay=cy+0.5, vx=tx-cx, vy=ty-cy, l2=vx*vx+vy*vy;
    int gx0=(int)((cx<tx?cx:tx)-(rx0>rx1?rx0:rx1)-4), gx1=(int)((cx>tx?cx:tx)+(rx0>rx1?rx0:rx1)+5), gy0=(int)((cy<ty?cy:ty)-(ry0>ry1?ry0:ry1)-4), gy1=(int)((cy>ty?cy:ty)+(ry0>ry1?ry0:ry1)+5);
    int X0=(int)(gx0*sx), X1=(int)(gx1*sx)+1, Y0=(int)(gy0*sy), Y1=(int)(gy1*sy)+1; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
    for(int Y=Y0;Y<Y1;Y++){ double gy=(Y+0.5)/sy; int gyi=(int)gy; if(gyi<18||gyi>159) continue;
        for(int X=X0;X<X1;X++){ if(Y<pn_top[X]-0.5f) continue; double gx=(X+0.5)/sx; int gxi=(int)gx; if(gxi<0||gxi>239) continue;
            double px=gx-ax, py=gy-ay, t = l2>0.01 ? (px*vx+py*vy)/l2 : 0; if(t<0) t=0; if(t>1) t=1;
            double ex=px-vx*t, ey=py-vy*t, rx=rx0+(rx1-rx0)*t, ry=ry0+(ry1-ry0)*t, q=__builtin_sqrt((ex*ex)/(rx*rx)+(ey*ey)/(ry*ry));
            double soft=0.30+0.40*t, f=(1.0+soft*0.5-q)/soft; if(f<=0) continue; if(f>1) f=1; f=f*f*(3-2*f);                    // a wider penumbra the farther from the hull
            double a=amax*(1.0-0.55*t)*f; if(t<0.2 && q<0.6) a+=0.18*(1.0-t*5);                                                   // darkest right under the hull
            if(sh_occ && sh_occ[(gxi>>1)<<3]<=gyi) continue;                                                                      // a nearer hill hides it
            int ix=pg[(gyi*240+gxi)>>1]; ix = (gxi&1) ? ix>>8 : ix&255; if(ix<GND_B || ix>=GND_B+16) continue;                    // only on the ground
            uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X]; uint32_t c=*p; int r=(int)(c>>16&255), g=(int)(c>>8&255), b=(int)(c&255);
            r=(int)(r*(1.0-0.72*a)); g=(int)(g*(1.0-0.68*a)); b=(int)(b*(1.0-0.52*a)); *p=(uint32_t)(r<<16|g<<8|b); } }
    return 1;
}
// the sun and the moons (sky_disc): smooth discs, drawn into the picture only over the sky; the game's own blocky disc is not drawn
static int pc_pn_disc(int cx, int cy, int rc, int dd, int lit, int dark, int sunmode){
    if(!pn_active) return 0; int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double sx=W/240.0, sy=H/160.0; double px=(cx*2+1)*sx, py=(cy+0.5)*sy, R=2.0*rc*sx, Ry=2.0*rc*sy;
    double glow=sunmode?R*2.2:0; double ex=R+glow, ey=Ry+glow*sy/sx; int ad=dd<0?-dd:dd; double f=ad/60.0, psi=3.14159265*(1-f), Lx=(dd>0?-1:1)*__builtin_sin(psi), Lz=__builtin_cos(psi);
    for(int Y=(int)(py-ey);Y<=(int)(py+ey)+1;Y++){ if(Y<ya||Y>=H) continue; for(int X=(int)(px-ex);X<=(int)(px+ex)+1;X++){ if(X<0||X>=W||Y>=pc_pn.h) continue; if(Y>(int)pn_top[X]) continue; double sfk=pn_sf(X,Y); if(sfk<=0) continue;
        double nx=(X+0.5-px)/R, ny=(Y+0.5-py)/Ry, q=nx*nx+ny*ny; uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X];
        if(sunmode){ double d=__builtin_sqrt(q); if(d>1){ double g=1.0-(d-1)/(ex/R-1); if(g>0){ g=g*g*0.55*sfk; *p=pn_mix(*p,pn_pal[89],(int)(g*255)); } continue; }
            uint32_t col= d<0.55? pn_mix(pn_pal[94],pn_pal[89],(int)(d/0.55*256)) : pn_mix(pn_pal[89],pn_pal[88],(int)((d-0.55)/0.45*256)); double a=(1-d)*R/1.0; if(a>1) a=1; a*=sfk; *p=pn_mix(*p,col,(int)(a*255)); }
        else { if(q>=1.0) continue; double nz=__builtin_sqrt(1-q), d=nx*Lx+nz*Lz; double s=d*4+0.5; if(s<0) s=0; if(s>1) s=1; uint32_t col=pn_mix(pn_pal[dark],pn_pal[lit],(int)(s*256)); double a=(1-__builtin_sqrt(q))*R; if(a>1) a=1; a*=sfk; *p=pn_mix(*p,col,(int)(a*255)); } } }
    return 1;
}
// ---- weather (rain, snow, dust, embers, ash): the game puts a handful of 1 or 2 pixel marks a frame on the page; here the same drops (same places, same fall) are made as
// streaks and soft flakes at the window's resolution. The nearer a drop (the faster it falls), the bigger and brighter it is. 1 = done here, the game's own marks are skipped.
static int pc_pn_weather(void){
    if(!pn_active || !wd.weather) return 0; int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double sx=W/240.0, sy=H/160.0, su=(sx+sy)*0.5; int ultra=pc_v3d_ultra();
    u32 s=wd.tseed+(frames>>1)*7; int n = wd.weather==3 ? 24 : 40;
    for(int k=0;k<n;k++){ s=s*1664525u+1013904223u; int x=(s>>8)%120, y=20+((s>>20)%138 + (wd.weather==2 ? (frames>>1) : frames*3)*(k+1))%138;
        int ix = wd.weather==1 ? 13 : wd.weather==2 ? C_WHITE : wd.weather==4 ? ((k&1)?C_GOLD:C_RED) : wd.weather==5 ? ((k&1)?C_GREY:C_DGREY) : C_GOLD;
        uint32_t col=pn_pal[ix]; double near=0.55+0.45*(k+1)/(double)n, cx=(x*2+1.0)*sx, cy=(y+0.5)*sy;
        int X0=(int)(cx-6*su), X1=(int)(cx+6*su)+1, Y0=(int)(cy-6*su), Y1=(int)(cy+6*su)+1; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
        if(wd.weather==1){ double len=(2.2+2.6*near)*su, lx=-0.35*len, ly=len, hw=(0.13+0.1*near)*su, x0=cx, y0=cy, x1=cx+lx, y1=cy-ly;                  // rain: a thin slanted streak, brighter at the leading end
            double dx=x1-x0, dy=y1-y0, l2=dx*dx+dy*dy;
            for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double px=X+0.5-x0, py=Y+0.5-y0, t=(px*dx+py*dy)/l2; if(t<0) t=0; if(t>1) t=1; double ex=px-t*dx, ey=py-t*dy, d=__builtin_sqrt(ex*ex+ey*ey), c=hw-d+0.5; if(c<=0) continue; if(c>1) c=1;
                uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X]; *p=pn_mix(*p,col,(int)(c*(0.30+0.40*t)*near*255)); } }
        else { double r=(wd.weather==2 ? 0.40+0.35*near : wd.weather==3 ? 0.30+0.25*near : 0.34+0.30*near)*su, a0=wd.weather==3 ? 0.75 : 0.95;                  // snow, dust, embers, ash: soft round flakes
            for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double dx=X+0.5-cx, dy=Y+0.5-cy, d=__builtin_sqrt(dx*dx+dy*dy), c=r-d+0.5; uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X];
                if(c>0){ if(c>1) c=1; *p=pn_mix(*p,col,(int)(c*a0*255)); }
                if((wd.weather==4 || ultra) && d<r*3.2){ double g=1.0-d/(r*3.2); g=g*g*(wd.weather==4?0.55:0.25); uint32_t d0=*p; int i=(int)(g*256); int rr=(d0>>16&255)+((col>>16&255)*i>>8), gg=(d0>>8&255)+((col>>8&255)*i>>8), bb=(d0&255)+((col&255)*i>>8); if(rr>255) rr=255; if(gg>255) gg=255; if(bb>255) bb=255; *p=(uint32_t)rr<<16|(uint32_t)gg<<8|(uint32_t)bb; } } } }
    return 1;
}
// ---- the sky of a weird world (SUNS, MOONS, RINGS, METEORS) and the great sun or skull-moon of a hell world: smooth discs with a soft edge and glow, a ring made as a curve with
// anti-aliased bands, meteors with a fading tail. All over the sky only (never over the ground). Same places as the game's blocky versions, which are skipped.
static const uint8_t pn_hellr[6]={14,9,12,13,15,8};
static void pn_sdisc(double cx,double cy,double R,uint32_t c0,uint32_t c1,uint32_t c2,double glow,uint32_t gcol){
    int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double ex=R+glow; int X0=(int)(cx-ex)-1, X1=(int)(cx+ex)+2, Y0=(int)(cy-ex)-1, Y1=(int)(cy+ex)+2; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
    for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double dx=X+0.5-cx, dy=Y+0.5-cy, d=__builtin_sqrt(dx*dx+dy*dy);
        if(d>R+0.5){ if(glow>0 && d<R+glow){ double g=1.0-(d-R)/glow; pn_sky_add(X,Y,gcol,g*g*0.42); } continue; }
        double t=d/R; uint32_t col = t<0.5 ? c0 : t<0.65 ? pn_mix(c0,c1,(int)((t-0.5)/0.15*256)) : t<0.78 ? c1 : t<0.9 ? pn_mix(c1,c2,(int)((t-0.78)/0.12*256)) : c2;
        double a=R-d+0.5; if(a>1) a=1; pn_sky_over(X,Y,col,a); if(glow>0 && t>0.8) pn_sky_add(X,Y,gcol,(t-0.8)*0.6); }
}
static int pc_pn_decor(volatile u16 *pg){
    (void)pg; if(!pn_active) return 0; int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double sx=W/240.0, sy=H/160.0, su=(sx+sy)*0.5; int ultra=pc_v3d_ultra();
    if(WL(MW_SUNS)){ int ns=2+WL(MW_SUNS)/7, r=3+WL(MW_SUNS)/4;
        for(int k=0;k<ns;k++){ int cx=sky_col(20+k*29), cy=30+k*9; if(cx<-40) continue; pn_sdisc((cx*2+1.0)*sx,(cy+0.5)*sy,r*su,pn_pal[94],pn_pal[89],pn_pal[88],r*su*(ultra?2.4:1.6),pn_pal[89]); } }
    if(WL(MW_MOONS)){ int nm=1+WL(MW_MOONS)/5;
        for(int k=0;k<nm;k++){ int cx=sky_col(k*23+60), cy=24+(k*13)%34, r=2+(k%3); if(cx<-40) continue; double mx=(cx*2+1.0)*sx, my=(cy+0.5)*sy, R=r*su;                                  // a moon: lit on the left, the terminator soft, a pale limb
            int X0=(int)(mx-R)-1, X1=(int)(mx+R)+2, Y0=(int)(my-R)-1, Y1=(int)(my+R)+2; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
            for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double dx=(X+0.5-mx)/R, dy=(Y+0.5-my)/R, q=dx*dx+dy*dy; if(q>=1.15) continue; double d=__builtin_sqrt(q), a=(1-d)*R+0.5; if(a<=0) continue; if(a>1) a=1;
                double lt=0.5-dx*0.9; if(lt<0) lt=0; if(lt>1) lt=1; uint32_t col=pn_mix(pn_pal[92],pn_pal[91],(int)(lt*256)); if(d>0.85) col=pn_mix(col,pn_pal[91],(int)((d-0.85)/0.15*70)); pn_sky_over(X,Y,col,a); } } }
    if(WL(MW_RINGS)){ int t=WL(MW_RINGS); double hor=((heading*536)>>10)/1.0;                                                                                                  // a band across the sky, bending with the horizon
        for(int X=0;X<W;X++){ double c=(X+0.5)*120.0/W-0.5; double dc=__builtin_fmod(c+hor+536-60+268,536.0)-268; double y=26+t/2+dc*dc*(4+t/4)/900.0; if(y>150) continue;
            for(int b=0;b<3;b++){ if(b==2 && t<=12) break; double yb0= b==0 ? y : b==1 ? y+1 : y+3; uint32_t col= b==1 ? pn_pal[93] : pn_pal[90]; double a0=yb0*sy, a1=(yb0+1)*sy;
                for(int Y=(int)a0;Y<=(int)a1;Y++){ if(Y<ya||Y>=H) continue; double lo=a0>Y?a0:Y, hi=a1<Y+1?a1:Y+1, cv=hi-lo; if(cv<=0) continue; pn_sky_over(X,Y,col,cv*0.92); } } } }
    if(WL(MW_METEOR)){ int tt=(frames*3)%200; int x0=(frames/67*37)%120; int cs=(x0+tt/2)%120; if(cs+7<120){
        double hx=((cs+7)*2+1.0)*sx, hy=(20+tt/3+7+0.5)*sy, tx=(cs*2+1.0)*sx, ty=(20+tt/3+0.5)*sy; double dx=hx-tx, dy=hy-ty, l2=dx*dx+dy*dy; int X0=(int)(tx-4*su), X1=(int)(hx+4*su)+1, Y0=(int)(ty-4*su), Y1=(int)(hy+4*su)+1; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
        for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double px=X+0.5-tx, py=Y+0.5-ty, t=(px*dx+py*dy)/l2; if(t<0) t=0; if(t>1) t=1; double ex=px-t*dx, ey=py-t*dy, d=__builtin_sqrt(ex*ex+ey*ey), w=(0.25+0.45*t)*sx, c=w-d+0.5; if(c>0){ if(c>1) c=1; pn_sky_over(X,Y,pn_pal[94],c*t*t); }
            if(ultra && d<w*5){ double g=1-d/(w*5); pn_sky_add(X,Y,pn_pal[94],g*g*0.35*t); } } } }
    return 1;
}
static int pc_pn_hellsky(volatile u16 *pg){
    (void)pg; if(!pn_active) return 0; double sx=pc_pn.w/240.0, sy=pc_pn.h/160.0, su=(sx+sy)*0.5; int ultra=pc_v3d_ultra();
    int k=wd.cls-PC_INFERNO, r=pn_hellr[k], cx=sky_col(30+k*17), cy=38+(k&1)*6; if(cx<-40) return 1; double X=(cx*2+1.0)*sx, Y=(cy+0.5)*sy;
    pn_sdisc(X,Y,r*su,pn_pal[94],pn_pal[89],pn_pal[88],r*su*(ultra?1.8:1.2),pn_pal[89]);
    if(wd.cls==PC_BONE){ static const int e[3][3]={{-2,-1,0},{2,-1,0},{0,2,1}};                                                     // the moon is a skull: two eyes and a nose, soft dark sockets
        for(int i=0;i<3;i++){ double ex=((cx+e[i][0])*2+1.0)*sx, ey=(cy+e[i][1]+1.0)*sy+(i==2?sy*0.5:0), rx=(i==2?1.1:2.0)*su, ry=(i==2?1.7:1.3)*su; int X0=(int)(ex-rx)-1, X1=(int)(ex+rx)+2, Y0=(int)(ey-ry)-1, Y1=(int)(ey+ry)+2; if(X0<0) X0=0; if(X1>pc_pn.w) X1=pc_pn.w; if(Y0<pc_pn.ya) Y0=pc_pn.ya; if(Y1>pc_pn.h) Y1=pc_pn.h;
            for(int yy=Y0;yy<Y1;yy++) for(int xx=X0;xx<X1;xx++){ double dx=(xx+0.5-ex)/rx, dy=(yy+0.5-ey)/ry, q=dx*dx+dy*dy; if(q>=1.2) continue; double a=(1-__builtin_sqrt(q))*rx*0.9+0.5; if(a<=0) continue; if(a>1) a=1; pn_sky_over(xx,yy,pn_pal[88],a*0.9); } } }
    return 1;
}
// ---- the ore map (MINIMAP option): the game paints a 30x30 panel of 2x2 dots; here it is a crisp panel with a rounded corner, a faint range ring, smooth round dots with a dark rim
// and a small arrow for you. Same dots (the game's own list, om_i), same colours; the blink of the scanned vein too.
static inline double pn_clamp01(double v){ return v<0?0:v>1?1:v; }
static void pn_mmdot(double cx,double cy,double r,uint32_t col,double a,int rim){
    int W=pc_pn.w, ya=pc_pn.ya, H=pc_pn.h; double e=r+1.8; int X0=(int)(cx-e), X1=(int)(cx+e)+1, Y0=(int)(cy-e), Y1=(int)(cy+e)+1; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
    for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double dx=X+0.5-cx, dy=Y+0.5-cy, d=__builtin_sqrt(dx*dx+dy*dy); uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X];
        if(rim){ double rc=pn_clamp01(r+0.9-d+0.5); if(rc>0) *p=pn_mix(*p,0x05060C,(int)(rc*0.55*255)); } double c=pn_clamp01(r-d+0.5); if(c>0) *p=pn_mix(*p,col,(int)(c*a*255)); } }
static int pc_pn_minimap(int ox,int oy,int R){
    if(!pn_active) return 0; int W=pc_pn.w, H=pc_pn.h, ya=pc_pn.ya; double sx=W/240.0, sy=H/160.0, su=(sx+sy)*0.5;
    double x0=(ox-R-1)*sx, x1=(ox+R+1)*sx, y0=(oy-R-1)*sy, y1=(oy+R+1)*sy, hx=(x1-x0)/2, hy=(y1-y0)/2, mx=(x0+x1)/2, my=(y0+y1)/2, rad=2.2*su, bw=su*0.34+0.8; uint32_t fill=pn_pal[1], bord=pn_pal[C_DGREY];
    int X0=(int)x0-1, X1=(int)x1+2, Y0=(int)y0-1, Y1=(int)y1+2; if(X0<0) X0=0; if(X1>W) X1=W; if(Y0<ya) Y0=ya; if(Y1>H) Y1=H;
    double rr=R*su*0.97;
    for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ double qx=(X<mx?mx-X-0.5:X+0.5-mx)-hx+rad, qy=(Y<my?my-Y-0.5:Y+0.5-my)-hy+rad; double ax=qx>0?qx:0, ay=qy>0?qy:0, d=__builtin_sqrt(ax*ax+ay*ay)+(qx>qy?(qx<0?qx:0):(qy<0?qy:0))-rad;
        double cin=pn_clamp01(0.5-d); if(cin<=0) continue; double cb=cin-pn_clamp01(0.5-(d+bw)); uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X];
        uint32_t base=pn_mix(*p,fill,(int)(cin*0.93*255)); if(cb>0) base=pn_mix(base,bord,(int)(cb*255));
        double dd=__builtin_sqrt((X+0.5-mx)*(X+0.5-mx)+(Y+0.5-my)*(Y+0.5-my)); double ring=pn_clamp01(0.5-__builtin_fabs(dd-rr)+0.0)*0.0; (void)ring;                          // (a faint range ring)
        double rg=pn_clamp01(su*0.12+0.45-__builtin_fabs(dd-rr)); if(rg>0 && cb<0.1) base=pn_mix(base,pn_pal[C_GREY],(int)(rg*0.28*255));
        *p=base; }
    for(int k=0;k<om_n;k++){ PObj *o=&po[om_i[k]]; if(!o->alive || o->kind!=O_ROCK) continue;
        s32 dx=wrapd(o->x-ppx), dy=wrapd(o->y-ppy); double fx=(((double)(dx>>8)*hcs - (double)(dy>>8)*hsn))/65536.0, fy=-(((double)(dx>>8)*hsn + (double)(dy>>8)*hcs))/65536.0;
        if(fx<-R+1||fx>R-1||fy<-R+1||fy>R-1) continue; int ov=ore_of(o->x,o->y); if(ov<0) continue; int c=om_col[ov];
        if(om_i[k]==scan_i && holding(TL_SCANNER) && (frames&8)) c=C_WHITE;
        pn_mmdot((ox+0.5+fx+0.5)*sx,(oy+0.5+fy+0.5)*sy,0.95*su,pn_pal[c],1.0,1); }
    { double cx=(ox+0.5)*sx, cy=(oy+0.5)*sy; double ax=cx, ay=cy-2.7*su, bx=cx-1.9*su, by=cy+1.8*su, ccx=cx+1.9*su, ccy=cy+1.8*su;                                           // you: a small arrow, the way you face is up
        int XX0=(int)(cx-4*su), XX1=(int)(cx+4*su)+1, YY0=(int)(cy-4*su), YY1=(int)(cy+4*su)+1; uint32_t white=pn_pal[C_WHITE];
        for(int Y=YY0;Y<YY1;Y++) for(int X=XX0;X<XX1;X++){ if(X<0||X>=W||Y<ya||Y>=H) continue; double px=X+0.5, py=Y+0.5, m=1e9;
            const double V[3][2]={{ax,ay},{ccx,ccy},{bx,by}}; for(int i=0;i<3;i++){ double ux=V[(i+1)%3][0]-V[i][0], uy=V[(i+1)%3][1]-V[i][1], l=__builtin_sqrt(ux*ux+uy*uy), dd=((px-V[i][0])*uy-(py-V[i][1])*ux)/l; if(dd<m) m=dd; }
            double c=pn_clamp01(m+0.5); if(c>0){ uint32_t *p=&pc_pn.buf[(size_t)(Y-ya)*W+X]; *p=pn_mix(*p,white,(int)(c*255)); } } }
    pn_mmdot((ox+0.5)*sx,(oy-R+0.5)*sy,0.8*su,pn_pal[C_GOLD],1.0,0);                                                                                                           // north
    return 1;
}
