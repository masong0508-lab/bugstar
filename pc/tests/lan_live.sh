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
# The normal shipyard script taps A only a few times: both games can remain
# blocked in Bolt's first dialogue (verified from real CI screenshots).
# BUGSTAR_LAN_BOOT skips only that opening story during this automated test.
# Send plenty of separated confirmation presses, then stop inputs well
# before taking the two actual 3D space screenshots.
: > out/lan_live/boot.txt
for ((f=24; f<800; f+=20)); do
    printf '%d %d A\n' "$f" "$((f+2))" >> out/lan_live/boot.txt
done
host_pid=''
cleanup(){ if [[ -n "$host_pid" ]]; then kill "$host_pid" 2>/dev/null || :; fi; }
trap cleanup EXIT
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_TRACE=1 BUGSTAR_LAN_BOOT=1 \
    BUGSTAR_SY_TEST=9,0 BUGSTAR_SY_AT=100 \
    timeout -k 3 95 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 240 160 --lan-host --lan-port 33777 \
    --data out/lan_live/host --script out/lan_live/boot.txt \
    --auto-shot 3550 out/lan_live/host.png --stop-after 3600 \
    > out/lan_live/host.log 2>&1 &
host_pid=$!
sleep 0.3
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_TRACE=1 BUGSTAR_LAN_BOOT=1 \
    BUGSTAR_SY_TEST=9,0 BUGSTAR_SY_AT=100 \
    timeout -k 3 95 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 240 160 --lan-join 127.0.0.1 --lan-port 33777 \
    --data out/lan_live/client --script out/lan_live/boot.txt \
    --auto-shot 3550 out/lan_live/client.png --stop-after 3600 \
    > out/lan_live/client.log 2>&1 || {
      echo 'LAN smoke: client game failed'; tail -50 out/lan_live/client.log; exit 1; }
wait "$host_pid" || {
    echo 'LAN smoke: host game failed'; tail -50 out/lan_live/host.log; exit 1; }
host_pid=''
for side in host client; do
    grep -Eq 'lan: status frame .* space 1 world' "out/lan_live/${side}.log" || {
        echo "LAN smoke: ${side} never reached live 3D space flight"
        tail -70 "out/lan_live/${side}.log"; exit 1; }
    grep -q 'lan: peer CONNECTED frame' "out/lan_live/${side}.log" || {
        echo "LAN smoke: ${side} never connected inside the actual game"
        tail -70 "out/lan_live/${side}.log"; exit 1; }
    test -s "out/lan_live/${side}.png" || {
        echo "LAN smoke: ${side} did not render its screenshot";exit 1; }
done
if cmp -s out/lan_live/host.png out/lan_live/client.png; then
    echo 'LAN smoke: both camera screenshots are identical (wrong scene or failed P1/P2 view)'; exit 1
fi
echo "LAN smoke: two independent full PC games connected, finished and rendered screenshots PASS"
