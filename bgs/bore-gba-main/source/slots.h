// slots.h - ROOM SLOTS: several named saves of a room, the person living in it and their life.
//
// WHAT A SLOT HOLDS      Chunks, each optional: ROOM (tiles, floors, wallpaper, run-length packed), PERSON (the creature: look, and
//                        its hand-built blocks if any), LIFE (needs, cash, job, clock, skill: the same 24 bytes sims.h saves).
// WHERE                  SRAM 20480.., SLOT_N slots of SLOT_SZ bytes (the old single room at 0, settings, jukebox and the life keep their places).
// ACTIVE SLOT            The slot you saved to / loaded last. A tiny block at SLOT_DIR remembers it. Options use it:
//                        SAVE MAP TO SLOT (the map editor's SAVE MAP also writes the active slot), BOOT LOADS PERSON.
//
// ---- THE FORMAT (made so a whole HOUSE can be added later without breaking any save) -------------------------------------
//  header (SLOT_HDR = 32 bytes)   0 'S' 1 'V' 2 version(1) 3 KIND 4 SPAN 5 has-mask 6..7 payload length 8..9 payload checksum
//                                 10 map width 11 map height 12..13 save counter 14..15 reserved (house: room count)
//                                 16..25 name 26..30 reserved 31 header checksum
//  KIND   0 = ROOM (one room).  1 = HOUSE (reserved: several rooms and how they connect, see "HOUSES" below).
//  SPAN   how many consecutive slots the save covers. A room is 1. A house spans several slots: its payload simply runs on
//         into the next slots' bytes, and the scan (slScan) skips the slots it covers.
//  payload   a list of chunks:  tag(1) length(2) data ...  and a 0 tag at the end. Readers SKIP tags they do not know, so new
//         chunk types can be added later. Tags: 'R' room, 'C' person, 'L' life, 'H' house plan (reserved).
//  'R' chunk  room index(1) format(1 = runs) then runs of  count, tile, floor<<4|wallpaper.  A ROOM slot has room index 0.
//  Every save is verified (checksum) before it is loaded, and the slot is invalidated while it is being written, so a power
//  cut in the middle of a save can only lose that one slot, never corrupt another.
//
// ---- HOUSES (not done yet: where to start) ----------------------------------------------------------------------------
//  A house = KIND 1, SPAN n, one 'R' chunk per room (room index 0..), plus an 'H' chunk: for each room its name, its position
//  in the house and its doors (which tile of which room leads to which tile of which room). Write houseSave()/houseLoad() below,
//  set SLOT_HOUSE_READY to 1, and the slot screen already lists, deletes and protects house slots.
//  The room editor edits one room at a time; a house load would copy room 0 into lifeMap and keep the rest until a door is used.
//
// Needs before it: the map (lifeMap/floorMap/wallMap, MW, MH, palIdx, palCol, flFlat), the creature (look, vox, dec, custom, buildLook,
// setColors), sims.h (simsPack/simsCheck/simsUnpack/simsSaveNow), SRAM_BASE, mapSave/mapScan, and the UI kit (box, menu, helpScreen, toast).
#define SLOT_BASE  20480
#define SLOT_N     6
#define SLOT_SZ    2048
#define SLOT_HDR   32
#define SLOT_NAME  10
#define SLOT_DIR   12352      // 'S' 'D' active-slot checksum   (4 bytes; the jukebox block before it ends at 12325)
#define SRAM_TEST  18432      // 16 spare bytes the SAVE TEST in the options writes to
#define SLOT_HOUSE_READY 0
_Static_assert(SLOT_BASE+SLOT_N*SLOT_SZ<=32768,"the slots do not fit in 32 KB of SRAM");
_Static_assert(NFL<=16&&NWP<=16,"a run stores floor and wallpaper in one byte (4 bits each)");

enum { SLK_ROOM=0, SLK_HOUSE=1 };
enum { SLH_ROOM=1, SLH_PERSON=2, SLH_LIFE=4 };                 // what a slot holds / what to load
enum { SLE_OK=0, SLE_EMPTY=-1, SLE_BAD=-2, SLE_BIG=-3, SLE_HOUSE=-4, SLE_FMT=-5, SLE_SIZE=-6, SLE_NOPART=-7, SLE_NOSRAM=-8 };
#define SLC_ROOM   'R'
#define SLC_PERSON 'C'
#define SLC_LIFE   'L'
#define SLC_HOUSE  'H'
#define CNV (H*D*W)                                            // voxels in the creature (192)
#define SLOT_NSPR ((int)(sizeof(spr)/sizeof(spr[0])))

