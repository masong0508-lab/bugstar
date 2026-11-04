// pc_dnative.inc: the dungeon at the window's own resolution (NATIVE 3D VIEWS). Included at the end of the game's main.c, so it can read the
// dungeon's own tables (map, textures, light, ramps). It makes a second picture of the same view, one ray per window column, one floor/ceiling
// sample per window pixel, with the game's own light and textures but a smooth shade instead of the 2x2 dither. It only READS the game: nothing
// here changes the game's state. Each pixel is stored as (palette index, next index, fraction) so the compositor can use the live palette (fades).
// The compositor (pc_hw.c) shows it wherever the game's 240x160 page still holds what render_view wrote (the snapshot compare); monsters, the HUD,
// sparks and text stay as the game drew them. 3D VIEWS = CLASSIC (or no window size known) turns it off. Water and polished-floor reflections: not done.
#include <stdlib.h>
extern int pc_nat_w, pc_nat_h, pc_dn_w, pc_dn_h, pc_ships_mode; extern uint32_t *pc_dn_buf, *pc_dn_spl; extern int pc_dn_spl_on; extern uint8_t pc_dn_snap2[2][240*160]; extern long pc_dn_f[2]; extern uint8_t pc_dn_snap[2][240*160];
extern double pc_now_ms(void); extern void pc_dn_report(double,int,int); extern int pc_v3d_classic(void); extern long pc_frames(void);
typedef void (*PcParFn)(void*,int,int); extern void pc_par_chunk(int total, int chunk, PcParFn fn, void *ctx);
typedef struct { double top, bot, inv; int yt, yb; const u16 *rp; int L, edge, perp, mm; u16 hv[48]; u8 nt[48]; } DnCol;
typedef struct { int W, H, Y0; double sy, hzf; DnCol *col; double *rxc, *ryc; uint32_t *buf; s32 px, py; } DnCtx;
#define PC_DN_NONE 0x20000000u        // (this pixel stays the game's own picture)
static inline uint32_t dn_pk(int base, int s){ int sh=s>>4, f=(s&15)*17; if(s<0){ sh=0; f=0; } if(sh>=7){ sh=7; f=0; } return (uint32_t)(base+sh) | (uint32_t)(base+sh+(sh<7))<<8 | (uint32_t)f<<16; }
static inline int dn_lsub(s32 fx, s32 fy){ s32 a=fx-0x2000, b=fy-0x2000; int x0=(a>>14)&127, x1=(x0+1)&127, y0=(b>>14)&127, y1=(y0+1)&127, fxr=(a>>6)&255, fyr=(b>>6)&255;
    int t0=lsub[y0*128+x0]*(256-fxr)+lsub[y0*128+x1]*fxr, t1=lsub[y1*128+x0]*(256-fxr)+lsub[y1*128+x1]*fxr; return (t0*(256-fyr)+t1*fyr)>>16; }
// a floor/ceiling texel with its 4 neighbours blended (only the shade; if the neighbours use other colour ramps the nearest texel wins). Returns the shade in 1/512 units and the ramp in *rmp.
static inline int dn_bil(const u8 *tab, int base, s32 fx, s32 fy, int *rmp, int m){      // m: window pixels per texel (x16): the blend is only as wide as one pixel, so texels stay crisp
    int px=((fx>>6)&1023)-16, py=((fy>>6)&1023)-16; int x0=(px>>5)&31, x1=(x0+1)&31, y0=(py>>5)&31, y1=(y0+1)&31, fu=px&31, fv=py&31;
    fu=((fu-16)*m>>4)+16; if(fu<0) fu=0; if(fu>32) fu=32; fv=((fv-16)*m>>4)+16; if(fv<0) fv=0; if(fv>32) fv=32;
    int a=tab[base|(y0<<5)|x0], b=tab[base|(y0<<5)|x1], c=tab[base|(y1<<5)|x0], d=tab[base|(y1<<5)|x1];
    if((a>>5)==(b>>5) && (a>>5)==(c>>5) && (a>>5)==(d>>5)){ *rmp=a>>5; return (((a&31)*(32-fu)+(b&31)*fu)*(32-fv)+((c&31)*(32-fu)+(d&31)*fu)*fv)>>1; }   // (31*32*32 = 31744, in 1/512 of a shade step... see below)
    int t=(fv>=16)?((fu>=16)?d:c):((fu>=16)?b:a); *rmp=t>>5; return (t&31)<<9; }
