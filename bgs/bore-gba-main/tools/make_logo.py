#!/usr/bin/env python3
"""BORE title logo generator.

    python3 tools/make_logo.py        -> source/logo.h  +  assets/preview/title_logo.png

Chunky bubble letters (blurred + re-thresholded so every corner is round), lit from the top left with a height map,
black outline + drop shadow, a bloodshot half-lidded eye on each letter, a cannabis leaf in the O and a lit joint
on the E. Output is a 16 colour indexed picture (index 0 = transparent) that main.c blits over the title photo.
"""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from scipy import ndimage as ndi

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TTF = "/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf"
W, H = 140, 56          # logo size in game pixels
S = 8                   # supersampling

# ---------------------------------------------------------------- letter mask (hi-res)
def letter_mask():
    big = Image.new("L", (W * S, H * S), 0)
    d = ImageDraw.Draw(big)
    f = ImageFont.truetype(TTF, 45 * S)
    x = 3 * S
    pos = []
    for ch in "BORE":
        bb = f.getbbox(ch)
        d.text((x - bb[0], 8 * S - bb[1]), ch, font=f, fill=255, stroke_width=int(0.5 * S), stroke_fill=255)
        pos.append((x, x + (bb[2] - bb[0])))
        x += (bb[2] - bb[0]) + int(2.2 * S)
    m = np.asarray(big, dtype=np.float32) / 255.0
    m = ndi.gaussian_filter(m, 0.9 * S)           # bubble it: blur ...
    m = m > 0.5                                   # ... and re-threshold -> soft round blobs
    return m, pos

mask_hi, spans = letter_mask()
# the O's counter gets a leaf, so keep it open; fill nothing else
# ---------------------------------------------------------------- shading
inside = ndi.distance_transform_edt(mask_hi)                       # depth into the letter
hgt = np.minimum(inside / (4.0 * S), 1.0) ** 0.6                   # 0 at edge .. 1 in the middle (puffy)
gy, gx = np.gradient(ndi.gaussian_filter(hgt, 1.0 * S))
nz = 1.0 / np.sqrt(1 + (gx * 9 * S) ** 2 + (gy * 9 * S) ** 2)
nx, ny = -gx * 9 * S * nz, -gy * 9 * S * nz
light = np.array([-0.55, -0.65, 0.52]); light /= np.linalg.norm(light)
lam = np.clip(nx * light[0] + ny * light[1] + nz * light[2], 0, 1)
hv = np.array([-0.55, -0.65, 0.52]); hv = hv / np.linalg.norm(hv)
spec = np.clip(lam, 0, 1) ** 14                                     # glossy highlight
ramp = [  # dark -> light, buttery yellow with orange shadows and a green-ish bounce (weed)
    (120, 60, 8), (196, 112, 12), (246, 170, 20), (255, 212, 40), (255, 238, 96)]
def ramp_col(t):
    t = np.clip(t, 0, 0.9999) * (len(ramp) - 1)
    i = t.astype(int); f = (t - i)[..., None]
    a = np.array(ramp)[i]; b = np.array(ramp)[np.minimum(i + 1, len(ramp) - 1)]
    return a * (1 - f) + b * f
tone = 0.18 + 0.82 * lam
col = ramp_col(tone)
col = col + spec[..., None] * np.array([255, 255, 255]) * 0.85     # specular
# rim bounce light from below-right (a hint of leaf green)
rim = np.clip(-(nx * light[0] + ny * light[1]), 0, 1) ** 2 * (1 - hgt) * 0.5
col = col * (1 - rim[..., None]) + rim[..., None] * np.array([120, 170, 40])
col = np.clip(col, 0, 255)

img_hi = np.zeros((H * S, W * S, 4), np.float32)
# outline + shadow in hi-res
outline = ndi.binary_dilation(mask_hi, iterations=int(1.5 * S))
shadow = ndi.shift(outline.astype(np.float32), (2.2 * S, 2.2 * S), order=0) > 0.5
img_hi[shadow] = (20, 8, 24, 255)
img_hi[outline] = (14, 6, 14, 255)
img_hi[mask_hi, :3] = col[mask_hi]
img_hi[mask_hi, 3] = 255

# ---------------------------------------------------------------- decorations (hi-res, PIL)
pil = Image.fromarray(img_hi.astype(np.uint8), "RGBA")
d = ImageDraw.Draw(pil)
def P(x, y): return (x * S, y * S)

