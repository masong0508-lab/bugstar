#!/bin/bash
# Tests for the PC sim layer's pure C code (no game needed): Lv, god powers, the sim core. Builds into pc/sim/out (not committed).
cd "$(dirname "$0")"; mkdir -p out
CC="gcc -std=gnu11 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -Wno-format-truncation -fwrapv"
set -e
$CC lv_test.c lv.c -o out/lv_test
./out/lv_test
$CC god_test.c god.c lv.c -o out/god_test
./out/god_test
$CC lvxp_test.c lvxp.c lv.c -o out/lvxp_test
./out/lvxp_test
$CC sim_test.c mine.c sim_core.c colony.c god.c lv.c -o out/sim_test
[ -f sim.golden ] || ./out/sim_test --make-golden
./out/sim_test
$CC env_test.c mine.c sim_core.c colony.c god.c lv.c -o out/env_test
./out/env_test
$CC social_test.c mine.c sim_core.c colony.c god.c lv.c -o out/social_test
./out/social_test
$CC ties_test.c mine.c sim_core.c colony.c god.c lv.c -o out/ties_test
./out/ties_test
$CC life_test.c mine.c sim_core.c colony.c god.c lv.c -o out/life_test
./out/life_test
$CC colony_test.c mine.c sim_core.c colony.c god.c lv.c -o out/colony_test
./out/colony_test
$CC standing_test.c mine.c sim_core.c colony.c god.c lv.c -o out/standing_test
./out/standing_test
$CC unlock_test.c mine.c sim_core.c colony.c god.c lv.c -o out/unlock_test
./out/unlock_test
$CC mine_test.c mine.c sim_core.c colony.c god.c lv.c -o out/mine_test
./out/mine_test
$CC mine_bal.c mine.c sim_core.c colony.c god.c lv.c -o out/mine_bal -include stdlib.h
./out/mine_bal 600 8 | tail -1     # the long run (1500 days x 30 seeds) is how TUNING.md patch 45 was measured
# against Python's own big integers: random and edge numbers, all operations, including saturation at 64^256
python3 - <<'PY' > out/lv_cases.txt
import random
random.seed(1234)
CAP = 1 << 1536
edge = [0, 1, 2, 3, 63, 64, 65, (1<<64)-1, 1<<64, (1<<64)+1, (1<<128)-1, 1<<128, CAP-1, CAP, CAP//2, CAP//2+1, CAP//3, 10**100, 10**462, 64**255, 64**255*63]
def rnd():
    r = random.random()
    if r < .3: return random.choice(edge)
    bits = random.choice([1, 8, 31, 32, 63, 64, 65, 127, 128, 300, 700, 767, 768, 769, 1000, 1500, 1535, 1536])
    return min(random.getrandbits(bits), CAP)
for i in range(3000):
    a, b = rnd(), rnd()
    for op in ("add", "sub", "mul", "cmp"): print(op, a, b)
    m = random.choice([1, 2, 3, 10, 63, 64, 65, 1000, 65535, 65536, 2**31, 2**32-1, random.getrandbits(32) or 1])
    print("muls", a, m); print("divs", a, m)
    print("shl", a, random.randrange(0, 1700)); print("shr", a, random.randrange(0, 1700))
    print("b64", a, 0); print("compact", a, random.randrange(5, 13))
PY
python3 - <<'PY'
import subprocess, sys
CAP = 1 << 1536
cases = [l.split() for l in open("out/lv_cases.txt")]
res = subprocess.run(["./out/lv_test", "--batch"], stdin=open("out/lv_cases.txt"), capture_output=True, text=True).stdout.split("\n")
bad = 0
def compact(v, ml):
    s = str(v)
    if len(s) <= ml: return s
    e = len(s) - 1; es = str(e); frac = max(0, min(ml - (2 + len(es)) - 1, len(s) - 1))
    return s[0] + ("." + s[1:1+frac] if frac else "") + "E" + es
for c, r in zip(cases, res):
    op, a, b = c[0], int(c[1]), int(c[2])
    if op == "add": want = (min(a + b, CAP), int(a + b > CAP))
    elif op == "sub": want = (max(a - b, 0), int(a < b))
    elif op == "mul": want = (min(a * b, CAP), int(a * b > CAP))
    elif op == "muls": want = (min(a * b, CAP), int(a * b > CAP))
    elif op == "divs": want = (a // b, a % b)
    elif op == "cmp": want = ((a > b) - (a < b), 0)
    elif op == "shl": want = (min(a << b, CAP) if a else 0, int(a << b > CAP))
    elif op == "shr": want = (a >> b, 0)
    elif op == "b64": want = (a, 0)
    elif op == "compact": want = (compact(a, b), 0)
    got = r.split()
    if str(want[0]) != got[0] or str(want[1]) != got[1]:
        bad += 1
        if bad < 6: print("MISMATCH", op, a if a < 10**40 else "big", b, "want", str(want[0])[:50], want[1], "got", got[0][:50], got[1])
print("lv vs python bigint: %d cases, %d mismatches" % (len(cases), bad))
sys.exit(1 if bad or len(res) < len(cases) else 0)
PY
# xp to level against Python (exact formula, binary search done in Python's own integers)
python3 - <<'PY'
import random, subprocess, sys
random.seed(99)
CAP = 1 << 1536
def cost(L, n, pct):
    S = 18*n*L*L + 18*L*n*(n-1) + 3*n*(n-1)*(2*n-1) + 12*n
    if S > CAP: return None
    T = S*pct//100
    return None if T > CAP else T
def gain(L, X, pct):
    if L >= CAP: return CAP, 0, 0
    lo, hi = 0, CAP - L
    while lo < hi:
        mid = lo + (hi - lo + 1)//2
        c = cost(L, mid, pct)
        if c is not None and c <= X: lo = mid
        else: hi = mid - 1
    gained = lo
    if lo == 0 and X == CAP and cost(L, 1, pct) is None: gained = 1; L += 1; X = 0
    elif lo: X -= cost(L, lo, pct); L += lo
    if L >= CAP: L, X = CAP, 0
    return L, X, gained
cases = []
for i in range(2500):
    bits = random.choice([1, 5, 20, 31, 32, 63, 64, 65, 100, 255, 400, 511, 512, 513, 700, 765, 766, 767, 800, 1000, 1400, 1535, 1536])
    L = max(1, random.getrandbits(bits)); L = min(L, CAP)
    xb = random.choice([1, 10, 40, 64, 100, 128, 300, 700, 1000, 1500, 1536])
    X = min(random.getrandbits(xb), CAP)
    if random.random() < .05: X = CAP
    if random.random() < .05: L = CAP - random.randrange(0, 3)
    cases.append((L, X, random.choice([100, 100, 100, 25, 50, 150, 200, 400])))
inp = "".join("%d %d %d\n" % c for c in cases)
out = subprocess.run(["./out/lvxp_test", "--batch"], input=inp, capture_output=True, text=True).stdout.split("\n")
bad = 0
for c, line in zip(cases, out):
    L, X, g = gain(*c); want = "%d %d %d" % (L, X, g)
    if line != want:
        bad += 1
        if bad < 4: print("MISMATCH L bits", c[0].bit_length(), "X bits", c[1].bit_length(), "pct", c[2], "\n want", want[:80], "\n got ", line[:80])
print("lvxp vs python: %d cases, %d mismatches" % (len(cases), bad))
sys.exit(1 if bad or len(out) < len(cases) else 0)
PY
