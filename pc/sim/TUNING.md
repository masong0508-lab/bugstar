# Side game tuning notes (patch 22): the "known weak spots" list from patches 14 and 15

Numbers below were measured with plain C against `pc/sim/sim_core.c` (12 bug town, 12 days, 40 seeds) and by reading the shop code. NOT played by you.

## Changed

**Town worry (was far too strong).** A colony warning (raid coming, hunger) added `+2 x worry` stress to every bug every 40 ticks. Over 12 days:

| | breakdown events per town (max) | average stress | bugs broken down on day 12 |
|---|---|---|---|
| no worry | 0 | 34.6 | 0 |
| worry 1, before | 68 (118) | 130 | 0.7 |
| worry 2, before | 354 (402) | 177 | 0.6 |
| worry 1, now | 4.8 (13) | 63 | 0.3 |
| worry 2, now | 17 (39) | 98 | 1.5 |

Now: worry 2 adds +1 every step, worry 1 adds +1 every other step (`sim_core.c`, `folk_tick`). A long warning still weighs on the town and can break a few bugs, but not the whole town over and over (and the log is not flooded). Worlds with no worry are byte for byte the same (`sim.golden` unchanged). `ties_test.c` now checks these numbers.

**Friend gift GOLD.** The gift is `units x (20 + min(level, 1000))`, units = 1, 4, 9, 16 by friendship step (+25 percent for sociable bugs). Real prices: supplies 10 to 120G, a gear upgrade `25 + 12 x lvl` (+200 for a unique), an ore tool upgrade 100 / 350 / 1200G. At level 1000 the top gift was 16,000 to 20,000G: a pile of upgrades. Now the per-unit factor stops at level 100: at most 16 x 120 = 1,920G (2,400 sociable), about one big purchase. (`sim_glue.inc`.)

**Friend gift ORE.** Checked: `ore_add(0, n)` is AMBER, tier 1, "what every world already grows", 4G each. So index 0 is right, but 1 to 4 amber was a pinch. A miner's gift is now 4 to 16 amber (`sim_gift`, `sim_core.c`). `ties_test.c` checks it.

## Checked, left alone on purpose

- **Gift HEAL (full heal).** A gift is logged once per friendship step (25, 50, 75, 100), so at most 4 per friend, ever; friendship never goes down (`friend_up` is only called with positive numbers), so it cannot be re-farmed by letting it fall and rise. No cooldown needed.
- **Colony call cooldown is runtime only.** A reload resets it, but what a call pays (raid cut to 70 percent, food, wood, a settler) IS saved, and a second call only shows up if the trouble is still there, which means clearing another dungeon floor. Accepted: no save format change.

## Not touched (still open in `pc/ROADMAP_TODO.md`)

- `col_call` priority stuck-ness (FAMINE on a floor that cannot spawn a nest), real dungeon generation work, the colony ASK side, and everything in sections C to E.

## Test status

- `bash pc/sim/test.sh`: all suites 0 failed (`ties_test` 47 checks). Headless `PC_SDL=0 pc/build.sh`: built. In-game `tests/sim_collect.sh`, `sim_town.sh`, `sim_unlock.sh`, `sim_lab.sh`: OK.
- **Two tests fail, and they fail the same way on the repo WITHOUT this patch, so they are not from it:** `tests/sim_off.sh` (its `sim_off.golden` pictures are from before the first side-game commit and are stale; the half that matters still holds, checked by hand: the `.sav` is identical with the side game ON and OFF, and no `.sim` file is written when OFF) and `tests/sim_friend.sh` (the dump file only has the first dump, so it finds no friendship numbers). Worth a stale-golden fix commit later.
