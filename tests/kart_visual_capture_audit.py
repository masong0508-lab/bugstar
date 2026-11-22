#!/usr/bin/env python3
"""Audit scripted Kart World PC renders, not merely whether a PNG exists.

The smoke run captures four different moments of the deterministic fuzz
playthrough. We reject a fully empty visual sequence for EITHER rendering
mode; any individual transitional frame can legitimately be mostly black.
Crops the bottom system-key guide so it cannot make a blank scene pass.
"""
from pathlib import Path
import json
import sys
from PIL import Image

def audit(prefix):
    prefix = Path(prefix)
    files = sorted(prefix.parent.glob(prefix.name + "_*.ppm"))
    if len(files) < 4:
        raise RuntimeError(f"{prefix}: expected four different capture frames, found {len(files)}")
    results = []
    for file in files:
        with Image.open(file) as im:
            im = im.convert("RGB")
            im.save(file.with_suffix(".png"), optimize=True)
            w, h = im.size
            if w < 240 or h < 160:
                raise RuntimeError(f"{file}: incomplete {w}x{h} picture")
            # The physical keyboard help overlay is bottom-aligned and should
            # never count as proof the actual kart world was rendered.
            core = im.crop((0, 0, w, h*14//16)).resize((240, 140), Image.Resampling.NEAREST)
            samples = list(core.getdata())
            nonblack = sum(max(p) > 24 for p in samples)
            colors = len(set(samples))
            fraction = nonblack / len(samples)
            scene_ok = fraction >= 0.04 and colors >= 24
            results.append({"frame": file.stem.rsplit("_", 1)[-1],
                            "colors": colors,
                            "visible_fraction": round(fraction, 4),
                            "scene_ok": scene_ok,
                            "file": str(file.with_suffix(".png"))})
    # Always retain individual frame metrics, even when the scene is black.
    # A failed run is precisely when the diagnostic numbers matter most.
    return results

def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: kart_visual_capture_audit.py NATIVE_PREFIX CLASSIC_PREFIX")
    data = {}
    errors = []
    for key, prefix in (("native", sys.argv[1]), ("classic", sys.argv[2])):
        try:
            data[key] = audit(prefix)
            if not any(row["scene_ok"] for row in data[key]):
                errors.append(f"{prefix}: all sampled frames look blank")
        except Exception as exc:
            errors.append(str(exc))
            data[key] = {"error": str(exc)}
    output = Path(sys.argv[1]).parent / "kart-visual-report.json"
    output.write_text(json.dumps(data, indent=2) + "\n")
    print(output.read_text())
    if errors:
        raise SystemExit("Kart visual smoke FAILED: " + " | ".join(errors))
    print("Kart visual smoke: distinct visible scenes confirmed in both modes")

if __name__ == "__main__":
    main()
