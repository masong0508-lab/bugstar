// ================================================================= THE DUNGEON
#define MW 32
enum { D_FLOOR, D_WALL, D_PILLAR, D_UP, D_DOWN, D_LOCK };     // D_LOCK: a locked door, solid until you bring the keycard
enum { M_BRICK, M_STONE, M_EARTH, M_MOSS, M_CRYSTAL, M_MARBLE };
#define F_TORCH 0x10
#define F_BEAM  0x20
enum { FL_FLAG, FL_TILE, FL_DIRT, FL_MOSS, FL_WATER, FL_PLANK, FL_SHALLOW };
enum { ST_HALL, ST_CRYPT, ST_CAVE, ST_GROTTO, ST_CRYSTAL, NSTYLE };
static const u8 style_wall[NSTYLE]={M_STONE,M_STONE,M_EARTH,M_MOSS,M_CRYSTAL};
static const u8 style_floor[NSTYLE]={FL_FLAG,FL_TILE,FL_DIRT,FL_MOSS,FL_TILE};
static u8 dmap[MW*MW];
EW static u8 seen[MW*MW]; static u8 room_seen[10];     // what you have seen on this floor (for the map)
EW static u8 wmat[MW*MW], fmat[MW*MW], rid[MW*MW], wdec[MW*MW];
static u16 lampcell[12]; static int nlamps;
typedef struct { s32 x,y; s16 hp, maxhp; s8 kind, alive, flash; u8 sub, it, rar, pre, suf, lvl, champ, cname; s16 cool, venom; s32 kvx, kvy; u8 stag, lunge, wind, ph; } DObj;     // wind: ticks of wind-up before it bites. ph: boss phase
enum { K_SPIDER, K_CHEST, K_ITEM, K_CAGE, K_BOSS, K_BAT, K_LAMP, K_BEETLE, K_WASP, K_PET, K_BARREL, K_CORE, K_SLIME, K_SCORP, K_GHOST, K_TOAD, K_ANT, K_CENTI, K_GOLEM, K_DRONE, K_MOTH, K_SHROOM, K_CRAB, K_WISP };   // K_MOTH: flutters fast, fragile, comes in packs. K_SHROOM: slow, spits spore clouds. K_CRAB: armoured, slow, hits very hard. K_WISP: floats, fires quick bolts, fragile. // K_SLIME: splits in two when it dies (sub=1: a small one). K_SCORP: fast, hits hard. K_GHOST: floats, erratic, fragile. K_TOAD: spits slow heavy blobs. // K_CORE: an outpost's power core: smash it
static const char *const champ_pre[8]={"GRIM","BLOODY","GIANT","SWIFT","ANCIENT","VILE","FROST","ROTTEN"};
static const char *const kind_name[24]={"SPIDER","","","","QUEEN","BAT","","BEETLE","WASP","","BARREL","CORE","SLIME","SCORPION","GHOST","TOAD","ANT","CENTIPEDE","GOLEM","DRONE","MOTH","SHROOM","CRAB","WISP"};
static int is_mon(int k){ return k==K_SPIDER||k==K_BOSS||k==K_BAT||k==K_BEETLE||k==K_WASP||k==K_CORE||k==K_SLIME||k==K_SCORP||k==K_GHOST||k==K_TOAD||k==K_ANT||k==K_CENTI||k==K_GOLEM||k==K_DRONE||k==K_MOTH||k==K_SHROOM||k==K_CRAB||k==K_WISP; }
enum { DC_NONE, DC_PANEL, DC_NICHE, DC_DOOR, DC_RUNE, DC_PLAQUE };
#define NDO 160                                   // objects on a floor (was 64): room for crowds on the deep floors
static s32 tgt_x, tgt_y, up_x, up_y; static int lantk=58;
EW static DObj dob[NDO]; static int ndo;
static s32 px_, py_; static int pang;
static s32 zbuf[120];
// TERRACE CAVERNS: per-cell floor heights (eighths of a cell), low rail blocks, a per-pixel depth buffer for sprites
#define TR_CEIL 24
static int terr_on=0, terr_snap=0; static s32 t_eye=0x8000;
EW static u8 trh[MW*MW], trb[MW*MW], zpix[120*80];
static s32 eyeat(s32 x,s32 y){ if(!terr_on) return 0x8000; int cx=x>>16, cy=y>>16; if(cx<0||cy<0||cx>=MW||cy>=MW) return 0x8000; return t_eye-(s32)trh[cy*MW+cx]*0x2000; }
static int terr_roll(int bossfl,int mz_last,int mboss); static void terr_build(void);
static int hurt_flash=0, atk_t=0;
typedef struct { u8 x,y,w,h,style; } Room;
EW static Room rooms[10]; static int nrooms;
// a natural pond: a wobbly ellipse (sometimes with a second lobe), smoothed, then deep core, shallow rim and a muddy bank
EW static u8 pm_[26*26];
static int pm_at(int w,int h,int x,int y){ return (x<0||y<0||x>=w||y>=h) ? 0 : pm_[y*26+x]; }
static u32 pm_h(int x,int y,u32 s){ u32 h=(u32)x*374761393u + (u32)y*668265263u + s; h=(h^(h>>13))*1274126177u; return h^(h>>16); }
static int ponds_made, pond_floor, pond_cells;   // ponds are rare and small: never on the first floor, never in the room you arrive in, never more than ~40 cells of water on a floor
static int want_pond(int first_pct){      // may this room get a pond? the first one: first_pct%; a second one, on a floor that has a pond: 25%; never a third
    if(!pond_floor || ponds_made>=2 || pond_cells>=40 || depth_no<2 || nrooms==0) return 0;   // nrooms==0: this is the room with the way up. depth_no<2: the first floor is dry
    return rndi(100) < (ponds_made==0 ? first_pct : 15); }
static void make_pond(int x0,int y0,int x1,int y1){
    if(x1-x0>23) x1=x0+23; if(y1-y0>23) y1=y0+23;
    int w=x1-x0+1, h=y1-y0+1; if(w<3||h<3) return;
    u32 seed=rnd(); int lx=(rndi(3)-1)*130, ly=(rndi(3)-1)*130, lr=70+rndi(60), big=rndi(4)==0;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){
        int nx=((2*x+1-w)*256)/w, ny=((2*y+1-h)*256)/h, e=(nx*nx+ny*ny)>>8;
        int n=(int)(((pm_h(x>>1,y>>1,seed)&63)+(pm_h(x,y,seed+7)&63))>>1) - 32;
        int in = e + n*4 < 95;                       // a pond fills about a quarter of its patch, with dry ground all round
        if(big){ int ax=nx-lx, ay=ny-ly; if(((ax*ax+ay*ay)>>8) + n*3 < lr/2+10) in=1; }
        pm_[y*26+x]=in; }
    EW static u8 t2[26*26];
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int c=0; for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) if(dx||dy) c+=pm_at(w,h,x+dx,y+dy);
        t2[y*26+x] = pm_[y*26+x] ? (c>=3) : (c>=6); }
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) pm_[y*26+x]=t2[y*26+x];
    int cnt=0; for(int y=0;y<h;y++) for(int x=0;x<w;x++) cnt+=pm_[y*26+x]; if(cnt<5) return;
    for(int y=-1;y<=h;y++) for(int x=-1;x<=w;x++){
        int mx=x0+x, my=y0+y; if(mx<1||my<1||mx>=MW-1||my>=MW-1) continue; int i=my*MW+mx; if(dmap[i]!=D_FLOOR) continue;
        if(pm_at(w,h,x,y)){ int edge = !pm_at(w,h,x-1,y)||!pm_at(w,h,x+1,y)||!pm_at(w,h,x,y-1)||!pm_at(w,h,x,y+1); fmat[i]= edge ? FL_SHALLOW : FL_WATER; pond_cells++; }
        else { int near=0; for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++) near|=pm_at(w,h,x+dx,y+dy);
               if(near && fmat[i]!=FL_WATER && fmat[i]!=FL_SHALLOW) fmat[i] = (pm_h(mx,my,seed)&3)==0 ? FL_MOSS : FL_DIRT; } }
}
typedef struct { u8 x,y,str; } Light;
EW static Light lights[64]; static int nlights;
EW static s32 torch_x[32], torch_y[32]; static int ntorch;
static void build_light(void); static void build_walls(void); static int base_floors(void);
// ---- dungeon DNA: every endless world (and every Dungeon Forge design) has a layout, wall material, creature mix, density and hazard
typedef struct { u8 on, layout, wall, mix, dens, haz, floors, lvl, boss; u16 seed; } DDna;
enum { LY_ROOMS, LY_HALLS, LY_CAVERNS, LY_TEMPLE, LY_CATA, LY_MAZE, NLY };
static DDna ddna; static int forge_on;
static int moon_tier(void){ return 1+(depth_no>0?depth_no-1:0)/5; }                   // MOON DUNGEONS: a new tier every 5 floors, the guardian waits at the bottom of each one                       // forge_on: a Dungeon Forge run is in progress
static const u8 mx_w[4][2]={{8,1},{5,2},{3,6},{2,2}};        // creature mix: spider and beetle weights out of 10, the rest are wasps
static const u8 lay_wall[NLY]={7,7,7,M_MARBLE,M_STONE,7};    // a layout's own wall when the walls are on AUTO
static int mboss=0, hunt_t=0, flow_cell=-1; EW static u16 flow[MW*MW];   // mboss: this boss floor is the rare labyrinth. hunt_t: ticks before the boss wakes. flow: path distance to you

static void add_light(int x,int y,int s){ if(nlights<64){ lights[nlights].x=x; lights[nlights].y=y; lights[nlights].str=s; nlights++; } }
static int floorish(int i){ int t=dmap[i]; return t==D_FLOOR||t==D_UP||t==D_DOWN; }
static void cor_cell(int x,int y,int fm){
    if(x<1||y<1||x>=MW-1||y>=MW-1) return; int i=y*MW+x;
    if(dmap[i]==D_WALL){ fmat[i]=fm; rid[i]=255; }
    if(dmap[i]!=D_UP && dmap[i]!=D_DOWN) dmap[i]=D_FLOOR;
}
static void corridor(Room *a, Room *b, int wide, int fm){
    int ax=a->x+a->w/2, ay=a->y+a->h/2, bx=b->x+b->w/2, by=b->y+b->h/2;
    int hx = (rnd()&1);
    if(hx){ for(int x=(ax<bx?ax:bx); x<=(ax<bx?bx:ax); x++){ cor_cell(x,ay,fm); for(int o=1;o<=wide;o++) cor_cell(x,ay+o,fm); }
            for(int y=(ay<by?ay:by); y<=(ay<by?by:ay); y++){ cor_cell(bx,y,fm); for(int o=1;o<=wide;o++) cor_cell(bx+o,y,fm); } }
    else  { for(int y=(ay<by?ay:by); y<=(ay<by?by:ay); y++){ cor_cell(ax,y,fm); for(int o=1;o<=wide;o++) cor_cell(ax+o,y,fm); }
            for(int x=(ax<bx?ax:bx); x<=(ax<bx?bx:ax); x++){ cor_cell(x,by,fm); for(int o=1;o<=wide;o++) cor_cell(x,by+o,fm); } }
}
static void pillar_at(int x,int y,int m){ if(x<1||y<1||x>=MW-1||y>=MW-1) return; int i=y*MW+x; dmap[i]=D_PILLAR; wmat[i]=m; }
static int room_floor_cell(Room *r){
    for(int t=0;t<40;t++){ int x=r->x+1+rndi(r->w-2), y=r->y+1+rndi(r->h-2); if(dmap[y*MW+x]==D_FLOOR && fmat[y*MW+x]!=FL_WATER && fmat[y*MW+x]!=FL_SHALLOW) return y*MW+x; }   // nothing is placed in the water: stairs, monsters, chests all stand on dry ground
    return (r->y+r->h/2)*MW + r->x+r->w/2;
}

