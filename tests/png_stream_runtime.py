#!/usr/bin/env python3
"""Exercise real PNG writer, bounded blocks, pixels, CRC and I/O errors."""
from pathlib import Path
import subprocess,tempfile,struct,zlib
s=(Path(__file__).resolve().parents[1]/"pc/platform/pc_sdl.c").read_text()
a=s.index("static uint32_t crc_t[256]")
# Stop at the actual writer's closing brace, not a neighboring subsystem.
start=s.index("static int png_write(const char *path, const uint32_t *xrgb,",a)
b=s.index("{",start)+1
depth=1
while depth:
    depth+=(s[b]=="{")-(s[b]=="}")
    b+=1
assert "case A_SHOT: menu_open=0; shot_pending=1;" in s
assert "if(draw||shot_pending)present_game(1);" in s
assert "uint8_t block[32768+5]" in s
assert "malloc(" not in s[a:b]
prefix=r"""
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
"""
cases=r"""
int main(void){
    enum { W=521,H=137 };
    static uint32_t pixels[W*H];
    for(unsigned i=0;i<W*H;i++)pixels[i]=(i*2654435761u)^0xaabbccdd;
    assert(plat_write_png("rgb.png",pixels,W,H));
    assert(plat_write_png_rgba("rgba.png",pixels,W,H));
    assert(plat_write_png("one.png",pixels,1,1));
    assert(!plat_write_png("bad.png",pixels,0,H));
    assert(!plat_write_png("bad.png",NULL,W,H));
    assert(!plat_write_png("/dev/full",pixels,W,H));
    assert(!plat_write_png("no-such-dir/test.png",pixels,W,H));
    return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
    d=Path(td);c=d/"test.c";exe=d/"test"
    c.write_text(prefix+s[a:b]+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],cwd=d,check=True)
    for name,w,h,bpp in [("rgb.png",521,137,3),("rgba.png",521,137,4),("one.png",1,1,3)]:
        data=(d/name).read_bytes()
        assert data[:8]==b"\x89PNG\r\n\x1a\n"
        pos=8;parts=[];ended=False
        while pos<len(data):
            n,typ=struct.unpack_from(">I4s",data,pos)
            payload=data[pos+8:pos+8+n]
            assert zlib.crc32(typ+payload)==struct.unpack_from(">I",data,pos+8+n)[0]
            pos+=n+12
            if typ==b"IHDR":assert struct.unpack(">IIBBBBB",payload)==(w,h,8,6 if bpp==4 else 2,0,0,0)
            if typ==b"IDAT":assert n<=32773;parts.append(payload)
            if typ==b"IEND":assert n==0;ended=True;break
        assert ended and pos==len(data)
        raw=zlib.decompress(b"".join(parts))
        expected=bytearray()
        for y in range(h):
            expected.append(0)
            for x in range(w):
                p=((y*w+x)*2654435761)^0xaabbccdd
                expected.extend(((p>>16)&255,(p>>8)&255,p&255))
                if bpp==4:expected.append((p>>24)&255)
        assert raw==expected
        if w>1:assert len(parts)>3
    print("PNG: 142755 exact RGB/RGBA pixels, streaming chunks, CRCs and failed writes PASS")
