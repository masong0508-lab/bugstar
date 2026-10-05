# BORE (tech demo)

GBA voxel creature creator, the seed of a later life sim. Pseudo-3D isometric view, humanoid-friendly build space:
**6 wide × 4 long × 8 high** blocks. Parts: HEAD, TORSO, ARM, LEG, EYE, MOUTH, EAR, HAIR, each in 3 sizes (S/M/L = 1×/2×/3× blocks).

## Controls
| Key | Action |
|---|---|
| D-pad | move cursor (left/right = X, up/down = Z) |
| L / R | cursor down / up |
| A / B | place / erase part |
| Start | cycle size S, M, L |
| Select (tap) | next part |
| Select + L/R | previous / next part |
| Select + Left/Right | turn the view 90° (D-pad then moves relative to the screen) |
| Select + A / B | cycle skin / hair colour (recolours everything) |

Arm, leg, eye and ear are mirrored automatically across the body.

## Build
Push to GitHub: the **Build GBA ROM** workflow produces `bore.gba` as an artifact (tag a commit to get it as a release).
Locally with devkitARM installed: `make`. Run in mGBA or on hardware.

## Life stages (BABY, CHILD, TEEN, ADULT, ELDER)
The creature has an **AGE**: pick it on the BODY tab of the creator (first row), or, in the classic creator, on the AGE entry of the list (A or Left/Right). The creative space grows with the age (and an elder is a little stooped):
| Stage | Build box (W x D x H) | Biggest block | Looks on offer |
|---|---|---|---|
| BABY | 4 x 4 x 5 | M | BIG HEAD / STUBBY, no big ears, CROP or BALD, 4 colours per row |
| CHILD | 4 x 4 x 6 | M | no BROAD, no LONG hair, 6 colours |
| TEEN | 6 x 4 x 7 | L | everything but BROAD, 8 colours |
| ADULT | 6 x 4 x 8 | L | everything (old people and saves are adults) |
| ELDER | 6 x 4 x 7 | L | everything |
The picker rows skip what a stage cannot have, the block builder only places inside the stage's box (its grid shows the box) and greys out sizes that do not fit. A look-built creature is rebuilt for the new stage; hand-built blocks stay (and are cut to the box if you go to a smaller stage, after asking).

In the life: the **BABY cannot be steered**: it toddles about by itself and a caretaker keeps its needs up. Child, teen and elder walk slower than an adult; the career (shifts, quota, bills) is for TEEN and ADULT only (an elder is retired). At midnight the days in the stage count up and the creature grows to the next stage (a "NOW A CHILD" note, and its sprites are re-baked). ELDER is the last stage.

**OPTIONS > AGES** (its own page): **AGING** (OFF / SLOW / NORMAL / FAST: slow doubles the days of every stage, fast halves them, off keeps the age you picked) and how many game days each stage lasts: **BABY** (default 2), **CHILD** (3), **TEEN** (3), **ADULT** (7, or FOREVER to never grow old); each is 1, 2, 3, 5, 7, 10, 14, 21, 30 or 60 days. The stage is saved in the person (room slot format 2; format 1 slots load as adults) and also in SRAM at 12416 so growth survives a power cycle.

## Main menu, room builder, settings
Boot goes title -> **main menu** (PLAY, MAKE CREATURE, BUILD ROOM, JUKEBOX, SETTINGS, HOW TO PLAY). "MAIN MENU" is the last entry in the creature part list and in the pause menu.

**Build Room** tools (Select taps to the next tool): ROOM (A corner, A again = walls + floor + doorway, min 3x3), WALL (straight line), FLOOR (fill area), ITEM (single tiles), ERASE (clear area). L/R picks floor (or item); Select+L/R picks wallpaper. 14 wallpapers and 14 floors, 90s house (floral, peach stripe, Memphis, wood panel, gingham, teal carpet, checker lino...) and factory (corrugated, red brick, cinder block, hazard, steel plate, grate, oil-stained concrete...). Floors, wallpaper and tiles are saved to SRAM.

