#!/usr/bin/env python3
"""Protect truthful, readable native 240x160 Bugstar space HUD semantics.

This is a lightweight source contract, NOT a visual screenshot or gameplay
substitute. The CI SDL two-player live test verifies actual rendering.
"""
from pathlib import Path

ship = Path("space.inc").read_text()
coop = Path("pc/pc_multiplayer.inc").read_text()
menu = Path("pc/platform/pc_sdl.c").read_text()

for marker in [
    '"SHIELD","HULL","FUEL"',
    'text(pg,105,129,"RADAR"',
    'scpy(sbuf,"SPEED ")',
    'text(pg,171,148,"AT:"',
    'text(pg,187,148,b,54,1)',
]:
    assert marker in ship, f"Shared GBA/PC/Android cockpit label missing: {marker}"
assert 'static const char *const lab[3]={"SHLD"' not in ship
for marker in [
    '"RAIDER HEALTH"', '"RAIDER DEFEATED"', '"WAVE %u OF 3"',
    '"P1 HEALTH: %u"', '"P2 HEALTH: %u"',
    '"DRONE HEALTH: %u"', '"DRONE: NEXT WAVE"',
    '"DRONE: DEFEATED"', '"LASER HITS: P1 %u P2 %u"',
    '"MISSIONS WON: %u"', '"ALL 3 WAVES CLEARED"',
    '"BOTH PILOTS WIN!"', 'pc_mp_note_clear_draw()',
]:
    assert marker in coop, f"Game-view label or victory rendering missing: {marker}"
for misleading in ('"P1 SH', '"DRONE %u H:', '"WAVE %u/3  CLEAR', '"LAN SPAR P1:'):
    assert misleading not in coop, f"Cryptic or misleading HUD label reappeared: {misleading}"
assert 'rect(pg,3,98,234,29,0);' in coop
assert 'rect(pg,119,100,1,26,56);' in coop

# font textlen() is four pixels per glyph; reserve margin/space for stats
def fits(label, x, upper=237):
    assert x + 4 * len(label) <= upper, (label, x, x + 4 * len(label))

for label, x in [
    ("RAIDER DEFEATED", 7),
    ("RAIDER HEALTH", 7),
    ("ALL 3 WAVES CLEARED", 126),
    ("WAVE 3 OF 3", 145),
    ("P1 HEALTH: 100", 7),
    ("P2 HEALTH: 100", 126),
    ("DRONE HEALTH: 100", 7),
    ("MISSIONS WON: 31", 7),
    ("LASER HITS: P1 255 P2 255", 126),
    ("BOOST ACTIVE", 180),
    ("SPEED 100%", 180),
    ("AT: 123456789012", 171),
]:
    fits(label, x)

for marker in ('"GAME MODE"', '"NETWORK PORT"', '"HOST IP ADDRESS"',
               '"CREATE GAME"', '"JOIN FRIEND"', '"I AM READY"',
               '"SHOW FRAME RATE"', '"FRAME RATE: %.0f FRAMES/SEC'):
    assert marker in menu, f"Real SDL menu / overlay reverted: {marker}"

print("BUGSTAR UI: 240x160 font-width budgets and truthful live labels PASS")
