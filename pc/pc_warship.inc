// pc_warship.inc: COLOSSAL WARSHIPS in the PC build (#included at the end of the generated main.c). The rules live in pc/warship (plain C, tested);
// this file is the glue: the WARSHIPS screen in the PC LAB, the scanner in space (hold R, press SELECT), the fight in the game's own space combat
// (the lair code path: in_lair = 1 while a warship fight is on), BOARDING and the interiors (a PC-drawn 3D view under the game's HUD), the COST
// BREAKDOWN after a kill, wide gold (a vault beside the game's 64-bit gold) and the save file (<save>.ws<slot>). The GBA game never sees any of it.
#include "warship/ws_fight.h"
#include "warship/ws_view.h"
#include "warship/ms.h"
#include "warship/ws_mix_sy.h"
static void pcm_reset(void), pcm_save(FILE *f), pcm_on_win(WsPay *p), pcm_on_sab(int s), pcm_collect(void), pcm_fight_tick(void), pc_ms_board(volatile u16 *pg, int place);
static int pcm_load_line(const char *ln), pcm_contact(int i, u32 *seed, int *z), pcm_obj_kind(void), pcm_enc_mode, pcm_escort; static void pcm_dwell(int target_room, const char **prompt, char *msg, int *msg_t);
static int pcm_obj_done(void), pcm_auto; static void pcm_mp_start(const char *mode), pcm_mp_tick(void); static u32 pcm_test_setup(int z); static int pcm_cf, pcm_enc_force, pcm_clk, pcm_match_k(const WsCraft *c); static char pcm_note[40]; static unsigned char pcm_mark[8];
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define cosf __builtin_cosf
#define sinf __builtin_sinf
#define sqrtf __builtin_sqrtf
#define atan2f __builtin_atan2f
#define fmod __builtin_fmod
#define floor __builtin_floor
#define log10 __builtin_log10
#define fabsf __builtin_fabsf

static int pc_ws_on=0;                                        // a warship fight is on (in_lair is 1 as well)
EW static WsFight pcw_f;
static int pcw_req=0, pcw_req_z=0, pcw_req_cheat=0; static u32 pcw_req_seed=0;    // ENGAGE was picked on a planet: the craft waits in orbit
static WN pcw_vault; static double pcw_dust=0; static u32 pcw_kills=0; static WN pcw_best; static int pcw_kits=0, pcw_ammo[WW_N]={-1,0,0,0,0}, pcw_weapon=0, pcw_cheated=0;
static int pcw_left_at=-1, pcw_board_ok=0, pcw_beam_t=0, pcw_target=-1; static double pcw_acc=0; static char pcw_dtxt[24]; static int pcw_dtxt_t=0;
static WsPay pcw_last; static int pcw_last_ok=0;
#define PCW_DS 2000                                            // how big the craft is drawn in space (world units): its true size is in the HUD
static int pc_ws_lim(int def){ return pc_ws_on ? PCW_DS*55/100 : def; }
static int pc_ws_danger(void){ int d=4+pcw_f.c.cls; return d>24?24:d; }

// ---------------------------------------------------------------- wide gold: the game's 64-bit gold first, the rest in the vault; tiny sums collect as dust
static void pcw_topup(void){ if(wn_is_zero(pcw_vault) || gold>=(1LL<<62)) return; double room=(double)((1LL<<62)-gold); WN r=wn(room);
    if(wn_cmp(pcw_vault,r)<=0){ gold+=(s64)wn_to_d(pcw_vault); pcw_vault=wn(0); } else { gold+=(s64)room; pcw_vault=wn_sub(pcw_vault,r); } }
static void pcw_pay_(WN g);
static void pcw_pay(WN g){ long long g0=(long long)gold; double d0=pcw_dust; pcw_pay_(g);
    if(pcm_auto) fprintf(stderr,"mp %ld pay reward=%.17g,%lld gold %lld -> %lld dust %.6f -> %.6f vault=%.15g,%lld\n",(long)pc_frames(),g.m,(long long)g.e,g0,(long long)gold,d0,pcw_dust,pcw_vault.m,(long long)pcw_vault.e); }
static void pcw_pay_(WN g){
    if(wn_is_zero(g)) return;
    if(g.e<0){ pcw_dust+=wn_to_d(g); if(pcw_dust>=1){ s64 w=(s64)pcw_dust; gold=sadd(gold,w); pcw_dust-=(double)w; } return; }
    if(g.e<15){ double v=wn_to_d(g); s64 w=(s64)v; pcw_dust+=v-(double)w; gold=sadd(gold,w); if(pcw_dust>=1){ s64 k=(s64)pcw_dust; gold=sadd(gold,k); pcw_dust-=(double)k; } }
    else pcw_vault=wn_add(pcw_vault,g);
    pcw_topup();
}
static void pcw_wallet(char *b, int cap){ WN t=wn_add(pcw_vault,wn((double)gold)); wn_text(t,b,cap,0); }

// ---------------------------------------------------------------- the save file
static void pcw_path(int s, char *o, int n){ const char *sv=pc_sav_path(); if(!sv){ o[0]=0; return; } snprintf(o,n,"%s.ws%d",sv,s); }
static void pcw_defaults(void){ pcw_vault=wn(0); pcw_dust=0; pcw_kills=0; pcw_best=wn(0); pcw_kits=0; pcw_ammo[0]=-1; for(int i=1;i<WW_N;i++) pcw_ammo[i]=0; pcw_weapon=0; pcw_cheated=0; pc_ws_on=0; pcw_left_at=-1; pcw_req=0; pcw_last_ok=0; pcm_reset(); }
static void pc_ws_slot_save(int s){ char p[640]; pcw_path(s,p,sizeof p); if(!p[0]) return; FILE *f=fopen(p,"w"); if(!f) return;
    fprintf(f,"vault=%.15g %lld\ndust=%.17g\nkills=%u\nbest=%.15g %lld\nkits=%d\nammo=%d %d %d %d %d\ncheated=%d\n",pcw_vault.m,(long long)pcw_vault.e,pcw_dust,pcw_kills,pcw_best.m,(long long)pcw_best.e,pcw_kits,pcw_ammo[0],pcw_ammo[1],pcw_ammo[2],pcw_ammo[3],pcw_ammo[4],pcw_cheated); pcm_save(f); fclose(f); }
static void pc_ws_slot_load(int s){ char p[640]; pcw_defaults(); pcw_path(s,p,sizeof p); if(!p[0]) return; FILE *f=fopen(p,"r"); if(!f) return; char ln[256];
    while(fgets(ln,sizeof ln,f)){ double m; long long e; int a[5];
        if(sscanf(ln,"vault=%lf %lld",&m,&e)==2){ pcw_vault.m=m; pcw_vault.e=e; } else if(sscanf(ln,"dust=%lf",&m)==1) pcw_dust=m; else if(sscanf(ln,"kills=%u",&pcw_kills)==1){}
        else if(sscanf(ln,"best=%lf %lld",&m,&e)==2){ pcw_best.m=m; pcw_best.e=e; } else if(sscanf(ln,"kits=%d",&pcw_kits)==1){}
        else if(sscanf(ln,"ammo=%d %d %d %d %d",&a[0],&a[1],&a[2],&a[3],&a[4])==5){ for(int i=0;i<5;i++) pcw_ammo[i]=a[i]; } else if(sscanf(ln,"cheated=%d",&pcw_cheated)==1){} else pcm_load_line(ln); }
    fclose(f); }
static void pc_ws_slot_new(int s){ char p[640]; pcw_defaults(); pcw_path(s,p,sizeof p); if(p[0]) remove(p); }

// ---------------------------------------------------------------- contacts: the colossal craft near this star
static int pcw_zcap(void){ int z=300+u_far*40+(ant_king?700:0)+beast_kills*300+(int)(level>100000?2000:level/50); return z>WS_ZMAX_FAIR?WS_ZMAX_FAIR:z; }
static void pcw_contact(int i, u32 *seed, int *z){
    u32 h=hmix(sys_id(cur_sys)*0x9E3779B1u ^ (u32)(i+1)*0x632BE5ABu ^ uni_w[0] ^ 0x5717u); *seed=h; { int mc=(i<8)?pcm_contact(i,seed,z):0; if(i<8) pcm_mark[i]=(unsigned char)(mc!=0); if(mc){ pcm_cf=mc-1; return; } pcm_cf=0; }
    if(i==0){ *seed=0; *z=WS_ZREF; return; }                                                      // THE GREY MOON, the reference station, waits everywhere
    int cap=pcw_zcap(); int lo=WS_ZMIN; int r=(int)((h>>8)%1000u); int zz=lo+(int)((u64)r*(u64)(cap-lo)/1000u);
    if(zz>2600 && (h%50u)!=0) zz=2500-(int)(h%300u);                                               // universe hulks are very rare
    *z=zz; }
static void pcw_make(WsCraft *c, u32 seed, int z, int cheat){ if(seed==0 && z==WS_ZREF && !cheat) ws_make_ref(c); else ws_make(c,seed,z,cheat); }
static WsShip pcw_ship(void){ WsShip s; memset(&s,0,sizeof s); s.attack=ws_attack((double)level,laser_lv,ship_gun(),tech_lv); s.hull_max=100; s.shield_max=shield_max(); s.armour=ship_armour();
    s.fuel_max=fuel_max(); s.wing=wing_n+pcm_escort; s.kits=pcw_kits; for(int i=0;i<WW_N;i++) s.ammo[i]=pcw_ammo[i]; return s; }
static int pcw_ehp(void){ int a=ship_armour(); if(a<10) a=10; return (100+shield_max())*100/a; }

// ---------------------------------------------------------------- drawing the craft in space: its hull grammar as a turning wireframe
static void pcw_rot(float *x, float *y, float *z){ float a=frames*0.004f, b=0.35f; float c=cosf(a), s=sinf(a); float nx=*x*c-*z*s, nz=*x*s+*z*c; *x=nx; *z=nz; float cy=cosf(b), sy=sinf(b); float ny=*y*cy-*z*sy; nz=*y*sy+*z*cy; *y=ny; *z=nz; }
static int pcw_proj(float x, float y, float z, int *sx, int *sy){ pcw_rot(&x,&y,&z); s32 zz; return sproj((s32)(x*PCW_DS),(s32)(y*PCW_DS),(s32)(z*PCW_DS),sx,sy,&zz); }
static void pcw_seg(volatile u16 *pg, float x0, float y0, float z0, float x1, float y1, float z1, int c){ int a,b,cx,cy; if(pcw_proj(x0,y0,z0,&a,&b) && pcw_proj(x1,y1,z1,&cx,&cy)) line2(pg,a,b,cx,cy,c); }
static void pcw_circle(volatile u16 *pg, float cx, float cy, float cz, float r, int axis, int n, int c){
    float px=0,py=0,pz=0; for(int k=0;k<=n;k++){ float a=6.2831853f*k/n, u=cosf(a)*r, v=sinf(a)*r; float x=cx,y=cy,z=cz; if(axis==0){ y+=u; z+=v; } else if(axis==1){ x+=u; z+=v; } else { x+=u; y+=v; }
        if(k) pcw_seg(pg,px,py,pz,x,y,z,c); px=x; py=y; pz=z; } }
