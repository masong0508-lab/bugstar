// ws_view.c: the first-person view inside a warship. See ws_view.h.
// Units: x, y in cells; heights in half metres (KZ = 0.25 cells per unit); the eye is at 3.2 units (1.6 m).
#include "ws_view.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define KZ 0.25f
#define EYE 3.2f
typedef struct { float r, g, b; } V3;
static inline V3 v3(float r, float g, float b){ V3 v={r,g,b}; return v; }
static inline V3 vmul(V3 a, float k){ return v3(a.r*k,a.g*k,a.b*k); }
static inline V3 vmix(V3 a, V3 b, float t){ return v3(a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t); }
static inline float fr_(float x){ return x-floorf(x); }
static inline uint32_t hh(uint32_t h){ h^=h>>16; h*=0x7FEB352Du; h^=h>>15; h*=0x846CA68Bu; h^=h>>16; return h; }
static inline float hf(int a, int b){ return (hh((uint32_t)a*0x9E3779B1u ^ (uint32_t)b*0x85EBCA6Bu)&0xFFFF)/65535.0f; }
static float vnoise(float x, float y){ int ix=(int)floorf(x), iy=(int)floorf(y); float fx=x-ix, fy=y-iy; fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy);
    float a=hf(ix,iy), b=hf(ix+1,iy), c=hf(ix,iy+1), d=hf(ix+1,iy+1); return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy; }

static const WdDeck *D; static double T; static int BAND;
static float *zb; static int zcap; static int SW, SH; static float FOC;
// culture palettes: hull metal, trim, accent light
static const float pal[WS_NCULT][3][3]={
 {{.62f,.66f,.72f},{.30f,.33f,.38f},{.55f,.80f,1.0f}}, {{.72f,.64f,.52f},{.38f,.30f,.24f},{1.0f,.75f,.40f}}, {{.56f,.52f,.62f},{.28f,.25f,.33f},{.85f,.60f,1.0f}},
 {{.55f,.42f,.28f},{.25f,.18f,.12f},{1.0f,.70f,.20f}}, {{.40f,.58f,.58f},{.18f,.28f,.30f},{.40f,1.0f,.85f}}, {{.86f,.88f,.90f},{.50f,.53f,.58f},{.40f,.75f,1.0f}},
 {{.60f,.57f,.52f},{.33f,.31f,.28f},{.90f,.85f,.60f}}, {{.40f,.42f,.46f},{.18f,.19f,.22f},{1.0f,.35f,.25f}}, {{.82f,.76f,.60f},{.45f,.38f,.25f},{1.0f,.90f,.55f}},
 {{.34f,.30f,.42f},{.16f,.14f,.22f},{.70f,.45f,1.0f}}, {{.55f,.55f,.53f},{.30f,.30f,.29f},{.80f,.85f,.90f}}, {{.70f,.78f,.70f},{.35f,.42f,.36f},{.55f,1.0f,.60f}}};
static V3 P0, P1, PA;

