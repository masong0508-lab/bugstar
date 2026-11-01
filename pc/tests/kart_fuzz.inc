// pc/tests/kart_fuzz.inc: appended to the game by build.sh. With -DTEST_KFUZZ=<frames per race> the game starts Kart World races, race after race, on many
// planets, themes, tracks, classes, rival counts and race kinds (quick, time attack, elimination, tour), with fuzzed key presses for tens of thousands of frames,
// and checks: no hang inside a frame (a watchdog alarm), no race that never ends, no rival stuck for ever, lap counters and path points always in range and only
// ever moving by one, the player's own lap logic (the test driver must be able to finish every track). One line per race; "KFUZZ BAD" lines are failures.
#ifdef TEST_KFUZZ
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
static int kfz_bad=0, kfz_race=0; static char kfz_what[160];
static void kfz_alarm(int s){ (void)s; printf("KFUZZ HANG: a single frame took over 20 s (race %d: %s)\n",kfz_race,kfz_what); fflush(stdout); _exit(3); }
#define KFZ_BAD(...) do { printf("KFUZZ BAD race %d (%s): ",kfz_race,kfz_what); printf(__VA_ARGS__); printf("\n"); fflush(stdout); kfz_bad++; } while(0)
static u32 kfz_s=12345; static u32 kfz_rnd(void){ kfz_s=kfz_s*1664525u+1013904223u; return kfz_s>>8; }
static void kfz_run(void){
    int maxf=TEST_KFUZZ; long races=getenv("KFUZZ_RACES")?atol(getenv("KFUZZ_RACES")):40; u32 first=getenv("KFUZZ_FIRST")?(u32)atol(getenv("KFUZZ_FIRST")):0;
    signal(SIGALRM,kfz_alarm);
    kw_load(); kw_mode=1; kw.unlocked=1; kw.cls_open=5;
    int tot_done=0, tot_to=0;
    for(u32 n=first;n<first+(u32)races;n++){
        kfz_race=(int)n; kfz_s=n*7919u+1;
        kw.laps=(u8)(1+n%3); kw.seed=hmix(n*2654435761u+17u)%900000000u; kw.track=hmix(n*40503u+9u)%KW_TRACKS; kw.mirror=(u8)((n>>3)&1); kw.reverse=(u8)((n>>4)&1); kw.stunts=(u8)(n%3==2?0:1);
        kw.items=(u8)(n%3); kw.catchup=(u8)((n>>2)&1); kw.skill=(u8)(n%4); kw.weather=(u8)(n%4==3?1+(n/4)%3:0); kw.hour=(u8)((n%5==4)?1+(n/5)%24:0);
        k_cls=(int)(n%5); int kind=(int)(n%9); /* 0..4 quick, 5 time attack, 6 elimination, 7 tour, 8 quick with 3 rivals */
        int mode=(int)((n/3)%4);                 /* 0 test driver, 1 test driver + noise, 2 mash, 3 hands off */
        kw_quit=0; kw_tour=(int)(n%NKWT); kfz_auto=0;
        snprintf(kfz_what,sizeof kfz_what,"seed %u track %u cls %d kind %d mode %d laps %d",(unsigned)kw.seed,(unsigned)kw.track,k_cls,kind,mode,kw.laps);
        alarm(60);
        if(kind==7){ if(kw_tour>=NKWT) kw_tour=0; k_cls=(int)(n%3); kw_tour_start(); }
        else kw_start_planet_race(kw.seed,kw.track, kind==5?1: kind==6||kind<5?NRC:4, kind==5?2: kind==6?6:0);
        if(kind==6){ k_elim_t=0; k_elim_out=0; }
        int shape=kt.shape, laps=kt.laps, np=knp, gates=kt.n, done_f=-1, nrace=0, quiet=0;
        int lastpi[NRC], lastchg[NRC], lastlap[NRC], stuckrep[NRC]; for(int r=0;r<NRC;r++){ lastpi[r]=rc[r].pi; lastchg[r]=0; lastlap[r]=0; stuckrep[r]=0; }
        int loopn=0, airn=0, lpr=0, apr=0;
        int held=0, hold_t=0; int race_f=0, ncase=0;
        int f;
        for(f=0;!kw_quit && f<maxf;f++){
            alarm(20);
            read_keys();
            if(mode==2){ if(hold_t--<=0){ held=(int)(kfz_rnd()&0x3FF)&~(K_START|K_SEL|K_B*0); held&=~(K_START|K_SEL); if(kfz_rnd()&1) held|=K_UP; hold_t=1+(int)(kfz_rnd()%30); } keys_old=keys; keys=(u32)held; }
            else if(mode==3){ keys_old=keys; keys=0; }
            else { kfz_auto=1; if(mode==1){ if(hold_t--<=0){ held=(int)(kfz_rnd()&(K_A|K_B|K_R|K_L|K_SEL)); if(kfz_rnd()%3) held=0; hold_t=1+(int)(kfz_rnd()%40); } keys_old=keys; keys=(u32)held; } else { keys_old=keys; keys=0; } }
            frames++; if(invuln) invuln--; rng_s+=keys; planet_frame(); vsync();
            if(m7_on){ oam_flush(); m7_after_vsync(); } else { REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(page_back?(1<<4):0); page_back^=1; oam_flush(); }
            if(kr_mode!=1 || nrc<1) continue;
            if(ks_state==KS_COUNT){ laps=kt.laps; np=knp; for(int r=0;r<NRC;r++){ lastlap[r]=0; lastpi[r]=rc[r].pi; lastchg[r]=nrace; stuckrep[r]=0; } }
            if(ks_state==KS_RACE) nrace++;
            for(int r=0;r<nrc;r++){ Racer *k=&rc[r];
                if(k->pi<0 || k->pi>=knp){ KFZ_BAD("racer %d path point %d outside 0..%d at frame %d",r,k->pi,knp-1,f); k->pi=0; }
                if(k->lap>laps+1){ KFZ_BAD("racer %d lap counter %d with only %d laps at frame %d",r,k->lap,laps,f); k->lap=(u8)(laps+1); }
                if(k->lap<lastlap[r]){ KFZ_BAD("racer %d lap counter went DOWN %d -> %d at frame %d",r,lastlap[r],k->lap,f); }
                if(k->lap>lastlap[r]+1){ KFZ_BAD("racer %d lap counter jumped %d -> %d in one frame at frame %d",r,lastlap[r],k->lap,f); }
                lastlap[r]=k->lap;
                if(r && ks_state==KS_RACE && !k->fin && !k->out && !k->la){ if(k->pi!=lastpi[r]){ lastpi[r]=k->pi; lastchg[r]=nrace; } else if(nrace-lastchg[r]>900 && !stuckrep[r]){ stuckrep[r]=1; KFZ_BAD("rival %d stuck at path point %d (lap %d) for 900 frames at race frame %d",r,k->pi,k->lap,nrace); } }
                if(k->x<0 || k->y<0 || k->v>(s32)K_VBASE*8 || k->v<-(s32)K_VBASE*8){ KFZ_BAD("racer %d wild state x=%d y=%d v=%d",r,k->x,k->y,k->v); k->v=0; } }
            if(kloop_on){ if(++loopn>700 && !lpr){ lpr=1; KFZ_BAD("player stuck in a loop-de-loop for 700 frames (phase %d)",(int)kloop_ph); } } else loopn=0;
            if(kair){ if(++airn>900 && !apr){ apr=1; KFZ_BAD("player in the air for 900 frames (alt %d)",(int)kalt); } } else airn=0;
            if(ks_state==KS_DONE && done_f<0) done_f=nrace;
            (void)ncase; (void)quiet; (void)race_f; }
        alarm(0);
        int finished = kw_quit;
        if(!finished && nrc==1 && mode>=2){ finished=1; printf("  (a time trial nobody drives never ends: fine)\n"); }
        if(!finished){ tot_to++; KFZ_BAD("race never ended in %d frames (shape %d, %d path points, %d gates, laps %d): player lap %d/%d pi %d nextg %d state %d; clock %d finished %d",maxf,shape,np,gates,laps,rc[0].lap,laps,rc[0].pi,rc[0].nextg,ks_state,k_clock,k_finished);
            for(int r=0;r<nrc;r++) printf("    racer %d: pi %d lap %d fin %d out %d v %d la %d\n",r,rc[r].pi,rc[r].lap,rc[r].fin,rc[r].out,(int)rc[r].v,rc[r].la); }
        else tot_done++;
        if((mode==0) && kind!=6 && finished && !rc[0].fin && !(kind==7)) KFZ_BAD("the test driver never crossed the finish (lap %d/%d, nextg %d of %d gates)",rc[0].lap,laps,rc[0].nextg,gates);
        printf("race %d: %s shape %d knp %d gates %d laps %d, %s after %d frames (place %d, lap %d)\n",(int)n,kfz_what,shape,np,gates,laps,finished?"ended":"NOT ENDED",f,rc[0].place,rc[0].lap); fflush(stdout);
        if(!finished) { kw_quit=0; kart_stop(); }
        fade(1); oam_hide_all(); oam_flush(); }
    printf("KFUZZ SUMMARY: %d races ended, %d never ended, %d problems\n",tot_done,tot_to,kfz_bad); printf(kfz_bad?"KFUZZ FAILED\n":"KFUZZ OK\n"); fflush(stdout);
    exit(kfz_bad?1:0); }
#endif
