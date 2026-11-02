#!/bin/bash
# Menus the random fuzz never reached, played by script in the real game (hook BUGSTAR_WS_TEST=<mode>, pc_warship.inc): the ship's SHOP menu, the SPELL SHOP, the WORKBENCH,
# the mission BOARD (accept) and a mission kill (complete), the COLONY screen (unlocked with BUGSTAR_SET_FREED=1), the GIANT NUMBERS viewer with no digit folder, the PC LAB.
# For each: three seeded button-mashing scripts (with bursts of B to leave), the screen must RETURN (no dead end), no crash, same on two runs.
# Then SAVE and CONTINUE with the saved game.
cd "$(dirname "$0")/.."; mkdir -p out/scr; B=./bugstar_pc; fail=0
mk(){ python3 - "$1" "$2" <<'P'
import random,sys
seed=int(sys.argv[1]); r=random.Random(seed); L=["300 304 A","600 604 A","900 904 A","1200 1204 S","1500 1504 S","2000 2004 S","2300 2304 A","2700 2704 A"]; f=3400
keys=['A','A','A','B','U','D','U','D','l','r','R','L','s']
while f<12000:
    if r.random()<0.04:
        for i in range(8): L.append(f"{f} {f+3} B"); f+=12
    else:
        L.append(f"{f} {f+r.choice([2,4,20])} {r.choice(keys)}"); f+=r.choice([6,10,30,90])
open(sys.argv[2],"w").write("\n".join(L)+"\n")
P
}
for mode in shop spshop bench missions colony giant pclab; do
  for seed in 1 2 3; do
    mk $seed out/scr/k$seed.txt; n=$mode$seed; rm -f out/scr/$n.sav*
    env BUGSTAR_SIDE=1 BUGSTAR_WS_AT=650 BUGSTAR_WS_TEST=$mode,0,0 BUGSTAR_AFT_GOLD=100000 BUGSTAR_SET_FREED=1 BUGSTAR_GIANT_DIR=$(pwd)/out/scr/nodir \
      timeout 300 $B --script out/scr/k$seed.txt --out out/scr/$n --sav out/scr/$n.sav --frames 14000 > out/scr/$n.log 2> out/scr/$n.err; rc=$?
    [ $rc = 0 ] || { echo "screens: $mode seed $seed exit $rc"; fail=1; continue; }
    grep -q "^screen $mode returned" out/scr/$n.err || { echo "screens: $mode seed $seed never returned (dead end?)"; fail=1; }
    grep -q "^end frame=14000" out/scr/$n.log || { echo "screens: $mode seed $seed did not finish"; fail=1; }
  done
  mk 1 out/scr/k1.txt; rm -f out/scr/x.sav*
  env BUGSTAR_SIDE=1 BUGSTAR_WS_AT=650 BUGSTAR_WS_TEST=$mode,0,0 BUGSTAR_AFT_GOLD=100000 BUGSTAR_SET_FREED=1 BUGSTAR_GIANT_DIR=$(pwd)/out/scr/nodir timeout 300 $B --script out/scr/k1.txt --out out/scr/x --sav out/scr/x.sav --frames 14000 > out/scr/x.log 2>/dev/null
  cmp -s <(grep "^end" out/scr/x.log) <(grep "^end" out/scr/${mode}1.log) || { echo "screens: $mode differs between two runs"; fail=1; }
done
# mission complete: an accepted DESTROY mission, a kill, the pay; and CONTINUE with the saved game
rm -f out/scr/mk.sav*; BUGSTAR_SIDE=0 BUGSTAR_WS_AT=650 BUGSTAR_WS_LEVEL=100000 BUGSTAR_WS_TEST=mkill,520,0 $B --script tests/ws_kill.txt --out out/scr/mk --sav out/scr/mk.sav --frames 3200 > out/scr/mk.log 2> out/scr/mk.err || { echo "screens: mission kill crashed"; fail=1; }
grep -q "^kills=1" out/scr/mk.sav.ws0 || { echo "screens: the mission kill was not saved"; fail=1; }
cp out/scr/mk.sav out/scr/mc.sav; cp out/scr/mk.sav.ws0 out/scr/mc.sav.ws0
BUGSTAR_SIDE=0 $B --script tests/cont.txt --out out/scr/mc --sav out/scr/mc.sav --frames 1500 > out/scr/mc.log 2>&1 || { echo "screens: CONTINUE crashed"; fail=1; }
[ $fail = 0 ] && echo "screens: OK (shop, spell shop, workbench, mission board, colony, giant numbers without V3, PC LAB: 3 mashes each, all return, same on two runs; mission kill saved; continue loads)" || exit 1
