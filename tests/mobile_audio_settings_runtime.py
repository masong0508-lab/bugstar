#!/usr/bin/env python3
"""Execute production audio timing / resampling and atomic settings replacement."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/"pc/platform/pc_sdl.c").read_text()
def func(mark):
    a=s.index(mark);i=s.index("{",a)+1;d=1
    while d:
        d+=(s[i]=="{")-(s[i]=="}");i+=1
    return s[a:i]
assert "pc_psg_render(buf,nominal);" in s
assert "if(was_paused&&!g_bg)audio_resume();" in s
assert "was_paused=1; audio_suspend();" in s
assert "if(!cfg_finish(f,tmp,g_cfgpath))" in s
prefix=r"""
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#define PC_AUDIO_RATE 48000
#define RING 16384
#define AUDIO_TARGET 1200
#define AUDIO_TRIM 8
#define AUDIO_HIGH 4000
static int deny_sync,deny_rename;
static int test_sync(int fd){if(deny_sync){errno=ENOSPC;return -1;}return fsync(fd);}
static int test_rename(const char*a,const char*b){if(deny_rename){errno=EACCES;return -1;}return rename(a,b);}
#define fsync test_sync
#define rename test_rename
"""
cases=r"""
static void expect(const char*p,const char*want){char b[64]={0};FILE*f=fopen(p,"r");assert(f);assert(fgets(b,sizeof b,f));assert(!strcmp(b,want));fclose(f);}
static int replace_settings(const char*tmp,const char*dest,const char*content){FILE*f=fopen(tmp,"w");assert(f);fputs(content,f);return cfg_finish(f,tmp,dest);}
int main(void){
    uint64_t acc=0,total=0;unsigned checks=0;
    for(unsigned i=0;i<60000;i++){int n=audio_tick_samples(&acc);assert(n==803||n==804);total+=n;}
    assert(total==((60000ull*48000*280896)>>24));
    int16_t src[2048];for(int i=0;i<1024;i++){src[i*2]=(int16_t)(-16000+i*31);src[i*2+1]=(int16_t)(16000-i*31);}
    for(int nominal=803;nominal<=804;nominal++)for(unsigned fill=0;fill<=RING+512;fill++){
        int n=audio_queue_samples(nominal,fill);
        assert(n>=0&&n<=876);
        if(fill>AUDIO_HIGH)assert(n==0);else assert(fill+(unsigned)n<=RING);
        if(nominal==n)for(int i=0;i<n;i++)assert(audio_resample(src,nominal,n,i,0)==src[i*2]);
        if(n){for(int i=0;i<n;i+=31){int a=audio_resample(src,nominal,n,i,0),b=audio_resample(src,nominal,n,i,1);assert(a==-b);assert(a>=-16000&&a<=16000);}}
        checks++;
    }
    assert(replace_settings("settings.tmp","settings.ini","old\n"));
    deny_sync=1;assert(!replace_settings("settings.tmp","settings.ini","new\n"));expect("settings.ini","old\n");assert(access("settings.tmp",F_OK)!=0);
    deny_sync=0;deny_rename=1;assert(!replace_settings("settings.tmp","settings.ini","new\n"));expect("settings.ini","old\n");
    deny_rename=0;assert(replace_settings("settings.tmp","settings.ini","new\n"));expect("settings.ini","new\n");
    printf("Audio: %llu exact source samples over 60000 ticks, %u queue cases; atomic settings failure recovery PASS\n",(unsigned long long)total,checks);
}
"""
with tempfile.TemporaryDirectory() as td:
    d=Path(td);c=d/"test.c";exe=d/"test"
    c.write_text(prefix+func("static int audio_tick_samples(")+"\n"+func("static int audio_queue_samples(")+"\n"+func("static int16_t audio_resample(")+"\n"+func("static int cfg_finish(")+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],cwd=d,check=True)
