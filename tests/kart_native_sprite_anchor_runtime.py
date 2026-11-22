#!/usr/bin/env python3
"""Exercise real native sprite composition with common kart ground anchors.

The previous per-sprite-centre warp moved the chassis and driver by different
amounts on hills. This harness compiles the actual registration, projection,
and OAM compositor, then checks pixels as well as projection invariants.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
renderer = (root / "pc/pc_kartnat_r.inc").read_text()
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


helpers = "\n".join(function(renderer, signature) for signature in (
    "static double kn_shi(", "static void kn_fwd(",
    "void pc_kn_sprites_clear(", "void pc_kn_sprites_flush(", "void pc_kn_sprites(",
    "static void kn_sprite_shift("))
compose = function(hardware, "static void nv_sprites(")
assert "kn_sprite_shift(i," in compose, "live OAM composition must use anchors"

harness = r"""
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do { if(!(v)){ fprintf(stderr,"line %d: %s\n",__LINE__,#v); exit(1); } } while(0)
static struct {
    double sh[160],piv,cs,sn;
    int g0;
    unsigned before;
    short sprx[128],spry[128];
    unsigned after;
} KN;
static struct { unsigned before; short x[128],y[128]; unsigned after; } kn_sprites_pending;
static int kn_on;
""" + helpers + r"""
#define OAMB 0
static uint16_t oam[512];
#define R16(a) oam[(a)/2]
typedef struct { int W, H, one_d, sprsm; double sx, sy; } SpCtx;
static uint16_t sp_tex[128][64*64], nv_oc[480*320];
static uint8_t sp_ok[128], nv_op[480*320];
""" + compose + r"""
typedef struct { int x0,y0,x1,y1,n; } Bounds;
static Bounds pixels(uint16_t color){
    Bounds b={480,320,-1,-1,0};
    for(int y=0;y<320;y++) for(int x=0;x<480;x++) if(nv_oc[y*480+x]==color){
        if(x<b.x0) b.x0=x; if(x>b.x1) b.x1=x;
        if(y<b.y0) b.y0=y; if(y>b.y1) b.y1=y;
        b.n++;
    }
    return b;
}
static void scene(int lift,int affine){
    memset(oam,0,sizeof oam); memset(sp_ok,0,sizeof sp_ok);
    for(int i=0;i<128;i++) oam[i*4]=0x200;
    // Player kart: 16x16 chassis, 8x16 driver, and a fixed 8x8 HUD icon.
    oam[0]=104-lift; oam[1]=112|(1<<14);
    oam[4]=(88-lift)|(2<<14); oam[5]=116;
    oam[8]=8; oam[9]=8;
    if(affine){
        // Real karts use double-size affine OAM. Identity matrices give the
        // same visible contact with a larger transparent bounding box.
        oam[0]=(96-lift)|0x300; oam[1]=104|(1<<14);
        oam[4]=(80-lift)|0x300|(2<<14); oam[5]=112;
        oam[3]=oam[15]=256; oam[7]=oam[11]=0;
    }
    for(int i=0;i<3;i++){
        sp_ok[i]=1;
        for(int j=0;j<64*64;j++) sp_tex[i][j]=(uint16_t)(0x8000u+10+i);
    }
    pc_kn_sprites_clear(); pc_kn_sprites(0,2,120,120); pc_kn_sprites_flush();
}
static void draw(void){
    memset(nv_oc,0,sizeof nv_oc); memset(nv_op,4,sizeof nv_op);
    SpCtx c={480,320,1,0,.5,.5}; nv_sprites(&c,0,320);
}
static int near(double a,double b){ return fabs(a-b)<1e-9; }
int main(void){
    int checks=0;
    KN.before=0x12345678u; KN.after=0x90ABCDEFu;
    kn_sprites_pending.before=0x456789ABu; kn_sprites_pending.after=0xABCDEF12u;
    pc_kn_sprites_clear(); pc_kn_sprites_flush();
    for(int i=0;i<128;i++) CHECK(KN.spry[i]==-32768);
    pc_kn_sprites(-100,1000,-127,159);
    pc_kn_sprites_flush();
    for(int i=0;i<128;i++) CHECK(KN.sprx[i]==-127 && KN.spry[i]==159);
    pc_kn_sprites(INT_MIN,INT_MIN,5,5); pc_kn_sprites(INT_MAX,INT_MAX,5,5);
    pc_kn_sprites(0,128,INT_MAX,1); pc_kn_sprites(0,128,1,INT_MIN);
    pc_kn_sprites_flush();
    for(int i=0;i<128;i++) CHECK(KN.sprx[i]==-127 && KN.spry[i]==159);
    CHECK(KN.before==0x12345678u && KN.after==0x90ABCDEFu);
    CHECK(kn_sprites_pending.before==0x456789ABu && kn_sprites_pending.after==0xABCDEF12u);
    KN.g0=70; KN.piv=70; KN.cs=1; KN.sn=0; kn_on=1;
    for(int profile=0;profile<7;profile++) for(int bank=-1;bank<=1;bank++){
        KN.cs=cos(bank*.09); KN.sn=sin(bank*.09);
        for(int row=0;row<160;row++) KN.sh[row]=profile==0?0:
            profile==1?14:profile==2?-14:profile==3?fmin(14,fmax(-14,.5*(row-100))):
            profile==4?14*sin(row*.2):profile==5?((row&1)?14:-14):14*cos(row*.07);
        for(int y=72;y<=150;y+=3) for(int x=20;x<=220;x+=10){
            double ax,ay,dx,dy,otherx,othery;
            pc_kn_sprites_clear(); pc_kn_sprites(2,7,x,y);
            pc_kn_sprites_flush();
            kn_fwd(x,y,&ax,&ay);
            kn_sprite_shift(2,x,y-8,&dx,&dy);
            CHECK(near(x+dx,ax) && near(y+dy,ay));
            // Driver, roof, cloud and airborne chassis keep their relative
            // offsets, including a part whose centre is above the horizon.
            for(int part=3;part<7;part++){
                kn_sprite_shift(part,x+part-4,y-8-part*9,&otherx,&othery);
                CHECK(near(otherx,dx) && near(othery,dy)); checks++;
            }
            // Native hill effects do not alter the game's physical lift.
            for(int lift=0;lift<=40;lift+=5){
                kn_sprite_shift(2,x,y-8-lift,&otherx,&othery);
                CHECK(near(ay-(y-lift+othery),lift)); checks++;
            }
        }
        for(int affine=0;affine<2;affine++){
            scene(0,affine); draw();
            Bounds chassis=pixels(10),driver=pixels(11),hud=pixels(12);
            CHECK(chassis.n==32*32 && driver.n==16*32 && hud.n==16*16);
            CHECK(chassis.y0==driver.y1+1 && driver.x0-chassis.x0==8);
            CHECK(hud.x0==16 && hud.y0==16);
            // The game constructs shadow OAM before vsync displays the old
            // hardware OAM. Clearing/building new contacts must not move
            // those old sprites until oam_flush publishes the new pair.
            pc_kn_sprites_clear(); pc_kn_sprites(0,2,20,75); draw();
            Bounds waiting=pixels(10),waitingdriver=pixels(11);
            CHECK(waiting.x0==chassis.x0 && waiting.y0==chassis.y0);
            CHECK(waitingdriver.x0==driver.x0 && waitingdriver.y0==driver.y0);
            double oldx,oldy,nextx,nexty;
            kn_sprite_shift(0,120,112,&oldx,&oldy);
            pc_kn_sprites_flush(); kn_sprite_shift(0,120,112,&nextx,&nexty);
            double projectedx,projectedy; kn_fwd(20,75,&projectedx,&projectedy);
            CHECK(near(nextx,projectedx-20) && near(nexty,projectedy-75));
            pc_kn_sprites_clear(); kn_sprite_shift(0,120,112,&oldx,&oldy);
            CHECK(near(oldx,nextx) && near(oldy,nexty));
            pc_kn_sprites_flush(); CHECK(KN.spry[0]==-32768);
            scene(20,affine); draw();
            Bounds air=pixels(10),airdriver=pixels(11);
            CHECK(air.x0==chassis.x0 && air.y0==chassis.y0-40);
            CHECK(airdriver.y0==driver.y0-40 && air.y0==airdriver.y1+1);
        }
    }
    // CLASSIC/disabled presentation is byte-aligned with original OAM.
    kn_on=0; scene(0,1); draw();
    Bounds base=pixels(10); CHECK(base.x0==224 && base.y0==208);
    double dx=99,dy=99; kn_sprite_shift(0,120,112,&dx,&dy);
    CHECK(dx==0 && dy==0);
    // Clearing OAM prevents a previous kart's anchors from being reused.
    kn_on=1; pc_kn_sprites_clear(); pc_kn_sprites_flush();
    kn_sprite_shift(0,8,8,&dx,&dy); CHECK(dx==0 && dy==0);
    CHECK(KN.before==0x12345678u && KN.after==0x90ABCDEFu);
    CHECK(kn_sprites_pending.before==0x456789ABu && kn_sprites_pending.after==0xABCDEF12u);
    printf("Native kart anchors: %d projection checks, 126 real OAM scenes, affine karts, jumps, pending/live frame isolation, HUD and bounds PASS\n",checks);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-native-sprite-anchor-") as td:
    executable = str(Path(td) / "anchor")
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Wno-misleading-indentation", "-Wno-unused-variable",
                    "-fsanitize=undefined,bounds", "-fno-sanitize-recover=all",
                    "-x", "c", "-", "-lm", "-o", executable],
                   input=harness, text=True, check=True)
    subprocess.run([executable], check=True)
