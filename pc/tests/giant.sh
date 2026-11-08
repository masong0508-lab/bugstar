#!/bin/bash
# GIANT NUMBERS in the real game (PC LAB > GIANT NUMBERS): the 2^(2^32) viewer worked out LIVE (no digit file: start, a jump 811,693 digits in,
# the menu, the progress screen), the counting number, the BUGSTARILLION V3 name (needs its index: uses $BUGSTAR_GIANT_DIR when it is set),
# the namer and the ladder. Every shot must be a different picture, nothing may crash, and the contact sheets go to out/giant_*.png.
cd "$(dirname "$0")/.."; mkdir -p out; export BUGSTAR_SIDE=0
fail=0
run(){ # name script frames [env]
    rm -f out/gt_$1_* out/gt_$1.sav*
    env $4 ./bugstar_pc --script tests/$2 --out out/gt_$1 --sav out/gt_$1.sav --frames $3 > out/gt_$1.log 2>&1 || { echo "giant: $1 CRASHED"; fail=1; return; }
    n=$(grep -c "^shot" out/gt_$1.log); u=$(grep "^shot" out/gt_$1.log | sed 's/.*screen=//' | sort -u | wc -l)
    [ "$n" -ge 4 ] && [ "$u" -ge "${MINU:-$((n - 1))}" ] || { echo "giant: $1: $u different pictures of $n shots"; fail=1; }
    python3 tests/ppm2png.py --sheet out/giant_$1.png out/gt_$1_*.ppm 2>/dev/null
    echo "giant: $1: $n shots, $u different"
}
G="BUGSTAR_WS_AT=650 BUGSTAR_WS_TEST=giant,0,0"      # the hook opens GIANT NUMBERS (the PC LAB entry) once the game runs; the scripts press keys in the hub
run live giant_pow.txt 5400 "BUGSTAR_GIANT_DIR=$(pwd)/out/gt_nodir $G"
run count giant_count.txt 5000 "$G"
run misc giant_misc.txt 5200 "$G"
# the V3 name with NO digit folder: the row says INDEX NEEDED, opening it must not crash
MINU=3 run v3none giant_v3.txt 4500 "BUGSTAR_GIANT_DIR=$(pwd)/out/gt_nodir $G"
if [ -n "$BUGSTAR_GIANT_DIR" ]; then MINU=4 run v3 giant_v3.txt 6600 "BUGSTAR_GIANT_DIR=$BUGSTAR_GIANT_DIR $G"
    # the long walk (patch 45): scroll, hold-scroll, jump to the last letter (SEL = max), the menu, go to the start / end, a term jump, the about pages, WRITE 1 MB (1,048,576 bytes and its SHA-256 on screen)
    MINU=9 run v3deep giant_v3deep.txt 10400 "BUGSTAR_GIANT_DIR=$BUGSTAR_GIANT_DIR $G"; else echo "giant: v3 with the index skipped (set BUGSTAR_GIANT_DIR to a folder with the V3 index)"; fi
[ $fail = 0 ] && echo "giant: OK" || exit 1
