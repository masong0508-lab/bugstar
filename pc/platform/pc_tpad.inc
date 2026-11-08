// pc_tpad.inc: the on-screen touch pad, drawn like a Mega Drive style 6 button pad (included by pc_sdl.c).
// Everything is drawn in code: shapes are signed distance fields, shaded per pixel (bevel, dome, gloss, soft shadows) into small
// textures at the exact size the pad has on THIS screen, so it is sharp at any size or DPI. The textures are cached; a pressed
// button has its own texture (flatter, darker, lower, no shadow). The letters are a tiny stroke font (round caps, optional slant).
// The look is generic: black/dark grey body, round D-pad with a dished centre, round A B C buttons in a slightly rising row,
// X Y Z above, pill shaped START and MODE, a red/blue/grey stripe and a slanted 'MEGA BUG' word mark. No logo of any company.
#include <math.h>
typedef struct { float r, g, b, a; } TgPx;          // premultiplied
static void tg_over(TgPx *p, float r, float g, float b, float a){ if(a<=0) return; if(a>1) a=1; float k=1-a; p->r=r*a+p->r*k; p->g=g*a+p->g*k; p->b=b*a+p->b*k; p->a=a+p->a*k; }
static float tg_clamp(float v, float lo, float hi){ return v<lo?lo:v>hi?hi:v; }
static float tg_smooth(float t){ t=tg_clamp(t,0,1); return t*t*(3-2*t); }
// ---- shapes
typedef struct { int t; float cx, cy, a, b, c; } TgSh;      // t 0 circle (a radius), 1 rounded rect (a,b half sizes, c corner), 2 cross (a arm length, b arm half width, c corner)
static float tg_rr(float x, float y, float hw, float hh, float rad){ float qx=fabsf(x)-hw+rad, qy=fabsf(y)-hh+rad; float mx=qx>0?qx:0, my=qy>0?qy:0; float m=qx>qy?qx:qy; return sqrtf(mx*mx+my*my)+(m<0?m:0)-rad; }
static float tg_sd(const TgSh *s, float x, float y){ x-=s->cx; y-=s->cy;
    if(s->t==0) return sqrtf(x*x+y*y)-s->a;
    if(s->t==1) return tg_rr(x,y,s->a,s->b,s->c);
    { float d1=tg_rr(x,y,s->a,s->b,s->c), d2=tg_rr(x,y,s->b,s->a,s->c); return d1<d2?d1:d2; } }
static float tg_tri(float x, float y, float cx, float cy, float s, int dir){      // a small triangle pointing 0 up 1 down 2 left 3 right, distance (negative inside)
    float px=x-cx, py=y-cy; if(dir==1) py=-py; else if(dir==2){ float t=px; px=py; py=t; } else if(dir==3){ float t=px; px=py; py=-t; }
    // up triangle: apex (0,-s), base at y=+0.7s
    float ax=0, ay=-s, bx=-s*1.05f, by=s*0.7f, cx2=s*1.05f, cy2=s*0.7f; float d=-1e9f;
    float ex[3]={bx-ax,cx2-bx,ax-cx2}, ey[3]={by-ay,cy2-by,ay-cy2}, sx[3]={ax,bx,cx2}, sy[3]={ay,by,cy2};
    for(int i=0;i<3;i++){ float l=sqrtf(ex[i]*ex[i]+ey[i]*ey[i]); float nx=ey[i]/l, ny=-ex[i]/l; float v=(px-sx[i])*nx+(py-sy[i])*ny; if(v>d) d=v; }
    return d; }
