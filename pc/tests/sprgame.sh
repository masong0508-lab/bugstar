#!/bin/bash
# The procedural sprites in the game's views (patch 46): the generated dungeon monsters and the space backdrop are in the picture (different from BUGSTAR_SPRGEN=0), two runs are
# the same, the background maker gives the same finished picture as making everything at once, and the game thread never spends 20 ms in a generator with it on.
cd "$(dirname "$0")/.."; mkdir -p out/sgm; fail=0; B=./bugstar_pc
run(){ n=$1; shift; rm -f out/sgm/$n.sav*; env PC_NATIVE=2 PC_SCALE=5 BUGSTAR_SIDE=0 BUGSTAR_VT_AT=100 PC_V3D=1 "$@" $B --script tests/vt_idle.txt --out out/sgm/$n --sav out/sgm/$n.sav --frames 3150 > out/sgm/$n.log 2> out/sgm/$n.err; md5sum < out/sgm/${n}_003140.ppm 2>/dev/null | cut -c1-16; }
for v in "dungeon,3,0,0 BUGSTAR_SG_DNDEMO=1" "space,1,150,0,0"; do
  set -- $v; mode=$1; shift; tag=$(echo $mode | cut -c1-3)
  a=$(run ${tag}_on BUGSTAR_VT=$mode "$@"); b=$(run ${tag}_on2 BUGSTAR_VT=$mode "$@"); o=$(run ${tag}_off BUGSTAR_VT=$mode BUGSTAR_SPRGEN=0 "$@")
  s=$(run ${tag}_async BUGSTAR_VT=$mode BUGSTAR_SG_ASYNC=1 BUGSTAR_SG_LOG=1 "$@")
  [ -n "$a" ] || { echo "sprgame: $tag no picture"; fail=1; continue; }
  [ "$a" = "$b" ] || { echo "sprgame: $tag two runs differ"; fail=1; }
  [ "$a" != "$o" ] || { echo "sprgame: $tag looks the same with the generators off"; fail=1; }
  [ "$a" = "$s" ] || { echo "sprgame: $tag the background maker's finished picture differs from the direct one"; fail=1; }
  w=$(grep -o "worst frame [0-9.]*" out/sgm/${tag}_async.err | awk '{print $3}'); [ -n "$w" ] && awk -v w=$w 'BEGIN{exit !(w<20)}' || { echo "sprgame: $tag game thread spent ${w:-?} ms in one frame in generators"; fail=1; }
done
[ $fail = 0 ] && echo "sprgame: OK (dungeon monsters and space backdrop drawn, same on two runs, background maker = direct, no generator frame over 20 ms)" || exit 1
