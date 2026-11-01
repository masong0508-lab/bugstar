#!/bin/bash
# The STORMCLOCK COIL: (1) the rule self-test (drop truth table, exactly 10 per 100,000 rolls, each missing condition blocks the drop),
# (2) the glitch fuzz: every speed tier for thousands of frames, normal and flash-safe, real SRAM unchanged, no crash or hang.
# It builds its own test copy (pc/bugstar_pc_coil) and puts the normal build back afterwards.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/c1.sav
PC_CFLAGS="-DTEST_COIL -DTEST_COILFUZZ=1500 -DPC_COILREPORT" ./build.sh >/dev/null 2>&1 || { echo "coil: build failed"; exit 1; }
cp bugstar_pc bugstar_pc_coil; ./build.sh >/dev/null 2>&1
st=$(BUGSTAR_COILTEST=1 ./bugstar_pc_coil --script tests/coilfuzz.txt --out out/c1 --sav out/c1.sav --frames 300 2>&1 | grep "coil self-test")
echo "$st"; echo "$st" | grep -q "ok (0 checks failed)" ||  { echo "coil: SELF-TEST FAILED"; exit 1; }
r=$(timeout 900 ./bugstar_pc_coil --script tests/coilfuzz.txt --out out/c1 --sav out/c1.sav --frames 40000 2>&1 | grep "coilfuzz:")
echo "$r"; echo "$r" | grep -q "ok" || { echo "coil: FUZZ FAILED"; exit 1; }
echo "coil: OK"
