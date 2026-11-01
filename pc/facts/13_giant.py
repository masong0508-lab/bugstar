from helpers import *
import math, decimal, sys
from decimal import Decimal, Context
if hasattr(sys, "set_int_max_str_digits"): sys.set_int_max_str_digits(0)

# Pages about the GIANT NUMBERS (pc/giant): the viewer's numbers, the -illion names, and factorials and towers worked out exactly.
# Everything here is computed by code: exact integers, or exact digit counts and leading digits from high-precision logarithms (Decimal, 150+ digits).
# The V3 totals (letters in the whole name) are measured by pc/giant/test.sh against the real file and written down in giant_numbers.txt.
def C(p): return Context(prec=p, Emax=decimal.MAX_EMAX, Emin=decimal.MIN_EMIN)
LOG2 = C(400).log10(Decimal(2)); LOG3 = C(400).log10(Decimal(3))
def digits_and_lead(x_log, nlead):          # x_log = log10 of a number (a Decimal with enough precision): (digit count, its first nlead digits)
    ip = int(x_log); fr = C(500).subtract(x_log, Decimal(ip))
    lead = int(C(400).multiply(C(400).power(Decimal(10), fr), Decimal(10) ** (nlead - 1)).to_integral_value(rounding=decimal.ROUND_FLOOR))
    return ip + 1, lead
PI = Decimal("3.14159265358979323846264338327950288419716939937510582097494459230781640628620899862803482534211706798214808651328230664709384460955058223172535940812848111745028410270193852110555964462294895493038196")
def _bern(m):                               # Bernoulli numbers B_2, B_4 ... B_2m as fractions
    from fractions import Fraction
    B = [Fraction(0)] * (2 * m + 1); B[0] = Fraction(1)
    for n in range(1, 2 * m + 1):
        B[n] = -sum(math.comb(n + 1, k) * B[k] for k in range(n)) / (n + 1)
    return B
_B = _bern(24)
def lnfact(n, prec):                        # ln(n!) by Stirling's series with 24 Bernoulli terms (n >= 1000: the error is below 10^-90)
    c = C(prec); n = Decimal(n)
    r = c.subtract(c.multiply(n, c.ln(n)), n)
    r = c.add(r, c.divide(c.ln(c.multiply(c.multiply(Decimal(2), PI), n)), Decimal(2)))
    for k in range(1, 25):
        b = _B[2 * k]; t = c.divide(Decimal(b.numerator), c.multiply(Decimal(b.denominator * 2 * k * (2 * k - 1)), c.power(n, 2 * k - 1))); r = c.add(r, t)
    return r
def fact_digits_lead(n, nlead=30):
    lg = C(500).divide(lnfact(n, 500), C(500).ln(Decimal(10))); return digits_and_lead(lg, nlead)
def fact_zeros(n):
    z = 0; p = 5
    while p <= n: z += n // p; p *= 5
    return z
def totient(n):
    r = n; p = 2; m = n
    while p * p <= m:
        if m % p == 0:
            while m % p == 0: m //= p
            r -= r // p
        p += 1
    if m > 1: r -= r // m
    return r
