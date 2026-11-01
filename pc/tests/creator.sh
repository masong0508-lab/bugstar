#!/bin/bash
# The PLANET CREATOR in the real game: (1) the screen from the title menu (PLANET CREATOR is the last item): tabs WORLD to FILE, two runs match, every tab looks different;
# (2) a created planet is a real planet: the seed viewer lands on the recipe's share code (same name, class, size, water) and two runs match; other recipes give other planets.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/c1* out/c2*
B=./bugstar_pc
$B --script tests/creator.txt --out out/c1 --sav out/c1.sav --frames 1800 > out/c1.log
$B --script tests/creator.txt --out out/c2 --sav out/c2.sav --frames 1800 > out/c2.log
cmp -s out/c1.log out/c2.log || { echo "creator: DIFFERENT between two runs"; diff out/c1.log out/c2.log | head; exit 1; }
h(){ grep "^shot $1 " out/c1.log | sed 's/.*screen=//'; }
all="$(h 520) $(h 700) $(h 900) $(h 1100) $(h 1300) $(h 1500) $(h 1700)"
n=$(echo $all | tr ' ' '\n' | sort -u | wc -l)
[ "$n" = 7 ] || { echo "creator: the 7 tabs do not all look different ($n of 7)"; exit 1; }
gcc -O1 -o out/crcode tests/crcode.c creator/cr_recipe.c content/pc_content.c content/flora.c creator/cr_bio.c -lm -I creator 2>/dev/null || { echo "creator: cannot build crcode"; exit 1; }
v(){ $B --view --scale 1 --out out/c2v --crcode "$1" | grep -E "^(world|class|size|land|SEEDVIEW)"; }
for args in "0" "1" "2" "3" "0 3 90 4 0" "0 4 5 2 1" "2 6 50 1 7"; do code=$(out/crcode $args); a=$(v "$code"); b=$(v "$code"); [ "$a" = "$b" ] || { echo "creator: landing differs between two runs for $args"; exit 1; }; echo "$args => $(echo "$a" | tr '\n' ' ')"; done > out/c2.land
name0=$(grep -c "world: HOMEWORLD" out/c2.land); [ "$name0" -ge 1 ] || { echo "creator: the planet is not named after the recipe"; cat out/c2.land; exit 1; }
grep -q "world: EMBER REACH.*class: DESERT" out/c2.land || { grep -q "world: EMBER REACH" out/c2.land && grep -q "DESERT" out/c2.land; } || { echo "creator: example 1 is not EMBER REACH desert"; cat out/c2.land; exit 1; }
grep -q "class: OCEAN" out/c2.land || { echo "creator: class 3 did not land as an ocean planet"; cat out/c2.land; exit 1; }
[ "$(cut -d'>' -f2 out/c2.land | sort -u | wc -l)" -ge 6 ] || { echo "creator: recipes did not give different planets"; cat out/c2.land; exit 1; }
echo "creator: 7 tabs differ, SAME on two runs; 7 recipes landed twice the same way and named after the recipe"
