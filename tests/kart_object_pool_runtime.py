#!/usr/bin/env python3
"""Exercise the game's kart object pool and track-editor gate reuse.

The allocation, release, restoration, and gate actions are extracted from the
shared game source. Only sound, messages, random scenery placement, and text
helpers are stubbed. This host regression does not claim emulator coverage.
"""
from pathlib import Path
import re
import subprocess
import tempfile


root = Path(__file__).resolve().parents[1]
kart = (root / "kart.inc").read_text()
editor = (root / "kart2.inc").read_text()
planet = (root / "planet.inc").read_text()


def function(source, signature):
    start = source.index(signature + "{")
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("unbalanced function " + signature)


def match(source, expression):
    found = re.search(expression, source)
    assert found, expression
    return found.group(0)


declarations = "\n".join([
    match(planet, r"#define TW \d+"),
    match(planet, r"#define NPO \d+"),
    match(planet, r"enum \{ O_DOOR,.*?\};"),
    match(planet, r"typedef struct \{[^\n]+\} PObj;"),
    match(kart, r"#define KG_MAX \d+"),
    match(kart, r"typedef struct \{[^\n]+\} KTrack;"),
    match(kart, r"EW static u8 kslot\[\d+\], khid\[\d+\];[^\n]+"),
    match(editor, r"EW static u8 kbeac\[KG_MAX\];"),
])

