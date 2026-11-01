#!/bin/bash
# AFTERMATH of a severe warship (pc/pc_missions.inc): hellscape, burning-surface escape, sealed entrance + dig, floor invasion, boarding party.
# One headless run each, driven by the test hooks (BUGSTAR_WS_TEST=hell|dun, BUGSTAR_AFT_*); the events come out on stderr (BUGSTAR_AFT_LOG).
#  1. no preparation: wreck, no pod, no wingman, 30 HP -> the fire kills, gold -20 percent + cargo, you wake in space; the same twice
#  2. prepared: wreck + wingman + 3000 HP -> picked up, alive, gold untouched
#  3. sealed entrance: blocked, a second stair appears, holding A digs through
#  4. floor invasion: troops on the floor and the stairs held; 5. boarding party spawns
cd "$(dirname "$0")/.."; mkdir -p out; B=./bugstar_pc; export BUGSTAR_SIDE=0 BUGSTAR_WS_AT=650 BUGSTAR_AFT_LOG=1; fail=0
run(){ n=$1; shift; rm -f out/$n.sav out/$n.sav.*; env "$@" $B --script ${KEYS:-tests/aft_idle.txt} --out out/$n --sav out/$n.sav --frames ${FR:-12000} > out/$n.log 2> out/$n.err; }
need(){ grep -q "$2" out/$1.err || { echo "aftermath: run $1 lacks '$2'"; fail=1; }; }
run afa BUGSTAR_WS_TEST=hell,0,0 BUGSTAR_AFT_SHIP=2 BUGSTAR_AFT_POD=0 BUGSTAR_AFT_GOLD=1000
run afb BUGSTAR_WS_TEST=hell,0,0 BUGSTAR_AFT_SHIP=2 BUGSTAR_AFT_POD=0 BUGSTAR_AFT_GOLD=1000
cmp -s out/afa.err out/afb.err || { echo "aftermath: two runs differ"; fail=1; }
need afa "aft hell ship 2 pod 0"; need afa "aft death gold 1000 -> 800"
run afc BUGSTAR_WS_TEST=hell,0,0 BUGSTAR_AFT_SHIP=2 BUGSTAR_AFT_POD=0 BUGSTAR_AFT_GOLD=1000 BUGSTAR_AFT_WING=1 BUGSTAR_AFT_HP=3000
need afc "aft rescued"; grep -q "aft death" out/afc.err && { echo "aftermath: prepared run died"; fail=1; }
KEYS=tests/aft_dig.txt FR=5600 run afd BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=seal BUGSTAR_AFT_TOPUP=1
need afd "aft sealed"; need afd "aft blocked_at_start 1"; need afd "aft alt "; need afd "aft dug"
FR=5000 run afe BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=inv BUGSTAR_AFT_TOPUP=1
need afe "aft wave floor 1"; need afe "aft troops floor 1"; need afe "aft up_blocked 1"
FR=4000 run aff BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=board BUGSTAR_AFT_TOPUP=1
need aff "aft boarders "
[ $fail = 0 ] && echo "aftermath: hell kills the unprepared (-20% gold), prepared escape works, entrance seals and digs out, troops hold the stairs, boarders spawn; two runs equal" || exit 1
