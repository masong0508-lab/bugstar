#!/usr/bin/env python3
"""Exercise the shared space movement code with the real ship presets.

Compiles the actual steering, thrust, braking, drag, speed-limit and position
updates under UBSan. Camera basis, ship statistics and the sine table also come
from the game. Rendering, rumble and the inactive Stormclock Coil are stubbed.
The fixture checks released controls, mirrored steering, stopping, asteroid
drift matching in/above the atmosphere, fractional thrust and intentional
unbraked coasting. A separate 64-bit oracle checks the actual thrust expressions
over complete fractional cycles and unsigned logic-clock wrap.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
space = (ROOT / "space.inc").read_text()
ship = (ROOT / "ship.inc").read_text()
main = (ROOT / "main.c").read_text()
game = (ROOT / "game.inc").read_text()
assets = (ROOT / "assets.h").read_text()


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


begin = space.index("    int tr=ship_turn();")
last = "    shx+=svx_; shy+=svy_; shz+=svz_; coil_bounds();"
movement = space[begin:space.index(last, begin) + len(last)]
thrust = function(space, "if(HELD(K_B)){ rum_hum(")
thrust_expressions = [re.search(r"\b" + name + r" \+= ([^;]+);", thrust).group(1)
                      for name in ("svx_", "svy_", "svz_")]
ship_types = ship[ship.index("typedef struct"):ship.index("static const char *const hull_name")]
presets_start = ship.index("typedef struct { const char *nm; u8 f[19];")
presets_end = ship.index("\n};", presets_start) + 3
presets = ship[presets_start:presets_end]
asteroid = re.search(r"typedef struct \{[^\n]*\} Ast;", space).group(0)
ranks = re.search(r"enum \{ SK_SPEED[^\n]*\};", game).group(0)
sines = re.search(r"static const short sin_tab\[256\]=\{.*?\n\};", assets, re.S).group(0)
helpers = [function(main, "static int " + name + "(") for name in ("isqrt", "sinx", "cosx")]
helpers += [function(ship, "static int " + name + "(void){") for name in (
    "ship_mass", "ship_thrust", "ship_top_speed", "ship_turn", "ship_damp", "ship_accel16")]
helpers += [function(ship, "static void " + name + "(") for name in ("ship_default", "ship_apply_preset")]
helpers += [function(space, "static " + signature) for signature in (
    "void basis(", "int cam_roll(", "void apply_roll(", "int rock_fast(", "int rock_rel(")]
# Keep the test runnable against the original source for runtime negative controls.
if "static int sp_ease_step(" in space:
    helpers += [function(space, "static int sp_ease_step(")]
if "static int sp_thrust_step(" in space:
    helpers += [function(space, "static int sp_thrust_step(")]

harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef int8_t s8; typedef uint16_t u16;
typedef int16_t s16; typedef uint32_t u32; typedef int32_t s32;
#define K_RIGHT 16
#define K_LEFT 32
#define K_UP 64
#define K_DOWN 128
#define K_R 256
#define K_L 512
#define K_B 2
#define HELD(k) (keys&(k))
#define PR 3000
#define RGBC(r,g,b) (((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10))
static int rgb15(int r,int g,int b){ return RGBC(r,g,b); }
static void scpy(char *d,const char *s){ strcpy(d,s); }
static void ship_build_model(void){}
static void rum_hum(int level){ (void)level; }
static int xo_fuel(int n){ return n; }
static s32 coil_vmax(s32 v){ return v; }
static void coil_bounds(void){}
static int keys,yaw,pitch,yawv,pitv,roll_,speed,frames,fuel=70,in_lair,chase_cam=1;
static s32 shx,shy,shz,svx_,svy_,svz_;
static s16 bf[3],br[3],bu[3];
static s8 spin_rock=-1;
static int sp_n[3],steps_checked;
''' + ship_types + ranks + "\nstatic u16 ship_rk[NSK];\n" + presets + asteroid + r'''
static Ast ast[30];
''' + sines + "\n" + "\n".join(helpers) + r'''
static void step(int input){ keys=input; frames++; steps_checked++;
''' + movement + r'''
}
static int force_step(int axis,int direction,int push,u32 tick){
    s16 bf[3]={0,0,0}; bf[axis]=(s16)direction;
    int a=push; u32 frames=tick; (void)frames;
    switch(axis){
''' + "\n".join(f"    case {axis}: return {expression};"
                 for axis, expression in enumerate(thrust_expressions)) + r'''
    }
    return 0;
}
static const char *current_ship;
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"space flight (%s), line %d: %s\n",current_ship,__LINE__,#x); return 1; } } while(0)
static void reset(int preset,int air){
    ship_apply_preset(preset); current_ship=ship_pre[preset].nm;
    memset(ship_rk,0,sizeof ship_rk); memset(sp_n,0,sizeof sp_n); memset(ast,0,sizeof ast);
    keys=frames=yaw=pitch=yawv=pitv=roll_=speed=in_lair=0;
    svx_=svy_=svz_=shx=shy=0; shz=air?-6000:-15000; spin_rock=-1; fuel=70;
}
static int angle(int axis){ return axis?pitch:((yaw+512)&1023)-512; }
static int steering(int preset,int axis,int ranked){
    struct { int angle,yawrate,pitchrate,bank; } positive[148];
    int positive_key=axis?K_DOWN:K_RIGHT,negative_key=axis?K_UP:K_LEFT;
    reset(preset,0); ship_rk[SK_TURN]=ranked?40:0;
    for(int tick=0;tick<148;tick++){
        step(tick<8?positive_key:0);
        positive[tick].angle=angle(axis); positive[tick].yawrate=yawv;
        positive[tick].pitchrate=pitv; positive[tick].bank=roll_;
    }
    CHECK(yawv==0 && pitv==0 && roll_==0);
    CHECK(positive[147].angle==positive[117].angle);
    reset(preset,0); ship_rk[SK_TURN]=ranked?40:0;
    for(int tick=0;tick<148;tick++){
        step(tick<8?negative_key:0);
        CHECK(angle(axis)==-positive[tick].angle);
        CHECK(yawv==-positive[tick].yawrate && pitv==-positive[tick].pitchrate);
        CHECK(roll_==-positive[tick].bank);
    }
    CHECK(yawv==0 && pitv==0 && roll_==0);
    return 0;
}
static int brakes(int preset,int air){
    static const int velocities[][3]={{60,0,0},{-60,30,-20},{3,-2,1}};
    for(int v=0;v<3;v++){
        reset(preset,air); svx_=velocities[v][0]; svy_=velocities[v][1]; svz_=velocities[v][2];
        for(int tick=0;tick<120;tick++) step(K_R);
        CHECK(svx_==0 && svy_==0 && svz_==0 && speed==0);
        int x=shx,y=shy,z=shz;
        for(int tick=0;tick<15;tick++) step(K_R);
        CHECK(shx==x && shy==y && shz==z);
    }
    // A stale reference to a destroyed rock must use ordinary braking.
    reset(preset,air); spin_rock=0; ast[0].vx=3; svx_=7;
    for(int tick=0;tick<120;tick++) step(K_R);
    CHECK(svx_==0 && svy_==0 && svz_==0);
    return 0;
}
static int matching(int preset,int air){
    for(int vx=-3;vx<=3;vx++)for(int vy=-1;vy<=1;vy++)for(int vz=-3;vz<=3;vz++){
        reset(preset,air);
        Ast *rock=&ast[0]; rock->alive=1; rock->vx=(s16)vx; rock->vy=(s16)vy; rock->vz=(s16)vz;
        rock->r=150; rock->x=shx+400; rock->y=shy; rock->z=shz;
        if(!rock_fast(rock)) continue;
        spin_rock=0;
        // Different nose directions must not pull an established match away.
        yaw=((vx+vz+6)&3)*256;
        if(vy>0){ svx_=-9; svy_=5; svz_=8; }
        for(int tick=0;tick<36;tick++) step(K_R);
        CHECK(svx_==vx && svy_==vy && svz_==vz);
        CHECK(rock_rel(rock)==0); // comfortably inside the actual <=2 landing rule
        for(int tick=0;tick<8;tick++) step(K_R);
        CHECK(rock_rel(rock)==0);
    }
    return 0;
}
static int coasting(int preset){
    reset(preset,0); svz_=60;
    int z=shz;
    for(int tick=0;tick<30;tick++) step(0);
    CHECK(svx_==0 && svy_==0 && svz_==60 && shz==z+1800);
    // Still respect the ship's existing speed limit under sustained thrust.
    reset(preset,0); in_lair=1;
    for(int tick=0;tick<240;tick++) step(K_B);
    CHECK(speed<=ship_top_speed());
    reset(preset,0); in_lair=1;
    for(int tick=0;tick<240;tick++) step(K_B|K_L);
    CHECK(speed<=ship_top_speed()*5/4);
    // Burn alone supplies no thrust and spends no fuel when B is released.
    reset(preset,0);
    for(int tick=0;tick<47;tick++) step(K_L);
    CHECK(svx_==0 && svy_==0 && svz_==0 && shz==-15000 && fuel==70);
    return 0;
}
static int cardinal_thrust(int preset){
    for(int boost=0;boost<2;boost++)for(int heading=0;heading<4;heading++)for(int phase=0;phase<16;phase++){
        reset(preset,0); yaw=heading*256; frames=phase;
        int expected=ship_accel16()*(boost?2:1);
        for(int tick=0;tick<16;tick++) step(K_B|(boost?K_L:0));
        int x=heading==1?expected:heading==3?-expected:0;
        int z=heading==0?expected:heading==2?-expected:0;
        CHECK(svx_==x && svy_==0 && svz_==z);
        CHECK(fuel==(boost?69:70));
    }
    return 0;
}
static unsigned gcd_(unsigned a,unsigned b){ while(b){ unsigned r=a%b; a=b; b=r; } return a; }
static uint64_t impulse_checks,impulse_cycles;
static int impulse_cycle(int direction,int push,int axis,u32 phase){
    const unsigned unit=1u<<18;
    unsigned magnitude=(unsigned)(direction*push);
    unsigned common=gcd_(magnitude,unit),period=unit/common;
    uint64_t start=(phase-1u)&(unit-1),sum=0;
    for(unsigned tick=0;tick<period;tick++){
        // Independent exact cumulative-impulse oracle, using wide division.
        int expected=(int)(((start+tick+1)*magnitude/unit)-((start+tick)*magnitude/unit));
        int actual=force_step(axis,direction,push,phase+tick);
        CHECK(actual==expected);
        CHECK(force_step(axis,-direction,push,phase+tick)==-actual);
        CHECK(actual>=0 && actual<=(int)((magnitude+unit-1)/unit));
        sum+=(unsigned)actual; impulse_checks++;
    }
    CHECK(sum==magnitude/common);
    impulse_cycles++;
    return 0;
}
static int thrust_oracle(void){
    static const u32 phases[]={0,1,31,0x12345u,0xfffffff7u};
    int directions[3][3]={{16384,0,0},{11585,0,11585},{0,0,0}};
    yaw=128; pitch=96; basis();
    for(int axis=0;axis<3;axis++) directions[2][axis]=bf[axis];
    for(int preset=0;preset<NSHP+2;preset++){
        int push;
        if(preset<NSHP){ reset(preset,0); push=ship_accel16(); }
        else { current_ship=preset==NSHP?"custom minimum push":"custom maximum push"; push=preset==NSHP?4:96; }
        // PC/Android custom shipyard push has the same 4..96 bounds; boost doubles it.
        for(int boost=1;boost<=2;boost++)for(int dir=0;dir<3;dir++)for(int axis=0;axis<3;axis++)
            for(unsigned phase=0;phase<sizeof phases/sizeof phases[0];phase++)
                if(impulse_cycle(directions[dir][axis],push*boost,axis,phases[phase])) return 1;
    }
    printf("Fractional thrust: %llu exact signed impulse checks, %llu complete phase/wrap cycles PASS\n",
           (unsigned long long)impulse_checks,(unsigned long long)impulse_cycles);
    return 0;
}
int main(void){
    ship_default();
    if(thrust_oracle()) return 1;
    for(int preset=0;preset<NSHP;preset++){
        for(int axis=0;axis<2;axis++)for(int ranked=0;ranked<2;ranked++)
            if(steering(preset,axis,ranked)) return 1;
        for(int air=0;air<2;air++) if(brakes(preset,air) || matching(preset,air)) return 1;
        if(coasting(preset) || cardinal_thrust(preset)) return 1;
    }
    printf("Shared space flight: %d real movement steps across %d ship presets with UBSan PASS\n",steps_checked,NSHP);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="bugstar-space-flight-") as temp:
    executable = str(Path(temp) / "flight")
    subprocess.run([
        "cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
        "-Wno-misleading-indentation", "-fsanitize=undefined",
        "-fno-sanitize-recover=all", "-x", "c", "-", "-o", executable,
    ], input=harness, text=True, check=True)
    subprocess.run([executable], check=True)
