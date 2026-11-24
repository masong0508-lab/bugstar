# BUGSTAR: what to do next (written 1 Nov 2026)

This is the working roadmap after the big 1 Nov 2026 push. It covers all three versions: GBA (the original), GBC (the port) and PC (native, with the side game).
The older `ROADMAP.md` is the mining roadmap and is still valid for the GBA. Read `pc/DEPTH_PLAN.md` for the side game and `pc/NATIVE_PLAN.md` for the window.

## Mining extraction and shaft transition reliability — 24 November 2026

- Surface and dungeon ore extraction is all-or-nothing: the full deposit
  must fit in its ore stack, including rich/pristine grades. A refused
  final hit retains the deposit and one remaining hit, with a clear sell
  or smelt hint; no mining statistic, quest count or XP is paid for it.
- Base yield is now stable for the same vein and tool bonus. Depth and cell
  are encoded separately in underground vein identity, rather than
  reusing almost identical quantized coordinates on every floor.
- Ordinary resource nodes stay intact at a full cargo hold. Underground
  depth yield bonuses cap at B90, keeping even a pristine mega vein below
  the 9,999 stack limit in very deep endless mines.
- Reset partial digging and gas/cave-in timers on floor generation; a
  new shaft never inherits a previous floor's bites or pending damage.
- Use bounded text receipts; no new permanent memory allocation or
  save-layout change. Expanded production ore regression checks every
  grade and near-cap stack and verifies both actual extraction paths.

## Crowded dungeon safety and precise elite objectives — 24 November 2026

- Reserve the existing 160-object pool in phases: optional crowds and props,
  cages/bats/lamps, campaign bosses/outpost keys, floor events and posted
  quest targets. Excess optional decorations or loot are skipped; essential
  fights and keycards cannot be starved by the earlier random spawn batches.
- Guard gear metadata writes when a spawn is rejected. Previously a full
  pool caused gear fields to be copied onto the last unrelated actor.
- A floor elite now has its own existing champion-byte value, distinctive
  HUD label, and exact kill/revisit counter; random champions do not
  satisfy the objective, including PC colony mine-burrower rescues.
- Production macro/elite tests run under ASan/UBSan. No new actor slots,
  WRAM/EWRAM buffers, assets or save fields; room generation remains
  deterministic within this build. Final emulator FPS is a separate gate.

## Colony dungeon-rescue progression and persistent mission cooldown — 23 November 2026

- **PC/Android dungeon mission:** when the colony mine has an uncleared
  cave-in or injured miners, the active dungeon can offer a fifth COLONY
  objective: FIGHT THE MINE BURROWERS. It reuses the existing two-elite
  dungeon event and pay, then opens the collapsed work shaft, heals injured
  crews, places at least three braces, and adds colony ore, wood and morale.
  A mine emergency ranks below urgent raids/famine but above regular
  supply requests or lost-settler rumors. An active rescue expedition takes
  precedence instead of issuing a duplicate distress call.
- The colony menu advertises the emergency directly. The completion
  notification is drawn after the standard ELITES SLAIN message so players
  can actually read it. No new sprites, framebuffer or per-frame work.
- **Anti-farming fix:** clearing any colony dungeon call grants its
  resources only once during the two-day cooldown. The cooldown is now
  stored in COLN **version 2, 168-byte records** (previously 164 bytes)
  alongside all existing colony data. The loader automatically imports
  164-byte v1 records and older 34/42-byte colony versions. The separate
  MINE v2 136-byte format remains unchanged, as do GBA/GBC cartridge
  saves. Back up PC/Android .sim saves when switching game versions.
- Production C colony and mine tests cover duplicate completions,
  half-completed cooldowns across reload, the real BSIM v1-to-v2
  migration, rescue paths, dry veins, injured crews and full-file capacity.
  A separate source audit guards the game build's dungeon hook timing
  and UI integration. The PC CI suite includes these UBSan regressions.
- Remaining release gates: complete in-game player playthrough, actual
  hardware/controller validation, and performance benchmarking where
  timing changes are claimed. Do not infer these from source-level tests.

## Colony-mine expedition expansion and saved mission state — 22 November 2026

- **Three new PC/Android activities** in the existing mine screen: a one-time
  survey at each work level uncovers new ore and gems; dispatching rescue teams
  frees miners from cave-ins and heals the injured; level-5+ salvage expeditions
  spend bars/food on a three-day run for gems and colony reward gold. Salvage
  runs have a persisted seven-day cooldown; every task consumes supplies
  up front and advances only with real simulated colony days.
