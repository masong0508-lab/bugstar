# BUGSTAR roadmap: mining and worlds (status, 29 Oct 2026)

Replaces the PDF of 28 Oct. Hard limits are unchanged: IWRAM and EWRAM are full (new code and tables go in ROM), SRAM blocks must not move, and every new block needs a version byte and an old-save fallback.

## Done
- **Step 0, stabilize:** stack fix, confirmed in play.
- **Step 1, foundations:** 12-ore catalog, ore save block at slot + 0xB60, old-save fallback.
- **Step 2, Phase 1:** veins in crystal rocks, 4 tool tiers, depletion, ore bag, MINING menu on the ship.
- **Step 3, Phase 4 basic:** smelter, bars, parts, tools. 56 recipes in ROM, 34 stock items (12 in the main block, 22 in the extra block at 0x830), hull plates and ship parts, mining log, ore grades (drift, quality).
- **Depth scaling:** deeper worlds give harder veins (up to +3 bites) and richer ore, with no new saved data.
- **Space speed:** far ships skip the orientation maths and are drawn every other frame.
- **Step 4, Phase 2 (first step):** landable belt rocks, five kinds (rocky, metallic, icy, crystal, hollow). Slot i of a star's belt is always the same rock. Nothing is saved for a rock. Icy rocks melt (about 2.5 minutes), crystal rocks crack, metallic rocks need a better tool. Voidsteel (tier 6) can now be found.
- **Dungeon walls:** the painted door sprites are removed (the keycard door stays).

## Left, in the order I would take them
1. **Hollow-rock mine (Phase 3 start).** A mine entrance on hollow rocks, with ore veins in the walls bitten out in place. Built from the dungeon code, no new RAM. Medium risk: needs a quick look in play.
2. **Link check with the real runtime library.** CI uses devkitARM; a local Ubuntu gcc 13.2 build reads 2696 bytes of stack headroom (want over 1500). CI prints its own number; compare the two after each patch.
3. **Ore grade stacks, gem sockets and trophy slot.** These need RAM, so they wait for item 4 or 5.
4. **Overlap the dungeon and planet buffers** (frees about 38 KB of EWRAM). Medium risk: it could garble terrain. Test: dungeon, back up, town, karts.
5. **Cut ground resolution to 128** (frees about 96 KB of EWRAM). High risk: land, town and kart race all need a look. Take item 4 before this one.
6. Later phases: tractor beam and belt hazards (Phase 2 rest), mine hazards, shaft boss and Forge ore dial (Phase 3), claims, contracts, megadeposits and prestige (Phase 5).

## Rules that stay
Small steps, one feature per build. Check the stack line stays above 1500. Play a phase before starting the next.
