# BUGSTAR

A Game Boy Advance action RPG: a pill bug on turning planets, textured real-time 3D dungeons under each one, a pet that fights beside you, and a galaxy that never ends.

**Play:** open `bugstar.gba` in mGBA (or any GBA emulator, or a flash cart). It saves on the cartridge (3 save slots).

## Controls
- On a planet, **UP walks into the screen** (away from the camera) in both third and first person.
| Button | On a planet | In a dungeon |
| --- | --- | --- |
| D-pad | Walk over the hills (in first person: up/down walk, left/right turn) | Up/down walk, left/right turn (a tap turns a little, holding turns faster). **Double-tap UP: auto-run** (steer with LEFT/RIGHT; DOWN, a wall, a monster or a hit stops it) |
| A | Door, ship, cage, slug (the gold line at the top says what A will do) | Bite (hold A to keep biting). With nothing to bite: open cages, stairs, fish at water |
| B | Use the picked item | Use the picked item |
| L / R | Pick an item in your bag | Strafe left / right |
| SELECT | Tap: switch between third person and first person. Hold SELECT + L/R: point the arrow at the door or your ship. Hold SELECT + LEFT/RIGHT (third person): turn the camera around your bug | Tap: cast your spell. Hold: up/down picks a bag item, L/R changes spell |
| START | Pause screen (BUG, GEAR, MAGIC, QUEST, WORLD, FEATS, EXTRAS, MAP, SYSTEM). EXTRAS lists everything you can unlock: UNLOCKED or LOCKED, and what you need | Opens on the MAP |

## Debug and cheat menu
Type the Konami code (UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A) on the **title screen**, or on the **SYSTEM page of the pause screen** in a game (after that, SELECT on the SYSTEM page opens it).
Pages (L/R): **GAME** (new/continue unlocked, save now, difficulty), **CHEAT** (god mode, infinite mana/gold/fuel, never hungry, one-hit kills, no-clip, move speed, level, add gold/XP, max stats), **ITEMS** (give gear by type/rarity/level, all 12 uniques, potions and gems, spells, friends, relics, tools, cores, ore), **WORLD** (hour, weather, world type, weird traits, kill monsters, reveal the map, teleport to the stairs), **SHIP** (fuel, repair, every shipyard level), **GO** (jump to a floor, a boss, space, a fleet battle, the beast...), **INFO** (position/seed overlay, FPS, hide HUD, state report), **TOOLS** (makers, music, sound test).
Rows that do not fit where you are are hidden or greyed with the reason. Hold LEFT/RIGHT on a number to go fast.

## The story
Your little droid **Bolt** rolls after you everywhere. He does not know what he is. He knows he is yours.
Long ago a seed ship, the **Star Cruiser DAWN**, planted your worlds: the Garden, the Pond, the Night Lawn. Its crew were bugs. Its mind was Bolt. It fell on the Anthill. The Ant King opened its heart and built a mouth around it: the **Technobeast**, which eats ships to grow. The ants broke Bolt's memory into eight **data cores** and used them to power their drone outposts.
Every outpost core you smash gives Bolt a memory back. The last one is his real name - the word that opens the beast's heart. Then fly into the beast, shoot its core, and let him say it.
- Bolt speaks after every boss, after every outpost, and whenever you pick **TALK TO BOLT** at your ship (he always tells you what to do next, and worries aloud if you are hurt, hungry, low on fuel or hull, or your hold is full; he has 64 idle remarks that change every visit, plus remarks for the world you are on: its story zone or planet class, its weird traits, its weather, hour, gravity, size and mood). His arrival remarks on each world now come in two flavours, plus special ones for worlds that love or hate you.
- **Drone outposts** are grey hatches with a red light on planets (zones 2-6 and some uncharted worlds; red marks on the galaxy map). Inside: tech dungeons with a **locked door** - find the **keycard** the drones keep somewhere on the floor - and a **power core** at the bottom to bite to pieces. Each dead outpost also makes the Technobeast smaller.
- With all 8 cores, hitting the beast's core does **8x damage**: Bolt opens its heart. Beat it and the Dawn flies again.
- **Station lounge** (a third tab at any station): three regulars - an old moth pilot, a trader bee, and an ant who ran away - with rumours, trade tips and where the drones are digging.

## Your ship (new)
**The ship menu** has four tabs (L/R or LEFT/RIGHT): **GO** (launch or autopilot, kart club, dungeon forge), **SHOP** (supplies, gear, sell, quest board), **CARE** (heal, identify, workbench, trophy case) and **YOU** (mirror, shape, hangar, Bolt). A line under the list says what the highlighted row does, rows show their price or state (FULL, LOCKED, NEW, ! DONE), and **START takes off** from any tab.
**Build your ship** after making your bug, or any time at your ship: **HANGAR**. It is a real 3D wireframe that turns in the preview (L/R spin it), and it is parked on every planet in 3D.
- **Hull:** dart, saucer, cargo box, wedge or needle - sets your armour and hold size.
- **Wings:** none, straight, swept, delta or X-wings, in three sizes - sets how fast you turn.
- **Engines** one to four: top speed. **Fins:** how quickly turns settle. **Cannon:** none, nose gun, wing guns or big cannon: laser damage. **Cockpit** bubble or slit.
- **Paint** and **trim** colours use the same pickers as your bug. SELECT: surprise me.
The stats card at the bottom shows exactly what each choice does.

