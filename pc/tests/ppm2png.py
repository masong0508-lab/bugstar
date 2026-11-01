#!/usr/bin/env python3
# ppm2png.py IN.ppm... : writes IN.png scaled x3 (nearest) so the 240x160 screens can be looked at. usage: ppm2png.py [--sheet OUT.png] files
import sys
from PIL import Image
a = sys.argv[1:]; sheet = None
if a and a[0] == "--sheet": sheet = a[1]; a = a[2:]
ims = [Image.open(f).convert("RGB") for f in a]
if sheet:
    w, h = ims[0].size; cols = 2; rows = (len(ims) + 1) // 2; S = 2
    im = Image.new("RGB", (cols * (w * S + 4), rows * (h * S + 4)), (40, 40, 40))
    for i, x in enumerate(ims): im.paste(x.resize((w * S, h * S), Image.NEAREST), ((i % cols) * (w * S + 4), (i // cols) * (h * S + 4)))
    im.save(sheet)
else:
    for f, x in zip(a, ims): x.resize((x.width * 3, x.height * 3), Image.NEAREST).save(f.rsplit(".", 1)[0] + ".png")
