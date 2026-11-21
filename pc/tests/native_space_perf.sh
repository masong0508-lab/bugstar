#!/usr/bin/env bash
# Compare the game's real 3D output with and without the column/shading caches
# and the sparse native-effects compositor/clear. Validate full pixel equality.
# Aim the actual game's 3D camera toward a nearby planet (yaw 896), so
# the real large-sphere surface shader is exercised rather than just the stars.
# No speed threshold: timing on shared CI workers is noisy; image equality
# and at least one real 120-frame timing report are the hard correctness requirements.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
mkdir -p out/native_space
run(){
    local name="$1" columns="$2" shading="$3" threads="$4" dirty="$5" legacy="$6" scale="$7"
    local width=$((240*scale)) height=$((160*scale))
    rm -f "out/native_space/${name}.sav" out/native_space/"${name}"_*.ppm
    env PC_NATIVE=2 PC_SCALE="$scale" PC_V3D=1 PC_SHIPS=1 PC_THREADS="$threads" PC_SN_BENCH=1 \
        PC_SN_DIRTY="$dirty" PC_SN_DIRTY_LOG=1 PC_NBENCH=12 \
        PC_PAR_LEGACY="$legacy" PC_PAR_STATS=1 \
        BUGSTAR_SN_XCACHE="$columns" BUGSTAR_SN_SHADECACHE="$shading" BUGSTAR_SIDE=0 \
        BUGSTAR_VT_AT=100 BUGSTAR_VT=space,0,896,0,0 \
        timeout -k 3 240 "$GAME" --headless --script tests/vt_idle.txt \
            --out "out/native_space/${name}" --sav "out/native_space/${name}.sav" \
            --frames 3900 > "out/native_space/${name}.log" 2>&1
    if ! grep -q "space-native bench ${width}x${height}:" "out/native_space/${name}.log"; then
        echo "native space: ${name} did not exercise the native 3D renderer" >&2
        tail -35 "out/native_space/${name}.log" >&2
        exit 1
    fi
    test -s "out/native_space/${name}_003140.ppm"
    grep "space-native bench ${width}x${height}:" "out/native_space/${name}.log" | tail -1
    grep -m1 'space effects .* scan ' "out/native_space/${name}.log"
}
run reference 0 0 4 0 0 4
run baseline  1 0 4 0 0 4
run full      1 1 4 0 0 4
run optimized 1 1 4 1 0 4
run single    1 1 1 1 0 4
# At 2x GBA resolution the 256-row space surface compositing job has 16
# chunks of 16 rows; 12 available threads must use only seven workers.
# At 4x that same pass has 32 chunks and needs the whole pool.
run legacy   1 1 12 1 1 2
run adaptive 1 1 12 1 0 2
cmp out/native_space/reference_003140.ppm out/native_space/baseline_003140.ppm
cmp out/native_space/baseline_003140.ppm out/native_space/full_003140.ppm
cmp out/native_space/full_003140.ppm out/native_space/optimized_003140.ppm
cmp out/native_space/optimized_003140.ppm out/native_space/single_003140.ppm
cmp out/native_space/legacy_003140.ppm out/native_space/adaptive_003140.ppm
python3 - <<'PY'
from pathlib import Path
import re,statistics
for name in ("reference","baseline","full","optimized","single","legacy","adaptive"):
    log=(Path("out/native_space")/(name+".log")).read_text()
    size="480x320" if name in ("legacy","adaptive") else "960x640"
    times=[float(m) for m in re.findall(rf"space-native bench {size}: avg wall ([0-9.]+) ms",log)]
    assert times, f"{name} did not render any measured space frames"
    tables=[int(m) for m in re.findall(rf"space-native bench {size}:.*shade tables ([0-9]+)",log)]
    assert tables, f"{name}: missing actual shade-table coverage measurements"
    if name in ("full","optimized","single"):
        assert sum(tables)>0, f"{name}: the test never exercised a cached planet shader"
    # The smaller camera scene need not meet the surface-cache area threshold;
    # its purpose is to exercise genuinely small row-scheduling jobs.
    else:
        assert sum(tables)==0, f"{name}: cache-disabled reference unexpectedly prepared shading"
    areas=[(mode,int(a),int(b)) for mode,a,b in re.findall(r"space effects (clipped|full) scan ([0-9]+)/([0-9]+) pixels",log)]
    assert areas, f"{name}: the snapshot test never ran the effects compositor"
    if name in ("optimized","single","legacy","adaptive"):
        assert any(mode=="clipped" and a<b for mode,a,b in areas), f"{name}: the effects scan wasn't narrowed"
    else:
        assert all(mode=="full" and a==b for mode,a,b in areas), f"{name}: diagnostic full effects scan missing"
    compose=[float(v) for v in re.findall(rf"native compose {size}: wall best [0-9.]+ avg ([0-9.]+) ms",log)]
    assert compose, f"{name}: missing full-compositor samples"
    # Report the median of completed 120-frame windows, not a single fastest frame.
    print(f"{name}: native space {statistics.median(times):.3f} ms; compositor {statistics.median(compose):.3f} ms; effects scan {areas[0][1]}/{areas[0][2]} pixels")
# A/B schedule coverage: original path wakes all workers even for a tiny
# scan, while the adaptive path uses fewer on at least one parallel job.
def rows(name):
    txt=(Path("out/native_space")/(name+".log")).read_text()
    return [(int(a),int(b),int(c),int(d),int(e),int(f)) for a,b,c,d,e,f in
        re.findall(r"row workers: pool=([0-9]+) active=([0-9]+) jobs=([0-9]+) saved wakes=([0-9]+) full=([0-9]+) limited=([0-9]+)",txt)]
old,new=rows("legacy"),rows("adaptive")
assert old and new, "worker pool did not report scheduling evidence"
assert any(row[0]==11 and row[4]>0 for row in old), "legacy never dispatched the full worker pool"
assert any(row[0]==11 and row[5]>0 for row in new), "adaptive never restricted a multi-worker job"
assert new[-1][3]>old[-1][3], "adaptive scheduling did not reduce wakeups"
print("worker schedule: full-pool legacy and limited-worker paths exercised; last saved wakes:",old[-1][3],new[-1][3],
      "adaptive limited jobs:",new[-1][5])
print("native space: cached/shaded/dirty and 1/4-thread pictures MATCH at 960x640; 12-thread legacy/adaptive pictures MATCH at 480x320")
PY
