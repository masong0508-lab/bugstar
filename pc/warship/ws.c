// ws.c: COLOSSAL WARSHIPS: wide numbers, sizes, the hull grammar, costs, the one reward formula, threat. See ws.h.
#include "ws.h"
#include "../giant/giant.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ================================================================= WN
static WN wn_norm(double m, int64_t e){
    WN r; if(!(m>0) || m!=m){ r.m=0; r.e=0; return r; }
    if(isinf(m)){ r.m=9.999999999999; r.e=WN_EMAX; return r; }
    int k=(int)floor(log10(m)); m/=pow(10.0,k); e+=k;
    while(m>=10.0){ m/=10.0; e++; } while(m<1.0){ m*=10.0; e--; }
    if(e>WN_EMAX){ r.m=9.999999999999; r.e=WN_EMAX; return r; }
    if(e< -WN_EMAX){ r.m=0; r.e=0; return r; }
    r.m=m; r.e=e; return r;
}
WN wn(double v){ return wn_norm(v,0); }
WN wn_p10(double x){ if(x>(double)WN_EMAX) return wn_norm(9.999999999999,WN_EMAX); if(x< -(double)WN_EMAX) return wn(0);
    double fl=floor(x); return wn_norm(pow(10.0,x-fl),(int64_t)fl); }
int wn_is_zero(WN a){ return a.m==0; }
int wn_sat(WN a){ return a.e>=WN_EMAX && a.m>9.99; }
WN wn_mul(WN a, WN b){ if(a.m==0||b.m==0) return wn(0); if(wn_sat(a)||wn_sat(b)) return wn_norm(9.999999999999,WN_EMAX); return wn_norm(a.m*b.m, a.e+b.e); }
WN wn_div(WN a, WN b){ if(a.m==0) return wn(0); if(b.m==0) return wn_norm(9.999999999999,WN_EMAX); return wn_norm(a.m/b.m, a.e-b.e); }
WN wn_muld(WN a, double k){ return wn_mul(a,wn(k)); }
WN wn_add(WN a, WN b){ if(a.m==0) return b; if(b.m==0) return a; if(a.e<b.e){ WN t=a; a=b; b=t; }
    int64_t d=a.e-b.e; if(d>18) return a; return wn_norm(a.m + b.m*pow(10.0,-(double)d), a.e); }
WN wn_sub(WN a, WN b){ if(wn_cmp(a,b)<=0) return wn(0); if(b.m==0) return a; int64_t d=a.e-b.e; if(d>18) return a; return wn_norm(a.m - b.m*pow(10.0,-(double)d), a.e); }
double wn_log10(WN a){ return a.m==0 ? -1e300 : (double)a.e + log10(a.m); }
WN wn_pow(WN a, double p){ if(a.m==0) return wn(0); return wn_p10(wn_log10(a)*p); }
double wn_to_d(WN a){ if(a.m==0) return 0; if(a.e>308) return INFINITY; if(a.e<-308) return 0; return a.m*pow(10.0,(double)a.e); }
int wn_cmp(WN a, WN b){ if(a.m==0||b.m==0){ if(a.m==b.m) return 0; return a.m==0?-1:1; }
    if(a.e!=b.e) return a.e<b.e?-1:1; if(a.m!=b.m) return a.m<b.m?-1:1; return 0; }

static const char *const small_nm[22]={"","THOUSAND","MILLION","BILLION","TRILLION","QUADRILLION","QUINTILLION","SEXTILLION","SEPTILLION","OCTILLION","NONILLION","DECILLION",
    "UNDECILLION","DUODECILLION","TREDECILLION","QUATTUORDECILLION","QUINDECILLION","SEXDECILLION","SEPTENDECILLION","OCTODECILLION","NOVEMDECILLION","VIGINTILLION"};
static void thousand_name(int64_t g, char *out, int cap){        // the name of 1000^g
    if(g<22){ snprintf(out,cap,"%s",small_nm[g]); return; }
    char m[32]; snprintf(m,sizeof m,"%lld",(long long)(g-1)); gu64 L=gcw_place(m,out,cap);
    if(L>(gu64)(cap-1) && cap>4){ out[cap-3]='.'; out[cap-2]='.'; out[cap-1]=0; }
}
static void commas(double v, char *out, int cap){                 // a whole number below 10^6 with commas
    long long n=(long long)(v+0.5); if(n<1000){ snprintf(out,cap,"%lld",n); return; } snprintf(out,cap,"%lld,%03lld",n/1000,n%1000); }
