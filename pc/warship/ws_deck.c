// ws_deck.c: the procedural interior of a warship. See ws_deck.h.
#include "ws_deck.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

static uint32_t hx(uint32_t h){ h^=h>>16; h*=0x7FEB352Du; h^=h>>15; h*=0x846CA68Bu; h^=h>>16; return h; }
typedef struct { uint32_t s; } R;
static uint32_t rn(R *r){ r->s=hx(r->s+0x9E3779B9u); return r->s; }
static int ri(R *r, int n){ return n<=1?0:(int)(rn(r)%(uint32_t)n); }
static float rf(R *r){ return (rn(r)&0xFFFFFF)/16777216.0f; }

// ================================================================= the tree of sectors
int wd_levels(const WsCraft *c){ double k=(c->logL-log10(WD_LEAF_M))/log10(2.0); if(k<=0) return 0; return (int)ceil(k); }
uint32_t wd_root(const WsCraft *c){ return hx(c->seed*0x9E3779B1u ^ (uint32_t)c->z*0x85EBCA6Bu ^ 0xDEC0u); }
uint32_t wd_child(uint32_t node, int child, int level){ return hx(node ^ (uint32_t)(child+1)*0x2545F491u ^ (uint32_t)level*0x27D4EB2Fu); }
int wd_sub_child(const WsCraft *c, int sub, int level){ return (int)(hx(c->seed*0x632BE5ABu ^ (uint32_t)(sub+7)*0x9E3779B9u ^ (uint32_t)level*0x85EBCA77u)%8u); }
int wd_band(const WsCraft *c){ double l=c->logL; return l<-6 ? WB_MOLECULE : l<-2 ? WB_MICRO : l<2 ? WB_SMALL : l<5 ? WB_NORMAL : l<12 ? WB_VAST : WB_COSMIC; }
double wd_level_m(const WsCraft *c, int level){ return pow(10.0,c->logL)/pow(2.0,level); }
static const char *const lvl_nm[9]={"GALAXY WING","NEBULA REACH","STAR SPAN","SYSTEM SPAN","WORLD DECK","MOON SECTOR","DISTRICT","QUARTER","BLOCK"};
static const char *const syl[16]={"KETH","ARA","VOL","MIR","SANG","OSK","TEL","DURA","PHAE","NOV","ILK","ROTH","CEN","YAVA","BRO","ZEL"};
void wd_level_name(const WsCraft *c, int level, uint32_t h, char *out, int cap){
    double m=wd_level_m(c,level); double l=log10(m>0?m:1e-300); if(m<=0) l=c->logL-level*0.30103;
    int k = l>20?0 : l>16?1 : l>13?2 : l>10?3 : l>7?4 : l>5?5 : l>3.6?6 : l>3?7 : 8;
    snprintf(out,cap,"%s %s %d",lvl_nm[k],syl[h%16],(int)((h>>8)%90)+1);
}
int wd_subs_at(const WsCraft *c, const uint8_t *path, int depth, int *subs){
    int D=wd_levels(c), n=0; if(depth!=D) return 0;
    for(int s=0;s<c->nsub;s++){ int ok=1; for(int k=0;k<depth && ok;k++) if(path[k]!=wd_sub_child(c,s,k)) ok=0; if(ok) subs[n++]=s; }
    return n;
}

// ================================================================= names at every scale
static const char *const rk_nm[WR_NKIND]={"CORRIDOR","HANGAR BAY","REACTOR SHAFT","CONTROL ROOM","CREW QUARTERS","MESS HALL","LOUNGE","HYDROPONIC GARDEN","WORKSHOP","SERVICE TRENCH","TRAM STATION","SHIELD GENERATOR HALL","TURRET ROOT","BRIDGE","SECURITY OFFICE"};
static const char *const rk_mol[WR_NKIND]={"BOND CHANNEL","DOCKING PORE","ATOM CORE","LOGIC GATE","RESTING LATTICE","FUEL ENZYME BAY","SIGNAL NODE","GROWTH LATTICE","ASSEMBLER NODE","ION TRENCH","CARRIER RAIL","FIELD NODE","EMITTER CRYSTAL","CORTEX KNOT","GUARD LATTICE"};
const char *wd_room_kind(int k, int band){ k=(k<0||k>=WR_NKIND)?0:k; return band<=WB_MICRO ? rk_mol[k] : rk_nm[k]; }
static const char *const act_nm[8]={"SLEEPING","EATING","CHATTING","WORKING","WALKING","RUNNING","HUNTING YOU","DOWN"};
const char *wd_act_name(int a){ return act_nm[(unsigned)a&7]; }

// ================================================================= building a deck
#define C(d,x,y) ((d)->cell[(y)*WD_W+(x)])
static int inb(int x,int y){ return x>=0&&y>=0&&x<WD_W&&y<WD_W; }
static const uint8_t rk_w[WR_NKIND][2]={{0,0},{16,12},{11,11},{8,6},{9,6},{9,7},{8,6},{10,8},{8,6},{3,14},{12,6},{10,10},{8,8},{11,7},{6,5}};
static const int8_t rk_ceil[WR_NKIND]={8,40,50,10,7,9,9,14,10,22,16,36,24,14,8};
static int8_t scale_ceil(int c, int band){ int v=c; if(band==WB_SMALL||band<=WB_MICRO) v=c*6/10; else if(band==WB_VAST) v=c*18/10; else if(band==WB_COSMIC) v=c*22/10; if(v<6) v=6; if(v>124) v=124; return (int8_t)v; }
static int g_tight;      // set only inside wd_make_mix when there is a mix
static int add_room(WdDeck *d, R *r, int kind, int sub){
    if(d->nroom>=WD_MAXROOM) return -1;
    int w=rk_w[kind][0], h=rk_w[kind][1]; if(d->band==WB_SMALL||d->band<=WB_MICRO){ w=w*7/10; h=h*7/10; } else if(d->band>=WB_VAST){ int k = g_tight ? 11 : 13; w=w*k/10; h=h*k/10; }      // (a mix needs more rooms on the same 64 x 64 floor, so its rooms are a little smaller)
    if(w<3) w=3; if(h<3) h=3; if(kind==WR_TRENCH && ri(r,2)){ int t=w; w=h; h=t; }
    w+=ri(r,3); h+=ri(r,3); if(w>30) w=30; if(h>30) h=30;
    for(int tries=0;tries<360;tries++){
        if(tries==120||tries==240){ w=w*2/3; h=h*2/3; if(w<3) w=3; if(h<3) h=3; }
        if(g_tight && (tries==300||tries==330||tries==350)){ w=w*3/4; h=h*3/4; if(w<3) w=3; if(h<3) h=3; }      // a room mix needs its rooms: shrink a little more before giving up (the plain builder is unchanged)
        int x0=3+ri(r,WD_W-6-w), y0=3+ri(r,WD_W-6-h), x1=x0+w-1, y1=y0+h-1, ok=1;
        int gap = (g_tight && tries>=200) ? 3 : 4;        // (mix mode only: when the deck is crowded, rooms may sit one cell closer)
        for(int i=1;i<d->nroom && ok;i++){ WdRoom *o=&d->room[i]; if(x0<=o->x1+gap && x1>=o->x0-gap && y0<=o->y1+gap && y1>=o->y0-gap) ok=0; }
        if(!ok) continue;
        WdRoom *m=&d->room[d->nroom]; memset(m,0,sizeof *m); m->kind=(uint8_t)kind; m->x0=(uint8_t)x0; m->y0=(uint8_t)y0; m->x1=(uint8_t)x1; m->y1=(uint8_t)y1; m->sub=(uint8_t)sub;
        m->ceil=scale_ceil(rk_ceil[kind],d->band); m->floor = kind==WR_TRENCH ? -8 : 0; snprintf(m->name,sizeof m->name,"%s",wd_room_kind(kind,d->band));
        for(int y=y0;y<=y1;y++) for(int x=x0;x<=x1;x++){ WdCell *c=&C(d,x,y); c->kind=WDC_OPEN; c->room=(uint8_t)d->nroom; c->floor=m->floor; c->ceil=m->ceil; }
        return d->nroom++;
    }
    return -1;
}
static int in_room_ring(const WdDeck *d, int x, int y, int skip_a, int skip_b){        // inside a room or its wall ring (other than a and b)
    for(int i=1;i<d->nroom;i++){ if(i==skip_a||i==skip_b) continue; const WdRoom *o=&d->room[i]; if(x>=o->x0-1&&x<=o->x1+1&&y>=o->y0-1&&y<=o->y1+1) return 1; } return 0; }
