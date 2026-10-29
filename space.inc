// ================================================================= SPACE: flight, pirates, stations, the galaxy map
// Every star system has a sun, planets and moons you can land on, and a wireframe trading station.
// Before the Ant King falls you can only fly between the six story zones; after that the whole universe opens (2^155 x 2^155 clusters of
// 32 stars), and so does the multiverse: 2^256 universes side by side (see universe_go).

static const u8 good_base[8]={8,18,34,55,80,110,65,190};
static const u8 eco_mul[4][8]={{55,65,80,110,130,150,120,120},{120,110,105,70,60,120,110,95},{130,120,110,105,110,60,75,110},{105,50,70,90,115,120,100,80}};

static u32 lseed;
static int lr(int n){ lseed = lseed*1664525u + 1013904223u; return (int)((lseed>>16)%(u32)n); }
static u32 sys_id(int i){ return !home_cluster() ? amix(gsk(107),i) : (u32)i + 1; }      // the star's 32-bit name code
// ---------------------------------------------------------------- THE UNIVERSE: a flat, tight map of star clusters that wraps round at its edges
// 2^155 clusters on a side, 25 light years each: about 1.1 quindecillion light years across. Neighbouring clusters touch, so there is always
// somewhere close to go; far places need fuel, a better warp drive, and stops on the way. Your place is two big numbers: how far east of home
// (clus[0..4]) and how far north (clus[5..9]). Every other word of the old, bigger addresses stays zero (see u_settle).
#define UAX 5
#define UTOP 0x07FFFFFFu                                       // the top word of each axis keeps 27 bits: 4*32+27 = 155 bits
#define ULY 25                                                 // light years across one cluster
static int fuel_tank=0, warp_lv=0, u_far=0;                    // shipyard upgrades, and how deep in space you are (it makes worlds more dangerous)
static int map_lv=0, fold_lv=0, map_units=0, dp_ix=0;          // far travel (deep.inc): the map computer and fold drive levels, how big distances are said (0 names, 1 powers of ten, 2 metres, 3 Planck lengths), and which far landmark you are at (0: none)
static void dist_units(char *out, const char *d, int n); static void dp_home_text(char *out); static void dp_here_name(char *b); static void dp_save(void); static void dp_load(void); static void dp_defaults(void);
static int deep_chart(volatile u16 *pg); static void dp_nav_draw(volatile u16 *pg, int sel); static int dp_nav_press(volatile u16 *pg, int sel);
static int fuel_max(void){ return 70+ship_fuel_bonus()+fuel_tank*40; }
static int warp_bits(void){ int b=4+warp_lv*19; return b>156?156:b; }        // one warp crosses up to 2^this clusters (MK 8 reaches anywhere)
static int warp_cost(int digits){ return 20+3*digits; }                      // fuel for a warp: a little more for every extra digit of distance
static void ax_get(int axis, u32 *a){ for(int w=0;w<UAX;w++) a[w]=clus[axis*UAX+w]; }
static void ax_put(int axis, const u32 *a){ for(int w=0;w<UAX;w++) clus[axis*UAX+w]=a[w]; clus[axis*UAX+UAX-1]&=UTOP; }
static void ax_add(u32 *r, const u32 *a, const u32 *b){ u64 c=0; for(int w=0;w<UAX;w++){ c+=(u64)a[w]+b[w]; r[w]=(u32)c; c>>=32; } r[UAX-1]&=UTOP; }
static void ax_sub(u32 *r, const u32 *a, const u32 *b){ u32 br=0; for(int w=0;w<UAX;w++){ u64 t=(u64)a[w]-b[w]-br; r[w]=(u32)t; br=(u32)(t>>63); } r[UAX-1]&=UTOP; }
static int ax_zero(const u32 *a){ u32 o=0; for(int w=0;w<UAX;w++) o|=a[w]; return !o; }
static int ax_cmp(const u32 *a, const u32 *b){ for(int w=UAX-1;w>=0;w--) if(a[w]!=b[w]) return a[w]<b[w]?-1:1; return 0; }
static int ax_bits(const u32 *a){ for(int w=UAX-1;w>=0;w--) if(a[w]){ int b=31; while(!((a[w]>>b)&1)) b--; return w*32+b+1; } return 0; }
static void ax_shr(u32 *a, int s){ if(s<=0) return; int ws=s>>5, bs=s&31; for(int w=0;w<UAX;w++){ int k=w+ws; u32 lo = k<UAX ? a[k] : 0, hi = k+1<UAX ? a[k+1] : 0; a[w] = bs ? (lo>>bs)|(hi<<(32-bs)) : lo; } }
static int ax_delta(u32 *mag, const u32 *a, const u32 *b){                    // the short way round from a to b: how far, and +1 or -1
    ax_sub(mag,b,a); if((mag[UAX-1]>>26)&1){ u32 z[UAX]={0,0,0,0,0}; ax_sub(mag,z,mag); return -1; } return 1; }
// big numbers for light years (14 words: a jump between universes is up to 2^256 universes of 2^155 clusters, in tenths of a light year)
#define LYW 14                                                  // 14 words: a jump across 2^256 universes is 2^419 tenths of a light year
#define TVW 8                                                   // (the travel log on the cartridge keeps 8 words, and saturates past that)
static void big_mul(u32 *b, int n, u32 k){ u64 c=0; for(int i=0;i<n;i++){ c+=(u64)b[i]*k; b[i]=(u32)c; c>>=32; } }
static void big_add(u32 *r, const u32 *a, int n){ u64 c=0; for(int i=0;i<n;i++){ c+=(u64)r[i]+a[i]; r[i]=(u32)c; c>>=32; } }
static u32 big_div(u32 *b, int n, u32 d){ u64 r=0; for(int i=n-1;i>=0;i--){ u64 cur=(r<<32)|b[i]; b[i]=(u32)(cur/d); r=cur%d; } return (u32)r; }
static int big_zero(const u32 *b, int n){ for(int i=0;i<n;i++) if(b[i]) return 0; return 1; }
static void big_shl(u32 *b, int n, int s){ int ws=s>>5, bs=s&31; for(int i=n-1;i>=0;i--){ u32 lo = i-ws>=0 ? b[i-ws] : 0, lo2 = i-ws-1>=0 ? b[i-ws-1] : 0; b[i] = bs ? (lo<<bs)|(lo2>>(32-bs)) : lo; } }
static void ly_between(u32 *ly, const u32 *dx, const u32 *dy){               // the straight-line distance in light years: (bigger + 0.414 x smaller) x 25
    const u32 *A=dx, *B=dy; if(ax_cmp(dx,dy)<0){ A=dy; B=dx; }
    u32 t[LYW]; for(int i=0;i<LYW;i++) t[i] = i<UAX ? B[i] : 0; big_mul(t,LYW,27146);
    for(int i=0;i<LYW;i++) t[i]=(t[i]>>16)|(i<LYW-1?(t[i+1]<<16):0);
    for(int i=0;i<LYW;i++) ly[i] = i<UAX ? A[i] : 0; big_add(ly,t,LYW); big_mul(ly,LYW,ULY);
}
static int big_dec(char *s, u32 *b){                                          // the decimal digits of an 8-word number (it is used up; s needs 96 chars); returns how many
    char tmp[144]; int n=0;
    if(big_zero(b,LYW)){ s[0]='0'; s[1]=0; return 1; }
    while(!big_zero(b,LYW) && n<138){ u32 r=big_div(b,LYW,1000000000u); for(int k=0;k<9;k++){ tmp[n++]=(char)('0'+r%10); r/=10; } }
    while(n>1 && tmp[n-1]=='0') n--;
    for(int i=0;i<n;i++) s[i]=tmp[n-1-i]; s[n]=0; return n;
}
static const char *const big_nm[22]={"","THOUSAND","MILLION","BILLION","TRILLION","QUADRILLION","QUINTILLION","SEXTILLION","SEPTILLION","OCTILLION","NONILLION","DECILLION",
    "UNDECILLION","DUODECILLION","TREDECILLION","QUATTUORDECILLION","QUINDECILLION","SEXDECILLION","SEPTENDECILLION","OCTODECILLION","NOVEMDECILLION","VIGINTILLION"};
                                                                // the dictionary names, a thousand times apart, up to VIGINTILLION (the short scale: a billion is a thousand million).
                                                                // Everything bigger is made by the Conway-Wechsler rules below, so UNVIGINTILLION, QUINQUAVIGINTILLION, SESVIGINTILLION...
static void cw_prefix(int v, char *o){                          // the Latin for v (0..999) in the Conway-Wechsler system for naming -illions
    static const char *const sm[10]={"NI","MI","BI","TRI","QUADRI","QUINTI","SEXTI","SEPTI","OCTI","NONI"};
    if(v<10){ scpy(o,sm[v]); return; }
    static const char *const un[10]={"","UN","DUO","TRE","QUATTUOR","QUINQUA","SE","SEPTE","OCTO","NOVE"};
    static const char *const te[10]={"","DECI","VIGINTI","TRIGINTA","QUADRAGINTA","QUINQUAGINTA","SEXAGINTA","SEPTUAGINTA","OCTOGINTA","NONAGINTA"};
    static const char *const hu[10]={"","CENTI","DUCENTI","TRECENTI","QUADRINGENTI","QUINGENTI","SESCENTI","SEPTINGENTI","OCTINGENTI","NONGENTI"};
    static const u8 tm[10]={0,1,2|4,1|4,1|4,1|4,1,1,2|8,0}, hm[10]={0,1|8,1,1|4,1|4,1|4,1,1,2|8,0};   // what each tens and hundreds word asks of the unit before it: 1 N, 2 M, 4 S, 8 X
    int u=v%10, t=(v/10)%10, h=v/100; o[0]=0;
    if(u){ int m = t ? tm[t] : hm[h]; scpy(o,un[u]);
        if(u==3 && (m&12)) scat(o,"S");
        if(u==6){ if(m&4) scat(o,"S"); else if(m&8) scat(o,"X"); }
        if(u==7||u==9){ if(m&2) scat(o,"M"); else if(m&1) scat(o,"N"); } }
    if(t) scat(o,te[t]); if(h) scat(o,hu[h]); }
static const char *illion(int g){                               // the name of a thousand to the power g (1 THOUSAND, 2 MILLION...), any size at all
    if(g<22) return big_nm[g<0?0:g];
    static char b[100]; int n=g-1, gs[4], ng=0; b[0]=0;          // the n-th -illion: its number in groups of three, each group's Latin, joined with ILLI
    do { gs[ng++]=n%1000; n/=1000; } while(n && ng<4);
    for(int k=ng-1;k>=0;k--){ char p[40]; cw_prefix(gs[k],p); int L=textlen(p); if(L && (p[L-1]=='I'||p[L-1]=='A')) p[L-1]=0; scat(b,p); scat(b, k ? "ILLI" : "ILLION"); }
    return b; }
static int ly_text(char *out, u32 *ly){                                       // "1.1 QUINDECILLION LY"; returns how many digits the distance has (map_units can change how it is said)
    char d[144]; int n=big_dec(d,ly); out[0]=0;
    if(!(n==1 && d[0]=='0') && (map_units>=2 || (map_units==1 && n>6))){ dist_units(out,d,n); return n; }
    if(n<=6) scpy(out,d);                                                     // up to 999999: just the number
    else { int g=(n-1)/3; int lead=n-3*g; char t[8]; int k=0; for(;k<lead && k<3;k++) t[k]=d[k]; if(lead<3){ t[k++]='.'; t[k++]=d[lead]; } t[k]=0;
        scpy(out,t); scat(out," "); scat(out,illion(g)); }
    scat(out," LY"); return n;
}
static void u_settle(void){                                     // fold an address from the older, bigger universes into this one, and see how deep in space you are
    u32 extra = galaxy_no | (clus[UAX-1]&~UTOP) | (clus[2*UAX-1]&~UTOP); for(int i=0;i<NGH;i++) extra|=galaxy_hi[i]; for(int i=2*UAX;i<NCW;i++) extra|=clus[i];
    if(extra){ u32 h=0x9E3779B9u^galaxy_no; for(int i=0;i<NGH;i++) h=hmix(h^galaxy_hi[i]^(u32)i); for(int i=0;i<NCW;i++) if(clus[i]) h=hmix(h^clus[i]^(u32)i*0x27D4EB2Fu);
        for(int w=0;w<2*UAX;w++){ h=hmix(h+0x632BE5ABu); clus[w]=h; } for(int i=2*UAX;i<NCW;i++) clus[i]=0; galaxy_no=0; for(int i=0;i<NGH;i++) galaxy_hi[i]=0; }
    clus[UAX-1]&=UTOP; clus[2*UAX-1]&=UTOP; clus_refold();
    { u32 a[UAX], b[UAX], m[UAX], z[UAX]={0,0,0,0,0}; ax_get(0,a); ax_get(1,b); ax_delta(m,z,a); int bx=ax_bits(m); ax_delta(m,z,b); int by=ax_bits(m); u_far=(bx>by?bx:by)/4; }
    if(dp_ix>0 && u_far<40+dp_ix) u_far=40+dp_ix;                  // the far landmarks are as hard as it gets
}
#define GAL_SHIFT 20                                            // a galaxy is a block of 2^20 x 2^20 clusters (about 26 million light years across)
static u32 gal_hash(void){ u32 a[UAX], b[UAX]; ax_get(0,a); ax_get(1,b); ax_shr(a,GAL_SHIFT); ax_shr(b,GAL_SHIFT); if(ax_zero(a)&&ax_zero(b)) return 0;
    u32 h=0x6C8E9CF5u; for(int w=0;w<UAX;w++){ h=hmix(h^a[w]^(u32)w*0x9E3779B1u); h=hmix(h^b[w]^(u32)(w+7)*0x9E3779B1u); } return h|1; }
