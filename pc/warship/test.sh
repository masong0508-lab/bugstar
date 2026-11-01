#!/bin/bash
# Tests for COLOSSAL WARSHIPS (pc/warship, plain C, no game needed). Builds into pc/warship/out (not committed).
cd "$(dirname "$0")"; mkdir -p out
gcc -std=gnu11 -O2 -fwrapv ws_test.c ws.c ws_fight.c ws_deck.c ws_view.c ../giant/giant.c -lm -o out/ws_test || exit 1
./out/ws_test
