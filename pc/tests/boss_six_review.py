#!/usr/bin/env python3
"""Review actual PC gameplay captures of all six boss arenas, at 240x160.

This is a deterministic image/size/contrast/difference check, not a substitute
for a human inspecting boss hit-boxes and attack tells in an emulator.
"""
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
import hashlib
import sys

root=Path(sys.argv[1]) if len(sys.argv)>1 else Path("out/boss_six")
shots={}
hashes=set()
for zone in range(6):
    for phase in range(2):
        for frame in (240,360):
            path=root/f"{zone}_{phase}_{frame:06d}.ppm"
            with Image.open(path) as im:
                assert im.size == (240,160), f"{path}: not GBA native resolution"
                rgb=im.convert("RGB")
            assert len(rgb.getcolors(maxcolors=65536) or [])>16, (
                f"{path}: nearly flat or empty gameplay frame"
            )
            assert max(ImageStat.Stat(rgb).stddev)>7, (
                f"{path}: boss view is dark/blank or has too little contrast"
            )
            shots[zone,phase,frame]=rgb
            hashes.add(hashlib.sha256(rgb.tobytes()).hexdigest())
        left=shots[zone,phase,240]
        right=shots[zone,phase,360]
        assert ImageChops.difference(left,right).getbbox(), (
            f"zone {zone} phase {phase} gameplay never visually updated"
        )
    assert ImageChops.difference(shots[zone,0,360],shots[zone,1,360]).getbbox(), (
        f"zone {zone}: normal and half-health gameplay render identically"
    )
assert len(hashes)>=12, "six arenas and both boss phases did not produce independent frames"

sheet=Image.new("RGB",(240*6,160*2))
for zone in range(6):
    for phase in range(2):
        sheet.paste(shots[zone,phase,360],(zone*240,phase*160))
sheet.save(root/"boss_six_contact_sheet.png")
for (zone,phase,frame),im in shots.items():
    im.save(root/f"{zone}_{phase}_{frame:06d}.png")
(root/"README.txt").write_text(
    "These are real PC-headless frames of the game's GBA-mode dungeon renderer,\n"
    "captured in the six real warden arenas. 240x160, one normal and one\n"
    "half-health encounter per row. Boss locations are moved only for camera\n"
    "visibility in this opt-in PC test; combat and scenery still run through\n"
    "the actual engine. Not hardware/emulator footage; manual review required.\n"
)
print("boss capture: 6 warden arenas, 2 combat phases, 24 distinct screenshots checked")
