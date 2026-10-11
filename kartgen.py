# Makes the kart art for BUGSTAR (imported by gen.py).
# Every kart body is a little 3D model made of simple shapes (boxes, rounded boxes, balls, cylinders), drawn from
# behind and from the side into 32x16 sprites, shaded, then outlined. Spoilers and exhausts are small stamps the game
# lays over a body at run time; toppers are their own 16x8 sprites. Palette (each racer has its own sprite bank):
#  1 outline  2/3/4 body dark/mid/light  5 trim  6 tyre  7 hub  8 metal  9 lamp  a glass  b white  c pattern
#  d dark inside  e flame  f trim light
import math
W, H = 32, 16
BODY, TRIM, TYRE, HUB, METAL, LAMP, GLASS, WHITE, DARK, FLAME, WIRE, KEYS = range(12)

def shade_ix(mat, s, u, v):
    # s: 0 dark, 1 mid, 2 light
    if mat == BODY:  return (2, 3, 4)[s]
    if mat == TRIM:  return (5, 5, 15)[s]
    if mat == TYRE:  return 6
    if mat == HUB:   return 7
    if mat in (METAL, WIRE): return (13, 8, 11)[s]
    if mat == LAMP:  return 9
    if mat == GLASS: return (10, 10, 11)[s]
    if mat == WHITE: return (8, 11, 11)[s]
    if mat == DARK:  return 13
    if mat == FLAME: return (9, 14, 14)[s]
    if mat == KEYS:  return 13 if (u % 2 == 0 and v % 3 != 2) else 11
    return 3

class Model:
    def __init__(s): s.p = []
    def box(s, x0, x1, y0, y1, z0, z1, m): s.p.append(('box', (x0, x1, y0, y1, z0, z1), m)); return s
    def rbox(s, cx, cy, cz, rx, ry, rz, m, k=4): s.p.append(('rbox', (cx, cy, cz, rx, ry, rz, k), m)); return s
    def ell(s, cx, cy, cz, rx, ry, rz, m): s.p.append(('ell', (cx, cy, cz, rx, ry, rz), m)); return s
    def cylx(s, cy, cz, r, x0, x1, m, r1=None): s.p.append(('cylx', (cy, cz, r, x0, x1, r if r1 is None else r1), m)); return s
    def cylz(s, cx, cy, r, z0, z1, m, r1=None): s.p.append(('cylz', (cx, cy, r, z0, z1, r if r1 is None else r1), m)); return s
    def wheel(s, cx, cz, r, y0, y1): s.p.append(('wheel', (cx, cz, r, y0, y1), TYRE)); return s
    def wheels(s, xr, xf, rr, rf, w0=8, w1=12):
        for y0, y1 in ((-w1, -w0), (w0, w1)):
            s.wheel(xr, rr, rr, y0, y1); s.wheel(xf, rf, rf, y0, y1)
        return s

# a ray through screen point (a, b) of a view: side view a = x (front on the right), b = z, depth = -y (the kart's left side faces you)
# rear view a = y (the kart's right on the right), b = z, depth = -x (its tail faces you)
L = (-0.45, 0.75, 0.5)
def lit(nx, ny, nz):
    d = nx*L[0] + ny*L[1] + nz*L[2]
    return 2 if d > 0.55 else 1 if d > -0.05 else 0

