# BUGSTAR PC: DEPTH PLAN (the side game)

Short sentences on purpose. Read top to bottom. Every name below is a real function or variable I read in this repo, or it is marked NEW.

## 0. Read this first (one minute)

- The action game stays exactly as it plays now. Planets, dungeons, space, karts: no change.
- We add a SIDE GAME inside it: a bug colony and a life sim. Think ActRaiser. Action phase, then sim phase, then action again.
- You enter it from your ship, a town building, or a settled planet. You can leave with B at any time.
- Winning in the action game UNLOCKS and GROWS the colony on that planet. The colony then feeds food, ore, gold, Buddies, gear and hints back into the action game.
- Everything new is PC-only. The GBA ROM build (`Makefile`, root `*.inc`, `main.c`) is never edited.
- The sim is a separate layer with its own clock, its own random numbers and its own save file. It never touches `rnd()`, `rndi()` or `wr()`. So old worlds, old saves and `seeds.golden` do not change.
- Phase 1 is small: named townsfolk with needs and a daily routine in the town you already have, plus a journal. Section 8.

## 1. The side-game frame (ActRaiser style)

### 1.1 Entry and exit

| Where | How | Hook (real name) |
|---|---|---|
| Your ship | New row COLONY in `shop_menu()` (ui.inc) | `case O_SHIP` in `planet_frame()` |
| Empty ground | A on nothing opens the QUICK MENU (JOURNAL, WHAT NEXT, COLONY, PEOPLE) | the `if(best<0) show_msg("NOTHING HERE")` line |
| A town building | A on `O_HOME` / `O_OFFICE` / `O_CANTINA` in a settled town | the `switch(po[best].kind)` in `planet_frame()` |
| Settled planet | The colony banner appears on `arrive_on_planet()` | NEW `sim_arrive()` |

- B always leaves. The side game never blocks `hearts<=0`, `launch_flag`, `enter_dungeon_flag` or `sys_req`.
- On a planet with no unlocks the side game shows ONE line: what to do to unlock the first step. No empty menus.

### 1.2 The unlock ladder (what action unlocks what)

Each rung uses a flag the game already keeps. Rungs are per planet, except where noted.

| # | Action milestone (existing flag) | Unlocks on THAT planet |
|---|---|---|
| 0 | Land on any planet. `arrive_on_planet()`, `st_planets` | JOURNAL and townsfolk life (Phase 1). Free. |
| 1 | Free a caged bug. `free_bug(t)`, `bugs_freed>=1`, `O_CAGE` | TENT plot and 2 settlers (the freed bug can be one). |
| 2 | Clear the door dungeon once. `zone_cleared` (story) or reach `best_depth>=3` (endless) | FIELDS, a well, a stockpile, 4 settlers. |
| 3 | Beat the planet's boss. `story_boss_beaten()`, `tiers[TZ]` | HOUSES, your BUG HOME (life sim), the first festival. |
| 4 | Clear a drone outpost. `outposts_done`, `data_cores`, `base_mask` | WORKSHOPS (kitchen, loom, smelter link to `smelter_menu()`). |
| 5 | Mine ore tier 2+. `ore_tool>=1`, `ml_total` | z-LEVEL 1 to 4 (the colony mine), dig designations. |
| 6 | Clean a hive. `hive_kill()`, `hive_here()` | GUARD post, walls, trade caravans, deeper z-levels (5 to 8). |
| 7 | Beat the Ant King. `ant_king`, `outposts_done>=6` | MARKET HALL, 20 settlers, z-levels 9 to 15, the Forge link (`forge_begin()`). |
| 8 | Hold a relic. `relic_bits` | MUSEUM (shows relics and made artifacts, like `trophy_case()`). |
| 9 | Planet "thriving" (score, section 6.3) | NEW COLONY PLUS (6.2). |

- Higher tiers of the same planet (`tier_now()`, `tiers[]`) raise colony caps and add one extra rung each. Going back down a cleared zone is rewarded.
- Unlocks are stored as a bit mask per colony in the side file. The action game never reads that mask.

### 1.3 The loop for one planet

1. ACTION: land, explore, fight in the door dungeon, free bugs, mine.
2. A milestone fires. A card says what it unlocked ("A TENT! 2 SETTLERS ARE WAITING.").
3. SIM: you visit the colony (ship menu). You place plots, pick jobs, read the journal. Short. A few minutes.
4. The colony runs by itself while you go back to action. It keeps ticking (coarse steps when you are in a dungeon or in space).
5. The colony makes things. They appear in the action game (1.5).
6. The colony has needs or trouble. It pulls you back into action (1.4).
7. Repeat. Each lap unlocks a bit more.

### 1.4 How the colony pulls you back into action

| Trouble | What the player gets in the action game | Reuses |
|---|---|---|
| Raid (danger trait, season, wealth) | Beetles and slugs walk into the town. You defend on the surface. Guards help. | `O_BEETLE`, `O_SLUG`, `life_tick()` |
| Monster nest under the colony mine | A nest floor in the door dungeon. Clear it to open the next z-level. | `FE_NEST` in `dungeon.inc` |
| Lost settler (wandered off, strange mood) | Rescue quest: a cage on a dungeon floor. | `Q_RESCUE`, `O_CAGE`, `free_bug()` |
| Famine | Hunt critters and gather. Food quest. | `O_CRIT`, `eat()`, `Q_GOODS` |
| Missing material | A mine run. "NEEDS 5 IRON." The arrow points to a vein. | `ore_of()`, `arrow_ship` |
| Festival needs a rare item | A rare drop quest. | `roll_gear()`, `relic_roll()` |

- Pull-back is a request, never a lock. The player can ignore it. The cost is lower mood and slower growth, not a game over.
- A colony cannot be destroyed by the sim alone. Worst case: settlers flee and the plot goes dormant, ready to restart.

### 1.5 How the colony feeds the action game

