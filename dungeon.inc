// ================================================================= THE DUNGEON
#define MW 32
enum { D_FLOOR, D_WALL, D_PILLAR, D_UP, D_DOWN };
enum { M_BRICK, M_STONE, M_EARTH, M_MOSS, M_CRYSTAL, M_MARBLE };
#define F_TORCH 0x10
#define F_BEAM  0x20
enum { FL_FLAG, FL_TILE, FL_DIRT, FL_MOSS, FL_WATER, FL_PLANK };
enum { ST_HALL, ST_CRYPT, ST_CAVE, ST_GROTTO, ST_CRYSTAL, NSTYLE };
static const u8 style_wall[NSTYLE]={M_BRICK,M_STONE,M_EARTH,M_MOSS,M_CRYSTAL};
static const u8 style_floor[NSTYLE]={FL_FLAG,FL_TILE,FL_DIRT,FL_MOSS,FL_TILE};
static u8 dmap[MW*MW];
EW static u8 wmat[MW*MW], fmat[MW*MW], rid[MW*MW], wdec[MW*MW];
static u16 lampcell[12]; static int nlamps;
typedef struct { s32 x,y; s16 hp, maxhp; s8 kind, alive, flash; u8 sub, it, rar, pre, suf, lvl, champ, cname; s16 cool, venom; s32 kvx, kvy; u8 stag, lunge; } DObj;
enum { K_SPIDER, K_CHEST, K_ITEM, K_CAGE, K_BOSS, K_BAT, K_LAMP, K_BEETLE, K_WASP, K_PET, K_BARREL };
static const char *const champ_pre[8]={"GRIM","BLOODY","GIANT","SWIFT","ANCIENT","VILE","FROST","ROTTEN"};
static const char *const kind_name[11]={"SPIDER","","","","QUEEN","BAT","","BEETLE","WASP","","BARREL"};
static int is_mon(int k){ return k==K_SPIDER||k==K_BOSS||k==K_BAT||k==K_BEETLE||k==K_WASP; }
enum { DC_NONE, DC_PANEL, DC_NICHE, DC_DOOR, DC_RUNE, DC_PLAQUE };
#define NDO 40
static s32 tgt_x, tgt_y; static int lantk=58;
EW static DObj dob[NDO]; static int ndo;
static s32 px_, py_; static int pang;
static s32 zbuf[120];
static int hurt_flash=0, atk_t=0;
typedef struct { u8 x,y,w,h,style; } Room;
EW static Room rooms[10]; static int nrooms;
typedef struct { u8 x,y,str; } Light;
EW static Light lights[64]; static int nlights;
EW static s32 torch_x[32], torch_y[32]; static int ntorch;
static void build_light(void); static void build_walls(void);

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
    if(hx){ for(int x=(ax<bx?ax:bx); x<=(ax<bx?bx:ax); x++){ cor_cell(x,ay,fm); if(wide) cor_cell(x,ay+1,fm); }
            for(int y=(ay<by?ay:by); y<=(ay<by?by:ay); y++){ cor_cell(bx,y,fm); if(wide) cor_cell(bx+1,y,fm); } }
    else  { for(int y=(ay<by?ay:by); y<=(ay<by?by:ay); y++){ cor_cell(ax,y,fm); if(wide) cor_cell(ax+1,y,fm); }
            for(int x=(ax<bx?ax:bx); x<=(ax<bx?bx:ax); x++){ cor_cell(x,by,fm); if(wide) cor_cell(x,by+1,fm); } }
}
static void pillar_at(int x,int y,int m){ if(x<1||y<1||x>=MW-1||y>=MW-1) return; int i=y*MW+x; dmap[i]=D_PILLAR; wmat[i]=m; }
static int room_floor_cell(Room *r){
    for(int t=0;t<40;t++){ int x=r->x+1+rndi(r->w-2), y=r->y+1+rndi(r->h-2); if(dmap[y*MW+x]==D_FLOOR && fmat[y*MW+x]!=FL_WATER) return y*MW+x; }
    return (r->y+r->h/2)*MW + r->x+r->w/2;
}