static void sig3(double v, char *out, int cap){                   // 1 <= v < 1000, three significant digits, no trailing zeros
    char t[32]; if(v>=99.95) snprintf(t,sizeof t,"%.0f",v); else if(v>=9.995) snprintf(t,sizeof t,"%.1f",v); else snprintf(t,sizeof t,"%.2f",v);
    int n=(int)strlen(t); if(strchr(t,'.')){ while(n>0 && t[n-1]=='0') t[--n]=0; if(n>0 && t[n-1]=='.') t[--n]=0; } snprintf(out,cap,"%s",t); }
void wn_sci(WN a, char *out, int cap){ if(a.m==0){ snprintf(out,cap,"0"); return; } snprintf(out,cap,"%.2f X 10^%lld",a.m,(long long)a.e); }
void wn_text(WN a, char *out, int cap, int style){
    char nm[96], v[32];
    if(a.m==0){ snprintf(out,cap,"0"); return; }
    if(style==1){ snprintf(out,cap,"%.2fE%lld",a.m,(long long)a.e); return; }
    if(a.e>=0 && a.e<6){ double x=a.m*pow(10.0,(double)a.e); if(x<100) sig3(x,v,sizeof v); else commas(x,v,sizeof v); snprintf(out,cap,"%s",v); return; }
    if(a.e<0 && a.e>=-4){ char f[16]; snprintf(f,sizeof f,"%%.%df",(int)(-a.e)+2); snprintf(v,sizeof v,f,a.m*pow(10.0,(double)a.e)); snprintf(out,cap,"%s",v); return; }
    int64_t g; double lead;
    if(a.e>0){ g=a.e/3; lead=a.m*pow(10.0,(double)(a.e-3*g)); }
    else { g=(-a.e+2)/3; lead=a.m*pow(10.0,(double)(a.e+3*g)); }
    if(lead>=999.5){ lead/=1000; g+= a.e>0 ? 1 : -1; }
    sig3(lead,v,sizeof v); thousand_name(g,nm,40);
    if(a.e>0) snprintf(out,cap,"%s %s",v,nm); else snprintf(out,cap,"%s %sTHS",v,nm);
    if(style==2 && (a.e>=24 || a.e<=-24)){ int n=(int)strlen(out); snprintf(out+n,cap-n," (%.2fE%lld)",a.m,(long long)a.e); }
}

// ================================================================= sizes and classes
static const short cls_lo[WC_N]={-32000,-600,-300,0,100,200,300,400,480,650,850,1050,1400,1700,2000,2250,2600,2900,5000};
static const char *const cls_nm[WC_N]={"NANO SWARM","MICRO SKIMMER","MITE DRONE","SKIFF","CUTTER","FRIGATE","DREADNOUGHT","CITADEL","MOON FORTRESS",
    "WORLD BREAKER","STAR HULK","SYSTEM ARK","NEBULA LEVIATHAN","CLUSTER COLOSSUS","GALAXY TITAN","SUPERCLUSTER BEHEMOTH","UNIVERSE HULK","MULTIVERSE HULK","BEYOND HULK"};
static const char *const cls_ab[WC_N]={"MACHINES SMALLER THAN A VIRUS, A LATTICE OF ATOMS","SKIMMERS THE SIZE OF A CELL","DRONES THE SIZE OF A FLEA",
    "A ONE-PILOT BOAT","A CREWED PATROL SHIP","A WARSHIP AS LONG AS A STADIUM","KILOMETRES OF GUNS","A FLYING CITY","A BATTLE STATION AS BIG AS A MOON",
    "A HULL AS BIG AS A PLANET","A HULL AS BIG AS A STAR","IT COULD SWALLOW A SOLAR SYSTEM","LIGHT YEARS LONG, LIKE A NEBULA","AS BIG AS A STAR CLUSTER",
    "AS BIG AS A GALAXY","AS BIG AS A SUPERCLUSTER","AS BIG AS THE WHOLE UNIVERSE","BIGGER THAN A UNIVERSE: IT HOLDS SEVERAL","ONLY CHEATS MAKE THESE"};
const char *ws_class_name(int c){ return (c>=0&&c<WC_N)?cls_nm[c]:"?"; }
const char *ws_class_about(int c){ return (c>=0&&c<WC_N)?cls_ab[c]:"?"; }
int ws_class_of(int z){ int c=0; for(int i=0;i<WC_N;i++) if(z>=cls_lo[i]) c=i; return c; }
int ws_class_lo(int c){ return cls_lo[c<0?0:c>=WC_N?WC_N-1:c]; }
void ws_len_text(int z, char *out, int cap){
    double lg=z/100.0; char v[64]; const char *u; double sh;
    if(lg<-6){ u="NM"; sh=-9; } else if(lg<-3){ u="MICROMETRES"; sh=-6; } else if(lg<0){ u="MM"; sh=-3; } else if(lg<3){ u="M"; sh=0; }
    else if(lg<10.5){ u="KM"; sh=3; } else if(lg<14.976){ u="AU"; sh=11.17493; } else if(lg<24.944){ u="LY"; sh=15.97598; } else { u="UNIVERSES"; sh=log10(WS_UNIVERSE_M); }
    WN q=wn_p10(lg-sh); wn_text(q,v,sizeof v,0); if(strlen(v)>26) snprintf(v,sizeof v,"%.2f X 10^%lld",q.m,(long long)q.e); snprintf(out,cap,"%s %s",v,u);
}