static void gal_name(char *b){ u32 h=gal_hash(); if(!h){ scpy(b,"THE BUG WAY"); return; } u32 ws=wseed; name_of(h^gsk(108),b); wseed=ws; scat(b," GALAXY"); }
static void here_ly(char *out, const u32 *ex, const u32 *ey){ u32 a[UAX],b[UAX],dx[UAX],dy[UAX],ly[LYW]; ax_get(0,a); ax_get(1,b); ax_delta(dx,a,ex); ax_delta(dy,b,ey); ly_between(ly,dx,dy); ly_text(out,ly); }
static void home_ly(char *out){ if(dp_ix>0){ dp_home_text(out); return; } u32 z[UAX]={0,0,0,0,0}; here_ly(out,z,z); }   // (at a far landmark: its distance)
static int sys_dist(int a, int b){ int dx=sys[a].x-sys[b].x, dy=sys[a].y-sys[b].y; return isqrt(dx*dx+dy*dy); }
static void gen_cluster(void){
    clus_refold();
    lseed = amix(gsk(109) ^ 0x5bd1e995, 99);
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
static int sys_dlv(int i){ return sys[i].kind<6 ? sys[i].kind : 6 + (cluster_no*4>u_far?cluster_no*4:u_far) + sys_dist(i,0)/40 + (is_hive(i) ? 4+hive_done*3 : 0); }   // the further from home, the harder
static void sys_name(int i, char *b){ if(sys[i].kind<6) scpy(b,tint_name[sys[i].kind]); else if(is_hive(i)) scpy(b,hive_nm[hive_done]); else name_of(sys_id(i),b); }
static int can_jump(int i){ if(i==cur_sys) return 0; if(sys[i].kind<6) return sys[i].kind<zones_open; return ant_king; }
// ---- planets and moons: every uncharted star has 1 or more planets, and each planet can have moons you can land on and mine.
// A few galaxies are CROWDED: every star there has 6 to 16 planets. (story stars and the hives keep their one world)
#define DENSE_ODDS 12                                         // 1 galaxy in 12 is crowded
static int galaxy_dense(void){ u32 h=gal_hash(); if(!h) return 0; return hmix(h^0xD3A5E000u^gsk(110))%DENSE_ODDS==0; }
static int sys_nplan(int i){ if(sys[i].kind<6 || is_hive(i)) return 1;
    u32 h=amix(gsk(111)^0x504C414Eu,i); int r=(int)(h%1000u);
    if(galaxy_dense()) return 6+(int)((h>>12)%11u);                                    // 6..16 planets: more than ten about half the time
    return r<480?1 : r<740?2 : r<880?3 : r<950?4 : r<985?5 : 6+(int)((h>>12)%3u); }      // about half the stars have 2 or more planets; a few have 6 to 8
static int sys_nmoon(int i, int p){ if(sys[i].kind<6 || is_hive(i)) return 0;
    int r=(int)(amix(gsk(112)^0x4D4F4F4Eu^((u32)p*0x9E3779B1u),i)%100u); return r<40?0 : r<70?1 : r<88?2 : r<96?3 : 4; }
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
    s32 px, py;
    if(cx>-8000000 && cx<8000000 && cy>-8000000 && cy<8000000){ px=120+cx*FOCS/cz; py=VCY-cy*FOCS/cz; }   // fits in 32 bits: a plain divide (the 64-bit one is a slow library call on the GBA, and every ship does 12 of them)
    else { px=120+(s32)(((long long)cx*FOCS)/cz); py=VCY-(s32)(((long long)cy*FOCS)/cz); }
    if(px<-4000||px>4000||py<-4000||py>4000) return 0;
    *sx=px; *sy=py; if(zz) *zz=cz; return 1;
}
static void dot2(volatile u16 *pg, int x, int y, int c){ if(x>=0&&x<240&&y>=0&&y<128) pix(pg,x,y,c); }
static int l2div(int a, int b){ if(b<0){ a=-a; b=-b; } return a>=0 ? (a+(b>>1))/b : -((-a+(b>>1))/b); }   // a/b rounded to the nearest
static void line2(volatile u16 *pg, int x0, int y0, int x1, int y1, int c){
    // clip to the 240x128 view first (Cohen-Sutherland), so a line that is off screen costs nothing and one that is partly on screen only walks its visible part
    #define L2OC(x,y) (((x)<0) | (((x)>239)<<1) | (((y)<0)<<2) | (((y)>127)<<3))
    if(x0<-20000||x0>20000||y0<-20000||y0>20000||x1<-20000||x1>20000||y1<-20000||y1>20000) return;
    int o0=L2OC(x0,y0), o1=L2OC(x1,y1);
    while(o0|o1){
        if(o0&o1) return;                                                       // wholly off one side
        int first = o0!=0, o = first?o0:o1, x, y;
        if(o&8){ x=x0+l2div((x1-x0)*(127-y0),y1-y0); y=127; }
        else if(o&4){ x=x0+l2div((x1-x0)*(0-y0),y1-y0); y=0; }
        else if(o&2){ y=y0+l2div((y1-y0)*(239-x0),x1-x0); x=239; }
        else { y=y0+l2div((y1-y0)*(0-x0),x1-x0); x=0; }
        if(first){ x0=x; y0=y; o0=L2OC(x0,y0); } else { x1=x; y1=y; o1=L2OC(x1,y1); }
    }
    int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(!n){ pix(pg,x0,y0,c); return; }
    s32 fx=x0<<16, fy=y0<<16, sx=(dx<<16)/n, sy=(dy<<16)/n;
    for(int i=0;i<=n;i++){ int px=(fx+0x8000)>>16, py=(fy+0x8000)>>16; if((unsigned)px<240u && (unsigned)py<128u) pix(pg,px,py,c); fx+=sx; fy+=sy; }
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
static int hive_here(void){ return endless && !forge_on && hive_active() && cur_sys==hive_sys(); }
static int hive_boss_floor(void){ return hive_here() && depth_no==hive_floors[hive_done]; }
static void hive_kill(void){ hive_done++; hive_card=hive_done+1; }
static int space_danger(void){ return in_lair ? (beast_fight==1 ? 1+clampi(sdiv(level,3)) : beast_fight==2 ? 4+tech_lv*2+clampi(sdiv(level,3)) : 8 + tech_lv*2 + beast_kills*6 + clampi(sdiv(level,3))) : sys_dlv(cur_sys) + clampi(sdiv(level,4)); }
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
    lseed = gsk(113) ^ 0x1234567 ^ frames;
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
    static const char *const lab[3]={"SHLD","HULL","FUEL"}; int val[3]={shield,hull,fuel}, mx[3]={20+shield_lv*20,100,fuel_max()}, col[3]={55,53,50};
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
static int uni_sub(u32 *r, const u32 *a, const u32 *b); static int uni_dec(char *d, const u32 *u);
static void voyage(const char *from, const char *to, const u32 *t10, u32 hours, int kind);
static void lair_leave(void){ int f=beast_fight; in_lair=0; music(3); { u32 t10[LYW]={20}; char nm[24]; sys_name(cur_sys,nm); voyage("THE LAIR",nm,t10,48,4); } no_ambush=1; space_enter(0); no_ambush=0; if(hive_card) show_hive_card(); space_say(f<3 && beast_stage>=f ? "IT GOT AWAY... FOR NOW" : "WARPED OUT. COME BACK STRONGER"); do_save(); }

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
            rect(pg,0,132,240,28,57); ui_lhint(pg,4,138,"UP/DOWN: PICK A WORLD",C_GREY); ui_lhint(pg,4,148,"A: LAND   B: BACK",C_GREEN);
        }
        vsync(); read_keys();
        if(HIT(K_UP) && cur>0){ cur--; dirty=1; sfx(7); }
        if(HIT(K_DOWN) && cur<n-1){ cur++; dirty=1; sfx(7); }
        if(HIT(K_A)){ body_at(cur_sys,cur,&cur_pl,&cur_moon); wait_release(); return 1; }
        if(HIT(K_B)){ wait_release(); return 0; }
    }
}
static int space_frame(void){   // 0 keep flying, 1 land, 2 open the galaxy map
#ifdef TEST_SAUTO
    { int best=-1; s32 bd=0x7FFFFFFF, rx=0, ry=0, rz=1;                                // (for filming: chase the nearest enemy, or the beast, and shoot)
      if(in_lair){ best=999; rx=(-shx*br[0]-shy*br[1]-shz*br[2])>>14; ry=(-shx*bu[0]-shy*bu[1]-shz*bu[2])>>14; rz=(-shx*bf[0]-shy*bf[1]-shz*bf[2])>>14; }
      else for(int i=0;i<NPIR;i++){ Pirate *q=&pir[i]; if(!q->alive||q->team!=1) continue; s32 ex=(q->x-shx)>>4, ey=(q->y-shy)>>4, ez=(q->z-shz)>>4; s32 d=ex*ex+ey*ey+ez*ez; if(d<bd){ bd=d; best=i;
          rx=((q->x-shx)*br[0]+(q->y-shy)*br[1]+(q->z-shz)*br[2])>>14; ry=((q->x-shx)*bu[0]+(q->y-shy)*bu[1]+(q->z-shz)*bu[2])>>14; rz=((q->x-shx)*bf[0]+(q->y-shy)*bf[1]+(q->z-shz)*bf[2])>>14; } }
      if(best>=0){ s32 az=rz>200?rz:200; if(rz<200) keys|=K_RIGHT; else { if(rx*8>az) keys|=K_RIGHT; else if(rx*8<-az) keys|=K_LEFT; if(ry*8>az) keys|=K_DOWN; else if(ry*8<-az) keys|=K_UP; }          // (DOWN lifts the nose)
          if(rz>200 && rx*5<az && rx*5>-az && ry*5<az && ry*5>-az) keys|=K_A; if(rz>(in_lair?6000:2500)) keys|=K_B; } }
#endif
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
      if(HELD(K_B)){ int a=a16*(burn?2:1); svx_ += (bf[0]*a)>>18; svy_ += (bf[1]*a)>>18; svz_ += (bf[2]*a)>>18; if(burn && (frames&15)==0) fuel-=xo_fuel(1); }
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
    int scans=0;                                                                          // (a full look round is slow: only a few a frame, so a big battle does not stall when ships die)
    for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i]; if(!p->alive) continue;
        if(scans<6 && (((frames+i)&31)==0 || (p->tgt>=0 && (!pir[p->tgt].alive || pir[p->tgt].team==p->team)))){     // re-pick a target now and then
            scans++;
            u32 bd; { s32 ax=(shx-p->x)>>4, ay=(shy-p->y)>>4, az=(shz-p->z)>>4; bd = p->team ? (u32)(ax*ax+ay*ay+az*az) : 765625u; }   // (squared distances in 1/16s: no square roots needed to find the nearest)
            p->tgt = p->team ? -1 : -2;                                                   // -1: you. -2: nobody (wingmen form up)
            for(int j=0;j<NPIR;j++){ Pirate *q=&pir[j]; if(!q->alive || q->team==p->team) continue; s32 ex=q->x-(p->team?p->x:shx), ey=q->y-(p->team?p->y:shy), ez=q->z-(p->team?p->z:shz);   // wingmen go for the enemy nearest YOU
                if(!p->team){ s32 px_=q->x-shx, pz_=q->z-shz; if((px_<0?-px_:px_)+(pz_<0?-pz_:pz_)>16000) continue; }
                ex>>=4; ey>>=4; ez>>=4; u32 dd=(u32)(ex*ex+ey*ey+ez*ez);
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
            else if(pir[p->tgt].alive && pir[p->tgt].team!=p->team && lr(100)<45){ Pirate *q=&pir[p->tgt]; q->hp -= p->team ? 4+danger*2 : 6+laser_lv*4+ship_gun(); boom(q->x,q->y,q->z,3,p->team?53:52);
                if(q->hp<=0) ship_dies(q,0); } }
    }
    // canisters: fly into them to scoop the cargo
    for(int k=0;k<4;k++) if(cans[k].alive){ s32 dx=shx-cans[k].x, dy=shy-cans[k].y, dz=shz-cans[k].z;
        if(dx>-500&&dx<500&&dy>-500&&dy<500&&dz>-500&&dz<500){ cans[k].alive=0; int n=0; for(int i=0;i<8;i++) n+=cargo[i];
            if(n<cargo_cap){ int g=cans[k].good<8 ? cans[k].good : lr(8); cargo[g]++; char b[32]; scpy(b,"SCOOPED "); scat(b,goods[g]); space_say(b); sfx(2); } else space_say("CARGO HOLD FULL"); } }
    for(int i=0;i<NSPP;i++){ SPart *p=&spp[i]; if(p->life<=0) continue; p->life--; p->x+=p->vx; p->y+=p->vy; p->z+=p->vz; }
    if(hull<=0){ if(in_lair){ in_lair=0; no_ambush=1; space_enter(0); no_ambush=0; } hull=100; for(int i=0;i<8;i++) cargo[i]=0; gold=sdiv(gold,2); for(int i=0;i<NPIR;i++) pir[i].alive=0; battle_on=0;
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
    // pirates: little wireframe fighters. In a crowd the far ones are drawn simpler (fewer lines) or as a mark, and the ones off screen are not drawn at all
    int crowd=0; for(int i=0;i<NPIR;i++) if(pir[i].alive) crowd++;
    s32 lod_z = crowd>32 ? 1500 : crowd>16 ? 2200 : 3000, dot_z = crowd>32 ? 4200 : 5500;
    for(int i=0;i<NPIR;i++){ Pirate *p=&pir[i]; if(!p->alive) continue;
        int col = p->team ? 53 : 52, lod=0;
        { int sx,sy; s32 z; if(!sproj(p->x,p->y,p->z,&sx,&sy,&z)) continue;
          if(p->fire_t){ int tx_,ty_; if(p->tgt==-1) line2(pg,sx,sy,120+(frames&7)-4,chase_cam?100:124,col); else if(p->tgt>=0 && sproj(pir[p->tgt].x,pir[p->tgt].y,pir[p->tgt].z,&tx_,&ty_,0)) line2(pg,sx,sy,tx_,ty_,col); }
          if(z>dot_z){ dot2(pg,sx,sy,col); dot2(pg,sx-1,sy,col); dot2(pg,sx+1,sy,col); dot2(pg,sx,sy-1,col); continue; }   // far away: a small mark
          { int rp=(int)(140*FOCS/z)+2; if(sx+rp<0 || sx-rp>239 || sy+rp<0 || sy-rp>127) continue; }                    // the whole ship is off the screen: skip it
          lod = z>lod_z; }
        s16 R[3]={p->f[2],0,(s16)-p->f[0]}; norm3(R); s16 U[3]={(s16)((p->f[1]*R[2]-p->f[2]*R[1])>>14),(s16)((p->f[2]*R[0]-p->f[0]*R[2])>>14),(s16)((p->f[0]*R[1]-p->f[1]*R[0])>>14)};
        static const s16 mdl[6][3]={{0,0,110},{-110,0,-50},{110,0,-50},{0,35,-50},{0,-18,-50},{0,0,-70}};
        s32 v[6*3]; for(int k=0;k<6;k++){ for(int a=0;a<3;a++) v[k*3+a]=(a==0?p->x:a==1?p->y:p->z)+((mdl[k][0]*R[a]+mdl[k][1]*U[a]+mdl[k][2]*p->f[a])>>14); }
        static const u8 pe[]={0,1,0,2,0,3,0,4,1,3,2,3,1,4,2,4,3,5,4,5,1,5,2,5};
        static const u8 pel[]={0,1,0,2,1,5,2,5,3,5};                                                                  // (far away: just the outline and the fin)
        if(lod) wire(pg,v,6,pel,5,col); else wire(pg,v,6,pe,12,col); }
    for(int k=0;k<4;k++) if(cans[k].alive){ int sx,sy; s32 z; if(sproj(cans[k].x,cans[k].y,cans[k].z,&sx,&sy,&z)){ int r=(int)(60*FOCS/z)+1; line2(pg,sx-r,sy,sx,sy-r,50); line2(pg,sx,sy-r,sx+r,sy,50); line2(pg,sx+r,sy,sx,sy+r,50); line2(pg,sx,sy+r,sx-r,sy,50); } }
    for(int i=0;i<NSPP;i++){ SPart *p=&spp[i]; if(p->life<=0) continue; int sx,sy; if(sproj(p->x,p->y,p->z,&sx,&sy,0)){ dot2(pg,sx,sy,p->life>12?49:p->col); dot2(pg,sx+1,sy,p->col); } }
    int lz=laser_t; if(laser_t) laser_t--;
    if(lz && !chase_cam){ line2(pg,30,127,120,VCY,52); line2(pg,210,127,120,VCY,52); }          // (the cockpit view: bolts from the bottom corners)
    // crosshair
    if(!FILM) for(int k=4;k<9;k++){ dot2(pg,120+k,VCY,52); dot2(pg,120-k,VCY,52); dot2(pg,120,VCY+k,52); dot2(pg,120,VCY-k,52); }
    // the chase camera: your own ship, seen from behind, banking into turns and nosing up and down
    if(chase_cam){ ship_roll=(-roll_*2)&1023; int bob=(sinx((frames*3)&255)>>12);
        { int scx=120 + (yawv>>2), scy=100 + bob + (pitv>>2), scs=52 + speed/6, syw=(-(yawv*2))&1023, spt=(40 - pitv)&1023;
          sh_flame = 1 + (HELD(K_B)?1:0) + ((HELD(K_B)&&HELD(K_L)&&fuel>0)?1:0); ship_draw(pg, scx, scy, scs, syw, spt, 0,0, 0,0,240,128); sh_flame=0;
          if(lz) for(int m=0;m<sh_mzn;m++){ int mx_,my_; ship_proj(sh_mz[m][0],sh_mz[m][1],sh_mz[m][2],scx,scy,scs,syw,spt,&mx_,&my_);          // the bolts leave the ship's own guns and run to where you are aiming
              int uh=(5-lz)*7, ut=uh-7; if(ut<0) ut=0;                                                       // a bolt flies out along the line to the crosshair, a few frames
              int hx=mx_+(120-mx_)*uh/20, hy=my_+(VCY-my_)*uh/20, tx=mx_+(120-mx_)*ut/20, ty=my_+(VCY-my_)*ut/20;
              line2(pg,tx,ty,hx,hy,52); line2(pg,tx+1,ty,hx+1,hy,52); line2(pg,(tx+hx)/2,(ty+hy)/2,hx,hy,54); } }
        ship_roll=0; }
    if(hit_t){ hit_t--; if(!FILM) for(int x=0;x<240;x+=3){ dot2(pg,x,0,53); dot2(pg,x,127,53); } }
    cockpit(pg);
    if(sv_yaw!=yaw || sv_pit!=pitch){ yaw=sv_yaw; pitch=sv_pit; }
    static char pb[24]; const char *pr = can_land ? "A: LAND" : can_dock ? "A: DOCK" : 0;
    if(can_land){ int bc=body_count(cur_sys); if(bc>1){ scpy(pb,"A: LAND ("); ncat(pb,bc); scat(pb," WORLDS)"); pr=pb; } }
    if(FILM){ if(space_msg_t) space_msg_t--; return 0; }                    // (filming: nothing over the view)
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

// ---------------------------------------------------------------- the voyage: every jump takes time, and the light years count up as you go
// Nothing is a teleport: the drive spools up, the stars stream past, the distance and the ship's clock count up, and the star
// you are heading for swells up ahead before you drop out beside it. Warps across the universe pass whole galaxies on the way.
static u32 tv_ly[LYW], tv_best[LYW], tv_trips=0, tv_hours=0;       // your travel log: tenths of a light year in all, the longest trip, how many trips, hours between the stars
static u32 vy_ufrom[UW], vy_uto[UW];                                    // a jump between universes: from which, to which (for the voyage's count of universes passed)
static u32 kw_num_edit(volatile u16 *pg, const char *title, u32 v, int nd, u32 lo, u32 hi); static void kcomma(char *b, u32 v); static void digits_named(char *out, const char *d, int k);   // (in kart3.inc)
static int big_cmp(const u32 *a, const u32 *b){ for(int i=LYW-1;i>=0;i--) if(a[i]!=b[i]) return a[i]<b[i]?-1:1; return 0; }
static int big_small(const u32 *b, u32 lim){ for(int i=1;i<LYW;i++) if(b[i]) return 0; return b[0]<lim; }
static void ly_fmt(char *out, const u32 *t10){                      // tenths of a light year, said simply: "3.2 LY", "250 LY", "1.1 QUINDECILLION LY"
    u32 t[LYW]; for(int i=0;i<LYW;i++) t[i]=t10[i]; u32 r=big_div(t,LYW,10);
    if(big_small(t,100)){ scpy(out,""); ncat(out,(int)t[0]); scat(out,"."); ncat(out,(int)r); scat(out," LY"); return; }
    ly_text(out,t); }
static void hours_fmt(char *b, u32 h){                              // "5 HOURS", "2 DAYS 6H", "40 DAYS", "12 YEARS"
    u32 d=h/24; b[0]=0;
    if(d==0){ ncat(b,(int)h); scat(b,h==1?" HOUR":" HOURS"); }
    else if(d<3){ ncat(b,(int)d); scat(b,d==1?" DAY ":" DAYS "); ncat(b,(int)(h%24)); scat(b,"H"); }
    else if(d<1100){ ncat(b,(int)d); scat(b," DAYS"); }
    else { u32 y=d/365; ncat(b,(int)y); scat(b," YEARS"); } }
static void digits_named(char *out, const char *d, int k){          // the first k digits of d, with the big number's name: "123,456", "12.3 MILLION"
    if(k<=6){ int j=0; for(int i=0;i<k;i++){ out[j++]=d[i]; if(i<k-1 && (k-1-i)%3==0) out[j++]=','; } out[j]=0; return; }
    int g=(k-1)/3; int lead=k-3*g, j=0; for(int i=0;i<lead && i<3;i++) out[j++]=d[i]; if(lead<3){ out[j++]='.'; out[j++]=d[lead]; } out[j]=0; scat(out," "); scat(out,illion(g)); }
static void tv_log(const u32 *t10, u32 hours){ big_add(tv_ly,t10,LYW); tv_trips++; tv_hours = tv_hours+hours<tv_hours ? 0xFFFFFFFFu : tv_hours+hours; if(big_cmp(t10,tv_best)>0) for(int i=0;i<LYW;i++) tv_best[i]=t10[i]; }
EW static s16 vsa[40], vsr[40], vsv[40];                              // the streaking stars of a voyage (the fold drive's voyage uses them too)
static void voyage(const char *from, const char *to, const u32 *t10, u32 hours, int kind){   // kind: 0 a star in this cluster, 1 the next cluster, 2 a warp, 3 into the beast's lair, 4 out of it, 5 to another universe
    char tot[40], ds[144]; ly_fmt(tot,t10); int n; { u32 t[LYW]; for(int i=0;i<LYW;i++) t[i]=t10[i]; big_div(t,LYW,10); n=big_dec(ds,t); }
    int big = n>6, T = kind==0 ? 110+(int)((t10[1]||t10[0]>2400?2400:t10[0])/20) : (kind==3||kind==4) ? 120 : 170+n*7; if(T>400) T=400;   // how long it takes, in 60ths: longer trips take longer
    tv_log(t10,hours);
    s16 *sa=vsa, *sr=vsr, *sv=vsv; lseed=amix(gsk(114),(u32)frames); for(int i=0;i<40;i++){ sa[i]=(s16)lr(256); sr[i]=(s16)(lr(120)<<4); sv[i]=(s16)(10+lr(26)); }
    char cap[44], cap2[48]; int cap_t=0, last_k=1; cap[0]=cap2[0]=0;
    if(kind==1){ scpy(cap,"CROSSING THE DARK"); scpy(cap2,"BETWEEN TWO STAR CLUSTERS"); cap_t=150; }
    else if(kind==3||kind==4){ scpy(cap,kind==3?"INTO THE BEAST'S LAIR":"WARPING OUT"); cap_t=110; }
    else if(kind==5){ scpy(cap,"LEAVING THIS UNIVERSE"); scpy(cap2,"OUT HERE THERE IS NOT EVEN SPACE"); cap_t=150; }
    else if(kind==0){ scpy(cap,"EVEN LIGHT TAKES"); { u32 y=t10[0]/10; scpy(cap2,""); if(t10[1]||y>=1){ ncat(cap2,(int)(t10[1]?999999:y)); scat(cap2,y==1?" YEAR":" YEARS"); } else { ncat(cap2,(int)(t10[0]*365/10)); scat(cap2," DAYS"); } scat(cap2," TO GET THERE"); } cap_t=140; }
    sfx(3); int prog=0, rot=0; u32 last=vbl_count;
    while(prog<T*256){
        u32 nv=vbl_count; int dv=(int)(nv-last); last=nv; if(dv<1) dv=1; if(dv>4) dv=4; read_keys();
        prog += dv*256*(HELD(K_B)?3:1); if(prog>T*256) prog=T*256;
        int p=prog/T, sp = p<36 ? p*7 : p>214 ? (256-p)*6 : 256; if(sp<12) sp=12; if(sp>256) sp=256;   // the drive spools up, cruises, then slows to drop out
        int q=(int)((s32)p*p*(768-2*p)>>16);                                                // how far along (eased), 0..256
        volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
        for(int i=0;i<120*128;i++) pg[i]=0;
        rot+=dv;
        for(int i=0;i<40;i++){ int r0=sr[i]; sr[i]=(s16)(sr[i]+sv[i]*sp*dv/64); if(sr[i]>(150<<4)){ sr[i]=(s16)(lr(24)<<4); sa[i]=(s16)lr(256); r0=sr[i]; }
            int len=sv[i]*sp/160+1; int ra=(sr[i]>>4)-len; if(ra<(r0>>4)) ra=r0>>4; int rb=sr[i]>>4; s32 ca=sinx((sa[i]+64)&255), sn=sinx(sa[i]&255);
            line2(pg,120+(ca*ra>>14),VCY+(sn*ra*3>>16),120+(ca*rb>>14),VCY+(sn*rb*3>>16), i%3==0?55:i%3==1?54:49); }
        if(kind==5){ for(int u=0;u<4;u++){ int ur=((rot*(3+u)+u*47)%170), ux=120+(sinx((u*64+rot/2)&255)*ur>>15), uy=VCY+(sinx((u*64+40+rot/3)&255)*ur>>16), rad=3+ur/6;   // other universes drift past: bubbles of stars
            for(int a=0;a<32;a++){ int an=a*8; dot2(pg,ux+(sinx((an+64)&255)*rad>>14),uy+(sinx(an)*rad>>15),(a&3)?55:54); } for(int k=0;k<5;k++){ int an=(k*51+u*30)&255, rr=rad*k/6; dot2(pg,ux+(sinx((an+64)&255)*rr>>14),uy+(sinx(an)*rr>>15),49); } } }
        if((kind==2||kind==5) && big && q*n>=256*8){ for(int g=0;g<3;g++){ int ga=(g*85+rot/3)&255, gr=((rot*(2+g)+g*60)%150); int gx=120+(sinx((ga+64)&255)*gr>>14), gy=VCY+(sinx(ga)*gr*3>>16), sz=2+gr/9;   // whole galaxies rush past
            for(int arm=0;arm<2;arm++) for(int t=1;t<15;t++){ int an=(rot*2+arm*128+t*12+g*40)&255, rr=t*sz/14; dot2(pg,gx+(sinx((an+64)&255)*rr>>14),gy+(sinx(an)*rr>>15),t<4?49:(t&1)?50:54); } dot2(pg,gx,gy,49); dot2(pg,gx+1,gy,49); } }
        if(p>196){ int R=(p-196)*30/60; for(int y=-R;y<=R;y++){ int w=isqrt((u32)(R*R-y*y)); int yy=VCY+y; if(yy<0||yy>127) continue; int x0=120-w, x1=120+w; if(x0<0) x0=0; if(x1>239) x1=239; for(int x=x0;x<=x1;x++) pix(pg,x,yy,(x-120)*(x-120)+y*y < R*R/3 ? 49 : 50); } }   // the star you are heading for swells up
        if(p>250) for(int i=0;i<120*128;i++) pg[i]=54|(54<<8);
        if(!FILM){ char b[48]; scpy(b,"JUMP: "); scat(b,tot); b[44]=0; text(pg,4,3,b,C_GOLD,1); if(T>150) ui_lhint(pg,236-9*4,3,"B: FASTER",C_DGREY); }
        if(kind==5 && !FILM){ u32 dv4[UW]; uni_sub(dv4,vy_uto,vy_ufrom); char ud[144]; int m=uni_dec(ud,dv4); int k = p>250 ? m : 1+(m-1)*q/256;   // the universes behind you, in step with the light years:
            char b[80], g1[24], g2[24]; digits_named(g1,ud,k); digits_named(g2,ud,m); scpy(b,"UNIVERSES PASSED: "); scat(b,g1); scat(b," OF "); scat(b,g2); b[56]=0; text(pg,4,11,b,C_CYAN,1); }                // it rolls up as you arrive
        if((kind==2||kind==5) && big){ int k = p>250 ? n : 1+(n-1)*q/256; if(k>last_k){ for(int kk=last_k+1;kk<=k;kk++){ if(kk>=4 && (kk-1)%3==0){ scpy(cap,"1 "); scat(cap,illion((kk-1)/3)); scat(cap," LIGHT YEARS"); cap2[0]=0; cap_t=70; }
                    if(kk==8){ scpy(cap,"AS FAR AS A GALAXY IS WIDE"); cap2[0]=0; cap_t=70; } }
                if(k>=9){ u32 L=(u32)((ds[0]-'0')*100+(ds[1]-'0')*10+(ds[2]-'0')), G=L*100000u/2621u; char g[48]; scpy(g,""); ncat(g,(int)G); int gl=textlen(g);   // a galaxy is about 26 million light years wide
                    if(k>=12){ for(int z=0;z<k-12 && gl<46;z++) g[gl++]='0'; g[gl]=0; } else { gl-=12-k; if(gl<1){ g[0]='1'; gl=1; } g[gl]=0; }
                    char gn[40]; digits_named(gn,g,gl); scpy(cap2,"PAST "); scat(cap2,gn); scat(cap2, gl==1 && g[0]=='1' ? " GALAXY" : " GALAXIES"); }
                last_k=k; } }
        if(p>190 || FILM) cap_t=0, cap2[0]=0;                                              // (the star ahead takes over)
        if(cap_t>0){ cap_t-=dv; int L=textlen(cap); if(L<=26) text(pg,120-L*4,40,cap,C_WHITE,2); else ctext(pg,120,44,cap,C_WHITE); if(cap2[0]) ctext(pg,120,58,cap2,C_CYAN); }
        else if(cap2[0] && (kind==2||kind==5)) ctext(pg,120,58,cap2,C_CYAN);
        for(int i=128*120;i<160*120;i++) pg[i]=57|(57<<8); for(int i=128*120;i<129*120;i++) pg[i]=58|(58<<8);   // the dashboard: from, to, how far, how long
        { char b[48]; scpy(b,"FROM "); scat(b,from); b[28]=0; text(pg,4,131,b,C_GREY,1); scpy(b,"TO "); scat(b,to); b[28]=0; text(pg,236-textlen(b)*4,131,b,C_GOLD,1); }
        rect(pg,4,141,232,4,56); rect(pg,4,141,232*q/256,4,55); { int sx=4+232*q/256; rect(pg,sx-1,139,3,8,54); }
        { char b[48], c[40]; scpy(b,"GONE ");
            if(!big){ u32 v[LYW]={0}; v[0]=(u32)((u64)t10[0]*(u32)q/256u); if(q>=256 || p>250) v[0]=t10[0]; ly_fmt(c,v); }   // (the whole way, once you are there)
            else { int k = p>250 ? n : 1+(n-1)*q/256; digits_named(c,ds,k); scat(c," LY"); }
            scat(b,c); b[40]=0; text(pg,4,150,b,C_WHITE,1);
            u32 hh=(u32)((u64)hours*(u32)q/256u); scpy(b,"SHIP CLOCK "); hours_fmt(c,hh); scat(b,c); text(pg,236-textlen(b)*4,150,b,C_CYAN,1); }
        vsync(); REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1;
    }
}
static void arrive_after(const char *pre, const u32 *t10, u32 hours){   // "ARRIVED: NAME. 3.2 LY IN 3 DAYS"
    char b[48], nm[24], d[40]; scpy(b,pre); sys_name(cur_sys,nm); nm[14]=0; scat(b,nm); ly_fmt(d,t10);
    if(textlen(b)+textlen(d)<37){ scat(b,". "); scat(b,d); } (void)hours; b[39]=0; space_say(b); }

// ---------------------------------------------------------------- the galaxy map (Elite-style chart)
// ---------------------------------------------------------------- FAVOURITES: up to 256 places you can find again
// They live in spare cartridge memory shared by the three save slots; each save only travels to the ones in its own universe.
#define FAV_BASE 0x3000
#define NFAV 256
#define FAV_SZ 48
#define FAV_HDR (FAV_BASE+NFAV*FAV_SZ)
typedef struct { u32 cx[UAX], cy[UAX]; u32 seed; u8 star, body, used, chk; } Fav;
_Static_assert(sizeof(Fav)==FAV_SZ, "a favourite is 48 bytes");
_Static_assert(FAV_HDR+4 <= 0x6C00, "the favourites must end before the Dungeon Forge's block");
// A favourite keeps the low word of its universe in 'seed'. The upper 224 bits live once, in a shared table of up to 31 other universes (UTab), and the
// favourite holds the table number in 'used' (0 = the upper words are zero, which is every old favourite). On the cartridge 'used' is 0xA5 ^ number.
#define UT_OFF 0x6C90
#define UT_N 31
typedef struct { u32 magic; u32 h[UT_N][UW-1]; u32 sum; } UTab;   // the upper 224 bits of the universes your favourites and tracks are in (shared by all slots)
_Static_assert(UT_OFF+sizeof(UTab) <= 0x7000 && UT_OFF >= 0x6C00+136, "the universe table sits between the Dungeon Forge block and your tracks");
static int uni_alloc(const u32 *u);                                                 // (in kart2.inc: it also knows the saved tracks)
static void ut_write(UTab *t){ t->sum=fnv(t,sizeof *t-4); sram_write(UT_OFF,t,sizeof *t); }
static void ut_read(UTab *t){ sram_read(UT_OFF,t,sizeof *t); if(t->magic==0x38545455u && t->sum==fnv(t,sizeof *t-4)) return;
    struct { u32 magic; u32 h[UT_N][3]; u32 sum; } o; sram_read(0x6D00,&o,sizeof o); memset(t,0,sizeof *t); t->magic=0x38545455u;   // the 128-bit version kept the table here: bring it over
    if(o.magic==0x42545455u && o.sum==fnv(&o,sizeof o-4)){ for(int i=0;i<UT_N;i++) for(int k=0;k<3;k++) t->h[i][k]=o.h[i][k]; ut_write(t); } }
static int ut_find(const UTab *t, const u32 *u){ u32 hi=0; for(int j=1;j<UW;j++) hi|=u[j]; if(!hi) return 0;
    for(int i=0;i<UT_N;i++){ int same=1; for(int j=0;j<UW-1;j++) if(t->h[i][j]!=u[1+j]){ same=0; break; } if(same) return i+1; } return -1; }
static int uni_find(const u32 *u){ UTab t; ut_read(&t); return ut_find(&t,u); }
static u8 fav_chk(const Fav *f){ const u8 *p=(const u8*)f; u8 c=0x5A; for(int i=0;i<FAV_SZ-1;i++) c=(u8)(((c<<1)|(c>>7))^p[i]); return c; }
static void fav_init(void){ u32 m; sram_read(FAV_HDR,&m,4); if(m==0x31564146u) return; for(int i=0;i<NFAV*FAV_SZ;i++) SRAM[FAV_BASE+i]=0; m=0x31564146u; sram_write(FAV_HDR,&m,4); }
static int fav_get(int i, Fav *f){ sram_read(FAV_BASE+i*FAV_SZ,f,FAV_SZ); if(!((f->used&0xE0)==0xA0 && f->chk==fav_chk(f))) return 0; f->used^=0xA5; return 1; }
static void fav_set(int i, const Fav *f){ Fav g=*f; if(g.used>UT_N) g.used=0; g.used^=0xA5; g.chk=fav_chk(&g); sram_write(FAV_BASE+i*FAV_SZ,&g,FAV_SZ); }
static void fav_clear(int i){ for(int k=0;k<FAV_SZ;k++) SRAM[FAV_BASE+i*FAV_SZ+k]=0; }
static void fav_here(Fav *f){ u8 *p=(u8*)f; for(int i=0;i<FAV_SZ;i++) p[i]=0; ax_get(0,f->cx); ax_get(1,f->cy); f->seed=uni_w[0]; { int ix=uni_find(uni_w); f->used=(u8)(ix<0?0x80:ix); } f->star=(u8)cur_sys; f->body=(u8)(cur_pl*8+cur_moon); }
static int fav_eq(const Fav *a, const Fav *b){ const u8 *p=(const u8*)a, *q=(const u8*)b; for(int i=0;i<FAV_SZ-2;i++) if(p[i]!=q[i]) return 0; return a->used==b->used; }
static void fav_uni(const Fav *f, u32 *u){ for(int i=1;i<UW;i++) u[i]=0; u[0]=f->seed; if(f->used>0 && f->used<=UT_N){ UTab t; ut_read(&t); for(int k=0;k<UW-1;k++) u[1+k]=t.h[f->used-1][k]; } }   // the favourite's universe
static int fav_in_uni(const Fav *f){ u32 u[UW]; fav_uni(f,u); return uni_eq(u,uni_w); }
static int fav_add_here(void){ fav_init(); Fav h, f; if(uni_alloc(uni_w)<0) return -3; fav_here(&h); int empty=-1;
    for(int i=0;i<NFAV;i++){ if(fav_get(i,&f)){ if(fav_eq(&f,&h)) return -2; } else if(empty<0) empty=i; }
    if(empty<0) return -1; fav_set(empty,&h); return empty; }
static void fav_name(const Fav *f, char *b){                                  // the star's name, and which planet or moon
    u32 keep[2*UAX]; for(int w=0;w<2*UAX;w++) keep[w]=clus[w]; u32 ks=galaxy_seed, ws=wseed, kuw[UW]; for(int w=0;w<UW;w++) kuw[w]=uni_w[w];
    for(int w=0;w<UAX;w++){ clus[w]=f->cx[w]; clus[UAX+w]=f->cy[w]; } { u32 fu[UW]; fav_uni(f,fu); for(int w=0;w<UW;w++) uni_w[w]=fu[w]; galaxy_seed=fu[0]^uni_fold(fu); } clus_refold();
    if(home_cluster() && f->star<6) scpy(b,tint_name[f->star]); else name_of(sys_id(f->star),b);
    for(int w=0;w<2*UAX;w++) clus[w]=keep[w]; galaxy_seed=ks; for(int w=0;w<UW;w++) uni_w[w]=kuw[w]; wseed=ws; clus_refold();
    if(f->body){ scat(b," P"); ncat(b,(f->body>>3)+1); if(f->body&7){ scat(b," MOON "); ncat(b,f->body&7); } }
}
static int too_close_to_jump(void){ s32 pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4; return !in_lair && pd<PR*2; }
static int edge_star(int dir){ int best=0;                                    // you come into a new cluster at the star nearest the side you came from (0 E, 1 W, 2 N, 3 S)
    for(int i=1;i<NSYS;i++){ int better = dir==0 ? sys[i].x<sys[best].x : dir==1 ? sys[i].x>sys[best].x : dir==2 ? sys[i].y>sys[best].y : sys[i].y<sys[best].y; if(better) best=i; } return best; }
static void arrive_msg(const char *pre){ char b[48], nm[24]; scpy(b,pre); sys_name(cur_sys,nm); scat(b,nm); b[46]=0; space_say(b); }
static int cluster_hop(int dir){                                              // to the next cluster: 3.0 LY of fuel
    if(fuel<30){ space_say("NEED 3.0 LY OF FUEL TO JUMP CLUSTERS"); return 0; }
    char fn[24], tn[24]; sys_name(cur_sys,fn);
    u32 a[UAX], one[UAX]={1,0,0,0,0}; int axis = dir<2 ? 0 : 1; ax_get(axis,a); if(dir==0||dir==2) ax_add(a,a,one); else ax_sub(a,a,one); ax_put(axis,a);
    u_settle(); gen_cluster(); cur_sys=edge_star(dir); cur_pl=cur_moon=0; fuel-=xo_fuel(30); visits++; sys_name(cur_sys,tn);
    { u32 t10[LYW]={250}; voyage(fn,tn,t10,25*24,1); } space_enter(0); { static const char *const dn[4]={"EAST: ","WEST: ","NORTH: ","SOUTH: "}; arrive_msg(dn[dir]); } return 1; }
static int warp_by(const u32 *dx, int sx, const u32 *dy, int sy, int exact, const Fav *f){   // a warp of (dx,dy) clusters; exact = land on f
    u32 ly[LYW]; ly_between(ly,dx,dy); char tx[48]; int cost=warp_cost(ly_text(tx,ly));
    if(cost>fuel){ char m[48]; scpy(m,"NEED "); ncat(m,cost/10); scat(m,"."); ncat(m,cost%10); scat(m," LY OF FUEL. YOU HAVE "); ncat(m,fuel/10); scat(m,"."); ncat(m,fuel%10); space_say(m); return 0; }
    fuel-=xo_fuel(cost); char fn[24], tn[24]; sys_name(cur_sys,fn);
    u32 a[UAX], b[UAX]; ax_get(0,a); ax_get(1,b);
    if(exact){ for(int w=0;w<UAX;w++){ a[w]=f->cx[w]; b[w]=f->cy[w]; } } else { if(sx>0) ax_add(a,a,dx); else ax_sub(a,a,dx); if(sy>0) ax_add(b,b,dy); else ax_sub(b,b,dy); }
    ax_put(0,a); ax_put(1,b); dp_ix=0; u_settle(); gen_cluster();
    if(exact){ cur_sys=f->star; cur_pl=f->body>>3; cur_moon=f->body&7; } else { cur_sys=lr(NSYS); cur_pl=cur_moon=0; }
    visits++; sys_name(cur_sys,tn); { u32 t10[LYW], l2[LYW]; ly_between(l2,dx,dy); for(int i=0;i<LYW;i++) t10[i]=l2[i]; big_mul(t10,LYW,10); int dg=ly_text(tx,l2); voyage(fn,tn,t10,(u32)(30+dg*25)*24,2); } space_enter(0); return 1; }
static int blind_warp(void){                                                  // somewhere random, as far as the drive reaches
    if(!ant_king){ space_say("BEAT THE ANT KING TO LEAVE THIS CLUSTER"); return 0; }
    int wb=warp_bits()-1-(int)(rnd()&1); if(wb>154) wb=154; if(wb<1) wb=1;
    u32 dx[UAX], dy[UAX]; for(int w=0;w<UAX;w++){ dx[w]=rnd(); dy[w]=rnd(); } dx[UAX-1]&=UTOP; dy[UAX-1]&=UTOP; ax_shr(dx,155-wb); ax_shr(dy,155-wb);
    if(ax_zero(dx)&&ax_zero(dy)) dx[0]=1;
    if(!warp_by(dx,(rnd()&1)?1:-1,dy,(rnd()&1)?1:-1,0,0)) return 0;
    { char b[48], d2[40]; home_ly(d2); scpy(b,"WARPED. HOME: "); scat(b,d2); space_say(b); } return 1; }
// ---------------------------------------------------------------- THE MULTIVERSE: once the story is beaten, any of 2^256 universes
// A universe's number is 256 bits (uni_w, see save.inc). They sit side by side, each 2^155 clusters (1.1 quindecillion light years) across, so the trip
// from universe A to universe B is |A-B| universes long. The drive folds space, so every jump costs the same fuel; the trip still takes its time.
#define UNI_COST 50                                                     // fuel for a jump between universes: 5.0 light years' worth
static int uni_dec(char *d, const u32 *u){ u32 t[LYW]; for(int i=0;i<LYW;i++) t[i]=i<UW?u[i]:0; return big_dec(d,t); }   // all the digits of a universe number (d needs 96)
static void uni_name(char *b, const u32 *u){                            // "UNIVERSE #1,234,567", or for the huge ones "UNIVERSE #1234..5678"
    scpy(b,"UNIVERSE #"); { u32 hi=0; for(int i=1;i<UW;i++) hi|=u[i]; if(!hi){ kcomma(b,u[0]); return; } }
    char d[144]; int n=uni_dec(d,u); if(n<=10){ char c[16]; int j=0; for(int i=0;i<n;i++){ c[j++]=d[i]; if(i<n-1 && (n-1-i)%3==0) c[j++]=','; } c[j]=0; scat(b,c); return; }
    char c[12]; for(int i=0;i<4;i++) c[i]=d[i]; c[4]='.'; c[5]='.'; for(int i=0;i<4;i++) c[6+i]=d[n-4+i]; c[10]=0; scat(b,c); }
static int uni_sub(u32 *r, const u32 *a, const u32 *b){                 // r = |a-b| (UW words); 1 if b was bigger
    int lt=0; for(int i=UW-1;i>=0;i--) if(a[i]!=b[i]){ lt = a[i]<b[i]; break; }
    const u32 *x = lt?b:a, *y = lt?a:b; u64 c=0; for(int i=0;i<UW;i++){ u64 v=(u64)x[i]-y[i]-c; r[i]=(u32)v; c=(v>>63)&1; } return lt; }
static void uni_addp(u32 *u, const u32 *k, int neg){                    // u += k or u -= k, round and round at 2^256
    u64 c=0; for(int i=0;i<UW;i++){ if(!neg){ u64 v=(u64)u[i]+k[i]+c; u[i]=(u32)v; c=v>>32; } else { u64 v=(u64)u[i]-k[i]-c; u[i]=(u32)v; c=(v>>63)&1; } } }
static void uni_ly(u32 *t10, const u32 *from, const u32 *to){           // tenths of a light year from one universe to another: |A-B| x 2^155 clusters x 25 LY x 10
    for(int i=0;i<LYW;i++) t10[i]=0; uni_sub(t10,from,to); big_mul(t10,LYW,ULY*10); big_shl(t10,LYW,155); }
static int universe_go(const u32 *to, const Fav *f){                    // jump to universe 'to': to the same place there, or onto the favourite f. 1 if you went
    if(!ant_king){ space_say("BEAT THE ANT KING TO LEAVE THIS UNIVERSE"); return 0; }
    if(uni_eq(to,uni_w) && !f){ space_say("YOU ARE ALREADY IN THAT UNIVERSE"); return 0; }
    if(too_close_to_jump()){ space_say("TOO CLOSE TO THE PLANET TO JUMP"); return 0; }
    if(fuel<UNI_COST){ space_say("A UNIVERSE JUMP NEEDS 5.0 LY OF FUEL"); return 0; }
    u32 dest[UW]; for(int i=0;i<UW;i++) dest[i]=to[i];                     // (to may point at the universe you are in)
    char fn[28], tn[28]; uni_name(fn,uni_w); uni_name(tn,dest); u32 t10[LYW]; uni_ly(t10,uni_w,dest);
    for(int i=0;i<UW;i++){ vy_ufrom[i]=uni_w[i]; vy_uto[i]=dest[i]; } fuel-=xo_fuel(UNI_COST); uni_set(dest);
    if(f){ for(int w=0;w<UAX;w++){ clus[w]=f->cx[w]; clus[UAX+w]=f->cy[w]; } }
    dp_ix=0; u_settle(); gen_cluster(); if(f){ cur_sys=f->star; cur_pl=f->body>>3; cur_moon=f->body&7; } else { cur_pl=cur_moon=0; } visits++;
    { u32 t[LYW]; char d[144]; for(int i=0;i<LYW;i++) t[i]=t10[i]; big_div(t,LYW,10); int dg=big_dec(d,t); voyage(fn,tn,t10,(u32)(30+dg*25)*24,5); }
    space_enter(0); { char b[48], nm[24]; uni_name(b,dest); scat(b,": "); sys_name(cur_sys,nm); nm[13]=0; scat(b,nm); b[39]=0; space_say(b); } return 1; }
static void uni_edit(volatile u16 *pg, u32 *u){                         // type a universe number digit by digit: 78 digits, 3 rows of 26
    char d[80]; { char t[144]; int n=uni_dec(t,u); for(int i=0;i<78;i++) d[i]='0'; for(int i=0;i<n && i<78;i++) d[78-n+i]=t[i]; } d[78]=0;
    int cur=77, t=0; wait_release();
    while(1){ box(pg,4,34,232,92); ctext(pg,120,38,"UNIVERSE NUMBER (0 TO 2^256-1)",C_GOLD);
        for(int i=0;i<78;i++){ int r=i/26, x=i%26; int px=16+x*8, py=56+r*14; char c[2]={d[i],0}; rect(pg,px-1,py-1,8,10,0); text(pg,px,py,c,i==cur?C_GOLD:C_WHITE,1); if(i==cur && (t&16)) rect(pg,px,py+8,5,1,C_GOLD); }
        ui_chint(pg,120,108,"UP/DOWN: DIGIT  LEFT/RIGHT: MOVE",C_GREY); ui_chint(pg,120,116,"A: OK  B: CANCEL",C_GREY); vsync(); read_keys(); t++;
        if(HIT(K_UP)) d[cur]=(char)(d[cur]=='9'?'0':d[cur]+1); if(HIT(K_DOWN)) d[cur]=(char)(d[cur]=='0'?'9':d[cur]-1);
        if(HIT(K_LEFT) && cur>0) cur--; if(HIT(K_RIGHT) && cur<77) cur++;
        if(HIT(K_B)){ wait_release(); return; }
        if(HIT(K_A)||HIT(K_START)){ u32 v[LYW]; for(int i=0;i<LYW;i++) v[i]=0;
            for(int i=0;i<78;i++){ big_mul(v,LYW,10); u32 dg[LYW]; for(int k=0;k<LYW;k++) dg[k]=0; dg[0]=(u32)(d[i]-'0'); big_add(v,dg,LYW); }
            int over=0; for(int i=UW;i<LYW;i++) if(v[i]) over=1; for(int i=0;i<UW;i++) u[i]= over ? 0xFFFFFFFFu : v[i]; wait_release(); return; } } }
static int universe_menu(volatile u16 *pg){                             // pick a universe by its number; 1 if you jumped
    u32 to[UW]; for(int i=0;i<UW;i++) to[i]=uni_w[i]; { u32 one[UW]={1}; uni_addp(to,one,0); } int dirty=2, rep=0; wait_release();
    while(1){
        if(dirty){ char b[64], c[48], dd[144];
            if(dirty==2){ rect(pg,0,0,240,160,0); frame1(pg,4,4,232,152,58); text(pg,68,12,"UNIVERSE JUMP",C_GOLD,2);   // the whole panel, once
                uni_name(c,uni_w); scpy(b,"YOU ARE IN "); scat(b,c); ctext(pg,120,28,b,C_WHITE);
                ctext(pg,120,38,"THERE ARE 116 QUATTUORVIGINTILLION UNIVERSES.",C_GREY); ctext(pg,120,46,"EACH ONE IS 1.1 QUINDECILLION LY ACROSS.",C_GREY);
                ctext(pg,120,60,"JUMP TO:",C_GOLD);
                scpy(b,"IT TAKES 5.0 LY OF FUEL. YOU HAVE "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10); ctext(pg,120,106,b,fuel>=UNI_COST?C_GREEN:C_RED);
                ui_chint(pg,120,120,"LEFT/RIGHT: CHANGE IT (HOLD: FASTER)",C_WHITE); ui_chint(pg,120,128,"A: TYPE A NUMBER   SELECT: A RANDOM ONE",C_WHITE);
                ui_chint(pg,120,142,"START: JUMP!   B: BACK",C_GOLD); }
            rect(pg,8,68,224,34,0);                                                                       // then only the lines that change
            { int n=uni_dec(dd,to);
              if(n<=10){ uni_name(c,to); scpy(b,"< "); scat(b,c); scat(b," >"); text(pg,120-textlen(b)*4,70,b,C_GOLD,2); }
              else { char sg[112], l1[60]; const char *l2=""; int k=0; for(int i=0;i<n;i++){ sg[k++]=dd[i]; if(i<n-1 && (n-1-i)%3==0) sg[k++]=','; } sg[k]=0;   // long ones: every digit, in threes with commas, on one or two lines
                  scpy(l1,"#");
                  if(k<=50) scat(l1,sg);
                  else { int cut=k/2; while(sg[cut]!=',') cut++; for(int i=0;i<=cut;i++) l1[1+i]=sg[i]; l1[cut+2]=0; l2=sg+cut+1; }   // (break after a comma near the middle)
                  if(l2[0]){ ctext(pg,120,69,l1,C_GOLD); ctext(pg,120,77,l2,C_GOLD); } else ctext(pg,120,73,l1,C_GOLD); } }
            { u32 d[UW], dz=0; uni_sub(d,to,uni_w); for(int i=0;i<UW;i++) dz|=d[i];
              if(!dz) ctext(pg,120,90,"THAT IS WHERE YOU ARE NOW",C_CYAN);
              else { char ds[144]; int n=uni_dec(ds,d); if(n<=10){ scpy(b,"THAT IS "); kcomma(b,d[0]); scat(b, d[0]==1 ? " UNIVERSE AWAY" : " UNIVERSES AWAY"); }
                  else { char g[24]; digits_named(g,ds,n); scpy(b,"THAT IS "); scat(b,g); scat(b," UNIVERSES AWAY"); }
                  ctext(pg,120,86,b,C_CYAN);
                  u32 t10[LYW]; uni_ly(t10,uni_w,to); scpy(b,"OR "); ly_fmt(c,t10); scat(b,c); ctext(pg,120,94,b,C_CYAN); } }
            dirty=0; }
        vsync(); read_keys();
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HELD(K_LEFT)||HELD(K_RIGHT)){ rep++; if(rep==1 || (rep>15 && !(rep&3))){ u32 st[UW]={1}; for(int k=(rep-15)/30;k>0;k--){ u64 c=0; u32 x[UW]; for(int i=0;i<UW;i++){ c+=(u64)st[i]*10; x[i]=(u32)c; c>>=32; } if(c) break; for(int i=0;i<UW;i++) st[i]=x[i]; }   // held: 1, 10, 100... at a time
                uni_addp(to,st,HELD(K_LEFT)?1:0); dirty=1; sfx(7); } } else rep=0;
        if(HIT(K_A)){ uni_edit(pg,to); dirty=2; }
        if(HIT(K_SEL)){ { int kw=1+(int)(rnd()%UW); for(int i=0;i<UW;i++) to[i]= i<kw ? rnd() : 0; to[kw-1]>>=(rnd()&31); } dirty=1; sfx(6); }      // (a random number: its size is random too, so near ones come up as well)
        if(HIT(K_START)){ int r=universe_go(to,0); wait_release(); return r; } } }