static void door_of(const WdDeck *d, int a, int b, int *dx, int *dy, int *ox, int *oy){   // the door of room a facing room b, and the corridor cell outside it
    const WdRoom *A=&d->room[a], *B=&d->room[b]; int ax=(A->x0+A->x1)/2, ay=(A->y0+A->y1)/2, bx=(B->x0+B->x1)/2, by=(B->y0+B->y1)/2;
    int sides[4], ns=0; int ddx=bx-ax, ddy=by-ay;
    if(abs(ddx)>=abs(ddy)){ sides[ns++]= ddx>0?0:1; sides[ns++]= ddy>0?2:3; sides[ns++]= ddy>0?3:2; sides[ns++]= ddx>0?1:0; }
    else { sides[ns++]= ddy>0?2:3; sides[ns++]= ddx>0?0:1; sides[ns++]= ddx>0?1:0; sides[ns++]= ddy>0?3:2; }
    for(int k=0;k<4;k++){ int s=sides[k], x,y,px,py;
        if(s==0){ x=A->x1+1; y=(A->y0+A->y1)/2; px=x+1; py=y; } else if(s==1){ x=A->x0-1; y=(A->y0+A->y1)/2; px=x-1; py=y; }
        else if(s==2){ y=A->y1+1; x=(A->x0+A->x1)/2; px=x; py=y+1; } else { y=A->y0-1; x=(A->x0+A->x1)/2; px=x; py=y-1; }
        if(px<1||py<1||px>=WD_W-1||py>=WD_W-1) continue; if(in_room_ring(d,px,py,a,b) && !(px>=B->x0-1&&px<=B->x1+1&&py>=B->y0-1&&py<=B->y1+1)) continue;
        *dx=x; *dy=y; *ox=px; *oy=py; return; }
    *dx=A->x1+1; *dy=(A->y0+A->y1)/2; *ox=*dx+1; *oy=*dy;
}
// a corridor: cheapest path with a turn penalty (straight halls look right in first person), around every other room
static int16_t g_cost[WD_W*WD_W*4]; static int16_t g_from[WD_W*WD_W*4];
static int carve(WdDeck *d, int a, int b, int cw, int8_t ceil){
    int ax,ay,aox,aoy,bx,by,box,boy; door_of(d,a,b,&ax,&ay,&aox,&aoy); door_of(d,b,a,&bx,&by,&box,&boy);
    static const int DX[4]={1,-1,0,0}, DY[4]={0,0,1,-1};
    for(int i=0;i<WD_W*WD_W*4;i++){ g_cost[i]=32000; g_from[i]=-1; }
    // a simple bucket queue (costs are small integers)
    static int q[WD_W*WD_W*4*2]; int qn=0;
    for(int dir=0;dir<4;dir++){ int s=(aoy*WD_W+aox)*4+dir; g_cost[s]=0; q[qn++]=s; }
    int best=-1;
    for(int pass=0;pass<4000 && qn>0;pass++){
        int nq=0; static int q2[WD_W*WD_W*4*2];
        for(int k=0;k<qn;k++){ int s=q[k]; int cell=s>>2, dir=s&3, x=cell%WD_W, y=cell/WD_W; int cc=g_cost[s];
            if(x==box&&y==boy){ if(best<0||cc<g_cost[best]) best=s; continue; }
            for(int nd=0;nd<4;nd++){ int nx=x+DX[nd], ny=y+DY[nd]; if(nx<1||ny<1||nx>=WD_W-1||ny>=WD_W-1) continue;
                if(!(nx==box&&ny==boy) && in_room_ring(d,nx,ny,0,0)) continue;
                int nc=cc+1+(nd!=dir?6:0)+(C(d,nx,ny).kind!=WDC_SOLID?-1:0); int ns=(ny*WD_W+nx)*4+nd;
                if(nc<g_cost[ns]){ g_cost[ns]=(int16_t)nc; g_from[ns]=(int16_t)s; if(nq<WD_W*WD_W*8) q2[nq++]=ns; } } }
        memcpy(q,q2,sizeof(int)*nq); qn=nq;
    }
    if(best<0) return 0;
    for(int s=best;s>=0;s=g_from[s]){ int cell=s>>2, x=cell%WD_W, y=cell/WD_W, dir=s&3;
        for(int w=0;w<cw;w++){ int xx=x+(dir<2?0:w), yy=y+(dir<2?w:0); if(!inb(xx,yy)||xx<1||yy<1||xx>=WD_W-1||yy>=WD_W-1) continue; if(w>0 && in_room_ring(d,xx,yy,0,0)) continue;
            WdCell *c=&C(d,xx,yy); if(c->kind==WDC_SOLID){ c->kind=WDC_OPEN; c->room=0; c->floor=0; c->ceil=ceil; if(((x+y)%5)==0) c->flags|=WF_LIGHT; } } }
    WdCell *da=&C(d,ax,ay), *db=&C(d,bx,by); da->kind=WDC_DOOR; da->room=0; da->floor=0; da->ceil=ceil; db->kind=WDC_DOOR; db->room=0; db->floor=0; db->ceil=ceil;
    return 1;
}
static void prop(WdDeck *d, int type, float x, float y, float z, float w, float h, int a){
    if(d->nprop>=WD_MAXPROP) return; if(type==WP_BUNK||type==WP_TABLE||type==WP_BENCH||type==WP_SOFA||type==WP_CONSOLE||type==WP_SCHEMA||type==WP_KEY) h*=0.5f; WdProp *p=&d->prop[d->nprop++]; p->type=(uint8_t)type; p->x=x; p->y=y; p->z=z; p->w=w; p->h=h; p->a=(uint8_t)a;
    int cx=(int)x, cy=(int)y; p->room = inb(cx,cy) ? C(d,cx,cy).room : 0; }