// ================================================================= the hull grammar
static const char *const shp_nm[WS_NSHAPE]={"SPHERE","RING","SPINDLE","WEDGE","HIVE CLUSTER","ORGANIC SHELL","CITY SHIP","FLEET-IN-ONE","TORUS","NEEDLE","CRESCENT","LATTICE","DISC","CROSS","FORK","SPIRAL"};
static const float shp_fs[WS_NSHAPE]={0.52f,0.06f,0.12f,0.17f,0.25f,0.30f,0.20f,0.10f,0.12f,0.03f,0.15f,0.05f,0.10f,0.09f,0.08f,0.07f};
static const float shp_fa[WS_NSHAPE]={3.14f,1.5f,1.2f,1.4f,2.5f,2.2f,2.8f,3.0f,2.0f,0.5f,1.8f,3.5f,1.7f,1.6f,1.5f,2.2f};
static const char *const srf_nm[WS_NSURF]={"SMOOTH","RIBBED","PLATED","CRYSTAL-FACETED","SCALED","GREEBLED","MIRRORED","PITTED"};
static const char *const cul_nm[WS_NCULT]={"KESHARI CONCORD","VOLLAR SPAN","MOTH SYNOD","ARRAK HIVE","PELUM DRIFTERS","QUORIN TECHNATE","SILT MONKS","IRON TIDE","LUMEN COURT","HOLLOW CHOIR","GRANITE KIN","EVERGREEN ASCENDANCY"};
static const float cul_crew[WS_NCULT]={1.0f,0.8f,1.3f,2.2f,0.7f,0.5f,1.1f,1.6f,0.9f,0.6f,1.2f,1.0f};
static const float cul_pay[WS_NCULT]={1.0f,1.4f,0.8f,0.3f,1.1f,1.8f,0.6f,0.9f,2.0f,1.2f,0.9f,1.0f};
static const float cul_wpn[WS_NCULT]={1.0f,1.2f,0.8f,0.9f,0.7f,1.5f,0.6f,1.6f,1.1f,1.3f,1.0f,0.8f};
static const char *const mat_nm[WS_NMAT]={"HULL STEEL","CARBON WEAVE","TITANITE ALLOY","CERAMIC PLATE","LIVING CHITIN","CRYSTAL LATTICE","VOIDGLASS","PHASE FOAM","STARFORGED PLATE","NEUTRONIUM SCALE"};
static const WsMat mat_tab[WS_NMAT]={{7800,2,1.0,1.0},{1800,6,1.6,1.2},{4500,5,1.4,1.0},{3000,3,1.1,1.1},{1300,4,0.9,1.4},{2600,9,1.3,0.9},{2200,14,1.8,0.8},{300,20,0.7,2.5},{19000,12,2.6,0.7},{400000,30,9.0,0.15}};
static const char *const add_nm[WS_NADD]={"SPINES","FINS","PODS","OUTER RINGS","TOWERS","ENGINE BANKS"};
static const char *const sub_nm[WSS_NKIND]={"SHIELD GENERATOR","REACTOR","HANGAR","TURRET NEST","BRIDGE"};
const char *ws_shape_name(int s){ return shp_nm[(unsigned)s%WS_NSHAPE]; }
const char *ws_surf_name(int s){ return srf_nm[(unsigned)s%WS_NSURF]; }
const char *ws_cult_name(int c){ return cul_nm[(unsigned)c%WS_NCULT]; }
const char *ws_mat_name(int m){ return mat_nm[(unsigned)m%WS_NMAT]; }
const char *ws_add_name(int b){ return add_nm[(unsigned)b%WS_NADD]; }
const char *ws_sub_name(int k){ return sub_nm[(unsigned)k%WSS_NKIND]; }
const WsMat *ws_mat(int m){ return &mat_tab[(unsigned)m%WS_NMAT]; }

static uint32_t hmx(uint32_t h){ h^=h>>16; h*=0x7FEB352Du; h^=h>>15; h*=0x846CA68Bu; h^=h>>16; return h; }
typedef struct { uint32_t s; } Rng;
static uint32_t rn(Rng *r){ r->s=hmx(r->s+0x9E3779B9u); return r->s; }
static int rr(Rng *r, int n){ return (int)(rn(r)%(uint32_t)n); }
static int pickw(Rng *r, const unsigned char *w, int n){ int t=0; for(int i=0;i<n;i++) t+=w[i]; int x=rr(r,t); for(int i=0;i<n;i++){ if(x<w[i]) return i; x-=w[i]; } return n-1; }