- A mission requires living settlers assigned to mine, a real shaft and
  a valid work level. The menu explains what each task costs and shows
  completion time and hints. During away-time, existing colony simulation
  completes them using the same deterministic rules. Ruined colonies pause
  their crew operations until recovery.
- **.sim MINE chunk version 2** adds the surveyed-floor bitmap, expedition
  type/level/timer, completed and rescue counts, discovered pockets and
  salvage rest date: 136 bytes per mine, up from 112. Readers still import
  version-one 112-byte records and keep existing resources and progress.
  The main GBA/GBC cartridge layouts are not changed.
- **Colony simulation speed pass:** cache the mine's working population in a
  single settler scan per simulated day. Mine risk, work-level selection and
  all three production workshops reuse the same tally instead of recomputing
  jobs many times per level/day. Legacy daily outcomes, RNG and resource
  consumption are intended to remain byte-for-byte the same without missions.
- Production mine tests add contracts, timers, resource gates, save migration,
  saturated/corrupt input and deterministic catchup coverage. The expanded
  mine test is included in PC CI with UBSan, alongside the usual native
  rendering, GBA, Windows and Android builds. A manual playthrough and
  actual FPS benchmarks remain release gates; the source optimization is
  not presented as a measured FPS gain.


## Chapter journal on the QUEST page — 22 November 2026

- Press A on START > QUEST to switch between berry/seed field notes and a
  four-line STORY SO FAR recap. The recap uses the same eight-stage story
  gate as Bolt's dialogue, so revisiting an earlier planet does not undo
  the King's defeat or spoil the DAWN before all eight memories.
- The space-flight return after the true ending now acknowledges that
  Dawn is free, instead of incorrectly telling the player to come back
  stronger after winning the final battle.
- Dynamic status identifies the next freed star, the actual next Tech Hive
  and its Overlord floor, and recovered data-core progress, using existing
  bounded string helpers. Read the journal any time, on planets or in a
  dungeon. The ordinary quest's title and progress remain visible.
- Zero persistent save changes, framebuffers, image assets, actor arrays,
  heap allocations or per-frame rendering costs. The new ROM-only story
  text is drawn only when the pause page is open and selected.
- `tests/story_journal_runtime.py` exercises production stage classification,
  story lines and dynamic status under sanitizers across all campaign
  combinations. A full playthrough and a native-resolution menu capture
  remain necessary before considering the narrative visually complete.

## Reactive station characters and truthful cantina rumours — 22 November 2026

- Vesper, the moth pilot, and Nib, the runaway ant guard, now tell a
  coherent eight-stage story: imprisoned bug transports, freeing story
  worlds, the Anthill, the King's surviving machine, five Tech Hives,
  Bolt's missing memories, the final battle, and the Dawn's return.
  Their lines are tied to the *same real campaign phase* as Bolt, so
  revisiting old stars does not resurrect the King in dialogue.
- Nib still names an actual reachable outpost when the player is
  searching for missing memory cores. Both the dynamic location and
  fixed dialogue are copied to bounded buffers before display.
- Hum's trading information now matches the static economy multipliers
  in `space.inc` and no longer promises a hold upgrade will pay for
  itself in exactly two trips.
- Cantina chatter no longer says jobs pay late, claims full holds pay
  nothing, or reveals the secret Tech Hive structure prematurely. The
  actionable second gossip line follows the actual quest phase.
- The new production-source regression checks phase-gated dialogue for
  spoilers, matches live economy data, verifies story hints and message
  lengths, and exercises bounded dynamic hints under sanitizers. Zero new
  saves, sprites, heap buffers, or per-frame rendering allocations.

## PC/Android life-sim reward reliability — 22 November 2026

- **Friendship miner rewards go to the actual mining bag:** iron (`ore_n[4]`) is
  visible and usable for tool upgrades and smelting. Previously the side game
  awarded amber through `ore_add(0,n)`, which lives outside the ore-bag list.
- **Never silently eat the reward when the bag is full:** miner gifts and mine
  pickups retain as much ore as fits; any overflow pays the actual ore price
  in gold. A partial collection reports both the ore and fallback credits.
- **Claim all new friend gifts once:** process the actual new event-log interval,
  capped by the event ring. The former one-gift `break` could discard several
  gifts if a catch-up update produced them together. Multiple arrivals receive
  a journal notification, without spamming cards for every gift.
- Bound collected-mine feedback and its 40-byte menu destination, which
  previously appended variable numbers without checking capacity.
