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
It starts 3103280543863286140299891155863645591032873999615578336754066... (read live from the file).

How its digits are made: `mkpow.py` squares 2 thirty-two times (2, 4, 16, 256, ... ) with Python's `decimal` module (libmpdec: exact arithmetic, number-theoretic-transform multiplication) with a precision large
enough that rounding never happens (rounding and inexact results are *trapped*: they would stop the program). It writes the digits packed two to a byte, with a 64-byte header, to `pow2_2e32.bcd` (646 MB).
Needs about 5 GB of RAM and 15 to 40 minutes. The game looks for the file in `$BUGSTAR_GIANT_DIR`, `<program folder>/giant/cache`, `./giant/cache`, `pc/giant/cache`; the viewer's "not built" screen has **A: BUILD IT NOW**
(starts the Python program in the background). The file is not in the repository (it is 646 MB); the counting number needs no file.

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
  about 0.7 MB, made in about a minute the first time and then kept next to the digit file) plus a short scan. A seek takes about a fraction of a second.

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

`giant.h` / `giant.c` the library (digits, names, SHA-256, export, V3) · `mkpow.py` builds the digit file · `ref.py` the independent Python reference · `test.sh`, `test_giant.py`, `test_pow.py`, `test_v3.py`, `giant_test.c` the tests ·
`../pc_giant.inc` the screens (included by `build.sh`) · `../tests/giant.sh` plays the screens in the real game · `../facts/13_giant.py` 36 HOW BIG pages (exact: digit counts and leading digits from 400-place logarithms, trailing digits by modular arithmetic) ·
`cache/` (ignored by git) the 646 MB file and the index.

## Tests (`pc/giant/test.sh all`, about 25 minutes the first time)

- names: all 1000 chunks (both endings and the English words), 309 big names (up to 400 digits), well known names, the googolplex name length, English for 64-bit numbers, SHA-256 against `hashlib` (also at every padding edge), decimal-string helpers;
- counting number: 3,027 windows (every change from 1-digit to 9-digit numbers, the end, 3,000 random) against a Python reader that finds the number by bisection; the 1,010,000,009-byte export, its size and SHA-256 against a Python program that writes 1..123,456,790 itself;
- 2^(2^32): the digit count from a 500-place logarithm; the first 300 digits from that logarithm; the last 2000 from `pow(2, 2^32, 10^2000)`; the **whole file's value modulo ten primes** against `pow(2, 2^32, p)`;
  4,000+ random windows and the edges against Python reading the file's nibbles; the name of the top place decoded back to the order of magnitude; the export's size and SHA-256; seek speed;
- v3: the total number of letters and every sampled term's start and length against a numpy sweep over all 1,292,913,987 terms; windows at term starts, ends and middles of the longest names against names rebuilt by Python; an exported range;
- in the real game: `pc/tests/giant.sh` (viewer, jump, hold-scroll, big type, menu, about pages, the namer, the ladder, an export, phone-size contact sheet).
