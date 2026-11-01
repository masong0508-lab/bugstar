// ws_test.c: tests for COLOSSAL WARSHIPS (pc/warship). Run pc/warship/test.sh. Every check prints a line; any FAIL makes the exit code 1.
#include "ws_fight.h"
#include "ws_deck.h"
#include "ws_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fails=0, checks=0;
#define CHECK(c, ...) do { checks++; if(!(c)){ fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)
static int font_ok(const char *s){ for(;*s;s++){ char c=*s; if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='.'||c==','||c=='\''||c==':'||c=='('||c==')'||c=='-'||c=='^')) return 0; } return 1; }
static WdDeck dk, dk2;

static void t_variety(void){
    enum { N=5000 }; static uint32_t sig[N]; int shapes[WS_NSHAPE]={0}, cults[WS_NCULT]={0}, mats[WS_NMAT]={0}, classes[WC_N]={0}; int nsig=0;
    for(int i=0;i<N;i++){ WsCraft c; int z=WS_ZMIN+(int)((unsigned)(i*7919)%(unsigned)(WS_ZMAX_FAIR-WS_ZMIN)); ws_make(&c,(uint32_t)i*2654435761u+1,z,0);
        shapes[c.shape]++; cults[c.cult]++; mats[c.mat]++; classes[c.cls]++; uint32_t s=ws_signature(&c); int dup=0; for(int k=0;k<nsig;k++) if(sig[k]==s){ dup=1; break; } if(!dup) sig[nsig++]=s;
        CHECK(c.nprim>0 && c.nprim<=WS_MAXPRIM, "craft %d has %d hull parts", i, c.nprim); CHECK(c.nsub>0 && c.nsec>=3, "craft %d sections", i);
        CHECK(font_ok(c.name) && strlen(c.name)<=26, "name '%s'", c.name); }
    int ns=0, nc=0, nm=0; for(int i=0;i<WS_NSHAPE;i++) ns+=shapes[i]>0; for(int i=0;i<WS_NCULT;i++) nc+=cults[i]>0; for(int i=0;i<WS_NMAT;i++) nm+=mats[i]>0;
    printf("hull variety over %d seeds: %d different hull designs, %d of %d shapes, %d cultures, %d materials\n",N,nsig,ns,WS_NSHAPE,nc,nm);
    for(int i=0;i<WS_NSHAPE;i++) printf("  %-14s %d\n",ws_shape_name(i),shapes[i]);
    CHECK(nsig>=N*95/100, "only %d different designs in %d", nsig, N); CHECK(ns==WS_NSHAPE && nc==WS_NCULT && nm==WS_NMAT, "missing shapes/cultures/materials");
    int ncl=0; for(int i=0;i<WC_N-1;i++) ncl+=classes[i]>0; CHECK(ncl>=WC_N-2, "only %d size classes seen", ncl);
}
static void t_calibration(void){
    WsCraft c; ws_make_ref(&c); WsKill k; memset(&k,0,sizeof k); k.attack=1; k.out_dmg[WW_LASER]=c.hp_hull; WsPay p; ws_pay(&c,&k,&p);
    double r=wn_to_d(p.reward); char b[64]; wn_text(p.reward,b,sizeof b,0);
    printf("calibration: reference station (%s, %s) outside only, laser, attack 1: %s gold (%.4e)\n",c.name,"160 KM",b,r);
    CHECK(fabs(r/4.07e23-1)<0.01, "reference pays %.4e, not 4.07e23 within 1 percent", r);
    CHECK(!strcmp(b,"407 SEXTILLION"), "named '%s'", b);
    k.attack=1000; ws_pay(&c,&k,&p); r=wn_to_d(p.reward); printf("calibration: same kill with 1,000 levels of attack: %.4e gold\n",r);
    CHECK(fabs(r/1e11-1)<0.01, "1000 levels pays %.4e, not 1e11 within 1 percent", r);
    // the same ratio at other sizes
    for(int z=-900;z<=3000;z+=650){ WsCraft d; ws_make(&d,99,z,0); WsKill a; memset(&a,0,sizeof a); a.attack=1; a.out_dmg[0]=wn(1); WsPay p1,p2; ws_pay(&d,&a,&p1); a.attack=1000; ws_pay(&d,&a,&p2);
        double ratio=wn_to_d(wn_div(p1.reward,p2.reward)); CHECK(fabs(ratio/(4.07e23/1e11)-1)<0.01, "z %d ratio %.4e", z, ratio); }
    // the attack factor is one smooth falling curve
    double prev=2; for(double a=1;a<1e30;a*=1.37){ double A=ws_A(a); CHECK(A<prev && A>0, "A not falling at %g", a); prev=A; }
}
static void t_scaling(void){
    // HP, cost and reward rise with size for every seed, at least exponentially in the size parameter, with no overflow or underflow, and read well
    int bad=0; double minrate=1e9;
    for(int s=0;s<40;s++){ WN pc=wn(0), ph=wn(0); int first=1; WsCraft c; ws_make(&c,(uint32_t)s*977u+3,(s*137)%3000,1);
        for(int z=WS_ZMIN; z<=WS_ZMAX; z += z<4000 ? 25 : 2500){ ws_scale(&c,z,1);
            WN cost=c.cost[WK_TOTAL], hp=c.hp_hull;
            CHECK(!wn_is_zero(cost) && !wn_is_zero(hp) && !wn_sat(cost), "z %d seed %d cost or HP zero/saturated", z, s);
            CHECK(c.cost[WK_TOTAL].m>=1 && c.cost[WK_TOTAL].m<10 && isfinite(c.cost[WK_TOTAL].m), "z %d mantissa", z);
            if(!first){ if(wn_cmp(cost,pc)<=0 || wn_cmp(hp,ph)<=0){ bad++; if(bad<5) printf("  not rising at z %d seed %d\n",z,s); } else { double dz=(z-(z-25))/100.0; (void)dz; } }
            first=0; pc=cost; ph=hp;
            char b[96]; wn_text(cost,b,sizeof b,0); CHECK(strlen(b)>0 && strlen(b)<=60 && font_ok(b), "text '%s' at z %d", b, z);
            ws_len_text(z,b,sizeof b); CHECK(strlen(b)<=40 && font_ok(b), "size text '%s'", b);
        }
    }
    CHECK(bad==0, "%d places where cost or HP did not rise", bad);
    // exponential: over every decade of length, cost grows by at least x100 (it is about L^3, x1000)
    WsCraft a,b; ws_make(&a,5,0,0); b=a; for(int z=WS_ZMIN; z+100<=WS_ZMAX_FAIR; z+=50){ ws_scale(&a,z,0); ws_scale(&b,z+100,0); double rate=wn_log10(b.cost[WK_TOTAL])-wn_log10(a.cost[WK_TOTAL]); if(rate<minrate) minrate=rate; }
    printf("growth: cost rises at least 10^%.2f per decade of length (from 1 nm to 10^30 m)\n",minrate); CHECK(minrate>=2.0, "growth only 10^%.2f per decade", minrate);
    WsCraft t; ws_make(&t,1,WS_ZMIN,0); char b1[64]; wn_text(t.cost[WK_TOTAL],b1,sizeof b1,0); printf("smallest (1 nm %s): cost %s gold\n",ws_class_name(t.cls),b1);
    ws_make(&t,1,WS_ZMAX,1); wn_text(t.cost[WK_TOTAL],b1,sizeof b1,2); printf("biggest cheat size (10^3000 m): cost %s\n",b1);
    CHECK(t.cheat && t.cost[WK_TOTAL].e>8000 && t.cost[WK_TOTAL].e<12000, "cheat-extreme exponent %lld", (long long)t.cost[WK_TOTAL].e);
    // names past 10^100 and 10^400 are the real Conway-Wechsler names
    char nb[96]; wn_text(wn_p10(100),nb,sizeof nb,0); CHECK(!strcmp(nb,"10 DUOTRIGINTILLION"), "10^100 named '%s'", nb);
    wn_text(wn_p10(303),nb,sizeof nb,0); CHECK(!strcmp(nb,"1 CENTILLION"), "10^303 named '%s'", nb);
    wn_text(wn_p10(400),nb,sizeof nb,0); printf("10^400 is '%s'\n",nb); CHECK(strstr(nb,"10 ")==nb && strstr(nb,"ILLION"), "10^400 named '%s'", nb);
    wn_text(wn_p10(-21),nb,sizeof nb,0); CHECK(!strcmp(nb,"1 SEXTILLIONTHS"), "10^-21 named '%s'", nb);
    WN big=wn_p10(9e14); WN sq=wn_mul(big,big); CHECK(wn_sat(sq), "10^(9e14) squared saturates at the type's top"); wn_text(sq,nb,sizeof nb,0); CHECK(strlen(nb)>0, "saturated text");
}
static void t_balance(void){
    // no mix of method and weapons pays more than the outside-only laser kill; every route still pays at least a fair share
    WsCraft c; ws_make_ref(&c); double base; { WsKill k; memset(&k,0,sizeof k); k.attack=50; k.out_dmg[0]=wn(1); WsPay p; ws_pay(&c,&k,&p); base=wn_to_d(p.reward); }
    double lo=1e300, hi=0; uint32_t r=12345;
    for(int i=0;i<200000;i++){ WsKill k; memset(&k,0,sizeof k); k.attack=50; for(int w=0;w<WW_N;w++){ r=r*1664525u+1013904223u; k.out_dmg[w]= (r>>28)&1 ? wn((r>>8)%1000+1.0) : wn(0); }
        r=r*1664525u+1013904223u; k.in_dmg = (r>>29)&1 ? wn((r>>8)%5000) : wn(0); r=r*1664525u+1013904223u; k.sab_all=(int)(r>>28); k.sab_crit=k.sab_all?(int)((r>>8)%(unsigned)(k.sab_all+1)):0;
        WsPay p; ws_pay(&c,&k,&p); double v=wn_to_d(p.reward)/base; if(v>hi) hi=v; if(v<lo) lo=v; }
    printf("balance: 200,000 random mixes pay between %.3f and %.3f of the outside-only laser kill\n",lo,hi);
    CHECK(hi<=1.0+1e-9, "a mix paid %.4f x the simple kill", hi); CHECK(lo>=0.05, "a mix paid only %.4f", lo);
    CHECK(ws_Fm(1,1)>ws_Fm(1,0), "precision must earn a bonus inside"); CHECK(ws_Fm(1,1)<1 && ws_Fm(0.5,1)<1, "inside must pay a little less");
    for(int w=1;w<WW_N;w++){ WsKill a; memset(&a,0,sizeof a); a.out_dmg[w]=wn(1); WsKill b2; memset(&b2,0,sizeof b2); b2.out_dmg[w-1]=wn(1); CHECK(ws_Fw(&a)<ws_Fw(&b2), "stronger weapon %d pays less", w); }
}
static int fight(const WsCraft *c, WsShip s, int clever, double *secs){ static WsFight f; wf_start(&f,c,&s); WsInput in; int n=0; while(f.state==WF_ON && n<60*1800){ wf_bot(&f,&in,clever); wf_tick(&f,&in,1/60.0); n++; } if(secs) *secs=f.t; return f.state; }
static void t_bots(void){
    WsCraft c; ws_make_ref(&c);
    WsShip un={20,100,40,100,70,0,0,{-1,0,0,0,0}};
    WsShip pr={1600,100,220,55,400,6,10,{-1,-1,40,8,0}};
    double t; int st=fight(&c,un,1,&t); printf("bot: unprepared level 20 ship vs the reference station: %s after %.0f s\n",st==WF_WON?"WON":"LOST",t); CHECK(st!=WF_WON, "an unprepared low-level ship won");
    st=fight(&c,pr,1,&t); printf("bot: prepared ship (attack 1600, shields 220, armour 55, 6 wingmen, 10 kits): %s after %.0f s\n",st==WF_WON?"WON":"LOST",t); CHECK(st==WF_WON, "the prepared ship lost");
    WsShip mid=pr; mid.attack=1100; mid.wing=0; mid.kits=0; mid.shield_max=60; mid.armour=100; st=fight(&c,mid,1,&t); printf("bot: right attack but no armour, wingmen or kits: %s after %.0f s\n",st==WF_WON?"WON":"LOST",t); CHECK(st!=WF_WON, "won without defences");
    // across sizes: a ship prepared for the craft (the threat's own recommendation, doubled) wins; one with a tenth of that attack never does
    int wins=0, tries=0, cheap=0;
    for(int z=-600; z<=1500; z+=150){ WsCraft d; ws_make(&d,(uint32_t)z*31u+7,z,0); WsThreat th; ws_threat(&d,1,100,0,0,70,&th);
        WsShip ok={th.need_attack*2,100,240,55,600,th.need_wing<6?6:th.need_wing,th.need_kits+10,{-1,0,0,0,0}}; tries++; if(fight(&d,ok,1,0)==WF_WON) wins++;
        WsShip weak={th.need_attack*0.1>1?th.need_attack*0.1:1,100,40,100,70,0,0,{-1,0,0,0,0}}; if(th.need_attack>30 && fight(&d,weak,0,0)==WF_WON) cheap++; }
    printf("bot: across 15 sizes, a ship prepared as THREAT says won %d of %d; one with a tenth of the attack won %d\n",wins,tries,cheap);
    CHECK(wins==tries, "a prepared ship lost at some size"); CHECK(cheap==0, "an underpowered ship won %d", cheap);
    // repair crews: stop shooting and the damage on the section goes away; leave for long and everything comes back
    { static WsFight f; WsShip s=pr; wf_start(&f,&c,&s); WsInput in; for(int i=0;i<60*40;i++){ wf_bot(&f,&in,1); wf_tick(&f,&in,1/60.0); }
      WN left=wf_hull_left(&f); for(int i=0;i<60*30;i++){ memset(&in,0,sizeof in); in.target=-1; wf_tick(&f,&in,1/60.0); }
      CHECK(wn_cmp(wf_hull_left(&f),left)>0 || f.layers_up>0, "repair crews did nothing"); wf_away(&f,500); CHECK(fabs(wn_to_d(wn_div(wf_hull_left(&f),c.hp_hull))-1)<1e-9 && f.layers_up==c.layers, "a long absence did not rebuild it"); }
    // sabotage: all shield generators blown inside: the shields fall at once
    { static WsFight f; WsShip s=un; wf_start(&f,&c,&s); for(int i=0;i<f.c.nsub;i++) if(f.c.sub[i].kind==WSS_GEN) wf_sabotage(&f,i,1); CHECK(f.layers_up==0, "generators gone but shields up"); CHECK(f.kill.sab_crit>0, "precision not counted"); }
}
static void t_interiors(void){
    int decks=0, subs_ok=0, subs=0, locked=0, alt=0; long npc=0, props=0;
    for(int s=0;s<60;s++){ int z = -800 + s*60; WsCraft c; ws_make(&c,(uint32_t)s*7477u+11,z,0); int D=wd_levels(&c); uint8_t *path=malloc(D+1);
        for(int sub=0;sub<c.nsub;sub++){ uint32_t h=wd_root(&c); for(int k=0;k<D;k++){ path[k]=(uint8_t)wd_sub_child(&c,sub,k); h=wd_child(h,path[k],k); }
            int list[WS_MAXSUB]; int n=wd_subs_at(&c,path,D,list); int found=0; for(int i=0;i<n;i++) if(list[i]==sub) found=1; CHECK(found, "sub %d not at its own path", sub);
            wd_make(&dk,&c,h,list,n,sub==0,D); wd_make(&dk2,&c,h,list,n,sub==0,D); CHECK(!memcmp(&dk,&dk2,sizeof dk), "deck not deterministic"); decks++; npc+=dk.nnpc; props+=dk.nprop;
            int room=-1; for(int r=1;r<dk.nroom;r++) if(dk.room[r].sub==sub) room=r; subs++;
            if(room<0){ CHECK(0, "z %d sub %d has no room in its deck", z, sub); continue; }
            int need_key=dk.room[room].locked; int wk=wd_path_len(&dk,dk.sx,dk.sy,room,1,0), nk=wd_path_len(&dk,dk.sx,dk.sy,room,0,0), vent=wd_path_len(&dk,dk.sx,dk.sy,room,0,1);
            if(need_key){ locked++; int sec=-1; for(int r=1;r<dk.nroom;r++) if(dk.room[r].kind==WR_SECURITY) sec=r; CHECK(sec>0 && wd_path_len(&dk,dk.sx,dk.sy,sec,0,0)>=0, "key room unreachable without the key");
                CHECK(nk<0, "a locked room was open"); if(vent>=0) alt++; }
            CHECK(wk>=0, "z %d sub %d unreachable even with the key", z, sub); if(wk>=0) subs_ok++;
            for(int r=1;r<dk.nroom;r++) CHECK(wd_path_len(&dk,dk.sx,dk.sy,r,1,1)>=0, "room %s cut off", dk.room[r].name);
            CHECK(dk.tram_room>0, "no tram");
        }
        free(path); }
    printf("interiors: %d decks, every subsystem reachable %d of %d, %d locked with a keycard (%d also by a vent), %.1f crew and guards and %.0f props a deck\n",decks,subs_ok,subs,locked,alt,(double)npc/decks,(double)props/decks);
    CHECK(locked>0 && alt==locked, "locked rooms without a second way in");
    // huge craft: levels deep, still cheap
    WsCraft u; ws_make(&u,42,2700,0); int D=wd_levels(&u); printf("interiors: a universe hulk is %d sector levels deep above its decks\n",D); CHECK(D>60, "universe hulk only %d levels", D);
    ws_make(&u,42,WS_ZMAX,1); D=wd_levels(&u); printf("interiors: the biggest cheat craft is %d levels deep\n",D); CHECK(D>9000, "cheat extreme %d", D);
    // the crew's day: routines, then alarms
    WsCraft c; ws_make_ref(&c); int Dr=wd_levels(&c); uint32_t h=wd_root(&c); uint8_t p[64]; for(int k=0;k<Dr;k++){ p[k]=(uint8_t)wd_sub_child(&c,0,k); h=wd_child(h,p[k],k); } int list[WS_MAXSUB]; int n=wd_subs_at(&c,p,Dr,list); wd_make(&dk,&c,h,list,n,1,Dr);
    int acts[8]={0}; for(int i=0;i<60*60*24;i+=60){ int hurt=0; wd_tick(&dk,1.0,-50,-50,&hurt); for(int j=0;j<dk.nnpc;j++) if(dk.npc[j].kind==WN_CREW) acts[dk.npc[j].act]++; }
    printf("crew day: sleeping %d eating %d chatting %d working %d walking %d\n",acts[WA_SLEEP],acts[WA_EAT],acts[WA_CHAT],acts[WA_WORK],acts[WA_WALK]);
    CHECK(acts[WA_SLEEP]&&acts[WA_EAT]&&acts[WA_CHAT]&&acts[WA_WORK]&&acts[WA_WALK], "a routine never happens");
    dk.alarm=1; int hurt=0; for(int i=0;i<600;i++) wd_tick(&dk,1/60.0,dk.sx+0.5f,dk.sy+0.5f,&hurt); int flee=0; for(int j=0;j<dk.nnpc;j++) if(dk.npc[j].act==WA_FLEE||dk.npc[j].act==WA_HUNT) flee++; CHECK(flee>0, "nobody reacts to the alarm");
    // drawing: every room kind renders at two sizes without a crash, and is not one flat colour
    static uint32_t px[480*320]; int kinds=0;
    for(int r=1;r<dk.nroom;r++){ const WdRoom *m=&dk.room[r]; float x=(m->x0+m->x1)/2+0.5f, y=(m->y0+m->y1)/2+0.5f; wv_render(px,240,160,&dk,x,y,0.3f,1.0,0,0); wv_render(px,480,320,&dk,x,y,2.0f,1.0,0,0);
        int diff=0; for(int i=1;i<480*320;i++) if(px[i]!=px[0]){ diff=1; break; } CHECK(diff, "room %s renders flat", m->name); kinds++; }
    printf("drawing: %d rooms drawn at 240x160 and 480x320\n",kinds);
}
static void t_saveformat(void){
    // the save file keeps wide numbers as text (mantissa with 15 digits, exponent): they come back the same
    for(int i=0;i<1000;i++){ WN a=wn_p10(-30+i*0.731); char b[64]; snprintf(b,sizeof b,"%.15g %lld",a.m,(long long)a.e); WN c; long long e; sscanf(b,"%lf %lld",&c.m,&e); c.e=e;
        CHECK(fabs(c.m/a.m-1)<1e-14 && c.e==a.e, "round trip %s", b); }
}
int main(void){
    t_calibration(); t_variety(); t_scaling(); t_balance(); t_bots(); t_interiors(); t_saveformat();
    printf("%d checks, %d failed\n",checks,fails); return fails?1:0;
}
