#!/usr/bin/env python3
"""Exercise the real shared approach, first-impact target and floor timing logic."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
sp=(root/"space.inc").read_text();du=(root/"dungeon.inc").read_text();ma=(root/"main.c").read_text()
def func(s,m):
    a=s.index(m);i=s.index("{",a)+1;d=1
    while d:
        d+=(s[i]=="{")-(s[i]=="}");i+=1
    return s[a:i]
assert "int best=sp_laser_pick();laser_contact=best>=0;" in sp
assert "approach==SP_APPROACH_NONE||fire_override" in sp
assert "held>=cargo_cap+ship_hold_bonus()" in sp
assert "fuel=fuel>cost?fuel-cost:0" in sp
assert "m->speed_left=(u16)fl_ticks_left(fl_end,vbl_count)" in du
assert "fl_end=(u32)vbl_count+(m->speed_left>3600?3600:m->speed_left)" in du
assert "near_loot_full&&bag_n>=BAGMAX" in ma
en=re.search(r"enum \{ SP_APPROACH_NONE,.*?\};",sp,re.S).group()
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef int32_t s32;typedef uint32_t u32;
#define NPIR 64
#define VCY 64
#define FOCS 160
typedef struct {s32 x,y,z;int alive,team;} Pirate;
typedef struct {s32 x,y,z;int alive,r;} Ast;
static Pirate pir[NPIR];static Ast ast[30];static int nast=30,in_lair;
static int sproj(s32 x,s32 y,s32 z,int *sx,int *sy,s32 *zz){
    *sx=x;*sy=y;*zz=z;return z>20;
}
"""
cases=r"""
static void clear(void){memset(pir,0,sizeof pir);memset(ast,0,sizeof ast);in_lair=0;}
int main(void){
    unsigned approaches=0;
    for(int land=0;land<2;land++)for(int dock=0;dock<2;dock++)
    for(int rock=-1;rock<2;rock++)for(int speed=0;speed<=110;speed++)
    for(int spin=0;spin<2;spin++)for(int rel=0;rel<20;rel++){
        int want=dock?(speed<=14?SP_APPROACH_DOCK:SP_APPROACH_BRAKE):
                     land?(speed<=12?SP_APPROACH_LAND:SP_APPROACH_BRAKE):
                     rock>=0?(!spin?SP_APPROACH_MATCH:rel<=12?SP_APPROACH_ROCK:SP_APPROACH_BRAKE):SP_APPROACH_NONE;
        assert(sp_approach(land,dock,rock,speed,spin,rel)==want);approaches++;
    }
    clear();pir[0]=(Pirate){120,64,4000,1,1};ast[0]=(Ast){120,64,6000,1,200};ast[29]=(Ast){120,64,2000,1,200};
    assert(sp_laser_pick()==NPIR+29);
    ast[29].alive=0;assert(sp_laser_pick()==0);
    pir[0].team=0;assert(sp_laser_pick()==NPIR);
    ast[0].x=150;assert(sp_laser_pick()==-1);
    pir[63]=(Pirate){120,64,9000,1,1};assert(sp_laser_pick()==63);
    in_lair=1;assert(sp_laser_pick()==63);
    clear();ast[0]=(Ast){120,64,1000,1,200};in_lair=1;assert(sp_laser_pick()==-1);
    assert(sp_laser_depth(120,64,20,200,1)==0);
    assert(sp_laser_depth(120,64,9001,200,1)==0);
    assert(!sp_laser_depth(145,89,2000,200,1));
    for(int left=0;left<=3600;left++){
        u32 now=0xfffffff0u;
        assert(fl_ticks_left(now+(u32)left,now)==left);
        assert(fl_seconds_left(now+(u32)left,now)==(left+59)/60);
    }
    assert(fl_ticks_left(1000,1001)==0);assert(fl_seconds_left(1000,999)==1);
    assert(fl_ticks_left(9000,0)==3600);
    printf("Flight/dungeon: %u approach combinations, single closest impact, and 3601 timer cases PASS\n",approaches);
}
"""
with tempfile.TemporaryDirectory() as td:
    p=Path(td);c=p/"flight.c";exe=p/"flight"
    c.write_text(prefix+en+"\n"+"\n".join(func(sp,k) for k in ("static int sp_approach(","static int sp_laser_depth(","static int sp_laser_pick("))+"\n"+func(du,"static int fl_ticks_left(")+"\n"+func(du,"static int fl_seconds_left(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
