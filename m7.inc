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
#define M7_PG0 ((volatile u16*)0x06000000)
#define M7_PG1 ((volatile u16*)0x0600A000)
#define M7_HUD 64                               // the HUD lives in the first 64 rows: they always come from page 0
static int m7_tab=0, m7_copy=1;                 // the table on show (the other one is being built); the map must be copied again
static volatile int m7_arm=0, m7_armed=0;       // arm: a table is ready. armed: the DMA is running
static int m7_wx0=0, m7_wy0=0, m7_nwx0=0, m7_nwy0=0;   // the map window in page 1 (its top-left texel), and the next one
static int m7_horizon=160, m7_nstrip=160, m7_cock=0;   // where the ground stops, the strip being built, cockpit view
static int m7_sy=0;                             // the kart's screen row (for its dust and sparks)
static s32 m7_sppx, m7_sppy, m7_scamh; static int m7_shsn, m7_shcs, m7_sphz, m7_sloop, m7_sfp, m7_want=0;   // a snapshot of the camera the frame on show was drawn with
#ifdef TEST_FPS
static u32 m7_t[6];
#endif
static inline void m7_wr8(volatile u16 *pg, int u, int v, int c){ volatile u16 *p=pg+((v*240+u)>>1); if(u&1) *p=(*p&0x00FF)|(c<<8); else *p=(*p&0xFF00)|c; }
static void m7_pix(int x, int y, int c){         // a pixel of the screen: into the sky half, or onto the map texel the ground shows there
    if(x<0||x>=240||y<0||y>=160) return;
    if(y<m7_strip || (m7_cock && y>=140)){ m7_wr8(M7_PG0,x,y,c); return; }
    if(!(m7_dc[m7_tab][y]&(1<<4))) return; const M7Line *L=&M7_TAB(m7_tab)[y];
    int u=(L->x+L->pa*x)>>8, v=(L->y+L->pc*x)>>8; if((unsigned)u<240 && (unsigned)v<160) m7_wr8(M7_PG1,u,v,c);
}
static void m7_span(int y, int x0, int x1, int c){
    if(y<0||y>=160) return; if(x0<0) x0=0; if(x1>239) x1=239; if(x0>x1) return;
    if(y<m7_strip || (m7_cock && y>=140)){ volatile u16 *row=M7_PG0+y*120;                                    // the sky: pairs at a time
        if(x0&1){ volatile u16 *p=row+(x0>>1); *p=(*p&0x00FF)|(c<<8); x0++; }
        if(x1>=x0 && !(x1&1)){ volatile u16 *p=row+(x1>>1); *p=(*p&0xFF00)|c; x1--; }
        u16 cc=(u16)(c|(c<<8)); for(int k=x0>>1, e=x1>>1; k<=e && x0<=x1; k++) row[k]=cc; return; }
    if(!(m7_dc[m7_tab][y]&(1<<4))) return; const M7Line *L=&M7_TAB(m7_tab)[y];
    s32 u=L->x+L->pa*x0, v=L->y+L->pc*x0, pa=L->pa, pc=L->pc; int lu=-1, lv=-1;                               // the ground: each texel once
    for(int x=x0;x<=x1;x++, u+=pa, v+=pc){ int uu=u>>8, vv=v>>8; if(uu==lu && vv==lv) continue; lu=uu; lv=vv; if((unsigned)uu<240 && (unsigned)vv<160) m7_wr8(M7_PG1,uu,vv,c); }
}
static void m7_begin(void){ m7_dc=(M7Dc*)(void*)gen_acc; m7_rowz=(s16*)(void*)gen_acc+320; m7_on=1; m7_arm=0; m7_tab=0; m7_strip=160; page_back=0; REG_DISPCNT=4|(1<<10)|(1<<12)|(1<<6); }
static void m7_end(void){ m7_on=0; m7_arm=0; m7_strip=160; page_back=0; }
IWRAM static void m7_irq(void){                 // at every VBlank: start the line DMAs for this frame, or stop them when a menu is up or the race is over
    if(m7_arm && !in_menu){ const M7Line *t=M7_TAB(m7_tab); const u16 *d=m7_dc[m7_tab];
        *(volatile u16*)0x04000020=t[0].pa; *(volatile u16*)0x04000022=0; *(volatile u16*)0x04000024=t[0].pc; *(volatile u16*)0x04000026=t[0].pd;
        *(volatile u32*)0x04000028=t[0].x; *(volatile u32*)0x0400002C=t[0].y; REG_DISPCNT=d[0];
        *(volatile u32*)0x040000C4=0; *(volatile u32*)0x040000D0=0;                                           // (restart both from the tables' tops)
        *(volatile u32*)0x040000BC=(u32)&d[1]; *(volatile u32*)0x040000C0=0x04000000;
        *(volatile u32*)0x040000C4=1u | ((u32)((3<<5)|(1<<9)|(2<<12)|(1<<15))<<16);                           // DMA1: the page, one halfword a line
        *(volatile u32*)0x040000C8=(u32)&t[1]; *(volatile u32*)0x040000CC=0x04000020;
        *(volatile u32*)0x040000D0=4u | ((u32)((3<<5)|(1<<9)|(1<<10)|(2<<12)|(1<<15))<<16);                   // DMA2: the affine registers, four words a line, at every HBlank
        m7_armed=1; }
    else if(m7_armed){ *(volatile u32*)0x040000C4=0; *(volatile u32*)0x040000D0=0; REG_DISPCNT=4|(1<<10)|(1<<12)|(1<<6);
        *(volatile u16*)0x04000020=256; *(volatile u16*)0x04000022=0; *(volatile u16*)0x04000024=0; *(volatile u16*)0x04000026=256; *(volatile u32*)0x04000028=0; *(volatile u32*)0x0400002C=0; m7_armed=0; }
}
// ---- the frame: the profile under the centre line, then the line table (the hidden one)
static void m7_frame(void){
    int T=m7_tab^1; M7Line *tab=M7_TAB(T); u16 *dc=m7_dc[T];
    m7_cock = cam_fp && !kloop_on;
    s32 cb=cam_back(); s32 cam_x=ppx-((hsn*cb)>>8), cam_y=ppy-((hcs*cb)>>8);
    int ck=curv_k(); s32 csub=tiny_sub(); int fk=fov_k(); s32 fkq=(fk<<12)/15360;
    // the next window: 240x160 texels, centred 52 texels ahead of the camera (the far ground's corners still fit)
    { int tx=(cam_x>>15)&HMASK, ty=(cam_y>>15)&HMASK; int wx=(tx+((hsn*52)>>8)-120)&HMASK&~3, wy=(ty+((hcs*52)>>8)-80)&HMASK;
      int dx=((wx-m7_wx0+128)&HMASK)-128, dy=((wy-m7_wy0+128)&HMASK)-128; if(dx<0) dx=-dx; if(dy<0) dy=-dy;
      if(dx>4 || dy>4 || !m7_arm){ m7_nwx0=wx; m7_nwy0=wy; } else { m7_nwx0=m7_wx0; m7_nwy0=m7_wy0; } }                      // (the map is copied again only when it has to be)
    // the profile: march out along the centre line; each row gets the depth where the ground first covers it
    for(int y=0;y<160;y++) m7_rowz[y]=0;
    int ytop=160; s32 z=stp_z[0], zl=z, zmax=(46<<8);
    for(int i=0;i<96;i++){
        s32 mx=cam_x+hsn*z, my=cam_y+hcs*z; int ti=((my>>15)&HMASK)*HW+((mx>>15)&HMASK); s32 hh=hfine[ti]*g_hs;
        s32 curve=((((z*z)>>14)*ck)>>3)-csub; s32 f=(FOCP<<16)/z; int y=PHZ+(((cam_h+curve-hh)*f)>>16);
        if(i==0){ int y0 = y>160 ? 160 : y<0 ? 0 : y; s32 dh=cam_h-hh; if(dh<16) dh=16;                 // the ground nearer than the first step: flat, under the camera
            for(int r=y0;r<160;r++){ s32 zz = r>PHZ ? (dh*FOCP)/(r-PHZ) : z; if(zz>z) zz=z; if(zz<16) zz=16; m7_rowz[r]=(s16)zz; }
            ytop=y0; zl=z; }
        else if(y<ytop){ if(y<0) y=0; int n=ytop-y; for(int r=y;r<ytop;r++) m7_rowz[r]=(s16)(z+(zl-z)*(r-y)/n); ytop=y; zl=z; }
        if(ytop<=0) break;
        z += 20+((i*i)>>5); if(z>zmax) break;
    }
    m7_horizon=ytop;
    int strip = ytop>M7_HUD ? ytop : M7_HUD; if(strip>160) strip=160; m7_nstrip=strip;
    for(int b=0;b<8;b++){ s32 Z=stp_z[8*b+7]; int yy=160; for(int r=0;r<160;r++) if(m7_rowz[r]>0 && m7_rowz[r]<=Z){ yy=r; break; }   // (sprites behind hills: the same profile for every column)
        for(int c=0;c<120;c++) ybin[c*8+b]=(u8)yy; }
    u16 base=4|(1<<10)|(1<<12)|(1<<6);
    for(int y=0;y<160;y++){ M7Line *L=&tab[y];
        int ground = y>=strip && m7_rowz[y]>0 && !(m7_cock && y>=140);
        if(!ground){ dc[y]=base; L->pa=256; L->pb=0; L->pc=0; L->pd=256; L->x=0; L->y=y<<8; }
        else { s32 z8=m7_rowz[y], zt8=z8*2; s32 pa=((((hcs*zt8)>>8)*fkq)>>12), pc=-(((((hsn*zt8)>>8)*fkq)>>12));
            s32 cx8=(cam_x>>7)+((hsn*z8)>>7), cy8=(cam_y>>7)+((hcs*z8)>>7);
            s32 X=((cx8-(m7_nwx0<<8))&((HW<<8)-1))-pa*120, Y=((cy8-(m7_nwy0<<8))&((HW<<8)-1))-pc*120;
            dc[y]=base|(1<<4); L->pa=(s16)pa; L->pb=0; L->pc=(s16)pc; L->pd=0; L->x=X; L->y=Y; } }
    BG_PAL[0]=bg_pal[SKY_B+7];
}
static void m7_present(void){                   // the end of the frame's drawing: this table goes live at the next VBlank; remember the camera it was drawn with
    m7_tab^=1; m7_strip=m7_nstrip; m7_copy = (m7_nwx0!=m7_wx0 || m7_nwy0!=m7_wy0 || !m7_arm); m7_wx0=m7_nwx0; m7_wy0=m7_nwy0; m7_arm=1;
    m7_sppx=ppx; m7_sppy=ppy; m7_scamh=cam_h; m7_shsn=hsn; m7_shcs=hcs; m7_sphz=g_phz; m7_sloop=kloop_on; m7_sfp=cam_fp; m7_cb_snap=cam_back(); m7_want=1; }
