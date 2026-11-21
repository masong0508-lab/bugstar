#!/usr/bin/env python3
"""Run Bugstar's real Kart World save loader against valid and malformed SRAM.

Extracts kw_defaults and kw_load directly from kart3.inc. Only external
storage and helper functions are stubbed. This is a host-side UBSan
regression, not an emulator or hardware validation.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "kart3.inc").read_text()
assert 'list_pick(pg,"WHO DRIVES?",NKWD,kwd_row' in source, (
    "Kart World driver selection must expose all 18 drivers"
)


def function(signature):
    start = source.find(signature + "{")
    assert start >= 0, "missing actual C function: " + signature
    opening = source.index("{", start)
    depth = 0
    for pos in range(opening, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start : pos + 1]
    raise AssertionError("unbalanced function " + signature)


record_start = source.index("typedef struct { u32 magic; u8 unlocked")
record_end = source.index("} KWSave;", record_start) + len("} KWSave;")
record = source[record_start:record_end]
defaults = function("static void kw_defaults(int unlocked)")
load = function("static void kw_load(void)")

prefix = r"""
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define NKPART 11
#define NKWT 10
#define NKWD 18
#define NRC 12
#define NPC 16
#define KW_TRACKS 1000000u
#define KW_OFF 0x6A10
#define KP_BODY 0
#define KP_SIZE 1
#define KP_EXH 6
#define NKBODY 16
#define C15(r,g,b) ((u16)((((r)>>3)&31) | ((((g)>>3)&31)<<5) | ((((b)>>3)&31)<<10)))
typedef struct { u8 part[NKPART]; u8 number, pad; u16 col[3]; char name[12]; } KLook;
typedef struct { u32 key; u16 best; u8 medal, chk; } KBest;
static const u8 kpart_n[NKPART]={NKBODY,3,12,8,12,8,8,16,10,8,8};
static void scpy(char *dst,const char *src){ strcpy(dst,src); }
static u32 fnv(const void *ptr,size_t len){
    const u8 *b=(const u8 *)ptr;
    u32 hash=2166136261u;
    for(size_t i=0;i<len;i++) hash=(hash^b[i])*16777619u;
    return hash;
}
"""

storage = r"""
static KWSave kw, saved;
static void sram_read(unsigned off,void *dst,size_t n){
    (void)off;
    memcpy(dst,&saved,n);
}
static void write_saved(KWSave value){
    value.sum=fnv(&value,sizeof value-4);
    saved=value;
}
"""

checks = r"""
#define CHECK(c) do { if(!(c)){ fprintf(stderr,"Kart World save test failed at line %d: %s\n",__LINE__,#c); return 1; } } while(0)
int main(void){
    kw_defaults(1);
    KWSave valid=kw;
    valid.driver=17; valid.cls_open=5; valid.cls=4;
    valid.ftype=NPC; valid.fsize=6;
    valid.track=KW_TRACKS-1; valid.laps=9; valid.riv=NRC-1;
    valid.look.part[KP_BODY]=NKBODY-1;
    valid.look.part[KP_SIZE]=2;
    valid.coins=8375; valid.races=27;
    valid.troph[4][3]=3;
    write_saved(valid);
    kw_load();
    CHECK(kw.driver==17 && kw.cls_open==5 && kw.cls==4);
    CHECK(kw.ftype==NPC && kw.fsize==6 && kw.track==KW_TRACKS-1);
    CHECK(kw.look.part[KP_BODY]==NKBODY-1 && kw.look.part[KP_SIZE]==2);
    CHECK(kw.coins==8375 && kw.races==27 && kw.troph[4][3]==3);

    KWSave corrupt=valid;
    for(int i=0;i<NKPART;i++) corrupt.look.part[i]=255;
    memset(corrupt.look.name,'N',sizeof corrupt.look.name);
    corrupt.look.number=255;
    corrupt.driver=255; corrupt.cls_open=0; corrupt.cls=255;
    corrupt.ftype=255; corrupt.fsize=255; corrupt.track=KW_TRACKS+1;
    corrupt.laps=255; corrupt.riv=255; corrupt.items=255;
    corrupt.skill=255; corrupt.stunts=255; corrupt.hour=255;
    corrupt.weather=255; corrupt.catchup=255; corrupt.mirror=255;
    corrupt.reverse=255; corrupt.weird=255;
    memset(corrupt.cam,255,sizeof corrupt.cam);
    write_saved(corrupt);
    kw_load();
    CHECK(kw.driver==0 && kw.cls_open==1 && kw.cls==0);
    CHECK(kw.ftype==0 && kw.fsize==0 && kw.track==0);
    CHECK(kw.laps==0 && kw.riv==NRC-1 && kw.items==1);
    CHECK(kw.skill==1 && kw.stunts==0 && kw.hour==0 && kw.weather==0);
    CHECK(kw.catchup==1 && kw.mirror==1 && kw.reverse==1 && kw.weird==1);
    CHECK(kw.look.part[KP_SIZE]==1 && kw.look.part[KP_BODY]==0);
    CHECK(kw.look.name[11]==0 && kw.look.number==55);
    for(int i=0;i<NKPART;i++) CHECK(kw.look.part[i]<kpart_n[i]);
    CHECK(kw.cam[0]<=1 && kw.cam[1]<5 && kw.cam[2]<5);
    CHECK(kw.cam[3]<4 && kw.cam[4]<4);
    CHECK(kw.coins==valid.coins && kw.troph[4][3]==3);

    // Bad checksum must fall back to safe defaults rather than decode garbage.
    saved.sum^=0x100u;
    kw_load();
    CHECK(kw.unlocked==0 && kw.cls_open==2 && kw.cls==1);
    CHECK(kw.driver==0 && kw.riv==11);
    puts("Kart World authentic save loader: valid records, corrupt settings, checksum fallback PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-kw-save-") as d:
    src = Path(d) / "kart_world_test.c"
    exe = Path(d) / "kart_world_test"
    src.write_text(prefix + "\n" + record + "\n" + storage + "\n" +
                   defaults + "\n" + load + "\n" + checks)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
