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

# Patch 41: life sim core v3

Measured with plain C against `pc/sim/sim_core.c` (12 bug towns, 80 seeds, 3 years of 28 days; mood and needs over 30 days, 60 seeds). NOT played by you.

## What was wrong (all measured on the old core)

| | before |
|---|---|
| FUN need | mean 73, below 70 for 59 percent of the time: every bug was always BORED |
| hunger, rest, social | pinned at 250+ about 20 percent of the time |
| the larder | thousands of food (mean 1,150 to 1,370, up to 3,800); no town ever ran out in 240 town-years; the season changed nothing (+422 to +448 over 3 days in every season) |
| jobs | only the forager did anything. Cooks, clerks, stall keepers, miners, racers and guards just raised a `skill` number nothing read |
| forager skill | never grew (the skill rule skipped foragers), yet only forager output read it |
| free time | scheduled, whatever the needs: a lonely bug and a happy one did the same |
| a sick bug at rest 160 | flipped between bed and plaza every 6 to 12 ticks (and logged WAKE/SLEEP each time): about half of all log events were this |
| hungry bug (below 50) | left the table the moment it passed 50, and was back a few ticks later |
| worn out (rest below 20) | the same flip around the line |

## What changed (`sim_core.c` only; no save format change, nothing new in `sim_hash`)

- **Food economy with a year.** Forager yield by season 100 / 120 / 140 / 35 percent (spring, summer, autumn, winter; scorched or frozen ground 60 percent of that), once per 100 ticks of work. The free trickle (`1 + n/4` per 100 ticks) is gone: STALL KEEPERS and CLERKS trade food in, MINERS sell ore for food, and a trader passes by now and then. Spoilage by climate (cold 5, normal 10, hot 15 percent a day), plus anything above the larder's room (`120 + 20 x bugs + 2 x shelter`) rots by a third a day. Meals cost 1 food per 40 ticks of eating (was 100).
- **Jobs matter.** COOK on duty (working or at the table) stretches the meals (up to 50 percent cheaper); GUARD keeps the pests down; RACER puts on an evening show (fun for everybody at leisure); skill (now growing for every job, forager included) counts for up to double. A bug counts as on duty only when it is working (or the cook / racer exceptions) and not sick or broken down.
- **Needs steer free time.** In the schedule's free hours a bug goes to the plaza to chat or to leisure by whichever need is lower (a sociable bug counts loneliness double, the evening schedule leans to the plaza, a crowd draws a crowd). It re-decides once an hour.
- **No more flapping.** Sick bugs stay in bed until rested (200) and get up at 150; worn out bugs sleep until 90; a hungry bug eats until 130. Sick bugs do not go to the plaza.
- **Fun and stress rebalanced.** Fun decays faster awake (1 per 30 ticks), leisure gives 1 per 5, chatting 1 per 16, a tireless bug enjoys work. A need below 70 (was 40) is a stressor; passive recovery only above mood 120 (was 110, and -2 became -1). Illness is 30 per 1000 a day to start (was 15), plus 15 in winter.
- **Cold winters** burn more food (hunger falls 1 per 24 ticks, not 30, when winter is 5 degrees or colder).

## Before and after (12 bug towns)

| | before | after |
|---|---|---|
| larder, spring / summer / autumn / winter (mean) | 1152 / 1322 / 1371 / 1372 | 273 / 408 / 486 / 325 |
| towns that ever ran empty in 3 years | 0 of 80 | 18 of 80 |
| towns that never fell below 100 | 80 of 80 | 47 of 80 |
| most food ever held | 3,804 | 1,471 |
| fun need, mean / below 70 | 73 / 59 percent | 152 / 8.6 percent |
| mood, mean | 170 | 194 |
| days sick (share of bug-days) | 0.7 percent | 3.3 to 5.5 percent |
| task changes per bug per day | 10.0 | 9.2 (the schedule itself is about 9) |
| log events per day | 140 | 113 |
| chats per town per 30 days | 934 | 690 |
| couples per town (30 days) | 3.8 | 2.5 |
| breakdowns, worry 0 / 1 / 2 (12 bugs, 12 days) | 0 / 3.4 / 13.9 | 0 / 1.6 / 5.6 |

