#!/usr/bin/env python3
# The Python REFERENCE for the Stormclock Coil's numbers (coilnum.inc). It uses exact decimal maths (60 digits) and prints one line per case:
#     KIND input... => expected text
# The C test (pc/tests/cn_check.inc, run by coil.sh) makes the same text with the game's own wide-number code and compares every line.
#   python3 coilref.py            -> the table for every tier, printed for reading
#   python3 coilref.py FILE       -> writes the test cases to FILE
# Kinds: WFMT / WORDS / KMPS (a whole-number speed in c, written m e: m x 10^e), RATE k and TRIP k (a speed in c as a decimal), TIME (seconds, a decimal).
import sys, random
from decimal import Decimal as D, getcontext, ROUND_HALF_UP, ROUND_FLOOR
getcontext().prec = 80

C_KMS = D("299792.458")            # km per second, exact
YEAR = D(31557600)                 # a Julian year in seconds
PLANCK = D("5.391247e-44")         # CODATA 2018
BIG = ["", "THOUSAND", "MILLION", "BILLION", "TRILLION", "QUADRILLION", "QUINTILLION", "SEXTILLION", "SEPTILLION", "OCTILLION", "NONILLION", "DECILLION",
       "UNDECILLION", "DUODECILLION", "TREDECILLION", "QUATTUORDECILLION", "QUINDECILLION", "SEXDECILLION", "SEPTENDECILLION", "OCTODECILLION", "NOVEMDECILLION", "VIGINTILLION"]
RATES = ["EARTH", "STAR", "MW", "GAL", "UNI"]
TRIPS = ["MOON", "SUN", "PROX", "MW", "AND", "UNI", "GAME"]
TIERS = [("LIGHT", 1, 0), ("KILO", 1, 3), ("MEGA", 1, 6), ("GIGA", 1, 9), ("TERA", 1, 12), ("LIMIT", 13, 12), ("STORM", 23759, 39)]

def adj(v): return v.adjusted()
def rnd(v, sig):                       # first 'sig' digits rounded half up, and the exponent of the first digit
    X = adj(v); q = (v.scaleb(-(X - sig + 1))).quantize(D(1), rounding=ROUND_HALF_UP)
    if q >= 10 ** sig: q = q // 10; X += 1
    return int(q), X
def comma(n): return "{:,}".format(n)
def sci(r, sig, X):
    d = str(r).rjust(sig, "0"); d = d.rstrip("0") or "0"; d = d if len(d) else "0"
    s = d[0] + ("." + d[1:] if len(d) > 1 else ""); return "%s X 10^%d" % (s, X)
def dec(r, sig, s):                    # the point after s+1 digits, trailing zeros of the fraction trimmed, padded with zeros if the number is short
    d = str(r).rjust(sig, "0"); head = d[:s + 1].ljust(s + 1, "0"); frac = d[s + 1:].rstrip("0")
    return head + ("." + frac if frac else "")
def sn(v):
    if v == 0: return "0"
    r, x = rnd(v, 5)
    if x > 5 or x < -3: return sci(r, 5, x)
    if x >= 4: return comma(int(v.quantize(D(1), rounding=ROUND_HALF_UP)))
    if x >= 0: return dec(r, 5, x)
    d = str(r).rstrip("0"); return "0." + "0" * (-x - 1) + d
def wfmt(value):                       # value: int
    if value < 10 ** 19: return comma(value)
    s = str(value)[:10]; return sci(int(s), 10, len(str(value)) - 1)
