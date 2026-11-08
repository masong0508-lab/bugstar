#!/bin/bash
# The procedural sprites (pc/pc_sprgen*.c): determinism (same id, same picture), difference (different ids, different pictures), not empty, any size, the cache, the blit, the planet discs.
# Built standalone (no game) twice: plain -O2 for the checks and with the address and undefined-behaviour sanitizers for memory errors. Also: the picture of a run must not depend on
# the thread timing, so two builds with different optimisation levels must give the same hashes (floating point contraction is off: -ffp-contract=off).
cd "$(dirname "$0")/.."; mkdir -p out/sprgen; P=$(pwd)
SRC="tests/sprgen_test.c pc_sprgen.c pc_sprgen_flora.c pc_sprgen_build.c pc_sprgen_people.c pc_sprgen_crit.c pc_sprgen_critx.c pc_sprgen_crit2.c pc_sprgen_craft.c pc_sprgen_space.c pc_sprgen_gal.c content/flora.c content/pc_content.c"
CF="-std=gnu11 -Wall -Wno-unused-function -Wno-misleading-indentation -fno-strict-aliasing -funsigned-char -fwrapv -ffp-contract=off -I$P -I$P/content"
gcc -O2 $CF -o out/sprgen/t_o2 $SRC -lm -lpthread 2>out/sprgen/build.log || { echo "sprgen: build failed"; head -20 out/sprgen/build.log; exit 1; }
out/sprgen/t_o2 > out/sprgen/o2.txt 2> out/sprgen/o2.err; rc=$?; tail -1 out/sprgen/o2.txt
[ $rc = 0 ] || { echo "sprgen: checks failed"; grep FAIL out/sprgen/o2.err | head -20; exit 1; }
gcc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined $CF -o out/sprgen/t_asan $SRC -lm -lpthread 2>out/sprgen/build2.log || { echo "sprgen: sanitizer build failed"; head -20 out/sprgen/build2.log; exit 1; }
out/sprgen/t_asan > out/sprgen/asan.txt 2> out/sprgen/asan.err || { echo "sprgen: SANITIZER found a problem"; head -30 out/sprgen/asan.err; exit 1; }
gcc -O0 $CF -o out/sprgen/t_o0 $SRC -lm -lpthread 2>/dev/null && out/sprgen/t_o0 > out/sprgen/o0.txt 2>/dev/null
cmp -s out/sprgen/o2.txt out/sprgen/o0.txt || { echo "sprgen: -O0 and -O2 differ (a picture depends on the build)"; diff out/sprgen/o2.txt out/sprgen/o0.txt | head; exit 1; }
echo "sprgen: ok"
