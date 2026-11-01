#!/bin/bash
# PC LAB > SIDE GAME in the real game (pause, WORLD page, B, up to SIDE GAME, A; then the list, PEOPLE). Level 2500 is set with BUGSTAR_SET_LEVEL.
# Two runs must match and the lab, the side-game menu, PEOPLE and the game afterwards must be four different pictures; the .sim file is written.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/sl1.* out/sl2.*
for r in 1 2; do BUGSTAR_SET_LEVEL=2500 ./bugstar_pc --script tests/sim_lab.txt --out out/sl$r --sav out/sl$r.sav --frames 6200 > out/sl$r.log; done
cmp -s out/sl1.log out/sl2.log || { echo "sim_lab: DIFFERENT between two runs"; diff out/sl1.log out/sl2.log | head; exit 1; }
h(){ grep "^shot $1 " out/sl1.log | sed 's/.*screen=//'; }
n=$(echo "$(h 5150) $(h 5300) $(h 5700) $(h 5900)" | tr ' ' '\n' | sort -u | wc -l)
[ "$n" = 4 ] || { echo "sim_lab: pictures not all different ($n of 4)"; exit 1; }
[ -e out/sl1.sav.sim ] || { echo "sim_lab: no .sim file"; exit 1; }
echo "sim_lab: OK (lab, side-game menu, people, game: 4 different pictures, same on two runs)"