| Colony output | Shows up in the action game as | Hook |
|---|---|---|
| Food (meals) | Meal items; `food` refills; BUDDY snack | `eat()`, `add_item()` |
| Ore and bars | Ore bag and smelter stock | `ore_add_q()`, `sm_n[]` |
| Gold (taxes, wages) | `gold`, with a small cap per visit | `earn()` |
| Shop stock | `restock()` adds colony goods and better gear after workshops | `restock()` |
| New Buddies | `free_bug(t)` when a pet bonds; Buddy `pet_lv` bonus | `pet_xp`, `feed_pet()` |
| Gear | Made by the smith with settler skill as quality | `add_gear()`, `Gear` |
| Hints | Gossip lines become true tips from the colony's scouts | `gossip_tip()`, `gossip_need()` |
| Quests | Bounty office jobs tied to colony needs | `make_quest()`, `pq_type` |

- Caps keep it fair: the colony adds at most about 25 percent to what the action game gives. No one can skip the action game by waiting.

## 2. What the game has today (audit, files and functions I read)

| Area | Real code | Can carry |
|---|---|---|
| Frame loop | `run_game()` in main.c: `read_keys()`, `planet_frame()`, `dungeon_frame()`, `space_frame()`, `vsync()` | Where the sim clock hooks |
| Time | `vbl_count` (sound.inc, +1 per vblank). `frames` counts loop turns, NOT 60ths. `wd.hour` (0 day, 1 dusk, 2 night) is fixed DNA. There is NO running day clock. | Sim owns the clock |
| Planet DNA | `make_world()` (game.inc): seeds A,B,C, `wd.tseed`, `wd.cseed`, `wd.cls`, `wd.weather`, `wd.mood`, `body_salt()` | World seed for the sim |
| Terrain | 128x128 `hmap`, 256x256 `hfine`, `cmap`; `city_flatten()`, `city_stamp()` stamp buildings into the ground | Colony buildings can be stamped the same way |
| Objects | `PObj po[NPO]`, NPO=160, always filled (`new_planet()` fills the rest with `O_PEBBLE`/`O_TUFT`). Sprite cap `slot<126`. | Free slots = decor slots |
| Town | `city_people()`: doors, `O_HOME`, `O_LOCAL` x (3+csize*3), `O_STALL`. Locals wander randomly in `life_tick()` | Townfolk |
| Talk | `local_talk()`: mood lines, gifts, cooldown by `vbl_count` (`gift_until`), `gossip_talk()`, `cantina()`, `bounty_office()`, `market()` | Dialogue, shops, jobs |
| Creatures | `make_species()` (2 species per planet, temper), `spawn_crit()`, herds breed every 900 frames | Critters, hunting, pets |
| Quests | `make_quest()`, `Q_*`, `pq_type` jobs (11 kinds), `job_tick()` | Quest feedback |
| Buddy | `pet_hp`, `pet_form`, `pet_lv`, `feed_pet()`, `pet_away_tick()`, `K_PET` in dungeon.inc. Not seen on planets. | Pet needs |
| Bug friends | `have_bug[6]`, `friend_name`, `bug_power`, `free_bug()` | Settler seeds |
| Outposts | `O_BASE`, `has_base()`, `base_mode`, `base_floors()`, `outposts_done`, `data_cores` | Unlock rung 4 |
| Hunger | `food`, `hunger_tick()`, `eat()`, hell/heat drains in `hell_frame()`, `weird_frame()` | Needs model |
| Fishing | `fish_step()`, `fish_roll()`, `angler()`, `pool_at()` (main.c). Used in dungeons. | Fishing loop |
| Mining | ore.inc: `ore_def[12]`, `ore_of()`, `ore_add_q()`, smelter `sm_rec`/`sm_make()`, `forge.inc`, mine floors `cur_rock==5`, `mine_hazard()` | Mining, workshops, z-levels |
| Ship, trade | `shop_menu()`, `bazaar()`, `workbench()`, `good_price()`, `eco_name`, `cargo[8]` | Trade |
| UI | `card()`, `list_pick()`, `panel()`, `char_screen()` (9 tabs), `say()`, `show_msg()`, `draw_hud()` | Screens |
| Save | 32 KB SRAM, 3 slots, `do_save()`, `load_game()`, blocks with magic + sum. `pc_hw.c` maps it to the `.sav` file. | We do NOT touch it |
| PC content | `pc/content/pc_content.c`: `planet_make()` (65 traits, 3 to 6 per planet), `universe_themes()` (24 themes), `addr_seed()` (2048-bit). Being wired in by another agent. | Colony flavour |
| Tests | `pc/tests/determinism.sh`, `seeds.sh`, `play.txt`, `cont.txt`, `pc/content/test.sh` | Pattern to copy |

Facts that shape the design:

- `po[]` is full, and `order[]` in `planet_frame()` is `u8`. So NPO cannot pass 255. Do not raise it. Recycle decor slots.
- Only `kind<=O_LAST_USE` (O_SHRINE) can be used with A. `O_STALL` and `O_BEACON` cannot. The sim keeps its own "nearest thing" scan.
- Dungeon floors use `rndi()` (unseeded). So a colony mine cannot reuse them directly. It gets its own seeded grid (Phase 7).
- `town.inc` already has `life_tick()`, `job_text()`, `job_tick()`, `weather_t`, `weather_draw()`. `game.inc` already has `trait_name(int,char*)`, `traits_apply()`, `NTRAIT`, `MW_*`. Do not reuse these names (section 9).

## 3. Simulation engine upgrade (PC only)

### 3.1 Files

```
pc/sim/sim_core.h / sim_core.c   pure C, no game types: rng, entities, needs, jobs, events, history
pc/sim/sim_world.c               colony grid, designations, pathing, production
pc/sim/sim_test.c                standalone tests (like pc/content/content_test.c)
pc/sim/sim_glue.inc              the only file that sees game statics (wd, po, gold, say, card...)
pc/sim/sim_ui.inc                screens (journal, colony map, build menu)
pc/sim/sim_proto.h               'static' prototypes the hooks need
pc/tests/sim_*.sh + *.txt        headless tests
```

