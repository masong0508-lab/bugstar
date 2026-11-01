# PC pages for HOW BIG IS IT?

The GBA game has ~336 pages (facts.py, facts_more.py). The PC build adds the pages in this folder (files `NN_name.py`, NN = 01..99), so the PC
version has 336 + the pages here (1,438 on 1 Nov 2026: pc seeds 25, pc content x16 20, pc laws 49, earth 157, cosmos 162, maths 132, tech 83, people 36, game 118, life 165, bugs 155). `pc/build.sh` sets `BUGSTAR_PC=1` and `facts.py` loads every `pc/facts/[0-9]*.py`.

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