// ---- the stroke font (0..10 boxes; PU starts a new line, -1 ends)
#define PU 99
static const signed char tgf_A[]={0,10,5,0,10,10,PU,2,6,8,6,-1};
static const signed char tgf_B[]={0,0,0,10,PU,0,0,6,0,9,1,9,4,6,5,0,5,PU,6,5,10,6,10,9,9,10,6,10,0,10,-1};
static const signed char tgf_C[]={10,2,8,0,5,0,2,1,0,3,0,7,2,9,5,10,8,10,10,8,-1};
static const signed char tgf_D[]={0,0,0,10,PU,0,0,5,0,8,1,10,4,10,6,8,9,5,10,0,10,-1};
static const signed char tgf_E[]={10,0,0,0,0,10,10,10,PU,0,5,7,5,-1};
static const signed char tgf_G[]={10,2,8,0,5,0,2,1,0,3,0,7,2,9,5,10,8,10,10,8,10,5,6,5,-1};
static const signed char tgf_L[]={0,0,0,10,9,10,-1};
static const signed char tgf_M[]={0,10,0,0,5,6,10,0,10,10,-1};
static const signed char tgf_N[]={0,10,0,0,10,10,10,0,-1};
static const signed char tgf_O[]={5,0,8,1,10,3,10,7,8,9,5,10,2,9,0,7,0,3,2,1,5,0,-1};
static const signed char tgf_R[]={0,10,0,0,6,0,9,1,10,3,9,5,6,6,0,6,PU,5,6,10,10,-1};
static const signed char tgf_S[]={10,2,8,0,3,0,0,2,0,4,3,5,7,5,10,6,10,8,7,10,2,10,0,8,-1};
static const signed char tgf_T[]={0,0,10,0,PU,5,0,5,10,-1};
static const signed char tgf_U[]={0,0,0,7,2,9,5,10,8,9,10,7,10,0,-1};
static const signed char tgf_X[]={0,0,10,10,PU,10,0,0,10,-1};
static const signed char tgf_Y[]={0,0,5,5,10,0,PU,5,5,5,10,-1};
static const signed char tgf_Z[]={0,0,10,0,0,10,10,10,-1};
static const signed char *tg_glyph(int c){ switch(c){ case 'A':return tgf_A; case 'B':return tgf_B; case 'C':return tgf_C; case 'D':return tgf_D; case 'E':return tgf_E; case 'G':return tgf_G; case 'L':return tgf_L; case 'M':return tgf_M;
    case 'N':return tgf_N; case 'O':return tgf_O; case 'R':return tgf_R; case 'S':return tgf_S; case 'T':return tgf_T; case 'U':return tgf_U; case 'X':return tgf_X; case 'Y':return tgf_Y; case 'Z':return tgf_Z; } return 0; }
typedef struct { const char *s; float x0, y0, cw, ch, gap, shear; } TgTxt;       // x0,y0: top left of the first letter box; shear: slant (x shift per pixel of height, 0.2 = italic)
static float tg_txt_w(const TgTxt *t){ int n=(int)strlen(t->s); return n*t->cw+(n>0?(n-1)*t->gap:0); }
static float tg_txt_d(const TgTxt *t, float x, float y){         // distance to the nearest stroke centre line, in pixels
    float best=1e9f; x-=t->shear*((t->y0+t->ch)-y);              // un-slant the point
    if(y<t->y0-t->ch*0.3f || y>t->y0+t->ch*1.3f) return best;
    float adv=t->cw+t->gap; int n=(int)strlen(t->s);
    int lo=(int)floorf((x-t->x0)/adv)-1, hi=lo+3; if(lo<0) lo=0; if(hi>n-1) hi=n-1;
    for(int ci=lo;ci<=hi;ci++){ const signed char *g=tg_glyph((unsigned char)t->s[ci]); if(!g) continue; float ox=t->x0+ci*adv;
        float px=0,py=0; int pen=0;
        for(const signed char *q=g;*q!=-1;){ if(*q==PU){ pen=0; q++; continue; } float qx=ox+q[0]*t->cw/10.f, qy=t->y0+q[1]*t->ch/10.f; q+=2;
            if(pen){ float dx=qx-px, dy=qy-py, l2=dx*dx+dy*dy, u=l2>0?((x-px)*dx+(y-py)*dy)/l2:0; u=tg_clamp(u,0,1); float ex=px+dx*u-x, ey=py+dy*u-y; float d=sqrtf(ex*ex+ey*ey); if(d<best) best=d; }
            px=qx; py=qy; pen=1; } }
    return best; }
