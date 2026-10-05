# BUGSTAR

A Game Boy Advance action RPG: a pill bug on turning planets, textured real-time 3D dungeons under each one, a pet that fights beside you, and a galaxy that never ends.

**Play:** open `bugstar.gba` in mGBA (or any GBA emulator, or a flash cart). It saves on the cartridge (3 save slots).

## Controls
| Button | On a planet | In a dungeon |
| --- | --- | --- |
| D-pad | Walk; the planet turns under you | Up/down walk, left/right turn (a tap turns a little, holding turns faster) |
| A | Door, ship, cage, slug | Bite (hold A to keep biting). With nothing to bite: open cages, stairs, fish at water |
| B | Use the picked item | Use the picked item |
| L / R | Pick an item in your bag | Strafe left / right |
| SELECT | | Tap: cast your spell. Hold: up/down picks a bag item, L/R changes spell |
| START | Character screen (BUG, GEAR, MAGIC, QUEST, SYSTEM) | Character screen |

## Speed
The 3D view is heavy to draw, so it shows about 15 pictures a second. Your own movement (walking, strafing, turning) runs 60 times a second, and the rest of the dungeon (monsters, hunger, cooldowns, timers) ticks at a steady 15 a second, so nothing speeds up or slows down when drawing does. Walking and turning speeds are the `WALK_*` and `TURN_*` numbers at the top of the dungeon code in `main.c`.

## Saving, loading and options
- **Title screen:** CONTINUE (your last slot), NEW GAME, LOAD GAME, OPTIONS. Up/down and A.
- **Three save slots.** Each card shows the world, level, XP bar, gold, fame and play time. In the slot picker: SELECT erases a slot, R copies it to another. Old single-slot saves show up as slot 1.
- **SYSTEM tab** (START, then L/R): SAVE GAME to your slot, SAVE TO ANOTHER SLOT, LOAD GAME, OPTIONS, QUIT TO TITLE. You can only save on the surface (not inside a dungeon). Loading and quitting work anywhere.
- **Autosave** still happens at your ship, after dungeons and at stations. Turn it off in OPTIONS if you want to save by hand.
- **Options:** music (off/low/full), sound effects, autosave, screen shake. They are remembered on the cartridge.

## What is in it
- **Six zones**, each dungeon deeper than the last (3 to 8 floors), a boss at the bottom. Then the **endless galaxy**: new worlds forever, bottomless dungeons, loot you only keep if you get out.
- **Your bug:** health, mana, level, and 5 points per level for Strength, Dexterity, Vitality and Magic.
- **Gear like Fate:** thorns (bite), shells (armour) and charms, in normal, magic (blue) and rare (gold) versions with names like SHARP WASP STING OF LIFE.
- **Spells:** Fireball, Spark, Heal, Shield. Find their books.
- **Buddy, your pet:** follows you and fights. Feed him fish to turn him into a Fire Beetle, Stone Beetle or Stag King. Send him off to sell your spare gear (SELECT on the Gear page).
- **Fishing** in dungeon pools.
- **Unknown berries and seeds** like Fatal Labyrinth, hunger, crumbs, potions.
- **Quests and fame** from your ship's quest board; named elite monsters; wasps that shoot; beetles that hit hard.
- **Your ship:** supplies, a gear shop, selling, healing, and flying on.
- **Warp crystals** open a portal home; the door takes you back to the same floor.
- **Planets from space:** a round, turning world with seas, land, polar ice, drifting clouds, a soft day/night edge and a hazy rim; it keeps the frame rate up when you fly close.
- **Looks:** baked stone, brick, earth, moss, crystal and marble textures, doors, potion niches, plaques, rune bands, torches, smooth lighting with soft corner shadows, water and polished floors that reflect, embers and dust, head bob, chiptune music.

## Build
Needs an ARM GBA toolchain (`arm-none-eabi-gcc`, e.g. devkitARM), `gbafix`, and Python 3 with numpy. GitHub Actions builds it on every push (see the Actions tab).
```
make            # or: make TC=/path/to/arm-none-eabi-
```
Files: `main.c` (planet, dungeon loop, title, main), `dungeon.inc` (level builder, light baking, 3D renderer), `game.inc` (stats, gear, pet, quests), `ui.inc` (screens, shop, SYSTEM tab), `save.inc` (save slots, options, slot picker and menu widgets), `sound.inc` (music and effects), `gen.py` + `texgen.py` (make `assets.h`: palettes, textures, sprites, tables), `crt0.s` + `gba.ld`.

## Space (new)
At your ship, pick **LAUNCH INTO SPACE**.
- D-pad: steer. B: thrust. R: brake. A: shoot pirates.
- Fly close to the planet and press A to land. Fly to the white box (station) and press A to dock.
- Station: buy and sell 8 kinds of goods (prices change by world type), refuel, repair, and upgrade laser, shield and cargo.
- SELECT: galaxy map. Pick a star and press A to jump (uses fuel).
- Beat each zone's boss to open the next star. Beat the Ant King to open the whole galaxy. Then START on the map jumps to a new cluster, forever.

## The Technobeast (space boss)
After you beat the Ant King, open the galaxy map (SELECT) and press **L** to fly to the Technobeast's lair (costs 2.0 LY fuel).
- It has **one trillion HP**. Its damage **stays** when you leave, so you can come back again and again.
- Shoot the glowing middle (the core) for 3x damage.
- When it charges its beam, fly fast (hold B) to dodge.
- It sends drone fighters. They get stronger with your level, your tech and every beast you kill.
- **SELECT** warps you out.
- Get stronger at any station: **TECH CORE** makes your laser 4x stronger on the beast, every level. Your weapon gear also adds damage.
- Kill it and it rebuilds with 10x more HP.
