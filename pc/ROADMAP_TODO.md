# BUGSTAR: unfinished work, written after patch 15 (4 Nov 2026). READ THIS FIRST when continuing.

Rule for myself: before starting new features, work this list top to bottom. When an item is done, change `[ ]` to `[x]` and add the patch number.
Patches so far: 14 = life sim TIES (gifts, requests, colony ties), 15 = DUNGEON CALLS (colony trouble sets floor goals). 16 = this file.

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
- [ ] BALANCE PASS (the one real gap): a bot-driven report per encounter setting (OFF/RARE/NORMAL/COMMON) and side game difficulty (CALM/NORMAL/HARSH): how often threats fire per hour of play, buster survival rate prepared vs unprepared, gold lost per lost buster. Tune the 1/5 loss, the 10 minute board window, the buster timer.
- [ ] Re-run the in-game `pc/tests/warship.sh` (it was skipped near the end) and the other skipped suites: coil.sh, giant.sh, giant/test.sh, older sim tests. Fix stale goldens once, in one commit.
- [ ] Wire shipyard rooms (`sy_rooms`) into warship interiors (`wd_make`).
- [ ] Foe designs that change stats and fight rules; designs for allied and enemy ships (README says not done).
- [ ] Extreme end: play a real dead-world path once (colony ruins, burnt planet, resettle elsewhere). Story worlds must never be destroyed for good.
- [ ] Death penalty check: the save stays byte identical apart from the stated penalty.

## D. Side game (pc/DEPTH_PLAN.md), phases 6 and up
- [ ] Workshops and the mine with z-levels (the nest under the mine is half done through the FAMINE call).
- [ ] WorldBox-style races, tribes and powers using the 12,672 creatures and 48 god powers.
- [ ] Replay layer.
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
4. New patch = next number after 16, same Termux command as in ROADMAP_PATCH.md.
