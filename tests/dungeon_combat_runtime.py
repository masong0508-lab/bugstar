#!/usr/bin/env python3
"""Compile production combat functions; compare collision order and behavior."""
from pathlib import Path
import re
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
src=(ROOT/"main.c").read_text()
dun=(ROOT/"dungeon.inc").read_text()
def function(mark):
    m=re.search(re.escape(mark)+r"[^;{}]*\{",src)
    assert m,mark
    b=src.index("{",m.start());e=b+1;depth=1
    while depth:
        depth+=(src[e]=="{")-(src[e]=="}");e+=1
    return src[m.start():e]
assert "#define NDO 160" in dun
assert "u16 hw, stp, rad, pspd;" in dun
assert src.count("u8 used[SP_USED_BYTES]={0};")==2
assert src.count("sp_mark(used,i);")==3
assert "i<32 && ((used>>i)&1)" not in src
assert "if(wtick){ mine_hazard(); mon_bodies(); }" in src
assert src.index("if(!freeze){ player_move(tvx,tvy);") < src.index("if(wtick){ mine_hazard(); mon_bodies(); }")
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t s32;
typedef uint32_t u32;
typedef uint8_t u8;
#define NDO 160
#define BODY_R_LIMIT 0x14000
#define SP_USED_BYTES ((NDO+7)/8)
enum {K_MON=1,K_CAGE,K_ITEM,K_BARREL};
typedef struct {s32 x,y,rad;int alive,kind,mass,champ;} DObj;
static DObj dob[NDO];
static int ndo,pet_away_t,pet_down_t;
static s32 px_,py_,pet_x,pet_y;
static unsigned radii,masses,solves;
static s32 body_r(DObj *o){radii++;return o->champ?o->rad*5/4:o->rad;}
static int body_m(DObj *o){masses++;return o->champ?2*o->mass:o->mass;}
static int is_mon(int k){return k==K_MON;}
/* A deterministic, position-changing solver fixture. Its broad phase
   exactly matches the production solver. Compare the real old/new
   loop, including chained movement and the pet, with the same fixture. */
