#!/usr/bin/env python3
"""Exercise checkpoint-based race ranking and record eligibility using production code."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
k=(root/"kart.inc").read_text();r=(root/"kart2.inc").read_text()
def fn(src,mark):
    a=src.index(mark);i=src.index("{",a)+1;d=1
    while d:
        d+=(src[i]=="{")-(src[i]=="}");i+=1
    return src[a:i]
assert "if(kart_record_eligible(rc[0].fin,rc[0].out,k_type,rc[0].ftime))" in r
assert 'if(k->out)scpy(t,k_type==6?"OUT":"DNF")' in r
assert "rc[0].out=1;rc[0].place=(u8)nrc" in k
assert 'if(kitem_active(0)){ int g=rc[0].nextg;' in k
prefix=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;
#define NRC 12
typedef struct {int lap,pi,nextg,fin,out,place;} Racer;
static Racer rc[NRC];static int nrc=12,knp=240;
static u16 kgate_pi[6]={0,40,80,120,160,200};
static struct {int n;} kt={6};
"""
cases=r"""
int main(void){
    unsigned checks=0;
    for(int lap=1;lap<=6;lap++)for(int gate=0;gate<6;gate++)for(int pi=0;pi<240;pi++){
        int s=kvalidated_progress(lap,pi,gate,240,6,kgate_pi);
        int lo=gate?kgate_pi[gate-1]:kgate_pi[5],hi=gate?kgate_pi[gate]:240;
        assert(s>=lap*240+lo&&s<lap*240+hi);
        if(pi>=lo&&pi<hi)assert(s==lap*240+pi);checks++;
    }
    assert(kvalidated_progress(1,239,1,240,6,kgate_pi)==240);
    assert(kvalidated_progress(1,87,1,240,6,kgate_pi)==279);
    assert(kvalidated_progress(1,2,0,240,6,kgate_pi)==479);
    assert(kvalidated_progress(0,238,0,240,6,kgate_pi)==238);
    assert(kvalidated_progress(0,222,0,240,6,kgate_pi)==222);
    rc[0]=(Racer){1,239,1,0,0,0};
    for(int i=1;i<nrc;i++)rc[i]=(Racer){1,40+i,0,0,0,0};
    kplaces();assert(rc[0].place==12);
    rc[0]=(Racer){1,50,2,0,0,0};kplaces();assert(rc[0].place==2);
    rc[1].fin=1;rc[1].place=1;rc[11].out=1;rc[11].place=12;
    kplaces();assert(rc[1].place==1&&rc[11].place==12);
    for(int fin=0;fin<2;fin++)for(int out=0;out<2;out++)for(int type=0;type<8;type++)for(int elapsed=0;elapsed<2;elapsed++)
        assert(kart_record_eligible(fin,out,type,elapsed)==(fin&&!out&&type!=3&&type!=6&&elapsed));
    printf("Kart progress: %u gate cases, backwards crossing, standings and 64 record-policy cases PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as td:
    p=Path(td);c=p/"test.c";exe=p/"test"
    c.write_text(prefix+"\n".join(fn(k,m) for m in ("static int kvalidated_progress(","static int kscore(","static void kplaces(","static int kart_record_eligible("))+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
