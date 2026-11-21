/* Opt-in PC/Android LAN prototype: a second independently flown ship.
 * The remote ship is a camera-correct world-space wireframe, not a HUD dot.
 * It is presence only: shots, rewards, docking and saves remain solo until
 * host-authoritative multiplayer events are added and verified.
 */
#include "pc_net.h"
static void pc_mp_space_draw(volatile unsigned short *pg){
#ifdef PC_SDL
    PcNetShip mine,other;
    mine.x=shx; mine.y=shy; mine.z=shz;
    mine.world=sys_id(cur_sys); mine.yaw=(unsigned short)(yaw&1023);
    mine.pitch=(short)pitch;
    pc_mp_submit(&mine);
    if(!pc_mp_remote((unsigned)pc_frames(),&other) || other.world!=mine.world)return;
    /* The same 3D projection and camera plane as stations, fighters and rocks:
     * five transformed vertices, eight edges, no heap allocations.
     */
    s32 v[5*3];
    s32 sy=sinx(other.yaw>>2), cy=cosx(other.yaw>>2);
    s32 sp=sinx((other.pitch&1023)>>2), cp=cosx((other.pitch&1023)>>2);
    s32 f[3]={(sy*cp)>>14,sp,(cy*cp)>>14};
    s32 r[3]={cy,0,-sy};
    s32 u[3]={(-sy*sp)>>14,cp,(-cy*sp)>>14};
    static const s16 model[5][3]={
        {0,0,125},{-105,0,-35},{105,0,-35},{0,50,-45},{0,0,-75}
    };
    for(int i=0;i<5;i++)for(int j=0;j<3;j++){
        s32 d=(s32)(((long long)model[i][0]*r[j]+
                      (long long)model[i][1]*u[j]+
                      (long long)model[i][2]*f[j])>>14);
        v[i*3+j]=(j==0?other.x:j==1?other.y:other.z)+d;
    }
    static const u8 edges[]={0,1,0,2,1,4,2,4,0,3,1,3,2,3,3,4};
    wire(pg,v,5,edges,8,55);
    {int sx,sy_; if(sproj(other.x,other.y,other.z,&sx,&sy_,0)){
        dot2(pg,sx,sy_,50);
        /* A compact ID cue; do not cover the aiming reticle in the middle. */
        if(sx>15 && sx<208 && sy_>18 && sy_<115)
            text(pg,sx+5,sy_-6,"P2",50,1);
    }}
#else
    (void)pg;
#endif
}