- `sim_core.c` and `sim_world.c` are plain .c, compiled into `bugstar_pc` and into `pc/sim/sim_test` (so logic is testable without booting the game).
- `sim_glue.inc` and `sim_ui.inc` are appended to `gen/main.c` the same way `pc_view.inc` is (`echo '#include "../pc_view.inc"' >> main.c` in build.sh).
- `sim_proto.h` is inserted after the existing `sed -i '1i #include "pc_hw.h"' main.c` line. Prototypes are `static`, defined later in the same file.
- Compile with `-DBUGSTAR_PC`. Nothing in the root files checks it. Only the generated copy sees hooks.

### 3.2 Hooks (sed on the copies in `pc/gen`, never on the repo root)

Add a `pc_hook 'anchor text' 'added text'` shell function to build.sh. It fails the build loudly if the anchor is not found exactly once. This protects us when the game changes.

| Anchor in the game copy | Added | Why |
|---|---|---|
| `life_tick(n); race_tick(n); kart_tick(n);` | `sim_frame(n);` | main planet tick |
| `if(wtick){ wacc=0; frames++;` | `sim_dungeon_tick();` | coarse steps in dungeons |
| `int r=space_frame();` | `sim_space_tick();` | coarse steps in space |
| `new_planet(); planet_enter(); arrow_ship=0;` | `sim_arrive();` | build the townfolk, catch up |
| `save_ext(); uni_save(); grind_save(); ore_save();` | `sim_save();` | write side file |
| after `load_game(1)` in `start_game()` | `sim_load();` | read side file |
| `static void new_game(void){ ore_reset();` | `sim_reset();` | fresh game |
| `if(best<0) show_msg("NOTHING HERE");` | `if(best<0 && !sim_quick()) show_msg(...)` | quick menu |
| `local_talk(&po[best]);` | `if(!sim_talk(&po[best])) local_talk(&po[best]);` | named talk |
| end of `draw_hud()` | `sim_hud(pg);` | clock chip |
| `case O_SHIP:` body | a COLONY row in `shop_menu()` | entry |

- Hooks only call `sim_*`. All sim logic lives in the new files.
- `sim_*` functions return at once when the colony layer is off (new OPTION `SIDE GAME: ON/OFF`, default ON). OFF means a byte-identical action game.

### 3.3 The clock (fixed step)

- Fixed step: 10 sim ticks a second. One tick = 6 vblanks.
- `sim_feed(n)` adds `n` (the vblanks `planet_frame()` already computed, 1 to 4) to an accumulator. `while(acc>=6){ acc-=6; sim_step(); }`.
- Time is the fake GBA's counter, so it is exact on the PC. No wall clock. Same key script gives same ticks.
- Menus: `vbl_count` runs on in menus. The sim does NOT use `vbl_count` deltas. It only counts what `sim_feed()` gets. So menus pause the world, except the colony screen, which has its own speed control (6.5).
- In dungeons and space: `sim_dungeon_tick()` and `sim_space_tick()` feed coarse steps (one step per 60 vblanks, rules simplified, same code path with `coarse=1`).
- On landing, `sim_arrive()` fast-forwards the planet by the play time away, capped at 7 game days, in "hour steps" (`coarse=2`). This is the "catch-up".
- Day length from the planet: `day_ticks = clamp(4800 * day_hours / 24, 1200, 12000)` (8 minutes for a 24 hour day). TIDALLY LOCKED (day_hours=0) has no day, only a fixed sun. LONG NIGHT is clamped, not literal.
- Season: 7 days per season, 4 seasons. Strength from `temp_c` and traits (6.3 Phase 2).
- Fast-forward x1, x3, x10 and PAUSE exist on the colony screen. Fast-forward runs more `sim_step()` per frame (cap 20 per frame) so it never makes input lag.

### 3.4 Determinism rules

1. The sim has its own generator: `sim_rng(world_seed, id, purpose, tick)` = `mix64` of those four. No state. Order of entities cannot change results.
2. `world_seed` = `mix64(wd.tseed, wd.cseed, body_salt(), sys_id(cur_sys), gsk(1))` (NEW `sim_world_seed()`). Once the other agent exposes `addr_seed()`, mix that in too, only for addresses beyond the old range. Old addresses keep the same value.
3. Never call `rnd()`, `rndi()`, `wr()`, `rng_s`. Never read `keys` inside the sim (the player acts through `sim_press()` commands queued and applied at the start of a tick).
4. Floating point is banned in sim code. Use integers and 16.16, like the game.
5. Iteration is by id, ascending. No pointer-order, no `qsort` on equal keys without a tiebreak.
6. The sim never writes the fake GBA memory except through the drawing functions during draw.

### 3.5 Entity table

```c
typedef struct {                 // about 200 bytes
  u16 id; u8 kind, alive;        // townfolk, settler, pet, critter, raider
  u32 name_seed; u8 species, traits[3];
  s32 x, y; u8 z;                // 16.16 world cells, z-level (0 = surface)
  u8 need[6];                    // HUNGER REST SOCIAL FUN COMFORT PURPOSE (0 bad .. 255 good)
  s8 mood; u8 thought_n; Thought th[6];   // thought token + decay
  u8 skill[9], skill_xp[9];      // DIG BUILD FARM COOK MINE FISH CRAFT FIGHT HAUL
  u8 job, task; u16 target;      // what and where
  u8 item_fav, food_fav;
  Rel rel[8];                    // other id, affinity -100..100, kind FRIEND/RIVAL/MENTOR/PET
  Mem mem[6];                    // event id + tick
} SimBug;
```

- Table size: 256 entries on the surface for the first release. About 50 KB.
- Event log: ring of 4096 x 16 bytes = 64 KB. A second list "LEGENDS" (up to 512 entries) keeps only important events forever.
- Colony grids: 128x128 x 3 byte layers (zone, designation, built) = 48 KB. z-levels: 16 x 64x64 x 2 bytes = 128 KB. Items and stockpiles: 64 KB.
- Total under 1 MB. It lives in normal PC memory, NOT in the fake EWRAM, so it competes with nothing.
- CPU: 10 ticks a second. Each tick updates 1/6 of the bugs (round robin), so no spike. Pathing is a flow field per target (BFS on 128x128, budget 4000 cells per tick). Worst case is far under 1 ms.

### 3.6 What is stored and what is not