static int fav_go(const Fav *f){                                              // fly to a favourite: 1 if you jumped
    if(!fav_in_uni(f)){ u32 fu[UW]; fav_uni(f,fu); return universe_go(fu,f); }                  // in another universe: a universe jump lands you right on it
    u32 a[UAX], b[UAX], dx[UAX], dy[UAX]; ax_get(0,a); ax_get(1,b); int sx=ax_delta(dx,a,f->cx), sy=ax_delta(dy,b,f->cy);
    if(ax_zero(dx)&&ax_zero(dy)){                                             // in this very cluster: an ordinary jump to the star
        if(f->star==cur_sys){ cur_pl=f->body>>3; cur_moon=f->body&7; space_say("YOU ARE THERE. LAND WHEN YOU LIKE"); return 0; }
        if(!can_jump(f->star)){ space_say("THAT STAR IS STILL LOCKED"); return 0; }
        int d=sys_dist(cur_sys,f->star); if(d>fuel){ space_say("NOT ENOUGH FUEL. REFUEL AT A STATION"); return 0; }
        char fn[24], tn[24]; sys_name(cur_sys,fn); sys_name(f->star,tn); u32 t10[LYW]={(u32)d};
        fuel-=xo_fuel(d); cur_sys=f->star; cur_pl=f->body>>3; cur_moon=f->body&7; visits++; voyage(fn,tn,t10,(u32)d*24/10+1,0); space_enter(0); arrive_after("ARRIVED: ",t10,0); return 1; }
    if(!ant_king){ space_say("BEAT THE ANT KING TO LEAVE THIS CLUSTER"); return 0; }
    int bits=ax_bits(dx); if(ax_bits(dy)>bits) bits=ax_bits(dy); int over=bits+1-warp_bits();
    if(over<=0){ if(!warp_by(dx,sx,dy,sy,1,f)) return 0; arrive_msg("ARRIVED: "); return 1; }
    ax_shr(dx,over); ax_shr(dy,over);                                         // too far for the drive: go as far as it reaches, the same way
    if(!warp_by(dx,sx,dy,sy,0,0)) return 0;
    { char b[48], d2[40]; here_ly(d2,f->cx,f->cy); scpy(b,"PART WAY. "); scat(b,d2); scat(b," TO GO"); b[46]=0; space_say(b); } return 1; }
