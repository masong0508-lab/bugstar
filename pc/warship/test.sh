#!/bin/bash
# Tests for COLOSSAL WARSHIPS (pc/warship, plain C, no game needed). Builds into pc/warship/out (not committed).
cd "$(dirname "$0")"; mkdir -p out
gcc -std=gnu11 -O2 -fwrapv ws_test.c ws.c ws_fight.c ws_deck.c ws_view.c ../giant/giant.c -lm -o out/ws_test || exit 1
./out/ws_test
gcc -std=gnu11 -O2 -fwrapv ms_test.c ms.c ws.c ../giant/giant.c -lm -o out/ms_test && ./out/ms_test || exit 1
bash ./bal.sh quick > out/bal_quick.txt || { tail -25 out/bal_quick.txt; exit 1; }; tail -1 out/bal_quick.txt
