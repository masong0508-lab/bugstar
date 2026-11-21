# HOW BIG IS IT? — facts and update notes

The `sizes.inc` screen mixes **exact combinatorics and game limits** with **time-sensitive observations and estimates**. These must not be treated the same.

## Verified snapshot: November 20, 2026

| Page | Displayed number | Basis | Next review |
| --- | ---: | --- | --- |
| WORLD POPULATION, NOV 2026 | About 8.22 billion | [U.S. Census world population clock](https://www.census.gov/popclock/data/population.php), ~8.224 billion on November 20-21, 2026; intentionally rounded | 2027 |
| ANTS FOR EVERY PERSON | About 2.43 million | Estimated 20 quadrillion ants divided by the population snapshot; both inputs uncertain | 2027 |
| HANDSHAKES FOR EVERYONE | n × (n − 1) / 2 | Derived from the same rounded population snapshot, not a physical measurement | With population |
| CONFIRMED EXOPLANETS, 2026 | At least 6,400 | [NASA Exoplanet Archive](https://exoplanetarchive.ipac.caltech.edu/), more than 6,464 listed November 2026 | 2027 |
| KNOWN ASTEROIDS, NOV 2026 | About 1.36 million | [ESA orbital census](https://neo.ssa.esa.int/search-for-asteroids?tab=summary), ~1,360,338 on November 20 | 2027 |
| ENGLISH WIKIPEDIA ARTICLES | About 7.26 million | [Wikimedia statistics](https://meta.wikimedia.org/wiki/List_of_Wikipedia), 7,264,153 in November 2026 | 2027 |
| KM IN A LIGHT DAY | 25,902,068,371 km | c × 86,400 seconds; [NASA](https://science.nasa.gov/mission/voyager/voyager-1/voyager-1-what-is-a-light-day/) confirms Voyager 1 passed this distance from Earth on November 18, 2026 | Permanent milestone |
| AVOGADRO'S NUMBER | 602,214,076,000,000,000,000,000 | [IUPAC](https://goldbook.iupac.org/terms/view/A00543/html): exactly this many entities per mole since SI redefinition in 2019 | Exact constant |
| METRES IN ONE AU | 149,597,870,700 | Exact IAU definition of an astronomical unit; Earth's physical distance to the Sun varies | Exact constant |

Other historical-model pages now say **model** or **estimate**, not “the exact number in the real universe.” Earth–Moon separation is an **average centre-to-centre distance**, not a fixed empty space between surfaces.

## GBA versus PC

The gameplay travel map uses 256-bit universe IDs, two 155-bit cluster axes and 32 stars per cluster. The extended **PC content library** uses 2,048-bit universe IDs and two 1,240-bit cluster axes. These are separate mathematical spaces. The original entries in `sizes.inc` are explicitly titled **GBA MAP**, and PC content counts appear in dedicated `PC:` entries built by `pc/facts/*.py`. Do not silently substitute one for the other.

## Regeneration and regression checks

- Edit `facts.py` for shared facts, `facts_more.py` for the second batch, `pc/facts/*.py` for PC-only pages, `sizes.inc` for live calculations.
- Run `python3 tests/facts_audit.py` for common entries and `BUGSTAR_PC=1 python3 tests/facts_audit.py` for PC expansions.
- `python3 gen.py` rebuilds `facts.bin` and `assets.h` for GBA; the ROM CI does this unconditionally. `pc/build.sh` rebuilds them with `BUGSTAR_PC=1` in its scratch copy, including the Android build.
- Date-stamp population, exoplanet, asteroid and Wikipedia numbers rather than claiming the game performs live research. Mark estimated values with `about=1`. Audit at least annually.
- Keep text within 28 characters for page titles, 58 for descriptions, in the character set supported by the tiny GBA font. Keep the huge-number storage and scratch budgets unchanged.
