#!/usr/bin/env python3
"""Check the real GBA/PC Mode7 depth profile under UndefinedBehaviorSanitizer.

Only hardware storage and camera/world inputs are substituted. The complete
m7_frame and reciprocal tables come from the game. Tests cover depth order,
an independent flat-plane projection oracle at every camera preset, reciprocal
one near the horizon, and a steep segment crossing the reciprocal table limit.
No emulator, generated PC source, or game-memory allocation is needed.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "m7.inc").read_text()
start = source.index("OVK static void m7_frame(void)")
opening = source.index("{", start)
depth = 1
end = opening + 1
while depth:
    depth += (source[end] == "{") - (source[end] == "}")
    end += 1
frame = source[start:end]

harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
typedef int32_t s32;
typedef int16_t s16;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
#define OVK
#define FOCP 120
#define PHZ g_phz
#define HW 256
#define HMASK (HW-1)
#define NSTEP 48
#define M7_HUD 64
#define SKY_B 8
typedef struct { s16 pa,pb,pc,pd; s32 x,y; } M7Line;
static M7Line tabs[2][160];
#define M7_TAB(t) tabs[t]
static u16 m7_dc[2][160],m7_fog[2][160],BG_PAL[256],bg_pal[256];
static struct { s16 before[8],rows[160],after[8]; } guarded;
static s16 *m7_rowz=guarded.rows;
static u8 ybin[960],hfine[HW*HW];
static s32 stp_z[NSTEP],ppx=64<<16,ppy=64<<16,cam_h=1024;
static int m7_tab,m7_cock,cam_fp,kloop_on,m7_wx0,m7_wy0;
static int m7_nwx0,m7_nwy0,m7_arm,m7_horizon,m7_nstrip;
static int hsn,hcs=256,g_hs=3,g_phz=58,ck_=2,cb_=6<<16;
static s32 cam_back(void){ return cb_; }
static int curv_k(void){ return ck_; }
static int curv_sh(void){ return 7; }
static s32 tiny_sub(void){ return ck_>4 ? ((((cb_>>8)*(cb_>>8))>>14)*ck_)>>7 : 0; }
static int fov_k(void){ return 180; }
''' + (ROOT / "recip.inc").read_text() + "\n" + frame + r'''
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"ground depth: line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int frames_checked,rows_checked;
static const int camera_horizon[]={62,58,52,42,32,64};
static const int camera_height[]={640,1024,1536,2304,3584,320};
static const int camera_distance[]={4,6,8,10,13};

static void camera(int preset,int distance){
    cam_fp=preset==5;
    g_phz=camera_horizon[preset];
    cam_h=camera_height[preset];
    cb_=cam_fp?32768:camera_distance[distance]<<16;
    s32 z=cam_fp?192:1152;
    if(!cam_fp){ s32 near=(cb_>>8)-384; if(near<256) near=256; if(near<z) z=near; }
    for(int i=0;i<NSTEP;i++){ stp_z[i]=z; z+=(cam_fp?24:40)+(i*i>>2); }
}
static int frame_check(void){
    for(int i=0;i<8;i++) guarded.before[i]=guarded.after[i]=12345;
    m7_frame();
    for(int i=0;i<8;i++) CHECK(guarded.before[i]==12345 && guarded.after[i]==12345);
    CHECK(m7_horizon>=0 && m7_horizon<=160);
    CHECK(m7_nstrip>=M7_HUD && m7_nstrip<=160);
    for(int row=0;row<160;row++){
        CHECK(m7_rowz[row]>=0 && m7_rowz[row]<=(64<<8));
        if(row<m7_horizon) CHECK(m7_rowz[row]==0);
        else {
            CHECK(m7_rowz[row]>=16);
            // Far-to-near visibility cannot jump farther away toward the feet.
            if(row>m7_horizon && m7_rowz[row]>m7_rowz[row-1]){
                fprintf(stderr,"depth reverses at row %d: %d -> %d\n",row,m7_rowz[row-1],m7_rowz[row]);
                return 1;
            }
        }
        rows_checked++;
    }
    frames_checked++;
    return 0;
}
static int profile_cases(void){
    static const int curvature[]={1,2,3,4,7,12};
    for(int shape=0;shape<3;shape++){
        for(int y=0;y<HW;y++)for(int x=0;x<HW;x++)
            hfine[y*HW+x]=(u8)(shape==0?0:shape==1?(y&31)*2:((y&8)?80:0));
        for(int cam=0;cam<6;cam++)for(int cv=0;cv<6;cv++)for(int offset=0;offset<32;offset++){
            camera(cam,1); ck_=curvature[cv];
            ppy=(64<<16)+(offset<<10);
            if(frame_check()) return 1;
        }
    }
    return 0;
}
static int flat_projection(void){
    memset(hfine,0,sizeof hfine); ck_=0;
    double worst=0;
    for(int cam=0;cam<6;cam++)for(int distance=0;distance<5;distance++){
        camera(cam,distance);
        if(cam==4 && distance==1){
            // Both initial samples lie below the screen at the highest real
            // preset. They must not become the visible bottom row's endpoints.
            CHECK(PHZ+((cam_h*krec(stp_z[0]))>>16)>160);
            CHECK(PHZ+((cam_h*krec(stp_z[0]+20))>>16)>160);
        }
        if(frame_check()) return 1;
        for(int row=m7_nstrip;row<160;row++){
            // Independent pinhole oracle: screen_y = horizon + height*focal/depth.
            // Projection rows are floored before interpolation in the game.
            double projected=PHZ+(double)cam_h*FOCP/m7_rowz[row];
            double error=projected-row;
            if(error>worst) worst=error;
            if(error < -0.1 || error>1.1){
                fprintf(stderr,"flat camera %d distance %d row %d depth %d reprojects to %.5f\n",
                        cam,distance,row,m7_rowz[row],projected);
                return 1;
            }
        }
    }
    printf("Flat-plane camera oracle: maximum row error %.6f\n",worst);
    return 0;
}
static int near_horizon(void){
    camera(1,1); g_phz=64; cam_h=16; ck_=0; g_hs=3;
    memset(hfine,0,sizeof hfine);
    if(frame_check()) return 1;
    // The first projected row is one below the horizon. 2^32 cannot be stored
    // in a u32 reciprocal: wrapping it to zero teleports this row to depth16.
    CHECK(m7_rowz[PHZ+1]==stp_z[0]);
    for(int row=PHZ+2;row<160;row++){
        int expected=cam_h*FOCP/(row-PHZ);
        CHECK(abs(m7_rowz[row]-expected)<=1);
    }
    return 0;
}
static int steep_clipping(void){
    // Put the first two march samples on opposite sides of a height step.
    // Check one/two rows, both sides of the table limit, and much wider clips.
    ppx=64<<16; ppy=(64<<16)+(120<<8); hsn=0; hcs=256;
    static const int cases[][3]={{3,0,1},{3,3,2},{10,250,257},{10,251,258},{12,255,0},{24,255,0},{64,255,0}};
    int wide=0;
    for(int h=0;h<7;h++){
        camera(0,1); ck_=0; g_hs=cases[h][0];
        int height=cases[h][1];
        for(int y=0;y<HW;y++)for(int x=0;x<HW;x++) hfine[y*HW+x]=(u8)(y>=126?height:0);
        int near=stp_z[0],far=near+20;
        int ya=PHZ+((cam_h*krec(near))>>16);
        int yb=PHZ+(((cam_h-height*g_hs)*krec(far))>>16);
        CHECK(ya>0 && ya<160 && yb<ya);
        if(cases[h][2]) CHECK(ya-yb==cases[h][2]);
        if(h>=2) CHECK(yb<0);
        if(ya-yb>=258) wide++;
        if(frame_check()) return 1;
        for(int row=yb<0?0:yb;row<ya;row++){
            // Direct rational segment intersection, independent of reciprocals
            // and accumulated fixed-point steps in the actual profile writer.
            int expected=(int)(((int64_t)far*(ya-yb)+(int64_t)(near-far)*(row-yb))/(ya-yb));
            CHECK(abs(m7_rowz[row]-expected)<=1);
        }
    }
    CHECK(wide==4);
    // A first endpoint at/above the top must still fill only valid rows and
    // retain the near-ground depth order, including the reciprocal-one row.
    for(int above=0;above<2;above++){
        camera(0,1); ck_=0; g_hs=above?12:5;
        int height=above?200:247;
        memset(hfine,height,sizeof hfine);
        int first_y=PHZ+(((cam_h-height*g_hs)*krec(stp_z[0]))>>16);
        CHECK(above ? first_y<0 : first_y==0);
        if(frame_check()) return 1;
        CHECK(m7_horizon==0 && m7_nstrip==M7_HUD);
    }
    return 0;
}
int main(void){
    if(profile_cases() || flat_projection() || near_horizon() || steep_clipping()) return 1;
    printf("Shared kart ground depth: %d frames, %d row checks with UBSan PASS\n",frames_checked,rows_checked);
    return 0;
}
'''
# abs() is used only by the test oracle, never by the extracted game code.
harness = "#include <stdlib.h>\n" + harness

with tempfile.TemporaryDirectory(prefix="bugstar-ground-depth-") as temp:
    executable = str(Path(temp) / "ground-depth")
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Wno-misleading-indentation", "-fsanitize=undefined",
        "-fno-sanitize-recover=all", "-x", "c", "-", "-o", executable,
    ], input=harness, text=True, check=True)
    subprocess.run([executable], check=True)
