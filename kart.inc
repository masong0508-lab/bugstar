// ================================================================= KART RACING
// Every planet has its own tracks (4 to 9, sized to the planet), painted onto the ground as real roads: kerbs, a centre
// line, a start grid and boost pads, raised into bridges over water. Race up to 7 rivals of every kind of being (beetles,
// slugs, locals, this world's creatures, trees, rocks, your evil twin...) in four classes, in single races, cups and time
// trials, with items. Build your own track on any planet by dropping gates as you drive: up to 50 of them are kept on the
// cartridge (shared by the three slots) and you can fly back to race them again. The kart garage (kart2.inc) dresses your kart.
#define KG_MAX 24                                  // gates on a track
#define KP_MAX 600                                 // points on a track's path (about one per half cell)
#define KMM_N 300                                  // minimap points (every other path point)
#define NRC 8                                      // karts in a race: you and 7 rivals
#define NKREC 50                                   // your tracks kept on the cartridge
static void thump(int big); static int kw_quit; static int kw_driver_on(void); static int kw_draw_driver(int slot, int cx, int feet, int inv, int flip);
enum { KRC_A=96, KRC_B, KRC_KR, KRC_KW, KRC_LINE, KRC_CK0, KRC_CK1, KRC_PAD, KRC_GLOW, KRC_RAMP, KRC_RAMP2 };   // BG palette: the road, your underglow, the ramps
static int kroad(int c){ return (c>=KRC_A && c<=KRC_PAD) || c==KRC_RAMP || c==KRC_RAMP2; }
_Static_assert(T_KDYN+NRC*16 <= 1024, "the karts built at run time must fit in sprite memory");

// ---------------------------------------------------------------- the parts of a kart
enum { KP_BODY, KP_SIZE, KP_PAT, KP_STICK, KP_WHEEL, KP_SPOIL, KP_EXH, KP_TOP, KP_TRAIL, KP_GLOW, KP_HORN, NKPART };
static const u8 kpart_n[NKPART]={NKBODY,3,12,8,12,8,8,16,10,8,8};
typedef struct { u8 part[NKPART]; u8 number, pad; u16 col[3]; char name[12]; } KLook;              // col: paint, trim, pattern
static const char *const ksize_name[3]={"MINI","NORMAL","CHONKY"};
static const char *const kpat_name[12]={"PLAIN","STRIPE","TWIN STRIPES","FLAMES","CHECKS","SPOTS","ZIGZAG","STARS","SPLIT","FADE","LIGHTNING","CAMO"};
static const char *const kstick_name[8]={"NONE","SPARKLE","HEART","SKULL","LADYBIRD","FLAME","BOLT","GRIN"};
static const char *const kwheel_name[12]={"SLICKS","WHITEWALLS","GOLD RIMS","CHROME","DONUTS","LEMONS","CHEESE","BUTTONS","NEON","STONE","COOKIES","EYEBALLS"};
static const char *const kspoil_name[8]={"NONE","LIP","WING","BIG WING","FIN","TWIN FINS","PROPELLER","SAIL"};
static const char *const kexh_name[8]={"HIDDEN","SINGLE","TWIN","QUAD","ROCKET","TRUMPET","CHIMNEY","KAZOO"};
static const char *const ktop_name[16]={"NONE","ANTENNA BALL","SIREN","CONE","PROPELLER HAT","FLOWER","CROWN","HORNS","FLAG","DISCO BALL","LAMP","CHERRY","TOP HAT","BOW","SHARK FIN","HALO"};
static const char *const ktrail_name[10]={"NONE","DUST","SPARKS","BUBBLES","HEARTS","RAINBOW","SMOKE","STARS","FIRE","CONFETTI"};
static const char *const kglow_name[8]={"NONE","CYAN","PINK","LIME","GOLD","RED","PURPLE","DISCO"};
static const char *const khorn_name[8]={"HONK","BEEP","MOO","QUACK","KAZOO","TRUMPET","BOING","SNEEZE"};
static const u16 kwheel_col[12][2]={{C15(34,34,40),C15(150,156,176)},{C15(30,30,36),C15(240,240,248)},{C15(30,30,36),C15(250,200,60)},{C15(60,62,70),C15(210,220,235)},
    {C15(240,140,190),C15(150,90,50)},{C15(250,230,60),C15(255,250,170)},{C15(250,200,60),C15(220,150,40)},{C15(220,50,60),C15(245,245,245)},
    {C15(40,255,180),C15(20,70,50)},{C15(120,120,124),C15(80,80,84)},{C15(170,110,60),C15(60,32,20)},{C15(240,240,240),C15(60,120,230)}};
static const u16 kglow_col[8]={0,C15(60,240,255),C15(255,90,200),C15(150,255,60),C15(255,210,60),C15(255,60,60),C15(170,90,255),C15(255,255,255)};
//                                   speed accel grip weight (1..5)
static const u8 kbody_stat[16][4]={{3,3,3,3},{2,3,3,4},{2,3,5,2},{3,4,3,2},{5,2,2,3},{3,4,4,1},{2,3,3,2},{3,3,4,2},{2,2,3,5},{1,2,5,4},{4,4,1,2},{4,3,4,2},{3,3,2,4},{2,2,2,5},{3,2,3,4},{3,5,2,1}};
static const s8 kwheel_mod[12][3]={{0,0,1},{0,0,0},{1,-1,0},{1,0,0},{0,1,-1},{0,0,0},{-1,0,1},{0,1,0},{0,0,0},{-1,-1,2},{0,1,0},{0,0,1}};
static const s8 kspoil_mod[8][2]={{0,0},{0,0},{0,1},{-1,2},{0,1},{0,1},{1,-1},{1,-1}};          // speed, grip
static const u8 kexh_boost[8]={0,0,1,1,2,1,0,1};
enum { KST_SPD, KST_ACC, KST_GRP, KST_WGT, KST_BST };
static int kstat(const KLook *l, int s){                                                  // 1..10
    const u8 *b=kbody_stat[l->part[KP_BODY]]; int w=l->part[KP_WHEEL], sp=l->part[KP_SPOIL], sz=l->part[KP_SIZE], v=0;
    switch(s){ case KST_SPD: v=b[0]*2+kwheel_mod[w][0]+kspoil_mod[sp][0]; break; case KST_ACC: v=b[1]*2+kwheel_mod[w][1]+(sz==0); break;
        case KST_GRP: v=b[2]*2+kwheel_mod[w][2]+kspoil_mod[sp][1]+(sz==0)-(sz==2); break; case KST_WGT: v=b[3]*2-(sz==0)*2+(sz==2)*3+(w==9); break;
        default: v=4+kexh_boost[l->part[KP_EXH]]*2+(w==5||w==8?2:0); }
    return v<1?1:v>10?10:v; }
// what each part costs to buy in the garage (0 = yours from the start; 9999 = you have to win it)
static const u16 kprice_body[16]={0,150,200,250,2500,300,0,400,500,1000,700,9999,800,1500,900,600};
static const u16 kprice_wheel[12]={0,0,300,250,180,120,140,90,400,200,160,666};
static const u16 kprice_spoil[8]={0,60,150,400,120,220,500,350};
static const u16 kprice_exh[8]={0,0,90,200,700,250,180,120};
static const u16 kprice_top[16]={0,40,120,60,150,80,9999,200,100,350,120,90,300,110,260,9999};
static const u16 kprice_pat[12]={0,0,60,200,90,70,120,150,50,80,180,140};
static const u16 kprice_stick[8]={0,30,30,60,40,50,50,40};
static const u16 kprice_trail[10]={0,0,150,120,200,500,100,300,400,350};
static const u16 kprice_glow[8]={0,200,200,200,250,200,250,900};
static const u16 kprice_horn[8]={0,30,60,60,80,100,70,90};
static int kprice(int p, int i){ switch(p){ case KP_BODY: return kprice_body[i]; case KP_SIZE: return i==1?0:300; case KP_PAT: return kprice_pat[i]; case KP_STICK: return kprice_stick[i];
    case KP_WHEEL: return kprice_wheel[i]; case KP_SPOIL: return kprice_spoil[i]; case KP_EXH: return kprice_exh[i]; case KP_TOP: return kprice_top[i]; case KP_TRAIL: return kprice_trail[i];
    case KP_GLOW: return kprice_glow[i]; default: return kprice_horn[i]; } }
static KLook kl;                                                                          // your kart
static u16 kown[NKPART];                                                                  // which parts you own (a bit each)
static u16 kst_races, kst_wins, kst_pods, kst_built, kst_tt, kst_cups[4]; static u8 kcls_open=1;   // your racing record
static int kowns(int p, int i){ return kprice(p,i)==0 || ((kown[p]>>i)&1); }
static void kart_defaults(void){
    { static const u8 dc[8]={0,1,1,1,1,0,1,1}; for(int i=0;i<8;i++) kcam[i]=dc[i]; }
    memset(&kl,0,sizeof kl); kl.part[KP_SIZE]=1; kl.part[KP_EXH]=1; kl.number=7; kl.col[0]=C15(220,50,60); kl.col[1]=C15(250,210,90); kl.col[2]=C15(240,240,248); scpy(kl.name,"ZOOMER");
    for(int p=0;p<NKPART;p++) kown[p]=0; kst_races=kst_wins=kst_pods=kst_built=kst_tt=0; for(int c=0;c<4;c++) kst_cups[c]=0; kcls_open=1; }

// ---------------------------------------------------------------- tracks
typedef struct { u8 n, laps, shape, surf, flags, riv, cls, pad; u8 gx[KG_MAX], gy[KG_MAX]; char name[16]; } KTrack;   // gates in texels (half cells)
#define KF_ITEMS 1
#define KF_PADS 2
#define KF_JUMPS 8                                 // ramps on dry land too (water always gets jumps or bridges)
#define KF_LOOPS 16                                // loop-de-loops
EW static KTrack kt;                               // the track being raced
EW static u8 kpx[KP_MAX], kpy[KP_MAX];             // its path, in texels
EW static u8 kmmx[KMM_N], kmmy[KMM_N];             // the same, shrunk to the minimap
EW static u16 kgate_pi[KG_MAX];                    // where each gate falls on the path
static int knp=0, kt_len=0, kt_painted=0, kmm_w=40, kmm_h=40;
static const char *const ksurf_name[6]={"ASPHALT","DIRT","ICE","NEON","CANDY","GOLD"};
static const u16 ksurf_col[6][5]={{C15(70,70,78),C15(84,84,92),C15(220,50,50),C15(240,240,240),C15(250,230,120)},{C15(130,96,60),C15(146,110,70),C15(90,64,40),C15(200,170,120),C15(220,200,150)},
    {C15(170,210,240),C15(190,225,250),C15(60,120,220),C15(250,250,255),C15(255,255,255)},{C15(20,14,40),C15(30,20,56),C15(255,40,200),C15(40,255,230),C15(255,255,120)},
    {C15(250,190,220),C15(255,210,230),C15(240,60,120),C15(255,255,255),C15(120,220,255)},{C15(220,170,40),C15(240,200,70),C15(140,90,20),C15(255,250,200),C15(255,255,255)}};
static const char *const kshape_name[16]={"OVAL","RING","FIGURE EIGHT","STAR","CLOVER","SQUARE","SNAKE","TRIANGLE","HAIRPIN","BEAN","HEART","WORLD TOUR","ZIGZAG","PRETZEL","SQUIGGLE","DOUBLE LOOP"};
static const char *const kadj[32]={"SOGGY","ROWDY","GRANNY'S","BUMPY","SPICY","HAUNTED","SLEEPY","TURBO","WOBBLY","GRUMPY","FANCY","STICKY","SHINY","ANGRY","LAZY","CRUNCHY",
    "MIDNIGHT","DUSTY","FROSTY","MEGA","TINY","SNEAKY","ROYAL","CHEESY","NOISY","SILLY","LUCKY","CURSED","FUZZY","SIZZLING","BREEZY","MOSSY"};
static const char *const knoun[16][2]={{"OVAL","EGG"},{"RING","DONUT"},{"EIGHT","KNOT"},{"STAR","SPARKLE"},{"CLOVER","PETALS"},{"BLOCK","BOX"},{"SNAKE","WIGGLE"},{"WEDGE","PIZZA"},
    {"HAIRPIN","PAPERCLIP"},{"BEAN","PEANUT"},{"HEART","VALENTINE"},{"WORLD TOUR","EQUATOR"},{"ZIGZAG","LIGHTNING"},{"PRETZEL","NOODLE"},{"SQUIGGLE","SPAGHETTI"},{"LOOPY LOOP","CURL"}};
static const char *const kpsize_name[6]={"TINY","SMALL","MEDIUM","LARGE","HUGE","GIANT"};
static int kwrap8(int d){ return ((d+128)&255)-128; }
static const u8 katan[65]={0,3,5,8,10,13,15,18,20,23,25,28,30,33,35,38,40,42,45,47,49,52,54,56,59,61,63,65,67,70,72,74,76,78,80,82,84,86,88,90,92,94,96,97,99,101,103,105,106,108,110,111,113,115,116,118,119,121,122,124,125,127,128,128,128};
static void katan_init(void){}
static int iatan2(s32 x, s32 y){                                 // the heading (0..1023, 0 = +y, 256 = +x) that points along (x, y)
    if(!x && !y) return 0; s32 ax=x<0?-x:x, ay=y<0?-y:y; while(ax>0x7FFF || ay>0x7FFF){ ax>>=1; ay>>=1; }
    int a = ax<=ay ? katan[(ax<<6)/(ay?ay:1)] : 256-katan[(ay<<6)/(ax?ax:1)];
    if(y<0) a=512-a; if(x<0) a=1024-a; return a&1023; }
static void kt_pt(int sh, int k, int n, u32 h, s32 *ox, s32 *oy){   // gate k of n on a shape, in 1/256ths of its size
    int t=(k*256)/n; s32 c=cosx(t), s=sinx(t), x=0, y=0, r;
    switch(sh){
        case 0: x=c; y=s*62/100; break;                                                                  // OVAL
        case 1: x=c; y=s; break;                                                                         // RING
        case 2: x=s; y=sinx(2*t)*62/100; break;                                                          // FIGURE EIGHT
        case 3: r=16384*64/100+cosx((5*t)&255)*34/100; x=(r*c)>>14; y=(r*s)>>14; break;                  // STAR
        case 4: r=16384*62/100+cosx((4*t)&255)*36/100; x=(r*c)>>14; y=(r*s)>>14; break;                  // CLOVER
        case 5: x=(c<0?-1:1)*isqrt((u32)((c<0?-c:c)<<14)); y=(s<0?-1:1)*isqrt((u32)((s<0?-s:s)<<14)); x=x*88/100; y=y*88/100; break;   // SQUARE
        case 6: r=16384*84/100+sinx((6*t)&255)*16/100; x=(r*c)>>14; y=(r*s)>>14; break;                  // SNAKE
        case 7: { int e=(k*3)/n, f=((k*3)%n)*256/n; static const s16 vx[4]={0,14189,-14189,0}, vy[4]={16384,-8192,-8192,16384}; int e2=(e+1)%3;   // TRIANGLE
                  x=vx[e]+(((vx[e2]-vx[e])*f)>>8); y=vy[e]+(((vy[e2]-vy[e])*f)>>8); } break;
        case 8: x=c*135/100; y=s*30/100+sinx((3*t)&255)*8/100; break;                                    // HAIRPIN
        case 9: r=16384*78/100+cosx((2*t)&255)*20/100+s*10/100; x=(r*c)>>14; y=(r*s)>>14; break;         // BEAN
        case 10: { s32 s3=(((s*s)>>14)*s)>>14; x=s3; y=(13*c-5*cosx((2*t)&255)-2*cosx((3*t)&255)-cosx((4*t)&255))/16; } break;   // HEART
        case 12: r=(k&1)?16384*55/100:16384; x=(r*c)>>14; y=(r*s)>>14; break;                            // ZIGZAG
        case 13: x=sinx((3*t)&255)*90/100; y=sinx((2*t)&255)*90/100; break;                              // PRETZEL
        case 14: { int a1=(h>>4)&15, a2=(h>>8)&15, a3=(h>>12)&15, p1=(h>>16)&255, p2=(h>>24)&255;        // SQUIGGLE
                   r=16384 + (cosx((t+p1)&255)*a1 + cosx((2*t+p2)&255)*a2 + cosx((3*t+p1+p2)&255)*a3)/64; r=r*72/100; x=(r*c)>>14; y=(r*s)>>14; } break;
        case 15: r=16384*36/100+c*72/100; x=(r*c)>>14; y=(r*s)>>14; x+=16384*30/100; break;             // DOUBLE LOOP
        default: x=c; y=s; }
    *ox=x>>6; *oy=y>>6; }