static void build_labyrinth(void){                      // a real maze: one-wide torch-lit passages, loops, and five open chambers
    for(int i=0;i<MW*MW;i++){ dmap[i]=D_WALL; wmat[i]=M_STONE; fmat[i]=FL_FLAG; rid[i]=254; wdec[i]=0; }
    nlights=0; nlamps=0; ntorch=0;
    EW static u8 stk[225], vis[225]; for(int i=0;i<225;i++) vis[i]=0;
    int sp=0; stk[sp++]=0; vis[0]=1; dmap[1*MW+1]=D_FLOOR; rid[1*MW+1]=255;
    while(sp){ int cur=stk[sp-1], cx=cur%15, cy=cur/15, nb[4], nn=0;                       // depth-first: always one way in, many dead ends
        if(cx>0 && !vis[cur-1]) nb[nn++]=cur-1; if(cx<14 && !vis[cur+1]) nb[nn++]=cur+1; if(cy>0 && !vis[cur-15]) nb[nn++]=cur-15; if(cy<14 && !vis[cur+15]) nb[nn++]=cur+15;
        if(!nn){ sp--; continue; }
        int n=nb[rndi(nn)], nx=n%15, ny=n/15; vis[n]=1; stk[sp++]=n;
        int mx=1+2*nx, my=1+2*ny, bx=1+2*cx+(nx-cx), by=1+2*cy+(ny-cy);
        dmap[my*MW+mx]=D_FLOOR; rid[my*MW+mx]=255; dmap[by*MW+bx]=D_FLOOR; rid[by*MW+bx]=255; }
    for(int k=0;k<40;k++){ int x=2+rndi(27), y=2+rndi(27); if(((x^y)&1)==0) continue; int i=y*MW+x; if(dmap[i]!=D_WALL) continue;   // knock out a few walls: loops
        if((dmap[i-1]==D_FLOOR && dmap[i+1]==D_FLOOR) || (dmap[i-MW]==D_FLOOR && dmap[i+MW]==D_FLOOR)){ dmap[i]=D_FLOOR; rid[i]=255; } }
    static const u8 cxy[5][2]={{1,1},{25,25},{25,1},{1,25},{13,13}};                       // room 0 holds the way up, room 1 is the far corner
    for(int k=0;k<5;k++){ Room *rm=&rooms[k]; rm->x=cxy[k][0]; rm->y=cxy[k][1]; rm->w=5; rm->h=5; rm->style=ST_CRYPT;
        for(int yy=rm->y;yy<rm->y+5;yy++) for(int xx=rm->x;xx<rm->x+5;xx++){ int i=yy*MW+xx; dmap[i]=D_FLOOR; fmat[i]=FL_TILE; rid[i]=k; }
        add_light(rm->x+2,rm->y+2, mboss ? (k==4?64:24) : 55); }
    pillar_at(15,15,M_MARBLE);                                                             // a monument in the middle
    if(mboss){ pillar_at(13,13,M_MARBLE); pillar_at(17,13,M_MARBLE); pillar_at(13,17,M_MARBLE); pillar_at(17,17,M_MARBLE); }   // the boss's arena
    nrooms=5; if(mboss) say("A DARK LABYRINTH. SOMETHING IS HERE"); else say("A LABYRINTH! FOLLOW THE TORCHES"); msg_timer=70;
}
static int pick_kind0(void){                                                      // a monster for this dungeon's creature mix
    int r=rndi(10);
    if(!ddna.on) return r<5 ? K_SPIDER : (r<7 && (dlv()>=1||depth_no>=2)) ? K_BEETLE : (r<9 && (dlv()>=2||depth_no>=3)) ? K_WASP : K_SPIDER;
    const u8 *w=mx_w[ddna.mix&3];
    if(r<w[0]) return K_SPIDER;
    if(r<w[0]+w[1]) return (dlv()>=1||depth_no>=2) ? K_BEETLE : K_SPIDER;
    return (dlv()>=2||depth_no>=3) ? K_WASP : K_SPIDER;
}
static int pick_kind(void){                                                      // the old mix, plus a few new creatures from the early floors on
    int k=pick_kind0(), x=rndi(100);
    if(base_mode && x<30) return K_DRONE;                                          // outposts: the ants' drones
    if(x>=90 && (dlv()>=1||depth_no>=2)) return K_ANT;
    if(x>=84 && x<90) return K_CENTI;
    if(x>=80 && x<84 && (dlv()>=3||depth_no>=5)) return K_GOLEM;
    if(x<8) return K_SLIME;
    if(x<15 && (dlv()>=1||depth_no>=2)) return K_SCORP;
    if(x<21 && (dlv()>=2||depth_no>=3)) return K_TOAD;
    if(x<26 && (dlv()>=3||depth_no>=4)) return K_GHOST;
    if(x>=26 && x<31 && (dlv()>=1||depth_no>=2)) return K_MOTH;
    if(x>=31 && x<36 && (dlv()>=2||depth_no>=3)) return K_SHROOM;
    if(x>=36 && x<40 && (dlv()>=2||depth_no>=3)) return K_CRAB;
    if(x>=40 && x<44 && (dlv()>=4||depth_no>=6)) return K_WISP;
    return k;
}
static int kind_hp(int k, int b){
    switch(k){ case K_BEETLE: return b*5/2; case K_WASP: return b*4/5; case K_SLIME: return b*3/2; case K_SCORP: return b*6/5; case K_GHOST: return b*3/5; case K_TOAD: return b*9/10; case K_ANT: return b*9/10; case K_CENTI: return b/2+1; case K_GOLEM: return b*4; case K_DRONE: return b*7/10; case K_MOTH: return b/2+1; case K_SHROOM: return b; case K_CRAB: return b*3; case K_WISP: return b/2+1; }
    return b;
}
static void make_floor(void){
    for(int i=0;i<MW*MW;i++){ dmap[i]=D_WALL; wmat[i]=M_BRICK; fmat[i]=FL_FLAG; rid[i]=254; wdec[i]=0; }
    nrooms=0; nlights=0; nlamps=0; ntorch=0; ponds_made=0; pond_cells=0; pond_floor = rndi(100) < (zone==1 ? 45 : 20);   // about 1 floor in 5 has a pond (the Pond zone: nearly every other floor)
    if(forge_on) rng_s=hmix(((u32)ddna.seed<<8)+(u32)depth_no+1u);                 // a forge design is the same dungeon every time
    else { ddna.on=endless; if(endless){ u32 h=hmix(wd.tseed*0x9E3779B1u ^ wd.cseed ^ 0xD9A5u); int lr=h%10, hz=(h>>20)%7;
            ddna.layout = lr<3?LY_ROOMS : lr<4?LY_HALLS : lr<6?LY_CAVERNS : lr<8?LY_TEMPLE : LY_CATA;
            ddna.wall=(h>>8)%8; ddna.mix=(h>>12)&3; ddna.dens=(h>>16)%3; ddna.haz = hz<3?0 : hz-2; ddna.boss=0;
            if(hellw()){ static const u8 hw[6]={0,2,4,5,0,4}; ddna.wall=hw[wd.cls-PC_INFERNO]; ddna.haz = wd.cls==PC_OBSIDIAN ? 1 : 2; ddna.dens=2; ddna.mix = wd.cls==PC_INFERNO||wd.cls==PC_BONE ? 3 : 2; } } }
    if(ddna.on && !forge_on && cur_moon){ ddna.layout = wd.cls==PC_ICE ? LY_CATA : LY_CAVERNS; ddna.wall = wd.cls==PC_ICE ? M_MARBLE : wd.cls==PC_DESERT ? M_EARTH : M_CRYSTAL; ddna.mix=3; ddna.dens=2; ddna.haz=1; }   // a moon dungeon: ice catacombs or lunar caverns, glittering walls, swarms of wasps, dark floors with double loot
    int mz_last = forge_on ? depth_no>=ddna.floors : base_mode ? depth_no>=base_floors() : (!endless && depth_no>=zone_floors());
    int bossfl = !base_mode && (forge_on ? mz_last : endless ? (depth_no%(cur_moon?5:10)==0 && !hive_boss_floor()) : mz_last);
    mboss=0; hunt_t=0; flow_cell=-1;
    if(bossfl){ if(forge_on) mboss = ddna.boss==1 || (ddna.boss==0 && rndi(100)<8);
                else if(endless || zone>0 || tier_now()) mboss = rndi(100) < 10+tier_now()*4; }      // RARE: a boss floor that is a labyrinth, and the boss hunts you through it
    int maze = mboss || (ddna.on && ddna.layout==LY_MAZE) || (!base_mode && !mz_last && depth_no>=2 && (rndi(100)<22 || (!endless && depth_no==zone_floors()-1 && (zone&1)==0)));   // a labyrinth floor, here and there
    terr_on = terr_roll(bossfl,mz_last,mboss); if(terr_on) maze=0;           // a very rare two-level terraced cavern
    for(int i=0;i<MW*MW;i++) seen[i]=0; for(int i=0;i<10;i++) room_seen[i]=0;
    int dd=depth_no<12?depth_no:12, maxr=7-dd/3, vm=7-dd/3; if(maxr<4) maxr=4; if(vm<4) vm=4; if(base_mode) maxr=6;
    if(ddna.on){ if(ddna.layout==LY_CATA) maxr=9; else if(ddna.layout==LY_HALLS) maxr=4; else if(ddna.layout==LY_CAVERNS) maxr=5; }
    for(int t=0;t<400 && nrooms<maxr && !terr_on;t++){
        static const u8 zst[6][3]={{ST_HALL,ST_CAVE,ST_GROTTO},{ST_GROTTO,ST_GROTTO,ST_CRYPT},{ST_HALL,ST_CRYPT,ST_HALL},{ST_CRYSTAL,ST_CRYSTAL,ST_CAVE},{ST_CRYSTAL,ST_HALL,ST_CRYPT},{ST_CAVE,ST_CAVE,ST_CRYPT}};
        static const u8 bst[3]={ST_CRYSTAL,ST_CRYSTAL,ST_CRYPT};
        const u8 *zs = base_mode ? bst : endless ? w_styles : zst[zone];
        int st = nrooms==0 ? zs[0] : (rndi(10)<7 ? zs[rndi(3)] : rndi(NSTYLE)), w, h;
        if(ddna.on){ static const u8 lst[NLY]={255,ST_HALL,ST_CAVE,ST_GROTTO,ST_CRYPT,255}; if(lst[ddna.layout]!=255) st=lst[ddna.layout]; }
        int sh=t/80;                                              // if big rooms will not fit, shrink a little every 80 tries
        if(st==ST_HALL){ w=11+rndi(5)+dd/4; h=9+rndi(3); if(rnd()&1){ int k=w; w=h; h=k; } }
        else { w=9+rndi(5)+dd/3; h=8+rndi(4)+dd/4; }
        if(ddna.on && ddna.layout==LY_CATA){ w=6+rndi(3); h=6+rndi(3); }
        w-=sh; h-=sh; if(w<6) w=6; if(h<6) h=6; if(w>20) w=20; if(h>20) h=20;
        if(w>MW-4) w=MW-4; if(h>MW-4) h=MW-4;
        int x=2+rndi(MW-4-w), y=2+rndi(MW-4-h), ok=1;
        for(int i=0;i<nrooms;i++){ Room*r=&rooms[i]; if(x<r->x+r->w+2 && r->x<x+w+2 && y<r->y+r->h+2 && r->y<y+h+2) ok=0; }
        if(!ok) continue;
        Room*r=&rooms[nrooms]; r->x=x; r->y=y; r->w=w; r->h=h; r->style=st;
        for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++){ int i=yy*MW+xx; dmap[i]=D_FLOOR; fmat[i]=style_floor[st]; rid[i]=nrooms; }
        if(st==ST_CAVE||st==ST_GROTTO||st==ST_CRYSTAL){          // ragged, natural edges
            for(int yy=y-1;yy<=y+h;yy++) for(int xx=x-1;xx<=x+w;xx++){
                int ring = xx==x-1||yy==y-1||xx==x+w||yy==y+h;
                if(ring && xx>0&&yy>0&&xx<MW-1&&yy<MW-1 && rndi(3)==0){ int i=yy*MW+xx; dmap[i]=D_FLOOR; fmat[i]=style_floor[st]; rid[i]=nrooms; }
            }
            int cx[4]={x,x+w-1,x,x+w-1}, cy[4]={y,y,y+h-1,y+h-1};
            for(int k=0;k<4;k++) if(rnd()&1) dmap[cy[k]*MW+cx[k]]=D_WALL;
        }
        if(st==ST_HALL){                                          // a colonnade down the long side
            if(w>=h){ for(int xx=x+2; xx<=x+w-3; xx+=3){ pillar_at(xx,y+2,M_MARBLE); pillar_at(xx,y+h-3,M_MARBLE); } }
            else    { for(int yy=y+2; yy<=y+h-3; yy+=3){ pillar_at(x+2,yy,M_MARBLE); pillar_at(x+w-3,yy,M_MARBLE); } }
        }
        if(st==ST_CRYPT){ for(int xx=x+2; xx<x+w-2; xx+=4) for(int yy=y+2; yy<y+h-2; yy+=4) pillar_at(xx,yy,M_STONE); }
        if(st==ST_GROTTO){ if(want_pond(100)){ make_pond(x+2,y+2,x+w-3,y+h-3); ponds_made++; } add_light(x+w/2,y+h/2,45); }   // a grotto has a pond more often than not, never more than two on a floor
        if(st==ST_CRYSTAL){ for(int k=0;k<2;k++){ pillar_at(x+2+rndi(w-4), y+2+rndi(h-4), M_CRYSTAL); } add_light(x+w/2,y+h/2,40); }
        if(st==ST_CAVE) add_light(x+w/2,y+h/2,28);
        if(w*h>=100){ if((st==ST_HALL||st==ST_CRYPT) && want_pond(40)){ make_pond(x+w/3-1,y+h/3-1,x+w-w/3,y+h-h/3); ponds_made++; }   // a still pool in the middle of a big chamber
            add_light(x+w/4,y+h/4,50); add_light(x+w-1-w/4,y+h-1-h/4,50); }
        if(st==ST_HALL||st==ST_CRYPT){
            for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++){ int i=yy*MW+xx; if(dmap[i]==D_FLOOR && fmat[i]!=FL_WATER && fmat[i]!=FL_SHALLOW && fmat[i]!=FL_DIRT){ int q=((xx>>1)*29+(yy>>1)*41+depth_no*5)%9; if(q==0) fmat[i]=FL_MOSS; else if(q==1 && depth_no>2) fmat[i]=FL_DIRT; } }   // overgrown, broken floor
            for(int k=0;k<(w*h>=100?3:1);k++) pillar_at(x+2+rndi(w-4), y+2+rndi(h-4), M_STONE); }                                                                   // a broken column
        nrooms++;
    }
    for(int i=1;i<nrooms;i++) corridor(&rooms[i-1],&rooms[i], 2, (i&1)?FL_FLAG:FL_PLANK);
    if(nrooms>3) corridor(&rooms[0],&rooms[nrooms-1], 2, FL_FLAG);     // a loop, so it is not one long line
    if(maze) build_labyrinth();
    if(terr_on) terr_build();
    // walls take the look of the room they face; decorate the faces
    for(int y=1;y<MW-1;y++) for(int x=1;x<MW-1;x++){
        int i=y*MW+x; if(dmap[i]!=D_WALL) continue;
        int nb[4]={i-1,i+1,i-MW,i+MW}, f=-1;
        for(int k=0;k<4;k++) if(floorish(nb[k])){ f=nb[k]; if(rid[nb[k]]<nrooms) break; }
        if(f<0) continue;
        int r=rid[f], st = r<nrooms ? rooms[r].style : -1;
        wmat[i] = st>=0 ? style_wall[st] : M_STONE;
        int hsh=(x*7+y*13)&7;
        if(st==ST_HALL||st==ST_CRYPT||st<0){
            int h2=(x*31+y*17+depth_no*7+dlv()*3)%13;
            wdec[i] = h2<2?DC_NICHE : h2<3?DC_DOOR : h2<4?DC_PLAQUE : h2<6?DC_RUNE : (st==ST_CRYPT && h2<11)?DC_PANEL : DC_NONE;
        }
        if((st==ST_HALL||st==ST_CRYPT) && hsh<2){ wmat[i]|=F_TORCH; add_light(f%MW,f/MW,72); if(ntorch<32){ torch_x[ntorch]=(x<<16)+0x8000+((f%MW-x)*0x8C00); torch_y[ntorch]=(y<<16)+0x8000+((f/MW-y)*0x8C00); ntorch++; } if(wdec[i]!=DC_PANEL) wdec[i]=DC_NONE; }
        else if(st<0 && ((x*5+y*3)%9)==0){ wmat[i]|=F_TORCH; add_light(f%MW,f/MW,60); if(ntorch<32){ torch_x[ntorch]=(x<<16)+0x8000+((f%MW-x)*0x8C00); torch_y[ntorch]=(y<<16)+0x8000+((f/MW-y)*0x8C00); ntorch++; } }
        else if(st==ST_CAVE && ((x+y)%3)==0) wmat[i]|=F_BEAM;
        else if(st==ST_CRYSTAL && hsh==3) add_light(f%MW,f/MW,34);
        if((st==ST_HALL||st==ST_CRYPT||st<0) && !(wmat[i]&0x30) && ((x*13+y*7)%vm)==0) wmat[i]=M_MOSS;      // vines and moss, thicker the deeper you go
    }
    if(ddna.on){ int ow = ddna.wall<6 ? ddna.wall : lay_wall[ddna.layout]; if(ow<6) for(int i=0;i<MW*MW;i++) if(dmap[i]==D_WALL) wmat[i]=(wmat[i]&0x30)|ow; }   // the DNA's wall material
    for(int i=0;i<nrooms && nlamps<12;i++) if(rooms[i].style==ST_HALL||rooms[i].style==ST_CRYPT){
        int c=1+(rooms[i].w*rooms[i].h>50);
        for(int k=0;k<c && nlamps<12;k++){ int cell=room_floor_cell(&rooms[i]); lampcell[nlamps++]=cell; add_light(cell%MW,cell/MW,60); } }
    // stairs
    Room *r0=&rooms[0]; int far=0, fd=-1;
    for(int i=1;i<nrooms;i++){ int d=(rooms[i].x-r0->x)*(rooms[i].x-r0->x)+(rooms[i].y-r0->y)*(rooms[i].y-r0->y); if(d>fd){ fd=d; far=i; } }
    int up=room_floor_cell(r0), dn=room_floor_cell(&rooms[far]);
    dmap[up]=D_UP; dmap[dn]=D_DOWN; add_light(up%MW,up/MW,40); add_light(dn%MW,dn/MW,40);
    build_light(); build_walls();
    // player on the up stairs, facing the next room
    px_=((up%MW)<<16)+0x8000; py_=((up/MW)<<16)+0x8000;
    { int best=0, bl=-1; static const s8 ddx[4]={1,0,-1,0}, ddy[4]={0,1,0,-1};
      for(int k=0;k<4;k++){ int x=up%MW, y=up/MW, l=0; while(l<20){ x+=ddx[k]; y+=ddy[k]; if(x<0||y<0||x>=MW||y>=MW) break; int t=dmap[y*MW+x]; if(t==D_WALL||t==D_PILLAR) break; l++; } if(l>bl){ bl=l; best=k; } }
      pang = best*256; }
    // monsters, treasure, items, a caged bug or two; the last floor of the zone has a boss instead of stairs down
    int last = mz_last;
    if(last){ dmap[dn]=D_FLOOR; }
    if(endless && depth_no>1 && !(forge_on && last)) dmap[up]=D_FLOOR;          // no way back up: warp out or keep going
    tgt_x=((dn%MW)<<16)+0x8000; tgt_y=((dn/MW)<<16)+0x8000;
    up_x=((up%MW)<<16)+0x8000; up_y=((up/MW)<<16)+0x8000;
    ndo=0;
    #define SPAWN(k_,cell_,hp_,sub_) do{ if(ndo<NDO){ DObj *o=&dob[ndo++]; memset(o,0,sizeof(DObj)); int c_=(cell_); o->kind=(k_); o->alive=1; o->hp=o->maxhp=xo_hp(hp_); o->sub=(sub_); o->x=((c_%MW)<<16)+0x8000; o->y=((c_/MW)<<16)+0x8000; } }while(0)
    int base_hp = 6 + dlv()*5 + depth_no*2; if(endless && cur_moon) base_hp = base_hp*(100+moon_tier()*10)/100;      // every moon tier: +10% monster health
    int bcell = mboss ? 14*MW+15 : dn;                                  // the labyrinth boss waits in the middle arena
    int ilvl = dlv()*3 + depth_no;
    for(int i=1;i<nrooms;i++){
        int c = 1 + (rooms[i].w*rooms[i].h>60) + (rooms[i].w*rooms[i].h>110) + (rooms[i].w*rooms[i].h>160) + ((dlv()+depth_no)/4 > 3 ? 3 : (dlv()+depth_no)/4);
        if(ddna.on){ c = c*(ddna.dens==0?2:ddna.dens==1?3:5)/3; if(c<1) c=1; }
        { int dm=dlv()+depth_no, ex=dm/5; if(ex>6) ex=6; c+=ex;                           // deeper floors: more monsters in every room
          if(!base_mode && (dlv()>=22 || depth_no>=40)) c=c*2+8;                         // the deepest floors: packed
          if(c>24) c=24; }
        c=xo_n(c); for(int k=0;k<c && ndo<NDO-28;k++){                                           // (28 slots stay free for chests, loot, bats, lamps and the boss)
            int kind = pick_kind();
            int hp = kind_hp(kind, base_hp);
            SPAWN(kind, room_floor_cell(&rooms[i]), hp, 0);
            if(rndi(100)<8){ DObj *o=&dob[ndo-1]; o->champ=1; o->cname=rndi(8); o->hp=o->maxhp=hp*3; }
            if(kind==K_ANT || kind==K_CENTI || kind==K_MOTH){ for(int p=0;p<2 && ndo<NDO-28;p++) SPAWN(kind, room_floor_cell(&rooms[i]), hp, 0); }   // ants and centipedes come in packs of three
        }
        if(rnd()&1) SPAWN(K_CHEST, room_floor_cell(&rooms[i]), 1, 0);
    }
    for(int i=0;i<nrooms;i++){ int c=rndi(3); for(int k=0;k<c;k++){ int cr=rndi(3)==0; SPAWN(K_BARREL, room_floor_cell(&rooms[i]), cr?3:1, cr); } }   // barrels and crates to shove and smash
    if(maze){ int made=0;                                                                  // the maze passages are full of things: wanderers and chests in the dead ends
        for(int t2=0;t2<400 && made<(mboss?7:14+dd/2);t2++){ int x=1+rndi(MW-2), y=1+rndi(MW-2), c2=y*MW+x; if(dmap[c2]!=D_FLOOR || rid[c2]!=255) continue;
            int dx2=x-(up%MW), dy2=y-(up/MW); if(dx2*dx2+dy2*dy2<36) continue;
            if(made<4) SPAWN(K_CHEST, c2, 1, 0);
            else { int kind = pick_kind(); SPAWN(kind, c2, kind_hp(kind, base_hp), 0); }
            made++; } }
    #define SPAWN_GEAR(cell_,g_) do{ Gear gg_=(g_); SPAWN(K_ITEM,(cell_),1,gg_.sub); DObj *o_=&dob[ndo-1]; o_->it=gg_.it; o_->rar=gg_.rar; o_->pre=gg_.pre; o_->suf=gg_.suf; o_->lvl=gg_.lvl; }while(0)
    for(int i=0;i<nrooms;i++){ int c = 1 + rndi(2); for(int k=0;k<c;k++) SPAWN_GEAR(room_floor_cell(&rooms[i]), roll_drop(ilvl,0)); }
    int cages = 1 + (nrooms>3);
    for(int k=0;k<cages;k++){ int r=1+rndi(nrooms>1?nrooms-1:1); SPAWN(K_CAGE, room_floor_cell(&rooms[r<nrooms?r:0]), 1, rndi(8)<2?BUG_PILL:rndi(6)); }
    int nb = 1 + (dlv()/2>3?3:dlv()/2) + (depth_no>2);
    for(int k=0;k<nb;k++) SPAWN(K_BAT, room_floor_cell(&rooms[nrooms>1 ? 1+rndi(nrooms-1) : 0]), base_hp/2+1, 0);
    for(int k=0;k<nlamps;k++) SPAWN(K_LAMP, lampcell[k], 1, 0);
    if(last && !base_mode){ SPAWN(K_BOSS, bcell, 40+dlv()*30 + tier_now()*40, 0); }       // the boss is always at the bottom: beat it again for the next tier
    if(last && !base_mode && !endless && !forge_on && (opt_diff>=3 || xo_v(XO_BOSSES))){                      // MULTIBOSS: higher difficulties send more bosses down deep. Insane-sect: 11+ on its deepest tiers
        int ex = opt_diff==5 ? (tier_now()*10+zone*8)/40 : opt_diff==4 ? (tier_now()+1)/2 : tier_now()/4; if(opt_diff==5 && ex<1) ex=1; if(opt_diff<3) ex=0; ex+=xo_v(XO_BOSSES); if(ex>15) ex=15;
        for(int k=0;k<ex && ndo<NDO-8;k++){ SPAWN(K_BOSS, room_floor_cell(&rooms[nrooms>1 ? 1+rndi(nrooms-1) : 0]), (40+dlv()*30+tier_now()*40)*6/10+1, 0); if(k%3==2) dob[ndo-1].champ=1; } }
    if(base_mode){                                                                       // an outpost floor: a locked door guards the way on, the keycard is somewhere else
        if(last){ SPAWN(K_CORE, dn, 80+dlv()*25+outposts_done*20, 0); }
        // lock: the corridor cell just outside the far room
        int lk=-1; Room *fr=&rooms[far];
        for(int y=fr->y-1;y<=fr->y+fr->h && lk<0;y++) for(int x=fr->x-1;x<=fr->x+fr->w && lk<0;x++){
            if(x<1||y<1||x>=MW-1||y>=MW-1) continue; int ring = x==fr->x-1||y==fr->y-1||x==fr->x+fr->w||y==fr->y+fr->h; if(!ring) continue;
            int i=y*MW+x; if(dmap[i]==D_FLOOR && rid[i]==255) lk=i; }
        if(lk>=0){ dmap[lk]=D_LOCK; int kr=rndi(nrooms>1?nrooms-1:1); if(kr==far) kr=0; SPAWN_GEAR(room_floor_cell(&rooms[kr]), ((Gear){IT_KEY,0,0,0,0,0})); }
        for(int k=0;k<2;k++) SPAWN(rndi(2)?K_WASP:K_BEETLE, room_floor_cell(&rooms[rndi(nrooms)]), base_hp*2, 0);
    }
    if(endless){
        if(hive_boss_floor()) SPAWN(K_BOSS, dn, 150+depth_no*12+dlv()*45, 0);              // a Tech Hive overlord guards the hive core
        else if(depth_no%(cur_moon?5:10)==0) SPAWN(K_BOSS, bcell, (60+depth_no*6+dlv()*20)*(cur_moon ? 100+moon_tier()*25 : 100)/100, 0);   // a moon guardian: +25% health per tier               // a guardian every 10 floors
        if(depth_no%5==0 || rndi(10)<3) SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), ((Gear){IT_WARP,0,0,0,0,0}));
        int ng = 1 + rndi(2) + depth_no/8 + (cur_moon?1:0); if(ng>4+(cur_moon?1:0)) ng=4+(cur_moon?1:0);                 // moons are crystal rock: one more gem a floor
        for(int k=0;k<ng;k++){ int t=rndi(10)+depth_no/6; Gear g={IT_GEM,(u8)(t<5?0 : t<8?1 : t<11?2 : 3),0,0,0,0}; SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), g); }
        if(depth_no>=5 && rndi(8)==0){ Gear g=roll_gear(rndi(2)?IT_SHELL:IT_THORN, ilvl+10, 60); g.sub=4; SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), g); }
    }
    if(mboss){ for(int i=0;i<ndo;i++) if(dob[i].kind==K_BOSS) dob[i].hp=dob[i].maxhp=dob[i].maxhp*5/4;
        hunt_t=150; tgt_x=((bcell%MW)<<16)+0x8000; tgt_y=((bcell/MW)<<16)+0x8000; SPAWN_GEAR(room_floor_cell(&rooms[1]), roll_drop(ilvl+8,30)); }          // it sleeps for a moment. the far chamber holds the prize
    // ---- a floor event (never on the first garden floor, a boss floor or in an outpost)
    fl_ev=FE_NONE; fl_need=fl_have=0; fl_bonus=100; fl_done=0;