static void ring_windows(WdDeck *d, int r, int side){
    WdRoom *m=&d->room[r];
    if(side==0||side==1){ int x= side==0 ? m->x1+1 : m->x0-1; for(int y=m->y0;y<=m->y1;y++) if(inb(x,y) && C(d,x,y).kind==WDC_SOLID){ C(d,x,y).kind=WDC_WINDOW; C(d,x,y).room=(uint8_t)r; C(d,x,y).ceil=m->ceil; } }
    else { int y= side==2 ? m->y1+1 : m->y0-1; for(int x=m->x0;x<=m->x1;x++) if(inb(x,y) && C(d,x,y).kind==WDC_SOLID){ C(d,x,y).kind=WDC_WINDOW; C(d,x,y).room=(uint8_t)r; C(d,x,y).ceil=m->ceil; } }
}
static int outer_side(const WdRoom *m){ int dl=m->x0, dr=WD_W-1-m->x1, dt=m->y0, db=WD_W-1-m->y1; int s=1, b=dl; if(dr<b){ b=dr; s=0; } if(dt<b){ b=dt; s=3; } if(db<b){ b=db; s=2; } return s; }
static void furnish(WdDeck *d, R *r, int ri_){
    WdRoom *m=&d->room[ri_]; float x0=m->x0, y0=m->y0, x1=m->x1+1, y1=m->y1+1, cx=(x0+x1)/2, cy=(y0+y1)/2, W=x1-x0, H=y1-y0; float hc=m->ceil;
    switch(m->kind){
    case WR_HANGAR: { int nx=(int)(W/4), ny=(int)(H/5); for(int i=0;i<nx;i++) for(int j=0;j<ny;j++) if(((i+j)&3)!=3) prop(d,WP_FIGHTER,x0+2+i*4.0f,y0+2.5f+j*5.0f,0,2.6f,1.6f,ri(r,4));
        for(int i=0;i<4;i++) prop(d,WP_CRATE,x0+0.7f,y0+1+i*1.2f,0,0.9f,0.9f,ri(r,3));
        for(float x=x0+2;x<x1-1;x+=5) prop(d,WP_LAMP,x,y0+0.5f,hc-3,0.8f,0.6f,0);
        ring_windows(d,ri_,outer_side(m)); break; }
    case WR_REACTOR: case WR_SHIELDGEN: case WR_TURRET: {
        int pit = m->kind==WR_REACTOR; int cw = d->band>=WB_VAST ? 2 : 1;
        if(pit) for(int y=m->y0+1;y<=m->y1-1;y++) for(int x=m->x0+1;x<=m->x1-1;x++){ int onx = abs(x-(int)cx)<cw || abs(x-(int)cx+1)<cw-1, ony = abs(y-(int)cy)<cw; if(onx||ony) { C(d,x,y).flags|=WF_RAIL; continue; } C(d,x,y).kind=WDC_PIT; C(d,x,y).floor=-100; }
        if(m->kind==WR_REACTOR) prop(d,WP_CORE,cx,cy,-100,cw*1.6f+1.2f,(float)hc+100,0);
        else if(m->kind==WR_SHIELDGEN) prop(d,WP_DOME,cx,cy,0,W*0.45f,hc*0.6f,0);
        else prop(d,WP_GUN,cx,cy,0,W*0.4f,hc*0.7f,0);
        for(int i=0;i<4;i++) prop(d,WP_CONSOLE,x0+0.6f+(i&1)*(W-1.2f),y0+0.6f+(i>>1)*(H-1.2f),0,0.9f,1.1f,i);
        prop(d,WP_SCHEMA,x0+0.6f,cy,0,0.8f,1.3f,0); break; }
    case WR_BRIDGE: case WR_CONTROL: { for(int row=0;row<2;row++) for(float x=x0+1.5f;x<x1-1;x+=1.6f) prop(d,WP_CONSOLE,x,y0+1.5f+row*2.2f,0,1.0f,1.0f,ri(r,4));
        if(m->kind==WR_BRIDGE){ prop(d,WP_CONSOLE,cx,y1-2,0,1.6f,1.4f,9); prop(d,WP_SCHEMA,x0+0.6f,y1-1,0,0.8f,1.3f,0); }
        ring_windows(d,ri_,3); break; }
    case WR_QUARTERS: for(float x=x0+0.9f;x<x1-0.5f;x+=1.6f){ prop(d,WP_BUNK,x,y0+0.6f,0,1.2f,1.8f,ri(r,4)); prop(d,WP_BUNK,x,y1-0.6f,0,1.2f,1.8f,ri(r,4)); } prop(d,WP_LAMP,cx,cy,hc-2,0.6f,0.5f,1); break;
    case WR_MESS: for(float x=x0+2;x<x1-1;x+=3) for(float y=y0+1.5f;y<y1-1;y+=2.5f){ prop(d,WP_TABLE,x,y,0,1.8f,0.8f,0); prop(d,WP_BENCH,x,y-0.8f,0,1.6f,0.45f,0); prop(d,WP_BENCH,x,y+0.8f,0,1.6f,0.45f,0); } prop(d,WP_LAMP,cx,cy,hc-2,0.8f,0.6f,1); break;
    case WR_LOUNGE: for(int i=0;i<3;i++) prop(d,WP_SOFA,x0+1.5f+i*2.4f,y0+1.2f,0,1.8f,0.8f,i); for(int i=0;i<3;i++) prop(d,WP_PLANT,x1-0.8f,y0+1+i*1.6f,0,0.8f,1.4f,ri(r,4)); ring_windows(d,ri_,3); break;
    case WR_GARDEN: for(float x=x0+1.2f;x<x1-0.8f;x+=2.4f) for(float y=y0+1.2f;y<y1-0.8f;y+=2.6f) prop(d,WP_PLANT,x,y,0,1.4f,0.5f+rf(r)*0.4f,4+ri(r,4)); for(float x=x0+2;x<x1;x+=4) prop(d,WP_LAMP,x,cy,hc-2,1.2f,0.4f,2); break;
    case WR_WORKSHOP: for(float x=x0+1.5f;x<x1-1;x+=2.5f){ prop(d,WP_BENCH,x,y0+1.0f,0,1.8f,0.9f,1); prop(d,WP_CRATE,x,y1-0.8f,0,0.9f,0.9f,ri(r,3)); } prop(d,WP_PIPE,x0+0.3f,cy,0,0.4f,hc,0); break;
    case WR_TRENCH: for(float t=0;t<(W>H?W:H);t+=3) prop(d,WP_PIPE, W>H ? x0+t+0.5f : x0+0.3f, W>H ? y0+0.3f : y0+t+0.5f, -8,0.35f,(float)hc+8,0); break;
    case WR_TRAM: { for(int y=m->y0;y<=m->y1;y++) for(int x=m->x0;x<=m->x1;x++){ if(y>=m->y0+(m->y1-m->y0)/2+1 && y<m->y1 && x>m->x0 && x<m->x1){ C(d,x,y).kind=WDC_PIT; C(d,x,y).floor=-6; } else if(y==m->y0+(m->y1-m->y0)/2) C(d,x,y).kind=WDC_TRAM; }
        prop(d,WP_TRAMCAR,cx,y0+H*0.78f,-6,W*0.7f,1.7f,0); for(float x=x0+1;x<x1;x+=3) prop(d,WP_LAMP,x,y0+0.5f,hc-2,0.6f,0.4f,0); break; }
    case WR_SECURITY: prop(d,WP_CONSOLE,x0+1,y0+1,0,1,1,3); prop(d,WP_CONSOLE,x0+2.2f,y0+1,0,1,1,2); prop(d,WP_CRATE,x1-1,y1-1,0,0.9f,0.9f,0); break;
    default: break;
    }
}
static void light_room(WdDeck *d, int i, int cult){
    static const uint8_t base[WR_NKIND][3]={{150,150,160},{190,195,210},{90,140,220},{120,170,220},{200,150,100},{220,190,140},{190,150,200},{170,230,150},{210,170,110},{200,80,60},{180,180,200},{120,200,255},{230,120,80},{140,190,255},{230,90,90}};
    static const uint8_t ct[WS_NCULT][3]={{255,255,255},{255,230,200},{220,230,255},{255,220,160},{230,255,230},{220,240,255},{255,240,220},{255,200,190},{255,250,220},{230,210,255},{240,230,215},{215,255,215}};
    WdRoom *m=&d->room[i]; int k=m->kind;
    m->r=(uint8_t)(base[k][0]*ct[cult][0]/255); m->g=(uint8_t)(base[k][1]*ct[cult][1]/255); m->b=(uint8_t)(base[k][2]*ct[cult][2]/255);
    if(d->band<=WB_MICRO){ m->r=(uint8_t)(m->r/2+60); m->g=(uint8_t)(m->g/3+90); m->b=(uint8_t)(m->b/2+110); }
}
static void build_light(WdDeck *d){
    for(int i=0;i<WD_W*WD_W;i++){ const WdCell *c=&d->cell[i]; const WdRoom *m=&d->room[c->room]; int dim = m->kind==WR_TRENCH||m->kind==WR_QUARTERS ? 55 : m->kind==WR_CORRIDOR ? 45 : 70;
        d->lr[i]=(uint8_t)(m->r*dim/100); d->lg[i]=(uint8_t)(m->g*dim/100); d->lb[i]=(uint8_t)(m->b*dim/100); }
    for(int y=0;y<WD_W;y++) for(int x=0;x<WD_W;x++){ const WdCell *c=&C(d,x,y); if(!(c->flags&WF_LIGHT) || c->kind==WDC_SOLID) continue;
        for(int dy=-4;dy<=4;dy++) for(int dx=-4;dx<=4;dx++){ int xx=x+dx, yy=y+dy; if(!inb(xx,yy)) continue; int j=yy*WD_W+xx; if(d->cell[j].room!=c->room && d->cell[j].kind!=WDC_DOOR) continue;
            int f=90-(dx*dx+dy*dy)*5; if(f<=0) continue; const WdRoom *m=&d->room[c->room];
            int r=d->lr[j]+m->r*f/200, g=d->lg[j]+m->g*f/200, b=d->lb[j]+m->b*f/200; d->lr[j]=(uint8_t)(r>255?255:r); d->lg[j]=(uint8_t)(g>255?255:g); d->lb[j]=(uint8_t)(b>255?255:b); } }
    for(int p=0;p<d->nprop;p++){ const WdProp *q=&d->prop[p]; int lr,lg,lb,rad;
        if(q->type==WP_LAMP){ lr=255; lg=235; lb=200; rad=6; } else if(q->type==WP_CORE){ lr=120; lg=200; lb=255; rad=9; } else if(q->type==WP_DOME){ lr=140; lg=220; lb=255; rad=8; }
        else if(q->type==WP_CONSOLE){ lr=80; lg=170; lb=255; rad=2; } else if(q->type==WP_PLANT){ lr=60; lg=120; lb=40; rad=1; } else continue;
        int x=(int)q->x, y=(int)q->y; for(int dy=-rad;dy<=rad;dy++) for(int dx=-rad;dx<=rad;dx++){ int xx=x+dx, yy=y+dy; if(!inb(xx,yy)) continue; int j=yy*WD_W+xx;
            if(d->cell[j].room!=q->room && d->cell[j].kind!=WDC_DOOR) continue; int f=100-(dx*dx+dy*dy)*100/(rad*rad+1); if(f<=0) continue;
            int r=d->lr[j]+lr*f/250, g=d->lg[j]+lg*f/250, b=d->lb[j]+lb*f/250; d->lr[j]=(uint8_t)(r>255?255:r); d->lg[j]=(uint8_t)(g>255?255:g); d->lb[j]=(uint8_t)(b>255?255:b); } }
}
static int room_of_kind(const WdDeck *d, int k){ for(int i=1;i<d->nroom;i++) if(d->room[i].kind==k) return i; return -1; }
static int free_spot(const WdDeck *d, R *r, int room, float *x, float *y){
    const WdRoom *m=&d->room[room]; for(int t=0;t<30;t++){ int cx=m->x0+ri(r,m->x1-m->x0+1), cy=m->y0+ri(r,m->y1-m->y0+1); if(C(d,cx,cy).kind==WDC_OPEN){ *x=cx+0.5f; *y=cy+0.5f; return 1; } }
    *x=(m->x0+m->x1)/2+0.5f; *y=(m->y0+m->y1)/2+0.5f; return 0; }