static void pcw_subpos(const WsCraft *c, int i, float *x, float *y, float *z){ u32 h=hmix(c->seed*31u+(u32)i*0x9E3779B1u+7u); float a=(h&1023)*6.2831853f/1024.0f, zz=((int)((h>>10)&1023)-512)/1400.0f;
    float r = c->shape==WS_SPHERE ? 0.5f*sqrtf(1-4*zz*zz) : 0.32f; *x=cosf(a)*r; *y=sinf(a)*r; *z=zz; }
static void pcw_draw(volatile u16 *pg){
    const WsCraft *c=&pcw_f.c; int hullc = pcw_f.phase==4 ? ((frames&8)?53:51) : 56;
    for(int i=0;i<c->nprim;i++){ const WsPrim *p=&c->prim[i]; float x=p->x, y=p->y, z=p->z;
        switch(p->kind){
        case 0: pcw_circle(pg,x,y,z,p->a,2,14,hullc); pcw_circle(pg,x,y,z,p->a,0,14,hullc); pcw_circle(pg,x,y,z,p->a,1,10,54); break;
        case 1: { float a=p->a,b=p->b,cc=p->c; for(int k=0;k<12;k++){ static const s8 e[12][6]={{-1,-1,-1,1,-1,-1},{-1,1,-1,1,1,-1},{-1,-1,1,1,-1,1},{-1,1,1,1,1,1},{-1,-1,-1,-1,1,-1},{1,-1,-1,1,1,-1},{-1,-1,1,-1,1,1},{1,-1,1,1,1,1},{-1,-1,-1,-1,-1,1},{1,-1,-1,1,-1,1},{-1,1,-1,-1,1,1},{1,1,-1,1,1,1}};
                  pcw_seg(pg,x+e[k][0]*a,y+e[k][1]*b,z+e[k][2]*cc,x+e[k][3]*a,y+e[k][4]*b,z+e[k][5]*cc,hullc); } break; }
        case 2: pcw_circle(pg,x,y,z,p->a,p->pitch>1?1:2,20,hullc); break;
        case 3: pcw_circle(pg,x,y,z-p->c*0.5f,p->a,2,10,hullc); for(int k=0;k<4;k++){ float a=k*1.5708f; pcw_seg(pg,x+cosf(a)*p->a,y+sinf(a)*p->a,z-p->c*0.5f,x,y,z+p->c*0.5f,hullc); } break;
        default: pcw_circle(pg,x,y,z-p->c,p->a,2,10,hullc); pcw_circle(pg,x,y,z+p->c,p->a,2,10,hullc); for(int k=0;k<4;k++){ float a=k*1.5708f; pcw_seg(pg,x+cosf(a)*p->a,y+sinf(a)*p->a,z-p->c,x+cosf(a)*p->a,y+sinf(a)*p->a,z+p->c,hullc); } break; } }
    static const u8 sc[WSS_NKIND]={55,51,52,53,50};
    for(int i=0;i<c->nsub;i++){ float x,y,z; pcw_subpos(c,i,&x,&y,&z); int sx,sy; if(!pcw_proj(x,y,z,&sx,&sy)) continue; const WsSub *s=&c->sub[i];
        int col = !s->alive ? 57 : s->sec==pcw_f.sec ? sc[s->kind] : 56; int r = i==pcw_target ? 5 : 3;
        line2(pg,sx-r,sy,sx,sy-r,col); line2(pg,sx,sy-r,sx+r,sy,col); line2(pg,sx+r,sy,sx,sy+r,col); line2(pg,sx,sy+r,sx-r,sy,col); if(!s->alive){ line2(pg,sx-2,sy-2,sx+2,sy+2,57); } }
    if(pcw_f.layers_up>0){ int sx,sy; s32 zz; if(sproj(0,0,0,&sx,&sy,&zz) && zz>100){ int r=(int)(PCW_DS*62LL/100*FOCS/zz); for(int k=0;k<64;k++){ if((k+frames/4)%(7-pcw_f.layers_up>1?7-pcw_f.layers_up:1)) continue; int a=k*4;
        dot2(pg,sx+((cosx(a&255)*r)>>14),sy+((sinx(a&255)*r)>>14),55); } } }
    if(pcw_beam_t){ pcw_beam_t--; for(int i=0;i<c->nsub;i++) if(c->sub[i].alive && c->sub[i].kind==WSS_TURRET && ((i+frames/40)&1)){ float x,y,z; pcw_subpos(c,i,&x,&y,&z); int sx,sy; if(pcw_proj(x,y,z,&sx,&sy)) line2(pg,sx,sy,120+(frames&7)-4,124,53); } }
}
static void pcw_bar(volatile u16 *pg, int x, int y, int w, double frac, int c){ rect(pg,x,y,w,3,57); int k=(int)(w*frac); if(k<0) k=0; if(k>w) k=w; if(!k && frac>0) k=1; rect(pg,x,y,k,3,c); }
static double pcw_frac(WN a, WN b){ if(wn_is_zero(b)) return 0; double f=wn_to_d(wn_div(a,b)); return f<0?0:f>1?1:f; }
static void pcw_hud(volatile u16 *pg){
    const WsCraft *c=&pcw_f.c; char b[64], l[40];
    scpy(b,c->name); text(pg,4,2,b,50,1); ws_len_text(c->z,l,sizeof l); scpy(b,ws_class_name(c->cls)); scat(b,"  "); scat(b,l); if(c->cheat) scat(b," CHEAT"); text(pg,236-textlen(b)*4,2,b,c->cheat?53:54,1);
    if(pcw_f.layers_up>0){ scpy(b,"SHIELD LAYER "); ncat(b,pcw_f.layers_up); scat(b," OF "); ncat(b,c->layers); text(pg,4,10,b,55,1); wn_text(pcw_f.lay_hp,l,sizeof l,0); text(pg,236-textlen(l)*4,10,l,55,1); pcw_bar(pg,4,17,232,pcw_frac(pcw_f.lay_hp,c->hp_shield),55); }
    else if(pcw_f.sec<c->nsec){ const WsSec *s=&c->sec[pcw_f.sec]; scpy(b,s->name); scat(b," "); ncat(b,pcw_f.sec+1); scat(b,"/"); ncat(b,c->nsec); text(pg,4,10,b,pcw_f.phase==4?53:54,1);
        wn_text(s->hp,l,sizeof l,0); scat(l," HP"); text(pg,236-textlen(l)*4,10,l,54,1); pcw_bar(pg,4,17,232,pcw_frac(s->hp,s->hp_max),pcw_f.phase==4?53:51); }
    scpy(b,wf_phase_name(pcw_f.phase)); if(pcw_target>=0){ scat(b,"  TARGET: "); scat(b,ws_sub_name(c->sub[pcw_target].kind)); } text(pg,4,22,b,56,1);
    if(pcw_dtxt_t){ pcw_dtxt_t--; text(pg,120-textlen(pcw_dtxt)*2,36,pcw_dtxt,50,1); }
    if(pcw_f.msg_t>0) text(pg,120-textlen(pcw_f.msg)*2,44,pcw_f.msg,54,1);
    { int w=pcw_weapon; scpy(b,ws_wpn_name(w)); if(pcw_f.ammo[w]>=0){ scat(b," "); ncat(b,pcw_f.ammo[w]); } scat(b,"  KITS "); ncat(b,pcw_kits); text(pg,4,112,b,pcw_f.overheat?53:54,1);
      text(pg,120,112,"HEAT",56,1); pcw_bar(pg,140,113,60,pcw_f.heat/wf_heat_max(),pcw_f.overheat?53:50); }
    if(pcw_board_ok) text(pg,120-textlen("A: BOARD THROUGH A HANGAR")*2,100,"A: BOARD THROUGH A HANGAR",52,1);
    else if(!(frames&64)) text(pg,4,104,"START: WEAPON  SELECT: RETREAT  R NEAR: BOARD",56,1);
}

