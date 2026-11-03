# Patch roadmap (3 Nov 2026) - read this first when continuing

## Done in this patch
- Buildings (planet.inc classic renderer + pc/pc_pnative.inc window renderer): roof ledge, slab lines, floors of real height
  (32 units, shrink with distance), punched windows (tints 0,2) or glass ribbons (tints 1,3), some windows lit, plain wall when far.
  The old checkerboard wall colour (ix&1) and half-the-buildings-have-no-windows are gone.
- Compiles cleanly (pc/build.sh, ~100 s). Checked in the classic viewer (headless city screenshot): nearby buildings show floors and
  windows, far ones stay plain blocks (detail cutoff ~8 cells). The native window renderer was NOT looked at, NOT run on Windows.

## Patch 03 (dungeon lag) - what was found and done
- Could NOT reproduce a multi-second stall on entering a dungeon. Headless (1 core), no frame over 40 ms in game logic: floor generation is not the culprit.
- Native dungeon renderer (pc_dnative.inc) costs ~9 ms/frame at 960x640 on one core (~30 ms estimated at 1920x1080 on one core); threads help on multi-core.
- Added: adaptive CHEAP MODE (nearest texel instead of the 4-texel blend on floors/ceilings). Switches on when the 30-frame average passes 12 ms,
  off again under 3.5 ms. Measured saving only ~17% (9.0 -> 7.6 ms). Force it with env PC_DN_LOW=1 (off with 0). Real Windows gain: unmeasured.
- Still open: the real cause of the Windows blank stall. Ideas: win64 memory mapping on first dungeon frame, compositor path (pc_hw.c ~line 540),
  a stronger cheap level (render fewer rows / lower internal size), a per-frame timing log written to a file (the Windows exe has no console).

## NOT done
1. Tune buildings further: look at the native (window) renderer, tune floor height (32), window ranges, glass tint choice.
   Camera is 4 cells up and buildings only ~0.7-2.2 cells tall: consider taller towers in town.inc city_stamp (hfine is capped at 255).
2. DUNGEON LAG - partly addressed by patch 03 (see above), cause of the stall still unknown. Video: blank stall of a few seconds right after entering a dungeon.
   Suspects: (a) pc/pc_dnative.inc native renderer, per-pixel double maths at full window size -> cap internal resolution / skip rows;
   (b) floor generation on entry (make_floor / fm_make in dungeon.inc).
   Measure: PC_TIMING=1 prints ms per frame. Headless entry: BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=seal BUGSTAR_AFT_TOPUP=1
   with --script tests/aft_dig.txt (see tests/aftermath.sh).
3. Win64 check: the Windows exe has never been run on real Windows (pc/README.md).

## Patch naming and the command (use exactly this)
Every patch zip gets its own name, number and temp folder, so Android never makes "name (1).zip" and an old zip is never applied by mistake.
- 02 = buildings:   zip `bugstar-patch-02-buildings.zip`   temp `~/tmp_patch_02`   commit `patch 02: buildings with floors, windows, roof ledges`
- 03 = dungeon lag: zip `bugstar-patch-03-dungeon.zip`     temp `~/tmp_patch_03`   commit `patch 03: dungeon lag fix`   (not made yet)
- (01 was the earlier win64 address-range patch)
Each zip holds: `apply.sh`, `bugstar.diff`, `ROADMAP_PATCH.md`. apply.sh runs from the repo root, stops with exit 1 on failure (so the commit and push do not run).

Termux command for patch 02 (clone first if needed: `gh repo clone masong0508-lab/bugstar` or `git clone git@github.com:masong0508-lab/bugstar.git`):

    cd ~/bugstar && git pull --ff-only && rm -rf ~/tmp_patch_02 && mkdir -p ~/tmp_patch_02 && unzip -o ~/storage/downloads/bugstar-patch-02-buildings.zip -d ~/tmp_patch_02 && sh ~/tmp_patch_02/apply.sh && rm -rf ~/tmp_patch_02 && git add -A && git commit -m "patch 02: buildings with floors, windows, roof ledges" && git push

For patch 03 swap the two 02 names and the commit message for the 03 ones above.

## Patch 04 - frame log (diagnostic for the Windows dungeon stall)
- Create an EMPTY file named `slowlog.on` in the same folder as BUGSTAR.exe, play, enter a dungeon, quit. `slowlog.txt` appears there:
  a line for every frame over 33 ms (with the dungeon renderer's share in ms) and an average line every 300 frames. Delete `slowlog.on` to turn it off.
- NEXT after you send slowlog.txt: if the slow frames show "dungeon renderer" high -> add a stronger cheap level (lower internal size / fewer rows);
  if they are slow with the renderer low -> the cause is elsewhere (compositor pc_hw.c ~line 540, win64 memory mapping, or the game's own code).
- Patch 04 names: zip `bugstar-patch-04-framelog.zip`, temp `~/tmp_patch_04`, commit `patch 04: frame log`.

## Patch 05 - buildings vary with the city's tech level (town.inc)
- city_tech() = 0 huts .. 5 megacity, from the city size (0-3) + the world's economy (farming 0, mining +1, hive +1, tech +2); a city planet (ecumenopolis) is always 5.
- Higher tech: taller blocks, bigger footprints (2-3 texels at tech 0, up to 6 at tech 5), more towers, taller towers (stepped crown on wide ones), more glass-ribbon blocks.
  Tech 0 has no glass at all. The city's ground level is capped at 110 (was 150) so towers have room: the height map stops at 255.
- Looked at 3 cities in the classic viewer: small low-tech town vs dense tall city vs a night city with lit windows. Native window renderer not looked at.
- This changes the GBA game's generated cities too (town.inc is shared): same seed, different buildings. pc/tests/seeds.golden may need regenerating if it hashes the land.
- Names: zip `bugstar-patch-05-towers.zip`, temp `~/tmp_patch_05`, commit `patch 05: building size follows tech level`.
- Ideas left: door band at street level, darker lower floors, per-face shading, tune the tech formula (add the PC meaning layer if it has a tech channel).
