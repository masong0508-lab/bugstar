// ================================================================= SPACE: flight, pirates, stations, the galaxy map
// Every star system has a sun, one planet you can land on, and a wireframe trading station.
// Before the Ant King falls you can only fly between the six story zones; after that the whole galaxy opens,
// and an intergalactic jump takes you to a new cluster of stars, forever.

static const u8 good_base[8]={8,18,34,55,80,110,65,190};
static const u8 eco_mul[4][8]={{55,65,80,110,130,150,120,120},{120,110,105,70,60,120,110,95},{130,120,110,105,110,60,75,110},{105,50,70,90,115,120,100,80}};

static u32 lseed;
static int lr(int n){ lseed = lseed*1664525u + 1013904223u; return (int)((lseed>>16)%(u32)n); }
static u32 sys_id(int i){ return !home_cluster() ? amix(galaxy_seed,i) : (u32)i + 1; }      // the star's 32-bit name code
static void galaxy_str(char *b, int full){                              // the galaxy number is 128 bits: small ones print in decimal as before, big ones in hex (their top two words)
    static const char hx[]="0123456789ABCDEF"; u32 gw[NGH+1]; gw[0]=galaxy_no; for(int i=0;i<NGH;i++) gw[i+1]=galaxy_hi[i]; int top=NGH; while(top>0 && !gw[top]) top--;
    if(top==0){ unum(b,gw[0]+1u); return; }
    int n=0; while(b[n]) n++;
    for(int w=top; w>=top-1; w--) for(int k=28;k>=0;k-=4) b[n++]=hx[(gw[w]>>k)&15];
    b[n]=0; if(full && top>1){ scat(b," ("); ncat(b,top+1); scat(b," WORDS)"); }
}
static int sys_dist(int a, int b){ int dx=sys[a].x-sys[b].x, dy=sys[a].y-sys[b].y; return isqrt(dx*dx+dy*dy); }
static void gen_cluster(void){
    clus_refold();
    lseed = amix(galaxy_seed ^ 0x5bd1e995, 99);
    int start=0;
    if(home_cluster()){                                                  // only the home cluster has the six story stars
        static const u8 pos[6][2]={{20,74},{52,52},{86,80},{120,54},{156,82},{196,60}};
        static const u8 eco[6]={0,0,3,1,3,2};
        for(int k=0;k<6;k++){ sys[k].x=pos[k][0]; sys[k].y=pos[k][1]; sys[k].kind=k; sys[k].eco=eco[k]; }
        start=6;
    }
    for(int i=start;i<NSYS;i++){
        for(int t=0;t<60;t++){
            sys[i].x=8+lr(208); sys[i].y=24+lr(100); int ok=1;
            for(int j=0;j<i;j++){ int dx=sys[i].x-sys[j].x, dy=sys[i].y-sys[j].y; if(dx*dx+dy*dy<15*15){ ok=0; break; } }
            if(ok) break;
        }
        sys[i].kind=6; sys[i].eco=lr(4);
    }
}
// ---- the post-Ant-King campaign: five Tech Hives feed the Technobeast
static int hive_done=0, hive_card=0, floor_msg_t=0;
static const char *const hive_nm[5]={"THE SCRAP HIVE","THE BATTERY MOON","THE GEAR FOREST","THE SILICON SWAMP","THE MOTHERBOARD"};
static const u8 hive_floors[5]={3,5,7,9,12};
static int hive_active(void); static void show_hive_card(void);
static int hive_sys(void){ return 8+hive_done*5; }
static int is_hive(int i){ return hive_active() && i==hive_sys(); }
static int sys_dlv(int i){ return sys[i].kind<6 ? sys[i].kind : 6 + cluster_no*4 + sys_dist(i,0)/40 + (is_hive(i) ? 4+hive_done*3 : 0); }
static void sys_name(int i, char *b){ if(sys[i].kind<6) scpy(b,tint_name[sys[i].kind]); else if(is_hive(i)) scpy(b,hive_nm[hive_done]); else name_of(sys_id(i),b); }
static int can_jump(int i){ if(i==cur_sys) return 0; if(sys[i].kind<6) return sys[i].kind<zones_open; return ant_king; }
// ---- planets and moons: every uncharted star has 1 or more planets, and each planet can have moons you can land on and mine.
// A few galaxies are CROWDED: every star there has 6 to 16 planets. (story stars and the hives keep their one world)
#define DENSE_ODDS 400                                        // 1 galaxy in 400 is crowded
static int galaxy_dense(void){ u32 w=galaxy_no; for(int i=0;i<NGH;i++) w|=galaxy_hi[i]; if(!w) return 0;
    u32 h=hmix(galaxy_no^hmix(galaxy_hi[0]^hmix(galaxy_hi[1]^hmix(galaxy_hi[2]^0xD3A5E000u^galaxy_seed))));
    for(int i=3;i<NGH;i++) if(galaxy_hi[i]) h=hmix(h^galaxy_hi[i]^(u32)i*0x9E3779B1u);
    return h%DENSE_ODDS==0; }
static int sys_nplan(int i){ if(sys[i].kind<6 || is_hive(i)) return 1;
    u32 h=amix(galaxy_seed^0x504C414Eu,i); int r=(int)(h%1000u);
    if(galaxy_dense()) return 6+(int)((h>>12)%11u);                                    // 6..16 planets: more than ten about half the time
    return r<650?1 : r<860?2 : r<950?3 : r<985?4 : r<997?5 : 6+(int)((h>>12)%3u); }      // most stars: 1 planet; a very few: 6 to 8
static int sys_nmoon(int i, int p){ if(sys[i].kind<6 || is_hive(i)) return 0;
    int r=(int)(amix(galaxy_seed^0x4D4F4F4Eu^((u32)p*0x9E3779B1u),i)%100u); return r<40?0 : r<70?1 : r<88?2 : r<96?3 : 4; }
static int body_count(int i){ int n=0, np=sys_nplan(i); for(int p=0;p<np;p++) n+=1+sys_nmoon(i,p); return n; }
static void body_at(int i, int k, int *pp, int *pm){ int np=sys_nplan(i); for(int p=0;p<np;p++){ int nm=sys_nmoon(i,p); if(k<=nm){ *pp=p; *pm=k; return; } k-=nm+1; } *pp=0; *pm=0; }

// where you are when you land: the planet of the current system
static void land_on_current(void){
    if(sys[cur_sys].kind<6 || cur_pl>=sys_nplan(cur_sys) || cur_moon>sys_nmoon(cur_sys,cur_pl)) cur_pl=cur_moon=0;    // a stale body after a load or a jump: the first planet
    if(sys[cur_sys].kind<6){ endless=0; zone=sys[cur_sys].kind; zone_cleared=(cleared_mask>>zone)&1; story_dna(zone); }
    else { endless=1; world_no=sys_id(cur_sys)^body_salt(); w_dlv=sys_dlv(cur_sys); make_world(); if(is_hive(cur_sys)) scpy(w_name,hive_nm[hive_done]); zone_cleared=0; }
}

// ---------------------------------------------------------------- the ship in space
#define PR 3000                     // planet radius
#define VCY 64                      // middle of the view
#define FOCS 160
static s32 shx, shy, shz; static int yaw, pitch, speed, laser_cool, laser_t, sh_regen, space_msg_t, hit_t;
static s32 stx_=2500, sty_=600, stz_=-9000;     // the station
static s16 bf[3], br[3], bu[3];     // forward, right, up (2.14)
static s16 sun[3]={-9000,6000,-11000};
EW static s16 star_dir[96*3];
EW static s32 dust[20*3];
typedef struct { s32 x,y,z; s16 f[3]; s16 hp, maxhp; s8 alive, cool, fire_t, team, tgt, slot; } Pirate;   // team 0: your wingmen, 1: enemies. tgt: who it shoots (-1 = you)
#define NPIR 64                     // up to 10 wingmen and 50 enemies at once
#define MAXWING 10
static int wing_n=0, battle_on=0, battle_kills=0, battle_req=0;
EW static Pirate pir[NPIR];
typedef struct { s32 x,y,z; s16 vx,vy,vz; s16 life; u8 col; } SPart;
#define NSPP 48
EW static SPart spp[NSPP];
typedef struct { s32 x,y,z; u8 alive, good; } Can;
// rocks in a belt around each planet: crash into them and it hurts, shoot them to mine crystals and amber
typedef struct { s32 x,y,z; s16 vx,vy,vz, r, hp; u8 alive, seed, flash; } Ast;
#define NAST 8
EW static Ast ast[NAST];
static s32 svx_, svy_, svz_; static int yawv=0, pitv=0, roll_=0, sshake=0, crash_cd=0;
EW static Can cans[4];
static char smsg[40];
static void space_say(const char *t){ int n=0; while(t[n]&&n<39){ smsg[n]=t[n]; n++; } smsg[n]=0; space_msg_t=90; }

static void norm3(s16 *v){ s32 l=isqrt((u32)(v[0]*v[0]+v[1]*v[1]+v[2]*v[2])); if(l>0){ v[0]=v[0]*16384/l; v[1]=v[1]*16384/l; v[2]=v[2]*16384/l; } }
static void basis(void){
    s32 sy=sinx(yaw>>2), cy=cosx(yaw>>2), sp=sinx(pitch>>2), cp=cosx(pitch>>2);
    bf[0]=(sy*cp)>>14; bf[1]=sp; bf[2]=(cy*cp)>>14;
    br[0]=cy; br[1]=0; br[2]=-sy;
    bu[0]=(-sy*sp)>>14; bu[1]=cp; bu[2]=(-cy*sp)>>14;
}
// world point -> camera; returns 0 if behind
static int cam3(s32 x, s32 y, s32 z, s32 *cx, s32 *cy, s32 *cz){
    s32 rx=x-shx, ry=y-shy, rz=z-shz;
    *cx=(s32)(((long long)rx*br[0]+(long long)ry*br[1]+(long long)rz*br[2])>>14);
    *cy=(s32)(((long long)rx*bu[0]+(long long)ry*bu[1]+(long long)rz*bu[2])>>14);
    *cz=(s32)(((long long)rx*bf[0]+(long long)ry*bf[1]+(long long)rz*bf[2])>>14);
    return *cz>20;
}
static int sproj(s32 x, s32 y, s32 z, int *sx, int *sy, s32 *zz){
    s32 cx,cy,cz; if(!cam3(x,y,z,&cx,&cy,&cz)) return 0;
    s32 px=120+(s32)(((long long)cx*FOCS)/cz), py=VCY-(s32)(((long long)cy*FOCS)/cz);
    if(px<-4000||px>4000||py<-4000||py>4000) return 0;
    *sx=px; *sy=py; if(zz) *zz=cz; return 1;
}
static void dot2(volatile u16 *pg, int x, int y, int c){ if(x>=0&&x<240&&y>=0&&y<128) pix(pg,x,y,c); }
static void line2(volatile u16 *pg, int x0, int y0, int x1, int y1, int c){
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(n>400) return; if(!n){ dot2(pg,x0,y0,c); return; }
    s32 fx=x0<<16, fy=y0<<16, sx=(dx<<16)/n, sy=(dy<<16)/n;
    for(int i=0;i<=n;i++){ dot2(pg,(fx+0x8000)>>16,(fy+0x8000)>>16,c); fx+=sx; fy+=sy; }
}
static void wire(volatile u16 *pg, const s32 *v, int nv, const u8 *e, int ne, int c){
    int sx[16], sy[16]; u8 ok[16];
    for(int i=0;i<nv;i++) ok[i]=sproj(v[i*3],v[i*3+1],v[i*3+2],&sx[i],&sy[i],0);
    for(int i=0;i<ne;i++){ int a=e[i*2], b=e[i*2+1]; if(ok[a]&&ok[b]) line2(pg,sx[a],sy[a],sx[b],sy[b],c); }
}