static void make_floor(void){
    for(int i=0;i<MW*MW;i++){ dmap[i]=D_WALL; wmat[i]=M_BRICK; fmat[i]=FL_FLAG; rid[i]=254; wdec[i]=0; }
    nrooms=0; nlights=0; nlamps=0; ntorch=0;
    for(int t=0;t<400 && nrooms<8;t++){
        static const u8 zst[6][3]={{ST_HALL,ST_CAVE,ST_GROTTO},{ST_GROTTO,ST_GROTTO,ST_CRYPT},{ST_HALL,ST_CRYPT,ST_HALL},{ST_CRYSTAL,ST_CRYSTAL,ST_CAVE},{ST_CRYSTAL,ST_HALL,ST_CRYPT},{ST_CAVE,ST_CAVE,ST_CRYPT}};
        const u8 *zs = endless ? w_styles : zst[zone];
        int st = nrooms==0 ? zs[0] : (rndi(10)<7 ? zs[rndi(3)] : rndi(NSTYLE)), w, h;
        if(st==ST_HALL){ w=9+rndi(5); h=7+rndi(2); if(rnd()&1){ int k=w; w=h; h=k; } }
        else { w=6+rndi(5); h=6+rndi(4); }
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
            if(w>=h){ for(int xx=x+2; xx<=x+w-3; xx+=2){ pillar_at(xx,y+2,M_MARBLE); pillar_at(xx,y+h-3,M_MARBLE); } }
            else    { for(int yy=y+2; yy<=y+h-3; yy+=2){ pillar_at(x+2,yy,M_MARBLE); pillar_at(x+w-3,yy,M_MARBLE); } }
        }
        if(st==ST_CRYPT){ for(int xx=x+2; xx<x+w-2; xx+=3) for(int yy=y+2; yy<y+h-2; yy+=3) pillar_at(xx,yy,M_STONE); }
        if(st==ST_GROTTO){ for(int yy=y+2;yy<y+h-2;yy++) for(int xx=x+2;xx<x+w-2;xx++) fmat[yy*MW+xx]=FL_WATER; add_light(x+w/2,y+h/2,45); }
        if(st==ST_CRYSTAL){ for(int k=0;k<2;k++){ pillar_at(x+2+rndi(w-4), y+2+rndi(h-4), M_CRYSTAL); } add_light(x+w/2,y+h/2,40); }
        if(st==ST_CAVE) add_light(x+w/2,y+h/2,28);
        nrooms++;
    }
    for(int i=1;i<nrooms;i++) corridor(&rooms[i-1],&rooms[i], rnd()&1, (i&1)?FL_FLAG:FL_PLANK);
    if(nrooms>3) corridor(&rooms[0],&rooms[nrooms-1], 1, FL_FLAG);     // a loop, so it is not one long line
    // walls take the look of the room they face; decorate the faces
    for(int y=1;y<MW-1;y++) for(int x=1;x<MW-1;x++){
        int i=y*MW+x; if(dmap[i]!=D_WALL) continue;
        int nb[4]={i-1,i+1,i-MW,i+MW}, f=-1;
        for(int k=0;k<4;k++) if(floorish(nb[k])){ f=nb[k]; if(rid[nb[k]]<nrooms) break; }
        if(f<0) continue;
        int r=rid[f], st = r<nrooms ? rooms[r].style : -1;
        wmat[i] = st>=0 ? style_wall[st] : M_BRICK;
        int hsh=(x*7+y*13)&7;
        if(st==ST_HALL||st==ST_CRYPT||st<0){
            int h2=(x*31+y*17+depth_no*7+dlv()*3)%13;
            wdec[i] = h2<2?DC_NICHE : h2<3?DC_DOOR : h2<4?DC_PLAQUE : h2<6?DC_RUNE : (st==ST_CRYPT && h2<11)?DC_PANEL : DC_NONE;
        }
        if((st==ST_HALL||st==ST_CRYPT) && hsh<2){ wmat[i]|=F_TORCH; add_light(f%MW,f/MW,72); if(ntorch<32){ torch_x[ntorch]=(x<<16)+0x8000+((f%MW-x)*0x8C00); torch_y[ntorch]=(y<<16)+0x8000+((f/MW-y)*0x8C00); ntorch++; } if(wdec[i]!=DC_PANEL) wdec[i]=DC_NONE; }
        else if(st<0 && ((x*5+y*3)%9)==0){ wmat[i]|=F_TORCH; add_light(f%MW,f/MW,60); if(ntorch<32){ torch_x[ntorch]=(x<<16)+0x8000+((f%MW-x)*0x8C00); torch_y[ntorch]=(y<<16)+0x8000+((f/MW-y)*0x8C00); ntorch++; } }
        else if(st==ST_CAVE && ((x+y)%3)==0) wmat[i]|=F_BEAM;
        else if(st==ST_CRYSTAL && hsh==3) add_light(f%MW,f/MW,34);
    }
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
    int last = !endless && depth_no>=zone_floors();
    if(last){ dmap[dn]=D_FLOOR; }
    if(endless && depth_no>1) dmap[up]=D_FLOOR;          // no way back up: warp out or keep going
    tgt_x=((dn%MW)<<16)+0x8000; tgt_y=((dn/MW)<<16)+0x8000;
    ndo=0;
    #define SPAWN(k_,cell_,hp_,sub_) do{ if(ndo<NDO){ DObj *o=&dob[ndo++]; memset(o,0,sizeof(DObj)); int c_=(cell_); o->kind=(k_); o->alive=1; o->hp=o->maxhp=(hp_); o->sub=(sub_); o->x=((c_%MW)<<16)+0x8000; o->y=((c_/MW)<<16)+0x8000; } }while(0)
    int base_hp = 6 + dlv()*5 + depth_no*2;
    int ilvl = dlv()*3 + depth_no;
    for(int i=1;i<nrooms;i++){
        int c = 1 + (rooms[i].w*rooms[i].h>60) + ((dlv()+depth_no)/4 > 3 ? 3 : (dlv()+depth_no)/4);
        for(int k=0;k<c;k++){
            int r=rndi(10), kind = r<5 ? K_SPIDER : (r<7 && (dlv()>=1||depth_no>=2)) ? K_BEETLE : (r<9 && (dlv()>=2||depth_no>=3)) ? K_WASP : K_SPIDER;
            int hp = kind==K_BEETLE ? base_hp*5/2 : kind==K_WASP ? base_hp*4/5 : base_hp;
            SPAWN(kind, room_floor_cell(&rooms[i]), hp, 0);
            if(rndi(100)<8){ DObj *o=&dob[ndo-1]; o->champ=1; o->cname=rndi(8); o->hp=o->maxhp=hp*3; }
        }
        if(rnd()&1) SPAWN(K_CHEST, room_floor_cell(&rooms[i]), 1, 0);
    }
    for(int i=0;i<nrooms;i++){ int c=rndi(3); for(int k=0;k<c;k++){ int cr=rndi(3)==0; SPAWN(K_BARREL, room_floor_cell(&rooms[i]), cr?3:1, cr); } }   // barrels and crates to shove and smash
    #define SPAWN_GEAR(cell_,g_) do{ Gear gg_=(g_); SPAWN(K_ITEM,(cell_),1,gg_.sub); DObj *o_=&dob[ndo-1]; o_->it=gg_.it; o_->rar=gg_.rar; o_->pre=gg_.pre; o_->suf=gg_.suf; o_->lvl=gg_.lvl; }while(0)
    for(int i=0;i<nrooms;i++){ int c = 1 + rndi(2); for(int k=0;k<c;k++) SPAWN_GEAR(room_floor_cell(&rooms[i]), roll_drop(ilvl,0)); }
    int cages = 1 + (nrooms>5);
    for(int k=0;k<cages;k++){ int r=1+rndi(nrooms>1?nrooms-1:1); SPAWN(K_CAGE, room_floor_cell(&rooms[r<nrooms?r:0]), 1, rndi(8)<2?BUG_PILL:rndi(6)); }
    int nb = 1 + (dlv()/2>3?3:dlv()/2) + (depth_no>2);
    for(int k=0;k<nb;k++) SPAWN(K_BAT, room_floor_cell(&rooms[nrooms>1 ? 1+rndi(nrooms-1) : 0]), base_hp/2+1, 0);
    for(int k=0;k<nlamps;k++) SPAWN(K_LAMP, lampcell[k], 1, 0);
    if(last && !zone_cleared){ SPAWN(K_BOSS, dn, 40+dlv()*30, 0); }
    if(endless){
        if(hive_boss_floor()) SPAWN(K_BOSS, dn, 150+depth_no*12+dlv()*45, 0);              // a Tech Hive overlord guards the hive core
        else if(depth_no%10==0) SPAWN(K_BOSS, dn, 60+depth_no*6+dlv()*20, 0);               // a guardian every 10 floors
        if(depth_no%5==0 || rndi(10)<3) SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), ((Gear){IT_WARP,0,0,0,0,0}));
        int ng = 1 + rndi(2) + depth_no/8; if(ng>4) ng=4;
        for(int k=0;k<ng;k++){ int t=rndi(10)+depth_no/6; Gear g={IT_GEM,(u8)(t<5?0 : t<8?1 : t<11?2 : 3),0,0,0,0}; SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), g); }
        if(depth_no>=5 && rndi(8)==0){ Gear g=roll_gear(rndi(2)?IT_SHELL:IT_THORN, ilvl+10, 60); g.sub=4; SPAWN_GEAR(room_floor_cell(&rooms[rndi(nrooms)]), g); }
    }
    // quests that live on this floor
    int qhere = q_type>0 && !q_done && depth_no==q_floor && q_zone==zone && q_world==world_no;
    if(qhere && q_type==Q_CHAMP){ SPAWN(rndi(2)?K_SPIDER:K_BEETLE, room_floor_cell(&rooms[far]), base_hp*5, 0); dob[ndo-1].champ=2; }
    if(qhere && q_type==Q_FETCH) SPAWN_GEAR(room_floor_cell(&rooms[far]), ((Gear){IT_QUEST,0,0,0,0,0}));
}
static int solid(s32 x, s32 y){ int cx=x>>16, cy=y>>16; if(cx<0||cy<0||cx>=MW||cy>=MW) return 1; int t=dmap[cy*MW+cx]; return t==D_WALL||t==D_PILLAR; }

