#!/usr/bin/env python3
"""Exercise production touch-menu geometry and hit testing, including tiny windows."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/"pc/platform/pc_sdl.c").read_text()
a=s.index("typedef struct { int gs, margin, header, footer, row, rows; } TouchMenuGeo;")
b=s.index("static void menu_key(",a)
assert "if(g_touch){ touch_menu_draw(); return; }" in s
assert "if(ted_on||menu_open||!tg_on()) return;" in s
assert "if(fr<0||e->type!=SDL_FINGERDOWN)" in s
assert "touch_menu_input((int)(e->tfinger.x*dw_)" in s
cases=r"""
#include <assert.h>
#include <stdio.h>
int main(void){
  int sz[][2]={{160,100},{240,160},{320,240},{240,320},{320,568},{360,640},{720,1600},{1080,2400},{2400,1080},{1200,1201},{3840,2160}};
  unsigned checks=0;
  for(unsigned i=0;i<sizeof sz/sizeof sz[0];i++)for(int z=0;z<3;z++){
    int w=sz[i][0],h=sz[i][1];TouchMenuGeo g=touch_menu_geometry(w,h,z);
    assert(g.gs>=1&&g.rows>=1&&g.row>0);
    assert(g.header+g.rows*g.row+g.footer<=h);
    for(int total=1;total<=24;total++)for(int first=0;first<total;first+=g.rows){
      assert(touch_menu_hit(g,w,h,first,total,w/2,h-1)==TM_BACK);
      assert(touch_menu_hit(g,w,h,first,total,0,h-1)==TM_PREV);
      assert(touch_menu_hit(g,w,h,first,total,w-1,h-1)==TM_NEXT);
      assert(touch_menu_hit(g,w,h,first,total,w/2,0)==TM_NONE);
      for(int j=0;j<g.rows;j++){
        int y=g.header+j*g.row+g.row/2;
        int want=first+j<total?first+j:TM_NONE;
        assert(touch_menu_hit(g,w,h,first,total,w/2,y)==want);
        checks++;
      }
    }
  }
  printf("Touch menu: %u row-hit checks, 33 display/text profiles PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"menu.c"; exe=Path(td)/"menu"
    c.write_text(s[a:b]+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
