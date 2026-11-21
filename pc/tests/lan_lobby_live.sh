#!/usr/bin/env bash
# Two complete SDL games connect WHILE their options menu is open.
# A socket-only test misses this because plat_frame pauses world time and
# previously starved HELLO/READY during the menu's event loop.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
mkdir -p out/lan_lobby/host out/lan_lobby/client
printf 'pause_unfocus=0\nvsync=0\n' > out/lan_lobby/host/settings.ini
printf 'pause_unfocus=0\nvsync=0\n' > out/lan_lobby/client/settings.ini
rm -f out/lan_lobby/{host,client}.log out/lan_lobby/{host,client}.png
{
    # BUGSTAR_MENU=10 opens the real LAN overlay at flight frame 30.
    # This script waits more than 2s inside the paused options menu, then
    # proves the UDP peers became connected without advancing the world.
    printf '190 nop\n0 dump\n0 shot out/lan_lobby/MENU.png\n'
    # ESC backs to the main options page; ESC again returns to flight.
    printf '0 key down Escape\n0 key down Escape\n'
    printf '120 nop\n0 dump\n60 nop\n0 quit\n'
} > out/lan_lobby/events.txt
host_pid=''
cleanup(){ if [[ -n "$host_pid" ]]; then kill "$host_pid" 2>/dev/null || :; fi; }
trap cleanup EXIT
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_BOOT=1 BUGSTAR_LAN_TRACE=1 \
    BUGSTAR_MENU=10 BUGSTAR_EVENTS=out/lan_lobby/events.txt \
    timeout -k 3 45 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 480 320 --lan-host --lan-coop --lan-port 33779 \
    --data out/lan_lobby/host > out/lan_lobby/host.log 2>&1 &
host_pid=$!
sleep 0.25
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_BOOT=1 BUGSTAR_LAN_TRACE=1 \
    BUGSTAR_MENU=10 BUGSTAR_EVENTS=out/lan_lobby/events.txt \
    timeout -k 3 45 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 480 320 --lan-join 127.0.0.1 --lan-coop --lan-port 33779 \
    --data out/lan_lobby/client > out/lan_lobby/client.log 2>&1 || {
        echo 'LAN lobby: guest game exited unsuccessfully'
        tail -80 out/lan_lobby/client.log; exit 1; }
wait "$host_pid" || { echo 'LAN lobby: host game failed'; tail -80 out/lan_lobby/host.log; exit 1; }
host_pid=''
for side in host client; do
    log="out/lan_lobby/${side}.log"
    grep -q 'lan: lobby peer CONNECTED' "$log" || {
        echo "LAN lobby: ${side} could not connect in the PAUSED options menu"
        tail -90 "$log";exit 1; }
    grep -Eq 'EV dump: menu=1 page=10 .*lan_connected=1 .*lan_ready=1 lan_peer_ready=1 lan_live=0' "$log" || {
        echo "LAN lobby: ${side} did not see BOTH Ready peers while combat paused"
        tail -90 "$log";exit 1; }
    grep -Eq 'EV dump: menu=0 .*lan_connected=1 .*lan_ready=1 lan_peer_ready=1 lan_live=1' "$log" || {
        echo "LAN lobby: ${side} did not resume active flight after leaving options"
        tail -90 "$log";exit 1; }
    ! grep -E 'lan: raider HP .*phase [1-4]' "$log" | grep -q 'frame 30' || {
        echo "LAN lobby: unexpected physics while paused";exit 1; }
done
echo "LAN SDL lobby: two games connected while PAUSED, both confirmed Ready, resumed flight PASS"
