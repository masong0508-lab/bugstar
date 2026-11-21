#!/usr/bin/env python3
"""Compile and exercise Bugstar's ACTUAL quest_home/add_gear C functions.

Only their external dependencies are stubbed. This avoids a second copy of
the quest logic and tests runtime state transitions directly. ROM/PC playthrough
and full engine build checks are still separate requirements.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "game.inc").read_text()


def extract(signature):
    m = re.search(r"\b" + re.escape(signature) + r"\s*\{", source)
    assert m, "C function not found: " + signature
    start = m.start()
    pos = m.end() - 1
    depth = 0
    for i in range(pos, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start : i + 1]
    raise AssertionError("Unclosed C function: " + signature)


quest_home = extract("static int quest_home(void)")
add_gear = extract("static int add_gear(Gear g)")
prefix = r"""
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
typedef unsigned char u8;
typedef struct { u8 it,sub,rar,pre,suf,lvl; } Gear;
#define BAGMAX 120
enum { IT_NONE, IT_BERRY, IT_SEED, IT_SHELL, IT_THORN, IT_CRUMB,
       IT_WARP, IT_GEM, IT_CHARM, IT_BOOK, IT_FISH, IT_POTION, IT_QUEST, IT_KEY };
enum { Q_NONE, Q_HUNT, Q_CHAMP, Q_FETCH, Q_RESCUE, Q_BOSS,
       Q_SHARDS, Q_BRING, Q_GOODS, NQ };
static int q_type,q_need,q_have,q_floor,q_done,q_zone,q_world;
static int zone,world_no,depth_no,forge_on,base_mode;
static Gear bag[BAGMAX]; static int bag_n,bag_sel;
static char last_msg[128];
static void scpy(char*d,const char*s){ strcpy(d,s); }
static void scat(char*d,const char*s){ strcat(d,s); }
static void ncat(char*d,int v){ char tmp[32]; snprintf(tmp,sizeof tmp,"%d",v); strcat(d,tmp); }
static void say(const char*s){ snprintf(last_msg,sizeof last_msg,"%s",s); }
static void sfx(int i){ (void)i; }
static void hud_now(void){}
static void gear_name(const Gear*g,char*b){ snprintf(b,40,"ITEM %u",g->it); }
"""
tests = r"""
#define CHECK(x) do { if(!(x)){fprintf(stderr,"quest C test failed at line %d: %s\n",__LINE__,#x); return 1;} } while(0)
static void reset(void){
    zone=2; world_no=4; q_zone=2; q_world=4;
    forge_on=0; base_mode=0; depth_no=3;
    q_type=Q_NONE; q_need=0; q_have=0; q_floor=3; q_done=0;
    bag_n=0; bag_sel=0; last_msg[0]=0;
}
int main(void){
    Gear relic={IT_QUEST,0,0,0,0,0}, potion={IT_POTION,0,0,0,0,0};
    reset(); q_type=Q_SHARDS; q_floor=4; q_need=3; depth_no=2;
    CHECK(add_gear(relic)==1 && q_have==1 && !q_done);
    CHECK(add_gear(relic)==1 && q_have==1 && !q_done); // can't farm same floor
    depth_no=4; CHECK(add_gear(relic)==1 && q_have==1); // cannot skip B3
    depth_no=3; CHECK(add_gear(relic)==1 && q_have==2 && !q_done);
    depth_no=4; CHECK(add_gear(relic)==1 && q_have==3 && q_done);
    CHECK(add_gear(relic)==1 && q_have==3); // completion isn't repeatable
    reset(); q_type=Q_FETCH; q_floor=5; depth_no=4;
    CHECK(add_gear(relic)==1 && !q_done); // wrong floor
    depth_no=5; q_zone=1;
    CHECK(add_gear(relic)==1 && !q_done); // wrong zone
    q_zone=2; q_world=3;
    CHECK(add_gear(relic)==1 && !q_done); // wrong world
    q_world=4; forge_on=1;
    CHECK(add_gear(relic)==1 && !q_done); // special forge not valid
    forge_on=0; base_mode=1;
    CHECK(add_gear(relic)==1 && !q_done); // outpost not valid
    base_mode=0; CHECK(add_gear(relic)==1 && q_done); // correct floor/world
    reset(); q_type=Q_HUNT; CHECK(add_gear(relic)==1 && !q_done);
    CHECK(strstr(last_msg,"NOT YOUR QUEST TARGET")!=NULL);
    reset(); bag_n=BAGMAX; CHECK(add_gear(potion)==0 && bag_n==BAGMAX);
    CHECK(strstr(last_msg,"BAG IS FULL")!=NULL);
    reset(); CHECK(add_gear(potion)==1 && bag_n==1 && bag[0].it==IT_POTION);
    puts("Quest C runtime: authentic relic and bag paths PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-quest-") as d:
    src = Path(d) / "quest_test.c"
    exe = Path(d) / "quest_test"
    src.write_text(prefix + "\n" + quest_home + "\n" + add_gear + "\n" + tests)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