**Settings** (saved to SRAM; open from the main menu or the pause menu). Rows are colour-coded green (fast) / yellow (mid) / red (slow), and a live **DRAW COST** meter measures how long a frame really takes to draw on your device, with 60 / 30 / 20 marks showing which frame rate it can hold (it warns when your frame rate is too high).
- PRESET: LOOKS / BALANCED / SPEED / BATTERY (CUSTOM once you change anything)
- AUTO TUNE (press A): measures your screen and picks the prettiest preset that stays smooth
- FRAME RATE 60 / 30 / 20 / 15 (logic always runs at 60 steps/s, only the redraw rate drops)
- WALLS full / cutaway / low, WALLPAPER and FLOORS patterns or plain, SHADOWS on/off
- INFO ON SCREEN full / slim / off (alerts always show), SOUND on/off
- ROM SPEED fast / safe (safe = power-on waits, for fussy flash carts)
- PERFORMANCE INFO off / FPS / DETAIL (FPS + LOAD, 100 = a frame is just fitting)
- RESET ALL

## Options (the SETTINGS screen, rebuilt)
Main menu -> **OPTIONS** (also in the pause menu and the map menu). Eight pages: **VIDEO** (the old settings: preset, auto tune, frame rate, walls, wallpaper, floors, shadows, performance info, ROM speed), **PLAY**, **AGES**, **AUDIO**, **INPUT**, **HUD**, **ROOMS**, **DATA**.
| Key | Action |
|---|---|
| L / R | change page |
| Up / Down | pick a row |
| Left / Right (or A) | change it |
| Select | put the row back to its normal value |
| B / Start | back (everything is saved) |
A gold dot marks a row that is not at its normal value. The row under the cursor explains itself in two lines.
- **PLAY**: NEEDS (off to brutal), FOOD AND WC, DAY LENGTH (3 / 6 / 12 / 24 min or stopped), CAREER on/off (off = no shifts, quota or bills), JOB QUOTA, BILLS, SCORE multiplier (x0.5 to x3), COMBO WINDOW, TOP SPEED (80 to 150 %), MOOD EFFECTS, HURT (normal / gentle / no death), AUTO SAVE LIFE.
- **AUDIO**: SOUND, SFX VOLUME, MUSIC VOLUME, **GAME MUSIC** (off by default: the jukebox songs play in their shuffled order while you play, the next song starts when one ends, it fades to half volume while the pause menu (or anything opened from it) is up, and sound effects pause the music while they sound; mixing runs in an interrupt, so it costs some speed on slow devices), TITLE MUSIC, JUKEBOX MODE.
- **INPUT**: BUTTONS (swap A/B, L/R or both, on every screen), CURSOR REPEAT speed of the editor, BUTTON TEST (shows the keys the game sees).
- **HUD**: INFO ON SCREEN, CLOCK (24 h / 12 h / hidden), THOUGHT BUBBLE, WANTS AND FEARS, ACTION CAM, ACCENT COLOUR (gold, mint, sky, pink, orange, lilac), MESSAGE TIME.
- **ROOMS**: EDITOR MINIMAP, SAVE ON EXIT, ASK BEFORE RESET, SLOTS SAVE (room / room + person / all three), ASK IN SLOTS, SAVE MAP TO SLOT, BOOT LOADS PERSON.
- **DATA**: SAVE LIFE NOW, ERASE LIFE, ERASE SAVED MAP, ERASE ALL SLOTS, ERASE EVERYTHING (main menu only, asks twice), SAVE MEMORY TEST (checks the cart or emulator keeps saves), RESET ALL OPTIONS.

**Adding an option** takes four small steps (written at the top of `source/opts.h`): add a name at the *end* of the `XO_` enum, add its choice count and default to `xoCnt[]` / `xoDef[]`, read `xo[XO_X]` where the game uses it, and add an `XR(...)` row to a page table in `source/optscreen.h`. Options are one byte each, saved with a checksum and a range check per value, so an older save simply gets the defaults for options it does not have.