static const char *const syl_a[24]={"VOL","KESH","AR","THRA","MOR","QUIN","SEL","DRAV","OR","UL","ZEN","BRAK","LUM","PEL","HAR","SKA","VEX","NOR","CAL","IM","TOR","GRAV","ESH","RUN"};
static const char *const syl_b[24]={"LAR","ETH","ION","AKA","OS","UNDA","IR","OTH","ESK","AVA","UM","IX","ARA","ON","EL","OR","YN","ASH","OCK","IRA","UL","ENT","AX","OME"};
static const char *const epi[16]={"THE GREY","THE PATIENT","THE LAST","THE SILENT","THE ENDLESS","THE BURNING","THE QUIET","THE HUNGRY","THE FIRST","THE DROWNED","THE HOLLOW","THE BRIGHT","THE IRON","THE SLEEPING","THE WANDERING","THE UNBORN"};
static void make_name(Rng *r, char *out, int cap){
    char a[16], b[16]; snprintf(a,sizeof a,"%s%s",syl_a[rr(r,24)],syl_b[rr(r,24)]);
    if(rr(r,3)) snprintf(b,sizeof b,"%s%s",syl_a[rr(r,24)],syl_b[rr(r,24)]); else b[0]=0;
    if(rr(r,4)==0) snprintf(out,cap,"%s %s",epi[rr(r,16)],a); else if(b[0]) snprintf(out,cap,"%s %s",a,b); else snprintf(out,cap,"%s",a);
}

// ---- the hull model: primitives in unit space (length 1 along z)
static void P(WsCraft *c, int k, float x, float y, float z, float a, float b, float cc, float yaw, float pit){
    if(c->nprim>=WS_MAXPRIM) return; WsPrim *p=&c->prim[c->nprim++]; p->kind=(uint8_t)k; p->x=x; p->y=y; p->z=z; p->a=a; p->b=b; p->c=cc; p->yaw=yaw; p->pitch=pit; }