// ---------- byte writer (a counting dry run when p is 0) and reader ----------
typedef struct { volatile u8*p; int pos,cap,over; u32 s1,s2; } SlW;
static void slwInit(SlW*w,volatile u8*p,int cap){ w->p=p; w->pos=0; w->cap=cap; w->over=0; w->s1=w->s2=0; }
static void slwPut(SlW*w,int b){
    if(w->pos>=w->cap){ w->over=1; return; }
    b&=255; if(w->p) w->p[w->pos]=(u8)b; w->pos++;
    w->s1+=(u32)b; if(w->s1>=255) w->s1-=255; w->s2+=w->s1; if(w->s2>=255) w->s2-=255;   // Fletcher-16
}
static void slwPut16(SlW*w,int v){ slwPut(w,v&255); slwPut(w,(v>>8)&255); }
static int slwSum(const SlW*w){ return (int)(w->s1|(w->s2<<8)); }
typedef struct { volatile u8*p; int pos,len,bad; } SlR;
static int slrGet(SlR*r){ if(r->pos>=r->len){ r->bad=1; return 0; } return r->p[r->pos++]; }
static int slrGet16(SlR*r){ int a=slrGet(r); int b=slrGet(r); return a|(b<<8); }
static int slSumOf(volatile u8*p,int n){ u32 s1=0,s2=0; for(int i=0;i<n;i++){ s1+=p[i]; if(s1>=255) s1-=255; s2+=s1; if(s2>=255) s2-=255; } return (int)(s1|(s2<<8)); }

// ---------- chunks: ROOM ----------
static void slEncRoom(SlW*w){
    slwPut(w,0);   // room index: 0 = the one room of a ROOM slot (a house numbers its rooms)
    slwPut(w,1);   // format 1: runs of (count, tile, floor<<4 | wallpaper)
    int n=0; u8 t=0,f=0;
    for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){
        u8 tt=(u8)lifeMap[y][x], ff=(u8)(((floorMap[y][x]&15)<<4)|(wallMap[y][x]&15));
        if(n>0&&tt==t&&ff==f&&n<255){ n++; continue; }
        if(n){ slwPut(w,n); slwPut(w,t); slwPut(w,f); }
        n=1; t=tt; f=ff;
    }
    if(n){ slwPut(w,n); slwPut(w,t); slwPut(w,f); }
}
// returns 1 if fine (or a room index we do not use), 0 if broken. apply=1 writes the map.
static int slDecRoom(SlR*c,int apply){
    int idx=slrGet(c), fmt=slrGet(c); if(c->bad) return 0;
    if(idx!=0) return 1;      // another room of a house: not used by a ROOM load
    if(fmt!=1) return 0;
    int i=0;
    while(c->pos<c->len){
        int n=slrGet(c), t=slrGet(c), f=slrGet(c); if(c->bad||n==0) return 0;
        if(palIdx((char)t)<0||(f>>4)>=NFL||(f&15)>=NWP) return 0;
        if(i+n>MSZ) return 0;
        if(apply) for(int k=0;k<n;k++){ int y=(i+k)/MW, x=(i+k)%MW; lifeMap[y][x]=(char)t; floorMap[y][x]=(u8)(f>>4); wallMap[y][x]=(u8)(f&15); }
        i+=n;
    }
    return i==MSZ;
}
// ---------- chunks: PERSON ----------
static void slEncPlane(SlW*w,int plane){   // 0 = voxels, 1 = face sprite low byte, 2 = face sprite high byte; run-length packed
    const u8*v=&vox[0][0][0]; const u16*d=&dec[0][0][0]; int n=0; u8 cur=0;
    for(int i=0;i<CNV;i++){
        u8 b=plane==0?v[i]:plane==1?(u8)(d[i]&255):(u8)(d[i]>>8);
        if(n&&b==cur&&n<255){ n++; continue; }
        if(n){ slwPut(w,n); slwPut(w,cur); }
        n=1; cur=b;
    }
    if(n){ slwPut(w,n); slwPut(w,cur); }
}
static void slEncPerson(SlW*w){
    slwPut(w,2);                                            // format 2 (format 1 had no life stage: those people are adults)
    for(int i=0;i<LK_N;i++) slwPut(w,look[i]);
    slwPut(w,stage); slwPut(w,ageDays);
    slwPut(w,custom?1:0);                                   // hand-built blocks: only then the blocks are stored (else buildLook() remakes them)
    if(custom){ slEncPlane(w,0); slEncPlane(w,1); slEncPlane(w,2); }
}
static u8 slTmp[3][CNV];                          // decoded planes, checked before anything is applied
static int slDecPlane(SlR*c,int plane){
    int i=0;
    while(i<CNV){ int n=slrGet(c), v=slrGet(c); if(c->bad||n==0||i+n>CNV) return 0; for(int k=0;k<n;k++) slTmp[plane][i+k]=(u8)v; i+=n; }
    return 1;
}
static int slDecPerson(SlR*c,int apply){
    int fmt=slrGet(c); if(c->bad||(fmt!=1&&fmt!=2)) return 0;
    u8 lk[LK_N]; for(int i=0;i<LK_N;i++) lk[i]=(u8)slrGet(c);
    int stg=AG_ADULT, agd=0; if(fmt==2){ stg=slrGet(c); agd=slrGet(c); }
    if(c->bad||stg>=AG_N) return 0;
    int cu=slrGet(c); if(c->bad) return 0;
    if(lk[LK_SHAPE]>=4||lk[LK_SKIN]>=NSW||lk[LK_EYES]>=3||lk[LK_MOUTH]>=3||lk[LK_EARS]>=3||lk[LK_HSTYLE]>=4||lk[LK_HCOL]>=NSW||lk[LK_TOP]>=NSW||lk[LK_BOT]>=NSW) return 0;
    if(cu>1) return 0;
    if(cu){
        if(!slDecPlane(c,0)||!slDecPlane(c,1)||!slDecPlane(c,2)) return 0;
        for(int i=0;i<CNV;i++){
            int vx=slTmp[0][i], dc=slTmp[1][i]|(slTmp[2][i]<<8);
            if((vx&15)>8||(vx>>4)>11) return 0;                       // a colour slot and a shape that exist
            if(dc&&((dc&7)<1||(dc&7)>SLOT_NSPR||(dc>>11))) return 0;  // a face sprite that exists
        }
    }
    if(apply){
        for(int i=0;i<LK_N;i++) look[i]=lk[i];
        stage=(u8)stg; ageDays=(u8)agd; fixLook(); ageSave();
        buildLook();                                                   // also sets sty[] and custom=0
        if(cu){ u8*v=&vox[0][0][0]; u16*d=&dec[0][0][0]; for(int i=0;i<CNV;i++){ v[i]=slTmp[0][i]; d[i]=(u16)(slTmp[1][i]|(slTmp[2][i]<<8)); } custom=1; }
        setColors();
    }
    return 1;
}
// ---------- chunks: LIFE (24 bytes straight from the life block) ----------
static void slEncLife(SlW*w){ slwPut(w,1); for(int i=0;i<SIM_BLOCK;i++) slwPut(w,SIM_SRAM[i]); }
static int slDecLife(SlR*c,int apply){
    if(c->len!=1+SIM_BLOCK||c->p[0]!=1) return 0;
    volatile unsigned char*b=(volatile unsigned char*)(c->p+1);
    if(!simsCheck(b)) return 0;
    if(apply){ simsUnpack(b); simsSaveNow(); }
    return 1;
}
static void slChunk(SlW*w,int tag,void(*enc)(SlW*)){   // a chunk: its length first (measured by a dry run), then the data
    SlW t; slwInit(&t,0,1<<20); enc(&t);
    slwPut(w,tag); slwPut16(w,t.pos); enc(w);
}
static int slBuild(SlW*w,int mask){
    if(mask&SLH_ROOM) slChunk(w,SLC_ROOM,slEncRoom);
    if(mask&SLH_PERSON) slChunk(w,SLC_PERSON,slEncPerson);
    if(mask&SLH_LIFE) slChunk(w,SLC_LIFE,slEncLife);
    slwPut(w,0);   // end
    return w->pos;
}

