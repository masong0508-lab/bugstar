#!/usr/bin/env python3
"""Exercise production Bugstar ore SRAM and smelter functions under ASan/UBSan.

Rich/pristine quantities used to be erased during ore_load: gr_load called
ore_fix before ore_n was restored. Crafting also consumed inputs even when
the 9,999-item stock cap discarded the result. Run the actual extracted C
helpers and player-facing sm_make caller, not a Python reimplementation.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ore = (ROOT / "ore.inc").read_text()
ui = (ROOT / "ui.inc").read_text()

def fun(src, marker):
    m = re.search(re.escape(marker) + r"[^;{}]*\{", src)
    assert m, f"missing function {marker}"
    brace = src.index("{", m.start())
    depth, end = 1, brace + 1
    while depth:
        depth += (src[end] == "{") - (src[end] == "}")
        end += 1
    return src[m.start():end]

def declaration(src, pattern):
    m = re.search(pattern, src, re.S)
    assert m, f"missing production declaration: {pattern}"
    return m.group(0)

# Check real menu binding, not just isolated helpers.
assert "if(q->kind==1 && !sm_room(q->out))" in ui
assert "if(n>room) n=room;" in ui
assert "if(!n){ say(\"BAR STOCK FULL. SELL SOME FIRST\"); return; }" in ui
assert "sm_room(sm_rec[sm_job-1].out)" in ui
assert "STOCK FULL" in ui
assert "gr_load();" in fun(ore, "static void ore_load(")
assert fun(ore, "static void ore_load(").index("for(int i=0;i<NORE;i++) ore_n[i]=") < fun(ore, "static void ore_load(").index("gr_load();")
assert "s.n[i]>9999 ? 9999 : s.n[i]" in fun(ore, "static void ore_load(")

defs = [
    declaration(ore, r"typedef struct \{ u8 kind, out, a, na, b, nb; \} Recipe;"),
    declaration(ore, r"static const Recipe sm_rec\[NREC\]=\{.*?\};"),
    declaration(ore, r"typedef struct \{ u32 magic; u8 ver, tool, ring_i, pad;.*?\} OreSave;"),
    declaration(ore, r"typedef struct \{ u32 magic; u8 ver, pad\[3\]; u16 rich\[8\], prist\[8\]; u32 sum; \} GrSave;"),
]
helpers = "\n\n".join(fun(ore, "static " + name) for name in (
    "int sp_have(", "int sp_cap(", "void sp_add(",
    "int sm_have(", "int sm_batches(", "int sm_ready(", "int sm_room(",
    "int sm_collect(", "int sm_left(", "void ore_fix(",
    "void gr_save(", "void gr_load(", "void ore_reset(", "void ore_save(",
    "void ore_load(", "int ore_add(", "int ore_add_q(", "int ore_take("
))
make = fun(ui, "static void sm_make(")

stub = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef int64_t s64;
#define NORE 12
#define ORE_RING 16
#define ORE_OFF 0xB60
#define GR_OFF 0x868
#define NMAT 34
#define NREC 56
#define SM_MAXB 5
#define SM_SECS 15
#define NSK 8
#define HP_MAX 10
#define SP_MAX 10
static u8 cart[0x8000];
static int cur_slot;
static u32 clock_seconds;
static u32 play_secs(void){ return clock_seconds; }
static unsigned slot_base(int slot){ return (unsigned)slot * 0x8000u; }
static u32 fnv(const void *p, unsigned n){
    const u8 *b=(const u8 *)p; u32 h=2166136261u;
    for(unsigned i=0;i<n;i++){ h^=b[i]; h*=16777619u; }
    return h;
}
static void sram_write(unsigned off, const void *p, unsigned n){
    if(off+n>sizeof cart){ fputs("SRAM write outside slot\n",stderr); exit(2); }
    memcpy(cart+off,p,n);
}
static void sram_read(unsigned off, void *p, unsigned n){
    if(off+n>sizeof cart){ fputs("SRAM read outside slot\n",stderr); exit(2); }
    memcpy(p,cart+off,n);
}
static u16 ore_n[NORE],ore_rich[NORE],ore_prist[NORE],ore_ring[ORE_RING];
static u8 ore_ring_i,ore_tool;
static u16 sm_n[NMAT];
static u8 sm_job,sm_jn;
static u32 sm_done,ship_pts;
static u16 ship_rk[NSK];
static u8 sp_n[3],hull_plates;
static const char *const sm_name[NMAT]={
    [0]="IRON BAR", [1]="COPPER BAR", [7]="PLATING"
};
static const char *const ore_tool_name[4]={"JAWS","PICKAXE","DRILL","LASER CUTTER"};
static const char *const sp_name[4]={"HULL PLATE","ENGINE TUNE","LASER LENS","SHIELD CELL"};
static void sm_reset(void){
    memset(sm_n,0,sizeof sm_n); sm_job=sm_jn=0; sm_done=0;
    hull_plates=0; memset(sp_n,0,sizeof sp_n);
}
static void sm_save(void){}
static void sm_load(void){ sm_reset(); }
static void sr_save(void){}
static void sr_load(void){}
static void ml_reset(void){}
static void ml_save(void){}
static void ml_load(void){}
static void belt_reset(void){}
static void belt_save(void){}
static void belt_load(void){}
static char said[160];
static void scpy(char *b,const char *s){ strcpy(b,s); }
static void scat(char *b,const char *s){ strcat(b,s); }
static void ncat(char *b,int n){ char tmp[24];snprintf(tmp,sizeof tmp,"%d",n);strcat(b,tmp); }
static void say(const char *s){ snprintf(said,sizeof said,"%s",s); }
static void sfx(int n){ (void)n; }
static void hud_now(void){}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"MINING TEST failed on line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""

cases = r"""
int main(void){
    unsigned tested=0;
    memset(cart,0,sizeof cart);
    // Save a mixed ore bag and a nearly-full grade distribution.
    ore_n[4]=10; ore_rich[4]=3; ore_prist[4]=2;
    ore_n[5]=6; ore_rich[5]=2; ore_prist[5]=1;
    ore_ring[0]=124; ore_ring_i=1; ore_tool=2;
    ore_save(); ore_reset(); ore_load();
    CHECK(ore_n[4]==10 && ore_rich[4]==3 && ore_prist[4]==2);
    CHECK(ore_n[5]==6 && ore_rich[5]==2 && ore_prist[5]==1);
    CHECK(ore_tool==2 && ore_ring_i==1 && ore_ring[0]==124); tested++;
    // Plain ore is taken before the rich, and pristine remains last.
    CHECK(ore_take(4,5));
    CHECK(ore_n[4]==5 && ore_rich[4]==3 && ore_prist[4]==2);
    CHECK(ore_take(4,1));
    CHECK(ore_n[4]==4 && ore_rich[4]==2 && ore_prist[4]==2);
    ore_save(); ore_reset(); ore_load();
    CHECK(ore_n[4]==4 && ore_rich[4]==2 && ore_prist[4]==2); tested++;
    CHECK(ore_add_q(4,2,3)==2);
    CHECK(ore_n[4]==6 && ore_rich[4]==2 && ore_prist[4]==4);
    ore_save(); ore_reset(); ore_load();
    CHECK(ore_n[4]==6 && ore_rich[4]==2 && ore_prist[4]==4); tested++;

    // An obsolete or corrupted grade block must not destroy the ore counts.
    cart[GR_OFF+8]^=1;
    ore_reset(); ore_load();
    CHECK(ore_n[4]==6 && ore_rich[4]==0 && ore_prist[4]==0); tested++;
    // A valid but inconsistent legacy grade file must fit its ore totals.
    ore_n[4]=6; ore_rich[4]=2; ore_prist[4]=4; ore_save();
    GrSave g; memcpy(&g,cart+GR_OFF,sizeof g);
    g.rich[0]=60000;g.prist[0]=60000;g.sum=fnv(&g,sizeof g-4);
    memcpy(cart+GR_OFF,&g,sizeof g);
    ore_reset();ore_load();
    CHECK(ore_n[4]==6 && ore_prist[4]==6 && ore_rich[4]==0); tested++;
    // The standalone ore block has historically allowed invalid >9999 counts.
    OreSave os; memcpy(&os,cart+ORE_OFF,sizeof os);
    os.n[4]=65535;os.sum=fnv(&os,sizeof os-4);
    memcpy(cart+ORE_OFF,&os,sizeof os);
    ore_reset();ore_load();
    CHECK(ore_n[4]==9999 && ore_prist[4]==9999 && ore_rich[4]==0);
    CHECK(ore_add(4,1)==0 && ore_n[4]==9999); tested++;
    // A damaged base bag must not import dangling grade records.
    cart[ORE_OFF+12]^=0x20;
    ore_reset();ore_load();
    CHECK(ore_n[4]==0 && ore_rich[4]==0 && ore_prist[4]==0); tested++;

    // Smelting a full bar stock must NOT consume the input ores.
    sm_reset();ore_n[4]=20;sm_n[0]=9999;clock_seconds=100;
    sm_make(0);
    CHECK(!sm_job && ore_n[4]==20 && sm_n[0]==9999);
    CHECK(strstr(said,"FULL")!=NULL);tested++;
    // Near-full stock can queue only as many batches as will fit.
    sm_n[0]=9997;sm_make(0);
    CHECK(sm_job==1 && sm_jn==2 && ore_n[4]==16 && sm_done==130);
    clock_seconds=130;
    CHECK(sm_collect()==2 && sm_n[0]==9999 && !sm_job && !sm_jn);tested++;
    // Legacy pre-fix saves can contain a completed batch larger than stock.
    sm_job=1;sm_jn=5;sm_done=130;sm_n[0]=9998;
    CHECK(sm_collect()==1 && sm_n[0]==9999 && sm_jn==4 && sm_job==1);
    CHECK(sm_collect()==0 && sm_jn==4 && sm_job==1);
    sm_make(0);
    CHECK(sm_job==1 && sm_jn==4 && strstr(said,"FULL")!=NULL);
    sm_n[0]=9997;
    sm_make(0);
    CHECK(sm_n[0]==9999 && sm_jn==2 && sm_job==1 && strstr(said,"COLLECTED 2")!=NULL);
    sm_n[0]=9997;
    CHECK(sm_collect()==2 && sm_n[0]==9999 && sm_job==0 && sm_jn==0);tested++;
    // Part crafting cannot burn inputs when the output stack is full.
    sm_reset();sm_n[7]=9999;sm_n[0]=10;sm_n[1]=10;
    sm_make(7);
    CHECK(sm_n[7]==9999 && sm_n[0]==10 && sm_n[1]==10);
    CHECK(strstr(said,"FULL")!=NULL);
    sm_n[7]=9998;sm_make(7);
    CHECK(sm_n[7]==9999 && sm_n[0]==8 && sm_n[1]==9);
    CHECK(strstr(said,"MADE 1")!=NULL);tested++;
    printf("Ore-grade SRAM, legacy files and cap-safe smelting: PASS (%u cases)\n",tested);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-mining-regression-") as td:
    src=Path(td)/"mining.c"
    exe=Path(td)/"mining"
    src.write_text(stub+"\n"+"\n".join(defs)+"\n"+helpers+"\n"+make+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-omit-frame-pointer",
                    "-fno-sanitize-recover=all",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
