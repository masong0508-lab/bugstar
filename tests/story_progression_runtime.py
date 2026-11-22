#!/usr/bin/env python3
"""Run the genuine Bugstar narrative-state and saved-finale logic under UBSan.

A player's first Technobeast victory must NOT gate the Dawn ending forever:
eight memories and a later victory can still complete it. The finale is
recorded in an existing, previously unused SaveB.stage bit, without changing
save layout or CRC. Bolt's hints must describe reachable actions at every
main-quest step, and never announce the Dawn prematurely on the Anthill.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
space=(ROOT/"space.inc").read_text()
ui=(ROOT/"ui.inc").read_text()
main=(ROOT/"main.c").read_text()
game=(ROOT/"game.inc").read_text()

def func(src,marker):
    # Match a function *definition*, not an earlier forward declaration.
    m=re.search(re.escape(marker)+r"[^;{}]*\{",src)
    assert m, f"missing definition: {marker}"
    start=src.index("{",m.start())
    depth,end=1,start+1
    while depth:
        depth+=(src[end]=="{")-(src[end]=="}")
        end+=1
    return src[i:end]

route=func(space,"static int bolt_next_step(")
hint=func(space,"static void bolt_hive_hint(")
ready=func(space,"static int beast_finale_ready(")
pack=func(space,"static u16 beast_stage_pack(")
unpack=func(space,"static void beast_stage_unpack(")
defaults=func(space,"static void beast_defaults(")
save=func(space,"static void beast_save(")
load=func(space,"static void beast_load(")
formatters="\n".join(func(game,"static void "+s+"(")
                     for s in ("txt_copy","txt_append","txt_number"))
save_type=re.search(r"typedef struct \{ u32 magic, hp_lo, hp_hi, mx_lo, mx_hi; u16 tech, kills, stage, hives; u32 sum; \} SaveB;",space)
hive_names=re.search(r"static const char \*const hive_nm\[5\]=\{[^;]*;",space)
hive_floors=re.search(r"static const u8 hive_floors\[5\]=\{[^;]*;",space)
assert save_type and hive_names and hive_floors

# Narrative / wiring assertions are checked against the actual game callers.
talk=func(ui,"static void bolt_talk(")
boss=func(space,"static void beast_shot(")
story=func(ui,"static void bolt_story(")
assert "switch(bolt_next_step())" in talk
assert "bolt_hive_hint(l1,sizeof l1)" in talk
assert "case 3:" in talk and "LAUNCH INTO SPACE. STAY ALERT." in talk
assert "HIVES DOWN. MY MEMORIES:" in talk
assert "FIVE HIVES DOWN. EIGHT MEMORIES WHOLE." in talk
assert "THE DAWN CAN FLY AGAIN." in talk
assert "case 6:" in talk and "L ON THE GALAXY MAP: BEAST'S LAIR." in talk
assert talk.index("case 4:") < talk.index("case 5:") < talk.index("case 6:")
assert "THE DAWN FELL HERE. SO DID I" not in ui
assert "THE KING SPLIT ME INTO EIGHT CORES." in ui
assert 'case 10:' in story and 'STAR CRUISER DAWN' in story
assert "if(beast_finale_ready())" in boss and "bolt_story(VISPG,10)" in boss
assert "dawn_free=1;" in boss
assert boss.index("if(beast_finale_ready())") < boss.index("else if(!beast_kills)")
assert "beast_stage_unpack(b.stage)" in load and "b.stage=beast_stage_pack()" in save
assert "dawn_free=0;" in defaults
assert "ITS WOUNDS NEVER HEAL." not in space[space.index("static void lair_enter("):space.index("static int uni_sub(")]
help_text=ui[ui.index("static const HelpLine help_pg"):ui.index("static const char *const help_ttl")]
assert "5 TECH HIVES KEEP IT FROM DYING" in help_text
assert "8 OUTPOST CORES RESTORE BOLT'S TRUE NAME" in help_text
assert "BEAT THE BEAST WITH 8 CORES: FREE DAWN" in help_text
assert 'else if(q_type==Q_BOSS){ scpy(qhint,' in talk
assert 'ncat(qhint,q_floor)' in talk
assert '#include "ui.inc"' in main and "static int bolt_next_step(void);" in main
assert "static void bolt_hive_hint(char *out, int cap);" in main

stub=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
static int ant_king,endless,zone_cleared,zone,beast_stage,hive_done;
static int data_cores,dawn_free,cleared_mask,tech_lv,beast_kills,cur_slot;
static u64 b3_max,b3_hp;
static u8 cart[4096];
static int slot_base(int slot){return slot*4096;}
static void sram_write(unsigned off,const void *data,unsigned n){
    if(off+n>sizeof cart){fputs("bad SRAM write\n",stderr);exit(1);}
    memcpy(cart+off,data,n);
}
static void sram_read(unsigned off,void *data,unsigned n){
    if(off+n>sizeof cart){fputs("bad SRAM read\n",stderr);exit(1);}
    memcpy(data,cart+off,n);
}
#define SB_OFF 0xA00
#define CHECK(x) do {if(!(x)){fprintf(stderr,"Story state at line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
cases=r"""
static u32 old_sum(SaveB *s){
    u32 h=2166136261u; const u8 *p=(const u8*)s;
    for(unsigned i=0;i<sizeof *s-4;i++){h^=p[i];h*=16777619u;}
    return h;
}
int main(void){
    unsigned cases=0;
    CHECK(sizeof(SaveB)==32);
    for(int king=0;king<=1;king++)
    for(int z=0;z<6;z++)for(int cleared=0;cleared<2;cleared++)
    for(int end=0;end<=1;end++)
    for(int beast=0;beast<4;beast++)
    for(int hive=0;hive<=5;hive++)
    for(int cores=0;cores<=8;cores++)
    for(int finale=0;finale<=1;finale++){
        ant_king=king;zone=z;zone_cleared=cleared;endless=end;
        beast_stage=beast;hive_done=hive;data_cores=cores;dawn_free=finale;
        int expected=!king?(!end&&!cleared?0:!end&&z<5?1:2):
                     beast<3?3:hive<5?4:cores<8?5:!finale?6:7;
        CHECK(bolt_next_step()==expected);
        CHECK(beast_finale_ready()==(cores>=8 && !finale));
        cases++;
    }
    for(int i=0;i<5;i++){
        hive_done=i;
        struct {u8 lo;char s[48];u8 hi;} b;
        b.lo=0xa5;b.hi=0x5a;
        bolt_hive_hint(b.s,sizeof b.s);
        CHECK(b.lo==0xa5 && b.hi==0x5a);
        char needle[10];sprintf(needle,"HIVE %d/5:",i+1);
        CHECK(strstr(b.s,needle)!=NULL);
        CHECK(strstr(b.s,hive_nm[i])!=NULL);
        char floor[8];sprintf(floor," B%u.",hive_floors[i]);
        CHECK(strstr(b.s,floor)!=NULL);
    }
    // Capped output remains NUL-terminated without touching the canary.
    {u8 b[3]={0xaa,0xbb,0xcc};hive_done=4;
     bolt_hive_hint((char*)&b[1],1);
     CHECK(b[0]==0xaa && b[1]==0 && b[2]==0xcc);}

    // Legacy clean saves have no bit 8. One later 8-core victory must be
    // able to show the finale even if earlier victories are recorded.
    beast_stage=3;hive_done=5;data_cores=8;dawn_free=0;
    beast_kills=4;tech_lv=12;b3_hp=0x100000010ull;b3_max=0x128000003ull;
    beast_save();
    SaveB saved;memcpy(&saved,cart+SB_OFF,sizeof saved);
    CHECK(saved.stage==3 && saved.kills==4 && saved.hives==5);
    CHECK(saved.sum==old_sum(&saved));
    beast_stage=0;hive_done=0;dawn_free=1;tech_lv=0;beast_kills=0;b3_hp=b3_max=0;
    beast_load();
    CHECK(beast_stage==3 && hive_done==5 && !dawn_free);
    CHECK(beast_kills==4 && tech_lv==12 && b3_hp==0x100000010ull && b3_max==0x128000003ull);
    CHECK(beast_finale_ready());
    // Actual flag packing is persisted and excludes repeat endings.
    dawn_free=1;beast_save();
    memcpy(&saved,cart+SB_OFF,sizeof saved);
    CHECK(saved.stage==0x103 && saved.sum==old_sum(&saved));
    dawn_free=0;beast_stage=0;beast_load();
    CHECK(beast_stage==3 && dawn_free==1 && !beast_finale_ready());
    // A corrupt block falls back to the old safe defaults.
    cart[SB_OFF+3]^=0x5a;
    ant_king=1;cleared_mask=0;dawn_free=1;hive_done=5;
    beast_load();
    CHECK(beast_stage==2 && hive_done==0 && dawn_free==0);
    // Save helpers stay layout-identical with and without the finale bit.
    for(int stage=0;stage<4;stage++)for(int seen=0;seen<2;seen++){
        beast_stage=stage;dawn_free=seen;
        u16 s=beast_stage_pack();
        beast_stage=7;dawn_free=!seen;beast_stage_unpack(s);
        CHECK(beast_stage==stage && dawn_free==seen);
    }
    printf("Story progression: %u campaign states, 5 hive hints, legacy/save-bit/finale recovery PASS\n",cases);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-story-") as d:
    src=Path(d)/"story.c";exe=Path(d)/"story"
    src.write_text(stub+"\n"+save_type.group(0)+"\n"+hive_names.group(0)+
                   "\n"+hive_floors.group(0)+"\n"+formatters+"\n"+route+
                   "\n"+hint+"\n"+ready+"\n"+pack+"\n"+unpack+
                   "\n"+defaults+"\n"+save+"\n"+load+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra","-Werror",
                    "-fsanitize=undefined","-fno-sanitize-recover=all",
                    str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