// ---------------------------------------------------------------- a round, lit, turning planet
// 128x128 sphere lookup, sea/land, polar ice, a drifting cloud layer, a soft terminator and a hazy rim.
// Cost is kept flat when the planet is close: big discs are drawn in 2x2 (then 4x2) blocks.
static int pl_rot=0, pl_tex=0;
IWRAM static void planet_fill(volatile u16 *pg, int cx, int cy, int r, s32 Lx, s32 Ly, s32 Lz, int rot, const u8 *noise){
    static const u8 b4[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
    const u8 *cloud = pl_noise + 16384;                           // the cloud layer sits after the two surfaces
    int crot = rot + (frames>>5) + 40;                            // clouds drift slowly over the ground
    int ys = r>64 ? 2 : 1, cs = r>150 ? 2 : 1;
    int y0=cy-r, y1=cy+r; if(y0<0) y0=0; if(y1>127) y1=127;
    for(int y=y0;y<=y1;y+=ys){
        int dy=y-cy; int w=isqrt((u32)(r*r-dy*dy));
        int c0=(cx-w)>>1, c1=(cx+w)>>1; if(c0<0) c0=0; if(c1>119) c1=119; if(c0>c1) continue;
        int ky=((dy+r)*64)/r; if(ky<0) ky=0; if(ky>127) ky=127;
        s32 kx=(((c0*2-cx+r)<<16)/r)*64, ks=(((2*cs)<<16)/r)*64;
        const u8 *bz=b4+(y&3)*4;
        volatile u16 *row=pg+y*120;
        int ny=(64-ky)*4;
        for(int c=c0;c<=c1;c+=cs, kx+=ks){
            int kxx=kx>>16; if(kxx<0) kxx=0; if(kxx>127) kxx=127;
            int k=(ky<<7)|kxx, nz=sph_z[k], ix;
            if(nz<10){ ix=48; }
            else {
                int nx=(kxx-64)*4;
                s32 lit=(nx*Lx + ny*Ly - nz*Lz)>>14;                  // about -256..256
                int v=sph_v[k], u=sph_u[k];
                int t=noise[(v<<7)|((u+rot)&127)];                    // 0..15, 8+ is land
                int base = t>=8 ? 50+t*4 : 14+t*3;                    // land is bright, sea is dark
                if(v<4 || v>59) base=100+t;                           // polar ice
                int cl=cloud[(v<<7)|((u+crot)&127)];
                if(cl>10) base += ((cl-9)*(124-base))>>3;             // clouds whiten whatever is below
                int d=lit+40; if(d<0) d=0; if(d>256) d=256;           // a soft edge between day and night
                int sh=((base*d)>>8)+4; if(sh>127) sh=127;
                int lv=(((sh*113)>>7)+bz[c&3])>>4; if(lv>7) lv=7; ix=40+lv;
                if(nz<90 && lit>-50 && ((90-nz)>>2)>bz[c&3]) ix=48;   // haze thickens toward the limb on the lit side
            }
            u16 vv=ix|(ix<<8); row[c]=vv; if(cs==2 && c<119) row[c+1]=vv;
        }
        if(ys==2 && y+1<=y1){ volatile u16 *r2=row+120; for(int c=c0;c<=c1;c++) r2[c]=row[c]; }
    }
}

// ---------------------------------------------------------------- setting up a system
static void build_space_palette(void){
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    const u16 *t = sys[cur_sys].kind<6 ? &tint_pal[sys[cur_sys].kind*6] : w_tint;
    for(int i=0;i<8;i++){                                   // 8 shades from the planet's 5 colours
        int f=i*4*16/7, a=f>>4, fr=f&15; if(a>=4){ a=3; fr=16; }
        u16 c0=t[a], c1=t[a+1];
        int r=((c0&31)*(16-fr)+(c1&31)*fr)>>4, g=(((c0>>5)&31)*(16-fr)+((c1>>5)&31)*fr)>>4, b=(((c0>>10)&31)*(16-fr)+((c1>>10)&31)*fr)>>4;
        BG_PAL[40+i]=r|(g<<5)|(b<<10);
    }
    BG_PAL[48]=t[5]|0x0421;                                  // atmosphere
    BG_PAL[49]=rgb15(255,255,230); BG_PAL[50]=rgb15(255,220,120); BG_PAL[51]=rgb15(220,120,40);
    BG_PAL[52]=rgb15(120,255,120); BG_PAL[53]=rgb15(255,80,70); BG_PAL[54]=rgb15(220,230,240); BG_PAL[55]=rgb15(90,220,255); BG_PAL[56]=rgb15(90,100,120);
    BG_PAL[57]=rgb15(16,20,34); BG_PAL[58]=rgb15(60,80,120);
}
static int beast_stage=0, beast_fight=0, ambush_t=0, no_ambush=0, dawn_free=0, chase_cam=1; static u64 b3_hp, b3_max;
static int in_lair=0, tech_lv=0, beast_kills=0, beast_charge=0, beast_fire=0, beast_t=0, lair_out=0; static u64 beast_hp, beast_max; static char beast_dmg_txt[16]; static int beast_dmg_t=0;
static int hive_active(void){ return beast_stage>=3 && hive_done<5; }
static int hive_here(void){ return endless && hive_active() && cur_sys==hive_sys(); }
static int hive_boss_floor(void){ return hive_here() && depth_no==hive_floors[hive_done]; }
static void hive_kill(void){ hive_done++; hive_card=hive_done+1; }
static int space_danger(void){ return in_lair ? (beast_fight==1 ? 1+level/3 : beast_fight==2 ? 4+tech_lv*2+level/3 : 8 + tech_lv*2 + beast_kills*6 + level/3) : sys_dlv(cur_sys) + level/4; }
static void spawn_pirates(int n){
    for(int i=0;i<NPIR && n;i++) if(!pir[i].alive){ Pirate *p=&pir[i];
        s16 d[3]={(s16)(lr(32768)-16384),(s16)(lr(16384)-8192),(s16)(lr(32768)-16384)}; norm3(d);
        int dist=7000+lr(3000);
        p->x=shx+((d[0]*dist)>>14); p->y=shy+((d[1]*dist)>>14); p->z=shz+((d[2]*dist)>>14);
        p->f[0]=-d[0]; p->f[1]=-d[1]; p->f[2]=-d[2]; p->hp=p->maxhp=10+space_danger()*5; p->alive=1; p->cool=60; p->fire_t=0; p->team=1; p->tgt=-1; n--; }
}
static void boom(s32 x, s32 y, s32 z, int n, int col); static void space_say(const char *t);
static int team_count(int t){ int n=0; for(int i=0;i<NPIR;i++) if(pir[i].alive && pir[i].team==t) n++; return n; }
static void spawn_wing(void){                                   // your hired wingmen form up around you
    int k=0; for(int i=0;i<NPIR && k<wing_n;i++) if(!pir[i].alive){ Pirate *p=&pir[i];
        p->team=0; p->slot=k; p->alive=1; p->hp=p->maxhp=30+laser_lv*8+shield_lv*6; p->cool=30+k*7; p->fire_t=0; p->tgt=-2;
        p->x=shx-((bf[0]*(700+(k/2)*450))>>14)+((br[0]*((k&1)?1:-1)*(500+(k/2)*300))>>14); p->y=shy-((bf[1]*(700+(k/2)*450))>>14); p->z=shz-((bf[2]*(700+(k/2)*450))>>14)+((br[2]*((k&1)?1:-1)*(500+(k/2)*300))>>14);
        p->f[0]=bf[0]; p->f[1]=bf[1]; p->f[2]=bf[2]; k++; }
}
static void start_battle(int n){                                 // a fleet battle: up to 50 enemy ships, spread out ahead of you
    int room=NPIR-team_count(0)-team_count(1); if(n>room) n=room; if(n>50) n=50; if(n<1) return;
    for(int i=0;i<NPIR && n;i++) if(!pir[i].alive){ Pirate *p=&pir[i];
        s16 d[3]={(s16)(bf[0]+lr(12000)-6000),(s16)(bf[1]+lr(6000)-3000),(s16)(bf[2]+lr(12000)-6000)}; norm3(d);
        int dist=9000+lr(7000); p->x=shx+((d[0]*dist)>>14); p->y=shy+((d[1]*dist)>>14); p->z=shz+((d[2]*dist)>>14);
        p->f[0]=-d[0]; p->f[1]=-d[1]; p->f[2]=-d[2]; p->hp=p->maxhp=10+space_danger()*4; p->alive=1; p->cool=60+lr(90); p->fire_t=0; p->team=1; p->tgt=-1; n--; }
    battle_on=1; battle_kills=0; char b[40]; scpy(b,"FLEET BATTLE! "); ncat(b,team_count(1)); scat(b," ENEMY SHIPS"); space_say(b); sfx(6);
}
static void ship_dies(Pirate *p, int by_you){
    p->alive=0; boom(p->x,p->y,p->z,24,50);
    if(p->team==0){ if(wing_n>0) wing_n--; space_say("WINGMAN DOWN! HIRE MORE AT A STATION"); sfx(4); return; }
    int g = by_you ? 30+space_danger()*15 : 10+space_danger()*5; gold+=g; if(by_you) gain_xp(10+space_danger()*3); else gain_xp(3+space_danger());
    if(battle_on) battle_kills++;
    if(battle_on && team_count(1)==0){ battle_on=0; int bonus=100+battle_kills*(20+space_danger()*4); gold+=bonus; fame+=10+battle_kills; char b[40]; scpy(b,"BATTLE WON! BONUS +"); ncat(b,bonus); scat(b,"G"); space_say(b); sfx(6); return; }
    if(by_you){ char b[32]; scpy(b,"ENEMY DOWN! +"); ncat(b,g); scat(b,"G"); space_say(b); }
    else if(lr(2)==0){ static const char *const wq[6]={"WINGMAN: GOT ONE!","WINGMAN: SPLASH ONE!","WINGMAN: THAT ONE WAS RUDE.","WINGMAN: FOR THE BUG!","WINGMAN: TOO EASY.","WINGMAN: DID YOU SEE THAT?"}; space_say(wq[lr(6)]); }
    if(lr(3)==0) for(int k=0;k<4;k++) if(!cans[k].alive){ cans[k].alive=1; cans[k].good=255; cans[k].x=p->x; cans[k].y=p->y; cans[k].z=p->z; break; }
}
static void space_enter(int from_planet){
    lseed = galaxy_seed ^ 0x1234567 ^ frames;
    for(int i=0;i<96;i++){ s16 *d=&star_dir[i*3]; d[0]=lr(32768)-16384; d[1]=lr(32768)-16384; d[2]=lr(32768)-16384; norm3(d); }
    for(int i=0;i<60;i++) dust[i]=lr(1600)-800;
    norm3(sun);
    for(int i=0;i<NPIR;i++) pir[i].alive=0; for(int i=0;i<NSPP;i++) spp[i].life=0; for(int i=0;i<4;i++) cans[i].alive=0;
    for(int i=0;i<NAST;i++){ Ast *a=&ast[i]; int ang=lr(256), rad=5600+lr(1500); a->x=(cosx(ang)*rad)>>14; a->z=(sinx(ang)*rad)>>14; a->y=lr(1200)-600;
        a->vx=lr(7)-3; a->vy=lr(3)-1; a->vz=lr(7)-3; a->r=140+lr(220); a->hp=a->r/12; a->alive=1; a->seed=lr(256); a->flash=0; }
    svx_=svy_=svz_=0; yawv=pitv=roll_=0; sshake=0;
    if(sys[cur_sys].kind==6){ world_no=sys_id(cur_sys)^body_salt(); w_dlv=sys_dlv(cur_sys); make_world(); if(is_hive(cur_sys)) scpy(w_name,hive_nm[hive_done]); }
    build_space_palette();
    { u32 sid=sys_id(cur_sys)^body_salt(); pl_tex = (sid&1)<<13; pl_rot=sid*7; }
    if(from_planet){ shx=0; shy=900; shz=-(PR*3); yaw=0; pitch=0; speed=0; space_say("LAUNCHED. B: THRUST  R: BRAKE  L: BURN"); }
    else { shx=0; shy=0; shz=-(PR*5); yaw=0; pitch=0; speed=20; }
    int danger=sys_dlv(cur_sys);
    in_lair=0;
    battle_on=0; basis(); spawn_wing();
    if(battle_req){ battle_req=0; start_battle(12+lr(10)+danger*2+cluster_no*3); }
    else if(!from_planet && ant_king && lr(100) < 10+danger) start_battle(10+lr(10+danger*3)+cluster_no*2);   // past the Ant King, whole fleets roam
    else if(!from_planet && lr(100) < 25+danger*6) spawn_pirates(1+lr(1+danger/3>3?3:1+danger/3));
    shield = 20+shield_lv*20; if(hull<1) hull=100;
    ambush_t=0;
    if(!no_ambush && ((beast_stage==0 && (cleared_mask&1)) || (beast_stage==1 && (cleared_mask&4)) || (beast_stage==2 && ant_king))){ ambush_t=160; space_say("WARNING: SOMETHING IS COMING..."); }
    if(hive_card && !no_ambush) show_hive_card();
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
}
static void boom(s32 x, s32 y, s32 z, int n, int col){
    for(int i=0;i<NSPP && n;i++) if(spp[i].life<=0){ SPart *p=&spp[i]; p->x=x; p->y=y; p->z=z; p->vx=lr(161)-80; p->vy=lr(161)-80; p->vz=lr(161)-80; p->life=20+lr(25); p->col=col; n--; }
}


// ---------------------------------------------------------------- campaign story cards
static void story_card(const char *title, const char *const *ln, int n);
static void show_hive_card(void){
    int k=hive_card; hive_card=0;
    static const char *const c[6][5]={
      {"THE BEAST FEEDS FROM 5 TECH HIVES.","WHILE THEY LIVE. ITS WOUNDS CLOSE","BEFORE IT CAN DIE.","THE FIRST HIVE IS MARKED ON YOUR MAP.","DIVE TO ITS CORE. KILL THE OVERLORD."},
      {"THE SCRAP HIVE FALLS SILENT.","THE BEAST SCREAMS ACROSS SPACE.","ITS ARMOUR CRACKS: 2X DAMAGE.","NEXT: THE BATTERY MOON.","IT DRINKS LIGHTNING THERE."},
      {"THE BATTERY MOON GOES DARK.","THE BEAST IS RUNNING LOW ON POWER.","ITS ARMOUR CRACKS AGAIN.","NEXT: THE GEAR FOREST.","ITS SHELL IS GROWN THERE."},
      {"THE GEAR FOREST STOPS TURNING.","NO MORE NEW SHELL FOR THE BEAST.","ITS ARMOUR IS FALLING APART.","NEXT: THE SILICON SWAMP.","ITS BRAIN IS COPIED THERE."},
      {"THE SILICON SWAMP BURNS.","THE BEAST CAN NOT THINK AHEAD NOW.","ONE HIVE LEFT.","NEXT: THE MOTHERBOARD.","THE HEART OF ITS NETWORK."},
      {"THE MOTHERBOARD IS DEAD!","THE BEAST IS ALONE.","NOTHING CAN HEAL IT NOW.","ITS ARMOUR IS GONE: 32X DAMAGE.","GO FINISH THE TECHNOBEAST!"}};
    static const char *const ttl[6]={"CHAPTER 1: THE TECH HIVES","HIVE 1 OF 5 DESTROYED","HIVE 2 OF 5 DESTROYED","HIVE 3 OF 5 DESTROYED","HIVE 4 OF 5 DESTROYED","ALL 5 HIVES DESTROYED"};
    story_card(ttl[k-1], c[k-1], 5);
}

// ---------------------------------------------------------------- the cockpit
static const s8 scan_pts[32][2]={{44,0},{43,3},{40,5},{35,7},{29,9},{22,10},{15,11},{7,12},{0,12},{-7,12},{-15,11},{-22,10},{-29,9},{-35,7},{-40,5},{-43,3},{-44,0},{-43,-3},{-40,-5},{-35,-7},{-29,-9},{-22,-10},{-15,-11},{-7,-12},{0,-12},{7,-12},{15,-11},{22,-10},{29,-9},{35,-7},{40,-5},{43,-3}};
static void blip(volatile u16 *pg, s32 x, s32 y, s32 z, int c){
    s32 cx,cy,cz; cam3(x,y,z,&cx,&cy,&cz);
    s32 rng=16000; if(cx>rng||cx<-rng||cz>rng||cz<-rng) return;
    int bx=120+cx*44/rng, by=144-cz*12/rng, h=-(cy*10/rng); if(h>10) h=10; if(h<-10) h=-10;
    for(int i=0;i!=h;i+= h>0?1:-1) if(by+i>=129&&by+i<159) pix(pg,bx,by+i,c);
    if(by+h>=129&&by+h<159){ pix(pg,bx,by+h,c); pix(pg,bx+1,by+h,c); }
}
static void cockpit(volatile u16 *pg){
    for(int i=128*120;i<160*120;i++) pg[i]=57|(57<<8);
    for(int i=128*120;i<129*120;i++) pg[i]=58|(58<<8);
    for(int i=0;i<32;i++) pix(pg,120+scan_pts[i][0],144+scan_pts[i][1],58);
    pix(pg,120,144,54); pix(pg,121,144,54);
    blip(pg,0,0,0,in_lair?53:48); if(!in_lair) blip(pg,stx_,sty_,stz_,55);
    for(int i=0;i<NPIR;i++) if(pir[i].alive) blip(pg,pir[i].x,pir[i].y,pir[i].z,pir[i].team?53:52);
    for(int i=0;i<4;i++) if(cans[i].alive) blip(pg,cans[i].x,cans[i].y,cans[i].z,50);
    if(!in_lair) for(int i=0;i<NAST;i++) if(ast[i].alive) blip(pg,ast[i].x,ast[i].y,ast[i].z,56);
    // bars on the left: shield, hull, fuel; speed on the right
    static const char *const lab[3]={"SHLD","HULL","FUEL"}; int val[3]={shield,hull,fuel}, mx[3]={20+shield_lv*20,100,70}, col[3]={55,53,50};
    for(int k=0;k<3;k++){ text(pg,4,131+k*9,lab[k],58,1); rect(pg,24,132+k*9,40,3,56); int w=val[k]*40/mx[k]; if(w<0) w=0; if(w>40) w=40; rect(pg,24,132+k*9,w,3,col[k]); }
    text(pg,180,131,HELD(K_L)&&fuel>0?"BURN!":"SPEED",HELD(K_L)&&fuel>0?53:58,1); rect(pg,180,140,52,3,56); { int w=speed*52/ship_top_speed(); if(w>52) w=52; rect(pg,180,140,w,3,52); }
    char b[24]; sys_name(cur_sys,b); b[12]=0; text(pg,180,148,b,54,1);
}


// ---------------------------------------------------------------- THE TECHNOBEAST: one trillion HP, and its damage stays between visits
static void lair_leave(void); static void hyperspace(void); static void beast_save(void); static void trophy_save(void);
static void fmt_big(u64 v, char *b){
    static const char *const suf[7]={"","K","M","B","T","QA","QI"};
    int e=0; u64 d=1; while(e<6 && v>=d*1000){ d*=1000; e++; }
    u32 whole=(u32)(v/d); b[0]=0; ncat(b,whole);
    if(e){ u32 fr=(u32)((v%d)*10/d); scat(b,"."); ncat(b,fr); scat(b,suf[e]); }
}
static u64 beast_damage(int *core){
    u64 d=laser_lv*5+2+ship_gun(); for(int t=0;t<tech_lv;t++) d*=4;
    d = d*(u64)(10+d_dmg)/10;
    { int um = 100 + (eqc.it && eqc.rar==R_UNIQUE && eqc.sub==11 ? 100 : 0) + ((tr_uq>>11)&1 ? 50 : 0) + tr_n[3]*5;   // the Techno Shard, and trophies
      d = d*(u64)um/100; }
    d <<= hive_done;                                  // every dead hive cracks its armour more
    if(*core) d*=3;
    if(*core && data_cores>=8) d*=8;                  // Bolt says his name: the heart opens
    return d;
}
static int beast_sc(void){ return beast_fight==1 ? 1 : beast_fight==2 ? 2 : 3; }     // size in thirds
static void story_card(const char *title, const char *const *ln, int n){
    volatile u16 *pg=(volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0xA000:0));
    rect(pg,8,24,224,112,57); rect(pg,8,24,224,1,53); rect(pg,8,135,224,1,53);
    text(pg,120-textlen(title)*2,30,title,53,1);
    for(int i=0;i<n;i++) text(pg,120-textlen(ln[i])*2,48+i*12,ln[i],54,1);
    text(pg,84,124,"A: CONTINUE",55,1);
    wait_release(); do { vsync(); read_keys(); } while(!HIT(K_A)); wait_release();
}
static void beast_shot(void){
    int sx,sy; s32 z; if(!sproj(0,0,0,&sx,&sy,&z) || z>30000) return;
    int r=(int)(1700L*beast_sc()/3*FOCS/z); if(r<4) r=4;
    int dx=sx-120, dy=sy-VCY; if(dx<0) dx=-dx; if(dy<0) dy=-dy;
    if(dx>r || dy>r) return;
    int core = dx<r/3 && dy<r/3;
    u64 d=beast_damage(&core); if(d>beast_hp) d=beast_hp;
    if(beast_fight==3){ u64 fl=b3_max/5*(u64)(5-hive_done);         // living hives heal it back up to here
        if(beast_hp-d < fl){ d = beast_hp>fl ? beast_hp-fl : 0;
            if(!floor_msg_t){ floor_msg_t=150; space_say(hive_done<5?"ITS HIVES HEAL IT! DESTROY A TECH HIVE":""); } } }
    if(floor_msg_t) floor_msg_t--;
    beast_hp-=d;
    s32 k=z-1500; boom(shx+((bf[0]*k)>>14),shy+((bf[1]*k)>>14),shz+((bf[2]*k)>>14), core?8:3, core?49:51);
    if(beast_fight==3){ b3_hp=beast_hp; gold += (1+tech_lv*2)*(1+beast_kills); } sfx(1);
    scpy(beast_dmg_txt, core?"CORE -":"-"); char nb[12]; fmt_big(d,nb); scat(beast_dmg_txt,nb); beast_dmg_t=30;
    if(!beast_hp && beast_fight<3){
        boom(0,0,0,40,49); sfx(9); beast_stage=beast_fight; lair_out=1;
        int g = beast_fight==1 ? 300 : 3000; gold+=g; gain_xp(beast_fight==1 ? 60 : 400);
        if(beast_fight==1){ static const char *const t[5]={"IT BREAKS APART... AND FLEES.","YOU GRAB A PIECE OF ITS TECH.","STATIONS CAN NOW BUILD TECH CORES.","EACH ONE MAKES YOUR LASER 4X","STRONGER AGAINST THE BEAST."}; story_card("THE HATCHLING ESCAPES",t,5); }
        else { static const char *const t[5]={"IT FLEES TOWARD THE ANTHILL.","IT WAS FEEDING TECH TO THE ANT KING!","THAT IS WHY THE ANTS GOT SO STRONG.","BEAT THE ANT KING","TO CUT THE BEAST OFF."}; story_card("THE BEAST ESCAPES AGAIN",t,5); }
        return;
    }
    if(!beast_hp){
        if(!beast_kills){ if(data_cores>=8){ static const char *const t[5]={"THE MOUTH FALLS APART.","INSIDE IT: A SHIP. OLD. WHOLE. SINGING.","BOLT'S VOICE COMES FROM EVERYWHERE AT ONCE.","\"I REMEMBER ALL OF IT. THANK YOU.\"","\"NOW LET ME CARRY YOU HOME.\""}; story_card("THE DAWN",t,5); dawn_free=1; }
                          else { static const char *const t[4]={"THE TECHNOBEAST FALLS!","BUT ITS CORE STILL HUMS...","IT WILL REBUILD ITSELF","TEN TIMES STRONGER."}; story_card("VICTORY",t,4); } }
        beast_kills++; int g=100000*beast_kills; gold+=g; gain_xp(2000*beast_kills);
        boom(0,0,0,40,49); sfx(9);
        b3_max = b3_max>=1000000000000000000ULL ? b3_max : b3_max*10; b3_hp=b3_max; beast_max=b3_max; beast_hp=b3_hp;
        lair_out=1; char b[40]; scpy(b,"TECHNOBEAST DOWN! +"); ncat(b,g); scat(b,"G"); space_say(b);
    }
}
static void beast_update(void){
    beast_t++;
    int phase = 3 - (int)(beast_hp*4/(beast_max?beast_max:1)); if(phase<0) phase=0;
    int dg = space_danger();
    if(beast_fire) beast_fire--;
    if(beast_charge){ if(--beast_charge==0){ beast_fire=8; sfx(4);
            int ch = speed>=30 ? 30 : 75;               // fly fast to dodge
            if(lr(100)<ch){ int dm=(beast_fight==3?10:5)+dg*2+phase*6; if(shield>=dm) shield-=dm; else { hull-=dm-shield; shield=0; } hit_t=8; } } }
    else if(beast_fight==1){ if(beast_t%50==0){ beast_fire=4; sfx(4); if(lr(100)<45){ int dm=2+dg; if(shield>=dm) shield-=dm; else { hull-=dm-shield; shield=0; } hit_t=6; } } }
    else if(beast_t % (100-phase*18) == 0){ beast_charge=40; space_say("THE BEAST CHARGES ITS BEAM! SPEED UP!"); }
    if(beast_fight>1 && beast_t % (420-phase*80) == 0 && !(beast_fight==2 && phase<2)){ int alive=team_count(1); if(alive<(beast_fight==2?1:1+phase)) spawn_pirates(1); }
}
static void beast_draw(volatile u16 *pg){
    s32 v[32*3]; int sx[32], sy[32]; u8 ok[32];
    int a=(beast_t*2)&255, b2=(beast_t*3+64)&255; s32 ca=cosx(a), sa=sinx(a), cb=cosx(b2), sb=sinx(b2);
    static const s8 oct[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    static const s8 cub[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    int pulse = beast_charge ? 1100 + (beast_charge&4)*60 : 900;
    for(int i=0;i<32;i++){ s32 x,y,z;
        if(i<6){ x=oct[i][0]*pulse; y=oct[i][1]*pulse; z=oct[i][2]*pulse; }
        else if(i<14){ x=cub[i-6][0]*1250; y=cub[i-6][1]*1250; z=cub[i-6][2]*1250; }
        else if(i<20){ x=oct[i-14][0]*3300; y=oct[i-14][1]*3300; z=oct[i-14][2]*3300; }
        else { int k=((i-20)*256/12)&255; s32 rx=(cosx(k)*2900)>>14, rz=(sinx(k)*2900)>>14;   // a ring that turns the other way
               x=(rx*cb-rz*sb)>>14; z=(rx*sb+rz*cb)>>14; y=(z*sinx(24))>>14; z=(z*cosx(24))>>14; }
        if(i<20){ s32 t=(x*ca-z*sa)>>14; z=(x*sa+z*ca)>>14; x=t; }
        { int sc=beast_sc(); x=x*sc/3; y=y*sc/3; z=z*sc/3; }
        v[i*3]=x; v[i*3+1]=y; v[i*3+2]=z;
        ok[i]=sproj(x,y,z,&sx[i],&sy[i],0);
    }
    #define E(p,q,c) if(ok[p]&&ok[q]) line2(pg,sx[p],sy[p],sx[q],sy[q],c)
    static const u8 ce[]={0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7};
    for(int i=0;i<24;i+=2) E(6+ce[i],6+ce[i+1],53);
    static const u8 oe[]={0,2,0,3,0,4,0,5,1,2,1,3,1,4,1,5,2,4,2,5,3,4,3,5};
    int cc = beast_charge ? ((beast_t&4)?49:53) : ((beast_t&16)?50:51);
    for(int i=0;i<24;i+=2) E(oe[i],oe[i+1],cc);
    for(int s=0;s<6;s++) for(int k=0;k<8;k++){ int x=cub[k][0], y=cub[k][1], z=cub[k][2];
        if((s==0&&x>0)||(s==1&&x<0)||(s==2&&y>0)||(s==3&&y<0)||(s==4&&z>0)||(s==5&&z<0)) E(14+s,6+k,51); }
    for(int i=0;i<12;i++) E(20+i,20+(i+1)%12,55);
    for(int i=0;i<12;i+=3) E(20+i,6+(i/3)*2,56);
    if(beast_fire && ok[0]){ int cx=(sx[0]+sx[1])/2, cy=(sy[0]+sy[1])/2;
        for(int w=-2;w<=2;w++) line2(pg,cx+w,cy,120+w*6,127,w?53:49); }
    #undef E
}
static void beast_hud(volatile u16 *pg){
    char b[24]; text(pg,4,2,beast_fight==1?"HATCHLING":"TECHNOBEAST",53,1);
    if(beast_fight==3 && beast_kills){ scpy(b,"MK "); ncat(b,beast_kills+1); text(pg,52,2,b,50,1); }
    fmt_big(beast_hp,b); scat(b," HP"); text(pg,236-textlen(b)*4,2,b,54,1);
    rect(pg,4,10,232,3,56); u32 w=(u32)(beast_hp*232/(beast_max?beast_max:1)); if(!w && beast_hp) w=1; rect(pg,4,10,w,3,53);
    if(beast_dmg_t){ beast_dmg_t--; text(pg,120-textlen(beast_dmg_txt)*2,18,beast_dmg_txt,beast_dmg_txt[0]=='C'?49:50,1); }
    else if(!(frames&64)) text(pg,64,18,"SELECT: WARP OUT",56,1);
}
static void lair_enter(void){
    music(4); no_ambush=1; space_enter(0); no_ambush=0; in_lair=1; ambush_t=0;
    beast_fight = beast_stage>=2 ? 3 : beast_stage+1;
    if(beast_fight==3){ if(beast_stage<3){ static const char *const t[5]={"WITHOUT ITS KING. THE BEAST WAKES.","IT ATE A WHOLE MOON. 1 TRILLION HP.","YOU CANNOT WIN TODAY. HIT IT AND WARP OUT.","GET STRONGER. AND COME BACK.","ITS WOUNDS WILL NEVER HEAL."}; story_card("THE TECHNOBEAST - COMPLETE",t,5); hive_card=1; hive_done=0; }
        beast_stage=3; beast_max=b3_max; beast_hp=b3_hp; }
    else if(beast_fight==1){ beast_max=beast_hp=600; static const char *const t[5]={"A SIGNAL FROM THE DARK...","A TINY MACHINE BUG BUILT FROM SCRAP.","IT EATS SHIPS TO GROW BIGGER.","SHOOT ITS GLOWING CORE!","STOP IT WHILE IT IS SMALL."}; story_card("THE TECHNOBEAST - HATCHLING",t,5); }
    else { beast_max=beast_hp=40000; static const char *const t[5]={"IT IS BACK. AND MUCH BIGGER.","IT ATE A WHOLE PIRATE FLEET.","NOW IT HAS A BEAM.","WHEN IT CHARGES. HOLD B","TO FLY FAST AND DODGE."}; story_card("THE TECHNOBEAST - GROWN",t,5); } beast_t=1; beast_charge=0; beast_fire=0;
    for(int i=0;i<NPIR;i++) pir[i].alive=0;
    shx=0; shy=0; shz=beast_fight==1?-7000:beast_fight==2?-11000:-15000; yaw=0; pitch=0; speed=0;
    BG_PAL[57]=rgb15(34,12,16); BG_PAL[58]=rgb15(130,50,60);
    space_say(beast_fight==3?"THE TECHNOBEAST AWAKENS":"IT ATTACKS!");
}
static void lair_leave(void){ int f=beast_fight; in_lair=0; music(3); hyperspace(); no_ambush=1; space_enter(0); no_ambush=0; if(hive_card) show_hive_card(); space_say(f<3 && beast_stage>=f ? "IT GOT AWAY... FOR NOW" : "WARPED OUT. COME BACK STRONGER"); do_save(); }

static void ship_hit(int dm, int crash){            // shields first, then the hull; the cockpit shakes
    dm=dm*ship_armour()/100; if(dm<1) dm=1;
    if(shield>=dm) shield-=dm; else { hull-=dm-shield; shield=0; }
    hit_t=6; sshake = crash ? 14 : 8; if(crash){ crash_cd=30; sfx(12); } else sfx(4);
}
// ---------------------------------------------------------------- one frame of flying
// ---- choose which world of the system to land on (only asked when the star has more than one)
static int body_menu(void){
    int n=body_count(cur_sys), np=sys_nplan(cur_sys);
    if(n<=1){ cur_pl=cur_moon=0; return 1; }
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0:0xA000));
    int cur=0, top=0, dirty=1; { int k=0; for(int p=0;p<np;p++) for(int m=0;m<=sys_nmoon(cur_sys,p);m++){ if(p==cur_pl && m==cur_moon) cur=k; k++; } }
    wait_release();
    while(1){
        if(dirty){ dirty=0;
            for(int i=0;i<240*160/2;i++) pg[i]=0;
            char b[40]; sys_name(cur_sys,b); b[18]=0; text(pg,4,3,b,C_GOLD,1);
            scpy(b,""); ncat(b,np); scat(b,np==1?" PLANET   ":" PLANETS   "); ncat(b,n); scat(b," WORLDS"); text(pg,4,11,b,C_GREY,1);
            if(cur<top) top=cur; if(cur>=top+9) top=cur-8;
            for(int r=0;r<9 && top+r<n;r++){ int p,m; body_at(cur_sys,top+r,&p,&m); char l[24]; scpy(l,m?"   MOON ":"PLANET "); ncat(l,p+1); if(m){ scat(l,"."); ncat(l,m); }
                int y=24+r*10; if(top+r==cur) rect(pg,2,y-1,236,9,C_DGREY);
                text(pg,8,y,l,top+r==cur?C_WHITE:C_GREY,1); if(m) text(pg,150,y,"ORE RICH",C_GOLD,1); }
            if(top>0) text(pg,226,22,"^",C_CYAN,1); if(top+9<n) text(pg,226,112,"v",C_CYAN,1);
            rect(pg,0,132,240,28,57); text(pg,4,138,"UP/DOWN: PICK A WORLD",C_GREY,1); text(pg,4,148,"A: LAND   B: BACK",C_GREEN,1);
        }
        vsync(); read_keys();
        if(HIT(K_UP) && cur>0){ cur--; dirty=1; sfx(7); }
        if(HIT(K_DOWN) && cur<n-1){ cur++; dirty=1; sfx(7); }
        if(HIT(K_A)){ body_at(cur_sys,cur,&cur_pl,&cur_moon); wait_release(); return 1; }
        if(HIT(K_B)){ wait_release(); return 0; }
    }
}
static int space_frame(void){   // 0 keep flying, 1 land, 2 open the galaxy map
    // steer, throttle
    // the ship has weight: turning ramps up and settles, it banks into turns, and it drifts before the engines catch up
    // ---- flight: real momentum. engines push along the nose, mass resists, fins settle the turn, wings bite in air
    int tr=ship_turn(); int ty = (HELD(K_LEFT) ? -40 : HELD(K_RIGHT) ? 40 : 0)*tr/100, tp = (HELD(K_UP) ? -28 : HELD(K_DOWN) ? 28 : 0)*tr/100;
    { int dp=ship_damp()+ship_mass()/8; yawv += (ty-yawv)/dp; pitv += (tp-pitv)/dp; }
    yaw=(yaw+(yawv>>2))&1023; pitch+=pitv>>2; if(pitch>230) pitch=230; if(pitch<-230) pitch=-230;
    roll_ += (yawv*(2+ship.wsize+(ship.wings?1:0)) - roll_)/6;
    basis();
    { s32 c=cosx(roll_>>2), s_=sinx(roll_>>2); for(int k=0;k<3;k++){ s32 r=br[k], u=bu[k]; br[k]=(r*c+u*s_)>>14; bu[k]=(u*c-r*s_)>>14; } }
    s32 pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4;
    int in_air = !in_lair && pd < PR*5/2;                                          // close to the planet there is air
    { int a16=ship_accel16(); int burn = HELD(K_L) && fuel>0;
      if(HELD(K_B)){ int a=a16*(burn?2:1); svx_ += (bf[0]*a)>>18; svy_ += (bf[1]*a)>>18; svz_ += (bf[2]*a)>>18; if(burn && (frames&15)==0) fuel--; }
      if(HELD(K_R)){ int k=6+ship.fins*2; svx_ -= svx_*k/64; svy_ -= svy_*k/64; svz_ -= svz_*k/64; }           // retro thrusters
      // drag: a whisper in space, a hand in the air. in the air the wings pull the ship round to where the nose points
      if(in_air){ int g = 10 + ship.wsize*6 + (ship.wings?6:0); s32 sp3=isqrt((u32)(svx_*svx_+svy_*svy_+svz_*svz_));
          svx_ += ((((bf[0]*sp3)>>14) - svx_)*g)>>7; svy_ += ((((bf[1]*sp3)>>14) - svy_)*g)>>7; svz_ += ((((bf[2]*sp3)>>14) - svz_)*g)>>7;
          svx_ -= svx_/48; svy_ -= svy_/48; svz_ -= svz_/48; }
      else { svx_ -= svx_/512; svy_ -= svy_/512; svz_ -= svz_/512; if(!HELD(K_B)&&!HELD(K_R)){ int g=ship.fins+1; s32 sp3=isqrt((u32)(svx_*svx_+svy_*svy_+svz_*svz_));
          svx_ += ((((bf[0]*sp3)>>14) - svx_)*g)>>9; svy_ += ((((bf[1]*sp3)>>14) - svy_)*g)>>9; svz_ += ((((bf[2]*sp3)>>14) - svz_)*g)>>9; } }
      s32 vmax = ship_top_speed()*3*(burn?5:4)/4; s32 sp3=isqrt((u32)(svx_*svx_+svy_*svy_+svz_*svz_));
      if(sp3>vmax){ svx_=svx_*vmax/sp3; svy_=svy_*vmax/sp3; svz_=svz_*vmax/sp3; sp3=vmax; }
      speed = sp3/3; }
    shx+=svx_; shy+=svy_; shz+=svz_;
    if(crash_cd) crash_cd--; if(sshake) sshake--;
    pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4;
    { s32 lim = in_lair ? 2400 : PR+300;                       // the planet's air, or the beast's hull: you bounce off
      if(pd<lim){ s32 k=lim*64/(pd?pd:1); shx=shx*k/64; shy=shy*k/64; shz=shz*k/64;
          s32 vn=(svx_*shx+svy_*shy+svz_*shz)/(pd?pd:1);
          if(vn<0){ svx_-=2*vn*shx/(pd?pd:1); svy_-=2*vn*shy/(pd?pd:1); svz_-=2*vn*shz/(pd?pd:1); }
          if(speed>12 && !crash_cd){ int dm = in_lair ? 10+speed/3 : speed/6; ship_hit(dm,1); space_say(in_lair?"YOU RAMMED THE BEAST!":"TOO FAST! THE AIR SLAMS YOU"); }
          { int ns = in_lair ? 0 : (speed>10?10:speed); if(speed>0){ svx_=svx_*ns/speed; svy_=svy_*ns/speed; svz_=svz_*ns/speed; } speed=ns; } } }
    // the station's hull: dock slowly, or crash
    { s32 dx=shx-stx_, dy=shy-sty_, dz=shz-stz_;
      if(!in_lair && dx>-640&&dx<640&&dy>-640&&dy<640&&dz>-640&&dz<640){ s32 l=isqrt((u32)((dx>>2)*(dx>>2)+(dy>>2)*(dy>>2)+(dz>>2)*(dz>>2)))<<2; if(l<1) l=1;
          if(l<620){ shx=stx_+dx*620/l; shy=sty_+dy*620/l; shz=stz_+dz*620/l; svx_=dx*speed/l; svy_=dy*speed/l; svz_=dz*speed/l;
              if(speed>14 && !crash_cd){ ship_hit(speed/4,1); space_say("CRASH! SLOW DOWN TO DOCK"); } speed/=3; svx_/=3; svy_/=3; svz_/=3; } } }
    // rocks and pirate hulls
    for(int i=0;i<NAST;i++){ Ast *a=&ast[i]; if(!a->alive) continue; a->x+=a->vx; a->y+=a->vy; a->z+=a->vz; if(a->flash) a->flash--;
        if(in_lair) continue; s32 dx=shx-a->x, dy=shy-a->y, dz=shz-a->z, rr=a->r+110;
        if(dx>-rr&&dx<rr&&dy>-rr&&dy<rr&&dz>-rr&&dz<rr){ s32 l=isqrt((u32)(dx*dx+dy*dy+dz*dz)); if(l<1) l=1;
            if(l<rr){ shx=a->x+dx*rr/l; shy=a->y+dy*rr/l; shz=a->z+dz*rr/l; svx_=dx*(speed+6)/l; svy_=dy*(speed+6)/l; svz_=dz*(speed+6)/l;
                a->vx-=dx*3/l; a->vz-=dz*3/l; if(!crash_cd){ ship_hit(2+speed/5,1); boom(a->x+dx*a->r/l,a->y+dy*a->r/l,a->z+dz*a->r/l,8,56); } speed=speed/2; svx_/=2; svy_/=2; svz_/=2; } } }
    for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i]; if(!p->alive || !p->team) continue; s32 dx=shx-p->x, dy=shy-p->y, dz=shz-p->z;
        if(dx>-260&&dx<260&&dy>-260&&dy<260&&dz>-260&&dz<260 && !crash_cd){ ship_hit(6,1); p->hp-=8+speed/3; boom(p->x,p->y,p->z,10,53);
            svx_+=dx/8; svy_+=dy/8; svz_+=dz/8; p->x-=dx/2; p->y-=dy/2; p->z-=dz/2; space_say("RAMMED A PIRATE!");
            if(p->hp<=0) ship_dies(p,1); } }
    s32 sdx=shx-stx_, sdy=shy-sty_, sdz=shz-stz_; s32 sd=isqrt((u32)((sdx>>4)*(sdx>>4)+(sdy>>4)*(sdy>>4)+(sdz>>4)*(sdz>>4)))<<4;
    int can_land = !in_lair && pd < PR+2200, can_dock = !in_lair && sd < 1100;
    if(++sh_regen>=20){ sh_regen=0; if(shield<20+shield_lv*20) shield++; }
    if(lair_out){ lair_out=0; lair_leave(); return 0; }
    if(ambush_t && !in_lair && --ambush_t==0){ hyperspace(); lair_enter(); return 0; }
    if(HIT(K_SEL)){ if(in_lair){ lair_leave(); return 0; } return 2; }
    if(HIT(K_START)){ chase_cam^=1; char b[40]; scpy(b, chase_cam?"CHASE VIEW. ":"COCKPIT VIEW. "); int n=0; for(int i=0;i<8;i++) n+=cargo[i]; scat(b,"CARGO "); ncat(b,n); scat(b,"/"); ncat(b,cargo_cap+ship_hold_bonus()); space_say(b); }
    // A: land, dock, or fire
    if(laser_cool) laser_cool--;
    if(HIT(K_A) && can_land){ if(body_menu()) return 1; return 0; }
    if(HIT(K_A) && can_dock){ speed=0; return 3; }
    if(HELD(K_A) && !laser_cool && !can_land && !can_dock){
        laser_cool=10-laser_lv; laser_t=4; sfx(10);
        { int best=-1; s32 bz=9000; for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i]; if(!p->alive || !p->team) continue; int sx,sy; s32 z;
            if(sproj(p->x,p->y,p->z,&sx,&sy,&z) && z<bz && sx>104 && sx<136 && sy>VCY-14 && sy<VCY+14){ bz=z; best=i; } }
          if(best>=0){ Pirate *p=&pir[best]; p->hp -= laser_lv*5+2+ship_gun() + (in_lair? tech_lv*12 : 0); boom(p->x,p->y,p->z,4,54); sfx(1);
              if(p->hp<=0){ sfx(9); ship_dies(p,1); } } }
        if(!in_lair) for(int i=0;i<NAST;i++){ Ast *a=&ast[i]; if(!a->alive) continue; int sx,sy; s32 z;
            if(!sproj(a->x,a->y,a->z,&sx,&sy,&z) || z>9000) continue; int rs=(int)(a->r*FOCS/z)+3;
            if(sx>120-rs && sx<120+rs && sy>VCY-rs && sy<VCY+rs){ a->hp -= laser_lv+1; a->flash=3; boom(shx+((bf[0]*(z-a->r))>>14),shy+((bf[1]*(z-a->r))>>14),shz+((bf[2]*(z-a->r))>>14),3,56); sfx(1);
                if(a->hp<=0){ a->alive=0; boom(a->x,a->y,a->z,20,56); sfx(13); int nc=1+(a->r>260);
                    for(int k=0;k<4 && nc;k++) if(!cans[k].alive){ cans[k].alive=1; int r=lr(10); cans[k].good = r<5?4 : r<9?3 : 7; cans[k].x=a->x+lr(200)-100; cans[k].y=a->y; cans[k].z=a->z+lr(200)-100; nc--; }
                    space_say("ROCK CRACKED! SCOOP THE MINERALS"); }
                break; } }
        if(in_lair) beast_shot();
    }
    if(in_lair) beast_update();
    // pirates
    int danger=space_danger();
    if(!in_lair && !can_land && lr(1500) < 2+danger) spawn_pirates(1);
    // ---- every ship: pick a target, fly at it, shoot. Wingmen fight enemies and form up on you when it's quiet; enemies hunt you and your wingmen
    for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i]; if(!p->alive) continue;
        if(((frames+i)&31)==0 || (p->tgt>=0 && (!pir[p->tgt].alive || pir[p->tgt].team==p->team))){     // re-pick a target now and then
            s32 bd = p->team ? (s32)(isqrt((u32)(((shx-p->x)>>4)*((shx-p->x)>>4)+((shy-p->y)>>4)*((shy-p->y)>>4)+((shz-p->z)>>4)*((shz-p->z)>>4)))<<4) : 14000;
            p->tgt = p->team ? -1 : -2;                                                   // -1: you. -2: nobody (wingmen form up)
            for(int j=0;j<NPIR;j++){ Pirate *q=&pir[j]; if(!q->alive || q->team==p->team) continue; s32 ex=q->x-(p->team?p->x:shx), ey=q->y-(p->team?p->y:shy), ez=q->z-(p->team?p->z:shz);   // wingmen go for the enemy nearest YOU
                s32 dd=isqrt((u32)((ex>>4)*(ex>>4)+(ey>>4)*(ey>>4)+(ez>>4)*(ez>>4)))<<4; if(!p->team){ s32 px_=q->x-shx, pz_=q->z-shz; if((px_<0?-px_:px_)+(pz_<0?-pz_:pz_)>16000) continue; }
                if(dd<bd){ bd=dd; p->tgt=j; } } }
        s32 tx,ty,tz;
        if(p->tgt==-1){ tx=shx; ty=shy; tz=shz; }
        else if(p->tgt>=0){ tx=pir[p->tgt].x; ty=pir[p->tgt].y; tz=pir[p->tgt].z; }
        else { int k=p->slot; tx=shx-((bf[0]*(700+(k/2)*450))>>14)+((br[0]*((k&1)?1:-1)*(500+(k/2)*300))>>14); ty=shy-((bf[1]*(700+(k/2)*450))>>14)+((bu[1]*(k%3-1)*150)>>14);
               tz=shz-((bf[2]*(700+(k/2)*450))>>14)+((br[2]*((k&1)?1:-1)*(500+(k/2)*300))>>14);
               p->x+=(tx-p->x)/12; p->y+=(ty-p->y)/12; p->z+=(tz-p->z)/12; for(int k2=0;k2<3;k2++) p->f[k2]+=(bf[k2]-p->f[k2])>>3; norm3(p->f);
               if(p->cool) p->cool--; if(p->fire_t) p->fire_t--; continue; }
        s16 d[3]; s32 dx=tx-p->x, dy=ty-p->y, dz=tz-p->z; s32 l=isqrt((u32)((dx>>4)*(dx>>4)+(dy>>4)*(dy>>4)+(dz>>4)*(dz>>4)))<<4; if(l<1) l=1;
        if(p->team && p->tgt==-1 && l>40000 && !battle_on){ p->alive=0; continue; }
        d[0]=dx*16384/l; d[1]=dy*16384/l; d[2]=dz*16384/l;
        if(l<1200){ d[0]=-d[0]; d[1]=-d[1]+8000; d[2]=-d[2]; }          // swerve away instead of ramming
        for(int k=0;k<3;k++) p->f[k]+= (d[k]-p->f[k])>>4;
        norm3(p->f);
        int ps = p->team ? 34+danger*2 : 60+laser_lv*4; if(!p->team){ s32 ex=p->x-shx, ez=p->z-shz; if((ex<0?-ex:ex)+(ez<0?-ez:ez)>3000 && ps<speed*3+40) ps=speed*3+40; }
        p->x+=(p->f[0]*ps)>>14; p->y+=(p->f[1]*ps)>>14; p->z+=(p->f[2]*ps)>>14;   // (wingmen keep up with you when they are far)
        if(p->cool) p->cool--; if(p->fire_t) p->fire_t--;
        s32 aim=(p->f[0]*d[0]+p->f[1]*d[1]+p->f[2]*d[2])>>14;
        if(!p->cool && l<7000 && aim>15500){ p->cool=40+lr(30); p->fire_t=4;
            s32 ex=p->x-shx, ez=p->z-shz; if((ex<0?-ex:ex)+(ez<0?-ez:ez)<6000) sfx(p->team?4:10);
            if(p->tgt==-1){ if(lr(100)<55) ship_hit(4+danger*2,0); }
            else if(lr(100)<45){ Pirate *q=&pir[p->tgt]; q->hp -= p->team ? 4+danger*2 : 6+laser_lv*4+ship_gun(); boom(q->x,q->y,q->z,3,p->team?53:52);
                if(q->hp<=0) ship_dies(q,0); } }
    }
    // canisters: fly into them to scoop the cargo
    for(int k=0;k<4;k++) if(cans[k].alive){ s32 dx=shx-cans[k].x, dy=shy-cans[k].y, dz=shz-cans[k].z;
        if(dx>-500&&dx<500&&dy>-500&&dy<500&&dz>-500&&dz<500){ cans[k].alive=0; int n=0; for(int i=0;i<8;i++) n+=cargo[i];
            if(n<cargo_cap){ int g=cans[k].good<8 ? cans[k].good : lr(8); cargo[g]++; char b[32]; scpy(b,"SCOOPED "); scat(b,goods[g]); space_say(b); sfx(2); } else space_say("CARGO HOLD FULL"); } }
    for(int i=0;i<NSPP;i++){ SPart *p=&spp[i]; if(p->life<=0) continue; p->life--; p->x+=p->vx; p->y+=p->vy; p->z+=p->vz; }
    if(hull<=0){ if(in_lair){ in_lair=0; no_ambush=1; space_enter(0); no_ambush=0; } hull=100; for(int i=0;i<8;i++) cargo[i]=0; gold/=2; for(int i=0;i<NPIR;i++) pir[i].alive=0; battle_on=0;
        shx=0; shy=0; shz=-(PR*5); yaw=0; pitch=0; speed=10; svx_=svy_=svz_=0; space_say("SHIP WRECKED! CARGO LOST"); sfx(9); }
    // ---- draw
    int sv_yaw=yaw, sv_pit=pitch;
    if(sshake && opt_shake){ yaw=(yaw+((frames&1)?sshake:-sshake)/2)&1023; pitch+=((frames&2)?sshake:-sshake)/3; basis();
        s32 c=cosx(roll_>>2), s_=sinx(roll_>>2); for(int k=0;k<3;k++){ s32 r=br[k], u=bu[k]; br[k]=(r*c+u*s_)>>14; bu[k]=(u*c-r*s_)>>14; } }
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
    { static u32 zero=0; *(volatile u32*)0x040000D4=(u32)&zero; *(volatile u32*)0x040000D8=(u32)pg; *(volatile u32*)0x040000DC=(120*128/2)|(2<<23)|(1<<26)|(1u<<31); }
    for(int i=0;i<96;i++){ s16 *d=&star_dir[i*3]; s32 z=(d[0]*bf[0]+d[1]*bf[1]+d[2]*bf[2])>>14; if(z<2000) continue;
        s32 x=(d[0]*br[0]+d[1]*br[1]+d[2]*br[2])>>14, y=(d[0]*bu[0]+d[1]*bu[1]+d[2]*bu[2])>>14;
        int sx=120+x*FOCS/z, sy=VCY-y*FOCS/z; dot2(pg,sx,sy, i<20?5 : i<50?4 : 3); }
    { s32 z=(sun[0]*bf[0]+sun[1]*bf[1]+sun[2]*bf[2])>>14;
      if(z>2000){ s32 x=(sun[0]*br[0]+sun[1]*br[1]+sun[2]*br[2])>>14, y=(sun[0]*bu[0]+sun[1]*bu[1]+sun[2]*bu[2])>>14;
          int sx=120+x*FOCS/z, sy=VCY-y*FOCS/z;
          for(int dy=-12;dy<=12;dy++) for(int dx=-12;dx<=12;dx++){ int d2=dx*dx+dy*dy; if(d2>144) continue; int c = d2<30?49 : d2<70?50 : ((dx+dy)&1)?51:0; if(c) dot2(pg,sx+dx,sy+dy,c); } } }
    // the planet
    { s32 cx,cy,cz; if(!in_lair && cam3(0,0,0,&cx,&cy,&cz) && cz>PR/3){
        int r=(int)(((long long)PR*FOCS)/cz), px=120+(s32)(((long long)cx*FOCS)/cz), py=VCY-(s32)(((long long)cy*FOCS)/cz);
        if(r<1200 && px+r>0 && px-r<240 && py+r>0 && py-r<128){
            s32 Lx=(sun[0]*br[0]+sun[1]*br[1]+sun[2]*br[2])>>14, Ly=(sun[0]*bu[0]+sun[1]*bu[1]+sun[2]*bu[2])>>14, Lz=(sun[0]*bf[0]+sun[1]*bf[1]+sun[2]*bf[2])>>14;
            if(r<2) r=2; planet_fill(pg,px,py,r,Lx,Ly,Lz,(pl_rot+(frames>>3))&127,pl_noise+pl_tex); } } }
    // flying dust gives a sense of speed
    for(int i=0;i<20;i++){ s32 *d=&dust[i*3];
        for(int k=0;k<3;k++){ s32 rel=d[k]-(k==0?shx:k==1?shy:shz); if(rel>800) d[k]-=1600; if(rel<-800) d[k]+=1600; }
        int sx,sy,sx2,sy2; s32 z;
        if(sproj(d[0],d[1],d[2],&sx,&sy,&z)){ if(speed>14 && sproj(d[0]-((bf[0]*speed*4)>>14),d[1]-((bf[1]*speed*4)>>14),d[2]-((bf[2]*speed*4)>>14),&sx2,&sy2,0)) line2(pg,sx,sy,sx2,sy2,56); else dot2(pg,sx,sy,56); } }
    // the station: a turning wireframe cube with a glowing dock slot, like Elite's Coriolis
    if(in_lair) beast_draw(pg);
    else { s32 v[12*3]; int a=frames*3; s32 S=420, c_=cosx(a>>2), s_=sinx(a>>2);
      static const s8 cube[8][3]={{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
      for(int i=0;i<8;i++){ s32 x=cube[i][0]*S, z=cube[i][2]*S; v[i*3]=stx_+((x*c_-z*s_)>>14); v[i*3+1]=sty_+cube[i][1]*S; v[i*3+2]=stz_+((x*s_+z*c_)>>14); }
      static const s8 slot[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
      for(int i=0;i<4;i++){ s32 x=slot[i][0]*140, z=-S; v[(8+i)*3]=stx_+((x*c_-z*s_)>>14); v[(8+i)*3+1]=sty_+slot[i][1]*70; v[(8+i)*3+2]=stz_+((x*s_+z*c_)>>14); }
      static const u8 ce[]={0,1,1,2,2,3,3,0,4,5,5,6,6,7,7,4,0,4,1,5,2,6,3,7}; static const u8 se[]={8,9,9,10,10,11,11,8};
      wire(pg,v,12,ce,12,54); wire(pg,v,12,se,4,(frames&16)?55:56); }
    // the asteroid belt: lumpy turning rocks
    if(!in_lair) for(int i=0;i<NAST;i++){ Ast *a=&ast[i]; if(!a->alive) continue;
        static const s8 oc[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        s32 v[6*3]; int ang=(frames*2+a->seed*4)&1023; s32 c=cosx(ang>>2), s_=sinx(ang>>2);
        for(int k=0;k<6;k++){ int rr=a->r*(10+((a->seed>>k)&3)*2)/14; s32 x=oc[k][0]*rr, y=oc[k][1]*rr*3/4, z=oc[k][2]*rr;
            v[k*3]=a->x+((x*c-z*s_)>>14); v[k*3+1]=a->y+y; v[k*3+2]=a->z+((x*s_+z*c)>>14); }
        static const u8 ae[]={0,2,0,3,0,4,0,5,1,2,1,3,1,4,1,5,2,4,2,5,3,4,3,5};
        wire(pg,v,6,ae,12,a->flash?49:(a->seed&1)?56:51); }
    // pirates: little wireframe fighters
    for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i]; if(!p->alive) continue;
        int col = p->team ? 53 : 52;
        { int sx,sy; s32 z; if(!sproj(p->x,p->y,p->z,&sx,&sy,&z)) continue;
          if(p->fire_t){ int tx_,ty_; if(p->tgt==-1) line2(pg,sx,sy,120+(frames&7)-4,124,col); else if(p->tgt>=0 && sproj(pir[p->tgt].x,pir[p->tgt].y,pir[p->tgt].z,&tx_,&ty_,0)) line2(pg,sx,sy,tx_,ty_,col); }
          if(z>5500){ dot2(pg,sx,sy,col); dot2(pg,sx-1,sy,col); dot2(pg,sx+1,sy,col); dot2(pg,sx,sy-1,col); continue; } }   // far away: a small mark
        s16 R[3]={p->f[2],0,(s16)-p->f[0]}; norm3(R); s16 U[3]={(s16)((p->f[1]*R[2]-p->f[2]*R[1])>>14),(s16)((p->f[2]*R[0]-p->f[0]*R[2])>>14),(s16)((p->f[0]*R[1]-p->f[1]*R[0])>>14)};
        static const s16 mdl[6][3]={{0,0,110},{-110,0,-50},{110,0,-50},{0,35,-50},{0,-18,-50},{0,0,-70}};
        s32 v[6*3]; for(int k=0;k<6;k++){ for(int a=0;a<3;a++) v[k*3+a]=(a==0?p->x:a==1?p->y:p->z)+((mdl[k][0]*R[a]+mdl[k][1]*U[a]+mdl[k][2]*p->f[a])>>14); }
        static const u8 pe[]={0,1,0,2,0,3,0,4,1,3,2,3,1,4,2,4,3,5,4,5,1,5,2,5};
        wire(pg,v,6,pe,12,col); }
    for(int k=0;k<4;k++) if(cans[k].alive){ int sx,sy; s32 z; if(sproj(cans[k].x,cans[k].y,cans[k].z,&sx,&sy,&z)){ int r=(int)(60*FOCS/z)+1; line2(pg,sx-r,sy,sx,sy-r,50); line2(pg,sx,sy-r,sx+r,sy,50); line2(pg,sx+r,sy,sx,sy+r,50); line2(pg,sx,sy+r,sx-r,sy,50); } }
    for(int i=0;i<NSPP;i++){ SPart *p=&spp[i]; if(p->life<=0) continue; int sx,sy; if(sproj(p->x,p->y,p->z,&sx,&sy,0)){ dot2(pg,sx,sy,p->life>12?49:p->col); dot2(pg,sx+1,sy,p->col); } }
    if(laser_t){ laser_t--; line2(pg,30,127,120,VCY,52); line2(pg,210,127,120,VCY,52); }
    // crosshair
    for(int k=4;k<9;k++){ dot2(pg,120+k,VCY,52); dot2(pg,120-k,VCY,52); dot2(pg,120,VCY+k,52); dot2(pg,120,VCY-k,52); }
    // the chase camera: your own ship, seen from behind, banking into turns and nosing up and down
    if(chase_cam){ ship_roll=(-roll_*2)&1023; int bob=(sinx((frames*3)&255)>>12);
        ship_draw(pg, 120 + (yawv>>2), 100 + bob + (pitv>>2), 52 + speed/6, (-(yawv*2))&1023, (40 - pitv)&1023, 0,0, 0,0,240,128);
        if(HELD(K_B)){ int fl = 3 + (frames&3); for(int k=-1;k<=1;k++) line2(pg,120+k*4+(yawv>>2),106+bob,120+k*6+(yawv>>2),106+bob+fl+(HELD(K_L)&&fuel>0?6:0),SHP_GLOW+6); }   // engine flame
        ship_roll=0; }
    if(hit_t){ hit_t--; for(int x=0;x<240;x+=3){ dot2(pg,x,0,53); dot2(pg,x,127,53); } }
    cockpit(pg);
    if(sv_yaw!=yaw || sv_pit!=pitch){ yaw=sv_yaw; pitch=sv_pit; }
    static char pb[24]; const char *pr = can_land ? "A: LAND" : can_dock ? "A: DOCK" : 0;
    if(can_land){ int bc=body_count(cur_sys); if(bc>1){ scpy(pb,"A: LAND ("); ncat(pb,bc); scat(pb," WORLDS)"); pr=pb; } }
    if(space_msg_t){ space_msg_t--; text(pg,120-textlen(smsg)*2,in_lair?28:4,smsg,54,1); }
    if(pr) text(pg,120-textlen(pr)*4,96,pr,55,2);
#ifdef TEST_FLEET
    { static int lv=0; char fb[16]; scpy(fb,"VBL "); ncat(fb,vbl_count-lv); lv=vbl_count; text(pg,200,20,fb,54,1); } { int e=0; char fb2[24]; for(int i=0;i<NPIR;i++) if(pir[i].alive&&!pir[i].team&&pir[i].tgt>=0) e++; scpy(fb2,"ENG "); ncat(fb2,e); text(pg,200,28,fb2,54,1); } { for(int i=0;i<NPIR;i++) if(pir[i].alive&&!pir[i].team){ Pirate *p=&pir[i]; s32 rx=p->x-shx, ry=p->y-shy, rz=p->z-shz; s32 zz=(rx*bf[0]+ry*bf[1]+rz*bf[2])>>14; char c3[48]; scpy(c3,"Z "); ncat(c3,zz); scat(c3," T "); ncat(c3,p->tgt); if(p->tgt>=0){ Pirate *q=&pir[p->tgt]; s32 qz=((q->x-shx)*bf[0]+(q->y-shy)*bf[1]+(q->z-shz)*bf[2])>>14; scat(c3," TZ "); ncat(c3,qz); scat(c3," A"); ncat(c3,q->alive); scat(c3," M"); ncat(c3,q->team); } text(pg,110,36,c3,54,1); break; } }
#endif
    if(in_lair) beast_hud(pg);
    else if(battle_on || team_count(0)){ char b[40]; scpy(b,"WINGMEN "); ncat(b,team_count(0)); if(battle_on){ scat(b,"   ENEMIES "); ncat(b,team_count(1)); } text(pg,120-textlen(b)*2,space_msg_t?12:4,b,battle_on?53:52,1); }
    else if(!space_msg_t && !(frames&64)) text(pg,64,4,"SELECT: GALAXY MAP",56,1);
    return 0;
}

// ---------------------------------------------------------------- hyperspace
static void hyperspace(void){
    sfx(3);
    for(int f=0;f<70;f++){
        volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
        for(int i=0;i<120*128;i++) pg[i]=0;
        for(int k=0;k<40;k++){ int a=(k*37+f*3)&255; s32 ca=sinx(a+64), sa=sinx(a); int r0=((k*53+f*f)%110)+4, r1=r0+6+f/3;
            line2(pg,120+(ca*r0>>14),VCY+(sa*r0>>15),120+(ca*r1>>14),VCY+(sa*r1>>15), k&1?55:(f>50?49:58)); }
        cockpit(pg);
        vsync(); REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1;
    }
}

// ---------------------------------------------------------------- the galaxy map (Elite-style chart)
static int galaxy_map(void){     // returns 1 if you jumped
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0:0xA000));     // draw on the page you are looking at
    int sel=cur_sys, dirty=1, rep=0;
    for(int k=0;k<6;k++) if(k<zones_open && !((cleared_mask>>k)&1) && k!=cur_sys && sys[k].kind==k){ sel=k; break; }     // start on the star you should go to next
    if(hive_active() && sel==cur_sys) sel=hive_sys();
    wait_release();
    while(1){
        if(dirty){ dirty=0;
            for(int i=0;i<240*160/2;i++) pg[i]=0;
            lseed=amix(7,98); for(int i=0;i<120;i++) pix(pg,lr(240),18+lr(110),1+lr(3));
            char b[48]; scpy(b,"GALAXY "); galaxy_str(b,1); text(pg,4,3,b,C_GOLD,1);
            { static const char hx[]="0123456789ABCDEF"; char c[48]; int n; scpy(c,"CLUSTER "); n=8; int top=NCW-1; while(top>0 && !clus[top]) top--;   // the cluster number: its top words, in hex
              if(top>1){ c[n++]='.'; c[n++]='.'; } for(int w=(top>1?1:top);w>=0;w--) for(int k=28;k>=0;k-=4) c[n++]=hx[(clus[w + (top>1?top-1:0)]>>k)&15]; c[n]=0;
              if(top>1){ scat(c," ("); ncat(c,top+1); scat(c," WORDS)"); } text(pg,4,10,c,C_GREY,1); }
            for(int i=0;i<64;i++){ int a=i*4; int r=fuel; int x=sys[cur_sys].x+(sinx(a+64)*r>>14), y=sys[cur_sys].y+(sinx(a)*r>>14); if(x>=0&&x<240&&y>=16&&y<130) pix(pg,x,y,C_DGREY); }
            for(int i=0;i<NSYS;i++){ int x=sys[i].x, y=sys[i].y, k=sys[i].kind;
                int c = k<6 ? (k<zones_open ? C_GOLD : C_DGREY) : (ant_king ? (sys[i].eco==0?C_GREEN : sys[i].eco==1?C_GREY : sys[i].eco==2?C_CYAN : C_RED) : C_DGREY);
                rect(pg,x-1,y-1,3,3,c); if(k<6){ b[0]='1'+k; b[1]=0; text(pg,x+3,y-6,b,c,1); } }
            for(int i=0;i<NSYS;i++) if(has_base(i) && (i<6 ? i<zones_open : ant_king)){ int x=sys[i].x, y=sys[i].y; rect(pg,x-3,y-3,2,2,C_RED); rect(pg,x+2,y-3,2,2,C_RED); rect(pg,x-3,y+2,2,2,C_RED); rect(pg,x+2,y+2,2,2,C_RED); }
            if(hive_active()){ int hx=sys[hive_sys()].x, hy=sys[hive_sys()].y; for(int a=0;a<256;a+=8) pix(pg,hx+(sinx(a+64)*7>>14),hy+(sinx(a)*7>>14),C_RED); text(pg,hx-8,hy+9,"HIVE",C_RED,1);
                scpy(b,"TECH HIVES LEFT: "); ncat(b,5-hive_done); text(pg,4,112,b,C_RED,1); }
            for(int k=0;k<5;k++) if((cleared_mask>>k)&1){ line2(pg,sys[k].x,sys[k].y,sys[k+1].x,sys[k+1].y,C_DGREY); }
            int x=sys[cur_sys].x, y=sys[cur_sys].y; for(int a=0;a<256;a+=16) pix(pg,x+(sinx(a+64)*5>>14),y+(sinx(a)*5>>14),C_WHITE);
            x=sys[sel].x; y=sys[sel].y; line2(pg,x-6,y,x-3,y,C_CYAN); line2(pg,x+3,y,x+6,y,C_CYAN); line2(pg,x,y-6,x,y-3,C_CYAN); line2(pg,x,y+3,x,y+6,C_CYAN);
            rect(pg,0,132,240,28,57);
            sys_name(sel,b); text(pg,4,135,b,C_WHITE,1);
            int d=sys_dist(sel,cur_sys);
            scpy(b, sys[sel].kind<6 ? "STORY ZONE  " : is_hive(sel) ? "TECH HIVE  " : "UNCHARTED  "); scat(b,eco_name[sys[sel].eco]); scat(b,"  DANGER "); ncat(b,sys_dlv(sel)); text(pg,4,143,b,C_GREY,1);
            scpy(b,"DISTANCE "); ncat(b,d/10); scat(b,"."); ncat(b,d%10); scat(b," LY   FUEL "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10);
            text(pg,4,151,b,C_GREY,1);
            if(sys[sel].kind==6 && !is_hive(sel)){ int bc=body_count(sel); if(bc>1){ scpy(b,"WORLDS "); ncat(b,bc); text(pg,236-textlen(b)*4,143,b,C_CYAN,1); } }
            if(galaxy_dense()) text(pg,4,24,"CROWDED GALAXY",C_GOLD,1);
            if(is_hive(sel)){ scpy(b,"OVERLORD ON FLOOR "); ncat(b,hive_floors[hive_done]); text(pg,236-textlen(b)*4,135,b,C_RED,1); }
            else if(has_base(sel)) text(pg,236-13*4,135,"DRONE OUTPOST",C_RED,1);
            { scpy(b,"BOLT'S MEMORY "); ncat(b,data_cores); scat(b,"/8"); text(pg,4,122-(beast_stage>=3?10:0),b,C_CYAN,1); }
            const char *st = sel==cur_sys ? "YOU ARE HERE" : !can_jump(sel) ? (sys[sel].kind<6 ? "LOCKED: BEAT THE BOSS BEFORE IT" : "LOCKED: BEAT THE ANT KING") : d>fuel ? "TOO FAR: REFUEL AT A STATION" : "A: JUMP HERE";
            text(pg,236-textlen(st)*4,151,st, (sel!=cur_sys && can_jump(sel) && d<=fuel)?C_GREEN:C_RED,1);
            if(ant_king){ text(pg,160,10,"START: NEXT CLUSTER",C_DGREY,1); text(pg,160,17,"R: DEEP JUMP",C_DGREY,1); } else text(pg,206,3,"B: BACK",C_DGREY,1);
            text(pg,4,17,"D-PAD: PICK A STAR",C_DGREY,1);
            if(beast_stage>=3) text(pg,4,122,"L: FIGHT THE TECHNOBEAST - 2.0 LY",C_RED,1);
        }
        vsync(); read_keys();
        // a tap jumps to the nearest star that way; holding keeps going
        int ddx = HELD(K_LEFT)?-1 : HELD(K_RIGHT)?1 : 0, ddy = HELD(K_UP)?-1 : HELD(K_DOWN)?1 : 0;
        if(ddx||ddy){ if(HIT(K_LEFT)||HIT(K_RIGHT)||HIT(K_UP)||HIT(K_DOWN)) rep=0; else rep++; } else rep=0;
        if((ddx||ddy) && (rep==0 || (rep>=14 && rep%7==0))){
            int best=-1, bd=1<<30;
            for(int i=0;i<NSYS;i++){ if(i==sel) continue; int dx=sys[i].x-sys[sel].x, dy=sys[i].y-sys[sel].y;
                int along = dx*ddx + dy*ddy; if(along<=0) continue;
                int side = ddx&&ddy ? 0 : (ddx ? (dy<0?-dy:dy) : (dx<0?-dx:dx));
                if(!(ddx&&ddy) && side>along*2) continue;
                int cost = dx*dx+dy*dy + side*side*3; if(cost<bd){ bd=cost; best=i; } }
            if(best>=0){ sel=best; dirty=1; sfx(7); } }
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HIT(K_L) && beast_stage>=3){
            if(fuel<20){ space_say("NEED 2.0 LY OF FUEL FOR THE LAIR"); wait_release(); return 0; }
            fuel-=20; hyperspace(); lair_enter(); wait_release(); return 1; }
        if(HIT(K_START) && ant_king){
            if(fuel<70){ space_say("NEED FULL FUEL FOR AN INTERGALACTIC JUMP"); wait_release(); return 0; }
            fuel=0; cluster_no++; for(int w=0;w<NCW;w++) if(++clus[w]) break; gen_cluster(); cur_sys=0; cur_pl=cur_moon=0; hyperspace(); space_enter(0); space_say("A NEW CLUSTER OF STARS!"); wait_release(); return 1; }
        if(HIT(K_R) && ant_king){                                                       // the deep jump: anywhere in the universe, at random
            if(fuel<70){ space_say("NEED FULL FUEL FOR A DEEP JUMP"); wait_release(); return 0; }
            fuel=0; cluster_no++; galaxy_no=rnd(); for(int k=0;k<NGH;k++) galaxy_hi[k]=rnd(); for(int w=0;w<NCW;w++) clus[w]=rnd(); gen_cluster(); cur_sys=0; cur_pl=cur_moon=0; hyperspace(); space_enter(0);
            { char b[40]; scpy(b,"DEEP JUMP! GALAXY "); galaxy_str(b,0); space_say(b); } wait_release(); return 1; }
        if(HIT(K_A) && sel!=cur_sys && can_jump(sel) && sys_dist(sel,cur_sys)<=fuel){
            s32 pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4;
            if(pd<PR*2){ space_say("TOO CLOSE TO THE PLANET TO JUMP"); wait_release(); return 0; }
            fuel-=sys_dist(sel,cur_sys); cur_sys=sel; cur_pl=cur_moon=0; visits++;
            hyperspace(); space_enter(0); char b[40]; scpy(b,"ARRIVED: "); char nm[24]; sys_name(cur_sys,nm); scat(b,nm); space_say(b);
            wait_release(); return 1; }
    }
}