static V3 light_at(float x, float y){                          // smooth light: bilinear between cell centres
    float fx=x-0.5f, fy=y-0.5f; int ix=(int)floorf(fx), iy=(int)floorf(fy); float ax=fx-ix, ay=fy-iy; V3 s=v3(0,0,0);
    for(int k=0;k<4;k++){ int cx=ix+(k&1), cy=iy+(k>>1); if(cx<0) cx=0; if(cy<0) cy=0; if(cx>WD_W-1) cx=WD_W-1; if(cy>WD_W-1) cy=WD_W-1; int i=cy*WD_W+cx;
        float w=((k&1)?ax:1-ax)*((k>>1)?ay:1-ay); s.r+=w*D->lr[i]; s.g+=w*D->lg[i]; s.b+=w*D->lb[i]; }
    return vmul(s,1/255.0f*1.9f);
}
static uint32_t pack(V3 c){ int r=(int)(c.r*255+0.5f), g=(int)(c.g*255+0.5f), b=(int)(c.b*255+0.5f); r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b; return (uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; }
static V3 tonemap(V3 c){ return v3(c.r/(1+c.r*0.35f)*1.25f, c.g/(1+c.g*0.35f)*1.25f, c.b/(1+c.b*0.35f)*1.25f); }
static V3 finish(V3 tex, V3 lit, float emis, float dist){       // light the surface, add its glow, fog into the dark
    V3 c=v3(tex.r*lit.r + emis*tex.r*2.2f, tex.g*lit.g + emis*tex.g*2.2f, tex.b*lit.b + emis*tex.b*2.2f);
    float fog=1.0f/(1.0f+dist*dist*0.0016f); c=vmul(c,fog); V3 haze=vmul(v3(lit.r*0.05f,lit.g*0.06f,lit.b*0.08f),1-fog); c.r+=haze.r; c.g+=haze.g; c.b+=haze.b;
    return tonemap(c);
}

// ---------------------------------------------------------------- surfaces
static V3 wall_tex(int rk, float u, float z, float ceil, uint32_t cellh, float *emis){
    *emis=0; float pu=fr_(u), pz=fr_(z/4.0f); int pid=(int)floorf(u)*31+(int)floorf(z/4.0f);
    V3 base=P0; float var=0.92f+0.16f*hf(pid,(int)cellh&255);
    if(BAND<=WB_MICRO){                                        // a lattice of atoms: hex bonds that glow
        float a=u*1.6f, b=z*0.4f; float hx=fr_(a+ (fr_(b*0.5f)<0.5f?0.5f:0)), hy=fr_(b); float dd=fminf(fminf(hx,1-hx),fminf(hy,1-hy));
        float node=(hx-0.5f)*(hx-0.5f)+(hy-0.5f)*(hy-0.5f); if(node<0.03f){ *emis=0.9f; return PA; } if(dd<0.07f){ *emis=0.35f; return vmix(PA,P1,0.5f); }
        return vmix(P1,P0,0.3f+0.2f*vnoise(u*3,z));
    }
    if(rk==WR_GARDEN) base=v3(.86f,.88f,.84f); else if(rk==WR_QUARTERS||rk==WR_LOUNGE) base=vmix(P0,v3(.62f,.46f,.33f),0.55f); else if(rk==WR_REACTOR||rk==WR_TRENCH) base=vmul(P1,1.05f); else if(rk==WR_MESS) base=vmix(P0,v3(.75f,.70f,.62f),0.4f);
    V3 c=vmul(base,var);
    if(ceil>20 && z>9){                                         // tall halls: ribs and girders up into the dark
        float rib=fr_(u*0.5f); if(rib<0.12f) return vmul(P1,0.8f+rib); if(fr_(z/12.0f)<0.06f) return vmul(P1,0.7f);
        if(fr_(u*0.5f+0.25f)<0.03f && fr_(z/6)<0.5f){ *emis=0.8f; return PA; }
        c=vmul(c,0.75f);
    }
    if(z<1.0f){ if(rk==WR_HANGAR||rk==WR_REACTOR||rk==WR_TURRET){ int st=(int)floorf((u+z*0.5f)*2.5f)&1; return st ? v3(.85f,.66f,.12f) : v3(.08f,.08f,.08f); } return vmul(P1,0.9f); }   // kick plate
    if(rk==WR_TRENCH){ float pz2=fr_(z/2.2f); float cyl=sinf(pz2*3.14159f); c=vmul(vmix(P1,v3(.45f,.40f,.33f),0.4f),0.35f+0.65f*cyl); if(pz2<0.05f) c=vmul(c,0.4f); if(fr_(u*0.25f)<0.04f) c=vmul(P1,0.5f); return c; }
    if(rk==WR_REACTOR && fr_(u*0.66f)<0.06f){ *emis=0.6f+0.4f*sinf((float)T*3+u); return v3(.35f,.75f,1.0f); }
    if(rk==WR_GARDEN && fr_(u*0.7f+vnoise(z*0.3f,u)*0.3f)<0.18f && z<9) return v3(.22f,.55f+0.2f*vnoise(u*4,z),.18f);
    // panels: seams, bevel, a few kinds of inset
    if(pu<0.025f||pu>0.975f||pz<0.03f) return vmul(c,0.45f);
    if(pz>0.96f) c=vmul(c,1.18f); else if(pz<0.08f) c=vmul(c,0.8f);
    if(pu<0.06f) c=vmul(c,1.12f); else if(pu>0.94f) c=vmul(c,0.85f);
    int kind=(int)(hh((uint32_t)pid*2654435761u^cellh)%7u);
    if(kind==0 && pu>0.25f&&pu<0.75f&&pz>0.35f&&pz<0.65f){ float sl=fr_(pz*14); return vmul(c, sl<0.4f ? 0.35f : 0.9f); }                  // vent slots
    if(kind==1 && pu>0.3f&&pu<0.7f&&pz>0.45f&&pz<0.7f && z<8){ *emis=0.7f; float l=fr_(pz*30)<0.5f?1:0.7f; return vmul(PA,l*(0.6f+0.4f*hf(pid,(int)(T*2)))); }   // a little screen
    if(kind==2){ float rx=fabsf(pu-0.5f), rz=fabsf(pz-0.5f); if((rx>0.38f&&rx<0.42f)||(rz>0.38f&&rz<0.42f)) return vmul(c,0.7f); }              // inset frame
    if(kind==3 && (fabsf(pu-0.12f)<0.025f||fabsf(pu-0.88f)<0.025f) && fabsf(fr_(pz*4)-0.5f)<0.1f) return vmul(c,0.5f);                     // rivets
    if(ceil<=20 && z>ceil-2.2f && z<ceil-1.6f && rk!=WR_QUARTERS){ *emis=0.9f; return vmix(PA,v3(1,1,1),0.6f); }                                // a light strip along the top
    return c;
}
static float CURD;
static V3 floor_tex(int rk, float x, float y, int flags, float fz, float *emis){
    *emis=0; float tx=fr_(x), ty=fr_(y); int tid=(int)floorf(x)*73+(int)floorf(y)*19;
    if(fz<-50){ float n=vnoise(x*1.3f+(float)T*0.4f,y*1.3f)*0.6f+vnoise(x*4,y*4-(float)T)*0.4f; *emis=1.0f; return v3(0.35f+0.6f*n,0.55f+0.45f*n,1.0f); }   // the reactor's glow far below
    if(fz<-4 && rk==WR_TRAM){ if(fabsf(fr_(y*0.5f)-0.3f)<0.04f) return v3(.6f,.6f,.62f); return v3(.10f,.10f,.11f); }
    if(BAND<=WB_MICRO){ float d=fminf(fminf(tx,1-tx),fminf(ty,1-ty)); if(d<0.05f){ *emis=0.4f; return PA; } return vmul(P1,0.8f+0.3f*vnoise(x*2,y*2)); }
    if(flags&WF_RAIL){ if(CURD>5) return vmix(v3(.05f,.06f,.08f),v3(.45f,.46f,.48f),0.36f); float gx=fr_(x*6), gy=fr_(y*6); if(gx<0.2f||gy<0.2f) return v3(.45f,.46f,.48f); return v3(.05f,.06f,.08f); }   // a catwalk grating
    if(rk==WR_HANGAR){ float bx=fr_(x*0.5f), by=fr_(y*0.5f); if(bx<0.02f||by<0.02f) return v3(.12f,.12f,.13f); if(fabsf(fr_(x/8.0f)-0.5f)<0.015f || fabsf(fr_(y/6.0f)-0.5f)<0.02f) return v3(.85f,.68f,.10f);
        return vmul(v3(.33f,.34f,.36f),0.9f+0.15f*hf(tid,3)+0.1f*vnoise(x*3,y*3)); }
    if(rk==WR_QUARTERS||rk==WR_LOUNGE){ return vmul(vmix(v3(.35f,.22f,.18f),PA,0.15f),0.85f+0.15f*vnoise(x*8,y*8)); }
    if(rk==WR_GARDEN){ if(fr_(x*0.7f)<0.55f) return v3(.24f+0.1f*vnoise(x*6,y*6),.17f,.10f); return v3(.55f,.56f,.52f); }
    if(tx<0.03f||ty<0.03f) return v3(.10f,.10f,.11f);
    V3 c=vmul(vmix(v3(.30f,.31f,.33f),P1,0.4f),0.85f+0.2f*hf(tid,7));
    if(rk==WR_CORRIDOR){ if(fabsf(tx-0.5f)<0.05f && fr_(y*2)<0.3f){ *emis=0.5f; return PA; } }
    float sp=vnoise(x*9,y*9); return vmul(c,0.9f+0.15f*sp);
}
static V3 ceil_tex(int rk, float x, float y, int flags, float cz, float *emis){
    *emis=0; float tx=fr_(x), ty=fr_(y);
    if(BAND<=WB_MICRO){ *emis=0.15f; return vmul(PA,0.5f+0.5f*vnoise(x,y)); }
    if(cz>20){ float gx=fr_(x*0.25f), gy=fr_(y*0.25f); if(gx<0.08f||gy<0.08f) return vmul(P1,0.55f); if(fabsf(gx-0.5f)<0.06f&&fabsf(gy-0.5f)<0.06f){ *emis=1; return v3(1,.97f,.9f); } return vmul(P1,0.25f); }
    if(flags&WF_LIGHT && tx>0.2f&&tx<0.8f&&ty>0.2f&&ty<0.8f){ *emis=1.0f; return vmix(PA,v3(1,1,1),0.7f); }
    if(tx<0.04f||ty<0.04f) return vmul(P1,0.4f);
    return vmul(P0,0.55f+0.1f*vnoise(x*5,y*5));
}
static V3 space_view(float rx, float ry, float rz, int hangar){
    float a=atan2f(ry,rx), e=rz/sqrtf(rx*rx+ry*ry); int qa=(int)floorf(a*400), qe=(int)floorf(e*400);
    float n=vnoise(a*3+7,e*3)*0.6f+vnoise(a*9,e*9)*0.4f; V3 c=vmul(vmix(v3(.02f,.02f,.05f),vmix(PA,v3(.6f,.2f,.5f),0.4f),n*n*0.5f),1);
    if(hf(qa,qe)>0.996f){ float b=0.6f+0.4f*hf(qe,qa); c=v3(b,b,b*1.05f); }
    float pa=1.2f+(D->seed%7)*0.3f, pe=-0.15f; float da=(a-pa), de=(e-pe); float r2=da*da+de*de;
    if(BAND>=WB_NORMAL && r2<0.06f){ float s=0.25f+0.75f*fmaxf(0,1-((da+0.12f)*(da+0.12f)+(de-0.1f)*(de-0.1f))/0.06f); c=vmul(vmix(v3(.2f,.35f,.6f),P0,vnoise(a*20,e*20)*0.5f),s); }
    if(hangar){ c.b+=0.12f; c.g+=0.04f; }
    return c;
}

// ---------------------------------------------------------------- solid models (props and crew): boxes, upright cylinders, spheres
typedef struct { uint8_t t, pat; float x,y,z, a,b,c; V3 col; float emis; } Pr;     // t 0 box (half sizes a,b,c), 1 cylinder (radius a, half height c), 2 sphere (radius a)
#define MAXPR 4096
static Pr pr[MAXPR]; static int npr;
typedef struct { int p0, p1; float x0,y0,z0,x1,y1,z1; } Obj;
#define MAXOBJ 600
static Obj ob[MAXOBJ]; static int nob;
static void add(int t, float x, float y, float z, float a, float b, float c, V3 col, float emis){ if(npr>=MAXPR) return; Pr *p=&pr[npr++]; p->pat=0; p->t=(uint8_t)t; p->x=x; p->y=y; p->z=z; p->a=a; p->b=b; p->c=c; p->col=col; p->emis=emis; }
static void obj_begin(void){ if(nob<MAXOBJ) ob[nob].p0=npr; }
static void obj_end(void){ if(nob>=MAXOBJ) return; Obj *o=&ob[nob]; o->p1=npr; if(o->p1==o->p0) return;
    o->x0=o->y0=o->z0=1e9f; o->x1=o->y1=o->z1=-1e9f;
    for(int i=o->p0;i<o->p1;i++){ const Pr *p=&pr[i]; float a=p->a, b= p->t==0?p->b:p->a, c= p->t==2?p->a:p->c;
        if(p->x-a<o->x0) o->x0=p->x-a; if(p->x+a>o->x1) o->x1=p->x+a; if(p->y-b<o->y0) o->y0=p->y-b; if(p->y+b>o->y1) o->y1=p->y+b; if(p->z-c<o->z0) o->z0=p->z-c; if(p->z+c>o->z1) o->z1=p->z+c; }
    nob++; }
#define U(h) ((h)*KZ)                                         /* height units to cells */
static V3 cul(int k){ return k==0?P0:k==1?P1:PA; }
static void model_prop(const WdProp *q){
    float x=q->x, y=q->y, z=U(q->z), w=q->w; int a=q->a; float hu = (q->type==WP_CORE||q->type==WP_DOME||q->type==WP_GUN||q->type==WP_PIPE) ? q->h : q->h*4; float h=U(hu);
    static const float fc[4][3]={{.75f,.75f,.78f},{.30f,.34f,.40f},{.70f,.30f,.22f},{.85f,.70f,.25f}};
    obj_begin();
    switch(q->type){
    case WP_FIGHTER: { V3 hull=vmix(v3(fc[a&3][0],fc[a&3][1],fc[a&3][2]),P0,0.35f), dk=vmul(hull,0.55f); float L=w*0.5f, gz=z+h*0.32f;        // a stepped arrowhead on three legs
        add(0,x,y,gz,L*0.95f,L*0.20f,h*0.07f,hull,0); add(0,x-L*0.15f,y,gz,L*0.65f,L*0.55f,h*0.05f,vmul(hull,0.9f),0); add(0,x-L*0.35f,y,gz,L*0.45f,L*0.95f,h*0.035f,dk,0);
        add(0,x-L*0.6f,y-L*0.9f,gz+h*0.12f,L*0.2f,L*0.03f,h*0.14f,dk,0); add(0,x-L*0.6f,y+L*0.9f,gz+h*0.12f,L*0.2f,L*0.03f,h*0.14f,dk,0);
        add(2,x+L*0.25f,y,gz+h*0.07f,L*0.17f,0,0,v3(.12f,.2f,.3f),0.25f); add(0,x+L*0.9f,y,gz,L*0.12f,L*0.06f,h*0.04f,PA,0.5f);
        add(0,x-L*0.92f,y-L*0.12f,gz,L*0.05f,L*0.08f,h*0.05f,v3(1,.6f,.3f),1.4f); add(0,x-L*0.92f,y+L*0.12f,gz,L*0.05f,L*0.08f,h*0.05f,v3(1,.6f,.3f),1.4f);
        add(1,x+L*0.5f,y,z+h*0.13f,0.04f,0,h*0.13f,v3(.15f,.15f,.15f),0); add(1,x-L*0.4f,y-L*0.4f,z+h*0.13f,0.04f,0,h*0.13f,v3(.15f,.15f,.15f),0); add(1,x-L*0.4f,y+L*0.4f,z+h*0.13f,0.04f,0,h*0.13f,v3(.15f,.15f,.15f),0); break; }
    case WP_CRATE: { V3 cc=v3(fc[a&3][0]*0.8f,fc[a&3][1]*0.8f,fc[a&3][2]*0.7f); add(0,x,y,z+h*0.5f,w*0.45f,w*0.45f,h*0.5f,cc,0); add(0,x,y,z+h*0.5f,w*0.46f,w*0.46f,h*0.06f,vmul(cc,0.5f),0); break; }
    case WP_CONSOLE: case WP_SCHEMA: { add(0,x,y,z+h*0.3f,w*0.42f,w*0.32f,h*0.3f,P1,0); V3 sc = q->type==WP_SCHEMA ? v3(.3f,.6f,1.0f) : vmix(PA,v3(.3f,1,.6f),(a&1)*0.5f);
        add(0,x,y,z+h*0.75f,w*0.4f,w*0.06f,h*0.18f,sc,0.9f+0.1f*sinf((float)T*3+x)); if(q->type==WP_SCHEMA) add(1,x,y,z+h*1.15f,w*0.2f,0,h*0.2f,v3(.3f,.6f,1),0.7f); break; }
    case WP_BUNK: { V3 bl=v3(fc[a&3][0]*0.6f+0.2f,0.3f+0.1f*a,0.35f); add(0,x,y,z+h*0.15f,w*0.45f,0.42f,h*0.03f,P1,0); add(0,x,y,z+h*0.2f,w*0.42f,0.38f,h*0.03f,bl,0);
        add(0,x,y,z+h*0.62f,w*0.45f,0.42f,h*0.03f,P1,0); add(0,x,y,z+h*0.67f,w*0.42f,0.38f,h*0.03f,vmul(bl,0.9f),0);
        for(int k=0;k<4;k++) add(0,x+((k&1)?w*0.43f:-w*0.43f),y+((k&2)?0.4f:-0.4f),z+h*0.5f,0.03f,0.03f,h*0.5f,P1,0); break; }
    case WP_TABLE: add(0,x,y,z+h*0.94f,w*0.5f,0.42f,h*0.06f,vmix(P0,v3(.8f,.8f,.78f),0.5f),0); add(1,x,y,z+h*0.45f,0.08f,0,h*0.45f,P1,0); break;
    case WP_BENCH: add(0,x,y,z+h*0.8f,w*0.5f,0.18f,h*0.2f,vmix(P1,v3(.5f,.3f,.2f),0.5f),0); break;
    case WP_SOFA: { V3 s=v3(.45f+0.15f*(a&1),.25f,.30f+0.1f*a); add(0,x,y,z+h*0.25f,w*0.5f,0.35f,h*0.25f,s,0); add(0,x,y-0.3f,z+h*0.6f,w*0.5f,0.08f,h*0.35f,vmul(s,0.85f),0); break; }
    case WP_PLANT: { add(1,x,y,z+0.12f,w*0.22f,0,0.12f,v3(.5f,.35f,.25f),0); V3 g=v3(.18f,.45f+0.1f*a,.15f); add(2,x,y,z+0.25f+h*0.45f,w*0.3f+h*0.1f,0,0,g,0.05f); add(2,x+0.12f,y-0.08f,z+0.25f+h*0.7f,w*0.2f+h*0.05f,0,0,vmul(g,1.15f),0.05f); break; }
    case WP_LAMP: add(0,x,y,z+0.05f,w*0.5f,w*0.25f,0.05f,v3(1,.95f,.85f),1.6f); add(1,x,y,z+0.6f,0.02f,0,0.5f,P1,0); break;
    case WP_CORE: { float rad=w*0.5f, pul=0.8f+0.2f*sinf((float)T*4); add(1,x,y,z+h*0.5f,rad,0,h*0.5f,v3(.45f,.75f,1.0f),0.75f*pul); pr[npr-1].pat=1;
        for(float k=0.05f;k<1;k+=0.07f) add(1,x,y,z+h*k,rad*1.25f,0,0.12f,P1,0); add(1,x,y,z+h-0.3f,rad*1.6f,0,0.3f,vmul(P1,0.8f),0); break; }
    case WP_DOME: add(2,x,y,z,w*0.5f,0,0,v3(.45f,.8f,1),0.6f+0.15f*sinf((float)T*2)); pr[npr-1].pat=2; add(1,x,y,z+0.15f,w*0.55f,0,0.15f,P1,0); for(int k=0;k<6;k++){ float an=k*1.047f+(float)T*0.3f; add(1,x+cosf(an)*w*0.62f,y+sinf(an)*w*0.62f,z+h*0.4f,0.12f,0,h*0.4f,P1,0); add(2,x+cosf(an)*w*0.62f,y+sinf(an)*w*0.62f,z+h*0.82f,0.18f,0,0,PA,1.2f); } break;
    case WP_GUN: add(0,x,y,z+h*0.25f,w*0.5f,w*0.5f,h*0.25f,P1,0); add(1,x,y,z+h*0.6f,w*0.35f,0,h*0.12f,vmul(P0,0.9f),0); add(0,x,y,z+h*0.78f,w*0.12f,w*0.8f,w*0.12f,vmul(P1,1.2f),0); add(0,x,y+w*0.8f,z+h*0.78f,w*0.06f,0.05f,w*0.06f,v3(1,.4f,.2f),1.5f); break;
    case WP_PIPE: add(1,x,y,z+h*0.5f,w*0.5f,0,h*0.5f,vmix(P1,v3(.5f,.42f,.3f),0.4f),0); break;
    case WP_TRAMCAR: add(0,x,y,z+h*0.5f,w*0.5f,0.9f,h*0.42f,vmix(P0,v3(.9f,.9f,.92f),0.4f),0); add(0,x,y-0.92f,z+h*0.62f,w*0.46f,0.02f,h*0.14f,v3(.8f,.9f,1),0.8f); add(0,x,y,z+h*0.05f,w*0.48f,0.6f,h*0.05f,v3(.1f,.1f,.1f),0); break;
    case WP_KEY: { static const float kc[4][3]={{1,1,1},{1,.3f,.3f},{.3f,1,.4f},{.4f,.6f,1}}; add(0,x,y,z+0.45f+0.05f*sinf((float)T*3),0.12f,0.08f,0.02f,v3(kc[a&3][0],kc[a&3][1],kc[a&3][2]),1.4f); add(0,x,y,z+0.2f,0.25f,0.25f,0.2f,P1,0); break; }
    case WP_CHARGE: add(0,x,y,z+0.12f,0.12f,0.12f,0.12f,v3(.3f,.3f,.3f),0); add(2,x,y,z+0.28f,0.05f,0,0,v3(1,.1f,.1f),fmodf((float)T,0.6f)<0.3f?2.0f:0.2f); break;
    default: break;
    }
    obj_end();
}
static void model_npc(const WdNpc *n){
    if(!n->alive && n->act!=WA_DEAD) return;
    float x=n->x, y=n->y; V3 uni = n->kind==WN_GUARD ? v3(.20f,.21f,.24f) : n->kind==WN_OFFICER ? v3(.85f,.85f,.80f) : vmix(PA,P1,0.45f);
    V3 skin=v3(.80f,.62f,.48f); if((int)(x*7+y*3)%3==1) skin=v3(.55f,.40f,.30f); else if((int)(x*7+y*3)%3==2) skin=v3(.92f,.78f,.66f);
    obj_begin();
    if(BAND<=WB_MICRO){ float b=0.08f*sinf((float)T*4+x); add(2,x,y,0.35f+b,0.12f,0,0,n->kind==WN_CREW?PA:v3(1,.3f,.3f),0.8f); add(2,x+0.14f,y,0.45f+b,0.08f,0,0,P0,0.2f); add(2,x-0.12f,y+0.1f,0.28f+b,0.08f,0,0,P0,0.2f); obj_end(); return; }
    if(!n->alive){ add(0,x,y,0.06f,0.4f,0.14f,0.06f,vmul(uni,0.5f),0); obj_end(); return; }
    int act=n->act; float ph=(float)T*6+x*3;
    if(act==WA_SLEEP){ add(0,x,y,0.22f,0.33f,0.13f,0.07f,uni,0); add(2,x-0.38f,y,0.25f,0.09f,0,0,skin,0); obj_end(); return; }
    if(act==WA_EAT){ add(0,x,y,0.32f,0.1f,0.15f,0.13f,uni,0); add(2,x,y,0.56f,0.09f,0,0,skin,0); add(0,x+0.12f,y,0.2f,0.12f,0.12f,0.04f,vmul(uni,0.7f),0); add(0,x+0.2f,y+0.1f*sinf(ph*0.5f),0.4f,0.08f,0.03f,0.03f,uni,0); obj_end(); return; }
    float lift = (act==WA_WALK||act==WA_FLEE||act==WA_HUNT) ? 0.06f : 0.0f; float l1=lift*sinf(ph), l2=-l1; float sway = act==WA_CHAT ? 0.03f*sinf(ph*0.3f) : 0;
    add(0,x,y-0.06f,0.2f+fmaxf(l1,0),0.05f,0.045f,0.2f,vmul(uni,0.6f),0); add(0,x,y+0.06f,0.2f+fmaxf(l2,0),0.05f,0.045f,0.2f,vmul(uni,0.6f),0);
    add(0,x+sway,y,0.55f,0.08f,0.13f,0.16f,uni,0); add(2,x+sway,y,0.8f,0.085f,0,0,skin,0);
    float arm = act==WA_CHAT ? 0.08f*(sinf(ph*0.7f)>0.6f) : 0; add(0,x,y-0.17f,0.55f+arm,0.035f,0.035f,0.14f,uni,0); add(0,x,y+0.17f,0.55f,0.035f,0.035f,0.14f,uni,0);
    if(n->kind==WN_GUARD){ add(2,x+sway,y,0.82f,0.1f,0,0,v3(.15f,.15f,.17f),0); add(0,x+0.07f,y,0.83f,0.03f,0.07f,0.02f,v3(1,.15f,.1f),1.4f); add(0,x+0.15f,y+0.12f,0.6f,0.12f,0.03f,0.03f,v3(.1f,.1f,.1f),0); }
    else if(n->kind==WN_OFFICER) add(0,x,y,0.9f,0.08f,0.09f,0.02f,v3(.9f,.75f,.3f),0.2f);
    else add(2,x-0.02f,y,0.86f,0.08f,0,0,v3(.2f+0.3f*((int)(y*5)&1),.15f,.1f),0);
    obj_end();
}
static int hit_prim(const Pr *p, float ox, float oy, float oz, float dx, float dy, float dz, float *t, float *nx, float *ny, float *nz){
    if(p->t==0){ float tmin=-1e30f, tmax=1e30f; float o[3]={ox-p->x,oy-p->y,oz-p->z}, dd[3]={dx,dy,dz}, h[3]={p->a,p->b,p->c}; int ax=-1; float sg=0;
        for(int k=0;k<3;k++){ if(fabsf(dd[k])<1e-8f){ if(o[k]<-h[k]||o[k]>h[k]) return 0; continue; } float t1=(-h[k]-o[k])/dd[k], t2=(h[k]-o[k])/dd[k]; float s=-1; if(t1>t2){ float tt=t1; t1=t2; t2=tt; s=1; } if(t1>tmin){ tmin=t1; ax=k; sg=s; } if(t2<tmax) tmax=t2; if(tmin>tmax) return 0; }
        if(tmax<0.02f||ax<0) return 0; *t=tmin>0.02f?tmin:tmax; *nx= ax==0?sg:0; *ny= ax==1?sg:0; *nz= ax==2?sg:0; return 1; }
    if(p->t==1){ float ex=ox-p->x, ey=oy-p->y; float A=dx*dx+dy*dy, B=2*(ex*dx+ey*dy), Cc=ex*ex+ey*ey-p->a*p->a; float best=1e30f; int side=0;
        if(A>1e-10f){ float disc=B*B-4*A*Cc; if(disc>=0){ float sq=sqrtf(disc), t1=(-B-sq)/(2*A); if(t1>0.02f){ float zz=oz+dz*t1; if(fabsf(zz-p->z)<=p->c){ best=t1; side=1; } } } }
        if(fabsf(dz)>1e-8f){ for(int s=-1;s<=1;s+=2){ float tc=(p->z+s*p->c-oz)/dz; if(tc>0.02f && tc<best){ float hx=ex+dx*tc, hy=ey+dy*tc; if(hx*hx+hy*hy<=p->a*p->a){ best=tc; side=2*s; } } } }
        if(best>=1e29f) return 0; *t=best; if(side==1){ float hx=ex+dx*best, hy=ey+dy*best, l=sqrtf(hx*hx+hy*hy)+1e-9f; *nx=hx/l; *ny=hy/l; *nz=0; } else { *nx=0; *ny=0; *nz=side>0?1:-1; } return 1; }
    { float ex=ox-p->x, ey=oy-p->y, ez=oz-p->z; float A=dx*dx+dy*dy+dz*dz, B=2*(ex*dx+ey*dy+ez*dz), Cc=ex*ex+ey*ey+ez*ez-p->a*p->a; float disc=B*B-4*A*Cc; if(disc<0) return 0; float t1=(-B-sqrtf(disc))/(2*A); if(t1<0.02f) return 0;
      *t=t1; *nx=(ex+dx*t1)/p->a; *ny=(ey+dy*t1)/p->a; *nz=(ez+dz*t1)/p->a; return 1; }
}

// ---------------------------------------------------------------- the frame
void wv_render(uint32_t *out, int W, int H, const WdDeck *d, float px, float py, float ang, double t, const WvMark *mark, float hurt){
    D=d; T=t; BAND=d->band; SW=W; SH=H; int ci=d->cult%WS_NCULT;
    P0=v3(pal[ci][0][0],pal[ci][0][1],pal[ci][0][2]); P1=v3(pal[ci][1][0],pal[ci][1][1],pal[ci][1][2]); PA=v3(pal[ci][2][0],pal[ci][2][1],pal[ci][2][2]);
    if(W*H>zcap){ free(zb); zb=malloc(sizeof(float)*W*H); zcap=W*H; }
    float dirx=cosf(ang), diry=sinf(ang), tn=0.80f; float plx=-diry*tn, ply=dirx*tn; FOC=(W*0.5f)/tn; float hz=H*0.5f;
    float alarm = d->alarm ? 0.5f+0.5f*sinf((float)t*6) : 0;
    for(int X=0;X<W;X++){
        float camx=2.0f*(X+0.5f)/W-1, rx=dirx+plx*camx, ry=diry+ply*camx;
        int mx=(int)floorf(px), my=(int)floorf(py); float ddx=fabsf(1/(rx==0?1e-9f:rx)), ddy=fabsf(1/(ry==0?1e-9f:ry));
        int sx= rx<0?-1:1, sy= ry<0?-1:1; float sdx= rx<0 ? (px-mx)*ddx : (mx+1-px)*ddx, sdy= ry<0 ? (py-my)*ddy : (my+1-py)*ddy;
        int ytop=0, ybot=H-1; float dprev=0.02f;
        if(mx<0||my<0||mx>=WD_W||my>=WD_W) { for(int Y=0;Y<H;Y++){ out[Y*W+X]=0; zb[Y*W+X]=1e9f; } continue; }
        const WdCell *cur=&d->cell[my*WD_W+mx]; float F=cur->floor, Cc=cur->ceil;
        for(int step=0;step<160 && ytop<=ybot;step++){
            int side; float dn; if(sdx<sdy){ dn=sdx; side=0; } else { dn=sdy; side=1; } if(dn<dprev) dn=dprev;
            int rk=d->room[cur->room].kind;
            if(EYE-F>0){ float yA=hz+(EYE-F)*KZ*FOC/dn, yB=hz+(EYE-F)*KZ*FOC/dprev; int a=(int)ceilf(yA-0.5f), b=(int)floorf(yB-0.5f); if(a<ytop) a=ytop; if(b>ybot) b=ybot;
                for(int Y=a;Y<=b;Y++){ float dist=(EYE-F)*KZ*FOC/((Y+0.5f)-hz); float wx=px+rx*dist, wy=py+ry*dist; float em; int cx=(int)wx, cy=(int)wy; int fl= (cx>=0&&cy>=0&&cx<WD_W&&cy<WD_W) ? d->cell[cy*WD_W+cx].flags : 0;
                    CURD=dist; V3 tx=floor_tex(rk,wx,wy,fl,F,&em); V3 li=light_at(wx,wy); if(F<-50) li=v3(1,1,1);
                    float refl = (rk==WR_HANGAR||rk==WR_CORRIDOR||rk==WR_BRIDGE||rk==WR_CONTROL) && F>-1 ? 0.18f : 0;                      // polished floors pick up the lights
                    if(refl>0){ float ldst=(Cc-EYE)*KZ*FOC; float yy=hz-(ldst/dist); (void)yy; int lcx=(int)wx, lcy=(int)wy; if(lcx>=0&&lcy>=0&&lcx<WD_W&&lcy<WD_W && (d->cell[lcy*WD_W+lcx].flags&WF_LIGHT)) em+=refl*(1-fr_(wx))*fr_(wx)*4*(1-fr_(wy))*fr_(wy)*4; }
                    out[Y*W+X]=pack(finish(tx,li,em,dist)); zb[Y*W+X]=dist; } }
            if(Cc-EYE>0){ float yA=hz-(Cc-EYE)*KZ*FOC/dprev, yB=hz-(Cc-EYE)*KZ*FOC/dn; int a=(int)ceilf(yA-0.5f), b=(int)floorf(yB-0.5f); if(a<ytop) a=ytop; if(b>ybot) b=ybot;
                for(int Y=a;Y<=b;Y++){ float dist=(Cc-EYE)*KZ*FOC/(hz-(Y+0.5f)); float wx=px+rx*dist, wy=py+ry*dist; float em; int cx=(int)wx, cy=(int)wy; int fl=(cx>=0&&cy>=0&&cx<WD_W&&cy<WD_W)?d->cell[cy*WD_W+cx].flags:0;
                    V3 tx=ceil_tex(rk,wx,wy,fl,Cc,&em); V3 li=light_at(wx,wy); out[Y*W+X]=pack(finish(tx,vmul(li,0.8f),em,dist)); zb[Y*W+X]=dist; } }
            // the next cell
            if(side==0){ sdx+=ddx; mx+=sx; } else { sdy+=ddy; my+=sy; }
            float hitu = side==0 ? py+ry*dn : px+rx*dn; hitu-=floorf(hitu);
            const WdCell *nc = (mx<0||my<0||mx>=WD_W||my>=WD_W) ? 0 : &d->cell[my*WD_W+mx];
            int solid = !nc || nc->kind==WDC_SOLID || nc->kind==WDC_WINDOW || nc->kind==WDC_LOCK;
            float wu = (side==0 ? (float)my : (float)mx) + hitu; uint32_t chh = nc ? (uint32_t)(my*WD_W+mx) : 0;
            V3 wl = light_at(px+rx*(dn-0.05f), py+ry*(dn-0.05f)); if(side==1) wl=vmul(wl,0.86f);
            if(solid){ float yT=hz-(Cc-EYE)*KZ*FOC/dn, yB=hz+(EYE-F)*KZ*FOC/dn; int a=(int)ceilf(yT-0.5f), b=(int)floorf(yB-0.5f); if(a<ytop) a=ytop; if(b>ybot) b=ybot;
                int win = nc && nc->kind==WDC_WINDOW, lock = nc && nc->kind==WDC_LOCK;
                for(int Y=a;Y<=b;Y++){ float z=EYE+((hz-(Y+0.5f))*dn/(KZ*FOC)); float em; V3 c;
                    if(win){ float mu=fr_(wu); int frame = mu<0.05f||mu>0.95f || z<F+1.6f || z>Cc-1.2f || fabsf(fr_((z-F)/6.0f)-0.5f)<0.015f;
                        if(frame){ c=finish(vmul(P1,0.9f),wl,0,dn); }
                        else { float rz=(hz-(Y+0.5f))/FOC; c=space_view(rx,ry,rz,rk==WR_HANGAR); c=v3(c.r+0.03f*wl.r,c.g+0.03f*wl.g,c.b+0.04f*wl.b); } }
                    else if(lock){ float mu=fr_(wu); int stripe=((int)floorf((mu+z*0.25f)*4))&1; c = z<F+0.6f||z>F+9 ? finish(P1,wl,0,dn) : fabsf(mu-0.5f)<0.02f ? v3(0,0,0) :
                            (fabsf(mu-0.5f)<0.08f && fabsf(z-F-4.5f)<0.5f) ? v3(1,.2f,.15f) : finish(stripe?v3(.75f,.55f,.1f):vmul(P1,0.9f),wl,0,dn); }
                    else { V3 tx=wall_tex(rk,wu,z-F,Cc-F,chh,&em); c=finish(tx,wl,em,dn); }
                    out[Y*W+X]=pack(c); zb[Y*W+X]=dn; }
                ytop=b+1; break; }
            float nF=nc->floor, nC=nc->ceil; int nrk=d->room[nc->room].kind;
            if(nF>F){ float yT=hz-(nF-EYE)*KZ*FOC/dn, yB=hz-(F-EYE)*KZ*FOC/dn; int a=(int)ceilf(yT-0.5f), b=(int)floorf(yB-0.5f); if(a<ytop) a=ytop; if(b>ybot) b=ybot;
                for(int Y=a;Y<=b;Y++){ float z=EYE+((hz-(Y+0.5f))*dn/(KZ*FOC)); float em; V3 tx = F<-50 ? vmix(v3(.25f,.5f,.8f),P1,fminf(1,(z-F)/60.0f)) : wall_tex(nrk==WR_CORRIDOR?rk:nrk,wu,z-F,nC-F,chh,&em);
                    if(F<-50){ em = fr_(wu*1.5f)<0.08f ? 0.8f : 0.0f; if(fr_(z/10.0f)<0.05f) tx=vmul(P1,0.5f); } V3 li = F<-50 ? vmul(v3(.5f,.8f,1.0f),0.4f+0.6f*fmaxf(0,1-(z-F)/100.0f)) : wl;
                    out[Y*W+X]=pack(finish(tx,li,em,dn)); zb[Y*W+X]=dn; } }
            if(nC<Cc){ float yT=hz-(Cc-EYE)*KZ*FOC/dn, yB=hz-(nC-EYE)*KZ*FOC/dn; int a=(int)ceilf(yT-0.5f), b=(int)floorf(yB-0.5f); if(a<ytop) a=ytop; if(b>ybot) b=ybot;
                for(int Y=a;Y<=b;Y++){ float z=EYE+((hz-(Y+0.5f))*dn/(KZ*FOC)); float em; V3 tx=wall_tex(rk,wu,z-F,Cc-F,chh,&em); if(nc->kind==WDC_DOOR && z<nC+0.4f) tx=vmul(PA,0.6f), em=0.6f; out[Y*W+X]=pack(finish(tx,wl,em,dn)); zb[Y*W+X]=dn; } }
            float ftop = nF>F?nF:F, ctop = nC<Cc?nC:Cc;
            { int b=(int)floorf(hz-(ftop-EYE)*KZ*FOC/dn-0.5f); if(b<ybot) ybot=b; int a=(int)ceilf(hz-(ctop-EYE)*KZ*FOC/dn-0.5f); if(a>ytop) ytop=a; }
            cur=nc; F=nF; Cc=nC; dprev=dn;
        }
        for(int Y=ytop;Y<=ybot;Y++){ out[Y*W+X]=0x050608; zb[Y*W+X]=1e9f; }
    }
    // ---- props and crew: build the models in front of you, then cast every pixel of each one's screen box
    npr=0; nob=0; if(getenv("WV_NOPROP")) goto noprop;
    for(int i=0;i<d->nprop;i++){ const WdProp *q=&d->prop[i]; float vx=q->x-px, vy=q->y-py; float dep=vx*dirx+vy*diry; if(dep<-3 || dep>48) continue; model_prop(q); }
    for(int i=0;i<d->nnpc;i++){ const WdNpc *n=&d->npc[i]; float vx=n->x-px, vy=n->y-py; float dep=vx*dirx+vy*diry; if(dep<-1 || dep>40) continue; model_npc(n); }
    if(mark && mark->on){ obj_begin(); float b=0.15f*sinf((float)t*3); add(2,mark->x,mark->y,U(mark->z)+b,0.22f,0,0,v3(1,.85f,.2f),2.0f); add(1,mark->x,mark->y,U(mark->z)/2,0.03f,0,U(mark->z)/2,v3(1,.85f,.2f),1.0f); obj_end(); }
    float ez=EYE*KZ;
    for(int o=0;o<nob;o++){ const Obj *b=&ob[o];
        float minX=1e9f,maxX=-1e9f,minY=1e9f,maxY=-1e9f; int behind=0;
        for(int k=0;k<8;k++){ float wx=(k&1)?b->x1:b->x0, wy=(k&2)?b->y1:b->y0, wz=(k&4)?b->z1:b->z0; float vx=wx-px, vy=wy-py; float dep=vx*dirx+vy*diry; if(dep<0.05f){ behind=1; break; }
            float lat=(vx*plx+vy*ply)/(tn*tn); float sxp=(W*0.5f)*(1+lat/dep); float syp=hz-(wz-ez)*FOC/dep; if(sxp<minX) minX=sxp; if(sxp>maxX) maxX=sxp; if(syp<minY) minY=syp; if(syp>maxY) maxY=syp; }
        if(behind){ minX=0; maxX=W-1; minY=0; maxY=H-1; }
        int x0=(int)floorf(minX), x1=(int)ceilf(maxX), y0=(int)floorf(minY), y1=(int)ceilf(maxY); if(x0<0) x0=0; if(y0<0) y0=0; if(x1>W-1) x1=W-1; if(y1>H-1) y1=H-1;
        for(int Y=y0;Y<=y1;Y++){ float rz=(hz-(Y+0.5f))/FOC;
            for(int X=x0;X<=x1;X++){ float camx=2.0f*(X+0.5f)/W-1, rx=dirx+plx*camx, ry=diry+ply*camx; float best=zb[Y*W+X]; int bi=-1; float bn[3]={0,0,1};
                for(int i=b->p0;i<b->p1;i++){ float tt,nx,ny,nz; if(hit_prim(&pr[i],px,py,ez,rx,ry,rz,&tt,&nx,&ny,&nz) && tt<best){ best=tt; bi=i; bn[0]=nx; bn[1]=ny; bn[2]=nz; } }
                if(bi<0) continue; const Pr *p=&pr[bi]; float hx=px+rx*best, hy=py+ry*best;
                float lam=0.45f+0.55f*fmaxf(0,bn[0]*0.35f+bn[1]*0.25f+bn[2]*0.9f); float rim=fmaxf(0,1-fabsf(bn[0]*rx+bn[1]*ry+bn[2]*rz)/sqrtf(rx*rx+ry*ry+rz*rz)); lam+=rim*0.15f;
                V3 col=p->col; float em=p->emis;
                if(p->pat==1){ float hz2=ez+rz*best; float an=atan2f(hy-p->y,hx-p->x); float band=sinf(hz2*9-(float)T*5); band*=band; float st=fr_(an*3.0f/3.14159f+(float)T*0.2f);
                    col=vmix(v3(.10f,.25f,.55f),v3(.75f,.95f,1.0f),0.25f+0.75f*band); if(st<0.08f) col=vmul(col,0.4f); em=p->emis*(0.5f+band); }
                else if(p->pat==2){ float hz2=ez+rz*best; float an=atan2f(hy-p->y,hx-p->x); if(fr_(an*4/3.14159f)<0.05f || fr_((hz2-p->z)*6)<0.06f){ col=vmul(col,0.5f); em*=0.3f; } }
                V3 li=light_at(hx,hy); out[Y*W+X]=pack(finish(vmul(col,lam),li,em,best)); zb[Y*W+X]=best; } } }
    noprop:
    if(alarm>0 || hurt>0){ float ar=alarm*0.25f+hurt*0.5f; for(int i=0;i<W*H;i++){ uint32_t c=out[i]; int r=(int)((c>>16&255)*(1-ar)+255*ar), g=(int)((c>>8&255)*(1-ar*0.8f)), bl=(int)((c&255)*(1-ar*0.8f)); out[i]=(uint32_t)(r>255?255:r)<<16|(uint32_t)g<<8|(uint32_t)bl; } }
    { int cx=W/2, cy=H/2, s=W/160+1; for(int k=-3*s;k<=3*s;k++){ if(abs(k)<s) continue; out[cy*W+cx+k]=0xD0E0F0; out[(cy+k)*W+cx]=0xD0E0F0; } }   // the aim
}
