# Patch roadmap (3 Nov 2026) - read this first when continuing

## Done in this patch
- Buildings (planet.inc classic renderer + pc/pc_pnative.inc window renderer): roof ledge, slab lines, floors of real height
  (32 units, shrink with distance), punched windows (tints 0,2) or glass ribbons (tints 1,3), some windows lit, plain wall when far.
  The old checkerboard wall colour (ix&1) and half-the-buildings-have-no-windows are gone.
- Compiles cleanly (pc/build.sh, ~100 s). Checked in the classic viewer (headless city screenshot): nearby buildings show floors and
  windows, far ones stay plain blocks (detail cutoff ~8 cells). The native window renderer was NOT looked at, NOT run on Windows.

## NOT done
1. Tune buildings further: look at the native (window) renderer, tune floor height (32), window ranges, glass tint choice.
   Camera is 4 cells up and buildings only ~0.7-2.2 cells tall: consider taller towers in town.inc city_stamp (hfine is capped at 255).
2. DUNGEON LAG - not diagnosed. Video: blank stall of a few seconds right after entering a dungeon.
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
