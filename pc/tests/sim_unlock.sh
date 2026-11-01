#!/bin/bash
# The colony's unlock ladder in the real game (Phase 4). BUGSTAR_SET_FREED sets how many caged bugs were freed (a test hook).
#  1. new game, 0 freed: the colony is locked (mask 0001, no settlers, no card)          2. new game, 1 freed: the TENT opens, 2 settlers, one card
#  3. continue save 1 with 0 freed: the tent stays (rungs never close) and the COLONY screen lists it; continue save 0: the screen says LOCKED
#  Two runs of 2 are the same.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/un*.*
run(){ BUGSTAR_SET_FREED=$2 BUGSTAR_SIM_DUMP=out/un$1.dump ./bugstar_pc --script $3 --out out/un$1 --sav out/un$1.sav --frames $4 > out/un$1.log; }
last(){ grep "^$2" out/un$1.dump | tail -1; }
field(){ echo "$1" | awk -v k=$2 '{for(i=1;i<=NF;i++) if($i==k) print $(i+1)}'; }
run 0 0 tests/sim_town.txt 12000; run 1 1 tests/sim_town.txt 12000; run 1b 1 tests/sim_town.txt 12000
cmp -s out/un1.dump out/un1b.dump || { echo "sim_unlock: DIFFERENT between two runs"; exit 1; }
c0=$(last 0 colony); c1=$(last 1 colony)
[ "$(field "$c0" mask)" = 0001 ] && [ "$(field "$c0" settlers)" = 0 ] && [ "$(field "$(last 0 colcards)" colcards)" = 0 ] || { echo "sim_unlock: locked colony wrong: $c0"; exit 1; }
[ "$(field "$c1" mask)" = 0003 ] && [ "$(field "$c1" settlers)" = 2 ] && [ "$(field "$(last 1 colcards)" colcards)" = 1 ] || { echo "sim_unlock: tent did not open: $c1"; exit 1; }
for s in 0 1; do cp out/un$s.sav out/unc$s.sav; cp out/un$s.sav.sim out/unc$s.sav.sim; run c$s 0 tests/sim_cont.txt 6000; done
c3=$(last c1 colony); [ "$(field "$c3" mask)" = 0003 ] && [ "$(field "$c3" settlers)" = 2 ] || { echo "sim_unlock: the tent was lost after reload: $c3"; exit 1; }
h(){ grep "^shot $2 " out/un$1.log | sed 's/.*screen=//'; }
[ "$(h c0 900)" != "$(h c1 900)" ] && [ "$(h c1 900)" != "$(h c1 1300)" ] || { echo "sim_unlock: colony screens not as expected"; exit 1; }
echo "sim_unlock: OK (locked 0001 -> tent 0003 with 2 settlers and 1 card; kept after reload; LOCKED and TENT screens differ; two runs SAME)"
