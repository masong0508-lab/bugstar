#!/usr/bin/env python3
"""Run the real mobile menu rendering policy and scheduler on the host."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/"pc/platform/pc_sdl.c").read_text()
def func(mark):
    a=s.index(mark);i=s.index("{",a)+1;d=1
    while d:
        d+=(s[i]=="{")-(s[i]=="}");i+=1
    return s[a:i]
assert "ui_only=mobile_menu_only(with_overlay,g_touch,menu_open,ted_on,shot_pending);" in s
p=func("static void present_game(int with_overlay){")
assert p.index("if(!ui_only){") < p.index("tex_ensure(W,H,linear)") < p.index("pc_compose_native(pix,W,H,smooth)") < p.index("}else{")
assert "mobile_menu_due(SDL_GetTicks64(),&next_menu_draw)" in s
assert "rect_fill(0,0,w,h,0x080D20,255)" in s
cases=r"""
#include <assert.h>
#include <stdio.h>
int main(void){
  for(unsigned mask=0;mask<32;mask++){
    int overlay=!!(mask&1),touch=!!(mask&2),open=!!(mask&4),edit=!!(mask&8),capture=!!(mask&16);
    assert(mobile_menu_only(overlay,touch,open,edit,capture)==(mask==7));
  }
  uint64_t next=0;
  assert(mobile_menu_due(0,&next));assert(next==33);
  for(uint64_t i=1;i<33;i++)assert(!mobile_menu_due(i,&next));
  assert(mobile_menu_due(33,&next));assert(next==66);
  assert(mobile_menu_due(1000,&next));assert(next==1033);
  assert(!mobile_menu_due(1001,&next));
  next=0;
  unsigned draws=0;
  for(uint64_t i=0;i<10000;i++)draws+=mobile_menu_due(i,&next);
  assert(draws==304);
  printf("Mobile menu: 32 policy cases, %u draws in 10-second 1ms test PASS\n",draws);
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"menu.c";exe=Path(td)/"menu"
    c.write_text("#include <stdint.h>\n"+func("static int mobile_menu_only(")+"\n"+func("static int mobile_menu_due(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
