# BUGSTAR

A Game Boy Advance action RPG: a pill bug on turning planets, textured real-time 3D dungeons under each one, a pet that fights beside you, and a galaxy that never ends.

**Play:** open `bugstar.gba` in mGBA (or any GBA emulator, or a flash cart). It saves on the cartridge (3 save slots).

## Controls
| Button | On a planet | In a dungeon |
| --- | --- | --- |
| D-pad | Walk over the hills (in first person: up/down walk, left/right turn) | Up/down walk, left/right turn (a tap turns a little, holding turns faster) |
| A | Door, ship, cage, slug (the gold line at the top says what A will do) | Bite (hold A to keep biting). With nothing to bite: open cages, stairs, fish at water |
| B | Use the picked item | Use the picked item |
| L / R | Pick an item in your bag | Strafe left / right |
| SELECT | Tap: switch between third person and first person. Hold SELECT + L/R: point the arrow at the door or your ship | Tap: cast your spell. Hold: up/down picks a bag item, L/R changes spell |
| START | Character screen (BUG, GEAR, MAGIC, QUEST, SYSTEM, MAP) | Opens on the MAP |

## Your bug (new)
- **Make your bug** when you start a new game: give it a name (up to 8 letters), pick a shell colour (steel, ruby, leaf, gold, violet, coral, onyx, pearl) and a pattern (plain, striped, spotted, banded). You see it live. Your jaws in first person take the same colour.
- Change your look any time at your ship: **MIRROR**.
- Your name shows on the BUG page and in the ending.

## Options (new)
Difficulty (easy / normal / hard: hard does more damage to you but pays +25% gold), which planet camera a new game starts with, music, sound effects, autosave, screen shake, minimap, damage numbers, button hints, and the guide arrow. All remembered on the cartridge.

## How to play (redone)
Five pages with colour-coded button badges: planet, dungeon, space, staying alive, and your quest. Your bug trots along the bottom.

