#!/bin/bash
# PROGRESSIVE DISCLOSURE of the life sim in the real game (patch 47, pc/sim/unlock.h). The game's own test dump has an "unlock" line:
#   unlock have <flags> told <flags> opened <flags> cards N toasts N crowds N names <features> fresh <features not opened yet> qm <rows> rows <side menu rows>
# Test hooks (BUGSTAR_LS_*): SHOW=0|1|2 (SHOW ALL option), TIPS=0|1, POP=0|1|2 (POPUP LEVEL), PAUSE=0|1 (AUTO-PAUSE), BIG=0|1, GRANT=a,b (open features silently),
# EARLY=0 (no first-10-minutes rule). BUGSTAR_SET_FAME / _FREED / _GOLD / _LEVEL set the action game's numbers.
#  1. NEW GAME: nothing of the life sim is shown but the clock; its tip card comes at once; the side menu holds only TIPS; after the talk story JOBS/NEEDS come one by one.
#  2. LATE GAME (fame 1300, 9 freed, gold, level 500): every menu row; a crowd of new things gets ONE summary card; opening a row clears its !.
#  3. SHOW ALL: SHAPES shows ??? rows, ALL shows every row and no tips; TIPS OFF: no cards and no toasts, the ! stays; POPUP LEVEL FEW/MANY change cards into toasts and back.
#  4. flags are saved and come back after CONTINUE.   Two runs of each are the same.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/ls_*
last(){ grep "^unlock" out/ls_$1.dump | tail -1; }
first_with(){ grep "^unlock" out/ls_$1.dump | grep -m1 "names [a-z,]*$2" ; }
f(){ echo "$1" | awk -v k=$2 '{for(i=1;i<=NF;i++) if($i==k) print $(i+1)}'; }
run(){ n=$1; scr=$2; fr=$3; shift 3; rm -f out/ls_$n.*; env BUGSTAR_SIM_DUMP=out/ls_$n.dump BUGSTAR_SIM_DUMP_EVERY=${EVERY:-200} "$@" ./bugstar_pc --script tests/$scr --out out/ls_$n --sav out/ls_$n.sav --frames $fr > out/ls_$n.log; }
fail(){ echo "sim_ls: $*"; exit 1; }
h(){ grep "^shot $2 " out/ls_$1.log | sed 's/.*screen=//'; }
has(){ case ",$1," in *",$2,"*) return 0;; esac; return 1; }

# ---- 1. a new game, twice
run new1 sim_ls_new.txt 8100; run new2 sim_ls_new.txt 8100
cmp -s out/ls_new1.dump out/ls_new2.dump && cmp -s out/ls_new1.log out/ls_new2.log || fail "new game: DIFFERENT between two runs"
l=$(last new1); names=$(f "$l" names)
[ "$names" = "clock,weather" ] || fail "new game: the first minutes opened more than the clock and the weather: $names"
[ "$(f "$l" cards)" = 1 ] || fail "new game: the clock's tip card was not shown once: $l"
has "$(f "$l" rows)" TIPS && [ "$(f "$l" qm)" = 1 ] || fail "new game: the side menu is not just TIPS: $l"
[ "$(h new1 2900)" != "$(h new1 3100)" ] && [ "$(h new1 3100)" != "$(h new1 3600)" ] && [ "$(h new1 3600)" != "$(h new1 3800)" ] && [ "$(h new1 3800)" != "$(h new1 4000)" ] || fail "new game: tip card / HUD / menu / TIPS page / tip pictures are not all different"
[ "$(grep "^sim " out/ls_new1.dump | tail -1 | awk '{print $3}')" -ge 600 ] || fail "new game: the game did not go on after the menus (tick $(grep "^sim " out/ls_new1.dump | tail -1 | cut -c1-30))"

# ---- 1b. the talk story: MEET, then JOBS, then NEEDS (the first 10 minutes, then days)
EVERY=1500 run talk1 sim_ls_talk.txt 71000 BUGSTAR_LS_PAUSE=0 BUGSTAR_SIM_NEAR=0; EVERY=1500 run talk2 sim_ls_talk.txt 71000 BUGSTAR_LS_PAUSE=0 BUGSTAR_SIM_NEAR=0
cmp -s out/ls_talk1.dump out/ls_talk2.dump || fail "talk: DIFFERENT between two runs"
a=$(first_with talk1 meet); b=$(first_with talk1 jobs); c=$(first_with talk1 needs)
[ -n "$a" ] && [ -n "$b" ] || fail "talk: no MEET / JOBS ($a / $b)"
has "$(f "$a" names)" jobs && fail "talk: JOBS came with the first talk"
[ -z "$c" ] || [ "$(grep -n "^unlock" out/ls_talk1.dump | grep -m1 "names [a-z,]*needs" | cut -d: -f1)" -gt "$(grep -n "^unlock" out/ls_talk1.dump | grep -m1 "names [a-z,]*jobs" | cut -d: -f1)" ] || fail "talk: NEEDS before JOBS"
[ "$(h talk1 12050)" != "$(h talk1 40850)" ] && [ "$(h talk1 40850)" != "$(h talk1 69650)" ] || fail "talk: the talk card did not grow (name only, then the job, then needs)"

