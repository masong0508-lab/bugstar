#!/usr/bin/env python3
"""Exercise actual touch editor panel and bounded key-state lookup."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/"pc/platform/pc_sdl.c").read_text()
def func(mark):
    a=s.index(mark);i=s.index("{",a)+1;d=1
    while d:
        d+=(s[i]=="{")-(s[i]=="}");i+=1
    return s[a:i]
a=s.index("enum { TA_SMALLER=1,")
b=s.index("static int ted_hits(",a)
assert "int64_t bestc=INT64_MAX;" in s
assert "1L<<60" not in func("static void ted_panel(")
assert "if(!g_touch||g_bg||dw_<=0||dh_<=0)return;" in func("static void touch_event(")
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define TC_N 12
typedef struct {int cx,cy,r;} TGeo;
static int dw_,dh_,ted_py=-1,ted_sel=-1,g_tsnap;
static void tg_ext(const TGeo *g,int i,int *w,int *h){*w=*h=g[i].r;}
static int tl_o(void){return dw_<dh_;}
"""
cases=r"""
static int inside(TedB a,TedP p){return a.w>0&&a.h>0&&a.x>=p.x&&a.y>=p.y&&a.x+a.w<=p.x+p.w&&a.y+a.h<=p.y+p.h;}
int main(void){
    unsigned checks=0;
    for(int w=1;w<2600;w+=31)for(int h=1;h<2000;h+=29){
        dw_=w;dh_=h;TGeo g[TC_N];
        for(int i=0;i<TC_N;i++)g[i]=(TGeo){w*(i%4+1)/5,h*(i/4+1)/4,1+(w<h?w:h)/12};
        for(int state=0;state<3;state++){
            ted_sel=state==0?-1:state==1?0:11;g_tsnap=state&1;
            TedP p;ted_panel(&p,g);
            assert(p.x>=0&&p.y>=0&&p.w>0&&p.h>0&&p.x+p.w<=w&&p.y+p.h<=h);
            assert(p.n==(w<108||h<90?1:12));
            unsigned acts=0;
            for(int i=0;i<p.n;i++){
                assert(inside(p.b[i],p));
                assert(!(acts&(1u<<p.b[i].act)));acts|=1u<<p.b[i].act;
                for(int j=0;j<i;j++)assert(p.b[i].x>=p.b[j].x+p.b[j].w||p.b[j].x>=p.b[i].x+p.b[i].w||p.b[i].y>=p.b[j].y+p.b[j].h||p.b[j].y>=p.b[i].y+p.b[i].h);
            }
            assert(acts&(1u<<TA_DONE));checks++;
        }
    }
    uint8_t keys[8]={0},cache[512]={0};keys[7]=1;cache[511]=1;
    for(int c=-100;c<1100;c++)assert(bounded_key_down(c,8,keys,cache)==(c==7||c==511));
    assert(!bounded_key_down(7,0,keys,0));
    assert(!bounded_key_down(511,8,keys,0));
    assert(bounded_key_down(511,0,0,cache));
    printf("Touch editor: %u display/selection cases, bounded key-state reads PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as td:
    d=Path(td);c=d/"editor.c";exe=d/"editor"
    c.write_text(prefix+s[a:b]+func("static int ted_hits(")+"\n"+func("static void ted_panel(")+"\n"+func("static int bounded_key_down(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined,address","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
