// ================================================================= THE PLANET: a rolling heightmap world, drawn column by column (voxel style)
// The ground is a 128x128 map of heights that wraps around, so walking keeps going round the world. Every frame each screen
// column marches away from the camera, samples the map, and draws the terrain as stacked spans. Far ground drops away with
// distance, which bends the horizon into the edge of a small planet. Things on the surface are sprites placed on that terrain.
#define TW 128
#define TMASK (TW-1)
#define HW 256                    // the rendered map is the world map smoothed up to 256x256 (two texels per cell)
#define HMASK (HW-1)
#define FOCP 120                  // focal length in pixels
#define CAM_BACK (6<<16)          // the camera sits 6 cells behind the bug
#define CAM_UP   (4<<8)           // ...and 4 cells above the ground under it
#define NSTEP 48
#define NEAR_STEPS 24             // smooth (bilinear) sampling for the first steps, nearest beyond
#define SKY_B 8                   // palette: sky 8..15, ground 40..55, water 56..63
#define GND_B 40
#define WAT_B 56
#define BLD_B 64                  // 64..87: city walls, windows, roads, roofs, sand, snow (set up by planet_palette)
enum { O_DOOR, O_SHIP, O_CAGE, O_SLUG, O_PLANT, O_TREE, O_ROCK, O_WEB, O_FLOWER, O_BASE, O_MARKET, O_CANTINA, O_OFFICE, O_RACE, O_HOME, O_LOCAL, O_FOSSIL, O_RUIN, O_BEETLE, O_CRIT, O_SHRINE, O_BEACON, O_STALL, O_PEBBLE, O_TUFT, O_KART, O_IBOX, O_KSHELL, O_KSLIME };   // the last four: kart racing
#define O_LAST_USE O_SHRINE
static void city_flatten(void); static void city_stamp(void); static int good_price(int g); static void species_palette(void);
static int has_base(int i){ return i>=0 && i<NSYS && !((base_mask>>(i&31))&1) && ((sys[i].kind>=1 && sys[i].kind<=5) || (sys[i].kind==6 && !is_hive(i) && (sys_id(i)%3)==0)); }                                // kinds up to this one react to A
typedef struct { s32 x,y; u8 kind, alive, hp, var; } PObj;      // x,y: where on the map, 16.16 cells. hp: bites left in a resource node
#define NPO 160
static const u8 node_good[4]={3,4,2,1};                    // tree -> AMBER, rock -> CRYSTALS, web -> SILK, flower -> NECTAR (the station's goods)
static const char *const node_name[4]={"AMBER TREE","CRYSTAL ROCK","SILK WEB","NECTAR FLOWER"};
EW static PObj po[NPO];
EW static u8 hmap[TW*TW];                                  // 0..255: height of the ground, one per cell
EW static u8 hfine[HW*HW];                                 // the same, smoothed, two texels per cell: what the renderer walks
EW static u8 cmap[HW*HW] __attribute__((aligned(4)));                                  // the colour of each texel, lit by a sun from the north-west
static s32 ppx, ppy;                                       // where the bug stands, 16.16 cells
static s32 cam_h;                                          // height of the camera above sea level, 8.8 cells
static int g_phz=66;                                        // the eye line (horizon) row: a high kart camera lifts it, which tips the view down
#define PHZ g_phz
static int water_lv=70;
static int facing=1, msg_timer=0, prompt_k=-2;
#define cur_tint zone
static char msg[40];
static int enter_dungeon_flag=0;
IWRAM_DATA static s16 raydx[120];                          // per column: sideways cells per cell of depth, 8.8 (before turning)
static int cam_fp=0, heading=0;                            // first person? and which way you face (0..1023, 0 = north = up the map)
static void ship_shadow(volatile u16 *pg, int cx, int cy, int sc);
static u8 kcam[8]={0,1,1,1,1,0,1,1};                       // the kart camera (kart.inc): view, distance, height, field of view, swing, look into turns, speed zoom, bump shake
static int kfovk=180;                                      // the kart camera's field of view: sideways cells per cell of depth at the screen's edge (8.8)
static s32 kcam_back(void); static s32 kcam_up(void); static s32 kcam_base(void);
static int kloop_on=0, kloop_hide=0; static s32 kbr_cur=0, kloop_cb=0;  // (kbr_cur: how far up a flyover you are, 8.8)                        // riding a loop-de-loop (kart.inc): the camera swings out to watch from the side
static s32 cam_back(void){ if(m7_snap) return m7_cb_snap; if(kloop_on) return kloop_cb; if(kr_mode && !cam_fp) return kcam_back(); return cam_fp ? (1<<16)/2 : CAM_BACK; }
static s32 cam_up(void){ if(kr_mode && !cam_fp) return kcam_up(); return cam_fp ? (5<<8)/4 : CAM_UP; }
static int kfov_add=0;                                     // the kart camera widens as you speed up and boost (kart.inc): the world rushes past
static int m7_fovs=0;                                              // the field of view the frame on show was drawn with
static int fov_k(void){ if(m7_snap && m7_fovs) return m7_fovs; return kr_mode ? kfovk+(kloop_on?0:kfov_add) : 180; }
IWRAM_DATA static s16 rayvx[120], rayvy[120];              // per column: the ray direction on the map, 8.8 per cell of depth
static int hcs=256, hsn=0;                                 // cos and sin of the heading, 8.8
static void heading_rays(void){
    hcs=cosx(heading>>2)>>6; hsn=sinx(heading>>2)>>6;
    for(int c=0;c<120;c++){ rayvx[c]=(s16)(hsn + ((hcs*raydx[c])>>8)); rayvy[c]=(s16)(hcs - ((hsn*raydx[c])>>8)); }
}
#define WP_WIN 6                 // a bite is TIMED if you hit again within this many frames of the last swing ending (the bug is upright again): it takes two bites off
static u8 mine_gap=99;           // frames since the last swing ended (99: a while ago)
static int pw_h=0, pw_v=0, pw_step=0, mine_t=0;
IWRAM_DATA static s8 wob64[64];                            // WOBBLE: how far each spot of the hills is breathing this frame
static int g_nst=NSTEP, g_wob=0, g_fog=0;                  // FOG: how far the view reaches; WOBBLE on or off
static u16 gbase[16], skybase[8];                          // the ground and sky colours before the animated traits touch them
// how fast the horizon falls away: the planet's size, and TINY PLANET
static int curv_k(void){ static const u8 ks[6]={12,7,4,3,2,1}; int k=ks[wd.psz%6]; if(WL(MW_TINY)){ int t=8<<(WL(MW_TINY)/7); if(t>k) k=t; }
    return k; }
static int curv_sh(void){ return kr_mode==1 && !kloop_on ? 5 : 3; }   // racing: the world bends away a quarter as fast, so the road ahead stays in view
static s32 curv(s32 z){ return (((z*z)>>14)*curv_k())>>curv_sh(); }                          // how far the ground has dropped out of sight at distance z (8.8 cells); 4 is the old z*z/32768
static s32 tiny_sub(void){ return curv_k()>4 ? curv(cam_back()>>8) : 0; }
static void pknock(s32 dx, s32 dy, int k){                    // push the bug away along (dx,dy) (map direction, bug minus enemy), in walk-input terms
    s32 side=((dx>>8)*hcs - (dy>>8)*hsn), fwd=((dx>>8)*hsn + (dy>>8)*hcs);
    pw_h = side>0 ? k : side<0 ? -k : 0; pw_v = fwd>0 ? -k*3/5 : fwd<0 ? k*3/5 : 0; }
IWRAM_DATA static s32 stp_f[NSTEP];                        // per march step: pixels per cell at that distance, 8.8
IWRAM_DATA static s32 stp_base[NSTEP];                     // per step: (camera height + curvature) * stp_f, 8.8 px above the eye line... as a y offset
IWRAM_DATA static s32 stp_z[NSTEP];                        // per step: distance from the camera, 8.8 cells
IWRAM_DATA static u8 ybin[120*8];                          // per column: the terrain's top row after each eighth of the march (for hiding sprites behind hills)
IWRAM_DATA static u8 skyrow[160];
IWRAM_DATA static s8 gtex[256];                            // a little grass/soil texture, +-1 shade
static const u8 bay4[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};