static void build_prims(WsCraft *c, Rng *r){
    int S=c->sym, G=c->segs; float tw=6.2831853f; c->nprim=0;
    switch(c->shape){
    case WS_SPHERE: P(c,0,0,0,0,0.5f,0.5f,0.5f,0,0); P(c,2,0,0,0,0.5f,0.03f,0,0,0); P(c,0,0.22f,0.18f,-0.36f,0.09f,0.09f,0.03f,0,0); break;   // the ball, its equator trench, a dish
    case WS_RING: for(int g=0;g<(G>3?3:G);g++) P(c,2,0,0,(g-(G>3?3:G)/2.0f)*0.08f,0.5f-g*0.06f,0.05f,0,0,1.5708f); P(c,0,0,0,0,0.08f,0.08f,0.08f,0,0); break;
    case WS_SPINDLE: P(c,3,0,0,0.25f,0.12f,0.12f,0.25f,0,0); P(c,3,0,0,-0.25f,0.12f,0.12f,-0.25f,0,0); P(c,4,0,0,0,0.13f,0.13f,0.1f,0,0); break;
    case WS_WEDGE: P(c,3,0,0,0,0.32f,0.08f,0.5f,0,0); P(c,1,0,0.06f,-0.28f,0.1f,0.05f,0.08f,0,0); break;
    case WS_HIVE: for(int i=0;i<7+G;i++){ float a=i*2.4f, rad=0.08f+0.03f*(i%4); P(c,0,0.28f*cosf(a)*(i%3)/2.0f,0.28f*sinf(a)*(i%3)/2.0f,(i-(7+G)/2.0f)*0.06f,rad,rad,rad,0,0); } break;
    case WS_SHELL: P(c,0,0,0,0,0.3f,0.24f,0.5f,0,0); P(c,3,0,0,0.42f,0.14f,0.14f,0.16f,0,0); P(c,2,0,0,-0.1f,0.3f,0.04f,0,0,1.5708f); break;
    case WS_CITY: P(c,4,0,-0.06f,0,0.45f,0.45f,0.04f,0,1.5708f); for(int i=0;i<5+G;i++){ float a=i*1.7f, d=0.1f+0.07f*(i%4); float h=0.06f+0.03f*(i%5); P(c,1,d*cosf(a),h*0.5f,d*sinf(a),0.03f,h,0.03f,0,0); } break;
    case WS_FLEET: for(int i=0;i<6+G;i++){ float x=((i%3)-1)*0.25f, z=(i/3-1)*0.22f; P(c,3,x,((i*7)%3-1)*0.06f,z,0.05f,0.03f,0.1f,0,0); } P(c,4,0,0,0,0.02f,0.02f,0.45f,0,0); break;
    case WS_TORUS: P(c,2,0,0,0,0.38f,0.12f,0,0,1.5708f); P(c,4,0,0,0,0.03f,0.03f,0.38f,0,0); break;
    case WS_NEEDLE: P(c,3,0,0,0.2f,0.04f,0.04f,0.3f,0,0); P(c,4,0,0,-0.2f,0.045f,0.045f,0.1f,0,0); break;
    case WS_CRESCENT: for(int i=0;i<7;i++){ float a=-1.3f+i*0.43f; P(c,0,0.4f*sinf(a),0,0.4f*cosf(a)-0.1f,0.07f+0.02f*(3-abs(i-3)),0.07f,0.07f,0,0); } break;
    case WS_LATTICE: for(int i=0;i<8;i++){ float x=(i&1)?0.3f:-0.3f, y=(i&2)?0.3f:-0.3f, z=(i&4)?0.45f:-0.45f; P(c,0,x,y,z,0.05f,0.05f,0.05f,0,0); } P(c,1,0,0,0,0.3f,0.3f,0.45f,0,0); break;
    case WS_DISC: P(c,4,0,0,0,0.5f,0.5f,0.05f,0,1.5708f); P(c,0,0,0.06f,0,0.12f,0.06f,0.12f,0,0); break;
    case WS_CROSS: P(c,1,0,0,0,0.06f,0.06f,0.5f,0,0); P(c,1,0,0,0.1f,0.4f,0.05f,0.06f,0,0); break;
    case WS_FORK: P(c,4,0,0,-0.2f,0.07f,0.07f,0.3f,0,0); for(int i=-1;i<=1;i++) P(c,3,i*0.15f,0,0.25f,0.04f,0.04f,0.25f,0,0); break;
    default: for(int i=0;i<10;i++){ float a=i*0.8f; P(c,0,0.3f*cosf(a)*i/10,0.3f*sinf(a)*i/10,(i-5)*0.09f,0.05f+0.01f*i,0.05f+0.01f*i,0.05f,0,0); } break;   // SPIRAL
    }
    int base=c->nprim;                                                    // symmetry: copies turned round the long axis
    if(S>1 && c->shape!=WS_SPHERE && c->shape!=WS_RING && c->shape!=WS_TORUS && c->shape!=WS_DISC)
        for(int k=1;k<S;k++) for(int i=0;i<base;i++){ WsPrim q=c->prim[i]; float a=tw*k/S, x=q.x*cosf(a)-q.y*sinf(a), y=q.x*sinf(a)+q.y*cosf(a);
            if(fabsf(q.x)<1e-3f && fabsf(q.y)<1e-3f && k>0) continue; P(c,q.kind,x,y,q.z,q.a,q.b,q.c,q.yaw+a,q.pitch); }
    for(int b=0;b<WS_NADD;b++) if(c->adds>>b&1){ int n=2+rr(r,3);
        for(int i=0;i<n;i++){ float a=tw*i/n+rr(r,100)*0.01f, z=-0.4f+0.8f*i/(n>1?n-1:1);
            switch(b){ case 0: P(c,3,0.35f*cosf(a),0.35f*sinf(a),z*0.6f,0.015f,0.015f,0.12f,a,1.2f); break;
                       case 1: P(c,1,0.38f*cosf(a),0.38f*sinf(a),-0.3f,0.1f,0.01f,0.08f,a,0); break;
                       case 2: P(c,0,0.32f*cosf(a),0.32f*sinf(a),z*0.5f,0.05f,0.05f,0.08f,0,0); break;
                       case 3: if(i==0) P(c,2,0,0,z*0.3f,0.55f,0.015f,0,0,1.5708f); break;
                       case 4: P(c,4,0.18f*cosf(a),0.3f,0.18f*sinf(a),0.02f,0.02f,0.08f,0,1.5708f); break;
                       default: P(c,4,0.12f*cosf(a),0.12f*sinf(a),-0.5f,0.04f,0.04f,0.05f,0,0); break; } } }
    if(G>1 && c->shape!=WS_RING){ int n=c->nprim<WS_MAXPRIM-G ? G : 0; for(int g=0;g<n;g++) P(c,2,0,0,-0.45f+0.9f*g/(G-1),0.2f+0.02f*g,0.01f,0,0,1.5708f); }   // segment bands
}

#define WS_K 52500.636                       // gold per credit: set so that the reference station pays 4.07 x 10^23 (tested)
double ws_K(void){ return WS_K; }

