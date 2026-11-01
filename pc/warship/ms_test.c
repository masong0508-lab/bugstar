// ms_test.c: quick checks for warship missions (pc/warship/ms.c). Run by test.sh.
#include "ms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fails=0, checks=0;
#define CHECK(c, ...) do { checks++; if(!(c)){ fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while(0)
static int font_ok(const char *s){ for(;*s;s++){ char c=*s; if(!((c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='.'||c==','||c=='\''||c==':'||c=='('||c==')'||c=='-'||c=='^')) return 0; } return 1; }
int main(void){
    int seen[MP_N][MK_N]; memset(seen,0,sizeof seen); int dreams=0, fars=0, total=0, galdiff=0;
    for(int pk=0;pk<MP_N;pk++) for(uint32_t s=1;s<=400;s++){ MsPlace pl; memset(&pl,0,sizeof pl); pl.seed=s*2654435761u; pl.gal=(s%3)?0:s*77u|1; pl.kind=pk; pl.danger=6+(int)(s%20); pl.zplayer=900; pl.theme=(int)(s%11); pl.npool=5; for(int i=0;i<5;i++) pl.pool[i]=100+i;
        for(int sl=0;sl<MS_SLOTS;sl++){ WsMission a,b; ms_offer(&a,&pl,sl); ms_offer(&b,&pl,sl); total++;
            CHECK(!memcmp(&a,&b,sizeof a),"offer not deterministic"); seen[pk][a.kind]++; dreams+=a.dream; fars+=a.far;
            CHECK(a.z>=150 && a.z<=WS_ZMAX_FAIR && !a.cheat,"size %d",a.z); CHECK(a.plan>=1&&a.plan<=8,"planet");
            if(s<=40 && sl==0){ WsCraft c,c2; ws_make(&c,a.cseed,a.z,0); ws_make(&c2,a.cseed,a.z,0); CHECK(!strcmp(c.name,c2.name) && wn_cmp(c.hp_hull,c2.hp_hull)==0,"craft differs");
                char g[32], ln[100]; ms_giver(&a,g,sizeof g); CHECK(font_ok(g),"giver '%s'",g); for(int r=0;r<3;r++){ ms_line(&a,c.name,r,ln,sizeof ln); CHECK(font_ok(ln)&&strlen(ln)<=58,"line '%s'",ln); }
                char pk2[256]; WsMission u; ms_pack(&a,pk2,sizeof pk2); CHECK(ms_unpack(pk2,&u) && !memcmp(&a,&u,sizeof a),"pack round trip");
                WsKill k; memset(&k,0,sizeof k); k.attack=50; k.out_dmg[0]=c.hp_hull; WsPay p,q; ws_pay(&c,&k,&p); a.bonus=0; ms_pay(&c,&k,50,&a,&q);
                CHECK(wn_cmp(q.reward,p.reward)<=0,"mission pay %d above plain pay",a.kind); a.bonus=40; ms_pay(&c,&k,50,&a,&q); if(!ms_inside(a.kind)) CHECK(wn_cmp(q.reward,p.reward)>0,"bonus adds nothing"); } }
        MsPlace o=pl; o.gal^=0x5555; WsMission x,y; ms_offer(&x,&pl,0); ms_offer(&y,&o,0); galdiff+=(x.kind!=y.kind); }
    for(int pk=0;pk<MP_N;pk++){ printf("%-10s",ms_place_name(pk)); for(int k=0;k<MK_N;k++) printf(" %s %d",ms_kind_name(k),seen[pk][k]); printf("\n"); }
    int distinct=0; for(int pk=0;pk<MP_N;pk++) for(int k=0;k<MK_N;k++) CHECK(seen[pk][k]>0,"kind %d never at place %d",k,pk);
    CHECK(dreams>total/20 && dreams<total/2,"dream share %d of %d",dreams,total); CHECK(fars>total/20 && fars<total/3,"far share %d",fars); CHECK(galdiff>200,"galaxies make too similar boards %d",galdiff); (void)distinct;
    printf("missions: %d checks, %d failures (%d offers, %d dream jobs, %d far galaxy)\n",checks,fails,total,dreams,fars); return fails?1:0;
}
