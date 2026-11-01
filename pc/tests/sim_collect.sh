#!/bin/bash
# Colony output flows back into the action game (Phase 5): 1 bug freed (tent, 2 settlers), 2.5 game days in town, then the quick menu, COLONY, A takes
# what the colony made. Checks: one gift was taken, the colony's food went to the player's bar, two runs are the same.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/sc1.* out/sc2.*
for r in 1 2; do BUGSTAR_SET_FREED=1 BUGSTAR_SIM_DUMP=out/sc$r.dump ./bugstar_pc --script tests/sim_collect.txt --out out/sc$r --sav out/sc$r.sav --frames 96000 > out/sc$r.log; done
cmp -s out/sc1.log out/sc2.log && cmp -s out/sc1.dump out/sc2.dump || { echo "sim_collect: DIFFERENT between two runs"; exit 1; }
b=$(grep "^colcards" out/sc1.dump | tail -2 | head -1); a=$(grep "^colcards" out/sc1.dump | tail -1)
cb=$(grep "^colony" out/sc1.dump | tail -2 | head -1); ca=$(grep "^colony" out/sc1.dump | tail -1)
f(){ echo "$1" | awk -v k=$2 '{for(i=1;i<=NF;i++) if($i==k) print $(i+1)}'; }
[ "$(f "$b" gifts)" = 0 ] && [ "$(f "$a" gifts)" = 1 ] || { echo "sim_collect: no gift taken ($a)"; exit 1; }
[ "$(f "$cb" food)" -gt 0 ] && [ "$(f "$ca" food)" -lt "$(f "$cb" food)" ] || { echo "sim_collect: colony food not taken ($cb / $ca)"; exit 1; }
echo "sim_collect: OK (colony food $(f "$cb" food) -> $(f "$ca" food), taken once, two runs SAME)"
