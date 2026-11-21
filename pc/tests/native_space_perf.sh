#!/usr/bin/env bash
# Compare native space rendering with and without the column/shading caches.
# Aim the actual game's 3D camera toward a nearby planet (yaw 896), so
# the real large-sphere surface shader is exercised rather than just the stars.
# No speed threshold: timing on shared CI workers is noisy; image equality
# and at least one real 120-frame timing report are the hard correctness requirements.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
mkdir -p out/native_space
run(){
    local name="$1" columns="$2" shading="$3" threads="$4"
    rm -f "out/native_space/${name}.sav" out/native_space/"${name}"_*.ppm
    env PC_NATIVE=2 PC_SCALE=4 PC_V3D=1 PC_SHIPS=1 PC_THREADS="$threads" PC_SN_BENCH=1 \
        BUGSTAR_SN_XCACHE="$columns" BUGSTAR_SN_SHADECACHE="$shading" BUGSTAR_SIDE=0 \
        BUGSTAR_VT_AT=100 BUGSTAR_VT=space,0,896,0,0 \
        timeout -k 3 240 "$GAME" --headless --script tests/vt_idle.txt \
            --out "out/native_space/${name}" --sav "out/native_space/${name}.sav" \
            --frames 3900 > "out/native_space/${name}.log" 2>&1
    if ! grep -q 'space-native bench 960x640:' "out/native_space/${name}.log"; then
        echo "native space: ${name} did not exercise the native 3D renderer" >&2
        tail -35 "out/native_space/${name}.log" >&2
        exit 1
    fi
    test -s "out/native_space/${name}_003140.ppm"
    grep 'space-native bench 960x640:' "out/native_space/${name}.log" | tail -1
}
run reference 0 0 4
run baseline  1 0 4
run optimized 1 1 4
run single    1 1 1
cmp out/native_space/reference_003140.ppm out/native_space/baseline_003140.ppm
cmp out/native_space/baseline_003140.ppm out/native_space/optimized_003140.ppm
cmp out/native_space/optimized_003140.ppm out/native_space/single_003140.ppm
python3 - <<'PY'
from pathlib import Path
import re,statistics
for name in ("reference","baseline","optimized","single"):
    log=(Path("out/native_space")/(name+".log")).read_text()
    times=[float(m) for m in re.findall(r"space-native bench 960x640: avg wall ([0-9.]+) ms",log)]
    assert times, f"{name} did not render any measured space frames"
    tables=[int(m) for m in re.findall(r"space-native bench 960x640:.*shade tables ([0-9]+)",log)]
    assert tables, f"{name}: missing actual shade-table coverage measurements"
    if name in ("optimized","single"):
        assert sum(tables)>0, f"{name}: the test never exercised a cached planet shader"
    else:
        assert sum(tables)==0, f"{name}: cache-disabled reference unexpectedly prepared shading"
    # Report the median of completed 120-frame windows, not a single fastest frame.
    print(f"{name}: {statistics.median(times):.3f} ms median of {len(times)} x 120-frame windows; {sum(tables)} shade tables")
print("native space: cached/uncached, shaded/unshaded and one/four-thread pictures MATCH at 960x640")
PY
