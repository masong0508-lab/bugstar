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
enum { O_DOOR, O_SHIP, O_CAGE, O_SLUG, O_PLANT, O_TREE, O_ROCK, O_WEB, O_FLOWER, O_PEBBLE, O_TUFT };
#define O_LAST_USE O_FLOWER                                // kinds up to this one react to A
typedef struct { s32 x,y; u8 kind, alive, hp; } PObj;      // x,y: where on the map, 16.16 cells. hp: bites left in a resource node
#define NPO 100
static const u8 node_good[4]={3,4,2,1};                    // tree -> AMBER, rock -> CRYSTALS, web -> SILK, flower -> NECTAR (the station's goods)
static const char *const node_name[4]={"AMBER TREE","CRYSTAL ROCK","SILK WEB","NECTAR FLOWER"};
EW static PObj po[NPO];
EW static u8 hmap[TW*TW];                                  // 0..255: height of the ground, one per cell
EW static u8 hfine[HW*HW];                                 // the same, smoothed, two texels per cell: what the renderer walks
EW static u8 cmap[HW*HW];                                  // the colour of each texel, lit by a sun from the north-west
static s32 ppx, ppy;                                       // where the bug stands, 16.16 cells
static s32 cam_h;                                          // height of the camera above sea level, 8.8 cells
#define PHZ 66                                              // the eye line (horizon) row
static int water_lv=70;
static int facing=1, msg_timer=0, prompt_k=-2;
#define cur_tint zone
static char msg[40];
static int enter_dungeon_flag=0;
IWRAM_DATA static s16 raydx[120];                          // per column: sideways cells per cell of depth, 8.8 (before turning)
static int cam_fp=0, heading=0;                            // first person? and which way you face (0..1023, 0 = north = up the map)
static s32 cam_back(void){ return cam_fp ? (1<<16)/2 : CAM_BACK; }
static s32 cam_up(void){ return cam_fp ? (5<<8)/4 : CAM_UP; }
IWRAM_DATA static s16 rayvx[120], rayvy[120];              // per column: the ray direction on the map, 8.8 per cell of depth
static int hcs=256, hsn=0;                                 // cos and sin of the heading, 8.8
static void heading_rays(void){
    hcs=cosx(heading>>2)>>6; hsn=sinx(heading>>2)>>6;
    for(int c=0;c<120;c++){ rayvx[c]=(s16)(hsn + ((hcs*raydx[c])>>8)); rayvy[c]=(s16)(hcs - ((hsn*raydx[c])>>8)); }
}
static int pw_h=0, pw_v=0, pw_step=0, mine_t=0;
IWRAM_DATA static s32 stp_f[NSTEP];                        // per march step: pixels per cell at that distance, 8.8
IWRAM_DATA static s32 stp_base[NSTEP];                     // per step: (camera height + curvature) * stp_f, 8.8 px above the eye line... as a y offset
IWRAM_DATA static s32 stp_z[NSTEP];                        // per step: distance from the camera, 8.8 cells
IWRAM_DATA static u8 ybin[120*8];                          // per column: the terrain's top row after each eighth of the march (for hiding sprites behind hills)
IWRAM_DATA static u8 skyrow[160];
IWRAM_DATA static s8 gtex[256];                            // a little grass/soil texture, +-1 shade
static const u8 bay4[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};