#ifdef TEST_FE
    if(1){
#else
    if(!last && !base_mode && !mboss && !(zone==0 && !endless && depth_no==1 && !zone_cleared) && (rndi(100)<65 || (ddna.on && ddna.haz))){
#endif
        fl_ev=1+rndi(NFE-1);
        if(ddna.on && ddna.haz && rndi(100)<70){ static const u8 he[5]={0,FE_DARK,FE_CURSED,FE_NEST,FE_AMBUSH}; fl_ev=he[ddna.haz%5]; }   // the dungeon's hazard
#ifdef TEST_FE
        fl_ev=TEST_FE;
#endif
        switch(fl_ev){
            case FE_VAULT: for(int k=0;k<4;k++){ int r=rndi(nrooms); SPAWN(K_CHEST, room_floor_cell(&rooms[r]), 1, 0); SPAWN(K_SPIDER, room_floor_cell(&rooms[r]), base_hp, 0); } break;
            case FE_NEST: for(int i=1;i<nrooms;i++) for(int k=0;k<2;k++) SPAWN(rndi(3)?K_SPIDER:K_WASP, room_floor_cell(&rooms[i]), base_hp, 0); fl_bonus=150; break;
            case FE_ELITE: for(int k=0;k<2;k++){ int kind=rndi(2)?K_BEETLE:K_SPIDER; SPAWN(kind, room_floor_cell(&rooms[1+rndi(nrooms>1?nrooms-1:1)]), base_hp*4, 0); DObj *o=&dob[ndo-1]; o->champ=1; o->cname=rndi(8); } fl_need=2; break;
            case FE_DARK: fl_bonus=200; break;
            case FE_SPEED: fl_end=vbl_count+60*60; break;
            case FE_CURSED: for(int i=0;i<ndo;i++) if(is_mon(dob[i].kind) && dob[i].kind!=K_BOSS){ dob[i].hp=dob[i].maxhp=dob[i].maxhp*3/2; } fl_bonus=175;
                for(int k=0;k<2;k++) SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), roll_drop(ilvl+6,0)); break;
            case FE_RUSH: for(int k=0;k<8;k++) SPAWN(K_CHEST, room_floor_cell(&rooms[rndi(nrooms)]), 1, 0); break;
            case FE_BLESSED: break;
            case FE_AMBUSH: for(int k=0;k<4;k++) SPAWN(rndi(2)?K_BEETLE:K_WASP, room_floor_cell(&rooms[far]), base_hp*3/2, 0); fl_bonus=130; break;
            case FE_QUIET: for(int i=0;i<ndo;i++) if(is_mon(dob[i].kind) && dob[i].kind!=K_BOSS && dob[i].kind!=K_CORE) dob[i].alive=0;
                SPAWN(K_CAGE, room_floor_cell(&rooms[rndi(nrooms)]), 1, rndi(6)); SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), roll_drop(ilvl,0)); break;
        }
    }
    if(endless && cur_moon && !mboss){ char mb[40]; scpy(mb,"MOON TIER "); ncat(mb,moon_tier()); scat(mb,". GUARDIAN ON B"); ncat(mb,moon_tier()*5); say(mb); msg_timer=70; }
    // quests that live on this floor
    int qhere = !forge_on && q_type>0 && !q_done && depth_no==q_floor && q_zone==zone && q_world==world_no;
    if(qhere && q_type==Q_CHAMP){ SPAWN(rndi(2)?K_SPIDER:K_BEETLE, room_floor_cell(&rooms[far]), base_hp*5, 0); dob[ndo-1].champ=2; }
    if(qhere && q_type==Q_FETCH) SPAWN_GEAR(room_floor_cell(&rooms[far]), ((Gear){IT_QUEST,0,0,0,0,0}));
    if(!forge_on && q_type==Q_SHARDS && !q_done && q_have<q_need && q_zone==zone && q_world==world_no && depth_no>q_floor-q_need) SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), ((Gear){IT_QUEST,0,0,0,0,0}));   // one star shard per floor near the quest depth
    if(forge_on) rng_s ^= vbl_count*2654435761u;                                          // the floor is fixed by the seed; the fight is not
}
static int solid(s32 x, s32 y){ int cx=x>>16, cy=y>>16; if(cx<0||cy<0||cx>=MW||cy>=MW) return 1; int t=dmap[cy*MW+cx]; return t==D_WALL||t==D_PILLAR||t==D_LOCK; }
static void hunt_flow(void){                                  // path distance from you to every cell (4-way flood), so the boss can follow the corridors
    EW static u16 q[MW*MW]; static const s8 ddx[4]={1,-1,0,0}, ddy[4]={0,0,1,-1};
    for(int i=0;i<MW*MW;i++) flow[i]=0xFFFF;
    int pc=(py_>>16)*MW+(px_>>16); if(pc<0||pc>=MW*MW) return;
    int h=0, t=0; q[t++]=pc; flow[pc]=0;
    while(h<t){ int c=q[h++], x=c%MW, y=c/MW;
        for(int k=0;k<4;k++){ int nx=x+ddx[k], ny=y+ddy[k]; if(nx<0||ny<0||nx>=MW||ny>=MW) continue; int n=ny*MW+nx, tt=dmap[n];
            if(flow[n]!=0xFFFF || tt==D_WALL || tt==D_PILLAR || tt==D_LOCK) continue; flow[n]=flow[c]+1; q[t++]=n; } }
}
static void hunt_move(DObj *o, s32 sp){                      // one step along the flow field toward you
    int pc=(py_>>16)*MW+(px_>>16); if(pc!=flow_cell){ flow_cell=pc; hunt_flow(); }
    int cx=o->x>>16, cy=o->y>>16, c=cy*MW+cx; s32 tx=px_, ty=py_;
    if(cx>=0&&cy>=0&&cx<MW&&cy<MW && flow[c]!=0xFFFF && flow[c]>0){ int best=flow[c]; static const s8 ddx[4]={1,-1,0,0}, ddy[4]={0,0,1,-1};
        for(int k=0;k<4;k++){ int nx=cx+ddx[k], ny=cy+ddy[k]; if(nx<0||ny<0||nx>=MW||ny>=MW) continue; int n=ny*MW+nx; if(flow[n]<best){ best=flow[n]; tx=((s32)nx<<16)+0x8000; ty=((s32)ny<<16)+0x8000; } } }
    s32 dx=tx-o->x, dy=ty-o->y; if(dx>sp) dx=sp; else if(dx<-sp) dx=-sp; if(dy>sp) dy=sp; else if(dy<-sp) dy=-sp;
    o->x+=dx; o->y+=dy;
}