// ---------------------------------------------------------------- starting, leaving, winning
static void pc_ws_breakdown(volatile u16 *pg, const WsCraft *c, const WsPay *p);
static void pcw_engage(u32 seed, int z, int cheat){
    WsCraft c; pcw_make(&c,seed,z,cheat); { int mi_=pcm_match_k(&c); pcm_escort = mi_==MK_ESCORT ? 2 : 0; }
    if(c.cheat){ pcw_cheated=1; plw_set_cheated(1); }
    int resume = pcw_f.c.seed==c.seed && pcw_f.c.z==c.z && pcw_f.state==WF_FLED;
    music(4); no_ambush=1; space_enter(0); no_ambush=0; in_lair=1; beast_fight=0; ambush_t=0; pc_ws_on=1;
    if(resume){ double away = pcw_left_at>=0 ? (frames-pcw_left_at)/60.0 : 1e9; wf_away(&pcw_f,away); pcw_f.ship=pcw_ship(); for(int i=0;i<WW_N;i++) pcw_f.ammo[i]=pcw_ammo[i]; space_say(away>90?"THE CREWS REBUILT EVERYTHING":"THE CREWS PATCHED IT. GO AGAIN"); }
    else { WsShip s=pcw_ship(); wf_start(&pcw_f,&c,&s); space_say("A COLOSSUS TURNS TOWARD YOU"); }
    pcw_f.ext=1; pcw_target=-1; pcw_acc=0;
    for(int i=0;i<NPIR;i++) if(pir[i].team==1) pir[i].alive=0;
    shx=0; shy=0; shz=-(PCW_DS*4); yaw=0; pitch=0; speed=0; svx_=svy_=svz_=0;
    BG_PAL[57]=rgb15(14,16,30); BG_PAL[58]=rgb15(60,70,120);
}
static void pcw_end(int how){ if(getenv("BUGSTAR_WS_DEBUG")) fprintf(stderr,"ws end %d at %d\n",how,frames);                                            // 0 retreat, 1 won, 2 lost
    pc_ws_on=0; in_lair=0; pcw_board_ok=0; pcw_ammo[0]=-1; for(int i=1;i<WW_N;i++) if(pcw_ammo[i]>0 || pcw_f.ammo[i]>=0) pcw_ammo[i]=pcw_f.ammo[i];
    if(how==0){ pcw_f.state=WF_FLED; pcw_left_at=frames; }
    if(how==2){ wf_away(&pcw_f,1e9); pcw_f.state=WF_FLED; pcw_left_at=-1; }
    music(3); no_ambush=1; space_enter(0); no_ambush=0;
    space_say(how==0?"YOU PULL BACK. ITS CREWS START REPAIRS":how==1?"THE COLOSSUS IS GONE":"");
    do_save(); pc_ws_slot_save(cur_slot);
}
static void pcw_victory(void){
    WsPay p; ws_pay(&pcw_f.c,&pcw_f.kill,&p); pcm_on_win(&p); pcw_last=p; pcw_last_ok=1;
    pcw_pay(p.reward); pcw_kills++; if(wn_cmp(p.reward,pcw_best)>0) pcw_best=p.reward; if(p.cheat){ pcw_cheated=1; plw_set_cheated(1); }
    gain_xp(200+pcw_f.c.cls*300); fame+=20+pcw_f.c.cls*5; boom(0,0,0,40,49); sfx(9);
    volatile u16 *pg=(volatile u16*)(GBAA(0x06000000) + (page_back?0:0xA000)); WsCraft c=pcw_f.c; pcw_end(1);
    pc_ws_breakdown(pg,&c,&p); build_space_palette();
}
static void pc_ws_lost(void){ if(!pc_ws_on) return; if(getenv("BUGSTAR_WS_DEBUG")) fprintf(stderr,"ws lost at %d\n",frames); pc_ws_on=0; wf_away(&pcw_f,1e9); pcw_f.state=WF_FLED; pcw_left_at=-1; music(3); }
static int pc_ws_sel(void){ if(!pc_ws_on) return 0; pcw_end(0); return 1; }
static void pc_ws_board(void);
static void pc_ws_shot(void){                                           // the trigger (A) while fighting: the chosen weapon, at whatever the crosshair is on
    if(pcw_board_ok && HIT(K_A)){ pc_ws_board(); return; }
    int sx,sy; s32 z; if(!sproj(0,0,0,&sx,&sy,&z) || z>40000) return;
    int r=(int)((long long)PCW_DS*55/100*FOCS/z); if(r<4) r=4; int dx=sx-120, dy=sy-VCY; if(dx*dx+dy*dy>r*r) return;
    pcw_target=-1; for(int i=0;i<pcw_f.c.nsub;i++){ const WsSub *s=&pcw_f.c.sub[i]; if(!s->alive || s->sec!=pcw_f.sec) continue; float x,y,zz; pcw_subpos(&pcw_f.c,i,&x,&y,&zz); int px,py; if(pcw_proj(x,y,zz,&px,&py) && (px-120)*(px-120)+(py-VCY)*(py-VCY)<64){ pcw_target=i; break; } }
    WN before=wf_hull_left(&pcw_f); WN lay=pcw_f.lay_hp; int lu=pcw_f.layers_up;
    int k=wf_fire(&pcw_f,pcw_weapon,pcw_target); if(!k){ if(pcw_f.overheat) space_say("OVERHEAT! LET THE GUNS COOL"); else if(pcw_f.ammo[pcw_weapon]==0) space_say("OUT OF THAT AMMO: START CHANGES WEAPON"); return; }
    s32 kk=z-PCW_DS/2; boom(shx+((bf[0]*kk)>>14),shy+((bf[1]*kk)>>14),shz+((bf[2]*kk)>>14), k==2?5:2, k==2?51:56); sfx(1);
    WN d = lu>pcw_f.layers_up ? lay : lu>0 ? wn_sub(lay,pcw_f.lay_hp) : wn_sub(before,wf_hull_left(&pcw_f));
    if(k==2 && !wn_is_zero(d)){ char nb[24]; wn_text(d,nb,sizeof nb,0); scpy(pcw_dtxt,"-"); scat(pcw_dtxt,nb); pcw_dtxt_t=30; }
    if(pcw_f.state==WF_WON) pcw_victory();
}
static void pc_ws_update(void){
    if(!pc_ws_on) return; pcm_fight_tick();
    if(HIT(K_START)){ chase_cam^=1; for(int k=1;k<=WW_N;k++){ int w=(pcw_weapon+k)%WW_N; if(pcw_f.ammo[w]!=0){ pcw_weapon=w; break; } } char b[32]; scpy(b,"WEAPON: "); scat(b,ws_wpn_name(pcw_weapon)); space_say(b); }
    WsInput in; memset(&in,0,sizeof in); in.boost = speed>=30 && fuel>0; in.nofuel = fuel<=0; in.target=-1;
    pcw_f.dmg_out=0; pcw_f.spawn_req=0; wf_tick(&pcw_f,&in,1/60.0);
    pcw_acc += pcw_f.dmg_out; if(pcw_acc>=3){ int d=(int)pcw_acc; pcw_acc-=d; ship_hit(d,0); pcw_beam_t=6; }
    if(pcw_f.spawn_req>0){ int alive=team_count(1), n=pcw_f.spawn_req; if(alive+n>20) n=20-alive; if(n>0) spawn_pirates(n); }
    if(hull<35 && pcw_kits>0){ pcw_kits--; hull+=40; if(hull>100) hull=100; space_say("REPAIR KIT USED: +40 HULL"); }
    if((frames&15)==0 && fuel>0 && speed>=30) fuel-=1;                       // flying hard round a colossus burns fuel
    { s32 pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4; pcw_board_ok = pcw_f.layers_up==0 && wf_alive(&pcw_f,WSS_HANGAR)>0 && HELD(K_R) && pd<PCW_DS*9/10 && speed<20; }
    if(pcw_f.state==WF_WON) pcw_victory();
}
static int pc_ws_scan(void);
static void pc_ws_autotest(void); static void pcm_tick(void); static void pcm_test_hell(void);

// ================================================================= THE INTERIOR
EW static WdDeck pcw_deck;
static uint8_t *pcw_path_; static int pcw_D=0, pcw_lv=0; static u32 *pcw_hash_;
static float pcw_px, pcw_py, pcw_pa; static int pcw_hp, pcw_key, pcw_schem, pcw_obj=-1, pcw_in=0; static float pcw_hurt=0; static double pcw_t=0;
typedef struct { int sub; float x, y; double t; } PcwCharge; static PcwCharge pcw_ch[4]; static int pcw_nch=0;
static void pcw_ovl(uint32_t *out, int w, int h){
    WvMark m={0,0,0,0};
    if(pcw_obj>=0){ int here=0; for(int i=0;i<pcw_deck.nsubs;i++) if(pcw_deck.subs[i]==pcw_obj) here=1;
        for(int i=1;i<pcw_deck.nroom && here;i++) if(pcw_deck.room[i].sub==pcw_obj){ m.x=(pcw_deck.room[i].x0+pcw_deck.room[i].x1+1)*0.5f; m.y=(pcw_deck.room[i].y0+pcw_deck.room[i].y1+1)*0.5f; m.z=14; m.on=1; }
        if(!here && pcw_D>0 && pcw_deck.tram_room>0){ const WdRoom *t=&pcw_deck.room[pcw_deck.tram_room]; m.x=(t->x0+t->x1+1)*0.5f; m.y=t->y0+1.5f; m.z=10; m.on=1; } }
    wv_render(out,w,h,&pcw_deck,pcw_px,pcw_py,pcw_pa,pcw_t,&m,pcw_hurt);
}
// ROOM MIX (patch 18): when the ship editor's FOES option is on (allied and enemy ships built from the same parts), a boarded craft gets a foe design made from its own
// seed and that design's rooms and props (bunks, crates, guns, fighters) build the deck. Off: the builder's own random mix, exactly as before.
// Test hook: BUGSTAR_WS_MIX=1 forces it on, 0 forces it off.
static WdMix pcw_mixv;
// patch 45: WHO OWNS THE DECK. pcw_mode 0: an enemy craft (a team 1 design with FOES on, as before). 1: an ALLIED craft (a team 0 design with FOES on: friendly crew, nobody shoots, a free refit when you leave).
// 2: YOUR OWN HANGAR DECK: built from your own ship's design (pc_sy_des, the one in the ship editor), always, FOES or not. Test hook BUGSTAR_WS_MIX=1 forces the foe designs on, 0 off.
static int pcw_mode=0, pcw_req_mode=0;
static SyDesign pcw_own_design(void){ SyDesign d; if(pc_sy_on) d=pc_sy_des; else sy_default(&d); return d; }
static const WdMix *pcw_mix(void){ const char *e=getenv("BUGSTAR_WS_MIX"); int on = e ? atoi(e)!=0 : (pc_sy_on && pc_sy_foes);
    if(pcw_mode==2){ SyDesign od=pcw_own_design(); wd_mix_from_sy(&od,&pcw_mixv); return &pcw_mixv; }
    if(!on) return 0;
    SyDesign fd; int lv=ws_class_of(pcw_f.c.z)/2; if(lv>9) lv=9; if(lv<0) lv=0; sy_foe(&fd,pcw_f.c.seed,pcw_mode==1?0:1,lv); wd_mix_from_sy(&fd,&pcw_mixv); return &pcw_mixv; }
static void pcw_peace(void);
static void pcw_deck_here(int from_hangar){                           // make the deck at the end of the current path
    u32 h=wd_root(&pcw_f.c); for(int k=0;k<pcw_D;k++){ h=wd_child(h,pcw_path_[k],k); pcw_hash_[k+1]=h; }
    int subs[WS_MAXSUB]; int n=pcw_mode==2 ? 0 : wd_subs_at(&pcw_f.c,pcw_path_,pcw_D,subs); wd_make_mix(&pcw_deck,&pcw_f.c,h,subs,n,from_hangar,pcw_D,pcw_mix());
    for(int i=0;i<pcw_deck.nroom;i++){ int s=pcw_deck.room[i].sub; if(s!=255 && !pcw_f.c.sub[s].alive) pcw_deck.room[i].sub=255; }
    pcw_px=pcw_deck.sx+0.5f; pcw_py=pcw_deck.sy+0.5f; pcw_pa=pcw_deck.sdir; pcw_key=0; pcw_schem=0; pcw_nch=0; pcw_lv=pcw_D; if(pcw_mode) pcw_peace(); }
static void pcw_peace(void){                                      // an allied or your own deck: no targets, no guards, no alarm
    for(int i=0;i<pcw_deck.nroom;i++) pcw_deck.room[i].sub=255;
    for(int i=0;i<WD_W*WD_W;i++) pcw_deck.cell[i].flags&=(uint8_t)~WF_TARGET;
    for(int i=0;i<pcw_deck.nnpc;i++){ WdNpc *n=&pcw_deck.npc[i]; if(n->kind!=WN_CREW){ n->kind=WN_CREW; if(n->act==WA_HUNT||n->act==WA_FLEE) n->act=WA_WORK; } }
    pcw_deck.alarm=0; }
static void pcw_pick_obj(void){ { int mk=pcm_obj_kind(); if(mk>=0){ for(int i=0;i<pcw_f.c.nsub;i++) if(pcw_f.c.sub[i].alive && pcw_f.c.sub[i].kind==mk){ pcw_obj=i; return; } } } static const u8 pri[5]={WSS_GEN,WSS_REACTOR,WSS_BRIDGE,WSS_HANGAR,WSS_TURRET}; pcw_obj=-1;
    for(int k=0;k<5 && pcw_obj<0;k++) for(int i=0;i<pcw_f.c.nsub;i++) if(pcw_f.c.sub[i].alive && pcw_f.c.sub[i].kind==pri[k]){ pcw_obj=i; break; } }
