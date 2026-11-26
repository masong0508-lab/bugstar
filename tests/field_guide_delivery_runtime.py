#!/usr/bin/env python3
"""Exercise production quest exchange and bound all ship field-guide text."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/"ui.inc").read_text()
def func(name):
    start=s.index(name); a=s.index("{", start); i=a+1; depth=1
    while depth:
        depth+=(s[i]=="{")-(s[i]=="}"); i+=1
    return s[start:i]
title=re.search(r'field_title\[NFIELD\]=\{([^}]+)\};',s).group(1)
lines=re.search(r'field_lines\[NFIELD\]\[6\]=\{(.*?)\n\};',s,re.S).group(1)
assert len(re.findall(r'"[^"]+"',title))==8
rows=re.findall(r'\{([^}]+)\}',lines)
assert len(rows)==8
for row in rows:
    strings=re.findall(r'"([^"]+)"',row)
    assert len(strings)==6 and all(len(x)<=48 for x in strings)
assert '{9,12,10,11,17}' in s and 'if(sel==17){ field_guide(pg);' in s
prefix=r"""
#include <assert.h>
#include <stdio.h>
typedef unsigned char u8;
enum { Q_NONE,Q_HUNT,Q_CHAMP,Q_FETCH,Q_RESCUE,Q_BOSS,Q_SHARDS,Q_BRING,Q_GOODS };
enum { IT_FISH=1,IT_BERRY,IT_SEED,IT_POTION,IT_THORN };
typedef struct { int it; } Gear;
#define BAGMAX 8
static Gear bag[BAGMAX];
static int bag_n,q_type,q_floor,q_need,cargo[8];
static const u8 bring_it[4]={IT_FISH,IT_BERRY,IT_SEED,IT_POTION};
static void drop_at(int k){for(int i=k;i<bag_n-1;i++)bag[i]=bag[i+1];bag_n--;}
static int add_gear(Gear g){if(bag_n>=BAGMAX)return 0;bag[bag_n++]=g;return 1;}
"""
cases=r"""
int main(void){
  Gear prize={IT_THORN};
  for(int n=0;n<=BAGMAX;n++) for(int wanted=1;wanted<=BAGMAX;wanted++) for(int have=0;have<=n;have++){
    bag_n=n;q_type=Q_BRING;q_floor=0;q_need=wanted;
    for(int i=0;i<n;i++)bag[i].it=i<have?IT_FISH:IT_POTION;
    if(have<wanted){if(n==BAGMAX)assert(!quest_prize_room());continue;}
    assert(quest_claim_prize(prize));assert(bag_n==n-wanted+1);
    int fish=0,prizes=0;for(int i=0;i<bag_n;i++){fish+=bag[i].it==IT_FISH;prizes+=bag[i].it==IT_THORN;}
    assert(fish==have-wanted && prizes==1);
  }
  bag_n=BAGMAX;q_type=Q_HUNT;assert(!quest_claim_prize(prize)&&bag_n==BAGMAX);
  q_type=Q_GOODS;q_need=3;cargo[0]=7;assert(!quest_claim_prize(prize)&&cargo[0]==7);
  bag_n=BAGMAX-1;assert(quest_claim_prize(prize)&&cargo[0]==4&&bag_n==BAGMAX);
  q_type=Q_BRING;q_need=0;assert(!quest_claim_prize(prize));
  puts("Field guide bounds and production delivery exchange PASS");
}
"""
with tempfile.TemporaryDirectory() as td:
    c=Path(td)/"field.c"; exe=Path(td)/"field"
    c.write_text(prefix+"\n".join(func(x) for x in ("static int quest_held(", "static void quest_take(", "static int quest_prize_room(", "static int quest_claim_prize("))+cases)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-fsanitize=undefined","-fno-sanitize-recover=all",str(c),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