def hit(prim, view, a, b):
    kind, q, m = prim
    if kind == 'box':
        x0, x1, y0, y1, z0, z1 = q
        if not (z0 <= b <= z1): return None
        if view == 'side':
            if not (x0 <= a <= x1): return None
            depth = -y0; u0, u1 = x0, x1
        else:
            if not (y0 <= a <= y1): return None
            depth = -x0; u0, u1 = y0, y1
        h = z1 - z0; t = (b - z0) / h if h > 0 else 0.5
        s = 2 if t > 0.78 else 0 if t < 0.2 else 1
        return depth, s
    if kind == 'rbox':
        cx, cy, cz, rx, ry, rz, k = q
        if view == 'side': du, dd, ru, rd = (a - cx) / rx, None, rx, ry
        else: du, ru, rd = (a - cy) / ry, ry, rx
        dv = (b - cz) / rz
        r = abs(du)**k + abs(dv)**k
        if r > 1: return None
        dep = (1 - r) ** (1.0 / k)
        depth = (cy if view == 'side' else cx) * -1 + dep * rd
        nu = math.copysign(abs(du)**(k-1), du); nv = math.copysign(abs(dv)**(k-1), dv); nd = dep**(k-1)
        n = math.sqrt(nu*nu + nv*nv + nd*nd) or 1
        return depth, lit(nu/n, nv/n, nd/n)
    if kind == 'ell':
        cx, cy, cz, rx, ry, rz = q
        if view == 'side': du, ce, rd = (a - cx) / rx, cy, ry
        else: du, ce, rd = (a - cy) / ry, cx, rx
        dv = (b - cz) / rz
        r = du*du + dv*dv
        if r > 1: return None
        dep = math.sqrt(1 - r)
        return -ce + dep * rd, lit(du, dv, dep)
    if kind == 'cylx':
        cy, cz, r0, x0, x1, r1 = q
        if view == 'side':
            if not (x0 <= a <= x1): return None
            t = (a - x0) / (x1 - x0) if x1 > x0 else 0
            r = r0 + (r1 - r0) * t
            if r <= 0 or abs(b - cz) > r: return None
            dv = (b - cz) / r; dep = math.sqrt(max(0, 1 - dv*dv))
            return -cy + dep * r, lit(0, dv, dep)
        else:
            r = max(r0, r1); du = (a - cy) / r; dv = (b - cz) / r
            if du*du + dv*dv > 1: return None
            return -x0, lit(du, dv, math.sqrt(max(0, 1 - du*du - dv*dv)))
    if kind == 'cylz':
        cx, cy, r0, z0, z1, r1 = q
        if not (z0 <= b <= z1): return None
        t = (b - z0) / (z1 - z0) if z1 > z0 else 0
        r = r0 + (r1 - r0) * t
        c = cx if view == 'side' else cy; ce = cy if view == 'side' else cx
        du = (a - c) / r
        if abs(du) > 1: return None
        dep = math.sqrt(1 - du*du)
        return -ce + dep * r, lit(du, 0.15, dep)
    if kind == 'wheel':
        cx, cz, r, y0, y1 = q
        if view == 'side':
            du = (a - cx) / r; dv = (b - cz) / r; d2 = du*du + dv*dv
            if d2 > 1: return None
            return -y0, ('hub' if d2 < 0.22 else 'tyre')
        else:
            if not (y0 <= a <= y1) or abs(b - cz) > r: return None
            return -(cx - r) + 0.5, 'tyre'
    return None

def render(model, view, w=W, h=H, ox=0, oz=0):
    img = [[0]*w for _ in range(h)]; dep = [[-1e9]*w for _ in range(h)]
    for row in range(h):
        for col in range(w):
            a = (col + 0.5) + ox if view == 'side' else (col + 0.5) - w/2 + ox
            b = (h - 1 - row) + 0.5 + oz
            for prim in model.p:
                r = hit(prim, view, a, b)
                if r is None: continue
                d, s = r
                if d <= dep[row][col]: continue
                m = prim[2]
                if m == WIRE:
                    # a wire basket: only its edges and a grid of bars are solid, the rest you see through
                    x0, x1, y0, y1, z0, z1 = prim[1]
                    u0, u1 = (x0, x1) if view == 'side' else (y0, y1)
                    edge = a - u0 < 1 or u1 - a < 1 or b - z0 < 1 or z1 - b < 1
                    if not edge and int(a - u0) % 3 != 0 and int(b - z0) % 3 != 0: continue
                if s == 'hub': ix = 7
                elif s == 'tyre': ix = 6
                else: ix = shade_ix(m, s, col, row)
                img[row][col] = ix; dep[row][col] = d
    # outline: around the outside, and where something sits clearly in front of something else
    out = [r[:] for r in img]
    for row in range(h):
        for col in range(w):
            if img[row][col]:
                for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    c2, r2 = col + dc, row + dr
                    if 0 <= c2 < w and 0 <= r2 < h and img[r2][c2] and dep[r2][c2] - dep[row][col] > 2.6 and img[row][col] != 1:
                        out[row][col] = 1
                continue
            for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                c2, r2 = col + dc, row + dr
                if 0 <= c2 < w and 0 <= r2 < h and img[r2][c2]:
                    out[row][col] = 1; break
    return out

