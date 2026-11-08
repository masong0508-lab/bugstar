#!/bin/bash
# STEADY MENUS (the GBA flicker fix, main.c shd_begin/shd_end): the universe jump menu and the universe number typer, played by script in the real game.
# With no key pressed, picture after picture must be exactly the same; a key must change the picture; the digits typed must show; B must leave (the screen returns).
# (The beam is not drawn here, so tearing itself is checked on the GBA build with mGBA; this guards the logic: nothing redraws or blinks by itself, the page flip leaves the right page up.)
cd "$(dirname "$0")/.."; mkdir -p out/steady; B=./bugstar_pc; fail=0
pre="300 304 A\n600 604 A\n900 904 A\n1200 1204 S\n1500 1504 S\n2000 2004 S\n2300 2304 A\n2700 2704 A\n"
run(){ # mode script frames
    printf "$pre$2" > out/steady/$1.txt; rm -f out/steady/$1_*.ppm out/steady/$1.sav*
    BUGSTAR_SIDE=1 BUGSTAR_LS_TIPS=0 BUGSTAR_WS_AT=650 BUGSTAR_WS_TEST=$1,0,0 BUGSTAR_SET_FREED=1 timeout 300 $B --script out/steady/$1.txt --out out/steady/$1 --sav out/steady/$1.sav --frames $3 > out/steady/$1.log 2> out/steady/$1.err || { echo "steady: $1 crashed"; fail=1; return; }
    grep -q "^screen $1 returned" out/steady/$1.err || { echo "steady: $1 never returned"; fail=1; }
}
same(){ cmp -s out/steady/$1_$2.ppm out/steady/$1_$3.ppm; }
run unisel "3300 3330 r\n3500 3504 B\nshot 3200\nshot 3260\nshot 3290\nshot 3340\nshot 3380\n" 3800
same unisel 003200 003260 || { echo "steady: universe menu changes by itself (3200 vs 3260)"; fail=1; }
same unisel 003340 003380 || { echo "steady: universe menu changes by itself after a key (3340 vs 3380)"; fail=1; }
same unisel 003200 003340 && { echo "steady: universe menu did not change when RIGHT was held"; fail=1; }
run unityp "3300 3304 U\n3340 3344 l\n3380 3384 U\n3500 3504 B\nshot 3200\nshot 3260\nshot 3320\nshot 3360\nshot 3420\nshot 3460\n" 3800
same unityp 003200 003260 || { echo "steady: the number typer changes by itself (3200 vs 3260)"; fail=1; }
same unityp 003200 003320 && { echo "steady: the number typer did not show the digit UP changed"; fail=1; }
same unityp 003420 003460 || { echo "steady: the number typer changes by itself after keys (3420 vs 3460)"; fail=1; }
same unityp 003320 003420 && { echo "steady: the number typer did not show the second digit"; fail=1; }
[ $fail = 0 ] && echo "steady: OK (universe menu and number typer: still when no key is pressed, a key changes the picture, both leave with B)" || exit 1
