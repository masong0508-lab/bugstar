// ================================================================= TERRACE CAVERNS (very rare)
// A floor with two real levels: an upper terrace and a deep cavern, joined by short stairways. Every cell has its own floor height,
// and the raycaster marches each ray front to back, drawing step risers, ledge caps, cliff faces, floors and a high ceiling in true 3D.
// Heights are in eighths of a cell (TU = 1/8). Floor cells: trh = floor height. Rails (low blocks): D_PILLAR with trb < TR_CEIL.
#define TU 0x2000
static void terr_cell(int x,int y,int h,int fm){ int i=y*MW+x; dmap[i]=D_FLOOR; trh[i]=h; trb[i]=0; fmat[i]=fm; }
static void terr_rail(int x,int y,int h,int top){ int i=y*MW+x; dmap[i]=D_PILLAR; wmat[i]=M_STONE; trh[i]=h; trb[i]=top; }
static void tmap(int m,int x,int y,int *ox,int *oy){ int nx=x, ny=y; if(m&1){ nx=y; ny=x; } if(m&2){ if(m&1) nx=MW-1-nx; else ny=MW-1-ny; } *ox=nx; *oy=ny; }
static void terr_xform(u8 *a,int m){ EW static u8 tmp[MW*MW]; for(int y=0;y<MW;y++) for(int x=0;x<MW;x++){ int nx,ny; tmap(m,x,y,&nx,&ny); tmp[ny*MW+nx]=a[y*MW+x]; } for(int i=0;i<MW*MW;i++) a[i]=tmp[i]; }
static void terr_build(void){
    for(int i=0;i<MW*MW;i++){ dmap[i]=D_WALL; wmat[i]=M_STONE; fmat[i]=FL_FLAG; rid[i]=254; wdec[i]=0; trh[i]=0; trb[i]=TR_CEIL; }
    nlights=0; nlamps=0; ntorch=0;
    for(int y=1;y<=9;y++) for(int x=1;x<=30;x++){ terr_cell(x,y,8,FL_TILE); rid[y*MW+x]=0; }           // the upper terrace
    for(int y=11;y<=30;y++) for(int x=1;x<=30;x++){ int moss=((x*7+y*3)%11)==0; terr_cell(x,y,0,moss?FL_MOSS:((x*5+y)%7==0?FL_DIRT:FL_FLAG)); rid[y*MW+x]=(y>=13)?1:255; }   // the cavern floor
    for(int x=1;x<=30;x++){ terr_rail(x,10,8,11); rid[10*MW+x]=255; }                                // the cliff edge: a low stone rail you can see over
    int sa[2]={3+rndi(8), 17+rndi(9)};
    for(int s=0;s<2;s++){ int a=sa[s];
        for(int k=0;k<3;k++){ terr_cell(a+k,10,6,FL_TILE); terr_cell(a+k,11,4,FL_TILE); terr_cell(a+k,12,2,FL_TILE); rid[10*MW+a+k]=rid[11*MW+a+k]=rid[12*MW+a+k]=255; }
        terr_rail(a-1,11,4,7); terr_rail(a+3,11,4,7); terr_rail(a-1,12,2,5); terr_rail(a+3,12,2,5); }   // banisters: you cannot step up the side of a stair
    for(int k=0;k<7;k++){ int x=2+rndi(28), y=2+rndi(7), i=y*MW+x; if(dmap[i]==D_FLOOR){ dmap[i]=D_PILLAR; wmat[i]=M_MARBLE; trb[i]=TR_CEIL; } }     // pillars on the terrace
    for(int k=0;k<18;k++){ int x=2+rndi(28), y=15+rndi(15), i=y*MW+x; if(dmap[i]==D_FLOOR){ dmap[i]=D_PILLAR; wmat[i]=(k&1)?M_STONE:M_MARBLE; trb[i]=TR_CEIL; } }   // tall columns holding up the cavern roof
    int m=rndi(4);
    terr_xform(dmap,m); terr_xform(wmat,m); terr_xform(fmat,m); terr_xform(rid,m); terr_xform(trh,m); terr_xform(trb,m);
    static const u8 rr[2][4]={{1,1,30,9},{1,13,30,18}};
    for(int r=0;r<2;r++){ int x0,y0,x1,y1; tmap(m,rr[r][0],rr[r][1],&x0,&y0); tmap(m,rr[r][0]+rr[r][2]-1,rr[r][1]+rr[r][3]-1,&x1,&y1);
        int ax=x0<x1?x0:x1, ay=y0<y1?y0:y1, bx=x0<x1?x1:x0, by=y0<y1?y1:y0;
        Room *R=&rooms[r]; R->x=ax; R->y=ay; R->w=bx-ax+1; R->h=by-ay+1; R->style=ST_CRYPT;
        int cols=R->w/9, rows=R->h/9; if(cols<1) cols=1; if(rows<1) rows=1;
        for(int j=0;j<rows;j++) for(int i=0;i<cols;i++){ int lx=R->x+(2*i+1)*R->w/(2*cols), ly=R->y+(2*j+1)*R->h/(2*rows);
            for(int t=0;t<9 && dmap[ly*MW+lx]!=D_FLOOR;t++){ lx+=(t&1)?-(t/2+1):(t/2+1); if(lx<1) lx=1; if(lx>30) lx=30; }
            if(dmap[ly*MW+lx]==D_FLOOR) add_light(lx,ly,r?58:66); } }
    nrooms=2; terr_snap=1;
    say("A TERRACED CAVERN. TWO LEVELS"); msg_timer=70;
}
static int terr_roll(int bossfl,int mz_last,int mboss){            // about 1 floor in 33, from the second floor on, never a boss or outpost floor
    if(base_mode||forge_on||mz_last||bossfl||mboss||depth_no<2) return 0;
    if(endless && hive_boss_floor()) return 0;
    return (hmix(rng_s^0x7E44ACE5u^((u32)depth_no*0x9E3779B1u)) % 100) < 3;
}
static u32 invt[161];
static inline void zset(int y,int c,s32 t){ int z=t>>13; zpix[y*120+c]= z>255?255:z; }
// one floor pixel at row y, depth t along this column's ray
static int terr_floor_px(int c,int y,s32 t,s32 rx,s32 ry){
    s32 fx=px_+(s32)(((long long)t*rx)>>16), fy=py_+(s32)(((long long)t*ry)>>16);
    u32 tx=fx>>16, ty=fy>>16; if(tx>=MW||ty>=MW) return R_CEIL;
    int fc=fcode[ty*MW+tx], tt=floor_tex[(fc<<10)|((fy>>6)&0x3E0)|((fx>>11)&31)];
    int e=framp[(fc<<3)|(tt>>5)], s; if(e&0x100) s=(tt&31)<<2;
    else { int li=t>>12; if(li>511) li=511; int Lp=((light_tab[li]*44)>>6)+((lsub[((fy>>7)&0x3F80)|((fx>>14)&127)]*((fog_tab[li]*52)>>6))>>6); if(Lp>127) Lp=127; s=((tt&31)*Lp)>>5; }
    return (e&0xFF)+lvt[s+bayi[((y>>1)&3)*4+(c&3)]];
}
static int terr_ceil_px(int c,int y,s32 t,s32 rx,s32 ry){
    s32 fx=px_+(s32)(((long long)t*rx)>>16), fy=py_+(s32)(((long long)t*ry)>>16);
    u32 tx=fx>>16, ty=fy>>16; if(tx>=MW||ty>=MW) return R_CEIL;
    int fc=fcode[ty*MW+tx], cc=((fc>=3&&fc<=5)||fc==9), tt=ceil_tex[(cc<<10)|((fy>>6)&0x3E0)|((fx>>11)&31)], e=cramp[(cc<<3)|(tt>>5)];
    int li=t>>12; if(li>511) li=511; int Lp=((light_tab[li]*36)>>6)+((lsub[((fy>>7)&0x3F80)|((fx>>14)&127)]*fog_tab[li])>>7); if(Lp>127) Lp=127;
    return (e&0xFF)+lvt[((tt&31)*Lp>>5)+bayi[((y>>1)&3)*4+(c&3)]];
}
static void render_terrace(volatile u16 *pg, s32 dirx, s32 diry, s32 plx, s32 ply){
    const int H=hz; const s32 CH=(s32)TR_CEIL*TU;
    if(!invt[1]) for(int d=1;d<=160;d++) invt[d]=0xFFFFFFFFu/(u32)d;
    { int pc=(py_>>16)*MW+(px_>>16); s32 tg=(s32)trh[pc]*TU+0x8000; if(terr_snap){ t_eye=tg; terr_snap=0; } else t_eye+=(tg-t_eye)>>2; }
    const s32 E=t_eye;
    for(int c=0;c<120;c++){
        s32 cam=camtab[c];
        s32 rx=dirx+(s32)(((long long)plx*cam)>>16), ry=diry+(s32)(((long long)ply*cam)>>16);
        rdx[c]=rx; rdy[c]=ry;
        int mx=px_>>16, my=py_>>16;
        u32 arx=rx<0?-rx:rx, ary=ry<0?-ry:ry;
        u32 qx = arx>2? 0xFFFFFFFFu/arx : 0x7fffffff; if(qx>0x7fffffff) qx=0x7fffffff;
        u32 qy = ary>2? 0xFFFFFFFFu/ary : 0x7fffffff; if(qy>0x7fffffff) qy=0x7fffffff;
        s32 ddx=(s32)qx, ddy=(s32)qy; int sx=rx<0?-1:1, sy=ry<0?-1:1;
        s32 fx0=px_&0xFFFF, fy0=py_&0xFFFF;
        s32 tx=rx<0 ? (s32)(((long long)fx0*ddx)>>16) : (s32)(((long long)(0x10000-fx0)*ddx)>>16);
        s32 ty=ry<0 ? (s32)(((long long)fy0*ddy)>>16) : (s32)(((long long)(0x10000-fy0)*ddy)>>16);
        int side=0, ylo=160, k=0, done=0;
        s32 fp=(s32)trh[my*MW+mx]*TU, dn=0x1000;
        zbuf[c]=0x7FFFFFFF;
        for(;;){
            int cell=my*MW+mx, t=dmap[cell];
            s32 df = tx<ty?tx:ty;
            int full=(t==D_WALL||t==D_LOCK||(t==D_PILLAR&&trb[cell]>=TR_CEIL)), rail=(t==D_PILLAR&&!full);
            s32 ht = full ? CH : rail ? (s32)trb[cell]*TU : (s32)trh[cell]*TU;
            if(k>0 && ht>fp){                                         // a wall, a cliff face or a step riser at the near edge of this cell
                int ya=H+(int)(((long long)(E-ht)*FOC)/dn), yb=H+(int)(((long long)(E-fp)*FOC)/dn);
                int y0=ya<18?18:ya, y1=yb<ylo?yb:ylo;
                if(y0<y1){
                    s32 hx=px_+(s32)(((long long)dn*rx)>>16), hy=py_+(s32)(((long long)dn*ry)>>16);
                    u32 u=(u32)((side==0?hy:hx)&0xFFFF); if((side==0&&rx>0)||(side==1&&ry<0)) u=0xFFFF-u;
                    int w = rail ? WT_STONE : (t==D_PILLAR||t==D_WALL||t==D_LOCK) ? wtex[cell] : (dmap[cell]==D_FLOOR ? WT_STONE : WT_STONE);
                    int tu=u>>11; if(w<=WT_CRYSTAL && ((cell*7+side)&1)) tu=31-tu;
                    const u8 *tcol=wall_tex+w*1536+tu*48; const u16 *rp=wramp+w*8;
                    int lx=(hx-((rx*5)>>4))>>14, ly=(hy-((ry*5)>>4))>>14; if(lx<0)lx=0; if(lx>127)lx=127; if(ly<0)ly=0; if(ly>127)ly=127;
                    int li=dn>>12; if(li>511) li=511;
                    int L=((light_tab[li]*lantk)>>6)+((lsub[(ly<<7)|lx]*fog_tab[li])>>6); L=(L*150)/(L+70); if(side==0) L=(L*48)>>6; L+=((cell*37)&7)-3; if(L<0) L=0; if(L>127) L=127;
                    s32 sh=dn/FOC, h0=E-(y0-H)*sh; const s32 D48=48<<16; s32 vp=((h0-fp)*32)%D48; if(vp<0) vp+=D48; s32 dvp=sh*32;
                    const u8 *bz=bayer+(c&3);
                    for(int y=y0;y<y1;y+=2){
                        int tt=tcol[47-(vp>>16)]; int e=rp[tt>>5];
                        int s=(e&0x100)?(tt&31)<<2:((tt&31)*L)>>5; int ix=(e&0xFF)+lvt[s+bz[((y>>1)&3)<<2]]; u16 p2=ix|(ix<<8);
                        pg[y*120+c]=p2; zset(y,c,dn); if(y+1<y1){ pg[y*120+120+c]=p2; zset(y+1,c,dn); }
                        vp-=dvp*2; while(vp<0) vp+=D48;
                    }
                }
                if(y0<ylo) ylo=y0;
            }
            if(full){
                zbuf[c]=dn; seen[cell]=1; done=1;
                for(int y=18;y<ylo;y+=2){ int d=H-y; if(d<1) break; s32 tt=(s32)((((u64)(u32)((CH-E)*FOC))*invt[d])>>32);      // the high ceiling
                    int ix=terr_ceil_px(c,y,tt,rx,ry); u16 p2=ix|(ix<<8); pg[y*120+c]=p2; zset(y,c,tt); if(y+1<ylo){ pg[y*120+120+c]=p2; zset(y+1,c,tt); } }
                break;
            }
            if(ht<E){                                                  // the floor, or the cap of a low block, from its far edge to its near edge
                int yfar=H+(int)(((long long)(E-ht)*FOC)/df), ynear=H+(int)(((long long)(E-ht)*FOC)/dn);
                int y0=yfar<18?18:yfar, y1=ynear<ylo?ynear:ylo;
                if(yfar<=H) y0=H+1;
                int last=R_CEIL;
                for(int y=y0;y<y1;y++){ int d=y-H; if(d<1||d>160) continue; int ix;
                    s32 tt=(s32)((((u64)(u32)((E-ht)*FOC))*invt[d])>>32);
                    if((y&1) && y>y0) ix=last; else ix=terr_floor_px(c,y,tt,rx,ry);
                    last=ix; pg[y*120+c]=ix|(ix<<8); zset(y,c,tt); }
                if(y0<y1 && y0<ylo) ylo=y0;
            }
            fp=ht;
            if(tx<ty){ dn=tx; tx+=ddx; mx+=sx; side=0; } else { dn=ty; ty+=ddy; my+=sy; side=1; }
            if(dn<0x1000) dn=0x1000;
            k++; if(k>=64 || mx<0||my<0||mx>=MW||my>=MW) break;
        }
        if(!done) for(int y=18;y<ylo;y++){ pg[y*120+c]=0; zpix[y*120+c]=255; }   // anything the ray never reached
    }
}