# ---------------------------------------------------------------- the sixteen bodies
def bodies():
    B = []
    # 0 BUGGY: a classic go-kart
    m = Model()
    m.box(3, 28, -8, 8, 2, 3.5, METAL).box(9, 21, -11, -8, 2, 5.5, BODY).box(9, 21, 8, 11, 2, 5.5, BODY)
    m.ell(24, 0, 4.5, 5.5, 7, 2.6, BODY).box(28, 30, -9, 9, 2, 3.5, TRIM)
    m.box(7, 13, -4, 4, 3.5, 7, DARK).box(6, 8, -4.5, 4.5, 3.5, 10.5, BODY).box(2, 7, 3, 8, 3.5, 7.5, METAL)
    m.box(0.5, 2, -6.5, 6.5, 2, 3.4, TRIM).box(15, 16, -2.5, 2.5, 7, 8.5, DARK)
    m.wheels(6, 24, 4.3, 3.2, 7.5, 12); B.append(("BUGGY", m, 9))
    # 1 BATHTUB
    m = Model()
    m.rbox(15, 0, 7, 13, 10, 5, BODY).box(4, 26, -8.5, 8.5, 11.5, 12.2, WHITE)
    for x, y in ((8, -4), (14, 3), (20, -2), (11, 5), (18, 6)): m.ell(x, y, 12.6, 2.2, 2.2, 1.5, WHITE)
    m.box(25, 28, -1, 1, 11, 14, METAL).box(27, 28.5, -1, 1, 12.5, 14, METAL)
    for x in (4, 26):
        for y in (-7, 7): m.ell(x, y, 2, 1.8, 1.8, 1.8, METAL)
    m.wheels(7, 23, 2.6, 2.6, 9, 11.5); B.append(("BATHTUB", m, 11))
    # 2 TEACUP
    m = Model()
    m.cylz(15, 0, 13, 2, 3.6, TRIM).cylz(15, 0, 6.5, 3.6, 13, BODY, 9.5).cylz(15, 0, 9.8, 12.3, 13.2, TRIM)
    m.box(14, 16, -12.5, -9, 10, 11.5, TRIM).box(14, 16, -12.5, -11, 6, 11.5, TRIM).box(14, 16, -12.5, -9, 6, 7.5, TRIM)
    m.wheels(7, 23, 2, 2, 6, 9); B.append(("TEACUP", m, 12))
    # 3 SNEAKER
    m = Model()
    m.rbox(15, 0, 2.6, 14, 8, 1.6, WHITE).ell(11, 0, 6.5, 10, 7, 5, BODY).ell(23, 0, 4.6, 7, 6.5, 3, BODY)
    for x in (14, 16.5, 19): m.box(x, x+1.2, -2.5, 2.5, 9.8, 10.8, WHITE)
    m.box(1, 3.5, -3, 3, 6, 11, TRIM).box(7, 21, -7.2, 7.2, 5, 6, TRIM)
    m.wheels(6, 24, 2.6, 2.6, 6, 9); B.append(("SNEAKER", m, 10))
    # 4 ROCKET
    m = Model()
    m.cylx(0, 7, 5, 4, 22, BODY).cylx(0, 7, 5, 22, 30.5, TRIM, 0.4).box(2, 7, -1, 1, 11, 15, TRIM)
    m.box(2, 7, -10, -5, 3, 7, TRIM).box(2, 7, 5, 10, 3, 7, TRIM).ell(17, -4.6, 9, 2.2, 1, 2, GLASS)
    m.cylx(0, 7, 3, 1, 4, METAL).wheels(7, 22, 2.5, 2.5, 4, 7); B.append(("ROCKET", m, 11))
    # 5 LEAF
    m = Model()
    m.ell(15, 0, 4, 14, 10.5, 2.4, BODY).ell(15, -8, 5.6, 12, 2.6, 2.2, BODY).ell(15, 8, 5.6, 12, 2.6, 2.2, BODY).box(2, 28, -0.6, 0.6, 5.8, 6.6, TRIM).box(0, 4, -0.7, 0.7, 4, 5, TRIM)
    for x in (9, 15, 21): m.box(x-0.5, x+0.5, -7, 7, 5.0, 5.6, TRIM)
    m.ell(25, 0, 5, 4, 8, 2.2, BODY).wheels(7, 23, 2.4, 2.4, 6, 9); B.append(("LEAF", m, 8))
    # 6 BOX (cardboard)
    m = Model()
    m.box(4, 26, -9, 9, 3, 12, BODY).box(4, 26, -11.5, -9, 11, 14.5, TRIM).box(4, 26, 9, 11.5, 11, 14.5, TRIM)
    m.box(4, 26, -1.2, 1.2, 3, 12.2, TRIM).wheels(7, 23, 3, 3, 9, 12); B.append(("BOX", m, 12))
    # 7 RUBBER DUCK
    m = Model()
    m.ell(13, 0, 6, 11, 8, 5, BODY).ell(3, 0, 9, 3, 3.5, 3, BODY).ell(22, 0, 11, 5, 4.5, 4.5, BODY)
    m.ell(27.5, 0, 10, 3, 2.6, 1.4, TRIM).ell(24.5, -3.3, 12.4, 1, 0.8, 1, DARK).ell(24.5, 3.3, 12.4, 1, 0.8, 1, DARK)
    m.ell(12, -7.5, 7, 6, 1.2, 2.6, BODY).ell(12, 7.5, 7, 6, 1.2, 2.6, BODY)
    m.wheels(6, 22, 2.8, 2.8, 7, 10); B.append(("RUBBER DUCK", m, 11))
    # 8 SOFA
    m = Model()
    m.rbox(15, 0, 4.5, 12, 10, 2.6, BODY).rbox(4, 0, 9, 3, 10.5, 5.5, BODY).rbox(15, -10, 7, 11, 2.2, 3, BODY).rbox(15, 10, 7, 11, 2.2, 3, BODY)
    m.rbox(16, -4.6, 7.6, 8, 4.2, 1.4, TRIM).rbox(16, 4.6, 7.6, 8, 4.2, 1.4, TRIM).rbox(6.5, -4.6, 10, 1.4, 4.2, 3.5, TRIM).rbox(6.5, 4.6, 10, 1.4, 4.2, 3.5, TRIM)
    m.wheels(6, 24, 2, 2, 7, 10); B.append(("SOFA", m, 10))
    # 9 SNAIL SHELL
    m = Model()
    m.rbox(15, 0, 2.6, 14, 7, 1.6, TRIM).ell(11, 0, 9, 8, 7, 7, BODY).ell(11, -6.4, 9, 4.5, 1.2, 4.5, TRIM).ell(11, -7.2, 9, 2.2, 0.7, 2.2, BODY)
    m.ell(11, 6.4, 9, 4.5, 1.2, 4.5, TRIM).ell(25, 0, 5, 5, 4, 3, TRIM).box(26, 27, -3, -2, 6, 11, TRIM).box(26, 27, 2, 3, 6, 11, TRIM)
    m.ell(26.5, -2.5, 11.5, 1, 1, 1, DARK).ell(26.5, 2.5, 11.5, 1, 1, 1, DARK).wheels(7, 23, 2, 2, 5, 8); B.append(("SNAIL SHELL", m, 12))
    # 10 BANANA
    m = Model()
    for x, z, r in ((3, 9.5, 2.6), (7, 6.6, 3.4), (12, 5, 3.8), (17, 5, 3.8), (22, 6.4, 3.4), (26, 9, 2.6)): m.ell(x, 0, z, 3.6, r + 2.6, r, BODY)
    m.box(0, 2, -0.8, 0.8, 10.5, 13, TRIM).ell(28.5, 0, 10.5, 1.4, 1.2, 1.4, DARK)
    m.wheels(7, 22, 2.5, 2.5, 6, 9); B.append(("BANANA", m, 9))
    # 11 SAUCER (it hovers: no wheels)
    m = Model()
    m.ell(15, 0, 5, 14, 12, 2.6, BODY).ell(15, 0, 3.4, 9, 8, 1.6, METAL).ell(15, 0, 8, 6, 6, 4, GLASS)
    for x, y in ((3, 0), (27, 0), (15, -11), (15, 11), (6, -7), (24, 7), (6, 7), (24, -7)): m.ell(x, y, 5, 1, 1, 0.9, LAMP)
    B.append(("SAUCER", m, 11))
    # 12 TOASTER
    m = Model()
    m.rbox(15, 0, 7, 12, 8, 5.5, BODY).box(9, 21, -4.6, -2, 11, 15, TRIM).box(9, 21, 2, 4.6, 11, 14, TRIM)
    m.box(24, 26, -9, -8, 5, 9, DARK).box(3, 27, -8.2, 8.2, 3.5, 4.5, METAL)
    m.wheels(6, 24, 2.8, 2.8, 8, 11); B.append(("TOASTER", m, 12))
    # 13 PIANO (a grand)
    m = Model()
    m.rbox(14, 0, 7, 12, 9, 3, BODY).box(8, 21, -9, 6, 10, 11, BODY).box(14, 15, 4, 5, 7, 10.5, METAL)
    m.box(25, 29, -9, 9, 6, 7.6, KEYS).box(5, 6, -7, -6, 2, 5, METAL).box(22, 23, 6, 7, 2, 5, METAL)
    m.wheels(6, 23, 2.5, 2.5, 6, 9); B.append(("PIANO", m, 11))
    # 14 BED
    m = Model()
    m.box(2, 29, -10, 10, 2.5, 4, TRIM).rbox(15, 0, 5.4, 13, 9.6, 1.8, WHITE).rbox(12, 0, 6.6, 10.5, 9.9, 1.6, BODY)
    m.rbox(25, 0, 7, 3.4, 6, 1.5, WHITE).box(28, 30, -10, 10, 2.5, 11.5, TRIM).box(1, 3, -10, 10, 2.5, 8, TRIM)
    m.wheels(5, 26, 2, 2, 7, 10); B.append(("BED", m, 10))
    # 15 SHOPPING CART
    m = Model()
    m.box(6, 27, -9, 9, 5, 13.5, WIRE).box(6, 27, -8, 8, 3, 4, METAL).box(2, 4, -9, 9, 12, 14, TRIM)
    m.box(3, 6.5, -8, -7, 8, 13, METAL).box(3, 6.5, 7, 8, 8, 13, METAL).box(8, 26, -8.5, 8.5, 4, 5, METAL)
    m.wheels(8, 25, 2, 2, 6, 8.5); B.append(("SHOPPING CART", m, 12))
    return B