static u8 bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
static int hz=89;          // the horizon row; it bobs while you walk and shakes when you are hit
#define FOC 182            // focal length in pixel rows
static s32 rd_rel[100], cd_rel[100];                  // floor / ceiling distance by rows from the horizon
static u8 rl_rel[100], rf_rel[100], cl_rel[100], cf_rel[100];
static s32 rdx[120], rdy[120];
static u8 ctyp[120], clit[120], ctex[120], ctu[120]; static u16 ccell[120]; static s16 ctop[120], cbot[120];
static u8 wbase[120];               // each column's wall ramp base: a reflection only mirrors real wall pixels, never stray ones
static u8 lvt[160];                 // light level 0..127 plus dither -> shade 0..7
static u8 bayi[16];
static s32 camtab[120];
static u8 fcode[MW*MW], wtex[MW*MW];
static u16 wramp[20*8], framp[10*8], cramp[2*8];
static s16 sinI[256];
static u8 lsub[128*128] __attribute__((section(".ewram")));     // smooth light, 4x4 per tile

// ---------------------------------------------------------------- baking the light
static int opaque(int tx,int ty){ if(tx<0||ty<0||tx>=MW||ty>=MW) return 1; int t=dmap[ty*MW+tx]; return t==D_WALL||t==D_PILLAR||t==D_LOCK; }
static void build_light(void){
    for(int i=0;i<128*128;i++) lsub[i]=16;
    for(int k=0;k<nlights;k++){ Light *L=&lights[k]; int lx=L->x*4+2, ly=L->y*4+2, st=L->str;
        for(int dy=-22;dy<=22;dy++){ int sy=ly+dy; if(sy<0||sy>=128) continue;
            for(int dx=-22;dx<=22;dx++){ int sx=lx+dx; if(sx<0||sx>=128) continue;
                int d2=dx*dx+dy*dy; if(d2>=484) continue;
                if(opaque(sx>>2,sy>>2)) continue;
                if(opaque((lx+dx/4)>>2,(ly+dy/4)>>2) || opaque((lx+dx/2)>>2,(ly+dy/2)>>2) || opaque((lx+dx*3/4)>>2,(ly+dy*3/4)>>2)) continue;
                int f=484-d2, add=st*f/484*f/484;
                int v=lsub[sy*128+sx]+add; lsub[sy*128+sx]= v>120?120:v; } } }
    for(int sy=0;sy<128;sy++) for(int sx=0;sx<128;sx++){      // soft shadows where floor meets wall
        int tx=sx>>2, ty=sy>>2; if(opaque(tx,ty)) continue;
        int ax=sx&3, ay=sy&3, f=64;
        if(opaque(tx-1,ty)) f = f*(ax==0?40:ax==1?54:64)/64;
        if(opaque(tx+1,ty)) f = f*(ax==3?40:ax==2?54:64)/64;
        if(opaque(tx,ty-1)) f = f*(ay==0?40:ay==1?54:64)/64;
        if(opaque(tx,ty+1)) f = f*(ay==3?40:ay==2?54:64)/64;
        lsub[sy*128+sx] = lsub[sy*128+sx]*f>>6;
    }
}
static void build_walls(void){
    for(int i=0;i<MW*MW;i++){
        int t=dmap[i], m=wmat[i]&15, fl=wmat[i]&0x30, dc=wdec[i], st=(m==M_STONE), w=WT_BRICK;
        if(t==D_LOCK) w = WT_STONE_NICHE+1;                                             // the locked door looks like a door
        else if(t==D_PILLAR) w = m==M_STONE?WT_STONE_PILLAR : m==M_CRYSTAL?WT_CRYSTAL_PILLAR : WT_MARBLE;
        else if(fl&F_TORCH) w = st?WT_STONE_TORCH:WT_BRICK_TORCH;
        else if(m==M_EARTH) w = (fl&F_BEAM)?WT_EARTH_BEAM:WT_EARTH;
        else if(m==M_MOSS) w=WT_MOSS;
        else if(m==M_CRYSTAL) w=WT_CRYSTAL;
        else if(dc==DC_PANEL) w = st?WT_STONE_PANEL:WT_BRICK;
        else if(dc>=DC_NICHE) w = (st?WT_STONE_NICHE:WT_BRICK_NICHE) + (dc-DC_NICHE);
        else w = st?WT_STONE:WT_BRICK;
        wtex[i]=w;
    }
}