static const u8 kshape_gates[16]={10,10,16,15,16,12,14,12,10,12,14,12,12,20,14,16};
static int kgate_bad(int gx, int gy){                              // how bad a spot is for a gate: water, the town, the landing field
    int cx=(gx>>1)&TMASK, cy=(gy>>1)&TMASK, b=0;
    if(hmap[cy*TW+cx]<water_lv) b+=2; if(in_city_cell(cx,cy)) b+=8;
    int lx=((cx-64+64)&TMASK)-64, ly=((cy-64+64)&TMASK)-64; if(lx*lx+ly*ly<100) b+=3; return b; }
static void kt_name(KTrack *t, u32 h){
    static const char *const kcust[8]={"LOOP","CIRCUIT","DASH","SPEEDWAY","RACEWAY","SCRAMBLE","RUN","GRAND PRIX"};   // a track you built has no shape of its own
    const char *a=kadj[(h>>3)&31], *nn = t->shape<16 ? knoun[t->shape][(h>>9)&1] : kcust[(h>>9)&7]; char b[40];
    int pat=(h>>12)%7;
    if(pat==0){ char w[24]; scpy(w,world_name()); for(int i=0;w[i];i++) if(w[i]==' '){ w[i]=0; break; } scpy(b,w); scat(b," "); scat(b,nn); }
    else if(pat==1){ scpy(b,"THE "); scat(b,a); scat(b," "); scat(b,nn); }
    else { scpy(b,a); scat(b," "); scat(b,nn); }
    if(textlen(b)>15){ scpy(b,a); scat(b," "); scat(b,nn); } if(textlen(b)>15) scpy(b,nn);
    scpy(t->name,b); }
static int kt_count(void){ return 4+(int)(hmix(wd.tseed^0x7AC5u)%6u); }
static void kt_official(int idx, KTrack *t){                       // this planet's track number idx: always the same one
    u32 h=hmix(wd.tseed^0x4B415254u^((u32)idx*0x9E3779B1u)); memset(t,0,sizeof *t);
    int ps=wd.psz; static const u8 rmin[6]={10,12,14,16,18,20}, rmax[6]={13,16,20,24,27,30};
    int sh=(int)(h%15u); if(sh>=11) sh++;                         // (the world tour is chosen below)
    if(idx==0) sh = (int)((h>>5)%3u);                              // the first track is always an easy one
    if(ps<=2 && ((h>>7)%(ps==0?3:ps==1?6:12))==0 && idx) sh=11;     // small worlds: a track right round the planet
    t->shape=sh; int n=kshape_gates[sh]; t->n=n;
    int R=rmin[ps]+(int)((h>>11)%(u32)(rmax[ps]-rmin[ps]+1)); int rot=(h>>16)&255; s32 rc=cosx(rot), rs=sinx(rot);
    int bx=64, by=64, best=1<<30;
    for(int cand=0;cand<16;cand++){ u32 g=hmix(h^((u32)cand*0x85EBCA6Bu)); int cx=(int)(g&127), cy=(int)((g>>8)&127), sc=0;
        for(int k=0;k<n;k++){ s32 ux,uy; int gx,gy;
            if(sh==11){ gx=(cx+k*128/n)*2; gy=(cy*2+(int)((sinx((k*256*(1+(int)((h>>20)%3u))/n)&255)*(3+(int)((h>>22)&3)))>>13))&255; }
            else { kt_pt(sh,k,n,h,&ux,&uy); s32 rx=(ux*rc-uy*rs)>>14, ry=(ux*rs+uy*rc)>>14; gx=(cx*2+((rx*R*2)>>8))&255; gy=(cy*2+((ry*R*2)>>8))&255; }
            sc+=kgate_bad(gx,gy)*16; int k2=(k+n-1)%n; (void)k2; }
        if(sc<best){ best=sc; bx=cx; by=cy; } }
    for(int k=0;k<n;k++){ s32 ux,uy; int gx,gy;
        if(sh==11){ gx=(bx+k*128/n)*2; gy=(by*2+(int)((sinx((k*256*(1+(int)((h>>20)%3u))/n)&255)*(3+(int)((h>>22)&3)))>>13))&255; }
        else { kt_pt(sh,k,n,h,&ux,&uy); s32 rx=(ux*rc-uy*rs)>>14, ry=(ux*rs+uy*rc)>>14; gx=(bx*2+((rx*R*2)>>8))&255; gy=(by*2+((ry*R*2)>>8))&255;
               for(int s=1;s<=6 && (kgate_bad(gx,gy)&2);s++){ int nx=(bx*2+((rx*R*2*(10-s))>>8)/10)&255, ny=(by*2+((ry*R*2*(10-s))>>8)/10)&255; if(!(kgate_bad(nx,ny)&2)){ gx=nx; gy=ny; } } }   // pull gates in out of the sea
        t->gx[k]=(u8)gx; t->gy[k]=(u8)gy; }
    switch(wd.cls){ case PC_ICE: case PC_COCYTUS: t->surf=(h>>24)&1 ? 2 : 0; break; case PC_DESERT: case PC_MEADOW: case PC_JUNGLE: case PC_BONE: t->surf=(h>>24)%3 ? 1 : 0; break;
        case PC_ECUMENOPOLIS: case PC_OBSIDIAN: t->surf=(h>>24)&1 ? 3 : 0; break; case PC_PARADISE: t->surf=(h>>24)%3==0 ? 4 : 0; break; default: t->surf=(h>>24)%4==0 ? 1 : 0; }
    if(wd.weird && (h>>26)%3==0) t->surf=3+(int)((h>>28)%2u); if((h>>27)%41==0) t->surf=5;      // weird worlds: neon or candy. Now and then: a gold road
    t->flags=KF_ITEMS|((h>>29)&1 || idx==0 ? KF_PADS : 0)|(idx && (h>>30) ? KF_JUMPS : 0)|(idx && hmix(h^0x100Fu)%10u<7 ? KF_LOOPS : 0); t->riv=7; t->cls=255; t->laps=3;
    kt_name(t,h); }

// ---------------------------------------------------------------- the path: a smooth loop through the gates (Catmull-Rom)
static void kt_build(void){
    int n=kt.n, tot=0; s16 ugx[KG_MAX], ugy[KG_MAX]; ugx[0]=kt.gx[0]; ugy[0]=kt.gy[0];
    for(int i=0;i<n;i++){ int j=(i+1)%n; int dx=kwrap8(kt.gx[j]-kt.gx[i]), dy=kwrap8(kt.gy[j]-kt.gy[i]); tot+=isqrt((u32)(dx*dx+dy*dy)); if(i+1<n){ ugx[i+1]=ugx[i]+dx; ugy[i+1]=ugy[i]+dy; } }
    int mnx=9999, mxx=-9999, mny=9999, mxy=-9999; for(int i=0;i<n;i++){ if(ugx[i]<mnx) mnx=ugx[i]; if(ugx[i]>mxx) mxx=ugx[i]; if(ugy[i]<mny) mny=ugy[i]; if(ugy[i]>mxy) mxy=ugy[i]; }
    mnx-=4; mny-=4; mxx+=4; mxy+=4; int span=mxx-mnx>mxy-mny ? mxx-mnx : mxy-mny; if(span<8) span=8; int sc=(40<<12)/span;
    kmm_w=((mxx-mnx)*sc)>>12; kmm_h=((mxy-mny)*sc)>>12;
    int stride=1; while(tot/stride > KP_MAX-2*n) stride++;
    knp=0;
    for(int i=0;i<n;i++){
        int i0=(i+n-1)%n, i2=(i+1)%n, i3=(i+2)%n, x1=kt.gx[i], y1=kt.gy[i];
        int p0x=kwrap8(kt.gx[i0]-x1), p0y=kwrap8(kt.gy[i0]-y1), p2x=kwrap8(kt.gx[i2]-x1), p2y=kwrap8(kt.gy[i2]-y1);
        int p3x=p2x+kwrap8(kt.gx[i3]-kt.gx[i2]), p3y=p2y+kwrap8(kt.gy[i3]-kt.gy[i2]);
        int L=isqrt((u32)(p2x*p2x+p2y*p2y)), steps=L/stride; if(steps<1) steps=1;
        kgate_pi[i]=knp;
        for(int s=0;s<steps && knp<KP_MAX;s++){
            int t=(s<<8)/steps, t2=(t*t)>>8, t3=(t2*t)>>8;
            int qx=((p2x-p0x)*t + (2*p0x+4*p2x-p3x)*t2 + (p3x-p0x-3*p2x)*t3)*16>>9, qy=((p2y-p0y)*t + (2*p0y+4*p2y-p3y)*t2 + (p3y-p0y-3*p2y)*t3)*16>>9;
            kpx[knp]=(u8)(((x1*16+qx+8)>>4)&255); kpy[knp]=(u8)(((y1*16+qy+8)>>4)&255);
            if(!(knp&1) && (knp>>1)<KMM_N){ int ux=ugx[i]+((qx+8)>>4), uy=ugy[i]+((qy+8)>>4); kmmx[knp>>1]=(u8)(((ux-mnx)*sc)>>12); kmmy[knp>>1]=(u8)(((uy-mny)*sc)>>12); }
            knp++; } }
    kt_len=(tot+1)/2;
    if(!kt.laps){ int l=(360+kt_len/2)/(kt_len?kt_len:1); kt.laps = l<2?2:l>5?5:l; }
}
static s32 kp_cx(int i){ return ((s32)kpx[i]<<15)+(1<<14); }       // a path point, in 16.16 cells
static s32 kp_cy(int i){ return ((s32)kpy[i]<<15)+(1<<14); }
static int kp_dir(int i){ int a=(i+knp-2)%knp, b=(i+2)%knp; return iatan2(kwrap8(kpx[b]-kpx[a]),kwrap8(kpy[b]-kpy[a])); }   // which way the road runs at point i

// ---------------------------------------------------------------- painting the road into the land (and taking it away again)
static void kart_palette(void){
    const u16 *s=ksurf_col[kt.surf%6]; for(int i=0;i<5;i++) BG_PAL[KRC_A+i]=s[i];
    BG_PAL[KRC_CK0]=C15(20,20,24); BG_PAL[KRC_CK1]=C15(240,240,240); BG_PAL[KRC_PAD]=C15(255,170,30); BG_PAL[KRC_GLOW]=kglow_col[kl.part[KP_GLOW]&7]; BG_PAL[KRC_RAMP]=C15(255,220,40); BG_PAL[KRC_RAMP2]=C15(40,30,24);
    if(wd.hour==2) for(int i=KRC_A;i<=KRC_RAMP2;i++){ if(i!=KRC_GLOW) BG_PAL[i]=dim15(BG_PAL[i],9); } else if(wd.hour==1) for(int i=KRC_A;i<=KRC_RAMP2;i++){ if(i!=KRC_GLOW) BG_PAL[i]=lerp15(BG_PAL[i],rgb15(255,140,60),3); } }
#define KW_IN 704                                  // asphalt half-width: 2.75 texels (8.8)
#define KW_OUT 930                                 // kerbs out to 3.6 texels
#define KPF_GAP 1                                  // no road here: you jump it
#define KPF_RAMP 2                                 // a ramp up to a jump
#define KPF_PAD 4                                  // a boost pad
#define KPF_LOOP 8                                 // a loop-de-loop stands here
#define NKJ 10
#define NKL 3
static s16 klp[NKL]; static int nkl=0;             // the loop-de-loops: where each one stands on the path
static int kloop_j=0, kloop_t=0, kloop_dir=0, kloop_sd=1, kloop_ph=0, kloop_prev=-1, kloop_hide=0; static s32 kla=0, kloop_v=0, kloop_da=0;   // your ride round one (see below)
static int k_stunts=0;                             // Kart World's STUNTS option: 0 as the track comes, 1 lots, 2 none
EW static u8 kpf[KP_MAX];                          // what each point of the path is
static s16 kjl[NKJ], kjr[NKJ]; static int nkj=0;   // the jumps: where you take off, and where the road starts again
static int kbend(int a, int b){ int d0=kp_dir((a+knp)%knp), m=0; for(int i=a+1;i<=b;i++){ int cv=((kp_dir((i+knp)%knp)-d0+512)&1023)-512; if(cv<0) cv=-cv; if(cv>m) m=cv; } return m; }   // how far the road swings away from its heading at a, from a to b
static void kjump_add(int a, int b){               // a gap from a to b with a ramp before it
    if(nkj>=NKJ || a<12 || b+10>=knp) return; for(int k=a-9;k<=b;k++) if(kpf[k]) return;
    if(kbend(a-9,a)>64 || kbend(a,b+10)>96 || kbend(a-9,b+10)>128) return;                // only where the road runs fairly straight: a sharp bend gets a bridge instead
    for(int k=a-9;k<a;k++) kpf[k]|=KPF_RAMP; for(int k=a;k<=b;k++) kpf[k]|=KPF_GAP; kjl[nkj]=(s16)(a-1); kjr[nkj]=(s16)(b+1); nkj++; }
static void kt_features(const u8 *raw){
    for(int i=0;i<knp;i++) kpf[i]=0; nkj=0;
    for(int i=0;i<knp;){ if(raw[i]>=water_lv){ i++; continue; }
        int a=i; while(i<knp && raw[i]<water_lv) i++; int b=i-1, L=b-a+1;
        if(a<14 || b>knp-6 || L<3 || k_stunts==2) continue;                               // never at the start line; puddles get a bridge (and no stunts: all bridges)
        if(L<=24) kjump_add(a,b);                                                          // a short crossing: jump it
        else for(int p=a+4;p+30<=b;){ int n0=nkj; kjump_add(p+18,p+29); p += nkj>n0 ? 34 : 6; } }   // a long one: a bridge, with a jump every so often (on the straight bits)
    if(((kt.flags&KF_JUMPS) || k_stunts==1) && k_stunts!=2 && kt_len>45) for(int k=0;k<(k_stunts==1?3:2);k++){ static const u8 jq[3]={44,80,12}; int q=knp*jq[k]/100, best=-1, bc=999;   // ramps on dry land, on straight bits
        for(int d=-40;d<=40;d+=2){ int p=(q+d+knp)%knp; if(p<24 || p>knp-26) continue; int ok=1; for(int e=-12;e<=22;e++) if(kpf[(p+e+knp)%knp] || raw[(p+e+knp)%knp]<water_lv) ok=0; if(!ok) continue;
            int b1=kbend(p-9,p), b2=kbend(p,p+22), b3=kbend(p-9,p+22); if(b1>64 || b2>96 || b3>128) continue;   // straight before, and straight where you land
            int cv=b1+b2+b3; if(cv<bc){ bc=cv; best=p; } }
        if(best>=0 && nkj<NKJ){ for(int e=best-8;e<=best;e++) kpf[e]|=KPF_RAMP; kjl[nkj]=(s16)best; kjr[nkj]=(s16)(best+12); nkj++; } }
    nkl=0;
    if(((kt.flags&KF_LOOPS) || k_stunts==1) && k_stunts!=2 && kt_len>36){ int want = k_stunts==1 ? (kt_len>120?3:2) : kt_len>150 ? 2 : 1, win = k_stunts==1 ? 10 : 12+(wd.psz%6)/2, lim = k_stunts==1 ? 110 : 96;   // loop-de-loops: on straight dry bits
        for(int k=0;k<want && nkl<NKL;k++){ static const u8 lq[3]={24,66,92}; int q=knp*lq[k]/100, best=-1, bc=999;
            for(int d=-50;d<=50;d+=2){ int p=(q+d+knp)%knp; if(p<24 || p>knp-24) continue; int ok=1;
                for(int e=-win;e<=win && ok;e++){ int pp=(p+e+knp)%knp; if(kpf[pp] || raw[pp]<water_lv) ok=0; } if(!ok) continue;
                int b=kbend(p-win,p+win); if(b>lim) continue; if(b<bc){ bc=b; best=p; } }
            if(best>=0){ klp[nkl++]=(s16)best; for(int e=-12;e<=12;e++) kpf[(best+e+knp)%knp]|=KPF_LOOP; } } }
    if(kt.flags&KF_PADS){ int np=2+kt_len/70; if(np>6) np=6;                                // boost pads on the straightest bits of each stretch
        for(int k=0;k<np;k++){ int q=knp*(2*k+1)/(2*np), best=-1, bc=999, len = (k==0 && kt_len>90) ? 10 : 5;
            for(int d=-18;d<=18;d+=2){ int p=(q+d+knp)%knp; if(p<10 || p>knp-14) continue; int ok=1; for(int e=-3;e<len+3;e++) if(kpf[(p+e)%knp]) ok=0; if(!ok) continue;
                int c0=kp_dir(p), c1=kp_dir((p+len)%knp), cv=((c1-c0+512)&1023)-512; if(cv<0) cv=-cv; if(cv<bc){ bc=cv; best=p; } }
            if(best>=0) for(int e=0;e<len;e++) kpf[(best+e)%knp]|=KPF_PAD; } } }
