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
static u8 dmap[MW*MW], wmat[MW*MW], fmat[MW*MW], lmap[MW*MW], rid[MW*MW], wdec[MW*MW];
static u16 lampcell[12]; static int nlamps;
typedef struct { s32 x,y; s16 hp; s8 kind, alive, flash; u8 sub; s16 cool; } DObj;
enum { K_SPIDER, K_CHEST, K_ITEM, K_CAGE, K_BOSS, K_BAT, K_LAMP };
enum { DC_NONE, DC_PANEL, DC_NICHE, DC_DOOR, DC_RUNE, DC_PLAQUE };
#define NDO 32
static s32 tgt_x, tgt_y; static int lantk=58;
static DObj dob[NDO]; static int ndo;
static s32 px_, py_; static int pang;
static s32 zbuf[120];
static int hurt_flash=0, atk_t=0;
typedef struct { u8 x,y,w,h,style; } Room;
static Room rooms[10]; static int nrooms;
typedef struct { u8 x,y,str; } Light;
static Light lights[64]; static int nlights;

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
    for(int i=0;i<MW*MW;i++){ dmap[i]=D_WALL; wmat[i]=M_BRICK; fmat[i]=FL_FLAG; lmap[i]=0; rid[i]=254; wdec[i]=0; }
    nrooms=0; nlights=0; nlamps=0;
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
        if((st==ST_HALL||st==ST_CRYPT) && hsh<2){ wmat[i]|=F_TORCH; add_light(f%MW,f/MW,72); if(wdec[i]!=DC_PANEL) wdec[i]=DC_NONE; }
        else if(st<0 && ((x*5+y*3)%9)==0){ wmat[i]|=F_TORCH; add_light(f%MW,f/MW,60); }
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
    // light map
    for(int k=0;k<nlights;k++){ Light *L=&lights[k];
        for(int dy=-6;dy<=6;dy++) for(int dx=-6;dx<=6;dx++){
            int x=L->x+dx, y=L->y+dy; if(x<0||y<0||x>=MW||y>=MW) continue;
            int d2=dx*dx+dy*dy; if(d2>36) continue;
            int add=L->str*(40-d2)/40, v=lmap[y*MW+x]+add; lmap[y*MW+x]= v>110?110:v; } }
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
    #define SPAWN(k_,cell_,hp_,sub_) do{ if(ndo<NDO){ DObj *o=&dob[ndo++]; int c_=(cell_); o->kind=(k_); o->alive=1; o->hp=(hp_); o->sub=(sub_); o->flash=0; o->cool=0; o->x=((c_%MW)<<16)+0x8000; o->y=((c_/MW)<<16)+0x8000; } }while(0)
    int mhp = 2 + dlv() + depth_no/2; if(mhp>60) mhp=60;
    for(int i=1;i<nrooms;i++){
        int c = 1 + (rooms[i].w*rooms[i].h>60) + ((dlv()+depth_no)/4 > 3 ? 3 : (dlv()+depth_no)/4);
        for(int k=0;k<c;k++) SPAWN(K_SPIDER, room_floor_cell(&rooms[i]), mhp, 0);
        if(rnd()&1) SPAWN(K_CHEST, room_floor_cell(&rooms[i]), 1, 0);
    }
    for(int i=0;i<nrooms;i++){
        int c = 1 + rndi(2);
        for(int k=0;k<c;k++){ int r=rndi(10), it = r<4?IT_BERRY : r<6?IT_CRUMB : r<8?IT_SEED : r<9?IT_SHELL : IT_THORN;
            int sub = it==IT_BERRY?rndi(8): it==IT_SEED?rndi(6): (it==IT_SHELL||it==IT_THORN)?rndi(4):0;
            SPAWN(K_ITEM, room_floor_cell(&rooms[i]), 1, (it<<4)|sub); }
    }
    int cages = 1 + (nrooms>5);
    for(int k=0;k<cages;k++){ int r=1+rndi(nrooms>1?nrooms-1:1); SPAWN(K_CAGE, room_floor_cell(&rooms[r<nrooms?r:0]), 1, rndi(8)<2?BUG_PILL:rndi(6)); }
    int nb = 1 + (dlv()/2>3?3:dlv()/2) + (depth_no>2);
    for(int k=0;k<nb;k++) SPAWN(K_BAT, room_floor_cell(&rooms[nrooms>1 ? 1+rndi(nrooms-1) : 0]), 1+dlv()/2, 0);
    for(int k=0;k<nlamps;k++) SPAWN(K_LAMP, lampcell[k], 1, 0);
    if(last && !zone_cleared){ SPAWN(K_BOSS, dn, 12+zone*6, 0); }
    if(endless){
        if(depth_no%10==0) SPAWN(K_BOSS, dn, 20+depth_no*2+dlv()*2, 0);               // a guardian every 10 floors
        if(depth_no%5==0 || rndi(10)<3) SPAWN(K_ITEM, room_floor_cell(&rooms[rndi(nrooms)]), 1, (IT_WARP<<4));
        int ng = 1 + rndi(2) + depth_no/8; if(ng>4) ng=4;
        for(int k=0;k<ng;k++){ int t=rndi(10)+depth_no/6; int tier = t<5?0 : t<8?1 : t<11?2 : 3;
            SPAWN(K_ITEM, room_floor_cell(&rooms[rndi(nrooms)]), 1, (IT_GEM<<4)|tier); }
        if(depth_no>=5 && rndi(8)==0) SPAWN(K_ITEM, room_floor_cell(&rooms[rndi(nrooms)]), 1, ((rndi(2)?IT_SHELL:IT_THORN)<<4)|4);
    }
}
static int solid(s32 x, s32 y){ int cx=x>>16, cy=y>>16; if(cx<0||cy<0||cx>=MW||cy>=MW) return 1; int t=dmap[cy*MW+cx]; return t==D_WALL||t==D_PILLAR; }

