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

## After the Ant King: the endless galaxy
- Your ship can warp to a new world any time. Every world is made fresh: its own name, colours, monsters and room mix, and it gets harder the further you go.
- Every world has a bottomless dungeon. Below floor 1 there are no stairs up: get out with a Warp Crystal (found every 5 floors, or 120G at the ship) or the Escape seed.
- Gold found down there is loot you carry (shown as + in the corner). Escape and it is banked with a depth bonus. Faint and it is gone.
- Deep floors drop gems (Ruby, Sapphire, Emerald, Diamond) and relics (Golden Shell, King Fang). Use them for their power or sell them at your ship.
- A guardian waits every 10 floors. Your deepest floor is on the stats screen.

## Build
Needs an ARM GBA toolchain (`arm-none-eabi-gcc`, e.g. devkitARM) and `gbafix`, plus Python 3 with numpy for the assets.
```
make            # or: make TC=/path/to/arm-none-eabi-
```
Files: `main.c` (planet, main loop), `dungeon.inc` (level builder and 3D renderer), `game.inc` (zones, bugs, items, RPG), `gen.py` (makes `assets.h`: palettes, planet picture, sprites, font, tables), `crt0.s` + `gba.ld` (start-up and memory map).