- NOT stored: the town. Names, traits, homes, jobs, routines and relationships between unvisited townfolk are a pure function of `(world_seed, npc_index, sim_tick)`.
- Stored (only what the player changed or what has history):
  - touched townfolk: friendship, up to 6 memories, gifts given;
  - the colony: plots, built tiles as a diff list, designations, stockpile counts, settlers in full;
  - calendar: tick count, last visit tick per planet;
  - the event log tail (last 256) and all LEGENDS you took part in;
  - unlock mask, policies, starting kit, colony-plus count.
- Side file: `<sav>.sim` (NEW). One file per slot inside it. Chunks with `magic, version, length, fnv`. Unknown chunk: skip. Missing file: empty side game.
- `pc_hw.c` gets `const char* pc_sav_path()` so `sim_save()` knows the name. `pc_hw_finish()` stays as is.
- The 32 KB `.sav` is untouched. A GBA save loads here. A PC save still loads in the emulator.

### 3.7 How the existing tests keep passing

- `determinism.sh` compares the log and the `.sav` of two runs. The sim adds no writes to the `.sav`. With the sim ON, the screens only change where we draw new things, and both runs change the same way.
- A NEW test (`pc/tests/sim_off.sh`) runs `play.txt` with `SIDE GAME: OFF` and checks the screen hashes equal a golden made BEFORE the first sim commit. This proves the action game is byte-identical when the layer is off.
- `seeds.sh` and `seeds.golden` use `--view`, which never calls any hook. They stay as they are.
- NEW `sim_test` (standalone): 10 worlds x 100,000 ticks, one hash per world, compared to `pc/sim/sim.golden`.
- NEW script command in `pc_hw.c`: `dump <frame>`. It calls `sim_dump()`, which prints one text line per fact (day, hour, season, weather, townfolk positions and tasks, stock counts). Tests grep these lines. Pictures are for humans.

## 4. Life sim (the bug home and daily life)

### 4.1 Townfolk (every town, from the start)

- Each `O_LOCAL` becomes a named bug. Binding is by order: the k-th `O_LOCAL` in `po[]`. Identity comes from `sim_rng(world_seed, k, ...)`, not from its position.
- Job by what the town has: stall keeper (`O_MARKET`), cook (`O_CANTINA`), clerk (`O_OFFICE`), racer (`O_RACE`), forager, miner, guard.
- Routine per day: SLEEP at home door (`O_HOME`), EAT, WORK at the job spot, LEISURE at the plaza (`city_cx`, `city_cy`), SOCIAL near friends. Times scale to the planet's day.
- Needs fall over time and are met by those places. Moods come from needs plus thoughts ("ATE A FINE MEAL", "SLEPT ON THE FLOOR").
- Movement: the sim sets `po[i].x/y` after `life_tick()` has run, so the old random walk is overruled with no edit to town.inc. Walkable test is the same as town.inc: road/plaza values 74, 75, 80, 81.
- Talk: `sim_talk()` uses name, job, mood, need, a memory of YOU, the weather and the news. Gifts and cooldowns keep the old `GIFT_MAX` rules.

### 4.2 Your bug home (rung 3)

- Choose a plot near the landing field. It is stamped into `hfine`/`cmap` like `city_stamp()`. Size grows 3x3, 5x5, 7x7.
- Furniture: bed, table, shelf, stove, chest, lamp, plant, rug, trophy stand (shows relics via the `relic_shelf()` idea). About 20 pieces to start.
- Comfort score from furniture and the planet. Sleeping in a good home heals, fills REST, and can skip a night (fast-forward x10 with a fade).
- Planet traits change what you can build (65 traits; `planet_make()`):

| Trait | Home feature |
|---|---|
| OCEAN WORLD / ARCHIPELAGO | dock, fishing pier |
| FUNGUS FORESTS | mushroom beds, spore lamps |
| GEYSERS / VOLCANIC | hot bath, forge heat bonus |
| CRYSTAL FIELDS | crystal windows, extra gems |
| THIN AIR / ACID CLOUDS | sealed dome needed (extra cost) |
| MIGRATING HERDS | herd pen, seasonal visitors |
| RUINS / BURIED SHIP | dig site next door |

### 4.3 Daily schedule (for YOU)

- You are not forced. The day clock offers pull, not rules:
  - REST falls: sleep on a bed to refill. A tired bug has slower legs (`mark_t[MK_SLUGGISH]` style).
  - Food: the old `food` bar stays. Meals from home are better than crumbs.
  - Work: a shift at a stall, kitchen or office pays gold and skill. Short (2 minutes), shown as a small mini-timer.
  - Hobbies: fishing, gardening, racing, collecting pebbles. They give FUN and a skill point.
- Calendar: seasons, named days, birthdays of friends, festivals (4 a year, from planet traits and legends). Shown in the clock chip and the CALENDAR page.

### 4.4 Friends, pets, letters

- Friends only. No romance. Kinds: FRIEND, RIVAL, MENTOR, PET. Affinity -100 to 100. Memories explain it ("YOU SAVED MY CRUMBS").
- Buddy gets needs (hunger, fun, bond) and a visible follower on planets (a recycled decor slot using `pet_x`/`pet_y`). Bond raises `pet_lv` gains.
- Letters: townfolk you befriended write to you. They arrive at the ship's mailbox and talk about events, requests and hints. Letters cross planets (stored in the side file).

### 4.5 Loops that feed each other

```
garden -> crops -> kitchen -> meals -> mood/food -> work faster
fishing -> fish -> kitchen / market gold
mining -> ore -> smelter/workshop -> tools/gear -> better mining + safer dungeons
herd/critters -> hides, nectar -> loom, brewery -> festival goods -> mood
gold -> furniture/buildings -> comfort -> mood -> output
```

- Each loop needs one thing from another loop. No loop wins alone (section 6.1).

## 5. Colony (the fortress layer)

### 5.1 Founding

