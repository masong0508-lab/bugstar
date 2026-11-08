// paint.inc: lines, rows and filled polygons (ship.inc includes this file twice).
// Plain copy: ROM code for the ship drawings and the filming. PAINT_K copy (names start with k): ARM code in the kart overlay (OVK, main.c), the same
// text but with the kart race's hardware ground (m7_on: each pixel finds its texel on the map, m7.inc). Running from IWRAM it is twice as fast.
PAT static void PN(linep)(volatile u16 *pg, int x0, int y0, int x1, int y1, int c, int cx0, int cy0, int cx1, int cy1){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(n>300) return;
#ifdef PAINT_K
    if(m7_on){ s32 fx=(x0<<16)+0x8000, fy=(y0<<16)+0x8000, sx=n?(dx<<16)/n:0, sy=n?(dy<<16)/n:0;         // (a kart race: each pixel finds its own way to the screen)
        for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1) m7_pix(x,y,c); fx+=sx; fy+=sy; } return; }
#endif
    if(!n){ if(x0>=cx0&&x0<cx1&&y0>=cy0&&y0<cy1) pix(pg,x0,y0,c); return; }
    s32 sx, sy; if(n<256){ sx=dx*(s32)sh_rt[n]; sy=dy*(s32)sh_rt[n]; } else { sx=(dx<<16)/n; sy=(dy<<16)/n; }
    s32 fx=(x0<<16)+0x8000, fy=(y0<<16)+0x8000; u16 lo=(u16)c, hi=(u16)(c<<8);
    if(sh_occ){ for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1 && sh_occ[(x>>1)<<3]>y){ volatile u16 *p=pg+((y*240+x)>>1); if(x&1) *p=(*p&0x00FF)|hi; else *p=(*p&0xFF00)|lo; } fx+=sx; fy+=sy; } return; }   // (on a planet: not through a hill)
    if(x0>=cx0&&x0<cx1&&x1>=cx0&&x1<cx1&&y0>=cy0&&y0<cy1&&y1>=cy0&&y1<cy1){        // both ends inside: no clipping per pixel
        for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; volatile u16 *p=pg+((y*240+x)>>1); if(x&1) *p=(*p&0x00FF)|hi; else *p=(*p&0xFF00)|lo; fx+=sx; fy+=sy; }
        return; }
    for(int i=0;i<=n;i++){ int x=fx>>16, y=fy>>16; if(x>=cx0&&x<cx1&&y>=cy0&&y<cy1){ volatile u16 *p=pg+((y*240+x)>>1); if(x&1) *p=(*p&0x00FF)|hi; else *p=(*p&0xFF00)|lo; } fx+=sx; fy+=sy; }
}
PAT static void PN(hspan)(volatile u16 *pg, int y, int x0, int x1, int c){                 // a row of pixels, x0..x1 inclusive, written two at a time
#ifdef PAINT_K
    if(m7_on){ m7_span(y,x0,x1,c+(sh_df>>1)); return; }
#endif
    volatile u16 *row=pg+y*120; u16 cc;
    if(!sh_df) cc=(u16)(c|(c<<8)); else { int f=sh_df, t0=(y&1)?3:0, t1=(y&1)?1:2; cc=(u16)((c+(f>t0)) | ((c+(f>t1))<<8)); }
    if(x0&1){ volatile u16 *p=row+(x0>>1); *p=(*p&0x00FF)|(cc&0xFF00); x0++; }
    if(x1>=x0 && !(x1&1)){ volatile u16 *p=row+(x1>>1); *p=(*p&0xFF00)|(cc&0x00FF); x1--; }
    for(int k=x0>>1, e=x1>>1; k<=e && x0<=x1; k++) row[k]=cc;
}
PAT static void PN(polyfill_)(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1);
PAT static void PN(polyfill)(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1){
#ifdef TEST_FPS
    u32 t0=*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16); PN(polyfill_)(pg,px,py,n,c,cx0,cy0,cx1,cy1); dbg_pt+=(*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16))-t0;
}
PAT static void PN(polyfill_)(volatile u16 *pg, const int *px, const int *py, int n, int c, int cx0, int cy0, int cx1, int cy1){
#endif
    KCNT(14,1); int ymin=py[0], ymax=py[0]; for(int i=1;i<n;i++){ if(py[i]<ymin) ymin=py[i]; if(py[i]>ymax) ymax=py[i]; }
    if(ymin<cy0) ymin=cy0; if(ymax>=cy1) ymax=cy1-1; if(ymin>ymax) return;
    for(int y=ymin;y<=ymax;y++){ sh_l[y]=32767; sh_r[y]=-32768; }
    for(int i=0;i<n;i++){ int j=i+1; if(j==n) j=0; int x0=px[i], y0=py[i], x1=px[j], y1=py[j];      // walk each edge once, noting where it crosses every row
        if(y0==y1){ if(y0>=ymin && y0<=ymax){ int a=x0<x1?x0:x1, b=x0<x1?x1:x0; if(a<sh_l[y0]) sh_l[y0]=a; if(b>sh_r[y0]) sh_r[y0]=b; } continue; }
        if(y0>y1){ int t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
        int ys=y0<ymin?ymin:y0, ye=y1>ymax?ymax:y1; if(ys>ye) continue;
        s32 dy=y1-y0, dx=x1-x0, sl = dy<256 ? dx*(s32)sh_rt[dy] : (dx<<16)/dy;
        s32 x=(x0<<16)+0x8000+sl*(ys-y0);
        KCNT(29,ye-ys+1); for(int y=ys;y<=ye;y++){ int xi=x>>16; if(xi<sh_l[y]) sh_l[y]=xi; if(xi>sh_r[y]) sh_r[y]=xi; x+=sl; } }
#ifdef TEST_FPS
    dbg_pc++;
#endif
    for(int y=ymin;y<=ymax;y++){ int xl=sh_l[y], xr=sh_r[y]; if(xl>xr) continue; if(xl<cx0) xl=cx0; if(xr>=cx1) xr=cx1-1; if(xl>xr) continue; KCNT(15,1);
#ifdef TEST_FPS
        dbg_rows++; dbg_px+=xr-xl+1;
#endif
        if(sh_occ){ int x=xl;                                                       // on a planet: columns a nearer hill covers from this row down are skipped
            while(x<=xr){ while(x<=xr && sh_occ[(x>>1)<<3]<=y) x++; if(x>xr) break; int s0=x; while(x<=xr && sh_occ[(x>>1)<<3]>y) x++; PN(hspan)(pg,y,s0,x-1,c); }
            continue; }
        PN(hspan)(pg,y,xl,xr,c); }
}