# ---------------------------------------------------------------- spoilers and exhausts: small stamps laid over a body by the game
def stamp(model, view, w, h, ox, oz):
    return render(model, view, w, h, ox, oz)

def spoilers():
    S = []
    for k in range(8):
        m = Model()
        if k == 1: m.box(-10, 10, -8, 8, 0, 1.2, TRIM)
        if k == 2: m.box(-1, 2, -6, -5, 0, 3, METAL).box(-1, 2, 5, 6, 0, 3, METAL).box(-2, 3, -10, 10, 3, 4.6, TRIM)
        if k == 3: m.box(-1, 2, -7, -6, 0, 3.5, METAL).box(-1, 2, 6, 7, 0, 3.5, METAL).box(-3, 4, -12, 12, 3.5, 5.6, TRIM).box(-3, 4, -12.5, -11, 2.5, 6, BODY).box(-3, 4, 11, 12.5, 2.5, 6, BODY)
        if k == 4: m.box(-3, 3, -0.8, 0.8, 0, 5.5, TRIM)
        if k == 5: m.box(-3, 3, -7, -5.6, 0, 5.5, TRIM).box(-3, 3, 5.6, 7, 0, 5.5, TRIM)
        if k == 6: m.box(0, 1, -0.6, 0.6, 0, 2, METAL).cylx(0, 4.2, 2.6, -1, 1, METAL).box(-1, 0, -4.5, 4.5, 3.6, 4.8, WHITE).box(-1, 0, -0.6, 0.6, 1.4, 7, WHITE)
        if k == 7: m.box(0, 1, -0.6, 0.6, 0, 6.5, METAL).box(-4, 0, -0.4, 0.4, 1, 6, WHITE).box(-3, -1, -0.5, 0.5, 4, 5, WHITE)
        rear = render(m, 'rear', 28, 7, 0, 0)
        side = render(m, 'side', 12, 8, -6, 0)
        S.append((rear, side))
    return S

