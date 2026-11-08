#!/bin/bash
# vt_run.sh NAME [env...]   : one headless picture run of a 3D view at 1200x800 (BUGSTAR_VT hook in pc_warship.inc). Needs tests/vt_idle.txt style scripts.
#   KEYS=script FR=frames SC=scale (5 = 1200x800)
cd "$(dirname "$0")/.."; mkdir -p out/vt; n=$1; shift; rm -f out/vt/$n.sav*
env PC_NATIVE=2 PC_SCALE=${SC:-5} BUGSTAR_SIDE=0 BUGSTAR_VT_AT=${AT:-100} "$@" ./bugstar_pc --script ${KEYS:-tests/vt_idle.txt} --out out/vt/$n --sav out/vt/$n.sav --frames ${FR:-3150} > out/vt/$n.log 2> out/vt/$n.err
for f in out/vt/${n}_*.ppm; do [ -f "${f%.ppm}.png" ] && [ "${f%.ppm}.png" -nt "$f" ] || python3 -c "
import sys
from PIL import Image
Image.open('$f').save('${f%.ppm}.png')"; done