// ---- shading
static float tg_H(float d, float bw, float rimw, float bwd, float domew){        // height 0..1 inside a shape: a rounded rim plus a gentle dome
    float t=tg_clamp(-d/bw,0,1), t2=tg_clamp(-d/bwd,0,1); return rimw*sinf(t*1.5707963f) + domew*(1-(1-t2)*(1-t2)); }
typedef struct { float bw, rimw, bwd, domew, hs, amb, kd, ks, shin; int inv; } TgMat;      // hs: relief in pixels; inv: concave (a dish)
static void tg_lit(const TgSh *s, float x, float y, const TgMat *m, float br, float bg, float bb, float *o){
    float e=0.7f; float hx=(tg_H(tg_sd(s,x+e,y),m->bw,m->rimw,m->bwd,m->domew)-tg_H(tg_sd(s,x-e,y),m->bw,m->rimw,m->bwd,m->domew))/(2*e);
    float hy=(tg_H(tg_sd(s,x,y+e),m->bw,m->rimw,m->bwd,m->domew)-tg_H(tg_sd(s,x,y-e),m->bw,m->rimw,m->bwd,m->domew))/(2*e);
    float k=m->hs*(m->inv?-1.f:1.f); float nx=-hx*k, ny=-hy*k, nz=1; float il=1.f/sqrtf(nx*nx+ny*ny+nz*nz); nx*=il; ny*=il; nz*=il;
    static const float L[3]={-0.42f,-0.62f,0.66f}; float diff=nx*L[0]+ny*L[1]+nz*L[2]; if(diff<0) diff=0;
    float hvx=L[0], hvy=L[1], hvz=L[2]+1; float hl=1.f/sqrtf(hvx*hvx+hvy*hvy+hvz*hvz); float sp=(nx*hvx+ny*hvy+nz*hvz)*hl; sp=sp>0?powf(sp,m->shin):0;
    float f=m->amb+m->kd*diff; o[0]=br*f+m->ks*sp; o[1]=bg*f+m->ks*sp; o[2]=bb*f+m->ks*sp; }
static void tg_shadow(TgPx *p, const TgSh *s, float x, float y, float ox, float oy, float blur, float alpha){
    float d=tg_sd(s,x-ox,y-oy); float v=tg_smooth(0.5f-d/blur); tg_over(p,0,0,0,alpha*v*v); }
static void tg_body(TgPx *p, const TgSh *s, float x, float y, const TgMat *m, float br, float bg, float bb, float opa){
    float d=tg_sd(s,x,y); float cov=tg_clamp(0.5f-d,0,1); if(cov<=0) return; float c[3]; tg_lit(s,x,y,m,br,bg,bb,c);
    tg_over(p,tg_clamp(c[0],0,1),tg_clamp(c[1],0,1),tg_clamp(c[2],0,1),cov*opa); }
