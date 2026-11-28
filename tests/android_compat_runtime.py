#!/usr/bin/env python3
"""Host test actual relocation and display geometry; no Android emulator claims."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def extract(path,mark):
    s=(r/path).read_text(); i=s.index(mark); j=s.index("{",i)+1; d=1
    while d:
        d+=(s[j]=="{")-(s[j]=="}"); j+=1
    return s[i:j]
prefix=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>
#define PC_GBA_TOP 0x30000000u
static uint32_t pc_gba_top;
static uint32_t live[8]; static size_t lengths[8]; static int n, mode;
static int map_at(uint32_t a,size_t s){
    // First candidate succeeds for its first three regions and then collides.
    if(mode==2 || (mode==1 && a==PC_GBA_TOP+0x05000000u))return 0;
    for(int i=0;i<n;i++)assert(live[i]!=a);
    assert(n<8);live[n]=a;lengths[n++]=s;return 1;
}
static int munmap(void *a,size_t s){assert(n>0&&live[n-1]==(uintptr_t)a&&lengths[n-1]==s);n--;return 0;}
static int dw_,dh_,g_touch,g_thide;
enum {SM_CRISP,SM_SMOOTH};
static struct {int scale_mode;} C;
typedef struct {int x,y,w,h;} SDL_Rect;
static int cap=153600;
static int cap_pixels(void){return cap;}
"""
cases=r"""
int main(void){
    mode=0;assert(map_android_regions()&&n==8&&pc_gba_top==PC_GBA_TOP);n=0;
    mode=1;assert(map_android_regions()&&n==8&&pc_gba_top==0x20000000u);n=0;
    mode=2;assert(!map_android_regions()&&n==0);
    int sizes[][2]={{1,1},{160,100},{240,160},{320,240},{320,568},{360,640},{720,1600},{1080,2400},{1200,1201},{1440,3200},{3840,2160}};
    for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++)for(int touch=0;touch<2;touch++)for(int sc=0;sc<2;sc++)for(int hide=0;hide<2;hide++){
        dw_=sizes[i][0];dh_=sizes[i][1];g_touch=touch;g_thide=hide;C.scale_mode=sc;
        int W,H,linear,smooth;SDL_Rect dst;frame_geometry(&W,&H,&dst,&linear,&smooth);
        assert(W>=240&&H>=160&&(long)W*H<=cap);
        assert(dst.w>0&&dst.h>0&&dst.x>=0&&dst.y>=0&&dst.x+dst.w<=dw_&&dst.y+dst.h<=dh_);
        if(touch&&!hide&&dw_==720&&dh_==1600)assert(dst.y+dst.h<=dh_-6*(dw_/7));
    }
    puts("Android compatibility: transactional relocation and 88 small/tall/wide layouts PASS");
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"compat.c"; exe=Path(td)/"compat"
    c.write_text(prefix+extract("pc/pc_hw.c","static int map_android_regions(")+"\n"+extract("pc/platform/pc_sdl.c","static void frame_geometry(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-lm","-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