static u16 lerp15(u16 a, u16 b, int t){ int r=((a&31)*(16-t)+(b&31)*t)>>4, g=(((a>>5)&31)*(16-t)+((b>>5)&31)*t)>>4, bl=(((a>>10)&31)*(16-t)+((b>>10)&31)*t)>>4; return r|(g<<5)|(bl<<10); }
static void hell_palette(void){                                   // the six hell worlds: their own ground ramp, sea, sky and glow (set after the dusk tint so the glow stays bright)
    static const u8 T[6][21]={ {24,10,10, 150,60,30, 255,150,30, 170,30,0, 255,200,40, 30,0,0, 230,80,20},
        {40,36,6, 190,170,40, 250,240,80, 40,120,10, 170,250,40, 30,34,0, 180,170,40},
        {6,4,14, 70,60,110, 255,60,200, 0,0,10, 60,40,120, 2,0,10, 60,10,90},
        {60,52,44, 240,230,205, 255,255,240, 80,70,70, 200,190,190, 20,16,26, 130,100,120},
        {30,4,10, 170,60,70, 255,90,90, 70,0,10, 200,20,40, 6,0,6, 150,10,40},
        {10,16,40, 190,225,255, 140,255,255, 140,190,230, 230,250,255, 4,6,24, 40,70,150} };
    const u8 *t=T[wd.cls-PC_INFERNO]; u16 g0=rgb15(t[0],t[1],t[2]), g1=rgb15(t[3],t[4],t[5]), gl=rgb15(t[6],t[7],t[8]), s0=rgb15(t[9],t[10],t[11]), s1=rgb15(t[12],t[13],t[14]), k0=rgb15(t[15],t[16],t[17]), k1=rgb15(t[18],t[19],t[20]);
    for(int i=0;i<14;i++) BG_PAL[GND_B+i]=lerp15(g0,g1,i*16/13);
    BG_PAL[GND_B+14]=gl; BG_PAL[GND_B+15]=wd.cls==PC_OBSIDIAN ? rgb15(60,255,230) : lift15(gl,8);              // 14,15: veins, embers, crusts
    for(int i=0;i<8;i++){ BG_PAL[WAT_B+i]=lerp15(s0,s1,i*2); BG_PAL[SKY_B+i]=lerp15(k0,k1,i*2); }
    BG_PAL[88]=lerp15(k1,gl,8); BG_PAL[89]=gl; BG_PAL[94]=lift15(gl,10);                                      // the great sun or moon
    if(wd.cls==PC_OBSIDIAN){ BG_PAL[88]=rgb15(255,60,200); BG_PAL[89]=rgb15(8,0,16); BG_PAL[94]=0; }           // a black sun with a neon rim
    if(wd.cls==PC_BONE){ BG_PAL[88]=rgb15(150,140,130); BG_PAL[89]=rgb15(225,215,195); BG_PAL[94]=rgb15(40,30,40); }
}
static void planet_palette(void){
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    const u16 *tp = endless ? w_tint : &tint_pal[zone*6];
    for(int i=0;i<6;i++) BG_PAL[16+i]=tp[i];
    for(int i=0;i<16;i++){ int k=i*4; int a=k>>4, t=k&15; if(a>3){ a=3; t=16; } BG_PAL[GND_B+i]=lerp15(tp[a],tp[a+1],t); }    // 16 ground shades, dark to light
    for(int i=0;i<8;i++) BG_PAL[WAT_B+i]=bg_pal[R_WATER+i];
    if(wd.cls==PC_DEATH) for(int i=0;i<8;i++) BG_PAL[WAT_B+i]=rgb15(120+i*18,20+i*20,0);                       // lava seas
    if(wd.cls==PC_ICE) for(int i=0;i<8;i++) BG_PAL[WAT_B+i]=rgb15(150+i*12,190+i*8,230+i*3);                  // frozen seas
    if(wd.cls==PC_OCEAN || wd.cls==PC_PARADISE) for(int i=0;i<8;i++) BG_PAL[WAT_B+i]=rgb15(10+i*14,60+i*22,110+i*18);   // deep, clear water
    u16 sky0=rgb15(4,4,14), sky1 = endless ? wd.sky : tp[5];
    if(wd.hour==1){ sky0=rgb15(40,10,30); sky1=rgb15(240,120,60); }                                // dusk
    if(wd.hour==2){ sky0=rgb15(2,2,8); sky1=rgb15(20,24,60); }                                     // night
    if(wd.weather==3){ sky0=rgb15(60,30,10); sky1=rgb15(200,140,70); }                              // dust
    if(wd.cls==PC_DEATH){ sky0=rgb15(10,0,0); sky1=rgb15(110,20,10); }                              // a death planet's sky
    for(int i=0;i<8;i++) BG_PAL[SKY_B+i]=lerp15(sky0,sky1,i*2);
    if(wd.hour) for(int i=0;i<24;i++) BG_PAL[GND_B+i] = wd.hour==2 ? dim15(BG_PAL[GND_B+i],8) : lerp15(BG_PAL[GND_B+i],rgb15(255,140,60),4);
    if(hellw()) hell_palette();
    // the city: four wall tints, windows, roads, roofs, plazas, sand and snow
    for(int t=0;t<4;t++){ u16 w = t==0 ? wd.bld : t==1 ? lift15(wd.bld,5) : t==2 ? shade15(wd.bld,10) : lerp15(wd.bld,wd.sky,6);
        BG_PAL[BLD_B+t*2]=w; BG_PAL[BLD_B+t*2+1]=shade15(w,13); BG_PAL[76+t]=lift15(w,4); }
    BG_PAL[72] = wd.hour==2 ? rgb15(255,230,120) : rgb15(190,220,240); BG_PAL[73]= wd.hour==2 ? rgb15(60,50,20) : rgb15(90,110,140);
    BG_PAL[74]=wd.road; BG_PAL[75]=lift15(wd.road,8); BG_PAL[80]=lift15(wd.road,5); BG_PAL[81]=lift15(wd.road,3);
    BG_PAL[82]=rgb15(220,200,140); BG_PAL[83]=rgb15(236,220,160); BG_PAL[84]=rgb15(250,250,255); BG_PAL[85]=rgb15(210,220,240); BG_PAL[86]=rgb15(236,240,250); BG_PAL[87]=rgb15(200,210,235);
    if(wd.hour==2) for(int i=64;i<88;i++) BG_PAL[i]=dim15(BG_PAL[i],9);
    if(WL(MW_METAL)) for(int i=0;i<16;i++) BG_PAL[GND_B+i] = i<8 ? shade15(wcol[MW_METAL],4+i) : lift15(wcol[MW_METAL],(i-8)*2);   // METAL GROUND
    if(WL(MW_VOID)){ for(int i=0;i<16;i++) BG_PAL[GND_B+i]= (i&3)==3 ? wcol[MW_VOID] : rgb15(10+i,0,20+i*3); for(int i=0;i<8;i++) BG_PAL[SKY_B+i]=0; }   // VOID: neon in the dark
    if(WL(MW_MIRROR)) for(int i=0;i<8;i++) BG_PAL[SKY_B+i]=BG_PAL[GND_B+15-i*2];                                   // MIRROR SKY: the sky is the ground
    if(WL(MW_HEAT)) for(int i=0;i<8;i++) BG_PAL[SKY_B+i]=lerp15(BG_PAL[SKY_B+i],rgb15(255,60,20),WL(MW_HEAT)/2);
    if(WL(MW_FROST)) for(int i=0;i<8;i++) BG_PAL[SKY_B+i]=lerp15(BG_PAL[SKY_B+i],rgb15(210,235,255),WL(MW_FROST)/2);
    BG_PAL[88]=wcol[MW_SUNS]; BG_PAL[89]=lift15(wcol[MW_SUNS],10); BG_PAL[90]=wcol[MW_RINGS]; BG_PAL[91]=lift15(wcol[MW_MOONS],6); BG_PAL[92]=shade15(wcol[MW_MOONS],9); BG_PAL[93]=lift15(wcol[MW_RINGS],8); BG_PAL[94]=rgb15(255,240,200);
    if(WL(MW_NEGATIVE)) for(int i=8;i<96;i++) if(i<30||i>=40) BG_PAL[i]=lerp15(BG_PAL[i],BG_PAL[i]^0x7FFF,6+WL(MW_NEGATIVE)/3);       // NEGATIVE: colours turn inside out
    if(WL(MW_SEPIA)) for(int i=8;i<96;i++){ if(i>=30&&i<40) continue; u16 c=BG_PAL[i]; int gy=((c&31)+((c>>5)&31)+((c>>10)&31))*8/3;        // OLD PHOTO: brown and faded
        BG_PAL[i]=lerp15(c,rgb15(gy*11/10,gy,gy*7/10),6+WL(MW_SEPIA)/3); }
    for(int i=0;i<16;i++) gbase[i]=BG_PAL[GND_B+i]; for(int i=0;i<8;i++) skybase[i]=BG_PAL[SKY_B+i];
    obj_palette(); species_palette();
}
static void planet_draw_bg(void){                           // the title screen still uses the painted planet picture
    volatile u16 *pg=VRAM; const u16 *src=(const u16*)planet_img;
    for(int i=0;i<240*160/2;i++) pg[i]=src[i];
}

// ---------------------------------------------------------------- making a world
static int hm_at(s32 x, s32 y){ return hmap[((y>>16)&TMASK)*TW + ((x>>16)&TMASK)]; }
static int hm_bil(s32 x, s32 y){                            // height between cells, 8.8 (0..255<<8 range as 0..255 * 256)
    int x0=(x>>16)&TMASK, y0=(y>>16)&TMASK, x1=(x0+1)&TMASK, y1=(y0+1)&TMASK, fx=(x>>8)&255, fy=(y>>8)&255;
    int a=hmap[y0*TW+x0], b=hmap[y0*TW+x1], c=hmap[y1*TW+x0], d=hmap[y1*TW+x1];
    int top=a*(256-fx)+b*fx, bot=c*(256-fx)+d*fx;
    return (top*(256-fy)+bot*fy)>>8;                          // 0..255<<8
}
static int g_hs=3;                                          // how tall the land is: 255 -> g_hs cells (mountain worlds are tall, meadows flat)
static s32 ground_h(s32 x, s32 y){ return hfine[((y>>15)&HMASK)*HW + ((x>>15)&HMASK)]*g_hs; }
static inline __attribute__((always_inline)) int hell_paint(int c, int h, int x, int y){            // the hell worlds' own ground patterns: lava veins, crusts, neon cracks, ribs, frost cracks
    int d=bay4[((y&3)<<2)|(x&3)];
    switch(wd.cls){
        case PC_INFERNO:   if(((h+(d>>2))%17)<2 && h>water_lv+3) c=GND_B+14+((x+y)&1); break;
        case PC_BRIMSTONE: if(((x*7+y*13+h*3)&15)==0) c=GND_B+14; else if(((h+(d>>2))%23)<1) c=GND_B+2; break;
        case PC_OBSIDIAN:  if((((x+(h>>3))%24)==0) || (((y+(h>>3))%24)==0)) c=GND_B+14+(((x>>3)^(y>>3))&1); break;
        case PC_BONE:      if((((y+(h>>2))/3)&1) && c>=GND_B+4) c-=4; break;
        case PC_BLOODSEA:  if(((h+(d>>2))%21)<1 && h>water_lv+3) c=GND_B+14; break;
        case PC_COCYTUS:   if(((x*5+y*3+(h>>2))%31)==0) c=GND_B+1; else if(((x*11+y*7+h)&63)==0) c=GND_B+14; break;
    }
    return c; }
