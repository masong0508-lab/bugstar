#!/usr/bin/env python3
"""Test production dungeon spawn macros and elite goal accounting on a host."""
from pathlib import Path
import re
import subprocess
import tempfile
R=Path(__file__).resolve().parents[1]
s=(R/"dungeon.inc").read_text()
main=(R/"main.c").read_text()
def line(start):
    return next(x.strip() for x in s.splitlines() if x.strip().startswith(start))
def function(sig):
    a=s.index(sig+"{")
    b=s.index("{",a)
    count=1
    i=b+1
    while count:
        count+=(s[i]=="{")-(s[i]=="}")
        i+=1
    return s[a:i]
spawn=line("#define SPAWN(")
gear=line("#define SPAWN_GEAR(")
# Actual event spawn block, not a Python recreation.
a=s.index("case FE_ELITE:",s.index("    // ---- a floor event"))
b=s.index("case FE_DARK:",a)
elite=s[a:b]
# Keep phase transitions and the post-event mission reserve ordered.
markers=["int spawn_cap=NDO-24;","spawn_cap=NDO-12;","spawn_cap=NDO-6;","spawn_cap=NDO-4;","spawn_cap=NDO;"]
offsets=[s.index(m) for m in markers]
assert offsets==sorted(offsets)
assert "if(ndo<spawn_cap)" in spawn and "if(ndo<spawn_cap)" in gear
assert "dob[i].champ==FLOOR_ELITE_CHAMP && !dob[i].alive" in s
assert 'scpy(b,"FLOOR ELITE ")' in main
assert "rooms[nrooms>1?1+rndi(nrooms-1):0]" in elite
prefix=r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8;
typedef struct { int32_t x,y; int hp,maxhp,kind,alive; u8 sub,it,rar,pre,suf,lvl,champ,cname; } DObj;
typedef struct { u8 it,sub,rar,pre,suf,lvl; } Gear;
typedef struct { int cell; } Room;
#define MW 32
#define NDO 160
#define FLOOR_ELITE_CHAMP 4
enum { K_ITEM=1,K_SPIDER,K_BEETLE,K_BOSS,K_CORE,K_CAGE };
enum { FE_NONE,FE_ELITE };
static DObj dob[NDO];
static int ndo,spawn_cap,fl_ev,fl_need,fl_have,fl_done,depth_no=5,msg_timer,paid,xp,base_hp=10,nrooms=2;
static Room rooms[2]={{33},{66}};
static int xo_hp(int hp){ return hp; }
static int rndi(int n){ return n>1?1:0; }
static int room_floor_cell(Room *r){return r->cell;}
static int dlv(void){return 2;}
static void earn(int v){paid+=v;}
static void gain_xp(int v){xp+=v;}
static void scpy(char *a,const char *b){strcpy(a,b);}
static void scat(char *a,const char *b){strcat(a,b);}
static void ncat(char *a,int n){char b[20];snprintf(b,sizeof b,"%d",n);strcat(a,b);}
static void say(const char *s){(void)s;}
static void sfx(int n){(void)n;}
#define CHECK(x) do{ if(!(x)){fprintf(stderr,"Dungeon spawn regression line %d: %s\n",__LINE__,#x);return 1;} }while(0)
"""
driver=r"""
static void event(void){switch(fl_ev){
"""
tests=r"""
default:break;}}
int main(void){
    Gear item={9,1,2,3,4,5};
    for(int init=0;init<=NDO-24;init++){
        memset(dob,0,sizeof dob);ndo=0;spawn_cap=NDO-24;
        for(int i=0;i<init;i++) SPAWN(K_SPIDER,33,10,0);
        for(int i=0;i<NDO*2;i++) SPAWN_GEAR(35,item);
        CHECK(ndo==NDO-24);
        DObj old=dob[ndo-1];
        SPAWN_GEAR(65,((Gear){8,7,6,5,4,3}));
        CHECK(ndo==NDO-24 && !memcmp(&old,&dob[ndo-1],sizeof old));
        spawn_cap=NDO-12;
        SPAWN(K_CAGE,66,1,0);SPAWN(K_CAGE,67,1,0);
        for(int i=0;i<NDO;i++) SPAWN(K_SPIDER,68,10,0);
        CHECK(ndo==NDO-12);
        spawn_cap=NDO-6;
        SPAWN(K_BOSS,69,100,0);SPAWN_GEAR(70,item);
        for(int i=0;i<NDO;i++) SPAWN(K_SPIDER,71,10,0);
        CHECK(ndo==NDO-6);
        spawn_cap=NDO-4; fl_ev=FE_ELITE;fl_need=fl_have=fl_done=paid=xp=0;
        nrooms=1+(init&1);
        event();
        CHECK(ndo==NDO-4 && fl_need==2 && dob[ndo-1].champ==FLOOR_ELITE_CHAMP && dob[ndo-2].champ==FLOOR_ELITE_CHAMP);
        DObj random={0}; random.champ=1;
        fl_kill(&random); CHECK(!fl_have && !paid);
        fl_kill(&dob[ndo-2]); CHECK(fl_have==1 && !fl_done && !paid);
        fl_kill(&dob[ndo-1]); CHECK(fl_have==2 && fl_done && paid>0 && xp>0);
        int once=paid;fl_kill(&dob[ndo-1]);CHECK(paid==once);
        spawn_cap=NDO;for(int i=0;i<4;i++) SPAWN(K_CAGE,72,1,0);
        CHECK(ndo==NDO);
        old=dob[NDO-1];SPAWN_GEAR(73,item);
        CHECK(!memcmp(&old,&dob[NDO-1],sizeof old));
    }
    // Defensive full-pool fallback: don't mutate the previous object,
    // read outside room 0, or advertise two nonexistent targets.
    ndo=spawn_cap=NDO;fl_ev=FE_ELITE;fl_need=0;event();
    CHECK(!fl_need && fl_ev==FE_NONE);
    puts("Dungeon budget: 137 crowded-floor cases, exact elite goals, full-pool writes PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/"budget.c";exe=Path(tmp)/"budget"
    src.write_text(prefix+"\n"+spawn+"\n"+gear+"\n"+function("static void fl_kill(DObj *o)")+"\n"+driver+elite+tests)
    subprocess.run(["cc","-std=c11","-O1","-fsanitize=address,undefined","-fno-sanitize-recover=all",str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