// ---------- header, scan ----------
typedef struct { u8 ok,kind,span,has,mw,mh; u16 len,sum,seq; char name[SLOT_NAME+1]; } SlInfo;
static SlInfo slI[SLOT_N]; static s8 slOwner[SLOT_N]; static u8 slGood[SLOT_N];   // per slot: header, which head covers it (-1 none), payload checksum ok
#define SLB(s) (SRAM_BASE+SLOT_BASE+(s)*SLOT_SZ)
static int slInfo(int slot,SlInfo*I){
    volatile u8*m=SLB(slot); I->ok=0;
    if(m[0]!='S'||m[1]!='V'||m[2]!=1) return 0;
    u8 hs=0x5B; for(int i=0;i<31;i++) hs=(u8)(hs+m[i]); if(m[31]!=hs) return 0;
    I->kind=m[3]; I->span=m[4]; I->has=m[5]; I->len=(u16)(m[6]|(m[7]<<8)); I->sum=(u16)(m[8]|(m[9]<<8)); I->mw=m[10]; I->mh=m[11]; I->seq=(u16)(m[12]|(m[13]<<8));
    int e=SLOT_NAME; for(int i=0;i<SLOT_NAME;i++){ char c=(char)m[16+i]; I->name[i]=((c>='A'&&c<='Z')||(c>='0'&&c<='9'))?c:' '; }
    while(e>0&&I->name[e-1]==' ') e--; I->name[e]=0;
    if(I->kind>SLK_HOUSE||I->span<1||slot+I->span>SLOT_N||I->len>I->span*SLOT_SZ-SLOT_HDR) return 0;
    I->ok=1; return 1;
}
static void slScan(void){
    for(int i=0;i<SLOT_N;i++){ slOwner[i]=-1; slGood[i]=0; slI[i].ok=0; }
    for(int i=0;i<SLOT_N;){
        SlInfo*I=&slI[i];
        if(slInfo(i,I)){ slGood[i]=(u8)(slSumOf(SLB(i)+SLOT_HDR,I->len)==I->sum);
            for(int k=0;k<I->span;k++) slOwner[i+k]=(s8)i; i+=I->span; }
        else i++;
    }
}
static int slActive(void){   // the active slot, or -1
    volatile u8*m=SRAM_BASE+SLOT_DIR;
    if(m[0]!='S'||m[1]!='D'||m[2]>=SLOT_N||m[3]!=(u8)(0x5D+m[2])) return -1;
    SlInfo I; if(!slInfo(m[2],&I)) return -1;
    return m[2];
}
static void slSetActive(int s){ volatile u8*m=SRAM_BASE+SLOT_DIR; m[0]='S'; m[1]='D'; m[2]=(u8)s; m[3]=(u8)(0x5D+s); }
static void slHeader(int slot,int kind,int span,int has,int len,int sum,int seq,const char*name){
    volatile u8*m=SLB(slot);
    for(int i=2;i<SLOT_HDR;i++) m[i]=0;
    m[2]=1; m[3]=(u8)kind; m[4]=(u8)span; m[5]=(u8)has; m[6]=(u8)(len&255); m[7]=(u8)(len>>8); m[8]=(u8)(sum&255); m[9]=(u8)(sum>>8);
    m[10]=MW; m[11]=MH; m[12]=(u8)(seq&255); m[13]=(u8)(seq>>8);
    for(int i=0;i<SLOT_NAME;i++){ char c=name[i]; if(!c){ for(;i<SLOT_NAME;i++) m[16+i]=' '; break; } m[16+i]=(u8)c; }
    m[0]='S'; m[1]='V';                                   // the magic goes in last: until now the slot read as empty
    u8 hs=0x5B; for(int i=0;i<31;i++) hs=(u8)(hs+m[i]); m[31]=hs;
}
static void slDefaultName(int slot,char*d){ const char*r="ROOM "; int i=0; while(*r) d[i++]=*r++; d[i++]=(char)('1'+slot); d[i]=0; }

