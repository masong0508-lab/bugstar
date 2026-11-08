#!/bin/bash
# tr_all.sh [ROM] [only-names...]: beam-level checks of the screen transitions (mGBA, tr.py): torn frames, controls changed while drawing, blank runs
R=$(readlink -f "${1:-/home/claude/gba-GF/bugstar.gba}"); shift; ONLY="$*"
cd "${HARNESS:-$(dirname "$0")}"
B='20 24 A\n700 704 A\n800 804 A\n900 904 S\n1000 1004 S\n1300 1304 A\n1400 1404 A\n1500 1504 A\n2000 2004 S'
UP='poke 2200 @launch_flag 1 4'
UNLOCK='poke 2300 @ant_king 1 4\npoke 2300 @zones_open 6 4\npoke 2300 @cleared_mask 63 4\npoke 2300 @fuel 900 4'
run(){ n=$1; f0=$2; f1=$3; sc=$4; [ -n "$ONLY" ] && ! echo " $ONLY " | grep -q " $n " && return; python3 tr.py $R bS_abig.sav $n $f0 $f1 "$B\n$sc"; }
run launch 2190 2700 "$UP"
run land 2320 2800 "$UP\npoke 2300 @speed 0 4\npoke 2300 @shx 0 4\npoke 2300 @shy 0 4\npoke 2300 @shz 0xFFFFF060 4\n2330 2334 A\n2400 2404 A"
run voyage 2790 3700 "$UP\n$UNLOCK\n2400 2404 s\n2700 2704 r\n2800 2804 A"
run dungeon 2190 3600 "poke 2200 @enter_dungeon_flag 1 4\npoke 3000 @hearts 0 4\npoke 3000 @hearts+4 0 4"
run forge 2190 3600 "poke 2200 @enter_dungeon_flag 3 4\npoke 3000 @hearts 0 4\npoke 3000 @hearts+4 0 4"
run kartq 2590 3100 "poke 2200 @kart_req 1 4\n2300 2304 S\n2500 2504 S\n2600 2604 A\n2700 2704 A\n2800 2804 A"
run lair 2790 3700 "$UP\n$UNLOCK\npoke 2300 @beast_stage 3 4\npoke 2300 @galaxy_no 0 4\n2400 2404 s\n2700 2704 L\n3300 3304 s"
