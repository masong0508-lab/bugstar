#!/usr/bin/env bash
# Actual SDL options menu exercised through the same keyboard/text event
# pump as the player: HOST -> LEAVE -> edit IPv4 -> JOIN -> LEAVE.
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
    # LEAVE MATCH is item 5.
    for ((i=0;i<2;i++)); do printf '0 key down Down\n'; done
    printf '0 key down Return\n0 dump\n'
    # Change the numeric IPv4 field via real SDL text input.
    for ((i=0;i<3;i++)); do printf '0 key down Up\n'; done
    printf '0 key down Return\n'
    for ((i=0;i<9;i++)); do printf '0 key down Backspace\n'; done
    printf '0 text 127.0.0.1\n0 key down Return\n'
    # JOIN MATCH is item 4, now with a complete numeric IPv4 value.
    printf '0 key down Down\n0 key down Down\n'
    printf '0 key down Return\n0 dump\n'
    # Leave again. In solo mode, no LAN callback remains active.
    printf '0 key down Down\n0 key down Return\n0 dump\n'
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
grep -Eq 'EV dump: menu=1 page=10 .*lan_role=2 ' out/lan_menu/game.log || {
    echo 'LAN menu: numeric IPv4 JOIN action failed'; tail -80 out/lan_menu/game.log; exit 1;
}
count=$(grep -Ec 'EV dump: menu=1 page=10 .*lan_role=0 ' out/lan_menu/game.log || :)
if [[ "$count" -lt 2 ]]; then
    echo "LAN menu: expected two separate successful LEAVE actions, found $count"
    tail -80 out/lan_menu/game.log; exit 1
fi
test -s out/lan_menu/host-options.png
echo "LAN SDL options: host, leave, IPv4 editor, join and leave PASS"