static int kjump_z(int pi){                        // how high a rival flies over a jump (8.8 cells)
    for(int j=0;j<nkj;j++){ int d=(pi-kjl[j]+knp)%knp, span=(kjr[j]-kjl[j]+knp)%knp+3; if(d<span){ int t=(d<<8)/span; return span*44*t/256*(256-t)/64; } } return 0; }
static int kgap_at(int pi){ return kpf[pi%knp]&KPF_GAP; }
static void kt_paint(void){
    u8 raw[KP_MAX]; for(int i=0;i<knp;i++) raw[i]=(u8)(hm_bil(kp_cx(i),kp_cy(i))>>8);
    kt_features(raw);
    int line = kt.surf!=1 && kt.surf!=2, rise = 300/(g_hs?g_hs:3); if(rise>110) rise=110;
    for(int i=0;i<knp;i++){
        int f=kpf[i]; if(f&KPF_GAP) continue;                                                 // the gap stays water: fly over it
        int sum=0; for(int j=-4;j<=4;j++) sum+=raw[(i+j+knp)%knp]; int hp=sum/9; if(hp<water_lv+2) hp=water_lv+2;
        if(f&KPF_RAMP){ int a=i, e=i; while(a>0 && (kpf[a-1]&KPF_RAMP)) a--; while(e<knp-1 && (kpf[e+1]&KPF_RAMP)) e++;
            int s0=0; for(int j=-4;j<=4;j++) s0+=raw[(a+j+knp)%knp]; int hb=s0/9; if(hb<water_lv+2) hb=water_lv+2; int wj = (e+1<knp && (kpf[e+1]&KPF_GAP)) ? rise*6/5 : rise;
            hp = hb + wj*(i-a+1)/(e-a+1); }
        if(hp>255) hp=255;
        int a=(i+knp-1)%knp, b=(i+1)%knp, tx=kwrap8(kpx[b]-kpx[a]), ty=kwrap8(kpy[b]-kpy[a]), L=isqrt((u32)(tx*tx+ty*ty)); if(!L) L=1;
        int ux=(tx<<8)/L, uy=(ty<<8)/L, cx=kpx[i], cy=kpy[i];
        int start = i<=1 || i>=knp-1, pad=f&KPF_PAD, ramp=f&KPF_RAMP, kerb = ((i/6)&1) ? KRC_KR : KRC_KW;
        for(int dy=-5;dy<=5;dy++) for(int dx=-5;dx<=5;dx++){
            int al=dx*ux+dy*uy, la=dy*ux-dx*uy, aal=al<0?-al:al, ala=la<0?-la:la;
            if(ala>(5<<8) || aal>256+(ala>>1)) continue;                                      // only the texels across the road from this point
            int t=((cy+dy)&255)*HW+((cx+dx)&255);
            if(ala>KW_OUT){ if(!ramp) hfine[t]=(u8)((hfine[t]+hp+1)>>1); continue; }           // the shoulder eases into the land
            int c;
            if(ramp) c = ala>KW_IN ? KRC_RAMP2 : (i&1) ? KRC_RAMP : KRC_RAMP2;                 // a ramp: warning stripes across it
            else if(ala>KW_IN) c = kerb;                                                       // red and white kerbs
            else if(start) c = ((((la+1024)>>8)+i)&1) ? KRC_CK0 : KRC_CK1;                    // the chequered start line
            else if(pad && ala<560) c = ((((i*256-ala)>>7))&3)<2 ? KRC_PAD : KRC_A;             // boost chevrons
            else if(line && ala<120 && ((i>>2)&1)) c=KRC_LINE;                                // the dashed centre line
            else c = ((cx+dx)*7+(cy+dy)*13)&3 ? KRC_A : KRC_B;
            cmap[t]=(u8)c; hfine[t]=(u8)hp; } }
    kt_painted=1; }
static void land_colour(void);
static void kt_unpaint(void){ if(!kt_painted) return; land_smooth(); land_colour(); kt_painted=0; }

// ---------------------------------------------------------------- the racers
enum { KB_BEETLE, KB_SLUG, KB_LOCAL1, KB_LOCAL2, KB_CRIT0, KB_CRIT1, KB_TWIN, KB_FOSSIL, KB_TREE, KB_ROCK, KB_FLOWER, KB_DROID, KB_STALL, KB_WEB, KB_RUIN, KB_SHIP, NKBEING };
static const char *const kbeing_kind[NKBEING]={"BEETLE","SLUG","LOCAL","LOCAL","","","EVIL TWIN","FOSSIL","TREE","ROCK","FLOWER","DROID","MARKET STALL","WEB","RUIN","TINY SHIP"};
static const char *const kbeing_nm[NKBEING][4]={{"SIR CRUNCH","CLICKETY","SHELLDON","MANDIBLE MAX"},{"SLIMOTHY","TURBO SLUG","SLOW-MO JO","GOOEY LOUIE"},{"ZORP","BLIX","GRUNDLE","THE MAYOR'S SON"},
    {"WIBBLE","QUONK","AUNTIE FLO","SKEETER"},{"","","",""},{"","","",""},{"","","",""},{"MR BONES","OLD RATTLES","DINO DAVE","FEMUR"},{"OAKLEY","TWIGGY","SIR BRANCH","MAPLE SYRUP"},
    {"ROCKY","PEBBLES","STONEY","GRAVELLA"},{"PETALINA","BUD","SIR STAMEN","POLLEN PAM"},{"BOLT (HE INSISTED)","BOLT JR","BOLT 2","BOLT'S COUSIN"},{"THE SHOP","BIG SALE","OPEN LATE","SPECIAL OFFER"},
    {"WEBSLEY","STICKY RICKY","SILKY","TANGLES"},{"OLD STONES","RUBBLE","THE RUIN","ANCIENT ANN"},{"LIL SHIP","MINI MOTH","TINY TINA","ZOOM ZOOM"}};
static const char *const kbeing_brag[NKBEING]={"CRUNCH! I WIN!","SLOW AND STEADY. MOSTLY STEADY.","ZORP ZORP! VICTORY!","QUONK! QUONK!","","","EVIL ALWAYS WINS. MWA HA.","RATTLE RATTLE! FIRST!",
    "I WAS ROOTING FOR ME.","ROCK SOLID DRIVING.","I AM BLOOMING GREAT.","BEEP BOOP. TOLD YOU.","EVERYTHING MUST GO! FAST!","STUCK THE LANDING.","OLD BUT GOLD.","ZOOM ZOOM ZOOM!"};
typedef struct { s32 x, y, v, lane; s16 hd, pi; u8 lap, being, skill, item, spin, shield, boost, cloud, fin, place, nextg, po, item_n, ai_t, out, pad2; u16 ftime, pts, best, la; KLook look; char name[20]; } Racer;   // la: how far round a loop (32768 = once; 0 = not in one)
EW static Racer rc[NRC];
static int nrc=1;
enum { KS_COUNT, KS_RACE, KS_DONE };
static int k_skill_adj=0, k_items=1, k_catch=1;                    // rival skill, items (0 off, 1 on, 2 crazy), catch-up: set by Kart World's options
static int khd=0, kfp_keep=0;                                       // your kart's own heading (the camera follows it), and the walking view to go back to
static const u8 kcam_dist[5]={4,6,8,10,13}, kcam_hgt[5]={10,16,24,36,56};   // the kart camera: cells behind you, quarter cells above
static const s16 kcam_fov[4]={140,180,225,280};
static s32 kcam_base(void){ return (s32)kcam_dist[kcam[1]%5]<<16; }
static s32 kcam_up(void){ return (s32)kcam_hgt[kcam[2]%5]<<6; }
static void kcam_apply(void){ static const u8 ph[5]={70,66,58,46,34}; kfovk=kcam_fov[kcam[3]&3]; if(kr_mode){ cam_fp=kcam[0]&1; g_phz = cam_fp ? 64 : ph[kcam[2]%5]; } else g_phz=66; planet_tables(); }   // higher cameras look down more
static s32 kalt=0, dh_last=0; static int kvz4=0, kair=0, kair_t=0, ktrick=0, kwet=0, k_lap_t0=0, k_best_lap=0;   // in the air: your height, how fast you rise (x4), a trick; lap times
static int k_elim_t=0, k_elim_out=0;                                // elimination races (Kart World)
static int ks_state=0, k_clock=0, k_count=0, k_end_t=0, k_type=0, k_cls=1, k_trk=0, k_rec=-1, k_cup_i=0, k_finished=0, k_wrong=0, k_lapmsg=0, k_rocket=0;
static int kret_x=0, kret_y=0, kret_h=0;
static s32 kv=0; static int kdrift=0, kcharge=0, kboost_t=0, kturn=0, kstall=0;
static u8 kcup_trk[4];
static const u8 kcls_pct[5]={85,100,115,130,150};
static const char *const kcls_name[5]={"LARVA","PUPA","IMAGO","STARBUG","HYPER"};
#define K_VBASE 0x3200                             // a balanced kart's top speed in the PUPA class: 0.2 cells a 60th
static s32 k_vmax(const KLook *l){ return (s32)K_VBASE*kcls_pct[k_cls]/100*(82+kstat(l,KST_SPD)*3)/100; }
static const char *kplace_s(int p){ static const char *const s[9]={"","1ST","2ND","3RD","4TH","5TH","6TH","7TH","8TH"}; return s[p<0?0:p>8?8:p]; }
static void kfmt_time(char *b, int fr){ int cs=fr*100/60, s=cs/100; b[0]=0; ncat(b,s/60); scat(b,":"); if(s%60<10) scat(b,"0"); ncat(b,s%60); scat(b,"."); ncat(b,(cs%100)/10); }

// ---------------------------------------------------------------- borrowing a few of the planet's little things (pebbles, tufts) to be karts, boxes and slime
EW static u8 kslot[48], khid[48]; static u8 nkslot=0, nkhid=0;
static int kgrab(int kind, s32 x, s32 y){
    for(int i=NPO-1;i>=0;i--){ PObj *o=&po[i]; if(o->alive && o->kind!=O_PEBBLE && o->kind!=O_TUFT) continue; if(!o->alive && o->kind!=O_PEBBLE && o->kind!=O_TUFT) continue;
        if(nkslot>=48) return -1; o->kind=(u8)kind; o->alive=1; o->hp=0; o->var=0; o->x=x&((TW<<16)-1); o->y=y&((TW<<16)-1); kslot[nkslot++]=(u8)i; return i; }
    return -1; }
static void kgive_back(void){
    for(int k=0;k<nkslot;k++){ PObj *o=&po[kslot[k]]; o->kind=(kslot[k]&1)?O_PEBBLE:O_TUFT; o->alive=1; o->hp=3; int a=rndi(256), r=3+rndi(62); o->x=(64<<16)+cosx(a)*r*4; o->y=(64<<16)+sinx(a)*r*4; }
    nkslot=0; for(int k=0;k<nkhid;k++) po[khid[k]].alive=1; nkhid=0; }
static void khide_road(void){                                      // plants and rocks on the road step aside for the race
    for(int i=0;i<NPO && nkhid<48;i++){ PObj *o=&po[i]; if(!o->alive || o->kind<=O_CAGE || o->kind==O_BASE || (o->kind>=O_MARKET && o->kind<=O_LOCAL) || o->kind==O_SHRINE || o->kind>O_TUFT) continue;
        if(kroad(cmap[((o->y>>15)&HMASK)*HW+((o->x>>15)&HMASK)])){ o->alive=0; khid[nkhid++]=(u8)i; } } }
static void kpo_release(int i){ for(int k=0;k<nkslot;k++) if(kslot[k]==i){ po[i].kind=O_PEBBLE; po[i].alive=0; return; } }

// ---------------------------------------------------------------- items
enum { KI_NONE, KI_PEPPER, KI_SHELL, KI_SLIME, KI_BUBBLE, KI_MAGNET, KI_SNEEZE, KI_PEPPER3, KI_CLOUD, NKI };
static const char *const kitem_name[NKI]={"","HOT PEPPER","BEETLE SHELL","SLIME","BUBBLE","MAGNET","SNEEZE","3 PEPPERS","GRUMPY CLOUD"};
typedef struct { s32 x, y; s16 pi; s8 owner, tgt; u8 life, po; } KShell;
EW static KShell ksh[3]; EW static u8 kslm_po[6], kslm_life[6], kbox_po[15], kbox_t[15];
static int kitem_roll(int place){                                  // further back, better things
    int r=rndi(100);
    if(place<=1) return r<35?KI_SLIME : r<65?KI_BUBBLE : r<85?KI_PEPPER : KI_SHELL;
    if(place<=3) return r<25?KI_PEPPER : r<50?KI_SHELL : r<65?KI_SLIME : r<80?KI_BUBBLE : r<92?KI_MAGNET : KI_SNEEZE;
    return r<20?KI_PEPPER3 : r<38?KI_SHELL : r<52?KI_MAGNET : r<66?KI_CLOUD : r<80?KI_SNEEZE : r<92?KI_PEPPER : KI_BUBBLE; }
static void kspin(int r, const char *why){
    Racer *k=&rc[r]; if(k->fin || k->la || (r==0 && kloop_on)) return;                      // (nothing hits you upside down in a loop)
    if(k->shield){ k->shield=0; if(r==0) show_msg("YOUR BUBBLE POPPED!"); sfx(2); return; }
    k->spin=48; if(r==0){ show_msg(why); sfx(4); kboost_t=0; kdrift=0; kcharge=0; } }
static int kahead(int r){                                          // the racer just ahead of r (or -1)
    int want=rc[r].place-1; if(want<1) return -1; for(int i=0;i<nrc;i++) if(rc[i].place==want) return i; return -1; }