u32 prof[8];
IWRAM static void render_view(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    u32 t0=TMR();
    const int H=hz;
    // pass 1: one ray per column
    for(int c=0;c<120;c++){
        s32 cam=camtab[c];
        s32 rx=dirx+(s32)(((long long)plx*cam)>>16), ry=diry+(s32)(((long long)ply*cam)>>16);
        rdx[c]=rx; rdy[c]=ry;
        int mx=px_>>16, my=py_>>16, pmx=mx, pmy=my;
        u32 arx=rx<0?-rx:rx, ary=ry<0?-ry:ry;
        u32 qx = arx>2? 0xFFFFFFFFu/arx : 0x7fffffff; if(qx>0x7fffffff) qx=0x7fffffff;
        u32 qy = ary>2? 0xFFFFFFFFu/ary : 0x7fffffff; if(qy>0x7fffffff) qy=0x7fffffff;
        s32 ddx=(s32)qx, ddy=(s32)qy;
        int sx = rx<0?-1:1, sy = ry<0?-1:1;
        s32 fx = px_&0xFFFF, fy = py_&0xFFFF;
        s32 tx = rx<0 ? (s32)(((long long)fx*ddx)>>16) : (s32)(((long long)(0x10000-fx)*ddx)>>16);
        s32 ty = ry<0 ? (s32)(((long long)fy*ddy)>>16) : (s32)(((long long)(0x10000-fy)*ddy)>>16);
        int side=0, t=D_WALL;
        for(int k=0;k<64;k++){
            pmx=mx; pmy=my;
            if(tx<ty){ tx+=ddx; mx+=sx; side=0; } else { ty+=ddy; my+=sy; side=1; }
            if(mx<0||my<0||mx>=MW||my>=MW){ t=D_WALL; mx=pmx; my=pmy; break; }
            t=dmap[my*MW+mx]; if(t==D_WALL||t==D_PILLAR||t==D_LOCK) break;
        }
        s32 perp = side==0 ? tx-ddx : ty-ddy; if(perp<0x1000) perp=0x1000;
        ctyp[c]=t; zbuf[c]=perp;
        int cell=my*MW+mx; ccell[c]=cell; seen[cell]=1;
        s32 hx = px_+(s32)(((long long)perp*rx)>>16), hy = py_+(s32)(((long long)perp*ry)>>16);
        u32 u = (u32)((side==0 ? hy : hx)&0xFFFF); if((side==0&&rx>0)||(side==1&&ry<0)) u=0xFFFF-u;
        int w=wtex[cell]; ctex[c]=w;
        int tu=u>>11; if(w<=WT_CRYSTAL && ((cell*7+side)&1)) tu=31-tu;      // mirror plain walls on every other cell
        ctu[c]=tu;
        int lx=(hx-((rx*5)>>4))>>14, ly=(hy-((ry*5)>>4))>>14; if(lx<0)lx=0; if(lx>127)lx=127; if(ly<0)ly=0; if(ly>127)ly=127;
        int li=perp>>12; if(li>511) li=511;
        int L = ((light_tab[li]*lantk)>>6) + ((lsub[(ly<<7)|lx]*fog_tab[li])>>6);
        L = (L*150)/(L+70); if(side==0) L=(L*48)>>6;
        L += ((cell*37)&7)-3; if(L<0) L=0;
        clit[c] = L>127?127:L;
        int h=(FOC<<16)/perp; ctop[c]=H-h; cbot[c]=H+h/2;
    }
    u32 t1=TMR(); prof[0]=t1-t0;
    // pass 2: walls, one column at a time, from the baked textures
    for(int c=0;c<120;c++){
        int top=ctop[c], bot=cbot[c], hh=bot-top; if(hh<1) hh=1;
        int w=ctex[c];
        const u8 *tcol = wall_tex + w*1536 + ctu[c]*48;
        const u16 *rp = wramp + w*8;
        int L=clit[c];
        volatile u16 *col = pg + c;
        int y0 = top<18 ? 18 : top, y1 = bot>159 ? 159 : bot;
        u16 dark=(rp[0]&0xFF)|((rp[0]&0xFF)<<8); wbase[c]=rp[0]&0xF8;
        s32 perp=zbuf[c];
        if((c>0 && zbuf[c-1]-perp > 0x4000) || (c<119 && zbuf[c+1]-perp > 0x4000)){ for(int y=y0;y<=y1;y++) col[y*120]=dark; continue; }
        s32 vstep=(48<<16)/(hh+1), v=(y0-top)*vstep, v2=vstep*2;
        const u8 *bz = bayer + (c&3);
        for(int y=y0; y<=y1; y+=2, v+=v2){
            int t=tcol[v>>16]; int e=rp[t>>5];
            int s = (e&0x100) ? (t&31)<<2 : ((t&31)*L)>>5;
            int ix=(e&0xFF)+lvt[s+bz[((y>>1)&3)<<2]];
            u16 p2=ix|(ix<<8); col[y*120]=p2; if(y<y1) col[y*120+120]=p2;
        }
        if(top>=18) col[top*120]=dark;
        if(bot<=159) col[bot*120]=dark;
    }
    u32 t2=TMR(); prof[3]=t2-t1;
    // pass 3: floor, two rows at a time; water and polished tiles mirror the walls
    int wob=frames*5;
    for(int y=H+1;y<160;y+=((y-H)>40)?4:2){
        int d=y-H, st4=d>40; s32 rd=rd_rel[d];
        s32 fx=px_+(s32)(((long long)rd*rdx[0])>>16), fy=py_+(s32)(((long long)rd*rdy[0])>>16);
        s32 sx=(s32)((((long long)rd*(rdx[119]-rdx[0])>>16)*8812)>>20), sy=(s32)((((long long)rd*(rdy[119]-rdy[0])>>16)*8812)>>20);   // (a multiply, not a slow divide by 119)
        int sl=rl_rel[d], fg=rf_rel[d];
        const u8 *bz=bayi+((y>>1)&3)*4;
        volatile u16 *row=pg+y*120;
        int two = y+1<160, rip = sinI[(y*12+wob)&255]>>13, half = d>16, last=R_CEIL, pv=cbot[0];
        for(int c=0;c<120;c++, fx+=sx, fy+=sy){
            int cb=cbot[c], pcb=pv; pv=cb; if(y+(st4?3:1)<=cb) continue;
            u32 tx=fx>>16, ty=fy>>16; int ix;
            if(half && (c&1) && y>pcb) ix=last;
            else if(tx<MW && ty<MW){
                int fc=fcode[ty*MW+tx];
                int t=floor_tex[(fc<<10)|((fy>>6)&0x3E0)|((fx>>11)&31)];
                int e=framp[(fc<<3)|(t>>5)], s;
                if(e&0x100) s=(t&31)<<2;
                else { int Lp=sl+((lsub[((fy>>7)&0x3F80)|((fx>>14)&127)]*fg)>>6); if(Lp>127) Lp=127; s=((t&31)*Lp)>>5; }
                ix=(e&0xFF)+lvt[s+bz[c&3]];
                if(fc==5 || (fc==2 && t<32)){
                    int dep=y-cb, yr = 2*cb - y + (fc==5 ? rip : 0);
                    if(dep<(fc==5?64:28) && yr>=18 && yr>=ctop[c] && yr<cb && yr<160){      // the mirror only reaches a little way out from the wall, and fades
                        int wv=pg[yr*120+c]&0xFF;
                        if((wv&0xF8)==wbase[c]){ int l=(wv&7)-(fc==5?2:3)-(dep>>(fc==5?4:3)); int rix=(wv&0xF8)+(l<0?0:l);
                            if(fc==5){ if((t&31)<19) ix=rix; } else if(((c^(y>>1))&1)==0) ix=rix; }
                    }
                }
            } else ix=R_CEIL;
            last=ix;
            u16 p2=ix|(ix<<8);
            if(y>cb) row[c]=p2;
            if(two && y+1>cb) row[c+120]=p2;
            if(st4){ if(y+2<160 && y+2>cb) row[c+240]=p2; if(y+3<160 && y+3>cb) row[c+360]=p2; }
        }
    }
    u32 t3=TMR(); prof[1]=t3-t2;
    // pass 4: ceiling, two rows at a time
    for(int y=18;y<H;y+=2){
        int d=H-y; s32 rd=cd_rel[d];
        s32 fx=px_+(s32)(((long long)rd*rdx[0])>>16), fy=py_+(s32)(((long long)rd*rdy[0])>>16);
        s32 sx=(s32)((((long long)rd*(rdx[119]-rdx[0])>>16)*8812)>>20), sy=(s32)((((long long)rd*(rdy[119]-rdy[0])>>16)*8812)>>20);   // (a multiply, not a slow divide by 119)
        int sl=cl_rel[d], fg=cf_rel[d];
        const u8 *bz=bayi+((y>>1)&3)*4;
        volatile u16 *row=pg+y*120;
        int two = y+1<H, last=R_CEIL, pt=ctop[0];
        for(int c=0;c<120;c++, fx+=sx, fy+=sy){
            int ct=ctop[c], pct=pt; pt=ct; if(y>=ct) continue;
            u32 tx=fx>>16, ty=fy>>16; int ix;
            if((c&1) && y<pct) ix=last;
            else if(tx<MW && ty<MW){
                int fc=fcode[ty*MW+tx], cc=((fc>=3&&fc<=5)||fc==9);
                int t=ceil_tex[(cc<<10)|((fy>>6)&0x3E0)|((fx>>11)&31)];
                int e=cramp[(cc<<3)|(t>>5)];
                int Lp=sl+((lsub[((fy>>7)&0x3F80)|((fx>>14)&127)]*fg)>>7); if(Lp>127) Lp=127;
                int s=((t&31)*Lp)>>5;
                ix=(e&0xFF)+lvt[s+bz[c&3]];
            } else ix=R_CEIL;
            last=ix;
            u16 p2=ix|(ix<<8);
            row[c]=p2;
            if(two && y+1<ct) row[c+120]=p2;
        }
    }
    prof[2]=TMR()-t3;
}

