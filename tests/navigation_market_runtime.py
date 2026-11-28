#!/usr/bin/env python3
"""Extract production navigation, radar, market, hazard and line helpers and run under UBSan."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
space=(root/"space.inc").read_text()
game=(root/"game.inc").read_text()
def fn(src, mark):
    a=src.index(mark);i=src.index("{",a)+1;d=1
    while d:
        d+=(src[i]=="{")-(src[i]=="}");i+=1
    return src[a:i]
enum=re.search(r"enum \{ FE_NONE[^}]+\};",game).group()
table=game[game.index("static const char *const fe_name["):game.index("static const char *floor_event_name(")]
prices=re.search(r"static const u8 good_base\[8\]=\{[^}]+\};",space).group()
mul=re.search(r"static const u8 eco_mul\[4\]\[8\]=\{.*?\};",space,re.S)
if not mul:
    a=space.index("static const u8 eco_mul");mul=space[a:space.index(";",a)+1]
else: mul=mul.group()
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef int32_t s32;typedef uint32_t u32;typedef uint8_t u8;typedef uint16_t u16;
#define NSYS 32
static struct {int eco;} sys[NSYS];
static int cur_sys,visits,fuel,unlock[NSYS],distance_[NSYS],fuel_multiplier=100;
static u32 sys_id(int i){return (u32)i+1;}
static int can_jump(int i){return i!=cur_sys&&unlock[i];}
static int sys_dist(int i,int j){(void)j;return distance_[i];}
static int xo_fuel(int n){return (n*fuel_multiplier+99)/100;}
static int pixels;
static void pix(volatile u16 *pg,int x,int y,int c){assert(x>=0&&x<240&&y>=0&&y<128);pg[y*240+x]=(u16)c;pixels++;}
"""
cases=r"""
int main(void){
    assert(!strcmp(floor_event_name(FE_GAS),"TOXIC GAS"));
    assert(!strcmp(floor_event_name(FE_CAVE),"CAVE-IN RISK"));
    assert(!*floor_event_name(NFE)&&!*floor_event_name(-1)&&!*floor_event_name(10000));
    for(int i=1;i<NFE;i++)assert(*floor_event_name(i));
    assert(!strcmp(sp_nav_direction(0,0,1000),"AHEAD"));
    assert(!strcmp(sp_nav_direction(-300,0,1000),"LEFT"));
    assert(!strcmp(sp_nav_direction(300,0,1000),"RIGHT"));
    assert(!strcmp(sp_nav_direction(0,300,1000),"CLIMB"));
    assert(!strcmp(sp_nav_direction(0,-300,1000),"DIVE"));
    assert(!strcmp(sp_nav_direction(0,0,-1000),"TURN"));
    unsigned radar=0;
    for(int x=-90000;x<=90000;x+=1777)for(int y=-90000;y<=90000;y+=1931)
        for(int a=0;a<4;a++){
            s32 c[]={16384,11585,0,-11585},sn[]={0,11585,16384,11585},px=x,py=y;
            sp_scanner_unroll(&px,&py,c[a],sn[a]);
            assert(px==(((long long)x*c[a]+(long long)y*sn[a])>>14));
            assert(py==(((long long)y*c[a]-(long long)x*sn[a])>>14));radar++;
        }
    static u16 screen[240*128];unsigned lines=0;
    for(int x=-300;x<500;x+=37)for(int y=-200;y<400;y+=29){
        line2(screen,x,y,239-x,127-y,1);lines++;
    }
    line2(screen,239,127,0,0,1);assert(pixels>0);
    unsigned markets=0;
    for(int i=0;i<NSYS;i++){sys[i].eco=i%4;distance_[i]=i*3;unlock[i]=i%3!=0;}
    for(int v=0;v<23;v++){visits=v;
        for(int g=0;g<8;g++)for(fuel=0;fuel<100;fuel+=7)for(fuel_multiplier=75;fuel_multiplier<=150;fuel_multiplier+=25){
            int gain,best=market_best_sale(g,&gain),max=0,bd=99999,want=-1;
            for(int i=0;i<NSYS;i++)if(can_jump(i)&&distance_[i]<=fuel&&xo_fuel(distance_[i])<=fuel){
                int p=good_price_at(i,g,(u32)visits+1)-good_price(g);
                if(p>max||(p>0&&p==max&&distance_[i]<bd)){max=p;bd=distance_[i];want=i;}
            }
            assert(best==want&&gain==max);markets++;
        }
    }
    printf("Hazard names, guidance, %u radar cases, %u clipped lines, %u market cases PASS\n",radar,lines,markets);
}
"""
code=prefix+enum+"\n"+table+prices+"\n"+mul+"\n"+fn(game,"static const char *floor_event_name(")
for m in ("static const char *sp_nav_direction(","static void sp_scanner_unroll(","static int l2div(","static void line2(","static int good_price_at(","static int good_price(","static int market_best_sale("):
    code+="\n"+fn(space,m)
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp); (p/"test.c").write_text(code+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-fsanitize=undefined","-fno-sanitize-recover=all",str(p/"test.c"),"-o",str(p/"test")],check=True)
    subprocess.run([str(p/"test")],check=True)
