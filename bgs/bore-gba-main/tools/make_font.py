#!/usr/bin/env python3
"""BORE font tool.  The font lives in assets/font/ as an editable asset:

    assets/font/bore_font.png    atlas: one strip per size, 8-bit grey = pixel coverage (0 = clear, 255 = solid)
    assets/font/bore_font.json   glyph table: where each glyph sits in the atlas, its advance, strip metrics
    assets/font/bore_font_preview.png   what the game text looks like (3x, on the game's panel colour)

    python3 tools/make_font.py             atlas -> source/fontdata.h   (what the build needs; no extra libraries but Pillow)
    python3 tools/make_font.py --regen     re-rasterise the atlas from a TrueType font first, then build the header
    python3 tools/make_font.py --font X.ttf --regen   use another TTF

You can also paint the atlas by hand (any editor that keeps 8-bit grey) and just run the tool with no flags.
The game blends each glyph pixel over whatever is already on screen using 9 coverage steps (0..8), so smooth
curves and diagonals are possible, and the text keeps its look on every background colour.
"""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ATLAS = os.path.join(ROOT, "assets/font/bore_font.png")
META = os.path.join(ROOT, "assets/font/bore_font.json")
PREVIEW = os.path.join(ROOT, "assets/font/bore_font_preview.png")
HEADER = os.path.join(ROOT, "source/fontdata.h")
DEFAULT_TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf"

CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!+-.,:;'?/()=>%"
# size name -> (cap height in px, gap between letters, width of a space, rows below the baseline)
SIZES = {
    "s": dict(cap=5,  gap=1, space=3, below=2, scale="text(...,1)  normal UI text"),
    "m": dict(cap=10, gap=2, space=5, below=3, scale="text(...,2)  headings and big menu"),
    "l": dict(cap=25, gap=3, space=9, below=7, scale="text(...,3+) the BORE logo"),
}
LEVELS = 8   # coverage steps used by the game's blender (0..8)

def fit_px(ttf, cap):
    """pick the TrueType pixel size whose hinted capital H is `cap` rows tall"""
    best = None
    s = 4.0
    while s < 80:
        f = ImageFont.truetype(ttf, s)
        bb = f.getbbox("H")
        if bb[3] - bb[1] == cap:
            best = s if best is None else best
            if s > best + 0.01 and bb[3] - bb[1] == cap: best = best  # keep the smallest that hits
        elif best is not None:
            break
        s = round(s + 0.25, 2)
    return best

def raster_glyph(font, ch, cap, below, ss=1):
    h = cap + below
    im = Image.new("L", (cap * 3 + 16, h), 0)
    ImageDraw.Draw(im).text((4, cap), ch, font=font, fill=255, anchor="ls")
    bb = im.getbbox()
    if not bb: return Image.new("L", (1, h), 0)
    return im.crop((bb[0], 0, bb[2], h))

def arrow(cap, h):
    """the menu cursor: a filled right-pointing triangle, drawn 8x then shrunk so the slanted edges are smooth"""
    w = max(3, round(cap * 0.62)); S = 8
    big = Image.new("L", (w * S, h * S), 0)
    top, bot = 0.0, cap * S
    ImageDraw.Draw(big).polygon([(0, top), (w * S, (top + bot) / 2), (0, bot)], fill=255)
    return big.resize((w, h), Image.BOX)

def quant(im):
    """snap coverage to the 9 steps the game uses"""
    return im.point(lambda v: round(round(v * LEVELS / 255) * 255 / LEVELS))

