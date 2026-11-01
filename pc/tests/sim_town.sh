#!/bin/bash
# The side game in the real game: stand in the first town for 40,000 frames (about 1.4 game days), dump the townfolk every 600 ticks.
# Checks: 3 or more named townfolk; one sleeps AT its home at night and one works AT its work by day; the log has 10 or more lines;
# every need is in 0..255; two runs give the same dump text. (The "near" lines are tenths of a cell from where the bug should be.)
cd "$(dirname "$0")/.."; mkdir -p out; rm -f out/st1.* out/st2.*
for r in 1 2; do BUGSTAR_SIM_DUMP=out/st$r.dump ./bugstar_pc --script tests/sim_town.txt --out out/st$r --sav out/st$r.sav --frames 40000 > out/st$r.log; done
cmp -s out/st1.dump out/st2.dump || { echo "sim_town: DIFFERENT between two runs"; diff out/st1.dump out/st2.dump | head; exit 1; }
cmp -s out/st1.sav.sim out/st2.sav.sim || { echo "sim_town: the .sim files differ between two runs"; exit 1; }
awk '
/^sim /   { logn=$NF; hour=$7; n++ ; for(i=1;i<=NF;i++) if($i=="log") logn=$(i+1); k=0 }
/^folk /  { name[$2]=$3; task[$2]=$7; for(i=1;i<=NF;i++) if($i=="hunger"||$i=="rest"||$i=="social"||$i=="fun"){ v=$(i+1); if(v<0||v>255) bad++ }; nf=($2+1>nf)?$2+1:nf }
/^near /  { d=$3; if(task[$2]=="SLEEPING" && (hour>=22||hour<5) && d<=30) home=1; if(task[$2]=="WORKING" && hour>=9 && hour<=16 && d<=30) work=1 }
END { fail=0
  if(nf<3){ print "sim_town: fewer than 3 townfolk (" nf ")"; fail=1 }
  if(!home){ print "sim_town: nobody slept at home at night"; fail=1 }
  if(!work){ print "sim_town: nobody worked at work by day"; fail=1 }
  if(logn<10){ print "sim_town: log has only " logn " lines"; fail=1 }
  if(bad){ print "sim_town: a need was outside 0..255"; fail=1 }
  if(n<8){ print "sim_town: only " n " dumps"; fail=1 }
  if(fail) exit 1
  print "sim_town: OK (" nf " townfolk, " n " dumps, log " logn ", sleep at home + work at work seen, two runs SAME)" }' out/st1.dump
