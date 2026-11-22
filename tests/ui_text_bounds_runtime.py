#!/usr/bin/env python3
"""Guard gameplay UI strings and run the production planet HUD under sanitizers.

No test-only copy helpers: pull the actual bounded functions and HUD from the
GBA/PC/Android source. Audit actual creator, local-dialogue and hangar callers.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
game = (ROOT / "game.inc").read_text()
planet = (ROOT / "planet.inc").read_text()
town = (ROOT / "town.inc").read_text()
hangar = (ROOT / "hangar.inc").read_text()
ui = (ROOT / "ui.inc").read_text()


def cfunc(src, sig):
    pos = src.index(sig)
    start = src.index("{", pos)
    level, end = 1, start + 1
    while level:
        level += (src[end] == "{") - (src[end] == "}")
        end += 1
    return src[pos:end]


hud = cfunc(planet, "static void planet_hud(void){")
helpers = "\n".join(cfunc(game, "static void " + x + "(")
                    for x in ("txt_copy", "txt_append", "txt_number"))
assert "#endif if(" not in planet, "preprocessor swallowed planet/hell tiers"
assert re.search(r"#endif\s*\n\s*badge\[0\]", hud)
assert "txt_number(badge,sizeof badge,tiers[TZ]+1)" in hud
assert "txt_number(badge,sizeof badge,hell_tier())" in hud
assert "if(textlen(b)>room) b[room]=0;" in hud, "tier must fit long world name"
assert "(u32)((per>>5)+1)" in planet, "moon bobbing period has wrong precedence"

hanger = cfunc(hangar, "static int make_ship_(volatile u16 *pg, int first){")
assert "scpy(sk_msg," not in hanger, "unsafe ship-level feedback"
assert hanger.count("txt_copy(sk_msg,sizeof sk_msg,") >= 3
for message in ("THAT ONE IS AT ITS TOP RANK", "NO POINTS: FIGHT SHIPS TO LEVEL",
                "ONLY THIS VISIT'S POINTS RETURN", "UNDONE: ALL POINTS BACK",
                "NOTHING TO UNDO"):
    assert message in hanger and len(message) < 32, message

creator = cfunc(ui, "static int cc_run(")
k = creator.index("{ char ds[44];")
hint = creator[k:creator.index("ui_foot(pg,", k)]
assert "scpy(ds," not in hint and "scat(ds," not in hint
assert hint.count("txt_copy(ds,sizeof ds,") >= 6
assert "txt_append(ds,sizeof ds,dn)" in hint
for message in ("LEFT/RIGHT: CHOOSE A CALLING FOR PERKS",
                "LEFT/RIGHT: LOAD/SAVE SLOT 1-3. A: OK"):
    assert message in hint and len(message) < 44, message
assert "txt_append(b,sizeof b,cal_fx[cl])" in creator

local = cfunc(town, "static void local_talk(")
for old in ("scpy(g,", "scat(g,", "ncat(g,", "scpy(w,", "scat(w,"):
    assert old not in local, "unsafe NPC gift/dialogue " + old
assert "txt_append(g,sizeof g,tool_name[tl])" in local
assert "txt_append(w,sizeof w,tagline[wd.tagline])" in local
assert "EVERYONE WILL HEAR YOU WERE KIND. +FAME" in local
assert len("EVERYONE WILL HEAR YOU WERE KIND. +FAME") < 40

arrival = cfunc(town, "static void planet_card(")
for field in ("l1", "l2", "l3", "tl", "q2", "cm2"):
    assert "txt_copy(" + field + ",sizeof " + field in arrival
    assert "scat(" + field + "," not in arrival
    assert "ncat(" + field + "," not in arrival
assert "txt_append(l1,sizeof l1," in arrival

stub = r'''
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned int u32;
static int kw_mode, zone_cleared, endless, hell_on, hell_rank, topbar_calls;
static int tiers[1], hud_calls;
enum { TZ = 0 };
static u32 phud_sig;
static const char *name_text = "GARDEN";
static const char *th_str = "HASHED WORLD";
static char displayed[64];
static u32 hud_sig(void){ return 3u; }
static void kw_topbar(void){ topbar_calls++; }
static const char *world_name(void){ return name_text; }
static int textlen(const char *s){ return (int)strlen(s); }
static int hellw(void){ return hell_on; }
static int hell_tier(void){ return hell_rank; }
static void draw_hud_safe(const char *s){
    assert(strlen(s)<28);
    strcpy(displayed,s);
    hud_calls++;
}
'''
cases = r'''
int main(void){
    struct { unsigned char left; char value[7]; unsigned char right; } g;
    memset(&g,0,sizeof g); g.left=0xA5; g.right=0x5A;
    txt_copy(g.value,sizeof g.value,"THE LONGEST LABEL IN THE UNIVERSE");
    assert(strcmp(g.value,"THE LO")==0);
    txt_append(g.value,sizeof g.value," NEVER FITS");
    assert(strcmp(g.value,"THE LO")==0 && g.left==0xA5 && g.right==0x5A);
    g.value[0]=0; txt_number(g.value,sizeof g.value,-41);
    assert(strcmp(g.value,"-41")==0);
    char n[24]; n[0]=0; txt_number(n,sizeof n,INT_MIN);
    assert(strcmp(n,"-2147483648")==0);
    n[0]=0; txt_number(n,sizeof n,INT_MAX);
    assert(strcmp(n,"2147483647")==0);
    char one[1]; txt_copy(one,sizeof one,"X"); assert(one[0]==0);
    txt_append(one,sizeof one,"Y"); assert(one[0]==0);
    char full[4]={'a','b','c','d'};
    txt_append(full,sizeof full,"Z"); assert(strcmp(full,"abc")==0);

    name_text="GARDEN"; zone_cleared=0; endless=0; hell_on=0;
    planet_hud();
#ifdef TEST_HASH
    assert(!strcmp(displayed,"HASHED WORLD"));
    th_str="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#else
    assert(!strcmp(displayed,"GARDEN"));
#endif
    name_text="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    tiers[0]=12; zone_cleared=1;
    planet_hud(); assert(strlen(displayed)==27);
    assert(!strcmp(displayed+strlen(displayed)-4," T13"));
    zone_cleared=0; hell_on=1; hell_rank=6;
    planet_hud(); assert(!strcmp(displayed+strlen(displayed)-3," H6"));
    endless=1; planet_hud(); assert(!strstr(displayed," H6"));
    kw_mode=1; int before=hud_calls; planet_hud();
    assert(hud_calls==before && topbar_calls==1);
#ifdef TEST_HASH
    th_str="HASHED WORLD";
    kw_mode=0; endless=0; hell_on=0; zone_cleared=1; tiers[0]=8;
    planet_hud(); assert(!strcmp(displayed,"HASHED WORLD T9"));
#endif
    assert(phud_sig==3u);
    puts("Production UI bounds and planet HUD: ASan/UBSan PASS");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="bugstar-ui-bounds-") as td:
    source = Path(td) / "ui_bounds.c"
    exe = Path(td) / "ui_bounds"
    source.write_text(stub + "\n" + helpers + "\n" + hud + "\n" + cases)
    for flags in ([], ["-DTEST_HASH"]):
        subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-fno-sanitize-recover=all", *flags, str(source), "-o", str(exe)],
                       check=True)
        subprocess.run([str(exe)], check=True)