static void spot_near(const WdDeck *d, R *r, int type, int room_kind, int *sx, int *sy){      // a crew member's place: next to a prop of this type, or in a room of that kind
    int cand[64], n=0; for(int p=0;p<d->nprop && n<64;p++) if(d->prop[p].type==type) cand[n++]=p;
    if(n){ const WdProp *q=&d->prop[cand[ri(r,n)]]; int bx=(int)q->x, by=(int)q->y; static const int o[9][2]={{0,1},{0,-1},{1,0},{-1,0},{1,1},{-1,-1},{1,-1},{-1,1},{0,0}};
        for(int k=0;k<9;k++){ int x=bx+o[k][0], y=by+o[k][1]; if(inb(x,y) && C(d,x,y).kind==WDC_OPEN){ *sx=x; *sy=y; return; } } }
    int rm=room_of_kind(d,room_kind); if(rm<0) rm=1+ri(r,d->nroom-1); float fx,fy; free_spot(d,r,rm,&fx,&fy); *sx=(int)fx; *sy=(int)fy;
}
// ================================================================= ROOM MIX: a ship design says which rooms and props the deck has (see WdMix in ws_deck.h)
static int count_kind(const WdDeck *d, int k){ int n=0; for(int i=1;i<d->nroom;i++) if(d->room[i].kind==k) n++; return n; }
int wd_count_rooms(const WdDeck *d, int kind){ int n=0; for(int i=1;i<d->nroom;i++) if(d->room[i].kind==kind && d->room[i].sub==255) n++; return n; }
int wd_count_props(const WdDeck *d, int type){ int n=0; for(int p=0;p<d->nprop;p++){ const WdProp *q=&d->prop[p]; if(q->type==type && q->room>0 && q->room<d->nroom && d->room[q->room].sub==255) n++; } return n; }
static int mix_host(int type, int kind){          // which rooms a mix prop lives in
    switch(type){ case WP_BUNK: return kind==WR_QUARTERS; case WP_FIGHTER: return kind==WR_HANGAR; case WP_GUN: return kind==WR_TURRET;
        case WP_CRATE: return kind==WR_HANGAR || kind==WR_WORKSHOP || kind==WR_SECURITY; default: return 0; } }