static void fill_numbers(WsCraft *c){
    const WsMat *mt=ws_mat(c->mat); double lg=c->z/100.0; c->logL=lg;
    double fs=shp_fs[c->shape]*(1.0+0.03*__builtin_popcount(c->adds))*(1.0+0.02*(c->segs-1)), fa=shp_fa[c->shape]*(1.0+0.05*__builtin_popcount(c->adds));
    c->fs=fs; c->fa=fa;
    WN L=wn_p10(lg), L3=wn_p10(3*lg); c->len=L;
    double sf=0.12*mt->structf; if(sf>0.6) sf=0.6;
    WN env=wn_muld(L3,fs); c->vol=env;
    c->mass=wn_muld(env, mt->rho*sf);
    WN Vi=wn_muld(env,1.0-sf);
    int ci=c->cult; double hang=0.6+0.2*((c->adds>>2)&1)+0.25*(c->shape==WS_FLEET)+0.15*(c->shape==WS_HIVE);
    c->crew=wn_muld(wn_pow(Vi,0.6),0.0012*cul_crew[ci]);
    double al=0.002*pow(10.0,0.75*lg), cap=pow(10.0,lg)/12; if(lg<40 && al>cap) al=cap; c->air_len=al;
    double air_lg = (lg<40 ? log10(al) : log10(0.002)+0.75*lg);
    c->air_n=wn_muld(wn_pow(Vi,0.55),0.0004*hang);
    c->wpn_n=wn_muld(wn_p10(0.45*lg),0.05*sqrt(fa)*cul_wpn[ci]*20);
    c->power=wn_muld(c->mass,1000.0*(0.5+0.1*(c->mat)));
    WN mat=wn_muld(c->mass,mt->price);
    WN crew=wn_mul(wn_muld(c->crew,5.4e11*cul_pay[ci]),wn_p10(1.2*(lg-5.2)));                     // a whole service life of pay, food, training and upkeep per crew member
    WN air=wn_mul(c->air_n, wn_p10(3*air_lg + log10(0.1*7800*5.85e6)));
    WN wpn=wn_muld(wn_p10(2.9*lg),288.0*fa*cul_wpn[ci]);
    WN pw=wn_muld(c->power,8e-4);
    double K=WS_K; c->cost[WK_MAT]=wn_muld(mat,K); c->cost[WK_CREW]=wn_muld(crew,K); c->cost[WK_AIR]=wn_muld(air,K); c->cost[WK_WPN]=wn_muld(wpn,K); c->cost[WK_POW]=wn_muld(pw,K);
    WN t=wn(0); for(int i=0;i<5;i++) t=wn_add(t,c->cost[i]); c->cost[WK_TOTAL]=t;
    c->hp_hull=wn_muld(c->mass, mt->hard/1e5);
    c->layers=1+c->cls/3; if(c->layers>6) c->layers=6;
    c->hp_shield=wn_muld(c->hp_hull,0.04);
    double d=lg+9; if(d<0) d=0; if(d>100) d=100; c->dps=0.5+0.2*d;
    c->wave=2+c->cls/2; if(c->wave>12) c->wave=12;
}
static const char *const sec_nm[8]={"OUTER ARMOUR","GUN DECKS","HANGAR RING","PLATING BELT","MID HULL","INNER BULWARK","REACTOR CORE","COMMAND CORE"};
static void build_sections(WsCraft *c, Rng *r){
    int n=3+c->cls/4; if(n>WS_MAXSEC) n=WS_MAXSEC; c->nsec=n;
    double share[WS_MAXSEC], tot=0; for(int i=0;i<n;i++){ share[i] = i==n-1 ? 0 : 1.0+0.15*rr(r,5); tot+=share[i]; }
    for(int i=0;i<n;i++){ double s = i==n-1 ? 0.15 : 0.85*share[i]/tot; WsSec *s_=&c->sec[i];
        int ni = i==n-1 ? 7 : i==n-2 ? 6 : (i*5)/(n-2>0?n-2:1); snprintf(s_->name,sizeof s_->name,"%s",sec_nm[ni<8?ni:7]);
        s_->hp_max=wn_muld(c->hp_hull,s); s_->hp=s_->hp_max; s_->nsub=0; }
    int ng=1+c->cls/5, nr=1+c->cls/7, nh=1+c->cls/6, nt=2+c->cls/4; if(ng>4) ng=4; if(nr>3) nr=3; if(nh>4) nh=4; if(nt>6) nt=6;
    if(c->z<0){ nh=nh>1?1:nh; }                                            // the smallest craft have one bay of even smaller swarmers
    int kinds[WS_MAXSUB], k=0;
    for(int i=0;i<nt;i++) kinds[k++]=WSS_TURRET; for(int i=0;i<nh;i++) kinds[k++]=WSS_HANGAR; for(int i=0;i<ng;i++) kinds[k++]=WSS_GEN; for(int i=0;i<nr;i++) kinds[k++]=WSS_REACTOR; kinds[k++]=WSS_BRIDGE;
    c->nsub=k;
    for(int i=0;i<k;i++){ WsSub *s=&c->sub[i]; s->kind=(uint8_t)kinds[i]; s->alive=1; s->inside_kill=0;
        int sec; switch(s->kind){ case WSS_TURRET: sec=rr(r,n>2?2:1); break; case WSS_HANGAR: sec=n>3?1+rr(r,2):rr(r,n-1); break;
            case WSS_GEN: sec=1+rr(r,n-2>0?n-2:1); if(sec>n-2) sec=n-2; break; case WSS_REACTOR: sec=n-2; break; default: sec=n-1; }
        if(sec<0) sec=0; s->sec=(uint8_t)sec; c->sec[sec].nsub++;
        double sh = s->kind==WSS_REACTOR ? 0.10 : s->kind==WSS_BRIDGE ? 0.25 : 0.05;
        s->hp_max=wn_muld(c->sec[sec].hp_max,sh); s->hp=s->hp_max; }
}
void ws_make(WsCraft *c, uint32_t seed, int z, int cheat){
    memset(c,0,sizeof *c); if(z<WS_ZMIN) z=WS_ZMIN; if(z>WS_ZMAX) z=WS_ZMAX;
    c->seed=seed; c->z=z; c->cheat = cheat || z>WS_ZMAX_FAIR; c->cls=ws_class_of(z);
    Rng r={seed*0x2545F491u ^ 0xC0105A1u};
    static const unsigned char w_small[WS_NSHAPE]={6,2,5,6,14,10,1,1,3,6,3,16,3,3,4,10}, w_mid[WS_NSHAPE]={9,7,10,12,6,7,8,6,6,7,6,4,8,6,6,4}, w_huge[WS_NSHAPE]={8,9,6,5,9,9,10,10,8,4,7,9,8,5,4,12};
    const unsigned char *w = c->cls<=WC_MITE ? w_small : c->cls<=WC_WORLD ? w_mid : w_huge;
    c->shape=pickw(&r,w,WS_NSHAPE);
    static const unsigned char syms[6]={1,2,3,4,6,8}; c->sym=syms[rr(&r,6)]; c->segs=1+rr(&r,8); c->adds=rr(&r,64); c->surf=rr(&r,WS_NSURF); c->cult=rr(&r,WS_NCULT);
    static const unsigned char wm[WS_NMAT]={20,14,12,12,9,8,6,5,4,2}; c->mat=pickw(&r,wm,WS_NMAT);
    if(c->mat==9 && c->cls<WC_DREAD) c->mat=8;                              // neutronium only on big hulls
    make_name(&r,c->name,sizeof c->name);
    snprintf(c->title,sizeof c->title,"%s %s",ws_shape_name(c->shape),ws_class_name(c->cls));
    fill_numbers(c); build_sections(c,&r); build_prims(c,&r);
}
void ws_scale(WsCraft *c, int z, int cheat){                      // the same design at another size
    if(z<WS_ZMIN) z=WS_ZMIN; if(z>WS_ZMAX) z=WS_ZMAX; c->z=z; c->cheat = cheat || z>WS_ZMAX_FAIR; c->cls=ws_class_of(z);
    Rng r={c->seed*0x2545F491u ^ 0xC0105A1u ^ 0x5CA1Eu}; snprintf(c->title,sizeof c->title,"%s %s",ws_shape_name(c->shape),ws_class_name(c->cls));
    fill_numbers(c); build_sections(c,&r); Rng r2={c->seed^0xB00Fu}; build_prims(c,&r2);
}
void ws_make_ref(WsCraft *c){
    memset(c,0,sizeof *c); Rng r={0x5EF0u}; c->seed=0; c->z=WS_ZREF; c->cls=ws_class_of(WS_ZREF);
    c->shape=WS_SPHERE; c->sym=1; c->segs=1; c->adds=0; c->surf=2; c->cult=0; c->mat=0;
    snprintf(c->name,sizeof c->name,"THE GREY MOON"); snprintf(c->title,sizeof c->title,"REFERENCE MOON FORTRESS");
    fill_numbers(c); build_sections(c,&r); build_prims(c,&r);
}
uint32_t ws_signature(const WsCraft *c){ uint32_t h=0x811C9DC5u; int v[7]={c->shape,c->sym,c->segs,c->adds,c->surf,c->cult,c->mat}; for(int i=0;i<7;i++) h=hmx(h^(uint32_t)v[i]*0x9E3779B1u^(uint32_t)i); return h; }