static int pcw_obj_child(int level){ return pcw_obj>=0 ? wd_sub_child(&pcw_f.c,pcw_obj,level) : -1; }
static void pcw_text(volatile u16 *pg, int x, int y, const char *s, int c){ text(pg,x+1,y+1,s,250,1); text(pg,x,y,s,c,1); }
static void pcw_sector_map(volatile u16 *pg){                       // the tram: up and down the sectors of the craft
    int lv=pcw_D-1, sel=0, dirty=1; if(lv<0) return; pc_ovl_fn=0; wait_release();
    while(1){
        if(dirty){ dirty=0; ui_bg(pg,0,160); ui_panel(pg,6,6,228,136); char b[64], nm[48];
            ui_head(pg,12,10,216,"TRAM: THE SECTORS OF THE CRAFT",C_GOLD);
            u32 h=pcw_hash_[lv]; wd_level_name(&pcw_f.c,lv,h,nm,sizeof nm); scpy(b,"YOU ARE IN "); scat(b,nm); text(pg,12,22,b,C_WHITE,1);
            scpy(b,"LEVEL "); ncat(b,lv+1); scat(b," OF "); ncat(b,pcw_D); scat(b,"   EACH SECTOR: "); { char m[24]; double mm=wd_level_m(&pcw_f.c,lv+1); int z=(int)floor(log10(mm>0?mm:1e-300)*100); ws_len_text(z,m,sizeof m); scat(b,m); } text(pg,12,30,b,C_GREY,1);
            int oc = (pcw_obj>=0) ? pcw_obj_child(lv) : -1; int on_path=1; for(int k=0;k<lv;k++) if(pcw_path_[k]!=pcw_obj_child(k)) on_path=0;
            for(int i=0;i<8;i++){ int x=24+(i&3)*50, y=44+(i>>2)*34; u32 ch=wd_child(h,i,lv); char sn[48]; wd_level_name(&pcw_f.c,lv+1,ch,sn,sizeof sn);
                int isobj = on_path && i==oc; rect(pg,x,y,44,28, i==sel?6:57); if(isobj) { rect(pg,x,y,44,2,C_GOLD); rect(pg,x,y+26,44,2,C_GOLD); }
                char s1[16]; int k=0; while(sn[k] && sn[k]!=' ' && k<10){ s1[k]=sn[k]; k++; } s1[k]=0; text(pg,x+3,y+4,s1,i==sel?C_WHITE:C_GREY,1);
                const char *rest=strchr(sn,' '); if(rest){ char s2[16]; snprintf(s2,sizeof s2,"%s",rest+1); s2[10]=0; text(pg,x+3,y+12,s2,C_DGREY,1); }
                if(isobj) text(pg,x+3,y+20,"OBJECTIVE",C_GOLD,1); }
            scpy(b,"OBJECTIVE: "); scat(b, pcw_obj>=0 ? ws_sub_name(pcw_f.c.sub[pcw_obj].kind) : "NONE LEFT"); if(!on_path) scat(b,"  (GO UP: B)"); text(pg,12,118,b,on_path?C_GOLD:C_RED,1);
            ui_foot(pg,"ARROWS: PICK  A: RIDE DOWN  B: UP  START: EXPRESS TO OBJECTIVE","SEL: BACK"); }
        vsync(); read_keys();
        if(HIT(K_LEFT)){ sel=(sel+7)%8; dirty=1; sfx(7); } if(HIT(K_RIGHT)){ sel=(sel+1)%8; dirty=1; sfx(7); } if(HIT(K_UP)||HIT(K_DOWN)){ sel^=4; dirty=1; sfx(7); }
        if(HIT(K_SEL)){ break; }
        if(HIT(K_B) && lv>0){ lv--; dirty=1; sfx(4); }
        if(HIT(K_A)){ pcw_path_[lv]=(u8)sel; sfx(2); if(lv+1>=pcw_D){ pcw_deck_here(0); break; } pcw_hash_[lv+1]=wd_child(pcw_hash_[lv],sel,lv); lv++; sel=0; dirty=1; }
        if(HIT(K_START) && pcw_obj>=0){ int up=lv; while(up>0){ int ok=1; for(int k=0;k<up;k++) if(pcw_path_[k]!=pcw_obj_child(k)) ok=0; if(ok) break; up--; }
            for(int k=up;k<pcw_D;k++){ pcw_path_[k]=(u8)pcw_obj_child(k); pcw_hash_[k+1]=wd_child(pcw_hash_[k],pcw_path_[k],k); } sfx(6); pcw_deck_here(0); break; }
    }
    wait_release(); pc_ovl_fn=pcw_ovl;
}
static void pcw_deck_map(volatile u16 *pg){
    pc_ovl_fn=0; wait_release(); ui_bg(pg,0,160); int S=2, ox=8, oy=8;
    for(int y=0;y<WD_W;y++) for(int x=0;x<WD_W;x++){ const WdCell *c=&pcw_deck.cell[y*WD_W+x]; int col=0; switch(c->kind){ case WDC_OPEN: col = (c->flags&WF_TARGET)?C_GOLD : c->room? 6 : C_DGREY; break; case WDC_DOOR: col=C_GREEN; break; case WDC_LOCK: col=C_RED; break; case WDC_VENT: col=C_CYAN; break; case WDC_TRAM: col=C_WHITE; break; case WDC_PIT: col=57; break; case WDC_WINDOW: col=C_CYAN; break; }
        if(col) rect(pg,ox+x*S,oy+y*S,S,S,col); }
    for(int i=0;i<pcw_deck.nnpc;i++){ const WdNpc *n=&pcw_deck.npc[i]; if(n->alive && n->kind!=WN_CREW) rect(pg,ox+(int)n->x*S,oy+(int)n->y*S,S,S,C_RED); }
    rect(pg,ox+(int)pcw_px*S-1,oy+(int)pcw_py*S-1,S+2,S+2,C_WHITE);
    char b[48]; int y=10; text(pg,140,y,pcw_deck.name,C_GOLD,1); y+=10;
    for(int i=1;i<pcw_deck.nroom && y<140;i++){ const WdRoom *r=&pcw_deck.room[i]; scpy(b,r->name); b[22]=0; text(pg,140,y,b,r->sub!=255?C_GOLD:r->locked?C_RED:C_GREY,1); y+=7; }
    ui_foot(pg,"GOLD: TARGETS  RED: LOCKED  CYAN: VENTS","ANY KEY: BACK"); wait_release(); do { vsync(); read_keys(); } while(!(keys&0x3FF)); wait_release(); pc_ovl_fn=pcw_ovl; }
static int pcw_free(float x, float y){ const float pr=0.22f; for(int k=0;k<4;k++){ int cx=(int)(x+((k&1)?pr:-pr)), cy=(int)(y+((k&2)?pr:-pr)); if(!wd_walkable(&pcw_deck,cx,cy,0)) return 0; }
    for(int i=0;i<pcw_deck.nprop;i++){ const WdProp *p=&pcw_deck.prop[i]; if(p->type==WP_LAMP||p->type==WP_KEY||p->type==WP_CHARGE||p->z<-1) continue;
        float rx=p->w*0.48f, ry=p->w*0.48f; if(p->type==WP_FIGHTER){ rx=p->w*0.5f; ry=p->w*0.5f; } else if(p->type==WP_BUNK||p->type==WP_TABLE||p->type==WP_SOFA){ ry=0.45f; } else if(p->type==WP_BENCH){ ry=0.2f; } else if(p->type==WP_DOME){ rx=ry=p->w*0.62f; } else if(p->type==WP_PIPE){ rx=ry=p->w*0.5f; }
        if(fabsf(p->x-x)<rx+pr && fabsf(p->y-y)<ry+pr) return 0; }
    return 1; }
static int pcw_near_prop(int type, float rad){ for(int i=0;i<pcw_deck.nprop;i++){ const WdProp *p=&pcw_deck.prop[i]; if(p->type!=type) continue; float dx=p->x-pcw_px, dy=p->y-pcw_py; if(dx*dx+dy*dy<rad*rad) return i; } return -1; }
// (mission tests, BUGSTAR_MS_AUTO) stand in the room of the objective; later, from there, back at the hangar
static void pcw_place_obj(void){
    if(pcw_obj<0) return;
    for(int k=0;k<pcw_D;k++) pcw_path_[k]=(u8)pcw_obj_child(k);
    pcw_deck_here(0); int rm=-1; for(int i=1;i<pcw_deck.nroom;i++) if(pcw_deck.room[i].sub==pcw_obj){ rm=i; break; } if(rm<=0) return;
    const WdRoom *m=&pcw_deck.room[rm]; float bx=m->x0+0.6f, by=m->y0+0.6f; int found=0;
    for(int yy=m->y0;yy<=m->y1 && !found;yy++) for(int xx=m->x0;xx<=m->x1 && !found;xx++) if(pcw_free(xx+0.5f,yy+0.5f)){ bx=xx+0.5f; by=yy+0.5f; found=1; }
    pcw_px=bx; pcw_py=by; pcw_pa=atan2f((m->y0+m->y1+1)*0.5f-by,(m->x0+m->x1+1)*0.5f-bx); }
static void pcw_goto_hangar(void){ int hang=-1; for(int i=0;i<pcw_f.c.nsub;i++) if(pcw_f.c.sub[i].alive && pcw_f.c.sub[i].kind==WSS_HANGAR){ hang=i; break; } if(hang<0) return;
    for(int k=0;k<pcw_D;k++) pcw_path_[k]=(u8)wd_sub_child(&pcw_f.c,hang,k); pcw_deck_here(1); }