static float g_mixk=1;      // prop scale of the deck being made: on a small craft the bunks, crates and fighters are packed closer
static int mix_slot(const WdRoom *m, int type, int idx, float *x, float *y, float *w, float *h){      // the idx-th place for a bunk, fighter or crate in a room; 0 when the room is full
    float x0=(float)m->x0, y0=(float)m->y0, x1=(float)m->x1+1, y1=(float)m->y1+1, ox, oy, sx, sy, mx, my; int cm=0, maxcol=99;
    switch(type){
    case WP_BUNK:    ox=0.9f; oy=0.6f; sx=1.6f; sy=2.0f; mx=0.5f; my=0.6f; *w=1.2f; *h=1.8f; break;
    case WP_FIGHTER: ox=2.6f; oy=2.5f; sx=3.4f; sy=3.0f; mx=1.5f; my=1.2f; *w=2.6f; *h=1.6f; break;
    case WP_CRATE:   ox=0.7f; oy=0.8f; sx=1.2f; sy=1.2f; mx=0.6f; my=0.6f; *w=0.9f; *h=0.9f; cm=1; if(m->kind==WR_HANGAR) maxcol=1; break;     // crates fill from the bottom left, column by column
    default: return 0; }
    ox*=g_mixk; oy*=g_mixk; sx*=g_mixk; sy*=g_mixk; mx*=g_mixk; my*=g_mixk; *w*=g_mixk; *h*=g_mixk; if(type==WP_CRATE && m->kind==WR_HANGAR && g_mixk<1) maxcol=2;
    int nc=0, nr=0; for(float xx=x0+ox; xx<x1-mx && nc<maxcol; xx+=sx) nc++; for(float yy=y0+oy; yy<y1-my; yy+=sy) nr++;
    if(nc<1 || nr<1 || idx<0 || idx>=nc*nr) return 0;
    int col = cm ? idx/nr : idx%nc, row = cm ? idx%nr : idx/nc;
    *x = x0+ox+col*sx; *y = cm ? y1-oy-row*sy : y0+oy+row*sy; return 1; }
static void mix_fill(WdDeck *d, int type, int target){        // make exactly `target` props of this type in the (non-target) rooms that host them, as many as the floor takes
    int host[WD_MAXROOM], nh=0; for(int i=1;i<d->nroom;i++) if(d->room[i].sub==255 && mix_host(type,d->room[i].kind)) host[nh++]=i;
    { int k=0; for(int p=0;p<d->nprop;p++){ const WdProp *q=&d->prop[p]; int rm=q->room; if(q->type==type && rm>0 && rm<d->nroom && d->room[rm].sub==255 && mix_host(type,d->room[rm].kind)) continue; d->prop[k++]=d->prop[p]; } d->nprop=k; }
    if(!nh || target<=0) return;
    if(type==WP_GUN){ for(int hi=0;hi<nh;hi++){ const WdRoom *m=&d->room[host[hi]]; int n=target/nh+(hi<target%nh?1:0); float W=(float)(m->x1+1-m->x0), H=(float)(m->y1+1-m->y0), cx=m->x0+W*0.5f, cy=m->y0+H*0.5f, sm=W<H?W:H;
            for(int k=0;k<n;k++){ if(n<=1) prop(d,WP_GUN,cx,cy,0,W*0.4f,m->ceil*0.7f,0); else { float a=6.2831853f*k/n+0.4f; prop(d,WP_GUN,cx+cosf(a)*sm*0.28f,cy+sinf(a)*sm*0.28f,0,sm*0.26f,m->ceil*0.5f,0); } } }
        return; }
    int used[WD_MAXROOM]={0};
    for(int k=0;k<target;k++){ int placed=0; for(int t=0;t<nh && !placed;t++){ int hi=(k+t)%nh; float x,y,w,h; if(mix_slot(&d->room[host[hi]],type,used[hi],&x,&y,&w,&h)){ prop(d,type,x,y,0,w,h,type==WP_FIGHTER?(hi+k)&3:k%3); used[hi]++; placed=1; } } if(!placed) break; }
}
static int mix_cap(const WdDeck *d, int type){      // how many of this prop the free (non-target) rooms can hold
    int n=0; float x,y,w,h; for(int i=1;i<d->nroom;i++) if(d->room[i].sub==255 && mix_host(type,d->room[i].kind)){ int k=0; while(mix_slot(&d->room[i],type,k,&x,&y,&w,&h)) k++; n+=k; } return n; }
