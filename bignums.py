# The giant numbers for HOW BIG IS IT? (see sizes.inc), worked out exactly with Python's whole numbers when the game is built.
# Each is kept as packed decimal digits, two to a byte, the first digit in the high half (an odd count ends with a spare 0 half).
# NZ is how many of its groups of three digits are not 000: the BY NAME view says only those, the way you would read it out loud.
import sys
if hasattr(sys, 'set_int_max_str_digits'): sys.set_int_max_str_digits(0)
STARS = 2**32 * (2**155)**2 * 32            # every star in every universe: 4,294,967,296 universes x 2^155 x 2^155 clusters x 32 stars = 2^347
GIANTS = [
    ("giant_trips", 31**1000),              # trips of 1,000 jumps, each to one of the other 31 stars in your cluster
    ("giant_favs", (STARS + 1)**256),       # 256 favourite slots, each empty or any star in any universe
    ("giant_maps", 256**(128 * 128)),       # planet maps: 128 x 128 spots, each one of 256 heights
    ("giant_saves", 2**(32768 * 8)),        # every way the 32 KB save chip can be set: 262,144 bits
]
def make(arr, out):
    for name, v in GIANTS:
        s = str(v); n = len(s); lead = n - 3 * ((n - 1) // 3)
        nz = sum(1 for g in [s[:lead]] + [s[i:i + 3] for i in range(lead, n, 3)] if int(g))
        if n % 2: s += '0'
        arr(name, "unsigned char", [int(s[i]) * 16 + int(s[i + 1]) for i in range(0, len(s), 2)], per=64)
        out.append("#define %s_ND %d" % (name.upper(), n))
        out.append("#define %s_NZ %d" % (name.upper(), nz))