static float tg_stroke(float d, float th){ return tg_clamp(th*0.5f-d+0.5f,0,1); }      // coverage of a stroke of width th at distance d from its centre line
// ---- the textures
enum { TG_DISC=1, TG_PILL, TG_DPAD, TG_ARM, TG_PLATE, TG_LOGO };
typedef struct { SDL_Texture *t; int k, a, b, w, h; unsigned stamp; } TgC;
static TgC tg_c[96]; static unsigned tg_stamp;
static void tg_flush(void){ for(int i=0;i<96;i++) if(tg_c[i].t){ SDL_DestroyTexture(tg_c[i].t); tg_c[i].t=0; } }
static const char *const tg_lab_s[]={"","B","A","L","R","MODE","START","MENU","C","X","Y","Z"};      // label of each element (TC_ order); the GBA key each one presses is in the legend
#define TG_RGB(c) ((((c)>>16)&255)/255.f),((((c)>>8)&255)/255.f),(((c)&255)/255.f)
static void tg_gen_disc(uint32_t *out, int S, int lab, int pr){
    float r=S/2.6f, cx=S/2.f, cy=S/2.f; TgSh well={0,cx,cy,r*1.17f,0,0}, btn={0,cx,cy+(pr?r*0.045f:0),r*(pr?0.955f:1.f),0,0};
    TgMat mw={r*0.25f,0.55f,r*0.9f,0.3f,r*0.45f,0.55f,0.5f,0.0f,8,1};
    TgMat mb= pr ? (TgMat){r*0.18f,0.30f,r*0.95f,0.25f,r*0.55f,0.62f,0.42f,0.10f,10,0} : (TgMat){r*0.22f,0.45f,r*0.95f,0.55f,r*0.95f,0.50f,0.62f,0.55f,24,0};
    TgTxt tx; char lb[2]={tg_lab_s[lab][0],0}; float ch=r*0.58f, cw=ch*0.74f; tx=(TgTxt){lb,cx-cw/2,btn.cy-ch/2,cw,ch,0,0};
    if(lab==TC_X||lab==TC_Y||lab==TC_Z){ }
    for(int y=0;y<S;y++) for(int x=0;x<S;x++){ TgPx p={0,0,0,0}; float fx=x+0.5f, fy=y+0.5f;
        tg_body(&p,&well,fx,fy,&mw,TG_RGB(0x0d0d10),1.f);
        { float dw=tg_sd(&well,fx,fy); if(dw<0 && dw>-r*0.07f){ float t=1-(-dw)/(r*0.07f); float down=(fy-cy)/(r*1.17f); if(down>0) tg_over(&p,0.30f,0.30f,0.34f,0.5f*t*down); } }       // the lit lip of the hole
        tg_shadow(&p,&btn,fx,fy,0,pr?r*0.03f:r*0.11f,pr?r*0.12f:r*0.24f,pr?0.35f:0.62f);
        tg_body(&p,&btn,fx,fy,&mb,TG_RGB(pr?0x1f1f23:0x303036),1.f);
        { float dl=tg_txt_d(&tx,fx,fy); float th=r*0.105f;
            float sh=tg_stroke(tg_txt_d(&tx,fx-r*0.02f,fy-r*0.035f),th); tg_over(&p,0,0,0,sh*0.55f);
            float a=tg_stroke(dl,th); tg_over(&p,TG_RGB(pr?0xa9a9b0:0xe4e4e8),a); }
        float a=p.a; uint32_t px=0; if(a>0){ float ir=1.f/a; px=((uint32_t)(a*255+0.5f)<<24)|((uint32_t)tg_clamp(p.r*ir*255+0.5f,0,255)<<16)|((uint32_t)tg_clamp(p.g*ir*255+0.5f,0,255)<<8)|(uint32_t)tg_clamp(p.b*ir*255+0.5f,0,255); }
        out[y*S+x]=px; } }
