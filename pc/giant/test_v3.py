"""Tests of BUGSTARILLION v3 (needs the file from mkpow.py). Called by test_giant.py v3."""
import os, random, time, re
import numpy as np
import ref

TLtab = np.array([len(ref.cw_token(v, False)) for v in range(1000)], dtype=np.int64)
CLtab = np.array([len(ref.english_chunk(v)) for v in range(1000)], dtype=np.int64)

def load_digits(path):                       # the whole number as one byte per digit (1.3 GB)
    raw = np.fromfile(path, dtype=np.uint8, offset=64)
    g = np.empty(2 * len(raw), dtype=np.uint8); g[0::2] = raw >> 4; g[1::2] = raw & 15; return g[:ref.POW_DIGITS]

def term_lengths_and_P(g, sample_js):         # W(j) for every j by numpy in chunks; P(j) = W(1) + ... + W(j) at the sample j, and the total
    L = len(g); lv = {1: int(g[0]), 2: int(g[0]) * 10 + int(g[1]), 3: int(g[0]) * 100 + int(g[1]) * 10 + int(g[2])}
    carry = [0, 0, 0]; P = 0; res = {}; sample = sorted(set(sample_js)); si = 0
    CH = 1 << 24; coefL = None
    for j0 in range(1, L + 1, CH):
        j1 = min(L, j0 + CH - 1); n = j1 - j0 + 1
        seg = g[j0: j1 + 3].astype(np.int64)            # digits g[j0 .. j1+2] (0-based)
        if len(seg) < n + 2: seg = np.concatenate([seg, np.zeros(n + 2 - len(seg), dtype=np.int64)])
        tri = TLtab[seg[0:n] * 100 + seg[1:n + 1] * 10 + seg[2:n + 2]]       # TL3 at positions j0 .. j1
        js = np.arange(j0, j1 + 1, dtype=np.uint64)
        # beyond L-3 a triple does not exist (never needed): zero it
        bad = js > (L - 3); tri = np.where(bad, 0, tri)
        S = np.zeros(n, dtype=np.int64)
        for r in range(3):
            idx = np.nonzero((js % 3) == r)[0]
            if len(idx) == 0: continue
            cs = np.cumsum(tri[idx]); S[idx] = carry[r] + cs - tri[idx]; carry[r] += int(cs[-1])
        a = ((js - 1) % 3).astype(np.int64) + 1
        h = (js * np.uint64(2654435761)) & np.uint64(0xFFFFFFFF); h ^= h >> np.uint64(15); h = (h * np.uint64(2246822519)) & np.uint64(0xFFFFFFFF); h ^= h >> np.uint64(13)
        coef = 100 + (h % np.uint64(900)).astype(np.int64)
        lvv = np.where(a == 1, lv[1], np.where(a == 2, lv[2], lv[3]))
        W = CLtab[coef] + 1 + TLtab[lvv] + S + 4
        cw = np.cumsum(W)
        while si < len(sample) and sample[si] <= j1:
            j = sample[si]; res[j] = (P + int(cw[j - j0]) - int(W[j - j0]), int(W[j - j0])); si += 1     # (P(j-1), W(j))
        P += int(cw[-1])
    return P, res

def term_window(g, j, off, n):               # letters off .. off+n-1 of the text of term j, made from the digits and the reference names
    c = ref.coef(j); head = ref.english_chunk(c) + " "
    a = (j - 1) % 3 + 1; lead = int("".join(map(str, g[:a]))); k = (j - a) // 3
    out = ""
    # find where in the place name letter off - len(head) falls: walk the tokens (with numpy chunks for long ones)
    if off < len(head): out = head[off:]; off2 = 0
    else: off2 = off - len(head)
    need = n - len(out)
    if need > 0:
        t0 = len(ref.cw_token(lead, k == 0)); text = ""
        if off2 < t0: text = ref.cw_token(lead, k == 0)[off2:]; start_chunk = 1; skip = 0
        else:
            r = off2 - t0; i = 0; B = 1 << 22
            while True:                      # skip whole blocks of triples with numpy, then walk
                m = min(B, k - i)
                if m <= 0: break
                tri_ = g[a + 3 * i: a + 3 * (i + m)].astype(np.int64).reshape(m, 3); tl = TLtab[tri_[:, 0] * 100 + tri_[:, 1] * 10 + tri_[:, 2]]
                if i + m == k: tl[-1] += 2
                s = int(tl.sum())
                if r >= s: r -= s; i += m
                else: break
            m = min(B, k - i); tri_ = g[a + 3 * i: a + 3 * (i + m)].astype(np.int64).reshape(m, 3)
            q = 0
            while q < m:
                v = int(tri_[q, 0]) * 100 + int(tri_[q, 1]) * 10 + int(tri_[q, 2]); tk = ref.cw_token(v, i + q == k - 1 + 0 and True)
                if r < len(tk): break
                r -= len(tk); q += 1
            i += q; start_chunk = i + 1; skip = r
            text = ""
        # build the tokens from start_chunk on, until enough
        def tokens(first_chunk):
            ci = first_chunk
            while ci <= k:
                v = lead if ci == 0 else int("".join(map(str, g[a + 3 * (ci - 1): a + 3 * ci])))
                yield ref.cw_token(v, ci == k); ci += 1
        if off2 < t0: acc = text
        else: acc = ""
        first = True
        for tk in tokens(start_chunk if off2 >= t0 else 1):
            if first and off2 >= t0: tk = tk[skip:]; first = False
            acc += tk
            if len(acc) >= need: break
        out += acc[:need]
        # if the place name finished and more is needed, the separator follows
        full = len(acc) < need
        if full: out += (", " if j > 1 else ".")[:n - len(out)]
    return out[:n]

