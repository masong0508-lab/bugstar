# BUGSTAR: what to do next (written 1 Nov 2026)

This is the working roadmap after the big 1 Nov 2026 push. It covers all three versions: GBA (the original), GBC (the port) and PC (native, with the side game).
The older `ROADMAP.md` is the mining roadmap and is still valid for the GBA. Read `pc/DEPTH_PLAN.md` for the side game and `pc/NATIVE_PLAN.md` for the window.

## UI safety and planet feedback — 22 November 2026

- Bound all displayed character-creator hints, hangar rank messages, local NPC
  gifts and taglines, and the planet arrival card fields. Prior compiler builds
  identified overflowing literal and dynamic messages; the new calls never
  write beyond their destination arrays and preserve readable prompts.
- Restore the world-tier and hell-tier suffixes hidden by an accidental
  `#endif` trailing expression. Reserve their display space even for long
  generated world names. Correct moon bob timing to divide by
  `(orbital_period >> 5) + 1` instead of the unintended `orbital_period >> 6`.
- Run `tests/ui_text_bounds_runtime.py` in PC headless CI. It extracts the
  production C bounded-string functions and actual planet HUD, then checks
  edge capacities, integer formatting, long names, tier priorities, TEST_HASH,
  and on-screen labels under ASan/UBSan. It also audits the real NPC, hangar,
  arrival and creator call sites. No new framebuffer, fixed array or save field.
  Platform build and emulator checks are required before release packaging.

## Space flight and combat continuation — 21 November 2026

- **Steering that settles (GBA/PC/Android):** integer easing completes its final
  step, so releasing a turn stops yaw/pitch rotation and levels the bank. Angle updates
  round symmetrically in both directions. Ship-specific inertia is retained.
- **Complete braking and asteroid matching:** R stops residual drift. Beside a
  fast belt rock it reaches the rock's velocity instead of stalling just outside
  the landing threshold; atmospheric nose alignment no longer undoes the match.
  Ordinary unbraked coasting and the existing speed limits are retained.
- **Thrust for every ship preset:** fractional engine force is preserved using
  the existing logic clock, with equal impulse in opposite directions. Previously
  weak engines could lose all positive-axis thrust while negative-axis thrust
  rounded up. This adds no persistent accumulator or save fields.
- **Safe assembly of complex ships (GBA/PC/Android):** failed vertex additions
  return an invalid index instead of reusing the last vertex; edges and faces
  reject invalid references before storing them. Rings and boxes require room
  for their full vertex group. This fixes an actual out-of-bounds model read
  found by ASan with shipyard preset 9, using the existing 128-vertex buffers.
  Models that fit retain their original allocation order and geometry.
- **Readable incoming fire (PC/Android):** NATIVE and ULTRA enemy beams meet the
  player's actual world position in chase view. They previously aimed at the
  cockpit's bottom-edge position, visibly below the ship. Cockpit and CLASSIC
  beam placement remain unchanged. No new framebuffer or actor allocation.
- **Consistent native space frames (PC/Android):** publish a completed space
  page before its existing display wait, keeping its bitmap, background,
  ships and effects together on every display blank. The old order briefly
  showed the previous page after the single native background buffer had
  advanced, then dropped native layers when its snapshot expired. Completion
  is tracked through normal flight, initial launch and the Coil test loop,
  including filming mode. Early exits cannot publish an unfinished page;
  CLASSIC ordering and the compositor's stale-frame limits are retained.
