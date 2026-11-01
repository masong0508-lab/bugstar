// pc_dnative.inc: the dungeon at the window's own resolution (NATIVE 3D VIEWS). Included at the end of the game's main.c, so it can read the
// dungeon's own tables (map, textures, light, ramps). It makes a second picture of the same view, one ray per window column, one floor/ceiling
// sample per window pixel, with the game's own light and textures but a smooth shade instead of the 2x2 dither. It only READS the game: nothing
// here changes the game's state. Each pixel is stored as (palette index, next index, fraction) so the compositor can use the live palette (fades).
// The compositor (pc_hw.c) shows it wherever the game's 240x160 page still holds what render_view wrote (the snapshot compare); monsters, the HUD,
// sparks and text stay as the game drew them. 3D VIEWS = CLASSIC (or no window size known) turns it off. Water and polished-floor reflections: not done.
extern void *malloc(unsigned long); extern void free(void*);
extern int pc_nat_w, pc_nat_h, pc_dn_w, pc_dn_h; extern uint32_t *pc_dn_buf; extern long pc_dn_f[2]; extern uint8_t pc_dn_snap[2][240*160];
extern int pc_v3d_classic(void); extern long pc_frames(void);
typedef void (*PcParFn)(void*,int,int); extern void pc_par_chunk(int total, int chunk, PcParFn fn, void *ctx);
typedef struct { double top, bot; const u8 *tcol; const u16 *rp; int L, edge; } DnCol;
typedef struct { int W, H, Y0; double sy, hzf; DnCol *col; double *rxc, *ryc; uint32_t *buf; s32 px, py; } DnCtx;
static inline uint32_t dn_pk(int base, int s){ int sh=s>>4, f=(s&15)*17; if(s<0){ sh=0; f=0; } if(sh>=7){ sh=7; f=0; } return (uint32_t)(base+sh) | (uint32_t)(base+sh+(sh<7))<<8 | (uint32_t)f<<16; }
static inline int dn_lsub(s32 fx, s32 fy){ s32 a=fx-0x2000, b=fy-0x2000; int x0=(a>>14)&127, x1=(x0+1)&127, y0=(b>>14)&127, y1=(y0+1)&127, fxr=(a>>6)&255, fyr=(b>>6)&255;
    int t0=lsub[y0*128+x0]*(256-fxr)+lsub[y0*128+x1]*fxr, t1=lsub[y1*128+x0]*(256-fxr)+lsub[y1*128+x1]*fxr; return (t0*(256-fyr)+t1*fyr)>>16; }
