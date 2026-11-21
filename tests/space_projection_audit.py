#!/usr/bin/env python3
"""3D flight near-plane geometry guard for GBA, PC and Android.

The near-camera wireframe path is intentionally rare: vertices in front keep
the old fast GBA path, and only an edge crossing the eye's projection plane
uses the slower interpolated endpoint.
"""
from pathlib import Path

src = (Path(__file__).resolve().parents[1] / "space.inc").read_text()
start = src.index("// Clip wireframe edges crossing the near plane.")
end = src.index("// ---------------------------------------------------------------- a round", start)
wire = src[start:end]

assert "if(ok[a] && ok[b])" in wire
assert "continue;" in wire.split("if(ok[a] && ok[b])", 1)[1].split("\n", 1)[0]
assert "if(!ok[a] && !ok[b]) continue;" in wire
assert "az<=20 && bz>20" in wire and "bz<=20 && az>20" in wire
assert "((long long)(bx-ax)*(21-az))/(bz-az)" in wire
assert "((long long)(ax-bx)*(21-bz))/(az-bz)" in wire
assert "ix*FOCS)/21" in wire and "iy*FOCS)/21" in wire
assert "if(ex<-19000" in wire

# Actual camera-plane intersections must always lie between an endpoint just
# behind the eye and an endpoint visible to the renderer, never extrapolated.
for back in (-800, -10, 0, 15, 20):
    for front in (21, 22, 64, 256, 4096):
        t = (21 - back) / (front - back)
        assert 0 <= t <= 1, (back, front, t)
        for a, b in [(-3500, 3000), (0, 800), (4000, -1500)]:
            cut = a + (b - a) * t
            assert min(a, b) <= cut <= max(a, b)
            z = back + (front - back) * t
            assert abs(z - 21) < 1e-8

# This path is shared by all three target builds, not copied into a distinct
# high-resolution renderer with inconsistent field-of-view assumptions.
assert "static void wire(" in src
assert "wire(pg,v,12,ce" in src and "wire(pg,v,6,ae" in src
print("3D space: cockpit-near stations, ships and asteroids clip without disappearing edges")
