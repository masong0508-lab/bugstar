// pc_ships.inc: the game's space ships through the PC ship renderer (pc/shipr). #included at the end of the generated main.c.
// build.sh puts two hooks into space.inc's draw: before the pirates, pc_sr_space() (when the renderer is on, the game's own wireframes are
// skipped and every ship, beam and explosion goes to the renderer instead), and after them pc_sr_mark() (the frame is complete; a copy of the
// page is kept, so the picture compositor knows which pixels the game drew on top later: cockpit, HUD, particles).
// It only reads the game. The sim, the random numbers and the save are untouched: same inputs, same game, whatever is drawn.
#include "../shipr/shipr.h"
extern int pc_ships_mode, pc_armada_n;           // pc_hw.c: -1 = off (the game draws its own), 0 CLASSIC, 1 NATIVE, 2 ULTRA; ships in the far battle
void pc_sr_snapshot(const volatile void *pg);    // pc_hw.c
static int pc_sr_on(void){ return pc_ships_mode >= 0; }
static uint32_t pc_sr_rgb(int idx){ u16 c=BG_PAL[idx]; int r=c&31, g=(c>>5)&31, b=(c>>10)&31; return (uint32_t)(r<<3|r>>2)<<16 | (uint32_t)(g<<3|g>>2)<<8 | (uint32_t)(b<<3|b>>2); }
static s16 pcsr_hp[NPIR]; static s8 pcsr_alive[NPIR]; static float pcsr_hit[NPIR];
typedef struct { float p[3]; int t0; u32 seed; } PcBoom; static PcBoom pcsr_b[48]; static int pcsr_nb;
static void pc_sr_space(volatile u16 *pg){
    (void)pg;
    float eye[3]={ (float)(shx+cam_o[0]), (float)(shy+cam_o[1]), (float)(shz+cam_o[2]) }, r[3], u[3], f[3], sn[3]={ sun[0], sun[1], sun[2] };
    for(int k=0;k<3;k++){ r[k]=br[k]/16384.0f; u[k]=bu[k]/16384.0f; f[k]=bf[k]/16384.0f; }
    sr_begin(pc_ships_mode, eye, r, u, f, FOCS, 120, VCY, 128, sn);
    uint32_t cf=pc_sr_rgb(52), ce=pc_sr_rgb(53);
    for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i];
        if(pcsr_alive[i] && !p->alive && pcsr_nb<48){ PcBoom *b=&pcsr_b[pcsr_nb++]; b->p[0]=p->x; b->p[1]=p->y; b->p[2]=p->z; b->t0=frames; b->seed=(u32)i*7919u+(u32)frames; }   // it just died
        if(p->alive && pcsr_alive[i] && p->hp<pcsr_hp[i]) pcsr_hit[i]=1;                                                   // it was just hit: the shield shines
        pcsr_alive[i]=p->alive; pcsr_hp[i]=p->hp; if(pcsr_hit[i]>0) pcsr_hit[i]-=0.07f;
        if(!p->alive) continue;
        SrShip s; memset(&s,0,sizeof s); s.p[0]=p->x; s.p[1]=p->y; s.p[2]=p->z;
        float fx=p->f[0]/16384.0f, fy=p->f[1]/16384.0f, fz=p->f[2]/16384.0f; s.f[0]=fx; s.f[1]=fy; s.f[2]=fz;
        float Rx=fz, Rz=-fx, rl=sqrtf(Rx*Rx+Rz*Rz); if(rl<1e-4f){ Rx=1; Rz=0; rl=1; } Rx/=rl; Rz/=rl;                   // (the game's own sideways and up for this ship)
        s.u[0]=fy*Rz; s.u[1]=fz*Rx-fx*Rz; s.u[2]=-fy*Rx;
        s.size=110; s.kind = p->team ? SR_FIGHTER : SR_WING; s.team = p->team ? 1 : 0; s.col = p->team ? ce : cf; s.firing = p->fire_t>0; s.hit = pcsr_hit[i]>0 ? pcsr_hit[i] : 0; s.thrust=0.75f;
        sr_ship(&s);
        if(p->fire_t){ float t[3];
            if(p->tgt==-1){ for(int k=0;k<3;k++) t[k]=eye[k]+f[k]*60-u[k]*30+r[k]*(((frames&7)-4)*4); sr_beam(s.p,t,s.col,1); }
            else if(p->tgt>=0 && pir[p->tgt].alive){ t[0]=pir[p->tgt].x; t[1]=pir[p->tgt].y; t[2]=pir[p->tgt].z; sr_beam(s.p,t,s.col,1); } } }
    for(int i=0;i<pcsr_nb;i++){ float t=(frames-pcsr_b[i].t0)/45.0f; if(t>=1 || t<0){ pcsr_b[i]=pcsr_b[--pcsr_nb]; i--; continue; } sr_boom(pcsr_b[i].p,160,t,pcsr_b[i].seed); }
    if(battle_on && pc_armada_n>0) sr_armada(pc_armada_n, sys_id(cur_sys)^0xA4A7u, frames, 14000);                          // the far battle around yours (only drawn)
    sr_run_hooks();                                                                                                     // (more ships: warships and anything else that registered)
}
static int pc_sr_tag; static void pc_sr_mark(volatile u16 *pg){ pc_sr_tag=((u32)(uintptr_t)pg)>=0x0600A000u ? 1 : 0; sr_commit(pc_sr_tag); pc_sr_snapshot(pg); }
// the player's own ship in the chase view: the game's own model (shv/shf/shn) with the same turn, scale and light as ship_draw, but each face is
// drawn at the window's resolution (no 120-column grid, no checkerboard dither: the two shades are mixed)
static uint32_t pc_sr_mixc(uint32_t a, uint32_t b){ return ((a>>1)&0x7F7F7F)+((b>>1)&0x7F7F7F); }
static void pc_sr_player(int cx, int cy, int sc, int yaw_, int pitch_){
    ship_ramps(); float rc=cosx(ship_roll>>2)/16384.0f, rs=sinx(ship_roll>>2)/16384.0f, cyw=cosx(yaw_>>2)/16384.0f, syw=sinx(yaw_>>2)/16384.0f, cp=cosx(pitch_>>2)/16384.0f, sp=sinx(pitch_>>2)/16384.0f;
    static float px[SV_MAX], py[SV_MAX], pz[SV_MAX];
    for(int i=0;i<snv;i++){ float x=shv[i*3], y=shv[i*3+1], z=shv[i*3+2];
        float rx=x*cyw+z*syw, rz=-x*syw+z*cyw, ry=y*cp+rz*sp; rz=-y*sp+rz*cp;
        float d=400+rz; if(d<60) d=60; float f=sc*25600.0f/d/16384.0f, X=rx*f, Y=-ry*f;
        float X2=X*rc-Y*rs, Y2=X*rs+Y*rc; px[i]=X2+cx; py[i]=Y2+cy; pz[i]=rz; }
    for(int fi=0;fi<snf;fi++){ const Face *F=&shf[fi]; float zs=0; for(int k=0;k<F->n;k++) zs+=pz[F->v[k]]; zs/=F->n;
        int a=F->v[0], b=F->v[1], c=F->v[2]; float area=(px[b]-px[a])*(py[c]-py[a])-(px[c]-px[a])*(py[b]-py[a]);
        float mx=shn[fi*3], my=shn[fi*3+1], mz=shn[fi*3+2]; float nx=mx*cyw+mz*syw, nz0=-mx*syw+mz*cyw, ny=my*cp+nz0*sp, nz=-my*sp+nz0*cp;
        int dot=(int)((-nx*5+ny*11-nz*8)/256); if(area>=0) dot=-dot;
        if(!(F->two&1) && area>=0 && 0) continue;
        int base, L; if(F->grp==2){ base=SHP_GLOW; L=20+((frames>>2)&1)*8; }
        else { L=11+dot+(dot>>3); if(dot>9) L+=(dot-9)*2; L-=(int)zs>>5;
            if(F->grp==3){ base=SHP_GLOW; L+=6; } else { int g=F->grp; if(shpx.scheme==SC_TWO){ if(!g && (F->two&2)) g=1; } else if(shpx.scheme==SC_INVERT) g^=1; else if(shpx.scheme==SC_MONO) g=0; base = g ? SHP_TRIM : SHP_PAINT; }
            if(L<0) L=0; if(L>28) L=28; }
        int lv=(L+1)>>1, sh=lv>>1; int half=(lv&1); if(sh>=7){ sh=7; half=0; }
        uint32_t col=pc_sr_rgb(base+sh); if(half) col=pc_sr_mixc(col,pc_sr_rgb(base+sh+1));
        float X[8], Y[8]; for(int k=0;k<F->n;k++){ X[k]=px[F->v[k]]; Y[k]=py[F->v[k]]; }
        sr_poly2d(X,Y,F->n,2.0f+zs/1000.0f,col); }
    if(sh_flame) for(int e=0;e<sh_en;e++){ float ex=sh_eng[e][0], ey=sh_eng[e][1], ez=sh_eng[e][2]-8;          // the engines: a hot glow at each nozzle
        float rx=ex*cyw+ez*syw, rz=-ex*syw+ez*cyw, ry=ey*cp+rz*sp; rz=-ey*sp+rz*cp; float d=400+rz; if(d<60) d=60; float f=sc*25600.0f/d/16384.0f, X=rx*f, Y=-ry*f;
        float gx=X*rc-Y*rs+cx, gy=X*rs+Y*rc+cy, r=sh_eng[e][3]*f*0.9f+1; sr_glow2d(gx,gy,r*(0.8f+0.25f*sh_flame),pc_sr_rgb(SHP_GLOW+5),0.8f); sr_glow2d(gx,gy,r*0.45f,0xFFFFFF,0.6f); }
    sr_commit(pc_sr_tag);                                                                                          // (this comes after the frame was marked: put it into the frame on show)
}
