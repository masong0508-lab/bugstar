# BUGSTAR PC (step 1 and 2 of PC_PORT_PLAN.md)

What this is: the GBA game's **own C code**, built for a PC (Linux, 64-bit), running on a **fake GBA** instead of the real chip.
It is headless for now: it reads a key script, writes pictures (PPM) and a save file. No window yet (see "Not done").

```
./pc/build.sh                     # builds pc/bugstar_pc  (needs gcc, python3; no SDL, no 32-bit libs)
./pc/tests/determinism.sh         # same key script twice -> same pictures, same save
./pc/tests/seeds.sh               # 6 addresses (near and far) -> same worlds twice, and same as tests/seeds.golden
./pc/seedview.sh --star 7         # the seed viewer: one PNG with 4 views + facts
./pc/bugstar_pc --script tests/play.txt --out out/p --sav out/p.sav --frames 4501   # play from a key script
```

## How it works (one page)

- `build.sh` copies the game's sources into `pc/gen/` (not committed) and patches them with `sed`. **Nothing in the repo root is edited.**
  The patches: drop the GBA-only `section(".iwram"/".ewram")`, `long_call`, `target("arm")`; use the PC `memset/memcpy`; read the beam line
  through `pc_vcount()`; hook the interrupt handler pointer; turn the DMA-3 start write into `pc_dma3()`; replace the 4 stack-pointer
  asm probes; rename `main` to `bugstar_main`; append `pc_view.inc` (the seed viewer).
- Compile flags that matter: `-funsigned-char` (the GBA compiler's `char` is unsigned), `-fwrapv` (ARM integers wrap), `-no-pie` and a
  stack below 4 GB (the game keeps addresses in 32-bit numbers for DMA).
- `pc_hw.c` is the fake GBA: the real memory map (EWRAM, IWRAM, I/O, palette, VRAM, OAM, cartridge GPIO, SRAM) is mapped at the **real
  addresses**, so the game's own `*(volatile u16*)0x06000000` code runs unchanged.
  - Time is a counter: every read of VCOUNT moves the beam one line; 228 reads = 1 frame; at line 160 the vblank interrupt runs, the next
    frame's keys are set and screenshots are taken. So the game runs on a perfect, endless-speed machine at exactly 60 frames a second.
  - The picture: mode 3/4/5 bitmap layer (with the affine registers), page flip, sprites (normal and rotating, 4/8-bit, 1D/2D), sprite
    priority, the fade/brighten/darken/alpha blend unit, forced blank.
  - The save: the 32 KB SRAM is read from / written to a `.sav` file. It is the **same layout as the GBA's**: a save from the emulator
    loads here (title shows CONTINUE, SLOT 1) and a save made by the same key script here matches the emulator's save except 12 bytes (at 0x7A-0x86 and 0x3B0-0x3B3; I did not look into what they are, most likely counters that follow timing).
- Key script format = the GBA test harness: `<from-frame> <to-frame> <keys>` and `shot <frame>`. Keys: A B s(select) S(start) U D l r R L.

## What was checked (honest numbers)

- Boots to the title, the bug creator, the ship builder, the planet surface; saves; loads a real GBA save.
- **Against the real GBA code in an emulator** (same sources built for ARM, same key script): title screen and bug creator are
  **pixel-identical** (0 differing pixels at frames 700 and 1100). Animated screens differ a little (1 px to 3 % of the picture): the
  emulator ran the game at ARM speed and the PC at perfect speed, so sprites are a frame or two apart.
- Determinism: two runs of the same script give the same hashes of 8 pictures and the same save (`tests/determinism.sh`).
- Seeds: 6 addresses, including a 256-bit universe number and 155-bit cluster numbers, give the same worlds on every run
  (`tests/seeds.sh`). The golden file was made by this build, so it guards against change; it is not a comparison with real hardware.

## Not done (the limits)

- **No window, sound or controller yet.** SDL is not installed here and the network is closed, so the front end could not be built or
  tested. `pc_render()` already gives a 240x160 RGB frame; an SDL window is a small step on top. Sound registers are written to
  memory and the music code runs, but nothing is played.
- **Kart races: the ground is wrong or missing.** It uses a DMA per scanline (HBlank DMA). `pc_dma3` only does "start now" copies.
- Not drawn: windows, mosaic, tile backgrounds, object-window sprites. A search of the game's code finds no use of them (only the boot
  logo uses tile layers, and the PC build skips the logo).
- The dungeon, space and kart screens were not played through by me on this build. They compile and link, nothing crashed in the
  title / creators / planet scene, but I have no evidence for the rest yet.
- The native 3D "POLY" look is **not started** (see below). The seed viewer shows the CLASSIC look only.
- This is 64-bit. The saved layout matches the GBA's (see above), so the saved structs held up; other 64-bit surprises are possible
  (the build hides compiler warnings with `-w`; I did not review them).

## The universe the game already has (names for the code that does it)

The game already has a huge address space. A place is:

| part | where | size |
|---|---|---|
| universe | `uni_w[UW]` (UW=8), `gsk(k)` mixes all 256 bits into a per-use key | 2^256 universes (about 1.2e77) |
| cluster east / north of home | `clus[0..4]` and `clus[5..9]` (`UAX`=5 words per axis, top word masked by `UTOP`) | 155 bits per axis; 2^310 clusters per universe |
| galaxy | `gal_hash()`: the cluster number shifted down by `GAL_SHIFT`=20 (a block of 2^20 x 2^20 clusters, 26 million ly) | 0 = "THE BUG WAY" (home) |
| star | index `0..NSYS-1` (32) in `sys[]` made by `gen_cluster()`; its id is `sys_id(i)` = `amix(gsk(107), i)` (home cluster: `i+1`) | 32 stars per cluster |
| planet / moon | `cur_pl`, `cur_moon`; `body_salt()` (0 for the first planet, so old worlds never changed); counts from `sys_nplan()` / `sys_nmoon()` | 1 to 16 planets (6 to 16 in a CROWDED galaxy, `galaxy_dense()`) |

