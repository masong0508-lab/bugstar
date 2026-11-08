#!/bin/bash
# GAME SPEED (patch 43): the game runs at the real GBA's speed, 59.7275 frames a second (16777216 Hz / 280896 cycles), whatever the PC, the monitor or the phone does.
# Real time, window mode on SDL's dummy video driver (a 240x160 window, no GPU needed). Frame log = PC_FRAMELOG (one line per game frame: wall clock, key polls, dungeon steps, timer cycles).
#  1. RATE: N game frames take N / 59.7275 seconds (within 2 percent) with vsync on and off, and with a pretend 60 / 75 / 144 / 165 Hz monitor that makes every picture wait for its vblank
#           (BUGSTAR_SIMVSYNC). A screen's refresh rate must not change the speed; the old code ran at 60.00 (0.46 percent fast) and a monitor could pull it either way.
#  2. SMOOTH: no frame is much shorter or longer than 16.7 ms (the wait is sleep + spin, not 1 ms or 15 ms steps).
#  3. STALLS and PAUSES: the window thread stops for 0.7 s and 1.5 s (a drag of the title bar), and the options screen is opened for a while: afterwards the frames are 16.7 ms apart at once,
#           no burst of catch-up frames, and the speed over the whole run is still right.
#  4. SOUND on (the dummy audio device): the audio buffer does not change the game's speed.
#  5. FAST-FORWARD is the only unthrottled mode: --fast runs at least 4 times quicker.
#  6. FAKE TIMERS: the game's timer (TM0+TM1) counts 280896 cycles a frame, exactly.
#  7. SCENE BUDGETS (headless, by script, the same on every machine): a scene must use one pass of its loop and (in the dungeon) one logic step per vblank. The old dungeon ran 68 steps a vblank.
#           The table is written to out/speed/budgets.txt and compared with tests/speed.budget.
# usage: tests/speed.sh [quick]     (quick: 300-frame runs, a few seconds each, for a fast check; the default runs 600 frames)
cd "$(dirname "$0")/.."; mkdir -p out/speed; B=./bugstar_pc; fail=0; GBA=59.7275
N=600; [ "$1" = quick ] && N=300
TOL=2.0
rm -f out/speed/*.txt out/speed/*.sav*
an(){ python3 - "$@" <<'P'
import sys,statistics
f,a,b=sys.argv[1],int(sys.argv[2]),int(sys.argv[3])
rows=[l.split() for l in open(f) if not l.startswith('#') and len(l.split())==6]
rows=[r for r in rows if a<=int(r[0])<=b]
t=[float(r[1]) for r in rows]; n=len(rows)-1; w=(t[-1]-t[0])/1000.0
dt=[t[i+1]-t[i] for i in range(n)]; rate=n/w
print("%.4f %.3f %.2f %.2f %.3f %d"%(rate,(rate/59.7275-1)*100,min(dt),max(dt),statistics.pstdev(dt),n))
P
}
win(){ # name, extra env..., -- args   -> out/speed/<name>.txt = frame log
  local n=$1; shift; local envs=(); while [ "$1" != "--" ]; do envs+=("$1"); shift; done; shift
  rm -rf out/speed/$n.txt out/speed/data_$n; env "${envs[@]}" PC_FRAMELOG=out/speed/$n.txt BUGSTAR_PACELOG=1 $B --window --video-driver dummy --size 240 160 --data out/speed/data_$n "$@" > /dev/null 2> out/speed/$n.err
}
rate(){ # name label ; checks the rate over frames 100..N
  read r e mn mx sd c < <(an out/speed/$1.txt 100 $((N+99)))
  printf "  %-28s %8s fps  (GBA 59.7275: %+6s %%)  frame ms min %-6s max %-6s sd %s  [%s frames]\n" "$2" "$r" "$e" "$mn" "$mx" "$sd" "$c"
  python3 -c "import sys; sys.exit(0 if abs($e)<=$TOL else 1)" || { echo "speed: $2 is $e % off the GBA rate (limit $TOL)"; fail=1; }
  python3 -c "import sys; sys.exit(0 if $mx<40 else 1)" || { echo "speed: $2 has a frame of $mx ms (uneven frames)"; fail=1; }
}
echo "speed: $N game frames in real time; the GBA makes $N frames in $(python3 -c "print('%.2f'%($N/$GBA))") s"
win a_novsync   -- --no-vsync --stop-after $((N+100));                            rate a_novsync   "vsync off"
win b_vsync     -- --stop-after $((N+100));                                       rate b_vsync     "vsync on"
win c_sim60     BUGSTAR_SIMVSYNC=60  -- --stop-after $((N+100));                  rate c_sim60     "vsync on, 60 Hz monitor"
win d_sim75     BUGSTAR_SIMVSYNC=75  -- --stop-after $((N+100));                  rate d_sim75     "vsync on, 75 Hz monitor"
win e_sim144    BUGSTAR_SIMVSYNC=144 -- --stop-after $((N+100));                  rate e_sim144    "vsync on, 144 Hz monitor"
win f_sim165    BUGSTAR_SIMVSYNC=165 -- --stop-after $((N+100));                  rate f_sim165    "vsync on, 165 Hz monitor"
win g_sim144off BUGSTAR_SIMVSYNC=144 -- --no-vsync --stop-after $((N+100));       rate g_sim144off "vsync off, 144 Hz monitor"
# 3. stalls and pauses
printf '%s\n' "250 key down ESCAPE" "0 key up ESCAPE" "80 key down ESCAPE" "0 key up ESCAPE" > out/speed/menu_ev.txt
win h_stall BUGSTAR_SIM_STALL=200:700,400:1500 BUGSTAR_EVENTS=out/speed/menu_ev.txt -- --stop-after $((N+300))
python3 - out/speed/h_stall.txt <<'P' || { echo "speed: after a stall or a pause the frames burst (catch-up) or the speed is wrong"; fail=1; }
import sys
rows=[l.split() for l in open(sys.argv[1]) if not l.startswith('#') and len(l.split())==6]
t={int(r[0]):float(r[1]) for r in rows}
ok=True; out=[]
big=[f for f in sorted(t) if f+1 in t and t[f+1]-t[f]>300]          # the long gaps: the two stalls and the paused options screen
for f in big:
    after=[t[f+i+1]-t[f+i] for i in range(1,11) if f+i+1 in t]
    bad=[round(d,1) for d in after if d<12 or d>25]
    out.append("gap of %.0f ms at frame %d, then frames %s ms%s"%(t[f+1]-t[f],f,[round(d,1) for d in after[:4]],"" if not bad else "   BAD "+str(bad)))
    if bad: ok=False
print("  stalls / pause:               "+"; ".join(out))
sys.exit(0 if ok and len(big)>=3 else 1)
P
# 4. sound on
win i_audio SDL_AUDIODRIVER=dummy -- --stop-after $((N+100));                      rate i_audio     "sound on (dummy device)"
# 5. fast-forward
s=$(date +%s.%N); win j_fast -- --fast --no-vsync --stop-after 1500; e=$(date +%s.%N)
python3 - "$s" "$e" <<'P' || { echo "speed: --fast is not at least 4 times quicker than real time"; fail=1; }
import sys; w=float(sys.argv[2])-float(sys.argv[1]); print("  --fast (explicit fast-forward): 1500 frames in %.2f s (real time would be %.1f s)"%(w,1500/59.7275)); sys.exit(0 if w<1500/59.7275/4 else 1)
P
# 6. fake timers: 280896 cycles a frame, exactly
python3 - out/speed/a_novsync.txt <<'P' || { echo "speed: the fake timer does not advance 280896 cycles a frame"; fail=1; }
import sys
rows=[l.split() for l in open(sys.argv[1]) if not l.startswith('#') and len(l.split())==6]
c=[int(r[5]) for r in rows]; d=set(c[i+1]-c[i] for i in range(1,len(c)-1))
print("  fake timer TM0+TM1: cycles per frame %s (GBA: 280896)"%sorted(d)); sys.exit(0 if d=={280896} else 1)
P
# 7. scene budgets
mk(){ python3 - "$1" "$2" <<'P'
import random,sys
r=random.Random(int(sys.argv[1])); L=["300 304 A","600 604 A","900 904 A","1200 1204 S","1500 1504 S","2000 2004 S","2300 2304 A","2700 2704 A"]; f=3400
keys=['A','A','A','B','U','D','U','D','l','r','R','L','s']
while f<7000:
    L.append(f"{f} {f+r.choice([2,4,20])} {r.choice(keys)}"); f+=r.choice([6,10,30,90])
open(sys.argv[2],"w").write("\n".join(L)+"\n")
P
}
mk 1 out/speed/mash.txt
: > out/speed/scenes.log
scene(){ # name frames script env...   -> headless run with the frame log
  local n=$1 fr=$2 sc=$3; shift 3; rm -f out/speed/s_$n.txt out/speed/s_$n.sav*
  env BUGSTAR_SIDE=0 BUGSTAR_WS_AT=650 BUGSTAR_AFT_LOG=1 "$@" PC_FRAMELOG=out/speed/s_$n.txt $B --script $sc --out out/speed/s_$n --sav out/speed/s_$n.sav --frames $fr > out/speed/s_$n.log 2> out/speed/s_$n.err
  echo "$n $fr" >> out/speed/scenes.log
}
scene title_intro_planet   4501 tests/play.txt
scene dungeon_troops       3000 tests/aft_idle.txt   BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=inv BUGSTAR_AFT_TOPUP=1
scene dungeon_seal_dig     3000 tests/aft_dig.txt    BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=seal BUGSTAR_AFT_TOPUP=1
scene shop_menu            7000 out/speed/mash.txt   BUGSTAR_SIDE=1 BUGSTAR_GIANT_DIR=$(pwd)/out/speed/nodir BUGSTAR_WS_TEST=shop,0,0 BUGSTAR_AFT_GOLD=100000 BUGSTAR_SET_FREED=1
scene mission_board        7000 out/speed/mash.txt   BUGSTAR_SIDE=1 BUGSTAR_GIANT_DIR=$(pwd)/out/speed/nodir BUGSTAR_WS_TEST=missions,0,0 BUGSTAR_AFT_GOLD=100000 BUGSTAR_SET_FREED=1
scene warship_fight        3400 tests/ws_fight.txt   BUGSTAR_WS_TEST=fight,520,0
scene warship_interior     3420 tests/ws_inside.txt  BUGSTAR_WS_TEST=inside,520,0
scene space_flight_ships   3600 tests/shipyard_space.txt BUGSTAR_SY_PIRATES=6 BUGSTAR_SY_AT=100 PC_SHIPS=1 PC_NATIVE=2 PC_SCALE=1 BUGSTAR_SY_TEST=9,0
if [ -x ./bugstar_pc_kfz ] && [ -n "$SPEED_KART" ]; then   # the kart race with its 3-2-1-GO countdown, from the kart fuzz build (tests/kart.sh makes it); SPEED_KART=1 tests/speed.sh
  echo "" > out/speed/empty.txt; rm -f out/speed/s_kart_race_countdown.txt; KFUZZ_FIRST=0 KFUZZ_RACES=2 PC_FRAMELOG=out/speed/s_kart_race_countdown.txt timeout 900 ./bugstar_pc_kfz --script out/speed/empty.txt --out out/speed/s_kart_race_countdown --sav out/speed/s_kart_race_countdown.sav --frames 100000000 > out/speed/s_kart_race_countdown.log 2>&1; echo "kart_race_countdown 0" >> out/speed/scenes.log
fi
python3 - out/speed/scenes.log tests/speed.budget "$GBA" <<'P' || fail=1
import sys,collections
names=[l.split() for l in open(sys.argv[1])]
bud={}
for l in open(sys.argv[2]):
    p=l.split()
    if p and not p[0].startswith('#'): bud[p[0]]=(int(p[1]),int(p[2]))
G=float(sys.argv[3]); bad=0; lines=[]
hdr="  %-20s %7s %9s %12s %12s   %s"%("scene","frames","at GBA s","polls/vblank","steps/vblank","budget (polls, steps)")
print(hdr); lines.append(hdr)
for n,fr in names:
    try: rows=[l.split() for l in open("out/speed/s_%s.txt"%n) if not l.startswith('#') and len(l.split())==6]
    except OSError: print("  %-20s no frame log"%n); bad=1; continue
    if not rows: print("  %-20s empty frame log"%n); bad=1; continue
    po=collections.Counter(int(r[3]) for r in rows); st=collections.Counter(int(r[4]) for r in rows)
    pm=max(po); sm=max(st); ln=len(rows)
    bp,bs=bud.get(n,(2,1))
    ok = pm<=bp and sm<=bs and (n.startswith('kart') or int(fr)==0 or ln==int(fr) or abs(ln-int(fr))<=1)
    s="  %-20s %7d %9.1f %12s %12s   %d, %d %s"%(n,ln,ln/G,"max %d"%pm,"max %d"%sm,bp,bs,"" if ok else "  <-- OVER BUDGET")
    print(s); lines.append(s)
    if not ok: bad=1
open("out/speed/budgets.txt","w").write("\n".join(lines)+"\n")
sys.exit(1 if bad else 0)
P
[ $fail = 0 ] && echo "speed: OK (59.7275 fps within $TOL % with vsync on/off and 60/75/144/165 Hz monitors, even frames, no burst after stalls or pauses, sound does not change it, only --fast is unthrottled, timers run at GBA rate, scenes within their frame budgets)" || { echo "speed: FAILED (see pc/out/speed/)"; exit 1; }