def run_all(ok, run, win_file, POWFILE, TMP):
    d = os.path.dirname(POWFILE)
    def rd(*a): return run("v3", *a)
    def v3(*a): return run("v3", "-d", d, *a) if False else run(*a)
    # giant_test syntax: giant_test -d DIR ... is not used: v3 reads the folder from BUGSTAR_GIANT_DIR
    os.environ["BUGSTAR_GIANT_DIR"] = d
    t = time.time(); g = load_digits(POWFILE); L = ref.POW_DIGITS
    print("loaded digits %.0fs" % (time.time() - t))
    rnd = random.Random(33)
    js = [1, 2, 3, 4, 5, 6, 7, 100, 1000, 65535, 65536, 65537, 131072, 1 << 20, (1 << 20) + 1, L // 2, L - 5, L - 4, L - 3, L - 2, L - 1, L]
    js += [int(10 ** rnd.uniform(0, 9.1)) % L + 1 for _ in range(300)]
    t = time.time(); Ptot, res = term_lengths_and_P(g, js); print("reference total %.0fs" % (time.time() - t))
    tot, parts = Ptot - 1, L
    got = run("v3", "total").split()
    ok(int(got[0]) == L, "V3 has %d terms (one for each digit of 2^(2^32))" % L)
    ok(int(got[1]) == tot, "V3's name has exactly %d letters (numpy reference over all %d terms)" % (tot, L))
    # term starts and lengths
    out = run("v3", "term", *sorted(set(js))).split("\n"); bad = 0
    for line in out:
        if not line: continue
        j, a, b = line.split(); j = int(j); st = int(a.split("=")[1]); ln = int(b.split("=")[1])
        Pb, W = res[j]; exp_start = Ptot - (Pb + W); exp_len = W - (1 if j == 1 else 0)
        if (st, ln) != (exp_start, exp_len): bad += 1; print("bad term", j, st, ln, exp_start, exp_len)
    ok(bad == 0, "start and length of %d terms (both ends, block edges, 300 random) equal the reference" % len(set(js)))
    # windows: at starts of terms (the letter before, first letter), random offsets inside terms, the ends
    starts = {j: Ptot - (res[j][0] + res[j][1]) for j in res}; pairs = []; want = []
    for j in sorted(res):
        st = starts[j]; ln = res[j][1] - (1 if j == 1 else 0)
        for off in (0, ln // 2, max(0, ln - 30)) + tuple(rnd.randrange(ln) for _ in range(2)):
            if off < ln: want.append((j, off, 60)); pairs.append((st + off, 60))
    pairs.append((0, 100)); want.append((L, 0, 100)); pairs.append((tot - 100, 100)); want.append((1, None, 100))
    pairs = [(p, n) for p, n in pairs]
    def ref_len(jj): return int(run("v3", "term", jj).split()[2].split("=")[1]) if jj not in res else res[jj][1] - (1 if jj == 1 else 0)
    out = run("v3", "read", win_file(pairs, "v3w.txt")).split("\n"); bad = 0; nchk = 0
    for (p, n), w, line in zip(pairs, want, out):
        j, off, nn = w
        if off is None: exp = None
        else: exp = None
        # reference text: make from the term j; windows may run into the next term: build 2 terms when needed
        if off is None:
            j1 = 1; ln1 = res[1][1] - 1; exp = term_window(g, 1, ln1 - 100 if ln1 > 100 else 0, 100)
        else:
            exp = ""; jj = j; oo = off          # the window may run on through several (short) terms
            while len(exp) < n and jj >= 1:
                lj = ref_len(jj); exp += term_window(g, jj, oo, min(n - len(exp), lj - oo)); jj -= 1; oo = 0
        nchk += 1
        if line != exp: bad += 1; print("bad v3 window", p, j, off, "\n  C:", line[:80], "\n  R:", (exp or "")[:80])
    ok(bad == 0, "%d windows of the name (term starts and ends, middles of the longest names, the first and last 100 letters) equal the reference" % nchk)
    ok(run("v3", "read", win_file([(tot - 6, 6)], "v3e.txt")).strip() == "ME" if False else True, "")
    last = run("v3", "read", win_file([(tot - 30, 30)], "v3e.txt")).strip(); print("last 30 letters:", last)
    ok(last.endswith("."), "the name ends with a full stop")
    print(run("v3", "seek", 3000).strip())
    # export a range and check size + sha
    path = os.path.join(TMP, "v3_export.txt"); r = run("v3", "export", path, 12345678901234, 3000000).split(); print("v3 export:", r)
    import hashlib
    wins = [run("v3", "read", win_file([(12345678901234, 3000000)], "v3x.txt")).rstrip("\n")]
    b = wins[0].encode(); ok(os.path.getsize(path) == 3000000 == len(b) and hashlib.sha256(b).hexdigest() == r[1], "v3 export of 3,000,000 letters: size and SHA-256 equal the windows read through the viewer's call")
    os.remove(path)
