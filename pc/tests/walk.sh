#!/bin/bash
# BOARDING A FRIEND AND YOUR OWN SHIP (patch 45): the scanner rows ALLY and YOUR SHIP open a deck you walk in peace (no guards, no targets, crew only).
#  - YOUR SHIP uses the ship you built in the ship editor (the default design here): its crates, guns and quarters are the rooms and props of the deck
#  - ALLY (a convoy craft) uses the team 0 design sy_foe makes for its craft seed; warship/sy_mix_check.c works the numbers out on its own and the deck must show the same crates, guns and turrets
#  - an ally deck with designs off uses the plain mix; leaving an ally refits you
cd "$(dirname "$0")/.."; mkdir -p out/wk; rm -f out/wk/*; B=./bugstar_pc; fail=0
gcc -std=gnu11 -O2 -w -o out/wk/chk warship/sy_mix_check.c shipyard/sy_design.c -lm || { echo "walk: cannot build sy_mix_check"; exit 1; }
cat > out/wk/own.txt <<'K'
300 304 A
600 604 A
900 904 A
1200 1204 S
1500 1504 S
2000 2004 S
2300 2304 A
2700 2704 A
3100 3104 U
3200 3204 A
3300 3304 S
shot 3500
K
sed 's/^3100 3104 U/3100 3104 U\n3150 3154 U/' out/wk/own.txt > out/wk/ally.txt
go(){ local n=$1 s=$2; shift 2; rm -f out/wk/$n.sav*; env BUGSTAR_SIDE=0 BUGSTAR_WS_TEST=mscan,0,0 BUGSTAR_WS_AT=0 BUGSTAR_MS_SYNC=3000 "$@" timeout 300 $B --script out/wk/$s.txt --out out/wk/$n --sav out/wk/$n.sav --frames 4200 2> out/wk/$n.err >/dev/null || { echo "walk: $n CRASHED"; fail=1; }; grep "walk mode" out/wk/$n.err | tail -1; }
g(){ echo "$1" | tr ' ' '\n' | grep "^$2=" | head -1 | cut -d= -f2; }
own=$(go own own); ally1=$(go ally1 ally BUGSTAR_WS_ALLY=200 BUGSTAR_WS_MIX=1); ally0=$(go ally0 ally BUGSTAR_WS_ALLY=200 BUGSTAR_WS_MIX=0)
for n in own ally1 ally0; do l=$(eval echo \$$n); [ -n "$l" ] || { echo "walk: $n never walked"; fail=1; continue; }
    [ "$(g "$l" guards)" = 0 ] && [ "$(g "$l" targets)" = 0 ] || { echo "walk: $n has guards or targets: $l"; fail=1; }; done
echo "$own" | grep -q "mode=2" || { echo "walk: own is not mode 2"; fail=1; }
echo "$ally1" | grep -q "mode=1" || { echo "walk: ally is not mode 1"; fail=1; }
d=$(out/wk/chk default); seed=$(g "$ally1" seed); t0=$(out/wk/chk foe $seed 0 5); t1=$(out/wk/chk foe $seed 1 5)
for k in crates guns turret quarters; do
  [ "$(g "$own" $k)" = "$(g "$d" $k)" ] || { echo "walk: own $k $(g "$own" $k) != design $(g "$d" $k)"; fail=1; }
  [ "$(g "$ally1" $k)" = "$(g "$t0" $k)" ] || { echo "walk: ally $k $(g "$ally1" $k) != team 0 design $(g "$t0" $k)"; fail=1; }; done
[ "$t0" != "$t1" ] || { echo "walk: team 0 and team 1 designs are the same, the test proves nothing"; fail=1; }
[ "$(g "$ally0" bunks)" = -1 ] || { echo "walk: ally with designs off should use the plain mix"; fail=1; }
[ $fail = 0 ] && echo "walk: OK (own deck = your design, ally deck = team 0 design [$t0] not the foe's [$t1], designs off = plain mix, no guards, no targets)" || exit 1