static void kuse_item(int r){
    Racer *k=&rc[r]; int it=k->item; if(!it) return;
    switch(it){
        case KI_PEPPER: case KI_PEPPER3: k->boost=50; if(r==0){ kboost_t=50; sfx(10); } if(it==KI_PEPPER3 && k->item_n>1){ k->item_n--; return; } break;
        case KI_SHELL: for(int s=0;s<3;s++) if(!ksh[s].life){ int t=kahead(r); int p=kgrab(O_KSHELL,k->x,k->y); if(p<0) break;
            ksh[s].x=k->x; ksh[s].y=k->y; ksh[s].pi=k->pi; ksh[s].owner=(s8)r; ksh[s].tgt=(s8)t; ksh[s].life=240; ksh[s].po=(u8)p; if(r==0) sfx(3); break; } break;
        case KI_SLIME: for(int s=0;s<6;s++) if(!kslm_life[s]){ s32 bx=k->x-((sinx(k->hd>>2)*3)<<2), by=k->y-((cosx(k->hd>>2)*3)<<2); int p=kgrab(O_KSLIME,bx,by); if(p<0) break; kslm_po[s]=(u8)p; kslm_life[s]=255; if(r==0) sfx(8); break; } break;
        case KI_BUBBLE: k->shield=255; if(r==0){ show_msg("BUBBLE ON! IT BLOCKS ONE HIT"); sfx(2); } break;
        case KI_MAGNET: { int t=kahead(r); k->boost=40; if(t>=0 && r==0) show_msg("MAGNET! WHOOSH"); if(r==0){ kboost_t=40; sfx(10); } } break;
        case KI_SNEEZE: for(int i=0;i<nrc;i++) if(i!=r && rc[i].place<k->place) kspin(i,"ACHOO! SOMEONE SNEEZED ON YOU"); if(r==0){ show_msg("ACHOO! EVERYONE AHEAD SPINS"); sfx(9); } break;
        case KI_CLOUD: for(int i=0;i<nrc;i++) if(rc[i].place==1 && i!=r){ rc[i].cloud=200; if(i==0) show_msg("A GRUMPY CLOUD! IT IS RAINING ON YOU"); } if(r==0) sfx(8); break; }
    k->item=0; k->item_n=0; }
static void kitems_tick(int n){
    for(int b=0;b<15;b++){ if(kbox_po[b]==255) continue; PObj *o=&po[kbox_po[b]];
        if(kbox_t[b]){ if(kbox_t[b]>n) kbox_t[b]-=n; else { kbox_t[b]=0; o->alive=1; } continue; }
        for(int r=0;r<nrc;r++){ s32 dx=wrapd(rc[r].x-o->x), dy=wrapd(rc[r].y-o->y); if((dx<0?-dx:dx)+(dy<0?-dy:dy)<0xE000){ o->alive=0; kbox_t[b] = k_items==2 ? 80 : 200;
            if(!rc[r].item && !rc[r].fin){ int it=kitem_roll(rc[r].place); if(kl.part[KP_TOP]==11 && r==0 && it<=KI_SLIME && rndi(3)==0) it=KI_PEPPER3;   // the cherry is lucky
                rc[r].item=(u8)it; rc[r].item_n = it==KI_PEPPER3 ? 3 : 1; rc[r].ai_t=(u8)(30+rndi(120)); if(r==0){ char b2[40]; scpy(b2,"GOT: "); scat(b2,kitem_name[it]); scat(b2,"! A: USE"); show_msg(b2); sfx(7); } }
            break; } } }
    for(int s=0;s<3;s++){ KShell *h=&ksh[s]; if(!h->life) continue; if(h->life>n) h->life-=n; else h->life=0;
        s32 tx, ty; int tg=h->tgt;
        if(tg>=0 && !rc[tg].fin){ s32 dx=wrapd(rc[tg].x-h->x), dy=wrapd(rc[tg].y-h->y); s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy);
            if(ad<0xC000){ kspin(tg,"BONK! A BEETLE SHELL GOT YOU"); h->life=0; }
            if(ad<(6<<16)){ tx=rc[tg].x; ty=rc[tg].y; } else { h->pi=(h->pi+3*n)%knp; tx=kp_cx(h->pi); ty=kp_cy(h->pi); } }
        else { h->pi=(h->pi+3*n)%knp; tx=kp_cx(h->pi); ty=kp_cy(h->pi); }
        s32 dx=wrapd(tx-h->x), dy=wrapd(ty-h->y); int a=iatan2(dx>>4,dy>>4); s32 sp=(K_VBASE*17/10)*n;
        s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy); if(ad<sp){ h->x=tx; h->y=ty; } else { h->x+=(sinx(a>>2)*sp)>>14; h->y+=(cosx(a>>2)*sp)>>14; }
        h->x&=(TW<<16)-1; h->y&=(TW<<16)-1; po[h->po].x=h->x; po[h->po].y=h->y;
        if(!h->life) kpo_release(h->po); }
    for(int s=0;s<6;s++){ if(!kslm_life[s]) continue; PObj *o=&po[kslm_po[s]]; if(frames%8==0) kslm_life[s]--;
        for(int r=0;r<nrc;r++){ s32 dx=wrapd(rc[r].x-o->x), dy=wrapd(rc[r].y-o->y); if((dx<0?-dx:dx)+(dy<0?-dy:dy)<0xC000 && !rc[r].spin){ kspin(r,"SPLAT! YOU HIT THE SLIME"); kslm_life[s]=0; break; } }
        if(!kslm_life[s]) kpo_release(kslm_po[s]); }
}

// ---------------------------------------------------------------- the rivals: every kind of being on this world
static void klook_random(KLook *l){
    for(int p=0;p<NKPART;p++) l->part[p]=(u8)rndi(kpart_n[p]); if(rndi(3)) l->part[KP_SIZE]=1; if(rndi(2)) l->part[KP_TOP]=0; if(rndi(2)) l->part[KP_STICK]=0;
    l->col[0]=cc_rgb(rndi(360),150+rndi(100),150+rndi(100)); l->col[1]=cc_rgb(rndi(360),120+rndi(130),180+rndi(70)); l->col[2]=cc_rgb(rndi(360),60+rndi(190),200+rndi(55)); l->number=(u8)rndi(100); }
static void krival(int r, int used){
    Racer *k=&rc[r]; memset(k,0,sizeof *k); int b;
    for(int tries=0;;tries++){ int q=rndi(100); b = q<14?KB_BEETLE : q<22?KB_SLUG : q<32?KB_LOCAL1 : q<42?KB_LOCAL2 : q<54?KB_CRIT0 : q<64?KB_CRIT1 : q<68?KB_TWIN : q<73?KB_FOSSIL : q<78?KB_TREE : q<83?KB_ROCK : q<87?KB_FLOWER : q<90?KB_DROID : q<93?KB_STALL : q<95?KB_WEB : q<97?KB_RUIN : KB_SHIP;
        if((b==KB_CRIT0 && !spc[0].maxpop) || (b==KB_CRIT1 && !spc[1].maxpop)) continue; if(!((used>>b)&1) || tries>20) break; }
    k->being=(u8)b;
    if(b==KB_CRIT0||b==KB_CRIT1){ scpy(k->name,spc[b-KB_CRIT0].name); }
    else if(b==KB_TWIN){ scpy(k->name,"EVIL "); scat(k->name,look.name); }
    else scpy(k->name,kbeing_nm[b][rndi(4)]);
    klook_random(&k->look); if(b==KB_TWIN){ k->look=kl; k->look.col[0]=C15(40,30,40); k->look.col[1]=C15(220,30,40); }
    static const u8 lo[5]={84,92,97,101,104}, hi[5]={92,100,104,108,112}; int sk=lo[k_cls]+rndi(hi[k_cls]-lo[k_cls]+1)+k_skill_adj; k->skill=(u8)(sk<60?60:sk>130?130:sk);
    k->lane=((s32)rndi(5)-2)<<14; }

// ---------------------------------------------------------------- building a racer's kart into sprite memory (behind and side views)
#define kview_px bug_px                                              // (the bug's picture buffer: they never build at the same time)
static void kart_compose(const KLook *l, int view){                // into kview_px: the body, its pattern, a sticker, the number, the exhaust and the spoiler
    int b=l->part[KP_BODY]; const u8 *src = view ? kb_side+b*512 : kb_rear+b*512; for(int i=0;i<512;i++) kview_px[i]=src[i];
    const u8 *pm=kpat_mask+(l->part[KP_PAT]*2+view)*64;
    if(l->part[KP_PAT]) for(int r=0;r<16;r++) for(int c=0;c<32;c++){ u8 *p=&kview_px[r*32+c]; if((*p==3||*p==4) && ((pm[r*4+(c>>3)]>>(c&7))&1)) *p=12; }
    if(view && l->part[KP_STICK]){ int r0=16, r1=0, c0=32, c1=0; for(int r=0;r<16;r++) for(int c=0;c<32;c++){ u8 v=kview_px[r*32+c]; if(v>=2&&v<=4){ if(r<r0) r0=r; if(r>r1) r1=r; if(c<c0) c0=c; if(c>c1) c1=c; } }
        if(r1>=r0){ const u8 *sp=kstk_px+(l->part[KP_STICK]-1)*36; int ox=(c0+c1)/2-2, oy=(r0+r1)/2-2;
            for(int y=0;y<6;y++) for(int x=0;x<6;x++){ int v=sp[y*6+x]; if(!v) continue; int yy=oy+y, xx=ox+x; if(yy<0||yy>15||xx<0||xx>31) continue; u8 *p=&kview_px[yy*32+xx]; if(*p>=2&&*p<=4) *p=(u8)v; else if(*p==12) *p=(u8)v; } } }
    if(!view){ int r0=16, r1=0; for(int r=0;r<16;r++) for(int c=12;c<20;c++){ u8 v=kview_px[r*32+c]; if(v>=2&&v<=4){ if(r<r0) r0=r; if(r>r1) r1=r; } else if(v==12){ if(r<r0) r0=r; if(r>r1) r1=r; } }
        if(r1-r0>=6){ int y0=(r0+r1)/2-3;                                                                   // a number plate
            for(int y=0;y<7;y++) for(int x=0;x<9;x++){ u8 *p=&kview_px[(y0+y)*32+12+x]; if(*p>=2 && *p!=6 && *p!=7) *p=11; }
            for(int d=0;d<2;d++){ int v=d==0 ? l->number/10 : l->number%10; u16 g=font['0'+v]; for(int k=0;k<15;k++) if(g&(1<<(14-k))) kview_px[(y0+1+k/3)*32+13+d*4+(k%3)]=1; } } }
    const u8 *an=kb_anchor+b*3; int top_r=an[0], tail=an[1], top_s=an[2];
    if(l->part[KP_EXH]){ const u8 *es = view ? kex_side+l->part[KP_EXH]*64 : kex_rear+l->part[KP_EXH]*192; int w = view?8:24, ox = view ? tail-5 : 4, oy=8;
        for(int y=0;y<8;y++) for(int x=0;x<w;x++){ int v=es[y*w+x]; if(!v) continue; int yy=oy+y, xx=ox+x; if(yy<0||yy>15||xx<0||xx>31) continue; u8 *p=&kview_px[yy*32+xx]; if(v==1 && *p) continue; *p=(u8)v; } }
    if(l->part[KP_SPOIL]){ const u8 *ss = view ? ksp_side+l->part[KP_SPOIL]*96 : ksp_rear+l->part[KP_SPOIL]*196; int w = view?12:28, hh = view?8:7, ox = view ? tail-3 : 2, oy = (view?top_s:top_r)+1-hh;
        for(int y=0;y<hh;y++) for(int x=0;x<w;x++){ int v=ss[y*w+x]; if(!v) continue; int yy=oy+y, xx=ox+x; if(yy<0||yy>15||xx<0||xx>31) continue; u8 *p=&kview_px[yy*32+xx]; if(v==1 && *p) continue; *p=(u8)v; } }
}
static void kart_tiles(int tile){                                  // pack kview_px (32x16) into eight 4bpp tiles
    volatile u16 *v=OBJ_VRAM+(tile-512)*16;
    for(int ty=0;ty<2;ty++) for(int tx=0;tx<4;tx++) for(int y=0;y<8;y++) for(int x=0;x<8;x+=4){ const u8 *p=&kview_px[(ty*8+y)*32+tx*8+x]; *v++=(u16)((p[0]|(p[1]<<4))|((p[2]|(p[3]<<4))<<8)); } }
static void kart_bank(int bank, const KLook *l){                   // a racer's sprite palette
    volatile u16 *P=OBJ_PAL+bank*16; u16 c=l->col[0], t=l->col[1];
    P[0]=0; P[1]=C15(14,16,28); P[2]=shade15(c,9); P[3]=c; P[4]=lift15(c,7); P[5]=t; P[6]=kwheel_col[l->part[KP_WHEEL]][0]; P[7]=kwheel_col[l->part[KP_WHEEL]][1];
    P[8]=C15(150,156,176); P[9]=C15(232,60,60); P[10]=C15(150,210,250); P[11]=C15(240,240,248); P[12]=l->col[2]; P[13]=C15(40,36,48); P[14]=C15(255,160,40); P[15]=lift15(t,8); }
static void kart_build(int r){                                     // racer r: palette bank 7+r, tiles at T_KDYN+r*16 (behind) and +8 (side)
    const KLook *l = r==0 ? &kl : &rc[r].look; kart_bank(7+r,l);
    kart_compose(l,0); kart_tiles(T_KDYN+r*16); kart_compose(l,1); kart_tiles(T_KDYN+r*16+8);
    if(r && rc[r].being==KB_TWIN){ volatile u16 *P=OBJ_PAL+15*16; for(int i=0;i<16;i++) P[i]=OBJ_PAL[48+i]; P[2]=C15(60,10,20); P[3]=C15(150,20,40); P[4]=C15(230,60,60); P[5]=C15(255,120,120); P[9]=C15(255,230,60); } }

// ---------------------------------------------------------------- horns (sound channel 1, or the noise channel for a sneeze)
static void khorn(int k){
    if(!opt_sfx) return;
    switch(k&7){
        case 0: SND(0x00)=0; SND(0x02)=(0xD<<12)|(3<<8)|(2<<6); SND(0x04)=0x8000|1350; break;                     // HONK
        case 1: SND(0x00)=0; SND(0x02)=(0xC<<12)|(1<<8)|(1<<6); SND(0x04)=0x8000|1880; break;                     // BEEP
        case 2: SND(0x00)=(7<<4)|(1<<3)|1; SND(0x02)=(0xF<<12)|(6<<8)|(2<<6); SND(0x04)=0x8000|1200; break;       // MOO
        case 3: SND(0x00)=(2<<4)|(1<<3)|3; SND(0x02)=(0xD<<12)|(2<<8)|(0<<6); SND(0x04)=0x8000|1700; break;       // QUACK
        case 4: SND(0x00)=0; SND(0x02)=(0xA<<12)|(4<<8)|(0<<6); SND(0x04)=0x8000|1600; break;                     // KAZOO
        case 5: SND(0x00)=(3<<4)|2; SND(0x02)=(0xE<<12)|(5<<8)|(2<<6); SND(0x04)=0x8000|1500; break;              // TRUMPET
        case 6: SND(0x00)=(2<<4)|1; SND(0x02)=(0xC<<12)|(3<<8)|(2<<6); SND(0x04)=0x8000|1400; break;              // BOING
        case 7: SND(0x18)=(0xF<<12)|(2<<8); SND(0x1C)=0x8000|(3<<4)|1; break; } }                                    // SNEEZE

// ---------------------------------------------------------------- where everyone is in the race
static int kscore(int r){ Racer *k=&rc[r]; if(k->fin) return 0x7FFF0000-k->place*1000; if(k->out) return -100000-k->place*1000; return k->lap*knp+k->pi; }
static void kplaces(void){
    for(int i=0;i<nrc;i++){ int p=1, si=kscore(i); for(int j=0;j<nrc;j++) if(j!=i){ int sj=kscore(j); if(sj>si || (sj==si && j<i)) p++; } if(!rc[i].fin && !rc[i].out) rc[i].place=(u8)p; } }
