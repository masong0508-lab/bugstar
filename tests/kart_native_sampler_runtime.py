#!/usr/bin/env python3
"""Pixel-for-pixel regression of the real native kart bilinear fast path.

Compiles the sampler from pc_kartnat_r.inc twice under UBSan: with and
without its flat-texel fast path. Checks flat roads, repeated stripes and
random terrain in the actual 240x160 source texture layout.
"""
from pathlib import Path
import re
import subprocess
import tempfile

src=(Path(__file__).resolve().parents[1]/"pc/pc_kartnat_r.inc").read_text()

def extract(name):
    m=re.search(r"static inline (?:uint32_t|int) "+name+r"\s*\([^)]*\)\s*\{",src)
    assert m, "native kart sampler missing "+name
    level=0
    for i in range(src.index("{",m.start()),len(src)):
        if src[i]=="{":level+=1
        elif src[i]=="}":
            level-=1
            if level==0:return src[m.start():i+1]
    raise AssertionError("Unbalanced C sampler: "+name)

actual=extract("kn_bilx")
marked=extract("kn_roadmark")
fast="if(a==b && a==c && a==d) return a;"
assert actual.count(fast)==1, "flat color fast path missing"
reference=actual.replace(fast,"").replace("kn_bilx(","kn_bilx_reference(",1)
assert actual!=reference
assert "if(ifx>=200 && kn_roadmark(a)) return a;" in actual, (
    "nearby road markings must retain their full source color"
)
harness=r"""
#include <stdint.h>
#include <stdio.h>
static uint32_t pg888[2][160][240];
static uint32_t kn_marks[4]={0x153417u,0xC62A31u,0xF1F2E1u,0xFFD02Au};
""" + marked + "\n" + actual + "\n" + reference + r"""
static uint32_t state=0x52ab0d17u;
static uint32_t rnd(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static int test(unsigned n){
    double u=(rnd()%(240u*256u))/256.0, v=(rnd()%(160u*256u))/256.0;
    int ifx=24+(int)(rnd()%720u);
    uint32_t a=kn_bilx(u,v,ifx),b=kn_bilx_reference(u,v,ifx);
    if(a!=b){
        fprintf(stderr,"kart native bilinear mismatch case %u at u %.3f v %.3f ifx %d: %08x != %08x\n",
                n,u,v,ifx,(unsigned)a,(unsigned)b);
        return 1;
    }
    return 0;
}
int main(void){
    unsigned checks=0;
    for(int stage=0;stage<3;stage++){
        for(int y=0;y<160;y++)for(int x=0;x<240;x++){
            uint32_t rgb=0x2A3B4Cu;
            if(stage==1) rgb=(y/10+x/13)%4==0?kn_marks[2]:((y/12+x/11)%2?0x191D25u:0x34363Au);
            if(stage==2) rgb=rnd()&0xFFFFFFu;
            pg888[1][y][x]=rgb;
        }
        for(int i=0;i<30000;i++){
            if(test(checks))return 1;
            checks++;
        }
    }
    printf("Native kart bilinear flat-color optimization: %u pixel-identical UBSan checks PASS\n",checks);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-kart-bil-") as td:
    exe=str(Path(td)/"sampler")
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-fno-sanitize-recover=all",
                    "-x","c","-","-o",exe],
                   input=harness,text=True,check=True)
    subprocess.run([exe],check=True)