static u8 bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
#define HZ 89
#define FOC 182      // focal length in pixel rows
static s32 rowdist[160], cdist[160];
static u8 rowlight[160], rowfog[160], clight_[160], cfog[160];
static s32 rdx[120], rdy[120];
static s32 cperp[120]; static u8 cside[120], cmat[120], ctyp[120], clit[120]; static u16 cu[120], ccell[120]; static u8 cdec[120]; static s16 ctop[120], cbot[120];

static u8 dtab[16*128];
static s32 camtab[120];
static u8 fcode[MW*MW];
#define DITH(ramp,s,bz) ({ int _s=(s); if(_s<0)_s=0; if(_s>127)_s=127; int _l=_s>>4; if((_s&15)>(bz) && _l<7) _l++; (ramp)+_l; })

u32 prof[8];
#define TMR() (*(volatile u16*)0x04000100 | ((u32)*(volatile u16*)0x04000104<<16))
IWRAM static void render_view(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    u32 t0=TMR();
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
        cperp[c]=perp; cside[c]=side; ctyp[c]=t; zbuf[c]=perp;
        int cell=my*MW+mx; ccell[c]=cell; cmat[c]=wmat[cell]; cdec[c]= t==D_WALL ? wdec[cell] : 0;
        s32 wx = side==0 ? py_+(s32)(((long long)perp*ry)>>16) : px_+(s32)(((long long)perp*rx)>>16);
        u16 u=(u16)(wx&0xFFFF); if((side==0&&rx>0)||(side==1&&ry<0)) u=0xFFFF-u; cu[c]=u;
        int li=perp>>12; if(li>511) li=511;
        int L = light_tab[li] + ((lmap[pmy*MW+pmx]*fog_tab[li])>>6);
        L=(L*lantk)>>6; L=(L*150)/(L+90); if(side==0) L=(L*46)>>6;
        clit[c] = L>127?127:L;
        int h=(FOC<<16)/perp; ctop[c]=HZ-h; cbot[c]=HZ+h/2;
    }
    u32 t1=TMR(); prof[0]=t1-t0;
    int flick = sin_tab[(frames*23)&255]>>11;
    // pass 2: floor, two rows at a time
    for(int y=HZ+1;y<160;y+=2){
        s32 rd=rowdist[y];
        s32 fx=px_+(s32)(((long long)rd*rdx[0])>>16), fy=py_+(s32)(((long long)rd*rdy[0])>>16);
        s32 sx=(s32)(((long long)rd*(rdx[119]-rdx[0]))>>16)/119, sy=(s32)(((long long)rd*(rdy[119]-rdy[0]))>>16)/119;
        int sl=rowlight[y], fg=rowfog[y];
        const u8 *bz=bayer+((y>>1)&3)*4;
        volatile u16 *row=pg+y*120;
        for(int c=0;c<120;c++, fx+=sx, fy+=sy){
            int cb=cbot[c]; if(y+1<=cb) continue;
            u32 tx=fx>>16, ty=fy>>16; int ix;
            if(tx<MW && ty<MW){
                int i=ty*MW+tx, s=sl+((lmap[i]*fg)>>7), ex=fx&0xFFFF, ey=fy&0xFFFF, ramp;
                switch(fcode[i]){
                    case 0: ramp=R_FLOORA; if(ex<0x900||ex>0xF700||ey<0x900||ey>0xF700) s=(s*36)>>6; else s+=((((ex>>11)*7)^((ey>>11)*13)^i)&7)-3; break;
                    case 1: ramp=R_FLOORB; if(ex<0x900||ex>0xF700||ey<0x900||ey>0xF700) s=(s*36)>>6; else s+=((((ex>>11)*5)^((ey>>11)*11)^i)&7)-3; break;
                    case 2: ramp=R_TILEF; if((ex&0x7FFF)<0x600||(ey&0x7FFF)<0x600) s=(s*38)>>6; else if(((ex^ey)>>15)&1) s+=4; break;
                    case 3: ramp=R_DIRT; s += ((((ex>>12)*7) ^ ((ey>>12)*13) ^ i) & 7) - 3; break;
                    case 4: ramp=R_MOSS; s = ((s*52)>>6) + ((((ex>>13) ^ (ey>>13) ^ i) & 3)*3); break;
                    case 5: ramp=R_WATER; s = ((s*3)>>2) + 6 + (sin_tab[((fx>>10)+(fy>>11)+frames*3)&255]>>10); break;
                    case 6: ramp=R_WOOD; if((ex&0x3FFF)<0x700) s=(s*34)>>6; break;
                    case 7: ramp=R_UP; s+=24; break;
                    default: ramp=R_DOWN; s+=24; break;
                }
                if(s<0) s=0; else if(s>127) s=127;
                ix=ramp+dtab[(bz[c&3]<<7)|s];
            } else ix=R_CEIL;
            u16 p2=ix|(ix<<8);
            if(y>cb) row[c]=p2;
            row[c+120]=p2;
        }
    }
    u32 t2=TMR(); prof[1]=t2-t1;
    // pass 3: ceiling, two rows at a time
    for(int y=18;y<HZ;y+=2){
        s32 rd=cdist[y];
        s32 fx=px_+(s32)(((long long)rd*rdx[0])>>16), fy=py_+(s32)(((long long)rd*rdy[0])>>16);
        s32 sx=(s32)(((long long)rd*(rdx[119]-rdx[0]))>>16)/119, sy=(s32)(((long long)rd*(rdy[119]-rdy[0]))>>16)/119;
        int sl=clight_[y];
        const u8 *bz=bayer+((y>>1)&3)*4;
        volatile u16 *row=pg+y*120;
        int two = y+1<HZ;
        for(int c=0;c<120;c++, fx+=sx, fy+=sy){
            int ct=ctop[c]; if(y>=ct) continue;
            u32 tx=fx>>16, ty=fy>>16; int ix;
            if(tx<MW && ty<MW){
                int i=ty*MW+tx, s=sl+(lmap[i]>>1), ramp;
                if(fcode[i]>=3 && fcode[i]<=5){ ramp=R_EARTH; s=((s*3)>>2) + ((((fx>>12)^(fy>>11)^i)&3)*2); }
                else { ramp=R_CEIL; int ex=fx&0xFFFF, ey=fy&0xFFFF;
                       if(ex<0x0A00||ey<0x0A00) s=(s*34)>>6; else s += ((((fx>>11)*7)^((fy>>12)*13)^i)&7)*2-6; }
                if(s>127) s=127;
                ix=ramp+dtab[(bz[c&3]<<7)|s];
            } else ix=R_CEIL;
            u16 p2=ix|(ix<<8);
            row[c]=p2;
            if(two && y+1<ct) row[c+120]=p2;
        }
    }
    u32 t3=TMR(); prof[2]=t3-t2;
    // pass 4: walls, one column at a time (1.5 tiles tall), texture two rows at a time
    for(int c=0;c<120;c++){
        s32 perp=cperp[c];
        int top=ctop[c], bot=cbot[c], hh=bot-top; if(hh<1) hh=1;
        int L=clit[c], mat=cmat[c]&15, fl=cmat[c]&0x30;
        if(ctyp[c]==D_PILLAR && mat==M_BRICK) mat=M_MARBLE;
        int ramp0 = mat==M_BRICK?R_WALL: mat==M_EARTH?R_EARTH: mat==M_MARBLE?R_PILLAR: R_STONE;
        volatile u16 *col = pg + c;
        int y0 = top<18 ? 18 : top, y1 = bot>159 ? 159 : bot;
        int outline = (c>0 && cperp[c-1]-perp > 0x4000) || (c<119 && cperp[c+1]-perp > 0x4000);
        if(outline){ u16 p2=ramp0|(ramp0<<8); for(int y=y0;y<=y1;y++) col[y*120]=p2; continue; }
        u16 u=cu[c];
        u32 ch=(ccell[c]*2654435761u)>>24;
        int mu0 = ((u*22/10) & 0xFFFF) < 0xD00, mu1 = ((u*22/10 + 0x8000) & 0xFFFF) < 0xD00;
        int udist = u>0x8000 ? u-0x8000 : 0x8000-u;
        int band = udist<0x2800 ? 64 : udist<0x5800 ? 46 : 29;
        u32 dripb = 0x2400 + ((((u>>11)+ch)*2654435761u)>>19 & 0x3FFF);
        int wob0 = (u+ch*97);
        int beamcol = (fl&F_BEAM) && (u<0x2400||u>0xDC00);
        int dec = cdec[c];
        int sL80=L*80, sL76=L*76, sL72=L*72;
        s32 vstep = (0x10000<<4)/hh;
        s32 v = (y0-top)*vstep;
        const u8 *bz = bayer + (c&3);
        for(int y=y0; y<=y1; y+=2, v+=2*vstep){
            int vv = v>>4, s, ramp=ramp0;
            switch(mat){
            case M_BRICK: { s=(sL80-L*(vv>>11))>>6; int v7=vv*7, rw=(v7>>16)&1;
                if((v7&0xFFFF)<0x1999 || (rw?mu1:mu0)) s=(s*40)>>6; if(vv>0xF000) s=(s*35)>>6; break; }
            case M_MOSS:
            case M_STONE: { s=(sL76-L*(vv>>12))>>6; int v4=vv*4, rw=v4>>16; u32 bu=(u32)u*2+rw*0x6000;
                if((v4&0xFFFF)<0x0F00 || (bu&0xFFFF)<0x0C00) s=(s*36)>>6;
                else { s += ((((rw*31+(bu>>16)*17+ch)&3)*5))-6; if((v4&0xFFFF)<0x2800) s+=7; }
                if(mat==M_MOSS && ((u32)vv<dripb || vv>0xE400)){ ramp=R_MOSS; s=((L*66)>>6) + ((((u>>10)^(vv>>10))&3)*3); }
                break; }
            case M_EARTH: { s=(sL72-L*(vv>>12))>>6;
                if(((wob0+(sin_tab[(vv>>8)&255]>>5))&0x3FFF)<0x500) s=(s*30)>>6;
                if((((vv>>12)*29+ch)&7)==0) s+=8;
                if(beamcol){ ramp=R_WOOD; s=(L*70)>>6; if(((vv>>9)&15)==0) s=(s*44)>>6; }
                else if((fl&F_BEAM) && vv<0x1A00){ ramp=R_WOOD; s=(L*80)>>6; if(vv>0x1700) s=(s*34)>>6; }
                break; }
            case M_CRYSTAL: { s=(L*40)>>6;
                if((((u32)u*2+vv)&0x3FFF)<0x380 || (((u32)u*3-vv*2)&0x7FFF)<0x500){ ramp=R_CRYSTAL; s=70+((L*3)>>3)+(sin_tab[(frames*2+(vv>>9))&255]>>11); }
                break; }
            default: { s=(L*band)>>6; if(vv<0x1600) s=(L*58)>>6; else if(vv>0xE800) s=(L*38)>>6;
                if((vv&0x1FFF)<0x180) s=(s*44)>>6; break; }
            }
            int potion=0;
            if(dec){
                switch(dec){
                case DC_PANEL:      // raised stone frame with a sunken panel, like Daggerfall's crypt walls
                    if(u<0x0E00||u>0xF200){ ramp=R_STONE; s=(L*84)>>6; }
                    else if(u<0x1400||u>0xEC00||(vv>0x1A00&&vv<0x2000)||(vv>0xC800&&vv<0xCE00)){ s=(s*30)>>6; }
                    else if(vv>=0x2000&&vv<=0xC800) s=(s*48)>>6;
                    break;
                case DC_NICHE:      // a shelf of potion bottles set into the wall
                    if(u>0x3000&&u<0xD000&&vv>0x4800&&vv<0x7C00){
                        if(vv>0x7400){ ramp=R_STONE; s=(L*80)>>6; }
                        else { s=(s*18)>>6; int bk=-1; if(u>0x4C00&&u<0x6000) bk=0; else if(u>0x7400&&u<0x8C00) bk=1; else if(u>0xA000&&u<0xB400) bk=2;
                               if(bk>=0){ int bu = bk==0?u-0x4C00 : bk==1?u-0x7400 : u-0xA000; int wide = bk==1?0x1800:0x1400;
                                   if(vv>0x6000 && bu<wide){ potion=1; } else if(vv>0x5400 && bu>wide/3 && bu<wide*2/3){ ramp=R_STONE; s=(L*60)>>6; } } }
                    } else if(u>0x2C00&&u<0xD400&&vv>0x4400&&vv<0x8000) s=(s*34)>>6;
                    break;
                case DC_DOOR: {     // an old plank door with iron bands and a cross brace
                    if(u>0x3000&&u<0xD000&&vv>0x5000){
                        ramp=R_WOOD; s=(L*62)>>6; int pu=(u-0x3000)%0x2000; if(pu<0x280) s=(s*40)>>6;
                        if((vv>0x7200&&vv<0x7A00)||(vv>0xD800&&vv<0xE000)){ ramp=R_STONE; s=(L*40)>>6; }
                        int dv=(vv-0x5000)*0xA000/0xB000, du=u-0x3000; int dd=dv-du; if(dd<0) dd=-dd; if(dd<0x900) s=(s*80)>>6;
                        if(u>0xB800&&u<0xC400&&vv>0x9C00&&vv<0xA800){ ramp=R_FLAME; s=60; }
                    } else if(u>0x2600&&u<0xDA00&&vv>0x4400){ ramp=R_STONE; s=(L*78)>>6; }
                    break; }
                case DC_RUNE:       // a carved band of runes
                    if(vv>0x8400&&vv<0x9800){ s=(s*26)>>6; if(((u>>11)*7 ^ (vv>>11)*3)%5==0 && (u&0x7FF)>0x200) s=(L*70)>>6; }
                    break;
                default:            // a bronze plaque with a face
                    if(u>0x4000&&u<0xC000&&vv>0x2C00&&vv<0x7000){
                        ramp=R_CHEST; s=(L*72)>>6;
                        if(u>0x4C00&&u<0xB400&&vv>0x3600&&vv<0x6600){ s=(L*50)>>6;
                            int eu = u<0x8000 ? u-0x6800 : u-0x9800; if(eu<0) eu=-eu;
                            if(vv>0x4400&&vv<0x4C00&&eu<0x700) s=(L*16)>>6;
                            if(vv>0x5600&&vv<0x5C00&&u>0x7000&&u<0x9000) s=(L*20)>>6;
                            if(vv<0x3C00) s=(L*80)>>6; } }
                    break;
                }
                if(potion){ int ix0 = 216 + ((u>>13)&7); u16 pp=ix0|(ix0<<8); col[y*120]=pp; if(y<y1) col[y*120+120]=pp; continue; }
            }
            if(fl&F_TORCH){
                if(udist<0x1C00 && vv>0x5000 && vv<0x6400){ ramp=R_WOOD; s=(vv>0x5C00)?8:20; }
                else if(vv>0x2800 && vv<=0x5000 && udist < (((vv-0x2800)>>2)+0x400+(flick<<6))){ ramp=R_FLAME; s=127-((0x5000-vv)>>8)+flick; }
                else { int dy = vv>0x4400 ? vv-0x4400 : 0x4400-vv; int g = 70 - ((udist+dy)>>9); if(g>0) s+=g; }
            }
            if(s<0) s=0; else if(s>127) s=127;
            int ix = ramp+dtab[(bz[((y>>1)&3)*4]<<7)|s];
            u16 p2=ix|(ix<<8);
            col[y*120]=p2; if(y<y1) col[y*120+120]=p2;
        }
        // crisp outline on the top and bottom edges
        u16 e=ramp0|(ramp0<<8);
        if(top>=18) col[top*120]=e;
        if(bot<=159) col[bot*120]=e;
    }
    prof[3]=TMR()-t3;
}