// ---------------------------------------------------------------- sprites, lit by the same light, with walking frames
typedef struct { const u8 *art; int aw, ah, hw, tint; } SprArt;
__attribute__((noinline)) static SprArt sprite_art(DObj *o){      // runs from ROM: which picture, how big, what tint
    int anim = ((frames>>2) + (int)(o-dob)) & 1;
    const u8 *art; int aw, ah, hw, tint=0;
    switch(o->kind){
        case K_SPIDER: art=anim?spider_art2:spider_art; aw=SPIDER_W; ah=SPIDER_H; hw=0x7000; break;
        case K_BOSS: art=anim?spider_art2:spider_art; aw=SPIDER_W; ah=SPIDER_H; hw=0x11000; break;
        case K_CHEST: art=chest_art; aw=CHEST_W; ah=CHEST_H; hw=0x5000; break;
        case K_BAT: if((frames>>1)&1){ art=art_bat2; aw=BAT2_W; ah=BAT2_H; } else { art=art_bat; aw=BAT_W; ah=BAT_H; } hw=0x3800; break;
        case K_LAMP: art=art_lamp; aw=LAMP_W; ah=LAMP_H; hw=0x8000; break;
        case K_BARREL: if(o->sub){ art=art_crate; aw=CRATE_W; ah=CRATE_H; hw=0x5800; } else { art=art_barrel; aw=BARREL_W; ah=BARREL_H; hw=0x6800; } break;
        case K_CAGE: art=art_cage; aw=CAGE_W; ah=CAGE_H; hw=0x9000; tint=224+o->sub; break;
        case K_CORE: art=art_gem; aw=GEM_W; ah=GEM_H; hw=0x14000; tint=(frames&8)?R_CRYSTAL+7:R_CRYSTAL+5; break;
        case K_BEETLE: art=anim?beetle_art2:beetle_art; aw=BEETLE_W; ah=BEETLE_H; hw=0x8000; break;
        case K_SLIME: art=art_slime; aw=SLIME_W; ah=SLIME_H; hw=o->sub?0x4000:0x7000; break;
        case K_SCORP: art=art_scorp; aw=SCORP_W; ah=SCORP_H; hw=0x6000; break;
        case K_GHOST: art=art_ghost; aw=GHOST_W; ah=GHOST_H; hw=0x6000; tint=0; break;
        case K_TOAD: art=art_toad; aw=TOAD_W; ah=TOAD_H; hw=0x6000; break;
        case K_ANT: art=anim?art_ant:art_ant; aw=ANT_W; ah=ANT_H; hw=0x5800; break;
        case K_CENTI: art=art_centi; aw=CENTI_W; ah=CENTI_H; hw=0x4000; break;
        case K_GOLEM: art=art_golem; aw=GOLEM_W; ah=GOLEM_H; hw=0xE000; break;
        case K_DRONE: art=art_drone; aw=DRONE_W; ah=DRONE_H; hw=0x5000; break;
        case K_MOTH: art=art_moth; aw=MOTH_W; ah=MOTH_H; hw=0x3800; break;
        case K_SHROOM: art=art_shroom; aw=SHROOM_W; ah=SHROOM_H; hw=0x6800; break;
        case K_CRAB: art=art_crab; aw=CRAB_W; ah=CRAB_H; hw=0x7800; break;
        case K_WISP: art=art_wisp; aw=WISP_W; ah=WISP_H; hw=0x4800; break;
        case K_WASP: if((frames>>1)&1){ art=art_wasp2; aw=WASP2_W; ah=WASP2_H; } else { art=art_wasp; aw=WASP_W; ah=WASP_H; } hw=0x4000; break;
        case K_PET: { static const u16 pt[4]={216,R_FLAME+5,R_STONE+5,218}; static const u16 ph[4]={0x3000,0x3800,0x4200,0x5800}; art=art_pet; aw=PET_W; ah=PET_H; hw=ph[pet_form]; tint=pt[pet_form]; break; }
        default: { int it=o->it, sub=o->sub;
            switch(it){ case IT_BERRY: art=art_berry; aw=BERRY_W; ah=BERRY_H; hw=0x2C00; tint=216+sub; break;
                        case IT_SEED: art=art_seed; aw=SEED_W; ah=SEED_H; hw=0x2400; break;
                        case IT_SHELL: art=art_shell; aw=SHELL_W; ah=SHELL_H; hw=0x2C00; break;
                        case IT_THORN: art=art_thorn; aw=THORN_W; ah=THORN_H; hw=0x3800; break;
                        case IT_WARP: art=art_warp; aw=WARP_W; ah=WARP_H; hw=0x3800; break;
                        case IT_GEM: { static const u8 gc[4]={216,217,222,220}; art=art_gem; aw=GEM_W; ah=GEM_H; hw=0x2C00; tint=gc[sub&3]; break; }
                        case IT_CHARM: art=art_gem; aw=GEM_W; ah=GEM_H; hw=0x2400; tint=R_CRYSTAL+6; break;
                        case IT_BOOK: art=chest_art; aw=CHEST_W; ah=CHEST_H; hw=0x2400; break;
                        case IT_FISH: art=art_thorn; aw=THORN_W; ah=THORN_H; hw=0x2C00; break;
                        case IT_POTION: art=art_berry; aw=BERRY_W; ah=BERRY_H; hw=0x2C00; tint=sub?217:216; break;
                        case IT_QUEST: art=art_warp; aw=WARP_W; ah=WARP_H; hw=0x4000; break;
                        default: art=art_crumb; aw=CRUMB_W; ah=CRUMB_H; hw=0x2400; break; } }
    }
    SprArt r={art,aw,ah,hw,tint}; return r; }
IWRAM static void draw_sprite(volatile u16 *pg, DObj *o, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 sx=o->x-px_, sy=o->y-py_;
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16);
    if(!det) return;
    s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
    s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
    if(tY < 0x3000) return;
    int scx = 60 + (s32)(((long long)tX*60)/tY);
    SprArt sa=sprite_art(o); const u8 *art=sa.art; int aw=sa.aw, ah=sa.ah, hw=sa.hw, tint=sa.tint;
    int hpx = (s32)(((long long)FOC*hw)/tY);
    int wcols = hpx*aw/ah/2;
    if(hpx<2||wcols<1) return;
    s32 z0 = 0;
    if(o->champ) hw = hw*11/8;
    if(o->kind==K_BAT || o->kind==K_WASP || o->kind==K_GHOST || o->kind==K_DRONE || o->kind==K_MOTH || o->kind==K_WISP) z0 = 0x8000 + (sinI[(frames*6+(int)(o-dob)*13)&255]>>3);
    if(o->kind==K_ITEM && o->rar) z0 = 0x1000 + (sinI[(frames*8)&255]>>4);
    if(o->kind==K_LAMP) z0 = 0x18000 - hw;
    s32 eyo=eyeat(o->x,o->y); int bot = hz + (s32)(((long long)(eyo-z0)*FOC)/tY);
    int top = bot - hpx;
    int x0 = scx - wcols/2;
    // how much light falls on it: the smooth light map plus your lantern
    int lx=(o->x>>14)&127, ly=(o->y>>14)&127, li=tY>>12; if(li>511) li=511;
    int lit = ((light_tab[li]*lantk)>>6) + lsub[(ly<<7)|lx];
    int dark = o->kind==K_LAMP ? 0 : lit>95?0 : lit>65?1 : lit>40?2 : 3;
    if(o->champ || (o->kind==K_ITEM && o->rar)) dark = (frames&8) ? -2 : -1;      // elites and magic loot glow
    s32 ustep=(aw<<16)/wcols, vstp=(ah<<16)/hpx;
    int c0 = x0<0?0:x0, y0 = top<18?18:top;
    s32 ua = (c0-x0)*ustep;
    for(int c=c0; c<x0+wcols && c<120; c++, ua+=ustep){
        if(!terr_on && tY >= zbuf[c]) continue;
        const u8 *acol = art + (ua>>16);
        s32 va = (y0-top)*vstp;
        for(int y=y0; y<bot && y<160; y+=2, va+=2*vstp){
            int ix=acol[(va>>16)*aw];
            if(!ix) continue;
            if(terr_on && y<160 && (tY>>13) > (s32)zpix[(y>>1)*120+c]+1) continue;      // hidden behind a ledge, a rail or a pillar
            if(ix==1) ix=tint; else if(dark && ix>=64 && ix<216){ int l=(ix&7)-dark; ix=(ix&~7)+(l<0?0:l>7?7:l); }
            if(o->flash){ if(o->flash>=5) ix = R_UP+7; else if(ix>=64 && ix<216){ int l=(ix&7)+3; ix=(ix&~7)+(l>7?7:l); } }   // one white blink, then a bright glow
            u16 p2=ix|(ix<<8); pg[y*120+c]=p2; if(y+1<bot && y+1<160) pg[y*120+120+c]=p2;
        }
    }
    // a health bar over every hurt monster, and a red ! while it winds up to bite
    if(is_mon(o->kind) && (o->hp<o->maxhp || o->wind) && tY<0x80000){
        int by=top-4; if(by<20) by=20; int w=wcols<8?8:wcols>24?24:wcols; int bx=scx-w/2;
        if(o->hp<o->maxhp){ int f=o->hp*w/(o->maxhp>0?o->maxhp:1); for(int c=bx;c<bx+w;c++){ if(c<0||c>=120||tY>=zbuf[c]) continue; int ix=(c-bx<f)?(o->kind==K_BOSS?C_RED:C_GREEN):C_DGREY; pg[by*120+c]=ix|(ix<<8); pg[(by+1)*120+c]=ix|(ix<<8); } }
        if(o->wind && (frames&2)){ int cx=scx; int y0=by-9; if(y0<19) y0=19; for(int y=y0;y<y0+5;y++) if(cx>=0&&cx<120&&tY<zbuf[cx]) pg[y*120+cx]=C_RED|(C_RED<<8); if(cx>=0&&cx<120&&tY<zbuf[cx]) pg[(y0+6)*120+cx]=C_RED|(C_RED<<8); }
    }
}

