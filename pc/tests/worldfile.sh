#!/bin/bash
# The PC WORLD FILE screen, in the real game: pause (START), WORLD page (R x4), B opens the PC LAB, A the file, R turns its pages, B closes it. Two runs must match,
# the pictures of the six file pages must all differ from each other and from the WORLD page, and the game must still be running after it closed.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/w1.sav out/w2.sav
export BUGSTAR_SIDE=0   # the key script presses A on bare ground, which would open the side-game menu
B=./bugstar_pc; S=tests/worldfile.txt
$B --script $S --out out/w1 --sav out/w1.sav --frames 6000 > out/w1.log
$B --script $S --out out/w2 --sav out/w2.sav --frames 6000 > out/w2.log
cmp -s out/w1.log out/w2.log || { echo "worldfile: DIFFERENT between two runs"; diff out/w1.log out/w2.log | head; exit 1; }
# shots (4560 is the game just before the pause): 4950 (file page 1), 5090 (2), 5190 (3), 5290 (4), 5390 (5), 5490 (6); 4560 the pause screen; 5900 back in the game
h(){ grep "^shot $1 " out/w1.log | sed 's/.*screen=//'; }
all="$(h 4560) $(h 4950) $(h 5090) $(h 5190) $(h 5290) $(h 5390) $(h 5490) $(h 5900)"
n=$(echo $all | tr ' ' '\n' | sort -u | wc -l)
[ "$n" = 8 ] || { echo "worldfile: pictures are not all different ($n of 8)"; echo $all; exit 1; }
echo "worldfile: SAME on two runs, 6 file pages + pause + game all different ($(grep -c '^shot' out/w1.log) shots)"
