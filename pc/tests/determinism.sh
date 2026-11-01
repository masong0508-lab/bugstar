#!/bin/bash
# Two runs of the same key script from a blank save must give the same pictures and the same save file.
cd "$(dirname "$0")/.."
B=./bugstar_pc; S=tests/play.txt; mkdir -p out
rm -f out/d1.sav out/d2.sav
$B --script $S --out out/d1 --sav out/d1.sav --frames 4501 > out/d1.log
$B --script $S --out out/d2 --sav out/d2.sav --frames 4501 > out/d2.log
if cmp -s out/d1.log out/d2.log && cmp -s out/d1.sav out/d2.sav; then echo "determinism: SAME ($(grep -c shot out/d1.log) shots + save, $(tail -1 out/d1.log))"; else echo "determinism: DIFFERENT"; diff out/d1.log out/d2.log | head; exit 1; fi