prefix = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef int32_t s32;
#define EW
"""

helpers = r"""
static PObj po[NPO];
static KTrack kt;
static s32 ppx, ppy;
static unsigned rng_calls;
static int rndi(int n){ rng_calls++; return (int)(rng_calls % (unsigned)n); }
static int cosx(int angle){ (void)angle; return 16384; }
static int sinx(int angle){ (void)angle; return 0; }
static void show_msg(const char *s){ (void)s; }
static void sfx(int s){ (void)s; }
static void scpy(char *dst,const char *src){ strcpy(dst,src); }
static void scat(char *dst,const char *src){ strcat(dst,src); }
static void ncat(char *dst,int n){ sprintf(dst+strlen(dst),"%d",n); }
"""

actual = "\n".join([
    function(kart, "static int kgrab(int kind, s32 x, s32 y)"),
    function(kart, "static void kpo_release(int i)"),
    function(kart, "static void kgive_back(void)"),
    function(kart, "static int kwrap8(int d)"),
    function(editor, "static void kbuild_drop(void)"),
    function(editor, "static void kbuild_undo(void)"),
])

checks = r"""
#define CHECK(c) do { if(!(c)){ fprintf(stderr,"Kart pool failed at line %d: %s\n",__LINE__,#c); return 1; } } while(0)
static void reset_world(void){
    memset(po,0,sizeof po);
    memset(&kt,0,sizeof kt);
    nkslot=nkhid=0; rng_calls=0;
    for(int i=0;i<NPO;i++){
        po[i].kind=i<64?O_TREE:(i&1)?O_PEBBLE:O_TUFT;
        po[i].alive=1; po[i].hp=3; po[i].var=(u8)i;
        po[i].x=i*3000; po[i].y=i*4000;
    }
}
static int unique_slots(void){
    for(int a=0;a<nkslot;a++) for(int b=0;b<a;b++)
        if(kslot[a]==kslot[b]) return 0;
    return 1;
}
int main(void){
    reset_world();
    PObj trees[64]; memcpy(trees,po,sizeof trees);
    int permanent[32]; PObj fixed[32];
    /* Twelve racers, fifteen boxes, two flags and four spectators borrow
       32 objects before the first shell or slime is launched. */
    for(int i=0;i<32;i++){
        int kind=i<11?O_KART:i<26?O_IBOX:i<28?O_BEACON:O_LOCAL;
        permanent[i]=kgrab(kind,i*512,i*1024);
        CHECK(permanent[i]>=0); fixed[i]=po[permanent[i]];
    }
    for(int launch=0;launch<10000;launch++){
        int kind=(launch&1)?O_KSHELL:O_KSLIME;
        int object=kgrab(kind,-1,(TW<<16)+17);
        CHECK(object>=0);
        CHECK(po[object].alive && po[object].kind==kind);
        CHECK(po[object].x==(TW<<16)-1 && po[object].y==17);
        CHECK(nkslot==33 && unique_slots());
        for(int i=0;i<32;i++) CHECK(memcmp(&po[permanent[i]],&fixed[i],sizeof(PObj))==0);
        kpo_release(object);
        CHECK(!po[object].alive && po[object].kind==O_PEBBLE);
    }
    CHECK(memcmp(trees,po,sizeof trees)==0);
    khid[0]=2; khid[1]=3; nkhid=2; po[2].alive=po[3].alive=0;
    u8 borrowed[48]; int count=nkslot; memcpy(borrowed,kslot,(size_t)count);
    kgive_back();
    CHECK(!nkslot && !nkhid && rng_calls==(unsigned)count*2);
    CHECK(memcmp(trees,po,sizeof trees)==0);
    for(int i=0;i<count;i++){
        PObj *o=&po[borrowed[i]];
        CHECK(o->alive && o->hp==3);
        CHECK(o->kind==((borrowed[i]&1)?O_PEBBLE:O_TUFT));
    }

    /* Full ownership must still permit retiring and replacing a weapon;
       sleeping item boxes remain reserved while waiting to respawn. */
    reset_world();
    for(int i=0;i<48;i++) CHECK(kgrab(i==0?O_IBOX:O_KSHELL,i,i)>=0);
    PObj full[NPO]; memcpy(full,po,sizeof full);
    CHECK(kgrab(O_BEACON,0,0)==-1 && nkslot==48);
    CHECK(memcmp(full,po,sizeof full)==0);
    int sleeping=kslot[0]; po[sleeping].alive=0;
    CHECK(kgrab(O_KSLIME,0,0)==-1);
    int released=kslot[17]; kpo_release(released); kpo_release(released);
    CHECK(kgrab(O_KSLIME,123,456)==released);
    CHECK(nkslot==48 && unique_slots());
    CHECK(po[sleeping].kind==O_IBOX && !po[sleeping].alive);
    kpo_release(-1); kpo_release(NPO); kpo_release(255);
    CHECK(nkslot==48 && unique_slots());

    /* Run the actual editor actions: keep three gates, then repeatedly
       place and undo a fourth. Its marker must never disappear. */
    reset_world();
    for(int i=0;i<3;i++){
        ppx=(s32)(32+i*16)<<15; ppy=(s32)32<<15;
        kbuild_drop(); CHECK(kt.n==i+1 && kbeac[i]!=255);
    }
    for(int edit=0;edit<1000;edit++){
        ppx=(s32)(80+(edit&1)*16)<<15; ppy=(s32)48<<15;
        kbuild_drop(); CHECK(kt.n==4 && kbeac[3]!=255);
        CHECK(po[kbeac[3]].alive && po[kbeac[3]].kind==O_BEACON);
        CHECK(nkslot==4 && unique_slots());
        kbuild_undo(); CHECK(kt.n==3 && !po[kbeac[3]].alive);
        for(int i=0;i<3;i++) CHECK(po[kbeac[i]].alive && po[kbeac[i]].kind==O_BEACON);
    }
    kgive_back(); CHECK(!nkslot && !nkhid && rng_calls==8);

    /* No eligible scenery: fail safely without borrowing gameplay objects. */
    reset_world();
    for(int i=0;i<NPO;i++) po[i].kind=O_TREE;
    memcpy(full,po,sizeof full);
    CHECK(kgrab(O_BEACON,0,0)==-1 && nkslot==0);
    CHECK(memcmp(full,po,sizeof full)==0);
    puts("Kart object pool: 10,000 item cycles, full-capacity reuse, 1,000 editor undo cycles, scenery restoration PASS");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-kart-pool-") as directory:
    source = Path(directory) / "kart_pool_test.c"
    executable = Path(directory) / "kart_pool_test"
    source.write_text(prefix + declarations + helpers + actual + checks)
    subprocess.run([
        "cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
        "-Wno-misleading-indentation",
        "-fsanitize=undefined", "-fno-sanitize-recover=all",
        str(source), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)
