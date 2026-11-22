#!/usr/bin/env python3
"""Exercise space salvage, GBA camera and 3D planet shader production C.

The fixed 4-canister pool now delivers fuel/repair supplies earned in combat
and mining without altering save layouts. The camera's GBA-only 32-bit fast
path must match the former wide math. The moved planet scale division must
leave every shaded screen pixel unchanged for varying discs and clipping.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
space=(ROOT/"space.inc").read_text()
main=(ROOT/"main.c").read_text()

def cfunc(source, signature):
    i=source.index(signature)
    beg=source.index("{",i)
    depth,end=1,beg+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[i:end]

cam=cfunc(space,"static int cam3(")
fill=cfunc(space,"IWRAM static void planet_fill(")
pick=cfunc(space,"static void can_collect(")
color=cfunc(space,"static int can_color(")
isqrt=cfunc(main,"static int isqrt(")
can_type=re.search(r"typedef struct \{ s32 x,y,z; u8 alive, good; \} Can;",space).group()
can_defs=re.search(r"#define CAN_FUEL 253\s*#define CAN_REPAIR 254\s*#define CAN_RANDOM 255",space).group()

assert "#if defined(__arm__) && !defined(__aarch64__)" in cam
assert "rx>=-20000 && rx<=20000" in cam
assert "(long long)rx*br[0]" in cam, "far coordinate fallback missing"
assert "s32 ks=(((2*cs)<<16)/r)*64;" in fill
assert fill.count("s32 ks=")==1
assert "s32 kx=(((c0*2-cx+r)*65536)/r)*64;" in fill, "clipped planet coordinate must be defined"
assert "int ny=(64-ky)*4;" in fill
assert "if(!c->alive) return;" in pick
assert "c->alive=0;" in pick
assert "cargo_cap+ship_hold_bonus()" in pick
assert "can_collect(&cans[k]);" in space, "real flight must use pickup function"
assert "can_color(cans[i].good)" in space, "scanner color omitted"
assert "can_color(cans[k].good)" in space, "3D salvage marker color omitted"
assert "CAN_RANDOM" in space[space.index("static void ship_dies("):space.index("static void space_enter(")]
assert "CAN_FUEL" in space[space.index("if(a->hp<=0){ a->alive=0;"):space.index("space_say(\"ROCK CRACKED!",space.index("if(a->hp<=0){ a->alive=0;"))]

# The old per-row projection scale, with everything else byte-identical.
legacy=fill.replace("    s32 ks=(((2*cs)<<16)/r)*64;","").replace(
    "        s32 kx=(((c0*2-cx+r)*65536)/r)*64;",
    "        s32 kx=(((c0*2-cx+r)*65536)/r)*64, ks=(((2*cs)<<16)/r)*64;")
assert legacy!=fill
legacy=legacy.replace("void planet_fill(","void planet_fill_legacy(",1)

stub=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef int32_t s32;
typedef uint32_t u32;
typedef int64_t s64;
#define IWRAM
static s32 shx,shy,shz,cam_o[3];
static s16 br[3],bu[3],bf[3];
static u8 pl_noise[32768], sph_z[16384], sph_v[16384], sph_u[16384];
static int frames, fuel,shield,hull,cargo_cap,hold_bonus,fu_max=70,sh_max=40;
static u16 cargo[8];
static s64 gold;
static int fx_count;
static char message[100];
static const char *goods[8]={"WOOD","SILK","STONE","SUGAR","CRYSTALS","ANT","RARE","SCRAP"};
static int fuel_max(void){return fu_max;}
static int shield_max(void){return sh_max;}
static int ship_hold_bonus(void){return hold_bonus;}
static int cargo_n(void){int n=0;for(int i=0;i<8;i++)n+=cargo[i];return n;}
static void space_say(const char *s){snprintf(message,sizeof message,"%s",s);}
static void sfx(int n){(void)n;fx_count++;}
static void scpy(char *d,const char *s){strcpy(d,s);}
static void scat(char *d,const char *s){strcat(d,s);}
static void ncat(char *d,int n){ char b[24]; snprintf(b,sizeof b,"%d",n); strcat(d,b); }
static u32 rnd_state=0x6EF271A3u;
static u32 rnd(void){rnd_state=rnd_state*1664525u+1013904223u;return rnd_state;}
static int lr(int n){return (int)(rnd()%((u32)n));}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"Space content/speed at %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
tests=r"""
static void wide(s32 x,s32 y,s32 z,s32 *cx,s32 *cy,s32 *cz){
    s32 rx=x-shx-cam_o[0],ry=y-shy-cam_o[1],rz=z-shz-cam_o[2];
    *cx=(s32)(((long long)rx*br[0]+(long long)ry*br[1]+(long long)rz*br[2])>>14);
    *cy=(s32)(((long long)rx*bu[0]+(long long)ry*bu[1]+(long long)rz*bu[2])>>14);
    *cz=(s32)(((long long)rx*bf[0]+(long long)ry*bf[1]+(long long)rz*bf[2])>>14);
}
static u16 new_pg[120*160], old_pg[120*160];
int main(void){
    // Supply canisters are not consumed twice. Full fuel or armour gives gold.
    Can can={0}; can.alive=1; can.good=CAN_FUEL;
    fuel=55; fu_max=70; gold=0;
    can_collect(&can); CHECK(!can.alive && fuel==70 && gold==0 && fx_count==1);
    CHECK(strstr(message,"FUEL CELL")!=NULL);
    can_collect(&can); CHECK(fuel==70 && fx_count==1);
    can.alive=1; can_collect(&can); CHECK(gold==15 && fx_count==2);
    can.alive=1; can.good=CAN_REPAIR; hull=90;shield=0;
    can_collect(&can); CHECK(hull==100 && shield==0 && gold==15);
    can.alive=1; can_collect(&can); CHECK(shield==20 && hull==100);
    can.alive=1;shield=sh_max;can_collect(&can); CHECK(gold==30);
    // The existing ship-design hold bonus must apply in space.
    memset(cargo,0,sizeof cargo); cargo_cap=4; hold_bonus=2; cargo[0]=5;
    can.alive=1;can.good=3;can_collect(&can);
    CHECK(cargo[3]==1 && cargo_n()==6);
    can.alive=1;can.good=3;can_collect(&can);
    CHECK(cargo_n()==6 && gold==40 && strstr(message,"CARGO FULL")!=NULL);
    can.alive=1;can.good=CAN_RANDOM;cargo[0]--;
    can_collect(&can); CHECK(!can.alive && cargo_n()==6);
    CHECK(can_color(CAN_FUEL)==52 && can_color(CAN_REPAIR)==55);
    CHECK(can_color(CAN_RANDOM)==50 && can_color(4)==50);
    // Test all 16-bit basis coefficients (not just normalized 2.14
    // vectors): the 20,000 fast radius is safe even at -32768.
    unsigned fast=0,far=0;
    for(int i=0;i<280000;i++){
        for(int j=0;j<3;j++){
            br[j]=(s16)(rnd()>>16);bu[j]=(s16)(rnd()>>16);bf[j]=(s16)(rnd()>>16);
        }
        shx=(s32)(rnd()%16000u)-8000;shy=(s32)(rnd()%16000u)-8000;shz=(s32)(rnd()%16000u)-8000;
        for(int j=0;j<3;j++)cam_o[j]=(s32)(rnd()%2001u)-1000;
        int radius=(i&1)?19500:47000;
        s32 x=shx+cam_o[0]+(s32)(rnd()%((u32)radius*2u+1u))-radius;
        s32 y=shy+cam_o[1]+(s32)(rnd()%((u32)radius*2u+1u))-radius;
        s32 z=shz+cam_o[2]+(s32)(rnd()%((u32)radius*2u+1u))-radius;
        s32 cx,cy,cz,ex,ey,ez;
        int ok=cam3(x,y,z,&cx,&cy,&cz);
        wide(x,y,z,&ex,&ey,&ez);
        CHECK(cx==ex && cy==ey && cz==ez && ok==(ez>20));
        if((x-shx-cam_o[0])>=-20000 && (x-shx-cam_o[0])<=20000 &&
           (y-shy-cam_o[1])>=-20000 && (y-shy-cam_o[1])<=20000 &&
           (z-shz-cam_o[2])>=-20000 && (z-shz-cam_o[2])<=20000)fast++;else far++;
    }
    CHECK(fast>100000 && far>50000);
    // Native/handheld shader must render the exact same pixels as legacy
    // across projected radius, camera clipping, LOD and light direction.
    for(int i=0;i<32768;i++)pl_noise[i]=(u8)((rnd()>>19)&15u);
    for(int i=0;i<16384;i++){
        sph_z[i]=(u8)(rnd()>>24);
        sph_v[i]=(u8)(rnd()>>26); sph_u[i]=(u8)(rnd()>>25);
    }
    const int radii[]={2,3,9,32,63,64,65,100,150,151,210,360};
    const int centers[]={-180,-15,0,40,80,120,220,310};
    const int heights[]={-140,0,36,85,127,260};
    int frames_checked=0;
    for(int a=0;a<(int)(sizeof radii/sizeof radii[0]);a++)
    for(int b=0;b<(int)(sizeof centers/sizeof centers[0]);b++)
    for(int c=0;c<(int)(sizeof heights/sizeof heights[0]);c++){
        int radius=radii[a],cx=centers[b],cy=heights[c],rot=(int)(rnd()&127u);
        int Lx=(int)(rnd()%32769u)-16384, Ly=(int)(rnd()%32769u)-16384, Lz=(int)(rnd()%32769u)-16384;
        frames=(int)(rnd()&65535u);
        const u8 *noise=pl_noise+((a&1)?8192:0);
        for(int j=0;j<120*160;j++)new_pg[j]=old_pg[j]=(u16)(j^0x5A5Au);
        planet_fill(new_pg,cx,cy,radius,Lx,Ly,Lz,rot,noise);
        planet_fill_legacy(old_pg,cx,cy,radius,Lx,Ly,Lz,rot,noise);
        CHECK(memcmp(new_pg,old_pg,sizeof new_pg)==0);
        frames_checked++;
    }
    printf("Salvage pickup/full-hold and ARM camera: %u fast %u fallback cases; planet fill %d pixel-exact frames PASS\n",fast,far,frames_checked);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-space-supply-") as tmp:
    t=Path(tmp)/"content_perf.c"
    t.write_text(stub+"\n"+can_defs+"\n"+can_type+"\n"+isqrt+"\n"+pick+
                "\n"+color+"\n"+cam+"\n"+fill+"\n"+legacy+"\n"+tests)
    for flags in (["-DBUGSTAR_CPU_ARM=1"],["-DBUGSTAR_CPU_NATIVE=1"]):
        # Define ARM only after standard library includes; this compiles and
        # runs the ARM-specific C arithmetic on a sanitizer-equipped host.
        source=t.read_text().replace(cam,
            ("#if defined(BUGSTAR_CPU_ARM)\n#define __arm__ 1\n#endif\n"+
             cam+"\n#undef __arm__"))
        t.write_text(source)
        exe=Path(tmp)/"content_perf"
        subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                        "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                        "-fno-omit-frame-pointer",*flags,str(t),"-o",str(exe)],
                       check=True)
        subprocess.run([str(exe)],check=True)
        t.write_text(stub+"\n"+can_defs+"\n"+can_type+"\n"+isqrt+"\n"+pick+
                    "\n"+color+"\n"+cam+"\n"+fill+"\n"+legacy+"\n"+tests)
