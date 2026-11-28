#!/usr/bin/env python3
"""Exercise actual SRAM replacement code, including forced write failures."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/"pc/pc_hw.c").read_text()
a=s.index("static int save_write(")
b=s.index("\nvoid pc_set_save_path",a)
prefix=r"""
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#define PC_ANDROID 1
static unsigned char data[0x8000];
#define SRAMB ((uintptr_t)data)
static int deny_rename,deny_sync;
static int test_rename(const char *a,const char*b){
    if(deny_rename){errno=EACCES;return -1;}
    return rename(a,b);
}
static int test_sync(int fd){
    if(deny_sync){errno=ENOSPC;return -1;}
    return fsync(fd);
}
#define rename test_rename
#define fsync test_sync
"""
cases=r"""
static void check_file(const char *p,int byte){
    FILE *f=fopen(p,"rb");assert(f);for(int i=0;i<0x8000;i++)assert(fgetc(f)==byte);
    assert(fgetc(f)==EOF);assert(fclose(f)==0);
}
int main(int argc,char**argv){
    assert(argc==2);
    char path[1024],tmp[1100];snprintf(path,sizeof path,"%s/save.sav",argv[1]);snprintf(tmp,sizeof tmp,"%s.tmp",path);
    memset(data,0x33,sizeof data);assert(save_write(path));check_file(path,0x33);
    memset(data,0x99,sizeof data);deny_rename=1;assert(!save_write(path));check_file(path,0x33);assert(access(tmp,F_OK)!=0);
    deny_rename=0;deny_sync=1;assert(!save_write(path));check_file(path,0x33);assert(access(tmp,F_OK)!=0);
    deny_sync=0;assert(save_write(path));check_file(path,0x99);assert(access(tmp,F_OK)!=0);
    char bad[1400];memset(bad,'x',sizeof bad);bad[sizeof bad-1]=0;assert(!save_write(bad));
    puts("SRAM atomic replacement, failed rename, failed disk sync and path bounds PASS");
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"save.c";exe=Path(td)/"save"
    c.write_text(prefix+s[a:b]+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe),td],check=True)
