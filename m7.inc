// ================================================================= THE KART RACES' GROUND: drawn by the GBA itself ("mode 7")
// The map around the kart is copied into bitmap page 1, and at every HBlank a DMA tells the screen which strip of that map the
// next scanline shows, at what angle and scale (BG2's affine registers). So the road is drawn at full resolution, in true
// perspective, at no CPU cost. Hills come from a 1D profile under the camera's centre line: each screen row is given the depth
// where the ground first reaches it. Page 0 holds the sky, the HUD and the top of any loop-de-loop, in two halves (80 rows
// each), so one half is drawn while the other shows. The old column renderer (render_planet) still draws the planet on foot.
typedef struct { s16 pa, pb, pc, pd; s32 x, y; } M7Line;   // BG2's affine registers, 0x20..0x2F
#define M7_TAB(t) ((M7Line*)((t) ? 0x06013600 : 0x06009600))   // a table of 160 lines each (line 0 is set by the CPU, lines 1..159 by the DMA): the two unused ends of the bitmap pages
typedef u16 M7Dc[160];
static M7Dc *m7_dc;                              // per line: which page shows (DISPCNT), the same way (2 x 160, in the world builder's scratch)
static s16 *m7_rowz;                             // per screen row: the depth it shows, 8.8 cells (0 = sky)
typedef u16 M7Fog[160];
static M7Fog *m7_fog;                            // per line: BLDALPHA, the haze (2 x 160): far ground fades into the horizon's sky colour (DMA0)
#define M7_BLD ((1<<2)|(1<<6)|(1<<13))          // BLDCNT: BG2 blended with the backdrop (which is the horizon colour)
#define M7_PG0 ((volatile u16*)0x06000000)
#define M7_PG1 ((volatile u16*)0x0600A000)
#define M7_HUD 64                               // the HUD lives in the first 64 rows: they always come from page 0
static int m7_tab=0, m7_copy=1;                 // the table on show (the other one is being built); the map must be copied again
static volatile int m7_arm=0, m7_armed=0;       // arm: a table is ready. armed: the DMA is running
static int m7_wx0=0, m7_wy0=0, m7_nwx0=0, m7_nwy0=0;   // the map window in page 1 (its top-left texel), and the next one
static int m7_horizon=160, m7_nstrip=160, m7_cock=0;   // where the ground stops, the strip being built, cockpit view
static int m7_sy=0;                             // the kart's screen row (for its dust and sparks)
static s32 m7_sppx, m7_sppy, m7_scamh; static int m7_shsn, m7_shcs, m7_sphz, m7_sloop, m7_shide, m7_shead, m7_sfp, m7_want=0;   // a snapshot of the camera the frame on show was drawn with
#if defined(TEST_FPS) || defined(TEST_KLOG)
static u32 m7_t[6], kt_v[4], kt_t0, kt_d[4], kfr_sum=0, kfr_n=0, kfr_avg=0, kh_t[8];
#endif
static inline void m7_wr8(volatile u16 *pg, int u, int v, int c){ volatile u16 *p=pg+((v*240+u)>>1); if(u&1) *p=(*p&0x00FF)|(c<<8); else *p=(*p&0xFF00)|c; }
OVK static void m7_pix(int x, int y, int c){         // a pixel of the screen: into the sky half, or onto the map texel the ground shows there
    KCNT(59,1);
    if(x<0||x>=240||y<0||y>=160) return;
    if(y<m7_strip || (m7_cock && y>=140)){ m7_wr8(M7_PG0,x,y,c); return; }
    if(!(m7_dc[m7_tab][y]&(1<<4))) return; const M7Line *L=&M7_TAB(m7_tab)[y];
    int u=(L->x+L->pa*x)>>8, v=(L->y+L->pc*x)>>8; if((unsigned)u<240 && (unsigned)v<160) m7_wr8(M7_PG1,u,v,c);
}
static inline void m7_wt(int u, int v, int c){ KCNT(57,1); if((unsigned)u<240 && (unsigned)v<160) m7_wr8(M7_PG1,u,v,c); }
OVK static void m7_gspan(s32 U0, s32 V0, s32 U1, s32 V1, int c){      // the texels a row of the screen passes over, from (U0,V0) to (U1,V1) (8.8 texels), each painted once
    s32 dU=U1-U0, dV=V1-V0; int sw = (dV<0?-dV:dV) > (dU<0?-dU:dU);                              // walk along whichever axis moves more
    s32 P0,Q0,P1,Q1,dP,dQ; if(sw){ P0=V0; Q0=U0; P1=V1; Q1=U1; dP=dV; dQ=dU; } else { P0=U0; Q0=V0; P1=U1; Q1=V1; dP=dU; dQ=dV; }
    int m=P0>>8, m1=P1>>8; if(m==m1 || !dQ){ int ia=Q0>>8, ib=Q1>>8; if(!dQ){ ib=ia; }
        if(m==m1){ int st = ib>=ia ? 1 : -1; for(int q=ia;;q+=st){ if(sw) m7_wt(q,m,c); else m7_wt(m,q,c); if(q==ib) break; } return; }
        int dir = dP>0 ? 1 : -1; for(;;m+=dir){ if(sw) m7_wt(ia,m,c); else m7_wt(m,ia,c); if(m==m1) break; } return; }    // (straight along the axis: no minor steps at all)
    int dir = dP>0 ? 1 : -1; s32 qs=(dQ*256)/dP, qa=Q0, qb;                                          // (qs: the minor axis moves this much for each whole texel along the major one)
    { s32 Pb = dir>0 ? ((m+1)<<8) : (m<<8); qb=Q0+(((Pb-P0)*qs)>>8); }
    for(;;){ if(m==m1) qb=Q1;
        int ia=qa>>8, ib=qb>>8;
        if(ia==ib){ if(sw) m7_wt(ia,m,c); else m7_wt(m,ia,c); }
        else { int st = ib>ia ? 1 : -1; for(int q=ia;;q+=st){ if(sw) m7_wt(q,m,c); else m7_wt(m,q,c); if(q==ib) break; } }
        if(m==m1) break; m+=dir; qa=qb; qb=qa+(dir>0?qs:-qs); } }
