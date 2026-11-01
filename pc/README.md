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

### Built so far: `pc/content/` (works on its own, not wired into the game yet)

- `bignum.h`: a small big number (5632 bits, no library): add, multiply, shift, compare, decimal and scientific print. Used for distances, trips and counts.
- `pc_content.c/.h`: the 8x-bits address (64 words for the universe, 39 per axis), `addr_seed()` that hashes every word, **65 planet traits in 9 categories**
  (3 to 6 per planet, one per category, 32 impossible pairs such as DEAD SUN + OCEAN WORLD), planet effects (gravity, day length, temperature, danger, loot, life),
  planet names, and **24 universe themes** (two per universe, no clashing pairs such as LOW GRAVITY + HEAVY HOLLOW).
- `content_test.c`, `test.sh`: checks the scale table above by computing it (all 12 numbers match), that flipping any one of the 4544 address bits changes the seed
  (average 31 of 64 bits), 1,000,000 random planets (valid, repeatable, every trait appears, 0 bad) and 200,000 universes (every theme appears, 0 bad).
  Run `pc/content/test.sh`. `pc/content/sampler.c` prints sample worlds: `pc/content/test.sh 5` builds it and prints five.
- Next: wire the traits into terrain, lighting, monsters, loot and quests, and the themes into the rules; then quests, gear sets and monsters from parts.