static void mix_rooms(WdDeck *d, R *r, const WdMix *x){      // the rooms the design has (the craft's own target rooms count for reactor, bridge and shield generator)
    int nt = x->turret>6 ? 6 : x->turret; if(!nt && x->guns) nt=1;
    int nh = (x->hangar||x->fighters) ? (x->fighters+9)/10 : 0; if((x->hangar||x->fighters) && nh<1) nh=1; if(nh>3) nh=3;
    int nq = (x->quarters||x->bunks) ? (x->bunks+11)/12 : 0; if((x->quarters||x->bunks) && nq<1) nq=1; if(nq>4) nq=4;
    // the rooms that hold props go first (they are the ones the design cannot do without), then the rest
    struct { int k, n, own; } plan[8]={ {WR_HANGAR,nh,1}, {WR_QUARTERS,nq,1}, {WR_TURRET,nt,1}, {WR_REACTOR,x->reactor?1:0,0}, {WR_BRIDGE,x->bridge?1:0,0}, {WR_SHIELDGEN,x->shieldgen?1:0,0}, {WR_WORKSHOP,x->workshop?1:0,0}, {WR_MESS,x->mess?1:0,0} };
    for(int i=0;i<8;i++){ int have = plan[i].own ? wd_count_rooms(d,plan[i].k) : count_kind(d,plan[i].k); for(int n=have;n<plan[i].n;n++) if(add_room(d,r,plan[i].k,255)<0) break; }
    // small decks hold fewer: add hangars and quarters until the floor takes the fighters and bunks (a few more than the plan, never past 6 of a kind)
    while(x->fighters && mix_cap(d,WP_FIGHTER)<x->fighters && wd_count_rooms(d,WR_HANGAR)<6 && add_room(d,r,WR_HANGAR,255)>=0) ;
    while(x->bunks && mix_cap(d,WP_BUNK)<x->bunks && wd_count_rooms(d,WR_QUARTERS)<6 && add_room(d,r,WR_QUARTERS,255)>=0) ;
    if(d->nroom<5) add_room(d,r,WR_CONTROL,255);              // a deck is never just a tram stop and a security desk
    if(d->nroom<5) add_room(d,r,WR_LOUNGE,255);
}
void wd_make(WdDeck *d, const WsCraft *c, uint32_t leaf, const int *subs, int nsubs, int from_hangar, int depth){ wd_make_mix(d,c,leaf,subs,nsubs,from_hangar,depth,0); }
void wd_make_mix(WdDeck *d, const WsCraft *c, uint32_t leaf, const int *subs, int nsubs, int from_hangar, int depth, const WdMix *mix){
    g_tight = mix!=0; g_mixk = (mix && wd_band(c)<=WB_SMALL) ? 0.65f : 1.0f;
    memset(d,0,sizeof *d); R r={leaf^0xD3C4u}; d->seed=c->seed; d->leaf=leaf; d->band=wd_band(c); d->cult=c->cult; d->depth=depth;
    d->cell_m = wd_level_m(c,depth)/WD_W; d->nsubs=nsubs; for(int i=0;i<nsubs;i++) d->subs[i]=subs[i];
    d->clock = 6+ (leaf%12);
    for(int i=0;i<WD_W*WD_W;i++){ d->cell[i].kind=WDC_SOLID; d->cell[i].wtex=(uint8_t)(leaf%4); }
    WdRoom *cor=&d->room[0]; memset(cor,0,sizeof *cor); cor->kind=WR_CORRIDOR; cor->x1=WD_W-1; cor->y1=WD_W-1; cor->sub=255; cor->ceil=scale_ceil(rk_ceil[WR_CORRIDOR],d->band); snprintf(cor->name,sizeof cor->name,"%s",wd_room_kind(WR_CORRIDOR,d->band)); d->nroom=1;
    int tram=add_room(d,&r,WR_TRAM,255); d->tram_room=tram;
    int hang_entry=-1;
    for(int i=0;i<nsubs;i++){ int k=c->sub[subs[i]].kind; int rk = k==WSS_GEN?WR_SHIELDGEN : k==WSS_REACTOR?WR_REACTOR : k==WSS_HANGAR?WR_HANGAR : k==WSS_TURRET?WR_TURRET : WR_BRIDGE;
        int rm=add_room(d,&r,rk,subs[i]); if(rm>0){ d->room[rm].ceil = d->room[rm].ceil; if(rk==WR_HANGAR && hang_entry<0) hang_entry=rm; } }
    if(from_hangar && hang_entry<0) hang_entry=add_room(d,&r,WR_HANGAR,255);
    int sec=add_room(d,&r,WR_SECURITY,255);
    static const uint8_t fill[10]={WR_QUARTERS,WR_MESS,WR_LOUNGE,WR_GARDEN,WR_WORKSHOP,WR_QUARTERS,WR_CONTROL,WR_TRENCH,WR_WORKSHOP,WR_QUARTERS};
    int maxr = d->band<=WB_SMALL ? 9 : d->band==WB_NORMAL ? 14 : 16;
    if(mix) mix_rooms(d,&r,mix);                                      // a design's room mix replaces the random fill
    else for(int i=0;i<16 && d->nroom<maxr;i++){ int k = i<10 ? fill[(i+(int)(leaf%3))%10] : fill[ri(&r,10)]; add_room(d,&r,k,255); }
    // ---- corridors: a tree (each room joins the nearest one already joined) plus a few loops; target rooms with one way in can be locked
    int cw = d->band<=WB_SMALL ? 1 : d->band==WB_NORMAL ? 2 : 3; int8_t cc=scale_ceil(rk_ceil[WR_CORRIDOR],d->band);
    int joined[WD_MAXROOM]={0}, deg[WD_MAXROOM]={0}; joined[1]=1;
    for(int n=2;n<d->nroom;n++){ int ba=-1,bb=-1,bd=1<<30;
        for(int a=1;a<d->nroom;a++) if(joined[a]) for(int b=1;b<d->nroom;b++) if(!joined[b]){ const WdRoom *A=&d->room[a],*B=&d->room[b]; int dx=(A->x0+A->x1)-(B->x0+B->x1), dy=(A->y0+A->y1)-(B->y0+B->y1); int dd=dx*dx+dy*dy; if(dd<bd){ bd=dd; ba=a; bb=b; } }
        if(bb<0) break; joined[bb]=1; if(carve(d,ba,bb,cw,cc)){ deg[ba]++; deg[bb]++; } }
    for(int k=0;k<2;k++){ int a=1+ri(&r,d->nroom-1), b=1+ri(&r,d->nroom-1); if(a!=b && deg[a]>1 && deg[b]>1 && carve(d,a,b,cw,cc)){ deg[a]++; deg[b]++; } }
    for(int i=1;i<d->nroom;i++) furnish(d,&r,i);
    if(mix){ mix_fill(d,WP_BUNK,mix->bunks); mix_fill(d,WP_FIGHTER,mix->fighters); mix_fill(d,WP_CRATE,mix->crates); mix_fill(d,WP_GUN,mix->guns); }
    // ---- every room must be reachable: any room a corridor could not reach gets a straight one, through whatever is in the way
    int sr0 = from_hangar && hang_entry>0 ? hang_entry : tram>0 ? tram : 1;
    for(int pass=0;pass<WD_MAXROOM;pass++){ int sx0=(d->room[sr0].x0+d->room[sr0].x1)/2, sy0=d->room[sr0].y0;
        while(!wd_walkable(d,sx0,sy0,1) && sy0<d->room[sr0].y1) sy0++;
        int cut=-1; for(int i=1;i<d->nroom;i++) if(wd_path_len(d,sx0,sy0,i,1,1)<0){ cut=i; break; } if(cut<0) break;
        int best=-1, bd=1<<30; for(int i=1;i<d->nroom;i++){ if(i==cut || wd_path_len(d,sx0,sy0,i,1,1)<0) continue; int dx=(d->room[i].x0+d->room[i].x1)-(d->room[cut].x0+d->room[cut].x1), dy=(d->room[i].y0+d->room[i].y1)-(d->room[cut].y0+d->room[cut].y1); if(dx*dx+dy*dy<bd){ bd=dx*dx+dy*dy; best=i; } }
        if(best<0) break; deg[cut]+=2; deg[best]+=2;
        int x=(d->room[cut].x0+d->room[cut].x1)/2, y=(d->room[cut].y0+d->room[cut].y1)/2, tx=(d->room[best].x0+d->room[best].x1)/2, ty=(d->room[best].y0+d->room[best].y1)/2;
        while(x!=tx || y!=ty){ if(x!=tx) x+= tx>x?1:-1; else y+= ty>y?1:-1; WdCell *cl=&C(d,x,y);
            if(cl->kind==WDC_SOLID){ int ring=0; for(int i=1;i<d->nroom;i++){ const WdRoom *o=&d->room[i]; if(x>=o->x0-1&&x<=o->x1+1&&y>=o->y0-1&&y<=o->y1+1) ring=1; } cl->kind = ring?WDC_DOOR:WDC_OPEN; cl->room=0; cl->floor=0; cl->ceil=cc; }
            else if(cl->kind==WDC_PIT){ cl->kind=WDC_OPEN; cl->floor=d->room[cl->room].floor; cl->flags|=WF_RAIL; } }
    }
    // ---- one target room with a single way in can be locked: its keycard is in SECURITY, and a crawl vent is the other way in
    int lockr=-1; for(int i=1;i<d->nroom;i++) if(d->room[i].sub!=255 && deg[i]==1 && i!=hang_entry && i!=sr0 && d->band>=WB_SMALL && (leaf>>5)%3!=0){ lockr=i; break; }
    if(lockr>0 && sec>0){ WdRoom *L=&d->room[lockr]; int saved[WD_W*WD_W/8], ns=0; static uint8_t before[WD_W*WD_W]; for(int i=0;i<WD_W*WD_W;i++) before[i]=d->cell[i].kind; (void)saved; (void)ns;
        for(int y=L->y0-1;y<=L->y1+1;y++) for(int x=L->x0-1;x<=L->x1+1;x++) if(inb(x,y) && C(d,x,y).kind==WDC_DOOR) C(d,x,y).kind=WDC_LOCK;
        int best=-1, bd=1<<30; for(int i=1;i<d->nroom;i++){ if(i==lockr) continue; int dx=(d->room[i].x0+d->room[i].x1)-(L->x0+L->x1), dy=(d->room[i].y0+d->room[i].y1)-(L->y0+L->y1); if(dx*dx+dy*dy<bd){ bd=dx*dx+dy*dy; best=i; } }
        if(best>0){ static int16_t prev[WD_W*WD_W]; static int q[WD_W*WD_W]; int h=0,t=0, goal=-1; for(int i=0;i<WD_W*WD_W;i++) prev[i]=-2;
            for(int i=0;i<WD_W*WD_W;i++) if(d->cell[i].room==best && d->cell[i].kind==WDC_OPEN){ prev[i]=-1; q[t++]=i; }
            while(h<t && goal<0){ int c=q[h++], x=c%WD_W, y=c/WD_W; static const int DX[4]={1,-1,0,0}, DY[4]={0,0,1,-1};
                for(int k=0;k<4;k++){ int nx=x+DX[k], ny=y+DY[k]; if(nx<1||ny<1||nx>=WD_W-1||ny>=WD_W-1) continue; int j=ny*WD_W+nx; if(prev[j]!=-2) continue; const WdCell *cl=&d->cell[j];
                    if(cl->room==lockr && cl->kind==WDC_OPEN){ prev[j]=(int16_t)c; goal=j; break; }
                    if(!(cl->kind==WDC_SOLID || (cl->kind==WDC_OPEN && cl->room==0))) continue; prev[j]=(int16_t)c; q[t++]=j; } }
            for(int j=goal; j>=0 && prev[j]>=0; j=prev[j]){ WdCell *cl=&d->cell[j]; if(cl->kind==WDC_SOLID){ cl->kind=WDC_VENT; cl->room=0; cl->floor=0; cl->ceil=4; } } }
        // keep the lock only if everything else is still open without the key, the keycard room too, and the vent gets you in
        int sx0=(d->room[sr0].x0+d->room[sr0].x1)/2, sy0=d->room[sr0].y0; while(!wd_walkable(d,sx0,sy0,0) && sy0<d->room[sr0].y1) sy0++;
        int ok = wd_path_len(d,sx0,sy0,lockr,0,0)<0 && wd_path_len(d,sx0,sy0,lockr,0,1)>=0; for(int i=1;i<d->nroom && ok;i++) if(i!=lockr && wd_path_len(d,sx0,sy0,i,0,0)<0) ok=0;
        if(ok){ L->locked=1; L->key=(uint8_t)(1+ri(&r,3)); d->room[sec].key=L->key; if(d->nprop<WD_MAXPROP){ const WdRoom *S=&d->room[sec]; prop(d,WP_KEY,(S->x0+S->x1+1)/2.0f,(S->y0+S->y1+1)/2.0f,0,0.5f,0.9f,L->key); } }
        else for(int i=0;i<WD_W*WD_W;i++) if(d->cell[i].kind!=before[i]){ d->cell[i].kind=before[i]; if(before[i]==WDC_SOLID){ d->cell[i].room=0; } } }
    for(int i=0;i<d->nroom;i++) light_room(d,i,d->cult);
    for(int i=0;i<WD_W*WD_W;i++){ WdCell *cl=&d->cell[i]; if(cl->kind==WDC_SOLID) continue; const WdRoom *m=&d->room[cl->room]; cl->ftex=(uint8_t)m->kind; cl->ctex=(uint8_t)m->kind; if(m->sub!=255) cl->flags|=WF_TARGET; }
    build_light(d);
    // ---- where you start: on the tram platform, or in the hangar you docked in
    int sr = from_hangar && hang_entry>0 ? hang_entry : tram>0 ? tram : 1; const WdRoom *S=&d->room[sr];
    d->sx=(S->x0+S->x1)/2; d->sy= sr==tram ? S->y0+1 : (S->y0+S->y1)/2; if(C(d,d->sx,d->sy).kind!=WDC_OPEN){ float fx,fy; free_spot(d,&r,sr,&fx,&fy); d->sx=(int)fx; d->sy=(int)fy; } d->sdir=1.5708f;
    if(from_hangar && hang_entry>0){ int bx=d->sx, by=d->sy; for(int rad=1;rad<6;rad++) for(int dy=-rad;dy<=rad;dy++) for(int dx=-rad;dx<=rad;dx++){ int x=bx+dx,y=by+dy; if(inb(x,y)&&C(d,x,y).kind==WDC_OPEN&&C(d,x,y).room==hang_entry){ int clear=1; for(int p=0;p<d->nprop;p++) if(fabsf(d->prop[p].x-(x+0.5f))<1.6f&&fabsf(d->prop[p].y-(y+0.5f))<1.2f) clear=0; if(clear){ d->sx=x; d->sy=y; goto st; } } } }
    st: ;
    // ---- the crew: each has a bunk, a seat in the mess, a place to chat and a job
    int living=0; for(int i=1;i<d->nroom;i++){ int k=d->room[i].kind; if(k==WR_QUARTERS||k==WR_MESS||k==WR_LOUNGE||k==WR_GARDEN||k==WR_WORKSHOP) living++; }
    int ncrew = d->band<=WB_SMALL ? 6+living*2 : 10+living*3; if(ncrew>WD_MAXNPC-8) ncrew=WD_MAXNPC-8;
    for(int i=0;i<ncrew;i++){ WdNpc *n=&d->npc[d->nnpc++]; memset(n,0,sizeof *n); n->kind=WN_CREW; n->alive=1; n->hp=20; n->home=(uint8_t)(i%3);
        spot_near(d,&r,WP_BUNK,WR_QUARTERS,&n->spot[WA_SLEEP][0],&n->spot[WA_SLEEP][1]); spot_near(d,&r,WP_BENCH,WR_MESS,&n->spot[WA_EAT][0],&n->spot[WA_EAT][1]);
        spot_near(d,&r,WP_SOFA,WR_LOUNGE,&n->spot[WA_CHAT][0],&n->spot[WA_CHAT][1]); { static const uint8_t jt[4]={WP_CONSOLE,WP_BENCH,WP_PLANT,WP_CRATE}; spot_near(d,&r,jt[i%4],WR_WORKSHOP,&n->spot[WA_WORK][0],&n->spot[WA_WORK][1]); }
        int a=WA_SLEEP+ri(&r,4); n->act=(uint8_t)a; n->x=n->spot[a][0]+0.5f; n->y=n->spot[a][1]+0.5f; n->tx=n->x; n->ty=n->y; n->chat=rf(&r)*10; }
    // guards: by the locked door, in each target room, in security
    for(int i=1;i<d->nroom && d->nnpc<WD_MAXNPC;i++){ const WdRoom *m=&d->room[i]; int g = m->sub!=255 ? 2 : m->kind==WR_SECURITY ? 2 : 0; if(m->locked) g++; if(i==hang_entry && from_hangar) g=0;
        for(int k=0;k<g && d->nnpc<WD_MAXNPC;k++){ WdNpc *n=&d->npc[d->nnpc++]; memset(n,0,sizeof *n); n->kind = (m->kind==WR_BRIDGE && k==0) ? WN_OFFICER : WN_GUARD; n->alive=1; n->hp=60; n->act=WA_WORK; n->home=(uint8_t)i;
            float fx,fy; free_spot(d,&r,i,&fx,&fy); n->x=fx; n->y=fy; n->tx=fx; n->ty=fy; for(int s=0;s<4;s++){ n->spot[s][0]=(int)fx; n->spot[s][1]=(int)fy; } } }
    snprintf(d->name,sizeof d->name,"%s DECK %d",syl[leaf%16],(int)((leaf>>4)%40)+1);
}

