#!/bin/bash
# Tests and coverage for the MEANING LAYER (pure C, no game needed). Built with the signed-overflow trap, so any overflow stops the run.   ./test.sh [quick]
cd "$(dirname "$0")"; mkdir -p out
gcc -std=gnu11 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -fsanitize=signed-integer-overflow,bounds -fno-sanitize-recover=all -I. \
  meaning_test.c meaning.c ../content/pc_content.c ../content/flora.c ../laws/laws.c ../sim/god.c ../sim/lv.c -lm -o out/meaning_test || exit 1
./out/meaning_test "$@"