// ---- the three big loops of making a world run from fast internal memory (they touch 65,536 points each)
static void land_octave(s16 *acc, const u8 *grid, const u16 *sm, int sh, int n, int am){
    int nm=n-1; u16 T[TW], B[TW], S[TW]; int band=-1;                                    // T/B: the grid rows above and below, already blended across x
    for(int x=0;x<TW;x++) S[x]=sm[x];
    for(int y=0;y<TW;y++){ int gy=y>>sh, fy=S[y];
        if(gy!=band){ band=gy; const u8 *g0=grid+gy*n, *g1=grid+((gy+1)&nm)*n;
            for(int x=0;x<TW;x++){ int gx=x>>sh, gx1=(gx+1)&nm, fx=S[x]; T[x]=(u16)(g0[gx]*(256-fx)+g0[gx1]*fx); B[x]=(u16)(g1[gx]*(256-fx)+g1[gx1]*fx); } }
        s16 *ac=acc+y*TW; int wy=256-fy;
        for(int x=0;x<TW;x++){ int v=(T[x]*wy+B[x]*fy)>>16; ac[x]+=((v-128)*am)>>7; } }
}
static void land_smooth(void){                         // the map smoothed up to two texels per cell (texel centres at 1/4 and 3/4 of a cell)
    for(int y0=0;y0<TW;y0++){ const u8 *r0=hmap+y0*TW, *r1=hmap+((y0+1)&TMASK)*TW; u16 *o0=(u16*)(hfine+(y0*2)*HW), *o1=(u16*)(hfine+(y0*2+1)*HW);
        int a=r0[0], c=r1[0];
        for(int x0=0;x0<TW;x0++){ int x1=(x0+1)&TMASK, b=r0[x1], d=r1[x1];          // one cell -> its four texels, written two at a time
            int t0=a*3+b, t1=a+b*3, b0=c*3+d, b1=c+d*3;                                 // across x: 3/4 and 1/4 weights
            o0[x0]=(u16)(((t0*3+b0)>>4) | (((t1*3+b1)>>4)<<8));
            o1[x0]=(u16)(((t0+b0*3)>>4) | (((t1+b1*3)>>4)<<8));
            a=b; c=d; } }
}
static void land_paint(const s8 *htab, int rst, int rch, int sl, int hw_){  // the colour of every texel: shade, sun, beaches, snow, stripes, hell patterns
    u8 b4[16]; for(int i=0;i<16;i++) b4[i]=bay4[i];
    s8 ht[256]; for(int i=0;i<256;i++) ht[i]=htab[i];                               // the tables live on the fast stack
    int wl=water_lv, two=wd.twotone, sand=wd.sand, snow=wd.snow;
    for(int y=0;y<HW;y++){ const u8 *row=hfine+y*HW, *rowu=hfine+((y-1)&HMASK)*HW; u8 *cm=cmap+y*HW; int prev=row[HW-1];
        for(int x=0;x<HW;x++){ int h=row[x], hl=prev; prev=h;
            if(h<wl){ cm[x]=(u8)ht[h]; continue; }
            int slope = (h-hl) + (h-rowu[x]);                                         // facing the sun (north-west) is brighter
            int s = ht[h] + ((slope*85)>>8) + ((b4[((y&3)<<2)|(x&3)] + ((x*7+y*13)&7)) >> 3);
            if(h<wl+10 && ((x+y)&1)) s-=1;                                         // a dark shoreline
            if(two && ((hfine[((y+3)&HMASK)*HW+((x+5)&HMASK)]>>4)&1)) s+=2;         // patches of a second shade
            if(s<0) s=0; if(s>15) s=15; if(hw_ && s>13) s=13; int c=GND_B+s;
            if(sand && h<wl+8) c=82+((x+y)&1);                                     // a sandy beach
            if(snow && h>sl){ int q=(h-sl)>>5; if(q>1) q=1; c=84+q*2+((b4[((y&3)<<2)|(x&3)]>(h&15))?0:1); }   // snow caps
            if(c>=GND_B && c<GND_B+16){
                if(rst && ((((x+y)*rst)>>16)&1)){ c+=3; if(c>GND_B+15) c=GND_B+15; }           // STRIPES
                if(rch && ((((x*rch)>>16)^((y*rch)>>16))&1)){ c-=4; if(c<GND_B) c=GND_B; }     // CHECKERS
                if(hw_) c=hell_paint(c,h,x,y); }
            cm[x]=(u8)c; } }
}
#ifdef TEST_GEN
static u32 gen_cyc, oct_cyc, col_cyc, city_cyc, nrm_cyc, smo_cyc;
#endif
static void land_colour(void){                                    // the colours of the land, the town on top, the glitches: after smoothing, and again when a race track is taken away
    EW static s8 htab[256];                                                           // per height: the water shade below the sea, the ground shade above it
    for(int h=0;h<256;h++){ if(h<water_lv){ int w=(h*6)/(water_lv+1); htab[h]=(s8)(WAT_B+1+(w>5?5:w)); } else htab[h]=(s8)(5+(h-water_lv)*6/(255-water_lv+1)); }
    int rst = WL(MW_STRIPES) ? 65536/(2+WL(MW_STRIPES)/2) : 0, rch = WL(MW_CHECKER) ? 65536/(2+WL(MW_CHECKER)/2) : 0;
    { int sl = WL(MW_FROST) ? wd.snowl-WL(MW_FROST)*5 : wd.snowl; if(sl<water_lv+4) sl=water_lv+4; land_paint(htab,rst,rch,sl,hellw()); }
    city_stamp();
    if(WL(MW_GLITCH)){ u32 r=wd.tseed; for(int b=0;b<200+WL(MW_GLITCH)*60;b++){ r=hmix(r+b); int x=r&HMASK, y=(r>>8)&HMASK, w=2+((r>>16)&7), h=1+((r>>20)&3), c=40+((r>>24)%48);   // GLITCH: torn blocks of wrong colour
        for(int j=0;j<h;j++) for(int i=0;i<w;i++) cmap[((y+j)&HMASK)*HW+((x+i)&HMASK)]=c; } }
}
EW static s16 gen_acc[TW*TW] __attribute__((aligned(4)));                            // 32 KB of scratch for building a world (and, between worlds, for the kart races' tables: m7.inc)
static void gen_terrain(u32 seed){
    wseed=seed;
    s16 *acc=gen_acc; for(int i=0;i<TW*TW;i++) acc[i]=0;
    static const u8 cell[4]={32,16,8,4}; u8 amp[4]={110,70,40,18};
    if(wd.cls==PC_MEADOW){ amp[2]=10; amp[3]=2; } if(wd.cls==PC_MOUNTAIN){ amp[1]=95; amp[2]=60; amp[3]=26; } if(wd.cls==PC_ECUMENOPOLIS){ amp[0]=40; amp[1]=20; amp[2]=4; amp[3]=0; }
    EW static u8 grid[33*33];
#ifdef TEST_GEN
    u32 tq=TMR();
#endif
    for(int o=0;o<4;o++){ int cs=cell[o], n=TW/cs;
        int full = endless && sys[cur_sys].kind==6 && !home_cluster();                 // out in the universe: every word of the address shapes the land
        for(int i=0;i<n*n;i++) grid[i] = full ? (hmix(aword(i+o*11,cur_sys|((cur_pl*8+cur_moon)<<8)) ^ seed ^ ((u32)o<<24) ^ (u32)i*0x9E3779B1u)>>12)&255 : wr(256);
        int sh=5-o, nm=n-1, am=amp[o];                                                  // cs is a power of two: shifts, not divisions (the GBA has no divide)
        EW static u16 sm[TW]; for(int x=0;x<TW;x++){ int f=((x&(cs-1))<<8)>>sh; sm[x]=(u16)((f*f*(3*256-2*f))>>16); }   // smoothstep, once per octave
        land_octave(acc,grid,sm,sh,n,am); (void)nm; }
#ifdef TEST_GEN
    oct_cyc=TMR()-tq; tq=TMR();
#endif
    int lo=32767, hi=-32768; for(int i=0;i<TW*TW;i++){ if(acc[i]<lo) lo=acc[i]; if(acc[i]>hi) hi=acc[i]; }
    s32 rk=(255<<16)/(hi-lo+1);
    for(int i=0;i<TW*TW;i++){ int v=((acc[i]-lo)*rk)>>16;                                         // the shape of the land, from the DNA
        switch(wd.style){
            case 0: v=(v*v)>>8; break;                                                               // rolling hills: wide valleys, round tops
            case 1: { int q=v>>5, f=v&31; v=q*32+(f<10?f*3:30); } break;                             // mesas: flat steps with short ramps
            case 2: v=v>128?255-v:v; v=(v*v)>>7; break;                                              // dunes: soft ridges
            case 3: v=(v*v*v)>>16; break;                                                             // islands: sharp peaks out of the sea
            case 4: v=(v*v)>>8; break;                                                               // craters: hills, then bowls (below)
            case 5: v = v<110 ? v*2/3 : 73+(v-110)*5/4; v=v>255?255:v; break;                        // canyons: a plateau cut by low valleys
            case 6: v=v>128?255-v:v; v=255-(v*2); v=(v*v)>>8; break;                                 // ridges: sharp crests
            default: v=128+(v-128)/3; break; }                                                       // plains: gentle
        hmap[i]=v; }
    if(wd.style==4) for(int c=0;c<6+wd.rough*2;c++){ int cx=wr(TW), cy=wr(TW), r=5+wr(9);          // craters: a bowl with a raised rim
        for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++){ int d2=x*x+y*y; if(d2>r*r) continue; int i=((cy+y)&TMASK)*TW+((cx+x)&TMASK);
            int t=256-d2*256/(r*r); int v=hmap[i]-(70*t>>8)+(d2>r*r*2/3?25:0); hmap[i]=v<0?0:v>255?255:v; } }
    if(wd.cls==PC_INFERNO) for(int v=0;v<3;v++){ int cx=wr(TW), cy=wr(TW), r=10+wr(7);                                // VOLCANOES: a tall cone with a lava lake in its top
        for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++){ int d2=x*x+y*y; if(d2>r*r) continue; int i=((cy+y)&TMASK)*TW+((cx+x)&TMASK); int dist=isqrt((u32)d2), cone=255-dist*205/r;
            if(dist<=r/5) hmap[i]=(u8)(water_lv>10 ? water_lv-8 : 0); else if(cone>hmap[i]) hmap[i]=(u8)cone; } }
    if(WL(MW_SPIKES) || wd.cls==PC_OBSIDIAN){ int th=175-(WL(MW_SPIKES)?WL(MW_SPIKES):10)*2; for(int i=0;i<TW*TW;i++){ int v=hmap[i]; hmap[i] = v>th ? 255 : v>th-30 ? (v-th+30)*8+15 : v/8; } }   // SPIKES: needles
    if(WL(MW_TERRACES)){ int st=6+WL(MW_TERRACES)*2; for(int i=0;i<TW*TW;i++) hmap[i]=(hmap[i]/st)*st; }                                      // TERRACES: giant stairs
    if(wd.cliffs) for(int i=0;i<TW*TW;i++){ int v=hmap[i]; if(v>120 && v<150) v = v<135 ? 120 : 150; hmap[i]=v; }   // a cliff line through the middle heights
    city_flatten();
    // a flat landing field at the start
    for(int y=-5;y<=5;y++) for(int x=-6;x<=6;x++){ int i=((64+y)&TMASK)*TW+((64+x)&TMASK); int d=x*x+y*y; int t = d<12 ? 0 : d<36 ? (d-12)*256/24 : 256; hmap[i]=(hmap[i]*t + 100*(256-t))>>8; }
#ifdef TEST_GEN
    nrm_cyc=TMR()-tq; tq=TMR();
#endif
    // smooth the map up to two texels per cell
    land_smooth();
#ifdef TEST_GEN
    smo_cyc=TMR()-tq; tq=TMR();
#endif
    land_colour();
#ifdef TEST_GEN
    col_cyc=TMR()-tq;
#endif
}
static void place(PObj *o, int kind, int minr, int maxr){ o->kind=kind; o->alive=1; o->hp=3; int a=wr(256), r=minr+wr(maxr-minr+1); o->x=(64<<16)+cosx(a)*r*4; o->y=(64<<16)+sinx(a)*r*4; }
static s32 wrapd(s32 d){ return ((d + (64<<16)) & ((TW<<16)-1)) - (64<<16); }        // shortest way round the world, 16.16
#include "town.inc"
static char tdbg[40];
static void new_planet(void){
    water_lv = endless ? 30+wr(60) : (zone==1 ? 100 : zone==0||zone==3 ? 70 : zone==5 ? 10 : 45);
    if(endless){ switch(wd.cls){ case PC_OCEAN: water_lv=86+wr(6); break; case PC_DESERT: water_lv=0; break; case PC_MOUNTAIN: water_lv=20+wr(20); break;
        case PC_DEATH: water_lv=45+wr(20); break;
        case PC_INFERNO: water_lv=48+wr(14); break; case PC_BRIMSTONE: water_lv=30+wr(14); break; case PC_OBSIDIAN: water_lv=12+wr(8); break;
        case PC_BONE: water_lv=0; break; case PC_BLOODSEA: water_lv=84+wr(8); break; case PC_COCYTUS: water_lv=34+wr(10); break; case PC_ECUMENOPOLIS: water_lv=10; break; case PC_MEADOW: water_lv=40+wr(30); break; case PC_PARADISE: water_lv=75+wr(12); break; } }
    g_hs = wd.hs ? wd.hs : 3;
    if(WL(MW_FLOOD)){ water_lv += WL(MW_FLOOD)*3; if(water_lv>92) water_lv=92; }                                   // FLOOD (the landing field stays dry)
    if(WL(MW_DROUGHT)) water_lv=0;                                                                                // DROUGHT
#ifdef TEST_GEN
    u32 tg0=TMR();
#endif
    gen_terrain(wd.tseed);                                                              // the land comes from its own seed
#ifdef TEST_GEN
    gen_cyc=TMR()-tg0;
#endif
    for(int i=0;i<NPO;i++) po[i].alive=0;
    int k=0;
    po[k].kind=O_SHIP; po[k].alive=1; po[k].x=(64<<16)-(3<<16); po[k].y=(64<<16)+(1<<16); k++;
    po[k].kind=O_DOOR; po[k].alive=1; { int a=wr(256); po[k].x=(64<<16)+cosx(a)*22*4; po[k].y=(64<<16)+sinx(a)*22*4; } k++;
    for(int i=0;i<2;i++) place(&po[k++],O_CAGE,14,40);
#ifdef TEST_BASE
    po[k].kind=O_BASE; po[k].alive=1; po[k].x=(66<<16); po[k].y=(64<<16); k++;
#else
    if(has_base(cur_sys) && !cur_pl && !cur_moon) place(&po[k++],O_BASE,24,40);
#endif
    for(int i=0;i<3;i++) place(&po[k++],O_SLUG,10,30);
    for(int i=0;i<20;i++) place(&po[k++],O_PLANT,4,64);
    { int eco = endless ? sys[cur_sys].eco : zone%4;                                  // what grows here depends on the world's economy and its DNA
      static const u8 mix[4][4]={{5,2,2,5},{2,7,2,3},{3,4,5,2},{2,3,6,3}};            // farming, mining, tech, hive
      static const u8 moonmix[4]={2,10,1,3};                                              // a moon is a mining rock: crystal and amber everywhere
      for(int t=0;t<4;t++){ int n=(cur_moon ? moonmix[t] : mix[eco][t])+wd.flora[t]-3; if(n<1) n=1; for(int i=0;i<n && k<NPO-50;i++) place(&po[k++],O_TREE+t,6,64); } }
    city_people(&k); planet_life(&k); pq_type=0; pq_done=0; race_on=0; kart_on=0; kart_won=0; race_best=0; kart_reset();
    while(k<NPO){ place(&po[k],(k&1)?O_PEBBLE:O_TUFT,3,64); k++; }
    for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind!=O_SHIP && (po[i].kind<O_MARKET || (po[i].kind>=O_FOSSIL && po[i].kind<=O_CRIT)) && hm_at(po[i].x,po[i].y)<water_lv){ po[i].x+=1<<16; if(hm_at(po[i].x,po[i].y)<water_lv) po[i].alive = po[i].kind<=O_CAGE ? 1 : 0; }
    for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==O_ROCK && ore_of(po[i].x,po[i].y)>=0 && ore_mined_recently(ore_id(po[i].x,po[i].y))) po[i].alive=0;      // veins you mined lately are still empty (the list holds the last 16, so they regrow as you mine others)
    ppx=64<<16; ppy=64<<16; cam_h = ground_h(ppx,ppy)+CAM_UP; facing=1; prompt_k=-2;
#ifdef TEST_CITY
    { static const u8 tk[11]={0,O_MARKET,O_CANTINA,O_OFFICE,O_RACE,O_LOCAL,O_RUIN,O_FOSSIL,O_CRIT,O_BEETLE,O_SHRINE}; int want=tk[TEST_CITY%11];
      if(TEST_CITY==0 && city_r){ ppx=city_cx<<16; ppy=((city_cy+city_r+2)&TMASK)<<16; }
      else for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==want){ ppx=po[i].x; ppy=po[i].y+(TEST_CITY>=8?(3<<16):(1<<16)); if(TEST_CITY==10){ for(int j=0;j<NPO;j++) if(j!=i && po[j].alive && po[j].kind>O_SHIP && po[j].kind<=O_LAST_USE) po[j].alive=0; ppy=po[i].y-(1<<16); } break; }
      { int n=0,f=0; for(int i=0;i<NPO;i++){ if(po[i].alive) n++; if(po[i].alive&&po[i].kind==want) f++; } scpy(tdbg,"TEST K"); ncat(tdbg,n); scat(tdbg," F"); ncat(tdbg,f); scat(tdbg," W"); ncat(tdbg,wd.weird); }
      cam_h=ground_h(ppx,ppy)+CAM_UP; }