// ================================================================= walking and paths
int wd_walkable(const WdDeck *d, int x, int y, int key){ if(!inb(x,y)) return 0; int k=C(d,x,y).kind; return k==WDC_OPEN||k==WDC_DOOR||k==WDC_VENT||k==WDC_TRAM||(k==WDC_LOCK&&key); }
static int16_t bfs_d[WD_W*WD_W];
static void bfs(const WdDeck *d, int x0, int y0, int key, int vents){
    static int q[WD_W*WD_W]; int h=0,t=0; for(int i=0;i<WD_W*WD_W;i++) bfs_d[i]=-1; if(!inb(x0,y0)) return; bfs_d[y0*WD_W+x0]=0; q[t++]=y0*WD_W+x0;
    while(h<t){ int c=q[h++], x=c%WD_W, y=c/WD_W; static const int DX[4]={1,-1,0,0}, DY[4]={0,0,1,-1};
        for(int k=0;k<4;k++){ int nx=x+DX[k], ny=y+DY[k]; if(!wd_walkable(d,nx,ny,key)) continue; if(!vents && C(d,nx,ny).kind==WDC_VENT) continue; int j=ny*WD_W+nx; if(bfs_d[j]>=0) continue; bfs_d[j]=(int16_t)(bfs_d[c]+1); q[t++]=j; } }
}
int wd_path_len(const WdDeck *d, int x0, int y0, int r, int key, int vents){
    bfs(d,x0,y0,key,vents); int best=-1; for(int i=0;i<WD_W*WD_W;i++) if(bfs_d[i]>=0 && d->cell[i].room==r && d->cell[i].kind!=WDC_SOLID && d->cell[i].kind!=WDC_WINDOW && (best<0||bfs_d[i]<best)) best=bfs_d[i]; return best; }