# bloodshot half-lidded eye on each letter (placed on the stem / bowl)
def eye(cx, cy, r=3.0, h=2.4):
    """a droopy bloodshot eye of half-width r, half-height h, centred on cx,cy"""
    d.ellipse([P(cx - r - .5, cy - h - .5), P(cx + r + .5, cy + h + .5)], fill=(14, 6, 14, 255))          # outline
    d.ellipse([P(cx - r, cy - h), P(cx + r, cy + h)], fill=(255, 226, 226, 255))                          # sclera
    for vx, vy in ((-2.3, .7), (-1.7, 1.5), (2.3, .8), (1.7, 1.5)):                                       # bloodshot veins
        d.line([P(cx + vx, cy + vy), P(cx + vx * .5, cy + vy * .3)], fill=(224, 40, 50, 255), width=int(.4 * S))
    d.ellipse([P(cx - 1.2, cy - .3), P(cx + 1.2, cy + 2.0)], fill=(60, 150, 40, 255))                     # iris
    d.ellipse([P(cx - .6, cy + .2), P(cx + .6, cy + 1.6)], fill=(14, 6, 14, 255))                         # pupil
    m = Image.new("L", pil.size, 0); md = ImageDraw.Draw(m)                                                # heavy lid over the top half
    md.ellipse([P(cx - r - .2, cy - h - .2), P(cx + r + .2, cy + h + .2)], fill=255)
    md.rectangle([P(cx - r - 2, cy + .1), P(cx + r + 2, cy + h + 2)], fill=0)
    pil.paste((236, 140, 16, 255), (0, 0), m)
    d.line([P(cx - r - .1, cy + .1), P(cx + r + .1, cy + .1)], fill=(14, 6, 14, 255), width=int(.6 * S))  # lid line

# cannabis leaf (7 leaflets) in the counter of the O
def leaf(cx, cy, s):
    green = (46, 170, 52, 255); dark = (18, 90, 30, 255)
    import math
    for ang, ln, wd in ((-90, 1.0, .20), (-52, .78, .17), (-128, .78, .17), (-18, .55, .14), (-162, .55, .14), (14, .36, .11), (-194, .36, .11)):
        a = math.radians(ang)
        tx, ty = cx + math.cos(a) * ln * s, cy + math.sin(a) * ln * s
        px_, py_ = -math.sin(a), math.cos(a)
        mx, my = cx + math.cos(a) * ln * s * .5, cy + math.sin(a) * ln * s * .5
        w = wd * s
        poly = [P(cx, cy), P(mx + px_ * w, my + py_ * w), P(tx, ty), P(mx - px_ * w, my - py_ * w)]
        d.polygon(poly, fill=green, outline=dark)
        d.line([P(cx, cy), P(tx, ty)], fill=(150, 235, 120, 255), width=int(.28 * S))
    d.line([P(cx, cy), P(cx, cy + s * .55)], fill=dark, width=int(.7 * S))

# lit joint
def joint(x0, y0, x1, y1):
    import math
    dx, dy = x1 - x0, y1 - y0; L = math.hypot(dx, dy); ux, uy = dx / L, dy / L; nx_, ny_ = -uy, ux
    r = 2.0
    def quad(a, b, rr=r):
        return [P(a[0] + nx_ * rr, a[1] + ny_ * rr), P(b[0] + nx_ * rr, b[1] + ny_ * rr), P(b[0] - nx_ * rr, b[1] - ny_ * rr), P(a[0] - nx_ * rr, a[1] - ny_ * rr)]
    o = r + 1.0
    d.polygon(quad((x0 - ux * 1.0, y0 - uy * 1.0), (x1 + ux * 1.4, y1 + uy * 1.4), o), fill=(14, 6, 14, 255))          # outline
    mid = (x0 + dx * .80, y0 + dy * .80)
    d.polygon(quad((x0, y0), mid), fill=(226, 230, 238, 255))                 # paper
    d.line([P(x0 + nx_ * r * .5, y0 + ny_ * r * .5), P(mid[0] + nx_ * r * .5, mid[1] + ny_ * r * .5)], fill=(255, 255, 255, 255), width=int(.55 * S))
    d.line([P(x0 - nx_ * r * .55, y0 - ny_ * r * .55), P(mid[0] - nx_ * r * .55, mid[1] - ny_ * r * .55)], fill=(198, 112, 12, 255), width=int(.55 * S))
    d.polygon(quad(mid, (x1, y1)), fill=(198, 112, 12, 255))                  # burnt end
    ex, ey = x1 + ux * .9, y1 + uy * .9                                       # ember
    d.ellipse([P(ex - 2.3, ey - 2.3), P(ex + 2.3, ey + 2.3)], fill=(14, 6, 14, 255))
    d.ellipse([P(ex - 1.8, ey - 1.8), P(ex + 1.8, ey + 1.8)], fill=(255, 90, 20, 255))
    d.ellipse([P(ex - 1.0, ey - 1.0), P(ex + 1.0, ey + 1.0)], fill=(255, 242, 120, 255))

