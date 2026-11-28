#!/usr/bin/env python3
"""Execute production touch latch, finger event, and sampling code on host."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/"pc/platform/pc_sdl.c").read_text()
def func(mark):
    a=s.index(mark); i=s.index("{",a)+1; d=1
    while d:
        d+=(s[i]=="{")-(s[i]=="}");i+=1
    return s[a:i]
a=s.index("static uint16_t touch_pending;")
b=s.index("static void touch_release_all(void);",a)
prefix=r"""
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int SDL_FingerID;
typedef struct { SDL_FingerID id; float x,y;int on;} TFing;
static TFing tfing[10];
static int g_touch=1,g_thide,ted_on,menu_open,g_bg,dw_=100,dh_=100,ted_drag,ted_pinch;
static int buzz,editor,menu_taps;
enum { SDL_FINGERDOWN=1,SDL_FINGERUP,SDL_FINGERMOTION };
typedef struct {int type;struct {SDL_FingerID fingerId;float x,y;} tfinger;} SDL_Event;
static uint16_t t_hit(int x,int y){(void)y;return x<10?1:x<20?2:x<30?0x10:x<40?0x20:x<50?0x40:x<60?0x80:x<70?0x100:x<80?0x200:x<90?4:0x8000;}
static uint32_t tg_state(uint16_t *d){uint32_t el=0;*d=0;for(int i=0;i<10;i++)if(tfing[i].on){uint16_t m=t_hit((int)(tfing[i].x*dw_),0);*d|=m&0xf0;el|=m&~0xf0;}return el;}
static uint16_t tg_keys(uint32_t el,uint16_t d){return el|d;}
static int tg_on(void){return g_touch&&!g_thide;}
static void t_buzz(void){buzz++;}
static void touch_release_all(void);
static void open_menu(void){touch_release_all();menu_open=1;}
static void ted_touch(const SDL_Event *e){(void)e;editor++;}
static void touch_menu_input(int x,int y){(void)x;(void)y;menu_taps++;}
"""
cases=r"""
static void event(int type,int id,float x){SDL_Event e={type,{id,x,0.5f}};touch_event(&e);}
static void reset(void){touch_release_all();g_touch=1;g_thide=ted_on=menu_open=g_bg=0;dw_=dh_=100;}
int main(void){
    reset();event(SDL_FINGERDOWN,1,.05f);event(SDL_FINGERUP,1,.05f);
    assert(touch_keys()==1);assert(touch_keys()==0);
    event(SDL_FINGERDOWN,1,.05f);assert(touch_keys()==1);assert(touch_keys()==1);
    event(SDL_FINGERDOWN,2,.65f);event(SDL_FINGERUP,2,.65f);assert(touch_keys()==0x101);
    assert(touch_keys()==1);event(SDL_FINGERUP,1,.05f);assert(touch_keys()==0);
    reset();event(SDL_FINGERDOWN,1,.25f);event(SDL_FINGERDOWN,2,.35f);assert(touch_keys()==0);
    event(SDL_FINGERUP,2,.35f);assert(touch_keys()==0x10);
    reset();event(SDL_FINGERDOWN,1,.05f);event(SDL_FINGERUP,1,.05f);touch_release_all();assert(!touch_keys());
    event(SDL_FINGERMOTION,1,.05f);assert(!touch_keys());
    reset();event(SDL_FINGERDOWN,1,.05f);event(SDL_FINGERDOWN,2,.95f);assert(menu_open&&!touch_keys());
    event(SDL_FINGERUP,2,.95f);assert(menu_taps==0);event(SDL_FINGERDOWN,3,.15f);assert(menu_taps==1);
    reset();event(SDL_FINGERDOWN,1,.05f);g_bg=1;assert(!touch_keys());
    reset();event(SDL_FINGERDOWN,1,.05f);g_thide=1;assert(!touch_keys());
    reset();ted_on=1;event(SDL_FINGERDOWN,1,.05f);assert(editor==1&&!touch_keys());
    for(unsigned m=0;m<65536;m++){touch_pending=0;touch_latch((uint16_t)m);uint16_t x=touch_consume(0);assert(!(x&~0x3ff));assert((x&0x30)!=0x30&&(x&0xc0)!=0xc0);assert(!touch_consume(0));}
    puts("Touch input: short taps, multi-touch, held keys, cancellation, menu and 65536 masks PASS");
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"touch.c";exe=Path(td)/"touch"
    c.write_text(prefix+s[a:b]+func("static void touch_release_all(void){")+"\n"+func("static uint16_t touch_keys(")+"\n"+func("static void touch_event(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
