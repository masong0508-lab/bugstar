#!/bin/bash
# Kart race fuzz: race after race (many planets, tracks, classes, race kinds, tours) with a test driver, noisy driver, button mashing and hands off.
# Fails on a hang inside a frame, a race that never ends, a rival stuck, a wrong lap counter or path point, a stuck loop-de-loop or jump.
# usage: [ASAN=1] tests/kart.sh [races=24] [first=0]   (ASAN=1: built with the address sanitizer: finds every out-of-range write and read)   (builds its own test copy pc/bugstar_pc_kfz; the normal build is untouched)
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/kz.sav
export BUGSTAR_LS_TIPS=0     # (a life-sim tip card is a modal box that waits for a key: the fuzz driver never presses one)
if [ -n "$ASAN" ]; then PC_ARCH="-m64 -no-pie -fsanitize=address -g" PC_OBJ=_kfza PC_OUT=bugstar_pc_kfz PC_CFLAGS="-DTEST_KW -DTEST_KFUZZ=30000 -g" PC_SDL=0 ./build.sh >/dev/null 2>&1 || { echo "kart: build failed"; exit 1; }
else PC_SDL=0 PC_OUT=bugstar_pc_kfz PC_OBJ=_kfz PC_CFLAGS="-DTEST_KW -DTEST_KFUZZ=30000" ./build.sh >/dev/null 2>&1 || { echo "kart: build failed"; exit 1; }; fi
echo "" > out/empty.txt
KFUZZ_FIRST=${2:-0} KFUZZ_RACES=${1:-24} timeout 3000 ./bugstar_pc_kfz --script out/empty.txt --out out/kz --sav out/kz.sav --frames 100000000 > out/kz.log 2>&1
grep "KFUZZ" out/kz.log | head -20
grep -q "^KFUZZ OK" out/kz.log && { echo "kart: OK ($(grep -c '^race' out/kz.log) races)"; exit 0; }
echo "kart: FAILED (see pc/out/kz.log)"; exit 1
