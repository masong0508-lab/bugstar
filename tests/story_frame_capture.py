#!/usr/bin/env python3
"""Compile the *actual* chapter-slate C routine under a tiny framebuffer stub.

This is an inexpensive layout/cutscene-control regression, not an emulator or
a claim of pixel-identical GBA output: sprite OAM and font glyphs are mocked.
Produces eight reproducible 240x160 PNG previews and a contact sheet so a
human can spot design regressions before full native emulator captures exist.
"""
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "out" / "story-layouts"
out.mkdir(parents=True, exist_ok=True)
ui = (root / "ui.inc").read_text()
start = ui.index("static int bolt_chapter_open(")
open_brace = ui.index("{", start)
depth = 0
end = None
for i in range(open_brace, len(ui)):
    if ui[i] == "{":
        depth += 1
    elif ui[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end is not None
chapter = ui[start:end]
assert "int k=beat==10?7:beat" in chapter
assert "return skip;" in chapter
attr0 = re.findall(r"shadow_oam\[0\]\.a0\s*=\s*([^;]+);", chapter)
assert len(attr0) >= 2 and all("(1<<14)" not in a for a in attr0)

c_prefix = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
enum {C_DGREY=35,C_WHITE=245,C_GREEN=104,C_GREY=108,C_GOLD=210,
      C_RED=177,C_CYAN=166,T_BOLT=19,K_A=1,K_START=2};
enum { WIDTH=240, HEIGHT=160 };
static u8 framebuffer[WIDTH*HEIGHT];
static int bad_pixel, pressed;
static struct { unsigned a0,a1,a2; } shadow_oam[128];
#define VISPG ((volatile u16 *)framebuffer)
#define HIT(k) (pressed==(k))
static int shd_begin(void){ return 0; }
static void shd_end(int sh){ (void)sh; }
static void wait_release(void){}
static void read_keys(void){}
static void vsync(void){}
static void oam_hide_all(void){ shadow_oam[0].a0=0; }
static void oam_flush(void){}
static void set_affine(int a,int b,int c,int d,int e){
    (void)a;(void)b;(void)c;(void)d;(void)e;
}
static int textlen(const char *s){ return (int)strlen(s); }
static void pix(volatile u16 *pg,int x,int y,int col){
    if(x<0 || x>=WIDTH || y<0 || y>=HEIGHT){ bad_pixel++; return; }
    ((volatile u8 *)pg)[y*WIDTH+x]=(u8)col;
}
static void rect(volatile u16 *pg,int x,int y,int w,int h,int col){
    for(int yy=y;yy<y+h;yy++) for(int xx=x;xx<x+w;xx++)
        pix(pg,xx,yy,col);
}
// Stand-in narrow glyphs (not production font). These show text footprints
// without importing hardware font tables or allocating a framebuffer in-game.
static void glyphs(volatile u16 *pg,int x,int y,const char *s,int col,int scale){
    int advance=4*scale;
    for(int j=0;s[j];j++){
        if(s[j]==' ') continue;
        int seed=(unsigned char)s[j];
        for(int yy=0;yy<5*scale;yy++) for(int xx=0;xx<3*scale;xx++)
            if(((seed+xx/scale*3+yy/scale*7)&3)!=0)
                pix(pg,x+j*advance+xx,y+yy,col);
    }
}
static void text(volatile u16 *pg,int x,int y,const char *s,int col,int size){
    glyphs(pg,x,y,s,col,size);
}
static void stext(volatile u16 *pg,int x,int y,const char *s,int col){
    glyphs(pg,x,y,s,col,1);
}
static uint64_t picture_hash(void){
    uint64_t h=1469598103934665603ULL;
    for(int i=0;i<WIDTH*HEIGHT;i++){h^=framebuffer[i];h*=1099511628211ULL;}
    return h;
}
"""
c_suffix = r"""
int main(int argc,char **argv){
    if(argc!=2) return 2;
    uint64_t hashes[8]={0};
    for(int k=0;k<8;k++){
        memset(framebuffer,0,sizeof framebuffer);
        memset(shadow_oam,0,sizeof shadow_oam);
        bad_pixel=0; pressed=K_A;
        int skipped=bolt_chapter_open(VISPG,k);
        if(skipped!=0 || bad_pixel!=0){
            fprintf(stderr,"chapter %d: skip=%d out-of-screen=%d\n",k,skipped,bad_pixel);
            return 3;
        }
        hashes[k]=picture_hash();
        char file[512]; snprintf(file,sizeof file,"%s/chapter_%d.ppm",argv[1],k);
        FILE *f=fopen(file,"wb"); if(!f) return 4;
        fprintf(f,"P6\n240 160\n255\n");
        for(int i=0;i<WIDTH*HEIGHT;i++){
            unsigned v=framebuffer[i];
            u8 rgb[3]={(u8)v,(u8)(v==35?52:v),(u8)(v==166?240:v)};
            if(fwrite(rgb,1,3,f)!=3){fclose(f);return 5;}
        }
        fclose(f);
        for(int n=0;n<k;n++)
            if(hashes[n]==hashes[k]){
                fprintf(stderr,"chapters %d and %d render identically\n",n,k);
                return 6;
            }
    }
    memset(framebuffer,0,sizeof framebuffer);
    bad_pixel=0; pressed=K_START;
    if(bolt_chapter_open(VISPG,0)!=1 || bad_pixel!=0) return 7;
    puts("Story layout: 8 unique 240x160 backdrops, no clipped draws, A and START controls PASS");
    return 0;
}
"""


def png_write(path, w, h, rgb):
    assert len(rgb) == w*h*3
    def chunk(name, raw):
        return struct.pack(">I", len(raw)) + name + raw + struct.pack(">I", zlib.crc32(name+raw))
    scanlines = b"".join(b"\x00" + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    data = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w,h,8,2,0,0,0)) +
            chunk(b"IDAT", zlib.compress(scanlines, 9)) +
            chunk(b"IEND", b""))
    path.write_bytes(data)


with tempfile.TemporaryDirectory(prefix="bugstar-scenes-") as tmp:
    src = Path(tmp) / "chapter_capture.c"
    exe = Path(tmp) / "chapter_capture"
    src.write_text(c_prefix + "\n" + chapter + "\n" + c_suffix)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-fno-sanitize-recover=all",
                    str(src),"-o",str(exe)], check=True)
    subprocess.run([str(exe), str(out.resolve())], check=True)

frames = []
for i in range(8):
    data = (out/f"chapter_{i}.ppm").read_bytes()
    header = b"P6\n240 160\n255\n"
    assert data.startswith(header)
    rgb = data[len(header):]
    assert len(rgb)==240*160*3
    frames.append(rgb)
    png_write(out/f"chapter_{i}.png",240,160,rgb)

# Arrange all eight original-resolution layout captures into one overview.
W,H=960,320
sheet=bytearray(W*H*3)
for i,frame in enumerate(frames):
    x0=(i%4)*240
    y0=(i//4)*160
    for row in range(160):
        pos=((y0+row)*W+x0)*3
        sheet[pos:pos+240*3]=frame[row*240*3:(row+1)*240*3]
png_write(out/"story_chapter_contact_sheet.png",W,H,sheet)
(out/"README.txt").write_text(
    "These are deterministic layout captures from the actual GBA cutscene C\n"
    "function, compiled with a mocked font, palette, and sprite OAM. They are\n"
    "NOT emulator screenshots or pixel-accurate representations of Bugstar.\n"
    "They check chapter distinctness, frame bounds, and A/START controls.\n"
    "Native gameplay screenshots and visual inspection are still required.\n"
)
print("Wrote 8 layout captures and contact sheet:", out)