int wd_next_step(const WdDeck *d, int x0, int y0, int tx, int ty, int key, int *nx, int *ny){
    bfs(d,tx,ty,key,1); int c0=y0*WD_W+x0; if(!inb(x0,y0)||bfs_d[c0]<0){ *nx=x0; *ny=y0; return 0; }
    static const int DX[4]={1,-1,0,0}, DY[4]={0,0,1,-1}; int best=bfs_d[c0]; *nx=x0; *ny=y0;
    for(int k=0;k<4;k++){ int x=x0+DX[k], y=y0+DY[k]; if(!inb(x,y)) continue; int v=bfs_d[y*WD_W+x]; if(v>=0 && v<best){ best=v; *nx=x; *ny=y; } } return 1; }
int wd_room_at(const WdDeck *d, float x, float y){ int cx=(int)x, cy=(int)y; if(!inb(cx,cy)) return 0; return C(d,cx,cy).room; }
static int los(const WdDeck *d, float x0, float y0, float x1, float y1){
    float dx=x1-x0, dy=y1-y0; int n=(int)(sqrtf(dx*dx+dy*dy)*3)+1; for(int i=1;i<n;i++){ float t=(float)i/n; int cx=(int)(x0+dx*t), cy=(int)(y0+dy*t); if(!inb(cx,cy)) return 0; int k=C(d,cx,cy).kind; if(k==WDC_SOLID||k==WDC_WINDOW||k==WDC_LOCK) return 0; } return 1; }

// ================================================================= the crew's day
static int want_act(const WdNpc *n, double clock){ double h=fmod(clock + n->home*8.0, 24.0);       // three shifts, eight hours apart
    return h<7 ? WA_SLEEP : h<8 ? WA_EAT : h<12 ? WA_WORK : h<13 ? WA_EAT : h<17 ? WA_WORK : h<19 ? WA_CHAT : h<20 ? WA_EAT : h<22 ? WA_CHAT : WA_SLEEP; }
void wd_tick(WdDeck *d, double dt, float px, float py, int *hurt){
    d->clock=fmod(d->clock + dt/60.0, 24.0);                             // one second here is one minute of the ship's day
    for(int i=0;i<d->nnpc;i++){ WdNpc *n=&d->npc[i]; if(!n->alive) continue; if(n->cool>0) n->cool-=(float)dt;
        float dx=px-n->x, dy=py-n->y, dist=sqrtf(dx*dx+dy*dy);
        if(n->kind!=WN_CREW){
            if(!d->alarm && dist<7 && los(d,n->x,n->y,px,py)){ n->seen=1; d->alarm=1; }
            if(d->alarm){ n->act=WA_HUNT; if(dist<10 && los(d,n->x,n->y,px,py)){ if(n->cool<=0){ n->cool=1.2f; if(hurt) *hurt += n->kind==WN_OFFICER ? 6 : 4; } n->tx=n->x; n->ty=n->y; if(dist>3){ n->tx=px; n->ty=py; } }
                else { n->tx=px; n->ty=py; } }
        } else {
            int a = d->alarm ? WA_FLEE : want_act(n,d->clock);
            if(a==WA_FLEE){ if(n->act!=WA_FLEE){ n->act=WA_FLEE; n->tx=n->spot[WA_SLEEP][0]+0.5f; n->ty=n->spot[WA_SLEEP][1]+0.5f; } }
            else if(a!=n->act && n->act!=WA_WALK){ n->act=WA_WALK; n->dir=(uint8_t)a; n->tx=n->spot[a][0]+0.5f; n->ty=n->spot[a][1]+0.5f; }
            if(n->act==WA_CHAT) n->chat+=(float)dt;
        }
        float ex=n->tx-n->x, ey=n->ty-n->y, ed=sqrtf(ex*ex+ey*ey);
        if(ed>0.15f){ int nx,ny; float sp=(n->act==WA_FLEE||n->act==WA_HUNT)?2.6f:1.2f;
            int cx=(int)n->x, cy=(int)n->y, tx=(int)n->tx, ty=(int)n->ty; float gx=n->tx, gy=n->ty;
            if(cx!=tx || cy!=ty){ if(wd_next_step(d,cx,cy,tx,ty,1,&nx,&ny)){ gx=nx+0.5f; gy=ny+0.5f; } }
            float vx=gx-n->x, vy=gy-n->y, vl=sqrtf(vx*vx+vy*vy); if(vl>0.001f){ float s=sp*(float)dt; if(s>vl) s=vl; n->x+=vx/vl*s; n->y+=vy/vl*s; } }
        else if(n->act==WA_WALK){ n->act=n->dir; }
    }
}
