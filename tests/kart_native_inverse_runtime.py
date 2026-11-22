#!/usr/bin/env python3
"""Check real native hill inversion against the real sprite forward mapping.

The oracle finds the nearest intersection by bisection of kn_fwd, independently
of the closed-form inverse's forward rasterization. Includes a captured game
profile that previously produced broken rows, plus exact analytic cases.
"""
from pathlib import Path
import subprocess
import tempfile

src = (Path(__file__).resolve().parents[1] / "pc/pc_kartnat_r.inc").read_text()


def function(signature):
    start = src.index(signature)
    depth = 0
    for i in range(src.index("{", start), len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if not depth:
                return src[start:i + 1]
    raise AssertionError("Unbalanced function: " + signature)


assert "kn_inverse_rows(KN.sh,KN.kidx);" in src, "live renderer must use the inverse"
prep = function("static int kn_prep(")
carry = prep.index("if(!kn_gnd(l)) KN.sh[l]=KN.sh[l>g0?l-1:l];")
taper = prep.index("KN.sh[l]=kn_hill_edge(KN.sh[l],l);")
sky = prep.index("for(int l=g0-1;l>=0;l--) KN.sh[l]=KN.sh[g0];")
inverse = prep.index("kn_inverse_rows(KN.sh,KN.kidx);")
assert carry < taper < sky < inverse, "taper must survive row propagation and keep the sky coherent"
helpers = "\n".join(function(sig) for sig in (
    "static double kn_hill_edge(", "static double kn_shi(",
    "static void kn_fwd(", "static void kn_inverse_rows("))

harness = r"""
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct { double sh[160],piv,cs,sn; } KN;
""" + helpers + r"""
static double project(double source){
    double x,y; kn_fwd(120,source,&x,&y); return y;
}
static double nearest_source(double y){
    // Search from nearest to farthest and bracket each forward intersection.
    // Unlike the renderer, the reference never computes a reciprocal slope
    // or scatters source intervals into the destination lookup.
    for(int row=159;row>=-1;row--){
        double lo=row<0?0:row+.5,hi=row==159?159.99:row+1.5;
        double a=project(lo),b=project(hi);
        if(y<fmin(a,b) || y>fmax(a,b)) continue;
        if(a==b) return hi; // a horizontal projected interval: nearest wins
        int increasing=b>a;
        for(int iteration=0;iteration<40;iteration++){
            double mid=(lo+hi)/2;
            if((project(mid)<y)==increasing) lo=mid; else hi=mid;
        }
        return (lo+hi)/2;
    }
    return y<=KN.sh[0]?0:159.99;
}
// Actual native frame 320: seed 4255248857, path length 162, player path
// position 152.277344, elevation 1024, first ground row 69. This profile
// triggered the four-iteration inverse's visible backward row jumps.
static const double captured[160]={
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -2.038526038,-2.038526038,-2.038526038,-2.038526038,-2.038526038,
    -1.220478991,0.508448425,-0.924169196,-1.869458348,-3.714740432,
    -3.085122913,-0.410928923,0.897677315,0.606269112,0.566194087,
    1.35692118,1.900809138,2.197357062,2.028421836,1.300040809,
    0.056512482,-1.541261211,-3.267787768,-4.892864309,-6.231175109,
    -7.171541625,-7.686100173,-7.491881585,-7.807259702,-7.621992151,
    -7.239008096,-7.442174249,-6.771460367,-6.319042782,-5.956483365,
    -6.115166861,-5.728657279,-5.651297116,-5.669435529,-5.718624874,
    -5.906463916,-5.802542154,-6.180125251,-6.039739213,-6.499010165,
    -6.822024935,-6.668548865,-7.111869934,-6.978925299,-7.338012698,
    -7.239210533,-7.479750279,-7.519419266,-7.513469181,-7.449762442,
    -7.496750466,-7.267454419,-7.369156316,-6.975082828,-7.126112739,
    -6.580230388,-6.781407216,-6.094431024,-5.532100412,-5.808340676,
    -4.894269603,-5.204192167,-4.20931709,-4.546354073,-3.475761271,
    -2.705568181,-3.079268539,-1.910685694,-2.29389178,-1.102747142,
    -1.489794698,-0.292837562,-0.678367109,0.508680186,1.320886734,
    0.952344717,2.108739108,1.725682946,2.894896813,3.674915094,
    3.297644226,4.444671489,4.089403621,5.200379984,5.938597406,
    5.601254328,6.656223015,7.385465865,7.036825461,8.089802525,
};
int main(void){
    struct { short before[8],index[4200],after[8]; } table;
    int checks=0,edgechecks=0;
    KN.piv=0;KN.cs=1;KN.sn=0;
    const double edge_shifts[5]={-14,-7.25,0,7.25,14};
    for(int e=0;e<5;e++){
        double shift=edge_shifts[e],previous=fabs(shift);
        for(int row=0;row<160;row++){
            double value=kn_hill_edge(shift,row),magnitude=fabs(value);
            if(!isfinite(value) || magnitude>fabs(shift)+1e-12 ||
               (value>0 && shift<=0) || (value<0 && shift>=0) ||
               (row<=111 && value!=shift) || (row==159 && value!=0) ||
               magnitude>previous+1e-12 ||
               previous-magnitude>fabs(shift)*1.5/48+1e-12){
                fprintf(stderr,"hill edge sign/range/continuity: row %d shift %.3f value %.9f\n",
                        row,shift,value);return 1;
            }
            KN.sh[row]=value;previous=magnitude;edgechecks++;
        }
        if(project(159.99)!=159.99){ fputs("bottom ground row is not anchored\n",stderr);return 1; }
    }
    for(int profile=0;profile<9;profile++){
        for(int row=0;row<160;row++){
            KN.sh[row]=profile==0?0:profile==1?14:profile==2?-14:
                profile==3?fmin(14,fmax(-14,2.0*(row-80))):
                profile==4?14*sin(row*.19):
                profile==5?((row&1)?14:-14):
                profile==6?fmin(14,fmax(-14,80.0-row)):
                profile==7?14*sin(row*.7)*sin(row*.031):captured[row];
        }
        for(int i=0;i<8;i++) table.before[i]=table.after[i]=12345;
        kn_inverse_rows(KN.sh,table.index);
        for(int i=0;i<4200;i++){
            double y=-30+i/16.0;
            int expected=(int)(nearest_source(y)*8),got=table.index[i];
            // Bisection can land on either side of an exact 1/8-row boundary.
            if(got<0 || got>1279 || abs(got-expected)>1){
                fprintf(stderr,"profile %d row %d: %d expected %d\n",profile,i,got,expected);
                return 1;
            }
            if(profile<3){
                double source=y-KN.sh[0];
                if(source<0) source=0; if(source>159.99) source=159.99;
                if(got!=(int)(source*8)){ fputs("flat/constant shift changed\n",stderr);return 1; }
            }
            checks++;
        }
        for(int i=0;i<8;i++) if(table.before[i]!=12345 || table.after[i]!=12345){
            fputs("inverse lookup guard overwritten\n",stderr);return 1;
        }
        // A steep but monotonic hill has F(source)=3*source-161 here.
        // Four old fixed-point iterations return 62, instead of exactly 77.
        if(profile==3 && table.index[1600]!=77*8){
            fputs("steep monotonic road inversion failed\n",stderr);return 1;
        }
    }
    printf("Native hill inverse: %d UBSan lookup checks, %d edge checks, exact flat/steep cases, nearest folds and guards PASS\n",checks,edgechecks);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-native-inverse-") as td:
    exe = str(Path(td) / "inverse")
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Wno-misleading-indentation", "-fsanitize=undefined",
                    "-fno-sanitize-recover=all", "-x", "c", "-", "-lm", "-o", exe],
                   input=harness, text=True, check=True)
    subprocess.run([exe], check=True)