IWRAM static void draw_sprite(volatile u16 *pg, DObj *o, s32 dirx, s32 diry, s32 plx, s32 ply){
    s32 sx=o->x-px_, sy=o->y-py_;
    s32 det = (s32)((((long long)plx*diry) - ((long long)dirx*ply))>>16);
    if(!det) return;
    s32 tX = (s32)((((long long)diry*sx) - ((long long)dirx*sy))/det);
    s32 tY = (s32)((((long long)-ply*sx) + ((long long)plx*sy))/det);
    if(tY < 0x3000) return;
    int scx = 60 + (s32)(((long long)tX*60)/tY);
    const u8 *art; int aw, ah, hw, tint=0;
    switch(o->kind){
        case K_SPIDER: art=spider_art; aw=SPIDER_W; ah=SPIDER_H; hw=0x7000; break;
        case K_BOSS: art=spider_art; aw=SPIDER_W; ah=SPIDER_H; hw=0x11000; break;
        case K_CHEST: art=chest_art; aw=CHEST_W; ah=CHEST_H; hw=0x5000; break;
        case K_BAT: art=art_bat; aw=BAT_W; ah=BAT_H; hw=0x3800; break;
        case K_LAMP: art=art_lamp; aw=LAMP_W; ah=LAMP_H; hw=0x8000; break;
        case K_CAGE: art=art_cage; aw=CAGE_W; ah=CAGE_H; hw=0x9000; tint=224+o->sub; break;
        default: { int it=o->sub>>4, sub=o->sub&15;
            switch(it){ case IT_BERRY: art=art_berry; aw=BERRY_W; ah=BERRY_H; hw=0x2C00; tint=216+sub; break;
                        case IT_SEED: art=art_seed; aw=SEED_W; ah=SEED_H; hw=0x2400; break;
                        case IT_SHELL: art=art_shell; aw=SHELL_W; ah=SHELL_H; hw=0x2C00; break;
                        case IT_THORN: art=art_thorn; aw=THORN_W; ah=THORN_H; hw=0x3800; break;
                        case IT_WARP: art=art_warp; aw=WARP_W; ah=WARP_H; hw=0x3800; break;
                        case IT_GEM: { static const u8 gc[4]={216,217,222,220}; art=art_gem; aw=GEM_W; ah=GEM_H; hw=0x2C00; tint=gc[sub&3]; break; }
                        default: art=art_crumb; aw=CRUMB_W; ah=CRUMB_H; hw=0x2400; break; } }
    }
    int hpx = (s32)(((long long)FOC*hw)/tY);
    int wcols = hpx*aw/ah/2;
    if(hpx<2||wcols<1) return;
    int bot = HZ + (FOC<<15)/tY;
    int top = bot - hpx;
    int x0 = scx - wcols/2;
    s32 ustep=(aw<<16)/wcols, vstp=(ah<<16)/hpx;
    int dark = tY>0x40000 ? 2 : tY>0x28000 ? 1 : 0;
    int c0 = x0<0?0:x0, y0 = top<18?18:top;
    s32 ua = (c0-x0)*ustep;
    for(int c=c0; c<x0+wcols && c<120; c++, ua+=ustep){
        if(tY >= zbuf[c]) continue;
        const u8 *acol = art + (ua>>16);
        s32 va = (y0-top)*vstp;
        for(int y=y0; y<bot && y<160; y+=2, va+=2*vstp){
            int ix=acol[(va>>16)*aw];
            if(!ix) continue;
            if(ix==1) ix=tint; else if(dark && ix>=64 && ix<216){ int l=(ix&7)-dark; ix=(ix&~7)+(l<0?0:l); }
            if(o->flash) ix = R_UP+7;
            u16 p2=ix|(ix<<8); pg[y*120+c]=p2; if(y+1<bot && y+1<160) pg[y*120+120+c]=p2;
        }
    }
}

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
    for(int y=0;y<160;y++){
        if(y>HZ){ rowdist[y]=(FOC<<15)/(y-HZ); int li=rowdist[y]>>12; if(li>511) li=511; rowlight[y]=light_tab[li]*rk/64; rowfog[y]=fog_tab[li]*48/64; }
        if(y<HZ){ cdist[y]=(FOC<<16)/(HZ-y); int li=cdist[y]>>12; if(li>511) li=511; clight_[y]=light_tab[li]*40/64; cfog[y]=fog_tab[li]; }
    }
    for(int c=0;c<120;c++) camtab[c]=((2*c-119)<<16)/120;
    for(int b=0;b<16;b++) for(int v=0;v<128;v++){ int l=v>>4; if((v&15)>b && l<7) l++; dtab[(b<<7)|v]=l; }
    for(int i=0;i<MW*MW;i++){ int t=dmap[i], f=fmat[i], x=i%MW, y=i/MW;
        fcode[i] = t==D_UP?7 : t==D_DOWN?8 : f==FL_FLAG?((x+y)&1) : f==FL_TILE?2 : f==FL_DIRT?3 : f==FL_MOSS?4 : f==FL_WATER?5 : 6; }
    REG_DISPCNT = 4 | (1<<10) | (1<<12) | (1<<6);
    dungeon_hud();
}