// ================================================================= the reward
static const char *const wpn_nm[WW_N]={"LASER","RAILGUN","MISSILES","TORPEDOES","NOVA LANCE"};
static const double wpn_mu[WW_N]={1,4,12,60,200};
const char *ws_wpn_name(int w){ return wpn_nm[(unsigned)w%WW_N]; }
double ws_wpn_mult(int w){ return wpn_mu[(unsigned)w%WW_N]; }
double ws_attack(double level, int laser_lv, int gun, int tech_lv){ double a=level+5.0*laser_lv+gun+25.0*tech_lv; return a<1?1:a; }
double ws_A(double a){ if(a<1) a=1; return pow(a,-WS_ALPHA); }
double ws_Fm(double in, double pr){ if(in<0) in=0; if(in>1) in=1; if(pr<0) pr=0; if(pr>1) pr=1; return 1.0-0.45*in+0.15*in*pr; }
double ws_Fw(const WsKill *k){ double tot=0, f=0;
    WN sum=wn(0); for(int w=0;w<WW_N;w++) sum=wn_add(sum,k->out_dmg[w]); if(wn_is_zero(sum)) return 1.0;
    for(int w=0;w<WW_N;w++){ double sh=wn_to_d(wn_div(k->out_dmg[w],sum)); f+=sh*pow(wpn_mu[w],-0.4); tot+=sh; }
    return tot>0 ? f/tot : 1.0; }
