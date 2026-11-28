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
    "racer icons must guard malformed path indices"
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
    for(int path_count=26;path_count<=600;path_count++) {
      int cnt=(path_count+1)/2;
      for(int pi=0;pi<path_count;pi++) {
        for(int step=2;step<=12;step+=2) {
            int got=kart_map_ahead_idx(pi,step,cnt);
            int expected=(pi/2+step)%cnt;
            if(got!=expected || got<0 || got>=cnt){
                fprintf(stderr,"map ahead pi %d step %d count %d => %d expected %d\n",
                        pi,step,cnt,got,expected); return 11;
            }
            mapchecks++;
        }
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


# Count arithmetic alone cannot catch a mismatch with the path generator.
# Compile the REAL builder and minimap consumers, replacing only drawing
# primitives with recorders. Exercise both drawing paths and every racer
# position on ordinary, odd-length and reversed generated circuits.
def extract_function(signature):
    begin = src.index(signature)
    opening = src.index("{", begin)
    depth = 0
    for i in range(opening, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[begin:i + 1]
    raise AssertionError("Unbalanced function: " + signature)


map_test = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
#define KG_MAX 24
#define KP_MAX 600
#define KMM_N 300
#define C_WHITE 1
#define C_GREY 2
#define C_GOLD 3
#define C_CYAN 4
#define C_RED 5
typedef struct { u8 n,laps,shape,surf,flags,riv,cls,pad; u8 gx[KG_MAX],gy[KG_MAX]; char name[16]; } KTrack;
static KTrack kt;
static u8 kpx[KP_MAX],kpy[KP_MAX],kmmx[KMM_N],kmmy[KMM_N];
static u16 kgate_pi[KG_MAX];
static int knp,kt_len,kmm_w,kmm_h,krev,kpaint_rev;
#define KGX(i) (kt.gx[krev ? (kt.n-(i))%kt.n : (i)])
#define KGY(i) (kt.gy[krev ? (kt.n-(i))%kt.n : (i)])
static int kwrap8(int d){ return ((d+128)&255)-128; }
static int isqrt(u32 v){ u32 r=0; while((r+1)*(r+1)<=v) r++; return (int)r; }
static struct { int pi,place,fin,nextg; } rc[1];
#define KS_RACE 1
static int ks_state;
static int kr_mode=1,nrc=1,khd=0,m7_strip;
static int outline_count,fast_count,player_x,player_y,ahead_count;
static int ahead_x[6],ahead_y[6],ahead_c[6];
static int gate_count,gate_x[4],gate_y[4],gate_c[4],route_expected;
static void ov_k(void){}
static void kmm_path(volatile u16 *pg,int x,int y,int n){
    (void)pg;(void)x;(void)y;fast_count=n;
}
static int sinx(int a){ (void)a;return 0; }
static int cosx(int a){ (void)a;return 16384; }
static void pix(volatile u16 *pg,int x,int y,int c){
    (void)pg;
    if(c==C_GREY || c==C_WHITE){ outline_count++; return; }
    if(ahead_count>=route_expected){
        if(gate_count>=4){fputs("too many gate pixels\n",stderr);exit(1);}
        gate_x[gate_count]=x;gate_y[gate_count]=y;gate_c[gate_count++]=c;return;
    }
    ahead_x[ahead_count]=x;ahead_y[ahead_count]=y;ahead_c[ahead_count++]=c;
}
static void rect(volatile u16 *pg,int x,int y,int w,int h,int c){
    (void)pg;
    if(w==3 && h==3 && c==C_GOLD){ player_x=x+1;player_y=y+1; }
}
""" + extract_function("static void kt_build(") + "\n" + map_function + "\n" + extract_function("static int kart_gate_missed(") + "\n" + extract_function("static void kart_minimap(") + r"""
static int check_map(int built_count,int pi,int fast){
    int ox=194+(40-kmm_w)/2,oy=20+(40-kmm_h)/2;
    m7_strip=fast?160:0;
    rc[0].pi=pi;rc[0].place=1;
    outline_count=fast_count=ahead_count=gate_count=route_expected=0;player_x=player_y=-1;
    if(built_count>12) for(int step=2;step<=12;step+=2){
        int i=(pi/2+step)%built_count,x=ox+kmmx[i],y=oy+kmmy[i];
        if(x>=194&&x<=234&&y>=21&&y<=61)route_expected++;
    }
    kart_minimap(NULL);
    if((fast?fast_count:outline_count)!=built_count){
        fprintf(stderr,"map outline: %d path points generated %d samples, drew %d (fast=%d)\n",
                knp,built_count,fast?fast_count:outline_count,fast);return 1;
    }
    if(player_x!=ox+kmmx[pi/2] || player_y!=oy+kmmy[pi/2]){
        fprintf(stderr,"map marker omitted generated point %d of %d\n",pi,knp);return 1;
    }
    int expected_count=0;
    if(built_count>12) for(int step=2;step<=12;step+=2){
        int i=(pi/2+step)%built_count;
        int x=ox+kmmx[i],y=oy+kmmy[i];
        if(x<194 || x>234 || y<21 || y>61) continue;
        if(expected_count>=ahead_count || ahead_x[expected_count]!=x ||
           ahead_y[expected_count]!=y || ahead_c[expected_count]!=(step<=6?C_GOLD:C_CYAN)){
            fprintf(stderr,"map route mismatch at path %d/%d step %d\n",pi,knp,step);return 1;
        }
        expected_count++;
    }
    if(expected_count!=ahead_count){ fputs("map route point count mismatch\n",stderr);return 1; }
    if(ks_state==KS_RACE && !rc[0].fin && rc[0].nextg<kt.n){
        int gp=kgate_pi[rc[0].nextg],i=gp/2,x=ox+kmmx[i],y=oy+kmmy[i];
        int past=(pi-gp+knp)%knp,c=past>8&&past<knp/2?C_RED:C_GOLD;
        int visible=i<built_count&&x>=195&&x<=233&&y>=22&&y<=60;
        if(gate_count!=(visible?4:0))return 1;
        if(visible){
            int dx[]={-1,1,0,0},dy[]={0,0,-1,1};
            for(int j=0;j<4;j++)if(gate_x[j]!=x+dx[j]||gate_y[j]!=y+dy[j]||gate_c[j]!=c)return 1;
        }
    }else if(gate_count)return 1;
    return 0;
}
int main(void){
    int tracks=0,odd_tracks=0,checks=0;
    for(int shape=0;shape<2;shape++) for(int width=1;width<=18;width++)
    for(int height=1;height<=18;height++) for(int reverse=0;reverse<2;reverse++){
        memset(&kt,0,sizeof kt);kt.n=shape?4:3;kt.laps=3;krev=reverse;
        kt.gx[0]=64;kt.gy[0]=64;
        kt.gx[1]=(u8)(64+width);kt.gy[1]=64;
        kt.gx[2]=(u8)(shape?64+width:64);kt.gy[2]=(u8)(64+height);
        kt.gx[3]=64;kt.gy[3]=(u8)(64+height);
        memset(kmmx,205,sizeof kmmx);memset(kmmy,205,sizeof kmmy);
        kt_build();
        int built_count=0;
        while(built_count<KMM_N && (kmmx[built_count]!=205 || kmmy[built_count]!=205)) built_count++;
        if(built_count!=(knp+1)/2){ fputs("unexpected builder sampling\n",stderr);return 1; }
        for(int i=built_count;i<KMM_N;i++) if(kmmx[i]!=205 || kmmy[i]!=205){
            fputs("non-contiguous builder samples\n",stderr);return 1;
        }
        tracks++;odd_tracks+=knp&1;
        for(int pi=0;pi<knp;pi++) for(int fast=0;fast<2;fast++){
            ks_state=0;
            if(check_map(built_count,pi,fast)) return 1;
            ks_state=KS_RACE;rc[0].nextg=pi%kt.n;
            if(check_map(built_count,pi,fast)) return 1;
            checks++;
        }
    }
    if(!odd_tracks){ fputs("missing odd-length circuit coverage\n",stderr);return 1; }
    printf("Kart minimap: %d real generated circuits (%d odd), %d outline/route/marker checks PASS\n",
           tracks,odd_tracks,checks);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-minimap-") as td:
    exe = str(Path(td) / "minimap")
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Wno-misleading-indentation",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    "-x", "c", "-", "-o", exe],
                   input=map_test, text=True, check=True)
    subprocess.run([exe], check=True)


