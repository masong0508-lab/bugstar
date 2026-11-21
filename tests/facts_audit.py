#!/usr/bin/env python3
"""Audit the HOW BIG IS IT source data before packing facts.bin for each port."""
import os
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root))
import facts

items = {entry[1]: entry for entry in facts.F}
for value, title, line1, line2, approx in facts.F:
    assert title and len(title) <= 28, (title, len(title))
    assert line1 is None or len(line1) <= 58, (title, line1)
    assert line2 is None or len(line2) <= 58, (title, line2)
    assert approx in (0, 1), (title, approx)
    assert isinstance(value, int) or callable(value), title

def value(title):
    v = items[title][0]
    return v() if callable(v) else v

population = value("WORLD POPULATION, NOV 2026")
assert population == facts.WORLD_POP_2026 == 8_220_000_000
assert items["WORLD POPULATION, NOV 2026"][4] == 1
assert value("HANDSHAKES FOR EVERYONE") == population * (population - 1) // 2
assert value("ANTS FOR EVERY PERSON") == facts.ANT_EST // population
assert value("AVOGADRO'S NUMBER") == 602214076000000000000000
assert items["AVOGADRO'S NUMBER"][4] == 0
assert value("METRES IN ONE AU") == 149597870700
assert value("CONFIRMED EXOPLANETS, 2026") == 6400
assert value("KNOWN ASTEROIDS, NOV 2026") == 1360000
assert value("ENGLISH WIKIPEDIA ARTICLES") == 7260000
assert "NOVEMBER 18, 2026" in str(items["KM IN A LIGHT DAY"][3])
assert items["ONE MODEL OF BEYOND THE SKY"][4] == 1
assert "GBA MAP: UNIVERSES" in (root / "sizes.inc").read_text()
assert "GBA MAP: ALL WORLDS" in (root / "sizes.inc").read_text()
if os.environ.get("BUGSTAR_PC"):
    assert "PC: UNIVERSES" in items
    assert value("PC: UNIVERSES") == 2**2048
    assert "PC: STAR CLUSTERS A UNIVERSE" in items
print("HOW BIG IS IT: %d pages verified, dated estimates and derived figures consistent" % len(facts.F))
