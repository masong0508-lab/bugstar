#!/usr/bin/env python3
"""Run the production ship builder and PC design conversion with sanitizers.

The old allocation helpers are retained only as a compatibility oracle: their
capacity exits abort that reference build before an invalid index can be used.
Every design that fits the old builder must have byte-identical active model
data now. Exhausted designs still run through the real builder and independent
index, normal and radius checks. No game renderer or shared build is needed.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ship = (ROOT / "ship.inc").read_text()
main = (ROOT / "main.c").read_text()
assets = (ROOT / "assets.h").read_text()
yard = (ROOT / "pc/shipyard/shipyard_ui.inc").read_text()


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


types = ship[ship.index("typedef struct"):ship.index("// ---- what the parts do")]
model = ship[ship.index("// ---- the model:"):ship.index("// ---- drawing it:")]
presets = ship[ship.index("// ---- ready-made ships"):ship.index("// ---- the numbers the hangar")]
sines = re.search(r"static const short sin_tab\[256\]=\{.*?\n\};", assets, re.S).group()
maths = "\n".join(function(main, "static int " + name + "(")
                  for name in ("isqrt", "sinx", "cosx"))

# The exact pre-fix allocation behavior, with a longjmp instead of continuing
# when a capacity is exhausted. This never runs an out-of-bounds reference.
legacy = {
    "sv_add": "static int sv_add(int x,int y,int z){ if(snv>=SV_MAX) longjmp(ref_full,1); shv[snv*3]=x; shv[snv*3+1]=y; shv[snv*3+2]=z; return snv++; }",
    "se_add": "static void se_add(int a,int b,int c){ if(sne>=SE_MAX) longjmp(ref_full,1); she[sne*3]=a; she[sne*3+1]=b; she[sne*3+2]=c; sne++; }",
    "sf_add": "static void sf_add(int grp,int two,int n,int a,int b,int c,int d,int e,int f,int g,int h){ if(snf>=SF_MAX) longjmp(ref_full,1); Face *F=&shf[snf++]; F->n=n; F->grp=grp; F->two=two; int vv[8]={a,b,c,d,e,f,g,h}; for(int i=0;i<8;i++) F->v[i]=vv[i]; }",
    "ring4": "static int ring4(int w,int h,int z){ int i=snv; sv_add(w,0,z); sv_add(0,h,z); sv_add(-w,0,z); sv_add(0,-h,z); for(int k=0;k<4;k++) se_add(i+k,i+((k+1)&3),0); return i; }",
    "ring8": "static int ring8(int r,int z,int c){ int i=snv; for(int k=0;k<8;k++){ int a=k*32; sv_add((cosx(a)*r)>>14, (sinx(a)*r)>>14, z); } for(int k=0;k<8;k++) se_add(i+k,i+((k+1)&7),c); return i; }",
    "sbox": "static void sbox(int cx,int cy,int cz,int dx,int dy,int dz,int grp){ int p=snv; for(int k=0;k<8;k++) sv_add(cx+((k&1)?dx:-dx),cy+((k&2)?dy:-dy),cz+((k&4)?dz:-dz)); QUAD(grp,0,p+0,p+1,p+3,p+2); QUAD(grp,0,p+4,p+5,p+7,p+6); QUAD(grp,0,p+0,p+1,p+5,p+4); QUAD(grp,0,p+2,p+3,p+7,p+6); QUAD(grp,0,p+0,p+2,p+6,p+4); QUAD(grp,0,p+1,p+3,p+7,p+5); }",
}
reference = model
for name, old in legacy.items():
    signature = "static " + ("int " if name in ("sv_add", "ring4", "ring8") else "void ") + name + "("
    reference = reference.replace(function(reference, signature), old)
identifiers = (
    "Face sh_eng sh_en sh_rad sh_flame shv she snv sne snf shf shn sh_mz sh_mzn "
    "sh_ord sh_ord_n sh_rt sh_rt_ok sh_l sh_r sh_rt_init sv_add se_add sf_add "
    "ring4 ring8 join tube fan room sbox LX WX ship_build_model"
).split()
reference = re.sub(r"\b(" + "|".join(identifiers) + r")\b",
                   lambda match: "ref_" + match.group(), reference)

harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "sy_design.h"
typedef uint8_t u8; typedef int8_t s8; typedef uint16_t u16;
typedef int16_t s16; typedef uint32_t u32; typedef int32_t s32;
#define EW
static int rgb15(int r,int g,int b){ return (r>>3)|((g>>3)<<5)|((b>>3)<<10); }
static void scpy(char *d,const char *s){ strcpy(d,s); }
static jmp_buf ref_full;
static unsigned checked,compared,exhausted,vertices,edges,faces;
static const char *case_name="boundary";
static unsigned case_id;
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"Ship model %s %u at %d: %s\n",case_name,case_id,__LINE__,#x); exit(1); } } while(0)
''' + sines + "\n" + maths + "\n" + types + "\n" + model + r'''
#undef TRI
#undef QUAD
''' + reference + "\n" + presets + "\n" + function(yard, "static void sy_to_look(") + r'''
static uint64_t root64(uint64_t n){
    uint64_t q=(uint64_t)sqrt((double)n);
    while((q+1)*(q+1)<=n) q++;
    while(q*q>n) q--;
    return q;
}
static void indices(void){
    CHECK(snv>=0 && snv<=SV_MAX && sne>=0 && sne<=SE_MAX && snf>=0 && snf<=SF_MAX);
    for(int i=0;i<sne;i++){ CHECK(she[i*3]<snv); CHECK(she[i*3+1]<snv); }
    for(int i=0;i<snf;i++){
        const Face *f=&shf[i]; CHECK(f->n>=3 && f->n<=8);
        for(int k=0;k<f->n;k++) CHECK(f->v[k]<snv);
    }
}
static void compare_original(void){
    if(setjmp(ref_full)){ exhausted++; return; }
    ref_ship_build_model();
    CHECK(snv==ref_snv && sne==ref_sne && snf==ref_snf);
    CHECK(sizeof(Face)==sizeof(ref_Face));
    CHECK(!memcmp(shv,ref_shv,snv*3*sizeof(*shv)));
    CHECK(!memcmp(she,ref_she,sne*3*sizeof(*she)));
    CHECK(!memcmp(shf,ref_shf,snf*sizeof(*shf)));
    CHECK(!memcmp(shn,ref_shn,snf*3*sizeof(*shn)));
    CHECK(sh_en==ref_sh_en && sh_mzn==ref_sh_mzn && sh_rad==ref_sh_rad);
    CHECK(!memcmp(sh_eng,ref_sh_eng,sh_en*sizeof(*sh_eng)));
    CHECK(!memcmp(sh_mz,ref_sh_mz,sh_mzn*sizeof(*sh_mz)));
    CHECK(sh_ord_n==ref_sh_ord_n && sh_ord_n==-1);
    compared++;
}
static void model_check(void){
    indices(); CHECK(snv>0 && snf>0 && sh_en>=1 && sh_en<=4 && sh_mzn>=1 && sh_mzn<=2);
    uint64_t biggest=0;
    for(int i=0;i<snv;i++){
        int64_t x=shv[i*3], y=shv[i*3+1], z=shv[i*3+2];
        uint64_t d=(uint64_t)(x*x+y*y+z*z); if(d>biggest) biggest=d;
    }
    int radius=(int)root64(biggest); if(radius<20) radius=20; CHECK(sh_rad==radius);
    for(int i=0;i<snf;i++){
        const Face *f=&shf[i];
        const s16 *a=&shv[f->v[0]*3], *b=&shv[f->v[1]*3], *c=&shv[f->v[2]*3];
        int64_t u[3],v[3],cross[3];
        for(int k=0;k<3;k++){ u[k]=(int64_t)b[k]-a[k]; v[k]=(int64_t)c[k]-a[k]; }
        cross[0]=u[1]*v[2]-u[2]*v[1]; cross[1]=u[2]*v[0]-u[0]*v[2]; cross[2]=u[0]*v[1]-u[1]*v[0];
        uint64_t q=0; for(int k=0;k<3;k++){ int64_t n=cross[k]>>4; q+=(uint64_t)(n*n); }
        int64_t length=(int64_t)root64(q)*16;
        for(int k=0;k<3;k++) CHECK(shn[i*3+k]==(length ? cross[k]*256/length : 0));
    }
    vertices+=(unsigned)snv; edges+=(unsigned)sne; faces+=(unsigned)snf;
    compare_original(); checked++;
}
static void pool(int n){
    snv=n; sne=snf=0;
    memset(shv,0x35,sizeof shv); memset(she,0x5A,sizeof she); memset(shf,0x63,sizeof shf);
}
static uint64_t pool_hash(void){
    uint64_t h=1469598103934665603ull;
    const void *p[3]={shv,she,shf}; size_t n[3]={sizeof shv,sizeof she,sizeof shf};
    for(int a=0;a<3;a++) for(size_t i=0;i<n[a];i++){ h^=((const u8*)p[a])[i]; h*=1099511628211ull; }
    return h;
}
static void boundaries(void){
    case_name="boundary";
    for(int kind=0;kind<3;kind++){
        int need=kind==0?4:8;
        for(int left=0;left<need;left++){
            case_id=(unsigned)(kind*10+left); pool(SV_MAX-left); uint64_t before=pool_hash();
            int n=snv,r=SV_MAX;
            if(kind==0) r=ring4(9,7,11); else if(kind==1) r=ring8(9,11,1); else sbox(1,2,3,4,5,6,0);
            CHECK(r==SV_MAX && snv==n && sne==0 && snf==0 && pool_hash()==before);
            if(kind<2){
                join(r,0,need,1); tube(r,0,need,0); fan(r,0,need,0,0); fan(0,r,need,0,1);
                CHECK(sne==0 && snf==0 && pool_hash()==before);
            }
        }
        pool(SV_MAX-need); int at=snv;
        if(kind==0) CHECK(ring4(9,7,11)==at); else if(kind==1) CHECK(ring8(9,11,1)==at); else sbox(1,2,3,4,5,6,0);
        CHECK(snv==SV_MAX); indices();
        CHECK(kind==2 ? snf==6 && sne==0 : sne==need && snf==0);
    }
    pool(SV_MAX); uint64_t before=pool_hash(); int missing=sv_add(1,2,3);
    CHECK(missing==SV_MAX && snv==SV_MAX && pool_hash()==before);
    static const int bad[]={-1,SV_MAX,SV_MAX+7,256,65536};
    for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){
        se_add(bad[i],0,1); se_add(0,bad[i],1);
        for(int n=3;n<=8;n++) for(int k=0;k<n;k++){
            int v[8]={0,1,2,3,4,5,6,7}; v[k]=bad[i];
            sf_add(0,0,n,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]);
        }
    }
    sf_add(0,0,2,0,1,2,3,4,5,6,7); sf_add(0,0,9,0,1,2,3,4,5,6,7);
    CHECK(!sne && !snf && pool_hash()==before);
    pool(8); se_add(7,8,1); sf_add(0,0,3,0,7,8,0,0,0,0,0); CHECK(!sne && !snf);
    sne=SE_MAX-1; se_add(0,7,1); CHECK(sne==SE_MAX); before=pool_hash(); se_add(0,1,1); CHECK(sne==SE_MAX && pool_hash()==before);
    snf=SF_MAX-1; sf_add(0,0,3,0,1,2,0,0,0,0,0); CHECK(snf==SF_MAX); before=pool_hash();
    sf_add(0,0,3,0,1,2,0,0,0,0,0); CHECK(snf==SF_MAX && pool_hash()==before);
}
static u32 mix(u32 x){ x^=x<<13; x^=x>>17; x^=x<<5; return x; }
int main(void){
    SyDesign d;
    // The real STARHAMMER conversion was the sanitizer crash in live flight.
    case_name="PC preset"; case_id=9; ship_default(); sy_default(&d); sy_preset(&d,9); sy_to_look(&d); model_check();
    case_name="default"; ship_default(); ship_build_model(); model_check();
    case_name="game preset";
    for(int i=0;i<NSHP;i++){ case_id=(unsigned)i; ship_apply_preset(i); model_check(); }
    case_name="PC preset";
    for(int i=0;i<SY_NPRESET;i++) for(int coil=0;coil<2;coil++){
        case_id=(unsigned)(i*2+coil); ship_default(); shpx.module=coil?MD_COIL:MD_NONE;
        sy_default(&d); sy_preset(&d,i); sy_to_look(&d); model_check();
    }
    case_name="part combination"; case_id=0;
    for(int hull=0;hull<NHULL;hull++) for(int wing=0;wing<NWING;wing++)
    for(int engines=0;engines<4;engines++) for(int pods=0;pods<4;pods++)
    for(int fins=0;fins<3;fins++) for(int cannon=0;cannon<NCANNON;cannon++)
    for(int cockpit=0;cockpit<NCOCK;cockpit++){
        ship_default(); ship.hull=hull; ship.wings=wing; ship.engines=engines;
        ship.pods=pods; ship.fins=fins; ship.cannon=cannon; ship.cockpit=cockpit;
        u32 r=mix(case_id+0x916B2C45u);
        ship.len=r%3; ship.wid=(r/3)%3; ship.nose=(r/9)%3; ship.wsize=(r/27)%3;
        ship.wpos=(r/81)%3; ship.dihed=(r/243)%3; ship.esize=(r/729)%3;
        ship.decal=case_id%NDECAL; shpx.module=(case_id/NDECAL)%NMODULE;
        shpx.topper=(case_id/(NDECAL*NMODULE))%NTOPPER;
        shpx.scheme=case_id%NSCHEME; shpx.finish=(case_id/NSCHEME)%NFINISH;
        ship_build_model(); model_check(); case_id++;
    }
    uint64_t seed=0x2E479A139C4Full;
    case_name="PC generated";
    for(case_id=0;case_id<2048;case_id++){
        ship_default(); sy_default(&d); sy_random(&d,&seed); sy_to_look(&d); model_check();
    }
    boundaries();
    CHECK(compared>1000 && exhausted>1000 && checked==compared+exhausted);
    printf("Ship model capacity: %u production models, %u vertices/%u edges/%u faces; all presets, valid part combinations and boundary failures PASS\n",checked,vertices,edges,faces);
    printf("Ship model compatibility: %u fitting models byte-identical to previous builder; %u exhausted designs safely bounded; ASan/UBSan PASS\n",compared,exhausted);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="bugstar-ship-capacity-") as td:
    temp = Path(td)
    source, program = temp / "capacity.c", temp / "capacity"
    source.write_text(harness)
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
        "-Wno-misleading-indentation", "-Wno-unused-function", "-Wno-unused-variable",
        "-fsanitize=address,undefined,bounds", "-fno-sanitize-recover=all",
        "-I" + str(ROOT / "pc/shipyard"), str(source),
        str(ROOT / "pc/shipyard/sy_design.c"), "-lm", "-o", str(program),
    ], check=True)
    # This builder has no heap allocations. Keep address/bounds checking on,
    # without LeakSanitizer's unsupported ptrace scan in managed workspaces.
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
    subprocess.run([str(program)], check=True, env=env)