## Room slots
Main menu (or pause menu, or map menu) -> **ROOM SLOTS**. Six named saves; each holds any of a **room** (walls, floors, wallpaper, items), the **person** (the creature, including hand built blocks) and the **life** (needs, cash, job, clock, skill). What a save stores is the SLOTS SAVE option.
| Key | Action |
|---|---|
| Up / Down | pick a slot (the room is previewed on the right) |
| A | the slot's list: SAVE HERE / SAVE OVER IT, LOAD ALL, LOAD ROOM ONLY, LOAD PERSON ONLY, RENAME, COPY TO, INFO, DELETE |
| Select | help |
| B / Start | back |
The slot you saved to or loaded last is the **active slot**. With SAVE MAP TO SLOT on, the editor's SAVE MAP also writes it; with BOOT LOADS PERSON on (default) the person of the active slot comes back at power on. Loading is checked first (checksum, map size, every value) and only then applied, a save only touches its own slot, and a damaged slot is shown as DAMAGED instead of being loaded.

**Format** (all in `source/slots.h`, made so a whole house can be added later without breaking any save): a 32 byte header (magic, version, kind ROOM or HOUSE, span = how many consecutive slots it covers, what it holds, payload length and checksum, map size, save counter, name) and then a list of tagged chunks (`R` room as run-length packed tiles, `C` person, `L` life, `H` reserved for the house plan). Readers skip tags they do not know. A house is KIND 1 with a span of several slots; the slot screen already lists, protects and deletes those. To add houses: write `houseSave()` / `houseLoad()` and set `SLOT_HOUSE_READY` to 1.

## Save memory map (32 KB SRAM)
| Offset | What |
|---|---|
| 0 | the room, "BM3" (4803 bytes; older 14 x 14 saves still load) |
| 8192 | settings (16 bytes) |
| 8448 | extended options (`opts.h`) |
| 12288 | jukebox order and mode |
| 12352 | active room slot |
| 16384 | the life (`sims.h`) |
| 18432 | 16 spare bytes for SAVE MEMORY TEST |
| 20480 | six room slots of 2048 bytes (to the end of SRAM) |

## Combos and the action cam
Clean tricks (spins, kickflips) and rail grinds now **chain**: each one adds to the chain and the chain multiplier equals the number of tricks. Land the next trick within 2.5 s (grinding keeps it alive) or the chain banks its bonus (points x (tricks - 1)). A bail or a hit loses the chain. The HUD shows `COMBO X5 2500` while it runs.
When a chain banks for more than the **ACTION CAM** threshold (Settings: OFF / over 10000 / over 5000 / over 2000, default 10000) the game holds still while the camera zooms in 1.3x on the skater and swings through all 4 sides of the room, then eases back. SELECT skips it. For reference, 14 kickflips in a row banks 19600.

## Big map, camera and minimap
The play map is now **40 x 40 tiles** (was 14 x 14), about 8x the floor space. The default map has a house (carpet, lino kitchen, pink-tile bathroom), a brick-and-steel factory with hazard lanes, and a rail park with long rails and crate boxes, joined by concrete roads and ringed by a low wall. The middle of the rail park is an open plaza.
- **Camera**: in play the view follows the skater, eased so it stays steady, and stops at the map edges. The action cam still spins round the skater.
- **Speed**: only the tiles on screen are drawn, so the bigger map costs far less than drawing all 1600 tiles. Use SETTINGS (AUTO TUNE) if your device needs it.
- **Map editor**: a dead-zone camera scrolls only when the cursor nears the edge of the screen, and a **minimap** (top right) shows the whole map, the area on screen and the blinking cursor.
- **Saves**: new maps save in a bigger SRAM block (settings moved to offset 8192). Older 14 x 14 saves and settings still load; an old room is placed into the plaza of the new map and re-saves in the new format.

