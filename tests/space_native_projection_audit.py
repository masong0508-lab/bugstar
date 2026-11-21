#!/usr/bin/env python3
"""Compile and exercise the *actual* native space wireframe clipping functions.

The PC and Android high-resolution views used to omit edges with one vertex
behind the near plane, even though the GBA renderer clipped them. This harness
extracts the production functions so that an accidental return to the old
both-endpoints-visible shortcut breaks CI, without requiring SDL or a ROM.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "pc/pc_snative.inc").read_text()


def function(name):
    marker = "static " + name
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise AssertionError("unterminated function: " + name)


clip = function("int so_screenclip(")
wire = function("void so_wire(")
assert "if(az<=20 && bz<=20) continue;" in wire
assert "az=21;" in wire and "bz=21;" in wire
assert "so_screenclip(&x0,&y0,&x1,&y1)" in wire
assert "if(ok[a]&&ok[b])" not in wire
assert "if(!next_buf) return;" in source
assert "if(!next_ibuf) return;" in source
assert "if(!next_msk) return;" in source
assert "need>SIZE_MAX/sizeof(*pc_sn.buf)" in source

harness = r"""
#include <assert.h>
#include <math.h>
#include <stdint.h>
typedef int32_t s32;
typedef uint8_t u8;
#define FOCS 160
#define VCY 64

static int calls;
static double x0seen, y0seen, x1seen, y1seen;
static int cam3(s32 x,s32 y,s32 z,s32 *cx,s32 *cy,s32 *cz){
    *cx=x; *cy=y; *cz=z;
    return z>20;
}
static void so_line(double x0,double y0,double x1,double y1,int col){
    (void)col;
    assert(isfinite(x0) && isfinite(x1) && isfinite(y0) && isfinite(y1));
    assert(x0>=-2.00001 && x0<=241.00001);
    assert(x1>=-2.00001 && x1<=241.00001);
    assert(y0>=-2.00001 && y0<=129.00001);
    assert(y1>=-2.00001 && y1<=129.00001);
    calls++; x0seen=x0; y0seen=y0; x1seen=x1; y1seen=y1;
}
""" + clip + "\n" + wire + r"""

static void check(const s32 *v,int expected){
    static const u8 edges[]={0,1};
    calls=0; so_wire(v,2,edges,1,54);
    assert(calls==expected);
}
static uint32_t seed=0xB6A5C441u;
static int ran(int n){
    seed=seed*1664525u+1013904223u;
    return (int)(seed%(uint32_t)n);
}
int main(void){
    static const s32 front[]={0,0,100, 20,20,200};
    static const s32 near_ab[]={0,0,-30, 50,0,70};
    static const s32 near_ba[]={50,0,70, 0,0,-30};
    static const s32 behind[]={0,0,-100, 20,0,20};
    static const s32 outside[]={800,0,100, 1000,0,200};
    static const s32 spanning[]={-1000,0,100, 1000,0,100};
    static const s32 near_wide[]={-10000,0,-40, 0,0,100};
    check(front,1);
    check(near_ab,1);
    check(near_ba,1);
    check(behind,0);
    check(outside,0);
    check(spanning,1);
    assert(x0seen<=-1.999 && x1seen>=240.999);
    check(near_wide,1);
    for(int i=0;i<12000;i++){
        s32 v[6]={ran(40001)-20000,ran(40001)-20000,ran(14001)-4000,
                  ran(40001)-20000,ran(40001)-20000,ran(14001)-4000};
        static const u8 edges[]={0,1};
        calls=0;
        so_wire(v,2,edges,1,51);
        assert(calls<=1);
    }
    return 0;
}
"""

compiler = shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise RuntimeError("native 3D camera regression test requires a host C compiler")
with tempfile.TemporaryDirectory(prefix="bugstar-3d-") as tmp:
    src = Path(tmp) / "wire.c"
    out = Path(tmp) / "wire"
    src.write_text(harness)
    subprocess.run(
        [compiler, "-std=c99", "-O1", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=undefined", "-fno-sanitize-recover=all",
         str(src), "-lm", "-o", str(out)],
        check=True,
    )
    subprocess.run([str(out)], check=True)
print("Native PC/Android 3D: near-plane, viewport, reversed edges, 12k randomized segments and allocation guards PASS")