#endif
#ifdef TEST_SHIPHIDE
    { s32 best=0; s32 cam_x0=ppx - ((hsn*cam_back())>>8), cam_y0=ppy - ((hcs*cam_back())>>8);     // (testing: park the ship where a ridge stands in front of it, straight ahead)
      for(int d=8; d<=18; d++) for(int l=-3; l<=3; l++){
        s32 ox=((l*hcs + d*hsn)<<8), oy=((-l*hsn + d*hcs)<<8);                          // cells ahead and sideways, 16.16
        s32 sx_=ppx+ox, sy_=ppy+oy, gh=ground_h(sx_,sy_), mx=-999999;
        for(int t=2;t<d;t++){ s32 h=ground_h(ppx+(ox*t/d), ppy+(oy*t/d)); if(h>mx) mx=h; }
        if(mx-gh>best){ best=mx-gh; po[0].x=sx_; po[0].y=sy_; } } (void)cam_x0; (void)cam_y0; }
#endif

#ifdef TEST_MINE
    po[3].kind=O_TREE; po[3].alive=1; po[3].hp=3; po[3].x=(64<<16)+(1<<16); po[3].y=(64<<16)-(1<<16); po[4].kind=O_ROCK; po[4].alive=1; po[4].hp=3; po[4].x=(64<<16)+(2<<16); po[4].y=(64<<16)+(1<<16);
#endif
}
static const u16 tile_of[]={T_DOOR,T_SHIP,T_CAGE,T_SLUG,T_PLANT,T_TREE,T_ROCK,T_WEB,T_FLOWER,T_HATCH, T_STALL,T_DOORWAY,T_DOORWAY,T_BEACON,T_DOORWAY,T_LOCAL1,T_FOSSIL,T_RUIN,T_SBEETLE,T_HOPPER,T_RUIN,T_BEACON,T_STALL, T_PEBBLE,T_TUFT, T_KDYN,T_IBOX,T_KSHELL,T_KSLIME};
static const u8 shape_of[]={0,0,1,1,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0, 1,0,0,1}, size_of[]={1,1,0,0,0,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1, 0,0, 2,1,1,0}, w_of[]={16,16,16,16,8,16,16,16,16,16, 16,16,16,16,16,16,16,16,16,16,16,16,16, 8,8, 32,16,16,16}, h_of[]={16,16,8,8,8,16,16,16,16,16, 16,16,16,16,16,16,16,16,16,16,16,16,16, 8,8, 16,16,16,8};

// ---------------------------------------------------------------- the renderer
static void planet_tables(void){
    s32 z = cam_fp ? (3<<8)/4 : (9<<8)/2;                   // start 0.75 cells out in first person, 4.5 behind the bug otherwise
    if(kr_mode && !cam_fp){ s32 m=(kcam_base()>>8)-(3<<7); if(m<(1<<8)) m=1<<8; if(m<z) z=m; }   // a close kart camera starts the march nearer
    for(int i=0;i<NSTEP;i++){ stp_z[i]=z; stp_f[i]=(FOCP<<16)/z; stp_base[i]=0; z += (cam_fp?24:40) + (i*i>>2); }   // steps grow with distance (8.8)
    { int fk=fov_k(); for(int c=0;c<120;c++) raydx[c]=(s16)(((2*c-119)*fk)/120); }    // field of view: about 70 degrees (the kart camera can change it), 8.8 cells sideways per cell of depth
    for(int y=0;y<160;y++){ int t = y<18 ? 0 : (y-18)*8/(PHZ+30-18); if(t>7) t=7; skyrow[y]=SKY_B+t; }
    { u32 r=0x2545F491; for(int i=0;i<256;i++){ r^=r<<13; r^=r>>17; r^=r<<5; int v=(r>>4)&7; gtex[i] = v==0?-1 : v==7?1 : 0; } }
    heading_rays();
}
IWRAM static void render_planet(volatile u16 *pg){
    s32 cb=cam_back(); s32 cam_x = ppx - ((hsn*cb)>>8), cam_y = ppy - ((hcs*cb)>>8);
    int ck = curv_k(); s32 csub = tiny_sub();                                       // small planets: the horizon falls away faster (measured from the bug)
    for(int i=0;i<NSTEP;i++){ s32 z=stp_z[i]; s32 curve = ((((z*z)>>14)*ck)>>curv_sh()) - csub;     // the world drops away
        stp_base[i] = ((cam_h + curve) * stp_f[i])>>8; }                             // (8.8 cells) * (8.8 px/cell) >> 8 = 8.8 px
    u32 sparkle=frames;
    for(int c=0;c<120;c++){
        int ytop=160; s32 vx=rayvx[c], vy=rayvy[c]; int bin=0;
        volatile u16 *col=pg+c;
        for(int i=0;i<g_nst;i++){
            s32 z=stp_z[i];
            s32 mx = cam_x + vx*z, my = cam_y + vy*z;
            int ti = ((my>>15)&HMASK)*HW + ((mx>>15)&HMASK);                             // texel: half a cell
            int h = hfine[ti]; if(g_wob){ h += wob64[(i*3+c)&63]; if(h<0) h=0; if(h>255) h=255; }
            s32 hh = h*g_hs;                                                           // 8.8 cells
            int y = PHZ + ((stp_base[i] - ((hh*stp_f[i])>>8))>>8);
            if(y<ytop){
                int ix = cmap[ti];
                if(i<NEAR_STEPS && ix>=GND_B+1 && ix<GND_B+15) ix += gtex[(((my>>11)&15)<<4)|((mx>>11)&15)];    // grass texture up close
                if(g_fog && i>=g_nst-6) ix=SKY_B+7-((g_nst-i)>>1);                                // FOG: the far ground fades into the mist
                if(ix<GND_B && ((((c+sparkle)*7+i*3)&31)==0)) ix=WAT_B+7;              // sparkles on the water
                u16 p2=ix|(ix<<8);
                int y0 = y<18 ? 18 : y;
                if(ix>=BLD_B && ix<BLD_B+8 && ytop-y0>3){                                // a building: a roof line, then a wall with window columns
                    int roof=76+((ix-BLD_B)>>1); col[y0*120]=roof|(roof<<8); int win=(ix&1);
                    for(int yy=y0+1; yy<ytop; yy++){ int c2 = (win && ((yy-y0)&3)>=2) ? 72+(((yy*3+c)&7)==0) : ix; col[yy*120]=c2|(c2<<8); } }
                else for(int yy=y0; yy<ytop; yy++) col[yy*120]=p2;
                ytop=y0;
                if(ytop<=18) break;
            }
            if((i&7)==7) ybin[c*8+bin++]=ytop;
        }
        while(bin<8) ybin[c*8+bin++]=ytop;
        for(int yy=18; yy<ytop; yy++){ int ix=skyrow[yy]; col[yy*120]=ix|(ix<<8); }
    }
    // a few stars in the sky
    for(int k=0;k<22;k++){ int x=(k*53+17)%120, y=19+((k*31+5)%50); if(y<ybin[x*8]) { int ix = ((frames>>3)+k)&7 ? 4 : 5; pg[y*120+x]=ix|(ix<<8); } }
}

// ---------------------------------------------------------------- the status bar, messages
static u32 phud_sig=0;
static void kw_topbar(void);
static void planet_hud(void){
    if(kw_mode){ kw_topbar(); return; }
    phud_sig=hud_sig(); char b[28]; scpy(b,world_name()); if(zone_cleared && !endless){ scat(b," T"); ncat(b,tiers[TZ]+1); } else if(hellw()){ scat(b," H"); ncat(b,hell_tier()); }
    for(int p=0;p<(m7_on?1:2);p++) draw_hud((volatile u16*)(0x06000000 + p*0xA000), b);     // (in a race the second page is the ground's map)
}
static void show_msg(const char *s){ int n=0; while(s[n] && n<39){ msg[n]=s[n]; n++; } msg[n]=0; msg_timer = n ? 150*xo_v(XO_MSGT)/100 : 0; }

// ---------------------------------------------------------------- one frame on the surface
static int p_last=0;
static void sky_put(volatile u16 *pg, int c, int y, int ix){                            // paint one pixel pair, only where the sky shows
    if(c<0||c>=120||y<19||y>=150) return; int v=pg[y*120+c]&0xFF; if((v>=SKY_B && v<SKY_B+8) || v==4 || v==5 || v==0) pg[y*120+c]=ix|(ix<<8); }