static void pc_ws_board(void){
    if(pcw_in) return; pcw_in=1; pcw_board_ok=0;
    int hang=-1; for(int i=0;i<pcw_f.c.nsub;i++) if(pcw_f.c.sub[i].alive && pcw_f.c.sub[i].kind==WSS_HANGAR){ hang=i; break; } if(hang<0 && pcw_mode!=2){ pcw_in=0; return; }
    pcw_D=wd_levels(&pcw_f.c); free(pcw_path_); free(pcw_hash_); pcw_path_=calloc(pcw_D+1,1); pcw_hash_=calloc(pcw_D+2,sizeof(u32)); pcw_hash_[0]=wd_root(&pcw_f.c);
    for(int k=0;k<pcw_D;k++) pcw_path_[k]=(u8)(hang>=0 ? wd_sub_child(&pcw_f.c,hang,k) : 0);
    pcw_deck_here(1); if(pcw_mode) pcw_obj=-1; else pcw_pick_obj(); pcw_hp=100+shield_max()/2; pcw_hurt=0; pcw_t=0; int tele=0;
    { const char *e=getenv("BUGSTAR_WS_ROOM"); if(e){ int kind=atoi(e); float turn=0; const char *t=strchr(e,','); if(t) turn=(float)atof(t+1);   // (tests: stand in a room of this kind)
        static const int int2sub[WR_NKIND]={-1,WSS_HANGAR,WSS_REACTOR,-1,-1,-1,-1,-1,-1,-1,-1,WSS_GEN,WSS_TURRET,WSS_BRIDGE,-1}; int sk=int2sub[kind];
        if(sk>=0){ for(int i=0;i<pcw_f.c.nsub;i++) if(pcw_f.c.sub[i].kind==sk){ for(int k=0;k<pcw_D;k++) pcw_path_[k]=(u8)wd_sub_child(&pcw_f.c,i,k); pcw_deck_here(0); break; } }
        int rm=-1; for(int tries=0;tries<40 && rm<0;tries++){ for(int i=1;i<pcw_deck.nroom;i++) if(pcw_deck.room[i].kind==kind){ rm=i; break; } if(rm<0 && pcw_D>0){ pcw_path_[pcw_D-1]=(u8)((pcw_path_[pcw_D-1]+1)&7); if(tries>7 && pcw_D>1) pcw_path_[pcw_D-2]=(u8)((pcw_path_[pcw_D-2]+1)&7); pcw_deck_here(0); } else if(rm<0) break; }
        if(kind==0){ int best=0; for(int i=0;i<WD_W*WD_W;i++){ int x=i%WD_W, y=i/WD_W; if(pcw_deck.cell[i].kind!=WDC_OPEN || pcw_deck.cell[i].room) continue;
                for(int dir=0;dir<4;dir++){ int dx=dir==0?1:dir==1?-1:0, dy=dir==2?1:dir==3?-1:0, n=0; while(n<40 && wd_walkable(&pcw_deck,x+dx*(n+1),y+dy*(n+1),0)) n++; if(n>best){ best=n; pcw_px=x+0.5f; pcw_py=y+0.5f; pcw_pa=atan2f((float)dy,(float)dx)+turn; } } } }
        if(rm>0){ const WdRoom *m=&pcw_deck.room[rm]; float bx=m->x0+0.6f, by=m->y0+0.6f; int found=0;
            for(int yy=m->y0;yy<=m->y1 && !found;yy++) for(int xx=m->x0;xx<=m->x1 && !found;xx++) if(pcw_free(xx+0.5f,yy+0.5f)){ bx=xx+0.5f; by=yy+0.5f; found=1; }
            pcw_px=bx; pcw_py=by; pcw_pa=atan2f((m->y0+m->y1+1)*0.5f-by,(m->x0+m->x1+1)*0.5f-bx)+turn; } } }
    if(pcm_auto && !pcw_mode) pcw_place_obj();
    if(pcm_auto){ int cr=0, gd=0; for(int i=0;i<pcw_deck.nnpc;i++){ if(pcw_deck.npc[i].kind==WN_CREW) cr++; else gd++; } const WdMix *mx=pcw_mix();
        fprintf(stderr,"mp %ld walk mode=%d craft=%s seed=%u z=%d D=%d mix=%d bunks=%d fighters=%d crates=%d guns=%d hangar=%d quarters=%d turret=%d | props bunk=%d fighter=%d crate=%d gun=%d | rooms hangar=%d quarters=%d | crew=%d guards=%d targets=%d\n",(long)pc_frames(),pcw_mode,pcw_f.c.name,pcw_f.c.seed,pcw_f.c.z,pcw_D,mx?1:0,
            mx?mx->bunks:-1,mx?mx->fighters:-1,mx?mx->crates:-1,mx?mx->guns:-1,mx?mx->hangar:-1,mx?mx->quarters:-1,mx?mx->turret:-1,wd_count_props(&pcw_deck,WP_BUNK),wd_count_props(&pcw_deck,WP_FIGHTER),wd_count_props(&pcw_deck,WP_CRATE),wd_count_props(&pcw_deck,WP_GUN),
            wd_count_rooms(&pcw_deck,WR_HANGAR),wd_count_rooms(&pcw_deck,WR_QUARTERS),cr,gd,({ int t=0; for(int i=1;i<pcw_deck.nroom;i++) if(pcw_deck.room[i].sub!=255) t++; t; })); }
    fade(1); oam_hide_all(); oam_flush(); for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i]; BG_PAL[0]=0; BG_PAL[250]=rgb15(6,8,12); BG_PAL[251]=rgb15(255,200,60);
    REG_DISPCNT = 4 | (1<<10); volatile u16 *pg=(volatile u16*)GBAA(0x06000000); pc_ovl_fn=pcw_ovl; REG_BLDCNT=0; REG_BLDY=0;
    char msg[48]="YOU ARE IN. FIND THE TARGET: SEL MAP"; int msg_t=180, hold=0, dead=0; float aim_cd=0, shot_t=0; u32 fr=0;
    while(1){
        vsync(); read_keys(); fr++; double dt=1/60.0; pcw_t+=dt; if(aim_cd>0) aim_cd-=dt; if(shot_t>0) shot_t-=dt;
        // ---- move (UP/DOWN walk, LEFT/RIGHT turn, L/R step sideways, B run). Vents are slow
        float sp = HELD(K_B) ? 0.16f : 0.085f; int cx=(int)pcw_px, cy=(int)pcw_py; if(pcw_deck.cell[cy*WD_W+cx].kind==WDC_VENT) sp*=0.45f;
        if(HELD(K_LEFT)) pcw_pa-=0.045f; if(HELD(K_RIGHT)) pcw_pa+=0.045f;
        float mvx=0, mvy=0; if(HELD(K_UP)){ mvx+=cosf(pcw_pa); mvy+=sinf(pcw_pa); } if(HELD(K_DOWN)){ mvx-=cosf(pcw_pa)*0.7f; mvy-=sinf(pcw_pa)*0.7f; }
        if(HELD(K_L)){ mvx+=sinf(pcw_pa)*0.8f; mvy-=cosf(pcw_pa)*0.8f; } if(HELD(K_R)){ mvx-=sinf(pcw_pa)*0.8f; mvy+=cosf(pcw_pa)*0.8f; }
        { float nx=pcw_px+mvx*sp, ny=pcw_py+mvy*sp; float r=0.25f; if(pcw_free(nx+(mvx>0?r:-r),pcw_py)) pcw_px=nx; if(pcw_free(pcw_px,ny+(mvy>0?r:-r))) pcw_py=ny; }
        // ---- the crew live; guards shoot
        int hurt=0; wd_tick(&pcw_deck,dt,pcw_px,pcw_py,&hurt); if(hurt){ pcw_hp-=hurt; pcw_hurt=0.6f; sfx(4); } if(pcw_hurt>0) pcw_hurt-=0.03f;
        // ---- charges go off
        for(int i=0;i<pcw_nch;i++){ PcwCharge *c=&pcw_ch[i]; if(pcw_t<c->t) continue; int s=c->sub; wf_sabotage(&pcw_f,s,pcw_schem); pcw_deck.alarm=1; pcm_on_sab(s); sfx(9);
            for(int r=0;r<pcw_deck.nroom;r++) if(pcw_deck.room[r].sub==s) pcw_deck.room[r].sub=255;
            snprintf(msg,sizeof msg,"%s DESTROYED%s",ws_sub_name(pcw_f.c.sub[s].kind),pcw_schem?" (WEAK POINT)":""); msg_t=200; pcw_ch[i]=pcw_ch[--pcw_nch]; i--; pcw_pick_obj();
            for(int p=0;p<pcw_deck.nprop;p++) if(pcw_deck.prop[p].type==WP_CHARGE){ pcw_deck.prop[p]=pcw_deck.prop[--pcw_deck.nprop]; break; } }
        if(pcw_f.state==WF_WON){ pc_ovl_fn=0; pcw_in=0; build_space_palette(); REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); pcw_victory(); return; }
        // ---- A: use what is in front of you, or shoot
        const char *prompt=0; int target_room=-1; { int r=wd_room_at(&pcw_deck,pcw_px,pcw_py); if(r>0 && pcw_deck.room[r].sub!=255) target_room=r; }
        int kp=pcw_near_prop(WP_KEY,1.6f), sp_=pcw_near_prop(WP_SCHEMA,1.8f);
        int at_tram = pcw_deck.cell[cy*WD_W+cx].kind==WDC_TRAM;
        int lock_near=-1; for(int k=0;k<4;k++){ static const int DX[4]={1,-1,0,0}, DY[4]={0,0,1,-1}; int x=cx+DX[k], y=cy+DY[k]; if(x>=0&&y>=0&&x<WD_W&&y<WD_W && pcw_deck.cell[y*WD_W+x].kind==WDC_LOCK) lock_near=y*WD_W+x; }
        if(kp>=0) prompt="A: TAKE THE KEYCARD"; else if(lock_near>=0) prompt = pcw_key ? "A: OPEN THE LOCKED DOOR" : "LOCKED: FIND THE KEYCARD (SECURITY)";
        else if(sp_>=0 && target_room>=0 && !pcw_schem) prompt="A: READ THE SCHEMATIC (WEAK POINT)"; else if(target_room>=0) prompt="HOLD A: PLANT A CHARGE";
        else if(at_tram && pcw_D>0) prompt="A: RIDE THE TRAM"; else if(at_tram) prompt="THE ONLY DECK: NO TRAM NEEDED";
        pcm_dwell(target_room,&prompt,msg,&msg_t);
        if(HIT(K_A) && kp>=0){ pcw_key=1; pcw_deck.prop[kp]=pcw_deck.prop[--pcw_deck.nprop]; scpy(msg,"KEYCARD TAKEN"); msg_t=120; sfx(6); }
        else if(HIT(K_A) && lock_near>=0 && pcw_key){ for(int i=0;i<WD_W*WD_W;i++) if(pcw_deck.cell[i].kind==WDC_LOCK) pcw_deck.cell[i].kind=WDC_DOOR; scpy(msg,"THE DOOR SLIDES OPEN"); msg_t=120; sfx(2); }
        else if(HIT(K_A) && sp_>=0 && target_room>=0 && !pcw_schem){ pcw_schem=1; scpy(msg,"WEAK POINT FOUND: PLANT THE CHARGE"); msg_t=150; sfx(6); }
        else if(HIT(K_A) && at_tram && pcw_D>0){ pcw_sector_map(pg); }
        else if(HELD(K_A) && target_room>=0){ hold++; if(hold==75){ int s=pcw_deck.room[target_room].sub; int dup=0; for(int i=0;i<pcw_nch;i++) if(pcw_ch[i].sub==s) dup=1;
                if(!dup && pcw_nch<4){ pcw_ch[pcw_nch].sub=s; pcw_ch[pcw_nch].x=pcw_px; pcw_ch[pcw_nch].y=pcw_py; pcw_ch[pcw_nch].t=pcw_t+5; pcw_nch++;
                    if(pcw_deck.nprop<WD_MAXPROP){ WdProp *p=&pcw_deck.prop[pcw_deck.nprop++]; memset(p,0,sizeof *p); p->type=WP_CHARGE; p->x=pcw_px+cosf(pcw_pa)*0.8f; p->y=pcw_py+sinf(pcw_pa)*0.8f; p->w=0.4f; p->h=0.3f; p->room=(u8)target_room; }
                    scpy(msg,"CHARGE SET: 5 SECONDS. MOVE!"); msg_t=150; sfx(6); } } }
        else if(HIT(K_A) && aim_cd<=0 && !pcw_mode){ aim_cd=0.3f; shot_t=0.08f; sfx(10);                     // the blaster: the first one in the middle of the view
            int best=-1; float bd=14; for(int i=0;i<pcw_deck.nnpc;i++){ WdNpc *n=&pcw_deck.npc[i]; if(!n->alive) continue; float dx=n->x-pcw_px, dy=n->y-pcw_py, d=sqrtf(dx*dx+dy*dy); float a=atan2f(dy,dx)-pcw_pa; while(a>3.14159f) a-=6.2832f; while(a<-3.14159f) a+=6.2832f;
                if(d<bd && fabsf(a)<0.09f+0.3f/(d+0.5f)){ bd=d; best=i; } }
            if(best>=0){ WdNpc *n=&pcw_deck.npc[best]; n->hp-=25+(float)(level>1000?1000:level)*0.1f; pcw_deck.alarm=1; if(n->hp<=0){ n->alive=0; n->act=WA_DEAD; sfx(9); } } }
        if(!HELD(K_A)) hold=0;
        if(HIT(K_SEL)) pcw_deck_map(pg);
        if(pcm_auto && !pcw_mode && !tele && pcm_obj_done()){ tele=1; pcw_goto_hangar(); fprintf(stderr,"mp %ld objective done: back to the hangar deck\n",(long)pc_frames()); }       // (tests) walk home
        else if(tele==1) tele=2;
        if(HIT(K_START) || tele==2){ int r=wd_room_at(&pcw_deck,pcw_px,pcw_py); if(r>0 && pcw_deck.room[r].kind==WR_HANGAR){ scpy(msg,"BACK TO YOUR SHIP"); break; } scpy(msg,"GET TO A HANGAR TO LEAVE"); msg_t=90; tele=pcm_auto?1:0; }
        if(pcw_hp<=0){ dead=1; break; }
        // ---- the HUD (the 3D view is under it)
        for(int i=0;i<240*160/2;i++) pg[i]=0;
        { char b[64]; const WdRoom *r=&pcw_deck.room[wd_room_at(&pcw_deck,pcw_px,pcw_py)]; pcw_text(pg,4,3,r->name,C_GOLD); pcw_text(pg,236-textlen(pcw_deck.name)*4,3,pcw_deck.name,C_GREY);
          scpy(b,"HP "); ncat(b,pcw_hp<0?0:pcw_hp); if(pcw_key) scat(b,"  KEY"); if(pcw_schem) scat(b,"  WEAK POINT"); pcw_text(pg,4,150,b,pcw_hp<40?C_RED:C_WHITE);
          if(pcw_obj>=0){ scpy(b,"OBJECTIVE: "); scat(b,ws_sub_name(pcw_f.c.sub[pcw_obj].kind)); int here=0; for(int i=0;i<pcw_deck.nsubs;i++) if(pcw_deck.subs[i]==pcw_obj) here=1;
              scat(b, here ? " (MARKED)" : pcw_D>0 ? " (TAKE THE TRAM)" : ""); pcw_text(pg,4,11,b,251); }
          else if(pcw_mode==1) pcw_text(pg,4,11,"ALLIED CRAFT: NO ONE SHOOTS. LEAVE FROM A HANGAR",C_GREEN);
          else if(pcw_mode==2){ snprintf(b,sizeof b,"YOUR SHIP: %d FIGHTERS  %d BUNKS  %d CRATES",pcw_mixv.fighters,pcw_mixv.bunks,pcw_mixv.crates); pcw_text(pg,4,11,b,C_GREEN); }
          { int h=(int)fmod(pcw_deck.clock,24); scpy(b,"SHIP TIME "); ncat(b,h); scat(b,":00"); if(pcw_deck.alarm) scpy(b,"ALARM!"); pcw_text(pg,236-textlen(b)*4,150,b,pcw_deck.alarm?C_RED:C_GREY); }
          int near=-1; float nd=3.5f; for(int i=0;i<pcw_deck.nnpc;i++){ const WdNpc *n=&pcw_deck.npc[i]; if(!n->alive) continue; float dx=n->x-pcw_px, dy=n->y-pcw_py, d=sqrtf(dx*dx+dy*dy); if(d<nd){ nd=d; near=i; } }
          if(near>=0 && !prompt){ const WdNpc *n=&pcw_deck.npc[near]; scpy(b, n->kind==WN_GUARD?"GUARD: ":n->kind==WN_OFFICER?"OFFICER: ":"CREW: "); scat(b,wd_act_name(n->act)); pcw_text(pg,120-textlen(b)*2,128,b,n->kind==WN_CREW?C_GREY:C_RED); }
          if(prompt) pcw_text(pg,120-textlen(prompt)*2,128,prompt,C_GREEN);
          if(hold>0 && hold<75 && target_room>=0) rect(pg,90,138,hold*60/75,3,C_GOLD);
          if(msg_t>0){ msg_t--; pcw_text(pg,120-textlen(msg)*2,24,msg,C_WHITE); }
          if(!(fr&128)) pcw_text(pg,60,150,"SEL: MAP  START: LEAVE",C_DGREY); }
    }
    pc_ovl_fn=0; pcw_in=0; fade(1);
    build_space_palette(); REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); BG_PAL[57]=rgb15(14,16,30); BG_PAL[58]=rgb15(60,70,120);
    if(dead){ hull-=30; if(hull<1) hull=1; space_say("THROWN OUT AN AIRLOCK! -30 HULL"); } else space_say("BACK IN YOUR SHIP");
    if(!dead && !pcw_mode) pcm_collect();
    if(!dead && pcw_mode==1){ hull=100; fuel=fuel_max(); space_say("THE ALLIED CREW REFUEL AND PATCH YOUR SHIP"); fprintf(stderr,"ally refit hull %d fuel %d\n",hull,fuel); }
    shz=-(PCW_DS*2); svx_=svy_=svz_=0; speed=0; fade(0); wait_release();
}