static int fav_screen(volatile u16 *pg){                                      // the list: A flies there, SELECT removes, the top row saves where you are
    fav_init(); int sel=0, top=0, dirty=1, n=0, arm=-1; static u8 idx[NFAV]; char note[48]; note[0]=0;
    #define FAV_REBUILD() do{ Fav q_; n=0; for(int i_=0;i_<NFAV;i_++) if(fav_get(i_,&q_)) idx[n++]=(u8)i_; }while(0)
    FAV_REBUILD(); wait_release();
    while(1){
        if(dirty){ dirty=0; char b[64], nm[32];
            for(int i=0;i<240*160/2;i++) pg[i]=0;
            text(pg,4,4,"FAVOURITES",C_GOLD,2); scpy(b,""); ncat(b,n); scat(b,"/256"); text(pg,236-textlen(b)*4,6,b,C_GREY,1);
            scpy(b,"FUEL "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10); scat(b," / "); ncat(b,fuel_max()/10); scat(b," LY   WARP DRIVE MK "); ncat(b,warp_lv); text(pg,4,18,b,C_CYAN,1);
            int rows=n+1;
            for(int r=top;r<rows && r<top+9;r++){ int y=30+(r-top)*10;
                if(r==sel) rect(pg,0,y-2,240,9,58);
                if(r==0){ sys_name(cur_sys,nm); scpy(b,"+ SAVE THIS PLACE ("); scat(b,nm); scat(b,")"); b[56]=0; text(pg,6,y,b,C_GREEN,1); continue; }
                Fav f; fav_get(idx[r-1],&f); fav_name(&f,nm); nm[22]=0; int other=!fav_in_uni(&f);
                text(pg,6,y,nm,(other && !ant_king)?C_DGREY:C_WHITE,1);
                if(other){ u32 fu[UW]; fav_uni(&f,fu); uni_name(b,fu); } else { u32 a[UAX],bb[UAX],dx[UAX],dy[UAX]; ax_get(0,a); ax_get(1,bb); ax_delta(dx,a,f.cx); ax_delta(dy,bb,f.cy);
                    if(ax_zero(dx)&&ax_zero(dy)) scpy(b,"THIS CLUSTER"); else { u32 ly[LYW]; ly_between(ly,dx,dy); ly_text(b,ly); } }
                b[30]=0; text(pg,236-textlen(b)*4,y,b,other?(ant_king?C_CYAN:C_DGREY):C_GREY,1); }
            if(rows>9){ text(pg,232,30,"^",top>0?C_WHITE:C_DGREY,1); text(pg,232,112,"v",top+9<rows?C_WHITE:C_DGREY,1); }
            rect(pg,0,122,240,1,58);
            if(sel==0){ text(pg,4,126,"SAVES THE STAR YOU ARE AT, SO YOU CAN COME BACK",C_GREY,1); ui_lhint(pg,4,140,"A: SAVE IT   B: BACK",C_CYAN); }
            else { Fav f; fav_get(idx[sel-1],&f);
                if(!fav_in_uni(&f)){ if(ant_king){ u32 fu[UW]; fav_uni(&f,fu); uni_name(b,fu); scat(b,": A UNIVERSE JUMP, 5.0 LY OF FUEL"); text(pg,4,126,b,fuel>=UNI_COST?C_GREEN:C_RED,1); }
                    else text(pg,4,126,"IN ANOTHER UNIVERSE. BEAT THE ANT KING TO GO THERE",C_DGREY,1); }
                else { u32 a[UAX],bb[UAX],dx[UAX],dy[UAX]; ax_get(0,a); ax_get(1,bb); ax_delta(dx,a,f.cx); ax_delta(dy,bb,f.cy);
                    if(ax_zero(dx)&&ax_zero(dy)){ int d=sys_dist(cur_sys,f.star); scpy(b,"A SHORT JUMP: "); ncat(b,d/10); scat(b,"."); ncat(b,d%10); scat(b," LY OF FUEL"); text(pg,4,126,b,d<=fuel?C_GREEN:C_RED,1); }
                    else { int bits=ax_bits(dx); if(ax_bits(dy)>bits) bits=ax_bits(dy); int over=bits+1-warp_bits(); if(over>0){ ax_shr(dx,over); ax_shr(dy,over); }
                        u32 ly[LYW]; ly_between(ly,dx,dy); char t2[48]; int c=warp_cost(ly_text(t2,ly));
                        scpy(b, over>0 ? "TOO FAR FOR YOUR DRIVE: A STOP ON THE WAY, " : "ONE WARP: "); ncat(b,c/10); scat(b,"."); ncat(b,c%10); scat(b," LY OF FUEL"); b[58]=0;
                        text(pg,4,126,b, !ant_king?C_DGREY : c<=fuel?C_GREEN:C_RED,1);
                        if(!ant_king) text(pg,4,133,"BEAT THE ANT KING TO LEAVE THIS CLUSTER",C_DGREY,1); } }
                text(pg,4,140, arm==sel ? "PRESS SELECT AGAIN TO REMOVE IT" : "A: GO THERE   SEL: REMOVE   B: BACK", arm==sel?C_RED:C_CYAN,1); }
            if(note[0]) text(pg,4,150,note,C_GOLD,1);
        }
        vsync(); read_keys();
        int rows=n+1;
        if(HIT(K_UP)){ sel=(sel+rows-1)%rows; arm=-1; note[0]=0; dirty=1; } if(HIT(K_DOWN)){ sel=(sel+1)%rows; arm=-1; note[0]=0; dirty=1; }
        if(sel<top) top=sel; if(sel>=top+9) top=sel-8;
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HIT(K_A)){
            if(sel==0){ int r=fav_add_here(); scpy(note, r>=0 ? "SAVED TO YOUR FAVOURITES" : r==-2 ? "IT IS ALREADY A FAVOURITE" : r==-3 ? "TOO MANY OTHER UNIVERSES SAVED (31)" : "FULL: REMOVE ONE FIRST (256 MAX)"); if(r>=0){ sfx(6); FAV_REBUILD(); } dirty=1; }
            else { Fav f; fav_get(idx[sel-1],&f); if(too_close_to_jump()){ scpy(note,"TOO CLOSE TO THE PLANET TO JUMP"); dirty=1; }
                else if(fav_go(&f)){ wait_release(); return 1; } else dirty=1; } }
        if(HIT(K_SEL) && sel>0){ if(arm==sel){ fav_clear(idx[sel-1]); FAV_REBUILD(); if(sel>n) sel=n; arm=-1; scpy(note,"REMOVED"); sfx(12); } else arm=sel; dirty=1; }
    }
    #undef FAV_REBUILD
}
static int cluster_jump(volatile u16 *pg){                                    // START on the galaxy map: to a neighbouring cluster, a blind warp, another universe, or the long-range chart
    char b[48], r2[40]; int dirty=1;
    wait_release();
    while(1){
        if(dirty){ dirty=0;
            rect(pg,30,34,180,96,0); frame1(pg,30,34,180,96,58); text(pg,40,39,"CLUSTER JUMP",C_GOLD,1);
            text(pg,40,50,"UP: NORTH       DOWN: SOUTH",C_WHITE,1); text(pg,40,58,"LEFT: WEST      RIGHT: EAST",C_WHITE,1);
            text(pg,40,67,"NEXT CLUSTER: 3.0 LY OF FUEL",C_GREY,1);
            { u32 r[LYW]={0}; int wb=warp_bits(); if(wb>154) wb=154; r[wb>>5]=1u<<(wb&31); big_mul(r,LYW,ULY); ly_text(r2,r); }
            scpy(b,"R: WARP UP TO "); scat(b,r2); b[42]=0; text(pg,40,77,b,C_CYAN,1);
            ui_lhint(pg,40,88,"A: JUMP TO ANOTHER UNIVERSE",C_GOLD);
            ui_lhint(pg,40,98,"L: LONG-RANGE CHART",C_GOLD);
            { static const char *const un[4]={"NAMES","POWERS OF TEN","METRES","PLANCK LENGTHS"}; scpy(b,"SELECT: UNITS = "); scat(b,un[map_units]); ui_lhint(pg,40,108,b,C_GREY); }
            ui_lhint(pg,40,119,"B: BACK",C_DGREY); }
        vsync(); read_keys();
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HIT(K_SEL)){ map_units=(map_units+1)&3; dirty=1; sfx(7); }
        if(HIT(K_L)){ int r=deep_chart(pg); wait_release(); return r; }                        // (the chart clears the screen: the map draws itself again)
        int dir = HIT(K_RIGHT)?0 : HIT(K_LEFT)?1 : HIT(K_UP)?2 : HIT(K_DOWN)?3 : -1;
        if(dir>=0){ if(too_close_to_jump()){ space_say("TOO CLOSE TO THE PLANET TO JUMP"); wait_release(); return 0; } int r=cluster_hop(dir); wait_release(); return r; }
        if(HIT(K_R)){ if(too_close_to_jump()){ space_say("TOO CLOSE TO THE PLANET TO JUMP"); wait_release(); return 0; } int r=blind_warp(); wait_release(); return r; }
        if(HIT(K_A)){ int r=universe_menu(pg); wait_release(); return r; } }
}
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
            char b[48]; gal_name(b); text(pg,4,3,b,C_GOLD,1);
            { char c[48]; if(home_cluster()) scpy(c,"THE HOME CLUSTER"); else { char d2[40]; home_ly(d2); scpy(c,"HOME IS "); scat(c,d2); scat(c," AWAY"); } text(pg,4,10,c,C_GREY,1); }
            // (where you are: the two lines above)
            for(int i=0;i<64;i++){ int a=i*4; int r=fuel; int x=sys[cur_sys].x+(sinx(a+64)*r>>14), y=sys[cur_sys].y+(sinx(a)*r>>14); if(x>=0&&x<240&&y>=16&&y<130) pix(pg,x,y,C_DGREY); }
            for(int i=0;i<NSYS;i++){ int x=sys[i].x, y=sys[i].y, k=sys[i].kind;
                int c = k<6 ? (k<zones_open ? C_GOLD : C_DGREY) : (ant_king ? (sys[i].eco==0?C_GREEN : sys[i].eco==1?C_GREY : sys[i].eco==2?C_CYAN : C_RED) : C_DGREY);
                rect(pg,x-1,y-1,3,3,c); if(k<6){ b[0]='1'+k; b[1]=0; text(pg,x+3,y-6,b,c,1); } }
            for(int i=0;i<NSYS;i++) if(has_base(i) && (i<6 ? i<zones_open : ant_king)){ int x=sys[i].x, y=sys[i].y; rect(pg,x-3,y-3,2,2,C_RED); rect(pg,x+2,y-3,2,2,C_RED); rect(pg,x-3,y+2,2,2,C_RED); rect(pg,x+2,y+2,2,2,C_RED); }
            if(hive_active()){ int hx=sys[hive_sys()].x, hy=sys[hive_sys()].y; for(int a=0;a<256;a+=8) pix(pg,hx+(sinx(a+64)*7>>14),hy+(sinx(a)*7>>14),C_RED); text(pg,hx-8,hy+9,"HIVE",C_RED,1);
                scpy(b,"TECH HIVES LEFT: "); ncat(b,5-hive_done); text(pg,4,112,b,C_RED,1); }
            if(home_cluster()) for(int k=0;k<5;k++) if((cleared_mask>>k)&1){ line2(pg,sys[k].x,sys[k].y,sys[k+1].x,sys[k+1].y,C_DGREY); }   // the story's path (only the home cluster has it)
            int x=sys[cur_sys].x, y=sys[cur_sys].y; for(int a=0;a<256;a+=16) pix(pg,x+(sinx(a+64)*5>>14),y+(sinx(a)*5>>14),C_WHITE);
            x=sys[sel].x; y=sys[sel].y; line2(pg,x-6,y,x-3,y,C_CYAN); line2(pg,x+3,y,x+6,y,C_CYAN); line2(pg,x,y-6,x,y-3,C_CYAN); line2(pg,x,y+3,x,y+6,C_CYAN);
            rect(pg,0,132,240,28,57);
            sys_name(sel,b); text(pg,4,135,b,C_WHITE,1);
            int d=sys_dist(sel,cur_sys);
            scpy(b, sys[sel].kind<6 ? "STORY ZONE  " : is_hive(sel) ? "TECH HIVE  " : "UNCHARTED  "); scat(b,eco_name[sys[sel].eco]); scat(b,"  DANGER "); ncat(b,sys_dlv(sel)); text(pg,4,143,b,C_GREY,1);
            scpy(b,"DISTANCE "); ncat(b,d/10); scat(b,"."); ncat(b,d%10); scat(b," LY"); if(d){ char hb[24]; hours_fmt(hb,(u32)d*24/10+1); scat(b,", "); scat(b,hb); } scat(b,"  FUEL "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10);
            text(pg,4,151,b,C_GREY,1);
            line2(pg,186,126,236,126,C_GREY); line2(pg,186,124,186,128,C_GREY); line2(pg,236,124,236,128,C_GREY); text(pg,200,119,"5 LY",C_GREY,1);   // the map's scale
            if(sys[sel].kind==6 && !is_hive(sel)){ int bc=body_count(sel); if(bc>1){ scpy(b,"WORLDS "); ncat(b,bc); text(pg,236-textlen(b)*4,143,b,C_CYAN,1); } }
            if(ant_king){ if(dp_ix>0) dp_here_name(b); else uni_name(b,uni_w); text(pg,4,24,b,C_CYAN,1); }
            if(galaxy_dense()) text(pg,236-14*4,24,"CROWDED GALAXY",C_GOLD,1);
            if(is_hive(sel)){ scpy(b,"OVERLORD ON FLOOR "); ncat(b,hive_floors[hive_done]); text(pg,236-textlen(b)*4,135,b,C_RED,1); }
            else if(has_base(sel)) text(pg,236-13*4,135,"DRONE OUTPOST",C_RED,1);
            { scpy(b,"BOLT'S MEMORY "); ncat(b,data_cores); scat(b,"/8"); text(pg,4,122-(beast_stage>=3?10:0),b,C_CYAN,1); }
            const char *st = sel==cur_sys ? "YOU ARE HERE" : !can_jump(sel) ? (sys[sel].kind<6 ? "LOCKED: BEAT THE BOSS BEFORE IT" : "LOCKED: BEAT THE ANT KING") : d>fuel ? "TOO FAR: REFUEL AT A STATION" : "A: JUMP HERE";
            text(pg,236-textlen(st)*4,151,st, (sel!=cur_sys && can_jump(sel) && d<=fuel)?C_GREEN:C_RED,1);
            ui_lhint(pg,236-15*4,3,"SEL: FAVOURITES",C_DGREY); if(ant_king) ui_lhint(pg,236-19*4,17,"START: CLUSTER JUMP",C_DGREY); else ui_lhint(pg,236-7*4,17,"B: BACK",C_DGREY);
            text(pg,4,17,"D-PAD: PICK A STAR",C_DGREY,1);
            if(beast_stage>=3) ui_lhint(pg,4,122,"L: FIGHT THE TECHNOBEAST - 2.0 LY",C_RED);
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
            fuel-=xo_fuel(20); { u32 t10[LYW]={20}; char fn[24]; sys_name(cur_sys,fn); voyage(fn,"THE BEAST'S LAIR",t10,48,3); } lair_enter(); wait_release(); return 1; }
        if(HIT(K_SEL)){ if(fav_screen(pg)) return 1; dirty=1; }                    // your favourite places
        if(HIT(K_START) && ant_king){ if(cluster_jump(pg)) return 1; dirty=1; }     // the next cluster over, or a blind warp
        if(HIT(K_R) && ant_king){ if(too_close_to_jump()){ space_say("TOO CLOSE TO THE PLANET TO JUMP"); wait_release(); return 0; } if(blind_warp()){ wait_release(); return 1; } }
        if(HIT(K_A) && sel!=cur_sys && can_jump(sel) && sys_dist(sel,cur_sys)<=fuel){
            s32 pd=isqrt((u32)((shx>>4)*(shx>>4)+(shy>>4)*(shy>>4)+(shz>>4)*(shz>>4)))<<4;
            if(pd<PR*2){ space_say("TOO CLOSE TO THE PLANET TO JUMP"); wait_release(); return 0; }
            int d=sys_dist(sel,cur_sys); char fn[24], tn[24]; sys_name(cur_sys,fn); sys_name(sel,tn); u32 t10[LYW]={(u32)d};
            fuel-=xo_fuel(d); cur_sys=sel; cur_pl=cur_moon=0; visits++;
            voyage(fn,tn,t10,(u32)d*24/10+1,0); space_enter(0); arrive_after("ARRIVED: ",t10,0);
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
    static const char *const yard[11]={"REFUEL","REPAIR HULL","LASER UPGRADE","SHIELD UPGRADE","BIGGER CARGO BAY","TECH CORE (X4 DMG)","HIRE A WINGMAN","JOIN A FLEET BATTLE","BIGGER FUEL TANK","WARP DRIVE","LEAVE"};
    while(1){
        if(dirty){ dirty=0; char b[48];
            for(int i=0;i<240*160/2;i++) pg[i]=57|(57<<8);
            char nm[24]; sys_name(cur_sys,nm); scpy(b,nm); scat(b," STATION"); text(pg,6,4,b,C_GOLD,1);
            scpy(b,"G"); ncat(b,gold); text(pg,190,4,b,C_GOLD,1);
            text(pg,6,14,"MARKET",tab==0?C_WHITE:C_DGREY,1); text(pg,46,14,"SHIPYARD",tab==1?C_WHITE:C_DGREY,1); text(pg,90,14,"LOUNGE",tab==2?C_WHITE:C_DGREY,1); if(ant_king) text(pg,122,14,"NAV",tab==3?C_WHITE:C_DGREY,1); scpy(b,eco_name[sys[cur_sys].eco]); scat(b," WORLD"); text(pg,150,14,b,C_CYAN,1);
            if(tab==0){
                int n=0; for(int i=0;i<8;i++) n+=cargo[i];
                text(pg,14,26,"GOODS        PRICE   YOU HAVE",C_GREY,1);
                for(int i=0;i<8;i++){ if(i==sel) text(pg,6,36+i*10,">",C_CYAN,1); scpy(b,goods[i]); while(textlen(b)<13) scat(b," "); ncat(b,good_price(i)); scat(b,"G"); while(textlen(b)<21) scat(b," "); ncat(b,cargo[i]);
                    text(pg,14,36+i*10,b,i==sel?C_WHITE:C_GREY,1); }
                scpy(b,"HOLD "); ncat(b,n); scat(b,"/"); ncat(b,cargo_cap); text(pg,14,122,b,C_WHITE,1);
                ui_lhint(pg,6,148,"A: BUY  B: SELL  L/R: SHIPYARD  START: LEAVE",C_CYAN);
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
                ui_lhint(pg,6,148,"UP/DOWN: TALK TO SOMEONE  L/R: MARKET  START: LEAVE",C_CYAN);
            } else if(tab==3){
                dp_nav_draw(pg,sel);
            } else {
                int cost[11]={ (fuel_max()-fuel)*2, (100-hull)*3, laser_lv<4?laser_lv*900:0, shield_lv<8?shield_lv*1000:0, cargo_cap<2000?cargo_cap*10:0, tech_cost(), wing_n<MAXWING?300+wing_n*150:0, 0,
                    fuel_tank<8?400*(fuel_tank+1):0, warp_lv<8?1500*(warp_lv+1)*(warp_lv+1):0, 0 };
                for(int i=0;i<11;i++){ int yy=24+i*8; if(i==sel) text(pg,6,yy,">",C_CYAN,1); scpy(b,yard[i]); if(i==9 && warp_lv<8){ scat(b," MK"); ncat(b,warp_lv+1); } while(textlen(b)<20) scat(b," ");
                    if(i==5 && beast_stage<1) scat(b,"LOCKED"); else if(i<7 || i==8 || i==9){ if(cost[i]) { ncat(b,cost[i]); scat(b,"G"); if(i==5){ scat(b,"+"); ncat(b,tech_scrap()); scat(b," SCRAP"); } } else scat(b, i<2?"FULL":"MAX"); }
                    else if(i==7) scat(b, battle_req ? "READY" : "FREE");
                    text(pg,14,yy,b,i==sel?C_WHITE:C_GREY,1); }
                { char r2[40]; u32 r[LYW]={0}; int wb=warp_bits(); if(wb>154) wb=154; r[wb>>5]=1u<<(wb&31); big_mul(r,LYW,ULY); ly_text(r2,r);
                  scpy(b,"WARP RANGE "); scat(b,r2); text(pg,14,113,b,C_CYAN,1); }
                scpy(b,"WINGMEN "); ncat(b,wing_n); scat(b,"/"); ncat(b,MAXWING); scat(b,"   LASER MK"); ncat(b,laser_lv); scat(b,"  SHIELD MK"); ncat(b,shield_lv); text(pg,14,121,b,C_WHITE,1);
                scpy(b,"HULL "); ncat(b,hull); scat(b,"%  FUEL "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10); scat(b,"/"); ncat(b,fuel_max()/10); scat(b," LY  CARGO "); ncat(b,cargo_cap); text(pg,14,129,b,C_WHITE,1);
                scpy(b,"TECH "); ncat(b,tech_lv); scat(b,"  SCRAP "); ncat(b,scrap); text(pg,14,137,b,C_CYAN,1);
                ui_lhint(pg,6,148,"A: BUY  L/R: MARKET  START: LEAVE",C_CYAN);
            }
        }
        vsync(); read_keys();
        int nt = ant_king ? 4 : 3, n = tab==0 ? 8 : tab==2 ? 3 : tab==3 ? 4 : 11;
        if(HIT(K_UP)){ sel=(sel+n-1)%n; dirty=1; } if(HIT(K_DOWN)){ sel=(sel+1)%n; dirty=1; }
        if(HIT(K_R)){ tab=(tab+1)%nt; sel=0; dirty=1; } if(HIT(K_L)){ tab=(tab+nt-1)%nt; sel=0; dirty=1; }
        if(HIT(K_START) || (tab==1 && HIT(K_A) && sel==10)) break;
        if(tab==3){
            if(HIT(K_A)){ if(dp_nav_press(pg,sel)) break; dirty=1; }
        } else if(tab==0){
            int held=0; for(int i=0;i<8;i++) held+=cargo[i];
            if(HIT(K_A)){ int p=good_price(sel); if(gold>=p && held<cargo_cap){ gold-=p; cargo[sel]++; sfx(7); } dirty=1; }
            if(HIT(K_B) && cargo[sel]){ gold+=good_price(sel); cargo[sel]--; sfx(7); dirty=1; }
        } else if(HIT(K_A) && sel==6){ int c=300+wing_n*150; if(wing_n>=MAXWING) say("YOU HAVE A FULL WING"); else if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; wing_n++; sfx(6); } dirty=1;
        } else if(HIT(K_A) && sel==7){ battle_req=1; sfx(6); dirty=1;
        } else if(HIT(K_A) && sel==8){ int c=400*(fuel_tank+1); if(fuel_tank>=8) say("THE TANK IS AS BIG AS IT GETS"); else if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; fuel_tank++; fuel+=40; sfx(6); } dirty=1;
        } else if(HIT(K_A) && sel==9){ int c=1500*(warp_lv+1)*(warp_lv+1); if(warp_lv>=8) say("YOUR DRIVE REACHES ANYWHERE ALREADY"); else if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; warp_lv++; sfx(6); } dirty=1;
        } else if(HIT(K_A)){
            int cost[6]={ (fuel_max()-fuel)*2, (100-hull)*3, laser_lv<4?laser_lv*900:0, shield_lv<8?shield_lv*1000:0, cargo_cap<2000?cargo_cap*10:0, tech_cost() };
            if(sel==5 && cost[5] && scrap<tech_scrap()){ say("NEED MORE TECH SCRAP. SCRAP GEAR AT YOUR SHIP"); }
            else if(cost[sel] && gold>=cost[sel]){ gold-=cost[sel]; if(sel==5) scrap-=tech_scrap(); sfx(6);
                if(sel==0) fuel=fuel_max(); else if(sel==1) hull=100; else if(sel==2) laser_lv++; else if(sel==3){ shield_lv++; shield=20+shield_lv*20; } else if(sel==4){ cargo_cap+=50; if(cargo_cap>2000) cargo_cap=2000; } else tech_lv++; }
            dirty=1;
        }
    }
    wait_release();
}

