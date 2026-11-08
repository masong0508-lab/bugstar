# BUGSTAR: unfinished work, written after patch 15 (4 Nov 2026). READ THIS FIRST when continuing.

Rule for myself: before starting new features, work this list top to bottom. When an item is done, change `[ ]` to `[x]` and add the patch number.
Patches so far: 14 = life sim TIES (gifts, requests, colony ties), 15 = DUNGEON CALLS (colony trouble sets floor goals). 16 = this file, 17 = WARSHIP BALANCE (part 1 of finishing the warship work), 18 = ROOM MIX (part 2: a shipyard design's rooms and props can drive a warship deck), 19 = ROOM MIX in the game (FOES option).

## A. Never played (all code below compiles and its plain-C tests pass, but nobody has seen it in the game)
- [ ] Patch 14 in game: talk to a bug that asks for help (meal / listen / peace), confirm prompt, friendship rise, gifts reaching the bag, PEOPLE "ASKS" flag, COLONY MAP footer with friend ties.
- [ ] Patch 15 in game: a dungeon floor with each call (RAID -> elite hunt, FAMINE -> nest, SUPPLY -> speed run, LOST -> caged settler). Check the gold "COLONY:" line, the payout message, and the 2 day rest.
- [ ] Headless proof for both: a script that forces a colony call (raid coming, hunger) and walks a dungeon floor; add it to pc/tests and to the sim test list.

## B. Known guesses and weak spots in patches 14 and 15
- [ ] Friend gift GOLD uses `units x (20 + min(level,1000))`; check against real shop prices and tune.
- [ ] Friend gift ORE calls `ore_add(0, n)`: confirm index 0 is plain ore, or pick the planet's own common ore.
- [ ] Gift HEAL for racers and guards is a full heal: maybe too strong, give it a cooldown.
- [ ] Colony call cooldown (`call_cool`) is runtime only: a reload resets it and a call can be collected twice. Either save it (needs a colony list version bump) or accept and note it.
- [ ] Town worry (`+2 x worry` stress per tick) may push whole towns into breakdowns during a long raid warning: simulate 12 days of worry on a 12 bug town and check.
- [ ] `col_call` priority is RAID, FAMINE, SUPPLY, LOST. Check a colony is never stuck with FAMINE on a floor that cannot spawn a nest (first garden floor, boss floors, outposts skip events: the call then simply waits).
- [ ] Dungeon calls only change the floor event. Still to do: real dungeon generation work (room shapes, hazards, new monsters, a mine variety, secret rooms) so floors differ in more than the event.
- [ ] Ties to the action game ASK side: the colony can ask you for things (missing ore, a rare item) with an arrow, as DEPTH_PLAN 1.4 lists (famine hunt, missing material, festival item). Only dungeon calls exist so far.

## C. Warships and planetary threats (see pc/README.md, ROADMAP_NEXT.md section 2)
Built and tested: sizes, reward formula, fights, interiors, missions, encounters, buster countdown, orbital strikes, troop waves, hellscape, sealed entrance, portal escape, floor invasions, boarders. `pc/warship/test.sh` 78,745 checks 0 failed; missions 25,210 checks 0 failed; `pc/tests/aftermath.sh` passes.
- [x] BALANCE PASS (patch 17): `bash pc/warship/bal.sh` (plain C, `pc/warship/BALANCE.md` has the before and after tables). Tuned: per-minute chance cap, 25 minute quiet time, buster share 1 in 6, buster window at least 5 minutes, hellscape ship/pod roll by armour (it was upside down), DEFEND raid 6 + 4 x class and capped at 6/5 of the colony defence on NORMAL. Left alone on purpose: the 1/5 death loss, the 10 minute board window. NOT played: the three player stages in bal.c are my guesses. A LATE player cannot stop a buster in its window (0 percent): check that in play.
- [ ] Re-run the skipped suites. DONE in patch 17 (all pass): in-game `pc/tests/warship.sh`, `pc/tests/aftermath.sh`, `pc/warship/test.sh`, `pc/sim/test.sh`. STILL TO RUN: coil.sh, giant.sh, giant/test.sh. Fix stale goldens once, in one commit.
- [x] Shipyard rooms into warship interiors (patch 18, part 2): `wd_make_mix` (`pc/warship/ws_deck.h`, `WdMix`) builds a deck from a design's rooms and props (bunks, crates, guns, fighters); `wd_make` is the same call with no mix and its decks are byte for byte what they were. Glue: `pc/warship/ws_mix_sy.h` (`wd_mix_from_sy`). Tests: `pc/warship/mix_test.c` (in `pc/warship/test.sh`). A craft with many subsystem rooms of its own (the 8 rooms of a single-deck NORMAL craft) leaves less floor: the mix then gets what fits and never more than it asked.
- [x] Call `wd_make_mix` from the game (patch 19, `pcw_mix` in `pc/pc_warship.inc`): when the ship editor's FOES option is on, a boarded craft gets a `sy_foe` design (team 1, level from its class) made from its own seed, and that design's rooms and props build the deck. FOES off: decks exactly as before. `BUGSTAR_WS_MIX=1/0` forces it on/off (tests). In-game check in `pc/tests/warship.sh`. NOT seen in play by you.
- [x] (patch 45: done, the scanner rows ALLY and YOUR SHIP open a peaceful deck; `pc/tests/walk.sh`) Boarding an ALLIED craft should use a team 0 design, and your own ship's design for your own hangar deck; today every boarded craft is treated as an enemy design. Foe designs still do not change stats or fight rules.
- [ ] Foe designs that change stats and fight rules; designs for allied and enemy ships (README says not done).
- [x] (patch 45: played by script: burnt colony map art, `col_burn`, hell terrain in place, DEFEND raid with a colony; `pc/tests/mission.sh`) Extreme end: play a real dead-world path once (colony ruins, burnt planet, resettle elsewhere). Story worlds must never be destroyed for good.
- [ ] Death penalty check: the save stays byte identical apart from the stated penalty.

## D. Side game (pc/DEPTH_PLAN.md), phases 6 and up
- [x] Workshops and the mine with z-levels (patch 44, `pc/sim/mine.c`; see README, 'The colony's WORKSHOPS and MINE'). Still to do: a colony call for the mine (a cave-in rescue or a missing-material run), mine events in the dungeon generator, the nest under the mine. NOT played by you.
- [ ] WorldBox-style races, tribes and powers using the 12,672 creatures and 48 god powers.
- [ ] Replay layer: scenarios and a sandbox switch for the mine are in (patch 44); kits, policies, challenge modes and colony plus are not.
- [ ] Permanent ruins shown in the planet file.
- [ ] Pull-backs not built yet: famine hunt on the surface, missing material mine run, festival rare item.

## E. Platform
- [ ] Run the Windows exe on real Windows (paths, audio, VirtualAlloc memory map, gamepad). Update README (it still says 32-bit/untested). Add a CI build for Linux and Windows.
- [ ] GBA: re-run coil fuzz and kart fuzz on the final ROM; play it start to finish once.
- [ ] GBC: PARITY.md still says karts are not ported (they are).

## F. How to continue
1. `git pull`, read this file, then ROADMAP_PATCH.md (patch table) and ROADMAP_NEXT.md.
2. Run `bash pc/sim/test.sh` and `bash pc/warship/test.sh` first; they must be 0 failed.
3. `pc/build.sh` must print `built pc/bugstar_pc`: every `H` hook fails loudly when its anchor moves.
4. New patch = next number after 19, same Termux command as in ROADMAP_PATCH.md.
5. `bash pc/warship/test.sh` now also runs the balance report (quick); when you change a PCM_* number in `pc/pc_missions.inc`, change it in `pc/warship/bal_rules.h` too.

- [x] Patch 45 (played by scripts, not by a person): all 7 warship mission kinds accepted, flown, finished and paid with the pay checked by `pc/warship/ms_check.c`, saved and reloaded (`pc/tests/mission.sh`); ally and own-ship hangar decks (`pc/tests/walk.sh`); second stair and dig on floors deeper than 1; burnt colony art; hell terrain in the same visit; mine and workshops run for 1,500 days x 30 towns (`pc/sim/mine_bal.c`, no softlock, no value changed, see `pc/sim/TUNING.md`); BUGSTARILLION v3 long walk with the real index (`pc/tests/giant_v3deep.txt`, needs BUGSTAR_GIANT_DIR). Still open: nothing played by a human; the dig and hell terrain were checked in code and by the aftermath script only.

## Patch 47: the life sim found step by step (progressive disclosure)
- [x] 47: unlock registry (`pc/sim/unlock.h`), tips, TIPS page, `!` markers, LIFE SIM options, hidden rows everywhere (see README "The life sim, found step by step").
- [ ] Not played by a person yet: only scripts and frames. Check the pace by hand (does the 10-minute rule feel right? is 20 seconds between cards enough?) and read the tip texts aloud for a dyslexic reader.
- [ ] Skills (`SimFolk.skill`) and the weather forecast have no feature of their own yet: add `UF_SKILL` when they show in a menu.
