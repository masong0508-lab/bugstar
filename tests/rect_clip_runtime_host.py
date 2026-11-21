#!/usr/bin/env python3
"""ASan/UBSan regression for Bugstar's actual shared 240x160 rectangle painter.

Extracts the shipped C implementation, runs edge-case and randomized draws,
and compares *every* resulting pixel against a clipped reference model. No
emulator or target hardware is needed; CI still builds the real GBA ROM.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "main.c").read_text()
match = re.search(r"static\s+void\s+rect\s*\([^)]*\)\s*\{", source)
if not match:
    raise SystemExit("Cannot find shared rect() in main.c")
start = match.start()
opening = source.index("{", match.start())
depth = 0
closing = None
for pos in range(opening, len(source)):
    if source[pos] == "{":
        depth += 1
    elif source[pos] == "}":
        depth -= 1
        if depth == 0:
            closing = pos + 1
            break
if closing is None:
    raise SystemExit("Incomplete shared rectangle painter")
rect = source[start:closing]

harness = r'''
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
typedef uint16_t u16;
typedef uint8_t u8;
#define GUARD 64
#define SW 240
#define SH 160
static u16 framebuffer[GUARD + SW*SH/2 + GUARD];
static void pix(volatile u16 *pg, int x, int y, int c){
    volatile u16 *p = pg + ((y*240+x)>>1);
    if(x&1) *p=(*p&0x00ff)|(c<<8);
    else *p=(*p&0xff00)|c;
}
''' + rect + r'''
static int count=0;
static int check(int x,int y,int w,int h,int color){
  for(int i=0;i<(int)(sizeof framebuffer/sizeof framebuffer[0]);i++)
    framebuffer[i]=0x5a5a;
  volatile u16 *pg=framebuffer+GUARD;
  rect(pg,x,y,w,h,color);
  for(int i=0;i<GUARD;i++)
    if(framebuffer[i]!=0x5a5a || framebuffer[GUARD+SW*SH/2+i]!=0x5a5a){
      fprintf(stderr,"VRAM guard overwritten\n"); return 1;
    }
  for(int yy=0;yy<SH;yy++)for(int xx=0;xx<SW;xx++){
    int inside=(int64_t)xx>=x && (int64_t)xx<(int64_t)x+w
        && (int64_t)yy>=y && (int64_t)yy<(int64_t)y+h;
    unsigned want=inside?((unsigned)color&255u):0x5a;
    unsigned got=((unsigned char *)pg)[yy*SW+xx];
    if(got!=want){
      fprintf(stderr,"case %d: rect(%d,%d,%d,%d,%d), pixel %d,%d got %u want %u\n",
          count,x,y,w,h,color,xx,yy,got,want);
      return 1;
    }
  }
  count++; return 0;
}
int main(void){
  const int cases[][5]={
    {0,0,240,160,255},{-10,-10,25,25,128},{238,158,10,10,200},
    {-240,0,240,160,11},{0,-160,240,160,11},
    {INT_MIN,0,INT_MAX,10,3},{0,INT_MIN,10,INT_MAX,3},
    {0,0,INT_MAX,INT_MAX,-1},{0,0,240,160,-1},
    {1,1,1,1,7},{2,1,1,2,12},{240,0,7,7,15},
    {0,160,7,7,15},{0,0,-1,5,6},{0,0,5,-1,6},
    {INT_MAX,INT_MAX,INT_MAX,INT_MAX,4},
    {-5,0,15,159,300},{0,-1,240,2,128},{239,159,1,1,0}
  };
  for(unsigned i=0;i<sizeof cases/sizeof cases[0];i++)
    if(check(cases[i][0],cases[i][1],cases[i][2],cases[i][3],cases[i][4])) return 1;
  uint32_t state=0x88c40453u;
  for(int i=0;i<2800;i++){
    state^=state<<13;state^=state>>17;state^=state<<5;
    int x=(int)(state%640u)-320;
    state^=state<<13;state^=state>>17;state^=state<<5;
    int y=(int)(state%440u)-220;
    state^=state<<13;state^=state>>17;state^=state<<5;
    int w=(int)(state%540u)-80;
    state^=state<<13;state^=state>>17;state^=state<<5;
    int h=(int)(state%390u)-80;
    state^=state<<13;state^=state>>17;state^=state<<5;
    int c=(int)(state&1023u)-256;
    if(check(x,y,w,h,c)) return 1;
  }
  printf("framebuffer rectangle clipping: %d ASan/UBSan cases PASS\n",count);
  return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="bugstar-rect-") as td:
    exe = Path(td) / "rect-test"
    subprocess.run(
        ["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
         "-x", "c", "-", "-o", str(exe)],
        input=harness, text=True, check=True,
    )
    subprocess.run([str(exe)], check=True)