def words(v):
    r, x = rnd(v, 5)
    if x < 6: return sn(v)
    return dec(r, 5, x % 3) + " " + BIG[x // 3]
def unit(name, one): return " " + name + ("" if one else "S")
SU = ["SECOND", "MILLISECOND", "MICROSECOND", "NANOSECOND", "PICOSECOND", "FEMTOSECOND", "ATTOSECOND", "ZEPTOSECOND", "YOCTOSECOND"]
def timetxt(t):
    if t == 0: return "NO TIME AT ALL"
    if t < PLANCK: return "LESS THAN ONE PLANCK TIME"
    X = adj(t)
    if X <= -25:
        q = t / PLANCK
        if adj(q) < 3:
            r, x = rnd(q, 3); return dec(r, 3, x) + unit("PLANCK TIME", r == 100 and x == 0)
        r, x = rnd(t, 3); return sci(r, 3, x) + " SECONDS"
    if X < 0:
        r, x = rnd(t, 3); k = (-x + 2) // 3; s = x + 3 * k
        return dec(r, 3, s) + unit(SU[k], r == 100 and s == 0)
    if t < 60: v, u = t, "SECOND"
    elif t < 3600: v, u = t / 60, "MINUTE"
    elif t < 86400: v, u = t / 3600, "HOUR"
    elif t < YEAR: v, u = t / 86400, "DAY"
    else: v, u = t / YEAR, "YEAR"
    r, x = rnd(v, 3)
    if u != "YEAR" or x < 3: return dec(r, 3, x) + unit(u, r == 100 and x == 0)
    if x < 6: return comma(r * 10 ** (x - 2)) + " YEARS"
    return dec(r, 3, x % 3) + " " + BIG[x // 3] + " YEARS"
LEN = {"EARTH": D(12742000) / 299792458, "STAR": 4 * YEAR, "MW": 100000 * YEAR, "GAL": 3000000 * YEAR, "UNI": D("9.3e10") * YEAR}     # light seconds across one of them
def rate(k, c):
    v = c / LEN[k]
    if v >= 1: return sn(v) + " PER SECOND"
    return "ONE EVERY " + timetxt(LEN[k] / c)
LIGHT = {"MOON": D(384400000) / 299792458, "SUN": D(149597870700) / 299792458, "PROX": D("4.2465") * YEAR, "MW": D(100000) * YEAR,
         "AND": D("2.537e6") * YEAR, "UNI": D("9.3e10") * YEAR, "GAME": D(25) * 2 ** 155 * YEAR}
def trip(k, c): return timetxt(LIGHT[k] / c)
def cases():
    L = []
    speeds = []                        # whole-number speeds as (m, e)
    for nm, m, e in TIERS: speeds.append((m, e))
    for nm, m, e in TIERS: speeds.append((m * 1000000, e))        # the x1 million cheat
    speeds += [(7, 0), (950, 0), (12345, 0), (1, 6), (999999999999999999, 0), (9999999999999999999, 0), (1, 19), (18446744073709551615, 0), (123456789012345, 6), (5, 100)]
    random.seed(20251101)
    for i in range(300):
        d = random.randint(0, 48); m = random.randint(1, 10 ** random.randint(1, 10) - 1); speeds.append((m, max(0, d - len(str(m)) + 1)))
    for m, e in speeds:
        if m * 10 ** e >= 10 ** 19 and len(str(m)) > 10: m, e = int(str(m)[:10]), e + len(str(m)) - 10   # (the game keeps 10 digits above 10^19)
        v = D(m) * D(10) ** e
        L.append(("WFMT %de%d" % (m, e), wfmt(int(v))))
        if rnd(v, 5)[1] // 3 < len(BIG): L.append(("WORDS %de%d" % (m, e), words(v)))      # (bigger names are made by the game's own naming rules)
        L.append(("KMPS %de%d" % (m, e), sn(v * C_KMS)))
        for k in RATES: L.append(("RATE %s %de%d" % (k, m, e), rate(k, v)))
        for k in TRIPS: L.append(("TRIP %s %de%d" % (k, m, e), trip(k, v)))
    for c in ["0.001", "0.01", "0.42", "0.6", "0.999", "1.5"]:      # the ship's own speed below 1 c, as a decimal
        v = D(c)
        for k in RATES: L.append(("RATE %s %s" % (k, c.replace(".", "d")), rate(k, v)))
        for k in TRIPS: L.append(("TRIP %s %s" % (k, c.replace(".", "d")), trip(k, v)))
        L.append(("KMPS %s" % c.replace(".", "d"), sn(v * C_KMS)))
    ts = ["1e-60", "5.391247e-44", "5.4e-44", "5.3e-44", "1e-43", "5.39e-41", "5.4e-41", "9.99e-25", "1e-24", "1.5e-24", "5e-21", "2.5e-19", "1e-9", "4.57e-7", "0.0004999", "0.9996", "0.5", "1", "1.28225", "59.9", "60", "3599", "3600", "86399", "86400", "31557599", "31557600",
          "63115200", "3.1557e10", "3.15576e12", "9.9996e8", "1e18", "2.934e18", "1.1e58", "9.999e20", "123456789", "1e30", "1.2345e50"]
    for t in ts: L.append(("TIME %s" % t, timetxt(D(t))))
    for i in range(400):
        m = random.randint(10 ** 9, 10 ** 10 - 1); e = random.randint(-71, 51); L.append(("TIME %de%d" % (m, e), timetxt(D(m) * D(10) ** e)))
    # the speed arithmetic (ramp up, ramp down, the cheat multipliers): expected = exact result, the game keeps 10 digits (compared to within 1e-8)
    def rv():
        d = random.choice([0, 0, 3, 6, 12, 18, 25, 40, 43, 49]); m = random.randint(1, 10 ** random.randint(1, 10) - 1); return m, max(0, d - len(str(m)) + 1)
    for i in range(300):
        (m1, e1), (m2, e2) = rv(), rv(); a = D(m1) * D(10) ** e1; b = D(m2) * D(10) ** e2
        L.append(("OPADD %de%d %de%d" % (m1, e1, m2, e2), "%d" % (a + b)))
        L.append(("OPSUB %de%d %de%d" % (m1, e1, m2, e2), "%d" % max(a - b, 0)))
        k = random.choice([2, 3, 4, 12, 1000, 1000000, 999999999, random.randint(1, 10 ** 9)])
        L.append(("OPMUL %de%d %d" % (m1, e1, k), "%d" % (a * k)))
        L.append(("OPDIV %de%d %d" % (m1, e1, k), "%d" % int(a / k)))
    return L

def show():
    print("TIER      SPEED (x c)            KM/S                 UNIVERSE (93 BILLION LY)   MOON")
    for nm, m, e in TIERS:
        v = D(m) * D(10) ** e
        print("%-6s %-22s %-20s %-26s %s" % (nm, wfmt(int(v)), sn(v * C_KMS), trip("UNI", v), trip("MOON", v)))
    for nm, m, e in TIERS[-2:]:
        v = D(m) * D(10) ** e
        for k in TRIPS: print("  %s %-6s %s" % (nm, k, trip(k, v)))
        for k in RATES: print("  %s %-6s %s" % (nm, k, rate(k, v)))

if __name__ == "__main__":
    if len(sys.argv) > 1:
        with open(sys.argv[1], "w") as f:
            for a, b in cases(): f.write("%s => %s\n" % (a, b))
        print("coilref: %d cases" % len(cases()))
    else: show()
