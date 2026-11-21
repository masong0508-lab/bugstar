#!/usr/bin/env python3
"""Pixel parity and ASan/UBSan safety for the *actual* kart HUD DMA rectangles.

The GBA DMA3 register writes are substituted with a 16-bit host memory copy,
but both clipping and address/pair calculations come from the real kart.inc.
The ordinary rectangle fallback is extracted from main.c without rewriting.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
kart=(root/"kart.inc").read_text()
main=(root/"main.c").read_text()

def extract(src,name):
    m=re.search(r"static (?:inline )?(?:u32|int|void) "+name+r"\s*\([^)]*\)\s*\{",src)
    assert m, f"Missing live C helper {name}"
    depth=0
    for i in range(src.index("{",m.start()),len(src)):
        if src[i]=="{":
            depth+=1
        elif src[i]=="}":
            depth-=1
            if depth==0:
                return src[m.start():i+1]
    raise AssertionError(f"Unbalanced {name}")

clip=extract(kart,"krect_clip")
word=extract(kart,"krect_dma_word")
kart_rect=extract(kart,"krect")
normal_rect=extract(main,"rect")
assert "if(!krect_clip(&x,&y,&w,&h)) return;" in kart_rect
assert "if(w<16){ rect(pg,x,y,w,h,c); return; }" in kart_rect
assert "volatile u32 v=krect_dma_word(c);" in kart_rect
assert "*(volatile u32*)0x040000DC=(u32)pairs|(2<<23)|(1u<<31);" in kart_rect

dma=r"""        *(volatile u32*)0x040000D4=(u32)&v; *(volatile u32*)0x040000D8=(u32)p;
        *(volatile u32*)0x040000DC=(u32)pairs|(2<<23)|(1u<<31);"""
assert kart_rect.count(dma)==1, "DMA3 source no longer matches host substitution"
kart_rect=kart_rect.replace(dma,"        host_dma_pairs(p, pairs, v);")

harness=r"""
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define SW 240
#define SH 160
#define GUARD 64
static u16 memory[GUARD + SW*SH/2 + GUARD];
static void pix(volatile u16 *pg,int x,int y,int c){
    if(x<0 || x>=SW || y<0 || y>=SH){
        fprintf(stderr,"Unexpected pix outside framebuffer (%d,%d)\n",x,y);
        __builtin_trap();
    }
    volatile u16 *p=pg+((y*SW+x)>>1);
    if(x&1) *p=(u16)((*p&0x00FF)|((u16)(u8)c<<8));
    else *p=(u16)((*p&0xFF00)|(u8)c);
}
static void host_dma_pairs(volatile u16 *dest,int pairs,volatile u32 palette){
    for(int i=0;i<pairs;i++) {
        dest[i]=(u16)palette;
    }
}
""" + normal_rect + "\n" + clip + "\n" + word + "\n" + kart_rect + r"""
static uint32_t seed=0x7a9c3e45u;
static uint32_t rand32(void){
    seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;
}
static int check(int x,int y,int w,int h,int col,int case_id){
    for(size_t i=0;i<sizeof memory/sizeof memory[0];i++) memory[i]=0x5a5a;
    volatile u16 *pg=memory+GUARD;
    krect(pg,x,y,w,h,col);
    for(int i=0;i<GUARD;i++) if(memory[i]!=0x5a5a ||
                              memory[GUARD+SW*SH/2+i]!=0x5a5a){
        fprintf(stderr,"guard smashed: case %d\n",case_id);return 1;
    }
    for(int yy=0;yy<SH;yy++)for(int xx=0;xx<SW;xx++){
        int inside=(int64_t)xx>=x && (int64_t)xx<(int64_t)x+w
                && (int64_t)yy>=y && (int64_t)yy<(int64_t)y+h
                && w>0 && h>0;
        int expected=inside?((unsigned)col&255u):0x5a;
        int got=((volatile u8 *)pg)[yy*SW+xx];
        if(got!=expected){
            fprintf(stderr,"case %d: rect %d %d %d %d c%d, pixel %d,%d got %d wanted %d\n",
                    case_id,x,y,w,h,col,xx,yy,got,expected);
            return 1;
        }
    }
    return 0;
}
int main(void){
    const int edges[][5]={
        {0,0,240,160,255},{-20,-20,80,80,128},
        {-15,0,31,20,222},{-16,0,32,40,56},
        {-1,0,32,8,3},{1,-1,20,3,7},
        {239,159,20,40,0},{230,155,12,20,200},
        {232,0,8,160,230},{233,33,40,4,21},
        {0,159,240,50,66},{1,0,240,160,9},
        {-240,0,240,160,45},{0,-160,240,160,36},
        {INT_MIN,0,INT_MAX,8,34},{0,INT_MIN,20,INT_MAX,34},
        {0,0,INT_MAX,INT_MAX,-1},{INT_MAX,INT_MAX,4,4,128},
        {0,0,0,100,99},{0,0,-10,100,99},
        {0,0,100,0,99},{0,0,100,-10,99},
        {1,1,1,1,3},{2,1,1,2,3}
    };
    int count=0;
    for(size_t i=0;i<sizeof(edges)/sizeof(edges[0]);i++)
        if(check(edges[i][0],edges[i][1],edges[i][2],edges[i][3],edges[i][4],count++)) return 1;
    for(int i=0;i<2800;i++){
        int x=(int)(rand32()%640u)-320, y=(int)(rand32()%440u)-220;
        int w=(int)(rand32()%540u)-80, h=(int)(rand32()%400u)-80;
        int c=(int)(rand32()%1024u)-128;
        if(check(x,y,w,h,c,count++)) return 1;
    }
    printf("Kart HUD DMA clipping: %d pixel-parity ASan/UBSan cases PASS\n",count);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-hud-dma-") as td:
    exe=Path(td)/"dma"
    subprocess.run(
        ["cc","-std=c11","-O1","-Wall","-Wextra","-Werror",
         "-fsanitize=address,undefined","-fno-sanitize-recover=all",
         "-x","c","-","-o",str(exe)],
        input=harness,text=True,check=True,
    )
    subprocess.run([str(exe)],check=True)