static void tg_gen_pill(uint32_t *out, int w, int h, int lab, int pr, int hh_px){
    float hw=(w-hh_px*0.9f)/2.f, hhf=hh_px/2.f, cx=w/2.f, cy=h/2.f; float rad=hhf;
    TgSh body={1,cx,cy+(pr?hhf*0.08f:0),hw,hhf*(pr?0.94f:1.f),rad*(pr?0.94f:1.f)};
    TgMat m= pr ? (TgMat){hhf*0.4f,0.5f,hhf*1.0f,0.3f,hhf*0.5f,0.60f,0.40f,0.05f,8,0} : (TgMat){hhf*0.45f,0.6f,hhf*1.0f,0.5f,hhf*0.8f,0.52f,0.58f,0.30f,14,0};
    TgSh well={1,cx,cy,hw+hhf*0.16f,hhf*1.16f,hhf*1.16f}; TgMat mw={hhf*0.3f,0.5f,hhf*0.9f,0.3f,hhf*0.4f,0.55f,0.5f,0,8,1};
    int slab=lab; TgTxt tx; float ch=hhf*0.74f, cw=ch*0.80f; const char *s=tg_lab_s[slab]; int n=(int)strlen(s); float gap=cw*0.22f; float tw=n*cw+(n-1)*gap;
    if(tw>hw*1.7f){ float k=hw*1.7f/tw; cw*=k; gap*=k; tw*=k; }
    tx=(TgTxt){s,cx-tw/2,body.cy-ch/2,cw,ch,gap,0};
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ TgPx p={0,0,0,0}; float fx=x+0.5f, fy=y+0.5f;
        tg_body(&p,&well,fx,fy,&mw,TG_RGB(0x0d0d10),1.f);
        tg_shadow(&p,&body,fx,fy,0,pr?hhf*0.05f:hhf*0.14f,pr?hhf*0.18f:hhf*0.34f,pr?0.35f:0.6f);
        tg_body(&p,&body,fx,fy,&m,TG_RGB(pr?0x25252a:0x37373d),1.f);
        { float th=hhf*0.15f; float sh=tg_stroke(tg_txt_d(&tx,fx-hhf*0.03f,fy-hhf*0.05f),th); tg_over(&p,0,0,0,sh*0.5f); float a=tg_stroke(tg_txt_d(&tx,fx,fy),th); tg_over(&p,TG_RGB(pr?0xa4a4ab:0xdadade),a); }
        float a=p.a; uint32_t px=0; if(a>0){ float ir=1.f/a; px=((uint32_t)(a*255+0.5f)<<24)|((uint32_t)tg_clamp(p.r*ir*255+0.5f,0,255)<<16)|((uint32_t)tg_clamp(p.g*ir*255+0.5f,0,255)<<8)|(uint32_t)tg_clamp(p.b*ir*255+0.5f,0,255); }
        out[y*w+x]=px; } }
static void tg_gen_dpad(uint32_t *out, int S){
    float Rv=S/2.9f, cx=S/2.f, cy=S/2.f; float arm=Rv*0.335f;
    TgSh plate={0,cx,cy,Rv*1.30f,0,0}, hole={0,cx,cy,Rv*1.10f,0,0}, cross={2,cx,cy,Rv,arm,arm*0.42f}, dish={0,cx,cy,Rv*0.40f,0,0};
    TgMat mp={Rv*0.16f,0.7f,Rv*1.2f,0.25f,Rv*0.55f,0.50f,0.55f,0.20f,14,0}, mh={Rv*0.3f,0.6f,Rv,0.3f,Rv*0.4f,0.55f,0.5f,0,8,1};
    TgMat mc={Rv*0.13f,0.55f,Rv*0.30f,0.45f,Rv*0.55f,0.50f,0.62f,0.60f,26,0}, md={Rv*0.22f,0.2f,Rv*0.40f,1.0f,Rv*0.55f,0.62f,0.55f,0.30f,18,1};
    for(int y=0;y<S;y++) for(int x=0;x<S;x++){ TgPx p={0,0,0,0}; float fx=x+0.5f, fy=y+0.5f;
        tg_shadow(&p,&plate,fx,fy,0,Rv*0.05f,Rv*0.14f,0.55f);
        tg_body(&p,&plate,fx,fy,&mp,TG_RGB(0x2a2a2f),1.f);
        tg_body(&p,&hole,fx,fy,&mh,TG_RGB(0x0c0c0f),1.f);
        { float d=tg_sd(&plate,fx,fy); if(d<0 && d>-Rv*0.05f) tg_over(&p,0.55f,0.55f,0.6f,0.18f*(1-(-d)/(Rv*0.05f))*(fy<cy?1.f:0.f)); }
        tg_shadow(&p,&cross,fx,fy,0,Rv*0.06f,Rv*0.10f,0.7f);
        tg_body(&p,&cross,fx,fy,&mc,TG_RGB(0x34343a),1.f);
        { float ax[4]={0,0,-1,1}, ay[4]={-1,1,0,0}; for(int dd=0;dd<4;dd++){ float tx=cx+ax[dd]*Rv*0.72f, ty=cy+ay[dd]*Rv*0.72f; float d=tg_tri(fx,fy,tx,ty,Rv*0.115f,dd); float a=tg_clamp(0.5f-d,0,1);
                float d2=tg_tri(fx,fy+Rv*0.02f,tx,ty,Rv*0.115f,dd); float a2=tg_clamp(0.5f-d2,0,1); tg_over(&p,0.62f,0.62f,0.66f,a2*0.30f*(1-a)); tg_over(&p,0.07f,0.07f,0.08f,a*0.85f); } }
        tg_body(&p,&dish,fx,fy,&md,TG_RGB(0x40404a),1.f);
        float a=p.a; uint32_t px=0; if(a>0){ float ir=1.f/a; px=((uint32_t)(a*255+0.5f)<<24)|((uint32_t)tg_clamp(p.r*ir*255+0.5f,0,255)<<16)|((uint32_t)tg_clamp(p.g*ir*255+0.5f,0,255)<<8)|(uint32_t)tg_clamp(p.b*ir*255+0.5f,0,255); }
        out[y*S+x]=px; } }
