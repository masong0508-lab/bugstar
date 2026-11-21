#!/usr/bin/env python3
"""Test the REAL shared GBA/PC/Android kart turn advisory under UBSan.

It must announce genuine upcoming bends, handle angle wraparound, avoid
flickering, and distinguish turns that deserve a slow-down warning.
"""
from pathlib import Path
import subprocess
import tempfile

src = (Path(__file__).resolve().parents[1] / "kart.inc").read_text()
sig = "static inline int kart_bend_cue("
start = src.find(sig)
assert start >= 0, "kart_bend_cue missing"
opening = src.index("{", start)
level = 0
for i in range(opening, len(src)):
    if src[i] == "{":
        level += 1
    elif src[i] == "}":
        level -= 1
        if level == 0:
            function = src[start : i + 1]
            break
else:
    raise AssertionError("Unbalanced bend helper")
assert "kart_bend_cue(kp_dir((p+4)%knp)" in src, "live HUD must use this helper"
assert "if(sharp) text(pg," in src and '"SLOW"' in src, "slow-down warning missing"
hud = src[src.index("static void kart_hud(") : src.index("static void kart_reset(")]
assert "((frames>>3)&1)" not in hud, "turn warnings must not blink"
assert "int nx=x+((sinx(khd>>2)*3)>>14), ny=y+((cosx(khd>>2)*3)>>14);" in src, (
    "the minimap must show real kart heading, not just a position square"
)
assert "if(nx>=194 && nx<=234 && ny>=21 && ny<=61)" in src, (
    "the kart direction marker must stay inside the 44x44 minimap"
)
assert "if(cnt>12) for(int step=2;step<=12;step+=2)" in src, (
    "the upcoming road segment must be highlighted on the minimap"
)
assert "if(i>=cnt) i=cnt-1; if(i<0) i=0;" in src, (
    "racer icons must not index unwritten path points on odd-length tracks"
)
map_sig = "static inline int kart_map_ahead_idx("
map_start = src.index(map_sig)
map_open = src.index("{", map_start)
map_depth = 0
for i in range(map_open, len(src)):
    if src[i] == "{":
        map_depth += 1
    elif src[i] == "}":
        map_depth -= 1
        if map_depth == 0:
            map_function = src[map_start : i + 1]
            break
else:
    raise AssertionError("Unbalanced minimap look-ahead helper")

test = r"""
#include <stdio.h>
#include <stdint.h>
typedef int32_t s32;
#define K_VBASE 0x3200
""" + function + "\n" + map_function + r"""
static int ref(int d0,int d1,int d2,int speed){
    if(speed<K_VBASE/6) return 0;
    int a1=d1-d0,a2=d2-d1;
    while(a1>511) a1-=1024;
    while(a1<-512) a1+=1024;
    while(a2>511) a2-=1024;
    while(a2<-512) a2+=1024;
    int near=a1<0?-a1:a1,far=a2<0?-a2:a2;
    int a=near>45?a1:a2;
    if(a>=-45 && a<=45) return 0;
    int sharp=(near>120||far>150)&&speed>K_VBASE/2;
    return (a>0?1:-1)*(sharp?2:1);
}
static int check(int a,int b,int c,int speed){
    int got=kart_bend_cue(a,b,c,speed), want=ref(a,b,c,speed);
    if(got!=want){
        fprintf(stderr,"cue(%d,%d,%d,%d): %d expected %d\n",a,b,c,speed,got,want);
        return 1;
    }
    return 0;
}
int main(void){
    if(kart_bend_cue(100,110,120,12000)!=0) return 1;
    if(kart_bend_cue(1000,50,80,8000)!=1) return 2;  // right across 1023->0
    if(kart_bend_cue(50,1000,965,8000)!=-1) return 3; // left across zero
    if(kart_bend_cue(100,260,400,12000)!=2) return 4;
    if(kart_bend_cue(500,300,160,12000)!=-2) return 5;
    if(kart_bend_cue(100,260,400,500)!=0) return 6; // no useless stationary warnings
    // The FIRST direction wins on an S-bend; a sharper second bend can
    // still trigger SLOW while the current arrow points the correct way.
    if(kart_bend_cue(0,60,924,10000)!=2) return 8; // right first, later sharp left
    if(kart_bend_cue(0,964,100,10000)!=-2) return 9; // left first, later sharp right
    if(kart_bend_cue(0,10,864,10000)!=-2) return 10; // first stretch straight, then left
    int mapchecks=0;
    for(int cnt=13;cnt<=300;cnt++) for(int pi=0;pi<=2*cnt;pi++) {
        for(int step=2;step<=12;step+=2) {
            int got=kart_map_ahead_idx(pi,step,cnt);
            int expected=((pi/2)%cnt+step)%cnt;
            if(got!=expected || got<0 || got>=cnt){
                fprintf(stderr,"map ahead pi %d step %d count %d => %d expected %d\n",
                        pi,step,cnt,got,expected); return 11;
            }
            mapchecks++;
        }
    }
    unsigned state=0xacc34597u; int checks=9;
    for(int i=0;i<20000;i++){
        state^=state<<13;state^=state>>17;state^=state<<5; int a=(int)(state&1023u);
        state^=state<<13;state^=state>>17;state^=state<<5; int b=(int)(state&1023u);
        state^=state<<13;state^=state>>17;state^=state<<5; int c=(int)(state&1023u);
        state^=state<<13;state^=state>>17;state^=state<<5; int v=(int)(state%30000u)-2000;
        if(check(a,b,c,v)) return 7;
        checks++;
    }
    printf("Kart turn advisory: %d UBSan cases, minimap look-ahead: %d cases PASS\n",checks,mapchecks);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-turn-") as td:
    exe = str(Path(td) / "turn")
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    "-x", "c", "-", "-o", exe],
                   input=test, text=True, check=True)
    subprocess.run([exe], check=True)
