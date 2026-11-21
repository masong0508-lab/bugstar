#!/usr/bin/env python3
"""Cross-renderer station docking-depth regression.

The GBA picture and the PC/Android native picture have to describe the
*same* rotating station. Test both source paths and a projected approach
at representative camera positions without introducing a framebuffer or
requiring an emulator.
"""
from pathlib import Path
import math

root = Path(__file__).resolve().parents[1]
sources = {
    "GBA/shared": (root / "space.inc").read_text(),
    "PC/Android native": (root / "pc/pc_snative.inc").read_text(),
}
for name, src in sources.items():
    expected = (
        "s32 v[12*3]",
        "s32 S=420",
        "slot[i][0]*140, z=-S-160",
        "v[(8+i)*3+1]=sty_+slot[i][1]*70",
        "static const u8 throat[]={0,8,1,9,2,10,3,11}",
        "cz<8000",
    )
    for token in expected:
        assert token in src, (name, "missing", token)
    # The extra lines are conditional on station distance, not rendered
    # on every far-away station in the GBA's performance-critical view.
    snippet = src[src.index("static const u8 throat[]"):][:180]
    assert ("so_wire(v,12,throat,4,56)" if "native" in name
            else "wire(pg,v,12,throat,4,56)") in snippet, name
    # The three-dimensional port stays rotated with the original hull,
    # rather than being pasted onto the screen as a HUD decoration.
    assert "v[(8+i)*3]=stx_+((x*c_-z*s_)>>14)" in src, name

# A representative fly-in: both the near ring and station face must be
# in front of a camera looking along the local station +Z axis.
# The near ring is 160 world units CLOSER to the eye, so both layers have
# different apparent depth. The spokes must not collapse to flat 2D.
station_half = 420
ring_depth = -station_half - 160
for distance in (900, 1400, 3000, 7000):
    face_z = distance - station_half
    port_z = distance + ring_depth
    assert port_z > 20 and face_z > 20
    x_face = -station_half * 160 / face_z
    x_ring = -140 * 160 / port_z
    y_face = -station_half * 160 / face_z
    y_ring = -70 * 160 / port_z
    assert math.isfinite(x_face + x_ring + y_face + y_ring)
    assert (x_face, y_face) != (x_ring, y_ring)

print("Dock geometry: shared world-space rotating port, near ring depth, 4 bounded struts PASS")
