# GIANT NUMBERS and BUGSTARILLION v3 (PC port only)

Where: pause (START), WORLD page, **B** (the PC LAB), **GIANT NUMBERS**. Five things there:

| Entry | What it is |
|---|---|
| 2 TO THE POWER 2^32 | a real number with **1,292,913,987 digits**, every digit readable |
| THE COUNTING NUMBER | 1 2 3 4 ... 123,456,790 written in a row: **1,000,000,008 digits**, worked out live |
| BUGSTARILLION V3: THE NAME | the complete name of the V3 number, **6,376,476,945,538,615,314 letters**, read anywhere in |
| NAME ANY NUMBER | type up to 128 digits (or a power of ten) and read its name piece by piece |
| THE SCALE LADDER | a thousand ... a googolplex ... V2 ... V3, each with its name and length |

Nothing here is stored whole. A digit (or a letter of a name) is worked out, or read from a file, only when it is on the screen.

## The viewer (the three big ones)

- UP / DOWN: one line. L / R: one page. **Hold** and it waits a moment, then goes faster and faster (doubling about every 7 frames: the whole number in a few seconds).
- **A**: jump to digit (or letter) number N. **START**: menu (jump, go to the start, go to the end, about this number, write the whole number to a file, big type).
  **SELECT**: big / small type (big type is for phones). **B**: back.
- The top line says `DIGIT 1,234,567 OF 1,292,913,987` and the percent. Digits are in groups of three with commas, alternate groups are coloured; a bar on the right shows where you are.
- **Write the whole number to a file**: streamed in 1 MiB pieces next to the save file (`bugstar-giant-*.txt`), 100 digits a line, a progress bar, **B cancels** (the part file stays), and the SHA-256 of the file is shown at the end.
  For the V3 name (6 quintillion letters: no disk holds it) you export 1 MB or 100 MB from where you stand.
- About pages (L / R): WHAT IT IS, HOW MANY DIGITS (in words; first and last digits read live), THE PARTS OF THE NAME, COMPARE, HOW IT WAS CHECKED. Every comparison is marked REAL (exact), EST (an estimate) or INVENTED (made up for the game).

## 2 to the power 2^32 (the real number)

`2^(2^32)` is the number of different values that 512 MiB (2^32 bits) of memory can hold. It is exactly `floor(2^32 x log10(2)) + 1 = 1,292,913,987` digits long.
It starts 3103280543863286140299891155863645591032873999615578336754066... and ends ...982336.

### How the game holds it: the rule, not the digits

The game ships **no digits**. The number is kept as what it is, `2^(2^32)`, and any stretch of its digits is **worked out exactly** when you look at it (`gpow.inc`):

- **First half (digits 1 to 646,000,000): the head method.** 2 is squared 32 times keeping only the top P limbs (base 100,000) of each square, always rounding down.
  The result L satisfies `L x 100000^e <= 2^(2^32) < (L + 100000^3) x 100000^e`: after t rounded squarings the relative error is at most `2^(t+1) / 100000^(P-1)`, so the error is
  below 100000 x 2^33 < 100000^3 units of the last limb. A digit is used only when L and L + 100000^3 agree on it; 20 guard digits are never used. So every digit shown is **proved**.
- **Second half: the tail method.** 2 is squared 32 times **modulo 100000^T** (keep the low T limbs). This is exact whole-number arithmetic.
- **Squaring**: number-theoretic transforms modulo two primes (3 x 2^30 + 1 and 13 x 2^28 + 1, Montgomery arithmetic, cache-blocked), joined by the Chinese remainder
  theorem, one prime on each of two threads. The biggest square (129 million limbs) uses transforms of 2^28 points. Plain C, no libraries.
- **Checked against an independent computation**: the game carries `gpow_blocks.h`, an 8-byte SHA-256 fingerprint of each of the 1,293 blocks of a million digits
  (**10,344 bytes**), made from the digits that Python's `decimal` module (libmpdec) computed (`mkpow.py`) and that were checked there (whole value modulo 10 primes,
  first 300 digits from a 500-place logarithm, last 2,000 from `pow(2, 2^32, 10^2000)`). Every block the game works out is hashed and must match, or it is not shown.
