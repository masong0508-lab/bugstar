// pc/coil_test.inc: appended to the game at the end by build.sh. Self-test of the Stormclock Coil's rules (run: BUGSTAR_COILTEST=1 ./bugstar_pc).
// It runs before the fake GBA starts, so it only touches the pure rules: the drop roll, the six conditions, the speed maths and the text.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tests/cn_check.inc"
static int ct_fail=0;
#define CT(c,msg) do { if(!(c)){ printf("FAIL: %s\n",msg); ct_fail++; } } while(0)
void pc_coil_report(int n, int sr){ printf("coilfuzz: %d frames, save block test %s\n",n,sr?"FAILED":"ok"); printf("coil fuzz coverage: %d passes, kinds mask %d of 126, highest heat %d, lowest hull %d, highest glitch tier %d\n",fz_pass,fz_kinds,fz_heat,fz_hmin,fz_gmax); fflush(stdout); }
static void ct_setup(int bits){                     // make the world match exactly the conditions in 'bits' (bit set = true)
    battle_on=(bits&1)?1:0; battle_kills=(bits&1)?5:4;
    clus[0]=(bits&2)?1:0; galaxy_no=0; clus_up=0; for(int i=0;i<NGH;i++) galaxy_hi[i]=0;
    cur_sys=0; sys[0].kind=(bits&4)?6:2; beast_stage=0;
    shield=(bits&8)?0:5; hull=(bits&8)?25:80;
    fuel=(bits&16)?0:fuel_max(); speed=(bits&32)?10000:0; }
