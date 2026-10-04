# Bakes BUGSTAR's dungeon textures. Each texel is one byte: (ramp slot << 5) | brightness 0..31.
# A texture also has a list of up to 8 ramps (palette base, +0x100 = glows on its own).
# Walls are 32 wide x 48 tall, stored column by column. Floors and ceilings are 32 x 32, row by row.
import numpy as np, math

E = 0x100     # emissive flag

def up(grid, h, w):
    """bilinear upsample of a small random grid to h x w"""
    gh, gw = grid.shape
    ys = np.linspace(0, gh - 1.001, h); xs = np.linspace(0, gw - 1.001, w)
    y0 = ys.astype(int); x0 = xs.astype(int); fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
    a = grid[y0][:, x0]; b = grid[y0][:, x0 + 1]; c = grid[y0 + 1][:, x0]; d = grid[y0 + 1][:, x0 + 1]
    return a * (1 - fx) * (1 - fy) + b * fx * (1 - fy) + c * (1 - fx) * fy + d * fx * fy

class Tex:
    def __init__(self, h, w, ramps, seed):
        self.h, self.w = h, w
        self.sel = np.zeros((h, w), int); self.b = np.zeros((h, w))
        self.ramps = list(ramps); self.r = np.random.default_rng(seed)
    def noise(self, cells, amp):
        g = self.r.random((self.h // cells + 2, self.w // cells + 2)) * 2 - 1
        return up(g, self.h, self.w) * amp
    def fine(self, amp):
        return (self.r.random((self.h, self.w)) * 2 - 1) * amp
    def encode(self, colmajor):
        b = np.clip(np.round(self.b), 0, 31).astype(int)
        t = (self.sel << 5) | b
        return (t.T.flatten() if colmajor else t.flatten()).tolist()

def pad(t, n):
    t.ramps = (t.ramps + [t.ramps[0]] * n)[:n]

def bevel_blocks(t, ids, light=3, dark=3):
    """raise every block: lighter top/left edge, darker bottom/right edge"""
    h, w = ids.shape
    for y in range(h):
        for x in range(w):
            k = ids[y, x]
            if k < 0: continue
            if y == 0 or ids[y - 1, x] != k or x == 0 or ids[y, x - 1] != k: t.b[y, x] += light
            if y == h - 1 or ids[y + 1, x] != k or x == w - 1 or ids[y, x + 1] != k: t.b[y, x] -= dark

# ---------------------------------------------------------------- wall bases
def brick(R, seed=1):
    t = Tex(48, 32, [R['WALL']], seed)
    ids = -np.ones((48, 32), int)
    for y in range(48):
        course = y // 6; off = 8 if course % 2 else 0
        for x in range(32):
            if y % 6 == 5 or (x - off) % 16 == 15: continue
            ids[y, x] = course * 4 + ((x - off) % 32) // 16
    tint = t.r.normal(0, 1.6, 64)
    t.b[:] = 21 + t.noise(4, 2.2) + t.fine(1.2)
    for y in range(48):
        for x in range(32):
            if ids[y, x] < 0: t.b[y, x] = 7 + t.r.random() * 2
            else: t.b[y, x] += tint[ids[y, x] % 64]
    bevel_blocks(t, ids, 3, 3)
    for k in range(5):                                      # a few cracked bricks
        cy = t.r.integers(1, 7) * 6 + 1; cx = t.r.integers(1, 30); d = 1 if t.r.random() < .5 else -1
        for i in range(4):
            yy, xx = cy + i, cx + d * (i // 2)
            if 0 <= yy < 48 and 0 <= xx < 32 and ids[yy, xx] >= 0: t.b[yy, xx] = 8
    t.b[44:, :] -= 4; t.b[:2, :] -= 3
    return t, ids

def stone(R, ramp='STONE', seed=2):
    t = Tex(48, 32, [R[ramp]], seed)
    ids = -np.ones((48, 32), int); k = 0
    for course in range(4):
        y0 = course * 12; x = -int(t.r.integers(0, 12)); cuts = []
        while x < 32: cuts.append(x); x += int(t.r.integers(10, 18))
        cuts.append(x)
        for i in range(len(cuts) - 1):
            for y in range(y0, y0 + 11):
                for xx in range(max(0, cuts[i]), min(32, cuts[i + 1] - 1)): ids[y, xx] = k
            k += 1
    tint = t.r.normal(0, 2.0, 64)
    t.b[:] = 20 + t.noise(6, 3) + t.noise(2, 1.2) + t.fine(1.0)
    for y in range(48):
        for x in range(32):
            if ids[y, x] < 0: t.b[y, x] = 6 + t.r.random() * 2
            else: t.b[y, x] += tint[ids[y, x] % 64]
    bevel_blocks(t, ids, 4, 4)
    for _ in range(14):                                      # chips
        y, x = t.r.integers(0, 48), t.r.integers(0, 32)
        if ids[y, x] >= 0: t.b[y, x] -= 6
    t.b[45:, :] -= 3; t.b[:2, :] -= 3
    return t, ids

def earth(R, seed=3):
    t = Tex(48, 32, [R['EARTH'], R['STONE']], seed)
    v = np.arange(48)[:, None] * np.ones((1, 32))
    strata = np.sin(v * 0.55 + t.noise(8, 2.5)) * 2.5
    t.b[:] = 17 + strata + t.noise(5, 2.5) + t.fine(1.5)
    for k in range(3):                                       # roots
        x0 = t.r.uniform(3, 29); ph = t.r.uniform(0, 6)
        for y in range(48):
            x = int(round(x0 + 2.2 * math.sin(y * 0.22 + ph)))
            if 0 <= x < 32: t.b[y, x] = 6
            if 0 <= x + 1 < 32: t.b[y, x + 1] = min(t.b[y, x + 1] + 4, 28)
    for _ in range(18):                                      # pebbles
        y, x = t.r.integers(0, 47), t.r.integers(0, 31)
        t.sel[y:y + 2, x:x + 2] = 1; t.b[y:y + 2, x:x + 2] = [[22, 18], [17, 12]]
    t.b[:3, :] -= 4
    return t

def moss(R, seed=4):
    t, ids = stone(R, 'STONE', seed + 10)
    t.ramps.append(R['MOSS'])
    drip = 5 + np.cumsum(t.r.integers(-2, 3, 32)).clip(-3, 12)
    for x in range(32):
        d = int(drip[x] + t.r.integers(0, 4))
        for y in range(0, min(48, d)):
            t.sel[y, x] = 1; t.b[y, x] = 17 + t.r.random() * 6 - (3 if y == d - 1 else 0)
        h = int(t.r.integers(0, 6))
        for y in range(48 - h, 48): t.sel[y, x] = 1; t.b[y, x] = 13 + t.r.random() * 5
    for _ in range(30):
        y, x = t.r.integers(10, 40), t.r.integers(0, 32)
        if ids[y, x] < 0: t.sel[y, x] = 1; t.b[y, x] = 15
    return t

def crystal(R, seed=5):
    t = Tex(48, 32, [R['STONE'], R['CRYSTAL'] | E], seed)
    t.b[:] = 9 + t.noise(5, 3) + t.fine(1.2)
    for k in range(4):                                       # glowing veins
        x, y = t.r.uniform(0, 32), t.r.uniform(0, 10); ang = t.r.uniform(0.9, 2.2)
        for i in range(60):
            xi, yi = int(x) % 32, int(y)
            if yi >= 48: break
            t.sel[yi, xi] = 1; t.b[yi, xi] = 26 + t.r.random() * 5
            if xi + 1 < 32 and t.sel[yi, xi + 1] == 0: t.b[yi, xi + 1] += 6
            x += math.cos(ang) * 0.8 + t.r.uniform(-.4, .4); y += math.sin(ang) * 0.8
            ang += t.r.uniform(-.25, .25)
    for k in range(3):                                       # shards growing from the floor
        cx = t.r.integers(3, 29); hgt = t.r.integers(8, 16); wd = t.r.integers(2, 4)
        for i in range(hgt):
            y = 47 - i; half = max(0, int(wd * (1 - i / hgt) + .5))
            for x in range(cx - half, cx + half + 1):
                if 0 <= x < 32: t.sel[y, x] = 1; t.b[y, x] = 16 + (x - cx + half) * 3 + i * 0.6
    return t

def pillar(R, ramp, seed=6, rough=False):
    t = Tex(48, 32, [R[ramp]], seed)
    u = (np.arange(32) + .5) / 32 - .5
    n = np.cos(u * math.pi * 0.95)
    t.b[:] = (5 + 22 * n)[None, :] + (t.noise(4, 2.5) if rough else t.noise(8, 1.0)) + t.fine(.8)
    for x in range(32):
        if x % 5 == 2 and 4 < x < 28: t.b[6:42, x] -= 4                 # fluting
    t.b[0:5, :] += 4; t.b[5, :] -= 7; t.b[6, :] -= 3                   # capital
    t.b[42, :] -= 7; t.b[43:, :] += 3                                  # base
    if not rough:
        for k in range(3):                                             # marble veins
            x = t.r.uniform(4, 28)
            for y in range(7, 41):
                x += t.r.uniform(-.8, .8); xi = int(x)
                if 0 <= xi < 32: t.b[y, xi] -= 3
    return t

def crystal_pillar(R, seed=7):
    t = Tex(48, 32, [R['STONE'], R['CRYSTAL'] | E], seed)
    t.b[:] = 8 + t.noise(4, 2)
    for x in range(32):
        for y in range(48):
            facet = ((x * 3 + y) // 9) % 4
            edge = abs(x - 16) < 13 - abs(y - 24) * 0.12
            if edge: t.sel[y, x] = 1; t.b[y, x] = 14 + facet * 4 + (6 if (x * 3 + y) % 9 == 0 else 0)
    return t

def beam(R, base, seed=8):
    t = base
    pad(t, 2); t.ramps += [R['WOOD']]
    for y in range(48):
        for x in list(range(0, 5)) + list(range(27, 32)):
            t.sel[y, x] = 2; t.b[y, x] = 17 + 3 * math.sin(y * 0.9 + x) + t.r.random() * 2
            if x in (0, 4, 27, 31): t.b[y, x] = 9
    for y in range(0, 7):
        for x in range(32):
            t.sel[y, x] = 2; t.b[y, x] = 20 + 2 * math.sin(x * 0.7 + y) + t.r.random() * 2
            if y in (0, 6): t.b[y, x] = 9
    for (x, y) in ((2, 3), (29, 3), (2, 20), (29, 20), (14, 3), (18, 3)): t.b[y, x] = 29
    return t

# ---------------------------------------------------------------- decorations on a wall
def torch(t, R):
    pad(t, 6); t.ramps += [R['STONE'], R['FLAME'] | E]
    s_iron, s_fire = 6, 7
    cx, cy = 15.5, 16.0
    for y in range(48):
        for x in range(32):
            d = math.hypot((x - cx) / 1.0, (y - cy) / 1.25)
            if d < 13 and t.sel[y, x] < 6: t.b[y, x] += 9 * (1 - d / 13)       # warm glow on the wall
    for y in range(2, 9):
        for x in range(14, 18): t.b[y, x] -= 4                                 # soot
    for y in range(22, 27):                                                    # iron cup
        w = 3 if y < 25 else 2
        for x in range(int(cx - w), int(cx + w) + 1): t.sel[y, x] = s_iron; t.b[y, x] = 12 if y == 22 else 7
    for y in range(27, 33): t.sel[y, 15] = s_iron; t.sel[y, 16] = s_iron; t.b[y, 15] = 9; t.b[y, 16] = 5
    for y in range(8, 23):                                                     # flame (palette-cycled)
        k = (y - 8) / 14.0; half = 0.6 + 3.2 * math.sin(k * math.pi * 0.85)
        for x in range(32):
            dx = abs(x - cx)
            if dx < half:
                t.sel[y, x] = s_fire; t.b[y, x] = 31 - dx * 3 - (1 - k) * 6
    return t

def niche(t, R, base_slot=0):
    pad(t, 3); t.ramps += [R['WOOD'], R['POTR'], R['POTY'], R['POTP'], R['STONE']]
    for y in range(13, 33):
        for x in range(6, 26):
            arch = y >= 16 or abs(x - 15.5) < 10 * math.sqrt(max(0, 1 - ((16 - y) / 3.5) ** 2))
            if not arch: continue
            frame = x in (6, 25) or y == 13 or (y < 16 and abs(x - 15.5) > 8.6 * math.sqrt(max(0, 1 - ((16 - y) / 3.5) ** 2)))
            t.sel[y, x] = base_slot
            t.b[y, x] = 26 if frame else 3 + (y - 13) * 0.15
    for x in range(7, 25):                                                     # shelf
        t.sel[30:33, x] = 7; t.b[30, x] = 24; t.b[31, x] = 15; t.b[32, x] = 7
    for slot, (x0, w, top) in zip((4, 5, 6), ((9, 4, 22), (14, 4, 19), (19, 4, 23))):
        for y in range(top, 30):
            for x in range(x0, x0 + w):
                neck = y < top + 3
                if neck and not (x0 + 1 <= x <= x0 + 2): continue
                t.sel[y, x] = slot
                t.b[y, x] = [27, 21, 15, 9][x - x0] + (2 if neck else 0)
        t.sel[top - 1, x0 + 1:x0 + 3] = 3; t.b[top - 1, x0 + 1:x0 + 3] = 20          # cork
        t.b[top + 4, x0] = 31                                                    # glint
    return t

def door(t, R):
    pad(t, 3); t.ramps += [R['WOOD'], R['STONE'], R['CHEST']]
    for y in range(14, 48):
        for x in range(5, 27):
            inside = 7 <= x <= 24 and y >= 16 and (y >= 19 or abs(x - 15.5) < 8.5 * math.sqrt(max(0, 1 - ((19 - y) / 3.2) ** 2)))
            if inside:
                t.sel[y, x] = 3; plank = (x - 7) % 5
                t.b[y, x] = 16 + 3 * math.sin(y * 0.5 + x * 2.1) + t.r.random() * 2 - (8 if plank == 0 else 0) + (2 if plank == 1 else 0)
            elif y >= 14:
                t.b[y, x] += 5                                                     # stone door frame
    for by in (24, 39):                                                        # iron bands with rivets
        for x in range(7, 25):
            t.sel[by:by + 2, x] = 4; t.b[by, x] = 11; t.b[by + 1, x] = 6
            if x % 4 == 1: t.b[by, x] = 22
    for i in range(18):                                                        # cross brace
        x = 8 + i; y = 26 + int(i * 12 / 18)
        if 7 <= x <= 24: t.b[y, x] += 6; t.b[y + 1, x] -= 4
    for (dx, dy) in ((0, -1), (1, 0), (0, 1), (-1, 0)):                         # ring handle
        t.sel[32 + dy, 21 + dx] = 5; t.b[32 + dy, 21 + dx] = 27
    return t

def rune(t, R):
    pad(t, 3); t.ramps += [R['CRYSTAL'] | E]
    t.b[25, :] += 5; t.b[32, :] -= 5
    for y in range(26, 32):
        t.b[y, :] = 5 + t.r.random(32) * 2
    glyphs = [[0b111, 0b101, 0b111, 0b100], [0b010, 0b111, 0b010, 0b010], [0b101, 0b010, 0b101, 0b010], [0b110, 0b011, 0b110, 0b011], [0b111, 0b001, 0b011, 0b001]]
    for k, x0 in enumerate(range(1, 30, 5)):
        g = glyphs[(k * 3 + int(t.r.integers(0, 5))) % 5]
        for gy in range(4):
            for gx in range(3):
                if g[gy] >> (2 - gx) & 1: t.sel[27 + gy, x0 + gx] = 3; t.b[27 + gy, x0 + gx] = 22
    return t

def plaque(t, R):
    pad(t, 3); t.ramps += [R['CHEST']]
    for y in range(6, 27):
        for x in range(8, 24):
            t.sel[y, x] = 3
            frame = x < 10 or x > 21 or y < 8 or y > 24
            if frame: t.b[y, x] = 26 if (x < 10 or y < 8) else 11
            else:
                nx, ny = (x - 15.5) / 5.0, (y - 16.5) / 6.5          # a bug king's face in relief
                if nx * nx + ny * ny < 1:
                    nz = math.sqrt(1 - nx * nx - ny * ny); t.b[y, x] = 10 + 16 * max(0, -0.5 * nx - 0.5 * ny + 0.7 * nz)
                else: t.b[y, x] = 9
    for (x, y) in ((13, 15), (18, 15)): t.b[y:y + 2, x:x + 2] = 3            # eyes
    t.b[20, 14:18] = 4                                                          # mouth
    for i in range(4): t.b[9 - i // 2, 12 - i] = 25; t.b[9 - i // 2, 19 + i] = 25   # antennae
    return t

def panel(t, R):
    for y in range(4, 44):
        for x in range(3, 29):
            fr = x < 6 or x > 25 or y < 7 or y > 40
            if fr: t.b[y, x] += 6 if (x < 5 or y < 6) else (-6 if (x > 26 or y > 41) else 2)
            else: t.b[y, x] -= 5
    for i in range(10):                                                         # carved diamond
        for (x, y) in ((15 - i, 14 + i), (16 + i, 14 + i), (15 - i, 34 - i), (16 + i, 34 - i)):
            if 0 <= x < 32: t.b[y, x] -= 6
    return t

# ---------------------------------------------------------------- floors and ceilings
def flag(R, ramp, seed):
    t = Tex(32, 32, [R[ramp]], seed)
    ids = -np.ones((32, 32), int); k = 0
    jx, jy = t.r.integers(13, 19), t.r.integers(13, 19)
    for y in range(32):
        for x in range(32):
            if y in (0, jy) or x in (0, jx): continue
            ids[y, x] = (y > jy) * 2 + (x > jx)
    tint = t.r.normal(0, 2.0, 4)
    t.b[:] = 19 + t.noise(5, 2.5) + t.fine(1.4)
    for y in range(32):
        for x in range(32):
            if ids[y, x] < 0: t.b[y, x] = 5
            else: t.b[y, x] += tint[ids[y, x]]
    bevel_blocks(t, ids, 3, 3)
    for _ in range(3):
        y, x = t.r.integers(2, 30), t.r.integers(2, 30); d = 1 if t.r.random() < .5 else -1
        for i in range(5):
            if 0 <= x + i * d < 32 and ids[min(31, y + i // 2), x + i * d] >= 0: t.b[min(31, y + i // 2), x + i * d] = 8
    return t

def tile(R, seed=20):
    t = Tex(32, 32, [R['TILEF'], R['TILEF']], seed)
    for y in range(32):
        for x in range(32):
            if y % 16 == 0 or x % 16 == 0: t.sel[y, x] = 1; t.b[y, x] = 7
            else: t.b[y, x] = 20 + (4 if ((x // 16 + y // 16) & 1) else 0) + (3 if (x % 16 + y % 16) in (10, 11) else 0)
    t.b += t.fine(0.8)
    return t

def dirt(R, seed=21):
    t = Tex(32, 32, [R['DIRT'], R['STONE']], seed)
    t.b[:] = 17 + t.noise(4, 3) + t.fine(2.0)
    for _ in range(10):
        y, x = t.r.integers(0, 31), t.r.integers(0, 31)
        t.sel[y:y + 2, x:x + 2] = 1; t.b[y:y + 2, x:x + 2] = [[22, 17], [16, 10]]
    return t

def mossfloor(R, seed=22):
    t = Tex(32, 32, [R['MOSS'], R['FLOORA']], seed)
    n = t.noise(5, 1)
    t.b[:] = 15 + t.noise(3, 3) + t.fine(1.5)
    t.sel[n > 0.35] = 1; t.b[n > 0.35] = 18 + t.fine(1.5)[n > 0.35]
    return t

def water(R, seed=23):
    t = Tex(32, 32, [R['WATER']], seed)
    y, x = np.mgrid[0:32, 0:32] * (2 * math.pi / 32)
    c = np.sin(x * 2 + np.sin(y * 3)) + np.sin(y * 2 + np.sin(x * 2) * 1.5)
    t.b[:] = 13 + 5 * c + t.fine(1.0)
    return t

def plank(R, seed=24):
    t = Tex(32, 32, [R['WOOD'], R['STONE']], seed)
    for y in range(32):
        for x in range(32):
            row = y // 8; off = (row * 11) % 32
            t.b[y, x] = 17 + 3 * math.sin(x * 0.45 + row * 2 + math.sin(y * 1.3)) + t.r.random() * 1.5 + (row % 2) * 2
            if y % 8 == 7: t.b[y, x] = 5
            elif (x - off) % 32 == 0: t.b[y, x] = 6
    for row in range(4):
        for x in ((row * 11 + 3) % 32, (row * 11 + 28) % 32):
            t.sel[row * 8 + 3, x] = 1; t.b[row * 8 + 3, x] = 27
    return t

def stairs(R, up, seed):
    t = flag(R, 'FLOORA', seed)
    t.ramps = [R['FLOORA'], R['UP' if up else 'DOWN'] | E, R['DOWN']]
    for y in range(32):
        for x in range(32):
            d = math.hypot(x - 15.5, y - 15.5)
            if 10.5 < d < 13: t.sel[y, x] = 1; t.b[y, x] = 30 - abs(d - 11.75) * 6
            elif d <= 10.5:
                if up:
                    t.sel[y, x] = 1; t.b[y, x] = 10 + (8 if (abs(x - 15.5) < 2 and 7 < y < 24) or (y - 7 > abs(x - 15.5) * 1.2 and y < 13) else 0)
                else:
                    t.sel[y, x] = 2; t.b[y, x] = max(0, 2 + (d - 6) * 1.5)
    return t

def ceil_stone(R, seed=30):
    t = Tex(32, 32, [R['CEIL']], seed)
    ids = -np.ones((32, 32), int)
    for y in range(32):
        for x in range(32):
            if y in (0, 16) or x == (0 if y < 16 else 16): continue
            ids[y, x] = (y >= 16) * 2 + ((x - (0 if y < 16 else 16)) % 32 >= 16)
    t.b[:] = 18 + t.noise(5, 3) + t.fine(1.2)
    t.b[ids < 0] = 5
    bevel_blocks(t, ids, 2, 3)
    return t

def ceil_earth(R, seed=31):
    t = Tex(32, 32, [R['EARTH'], R['WOOD']], seed)
    t.b[:] = 13 + t.noise(4, 3) + t.fine(1.5)
    for k in range(4):
        x0 = t.r.integers(0, 32); ph = t.r.uniform(0, 6)
        for y in range(32):
            x = int(x0 + 1.5 * math.sin(y * .3 + ph)) % 32; t.b[y, x] = 5
    return t

# ---------------------------------------------------------------- everything, in the order the game uses
WALLS = ['BRICK', 'STONE', 'EARTH', 'MOSS', 'CRYSTAL', 'MARBLE', 'EARTH_BEAM', 'BRICK_TORCH', 'STONE_TORCH',
         'BRICK_NICHE', 'BRICK_DOOR', 'BRICK_RUNE', 'BRICK_PLAQUE', 'STONE_NICHE', 'STONE_DOOR', 'STONE_RUNE',
         'STONE_PLAQUE', 'STONE_PANEL', 'STONE_PILLAR', 'CRYSTAL_PILLAR']
FLOORS = ['FLAG_A', 'FLAG_B', 'TILE', 'DIRT', 'MOSS', 'WATER', 'PLANK', 'UP', 'DOWN']
CEILS = ['CEIL_STONE', 'CEIL_EARTH']

def make_all(R):
    def wall(name):
        if name == 'BRICK': return brick(R)[0]
        if name == 'STONE': return stone(R)[0]
        if name == 'EARTH': return earth(R)
        if name == 'MOSS': return moss(R)
        if name == 'CRYSTAL': return crystal(R)
        if name == 'MARBLE': return pillar(R, 'PILLAR')
        if name == 'STONE_PILLAR': return pillar(R, 'STONE', 9, rough=True)
        if name == 'CRYSTAL_PILLAR': return crystal_pillar(R)
        if name == 'EARTH_BEAM': return beam(R, earth(R, 13))
        base, deco = name.split('_')
        t = brick(R, 41)[0] if base == 'BRICK' else stone(R, 'STONE', 42)[0]
        return {'TORCH': torch, 'NICHE': niche, 'DOOR': door, 'RUNE': rune, 'PLAQUE': plaque, 'PANEL': panel}[deco](t, R)
    def floor(name):
        return {'FLAG_A': lambda: flag(R, 'FLOORA', 10), 'FLAG_B': lambda: flag(R, 'FLOORB', 11), 'TILE': lambda: tile(R),
                'DIRT': lambda: dirt(R), 'MOSS': lambda: mossfloor(R), 'WATER': lambda: water(R), 'PLANK': lambda: plank(R),
                'UP': lambda: stairs(R, True, 12), 'DOWN': lambda: stairs(R, False, 13)}[name]()
    out = {}
    for group, names, fn, colmajor in (('wall', WALLS, wall, True), ('floor', FLOORS, floor, False), ('ceil', CEILS, lambda n: ceil_stone(R) if n == 'CEIL_STONE' else ceil_earth(R), False)):
        texels, ramps = [], []
        for n in names:
            t = fn(n); assert len(t.ramps) <= 8, n
            texels += t.encode(colmajor); ramps += (t.ramps + [t.ramps[0]] * 8)[:8]
        out[group] = (texels, ramps)
    return out
