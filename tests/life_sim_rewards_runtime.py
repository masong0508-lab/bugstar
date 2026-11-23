#!/usr/bin/env python3
"""Guard real PC/Android life-sim gifts and mine pickups under ASan/UBSan.

The old miner reward called ore_add(0,n), an invisible trading-good slot.
A full bag ate collected mine gems, and sim_unlock_msgs only paid the first
new friend gift per update. This script executes the actual production C
reward/event-log/mine functions, not rewritten Python behavior.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
glue=(ROOT/"pc/sim/sim_glue.inc").read_text()
ui=(ROOT/"pc/sim/sim_ui.inc").read_text()
ore=(ROOT/"ore.inc").read_text()
mine=(ROOT/"pc/sim/mine.c").read_text()
core=(ROOT/"pc/sim/sim_core.c").read_text()
game=(ROOT/"game.inc").read_text()

def fun(src,marker):
    m=re.search(re.escape(marker)+r"[^;{}]*\{",src)
    assert m, f"missing definition: {marker}"
    i=src.index("{",m.start())+1
    depth=1
    while depth:
        depth+=(src[i]=="{")-(src[i]=="}")
        i+=1
    return src[m.start():i]

# These callers must use the actual credit flow, not just the test helpers.
assert "ore_add(0,a)" not in glue
assert "sim_grant_friend_gift(e,gift_text,sizeof gift_text)" in fun(glue,"static void sim_unlock_msgs(")
assert "u32 kept=sim_log_count(&sim_w); if(pending>kept) pending=kept;" in glue
assert "sim_credit_ore(g.ore_kind,g.ore_n,&credit)" in ui
assert "scat(menu_msg,mm)" not in ui
assert ui.count("txt_append(menu_msg,sizeof menu_msg,mm)") == 2
assert ui.count("sim_take_mine(mm,sizeof mm)") == 2
assert "char menu_msg[40]" in (ROOT/"main.c").read_text()

fns=[
    fun(game,"static void txt_copy("),
    fun(game,"static void txt_append("),
    fun(game,"static void txt_number("),
    fun(ore,"static int ore_add("),
    fun(glue,"static u64 sim_unit("),
    fun(glue,"static int sim_credit_ore("),
    fun(glue,"static void sim_grant_friend_gift("),
    fun(core,"uint32_t sim_log_count("),
    fun(core,"const SimEvent *sim_log_at("),
    fun(glue,"static void sim_unlock_msgs("),
    fun(mine,"int mn_game_ore("),
    fun(mine,"int mn_collect("),
    fun(ui,"static int sim_take_mine("),
]

stub=r"""
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;
#define NORE 12
#define SIM_LOGN 8
#define MN_TAKE_GEMS 6
enum { SIME_NONE, SIME_GIFT, SIME_LETTER, SIME_UNLOCK };
enum { SIMG_NONE, SIMG_FOOD, SIMG_GOLD, SIMG_HEAL, SIMG_ORE };
enum { UF_GODS, ULC_GIFT };
typedef struct { u32 tick; u16 kind,who,a,b; u32 seed32; } SimEvent;
typedef struct { u32 log_n,log_head; SimEvent log[SIM_LOGN]; } Sim;
typedef struct { int unused; } Lv;
typedef struct { int unused; } Unlock;
typedef struct { const char *name; } GodPower;
typedef struct { s64 gold; u32 bars,tools,cloth,gems; int ore_kind,ore_n; } MnGift;
typedef struct { u32 bars,tools,cloth,gems; int deepest; u32 mined; } Mine;
static Sim sim_w;
static int sim_seen_log, sim_said_letters, sim_gifts_got;
static int food, hearts, maxh=50,mp,maxmp=30;
static s64 level,gold;
static u16 ore_n[NORE];
static Mine mining;
static Unlock u;
static int said_n, noted_n, sfx_n;
static char last_msg[160];