// ---------- save / load / delete / copy / rename ----------
static int slMaskOpt(void){ static const u8 t[3]={SLH_ROOM,SLH_ROOM|SLH_PERSON,SLH_ROOM|SLH_PERSON|SLH_LIFE}; return t[xo[XO_SLOTCONT]]; }
// name = 0: keep the name the slot has (or ROOM n for an empty slot). Returns SLE_OK or an error and leaves the slot as it was on SLE_BIG.
static int slSave(int slot,int mask,const char*name){
    SlInfo old; int had=slInfo(slot,&old); char nm[SLOT_NAME+1];
    if(name) { int i=0; for(;name[i]&&i<SLOT_NAME;i++) nm[i]=name[i]; nm[i]=0; }
    else if(had&&old.kind==SLK_ROOM){ for(int i=0;i<=SLOT_NAME;i++) nm[i]=old.name[i]; }
    else slDefaultName(slot,nm);
    if(had&&old.kind==SLK_HOUSE) return SLE_HOUSE;
    if((mask&SLH_LIFE)&&!simsCheck(SIM_SRAM)) mask&=~SLH_LIFE;       // no life has been played yet: nothing to store
    SlW d; slwInit(&d,0,1<<20); slBuild(&d,mask);
    if(d.pos>SLOT_SZ-SLOT_HDR) return SLE_BIG;                       // checked BEFORE touching the slot
    volatile u8*m=SLB(slot);
    m[0]=0; m[1]=0;                                                  // empty while we write
    SlW w; slwInit(&w,m+SLOT_HDR,SLOT_SZ-SLOT_HDR); slBuild(&w,mask);
    slHeader(slot,SLK_ROOM,1,mask,w.pos,slwSum(&w),had?old.seq+1:1,nm);
    SlInfo chk; if(!slInfo(slot,&chk)||slSumOf(m+SLOT_HDR,chk.len)!=chk.sum) return SLE_NOSRAM;   // read it back: no battery RAM here?
    return SLE_OK;
}
static int slLoadedMask;   // what the last slLoad really changed
static int slParse(volatile u8*body,int len,int mask,int apply){
    SlR r={body,0,len,0};
    while(r.pos<len){
        int tag=slrGet(&r); if(tag==0) break;
        int cl=slrGet16(&r); if(r.bad||r.pos+cl>len) return SLE_FMT;
        SlR c={body+r.pos,0,cl,0}; r.pos+=cl;
        if(tag==SLC_ROOM&&(mask&SLH_ROOM)){ if(!slDecRoom(&c,apply)) return SLE_FMT; if(apply) slLoadedMask|=SLH_ROOM; }
        else if(tag==SLC_PERSON&&(mask&SLH_PERSON)){ if(!slDecPerson(&c,apply)) return SLE_FMT; if(apply) slLoadedMask|=SLH_PERSON; }
        else if(tag==SLC_LIFE&&(mask&SLH_LIFE)){ if(!slDecLife(&c,apply)) return SLE_FMT; if(apply) slLoadedMask|=SLH_LIFE; }
        // SLC_HOUSE and any tag we do not know: skipped on purpose (see the HOUSES note at the top)
    }
    return SLE_OK;
}
static int houseLoad(int slot){ (void)slot; return SLE_HOUSE; }                     // TODO (see HOUSES at the top of this file)
static int houseSave(int slot,const char*name){ (void)slot; (void)name; return SLE_HOUSE; }   // TODO
static int slLoad(int slot,int mask){
    SlInfo I; slLoadedMask=0;
    if(!slInfo(slot,&I)) return SLE_EMPTY;
    if(I.kind==SLK_HOUSE) return SLOT_HOUSE_READY?houseLoad(slot):SLE_HOUSE;
    if(slSumOf(SLB(slot)+SLOT_HDR,I.len)!=I.sum) return SLE_BAD;
    if(I.mw!=MW||I.mh!=MH) return SLE_SIZE;
    mask&=I.has; if(!mask) return SLE_NOPART;
    int e=slParse(SLB(slot)+SLOT_HDR,I.len,mask,0); if(e) return e;      // pass 1: check everything, change nothing
    e=slParse(SLB(slot)+SLOT_HDR,I.len,mask,1); if(e) return e;          // pass 2: apply
    if(slLoadedMask&SLH_ROOM){ mapSave(); mapScan(); }                  // the loaded room becomes the current room
    slSetActive(slot);
    return SLE_OK;
}
static void slDelete(int slot){
    SLB(slot)[0]=0; SLB(slot)[1]=0;
    volatile u8*d=SRAM_BASE+SLOT_DIR; if(d[0]=='S'&&d[2]==(u8)slot) d[0]=0;   // it was the active slot: there is none now
}
static void slEraseAll(void){ volatile u8*m=SRAM_BASE+SLOT_BASE; for(int i=0;i<SLOT_N*SLOT_SZ;i++) m[i]=0; volatile u8*d=SRAM_BASE+SLOT_DIR; for(int i=0;i<4;i++) d[i]=0; }
static int slCopy(int src,int dst){   // span 1 only; byte for byte, magic last
    SlInfo I; if(!slInfo(src,&I)) return SLE_EMPTY; if(I.span!=1) return SLE_HOUSE;
    volatile u8*a=SLB(src),*b=SLB(dst); int n=SLOT_HDR+I.len;
    b[0]=0; b[1]=0; for(int i=2;i<n;i++) b[i]=a[i]; b[0]='S'; b[1]='V';
    return SLE_OK;
}
static void slRename(int slot,const char*name){
    SlInfo I; if(!slInfo(slot,&I)) return; volatile u8*m=SLB(slot);
    for(int i=0;i<SLOT_NAME;i++){ char c=name[i]; if(!c){ for(;i<SLOT_NAME;i++) m[16+i]=' '; break; } m[16+i]=(u8)c; }
    u8 hs=0x5B; for(int i=0;i<31;i++) hs=(u8)(hs+m[i]); m[31]=hs;
}
static int slotSyncActive(void){   // SAVE MAP TO SLOT: write the room into the active slot too
    int a=slActive(); if(a<0) return 0;
    return slSave(a,slMaskOpt(),0)==SLE_OK;
}
static void slotBoot(void){        // BOOT LOADS PERSON: the creature is not kept anywhere else, so bring it back from the active slot
    if(!xo[XO_SLOTBOOT]) return;
    int a=slActive(); if(a>=0) slLoad(a,SLH_PERSON);
}
static int slotUsedBytes(void){ int n=0; for(int i=0;i<SLOT_N;i++) if(slOwner[i]==i) n+=SLOT_HDR+slI[i].len; return n; }
static const char* slErrMsg(int e){
    switch(e){ case SLE_BIG: return "TOO BIG FOR A SLOT"; case SLE_BAD: return "SLOT IS DAMAGED"; case SLE_EMPTY: return "SLOT IS EMPTY";
        case SLE_HOUSE: return "HOUSES COME LATER"; case SLE_SIZE: return "MAP SIZE DOES NOT MATCH"; case SLE_NOPART: return "THAT PART IS NOT SAVED";
        case SLE_NOSRAM: return "SAVE NOT SUPPORTED HERE"; default: return "COULD NOT READ SLOT"; }
}