## Your bug (big update)
**Make your bug** when you start a new game, or any time at your ship's **MIRROR**. It is built from parts, like a creature creator, and you watch it change as you go:
- **Name** (up to 8 letters), and **18 ready-made looks** to start from (ladybug, wasp, stag, firefly, butterfly, unicorn, knight, pirate, royal, chef, demon, ghost, robot, viking and more).
- **Shell:** short, normal or long. **Shape:** round, dome, flat, spiky or humped. **Finish:** shiny, matte, metal or pearl.
- **12 patterns:** plain, striped, spotted, banded, checked, zigzag, dotted, diamonds, tiger, halves, speckled and rings.
- **8 eyes** (googly, sleepy, angry, starry, one big eye, shades...), **6 mouths** (smile, jaws, pincers, fangs, tongue), **8 antennae** (feathery, clubbed, zigzag, long whips...).
- **Legs:** four, six or eight, thin, chunky, hairy or in boots. **Horns:** horn, twin horns, rhino, unicorn or antlers.
- **Wings:** little, beetle, butterfly, bat or fairy. **Tails:** stinger, spikes, curly or long.
- **12 hats:** cap, top hat, bow, crown, party hat, flower, halo, beanie, chef, helmet and viking.
- **10 colours**, including wings, hat and the outline. Every colour has a **simple picker** (16 presets) and a **mix-your-own** mode.
- **L** locks a row, **R** builds matching colours, **SELECT** surprises you, **B** undoes.
The bug's sprite is drawn by the game from your choices, so every bug is different.

**Humanoids** have their own creator too: body (human or furry), height, build, **10 hair styles** (or 10 kinds of ears), 8 faces (glasses, shades, eyepatch...), beards and moustaches, **10 tops** (t-shirt, hoodie, robe, dress, armour...), 6 bottoms, 5 kinds of shoes, **12 hats** (wizard, cowboy, crown, halo...), and something on your back (cape, wings, backpack, scarf or jetpack). 16 ready-made looks.

## Even more planet monsters (new)
50 more creatures, so there are now 66 planet monsters, and every kind of world has 8 of its own (it was 2). About half the monsters in a dungeon now belong to that planet.
- **Green worlds:** earwig, aphid packs, dragonfly, stinkbug, flatworm, hornet, weevil, hopper, monarch, tick, pixie, puffball.
- **Water worlds:** crayfish, piranha packs, eel, urchin, squid, mosquito swarms, newt, angler.
- **Dry and rocky worlds:** antlion, scarab, solifuge, geode, gecko. **Ice:** iceworm, yeti, snowball, icicle, penguins.
- **Jungle:** tarantula, tree frog, strangler vine, pitcher plant, bullet ants. **Death and bone:** zombug, banshee, mummy, crows, graveworm.
- **City:** roaches, turret, rats, scrapbot, pigeons. **Fire:** lava worm, cinders, salamander, hellhounds, pyre.
- Two new tricks: **BOOM** monsters (stinkbug, snowball, scrapbot, cinder) blow up when they die, so stand back. **SPLIT** monsters (flatworm, puffball, geode, zombug) break into two smaller ones.

## Planet monsters (new)
Every planet now has its own creatures down in the dungeon (about 3 in 10 monsters are local ones; deeper floors bring the nastier ones). They fight in different ways:
- **Garden:** SNAIL (slow, tough), BUMBLEBEE (hovers and flings stingers). **Pond / ocean worlds:** JELLYFISH (drifting slow shocks), LEECH (heals when it bites). **Picnic:** bumblebees and ACIDLINGS (spit acid). **Night Lawn:** GLOWFLY swarms and WRAITHS. **Beehive:** bumblebees and MANTISES (lunge). **Anthill:** SANDWORMS (charge) and ROCKBACKS (armoured: half damage).
- **Galaxy worlds:** temperate/meadow/paradise: snails, bumblebees, glowflies. Mountain: rockbacks and SHARDS (floating crystals, fast needles). Desert: sandworms. Ice: FROSTMITE packs and wraiths. Jungle: mantises. Death planet: REAPERS (steal life) and SKELETONS (get up once). City planet: SENTRIES (never move, shoot fast). Hell worlds: fire imps, acidlings, shards, skeletons, reapers, leeches, wraiths. Moons count as rock, dust or ice. Drone outposts also have sentries.
- Fixed: the ant now has its body colour.

## Baby mode and more options (new)
The options screen scrolls and has five new rows. **BABY MODE** (for babies, really): you take 1 damage at most, you never get hungry, fainting costs nothing, Buddy heals your boo-boos, and gold and XP are doubled. **HUNGER** on/off, **LOSE STUFF WHEN YOU FAINT** on/off, **XP SPEED** and **GOLD** x1 / x2 / x5. All remembered on the cartridge.

## Options (new)
Difficulty (easy / normal / hard: hard does more damage to you but pays +25% gold), which planet camera a new game starts with, music, sound effects, autosave, screen shake, minimap, damage numbers, button hints, and the guide arrow. All remembered on the cartridge.

## How to play (redone)
Five pages with colour-coded button badges: planet, dungeon, space, staying alive, and your quest. Your bug trots along the bottom.

## Landable asteroids (Phase 2, first step)
Every star's rock belt is now a place you can land. Fly close to a rock (the prompt reads **A: LAND ON <KIND>**) and press A. A rock is a tiny, almost zero-gravity world with its own veins; take off and you are back in space. Slot i of a star's belt is always the same rock, so a rock keeps its kind and its veins every visit.
- **ROCKY** (orange): iron and copper veins, the easy start.
- **METALLIC** (steel): dense. Iron, copper, scrap alloy and rare voidsteel; every vein needs one tool tier more and takes half again as many hits; a little more ore.
- **ICY** (white): ice quartz and calcite. The ice **melts**: about two and a half minutes after you land it lifts you off by itself (warnings before).
- **CRYSTAL** (cyan): ice quartz, sulfur, obsidian, calcite, voidsteel; far more ore per vein, but it **cracks** and hurts about one vein in five.
- **HOLLOW** (dark blue): rock and ice ore with a dungeon door; the mine inside comes with Phase 3.
Belt rocks are all vein (about 85% of crystal rocks), have a moon-style dungeon, and are not offered in story systems or hive systems.
Also fixed: voidsteel (the rare tier-6 ore) could never be found on any world; now it turns up in about one deep vein in eight.