# ---- 2. late game: a crowd at once -> one summary card; every row; opening a row clears its !
L="BUGSTAR_LS_GRANT=meet BUGSTAR_SET_FAME=1300 BUGSTAR_SET_FREED=9 BUGSTAR_SET_LEVEL=500 BUGSTAR_SET_GOLD=500000 BUGSTAR_LS_EARLY=0"
run late1 sim_ls_late.txt 20000 $L; run late2 sim_ls_late.txt 20000 $L
cmp -s out/ls_late1.dump out/ls_late2.dump && cmp -s out/ls_late1.log out/ls_late2.log || fail "late game: DIFFERENT between two runs"
l=$(last late1); [ "$(f "$l" crowds)" = 1 ] && [ "$(f "$l" cards)" = 0 ] || fail "late game: no single summary card: $l"
[ "$(f "$l" qm)" = 9 ] || fail "late game: not all 9 rows: $l"
has "$(f "$l" names)" armada && has "$(f "$l" names)" mayor && has "$(f "$l" names)" gods || fail "late game: names: $(f "$l" names)"
fr=$(f "$l" fresh); has "$fr" colony && fail "late game: the COLONY row still has its ! after it was opened ($fr)"
has "$fr" gods || fail "late game: GOD POWERS lost its ! without being opened ($fr)"
[ "$(h late1 3000)" != "$(h late1 3600)" ] && [ "$(h late1 3600)" != "$(h late1 4300)" ] || fail "late game: summary card / side menu / colony pictures are not all different"

# ---- 3. SHOW ALL, TIPS, POPUP LEVEL
run shp sim_ls_menu.txt 12000 BUGSTAR_LS_PAUSE=0 BUGSTAR_LS_SHOW=1 BUGSTAR_LS_GRANT=meet,colony
l=$(last shp); case "$l" in *"???"*) ;; *) fail "SHAPES: no ??? rows: $l";; esac
run all sim_ls_menu.txt 14000 BUGSTAR_LS_SHOW=2 BUGSTAR_SET_FAME=1300
l=$(last all); [ "$(f "$l" qm)" = 9 ] && [ "$(f "$l" cards)" = 0 ] && [ "$(f "$l" toasts)" = 0 ] || fail "SHOW ALL=ALL: not 9 rows or a tip was shown: $l"
run off sim_ls_idle.txt 8100 BUGSTAR_LS_TIPS=0
l=$(last off); [ "$(f "$l" cards)" = 0 ] && [ "$(f "$l" toasts)" = 0 ] && [ "$(f "$l" told)" = "$(f "$l" have)" ] && [ "$(f "$l" fresh)" != "-" ] || fail "TIPS OFF: a tip was shown, or the ! marks are gone: $l"
run few sim_ls_idle.txt 8100 BUGSTAR_LS_POP=0
l=$(last few); [ "$(f "$l" cards)" = 0 ] && [ "$(f "$l" toasts)" -ge 1 ] || fail "POPUP LEVEL FEW: the clock still stopped the game: $l"
run many sim_ls_new.txt 8100 BUGSTAR_LS_POP=2
l=$(last many); [ "$(f "$l" cards)" -ge 1 ] || fail "POPUP LEVEL MANY: no card: $l"
run nop sim_ls_idle.txt 8100 BUGSTAR_LS_PAUSE=0
l=$(last nop); [ "$(f "$l" cards)" = 0 ] && [ "$(f "$l" toasts)" -ge 1 ] || fail "AUTO-PAUSE OFF: a card stopped the game: $l"
run big sim_ls_new.txt 8100 BUGSTAR_LS_BIG=1
[ "$(h big 2900)" != "$(h new1 2900)" ] || fail "BIG TIP TEXT: the card looks the same"
# the first 10 minutes: nothing but the basics even with the numbers of a late game
run early sim_ls_idle.txt 8100 BUGSTAR_LS_PAUSE=0 BUGSTAR_SET_FAME=1300 BUGSTAR_SET_FREED=9 BUGSTAR_SET_GOLD=900
l=$(last early); n=$(f "$l" names); has "$n" colony || has "$n" gods || has "$n" mayor || has "$n" armada || has "$n" money || [ -n "$n" ] || fail "early: nothing opened"; for x in colony gods mayor armada money fame; do has "$n" $x && fail "the first 10 minutes opened $x ($n)"; done

# ---- 4. the flags are saved with the game and come back
rm -f out/ls_sv.*; BUGSTAR_LS_PAUSE=0 BUGSTAR_LS_GRANT=meet,jobs,colony BUGSTAR_SIM_DUMP=out/ls_sv.dump BUGSTAR_SIM_DUMP_EVERY=100 ./bugstar_pc --script tests/sim_town.txt --out out/ls_sv --sav out/ls_sv.sav --frames 12000 > out/ls_sv.log
[ -s out/ls_sv.sav.sim ] || fail "no .sim file"
cp out/ls_sv.sav out/ls_sc.sav; cp out/ls_sv.sav.sim out/ls_sc.sav.sim
BUGSTAR_LS_PAUSE=0 BUGSTAR_SIM_DUMP=out/ls_sc.dump BUGSTAR_SIM_DUMP_EVERY=100 ./bugstar_pc --script tests/sim_ls_cont.txt --out out/ls_sc --sav out/ls_sc.sav --frames 6000 > out/ls_sc.log
l=$(last sc); has "$(f "$l" names)" meet && has "$(f "$l" names)" jobs && has "$(f "$l" names)" colony || fail "after CONTINUE the flags are gone: $l"
echo "sim_ls: OK (new game: clock + weather only, TIPS the only row, card pictures differ; talk story MEET -> JOBS; late game: 9 rows, one summary card, ! cleared on open; SHAPES ???, ALL, TIPS OFF, FEW, MANY, AUTO-PAUSE OFF, BIG; flags kept after CONTINUE; two runs SAME)"
