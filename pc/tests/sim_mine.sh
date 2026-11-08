#!/bin/bash
# The colony's WORKSHOPS and MINE in the real game (Phase 6). Test hooks: BUGSTAR_SIM_MINE=1 gives the colony the mine rungs, a mine and miners;
# BUGSTAR_SIM_AGE=300 makes it catch up 300 days at once. The script opens PC LAB > SIDE GAME > COLONY, then the WORKSHOPS and MINE screens (upgrade a workshop, brace).
# Checks: two runs are the same; the mine is deep and the chains made bars/tools/cloth; the five screens (away card, colony, workshops x2, mine) are different pictures;
# the .sim file keeps the mine (chunk MINE) and a CONTINUE from it still has the mine and the workshop level 2; with the side game OFF there is no mine at all.
export BUGSTAR_LS_ALL=1     # patch 47: the old checks press keys on menus by position: SHOW ALL keeps every row, no tip cards
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/mn1.* out/mn2.* out/mn3.* out/mnc.* out/mno.*
run(){ BUGSTAR_SET_FAME=699 BUGSTAR_SET_LEVEL=2500 BUGSTAR_SET_FREED=1 "${@:5}" BUGSTAR_SIM_DUMP=out/$1.dump BUGSTAR_SIM_DUMP_EVERY=100 ./bugstar_pc --script $2 --out out/$1 --sav out/$1.sav --frames $3 > out/$1.log; }
run1(){ env BUGSTAR_SET_FAME=699 BUGSTAR_SET_LEVEL=2500 BUGSTAR_SET_FREED=1 BUGSTAR_SIM_MINE=1 BUGSTAR_SIM_AGE=300 BUGSTAR_SIM_DUMP=out/$1.dump BUGSTAR_SIM_DUMP_EVERY=100 ./bugstar_pc --script tests/sim_mine.txt --out out/$1 --sav out/$1.sav --frames 12000 > out/$1.log; }
run1 mn1; run1 mn2
cmp -s out/mn1.log out/mn2.log && cmp -s out/mn1.dump out/mn2.dump && cmp -s out/mn1.sav.sim out/mn2.sav.sim || { echo "sim_mine: DIFFERENT between two runs"; exit 1; }
f(){ echo "$1" | awk -v k=$2 '{for(i=1;i<=NF;i++) if($i==k) print $(i+1)}'; }
m=$(grep "^mine" out/mn1.dump | tail -1)
[ "$(f "$m" depth)" -ge 8 ] && [ "$(f "$m" bars)" -gt 0 ] && [ "$(f "$m" tools)" -gt 0 ] && [ "$(f "$m" cloth)" -gt 0 ] || { echo "sim_mine: the mine did not run: $m"; exit 1; }
[ "$(f "$m" scn)" -ge 1 ] && [ "$(f "$m" safe)" = 1 ] || { echo "sim_mine: the scenario and sandbox rows did nothing: $m"; exit 1; }
ws=$(f "$m" ws); [ "${ws%%/*}" = 2 ] || { echo "sim_mine: the smelter was not upgraded by hand (ws $ws)"; exit 1; }
n=$(grep "^shot" out/mn1.log | sed 's/.*screen=//' | sort -u | wc -l); [ "$n" -ge 5 ] || { echo "sim_mine: screens not different ($n)"; exit 1; }
grep -qa MINE out/mn1.sav.sim || { echo "sim_mine: no MINE chunk in the .sim file"; exit 1; }
# a CONTINUE from the save keeps the mine
cp out/mn1.sav out/mnc.sav; cp out/mn1.sav.sim out/mnc.sav.sim; [ -e out/mn1.sav.ws0 ] && cp out/mn1.sav.ws0 out/mnc.sav.ws0
BUGSTAR_SIM_DUMP_EVERY=100 BUGSTAR_SET_FAME=699 BUGSTAR_SET_LEVEL=2500 BUGSTAR_SIM_DUMP=out/mnc.dump ./bugstar_pc --script tests/sim_cont.txt --out out/mnc --sav out/mnc.sav --frames 6000 > out/mnc.log
c=$(grep "^mine" out/mnc.dump | tail -1); [ "$(f "$c" safe)" = 1 ] && [ "$(f "$c" depth)" = "$(f "$m" depth)" ] && [ "${ws%%/*}" = "$(f "$c" ws | cut -d/ -f1)" ] || { echo "sim_mine: the mine was lost on reload: $c"; exit 1; }
# side game OFF: no .sim file, no mine
BUGSTAR_SIDE=0 BUGSTAR_SIM_MINE=1 BUGSTAR_SIM_AGE=300 BUGSTAR_SET_FREED=1 ./bugstar_pc --script tests/sim_cont.txt --out out/mno --sav out/mno.sav --frames 3000 > out/mno.log
[ ! -e out/mno.sav.sim ] || { echo "sim_mine: a .sim file was written with the side game OFF"; exit 1; }
echo "sim_mine: OK (mine level $(f "$m" depth), bars $(f "$m" bars) tools $(f "$m" tools) cloth $(f "$m" cloth) gems $(f "$m" gems), smelter upgraded, $n different screens, kept after reload, nothing with the side game off, two runs SAME)"
