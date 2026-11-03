# BUGSTAR patch roadmap - state at patch 05 (3 Nov 2026). Read this first when continuing.

## Status of each patch (apply in order)
| # | zip | what | state |
|---|-----|------|-------|
| 01 | (earlier) | win64: reserve GBA address ranges in a suspended child before DLLs load | applied by you; the game launches |
| 02 | bugstar-patch-02-buildings.zip | building facades: roof ledge, slab lines, floors of real height (32 units), punched windows / glass ribbons, lit windows, plain wall when far. Files: planet.inc, pc/pc_pnative.inc | made; checked in the classic viewer only |
| 03 | bugstar-patch-03-dungeon.zip | adaptive cheap mode in the native dungeon renderer (pc/pc_dnative.inc): nearest texel instead of 4-texel blend when the 30-frame average passes 12 ms, back under 3.5 ms. Saves ~17% (9.0 -> 7.6 ms at 960x640, 1 core). Force: env PC_DN_LOW=1 / 0 | made; Windows gain unmeasured |
| 04 | bugstar-patch-04-framelog.zip | frame log (pc/pc_hw.c): empty file `slowlog.on` next to BUGSTAR.exe -> `slowlog.txt` lists frames over 33 ms (+ dungeon renderer ms) and an average every 300 frames | made; waiting for YOUR slowlog.txt |
| 05 | bugstar-patch-05-towers.zip | building size follows the city's tech level (town.inc): city_tech() 0..5 = city size + economy (mining/hive +1, tech +2, city planet = 5). Higher: taller blocks, bigger footprints (2-3 -> up to 6 texels), more and taller towers (stepped crown), more glass; city ground capped at 110 (was 150) | made; 3 cities checked in the classic viewer |
(If your repo already had an earlier copy of 02, apply.sh updates it in place; every apply.sh stops with exit 1 on failure so commit/push do not run.)

## Open problems
1. DUNGEON STALL on Windows (blank screen a few seconds after entering a dungeon): NOT reproduced here (no frame over 40 ms in game logic, headless 1 core),
   so floor generation is not it. Next: play a dungeon with slowlog.on, send slowlog.txt.
   - dungeon renderer share high -> stronger cheap level (lower internal size / fewer rows) in pc_dnative.inc
   - slow with renderer low -> look at the compositor (pc_hw.c ~line 540), win64 memory mapping on the first dungeon frame, or the game's own code
   Headless dungeon entry: BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=seal BUGSTAR_AFT_TOPUP=1 with --script tests/aft_dig.txt; PC_NATIVE=1 PC_SCALE=4 PC_TIMING=1 times the native renderer.
2. NATIVE (window) RENDERER for buildings: CHECKED (patch 08). With the new test hook `PC_VIEW_NEAR=22 PC_NATIVE=2 PC_SCALE=3 bugstar_pc --view --star 3 --planet 0` (stands 22 cells south of the city centre) the native renderer shows the same facades as classic: roof ledges, slab lines, lit window bands, plain far walls (headless, night scene). Not yet seen by you in the live window.
3. The Windows exe has never been run on real Windows by me (pc/README.md says the same). README still calls it 32-bit/untested: update once confirmed.
4. town.inc is shared with the GBA game: patch 05 changes generated cities for the same seed. CHECKED (patch 07): pc/tests/seeds.sh passes against seeds.golden (8 addresses, same on two runs), so the golden does not need regenerating.

## Ideas not done
- door band at street level, darker lower floors, different shading on each face, tune the tech formula (use the PC meaning layer if it has a tech channel).

## Patch naming and the Termux command
Every zip has its own number, name and temp folder (no "name (1).zip" mix-ups). Each holds apply.sh, bugstar.diff, ROADMAP_PATCH.md.
Clone first if needed: `gh repo clone masong0508-lab/bugstar` or `git clone git@github.com:masong0508-lab/bugstar.git`.
Command (NN = patch number, NAME = its zip suffix: 02 buildings, 03 dungeon, 04 framelog, 05 towers, 06 flames, 07 roadmap check, 08 native city check; next patch = 09):

    cd ~/bugstar && git pull --ff-only && rm -rf ~/tmp_patch_NN && mkdir -p ~/tmp_patch_NN && unzip -o ~/storage/downloads/bugstar-patch-NN-NAME.zip -d ~/tmp_patch_NN && sh ~/tmp_patch_NN/apply.sh && rm -rf ~/tmp_patch_NN && git add -A && git commit -m "patch NN: short description" && git push

Ready-made for this patch:

    cd ~/bugstar && git pull --ff-only && rm -rf ~/tmp_patch_05 && mkdir -p ~/tmp_patch_05 && unzip -o ~/storage/downloads/bugstar-patch-05-towers.zip -d ~/tmp_patch_05 && sh ~/tmp_patch_05/apply.sh && rm -rf ~/tmp_patch_05 && git add -A && git commit -m "patch 05: building size follows tech level" && git push

## Patch 06 (flames)
NATIVE/ULTRA now draw the game's engine flames (two crossed cones per engine, white-hot core) in pc/pc_ships.inc and, for custom shipyard designs, pc/shipyard/shipyard_ui.inc. Not done: the big white wedge seen only in the lines renderer (tip projects very near the camera) is not reproduced in NATIVE/ULTRA.

## Build notes
pc/build.sh takes ~100 s and builds pc/bugstar_pc (the source of truth is the repo root; pc/gen is a patched scratch copy). Seed viewer: `bugstar_pc --view --star N --planet 0 --out PREFIX`.