static int dn_low;      // 1 = cheap mode (nearest texel instead of the 4-texel blend on floors and ceilings); switched on by pc_dn_native when frames cost too much
static void dn_rows(void *vc, int lo, int hi){
    DnCtx *c=(DnCtx*)vc; int W=c->W; double sy=c->sy, hz=c->hzf;
    for(int Y=lo+c->Y0;Y<hi+c->Y0;Y++){ double yg=(Y+0.5)/sy; uint32_t *row=c->buf+(unsigned long)(Y-c->Y0)*W;
        int fl=yg>hz, d0=0, d1=0; double fr=0, rd=0; int sl=0, fg=0, bil=0, bm=16;
        if(fl || yg<hz){ double dd=fl?yg-hz:hz-yg; if(dd<0.5) dd=0.5; if(dd>98.5) dd=98.5; d0=(int)dd; d1=d0+1; fr=dd-d0; if(d0<1){ d0=1; fr=0; }
            if(fl){ rd=(double)(FOC<<15)/dd; sl=(int)(rl_rel[d0]+(rl_rel[d1]-rl_rel[d0])*fr); fg=(int)(rf_rel[d0]+(rf_rel[d1]-rf_rel[d0])*fr); }
            else { rd=(double)(FOC<<16)/dd; sl=(int)(cl_rel[d0]+(cl_rel[d1]-cl_rel[d0])*fr); fg=(int)(cf_rel[d0]+(cf_rel[d1]-cf_rel[d0])*fr); }
            { double dx=rd*(c->rxc[W>1?W-1:0]-c->rxc[0])/65536.0/(W>1?W-1:1); if(dx<0) dx=-dx; bil = !dn_low && dx<3000.0; { double m=2048.0/(dx>1?dx:1); bm=(int)(m*16); if(bm>256) bm=256; if(bm<16) bm=16; } } }      // (one pixel covers less than 1.5 texels: blend the texels)
        for(int X=0;X<W;X++){ const DnCol *k=&c->col[X];
            if(yg>=k->top && yg<k->bot){                                                             // the wall
                if(k->edge || Y==k->yt || Y==k->yb){ row[X]=dn_pk(k->rp[0]&0xFF,0); continue; }
                double vv=(yg-k->top)*k->inv-0.5; if(vv<0) vv=0; int v0=(int)vv, fv=(int)((vv-v0)*16), v1=v0+1>47?47:v0+1; fv=((fv-8)*k->mm>>4)+8; if(fv<0) fv=0; if(fv>16) fv=16; if(v0>47) v0=47;
                int h0=k->hv[v0], h1=k->hv[v1], e, s;
                if(!((h0|h1)&0x2000) && ((h0>>10)&7)==((h1>>10)&7)){ int r=(h0>>10)&7; e=k->rp[r]; int ts=(h0&1023)*(16-fv)+(h1&1023)*fv;      // all four texels use one ramp: blend the shade
                    s=(e&0x100) ? (ts>>7) : (ts*k->L)>>14; }
                else { int t=k->nt[fv>=8?v1:v0]; e=k->rp[t>>5]; s=(e&0x100) ? (t&31)<<2 : ((t&31)*k->L)>>5; }
                row[X]=dn_pk(e&0xFF,s); continue; }
            s32 fx=c->px+(s32)(rd*c->rxc[X]/65536.0), fy=c->py+(s32)(rd*c->ryc[X]/65536.0); u32 tx=(u32)fx>>16, ty=(u32)fy>>16;
            if(tx>=MW || ty>=MW){ row[X]=dn_pk(R_CEIL,0); continue; }
            int fc=fcode[ty*MW+tx], Lp, lsv=dn_lsub(fx,fy);
            if(fl){ int e, s, ts, rp;
                if(bil){ ts=dn_bil(floor_tex,fc<<10,fx,fy,&rp,bm); } else { int t=floor_tex[(fc<<10)|(((fy>>11)&31)<<5)|((fx>>11)&31)]; rp=t>>5; ts=(t&31)<<9; }
                e=framp[(fc<<3)|rp];
                if(fc==5 || (fc==2 && rp==0)){ row[X]=PC_DN_NONE; continue; }                   // water and polished floors mirror the walls: the game's own picture stays
                if(e&0x100) s=ts>>7; else { Lp=sl+((lsv*fg)>>6); if(Lp>127) Lp=127; s=(ts*Lp)>>14; }
                row[X]=dn_pk(e&0xFF,s); }
            else { int cc=((fc>=3&&fc<=5)||fc==9), ts, rp;
                if(bil){ ts=dn_bil(ceil_tex,cc<<10,fx,fy,&rp,bm); } else { int t=ceil_tex[(cc<<10)|(((fy>>11)&31)<<5)|((fx>>11)&31)]; rp=t>>5; ts=(t&31)<<9; }
                int e=cramp[(cc<<3)|rp];
                Lp=sl+((lsv*fg)>>7); if(Lp>127) Lp=127; row[X]=dn_pk(e&0xFF,(ts*Lp)>>14); } } }
}
// ---- the things (monsters, items, lamps, crates) at the window's size. draw_sprite (the game's) hands each one over after it has drawn it; they go in the sprite layer,
// with the game's own art, tint, darkness and hit flash, one texel per window pixel, hidden behind the wall columns exactly as the game hides them.
typedef struct { const u8 *art; int aw, ah, x0, wcols, top, bot, tint, dark, flash; s32 tY; } DnSpr;
static DnSpr dn_sp[96]; static int dn_nsp, dn_active; static int *dn_perp; static int dn_perp_cap;
static void pc_dn_spr(const u8 *art, int aw, int ah, int x0, int wcols, int top, int bot, s32 tY, int tint, int dark, int flash){
    if(!dn_active || dn_nsp>=96) return; DnSpr *s=&dn_sp[dn_nsp++]; s->art=art; s->aw=aw; s->ah=ah; s->x0=x0; s->wcols=wcols; s->top=top; s->bot=bot; s->tY=tY; s->tint=tint; s->dark=dark; s->flash=flash; }
