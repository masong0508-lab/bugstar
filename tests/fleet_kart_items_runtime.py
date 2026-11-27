#!/usr/bin/env python3
"""Compile actual fleet target selection and kart item handling under UBSan."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
sp=(root/"space.inc").read_text();k=(root/"kart.inc").read_text()
def fun(s,mark):
    a=s.index(mark);i=s.index("{",a)+1;d=1
    while d:
        d+=(s[i]=="{")-(s[i]=="}");i+=1
    return s[a:i]
assert "sp_target_update(p,i,&urgent_scans);" in sp
assert "else if(sp_target_live(p->tgt,p->team) && sproj" in sp
assert "else if(tab==2){\n            // The lounge is a conversation panel" in sp
assert "if(!kitem_active(r)||rc[r].item)continue;" in k
assert "if(!kitem_active(i)||!kitem_active(j)" in k
assert "else {h->life=0;kpo_release(h->po);continue;}" in k
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;typedef int8_t s8;typedef uint32_t u32;typedef int32_t s32;
#define NPIR 64
#define NRC 12
typedef struct {s32 x,y,z;int alive,team,tgt,fire_t;} Pirate;
static Pirate pir[NPIR];static s32 shx,shy,shz;static unsigned frames;
enum {KI_NONE,KI_PEPPER,KI_SHELL,KI_SLIME,KI_BUBBLE,KI_MAGNET,KI_SNEEZE,KI_PEPPER3,KI_CLOUD};
enum {O_KSHELL,O_KSLIME};
typedef struct{s32 x,y;int pi,hd,fin,out,la,spin,shield,place,item,item_n,boost,cloud,ai_t;}Racer;
typedef struct{s32 x,y;int pi,owner,tgt,life,po;}KShell;
static Racer rc[NRC];static int nrc=4,kloop_on,kboost_t,kdrift,kcharge,kstar,alloc_ok,allocs;
static KShell ksh[3];static u8 kslm_po[6],kslm_life[6];
static int kpnear(s32 x,s32 y){(void)x;(void)y;return 0;}
static void show_msg(const char*s){(void)s;}static void sfx(int n){(void)n;}
static int sinx(int x){(void)x;return -1024;}static int cosx(int x){(void)x;return 16384;}
static int kgrab(int kind,s32 x,s32 y){(void)kind;(void)x;(void)y;allocs++;return alloc_ok?5:-1;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Fleet/kart line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
cases=r"""
int main(void){
    for(int i=0;i<63;i++)pir[i]=(Pirate){1000+i,0,0,1,1,63,4};
    pir[63].alive=0;frames=1;int urgent=0,scans=0;
    for(int i=0;i<63;i++)scans+=sp_target_update(&pir[i],i,&urgent);
    CHECK(scans<=6);
    for(int i=0;i<63;i++)CHECK(pir[i].tgt==-1&&pir[i].fire_t==0);
    for(int i=0;i<NPIR;i++)pir[i]=(Pirate){1000+i,0,0,1,i%2,i%2?-1:-2,0};
    for(frames=0;frames<32;frames++){urgent=scans=0;for(int i=0;i<NPIR;i++)scans+=sp_target_update(&pir[i],i,&urgent);CHECK(scans==2);}
    for(int i=0;i<NPIR;i++)CHECK(pir[i].tgt==(pir[i].team?-1:-2)||sp_target_live(pir[i].tgt,pir[i].team));
    for(int i=0;i<NPIR;i++){pir[i].tgt=127;pir[i].fire_t=3;}urgent=scans=0;
    for(int i=0;i<NPIR;i++){scans+=sp_target_update(&pir[i],i,&urgent);CHECK(pir[i].tgt==(pir[i].team?-1:-2)||sp_target_live(pir[i].tgt,pir[i].team));}
    CHECK(scans<=6&&sp_distance16_sq(500000,0,0)==0xffffffffu);
    for(int i=0;i<nrc;i++)rc[i].place=i+1;
    rc[0].item=KI_SHELL;rc[0].item_n=1;
    for(int i=0;i<3;i++)ksh[i].life=90;
    kuse_item(0);CHECK(rc[0].item==KI_SHELL&&rc[0].item_n==1&&allocs==0);
    ksh[1].life=0;alloc_ok=0;kuse_item(0);CHECK(rc[0].item==KI_SHELL&&allocs==1);
    alloc_ok=1;kuse_item(0);CHECK(!rc[0].item&&ksh[1].life==240&&ksh[1].po==5);
    rc[1].item=KI_SLIME;rc[1].item_n=1;memset(kslm_life,1,sizeof kslm_life);
    kuse_item(1);CHECK(rc[1].item==KI_SLIME&&rc[1].ai_t>0);
    kslm_life[0]=0;kuse_item(1);CHECK(!rc[1].item&&kslm_life[0]==255);
    rc[0].fin=1;rc[1].out=1;CHECK(kahead(3)==2);rc[2].fin=1;CHECK(kahead(3)==-1);
    kspin(1,"test");CHECK(!rc[1].spin);
    rc[3].item=KI_PEPPER3;rc[3].item_n=3;kuse_item(3);CHECK(rc[3].item_n==2&&rc[3].item==KI_PEPPER3);
    rc[3].out=1;kuse_item(3);CHECK(rc[3].item_n==2);
    puts("Fleet mass target loss, 2048 scheduled refreshes, bounded searches; kart item pool, retry and active targets PASS");return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
    p=Path(td);c=p/"test.c";exe=p/"test"
    fleet=("static int sp_target_live(","static u32 sp_distance16_sq(","static int sp_best_target(","static int sp_target_update(")
    kart=("static int kitem_active(","static void kspin(","static int kahead(","static void kitem_wait(","static void kuse_item(")
    c.write_text(prefix+"\n".join(fun(sp,m) for m in fleet)+"\n"+"\n".join(fun(k,m) for m in kart)+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
