// ms_check.c: the payout of one mission, worked out by itself, for tests/mission.sh: the game says (craft seed, size, kind, bonus, attack, outside damage) and what it paid; this
// program rebuilds the craft and applies the formula by hand (ws_pay for the craft and the kill, times the kind's share, times 1 + bonus/100) and prints the same number.
//   ms_check SEED Z KIND BONUS ATTACK OUT_M OUT_E     ->  "reward M E" (the numbers the game logs)
#include "ms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv){
    if(argc < 8) return 2;
    uint32_t seed=(uint32_t)strtoul(argv[1],0,10); int z=atoi(argv[2]), kind=atoi(argv[3]), bonus=atoi(argv[4]); double attack=atof(argv[5]); WN out; out.m=atof(argv[6]); out.e=atoll(argv[7]);
    WsCraft c; ws_make(&c,seed,z,0); WsKill k; memset(&k,0,sizeof k); k.attack=attack;
    if(ms_inside(kind)){ k.in_dmg=wn_muld(c.hp_hull,0.3); k.sab_crit=1; k.sab_all=2; } else k.out_dmg[0]=out;
    WsPay p; ws_pay(&c,&k,&p); double r=wn_to_d(p.reward)*ms_share(kind)*(1.0+bonus/100.0);
    int e=0; while(r>=10){ r/=10; e++; } while(r>0 && r<1){ r*=10; e--; }
    printf("reward %.17g %d\n",r,e); return 0;
}
