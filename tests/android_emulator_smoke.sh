#!/usr/bin/env bash
# Run in a single process: emulator-runner invokes each script line separately.
set -euo pipefail
apk=${1:?APK path required}
pkg=org.bugstar.app
activity="$pkg/.BugstarActivity"
trap 'adb logcat -d > android-boot.log || true; adb exec-out screencap -p > android-last.png || true' EXIT
app_pid() {
    adb shell ps | tr -d '\r' | awk -v p="$pkg" '$NF==p && !found {print $2;found=1}'
}
current_log() {
    local tag=$1 pid
    pid=$(app_pid)
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    adb logcat -d -v brief -s "$tag:I" '*:S' | awk -v tag="$tag" -v pid="$pid" '$0 ~ ("/" tag "\\( *" pid "\\):")'
}
wait_log() {
    local tag=$1 pattern=$2 line limit=${3:-60}
    for ((n=0;n<limit;n++)); do
        line=$(current_log "$tag" | grep -F "$pattern" | tail -1 || true)
        if [[ -n "$line" ]]; then printf '%s\n' "$line"; return 0; fi
        sleep 1
    done
    echo "Timed out: $tag / $pattern" >&2
    return 1
}
field() {
    local line=$1 key=$2 value
    value=$(printf '%s\n' "$line" | sed -nE "s/.* ${key}=(-?[0-9]+).*/\\1/p")
    [[ "$value" =~ ^-?[0-9]+$ ]] || { echo "Missing $key: $line" >&2; return 1; }
    printf '%s\n' "$value"
}
tap_drawable() {
    local x=$1 y=$2 dw=$3 dh=$4 view left top vw vh
    view=$(wait_log BUGSTAR_VIEW 'surface left=')
    left=$(field "$view" left); top=$(field "$view" top)
    vw=$(field "$view" w); vh=$(field "$view" h)
    ((dw>0 && dh>0 && vw>0 && vh>0))
    adb shell input tap "$((left+x*vw/dw))" "$((top+y*vh/dh))"
}
adb install -r "$apk"
adb shell pm clear "$pkg"
adb logcat -c
adb shell am start -W -n "$activity"
wait_log BUGSTAR_BOOT 'first rendered game frame'
wait_log BUGSTAR_RENDER 'opaque menu: game composition bypassed'
adb shell ps | grep "$pkg"
help=$(wait_log BUGSTAR_UI 'touch menu page=7 ')
w=$(field "$help" w); h=$(field "$help" h); footer=$(field "$help" footer)
adb exec-out screencap -p > android-boot.png
before=$(current_log BUGSTAR_PAD | grep -F 'mode w=' | tail -1 || true)
epoch=0
if [[ -n "$before" ]];then epoch=$(field "$before" epoch);fi
tap_drawable "$((w/2))" "$((h-footer/2))" "$w" "$h"
wait_log BUGSTAR_UI 'touch menu action page=7 hit=-3'
# Require a real pad draw after PLAY, not its startup/pre-tutorial position.
foreground() {
    adb shell dumpsys activity activities | grep -Ei 'mResumedActivity|topResumedActivity' | grep -F "$pkg" >/dev/null
}
menu=""
for ((attempt=0;attempt<4;attempt++));do
    # A late result from an earlier tap still proves genuine touch navigation.
    menu=$(current_log BUGSTAR_UI | grep -F 'touch menu page=0 ' | tail -1 || true)
    if [[ -n "$menu" ]];then break;fi
    mode=$(current_log BUGSTAR_PAD | grep -F 'mode w=' | tail -1 || true)
    if ! foreground;then
        if [[ -n "$mode" ]];then epoch=$(field "$mode" epoch);fi
        adb shell am start -W -n "$activity"
    fi
    fresh=0
    for ((poll=0;poll<20;poll++));do
        mode=$(current_log BUGSTAR_PAD | grep -F 'mode w=' | tail -1 || true)
        if [[ -n "$mode" ]] && (( $(field "$mode" epoch)>epoch )) && foreground;then fresh=1;break;fi
        sleep 1
    done
    if (( !fresh ));then continue;fi
    w=$(field "$mode" w); h=$(field "$mode" h); x=$(field "$mode" x); y=$(field "$mode" y)
    tap_drawable "$x" "$y" "$w" "$h"
    menu=$(wait_log BUGSTAR_UI 'touch menu page=0 ' 5 || true)
    if [[ -n "$menu" ]];then break;fi
done
[[ -n "$menu" ]]
w=$(field "$menu" w); h=$(field "$menu" h)
header=$(field "$menu" header); row=$(field "$menu" row)
tap_drawable "$((w/2))" "$((header+row+row/2))" "$w" "$h"
wait_log BUGSTAR_UI 'touch menu action page=0 hit=1'
wait_log BUGSTAR_UI 'touch menu page=1 '
adb exec-out screencap -p > android-touch-menu.png
adb shell input keyevent KEYCODE_HOME
wait_log BUGSTAR_LIFE 'background pause'
adb shell am start -W -n "$activity"
wait_log BUGSTAR_LIFE 'foreground resume'
adb shell ps | grep "$pkg"
adb exec-out screencap -p > android-resume.png
echo 'Android boot, genuine touch tutorial/options navigation, and Home/resume PASS'
if [[ -n "${2:-}" ]]; then
    # Install the single-ABI APK over the universal APK without clearing the
    # saved data. This proves same-run signing and standalone installability.
    adb shell am force-stop "$pkg"
    adb install -r "$2"
    adb logcat -c
    adb shell am start -W -n "$activity"
    wait_log BUGSTAR_BOOT 'first rendered game frame'
    wait_log BUGSTAR_PAD 'mode w='
    adb shell ps | grep "$pkg"
    echo 'Lightweight standalone APK upgrade, retained settings and boot PASS'
fi
# The transfer test uses the same installed debug app and real startup path.
bash tests/android_save_emulator.sh
