# Patch roadmap (3 Nov 2026) - read this first when continuing

## Done in this patch
- Buildings (planet.inc classic renderer + pc/pc_pnative.inc window renderer): roof ledge, slab lines, floors of real height
  (32 units, shrink with distance), punched windows (tints 0,2) or glass ribbons (tints 1,3), some windows lit, plain wall when far.
  The old checkerboard wall colour (ix&1) and half-the-buildings-have-no-windows are gone.
- Compiles cleanly (pc/build.sh, ~100 s). NOT looked at yet, NOT run on Windows.

## NOT done
1. LOOK at the buildings: headless city screenshot, tune floor height (32), window ranges, glass tint choice.
   Camera is 4 cells up and buildings only ~0.7-2.2 cells tall: consider taller towers in town.inc city_stamp (hfine is capped at 255).
2. DUNGEON LAG - not diagnosed. Video: blank stall of a few seconds right after entering a dungeon.
   Suspects: (a) pc/pc_dnative.inc native renderer, per-pixel double maths at full window size -> cap internal resolution / skip rows;
   (b) floor generation on entry (make_floor / fm_make in dungeon.inc).
   Measure: PC_TIMING=1 prints ms per frame. Headless entry: BUGSTAR_WS_TEST=dun,0,0 BUGSTAR_AFT_DUN=seal BUGSTAR_AFT_TOPUP=1
   with --script tests/aft_dig.txt (see tests/aftermath.sh).
3. Win64 check: the Windows exe has never been run on real Windows (pc/README.md).