def regen(ttf):
    glyphs = {}
    strips = {}
    for name, c in SIZES.items():
        px = fit_px(ttf, c["cap"])
        if px is None: sys.exit("no pixel size of %s gives a capital height of %d" % (ttf, c["cap"]))
        font = ImageFont.truetype(ttf, px)
        gl = {}
        for ch in CHARS:
            gl[ch] = arrow(c["cap"], c["cap"] + c["below"]) if ch == ">" else raster_glyph(font, ch, c["cap"], c["below"])
        # digits share one width so counters (FPS, LOAD, score) do not jiggle
        dw = max(gl[d].width for d in "0123456789")
        for d in "0123456789":
            g = gl[d]; cell = Image.new("L", (dw, g.height), 0)
            cell.paste(g, ((dw - g.width) // 2, 0)); gl[d] = cell
        strips[name] = (px, {ch: quant(g) for ch, g in gl.items()})
    # pack into the atlas: one strip per size, 2px gutters (kept as black), strips stacked with a 2px gap
    meta = {"chars": CHARS, "levels": LEVELS, "sizes": {}}
    width = max(sum(g.width + 2 for g in s[1].values()) for s in strips.values()) + 2
    height = sum(SIZES[n]["cap"] + SIZES[n]["below"] + 2 for n in SIZES) + 2
    atlas = Image.new("L", (width, height), 0)
    y = 2
    for name, (px, gl) in strips.items():
        c = SIZES[name]; h = c["cap"] + c["below"]; x = 2; table = {}
        for ch in CHARS:
            g = gl[ch]; atlas.paste(g, (x, y)); table[ch] = {"x": x, "w": g.width}; x += g.width + 2
        meta["sizes"][name] = {"y": y, "h": h, "cap": c["cap"], "gap": c["gap"], "space": c["space"],
                               "ttf_px": px, "used_by": c["scale"], "glyphs": table}
        y += h + 2
    atlas.save(ATLAS, optimize=True)
    json.dump(meta, open(META, "w"), indent=1)
    print("atlas:", ATLAS, atlas.size)

def build():
    meta = json.load(open(META)); atlas = Image.open(ATLAS).convert("L")
    chars = meta["chars"]; out = []
    out.append("// GENERATED by tools/make_font.py from assets/font/bore_font.png - edit the atlas or the tool, not this file.\n")
    out.append("// Each glyph pixel is a coverage step 0..8 (0 clear, 8 solid); the game blends it over the screen.\n")
    out.append("#define FNT_CHARS \"%s\"\n" % chars.replace("\\", "\\\\").replace('"', '\\"'))
    for name, s in meta["sizes"].items():
        data = []; offs = []; ws = []; adv = []
        for ch in chars:
            g = s["glyphs"][ch]
            offs.append(len(data)); ws.append(g["w"]); adv.append(g["w"] + s["gap"])
            for yy in range(s["h"]):
                for xx in range(g["w"]):
                    v = atlas.getpixel((g["x"] + xx, s["y"] + yy))
                    data.append(max(0, min(LEVELS, round(v * LEVELS / 255))))
        def arr(t, n, a, per=24):
            body = ",".join(str(v) for v in a)
            lines = [body[i:i+120] for i in range(0, len(body), 120)] if False else None
            txt = ""; row = []
            for i, v in enumerate(a):
                row.append(str(v))
                if len(row) == per: txt += ",".join(row) + ",\n"; row = []
            if row: txt += ",".join(row) + "\n"
            else: txt = txt.rstrip(",\n") + "\n"
            return "static const %s %s[%d]={\n%s};\n" % (t, n, len(a), txt)
        out.append("// size '%s': cap %d, cell height %d, letter gap %d (%s)\n" % (name, s["cap"], s["h"], s["gap"], s["used_by"]))
        out.append(arr("u32", "fo_" + name, offs, 12)); out.append(arr("u8", "fw_" + name, ws)); out.append(arr("u8", "fa_" + name, adv))
        out.append(arr("u8", "fp_" + name, data, 40))
        out.append("#define FH_%s %d\n#define FSP_%s %d\n" % (name, s["h"], name, s["space"]))
    open(HEADER, "w").write("".join(out)); print("header:", HEADER, os.path.getsize(HEADER), "bytes")
    preview(meta, atlas)

def draw_run(canvas, atlas, meta, size, text, x, y, col, bg=None):
    s = meta["sizes"][size]
    for ch in text:
        if ch not in s["glyphs"]: x += s["space"]; continue
        g = s["glyphs"][ch]
        for yy in range(s["h"]):
            for xx in range(g["w"]):
                a = round(atlas.getpixel((g["x"] + xx, s["y"] + yy)) * LEVELS / 255)
                if a:
                    px, py = x + xx, y + yy
                    if 0 <= px < canvas.width and 0 <= py < canvas.height:
                        b = canvas.getpixel((px, py))
                        canvas.putpixel((px, py), tuple((b[i] * (LEVELS - a) + col[i] * a) // LEVELS for i in range(3)))
        x += g["w"] + s["gap"]
    return x

def preview(meta, atlas):
    bg = (20, 26, 60); fg = (250, 230, 120)
    c = Image.new("RGB", (240, 160), bg)
    y = 4
    for line in ["ABCDEFGHIJKLM", "NOPQRSTUVWXYZ", "0123456789 +-!.,:;'?/()=%", "DRAW COST TOO SLOW FOR THIS RATE", "SEL+LEFT RIGHT TURN VIEW"]:
        draw_run(c, atlas, meta, "s", line, 4, y, fg); y += 9
    draw_run(c, atlas, meta, "m", "SETTINGS", 4, y + 4, fg)
    draw_run(c, atlas, meta, "m", "> MAKE CREATURE", 84, y + 4, (255, 255, 255))
    c.resize((960, 640), Image.NEAREST).save(PREVIEW)
    print("preview:", PREVIEW)

if __name__ == "__main__":
    a = sys.argv[1:]
    ttf = DEFAULT_TTF
    if "--font" in a: ttf = a[a.index("--font") + 1]
    if "--regen" in a: regen(ttf)
    build()
