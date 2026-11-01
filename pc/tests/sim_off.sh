#!/bin/bash
# Proof that the side game does not change the core game.
#  1. With the side game OFF (BUGSTAR_SIDE=0, same as the SIDE GAME option OFF) the action game is byte for byte the old one: the same pictures and the
#     same save as tests/sim_off.golden, which was made by the build from before the first side-game commit.
#  2. With it ON the .sav file is still exactly the same (the side game keeps its own <sav>.sim), and no .sim file is made when it is OFF.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/so_off.sav* out/so_on.sav*
BUGSTAR_SIDE=0 ./bugstar_pc --script tests/play.txt --out out/so_off --sav out/so_off.sav --frames 4501 > out/so_off.log
./bugstar_pc --script tests/play.txt --out out/so_on --sav out/so_on.sav --frames 4501 > out/so_on.log
cmp -s out/so_off.log tests/sim_off.golden || { echo "sim_off: OFF is different from the golden"; diff out/so_off.log tests/sim_off.golden | head; exit 1; }
[ ! -e out/so_off.sav.sim ] || { echo "sim_off: a .sim file was written while OFF"; exit 1; }
cmp -s out/so_off.sav out/so_on.sav || { echo "sim_off: the .sav differs with the side game ON"; exit 1; }
[ -e out/so_on.sav.sim ] || { echo "sim_off: ON wrote no .sim file"; exit 1; }
echo "sim_off: OK (OFF = golden pictures + save, ON = same .sav, own .sim file)"
