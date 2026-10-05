// itembake.h - HOST-ONLY item art + rasteriser (never compiled into the GBA ROM; tools/bake_items.sh turns it into itemrom.h).
// BORE room items as true 2:1 isometric sprite art (crate, rail, fridge, toilet, door mat, skateboard, spawn marker).
// Each item is a few textured boxes, rendered ONCE on the PC into small sprites that live in ROM (itemrom.h). They cost NO RAM at all.
// Local frame of an item: a = across, b = front (+b faces the viewer at r=0),
// 8 units per tile, 1 unit = 1 px wide, z in px. Textures are authored as seen from OUTSIDE the face, left to right, top to bottom.
// Letters a..h index the material palette, '.' = see-through.
typedef struct { const char*const*rows; u8 w,h; const u16*pal; } Mat;
typedef struct { u8 a0,b0,a1,b1,z0,z1; const Mat*m[5]; } IBox;   // faces: 0 front(+b) 1 +a 2 back 3 -a 4 top
typedef struct { const IBox*b; u8 n; } IObj;
#define MAT(nm,pal,w,h,...) static const char*const nm##_r[]={__VA_ARGS__}; static const Mat nm={nm##_r,w,h,pal};

// ---- crate (2 stacked wooden crates) ----
static const u16 pCr[4]={RGB(8,4,2),RGB(24,16,7),RGB(17,10,4),RGB(29,21,10)};
#define CR8 "aaaaaaaa","acbbbbca","abcbbcba","abbccbba","abbccbba","abcbbcba","acbbbbca","aaaaaaaa"
MAT(mCrS,pCr,8,16,CR8,CR8)
MAT(mCrT,pCr,8,8,"aaaaaaaa","adddddda","abbbbbba","acccccca","abbbbbba","abbbbbba","acccccca","aaaaaaaa")
static const IBox bxCrate[1]={{0,0,8,8,0,16,{&mCrS,&mCrS,&mCrS,&mCrS,&mCrT}}};

// ---- fridge (front gets the doors and handles) ----
static const u16 pFr[5]={RGB(17,18,21),RGB(31,31,31),RGB(23,24,27),RGB(7,8,12),RGB(28,6,6)};
MAT(mFrF,pFr,6,16,"aaaaaa","abbbba","abbbba","abbdda","abbdda","abbbba","acccca","abbbba","abbdda","abedda","abbdda","abbdda","abbbba","abbbba","abbbba","aaaaaa")
MAT(mFrS,pFr,6,16,"aaaaaa","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","abbbba","aaaaaa")
MAT(mFrT,pFr,6,6,"aaaaaa","abbbba","abbbba","abbbba","abbbba","aaaaaa")
static const IBox bxFridge[1]={{1,1,7,7,0,16,{&mFrF,&mFrS,&mFrS,&mFrS,&mFrT}}};

// ---- toilet: pedestal, bowl, tank at the back ----
static const u16 pTo[5]={RGB(17,18,22),RGB(31,31,31),RGB(23,24,26),RGB(9,17,27),RGB(12,13,16)};
MAT(mToBase,pTo,1,1,"c")
MAT(mToBowl,pTo,4,3,"bbbb","bbbb","cccc")
MAT(mToBowlT,pTo,4,4,"bbbb","bddb","bddb","bbbb")
MAT(mToTankF,pTo,4,5,"bbbb","bbbb","bbbb","bbbb","cccc")
MAT(mToTankS,pTo,2,5,"bb","bb","bb","bb","cc")
MAT(mToTankT,pTo,4,2,"bbeb","bbbb")
static const IBox bxToilet[3]={
 {3,4,5,6,0,2,{&mToBase,&mToBase,&mToBase,&mToBase,&mToBase}},
 {2,3,6,7,2,5,{&mToBowl,&mToBowl,&mToBowl,&mToBowl,&mToBowlT}},
 {2,1,6,3,3,8,{&mToTankF,&mToTankS,&mToTankF,&mToTankS,&mToTankT}} };

// ---- grind rail, kicker ramp, quarter pipe, ledge, bench: generated art (tools/make_skate_items.py) ----
#include "skateart.h"
// ---- bed, shower, sofa: the life-sim furniture ----
#include "simart.h"