static void kpath_track(int r){                                    // the path point nearest a racer, searched near the last one
    Racer *k=&rc[r]; int tx=(k->x>>15)&255, ty=(k->y>>15)&255, best=k->pi, bd=1<<30;
    for(int d=-6;d<=22;d++){ int i=(k->pi+d+knp)%knp; int dx=kwrap8(kpx[i]-tx), dy=kwrap8(kpy[i]-ty), dd=dx*dx+dy*dy; if(dd<bd){ bd=dd; best=i; } }
    if(bd>144) for(int i=0;i<knp;i++){ int dx=kwrap8(kpx[i]-tx), dy=kwrap8(kpy[i]-ty), dd=dx*dx+dy*dy; if(dd<bd){ bd=dd; best=i; } }   // lost? look everywhere
    if(r && best<k->pi-knp/2) k->lap++;                                                       // a rival came round past the line
    k->pi=(s16)best; }
static void kfinish(int r){
    Racer *k=&rc[r]; if(k->fin) return; k->fin=1; k->place=(u8)(++k_finished); k->ftime=(u16)(k_clock>65000?65000:k_clock);
    if(r==0){ ks_state=KS_DONE; k_end_t=0; char b[40]; scpy(b,k->place==1?"YOU WIN! ":"FINISHED "); if(k->place>1) scat(b,kplace_s(k->place)); else { char t[12]; kfmt_time(t,k_clock); scat(b,t); }
        show_msg(b); sfx(6); music(5); } }

// ---------------------------------------------------------------- setting up a race on the track in kt
static void kart_place_grid(int r, int slot){
    int row=slot/2, side=(slot&1)?1:-1, pi=(knp-2-row*3+knp*4)%knp, dir=kp_dir(pi), nd=(dir+256)&1023;
    Racer *k=&rc[r]; k->pi=(s16)pi; k->hd=(s16)dir; k->lap=0; k->nextg=0;
    k->x=(kp_cx(pi)+((sinx(nd>>2)*side*9)>>1))&((TW<<16)-1); k->y=(kp_cy(pi)+((cosx(nd>>2)*side*9)>>1))&((TW<<16)-1); }   // two columns, 1.1 cells either side
static void kart_go(int riders, int grid_me, int keep, int repaint){   // paint the track, line everyone up and start the countdown
    if(!kr_mode){ kret_x=ppx; kret_y=ppy; kret_h=heading; }
    kgive_back(); katan_init(); kloop_on=0; kloop_ph=0; kla=0; kloop_prev=-1; kloop_hide=0;
    if(repaint || !kt_painted){ kt_unpaint(); kt_build(); kt_paint(); }
    khide_road(); kart_palette();
    nrc=riders<1?1:riders>NRC?NRC:riders; int used=0;
    { u16 pts=rc[0].pts; memset(&rc[0],0,sizeof(Racer)); rc[0].look=kl; scpy(rc[0].name,look.name); rc[0].skill=100; if(keep) rc[0].pts=pts; }
    for(int r=1;r<nrc;r++){ if(keep){ Racer *k=&rc[r]; k->lap=0; k->fin=0; k->place=0; k->item=0; k->item_n=0; k->spin=0; k->shield=0; k->boost=0; k->cloud=0; k->v=0; k->ftime=0; k->nextg=0; }
        else { krival(r,used); used|=1<<rc[r].being; } }
    if(grid_me<0) grid_me = nrc>=6 ? nrc-3 : nrc-1;
    int slot=0; for(int r=0;r<nrc;r++){ if(slot==grid_me) slot++; kart_place_grid(r, r==0?grid_me:slot); if(r) slot++; }
    for(int r=1;r<nrc;r++){ int p=kgrab(O_KART,rc[r].x,rc[r].y); rc[r].po=(u8)(p<0?255:p); if(p>=0) po[p].var=(u8)r; }
    for(int b=0;b<15;b++){ kbox_po[b]=255; kbox_t[b]=0; }
    if((kt.flags&KF_ITEMS) && k_items) for(int row=0;row<(k_items==2?5:3);row++){ static const u8 fr[3]={30,62,88}, fc[5]={16,34,52,70,88}; int pi=knp*(k_items==2?fc[row]:fr[row])/100, nd=(kp_dir(pi)+256)&1023;
        for(int c=0;c<3;c++){ int off=(c-1)*10; int p=kgrab(O_IBOX,kp_cx(pi)+((sinx(nd>>2)*off)>>1),kp_cy(pi)+((cosx(nd>>2)*off)>>1)); kbox_po[row*3+c]=(u8)(p<0?255:p); } }   // three across the road
    { int nd=(kp_dir(0)+256)&1023; for(int s=-1;s<=1;s+=2) kgrab(O_BEACON,kp_cx(0)+((sinx(nd>>2)*s*19)>>1),kp_cy(0)+((cosx(nd>>2)*s*19)>>1));   // flags either side of the start line
      for(int k=0;k<4;k++){ int pi=(k*4+3)%knp, nd2=(kp_dir(pi)+256)&1023, sd=(k&1)?1:-1; int p=kgrab(O_LOCAL,kp_cx(pi)+((sinx(nd2>>2)*sd*30)>>1),kp_cy(pi)+((cosx(nd2>>2)*sd*30)>>1)); if(p>=0){ po[p].var=(u8)(k&1); po[p].hp=0; } } }   // a few locals came to watch
    for(int s=0;s<3;s++) ksh[s].life=0; for(int s=0;s<6;s++) kslm_life[s]=0;
    for(int r=0;r<nrc;r++){ kart_build(r); rc[r].la=0; }
    if(!kr_mode) kfp_keep=cam_fp; kr_mode=1; kcam_apply();
    ppx=rc[0].x; ppy=rc[0].y; khd=heading=rc[0].hd; heading_rays(); cam_h=ground_h(ppx,ppy)+cam_up();
    kv=0; kdrift=kcharge=kboost_t=kturn=kstall=0; kart_boost=100; k_rocket=0; k_wrong=0; k_lapmsg=0; kair=0; ktrick=0; kwet=0; kalt=ground_h(ppx,ppy); k_lap_t0=0; k_best_lap=0;
    kr_mode=1; ks_state=KS_COUNT; k_count=250; k_clock=0; k_finished=0; k_end_t=0; race_on=0; kart_on=1; k_elim_t=0; k_elim_out=0;
    music(6); msg_timer=0; p_last=vbl_count; }
static void kart_stop(void){                                       // back to walking, where you were (in Kart World: back to its menu)
    kloop_on=0; kloop_ph=0; kla=0;
    if(kw_mode){ kgive_back(); kr_mode=0; kart_on=0; ks_state=0; nrc=1; kw_quit=1; cam_fp=0; g_phz=66; planet_tables(); return; }
    kgive_back(); kt_unpaint(); kr_mode=0; kart_on=0; ks_state=0; nrc=1; cam_fp=kfp_keep; g_phz=66; planet_tables();
    ppx=kret_x; ppy=kret_y; heading=kret_h; heading_rays(); cam_h=ground_h(ppx,ppy)+CAM_UP; kv=0;
    music(0); obj_palette(); species_palette(); planet_hud(); p_last=vbl_count; }

// ---------------------------------------------------------------- driving: your kart (one 60th at a time)
static int kgrav4(void){ return WL(MW_ZEROG) ? 1 : 3+wd.grav; }      // low gravity planets: floaty jumps
static void kart_splash(void){                                     // fell in the water: a kind fish throws you back to the road
    int p=rc[0].pi; for(int j=0;j<nkj;j++){ int d=(p-kjl[j]+knp)%knp; if(d<=(kjr[j]-kjl[j]+knp)%knp+6){ p=(kjl[j]-12+knp)%knp; break; } }
    if(kgap_at(p)) p=(p-14+knp)%knp;
    ppx=kp_cx(p); ppy=kp_cy(p); khd=heading=kp_dir(p); heading_rays(); kv=0; kair=0; kwet=0; kalt=ground_h(ppx,ppy); cam_h=kalt+CAM_UP; kstall=45; rc[0].pi=(s16)p; kboost_t=0; kdrift=0;
    show_msg("SPLASH! A KIND FISH THREW YOU BACK"); sfx(8); }
static s32 kcam_back(void){ s32 b=kcam_base(); if(kcam[6]){ s32 v=kv>0?kv:0; if(v>K_VBASE*3/4) b+=(v-K_VBASE*3/4)*8; if(kboost_t) b+=1<<15; } return b; }   // speed zoom: the camera falls back as you go faster
static void kjump_throw(void){                                      // off a jump's ramp at a good speed: fly far enough to land on the road beyond
    int p=rc[0].pi;
    for(int j=0;j<nkj;j++){ int before=(kjl[j]-p+knp)%knp, after=(p-kjl[j]+knp)%knp; if(before>8 && after>4) continue;
        int lp=(kjr[j]+5)%knp; s32 dx=wrapd(kp_cx(lp)-ppx)>>8, dy=wrapd(kp_cy(lp)-ppy)>>8, D=isqrt((u32)(dx*dx+dy*dy)), v=kv>>8; if(v<8) return;
        s32 T=D/v; if(T<12) T=12; s32 H=kalt-ground_h(kp_cx(lp),kp_cy(lp)), g=kgrav4();
        s32 need=(g*(T-1))/2-(4*H)/T; if(need>360) need=360; if(need<-60) need=-60; kvz4=(int)need;   // just the right throw: not short (splash), not long (past the bend)
        int want=iatan2(dx,dy), err=((want-khd+512)&1023)-512; if(err>90) err=90; if(err<-90) err=-90; khd=(khd+err)&1023; return; } }   // and the ramp points you at the road on the far side
// ---------------------------------------------------------------- loop-de-loops: the road climbs up, over your head and back down
// The land can't bend over itself, so a loop is drawn into the picture as a ribbon of road. When you hit one fast enough the
// camera swings out to the side and you watch your kart go all the way round; too slow and it spits you back out.
#define KLOOP_FULL (1024<<8)
static s32 kloop_r(void){ static const u8 r4[6]={12,13,14,16,18,20}; return (s32)r4[wd.psz%6]<<6; }   // a loop's radius, 8.8 cells: 3 on a tiny world up to 5 on a giant one
static s32 kloop_zs(void){ return (kloop_r()*120/40)<<8; }          // how far away the side camera watches from (16.16 cells): the loop fills the view
static void kloop_fov(int fk){ kfovk=fk; for(int c=0;c<120;c++) raydx[c]=(s16)(((2*c-119)*fk)/120); heading_rays(); }   // (square pixels from the side, so the loop is round)
static int kproj(s32 wx, s32 wy, s32 wh, int *sx, int *sy, s32 *zc){  // a point on the map (16.16 cells), wh high (8.8 cells), onto the screen. 0: behind you
    s32 cb=cam_back(), cx=ppx-((hsn*cb)>>8), cy=ppy-((hcs*cb)>>8), dxw=wrapd(wx-cx), dyw=wrapd(wy-cy);
    s32 dz=((dxw*hsn)>>8)+((dyw*hcs)>>8), dx=((dxw*hcs)>>8)-((dyw*hsn)>>8); if(dz<(1<<15)) return 0;
    s32 z=dz>>8, f=(FOCP<<16)/z; long long xx=(((long long)dx*f)>>24)*256/fov_k(); s32 yy=PHZ+(((cam_h+curv(z)-tiny_sub()-wh)*f)>>16);
    if(xx<-2000) xx=-2000; if(xx>2000) xx=2000; if(yy<-2000) yy=-2000; if(yy>2000) yy=2000; *sx=120+(int)xx; *sy=(int)yy; *zc=z; return 1; }
#define KLSEG 28
#define KLSH 256                                                    // the loop is a little corkscrew: it comes out a cell to the right of where it went in
static void kloop_one(volatile u16 *pg, int j, int cy0, int cy1){                     // one loop: 28 pieces of ribbon, far ones first
    int pi=klp[j], d=kp_dir(pi); s32 ex=kp_cx(pi), ey=kp_cy(pi), R=kloop_r(), g=ground_h(ex,ey);
    { s32 cb=cam_back(), dxw=wrapd(ex-(ppx-((hsn*cb)>>8))), dyw=wrapd(ey-(ppy-((hcs*cb)>>8))), dz=((dxw*hsn)>>8)+((dyw*hcs)>>8), dx=((dxw*hcs)>>8)-((dyw*hsn)>>8);
      if(dz>(56<<16) || dz<-(8<<16) || dx>(48<<16) || dx<-(48<<16)) return; }               // too far away to matter
    s32 fx=sinx(d>>2)>>6, fy=cosx(d>>2)>>6, rx=cosx(d>>2)>>6, ry=-(sinx(d>>2)>>6);           // the road's forward and right (8.8)
    EW static s16 px[KLSEG+1][2], py[KLSEG+1][2]; EW static s32 zz[KLSEG+1]; u8 ok[KLSEG+1];
    for(int i=0;i<=KLSEG;i++){ int a=i*256/KLSEG; s32 sa=sinx(a&255), ca=cosx(a&255), fw=(R*sa)>>14, up=R-((R*ca)>>14), lat=KLSH*(2*i-KLSEG)/KLSEG;   // 8.8 cells along, up and across
        s32 cxp=ex+fx*fw+rx*lat, cyp=ey+fy*fw+ry*lat; ok[i]=1; zz[i]=0;
        for(int e=0;e<2;e++){ s32 w = e ? 290 : -290; s32 x=cxp+rx*w, y=cyp+ry*w; s32 z; int qx,qy; if(!kproj(x,y,g+up,&qx,&qy,&z)) ok[i]=0; else { zz[i]+=z; px[i][e]=(s16)qx; py[i][e]=(s16)qy; } } }
    int lim=KLSEG; if(kloop_hide==j+1 && !kloop_on){ int dd=(rc[0].pi-pi+knp)%knp; if(dd>24 && dd<knp/2) kloop_hide=0; else lim=KLSEG*5/8; }   // the half you just came down is behind and above you: leave it out
    u8 ord[KLSEG]; int n=0; for(int i=0;i<lim;i++) if(ok[i] && ok[i+1]) ord[n++]=(u8)i;
    for(int a=1;a<n;a++){ u8 t=ord[a]; int b=a; s32 zt=zz[t]+zz[t+1]; while(b>0 && zz[ord[b-1]]+zz[ord[b-1]+1]<zt){ ord[b]=ord[b-1]; b--; } ord[b]=t; }
    if(!sh_rt_ok) sh_rt_init();
    for(int k=0;k<n;k++){ int i=ord[k], i2=i+1; int qx[4]={px[i][0],px[i][1],px[i2][1],px[i2][0]}, qy[4]={py[i][0],py[i][1],py[i2][1],py[i2][0]};
        s32 cr=(s32)(qx[1]-qx[0])*(qy[3]-qy[0])-(s32)(qy[1]-qy[0])*(qx[3]-qx[0]);              // which side of the ribbon faces you
        int top = cr<0, col = top ? ((i&1)?KRC_A:KRC_B) : ((i&1)?C_DGREY:C_GREY);
        int kc=(i&1)?KRC_KR:KRC_KW; polyfill(pg,qx,qy,4,kc,0,cy0,240,cy1);                                 // the kerbs: the whole ribbon in the kerb colour...
        int ix[4]={qx[0]+2,qx[1]-2,qx[2]-2,qx[3]+2}; if(ix[0]<ix[1] && ix[3]<ix[2]) polyfill(pg,ix,qy,4,col,0,cy0,240,cy1);   // ...then the road inside it
        if(top && (i&3)==1) linep(pg,(qx[0]+qx[1])/2,(qy[0]+qy[1])/2,(qx[2]+qx[3])/2,(qy[2]+qy[3])/2,KRC_LINE,0,cy0,240,cy1); } }   // the centre line
