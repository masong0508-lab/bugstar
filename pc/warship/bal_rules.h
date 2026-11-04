// bal_rules.h: the encounter and aftermath RULES of pc/pc_missions.inc as plain C, so bal.c can measure them.
// Every tuned number lives here and in pc/pc_missions.inc under the same name (PCM_*). Build bal.c with -DBAL_OLD to get the numbers BEFORE patch 17.
#ifndef BAL_RULES_H
#define BAL_RULES_H
#include <string.h>
#include <stdint.h>
#include "ws.h"
#include "ws_fight.h"

typedef struct { int level, laser, gun, tech, u_far, dlv, shield, armour, wing, kits; long long maxh; int potions, d_def, beast, prep; } Player;
typedef struct { int type, severe, cls, z; } Enc;       // type: 0 plain, 1 buster, 2 orbital strikes, 3 troop waves, 4 boarders

#ifdef BAL_OLD
#define LOSS_DIV 5                 // death penalty: 1/5 of the gold
#define ENC_EXPIRE_MIN 10          // an open warship waits this long (36000 frames)
#define ENC_QUIET_MIN 10           // after a severe threat, no new roll for this long
static const int ENC_P_CAP[4]={1000,1000,1000,1000};   // per-minute chance cap (thousandths)
#define BUSTER_MIN_MINS 3
#define BUSTER_APPROACH_S 60
#define BUSTER_SHARE 3             // busters are 1 of 3 severe types
#define HELL_AR_FIXED 0
#define DEFEND_CAP_ON 0
#define DEFEND_K 8                 // DEFEND raid size = 6 + K x class + bonus/5
#else
#define LOSS_DIV 5                 // PCM_LOSS_DIV: kept at 1/5 (see ROADMAP_TODO.md C: it was never the problem)
#define ENC_EXPIRE_MIN 10          // PCM_EXPIRE_MIN
#define ENC_QUIET_MIN 25           // PCM_QUIET_MIN: after a severe threat the sky stays quiet for 25 minutes
static const int ENC_P_CAP[4]={0,20,45,70};             // PCM_P_CAP: the most a minute can roll (thousandths), however high the danger or level
#define BUSTER_MIN_MINS 5          // PCM_BUSTER_MIN: the window is never under 5 minutes
#define BUSTER_APPROACH_S 60
#define BUSTER_SHARE 6             // busters are 1 of 6 severe rolls (strikes 2, troops 3)
#define HELL_AR_FIXED 1            // PCM_HELL_AR: armour scale fixed (it was upside down)
#define DEFEND_CAP_ON 1            // PCM_DEFEND_CAP: on NORMAL a DEFEND raid is at most 6/5 of the colony defence + 2 (CALM always holds; HARSH keeps the full size)
#define DEFEND_K 4                 // PCM_DEFEND_K: a DEFEND raid is 6 + 4 x class + bonus/5 (was 8 x class)
#endif

static const int ENC_P[4]={0,8,22,55};            // chance per game minute in thousandths, before danger and level (unchanged)

static int enc_zcap(const Player *p){ long z=300+p->u_far*40+p->beast*300+(p->level>100000?2000:p->level/50); return z>WS_ZMAX_FAIR?WS_ZMAX_FAIR:(int)z; }
static int enc_zplayer(const Player *p){ return enc_zcap(p)*80/100+p->dlv*10; }
static int enc_z(const Player *p, uint32_t h){ int z=enc_zplayer(p)-300+(int)((h>>10)%450u); if(z<150) z=150; if(z>WS_ZMAX_FAIR) z=WS_ZMAX_FAIR; return z; }
static int buster_mins(int cls){ int m=15-cls; if(m<BUSTER_MIN_MINS) m=BUSTER_MIN_MINS; if(m>12) m=12; return m; }

static int enc_roll(const Player *p, int mode, uint32_t h, Enc *e){
    int dg=p->dlv; int p1=ENC_P[mode]*(100+dg*8+(p->level>2000?200:p->level/10))/100; if(p1>ENC_P_CAP[mode]) p1=ENC_P_CAP[mode];
    if((int)(h%1000u)>=p1) return 0;
    memset(e,0,sizeof *e); e->z=enc_z(p,h); static WsCraft c; ws_make(&c,h*2654435761u|1u,e->z,0); e->cls=c.cls;
    e->severe = mode>=2 && c.cls>=WC_CITADEL;
    if(e->severe){
#ifdef BAL_OLD
        e->type=1+(int)((h>>20)%3u);
#else
        { unsigned r=(h>>20)%(unsigned)BUSTER_SHARE; e->type = r==0 ? 1 : r<3 ? 2 : 3; }
#endif
    }
    else if(mode>=2 && (h>>25)%10u<3 && (h>>24)%4u==0) e->type=4;     // boarders: only when you are in a dungeon (about 3 in 10 minutes of play)
    return 1; }

static WsShip ship_of(const Player *p){ WsShip s; memset(&s,0,sizeof s); s.attack=ws_attack((double)p->level,p->laser,p->gun,p->tech); s.hull_max=100; s.shield_max=p->shield; s.armour=p->armour; s.fuel_max=200; s.wing=p->wing; s.kits=p->kits; s.ammo[0]=-1; return s; }

// the burning surface: a player on the surface when a buster fires. dist = cells to the ship (5..96: the world is 128 wide, you can be anywhere),
// walking about 4 cells a second (5 on flat ground, slower on slopes and in water). react_s = seconds before the player moves (reading the warnings),
// hp = hearts when it starts, as a share of maximum (a player mid-fight is not at full health).
static int hell_survive(const Player *p, int cls, uint32_t h, double react_s, double r1, double hp){
#if HELL_AR_FIXED
    int a=p->armour; int ex=a>40?a-40:0; int dthr=5+ex/4, pp=85-ex/3;      // a = percent of damage taken (100 none, 40 and under = best): destroyed 5..20 percent, pod 85..65 percent
#else
    int ar=p->armour; int dthr=40-ar*3; if(dthr<5) dthr=5; int pp=15+ar*4; if(pp>85) pp=85;   // old: "percent taken" fed in as if more were better: always 5 and 85
#endif
    int r=(int)(h%100u); int ship= r<dthr?2 : r<dthr+35?1 : 0; int pod=(int)((h>>8)%100u)<pp;
    int fire_s=60*(3+cls/2); int res=(p->d_def>100?100:p->d_def)/2 + (p->prep?35:0); if(res>85) res=85;
    double dist=5+r1*91; int escape_s; int walk=(int)(react_s);
    if(ship<2) escape_s=walk+(int)(dist/4.0)+2;
    else if(pod||p->wing>0) escape_s=walk+(int)((dist>5?dist-5:0)/4.0)+4;
    else escape_s=fire_s;
    long long hearts=(long long)((double)p->maxh*hp); if(hearts<1) hearts=1; int pots=p->potions;
    for(int s=0; s<escape_s && s<fire_s; s++){
        int ramp=100+(s/30)*10; if(ramp>250) ramp=250; long long d=(long long)((double)p->maxh*(10+cls*2)*ramp*(100-res)/1e7); if(d<1) d=1; hearts-=d;
        while(hearts<=0){ if(pots<=0) return 0; pots--; hearts+=p->maxh/3; }
        if(hearts<p->maxh/3 && pots>0){ pots--; hearts+=p->maxh/3; if(hearts>p->maxh) hearts=p->maxh; } }
    return 1; }
#endif
