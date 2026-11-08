#!/bin/bash
# The WARSHIP BALANCE REPORT (plain C, no game needed).  bash pc/warship/bal.sh [quick|old]
#   (no argument)  full run, about 3 seconds, prints the tables and checks the targets
#   quick          a shorter run (what warship/test.sh uses)
#   old            the numbers BEFORE patch 17 (build with -DBAL_OLD): shows what was wrong; it is expected to FAIL its targets
cd "$(dirname "$0")"; mkdir -p out
SRC="bal.c ws.c ws_fight.c ../sim/colony.c ../sim/mine.c ../sim/sim_core.c ../sim/god.c ../sim/lv.c ../giant/giant.c"
if [ "$1" = old ]; then gcc -std=gnu11 -O2 -fwrapv -w -DBAL_OLD $SRC -lm -o out/bal_old && ./out/bal_old; else gcc -std=gnu11 -O2 -fwrapv -w $SRC -lm -o out/bal && ./out/bal "$1"; fi