OVK static void m7_span(int y, int x0, int x1, int c){
    if(y<0||y>=160) return; if(x0<0) x0=0; if(x1>239) x1=239; if(x0>x1) return;
    if(y<m7_strip || (m7_cock && y>=140)){ volatile u16 *row=M7_PG0+y*120;                                    // the sky: pairs at a time
        if(x0&1){ volatile u16 *p=row+(x0>>1); *p=(*p&0x00FF)|(c<<8); x0++; }
        if(x1>=x0 && !(x1&1)){ volatile u16 *p=row+(x1>>1); *p=(*p&0xFF00)|c; x1--; }
        u16 cc=(u16)(c|(c<<8)); for(int k=x0>>1, e=x1>>1; k<=e && x0<=x1; k++) row[k]=cc; return; }
    KCNT(56,1); if(!(m7_dc[m7_tab][y]&(1<<4))) return; const M7Line *L=&M7_TAB(m7_tab)[y];
    s32 pa=L->pa, pc=L->pc, u=L->x+pa*x0, v=L->y+pc*x0; int lu=-1, lv=-1;                                       // the ground: each texel once
    { int n=x1-x0; s32 du=pa*n, dv=pc*n; int adu=du<0?-du:du, adv=dv<0?-dv:dv, mx=adu>adv?adu:adv;
#ifndef TEST_NODDA
      if(((mx>>8)+1)*5 < (n+1)*2){ m7_gspan(u,v,u+du,v+dv,c); return; }
#endif
    }                                      // (near the camera a texel is many pixels wide: walk the texels, not the pixels)
    KCNT(58,x1-x0+1); for(int x=x0;x<=x1;x++, u+=pa, v+=pc){ int uu=u>>8, vv=v>>8; if(uu==lu && vv==lv) continue; lu=uu; lv=vv; if((unsigned)uu<240 && (unsigned)vv<160) m7_wr8(M7_PG1,uu,vv,c); }
}
static void m7_begin(void){ ov_k(); m7_dc=(M7Dc*)(void*)gen_acc; m7_rowz=(s16*)(void*)gen_acc+320; m7_fog=(M7Fog*)(void*)((s16*)(void*)gen_acc+480); m7_on=1; m7_arm=0; m7_tab=0; m7_strip=160; page_back=0; REG_DISPCNT=4|(1<<10)|(1<<12)|(1<<6); }
static void m7_end(void){ m7_on=0; m7_arm=0; m7_strip=160; page_back=0; }
IWRAM static void m7_irq(void){                 // at every VBlank: start the line DMAs for this frame, or stop them when a menu is up or the race is over
    if(m7_arm && !(in_menu&0xFFFF)){ const M7Line *t=M7_TAB(m7_tab); const u16 *d=m7_dc[m7_tab];
        *(volatile u16*)0x04000020=t[0].pa; *(volatile u16*)0x04000022=0; *(volatile u16*)0x04000024=t[0].pc; *(volatile u16*)0x04000026=t[0].pd;
        *(volatile u32*)0x04000028=t[0].x; *(volatile u32*)0x0400002C=t[0].y; REG_DISPCNT=d[0];
        *(volatile u32*)0x040000C4=0; *(volatile u32*)0x040000D0=0; *(volatile u32*)0x040000B8=0;              // (restart them all from the tables' tops)
        if(((*(volatile u16*)0x04000050>>6)&3)!=3){ const u16 *f=m7_fog[m7_tab];                               // the haze (not while the screen is fading)
            *(volatile u16*)0x04000050=M7_BLD; *(volatile u16*)0x04000052=f[0];
            *(volatile u32*)0x040000B0=(u32)&f[1]; *(volatile u32*)0x040000B4=0x04000052;
            *(volatile u32*)0x040000B8=1u | ((u32)((3<<5)|(1<<9)|(2<<12)|(1<<15))<<16); }                     // DMA0: one halfword a line
        *(volatile u32*)0x040000BC=(u32)&d[1]; *(volatile u32*)0x040000C0=0x04000000;
        *(volatile u32*)0x040000C4=1u | ((u32)((3<<5)|(1<<9)|(2<<12)|(1<<15))<<16);                           // DMA1: the page, one halfword a line
        *(volatile u32*)0x040000C8=(u32)&t[1]; *(volatile u32*)0x040000CC=0x04000020;
        *(volatile u32*)0x040000D0=4u | ((u32)((3<<5)|(1<<9)|(1<<10)|(2<<12)|(1<<15))<<16);                   // DMA2: the affine registers, four words a line, at every HBlank
        m7_armed=1; }
    else if(m7_armed){ *(volatile u32*)0x040000C4=0; *(volatile u32*)0x040000D0=0; *(volatile u32*)0x040000B8=0; REG_DISPCNT=4|(1<<10)|(1<<12)|(1<<6);
        if(*(volatile u16*)0x04000050==M7_BLD) *(volatile u16*)0x04000050=0;
        *(volatile u16*)0x04000020=256; *(volatile u16*)0x04000022=0; *(volatile u16*)0x04000024=0; *(volatile u16*)0x04000026=256; *(volatile u32*)0x04000028=0; *(volatile u32*)0x0400002C=0; m7_armed=0; }
}
// ---- the frame: the profile under the centre line, then the line table (the hidden one)
static void m7_frame(void){
    int T=m7_tab^1; M7Line *tab=M7_TAB(T); u16 *dc=m7_dc[T]; u16 *fog=m7_fog[T];
    m7_cock = cam_fp && !kloop_on;
    s32 cb=cam_back(); s32 cam_x=ppx-((hsn*cb)>>8), cam_y=ppy-((hcs*cb)>>8);
    int ck=curv_k(), csh=curv_sh(); s32 csub=tiny_sub(); int fk=fov_k(); s32 fkq=(fk<<12)/15360;
    // the next window: 240x160 texels, centred 60 texels ahead of the camera (the far ground's corners still fit)
    { int tx=(cam_x>>15)&HMASK, ty=(cam_y>>15)&HMASK; int wx=(tx+((hsn*60)>>8)-120)&HMASK&~3, wy=(ty+((hcs*60)>>8)-80)&HMASK;
      int dx=((wx-m7_wx0+128)&HMASK)-128, dy=((wy-m7_wy0+128)&HMASK)-128; if(dx<0) dx=-dx; if(dy<0) dy=-dy;
      if(dx>4 || dy>4 || !m7_arm){ m7_nwx0=wx; m7_nwy0=wy; } else { m7_nwx0=m7_wx0; m7_nwy0=m7_wy0; } }                      // (the map is copied again only when it has to be)
    // the profile: march out along the centre line; each row gets the depth where the ground first covers it
    for(int y=0;y<160;y++) m7_rowz[y]=0;
    int ytop=160; s32 z=stp_z[0], zl=z, zmax=(64<<8);
    for(int i=0;i<96;i++){
        s32 mx=cam_x+hsn*z, my=cam_y+hcs*z; int ti=((my>>15)&HMASK)*HW+((mx>>15)&HMASK); s32 hh=hfine[ti]*g_hs;
        s32 curve=((((z*z)>>14)*ck)>>csh)-csub; s32 f=(FOCP<<16)/z; int y=PHZ+(((cam_h+curve-hh)*f)>>16);
        if(i==0){ int y0 = y>160 ? 160 : y<0 ? 0 : y; s32 dh=cam_h-hh; if(dh<16) dh=16;                 // the ground nearer than the first step: flat, under the camera
            for(int r=y0;r<160;r++){ s32 zz = r>PHZ ? (dh*FOCP)/(r-PHZ) : z; if(zz>z) zz=z; if(zz<16) zz=16; m7_rowz[r]=(s16)zz; }
            ytop=y0; zl=z; }
        else if(y<ytop){ if(y<0) y=0; int n=ytop-y; for(int r=y;r<ytop;r++) m7_rowz[r]=(s16)(z+(zl-z)*(r-y)/n); ytop=y; zl=z; }
        if(ytop<=0) break;
        z += 20+((i*i)>>4); if(z>zmax) break;
    }
    m7_horizon=ytop;
    int strip = ytop>M7_HUD ? ytop : M7_HUD; if(strip>160) strip=160; m7_nstrip=strip;
    for(int b=0;b<8;b++){ s32 Z=stp_z[8*b+7<NSTEP?8*b+7:NSTEP-1]; int yy=160; for(int r=0;r<160;r++) if(m7_rowz[r]>0 && m7_rowz[r]<=Z){ yy=r; break; }   // (sprites behind hills: the same profile for every column)
        for(int c=0;c<120;c++) ybin[c*8+b]=(u8)yy; }
    u16 base=4|(1<<10)|(1<<12)|(1<<6);
    for(int y=0;y<160;y++){ M7Line *L=&tab[y];
        int ground = y>=strip && m7_rowz[y]>0 && !(m7_cock && y>=140);
        if(!ground) fog[y]=16;
        else { s32 z8=m7_rowz[y]; int f = z8<(9<<8) ? 0 : (int)((z8-(9<<8))>>9); if(f>13) f=13; fog[y]=(u16)((16-f)|(f<<8)); }   // the haze: none up close, thickest at the horizon
        if(!ground){ dc[y]=base; L->pa=256; L->pb=0; L->pc=0; L->pd=256; L->x=0; L->y=y<<8; }
        else { s32 z8=m7_rowz[y], zt8=z8*2; s32 pa=((((hcs*zt8)>>8)*fkq)>>12), pc=-(((((hsn*zt8)>>8)*fkq)>>12));
            s32 cx8=(cam_x>>7)+((hsn*z8)>>7), cy8=(cam_y>>7)+((hcs*z8)>>7);
            s32 X=((cx8-(m7_nwx0<<8))&((HW<<8)-1))-pa*120, Y=((cy8-(m7_nwy0<<8))&((HW<<8)-1))-pc*120;
            dc[y]=base|(1<<4); L->pa=(s16)pa; L->pb=0; L->pc=(s16)pc; L->pd=0; L->x=X; L->y=Y; } }
    BG_PAL[0]=bg_pal[SKY_B+7];
}
static void m7_present(void){                   // the end of the frame's drawing: this table goes live at the next VBlank; remember the camera it was drawn with
    m7_tab^=1; m7_strip=m7_nstrip; m7_copy = (m7_nwx0!=m7_wx0 || m7_nwy0!=m7_wy0 || !m7_arm); m7_wx0=m7_nwx0; m7_wy0=m7_nwy0; m7_arm=1;
    m7_sppx=ppx; m7_sppy=ppy; m7_scamh=cam_h; m7_shsn=hsn; m7_shcs=hcs; m7_sphz=g_phz; m7_sloop=kloop_on; m7_shide=kloop_hide; m7_shead=heading; m7_fovs=fov_k(); m7_sfp=cam_fp; m7_cb_snap=cam_back(); m7_want=1; }
