#!/bin/bash
# Tests for the PC sim layer's pure C code (no game needed): Lv, god powers, the sim core. Builds into pc/sim/out (not committed).
cd "$(dirname "$0")"; mkdir -p out
CC="gcc -std=gnu11 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -fwrapv"
set -e
$CC lv_test.c lv.c -o out/lv_test
./out/lv_test
$CC god_test.c god.c lv.c -o out/god_test
./out/god_test
$CC sim_test.c sim_core.c god.c lv.c -o out/sim_test
[ -f sim.golden ] || ./out/sim_test --make-golden
./out/sim_test
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