- What has been worked out is kept in memory in up to two **bands** (block aligned, at most 120 million digits each, 10 bits per 3 digits: **at most about 100 MB of RAM**).
  Hold-scrolling fast does not start any work; it starts when you let go. While it works there is a progress screen; **B stops it**.

How long a window takes depends only on how deep it is (distance from the nearer end), measured on this 2-core test machine while other jobs were running:

| depth (digits from the nearer end) | wall time | CPU time | peak memory |
|---|---|---|---|
| up to 10,000 | under 0.1 s | | |
| 120,000 | 0.3 s | | |
| 1,000,000 to 2,000,000 | 2 to 4 s | 1.6 s | |
| 20,000,000 | 20 s | 10 s | 81 MB |
| 200,000,000 | 117 s | 72 s | 1.2 GB |
| 646,000,000 (the middle, the worst case) | 151 to 190 s | 97 s | 2.5 GB |

**The FAST CACHE (optional, made only when you ask: viewer menu, WHERE THE DIGITS COME FROM)**: the 240,000,000 digits around the middle (digits 526,000,001 to
766,000,000), where working out is dearest, in one file of **100,000,067 bytes** (`pow2_2e32.band`, 10 bits per 3 digits), every block checked when it is made. Making it
takes two middle-sized computations (about 5 minutes here); SELECT deletes it. With it, the worst window left is at depth 526 million instead of 646 million (about 20% faster);
inside it any window is instant. An honest note: no 100 MB store can make *every* window instant, because the digits are essentially random (537 MB of information, see below)
and a digit at depth d needs arithmetic on about d digits; the cache spends its 100 MB where the arithmetic costs most.

**Sizes, measured** (xz -9 on 20 MB samples from the middle of the number):

