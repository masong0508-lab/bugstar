#!/bin/bash
# Tests for COLOSSAL WARSHIPS (pc/warship, plain C, no game needed). Builds into pc/warship/out (not committed).
cd "$(dirname "$0")"; mkdir -p out
gcc -std=gnu11 -O2 -fwrapv ws_test.c ws.c ws_fight.c ws_deck.c ws_view.c ../giant/giant.c -lm -o out/ws_test || exit 1
./out/ws_test
gcc -std=gnu11 -O2 -fwrapv ms_test.c ms.c ws.c ../giant/giant.c -lm -o out/ms_test && ./out/ms_test || exit 1
gcc -std=gnu11 -O2 -fwrapv -w mix_test.c ws.c ws_deck.c ../shipyard/sy_design.c ../giant/giant.c -lm -o out/mix_test && ./out/mix_test || exit 1      # the ROOM MIX option (a shipyard design drives a deck)
bash ./bal.sh quick > out/bal_quick.txt || { tail -25 out/bal_quick.txt; exit 1; }; tail -1 out/bal_quick.txt
