#!/usr/bin/env python3
"""Exercise native space publication through the actual patched caller loops.

Apply the production build hooks in a temporary directory, then compile their
space loop, initial-launch block and Coil presentation tail with the production
publication helpers. A synchronous vblank observer checks all four display
phases, late effects, early returns and the unchanged CLASSIC ordering.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
native = (root / "pc/pc_snative.inc").read_text()
build = (root / "pc/build.sh").read_text()


def block(source, start):
    a = source.index(start)
    depth = 0
    for b in range(source.index("{", a), len(source)):
        if source[b] == "{":
            depth += 1
        elif source[b] == "}":
            depth -= 1
            if not depth:
                return source[a:b + 1]
    raise AssertionError("Unterminated block " + start)


with tempfile.TemporaryDirectory(prefix="bugstar-space-present-") as td:
    temp = Path(td)
    for name in ("main.c", "space.inc", "coil2.inc"):
        text = (root / name).read_text()
        if name == "main.c":
            text = '#include "pc_hw.h"\n' + text
        (temp / name).write_text(text)
    hooks = build.split(
        "# --- native space publication (tested from these exact guarded hooks)\n", 1
    )[1].split("# --- end native space publication", 1)[0]
    subprocess.run(
        ["bash", "-eu", "-c",
         'H() { python3 "$HOOK_SCRIPT" "$@"; }\n' + hooks],
        cwd=temp, env=os.environ | {"HOOK_SCRIPT": str(root / "pc/sim/hook.py")},
        check=True,
    )
    main = (temp / "main.c").read_text()
    space = (temp / "space.inc").read_text()
    coil = (temp / "coil2.inc").read_text()
    game = block(main, "static void run_game(void)")
    # Preserve the real 15 Hz scheduler, catch-up loop and page-flip sequence.
    cadence = game[game.index("        int sk=0;"):game.index("        read_keys();")]
    flight = block(game, "if(scene==2){\n            int r=0;")
    launch = block(main, "{ keys=keys_old=0; int r=space_frame();")
    fuzz = block(coil, "static int coil_fuzz(")
    tail = fuzz[fuzz.index("            pc_sn_publish(page_back);"):
                fuzz.index("            ok++; }") + len("            ok++;")]
    frame = block(space, "static int space_frame(void)")
    start = frame.split("{", 1)[1].split("//", 1)[0].strip()
    film = block(frame, "if(FILM){ if(space_msg_t)")
    finish = frame[frame.rindex("    pc_sn_frame_end(pg);"):].rsplit("}", 1)[0]
    # All other returns (menus, landing, death, lair and skipped drawing) occur
    # before both completion hooks in the real function.
    assert frame.count("pc_sn_frame_begin();") == 1
    assert frame.count("pc_sn_frame_end(pg);") == 2
    assert frame.index("pc_sn_frame_begin();") < frame.index("return")
    assert "return" not in frame[frame.index(film) + len(film):frame.rindex(finish)]
    assert game.count("pc_sn_publish(page_back);") == 2
    assert fuzz.count("pc_sn_publish(page_back);") == 1
    state = re.search(r"typedef struct \{[^{}]*\} PcSNat;", native).group()
    helpers = native[native.index("static int sn_ready_page=-1;"):
                     native.index("static void pc_sn_begin(void)")]
    timer = block(main, "static int tk_take(")
    harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
typedef uint16_t u16; typedef uint32_t u32;
#define GBAA(a) (a)
#define SP_BLANKS 4
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static u16 REG_DISPCNT, REG_KEYINPUT;
static int pc_nat_w,pc_nat_h,classic,scene,page_back,frames,invuln,sp_skip;
static int keys,keys_old,key_latch,space_msg_t,FILM,ok;
static u32 vbl_count,tk_slast;
static int tk_sacc;
static long display_frame;
static int generation,bitmap_generation[2],native_generation,late_generation[2];
static int rendered,kind,observe,expected_page,waits,menu_count,return_kind;
static int checks,normal_checks,launch_checks,fuzz_checks,classic_checks;
static jmp_buf stop;
static long pc_frames(void){ return display_frame; }
static int pc_v3d_classic(void){ return classic; }
''' + state + "\nstatic PcSNat pc_sn;\n" + helpers + "\n" + timer + r'''
static void pet_away_tick(void){}
static void make_frame(void){
    generation++; rendered=1;
    bitmap_generation[page_back]=native_generation=generation;
    pc_sn.w=pc_nat_w; pc_sn.h=pc_nat_h;
    pc_sn.f[page_back]=pc_sn.f2[page_back]=pc_sn.ff[page_back]=pc_sn.fc[page_back]=display_frame;
}
static void vsync(void){
    display_frame++; vbl_count++; waits++;
    if(observe && rendered){
        int t=(REG_DISPCNT>>4)&1;
        if(!classic){
            CHECK(bitmap_generation[t]==native_generation);
            CHECK(late_generation[t]==native_generation);
            CHECK(display_frame-pc_sn.f[t]>=1 && display_frame-pc_sn.f[t]<=4);
            CHECK(display_frame-pc_sn.f2[t]<=4);
            CHECK(display_frame-pc_sn.ff[t]<=4);
            CHECK(display_frame-pc_sn.fc[t]<=4);
            if(kind==1) launch_checks++; else if(kind==2) fuzz_checks++; else normal_checks++;
        } else { CHECK(t==expected_page); classic_checks++; }
        checks++;
    }
    if(waits>=256) longjmp(stop,1);
}
static void menu(void){
    CHECK(sn_ready_page==-1); menu_count++;
    int was=observe; observe=0;
    for(int i=0;i<12;i++) vsync();
    observe=was;
}
static int space_frame(void){
''' + start + r'''
    if(return_kind){
        // Begin must cancel even an unconsumed, otherwise-current token.
        CHECK(sn_ready_page==-1);
        int r=return_kind-1; if(r==0) menu(); return r;
    }
    if(sp_skip){ CHECK(sn_ready_page==-1); return 0; }
    volatile u16 *pg=(volatile u16*)(uintptr_t)(0x06000000u+(page_back?0xA000:0));
    make_frame();
''' + film + "\n" + finish + r'''
}
static void coil_fx_post(volatile u16 *pg){
    int t=(uintptr_t)pg>=0x0600A000u;
    CHECK(t==page_back); late_generation[t]=generation;
}
static void coil_fx_end(void){ if(rendered) late_generation[page_back]=generation; }
static void galaxy_map(void){ CHECK(sn_ready_page==-1); menu(); scene=0; }
static void station_menu(void){ CHECK(sn_ready_page==-1); menu(); scene=0; }
static void save_game(void){}
static void fade(int x){ (void)x; CHECK(sn_ready_page==-1); }
static void arrive_on_planet(void){ CHECK(sn_ready_page==-1); scene=0; }
static void ordinary(void){
    while(scene==2){
''' + cadence + "\n" + flight + r'''
    }
}
static void initial_launch(void){
''' + launch + r'''
}
static void coil_tail(void){
''' + tail + r'''
}
static void reset(void){
    memset(&pc_sn,0,sizeof(pc_sn)); pc_nat_w=960; pc_nat_h=640;
    memset(bitmap_generation,0,sizeof(bitmap_generation));
    memset(late_generation,0,sizeof(late_generation));
    REG_DISPCNT=0x1444; REG_KEYINPUT=1023; page_back=1; scene=2;
    sn_ready_page=-1; display_frame=vbl_count=100; tk_slast=vbl_count; tk_sacc=3;
    generation=native_generation=rendered=waits=menu_count=return_kind=0;
    classic=FILM=sp_skip=0; observe=1; kind=0;
}
static void gates(void){
    for(int g=0;g<12;g++){
        reset(); make_frame(); pc_sn_frame_end((volatile u16*)0x0600A000u);
        switch(g){
        case 0: classic=1; break;
        case 1: REG_DISPCNT=3; break;
        case 2: pc_sn.w=0; break;
        case 3: pc_sn.h=160; break;
        case 4: pc_nat_w=1200; break;
        case 5: pc_nat_h=800; break;
        case 6: pc_sn.f[1]--; break;
        case 7: pc_sn.f2[1]--; break;
        case 8: pc_sn.ff[1]--; break;
        case 9: pc_sn.fc[1]--; break;
        case 10: sn_ready_page=0; break;
        case 11: pc_sn_frame_begin(); break;
        }
        u16 dc=REG_DISPCNT; pc_sn_publish(1); CHECK(REG_DISPCNT==dc); CHECK(sn_ready_page==-1);
    }
    reset(); make_frame(); pc_sn_frame_end((volatile u16*)0x0600A000u);
    pc_sn_publish(-1); CHECK(REG_DISPCNT==0x1444 && sn_ready_page==-1);
    pc_sn_frame_end((volatile u16*)0x0600A000u); pc_sn_publish(2);
    CHECK(REG_DISPCNT==0x1444 && sn_ready_page==-1);
}
int main(void){
    gates();
    for(int film=0;film<2;film++){
        reset(); FILM=film; if(!setjmp(stop)) ordinary();
        CHECK(waits==256 && generation==64 && sn_ready_page==-1);
        // Missed logic steps are simulated without publishing incomplete frames.
        reset(); FILM=film; tk_sacc=12; if(!setjmp(stop)) ordinary();
        CHECK(waits==256 && generation==64 && sn_ready_page==-1);
        reset(); FILM=film; kind=1; initial_launch();
        CHECK(page_back==0 && sn_ready_page==-1);
        for(int i=0;i<3;i++) vsync();
        reset(); FILM=film; kind=2;
        for(int i=0;i<64;i++){
            CHECK(space_frame()==0); coil_fx_post((volatile u16*)(uintptr_t)(0x06000000u+(page_back?0xA000:0)));
            coil_tail();
        }
        CHECK(ok>=64 && sn_ready_page==-1);
    }
    // CLASSIC still presents the previous hardware page, then advances it once.
    reset(); classic=1; expected_page=0; initial_launch(); CHECK(page_back==0);
    expected_page=1; CHECK(space_frame()==0); coil_fx_end(); coil_tail(); CHECK(page_back==1);
    // Every early result, including an r==0 menu cancellation, retires readiness.
    for(int r=1;r<=4;r++){
        reset(); observe=0; make_frame(); sn_ready_page=1; return_kind=r;
        if(r==1){ CHECK(space_frame()==0); CHECK(menu_count==1); pc_sn_publish(1); }
        else ordinary();
        CHECK(sn_ready_page==-1);
    }
    printf("Native space publication: %d vblank checks (normal %d, launch %d, Coil %d, CLASSIC %d); four phases, FILM, catch-up, early exits and stale gates PASS\n",checks,normal_checks,launch_checks,fuzz_checks,classic_checks);
}
'''
    source = temp / "present.c"
    source.write_text(harness)
    compiler = os.environ.get("CC", "cc")
    if not shutil.which(compiler):
        raise SystemExit("C compiler required for space publication runtime test")
    program = temp / "present"
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Wno-int-to-pointer-cast",
                    "-fsanitize=undefined,bounds", "-fno-sanitize-recover=all",
                    str(source), "-o", str(program)], check=True)
    subprocess.run([str(program)], check=True)