static u16 lerp15(u16 a, u16 b, int t){ int r=((a&31)*(16-t)+(b&31)*t)>>4, g=(((a>>5)&31)*(16-t)+((b>>5)&31)*t)>>4, bl=(((a>>10)&31)*(16-t)+((b>>10)&31)*t)>>4; return r|(g<<5)|(bl<<10); }
static void planet_palette(void){
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    const u16 *tp = endless ? w_tint : &tint_pal[zone*6];
    for(int i=0;i<6;i++) BG_PAL[16+i]=tp[i];
    for(int i=0;i<16;i++){ int k=i*4; int a=k>>4, t=k&15; if(a>3){ a=3; t=16; } BG_PAL[GND_B+i]=lerp15(tp[a],tp[a+1],t); }    // 16 ground shades, dark to light
    for(int i=0;i<8;i++) BG_PAL[WAT_B+i]=bg_pal[R_WATER+i];
    u16 sky0=rgb15(4,4,14), sky1=tp[5];
    for(int i=0;i<8;i++) BG_PAL[SKY_B+i]=lerp15(sky0,sky1,i*2);
    obj_palette();
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
static s32 ground_h(s32 x, s32 y){ return hfine[((y>>15)&HMASK)*HW + ((x>>15)&HMASK)]*3; }   // height in 8.8 cells: 255 -> 3 cells
static void gen_terrain(u32 seed){
    wseed=seed;
    EW static s16 acc[TW*TW]; for(int i=0;i<TW*TW;i++) acc[i]=0;
    static const u8 cell[4]={32,16,8,4}; static const u8 amp[4]={110,70,40,18};
    EW static u8 grid[33*33];
    for(int o=0;o<4;o++){ int cs=cell[o], n=TW/cs;
        for(int i=0;i<n*n;i++) grid[i]=wr(256);
        for(int y=0;y<TW;y++) for(int x=0;x<TW;x++){
            int gx=x/cs, gy=y/cs, fx=(x%cs)*256/cs, fy=(y%cs)*256/cs;
            fx=(fx*fx*(3*256-2*fx))>>16; fy=(fy*fy*(3*256-2*fy))>>16;           // smoothstep
            int a=grid[gy*n+gx], b=grid[gy*n+((gx+1)%n)], c=grid[((gy+1)%n)*n+gx], d=grid[((gy+1)%n)*n+((gx+1)%n)];
            int top=a*(256-fx)+b*fx, bot=c*(256-fx)+d*fx; int v=(top*(256-fy)+bot*fy)>>16;
            acc[y*TW+x]+= (v-128)*amp[o]/128; } }
    int lo=32767, hi=-32768; for(int i=0;i<TW*TW;i++){ if(acc[i]<lo) lo=acc[i]; if(acc[i]>hi) hi=acc[i]; }
    for(int i=0;i<TW*TW;i++){ int v=(acc[i]-lo)*255/(hi-lo+1); v=(v*v)>>8; hmap[i]=v; }         // squared: wide valleys, round hills
    // a flat landing field at the start
    for(int y=-5;y<=5;y++) for(int x=-6;x<=6;x++){ int i=((64+y)&TMASK)*TW+((64+x)&TMASK); int d=x*x+y*y; int t = d<12 ? 0 : d<36 ? (d-12)*256/24 : 256; hmap[i]=(hmap[i]*t + 100*(256-t))>>8; }
    // smooth the map up to two texels per cell
    for(int y=0;y<HW;y++){ int y0=(y>>1)&TMASK, y1=(y0+1)&TMASK, fy=(y&1)?3:1;           // texel centres sit at 1/4 and 3/4 of a cell
        const u8 *r0=hmap+y0*TW, *r1=hmap+y1*TW; u8 *out=hfine+y*HW;
        for(int x=0;x<HW;x++){ int x0=(x>>1)&TMASK, x1=(x0+1)&TMASK, fx=(x&1)?3:1;
            int top=r0[x0]*(4-fx)+r0[x1]*fx, bot=r1[x0]*(4-fx)+r1[x1]*fx; out[x]=(top*(4-fy)+bot*fy)>>4; } }
    for(int y=0;y<HW;y++) for(int x=0;x<HW;x++){ int i=y*HW+x, h=hfine[i];
        if(h<water_lv){ int w=(h*6)/(water_lv+1); cmap[i]=WAT_B+1+(w>5?5:w); continue; }
        int hl=hfine[y*HW+((x-1)&HMASK)], hu=hfine[((y-1)&HMASK)*HW+x];
        int slope = (h-hl) + (h-hu);                                  // facing the sun (north-west) is brighter
        int s = 5 + (h-water_lv)*6/(255-water_lv+1) + slope/3 + ((bay4[((y&3)<<2)|(x&3)] + ((x*7+y*13)&7)) >> 3);
        if(h<water_lv+10 && ((x+y)&1)) s-=1;                            // a dark shoreline
        if(s<0) s=0; if(s>15) s=15; cmap[i]=GND_B+s; }
}
static void place(PObj *o, int kind, int minr, int maxr){ o->kind=kind; o->alive=1; o->hp=3; int a=wr(256), r=minr+wr(maxr-minr+1); o->x=(64<<16)+cosx(a)*r*4; o->y=(64<<16)+sinx(a)*r*4; }
static void new_planet(void){
    water_lv = endless ? 30+wr(60) : (zone==1 ? 100 : zone==0||zone==3 ? 70 : zone==5 ? 10 : 45);
    gen_terrain(endless ? (u32)world_no*2654435761u+7 : (u32)zone*977+13);
    for(int i=0;i<NPO;i++) po[i].alive=0;
    int k=0;
    po[k].kind=O_SHIP; po[k].alive=1; po[k].x=(64<<16)-(3<<16); po[k].y=(64<<16)+(1<<16); k++;
    po[k].kind=O_DOOR; po[k].alive=1; { int a=wr(256); po[k].x=(64<<16)+cosx(a)*22*4; po[k].y=(64<<16)+sinx(a)*22*4; } k++;
    for(int i=0;i<2;i++) place(&po[k++],O_CAGE,14,40);
    for(int i=0;i<3;i++) place(&po[k++],O_SLUG,10,30);
    for(int i=0;i<20;i++) place(&po[k++],O_PLANT,4,64);
    { int eco = endless ? sys[cur_sys].eco : zone%4;                                  // what grows here depends on the world's economy
      static const u8 mix[4][4]={{5,2,2,5},{2,7,2,3},{3,4,5,2},{2,3,6,3}};            // farming, mining, tech, hive
      for(int t=0;t<4;t++) for(int i=0;i<mix[eco][t];i++) place(&po[k++],O_TREE+t,6,64); }
    while(k<NPO){ place(&po[k],(k&1)?O_PEBBLE:O_TUFT,3,64); k++; }
    for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind!=O_SHIP && hm_at(po[i].x,po[i].y)<water_lv){ po[i].x+=1<<16; if(hm_at(po[i].x,po[i].y)<water_lv) po[i].alive = po[i].kind<=O_CAGE ? 1 : 0; }
    ppx=64<<16; ppy=64<<16; cam_h = ground_h(ppx,ppy)+CAM_UP; facing=1; prompt_k=-2;
#ifdef TEST_MINE
    po[3].kind=O_TREE; po[3].alive=1; po[3].hp=3; po[3].x=(64<<16)+(1<<16); po[3].y=(64<<16)-(1<<16); po[4].kind=O_ROCK; po[4].alive=1; po[4].hp=3; po[4].x=(64<<16)+(2<<16); po[4].y=(64<<16)+(1<<16);
#endif
}
static const u16 tile_of[]={T_DOOR,T_SHIP,T_CAGE,T_SLUG,T_PLANT,T_TREE,T_ROCK,T_WEB,T_FLOWER,T_PEBBLE,T_TUFT};
static const u8 shape_of[]={0,0,1,1,0,0,0,0,0,0,0}, size_of[]={1,1,0,0,0,1,1,1,1,0,0}, w_of[]={16,16,16,16,8,16,16,16,16,8,8}, h_of[]={16,16,8,8,8,16,16,16,16,8,8};
static s32 wrapd(s32 d){ return ((d + (64<<16)) & ((TW<<16)-1)) - (64<<16); }        // shortest way round the world, 16.16