static void pc_dn_sprites_done(volatile u16 *pg){
    if(!dn_active) return; int W=pc_dn_w, H=pc_dn_h, Y0=(18*H+159)/160; double sy=H/160.0, sx=W/240.0; int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0;
    memcpy(pc_dn_snap2[t],(const void*)pg,240*160);
    if(!dn_nsp){ pc_dn_spl_on=0; return; }
    unsigned long need=(unsigned long)W*(H-Y0); static unsigned long scap; if(need>scap){ free(pc_dn_spl); pc_dn_spl=malloc(need*4); scap=need; } memset(pc_dn_spl,0,need*4); pc_dn_spl_on=1;
    for(int n=0;n<dn_nsp;n++){ const DnSpr *s=&dn_sp[n];
        int XA=(int)(s->x0*2*sx), XB=(int)((s->x0+s->wcols)*2*sx+0.999); int YA=(int)(s->top*sy), YB=(int)(s->bot*sy+0.999);
        if(XA<0) XA=0; if(XB>W) XB=W; if(YA<Y0) YA=Y0; if(YB>H) YB=H;
        double iu=s->aw/((double)s->wcols*2*sx), iv=s->ah/((double)(s->bot-s->top)*sy); double u0=(s->x0*2*sx), v0=(s->top*sy);
        u8 lut[256]; for(int i=0;i<256;i++){ int ix=i; if(ix==1) ix=s->tint; else if(s->dark && ix>=64 && ix<216){ int l=(ix&7)-s->dark; ix=(ix&~7)+(l<0?0:l>7?7:l); }
            if(s->flash){ if(s->flash>=5) ix=R_UP+7; else if(ix>=64 && ix<216){ int l=(ix&7)+3; ix=(ix&~7)+(l>7?7:l); } } lut[i]=(u8)ix; }
        for(int Y=YA;Y<YB;Y++){ int v=(int)((Y+0.5-v0)*iv); if(v<0) v=0; if(v>=s->ah) v=s->ah-1; uint32_t *row=pc_dn_spl+(unsigned long)(Y-Y0)*W; const u8 *ar=s->art+v*s->aw;
            for(int X=XA;X<XB;X++){ if(s->tY>=dn_perp[X]){ if(!row[X]) row[X]=0x40000000u; continue; }
                int u=(int)((X+0.5-u0)*iu); if(u<0) u=0; if(u>=s->aw) u=s->aw-1; int ix=ar[u];
                if(!ix){ if(!row[X]) row[X]=0x40000000u; continue; }
                ix=lut[ix]; row[X]=0x80000000u|(uint32_t)ix|(uint32_t)ix<<8; } } }
}
static void pc_dn_native(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    int W=pc_nat_w, H=pc_nat_h; dn_active=0; dn_nsp=0; pc_dn_spl_on=0; if(terr_on || W<=240 || H<=160 || pc_v3d_classic()) { pc_dn_w=0; return; }
    double a0=pc_now_ms();
    static DnCol *col; static double *rxc, *ryc; static int cap; static unsigned long bcap;
    if(W>cap){ free(col); free(rxc); free(ryc); free(dn_perp); col=malloc(sizeof(DnCol)*W); rxc=malloc(sizeof(double)*W); ryc=malloc(sizeof(double)*W); dn_perp=malloc(sizeof(int)*W); cap=W; }
    int Y0=(18*H+159)/160; unsigned long need=(unsigned long)W*(H-Y0); if(need>bcap){ free(pc_dn_buf); pc_dn_buf=malloc(need*4); bcap=need; }
    double sy=H/160.0; const double hzf=hz;
    for(int X=0;X<W;X++){ double gc=(X+0.5)*120.0/W-0.5; s32 cam=camtab[0]+(s32)((double)(camtab[119]-camtab[0])*gc/119.0);
        s32 rx=dirx+(s32)(((long long)plx*cam)>>16), ry=diry+(s32)(((long long)ply*cam)>>16); rxc[X]=rx; ryc[X]=ry;
        int mx=px_>>16, my=py_>>16, pmx=mx, pmy=my; u32 arx=rx<0?-rx:rx, ary=ry<0?-ry:ry;
        u32 qx=arx>2?recip32(arx):0x7fffffff; if(qx>0x7fffffff) qx=0x7fffffff; u32 qy=ary>2?recip32(ary):0x7fffffff; if(qy>0x7fffffff) qy=0x7fffffff;
        s32 ddx=(s32)qx, ddy=(s32)qy; int sxs=rx<0?-1:1, sys=ry<0?-1:1; s32 fx=px_&0xFFFF, fy=py_&0xFFFF;
        s32 tx=rx<0?(s32)(((long long)fx*ddx)>>16):(s32)(((long long)(0x10000-fx)*ddx)>>16), ty=ry<0?(s32)(((long long)fy*ddy)>>16):(s32)(((long long)(0x10000-fy)*ddy)>>16);
        int side=0, t=D_WALL;
        for(int kk=0;kk<64;kk++){ pmx=mx; pmy=my; if(tx<ty){ tx+=ddx; mx+=sxs; side=0; } else { ty+=ddy; my+=sys; side=1; }
            if(mx<0||my<0||mx>=MW||my>=MW){ t=D_WALL; mx=pmx; my=pmy; break; } t=dmap[my*MW+mx]; if(t==D_WALL||t==D_PILLAR||t==D_LOCK) break; }
        s32 perp=side==0?tx-ddx:ty-ddy; if(perp<0x1000) perp=0x1000; int cell=my*MW+mx;
        s32 hx=px_+(s32)(((long long)perp*rx)>>16), hy=py_+(s32)(((long long)perp*ry)>>16);
        u32 u=(u32)((side==0?hy:hx)&0xFFFF); if((side==0&&rx>0)||(side==1&&ry<0)) u=0xFFFF-u;
        int w=wtex[cell]; if(w<=WT_CRYSTAL && ((cell*7+side)&1)) u=0xFFFF-u;      // (a mirrored wall: mirror u)
        int lx=(hx-((rx*5)>>4))>>14, ly=(hy-((ry*5)>>4))>>14; if(lx<0)lx=0; if(lx>127)lx=127; if(ly<0)ly=0; if(ly>127)ly=127;
        int li=perp>>12; if(li>511) li=511; int L=((light_tab[li]*lantk)>>6)+((lsub[(ly<<7)|lx]*fog_tab[li])>>6); L=tonemap[L>511?511:L]; if(side==0) L=(L*48)>>6; L+=((cell*37)&7)-3; if(L<0) L=0; if(L>127) L=127;
        double h=(double)FOC*65536.0/perp; DnCol *k=&col[X]; double mm=h*1.5*sy/48.0; if(mm<1) mm=1; if(mm>8) mm=8; k->mm=(int)(mm*16); k->top=hzf-h; k->bot=hzf+h/2; k->rp=wramp+w*8; k->L=L; k->perp=perp; dn_perp[X]=perp;
        k->inv=48.0/(k->bot-k->top); k->yt=(int)(k->top*sy); k->yb=(int)(k->bot*sy); k->edge=(int)(perp>>4);   // (edge holds perp until the edge test below)
        { int p=(int)(u>>6)-16, ta=p>>5, fu0=p&31, fu=((fu0-16)*k->mm>>4)+16, ca=ta<0?0:ta, cb=ta+1>31?31:ta+1; if(fu<0) fu=0; if(fu>32) fu=32; if(ca>31) ca=31;
          const u8 *c0=wall_tex+w*1536+ca*48, *c1=wall_tex+w*1536+cb*48;
          for(int v=0;v<48;v++){ int a=c0[v], b=c1[v]; k->nt[v]=(fu0>=16?b:a);
              if((a>>5)==(b>>5)) k->hv[v]=(u16)(((a>>5)<<10)|(((a&31)*(32-fu)+(b&31)*fu)&1023)); else k->hv[v]=0x2000; } } }
    { int step=W/120>1?W/120:1; for(int X=0;X<W;X++){ int p=col[X].edge, e=0; if(X>=step && col[X-step].edge-p>0x400) e=1; if(X+step<W && col[X+step].edge-p>0x400) e=1; col[X].L|=e<<16; } for(int X=0;X<W;X++){ col[X].edge=col[X].L>>16; col[X].L&=0xFFFF; } }
    DnCtx c={W,H,Y0,sy,hzf,col,rxc,ryc,pc_dn_buf,px_,py_}; pc_par_chunk(H-Y0,8,dn_rows,&c);
    int t=((uintptr_t)pg)>=GBAA(0x0600A000u)?1:0; pc_dn_w=W; pc_dn_h=H; memcpy(pc_dn_snap[t],(const void*)pg,240*160); pc_dn_f[t]=pc_frames(); dn_active=1;      // (the page as render_view left it: what changes after is the game's own)
    { double ms=pc_now_ms()-a0; static double ema; static int n, env=-1; if(env<0){ const char *e=getenv("PC_DN_LOW"); env=e?atoi(e):0; }
      ema = n++ ? ema*0.92+ms*0.08 : ms; if(n>30){ if(!dn_low && ema>12.0) dn_low=1; else if(dn_low && ema<3.5) dn_low=0; } if(env) dn_low=env>0;      // slow machine: drop to the cheap mode (an average above 12 ms a frame); back when it is cheap again
      pc_dn_report(ms,W,H); }
}