- Rung 1 gives a tent. You pick where. The first 2 settlers come from freed bugs (`have_bug[]`), the rest arrive by caravan as prosperity grows.
- Settlers are named, with 3 traits from a list of about 30 (SHY, GLUTTON, HOARDER, BRAVE, NIGHT OWL, TANTRUMY, CURIOUS, LAZY, KIND, GRUMPY, ARTISTIC, FUSSY). Each has favourite item and food. Species follow `spc[0..1]` and the player's own bug.

### 5.2 Designation (by hand) and jobs (by the colony)

| By hand (you) | By the colony (settlers) |
|---|---|
| Place zones: DIG, BUILD, FARM, HAUL, GUARD, REST | Pick the best free task for their skill and mood |
| Place buildings and workshop orders | Haul, dig, build, farm, cook, craft, guard |
| Set policies (rations, shift length, curfew) | Eat, sleep, socialise, argue, mourn, celebrate |
| Fight raiders and nest floors yourself | Auto-resolve small raids with guards |
| Choose trade and festival goals | Run the market and the festival |
| Name things, answer strange moods | Tell stories (the log) |

- Designation uses a cursor on a top-down colony map screen (8x8 pixel cells over the planet's 128x128 grid). D-pad moves, A paints, B backs out, L/R change tool.
- A "default plan" button (SELECT) places a sensible starter layout so nobody is stuck.

### 5.3 Stockpiles and production chains

- Stockpiles are zones. Items are counted stacks (not 3D objects). Haul tasks move counts between zones with a visible settler carrying a crumb sprite.
- Workshops (rung 4): KITCHEN, LOOM, CARPENTER, SMELTER (links to `sm_rec`/`sm_make()` so bars are the same as the ship's), BREWERY, SMITH.
- Orders: a queue per workshop. Quality comes from skill.
- Chains example: farm -> flour -> kitchen -> bread; ore -> smelter -> bar -> smith -> pick -> faster digging.

### 5.4 z-levels (rungs 5 to 7)

- Colony mine = 16 levels of 64x64, from `sim_rng(world_seed, z)`. Ore tier grows with depth, using `ore_def` tiers and `ore_of()` ideas. Cave-ins, gas, water and nests use the `FE_GAS`, `FE_CAVE`, `FE_NEST` ideas.
- Cutaway view: a 2D map screen with a z slider (L/R). No 3D renderer work.
- Hand mining: enter a level in first person using the hollow-rock mine code (`cur_rock==5`, `mine_hazard()`). A NEW seed override makes the level the same each time. This is the action side of the same mine.

### 5.5 Threats

| Threat | Source | Result |
|---|---|---|
| Raids | planet `danger` + wealth + season | fight (1.4) or auto-resolve |
| Weather | WEATHER traits (STORMY, ASH FALL, SANDSTORMS, ACID RAIN) | crops, roofs, mood |
| Hunger | harvest, hoards, drains from `hunger_tick()` rules | famine events |
| Strange moods | low mood + skill | tantrum, wander, or an artifact |
| Mine | gas, cave-in, flood, nest | injuries, lost z-level |
| Pests | `O_SLUG` swarms, `MW_SWARM` | crops lost |

### 5.6 Event log and legends

- Event kinds have text templates. Example: "PIP THE WOODLOUSE THREW A TANTRUM AFTER LOSING HER FAVOURITE PEBBLE."
- Planet history is generated from the seed: 6 to 12 legends (founding, war, plague, great storm, a lost city, a hero). RUINS and ANCIENT MACHINES traits place real ruins (`O_RUIN` sites) tied to legends.
- A colony chronicle records YOUR story. The journal merges both.

## 6. DEEP, REPLAYABLE, RESPONSIVE

### 6.1 DEEP: systems that push on each other

| System | It pushes on | Delivered in phase |
|---|---|---|
| Needs and mood | work speed, tantrums, friendships, leaving | 1, 9 |
| Skills | quality, speed, who gets which job | 5, 6 |
| Seasons and weather | crops, mood, raids, roofs | 2 |
| Production chains | stock, shop, gear, festival goods | 5, 7 |
| Trade | prices (`good_price()`), caravans, ship cargo | 7, 11 |
| Threats | guards, walls, mood, pull-back to action | 9 |
| History and legends | festivals, ruins, names, quests | 10 |
| Relationships | job swaps, brawls, mentors, who stays | 3 |

- Knock-on examples (all must be testable):
  - A rainy week -> no field work -> less food -> low mood -> a tantrum -> a broken stove.
  - More guards -> less farming -> famine in winter -> need hunting trips.
  - Deep mining -> richer ore -> but gas, nests, and angry miners.
- No single best plan, by rule:
  1. Every strong choice has a cost on another system (more mining = more nests; more food = more pests; more guards = less output).
  2. Planet traits tilt the balance, so the best plan changes by planet.
  3. Test: `sim_test` runs 8 scripted "strategies" (all farms, all mines, all guards, balanced, ...) on 10 worlds and checks that no strategy wins on all of them (prosperity score spread).

### 6.2 REPLAYABLE: every planet is different

- Source of variety:
  - the planet seed and its 3 to 6 of 65 traits (`planet_make()`);
  - the 2 universe themes (24) from `universe_themes()`, e.g. HUNGER FREE removes the food chain; STARVING makes cooking mandatory; ENDLESS NIGHT swaps the day schedule; PIRATE KINGDOM raises raids;
  - the settler cast (names, traits, skills) from the seed;
  - the founding choices: site, starting kit, policy;
  - random events from `sim_rng`, plus legends;
  - scenario goals and challenge modes.
- Starting kits (unlocked by feats): FARMERS, MINERS, ARTISTS, SCOUTS, A SHIP CREW. Each changes the first 20 minutes.
- Policies: RATIONS, SHIFT LENGTH, CURFEW, OPEN BORDERS, WAR FOOTING. Each helps one thing and hurts another.
- Scenarios (pick at founding, each ends with a card and a reward):
  - FEED 20, HOLD A FESTIVAL, REACH Z-LEVEL 8, SURVIVE 3 RAIDS, 0 DEATHS FOR A YEAR, MAKE AN ARTIFACT.
- Challenge modes (opt-in, like `opt_insane`): NO MARKET, PERMADEATH SETTLERS, SIEGE YEAR, ONE-TOOL COLONY.
- New colony plus (rung 9): when a planet is thriving, you may RETIRE it. It keeps running as a trade partner and legend. You get a legacy perk (a starting kit slot, a cast member, a recipe). The next colony starts with that. Score for "thriving" = pop, mood, wealth, years, deaths, artifacts.
- Tests: `legends.golden` for 6 seeds proves different history; `cast.golden` proves different settler casts.

### 6.3 RESPONSIVE: the world reacts and never makes you wait

- Fixed step (3.3). Input is read every frame by the game's own `read_keys()`. The sim applies queued commands at the next tick (at most 100 ms). Pressing A on the colony map shows its effect that frame (a ghost mark) and the tick makes it real.
- Instant feedback for every key: a sound cue (`sfx(n)`), a one-line message (`say()`), and a changed pixel. The map cursor never waits for a sim step.
- Short steps: no step may take more than 1 ms. A budget counter in `sim_step()` defers pathing work to later ticks.
- PAUSE, x1, x3, x10 on the colony screen (L/R cycle speed, SELECT pauses). While paused you can still place plots (planning mode).
- The world remembers your choices:
  - settlers comment on what you did last ("THE BRIDGE YOU BUILT IS NICE") using memory tokens;
  - the log and journal explain WHY: every mood change lists its top two thoughts;
  - the environment changes: felled trees stay gone, pebbles you moved stay moved (stored as diffs), critters flee a new field then return, birds nest on roofs, paths wear into the grass (`cmap` tint).
- Delivered: phase 1 (log, names), 2 (weather response), 3 (memory), 4 (home shows your choices), 6 (instant map controls), 11 (speed controls, polish).

## 7. Intuitive and alive (phone-sized view)

The screen stays 240x160. Text is 4 px per letter. Use the D-pad and A, B, L, R, START, SELECT only.

### 7.1 Always on screen

- Top bar (exists): HP, MP, food, level, gold, bag item.
- NEW clock chip, bottom left: sun or moon icon, hour, season letter, weather icon. 28 px wide.
- NEW prompt line, bottom centre: "A: TALK TO PIP", "A: SLEEP", "A: OPEN COLONY". It only shows when something is near.
- NEW alert dot, bottom right: a coloured dot when the colony needs you (yellow = request, red = raid). Press SELECT+START to jump to it.

### 7.2 One-button context

| Situation | A does |
|---|---|
| Near a person | talk (name, job, mood in the card title) |
| Near furniture or a building | use it |
| On empty ground | open the QUICK MENU |
| On the colony map | paint the current tool |

- QUICK MENU rows: WHAT NEXT, JOURNAL, PEOPLE, COLONY, CALENDAR, LETTERS. One screen, six rows, no sub-menus deeper than two.

### 7.3 WHAT NEXT helper

`sim_next()` returns one sentence and a target, by priority:
1. You are hungry (`food<30`): "EAT. THE KITCHEN IS 12 STEPS NORTH."
2. A raid or lost settler is active.
3. A colony stock is empty and blocks work.
4. A letter is unread, or a festival is today.
5. The next unlock ("CLEAR THE DOOR DUNGEON TO GET FIELDS").
6. The oldest open quest step (`q_type`, `pq_type`).
- The target is shown by the existing guide arrow: add arrow mode 4 (sed on the `PObj *t=&po[arrow_ship==1?0:1];` line, then extend the mode count in the SELECT+L/R code).

### 7.4 Tooltips and journal

- Every row in a menu has a second line (the old `list_desc` hook does exactly that).
- JOURNAL tabs: LOG (latest first, filter by person), PEOPLE (needs, mood, why), LEGENDS, GOALS.
- Words: all caps, plain, 3 to 6 words per line. Same style as the rest of the game.

### 7.5 Ambient life

- Critters follow routines from `spc[]` and seasons (nesting, herds migrate on MIGRATING HERDS).
- Townfolk walk, wave, sit, sleep. Window lights follow the hour (palette tint via existing `lerp15()`).
- Weather from traits through the old `weather_draw()` with sim state choosing rain, snow, dust.
- Sound cues with the existing `sfx()` set: bell at dawn, rain loop on the sound chip, festival jingle, alarm for raids. No new audio engine needed in the first phases.

## 8. PHASES

Each phase is playable and testable headless on its own. Each keeps the GBA build untouched. Sizes: small = a day or two, medium = a week, large = more.

| # | Phase | Size |
|---|---|---|
| 1 | Townfolk + journal | small |
| 2 | Clock, seasons, weather, ambient | medium |
| 3 | Friends, memory, letters, Buddy needs | medium |
| 4 | Side-game entry, unlock ladder, tent + first settlers | medium |
| 5 | Fields, houses, bug home, loops | large |
| 6 | Colony map: zones, jobs, stockpiles | large |
| 7 | Workshops and trade | large |
| 8 | z-levels | large |
| 9 | Moods, threats, pull-back | large |
| 10 | Legends and history | medium |
| 11 | Replay layer: kits, policies, scenarios, colony plus; polish | medium |

### Phase 1: TOWNFOLK AND THE JOURNAL (small, visible)

- Goal: every `O_LOCAL` in a town is a named bug with needs, a job and a daily routine. A journal screen shows what they do. No unlock needed. The side game is only "alive town" here.
- Add: `pc/sim/sim_core.[ch]` (rng, SimBug, needs, schedule, event log), `sim_glue.inc` (`sim_arrive`, `sim_frame`, `sim_talk`, `sim_quick`, `sim_hud`), `sim_ui.inc` (journal), hooks from 3.2, `pc_hook` in build.sh, `dump` command in pc_hw.c, `-DBUGSTAR_PC`.
- Touch: `build.sh`, `pc_hw.c`, `pc_hw.h`. No game file.
- Day length: fixed 8 minutes (traits come in phase 2).
- Test (headless):
  1. `pc/sim/sim_test`: 3 worlds x 100,000 ticks, hash equals `sim.golden`.
  2. `pc/tests/sim_town.sh`: continue a save standing in the first town (`cont.txt` pattern), run 20,000 frames, `dump` every 600 frames. Assert: at least 3 named townfolk; one is at its home at night and at its work by day; the log has 10 or more lines; every need stays inside 0 to 255; two runs give equal dump text.
  3. `sim_off.sh`: with SIDE GAME OFF, `play.txt` hashes equal the pre-sim golden.
- Player sees: locals have names and walk to work, eat, sleep and chat. A on a local shows "PIP, STALL KEEPER. TIRED. HUNGRY." A on empty ground: QUICK MENU with JOURNAL. The log reads like "PIP OPENED THE STALL." A small clock chip shows the hour.

### Phase 2: CLOCK, SEASONS, WEATHER, AMBIENT (medium)

- Goal: a running day and year. Planet traits shape them. The sky and light follow the hour.
- Add: `sim_env` (day length, season strength, weather odds) from `PlanetInfo` (`gravity_pct`, `day_hours`, `temp_c`, `danger`, `life`) with a fallback from `wd` when traits are not wired; palette tint hook after `planet_palette()`; weather state machine feeding `wd.weather` and `weather_draw()`; calendar page; first sound cues.
- Test: `sim_env.sh` for 6 seeds: `dump` shows the day length differs by trait; TIDALLY LOCKED has no day; screens at four hours differ and repeat; rain only where traits allow.
- Size: medium.
- Player sees: dusk and night on every planet, seasons, rain and snow on schedule, window lights, a bell at dawn.

### Phase 3: FRIENDS, MEMORY, LETTERS, BUDDY (medium)

- Goal: townfolk remember you; letters; Buddy has needs and follows on planets.
- Add: relationships, memory tokens, dialogue grammar, mailbox row on the ship menu, side-file chunk `FRND` and `LETR`, Buddy follower in a recycled decor slot.
- Test: script gives a gift; `dump` shows affinity up; save and reload keeps it (compare `.sim`); letter appears after one in-game day.
- Player sees: "PIP: THANKS FOR THE CRUMB LAST TUESDAY." Letters at the ship. Buddy trots behind you and sulks if you forget it.

### Phase 4: SIDE-GAME ENTRY, UNLOCK LADDER, TENT (medium)

- Goal: the frame of section 1. Rungs 0 to 2 work. A tent and 2 settlers exist.
- Add: COLONY row in `shop_menu()`, unlock mask from game flags (`bugs_freed`, `zone_cleared`, `best_depth`, `outposts_done`, `hive_kill()`, `ant_king`, `relic_bits`), unlock cards, colony record in the side file, tent stamp, 2 settlers as `SimBug` (kind settler), the exit rule (B anywhere).
- Test: `sim_unlock.sh`: a save with `bugs_freed=0` shows "LOCKED: FREE A CAGED BUG"; a save with 1 freed bug shows the tent; leaving and re-entering keeps it; `sim_off.sh` still passes.
- Player sees: free a bug and a card says "A TENT! 2 SETTLERS". The ship menu now has COLONY with a tent and two named bugs who work.

### Phase 5: FIELDS, HOUSES, BUG HOME, LOOPS (large)

- Goal: rung 2 and 3 content. Garden, kitchen at home, fishing, mining feed each other. Output flows to the action game (1.5).
- Add: farm plots with seasons; recipes (crop + fish + berry -> meals with mood buffs); furniture and comfort; `sim_to_game()` for food, gold, ore, shop stock via `restock()`; fishing at planet water using `fish_roll()`.
- Test: scripted week: crops planted, harvested, cooked; `dump` shows stock chain; `food` rises after a meal; shop stock gains an item after workshop.
- Player sees: a home, a garden, hot meals, a visible reward in the shop.

### Phase 6: COLONY MAP: ZONES, JOBS, STOCKPILES (large)

- Goal: the top-down map with DIG, BUILD, FARM, HAUL, GUARD, REST zones; settlers pick tasks; pathing by flow fields.
- Add: `sim_world.c`, map screen, cursor, pause and speed, the default plan button.
- Test: designate a farm zone and a stockpile; after 3,000 ticks `dump` shows fields tilled, items hauled; path cost under budget; a replay of the same command list gives the same hash.
- Player sees: paint a zone, watch bugs go and do it.

### Phase 7: WORKSHOPS AND TRADE (large)

- Goal: rung 4 and 6 content. Workshop orders, smelter link, caravans, market prices.
- Add: workshop table, order queue, skill quality, `good_price()` link, caravan events, festival goods.
- Test: an order for 5 bars finishes; ship `sm_n[]` receives them; a caravan arrives on a fixed day for a fixed seed.
- Player sees: smoke from a smelter, a caravan, better gear in the shop.

### Phase 8: Z-LEVELS (large)

- Goal: rungs 5 to 7. 16 levels, dig designations, cutaway view, hazards, nests.
- Add: seeded level generator, dig tasks, cave-in and gas rules, nest floors hooked to `FE_NEST`, optional hand-mining entry.
- Test: designate digging; after N ticks level 3 is open with the expected ore count for the seed; a nest raises a pull-back alert; the same seed gives the same map.
- Player sees: a mine that grows downward, ore piling up, a "NEST!" alert that sends you into the action game.

### Phase 9: MOODS, THREATS, PULL-BACK (large)

- Goal: tantrums, strange moods and artifacts, raids, famine, lost settlers; the pull-back table of 1.4.
- Add: break rules, strange-mood workshop claim, raid generator (danger trait), guard auto-resolve, lost-settler `Q_RESCUE`.
- Test: forced seed: raid arrives on day 9; guards auto-resolve; with no guards, the alert fires and the surface fight spawns beetles; settlers survive or flee deterministically; `dump` log contains the expected story lines.
- Player sees: stories in the log, a red dot, a fight at the gate, a lost bug to rescue.

### Phase 10: LEGENDS AND HISTORY (medium)

- Goal: seed-made planet history and the chronicle.
- Add: history generator (eras, names, ruins sites), LEGENDS tab, festival tie-ins, museum (rung 8).
- Test: `legends.golden` for 6 seeds; two runs identical; a RUINS planet has a ruin tied to a legend.
- Player sees: "LONG AGO THE ANTS OF KORAX-7 FOUGHT THE BEETLES OF THE SALT." and the real ruin that matches it.

### Phase 11: REPLAY LAYER AND POLISH (medium)

- Goal: kits, policies, scenarios, challenge modes, colony plus, WHAT NEXT helper polish, tooltips, balance sweeps.
- Add: scenario table, kit table, legacy perks, balance test.
- Test: the 8-strategy sweep (6.1); `cast.golden`; colony-plus carries one perk; full headless run of a 2-hour scripted game under 2 minutes of CPU.
- Player sees: a choice of how to start, goals, and a second colony that starts better.

### Where each promise is delivered

| Promise | Phases |
|---|---|
| Side game, not a replacement | 1, 4 (OFF switch, `sim_off.sh`) |
| ActRaiser unlock ladder | 4, then 5 to 8 |
| Pull-back into action | 8, 9 |
| Colony feeds the action game | 5, 7 |
| Deep | 2, 5, 7, 9, 10 |
| Replayable | 10, 11 |
| Responsive | 1, 2, 3, 6, 11 |
| Intuitive HUD and helper | 1 (chip, quick menu), 4, 11 |

## 9. RISKS and what NOT to do

| Risk | Rule |
|---|---|
| Breaking the GBA build | Never edit root files. All hooks are `sed` on `pc/gen` copies. If an anchor moves, the build fails loudly. |
| Old saves | The `.sav` layout is untouched. The side file is optional. No file = empty side game. Version every chunk. |
| `seeds.golden` | The sim never calls the game's RNG and never changes `make_world()`. Test it with `seeds.sh` after each phase. |
| Name clashes | game.inc has `trait_name`, `traits_apply`, `NTRAIT`, `MW_*`. town.inc has `life_tick`, `job_text`, `job_tick`, `weather_t`, `weather_draw`. main.c has `rnd`, `num`, `text`, `rect`, `bar`, `box`, `card`. All new code uses `sim_` or `col_`. The other agent's `pc_content.h` also exports `trait_name`; keep that file in its own .c and call it only from `sim_glue.inc` through a `pcx_` wrapper. |
| Sprite and object limits | NPO=160, `order[]` is `u8`, `slot<126`. Do not raise NPO. Recycle decor slots; show at most about 40 colony sprites; draw buildings as stamped terrain. |
| Stack and size | The game runs on a 16 MB mmap stack. Put sim tables in `static` arrays or `malloc`, never on the stack. |
| Merge with the other agent | They edit main.c, game.inc, space.inc, deep.inc, save.inc and `pc/*`. Start Phase 1 after they merge. `build.sh` is shared: add hooks in a separate block at the end. |
| Determinism leaks | No floats, no wall clock, no pointer-order, no uninitialised memory (`memset` all structs). Run each test twice and compare. |
| Fun risk: spreadsheet game | Keep the action game the star. Every colony screen has a "go do something" line. Colony output is capped (1.5). |
| Scope creep | Phases are ordered. Do not start Phase 6 before Phase 4 has been played. Play each phase before the next, as ROADMAP.md says. |
| Save bloat | Store diffs only. Cap the side file at 256 KB per slot; compact legends when over. |
| Unwinnable colony | Colonies go dormant, never die. A dormant plot can restart free. |
| Text overflow | `msg[40]`, `menu_msg[40]`, `ln[]` lines are about 52 letters at most. Test every template at its longest name. |

What NOT to do:

- Do not change `planet_frame()`, `dungeon_frame()` or `space_frame()` logic. Only add one-line hooks.
- Do not store the world. Store only what the player changed.
- Do not use the 32 KB SRAM for the side game.
- Do not make the side game required to finish the story.
- Do not build a new 3D renderer for the colony. Use the 2D map and stamped terrain.
- Do not add romance.

## 12. Inspiration: WorldBox-style sandbox (added by the user's request)
The user pointed at the WorldBox creatures wiki (it could not be fetched, so this uses general knowledge only; no WorldBox names or text are copied).
Take the ideas, not the content:
- **Many races, each different.** Settlers and neighbours come in many kinds (from the PC x16 'beings and peoples' catalogue). Each has lifespan, diet, temper, craft, beliefs, what it hunts and fears.
- **Tribes, villages and kingdoms grow by themselves** on a settled planet: they claim land, trade, quarrel, ally, go to war, split and fade. The player can watch, nudge or step in.
- **Gentle 'god powers' in the side game** (bless a field, calm a storm, call rain, send a spark of an idea, raise a hill, plant a forest, seed a species). Powers cost the colony's 'spirit' that grows with prosperity and with the player's action-game milestones.
- **Evolution and mutation:** creatures and crops drift over generations with planet traits (a FUNGUS FOREST breeds spore-singers); legends record it.
- **Predators and prey, herds and migrations** make the planet alive without the player touching it.
- **Always a sandbox exit:** a sandbox mode with all powers open (no unlock ladder) for play, kept apart from the ActRaiser-style campaign ladder.
- Phase mapping: races and tribes appear from Phase 3 (after the PC x16 catalogues land), powers and the sandbox mode in the replay layer (Phase 11); the event log (Phase 1) records the stories.

## 13. God powers and the level cap (user, 2026-11-01)

- The level cap of your character on PC is 64^256 (= 2^1536, about 10^461). Levels are a 1536-bit unsigned number on PC (`Lv`, saturating add and multiply, divide by small ints, compare, print in digits and in 64-based groups). The GBA game keeps its 64-bit saturating level. PC only.
- Every god power unlocks after 1,000 levels each: power number n (1-based) opens at level 1,000 x n. The first power is open at level 1,000, the second at 2,000, and so on. If the table has P powers the last opens at 1,000 x P. (Easy to change: one constant, `GOD_STEP_LV = 1000`.)
- Powers are the gentle list in section 12 (bless a field, calm a storm, call rain, spark an idea, raise a hill, plant a forest, seed a species, ...) and grow to as many as the creature, trait and theme catalogues allow. The side game's spirit cost still applies; the level only unlocks the power.
- The sandbox mode opens every power at once (kept apart from the campaign ladder).
- Every unlock shows a plain message and is saved in the `.sim` file.