// ---------- the screen ----------
static char* slCat(char*d,const char*s){ while(*s) *d++=*s++; *d=0; return d; }
static char* slNum(char*d,int n){ char t[8]; int k=0; if(n<=0) t[k++]='0'; while(n>0&&k<7){ t[k++]=(char)('0'+n%10); n/=10; } while(k>0) *d++=t[--k]; *d=0; return d; }
static char* slSize(char*d,int n){   // 812B  or  1.4K
    if(n<1000) { d=slNum(d,n); return slCat(d,"B"); }
    d=slNum(d,n/1024); *d++='.'; d=slNum(d,(n%1024)*10/1024); return slCat(d,"K");
}
static void slThumb(int slot,int x0,int y0){   // the room of a slot at 1 pixel per tile, decoded straight from SRAM
    rect(x0-1,y0-1,MW+2,MH+2,RGB(3,4,7)); rect(x0,y0,MW,MH,RGB(6,7,10));
    SlInfo*I=&slI[slot]; if(!I->ok||!slGood[slot]||I->kind!=SLK_ROOM||!(I->has&SLH_ROOM)) return;
    SlR r={SLB(slot)+SLOT_HDR,0,I->len,0};
    while(r.pos<r.len){
        int tag=slrGet(&r); if(!tag) break; int cl=slrGet16(&r); if(r.bad) return;
        if(tag!=SLC_ROOM){ r.pos+=cl; continue; }
        SlR c={r.p+r.pos,0,cl,0}; int idx=slrGet(&c), fmt=slrGet(&c); if(idx!=0||fmt!=1) return;
        int i=0;
        while(c.pos<c.len&&i<MSZ){
            int n=slrGet(&c), t=slrGet(&c), f=slrGet(&c); if(c.bad) return;
            u16 col; if(t=='.') col=shade(flFlat[(f>>4)%NFL][0],10); else { int pi=palIdx((char)t); col=pi>=0?palCol[pi]:0; }
            for(int k=0;k<n&&i<MSZ;k++,i++) px(x0+i%MW,y0+i/MW,col);
        }
        return;
    }
}
static void slDraw(int sel){
    fillCols(0,ROW_W,RGB(3,4,8));
    box(3,1,234,157); text(12,6,"ROOM SLOTS",GOLD,1);
    int act=slActive();
    text(100,6,act>=0?"ACTIVE SLOT":"NO ACTIVE SLOT",DIMC,1); if(act>=0){ char nb[2]={(char)('1'+act),0}; text(160,6,nb,GOLD,1); }
    for(int i=0;i<SLOT_N;i++){
        int y=18+i*15; SlInfo*I=&slI[i];
        if(i==sel){ rect(8,y-2,142,14,RGB(6,16,8)); rect(8,y-2,2,14,GOLD); }
        char nb[2]={(char)('1'+i),0}; text(13,y+2,nb,i==sel?GOLD:DIMC,1);
        u16 nc=i==sel?WHITE:DIMC;
        if(slOwner[i]<0) text(24,y,"EMPTY",i==sel?RGB(20,23,26):RGB(9,11,15),1);
        else if(slOwner[i]!=i) text(24,y,"PART OF A HOUSE",RGB(14,16,22),1);
        else {
            text(24,y,I->name[0]?I->name:"NO NAME",slGood[i]?nc:RGB(30,10,8),1);
            char b[40]; char*e=b; *e=0;
            if(!slGood[i]) e=slCat(e,"DAMAGED");
            else if(I->kind==SLK_HOUSE) e=slCat(e,"HOUSE");
            else { if(I->has&SLH_ROOM) e=slCat(e,"ROOM "); if(I->has&SLH_PERSON) e=slCat(e,"PERSON "); if(I->has&SLH_LIFE) e=slCat(e,"LIFE "); }
            e=slCat(e," "); slSize(e,SLOT_HDR+I->len);
            text(24,y+7,b,i==sel?RGB(22,25,28):RGB(11,13,18),1);
            if(act==i) text(120,y,"ACTIVE",GOLD,1);
        }
    }
    // right: the room of the slot under the cursor
    slThumb(sel,166,20);
    if(slOwner[sel]==sel&&slGood[sel]){
        SlInfo*I=&slI[sel]; char b[24]; char*e=slCat(b,"SAVED "); e=slNum(e,I->seq); slCat(e,I->seq==1?" TIME":" TIMES");
        text(158,64,b,DIMC,1);
        text(158,72,I->kind==SLK_HOUSE?"A HOUSE":"ONE ROOM",DIMC,1);
    } else if(slOwner[sel]<0) text(158,64,"A FREE SLOT",DIMC,1);
    // bottom: how full the slots are
    int used=slotUsedBytes(), tot=SLOT_N*SLOT_SZ, cnt=0; for(int i=0;i<SLOT_N;i++) if(slOwner[i]>=0) cnt++;
    char b[40]; char*e=slNum(b,cnt); e=slCat(e," OF "); e=slNum(e,SLOT_N); e=slCat(e," SLOTS USED   "); e=slSize(e,used); e=slCat(e," OF "); slSize(e,tot);
    text(12,112,b,DIMC,1); rect(12,121,150,4,RGB(8,10,14)); rect(12,121,used*150/tot,4,GOLD);
    text(12,132,"UP DOWN PICK A SLOT   A OPTIONS",WHITE,1);
    text(12,141,"B BACK   SELECT HELP",DIMC,1);
    text(12,150,"SAVES ARE ONLY WRITTEN BY YOU HERE",RGB(12,14,16),1);
}

