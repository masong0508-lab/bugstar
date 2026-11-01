# Helpers for the PC pages (pc/facts/NN_*.py): numbers worked out by code, so the values are right.
import math, decimal
from decimal import Decimal
def A(m, e): return int(Decimal(str(m)).scaleb(e))               # A(3.8, 26) = 3.8 x 10^26
def P10(e): return lambda: "1" + "0" * e                           # 1 followed by e zeros, as digits (any size)
def P2(e, minus=0):                                                # 2^e - minus, as digits (any size; no 4300-digit limit)
    def f():
        c = decimal.Context(prec=int(e * 0.30103) + 20, Emax=decimal.MAX_EMAX, traps=[])
        r = c.power(Decimal(2), e)
        return str(c.subtract(r, Decimal(minus)) if minus else r)
    return f
def first_pow2_with(digits): return math.ceil((digits - 1) / math.log10(2))   # smallest e with 2^e having `digits` digits
def FACT(n):                                                       # n! as digits (any size)
    return lambda: str(_big(math.factorial(n)))
def _big(v):
    import sys
    if hasattr(sys, "set_int_max_str_digits"): sys.set_int_max_str_digits(0)
    return v
def fib(n):
    a, b = 0, 1
    for _ in range(n): a, b = b, a + b
    return a
def lucas(n):
    a, b = 2, 1
    for _ in range(n): a, b = b, a + b
    return a
def bell(n):
    row = [1]
    for _ in range(n - 1):
        new = [row[-1]]
        for x in row: new.append(new[-1] + x)
        row = new
    return row[-1]
def partitions(n):
    p = [1] + [0] * n
    for k in range(1, n + 1):
        for j in range(k, n + 1): p[j] += p[j - k]
    return p[n]
def catalan(n): return math.comb(2 * n, n) // (n + 1)
def nprimes_below(n):                                              # pi(n) by a sieve (n up to about 10^8)
    s = bytearray([1]) * (n + 1); s[0:2] = b"\0\0"
    for i in range(2, int(n ** 0.5) + 1):
        if s[i]: s[i * i::i] = bytearray(len(s[i * i::i]))
    return sum(s)
def digits_of(v): return len(str(_big(v)))
