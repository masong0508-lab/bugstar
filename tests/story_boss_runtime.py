#!/usr/bin/env python3
"""Execute the original story-boss first-clear code under UBSan.

This extracts the same C function linked into GBA/PC/Android; the small
host harness replaces unrelated ship/galaxy systems with deterministic stubs.
It checks unique rewards, old cleared_mask saves and world progression.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
space = (root/"space.inc").read_text()
sig = "static void story_boss_beaten(void)"
start = space.index(sig + "{")
at = space.index("{",start)
depth = 0
end = None
for i in range(at,len(space)):
    if space[i]=="{":
        depth += 1
    elif space[i]=="}":
        depth -= 1
        if depth==0:
            end=i+1
            break
assert end is not None, "story boss C function not balanced"
fn=space[start:end]

prefix = r"""
#include <stdint.h>
#include <stdio.h>
static int cleared_mask, zones_open, zone, ant_king;
static int food, fuel, hull, scrap, gold, awards, galaxy_calls, seed_calls;
static long long hearts, maxh;
#define CHECK(e) do { if(!(e)){ fprintf(stderr,"boss reward line %d: %s\n",__LINE__,#e); return 1; } } while(0)
static int fuel_max(void){return 100;}
static void earn(int amt){ gold+=amt; awards++; }
static uint32_t rnd(void){return 123u;}
static void uni_set32(uint32_t seed){ (void)seed;seed_calls++; }
static void gen_cluster(void){galaxy_calls++;}
"""

suffix = r"""
static void setup(int z){
    zone=z; cleared_mask=0; zones_open=1; ant_king=0;
    food=42; fuel=30; hull=50; scrap=7; gold=10; awards=0;
    hearts=14; maxh=80; galaxy_calls=seed_calls=0;
}
int main(void){
    for(int z=0;z<6;z++){
        setup(z);
        story_boss_beaten();
        CHECK((cleared_mask&(1<<z))!=0);
        CHECK(zones_open==(z+2<6?z+2:6));
        switch(z){
          case 0: CHECK(food==100); break;
          case 1: CHECK(fuel==50); break;
          case 2: CHECK(gold==160 && awards==1); break;
          case 3: CHECK(hull==75); break;
          case 4: CHECK(scrap==31); break;
          case 5: CHECK(hearts==maxh); CHECK(ant_king==1);
                  CHECK(galaxy_calls==1 && seed_calls==1); break;
        }
        int f=food,fu=fuel,h=hull,sc=scrap,g=gold,ac=awards,cl=galaxy_calls;
        long long hp=hearts;
        story_boss_beaten();
        CHECK(food==f && fuel==fu && hull==h && scrap==sc);
        CHECK(gold==g && awards==ac && hearts==hp);
        CHECK(galaxy_calls==cl);
    }
    // Legacy saves already have this bit set and must not double-pay.
    setup(2); cleared_mask=1<<2; zones_open=4;
    story_boss_beaten();
    CHECK(gold==10 && awards==0 && zones_open==4);
    setup(4); cleared_mask=1<<4; zones_open=6;
    story_boss_beaten();
    CHECK(scrap==7 && zones_open==6);

    // Recovery must never lower a previously upgraded value.
    setup(1); fuel=125; story_boss_beaten(); CHECK(fuel==125);
    setup(3); hull=120; story_boss_beaten(); CHECK(hull==120);
    setup(0); food=155; story_boss_beaten(); CHECK(food==155);
    setup(5); hearts=120; story_boss_beaten(); CHECK(hearts==120);

    puts("Boss progression runtime: six one-time recoveries, old saves, repeat tiers, unlocks PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-story-boss-") as tmp:
    c=Path(tmp)/"story_boss.c"
    exe=Path(tmp)/"story_boss"
    c.write_text(prefix+"\n"+fn+"\n"+suffix)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-fno-sanitize-recover=all",
                    str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