// ---- door mat / threshold ----
static const u16 pDr[4]={RGB(8,5,3),RGB(18,11,6),RGB(24,16,8),RGB(28,20,10)};
MAT(mDrSide,pDr,1,1,"a")
MAT(mDrTop,pDr,8,8,"aaaaaaaa","abbbbbba","abccccba","abcddcba","abcddcba","abccccba","abbbbbba","aaaaaaaa")
static const IBox bxDoor[1]={{0,0,8,8,0,1,{&mDrSide,&mDrSide,&mDrSide,&mDrSide,&mDrTop}}};

// ---- skateboard pickup ----
static const u16 pBd[3]={RGB(6,6,9),RGB(30,13,5),RGB(31,30,26)};
MAT(mBdEdge,pBd,1,1,"b")
MAT(mBdTop,pBd,4,6,"bbbb","baab","baab","baab","baab","bbbb")
MAT(mBdWh,pBd,1,1,"c")
static const IBox bxBoard[5]={
 {2,2,3,3,2,3,{&mBdWh,&mBdWh,&mBdWh,&mBdWh,&mBdWh}}, {5,2,6,3,2,3,{&mBdWh,&mBdWh,&mBdWh,&mBdWh,&mBdWh}},
 {2,5,3,6,2,3,{&mBdWh,&mBdWh,&mBdWh,&mBdWh,&mBdWh}}, {5,5,6,6,2,3,{&mBdWh,&mBdWh,&mBdWh,&mBdWh,&mBdWh}},
 {2,1,6,7,3,4,{&mBdEdge,&mBdEdge,&mBdEdge,&mBdEdge,&mBdTop}} };

