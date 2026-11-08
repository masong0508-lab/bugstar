#!/bin/bash
# GBA SCREEN TRANSITIONS (needs mGBA + the arm toolchain; not part of the PC run): beam-level checks, frame by frame, of launch, landing, galaxy voyage,
# dungeon/forge enter+exit, kart menu, lair. Reports torn frames, control-register changes while the beam draws, palette writes while drawing, blank runs.
# Expect 0 torn / 0 ctrl / 0 palette for launch, land, voyage, lair. kartq: ctrl/torn counts are by design (the race ground is drawn by per-line DMA).
# dungeon/forge: the torn frames after the faint (hearts poked to 0) are the black page clear in planet_enter (screen is black by BLDY then; the checker cannot see BLDY).
# The scripts' paths are the author's tools dir: edit HARNESS= below.
cd "$(dirname "$0")/gba_trans" && HARNESS=${HARNESS:-/home/claude/tools/harness} ./tr_all.sh "${1:-../../../bugstar.gba}"
