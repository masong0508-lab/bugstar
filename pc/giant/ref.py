"""Independent Python reference for pc/giant (written from the Conway-Wechsler rules and the definitions in README.md, not from the C code).
Used by test.sh / test_giant.py. Nothing here is needed to play the game."""
import os, struct, hashlib

# ---- Conway-Wechsler: units, tens, hundreds, each with the letters (M N S X) that the unit before it may need
UNITS = ["", "un", "duo", "tre", "quattuor", "quinqua", "se", "septe", "octo", "nove"]
TENS = [("", ""), ("deci", "N"), ("viginti", "MS"), ("triginta", "NS"), ("quadraginta", "NS"), ("quinquaginta", "NS"), ("sexaginta", "N"), ("septuaginta", "N"), ("octoginta", "MX"), ("nonaginta", "")]
HUNDS = [("", ""), ("centi", "NX"), ("ducenti", "N"), ("trecenti", "NS"), ("quadringenti", "NS"), ("quingenti", "NS"), ("sescenti", "N"), ("septingenti", "N"), ("octingenti", "MX"), ("nongenti", "")]
ONES = ["ni", "mi", "bi", "tri", "quadri", "quinti", "sexti", "septi", "octi", "noni"]

def cw_prefix(v):
    if v < 10: return ONES[v].upper()
    h, t, u = v // 100, (v // 10) % 10, v % 10
    tp, tm = TENS[t]; hp, hm = HUNDS[h]
    nxt = tm if t else hm            # the part right after the unit decides the extra letter
    unit = UNITS[u]
    if u == 3 and ("S" in nxt or "X" in nxt): unit += "s"
    elif u == 6:
        if "S" in nxt: unit += "s"
        elif "X" in nxt: unit += "x"
    elif u in (7, 9):
        if "M" in nxt: unit += "m"
        elif "N" in nxt: unit += "n"
    return (unit + tp + hp).upper()

def cw_token(v, last):
    p = cw_prefix(v)
    if p and p[-1] in "IA": p = p[:-1]
    return p + ("ILLION" if last else "ILLI")

def chunks_of(m):                    # decimal string -> list of ints, most significant first, groups of three counted from the right
    m = m.lstrip("0") or "0"
    out = []
    e = len(m)
    while e > 0:
        out.append(int(m[max(0, e - 3):e])); e -= 3
    return out[::-1]

def place_name(m):                   # the name of the m-illion = 10^(3m+3), m a decimal string
    if m.lstrip("0") == "": return "THOUSAND"
    ch = chunks_of(m)
    return "".join(cw_token(c, i == len(ch) - 1) for i, c in enumerate(ch))

ONE = ["ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN", "ELEVEN", "TWELVE", "THIRTEEN", "FOURTEEN", "FIFTEEN", "SIXTEEN", "SEVENTEEN", "EIGHTEEN", "NINETEEN"]
TY = ["", "", "TWENTY", "THIRTY", "FORTY", "FIFTY", "SIXTY", "SEVENTY", "EIGHTY", "NINETY"]
def english_chunk(v):
    parts = []
    if v >= 100: parts.append(ONE[v // 100] + " HUNDRED")
    r = v % 100
    if r or v == 0:
        if r < 20: parts.append(ONE[r])
        else: parts.append(TY[r // 10] + ("-" + ONE[r % 10] if r % 10 else ""))
    return " ".join(parts)
SCALE = ["", "THOUSAND", "MILLION", "BILLION", "TRILLION", "QUADRILLION", "QUINTILLION"]
def english(n):
    if n == 0: return "ZERO"
    g = []
    while n: g.append(n % 1000); n //= 1000
    out = []
    for i in range(len(g) - 1, -1, -1):
        if g[i]: out.append(english_chunk(g[i]) + (" " + SCALE[i] if i else ""))
    return " ".join(out)

# ---- the numbers
E = 32; N = 1 << E; POW_DIGITS = 1292913987
COUNT_LAST = 123456790
def count_digits_upto(n):            # digits of 1..n written one after another
    t = 0; d = 1; lo = 1
    while lo <= n:
        hi = min(n, lo * 10 - 1); t += (hi - lo + 1) * d; d += 1; lo *= 10
    return t
COUNT_DIGITS = count_digits_upto(COUNT_LAST)
def count_window(pos, n):            # by bisection on "how many digits do 1..k take": a different way from the C code's class walk
    lo, hi = 1, COUNT_LAST
    while lo < hi:
        mid = (lo + hi) // 2
        if count_digits_upto(mid) > pos: hi = mid
        else: lo = mid + 1
    k = lo; start = count_digits_upto(k - 1); s = ""
    while len(s) < n + (pos - start) and k <= COUNT_LAST:
        s += str(k); k += 1
    return s[pos - start: pos - start + n]

def pow_open(path):
    f = open(path, "rb"); h = f.read(64)
    assert h[:8] == b"BSGPOW2\0"
    e, dg, _ = struct.unpack("<QQQ", h[8:32]); assert e == E and dg == POW_DIGITS
    return f
def pow_window(f, pos, n):           # nibbles straight from the file
    n = min(n, POW_DIGITS - pos)
    b0, b1 = pos >> 1, (pos + n - 1) >> 1
    f.seek(64 + b0); raw = f.read(b1 - b0 + 1).hex()
    s = raw[pos - 2 * b0: pos - 2 * b0 + n]
    return s

# ---- v3
def h32(q):
    h = (q * 2654435761) & 0xFFFFFFFF; h ^= h >> 15; h = (h * 2246822519) & 0xFFFFFFFF; h ^= h >> 13; return h
def coef(j): return 100 + h32(j) % 900
