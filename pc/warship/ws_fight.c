// ws_fight.c: the warship fight rules. See ws_fight.h and the README (FIGHT RULES).
#include "ws_fight.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const double rate_[WW_N]={6,2,1,0.25,0.1}, heat_[WW_N]={5,12,4,10,80};
#define HEAT_MAX 100.0
#define COOL 20.0                        // heat lost per second
double wf_heat_max(void){ return HEAT_MAX; }
double wf_rate(int w){ return rate_[(unsigned)w%WW_N]; }
static const char *const ph_nm[5]={"","SHIELDS","OUTER HULL","INNER HULL","ENRAGED CORE"};
const char *wf_phase_name(int p){ return ph_nm[p<1?1:p>4?4:p]; }
static uint32_t fr(WsFight *f){ f->rng=f->rng*1664525u+1013904223u; return f->rng>>8; }
static double fr01(WsFight *f){ return (fr(f)&0xFFFFFF)/16777216.0; }
static void say(WsFight *f, const char *m){ snprintf(f->msg,sizeof f->msg,"%s",m); f->msg_t=150; }
int wf_alive(const WsFight *f, int kind){ int n=0; for(int i=0;i<f->c.nsub;i++) if(f->c.sub[i].kind==kind && f->c.sub[i].alive) n++; return n; }
static int count_kind(const WsCraft *c, int kind){ int n=0; for(int i=0;i<c->nsub;i++) if(c->sub[i].kind==kind) n++; return n; }
WN wf_hull_left(const WsFight *f){ WN t=wn(0); for(int i=0;i<f->c.nsec;i++) t=wn_add(t,f->c.sec[i].hp); return t; }