- **Companion GBC space pass:** [PR #13](https://github.com/masong0508-lab/bugstar-gbc/pull/13)
  makes all 256 worlds actionable in both flight views, preserves the full
  pirate reinforcement delay, and keeps the cockpit's control/destination row
  visible after dialogue. Controller-only chart, warp, station, undock and
  landing replays at world 255 produce valid autosaves in both views. Fixed
  WRAM decreases by two bytes; saved world IDs and save format stay unchanged.

The production model, flight, beam and presentation paths are covered by
`tests/ship_model_capacity_runtime.py`,
`tests/space_flight_runtime.py`, `tests/space_native_beam_runtime.py` and
`tests/space_native_present_runtime.py`, wired into the PC workflow together
with `pc/tests/native_space_lifetime.sh`. Actual space captures and handheld
control checks use the game's existing scene/debug entry points and ordinary
controller input.

Validation for this continuation:

- The model regression checks 96,160 production models under ASan/UBSan,
  including all game/PC presets, 94,080 valid part combinations, 2,048 generated
  designs and allocation boundaries. All 94,077 models that fit are byte-for-byte
  identical to the previous builder; 2,083 exhausted models keep valid indices.
  Five original/missing-guard negative controls fail. Actual CLASSIC captures
  no longer contain the malformed polygon; native custom-hull pictures and
  the HUD are unchanged by this capacity correction.
- The flight regression checks 143,430 actual movement updates across all ten
  ship presets, including released controls, mirrored turns, braking, drift
  matching, speed limits and fuel. A separate exact impulse oracle checks
  33,221,000 cases over 1,080 complete fractional cycles, including clock wrap,
  oblique headings and custom-engine limits. Eight isolated regressions fail
  against the original or deliberately incomplete fixes.
- The real native ship renderer passes 4,000 chase-target checks and 8,000
  cockpit/CLASSIC comparisons, with six render configurations. Actual NATIVE
  and ULTRA space captures show incoming beams meeting the player hull. Held-A
  captures also confirm that the existing player bolts and ULTRA glow render
  correctly throughout their firing phase; sparse captures had missed them.
- Native publication passes 1,160 checks through the actual scheduler and
  guarded caller sequences. The real-game lifetime smoke passes 15 cases and
  1,018 captured display blanks on both the final normal and ASan binaries:
  all four flight phases, turning/firing, chase/cockpit, NATIVE/ULTRA/CLASSIC,
  galaxy-menu return and landing/walking/shop/launch. One/four-thread pixels
  and saves match. The old binary fails on its expired age-five snapshot.
  Existing cache/shading/effects parity also passes at 960x640 and 480x320.
  Local LeakSanitizer scanning is disabled for the traced runner; address and
  bounds instrumentation remain active.
- Five fresh-boot mGBA scenarios exercise the production ROM through ordinary
  controller input, totaling 9,554 emulated frames. Released yaw/pitch rates and bank
  now reach zero; equal left/right input gives equal opposite turns. R braking
  reaches zero velocity instead of retaining forward drift. Firing and both
  cockpit/chase transitions also pass. The comparison uses identical emulated
  and simulation frame counts. All 39 final-ROM checkpoints and sampled flight
  states match the earlier flight fix after the model-capacity correction.
  This is a bounded flight smoke, not a campaign run.
- The final production GBA build adds 3,752 bytes of ROM code. Data/BSS remain
  564 / 263,684 bytes. IWRAM stack reserve is 1,664 bytes (eight bytes below the
  preceding kart pass), above the 1,500-byte project requirement. Model buffer
  sizes, save layout and persistent RAM are unchanged.

## Kart road projection and filtering — 21 November 2026

- **Ordered ground depth (GBA/PC/Android):** retain the unsigned reciprocal
  when interpolating scanlines. Casting the reciprocal for two rows to a signed
  value reversed the interpolation and made road edges repeatedly step backwards.
  The reciprocal for one row now saturates instead of overflowing to zero.
- **Clipped camera endpoints:** preserve the actual projected endpoints when
  the profile starts below the screen or crosses above it; clip only the rows
  written. This fixes stretching at high camera settings. Spans beyond the
  reciprocal table use bounded integer division instead of indexing past it.
- **Continuous native filtering (PC/Android):** retain each texel's color in
  its interior and blend at its boundaries. The previous weights switched
  neighbors at the texel center, creating sharp repeating ripples. Both native
  sampler paths are corrected. Road-marking emphasis, distant sampling and the
  flat-color fast path remain intact. Sample footprints now use game-space
  screen center 120 at every output resolution.
- **Build and regression coverage:** GBA incremental builds now depend on
  `m7.inc` and `recip.inc`. The PC workflow watches both files and exercises the
  real ground-depth code and native filters against independent references.

Validation for this follow-up:

- The shared-depth regression exercises 3,496 frames and 559,360 row checks
  under UBSan, including every camera/distance preset, spans of 1/2/257/258
  rows, large clipped spans and guarded buffers. An independent flat-plane
  oracle stays within 1.037 screen rows. Restoring either reciprocal defect or
  the old clipping independently makes the test fail.
- The native filter regression checks 391,038 texel centers/interiors, 72,490
  independently integrated color samples, 20,500 monotonic ramp samples and
  5,120 screen-center footprints. It also covers far sampling, edge clamping
  and the road-marking threshold; the original filter and footprint each fail
  an independent negative control. Existing sampler, inverse, camera and
  sprite-anchor regressions pass.
- Eight automated PC race scenarios and four further scenarios under
  AddressSanitizer end with zero reported problems. Native composition at four
  checkpoints also passes ASan. Local leak detection is disabled because this
  traced runner cannot run LeakSanitizer; bounds instrumentation remains active.
  Actual native and classic SDL frames at 320, 600, 900 and 1200 show the
  corrected road geometry and reduced texture ripple. Native pictures match
  exactly with one and four rendering workers.
- Local 960x640 composition averages 8.58 ms with one worker versus 10.03 ms
  before. Three paired four-worker runs give median averages of 3.54 ms versus
  4.10 ms before; individual runs vary with scheduling. These measure renderer
  composition on this host, not whole-game FPS or handheld performance.
- The production GBA ROM passes a real mGBA smoke through 2,954 frames,
  including Kart World, acceleration, steering and braking. The five title/menu
  checkpoints are pixel-identical to the prior build; race images change as
  expected. This is a bounded runtime smoke, not a full handheld race.
- GCC 12.2 builds with unchanged data/BSS (564 / 263,684 bytes). Loaded kart
  code grows by 72 bytes, reducing the IWRAM stack reserve from 1,744 to 1,672
  bytes, still above the project's 1,500-byte requirement. No persistent
  buffers, framebuffers or save fields were added.

## Kart reliability continuation — 21 November 2026

- **Reusable race items and editor markers (GBA/PC/Android):** retired shells,
  slime and gate markers reuse their existing borrowed scenery slot. Previously,
  repeated reuse appended duplicate ownership entries until the 48-slot list
  filled, after which new items or gate markers silently failed. Active racers,
  scenery and item boxes waiting to respawn remain protected. No production RAM
  arrays, save fields or framebuffer allocations were added.
- **Coherent native kart sprites (PC/Android):** chassis, driver, roof and cloud
  effects move together using the kart's ground contact; scenery uses its feet.
  Jump lift stays intact. Pending contacts are published when OAM is flushed,
  so constructing the next frame cannot move sprites still on display. This
  uses 1 KiB of PC renderer metadata and does not change GBA memory.
- **Companion GBC fix:** [PR #11](https://github.com/masong0508-lab/bugstar-gbc/pull/11)
  restores the shield's intended 420-tick duration instead of byte-truncating it
  to 164. An unused byte offsets the wider timer; fixed WRAM remains unchanged.

Validation performed for this continuation:

- `python3 tests/kart_object_pool_runtime.py`: 10,000 item recycling cycles,
  full-capacity reuse, 1,000 real editor drop/undo cycles and scenery restoration
  under UBSan. The original allocator fails the regression.
- `python3 tests/kart_native_sprite_anchor_runtime.py`: 154,791 projection
  checks and 126 compositor fixtures, including affine sprites, jumps, fixed HUD
  placement and pending/live frame isolation. The old per-sprite warp fails.
- Eight full automated PC race scenarios and four further scenarios under
  AddressSanitizer ended without hangs or reported errors. Local leak detection
  was disabled because the traced runner cannot run LeakSanitizer; bounds
  instrumentation remained active. Actual SDL captures at frames 320, 600, 900 and 1200 were inspected;
  classic captures are byte-identical to the baseline. One- and four-worker
  native pictures are identical.
- GBA ROM built with GCC 12.2: data/BSS and the 1,744-byte stack reserve are
  unchanged; ROM code grows by 24 bytes. A real mGBA run exercised title,
  Kart World, random race, acceleration, water recovery, steering and braking
  across 2,954 emulated frames; all ten checkpoints match the baseline.

The emulator check is a bounded runtime smoke, not a complete handheld race or
hardware performance measurement. That pass did not change the road sampler;
the follow-up above repairs the depth and filtering defects behind its ripple.

## 0. Where things stand

**GBA (root `main.c` + `*.inc`)**
- Done: editable difficulty (13 numbers, saved per difficulty); Bugstarillion v2 (11,896,480 parts); level cap is the 32-bit limit (`LEVEL_CAP`, 4,294,967,295; PC overrides it); kart fixes (results stack overflow, circling rivals, lap counting, table over-read, road ground left on); Stormclock Coil (8-step quest, 1 in 10,000 drop with 6 conditions, speed tiers up to STORM at 2.3759 x 10^43 c, exact readout, 9 detail pages, Planck-time counts, pass-through effects with an Options setting, glitches with FLASH-SAFE); living stat bars; mini stat panel hidden by default (Options: MINI STAT BARS); ship points spend by MAX/100/50/5/1 (LEVEL tab in the hangar).
- Hard limits unchanged: IWRAM and EWRAM are full (new code and tables go in ROM), stack headroom must stay above 1500 bytes (last read 3,248), SRAM blocks do not move, every new block needs a version.

**GBC (`bugstar-gbc`, GBDK-2020)**
- Done: town places, PORTAL spell, PARITY.md, the kart jump-pad sprite fix (compiled, not played).
- Not ported yet (see PARITY.md): humanoid form and callings, landable asteroids and mines, keycard outposts, kart garage, IDENTIFY, WORKBENCH. The GBC HOW BIG page is excluded. `PARITY.md` still says karts are not ported; that is out of date (the GBC has simple karts).

**PC (`pc/`)**
- Done: SDL2 window (resizable, fullscreen, hi-DPI, vsync, keyboard, gamepad with hot-plug and rebinding, mouse, text size, high contrast), per-OS saves, NATIVE kart view, ship renderer (CLASSIC/NATIVE/ULTRA, 10,000-ship far battles), content x16 (1,040 traits, 112 stats, 512 peoples, 256 cities, 12,672 creature kinds, 536,870,912 plants), meaning layer (32 channels, roles, synergies), 146 universe laws with editor, HOW THIS WORKS, CHEATS, planet creator with biofootprint and share codes, side game Phases 1 to 5 (town, seasons, friends and letters, colony ladder, failure and rewards, 100-year catch-up), 48 god powers (power n at level 1000 x n), 64^256 level (`Lv`), giant numbers viewer (2^(2^32), 1,292,913,987 digits) and Bugstarillion v3, colossal warships (19 sizes, 16 hulls, interiors, reward formula, threat, cost screen), missions board and random encounters, ship editor, 1,565 facts pages.
- Windows exe: compiled only, never run.
- Status 1 Nov 2026 (late): warship aftermath (hellscape, sealed entrance, escape, floor invasions, boarders), colony map screen, side game DIFFICULTY, native dungeon, kart NATIVE ground, window screenshots, facts pages refreshed (1,571 total), planet creator crash fixed, all pc/tests suites re-run and passing.

## 1. Do first (small, high value)

1. **Run the Windows exe on a real Windows machine.** Fix whatever breaks (paths, audio, `VirtualAlloc` memory map, gamepad). Add an auto-built CI job for Linux and Windows packages.
2. **Regenerate and re-run the whole PC test set** (`pc/tests/*.sh`, `pc/*/test.sh`). Several suites were skipped near the end: `coil.sh`, `giant.sh`, `giant/test.sh`, `warship.sh`, the older sim tests after the last rebase. Fix stale goldens (`seeds.golden`, `laws.golden`) once, in one commit.
3. **Re-run the GBA coil fuzz and kart fuzz on the final ROM** (the fuzz passed on earlier ROMs, not on the last merge).
4. **Play the GBA ROM start to finish** once: new game, level up, spend ship points, MINI STAT BARS toggle, kart race, coil quest log. Look for anything that looks wrong at the HUD.
5. **Fix `PARITY.md`** (karts exist on the GBC) and write the GBC jump-pad fix into its notes after playing it.

## 2. Warship threats: DONE except balance (PC, `pc/pc_missions.inc`, `pc/warship/`)

The planet-buster countdown, orbital strikes and troop waves are in. The extreme end is only a documented hook (`pcm_planet_lost`, `pcm_tick`):
- **Hellscape:** a severe bombardment turns the whole surface into fire (use the hell-world look, weather and per-second damage).
- **Collapsing entrance:** a chance that the dungeon entrance is rubble-sealed; other ways out (dig, another stairway, portal).
- **Portal escape:** arrive on the burning surface with a survival timer from HP, armour, shield, fire resistance and supplies; ship may be intact, damaged or destroyed (escape pod, wingman pickup, or stranded).
- **Floor invasions:** troop waves that hit your dungeon floor and every floor above you in turn.
- **Boarding party** fight inside the dungeon at high threat.
- **Death penalty only:** wake at the nearest station with the stated gold and item loss; never corrupt the save; story worlds never permanently destroyed.
- Gate everything with WARSHIP ENCOUNTERS (OFF/RARE/NORMAL/COMMON) and side game DIFFICULTY.
- Tests: bot runs with and without preparation; the entrance-collapse path; the save is byte-identical apart from the stated penalty.

## 3. Side game: Phases 6 and up remain (PC, `pc/sim/`)

- **Colony map screen:** the 8 x 6 grid exists in code but has no screen. Add placement ("choose where"), selecting a building, walking and carrying (Keflings-style) instead of menu actions.
- **Permanent ruins:** a destroyed planet stays a burnt world in the colony records, resettle elsewhere, and the planet file shows it.
- **Invasion hook:** DEFEND missions are only a flavour tag; connect them to the colony's raids once an invasion API exists.
- Phases 6 and up from `pc/DEPTH_PLAN.md`: workshops and the mine with z-levels, walls and raids, the market, WorldBox-style races/tribes/powers using the 12,672 creatures and 48 god powers, the replay layer and sandbox (all powers open).
- Put side game DIFFICULTY in the main Options (needs `pc/gen/main.c` and the options save).
- Pass the count of open god powers into the meaning profile (done through `pcx_gods`; check it with a test).

## 4. PC rendering and feel (`pc/platform`, `pc/shipr`)

- **Native-resolution 3D** for the dungeon, planet and space views (Seam 6 in `pc/NATIVE_PLAN.md`). They still draw at 240 x 160 and are scaled.
- **Kart NATIVE view** is about 34 to 44 ms per frame at 1200 x 800 on a 2-core box; get it to 60 fps (more threads, tile cache, fewer per-pixel branches). Add banking, hills and shadows.
- **CLASSIC far fleets** at 10,000 ships turn into solid blocks; use a density map instead.
- **Enemy beams:** present in NATIVE and ULTRA; chase-view alignment is corrected
  in the space continuation above.
- Capture real window screenshots (the automatic screenshot was not written in window mode).
- Ship editor: use the renderer for the preview, `sy_rooms` now drives the warship interior builder (`wd_make_mix`, patch 18; the game does not call it yet), make foe designs change stats and fight rules.
- Flash-safe and photosensitivity check on the NATIVE glitches.

## 5. Giant numbers (PC, `pc/giant/`)

- Run the Python reference tests (`pc/giant/test.sh`) and compare the window at 1,000,000 digits against Python.
- Test the cache build and delete buttons, the full text export (1.3 GB) with its SHA-256, and the v3 screens.
- Speed up the worst-case middle window (151 to 190 s and 2.5 GB RAM): chunked exact computation, more checkpoints inside the 100 MB budget.

## 6. Content and the facts screen

- DONE: `pc/facts/09_pc.py` now quotes the x16 numbers; hellscape, threat and native view pages added.
- The in-game text of each new PC screen needs a pass for plain words.
- Add the planned page counts to the GBC (the GBC HOW BIG page is excluded on purpose).

## 7. GBC parity (`bugstar-gbc`)

Port in this order, one feature per build, `make -j3 GBDK_HOME=/opt/gbdkx/gbdk` takes about 4 minutes, check with `/home/claude/tools/walnut/gbrun`:
1. Humanoid form and callings.
2. Landable asteroids and mines.
3. Keycard outposts.
4. Kart garage.
5. IDENTIFY and WORKBENCH.
6. The new HUD (living bars, mini panel option) and ship points if ROM bank space allows.
Keep PARITY.md the single source of truth.

## 8. GBA ideas still open

- The older `ROADMAP.md` list (link check against the real runtime library, ore grade stacks and gem sockets, overlapping the dungeon and planet buffers to free about 38 KB of EWRAM, then ground resolution 128).
- Smoother GBA kart road, a planet horizon, less flicker (asked for, not done; the PC has a better look).
- A planet-surface turn inversion report from earlier was not reproduced; ask for the view and situation if it is seen again.
- Gyroscope and tilt controls were rejected.

## Rules that stay

- Small steps; push to main with `git fetch origin && git rebase origin/main && git push -q origin <branch>:main` (never push without `:main`; never `git stash`).
- Never touch the GBA ROM build when working on PC-only code (guard it).
- The core game is never overridden by the side game: with SIDE GAME off the save is byte-identical (`pc/tests/sim_off.sh`).
- Facts pages: font only A-Z 0-9 space . , ' : ( ) - ^, no plus sign, title at most 28, lines at most 58; run `python3 pc/facts/check.py`.
- Commit messages end with the Co-Authored-By and Claude-Session lines.

## Status 3 Nov 2026 (test run)

- Passing: determinism, laws, inputs, creator, screens, worldfile, coil, giant, warship, aftermath, shipyard, shipyard_space, sim_friend, sim_lab, sim_town, sim_unlock, sim_collect, kart (12 races, 0 problems).
- Do not run test suites in parallel: they share `pc/gen`, and a parallel `kart.sh` build fails.
- `seeds.golden`: 4 picture hashes refreshed (2 trait planets, only the distant city skyline rows differ: building windows changed in `planet.inc`). Text lines unchanged.
- `sim_off.golden`: refreshed from the current build with the side game OFF. It is no longer proof of the pre-side-game picture (drift from frame 2800, cause not found); the ON vs OFF `.sav` equality and no-`.sim`-when-OFF checks still pass and are the real isolation proof.
- Rechecked in the November space pass: the preset-ship player bolts are visible
  in both NATIVE and ULTRA. Held-A versus idle captures across four firing
  phases show the paired bolts and ULTRA glow; sparse captures between bursts
  can miss them. Enemy beams are also present, with their chase-view endpoint
  corrected by the continuation above.
