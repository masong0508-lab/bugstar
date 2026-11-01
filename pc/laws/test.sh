#!/bin/bash
# Tests for the PC LAWS module (pure C, no game needed): table, clamping, files, determinism, defaults change nothing, every law moves a number,
# 100,000 random law sets with extreme sizes. Builds into pc/laws/out (not committed).   ./test.sh
cd "$(dirname "$0")"; mkdir -p out
gcc -std=gnu11 -O2 -I. -Wall -Wextra -Wno-misleading-indentation -Wno-unused-parameter -ffp-contract=off -fwrapv laws_test.c laws.c lawmodel.c lawtext.c -lm -o out/laws_test || exit 1
time ./out/laws_test
