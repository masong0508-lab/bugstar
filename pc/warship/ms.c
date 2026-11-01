// ms.c: warship missions. See ms.h.
#include "ms.h"
#include <stdio.h>
#include <string.h>
static uint32_t mh(uint32_t a){ a^=a>>16; a*=0x7feb352dU; a^=a>>15; a*=0x846ca68bU; a^=a>>16; return a; }
static uint32_t mx(uint32_t a, uint32_t b){ return mh(a*0x9E3779B1u ^ mh(b+0x7F4A7C15u)); }
static const char *const KN[MK_N]={"DESTROY","SABOTAGE","STEAL PLANS","RESCUE","ESCORT","DEFEND","SURVEY"};
static const char *const KA[MK_N]={"SINK IT FROM OUTSIDE","BOARD IT AND BLOW A SYSTEM","BOARD IT, COPY ITS PLANS","BOARD IT, FREE THE CAPTIVES","KEEP A CONVOY ALIVE","HOLD A COLONY AGAINST IT","BOARD A DERELICT, SCAN THE CORE"};
static const char *const PN[MP_N]={"SPACE MENU","STATION","TOWN","OUTPOST"};
static const char *const FN[MS_NFAC]={"THE ASHEN LEDGER","VOSK FREE FLEET","THE QUIET CHOIR","HALLOW TRADE LEAGUE","THE BRASS COVENANT","KEL MERIDIAN","THE LAST HARVEST","ORRIN SALVAGE","THE PALE ACCORD","DUSKWATER CREW","THE LOW SUN GUILD","MARROW AND SONS"};
const char *ms_kind_name(int k){ return KN[(unsigned)k%MK_N]; }
const char *ms_kind_about(int k){ return KA[(unsigned)k%MK_N]; }
const char *ms_place_name(int p){ return PN[(unsigned)p%MP_N]; }
const char *ms_faction_name(int f){ return FN[(unsigned)f%MS_NFAC]; }
static const double SH[MK_N]={1.0,0.8,0.5,0.5,1.0,1.0,0.25};
double ms_share(int k){ return SH[(unsigned)k%MK_N]; }
int ms_inside(int k){ return k==MK_SABOTAGE||k==MK_STEAL||k==MK_RESCUE||k==MK_SURVEY; }
// how often each kind is offered at each kind of place
static const int BASE[MP_N][MK_N]={{3,2,2,2,2,2,2},{4,1,3,1,4,3,1},{1,2,1,4,2,4,3},{2,4,4,3,1,1,1}};
static const int OBJ[MK_N]={0,0,WSS_BRIDGE,WSS_TURRET,0,0,WSS_REACTOR};
void ms_offer(WsMission *m, const MsPlace *pl, int slot){
    memset(m,0,sizeof *m); uint32_t h=mx(pl->seed,(uint32_t)(slot*7919+pl->epoch*131+1));
    m->id=h|1; m->slot=(uint8_t)slot; m->place=(uint8_t)pl->kind;
    int w[MK_N], tot=0; uint32_t g1=mx(pl->gal,1)%MK_N, g2=mx(pl->gal,2)%MK_N;
    for(int k=0;k<MK_N;k++){ w[k]=BASE[pl->kind%MP_N][k]; if((uint32_t)k==g1||(uint32_t)k==g2) w[k]+=3; if(k==pl->theme%MK_N) w[k]+=2; if(pl->law>0&&k==MK_SABOTAGE) w[k]+=2; tot+=w[k]; }
    int r=(int)(mx(h,2)%(uint32_t)tot), k=0; while(r>=w[k]){ r-=w[k]; k++; } m->kind=(uint8_t)k; m->objsub=(uint8_t)OBJ[k];
    if(k==MK_SABOTAGE){ static const uint8_t t[3]={WSS_GEN,WSS_REACTOR,WSS_BRIDGE}; m->objsub=t[mx(h,3)%3]; }
    m->faction=(uint8_t)((mx(h,4)%100<45 ? mx(pl->seed,5) : mx(pl->gal,6))%MS_NFAC);
    m->dream=(uint8_t)(mx(h,7)%100 < (uint32_t)(10+pl->danger/2>30?30:10+pl->danger/2));
    int r2=(int)(mx(h,8)%1000); int zp=pl->zplayer;
    int z = m->dream ? zp+300+r2*9/10 : zp-500+r2*650/1000;
    if(z<150){ z=150; } if(z>WS_ZMAX_FAIR){ z=WS_ZMAX_FAIR; } m->z=z;
    if(slot==MS_SLOTS && pl->cheat_world){ m->z=3200+(int)(mx(h,9)%20000); m->cheat=1; m->dream=1; }
    m->cseed=mx(m->id,0xC4AF7)|1;
    m->far=(uint8_t)(mx(h,10)%100<18);
    if(m->far){ m->gal=mx(pl->gal,(uint32_t)slot+99)|1; if(m->gal==pl->gal) m->gal^=2; m->tsys=0; m->tidx=(int)(mx(h,11)%MS_NSYS); }
    else { m->gal=pl->gal; if(pl->npool>0){ m->tsys=pl->pool[mx(h,11)%(uint32_t)pl->npool]; } m->tidx=-1; }
    m->plan=1+(int)(mx(h,12)%8);
    m->dl_rel = (mx(h,13)%4==0) ? 8+(int)(mx(h,14)%12) : 0;
    m->bonus = 10+(int)(mx(h,15)%40)+(m->dream?30:0)+(k==MK_SABOTAGE?10:0)+(m->dl_rel?15:0)+(pl->law>0?10:0);
}
void ms_giver(const WsMission *m, char *out, int cap){
    static const char *const A[]={"VO","KE","AL","MA","RIN","TOS","HAL","SU","DRE","ORI","NAV","PEL"}; static const char *const B[]={"RA","LEN","MIR","DOK","VEL","SHA","KOR","NEL","TU","BAS"}; static const char *const C[]={"","N","S","K","TH","M"};
    uint32_t h=mx(m->id,0x61); snprintf(out,(size_t)cap,"%s%s%s",A[h%12],B[(h>>8)%10],C[(h>>16)%6]);
}
void ms_line(const WsMission *m, const char *craft, int row, char *o, int cap){
    static const char *const L[MK_N][3]={
      {"%s IS RAIDING THE LANES.","SINK IT FROM OUTSIDE. NO BOARDING NEEDED.","THE HULL MUST BREAK. ANY WEAPON PAYS."},
      {"%s HAS A ","BOARD IT AND BLOW ITS %s.","PLANT THE CHARGE AND GET OUT. PAID ON LEAVING."},
      {"%s CARRIES OUR STOLEN PLANS.","BOARD, REACH THE BRIDGE, STAY 3 SECONDS.","THE COPY IS PAID WHEN YOU LEAVE THE CRAFT."},
      {"%s HOLDS OUR PEOPLE IN ITS GUN DECK.","BOARD, REACH A TURRET ROOM, STAY 3 SECONDS.","GUARDS FIGHT. THE CAPTIVES PAY ON YOUR RETURN."},
      {"A CONVOY CROSSES NEAR %s.","TWO CONVOY ESCORTS FLY WITH YOU IN THE FIGHT.","SINK THE WARSHIP BEFORE IT SINKS THE CONVOY."},
      {"%s FALLS ON A COLONY.","THE ARMADA IS YOURS TO BREAK.","SINK IT. THE COLONY LIVES AND PAYS."},
      {"%s DRIFTS DEAD AND DARK.","BOARD, REACH THE REACTOR, STAY 3 SECONDS.","LOG THE CORE. PAID WHEN YOU LEAVE THE CRAFT."}};
    char b[96]; if(row==1 && m->kind==MK_SABOTAGE){ snprintf(b,sizeof b,"BOARD IT AND BLOW ITS %s.",ws_sub_name(m->objsub)); }
    else if(row==0 && m->kind==MK_SABOTAGE){ snprintf(b,sizeof b,"%s IS A THREAT TO THE LANES.",craft); }
    else if(row==0){ snprintf(b,sizeof b,L[m->kind%MK_N][0],craft); } else snprintf(b,sizeof b,"%s",L[m->kind%MK_N][row]);
    snprintf(o,(size_t)cap,"%s",b);
}
void ms_pay(const WsCraft *c, const WsKill *k, double attack, const WsMission *m, WsPay *out){
    WsKill kk; if(k && !ms_inside(m->kind)) kk=*k; else { memset(&kk,0,sizeof kk); kk.attack=attack; kk.in_dmg=wn_muld(c->hp_hull,0.3); kk.sab_crit=1; kk.sab_all=2; }
    ws_pay(c,&kk,out); out->reward=wn_muld(out->reward,ms_share(m->kind)*(1.0+m->bonus/100.0)); if(m->cheat) out->cheat=1;
}
int ms_pack(const WsMission *m, char *o, int cap){
    return snprintf(o,(size_t)cap,"%u %u %d %d %d %d %d %d %d %d %d %d %d %d %u %u %d %d %d %d",m->id,m->cseed,m->z,m->cheat,m->kind,m->place,m->dream,m->far,m->objsub,m->faction,m->slot,m->state,m->prog,m->bonus,m->gal,m->tsys,m->tidx,m->plan,m->dl_rel,m->dl_abs);
}
int ms_unpack(const char *s, WsMission *m){
    unsigned id,cs,gal,ts; int z,ch,ki,pl,dr,fa,os,fc,sl,st,pg,bo,ti,pn,dr2,da;
    if(sscanf(s,"%u %u %d %d %d %d %d %d %d %d %d %d %d %d %u %u %d %d %d %d",&id,&cs,&z,&ch,&ki,&pl,&dr,&fa,&os,&fc,&sl,&st,&pg,&bo,&gal,&ts,&ti,&pn,&dr2,&da)!=20) return 0;
    if(ki<0||ki>=MK_N||z<WS_ZMIN||z>WS_ZMAX) return 0;
    memset(m,0,sizeof *m); m->id=id; m->cseed=cs; m->z=z; m->cheat=ch!=0; m->kind=(uint8_t)ki; m->place=(uint8_t)(pl&3); m->dream=(uint8_t)dr; m->far=(uint8_t)fa; m->objsub=(uint8_t)os; m->faction=(uint8_t)(fc%MS_NFAC); m->slot=(uint8_t)sl; m->state=(uint8_t)st; m->prog=(uint8_t)pg;
    m->bonus=bo; m->gal=gal; m->tsys=ts; m->tidx=ti; m->plan=pn<1||pn>8?1:pn; m->dl_rel=dr2; m->dl_abs=da; return 1;
}
