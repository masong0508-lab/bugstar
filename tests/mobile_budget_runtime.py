#!/usr/bin/env python3
"""Compile production mobile budget and texture-size helpers under UBSan."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/"pc/platform/pc_sdl.c").read_text()
p=(r/"pc/platform/pc_tpad.inc").read_text()
def extract(src,mark):
    a=src.index(mark);i=src.index("{",a)+1;d=1
    while d:
        d+=(src[i]=="{")-(src[i]=="}");i+=1
    return src[a:i]
a=s.index("typedef struct { int battery, pixels, divisor, simple; } MobileBudget;")
b=s.index("#ifdef PC_ANDROID",a)
assert "if(!g_fast && current_budget().divisor==2 && (fr&1)" in s
assert "if(draw||shot_pending)present_game(1);" in s
assert "if(tg_failed)simple_pad_draw(g,el,dirs,a);" in s
assert "while(tg_bytes+bytes>6u*1024u*1024u)" in p
cases=r"""
#include <assert.h>
#include <stdio.h>
int main(void){
    unsigned n=0;
    for(int mode=0;mode<3;mode++)for(int low=0;low<2;low++)for(int pressure=0;pressure<2;pressure++)
    for(int style=0;style<3;style++)for(int mp=0;mp<=40;mp++){
        MobileBudget b=mobile_budget(mode,low,pressure,style,mp);
        assert(b.pixels>=38400&&b.pixels<=650000);
        assert(!low||b.pixels<=153600);
        assert(b.battery==(pressure||mode==1||(mode==0&&low)));
        assert(b.divisor==(b.battery?2:1));
        assert(!b.battery||b.pixels==38400);
        assert(b.simple==(pressure||style==1||(style==0&&b.battery)));
        n++;
    }
    unsigned dims=0;
    for(int w=1;w<=16384;w+=137)for(int h=1;h<=16384;h+=139){
        int x=w,y=h,e=h/2;tg_limit_dims(&x,&y,&e);
        assert(x>=1&&y>=1&&x<=256&&y<=256);
        assert(x<=w&&y<=h);
        assert((unsigned)x*y*4<=256u*256u*4);
        if(w<=256&&h<=256)assert(x==w&&y==h);
        dims++;
    }
    printf("Mobile policy: %u budget cases, %u bounded texture sizes PASS\n",n,dims);
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"mobile.c";exe=Path(td)/"mobile"
    c.write_text("#include <stdint.h>\n"+s[a:b]+extract(p,"static void tg_limit_dims(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