static void tg_gen_arm(uint32_t *out, int S, int dir){       // the pressed arm: dark and flat over the D-pad (same box as the pad texture)
    float Rv=S/2.9f, arm=Rv*0.335f, c=S/2.f; TgSh cross={2,c,c,Rv,arm,arm*0.42f};
    for(int y=0;y<S;y++) for(int x=0;x<S;x++){ float X=x+0.5f-c, Y=y+0.5f-c; float d=tg_sd(&cross,x+0.5f,y+0.5f); float cov=tg_clamp(0.5f-d,0,1); uint32_t px=0;
        if(cov>0){ float ax=fabsf(X), ay=fabsf(Y); float dom= dir==0 ? (Y<0 ? ay-ax : -1e9f) : dir==1 ? (Y>0 ? ay-ax : -1e9f) : dir==2 ? (X<0 ? ax-ay : -1e9f) : (X>0 ? ax-ay : -1e9f);
            float in=tg_clamp(dom/(Rv*0.02f)+0.5f,0,1); float r=sqrtf(X*X+Y*Y); float fall=tg_clamp(r/(Rv*0.45f),0,1);
            float al=cov*in*(0.52f+0.25f*fall); if(al>0) px=(uint32_t)(al*255+0.5f)<<24; }
        out[y*S+x]=px; } }
static void tg_gen_plate(uint32_t *out, int w, int h){
    float cx=w/2.f, cy=h/2.f, m=h*0.07f; TgSh sh={1,cx,cy,w/2.f-m*1.6f,h/2.f-m*1.6f,(h/2.f-m*1.6f)*0.62f};
    TgMat mt={h*0.045f,0.6f,h*0.5f,0.35f,h*0.20f,0.56f,0.50f,0.10f,10,0};
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ TgPx p={0,0,0,0}; float fx=x+0.5f, fy=y+0.5f;
        tg_shadow(&p,&sh,fx,fy,0,m*0.35f,m*0.8f,0.5f);
        tg_body(&p,&sh,fx,fy,&mt,TG_RGB(0x1c1c20),0.93f);
        float a=p.a; uint32_t px=0; if(a>0){ float ir=1.f/a; px=((uint32_t)(a*255+0.5f)<<24)|((uint32_t)tg_clamp(p.r*ir*255+0.5f,0,255)<<16)|((uint32_t)tg_clamp(p.g*ir*255+0.5f,0,255)<<8)|(uint32_t)tg_clamp(p.b*ir*255+0.5f,0,255); }
        out[y*w+x]=px; } }