static void kloop_draw(volatile u16 *pg, int cy0, int cy1){ if(!nkl || kr_mode!=1 || cy0>=cy1) return; for(int j=0;j<nkl;j++) kloop_one(pg,j,cy0,cy1); }
static void kloop_cam(void);
static void kloop_begin(int j){
    s32 vmax=k_vmax(&kl);
    if(kv<vmax*2/5){ kv=-kv/2-0x400; kstall=30; show_msg("TOO SLOW FOR THE LOOP! GO FASTER"); sfx(4); return; }   // not enough speed: it spits you back
    kloop_on=1; kloop_j=j; kloop_t=0; kloop_ph=1; kla=0; kloop_v=kv; kloop_dir=kp_dir(klp[j]);
    ppx=kp_cx(klp[j]); ppy=kp_cy(klp[j]); khd=kloop_dir; kdrift=0; kcharge=0; kair=0; rc[0].spin=0; kboost_t=0;
    { s32 zs=kloop_zs(); s32 lo=0x7FFFFFFF; int best=1;                                      // watch from whichever side is lower, so no hill is in the way
      for(int sd=-1;sd<=1;sd+=2){ int hh=(kloop_dir+(sd>0?768:256))&1023; s32 ox=(sinx(hh>>2)>>6)*(zs>>8), oy=(cosx(hh>>2)>>6)*(zs>>8);
          s32 h1=ground_h(ppx-ox,ppy-oy), h2=ground_h(ppx-ox/2,ppy-oy/2), m=h1>h2?h1:h2; if(m<lo){ lo=m; best=sd; } }
      kloop_sd=best; }
    { s32 C=(kloop_r()*1608)>>8; kloop_da=(kloop_v>>8)*262144/(C?C:1); if(kloop_da<KLOOP_FULL/150) kloop_da=KLOOP_FULL/150; if(kloop_da>KLOOP_FULL/56) kloop_da=KLOOP_FULL/56; }
    kloop_cam(); show_msg("LOOP-DE-LOOP!"); sfx(11); }
static void kloop_end(void){
    kloop_on=0; kla=0; kloop_ph=0; khd=heading=kloop_dir; kv=kloop_v;
    ppx=(ppx+(cosx(kloop_dir>>2)>>6)*KLSH)&((TW<<16)-1); ppy=(ppy-(sinx(kloop_dir>>2)>>6)*KLSH)&((TW<<16)-1); kloop_hide=kloop_j+1;   // out in the exit lane, beside the way in
    kcam_apply(); cam_h=ground_h(ppx,ppy)+cam_up(); kalt=ground_h(ppx,ppy);
    kboost_t=40; show_msg("WHEEE! LOOP TURBO!"); sfx(10); kloop_prev=rc[0].pi; }
static void kloop_cam(void){                                        // the side camera: level with the middle of the loop, far enough back to see all of it
    s32 R=kloop_r(), zs=kloop_zs();
    heading=(kloop_dir+(kloop_sd>0?768:256))&1023; g_phz=64; kloop_fov(256); kloop_cb=zs;
    s32 g=ground_h(ppx,ppy); cam_h=g+R;
    { s32 ox=(hsn*kloop_cb)>>8, oy=(hcs*kloop_cb)>>8; s32 gc=ground_h(ppx-ox,ppy-oy)+(1<<8); if(cam_h<gc) cam_h=gc; }   // never under the ground
    kv=0; kalt=g; }
static void kloop_step(void){                                       // one 60th of a loop: round you go, then a moment at the bottom, then back behind you
    kloop_t++;
    if(kloop_ph==1){ kla+=kloop_da; if(kla>=KLOOP_FULL){ kla=KLOOP_FULL; kloop_ph=2; kloop_t=0; sfx(12); } }
    else if(kloop_t>=12){ kloop_end(); return; }
    kloop_cam(); }