## Skate objects and ramps
New skate-park pieces, all in the room editor's ITEM tool (L/R to pick, **Select+A turns a ramp** to face S / E / N / W):
| Item | Height | Notes |
|---|---|---|
| RAIL | 6 px | grind (links up with neighbouring rails) |
| LEDGE | 6 px | concrete box with steel edges, grind it; runs along its neighbours like a rail |
| BENCH | 6 px | slatted seat on legs, grind it |
| KICKER | 0 to 8 px | wedge ramp; the lip is on the side opposite the direction it faces |
| Q PIPE | 0 to 14 px | quarter pipe, steel coping on the lip |
Ramps have a real slope: ride up a kicker or quarter pipe on the board and you are launched off the lip, harder the faster you were going. Walking up one just works. Tuning is at the top of `source/ramps.h` (`F_RAMP_BOOST`, `F_RAMP_MAX`, `F_RAMP_TOL`). Saved maps keep working; to see the new pieces in the default park, reset the map (the default plaza now has two kickers facing each other, two quarter pipes under the wall, a ledge and a bench).

The sprite art is authored in `tools/make_skate_items.py`, which writes `source/skateart.h` (sprites), `source/rampdata.h` (matching physics heights) and a preview sheet `assets/preview/skate_items.png`. Edit the art there and re-run `python3 tools/make_skate_items.py`.

## Skate pack 2 (more objects)
Eight more pieces in the room editor's ITEM tool (the palette bar now scrolls, 25 entries). **Select+A turns the LAUNCH ramp** like the kicker. Tuning: `LAUNCH_H` in `source/rampdata.h`, the heights in `tileH()` in `main.c`.
| Item | Tile | Height | Notes |
|---|---|---|---|
| LAUNCH | `9 : ; <` (S E N W) | 0 to 12 px | taller blue wedge; launches harder than a kicker (capped by `F_RAMP_MAX`) |
| FUNBOX | `X` | 10 px | solid platform: ollie onto it, land on top |
| BARREL | `O` | 8 px | oil drum, solid (bumping at speed hurts) |
| TRASH CAN | `Y` | 10 px | solid |
| PLANTER | `Z` | 6 px | brick box, **grinds** |
| PICNIC | `K` | 6 px | table top, **grinds** |
| JERSEY | `J` | 6 px | concrete barrier with red/white top, **grinds**, links up with neighbours like the ledge |
| MANUAL PAD | `M` | 3 px | low painted pad, rolls straight on with no jump (the spot for a future manual mechanic) |
The default plaza now has a funbox with two launch ramps in line with the rail above it, barrels, jersey barriers, planters, a picnic table, trash cans and a manual pad (they only go on empty floor). A map you saved earlier keeps its layout: place them yourself or RESET MAP. Art is in `tools/make_skate_items.py` (pack 2 section), preview sheet `assets/preview/skate_items2.png`.

## Mood meters (FUN and HAPPY)
Two new HUD bars under FOOD and WC, plus a face and a mood word (SAD, BORED, OK, HAPPY, STOKED). All the logic is in `source/mood.h`, with every tuning number in the MOOD block at the top.
- **FUN** is fast: tricks, combos, grinds, air time and ramp launches fill it; it drains by itself, and twice as fast after 15 s of nothing fun (that is the BORE in BORE). Cruising on the board only slows the drain.
- **HAPPY** is slow: it drifts toward a target made of comfort (fed, bladder ok) and fun, falls faster than it rises, and takes instant knocks from bails, hurts, accidents and fainting.
- **Effects so far:** SAD cuts top speed by 15%, STOKED adds 6%; trick points are +25% when STOKED and -25% when BORED. The skater announces it when they slip into SAD, BORED or STOKED.
- **Adding a mechanic:** add a name to `MoodEv` and a row to `moodTab`, call `moodEvent(M_X)` where it happens; per-step things go in `moodTick()`; things the mood changes go in the effect functions (`moodTop`, `moodPts`) at the bottom, or read `moodState()`.
Meters are not saved to SRAM yet.

## Title music
The title screen plays "The Dipper Man" (tools/the_dipper_man.xm). `python3 tools/xm2gba.py` converts the XM (and every other `SONG_XM` song listed in `source/songs.h`) to `source/musicdata.h`: note events per pattern (with per-note volume) plus the instrument samples that are actually used (8-bit, band-limited, down-sampled to the lowest rate that keeps them clean; the title song is about 70 KB in the ROM). A 10-voice mixer with linear interpolation plays it through Direct Sound B at 18157 Hz (exactly 304 samples per frame). The 7.7 s intro plays once, then the song loops from order 4; voices are never cut at the jump, so the last notes ring into the first ones. Music stops when you press START.

