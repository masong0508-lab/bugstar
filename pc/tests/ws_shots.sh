#!/bin/bash
# Pictures of warship interiors in the real game (native 720x480): pc/tests/ws_shots.sh [Z SEED] -> out/wr<kind>.png and out/rooms*.png
cd "$(dirname "$0")/.."; mkdir -p out; Z=${1:-520}; SEED=${2:-0}
for k in 0 1 2 4 5 6 7 8 9 10 11 13; do rm -f out/wr.sav*
  PC_SCALE=3 PC_NATIVE=1 BUGSTAR_SIDE=0 BUGSTAR_WS_TEST=inside,$Z,$SEED BUGSTAR_WS_AT=650 BUGSTAR_WS_ROOM=$k ./bugstar_pc --script tests/ws_room.txt --out out/wr$k --sav out/wr.sav --frames 2910 >/dev/null
  python3 -c "from PIL import Image; Image.open('out/wr${k}_002900.ppm').save('out/wr$k.png')"; done
