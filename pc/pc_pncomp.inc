// pc_pncomp.inc: included by pc_hw.c. The native planet (pc_pnative.inc) leaves a window-size picture in pc_pn.buf (0x00RRGGBB) and a copy of the page as the
// game's 3D pass left it; the compositor shows the picture wherever the page still holds that copy (text, HUD, minimap, rain stay the game's own).
typedef struct { int w,h,ya; uint32_t *buf; long f[2]; uint8_t snap[2][240*160]; int sc; } PcNat;
PcNat pc_pn;
// the picture may be made smaller than the window (the quality levels: f = 2 is half size each way) and is shown with each of its pixels repeated f times
typedef struct { uint32_t *out; int W,H,f,fadeop,ev; const uint8_t *pgb,*sn; double sx,sy; PcNat *n; int ya; } PnCC;
static void pn_rows(void *vc,int lo,int hi){
    PnCC *c=(PnCC*)vc; PcNat *n=c->n; int W=c->W, f=c->f, fadeop=c->fadeop, ev=c->ev; uint32_t *out=c->out;
    for(int Y=c->ya+lo;Y<c->ya+hi;Y++){ int gy=(int)((Y+0.5)*c->sy); if(gy>159) gy=159; if(gy<18) continue;
        int Yi=Y/f; if(Yi<n->ya) Yi=n->ya; if(Yi>=n->h) Yi=n->h-1; const uint32_t *src=n->buf+(size_t)(Yi-n->ya)*n->w;
        for(int X=0;X<W;X++){ int gx=(int)((X+0.5)*c->sx); if(gx>239) gx=239; int gi=gy*240+gx; size_t q=(size_t)Y*W+X;
            if((nv_op[q]&7)<4 || c->pgb[gi]!=c->sn[gi]) continue; uint32_t col=src[f==1?X:X/f];
            if(fadeop==2){ uint32_t r2=(col>>16&255)+((255-(col>>16&255))*ev>>4), g2=(col>>8&255)+((255-(col>>8&255))*ev>>4), b2=(col&255)+((255-(col&255))*ev>>4); col=r2<<16|g2<<8|b2; }
            else if(fadeop==3){ uint32_t r2=(col>>16&255)*(16-ev)>>4, g2=(col>>8&255)*(16-ev)>>4, b2=(col&255)*(16-ev)>>4; col=r2<<16|g2<<8|b2; }
            out[q]=col; nv_op[q]|=0x40; } }       // (0x40: the window picture has this pixel, the bitmap pass skips it)
}
static void pn_compose(uint32_t *out,int W,int H,uint16_t dc,int bmode,int ev,uint16_t bldcnt){
    PcNat *n=&pc_pn; int t=(dc>>4)&1; if(!n->buf || n->w<=0 || n->sc<1 || n->w!=(W+n->sc-1)/n->sc || n->h!=(H+n->sc-1)/n->sc || g_frame-n->f[t]>4 || pc_v3d_classic() || ((dc&7)!=4)) return;
    int ya=(18*H+159)/160; bl_y0=ya; bl_y1=H;
    PnCC c; c.out=out; c.W=W; c.H=H; c.f=n->sc; c.fadeop=(bmode>=2 && ev>0 && (bldcnt&4)) ? bmode : 0; c.ev=ev; c.pgb=(const uint8_t*)(uintptr_t)(VRAMB+(t?0xA000:0)); c.sn=n->snap[t]; c.sx=240.0/W; c.sy=160.0/H; c.n=n; c.ya=ya;
    pc_par_chunk(H-ya,16,pn_rows,&c);
}