// ---------------------------------------------------------------- the renderer
static void planet_tables(void){
    s32 z = cam_fp ? (3<<8)/4 : (9<<8)/2;                   // start 0.75 cells out in first person, 4.5 behind the bug otherwise
    for(int i=0;i<NSTEP;i++){ stp_z[i]=z; stp_f[i]=(FOCP<<16)/z; stp_base[i]=0; z += (cam_fp?24:40) + (i*i>>2); }   // steps grow with distance (8.8)
    for(int c=0;c<120;c++) raydx[c]=(s16)(((2*c-119)*180)/120);    // field of view: about 70 degrees, 8.8 cells sideways per cell of depth
    for(int y=0;y<160;y++){ int t = y<18 ? 0 : (y-18)*8/(PHZ+30-18); if(t>7) t=7; skyrow[y]=SKY_B+t; }
    { u32 r=0x2545F491; for(int i=0;i<256;i++){ r^=r<<13; r^=r>>17; r^=r<<5; int v=(r>>4)&7; gtex[i] = v==0?-1 : v==7?1 : 0; } }
    heading_rays();
}
IWRAM static void render_planet(volatile u16 *pg){
    s32 cb=cam_back(); s32 cam_x = ppx - ((hsn*cb)>>8), cam_y = ppy - ((hcs*cb)>>8);
    for(int i=0;i<NSTEP;i++){ s32 z=stp_z[i]; s32 curve = (z*z)>>(8+7);             // the world drops away: z*z/128 cells
        stp_base[i] = ((cam_h + curve) * stp_f[i])>>8; }                             // (8.8 cells) * (8.8 px/cell) >> 8 = 8.8 px
    u32 sparkle=frames;
    for(int c=0;c<120;c++){
        int ytop=160; s32 vx=rayvx[c], vy=rayvy[c]; int bin=0;
        volatile u16 *col=pg+c;
        for(int i=0;i<NSTEP;i++){
            s32 z=stp_z[i];
            s32 mx = cam_x + vx*z, my = cam_y + vy*z;
            int ti = ((my>>15)&HMASK)*HW + ((mx>>15)&HMASK);                             // texel: half a cell
            int h = hfine[ti];
            s32 hh = h*3;                                                              // 8.8 cells
            int y = PHZ + ((stp_base[i] - ((hh*stp_f[i])>>8))>>8);
            if(y<ytop){
                int ix = cmap[ti];
                if(i<NEAR_STEPS && ix>=GND_B+1 && ix<GND_B+15) ix += gtex[(((my>>11)&15)<<4)|((mx>>11)&15)];    // grass texture up close
                if(ix<GND_B && ((((c+sparkle)*7+i*3)&31)==0)) ix=WAT_B+7;              // sparkles on the water
                u16 p2=ix|(ix<<8);
                int y0 = y<18 ? 18 : y;
                for(int yy=y0; yy<ytop; yy++) col[yy*120]=p2;
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
static void planet_hud(void){
    phud_sig=hud_sig(); char b[24]; scpy(b,world_name()); if(zone_cleared && !endless){ scat(b," T"); ncat(b,tiers[zone]+1); }
    for(int p=0;p<2;p++) draw_hud((volatile u16*)(0x06000000 + p*0xA000), b);
}
static void show_msg(const char *s){ int n=0; while(s[n] && n<39){ msg[n]=s[n]; n++; } msg[n]=0; msg_timer = n ? 150 : 0; }

// ---------------------------------------------------------------- one frame on the surface
static int p_last=0;
static void planet_frame(void){
    u32 t_f0=TMR();
    volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
    int n = vbl_count - p_last; p_last=vbl_count; if(n<1) n=1; if(n>4) n=4;           // the view takes a few 60ths to draw: move by that much
    int moved=0;
    int th = HELD(K_LEFT) ? -16 : HELD(K_RIGHT) ? 16 : 0, tv = HELD(K_UP) ? -16 : HELD(K_DOWN) ? 16 : 0;
    if(HELD(K_SEL)){ th=tv=0; if(HIT(K_L)||HIT(K_R)){ sel_used=1; arrow_ship=(arrow_ship+1)%3; show_msg(arrow_ship==1?"ARROW: TO YOUR SHIP":arrow_ship==2?"ARROW: TO THE NEAREST RESOURCE":"ARROW: TO THE DOOR"); sfx(7); } }
    if(cam_fp){                                                                         // first person: left/right turn, L/R step sideways
        int turn = th; th = HELD(K_SEL) ? 0 : HELD(K_R) ? 16 : HELD(K_L) ? -16 : 0;
        if(turn){ heading=(heading+turn*n*5/8)&1023; heading_rays(); }
    } else { if(HELD(K_LEFT)) facing=0; if(HELD(K_RIGHT)) facing=1; }
    for(int k=0;k<n;k++){
        pw_h += th ? (th-pw_h)/4 + (th>pw_h?1:th<pw_h?-1:0) : -pw_h/3; pw_v += tv ? (tv-pw_v)/4 + (tv>pw_v?1:tv<pw_v?-1:0) : -pw_v/3;
        s32 sp = 0x1600;                                                                // cells per 60th, 16.16: about 5 cells a second
        if(hm_at(ppx,ppy)<water_lv) sp=sp*2/3;                                          // wading is slow
        s32 mvx=(pw_h*sp)>>4, mvy=(pw_v*sp)>>4;
        if(cam_fp){ s32 fwd=-mvy, side=mvx; mvx=((hsn*fwd)>>8)+((hcs*side)>>8); mvy=((hcs*fwd)>>8)-((hsn*side)>>8); }   // forward is where you face
        s32 nx = ppx + mvx, ny = ppy + mvy;
        // climbing a steep slope is slow
        s32 h0=ground_h(ppx,ppy), h1=ground_h(nx,ny); s32 dh=h1-h0;
        if(dh>0x0060){ nx=ppx+((nx-ppx)>>1); ny=ppy+((ny-ppy)>>1); }
        ppx = nx & ((TW<<16)-1); ppy = ny & ((TW<<16)-1);
        if(pw_h||pw_v) moved=1;
        s32 want = ground_h(ppx,ppy)+cam_up(); cam_h += (want-cam_h)>>3;                // the camera eases up and down the hills
    }
    if(moved && (pw_step+=n)>=14){ pw_step=0; sfx(hm_at(ppx,ppy)<water_lv?8:11); }
    // slugs crawl toward you
    for(int i=0;i<NPO;i++) if(po[i].kind==O_SLUG && po[i].alive){ PObj *o=&po[i]; s32 dx=wrapd(ppx-o->x), dy=wrapd(ppy-o->y);
        s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy);
        if(ad<(14<<16)){ o->x=(o->x+((dx>>7)*n))&((TW<<16)-1); o->y=(o->y+((dy>>7)*n))&((TW<<16)-1); }
        if(ad<0x14000 && !invuln){ hurt(3+dlv()*2); if(invuln>20) show_msg("OUCH! A SLUG"); pw_h = dx<0 ? 40 : -40; pw_v = dy<0 ? 24 : -24; } }
    // what is next to you
    int best=-1; s32 bd=0x1A000;
    for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind<=O_LAST_USE){ s32 dx=wrapd(po[i].x-ppx), dy=wrapd(po[i].y-ppy); s32 d=(dx<0?-dx:dx)+(dy<0?-dy:dy); if(d<bd){ bd=d; best=i; } }
    if(HIT(K_A)){
        if(best<0) show_msg("NOTHING HERE");
        else switch(po[best].kind){
            case O_DOOR: enter_dungeon_flag=1; sfx(5); break;
            case O_SHIP:
                { hdma_on=0; int r=shop_menu();
                  planet_hud(); if(!r){ save_game(); show_msg(opt_autosave?"GAME SAVED":""); p_last=vbl_count; return; }
                  save_game(); launch_flag=r; return; }
            case O_CAGE: po[best].alive=0; free_bug(rndi(8)<2?BUG_PILL:rndi(6)); break;
            case O_SLUG: po[best].alive=0; gold+=5; show_msg("SQUASH! +5 GOLD"); gain_xp(3+dlv()); sfx(9); break;
            case O_TREE: case O_ROCK: case O_WEB: case O_FLOWER: {                     // mining: a few bites, then the goods go in your ship's hold
                PObj *o=&po[best]; int t=o->kind-O_TREE; sfx(o->kind==O_ROCK?12:13); mine_t=8;
                if(--o->hp>0){ char b[40]; scpy(b,node_name[t]); scat(b,": "); ncat(b,o->hp); scat(b," MORE"); show_msg(b); break; }
                o->alive=0; int g=node_good[t], n=1+rndi(2)+(rndi(4)==0);
                if(cargo_n()>=cargo_cap){ show_msg("HOLD FULL! SELL GOODS AT A STATION"); break; }
                int room=cargo_cap-cargo_n(); if(n>room) n=room; cargo[g]+=n; st_mined+=n; ach_check(); gain_xp(2+dlv());
                char b[40]; scpy(b,"+"); ncat(b,n); scat(b," "); scat(b,goods[g]); scat(b,"  HOLD "); ncat(b,cargo_n()); scat(b,"/"); ncat(b,cargo_cap); show_msg(b); sfx(7);
                break; }
            case O_PLANT: { po[best].alive=0; int r=rndi(10);
                if(r<5){ eat(15); show_msg("NIBBLED A LEAF. FOOD +15"); sfx(11); }
                else if(r<8){ Gear g={IT_BERRY,(u8)rndi(8),0,0,0,0}; if(add_gear(g)) show_msg("FOUND A BERRY IN THE LEAVES"); }
                else if(r<9){ Gear g={IT_SEED,(u8)rndi(6),0,0,0,0}; if(add_gear(g)) show_msg("FOUND A SEED"); }
                else { gold+=2+dlv(); show_msg("A SHINY PEBBLE! +GOLD"); sfx(7); }
                break; }
        }
    }
    if(frames%600==0){ for(int i=0;i<NPO;i++) if(po[i].kind==O_PLANT && !po[i].alive){ place(&po[i],O_PLANT,4,64); break; } }
    bag_input(); tick_timers(); hunger_tick(240); regen_tick(120);
    if(HIT(K_START)){ oam_hide_all(); oam_flush(); char_screen(VISPG); planet_hud(); msg_timer=0; p_last=vbl_count; return; }
    if(cast_flag>=0){ cam_fp^=1; planet_tables(); show_msg(cam_fp?"FIRST PERSON. SELECT: SWITCH VIEW":"THIRD PERSON. SELECT: SWITCH VIEW"); sfx(2); }
    escape_flag=call_flag=blast_flag=0; cast_flag=-1;
    if(hearts<=0){ hearts=maxh; food=60; gold/=2; if(!opt_easy){ bag_n=0; bag_sel=0; } show_msg(opt_easy?"YOU FAINTED. HALF YOUR GOLD IS GONE":"YOU FAINTED. YOU LOST YOUR BAG"); planet_hud(); }
    if(msg_timer){ msg_timer-=n; if(msg_timer<0) msg_timer=0; }
    if(frames%30==0 && hud_sig()!=phud_sig) planet_hud();
    // ---- draw the world
    u32 t_r0=TMR(); render_planet(pg); u32 t_r1=TMR();
    if(msg_timer){ int L=textlen(msg); if(L<=29) text(pg,120-L*4,24,msg,C_WHITE,2); else stext(pg,120-L*2,26,msg,C_WHITE); }
#ifdef TEST_FPS
    { static u32 tf0=0; u32 now=TMR(); char fb[40]; scpy(fb,"N"); ncat(fb,n); scat(fb," RENDER "); ncat(fb,(int)((t_r1-t_r0)/1000)); scat(fb,"K PRE "); ncat(fb,(int)((t_r0-t_f0)/1000)); scat(fb,"K FRAME "); ncat(fb,(int)((now-tf0)/1000)); scat(fb,"K"); tf0=now; rect(pg,0,40,240,8,0); text(pg,4,41,fb,C_GOLD,1); }
#endif
    { int pk = best<0 ? -1 : po[best].kind;
      static const char *const pt[9]={"A: GO DOWN THE DOOR","A: YOUR SHIP","A: OPEN THE CAGE","A: SQUASH THE SLUG","A: SEARCH THE PLANT","A: TAP THE AMBER TREE","A: MINE THE CRYSTAL","A: GATHER THE SILK","A: SIP THE NECTAR"};
      if(pk>=0 && opt_hints){ const char *s=pt[pk]; stext(pg,120-textlen(s)*2,msg_timer?38:26,s,C_GOLD); } }
    (void)prompt_k;
#ifdef TEST_FPS
    u32 t_s0=TMR();
#endif
    // ---- sprites: project everything onto the terrain
    s32 cam_y = ppy - ((hcs*cam_back())>>8), cam_x = ppx - ((hsn*cam_back())>>8);
    for(int m=0;m<30;m++){ int s=13+m*4; int inv=16384/s; set_affine(m, inv, 0, 0, inv); }      // scales 0.2x .. 2x
    oam_hide_all();
    int slot=0;
    s32 bug_sy;
    { s32 dz=cam_back(); s32 f=(FOCP<<16)/(dz>>8); s32 gh=ground_h(ppx,ppy); s32 curve=((dz>>8)*(dz>>8))>>15;
      bug_sy = PHZ + (((cam_h + curve - gh) * f)>>16); }
    if(!cam_fp){                                                                        // the bug, standing on the ground ahead of the camera
      int sy=bug_sy;
      int bob = moved ? ((frames>>3)&1) : 0;
      int pvis = !(invuln && (frames&4));
      if(mine_t) mine_t--;
      int lean = moved ? 10 : (mine_t ? 24 : 0), sq = (moved && ((frames>>3)&1)) || mine_t ? 8 : 0;
      int up = tv<0 && !th;
      set_affine(31, up ? 256 : facing? -128:128, up ? 0 : facing? lean:-lean, 0, up ? 256 : 128+sq);
      if(pvis){ shadow_oam[slot].a0 = ((up ? sy-96-bob : sy-32-bob)&255) | (1<<8)|(1<<9) | (up?(0<<14):(1<<14));
                shadow_oam[slot].a1 = ((up ? 120-64 : 120-32)&511) | (31<<9) | (up?(3<<14):(2<<14));
                shadow_oam[slot].a2 = up ? T_BACK : bug_tile(moved && ((frames>>3)&1)); slot++; }
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
    { PObj *t=&po[arrow_ship==1?0:1];
      if(arrow_ship==2){ s32 bd2=0x7FFFFFFF; for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind>=O_TREE && po[i].kind<=O_FLOWER){ s32 ddx=wrapd(po[i].x-ppx), ddy=wrapd(po[i].y-ppy); s32 d=(ddx<0?-ddx:ddx)+(ddy<0?-ddy:ddy); if(d<bd2){ bd2=d; t=&po[i]; } } }
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
    EW static s32 odx[NPO];
    for(int i=0;i<NPO;i++){ PObj *o=&po[i]; if(!o->alive) continue; s32 wx=wrapd(o->x-cam_x), wy=wrapd(o->y-cam_y);
        s32 dz=((wx*hsn)>>8)+((wy*hcs)>>8), dx=((wx*hcs)>>8)-((wy*hsn)>>8);
        if(dz<near_z || dz>(40<<16)) continue; odz[i]=dz; odx[i]=dx; order[cnt++]=i; }
    for(int i=1;i<cnt;i++){ u8 t=order[i]; int j=i; while(j>0 && odz[order[j-1]]<odz[t]){ order[j]=order[j-1]; j--; } order[j]=t; }
    for(int k=0;k<cnt && slot<126;k++){
        PObj *o=&po[order[k]]; s32 dz=odz[order[k]], dx=odx[order[k]];
        s32 zc=dz>>8;                                                                  // 8.8 cells
        s32 f=(FOCP<<16)/zc;                                                           // 8.8 px per cell
        int sx = 120 + (s32)(((long long)dx*f)>>24);                                   // (16.16 cells * 8.8) >> 24 = px
        s32 gh=ground_h(o->x,o->y); s32 curve=(zc*zc)>>15;
        int sy = PHZ + (((cam_h + curve - gh) * f)>>16);                                // (8.8 cells * 8.8) >> 16 = px
        if(sx<-40||sx>280||sy<10||sy>190) continue;
        int c=sx>>1; if(c<0) c=0; if(c>119) c=119;
        int si=0; while(si<NSTEP-1 && stp_z[si]<zc) si++; int bin=(si>>3)-1; if(bin>7) bin=7;
        if(bin>=0 && sy > ybin[c*8+bin]+3) continue;                                   // its feet are behind a nearer hill
        static const u8 ksz[11]={28,28,22,18,16,30,24,22,18,12,14};                     // size of each kind in 16ths of a cell
        int s=(f>>6)*ksz[o->kind]>>4; if(s<13) s=13; int m=(s-13)/4; if(m>27) m=27;                      // px per cell / 16 px * 64             // sprite scale: a 16px sprite is one cell (matrices 28-31 belong to the jaws, arrow and bug)
        s32 sc=13+m*4;
        int w=w_of[o->kind], h=h_of[o->kind];
        int cy=sy - (sc*h)/128;
        int ox=sx-w, oy=cy-h;
        if(ox<-64||ox>240||oy<-64||oy>160) continue;
        int pb = zc>(28<<8) ? 2 : zc>(18<<8) ? 1 : 0;
        shadow_oam[slot].a0 = (oy&255) | (1<<8)|(1<<9) | (shape_of[o->kind]<<14);
        shadow_oam[slot].a1 = (ox&511) | (m<<9) | (size_of[o->kind]<<14);
        shadow_oam[slot].a2 = tile_of[o->kind] | (pb<<12);
        slot++;
    }
#ifdef TEST_FPS
    { char fb[24]; scpy(fb,"SPR "); ncat(fb,(int)((TMR()-t_s0)/1000)); scat(fb,"K TXT "); ncat(fb,(int)((t_s0-t_r1)/1000)); scat(fb,"K"); rect(pg,0,48,240,8,0); text(pg,4,49,fb,C_GOLD,1); }
#endif
}
static void planet_enter(void){
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6); REG_BG2CNT=3; hdma_on=0; REG_BLDCNT=0;
    planet_tables(); planet_palette(); planet_hud(); page_back=0; p_last=vbl_count;
    volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i];
    cam_h = ground_h(ppx,ppy)+CAM_UP;
    for(int p=0;p<2;p++){ volatile u16 *pg=(volatile u16*)(0x06000000 + p*0xA000); for(int i=18*120;i<160*120;i++) pg[i]=0; }
}