// ---------------------------------------------------------------- embers off the torches, dust in the light
typedef struct { s32 x,y,h; s16 vx,vy,vh,life; u8 kind; } Part;
#define NPART 40
EW static Part part[NPART];
static void parts_update(void){
    for(int i=0;i<NPART;i++){ Part *p=&part[i]; if(p->life<=0) continue;
        p->life--; p->x+=p->vx; p->y+=p->vy; p->h+=p->vh;
        if(p->kind==0){ p->vx += (rndi(5)-2)<<4; p->vy += (rndi(5)-2)<<4; }
        else if(p->kind>=2){ p->vh -= 0x180; if(p->h<0){ p->h=0; p->vh=-p->vh/3; p->vx/=2; p->vy/=2; } } }    // debris falls and bounces
    // a new ember from a torch near you
    if(ntorch && (frames&1)){ int k=rndi(ntorch); s32 dx=torch_x[k]-px_, dy=torch_y[k]-py_;
        if(dx<0x70000 && dx>-0x70000 && dy<0x70000 && dy>-0x70000)
            for(int i=0;i<NPART;i++) if(part[i].life<=0){ Part *p=&part[i]; p->kind=0; p->x=torch_x[k]+((rndi(9)-4)<<10); p->y=torch_y[k]+((rndi(9)-4)<<10);
                p->h=0xF000; p->vx=(rndi(9)-4)<<5; p->vy=(rndi(9)-4)<<5; p->vh=0x280+rndi(0x200); p->life=18+rndi(22); break; } }
    // dust drifting where the light is
    if((frames%3)==0) for(int i=0;i<NPART;i++) if(part[i].life<=0){ Part *p=&part[i];
        p->x=px_+((rndi(7)-3)<<16)+rndi(0x10000); p->y=py_+((rndi(7)-3)<<16)+rndi(0x10000);
        int lx=(p->x>>14)&127, ly=(p->y>>14)&127; if(lsub[(ly<<7)|lx]<35) break;
        p->kind=1; p->h=rndi(0x16000); p->vx=(rndi(5)-2)<<6; p->vy=(rndi(5)-2)<<6; p->vh=(rndi(5)-2)<<6; p->life=50+rndi(50); break; }
}
static void parts_draw(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16); if(!det) return;
    for(int i=0;i<NPART;i++){ Part *p=&part[i]; if(p->life<=0) continue;
        s32 sx=p->x-px_, sy=p->y-py_;
        s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
        s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
        if(tY<0x4000 || tY>0x60000) continue;
        int c = 60 + (s32)(((long long)tX*60)/tY); if(c<0||c>=120) continue;
        if(tY>=zbuf[c]) continue;
        int y = hz + (s32)(((long long)(eyeat(p->x,p->y)-p->h)*FOC)/tY); y&=~1; if(y<18||y>156) continue;
        int ix = p->kind==0 ? R_FLAME+5+(p->life>20) : p->kind==2 ? R_WOOD+4+(i&1)*2 : p->kind==3 ? R_SPIDER+3+(i&3) : R_STONE+5;
        u16 px2=ix|(ix<<8); int big = tY<0x18000;
        pg[y*120+c]=px2; pg[y*120+120+c]=px2;
        if(big && c+1<120){ pg[y*120+c+1]=px2; pg[y*120+121+c]=px2; }
    }
}

// ---------------------------------------------------------------- fireballs and stingers
typedef struct { s32 x,y,h,vx,vy; s16 life, dmg; u8 kind, alive; } Proj;     // kind 0 fireball (yours), 1 stinger (theirs)
#define NPROJ 10
EW static Proj proj[NPROJ];
static void fire_proj(int kind, s32 x, s32 y, s32 h, s32 vx, s32 vy, int dmg){
    for(int i=0;i<NPROJ;i++) if(!proj[i].alive){ Proj *p=&proj[i]; p->kind=kind; p->x=x; p->y=y; p->h=h; p->vx=vx; p->vy=vy; p->dmg=dmg; p->life=90; p->alive=1; return; }
}
static void debris(s32 x, s32 y, s32 h, int n, int kind, s32 dx, s32 dy){      // chunks that fly, fall and bounce
    for(int i=0;i<NPART && n;i++) if(part[i].life<=0 || part[i].kind==1){ Part *p=&part[i]; p->kind=kind; p->x=x; p->y=y; p->h=h;
        p->vx=((rndi(17)-8)<<7)+(dx>>5); p->vy=((rndi(17)-8)<<7)+(dy>>5); p->vh=0x500+rndi(0x600); p->life=14+rndi(10); n--; }
}
static void burst(s32 x, s32 y, s32 h, int n, int kind){        // a puff of sparks
    for(int i=0;i<NPART && n;i++) if(part[i].life<=0){ Part *p=&part[i]; p->kind=kind; p->x=x; p->y=y; p->h=h; p->vx=(rndi(17)-8)<<7; p->vy=(rndi(17)-8)<<7; p->vh=(rndi(13)-4)<<7; p->life=8+rndi(10); n--; }
}
static void draw_glow(volatile u16 *pg, s32 wx, s32 wy, s32 h, s32 rad, int c_in, int c_out, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16); if(!det) return;
    s32 sx=wx-px_, sy=wy-py_;
    s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
    s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
    if(tY<0x3000) return;
    int c0 = 60 + (s32)(((long long)tX*60)/tY), y0 = hz + (s32)(((long long)(eyeat(wx,wy)-h)*FOC)/tY);
    int r = (s32)(((long long)rad*FOC)/tY); if(r<2) r=2; if(r>30) r=30; int rc=(r+1)/2;
    for(int dc=-rc; dc<=rc; dc++){ int c=c0+dc; if(c<0||c>=120||tY>=zbuf[c]) continue;
        for(int dy=-r; dy<=r; dy++){ int y=y0+dy; if(y<18||y>159) continue; int d2=dc*dc*4+dy*dy; if(d2>r*r) continue;
            int ix = d2*3 < r*r ? c_in : c_out; pg[y*120+c]=ix|(ix<<8); } }
}
// floating damage numbers
typedef struct { s32 x,y,h; s16 v, life; u8 col; } FNum;
EW static FNum fnum[8];
static void add_fnum(s32 x, s32 y, int v, int col){ if(!opt_nums) return; int k=0; for(int i=1;i<8;i++) if(fnum[i].life<fnum[k].life) k=i; fnum[k]=(FNum){x,y,0xC000,(s16)v,32,(u8)col}; }
static void fnums_update(void){ for(int i=0;i<8;i++){ FNum *f=&fnum[i]; if(f->life>0){ f->life--; f->h+=0x0900; } } }
static void draw_fnums(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16); if(!det) return;
    for(int i=0;i<8;i++){ FNum *f=&fnum[i]; if(f->life<=0) continue;
        s32 sx=f->x-px_, sy=f->y-py_;
        s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
        s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
        if(tY<0x3000) continue;
        int c = 60 + (s32)(((long long)tX*60)/tY), y = hz + (s32)(((long long)(eyeat(f->x,f->y)-f->h)*FOC)/tY);
        char b[8]; b[0]=0; ncat(b,f->v); int x=c*2-textlen(b)*2;
        if(x<0||x>228||y<20||y>150) continue;
        text(pg,x+1,y+1,b,0,1); text(pg,x,y,b,f->col,1);
    }
}
// ---------------------------------------------------------------- the map: rooms you walk into are drawn whole, corridors as you pass
static void map_reveal(void){
    int cx=px_>>16, cy=py_>>16; if(cx<0||cy<0||cx>=MW||cy>=MW) return;
    int r=rid[cy*MW+cx];
    if(r<nrooms && !room_seen[r]){ room_seen[r]=1; Room *R=&rooms[r];
        for(int y=R->y-1;y<=R->y+R->h;y++) for(int x=R->x-1;x<=R->x+R->w;x++) if(x>=0&&y>=0&&x<MW&&y<MW) seen[y*MW+x]=1; }
    for(int y=cy-1;y<=cy+1;y++) for(int x=cx-1;x<=cx+1;x++) if(x>=0&&y>=0&&x<MW&&y<MW) seen[y*MW+x]=1;
}
// a little map in the corner: 15x15 cells around you, north up. one cell = one column by two rows
static void minimap(volatile u16 *pg){
    int cx=px_>>16, cy=py_>>16, ox=2, oy=30;
    for(int j=0;j<15;j++) for(int i=0;i<15;i++){ int x=cx-7+i, y=cy-7+j; int ix=0;
        if(x>=0&&y>=0&&x<MW&&y<MW && seen[y*MW+x]){ int t=dmap[y*MW+x];
            ix = t==D_LOCK ? C_RED : t==D_WALL||t==D_PILLAR ? R_STONE+3 : t==D_DOWN ? C_GOLD : t==D_UP ? C_CYAN : 2; }
        else if(!(x>=0&&y>=0&&x<MW&&y<MW)) ix=0; else ix=1;
        u16 p2=ix|(ix<<8); pg[(oy+j*2)*120+ox+i]=p2; pg[(oy+j*2+1)*120+ox+i]=p2; }
    for(int k=0;k<ndo;k++){ DObj *o=&dob[k]; if(!o->alive) continue; int x=(o->x>>16)-cx+7, y=(o->y>>16)-cy+7; if(x<0||y<0||x>14||y>14||!seen[(o->y>>16)*MW+(o->x>>16)]) continue;
        int ix = is_mon(o->kind) ? (o->kind==K_BOSS?C_RED:C_RED) : o->kind==K_CAGE ? C_GREEN : o->kind==K_CHEST||(o->kind==K_ITEM&&o->rar) ? C_GOLD : -1; if(ix<0) continue;
        if(is_mon(o->kind) && (frames&4)) continue; pg[(oy+y*2)*120+ox+x]=ix|(ix<<8); pg[(oy+y*2+1)*120+ox+x]=ix|(ix<<8); }
    pg[(oy+14)*120+ox+7]=C_WHITE|(C_WHITE<<8); pg[(oy+15)*120+ox+7]=C_WHITE|(C_WHITE<<8);
    { s32 dx=cosx(pang>>2), dy=sinx(pang>>2); int fx=ox+7+((dx*3)>>14), fy=oy+14+((dy*6)>>14); if(fx>=0&&fx<120&&fy>=18&&fy<160) pg[fy*120+fx]=C_WHITE|(C_WHITE<<8); }
}
static int boss_floor(void);
static int stairs_seen(void){ int c=(tgt_y>>16)*MW+(tgt_x>>16); return seen[c] && dmap[c]==D_DOWN; }
static void map_page(volatile u16 *pg){
    if(scene!=1){ text(pg,8,40,"THE MAP WORKS IN DUNGEONS.",C_WHITE,1); text(pg,8,52,"ON A PLANET, FOLLOW THE YELLOW ARROW.",C_GREY,1); return; }
    int ox=8, oy=31;
    for(int y=0;y<MW;y++) for(int x=0;x<MW;x++){ int i=y*MW+x; if(!seen[i] && !sight) continue;
        int t=dmap[i], c;
        if(t==D_WALL){ int any=0; for(int k=0;k<4;k++){ static const s8 d4[4][2]={{1,0},{-1,0},{0,1},{0,-1}}; int xx=x+d4[k][0], yy=y+d4[k][1];
                if(xx>=0&&yy>=0&&xx<MW&&yy<MW && dmap[yy*MW+xx]!=D_WALL) any=1; } if(!any) continue; c=R_STONE+3; }
        else if(t==D_PILLAR) c=R_STONE+5;
        else if(t==D_DOWN) c=C_GOLD; else if(t==D_UP) c=C_CYAN;
        else if(fmat[i]==FL_WATER) c=R_WATER+3; else if(fmat[i]==FL_SHALLOW) c=R_WATER+5;
        else c=C_DGREY;
        if(!seen[i]) c = t==D_DOWN ? C_GOLD : R_STONE+1;           // a sight berry shows the whole floor, dimly
        rect(pg,ox+x*4,oy+y*4,4,4,c); }
    for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive) continue; int x=o->x>>16, y=o->y>>16; if(!seen[y*MW+x]) continue;
        int c = o->kind==K_CAGE ? C_GREEN : o->kind==K_CHEST ? C_GOLD : o->kind==K_BOSS||o->kind==K_CORE ? C_RED : (o->kind==K_ITEM && o->it==IT_KEY) ? C_GOLD : -1;
        if(c>=0){ rect(pg,ox+x*4+1,oy+y*4+1,2,2,c); } }
    int px=ox+(px_>>14), py=oy+(py_>>14);
    rect(pg,px-1,py-1,3,3,C_RED);
    { s32 dx=cosx(pang>>2), dy=sinx(pang>>2); for(int k=2;k<5;k++) pix(pg,px+(dx*k>>14),py+(dy*k>>14),C_WHITE); }
    int lx=144, y=40; char b[32];
    scpy(b,base_mode?"DRONE OUTPOST":world_name()); b[22]=0; text(pg,lx,y,b,C_GOLD,1); y+=8;
    scpy(b,"FLOOR B"); ncat(b,depth_no); if(base_mode){ scat(b," OF "); ncat(b,base_floors()); } else if(!endless){ scat(b," OF "); ncat(b,zone_floors()); } text(pg,lx,y,b,C_WHITE,1); y+=14;
    rect(pg,lx,y,5,5,C_RED); text(pg,lx+9,y,"YOU",C_WHITE,1); y+=9;
    rect(pg,lx,y,5,5,C_GOLD); text(pg,lx+9,y,"STAIRS DOWN",C_WHITE,1); y+=9;
    rect(pg,lx,y,5,5,C_CYAN); text(pg,lx+9,y,"STAIRS UP",C_WHITE,1); y+=9;
    rect(pg,lx+1,y+1,3,3,C_GREEN); text(pg,lx+9,y,"CAGED BUG",C_WHITE,1); y+=9;
    rect(pg,lx+1,y+1,3,3,C_RED); text(pg,lx+9,y,base_mode?"CORE":"BOSS",C_WHITE,1); y+=9;
    if(base_mode){ rect(pg,lx,y,5,5,C_RED); text(pg,lx+9,y,"LOCKED DOOR",C_WHITE,1); y+=9; }
    y+=5;
    if(!endless && depth_no>=zone_floors()){ if(boss_floor()) text(pg,lx,y,"THE BOSS IS HERE",C_RED,1); else { text(pg,lx,y,"BOSS BEATEN!",C_GREEN,1); text(pg,lx,y+9,"EXIT: STAIRS UP",C_GREY,1); } }
    else text(pg,lx,y,stairs_seen()?"YOU FOUND THE STAIRS":"FIND THE STAIRS",stairs_seen()?C_GREEN:C_GREY,1);
}
static int base_floors(void){ return 2 + (outposts_done>5?3:outposts_done/2); }
static int boss_floor(void){ for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].kind==K_BOSS) return 1; return 0; }
static DObj petobj;
static s32 pet_x, pet_y;

