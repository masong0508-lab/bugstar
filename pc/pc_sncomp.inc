// pc_sncomp.inc: included by pc_hw.c. The native space view (pc_snative.inc) leaves a window-size picture of the background in pc_sn.buf and a copy of the page
// as it was right after the background. The compositor shows the picture wherever the page still holds that copy and the copy is something the picture redraws
// (black, stars, sun, planets and moons); ships, rocks, station, HUD and cockpit stay the game's own pixels.
typedef struct { int w,h,hn; uint32_t *buf; long f[2]; uint8_t snap[2][240*128]; uint8_t ok[256]; int sunx,suny; uint8_t snap2[2][240*128]; uint8_t *msk; long f2[2]; uint32_t *ibuf; uint8_t foot[2][240*128], fval[2][240*128]; long ff[2]; } PcSNat;
PcSNat pc_sn;
static void sn_compose(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcSNat *n=&pc_sn; int t=(dc>>4)&1; if(!n->buf || n->w!=W || n->h!=H || g_frame-n->f[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    bl_y0=0; bl_y1=H*128/160;
    const uint8_t *pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)), *sn=n->snap[t], *sn2=n->snap2[t]; int has2 = n->msk && g_frame-n->f2[t]<=4, has3 = n->msk && g_frame-n->ff[t]<=4; double sy=160.0/H, sx=240.0/W; int fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0;
    for(int Y=0;Y<n->hn;Y++){ int gy=(int)((Y+0.5)*sy); if(gy>127) break; const uint32_t *src=n->buf+(size_t)Y*W;
        for(int X=0;X<W;X++){ int gx=(int)((X+0.5)*sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X;
            if((nv_op[q]&7)<4) continue;
            if(has3 && n->foot[t][gi] && pgb[gi]==n->fval[t][gi]){ uint32_t c=src[X];                      // a cargo can, spark, bolt or crosshair the game drew blocky: the picture's own background here (the layer goes over it afterwards)
                if(fadeop==2){ uint32_t r2=(c>>16&255)+((255-(c>>16&255))*ev>>4), g2=(c>>8&255)+((255-(c>>8&255))*ev>>4), b2=(c&255)+((255-(c&255))*ev>>4); c=r2<<16|g2<<8|b2; }
                else if(fadeop==3){ uint32_t r2=(c>>16&255)*(16-ev)>>4, g2=(c>>8&255)*(16-ev)>>4, b2=(c&255)*(16-ev)>>4; c=r2<<16|g2<<8|b2; }
                out[q]=c; continue; }
            if(has2 && n->msk[q] && pgb[gi]==sn2[gi]){ uint32_t c=src[X];                                  // a wireframe the native view redraws: its blocky copy is hidden here
                if(fadeop==2){ uint32_t r2=(c>>16&255)+((255-(c>>16&255))*ev>>4), g2=(c>>8&255)+((255-(c>>8&255))*ev>>4), b2=(c&255)+((255-(c&255))*ev>>4); c=r2<<16|g2<<8|b2; }
                else if(fadeop==3){ uint32_t r2=(c>>16&255)*(16-ev)>>4, g2=(c>>8&255)*(16-ev)>>4, b2=(c&255)*(16-ev)>>4; c=r2<<16|g2<<8|b2; }
                out[q]=c; continue; }
            if(pgb[gi]!=sn[gi]) continue; int ix=sn[gi];
            if(!n->ok[ix] && !(ix==51 && gx>=n->sunx-12 && gx<=n->sunx+12 && gy>=n->suny-12 && gy<=n->suny+12)) continue; uint32_t c=src[X];
            if(fadeop==2){ uint32_t r2=(c>>16&255)+((255-(c>>16&255))*ev>>4), g2=(c>>8&255)+((255-(c>>8&255))*ev>>4), b2=(c&255)+((255-(c&255))*ev>>4); c=r2<<16|g2<<8|b2; }
            else if(fadeop==3){ uint32_t r2=(c>>16&255)*(16-ev)>>4, g2=(c>>8&255)*(16-ev)>>4, b2=(c&255)*(16-ev)>>4; c=r2<<16|g2<<8|b2; }
            out[q]=c; } }
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