## Ship parts, SCAN, spin-matching and the ore map (new)
- **Ship parts.** At the smelter, SHIP PARTS makes a **hull plate** (up to 5, 3% less damage each), an **engine tune** (more thrust), a **laser lens** (more laser damage) and a **shield cell** (+10 shield); up to 3 of each. Ion cells, lenses and bone plates are the parts they use.
- **SCAN.** Hold the SCANNER tool, then SELECT + L/R: the arrow has a fourth stop that points at the richest ore vein of the world and names its ore.
- **Spin-matching.** A fast tumbling belt rock cannot be landed on at speed. Fly beside it and hold R: the retro thrusters match its drift instead of stopping you. The prompt shows your relative speed; land when A: LAND appears.
- **Ore map.** With MINIMAP on, a small map on the left of the planet view shows nearby ore veins, coloured by ore (iron grey, copper red, ice quartz cyan, sulfur gold, obsidian dark red, calcite white, scrap alloy green, voidsteel purple). The scanned vein blinks.

## Hell worlds (new)
Out in the galaxy, 8% of worlds near home (and up to 24% far out) are **hell worlds**, in six flavours: **INFERNO** (lava canyons, volcanoes with lava lakes, falling embers), **BRIMSTONE** (yellow mesas, acid pools, bad air), **OBSIDIAN** (black glass needles with neon cracks, a black sun), **BONE WORLD** (rib dunes under a skull moon), **BLOOD SEA** (red islands in a churning red sea) and **FROZEN HELL** (cracking ice, aurora). Each has its own ground, sea, sky and hazards: lava, acid and the red sea hurt if you wade in, glass cuts, fumes and cold wear you down.
- **Hell tiers T1-T10** go up 1 for every 4 danger you fly out, just like the zone tiers. The tier is on the landing card and the top bar (H4...). Each tier multiplies the pain: creatures bite x2 (T1-3), x3 (T4-7), x4 (T8-10); the dungeon underneath counts as 2-7 levels deeper, drops more gold (+15% per tier) and mining gives more ore.

## Moon dungeons (new)
Every moon has its own kind of dungeon: **lunar caverns** (rock and dust moons) or **ice catacombs** (ice moons), with glittering walls, swarms of wasps, dark floors that pay double loot, one extra gem on every floor, and **low gravity** (+15% speed). The depth is split into **moon tiers**: a new tier every 5 floors (the top bar shows T1, T2...). Each tier gives monsters +10% health. A **moon guardian** waits on every 5th floor: +25% health per tier, a better chance of a unique drop, and 200 gold x the tier when it falls.

## Living planets: cities, creatures, jobs (new)
- **Every planet is grown from its own DNA**: the shape of the land (rolling hills, mesas, dunes, islands, craters, canyons, ridges or plains), cliffs, beaches, snow caps, the sea level, the time of day (day, dusk, night), the weather (clear, rain, snow, dust), gravity (it changes how fast you walk), the sky and ground colours, what grows there, its two creature species, and what it is **known for** ("ITS SOUP", "A MAYOR WHO IS A MOTH", "RAIN THAT FALLS UP"...). The parts combine into about **10^98 different planets**, far past an unvigintillion (10^66).
- **The universe.** A universe is 2^155 × 2^155 star clusters (it wraps round at the edges), with 32 stars in each cluster and about 5.6 planets and moons for every star. It is about 1.1 quindecillion light years wide. On the galaxy map: **START** jumps to the next cluster or warps far away (**R**), and after the story it also jumps to **another universe**: there are 4,294,967,296 of them (see *Universe jumps*).
- **Weird worlds.** One planet in 111 is weird. There are **1000 weird traits**: 40 kinds of weirdness (glitch, rainbow, tiny planet, spikes, giants, zero-g, metal ground, void, meteors, mirror sky, disco, wobble, shrink ray, jungle, shy life, angry life, sticky ground, ice, singing rocks, suns, rings, stripes, checkers, fog, ghosts, slug swarm, heat, frost, flood, drought, negative, old photo, moons, aurora, luck, healing air, bad air, magnet, shy ore, terraces) at 25 strengths each, from FAINT to FORBIDDEN. A weird world stacks 1 to 4 traits ("INSANE SUNS + INSANE RINGS"). They change the land, the sky, the colours, the creatures and how you move. Every weird world hides a **relic shrine**.
- **Marks.** A weird world can change *you*: SWIFT, TOUGH, LUCKY, STRONG, SHARP, REGROWING, WELL FED, THICK SHELL (good), or SLUGGISH, FRAIL, UNLUCKY, WEAK, HUNGRY, DULL (bad). Most are good. Most last a few minutes; a few stay **forever**. Stronger traits are more likely to leave a forever mark.
- **Planet classes.** Every world has a class that ties it together: TEMPERATE, MOUNTAIN (tall ridges, snow), MEADOW (smooth plains), OCEAN (islands in a deep sea), DESERT (dunes, no water), ICE (snow and frozen seas), JUNGLE (dense plants), DEATH PLANET (craters, lava seas, a dark red sky, everything bites twice as hard), CITY PLANET (rare: one city covers the whole world) and PARADISE.
- **Moods.** The locals adore you, don't care, or hate you. It changes what they say, their gifts, the prices they pay, and whether creatures bite.
- **Scan.** Every landing card shows the planet's gravity, temperature and day length.
- **Legendary relics.** There are only 16 in the whole universe ("THE COSMIC SPOON", "A STAR IN A JAR", "THE LAST SOCK"...). Each one is worth 1000+ gold and 100 fame, and gives **+5% gold and +2 max health forever**. Find them at relic shrines, or (very rarely) when you mine, dig fossils, beat beetles or solve a ruin's riddle. Weird worlds are twelve times luckier. Your count is on the FEATS page.
- **Cities.** About one world in three has a town or a big city, built right into the 3D land: streets, plazas, blocks and towers with lit windows (they glow at night). Walk the streets and press A at the doors:
  - **THE MARKET** buys your hold's goods and sells local goods to trade elsewhere.
  - **THE CANTINA**: soup, gossip (a rumour you have not heard yet, a hint for what you need right now, and how far and which way the nearest ruin, fossils, shrine, office, race post or market is), buy a round for fame, or arm-wrestle the bartender (he is a beetle).
  - **THE BOUNTY OFFICE** gives a job for that planet: squash slugs, beat beetles, dig fossils, win the race, meet the locals, hunt a creature or mine ore. It pays the moment you finish.
  - **THE RACE POST** starts a flag race over the hills. Beat the par time for gold and fame.
  - **Houses**: knock and see who answers.
  - **Locals** walk the streets. Talk to them. Some give gifts. Most say odd things.