| form | bytes | note |
|---|---|---|
| text, 100 digits a line (the export) | 1,305,843,127 | xz brings text to 44%, no better than packing |
| one byte per digit | 1,292,913,987 | |
| packed decimal, 2 digits a byte (`mkpow.py`'s file, tests only) | 646,457,058 | xz: -15% (it removes the unused nibble values) |
| 10 bits per 3 digits | 538,714,162 | xz: +0.005% (nothing left to squeeze) |
| the information floor, 1,292,913,987 x log2(10) / 8 | 536,870,912 | = 2^32 bits, the number's own binary size |
| **what the game ships** | **10,344** | the block fingerprints; the digits come from the rule |
| what the game writes to disk by default | **0** (0.7 MB if you make the V3 index) | |
| the FAST CACHE, only if asked | 100,000,067 | |

The whole number can still be written out as text (menu, WRITE THE WHOLE NUMBER TO A FILE): the game works it out in two passes (head half, tail half) straight to the file,
about 6 to 7 minutes here and 1.3 GB on disk, with the SHA-256 shown at the end (it is `8df43d3fd1f669b3c1d422dca7dea4bea6a1bcc325d688681ce94efbc964059b`).

`mkpow.py` (Python decimal, about 16 minutes and 5 GB of RAM) still builds the full packed file `pow2_2e32.bcd`; it is the **independent reference** the tests compare with,
and the game uses it when it is present (`BUGSTAR_GIANT_DIR`, `<program folder>/giant/cache`, `./giant/cache`, `pc/giant/cache`), but the game does not need it.

## The counting number (also real)

`123456789101112...123456789123456790`: the numbers 1 to 123,456,790 written one after another. Champernowne's constant (1933) is this string read as a decimal fraction; here it is cut after 123,456,790 and read as a whole number.
The cut was chosen so the digit count is just over a billion: `9 + 180 + 2,700 + 36,000 + 450,000 + 5,400,000 + 63,000,000 + 720,000,000 + 9 x 23,456,791 = 1,000,000,008`. Digit N is found by arithmetic (how many 1-digit, 2-digit ... numbers come first), no file.

## The naming system (Conway-Wechsler)

`10^(3n+3)` is the *n-illion* (n = 1 million, 2 billion, 3 trillion ... 10 decillion ... 100 centillion). For n of three digits or fewer the name is a Latin prefix + `illion`.
The prefix is built from the digits of n: **units + tens + hundreds** (this is the order the Latin parts are written in):

| part | 1..9 | 10 .. 90 | 100 .. 900 |
|---|---|---|---|
| name | un duo tre quattuor quinqua se septe octo nove | deci viginti triginta quadraginta quinquaginta sexaginta septuaginta octoginta nonaginta | centi ducenti trecenti quadringenti quingenti sescenti septingenti octingenti nongenti |
| letter the unit before it may need | | N, MS, NS, NS, NS, N, N, MX, none | NX, N, NS, NS, NS, N, N, MX, none |

Rules for the unit before a ten or hundred: **tre** becomes **tres** before an S or X part; **se** becomes **ses** before S and **sex** before X; **septe** and **nove** become **septem** / **novem** before M and **septen** / **noven** before N.
(357 = *septen* + *quinquaginta* + *trecenti* = SEPTENQUINQUAGINTATRECENTI.) Digits 0 to 9 alone use the short list: ni mi bi tri quadri quinti sexti septi octi noni (so 1 = mi-llion, 2 = bi-llion).
For n of more than three digits: cut n into groups of three from the right; each group's prefix drops its last I or A and gets **-illi**, except the last, which gets **-illion**; a group of 000 is **nilli**.
So 1000 = MILLI + NILLION = MILLINILLION (10^3003). The game's `illion()` and this library make the same names (checked by test).
`pc/giant/giant.c` has the rules in `gcw_prefix` / `gcw_tok` / `gcw_place`; `pc/giant/ref.py` has them again, written separately from the table above.

Checked against a published figure: a googolplex (10^(10^100)) is TEN + the 766-letter *(10^100 / 3 - 1)-illion* = **769 letters**, which is the "about 769 letters" Sbiis Saibian's page gives for the Conway-Guy name.

## BUGSTARILLION v3

Definition (exact):

```
V3  =  sum, for j = 1 .. L, of   c(j) x 1000^(n(j) + 1)          L = 1,292,913,987
n(j) = the number written by the first j digits of 2^(2^32)       (n(1) = 3, n(2) = 31, n(3) = 310, n(4) = 3103, ...)
c(j) = 100 + h(j) mod 900,   h(j) = a fixed 32-bit hash:  q = j x 2654435761 mod 2^32;  q ^= q >> 15;  q = q x 2246822519 mod 2^32;  q ^= q >> 13
```

`1000^(n+1) = 10^(3n+3)` is the *n-illion*, so every term is said as **"c(j) in English words, then the n(j)-illion"**. The name grows one digit of n(j) at a time, as v2's did:
term 1 is `c(1) TRILLION` (3-illion), term 2 `c(2) UNTRIGINTILLION` (31-illion), term 3 `c(3) DECITRECENTILLION` (310-illion), term 4 is a 3103-illion, and so on up to term L, whose place is the
2^(2^32)-illion: a name of 430,971,329 pieces. The whole name, **biggest term first**, is one stream of letters: `<WORDS> <ILLION NAME>, <WORDS> <ILLION NAME>, ... .` (", " between terms, a full stop at the end).

- Size of the number: V3 is about `c x 10^(3 x 2^(2^32) + 3)`, a number whose count of digits has 1,292,913,987 digits itself.
- Parts: L = **1,292,913,987** terms (v2: 11,896,480).
- Letters in the whole name: **6,376,476,945,538,615,314** (v2's place names add up to about 8.9 billion letters; the googolplex name is 769; Noll's googolplex word 3,871).
- The viewer finds any letter without reading the name from the start: a table with the position of every 65,536th term and the running letter count of the three alignments of triples (`pow2_2e32.v3idx`,
  about 0.7 MB, made once from all the digits worked out in two passes, a few minutes, then kept in `giant/cache`) plus a short scan. A seek takes about a fraction of a second.

### What is real and what is invented (read this before quoting it)

- **Real**: the Conway-Wechsler rules; the googolplex figures (769 and 3,871 letters are published); the exact digits of 2^(2^32); every statement in the COMPARE page marked REAL.
- **Invented for the game**: V3 itself (the sum, the hash, the order the name is said in) and the idea that it is a record.
- **The record that was researched** (web search, 1 Nov 2026): nobody keeps an official record for "longest systematic number name". The systems themselves have no limit: Conway-Wechsler can name *any* whole
  number; Landon Curt Noll's script says its algorithm "has no limit" (its web page caps the input at 1,000,000 for the Latin power); Bowers' -illions (tiers up to ikillion, trakillion, and beyond) and Rowlett/Saibian's names reach numbers far bigger than anything here in a few letters.
  The longest worked examples I found in print are the googolplex names: **769 letters** (Conway-Guy, as given by Saibian) and **3,871 letters** (Noll's script). So *size of the number named* is not a measure that anyone can "beat" for a naming system,
  and V3 does **not** beat Bowers' systems on it.
- **The measure on which V3 beats them** (made up by this game, and honest about it): *the number of letters in one complete, written-out, rule-made name of a single number that you can read from any place and check against separate code.*
  V3's name has 6,376,476,945,538,615,314 letters: about 1.65 x 10^15 times Noll's 3,871 and about 7.2 x 10^8 times v2's 8.9 billion; and the number it names (about 10^(3 x 2^(2^32))) is far bigger than v2's (about 10^(3 x 10^185)).
  It is a **game record**, not a published one.

## Names of the files

`giant.h` / `giant.c` the library (digits, names, SHA-256, export, V3) · `gpow.inc` the exact on-the-spot arithmetic for 2^(2^32) · `gpow_blocks.h` the 1,293 block fingerprints · `mkpow.py` builds the reference digit file · `ref.py` the independent Python reference · `test.sh`, `test_giant.py`, `test_pow.py`, `test_v3.py`, `giant_test.c` the tests ·
`../pc_giant.inc` the screens (included by `build.sh`) · `../tests/giant.sh` plays the screens in the real game · `../facts/13_giant.py` 40 HOW BIG pages (exact: digit counts and leading digits from 400-place logarithms, trailing digits by modular arithmetic) ·
`cache/` (ignored by git) the V3 index, the optional FAST CACHE, and (tests only) the 646 MB reference file.

## Tests (`pc/giant/test.sh all`, about 25 minutes the first time)

- names: all 1000 chunks (both endings and the English words), 309 big names (up to 400 digits), well known names, the googolplex name length, English for 64-bit numbers, SHA-256 against `hashlib` (also at every padding edge), decimal-string helpers;
- counting number: 3,027 windows (every change from 1-digit to 9-digit numbers, the end, 3,000 random) against a Python reader that finds the number by bisection; the 1,010,000,009-byte export, its size and SHA-256 against a Python program that writes 1..123,456,790 itself;
- live (`test.sh live`; `GIANT_FULL=1` adds the whole export and the FAST CACHE): windows worked out on the spot at both ends, the split and random depths against the
  reference digits; reads through the bands with their block checks; the whole number worked out and written out has the reference SHA-256; the FAST CACHE reads;
- 2^(2^32) reference file: the digit count from a 500-place logarithm; the first 300 digits from that logarithm; the last 2000 from `pow(2, 2^32, 10^2000)`; the **whole file's value modulo ten primes** against `pow(2, 2^32, p)`;
  4,000+ random windows and the edges against Python reading the file's nibbles; the name of the top place decoded back to the order of magnitude; the export's size and SHA-256; seek speed;
- v3: the total number of letters and every sampled term's start and length against a numpy sweep over all 1,292,913,987 terms; windows at term starts, ends and middles of the longest names against names rebuilt by Python; an exported range;
- in the real game: `pc/tests/giant.sh` (viewer, jump, hold-scroll, big type, menu, about pages, the namer, the ladder, an export, phone-size contact sheet).