// ---------------------------------------------------------------- the ship's own save block (after the main save)
typedef struct { u32 magic; s16 cur_sys, cluster_no, fuel, hull, shield_lv, laser_lv, cargo_cap, ant_king, cleared_mask, zones_open, visits, w_dlv; u16 cargo[8]; u32 sum; } SaveX;
EW static SaveX svx;
_Static_assert(__builtin_offsetof(SaveX,sum)==sizeof(SaveX)-4, "SaveX: the sum must be the last 4 bytes");
static u32 sx_sum(const SaveX *s){ const u8 *p=(const u8*)s; u32 h=2166136261u; for(unsigned i=0;i<sizeof(SaveX)-4;i++){ h^=p[i]; h*=16777619u; } return h; }
static void tv_save(void);
static void save_ext(void){
    SaveX *s=&svx; memset(s,0,sizeof(SaveX)); s->magic=0x50494853;
    s->cur_sys=cur_sys; s->cluster_no=cluster_no; s->fuel=fuel; s->hull=hull; s->shield_lv=shield_lv; s->laser_lv=laser_lv; s->cargo_cap=cargo_cap;
    s->ant_king=ant_king; s->cleared_mask=cleared_mask; s->zones_open=zones_open; s->visits=visits; s->w_dlv=w_dlv; memcpy(s->cargo,cargo,8);
    s->sum=sx_sum(s); const u8 *p=(const u8*)s; for(unsigned i=0;i<sizeof(SaveX);i++) SRAM[slot_base(cur_slot)+SX_OFF+i]=p[i];
    beast_save(); trophy_save();
    { u32 a[NCW0+10]; a[0]=0x36444441u; a[1]=galaxy_no; for(int w=0;w<NCW0;w++) a[2+w]=clus[w]; a[NCW0+2]=relic_bits; a[NCW0+3]=(u32)weird_seen; a[NCW0+4]=(u32)relic_gold; a[NCW0+5]=mark_perm; a[NCW0+6]=(u32)wing_n|((u32)fuel_tank<<8)|((u32)warp_lv<<12); a[NCW0+7]=(u32)form|((u32)forms_open<<4)|((u32)tool_held<<8)|((u32)form_body<<12)|(tools_owned<<16); a[NCW0+8]=form_col|((u32)(form_t>0?1:0)<<16);
      u32 h=2166136261u; for(int i=0;i<NCW0+9;i++){ h^=a[i]; h*=16777619u; } a[NCW0+9]=h; sram_write(slot_base(cur_slot)+0x900,a,sizeof a); }
    { u32 g[NGH+2]; g[0]=0x324C4147u; for(int i=0;i<NGH;i++) g[1+i]=galaxy_hi[i]; u32 h=2166136261u; for(int i=0;i<NGH+1;i++){ h^=g[i]; h*=16777619u; } g[NGH+1]=h; sram_write(slot_base(cur_slot)+0x9C0,g,sizeof g); }   // the upper galaxy words: their own block (ends at 0x9EC), so older saves still load
    { u32 g[3]; g[0]=0x59444F42u; g[1]=(u32)(cur_pl*8+cur_moon); g[2]=(2166136261u^g[0])*16777619u; g[2]=(g[2]^g[1])*16777619u; sram_write(slot_base(cur_slot)+0x9EC,g,sizeof g); }   // which planet or moon you are on
    // (the spare memory past the slots holds the favourites now; old far-off addresses are folded into the compact universe, see u_settle)
    hum_save(); kart_save(); tv_save(); dp_save();
}
static void addr_load(void){ u32 a[NCW0+10]; sram_read(slot_base(cur_slot)+0x900,a,sizeof a); u32 h9=2166136261u, h7=2166136261u, h=2166136261u, h5=2166136261u; for(int i=0;i<NCW0+9;i++){ h9^=a[i]; h9*=16777619u; } for(int i=0;i<NCW0+7;i++){ h7^=a[i]; h7*=16777619u; if(i<NCW0+6){ h^=a[i]; h*=16777619u; } if(i<NCW0+5){ h5^=a[i]; h5*=16777619u; } }
    wing_n=0; fuel_tank=0; warp_lv=0; form=0; form_t=0; forms_open=1; tools_owned=0; tool_held=0; form_body=0; form_col=0;
    relic_bits=0; weird_seen=0; relic_gold=0; mark_perm=0; for(int k=0;k<NMK;k++) mark_t[k]=0; for(int w=0;w<NCW;w++) clus[w]=0; galaxy_no=0; for(int k=0;k<NGH;k++) galaxy_hi[k]=0;
    if(a[0]==0x36444441u && a[NCW0+9]==h9){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; mark_perm=a[NCW0+5]; wing_n=(int)(a[NCW0+6]&0xFF); fuel_tank=(int)((a[NCW0+6]>>8)&15); warp_lv=(int)((a[NCW0+6]>>12)&15); if(wing_n>MAXWING||wing_n<0) wing_n=0; if(fuel_tank>8) fuel_tank=8; if(warp_lv>8) warp_lv=8;
        u32 f=a[NCW0+7]; form=f&3; if(form>2) form=0; forms_open=((f>>4)&7)|1; tool_held=(f>>8)&7; form_body=(f>>12)%3; tools_owned=(f>>16)&0xFF; form_col=(u16)a[NCW0+8]; if((a[NCW0+8]>>16)&1) form=FORM_BUG; }   // a timed shape does not survive a reload
    else if(a[0]==0x35444441u && a[NCW0+7]==h7){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; mark_perm=a[NCW0+5]; wing_n=(int)a[NCW0+6]; if(wing_n>MAXWING||wing_n<0) wing_n=0; }
    else if(a[0]==0x34444441u && a[NCW0+6]==h){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; mark_perm=a[NCW0+5]; }
    else if(a[0]==0x33444441u && a[NCW0+5]==h5){ galaxy_no=a[1]; for(int w=0;w<NCW0;w++) clus[w]=a[2+w]; relic_bits=a[NCW0+2]; weird_seen=(int)a[NCW0+3]; relic_gold=(int)a[NCW0+4]; }
    else if(a[0]==0x52444441u && a[4]==(a[0]^a[1]*31^a[2]*131^a[3]*1031^0x5A5A)){ galaxy_no=a[1]; clus[0]=a[2]; clus[1]=a[3]; }   // last version's save
    else clus[0]=(u32)cluster_no;    // an older save: its cluster count becomes its address
    // (the old upper cluster words are no longer read: u_settle folds old far-off saves into the compact universe)
    { u32 g[NGH+2]; sram_read(slot_base(cur_slot)+0x9C0,g,sizeof g); int boff=0x9E0;      // new saves keep 9 upper words at 0x9C0 and the body at 0x9EC; the last version kept 3 words and the body at 0x9E0
      u32 hn=2166136261u; for(int i=0;i<NGH+1;i++){ hn^=g[i]; hn*=16777619u; } u32 ho=2166136261u; for(int i=0;i<4;i++){ ho^=g[i]; ho*=16777619u; }
      if(a[0]==0x36444441u){
          if(g[0]==0x324C4147u && g[NGH+1]==hn){ for(int i=0;i<NGH;i++) galaxy_hi[i]=g[1+i]; boff=0x9EC; }
          else if(g[0]==0x58474C47u && g[4]==ho){ galaxy_hi[0]=g[1]; galaxy_hi[1]=g[2]; galaxy_hi[2]=g[3]; } }
      { u32 b[3]; sram_read(slot_base(cur_slot)+boff,b,sizeof b); u32 bh=(2166136261u^0x59444F42u)*16777619u; bh=(bh^b[1])*16777619u; cur_pl=cur_moon=0;
        if(b[0]==0x59444F42u && b[2]==bh && b[1]<128u){ cur_pl=(int)(b[1]>>3); cur_moon=(int)(b[1]&7); } } }
}
typedef struct { u32 magic, hp_lo, hp_hi, mx_lo, mx_hi; u16 tech, kills, stage, hives; u32 sum; } SaveB;
_Static_assert(__builtin_offsetof(SaveB,sum)==sizeof(SaveB)-4, "SaveB: the sum must be the last 4 bytes");
static void beast_defaults(void){ hive_done=0; tech_lv=0; beast_kills=0; b3_max=1000000000000ULL; b3_hp=b3_max; beast_stage = ant_king ? 2 : (cleared_mask&4) ? 1 : 0; }
#define SB_OFF 0xA00
_Static_assert(sizeof(SaveX) <= 0x900-SX_OFF, "SaveX must end before the address block at +0x900");
_Static_assert((NCW0+10)*4 <= 0x9C0-0x900, "the address block must end before the galaxy words at +0x9C0");
_Static_assert((NGH+2)*4 <= 0x9EC-0x9C0, "the galaxy words must end before the body word at +0x9EC");
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
typedef struct { u32 magic; u32 ly[6], best[6], trips, hours; u32 sum; } SaveTv;   // the travel log as the last version kept it (6 words)
typedef struct { u32 magic; u32 ly[TVW], best[TVW], trips, hours; u32 sum; } SaveTv2;   // the travel log now: 8 words, so a jump between universes fits
_Static_assert(__builtin_offsetof(SaveTv2,sum)==sizeof(SaveTv2)-4, "SaveTv2: the sum must be the last 4 bytes");
#define TV2_OFF 0xF00
_Static_assert(TV2_OFF+sizeof(SaveTv2) <= SLOT_STRIDE, "the travel log must end inside the slot");
_Static_assert(__builtin_offsetof(SaveTv,sum)==sizeof(SaveTv)-4, "SaveTv: the sum must be the last 4 bytes");
#define TV_OFF 0xA40
_Static_assert(SB_OFF+sizeof(SaveB) <= TV_OFF && TV_OFF+sizeof(SaveTv) <= 0xB00, "the travel log sits between the beast block and the trophies");
static void tv_save(void){ SaveTv2 t; memset(&t,0,sizeof t); t.magic=0x324C5654u; for(int i=0;i<TVW;i++){ t.ly[i]=tv_ly[i]; t.best[i]=tv_best[i]; } for(int i=TVW;i<LYW;i++){ if(tv_ly[i]) for(int k=0;k<TVW;k++) t.ly[k]=0xFFFFFFFFu; if(tv_best[i]) for(int k=0;k<TVW;k++) t.best[k]=0xFFFFFFFFu; } t.trips=tv_trips; t.hours=tv_hours; t.sum=fnv(&t,sizeof t-4); sram_write(slot_base(cur_slot)+TV2_OFF,&t,sizeof t); }
static void tv_load(void){ for(int i=0;i<LYW;i++){ tv_ly[i]=0; tv_best[i]=0; } tv_trips=0; tv_hours=0;
    SaveTv2 t2; sram_read(slot_base(cur_slot)+TV2_OFF,&t2,sizeof t2);
    if(t2.magic==0x324C5654u && t2.sum==fnv(&t2,sizeof t2-4)){ for(int i=0;i<TVW;i++){ tv_ly[i]=t2.ly[i]; tv_best[i]=t2.best[i]; } tv_trips=t2.trips; tv_hours=t2.hours; return; }
    SaveTv t; sram_read(slot_base(cur_slot)+TV_OFF,&t,sizeof t);                   // a save from the last version
    if(t.magic!=0x4C565254u || t.sum!=fnv(&t,sizeof t-4)) return; for(int i=0;i<6;i++){ tv_ly[i]=t.ly[i]; tv_best[i]=t.best[i]; } tv_trips=t.trips; tv_hours=t.hours; }
