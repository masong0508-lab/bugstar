// pc_kartnat_r.inc: the kart race NATIVE/ULTRA ground extras, included into pc_hw.c (own file on purpose). It only READS what the game hands over
// each frame (pc_kn_frame, called from the game's m7_present through pc_kartnat.inc) and changes how the picture is MADE, never the game:
//   BANKING: the world (ground, horizon, sky, far hills) leans into corners (a small roll about the horizon centre, eased).
//   HILLS:   a made-up height along the road (from the track's seed and the player's place on it) lifts and drops the ground ahead, sprites follow.
//   SHADOWS: soft shadows under the player's kart and the rivals (the game's own dithered ones are skipped in this mode).
// CLASSIC never gets here (pc_v3d_classic) so the GBA picture is unchanged. PC_KN=0 turns it all off (A/B tests).
double pc_now_ms(void);
#define KN_FOCP 120.0
static struct { int active; double roll, prevh, curv; int hinit; short rowz[160]; double sh[160]; int g0; double piv, s0, elev, cs, sn; unsigned seed; int knp; int nsh; double shx[72], shy[72], shw[72]; short kidx[4200]; } KN;
static double kn_cpu(void){ struct timespec a; clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&a); return a.tv_sec*1e3+a.tv_nsec/1e6; }
static int kn_on; static int kn_x;                     // this picture: extras are used
static uint32_t *kn_buf; static size_t kn_cap;
static int kn_disabled(void){ static int e=-2; if(e==-2){ const char *s=getenv("PC_KN"); e = s && atoi(s)==0; } return e; }
int pc_kn_on(void){ return !kn_disabled() && pc_nat_w>240 && !pc_v3d_classic(); }
static double kn_H(double s){ double a=6.283185307179586*s/(KN.knp>8?KN.knp:8); unsigned h=KN.seed; double p1=(h&255)/40.0, p2=((h>>8)&255)/40.0; int n1=3+((h>>16)&3), n2=8+((h>>20)&3);
    return 0.15*sin(n1*a+p1)+0.06*sin(n2*a+p2); }
void pc_kn_frame(const short *rowz, int active, int pi256, int heading, int curvahead, int elev, unsigned seed, int knp, int nsh, const short *shx, const short *shy, const short *shw, int pcx, int pcy, int pcw){
    KN.active=active; if(!active){ KN.hinit=0; KN.roll*=0.8; return; }
    memcpy(KN.rowz,rowz,sizeof KN.rowz); KN.s0=pi256/256.0; KN.elev=elev; KN.seed=seed; KN.knp=knp; KN.curv=curvahead;
    int dh=0; if(KN.hinit){ dh=((heading-(int)KN.prevh+512)&1023)-512; } KN.prevh=heading; KN.hinit=1;
    double tgt=(-dh*0.010 - curvahead*0.0007); if(tgt>0.13) tgt=0.13; if(tgt<-0.13) tgt=-0.13; KN.roll+=(tgt-KN.roll)*0.12; if(getenv("PC_KNLOG")) fprintf(stderr,"KNLOG heading %d dh %d curv %d roll %.3f\n",heading,dh,curvahead,KN.roll);
    int n=0; if(pcw>0){ KN.shx[n]=pcx; KN.shy[n]=pcy; KN.shw[n]=pcw; n++; } for(int i=0;i<nsh && n<72;i++){ KN.shx[n]=shx[i]; KN.shy[n]=shy[i]; KN.shw[n]=shw[i]; n++; } KN.nsh=n;
}
static double kn_shi(double L){ if(L<=0) return KN.sh[0]; if(L>=159) return KN.sh[159]; int l=(int)L; double f=L-l; return KN.sh[l]*(1-f)+KN.sh[l+1]*f; }
static void kn_fwd(double x, double y, double *ox, double *oy){ double yh=y+kn_shi(y-0.5), dx=x-120, dy=yh-KN.piv; *ox=120+dx*KN.cs-dy*KN.sn; *oy=KN.piv+dx*KN.sn+dy*KN.cs; }
static int kn_gnd(int l){ return ((lst[l].dc&0x410)==0x410) && lst[l].pa!=0; }
static int kn_prep(int W, int H, int mode){            // before the sprites: the tables for this picture
    kn_on=0; if(kn_disabled() || !KN.active || !lst_dma || mode!=4 || W<=240 || nv_v3()==2) return 0;
    int g0=-1; for(int l=19;l<160;l++) if(kn_gnd(l)){ g0=l; break; } if(g0<0) return 0;
    KN.g0=g0; double s0=KN.s0, h0=kn_H(s0);
    for(int l=0;l<160;l++){ double sh=0; if(kn_gnd(l) && KN.rowz[l]>0){ double z8=KN.rowz[l]; double hr=(kn_H(s0+z8/128.0)-h0)*KN.elev; sh=-hr*KN_FOCP/z8; if(sh>22) sh=22; if(sh<-22) sh=-22; } KN.sh[l]=sh; }
    for(int l=g0-1;l>=0;l--) KN.sh[l]=KN.sh[g0];
    { double a=0.0; for(int l=g0+1;l<160;l++) if(!kn_gnd(l) && 0) a=1; (void)a; }
    for(int l=g0;l<160;l++) if(!kn_gnd(l)) KN.sh[l]=KN.sh[l>g0?l-1:l];
    KN.piv=g0+KN.sh[g0]; { const char *fr=getenv("PC_KNROLL"); if(fr) KN.roll=atof(fr); } KN.cs=cos(KN.roll); KN.sn=sin(KN.roll);
    for(int i=0;i<4200;i++){ double yh=-30+i*(1.0/16), L=yh; for(int k=0;k<4;k++) L=yh-kn_shi(L); if(L<0) L=0; if(L>159.99) L=159.99; KN.kidx[i]=(short)(int)(L*8); }       // the hills undone: where (game row) the picture row yh comes from
    kn_on=1; return 1; }