# find the O's counter (the hole) so the leaf sits dead centre in it
filled = ndi.binary_fill_holes(mask_hi)
hole = filled & ~mask_hi
ys, xs = np.nonzero(hole[:, spans[1][0]:spans[1][1] + 4 * S])
ccx = (xs.mean() + spans[1][0]) / S; ccy = ys.mean() / S
b0, b1 = spans[0]; ox0, ox1 = spans[1]; r0, r1 = spans[2]; e0, e1 = spans[3]
# eyes peek out of the upper hole of the B and the hole of the R
lab, nh = ndi.label(hole)
def hole_centre(x0, x1, ymax):
    best = None
    for i in range(1, nh + 1):
        ys, xs = np.nonzero(lab == i)
        cx, cy = xs.mean() / S, ys.mean() / S
        if x0 <= cx * S <= x1 and (best is None or cy < best[1]) and cy < ymax:
            best = (cx, cy)
    return best
bx, by = hole_centre(b0, b1, 99)       # B: topmost hole
rx, ry = hole_centre(r0, r1, 99)       # R: its one hole
eye(bx, by, 3.0, 2.4)
eye(rx, ry, 3.1, 2.6)
leaf(ccx, ccy + 3.2, 7.6)
# lit joint poking out of the top arm of the E, ember up-right, smoke wisps above it
joint(e0 / S + 13.0, 13.5, e1 / S + 5.5, 3.2)
for (sx, sy, sr, sa) in ((e1 / S + 7.5, 1.5, 1.3, 255), (e1 / S + 5.8, -.6, 1.0, 255)):
    if sy - sr > 0: d.ellipse([P(sx - sr, sy - sr), P(sx + sr, sy + sr)], fill=(226, 230, 238, sa))

# ---------------------------------------------------------------- map to a hand-picked 15 colour palette
PAL = np.array([
    (14, 6, 14), (58, 26, 60),                                   # 1 outline  2 drop shadow
    (122, 62, 8), (198, 112, 12), (246, 170, 20), (255, 212, 40), (255, 242, 120), (255, 255, 255),   # yellow ramp, white
    (255, 226, 226), (224, 40, 50),                              # sclera, vein red
    (60, 150, 40), (150, 235, 120), (18, 90, 30),                # leaf green, leaf light, leaf dark
    (255, 90, 20), (226, 230, 238)], np.float32)                 # ember, smoke / joint paper
final = pil.resize((W, H), Image.BOX)
rgba = np.asarray(final).astype(np.float32)
opaque = rgba[..., 3] > 127
dist = ((rgba[..., None, :3] - PAL[None, None]) ** 2 * np.array([0.9, 1.2, 0.7])).sum(-1)
idx = dist.argmin(-1).astype(np.int32) + 1
idx[~opaque] = 0
pal5 = (PAL.astype(int) * 31 + 127) // 255
pal8 = (pal5 * 255 + 15) // 31

# ---------------------------------------------------------------- write logo.h
def hexc(i): return "0123456789abcdef"[i]
rows = ["".join(hexc(int(v)) for v in r) for r in idx]
with open(os.path.join(ROOT, "source/logo.h"), "w") as f:
    f.write("// GENERATED by tools/make_logo.py - the BORE title logo. %dx%d, one hex digit per pixel (0 = transparent, 1..f = logoPal[i-1]).\n" % (W, H))
    f.write("#define LOGO_W %d\n#define LOGO_H %d\n" % (W, H))
    f.write("static const u16 logoPal[15]={ " + ",".join("RGB(%d,%d,%d)" % tuple(c) for c in pal5) + " };\n")
    f.write("static const char* const logoArt[LOGO_H]={\n" + ",\n".join('"%s"' % r for r in rows) + "\n};\n")

# ---------------------------------------------------------------- preview (over a dark photo-ish backdrop, 4x)
bg = Image.new("RGB", (W, H), (170, 90, 60))
bgd = ImageDraw.Draw(bg)
for y in range(H): bgd.line([(0, y), (W, y)], fill=(190 - y, 100 - y // 2, 70))
prev = bg.copy()
pa = np.zeros((H, W, 3), np.uint8)
for y in range(H):
    for x in range(W):
        if idx[y, x]: pa[y, x] = pal8[idx[y, x] - 1]
mask_px = (idx > 0)
parr = np.asarray(prev).copy(); parr[mask_px] = pa[mask_px]
os.makedirs(os.path.join(ROOT, "assets/preview"), exist_ok=True)
Image.fromarray(parr).resize((W * 5, H * 5), Image.NEAREST).save(os.path.join(ROOT, "assets/preview/title_logo.png"))
print("ok", W, H, "colours", len(set(idx.flatten())))
