#!/usr/bin/env bash
# A fixed-frame regression test for the PC + Android SDL timing path.
# The real engine must simulate at the GBA's 59.7275 Hz, independent of
# monitor refresh, vsync, render stalls, or hardware speed.
set -euo pipefail
cd "$(dirname "$0")/.."
B="${1:-./bugstar_pc}"
mkdir -p out/speed_guard
run() {
    local name="$1"; shift
    rm -rf "out/speed_guard/${name}.txt" "out/speed_guard/data_${name}"
    mkdir -p "out/speed_guard/data_${name}"
    # Dummy/offscreen windows are intentionally unfocused. Do not let the
    # ordinary pause-on-focus-loss option stop the test clock forever.
    printf 'pause_unfocus=0\n' > "out/speed_guard/data_${name}/settings.ini"
    env "$@" SDL_AUDIODRIVER=dummy PC_FRAMELOG="out/speed_guard/${name}.txt" \
        timeout -k 3 25 "$B" --window --video-driver dummy --no-audio \
        --size 240 160 --data "out/speed_guard/data_${name}" \
        --stop-after 275 > "out/speed_guard/${name}.log" 2>&1
}
run normal
run high_refresh BUGSTAR_SIMVSYNC=144
run recovery BUGSTAR_SIM_STALL=110:700,190:500
python3 - <<'PY'
from pathlib import Path
import statistics,sys
ok=True
for name in ('normal','high_refresh','recovery'):
    p=Path('out/speed_guard')/(name+'.txt')
    rows=[line.split() for line in p.read_text().splitlines() if not line.startswith('#')]
    frames={int(r[0]):float(r[1]) for r in rows if len(r)==6}
    if len(frames)<250:
        print(name,'FAIL: too few game frames',len(frames));ok=False;continue
    # Ignore cold startup and the two deliberate 700 / 500 ms stalls.
    pairs=[(f,frames[f+1]-t) for f,t in frames.items() if f+1 in frames]
    ordinary=[dt for f,dt in pairs if 50<=f<260 and
              not (name=='recovery' and (100<=f<=116 or 180<=f<=196))]
    median=statistics.median(ordinary)
    rate=1000.0/median
    # Overspeed must not occur even for a handful of quick catch-up frames.
    too_fast=[(f,round(dt,2)) for f,dt in pairs if f>=50 and dt<10.5]
    if abs(rate/59.7275-1)>0.03 or too_fast:
        ok=False
    if name=='recovery':
        # A stale timeline must never trigger a catch-up burst after a stall.
        for f,dt in pairs:
            if dt>300 and f>40:
                next_steps=[d for ff,d in pairs if f+1<=ff<=f+4]
                if any(d<11 for d in next_steps): ok=False;too_fast.extend([(f,'after stall')])
    print('%s: median %.3f ms (%.2f Hz), %d overspeed intervals, status %s'%
          (name,median,rate,len(too_fast),'PASS' if not too_fast and abs(rate/59.7275-1)<=0.03 else 'FAIL'))
sys.exit(0 if ok else 1)
PY
