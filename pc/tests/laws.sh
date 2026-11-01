#!/bin/bash
# The PC LAB in the real game: pause, WORLD page, B opens the lab; laws editor (presets, a change, hold to repeat, menu, ADVANCED, search), HOW THIS WORKS (9 pages),
# CHEATS (off, confirm, on, size jump, speed). Two runs must match; the pictures must differ from each other where the screens differ; the laws file beside the save must hold
# what was set and the cheat mark.  out/lawsheet*.png (if PIL is there) are contact sheets to look at.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/l1.* out/l2.*; python3 tests/mklaws.py > /dev/null
B=./bugstar_pc; S=tests/laws.txt
$B --script $S --out out/l1 --sav out/l1.sav --frames 8000 > out/l1.log
$B --script $S --out out/l2 --sav out/l2.sav --frames 8000 > out/l2.log
cmp -s out/l1.log out/l2.log || { echo "laws: DIFFERENT between two runs"; diff out/l1.log out/l2.log | head; exit 1; }
cmp -s out/l1.laws0 out/l2.laws0 || { echo "laws: the laws files differ"; exit 1; }
h(){ grep "^shot $1 " out/l1.log | sed 's/.*screen=//'; }
names=""; hashes=""
while read f name; do hh=$(h $f); hashes="$hashes $hh"; done < tests/laws.shots
n=$(echo $hashes | tr ' ' '\n' | sort -u | wc -l); t=$(echo $hashes | wc -w)
[ "$n" -ge $((t-4)) ] || { echo "laws: too many screens look the same ($n different of $t)"; exit 1; }
grep -q "^U_GRAV=250" out/l1.laws0 || { echo "laws: HIGH GRAVITY did not reach the laws file"; cat out/l1.laws0; exit 1; }
grep -q "^cheated=1" out/l1.laws0 || { echo "laws: the cheat mark is not in the laws file"; cat out/l1.laws0; exit 1; }
grep -q "^P_SIZE=" out/l1.laws0 || { echo "laws: the cheat size jump is not in the laws file"; cat out/l1.laws0; exit 1; }
echo "laws: SAME on two runs, $n different screens of $t shots, laws file has the preset, the size and the cheat mark"
