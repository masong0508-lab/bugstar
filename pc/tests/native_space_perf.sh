#!/usr/bin/env bash
# Compare native space rendering with and without column caching. Both use
# the actual game 3D view (BUGSTAR_VT=space), not a synthetic bitmap.
# No speed threshold: timing on shared CI workers is noisy; image equality
# and a real 120-frame timing report are the hard correctness requirements.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
mkdir -p out/native_space
run(){
    local name="$1" cached="$2" threads="$3"
    rm -f "out/native_space/${name}.sav" out/native_space/"${name}"_*.ppm
    env PC_NATIVE=2 PC_SCALE=4 PC_V3D=1 PC_THREADS="$threads" PC_SN_BENCH=1 \
        BUGSTAR_SN_XCACHE="$cached" BUGSTAR_SIDE=0 \
        BUGSTAR_VT_AT=100 BUGSTAR_VT=space,1,150,0,0 \
        timeout -k 3 240 "$GAME" --headless --script tests/vt_idle.txt \
            --out "out/native_space/${name}" --sav "out/native_space/${name}.sav" \
            --frames 3150 > "out/native_space/${name}.log" 2>&1
    if ! grep -q 'space-native bench 960x640:' "out/native_space/${name}.log"; then
        echo "native space: ${name} did not exercise the native 3D renderer" >&2
        tail -35 "out/native_space/${name}.log" >&2
        exit 1
    fi
    test -s "out/native_space/${name}_003140.ppm"
    grep 'space-native bench 960x640:' "out/native_space/${name}.log" | tail -1
}
run reference 0 4
run optimized 1 4
run single 1 1
cmp out/native_space/reference_003140.ppm out/native_space/optimized_003140.ppm
cmp out/native_space/optimized_003140.ppm out/native_space/single_003140.ppm
python3 - <<'PY'
from pathlib import Path
import re,statistics
for name in ("reference","optimized","single"):
    log=(Path("out/native_space")/(name+".log")).read_text()
    times=[float(m) for m in re.findall(r"space-native bench 960x640: avg wall ([0-9.]+) ms",log)]
    assert times, f"{name} did not render any measured space frames"
    # Report the median of completed 120-frame windows, not a single fastest frame.
    print(f"{name}: {statistics.median(times):.3f} ms median of {len(times)} x 120-frame windows")
print("native space: cached/uncached and one/four-thread pictures MATCH at 960x640")
PY
