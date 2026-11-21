#!/usr/bin/env python3
"""Verify the ACTUAL two independently rendered 240x160 victory screenshots.

The old screenshot revealed a ship cutting across MISSION COMPLETE.
This pixel test requires the new solid, bright-edged results plate and real
underlying 3D world detail; log-only mission completion is insufficient.
"""
from pathlib import Path
from PIL import Image
import sys

assert len(sys.argv)==3, "two player victory screenshots required"
for file_name in sys.argv[1:]:
    with Image.open(file_name) as src:
        image=src.convert("RGB")
    assert image.size==(240,160), (file_name,image.size)
    p=image.load()
    def bright(x,y):
        r,g,b=p[x,y]
        return r+g+b>=115
    # Full-width gold border on both ends of the result plate (not the HUD).
    for y in (55,95):
        n=sum(bright(x,y) for x in range(50,190))
        assert n>=135,(file_name,"victory panel border missing",y,n)
    # A dark backing remains even where the planet and P2 ship cross text.
    for x in (50,188):
        r,g,b=p[x,72]
        assert max(r,g,b)<85,(file_name,"result plate not opaque",x,(r,g,b))
    # Letter glyphs actually rendered into the panel, rather than a black
    # rectangle that would technically pass the plate-only checks.
    glyphs=sum(bright(x,y) for y in range(61,77) for x in range(56,185))
    assert glyphs>=90,(file_name,"mission title glyphs missing",glyphs)
    # Drawing the plate must never replace the 3D world behind the mission.
    green=sum(1 for y in range(22,52) for x in range(85,150)
              if p[x,y][1]>p[x,y][0] and p[x,y][1]>p[x,y][2])
    assert green>=100,(file_name,"planet/world disappeared",green)
    print(f"BUGSTAR graphical victory: readable result plate above ships, live 3D world {Path(file_name).name} PASS")
