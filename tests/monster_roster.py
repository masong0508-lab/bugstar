#!/usr/bin/env python3
"""Fail fast when GBA/PC/Android dungeon monsters diverge from generated art."""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import monsters

assert len(monsters.M) == 74, "Expected 50 classic + 24 new monster art profiles"
assert len(monsters.EXTRA_MONSTERS) == 24
keys = [row[0] for row in monsters.M]
names = [row[1] for row in monsters.M]
assert len(set(keys)) == len(keys)
assert len(set(names)) == len(names)
base = {row[0]: row for row in monsters.M[:50]}
for key, name, src, stat, biome in monsters.EXTRA_MONSTERS:
    assert src in base, (key, "missing generated art parent", src)
    assert 0 <= biome < 6, (key, biome)
    assert len(stat.split(",")) == 14, (key, "invalid NMon combat fields")
    art = monsters.ART["MN_" + key]
    assert len(art) == len(base[src][2]), key
    assert all(len(line) == len(parent)
               for line, parent in zip(art, base[src][2])), key
    assert art != base[src][2], ("New monster must be visibly different", key)

dungeon = (ROOT / "dungeon.inc").read_text()
assert "#define NPOOL 12" in dungeon
assert "K_NEND" in dungeon and "K_SNAIL=24" in dungeon
for key in keys[50:]:
    assert "K_" + key in dungeon, ("Missing gameplay enum", key)
    assert "MN_" + key + "_W" in dungeon, ("Missing sprite dimensions", key)
    assert "art_mn_" + key.lower() in dungeon, ("Missing packed sprite", key)
for group, count in (("zone_pool", 6), ("cls_pool", 16)):
    found = re.search(
        rf"static const u8 {group}\[(?:6|NPC)\]\[NPOOL\]=\{{\{{([\s\S]*?)\}}\}};",
        dungeon)
    assert found, ("Missing spawn pool", group)
    rows = re.split(r"\}\s*,\s*\{", found.group(1))
    assert len(rows) == count, (group, len(rows))
    for i, row in enumerate(rows):
        cells = [part.strip() for part in row.split(",") if part.strip()]
        assert len(cells) == 12, (group, i, len(cells))
        assert any("K_" + key in cells for key in keys[50:]), (group, i, "no new monsters")
print("Monster roster: 74 generated art recipes, 90 dungeon planet species, all 22 biome pools at 12 species")