**Meltdown in Mars (90s house mix)**: `tools/make_meltdown_house.py` builds `tools/meltdown_in_mars_house.xm` (126 BPM, about 6 minutes, 10 channels, all sounds synthesised), it is listed in `source/songs.h`, and `python3 tools/xm2gba.py` bakes it into `source/musicdata.h`. `python3 tools/preview_xm.py meltdown_in_mars_house tools/meltdown_in_mars_house.xm out.wav` renders it the way the GBA mixer will play it.

## Jukebox
Main menu -> **JUKEBOX**. Opening it starts the song the playlist is on. The song list lives in `source/songs.h`.
| Key | Action |
|---|---|
| Up / Down | move the cursor through the playlist |
| A | play the song under the cursor |
| L / R | previous / next song |
| Start | stop, or play the song under the cursor |
| Left / Right | mode: SHUFFLE, IN ORDER, REPEAT ONE |
| Select | re-roll the shuffle (saved; the playing song stays first) |
| B | back to the menu |

When a song ends the next one starts (REPEAT ONE replays it). **The shuffled order is saved in SRAM (offset 12288), so the song set comes back in the same shuffled order every time the game starts**, and the playlist carries on from the last song played. It is re-rolled only by SELECT, or automatically when the number of songs in `songs.h` changes. The mode is a normal setting (SETTINGS -> JUKEBOX).

**Adding a tracker song (.xm):** copy it into `tools/`, add `SONG_XM(my_id,"MY SONG","tools/my_song.xm")` to `source/songs.h`, then run `python3 tools/xm2gba.py` (needs numpy + scipy) and commit the new `source/musicdata.h`. Tracker songs are tiny (tens of KB). The player handles up to 10 channels, 32 instruments, any pattern length, notes and the volume column; it ignores effects, panning, envelopes and note-off (the script warns if a song uses them). Speed/BPM must stay fixed in the song. `GAIN` in `tools/xm2gba.py` sets a song's loudness.

**Adding streamed songs:** `python3 tools/encode_song.py "my song.mp3"` (needs ffmpeg + numpy). It writes `source/music/<id>.adp` and adds a `SONG_ADP(...)` line to `source/songs.h`. Song spec: 4-bit IMA-ADPCM, mono, 18157 Hz, about 9 KB per second, up to 32 songs. Song titles use capitals, digits and spaces (the font has no punctuation). The three `PLACEHOLDER` songs and the tracker songs (`SONG_XM`) are there so shuffle can be heard from day one: delete the placeholder lines in `songs.h` and the files in `source/music/` when you add real songs.
The jukebox plays only on its own screen for now: music during gameplay needs a vblank interrupt (game frames can run longer than a sound buffer).