static int separate(s32 *ax,s32 *ay,s32 ar,int am,s32 awr,
                    s32 *bx,s32 *by,s32 br,int bm,s32 bwr,s32 *nx,s32 *ny){
    solves++;s32 dx=*ax-*bx,dy=*ay-*by,rr=ar+br;
    (void)awr;(void)bwr;
    if(dx>=rr||dx<=-rr||dy>=rr||dy<=-rr)return 0;
    if((int64_t)dx*dx+(int64_t)dy*dy >= (int64_t)rr*rr)return 0;
    s32 v=rr/8+1;*ax+=(dx>=0?v:-v)*bm/(am+bm);
    *bx-=(dx>=0?v:-v)*am/(am+bm);
    *ay+=(dy>=0?v:-v)*bm/(am+bm);
    *by-=(dy>=0?v:-v)*am/(am+bm);
    *nx=dx;*ny=dy;return 1;
}
enum {FE_NONE,FE_GAS,FE_CAVE};
#define RP_QUAKE 0
static int fl_ev,mh_t,moved_real,depth_no,shield_t,opt_shake,shake_t,damage,sounds,warnings;
static const char *said;
static int dlv(void){return 12;}
static void say(const char *s){said=s;if(strstr(s,"RUMBLES"))warnings++;}
static void sfx(int i){(void)i;sounds++;}
static void rumble(int i){(void)i;}
static void hurt(int i){damage+=i;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"combat regression line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
baseline=r"""
static void old_bodies(void){
    int pet_here = !pet_away_t && !pet_down_t;
    for(int i=0;i<ndo;i++){ DObj *a=&dob[i]; if(!a->alive) continue; s32 ra=body_r(a); if(!ra) continue;
        if(iabs(a->x-px_)>0x70000 || iabs(a->y-py_)>0x70000) continue; s32 nx,ny;
        if(pet_here && a->kind!=K_CAGE) separate(&pet_x,&pet_y,0x2800,1,0x1400,&a->x,&a->y,ra,body_m(a),ra>>1,&nx,&ny);
        for(int j=i+1;j<ndo;j++){ DObj *b=&dob[j]; if(!b->alive) continue; s32 rb=body_r(b); if(!rb) continue;
            separate(&a->x,&a->y,ra,body_m(a),ra>>1,&b->x,&b->y,rb,body_m(b),rb>>1,&nx,&ny); } }
}
static u32 random_state=0x12345678;
static u32 rnd(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
int main(void){
    unsigned long long old_solves=0,new_solves=0,old_radii=0,new_radii=0,old_masses=0,new_masses=0;
    DObj initial[NDO],reference[NDO];
    for(int test=0;test<400;test++){
        ndo=1+rnd()%NDO;px_=py_=16*65536;pet_x=px_+12000;pet_y=py_-8000;
        s32 pet0x=pet_x,pet0y=pet_y;
        pet_away_t=test%5==0;pet_down_t=test%7==0;
        for(int i=0;i<ndo;i++){
            DObj *o=&dob[i];
            int extent=test%3==0?262144:1048576;
            *o=(DObj){px_+(s32)(rnd()%(2*extent))-extent,
                      py_+(s32)(rnd()%(2*extent))-extent,
                      (s32)(rnd()%65536),rnd()%5!=0,1+(int)(rnd()%4),1+(int)(rnd()%40),rnd()%4==0};
            if(o->kind==K_ITEM)o->rad=0;
        }
        memcpy(initial,dob,sizeof dob);solves=radii=masses=0;
        old_bodies();
        old_solves+=solves;old_radii+=radii;old_masses+=masses;
        memcpy(reference,dob,sizeof dob);s32 rx=pet_x,ry=pet_y;
        memcpy(dob,initial,sizeof dob);pet_x=pet0x;pet_y=pet0y;solves=radii=masses=0;
        mon_bodies();
        new_solves+=solves;new_radii+=radii;new_masses+=masses;
        CHECK(!memcmp(reference,dob,sizeof dob)&&pet_x==rx&&pet_y==ry);
    }
    CHECK(new_solves<old_solves && new_radii<old_radii && new_masses<old_masses);
    printf("400 crowd scenes identical: pair solver calls %llu -> %llu, radius calls %llu -> %llu, mass calls %llu -> %llu\n",
           old_solves,new_solves,old_radii,new_radii,old_masses,new_masses);
    // Every actor in the dense object array is individually targetable.
    ndo=NDO;px_=py_=0;
    for(int i=0;i<ndo;i++)dob[i]=(DObj){(NDO-i)*100,0,1000,1,K_MON,1,0};
    u8 used[SP_USED_BYTES]={0};
    CHECK(sp_near(0x60000,NULL)==159);
    for(int i=NDO-1;i>=0;i--){int got=sp_near(0x60000,used);CHECK(got==i);sp_mark(used,got);}
    CHECK(sp_near(0x60000,used)==-1);
    memset(used,0,sizeof used);dob[159].alive=0;dob[158].kind=K_ITEM;
    CHECK(sp_near(0x60000,used)==157);
    // Warning and damage cadence, stationary vs moving and protected gas.
    fl_ev=FE_CAVE;depth_no=8;mh_t=329;opt_shake=1;damage=0;warnings=0;
    mine_hazard();CHECK(mh_t==330&&warnings==1&&shake_t==20);
    moved_real=1;mh_t=419;mine_hazard();CHECK(!damage&&mh_t==0&&strstr(said,"BEHIND"));
    moved_real=0;mh_t=419;mine_hazard();CHECK(damage==10&&mh_t==0&&strstr(said,"CAVE-IN"));
    fl_ev=FE_GAS;mh_t=239;shield_t=10;mine_hazard();CHECK(damage==10&&mh_t==0);
    mh_t=239;shield_t=0;mine_hazard();CHECK(damage==15);
    fl_ev=FE_NONE;mh_t=100;mine_hazard();CHECK(mh_t==0);
    puts("160-slot spells and movement-aware mine hazards PASS");return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-dungeon-combat-") as td:
    c=Path(td)/"combat.c"; exe=Path(td)/"combat"
    code="\n".join(function(x) for x in ["static s32 iabs(","static s32 dist8(","static void mon_bodies(","static int sp_near(","static void sp_mark(","static void mine_hazard("])
    c.write_text(prefix+code+baseline)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-fsanitize=address,undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
