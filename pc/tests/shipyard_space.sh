#!/bin/bash
# Your ship design and the allied/enemy designs are DRAWN in space by the PC ship renderer (pc/shipr): headless, SHIPS=NATIVE.
# The same flight with the design on / off / foes on gives different pictures; each is the same on two runs.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/ss*
B=./bugstar_pc; run(){ BUGSTAR_SY_PIRATES=6 BUGSTAR_SY_AT=100 PC_SHIPS=1 PC_NATIVE=2 PC_SCALE=1 BUGSTAR_SY_TEST=$2 $B --script tests/shipyard_space.txt --out out/ss$1 --sav out/ss$1.sav --frames 3600 > out/ss$1.log; }
run a 9,0; run b 9,0; run c 9,1; run d 1,0
h(){ md5sum < out/ss$1_003500.ppm; }
[ "$(h a)" = "$(h b)" ] || { echo "shipyard_space: DIFFERENT between two runs"; exit 1; }
[ "$(h a)" != "$(h c)" ] || { echo "shipyard_space: foe designs change nothing in the picture"; exit 1; }
[ "$(h a)" != "$(h d)" ] || { echo "shipyard_space: another design looks the same in space"; exit 1; }
echo "shipyard_space: own design, other design and foe designs all look different in space, SAME on two runs"
