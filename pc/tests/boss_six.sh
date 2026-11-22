#!/usr/bin/env bash
# Film actual six story bosses, before and after the half-HP guard phase,
# from the real PC game loop. PC test hook only; GBA/Android not modified.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME=./bugstar_pc
if [ "$#" -gt 0 ]; then GAME="$1"; fi
OUT=out/boss_six
mkdir -p "$OUT"
for zone in 0 1 2 3 4 5; do
  for phase in 0 1; do
    name="$zone"_"$phase"
    rm -f "$OUT/$name".sav*
    timeout -k 5 120 env BUGSTAR_LAN_BOOT=1 BUGSTAR_SIDE=0 \
      BUGSTAR_BOSS_CAPTURE="$zone:$phase" \
      "$GAME" --headless --script tests/boss_capture.txt \
      --out "$OUT/$name" --sav "$OUT/$name.sav" --frames 440 \
      > "$OUT/$name.log" 2> "$OUT/$name.err"
    grep -q "^boss-capture: zone=$zone phase=$phase " "$OUT/$name.err" || {
      echo "boss-capture $name: not in the requested boss arena" >&2
      tail -30 "$OUT/$name.err" >&2
      exit 1
    }
    for frame in 000240 000302; do
      test -s "$OUT/$name"_"$frame".ppm || {
        echo "boss-capture $name: no real gameplay screenshot for frame $frame" >&2
        exit 1
      }
    done
    echo "boss-capture $name: $(grep '^boss-capture:' "$OUT/$name.err" | tail -1)"
  done
done
python3 tests/boss_six_review.py "$OUT"