// ---------------------------------------------------------------- docked at the station: trading and the shipyard
static int tech_scrap(void){ return 3*(tech_lv+1); }
static int tech_cost(void){ if(beast_stage<1) return 0; return tech_lv<30 ? 2000*(tech_lv+1)*(tech_lv+1) : 0; }
static int good_price(int g){ int e=sys[cur_sys].eco; int p=good_base[g]*eco_mul[e][g]/100; int wob=(int)((sys_id(cur_sys)*7+g*13+visits*5)%21); return p*(90+wob)/100+1; }
static void station_menu(void){
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0:0xA000));
    int tab=0, sel=0, dirty=1; wait_release(); sfx(5);
    static const char *const yard[9]={"REFUEL","REPAIR HULL","LASER UPGRADE","SHIELD UPGRADE","BIGGER CARGO BAY","TECH CORE (X4 DMG)","HIRE A WINGMAN","JOIN A FLEET BATTLE","LEAVE"};
    while(1){
        if(dirty){ dirty=0; char b[48];
            for(int i=0;i<240*160/2;i++) pg[i]=57|(57<<8);
            char nm[24]; sys_name(cur_sys,nm); scpy(b,nm); scat(b," STATION"); text(pg,6,4,b,C_GOLD,1);
            scpy(b,"G"); ncat(b,gold); text(pg,190,4,b,C_GOLD,1);
            text(pg,6,14,"MARKET",tab==0?C_WHITE:C_DGREY,1); text(pg,46,14,"SHIPYARD",tab==1?C_WHITE:C_DGREY,1); text(pg,90,14,"LOUNGE",tab==2?C_WHITE:C_DGREY,1); scpy(b,eco_name[sys[cur_sys].eco]); scat(b," WORLD"); text(pg,140,14,b,C_CYAN,1);
            if(tab==0){
                int n=0; for(int i=0;i<8;i++) n+=cargo[i];
                text(pg,14,26,"GOODS        PRICE   YOU HAVE",C_GREY,1);
                for(int i=0;i<8;i++){ if(i==sel) text(pg,6,36+i*10,">",C_CYAN,1); scpy(b,goods[i]); while(textlen(b)<13) scat(b," "); ncat(b,good_price(i)); scat(b,"G"); while(textlen(b)<21) scat(b," "); ncat(b,cargo[i]);
                    text(pg,14,36+i*10,b,i==sel?C_WHITE:C_GREY,1); }
                scpy(b,"HOLD "); ncat(b,n); scat(b,"/"); ncat(b,cargo_cap); text(pg,14,122,b,C_WHITE,1);
                text(pg,6,148,"A: BUY  B: SELL  L/R: SHIPYARD  START: LEAVE",C_CYAN,1);
            } else if(tab==2){
                // three regulars at the bar. what they say moves with the story
                static const char *const who[3]={"VESPER, OLD MOTH PILOT","HUM, A TRADER BEE","NIB, AN ANT WHO RAN AWAY"};
                const char *say1[3], *say2[3]; static char rb[48];
                say1[0] = beast_stage>=3 ? "I SAW IT EAT A FREIGHTER WHOLE. IT HUMMED A TUNE." : ant_king ? "SOMETHING WOKE UP OUT THERE. THE SKY WENT QUIET." : "THERE'S A SONG THE BEES SING. ABOUT A FALLEN STAR.";
                say2[0] = data_cores>=8 ? "YOUR TIN FRIEND. HE'S NOT A DROID, IS HE." : "STAY FAST. FAST THINGS DON'T GET EATEN.";
                { int e=sys[cur_sys].eco; say1[1] = e==1 ? "MINING WORLDS BUY FOOD AND NECTAR HIGH." : e==2 ? "TECH WORLDS PAY FOR AMBER AND CRYSTALS." : e==3 ? "HIVE WORLDS WANT MACHINES. BRING ANY YOU FIND." : "FARMING WORLDS BUY MACHINES AND MEDICINE."; }
                say2[1] = "A BIGGER HOLD PAYS FOR ITSELF IN TWO RUNS.";
                { int nb=-1, bd=99999; for(int i=0;i<NSYS;i++) if(i!=cur_sys && has_base(i) && (i<6 ? i<zones_open : ant_king)){ int d=sys_dist(i,cur_sys); if(d<bd){ bd=d; nb=i; } }
                  if(nb>=0){ char nm[24]; sys_name(nb,nm); scpy(rb,"THE DRONES ARE DIGGING ON "); scat(rb,nm); scat(rb,"."); say1[2]=rb; } else say1[2]="THE DRONES HAVE GONE QUIET. YOU DID THAT."; }
                say2[2] = outposts_done ? "THE KING IS AFRAID OF YOU. GOOD. SO WAS I." : "THE KING KEEPS KEYCARDS ON HIS DRONES. LOOK FOR THEM.";
                for(int i=0;i<3;i++){ int y=28+i*36; if(i==sel) text(pg,6,y,">",C_CYAN,1); text(pg,14,y,who[i],i==sel?C_GOLD:C_GREY,1);
                    if(i==sel){ char q[52]; scpy(q,"\""); scat(q,say1[i]); scat(q,"\""); q[51]=0; text(pg,14,y+10,q,C_WHITE,1); scpy(q,"\""); scat(q,say2[i]); scat(q,"\""); q[51]=0; text(pg,14,y+19,q,C_WHITE,1); } }
                text(pg,6,148,"UP/DOWN: TALK TO SOMEONE  L/R: MARKET  START: LEAVE",C_CYAN,1);
            } else {
                int cost[9]={ (70-fuel)*2, (100-hull)*3, laser_lv<4?laser_lv*900:0, shield_lv<8?shield_lv*1000:0, cargo_cap<50?cargo_cap*60:0, tech_cost(), wing_n<MAXWING?300+wing_n*150:0, 0, 0 };
                for(int i=0;i<9;i++){ if(i==sel) text(pg,6,26+i*10,">",C_CYAN,1); scpy(b,yard[i]); while(textlen(b)<20) scat(b," ");
                    if(i==5 && beast_stage<1) scat(b,"LOCKED"); else if(i<7){ if(cost[i]) { ncat(b,cost[i]); scat(b,"G"); if(i==5){ scat(b,"+"); ncat(b,tech_scrap()); scat(b," SCRAP"); } } else scat(b, i<2?"FULL":"MAX"); }
                    else if(i==7) scat(b, battle_req ? "READY" : "FREE");
                    text(pg,14,26+i*10,b,i==sel?C_WHITE:C_GREY,1); }
                scpy(b,"WINGMEN "); ncat(b,wing_n); scat(b,"/"); ncat(b,MAXWING); text(pg,14,108,b,C_GREEN,1);
                scpy(b,"LASER MK"); ncat(b,laser_lv); scat(b,"  SHIELD MK"); ncat(b,shield_lv); scat(b,"  CARGO "); ncat(b,cargo_cap); scat(b,"  TECH "); ncat(b,tech_lv); text(pg,14,116,b,C_WHITE,1);
                scpy(b,"HULL "); ncat(b,hull); scat(b,"%  FUEL "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10); scat(b," LY"); text(pg,14,126,b,C_WHITE,1);
                scpy(b,"TECH SCRAP "); ncat(b,scrap); text(pg,150,126,b,C_CYAN,1);
                text(pg,6,148,"A: BUY  L/R: MARKET  START: LEAVE",C_CYAN,1);
            }
        }
        vsync(); read_keys();
        int n = tab==0 ? 8 : tab==2 ? 3 : 9;
        if(HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; } if(HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; }
        if(HIT(K_R)){ tab=(tab+1)%3; sel=0; dirty=1; } if(HIT(K_L)){ tab=(tab+2)%3; sel=0; dirty=1; }
        if(HIT(K_START) || (tab==1 && HIT(K_A) && sel==8)) break;
        if(tab==0){
            int held=0; for(int i=0;i<8;i++) held+=cargo[i];
            if(HIT(K_A)){ int p=good_price(sel); if(gold>=p && held<cargo_cap){ gold-=p; cargo[sel]++; sfx(7); } dirty=1; }
            if(HIT(K_B) && cargo[sel]){ gold+=good_price(sel); cargo[sel]--; sfx(7); dirty=1; }
        } else if(HIT(K_A) && sel==6){ int c=300+wing_n*150; if(wing_n>=MAXWING) say("YOU HAVE A FULL WING"); else if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; wing_n++; sfx(6); } dirty=1;
        } else if(HIT(K_A) && sel==7){ battle_req=1; sfx(6); dirty=1;
        } else if(HIT(K_A)){
            int cost[6]={ (70-fuel)*2, (100-hull)*3, laser_lv<4?laser_lv*900:0, shield_lv<8?shield_lv*1000:0, cargo_cap<50?cargo_cap*60:0, tech_cost() };
            if(sel==5 && cost[5] && scrap<tech_scrap()){ say("NEED MORE TECH SCRAP. SCRAP GEAR AT YOUR SHIP"); }
            else if(cost[sel] && gold>=cost[sel]){ gold-=cost[sel]; if(sel==5) scrap-=tech_scrap(); sfx(6);
                if(sel==0) fuel=70; else if(sel==1) hull=100; else if(sel==2) laser_lv++; else if(sel==3){ shield_lv++; shield=20+shield_lv*20; } else if(sel==4) cargo_cap+=10; else tech_lv++; }
            dirty=1;
        }
    }
    wait_release();
}

