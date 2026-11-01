// pc/coil_test.inc: appended to the game at the end by build.sh. Self-test of the Stormclock Coil's rules (run: BUGSTAR_COILTEST=1 ./bugstar_pc).
// It runs before the fake GBA starts, so it only touches the pure rules: the drop roll, the six conditions, the speed maths and the text.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int ct_fail=0;
#define CT(c,msg) do { if(!(c)){ printf("FAIL: %s\n",msg); ct_fail++; } } while(0)
void pc_coil_report(int n, int sr){ printf("coilfuzz: %d frames, save block test %s\n",n,sr?"FAILED":"ok"); fflush(stdout); }
static void ct_setup(int bits){                     // make the world match exactly the conditions in 'bits' (bit set = true)
    battle_on=(bits&1)?1:0; battle_kills=(bits&1)?5:4;
    clus[0]=(bits&2)?1:0; galaxy_no=0; clus_up=0; for(int i=0;i<NGH;i++) galaxy_hi[i]=0;
    cur_sys=0; sys[0].kind=(bits&4)?6:2; beast_stage=0;
    shield=(bits&8)?0:5; hull=(bits&8)?25:80;
    fuel=(bits&16)?0:fuel_max(); speed=(bits&32)?10000:0; }
__attribute__((constructor)) static void pc_coil_selftest(void){
    if(!getenv("BUGSTAR_COILTEST")) return;
    // 1. the drop rule: a full truth table
    int rolls=0, hits=0;
    for(int qd=0;qd<2;qd++) for(int own=0;own<2;own++) for(int mask=0;mask<64;mask++) for(int force=0;force<2;force++) for(int r=0;r<3;r++){
        u32 rv = r==0 ? 0u : r==1 ? 1u : 20000u;                                // 0 and 20000 are winning rolls (a multiple of 10,000), 1 is not
        int want = qd && !own && mask==63 && (force || rv%10000u==0);
        CT(coil_drop_roll(qd,mask,own,rv,force)==want,"coil_drop_roll truth table"); }
    for(u32 r=0;r<100000u;r++){ rolls++; hits+=coil_drop_roll(1,63,0,r,0); }
    CT(hits==10 && rolls==100000,"exactly 1 in 10,000 of the rolls win");
    // 2. the six conditions, one at a time: with any single one false, the mask is never 63, so nothing can drop, even with the cheat that forces the roll
    ct_setup(63); CT(coil_cond_mask()==63,"all six conditions can hold together");
    for(int k=0;k<6;k++){ ct_setup(63&~(1<<k)); int m=coil_cond_mask(); CT(m==(63&~(1<<k)),"each condition sets its own bit"); CT(!coil_drop_roll(1,m,0,0,1),"one condition missing: no drop even when forced"); }
    ct_setup(63); CT(coil_drop_roll(1,coil_cond_mask(),0,0,0)==1,"all true and a winning roll: it drops");
    CT(coil_drop_roll(0,coil_cond_mask(),0,0,1)==0,"quest not done: it never drops");
    CT(coil_drop_roll(1,coil_cond_mask(),1,0,1)==0,"already owned: it never drops again");
    // 3. coil_try_drop only counts a try when the quest is done and all six hold
    coil_own=0; coil_qstep=0; dbg_force=0; dbg_qdone=0; coil_rolls=0;
    ct_setup(63); coil_try_drop(); CT(coil_rolls==0,"quest not done: no try counted");
    coil_qstep=NQS; for(int k=0;k<6;k++){ ct_setup(63&~(1<<k)); coil_try_drop(); } CT(coil_rolls==0 && !coil_own,"a condition missing: no try counted, no drop");
    // 4. the speed ladder: no overflow at the top, and the right glitch tiers
    dbg_sm=0; coil_cur=0; int mi; CT(coil_gt(coil_total_c(0,30,&mi))==0,"standing still: tier 0");
    for(int t=1;t<=NCT;t++){ coil_cur=coil_mult[t]; u64 w=coil_total_c(50,50,&mi); CT(w==coil_mult[t],"the speed is the tier"); CT(coil_gt(w)==t,"each tier has its glitch tier"); }
    coil_cur=coil_mult[NCT]; dbg_sm=5; u64 top=coil_total_c(50,50,&mi); CT(top==13000000000000ULL*1000000ULL,"the cheat can pass the limit: 13 trillion x 1 million"); CT(coil_gt(top)==7,"beyond the limit is tier 7");
    CT(top/1000000ULL==13000000000000ULL,"it did not wrap round");
    dbg_sm=0; coil_cur=0;
    char b[96]; coil_num(b,13000000000000ULL); CT(!strcmp(b,"13 TRILLION"),"13 TRILLION says so"); coil_num(b,1000ULL); CT(!strcmp(b,"1,000"),"1,000 has its comma");
    coil_num(b,0xFFFFFFFFFFFFFFFFULL); CT(strlen(b)<30,"the biggest number can be said");
    coil_time(b,132556800ULL,0,1); CT(!strcmp(b,"4 YEARS"),"4.2 light years at light speed take 4 years"); coil_time(b,132556800ULL,0,13000000000000ULL); CT(strstr(b,"MICROSECONDS")!=0,"at the limit it takes microseconds");
    // 5. the texts never overflow their buffers, whatever the numbers
    u32 s=12345; for(int i=0;i<200000;i++){ s=s*1664525u+1013904223u; u64 n=((u64)s<<32)|(s*2654435761u); u64 M=((u64)(s*7919u)<<(s>>27))|1ULL; if(i&1) n>>=(s>>20)&63; coil_time(b,n,(int)(s%7),M); CT(strlen(b)<60,"coil_time fits its line"); coil_num(b,n); CT(strlen(b)<40,"coil_num fits its line"); }
    printf("coil self-test: %s (%d checks failed)\n", ct_fail?"FAILED":"ok", ct_fail); fflush(stdout); exit(ct_fail?1:0); }