static void sky_decor(volatile u16 *pg){
    int ho=(heading*120)>>10;
    if(WL(MW_SUNS)){ int ns=2+WL(MW_SUNS)/7, r=3+WL(MW_SUNS)/4;                         // SUNS: two to five of them
        for(int k=0;k<ns;k++){ int cx=(20+k*29+120-ho)%120, cy=30+k*9;
            for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++){ int d=dx*dx*4+dy*dy; if(d<=r*r) sky_put(pg,cx+dx,cy+dy, d<r*r/3?94:d<r*r*2/3?89:88); } } }
    if(WL(MW_MOONS)){ int nm=1+WL(MW_MOONS)/5;                                          // MOONS
        for(int k=0;k<nm;k++){ int cx=(k*23+60+120-ho)%120, cy=24+(k*13)%34, r=2+(k%3);
            for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++){ int d=dx*dx*4+dy*dy; if(d<=r*r) sky_put(pg,cx+dx,cy+dy, dx>0?92:91); } } }
    if(WL(MW_RINGS)){ int t=WL(MW_RINGS);                                               // RINGS: a band across the sky
        for(int c=0;c<120;c++){ int dc=((c+ho)%120)-60; int y=26+t/2+dc*dc*(4+t/4)/900; sky_put(pg,c,y,90); sky_put(pg,c,y+1,93); if(t>12) sky_put(pg,c,y+3,90); } }
    if(WL(MW_METEOR)){ int t=(frames*3)%200; int x0=(frames/67*37)%120;                 // METEORS: streaks
        for(int k=0;k<8;k++) sky_put(pg,(x0+t/2+k)%120,20+t/3+k,94); }
}
static void weird_frame(int n){
    if(!wd.weird){ g_nst=NSTEP; g_wob=0; g_fog=0; return; }
    g_fog = WL(MW_FOG)!=0; g_nst = g_fog ? NSTEP-WL(MW_FOG) : NSTEP; if(g_nst<22) g_nst=22;
    g_wob = WL(MW_WOBBLE)!=0;
    if(g_wob){ int a=WL(MW_WOBBLE), sp=2+a/4; for(int k=0;k<64;k++) wob64[k]=(s8)((sinx((k*8+frames*sp)&255)*a*3)>>14); }
    if(WL(MW_RAINBOW)){ int every=1+(25-WL(MW_RAINBOW))/4; if(frames%every==0){                                     // RAINBOW: the ground's colours roll
        if(WL(MW_RAINBOW)&1){ u16 c0=BG_PAL[GND_B]; for(int i=0;i<15;i++) BG_PAL[GND_B+i]=BG_PAL[GND_B+i+1]; BG_PAL[GND_B+15]=c0; }
        else { u16 c0=BG_PAL[GND_B+15]; for(int i=15;i>0;i--) BG_PAL[GND_B+i]=BG_PAL[GND_B+i-1]; BG_PAL[GND_B]=c0; } } }
    if(WL(MW_GLITCH)){ int every=16-WL(MW_GLITCH)/2; if(every<2) every=2; if(frames%every==0){ int k=rndi(8); BG_PAL[SKY_B+k]=rgb15(rndi(256),rndi(256),rndi(256)); } }   // GLITCH: the sky flickers
    if(WL(MW_DISCO)){ int per=32-WL(MW_DISCO); if(frames%per==0){ int ph=(frames/per)&3;                                // DISCO
        u16 dc = ph==0 ? wcol[MW_DISCO] : ph==1 ? rgb15(255,40,80) : ph==2 ? rgb15(40,255,120) : rgb15(60,120,255);
        for(int i=0;i<16;i++) BG_PAL[GND_B+i]=lerp15(gbase[i],dc,4+WL(MW_DISCO)/3); } }
    if(WL(MW_AURORA) && (frames&3)==0) for(int i=0;i<4;i++){ int s=sinx((frames*2+i*40)&255); int t=((s+16384)*(4+WL(MW_AURORA)/3))>>15; BG_PAL[SKY_B+i]=lerp15(skybase[i],wcol[MW_AURORA],t>16?16:t); }   // AURORA
    if(WL(MW_SING)){ int every=100-WL(MW_SING)*3; if(frames%every==0){ static const u8 nt[4]={2,7,10,13}; sfx(nt[(frames/every)&3]); } }   // SINGING ROCKS
    if(WL(MW_HEALING)){ int every=200-WL(MW_HEALING)*6; if(frames%every==0 && hearts<maxh){ hearts++; hud_now(); } }     // HEALING AIR
    if(WL(MW_BADAIR)){ int every=260-WL(MW_BADAIR)*8; if(frames%every==0 && hearts>1){ hearts--; if(rndi(4)==0) show_msg("COUGH. THE AIR HERE IS BAD"); hud_now(); } }   // BAD AIR
    if(WL(MW_HEAT)){ int every=240-WL(MW_HEAT)*7; if(frames%every==0 && food>0){ food--; } }                              // HEAT: you get hungry
    if(WL(MW_METEOR)){ int every=720-WL(MW_METEOR)*25; if(frames%every==every-1){                                        // METEORS: one lands near you
        int j=-1; for(int i=0;i<NPO;i++) if((!po[i].alive && po[i].kind<O_KART) || po[i].kind==O_PEBBLE || po[i].kind==O_TUFT){ j=i; break; }
        if(j>=0){ int a=rndi(256), d=3+rndi(4); po[j].kind=O_ROCK; po[j].alive=1; po[j].hp=3; po[j].x=(ppx+cosx(a)*d*4)&((TW<<16)-1); po[j].y=(ppy+sinx(a)*d*4)&((TW<<16)-1);
            sfx(12); show_msg("A METEOR LANDED! A CRYSTAL ROCK NEARBY"); } } }
    (void)n;
}
static void hell_sky(volatile u16 *pg){                                                 // a great sun or moon hangs over every hell world
    static const u8 rr[6]={14,9,12,13,15,8}; int k=wd.cls-PC_INFERNO, ho=(heading*120)>>10, r=rr[k], cx=(30+k*17+120-ho)%120, cy=38+(k&1)*6;
    for(int dy=-r;dy<=r;dy++) for(int dx=-r;dx<=r;dx++){ int d=dx*dx*4+dy*dy; if(d<=r*r) sky_put(pg,cx+dx,cy+dy, d<r*r/3?94:d<r*r*2/3?89:88); }
    if(wd.cls==PC_BONE){ sky_put(pg,cx-2,cy-1,94); sky_put(pg,cx+2,cy-1,94); sky_put(pg,cx-2,cy,94); sky_put(pg,cx+2,cy,94); sky_put(pg,cx,cy+2,94); sky_put(pg,cx,cy+3,94); }   // the moon is a skull
}
static void hell_frame(int n){                                                          // what the six hell worlds do to you; every hit grows with the tier (hell_tier)
    (void)n; if(!hellw()) return; int T=hell_tier(); u32 f=frames; int sea = hm_at(ppx,ppy)<water_lv && !(form==FORM_CREATURE && form_body==2);
    if((wd.cls==PC_INFERNO || wd.cls==PC_BLOODSEA) && f%8==0){ u16 c0=BG_PAL[WAT_B+1]; for(int i=1;i<6;i++) BG_PAL[WAT_B+i]=BG_PAL[WAT_B+i+1]; BG_PAL[WAT_B+6]=c0; }   // the lava and the blood slowly churn
    if(wd.cls==PC_INFERNO && (f&3)==0){ int t=(sinx((f*3)&255)+16384)>>11; for(int i=5;i<8;i++) BG_PAL[SKY_B+i]=lerp15(skybase[i],rgb15(255,120,30),t>16?16:t); }
    if(wd.cls==PC_COCYTUS && (f&3)==0) for(int i=0;i<4;i++){ int s=sinx((f*2+i*40)&255); int t=((s+16384)*8)>>15; BG_PAL[SKY_B+i]=lerp15(skybase[i],rgb15(80,255,200),t>16?16:t); }
    switch(wd.cls){
        case PC_INFERNO:   if(sea && f%20==0){ hurt(2+T); show_msg("LAVA! GET OUT!"); } if(f%(300-T*20)==0 && food>0) food--; break;
        case PC_BRIMSTONE: if(f%(260-T*16)==0 && hearts>1){ hearts--; hud_now(); if(rndi(4)==0) show_msg("THE FUMES BURN YOUR LUNGS"); } if(sea && f%25==0){ hurt(1+T); show_msg("ACID!"); } break;
        case PC_OBSIDIAN:  if((pw_h||pw_v) && f%90==0){ hurt(1+T/2); show_msg("GLASS SHARDS CUT YOU"); } break;
        case PC_BONE:      if(f%(220-T*12)==0 && food>0) food--; break;
        case PC_BLOODSEA:  if(sea && f%40==0){ hurt(1+T/2); show_msg("THE RED SEA BITES"); } break;
        case PC_COCYTUS:   if(f%(240-T*14)==0 && food>0) food--; if(sea && f%30==0){ hurt(T); show_msg("THE ICE CRACKS! IT IS COLD"); } break;
    }
}
#include "m7.inc"
static void rock_frame(void){                                     // an icy rock melts while you are on it
    if(cur_rock!=3 || !rock_t) return; rock_t--;
    if(rock_t==6000) show_msg("THE ICE IS SOFTENING"); else if(rock_t==2400) show_msg("THE ICE IS MELTING! FINISH UP"); else if(rock_t==0){ show_msg("IT MELTED AWAY! LIFTING OFF"); launch_flag=1; } }
// ---- SCAN: with the SCANNER in hand, the guide arrow (SELECT + L/R) has a fourth stop that points at the richest ore vein of the world
static int scan_i=-1;                                      // the vein the scan found (an index into po[]), -1 for none
static int scan_vein(void){                                // the highest tier vein still in the ground; the nearest one wins a tie. One pass over the objects, nothing per screen column
    int best=-1, bt=0; s32 bd=0x7FFFFFFF;
    for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==O_ROCK){ int o=ore_of(po[i].x,po[i].y); if(o<0) continue; int t=ore_def[o].tier;
        s32 ddx=wrapd(po[i].x-ppx), ddy=wrapd(po[i].y-ppy); s32 d=(ddx<0?-ddx:ddx)+(ddy<0?-ddy:ddy); if(t>bt || (t==bt && d<bd)){ bt=t; bd=d; best=i; } }
    scan_i=best; return best; }
// ---- the ore map: a small round-the-bug map of the ore veins, in the corner of the planet view (the MINIMAP option). Veins are coloured by their ore.
// It costs nothing per screen column: the veins near you are listed once every half second, and each frame draws a 30x30 panel and at most OM_N dots.
#define OM_N 40
static u8 om_i[OM_N]; static u8 om_n; static int om_t=-999;
static const u8 om_col[NORE]={0,0,0,0, C_GREY,C_RED,C_CYAN,C_GOLD,C_DRED,C_WHITE,C_GREEN,C_UNIQ};      // iron grey, copper red, ice quartz cyan, sulfur gold, obsidian dark red, calcite white, scrap green, voidsteel purple
static void oremap_list(void){ om_n=0; om_t=frames;
    for(int i=0;i<NPO && om_n<OM_N;i++) if(po[i].alive && po[i].kind==O_ROCK){ s32 dx=wrapd(po[i].x-ppx), dy=wrapd(po[i].y-ppy); if(dx>(20<<16)||dx<-(20<<16)||dy>(20<<16)||dy<-(20<<16)) continue; if(ore_of(po[i].x,po[i].y)>=0) om_i[om_n++]=(u8)i; } }
static void planet_oremap(volatile u16 *pg){
    const int ox=19, oy=59, R=15;                           // the middle of the panel
    if(frames-om_t>=30 || frames<om_t) oremap_list();
    rect(pg,ox-R-1,oy-R-1,2*R+2,2*R+2,C_DGREY); rect(pg,ox-R,oy-R,2*R,2*R,1);
    for(int k=0;k<om_n;k++){ PObj *o=&po[om_i[k]]; if(!o->alive || o->kind!=O_ROCK) continue;
        s32 dx=wrapd(o->x-ppx), dy=wrapd(o->y-ppy); int sx=(int)((((dx>>8)*hcs - (dy>>8)*hsn))>>16), sy=-(int)((((dx>>8)*hsn + (dy>>8)*hcs))>>16);
        if(sx<-R+1||sx>R-2||sy<-R+1||sy>R-2) continue;
        int ov=ore_of(o->x,o->y); if(ov<0) continue; int c=om_col[ov], x=ox+sx, y=oy+sy;
        if(om_i[k]==scan_i && holding(TL_SCANNER) && (frames&8)) c=C_WHITE;                        // the scanned rich vein blinks
        pix(pg,x,y,c); pix(pg,x+1,y,c); pix(pg,x,y+1,c); pix(pg,x+1,y+1,c); }
    pix(pg,ox,oy,C_WHITE); pix(pg,ox,oy-1,C_WHITE); pix(pg,ox-1,oy,C_WHITE); pix(pg,ox+1,oy,C_WHITE); pix(pg,ox,oy+1,C_WHITE);      // you, in the middle
    pix(pg,ox,oy-R,C_GOLD); }                               // north of the view, the way you face