- **Creatures.** Two species live on each world, each with a body (hopper, grazer or flyer), a size, a colour and a temper: **timid** ones run, **curious** ones come to look, **hostile** ones bite. They live in herds, wander, and breed back slowly if you leave some. Press A to catch one (food).
- **Fossils** to dig up (relics for your hold), **surface beetles** that charge you, and old **ruins** that ask a riddle (right answer: gold and a relic; wrong answer: bonk).
- **The arrival card**: when you land you get the planet's name, what it is known for, its land and weather, who lives there, and a remark from Bolt.
- **Boot logo**: the DIPPINN PRODUCTIONS logo from BORE plays at power on (A or START skips it).

## Shapes: creature or humanoid (new)
- A weird world can **reshape you** into one of its creatures (HOPPER: fast, GRAZER: tough, FLYER: fastest and floats over water) or into a **HUMANOID**. Most shapes last a few minutes; rarely one stays.
- Once you have been a shape, the **YOUR SHAPE** option on your ship switches between the shapes you have unlocked.
- A humanoid **holds tools**: PICKAXE (mine in one hit, more ore), SWORD (+6 damage), BLASTER (+4 damage, +10% crit), LANTERN (lights dungeons), JETPACK (+30% speed), SHIELD (+4 defence), SCANNER (relics easier to find), GOLDEN GLOVE (+20% gold). Buy them at city markets or get them from friendly locals. **SELECT + UP/DOWN** switches what you hold.

## Fleet battles (new)
- **Hire up to 10 wingmen** at any station's shipyard. They fly in formation, follow you between systems, defend you, and call out their kills.
- **Fleet battles** of up to **50 enemy ships** break out past the Ant King, or pick **JOIN A FLEET BATTLE** at a shipyard. Ships pick targets and shoot each other. Win the battle for a big bonus.

## More quests, a portal spell, and more saving (new)
- **Quest board** (at your ship) now offers eight kinds: slay monsters, slay a champion, find the lost treasure, free caged bugs, plus **SLAY THE BOSS** (the zone boss on its last floor), **FIND STAR SHARDS** (one glows on each of the last floors before the quest depth), **BRING** fish, berries, seeds or potions, and **BRING** trade goods (food, silk, amber, crystals...). Bring quests finish the moment you carry enough; the board takes the items when you claim.
- **Bounty office** has a new job: **SEARCH PLANTS** (press A on plants).
- **PORTAL spell** (12 MP, everyone knows it): cast it in a dungeon (SELECT) to pop up on the surface. The planet's door then reads "A: PORTAL BACK TO B#" and drops you on the same floor, so you can sell, heal and come back. Warp crystals still work the same way.
- **Saves:** the game now saves at every new dungeon floor (a checkpoint: loading wakes you on the surface with a portal open to that floor), when you go down the door, when a quest is taken or claimed, when a bounty job pays, and every few minutes on the surface. Turn AUTOSAVE off in OPTIONS to stop all of these.

## Roomier ruins (new)
Dungeons are now carved-stone ruins rather than red brick: **fewer, much bigger chambers** (the deeper you go, the bigger and fewer), **three-wide passages**, airy colonnades, a **still pool** in the middle of big halls, broken columns, overgrown and cracked floors, and **vines and moss** on the walls that thicken with depth. A little ambient light means it is gloomy, not pitch black. Bigger rooms hold more monsters. The 3D view is unchanged, so it runs the same speed.

## Labyrinths (new)
Every so often (about one dungeon floor in four, never a boss floor) you land in a **labyrinth**: a real maze of one-wide, torch-lit passages with dead ends, a few loops, five open chambers (one in each corner and a monument in the middle) and chests and wanderers hiding in the corridors. The stairs are in opposite corners. The message says "A LABYRINTH!" when you arrive, and the map (START) shows where you have been.

## Stairs, shortcuts and remembered floors
- **Floors are remembered for the whole trip.** Go up or down and the same floor comes back: same rooms, same stairs, the monsters you killed still dead, the loot you left still on the floor, the map you drew, the holes you mined. (Monsters that are alive heal and stand where they started.) It needs no save space: a floor is just its seed plus a few small changes, kept in scratch memory that was idle anyway. A trip ends when you leave the dungeon, and loading a save starts a new one.
- **Shortcut stairs** (about 1 floor in 3, from B2 on): a second way down that drops you two floors. It is a **gold square** with three down-arrows on the floor, and gold on the map.
- **The second stair up** is the pair of a shortcut: when you go down a shortcut you step out of a **green square** with three up-arrows, and that stair climbs two floors back to the shortcut you came from. Some floors have one on their own too (about 1 in 12).
- Coming back up by the ordinary stairs puts you on that floor's stairs down. The stairs up on B2 and deeper open a picker: any floor you have been to.

## Kart racing (big update)
- Press A at a **RACE POST** in a town, or pick **KART CLUB** on your ship.
- Every planet has its own **4 to 9 tracks**, sized to the planet. Tiny worlds get little tracks. Giant worlds get huge ones.
- You race **7 rivals** of every kind: beetles, slugs, locals, the planet's own creatures, fossils, trees, rocks, flowers, droids, market stalls... even your evil twin.
- **Controls:** UP go, B boost, R drift (hold it through a turn for a mini-turbo), A use an item, L horn, SELECT look behind, START pause.
- **Stunts:** ramps, **jumps over lakes and the open sea**, boost pads and **loop-de-loops**. Hit a loop fast and the camera cuts to the side, so you watch your kart go all the way round. Too slow and the loop spits you back out.
- **Items** from the ? boxes: pepper, beetle shell, slime, bubble, magnet, sneeze, 3 peppers and the grumpy cloud.
- **Grand Prix:** 4 races with points for every place. Win cups to open faster classes.
- **Time trial:** beat the gold, silver and bronze times on every track.
- **Build your own tracks** on any planet: drive round and press A to drop gates. The road joins them up. Turn ramps, boost pads and loops on or off, name it, test it and save up to **50 tracks**.
- **Kart garage:** 16 bodies (bathtub, teacup, sneaker, rubber duck, sofa, piano, bed, shopping cart...), colours, wheels, spoilers, exhausts, stickers, toppers, trails, underglow, horns and more.
- **Kart camera:** distance, height, field of view, swing, look into turns, speed zoom, bump shake, or a cockpit view. Pause a race and pick CAMERA to change it live.