// ---------------------------------------------------------------- the ship's own save block (after the main save)
typedef struct { u32 magic; s16 cur_sys, cluster_no, fuel, hull, shield_lv, laser_lv, cargo_cap, ant_king, cleared_mask, zones_open, visits, w_dlv; u8 cargo[8]; u32 sum; } SaveX;
EW static SaveX svx;
static u32 sx_sum(const SaveX *s){ const u8 *p=(const u8*)s; u32 h=2166136261u; for(unsigned i=0;i<sizeof(SaveX)-4;i++){ h^=p[i]; h*=16777619u; } return h; }
static void save_ext(void){
    SaveX *s=&svx; memset(s,0,sizeof(SaveX)); s->magic=0x50494853;
    s->cur_sys=cur_sys; s->cluster_no=cluster_no; s->fuel=fuel; s->hull=hull; s->shield_lv=shield_lv; s->laser_lv=laser_lv; s->cargo_cap=cargo_cap;
    s->ant_king=ant_king; s->cleared_mask=cleared_mask; s->zones_open=zones_open; s->visits=visits; s->w_dlv=w_dlv; memcpy(s->cargo,cargo,8);
    s->sum=sx_sum(s); const u8 *p=(const u8*)s; for(unsigned i=0;i<sizeof(SaveX);i++) SRAM[slot_base(cur_slot)+SX_OFF+i]=p[i];
    beast_save(); trophy_save();
    { u32 a[NCW0+10]; a[0]=0x36444441u; a[1]=galaxy_no; for(int w=0;w<NCW0;w++) a[2+w]=clus[w]; a[NCW0+2]=relic_bits; a[NCW0+3]=(u32)weird_seen; a[NCW0+4]=(u32)relic_gold; a[NCW0+5]=mark_perm; a[NCW0+6]=(u32)wing_n; a[NCW0+7]=(u32)form|((u32)forms_open<<4)|((u32)tool_held<<8)|((u32)form_body<<12)|(tools_owned<<16); a[NCW0+8]=form_col|((u32)(form_t>0?1:0)<<16);
      u32 h=2166136261u; for(int i=0;i<NCW0+9;i++){ h^=a[i]; h*=16777619u; } a[NCW0+9]=h; sram_write(slot_base(cur_slot)+0x900,a,sizeof a); }
    { u32 g[NGH+2]; g[0]=0x324C4147u; for(int i=0;i<NGH;i++) g[1+i]=galaxy_hi[i]; u32 h=2166136261u; for(int i=0;i<NGH+1;i++){ h^=g[i]; h*=16777619u; } g[NGH+1]=h; sram_write(slot_base(cur_slot)+0x9C0,g,sizeof g); }   // the upper galaxy words: their own block (ends at 0x9EC), so older saves still load
    { u32 g[3]; g[0]=0x59444F42u; g[1]=(u32)(cur_pl*8+cur_moon); g[2]=(2166136261u^g[0])*16777619u; g[2]=(g[2]^g[1])*16777619u; sram_write(slot_base(cur_slot)+0x9EC,g,sizeof g); }   // which planet or moon you are on
    { int xo=xt_base(cur_slot); u32 hd[3]; hd[0]=0x32554C43u; hd[1]=(u32)(NCW-NCW0); hd[2]=fnv(&clus[NCW0],(NCW-NCW0)*4); sram_write(xo,hd,12); sram_write(xo+12,&clus[NCW0],(NCW-NCW0)*4); }   // the upper cluster words, straight from RAM (no big buffer on the stack)
}
static void addr_load(void){ u32 a[NCW0+10]; sram_read(slot_base(cur_slot)+0x900,a,sizeof a); u32 h9=2166136261u, h7=2166136261u, h=2166136261u, h5=2166136261u; for(int i=0;i<NCW0+9;i++){ h9^=a[i]; h9*=16777619u; } for(int i=0;i<NCW0+7;i++){ h7^=a[i]; h7*=16777619u; if(i<NCW0+6){ h^=a[i]; h*=16777619u; } if(i<NCW0+5){ h5^=a[i]; h5*=16777619u; } }
    wing_n=0; form=0; form_t=0; forms_open=1; tools_owned=0; tool_held=0; form_body=0; form_col=0;
    relic_bits=0; weird_seen=0; relic_gold=0; mark_perm=0; for(int k=0;k<NMK;k++) mark_t[k]=0; for(int w=0;w<NCW;w++) clus[w]=0; galaxy_no=0; for(int k=0;k<NGH;k++) galaxy_hi[k]=0;
    if(a[0]==0x36444441u && a[NCW0+9]==h9){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; mark_perm=a[NCW0+5]; wing_n=(int)a[NCW0+6]; if(wing_n>MAXWING||wing_n<0) wing_n=0;
        u32 f=a[NCW0+7]; form=f&3; if(form>2) form=0; forms_open=((f>>4)&7)|1; tool_held=(f>>8)&7; form_body=(f>>12)%3; tools_owned=(f>>16)&0xFF; form_col=(u16)a[NCW0+8]; if((a[NCW0+8]>>16)&1) form=FORM_BUG; }   // a timed shape does not survive a reload
    else if(a[0]==0x35444441u && a[NCW0+7]==h7){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; mark_perm=a[NCW0+5]; wing_n=(int)a[NCW0+6]; if(wing_n>MAXWING||wing_n<0) wing_n=0; }
    else if(a[0]==0x34444441u && a[NCW0+6]==h){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; mark_perm=a[NCW0+5]; }
    else if(a[0]==0x33444441u && a[NCW0+5]==h5){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; }
    else if(a[0]==0x52444441u && a[4]==(a[0]^a[1]*31^a[2]*131^a[3]*1031^0x5A5A)){ galaxy_no=a[1]; clus[0]=a[2]; clus[1]=a[3]; }   // last version's save
    else clus[0]=(u32)cluster_no;    // an older save: its cluster count becomes its address
    { int xo=xt_base(cur_slot); u32 hd[3]; sram_read(xo,hd,12);                          // the upper cluster words: all zero unless a good block is there
      if(a[0]==0x36444441u && hd[0]==0x32554C43u && hd[1]==(u32)(NCW-NCW0)){ sram_read(xo+12,&clus[NCW0],(NCW-NCW0)*4); if(fnv(&clus[NCW0],(NCW-NCW0)*4)!=hd[2]) for(int w=NCW0;w<NCW;w++) clus[w]=0; } }
    { u32 g[NGH+2]; sram_read(slot_base(cur_slot)+0x9C0,g,sizeof g); int boff=0x9E0;      // new saves keep 9 upper words at 0x9C0 and the body at 0x9EC; the last version kept 3 words and the body at 0x9E0
      u32 hn=2166136261u; for(int i=0;i<NGH+1;i++){ hn^=g[i]; hn*=16777619u; } u32 ho=2166136261u; for(int i=0;i<4;i++){ ho^=g[i]; ho*=16777619u; }
      if(a[0]==0x36444441u){
          if(g[0]==0x324C4147u && g[NGH+1]==hn){ for(int i=0;i<NGH;i++) galaxy_hi[i]=g[1+i]; boff=0x9EC; }
          else if(g[0]==0x58474C47u && g[4]==ho){ galaxy_hi[0]=g[1]; galaxy_hi[1]=g[2]; galaxy_hi[2]=g[3]; } }
      { u32 b[3]; sram_read(slot_base(cur_slot)+boff,b,sizeof b); u32 bh=(2166136261u^0x59444F42u)*16777619u; bh=(bh^b[1])*16777619u; cur_pl=cur_moon=0;
        if(b[0]==0x59444F42u && b[2]==bh && b[1]<128u){ cur_pl=(int)(b[1]>>3); cur_moon=(int)(b[1]&7); } } }
}
typedef struct { u32 magic, hp_lo, hp_hi, mx_lo, mx_hi; u16 tech, kills, stage, hives; u32 sum; } SaveB;
static void beast_defaults(void){ hive_done=0; tech_lv=0; beast_kills=0; b3_max=1000000000000ULL; b3_hp=b3_max; beast_stage = ant_king ? 2 : (cleared_mask&4) ? 1 : 0; }
#define SB_OFF 0xA00
static void beast_load(void){
    SaveB b; sram_read(slot_base(cur_slot)+SB_OFF,&b,sizeof b);
    u32 h=2166136261u; const u8 *p=(const u8*)&b; for(unsigned i=0;i<sizeof b-4;i++){ h^=p[i]; h*=16777619u; }
    if(b.magic!=0x32534542 || b.sum!=h){ beast_defaults(); return; }
    b3_hp=((u64)b.hp_hi<<32)|b.hp_lo; b3_max=((u64)b.mx_hi<<32)|b.mx_lo; tech_lv=b.tech; beast_kills=b.kills; beast_stage=b.stage; hive_done=b.hives>5?5:b.hives;
}
static void beast_save(void){
    SaveB b; memset(&b,0,sizeof b); b.magic=0x32534542; b.hp_lo=(u32)b3_hp; b.hp_hi=(u32)(b3_hp>>32); b.mx_lo=(u32)b3_max; b.mx_hi=(u32)(b3_max>>32); b.tech=tech_lv; b.kills=beast_kills; b.stage=beast_stage; b.hives=hive_done;
    u32 h=2166136261u; const u8 *p=(const u8*)&b; for(unsigned i=0;i<sizeof b-4;i++){ h^=p[i]; h*=16777619u; } b.sum=h;
    sram_write(slot_base(cur_slot)+SB_OFF,&b,sizeof b);
}
typedef struct { u32 magic; u16 n[4], uq, scrap; u32 sum; } SaveTr;
#define ST_OFF 0xB00
static void trophy_save(void){
    SaveTr t; memset(&t,0,sizeof t); t.magic=0x504F5254; for(int i=0;i<4;i++) t.n[i]=tr_n[i]; t.uq=tr_uq; t.scrap=scrap>65000?65000:scrap;
    u32 h=2166136261u; const u8 *p=(const u8*)&t; for(unsigned i=0;i<sizeof t-4;i++){ h^=p[i]; h*=16777619u; } t.sum=h;
    sram_write(slot_base(cur_slot)+ST_OFF,&t,sizeof t);
}
static void trophy_load(void){
    SaveTr t; sram_read(slot_base(cur_slot)+ST_OFF,&t,sizeof t);
    u32 h=2166136261u; const u8 *p=(const u8*)&t; for(unsigned i=0;i<sizeof t-4;i++){ h^=p[i]; h*=16777619u; }
    if(t.magic!=0x504F5254 || t.sum!=h){ for(int i=0;i<4;i++) tr_n[i]=0; tr_uq=0; scrap=0; return; }
    for(int i=0;i<4;i++) tr_n[i]=t.n[i]; tr_uq=t.uq; scrap=t.scrap;
}
static void load_ext_defaults(void){
    for(int i=0;i<4;i++) tr_n[i]=0; tr_uq=0; scrap=0;

    fuel=70; hull=100; shield_lv=1; laser_lv=1; cargo_cap=10; visits=0; cluster_no=0; galaxy_no=0; for(int w=0;w<NCW;w++) clus[w]=0; clus_f=0; clus_up=0; relic_bits=0; weird_seen=0; relic_gold=0; mark_perm=0; for(int k=0;k<NGH;k++) galaxy_hi[k]=0; cur_pl=cur_moon=0; wing_n=0; form=0; form_t=0; forms_open=1; tools_owned=0; tool_held=0; for(int k=0;k<NMK;k++) mark_t[k]=0; memset(cargo,0,8);
    if(endless){ ant_king=1; zones_open=6; cleared_mask=63; w_dlv=6+world_no/2; cur_sys=6; }
    else { ant_king=0; zones_open=zone+1+(zone_cleared?1:0); if(zones_open>6) zones_open=6; cleared_mask=((1<<zone)-1)|(zone_cleared?1<<zone:0); cur_sys=zone; }
    beast_defaults(); gen_cluster();
}
static int load_ext(void){
    SaveX *s=&svx; u8 *p=(u8*)s; for(unsigned i=0;i<sizeof(SaveX);i++) p[i]=SRAM[slot_base(cur_slot)+SX_OFF+i];
    if(s->magic!=0x50494853 || s->sum!=sx_sum(s)){ load_ext_defaults(); beast_load(); trophy_load(); return 0; }
    cur_sys=s->cur_sys; cluster_no=s->cluster_no; fuel=s->fuel; hull=s->hull; shield_lv=s->shield_lv; laser_lv=s->laser_lv; cargo_cap=s->cargo_cap;
    ant_king=s->ant_king; cleared_mask=s->cleared_mask; zones_open=s->zones_open; visits=s->visits; w_dlv=s->w_dlv; memcpy(cargo,s->cargo,8);
    beast_load(); trophy_load(); addr_load(); gen_cluster(); return 1;
}
static void story_boss_beaten(void){
    cleared_mask|=1<<zone; if(zones_open<zone+2) zones_open=zone+2; if(zones_open>6) zones_open=6;
    if(zone==5 && !ant_king){ ant_king=1; galaxy_seed=rnd()|1; gen_cluster(); }
}
