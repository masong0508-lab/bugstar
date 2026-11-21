#!/usr/bin/env python3
"""Exercise the real Bugstar list_pick_ C function with 50-item menus.

The in-game renderer is stubbed, but selection, window paging, callbacks,
and key transitions are compiled from ui.inc itself with UBSan. This is not
a screenshot or a handheld-controller playthrough.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "ui.inc").read_text()
kart = (root / "kart2.inc").read_text()
assert "NKREC,ksv_row" in kart, "replacement menu must expose all 50 slots"
assert "kmy_n,kmy_row" in kart, "saved-track menu must expose every entry"
signature = "static int list_pick_(volatile u16 *pg, const char *title, int n, void (*row)(int,char*,int*), int *sel, const char *help)"
start = source.find(signature + "{")
assert start >= 0, "actual list_pick_ function missing"
opening = source.index("{", start)
depth = 0
end = -1
for i in range(opening, len(source)):
    if source[i] == "{":
        depth += 1
    elif source[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end >= 0, "unbalanced source list picker"
actual = source[start:end]
assert "for(int i=first;i<first+shown;i++)" in actual, "list rendering no longer pages"

stub = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint16_t u16;
enum { C_GREY=1, C_WHITE=2, C_GOLD=3, C_CYAN=4, C_GREEN=5,
       U_SEL=6, U_GLGOLD=7, U_RAISE=8, U_SOFT=9 };
enum { K_UP=1, K_DOWN=2, K_A=4, K_B=8 };
static int gold=20, keys, keys_old, events[256], event_n, event_at;
static char menu_msg[80];
static void (*list_desc)(int,char*,int*)=0;
static int seen[128], bad_index, rows_this_draw, max_drawn;
static int textlen(const char *s){ return (int)strlen(s); }
static void ncat(char *s,int n){ char t[24]; snprintf(t,sizeof t,"%d",n); strcat(s,t); }
static void scat(char *s,const char *t){ strcat(s,t); }
static void wait_release(void){ keys=keys_old=0; }
static void vsync(void){}
static void read_keys(void){
    keys_old=keys;
    keys=event_at<event_n ? events[event_at++] : K_B;
}
#define HIT(key) ((keys & (key)) && !(keys_old & (key)))
static void panel(volatile u16 *p,int x,int y,int w,int h,const char *t){
    (void)p;(void)x;(void)y;(void)w;(void)h;(void)t;
    rows_this_draw=0;
}
static void rect(volatile u16 *p,int x,int y,int w,int h,int c){
    (void)p;(void)x;(void)y;(void)w;(void)h;(void)c;
}
static void text(volatile u16 *p,int x,int y,const char *s,int c,int scale){
    (void)p;(void)x;(void)y;(void)s;(void)c;(void)scale;
}
static void stext(volatile u16 *p,int x,int y,const char *s,int c){
    (void)p;(void)x;(void)y;(void)s;(void)c;
}
static void ui_sel(volatile u16 *p,int x,int y,int w,int h,int c){
    (void)p;(void)x;(void)y;(void)w;(void)h;(void)c;
}
static void ui_hints(volatile u16 *p,int x,int y,const char *s,int c,int scale){
    (void)p;(void)x;(void)y;(void)s;(void)c;(void)scale;
}
static void row(int i,char *b,int *c){
    if(i<0 || i>=128){ bad_index=1; return; }
    seen[i]++;
    rows_this_draw++;
    if(rows_this_draw>max_drawn) max_drawn=rows_this_draw;
    snprintf(b,64,"TRACK %d",i+1);
    *c=C_GREY;
}
"""

tests = r"""
#define CHECK(c) do { if(!(c)){ fprintf(stderr,"list picker failed at %d: %s\n",__LINE__,#c); return 1; } } while(0)
static void clear_events(void){
    event_at=event_n=0;
    keys=keys_old=0;
    memset(seen,0,sizeof seen);
    bad_index=max_drawn=rows_this_draw=0;
}
static void push(int key){ if(event_n<256) events[event_n++]=key; }
int main(void){
    int selected=0, chosen;
    // The final slot must be selectable and visible, without drawing 50
    // rows or covering the help area during an individual redraw.
    clear_events();
    for(int i=0;i<49;i++){ push(K_DOWN); push(0); }
    push(K_A);
    chosen=list_pick_(0,"YOUR TRACKS",50,row,&selected,"A: PICK");
    CHECK(chosen==49 && selected==49 && seen[49]>0);
    CHECK(!bad_index && max_drawn<=10 && max_drawn==10);

    // From the first row, UP should wrap to the last row and scroll to it.
    selected=0; clear_events(); push(K_UP); push(0); push(K_A);
    chosen=list_pick_(0,"REPLACE A TRACK",50,row,&selected,"A: REPLACE");
    CHECK(chosen==49 && seen[49]>0 && max_drawn<=10);

    // A stale index from an older menu cannot read past the collection.
    selected=100; clear_events(); push(K_A);
    chosen=list_pick_(0,"LIST",3,row,&selected,"A: PICK");
    CHECK(chosen==0 && selected==0 && max_drawn==3);

    selected=-1; clear_events(); push(K_A);
    chosen=list_pick_(0,"LIST",3,row,&selected,"A: PICK");
    CHECK(chosen==0 && selected==0);

    // Empty lists must still support Back without accessing row(-1).
    selected=8; clear_events(); push(K_B);
    chosen=list_pick_(0,"EMPTY",0,row,&selected,"B: BACK");
    CHECK(chosen==-1 && max_drawn==0 && !bad_index);
    puts("Bugstar actual list picker: 50 tracks, wrap, bounds and 10-row viewport PASS");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-menu-") as d:
    source_file = Path(d) / "list_test.c"
    exe = Path(d) / "list_test"
    source_file.write_text(stub + "\n" + actual + "\n" + tests)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    str(source_file), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
