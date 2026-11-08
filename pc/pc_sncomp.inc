// pc_sncomp.inc: included by pc_hw.c. The native space view (pc_snative.inc) leaves a window-size picture of the background in pc_sn.buf and a copy of the page
// as it was right after the background. The compositor shows the picture wherever the page still holds that copy and the copy is something the picture redraws
// (black, stars, sun, planets and moons); ships, rocks, station, HUD and cockpit stay the game's own pixels.
typedef struct { int w,h,hn; uint32_t *buf; long f[2]; uint8_t snap[2][240*128]; uint8_t ok[256]; int sunx,suny; uint8_t snap2[2][240*128]; uint8_t *msk; long f2[2]; uint32_t *ibuf; uint8_t foot[2][240*128], fval[2][240*128]; long ff[2]; uint8_t snapc[2][240*128]; long fc[2]; int cpmode[2]; } PcSNat;
PcSNat pc_sn;
typedef struct { uint32_t *out; int W,H,t,fadeop,ev,has2,has3; const uint8_t *pgb,*sn,*sn2; double sx,sy; PcSNat *n; } SnCC;
static void sn_fade(uint32_t *c_,int fadeop,int ev){ uint32_t c=*c_;
    if(fadeop==2){ uint32_t r2=(c>>16&255)+((255-(c>>16&255))*ev>>4), g2=(c>>8&255)+((255-(c>>8&255))*ev>>4), b2=(c&255)+((255-(c&255))*ev>>4); c=r2<<16|g2<<8|b2; }
    else if(fadeop==3){ uint32_t r2=(c>>16&255)*(16-ev)>>4, g2=(c>>8&255)*(16-ev)>>4, b2=(c&255)*(16-ev)>>4; c=r2<<16|g2<<8|b2; }
    *c_=c; }