How an address becomes a world, in order: `u_settle()` folds an old/huge address into this universe and sets how deep in space you are
(`u_far`) -> `gen_cluster()` -> `land_on_current()` -> `make_world()` (three 32-bit seeds A, B, C from `amix(gsk(102..104) ^ ..., star)`;
from them the whole DNA: class, colours, weather, size, city, life; `wd.tseed` = terrain seed, `wd.cseed` = city seed) -> `new_planet()` ->
`gen_terrain(wd.tseed)` and the planet tables (`planet_tables()`, `planet_palette()`). Nothing is stored; leave and it is gone.
`amix()` hashes **every** word of the address, so two addresses never share a world by accident. Distances: `ly_between()` =
(bigger + 0.414 x smaller) x 25 ly, written by `ly_text()`.

## The trip-length rule (written down, not in the game yet)

From PC_PORT_PLAN.md: `trip_length = base_distance * U_count^2`, with `base_distance` = the width of one universe =
`25 ly x 2^155` = about 1.14e48 ly. The game today has 2^256 universes, so the factor is about 1.3e154 if every number is used; the plan
starts smaller and grows:

| universes in play | U^2 | one trip between neighbours |
|---|---|---|
| 64 | 4,096 | 4.7e51 ly |
| 4,096 | 16,777,216 | 1.9e55 ly |
| 65,536 | 4,294,967,296 | 4.9e57 ly |

Nobody flies that by hand: it is paid for with the travel types in the plan (cruise, warp, fold, rift). The rule belongs in a PC-only travel
layer (`warp_cost()`, `fuel_max()` and `universe_go()` are the places it plugs into). It is **not wired in yet**.

## POLY look (stub, not built)

The same game state drives both looks; only the drawing changes. Plan for the hook: the game's drawing calls write 8-bit pixels and sprites
into the fake GBA memory (`pc_hw.c`). CLASSIC = `pc_render()` scaled with sharp pixels. POLY = a second renderer that reads the *state*
(terrain seed `wd.tseed`, the planet tables, the object list `po[]`, the dungeon map and lights) and draws meshes: cube-sphere planets from the
same noise as the tables, wall/floor meshes with the dungeon's own torches as lights, sprites as billboards first. Both need a window and
OpenGL/SDL, which this container does not have, so POLY starts after the window does.

## Files

`build.sh` build · `pc_hw.c/.h` fake GBA · `pc_main.c` program + 32-bit stack · `pc_view.inc` seed viewer · `seedview.sh` viewer to PNG ·
`tests/` scripts, key scripts and `seeds.golden` · `gen/`, `out/`, `bugstar_pc` are build output (ignored).

## Scale update (PC port only, a plan, not wired in yet)

Asked for: multiply the galaxies, stars and planets in ONE universe by 2^256; make one universe's light years 30x bigger
(sexquintuple read as 6 x 5); and multiply the NUMBER OF UNIVERSES by the same 2^256.

| | before (GBA rules) | PC plan |
|---|---|---|
| universes | 2^256 = 1.16e77 | 2^512 = 1.34e154 |
| galaxies per universe | 2^270 = 1.90e81 | 2^526 = 2.20e158 |
| stars per universe (32 per cluster) | 2^315 = 6.7e94 | 2^571 = 7.7e171 |
| planets per universe (up to 16 per star) | 1.07e96 | 1.24e173 |
| width of one universe | 25 ly x 2^155 = 1.14e48 ly | x30 = 3.4e49 ly |
| galaxies, all universes | 2.2e158 | 2.9e312 |
| stars, all universes | 7.7e171 | 1.0e326 |
| planets, all universes (max) | 1.2e173 | 1.7e327 |
| trip to a neighbour universe (width x U^2) | 1.5e202 ly | 6.2e357 ly |

Notes:
- The per-universe counts (`2^270`, `2^315`) come from the code's own address sizes: 155 bits per axis,
  a galaxy is a 2^20 x 2^20 block of clusters, 32 stars per cluster.
- To hold this, the PC address needs about 2x the bits: 5 words per axis -> 9 words, and the universe number
  UW=8 words -> 16 words. `amix()` must hash every word, as it does now.
- Old GBA addresses still map to the same worlds: they sit at the start of the bigger range. The PC build never changes the GBA game.
- Nothing is stored for this. Every place is made from its address when you arrive.
- These are counts of PLACES THAT CAN EXIST. Nobody can visit them all, and the numbers say nothing about how deep each place is.

## Scale update 2: eight times the bits (PC port only, a plan, not wired in yet)

Asked for: "oct-rice (8x) as many bits", with new content, planet traits and anything that helps content, replayability and feasibility.
This **replaces the 2^256 multipliers above**: instead of multiplying the counts by 2^256, the address itself gets 8x as many bits.

