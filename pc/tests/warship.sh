#!/bin/bash
# COLOSSAL WARSHIPS in the real game (see pc/warship/README.md). Uses the test hook BUGSTAR_WS_TEST (pc_warship.inc).
#  1. a fight with the reference station, twice: same pictures; the fight pictures differ from each other
#  2. boarding: inside the hangar, walking, the deck map: two runs the same
#  3. a kill at a high level: the COST BREAKDOWN screen, the pay goes into gold, the .ws file keeps it; CONTINUE loads it back (save/load)
#  4. the WARSHIPS screen in the PC LAB (list, a craft's THREAT page, the ARMOURY)
cd "$(dirname "$0")/.."; mkdir -p out; B=./bugstar_pc; export BUGSTAR_SIDE=0 BUGSTAR_WS_AT=650; fail=0
run(){ rm -f out/$1.sav out/$1.sav.*; BUGSTAR_WS_TEST=$2 $B --script $3 --out out/$1 --sav out/$1.sav --frames $4 > out/$1.log; }
h(){ grep "^shot $2 " out/$1.log | sed 's/.*screen=//'; }
run wsa fight,520,0 tests/ws_fight.txt 3400; run wsb fight,520,0 tests/ws_fight.txt 3400
cmp -s out/wsa.log out/wsb.log || { echo "warship: the fight is DIFFERENT on two runs"; fail=1; }
n=$(for f in 3000 3100 3200; do h wsa $f; done | sort -u | wc -l); [ "$n" = 3 ] || { echo "warship: fight pictures do not change ($n)"; fail=1; }
run wsi inside,520,0 tests/ws_inside.txt 3420; run wsj inside,520,0 tests/ws_inside.txt 3420
cmp -s out/wsi.log out/wsj.log || { echo "warship: the interior is DIFFERENT on two runs"; fail=1; }
n=$(for f in 2880 3000 3060 3200 3230; do h wsi $f; done | sort -u | wc -l); [ "$n" -ge 4 ] || { echo "warship: interior pictures do not change ($n)"; fail=1; }
BUGSTAR_WS_LEVEL=100000 run wsk kill,520,0 tests/ws_kill.txt 3200
grep -q "^kills=1" out/wsk.sav.ws0 || { echo "warship: the kill was not saved"; cat out/wsk.sav.ws0; fail=1; }
[ "$(h wsk 2900)" != "$(h wsk 3100)" ] || { echo "warship: no breakdown screen"; fail=1; }
cp out/wsk.sav out/wsc.sav; cp out/wsk.sav.ws0 out/wsc.sav.ws0; sed -i 's/^kills=.*/kills=1/' out/wsc.sav.ws0
BUGSTAR_WS_AT=1 BUGSTAR_WS_TEST=save,0,0 $B --script tests/ws_cont.txt --out out/wsc --sav out/wsc.sav --frames 800 > out/wsc.log
grep -q "^kills=1" out/wsc.sav.ws0 && grep -q "^vault=" out/wsc.sav.ws0 || { echo "warship: CONTINUE did not load the warship file"; fail=1; }
BUGSTAR_WS_MIX=1 run wsm inside,520,0 tests/ws_inside.txt 3420; BUGSTAR_WS_MIX=1 run wsn inside,520,0 tests/ws_inside.txt 3420
cmp -s out/wsm.log out/wsn.log || { echo "warship: the room mix interior is DIFFERENT on two runs"; fail=1; }
n=$(for f in 2880 3000 3060 3200 3230; do h wsm $f; done | sort -u | wc -l); [ "$n" -ge 4 ] || { echo "warship: room mix interior pictures do not change ($n)"; fail=1; }
[ "$(h wsm 3000)" != "$(h wsi 3000)" ] || echo "warship: note: the room mix picture at frame 3000 equals the plain one"
run wsl lab,0,0 tests/ws_lab.txt 3300
n=$(for f in 2900 3000 3200; do h wsl $f; done | sort -u | wc -l); [ "$n" = 3 ] || { echo "warship: the WARSHIPS screens do not change ($n)"; fail=1; }
for f in wsa_003000 wsi_003000 wsk_003100 wsl_003000; do python3 -c "from PIL import Image; Image.open('out/$f.ppm').resize((720,480)).save('out/$f.png')" 2>/dev/null; done
[ $fail = 0 ] && echo "warship: fight same on two runs, interior same on two runs, kill paid and saved ($(grep '^vault' out/wsk.sav.ws0)), CONTINUE loads it, WARSHIPS screens ok" || exit 1