// ================================================================= THE SCREENS
static void pc_ws_breakdown(volatile u16 *pg, const WsCraft *c, const WsPay *p){
    oam_hide_all(); oam_flush(); for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i]; REG_DISPCNT = 4 | (1<<10); pg=(volatile u16*)GBAA(0x06000000); wait_release();
    ui_bg(pg,0,160); ui_panel(pg,4,4,232,140); char b[80], v[48];
    ui_head(pg,10,8,220,"COST BREAKDOWN",C_GOLD); scpy(b,c->name); scat(b," - "); scat(b,ws_class_name(c->cls)); text(pg,10,18,b,C_WHITE,1);
    static const char *const rn[6]={"MATERIALS","CREW AND PAY","AIRCRAFT","WEAPONS","POWER","TOTAL COST"};
    int y=28; for(int i=0;i<6;i++){ wn_text(c->cost[i],v,sizeof v,0); text(pg,14,y,rn[i],i==5?C_GOLD:C_GREY,1); rtext(pg,226,y,v,i==5?C_GOLD:C_WHITE); y+=8; }
    y+=2; snprintf(b,sizeof b,"OUTSIDE %d PERCENT   INSIDE %d PERCENT",(int)((1-p->inside)*100+0.5),(int)(p->inside*100+0.5)); text(pg,14,y,b,C_GREY,1); y+=8;
    { WN a=wn(p->A); wn_text(a,v,sizeof v,1); scpy(b,"ATTACK FACTOR"); text(pg,14,y,b,C_GREY,1); rtext(pg,226,y,v,C_WHITE); y+=8; }
    snprintf(v,sizeof v,"%.3f",p->Fm); text(pg,14,y,"METHOD FACTOR",C_GREY,1); rtext(pg,226,y,v,C_WHITE); y+=8;
    snprintf(v,sizeof v,"%.3f",p->Fw); text(pg,14,y,"WEAPON FACTOR",C_GREY,1); rtext(pg,226,y,v,C_WHITE); y+=8;
    if(pcm_note[0]) text(pg,56,y+2,pcm_note,C_CYAN,1); wn_text(p->reward,v,sizeof v,0); text(pg,14,y+2,"GOLD PAID",C_GOLD,1); rtext(pg,226,y+2,v,C_GOLD); y+=12;
    if(p->reward.e>=24){ wn_sci(p->reward,v,sizeof v); rtext(pg,226,y,v,C_GREY); } y+=8;
    if(p->cheat) text(pg,14,y,"CHEATED SIZE: THIS PAY IS MARKED CHEATED",C_RED,1); else if(p->reward.e<0) text(pg,14,y,"LESS THAN ONE GOLD: IT GOES TO YOUR GOLD DUST",C_GREY,1);
    ui_foot(pg,"REWARD = COST X ATTACK X METHOD X WEAPON","A: OK");
    do { vsync(); read_keys(); } while(!HIT(K_A) && !HIT(K_B)); wait_release(); pcm_note[0]=0;
}
static void pcw_detail(volatile u16 *pg, const WsCraft *c, int y0){
    char b[80], v[48]; int y=y0;
    scpy(b,c->name); text(pg,12,y,b,C_GOLD,1); ws_len_text(c->z,v,sizeof v); rtext(pg,228,y,v,C_WHITE); y+=8;
    scpy(b,c->title); text(pg,12,y,b,C_WHITE,1); y+=8;
    scpy(b,ws_cult_name(c->cult)); scat(b,", "); scat(b,ws_mat_name(c->mat)); scat(b,", "); scat(b,ws_surf_name(c->surf)); text(pg,12,y,b,C_GREY,1); y+=8;
    wn_text(c->hp_hull,v,sizeof v,0); scpy(b,"HULL HP "); scat(b,v); text(pg,12,y,b,C_GREY,1); snprintf(b,sizeof b,"SHIELD LAYERS %d  SECTIONS %d",c->layers,c->nsec); rtext(pg,228,y,b,C_GREY); y+=8;
    wn_text(c->crew,v,sizeof v,0); scpy(b,"CREW "); scat(b,v); text(pg,12,y,b,C_GREY,1); wn_text(c->air_n,v,sizeof v,0); scpy(b,"AIRCRAFT "); scat(b,v); rtext(pg,228,y,b,C_GREY); y+=8;
    wn_text(c->cost[WK_TOTAL],v,sizeof v,0); scpy(b,"COST "); scat(b,v); scat(b," G"); text(pg,12,y,b,C_GOLD,1); y+=8;
    WsThreat t; ws_threat(c,ws_attack((double)level,laser_lv,ship_gun(),tech_lv),pcw_ehp(),wing_n,pcw_kits,fuel_max(),&t);
    scpy(b,"THREAT: "); scat(b,t.word); text(pg,12,y,b,t.tier>=4?C_RED:t.tier>=2?C_GOLD:C_GREEN,1);
    { WsKill k; memset(&k,0,sizeof k); k.attack=ws_attack((double)level,laser_lv,ship_gun(),tech_lv); k.out_dmg[0]=wn(1); WsPay p; ws_pay(c,&k,&p); wn_text(p.reward,v,sizeof v,0); scpy(b,"PAYS YOU ABOUT "); scat(b,v); rtext(pg,228,y,b,C_GOLD); } y+=10;
    text(pg,12,y,"READY WHEN YOU HAVE:",C_WHITE,1); y+=8;
    { WN a=wn(t.need_attack); wn_text(a,v,sizeof v,0); snprintf(b,sizeof b,"ATTACK %s (YOU %.0f)",v,ws_attack((double)level,laser_lv,ship_gun(),tech_lv)); text(pg,16,y,b,C_GREY,1); y+=8; }
    snprintf(b,sizeof b,"TOUGHNESS %d (YOU %d)  WINGMEN %d  KITS %d  FUEL %d",t.need_ehp,pcw_ehp(),t.need_wing,t.need_kits,t.need_fuel); text(pg,16,y,b,C_GREY,1);
}
static void pcw_armoury(volatile u16 *pg){
    static const int price[6]={0,25000,400,3000,250000,600}; static const char *const nm[6]={"","RAILGUN (FIT, NO AMMO)","10 MISSILES","4 TORPEDOES","NOVA LANCE: 3 SHOTS","REPAIR KIT"};
    int sel=1, dirty=1; wait_release();
    while(1){ if(dirty){ dirty=0; ui_bg(pg,0,160); ui_panel(pg,6,6,228,136); ui_head(pg,12,10,216,"ARMOURY",C_GOLD); char b[64];
            for(int i=1;i<6;i++){ int y=26+i*12; if(i==sel) rect(pg,10,y-2,220,10,6); text(pg,16,y,nm[i],i==sel?C_WHITE:C_GREY,1); scpy(b,""); ncat(b,price[i]); scat(b,"G"); rtext(pg,226,y,b,C_GOLD); }
            scpy(b,"YOU HAVE: RAIL "); scat(b,pcw_ammo[1]?"YES":"NO"); scat(b,"  MISSILES "); ncat(b,pcw_ammo[2]); scat(b,"  TORPEDOES "); ncat(b,pcw_ammo[3]); text(pg,12,108,b,C_GREY,1);
            scpy(b,"NOVA "); ncat(b,pcw_ammo[4]); scat(b,"  KITS "); ncat(b,pcw_kits); scat(b,"   GOLD "); { char g[32]; pcw_wallet(g,sizeof g); scat(b,g); } text(pg,12,116,b,C_GREY,1);
            text(pg,12,126,"STRONGER WEAPONS KILL FASTER BUT PAY LESS",C_DGREY,1); ui_foot(pg,"UP/DOWN: PICK  A: BUY","B: BACK"); }
        vsync(); read_keys(); if(HIT(K_B)) break;
        if(HIT(K_UP)){ sel=sel>1?sel-1:5; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=sel<5?sel+1:1; dirty=1; sfx(7); }
        if(HIT(K_A)){ if(gold<price[sel] || (sel==1 && pcw_ammo[1])){ sfx(0); continue; } gold-=price[sel]; sfx(6); dirty=1;
            if(sel==1) pcw_ammo[1]=-1; else if(sel==2) pcw_ammo[2]+=10; else if(sel==3) pcw_ammo[3]+=4; else if(sel==4) pcw_ammo[4]+=3; else pcw_kits++; } }
    wait_release(); pc_ws_slot_save(cur_slot);
}
// the list of colossal craft near this star; in space (from the scanner) A then START engages at once, on a planet ENGAGE waits for take-off
static int pcm_ally_craft(WsCraft *c);
static void pcw_own_craft(WsCraft *c){ ws_make(c,0xB06C0DEu^(u32)cur_slot*0x9E3779B1u,240,0); snprintf(c->name,sizeof c->name,"YOUR OWN SHIP"); snprintf(c->title,sizeof c->title,"YOUR OWN SHIP: THE HANGAR DECK"); }
static void pcw_detail_extra(volatile u16 *pg, int kind, const WsCraft *c, int y0){            // an allied craft, or your own ship
    char b[80], v[48]; int y=y0; scpy(b,c->name); text(pg,12,y,b,C_GOLD,1); ws_len_text(c->z,v,sizeof v); rtext(pg,228,y,v,C_WHITE); y+=8;
    scpy(b,c->title); text(pg,12,y,b,C_WHITE,1); y+=10;
    if(kind==1){ text(pg,12,y,"ALLIED CRAFT: IT FLIES WITH THE CONVOY.",C_GREEN,1); y+=8; text(pg,12,y,"DOCK AT ITS HANGAR. THE CREW ARE FRIENDS: NO ONE",C_GREY,1); y+=8;
        text(pg,12,y,"SHOOTS, NOTHING TO BLOW UP. LEAVE FROM A HANGAR",C_GREY,1); y+=8; text(pg,12,y,"AND ITS CREW REFUEL AND PATCH YOUR SHIP.",C_GREY,1); y+=10;
        text(pg,12,y,pc_sy_on && pc_sy_foes ? "ITS ROOMS COME FROM A TEAM 0 DESIGN (SHIP EDITOR)." : "(TURN FOES ON IN THE SHIP EDITOR FOR TEAM 0 DESIGNS)",C_DGREY,1); }
    else { SyDesign od=pcw_own_design(); SyRooms r; sy_rooms(&od,&r); text(pg,12,y,"YOUR OWN HANGAR DECK",C_GREEN,1); y+=8; text(pg,12,y,"BUILT FROM YOUR SHIP'S DESIGN IN THE SHIP EDITOR.",C_GREY,1); y+=10;
        snprintf(b,sizeof b,"FIGHTERS %d  BUNKS %d  CRATES %d  GUNS %d",r.fighters,r.bunks,r.crates,r.guns); text(pg,12,y,b,C_WHITE,1); y+=8;
        snprintf(b,sizeof b,"HANGARS %d  QUARTERS %d  WORKSHOP %d  MESS %d",r.hangar,r.quarters,r.workshop,r.mess); text(pg,12,y,b,C_GREY,1); y+=8;
        text(pg,12,y,pc_sy_on ? "THE DESIGN IS YOUR SAVED SHIP." : "NO SAVED DESIGN YET: THE DEFAULT SHIP.",C_DGREY,1); } }
