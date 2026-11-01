#!/bin/bash
# The PC SHIP EDITOR in the real game: NEW GAME -> bug -> the editor (it replaces the hangar). Every tab is shot and must look different, two runs match,
# leaving it writes the slot's design next to the save, and the design really changes the game's ship numbers (the game's own functions, via the hooks).
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/sy1* out/sy2* out/sy3*
B=./bugstar_pc; F=2350
PC_SY_PRINT=1 $B --script tests/shipyard.txt --out out/sy1 --sav out/sy1.sav --frames $F > out/sy1.log
PC_SY_PRINT=1 $B --script tests/shipyard.txt --out out/sy2 --sav out/sy2.sav --frames $F > out/sy2.log
cmp -s out/sy1.log out/sy2.log || { echo "shipyard: DIFFERENT between two runs"; diff out/sy1.log out/sy2.log | head; exit 1; }
n=$(grep "^shot" out/sy1.log | head -10 | sed 's/.*screen=//' | sort -u | wc -l); [ "$n" = 10 ] || { echo "shipyard: the 10 tabs do not all look different ($n)"; exit 1; }
[ -f out/sy1.slot0.ship ] && [ $(stat -c %s out/sy1.slot0.ship) -lt 120 ] || { echo "shipyard: no small design file next to the save"; exit 1; }
g(){ grep "^shipyard:" "$1" | sed 's/.*top speed //'; }
a=$(g out/sy1.log); [ -n "$a" ] || { echo "shipyard: no stats line"; exit 1; }
PC_SY_PRINT=1 PC_SHIP_PRESET=6 $B --script tests/shipyard.txt --out out/sy3 --sav out/sy3.sav --frames $F > out/sy3.log; t=$(g out/sy3.log)
PC_SY_PRINT=1 PC_SHIP_PRESET=4 $B --script tests/shipyard.txt --out out/sy3 --sav out/sy3b.sav --frames $F > out/sy3b.log; i=$(g out/sy3b.log)
set -- $t; ts=$1; tdmg=$6; set -- $i; is=$1; idmg=$6
[ "$is" -gt "$ts" ] || { echo "shipyard: the INTERCEPTOR is not faster than the TANK in the game ($is vs $ts)"; exit 1; }
[ "$tdmg" -lt "$idmg" ] || { echo "shipyard: the TANK does not take less damage in the game ($tdmg vs $idmg)"; exit 1; }
echo "shipyard: 10 tabs differ, SAME on two runs, design file $(stat -c %s out/sy1.slot0.ship) bytes; game: tank speed $ts dmg $tdmg%, interceptor speed $is dmg $idmg%"
