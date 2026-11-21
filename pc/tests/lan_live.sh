#!/usr/bin/env bash
# Two complete BUGSTAR PC games in the same star, in separate save folders.
# Unlike the tiny socket unit test, this exercises the *real* C game, SDL
# input/pacing, space 3D draw hook, LAN transport and two on-screen viewpoints.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
MODE="${2:-spar}"
LAN_MODE=()
if [[ "$MODE" == coop ]]; then LAN_MODE=(--lan-coop); fi
mkdir -p out/lan_live/host out/lan_live/client
printf 'pause_unfocus=0\nvsync=0\n' > out/lan_live/host/settings.ini
printf 'pause_unfocus=0\nvsync=0\n' > out/lan_live/client/settings.ini
rm -f out/lan_live/{host,client}.log out/lan_live/{host,client}.png out/lan_live/{host,client}-clear.png
# The generated PC game takes the same normal start_game() path but
# BUGSTAR_LAN_BOOT skips title/creator menus and enters live space directly.
# Co-op uses two stationary, aim-following test pilots. Their laser presses
# still go through the REAL game cooldown, snapshots and authoritative UDP.
: > out/lan_live/boot.txt
if [[ "$MODE" == coop ]]; then
    printf '180 1700 A\n' > out/lan_live/boot.txt
else
    for ((f=180; f<900; f+=90)); do
        printf '%d %d A\n' "$f" "$((f+43))" >> out/lan_live/boot.txt
    done
fi
PILOT=0
if [[ "$MODE" == coop ]]; then PILOT=1; fi
host_pid=''
cleanup(){ if [[ -n "$host_pid" ]]; then kill "$host_pid" 2>/dev/null || :; fi; }
trap cleanup EXIT
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_TRACE=1 BUGSTAR_LAN_BOOT=1 \
    BUGSTAR_LAN_TEST_PILOT="$PILOT" BUGSTAR_LAN_CLEAR_SHOT=out/lan_live/host-clear.png \
    timeout -k 3 70 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 240 160 --lan-host "${LAN_MODE[@]}" --lan-port 33777 \
    --data out/lan_live/host --script out/lan_live/boot.txt \
    --auto-shot 1570 out/lan_live/host.png --stop-after 1610 \
    > out/lan_live/host.log 2>&1 &
host_pid=$!
sleep 0.3
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_TRACE=1 BUGSTAR_LAN_BOOT=1 \
    BUGSTAR_LAN_TEST_PILOT="$PILOT" BUGSTAR_LAN_CLEAR_SHOT=out/lan_live/client-clear.png \
    timeout -k 3 70 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 240 160 --lan-join 127.0.0.1 "${LAN_MODE[@]}" --lan-port 33777 \
    --data out/lan_live/client --script out/lan_live/boot.txt \
    --auto-shot 1570 out/lan_live/client.png --stop-after 1610 \
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
if [[ "$MODE" == coop ]]; then
    for side in host client; do
        grep -Eq 'lan: raider HP 0 wins [1-9][0-9]* phase 2 hits [1-9][0-9]*/[1-9][0-9]*' "out/lan_live/${side}.log" || {
            echo "LAN smoke: ${side} has not proven BOTH pilots hit and defeated the SAME raider"
            tail -70 "out/lan_live/${side}.log"; exit 1; }
        for wave in 1 2 3; do
            grep -Eq "lan: raider .* wave ${wave} " "out/lan_live/${side}.log" || {
                echo "LAN smoke: ${side} never reached wave ${wave}"
                tail -70 "out/lan_live/${side}.log";exit 1; }
        done
        grep -Eq 'lan: raider HP 0 wins 3 phase 4 hits [1-9][0-9]*/[1-9][0-9]* wave 3 wing 0 cleared 1' "out/lan_live/${side}.log" || {
            echo "LAN smoke: ${side} missed the three-wave clear";tail -70 "out/lan_live/${side}.log";exit 1; }
        grep -q 'lan: victory overlay rendered' "out/lan_live/${side}.log" || {
            echo "LAN smoke: ${side} victory state never drew the actual 3D victory overlay";exit 1; }
        test -s "out/lan_live/${side}-clear.png" || {
            echo "LAN smoke: ${side} did not capture its rendered victory";exit 1; }
    done
else
    for side in host client; do
        grep -Eq 'lan: duel .* wins=[1-9]' "out/lan_live/${side}.log" || {
            echo "LAN smoke: ${side} did not see the actual synchronized sparring victory"
            tail -70 "out/lan_live/${side}.log"; exit 1; }
    done
fi
for side in host client; do
    cp "out/lan_live/${side}.png" "out/lan_live/${MODE}_${side}.png"
    cp "out/lan_live/${side}.log" "out/lan_live/${MODE}_${side}.log"
    if [[ "$MODE" == coop ]]; then
        cp "out/lan_live/${side}-clear.png" "out/lan_live/coop_${side}_clear.png"
    fi
done
echo "LAN smoke: mode ${MODE}, two independent full PC games connected, finished and rendered screenshots PASS"