## Kart World (opens after the ending)
- Beat the game and **KART WORLD** opens on the title screen. It is a whole racing game of its own.
- Every planet number (over 4 billion of them) has **1,000,000 tracks**. That is about **4.3 quadrillion tracks**.
- Grand Tour (10 tours), quick race, random race, time attack, elimination, endless run, track maker and your tracks.
- Options: planet number, world type, size, weird worlds, track number, class, laps, rivals, items, **stunts** (lots of jumps and loops, or none), rival skill, catch-up, mirror, reverse, time of day and weather.
- 18 drivers to unlock with trophies. Coins buy parts in Kart World's own garage.

## How big is it? (opens after the story)
Beat the Ant King and **HOW BIG IS IT?** opens on the title screen, and on the WORLD page of the pause screen (press SELECT). One beaten game opens it for every save. It has **37 real numbers from the game**, smallest first, all worked out exactly (every digit is checked against a big-number program):
- how far you have flown,
- colours you can mix, dots on the screen, relic collections, star and planet names,
- the planets and tracks in Kart World, every way to set up a race, and every trophy shelf,
- the universes, weird world mixes, planet DNA codes,
- bug names and track names you can type,
- the Technobeast's health (and the most it can ever have), the most gold you can have,
- the ships, karts, humanoids and bugs you can make, and every look you could have (all four together, 152 digits),
- every way to tour a cluster (32 x 31 x ... x 1), the farthest you can be from home, how wide a universe is, and all of them side by side,
- the clusters, stars and worlds in one universe and in all of them,
- **giant numbers, every digit:** every way to visit 256 favourites (507 digits), trips of 1,000 jumps (1,492 digits), favourites lists (26,742 digits), planet maps (39,457 digits), every save file the save chip could hold (78,914 digits) and every picture the screen can show (**173,394 digits**).

**SELECT** shows the list of all of them, with how many digits each one has: pick one and press A.

Each number has three views (A changes the view, LEFT/RIGHT changes the number):
- **IN SHORT:** "ABOUT 4.3 BILLION", how many digits it has, and how long it would take to read out.
- **EVERY DIGIT:** the whole number in big type, with commas.
- **BY NAME:** each group of three digits with its name, like **4 BILLION, 294 MILLION, 967 THOUSAND, 296**. The names go far past a centillion, built by the Conway-Wechsler rules. The biggest one is **SEPTENQUINQUAGINTILLISENONAGINTASEPTINGENTILLION**.

UP/DOWN scrolls (hold it and it goes faster and faster), L/R jumps a whole screen, SELECT lets the number roll by on its own, B goes back.

