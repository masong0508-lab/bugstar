/* Opt-in PC/Android LAN prototype: a second independently flown ship.
 * The remote ship is a camera-correct world-space wireframe, not a HUD dot.
 * Sparring and the opt-in shared-raider co-op encounter are session-only;
 * campaign rewards, docking, missions and saves remain solo.
 */
#include "pc_net.h"
#include <math.h>
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
    /* Test-only stationary tracking pilot: exercises real SDL, laser input,
     * three enemy waves and independent UDP sockets without game cheats
     * or campaign mutations. Normal gameplay has zero autopilot work. */
    { static int pilot=-1;
      if(pilot<0)pilot=(getenv("BUGSTAR_LAN_TEST_PILOT")!=0);
      if(pilot && pc_mp_coop_mode()){
          PcCoop c; speed=0;
          if(pc_mp_raider((uint32_t)pc_frames(),&c) && c.phase==1){
              s32 tx=c.hp?c.x:c.wing_x,ty=c.hp?c.y:c.wing_y,
                  tz=c.hp?c.z:c.wing_z;
              double dx=(double)tx-shx,dy=(double)ty-shy,dz=(double)tz-shz;
              double h=sqrt(dx*dx+dz*dz);
              yaw=((int)lrint(atan2(dx,dz)*1024.0/6.2831853071795864769))&1023;
              pitch=(int)lrint(atan2(dy,h)*1024.0/6.2831853071795864769);
          }
      }
    }
    /* Draw the mission result last, above the partner ship model. */
    int result_banner=0;
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
        text(pg,5,112,pc_mp_reconnecting()?"CONNECTION LOST - OPEN LAN MENU":
             pc_mp_role()==1?"WAITING FOR PLAYER 2":"CONNECTING TO PLAYER 1",50,1);
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
    if(!pc_mp_match_ready()){
        text(pg,5,102,pc_mp_local_ready()?"YOU ARE READY":"OPEN LAN MENU AND CONFIRM READY",53,1);
        text(pg,5,112,pc_mp_peer_ready()?"OTHER PLAYER IS READY":"WAITING FOR OTHER PLAYER",50,1);
    }
    if(pc_mp_coop_mode() && pc_mp_match_ready()){
        PcCoop raider;
        if(pc_mp_raider((uint32_t)pc_frames(),&raider)){
            /* A readable 3D hostile interceptor: delta-wing tips, raised
             * cockpit, keel and two engine pods. This fixed-size mesh is
             * only drawn with LAN co-op enabled; solo/handheld paths cost 0. */
            s32 rv[12*3];
            static const s16 rm[12][3]={
                {0,10,260}, {-175,0,60}, {175,0,60},
                {-310,-25,-135}, {310,-25,-135},
                {-140,0,-210}, {140,0,-210},
                {0,115,5}, {0,-90,-30},
                {-115,-35,-225}, {115,-35,-225}, {0,45,-250}
            };
            static const u8 re[]={
                0,1,0,2,1,3,2,4,3,5,4,6,5,6,0,7,
                7,11,7,1,7,2,1,8,2,8,8,9,8,10,5,9,
                6,10,9,10,3,9,4,10,11,9,11,10,5,11,6,11
            };
            static const u8 canopy[]={0,7,7,11,1,7,2,7};
            for(int k=0;k<12;k++){
                rv[k*3]=raider.x+rm[k][0];
                rv[k*3+1]=raider.y+rm[k][1];
                rv[k*3+2]=raider.z+rm[k][2];
            }
            if(raider.phase==1 && raider.hp){
                int ci=raider.flash?49:53;
                wire(pg,rv,12,re,24,ci);
                wire(pg,rv,12,canopy,4,55);
                if(pc_sn.w && pc_sn.msk){
                    so_wire(rv,12,re,24,ci);
                    so_wire(rv,12,canopy,4,55);
                }
                int sx,sy_;s32 range;
                if(sproj(raider.x,raider.y,raider.z,&sx,&sy_,&range) &&
                   range>100 && range<10000 && sx>18 && sx<195 &&
                   sy_>14 && sy_<100)
                    text(pg,sx+8,sy_-9,"RAIDER",ci,1);
            }
            /* The second enemy flies its own flank path, not a HUD
             * duplicate. Reuse the same 3D vertices and perspective at
             * three-quarter scale, with no textures or per-frame malloc. */
            if(raider.wing_hp && raider.phase==1){
                s32 wing[12*3];
                for(int k=0;k<12;k++){
                    wing[k*3]=raider.wing_x+rm[k][0]*3/4;
                    wing[k*3+1]=raider.wing_y+rm[k][1]*3/4;
                    wing[k*3+2]=raider.wing_z+rm[k][2]*3/4;
                }
                wire(pg,wing,12,re,24,49);
                if(pc_sn.w && pc_sn.msk)so_wire(wing,12,re,24,49);
                int wx,wy;s32 wrange;
                if(sproj(raider.wing_x,raider.wing_y,raider.wing_z,
                         &wx,&wy,&wrange) && wrange>100 &&
                   wrange<10000 && wx>20 && wx<204 && wy>10 && wy<100)
                    text(pg,wx+8,wy-10,"DRONE",49,1);
            }
            /* Draw an actual pre-shot lock line so the marked player
             * can dodge laterally before the host resolves the strike. */
            if(raider.charge && raider.phase==1){
                PcNetShip *marked=(pc_mp_role()==(raider.charge_target?2:1))
                                  ? &mine : &other;
                int ex,ey,tx,ty;
                s32 ax=raider.charge_source?raider.wing_x:raider.x;
                s32 ay=raider.charge_source?raider.wing_y:raider.y;
                s32 az=raider.charge_source?raider.wing_z:raider.z;
                if(sproj(ax,ay,az,&ex,&ey,0) &&
                   sproj(marked->x,marked->y,marked->z,&tx,&ty,0)){
                    int ci=(raider.charge&2)?49:53;
                    line2(pg,ex,ey,tx,ty,ci);
                    if(pc_sn.w && pc_sn.msk)so_line(ex,ey,tx,ty,ci);
                }
                text(pg,5,103,raider.charge_target
                     ?"RAIDER LOCK P2 - DODGE!":"RAIDER LOCK P1 - DODGE!",53,1);
            }
            /* The enemy's short real 3D laser is owned and approved by
             * the host. Render the same beam in either player's camera. */
            if(raider.beam && raider.phase==1){
                PcNetShip *aimed=(pc_mp_role()==(raider.beam_target?2:1))
                                 ? &mine : &other;
                int ex,ey,tx,ty;
                s32 ax=raider.beam_source?raider.wing_x:raider.x;
                s32 ay=raider.beam_source?raider.wing_y:raider.y;
                s32 az=raider.beam_source?raider.wing_z:raider.z;
                if(sproj(ax,ay,az,&ex,&ey,0) &&
                   sproj(aimed->x,aimed->y,aimed->z,&tx,&ty,0)){
                    line2(pg,ex,ey,tx,ty,53);
                    if(pc_sn.w && pc_sn.msk)so_line(ex,ey,tx,ty,53);
                }
            }
            /* Compact three-row mission HUD, genuine 240x160 game pixels.
             * These are encounter HEALTH values, NOT the native ship shields.
             * 'H' was successful laser hits, not objective progress; 'CLEAR'
             * was completed mission count, not waves won. Keep them distinct.
             * No dynamic allocations, bitmap assets or extra framebuffers. */
            rect(pg,3,98,234,29,0);
            rect(pg,3,98,234,1,53); /* red enemy/mission border */
            rect(pg,4,108,232,1,56); /* separate boss, pilot, and support rows */
            rect(pg,4,117,232,1,56);
            rect(pg,119,100,1,26,56); /* two stable readable columns */
            char hud[72];
            if(raider.phase==4){
                text(pg,7,101,"RAIDER DEFEATED",54,1);
                text(pg,126,101,"ALL 3 WAVES CLEARED",54,1);
            }else{
                text(pg,7,101,raider.hp?"RAIDER HEALTH":"RAIDER DEFEATED",53,1);
                rect(pg,75,103,38,5,3);
                if(raider.hp)rect(pg,76,104,(int)raider.hp*36/100,3,
                                 raider.hp<=20?49:54);
                snprintf(hud,sizeof hud,"WAVE %u OF 3",
                         (unsigned)raider.wave+1);
                text(pg,145,101,hud,50,1);
            }
            snprintf(hud,sizeof hud,"P1 HEALTH: %u",(unsigned)raider.host_hp);
            text(pg,7,110,hud,raider.host_hp<=20?49:54,1); /* P1 gold */
            snprintf(hud,sizeof hud,"P2 HEALTH: %u",(unsigned)raider.guest_hp);
            text(pg,126,110,hud,raider.guest_hp<=20?49:50,1); /* P2 cyan */
            if(raider.phase==4)
                snprintf(hud,sizeof hud,"MISSIONS WON: %u",
                         (unsigned)raider.completed);
            else if(!raider.wave)
                snprintf(hud,sizeof hud,"DRONE: NEXT WAVE");
            else if(!raider.wing_hp)
                snprintf(hud,sizeof hud,"DRONE: DEFEATED");
            else
                snprintf(hud,sizeof hud,"DRONE HEALTH: %u",
                         (unsigned)raider.wing_hp);
            text(pg,7,119,hud,50,1);
            snprintf(hud,sizeof hud,"LASER HITS: P1 %u P2 %u",
                     (unsigned)raider.host_hits,(unsigned)raider.guest_hits);
            text(pg,126,119,hud,54,1);
            /* The main target must disappear after destruction. The
             * surviving flank drone becomes the explicit objective, not
             * a misleading ghost RAIDER label or empty boss bar. */
            /* The mission states what to DO, not just enemy hit points.
             * A thin 4-pixel-font cue above the mission panel only, preserving
             * the 3D view and all existing solo/handheld rendering budgets. */
            if(raider.phase==1 && !raider.charge){
                const char *goal=!raider.hp && raider.wing_hp?
                    "DESTROY THE DRONE TO CLEAR WAVE":
                    raider.wing_hp?"DEFEAT RAIDER AND DRONE":
                    "DEFEAT THE RAIDER";
                text(pg,6,89,goal,54,1);
            }
            if(raider.phase==1 && !raider.hp && raider.wing_hp){
                s32 dx=raider.wing_x-shx,dy=raider.wing_y-shy,
                    dz=raider.wing_z-shz;
                s32 side=(s32)(((long long)dx*br[0]+
                       (long long)dy*br[1]+(long long)dz*br[2])>>14);
                const char *hint=side>250?"DRONE RIGHT - TURN RIGHT":
                                 side< -250?"DRONE LEFT - TURN LEFT":
                                 "DRONE AHEAD - FIRE!";
                text(pg,10,90,hint,53,1);
            }
            if(raider.phase==2){
                text(pg,65,77,"WAVE CLEARED!",54,2);
                text(pg,68,91,"NEXT WAVE SOON",50,1);
            }
            if(raider.phase==3){
                text(pg,61,73,"MISSION FAILED",53,2);
                text(pg,72,91,"RESTARTING SOON",50,1);
            }
            if(raider.phase==4)result_banner=1;
        }else text(pg,4,112,"LOADING SHARED MISSION...",50,1);
    }else if(!pc_mp_coop_mode() && pc_mp_match_ready()){
        uint8_t hh,gh,wh,wg;
        if(pc_mp_duel(&hh,&gh,&wh,&wg)){
            int remote_hp=(pc_mp_role()==1)?gh:hh;
            if(have_hp && remote_hp<previous_remote_hp)hit_flash=8;
            previous_remote_hp=remote_hp;have_hp=1;
            char status[64];
            rect(pg,3,106,234,21,0);
            rect(pg,3,106,234,1,53);
            snprintf(status,sizeof status,"SPARRING  P1 HEALTH: %u  P2 HEALTH: %u",
                     (unsigned)hh,(unsigned)gh);
            text(pg,7,109,status,50,1);
            snprintf(status,sizeof status,"ROUNDS WON: PLAYER 1 %u  PLAYER 2 %u",
                     (unsigned)wh,(unsigned)wg);
            text(pg,7,118,status,54,1);
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
    if(result_banner){
        /* Overlay AFTER all 3D ships and labels. A solid high-contrast
         * plate keeps the planet's detailed scene visible around it, but
         * prevents wireframes/pilot labels from cutting through the title.
         * Static rect/text only; zero allocations or sprite resources. */
        rect(pg,47,55,146,42,0);
        rect(pg,47,55,146,2,54);
        rect(pg,47,95,146,2,54);
        rect(pg,47,55,2,42,55);
        rect(pg,191,55,2,42,55);
        text(pg,55,61,"MISSION COMPLETE",54,2);
        text(pg,87,80,"BOTH PILOTS WIN!",50,1);
        text(pg,78,88,"NEXT MATCH SOON",55,1);
        pc_mp_note_clear_draw();
    }
#else
    (void)pg;
#endif
}
