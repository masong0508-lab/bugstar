#!/bin/bash
# Friends and letters in the real game: BUGSTAR_SIM_NEAR=0 keeps the player beside townsperson 0; A is pressed on them once a game day for 3 days
# (each talk: A opens the friendship card, A closes it, A closes the local's own card). Checks: the card picture (12030) differs from the game after it
# (12300); friendship goes up every day; a letter arrives (friendship passed 25) and its pop-up was shown; two runs are the same.
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/sf1.* out/sf2.*
for r in 1 2; do BUGSTAR_SIM_NEAR=0 BUGSTAR_SIM_DUMP=out/sf$r.dump ./bugstar_pc --script tests/sim_friend.txt --out out/sf$r --sav out/sf$r.sav --frames 75000 > out/sf$r.log; done
cmp -s out/sf1.log out/sf2.log && cmp -s out/sf1.dump out/sf2.dump || { echo "sim_friend: DIFFERENT between two runs"; exit 1; }
h(){ grep "^shot $1 " out/sf1.log | sed 's/.*screen=//'; }
[ "$(h 12030)" != "$(h 12300)" ] || { echo "sim_friend: no card on screen"; exit 1; }
vals=$(grep "^friends" out/sf1.dump | awk '{print $2}' | uniq | tr '\n' ' ')
last=$(grep "^friends" out/sf1.dump | tail -1)
n=$(echo $vals | wc -w); letters=$(echo "$last" | awk '{for(i=1;i<=NF;i++) if($i=="letters") print $(i+1)}'); shown=$(echo "$last" | awk '{for(i=1;i<=NF;i++) if($i=="shown") print $(i+1)}')
[ "$n" -ge 3 ] || { echo "sim_friend: friendship did not rise each day ($vals)"; exit 1; }
[ "$letters" -ge 1 ] && [ "$shown" -ge 1 ] || { echo "sim_friend: no letter ($last)"; exit 1; }
grep -q "^sim " out/sf1.sav.sim 2>/dev/null; [ -s out/sf1.sav.sim ] || { echo "sim_friend: no .sim file"; exit 1; }
echo "sim_friend: OK (friendship $vals, $letters letter(s), pop-up shown $shown, card seen, two runs SAME)"