LIM = 1 << 20
def tower(a, h, m):                         # a^a^...^a (h times) mod m, and the true value if it is small (else LIM): the generalized Euler theorem
    if h == 1: return a % m, min(a, LIM)
    phi = totient(m); em, ec = tower(a, h - 1, phi)
    if ec < LIM:
        e = ec; vc = a ** e if e * math.log2(a) < 21 else LIM; return pow(a, e, m), min(vc, LIM)
    e = em + phi * ((64 + phi - 1) // phi); return pow(a, e, m), LIM
def tw_mod(a, h, m): return tower(a, h, m)[0]

# ---- numbers
D2 = 1 << 32; dg32, lead32 = digits_and_lead(C(400).multiply(Decimal(D2), LOG2), 40)
assert dg32 == 1292913987
last40 = pow(2, D2, 10 ** 40); assert last40 >= 10 ** 39
dg21 = digits_and_lead(C(400).multiply(Decimal(1 << 21), LOG2), 1)[0]; assert dg21 == 631306
t33 = 3 ** 27; assert t33 == 7625597484987
dg34, lead34 = digits_and_lead(C(400).multiply(Decimal(t33), LOG3), 30)
assert dg34 == 3638334640025 and str(lead34).startswith("1258014")
last34 = tw_mod(3, 4, 10 ** 30); assert last34 == pow(3, t33, 10 ** 30)
last35 = tw_mod(3, 5, 10 ** 30); assert last35 >= 10 ** 29
GR10 = tw_mod(3, 40, 10 ** 10); assert GR10 == tw_mod(3, 41, 10 ** 10) == tw_mod(3, 90, 10 ** 10) == 2464195387      # Graham's number ends ...2464195387 (a known result)
GR20 = tw_mod(3, 60, 10 ** 18); assert GR20 == tw_mod(3, 61, 10 ** 18) == tw_mod(3, 100, 10 ** 18) and GR20 >= 10 ** 17
f6d, f6l = fact_digits_lead(10 ** 6); assert f6d == 5565709 and str(f6l).startswith("82639316883")
f5d, f5l = fact_digits_lead(10 ** 5); assert f5d == len(str(math.factorial(10 ** 5))) and str(math.factorial(10 ** 5))[:30] == str(f5l)
f9d, f9l = fact_digits_lead(10 ** 9); f12d, f12l = fact_digits_lead(10 ** 12)
fgd, fgl = fact_digits_lead(10 ** 100, 40); assert len(str(fgd)) == 102
def last_nonzero(n, k):                       # the last k digits of n! before its trailing zeros, exactly (n up to a few million)
    v = math.factorial(n); z = fact_zeros(n); return (v // 10 ** z) % 10 ** k
LN6 = last_nonzero(10 ** 5, 20)
assert str(math.factorial(10 ** 5)).rstrip("0")[-20:] == "%020d" % LN6
GOOGOLPLEX_NAME = 769                         # "TEN" + 766 letters (checked against the published figure in pc/giant/test_giant.py)

PAGES = [
 (dg32, "DIGITS IN 2^(2^32)", "EVERY STATE OF 512 MIB OF MEMORY. IT STARTS " + str(lead32)[:8] + ".", "(THE GIANT NUMBER VIEWER SHOWS EVERY ONE OF THEM)", 0),
 (lead32, "FIRST 40 DIGITS OF 2^(2^32)", "FROM A LOGARITHM WORKED OUT TO 400 PLACES.", "(THE VIEWER'S FILE STARTS THE SAME WAY)", 0),
 (last40, "LAST 40 DIGITS OF 2^(2^32)", "FROM POW(2, 2^32, 10^40): THE LAST DIGITS COME CHEAP.", "(THE MIDDLE ONES TOOK 20 MINUTES AND 5 GB)", 0),
 (dg32 // 3, "GROUPS OF 3 IN 2^(2^32)", "THE VIEWER'S COMMAS SPLIT IT INTO THIS MANY GROUPS.", "(THE LAST GROUP ENDS IN 6: A POWER OF TWO)", 0),
 (dg21, "DIGITS IN 2^(2^21)", "EVERY STATE OF A GBA'S 256 KIB OF MAIN RAM (2^21 BITS).", "(THE PC'S 512 MIB HAS 2,048 TIMES THE DIGITS)", 0),
 (1000000008, "THE COUNTING NUMBER'S DIGITS", "1 2 3 ... 123,456,790 WRITTEN IN A ROW.", "(CHAMPERNOWNE'S CONSTANT, CUT AFTER A BILLION DIGITS)", 0),
 (123456790, "WHERE THE COUNTING STOPS", "THE LAST NUMBER WRITTEN IN THE COUNTING NUMBER.", "(CHOSEN SO IT HAS 1,000,000,008 DIGITS)", 0),
 (GOOGOLPLEX_NAME, "LETTERS IN A GOOGOLPLEX NAME", "TEN PLUS A 766-LETTER -ILLION. CONWAY AND GUY: ABOUT 769.", "(OUR CODE GIVES THE SAME NUMBER OF LETTERS)", 0),
 (19, "LETTERS IN A GOOGOL'S NAME", "TEN DUOTRIGINTILLION: 10^100 IS 10 X 1000^33.", "(THE -ILLION IS THE 32ND, NOT THE 100TH)", 0),
 (3871, "NOLL'S GOOGOLPLEX WORD", "LANDON CURT NOLL'S SCRIPT NAMES IT WITH 3,871 LETTERS.", "(A DIFFERENT SYSTEM FROM CONWAY AND WECHSLER)", 0),
 (11896480, "PARTS IN BUGSTARILLION V2", "EACH PART IS THREE DIGITS AT ITS OWN -ILLION PLACE.", "(V3 HAS 1,292,913,987 OF THEM)", 0),
 (8888090387, "LETTERS IN V2 PLACE NAMES", "ADDING UP EVERY PLACE NAME OF THE 11.9 MILLION PARTS.", "(V3'S NAME IS ABOUT 700 MILLION TIMES LONGER)", 0),
 (1292913987, "TERMS IN BUGSTARILLION V3", "ONE FOR EACH DIGIT OF 2^(2^32): THE J-TH SAYS THE", "FIRST J DIGITS AS AN -ILLION NUMBER", 0),
 (2568, "DIGITS IN 1,000 FACTORIAL", "THE EXACT NUMBER OF WAYS TO LINE UP 1,000 THINGS.", "(IT WOULD FILL ABOUT SIX PAGES OF THIS GAME'S TEXT)", 0),
 (f5d, "DIGITS IN 100,000 FACTORIAL", "STARTS " + str(f5l)[:8] + ". WORKED OUT BOTH EXACTLY AND BY LOGS.", "(THE TWO WAYS AGREE ON ALL OF IT)", 0),
 (f6d, "DIGITS IN 10^6 FACTORIAL", "STARTS " + str(f6l)[:10] + ". ENDS IN " + str(fact_zeros(10 ** 6)) + " ZEROS.", "(FROM STIRLING'S FORMULA TO 500 PLACES)", 0),
 (f6l, "LEADING DIGITS OF 10^6 FACT.", "THE LEADING DIGITS OF A MILLION FACTORIAL.", "(THE TRAILING ZEROS COME FROM COUNTING FIVES)", 0),
 (f9d, "DIGITS IN 10^9 FACTORIAL", "STARTS " + str(f9l)[:10] + ".", "(MORE DIGITS THAN THE GIANT NUMBER VIEWER'S NUMBERS)", 0),
 (f12d, "DIGITS IN 10^12 FACTORIAL", "STARTS " + str(f12l)[:10] + ".", "(A TRILLION THINGS, LINED UP IN EVERY WAY)", 0),
 (fgd, "DIGITS IN A GOOGOL FACTORIAL", "A NUMBER WITH 102 DIGITS TELLS YOU HOW MANY DIGITS.", "(STARTS " + str(fgd)[:8] + " AND SOME MORE)", 0),
 (fact_zeros(10 ** 100), "ZEROS ON GOOGOL FACTORIAL", "COUNTING FIVES: N OVER 5, N OVER 25, N OVER 125 ...", "(ONE QUARTER OF A GOOGOL, ALMOST)", 0),
 (fact_zeros(10 ** 9), "ZEROS ENDING 10^9 FACTORIAL", "THE SUM OF 10^9 DIVIDED BY 5, 25, 125 ... EXACTLY.", "(JUST BELOW A QUARTER OF A BILLION)", 0),
 (LN6, "LAST DIGITS OF 100,000 FACT.", "THE LAST 20 DIGITS BEFORE THE ZEROS (BIG ARITHMETIC).", "(EVERY 2 AND 5 PAIRED OFF: THIS IS WHAT IS LEFT)", 0),
 (t33, "3 TO THE 3 TO THE 3", "3^27. THE TOWER OF THREE THREES.", "(3^^4 IS 3 TO THE POWER OF THIS)", 0),
 (dg34, "DIGITS IN 3^^4", "3 TO THE 7,625,597,484,987. STARTS 1.2580143.", "(A COMPUTER CAN WRITE IT IN FULL ONLY AS A CATALOGUE)", 0),
 (lead34, "FIRST 30 DIGITS OF 3^^4", "FROM A LOGARITHM WORKED OUT TO 400 PLACES.", "(3^^4 IS 3 TO THE 3 TO THE 3 TO THE 3)", 0),
 (last34, "LAST 30 DIGITS OF 3^^4", "FROM POW(3, 3^27, 10^30), EXACTLY.", "(THE MIDDLE OF IT HAS 3.6 TRILLION DIGITS)", 0),
 (last35, "LAST 30 DIGITS OF 3^^5", "A TOWER OF FIVE THREES, BY EULER'S THEOREM.", "(THE WHOLE NUMBER HAS ABOUT 10^(3.6 TRILLION) DIGITS)", 0),
 (GR10, "GRAHAM'S NUMBER: LAST 10", "ENDS ...2464195387. A TOWER OF 3S ENDS THE SAME WAY.", "(THE NUMBER ITSELF CANNOT BE WRITTEN IN THIS UNIVERSE)", 0),
 (GR20, "GRAHAM'S NUMBER: LAST 18", "THE LAST 18 DIGITS, FROM THE SAME TOWER RULE.", "(EXACT. THE DIGITS BEFORE THEM ARE BEYOND ANY PRINT)", 0),
 (P2(65536), "2 TO THE 65,536 (2^^5)", "19,729 DIGITS. 2^^6 IS 2 TO THE POWER OF THIS.", "(2^^6 HAS A 19,729-DIGIT COUNT OF DIGITS)", 0),
 (10 ** 100 + 1, "DIGITS IN A GOOGOLPLEX", "10^(10^100) HAS A GOOGOL AND ONE DIGITS.", "(THE GIANT NUMBER VIEWER'S NUMBERS HAVE ONLY A BILLION)", 0),
 (10 ** 102 + 1, "DIGITS IN GOOGOL TO A GOOGOL", "(10^100) TO THE (10^100) IS 10^(100 X 10^100).", "(THE DIGIT COUNT HAS 103 DIGITS)", 0),
 (43252003274489856000, "RUBIK CUBE POSITIONS (3X3)", "COUNTED EXACTLY. ABOUT 4.3 X 10^19.", "(A CUBE FOR EVERY GRAIN OF SAND ON EARTH IS FAR FEWER)", 1),
 (41024320, "DIGITS IN M136279841", "THE BIGGEST KNOWN PRIME (OCTOBER 2024), 2^136279841 - 1.", "(THE VIEWER'S NUMBERS HAVE 30 TIMES MORE DIGITS)", 0),
]
