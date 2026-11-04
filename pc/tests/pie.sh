#!/bin/bash
# patch 25: the position-independent build (PC_PIE=1, what the Android APK is) must play the same game as the normal build.
# Build it first:  PC_PIE=1 PC_GBA_TOP=0x30000000 PC_SDL=0 PC_OUT=bugstar_pie ./pc/build.sh
cd "$(dirname "$0")/.."; mkdir -p out
B=./bugstar_pie; [ -x $B ] || { echo "pie: build $B first (see the top of this file)"; exit 1; }
rm -f out/pie.sav
want=$(cat tests/pie.golden)
$B --script tests/play.txt --out out/pie --sav out/pie.sav --frames 4501 > out/pie.log 2>out/pie.err
got=$(tail -1 out/pie.log)
if [ "$got" = "$want" ]; then echo "pie: SAME as the normal build ($got)"; else echo "pie: DIFFERENT"; echo " want $want"; echo " got  $got"; exit 1; fi
