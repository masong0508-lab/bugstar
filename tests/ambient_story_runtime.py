#!/usr/bin/env python3
"""Exercise production Bugstar ambient story and source-backed economy hints.

This pass fixes gossip and lounge lines contradicting the quest state. No
per-frame jobs, save fields, sprites, or framebuffers: the two regulars'
dialogue changes only while the station's existing menu redraws.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
space=(ROOT/"space.inc").read_text()
town=(ROOT/"town.inc").read_text()
game=(ROOT/"game.inc").read_text()
main=(ROOT/"main.c").read_text()

def function(source,signature):
    m=re.search(re.escape(signature)+r"[^;{}]*\{",source)
    assert m, f"missing production C definition: {signature}"
    at=source.index("{",m.start())
    depth,end=1,at+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[m.start():end]

route=function(space,"static int bolt_next_step(")
arc=function(space,"static const char *lounge_arc_line(")
need=function(town,"static int gossip_need(")
helpers="\n".join(function(game,"static void "+name+"(")
                  for name in ("scpy","txt_copy","txt_append","txt_number"))

# Keep a 40-entry NPC vocabulary and 32 rumor cycle. The regulars' lines
# must fit the 52-byte quote buffer; a long location name must stay bounded.
def literals(blob):
    return re.findall(r'"((?:[^"\\]|\\.)*)"',blob)

gossip_arr=town.split("static const char *const gossip[NGOS]={",1)[1].split("};",1)[0]
locals_arr=town.split("static const char *const talk_ln[NTALK]={",1)[1].split("};",1)[0]
assert len(literals(gossip_arr))==32 and len(literals(locals_arr))==40
assert all(len(x)<65 for x in literals(gossip_arr))
assert all("HIVES FEED THE BEAST" not in x and "DROID'S MEMORY" not in x
           for x in literals(gossip_arr)), "unearned late-game spoiler"
assert "THE BOUNTY OFFICE PAYS LATE" not in town
assert "A FULL HOLD EARNS NOTHING" not in town
assert "THE BEAST GROWS WHEN YOU IGNORE ITS HIVES" not in town
assert "THE ANT KING SNORES" not in town
assert "THE TECHNOBEAST? NEVER HEARD" not in town
assert "static int hive_done;" in main, "town lore needs forward-declared chapter state"
assert "bolt_next_step()" in need
assert "lounge_arc_line(act,0)" in space and "lounge_arc_line(act,3)" in space
assert "int act=bolt_next_step();" in space
assert "if(act==5)" in space and "has_base(i)" in space, "live outpost locator lost"
assert "txt_copy(rb,sizeof rb" in space and "txt_append(rb,sizeof rb" in space
assert 'txt_copy(q,sizeof q' in space and 'txt_append(q,sizeof q' in space
assert "THE KING IS AFRAID OF YOU" not in space
assert "A BIGGER HOLD PAYS FOR ITSELF" not in space
assert "TECH WORLDS PAY FOR AMBER AND CRYSTALS" not in space
assert 'e==1 ? "MINERS PAY MORE FOR FOOD AND MACHINES."' in space
assert 'e==2 ? "TECH WORLDS PAY MORE FOR FOOD AND NECTAR."' in space
assert 'e==3 ? "HIVES PAY MORE FOR MACHINES AND CRYSTALS."' in space
assert '"FARMS PAY MORE FOR MACHINES AND MEDICINE."' in space
# Validate each specific piece of market advice against the actual economy
# values, instead of merely accepting the prose.
eco=re.search(r"static const u8 eco_mul\[4\]\[8\]=\{([^;]+)\};",space)
assert eco
weights=[list(map(int,re.findall(r"\d+",row)))
         for row in re.findall(r"\{([^{}]+)\}",eco.group(1))]
assert len(weights)==4 and all(len(row)==8 for row in weights)
for kind,goods in ((0,(5,6)),(1,(0,5)),(2,(0,1)),(3,(5,4))):
    assert all(weights[kind][i]>100 for i in goods), (
        f"trading advice wrong for economy {kind}: {goods}")

stub=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static int ant_king,endless,zone_cleared,zone,beast_stage,hive_done,data_cores,dawn_free;
static int hearts,maxh=100,food=100,cargo_cap=50,bag_n;
static int bonus;
static int cargo_n(void){return bag_n;}
static int ship_hold_bonus(void){return bonus;}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"Ambient narrative test line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
cases=r"""
static const char *const stage_hint[8]={
    "CAGED BUGS", "AUTOPILOT", "ANTHILL", "LAUNCH",
    "HIVE ", "MEMORIES:", "HIVES DOWN", "DAWN IS FREE"
};
int main(void){
    int stories=0;
    for(int stage=0;stage<8;stage++){
        for(int line=0;line<4;line++){
            const char *s=lounge_arc_line(stage,line);
            CHECK(s && strlen(s)>10 && strlen(s)<50);
            if(stage<3){
                CHECK(!strstr(s,"DAWN") && !strstr(s,"TECH HIVE"));
                CHECK(!strstr(s,"BOLT'S MEMORIES"));
            }
            if(stage==4 && line==0)CHECK(strstr(s,"TECH HIVES"));
            if(stage==4 && line==1)CHECK(strstr(s,"OVERLORD"));
        }
    }
    CHECK(!strcmp(lounge_arc_line(-1,0),""));
    CHECK(!strcmp(lounge_arc_line(8,0),""));
    CHECK(!strcmp(lounge_arc_line(0,4),""));

    for(int king=0;king<2;king++)
    for(int z=0;z<6;z++)
    for(int cleared=0;cleared<2;cleared++)
    for(int en=0;en<2;en++)
    for(int beast=0;beast<4;beast++)
    for(int hive=0;hive<=5;hive++)
    for(int cores=0;cores<=8;cores++)
    for(int done=0;done<2;done++){
        ant_king=king;zone=z;zone_cleared=cleared;endless=en;
        beast_stage=beast;hive_done=hive;data_cores=cores;dawn_free=done;
        hearts=maxh;food=100;bag_n=0;bonus=0;
        int stage=bolt_next_step();
        struct{unsigned char left;char message[40];unsigned char right;} guard={0};
        guard.left=0xA5;guard.right=0x5A;
        int ok=gossip_need(guard.message);
        CHECK(ok==1 && guard.left==0xA5 && guard.right==0x5A);
        CHECK(strlen(guard.message)<40);
        CHECK(strstr(guard.message,stage_hint[stage]));
        if(stage==4){
            char number[12];snprintf(number,sizeof number,"%u OF 5",(unsigned)(hive+1));
            CHECK(strstr(guard.message,number));
        }
        if(stage==5){
            char number[12];snprintf(number,sizeof number,"%u/8",(unsigned)cores);
            CHECK(strstr(guard.message,number));
        }
        CHECK(strlen(lounge_arc_line(stage,0))<50);
        stories++;
    }
    // Actual immediate safety advice still beats the optional story rumor.
    ant_king=1;beast_stage=3;hive_done=5;data_cores=8;dawn_free=1;
    char b[40];hearts=10;gossip_need(b);CHECK(strstr(b,"HURT"));
    hearts=maxh;food=9;gossip_need(b);CHECK(strstr(b,"HUNGRY"));
    food=100;bag_n=cargo_cap;gossip_need(b);CHECK(strstr(b,"HOLD IS FULL"));
    bag_n=0;bonus=15;bag_n=cargo_cap;gossip_need(b);CHECK(strstr(b,"DAWN IS FREE"));
    printf("Ambient story: %d actual chapter combinations, 32 lounge lines, market economy and bounded rumor safety PASS\n",stories);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-ambient-story-") as td:
    source=Path(td)/"story.c"
    exe=Path(td)/"story"
    source.write_text(stub+"\n"+helpers+"\n"+route+"\n"+arc+"\n"+need+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                    "-fno-omit-frame-pointer",str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
