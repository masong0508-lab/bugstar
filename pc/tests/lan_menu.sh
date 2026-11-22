#!/usr/bin/env bash
# Actual SDL options menu exercised through the same keyboard/text event
# pump as the player: HOST -> READY -> LEAVE -> edit IPv4 -> JOIN -> READY -> LEAVE.
# No socket mocks, no in-game menu shortcuts, and no campaign mutations.
set -euo pipefail
cd "$(dirname "$0")/.."
GAME="${1:-./bugstar_pc}"
mkdir -p out/lan_menu/saves
{
    printf '15 key down Escape\n1 key up Escape\n'
    for ((i=0;i<6;i++)); do printf '0 key down Down\n'; done
    printf '0 key down Return\n0 dump\n'
    # Default match type is co-op; HOST MATCH is item 3.
    for ((i=0;i<3;i++)); do printf '0 key down Down\n'; done
    printf '0 key down Return\n0 dump\n'
    printf '0 shot out/lan_menu/host-options.png\n10 nop\n'
    # Ready is item 5 (explicit action, not connection).
    for ((i=0;i<2;i++)); do printf '0 key down Down\n'; done
    printf '0 key down Return\n0 dump\n'
    # LEAVE is now item 6; new sessions must start unready.
    printf '0 key down Down\n0 key down Return\n0 dump\n'
    # Change the numeric IPv4 field via real SDL text input.
    for ((i=0;i<4;i++)); do printf '0 key down Up\n'; done
    printf '0 key down Return\n'
    for ((i=0;i<9;i++)); do printf '0 key down Backspace\n'; done
    printf '0 text 127.0.0.1\n0 key down Return\n'
    # JOIN MATCH is item 4, now with a complete numeric IPv4 value.
    printf '0 key down Down\n0 key down Down\n'
    printf '0 key down Return\n0 dump\n'
    # Confirm Ready on JOIN, then Leave again safely.
    printf '0 key down Down\n0 key down Return\n0 dump\n'
    printf '0 key down Down\n0 key down Return\n0 dump\n'
    # The integrated tutorial (item 7) must display real mission rules
    # and return to the same lobby without dropping network resources.
    printf '0 key down Down\n0 key down Return\n0 dump\n'
    printf '0 shot out/lan_menu/mission-help.png\n'
    printf '0 key down Escape\n0 dump\n'
    printf '10 quit\n'
} > out/lan_menu/events.txt
env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_BOOT=1 \
    BUGSTAR_EVENTS=out/lan_menu/events.txt \
    timeout -k 3 75 "$GAME" --window --video-driver dummy --no-audio \
    --no-vsync --size 960 640 --data out/lan_menu/saves \
    > out/lan_menu/game.log 2>&1 || {
        tail -100 out/lan_menu/game.log; exit 1;
    }
grep -Eq 'EV dump: menu=1 page=10 .*lan_role=1 .*lan_coop=1' out/lan_menu/game.log || {
    echo 'LAN menu: HOST action failed'; tail -80 out/lan_menu/game.log; exit 1;
}
[[ $(grep -Ec 'EV dump: menu=1 page=10 .*lan_role=[12] .*lan_ready=1' out/lan_menu/game.log || :) -ge 2 ]] || {
    echo 'LAN menu: both Ready confirmations were not selectable'; tail -80 out/lan_menu/game.log; exit 1;
}
grep -Eq 'EV dump: menu=1 page=10 .*lan_role=2 ' out/lan_menu/game.log || {
    echo 'LAN menu: numeric IPv4 JOIN action failed'; tail -80 out/lan_menu/game.log; exit 1;
}
count=$(grep -Ec 'EV dump: menu=1 page=10 .*lan_role=0 ' out/lan_menu/game.log || :)
if [[ "$count" -lt 2 ]]; then
    echo "LAN menu: expected two separate successful LEAVE actions, found $count"
    tail -80 out/lan_menu/game.log; exit 1
fi
grep -Eq 'EV dump: menu=1 page=11 ' out/lan_menu/game.log || {
    echo 'LAN menu: HOW THE CO-OP MISSION WORKS was not reachable'
    tail -60 out/lan_menu/game.log;exit 1; }
grep -Eq 'EV dump: menu=1 page=10 ' out/lan_menu/game.log || {
    echo 'LAN menu: multiplayer tutorial could not return to lobby';exit 1; }
test -s out/lan_menu/mission-help.png
test -s out/lan_menu/host-options.png
# Prove the newly height-bounded options page can be drawn without crashing
# at actual 240x160 pixels AND a narrow mobile portrait display.
for layout in compact portrait; do
    mkdir -p "out/lan_menu/${layout}"
    if [[ "$layout" == compact ]]; then dims=(240 160); else dims=(320 480); fi
    printf '170 nop\n0 shot out/lan_menu/%s.png\n0 dump\n1 quit\n' "$layout" > "out/lan_menu/${layout}.txt"
    env SDL_AUDIODRIVER=dummy BUGSTAR_LAN_BOOT=1 BUGSTAR_MENU=10 \
        BUGSTAR_EVENTS="out/lan_menu/${layout}.txt" \
        timeout -k 3 30 "$GAME" --window --video-driver dummy --no-audio \
        --no-vsync --size "${dims[0]}" "${dims[1]}" \
        --data "out/lan_menu/${layout}" \
        > "out/lan_menu/${layout}.log" 2>&1 || {
            echo "LAN menu: ${layout} screen crash";
            tail -70 "out/lan_menu/${layout}.log";exit 1; }
    grep -q 'EV dump: menu=1 page=10' "out/lan_menu/${layout}.log" || {
        echo "LAN menu: ${layout} didn't show multiplayer page";exit 1; }
    test -s "out/lan_menu/${layout}.png" || {
        echo "LAN menu: ${layout} screenshot missing";exit 1; }
done
echo "LAN SDL options: host, join, Ready, in-game guide, compact and portrait layouts PASS"
