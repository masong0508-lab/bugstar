#!/bin/bash
# Keyboard, gamepad, mouse, Esc options (video/audio/rebind), F11, F12 with SDL events injected under the dummy video driver (BUGSTAR_EVENTS).
cd "$(dirname "$0")/.."; rm -rf out/inp; mkdir -p out/inp; fail=0
BUGSTAR_EVENTS=tests/inputs.txt SDL_AUDIODRIVER=dummy timeout 120 ./bugstar_pc --video-driver dummy --no-audio --no-vsync --fast --data out/inp --stop-after 100000 2> out/inp/err.txt >/dev/null; rc=$?
d(){ grep "^EV dump" out/inp/err.txt | sed -n "$1p"; }
chk(){ d $1 | grep -q "$2" || { echo "inputs: dump $1 lacks '$2': $(d $1)"; fail=1; }; }
[ $rc = 0 ] || { echo "inputs: exit code $rc"; fail=1; }
chk 2 "keys=001"; chk 3 "keys=010"; chk 4 "keys=000"; chk 5 "keys=008"; chk 6 "keys=001"; chk 7 "keys=010"; chk 8 "keys=040"
chk 9 "keys=001"; chk 9 "mouse=120,80"
chk 11 "menu=1 page=0"; chk 12 "menu=1 page=2"; chk 13 "vol=85"; chk 14 "page=0"; chk 15 "page=3"; chk 16 "wait=1"; chk 17 "kb_A=J/Space"
chk 18 "menu=0"; chk 19 "fs=1"; chk 20 "fs=0"; chk 21 "menu=1"; chk 22 "menu=0"; chk 23 "menu=0"
ls out/inp/screenshots/*.png >/dev/null 2>&1 || { echo "inputs: F12 made no screenshot"; fail=1; }
grep -q "^EV" out/inp/err.txt && grep -i "EV bad\|failed" out/inp/err.txt && fail=1
[ -s out/inp/settings.ini ] && grep -q "^key_A=J" out/inp/settings.ini || { echo "inputs: rebind not saved"; fail=1; }
[ $fail = 0 ] && echo "inputs: OK (keys, pad buttons+stick, mouse, Esc menu, volume, rebind saved, F11 both ways, F12 screenshot, pad BACK+START menu)" || { cat out/inp/err.txt | head -40; exit 1; }