| | GBA rules | scale update 1 | **now (8x bits)** |
|---|---|---|---|
| bits for the universe number | 256 | 512 | **2048** |
| bits per axis inside a universe | 155 | 283 | **1240** |
| universes | 1.16e77 | 1.34e154 | **3.232e616** |
| clusters per universe | 2^310 | 2^566 | **3.584e746** |
| galaxies per universe (2^20 x 2^20 clusters each) | 1.90e81 | 2.20e158 | **3.260e734** |
| stars per universe (32 per cluster) | 6.7e94 | 7.7e171 | **1.147e748** |
| planets per universe (up to 16 per star) | 1.07e96 | 1.24e173 | **1.835e749** |
| width of one universe (25 ly x 2^axis x 30) | 1.14e48 ly | 3.4e49 ly | **1.420e376 ly** |
| galaxies, all universes | 2.2e158 | 2.9e312 | **1.053e1351** |
| stars, all universes | 7.7e171 | 1.0e326 | **3.707e1364** |
| planets, all universes (max) | 1.2e173 | 1.7e327 | **5.930e1365** |
| trip to a neighbour universe (width x universes^2) | 1.5e202 ly | 6.2e357 ly | **1.483e1609 ly** |

Notes:
- Address size: 1240 bits per axis is 39 words of 32 bits (38.75, so 39), the universe number is 64 words. `amix()` still hashes every word.
- The old GBA addresses still map to the same worlds (they sit at the start of the bigger range). The PC build never changes the GBA game or its saves.
- Nothing is stored for this. Every place is made from its address when you arrive. Big numbers are only addresses, never lists.
- Distances need big-number math (a small fixed-size bignum, no library) for the trip length, warp cost and fuel. Everything near you stays 64-bit float.
- These are counts of places that CAN exist. Nobody can visit them all, and the numbers say nothing about how deep each place is. That is what the content list below is for.

### More content to go with the bigger universe (the plan for the PC worker)

Rule-made, so it grows with the address instead of by hand:
- **Planet traits** (new, rolled from the planet seed, 3 to 6 per planet, some pairs forbidden): gravity, day length, tilt and seasons, tidal locking,
  ring system, moons, atmosphere mix, weather set (storms, ash, aurora, acid rain, meteor showers), magnetic field, volcanism, ocean depth, glass dunes,
  crystal fields, fungus forests, ruins age, ancient machines, hive worlds, ghost worlds (no life, loud ruins), rogue planets (no sun), binary-star light.
  Each trait changes the terrain tables, the lighting, the monsters that spawn, the loot, the quests and the shop prices.
- **Universe themes** (new): each universe gets 2 rule changes (low gravity, no magic, huge bugs, endless night, double gold, fragile, hunger-free,
  mirror world...). Universe number picks them. Rifts show the theme before you go.
- **Biomes and life**: more bug families from parts (body, legs, head, element, trait), herds, nests, bosses per biome, rare "named" creatures per planet.
- **Quests**: hand-written templates with slots (who, where, what, why, reward), chained into arcs. Planet traits feed the slots.
- **Gear**: more prefixes, suffixes, sets and uniques, tied to planet traits and universe themes.
- **Towns and ships**: more town layouts, shops, rumours, factions, ship parts, wingmen and outposts from the grammar tables.
- **Feasibility**: a seed viewer for the new fields, a world sampler that checks thousands of random addresses (no crashes, no impossible trait pairs),
  a soak test for rifts, and an address/bignum test suite. A one-switch "small universe" mode for slow PCs.

### Built so far: `pc/content/` (the library) and how the game uses it

Everything is made on demand from a seed or an id by word lists (parts x parts); nothing is stored. Ids are "part B x (number of parts A) + part A", so low ids are the plain ones.

| What | Count | Made of |
|---|---|---|
| planet traits | **1,040** | 65 classic traits (9 categories) x 16 flavours (flavour 0 = plain, 15 flavour words per category: SALTY LAVA SEAS) |
| stats per planet | **112** | 7 families (gravity, temperature, daylight, danger, loot, prices, life) x 16 zones (lowlands ... orbit) |
| kinds of people | **512** | 32 stocks x 16 ways of life (REED-WEAVING FERNLINGS) |
| kinds of city | **256** | 16 forms x 16 characters (QUIET CLIFF NEST) |
| creature kinds | **12,672** | 66 body plans (6 classes of 11) x 192 variants (size 4 x diet 3 x habitat 4 x behaviour 4) = 192 x the GBA game's 66 monster kinds (16 x 12) |
| creature individuals | **415,236,096** | 12,672 kinds x 32,768 (gift 16 x colouring 16 x age/rank 8 x mutation 16) |
| universe themes | 24 | unchanged; 266 pairs that can share a universe |
| trait recipes | 10,780,270 plain, **144,586,870,865,920** with flavours | 3 to 6 traits, one per category, no clashing pair |

- `bignum.h`: a small big number (5632 bits, no library): add, multiply, shift, compare, decimal and scientific print. Used for distances, trips and counts.
- `pc_content.c/.h` (+ `pc_beings.inc`, same translation unit): the 8x-bits address (64 words for the universe, 39 per axis), `addr_seed()` that hashes every word,
  the traits (3 to 6 per planet, one per category, 31 impossible pairs across categories such as DEAD SUN + OCEAN WORLD; the rules sit on the classic traits so a flavour never breaks them),
  the 112 stats, the native creatures, peoples and city of each planet, one famous native creature, planet names, and the 24 universe themes (two per universe, no clashing pairs).
- `pcx.c/.h`: a flat API with plain types. The game's own code has names like `trait_name`, so it never includes `pc_content.h`; it calls `pcx_world()` (finished pages of text),
  `pcx_effects()` (what the traits change in the game's world) and `pcx_fold()` / `pcx_hi_set()` (the address bits above the game's own).
