# BUGSTAR PC — plan

Short version: one engine, two looks, a universe made by math instead of by hand.

## 1. The big idea

- Keep the GBA game's C code as the "brain" (rules, items, quests, saves, seeds).
  It already runs on plain C, so the PC build can reuse it.
- Add a new "eyes" layer (graphics, sound, input) for PC.
- Two looks, one switch in Options:
  - **CLASSIC**: the GBA look, sharp pixels, same dither, same planet tables,
    drawn big on a modern screen.
  - **POLY**: real 3D polygons, real lights, real shadows, water, weather.
- Same seeds give the same worlds in both looks. A save works in both.

## 2. Language and tools

- C (reuse the GBA code) plus a thin C++ layer for the new parts.
- SDL3 for window, input, sound.
- Graphics: OpenGL 3.3 first (runs on old PCs), Vulkan later.
- Builds: Windows, Linux, Mac. One CMake project.
- Music: play the same sequenced songs (small), plus an optional better synth.

## 3. The universe (the "squared" part)

Layout, from big to small:
- U universes (start with 64, can grow to 65,536).
- Each universe has G galaxies (start 4,096).
- Each galaxy has stars, each star has planets and moons.
- Every place has a 64-bit seed made from its address: (universe, galaxy, star, planet).
  Nothing is stored. It is made when you arrive and thrown away when you leave.

Distance rule:
- Distance between two universes grows by the square of the universe count,
  so more universes means a much longer trip. Plan:
  `trip_length = base_distance * (U_count ^ 2)`.
- Nobody flies that far by hand. Travel types:
  - Cruise: real flight inside a star system.
  - Warp: between stars (takes minutes).
  - Fold (the GBA already has this): between galaxies.
  - Rift: between universes. Needs fuel, parts and a quest. Long cooldown.
- Positions use two numbers (double-double) plus a "floating origin", so
  nothing shakes or jumps when you are far from the middle.
- Save file stores only: where you are, what you changed, what you own.

Honest note: nobody can hand-make this much. It works only because worlds are
made by rules (see section 5). I will not promise every world is deep.

## 4. Native 3D polygon look ("POLY")

- Planets: sphere made of 6 square patches (cube-sphere), split into smaller
  patches as you get close (quadtree). Height comes from the same noise the
  GBA tables were made from, so a planet looks like the same planet.
- The GBA "planet tables" (hfine, cmap, planet_img) become:
  1. a small generator that rebuilds the tables on load (exact, CLASSIC look)
  2. a GPU version of the same math that makes heights and colors live (POLY look)
- Dungeons: same maps as the GBA, but walls and floors become real meshes
  with real lights from the torches, lamps and crystals the GBA already places.
  The ponds, the pillars in water, the caves all come over as they are.
- Bugs, monsters, sprites: start as billboards (flat, like the GBA), then
  upgrade to voxel or low-poly models one family at a time.
- Space: ships and stations as meshes. Same flight rules as the GBA.
- Camera, FOV, view distance, shadow quality, and frame cap are all in Options.
  Target: 60 fps on a 10-year-old PC at medium, 144 on a new one.

## 5. "More content, many times over"

Honest version: hand-made content grows by a few times. Rule-made content
can grow by thousands of times.

Rule-made (the big win):
- Monsters: build from parts (body, legs, head, trait, element) -> thousands.
- Planets: biomes, weather, day/night, life, ruins, resources from the seed.
- Quests: templates with slots (who, where, what, why, reward) -> endless, but
  each template is hand-written so it stays readable.
- Gear: prefixes, suffixes, sets, uniques from tables (the GBA already does this).
- Towns, shops, names, lore lines, rumours: grammar-based text.
- Dungeons: more room styles, traps, puzzles, bosses made from mix-and-match.

Hand-made (a smaller list, done well):
- Story zones, big bosses, the ending, special planets, each universe's "theme".
- Each universe gets a theme (rules change: gravity, bugs, magic, hazards).

## 6. Steps (in order)

1. Make the GBA code build on PC with a fake screen. Prove saves match.
2. CLASSIC look in a window (software draw, scaled, same dither). Full game plays.
3. Input, sound, options, controller. Steam-style save folder.
4. POLY dungeons (meshes + lights). Switch between looks live.
5. POLY planets (cube-sphere + LOD) and space.
6. Universe layer: addresses, seeds, rifts, floating origin, universe themes.
7. Content generators (monsters, quests, planets). Add in small batches.
8. Tools: seed viewer, world sampler, automatic crash/soak test that flies
   thousands of random trips.
9. Polish, performance, packaging.

## 7. Risks

- GBA code leans on GBA memory and timing. Needs a clean layer between "rules" and "hardware".
- Huge numbers: math must be checked for overflow far from the origin.
- More content means more bugs. Soak tests are a must from step 8, earlier is better.
- Scope. This is a long project. Each step above should ship something playable.

## 8. What I can start now

- Step 1: a PC build of the GBA rules code (fake screen) in a new folder.
- A seed-to-planet viewer so you can see one planet in both looks early.
