/* Opt-in PC/Android LAN prototype: a second independently flown ship.
 * The remote ship is a camera-correct world-space wireframe, not a HUD dot.
 * Sparring and the opt-in shared-raider co-op encounter are session-only;
 * campaign rewards, docking, missions and saves remain solo.
 */
#include "pc_net.h"
/* The LAN guest enters near, but not inside, the host's spawn. This offset
 * is applied only in opt-in LAN mode, after the normal space spawn and before
 * the camera basis is calculated; solo and GBA space_enter are unchanged. */
static void pc_mp_arrive(void){
#ifdef PC_SDL
    if(pc_mp_role()==2){
        shx+=192; shz+=1720;
        yaw=pc_mp_coop_mode()?0:512; pitch=0; speed=0;
    }
#endif
}
static void pc_mp_space_draw(volatile unsigned short *pg){
#ifdef PC_SDL
    /* Zero multiplayer work in ordinary single-player flight; never overlay
     * a network sparring ship during cinematic or beast-interior scenes. */
    if(!pc_mp_role() || in_lair || FILM) return;
    PcNetShip mine,other;
    mine.x=shx; mine.y=shy; mine.z=shz;
    mine.world=sys_id(cur_sys); mine.yaw=(unsigned short)(yaw&1023);
    mine.pitch=(short)pitch;
    pc_mp_submit(&mine);
    /* Visual-only smoothing: positions arrive around 20 Hz, draw at ~60 Hz.
     * A large movement is a teleport/reconnect: snap rather than fly through
     * intervening planets. No changes to local movement or hit authority.
     */
    static PcNetShip shown;
    static int shown_valid, hit_flash, have_hp;
    static int previous_remote_hp;
    if(!pc_mp_remote((unsigned)pc_frames(),&other) || other.world!=mine.world){
        shown_valid=0; hit_flash=0; have_hp=0;
        text(pg,5,112,pc_mp_role()==1?"LAN: WAITING FOR P2":"LAN: JOINING P1",50,1);
        return;
    }
    if(!shown_valid || shown.world!=other.world ||
       other.x-shown.x>2048 || other.x-shown.x< -2048 ||
       other.y-shown.y>2048 || other.y-shown.y< -2048 ||
       other.z-shown.z>2048 || other.z-shown.z< -2048){
        shown=other; shown_valid=1;
    }else{
        shown.x+=(other.x-shown.x)/3;
        shown.y+=(other.y-shown.y)/3;
        shown.z+=(other.z-shown.z)/3;
        {int d=((int)other.yaw-(int)shown.yaw+512)&1023;
         d-=512; int step=d/3;
         if(!step && d)step=d>0?1:-1;
         shown.yaw=(unsigned short)((shown.yaw+step)&1023);}
        shown.pitch+=(short)((other.pitch-shown.pitch)/3);
    }
    other=shown;
    if(pc_mp_coop_mode()){
        PcCoop raider;
        if(pc_mp_raider((uint32_t)pc_frames(),&raider)){
            /* A shared in-world patrol ship, not a reticle painted on the
             * screen. Host and guest use the same world-space coordinates. */
            s32 rv[6*3];
            static const s16 rm[6][3]={
                {0,0,135},{-125,0,-45},{125,0,-45},
                {0,55,-50},{0,-35,-50},{0,0,-105}
            };
            static const u8 re[]={
                0,1,0,2,0,3,0,4,1,3,2,3,1,4,2,4,3,5,4,5,1,5,2,5
            };
            for(int k=0;k<6;k++){
                rv[k*3]=raider.x+rm[k][0];
                rv[k*3+1]=raider.y+rm[k][1];
                rv[k*3+2]=raider.z+rm[k][2];
            }
            if(raider.phase==1){
                int ci=raider.flash?49:53;
                wire(pg,rv,6,re,12,ci);
                if(pc_sn.w && pc_sn.msk)so_wire(rv,6,re,12,ci);
                int sx,sy_;s32 range;
                if(sproj(raider.x,raider.y,raider.z,&sx,&sy_,&range) &&
                   range>100 && range<10000 && sx>15 && sx<205 &&
                   sy_>14 && sy_<107)
                    text(pg,sx+7,sy_-8,"RAIDER",ci,1);
            }
            char hud[64];
            snprintf(hud,sizeof hud,"CO-OP RAID HP:%u  WINS:%u",
                     (unsigned)raider.hp,(unsigned)raider.wins);
            text(pg,4,112,hud,50,1);
            if(raider.phase==2)
                text(pg,66,99,"RAIDER DOWN!",54,1);
        }else text(pg,4,112,"CO-OP: SYNCING RAIDER",50,1);
    }else {
        uint8_t hh,gh,wh,wg;
        if(pc_mp_duel(&hh,&gh,&wh,&wg)){
            int remote_hp=(pc_mp_role()==1)?gh:hh;
            if(have_hp && remote_hp<previous_remote_hp)hit_flash=8;
            previous_remote_hp=remote_hp;have_hp=1;
            char status[64];
            snprintf(status,sizeof status,"LAN SPAR P1:%u P2:%u  W:%u-%u",
                     (unsigned)hh,(unsigned)gh,(unsigned)wh,(unsigned)wg);
            text(pg,4,113,status,50,1);
            if(!hh || !gh)text(pg,65,102,hh?"P1 WINS":"P2 WINS",54,1);
        }else have_hp=0;
    }
    if(hit_flash)hit_flash--;
    /* The same 3D projection and camera plane as stations, fighters and rocks:
     * eight transformed vertices, fourteen edges, no heap allocations.
     */
    s32 v[8*3];
    s32 sy=sinx(other.yaw>>2), cy=cosx(other.yaw>>2);
    s32 sp=sinx((other.pitch&1023)>>2), cp=cosx((other.pitch&1023)>>2);
    s32 f[3]={(sy*cp)>>14,sp,(cy*cp)>>14};
    s32 r[3]={cy,0,-sy};
    s32 u[3]={(-sy*sp)>>14,cp,(-cy*sp)>>14};
    /* A small actual three-dimensional interceptor: raised canopy, wings,
     * paired engine pods and a keel. All parts rotate with remote yaw/pitch.
     * The 8-vertex/14-edge budget exists only when a LAN peer is visible. */
    static const s16 model[8][3]={
        {0,0,150}, {-145,-10,-20}, {145,-10,-20}, {0,55,-20},
        {0,0,-105}, {-52,-9,-112}, {52,-9,-112}, {0,-50,-65}
    };
    for(int i=0;i<8;i++)for(int j=0;j<3;j++){
        s32 d=(s32)(((long long)model[i][0]*r[j]+
                      (long long)model[i][1]*u[j]+
                      (long long)model[i][2]*f[j])>>14);
        v[i*3+j]=(j==0?other.x:j==1?other.y:other.z)+d;
    }
    static const u8 edges[]={
        0,1,0,2,0,3,0,7,1,4,2,4,3,4,7,4,
        1,5,2,6,5,4,6,4,5,6,3,7
    };
    wire(pg,v,8,edges,14,hit_flash?49:55);
    /* Reuse the native line layer when available; the GBA-sized fallback
     * is still drawn for CLASSIC and keeps the gameplay camera identical. */
    if(pc_sn.w && pc_sn.msk)so_wire(v,8,edges,14,hit_flash?49:55);
    {int sx,sy_; s32 range;
      if(sproj(other.x,other.y,other.z,&sx,&sy_,&range)){
        dot2(pg,sx,sy_,50);
        /* Range-aware corner guides help identify the remote ship in dense
         * belts. Only four very short segments, never an entire HUD mesh. */
        if(range>100 && range<9500 && sx>16 && sx<224 && sy_>16 && sy_<112){
            int r=(int)(140*FOCS/range)+3;
            if(r<5)r=5; if(r>15)r=15;
            int col=hit_flash?49:50;
            line2(pg,sx-r,sy_-r,sx-r+4,sy_-r,col);
            line2(pg,sx+r-4,sy_-r,sx+r,sy_-r,col);
            line2(pg,sx-r,sy_+r,sx-r+4,sy_+r,col);
            line2(pg,sx+r-4,sy_+r,sx+r,sy_+r,col);
        }
        if(sx>15 && sx<208 && sy_>18 && sy_<115)
            text(pg,sx+5,sy_-6,pc_mp_role()==1?"P2":"P1",50,1);
      }
    }
#else
    (void)pg;
#endif
}
