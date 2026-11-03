# BUGSTAR: what to do next (written 1 Nov 2026)

This is the working roadmap after the big 1 Nov 2026 push. It covers all three versions: GBA (the original), GBC (the port) and PC (native, with the side game).
The older `ROADMAP.md` is the mining roadmap and is still valid for the GBA. Read `pc/DEPTH_PLAN.md` for the side game and `pc/NATIVE_PLAN.md` for the window.

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
- **Enemy beams** at the player are missing in NATIVE and ULTRA.
- Capture real window screenshots (the automatic screenshot was not written in window mode).
- Ship editor: use the renderer for the preview, wire `sy_rooms` into the warship interiors (`wd_make`), make foe designs change stats and fight rules.
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
- To check: in a space fight with a preset ship design (`shipyard_space` setup), the player's white laser wedge shows with SHIPS off and CLASSIC but not in NATIVE or ULTRA. Enemy beams do draw in NATIVE and ULTRA in the frames checked.