## Universe jumps (after the story)
There are **2^128 universes (about 340 undecillion)**, side by side, each one 1.1 quindecillion light years wide. Beating the Ant King gives your game a universe of its own (the story happens in UNIVERSE #1).
- On the galaxy map press **START**, then **A: JUMP TO ANOTHER UNIVERSE**.
- Pick its number: LEFT/RIGHT (hold to go faster), A to type it in, or SELECT for a random one. Then press START.
- A universe jump always costs **5.0 LY of fuel**, however far it goes. The trip counts up the light years and, at the end, the universes you passed.
- A favourite in another universe takes you straight there.
- The galaxy map and the WORLD page show which universe you are in.

## The Stormclock Coil (the faster-than-light ship part, new)
An original ship part that lets the ship fly **faster than light**, in six tiers: **x1** (light speed), **x1,000**, **x1 million**, **x1 billion**, **x1 trillion** and the top one, **x13 trillion light speed**. The speed shows on screen as a multiple of light ("SPEED 25 MILLION X LIGHT"), with a trip time to the planet, the next star, the galaxy edge or the edge of the universe. Trip times, fuel and distances use wide, saturating maths, so nothing overflows.
- **The speed figure.** The top tier, 13 trillion times light speed, comes from a fan estimate (SlashFilm's sums for a famous comic-book evacuation run, about 2.48 quintillion miles a second). It is not an official number, and no documented "attosecond run" speed could be found, so the figure is labelled as an estimate on its facts page. Facts pages in HOW BIG IS IT? explain the tiers, the quest and the odds.
- **How to get it (two locks).** EXTRAS shows STORMCLOCK COIL with a QUEST LOG (press A). Lock 1: an 8-step saved quest across the game (the Ant King, tech cores, a beast kill, fleet-battle wins, warp and fold drives, trips, dungeon depth, unique items). Lock 2: after the quest, every enemy **you** shoot down in a fleet battle rolls a **1 in 10,000** chance of dropping it, but only while **all six** conditions are true at once (hints appear in the quest log once the quest is done, and the live ones turn green): a fleet falls around you and you have five kills; you are far from home in the wild galaxy; you are among the rocks of a belt; your shield is gone and your hull is nearly dead; your tank is nearly empty; and you are flying at full speed. The quest and the coil are saved with your game.
- **Equip it** in the garage as a ship module. **L+R** engages it and steps up a tier; **R held** or **SELECT** slows down at any moment. It cuts out near planets, stations and lairs, or with no fuel (1 fuel a second).
- **The glitches.** The faster you go, the more the picture breaks, all of it a show on the screen (never your save or your game): star streaks and blue/red shift, palette cycling, corrupted tiles and sprites, wobbling and tearing lines, a tilted or mirrored world, scrambled speed digits, fake memory-error and crash screens, a fake "save corrupted" scare, warping music, garbage that snaps back, and a full REALITY BREAK at the top. The first time you engage it a warning screen explains this. In OPTIONS, MORE: EXTRAS you can turn on **FLASH-SAFE MODE** (no strobing or noise, slow colour changes, small shakes) or turn the **SPEED GLITCHES** off entirely.
- **Cheats** (debug menu, SHIP page, RAM only, never saved, marked CHEAT on screen): lend yourself the coil, a ship speed multiplier (x2, x10, x100, x1,000, x1 million) beyond the top tier, a speed readout, force the rare roll (still needs all six conditions and the quest), and mark the quest done. A coil lent by a cheat is never written into your save.
- **Tests.** `pc/tests/coil.sh` runs a rule self-test (the drop truth table, exactly 10 wins per 100,000 rolls, each missing condition blocks the drop even when forced, no overflow) and a glitch fuzz of every tier, normal and flash-safe, checking the real save is unchanged. The same fuzz runs on the GBA build.

## Debug menu (the Konami code)
On the title screen press UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A. That opens every mode at once (Kart World, HOW BIG IS IT?,
the INSANE-SECT difficulty and all of Kart World's classes, parts and coins) and shows the debug menu. From it you can start a
game with everything unlocked (every star, form, tool, relic, spell and bug friend, the best drives, a full wing), unlock
everything in a save you already have, and jump straight to any part of the game: a zone's planet, any dungeon floor, a boss,
space, a fleet battle of any size, the Technobeast at any stage, a far world of any type with any weird trait at any strength,
the galaxy map (for cluster and universe jumps), a kart race, Kart World, the makers, the garage, HOW BIG IS IT? and the credits.
Cheats: god mode, max stats, your level and gold, the difficulty, hide the HUD, an FPS counter, your form, the time of day and
the weather on every planet. On the SHIP page: the Stormclock Coil cheats (lend the coil, ship speed multiplier, speed readout, force the rare roll, quest done). There is a sound test too. Pick the save slot first: the debug game saves there.

## Dungeon floor events (new)
- Most floors roll an event: TREASURE VAULT, MONSTER NEST (bonus gold), ELITE HUNT (slay 2 elites for a reward), DARK FLOOR (double loot), SPEED RUN (reach the stairs in 60 seconds), CURSED FLOOR (tougher monsters, better loot), GOLD RUSH, BLESSED SHRINE (a free good mark), AMBUSH and QUIET FLOOR. The goal shows under your spell name.

## Things to do on planets (new)
- **Mining.** Every world grows four kinds of resource: **amber trees, crystal rocks, silk webs and nectar flowers** (what grows depends on the world's economy). Stand next to one and press A three times. The goods go into your ship's hold (HOLD 3/10 on the ship menu). Sell them at any station, where prices differ by economy, or...
- **The workbench** on your ship upgrades gear: thorns need crystals, shells need amber, charms need silk, plus gold. Each upgrade adds +2 levels, which raises damage or defence and strengthens magic effects. No cap except 97.
- **Hold SELECT + L or R** to point the arrow at the door, your ship, or the nearest resource.
- Buy a bigger hold at a station when it fills up.

## Longer and grindier (new)
- **Zone tiers.** After a zone's boss is beaten, going back down its door starts **tier 2**: one more floor, stronger monsters, better loot, and the boss is back at the bottom. Beat it for tier 3, and so on up to tier 10. A message tells you which tier you are entering. To go again: beat the boss, take the stairs up, land on the same planet and go down its door. The planet name shows T2, T3... on the status bar.
- **13 feats** (START > FEATS): slay 50 spiders, catch 10 fish, mine 40 things, dodge 25 bites, land on 6 worlds, go down 30 floors, find 3 uniques, clear 3 tiers, reach level 20, and more. Each pays gold and fame.
- **Fame has perks** now: each rank (KNOWN BUG, BRAVE BUG, HERO, CHAMPION, BUG LEGEND) adds gold find, health and crit.
- **Buddy levels up** from the XP you earn: more damage and health. His level shows on the BUG page.
- **Engine:** the planet ground is now pre-smoothed to twice the detail with a grass texture, and draws faster.

## Fighting (new)
- **Monsters wind up before they bite.** A red **!** flashes over them and they hiss. Step back, strafe, or dodge, and they snap at empty air and are open for a moment.
- **Dodge:** tap L or R twice to dash sideways. You can't be hurt while you dash. Dodge a bite at the right moment for "DODGED!".
- **Combos:** bites in a row count up (COMBO X2, X3). The third bite is a **CRUNCH**: double damage and a huge shove.
- **Extra damage** on anything staggered or winding up.
- **Beetles charge** from a distance. Sidestep and they slam into the wall and are stunned.
- **The Queen calls her brood** at half health.
- **Health bars** over every hurt monster. Buddy fights from the side now, so he doesn't block your bite.

## New screens
- **Minimap** in the corner of the dungeon view: walls, you, stairs, monsters (blinking red), cages, treasure.
- **Character screen** redone: health, mana, food and XP bars, stat pips, Buddy's health, and a bag in two columns (left/right to switch). Gear shows **+ or -** next to it if it would be better or worse than what you wear, with the exact change below.
- **Shops** show what an item does and how it compares to your gear.
- **Easy mode** in OPTIONS: half damage to you, slower hunger, you keep your bag when you faint.
- **Autopilot:** after you beat a zone's boss, the ship's first choice flies you straight to the next star. Hold A on it to launch by hand instead.
- **Planets:** search plants (A) for leaves to eat, berries, seeds and the odd shiny pebble. Plants grow back.
- **Planets are real 3D terrain now.** Every world is a height map that wraps around: rolling hills, valleys and lakes, drawn in 3D every frame (voxel style, like the old game Comanche). The ground rises and falls under you, the horizon curves like a small planet, far things are dimmer, and the sun lights the slopes.
- **Two cameras:** third person (behind your bug) or first person (through your own jaws, like the dungeons). Tap SELECT to switch. In first person, left/right turn and L/R step sideways.
- Lakes slow you down. Steep slopes are slow to climb.

## New in this update
- **Yellow arrow** on every planet. It points the short way to the door (or your ship). SELECT switches it.
- **A-button hints** on planets: "A: GO DOWN THE DOOR", "A: YOUR SHIP" and so on.
- **Dungeon map** (START). Rooms show up when you walk in. Gold = stairs down, blue = stairs up, green = caged bug.
- **Compass to the stairs** turns on once you have seen the stairs.
- **No more falling out by accident:** you start each floor on the up stairs, and they only work after you step off once.
- **How to play** on the title screen: 4 short pages.
- **Story cards:** a short intro on a new game, and a "what to do next" card after each boss.
- **An ending:** beat the Ant King to see THE END, your stats, and the credits. Then the endless galaxy opens.
- **New music:** space, boss fights (also the Technobeast), and the ending.
- **Galaxy map:** opens on the star you should go to next. Each tap jumps to the next star.
- **Fixes:** the dungeon status bar no longer flickers; shop and menu messages stay on screen inside the menu.

## Speed
The 3D view is heavy to draw, so it shows about 15 pictures a second. Your own movement (walking, strafing, turning) runs 60 times a second, and the rest of the dungeon (monsters, hunger, cooldowns, timers) ticks at a steady 15 a second, so nothing speeds up or slows down when drawing does. Walking and turning speeds are the `WALK_*` and `TURN_*` numbers at the top of the dungeon code in `main.c`.

## Saving, loading and options
- **Title screen:** CONTINUE (your last slot), NEW GAME (then MAKE YOUR BUG), LOAD GAME, HOW TO PLAY, OPTIONS. Up/down and A.
- **Three save slots.** Each card shows the world, level, XP bar, gold, fame and play time. In the slot picker: SELECT erases a slot, R copies it to another. Old single-slot saves show up as slot 1.
- **SYSTEM tab** (START, then L/R): SAVE GAME to your slot, SAVE TO ANOTHER SLOT, LOAD GAME, OPTIONS, QUIT TO TITLE. You can only save on the surface (not inside a dungeon). Loading and quitting work anywhere.
- **Autosave** still happens at your ship, after dungeons and at stations. Turn it off in OPTIONS if you want to save by hand.
- **Options:** music (off/low/full), sound effects, autosave, screen shake. They are remembered on the cartridge.
- **Fixed:** saved games load again. An old bug made the game think every save was broken.

## What is in it
- **Six zones**, each dungeon deeper than the last (3 to 8 floors), a boss at the bottom. Then the **endless galaxy**: new worlds forever, bottomless dungeons, loot you only keep if you get out.
- **Your bug:** health, mana, level, and 5 points per level for Strength, Dexterity, Vitality and Magic.
- **Gear like Fate:** thorns (bite), shells (armour) and charms, in normal, magic (blue) and rare (gold) versions with names like SHARP WASP STING OF LIFE.
- **Spells:** Fireball, Spark, Heal, Shield. Find their books.
- **Buddy, your pet:** follows you and fights. Feed him fish to turn him into a Fire Beetle, Stone Beetle or Stag King. Send him off to sell your spare gear (SELECT on the Gear page).
- **Fishing** in dungeon pools.
- **Unknown berries and seeds** like Fatal Labyrinth, hunger, crumbs, potions.
- **Quests and fame** from your ship's quest board; named elite monsters; wasps that shoot; beetles that hit hard.
- **Your ship:** supplies, a gear shop, selling, healing, and flying on.
- **Warp crystals** open a portal home; the door takes you back to the same floor.
- **Planets from space:** a round, turning world with seas, land, polar ice, drifting clouds, a soft day/night edge and a hazy rim; it keeps the frame rate up when you fly close.
- **Looks:** baked stone, brick, earth, moss, crystal and marble textures, doors, potion niches, plaques, rune bands, torches, smooth lighting with soft corner shadows, water and polished floors that reflect, embers and dust, head bob, chiptune music.

## Build
Needs an ARM GBA toolchain (`arm-none-eabi-gcc`, e.g. devkitARM), `gbafix`, and Python 3 with numpy. GitHub Actions builds it on every push (see the Actions tab).
```
make            # or: make TC=/path/to/arm-none-eabi-
```
Files: `main.c` (planet, dungeon loop, title, main), `dungeon.inc` (level builder, light baking, 3D renderer), `game.inc` (stats, gear, pet, quests), `ui.inc` (screens, shop, SYSTEM tab), `save.inc` (save slots, options, slot picker and menu widgets), `sound.inc` (music and effects), `planet.inc` (the 3D planet), `town.inc` (cities, creatures, jobs, races, ruins), `ship.inc` + `hangar.inc` (your ship), `space.inc`, `kart.inc` + `kart2.inc` + `kart3.inc` (kart racing, the track maker, Kart World), `sizes.inc` (HOW BIG IS IT?), `logo.c` (the DIPPINN PRODUCTIONS boot logo, shared with BORE), `gen.py` + `texgen.py` + `kartgen.py` + `bignums.py` (make `assets.h`: palettes, textures, sprites, the 3D-rendered karts, tables, and the giant numbers for HOW BIG IS IT?, every digit), `crt0.s` + `gba.ld`.

## Dungeon walls
The painted door sprites on dungeon walls are gone (they looked like doors and meant nothing). Niches, plaques, runes and panels stay. The locked keycard door in outposts is a real door and still looks like one.

## Space (new)
At your ship, pick **LAUNCH INTO SPACE**.
- D-pad: steer. B: thrust. R: brake. A: shoot pirates.
- Fly close to the planet and press A to land. Fly to the white box (station) and press A to dock.
- Station: buy and sell 8 kinds of goods (prices change by world type), refuel, repair, and upgrade laser, shield and cargo.
- SELECT: galaxy map. Pick a star and press A to jump (uses fuel). The map shows how far each star is in light years and how many days the trip takes.
- **Jumps take time.** Stars stream past, and the light years and the ship's clock count up as you go. A warp across the universe climbs through millions, billions, trillions... of light years and counts the galaxies you pass. A universe jump goes further still, and counts the universes.
- Beat each zone's boss to open the next star. Beat the Ant King to open the whole universe. Then START on the map jumps to the next cluster, warps far away, or jumps to another universe.

## The Technobeast (the second villain)
A machine bug that eats ships to grow. You meet it 3 times:
1. **Hatchling** - after you beat the Garden boss, it ambushes you in space. It is small and weak. Beat it and it flees, but you grab its tech: stations can now build **TECH CORE** (each level = 4x laser damage on the beast).
2. **Grown** - after the Picnic boss it comes back bigger, with a beam (hold B to fly fast and dodge) and a drone. Beat it and it flees... it was feeding tech to the Ant King!
3. **Complete** - after the Ant King falls, it wakes with **1 trillion HP**. You can't win in one go. Hit it, warp out (SELECT), get stronger, come back. **Its wounds never heal.** Find it any time from the galaxy map with **L**.

Shoot the glowing core for 3x damage. Weapon gear adds damage too. Its drones get stronger with your level, tech and kills. Kill it and it rebuilds 10x stronger.

## After the Ant King: the Tech Hive campaign
The Technobeast feeds from **5 Tech Hives**. While a hive lives, the beast heals itself back up: each living hive keeps 20% of its HP safe. You can't kill it until all 5 are gone.
- The next hive is marked in red on the galaxy map (SELECT).
- Fly there, land, and dive its dungeon. The **Hive Overlord** waits at the core: floors 3, 5, 7, 9 and 12.
- Each dead hive cracks the beast's armour: **2x damage** each time (32x after all 5).
- The hives are: The Scrap Hive, The Battery Moon, The Gear Forest, The Silicon Swamp, The Motherboard.
- Go back and forth: dive a hive, get loot and Tech Cores, then chip more off the beast.


## Feel and physics
- You speed up and slow down, bump walls, and can't walk through monsters. Monsters shove each other.
- Bites knock enemies back. Slam them into walls or into each other for extra damage. A short freeze when a hit lands.
- Barrels and crates: push them, smash them for gold and food.
- You see your own jaws. They sway when you walk and snap shut when you bite.
- The ship drifts and banks. Crashing into the planet, station, rocks or pirates hurts. Shoot asteroids to mine crystals and amber.

## Keep or scrap your loot
- 12 UNIQUE items (orange), mostly from bosses. Hive Overlords always drop one.
- At your ship: TROPHY CASE. KEEP an item forever (safe even if you faint) for permanent points: common 1, magic 3, rare 10, unique 50. Points give damage, gold and health. You can take uniques back out.
- Or SCRAP it (B on the gear page, or in the case) for Tech Scrap. Tech Cores now need gold + scrap.
- The Techno Shard doubles your damage on the Technobeast.

- **Difficulty:** EASY, NORMAL, HARD, BRUTAL, NIGHTMARE. Beat the Ant King on NIGHTMARE to unlock **INSANE-SECT** in OPTIONS: every story dungeon has 54x the floors and its own 54 tiers to fight down through (tier 54 is the last).

## The hollow-rock mine (Phase 3, first step)
A HOLLOW rock's dungeon is now a mine: long labyrinth shafts with earth walls, and some wall faces are ore veins (they look like crystal). Walk up to one and bite it (hold A): it shows how many bites are left, needs the same tool tier as a vein on the surface, and breaks into floor and ore in your bag (richer the deeper you go). Plain rock just says SOLID ROCK. Boss floors stay normal. Mine floors can have **TOXIC GAS** (it hurts you every few seconds; a shield keeps it out) or a **CAVE-IN RISK** (rocks fall now and then; keep moving and they miss, stand still and they hit). Every 5th mine floor is an **ELITE HUNT**. About 1 vein in 40 is a **MEGADEPOSIT** (3x ore, never poor), on the surface and in mines. Not yet: a drill-beast boss or the Forge dial.

## Humanoid callings
Your humanoid's outfit now gives it a **calling**, worked out from the parts you picked (nothing extra is saved). KNIGHT (armour, helmet, boots): +3 DEF, +8 HP, a bit slower. WIZARD (wizard hat, robe, glasses): +8 MANA, +1 DMG. RANGER (hoodie or jacket, cowboy hat or cap, boots): +15 SPD, +6 CRIT. ROGUE (shades or eyepatch, scarf, slim): +12 CRIT, +15 GOLD. EXPLORER (jetpack or backpack): +8 SPD, +1 LIGHT. ANGEL (wings, halo): +1 REGEN, +4 HP. ROYAL (crown or top hat): +35 GOLD, +1 DMG. Dress for none of them and you are a WANDERER with no bonus. Height and build count too: TINY is quick and frail, TOWERING tough and slow, SLIM quick, BROAD sturdy. In the mirror the new CALLING row dresses you for one, and the perks show live under the preview.

## Rumble
`rumble.inc` drives the motor of a rumble cartridge (Drill Dozer style GPIO, pin 3 at 0x080000C4). Plain ROM ignores the writes, so it is harmless without a motor.
Options > SELECT (MORE OPTIONS) > EXTRAS: RUMBLE (off, low, medium, high, max, extreme), RUMBLE EVENTS (all, fights and big, big only) and RUMBLE HUM (steady rumble from ship engines, kart boosts and drifts).
Every sound effect maps to a pattern (strength, length, shape, class); extra patterns: cave quakes, ship crashes and rare catches. The motor is pulsed as a duty cycle from the vblank interrupt, so a long load never leaves it stuck on.

<!-- RUMBLE-GAMECODE -->
**Rumble in emulators:** the ROM's game code is `V49E` (the Drill Dozer code) because mGBA only enables its rumble motor for game codes in its built-in list. Plain ROM and real carts ignore the code.
