// pc_sncomp.inc: included by pc_hw.c. The native space view (pc_snative.inc) leaves a window-size picture of the background in pc_sn.buf and a copy of the page
// as it was right after the background. The compositor shows the picture wherever the page still holds that copy and the copy is something the picture redraws
// (black, stars, sun, planets and moons); ships, rocks, station, HUD and cockpit stay the game's own pixels.
typedef struct { int w,h,hn; uint32_t *buf; long f[2]; uint8_t snap[2][240*128]; uint8_t ok[256]; int sunx,suny; uint8_t snap2[2][240*128]; uint8_t *msk; long f2[2]; } PcSNat;
PcSNat pc_sn;
static void sn_compose(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcSNat *n=&pc_sn; int t=(dc>>4)&1; if(!n->buf || n->w!=W || n->h!=H || g_frame-n->f[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    const uint8_t *pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)), *sn=n->snap[t], *sn2=n->snap2[t]; int has2 = n->msk && g_frame-n->f2[t]<=4; double sy=160.0/H, sx=240.0/W; int fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0;
    for(int Y=0;Y<n->hn;Y++){ int gy=(int)((Y+0.5)*sy); if(gy>127) break; const uint32_t *src=n->buf+(size_t)Y*W;
        for(int X=0;X<W;X++){ int gx=(int)((X+0.5)*sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X;
            if((nv_op[q]&7)<4) continue;
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