// the list of colossal craft near this star; in space (from the scanner) A then START engages at once, on a planet ENGAGE waits for take-off.
// In space the list also holds an ALLIED craft (when an ESCORT job is on here: START docks) and YOUR OWN SHIP (START walks your hangar deck).
static int pcw_list(volatile u16 *pg, int in_space){
    enum { NC=6 }; WsCraft *cs=malloc(sizeof(WsCraft)*(NC+3)); int nc=NC, sel=0, dirty=1, detail=0, cz=WS_ZREF*2, go=0, cheat_row=plw_cheated(), nx=0, xk[2];
    u32 sd[NC]; int zz[NC]; for(int i=0;i<NC;i++){ pcw_contact(i,&sd[i],&zz[i]); pcw_make(&cs[i],sd[i],zz[i],pcm_cf); }
    if(in_space){ if(pcm_ally_craft(&cs[NC+1])) xk[nx++]=1; pcw_own_craft(&cs[NC+2]); xk[nx++]=2; }
    int cr=nc+nx; pcw_req_mode=0; pcw_topup(); wait_release();
    while(1){
        if(dirty){ dirty=0; oam_hide_all(); oam_flush(); ui_bg(pg,0,160); ui_panel(pg,6,6,228,136); char b[64], v[40];
            ui_head(pg,12,10,216,in_space?"WARSHIP SCANNER":"WARSHIPS NEAR THIS STAR",C_GOLD);
            if(detail){ if(sel<nc) pcw_detail(pg,&cs[sel],22); else if(sel<cr) pcw_detail_extra(pg,xk[sel-nc],&cs[NC+xk[sel-nc]],22); else pcw_detail(pg,&cs[NC],22);
                ui_foot(pg,sel>=nc&&sel<cr ? (xk[sel-nc]==1?"START: DOCK AT IT":"START: WALK YOUR HANGAR DECK") : in_space?"START: ENGAGE":"START: ENGAGE AFTER TAKE-OFF","B: BACK"); }
            else { for(int i=0;i<nc+nx;i++){ int y=(nx?22:24)+i*(nx?11:12); const WsCraft *c = i<nc ? &cs[i] : &cs[NC+xk[i-nc]]; if(i==sel) rect(pg,10,y-2,220,11,6);
                    if(i>=nc){ text(pg,16,y,xk[i-nc]==1?"ALLY":"YOUR SHIP",i==sel?C_WHITE:C_GREEN,1); text(pg,110,y,xk[i-nc]==1?c->name:"HANGAR DECK",C_GREY,1); rtext(pg,226,y,xk[i-nc]==1?"DOCK":"WALK",C_GREEN); continue; }
                    scpy(b,ws_class_name(c->cls)); text(pg,16,y,b,i==sel?C_WHITE:C_GREY,1); ws_len_text(c->z,v,sizeof v); text(pg,110,y,v,C_GREY,1); if(pcm_mark[i]) text(pg,152,y,"MISSION",C_CYAN,1);
                    WsThreat t; ws_threat(c,ws_attack((double)level,laser_lv,ship_gun(),tech_lv),pcw_ehp(),wing_n,pcw_kits,fuel_max(),&t); rtext(pg,226,y,t.word,t.tier>=4?C_RED:t.tier>=2?C_GOLD:C_GREEN); }
                if(cheat_row){ int y=(nx?22:24)+cr*(nx?11:12); if(sel==cr) rect(pg,10,y-2,220,11,6); ws_len_text(cz,v,sizeof v); scpy(b,"CHEAT: ANY SIZE  "); scat(b,v); text(pg,16,y,b,C_RED,1); }
                pcw_wallet(v,sizeof v); scpy(b,"GOLD "); scat(b,v); text(pg,12,118,b,C_GOLD,1); scpy(b,"KILLS "); ncat(b,pcw_kills); if(pcw_kills){ wn_text(pcw_best,v,sizeof v,0); scat(b,"  BEST PAY "); scat(b,v); } text(pg,12,126,b,C_GREY,1);
                { static const char *const en[4]={"OFF","RARE","NORMAL","COMMON"}; scpy(b,"L: WARSHIP ENCOUNTERS: "); scat(b,en[pcm_enc_mode&3]); text(pg,12,108,b,C_CYAN,1); }
                ui_foot(pg, cheat_row && sel==cr ? "LEFT/RIGHT: SIZE (L/R: X10)  A: SEE IT" : "A: SEE IT  SEL: ARMOURY  START: MISSIONS","B: BACK"); } }
        vsync(); read_keys();
        if(HIT(K_B)){ if(detail){ detail=0; dirty=1; continue; } break; }
        if(!detail){ int n=cr+(cheat_row?1:0);
            if(HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; sfx(7); }
            if(sel==cr && cheat_row){ int st = HELD(K_L)||HELD(K_R) ? 100 : 10; if(HIT(K_LEFT)){ cz-=st; if(cz<WS_ZMIN) cz=WS_ZMIN; dirty=1; } if(HIT(K_RIGHT)){ cz+=st; if(cz>WS_ZMAX) cz=WS_ZMAX; dirty=1; } }
            if(HIT(K_SEL)){ pcw_armoury(pg); dirty=1; } if(HIT(K_START)){ pc_ms_board(pg,MP_SPACE); dirty=1; } if(HIT(K_L) && !(sel==cr && cheat_row)){ pcm_enc_mode=(pcm_enc_mode+1)&3; dirty=1; sfx(7); }
            if(HIT(K_A)){ if(sel==cr && cheat_row) pcw_make(&cs[NC],0x5EED0000u+(u32)cz,cz,1); detail=1; dirty=1; sfx(2); } }
        else if(HIT(K_START)){ if(sel>=nc && sel<cr){ pcw_req_mode=xk[sel-nc]; go=1; sfx(6); break; }
            const WsCraft *c = sel<nc ? &cs[sel] : &cs[NC]; pcw_req_seed=c->seed; pcw_req_z=c->z; pcw_req_cheat=c->cheat; pcw_req=1; go=1; sfx(6); break; }
    }
    free(cs); wait_release(); pc_ws_slot_save(cur_slot); return go;
}
// walk an allied craft (mode 1) or your own hangar deck (mode 2): the same interior view and walking as a boarding, no fight, nothing to blow up
static void pc_ws_board(void);
static void pcw_walk(int mode){
    WsFight *sv=malloc(sizeof(WsFight)); *sv=pcw_f; WsCraft c; if(mode==1){ if(!pcm_ally_craft(&c)){ free(sv); return; } } else pcw_own_craft(&c);
    WsShip sh=pcw_ship(); wf_start(&pcw_f,&c,&sh); pcw_f.layers_up=0; pcw_f.lay_hp=wn(0); pcw_mode=mode; pcw_board_ok=0;
    pc_ws_board();
    pcw_mode=0; pcw_f=*sv; free(sv); }