typedef struct { double pa,pc,rx,ry,fp,ifp,fog; int g; } KnL;
static KnL knl[160]; static KnL knf[160*8+8]; static uint32_t kn_bd888; static int kn_W, kn_H_; static double kn_sx, kn_sy; static uint16_t kn_hill[2];
static void kn_fine(void); static void kn_tab(void){
    for(int l=0;l<160;l++){ const LineSt *A=&lst[l]; KnL *k=&knl[l]; k->g=kn_gnd(l); k->pa=A->pa/256.0; k->pc=A->pc/256.0; k->rx=A->rx/256.0; k->ry=A->ry/256.0;
        k->fog=-1; uint16_t bc=A->bldcnt; if(((bc>>6)&3)==1 && (bc&4) && (bc&0x2000)){ double eb=((A->bldalpha>>8)&31)/16.0; k->fog=eb>1?1:eb; }
        const LineSt *B=&lst[l<159?l+1:l]; double g=120*kn_sx; double du=(B->rx+B->pa*g)/256.0-(A->rx+A->pa*g)/256.0, dv=(B->ry+B->pc*g)/256.0-(A->ry+A->pc*g)/256.0;
        double fy=sqrt(du*du+dv*dv)*kn_sy, fx=sqrt(k->pa*k->pa+k->pc*k->pc)*kn_sx; double F=fx>fy?fx:fy; if(F<0.0625) F=0.0625; k->fp=F; k->ifp=1.0/F; } }
static void kn_fine(void){ for(int i=0;i<160*8+8;i++){ double Ls=(i+0.5)/8.0; if(Ls>159.99) Ls=159.99; int N=(int)Ls; KnL *o=&knf[i]; o->g=knl[N].g; if(!o->g) continue;
        double lc=Ls-0.5; int l0=(int)floor(lc); if(l0<0) l0=0; int l1=l0<159?l0+1:l0; double f=lc-l0; if(f<0) f=0; if(f>1) f=1; const KnL *A=&knl[l0], *B=&knl[l1]; if(!(A->g && B->g)){ A=B=&knl[N]; f=0; }
        o->pa=A->pa*(1-f)+B->pa*f; o->pc=A->pc*(1-f)+B->pc*f; o->rx=A->rx*(1-f)+B->rx*f; o->ry=A->ry*(1-f)+B->ry*f; o->fp=A->fp*(1-f)+B->fp*f; o->ifp=1.0/o->fp;
        double fg=knl[N].fog>=0 ? (A->fog*(1-f)+B->fog*f) : -1; o->fog=fg>1?1:fg; } }