- `tests/life_sim_rewards_runtime.py` executes actual C event, ore and mine
  logic under ASan/UBSan for small/full bags, long event backlogs, journal
  idempotence, and collection-text canaries. Validate PC, Windows and Android
  CI plus a UI playthrough before tagging a release. No new saves or gameplay
  allocations; the GBA and GBC lack this optional side-game module.

## Story coherence and true-ending recovery — 22 November 2026

- Bolt's advice follows the actual campaign phase, not whichever early
  planet the player has revisited. Before the King he explains the next
  dungeon/star; after the King he warns of the third-space ambush; then
  he names the next of five hives and its Overlord floor, the missing
  memory cores, and finally the return trip to the beast.
- The Anthill ambient remark no longer reveals Bolt's identity early.
  The stolen-memory scene now explains why the data cores were placed
  in drone outposts. The help page puts the hive shield before the
  final strike and the boss quest hint names its assigned floor.
- The one-time DAWN ending is no longer locked out by an earlier victory
  without all eight cores. Its seen flag is stored in a spare bit of
  SaveB.stage, preserving the entire SaveB/SRAM layout and checksum.
  The original Bolt finale card is used at the correct moment.
- The third-beast opening now explains the hive healing floor instead of
  suggesting that all damage persists unconditionally.
- `tests/story_progression_runtime.py` checks the actual route and
  storage helpers, branching gates, spoiler timing, and ending invocation
  under sanitizers. Existing platform CI remains a release gate.

## Salvage gameplay and space rendering fast path — 22 November 2026

- **New in-space salvage content:** destroying hostile pirate craft and mining
  asteroid belts sometimes releases green fuel cells or cyan field repair kits
  through the same four existing collectible slots. Fly through the pickup or
  hold R for the tractor beam. Fuel cells replenish up to 20 fuel, and repair
  kits patch up to 18 hull or 20 shield when the hull is already intact.
  Collected salvage has distinct 3D and radar colours. Normal cargo remains
  collectible, including the ship's hold-capacity bonus. Full cargo/fuel/hull
  does not swallow a one-shot pickup: instead it awards 10/15 gold with a
  truthful message. Spawns use existing kill/mining events, so this is
  earned gameplay content rather than repeated launch farming.
- **Handheld camera speed:** nearby projected space objects on ARM7TDMI use
  mathematically bounded signed 32-bit dot products instead of multiple
  signed 64-bit products, with exact 64-bit fallback outside a ±20,000-unit
  camera cube. The cube is safe for *any* three signed 16-bit basis entries:
  3 × 20,000 × 32,768 = 1,966,080,000, less than signed 32-bit maximum.
  PC and ARM64 keep their existing camera math. No new state or memory.
- **Shared 3D planet shading:** move the invariant projected scanline-scale
  division out of the scanline loop in `planet_fill`. Pixel computations
  remain unchanged in every platform. This avoids source-level repeated
  integer divides per frame without allocating any new tables.
- `tests/space_content_speed_runtime.py` compiles actual production camera,
  canister and planet-shader helpers. Validate fuel, repairs, holding-capacity
  bonuses and idempotent pickups, compare ARM/PC cameras against an
  independent 64-bit oracle and compare the optimized shader to the exact
  previous source across screen-clipping/radius/LOD cases. CI needs to pass
  and actual handheld FPS still requires hardware benchmarking.

## Planet rewards and resident census — 22 November 2026

- Give a `TALK TO LOCALS` bounty credit only once per distinct resident.
  Repeatedly pressing A on one local no longer completes the whole census;
  speaking while the local's gift cooldown is active still counts. Reset the
  visit bits only when a new census is issued. Reuse the high bit of the
  existing local appearance byte, preserving the original sprite choice in
  its low bit; no extra actor state, save fields or framebuffers.
- Compute dialogue indices with unsigned modulo, so world seeds with their
  high bit set cannot select a negative index into NPC dialogue tables.
- If the item bag is full, compensate for lost one-time local gifts,
  fossil shells and plant berries/seeds with a clear small gold payout.
  Messages always distinguish the real item from the fallback reward.
- `tests/local_rewards_runtime.py` compiles the genuine C census and reward
  helper logic under ASan/UBSan, runs 200,000 generated world-seed/index
  cases, verifies different residents, reissued census jobs, appearance
  identity, and full/available inventory outcomes, and audits actual NPC
  and planet pickup call sites. Validate all platform CI before shipping.

## UI safety and planet feedback — 22 November 2026

