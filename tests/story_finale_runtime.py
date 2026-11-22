#!/usr/bin/env python3
"""Run Bugstar's actual Technobeast save/load and Dawn-ending gate under UBSan.

Keeps SaveB's original SRAM layout and checks legacy saves, early beast kills,
the eight-core finale, restart persistence and damaged saves. The C code
under test is extracted from the same space.inc compiled into every port.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "space.inc").read_text()
ui = (root / "ui.inc").read_text()
main = (root / "main.c").read_text()


def extract(signature):
    start = source.index(signature + "{")
    begin = source.index("{", start)
    nest = 0
    for idx in range(begin, len(source)):
        if source[idx] == "{":
            nest += 1
        elif source[idx] == "}":
            nest -= 1
            if nest == 0:
                return source[start:idx+1]
    raise AssertionError("unbalanced C function: " + signature)


struct_start = source.index("typedef struct { u32 magic, hp_lo, hp_hi, mx_lo, mx_hi;")
struct_end = source.index("} SaveB;", struct_start) + len("} SaveB;")
record = source[struct_start:struct_end]

logic = "\n".join(extract(sig) for sig in (
    "static int beast_finale_ready(int cores, int released)",
    "static void beast_defaults(void)",
    "static void beast_load(void)",
    "static void beast_save(void)",
))

# Check actual event wiring: ending can play after previous kills, but only
# with all eight cores, and is explicitly marked as complete before saving.
assert "if(beast_finale_ready(data_cores,dawn_free))" in source
assert "bolt_story(VISPG,10); dawn_free=1;" in source
assert "else if(!beast_kills && data_cores<8)" in source
assert "static int dawn_free;" in main
assert 'if(dawn_free){ t[n++]=' in ui
assert "if(!beast_kills){ if(data_cores>=8)" not in source

preamble = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#define SB_OFF 0xA00
#define CHECK(e) do { if(!(e)){ fprintf(stderr,"beast story save failed at %d: %s\n",__LINE__,#e); return 1; } } while(0)
static int hive_done,tech_lv,beast_kills,beast_stage,dawn_free,ant_king,cleared_mask,cur_slot;
static u64 b3_hp,b3_max;
static u8 save_data[64];
static u32 slot_base(int slot){ (void)slot; return 0; }
static void sram_read(u32 off,void *p,unsigned size){
    if(off != SB_OFF || size>sizeof(save_data)) { fprintf(stderr,"bad SRAM read\n"); return; }
    memcpy(p,save_data,size);
}
static void sram_write(u32 off,const void *p,unsigned size){
    if(off != SB_OFF || size>sizeof(save_data)) { fprintf(stderr,"bad SRAM write\n"); return; }
    memcpy(save_data,p,size);
}
"""

test = r"""
static u32 rec_sum(const SaveB *b){
    u32 h=2166136261u; const u8 *p=(const u8 *)b;
    for(unsigned i=0;i<sizeof *b-4;i++){h^=p[i];h*=16777619u;}
    return h;
}
int main(void){
    CHECK(sizeof(SaveB)==32);
    beast_defaults();
    CHECK(beast_kills==0 && dawn_free==0);

    // A legacy save predating the ending flag (stage=3, kills=2) must
    // remain valid and be eligible to complete the ending once cores=8.
    SaveB old={0};
    old.magic=0x32534542; old.hp_lo=1; old.mx_lo=2;
    old.tech=4; old.kills=2; old.stage=3; old.hives=5;
    old.sum=rec_sum(&old); memcpy(save_data,&old,sizeof old);
    dawn_free=1; beast_kills=0; beast_stage=0;
    beast_load();
    CHECK(beast_stage==3 && beast_kills==2 && dawn_free==0);
    CHECK(tech_lv==4 && hive_done==5);
    CHECK(!beast_finale_ready(7,dawn_free));
    CHECK(beast_finale_ready(8,dawn_free)); // Old early wins do NOT softlock Dawn

    // Winning with all cores marks the cinematic once, then save to the
    // exact same SRAM bytes as before, and reload after an app restart.
    dawn_free=1;
    beast_save();
    SaveB stored;
    memcpy(&stored,save_data,sizeof stored);
    CHECK((stored.stage&0x8000u)!=0);
    CHECK((stored.stage&0x7fffu)==3);
    CHECK(stored.kills==2 && stored.hives==5 && stored.tech==4);
    CHECK(stored.sum==rec_sum(&stored));
    dawn_free=0; beast_kills=0; beast_stage=0;
    beast_load();
    CHECK(dawn_free==1 && beast_stage==3 && beast_kills==2);
    CHECK(!beast_finale_ready(8,dawn_free)); // Does not replay every victory

    // Earlier stages must still survive stage-bit encoding unmodified.
    for(int stage=0;stage<4;stage++){
        beast_stage=stage; dawn_free=1; beast_save();
        beast_stage=0; dawn_free=0; beast_load();
        CHECK(beast_stage==stage && dawn_free==1);
    }

    // An invalid checksum resets the finale flag too, not just kills.
    save_data[11]^=0x20;
    dawn_free=1; beast_kills=9;
    beast_load();
    CHECK(!dawn_free && beast_kills==0);
    puts("Dawn story: legacy wins, all-core finale, persistence, and damage fallback PASS");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-dawn-finale-") as tmp:
    src = Path(tmp) / "story_save_test.c"
    exe = Path(tmp) / "story_save_test"
    src.write_text(preamble + "\n" + record + "\n" + logic + "\n" + test)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
