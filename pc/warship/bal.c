// bal.c: the WARSHIP BALANCE REPORT (plain C, no game needed). Run with pc/warship/bal.sh.
// It mirrors the numbers in pc/pc_missions.inc (encounter chance, severe types, buster timer, hellscape survival, 20 percent loss)
// and drives the real fight rules (ws_fight.c) with the bot pilot. It prints a table and checks the targets at the bottom.
// The three player profiles are ASSUMPTIONS (see PROFILES): change them and the table changes. Nothing here touches the game.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "ws.h"
#include "ws_fight.h"
#include "../sim/colony.h"
#include "bal_rules.h"

static int checks, fails;
#define CHECK(c, ...) do{ checks++; if(!(c)){ fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } }while(0)
static uint32_t rs=0x1234567u; static uint32_t rnd(void){ rs=rs*1664525u+1013904223u; uint32_t x=rs; x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static double rf(void){ return (rnd()&0xFFFFFF)/16777216.0; }

typedef struct { const char *nm; int prepared; Player p; } Prof;
static const char *const MODE[4]={"OFF","RARE","NORMAL","COMMON"};
static const char *const DIFF[3]={"CALM","NORMAL","HARSH"};

// ---- PROFILES: what a player of that stage looks like (level, ship, armour, potions). "prepared" = shield spell, firefly friend, kits, wingmen
static void profiles(Prof *P){
    Player e={80,2,3,0,0,4,  40,90,1,1,800LL,2,30,0};          // level laser gun tech u_far dlv  shield armour wing kits maxh potions d_def beast
    Player m={800,6,10,1,6,14, 120,60,2,3,20000LL,3,200,0};
    Player l={20000,12,20,3,30,40, 300,40,6,8,5000000LL,5,800,0};
    Prof q[6]={{"EARLY",0,e},{"EARLY",1,e},{"MID",0,m},{"MID",1,m},{"LATE",0,l},{"LATE",1,l}};
    for(int i=0;i<6;i++){ P[i]=q[i]; P[i].p.prep=q[i].prepared; if(!q[i].prepared){ P[i].p.wing=0; P[i].p.kits=0; P[i].p.potions=q[i].p.potions>1?1:0; P[i].p.armour=q[i].p.armour<100?q[i].p.armour+30:100; P[i].p.shield=q[i].p.shield/3; } }
}

// ---- 1. how often things fire, per hour of play (one hour = 3600 game seconds on the surface/dungeon)
static void freq(const Player *pl, int mode, double hours, double *enc, double *severe, double *bust, double *board){
    long ne=0, ns=0, nb=0, nd=0; int active_until=-1; long minutes=(long)(hours*60);
    for(long mn=0; mn<minutes; mn++){
        if(mn<active_until) continue;
        Enc e; if(!enc_roll(pl,mode,rnd(),&e)) continue;
        ne++; active_until = (int)mn + ENC_EXPIRE_MIN + (e.type?0:0); if(e.severe) ns++; if(e.type==1) nb++; if(e.type==4) nd++;
        if(e.type) active_until = (int)mn + ENC_QUIET_MIN;      // a threat that fired blocks the next roll for the quiet time
    }
    *enc=ne/hours; *severe=ns/hours; *bust=nb/hours; *board=nd/hours;
}

// ---- 2. a planet-buster: can the player stop it in the window (fight), and what if they just stay on the surface
static double stop_rate(const Player *pl, int trials, int *cls_out){
    int ok=0; int cls_sum=0, n=0; rs=0xABCDEF01u;          // the same busters for a raw and a prepared player of one stage
    for(int t=0;t<trials;t++){ uint32_t seed=rnd()|1; int z=enc_z(pl,rnd()); static WsCraft c; ws_make(&c,seed,z,0); if(c.cls<WC_CITADEL){ t--; if(++n>20000) break; continue; }
        cls_sum+=c.cls; int mins=buster_mins(c.cls); WsShip s=ship_of(pl); static WsFight f; wf_start(&f,&c,&s); WsInput in; int ticks=0;
        while(f.state==WF_ON && f.layers_up>0 && ticks<60*60*mins){ wf_bot(&f,&in,1); wf_tick(&f,&in,1/60.0); ticks++; }
        if(f.layers_up==0 && f.state==WF_ON && ticks+60*BUSTER_APPROACH_S<=60*60*mins) ok++; }
    if(cls_out) *cls_out = trials? cls_sum/trials : 0; return trials? (double)ok/trials : 0;
}
// surface survival after the buster fires, for a player who does NOT leave: hellscape, ship state, rescue, potions
static double surface_survive(const Player *pl, int cls, int trials){
    int ok=0; for(int t=0;t<trials;t++) ok += hell_survive(pl,cls,rnd(),rf()*15.0,rf(),0.3+0.7*rf())?1:0; return (double)ok/trials; }

int main(int argc, char **argv){
    int quick = argc>1 && !strcmp(argv[1],"quick"); double hours = quick?400:4000; int trials = quick?60:200;
    Prof P[6]; profiles(P);
    printf("WARSHIP BALANCE REPORT (%s run: %.0f game hours per cell, %d busters per cell)\n\n",quick?"quick":"full",hours,trials);
    printf("1. HOW OFTEN (per hour of play, the player is always somewhere a warship can drop in)\n");
    printf("%-14s %-7s %9s %9s %9s %9s %10s\n","player","mode","threats/h","severe/h","buster/h","boarder/h","minutes/buster");
    double rate[6][4][4];
    for(int i=0;i<6;i++) for(int m=1;m<4;m++){ double e,s,b,d; freq(&P[i].p,m,hours,&e,&s,&b,&d); rate[i][m][0]=e; rate[i][m][1]=s; rate[i][m][2]=b; rate[i][m][3]=d;
        char nm[24]; snprintf(nm,sizeof nm,"%s %s",P[i].nm,P[i].prepared?"PREP":"RAW"); printf("%-14s %-7s %9.2f %9.2f %9.3f %9.3f %10.0f\n",nm,MODE[m],e,s,b,d,b>0?60.0/b:0); }
    printf("\n2. PLANET-BUSTER (NORMAL and COMMON only): stop it in the window, or stay on the surface and burn\n");
    printf("%-14s %9s %8s %12s %14s\n","player","avg class","window","stop it","survive the fire (stays on the surface)");
    double stop[6], surv[6]; int cls[6];
    for(int i=0;i<6;i++){ stop[i]=stop_rate(&P[i].p,trials,&cls[i]); surv[i]=surface_survive(&P[i].p,cls[i]>=WC_CITADEL?cls[i]:WC_CITADEL,4000);
        char nm[24]; snprintf(nm,sizeof nm,"%s %s",P[i].nm,P[i].prepared?"PREP":"RAW"); printf("%-14s %9d %6d m %11.0f%% %13.0f%%\n",nm,cls[i],buster_mins(cls[i]),stop[i]*100,surv[i]*100); }
    printf("\n3. WHAT A LOST BUSTER COSTS (only the death penalty: 1/%d of the gold and the cargo hold; the save is otherwise untouched)\n",LOSS_DIV);
    printf("   a player with 10,000 gold loses %d gold per death; busters that end in death per hour of play, stay-and-burn:\n",10000/LOSS_DIV);
    for(int i=0;i<6;i++){ for(int m=2;m<4;m++){ double dpb = rate[i][m][2]*(1-stop[i])*(1-surv[i]); if(m==2 || m==3){ char nm[24]; snprintf(nm,sizeof nm,"%s %s",P[i].nm,P[i].prepared?"PREP":"RAW"); printf("   %-14s %-7s deaths per hour (never leaves, never fights) %.3f = one per %.1f hours\n",nm,MODE[m],dpb,dpb>0?1.0/dpb:999); } } }
    printf("\n4. SIDE GAME DIFFICULTY: a DEFEND mission brings a real raid on the colony (size 6 + %d x class + bonus/5). Same colonies and raids on all three.\n",DEFEND_K);
    printf("%-9s %6s | %-24s | %-24s | %-24s\n","colony","def","CALM  won lost ruin gone","NORMAL won lost ruin gone","HARSH  won lost ruin gone");
    int dd[3][3][4]; double defavg[3];
    for(int q=0;q<3;q++){ int N=300, n=0; double dsum=0; memset(dd[0][q],0,sizeof dd[0][q]); memset(dd[1][q],0,sizeof dd[1][q]); memset(dd[2][q],0,sizeof dd[2][q]);
        for(int k=0;k<N;k++){ SimColony c; col_init(&c,rnd(),0); col_update(&c,q==0?0x7:q==1?0xFF:0x1FF); c.world_ok=1; c.danger=2;
            for(uint32_t dy=1; dy<=28u*(q==0?3u:q==1?8u:20u); dy++) col_day_ex(&c,dy,28,COLD_CALM,0);
            if(c.state!=COLS_ALIVE || !c.nset) continue; n++; dsum+=col_defence(&c);
            int cl=WC_CITADEL+(int)(rnd()%6u); uint32_t size0=6u+(uint32_t)cl*DEFEND_K+(20u+rnd()%40u)/5u;
            for(int d=0; d<3; d++){ uint32_t size=size0; if(DEFEND_CAP_ON && d==COLD_NORMAL){ uint32_t cap=(uint32_t)(col_defence(&c)*6/5+2); if(size>cap) size=cap; } SimColony k2=c; ColAway aw; uint64_t rw; int r=col_invade(&k2,size,d,500,&aw,&rw);
                if(r>0) dd[d][q][0]++; else if(r<0){ dd[d][q][1]++; if(k2.state==COLS_RUINS) dd[d][q][2]++; if(k2.state==COLS_DEAD_WORLD) dd[d][q][3]++; } } }
        defavg[q]= n? dsum/n : 0; for(int d=0;d<3;d++) for(int j=0;j<4;j++) dd[d][q][j]= n? dd[d][q][j]*100/n : 0;
        printf("%-9s %6.0f | %9d%% %4d%% %4d%% %4d%% | %9d%% %4d%% %4d%% %4d%% | %9d%% %4d%% %4d%% %4d%%\n",q==0?"SMALL":q==1?"MIDDLE":"BIG",defavg[q],dd[0][q][0],dd[0][q][1],dd[0][q][2],dd[0][q][3],dd[1][q][0],dd[1][q][1],dd[1][q][2],dd[1][q][3],dd[2][q][0],dd[2][q][1],dd[2][q][2],dd[2][q][3]); }

    // ---- the targets (what the numbers must satisfy; the tuned constants live in bal_rules.h and are copied into pc_missions.inc)
    printf("\nTARGETS\n");
    for(int i=0;i<6;i++){ CHECK(rate[i][1][1]==0, "RARE must never fire a severe threat (%s)",P[i].nm); CHECK(rate[i][1][0]<=1.2, "RARE fires %.2f threats an hour for %s (limit 1.2)",rate[i][1][0],P[i].nm);
        CHECK(rate[i][2][2] <= 0.35, "NORMAL busters %.3f an hour for %s (limit one per 3 hours)",rate[i][2][2],P[i].nm);
        CHECK(rate[i][3][2] <= 0.70, "COMMON busters %.3f an hour for %s (limit one per 85 minutes)",rate[i][3][2],P[i].nm);
        CHECK(rate[i][2][1] <= 1.6, "NORMAL severe threats %.2f an hour for %s (limit 1.6)",rate[i][2][1],P[i].nm);
        CHECK(rate[i][2][0] >= 0.3, "NORMAL almost never fires for %s (%.2f an hour)",P[i].nm,rate[i][2][0]);
        CHECK(rate[i][3][0] >= rate[i][2][0] && rate[i][2][0] >= rate[i][1][0], "modes not ordered for %s",P[i].nm); }
    for(int i=0;i<6;i+=2){ CHECK(surv[i+1] >= 0.95, "a PREPARED %s player survives the fire only %.0f%%",P[i].nm,surv[i+1]*100); CHECK(surv[i+1] > surv[i], "preparing must help (%s: %.0f%% against %.0f%%)",P[i].nm,surv[i+1]*100,surv[i]*100);
        CHECK(surv[i] >= 0.70, "a RAW %s player survives only %.0f%%: a death trap",P[i].nm,surv[i]*100); }
    for(int i=0;i<6;i+=2) for(int m=2;m<4;m++){ double dpb=rate[i][m][2]*(1-stop[i])*(1-surv[i]); CHECK(dpb <= 0.05, "RAW %s dies to busters once per %.0f hours (%s)",P[i].nm,1.0/dpb,MODE[m]); }
    CHECK(stop[1]>=stop[0] && stop[3]>=stop[2], "preparing must not make a buster harder to stop");
    CHECK(dd[0][0][3]==0 && dd[0][1][3]==0 && dd[0][2][3]==0 && dd[0][0][2]==0 && dd[0][1][2]==0 && dd[0][2][2]==0, "CALM must never ruin a colony or end a world");
    for(int q=0;q<3;q++) CHECK(dd[0][q][0]>=dd[1][q][0] && dd[1][q][0]>=dd[2][q][0], "colony wins must not rise from CALM to NORMAL to HARSH (colony %d)",q);
    CHECK(dd[1][2][0]>=70, "NORMAL: a big colony wins only %d%% of DEFEND raids",dd[1][2][0]); CHECK(dd[1][0][2]<=30 && dd[1][0][3]==0, "NORMAL: a small colony is ruined by %d%% of DEFEND raids",dd[1][0][2]); CHECK(dd[1][2][3]==0, "NORMAL ended a world through a DEFEND raid");
    printf("%d checks, %d failed\n",checks,fails); return fails?1:0;
}
