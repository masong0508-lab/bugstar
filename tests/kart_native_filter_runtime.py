#!/usr/bin/env python3
"""Check native ground filtering against independent texel-area integrals.

The oracle integrates a small square over piecewise-constant source texels;
it does not repeat either sampler's neighbour/weight calculation. Also checks
the actual row footprint at the game-space screen centre across output sizes.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
kart = (root / "pc/pc_kartnat_r.inc").read_text()
hardware = (root / "pc/pc_hw.c").read_text()


def function(source, signature):
    start = source.index(signature)
    depth = 0
    for i in range(source.index("{", start), len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if not depth:
                return source[start:i + 1]
    raise AssertionError("Unbalanced function: " + signature)


samplers = "\n".join((
    function(kart, "static inline int kn_roadmark("),
    function(kart, "static inline uint32_t kn_bilx("),
    function(hardware, "static inline uint32_t nv_bil("),
))
line_type = re.search(r"typedef struct \{[^{}]*\} LineSt;", hardware).group()
filter_type = re.search(r"typedef struct \{[^{}]*\} KnL;", kart).group()
footprint = "\n".join((
    function(kart, "static int kn_gnd("),
    function(kart, "static void kn_tab("),
))

harness = r"""
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(v) do { if(!(v)){ fprintf(stderr,"line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
static uint32_t pg888[2][160][240];
// Impossible RGB values disable the optional marking override for the oracle.
static uint32_t kn_marks[4]={0xFFFFFFFFu,0xFFFFFFFEu,0xFFFFFFFDu,0xFFFFFFFCu};
""" + samplers + "\n" + line_type + "\n" + filter_type + r"""
static LineSt lst[160];
static KnL knl[160];
static double kn_sx,kn_sy;
""" + footprint + r"""
static uint32_t state=0x13579BDFu;
static uint32_t rnd(void){ state^=state<<13; state^=state>>17; state^=state<<5; return state; }
static int bound(int value,int high){ return value<0?0:value>high?high:value; }
static void reference(double u,double v,double inverse,double out[3]){
    // Integrate source-pixel squares under a centred reconstruction box.
    // Minification already has distant taps: each tap reconstructs at <=1.
    if(u<0) u=0; if(v<0) v=0;
    double width=inverse>1?1/inverse:1;
    double x0=u-width/2,x1=u+width/2,y0=v-width/2,y1=v+width/2;
    out[0]=out[1]=out[2]=0;
    for(int y=(int)floor(y0);y<=(int)floor(y1);y++){
        double height=fmin(y1,y+1)-fmax(y0,y); if(height<=0) continue;
        for(int x=(int)floor(x0);x<=(int)floor(x1);x++){
            double length=fmin(x1,x+1)-fmax(x0,x); if(length<=0) continue;
            double area=height*length/(width*width);
            uint32_t rgb=pg888[1][bound(y,159)][bound(x,239)];
            for(int c=0;c<3;c++) out[c]+=((rgb>>(16-c*8))&255)*area;
        }
    }
}
static double max_area_error;
static int within(uint32_t rgb,const double out[3]){
    // Each of two axes truncates its 8-bit weight and then its RGB result:
    // less than one channel level per operation, at most four in total.
    for(int c=0;c<3;c++){
        double error=fabs((int)((rgb>>(16-c*8))&255)-out[c]);
        if(error>max_area_error) max_area_error=error;
        if(error>4.0){
            fprintf(stderr,"area mismatch: %06X expected %.5f %.5f %.5f\n",rgb,out[0],out[1],out[2]); return 0;
        }
    }
    return 1;
}
static uint32_t average(uint32_t a,uint32_t b){
    return ((a>>1)&0x7F7F7Fu)+((b>>1)&0x7F7F7Fu)+((a&b)&0x010101u);
}
int main(void){
    unsigned oracle_checks=0,centres=0,ramps=0,rows=0;
    for(int y=0;y<160;y++) for(int x=0;x<240;x++) pg888[1][y][x]=rnd()&0xFFFFFFu;
    const int inverses[]={24,64,128,199,200,256,512,1024,4096,8192};
    for(int j=0;j<10;j++) for(int y=0;y<160;y++) for(int x=0;x<240;x++){
        int inv=inverses[j]; uint32_t expected=pg888[1][y][x];
        CHECK(kn_bilx(x+.5,y+.5,inv)==expected);
        CHECK(nv_bil(1,x+.5f,y+.5f,inv/256.0f)==expected); centres++;
    }
    // A quarter-texel footprint never samples neighbours in this interior.
    for(int y=1;y<159;y+=7) for(int x=1;x<239;x+=7) for(int a=1;a<4;a++) for(int b=1;b<4;b++){
        CHECK(kn_bilx(x+a*.25,y+b*.25,1024)==pg888[1][y][x]);
        CHECK(nv_bil(1,x+a*.25f,y+b*.25f,4)==pg888[1][y][x]); centres++;
    }
    for(int i=0;i<60000;i++){
        double u=(rnd()%(240u*256u))/256.0,v=(rnd()%(160u*256u))/256.0;
        int inverse=24+(int)(rnd()%8169); double ref[3];
        uint32_t fixed=kn_bilx(u,v,inverse),floating=nv_bil(1,(float)u,(float)v,inverse/256.0f);
        CHECK(fixed==floating); reference(u,v,inverse/256.0,ref);
        CHECK(within(fixed,ref)); oracle_checks++;
    }
    // Fractional coordinates not representable in the integer sampler are
    // still continuous and correct in the fallback floating-point path.
    for(int i=0;i<12000;i++){
        float u=(rnd()%240000u)/1000.0f,v=(rnd()%160000u)/1000.0f;
        float inverse=(24+(int)(rnd()%8169))/256.0f; double ref[3];
        reference(u,v,inverse,ref); CHECK(within(nv_bil(1,u,v,inverse),ref)); oracle_checks++;
    }
    const double edge_x[]={-5,-.5,0,.5,239.5,240,246};
    const double edge_y[]={-5,-.5,0,.5,159.5,160,166};
    for(int j=0;j<10;j++) for(int x=0;x<7;x++) for(int y=0;y<7;y++){
        double ref[3]; reference(edge_x[x],edge_y[y],inverses[j]/256.0,ref);
        CHECK(within(kn_bilx(edge_x[x],edge_y[y],inverses[j]),ref));
        CHECK(within(nv_bil(1,edge_x[x],edge_y[y],inverses[j]/256.0f),ref)); oracle_checks++;
    }
    // A straight boundary must stay monotonic; no backwards jump when the
    // sampler switches neighbours at a texel centre or at the next edge.
    for(int axis=0;axis<2;axis++){
        for(int y=0;y<160;y++) for(int x=0;x<240;x++)
            pg888[1][y][x]=(axis?y>=80:x>=120)?0xFFFFFFu:0;
        for(int j=0;j<10;j++){
            int previous=-1;
            for(int k=-512;k<=512;k++){
                double t=k/256.0,u=axis?120.5:120+t,v=axis?80+t:80.5;
                uint32_t color=kn_bilx(u,v,inverses[j]);
                CHECK(color==nv_bil(1,u,v,inverses[j]/256.0f));
                int value=color&255; CHECK(value>=previous);
                if(k==0) CHECK(value==127);
                previous=value; ramps++;
            }
            CHECK(previous==255);
        }
    }
    // Keep distant marks filtered: clamping the reconstruction inverse must
    // not turn every original footprint into the nearby-mark override.
    for(int y=0;y<160;y++) for(int x=0;x<240;x++) pg888[1][y][x]=x>=120?0xFFFFFFu:0;
    kn_marks[0]=0xFFFFFFu;
    CHECK(kn_bilx(120,80.5,199)==0x7F7F7Fu);
    CHECK(kn_bilx(120,80.5,200)==0xFFFFFFu);
    kn_marks[0]=0xFFFFFFFFu;
    // Existing two distant taps over a two-texel footprint keep a one-texel
    // checker stable across all subtexel phases (one rounding level allowed).
    for(int axis=0;axis<2;axis++){
        for(int y=0;y<160;y++) for(int x=0;x<240;x++)
            pg888[1][y][x]=((axis?y:x)&1)?0xFFFFFFu:0;
        for(int phase=0;phase<512;phase++){
            double u=120+phase/256.0,v=80+phase/256.0;
            uint32_t a=kn_bilx(u-(axis?0:.5),v-(axis?.5:0),256);
            uint32_t b=kn_bilx(u+(axis?0:.5),v+(axis?.5:0),256);
            int c=average(a,b)&255; CHECK(c>=126 && c<=128);
        }
    }
    // Independently construct rows by their centre position and scale. A
    // scale change around that centre must not masquerade as forward motion.
    for(int moving=0;moving<2;moving++) for(int scale=1;scale<=8;scale++) for(int aspect=1;aspect<=2;aspect++){
        kn_sx=1.0/scale; kn_sy=1.0/(scale*aspect);
        for(int row=0;row<160;row++){
            LineSt *l=&lst[row]; l->dc=0x414; l->pa=128+row*8; l->pc=-64+row*4;
            l->rx=40*256+moving*row*512-l->pa*120;
            l->ry=70*256+moving*row*256-l->pc*120;
        }
        kn_tab();
        for(int row=0;row<160;row++){
            double horizontal=hypot(lst[row].pa/256.0,lst[row].pc/256.0)*kn_sx;
            double vertical=moving && row<159?sqrt(5.0)*kn_sy:0;
            double expected=fmax(.0625,fmax(horizontal,vertical));
            CHECK(fabs(knl[row].fp-expected)<1e-10);
            CHECK(fabs(knl[row].ifp-1/expected)<1e-10); rows++;
        }
    }
    printf("Native ground filtering: %u centre/interior checks, %u independent area checks (max RGB rounding error %.3f/255), %u monotonic ramp samples, %u screen-centre footprints, far taps and edge clamps PASS\n",
           centres,oracle_checks,max_area_error,ramps,rows);
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-native-filter-") as td:
    executable = str(Path(td) / "filter")
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-misleading-indentation", "-fsanitize=undefined,bounds",
                    "-fno-sanitize-recover=all", "-x", "c", "-", "-lm", "-o", executable],
                   input=harness, text=True, check=True)
    subprocess.run([executable], check=True)