static void planet_frame(void){
    u32 t_f0=TMR();
#ifdef TEST_FPS
    { static u32 lf=0; kfr_sum+=t_f0-lf; lf=t_f0; if(++kfr_n==30){ kfr_avg=kfr_sum/30; kfr_sum=0; kfr_n=0; } }
#endif
    { int want = kr_mode && !in_menu; if(want && !m7_on) m7_begin(); else if(!want && m7_on) m7_end(); }      // a kart race: the hardware ground (m7.inc)
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
    int n = vbl_count - p_last; p_last=vbl_count; if(n<1) n=1; if(n>4) n=4;           // the view takes a few 60ths to draw: move by that much
    int moved=0;
    int th = HELD(K_LEFT) ? -16 : HELD(K_RIGHT) ? 16 : 0, tv = HELD(K_UP) ? -16 : HELD(K_DOWN) ? 16 : 0;
    if(kr_mode){ th=tv=0; }
    else if(HELD(K_SEL) && form==FORM_HUMAN && (HIT(K_UP)||HIT(K_DOWN))){ sel_used=1;                       // a humanoid: SELECT + UP/DOWN picks the tool in your hand
        if(!tools_owned) show_msg("NO TOOLS YET. BUY THEM AT A CITY MARKET");
        else { int d=HIT(K_UP)?NTOOL-1:1; do tool_held=(tool_held+d)%NTOOL; while(!((tools_owned>>tool_held)&1)); recalc(); char b[48]; scpy(b,"HOLDING: "); scat(b,tool_name[tool_held]); scat(b," ("); scat(b,tool_what[tool_held]); scat(b,")"); show_msg(b); sfx(2); } }
    if(HELD(K_SEL) && !kr_mode){ if(!cam_fp){ int tr = HELD(K_LEFT) ? -16 : HELD(K_RIGHT) ? 16 : 0; if(tr){ sel_used=1; heading=(heading+tr*n*5/8)&1023; heading_rays(); } }    // third person: hold SELECT + LEFT/RIGHT to turn the camera
        th=tv=0; if(HIT(K_L)||HIT(K_R)){ sel_used=1; arrow_ship=(arrow_ship+1)%(holding(TL_SCANNER)?4:3); scan_i=-1;
            if(arrow_ship==3){ int v=scan_vein(); if(v<0){ arrow_ship=0; show_msg("SCAN: NO VEIN ON THIS WORLD"); } else { char b[44]; scpy(b,"SCAN: RICH VEIN: "); scat(b,ore_def[ore_of(po[v].x,po[v].y)].name); show_msg(b); } }
            else show_msg(arrow_ship==1?"ARROW: TO YOUR SHIP":arrow_ship==2?"ARROW: TO THE NEAREST RESOURCE":"ARROW: TO THE DOOR"); sfx(7); } }
    if(kr_mode){}
    else if(cam_fp){                                                                    // first person: left/right turn, L/R step sideways
        int turn = th; th = HELD(K_SEL) ? 0 : HELD(K_R) ? 16 : HELD(K_L) ? -16 : 0;
        if(turn){ heading=(heading+turn*n*5/8)&1023; heading_rays(); }
    } else { if(HELD(K_LEFT)) facing=0; if(HELD(K_RIGHT)) facing=1; }
#ifdef TEST_ORBIT
    if(!cam_fp && !kr_mode){ heading=(heading+(TEST_ORBIT)*n)&1023; heading_rays(); }   // (for filming: the camera sweeps slowly round as you walk)
#endif
    for(int k=0;k<n;k++){
        if(kr_mode){ kart_step(); continue; }                                           // in a kart (kart.inc)
        { int ph0=pw_h, pv0=pw_v, ik = WL(MW_ICE) ? 2+WL(MW_ICE)/3 : 0;                 // ICE: you keep sliding
          pw_h += th ? (th-pw_h)/4 + (th>pw_h?1:th<pw_h?-1:0) : -pw_h/3; pw_v += tv ? (tv-pw_v)/4 + (tv>pw_v?1:tv<pw_v?-1:0) : -pw_v/3;
          if(ik && (frames+k)%ik){ if(!th) pw_h=ph0; else pw_h=ph0+(pw_h-ph0)/2; if(!tv) pw_v=pv0; else pw_v=pv0+(pw_v-pv0)/2; } }
        s32 sp = 0x1600 + (wd.grav-1)*0x200; if(WL(MW_ZEROG)) sp=sp*(14+WL(MW_ZEROG))/10;   // ZERO-G: you bounce along
        if(WL(MW_STICKY)) sp=sp*10/(12+WL(MW_STICKY)); if(d_spd||dbg_spd) sp=sp*(100+d_spd+dbg_spd)/100;      // STICKY GROUND; your marks and gear                                            // cells per 60th, 16.16: about 5 cells a second (gravity changes it)
        if(kart_on){ sp=sp*9/4; if(HELD(K_B) && kart_boost>0){ sp=sp*7/4; kart_boost--; } else if(!HELD(K_B) && kart_boost<100 && (frames+k)%3==0) kart_boost++; }   // the kart: fast, and B boosts
        if(hm_at(ppx,ppy)<water_lv && !(form==FORM_CREATURE && form_body==2)) sp=sp*2/3;   // wading is slow (flyers don't wade)
        s32 mvx=(pw_h*sp)>>4, mvy=(pw_v*sp)>>4;
        { s32 fwd=-mvy, side=mvx; mvx=((hsn*fwd)>>8)+((hcs*side)>>8); mvy=((hcs*fwd)>>8)-((hsn*side)>>8); }   // UP walks away from the camera (into the screen), in both views
        s32 nx = ppx + mvx, ny = ppy + mvy;
        // climbing a steep slope is slow
        s32 h0=ground_h(ppx,ppy), h1=ground_h(nx,ny); s32 dh=h1-h0;
        s32 wall=0x40*g_hs; if(dh>wall){ if(ground_h(nx,ppy)-h0<=wall) ny=ppy; else if(ground_h(ppx,ny)-h0<=wall) nx=ppx; else { nx=ppx; ny=ppy; } }   // walls: slide along them
        else if(dh>0x0060){ nx=ppx+((nx-ppx)>>1); ny=ppy+((ny-ppy)>>1); }
        ppx = nx & ((TW<<16)-1); ppy = ny & ((TW<<16)-1);
        if(pw_h||pw_v) moved=1;
        s32 want = ground_h(ppx,ppy)+cam_up(); if(!cam_fp){ s32 cg=ground_h(ppx-((hsn*CAM_BACK)>>8), ppy-((hcs*CAM_BACK)>>8)); if(cg+(1<<8)>want) want=cg+(1<<8); }   // ...and climbs over buildings behind you
        cam_h += (want-cam_h)>>3;                                                       // the camera eases up and down the hills
    }
    if(kr_mode) moved=0;
    if(moved && (pw_step+=n)>=14){ pw_step=0; sfx(hm_at(ppx,ppy)<water_lv?8:11); }
    life_tick(n); race_tick(n); kart_tick(n);
    // slugs crawl toward you
    for(int i=0;i<NPO;i++) if(po[i].kind==O_SLUG && po[i].alive){ PObj *o=&po[i]; s32 dx=wrapd(ppx-o->x), dy=wrapd(ppy-o->y);
        s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy);
        if(ad<(14<<16)){ int sg = WL(MW_SHY) ? -1 : 1, ml = 8+WL(MW_ANGRY);                 // SHY LIFE runs away; ANGRY LIFE hurries
            o->x=(o->x+sg*((dx>>7)*n)*ml/8)&((TW<<16)-1); o->y=(o->y+sg*((dy>>7)*n)*ml/8)&((TW<<16)-1); }
        if(ad<0x14000 && !invuln){ hurt((3+dlv()*2)*deadly_mul()); if(invuln>20) show_msg("OUCH! A SLUG"); pknock(dx,dy,40); } }
    // what is next to you
    int best=-1; s32 bd=0x1A000;
    for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind<=O_LAST_USE){ s32 dx=wrapd(po[i].x-ppx), dy=wrapd(po[i].y-ppy); s32 d=(dx<0?-dx:dx)+(dy<0?-dy:dy); if(d<bd){ bd=d; best=i; } }
    if(kr_mode){ if(kart_buttons()) return; }
    else if(HIT(K_A)){
        if(best<0) show_msg("NOTHING HERE");
        else switch(po[best].kind){
            case O_DOOR: enter_dungeon_flag=1; sfx(5); break;
            case O_BASE: enter_dungeon_flag=2; sfx(5); show_msg("A DRONE OUTPOST. BOLT: FIND THE KEYCARD"); break;
            case O_SHIP:
                { hdma_on=0; int r=shop_menu();
                  planet_hud(); if(!r){ save_game(); show_msg(opt_autosave?"GAME SAVED":""); p_last=vbl_count; if(kart_req){ kart_req=0; kart_menu(0); } return; }
                  save_game(); launch_flag=r; return; }
            case O_CAGE: po[best].alive=0; free_bug(rndi(8)<2?BUG_PILL:rndi(6)); break;
            case O_SLUG: po[best].alive=0; gold+=5; show_msg("SQUASH! +5 GOLD"); gain_xp(3+dlv()); sfx(9); if(pq_type==1){ pq_have++; job_tick(); } break;
            case O_MARKET: { oam_hide_all(); oam_flush(); in_menu++; market(VISPG); in_menu--; planet_hud(); p_last=vbl_count; return; }
            case O_CANTINA: { oam_hide_all(); oam_flush(); in_menu++; cantina(VISPG); in_menu--; planet_hud(); p_last=vbl_count; return; }
            case O_OFFICE: { oam_hide_all(); oam_flush(); bounty_office(VISPG); planet_hud(); p_last=vbl_count; return; }
            case O_HOME: { oam_hide_all(); oam_flush(); const char *ln[2]; static const char *const kn[6]={"KNOCK KNOCK. NOBODY. THEN A SNORE.","A MOTH ANSWERS. SHE IS THE MAYOR. SHE WAVES.","SOMEONE SHOUTS: NO SOUP TODAY!","A TINY VOICE: WE GAVE AT THE OFFICE.","THE DOOR OPENS. A CAT. ON A PLANET. ODD.","A BEETLE IN A ROBE SAYS: IT IS LATE. GO HOME."};
                ln[0]=kn[(po[best].x>>16)%6]; ln[1]=po[best].hp ? "SOMETHING ROLLS OUT UNDER THE DOOR: 10G" : "..."; if(po[best].hp){ po[best].hp=0; gold+=10; sfx(7); }
                card(VISPG,"A HOUSE",ln,2); planet_hud(); p_last=vbl_count; return; }
            case O_LOCAL: { oam_hide_all(); oam_flush(); local_talk(&po[best]); job_tick(); planet_hud(); p_last=vbl_count; return; }
            case O_SHRINE: { oam_hide_all(); oam_flush(); po[best].alive=0; relic_found(); planet_hud(); p_last=vbl_count; return; }
            case O_RACE: if(race_on){ race_on=0; kart_on=0; for(int i=0;i<NPO;i++) if(po[i].kind==O_BEACON) po[i].alive=0; show_msg("RACE CALLED OFF"); } else { kart_menu(1); return; } break;   // the race post: the kart club (kart2.inc)
            case O_RUIN: if(!po[best].hp){ show_msg("THE STONES ARE QUIET NOW"); break; } { oam_hide_all(); oam_flush(); ruin_visit(VISPG,&po[best]); planet_hud(); p_last=vbl_count; return; }
            case O_FOSSIL: { PObj *o=&po[best]; sfx(12); mine_t=8; if(--o->hp>0){ char b[40]; scpy(b,"DIG... "); ncat(b,o->hp); scat(b," MORE"); show_msg(b); break; }
                o->alive=0; st_mined++; gain_xp(4+dlv()); if(cargo_n()<cargo_cap+ship_hold_bonus()){ cargo[7]++; show_msg("A FOSSIL! +1 RELIC IN THE HOLD"); } else { gold+=20; show_msg("A FOSSIL! HOLD FULL, SO: +20G"); }
                if(rndi(3)==0){ Gear g={IT_SHELL,(u8)rndi(4),0,0,0,(u8)dlv()}; add_gear(g); show_msg("OLD BONES... AND AN OLD SHELL!"); } sfx(6); if(pq_type==3){ pq_have++; job_tick(); } if(relic_roll(150)){ planet_hud(); p_last=vbl_count; return; } break; }
            case O_BEETLE: { PObj *o=&po[best]; sfx(0); mine_t=6; s32 dx=wrapd(o->x-ppx), dy=wrapd(o->y-ppy); o->x=(o->x+dx*2)&((TW<<16)-1); o->y=(o->y+dy*2)&((TW<<16)-1); invuln=20;
                if(--o->hp>0){ char b[40]; scpy(b,"BITE! THE BEETLE HAS "); ncat(b,o->hp); scat(b," LEFT"); show_msg(b); break; }
                o->alive=0; gold+=12+dlv()*3; gain_xp(8+dlv()*2); st_kills[2]++; show_msg("BEETLE DOWN! +GOLD"); sfx(9); if(pq_type==2){ pq_have++; job_tick(); } if(relic_roll(250)){ planet_hud(); p_last=vbl_count; return; } break; }
            case O_CRIT: { PObj *o=&po[best]; Species *p=&spc[o->var]; sfx(0); mine_t=6; invuln=12; s32 dx=wrapd(o->x-ppx), dy=wrapd(o->y-ppy); o->x=(o->x+dx)&((TW<<16)-1); o->y=(o->y+dy)&((TW<<16)-1); ctim[o->var]=0;
                if(--o->hp>0){ char b[40]; scpy(b,p->temper==1?"IT LOOKS HURT. AND CONFUSED":"THE "); if(p->temper!=1){ scat(b,p->name); scat(b," SQUEAKS"); } show_msg(b); break; }
                o->alive=0; p->pop=crit_count(o->var); eat(12); gain_xp(5+dlv()); char b[40]; scpy(b,"CAUGHT A "); scat(b,p->name); scat(b,". FOOD +12. "); ncat(b,p->pop); scat(b," LEFT"); show_msg(b); sfx(9);
                if(pq_type==6 && o->var==0){ pq_have++; job_tick(); } break; }
            case O_TREE: case O_ROCK: case O_WEB: case O_FLOWER: {                     // mining: a few bites, then the goods go in your ship's hold
                PObj *o=&po[best]; int t=o->kind-O_TREE; int timed=(mine_t==0 && mine_gap<=WP_WIN); sfx(o->kind==O_ROCK?12:13); mine_t=8;
                { int ore = o->kind==O_ROCK ? ore_of(o->x,o->y) : -1;                                      // a vein: a harder rock that gives ore, and needs the right tool
                  if(ore>=0){ if(ore_tool_eff()<ore_need_tool(ore)){ char b[40]; scpy(b,ore_def[ore].name); scat(b," VEIN: NEEDS A "); scat(b,ore_tool_name[ore_need_tool(ore)]); show_msg(b); sfx(4); break; }
                      if(!o->var){ o->var=1; o->hp=(u8)ore_hits(ore); }
                      if(timed && o->hp>1) o->hp--;                                                  // a TIMED bite counts double                              // (the first bite works out how many it needs with your tool)
                      if(--o->hp>0){ char b[40]; scpy(b,timed?"WELL TIMED! ":""); scat(b,ore_def[ore].name); scat(b," VEIN: "); ncat(b,o->hp); scat(b," MORE"); show_msg(b); break; }
                      o->alive=0; ore_mark_mined(ore_id(o->x,o->y)); int tt=ore_tool_eff(), n=1+rndi(2)+(tt>=2)+(cur_moon?1:0)+(hellw()?hell_tier()/4:0)+(cur_rock==4?3 : cur_rock==2?1 : 0); n+=dlv()/10; int oq=ore_quality(o->x,o->y); n=ore_qn(n,oq); int mg=ore_mega(o->x,o->y); if(mg){ n=n*3+2; if(oq<2) oq=2; } if(pq_type==7){ pq_have++; job_tick(); }
                      int got=ore_add_q(ore,n,oq); st_mined+=n; ml_add(ore,got); if(pq_type==10 && ore==pq_ore) pq_have=ore_n[ore]; ach_check(); gain_xp(3+dlv()+ore_def[ore].tier*2);
                      char b[44]; scpy(b,mg?"MEGA! +":"+"); ncat(b,got); scat(b," "); scat(b,oq_name[oq]); scat(b,ore_def[ore].name); scat(b," ORE  BAG "); ncat(b,ore_n[ore]); show_msg(b); sfx(7);
                      if(cur_rock==4 && rndi(5)==0){ hurt(2); show_msg("CRACK! SHARDS HIT YOU"); sfx(4); }          // a crystal rock is brittle
                      break; } }
                if(timed && o->hp>1) o->hp--;
                if(holding(TL_PICK)) o->hp=1;                                                    // the pickaxe: one hit
                if(--o->hp>0){ char b[40]; scpy(b,timed?"WELL TIMED! ":""); scat(b,node_name[t]); scat(b,": "); ncat(b,o->hp); scat(b," MORE"); show_msg(b); break; }
                o->alive=0; int g=node_good[t], n=1+rndi(2)+(rndi(4)==0)+(holding(TL_PICK)?1:0)+(cur_moon?1:0)+(hellw()?hell_tier()/3:0)+(cur_rock==4?2:0); if(pq_type==7){ pq_have++; job_tick(); }
                if(relic_roll(400)){ planet_hud(); p_last=vbl_count; return; }
                if(cargo_n()>=cargo_cap+ship_hold_bonus()){ show_msg("HOLD FULL! SELL GOODS AT A STATION"); break; }
                int room=cargo_cap+ship_hold_bonus()-cargo_n(); if(n>room) n=room; cargo[g]+=n; st_mined+=n; ach_check(); gain_xp(2+dlv());
                char b[40]; scpy(b,"+"); ncat(b,n); scat(b," "); scat(b,goods[g]); scat(b,"  HOLD "); ncat(b,cargo_n()); scat(b,"/"); ncat(b,cargo_cap+ship_hold_bonus()); show_msg(b); sfx(7);
                break; }
            case O_PLANT: { po[best].alive=0; int r=rndi(10); if(pq_type==9){ pq_have++; job_tick(); }
                if(r<5){ eat(15); show_msg("NIBBLED A LEAF. FOOD +15"); sfx(11); }
                else if(r<8){ Gear g={IT_BERRY,(u8)rndi(8),0,0,0,0}; if(add_gear(g)) show_msg("FOUND A BERRY IN THE LEAVES"); }
                else if(r<9){ Gear g={IT_SEED,(u8)rndi(6),0,0,0,0}; if(add_gear(g)) show_msg("FOUND A SEED"); }
                else { gold+=2+dlv(); show_msg("A SHINY PEBBLE! +GOLD"); sfx(7); }
                break; }
        }
    }
    if(frames%600==0){ for(int i=0;i<NPO;i++) if(po[i].kind==O_PLANT && !po[i].alive){ place(&po[i],O_PLANT,4,64); break; } }
    if(!kart_on) bag_input(); tick_timers(); hunger_tick(240); regen_tick(120);
    if(HIT(K_START) && !kr_mode){ oam_hide_all(); oam_flush(); char_screen(VISPG); planet_hud(); msg_timer=0; p_last=vbl_count; if(kart_req){ kart_req=0; kart_menu(0); } return; }
    if(cast_flag>=0){ cam_fp^=1; planet_tables(); show_msg(cam_fp?"FIRST PERSON. SELECT: SWITCH VIEW":"THIRD PERSON. HOLD SELECT + LEFT/RIGHT: TURN"); sfx(2); }
    escape_flag=call_flag=blast_flag=0; cast_flag=-1;
    if(hearts<=0){ hearts=maxh; food=60; int nolose=!opt_faintloss||opt_baby; if(!nolose) gold=sdiv(gold,2); if(!nolose && !opt_easy){ bag_n=0; bag_sel=0; } show_msg(nolose?"YOU FAINTED. NOTHING LOST. NAP TIME":opt_easy?"YOU FAINTED. HALF YOUR GOLD IS GONE":"YOU FAINTED. YOU LOST YOUR BAG"); planet_hud(); }
    if(msg_timer){ msg_timer-=n; if(msg_timer<0) msg_timer=0; }
    if(frames%30==0 && hud_sig()!=phud_sig) planet_hud();
    if(opt_autosave && frames%6000==5999){ save_game(); show_msg("GAME SAVED"); }          // a quiet save every few minutes on the surface
    // ---- draw the world
    weird_frame(n); hell_frame(n); rock_frame();
    if(m7_on) m7_sky_now();                                                             // (a race: the sky, loops and HUD of the frame on show, once the beam is past them)
    u32 t_r0=TMR(); if(m7_on) m7_frame(); else render_planet(pg); u32 t_r1=TMR();
