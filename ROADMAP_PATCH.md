> UNFINISHED WORK: read pc/ROADMAP_TODO.md first (patches 14-16: life sim ties, dungeon calls, the open list).

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
Command (NN = patch number, NAME = its zip suffix: 02 buildings, 03 dungeon, 04 framelog, 05 towers, 06 flames, 07 roadmap check, 08 native city check, 09 shipyard overlap fix, 10 ship lighting; next patch = 11):

    cd ~/bugstar && git pull --ff-only && rm -rf ~/tmp_patch_NN && mkdir -p ~/tmp_patch_NN && unzip -o ~/storage/downloads/bugstar-patch-NN-NAME.zip -d ~/tmp_patch_NN && sh ~/tmp_patch_NN/apply.sh && rm -rf ~/tmp_patch_NN && git add -A && git commit -m "patch NN: short description" && git push

Ready-made for this patch:

    cd ~/bugstar && git pull --ff-only && rm -rf ~/tmp_patch_05 && mkdir -p ~/tmp_patch_05 && unzip -o ~/storage/downloads/bugstar-patch-05-towers.zip -d ~/tmp_patch_05 && sh ~/tmp_patch_05/apply.sh && rm -rf ~/tmp_patch_05 && git add -A && git commit -m "patch 05: building size follows tech level" && git push

## Patch 06 (flames)
NATIVE/ULTRA now draw the game's engine flames (two crossed cones per engine, white-hot core) in pc/pc_ships.inc and, for custom shipyard designs, pc/shipyard/shipyard_ui.inc. Not done: the big white wedge seen only in the lines renderer (tip projects very near the camera) is not reproduced in NATIVE/ULTRA.

## Patch 09 (shipyard overlap)
Your video showed the old BUILD YOUR SHIP / MAKE YOUR BUG screens painted over the new shipyard (its tabs, list and preview still showing around them). The old screens only paint their own panel, so the shipyard now wipes both pages when it hands over (FILE > OLD HANGAR, and every other exit). Checked headless: FILE > OLD HANGAR shows a clean hangar; tests/shipyard.sh still passes. Not seen by you on Windows yet; the ship preview itself looked fine headless.

## Patch 10 (ship lighting)
The ship models looked lit from below (bright belly, dark top and nose). Cause: ship.inc ship_draw (and pc_sr_player in pc/pc_ships.inc) flipped the light by screen winding, which is backwards for this model. Now each face is lit on the side that faces the camera, and the ambient level is 8 (was 11) so the lit top is not washed out. Changes the GBA ship look too (ship.inc is shared). Checked in the old hangar preview, 6 angles; the native/ULTRA stock-ship path got the same edit but was not looked at.

## Build notes
pc/build.sh takes ~100 s and builds pc/bugstar_pc (the source of truth is the repo root; pc/gen is a patched scratch copy). Seed viewer: `bugstar_pc --view --star N --planet 0 --out PREFIX`.

## Patch 17 (warship balance)
New `pc/warship/bal.c`, `bal_rules.h`, `bal.sh`, `BALANCE.md`; changed `pc/pc_missions.inc`, `pc/sim/sim_glue.inc`, `pc/warship/test.sh` (runs the report). The report showed that a LATE player met a planet-buster every 36 to 48 minutes (NORMAL / COMMON), that the hellscape ship roll used armour upside down, and that DEFEND raids were far beyond what a colony can hold. Now: per-minute chance cap (RARE 20, NORMAL 45, COMMON 70 in thousandths), 25 minute quiet time after a severe threat, busters 1 in 6 of the severe rolls, window at least 5 minutes, ship/pod roll by armour, DEFEND raid 6 + 4 x class (capped at 6/5 of the colony defence on NORMAL). Checked: pc/warship/test.sh (78,745 + 25,210 + 65 balance checks), pc/sim/test.sh, in-game pc/tests/warship.sh and aftermath.sh, all pass on a fresh pc/build.sh. Not played by you.

## Patch 18 (warship room mix, part 2)
New `pc/warship/mix_test.c`, `pc/warship/ws_mix_sy.h`; changed `pc/warship/ws_deck.h`, `pc/warship/ws_deck.c`, `pc/warship/test.sh`, `pc/shipyard/sy_design.h` (comment), `pc/ROADMAP_TODO.md`, `ROADMAP_NEXT.md`, `pc/README.md`. The interior builder has a ROOM MIX option: `wd_make_mix(..., const WdMix *mix)`. A mix is the shipyard's `SyRooms` (reactor, hangar, quarters, bridge, turret, shieldgen, workshop, mess; bunks, crates, guns, fighters) and replaces the builder's random fill: the rooms the design has, a hangar per 10 fighters and a quarters per 12 bunks (more if the floor is small), then exactly that many bunks, crates, guns and fighters in the rooms that hold them. The craft's own target rooms are never touched. `wd_make` is `wd_make_mix` with no mix: checked byte for byte on 96 decks, and the old interior numbers are unchanged (670 decks, 131 props a deck, 78,745 checks, 0 failed). Checked: all 10 shipyard presets (180 decks: 708 of 720 prop counts exactly as asked, the rest on decks the craft's own 8 subsystem rooms fill, never over), 300 random mixes (all reachable, deterministic, never over the ask), carrier = 2 hangars, 16 fighters, 2 quarters, 24 bunks. NOT done: the game does not call it yet (no rule for which design a boarded ship uses), and the full `pc/build.sh` was not run (only `ws_deck.c` and the plain C tests were built).