void ws_pay(const WsCraft *c, const WsKill *k, WsPay *p){
    memset(p,0,sizeof *p); p->cost=c->cost[WK_TOTAL]; p->A=ws_A(k->attack);
    WN out=wn(0); for(int w=0;w<WW_N;w++) out=wn_add(out,k->out_dmg[w]); WN all=wn_add(out,k->in_dmg);
    p->inside = wn_is_zero(all) ? 0 : wn_to_d(wn_div(k->in_dmg,all)); p->precision = k->sab_all>0 ? (double)k->sab_crit/k->sab_all : 0;
    p->Fm=ws_Fm(p->inside,p->precision); p->Fw=ws_Fw(k);
    p->reward=wn_muld(wn_muld(wn_muld(p->cost,p->A),p->Fm),p->Fw); p->cheat=c->cheat;
}

// ================================================================= the fight numbers and THREAT
double ws_shot(double a, int w){ if(a<1) a=1; return 10.0*a*a*a*wpn_mu[(unsigned)w%WW_N]; }
static const char *const thr_w[6]={"TRIVIAL","EASY","FAIR","HARD","DEADLY","HOPELESS"};
const char *ws_threat_word(int t){ return thr_w[t<0?0:t>5?5:t]; }
void ws_threat(const WsCraft *c, double attack, int ehp, int wing, int kits, int fuel, WsThreat *t){
    memset(t,0,sizeof *t);
    double lay=wn_log10(c->hp_shield), hul=wn_log10(c->hp_hull);
    // to break a shield layer you must out-shoot its regeneration (3 percent of a layer each second); to finish in time, the whole lot in 10 minutes
    double need_dps_lg = lay+log10(0.03*2.5); double t2 = log10(pow(10.0,hul-lay)+c->layers)+lay-log10(600.0); if(t2>need_dps_lg) need_dps_lg=t2;
    double need_lg = (need_dps_lg - log10(60.0))/3.0;                   // laser: 6 shots a second of 10 x attack^3
    t->need_attack = need_lg>300 ? 1e300 : pow(10.0,need_lg); if(t->need_attack<1) t->need_attack=1;
    double my_dps_lg=log10(60.0)+3*log10(attack<1?1:attack);
    double ttk_lg = log10(pow(10.0,hul-lay)+c->layers)+lay-my_dps_lg; t->ttk_s = ttk_lg>300?1e300:pow(10.0,ttk_lg);
    double tt = t->ttk_s<5?5:t->ttk_s>900?900:t->ttk_s;
    t->need_ehp=(int)(60+c->dps*tt*0.30); t->need_wing = c->cls<WC_FRIGATE?0 : c->cls/2>10?10:c->cls/2; t->need_kits=(int)(c->dps*tt*0.3/60); t->need_fuel=(int)(tt*0.25+20);
    double ra=attack/t->need_attack, eff=ehp+kits*50.0+wing*25.0, re=eff/(t->need_ehp+t->need_kits*50.0+t->need_wing*25.0); if(fuel<t->need_fuel) re*=0.8;
    double r = ra<1 ? ra*ra*ra : (re<ra?re:ra); if(ra>=1 && re<1) r=re;
    t->tier = r>=4?0 : r>=1.6?1 : r>=1?2 : r>=0.6?3 : r>=0.25?4 : 5; t->word=thr_w[t->tier];
}
