#!/usr/bin/env bash
# Two complete BUGSTAR PC games in the same star, in separate save folders.
# Unlike the tiny socket unit test, this exercises the *real* C game, SDL
# input/pacing, space 3D draw hook, LAN transport and two on-screen viewpoints.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
mkdir -p out/lan_live/host out/lan_live/client
printf 'pause_unfocus=0\nvsync=0\n' > out/lan_live/host/settings.ini
printf 'pause_unfocus=0\nvsync=0\n' > out/lan_live/client/settings.ini
rm -f out/lan_live/{host,client}.log out/lan_live/{host,client}.png
host_pid=''
cleanup(){ if [[ -n "$host_pid" ]]; then kill "$host_pid" 2>/dev/null || :; fi; }
trap cleanup EXIT
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_TRACE=1 \
    BUGSTAR_SY_TEST=9,0 BUGSTAR_SY_AT=100 \
    timeout -k 3 95 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 240 160 --lan-host --lan-port 33777 \
    --data out/lan_live/host --script tests/shipyard_space.txt \
    --auto-shot 3550 out/lan_live/host.png --stop-after 3600 \
    > out/lan_live/host.log 2>&1 &
host_pid=$!
sleep 0.3
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_TRACE=1 \
    BUGSTAR_SY_TEST=9,0 BUGSTAR_SY_AT=100 \
    timeout -k 3 95 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 240 160 --lan-join 127.0.0.1 --lan-port 33777 \
    --data out/lan_live/client --script tests/shipyard_space.txt \
    --auto-shot 3550 out/lan_live/client.png --stop-after 3600 \
    > out/lan_live/client.log 2>&1 || {
      echo 'LAN smoke: client game failed'; tail -50 out/lan_live/client.log; exit 1; }
wait "$host_pid" || {
    echo 'LAN smoke: host game failed'; tail -50 out/lan_live/host.log; exit 1; }
host_pid=''
for side in host client; do
    grep -q 'lan: peer CONNECTED frame' "out/lan_live/${side}.log" || {
        echo "LAN smoke: ${side} never connected inside the actual game"
        tail -70 "out/lan_live/${side}.log"; exit 1; }
    test -s "out/lan_live/${side}.png" || {
        echo "LAN smoke: ${side} did not render its screenshot";exit 1; }
done
echo "LAN smoke: two independent full PC games connected, finished and rendered screenshots PASS"
