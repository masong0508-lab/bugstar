#!/usr/bin/env python3
"""Run the production auto-performance governor and budget policy on the host."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/"pc/platform/pc_sdl.c").read_text()
hw=(root/"pc/pc_hw.c").read_text()
a=s.index("typedef struct { int battery, pixels, divisor, simple; } MobileBudget;")
b=s.index("#ifdef PC_ANDROID",a)
assert "mobile_auto_budget(b,C.mobile_mode,g_low_ram,g_mem_pressure,C.pad_style,g_governor.level)" in s
assert s.index("mobile_governor_sample(&g_governor,governor_eligible") < s.index("SDL_RenderPresent(ren);",s.index("mobile_governor_sample(&g_governor,governor_eligible"))
assert "if(g_window || (W>240 && H>160)){ pc_nat_w=W; pc_nat_h=H; }" in hw
cases=r"""
#include <assert.h>
#include <stdio.h>
static void samples(MobileGovernor *g,int n,double ms){for(int i=0;i<n;i++)mobile_governor_sample(g,1,ms);}
int main(void){
    MobileGovernor g={0};
    samples(&g,120,8);assert(g.level==0&&g.count==0);
    samples(&g,29,15);samples(&g,31,8);assert(g.level==0);
    samples(&g,59,14);assert(g.level==0&&g.count==59);
    assert(mobile_governor_sample(&g,1,14)&&g.level==1);
    MobileBudget b=mobile_auto_budget(mobile_budget(0,0,0,0,0),0,0,0,0,g.level);
    assert(b.pixels==153600&&!b.battery&&b.divisor==1);
    samples(&g,30,20);mobile_governor_sample(&g,0,20);
    assert(g.level==1&&g.count==0);
    samples(&g,59,20);mobile_governor_sample(&g,1,2000);
    assert(g.level==1&&g.count==0);
    samples(&g,60,20);assert(g.level==2);
    samples(&g,600,2);assert(g.level==2);
    b=mobile_auto_budget(mobile_budget(0,0,0,0,0),0,0,0,0,g.level);
    assert(b.pixels==38400&&b.battery&&b.divisor==2);
    unsigned checks=0;
    for(int mode=0;mode<3;mode++)for(int low=0;low<2;low++)
    for(int pressure=0;pressure<2;pressure++)for(int style=0;style<3;style++)
    for(int level=0;level<3;level++){
        MobileBudget base=mobile_budget(mode,low,pressure,style,0);
        b=mobile_auto_budget(base,mode,low,pressure,style,level);
        assert(b.pixels<=base.pixels);
        if(mode||base.battery||!level)assert(b.pixels==base.pixels&&b.divisor==base.divisor&&b.simple==base.simple);
        if(pressure)assert(b.battery);
        checks++;
    }
    printf("Mobile governor: threshold, two downshifts, reset and %u profile cases PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"governor.c";exe=Path(td)/"governor"
    c.write_text("#include <stdint.h>\n"+s[a:b]+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
