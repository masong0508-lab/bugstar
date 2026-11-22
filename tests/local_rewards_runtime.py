#!/usr/bin/env python3
"""Run real Bugstar local-census and inventory fallback code under ASan/UBSan.

Census credit must be per distinct resident, even with empty pockets; high
galaxy seeds cannot index dialogue with a negative remainder. Inventory-full
gifts, plants and fossil bonuses always grant their documented fallback.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
town=(ROOT/"town.inc").read_text()
planet=(ROOT/"planet.inc").read_text()
game=(ROOT/"game.inc").read_text()

def func(source,signature):
    start=source.index(signature)
    at=source.index("{",start)
    depth=1
    end=at+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[start:end]

helpers="\n".join(func(town,"static "+signature) for signature in (
    "int local_line_index(", "int local_job_visit(", "void local_job_reset("))
credit=func(game,"static int add_or_credit(")
talk=func(town,"static void local_talk(")
assert "int li=local_line_index(o);" in talk, "dialogue must use unsigned index"
assert "local_job_visit(o);" in talk, "census must count distinct locals"
assert "if(pq_type==5) local_job_reset();" in town, "new census must reset visits"
assert "if(o->kind==O_LOCAL) tl = (o->var&1)" in planet, "visit flag must not change NPC art"
assert talk.count("if(add_or_credit(gg,5+dlv()*3))")==2
assert "BAG FULL! TAKE " in talk and "G INSTEAD." in talk
assert planet.count("if(add_or_credit(g,5))")==2
assert "if(add_or_credit(g,10))" in planet
for msg in ("BAG FULL. BONUS SHELL = +10G",
            "BAG FULL. BERRY = +5G","BAG FULL. SEED = +5G"):
    assert msg in planet, msg
assert "add_gear(g); show_msg(\"OLD BONES" not in planet, "phantom fossil reward"

fixture=r"""
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef int64_t s64;
enum { NPO=160, NTALK=40, O_LOCAL=15, O_OTHER=4 };
#define LOCAL_VISITED 0x80u
typedef struct { s32 x,y; u8 kind,alive,hp,var; } PObj;
typedef struct { u8 it,sub,rar,pre,suf,lvl; } Gear;
static PObj po[NPO];
static struct { u32 tseed; } wd;
static int pq_type,pq_done,pq_have;
static s64 gold;
static int bag_n,bag_max=6,hud_calls;
static void hud_now(void){ hud_calls++; }
static int add_gear(Gear gg){ (void)gg; if(bag_n>=bag_max) return 0; bag_n++; return 1; }
#define CHECK(cond) do {if(!(cond)){fprintf(stderr,"gameplay reward/census failure at %d: %s\n",__LINE__,#cond);return 1;}}while(0)
"""
cases=r"""
int main(void){
    PObj *a=&po[0], *b=&po[1];
    a->alive=b->alive=1; a->kind=b->kind=O_LOCAL;
    a->var=0; b->var=1;
    po[2].alive=1; po[2].kind=O_OTHER; po[2].var=0x81;
    pq_type=5; pq_done=0;
    CHECK(local_job_visit(a)==1);
    CHECK(local_job_visit(a)==0);
    CHECK(pq_have==1 && a->var==0x80);
    CHECK(local_job_visit(b)==1 && pq_have==2 && b->var==0x81);
    CHECK(local_job_visit(b)==0 && pq_have==2);
    pq_done=1; CHECK(local_job_visit(a)==0 && pq_have==2);
    local_job_reset();
    CHECK(a->var==0 && b->var==1 && po[2].var==0x81);
    pq_type=1; pq_done=0; CHECK(local_job_visit(a)==0 && pq_have==2);
    pq_type=5; pq_have=0; CHECK(local_job_visit(b)==1 && pq_have==1);
    // Reassigning a census clears the same resident's old visit record.
    local_job_reset(); pq_have=0; CHECK(local_job_visit(b)==1 && pq_have==1);
    // A full-bag item reward must pay out instead of being lost.
    Gear g={1,2,0,0,0,0};
    gold=0; bag_n=bag_max; hud_calls=0;
    CHECK(add_or_credit(g,17)==0);
    CHECK(gold==17 && bag_n==bag_max && hud_calls==1);
    CHECK(add_or_credit(g,5)==0 && gold==22 && bag_n==bag_max && hud_calls==2);
    // With room, receive the real item, not both the item and the gold.
    bag_n=bag_max-1;
    CHECK(add_or_credit(g,10)==1);
    CHECK(gold==22 && bag_n==bag_max && hud_calls==2);
    // A claimed census resident and a ready inventory reward are unrelated.
    CHECK(local_job_visit(b)==0 && pq_have==1);
    // Dialogue index stays inside NTALK for every sign bit, world boundary
    // and a broad deterministic mix of generated seeds and map coordinates.
    u32 z=0xE20A9713u;
    for(int i=0;i<200000;i++){
        z=z*1664525u+1013904223u;
        wd.tseed=(i&1)?z:(z|0x80000000u);
        a->x=(s32)((z&127u)<<16); a->y=(s32)(((z>>8)&127u)<<16);
        u32 key=(u32)(((a->x>>16)*7)+((a->y>>16)*13))+wd.tseed;
        int idx=local_line_index(a);
        CHECK(idx>=0 && idx<NTALK);
        CHECK(idx==(int)(key%NTALK));
    }
    wd.tseed=0xffffffffu; a->x=0; a->y=0;
    CHECK(local_line_index(a)==(int)(0xffffffffu%NTALK));
    printf("Local census, 200000 dialogue seeds, gift/plant/fossil inventory fallback: PASS\n");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-local-rewards-") as tmp:
    src=Path(tmp)/"local_test.c"
    exe=Path(tmp)/"local_test"
    src.write_text(fixture+"\n"+helpers+"\n"+credit+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-omit-frame-pointer",
                    "-fno-sanitize-recover=all",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