- Bound all displayed character-creator hints, hangar rank messages, local NPC
  gifts and taglines, and the planet arrival card fields. Prior compiler builds
  identified overflowing literal and dynamic messages; the new calls never
  write beyond their destination arrays and preserve readable prompts.
- Restore the world-tier and hell-tier suffixes hidden by an accidental
  `#endif` trailing expression. Reserve their display space even for long
  generated world names. Correct moon bob timing to divide by
  `(orbital_period >> 5) + 1` instead of the unintended `orbital_period >> 6`.
- Run `tests/ui_text_bounds_runtime.py` in PC headless CI. It extracts the
  production C bounded-string functions and actual planet HUD, then checks
  edge capacities, integer formatting, long names, tier priorities, TEST_HASH,
  and on-screen labels under ASan/UBSan. It also audits the real NPC, hangar,
  arrival and creator call sites. No new framebuffer, fixed array or save field.
  Platform build and emulator checks are required before release packaging.

## Space flight and combat continuation — 21 November 2026

- **Steering that settles (GBA/PC/Android):** integer easing completes its final
  step, so releasing a turn stops yaw/pitch rotation and levels the bank. Angle updates
  round symmetrically in both directions. Ship-specific inertia is retained.
- **Complete braking and asteroid matching:** R stops residual drift. Beside a
  fast belt rock it reaches the rock's velocity instead of stalling just outside
  the landing threshold; atmospheric nose alignment no longer undoes the match.
  Ordinary unbraked coasting and the existing speed limits are retained.
- **Thrust for every ship preset:** fractional engine force is preserved using
  the existing logic clock, with equal impulse in opposite directions. Previously
  weak engines could lose all positive-axis thrust while negative-axis thrust
  rounded up. This adds no persistent accumulator or save fields.
- **Safe assembly of complex ships (GBA/PC/Android):** failed vertex additions
  return an invalid index instead of reusing the last vertex; edges and faces
  reject invalid references before storing them. Rings and boxes require room
  for their full vertex group. This fixes an actual out-of-bounds model read
  found by ASan with shipyard preset 9, using the existing 128-vertex buffers.
  Models that fit retain their original allocation order and geometry.
- **Readable incoming fire (PC/Android):** NATIVE and ULTRA enemy beams meet the
  player's actual world position in chase view. They previously aimed at the
  cockpit's bottom-edge position, visibly below the ship. Cockpit and CLASSIC
  beam placement remain unchanged. No new framebuffer or actor allocation.
- **Consistent native space frames (PC/Android):** publish a completed space
  page before its existing display wait, keeping its bitmap, background,
  ships and effects together on every display blank. The old order briefly
  showed the previous page after the single native background buffer had
  advanced, then dropped native layers when its snapshot expired. Completion
  is tracked through normal flight, initial launch and the Coil test loop,
  including filming mode. Early exits cannot publish an unfinished page;
  CLASSIC ordering and the compositor's stale-frame limits are retained.
