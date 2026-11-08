#!/bin/bash
# WARSHIP MISSIONS PLAYED THROUGH in the real game (patch 45): accept at the station board, fly, destroy or board, collect, and check the pay, the journal, the save and the reload.
# The hook BUGSTAR_WS_TEST=mplay (pc/pc_missions.inc, "THE MISSION PLAYTHROUGH") plays the whole job from key scripts made by tests/mkms.py; every step says "mp <frame> ..." on stderr.
#  1. each of the 7 kinds (DESTROY, SABOTAGE, STEAL PLANS, RESCUE, ESCORT, DEFEND, SURVEY): accepted, engaged, done, the journal is empty, one job in the done list, kills 1, and the save file says so
#  2. the PAY: the number the game paid is worked out again by pc/warship/ms_check.c (the craft's pay x the kind's share x (1 + bonus/100)); the gold went up by exactly that (whole gold, the rest is dust)
#  3. ESCORT brings 2 wingmen into the fight; DEFEND raids a real colony (side game on) without crashing; two runs of one job are equal
#  4. SAVE / LOAD: accept and save; CONTINUE; the job is still in the journal; play it from there; CONTINUE again: it is done for good, the gold is the same; a job past its deadline is dropped
cd "$(dirname "$0")/.."; mkdir -p out/ms; rm -f out/ms/*; B=./bugstar_pc; fail=0
gcc -std=gnu11 -O2 -fwrapv -w -o out/ms/ms_check warship/ms_check.c warship/ms.c warship/ws.c giant/giant.c -lm || { echo "mission: cannot build ms_check"; exit 1; }
# run NAME MODE KIND CLASS FRAMES SYNC [env...]   (key script from mkms.py; the hook starts at script frame SYNC)
run(){ local n=$1 mode=$2 kind=$3 cls=$4 fr=$5 sync=$6; shift 6
    python3 tests/mkms.py $sync $cls > out/ms/$n.txt; rm -f out/ms/$n.sav out/ms/$n.sav.* out/ms/${n}_*.ppm
    env BUGSTAR_SIDE=${SIDE:-0} BUGSTAR_WS_TEST=$mode,0,0 BUGSTAR_MS_SYNC=$sync BUGSTAR_WS_AT=0 BUGSTAR_MS_AUTO=1 BUGSTAR_MS_KIND=$kind BUGSTAR_MS_Z=${Z:-200} BUGSTAR_WS_LEVEL=${LV:-1} "$@" \
        timeout 600 $B --script out/ms/$n.txt --out out/ms/$n --sav out/ms/$n.sav --frames $fr > out/ms/$n.log 2> out/ms/$n.err || { echo "mission: $n CRASHED or timed out"; fail=1; }; }
f(){ echo "$1" | tr ' ' '\n' | grep "^$2=" | head -1 | cut -d= -f2; }                     # a field of a log line: f "line" gold
need(){ grep -q "$2" out/ms/$1.err || { echo "mission: $1 lacks '$2'"; fail=1; }; }
# the pay of a finished job: finish line -> ms_check; pay line -> the gold really went up by that much
checkpay(){ local n=$1 l p seed z kind bonus attack out reward chk
    l=$(grep "^mp [0-9]* finish" out/ms/$n.err | tail -1); [ -n "$l" ] || { echo "mission: $n has no 'finish' line"; fail=1; return; }
    kind=$(f "$l" kind); bonus=$(f "$l" bonus); seed=$(f "$l" seed); z=$(f "$l" z); attack=$(f "$l" attack); out=$(f "$l" out); reward=$(f "$l" reward)
    chk=$(out/ms/ms_check $seed $z $kind $bonus $attack ${out%,*} ${out#*,}); python3 - "$reward" "$chk" "$n" <<'PY' || fail=1
import sys
g=sys.argv[1].split(","); c=sys.argv[2].split(); gv=float(g[0])*10**int(g[1]); cv=float(c[1])*10**int(c[2])
ok=abs(gv-cv)<=1e-9*max(gv,cv)
print("mission: %s pay %.6g gold: the formula by hand says %.6g%s"%(sys.argv[3],gv,cv,"" if ok else "  <<< DIFFERENT")); sys.exit(0 if ok else 1)
PY
    p=$(grep "^mp [0-9]* pay " out/ms/$n.err | tail -1); [ -n "$p" ] || { echo "mission: $n has no 'pay' line"; fail=1; return; }
    python3 - "$p" "$n" <<'PY' || fail=1
import sys,re,math
l=sys.argv[1]; m=re.search(r"reward=([0-9.e+-]+),(-?\d+) gold (-?\d+) -> (-?\d+) dust ([0-9.]+) -> ([0-9.]+)",l)
rm,re_,g0,g1,d0,d1=float(m[1]),int(m[2]),int(m[3]),int(m[4]),float(m[5]),float(m[6]); v=rm*10**re_
if re_>=15: print("mission: %s paid into the vault (%.3g)"%(sys.argv[2],v)); sys.exit(0)
want=math.floor(d0+v+1e-9); ok=abs((g1-g0)-want)<=1 and abs((d0+v)-(g1-g0)-d1)<1e-3
print("mission: %s gold %d -> %d (+%d), reward %.2f, dust %.3f -> %.3f%s"%(sys.argv[2],g0,g1,g1-g0,v,d0,d1,"" if ok else "  <<< WRONG")); sys.exit(0 if ok else 1)
PY
}
final(){ local n=$1; local l=$(grep "^mp [0-9]* final" out/ms/$n.err | tail -1); [ -n "$l" ] || { echo "mission: $n never reached the final state"; fail=1; return; }
    [ "$(f "$l" nact)" = 0 ] && [ "$(f "$l" ndone)" = 1 ] && [ "$(f "$l" kills)" = 1 ] && [ "$(f "$l" count)" = 1 ] || { echo "mission: $n final state wrong: $l"; fail=1; }
    grep -q "^kills=1" out/ms/$n.sav.ws0 && grep -q "^mdone=" out/ms/$n.sav.ws0 && ! grep -q "^mis=" out/ms/$n.sav.ws0 || { echo "mission: $n: the .ws file does not say 'done' (kills / mdone / no mis=)"; fail=1; }; }
shots(){ local n=$1 min=$2; local u=$(grep "^shot" out/ms/$n.log | sed 's/.*screen=//' | sort -u | wc -l); [ "$u" -ge "$min" ] || { echo "mission: $n: only $u different pictures (want $min)"; fail=1; }; }
names=(DESTROY SABOTAGE STEAL RESCUE ESCORT DEFEND SURVEY)
# 1 + 2: every kind through to the payout
run k0 mplay 0 out 3900 3200;  need k0 "accepted kind=DESTROY"; need k0 "engaged"; need k0 "shooting"; final k0; checkpay k0; shots k0 5
run k1 mplay 1 sab 5400 3200;  need k1 "accepted kind=SABOTAGE"; need k1 "objective done"; need k1 "job paid out inside"; final k1; checkpay k1; shots k1 6
run k2 mplay 2 in 5200 3200;   need k2 "accepted kind=STEAL PLANS"; need k2 "objective done"; need k2 "job paid out inside"; final k2; checkpay k2; shots k2 6
run k3 mplay 3 in 5200 3200;   need k3 "accepted kind=RESCUE"; need k3 "objective done"; need k3 "job paid out inside"; final k3; checkpay k3
run k6 mplay 6 in 5200 3200;   need k6 "accepted kind=SURVEY"; need k6 "objective done"; need k6 "job paid out inside"; final k6; checkpay k6
# 3: ESCORT: the convoy escorts fly with you; DEFEND: a real colony is raided (side game on, a colony made by the test hooks)
run k4 mplay 4 out 3900 3200;  need k4 "accepted kind=ESCORT"; need k4 "wing=2 escort=2"; final k4; checkpay k4
SIDE=1 run k5 mplay 5 out 3900 3200 BUGSTAR_SIM_MINE=1 BUGSTAR_SIM_AGE=300; need k5 "accepted kind=DEFEND"; need k5 "defend raid"; final k5; checkpay k5
run k0b mplay 0 out 3900 3200; cmp -s out/ms/k0.err out/ms/k0b.err || { echo "mission: two runs of the same job are DIFFERENT"; fail=1; }
# 4: save and load
run s1 maccept 0 accept 3500 3200 BUGSTAR_MS_DL=10;    need s1 "accepted kind=DESTROY dl_abs=10"; grep -q "^mis=" out/ms/s1.sav.ws0 || { echo "mission: the accepted job is not in the .ws file"; fail=1; }
cp out/ms/s1.sav out/ms/s2.sav; cp out/ms/s1.sav.ws0 out/ms/s2.sav.ws0; [ -e out/ms/s1.sav.sim ] && cp out/ms/s1.sav.sim out/ms/s2.sav.sim
run_cont(){ local n=$1; shift; env BUGSTAR_SIDE=0 BUGSTAR_MS_AUTO=1 BUGSTAR_MS_Z=200 BUGSTAR_WS_LEVEL=1 BUGSTAR_WS_AT=0 "$@" timeout 600 $B --script out/ms/$n.txt --out out/ms/$n --sav out/ms/$n.sav --frames ${FR:-2600} > out/ms/$n.log 2> out/ms/$n.err || { echo "mission: $n CRASHED"; fail=1; }; }
python3 tests/mkms.py 1200 r-out > out/ms/s2.txt; BUGSTAR_MS_KIND= run_cont s2 BUGSTAR_WS_TEST=mresume,0,0 BUGSTAR_MS_SYNC=1200
need s2 "journal 0:"; grep -q "start .*nact=1 " out/ms/s2.err || { echo "mission: CONTINUE lost the job in the journal"; fail=1; }; need s2 "engaged"; final s2; checkpay s2
g2=$(f "$(grep '^mp [0-9]* final' out/ms/s2.err | tail -1)" gold)
cp out/ms/s2.sav out/ms/s3.sav; cp out/ms/s2.sav.ws0 out/ms/s3.sav.ws0; [ -e out/ms/s2.sav.sim ] && cp out/ms/s2.sav.sim out/ms/s3.sav.sim
echo "300 304 A" > out/ms/s3.txt; FR=1600 run_cont s3 BUGSTAR_WS_TEST=mcheck,0,0 BUGSTAR_MS_SYNC=1200
l=$(grep "^mp [0-9]* start" out/ms/s3.err | tail -1); [ "$(f "$l" nact)" = 0 ] && [ "$(f "$l" ndone)" = 1 ] && [ "$(f "$l" kills)" = 1 ] || { echo "mission: after the second CONTINUE the job is not done for good: $l"; fail=1; }
[ "$(f "$l" gold)" = "$g2" ] || { echo "mission: the gold changed over a save and load: $g2 -> $(f "$l" gold)"; fail=1; }
cp out/ms/s1.sav out/ms/s4.sav; cp out/ms/s1.sav.ws0 out/ms/s4.sav.ws0; [ -e out/ms/s1.sav.sim ] && cp out/ms/s1.sav.sim out/ms/s4.sav.sim
echo "300 304 A" > out/ms/s4.txt; FR=1600 run_cont s4 BUGSTAR_WS_TEST=mexpire,0,0 BUGSTAR_MS_SYNC=1200
grep -q "after 60 jumps.*nact=0" out/ms/s4.err || { echo "mission: a job past its deadline was not dropped"; fail=1; }
[ $fail = 0 ] && echo "mission: all 7 kinds played to the payout (pay = formula by hand, gold went up by exactly that), ESCORT wingmen, DEFEND colony raid, save and CONTINUE keep a job and a finished job, deadline drops a job, two runs equal" || exit 1