static void dungeon_label(char *b){
    if(base_mode) scpy(b,"OUTPOST"); else scpy(b,world_name()); scat(b," B"); char n2[6];
    if(endless){ num(n2,depth_no,depth_no>99?3:depth_no>9?2:1); scat(b,n2); if(cur_moon){ scat(b," T"); ncat(b,moon_tier()); } }
    else if(base_mode){ num(n2,depth_no,1); scat(b,n2); scat(b,"/"); num(n2,base_floors(),1); scat(b,n2); }
    else { num(n2,depth_no,3); scat(b,n2); scat(b,"/"); num(n2,zone_floors(),3); scat(b,n2); if(tier_now()){ scat(b," T"); ncat(b,tier_now()); } }
}
// the status bar is drawn only into the page you can't see, so it never flickers: once for each of the two pages
static int hud_dirty=0;
static void dungeon_hud(void){ hud_dirty=2; }
static void dungeon_hud_both(void){ char b[40]; dungeon_label(b); for(int p=0;p<2;p++) draw_hud((volatile u16*)(0x06000000 + p*0xA000), b); hud_dirty=0; }
static int stair_ok;
static void dungeon_palette(void){      // the dungeon's whole background palette, from the clean copy
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    for(int i=0;i<8;i++) BG_PAL[R_SPIDER+i]= endless ? w_mon[i] : zone_mon_pal[zone*8+i];
}
static void dungeon_enter(void){ auto_run=0;
    depth_no++; stair_ok=0; st_floors++; ach_check(); sight=0; msg_timer=0;
    make_floor();
    dungeon_palette();
    obj_palette();
    if(endless && !forge_on && depth_no>best_depth) best_depth=depth_no;
    lantk = have_bug[BUG_FIRE] ? 74 : 58;
    int rk = have_bug[BUG_FIRE] ? 56 : 44; if(fl_ev==FE_DARK){ rk=rk*3/5; lantk=lantk*3/4; } if(mboss){ rk=rk*3/4; lantk=lantk*4/5; }
    for(int d=1;d<100;d++){
        rd_rel[d]=(FOC<<15)/d; int li=rd_rel[d]>>12; if(li>511) li=511; rl_rel[d]=light_tab[li]*rk/64; rf_rel[d]=fog_tab[li]*52/64;
        cd_rel[d]=(FOC<<16)/d; li=cd_rel[d]>>12; if(li>511) li=511; cl_rel[d]=light_tab[li]*36/64; cf_rel[d]=fog_tab[li];
    }
    for(int i=0;i<160;i++) wramp[i]=wall_ramp[i];
    for(int i=0;i<80;i++) framp[i]=floor_ramp[i];
    for(int i=0;i<16;i++) cramp[i]=ceil_ramp[i];
    for(int i=0;i<256;i++) sinI[i]=sin_tab[i];
    for(int i=0;i<NPART;i++) part[i].life=0;
    for(int i=0;i<NPROJ;i++) proj[i].alive=0;
    for(int i=0;i<8;i++) fnum[i].life=0;
    pet_x=px_; pet_y=py_;
    for(int c=0;c<120;c++) camtab[c]=((2*c-119)<<16)/120;
    for(int i=0;i<160;i++) lvt[i]= (i>>4)>7 ? 7 : (i>>4);
    for(int i=0;i<16;i++) bayi[i]=15-bayer[i];
    for(int i=0;i<MW*MW;i++){ int t=dmap[i], f=fmat[i], x=i%MW, y=i/MW;
        fcode[i] = t==D_UP?7 : t==D_DOWN?8 : f==FL_FLAG?((x+y)&1) : f==FL_TILE?2 : f==FL_DIRT?3 : f==FL_MOSS?4 : f==FL_WATER?5 : f==FL_SHALLOW?9 : 6; }
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    dungeon_hud_both();
    if(fl_ev){ char b[40]; scpy(b,fe_name[fl_ev]); scat(b,"!"); say(b); msg_timer=60;
        if(fl_ev==FE_BLESSED){ int mk=rndi(MK_GOOD); mark_t[mk]=150; recalc(); scpy(b,"BLESSED: "); scat(b,mk_name[mk]); scat(b," FOR A WHILE"); say(b); msg_timer=60; } }
}
static void dungeon_checkpoint(void){                 // every new floor is a save point: if you load, you wake on the surface with a portal open to this floor
    if(!opt_autosave || endless || base_mode) return;
    int pd=portal_depth; portal_depth=depth_no; do_save(); portal_depth=pd; say("CHECKPOINT SAVED"); msg_timer=40;
}
static void fl_kill(DObj *o){                                                   // a monster died: does that move the floor's goal?
    if(fl_ev==FE_ELITE && o->champ==1 && !fl_done && ++fl_have>=fl_need){ fl_done=1; int g=200+dlv()*40+depth_no*10; earn(g); gain_xp(30+dlv()*6); char b[40]; scpy(b,"ELITES SLAIN! +"); ncat(b,g); scat(b,"G"); say(b); msg_timer=50; sfx(6); }
}
static void fl_leave(void){                                                     // taking the stairs down
    if(fl_ev==FE_SPEED && !fl_done && (s32)(fl_end-vbl_count)>0){ fl_done=1; int g=120+dlv()*30+depth_no*8; earn(g); gain_xp(20+dlv()*5); char b[40]; scpy(b,"SPEED RUN BEATEN! +"); ncat(b,g); scat(b,"G"); say(b); sfx(6); }
    fl_bonus=100;
}
static void fl_draw(volatile u16 *pg){                                          // the goal line under the compass
    if(!fl_ev || fl_done) return; char b[40]; scpy(b,fe_name[fl_ev]);
    if(fl_ev==FE_ELITE){ scat(b," "); ncat(b,fl_have); scat(b,"/"); ncat(b,fl_need); }
    else if(fl_ev==FE_SPEED){ s32 left=(s32)(fl_end-vbl_count)/60; if(left<0){ left=0; } scat(b," "); ncat(b,left); scat(b,"S"); if(left==0) scpy(b,"SPEED RUN: TOO SLOW"); }
    text(pg,4,30,b, fl_ev==FE_CURSED||fl_ev==FE_AMBUSH ? C_RED : fl_ev==FE_SPEED ? C_GOLD : C_CYAN, 1);
}