static void kproj_prep(void); static int kpc_on; static void kloop_draw(volatile u16 *pg, int cy0, int cy1); static void kbridge_draw(volatile u16 *pg, int cy0, int cy1); static void kart_fx(volatile u16 *pg, int sy); static void weather_draw(volatile u16 *pg); static void kart_hud(volatile u16 *pg); static int kproj(s32 wx, s32 wy, s32 wh, int *sx, int *sy, s32 *zc);
static const u8 khill_t[1024]={181,181,181,181,181,181,181,181,181,181,181,181,181,181,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,182,181,181,181,181,181,181,181,181,181,181,182,182,182,182,182,182,182,183,183,183,183,184,184,184,184,185,185,185,185,185,185,186,186,186,186,186,186,185,185,185,185,185,185,184,184,184,168,167,167,167,167,166,166,166,166,165,165,165,165,149,148,148,148,148,148,148,148,148,148,148,148,147,147,147,147,147,147,147,147,146,146,146,146,146,161,161,161,161,161,160,160,160,160,160,176,176,176,176,176,176,176,176,192,192,192,192,192,193,193,193,193,194,194,194,194,195,195,195,195,195,196,196,196,196,196,196,196,196,196,196,196,180,180,180,180,180,180,180,164,164,163,163,163,147,147,147,147,147,147,131,131,131,132,132,132,116,116,116,116,116,116,116,116,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,84,84,83,83,83,83,84,84,84,84,84,84,84,85,85,85,85,86,86,86,87,87,87,88,88,72,72,73,73,73,73,73,74,74,74,74,74,74,74,74,74,74,73,73,73,73,73,73,73,72,72,72,72,72,72,72,72,72,88,87,87,87,87,87,87,103,103,103,103,103,103,119,118,118,118,118,118,134,133,133,133,133,132,148,148,147,147,147,146,146,146,146,161,161,161,161,161,161,161,161,161,161,161,161,161,161,161,161,161,146,146,146,146,146,147,147,147,147,147,147,131,131,132,132,132,132,132,132,132,131,131,131,131,131,115,115,115,115,115,115,115,115,115,114,130,130,131,131,131,131,131,131,131,131,131,131,131,131,131,147,147,147,147,147,147,147,147,147,147,147,147,163,162,162,162,162,162,162,162,161,161,161,161,161,161,161,161,161,161,161,162,162,162,162,163,163,163,163,164,180,180,181,181,181,182,182,182,183,183,183,183,183,184,184,184,184,200,200,200,200,200,200,200,200,200,200,216,216,216,216,216,216,216,216,216,217,217,233,233,233,233,233,233,233,233,233,233,233,233,233,233,233,233,233,217,216,216,216,216,216,215,215,199,198,198,198,197,197,181,180,180,180,164,163,163,163,163,147,147,147,147,147,131,131,131,131,131,131,115,115,116,116,116,116,116,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,99,100,100,100,100,100,100,100,100,100,100,99,99,99,99,99,99,99,98,98,98,98,97,97,97,97,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,97,97,97,113,114,114,114,114,115,115,115,115,116,116,116,116,132,133,133,133,133,133,133,133,133,133,133,133,134,134,134,134,134,134,134,134,135,135,135,135,135,120,120,120,120,120,121,121,121,121,121,105,105,106,106,106,106,106,105,89,89,89,89,89,88,88,88,88,87,87,87,87,86,86,86,86,86,85,85,85,85,85,85,85,85,85,85,85,101,101,101,101,101,101,101,117,117,118,118,118,134,134,134,134,134,134,150,150,150,149,149,149,165,165,165,165,165,165,165,165,181,181,181,181,181,181,181,181,181,181,181,181,181,181,181,181,181,197,197,198,198,198,198,197,197,197,197,197,197,197,196,196,196,196,195,195,195,194,194,194,193,193,209,209,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,208,209,209,209,209,209,209,209,209,209,193,194,194,194,194,194,194,178,178,178,178,178,178,162,163,163,163,163,163,147,148,148,148,148,149,133,133,134,134,134,135,135,135,135,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,135,135,135,135,135,134,134,134,134,134,134,150,150,149,149,149,149,149,149,149,150,150,150,150,150,166,166,166,166,166,166,166,166,166,167,151,151,150,150,150,150,150,150,150,150,150,150,150,150,150,134,134,134,134,134,134,134,134,134,134,134,134,118,119,119,119,119,119,119,119,120,120,120,120,120,120,120,120,120,120,120,119,119,119,119,118,118,118,118,117,101,101,100,100,100,99,99,99,98,98,98,98,98,97,97,97,97,81,81,81,81,81,81,81,81,81,81,65,65,65,65,65,65,65,65,65,64,64,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,48,64,65,65,65,65,65,66,66,82,83,83,83,84,84,100,101,101,101,117,118,118,118,118,134,134,134,134,134,150,150,150,150,150,150,166,166,165,165,165,165,165,181,181,181,181,181};   // sky_hills: the two hill heights for each bearing (hf<<4 | hn), from the old per-column sums of sines
static void sky_hills(volatile u16 *pg){                                          // two ranges of hills on the horizon that slide past as you turn: the sense of turning, for free
    int hor=m7_horizon; if(hor<24 || hor>160 || WL(MW_VOID)) return;
    BG_PAL[108]=lerp15(BG_PAL[SKY_B+7],BG_PAL[GND_B+8],8); BG_PAL[109]=lerp15(BG_PAL[SKY_B+7],BG_PAL[GND_B+3],14);
    int fv=fov_k(), b0=m7_shead;
    int base=((-118)*fv);
    for(int c=0;c<120;c+=2, base+=4*fv){ int i=(b0+(((base)*349)>>16))&1023;                          // the compass bearing of this pair of columns
        int hh=khill_t[i], hf=hh>>4, hn=hh&15;
        for(int k=1;k<=hf;k++){ int y=hor-k; if(y>=19) *(volatile u32*)(pg+y*120+c)=0x6C6C6C6Cu; }
        for(int k=1;k<=hn;k++){ int y=hor-k; if(y>=19) *(volatile u32*)(pg+y*120+c)=0x6D6D6D6Du; } } }
