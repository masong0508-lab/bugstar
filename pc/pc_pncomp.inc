// pc_pncomp.inc: included by pc_hw.c. The native planet (pc_pnative.inc) leaves a window-size picture in pc_pn.buf (0x00RRGGBB) and a copy of the page as the
// game's 3D pass left it; the compositor shows the picture wherever the page still holds that copy (text, HUD, minimap, rain stay the game's own).
typedef struct { int w,h,ya; uint32_t *buf; long f[2]; uint8_t snap[2][240*160]; } PcNat;
PcNat pc_pn;
static void pn_compose(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcNat *n=&pc_pn; int t=(dc>>4)&1; if(!n->buf || n->w!=W || n->h!=H || g_frame-n->f[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    const uint8_t *pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)), *sn=n->snap[t]; double sy=160.0/H, sx=240.0/W; int fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0;
    for(int Y=n->ya;Y<H;Y++){ int gy=(int)((Y+0.5)*sy); if(gy>159) gy=159; if(gy<18) continue; const uint32_t *src=n->buf+(size_t)(Y-n->ya)*W;
        for(int X=0;X<W;X++){ int gx=(int)((X+0.5)*sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X;
            if((nv_op[q]&7)<4 || pgb[gi]!=sn[gi]) continue; uint32_t c=src[X];
            if(fadeop==2){ uint32_t r2=(c>>16&255)+((255-(c>>16&255))*ev>>4), g2=(c>>8&255)+((255-(c>>8&255))*ev>>4), b2=(c&255)+((255-(c&255))*ev>>4); c=r2<<16|g2<<8|b2; }
            else if(fadeop==3){ uint32_t r2=(c>>16&255)*(16-ev)>>4, g2=(c>>8&255)*(16-ev)>>4, b2=(c&255)*(16-ev)>>4; c=r2<<16|g2<<8|b2; }
            out[q]=c; } }
}