// the pass-through soak (needs the cartridge, so the game's TEST_COIL start-up calls it, not the constructor): "pass-through, at the worst speed"
int pc_coil_soak(void){ int f0_=ct_fail;
    // pass-through, at the worst speed: in every mode and strength the hull never goes under 10, the heat stays 0 to 100, VISUAL ONLY and OFF cost nothing, and nothing but REDUCED/FULL touches fuel
    { u8 xp=xo[XO_PASS], xd=xo[XO_PDMG], xv=xo[XO_PVIS]; int worst_hull=1000, bad=0; dbg_god=0;
      for(int pm=0;pm<4;pm++) for(int pd=0;pd<4;pd++){ xo[XO_PASS]=(u8)(pm^XOT[XO_PASS].def); xo[XO_PDMG]=(u8)(pd^XOT[XO_PDMG].def); xo[XO_PVIS]=0;
        for(int start=0;start<2;start++){ pt_reset(); coil_cur=coil_mw[NCT]; coil_tier=NCT; fuel=1000; hull=start?10:100; shield=0; int f0=fuel, passes=0; u32 p0=coil_passes;
            for(int f=0;f<60000;f++){ fx_t=f; int hb=hull; pt_frame(coil_mw[NCT],7); if(pt_kind && pt_t==1) passes++;
                if(hull<hb && hb<=10) bad|=1; if(hull<10 && hb>=10) bad|=1; if(pt_heat>100) bad|=2; if(hull<worst_hull) worst_hull=hull; if(hull>100) bad|=4; if(shield<0 || fuel<0) bad|=8;
                if(cw_zero(coil_cur)){ coil_cur=coil_mw[NCT]; coil_tier=NCT; pt_heat=0; shield=0; fuel=1000; } }          // (an overheat dropped the coil out: engage again)
            if(pm>=2 || pt_dmg()==0){ if(hull!=(start?10:100) || shield!=0 || fuel!=f0 || pt_heat!=0) bad|=16; }
            if(pm==3 && coil_passes!=p0) bad|=32;                                                   // OFF: nothing at all happens
            if(pm<2 && pd==0 && coil_passes==p0) bad|=64; (void)passes; } }
      CT(bad==0,"pass-through keeps its promises in every mode"); CT(worst_hull>=10,"the hull is never taken below 10");
      xo[XO_PASS]=xp; xo[XO_PDMG]=xd; xo[XO_PVIS]=xv; pt_reset(); coil_cur=cw_u64(0); coil_tier=0; }
    printf("coil soak: %s\n", ct_fail==f0_ ? "ok" : "FAILED"); return ct_fail!=f0_; }
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
    dbg_sm=0; coil_cur=cw_u64(0); int mi; CT(coil_gt(coil_total_c(0,30,&mi))==0,"standing still: tier 0");
    for(int t=1;t<=NCT;t++){ coil_cur=coil_mw[t]; CW w=coil_total_c(50,50,&mi); CT(cw_cmp(w,coil_mw[t])==0,"the speed is the tier"); CT(coil_gt(w)==t,"each tier has its glitch tier"); CT(t==1 || cw_cmp(coil_mw[t],coil_mw[t-1])>0,"the tiers rise"); }
    CT(NCT==7 && cw_cmp(coil_mw[7],cw_make(23759,39))==0,"STORM is exactly 2.3759 x 10^43"); CT(coil_mw[6].e==0 && coil_mw[6].m==13000000000000ULL,"LIMIT is still 13 trillion");
    CT(coil_gt(cw_u64(13000000000001ULL))==6 && coil_gt(cw_make(2375899999ULL,34))==6,"between LIMIT and STORM is still the LIMIT show"); CT(coil_gt(cw_make(2375900001ULL,34))==8,"just past STORM is beyond");
    coil_cur=coil_mw[NCT]; dbg_sm=5; CW top=coil_total_c(50,50,&mi); CT(cw_cmp(top,cw_make(23759,45))==0,"the cheat can pass STORM: x 1 million = 2.3759 x 10^49"); CT(coil_gt(top)==8,"beyond STORM is tier 8");
    dbg_sm=0; coil_cur=cw_u64(0);
    CT(cw_cmp(coil_mw[1],cw_u64(1))==0 && coil_total_c(50,50,&mi).m==0 && mi==600,"full throttle without the coil is 0.6 c");
    // 5. the wide numbers: every result stays in the one canonical form, whatever is added, taken away, multiplied or divided (and nothing wraps)
    { u32 s=777; CW x=cw_u64(1); int lowest=0; for(int i=0;i<300000;i++){ s=s*1664525u+1013904223u; u32 k=(s>>8)%1000000000u+1; CW y=x;
        switch((s>>5)&7){ case 0: x=cw_mulu(x,(s>>12)%1000+1); break; case 1: x=cw_divu(x,(s>>12)%1000+1); break; case 2: x=cw_add(x,cw_divu(x,12)); break; case 3: x=cw_sub(x,cw_divu(x,4)); break;
            case 4: x=cw_add(x,cw_u64(k)); break; case 5: x=cw_mulu(x,k); break; case 6: x=cw_divu(x,k); break; default: x=cw_u64(k); break; }
        int ok = x.m==0 ? x.e==0 : x.e==0 ? x.m<10000000000000000000ULL : (x.e>=10 && x.m>=1000000000ULL && x.m<10000000000ULL);
        if(!ok){ CT(0,"a wide number left its canonical form"); break; }
        if(((s>>5)&7)==0 || ((s>>5)&7)==2 || ((s>>5)&7)==5) CT(cw_cmp(x,y)>=0 || y.m==0,"growing never shrinks"); if(x.m==0) lowest++; if(x.e>1000000) x=cw_u64(1); }
      CT(lowest>=0,"ran"); }
    { CW a=cw_u64(0xFFFFFFFFFFFFFFFFULL); CW b=cw_add(a,a); CT(b.e>=10 && cw_cmp(b,a)>0,"adding two big u64 does not wrap"); CW c2=cw_mulu(a,1000000000u); CT(c2.e>=10 && cw_cmp(c2,a)>0,"multiplying a big u64 does not wrap"); CT(cw_cmp(cw_sub(a,b),cw_u64(0))==0,"taking more than there is gives zero"); }
    { CW r=cw_u64(1); int frames=0; while(cw_cmp(r,coil_mw[7])<0 && frames<5000){ CW st=cw_add(cw_divu(r,cw_cmp(r,coil_mw[6])<0?12:2),cw_u64(1)); r=cw_add(r,st); if(cw_cmp(r,coil_mw[7])>0) r=coil_mw[7]; frames++; }
      CT(cw_cmp(r,coil_mw[7])==0 && frames<1500,"the ramp from 1 c reaches STORM in under 25 seconds"); }
    // 6. the texts: the tiers, in the game's own words, and then every line of the Python reference (BUGSTAR_COILREF=file)
    char b[160];
    b[0]=0; coil_fmt(b,coil_mw[7],0); CT(!strcmp(b,"2.3759 X 10^43 X LIGHT"),"STORM reads 2.3759 X 10^43 X LIGHT");
    b[0]=0; coil_fmt(b,coil_mw[6],0); CT(!strcmp(b,"13,000,000,000,000 X LIGHT"),"LIMIT reads in full, with commas"); b[0]=0; coil_fmt(b,cw_u64(0),420); CT(!strcmp(b,"0.42 X LIGHT"),"0.42 X LIGHT below light speed");
    b[0]=0; coil_fmt(b,cw_u64(12345),0); CT(!strcmp(b,"12,345 X LIGHT"),"12,345 is exact, not rounded to words");
    b[0]=0; cn_words(b,cs_from_cw(coil_mw[7])); CT(!strcmp(b,"23.759 TREDECILLION"),"STORM in words"); b[0]=0; cn_words(b,cs_from_cw(coil_mw[6])); CT(!strcmp(b,"13 TRILLION"),"LIMIT in words");
    coil_trip(b,CT_MOON,0,cs_from_cw(coil_mw[7])); CT(!strcmp(b,"1 PLANCK TIME"),"the Moon at STORM is one Planck time away"); coil_trip(b,CT_N,1,cs_from_cw(coil_mw[7])); CT(!strcmp(b,"LESS THAN ONE PLANCK TIME"),"1 game unit (2,000 km) at STORM is under a Planck time");
    coil_trip(b,CT_PROX,0,cs_u(1)); CT(!strcmp(b,"4.25 YEARS"),"Proxima at light speed: 4.25 years"); coil_trip(b,CT_UNI,0,cs_from_cw(coil_mw[6])); CT(!strcmp(b,"2.61 DAYS"),"the universe at LIMIT: 2.61 days");
    coil_trip(b,CT_GAME,0,cs_from_cw(coil_mw[7])); CT(!strcmp(b,"48,100 YEARS"),"even at STORM the game map takes 48,100 years");
    cn_time(b,cs_make(1,-60)); CT(!strcmp(b,"LESS THAN ONE PLANCK TIME"),"1e-60 s is under a Planck time"); cn_time(b,cs_make(128225,-5)); CT(!strcmp(b,"1.28 SECONDS"),"the Moon at light speed"); cn_time(b,cs_make(1,-12)); CT(!strcmp(b,"1 PICOSECOND"),"a picosecond");
    { const char *ref=getenv("BUGSTAR_COILREF"); if(ref){ int total=0; int bad=cnc_run(ref,&total); CT(bad==0 && total>5000,"every number matches the Python reference (pc/tests/coilref.py)"); printf("coil numbers vs Python reference: %d cases, %d differ\n",total,bad); } }
    // 7. the texts never overflow their buffers, whatever the numbers (200,000 random speeds, times and trips)
    u32 s=12345; for(int i=0;i<200000;i++){ s=s*1664525u+1013904223u; u64 n=((u64)s<<32)|(s*2654435761u); CS c=cs_make(n>>(s>>26),(int)(s%140)-70); if(!c.m) continue; CS t=cs_make((u64)s*7919u+1000000000ULL,(int)((s>>8)%200)-110);
        b[0]=0; cn_time(b,t); CT(strlen(b)<60,"cn_time fits its line"); b[0]=0; cn_sn(b,c); CT(strlen(b)<40,"cn_sn fits its line"); b[0]=0; cn_words(b,c); CT(strlen(b)<60,"cn_words fits its line");
        b[0]=0; cn_rate_text(b,(int)(s%CR_N),c); CT(strlen(b)<90,"cn_rate_text fits its line"); b[0]=0; cn_trip_text(b,(int)((s>>4)%CT_N),c); CT(strlen(b)<60,"cn_trip_text fits its line");
        CW w=cw_make(n>>(s>>27),(int)(s%60)); b[0]=0; coil_fmt(b,w,0); CT(strlen(b)<50,"coil_fmt fits its line"); }
    // 8. the last step is locked until LIMIT was held for 30 seconds (a cheat-lent coil skips the lock)
    dbg_coil=0; coil_limit_s=0; CT(!coil_storm_open(),"STORM is locked at the start"); coil_limit_s=COIL_GATE_S-1; CT(!coil_storm_open(),"29 seconds is not enough"); coil_limit_s=COIL_GATE_S; CT(coil_storm_open(),"30 seconds opens STORM");
    coil_limit_s=0; dbg_coil=1; CT(coil_storm_open(),"a cheat-lent coil skips the lock"); dbg_coil=0;
    printf("coil self-test: %s (%d checks failed)\n", ct_fail?"FAILED":"ok", ct_fail); fflush(stdout); exit(ct_fail?1:0); }