static void sn_rows_c(void *vc,int lo,int hi){
    SnCC *k=(SnCC*)vc; PcSNat *n=k->n; int W=k->W, t=k->t; const uint8_t *pgb=k->pgb, *sn=n->snap[t], *sn2=k->sn2; uint32_t *out=k->out; int fadeop=k->fadeop, ev=k->ev;
    for(int Y=lo;Y<hi;Y++){ int gy=(int)((Y+0.5)*k->sy); if(gy>127) break; const uint32_t *src=n->buf+(size_t)Y*W;
        for(int X=0;X<W;X++){ int gx=(int)((X+0.5)*k->sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X;
            if((nv_op[q]&7)<4) continue;
            if(k->has3 && n->foot[t][gi] && pgb[gi]==n->fval[t][gi]){ uint32_t c=src[X]; sn_fade(&c,fadeop,ev); out[q]=c; nv_op[q]|=0x40; continue; }                      // a cargo can, spark, bolt or crosshair the game drew blocky: the picture's own background here (the layer goes over it afterwards)
            if(k->has2 && n->msk[q] && pgb[gi]==sn2[gi]){ uint32_t c=src[X]; sn_fade(&c,fadeop,ev); out[q]=c; nv_op[q]|=0x40; continue; }                                // a wireframe the native view redraws: its blocky copy is hidden here
            if(pgb[gi]!=sn[gi]) continue; int ix=sn[gi];
            if(!n->ok[ix] && !(ix==51 && gx>=n->sunx-12 && gx<=n->sunx+12 && gy>=n->suny-12 && gy<=n->suny+12)) continue; uint32_t c=src[X]; sn_fade(&c,fadeop,ev); out[q]=c; nv_op[q]|=0x40; } }
}
static void sn_compose(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcSNat *n=&pc_sn; int t=(dc>>4)&1; if(!n->buf || n->w!=W || n->h!=H || g_frame-n->f[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    bl_y0=0; bl_y1=H*128/160;
    SnCC c; c.out=out; c.W=W; c.H=H; c.t=t; c.pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)); c.sn2=n->snap2[t]; c.has2 = n->msk && g_frame-n->f2[t]<=4; c.has3 = n->msk && g_frame-n->ff[t]<=4;
    c.sx=240.0/W; c.sy=160.0/H; c.fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0; c.ev=ev; c.n=n;
    pc_par_chunk(n->hn,16,sn_rows_c,&c);
}

// the layer of cans, sparks, bolts and crosshair goes over the finished picture (ships included) wherever the page still holds what it held when they were drawn
static int sn_foot_still(int t,int i,uint8_t v){ PcSNat *n=&pc_sn; return n->buf && g_frame-n->ff[t]<=4 && n->foot[t][i] && v==n->fval[t][i]; }
static void sn_items(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcSNat *n=&pc_sn; int t=(dc>>4)&1; if(!n->ibuf || n->w!=W || n->h!=H || g_frame-n->ff[t]>4 || g_frame-n->f2[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    if(bmode>=2 && ev>0 && (bldcnt&4)) return;
    const uint8_t *pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)), *sn2=n->snap2[t]; double sy=160.0/H, sx=240.0/W;
    for(int Y=0;Y<n->hn;Y++){ int gy=(int)((Y+0.5)*sy); if(gy>127) break; const uint32_t *src=n->ibuf+(size_t)Y*W;
        for(int X=0;X<W;X++){ uint32_t d=src[X]; if(!d) continue; int gx=(int)((X+0.5)*sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X;
            if((nv_op[q]&7)<4) continue; if(!(pgb[gi]==sn2[gi] || sn_foot_still(t,gi,pgb[gi]))) continue;
            uint32_t o=out[q]; int a=255-(int)(d>>24); int r=(d>>16&255)+((o>>16&255)*a>>8), g=(d>>8&255)+((o>>8&255)*a>>8), b=(d&255)+((o&255)*a>>8); if(r>255) r=255; if(g>255) g=255; if(b>255) b=255; out[q]=(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; } }
}

// ---- the cockpit frame at the window's size. The canopy (corners, pillars, top line) and the dash (panel, scanner ring) are shapes: here they are drawn smooth, with soft edges, from
// the same geometry the game uses (blocky pixels of palette 57 and 58). cp_E says which game pixel the game paints with which of the two colours; a pixel of the page that still holds that
// value is the frame (hidden, replaced by the smooth one over the background picture); the soft fringe outside it goes over what is composed there while the page still holds what it held when
// the frame was done (pc_sn.snapc). Text, bars and blips the game puts on the dash stay its own pixels.
static uint8_t cp_E[2][240*160];                       // [cockpit view or not][game pixel] expected colour (0 = the game draws no frame there)
static void cp_build_E(void){
    static int done; if(done) return; done=1;
    for(int v=0;v<2;v++){ uint8_t *E=cp_E[v]; memset(E,0,240*160);
        #define R_(x0,y0,w,h,c) for(int yy=(y0);yy<(y0)+(h);yy++) for(int xx=(x0);xx<(x0)+(w);xx++) if(xx>=0&&xx<240&&yy>=0&&yy<160) E[yy*240+xx]=(c)
        if(v){ for(int y=0;y<18;y++){ int w=(18-y)*2+2; R_(0,y,w,1,57); R_(240-w,y,w,1,57); } for(int y=0;y<14;y++){ int w=(14-y)*2; R_(0,127-y,w,1,57); R_(240-w,127-y,w,1,57); }
            R_(0,18,3,95,57); R_(237,18,3,95,57); R_(0,0,240,1,58); R_(3,18,1,95,58); R_(236,18,1,95,58); }
        R_(0,128,240,32,57); R_(0,128,240,1,58);
        static const signed char sp[32][2]={{44,0},{43,3},{40,5},{35,7},{29,9},{22,10},{15,11},{7,12},{0,12},{-7,12},{-15,11},{-22,10},{-29,9},{-35,7},{-40,5},{-43,3},{-44,0},{-43,-3},{-40,-5},{-35,-7},{-29,-9},{-22,-10},{-15,-11},{-7,-12},{0,-12},{7,-12},{15,-11},{22,-10},{29,-9},{35,-7},{40,-5},{43,-3}};
        for(int i=0;i<32;i++) E[(144+sp[i][1])*240+120+sp[i][0]]=58;
        #undef R_
    }
}
typedef struct { int W,H; uint16_t *lay; int *l1,*r0; } CpLay;      // per window row: the layer (57 coverage in the low byte, 58 in the high byte) and where its left and right parts end / start
static CpLay cp_lay[2];
static inline int cp_in57(double x,double y,int v){          // x, y in game pixels
    if(y>=128) return y<160; if(!v || y<0) return 0; if(x>120) x=240-x; if(x<0) return 0;
    double b = y<18 ? 39-2*y : y<114 ? 3 : 3+(y-114)*(26.0/14.0); if(y<18 && b<3) b=3; return x<b; }
static inline int cp_in58(double x,double y,int v){
    if(y>=128 && y<129) return 1; if(!v) return 0; if(y>=0 && y<1) return 1; if(y>=18 && y<113 && ((x>=3&&x<4)||(x>=236&&x<237))) return 1; return 0; }
static void cp_make(int v,int W,int H){
    CpLay *L=&cp_lay[v]; if(L->lay && L->W==W && L->H==H) return; free(L->lay); free(L->l1); free(L->r0); L->lay=(uint16_t*)calloc((size_t)W*H,2); L->l1=(int*)calloc(H,sizeof(int)); L->r0=(int*)calloc(H,sizeof(int)); L->W=W; L->H=H;
    double sx=240.0/W, sy=160.0/H, S=W/240.0; const int N=4;
    for(int Y=0;Y<H;Y++){ double gy0=Y*sy, gy1=(Y+1)*sy; int lend=0, rbeg=W; L->r0[Y]=W; L->l1[Y]=0; if(!v && gy1<=128.0) continue;
        double reach = gy1<=1.0 ? 240 : gy1<=18 ? 39.5 : gy1<=113 ? 4.5 : gy1<=128 ? 3.5+(gy1-114)*(26.0/14.0) : 240;           // how far in from each side the shapes reach on this row (game pixels)
        int xl=(int)(reach/sx)+2, xr=W-xl; if(xl>W/2){ xl=W/2; xr=W/2; }
        for(int pass=0;pass<2;pass++){ int xa=pass?xr:0, xb=pass?W:xl;
            for(int X=xa;X<xb;X++){ double gx0=X*sx, gx1=(X+1)*sx; int c57=0,c58=0;
                for(int j=0;j<N;j++) for(int i=0;i<N;i++){ double x=gx0+(i+0.5)*(gx1-gx0)/N, y=gy0+(j+0.5)*(gy1-gy0)/N; if(cp_in57(x,y,v)) c57++; if(cp_in58(x,y,v)) c58++; }
                if(gy1>128.0){                          // the scanner ring: a curve 1 game pixel thick (an ellipse 44 by 12 round the middle of the dash), soft at its edges
                    double x=(gx0+gx1)*0.5, y=(gy0+gy1)*0.5; double dx=x-120.5, dy=y-144.5, f=__builtin_sqrt(dx*dx/(44.0*44.0)+dy*dy/(12.0*12.0));
                    if(f>0.5 && f<1.6){ double gxx=dx/(44.0*44.0), gyy=dy/(12.0*12.0), g=__builtin_sqrt(gxx*gxx+gyy*gyy)/f, d=(f-1)/g; double cv=0.5+(0.5-(d<0?-d:d))*S; if(cv>0){ if(cv>1) cv=1; int add=(int)(cv*N*N+0.5); if(add>c58) c58=add; } } }
                int a57=c57*255/(N*N), a58=c58*255/(N*N); if(a58>255) a58=255;
                if(a57||a58){ L->lay[(size_t)Y*W+X]=(uint16_t)(a57|(a58<<8)); if(!pass) lend=X+1; else if(rbeg==W) rbeg=X; } } }
        L->l1[Y]=lend; L->r0[Y]=rbeg; }
}
static inline uint32_t cp_pal(int i){ uint16_t cc=R16(PALRAM+i*2); return (uint32_t)((cc&31)<<3|(cc&31)>>2)<<16 | (uint32_t)(((cc>>5)&31)<<3|((cc>>5)&31)>>2)<<8 | (uint32_t)(((cc>>10)&31)<<3|((cc>>10)&31)>>2); }
static inline uint32_t cp_mix(uint32_t a, uint32_t b, int f){ if(f<0) f=0; if(f>256) f=256; uint32_t rb=((a&0xFF00FF)*(256-f)+(b&0xFF00FF)*f)>>8, g=((a&0xFF00)*(256-f)+(b&0xFF00)*f)>>8; return (rb&0xFF00FF)|(g&0xFF00); }
static void sn_cockpit(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcSNat *n=&pc_sn; int t=(dc>>4)&1; if(!n->buf || n->w!=W || n->h!=H || g_frame-n->f[t]>4 || g_frame-n->fc[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    int v=n->cpmode[t]; cp_build_E(); cp_make(v,W,H); CpLay *L=&cp_lay[v]; const uint8_t *E=cp_E[v]; const uint8_t *pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)), *p1=n->snapc[t];
    int fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0; uint32_t c57=cp_pal(57), c58=cp_pal(58); sn_fade(&c57,fadeop,ev); sn_fade(&c58,fadeop,ev);
    double sx=240.0/W, sy=160.0/H;
    for(int Y=0;Y<H;Y++){ const uint16_t *lay=L->lay+(size_t)Y*W; int gy=(int)((Y+0.5)*sy); if(gy>159) gy=159; int l1=L->l1[Y], r0=L->r0[Y]; if(l1==0 && r0==W) continue;
        for(int pass=0;pass<2;pass++){ int xa=pass?r0:0, xb=pass?W:l1;
            for(int X=xa;X<xb;X++){ uint16_t a=lay[X]; if(!a) continue; int gx=(int)((X+0.5)*sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X; int e=E[gi]; uint32_t base;
                if(e && pgb[gi]==e){ if(gy<128){ if(Y>=n->hn) continue; base=n->buf[(size_t)Y*W+X]; sn_fade(&base,fadeop,ev); } else { base=cp_pal(57); sn_fade(&base,fadeop,ev); } }
                else if(!e && gy<128 && pgb[gi]==p1[gi]) base=out[q];
                else continue;
                int a1=a&255, a2=a>>8; if(a1) base=cp_mix(base,c57,a1+(a1>>7)); if(a2) base=cp_mix(base,c58,a2+(a2>>7)); out[q]=base; } } }
}
