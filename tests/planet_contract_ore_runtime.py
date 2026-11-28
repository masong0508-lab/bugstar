#!/usr/bin/env python3
"""Verify actual local job availability and cached ore selection."""
from pathlib import Path
import re, tempfile, subprocess
root=Path(__file__).resolve().parents[1]
town=(root/"town.inc").read_text()
ore=(root/"ore.inc").read_text()
planet=(root/"planet.inc").read_text()
def func(src,mark):
    a=src.index(mark);b=src.index("{",a)+1;depth=1
    while depth:
        depth+=(src[b]=="{")-(src[b]=="}");b+=1
    return src[a:b]
prefix=r"""
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;typedef uint32_t u32;typedef uint16_t u16;typedef int32_t s32;
#define NORE 12
#define NPO 160
enum {O_DOOR,O_SHIP,O_CAGE,O_SLUG,O_PLANT,O_TREE,O_ROCK,O_WEB,O_FLOWER,O_BASE,O_MARKET,O_CANTINA,O_OFFICE,O_RACE,O_HOME,O_LOCAL,O_FOSSIL,O_RUIN,O_BEETLE,O_CRIT,O_SHRINE};
typedef struct {const char *name;u8 tier,hard;u16 price;} OreDef;
typedef struct {s32 x,y;u8 kind,alive,hp,var;} PObj;
static PObj po[NPO];
static u8 pb_count[11],pb_choices[10],pb_ore;
static u16 ore_n[NORE];
static int tool;
static int ore_tool_eff(void){return tool;}
static int ore_of(s32 x,s32 y){(void)y;return x;}
static int ore_need_tool(int o){return o<6?1:2;}
static u8 ore_candidates[2][NORE],ore_candidate_n[2];
static int ore_cache_hab=-1,ore_cache_tier=-1;
"""
tables="\n".join(re.search(p,ore,re.S).group() for p in (r"static const OreDef ore_def\[NORE\]=\{.*?\};",r"static const u8 ore_hab\[NORE\]=\{.*?\};"))
cases=r"""
static int original(int hab,int maxt,u32 h){
    int cand[NORE],n=0;
    for(int i=4;i<NORE;i++)if((ore_hab[i]&hab)&&ore_def[i].tier<=maxt&&(ore_hab[i]!=32||(h&7)==0))cand[n++]=i;
    return n?cand[(h>>8)%n]:4;
}
int main(void){
    assert(local_offer_prepare()==0);
    po[0]=(PObj){.kind=O_SLUG,.alive=1};
    po[1]=(PObj){.kind=O_BEETLE,.alive=0};
    po[2]=(PObj){.kind=O_RACE,.alive=1};
    po[3]=(PObj){.kind=O_ROCK,.alive=1,.x=6};
    po[4]=(PObj){.kind=O_CRIT,.alive=1,.var=1};
    po[5]=(PObj){.kind=O_CRIT,.alive=1,.var=0};
    assert(local_offer_prepare()==4);
    assert(pb_count[1]==1&&pb_count[2]==0&&pb_count[4]==1&&pb_count[8]==1&&pb_count[6]==1&&pb_count[7]==0&&pb_count[10]==0);
    tool=2;assert(local_offer_prepare()==6&&pb_count[7]==1&&pb_count[10]==1&&pb_ore==6);
    ore_n[9]=5;assert(local_offer_prepare()==6&&pb_count[10]==5&&pb_ore==9);
    for(int t=1;t<=10;t++)for(int n=0;n<=160;n++)for(int extra=0;extra<=2;extra++){
        int v=local_offer_need(t,n,extra);assert(v>=0&&v<=n&&(!n||v>0));
    }
    memset(po,0,sizeof po);memset(ore_n,0,sizeof ore_n);
    for(int i=0;i<NPO;i++)po[i]=(PObj){.kind=O_PLANT,.alive=1};
    assert(local_offer_prepare()==1&&pb_count[9]==NPO&&pb_choices[0]==9);
    unsigned checks=0;
    for(int hab=0;hab<64;hab++)for(int tier=0;tier<=6;tier++)for(u32 i=0;i<4096;i++){
        u32 h=i*2654435761u+hab*17u;
        assert(ore_pick_cached(hab,tier,h)==original(hab,tier,h));checks++;
    }
    for(u32 i=0;i<10000;i++){
        int hab=i&63,tier=i%7;u32 h=i*2246822519u;
        assert(ore_pick_cached(hab,tier,h)==original(hab,tier,h));checks++;
    }
    printf("Local work availability and %u exact cached ore selections PASS\n",checks);
}
"""
assert "if(!o->alive)continue;" in func(town,"static int local_offer_prepare(")
assert "if(list_pick(pg,\"CHOOSE LOCAL WORK\"" in town
assert "ctim[best]=0" in planet and "ore_nav_reset();" in planet
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"test.c";exe=Path(td)/"test"
    c.write_text(prefix+tables+"\n"+"\n".join(func(town,x) for x in ("static void local_offer_count(","static int local_offer_need(","static int local_offer_prepare("))+"\n"+func(ore,"static int ore_pick_cached(")+cases)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
