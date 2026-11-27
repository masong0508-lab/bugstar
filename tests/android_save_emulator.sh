#!/usr/bin/env bash
# Run on a fresh CI emulator only. Never touches a real player's files.
set -euo pipefail
pkg=org.bugstar.app
activity="$pkg/.BugstarActivity"
wait_log(){
    local tag=$1 pattern=$2 line
    for ((n=0;n<60;n++));do
        line=$(adb logcat -d -s "$tag:I" '*:S' | grep -F "$pattern" | tail -1 || true)
        if [[ -n "$line" ]];then printf '%s\n' "$line";return 0;fi
        sleep 1
    done
    echo "Timed out: $tag / $pattern" >&2;return 1
}
field(){
    local line=$1 key=$2 value
    value=$(printf '%s\n' "$line" | sed -nE "s/.* ${key}=(-?[0-9]+).*/\\1/p")
    [[ "$value" =~ ^-?[0-9]+$ ]] || return 1
    printf '%s\n' "$value"
}
tap(){
    local x=$1 y=$2 dw=$3 dh=$4 view left top vw vh
    view=$(wait_log BUGSTAR_VIEW 'surface left=')
    left=$(field "$view" left);top=$(field "$view" top);vw=$(field "$view" w);vh=$(field "$view" h)
    adb shell input tap "$((left+x*vw/dw))" "$((top+y*vh/dh))"
}
# Use the genuine game UI, including pagination. Export is main-menu item 11.
mode=$(wait_log BUGSTAR_PAD 'mode w=')
w=$(field "$mode" w);h=$(field "$mode" h)
tap "$(field "$mode" x)" "$(field "$mode" y)" "$w" "$h"
menu=$(wait_log BUGSTAR_UI 'touch menu page=0 ')
for ((n=0;n<14;n++));do
    w=$(field "$menu" w);h=$(field "$menu" h)
    first=$(field "$menu" first);rows=$(field "$menu" rows)
    footer=$(field "$menu" footer);row=$(field "$menu" row);head=$(field "$menu" header)
    if ((first<=11 && first+rows>11));then break;fi
    tap "$((w*5/6))" "$((h-footer/2))" "$w" "$h"
    menu=$(wait_log BUGSTAR_UI "first=$((first+rows)) rows=")
done
((first<=11 && first+rows>11))
tap "$((w/2))" "$((head+(11-first)*row+row/2))" "$w" "$h"
wait_log BUGSTAR_SAVE 'picker launched export'
# Wait for the system documents activity to become visible (both AOSP and Google variants).
found=0
for ((n=0;n<20;n++));do
    if adb shell dumpsys activity activities | grep -Ei 'mResumedActivity|topResumedActivity' | grep -qi 'documentsui';then found=1;break;fi
    sleep 1
done
((found==1))
# On API 21 the picker can open with its navigation drawer and keyboard.
# ActivityManager may call it resumed before the window accepts key events.
sleep 2
adb exec-out screencap -p > android-save-picker.png
for ((n=0;n<8;n++));do
    if adb logcat -d -s BUGSTAR_SAVE:I | grep -Fq 'picker cancelled';then break;fi
    if adb shell dumpsys activity activities | grep -Ei 'mResumedActivity|topResumedActivity' | grep -qi 'documentsui';then
        adb shell input keyevent KEYCODE_BACK
    fi
    sleep 2
done
wait_log BUGSTAR_SAVE 'picker cancelled'
save=$(wait_log BUGSTAR_SAVE 'cartridge path=' | sed 's/.*cartridge path=//' | tr -d '\r')
[[ "$save" =~ ^/data/[A-Za-z0-9_./-]+/bugstar\.sav$ ]]
adb shell am force-stop "$pkg"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
adb exec-out run-as "$pkg" cat "$save" > "$tmp/previous.sav"
python3 - "$tmp" <<'PY'
from pathlib import Path
import struct,zlib,sys
p=Path(sys.argv[1]);raw=(p/"previous.sav").read_bytes()
assert len(raw)==32768,len(raw)
(p/"restore.bgsave").write_bytes(b"BGSTAR1\n"+struct.pack(">I",zlib.crc32(raw))+raw)
PY
# Add a sentinel beyond the old file's cartridge bytes. It is intentionally
# not in the incoming restore, proving recovery and replacement differ.
printf '\x7e' >> "$tmp/previous.sav"
adb push "$tmp/previous.sav" /data/local/tmp/bugstar-previous-test.sav
adb shell "run-as $pkg sh -c 'cat > $save' < /data/local/tmp/bugstar-previous-test.sav"
adb push "$tmp/restore.bgsave" /data/local/tmp/bugstar-restore-test.bgsave
adb shell "run-as $pkg sh -c 'cat > $save.restore' < /data/local/tmp/bugstar-restore-test.bgsave"
adb logcat -c
adb shell am start -W -n "$activity"
wait_log BUGSTAR_SAVE 'restore result=1'
wait_log BUGSTAR_BOOT 'first rendered game frame'
adb exec-out run-as "$pkg" cat "$save.before-import" > "$tmp/recovery.sav"
cmp "$tmp/previous.sav" "$tmp/recovery.sav"
adb exec-out run-as "$pkg" cat "$save" > "$tmp/restored.sav"
test "$(wc -c < "$tmp/restored.sav")" -eq 32768
adb shell "run-as $pkg sh -c 'test ! -f $save.restore'"
adb shell am force-stop "$pkg"
adb logcat -c
adb shell am start -W -n "$activity"
wait_log BUGSTAR_BOOT 'first rendered game frame'
if adb logcat -d -s BUGSTAR_SAVE:I | grep -Fq 'restore result=1';then echo 'Restore repeated on second launch' >&2;exit 1;fi
echo 'Android export picker cancellation, real startup restore, recovery bytes, and one-time consumption PASS'