typedef struct { u32 magic; u16 n[4], uq, scrap; u32 sum; } SaveTr;
_Static_assert(__builtin_offsetof(SaveTr,sum)==sizeof(SaveTr)-4, "SaveTr: the sum must be the last 4 bytes");
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
    beast_defaults(); kart_defaults(); dp_defaults(); for(int i=0;i<LYW;i++){ tv_ly[i]=0; tv_best[i]=0; } tv_trips=0; tv_hours=0; gen_cluster();
}
static int load_ext(void){
    SaveX *s=&svx; u8 *p=(u8*)s; for(unsigned i=0;i<sizeof(SaveX);i++) p[i]=SRAM[slot_base(cur_slot)+SX_OFF+i];
    if(s->magic!=0x50494853 || s->sum!=sx_sum(s)){ load_ext_defaults(); beast_load(); trophy_load(); tv_load(); return 0; }
    cur_sys=s->cur_sys; cluster_no=s->cluster_no; fuel=s->fuel; hull=s->hull; shield_lv=s->shield_lv; laser_lv=s->laser_lv; cargo_cap=s->cargo_cap;
    ant_king=s->ant_king; cleared_mask=s->cleared_mask; zones_open=s->zones_open; visits=s->visits; w_dlv=s->w_dlv; memcpy(cargo,s->cargo,8);
    beast_load(); trophy_load(); tv_load(); dp_load(); addr_load(); u_settle(); hum_load(); kart_load(); ore_load(); gen_cluster(); if(ant_king) kw_unlock(); return 1;
}
static void story_boss_beaten(void){
    cleared_mask|=1<<zone; if(zones_open<zone+2) zones_open=zone+2; if(zones_open>6) zones_open=6;
    if(zone==5 && !ant_king){ ant_king=1; uni_set32(rnd()|1); gen_cluster(); }
}