#include "itemids.h"
static u16 bakeBuf[NIV][IH][IW];
static u16 itemTmp[IH][IW];
static void rotPt(int r,int a,int b,int*u,int*v){
    switch(r){ case 0:*u=a;*v=b;break; case 1:*u=b;*v=8-a;break; case 2:*u=8-a;*v=8-b;break; default:*u=8-b;*v=a; }
}
static inline u16 matPx(const Mat*m,int col,int row,int n){
    if(col>=m->w) col=m->w-1; if(row>=m->h) row=m->h-1; if(col<0) col=0; if(row<0) row=0;
    char ch=m->rows[row][col]; if(ch=='.') return IKEY; return shade(m->pal[ch-'a'],n);
}
typedef struct { int u0,v0,u1,v1,z0,z1; const IBox*s; } VBox;
static void drawBox(u16 (*d)[IW],const VBox*q,int r){
    const IBox*bx=q->s; int u0=q->u0,u1=q->u1,v0=q->v0,v1=q->v1,z1=q->z1, HT=bx->z1-bx->z0;
    const Mat*ml=bx->m[(4-r)&3], *mr=bx->m[(5-r)&3], *mt=bx->m[4];   // view +v face = local face (0-r), view +u face = local face (1-r)
    for(int u=u0;u<u1;u++){   // left face
        int X=IOX+u-v1, yh=IOY-4+((u+v1)>>1)-z1;
        for(int j=0;j<HT;j++){ int y=yh+1+j; u16 c=matPx(ml,u-u0,j,12); if(c!=IKEY&&(unsigned)X<IW&&(unsigned)y<IH) d[y][X]=c; }
    }
    for(int v=v1;v>=v0;v--){  // right face (the last column is the tip)
        int X=IOX+u1-v, yh=IOY-4+((u1+v)>>1)-z1;
        for(int j=0;j<HT;j++){ int y=yh+1+j; u16 c=matPx(mr,v1-v,j,9); if(c!=IKEY&&(unsigned)X<IW&&(unsigned)y<IH) d[y][X]=c; }
    }
    for(int X=u0-v1;X<=u1-v0;X++){   // top face
        int vlo=v0>u0-X?v0:u0-X, vhi=v1<u1-X?v1:u1-X, ylo=(2*vlo+X+1)>>1, yhi=(2*vhi+X)>>1;
        for(int Y=ylo;Y<=yhi;Y++){
            int v2=2*Y-X, cv=v2>>1, cu=(v2+2*X)>>1;
            if(cu<u0) cu=u0; if(cu>u1-1) cu=u1-1; if(cv<v0) cv=v0; if(cv>v1-1) cv=v1-1;
            int la,lb; switch(r){ case 0:la=cu;lb=cv;break; case 1:la=7-cv;lb=cu;break; case 2:la=7-cu;lb=7-cv;break; default:la=cv;lb=7-cu; }
            u16 c=matPx(mt,la-bx->a0,lb-bx->b0,16);
            int x=IOX+X, y=IOY-4+Y-z1; if(c!=IKEY&&(unsigned)x<IW&&(unsigned)y<IH) d[y][x]=c;
        }
    }
}
static int boxBehind(const VBox*A,const VBox*B){ return A->u1<=B->u0||A->v1<=B->v0||A->z1<=B->z0; }
static void drawObj(u16 (*d)[IW],const IBox*b,int n,int r,int zoff){
    VBox q[10]; int done=0;
    for(int i=0;i<n;i++){
        int ua,va,ub,vb; rotPt(r,b[i].a0,b[i].b0,&ua,&va); rotPt(r,b[i].a1,b[i].b1,&ub,&vb);
        q[i].u0=ua<ub?ua:ub; q[i].u1=ua<ub?ub:ua; q[i].v0=va<vb?va:vb; q[i].v1=va<vb?vb:va;
        q[i].z0=b[i].z0+zoff; q[i].z1=b[i].z1+zoff; q[i].s=&b[i];
    }
    while(done<n){   // painter order: a box waits for any box that must be drawn before it
        int pick=-1;
        for(int i=0;i<n&&pick<0;i++){ if(!q[i].s) continue; int ok=1;
            for(int j=0;j<n;j++){ if(j==i||!q[j].s) continue; if(boxBehind(&q[j],&q[i])&&!boxBehind(&q[i],&q[j])) { ok=0; break; } }
            if(ok) pick=i; }
        if(pick<0) for(int i=0;i<n;i++) if(q[i].s){ pick=i; break; }
        drawBox(d,&q[pick],r); q[pick].s=0; done++;
    }
}
static void outlineSpr(u16 (*d)[IW],int nsh){   // tonal outline on the silhouette, like the voxel cubes had
    for(int y=0;y<IH;y++)for(int x=0;x<IW;x++) itemTmp[y][x]=d[y][x];
    for(int y=0;y<IH;y++)for(int x=0;x<IW;x++){
        if(itemTmp[y][x]==IKEY) continue;
        int e=(x==0||itemTmp[y][x-1]==IKEY)||(x==IW-1||itemTmp[y][x+1]==IKEY)||(y==0||itemTmp[y-1][x]==IKEY)||(y==IH-1||itemTmp[y+1][x]==IKEY);
        if(e) d[y][x]=shade(itemTmp[y][x],nsh);
    }
}
static void bakeOne(int k,const IBox*b,int n,int r,int nsh){
    u16 (*d)[IW]=bakeBuf[k]; for(int y=0;y<IH;y++)for(int x=0;x<IW;x++) d[y][x]=IKEY;
    drawObj(d,b,n,r,0); if(nsh<16) outlineSpr(d,nsh);
}
static void bakeAll(void){
    bakeOne(V_CRATE,bxCrate,1,0,11);
    for(int r=0;r<4;r++){ bakeOne(V_FRIDGE+r,bxFridge,1,r,11); bakeOne(V_TOILET+r,bxToilet,3,r,11);
        bakeOne(V_BED+r,bxBed,4,r,11); bakeOne(V_SHOWER+r,bxShower,4,r,11); bakeOne(V_SOFA+r,bxSofa,4,r,11); }
    bakeOne(V_RAILU,bxRailU,3,0,16); bakeOne(V_RAILV,bxRailV,3,0,16);
    bakeOne(V_DOOR,bxDoor,1,0,13); bakeOne(V_BOARD,bxBoard,5,0,12);
    for(int r=0;r<4;r++){ bakeOne(V_KICKER+r,bxKicker,8,r,11); bakeOne(V_QPIPE+r,bxQuarterPipe,8,r,11); }
    bakeOne(V_LEDGEU,bxLedgeU,1,0,12); bakeOne(V_LEDGEV,bxLedgeV,1,0,12); bakeOne(V_BENCHU,bxBenchU,3,0,12); bakeOne(V_BENCHV,bxBenchV,3,0,12);
    for(int r=0;r<4;r++) bakeOne(V_LAUNCH+r,bxLaunch,8,r,11);   // skate pack 2
    bakeOne(V_FUNBOX,bxFunbox,1,0,12); bakeOne(V_BARREL,bxBarrel,1,0,11); bakeOne(V_TRASH,bxTrashCan,2,0,11); bakeOne(V_PLANTER,bxPlanter,1,0,11);
    bakeOne(V_PICNIC,bxPicnicTable,7,0,12); bakeOne(V_JERSEYU,bxJerseyU,3,0,12); bakeOne(V_JERSEYV,bxJerseyV,3,0,12); bakeOne(V_MPAD,bxManualPad,1,0,12);
}