- **Companion GBC space pass:** [PR #13](https://github.com/masong0508-lab/bugstar-gbc/pull/13)
  makes all 256 worlds actionable in both flight views, preserves the full
  pirate reinforcement delay, and keeps the cockpit's control/destination row
  visible after dialogue. Controller-only chart, warp, station, undock and
  landing replays at world 255 produce valid autosaves in both views. Fixed
  WRAM decreases by two bytes; saved world IDs and save format stay unchanged.

The production model, flight, beam and presentation paths are covered by
`tests/ship_model_capacity_runtime.py`,
`tests/space_flight_runtime.py`, `tests/space_native_beam_runtime.py` and
`tests/space_native_present_runtime.py`, wired into the PC workflow together
with `pc/tests/native_space_lifetime.sh`. Actual space captures and handheld
control checks use the game's existing scene/debug entry points and ordinary
controller input.

Validation for this continuation:

- The model regression checks 96,160 production models under ASan/UBSan,
  including all game/PC presets, 94,080 valid part combinations, 2,048 generated
  designs and allocation boundaries. All 94,077 models that fit are byte-for-byte
  identical to the previous builder; 2,083 exhausted models keep valid indices.
  Five original/missing-guard negative controls fail. Actual CLASSIC captures
  no longer contain the malformed polygon; native custom-hull pictures and
  the HUD are unchanged by this capacity correction.
- The flight regression checks 143,430 actual movement updates across all ten
  ship presets, including released controls, mirrored turns, braking, drift
  matching, speed limits and fuel. A separate exact impulse oracle checks
  33,221,000 cases over 1,080 complete fractional cycles, including clock wrap,
  oblique headings and custom-engine limits. Eight isolated regressions fail
  against the original or deliberately incomplete fixes.
- The real native ship renderer passes 4,000 chase-target checks and 8,000
  cockpit/CLASSIC comparisons, with six render configurations. Actual NATIVE
  and ULTRA space captures show incoming beams meeting the player hull. Held-A
  captures also confirm that the existing player bolts and ULTRA glow render
  correctly throughout their firing phase; sparse captures had missed them.
- Native publication passes 1,160 checks through the actual scheduler and
  guarded caller sequences. The real-game lifetime smoke passes 15 cases and
  1,018 captured display blanks on both the final normal and ASan binaries:
  all four flight phases, turning/firing, chase/cockpit, NATIVE/ULTRA/CLASSIC,
  galaxy-menu return and landing/walking/shop/launch. One/four-thread pixels
  and saves match. The old binary fails on its expired age-five snapshot.
  Existing cache/shading/effects parity also passes at 960x640 and 480x320.
  Local LeakSanitizer scanning is disabled for the traced runner; address and
  bounds instrumentation remain active.
- Five fresh-boot mGBA scenarios exercise the production ROM through ordinary
  controller input, totaling 9,554 emulated frames. Released yaw/pitch rates and bank
  now reach zero; equal left/right input gives equal opposite turns. R braking
  reaches zero velocity instead of retaining forward drift. Firing and both
  cockpit/chase transitions also pass. The comparison uses identical emulated
  and simulation frame counts. All 39 final-ROM checkpoints and sampled flight
  states match the earlier flight fix after the model-capacity correction.
  This is a bounded flight smoke, not a campaign run.
- The final production GBA build adds 3,752 bytes of ROM code. Data/BSS remain
  564 / 263,684 bytes. IWRAM stack reserve is 1,664 bytes (eight bytes below the
  preceding kart pass), above the 1,500-byte project requirement. Model buffer
  sizes, save layout and persistent RAM are unchanged.

## Kart road projection and filtering — 21 November 2026

- **Ordered ground depth (GBA/PC/Android):** retain the unsigned reciprocal
  when interpolating scanlines. Casting the reciprocal for two rows to a signed
  value reversed the interpolation and made road edges repeatedly step backwards.
  The reciprocal for one row now saturates instead of overflowing to zero.
- **Clipped camera endpoints:** preserve the actual projected endpoints when
  the profile starts below the screen or crosses above it; clip only the rows
  written. This fixes stretching at high camera settings. Spans beyond the
  reciprocal table use bounded integer division instead of indexing past it.
- **Continuous native filtering (PC/Android):** retain each texel's color in
  its interior and blend at its boundaries. The previous weights switched
  neighbors at the texel center, creating sharp repeating ripples. Both native
  sampler paths are corrected. Road-marking emphasis, distant sampling and the
  flat-color fast path remain intact. Sample footprints now use game-space
  screen center 120 at every output resolution.
- **Build and regression coverage:** GBA incremental builds now depend on
  `m7.inc` and `recip.inc`. The PC workflow watches both files and exercises the
  real ground-depth code and native filters against independent references.

Validation for this follow-up:

- The shared-depth regression exercises 3,496 frames and 559,360 row checks
  under UBSan, including every camera/distance preset, spans of 1/2/257/258
  rows, large clipped spans and guarded buffers. An independent flat-plane
  oracle stays within 1.037 screen rows. Restoring either reciprocal defect or
  the old clipping independently makes the test fail.
- The native filter regression checks 391,038 texel centers/interiors, 72,490
  independently integrated color samples, 20,500 monotonic ramp samples and
  5,120 screen-center footprints. It also covers far sampling, edge clamping
  and the road-marking threshold; the original filter and footprint each fail
  an independent negative control. Existing sampler, inverse, camera and
  sprite-anchor regressions pass.
- Eight automated PC race scenarios and four further scenarios under
  AddressSanitizer end with zero reported problems. Native composition at four
  checkpoints also passes ASan. Local leak detection is disabled because this
  traced runner cannot run LeakSanitizer; bounds instrumentation remains active.
  Actual native and classic SDL frames at 320, 600, 900 and 1200 show the
  corrected road geometry and reduced texture ripple. Native pictures match
  exactly with one and four rendering workers.
- Local 960x640 composition averages 8.58 ms with one worker versus 10.03 ms
  before. Three paired four-worker runs give median averages of 3.54 ms versus
  4.10 ms before; individual runs vary with scheduling. These measure renderer
  composition on this host, not whole-game FPS or handheld performance.
- The production GBA ROM passes a real mGBA smoke through 2,954 frames,
  including Kart World, acceleration, steering and braking. The five title/menu
  checkpoints are pixel-identical to the prior build; race images change as
  expected. This is a bounded runtime smoke, not a full handheld race.
- GCC 12.2 builds with unchanged data/BSS (564 / 263,684 bytes). Loaded kart
  code grows by 72 bytes, reducing the IWRAM stack reserve from 1,744 to 1,672
  bytes, still above the project's 1,500-byte requirement. No persistent
  buffers, framebuffers or save fields were added.

## Kart reliability continuation — 21 November 2026

- **Reusable race items and editor markers (GBA/PC/Android):** retired shells,
  slime and gate markers reuse their existing borrowed scenery slot. Previously,
  repeated reuse appended duplicate ownership entries until the 48-slot list
  filled, after which new items or gate markers silently failed. Active racers,
  scenery and item boxes waiting to respawn remain protected. No production RAM
  arrays, save fields or framebuffer allocations were added.
- **Coherent native kart sprites (PC/Android):** chassis, driver, roof and cloud
  effects move together using the kart's ground contact; scenery uses its feet.
  Jump lift stays intact. Pending contacts are published when OAM is flushed,
  so constructing the next frame cannot move sprites still on display. This
  uses 1 KiB of PC renderer metadata and does not change GBA memory.
- **Companion GBC fix:** [PR #11](https://github.com/masong0508-lab/bugstar-gbc/pull/11)
  restores the shield's intended 420-tick duration instead of byte-truncating it
  to 164. An unused byte offsets the wider timer; fixed WRAM remains unchanged.

Validation performed for this continuation:

- `python3 tests/kart_object_pool_runtime.py`: 10,000 item recycling cycles,
  full-capacity reuse, 1,000 real editor drop/undo cycles and scenery restoration
  under UBSan. The original allocator fails the regression.
- `python3 tests/kart_native_sprite_anchor_runtime.py`: 154,791 projection
  checks and 126 compositor fixtures, including affine sprites, jumps, fixed HUD
  placement and pending/live frame isolation. The old per-sprite warp fails.
- Eight full automated PC race scenarios and four further scenarios under
  AddressSanitizer ended without hangs or reported errors. Local leak detection
  was disabled because the traced runner cannot run LeakSanitizer; bounds
  instrumentation remained active. Actual SDL captures at frames 320, 600, 900 and 1200 were inspected;
  classic captures are byte-identical to the baseline. One- and four-worker
  native pictures are identical.
- GBA ROM built with GCC 12.2: data/BSS and the 1,744-byte stack reserve are
  unchanged; ROM code grows by 24 bytes. A real mGBA run exercised title,
  Kart World, random race, acceleration, water recovery, steering and braking
  across 2,954 emulated frames; all ten checkpoints match the baseline.

The emulator check is a bounded runtime smoke, not a complete handheld race or
hardware performance measurement. That pass did not change the road sampler;
the follow-up above repairs the depth and filtering defects behind its ripple.

## 0. Where things stand

**GBA (root `main.c` + `*.inc`)**
- Done: editable difficulty (13 numbers, saved per difficulty); Bugstarillion v2 (11,896,480 parts); level cap is the 32-bit limit (`LEVEL_CAP`, 4,294,967,295; PC overrides it); kart fixes (results stack overflow, circling rivals, lap counting, table over-read, road ground left on); Stormclock Coil (8-step quest, 1 in 10,000 drop with 6 conditions, speed tiers up to STORM at 2.3759 x 10^43 c, exact readout, 9 detail pages, Planck-time counts, pass-through effects with an Options setting, glitches with FLASH-SAFE); living stat bars; mini stat panel hidden by default (Options: MINI STAT BARS); ship points spend by MAX/100/50/5/1 (LEVEL tab in the hangar).
- Hard limits unchanged: IWRAM and EWRAM are full (new code and tables go in ROM), stack headroom must stay above 1500 bytes (last read 3,248), SRAM blocks do not move, every new block needs a version.

**GBC (`bugstar-gbc`, GBDK-2020)**
- Done: town places, PORTAL spell, PARITY.md, the kart jump-pad sprite fix (compiled, not played).
- Not ported yet (see PARITY.md): humanoid form and callings, landable asteroids and mines, keycard outposts, kart garage, IDENTIFY, WORKBENCH. The GBC HOW BIG page is excluded. `PARITY.md` still says karts are not ported; that is out of date (the GBC has simple karts).

**PC (`pc/`)**
- Done: SDL2 window (resizable, fullscreen, hi-DPI, vsync, keyboard, gamepad with hot-plug and rebinding, mouse, text size, high contrast), per-OS saves, NATIVE kart view, ship renderer (CLASSIC/NATIVE/ULTRA, 10,000-ship far battles), content x16 (1,040 traits, 112 stats, 512 peoples, 256 cities, 12,672 creature kinds, 536,870,912 plants), meaning layer (32 channels, roles, synergies), 146 universe laws with editor, HOW THIS WORKS, CHEATS, planet creator with biofootprint and share codes, side game Phases 1 to 5 (town, seasons, friends and letters, colony ladder, failure and rewards, 100-year catch-up), 48 god powers (power n at level 1000 x n), 64^256 level (`Lv`), giant numbers viewer (2^(2^32), 1,292,913,987 digits) and Bugstarillion v3, colossal warships (19 sizes, 16 hulls, interiors, reward formula, threat, cost screen), missions board and random encounters, ship editor, 1,565 facts pages.
- Windows exe: compiled only, never run.
- Status 1 Nov 2026 (late): warship aftermath (hellscape, sealed entrance, escape, floor invasions, boarders), colony map screen, side game DIFFICULTY, native dungeon, kart NATIVE ground, window screenshots, facts pages refreshed (1,571 total), planet creator crash fixed, all pc/tests suites re-run and passing.

## 1. Do first (small, high value)

1. **Run the Windows exe on a real Windows machine.** Fix whatever breaks (paths, audio, `VirtualAlloc` memory map, gamepad). Add an auto-built CI job for Linux and Windows packages.
2. **Regenerate and re-run the whole PC test set** (`pc/tests/*.sh`, `pc/*/test.sh`). Several suites were skipped near the end: `coil.sh`, `giant.sh`, `giant/test.sh`, `warship.sh`, the older sim tests after the last rebase. Fix stale goldens (`seeds.golden`, `laws.golden`) once, in one commit.
3. **Re-run the GBA coil fuzz and kart fuzz on the final ROM** (the fuzz passed on earlier ROMs, not on the last merge).
4. **Play the GBA ROM start to finish** once: new game, level up, spend ship points, MINI STAT BARS toggle, kart race, coil quest log. Look for anything that looks wrong at the HUD.
5. **Fix `PARITY.md`** (karts exist on the GBC) and write the GBC jump-pad fix into its notes after playing it.

## 2. Warship threats: DONE except balance (PC, `pc/pc_missions.inc`, `pc/warship/`)

The planet-buster countdown, orbital strikes and troop waves are in. The extreme end is only a documented hook (`pcm_planet_lost`, `pcm_tick`):
- **Hellscape:** a severe bombardment turns the whole surface into fire (use the hell-world look, weather and per-second damage).
- **Collapsing entrance:** a chance that the dungeon entrance is rubble-sealed; other ways out (dig, another stairway, portal).
- **Portal escape:** arrive on the burning surface with a survival timer from HP, armour, shield, fire resistance and supplies; ship may be intact, damaged or destroyed (escape pod, wingman pickup, or stranded).
- **Floor invasions:** troop waves that hit your dungeon floor and every floor above you in turn.
- **Boarding party** fight inside the dungeon at high threat.
- **Death penalty only:** wake at the nearest station with the stated gold and item loss; never corrupt the save; story worlds never permanently destroyed.
- Gate everything with WARSHIP ENCOUNTERS (OFF/RARE/NORMAL/COMMON) and side game DIFFICULTY.
- Tests: bot runs with and without preparation; the entrance-collapse path; the save is byte-identical apart from the stated penalty.

## 3. Side game: Phases 6 and up remain (PC, `pc/sim/`)

- **Colony map screen:** the 8 x 6 grid exists in code but has no screen. Add placement ("choose where"), selecting a building, walking and carrying (Keflings-style) instead of menu actions.
- **Permanent ruins:** a destroyed planet stays a burnt world in the colony records, resettle elsewhere, and the planet file shows it.
- **Invasion hook:** DEFEND missions are only a flavour tag; connect them to the colony's raids once an invasion API exists.
- Phases 6 and up from `pc/DEPTH_PLAN.md`: workshops and the mine with z-levels, walls and raids, the market, WorldBox-style races/tribes/powers using the 12,672 creatures and 48 god powers, the replay layer and sandbox (all powers open).
- Put side game DIFFICULTY in the main Options (needs `pc/gen/main.c` and the options save).
- Pass the count of open god powers into the meaning profile (done through `pcx_gods`; check it with a test).

## 4. PC rendering and feel (`pc/platform`, `pc/shipr`)

- **Native-resolution 3D** for the dungeon, planet and space views (Seam 6 in `pc/NATIVE_PLAN.md`). They still draw at 240 x 160 and are scaled.
- **Kart NATIVE view** is about 34 to 44 ms per frame at 1200 x 800 on a 2-core box; get it to 60 fps (more threads, tile cache, fewer per-pixel branches). Add banking, hills and shadows.
- **CLASSIC far fleets** at 10,000 ships turn into solid blocks; use a density map instead.
- **Enemy beams:** present in NATIVE and ULTRA; chase-view alignment is corrected
  in the space continuation above.
- Capture real window screenshots (the automatic screenshot was not written in window mode).
- Ship editor: use the renderer for the preview, `sy_rooms` now drives the warship interior builder (`wd_make_mix`, patch 18; the game does not call it yet), make foe designs change stats and fight rules.
- Flash-safe and photosensitivity check on the NATIVE glitches.

## 5. Giant numbers (PC, `pc/giant/`)

- Run the Python reference tests (`pc/giant/test.sh`) and compare the window at 1,000,000 digits against Python.
- Test the cache build and delete buttons, the full text export (1.3 GB) with its SHA-256, and the v3 screens.
- Speed up the worst-case middle window (151 to 190 s and 2.5 GB RAM): chunked exact computation, more checkpoints inside the 100 MB budget.

## 6. Content and the facts screen

- DONE: `pc/facts/09_pc.py` now quotes the x16 numbers; hellscape, threat and native view pages added.
- The in-game text of each new PC screen needs a pass for plain words.
- Add the planned page counts to the GBC (the GBC HOW BIG page is excluded on purpose).

## 7. GBC parity (`bugstar-gbc`)

Port in this order, one feature per build, `make -j3 GBDK_HOME=/opt/gbdkx/gbdk` takes about 4 minutes, check with `/home/claude/tools/walnut/gbrun`:
1. Humanoid form and callings.
2. Landable asteroids and mines.
3. Keycard outposts.
4. Kart garage.
5. IDENTIFY and WORKBENCH.
6. The new HUD (living bars, mini panel option) and ship points if ROM bank space allows.
Keep PARITY.md the single source of truth.

## 8. GBA ideas still open

- The older `ROADMAP.md` list (link check against the real runtime library, ore grade stacks and gem sockets, overlapping the dungeon and planet buffers to free about 38 KB of EWRAM, then ground resolution 128).
- Smoother GBA kart road, a planet horizon, less flicker (asked for, not done; the PC has a better look).
- A planet-surface turn inversion report from earlier was not reproduced; ask for the view and situation if it is seen again.
- Gyroscope and tilt controls were rejected.

## Rules that stay

- Small steps; push to main with `git fetch origin && git rebase origin/main && git push -q origin <branch>:main` (never push without `:main`; never `git stash`).
- Never touch the GBA ROM build when working on PC-only code (guard it).
- The core game is never overridden by the side game: with SIDE GAME off the save is byte-identical (`pc/tests/sim_off.sh`).
- Facts pages: font only A-Z 0-9 space . , ' : ( ) - ^, no plus sign, title at most 28, lines at most 58; run `python3 pc/facts/check.py`.
- Commit messages end with the Co-Authored-By and Claude-Session lines.

## Status 3 Nov 2026 (test run)

- Passing: determinism, laws, inputs, creator, screens, worldfile, coil, giant, warship, aftermath, shipyard, shipyard_space, sim_friend, sim_lab, sim_town, sim_unlock, sim_collect, kart (12 races, 0 problems).
- Do not run test suites in parallel: they share `pc/gen`, and a parallel `kart.sh` build fails.
- `seeds.golden`: 4 picture hashes refreshed (2 trait planets, only the distant city skyline rows differ: building windows changed in `planet.inc`). Text lines unchanged.
- `sim_off.golden`: refreshed from the current build with the side game OFF. It is no longer proof of the pre-side-game picture (drift from frame 2800, cause not found); the ON vs OFF `.sav` equality and no-`.sim`-when-OFF checks still pass and are the real isolation proof.
- Rechecked in the November space pass: the preset-ship player bolts are visible
  in both NATIVE and ULTRA. Held-A versus idle captures across four firing
  phases show the paired bolts and ULTRA glow; sparse captures between bursts
  can miss them. Enemy beams are also present, with their chase-view endpoint
  corrected by the continuation above.