static void kloop_draw(volatile u16 *pg, int cy0, int cy1); static void kart_fx(volatile u16 *pg, int sy); static void weather_draw(volatile u16 *pg); static void kart_hud(volatile u16 *pg);
static void m7_sky(void){                     // the sky, the loops, the dust, the messages and the HUD, on page 0 (ground pixels find their way onto the map)
    volatile u16 *pg=M7_PG0;
    s32 kpx=ppx, kpy=ppy, kch=cam_h; int khs=hsn, khc=hcs, kph=g_phz, klo=kloop_on, kfp=cam_fp;                 // the loops are drawn where the frame on show has its camera
    ppx=m7_sppx; ppy=m7_sppy; cam_h=m7_scamh; hsn=m7_shsn; hcs=m7_shcs; g_phz=m7_sphz; kloop_on=m7_sloop; cam_fp=m7_sfp; m7_snap=1;
#ifdef TEST_FPS
    u32 ta=TMR();
#endif
    for(int y=18;y<m7_strip;y++){ int ix=skyrow[y]; static u32 sv; sv=ix*0x01010101u; *(volatile u32*)0x040000D4=(u32)&sv; *(volatile u32*)0x040000D8=(u32)(pg+y*120); *(volatile u32*)0x040000DC=60|(2<<23)|(1<<26)|(1u<<31); }
    for(int k=0;k<22;k++){ int x=(k*53+17)%120, y=19+((k*31+5)%50); if(y<m7_horizon){ int ix=((frames>>3)+k)&7 ? 4 : 5; pg[y*120+x]=ix|(ix<<8); } }
#ifdef TEST_FPS
    u32 tb=TMR();
#endif
    kart_hud(pg);                                                                   // (the HUD first: it must be done before the beam is back at the top)
    if(kr_mode==1) kloop_draw(pg,18,160);
#ifdef TEST_FPS
    u32 tc=TMR();
#endif
    if(!m7_cock && !kloop_on) kart_fx(pg,m7_sy);
    weather_draw(pg); if(wd.weird) sky_decor(pg); if(hellw()) hell_sky(pg);
    if(msg_timer && !FILM){ int L=textlen(msg); stext(pg,120-L*2,50,msg,C_WHITE); }
#ifdef TEST_FPS
    u32 td=TMR();
#endif
#ifdef TEST_FPS
    { u32 te=TMR(); char fb[48]; scpy(fb,"LOGIC "); ncat(fb,(int)(m7_t[0]/1000)); scat(fb,"K TAB "); ncat(fb,(int)(m7_t[1]/1000)); scat(fb,"K SPR "); ncat(fb,(int)(m7_t[2]/1000)); scat(fb,"K COPY "); ncat(fb,(int)(m7_t[3]/1000)); scat(fb,"K WAIT "); ncat(fb,(int)(m7_t[4]/1000)); scat(fb,"K SKY "); ncat(fb,(int)(m7_t[5]/1000)); scat(fb,"K"); rect(pg,0,19,240,8,0); text(pg,2,20,fb,C_GOLD,1);
      scpy(fb,"FILL "); ncat(fb,(int)((tb-ta)/1000)); scat(fb,"K LOOP "); ncat(fb,(int)((tc-tb)/1000)); scat(fb,"K FX "); ncat(fb,(int)((td-tc)/1000)); scat(fb,"K HUD "); ncat(fb,(int)((te-td)/1000)); scat(fb,"K"); rect(pg,0,27,240,8,0); text(pg,2,28,fb,C_GOLD,1); }
#endif
    ppx=kpx; ppy=kpy; cam_h=kch; hsn=khs; hcs=khc; g_phz=kph; kloop_on=klo; cam_fp=kfp; m7_snap=0;                                   // (and back to the frame being built)
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
#ifdef TEST_FPS
    m7_t[4]=tq2-tq1; m7_t[5]=TMR()-tq2;
#else
    (void)tq1; (void)tq2;
#endif
}