static void dn_rows(void *vc, int lo, int hi){
    DnCtx *c=(DnCtx*)vc; int W=c->W; double sy=c->sy, hz=c->hzf;
    for(int Y=lo+c->Y0;Y<hi+c->Y0;Y++){ double yg=(Y+0.5)/sy; uint32_t *row=c->buf+(unsigned long)(Y-c->Y0)*W;
        int fl=yg>hz, d0=0, d1=0; double fr=0, rd=0; int sl=0, fg=0;
        if(fl || yg<hz){ double dd=fl?yg-hz:hz-yg; if(dd<0.5) dd=0.5; if(dd>98.5) dd=98.5; d0=(int)dd; d1=d0+1; fr=dd-d0; if(d0<1){ d0=1; fr=0; }
            if(fl){ rd=(double)(FOC<<15)/dd; sl=(int)(rl_rel[d0]+(rl_rel[d1]-rl_rel[d0])*fr); fg=(int)(rf_rel[d0]+(rf_rel[d1]-rf_rel[d0])*fr); }
            else { rd=(double)(FOC<<16)/dd; sl=(int)(cl_rel[d0]+(cl_rel[d1]-cl_rel[d0])*fr); fg=(int)(cf_rel[d0]+(cf_rel[d1]-cf_rel[d0])*fr); } }
        for(int X=0;X<W;X++){ const DnCol *k=&c->col[X];
            if(yg>=k->top && yg<k->bot){                                                             // the wall
                if(k->edge || (Y==(int)(k->top*sy)) || (Y==(int)(k->bot*sy))){ row[X]=dn_pk(k->rp[0]&0xFF,0); continue; }
                int v=(int)((yg-k->top)/(k->bot-k->top)*48); if(v>47) v=47; if(v<0) v=0; int t=k->tcol[v], e=k->rp[t>>5];
                int s=(e&0x100) ? (t&31)<<2 : ((t&31)*k->L)>>5; row[X]=dn_pk(e&0xFF,s); continue; }
            s32 fx=c->px+(s32)(rd*c->rxc[X]/65536.0), fy=c->py+(s32)(rd*c->ryc[X]/65536.0); u32 tx=(u32)fx>>16, ty=(u32)fy>>16;
            if(tx>=MW || ty>=MW){ row[X]=dn_pk(R_CEIL,0); continue; }
            int fc=fcode[ty*MW+tx], Lp, lsv=dn_lsub(fx,fy);
            if(fl){ int t=floor_tex[(fc<<10)|(((fy>>11)&31)<<5)|((fx>>11)&31)]; int e=framp[(fc<<3)|(t>>5)], s;
                if(e&0x100) s=(t&31)<<2; else { Lp=sl+((lsv*fg)>>6); if(Lp>127) Lp=127; s=((t&31)*Lp)>>5; }
                row[X]=dn_pk(e&0xFF,s); }
            else { int cc=((fc>=3&&fc<=5)||fc==9); int t=ceil_tex[(cc<<10)|(((fy>>11)&31)<<5)|((fx>>11)&31)]; int e=cramp[(cc<<3)|(t>>5)];
                Lp=sl+((lsv*fg)>>7); if(Lp>127) Lp=127; row[X]=dn_pk(e&0xFF,((t&31)*Lp)>>5); } } }
}
static void pc_dn_native(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    int W=pc_nat_w, H=pc_nat_h; if(W<=240 || H<=160 || pc_v3d_classic()) { pc_dn_w=0; return; }
    static DnCol *col; static double *rxc, *ryc; static int cap; static unsigned long bcap;
    if(W>cap){ free(col); free(rxc); free(ryc); col=malloc(sizeof(DnCol)*W); rxc=malloc(sizeof(double)*W); ryc=malloc(sizeof(double)*W); cap=W; }
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
        int w=wtex[cell], tu=u>>11; if(w<=WT_CRYSTAL && ((cell*7+side)&1)) tu=31-tu;
        int lx=(hx-((rx*5)>>4))>>14, ly=(hy-((ry*5)>>4))>>14; if(lx<0)lx=0; if(lx>127)lx=127; if(ly<0)ly=0; if(ly>127)ly=127;
        int li=perp>>12; if(li>511) li=511; int L=((light_tab[li]*lantk)>>6)+((lsub[(ly<<7)|lx]*fog_tab[li])>>6); L=tonemap[L>511?511:L]; if(side==0) L=(L*48)>>6; L+=((cell*37)&7)-3; if(L<0) L=0; if(L>127) L=127;
        double h=(double)FOC*65536.0/perp; DnCol *k=&col[X]; k->top=hzf-h; k->bot=hzf+h/2; k->tcol=wall_tex+w*1536+tu*48; k->rp=wramp+w*8; k->L=L; k->edge=0; k->L=L; (void)perp;
        k->edge=(int)(perp>>4); }                                                                                 // (perp is kept here for the edge test below)
    { int step=W/120>1?W/120:1; for(int X=0;X<W;X++){ int p=col[X].edge, e=0; if(X>=step && col[X-step].edge-p>0x400) e=1; if(X+step<W && col[X+step].edge-p>0x400) e=1; col[X].L|=e<<16; } for(int X=0;X<W;X++){ col[X].edge=col[X].L>>16; col[X].L&=0xFFFF; } }
    DnCtx c={W,H,Y0,sy,hzf,col,rxc,ryc,pc_dn_buf,px_,py_}; pc_par_chunk(H-Y0,8,dn_rows,&c);
    int t=((uintptr_t)pg)>=0x0600A000u?1:0; pc_dn_w=W; pc_dn_h=H; memcpy(pc_dn_snap[t],(const void*)pg,240*160); pc_dn_f[t]=pc_frames();       // (the page as render_view left it: what changes after is the game's own)
}