def exhausts():
    E = []
    for k in range(8):
        m = Model()
        if k == 1: m.cylx(0, 2, 1.2, 0, 3, METAL)
        if k == 2: m.cylx(-5, 2, 1.2, 0, 3, METAL).cylx(5, 2, 1.2, 0, 3, METAL)
        if k == 3:
            for y in (-7, -4, 4, 7): m.cylx(y, 2, 1, 0, 3, METAL)
        if k == 4: m.cylx(0, 2.4, 2.4, 0, 3, METAL).cylx(0, 2.4, 1.6, -3, 0, FLAME, 0.3)
        if k == 5: m.cylx(0, 2.4, 0.8, 1, 4, TRIM).cylx(0, 2.4, 2.4, -0.6, 1, TRIM, 1)
        if k == 6: m.cylz(-0.5, 5, 1.1, 0, 5, METAL).ell(-0.5, 5, 6.2, 1.8, 1.8, 1.4, WHITE)
        if k == 7: m.cylx(0, 2, 1.3, -2, 2, WHITE).box(-1.4, 0, -1.4, 1.4, 1, 3, LAMP)
        rear = render(m, 'rear', 24, 8, 0, -0.5)
        side = render(m, 'side', 8, 8, -4, -0.5)
        E.append((rear, side))
    return E

