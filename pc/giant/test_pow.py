"""Tests of the 2^(2^32) number (needs the 646 MB file from mkpow.py). Called by test_giant.py pow."""
import os, random, hashlib, time, decimal, subprocess
from decimal import Decimal, Context
import ref

def C(p): return Context(prec=p, Emax=decimal.MAX_EMAX, Emin=decimal.MIN_EMIN)

def run_all(ok, run, win_file, POWFILE, TMP):
    d = os.path.dirname(POWFILE); f = ref.pow_open(POWFILE); N = ref.N; D = ref.POW_DIGITS
    out = run("-d", d) if False else None
    def runp(*a): return run(a[0], "pow", "-d", d, *a[1:]) if False else run(*a)
    # (giant_test takes: cmd [which] [-d dir] ...)
    os.environ["BUGSTAR_GIANT_DIR"] = d
    def rd(cmd, *a): return run(cmd, "pow", *a)
    log2 = C(500).log10(Decimal(2)); x = C(500).multiply(Decimal(N), log2)
    digits = int(x) + 1
    ok(digits == D, "digit count: floor(2^32 x log10 2) + 1 = %d (500-digit logarithm)" % digits)
    # leading digits from the logarithm, a different road from the file
    fr = C(500).subtract(x, Decimal(int(x)))
    lead = int(C(500).multiply(C(500).power(Decimal(10), fr), Decimal(10) ** 299).to_integral_value(rounding=decimal.ROUND_FLOOR))
    first = ref.pow_window(f, 0, 300)
    ok(str(lead) == first, "first 300 digits of the file equal the logarithm's: %s..." % first[:30])
    tail = "%02000d" % pow(2, N, 10 ** 2000)
    last = ref.pow_window(f, D - 2000, 2000)
    ok(tail == last, "last 2000 digits of the file equal pow(2, 2^32, 10^2000)")
    # the whole number modulo primes (every digit matters)
    primes = [3, 7, 11, 13, 101, 998244353, 1000000007, 2147483647, 2305843009213693951, 1000000000000000003]
    t = time.time(); got = rd("modcheck", *primes).split("\n"); bad = 0
    for line in got:
        if not line: continue
        p, r = map(int, line.split())
        if pow(2, N, p) != r: bad += 1; print("bad residue", p, r, pow(2, N, p))
    ok(bad == 0 and len(got) >= len(primes), "the file's whole value equals 2^(2^32) modulo %d primes (%.0fs)" % (len(primes), time.time() - t))
    # random windows
    rnd = random.Random(2026); pairs = []
    edges = [0, 1, 2, 3, 299, 300, D - 2, D - 1, D - 40, D // 2, D // 2 + 1, 1 << 20, (1 << 20) + 1, (1 << 26) - 1, (1 << 26), (1 << 26) + 1]
    pairs += [(p, 40) for p in edges if p + 40 <= D]
    pairs += [(D - 3, 3), (D - 1, 1)]
    for _ in range(4000): pairs.append((rnd.randrange(D - 700), rnd.randrange(1, 700)))
    out = rd("read", win_file(pairs, "pw.txt")).split("\n"); bad = 0
    for (p, n), g in zip(pairs, out):
        if g != ref.pow_window(f, p, n): bad += 1; print("bad window", p, n)
    ok(bad == 0, "%d windows (both nibble halves, both ends, the middle, 4000 random) equal the file read by Python" % len(pairs))
    # whole-number group structure: reconstruct the order of magnitude from the name
    G = D // 3; m = str(G - 2); name = ref.place_name(m)
    toks = []
    import re
    n_back = 0
    parts = re.findall(r"[A-Z]+?ILLI(?:ON)?", name)
    ok("".join(parts) == name and len(parts) == 3, "the name of the 1000^%d place is %s (%d letters, 3 parts)" % (G - 1, name, len(name)))
    # decode: find each chunk whose token equals the part
    tokmap = {ref.cw_token(v, p == len(parts) - 1) if False else None: v for v in []}
    decoded = []
    for i, pt in enumerate(parts):
        last_ = i == len(parts) - 1
        cands = [v for v in range(1000) if ref.cw_token(v, last_) == pt]
        assert len(cands) == 1, (pt, cands); decoded.append(cands[0])
    n = decoded[0] * 10 ** 6 + decoded[1] * 1000 + decoded[2]
    firstgroup = ref.pow_window(f, 0, 3)
    # value = firstgroup x 1000^(G-1) x (rest < 1): its number of digits is len(firstgroup) + 3 (G - 1)
    ok(n == G - 2 and len(firstgroup) + 3 * (n + 1) == D, "decoding the name gives back n = %d, so the number has %d + 3 x %d = %d digits" % (n, len(firstgroup), n + 1, D))
    # export
    path = os.path.join(TMP, "pow_export.txt"); t = time.time()
    r = rd("export", path).split(); print("export:", r)
    sh = hashlib.sha256(); size = 0; B = 100 * 100000; pos = 0
    while pos < D:
        n_ = min(B, D - pos); s = ref.pow_window(f, pos, n_)
        lines = "\n".join(s[i:i + 100] for i in range(0, len(s), 100)) + "\n"; b = lines.encode(); sh.update(b); size += len(b); pos += n_
    ok(os.path.getsize(path) == size == int(r[1]) == D + (D + 99) // 100, "export: file size %d = digits + one newline per 100 digits" % size)
    ok(sh.hexdigest() == r[2], "export: SHA-256 equals the reference stream (%s...)" % sh.hexdigest()[:16])
    os.remove(path)
    print(rd("seek", 20000).strip())
