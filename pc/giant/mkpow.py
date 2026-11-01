#!/usr/bin/env python3
"""Builds the digit file of  2^(2^32)  (4,294,967,296 doublings), the GIANT NUMBER of the BUGSTAR PC viewer.
2^(2^32) is the number of different things 512 MiB of memory (2^32 bits) can hold.  It has exactly 1,292,913,987 decimal digits.
Method: Python's decimal module (libmpdec: exact arithmetic with number-theoretic-transform multiplication) with a precision large
enough that no rounding ever happens (the result is checked to be exact: Inexact and Rounded are trapped).  Needs about 5 GB of RAM and
10 to 20 minutes.  Output: packed decimal, two digits to a byte, after a 64-byte header (see pc/giant/README or giant.h).
usage: mkpow.py OUTFILE"""
import sys, decimal, time, struct
from decimal import Decimal, Context
E = 32; N = 1 << E
DIG = 1292913987
out = sys.argv[1]
t0 = time.time()
c = Context(prec=DIG + 10, Emax=decimal.MAX_EMAX, Emin=decimal.MIN_EMIN, traps=[decimal.InvalidOperation, decimal.Inexact, decimal.Rounded, decimal.Overflow])
x = Decimal(2)
for i in range(E):                       # square 32 times: 2, 4, 16, 256, ... 2^(2^32)
    x = c.multiply(x, x)
    print("square", i + 1, "of", E, "%.0fs" % (time.time() - t0), file=sys.stderr, flush=True)
assert x.as_tuple().exponent == 0 and x.adjusted() + 1 == DIG, (x.adjusted() + 1)
s = str(x); del x
assert len(s) == DIG and s[0] != '0'
print("converted", len(s), "%.0fs" % (time.time() - t0), file=sys.stderr, flush=True)
with open(out, "wb") as f:
    f.write(b"BSGPOW2\0" + struct.pack("<QQQ", E, DIG, 0).ljust(56, b"\0"))
    B = 1 << 26
    for i in range(0, DIG, B):
        ch = s[i:i + B]
        if len(ch) & 1: ch += "0"                 # (only the last chunk can be odd)
        f.write(bytes.fromhex(ch))
print("done %.0fs" % (time.time() - t0), file=sys.stderr, flush=True)