static void tg_gen_logo(uint32_t *out, int w, int h){       // 'MEGA BUG' in a slanted block face + the red / blue / grey stripe under it
    float ch=h*0.50f, cw=ch*0.62f, gap=cw*0.38f; TgTxt t={"MEGA BUG",0,h*0.10f,cw,ch,gap,0.22f}; float tw=tg_txt_w(&t)+ch*0.22f; t.x0=(w-tw)/2; float th=ch*0.20f;
    float sy=h*0.10f+ch+h*0.08f, sth=h*0.075f; float sx0=t.x0, sx1=t.x0+tw;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ TgPx p={0,0,0,0}; float fx=x+0.5f, fy=y+0.5f;
        float sh=tg_stroke(tg_txt_d(&t,fx-th*0.12f,fy-th*0.18f),th); tg_over(&p,0,0,0,sh*0.55f);
        float a=tg_stroke(tg_txt_d(&t,fx,fy),th); float vg=0.78f+0.22f*tg_clamp((fy-t.y0)/ch,0,1); tg_over(&p,0.86f*vg,0.87f*vg,0.90f*vg,a);
        for(int s=0;s<3;s++){ float y0=sy+s*(sth+h*0.012f); float sl=0.22f*(sy+sth*1.5f-fy); float xa=fx-sl; float cv=tg_clamp(0.5f-fmaxf(fmaxf(y0-fy,fy-(y0+sth)),fmaxf(sx0-xa,xa-sx1)),0,1);
            static const float cl[3][3]={{0.82f,0.10f,0.14f},{0.10f,0.30f,0.70f},{0.62f,0.63f,0.68f}}; tg_over(&p,cl[s][0],cl[s][1],cl[s][2],cv); }
        float a2=p.a; uint32_t px=0; if(a2>0){ float ir=1.f/a2; px=((uint32_t)(a2*255+0.5f)<<24)|((uint32_t)tg_clamp(p.r*ir*255+0.5f,0,255)<<16)|((uint32_t)tg_clamp(p.g*ir*255+0.5f,0,255)<<8)|(uint32_t)tg_clamp(p.b*ir*255+0.5f,0,255); }
        out[y*w+x]=px; } }
static SDL_Texture *tg_get(int k, int a, int b, int w, int h, int extra){
    for(int i=0;i<96;i++) if(tg_c[i].t && tg_c[i].k==k && tg_c[i].a==a && tg_c[i].b==b && tg_c[i].w==w && tg_c[i].h==h){ tg_c[i].stamp=tg_stamp; return tg_c[i].t; }
    int slot=-1; unsigned old=~0u; for(int i=0;i<96;i++){ if(!tg_c[i].t){ slot=i; break; } if(tg_c[i].stamp<old && tg_c[i].stamp!=tg_stamp){ old=tg_c[i].stamp; slot=i; } }
    if(slot<0) return 0; if(tg_c[slot].t){ SDL_DestroyTexture(tg_c[slot].t); tg_c[slot].t=0; }
    int gw=w, gh=h; if(k==TG_PLATE){ gw=(w+1)/2; gh=(h+1)/2; }          // the big plate is drawn at half size (soft shading) and scaled up smoothly
    uint32_t *buf=(uint32_t*)calloc((size_t)gw*gh,4); if(!buf) return 0;
    switch(k){ case TG_DISC: tg_gen_disc(buf,w,a,b); break; case TG_PILL: tg_gen_pill(buf,w,h,a,b,extra); break; case TG_DPAD: tg_gen_dpad(buf,w); break; case TG_ARM: tg_gen_arm(buf,w,a); break;
        case TG_PLATE: tg_gen_plate(buf,gw,gh); break; case TG_LOGO: tg_gen_logo(buf,w,h); break; }
    const char *old_hint=SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY); char sv[16]; snprintf(sv,sizeof sv,"%s",old_hint?old_hint:"nearest");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,k==TG_PLATE?"linear":"nearest");
    SDL_Texture *t=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,gw,gh);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,sv);
    if(t){ SDL_UpdateTexture(t,NULL,buf,gw*4); SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND); }
    free(buf); if(!t) return 0;
    tg_c[slot]=(TgC){t,k,a,b,w,h,tg_stamp}; return t; }
static void tg_blit(SDL_Texture *t, int cx, int cy, int w, int h, int alpha){ if(!t) return; SDL_SetTextureAlphaMod(t,(Uint8)alpha); SDL_Rect d={cx-w/2,cy-h/2,w,h}; SDL_RenderCopy(ren,t,NULL,&d); }
