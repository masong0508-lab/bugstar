#!/bin/bash
# pc/seedview.sh [--universe HEX] [--cx HEX] [--cy HEX] [--star N] [--planet N] [--moon N] [--out prefix]
# Runs the viewer and turns its pictures into one PNG: out/<prefix>_sheet.png (4 views + the facts card). Needs python3 + PIL.
cd "$(dirname "$0")"; mkdir -p out
OUT=out/seed; args=("$@"); for ((i=0;i<${#args[@]};i++)); do [ "${args[$i]}" = "--out" ] && OUT=${args[$((i+1))]}; done
[ -x ./bugstar_pc ] || ./build.sh
./bugstar_pc --view --scale 2 "$@" --out "$OUT" || exit 1
python3 - "$OUT" <<'P'
import sys
from PIL import Image
o=sys.argv[1]; ims=[Image.open(f"{o}_view{k}.ppm") for k in range(4)]+[Image.open(f"{o}_card.ppm")]
w,h=ims[0].size; sh=Image.new("RGB",(w*3+8,h*2+4),(24,24,24))
for i,im in enumerate(ims): sh.paste(im,((i%3)*(w+4),(i//3)*(h+4)))
sh.save(f"{o}_sheet.png"); print("wrote",f"{o}_sheet.png")
P