static inline int kn_world(uint16_t c){ return c==kn_hill[0] || c==kn_hill[1]; }
static inline void kn_uv(const KnL *K, double x0, double *u, double *v){ double g=x0-0.5; *u=K->rx+K->pa*g; *v=K->ry+K->pc*g; }
static int8_t kn_gt[65536]; static int kn_gt_ok;
static inline uint32_t kn_grain(uint32_t c, float u, float v, float F){      // nv_grain with its hash made once into a table (same look, no hashing per pixel)
    double k=(0.5-F)/0.5; int iu=(int)(u*5), iv=(int)(v*5); int d=(int)kn_gt[(iu&255)<<8|(iv&255)]*(int)(k*9)/128;
    int r=(c>>16&255)+d, g=(c>>8&255)+d, b=(c&255)+d; r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b; return (uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; }
static void kn_gtab(void){ if(kn_gt_ok) return; kn_gt_ok=1; for(int iu=0;iu<256;iu++) for(int iv=0;iv<256;iv++){ uint32_t h=(uint32_t)iu*2654435761u ^ (uint32_t)iv*2246822519u; h^=h>>15; h*=2246822519u; h^=h>>13; kn_gt[iu<<8|iv]=(int8_t)((int)(h&255)-128); } }
static uint32_t kn_skycol[1300];
static void kn_skytab(void){ uint8_t lut5[32]; for(int i=0;i<32;i++) lut5[i]=(uint8_t)(i<<3|i>>2);
    #define KN_C888(p) ((uint32_t)lut5[(p)&31]<<16|(uint32_t)lut5[((p)>>5)&31]<<8|lut5[((p)>>10)&31])
    for(int ki=0;ki<1300;ki++){ double Ls=ki*0.125+0.0625; int N=(int)Ls; uint32_t col=0; if(N>=19 && N<160 && skyok[N]){ double p=Ls-0.5-N; int nb=N+(p>0?1:-1); double w=p<0?-p:p; if(w>1) w=1; uint32_t ca=KN_C888(skym[N]); col=ca; if(nb>=19 && nb<160 && skyok[nb]) col=mix888(ca,KN_C888(skym[nb]),w); } kn_skycol[ki]=col; } }
static void kn_rows(void *vc, int lo, int hi){
    (void)vc; int W=kn_W; double sx=kn_sx, sy=kn_sy, c=KN.cs, s=KN.sn; uint8_t lut5[32]; for(int i=0;i<32;i++) lut5[i]=(uint8_t)(i<<3|i>>2);
    #define KN_C888(p) ((uint32_t)lut5[(p)&31]<<16|(uint32_t)lut5[((p)>>5)&31]<<8|lut5[((p)>>10)&31])
    for(int Y=lo;Y<hi;Y++){ double yg=(Y+0.5)*sy; uint32_t *row=kn_buf+(size_t)Y*W;
        if(yg<19){ for(int X=0;X<W;X++) row[X]=0; continue; }
        double dy=yg-KN.piv; int no=(int)yg; if(no>159) no=159; int ognd=kn_gnd(no);
        double x0b=120+(0.5*sx-120)*c+dy*s, yhb=KN.piv-(0.5*sx-120)*s+dy*c, dxs=sx*c, dys=sx*s;      // (x0 and yh step the same each pixel)
        double tb=(yhb+30)*16, ts=dys*16; const short *kidx=KN.kidx; const double xs0=0.5*sx;
        for(int X=0;X<W;){
            double Xf=X; int pair=0, ti=(int)(tb-ts*Xf); if(ti<0) ti=0; if(ti>4198) ti=4198; int ki=kidx[ti]; const KnL *K=&knf[ki];
            if(X+1<W && !(K->g && K->fp<1.6)){ pair=1; Xf=X+0.5; ti=(int)(tb-ts*Xf); if(ti<0) ti=0; if(ti>4198) ti=4198; ki=kidx[ti]; K=&knf[ki]; if(K->g && K->fp<1.6){ pair=0; Xf=X; ki=kidx[(int)(tb-ts*Xf)<0?0:(int)(tb-ts*Xf)>4198?4198:(int)(tb-ts*Xf)]; K=&knf[ki]; } }   // far ground and sky: two pixels at a time (one texel is 5 pixels there, you cannot see it)
            double x0=x0b+dxs*Xf; if(x0<0) x0=0; if(x0>239.99) x0=239.99;
            uint32_t pix=0; int n=pair?2:1; int ok0=1, ok1=pair;
            if(!ognd){ if(!skyok[no]) ok0=ok1=0; else { int xo=(int)(xs0+sx*X); if(xo>239) xo=239; uint16_t oc=bgres[no][xo]; if(bghave[no][xo] && oc!=skym[no] && !kn_world(oc)) ok0=0;
                if(pair){ xo=(int)(xs0+sx*(X+1)); if(xo>239) xo=239; oc=bgres[no][xo]; if(bghave[no][xo] && oc!=skym[no] && !kn_world(oc)) ok1=0; } } }   // the HUD, stars and text stay as the game made them
            if(ok0|ok1){
            if(K->g){ double fp=K->fp, fog=K->fog; double g=x0-0.5, u=K->rx+K->pa*g, v=K->ry+K->pc*g; uint32_t col;
                if(u<0||v<0||u>=240||v>=160) goto none;
                float ifp=(float)K->ifp;
                if(fp<1.6){ col=nv_bil(1,(float)u,(float)v,ifp); if(fp<0.5 && !kn_x) col=kn_grain(col,(float)u,(float)v,(float)fp); }
                else { double du=K->pa*sx*0.25, dv=K->pc*sx*0.25; uint32_t t0=nv_bil(1,(float)(u-du),(float)(v-dv),2*ifp), t1=nv_bil(1,(float)(u+du),(float)(v+dv),2*ifp);
                    col=((t0>>1)&0x7F7F7F)+((t1>>1)&0x7F7F7F)+((t0&t1)&0x010101); }
                int fg=fog>0?(int)(fog*256+0.5):0; if(fg){ uint32_t rb=((col&0xFF00FF)*(256-fg)+(kn_bd888&0xFF00FF)*fg)>>8, gg=((col&0xFF00)*(256-fg)+(kn_bd888&0xFF00)*fg)>>8; col=(rb&0xFF00FF)|(gg&0xFF00); }
                pix=0xC0000000u|col; }
            else { int N=ki>>3; if(N<19 || !skyok[N]) goto none; uint16_t pc2=bgres[N][(int)x0]; uint32_t col;
                if(kn_world(pc2) && bghave[N][(int)x0]) col=KN_C888(pc2); else col=kn_skycol[ki];
                pix=0x80000000u|col; }
            }
            none:
            row[X]=ok0?pix:0; if(n==2) row[X+1]=ok1?pix:0; X+=n; } } }
static void kn_shadows(int W, int H){
    for(int i=0;i<KN.nsh;i++){ double fx,fy; kn_fwd(KN.shx[i],KN.shy[i],&fx,&fy); double w=KN.shw[i]*1.25, h=w*0.32; if(w<2) continue;
        int X0=(int)floor((fx-w*1.3)/kn_sx), X1=(int)ceil((fx+w*1.3)/kn_sx), Y0=(int)floor((fy-h*1.5)/kn_sy), Y1=(int)ceil((fy+h*1.5)/kn_sy);
        if(X0<0) X0=0; if(Y0<0) Y0=0; if(X1>W) X1=W; if(Y1>H) Y1=H;
        for(int Y=Y0;Y<Y1;Y++) for(int X=X0;X<X1;X++){ uint32_t *p=&kn_buf[(size_t)Y*W+X]; if(!(*p&0x40000000u)) continue;
            double ex=((X+0.5)*kn_sx-fx)/w, ey=((Y+0.5)*kn_sy-fy)/h, r2=ex*ex+ey*ey; if(r2>=1.7) continue; double t=(1.7-r2)/1.3; if(t>1) t=1; double d=0.55*t*t*(3-2*t);
            uint32_t col=*p; int r=(int)((col>>16&255)*(1-d)), g=(int)((col>>8&255)*(1-d)), b=(int)((col&255)*(1-d*0.8)); *p=(*p&0xFF000000u)|(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; } } }
static void kn_pass(int W, int H, double sx, double sy, uint16_t bd){
    size_t need=(size_t)W*H; if(need>kn_cap){ free(kn_buf); kn_buf=malloc(need*4); kn_cap=need; }
    kn_W=W; kn_H_=H; kn_sx=sx; kn_sy=sy; kn_bd888=(uint32_t)((bd&31)<<3|(bd&31)>>2)<<16 | (uint32_t)(((bd>>5)&31)<<3|((bd>>5)&31)>>2)<<8 | (((bd>>10)&31)<<3|((bd>>10)&31)>>2);
    kn_hill[0]=R16(PALRAM+108*2); kn_hill[1]=R16(PALRAM+109*2); kn_hill[0]=rgbpix(kn_hill[0]); kn_hill[1]=rgbpix(kn_hill[1]);
    kn_x=getenv("PC_KNX")?atoi(getenv("PC_KNX")):0; double t0=pc_now_ms(); double c0=kn_cpu(); kn_tab(); kn_fine(); kn_skytab(); kn_gtab(); pc_par(H,kn_rows,0); double t1=pc_now_ms(); kn_shadows(W,H); if(getenv("PC_KNT")) fprintf(stderr,"roll %.3f curv %.0f kn rows %.2f ms (wall) %.2f ms (cpu, all threads) shadows %.2f\n",KN.roll,KN.curv,t1-t0,kn_cpu()-c0,pc_now_ms()-t1); }
