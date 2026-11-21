# PC pages for HOW BIG IS IT?

The GBA game contains the original live game counts, the shared `facts.py` and `facts_more.py` pages, and the Bugstarillion. The PC build adds the pages in this folder (files `NN_name.py`, NN = 01..99). **Page totals change as the game evolves:** `NSOLD` and `NFACT` in generated `assets.h` are authoritative, and the `tests/facts_audit.py` output reports the current number. Do not copy old page-count totals into new UI text. `pc/build.sh` sets `BUGSTAR_PC=1` and `facts.py` loads every `pc/facts/[0-9]*.py`.

The number viewer itself now caches page digit counts when sorting them, so opening its complete index no longer recalculates every giant integer. A small `EST.`/`EXACT` label distinguishes measured or rounded entries from exact combinatoric counts and defined constants. The original **GBA MAP:** counts and extended **PC:** content-library counts are explicitly separate mathematical spaces.

## A page
`(value, title, line1, line2, about)`
- `value`: an int (> 0) or a function that returns the digits (helpers: `A(3.8, 26)`, `P10(e)`, `P2(e, minus)`, `FACT(n)` ...). Compute it in
  code from a formula or a well-sourced number: never type a long number from memory. Values need not be sorted (the game sorts them).
- `title`: at most 28 letters. `line1`: at most 58. `line2`: at most 58, or `None`.
- Only the letters the font has: A-Z, 0-9, space and `. , ' : ( ) - ^`. No lower case, no `% & / + = ? ! "`. Write PERCENT, TIMES, PLUS, OVER.
- `about`: 1 if the number is an estimate or rounded (the game says ABOUT), 0 if it is exact.
- No two pages with the same title (the checker looks at all GBA pages and all PC pages).

## Voice (read facts.py and facts_more.py for ~300 examples)
- The page title says WHAT is counted, line 1 gives the one fact that makes the number matter (a source, a year, a comparison), line 2 is
  usually a short joke in brackets, deadpan, friendly, G-rated, never mean, never about a real private person. Sometimes line 2 is a second
  true fact instead. Bugs, ladybugs, the GBA and the game itself are a running joke: use them lightly, not on every page.
- Every fact must be TRUE. Check with WebSearch/WebFetch when you are not sure (units, years, records, counts). If a number changes over
  time (a record, a population), say the year in the text or mark it about=1. If you cannot verify it, leave it out.
- Prefer variety: record-holders, counts, sizes, durations, speeds, masses, odds, ages, distances, how-many-of-X-fit-in-Y, history, firsts.

## Check your file
`python3 pc/facts/check.py pc/facts/05_yourfile.py`  (no argument = all files). It must say `0 problems`.

For the complete shared-and-PC viewer, run `python3 tests/facts_audit.py` and `BUGSTAR_PC=1 python3 tests/facts_audit.py`. The audit rejects duplicated page names, unsupported GBA-font characters, overlong labels, inconsistent population-derived statistics, and the incorrect 256-bit scaling of the PC universe-law count. GBA and Android CI run this audit before building.
