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
assert "kart_gate_in_range(kgate_pi,kt.n,a-9,b+10)" in k
assert "kart_gate_in_range(kgate_pi,kt.n,p-12,p+22)" in k
assert "kart_gate_in_range(kgate_pi,kt.n,p-12,p+12)" in k
assert "kart_race_retire(k_end_t,k_finish_idle,knp,kt.laps)" in k
rescue=fn(k,"static void kart_restore_road(")
assert "nextg=" not in rescue and ".lap=" not in rescue
assert "rc[0].x=ppx; rc[0].y=ppy;" in rescue
assert "kart_gate_contact(" in fn(k,"static int kart_checkpoint_rescue_point(")
prefix=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;typedef int16_t s16;typedef int32_t s32;
#define NRC 12
typedef struct {int lap,pi,nextg,fin,out,place,x,y;} Racer;
static Racer rc[NRC];static int nrc=12,knp=240;
static u16 kgate_pi[6]={0,40,80,120,160,200};
static struct {int n;} kt={6};
static u8 kpx[512],kpy[512];
"""
cases=r"""
/* Original search, retained here as an independent behavior oracle. */
static Racer old_track(int r,Racer k){
    int tx=(k.x>>15)&255,ty=(k.y>>15)&255,best=k.pi,bd=1<<30;
    for(int d=-6;d<=22;d++){
        int i=kwp(k.pi+d+knp),dx=kwrap8(kpx[i]-tx),dy=kwrap8(kpy[i]-ty),dd=dx*dx+dy*dy;
        if(dd<bd){bd=dd;best=i;}
    }
    if(bd>144)for(int i=0;i<knp;i++){
        int dx=kwrap8(kpx[i]-tx),dy=kwrap8(kpy[i]-ty),dd=dx*dx+dy*dy;
        if(dd<bd){bd=dd;best=i;}
    }
    if(r&&best<k.pi-knp/2)k.lap++;
    k.pi=best;return k;
}
static uint32_t rs=0x9e3779b9u;
static unsigned rnd(void){rs^=rs<<13;rs^=rs>>17;rs^=rs<<5;return rs;}
int main(void){
    unsigned checks=0;
    for(int x=-880;x<=880;x+=4)for(int y=-880;y<=880;y+=4){
        int expect=x*x+y*y<864*864;
        assert(kart_gate_contact(x*256,y*256)==expect);
    }
    assert(!kart_gate_contact(INT32_MIN,0));
    assert(!kart_gate_contact(INT32_MAX,0));
    for(int path=1;path<=600;path++)for(int laps=1;laps<=9;laps++){
        int grace=kart_finish_grace(path,laps);
        assert(grace>=900&&grace<=7200);
        if(laps>1)assert(grace>=kart_finish_grace(path,laps-1));
        assert(!kart_race_retire(grace,900,path,laps));
        assert(kart_race_retire(grace+1,0,path,laps));
        assert(kart_race_retire(901,901,path,laps));
        assert(!kart_race_retire(0,0,path,laps));
    }
    for(int a=0;a<240;a++)for(int b=a;b<240;b++){
        int found=0;for(int i=0;i<6;i++)if(kgate_pi[i]>=a&&kgate_pi[i]<=b)found=1;
        assert(kart_gate_in_range(kgate_pi,6,a,b)==found);
    }
    const int lens[]={1,2,7,29,32,127,256,512};
    for(unsigned z=0;z<sizeof(lens)/sizeof(*lens);z++){
        knp=lens[z];
        for(int i=0;i<knp;i++){kpx[i]=rnd()&255;kpy[i]=rnd()&255;}
        for(int j=0;j<2500;j++){
            int r=j&1;
            Racer k={.lap=1,.pi=(int)(rnd()%knp),.x=(int)(rnd()&255)<<15,.y=(int)(rnd()&255)<<15};
            Racer expected=old_track(r,k);rc[r]=k;kpath_track(r);
            assert(rc[r].pi==expected.pi&&rc[r].lap==expected.lap);
        }
    }
    knp=240;
    for(int path=32;path<=256;path*=2)for(int pi=0;pi<path;pi++)for(int gp=0;gp<path;gp++){
        int ahead=(gp-pi+path)%path,past=(pi-gp+path)%path;
        assert(kart_gate_missed(pi,gp,path)==(past>8&&past<path/2));
        int target=kart_driver_target(pi,gp,path);
        assert(target>=0&&target<path);
        assert(target==((ahead<=5||ahead>path/2)?gp:(pi+5)%path));
    }
    assert(!kart_gate_missed(239,0,240));
    assert(kart_gate_missed(12,0,240));
    assert(kart_driver_target(239,0,240)==0);
    assert(kart_driver_target(12,0,240)==0);
    assert(!kart_gate_missed(-1,0,240));
    /* Recovery is bounded by the last cleared gate and never lands at
       or beyond the required next gate, including the finish-line wrap. */
    for(int path=2;path<=40;path++)for(int prev=0;prev<path;prev++)
        for(int gp=0;gp<path;gp++)for(int pi=0;pi<path;pi++){
            int span=(gp-prev+path)%path,offset=(pi-prev+path)%path;
            int result=kart_recovery_limit(pi,prev,gp,path);
            if(!span){assert(result==-1);continue;}
            assert(result>=0&&result<span);
            if(offset<span)assert(result==offset);
            else if(kart_gate_missed(pi,gp,path))assert(result==span-1);
            else assert(result==0);
        }
    assert(kart_recovery_limit(-1,200,0,240)==-1);
    assert(kart_recovery_limit(239,200,0,240)==39);
    assert(kart_recovery_limit(12,200,0,240)==39);
    assert(kart_recovery_limit(6,200,0,240)==0);
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
    printf("Kart progress: %u gate cases, 20,000 path-search parity cases, checkpoint coverage, race grace, gate-safe jumps, backwards crossing, standings and 64 record-policy cases PASS\n",checks);
}
"""
with tempfile.TemporaryDirectory() as td:
    p=Path(td);c=p/"test.c";exe=p/"test"
    c.write_text(prefix+"\n".join(fn(k,m) for m in ("static inline int kwp(","static int kwrap8(","static void kpath_track(","static int kart_gate_in_range(","static int kart_gate_contact(","static int kart_finish_grace(","static int kart_race_retire(","static int kart_gate_missed(","static int kart_driver_target(","static int kart_recovery_limit(","static int kvalidated_progress(","static int kscore(","static void kplaces(","static int kart_record_eligible("))+cases)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
