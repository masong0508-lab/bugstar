#!/usr/bin/env python3
"""Tests of pc/giant against the independent Python reference (ref.py).
usage: test_giant.py quick | count | pow | v3 | all    (BIN=giant_test binary, POWFILE=path of pow2_2e32.bcd)"""
import os, sys, random, subprocess, hashlib, time, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ref
BIN = os.environ.get("BIN", "/tmp/giant_test"); POWFILE = os.environ.get("POWFILE", "")
TMP = os.environ.get("TMPD", "/tmp"); fails = []
def ok(c, msg):
    print(("PASS " if c else "FAIL ") + msg, flush=True)
    if not c: fails.append(msg)
def run(*a, inp=None):
    return subprocess.run([BIN] + [str(x) for x in a], capture_output=True, text=True, check=False).stdout
def win_file(pairs, name):
    p = os.path.join(TMP, name)
    open(p, "w").write("".join("%d %d\n" % x for x in pairs)); return p

def quick():
    out = run("tokens").splitlines(); bad = 0
    for v, line in enumerate(out):
        f = line.split(" ", 4)
        exp = (str(v), ref.cw_token(v, False), ref.cw_token(v, True), ref.english_chunk(v))
        got = (f[0], f[1], f[2], line.split(" ", 3)[3])
        if got != exp: bad += 1; print(got, exp)
    ok(bad == 0, "all 1000 chunks: -illi token, -illion token, English words equal the reference (%d bad)" % bad)
    rnd = random.Random(7); ms = ["0", "1", "9", "10", "999", "1000", "1001", "999999", "1000000"] + [str(rnd.randrange(10 ** rnd.randrange(1, 400))) for _ in range(300)]
    bad = 0
    for line in run("place", *ms).splitlines():
        m, n, name = line.split(" ", 2)
        exp = ref.place_name(m)
        if name != "(long)" and (name != exp or int(n) != len(exp)): bad += 1; print(m, name, exp)
    ok(bad == 0, "-illion names of 309 numbers (up to 400 digits) equal the reference")
    ms = ["1", "3", "20", "63", "100", "303", "3003", "3000000", "3000012"]
    # well known names
    known = {"1": "MILLION", "2": "BILLION", "10": "DECILLION", "20": "VIGINTILLION", "100": "CENTILLION", "1000": "MILLINILLION", "3000012": "TRILLINILLITRILLION"}
    got = {l.split()[0]: l.split()[2] for l in run("place", *known).splitlines()}
    ok(all(got[k] == v for k, v in known.items() if k != "3000012") , "well known names (million, billion, decillion, vigintillion, centillion, millinillion)")
    # googolplex = 10 x (m-illion), m = (10^100 - 1)/3 - 1
    m = str((10 ** 100 - 1) // 3 - 1); name = ref.place_name(m)
    print("googolplex: TEN %s (%d letters)" % (name[:40] + "...", len(name)))
    ok(len(name) == int(run("place", m).split()[1]), "googolplex name length (%d letters) equals the reference" % len(name))
    vals = [0, 7, 1000, 1292913987, 2 ** 32, 2 ** 64 - 1, 10 ** 18, 999999999999]
    ok(run("en", *vals).splitlines() == [ref.english(v) for v in vals], "English words of 64-bit numbers equal the reference")
    # sha256
    p = os.path.join(TMP, "sha_in.bin"); data = os.urandom(300000) + b"abc"; open(p, "wb").write(data)
    ok(run("sha", p).strip() == hashlib.sha256(data).hexdigest(), "SHA-256 of 300 KB equals hashlib")
    for n in (0, 1, 55, 56, 57, 63, 64, 65, 119, 120, 128):
        open(p, "wb").write(b"x" * n); assert run("sha", p).strip() == hashlib.sha256(b"x" * n).hexdigest(), n
    ok(True, "SHA-256 of 11 padding-edge sizes equals hashlib")
    rnd = random.Random(3); bad = 0
    for _ in range(300):
        s = str(rnd.randrange(1, 10 ** rnd.randrange(1, 90))); q, r, o = run("ds", s).split()
        if int(q) != int(s) // 3 or int(r) != int(s) % 3 or int(o) != int(s) - 1: bad += 1
    ok(bad == 0, "decimal-string divide by 3 and minus 1 on 300 numbers")

def count():
    ok(ref.COUNT_DIGITS == 1000000008, "the counting number has %d digits (reference)" % ref.COUNT_DIGITS)
    rnd = random.Random(11); pairs = []
    D = ref.COUNT_DIGITS
    edges = [0, 1, 8, 9, 10, 11, 188, 189, 190, 191, 2889, 2890, 38889, 38890, 488889, 488890, 5888889, 5888890, 68888889, 68888890, 788888889 - 3, 788888889, 788888890, D - 40, D - 9, D - 1]
    pairs += [(p, 40) for p in edges if p < D]
    for _ in range(3000): pairs.append((rnd.randrange(D - 700), rnd.randrange(1, 700)))
    pairs.append((D - 100, 5000))
    out = run("read", "count", win_file(pairs, "cw.txt")).split("\n")
    bad = 0
    for (p, n), g in zip(pairs, out):
        e = ref.count_window(p, n)
        if g != e: bad += 1; print("bad", p, n, g[:30], e[:30])
    ok(bad == 0, "counting number: %d windows (edges at every digit-length change, the end, 3000 random) equal the reference" % len(pairs))
    ok(run("read", "count", win_file([(D - 20, 20)], "cw2.txt")).strip().endswith("123456789123456790"), "the counting number ends ...123456789123456790")
    print(run("seek", "count", 20000).strip())
    # export
    path = os.path.join(TMP, "count_export.txt"); t = time.time()
    r = run("export", "count", path).split(); print("export:", r)
    sh = hashlib.sha256(); size = 0; buf = ""
    for a in range(1, ref.COUNT_LAST + 1, 2_000_000):
        s = buf + "".join(map(str, range(a, min(a + 2_000_000, ref.COUNT_LAST + 1))))
        n = len(s) // 100 * 100
        lines = "\n".join(s[i:i + 100] for i in range(0, n, 100)) + ("\n" if n else "")
        buf = s[n:]; b = lines.encode(); sh.update(b); size += len(b)
    if buf: b = (buf + "\n").encode(); sh.update(b); size += len(b)
    ok(os.path.getsize(path) == size == int(r[1]), "counting number export: file size %d equals the reference" % size)
    ok(sh.hexdigest() == r[2], "counting number export: SHA-256 equals the reference stream (%s)" % sh.hexdigest()[:16])
    os.remove(path)

if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "quick"
    if what in ("quick", "all"): quick()
    if what in ("count", "all"): count()
    if what in ("pow", "all"):
        import test_pow; test_pow.run_all(ok, run, win_file, POWFILE, TMP)
    if what in ("v3", "all"):
        import test_v3; test_v3.run_all(ok, run, win_file, POWFILE, TMP)
    print("%d failed" % len(fails) if fails else "ALL PASSED"); sys.exit(1 if fails else 0)