## Things to do on planets (new)
- **Mining.** Every world grows four kinds of resource: **amber trees, crystal rocks, silk webs and nectar flowers** (what grows depends on the world's economy). Stand next to one and press A three times. The goods go into your ship's hold (HOLD 3/10 on the ship menu). Sell them at any station, where prices differ by economy, or...
- **The workbench** on your ship upgrades gear: thorns need crystals, shells need amber, charms need silk, plus gold. Each upgrade adds +2 levels, which raises damage or defence and strengthens magic effects. No cap except 97.
- **Hold SELECT + L or R** to point the arrow at the door, your ship, or the nearest resource.
- Buy a bigger hold at a station when it fills up.

## Longer and grindier (new)
- **Zone tiers.** After a zone's boss is beaten, going back down its door starts **tier 2**: one more floor, stronger monsters, better loot, and the boss is back at the bottom. Beat it for tier 3, and so on up to tier 10. The planet name shows T2, T3... on the status bar.
- **13 feats** (START > FEATS): slay 50 spiders, catch 10 fish, mine 40 things, dodge 25 bites, land on 6 worlds, go down 30 floors, find 3 uniques, clear 3 tiers, reach level 20, and more. Each pays gold and fame.
- **Fame has perks** now: each rank (KNOWN BUG, BRAVE BUG, HERO, CHAMPION, BUG LEGEND) adds gold find, health and crit.
- **Buddy levels up** from the XP you earn: more damage and health. His level shows on the BUG page.
- **Engine:** the planet ground is now pre-smoothed to twice the detail with a grass texture, and draws faster.

## Fighting (new)
- **Monsters wind up before they bite.** A red **!** flashes over them and they hiss. Step back, strafe, or dodge, and they snap at empty air and are open for a moment.
- **Dodge:** tap L or R twice to dash sideways. You can't be hurt while you dash. Dodge a bite at the right moment for "DODGED!".
- **Combos:** bites in a row count up (COMBO X2, X3). The third bite is a **CRUNCH**: double damage and a huge shove.
- **Extra damage** on anything staggered or winding up.
- **Beetles charge** from a distance. Sidestep and they slam into the wall and are stunned.
- **The Queen calls her brood** at half health.
- **Health bars** over every hurt monster. Buddy fights from the side now, so he doesn't block your bite.

## New screens
- **Minimap** in the corner of the dungeon view: walls, you, stairs, monsters (blinking red), cages, treasure.
- **Character screen** redone: health, mana, food and XP bars, stat pips, Buddy's health, and a bag in two columns (left/right to switch). Gear shows **+ or -** next to it if it would be better or worse than what you wear, with the exact change below.
- **Shops** show what an item does and how it compares to your gear.
- **Easy mode** in OPTIONS: half damage to you, slower hunger, you keep your bag when you faint.
- **Autopilot:** after you beat a zone's boss, the ship's first choice flies you straight to the next star. Hold A on it to launch by hand instead.
- **Planets:** search plants (A) for leaves to eat, berries, seeds and the odd shiny pebble. Plants grow back.
- **Planets are real 3D terrain now.** Every world is a height map that wraps around: rolling hills, valleys and lakes, drawn in 3D every frame (voxel style, like the old game Comanche). The ground rises and falls under you, the horizon curves like a small planet, far things are dimmer, and the sun lights the slopes.
- **Two cameras:** third person (behind your bug) or first person (through your own jaws, like the dungeons). Tap SELECT to switch. In first person, left/right turn and L/R step sideways.
- Lakes slow you down. Steep slopes are slow to climb.

## New in this update
- **Yellow arrow** on every planet. It points the short way to the door (or your ship). SELECT switches it.
- **A-button hints** on planets: "A: GO DOWN THE DOOR", "A: YOUR SHIP" and so on.
- **Dungeon map** (START). Rooms show up when you walk in. Gold = stairs down, blue = stairs up, green = caged bug.
- **Compass to the stairs** turns on once you have seen the stairs.
- **No more falling out by accident:** you start each floor on the up stairs, and they only work after you step off once.
- **How to play** on the title screen: 4 short pages.
- **Story cards:** a short intro on a new game, and a "what to do next" card after each boss.
- **An ending:** beat the Ant King to see THE END, your stats, and the credits. Then the endless galaxy opens.
- **New music:** space, boss fights (also the Technobeast), and the ending.
- **Galaxy map:** opens on the star you should go to next. Each tap jumps to the next star.
- **Fixes:** the dungeon status bar no longer flickers; shop and menu messages stay on screen inside the menu.

## Speed
The 3D view is heavy to draw, so it shows about 15 pictures a second. Your own movement (walking, strafing, turning) runs 60 times a second, and the rest of the dungeon (monsters, hunger, cooldowns, timers) ticks at a steady 15 a second, so nothing speeds up or slows down when drawing does. Walking and turning speeds are the `WALK_*` and `TURN_*` numbers at the top of the dungeon code in `main.c`.

## Saving, loading and options
- **Title screen:** CONTINUE (your last slot), NEW GAME (then MAKE YOUR BUG), LOAD GAME, HOW TO PLAY, OPTIONS. Up/down and A.
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

## The Technobeast (the second villain)
A machine bug that eats ships to grow. You meet it 3 times:
1. **Hatchling** - after you beat the Garden boss, it ambushes you in space. It is small and weak. Beat it and it flees, but you grab its tech: stations can now build **TECH CORE** (each level = 4x laser damage on the beast).
2. **Grown** - after the Picnic boss it comes back bigger, with a beam (hold B to fly fast and dodge) and a drone. Beat it and it flees... it was feeding tech to the Ant King!
3. **Complete** - after the Ant King falls, it wakes with **1 trillion HP**. You can't win in one go. Hit it, warp out (SELECT), get stronger, come back. **Its wounds never heal.** Find it any time from the galaxy map with **L**.

Shoot the glowing core for 3x damage. Weapon gear adds damage too. Its drones get stronger with your level, tech and kills. Kill it and it rebuilds 10x stronger.

## After the Ant King: the Tech Hive campaign
The Technobeast feeds from **5 Tech Hives**. While a hive lives, the beast heals itself back up: each living hive keeps 20% of its HP safe. You can't kill it until all 5 are gone.
- The next hive is marked in red on the galaxy map (SELECT).
- Fly there, land, and dive its dungeon. The **Hive Overlord** waits at the core: floors 3, 5, 7, 9 and 12.
- Each dead hive cracks the beast's armour: **2x damage** each time (32x after all 5).
- The hives are: The Scrap Hive, The Battery Moon, The Gear Forest, The Silicon Swamp, The Motherboard.
- Go back and forth: dive a hive, get loot and Tech Cores, then chip more off the beast.


## Feel and physics
- You speed up and slow down, bump walls, and can't walk through monsters. Monsters shove each other.
- Bites knock enemies back. Slam them into walls or into each other for extra damage. A short freeze when a hit lands.
- Barrels and crates: push them, smash them for gold and food.
- You see your own jaws. They sway when you walk and snap shut when you bite.
- The ship drifts and banks. Crashing into the planet, station, rocks or pirates hurts. Shoot asteroids to mine crystals and amber.

## Keep or scrap your loot
- 12 UNIQUE items (orange), mostly from bosses. Hive Overlords always drop one.
- At your ship: TROPHY CASE. KEEP an item forever (safe even if you faint) for permanent points: common 1, magic 3, rare 10, unique 50. Points give damage, gold and health. You can take uniques back out.
- Or SCRAP it (B on the gear page, or in the case) for Tech Scrap. Tech Cores now need gold + scrap.
- The Techno Shard doubles your damage on the Technobeast.
