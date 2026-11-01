#!/bin/bash
# The STORMCLOCK COIL: (0) the wide-number code against the Python reference (exact decimal maths: pc/tests/coilref.py), on its own and inside the game build,
# (1) the rule self-test (drop truth table, exactly 10 per 100,000 rolls, each missing condition blocks the drop, the speed ladder up to STORM, wide numbers that
# never overflow, the texts and trip times down to the Planck time, the STORM lock, pass-through safety in every mode),
# (2) the glitch + pass-through fuzz: 4 pass-through modes x flash-safe off/on x every speed (off, 7 tiers, STORM x 1 million) for thousands of frames, real SRAM
# unchanged, hull never under 10 from a pass-through, no crash or hang, and the save block (also an older save, and the options block).
# It builds its own test copy (pc/bugstar_pc_coil) and puts the normal build back afterwards.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/c1.sav
python3 tests/coilref.py out/coilref.txt || { echo "coil: the Python reference failed"; exit 1; }
cc -O1 -w -fsanitize=undefined,address -o out/cn_host tests/cn_host.c && out/cn_host out/coilref.txt || { echo "coil: NUMBERS DIFFER FROM THE PYTHON REFERENCE"; exit 1; }
PC_CFLAGS="-DTEST_COIL -DTEST_COILFUZZ=1500 -DPC_COILREPORT" ./build.sh >/dev/null 2>&1 || { echo "coil: build failed"; exit 1; }
cp bugstar_pc bugstar_pc_coil; ./build.sh >/dev/null 2>&1
st=$(BUGSTAR_COILREF=out/coilref.txt BUGSTAR_COILTEST=1 ./bugstar_pc_coil --script tests/coilfuzz.txt --out out/c1 --sav out/c1.sav --frames 300 2>&1 | grep -E "coil self-test|FAIL|MISMATCH|coil numbers")
echo "$st"; echo "$st" | grep -q "ok (0 checks failed)" ||  { echo "coil: SELF-TEST FAILED"; exit 1; }
r=$(timeout 1800 ./bugstar_pc_coil --script tests/coilfuzz.txt --out out/c1 --sav out/c1.sav --frames 400000 2>&1 | grep -E "coilfuzz:|coil soak|coverage")
echo "$r"; echo "$r" | grep -q "coil soak: ok" && echo "$r" | grep -q "save block test ok" || { echo "coil: FUZZ FAILED"; exit 1; }
echo "coil: OK"