static void m7_sky(void){                     // the sky, the loops, the dust, the messages and the HUD, on page 0 (ground pixels find their way onto the map)
    ov_k();
    volatile u16 *pg=M7_PG0;
    s32 kpx=ppx, kpy=ppy, kch=cam_h; int khs=hsn, khc=hcs, kph=g_phz, klo=kloop_on, kfp=cam_fp, khd_=kloop_hide;                 // the loops are drawn where the frame on show has its camera
    ppx=m7_sppx; ppy=m7_sppy; cam_h=m7_scamh; hsn=m7_shsn; hcs=m7_shcs; g_phz=m7_sphz; kloop_on=m7_sloop; kloop_hide=m7_shide; cam_fp=m7_sfp; m7_snap=1;
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 ta=TMR();
#endif
    for(int y=18;y<m7_strip;y++){ int ix=skyrow[y]; static u32 sv; sv=ix*0x01010101u; *(volatile u32*)0x040000D4=(u32)&sv; *(volatile u32*)0x040000D8=(u32)(pg+y*120); *(volatile u32*)0x040000DC=60|(2<<23)|(1<<26)|(1u<<31); }
    for(int k=0;k<22;k++){ int x=(k*53+17)%120, y=19+((k*31+5)%50); if(y<m7_horizon){ int ix=((frames>>3)+k)&7 ? 4 : 5; pg[y*120+x]=ix|(ix<<8); } }
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 tb=TMR();
#endif
    kart_hud(pg);                                                                   // (the HUD first: it must be done before the beam is back at the top)
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 t1=TMR();
#endif
    if(kr_mode==1) sky_hills(pg);
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 t2=TMR();
#endif
    if(kr_mode==1) kloop_draw(pg,18,160);
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 t3=TMR();
#endif
#ifdef TEST_FPS
    dbg_pc=0; dbg_rows=0; dbg_px=0; dbg_pt=0; dbg_kt=0; dbg_kn=0;
#endif
    if(kr_mode==1) kbridge_draw(pg,18,160);
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 tc=TMR();
#endif
#ifdef TEST_FPS
    u32 qpc=dbg_pc, qrows=dbg_rows, qpx=dbg_px, qpt=dbg_pt, qkt=dbg_kt, qkn=dbg_kn;
#endif
    if(!m7_cock && !kloop_on) kart_fx(pg,m7_sy);
    weather_draw(pg); if(wd.weird) sky_decor(pg); if(hellw()) hell_sky(pg);
    if(msg_timer && !FILM){ int L=textlen(msg); stext(pg,120-L*2,50,msg,C_WHITE); }
#if defined(TEST_FPS) || defined(TEST_KLOG)
    u32 td=TMR();
#endif
#ifdef TEST_KLOG
    if(kr_mode==1){ extern u32 klog[]; klog[16]+=tb-ta; klog[17]+=t1-tb; klog[18]+=t2-t1; klog[19]+=t3-t2; klog[20]+=tc-t3; klog[21]+=td-tc; klog[22]++; klog[32]+=kt_v[0]; klog[33]+=kt_v[1]; klog[34]+=kt_v[2]; klog[35]+=tc-kt_t0; }
#endif
#ifdef TEST_FPS
    { char fb[80]; scpy(fb,"LOGIC "); ncat(fb,(int)(m7_t[0]/1000)); scat(fb,"K TAB "); ncat(fb,(int)(m7_t[1]/1000)); scat(fb,"K SPR "); ncat(fb,(int)(m7_t[2]/1000)); scat(fb,"K COPY "); ncat(fb,(int)(m7_t[3]/1000)); scat(fb,"K WAIT "); ncat(fb,(int)(m7_t[4]/1000)); scat(fb,"K SKY "); ncat(fb,(int)(m7_t[5]/1000)); scat(fb,"K"); rect(pg,0,19,240,8,0); text(pg,2,20,fb,C_GOLD,1);
      scpy(fb,"FILL "); ncat(fb,(int)((tb-ta)/1000)); scat(fb," HUD "); ncat(fb,(int)((t1-tb)/1000)); scat(fb," HILL "); ncat(fb,(int)((t2-t1)/1000)); scat(fb," LOOP "); ncat(fb,(int)((t3-t2)/1000)); scat(fb," BR "); ncat(fb,(int)((tc-t3)/1000)); scat(fb,"K FX "); ncat(fb,(int)((td-tc)/1000)); scat(fb,"K"); rect(pg,0,27,240,8,0); text(pg,2,28,fb,C_GOLD,1);
      { static u32 sm[4]={0,0,0,0}, ss[4]={0,0,0,0}, sn=0, sa[4]={0,0,0,0}, sx[4]={0,0,0,0}; u32 v[4]={kt_v[0],kt_v[1],kt_v[2],tc-kt_t0}; for(int q=0;q<4;q++){ ss[q]+=v[q]; if(v[q]>sm[q]) sm[q]=v[q]; }
        if(++sn==60){ for(int q=0;q<4;q++){ sa[q]=ss[q]/60; sx[q]=sm[q]; ss[q]=0; sm[q]=0; } sn=0; }
        scpy(fb,"SIGN "); ncat(fb,(int)(sa[0]/1000)); scat(fb,"/"); ncat(fb,(int)(sx[0]/1000)); scat(fb," STAR "); ncat(fb,(int)(sa[1]/1000)); scat(fb,"/"); ncat(fb,(int)(sx[1]/1000)); scat(fb," GANT "); ncat(fb,(int)(sa[2]/1000)); scat(fb,"/"); ncat(fb,(int)(sx[2]/1000)); scat(fb," DECK "); ncat(fb,(int)(sa[3]/1000)); scat(fb,"/"); ncat(fb,(int)(sx[3]/1000));
        { int qa,qb; s32 qz;
          rect(pg,0,35,240,8,0); text(pg,2,36,fb,C_GOLD,1); scpy(fb,"P "); ncat(fb,(int)qpc); scat(fb," R "); ncat(fb,(int)qrows); scat(fb," X "); ncat(fb,(int)qpx); scat(fb," PT "); ncat(fb,(int)(qpt/1000)); scat(fb," KT "); ncat(fb,(int)(qkt/1000)); scat(fb," KN "); ncat(fb,(int)qkn);  rect(pg,0,43,240,8,0); text(pg,2,44,fb,C_GOLD,1); scpy(fb,"FRAME "); ncat(fb,(int)(kfr_avg/1000)); scat(fb,"K FPS "); ncat(fb,(int)(16780000u/(kfr_avg?kfr_avg:1))); rect(pg,0,51,240,8,0); text(pg,2,52,fb,C_GOLD,1); scpy(fb,"HUD PLACE "); ncat(fb,(int)(kh_t[0]/1000)); scat(fb," LAP "); ncat(fb,(int)(kh_t[1]/1000)); scat(fb," ITEM "); ncat(fb,(int)(kh_t[2]/1000)); scat(fb," STAR "); ncat(fb,(int)(kh_t[3]/1000)); scat(fb," MAP "); ncat(fb,(int)(kh_t[4]/1000)); scat(fb," BAR "); ncat(fb,(int)(kh_t[5]/1000)); rect(pg,0,59,240,8,0); text(pg,2,60,fb,C_GOLD,1); } } }
#endif
    ppx=kpx; ppy=kpy; cam_h=kch; hsn=khs; hcs=khc; g_phz=kph; kloop_on=klo; kloop_hide=khd_; cam_fp=kfp; m7_snap=0;                                   // (and back to the frame being built)
}
static void m7_after_vsync(void){               // in the blank: the map window into page 1, when the window moved
    if(!m7_arm || !m7_copy) return;
    u32 tq0=TMR();
    for(int r=0;r<160;r++){ int my=(m7_wy0+r)&HMASK; const u8 *src=&cmap[my*HW]; volatile u16 *dst=M7_PG1+r*120; int n1=HW-m7_wx0;
        if(n1>=240){ *(volatile u32*)0x040000D4=(u32)(src+m7_wx0); *(volatile u32*)0x040000D8=(u32)dst; *(volatile u32*)0x040000DC=60|(1<<26)|(1u<<31); }
        else { *(volatile u32*)0x040000D4=(u32)(src+m7_wx0); *(volatile u32*)0x040000D8=(u32)dst; *(volatile u32*)0x040000DC=(n1>>2)|(1<<26)|(1u<<31);
               *(volatile u32*)0x040000D4=(u32)src; *(volatile u32*)0x040000D8=(u32)(dst+(n1>>1)); *(volatile u32*)0x040000DC=((240-n1)>>2)|(1<<26)|(1u<<31); } }
    m7_copy=0;
#ifdef TEST_FPS
    m7_t[3]=TMR()-tq0;
#else
    (void)tq0;
#endif
}
static void m7_sky_now(void){                   // once the beam is past the sky: draw it (the frame on show's sky, loops and HUD)
    if(!m7_want) return; m7_want=0;
    u32 tq1=TMR();
    while(REG_VCOUNT<m7_strip || REG_VCOUNT>=160);
    u32 tq2=TMR();
    m7_sky();
#if defined(TEST_FPS) || defined(TEST_KLOG)
    m7_t[4]=tq2-tq1; m7_t[5]=TMR()-tq2;
#else
    (void)tq1; (void)tq2;
#endif
}
