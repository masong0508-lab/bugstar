#!/usr/bin/env python3
"""Verify the real native kart camera and road-marking helpers under UBSan.

The native 3D renderer reads kart speed and stabilizes the horizon when moving
slowly, without changing simulation physics or native 240x160 game UI.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
src=(root/"pc/pc_kartnat_r.inc").read_text()
snapshot=(root/"pc/pc_kartnat.inc").read_text()
header=(root/"pc/pc_hw.h").read_text()

def func(name):
    m=re.search(r"static (?:inline )?(?:int|double) "+name+r"\s*\([^)]*\)\s*\{",src)
    assert m, "missing real native helper "+name
    depth=0
    for i in range(src.index("{",m.start()),len(src)):
        if src[i]=="{":
            depth+=1
        elif src[i]=="}":
            depth-=1
            if depth==0:
                return src[m.start():i+1]
    raise AssertionError("unbalanced native helper "+name)

helper="\n".join(func(n) for n in ("kn_target_roll","kn_hill_shift","kn_roadmark"))
assert "kn_target_roll(dh,curvahead,speed)" in src
assert "sh=kn_hill_shift(-hr*KN_FOCP/z8)" in src
assert "if(ifx>=200 && kn_roadmark(a)) return a;" in src
assert "{98,99,100,103}" in src, "the actual road kerb/line/pad indices must be sampled"
assert "pc_kn_frame(m7_rowz,1,pi*256+fr,heading,kcv,kv,elev," in snapshot
assert "int curvahead, int speed, int elev, unsigned seed" in header
assert "int curvahead, int speed, int elev, unsigned seed" in src
assert "pc_kn_frame(m7_rowz,0,0,0,0,0,0,0," in snapshot

test=r"""
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
typedef uint32_t u32;
static uint32_t kn_marks[4]={0x123456u,0xABCDEFu,0x00FF00u,0xF0CA20u};
""" + helper + r"""
#define CHECK(cond) do {if(!(cond)){fprintf(stderr,"failed at line %d: %s\n",__LINE__,#cond);return 1;}}while(0)
static int near(double x,double y){return fabs(x-y)<0.0000001;}
int main(void) {
    CHECK(near(kn_target_roll(90,220,0),0)); // no gratuitous tilt at a standstill
    CHECK(near(kn_target_roll(90,220,-0),0));
    CHECK(kn_target_roll(15,0,8000)<0);
    CHECK(kn_target_roll(-15,0,8000)>0);
    CHECK(kn_target_roll(15,0,-8000)<0); // reverse follows actual movement magnitude
    CHECK(fabs(kn_target_roll(100000,300000,INT_MAX))<=0.09000001);
    CHECK(fabs(kn_target_roll(-100000,-300000,INT_MIN))<=0.09000001);
    CHECK(fabs(kn_target_roll(40,100,2500))<fabs(kn_target_roll(40,100,12000)));
    CHECK(near(kn_hill_shift(22),14));
    CHECK(near(kn_hill_shift(-22),-14));
    CHECK(near(kn_hill_shift(7.5),7.5));
    CHECK(near(kn_hill_shift(-12.5),-12.5));
    CHECK(kn_roadmark(kn_marks[0]));
    CHECK(kn_roadmark(kn_marks[1]));
    CHECK(kn_roadmark(kn_marks[2]));
    CHECK(kn_roadmark(kn_marks[3]));
    CHECK(!kn_roadmark(0xFF0000u));
    CHECK(!kn_roadmark(0x000000u));
    unsigned state=0x483ea941u;
    for(int i=0;i<20000;i++){
        state^=state<<13; state^=state>>17; state^=state<<5;
        int dh=(int)(state&1023u)-511;
        state^=state<<13; state^=state>>17; state^=state<<5;
        int curve=(int)(state&2047u)-1023;
        state^=state<<13; state^=state>>17; state^=state<<5;
        int speed=(int)(state%40000u)-10000;
        double a=kn_target_roll(dh,curve,speed);
        double b=kn_target_roll(dh,curve,-speed);
        CHECK(isfinite(a));
        CHECK(fabs(a)<=0.090000001);
        CHECK(near(a,b));
        CHECK(kn_hill_shift(a*1000)<=14.000001);
        CHECK(kn_hill_shift(a*1000)>=-14.000001);
    }
    printf("Native kart camera, road marks and hill bound: 20,018 UBSan cases PASS\n");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-kart-camera-") as td:
    exe=str(Path(td)/"native-camera")
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-fno-sanitize-recover=all",
                    "-x","c","-","-lm","-o",exe],
                   input=test,text=True,check=True)
    subprocess.run([exe],check=True)
