#!/usr/bin/env python3
"""Exercise actual restore decoder, transaction and backup preservation."""
from pathlib import Path
import subprocess,tempfile,struct,zlib
s=(Path(__file__).resolve().parents[1]/"pc/platform/pc_sdl.c").read_text()
a=s.index("static uint32_t crc_t[256]")
b=s.index("static void be32(",a)
c=s.index("#define TRANSFER_SIZE 32768")
d=s.index("static int transfer_result;",c)
assert "transfer_result=transfer_apply(g_savepath);" in s
assert "rename(stage,used)" in s[c:d]
prefix=r"""
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
static int deny_replace;
static int test_rename(const char*a,const char*b){
    if(deny_replace&&!strcmp(b,"game.sav")){errno=EACCES;return -1;}
    return rename(a,b);
}
#define rename test_rename
"""
cases=r"""
static void check(const char*p,int byte,int length){
    FILE*f=fopen(p,"rb");assert(f);
    for(int i=0;i<length;i++)assert(fgetc(f)==byte);
    assert(fgetc(f)==EOF);fclose(f);
}
int main(void){
    assert(transfer_apply("game.sav")==0);
    FILE*f=fopen("game.sav","wb");assert(f);
    for(int i=0;i<32769;i++)fputc(0x44,f);fclose(f);
    assert(transfer_apply("game.sav")==0);
    assert(rename("bad.bgsave","game.sav.restore")==0);
    assert(transfer_apply("game.sav")==-1);
    check("game.sav",0x44,32769);
    assert(rename("valid.bgsave","game.sav.restore")==0);
    deny_replace=1;assert(transfer_apply("game.sav")==-1);
    check("game.sav",0x44,32769);assert(access("game.sav.restore",F_OK)==0);
    deny_replace=0;assert(transfer_apply("game.sav")==1);
    check("game.sav",0x99,32768);check("game.sav.before-import",0x44,32769);
    assert(access("game.sav.restore",F_OK)!=0);
    assert(transfer_apply("game.sav")==0);
    assert(rename("fresh.bgsave","new.sav.restore")==0);
    assert(transfer_apply("new.sav")==1);check("new.sav",0x99,32768);
    assert(access("new.sav.before-import",F_OK)!=0);
    puts("Cartridge restore: CRC, exact length, failed replacement, retained backup and one-time application PASS");
}
"""
with tempfile.TemporaryDirectory() as td:
    p=Path(td);raw=bytes([0x99])*32768
    valid=b"BGSTAR1\n"+struct.pack(">I",zlib.crc32(raw))+raw
    (p/"valid.bgsave").write_bytes(valid)
    (p/"fresh.bgsave").write_bytes(valid)
    (p/"bad.bgsave").write_bytes(valid[:-1]+b"\x00")
    code=p/"t.c";exe=p/"t"
    code.write_text(prefix+s[a:b]+s[c:d]+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(code),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],cwd=p,check=True)