#ifdef TEST_FPS
    if(m7_on){ m7_t[0]=t_r0-t_f0; m7_t[1]=t_r1-t_r0; }
#endif
    if(m7_on) goto sprites;                                                            // (a race: the loops, messages and HUD are drawn after the blank, by m7_strip)
    if(kr_mode==1){ kloop_draw(pg,18,160); kbridge_draw(pg,18,160); }                    // loop-de-loops stand on the road (kart.inc)
    if(opt_minimap && !FILM && !kr_mode) planet_oremap(pg);
    if(msg_timer && !FILM){ int L=textlen(msg); if(kr_mode) stext(pg,120-L*2,50,msg,C_WHITE);                                                            // in a kart: under the HUD
                   else if(L<=29) text(pg,120-L*4,24,msg,C_WHITE,2); else stext(pg,120-L*2,26,msg,C_WHITE); }
#ifdef TEST_FPS
    { static u32 tf0=0; u32 now=TMR(); char fb[40]; scpy(fb,"N"); ncat(fb,n); scat(fb," RENDER "); ncat(fb,(int)((t_r1-t_r0)/1000)); scat(fb,"K PRE "); ncat(fb,(int)((t_r0-t_f0)/1000)); scat(fb,"K FRAME "); ncat(fb,(int)((now-tf0)/1000)); scat(fb,"K"); tf0=now; rect(pg,0,40,240,8,0); text(pg,4,41,fb,C_GOLD,1); }
#endif
    { int pk = best<0 ? -1 : po[best].kind;
      static const char *const pt[21]={"A: GO DOWN THE DOOR","A: YOUR SHIP","A: OPEN THE CAGE","A: SQUASH THE SLUG","A: SEARCH THE PLANT","A: TAP THE AMBER TREE","A: MINE THE CRYSTAL","A: GATHER THE SILK","A: SIP THE NECTAR","A: ENTER THE DRONE OUTPOST",
          "A: THE MARKET","A: THE CANTINA","A: THE BOUNTY OFFICE","A: THE RACE POST","A: KNOCK","A: TALK","A: DIG THE FOSSIL","A: THE RUIN","A: BITE THE BEETLE","A: CATCH IT","A: THE RELIC SHRINE!"};
      if(kr_mode) kart_hud(pg);
      else if(pk>=0 && opt_hints){ char cb[40]; const char *s=pt[pk]; if(pk==O_ROCK){ int ov=ore_of(po[best].x,po[best].y); if(ov>=0){ scpy(cb,"A: MINE "); scat(cb,oq_name[ore_quality(po[best].x,po[best].y)]); scat(cb,ore_def[ov].name); scat(cb," VEIN"); if(ore_tool_eff()<ore_need_tool(ov)){ scat(cb," (NEEDS "); scat(cb,ore_tool_name[ore_need_tool(ov)]); scat(cb,")"); } s=cb; } } if(pk==O_CRIT){ scpy(cb,"A: CATCH THE "); scat(cb,spc[po[best].var].name); s=cb; } else if(pk==O_DOOR && portal_depth>0){ scpy(cb,"A: PORTAL BACK TO B"); ncat(cb,portal_depth); s=cb; } stext(pg,120-textlen(s)*2,msg_timer?38:26,s,C_GOLD); }
      if(kr_mode || FILM){}
      else if(race_on){ char rb[40]; scpy(rb,kart_on?"KART ":"RACE "); ncat(rb,race_t/60); scat(rb,kart_on?"S  GATE ":"S  FLAG "); ncat(rb,race_next+1); scat(rb,"/"); ncat(rb,race_n); if(kart_on){ scat(rb,"  PAR "); ncat(rb,race_par/60); scat(rb,"S"); rect(pg,4,146,50,3,C_DGREY); if(kart_boost>0) rect(pg,4,146,kart_boost/2,3,kart_boost>20?C_GOLD:C_RED); } stext(pg,4,151,rb,C_CYAN); }
      else if(pq_type && !pq_done){ char jb[48]; if(pq_type==10) pq_have=ore_n[pq_ore]; scpy(jb,"JOB: "); { char t2[40]; job_text(t2); scat(jb,t2); } if(pq_type!=4 && pq_type!=8){ scat(jb," "); ncat(jb,pq_have); scat(jb,"/"); ncat(jb,pq_need); } stext(pg,4,151,jb,C_GOLD); }
#ifdef TEST_GEN
      { char gb[48]; scpy(gb,"GEN "); ncat(gb,(int)(gen_cyc/16780)); scat(gb,"MS OCT "); ncat(gb,(int)(oct_cyc/16780)); scat(gb," COL "); ncat(gb,(int)(col_cyc/16780)); scat(gb," CITY "); ncat(gb,(int)(city_cyc/16780)); stext(pg,4,130,"",C_RED); { char g2[40]; scpy(g2,"NRM "); ncat(g2,(int)(nrm_cyc/16780)); scat(g2," SMO "); ncat(g2,(int)(smo_cyc/16780)); stext(pg,4,40,g2,C_RED); } stext(pg,4,140,gb,C_RED); }
#endif
      weather_draw(pg); if(wd.weird) sky_decor(pg); if(hellw()) hell_sky(pg);
#ifdef TEST_CITY
      stext(pg,4,140,tdbg,C_RED);
#endif
      }
    (void)prompt_k;
sprites:;
#ifdef TEST_FPS
    u32 t_s0=TMR();