static u8 bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
static int hz=89;          // the horizon row; it bobs while you walk and shakes when you are hit
#define FOC 182            // focal length in pixel rows
static s32 rd_rel[100], cd_rel[100];                  // floor / ceiling distance by rows from the horizon
static u8 rl_rel[100], rf_rel[100], cl_rel[100], cf_rel[100];
static s32 rdx[120], rdy[120];
static u8 ctyp[120], clit[120], ctex[120], ctu[120]; static u16 ccell[120]; static s16 ctop[120], cbot[120];
static u8 lvt[160];                 // light level 0..127 plus dither -> shade 0..7
static u8 bayi[16];
static s32 camtab[120];
static u8 fcode[MW*MW], wtex[MW*MW];
static u16 wramp[20*8], framp[9*8], cramp[2*8];
static s16 sinI[256];
static u8 lsub[128*128] __attribute__((section(".ewram")));     // smooth light, 4x4 per tile

// ---------------------------------------------------------------- baking the light
static int opaque(int tx,int ty){ if(tx<0||ty<0||tx>=MW||ty>=MW) return 1; int t=dmap[ty*MW+tx]; return t==D_WALL||t==D_PILLAR; }
static void build_light(void){
    for(int i=0;i<128*128;i++) lsub[i]=9;
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
        if(t==D_PILLAR) w = m==M_STONE?WT_STONE_PILLAR : m==M_CRYSTAL?WT_CRYSTAL_PILLAR : WT_MARBLE;
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
#define TMR() (*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16))
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
            t=dmap[my*MW+mx]; if(t==D_WALL||t==D_PILLAR) break;
        }
        s32 perp = side==0 ? tx-ddx : ty-ddy; if(perp<0x1000) perp=0x1000;
        ctyp[c]=t; zbuf[c]=perp;
        int cell=my*MW+mx; ccell[c]=cell;
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
        u16 dark=(rp[0]&0xFF)|((rp[0]&0xFF)<<8);
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
    for(int y=H+1;y<160;y+=2){
        int d=y-H; s32 rd=rd_rel[d];
        s32 fx=px_+(s32)(((long long)rd*rdx[0])>>16), fy=py_+(s32)(((long long)rd*rdy[0])>>16);
        s32 sx=(s32)(((long long)rd*(rdx[119]-rdx[0]))>>16)/119, sy=(s32)(((long long)rd*(rdy[119]-rdy[0]))>>16)/119;
        int sl=rl_rel[d], fg=rf_rel[d];
        const u8 *bz=bayi+((y>>1)&3)*4;
        volatile u16 *row=pg+y*120;
        int two = y+1<160, rip = sinI[(y*12+wob)&255]>>13, half = d>22, last=R_CEIL;
        for(int c=0;c<120;c++, fx+=sx, fy+=sy){
            int cb=cbot[c]; if(y+1<=cb) continue;
            u32 tx=fx>>16, ty=fy>>16; int ix;
            if(half && (c&1) && y>cbot[c-1]) ix=last;
            else if(tx<MW && ty<MW){
                int fc=fcode[ty*MW+tx];
                int t=floor_tex[(fc<<10)|((fy>>6)&0x3E0)|((fx>>11)&31)];
                int e=framp[(fc<<3)|(t>>5)], s;
                if(e&0x100) s=(t&31)<<2;
                else { int Lp=sl+((lsub[((fy>>7)&0x3F80)|((fx>>14)&127)]*fg)>>6); if(Lp>127) Lp=127; s=((t&31)*Lp)>>5; }
                ix=(e&0xFF)+lvt[s+bz[c&3]];
                if(fc==5 || (fc==2 && t<32)){
                    int yr = 2*cb - y + (fc==5 ? rip : 0);
                    if(yr>=18 && yr>=ctop[c] && yr<cb){
                        int wv=pg[yr*120+c]&0xFF;
                        if(wv>=64 && wv<216){ int l=(wv&7)-(fc==5?2:3); int rix=(wv&~7)+(l<0?0:l);
                            if(fc==5){ if((t&31)<19) ix=rix; } else if(((c^(y>>1))&1)==0) ix=rix; }
                    }
                }
            } else ix=R_CEIL;
            last=ix;
            u16 p2=ix|(ix<<8);
            if(y>cb) row[c]=p2;
            if(two) row[c+120]=p2;
        }
    }
    u32 t3=TMR(); prof[1]=t3-t2;
    // pass 4: ceiling, two rows at a time
    for(int y=18;y<H;y+=2){
        int d=H-y; s32 rd=cd_rel[d];
        s32 fx=px_+(s32)(((long long)rd*rdx[0])>>16), fy=py_+(s32)(((long long)rd*rdy[0])>>16);
        s32 sx=(s32)(((long long)rd*(rdx[119]-rdx[0]))>>16)/119, sy=(s32)(((long long)rd*(rdy[119]-rdy[0]))>>16)/119;
        int sl=cl_rel[d], fg=cf_rel[d];
        const u8 *bz=bayi+((y>>1)&3)*4;
        volatile u16 *row=pg+y*120;
        int two = y+1<H, last=R_CEIL;
        for(int c=0;c<120;c++, fx+=sx, fy+=sy){
            int ct=ctop[c]; if(y>=ct) continue;
            u32 tx=fx>>16, ty=fy>>16; int ix;
            if((c&1) && y<ctop[c-1]) ix=last;
            else if(tx<MW && ty<MW){
                int fc=fcode[ty*MW+tx], cc=(fc>=3&&fc<=5);
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
IWRAM static void draw_sprite(volatile u16 *pg, DObj *o, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 sx=o->x-px_, sy=o->y-py_;
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16);
    if(!det) return;
    s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
    s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
    if(tY < 0x3000) return;
    int scx = 60 + (s32)(((long long)tX*60)/tY);
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
        case K_BEETLE: art=anim?beetle_art2:beetle_art; aw=BEETLE_W; ah=BEETLE_H; hw=0x8000; break;
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
    int hpx = (s32)(((long long)FOC*hw)/tY);
    int wcols = hpx*aw/ah/2;
    if(hpx<2||wcols<1) return;
    s32 z0 = 0;
    if(o->champ) hw = hw*11/8;
    if(o->kind==K_BAT || o->kind==K_WASP) z0 = 0x8000 + (sinI[(frames*6+(int)(o-dob)*13)&255]>>3);
    if(o->kind==K_ITEM && o->rar) z0 = 0x1000 + (sinI[(frames*8)&255]>>4);
    if(o->kind==K_LAMP) z0 = 0x18000 - hw;
    int bot = hz + (s32)(((long long)(0x8000-z0)*FOC)/tY);
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
        if(tY >= zbuf[c]) continue;
        const u8 *acol = art + (ua>>16);
        s32 va = (y0-top)*vstp;
        for(int y=y0; y<bot && y<160; y+=2, va+=2*vstp){
            int ix=acol[(va>>16)*aw];
            if(!ix) continue;
            if(ix==1) ix=tint; else if(dark && ix>=64 && ix<216){ int l=(ix&7)-dark; ix=(ix&~7)+(l<0?0:l>7?7:l); }
            if(o->flash){ if(o->flash>=5) ix = R_UP+7; else if(ix>=64 && ix<216){ int l=(ix&7)+3; ix=(ix&~7)+(l>7?7:l); } }   // one white blink, then a bright glow
            u16 p2=ix|(ix<<8); pg[y*120+c]=p2; if(y+1<bot && y+1<160) pg[y*120+120+c]=p2;
        }
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
        int y = hz + (s32)(((long long)(0x8000-p->h)*FOC)/tY); y&=~1; if(y<18||y>156) continue;
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
    int c0 = 60 + (s32)(((long long)tX*60)/tY), y0 = hz + (s32)(((long long)(0x8000-h)*FOC)/tY);
    int r = (s32)(((long long)rad*FOC)/tY); if(r<2) r=2; if(r>30) r=30; int rc=(r+1)/2;
    for(int dc=-rc; dc<=rc; dc++){ int c=c0+dc; if(c<0||c>=120||tY>=zbuf[c]) continue;
        for(int dy=-r; dy<=r; dy++){ int y=y0+dy; if(y<18||y>159) continue; int d2=dc*dc*4+dy*dy; if(d2>r*r) continue;
            int ix = d2*3 < r*r ? c_in : c_out; pg[y*120+c]=ix|(ix<<8); } }
}
// floating damage numbers
typedef struct { s32 x,y,h; s16 v, life; u8 col; } FNum;
EW static FNum fnum[8];
static void add_fnum(s32 x, s32 y, int v, int col){ int k=0; for(int i=1;i<8;i++) if(fnum[i].life<fnum[k].life) k=i; fnum[k]=(FNum){x,y,0xC000,(s16)v,32,(u8)col}; }
static void fnums_update(void){ for(int i=0;i<8;i++){ FNum *f=&fnum[i]; if(f->life>0){ f->life--; f->h+=0x0900; } } }
static void draw_fnums(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16); if(!det) return;
    for(int i=0;i<8;i++){ FNum *f=&fnum[i]; if(f->life<=0) continue;
        s32 sx=f->x-px_, sy=f->y-py_;
        s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
        s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
        if(tY<0x3000) continue;
        int c = 60 + (s32)(((long long)tX*60)/tY), y = hz + (s32)(((long long)(0x8000-f->h)*FOC)/tY);
        char b[8]; b[0]=0; ncat(b,f->v); int x=c*2-textlen(b)*2;
        if(x<0||x>228||y<20||y>150) continue;
        text(pg,x+1,y+1,b,0,1); text(pg,x,y,b,f->col,1);
    }
}
static DObj petobj;
static s32 pet_x, pet_y;

static void dungeon_hud(void){
    char b[40]; scpy(b,world_name()); scat(b," B"); char n2[6];
    if(endless){ num(n2,depth_no,depth_no>99?3:depth_no>9?2:1); scat(b,n2); }
    else { num(n2,depth_no,1); scat(b,n2); scat(b,"/"); num(n2,zone_floors(),1); scat(b,n2); }
    for(int p=0;p<2;p++) draw_hud((volatile u16*)(0x06000000 + p*0xA000), b);
}
static void dungeon_enter(void){
    depth_no++; sight=0; msg_timer=0;
    make_floor();
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i];
    for(int i=0;i<16;i++) OBJ_PAL[i]=obj_pal[i];
    for(int i=0;i<8;i++) BG_PAL[R_SPIDER+i]= endless ? w_mon[i] : zone_mon_pal[zone*8+i];
    if(endless && depth_no>best_depth) best_depth=depth_no;
    lantk = have_bug[BUG_FIRE] ? 74 : 58;
    int rk = have_bug[BUG_FIRE] ? 56 : 44;
    for(int d=1;d<100;d++){
        rd_rel[d]=(FOC<<15)/d; int li=rd_rel[d]>>12; if(li>511) li=511; rl_rel[d]=light_tab[li]*rk/64; rf_rel[d]=fog_tab[li]*52/64;
        cd_rel[d]=(FOC<<16)/d; li=cd_rel[d]>>12; if(li>511) li=511; cl_rel[d]=light_tab[li]*36/64; cf_rel[d]=fog_tab[li];
    }
    for(int i=0;i<160;i++) wramp[i]=wall_ramp[i];
    for(int i=0;i<72;i++) framp[i]=floor_ramp[i];
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
        fcode[i] = t==D_UP?7 : t==D_DOWN?8 : f==FL_FLAG?((x+y)&1) : f==FL_TILE?2 : f==FL_DIRT?3 : f==FL_MOSS?4 : f==FL_WATER?5 : 6; }
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    dungeon_hud();
}
