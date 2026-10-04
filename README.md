# BUGSTAR

A Game Boy Advance game: a pill bug on turning planets, real-time 3D dungeons under each one, and a ship to fly between zones.

**Play:** open `bugstar.gba` in mGBA (or any GBA emulator, or a flash cart).

## Controls
| Button | On a planet | In a dungeon |
| --- | --- | --- |
| D-pad | Walk; the planet turns under you | Up/down walk, left/right turn |
| A | Use what you stand next to: door, ship, cage, slug | Bite; open cages; take the stairs |
| B | Use the picked item | Use the picked item |
| L / R | Pick an item in your bag | Pick an item in your bag |
| Start | Stats screen | Stats screen |

## How it plays
- Six zones: The Garden, The Pond, The Picnic, The Night Lawn, The Beehive, The Anthill. Each dungeon is deeper than the last (3 to 8 floors) and the bottom floor has a boss. Beat it, then fly on from your ship.
- Free bugs from cages, like Bugdom. Each kind gives a power: pill bug (+10 gold), ladybug (+1 heart), firefly (brighter light), bee (stings attackers), cricket (faster), runaway ant (points to the stairs).
- Items look the same until you try them, like Fatal Labyrinth: 8 berry colours and 6 seed kinds change their effect every game. Shells are armour, thorns sharpen your bite, crumbs are food. Some are cursed.
- Hunger, XP and levels, loot drops, and a trading post on your ship (food, mystery berries and seeds, healing, identifying items).
- Faint and you wake on the surface without your bag and with half your gold.

## Build
Needs an ARM GBA toolchain (`arm-none-eabi-gcc`, e.g. devkitARM) and `gbafix`, plus Python 3 with numpy for the assets.
```
make            # or: make TC=/path/to/arm-none-eabi-
```
Files: `main.c` (planet, main loop), `dungeon.inc` (level builder and 3D renderer), `game.inc` (zones, bugs, items, RPG), `gen.py` (makes `assets.h`: palettes, planet picture, sprites, font, tables), `crt0.s` + `gba.ld` (start-up and memory map).
