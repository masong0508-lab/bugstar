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
grep -q "^P_DENS=190" out/l1.laws0 || { echo "laws: HIGH GRAVITY did not reach the laws file"; cat out/l1.laws0; exit 1; }
grep -q "^cheated=1" out/l1.laws0 || { echo "laws: the cheat mark is not in the laws file"; cat out/l1.laws0; exit 1; }
grep -q "^P_SIZE=" out/l1.laws0 || { echo "laws: the cheat size jump is not in the laws file"; cat out/l1.laws0; exit 1; }
# a saved world keeps its laws: continue the save of run 1 with and without its laws file; the lab and the editor must show it (CHEATED, the changed laws)
for v in with without; do cp out/l1.sav out/l3$v.sav; [ $v = with ] && cp out/l1.laws0 out/l3with.laws0 || rm -f out/l3without.laws0; done
$B --script tests/laws_load.txt --out out/l3w --sav out/l3with.sav --frames 2400 > out/l3w.log
$B --script tests/laws_load.txt --out out/l3n --sav out/l3without.sav --frames 2400 > out/l3n.log
g(){ grep "^shot $2 " $1 | sed 's/.*screen=//'; }
[ "$(g out/l3w.log 2100)" != "$(g out/l3n.log 2100)" ] && [ "$(g out/l3w.log 2300)" != "$(g out/l3n.log 2300)" ] || { echo "laws: a continued save did not load its laws file"; exit 1; }
# the seed viewer with laws: the same laws and address make the same picture twice, and a size changes it
v(){ ./bugstar_pc --view --scale 1 --out out/lawview --universe 1 --cx 5 --cy 7 --star 10 "$@" | grep -E "^(size|class|world|view0)"; }
{ for a in "" "--law P_SIZE=-112" "--law P_SIZE=-60" "--law P_SIZE=40" "--law P_SIZE=100" "--preset 4" "--preset 9" "--law U_LIGHT=10 --law P_PERSTAR=300"; do echo "== $a"; v $a; done; } > out/lawview.a
{ for a in "" "--law P_SIZE=-112" "--law P_SIZE=-60" "--law P_SIZE=40" "--law P_SIZE=100" "--preset 4" "--preset 9" "--law U_LIGHT=10 --law P_PERSTAR=300"; do echo "== $a"; v $a; done; } > out/lawview.b
cmp -s out/lawview.a out/lawview.b || { echo "laws: the seed viewer with laws differs between two runs"; exit 1; }
if [ "$1" = "--make-golden" ]; then cp out/lawview.a tests/laws.golden; echo "laws golden written"; fi
cmp -s out/lawview.a tests/laws.golden || { echo "laws: planets under laws differ from tests/laws.golden"; diff out/lawview.a tests/laws.golden | head; exit 1; }
echo "laws: SAME on two runs, $n different screens of $t shots, laws file has the preset, the size and the cheat mark; a continued save loads its laws; 8 planets under laws match laws.golden"