static const char slChars[]=" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static int slEditName(char*nm){   // nm holds SLOT_NAME letters/digits/spaces and a 0. Returns 1 if the player accepted it.
    char b[SLOT_NAME+1]; int cur=0; u16 prev=keyNow();
    { int i=0; for(;i<SLOT_NAME&&nm[i];i++) b[i]=nm[i]; for(;i<SLOT_NAME;i++) b[i]=' '; b[SLOT_NAME]=0; }
    const int NC=(int)sizeof(slChars)-1;
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k;
        int ci=0; for(int i=0;i<NC;i++) if(slChars[i]==b[cur]) ci=i;
        if(pr&K_UP) b[cur]=slChars[(ci+1)%NC];
        if(pr&K_DOWN) b[cur]=slChars[(ci+NC-1)%NC];
        if(pr&K_LEFT) cur=(cur+SLOT_NAME-1)%SLOT_NAME;
        if(pr&K_RIGHT) cur=(cur+1)%SLOT_NAME;
        if(pr&K_SEL) for(int i=0;i<SLOT_NAME;i++) b[i]=' ';
        if(pr&K_B) return 0;
        if(pr&(K_A|K_START)){ int e=SLOT_NAME; while(e>0&&b[e-1]==' ') e--; if(e==0) continue; for(int i=0;i<SLOT_NAME;i++) nm[i]=b[i]; nm[SLOT_NAME]=0; return 1; }
        box(20,44,200,66); text(28,50,"NAME THIS ROOM",GOLD,1);
        for(int i=0;i<SLOT_NAME;i++){ char s[2]={b[i],0}; int x=28+i*17; rect(x-1,66,15,18,i==cur?RGB(6,16,8):RGB(5,6,10)); if(b[i]!=' ') text(x+2,68,s,WHITE,2); if(i==cur) rect(x,83,13,2,GOLD); }
        text(28,92,"UP DOWN LETTER  LEFT RIGHT MOVE",DIMC,1);
        text(28,100,"A OK  B CANCEL  SELECT CLEARS",DIMC,1);
        present();
    }
}
static const char* const slYesNo[2]={"NO","YES"};
static char slLn[9][30]; static const char* slLp[9];
static void slInfoScreen(int s){
    SlInfo*I=&slI[s]; int n=0; char*e;
    slLn[n][0]='>'; slCat(slLn[n]+1,I->name[0]?I->name:"NO NAME"); n++;
    e=slCat(slLn[n],"KIND   "); slCat(e,I->kind==SLK_HOUSE?"HOUSE":"ROOM"); n++;
    e=slCat(slLn[n],"SIZE   "); e=slSize(e,SLOT_HDR+I->len); e=slCat(e," OF "); slSize(e,SLOT_SZ*I->span); n++;
    e=slCat(slLn[n],"SAVED  "); e=slNum(e,I->seq); slCat(e,I->seq==1?" TIME":" TIMES"); n++;
    e=slCat(slLn[n],"MAP    "); e=slNum(e,I->mw); e=slCat(e," X "); slNum(e,I->mh); n++;
    e=slCat(slLn[n],"HOLDS  "); if(I->has&SLH_ROOM) e=slCat(e,"ROOM "); if(I->has&SLH_PERSON) e=slCat(e,"PERSON "); if(I->has&SLH_LIFE) e=slCat(e,"LIFE "); n++;
    slCat(slLn[n],slGood[s]?"CHECK  OK":"CHECK  DAMAGED"); n++;
    for(int i=0;i<n;i++) slLp[i]=slLn[i];
    helpScreen("SLOT INFO",slLp,n);
}
enum { SA_SAVE, SA_LOADALL, SA_LOADROOM, SA_LOADPERSON, SA_RENAME, SA_COPY, SA_INFO, SA_DELETE };
static char slCopyNm[SLOT_N][20]; static const char* slCopyIt[SLOT_N];
// A on a slot: the list of things you can do with it. Returns 1 if a slot was loaded (the caller must restart play).
static int slActions(int s){
    int own=slOwner[s]; SlInfo*I=&slI[s];
    if(own>=0&&own!=s){ toast("PART OF A HOUSE"); return 0; }
    int occ=(own==s), good=occ&&slGood[s], house=occ&&I->kind==SLK_HOUSE, conf=xo[XO_SLOTCONF], changed=0;
    const char*it[9]; int id[9], n=0;
    if(!occ){ it[n]="SAVE HERE"; id[n++]=SA_SAVE; }
    else {
        if(good){ it[n]="LOAD ALL"; id[n++]=SA_LOADALL;
            if(!house&&(I->has&SLH_ROOM)&&(I->has&~SLH_ROOM)){ it[n]="LOAD ROOM ONLY"; id[n++]=SA_LOADROOM; }
            if(!house&&(I->has&SLH_PERSON)&&(I->has&~SLH_PERSON)){ it[n]="LOAD PERSON ONLY"; id[n++]=SA_LOADPERSON; } }
        if(!house){ it[n]="SAVE OVER IT"; id[n++]=SA_SAVE; }
        if(good&&!house){ it[n]="RENAME"; id[n++]=SA_RENAME; it[n]="COPY TO"; id[n++]=SA_COPY; }
        it[n]="INFO"; id[n++]=SA_INFO; it[n]="DELETE"; id[n++]=SA_DELETE;
    }
    int c=menu(occ?(I->name[0]?I->name:"NO NAME"):"EMPTY SLOT",it,n); if(c<0) return 0;
    switch(id[c]){
        case SA_SAVE:{
            if(occ&&conf&&menu("OVERWRITE THIS SLOT",slYesNo,2)!=1) break;
            int e=slSave(s,slMaskOpt(),0);
            if(e) toast(slErrMsg(e)); else { slSetActive(s); toast("SLOT SAVED"); }
        } break;
        case SA_LOADALL: case SA_LOADROOM: case SA_LOADPERSON:{
            if(conf&&menu("LOAD OVER WHAT YOU HAVE",slYesNo,2)!=1) break;
            int m=id[c]==SA_LOADALL?(SLH_ROOM|SLH_PERSON|SLH_LIFE):id[c]==SA_LOADROOM?SLH_ROOM:SLH_PERSON;
            int e=slLoad(s,m);
            if(e) toast(slErrMsg(e)); else { toast("LOADED"); changed=1; }
        } break;
        case SA_RENAME:{ char nm[SLOT_NAME+1]; for(int i=0;i<=SLOT_NAME;i++) nm[i]=I->name[i]; if(slEditName(nm)) slRename(s,nm); } break;
        case SA_COPY:{
            for(int i=0;i<SLOT_N;i++){ char*e=slNum(slCopyNm[i],i+1); e=slCat(e," "); slCat(e,slOwner[i]==i?(slI[i].name[0]?slI[i].name:"NO NAME"):slOwner[i]<0?"EMPTY":"HOUSE"); slCopyIt[i]=slCopyNm[i]; }
            int d=menu("COPY TO WHICH SLOT",slCopyIt,SLOT_N); if(d<0) break;
            if(d==s){ toast("THAT IS THE SAME SLOT"); break; }
            if(slOwner[d]>=0&&slI[slOwner[d]].span>1){ toast("A HOUSE IS IN THE WAY"); break; }
            if(slOwner[d]>=0&&conf&&menu("OVERWRITE THAT SLOT",slYesNo,2)!=1) break;
            int e=slCopy(s,d); toast(e?slErrMsg(e):"COPIED");
        } break;
        case SA_INFO: slInfoScreen(s); break;
        case SA_DELETE: if(!conf||menu("DELETE THIS SLOT",slYesNo,2)==1){ slDelete(s); toast("SLOT DELETED"); } break;
    }
    return changed;
}
static const char* const slotHelp[12]={">ROOM SLOTS","SIX NAMED SAVES OF A ROOM","A ON A SLOT OPENS ITS LIST",">WHAT A SLOT HOLDS","ROOM  WALLS FLOORS ITEMS","PERSON  YOUR CREATURE  LIFE  CASH AND CLOCK","OPTIONS PICK WHAT SAVING STORES",">GOOD TO KNOW","LOADING MAKES THAT ROOM THE CURRENT ONE","THE ACTIVE SLOT IS THE LAST YOU USED","HOUSES WILL SHARE THIS SCREEN LATER"};
static int slotScreen(void){   // returns 1 if something was loaded
    int sel=0, dirty=1, changed=0; u16 prev=keyNow();
    slScan();
    for(;;){
        u16 k=keyNow(), pr=k&~prev; prev=k;
        if(pr&K_DOWN){ sel=(sel+1)%SLOT_N; dirty=1; }
        if(pr&K_UP){ sel=(sel+SLOT_N-1)%SLOT_N; dirty=1; }
        if(pr&(K_B|K_START)) break;
        if(pr&K_SEL){ helpScreen("ROOM SLOTS",slotHelp,11); prev=keyNow(); dirty=1; }
        if(pr&K_A){ changed|=slActions(sel); slScan(); prev=keyNow(); dirty=1; }
        if(dirty){ slDraw(sel); present(); dirty=0; } else vsync();
    }
    while((~REG_KEYINPUT)&0x3FF) vsync();
    return changed;
}