## Creature creator (rebuilt)
The creature screen is now a character-creator: a live preview in a little house room (the game's own wallpaper and floor) on the left, a card of numbered tabs on the right.
| Key | Action |
|---|---|
| L / R | change tab (1 BODY, 2 FACE, 3 HAIR, 4 CLOTHES, 5 BUILD, tick = DONE) |
| Up / Down | pick a row |
| Left / Right (or A) | change it: named options (shape, eyes, mouth, ears, hair style) or 8 colour swatches (skin, hair, top, bottom) |
| Select | turn the creature (compass bottom left) |
| Start | jump to DONE (GO LIVE LIFE, EDIT MAP, MAIN MENU) |
| B | back to the main menu |

Tab 5 opens the original block builder (legend with key caps; Select+Start returns). Changing shape, ears or hair style after hand-building asks before replacing your blocks.

## Life sim layer (Sims 2, handheld edition)
The life part of the game has a Sims 2 style loop on top of the skating. All the logic is in `source/sims.h` (art in `source/simart.h`, preview in `assets/preview/sims_furniture.png`); every tuning number is in the TUNING block at the top of `sims.h`. Everything except other people (social needs, relationships) is in.

**Furniture** (room editor, ITEM tool, 3 slots at the end of the list so old saves still load): **BED** (S), **SHOWER** (H), **SOFA** (C). They face away from the wall like the fridge. Stand next to one and press **R**; **A, B or R** gets you up early. The default house now has a shower in the bathroom, and a bed and sofa in the lounge. (A map you saved earlier keeps its old layout: place them yourself, or RESET MAP.)

**Needs.** FOOD and WC were already there. REST, CLEAN, COMFY and ROOM join them in the right-hand column. They drain on their own (REST empties in about 4 minutes awake, faster at night). Bed refills REST (~10 s), shower CLEAN (~4 s), sofa COMFY (~6 s). **ROOM** is the look of the place you stand in: it rises near furniture (fridge, toilet, bed, shower, sofa; five different kinds in 5 tiles = 100) and sags slowly in an empty place. At 0 REST the skater **passes out** (blackout, REST back to 25, a mood knock); below 20 REST top speed drops 20%. All needs feed the HAPPY meter.

**Clock.** A game day is 6 real minutes (`SIM_STEPS_MIN`), a week is MON..SUN, the game starts MON 08:00. The clock is top right and turns gold during your shift. Sleeping runs the clock fast (one game minute per step); between 22:00 and 06:00 sleep restores 25% faster and staying awake costs REST 50% faster.

**Career: pro skater**, Mon to Fri 09:00 to 17:00. Trick points you score during the shift count towards the day's quota (600 at level 0, +500 per level). At 17:00 you are paid: full quota = full pay (70, +40 per level, +30 for double the quota) and a step to promotion; half quota = half pay; less = nothing and a strike. **3 good days = promotion, 3 strikes = demotion.** Levels: NEWBIE, AMATEUR, SPONSORED, PRO, TEAM RIDER, LEGEND. A reminder shows at 08:00 and when the shift starts. **Bills** of 40 are taken every midnight; if you cannot pay you are BROKE (cash to 0, a mood knock, and it ticks the BEING BROKE fear). You start with 200.

**Skill.** SKATING skill 0 to 5 is trained by tricks, combos and grinds (12 / 35 / 70 / 120 / 200 points). Each level adds 8% to trick points. It shows as SK on the job line.

**Thought bubble.** A thought bubble over the head shows the most urgent need (WC, EAT, ZZZ, STINKY, SIT), otherwise it alternates between your wants.

**Wants and fears.** Two wants and one fear are always on show under the needs (green and red markers). Wants: snack, WC, nap, get clean, sofa, land a trick, trick combo, grind, get air, feel stoked, finish a shift, get promoted, learn a skill, nice room. Fears: bailing, accident, passing out, getting hurt, fainting, being broke, demotion. Meeting a want pays aspiration points and a mood lift; a fear coming true costs points. A want is only offered if the map has what it needs (no bed, no nap want). All of it hooks into the mood events, so every `moodEvent()` in the game feeds it.

**Aspiration.** Points climb through BRONZE, SILVER, GOLD and PLATINUM (40 / 120 / 260 / 450). Each level slows the needs down; PLATINUM halves them.

**Saving.** Needs, cash, aspiration, clock, job level and progress, and skill are saved to SRAM (offset 16384) at every midnight, every payday, when you open the pause menu and when you leave the life game, with a checksum so a bad save is ignored. Dying only resets the needs: the life goes on. **Pause menu -> NEW LIFE** erases it and starts over.

**Adding things.** A want or fear: add a `SE_` name, a row in `simWants` / `simFears`, and map the game event in `simsMood()` (or call `simEvent(SE_X)` yourself). A need: a variable (and in `simsSave`/`simsLoad`), a rate in `simsTick()`, a use in `simBegin()`, a bar in `simsHud()`. A piece of furniture: art in `simart.h`, a slot in `palCh`/`palNm`/`palCol` and a `drawItemTile` line.

**Not done yet:** SOCIAL need, other people to talk to, relationships.