static void set_phase(WsFight *f){
    int p = f->layers_up>0 ? 1 : f->sec < f->c.nsec/2 ? 2 : f->sec < f->c.nsec-1 ? 3 : 4;
    if(p!=f->phase){ f->phase=p; if(p==4 && wf_alive(f,WSS_BRIDGE)) say(f,"IT IS ENRAGED! EVERYTHING FIRES AT ONCE");
        else if(p==2) say(f,"SHIELDS DOWN: HIT THE OUTER HULL"); else if(p==3) say(f,"OUTER HULL BROKEN: THE INNER HULL"); else if(p==4) say(f,"THE CORE IS OPEN"); }
}
void wf_start(WsFight *f, const WsCraft *c, const WsShip *s){
    memset(f,0,sizeof *f); f->c=*c; f->ship=*s; f->hull=s->hull_max; f->shield=s->shield_max; f->fuel=s->fuel_max; f->wing=s->wing>10?10:s->wing; f->kits=s->kits;
    for(int w=0;w<WW_N;w++) f->ammo[w]=s->ammo[w];
    for(int i=0;i<f->wing;i++) f->wing_hp[i]=60;
    f->layers_up=f->c.layers; f->lay_hp=f->c.hp_shield; f->sec=0; f->phase=0; f->state=WF_ON; f->rng=c->seed^0xF16Bu;
    f->turrets0=count_kind(c,WSS_TURRET); f->hangars0=count_kind(c,WSS_HANGAR); f->gens0=count_kind(c,WSS_GEN); f->reacs0=count_kind(c,WSS_REACTOR);
    f->kill.attack=s->attack; f->fighter_t=12; f->out_total=wn(0);
    set_phase(f);
}
static void sub_dies(WsFight *f, WsSub *s, int inside){
    s->alive=0; s->hp=wn(0); s->inside_kill=(uint8_t)inside; char b[48];
    snprintf(b,sizeof b,"%s DESTROYED",ws_sub_name(s->kind)); say(f,b);
    if(s->kind==WSS_GEN && !wf_alive(f,WSS_GEN) && f->layers_up>0){ f->layers_up=0; f->lay_hp=wn(0); say(f,"ALL SHIELD GENERATORS DOWN: SHIELDS GONE"); set_phase(f); }
}
static void hull_damage(WsFight *f, WN d, int target, int inside, int w){
    while(f->sec<f->c.nsec && wn_is_zero(f->c.sec[f->sec].hp)) f->sec++;
    if(f->sec>=f->c.nsec) return;
    WsSec *s=&f->c.sec[f->sec];
    if(wn_cmp(d,s->hp)>0) d=s->hp;
    if(target>=0 && target<f->c.nsub){ WsSub *u=&f->c.sub[target]; if(u->alive && u->sec==f->sec){ WN k = wn_cmp(d,u->hp)>0 ? u->hp : d; u->hp=wn_sub(u->hp,k); if(wn_is_zero(u->hp)) sub_dies(f,u,inside); } }
    s->hp=wn_sub(s->hp,d);
    if(inside) f->kill.in_dmg=wn_add(f->kill.in_dmg,d); else { f->kill.out_dmg[w]=wn_add(f->kill.out_dmg[w],d); f->out_total=wn_add(f->out_total,d); }
    if(wn_is_zero(s->hp)){                                       // the section breaks: everything in it goes with it
        for(int i=0;i<f->c.nsub;i++){ WsSub *u=&f->c.sub[i]; if(u->alive && u->sec==f->sec){ u->alive=0; u->hp=wn(0); if(u->kind==WSS_GEN && !wf_alive(f,WSS_GEN)){ f->layers_up=0; f->lay_hp=wn(0); } } }
        char b[48]; snprintf(b,sizeof b,"%s BROKEN",s->name); say(f,b);
        f->sec++; if(f->sec>=f->c.nsec){ f->state=WF_WON; say(f,"IT BREAKS APART!"); }
    }
    set_phase(f);
}
void wf_sabotage(WsFight *f, int i, int precise){
    if(i<0||i>=f->c.nsub) return; WsSub *u=&f->c.sub[i]; if(!u->alive) return;
    int crit = u->kind==WSS_GEN || u->kind==WSS_REACTOR || u->kind==WSS_BRIDGE; f->kill.sab_all++; if(crit && precise) f->kill.sab_crit++;
    WsSec *s=&f->c.sec[u->sec]; WN d=u->hp; if(u->kind==WSS_REACTOR) d=wn_add(d,wn_muld(s->hp_max,0.2));     // a reactor takes a fifth of its section with it
    if(wn_cmp(d,s->hp)>0) d=s->hp; s->hp=wn_sub(s->hp,d); f->kill.in_dmg=wn_add(f->kill.in_dmg,d); f->nsab++;
    sub_dies(f,u,1);
    while(f->sec<f->c.nsec && wn_is_zero(f->c.sec[f->sec].hp)) f->sec++; if(f->sec>=f->c.nsec){ f->state=WF_WON; say(f,"IT BREAKS APART FROM INSIDE!"); }
    set_phase(f);
}
void wf_away(WsFight *f, double secs){
    if(f->state==WF_WON) return;
    if(f->gens0==0 || wf_alive(f,WSS_GEN)){ f->layers_up=f->c.layers; f->lay_hp=f->c.hp_shield; }
    if(f->sec<f->c.nsec){ f->c.sec[f->sec].hp=f->c.sec[f->sec].hp_max; }                  // the section you were cutting into is patched at once
    if(secs>90){                                                                            // after a long time away: all of it, and the systems too
        for(int i=0;i<f->c.nsec;i++) f->c.sec[i].hp=f->c.sec[i].hp_max; for(int i=0;i<f->c.nsub;i++){ f->c.sub[i].alive=1; f->c.sub[i].hp=f->c.sub[i].hp_max; }
        f->sec=0; f->layers_up=f->c.layers; f->lay_hp=f->c.hp_shield; memset(&f->kill,0,sizeof f->kill); f->kill.attack=f->ship.attack; f->out_total=wn(0); f->nsab=0; }
    f->fighters=0; f->repairs++; f->phase=0; set_phase(f); f->state=WF_ON; f->since_my_hit=0;
}
int wf_fire(WsFight *f, int w, int target){
    if(f->state!=WF_ON) return 0; if(w<0||w>=WW_N) w=0;
    if(f->overheat || f->fire_cd>0 || f->ammo[w]==0) return 0;
    f->fire_cd=1.0/rate_[w]; f->heat+=heat_[w]; if(f->heat>=HEAT_MAX){ f->heat=HEAT_MAX; f->overheat=1; say(f,"OVERHEAT! LET THE GUNS COOL"); }
    if(f->ammo[w]>0) f->ammo[w]--; f->shots++;
    if(w==WW_MISSILE||w==WW_TORPEDO){ double pd=0.5*(f->turrets0?(double)wf_alive(f,WSS_TURRET)/f->turrets0:0); if(fr01(f)<pd){ f->intercepted++; say(f,"POINT DEFENCE SHOT IT DOWN"); return 1; } }
    if(!f->ext && fr01(f)>=0.9) return 1;
    f->hits++; f->since_my_hit=0;
    if(target==-2 && f->fighters>0){ f->fighters--; return 2; }
    WN d=wn(ws_shot(f->ship.attack,w));
    if(f->layers_up>0){ WN pass=wn(0); if(w==WW_TORPEDO){ pass=wn_muld(d,0.5); d=wn_muld(d,0.5); }
        f->since_hit=0; if(wn_cmp(d,f->lay_hp)>=0){ f->layers_up--; f->lay_hp = f->layers_up ? f->c.hp_shield : wn(0); char b[40]; snprintf(b,sizeof b,"SHIELD LAYER DOWN: %d LEFT",f->layers_up); say(f,b); set_phase(f); f->sh_back_t=0; }
        else f->lay_hp=wn_sub(f->lay_hp,d);
        if(!wn_is_zero(pass)) hull_damage(f,pass,target,0,w); }
    else hull_damage(f,d,target,0,w);
    return 2;
}
void wf_tick(WsFight *f, const WsInput *in, double dt){
    if(f->state!=WF_ON) return;
    f->t+=dt; f->since_hit+=dt; f->since_my_hit+=dt; if(f->msg_t) f->msg_t--;
    if(in->retreat){ f->state=WF_FLED; say(f,"YOU PULL BACK. THE CREWS START REPAIRS"); return; }
    // ---- fuel: life support always, more when you fly hard. No fuel: you drift and every shot finds you
    if(!f->ext){ f->fuel -= dt*(in->boost ? 1.2 : 0.15); if(f->fuel<0) f->fuel=0; }
    // ---- heat
    f->heat -= COOL*dt; if(f->heat<0) f->heat=0; if(f->overheat && f->heat<=0) f->overheat=0;
    if(f->kit_cd>0) f->kit_cd-=dt; if(f->fire_cd>0) f->fire_cd-=dt;
    if(!f->ext && in->kit && f->kits>0 && f->kit_cd<=0 && f->hull<f->ship.hull_max){ f->kits--; f->hull+=40; if(f->hull>f->ship.hull_max) f->hull=f->ship.hull_max; f->kit_cd=5; }
    // ---- you shoot
    if(in->fire && f->fire_cd<=0) wf_fire(f,in->weapon,in->target);
    if(f->state!=WF_ON) return;
    // ---- the craft heals: shields (while a generator lives), and repair crews on the section you stopped cutting
    int gens=wf_alive(f,WSS_GEN), gens_ok = f->gens0==0 || gens>0;
    if(gens_ok){
        if(f->layers_up>0){ double k = f->since_hit>3 ? 0.09 : 0.03; f->lay_hp=wn_add(f->lay_hp,wn_muld(f->c.hp_shield,k*dt)); if(wn_cmp(f->lay_hp,f->c.hp_shield)>0) f->lay_hp=f->c.hp_shield; }
        if(f->layers_up<f->c.layers && f->since_hit>3){ f->sh_back_t+=dt; if(f->sh_back_t>=20){ f->sh_back_t=0; if(f->layers_up==0) f->lay_hp=wn_muld(f->c.hp_shield,0.25); f->layers_up++; say(f,"A SHIELD LAYER COMES BACK UP"); set_phase(f); } }
    }
    if(f->since_my_hit>6 && f->sec<f->c.nsec){ WsSec *s=&f->c.sec[f->sec]; if(wn_cmp(s->hp,s->hp_max)<0){ s->hp=wn_add(s->hp,wn_muld(s->hp_max,0.04*dt)); if(wn_cmp(s->hp,s->hp_max)>0) s->hp=s->hp_max;
        if(f->msg_t<30) say(f,"REPAIR CREWS PATCH THE HULL"); } }
    // ---- fighter waves from the hangars that still work
    int hang=wf_alive(f,WSS_HANGAR); f->fighter_t-=dt;
    if(f->fighter_t<=0){ double per = !wf_alive(f,WSS_BRIDGE) ? 45 : f->phase==4 ? 18 : 30; f->fighter_t=per;
        if(hang>0){ int n=(f->c.wave*hang+f->hangars0-1)/(f->hangars0?f->hangars0:1); if(f->ext){ f->spawn_req+=n; n=0; } f->fighters+=n; if(f->fighters>40) f->fighters=40; f->waves++; say(f,"FIGHTERS LAUNCHING!"); } }
    if(f->fighters>0 && f->wing>0){ double k=0.25*f->wing*dt; if(fr01(f)<k) f->fighters--; }
    // ---- it shoots: turrets (fewer as you break them), reactors feed them, the enraged core doubles it, fighters add theirs
    double tur = f->turrets0 ? 0.4+0.6*wf_alive(f,WSS_TURRET)/(double)f->turrets0 : 1.0, rea = f->reacs0 ? 0.7+0.3*wf_alive(f,WSS_REACTOR)/(double)f->reacs0 : 1.0;
    double dps = f->c.dps*tur*rea*(f->phase==4 && wf_alive(f,WSS_BRIDGE) ? 2.2 : 1.0) + f->fighters*0.18;
    double dodge = f->ext ? (in->nofuel ? 1.5 : in->boost ? 0.6 : 1.0) : (in->boost && f->fuel>0 ? 0.6 : f->fuel<=0 ? 1.5 : 1.0);
    double dmg = dps*dodge*dt;
    if(f->ext){ f->dmg_out+=dmg; return; }
    if(f->wing>0){ double share = dmg*f->wing/(f->wing+2.0)*0.5; dmg-=share; int k=(int)(fr(f)%(unsigned)f->wing); f->wing_hp[k]-=share*f->wing;
        if(f->wing_hp[k]<=0){ for(int j=k;j<f->wing-1;j++) f->wing_hp[j]=f->wing_hp[j+1]; f->wing--; say(f,"WINGMAN DOWN!"); } }
    dmg = dmg*f->ship.armour/100.0;
    if(f->shield>=dmg) f->shield-=dmg; else { f->hull-=dmg-f->shield; f->shield=0; }
    f->shield += dt*1.0; if(f->shield>f->ship.shield_max) f->shield=f->ship.shield_max;
    if(f->hull<=0){ f->hull=0; f->state=WF_LOST; say(f,"YOUR SHIP IS LOST"); }
}
void wf_bot(const WsFight *f, WsInput *in, int clever){
    memset(in,0,sizeof *in); in->target=-1; in->weapon=WW_LASER; in->fire=1; in->boost = f->fuel > f->ship.fuel_max*0.15;
    if(!clever) return;
    if(f->heat>HEAT_MAX-12 && !f->overheat) in->fire=0;                                // never overheat: stop a moment
    if(f->hull < f->ship.hull_max*0.5) in->kit=1;
    if(f->fighters>6 && f->wing<3) in->target=-2;
    for(int i=0;i<f->c.nsub;i++){ const WsSub *u=&f->c.sub[i]; if(u->alive && u->sec==f->sec && u->kind==WSS_TURRET && in->target==-1){ in->target=i; break; } }
}