// ---------------------------------------------------------------- the pause screen's WORLD page and relic shelf
static void world_page(volatile u16 *pg, int y){
    char b[64], nm[24]; static const char *const tmp[3]={"TIMID","CURIOUS","HOSTILE"};
    if(ant_king) uni_name(b,uni_w); else scpy(b,"WHERE YOU ARE");          // after the story there are other universes: say which one this is
    text(pg,8,y,b,C_GOLD,1); text(pg,128,y,"YOU",C_GOLD,1); y+=9; int y0=y;
    gal_name(b); b[28]=0; text(pg,8,y,b,C_WHITE,1); y+=7;
    if(home_cluster()) scpy(b,"THE HOME CLUSTER"); else { char d2[40]; home_ly(d2); scpy(b,"HOME "); scat(b,d2); } b[29]=0; text(pg,8,y,b,C_WHITE,1); y+=7;   // (29 letters: HOME 511 QUATTUORDECILLION LY)
    sys_name(cur_sys,nm); scpy(b,"STAR "); scat(b,nm); if(cur_pl||cur_moon){ scat(b,"  PLANET "); ncat(b,cur_pl+1); if(cur_moon){ scat(b," MOON "); ncat(b,cur_moon); } } b[30]=0; text(pg,8,y,b,C_WHITE,1); y+=7;
    scpy(b, scene==2 ? "IN SPACE" : scene==1 ? (base_mode ? "IN A DRONE OUTPOST" : "IN A DUNGEON") : "ON THE SURFACE"); if(scene==1){ scat(b,", B"); ncat(b,depth_no); } text(pg,8,y,b,C_CYAN,1);
    { int yy=y0; const char *fn = form==FORM_CREATURE ? body_name[form_body] : form_name[form]; scpy(b,"SHAPE: "); scat(b,fn); if(form && form_t>0){ scat(b,"  "); ncat(b,(form_t+59)/60); scat(b,"M LEFT"); } text(pg,128,yy,b,C_WHITE,1); yy+=7;
      if(form==FORM_HUMAN){ if(tools_owned){ scpy(b,"HOLDING: "); scat(b,tool_name[tool_held]); } else scpy(b,"NO TOOLS YET"); text(pg,128,yy,b,C_CYAN,1); } yy+=7;
      scpy(b,"WINGMEN "); ncat(b,wing_n); scat(b,"/"); ncat(b,MAXWING); text(pg,128,yy,b,C_GREEN,1); yy+=7;
      { int rn=0; for(u32 r=relic_bits;r;r>>=1) rn+=r&1; scpy(b,"RELICS "); ncat(b,rn); scat(b,"/16"); text(pg,128,yy,b,rn?C_UNIQ:C_DGREY,1); } }
    y+=10; rect(pg,4,y-3,232,1,6);
    if(scene==2){ text(pg,8,y,"LAND ON A PLANET TO SEE IT HERE",C_DGREY,1); return; }
    scpy(b,"THIS WORLD: "); scat(b,pc_name[wd.cls]); text(pg,8,y,b,C_GOLD,1); rtext(pg,236,y,mood_name[wd.mood], wd.mood==MOOD_LOVE?C_GREEN:wd.mood==MOOD_HATE?C_RED:C_GREY); y+=8;
    scan_text(b); text(pg,8,y,b,C_WHITE,1); y+=7;
    scpy(b,style_name[wd.style]); scat(b,". "); scat(b,hour_name[wd.hour]); scat(b,". "); scat(b,weather_name[wd.weather]); scat(b,"."); if(wd.city) scat(b, wd.csize>=2 ? " A CITY." : " A TOWN."); text(pg,8,y,b,C_GREY,1); y+=7;
    scpy(b,"KNOWN FOR: "); scat(b,tagline[wd.tagline]); text(pg,8,y,b,C_GREY,1); y+=7;
    for(int s2=0;s2<2;s2++){ if(!spc[s2].maxpop) continue; scpy(b,"LIFE: "); ncat(b,spc[s2].pop); scat(b," "); scat(b,spc[s2].name); scat(b,"S, "); scat(b,tmp[spc[s2].temper%3]); text(pg,8,y,b,spc[s2].temper==2?C_RED:C_WHITE,1); y+=7; }
    if(wd.ntr){ char t1[28]; b[0]=0; for(int k=0;k<wd.ntr && k<2;k++){ trait_name(wd.tr[k],t1); if(b[0]) scat(b," + "); scat(b,t1); } scpy(nm,"WEIRD: "); text(pg,8,y,nm,C_RED,1); text(pg,36,y,b,C_UNIQ,1); y+=7;
        if(wd.ntr>2){ b[0]=0; for(int k=2;k<wd.ntr;k++){ trait_name(wd.tr[k],t1); if(b[0]) scat(b," + "); scat(b,t1); } text(pg,36,y,b,C_UNIQ,1); y+=7; } }
    else { text(pg,8,y,"NOTHING WEIRD HERE",C_DGREY,1); y+=7; }
    // your marks: the ones that last forever, and the ones still wearing off
    b[0]=0; int any=0;
    for(int k=0;k<NMK && textlen(b)<44;k++){ int f=(mark_perm>>k)&1, t=mark_t[k]; if(!f && t<=0) continue; if(any) scat(b,", "); scat(b,mk_name[k]); if(!f){ scat(b," "); ncat(b,(t+59)/60); scat(b,"M"); } any=1; }
    text(pg,8,y,"MARKS:",C_GOLD,1); text(pg,36,y, any?b:"NONE", any?C_GREEN:C_DGREY,1);
}
static void relic_shelf(volatile u16 *pg, int y){
    char b[48]; int rn=0; for(u32 r=relic_bits;r;r>>=1) rn+=r&1;
    scpy(b,"LEGENDARY RELICS  "); ncat(b,rn); scat(b,"/16"); text(pg,8,y,b,C_GOLD,1); y+=8;
    text(pg,8,y,"ONLY 16 IN THE UNIVERSE. EACH: +5% GOLD, +2 MAX HEALTH",C_GREY,1); y+=10;
    for(int i=0;i<16;i++){ int have=(relic_bits>>i)&1, x=8+(i&1)*116, yy=y+(i>>1)*8;
        rect(pg,x,yy,5,5, have?C_UNIQ:C_DGREY); text(pg,x+8,yy, have?relic_name[i]:"? ? ?", have?C_WHITE:C_DGREY,1); }
    y+=16*4+4;
    text(pg,8,y, rn ? "FIND MORE AT THE SHRINES ON WEIRD WORLDS" : "WEIRD WORLDS HIDE RELIC SHRINES. GO LOOK",C_CYAN,1);
}