#endif
    // ---- sprites: project everything onto the terrain
    s32 cam_y = ppy - ((hcs*cam_back())>>8), cam_x = ppx - ((hsn*cam_back())>>8);
    for(int m=0;m<28;m++){ int s=8+m*4; int inv=16384/s; set_affine(m, inv, 0, 0, inv); }      // scales 0.125x .. 1.8x
    oam_hide_all();
    int slot=0;
    s32 bug_sy;
    { s32 dz=cam_back(); s32 f=(FOCP<<16)/(dz>>8); s32 gh=ground_h(ppx,ppy)+kbr_cur; s32 curve=curv(dz>>8) - tiny_sub();
      bug_sy = PHZ + (((cam_h + curve - gh) * f)>>16); }
    if(kr_mode && (!cam_fp || kloop_on)){ slot=kart_draw_me(slot,bug_sy); if(!kart_glow() && !kloop_on && slot<120){ shadow_oam[slot].a0=((bug_sy-6)&255)|(1<<14); shadow_oam[slot].a1=((120-8)&511); shadow_oam[slot].a2=T_SHADOW; slot++; } }   // in a kart
    else if(kr_mode){}
    else if(!cam_fp){                                                                   // the bug, standing on the ground ahead of the camera
      int sy=bug_sy;
      int bob = moved ? ((frames>>3)&1) : 0; if(WL(MW_ZEROG)) bob += 3+WL(MW_ZEROG)/3+(sinx((frames*3)&255)>>11);
      int pvis = !(invuln && (frames&4));
      if(mine_t){ if(!--mine_t) mine_gap=0; } else if(mine_gap<99) mine_gap++;
      int lean = moved ? 10 : (mine_t ? 24 : 0), sq = (moved && ((frames>>3)&1)) || mine_t ? 8 : 0;
      int up = tv<0 && !th && form==FORM_BUG;
      int shr=WL(MW_SHRINK)*5+64;   /* 64 = standing at two thirds of the old size */                                                          // SHRINK RAY: you are smaller
      set_affine(31, up ? 256+shr*2 : facing? -(128+shr):128+shr, up ? 0 : facing? lean:-lean, 0, up ? 256+shr*2 : 128+sq+shr);
      if(shr && form!=FORM_HUMAN) sy += up ? 32-8192/(256+shr*2) : 16-2048/(128+shr);
      if(pvis && form==FORM_HUMAN){                                                   // the humanoid: its own height, build, colours, ears and tail (see the creator in ui.inc)
                slot=hum_oam(slot,120,sy-bob,2,facing,lean,sq,shr,moved?((frames>>3)&1):0,tools_owned?tool_held:-1); }
      else if(pvis && form==FORM_CREATURE){                                           // a creature: one of this world's body plans, in your colour
                int fl = form_body==2; int hop = (form_body==0 && moved) ? ((frames>>2)&1)*4 : 0; int fly = fl ? 10+(sinx((frames*5)&255)>>12) : 0;
                shadow_oam[slot].a0 = ((sy-(fl?16:32)-bob-hop-fly)&255) | (1<<8)|(1<<9) | (fl?(1<<14):0);
                shadow_oam[slot].a1 = ((120-16)&511) | (31<<9) | (fl?(0<<14):(1<<14)); shadow_oam[slot].a2 = (form_body==0?T_HOPPER:form_body==1?T_GRAZER:T_FLYER) | (6<<12); slot++; }
      else if(pvis){ if(up){ shadow_oam[slot].a0 = ((sy-96-bob)&255) | (1<<8)|(1<<9); shadow_oam[slot].a1 = ((120-64)&511) | (31<<9) | (3<<14); }
                else bug_obj(slot,120,sy-16-bob,31,128+sq+shr);
                shadow_oam[slot].a2 = (up ? T_BACK : bug_tile(moved && ((frames>>3)&1))) | BUG_BANK; slot++; }
      if(slot<120){ shadow_oam[slot].a0 = ((sy-6)&255) | (1<<14); shadow_oam[slot].a1 = ((120-8)&511); shadow_oam[slot].a2 = T_SHADOW; slot++; }
    } else {                                                                            // first person: your own jaws, swaying as you walk
      if(!(invuln && (frames&4))){
        int sway = moved ? (sinx(frames*8)>>12) : (sinx(frames*2)>>13);
        int ang=sway&255; s32 cc=cosx(ang)>>6, sn=sinx(ang)>>6;
        set_affine(28, cc, -sn, sn, cc); set_affine(29, -cc, -sn, -sn, cc);
        int dy = (moved ? (sinx(frames*16)>>13) : 0) + 2;
        shadow_oam[slot].a0 = ((136-64+dy)&255) | (1<<8)|(1<<9); shadow_oam[slot].a1 = ((52-64)&511) | (28<<9) | (3<<14); shadow_oam[slot].a2 = T_JAW; slot++;
        shadow_oam[slot].a0 = ((136-64+dy)&255) | (1<<8)|(1<<9); shadow_oam[slot].a1 = ((188-64)&511) | (29<<9) | (3<<14); shadow_oam[slot].a2 = T_JAW; slot++; }
    }
    // the guide arrow: it circles the bug (or sits at the top of the view) and points the short way to the door or the ship
    if(!kr_mode){ PObj *t=&po[arrow_ship==1?0:1];
      if(race_on){ for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==O_BEACON){ t=&po[i]; break; } }
      else if(arrow_ship==3 && holding(TL_SCANNER)){ if(scan_i<0 || !po[scan_i].alive || (frames&63)==0) scan_vein(); if(scan_i>=0) t=&po[scan_i]; }
      else if(arrow_ship==2){ s32 bd2=0x7FFFFFFF; for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind>=O_TREE && po[i].kind<=O_FLOWER){ s32 ddx=wrapd(po[i].x-ppx), ddy=wrapd(po[i].y-ppy); s32 d=(ddx<0?-ddx:ddx)+(ddy<0?-ddy:ddy); if(d<bd2){ bd2=d; t=&po[i]; } } }
      s32 dx=wrapd(t->x-ppx), dy=wrapd(t->y-ppy);
      if(opt_arrow && (dx<0?-dx:dx)+(dy<0?-dy:dy) > 0x30000){
          s32 rx=((dx*hcs)>>8)-((dy*hsn)>>8), rz=((dx*hsn)>>8)+((dy*hcs)>>8);       // in camera space: right, and ahead
          s32 l=isqrt((u32)((rx>>8)*(rx>>8)+(rz>>8)*(rz>>8))); if(l<1) l=1;
          s32 ux=(rx>>8)*256/l, uy=-(rz>>8)*256/l;                                  // ahead points up the screen
          set_affine(30, ux, uy, -uy, ux);
          int pulse=(sinx(frames*6)>>12); int cx, cy;
          if(cam_fp){ cx=120+ux*(14+pulse)/256; cy=52+uy*(10+pulse)/256; } else { cx=120+ux*(30+pulse)/256; cy=bug_sy-14+uy*(16+pulse)/256; }
          shadow_oam[slot].a0 = ((cy-16)&255) | (1<<8)|(1<<9); shadow_oam[slot].a1 = ((cx-16)&511) | (30<<9) | (1<<14); shadow_oam[slot].a2 = T_ARROW; slot++; } }
    // everything else, far to near so near things draw on top
    EW static u8 order[NPO]; EW static s32 odz[NPO]; int cnt=0;
    s32 near_z = cam_fp ? (1<<16) : (5<<16);
    EW static s32 odx[NPO]; s32 fovq=65536/fov_k();
    for(int i=0;i<NPO;i++){ PObj *o=&po[i]; if(!o->alive) continue; s32 wx=wrapd(o->x-cam_x), wy=wrapd(o->y-cam_y);
        s32 dz=((wx*hsn)>>8)+((wy*hcs)>>8), dx=((wx*hcs)>>8)-((wy*hsn)>>8);
        if(dz<near_z || dz>(40<<16)) continue; odz[i]=dz; odx[i]=dx; order[cnt++]=i; }
    for(int i=1;i<cnt;i++){ u8 t=order[i]; int j=i; while(j>0 && odz[order[j-1]]<odz[t]){ order[j]=order[j-1]; j--; } order[j]=t; }
    for(int k=0;k<cnt && slot<126;k++){
        PObj *o=&po[order[k]]; s32 dz=odz[order[k]], dx=odx[order[k]];
        s32 zc=dz>>8;                                                                  // 8.8 cells
        s32 f=(FOCP<<16)/zc;                                                           // 8.8 px per cell
        int sx = 120 + (s32)(((((long long)dx*f)>>24)*fovq)>>8);                       // (16.16 cells * 8.8) >> 24 = px, spread to match the land's field of view
        s32 gh=ground_h(o->x,o->y); s32 curve=curv(zc) - tiny_sub();
        int sy = PHZ + (((cam_h + curve - gh) * f)>>16);                                // (8.8 cells * 8.8) >> 16 = px
#ifdef TEST_SHIPHIDE
        if(o->kind==O_SHIP){ char db[40]; scpy(db,"SHIP "); ncat(db,sx); scat(db," "); ncat(db,sy); scat(db," Z"); ncat(db,zc>>8); text(pg,4,70,db,C_GOLD,1); }
#endif
        if(sx<-40||sx>280||sy<10||sy>190) continue;
        int c=sx>>1; if(c<0) c=0; if(c>119) c=119;
        int si; { int lo=0, hi=NSTEP-1; while(lo<hi){ int mid=(lo+hi)>>1; if(stp_z[mid]<zc) lo=mid+1; else hi=mid; } si=lo; } int bin=(si>>3)-1; if(bin>7) bin=7;
        if(o->kind==O_SHIP){                                                           // your ship: the real 3D model, parked
            BG_PAL[236]=ship.paint; BG_PAL[237]=ship.trim;
            int sc=f/50; if(sc<2) sc=2;                                                   // 100 model units = 2 cells
#ifndef TEST_NOOCC
            if(!m7_on) sh_occ = bin>=0 ? ybin+bin : 0;                                    // a nearer hill or canyon wall hides it (and its shadow)
#endif
            if(!m7_on) ship_shadow(pg, sx, sy, sc);
            ship_draw(pg, sx, sy-(f>>9), sc, (1024-heading)&1023, cam_fp?30:70, 236, 237, 0,18,240,160);
            sh_occ=0; continue; }
        if(bin>=0 && sy > ybin[c*8+bin]+3) continue;                                   // its feet are behind a nearer hill
        static const u8 ksz[29]={28,28,22,18,16,30,24,22,18,30, 28,26,26,24,26,18,20,30,20,18,44,24,28, 12,14, 17,18,12,14};      // size of each kind in 16ths of a cell
        int s=(f>>6)*ksz[o->kind]>>4; if(s<8) continue; int m=(s-8)/4; if(m>27) m=27;       // px per cell / 16 px * 64. too far away to see: skip it (it used to be drawn too big)
        if(o->kind==O_KART){ slot=kart_draw_obj(slot,o,sx,sy,m,f); continue; }               // a rival kart (kart.inc)
        if(kr_mode) m&=~1;                                                              // in a race the odd scales belong to the rivals
        int pb = zc>(28<<8) ? 2 : zc>(18<<8) ? 1 : 0;
        int tl=tile_of[o->kind], dy2=0;
        if(o->kind==O_CRIT){ Species *sp=&spc[o->var]; tl = sp->body==0 ? T_HOPPER : sp->body==1 ? T_GRAZER : T_FLYER; pb=4+o->var;
            if(sp->body==2){ dy2 = (sinx((frames*6+order[k]*40)&255)>>13) - 10; } else if(sp->body==0 && (cvx[order[k]]||cvy[order[k]])) dy2 = -((frames>>2)&1)*3;
            if(sp->size==0 && m>2) m-=2; if(sp->size==2 && m<27) m+=2; if(WL(MW_GIANTS)){ m+=2+WL(MW_GIANTS)/3; if(m>27) m=27; }
            if(WL(MW_GHOSTS) && (((frames>>3)+order[k])%(2+(25-WL(MW_GHOSTS))/5))==0) continue; }                   // GHOSTS: creatures blink out
        if(WL(MW_JUNGLE) && (o->kind==O_PLANT || (o->kind>=O_TREE && o->kind<=O_FLOWER) || o->kind==O_TUFT)){ m+=WL(MW_JUNGLE)/3; if(m>27) m=27; }   // JUNGLE: giant plants
        if(o->kind==O_LOCAL) tl = o->var ? T_LOCAL2 : T_LOCAL1;
        s32 sc=8+m*4;                                                                  // the scale actually drawn: feet are placed from THIS (they used the pre-resize scale before)
        int w=w_of[o->kind], h=h_of[o->kind];
        int cy=sy - (sc*h)/128;
        int ox=sx-w, oy=cy-h+dy2;
        if(ox<-64||ox>240||oy<-64||oy>160) continue;
        shadow_oam[slot].a0 = (oy&255) | (1<<8)|(1<<9) | (shape_of[o->kind]<<14);
        shadow_oam[slot].a1 = (ox&511) | (m<<9) | (size_of[o->kind]<<14);
        shadow_oam[slot].a2 = tl | (pb<<12);
        slot++;
    }
#ifdef TEST_FPS
    { char fb[24]; scpy(fb,"SPR "); ncat(fb,(int)((TMR()-t_s0)/1000)); scat(fb,"K TXT "); ncat(fb,(int)((t_s0-t_r1)/1000)); scat(fb,"K"); rect(pg,0,48,240,8,0); text(pg,4,49,fb,C_GOLD,1); }
#endif
#ifdef TEST_FPS
    if(m7_on) m7_t[2]=TMR()-t_s0;
#endif
    if(m7_on) m7_present();
}
static void ship_shadow(volatile u16 *pg, int cx, int cy, int sc){      // a soft shadow under the parked ship: the ground under it, a few shades darker
    if(sc>140) sc=140;                                                  // (first person: the ship can stand right beside you; never paint a shadow wider than the screen)
    int rx=sc*5/12+2, ry=rx/3+1;
    for(int dy=-ry; dy<=ry; dy++){ int y=cy+dy; if(y<18||y>=160) continue;
        int w=rx*(int)isqrt((u32)(ry*ry-dy*dy))/ry;
        for(int x=cx-w; x<=cx+w; x++){ if(x<0||x>=240) continue; if(sh_occ && sh_occ[(x>>1)<<3]<=y) continue;
            volatile u16 *p=pg+y*120+(x>>1); u16 v=*p; int ix = (x&1) ? (v>>8) : (v&255);
            if(ix>=GND_B && ix<GND_B+16){ int dk = (x-cx>-w/2 && x-cx<w/2) ? 5 : 3; ix-=dk; if(ix<GND_B) ix=GND_B; *p = (x&1) ? (u16)((v&0x00FF)|(ix<<8)) : (u16)((v&0xFF00)|ix); } } }
}
static void planet_enter(void){
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); REG_BG2CNT=3; hdma_on=0; REG_BLDCNT=0;
    planet_tables(); planet_palette(); planet_hud(); page_back=0; p_last=vbl_count;
    volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i];
    bug_build();
    cam_h = ground_h(ppx,ppy)+CAM_UP;
    for(int p=0;p<2;p++){ volatile u16 *pg=(volatile u16*)(0x06000000 + p*0xA000); for(int i=18*120;i<160*120;i++) pg[i]=0; }
}