Chats and couples went down on purpose: free time is now split between the plaza and leisure by need, where before everybody was at the plaza at the same hours.

## Tests

- `pc/sim/life_test.c` (21 checks, in `test.sh`): flapping (sick, worn out, hungry, 12 towns x 8 bugs x 20 days), free time follows the lower need, autumn beats winter, the larder is bounded and some towns do run out, forager skill grows, cooks / guards / racers / traders each change the outcome, a skilled forager is not worse, determinism, no overflow. **13 of these 21 fail on the old core.**
- `bash pc/sim/test.sh`: every suite 0 failed. `sim.golden` regenerated (behaviour changed on purpose). `ties_test` and `colony_test` needed no change.
- In-game, headless: `sim_town`, `sim_collect`, `sim_unlock`, `sim_lab`, `sim_friend`, `determinism`, `screens` all OK. `sim_off` fails exactly as before the patch (stale golden pictures, identical output with and without this patch).

## Not done (ideas)

- Birth, ageing and leaving (the town size is still fixed unless a god power adds a bug).
- Bugs that choose a different job or move house; goals and memories of events.
- Friendship rivalries are still rare (0.05 rival pairs per town): quarrels need two bugs who dislike each other to chat.

# Patch 42: standing (fame opens the side game, mayor, armada)

NOT played by you. Plain C (`pc/sim/standing.h`, header only) plus glue; PC and Android only.

- Quick menu (A on empty ground) lists only what your fame opened: 0 JOURNAL + MY LEVEL, 50 PEOPLE, 150 COLONY, 350 GOD POWERS, 700 SANDBOX. One line under the list says what is next.
- At 700 fame, on a planet with a town (2+ bugs), the world names you MAYOR: TOWN HALL (taxes from the home town and tribute from settled worlds, a feast for the town).
- At 1200 fame, as mayor: ARMADA. Ships cost gold (dearer as the fleet grows), your level caps the fleet (one per level, 5 at least). 8 + 6 per world ships settle a world (stays yours, pays tribute, +fame); after 12 worlds you cross to the next galaxy (costs triple, ships x4 each time).
- Old saves: menus already earned are adopted silently. New save chunk STND (80 bytes); old files have none.
- PC LAB > SIDE GAME keeps the first six menus open (cheat page). Test hook: BUGSTAR_SET_FAME=n.
- Tests: standing_test.c (13,570 checks) in test.sh; whole test.sh passes. In-game: sim_town, sim_lab, sim_friend, sim_unlock, determinism OK. sim_collect still FAILS with fame 699 (not diagnosed). sim_off golden was already stale.

# Patch 44: workshops and mine (phase 6)

NOT played by you. Plain C (`pc/sim/mine.c`) plus glue; the numbers below are guesses measured only with `mine_test.c` and one in-game run.

- Seams: `150 + 40 x level + 0..99` ore a level (an eight-miner colony used to dry a level in a few days at 30 + 8 x level; now about 10 to 25 days). Dry levels find new veins (a quarter of a seam) every 120 days, so a mine never ends.
- Yield per miner a day: `1 + level/6 + tools/4 + 0..1`; ore into the colony counts `1 + level/8` per unit. Deep ore is a few times the surface miner's 2 a day, not ten times.
- Creak odds (per mille a day) `4 + 3 x level - 2 x braces - tools/2 + miners/4`; the colony works levels at 24 or less (two braces: down to level 7 on NORMAL, deeper on CALM). In 108,000 mine-days of 8 miners that was one cave-in per 144 days, 172 deaths on NORMAL and HARSH, none on CALM. Brace 0 against brace 9 at level 8: 488 against 28 cave-ins in 24,000 mine-days.
- The colony never saves wood below 12 for the workshops, braces or digging (houses and fields still get built).
- Check against play: gold from bars and tools (9G and 30G each) next to the old surface ore (3G); gems 60G. `col_collect` still sells all colony ore at 3G, so a deep mine mostly feeds the smelter's 1 to 3 units a day.
- Found while testing: `sim_rows` kept each row in 56 letters but a locked rung's text is 67 (stack overflow when a colony had locked rungs): the buffers are 96 now. The side file's chunk buffer was 4,499 bytes, so a colony list of more than 27 colonies was silently not saved: 8,192 now.
