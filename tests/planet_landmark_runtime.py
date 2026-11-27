#!/usr/bin/env python3
"""Execute the actual shared surface landmark and street shading helpers."""
from pathlib import Path
import subprocess, tempfile
src=(Path(__file__).resolve().parents[1]/"planet.inc").read_text()
def extract(mark):
    a=src.index(mark); b=src.index("{",a)+1; depth=1
    while depth:
        depth+=(src[b]=="{")-(src[b]=="}"); b+=1
    return src[a:b]
prefix=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int32_t s32;
typedef uint8_t u8;
#define HW 256
#define HMASK 255
#define TW 128
#define TMASK 127
#define BLD_B 64
#define NPO 160
enum {O_DOOR,O_SHIP,O_CAGE,O_SLUG,O_PLANT,O_TREE,O_ROCK,O_WEB,O_FLOWER,O_BASE,O_MARKET,O_CANTINA,O_OFFICE,O_RACE,O_HOME,O_LOCAL,O_FOSSIL,O_RUIN,O_BEETLE,O_CRIT,O_SHRINE};
typedef struct {int kind,alive; s32 x,y;} PObj;
static PObj po[NPO];
static u8 cmap[HW*HW],hfine[HW*HW],hmap[TW*TW];
static int water_lv=70, tech=0;
static int city_tech(void){return tech;}
static int hm_at(s32 x,s32 y){return hmap[((y>>16)&TMASK)*TW+((x>>16)&TMASK)];}
"""
checks=r"""
/* Independent copy of the pre-optimization shading rule. */
static int original(int ix,s32 mx,s32 my){
    int tx=(mx>>15)&HMASK,ty=(my>>15)&HMASK;
    int cr=cmap[ty*HW+((tx+1)&HMASK)],cl=cmap[ty*HW+((tx-1)&HMASK)];
    int cd=cmap[((ty+1)&HMASK)*HW+tx],cu=cmap[((ty-1)&HMASK)*HW+tx];
    int fx=(mx>>11)&15,fy=(my>>11)&15;
    #define ISB(q) ((q)>=BLD_B&&(q)<BLD_B+8)
    if(ix==74){
        int ke=(ISB(cr)&&fx>=12)||(ISB(cl)&&fx<4)||(ISB(cd)&&fy>=12)||(ISB(cu)&&fy<4);
        int yk=((cr==80||cr==81)&&fx>=14)||((cl==80||cl==81)&&fx<2)||((cd==80||cd==81)&&fy>=14)||((cu==80||cu==81)&&fy<2);
        return ke?112:yk?113:ix;
    }
    if(ix==80||ix==81){
        int ke=(ISB(cr)&&fx>=11)||(ISB(cl)&&fx<5)||(ISB(cd)&&fy>=11)||(ISB(cu)&&fy<5);
        int kb=tech>=1&&((cr==74&&fx>=13)||(cl==74&&fx<3)||(cd==74&&fy>=13)||(cu==74&&fy<3));
        return ke?(ix==80?114:115):kb?116:ix;
    }
    #undef ISB
    return ix;
}
static void make_land(int x,int y){
    hmap[y*TW+x]=100;
    for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)hfine[(2*y+dy)*HW+2*x+dx]=100;
}
int main(void){
    const int cols[]={24,64,71,74,75,80,81};
    unsigned n=0;
    for(tech=0;tech<2;tech++)for(int a=0;a<7;a++)for(int b=0;b<7;b++)
    for(int c=0;c<7;c++)for(int d=0;d<7;d++){
        cmap[1]=cols[a];cmap[255]=cols[b];cmap[HW]=cols[c];cmap[255*HW]=cols[d];
        for(int ix=74;ix<=81;ix++)for(int fx=0;fx<16;fx++)for(int fy=0;fy<16;fy++){
            assert(street_ix(ix,fx<<11,fy<<11)==original(ix,fx<<11,fy<<11));n++;
        }
    }
    printf("City shade parity: %u exact palette checks\n",n);
    memset(cmap,0,sizeof cmap);
    for(int y=60;y<=68;y++)for(int x=60;x<=68;x++)make_land(x,y);
    po[0]=(PObj){O_SHIP,1,61<<16,65<<16};
    po[1]=(PObj){O_DOOR,1,12<<16,12<<16}; /* surrounded by water */
    po[2]=(PObj){O_BASE,1,63<<16,63<<16}; /* inside a building */
    cmap[(63*2)*HW+63*2]=64;
    po[3]=(PObj){O_CAGE,1,65<<16,65<<16};
    po[4]=(PObj){O_SHRINE,1,65<<16,65<<16}; /* duplicate */
    for(int i=1;i<=4;i++)assert(surface_goal_repair(i));
    for(int i=1;i<=4;i++){
        assert(po[i].alive&&surface_goal_safe(i,po[i].x,po[i].y));
        s32 x=po[i].x,y=po[i].y;
        assert(surface_goal_repair(i)&&po[i].x==x&&po[i].y==y);
    }
    /* Wraparound destinations can be repaired locally. */
    po[5]=(PObj){O_CAGE,1,0,0};make_land(127,127);
    assert(surface_goal_repair(5));
    assert((po[5].x>>16)==127&&(po[5].y>>16)==127);
    memset(hfine,0,sizeof hfine);memset(hmap,0,sizeof hmap);
    assert(!surface_goal_repair(1)&&po[1].alive);
    puts("Surface goals: water, building, crowding, idempotence, wrap and impossible-seed fallback PASS");
}
"""
assert "if(po[i].alive && surface_goal_kind(po[i].kind)) surface_goal_repair(i)" in src
assert "!surface_goal_kind(po[i].kind)" in src
with tempfile.TemporaryDirectory() as td:
    p=Path(td); c=p/"surface.c"; exe=p/"surface"
    c.write_text(prefix+"\n".join(extract(m) for m in ("static s32 wrapd(","static int surface_goal_kind(","static int surface_goal_safe(","static int surface_goal_repair(","static int street_ix("))+checks)
    subprocess.run(["cc","-std=c11","-O2","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
