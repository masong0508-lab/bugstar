#!/usr/bin/env python3
"""Keep native enemy shots attached to the player in the chase camera.

Exercise the production game-to-renderer bridge and the actual ship renderer.
The shared game's projection supplies an independent screen-position oracle;
cockpit and CLASSIC rays retain their established endpoint exactly.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
production = (root / "pc/pc_ships.inc").read_text()
space = (root / "space.inc").read_text()


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
    raise AssertionError("Unterminated function: " + signature)


pirate = re.search(r"typedef struct \{[^{}]*\} Pirate;", space).group()
state = production[production.index("static s16 pcsr_hp["):
                   production.index("static void pc_sr_space(")]

harness = r"""
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shipr.h"
typedef int8_t s8;
typedef uint8_t u8;
enum { SP_FIGHTER, SP_SCOUT, SP_HEAVY };
typedef int16_t s16;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
#define CHECK(x) do { if(!(x)){ fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
#define NPIR 64
#define FOCS 160
#define VCY 64
static s32 shx,shy,shz,cam_o[3];
static s16 br[3],bu[3],bf[3],sun[3]={-8000,11000,-4000};
static int chase_cam,pc_ships_mode,pc_armada_n,battle_on,cur_sys,vbl_count;
static u16 BG_PAL[256];
static u32 sys_id(int i){ return (u32)i; }
static int pc_sy_foe(int i,SrShip *s){ (void)i; (void)s; return 0; }
""" + pirate + "\nstatic Pirate pir[NPIR];\n" + state + r"""
static unsigned beam_count;
static float beam_a[NPIR][3],beam_b[NPIR][3];
static uint32_t beam_color[NPIR];
static float beam_width[NPIR];
static void observe_beam(const float a[3],const float b[3],uint32_t col,float width){
    CHECK(beam_count<NPIR);
    memcpy(beam_a[beam_count],a,3*sizeof(float));
    memcpy(beam_b[beam_count],b,3*sizeof(float));
    beam_color[beam_count]=col; beam_width[beam_count]=width; beam_count++;
    sr_beam(a,b,col,width);
}
""" + function(space, "static int sp_role_size(") + "\n" + function(production, "static uint32_t pc_sr_rgb(") + r"""
#define sr_beam observe_beam
""" + function(production, "static void pc_sr_space(") + r"""
#undef sr_beam
""" + function(space, "static int cam3(") + "\n" + function(space, "static int sproj(") + r"""
static unsigned state_rng=0xB38D129u;
static unsigned random_u(void){ state_rng=1664525u*state_rng+1013904223u; return state_rng; }
static int random_n(int n){ return (int)(random_u()%(unsigned)n); }
static void camera(int yaw,int pitch,int roll,int speed){
    const double angle=6.283185307179586/1024;
    double cy=cos(yaw*angle),sy=sin(yaw*angle),cp=cos(pitch*angle),sp=sin(pitch*angle);
    double cr=cos(roll*angle),sr=sin(roll*angle);
    double r[3]={cy,0,-sy},u[3]={-sy*sp,cp,-cy*sp},f[3]={sy*cp,sp,cy*cp};
    for(int k=0;k<3;k++){
        br[k]=(s16)lround((r[k]*cr-u[k]*sr)*16384);
        bu[k]=(s16)lround((u[k]*cr+r[k]*sr)*16384);
        bf[k]=(s16)lround(f[k]*16384);
        cam_o[k]=0;
    }
    if(chase_cam){
        // Same bounded chase distance/height and look-down as space_frame.
        int dd=880+(speed>80?80:speed)*3,hh=dd/5;
        for(int k=0;k<3;k++) cam_o[k]=(-bf[k]*dd+bu[k]*hh)>>14;
        s32 ts=hh*16384/(dd+2400),tc=(s32)sqrt(16384.0*16384-ts*ts);
        for(int k=0;k<3;k++){
            s32 ff=bf[k],uu=bu[k]; bf[k]=(ff*tc-uu*ts)>>14; bu[k]=(uu*tc+ff*ts)>>14;
        }
    }
}
static void reset_ships(void){
    memset(pir,0,sizeof pir); memset(pcsr_alive,0,sizeof pcsr_alive);
    memset(pcsr_hp,0,sizeof pcsr_hp); memset(pcsr_hit,0,sizeof pcsr_hit); pcsr_nb=0;
    Pirate *p=&pir[0]; p->alive=1; p->hp=p->maxhp=30;
    p->team=1; p->tgt=-1; p->fire_t=4; p->f[2]=-16384;
    // In front of the camera and away from the player's on-screen centre.
    for(int k=0;k<3;k++){
        s32 origin=k==0?shx:k==1?shy:shz;
        s32 coordinate=origin+cam_o[k]+((bf[k]*3000-br[k]*700+bu[k]*400)>>14);
        if(k==0) p->x=coordinate; else if(k==1) p->y=coordinate; else p->z=coordinate;
    }
}
static void submit(void){
    Pirate untouched[NPIR]; memcpy(untouched,pir,sizeof pir);
    beam_count=0; pc_sr_space(NULL);
    CHECK(memcmp(untouched,pir,sizeof pir)==0); // rendering cannot change combat
}
static void screen(const float point[3],double *x,double *y){
    double dx=(double)point[0]-shx-cam_o[0],dy=(double)point[1]-shy-cam_o[1],dz=(double)point[2]-shz-cam_o[2];
    double cx=(dx*br[0]+dy*br[1]+dz*br[2])/16384;
    double cy=(dx*bu[0]+dy*bu[1]+dz*bu[2])/16384;
    double cz=(dx*bf[0]+dy*bf[1]+dz*bf[2])/16384;
    CHECK(cz>60); *x=120+cx*FOCS/cz; *y=VCY-cy*FOCS/cz;
}
static unsigned aim_checks,legacy_checks,picture_checks;
static void aim_case(void){
    reset_ships(); pir[0].slot=(s8)(vbl_count%3); submit(); CHECK(beam_count==1);
    CHECK(beam_a[0][0]==pir[0].x && beam_a[0][1]==pir[0].y && beam_a[0][2]==pir[0].z);
    CHECK(beam_color[0]==pc_sr_rgb(53) && beam_width[0]==1);
    if(chase_cam && pc_ships_mode>0){
        CHECK(beam_b[0][0]==shx && beam_b[0][1]==shy && beam_b[0][2]==shz);
        int x,y; CHECK(sproj(shx,shy,shz,&x,&y,NULL));
        double bx,by; screen(beam_b[0],&bx,&by);
        CHECK(fabs(bx-x)<1.25 && fabs(by-y)<1.25); aim_checks++;
    } else {
        // The old cockpit/CLASSIC visual must be bit-for-bit unchanged,
        // including its eight-frame horizontal spread.
        float expected[3];
        for(int k=0;k<3;k++){
            float eye=(float)((k==0?shx:k==1?shy:shz)+cam_o[k]);
            float f=bf[k]/16384.0f,u=bu[k]/16384.0f,r=br[k]/16384.0f;
            expected[k]=eye+f*400-u*((124-VCY)*400.0f/FOCS)+r*(((vbl_count&7)-4)*400.0f/FOCS);
        }
        CHECK(memcmp(expected,beam_b[0],sizeof expected)==0); legacy_checks++;
    }
}
static void draw(uint32_t *out,int w,int h,const uint8_t *mask,int firing){
    pir[0].fire_t=(s8)firing; submit(); sr_commit(0);
    memset(out,0,(size_t)w*h*sizeof(*out)); sr_render(out,w,h,mask,0);
}
static void actual_picture(int mode,int scale){
    int w=240*scale,h=160*scale; size_t pixels=(size_t)w*h;
    uint32_t *idle=calloc(pixels,sizeof(*idle)),*firing=calloc(pixels,sizeof(*firing));
    CHECK(idle && firing); uint8_t mask[240*160]; memset(mask,1,sizeof mask);
    chase_cam=1; pc_ships_mode=mode; shx=20000; shy=900; shz=-32000;
    camera(0,0,0,0); reset_ships();
    int px,py; CHECK(sproj(shx,shy,shz,&px,&py,NULL));
    CHECK(py>=85 && py<=87); // the player's actual chase position, far above row124
    draw(idle,w,h,mask,0); draw(firing,w,h,mask,4);
    int changed=0;
    for(int y=(py-2)*scale;y<(py+3)*scale;y++) for(int x=(px-2)*scale;x<(px+3)*scale;x++)
        if(idle[(size_t)y*w+x]!=firing[(size_t)y*w+x]) changed++;
    CHECK(changed>0); // real rasterized beam visibly reaches the player's hull
    for(int y=128*scale;y<h;y++) for(int x=0;x<w;x++) CHECK(firing[(size_t)y*w+x]==0);
    // Preserve the compositor's protected HUD/foreground pixels.
    for(int y=py-3;y<=py+3;y++) for(int x=px-3;x<=px+3;x++) mask[y*240+x]=0;
    draw(firing,w,h,mask,4);
    for(int y=(py-3)*scale;y<(py+4)*scale;y++) for(int x=(px-3)*scale;x<(px+4)*scale;x++)
        CHECK(firing[(size_t)y*w+x]==0);
    free(idle); free(firing); picture_checks++;
}
int main(void){
    BG_PAL[52]=(u16)(14|(31<<5)|(14<<10)); BG_PAL[53]=(u16)(31|(10<<5)|(8<<10));
    for(int mode=0;mode<=2;mode++) for(int chase=0;chase<=1;chase++) for(int i=0;i<2000;i++){
        pc_ships_mode=mode; chase_cam=chase; vbl_count=i;
        shx=random_n(200001)-100000; shy=random_n(40001)-20000; shz=random_n(200001)-100000;
        camera(random_n(1024),random_n(321)-160,random_n(193)-96,random_n(121)); aim_case();
    }
    // Ship-to-ship shots retain their exact existing target; dead targets,
    // idle ships and ships with no combat target cannot add stray beams.
    chase_cam=1; pc_ships_mode=2; shx=0; shy=0; shz=0; camera(0,0,0,0); reset_ships();
    pir[1]=pir[0]; pir[1].x=700; pir[1].y=100; pir[1].z=2500;
    pir[1].team=0; pir[1].tgt=-2; pir[1].fire_t=0; pir[0].tgt=1;
    submit(); CHECK(beam_count==1);
    CHECK(beam_b[0][0]==700 && beam_b[0][1]==100 && beam_b[0][2]==2500);
    pir[1].alive=0; submit(); CHECK(beam_count==0);
    pir[0].tgt=-2; submit(); CHECK(beam_count==0);
    pir[0].tgt=-1; pir[0].fire_t=0; submit(); CHECK(beam_count==0);
    pir[0].alive=0; pir[0].fire_t=4; submit(); CHECK(beam_count==0);
    for(int mode=1;mode<=2;mode++) for(int scale=1;scale<=4;scale*=2) actual_picture(mode,scale);
    printf("Native space beams: %u chase aim checks, %u unchanged cockpit/CLASSIC rays, "
           "%u render configurations, target validity and HUD occlusion PASS\n",aim_checks,legacy_checks,picture_checks);
    return 0;
}
"""

compiler = shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise RuntimeError("Native space beam regression requires a host C compiler")
with tempfile.TemporaryDirectory(prefix="bugstar-space-beam-") as tmp:
    source = Path(tmp) / "beams.c"
    binary = Path(tmp) / "beams"
    source.write_text(harness)
    subprocess.run(
        [compiler, "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-O1", "-Wall", "-Wextra",
         "-Wno-misleading-indentation", "-fsanitize=undefined,bounds", "-fno-sanitize-recover=all",
         "-I", str(root / "pc/shipr"), str(source), str(root / "pc/shipr/shipr.c"),
         "-lm", "-o", str(binary)], check=True,
    )
    subprocess.run([str(binary)], check=True)
