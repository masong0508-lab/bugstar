#!/bin/bash
# Windows smoke test: does the packaged BUGSTAR.exe start, make its data/ files and keep running?
#   pc/tests/win_smoke.sh [seconds]            default 8 seconds
#   pc/tests/win_smoke.sh --strict [seconds]   a missing wine is a failure (exit 3) instead of a skip
# 1. Package checks (always): pc/dist/BUGSTAR-PC-windows32.zip holds BUGSTAR.exe (a 32-bit Windows PE file), SDL2.dll, portable.txt,
#    README.txt and data/readme.txt, and no saves.
# 2. Run (only if wine is installed): unpack the zip in a scratch folder, run BUGSTAR.exe under wine with SDL's dummy video and audio
#    drivers (no display needed), for N seconds. Pass = still running when the time is up (timeout's exit 124), data/ now holds
#    bugstar.sav and settings.ini, and wine printed no unhandled exception / page fault.
# The Windows exe is cross-compiled; this is the only automatic run of it. Where wine cannot be installed (no apt or registry access)
# the test says SKIP and exits 0, so it never makes a machine without wine look broken.
cd "$(dirname "$0")/.."
STRICT=0; [ "$1" = "--strict" ] && { STRICT=1; shift; }
SECS=${1:-8}
ZIP=dist/BUGSTAR-PC-windows32.zip; D=BUGSTAR-PC-windows32
fail(){ echo "win_smoke: FAIL: $*"; exit 1; }
[ -f $ZIP ] || fail "$ZIP is missing (build it with pc/make_dist.sh)"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
unzip -q $ZIP -d $T || fail "cannot unzip $ZIP"
for f in BUGSTAR.exe SDL2.dll portable.txt README.txt data/readme.txt; do [ -f $T/$D/$f ] || fail "$f is not in the zip"; done
[ "$(ls $T/$D/data | wc -l)" = 1 ] || fail "data/ in the zip holds more than readme.txt (a save was packed): $(ls $T/$D/data)"
head -c 2 $T/$D/BUGSTAR.exe | grep -q MZ || fail "BUGSTAR.exe is not a Windows program (no MZ header)"
pe=$(python3 -I - "$T/$D/BUGSTAR.exe" <<'EOF'
import struct, sys
d = open(sys.argv[1], 'rb').read(); o = struct.unpack_from('<I', d, 0x3C)[0]
assert d[o:o+4] == b'PE\0\0'
m = struct.unpack_from('<H', d, o+4)[0]; print({0x14c: 'i386', 0x8664: 'x86-64', 0xaa64: 'arm64'}.get(m, hex(m)))
EOF
)
[ "$pe" = i386 ] || fail "BUGSTAR.exe is $pe, the package says 32-bit (windows32)"
echo "win_smoke: package OK (32-bit PE exe, SDL2.dll, portable.txt, README.txt, clean data/)"

if ! command -v wine >/dev/null 2>&1; then
  [ $STRICT = 1 ] && { echo "win_smoke: wine is not installed (--strict)"; exit 3; }
  echo "win_smoke: SKIP the run: wine is not installed (apt install wine, or wine from winehq.org). Package checks passed."; exit 0
fi
export WINEPREFIX=$T/prefix WINEDEBUG=err+all WINEDLLOVERRIDES="mscoree,mshtml=" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
wine wineboot -i >/dev/null 2>&1 || true
( cd $T/$D && timeout $SECS wine BUGSTAR.exe > $T/run.log 2>&1; echo $? > $T/exit )
rc=$(cat $T/exit)
[ "$rc" = 124 ] || { tail -15 $T/run.log; fail "BUGSTAR.exe ended by itself after less than $SECS s (exit $rc)"; }
grep -q -i -E "unhandled exception|page fault|stack overflow|fatal" $T/run.log && { grep -i -E "unhandled exception|page fault|stack overflow|fatal" $T/run.log | head -5; fail "wine reports a crash"; }
for f in bugstar.sav settings.ini; do [ -s $T/$D/data/$f ] || fail "data/$f was not made in $SECS s"; done
echo "win_smoke: OK (BUGSTAR.exe ran $SECS s under wine, still running, data/ has $(ls $T/$D/data | tr '\n' ' '))"