- `content_test.c`, `test.sh`: scale numbers, 4544 address bit flips (average 31 of 64 bits change), 1,000,000 random planets (valid, repeatable, all 1,040 traits appear),
  unique names for every trait, stat, people, city and creature kind (all 12,672 compared), 200,000 random individuals (same name means same individual), the
  forbidden-pair rules over all 1,040 x 1,040 traits, the exact counts, 3,000 `pcx` places (every line fits the screen and the font). Run `pc/content/test.sh`.
  `pc/content/sampler.c` prints sample worlds: `pc/content/test.sh 5` builds it and prints five.

**How the game uses it (PC build only; `build.sh` adds `pc_info.inc` and three small `sed` patches to the copies in `pc/gen`):**
- **PC WORLD FILE**: pause (START), go to the WORLD page, press **B** (the PC LAB, see UNIVERSE LAWS below), then **A** on PLANET FILE. Six pages, L and R turn them, B closes: THIS PLACE (the universe's two themes and what they do, the planet's numbers),
  TRAITS (3 to 6 of the 1,040), STATS (the 112 as a table, zones down and families across), PEOPLE AND CITY, CREATURES (the native kinds), FAMOUS CREATURE (one individual with name, gift, mutation, stats).
  Test: `pc/tests/worldfile.sh` (plays the game to a planet, opens the file, turns all pages).
- **The traits change the planet** on the endless galaxy's worlds (after the story; not moons, not the hell worlds): gravity (0.35 / 0.70 / 1.05 / 1.40 g from the planet's gravity percent),
  light (LONG NIGHT and DEAD SUN make night, TIDALLY LOCKED and RED GIANT SKY dusk) and weather (STORMY and ACID RAIN rain, SNOWFALL snow, SANDSTORMS and ENDLESS FOG dust, ASH FALL ash).
  The six story worlds keep their own rules and the file says so. The other numbers (danger, loot, prices, life, the stats, peoples, creatures, themes) are shown, not yet used by the game's rules.
- **The seed viewer** (`--view`) reads a full 2048-bit universe (up to 512 hex digits) and two 1240-bit sides (up to 310 digits), prints the six pages (`PCX` lines) and writes `<prefix>_pcfile1..6.ppm`.
  Bits above the game's own 256 / 155 are handed to `pcx_hi_set()` and folded into the game's words, so the top digit changes the world too (tested in `seeds.sh`).
  In normal play the address only reaches the game's own 256 / 155 bits (that is all the game's navigation and save format hold), so the high words are zero there.
- Next: wire danger, loot, prices, life, the themes, the native creatures, peoples and cities into the game's rules (monsters, loot, quests, towns).


## UNIVERSE LAWS: tiny and giant planets, 146 laws, HOW THIS WORKS, CHEATS (PC port only)