static void pc_ws_lab(volatile u16 *pg){ int go=pcw_list(pg,scene==2); if(go && scene!=2){ ui_bg(pg,0,160); ui_panel(pg,30,60,180,30); text(pg,40,68,"TAKE OFF: IT WAITS IN ORBIT",C_GOLD,1); text(pg,40,78,"(LAUNCH FROM YOUR SHIP)",C_GREY,1); wait_release(); do { vsync(); read_keys(); } while(!HIT(K_A)&&!HIT(K_B)); wait_release(); } }
static int pc_ws_scan(void){                                   // in space: hold R and press SELECT
    if(pc_ws_on || in_lair) return 0;
    volatile u16 *pg=(volatile u16*)(GBAA(0x06000000) + (page_back?0:0xA000)); REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(page_back?0:(1<<4));
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    int go=pcw_list(pg,1); build_space_palette();
    if(go && pcw_req_mode){ int m=pcw_req_mode; pcw_req_mode=0; pcw_walk(m); }
    else if(go){ pcw_req=0; pcw_engage(pcw_req_seed,pcw_req_z,pcw_req_cheat); }
    return 1;
}
static void pc_ws_space_enter(void){                          // a craft engaged from a planet waits in orbit; far out, now and then one drifts by
    if(pcw_req && !pc_ws_on && !no_ambush){ pcw_req=0; pcw_engage(pcw_req_seed,pcw_req_z,pcw_req_cheat); return; }
    if(!no_ambush && ant_king && sys[cur_sys].kind==6 && (hmix(sys_id(cur_sys)^0xC0105Au)%30u)==0) space_say("A COLOSSUS DRIFTS HERE: HOLD R, PRESS SELECT");
}
// ---- RENDER TEST HOOK (pc/tests/views.sh): BUGSTAR_VT=<view>,args jumps a running game (after BUGSTAR_VT_AT frames, default 650) to one 3D view, so headless pictures and
// timings of the native renderers can be made without playing there. Nothing here runs unless BUGSTAR_VT is set.
//   space,CHASE,YAW,PITCH    out in space; BUGSTAR_VT_FX=1 keeps cans, sparks and a laser burst in front of the ship
//   planet,CLS,WEIRD,WEATHER,HOUR,SEED   a world (CLS -1 any, 7+ hell classes; WEIRD trait id or -1; WEATHER 0-5 or -1; HOUR 0-2 or -1)
//   dungeon,FLOOR,TERR,WATER    FLOOR = depth, TERR=1 the terraced cavern, WATER=1 a pond and polished tiles ahead of you
//   kart,TRACK                  a kart race on that track
static int pc_vt_terr_force;
static int pc_vt_terr(void){ return pc_vt_terr_force; }
static void pc_vt_tick(void){
    static int st=-1; static char mode[16]; static int a[6]; const char *e=getenv("BUGSTAR_VT"); if(!e) return;
    if(st<0){ st=0; memset(a,0,sizeof a); a[1]=-1; a[2]=-1; a[3]=-1; a[4]=-1; sscanf(e,"%15[a-z],%d,%d,%d,%d,%d",mode,&a[0],&a[1],&a[2],&a[3],&a[4]); }
    const char *at=getenv("BUGSTAR_VT_AT"); if(!in_game || frames < (at?atoi(at):650)) return;
    if(st==0 && scene==0){ st=1;
        if(!strcmp(mode,"space")){ chase_cam=a[0]; oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(3); shx=PR*3; shy=0; shz=0; yaw=a[1]>=0?a[1]:0; pitch=a[2]>=0?a[2]:0; speed=a[3]>=0?a[3]:0; basis(); }
        else if(!strcmp(mode,"planet")){ int seed=a[4]>=0?a[4]:5; ant_king=1; zones_open=6; cleared_mask=63; endless=1; galaxy_no=4000000000u; for(int w=0;w<NCW;w++) clus[w]=hmix(w*77+seed); u_settle(); gen_cluster(); cur_sys=7+seed%20; dbg_cls=a[0]; dbg_weird=a[1]; dbg_weather=a[2]; dbg_hour=a[3]; fade(0); arrive_on_planet();
            { const char *w=getenv("BUGSTAR_VT_WLV"); if(w){ int k,l,n; wd.weird=1; while(sscanf(w,"%d:%d%n",&k,&l,&n)==2){ if(k>=0&&k<NMW){ wlv[k]=(u8)l; wcol[k]=rgb15(120+(k*53)%130,100+(k*97)%150,110+(k*31)%140); } w+=n; if(*w==',') w++; } BG_PAL[88]=wcol[MW_SUNS]; BG_PAL[89]=lift15(wcol[MW_SUNS],10); BG_PAL[90]=wcol[MW_RINGS]; BG_PAL[91]=lift15(wcol[MW_MOONS],6); BG_PAL[92]=shade15(wcol[MW_MOONS],9); BG_PAL[93]=lift15(wcol[MW_RINGS],8); BG_PAL[94]=rgb15(255,240,200); } }
            { const char *h=getenv("BUGSTAR_VT_HEAD"); if(h){ heading=atoi(h)&1023; heading_rays(); } } }
        else if(!strcmp(mode,"dungeon")){ portal_depth=a[0]>0?a[0]:3; pc_vt_terr_force=a[1]==1; enter_dungeon_flag=1; }
        else if(!strcmp(mode,"kart")){ k_cls=1; k_type=0; k_trk=a[0]%kt_count(); k_rec=-1; kt_official(k_trk,&kt); kt.laps=3; kart_go(NRC,-1,0,1); }
        return; }
    if(st==1 && scene==2 && getenv("BUGSTAR_VT_FX")){                                           // space: things to look at, kept in front of the ship
        s32 *o; Can *c=cans; (void)o; for(int k=0;k<3;k++){ c[k].alive=1; c[k].good=(u8)(3+k); int f=700+k*500, r=(k-1)*260, u=(k==1?-120:90);
            c[k].x=shx+((bf[0]*f+br[0]*r+bu[0]*u)>>14); c[k].y=shy+((bf[1]*f+br[1]*r+bu[1]*u)>>14); c[k].z=shz+((bf[2]*f+br[2]*r+bu[2]*u)>>14); }
        if((frames%12)==0){ int f=1500; boom(shx+((bf[0]*f+br[0]*300)>>14), shy+((bf[1]*f+br[1]*300)>>14), shz+((bf[2]*f+br[2]*300)>>14), 6, 54); } }
    if(st==1 && scene==1 && getenv("BUGSTAR_VT_FX") && a[2]==1){ st=2;                          // dungeon: a pond and polished tiles in front of you
        int cx=px_>>16, cy=py_>>16, k=(pang>>8)&3; static const int dx[4]={1,0,-1,0}, dy[4]={0,1,0,-1};
        for(int d=2;d<=7;d++) for(int sd=-3;sd<=3;sd++){ int x=cx+dx[k]*d-dy[k]*sd, y=cy+dy[k]*d+dx[k]*sd; if(x<1||y<1||x>=MW-1||y>=MW-1) continue; int i=y*MW+x; if(dmap[i]!=D_FLOOR) continue;
            fmat[i]= (sd<0) ? FL_TILE : ((d==2||d==7||sd==3) ? FL_SHALLOW : FL_WATER); fcode[i]= (sd<0) ? 2 : (fmat[i]==FL_WATER ? 5 : 9); } }
}
// ---- the test hook: BUGSTAR_WS_TEST=fight,Z,SEED | inside,Z,SEED | kill,Z,SEED | lab  (after BUGSTAR_WS_AT frames in the game)
static void pc_ws_autotest(void){
    pc_vt_tick(); pc_sg_tick();
    static int done=0; if(in_game){ pcm_tick(); pcm_mp_tick(); } if(done || !in_game) return; const char *e=getenv("BUGSTAR_WS_TEST"); if(!e){ done=1; return; }
    const char *at=getenv("BUGSTAR_WS_AT"); const char *sy=getenv("BUGSTAR_MS_SYNC"); if(frames < (at?atoi(at):0) || (scene!=0 && !(scene==2 && !strncmp(e,"mscan",5))) || (sy && pc_frames()<atol(sy))) return; done=1; if(getenv("BUGSTAR_WS_DEBUG")) fprintf(stderr,"ws autotest at game frame %d scene %d script frame %ld\n",frames,scene,pc_frames());
    char mode[16]={0}; int z=WS_ZREF; unsigned seed=0; sscanf(e,"%15[a-z],%d,%u",mode,&z,&seed);
    if(!strcmp(mode,"lab")){ pc_ws_lab((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen lab returned\n"); return; }
    { const char *g=getenv("BUGSTAR_AFT_GOLD"); if(g) gold=atoll(g); }
    if(!strcmp(mode,"sprgal")){ pc_sg_gallery((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen sprgal returned\n"); return; }
    if(!strcmp(mode,"pclab")){ pc_lab_screen((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen pclab returned\n"); return; }
    if(!strcmp(mode,"shop")){ shop_menu(); fprintf(stderr,"screen shop returned\n"); return; }                                  // (screens test) the ship's shop menu (supplies, gear, sell, quest board, heal...)
    if(!strcmp(mode,"spshop")){ spell_shop((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen spshop returned\n"); return; }
    if(!strcmp(mode,"unisel")){ universe_menu(VISPG); fprintf(stderr,"screen unisel returned\n"); return; }                                 // (steady test) the universe jump menu: pick a universe by its number
    if(!strcmp(mode,"unityp")){ u32 u[UW]={1}; uni_edit(VISPG,u); fprintf(stderr,"screen unityp returned\n"); return; }              // (steady test) typing a universe number digit by digit
    if(!strcmp(mode,"bench")){ workbench((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen bench returned\n"); return; }
    if(!strcmp(mode,"colony")){ sim_ship_colony((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen colony returned\n"); return; }
    if(!strcmp(mode,"giant")){ pc_giant_hub((volatile u16*)GBAA(0x06000000)); fprintf(stderr,"screen giant returned\n"); return; }
    if(!strcmp(mode,"missions")){ pc_ms_board((volatile u16*)GBAA(0x06000000),MP_STATION); fprintf(stderr,"screen missions returned\n"); return; }
    if(!strncmp(mode,"mplay",5)||!strcmp(mode,"mscan")||!strcmp(mode,"maccept")||!strcmp(mode,"mresume")||!strcmp(mode,"mcheck")||!strcmp(mode,"mexpire")){ pcm_mp_start(mode); return; }
    if(!strcmp(mode,"hell")){ pcm_enc_mode=3; pcm_test_hell(); return; }               // (aftermath test) the surface burns now
    if(!strcmp(mode,"dun")){ pcm_enc_mode=3; { const char *d=getenv("BUGSTAR_AFT_DEPTH"); if(d && atoi(d)>0) portal_depth=atoi(d); } enter_dungeon_flag=1; return; }                            // (aftermath test) go through the door; BUGSTAR_AFT_DUN=seal|inv|board says what happens down there
    if(!strcmp(mode,"encounter")){ pcm_enc_mode=3; pcm_clk=3599; pcm_enc_force=1; return; }
    if(!strcmp(mode,"save")){ pc_ws_slot_save(cur_slot); return; }
    { const char *lv=getenv("BUGSTAR_WS_LEVEL"); if(lv) level=atoll(lv); }
    if(scene!=2){ oam_hide_all(); oam_flush(); scene=2; page_back=1; space_enter(1); music(3); }
    if(!strcmp(mode,"mkill")) seed=pcm_test_setup(z);
    pcw_engage(seed,z,0);
    if(!strcmp(mode,"mkill")) strcpy(mode,"kill");
    if(!strcmp(mode,"inside")){ pcw_f.layers_up=0; pcw_f.lay_hp=wn(0); pc_ws_board(); }
    if(!strcmp(mode,"kill")){ while(pcw_f.state==WF_ON && pcw_f.shots<2000000u){ pcw_f.fire_cd=0; pcw_f.overheat=0; pcw_f.heat=0; wf_fire(&pcw_f,0,-1); } if(pcw_f.state==WF_WON) pcw_victory(); }
}
#include "pc_missions.inc"
#undef cosf
#undef sinf
#undef sqrtf
#undef atan2f
#undef fmod
#undef floor
#undef log10
#undef fabsf