static int kloop_draw_me(int slot){                                 // you, going round: the kart from the side, turned with the loop
    s32 R=kloop_r(), hk=154; int pi=klp[kloop_j], d=kp_dir(pi); s32 ex=kp_cx(pi), ey=kp_cy(pi), g=ground_h(ex,ey);
    int a = kloop_ph==2 ? 1023 : (int)((kla>>8)&1023); s32 sa=sinx(a>>2), ca=cosx(a>>2), rr=R-hk, fw=(rr*sa)>>14, up=R-((rr*ca)>>14);
    s32 fx=sinx(d>>2)>>6, fy=cosx(d>>2)>>6, rx=cosx(d>>2)>>6, ry=-(sinx(d>>2)>>6), lat=KLSH*(a-512)/512; int x,y,cx,cy; s32 z,z2;
    if(!kproj(ex+fx*fw+rx*lat,ey+fy*fw+ry*lat,g+up,&x,&y,&z) || !kproj(ex+rx*lat,ey+ry*lat,g+R,&cx,&cy,&z2)) return slot;
    s32 ux=cx-x, uy=cy-y, l=isqrt((u32)(ux*ux+uy*uy)); if(l<1){ ux=0; uy=-256; } else { ux=ux*256/l; uy=uy*256/l; }   // the kart's "up" points at the middle of the loop
    int sz=kl.part[KP_SIZE], inv0 = sz==0 ? 232 : sz==2 ? 160 : 190; int inv=(int)((s32)inv0*(z>>4)/(6<<4)); if(inv<100) inv=100;
    s32 pa,pb,pc,pd; if(kloop_sd>0){ pa=-inv*uy/256; pb=inv*ux/256; pc=-inv*ux/256; pd=-inv*uy/256; } else { pa=inv*uy/256; pb=-inv*ux/256; pc=-inv*ux/256; pd=-inv*uy/256; }
    set_affine(29,(s16)pa,(s16)pb,(s16)pc,(s16)pd);
    shadow_oam[slot].a0=((y-16)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((x-32)&511)|(29<<9)|(2<<14); shadow_oam[slot].a2=(T_KDYN+8)|(7<<12); slot++;   // the side view
    int b=kl.part[KP_BODY], off=(kb_seat[b]-1)*256/inv;
    if(form==FORM_BUG && !(kw_mode && kw_driver_on())){ int bi=inv*3/2; s32 qa=-pa*bi/inv, qb=-pb*bi/inv, qc=pc*bi/inv, qd=pd*bi/inv; set_affine(31,(s16)qa,(s16)qb,(s16)qc,(s16)qd);   // your bug in the seat, from the side
        int dx=(int)(ux*off/256)+(int)(ux*8/bi), dy=(int)(uy*off/256)+(int)(uy*8/bi); shadow_oam[slot].a0=((y+dy-32)&255)|(1<<8)|(1<<9); shadow_oam[slot].a1=((x+dx-32)&511)|(31<<9)|(2<<14); shadow_oam[slot].a2=bug_tile(0)|BUG_BANK; slot++; }
    else if(form==FORM_CREATURE && form_body!=2){ int bi=inv*200/150; s32 qa=-pa*bi/inv, qb=-pb*bi/inv, qc=pc*bi/inv, qd=pd*bi/inv; set_affine(31,(s16)qa,(s16)qb,(s16)qc,(s16)qd);
        int dx=(int)(ux*off/256), dy=(int)(uy*off/256); shadow_oam[slot].a0=((y+dy-16)&255)|(1<<8)|(1<<9); shadow_oam[slot].a1=((x+dx-16)&511)|(31<<9)|(1<<14); shadow_oam[slot].a2=(form_body==0?T_HOPPER:T_GRAZER)|(6<<12); slot++; }
    return slot; }
static void kart_auto(s32 vt){                                     // after the flag: the kart drives itself round
    Racer *k=&rc[0]; int tp=(k->pi+8)%knp; s32 dx=wrapd(kp_cx(tp)-ppx), dy=wrapd(kp_cy(tp)-ppy); int want=iatan2(dx>>4,dy>>4);
    int err=((want-khd+512)&1023)-512; if(err>12) err=12; if(err<-12) err=-12; khd=(khd+err)&1023; kturn=err>4?1:err<-4?-1:0;
    kv += (vt-kv)/16; }
static void kart_step(void){
    s32 vmax=k_vmax(&kl);
    int up=HELD(K_UP), dn=HELD(K_DOWN), s = HELD(K_LEFT)?-1 : HELD(K_RIGHT)?1 : 0, h0=heading;
    if(kr_mode==1 && ks_state==KS_COUNT){ kv=0; return; }
    if(kloop_on){ kloop_step(); return; }
#ifdef TEST_KAUTO
    if(kr_mode==1 && ks_state==KS_RACE){ Racer *k=&rc[0]; int tp=(k->pi+5)%knp; int want=iatan2(wrapd(kp_cx(tp)-ppx)>>4,wrapd(kp_cy(tp)-ppy)>>4), err=((want-khd+512)&1023)-512; s = err>3?1 : err<-3?-1 : 0; up = (err<80 && err>-80) || kv<k_vmax(&kl)/3; dn=0; }   // the test driver
#endif
    int c=cmap[((ppy>>15)&HMASK)*HW+((ppx>>15)&HMASK)], fl = form==FORM_CREATURE && form_body==2;
    int water = hm_at(ppx,ppy)<water_lv && !fl && !kroad(c), onroad = kroad(c) || (kr_mode==2 && !water);
    int grip=kstat(&kl,KST_GRP), tr=5+grip/2; s32 vcap=vmax;
    if(onroad){ if(kt.surf==2 && kr_mode==1){ tr=tr*11/20; } else if(kt.surf==1 && kr_mode==1) tr=tr*9/10; if(wd.weather==1) tr=tr*9/10; else if(wd.weather==2) tr=tr*4/5; }   // ice, dirt, rain and snow are slippery
    else if(water) vcap=vmax*35/100; else vcap = (c>=82 && c<=87) ? vmax/2 : vmax*62/100;
    if(c==KRC_PAD && kboost_t<36 && !kair){ kboost_t=40; sfx(10); }
    if(kr_mode==1 && water && !kair && kgap_at(rc[0].pi)){ if(++kwet>20){ kart_splash(); return; } } else kwet=0;   // you dropped into a jump's water
    int boosting=0; if(HELD(K_B) && kart_boost>0 && !rc[0].spin){ boosting=1; if((frames&1) || kstat(&kl,KST_BST)<7) kart_boost--; } else if(!HELD(K_B) && kart_boost<100 && (frames%3)==0) kart_boost++;
    if(kboost_t){ kboost_t--; boosting=1; }
    if(boosting) vcap = onroad ? vmax*160/100 : vmax*120/100;
    if(rc[0].cloud) vcap=vcap*3/4;
    if(kr_mode==1 && ks_state==KS_DONE){ kart_auto(vmax*7/10); s=0; up=0; dn=0; }
    else if(rc[0].spin){ kv -= kv/16; s=0; }
    else if(kair){ kair_t++; khd=(khd+s*(tr/3))&1023; kturn=s; if(HIT(K_R) && !ktrick && kair_t<30){ ktrick=1; sfx(2); } }   // flying: a little steering; R does a trick
    else {
        s32 spd=kv<0?-kv:kv; int fa=(int)((spd*768)/(vmax?vmax:1)); if(fa>256) fa=256; tr=(tr*fa)>>8;
        if(HELD(K_R) && s && kv>vmax/2){ if(!kdrift){ kdrift=s; sfx(11); } if(s==kdrift){ tr=tr*3/2; if(kcharge<200) kcharge++; } }
        if(kdrift && !(HELD(K_R) && s==kdrift)){ if(kcharge>110){ kboost_t=50; show_msg("SUPER MINI-TURBO!"); sfx(10); } else if(kcharge>50){ kboost_t=28; sfx(10); } kdrift=0; kcharge=0; }
        khd=(khd+s*tr)&1023; kturn=s;
        int acc=(int)(vmax/(110-kstat(&kl,KST_ACC)*7)); if(acc<16) acc=16;
        if(kstall){ kstall--; up=0; }
        if(up) kv+=acc; else if(dn){ if(kv>0) kv-=vmax/30; else if(kv>-vmax/3) kv-=acc/2; } else kv-=kv/40;
        if(boosting && kv<vcap) kv+=acc*2; }
    if(kv>vcap && !kair) kv-=(kv-vcap)/10+1;                                                 // (in the air nothing slows you)
    s32 nx=ppx+((sinx(khd>>2)*kv)>>14), ny=ppy+((cosx(khd>>2)*kv)>>14);
    s32 g0=ground_h(ppx,ppy), g1=ground_h(nx,ny), dh=g1-g0, wall=0x80*g_hs;
    if(kair){ kalt+=kvz4>>2; kvz4-=kgrav4();
        if(g1>kalt+0x100){ kv=-kv/4; nx=ppx; ny=ppy; }                                            // flew into a cliff
        else if(kalt<=g1){ kair=0; kalt=g1;                                                        // touch down
            int c2=cmap[((ny>>15)&HMASK)*HW+((nx>>15)&HMASK)];
            if(hm_at(nx,ny)<water_lv && !kroad(c2) && kr_mode==1 && !fl){ ppx=nx&((TW<<16)-1); ppy=ny&((TW<<16)-1); kart_splash(); return; }
            if(ktrick){ kboost_t=42; show_msg("TRICK! TURBO!"); sfx(10); } else sfx(12); if(kcam[7] && kvz4<-60) cam_h-=(-kvz4)<<1; ktrick=0; } }   // a hard landing bumps the camera
    else if(dh>wall){ kv=-kv/3; thump(1); nx=ppx; ny=ppy; if(kcam[7]) cam_h-=0x60; }
    else if(-dh>0x60 && kv>vmax/4){ kair=1; kair_t=0; ktrick=0; kalt=g0; kvz4=(dh_last>0?dh_last:0)*4+40; if(kr_mode==1 && kv>vmax*45/100) kjump_throw(); sfx(11); }  // off the end of a ramp (or a cliff): fly!
    else kv-=(dh*vmax)>>11;
    if(!kair){ dh_last=dh; kalt=ground_h(nx,ny); }
    ppx=nx&((TW<<16)-1); ppy=ny&((TW<<16)-1);
    s32 want=(kair?kalt:ground_h(ppx,ppy))+cam_up(); if(!cam_fp){ s32 cg=ground_h(ppx-((hsn*CAM_BACK)>>8), ppy-((hcs*CAM_BACK)>>8)); if(cg+(1<<8)>want) want=cg+(1<<8); }
    cam_h+=(want-cam_h)>>(kair?5:3);                                                        // in the air the camera lags, so you see the jump
    { int tgt=khd; if(HELD(K_SEL) && kr_mode==1) tgt=(khd+512)&1023; else if(kcam[5]) tgt=(khd+kturn*56+(kdrift?kdrift*40:0))&1023;   // SELECT: look behind. LOOK INTO TURNS swings the view ahead
      int err=((tgt-heading+512)&1023)-512, sw=kcam[4]&3; if(HELD(K_SEL) && kr_mode==1) sw=0;
      heading = sw==0 ? tgt : (heading + (sw==1 ? err/2 : sw==2 ? err/4 : err/8))&1023; }                       // the camera swings round behind the kart: stiff, soft, normal or loose
    if(heading!=h0) heading_rays();
    if(nkl && kr_mode==1 && !kair && kv>0) for(int j=0;j<nkl;j++){ int d0=(klp[j]-rc[0].pi+knp)%knp; if(d0<1 || d0>16) continue;   // nearly at a loop: ease into its way-in lane (the left one)
        int d=kp_dir(klp[j]), off=((khd-d+512)&1023)-512; if(off>140 || off<-140) break;
        s32 rx=cosx(d>>2)>>6, ry=-(sinx(d>>2)>>6), lx=kp_cx(klp[j])-rx*KLSH, ly=kp_cy(klp[j])-ry*KLSH, ex=wrapd(lx-ppx), ey=wrapd(ly-ppy), side=((ex>>8)*rx+(ey>>8)*ry)>>8;   // how far right of the lane you are (8.8 cells)
        ppx=(ppx+rx*(side/10))&((TW<<16)-1); ppy=(ppy+ry*(side/10))&((TW<<16)-1); break; }
    rc[0].x=ppx; rc[0].y=ppy; rc[0].hd=(s16)khd; rc[0].v=kv; }

// ---------------------------------------------------------------- driving: a rival (once a frame, n 60ths at a time)
static void kai(int r, int n){
    Racer *k=&rc[r]; if(k->out) return;
    if(k->la){ s32 C=(kloop_r()*1608)>>8; s32 da=((k->v*n)>>8)*32768/(C?C:1); if(da<(32768/150)*n) da=(32768/150)*n; s32 nl=(s32)k->la+da;   // round a loop-de-loop
        if(nl>=32768){ k->la=0; k->boost=30; } else k->la=(u16)nl; return; }
    int pi0=k->pi; s32 vtop=(s32)K_VBASE*kcls_pct[k_cls]/100*k->skill/100;
    if(!k->fin && rc[0].fin==0 && k_catch){ int diff=kscore(r)-kscore(0); if(diff<-knp/3) vtop=vtop*109/100; else if(diff>knp/4) vtop=vtop*93/100; }
    if(k->fin) vtop=vtop*7/10;
    if(k->cloud){ vtop=vtop*3/4; k->cloud = k->cloud>n ? k->cloud-n : 0; }
    if(k->boost){ vtop=vtop*155/100; k->boost = k->boost>n ? k->boost-n : 0; }
    int la=5+(int)(k->v>>11), tp=(k->pi+la)%knp, dir=kp_dir(tp), nd=(dir+256)&1023;
    s32 tx=kp_cx(tp)+((sinx(nd>>2)*(k->lane>>8))>>6), ty=kp_cy(tp)+((cosx(nd>>2)*(k->lane>>8))>>6);
    s32 dx=wrapd(tx-k->x), dy=wrapd(ty-k->y); int want=iatan2(dx>>4,dy>>4), err=((want-k->hd+512)&1023)-512, trm=14*n;
    if(k->spin){ k->spin = k->spin>n ? k->spin-n : 0; k->v -= (k->v/8)*n/2; }
    else { if(err>trm) err=trm; if(err<-trm) err=-trm; k->hd=(s16)((k->hd+err)&1023);
        int d0=kp_dir(k->pi), d1=kp_dir((k->pi+12)%knp), cv=((d1-d0+512)&1023)-512; if(cv<0) cv=-cv; if(cv>170) cv=170;
        s32 vt=vtop*(256-cv/2)/256; if(k->v<vt){ k->v+=vt/40*n; if(k->v>vt) k->v=vt; } else k->v-=(k->v-vt)/8; }
    s32 st=k->v*n; k->x=(k->x+((sinx(k->hd>>2)*st)>>14))&((TW<<16)-1); k->y=(k->y+((cosx(k->hd>>2)*st)>>14))&((TW<<16)-1);
    int tx2=(k->x>>15)&255, ty2=(k->y>>15)&255;
    for(int g=0;g<24;g++){ int a=k->pi, b=(a+1)%knp; int da=kwrap8(kpx[a]-tx2)*kwrap8(kpx[a]-tx2)+kwrap8(kpy[a]-ty2)*kwrap8(kpy[a]-ty2), db=kwrap8(kpx[b]-tx2)*kwrap8(kpx[b]-tx2)+kwrap8(kpy[b]-ty2)*kwrap8(kpy[b]-ty2);
        if(db>da) break; k->pi=(s16)b; if(b==0){ k->lap++; if(k->lap>kt.laps) kfinish(r); } }
    for(int j=0;j<nkl;j++){ int d0=(klp[j]-pi0+knp)%knp, d1=(klp[j]-k->pi+knp)%knp; if(d0>0 && d0<30 && (d1==0 || d1>knp-30)){ k->la=1; k->pi=klp[j]; k->x=kp_cx(klp[j]); k->y=kp_cy(klp[j]); k->hd=(s16)kp_dir(klp[j]); return; } }
    if(cmap[ty2*HW+tx2]==KRC_PAD && !k->boost) k->boost=40;
    if(k->item && !k->fin){ if(k->ai_t>n) k->ai_t-=n; else { int t=kahead(r);
        if(k->item==KI_SHELL && t>=0){ s32 ex=wrapd(rc[t].x-k->x), ey=wrapd(rc[t].y-k->y); if((ex<0?-ex:ex)+(ey<0?-ey:ey)>(12<<16)){ k->ai_t=30; t=-2; } }
        if(t!=-2) kuse_item(r); } }
    if((frames+r*37)%240==0) k->lane=((s32)rndi(5)-2)<<14; }
static void kbump(void){                                           // karts that touch push each other apart; the heavy one wins
    for(int i=0;i<nrc;i++) for(int j=i+1;j<nrc;j++){ if(rc[i].la || rc[j].la || (i==0 && kloop_on)) continue; s32 dx=wrapd(rc[j].x-rc[i].x), dy=wrapd(rc[j].y-rc[i].y); if(dx>0xE000||dx<-0xE000||dy>0xE000||dy<-0xE000) continue;
        int ex=dx>>8, ey=dy>>8, d=isqrt((u32)(ex*ex+ey*ey)); if(d>=218) continue; if(d<1){ ex=1; d=1; }
        int wi=kstat(&rc[i].look,KST_WGT), wj=kstat(&rc[j].look,KST_WGT), ov=218-d;
        s32 pxi=(ex*ov*wj/(wi+wj)/d)<<8, pyi=(ey*ov*wj/(wi+wj)/d)<<8, pxj=(ex*ov*wi/(wi+wj)/d)<<8, pyj=(ey*ov*wi/(wi+wj)/d)<<8;
        rc[i].x=(rc[i].x-pxi)&((TW<<16)-1); rc[i].y=(rc[i].y-pyi)&((TW<<16)-1); rc[j].x=(rc[j].x+pxj)&((TW<<16)-1); rc[j].y=(rc[j].y+pyj)&((TW<<16)-1);
        if(i==0){ ppx=rc[0].x; ppy=rc[0].y; kv=kv*9/10; thump(0); } } }

// ---------------------------------------------------------------- every frame of a race
static void kart_results(void); static void kw_elim_tick(int n);
static void kart_tick(int n){
    if(kr_mode!=1) return;
    if(ks_state==KS_COUNT){ int was=k_count; k_count-=n;
        if((was>190 && k_count<=190) || (was>130 && k_count<=130) || (was>70 && k_count<=70)) sfx(7);
        if(k_count<=70 && k_count>40 && HIT(K_UP) && !k_rocket) k_rocket=1; else if(k_count>70 && HIT(K_UP)) k_rocket=-1;
        if(k_count<=10){ ks_state=KS_RACE; sfx(6); if(k_rocket==1){ kboost_t=45; show_msg("ROCKET START!"); } else if(k_rocket==-1){ kstall=50; show_msg("YOU STALLED! TOO EARLY"); } }
        return; }
    k_clock+=n; if(ks_state==KS_DONE) k_end_t+=n;
    rc[0].x=ppx; rc[0].y=ppy; rc[0].hd=(s16)khd; rc[0].v=kv; if(rc[0].spin) rc[0].spin = rc[0].spin>n ? rc[0].spin-n : 0; if(rc[0].cloud) rc[0].cloud = rc[0].cloud>n ? rc[0].cloud-n : 0;
    if(!kloop_on) kpath_track(0);
    if(nkl && !kloop_on && !kair){ int p=rc[0].pi;                                                // driving onto a loop-de-loop
        if(kloop_prev>=0) for(int j=0;j<nkl;j++){ int d0=(klp[j]-kloop_prev+knp)%knp, d1=(klp[j]-p+knp)%knp; if(d0>0 && d0<30 && (d1==0 || d1>knp-30)){ if(kroad(cmap[((ppy>>15)&HMASK)*HW+((ppx>>15)&HMASK)])) kloop_begin(j); break; } }
        kloop_prev=rc[0].pi; }
    if(!rc[0].fin){ int g=rc[0].nextg; s32 gx=((s32)kt.gx[g]<<15)+(1<<14), gy=((s32)kt.gy[g]<<15)+(1<<14), dx=wrapd(gx-ppx), dy=wrapd(gy-ppy);
        if((dx<0?-dx:dx)+(dy<0?-dy:dy) < 0x36000){ if(g==0){ rc[0].lap++; if(rc[0].lap>1){ int lt=k_clock-k_lap_t0; if(!k_best_lap || lt<k_best_lap) k_best_lap=lt; } k_lap_t0=k_clock;
            if(rc[0].lap>kt.laps) kfinish(0); else if(rc[0].lap>1){ if(rc[0].lap==kt.laps){ show_msg("FINAL LAP!"); sfx(6); music(7); } else { char b[24]; scpy(b,"LAP "); ncat(b,rc[0].lap); scat(b," OF "); ncat(b,kt.laps); show_msg(b); sfx(7); } } }
            rc[0].nextg=(u8)((g+1)%kt.n); }
        else { int gp=kgate_pi[g], ahead=(rc[0].pi-gp+knp)%knp; if(rc[0].lap>0 && ahead>8 && ahead<knp/3 && (frames&63)==0){ char b[32]; scpy(b,"MISSED A GATE! GO BACK"); show_msg(b); } }
        int dirn=kp_dir(rc[0].pi), off=((khd-dirn+512)&1023)-512; if(off<0) off=-off; if(off>380 && kv>K_VBASE/4) k_wrong+=n; else k_wrong=0; if(k_wrong>50 && (frames&31)==0) show_msg("WRONG WAY!"); }
    for(int r=1;r<nrc;r++) kai(r,n);
    kitems_tick(n); kbump(); kplaces(); kw_elim_tick(n);
    for(int r=1;r<nrc;r++) if(rc[r].po!=255){ po[rc[r].po].x=rc[r].x; po[rc[r].po].y=rc[r].y; }
    if(HIT(K_L)){ khorn(kl.part[KP_HORN]); for(int r=1;r<nrc;r++){ s32 dx=wrapd(rc[r].x-ppx), dy=wrapd(rc[r].y-ppy); if((dx<0?-dx:dx)+(dy<0?-dy:dy)<(4<<16)) rc[r].lane=-rc[r].lane; } }
    if(!rc[0].fin && k_finished>=nrc-1 && nrc>1){ if(k_end_t==0) show_msg("HURRY! THE OTHERS ARE DONE"); k_end_t+=n; if(k_end_t>900){ rc[0].place=(u8)nrc; kfinish(0); rc[0].place=(u8)nrc; ks_state=KS_DONE; k_end_t=150; } }
    if(ks_state==KS_DONE && k_end_t>=150) kart_results();
}

// ---------------------------------------------------------------- the karts on screen
static int kart_draw_me(int slot, int sy){
    if(kloop_on) return kloop_draw_me(slot);
    int sz=kl.part[KP_SIZE], inv = sz==0 ? 232 : sz==2 ? 160 : 190, view=0, flip=0, spin=rc[0].spin;
    inv = (int)((s32)inv*(cam_back()>>12)/(6<<4)); if(inv<100) inv=100;                       // a far camera sees a smaller kart
    int rel=((khd-heading+512)&1023)-512, arel=rel<0?-rel:rel;                                // the kart against the camera (it swings, or you look behind)
    if(arel>160){ if(arel>352){ view=0; flip=1; } else { view=1; flip = rel<0; } }
    if(spin){ int ph=(frames>>2)&3; view=ph&1; flip=ph>>1; }
    int lean = (kdrift ? kdrift*40 : kturn*18) + (arel<=160 ? rel/3 : 0), bob = (kv>K_VBASE/3 && ((frames>>2)&1) && !kair) ? 1 : 0;
    if(kair){ s32 dz=cam_back(), f=(FOCP<<16)/(dz>>8); int lift=(int)(((kalt-ground_h(ppx,ppy))*f)>>16); if(lift>0) sy-=lift; }   // up in the air (your shadow stays on the ground)
    if(kair && ktrick){ int a=(kair_t*20)&255; s32 c=cosx(a), sn=sinx(a); set_affine(29,(s16)((c*inv)>>14),(s16)((-sn*inv)>>14),(s16)((sn*inv)>>14),(s16)((c*inv)>>14)); }   // a trick: a full spin
    else set_affine(29, flip?-inv:inv, lean, 0, inv);
    int kh=16*256/inv, cy=sy-kh/2-bob;
    shadow_oam[slot].a0=((cy-16)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((120-32)&511)|(29<<9)|(2<<14); shadow_oam[slot].a2=(T_KDYN+view*8)|(7<<12); slot++;
    int b=kl.part[KP_BODY]; const u8 *an=kb_anchor+b*3;
    if(kl.part[KP_TOP] && slot<120){ int top=an[0], ty=sy-(16-top)*256/inv-4*256/inv-bob;
        shadow_oam[slot].a0=((ty-8)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((120-16)&511)|(29<<9); shadow_oam[slot].a2=(T_TOP1+(kl.part[KP_TOP]-1)*2); slot++; }
    int seat=kb_seat[b], feet=sy-seat*256/inv-bob;
    if(kw_mode && kw_driver_on()) slot=kw_draw_driver(slot,120,feet+2,150*inv/190,0);
    else if(form==FORM_HUMAN) slot=hum_oam(slot,120,feet+4,2,1,0,0,40,0,-1);
    else if(form==FORM_CREATURE){ int fl=form_body==2; set_affine(31,-200,0,0,200);
        shadow_oam[slot].a0=((feet-(fl?10:20))&255)|(1<<8)|(1<<9)|(fl?(1<<14):0); shadow_oam[slot].a1=((120-16)&511)|(31<<9)|(fl?0:(1<<14)); shadow_oam[slot].a2=(form_body==0?T_HOPPER:form_body==1?T_GRAZER:T_FLYER)|(6<<12); slot++; }
    else { int bi=400*inv/190; set_affine(31,bi,0,0,bi); int cy2=feet-31*256/bi; shadow_oam[slot].a0=((cy2-64)&255)|(1<<8)|(1<<9); shadow_oam[slot].a1=((120-64)&511)|(31<<9)|(3<<14); shadow_oam[slot].a2=T_BACK|BUG_BANK; slot++; }   // your bug from behind, its shell over the seat
    if(rc[0].cloud && slot<124){ shadow_oam[slot].a0=((sy-kh-36)&255)|(1<<14); shadow_oam[slot].a1=((120-16)&511)|(1<<14); shadow_oam[slot].a2=T_CLOUD; slot++; }
    return slot; }
static int kart_draw_obj(int slot, PObj *o, int sx, int sy, int m, s32 f){
    if(o->kind!=O_KART || slot>122) return slot;
    int r=o->var; if(r<1||r>=nrc) return slot; Racer *k=&rc[r];
    if(nkj){ int z=kjump_z(k->pi); if(z) sy-=(int)(((s32)z*f)>>16); }                       // flying over a jump
    int upside=0; if(k->la){ int a=(k->la>>5)&1023; s32 R=kloop_r(), lift=R-((R*cosx(a>>2))>>14), fw=(R*sinx(a>>2))>>14; int d=kp_dir(k->pi);   // round a loop: up, over and down
        s32 side=((sinx(d>>2)>>6)*hcs-(cosx(d>>2)>>6)*hsn)>>8, rside=((cosx(d>>2)>>6)*hcs+(sinx(d>>2)>>6)*hsn)>>8, lat=KLSH*(a-512)/512;
        sy-=(int)((lift*f)>>16); sx+=(int)((((fw*side+lat*rside)>>8)*f)>>16)*256/fov_k(); upside = a>256 && a<768; }
    int rel=(k->hd-heading)&1023; if(k->spin) rel=(rel+frames*48)&1023;
    int view, flip; if(rel<128||rel>=896){ view=0; flip=0; } else if(rel<384){ view=1; flip=0; } else if(rel<640){ view=0; flip=1; } else { view=1; flip=1; }
    int s=8+m*4, sz=k->look.part[KP_SIZE]; if(sz==0) s=s*85/100; if(sz==2) s=s*118/100; if(s<6) return slot;
    int inv=16384/s, mid=2*r-1; set_affine(mid, flip?-inv:inv, 0, 0, upside?-inv:inv);
    int kh=16*s/64, cy=sy-kh/2;
    shadow_oam[slot].a0=((cy-16)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((sx-32)&511)|(mid<<9)|(2<<14); shadow_oam[slot].a2=(T_KDYN+r*16+view*8)|((7+r)<<12); slot++;
    if(upside) return slot;                                                                 // (upside down at the top: just the kart)
    int b=k->look.part[KP_BODY]; const u8 *an=kb_anchor+b*3; int fs=flip?-1:1;
    if(k->look.part[KP_TOP]){ int top = view ? an[2] : an[0], tx = view ? sx+((an[1]+3-16)*s/64)*fs : sx, ty=sy-(16-top)*s/64-4*s/64;
        shadow_oam[slot].a0=((ty-8)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((tx-16)&511)|(mid<<9); shadow_oam[slot].a2=(T_TOP1+(k->look.part[KP_TOP]-1)*2); slot++; }
    int bg=k->being, feet=sy-kb_seat[b]*s/64, dx2 = view ? ((12-16)*s/64)*fs : 0, tl, shp=0, siz=1, w=16, h=16, pb=0;
    switch(bg){ case KB_BEETLE: tl=T_SBEETLE; break; case KB_SLUG: tl=T_SLUG; shp=1; siz=0; h=8; break; case KB_LOCAL1: tl=T_LOCAL1; break; case KB_LOCAL2: tl=T_LOCAL2; break;
        case KB_CRIT0: case KB_CRIT1: { Species *p=&spc[bg-KB_CRIT0]; pb=4+bg-KB_CRIT0; if(p->body==2){ tl=T_FLYER; shp=1; siz=0; h=8; } else tl = p->body==0 ? T_HOPPER : T_GRAZER; } break;
        case KB_TWIN: tl=T_BUGSIDE; shp=0; siz=2; w=32; h=32; pb=15; break; case KB_FOSSIL: tl=T_FOSSIL; break; case KB_TREE: tl=T_TREE; break; case KB_ROCK: tl=T_ROCK; break;
        case KB_FLOWER: tl=T_FLOWER; break; case KB_DROID: tl=T_BOLT; break; case KB_STALL: tl=T_STALL; break; case KB_WEB: tl=T_WEB; break; case KB_RUIN: tl=T_RUIN; break; default: tl=T_SHIP; }
    int dh=h*s/64; shadow_oam[slot].a0=((feet-dh/2-h)&255)|(1<<8)|(1<<9)|(shp<<14); shadow_oam[slot].a1=((sx+dx2-w)&511)|(mid<<9)|(siz<<14); shadow_oam[slot].a2=tl|(pb<<12); slot++;
    if(k->cloud && slot<124){ shadow_oam[slot].a0=((sy-kh-dh-14)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((sx-32)&511)|(mid<<9)|(1<<14); shadow_oam[slot].a2=T_CLOUD; slot++; }
    return slot; }

// ---------------------------------------------------------------- trails, sparks and the underglow (drawn into the picture)
typedef struct { s16 x, y; s8 vx, vy; u8 life, c; } KPart;
EW static KPart kpt[24]; static int kpt_i=0;
static void kpart_add(int x, int y, int vx, int vy, int life, int c){ KPart *p=&kpt[kpt_i]; kpt_i=(kpt_i+1)%24; p->x=(s16)(x<<4); p->y=(s16)(y<<4); p->vx=(s8)vx; p->vy=(s8)vy; p->life=(u8)life; p->c=(u8)c; }
static void kart_fx(volatile u16 *pg, int sy){
    int g=kl.part[KP_GLOW];
    if(g && !cam_fp){ if(g==7) BG_PAL[KRC_GLOW]=kglow_col[1+((frames>>3)%6)]; for(int y=-3;y<=3;y++){ int w=24-y*y*2; for(int x=-w;x<=w;x+=2){ int xx=120+x+(y&1), yy=sy+y; if(yy>18&&yy<160) pix(pg,xx,yy,KRC_GLOW); } } }
    int tr=kl.part[KP_TRAIL], mv = kv>K_VBASE/4;
    if(!cam_fp && mv && tr && (frames&1)==0){ static const u8 tc[10][2]={{0,0},{C_DGREY,C_GREY},{C_GOLD,C_WHITE},{C_CYAN,C_WHITE},{C_RED,C_RED},{C_RED,C_GOLD},{C_GREY,C_DGREY},{C_GOLD,C_WHITE},{C_RED,C_GOLD},{C_GREEN,C_CYAN}};
        int c = tr==5 ? (int)(C_RED+((frames>>1)%4==0?0:(frames>>1)%4==1?3:(frames>>1)%4==2?2:6)) : tr==9 ? (int)(C_RED+rndi(8)) : tc[tr][rndi(2)];
        for(int s=-1;s<=1;s+=2) kpart_add(120+s*14+rndi(3)-1, sy-2, s*(1+rndi(2)), 4+rndi(4), 14+rndi(8), c); }
    if(kdrift && (frames&1)==0){ int c = kcharge>110 ? C_GOLD : kcharge>50 ? C_CYAN : C_WHITE; for(int s=-1;s<=1;s+=2) kpart_add(120+s*13, sy-1, s*(2+rndi(3)), -(rndi(6)), 8, c); }
    if(kboost_t && (frames&1)==0) kpart_add(120+rndi(9)-4, sy-6, 0, 8, 10, (frames&2)?C_GOLD:C_RED);
    for(int i=0;i<24;i++){ KPart *p=&kpt[i]; if(!p->life) continue; p->life--; p->x+=p->vx; p->y+=p->vy; int x=p->x>>4, y=p->y>>4; if(x<1||x>238||y<19||y>158) { p->life=0; continue; }
        pix(pg,x,y,p->c); if(tr==3 || tr==4 || tr==7){ pix(pg,x+1,y,p->c); pix(pg,x,y+1,p->c); } } }

// ---------------------------------------------------------------- the race's own status: place, lap, time, item, minimap
static const char *const kicon[NKI]={0,
    "..rr.g..rrrrg...rrrrr...rrrrr...rrrrr...rrrr....rrr.....r.......",   // pepper
    "..gggg...gwgggg.gggggggggggggggggdggggdg.gddddg..gggg...........",   // shell
    "........gggg...ggwgggg.gggggggggggggggg.gggggg...gggg...........",   // slime
    "..cccc...c.ww.c.c.w...cc......cc......c.c....c...cccc...........",   // bubble
    "rr....wwrr....wwrr....rrrr....rr.rr..rr...rrrr.....rr...........",   // magnet
    "..wwww...w.ww.w.w....w.w.w..w.w....w..w.ww.w.....ww.....g.g.g...",   // sneeze
    ".r..r..rrrr.rrr.rrrrrrrrrrrrrrrr.rrr.rrr..r...r.................",   // 3 peppers
    "..dddd...dddddd.dddddddd.dddddd...c.c.c...c.c.c..c.c.c..........",   // cloud
};
static int kicon_c(char ch){ switch(ch){ case 'r': return C_RED; case 'g': return C_GREEN; case 'w': return C_WHITE; case 'c': return C_CYAN; case 'd': return C_GREY; default: return -1; } }
static void kart_minimap(volatile u16 *pg){
    int ox=194+(40-kmm_w)/2, oy=20+(40-kmm_h)/2;
    for(int i=0;i<(knp>>1) && i<KMM_N;i++) pix(pg,ox+kmmx[i],oy+kmmy[i],(i==0)?C_WHITE:C_GREY);
    if(kr_mode==1) for(int r=nrc-1;r>=0;r--){ int i=(rc[r].pi>>1); if(i>=KMM_N) i=KMM_N-1; int x=ox+kmmx[i], y=oy+kmmy[i], c = r==0 ? C_GOLD : (rc[r].place==1 ? C_RED : C_CYAN);
        if(r==0){ rect(pg,x-1,y-1,3,3,c); } else pix(pg,x,y,c); } }
static void kart_cockpit(volatile u16 *pg){                        // the cockpit view: a dashboard, a wheel that turns, your speed
    rect(pg,0,140,240,20,1); rect(pg,0,139,240,1,C_DGREY); for(int x=0;x<240;x+=2) pix(pg,x,141,6);
    int a=(kturn*24+(kdrift?kdrift*20:0)+256)&255; s32 c=cosx(a), sn=sinx(a);
    for(int k=0;k<48;k++){ int an=k*16/3; int x=120+((cosx(an)*15)>>14), y=153+((sinx(an)*9)>>14); if(y>141 && y<160){ pix(pg,x,y,C_GREY); pix(pg,x+1,y,C_DGREY); } }
    for(int t=-14;t<=14;t++){ int x=120+((c*t)>>14), y=153+((sn*t*9/15)>>14); if(y>141 && y<160) pix(pg,x,y,C_WHITE); }
    { char b[20]; int kmh=(int)((kv>0?kv:-kv)*600/65536); scpy(b,""); ncat(b,kmh); scat(b," KM/H"); text(pg,30,150,b,kboost_t?C_GOLD:C_CYAN,1); }
    rect(pg,170,149,50,4,C_DGREY); if(kart_boost>0) rect(pg,170,149,kart_boost/2,4,kart_boost>20?C_GOLD:C_RED); text(pg,170,143,"BOOST",C_DGREY,1); }
static void kart_hud(volatile u16 *pg){
    s32 dz=cam_back(); s32 f=(FOCP<<16)/(dz>>8); s32 gh=ground_h(ppx,ppy); s32 curve=curv(dz>>8)-tiny_sub(); int sy=PHZ+(((cam_h+curve-gh)*f)>>16);
    m7_sy=sy;
    if(kloop_on){} else if(cam_fp) kart_cockpit(pg); else if(!m7_on) kart_fx(pg,sy);     // (m7: the dust is drawn with the sky, m7_strip)
    if(FILM) return;                                                       // (filming: the kart's dust and sparks, but no HUD)
    char b[40];
    if(kr_mode==2){ scpy(b,"GATES "); ncat(b,kt.n); scat(b,"/"); ncat(b,KG_MAX); stext(pg,4,20,b,C_GOLD);
        stext(pg,4,28,"A: DROP A GATE",C_WHITE); stext(pg,4,35,"SEL: UNDO",C_GREY); stext(pg,4,42,"START: DONE",C_GREY);
        if(kt.n){ int mnx=999,mxx=-999,mny=999,mxy=-999, ux[KG_MAX+1], uy[KG_MAX+1]; ux[0]=kt.gx[0]; uy[0]=kt.gy[0];
            for(int i=1;i<=kt.n;i++){ int px = i<kt.n ? kt.gx[i] : (ppx>>15)&255, py = i<kt.n ? kt.gy[i] : (ppy>>15)&255; ux[i]=ux[i-1]+kwrap8(px-(i-1<kt.n?kt.gx[i-1]:0)); uy[i]=uy[i-1]+kwrap8(py-kt.gy[i-1]); }
            for(int i=0;i<=kt.n;i++){ if(ux[i]<mnx) mnx=ux[i]; if(ux[i]>mxx) mxx=ux[i]; if(uy[i]<mny) mny=uy[i]; if(uy[i]>mxy) mxy=uy[i]; }
            int span=mxx-mnx>mxy-mny?mxx-mnx:mxy-mny; if(span<20) span=20; rect(pg,192,19,44,44,1); frame1(pg,192,19,44,44,C_DGREY);
            for(int i=0;i<=kt.n;i++){ int x=194+(ux[i]-mnx)*40/span, y=21+(uy[i]-mny)*40/span; if(i==kt.n) rect(pg,x-1,y-1,3,3,C_GOLD); else rect(pg,x,y,2,2,i==0?C_WHITE:C_CYAN);
                if(i>0){ int x0=194+(ux[i-1]-mnx)*40/span, y0=21+(uy[i-1]-mny)*40/span; for(int t=1;t<8;t++) pix(pg,x0+(x-x0)*t/8,y0+(y-y0)*t/8,C_GREY); } } }
        rect(pg,56,58,50,3,C_DGREY); if(kart_boost>0) rect(pg,56,58,kart_boost/2,3,kart_boost>20?C_GOLD:C_RED); return; }
    if(nrc>1 && ks_state!=KS_COUNT){ const char *p=kplace_s(rc[0].place); text(pg,5,21,p,0,2); text(pg,4,20,p,rc[0].place==1?C_GOLD:C_WHITE,2); scpy(b,"/"); ncat(b,nrc); stext(pg,4+textlen(p)*8,26,b,C_GREY); }
    { int lp=rc[0].lap<1?1:rc[0].lap>kt.laps?kt.laps:rc[0].lap; scpy(b,"LAP "); ncat(b,lp); scat(b,"/"); ncat(b,kt.laps); stext(pg,4,33,b,C_CYAN); }
    { char t[12]; kfmt_time(t, rc[0].fin ? rc[0].ftime : k_clock); stext(pg,4,40,t,C_WHITE); }
    if(rc[0].item){ rect(pg,28,48,16,16,1); frame1(pg,28,48,16,16,(frames&16)?C_GOLD:C_WHITE); const char *ic=kicon[rc[0].item];
        for(int y=0;y<8;y++) for(int x=0;x<8;x++){ int c=kicon_c(ic[y*8+x]); if(c>=0) rect(pg,29+x*2-(x==7),49+y*2-(y==7),2,2,c); }
        if(rc[0].item_n>1){ scpy(b,"X"); ncat(b,rc[0].item_n); stext(pg,46,57,b,C_GOLD); } }
    rect(pg,192,19,44,44,1); frame1(pg,192,19,44,44,C_DGREY); kart_minimap(pg);
    rect(pg,56,58,50,3,C_DGREY); if(kart_boost>0) rect(pg,56,58,kart_boost/2,3,kart_boost>20?C_GOLD:C_RED);
    if(kdrift){ rect(pg,110,58,40,3,C_DGREY); rect(pg,110,58,kcharge>110?40:kcharge*40/110,3,kcharge>110?C_GOLD:kcharge>50?C_CYAN:C_WHITE); }
    if(ks_state==KS_COUNT){ const char *c = k_count>190 ? "3" : k_count>130 ? "2" : k_count>70 ? "1" : "GO!"; int L=textlen(c); text(pg,120-L*6+1,25,c,0,3); text(pg,120-L*6,24,c,k_count>70?C_WHITE:C_GREEN,3);
        if(k_count>190) ctext(pg,120,42,"HOLD UP TO GO. UP RIGHT AT 1 = ROCKET START",C_GREY);
        else if(nkl && k_count>70) ctext(pg,120,42,nkl>1?"THIS TRACK HAS LOOP-DE-LOOPS! GO FAST":"THIS TRACK HAS A LOOP-DE-LOOP! GO FAST",C_GOLD); }
    else if(ks_state==KS_RACE && k_clock<50){ text(pg,120-18+1,25,"GO!",0,3); text(pg,120-18,24,"GO!",C_GREEN,3); }
#ifdef TEST_KDBG
    { char d[48]; int ti=((ppy>>15)&HMASK)*HW+((ppx>>15)&HMASK); scpy(d,"C "); ncat(d,cmap[ti]); scat(d," H "); ncat(d,hfine[ti]); scat(d," PI "); ncat(d,rc[0].pi); scat(d," V "); ncat(d,kv); scat(d," P96 "); ncat(d,BG_PAL[96]); scat(d," AIR "); ncat(d,kair); stext(pg,4,120,d,C_WHITE);
      scpy(d,"X "); ncat(d,ppx>>15); scat(d," Y "); ncat(d,ppy>>15); scat(d," J "); ncat(d,nkj?kjl[0]:-1); scat(d,"-"); ncat(d,nkj?kjr[0]:-1); scat(d," Z "); ncat(d,kalt); scat(d," VZ "); ncat(d,kvz4); scat(d," WL "); ncat(d,water_lv); stext(pg,4,128,d,C_WHITE); }
#endif
    if(!opt_hints) return;
    if(ks_state==KS_RACE && k_clock<400) stext(pg,32,42,"UP: GO  B: BOOST  R: DRIFT  A: ITEM  L: HORN",C_DGREY);
}

static void kart_reset(void){ kr_mode=0; kart_on=0; kt_painted=0; nkslot=0; nkhid=0; nrc=1; ks_state=0; kloop_on=0; nkl=0; }   // a new planet: its land is fresh, nothing is borrowed
static int kart_glow(void){ return kl.part[KP_GLOW]; }