**What it is.** A table of **146 named laws** (`pc/laws/laws_def.h`): UNIVERSE 17, GALAXY 29, STARS 27, PLANETS 37, LIFE 21, TIME 15. Each has a plain name, a one-line meaning, a unit, a sensible range, a wide
(cheat) range, a default, and a small and a big step. They really drive the game: the model (`pc/laws/lawmodel.c`, plain C, no libm, only `+ - * /` and compares, so every machine gives the same answer) turns a law set and a
place (universe, galaxy, star, planet, and the planet's own DNA from the game) into about 100 physical numbers (age, light, star mass and heat, orbit, year, day, gravity, air, temperature, water, radiation, life...) and
into changes the game applies. **Default-is-identity:** every number is worked out under the default laws (the reference) and under the current laws, and the game applies only the difference. So default laws change nothing:
`seeds.golden` and `determinism.sh` are the same as before. UNIVERSE VARIETY bends the laws a little from universe to universe, so different universe numbers give different universes under the same law set.

**Try it.** `./pc/build.sh`, play, pause (START), go to the WORLD page (R four times), press **B**: the PC LAB with PLANET FILE, UNIVERSE LAWS, HOW THIS WORKS, CHEATS.
- **UNIVERSE LAWS** (the editor): tabs PRESETS, UNIVERSE, GALAXY, STARS, PLANETS, LIFE, TIME (go UP from the first row to reach the tabs, LEFT/RIGHT to change tab, or L and R). PRESETS come first, in plain words:
  NORMAL, TINY WORLDS, PEBBLE WORLDS, GIANT WORLDS, HIGH GRAVITY, LOW GRAVITY, DEAD UNIVERSE, STAR NURSERY, CROWDED GALAXIES, LONELY SPACE, SLOW LIGHT, FAST LIGHT, HOT UNIVERSE, ICY UNIVERSE, WATER WORLDS, SPIRALS ONLY,
  GALAXY CRASH, STRANGE LIFE, FAST CLOCKS, SLOW CLOCKS (20). A picks one. In a tab: UP/DOWN pick a row, **LEFT/RIGHT change it** (hold to repeat; a long hold or a shoulder button gives big steps; a step lands on NORMAL),
  **A puts one back to normal**, the last row puts everything back (it asks first). Edited rows have a star and are gold; a value outside the sensible range is red. Under the list: what the law means (plain words) and
  **the live effect** (the numbers of this planet it moves most, with the normal value in brackets). **START** opens the menu: ADVANCED NUMBERS (off: values are words like NORMAL, X2, OFF; on: real numbers with units, and the
  86 expert laws), UNDO (24 deep), SEARCH BY WORD (also SELECT: UP/DOWN pick a letter, RIGHT adds it, LEFT deletes, A done), MAKE THIS PLANET AGAIN NOW (saves and rebuilds it with the new laws), RESET EVERYTHING, HOW THIS WORKS. B goes back.
  The simple list has 60 laws; sensible limits keep a new player inside a game that still works.
- **HOW THIS WORKS** (9 pages, L and R turn them): THE UNIVERSE, THE GALAXY, THE STAR, HOW BIG IS THIS PLANET, THE SURFACE, THE AIR, DAY YEAR AND SEASONS, LIFE, TRIPS AND DISTANCES. Plain explanations (what makes the sun
  hot, why galaxies spin, how stars are born and die, why the horizon bends, where seasons come from, why light speed sets trip times) with the real current numbers of the place you stand in filled in.
- **CHEATS** (red, off by default). Nothing there changes the game until the switch is turned on (it asks first). Turning it on **marks the world as cheated** (`cheated=1` in its laws file; the mark stays). Then: SET ANY LAW (the wide
  ranges), JUMP TO A PLANET SIZE (a pebble, a house, an asteroid, a moon, Earth, Jupiter, the Sun, a giant star, a solar system, a light year, the biggest), GAME SPEED (5 to 3,200 percent; the window paces the game by it, 10 to 2,000), YOUR LEVEL,
  GIVE GOLD, FILL HEALTH FOOD AND FUEL, TELEPORT BY ADDRESS (east and north cluster numbers in base 16, 155 bits each, and a star; endless galaxy only), MAKE THIS PLANET AGAIN. Level and gold are the game's own 64-bit numbers.
- **Saved per world.** Laws are kept next to the save, one file per slot: `<save name>.laws0`, `.laws1`, `.laws2` (text: `KEY=VALUE` for every edited law, and `cheated=0|1`). Loading a slot loads its laws; a new game starts at the defaults;
  erasing or copying a slot deletes or copies its file. Normal play never touches the `.sav`: the laws are in their own file.

**Planet sizes from a pebble to bigger than a star.** The size law (PLANET SIZE, in quarter steps of doubling, 2^-32 to 2^44 times the game's size class) works through radius, gravity (from size and density; the game's four
gravity steps, a walking-speed penalty above 1.4 g, ZERO-G below 0.2 g), escape speed, air, temperature, water, day, year, the horizon and the numbers on the HOW THIS WORKS pages. **Horizon:** the game's own horizon bend is
now in sixteenths and rescaled by radius, so a small world curves away fast (never more than the game's strongest TINY PLANET) and a huge one is flat. **Pebbles:** when a world is fewer than 128 steps round (a step is 5 cm), the walkable world
repeats every 64, 32, 16 or 8 cells: the ground is cross-faded into a seamless tile, the wrap mask is a run-time number (`pc_tw`), objects are wrapped and thinned (one per cell, almost nothing that bites), and no town fits. On a huge world the 128 cell walking world stays the same (the land just gets flatter).
**Limits:** Kart racing is not made for pebble worlds; the six story worlds follow only size and gravity; the PC window paces the game speed, the sound runs at the game's rate.

**What the laws change in play.** Planets and moons per star (and CROWDED galaxies), hell and weird world odds, light years per cluster and the galaxy block size, the hours of a voyage (speed of light), the fuel of a warp
(expansion and dark energy), danger (radiation, angry life), hunger, the planet's class, hour, weather, snow, sand, water level, mountains and roughness, creatures, beetles, cities and ruins, mood, AURORA, METEORS and BAD AIR levels,
resource nodes, the speed the sky turns (day length), and the game speed. `build.sh` patches only the generated copies; the root game sources are never edited.

**Tests.** `pc/laws/test.sh` (table integrity and clamps, every law walks from minimum to maximum, text and file round trips, the cheat flag, search, defaults change nothing over 20,000 places, every law moves at least one number,
100,000 random law sets and places with no non-finite number and valid game changes, a size sweep from a 1.5 mm pebble to a 1e20 m world, text fits the screen). `pc/tests/laws.sh` (the lab in the real game, 26 screenshots, two runs
equal, the laws file holds the preset, the size and the cheat mark, a continued save loads its laws, 8 planets under laws equal `tests/laws.golden`). The seed viewer takes `--law KEY=VALUE`, `--laws FILE`, `--preset N`.
If the model changes on purpose, make the golden again with `pc/tests/laws.sh --make-golden`. 49 HOW BIG pages about the laws and the real numbers behind them are in `pc/facts/11_laws.py`.

## Window version and packages
`./pc/build.sh` builds the window version (SDL2). Linux: `bugstar_pc` (libSDL2 bundled in `lib/` in the tar.gz). Windows: a 32-bit exe cross-built with llvm-mingw
(`PC_WIN=<SDL2 mingw i686 dir> PC_CC=i686-w64-mingw32-gcc PC_ARCH=-m32 PC_OUT=BUGSTAR.exe PC_CFLAGS="-include stdio.h -include string.h -include stdlib.h -Wno-error -Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-pointer-types" ./pc/build.sh`).
32-bit on purpose: the game keeps addresses in 32-bit numbers, so no stack trick is needed; the GBA memory map uses VirtualAlloc. The Windows exe was compiled, NOT run (no Windows/Wine here).
Keys: arrows/WASD, X/Space=A, Z/K=B, Backspace/Tab=Select, Enter=Start, Q=L, E=R (Coil = Q+E). Esc = options, F11 fullscreen, F12 screenshot.

## The meaning layer (pc/meaning)

Everything the PC game can be made of lines up in one system: 1,040 planet traits, 112 stats, 24 universe themes, 512 peoples, 256 cities, 12,672 creature kinds
(and their gift, colouring, rank, mutation, diet, habitat, behaviour, size parts), plants, the 146 universe laws and the 48 god powers.
- **32 channels**: 25 rule channels (GRAVITY, LIGHT, HEAT, WEATHER, FOOD, WATER, ORE, DANGER, SPEED, TRADE, KNOWLEDGE, FAITH, HEALTH, LUCK, MAGIC, GROWTH, SHELTER, FUEL, CRAFT, DEFENCE, CROWD, TAMING, STEALTH, AIR, TIME) and 7 look channels (BEAUTY, SOUND, TINT, MOOD, GLOW, SCENT, LORE).
- **Roles**: PURPOSE (rule channels only), COSMETIC (look channels only) or BOTH. A made-up item takes the role of its lead part (base trait, stock, form, body plan, plant use), so the roles are generated from 65 + 32 + 16 + 66 + 10 small part rows, not from a table of 12,672 rows. Real mix over the 14,866 whole items: 60.8 percent purpose, 24.9 cosmetic, 14.2 both (per class in `meaning_test` output). A cosmetic base trait changes no number at all: 10 classic traits had their small numbers set to zero for this (so worlds that carry them changed a little).
- **Tags** (24 synergy families, every item has at least one) make matching things help each other (creature habitat vs terrain, diet vs plants, people vs city, theme vs trait; the pair weights are in `lwgt`) and 13 opposite pairs clash softly. The hard forbidden trait pairs are unchanged.
- **Resolver** `mn_resolve`: any list of items becomes one profile (channel totals, saturating at 100,000, levels -100..100). Deterministic; no overflow (tested with the overflow trap on 1,000,000 random piles).
- **MEANS lines** (`mn_means`) and the WHAT IT MEANS, SYNERGIES, CLASHES AND LOOKS and EACH TRAIT MEANS pages (pages 7 to 10 of the PC WORLD FILE).
- **In play** (`pc_mn_dna` in pc_info.inc, endless-galaxy planets only, story worlds untouched): FOOD slows or speeds hunger, DANGER changes the number of animals and beetles, TRADE and LUCK scale gold from kills, ORE/GROWTH/CRAFT/FOOD add resource nodes, SPEED changes walking speed, LIGHT sets dusk or night, WEATHER sets the weather, TINT/GLOW/MOOD/BEAUTY tint the sky and buildings. Switch it off with the environment variable `BUGSTAR_MEANING=0` (the core game is then exactly as before).
- Tests: `pc/meaning/test.sh` (roles, role mix, every channel reachable, every item in a family, 112 stats read or cosmetic, 1,000,000 combinations, coverage: DEAD PARAMETERS 0), `pc/content/test.sh`.

## The PLANET CREATOR (pc/creator, Spore-like)

Open it from the title menu (PLANET CREATOR, the last item) or from the PC LAB (pause, WORLD page, B, PLANET CREATOR). Tabs (L and R): WORLD (name, class, size, extreme size from a pebble to
x1,000 bigger, seas, land shape, roughness, light, weather, sky and ground colour, climate), TRAITS (9 kinds of the 1,040 traits and 2 universe themes), PEOPLE (stock and calling = one of
512 peoples, body, limbs, height, colours, three gifts, city form and mood = one of 256 cities, diet, temper, tech, how many), LIFE (plant cover and kinds, up to 10 animal groups built from
12,672 kinds), FLORA (the plant book of your planet, up to 8 plants pinned from 536,870,912 named plants), BIO (the BIOFOOTPRINT: health A to F, food web, energy, your people's footprint,
120 years ahead, collapse warnings) and FILE (undo, redo, save recipe, share code, import code, examples, dice, LAND ON IT). UP and DOWN pick a row, LEFT and RIGHT change it, A opens a list,
SELECT rolls the dice for the tab, START undoes. The planet and the footprint panel update live.
- A recipe makes a planet seed (`cr_seed`) and an address (`cr_uni4`: a created universe, star 6). LAND ON IT (FILE tab, from the PC LAB while in the endless galaxy) saves the recipe next
  to the .sav (`<sav>.<NAME>.crp`, `<sav>.seed_<HEX>.crp`) and lands there; `make_world` calls `pc_cr_apply` for that address. The share code is `PL2-...`; `<sav>.code` is read by IMPORT CODE.
- Tests: `pc/creator/test.sh` (recipe to seed, share code round trip, files, undo, footprint, flora names unique) and `pc/tests/creator.sh` (the screen in the real game, landing through
  `bugstar_pc --view --crcode CODE`).

## GIANT NUMBERS and BUGSTARILLION v3

PC LAB (pause, WORLD page, B) then GIANT NUMBERS: a viewer for 2^(2^32) (1,292,913,987 digits), the counting number 1..123,456,790 (1,000,000,008 digits),
the complete name of BUGSTARILLION v3 (6,376,476,945,538,615,314 letters), a namer for any typed number, and a scale ladder. Details, definitions, the record research
and the tests: `pc/giant/README.md`. No digit file is needed or shipped: the game keeps the rule 2^(2^32) and works any digits out exactly when they are shown (proved error bound or exact modular arithmetic, every million-digit block checked against a 10 KB fingerprint table). An optional 100 MB FAST CACHE for the middle is made only when asked.

## SHIPS: the PC ship renderer (pc/shipr) and the far battle
**What it is.** In space and in fleet battles the window build draws the ships itself, at the window's own resolution, instead of the game's
240x160 wireframes: `pc/shipr/shipr.c` (plain C, about 30 KB of source, no image files: every ship, glow and explosion is made from a few numbers).
The game's simulation is untouched (the ships, their fights and the save are the game's; the same keys give the same game in every mode).
`pc/pc_ships.inc` reads the game's ships each frame (`pir[]`, the camera `br/bu/bf`, `cam_o`) and hands them over; build.sh puts two hooks into
`space.inc` (before the pirates: `pc_sr_space`, the game's own drawing is skipped; after them: `pc_sr_mark`, a copy of the page so the cockpit,
HUD and sparks the game draws later stay on top).

**Options** (Esc > VIDEO): **SHIPS** = CLASSIC (the GBA look: wireframes and 2x2 marks on the game's own grid, any number of ships), NATIVE
(lit solid ships, engine glows, beams, shields, explosions with debris) or ULTRA (more parts per ship, highlights, engine trails, shock rings,
glowing beams). **FAR BATTLE** = OFF, 500, 2,000 or 10,000 ships: during a fleet battle a distant battle is drawn around yours (only drawn,
from the system's seed and the frame number; it never covers planets, the station or the HUD). Headless tests: `PC_SHIPS=0|1|2`, `PC_ARMADA=N`.

**How it stays smooth.** Each ship's centre is projected once (the same projection as the game's `sproj`), whatever is behind or off screen is
dropped, and the size on screen picks the detail: a dot under about 1.6 px (sooner in a crowd), a lit arrow under 7 to 11 px, a lit mesh above
(6 to 14 triangles, a depth buffer, no sorting). Beams are glowing lines, explosions are a fireball, a ring and debris from a seed. The picture is
cut into bands of rows that draw on all CPU cores at once (the same picture as one core, tested byte for byte).

**Measured** (`gcc -O2 -o out/srbench shipr/bench.c shipr/shipr.c -lm -lpthread && out/srbench 1280 720 out/sr/b [threads]`; a 1280x720 frame,
a close dogfight of 40 plus a middle cloud plus a far armada; one core of a busy 2-core build machine, so a desktop is faster):

| ships | CLASSIC | NATIVE | ULTRA |
|---|---|---|---|
| 100 | 2.5 ms | 4.3 ms | 7.1 ms |
| 1,000 | 1.2 ms | 5.5 ms | 8.8 ms |
| 10,000 | 2.3 ms | 11.7 ms | 15.8 ms |
| 100,000 | 4.1 ms | 42.8 ms | 57.6 ms |

**Hooks for more ships (warships, armadas).** Other PC code can draw through the same renderer, in the same frame and the same depth buffer:
`sr_on_frame(fn)` registers a function that is called every space frame after the game's ships went in; inside it call
`sr_ship()` / `sr_ships()` for any number of ships (`SrShip`: position, forward and up, half length, colour, kind, team, firing, shield hit,
thrust, `far`), `sr_mesh()` for a big hull (`SrMesh`: vertices, triangles, optional colour per triangle; up to 64 hulls a frame, lit by the sun),
`sr_beam()`, `sr_boom()` and `sr_armada()`. The colossal warships (`pc/warship`) can register their draw this way. See `pc/shipr/shipr.h`.

## The ship editor (PC only, `pc/shipyard`)

On the PC the hangar opens the **SHIP EDITOR** (new game, and SHIP > HANGAR). The old hangar is still there: FILE > OLD HANGAR.

- **Tabs**: SHIP (start from 10 presets, SURPRISE ME dice, SIMPLE / ADVANCED, FLY IT, undo, redo, snap), HULL (1 to 8 sections: length,
  width, height, shape; nose; add, copy, remove, stretch all; symmetry), WINGS (kind, span, sweep, spot, tilt, fins), POWER (engine kind,
  count, size, reactor, radiators, fuel tanks), GUNS (6 mounts: weapon, size, section, place; add, mirror copy, remove), GUARD (shield,
  armour on all or each section), ROOMS (cockpit, hangar bays, cargo pods, crew rooms), PAINT (paint, trim, engine glow, lights, pattern,
  finish), BADGE (name, an 8x8 emblem and wing decal drawn in the editor), FILE (save, load, share code, import, old hangar, done).
- **Live**: a rotating 3D preview of the design itself, 11 numbers (speed, turn, armour, shield, fire, range, hold, fuel, crew, cost,
  THREAT) green when better / red when worse than when you came in, mass, power used/made, heat/cooling, the selected part's cost
  line, and warnings in plain words (underpowered, overheating, too heavy, not enough beds, no guns, small fuel tank).
- **Controls**: UP DOWN row, LEFT RIGHT change (hold for big steps), A open/do, L R tab, SELECT tap = surprise me, SELECT held + arrows =
  turn the view, START or right click = undo, B = done. Mouse: click tabs and rows, wheel scrolls rows or zooms the view, drag the view
  to turn it, drag a section in the HULL plan to stretch it (snap: steps of 2). Gamepad works through the same keys.
- **Files** next to the save: `<save>.slot<N>.ship` (the design for that slot, about 50 bytes), `<save>.<NAME>.shp` (saved designs),
  `<save>.shipcode` (share code `SHIP-...`, or the PC_SHIP_CODE variable). No `.ship` file = the game's own ship, so old saves are unchanged.
- **In play**: `build.sh` renames the 8 ship number functions in ship.inc to `*_g` and wraps them with `pc_sy_stat` (top speed, turn,
  damage taken, turn settling, laser, hold, fuel, push). Smelter hull plates, engine tunes and laser lenses still add on top. The
  game's own ship model gets the nearest look (space chase view). FLY IT: NO keeps the design but flies the old numbers.
- Tests: `pc/shipyard/test.sh` (recipe and share code round trips, determinism, stats bounds, 200,000 random edits, random bytes,
  files) and `pc/tests/shipyard.sh` (all 10 tabs in the real game, two runs match, the design file, and a TANK really takes less damage
  and an INTERCEPTOR really flies faster in the game). Not done yet: designs for allied/enemy ships and boarding interiors.

## COLOSSAL WARSHIPS (pc/warship, pc/pc_warship.inc)

**Where to find them:** go to the PC LAB, pick WARSHIPS, choose a craft, press START (ENGAGE), then take off. Or in space, hold R and press SELECT to open the scanner. THE GREY MOON (the reference station) is on every list. If cheats are on, the list also has an ANY SIZE row. Inside the fight: START changes weapon, SELECT retreats, and you can board once the shields are down (hold R near the hull, then A). On board: arrows to walk and turn, L/R to step sideways, B to run, A to use things, shoot or hold to plant a charge, SELECT for the deck map, the tram platform to move between sectors, START in a hangar to leave.

**Sizes:** z = 100 x log10(length in m), from 1 nm (NANO SWARM) to 10^30 m (MULTIVERSE HULK). Cheats go up to 10^3000 m. There are 19 classes. Hull grammar: 16 shapes x symmetry x segments x 6 add-ons x 8 surfaces x 12 cultures x 10 materials. Over 5,000 seeds this gave 5,000 different designs.

**The one reward formula:** reward = cost x A x Fm x Fw.
- cost = K x (materials + crew pay + aircraft + weapons + power), all built from the mass, about L^3. K = 52500.636.
- A = attack^-4.2032, where attack = level + 5 x laser + gun + 25 x tech.
- Fm = 1 - 0.45 x inside + 0.15 x inside x precision.
- Fw = the damage-weighted mean of (weapon multiplier)^-0.4.
- Fm and Fw are never above 1, so no mix of method or weapons pays more than the outside-only laser kill. The worst mix pays 0.066 of it.

| size | class | cost (attack 1) |
|---|---|---|
| 1 nm | NANO SWARM | about 10^-19 gold |
| 1 m | SKIFF | 82.8 million |
| 1 km | DREADNOUGHT | 63.6 quadrillion |
| 160 km | THE GREY MOON (reference) | 407 sextillion (4.07e23) |
| 10^9 m | STAR HULK | about 4.8e37 |
| 1 universe | UNIVERSE HULK | about 7.6e107 |
| 10^3000 m (cheat) | BEYOND HULK | 1.12e11702, marked CHEATED |

**Calibration (tested):** the reference station, killed outside only with the laser at attack 1, pays 4.0700e23. At attack 1,000 it pays 9.999e10. The ratio is the same at every size.

**Numbers:** WN is a mantissa times 10^exponent. It saturates at 10^(10^15) and says so. Names come from pc/giant (Conway-Wechsler). Gold goes into the game's 64-bit gold while it fits. The rest goes into a vault, and amounts below 1 collect as gold dust. All of this is saved in `<save>.ws<slot>`.

**Fight rules:** regenerating shield layers (when every generator is down, the shields drop), fighter waves (the game's own pirates), point defence, repair crews (stop shooting and the damage on the current section heals; a long absence rebuilds everything), sections in order, phases, an enraged core, heat, ammo and fuel. THREAT gives the attack, toughness, wingmen, kits and fuel you need.

**Interiors:** a tree of sectors, 8 children per node, down to 64x64 decks. A universe hulk is 82 levels deep. Every subsystem has a path and a room. Some rooms are locked: the keycard is in SECURITY, and a crawl vent is the other way in. The crew follow sleep, eat, chat and work shifts, and react to alarms. Guards fight you. Charges blow up subsystems, which changes the fight outside; read the schematic first to hit the weak point. The view is drawn by PC code (pc/warship/ws_view.c) through `pc_ovl_fn` at the window's own resolution.

**Missions and encounters (pc/warship/ms.c, pc/pc_missions.inc).** Find a mission board in the YOUR SHIP menu (MISSIONS), at a station (SELECT), at a town's bounty office, at an outpost (START inside the station board, if the star has one), or in the warship scanner/PC LAB list (START). Seven kinds: DESTROY, SABOTAGE, STEAL PLANS, RESCUE, ESCORT (2 convoy wingmen join the fight), DEFEND, SURVEY. Each place posts 5 jobs from its own seed, with a mix set by the kind of place, the star's economy and the galaxy; some are DREAM jobs far above your level and some are in a FAR galaxy. The briefing shows giver, faction, craft, size class, THREAT, preparation, pay (the one formula x the kind's share x the mission bonus), target star and planet, and an optional deadline in star jumps. The JOURNAL (L/R) lists active missions and abandons them; target stars get a cyan diamond on the galaxy map. Go to the target star and use the scanner: the mission craft is on the list. Inside jobs: stay 3 seconds in the BRIDGE (plans), a TURRET room (captives) or the REACTOR (survey), or blow the named system; get back to a hangar and START to be paid on the cost breakdown. Missions and the done list are saved in the .ws file (old saves load). Cheated sizes stay marked CHEATED.
WARSHIP ENCOUNTERS (L in the PC LAB warships list: OFF, RARE default, NORMAL, COMMON): on a planet or in a dungeon a warship may arrive; it shows on the board and scanner for 10 game minutes. On NORMAL and COMMON big ones bring a PLANET-BUSTER (timer; leave the planet, beat its shield layers or sabotage a turret), orbital strikes or troop waves, with warnings. Not done yet: the burning-planet hellscape, collapsing entrances, portal escapes, dungeon floor invasions and side-game colony destruction (hooks: pcm_planet_lost and pcm_tick in pc/pc_missions.inc). A lost buster costs 1/5 of your gold and cargo.

**Tests:** `pc/warship/test.sh` covers calibration, variety, scaling and names, balance, the bot fights, interior reachability and determinism, and the save format: 78,745 checks, 0 failures. `pc/tests/warship.sh` is the in-game test. `pc/tests/ws_shots.sh` writes interior pictures.