def body_anchor(m, img_r, img_s):
    # where the spoiler and exhaust sit: the top of the body near its tail, and the bottom of its tail
    top_r = next((r for r in range(H) if any(img_r[r])), 4)
    cols = [c for c in range(W) if any(img_s[r][c] for r in range(H))]
    tail = cols[0] if cols else 2
    top_s = next((r for r in range(H) if any(img_s[r][c] for c in range(tail, tail + 6))), 4)
    return top_r, tail, top_s

def make(arr, out):
    B = bodies(); S = spoilers(); E = exhausts()
    rear_all, side_all, anchors, names, seat = [], [], [], [], []
    for name, m, seatz in B:
        r = render(m, 'rear'); s = render(m, 'side')
        rear_all += [v for row in r for v in row]; side_all += [v for row in s for v in row]
        anchors += list(body_anchor(m, r, s)); names.append(name); seat.append(seatz)
    arr("kb_rear", "unsigned char", rear_all, per=32); arr("kb_side", "unsigned char", side_all, per=32)
    arr("kb_anchor", "unsigned char", anchors); arr("kb_seat", "unsigned char", seat)
    out.append("#define NKBODY %d" % len(B))
    out.append("static const char *const kbody_name[NKBODY]={%s};" % ",".join('"%s"' % n for n in names))
    arr("ksp_rear", "unsigned char", [v for st in S for row in st[0] for v in row], per=28)
    arr("ksp_side", "unsigned char", [v for st in S for row in st[1] for v in row], per=12)
    arr("kex_rear", "unsigned char", [v for st in E for row in st[0] for v in row], per=24)
    arr("kex_side", "unsigned char", [v for st in E for row in st[1] for v in row], per=8)
    arr("kpat_mask", "unsigned char", pat_masks(), per=32)
    arr("kstk_px", "unsigned char", sticker_px(), per=36)
    return B, S, E