static void sim_level_lv(Lv *l){ (void)l; }
static void sim_check_unlocks(Sim *s,Lv *l){ (void)s; (void)l; }
static int sim_ls_on(int n){(void)n;return 0;}
static Unlock *sim_ul(void){return &u;}
static void ul_note(Unlock *v,int n){(void)v;(void)n;noted_n++;}
static const GodPower *god_power(int i){static const GodPower g={"TEST"};(void)i;return &g;}
static void sim_event_text(const Sim *s,const SimEvent *e,char *t,size_t n){
    (void)s;snprintf(t,n,"GIFT KIND %d AMOUNT %d",e->a,e->b);
}
static void say(const char *t){snprintf(last_msg,sizeof last_msg,"%s",t);said_n++;}
static void sfx(int i){(void)i;sfx_n++;}
static void eat(int n){food+=n;}
static s64 sadd(s64 a,s64 b){return a+b;}
static int ore_px(int k){return 10+k;}
static Mine *sim_mine(void){return &mining;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"LIFE SIM REWARD FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""

cases=r"""
static void event_add(int kind,int a,int b){
    SimEvent *e=&sim_w.log[sim_w.log_head%SIM_LOGN];
    memset(e,0,sizeof *e); e->kind=(u16)kind;e->a=(u16)a;e->b=(u16)b;
    sim_w.log_head=(sim_w.log_head+1)%SIM_LOGN;
    sim_w.log_n++;
}
int main(void){
    int checks=0;
    char b[80];
    s64 extra;
    // Tier-one trading goods aren't the mining bag. A miner sends usable iron.
    SimEvent gift={0};gift.kind=SIME_GIFT;gift.a=SIMG_ORE;gift.b=8;
    sim_grant_friend_gift(&gift,b,sizeof b);
    CHECK(ore_n[0]==0 && ore_n[4]==8 && gold==0);
    CHECK(strstr(b,"8 IRON")!=NULL);checks++;
    // A partial/full bag must credit the unsaved part at the current ore price.
    ore_n[4]=9997;gift.b=9;
    sim_grant_friend_gift(&gift,b,sizeof b);
    CHECK(ore_n[4]==9999 && gold==7*ore_px(4));
    CHECK(strstr(b,"2 IRON") && strstr(b,"BAG FULL +98G"));checks++;
    extra=0;
    CHECK(sim_credit_ore(4,5,&extra)==0 && extra==5*ore_px(4));
    CHECK(ore_n[4]==9999 && gold==12*ore_px(4));checks++;
    // Defensive guards: bad kind is paid into iron, invalid amount gives nothing.
    ore_n[4]=0; extra=-1;
    CHECK(sim_credit_ore(-8,3,&extra)==3 && ore_n[4]==3 && extra==0);
    CHECK(sim_credit_ore(4,0,&extra)==0 && extra==0);checks++;
    // Text helpers protect short display buffers and their canaries.
    struct {u8 left;char out[4];u8 right;} tiny={0xa5,{0},0x5a};
    sim_grant_friend_gift(&gift,tiny.out,sizeof tiny.out);
    CHECK(tiny.left==0xa5 && tiny.right==0x5a && tiny.out[3]==0);checks++;

    // Several different gifts generated in one frame must ALL be applied,
    // not merely the first one before sim_seen_log jumps to the end.
    memset(&sim_w,0,sizeof sim_w);sim_seen_log=0;sim_gifts_got=0;
    food=0;gold=0;hearts=1;mp=1;level=20;ore_n[4]=0;said_n=0;noted_n=0;
    event_add(SIME_GIFT,SIMG_FOOD,5);
    event_add(SIME_GIFT,SIMG_GOLD,2);
    event_add(SIME_GIFT,SIMG_ORE,3);
    event_add(SIME_GIFT,SIMG_HEAL,1);
    sim_unlock_msgs();
    CHECK(food==5 && gold==80 && ore_n[4]==3);
    CHECK(hearts==maxh && mp==maxmp && sim_gifts_got==4 && sim_seen_log==4);
    CHECK(strstr(last_msg,"4 FRIEND GIFTS")!=NULL && noted_n==1);checks++;
    sim_unlock_msgs();
    CHECK(food==5 && gold==80 && ore_n[4]==3 && sim_gifts_got==4 && noted_n==1);checks++;
    // A level above 100 must use the same capped economic unit as sim_unit.
    level=5000;gift.a=SIMG_GOLD;gift.b=5;sim_grant_friend_gift(&gift,b,sizeof b);
    CHECK(gold==80+5*120);checks++;

    // The journal is an 8-event ring here. Never reread older overwritten
    // records or pay a reward twice after a long simulated time jump.
    memset(&sim_w,0,sizeof sim_w);sim_seen_log=0;sim_gifts_got=0;gold=0;
    for(int i=0;i<12;i++) event_add(SIME_GIFT,SIMG_GOLD,1);
    sim_unlock_msgs();
    CHECK(gold==8*120 && sim_gifts_got==8 && sim_seen_log==12);
    sim_unlock_msgs();CHECK(gold==8*120 && sim_gifts_got==8);checks++;

    // Take six mine gems as a real ore item, the remaining three next visit.
    memset(&mining,0,sizeof mining); memset(ore_n,0,sizeof ore_n);
    mining.gems=9;mining.bars=2;mining.tools=1;
    mining.deepest=15;mining.mined=3;
    CHECK(mn_game_ore(mining.deepest,mining.mined)==11);
    char msg[44];gold=0;
    CHECK(sim_take_mine(msg,sizeof msg)==1);
    CHECK(ore_n[11]==6 && mining.gems==3 && mining.bars==0 && mining.tools==0);
    CHECK(gold==2*9+30+6*60 && strstr(msg,"ORE+6")!=NULL);checks++;
    ore_n[11]=9998;
    struct{u8 left;char msg[44];u8 right;} out={0xa5,{0},0x5a};
    s64 g0=gold;
    CHECK(sim_take_mine(out.msg,sizeof out.msg)==1);
    CHECK(ore_n[11]==9999 && mining.gems==0);
    CHECK(gold==g0+3*60+2*ore_px(11));
    CHECK(strstr(out.msg,"ORE+1") && strstr(out.msg,"FULL+42G"));
    CHECK(out.left==0xa5 && out.right==0x5a);checks++;
    CHECK(sim_take_mine(msg,sizeof msg)==0 && mining.gems==0);checks++;
    // Even when completely full, the mine and friend gift have a fair payout.
    mining.gems=1;mining.deepest=4;mining.mined=0;ore_n[4]=9999;g0=gold;
    CHECK(sim_take_mine(msg,sizeof msg));
    CHECK(strstr(msg,"FULL+14G")!=NULL);
    CHECK(gold==g0+60+14 && ore_n[4]==9999);checks++;
    printf("Life-sim gifts, event backlog, mine ore and full-bag refunds: PASS (%d cases)\n",checks);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-life-sim-") as td:
    src=Path(td)/"life_rewards.c"
    exe=Path(td)/"life_rewards"
    src.write_text(stub+"\n\n"+"\n\n".join(fns)+"\n\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-omit-frame-pointer",
                    "-fno-sanitize-recover=all",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