if __name__ == "__main__":
    # preview: a big contact sheet of every body from behind and from the side
    from PIL import Image
    PAL = [(255, 0, 255), (14, 16, 28), (150, 40, 50), (210, 60, 70), (250, 120, 120), (250, 200, 60), (40, 40, 48), (170, 170, 185),
           (150, 156, 176), (232, 60, 60), (150, 210, 250), (240, 240, 248), (60, 120, 220), (40, 36, 48), (255, 160, 40), (255, 235, 140)]
    B = bodies(); S = spoilers(); E = exhausts()
    sc = 6; im = Image.new("RGB", (2 * (W * sc + 8) + 8, len(B) * (H * sc + 6) + 8), (30, 30, 40))
    for i, (name, m, _) in enumerate(B):
        for j, view in enumerate(('rear', 'side')):
            img = render(m, view)
            if i % 2 == 0 and view == 'rear':
                st = S[(i // 2) % 8][0]; ex = E[(i // 2) % 8][0]
            for r in range(H):
                for c in range(W):
                    v = img[r][c]
                    for yy in range(sc):
                        for xx in range(sc):
                            im.putpixel((8 + j * (W * sc + 8) + c * sc + xx, 8 + i * (H * sc + 6) + r * sc + yy), PAL[v] if v else (60, 64, 80))
    im.save("/tmp/claude-0/-home-claude-bugstar/8a995888-4458-500d-9733-3f2e01c07817/scratchpad/karts.png")
    print("ok", len(B))

# ---------------------------------------------------------------- body patterns (which body pixels take the pattern colour), stickers, toppers, props
def tri(c, p, a):
    k = c % p; h = p // 2
    return (k if k <= h else p - k) * a // h

def pat_masks():
    def noise(c, r):
        v = 0
        for (sx, sy, w) in ((5, 4, 2), (3, 3, 1)):
            h = (((c // sx) * 73856093) ^ ((r // sy) * 19349663)) & 0xFFFF
            v += (h % 7) * w
        return v
    M = []
    for p in range(12):
        for view in ('rear', 'side'):
            bits = []
            for r in range(H):
                row = 0
                for c in range(W):
                    on = False
                    if p == 1: on = (14 <= c <= 17) if view == 'rear' else r in (9, 10)
                    if p == 2: on = c in (11, 12, 19, 20) if view == 'rear' else r in (8, 12)
                    if p == 3:
                        if view == 'rear': on = r >= 11 - tri(c, 6, 4)
                        else:
                            L = {6: 3, 7: 6, 8: 10, 9: 7, 10: 12, 11: 8, 12: 11, 13: 6, 14: 4}.get(r, 0)
                            on = c >= 30 - L
                    if p == 4: on = ((c // 2) + (r // 2)) % 2 == 0
                    if p == 5: on = ((c + (r // 5) * 3) % 6) in (1, 2) and r % 5 in (1, 2)
                    if p == 6: on = abs(r - (8 + tri(c, 6, 3))) <= 0 or abs(r - (9 + tri(c, 6, 3))) <= 0
                    if p == 7:
                        stars = ((8, 7), (20, 9), (14, 12), (25, 6), (5, 11)) if view == 'side' else ((10, 8), (21, 9), (16, 12))
                        on = any((abs(c - x) + abs(r - y)) <= 1 for x, y in stars)
                    if p == 8: on = (c < 16) if view == 'rear' else (c >= 16)
                    if p == 9: on = r >= 12 or (r >= 9 and (c + r) % 2 == 0)
                    if p == 10:
                        if view == 'rear': on = abs(c - (16 + (3 if (r // 3) % 2 else -3) + (r % 3) * (-2 if (r // 3) % 2 else 2))) <= 1
                        else: on = abs(r - (6 + c // 3)) <= 0 or abs(r - (7 + c // 3)) <= 0 if (c // 6) % 2 == 0 else abs(r - (14 - c // 4)) <= 0 or abs(r - (13 - c // 4)) <= 0
                    if p == 11: on = noise(c, r) >= 9
                    if on: row |= 1 << c
                bits += [row & 255, (row >> 8) & 255, (row >> 16) & 255, (row >> 24) & 255]
            M += bits
    return M

STICKERS = [None,
 ["..1...", ".1b1..", "1bbb1.", ".1b1..", "..1...", "......"],       # STAR (a white sparkle)
 [".1.1..", "1919 1".replace(" ", ""), "19991.", ".191..", "..1...", "......"],
 [".111..", "1bbb1.", "1dbd1.", "1bbb1.", ".1b1..", "......"],       # SKULL
 [".111..", "19d91.", "19991.", "1d9d1.", ".111..", "......"],       # BUG (a ladybird)
 ["..1...", ".1e1..", ".1e91.", "1e991.", ".111..", "......"],       # FLAME
 ["..11..", ".1e1..", "1ee1..", ".1ee1.", ".1e1..", ".11..."],       # BOLT
 [".111..", "1eee1.", "1d1d1.", "1eee1.", ".1d1..", "..1..."]]       # SMILE (more of a grin)
def sticker_px():
    out = []
    for st in STICKERS[1:]:
        rows = [r.ljust(6, '.')[:6] for r in st]
        for r in rows:
            for ch in r: out.append(0 if ch == '.' else int(ch, 16))
    return out

TOPPERS = [
 ["......1111......", ".....195991.....", ".....199991.....", "......1111......", ".......1f1......", ".......1f1......", ".......1f1......", "......11f11....."],   # ANTENNA BALL
 ["......1111......", ".....1a5aa1.....", ".....1aaaa1.....", "....1aaaaaa1....", "....1aaaaaa1....", "...1ffffffff1...", "...1f5ffffff1...", "....11111111...."],   # SIREN
 [".......11.......", "......1991......", "......1551......", ".....199991.....", ".....155551.....", "....19999991....", "...1111111111...", "...1ffffffff1..."],   # CONE
 ["...111...111....", "..1ddd111ddd1...", "...111.1.111....", ".......1........", ".....11111......", "....1939391.....", "...193939391....", "...111111111...."],   # PROPELLER HAT
 ["......111.......", ".....19991......", "....199d991.....", ".....19991......", "......1c1.11....", "......1c11cc1...", "......1cc1c1....", "......1c11......"],   # FLOWER
 ["................", "..1...1..1...1..", ".1d1.1d11d1.1d1.", ".1d11d9dd9d11d1.", ".1dddddddddddd1.", ".1d9dd5dd9dd5d1.", ".1dddddddddddd1.", "..111111111111.."],   # CROWN
 ["..11........11..", ".151........151.", ".15.1......1.51.", "..15.1....1.51..", "..155.1111.551..", "...1555555551...", "....11111111....", "................"],   # HORNS
 [".11111111111....", ".1f15151511.....", ".1f51515151.....", ".1f15151511.....", ".1f11111111.....", ".1f1............", ".1f1............", ".111............"],   # FLAG
 [".......11.......", ".....111111.....", "....15f5f5f1....", "...1f5f5f5f51...", "...15f5f5f5f1...", "....1f5f5f51....", ".....111111.....", "................"],   # DISCO BALL
 [".....11111......", "....1ddddd1.....", "....1d555d1.....", ".....11111......", ".......1f1......", ".......1f1......", ".......1f1......", "......11111....."],   # LAMP
 [".........11.....", "........1c1.....", ".......1c..1....", "......1c...1....", "..1111c.1111....", ".199591.199591..", ".199991.199991..", "..1111...1111..."],   # CHERRY
 ["....11111111....", "....12222221....", "....12322221....", "....12222221....", "....19999991....", "....12222221....", "..111111111111..", "..122222222221.."],   # TOP HAT
 ["..11........11..", ".1991......1991.", ".19991.11.19991.", ".199991991999 1.".replace(" ", "9"), ".19991.11.19991.", ".1991..19..1991.", "..11...11...11..", "................"],   # BOW
 ["........1.......", ".......1f1......", "......1ff1......", ".....1fff1......", "....1ffff51.....", "...1fffff51.....", "..1ffffff51.....", ".11111111111...."],   # SHARK FIN
 ["................", "...1111111111...", "..1dddddddddd1..", ".1d1111111111d1.", "..1dddddddddd1..", "...1111111111...", "................", "................"],   # HALO
]
IBOX = ["..111111111111..", ".1dddddddddddd1.", ".1daaaaaaaaaad1.", ".1daaa5555aaad1.", ".1daa55aa55aad1.", ".1daaaaaa55aad1.", ".1daaaaa55aaad1.", ".1daaaa55aaaad1.",
        ".1daaaa55aaaad1.", ".1daaaaaaaaaad1.", ".1daaaa55aaaad1.", ".1daaaa55aaaad1.", ".1daaaaaaaaaad1.", ".1dddddddddddd1.", "..111111111111..", "................"]
KSHELL = [".....111111.....", "....1cccccc1....", "...1cc5cccbc1...", "..1ccccccccbc1..", "..1cbcccccbcc1..", "..1ccbbbbbbcc1..", "..1cccccccccc1..", "...1cccccccc1...", "....11111111....", "...1ffffffff1...", "....11111111...."]
KSLIME = ["................", "......1111......", "....11cccc11....", "..11cc5ccccc11..", ".1cccccccccbbc1.", ".1bccccccbbbbc1.", "..11bbbbbbbb11..", "....11111111...."]
