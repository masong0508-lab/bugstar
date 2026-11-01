// pc_beings.inc: peoples, cities and creatures for pc_content.c (included at its end, one translation unit).
// All names are built from word lists on demand (parts x parts), nothing is stored. Every name and text is original.
//   peoples:   32 stocks x 16 ways of life              = 512
//   cities:    16 forms  x 16 characters                = 256
//   creatures: 66 body plans x 192 variants             = 12,672 kinds (variant = size 4 x diet 3 x habitat 4 x behaviour 4)
//              x 32,768 individuals per kind (gift 16 x colouring 16 x age/rank 8 x mutation 16) = 415,236,096
// An id is "part B * (number of parts A) + part A", so low ids are the plain ones.

// ------------------------------------------------------------------ peoples
static const struct { const char *name, *about; char s[6]; uint8_t life; } stock[N_STOCK] = {   // s = might wit charm craft temper (1..5)
 {"FERNLINGS","KNEE-HIGH AND LEAF-SKINNED. THEY FACE THE SUN.","23441",60},      {"QUILLFOLK","TALL AND SPINY. THEY WRITE WITH THEIR OWN QUILLS.","24342",70},
 {"BRINEKIN","SALT-BLUE SHORE DWELLERS WITH WEBBED HANDS.","33432",55},          {"CINDERSHADES","BLACK-ROCK FOLK, DULL RED WHEN ANGRY.","44225",50},
 {"MOSSWARDENS","BROAD, SLOW AND GREEN. KEEPERS OF OLD WOODS.","53343",120},     {"STILTERS","LONG-LEGGED WADERS WHO BUILD ABOVE THE FLOOD.","24343",65},
 {"GLASSBORN","CLEAR-SKINNED. THEY RING LIKE BELLS WHEN THEY WALK.","22544",90},  {"DUNEWALKERS","LEAN NOMADS WITH WIDE FEET AND WIDER HATS.","34332",60},
 {"HOLLOWFOLK","SMALL CAVE PEOPLE, HUGE EYES, SOFT VOICES.","23541",55},         {"CLIFFGLIDERS","LIGHT-BONED FLIERS WHO NEST ON SHEER ROCK.","25323",45},
 {"TIDESINGERS","SLEEK SWIMMERS WHOSE SONGS CARRY FOR MILES.","32532",65},       {"DEEPDIGGERS","STOCKY TUNNELLERS WHO READ STONE BY TOUCH.","52451",85},
 {"LANTERNKIN","THEIR SKIN GLOWS. THEY NEVER NEED A LAMP.","23531",70},          {"THORNLINGS","SMALL, PRICKLY AND PROUD. NO HUGGING.","32325",50},
 {"FROSTHIDES","THICK-FURRED POLAR FOLK. THEY LOVE A COLD MORNING.","43332",75}, {"BEETLEWRIGHTS","SHELL-BACKED CRAFTSFOLK WHO RIDE BEETLES.","33252",60},
 {"WEBWRIGHTS","SIX-ARMED SPINNERS. THEIR CLOTH WEIGHS NOTHING.","24253",55},      {"STONEKEEPERS","GREY, PATIENT AND VERY OLD. THEY REMEMBER ALL.","43451",200},
 {"FLEETHOOVES","FAST HOOFED RUNNERS WHO NEVER STAY PUT.","44323",50},           {"RIVERSKINS","SMOOTH, DAMP AND FRIENDLY. THEY LIVE ON BANKS.","32531",60},
 {"SUNBASKERS","SLOW MORNINGS, SHARP NOONS. SCALED, SQUINTING.","34322",65},     {"MOONGRAZERS","PALE HERD-FOLK WHO WORK ONLY AT NIGHT.","33432",70},
 {"COPPERMANES","MANED TRADERS WITH BRIGHT, NOISY JEWELLERY.","32523",55},       {"PEBBLEFOLK","ROUND AND TINY. THEY ROLL DOWNHILL ON PURPOSE.","21343",45},
 {"WILLOWKIN","BENDY AND GRACEFUL. THEY SWAY EVEN WHEN STILL.","22531",80},       {"SANDSHELLS","HARD-BACKED BURROWERS OF THE DRY COAST.","43332",70},
 {"BRAMBLEHANDS","FARMERS WITH TOUGH, TANGLED FINGERS.","43242",65},             {"SALTMARCHERS","TIDE-FLAT WALKERS WHO KNOW EVERY TIDE BY NAME.","33342",60},
 {"STARSCRIBES","THIN, STARE-EYED STUDENTS OF THE NIGHT SKY.","15331",75},       {"HIVEHEARTS","TINY FOLK WHO THINK AS ONE LARGE CROWD.","21442",30},
 {"RUSTWALKERS","SCRAP-BUILT BODIES OF IRON AND CLEVER WIRE.","44253",150},      {"DRIFTMASKS","MASKED WANDERERS. NO ONE HAS SEEN THEIR FACES.","33423",70},
};
static const struct { const char *name, *about; int8_t m, w, c, k; } way[N_WAY] = {            // deltas to might, wit, charm, craft
 {"REED-WEAVING","THEY WEAVE REEDS INTO BOATS, ROOFS AND LETTERS.",0,0,0,2},   {"STAR-READING","THEY PLAN EVERY YEAR BY THE SKY.",-1,2,0,0},
 {"GLASS-BLOWING","THEIR WORKSHOPS GLOW BY DAY AND BY NIGHT.",0,0,0,2},        {"HERD-KEEPING","THEIR HERDS ARE THEIR WEALTH AND FAMILY.",1,0,1,0},
 {"TIDE-FISHING","THEY FISH BY THE MOON AND SELL BY THE SUN.",1,0,0,1},        {"TRADE-ROAD","A BAG OF GOODS AND A SMILE FOR EVERY ROAD.",0,1,2,0},
 {"SHIELD-BEARING","THEY GUARD THEIR NEIGHBOURS FOR A FAIR PRICE.",2,0,0,0},   {"STORY-TELLING","NO FEAST ENDS UNTIL THE LAST TALE IS TOLD.",-1,1,2,0},
 {"DEEP-DELVING","THEY MINE GEMS AND SING TO THE ROCK.",1,0,0,1},              {"FIELD-TENDING","THEY GROW MORE FOOD THAN THEY CAN EAT.",1,0,1,0},
 {"HERB-HEALING","THEIR TEAS CURE EVERYTHING BUT BOREDOM.",-1,1,1,1},          {"TOWER-RAISING","THEY BUILD UPWARD, ALWAYS UPWARD.",1,0,0,2},
 {"SAIL-MAKING","THEIR SAILS CATCH WINDS NO ONE ELSE CAN FEEL.",0,1,0,1},      {"TRAIL-READING","THEY HUNT BY SMELL, TRACK AND PATIENCE.",1,1,-1,0},
 {"CHOIR-KEEPING","THEY SING AT SUNRISE, AND AT ANY EXCUSE.",-1,0,2,0},        {"QUIET-WANDERING","THEY OWN NOTHING AND KNOW EVERY ROAD.",0,2,-1,0},
};
int people_count(void){ return N_PEOPLE; }
void people_get(int id, People *o){
    memset(o, 0, sizeof *o); if(id < 0 || id >= N_PEOPLE){ snprintf(o->name, sizeof o->name, "?"); return; }
    int s = id % N_STOCK, w = id / N_STOCK;
    snprintf(o->name, sizeof o->name, "%s %s", way[w].name, stock[s].name);
    snprintf(o->about1, sizeof o->about1, "%s", stock[s].about); snprintf(o->about2, sizeof o->about2, "%s", way[w].about);
    o->might = (int8_t)clampi(stock[s].s[0] - '0' + way[w].m, 1, 7); o->wit = (int8_t)clampi(stock[s].s[1] - '0' + way[w].w, 1, 7);
    o->charm = (int8_t)clampi(stock[s].s[2] - '0' + way[w].c, 1, 7); o->craft = (int8_t)clampi(stock[s].s[3] - '0' + way[w].k, 1, 7);
    o->temper = (int8_t)(stock[s].s[4] - '0'); o->lifespan = (int16_t)(stock[s].life + ((w * 7) % 11) - 5);
}

// ------------------------------------------------------------------ cities
static const struct { const char *name, *about; int8_t pop, walls, trade, wonder; } form[N_FORM] = {
 {"TERRACE TOWN","HOUSES STEP DOWN A HILL IN LONG GREEN RINGS.",5,3,4,5},       {"CANAL CITY","STREETS OF WATER. EVERY DOOR HAS A BOAT.",7,2,8,6},
 {"STILT VILLAGE","HOMES ON LEGS ABOVE THE FLOOD, JOINED BY ROPE.",3,1,3,4},    {"CAVE WARREN","A MAZE OF HOMES CUT INTO LIVING ROCK.",6,6,2,3},
 {"TREETOP HAMLET","RUNGS AND LANTERNS HIGH IN GREAT TREES.",2,2,2,7},         {"CLIFF NEST","ROOMS HUNG ON A CLIFF. VISITORS ARRIVE BY BASKET.",3,7,1,6},
 {"WALLED FORT","THICK WALLS, ONE GATE AND A VERY LOUD BELL.",4,9,3,1},         {"MARKET CROSSING","FOUR ROADS MEET HERE AND EVERYONE HAGGLES.",6,2,9,2},
 {"TENT RING","A CIRCLE OF TENTS THAT MOVES WITH THE SEASONS.",2,1,5,3},        {"SPIRE CLUSTER","NEEDLE TOWERS LINKED BY BRIDGES OF GLASS.",8,5,6,9},
 {"HARBOUR TOWN","CRANES, ROPES AND GULLS. THE TIDE RUNS THE CLOCK.",6,3,8,3},  {"RAFT FLEET","A WHOLE CITY LASHED TOGETHER, ADRIFT.",5,2,6,4},
 {"HIVE BLOCKS","STACKED CELLS OF CLAY, BUSY AS A SUMMER NEST.",9,3,5,2},       {"GLASS DOME","ONE GREAT DOME KEEPS THE WEATHER OUT.",6,6,4,8},
 {"CARAVAN WHEEL","WAGONS IN A RING THAT ROLLS ACROSS THE DESERT.",3,4,7,3},    {"STONE CIRCLE","STANDING STONES AROUND A SMALL, VERY OLD TOWN.",2,4,2,9},
};
static const struct { const char *name, *about; int8_t pop, walls, trade, wonder; } chr[N_CHAR] = {
 {"SINGING","A SONG DRIFTS THROUGH ITS STREETS FROM DAWN TO DUSK.",0,0,0,2},    {"LANTERN-LIT","AT NIGHT A THOUSAND LAMPS TURN IT TO GOLD.",0,0,1,1},
 {"WINDY","FLAGS SNAP AND HATS FLY. NOBODY MINDS.",0,0,0,0},                    {"GOLDEN","ITS ROOFS ARE GILDED, AND SO ARE ITS TAXES.",0,0,2,1},
 {"QUIET","VISITORS LOWER THEIR VOICES WITHOUT BEING TOLD.",-1,0,-1,1},         {"BUSY","THE STREETS ARE FULL FROM FIRST LIGHT.",2,0,1,0},
 {"ANCIENT","ITS OLDEST WALL HAS A NAME AND A GRANDCHILD.",0,1,0,2},            {"NEWBORN","STILL SMELLING OF FRESH PAINT AND WET CLAY.",-1,0,0,0},
 {"PROUD","ITS FLAG IS HUGE AND ITS MOTTO IS LONGER.",0,1,0,0},                 {"HUNGRY","THE BAKERS NEVER SLEEP. THE BAKERS ARE NEVER ENOUGH.",1,0,0,0},
 {"FESTIVE","THERE IS A PARADE ON, OR ONE IS ABOUT TO START.",1,-1,1,1},         {"HAUNTED","THE FRIENDLY KIND OF GHOSTS. MOSTLY.",-1,0,-1,2},
 {"CLEVER","EVERY GATE HAS A RIDDLE AND EVERY RIDDLE A PRIZE.",0,1,1,1},        {"ROUGH","TOUGH ROADS, TOUGH FOLK, SURPRISINGLY SOFT PIE.",0,1,0,-1},
 {"SECRET","NOT ON ANY MAP. THE LOCALS LIKE IT THAT WAY.",-1,2,-1,1},           {"FLOWERING","WINDOW BOXES ON EVERY SILL, VINES ON EVERY WALL.",0,-1,0,2},
};
int city_count(void){ return N_CITY; }
void city_get(int id, CityKind *o){
    memset(o, 0, sizeof *o); if(id < 0 || id >= N_CITY){ snprintf(o->name, sizeof o->name, "?"); return; }
    int f = id % N_FORM, c = id / N_FORM;
    snprintf(o->name, sizeof o->name, "%s %s", chr[c].name, form[f].name);
    snprintf(o->about1, sizeof o->about1, "%s", form[f].about); snprintf(o->about2, sizeof o->about2, "%s", chr[c].about);
    o->pop = (int16_t)clampi(form[f].pop + chr[c].pop, 1, 9); o->walls = (int16_t)clampi(form[f].walls + chr[c].walls, 0, 9);
    o->trade = (int16_t)clampi(form[f].trade + chr[c].trade, 0, 9); o->wonder = (int16_t)clampi(form[f].wonder + chr[c].wonder, 0, 9);
}

// ------------------------------------------------------------------ creatures
static const char *const cls_nm[6] = {"BEAST","FLIER","SWIMMER","CRAWLER","SPIRIT","MONSTER"};
static const char *const rar_nm[6] = {"COMMON","UNCOMMON","RARE","EPIC","LEGENDARY","MYTHIC"};
static const char *const cst_nm[N_CSTAT] = {"HEALTH","SPEED","BITE","GUARD","WIT","TAMING","LIFESPAN"};
// 66 body plans, 11 per class. s = health speed bite guard wit lifespan (1..9)
static const struct { const char *name, *about; char s[7]; } plan[N_PLAN] = {
 {"THORNBACK","A BROAD GRAZER WITH A HEDGE OF SPINES.","724624"},   {"MIREHOUND","A LONG-EARED SNIFFER OF BOGS AND DITCHES.","545535"},
 {"DUSKSTAG","A PALE DEER THAT ONLY WALKS AT TWILIGHT.","368348"},    {"LANTERN BOAR","ITS TUSKS GLOW WHEN IT IS HAPPY.","635535"},
 {"SALTHIDE","A GREY, WRINKLED HAULER OF THE DRY COAST.","823427"},   {"GRAVELKIT","A QUICK LITTLE DIGGER THAT HOARDS SHINY STONES.","258246"},
 {"HOLLOWBEAR","A SLEEPY GIANT WHO LIVES IN HOLLOW TREES.","835529"}, {"STILTWALKER","A TALL, THIN BROWSER WITH KNEES BENDING BOTH WAYS.","458237"},
 {"WOOLBRAMBLE","A SHAGGY HERDER WITH BERRIES IN ITS FLEECE.","633438"},{"CLAYHORN","A STOCKY BULL WHOSE HORNS ARE BAKED CLAY.","748426"},
 {"SUNDIAL TORTOISE","ITS SHELL CASTS A TRUE SHADOW AT NOON.","912679"},
 {"GLIMMERWING","A MOTH-LIKE FLUTTERER TRAILING SPARKLING DUST.","187334"},{"KETTLEKITE","A ROUND HAWK THAT WHISTLES LIKE A BOILING POT.","477546"},
 {"ROOKLET","A CLEVER LITTLE CROW THAT COLLECTS BUTTONS.","267857"},     {"SKYWHELK","A SPIRAL-SHELLED GLIDER OF THE HIGH WINDS.","349448"},
 {"DUSTMOTH","A GREY NIGHT-FLIER THAT SMELLS OF OLD BOOKS.","176435"},   {"PAPERCRANE","A THIN, FOLDED-LOOKING WADER, VERY STILL.","356569"},
 {"BELLFINCH","A TINY BIRD WHOSE SONG IS A CLEAR SMALL BELL.","189457"},{"STORMWREN","A FEARLESS FLIER THAT RIDES THE FRONT OF A GALE.","387666"},
 {"CINDERBAT","A SOOT-COLOURED BAT OF OLD CHIMNEYS AND VENTS.","266544"},{"TASSELOWL","AN OWL WITH SILK TASSELS FOR EAR TUFTS.","376768"},
 {"LOFTRAY","A FLAT, WIDE SKY-RAY DRIFTING ABOVE THE CLOUDS.","635388"},
 {"REEFLURKER","A STRIPED AMBUSHER WEARING THE REEF AS A COAT.","566656"},{"INKFIN","A SHY SWIMMER THAT WRITES WARNINGS IN BLUE INK.","358358"},
 {"GLASSCARP","A CLEAR-BODIED FISH YOU CAN COUNT THE BONES OF.","274235"},{"TIDEHARE","A SWIMMING HARE THAT BOUNCES ALONG THE SURF.","268445"},
 {"BRINEMAW","A BIG, SLOW, WIDE-MOUTHED FILTER FEEDER.","823429"},        {"PEARLCRAB","A SMALL CRAB THAT GUARDS ONE PEARL.","447637"},
 {"DRIFTJELLY","A SOFT GLOWING BELL THAT NEVER HURRIES.","312214"},      {"MUDLEAPER","A STUBBY FISH THAT WALKS ON ITS FINS.","444435"},
 {"RIBBONWORM","A LONG, FLAT, COLOURFUL RIBBON OF A HUNTER.","376324"},  {"LAMPGILL","A DEEP-WATER FISH WITH A LAMP ON A STALK.","444558"},
 {"KELPSLUG","A FAT, GENTLE GRAZER OF THE KELP FORESTS.","621338"},
 {"LADYBELL","A ROUND SPOTTED BEETLE THAT CHIMES WHEN IT FLIES.","255434"},{"DUNGWRIGHT","A BEETLE THAT BUILDS WITH WHAT OTHERS LEAVE.","436546"},
 {"STILTSPIDER","A TALL SPIDER THAT WALKS ON NEEDLE LEGS.","365443"},    {"NEEDLEANT","A SLENDER, FAST ANT WITH A SMALL BRAVE STING.","268335"},
 {"BARKLOUSE","A FLAT GREY SUCKER THAT PASSES FOR BARK.","324224"},      {"GLOWGRUB","A PLUMP LARVA THAT LIGHTS UP WHEN IT SINGS.","412325"},
 {"MIRRORBEETLE","ITS SHELL SHOWS WHATEVER YOU LOOK LIKE. RUDELY.","456637"},{"SILKWASP","A BUSY WASP THAT WRAPS ITS NEST IN SHINY THREAD.","287456"},
 {"TUMBLEROLL","A WOODLOUSE THAT BALLS UP AND ROLLS AWAY.","345324"},     {"SPINDLEMANTIS","A GREEN HUNTER, STILL AS A TWIG, QUICK AS A BLINK.","358645"},
 {"WAXCENTIPEDE","A LONG, SLOW WALKER, GLOSSY AND ALMOST HARMLESS.","533437"},
 {"HEARTHWISP","A SMALL WARM FLAME THAT FOLLOWS GOOD COOKS.","165659"},   {"FOGSINGER","A THIN VOICE IN THE MIST THAT SINGS YOU BACK TO THE PATH.","255779"},
 {"ECHOLING","A SOUND WITH LEGS. IT REPEATS YOU, A LITTLE WRONG.","276768"},{"LANTERN SHADE","A DARK SHAPE HOLDING A TINY LIGHT FOR THE LOST.","347879"},
 {"ASHWARDEN","A SOFT GREY GUARD OF BURNT FIELDS AND OLD FIRES.","554779"},{"BELLWRAITH","A PALE TOLLING GHOST THAT RINGS BEFORE BAD WEATHER.","346889"},
 {"DEWSPRITE","A SHINING DROP WITH WINGS, UP BEFORE THE SUN.","189657"},  {"GALEHERALD","A SWIRLING WIND THAT ANNOUNCES STORMS ALOUD.","469649"},
 {"STARLORN","A DIM SPARK FROM THE SKY THAT MISSES ITS FRIENDS.","248799"},{"WILLOWSHADE","A TREE SPIRIT THAT WEEPS ONLY WHEN IT RAINS.","436879"},
 {"TIDEMUSE","A WATER SPIRIT WHO HUMS THE TUNE OF THE TIDE.","355779"},
 {"MAWBLOOM","A HUGE FLOWER WITH A STOMACH, POLITE UNTIL FED.","749538"},  {"SCRAPGOLEM","A WALKING HEAP OF ROOF TILES, WIRE AND BAD MOODS.","836626"},
 {"CRAGSERPENT","A STONE-COLOURED COIL THAT SLEEPS IN MOUNTAINS.","738549"},{"BOGTITAN","A MOUNTAIN OF MUD AND REEDS THAT STANDS UP SLOWLY.","912729"},
 {"SPOREMOTHER","A SWOLLEN FUNGUS QUEEN AND HER SPORE-CHOIR.","635759"},  {"PATCHWORK FIEND","STITCHED FROM SPARE PARTS OF MANY BEASTS.","657547"},
 {"GLASSTUSK","A CRYSTAL-HORNED CHARGER THAT SHATTERS WHEN IT SNEEZES.","557418"},{"RUST COLOSSUS","A HUNDRED-YEAR GIANT OF IRON, CREAKING.","913839"},
 {"NIGHTLOOM","A MANY-LEGGED WEAVER OF DREAMS AND OF TRAPS.","557758"},   {"THORNCHOIR","A BUSH OF SINGING THORNS THAT SURROUNDS ITS PREY.","538648"},
 {"MIRRORWORM","A BURROWER WHOSE TRAIL IS A ROAD OF SHINY SILVER.","567538"},
};
_Static_assert(sizeof plan / sizeof plan[0] == N_PLAN, "66 body plans");
static const char *const size_nm[4] = {"TINY","SMALL","LARGE","HUGE"};
static const char *const diet_nm[3] = {"GRAZER","HUNTER","SCAVENGER"};
static const char *const hab_nm[4] = {"MARSH","HILLS","CAVES","SHORES"};
static const char *const beh_ph[4] = {"SHY: FLEES NOISE","FIERCE: GUARDS TURF","SWARMS IN GROUPS","ROAMS ALONE"};
// the two lead words of a kind's name: ADJ1 comes from size x diet, ADJ2 from habitat x behaviour (12 + 16 words, all different)
static const char *const adj1[4][3] = {{"NIBBLING","STINGING","SCRAPPY"},{"GRAZING","PROWLING","FORAGING"},{"BROWSING","STALKING","RAVENING"},{"TOWERING","THUNDERING","BOTTOMLESS"}};
static const char *const adj2[4][4] = {{"MIRE-HIDING","BOG-SNAPPING","REED-SWARMING","FEN-WANDERING"},{"CRAG-HIDING","CLIFF-RAGING","RIDGE-HERDING","SLOPE-ROAMING"},
                                      {"DARK-SHY","DEN-GUARDING","HIVE-DWELLING","TUNNEL-ROAMING"},{"TIDE-HIDING","SURF-BITING","BEACH-FLOCKING","WRACK-ROAMING"}};
static const struct { const char *name, *about; int8_t st, amt; } gift[N_GIFT] = {
 {"NIGHT-SIGHT","SEES AS WELL IN THE DARK AS IN DAYLIGHT.",4,2},   {"SILK-SPINNING","SPINS A THREAD STRONGER THAN ROPE.",3,2},
 {"WATER-WALKING","SKIMS OVER PONDS WITHOUT A RIPPLE.",1,2},       {"STONE-SKIN","ITS HIDE TURNS ARROWS, MOSTLY.",3,3},
 {"SWEET SONG","CALMS EVERY LISTENER, SOMETIMES TO SLEEP.",5,3},   {"LIGHT-FOOT","LEAVES NO TRACKS AT ALL.",1,3},
 {"HEALING-TOUCH","A GENTLE LICK CLOSES SMALL WOUNDS.",0,4},       {"WEATHER-SENSE","KNOWS A STORM A DAY BEFORE IT COMES.",4,2},
 {"MIMICRY","COPIES ANY VOICE IT HEARS.",4,3},                     {"PACK-LEADING","OTHERS OF ITS KIND FOLLOW WITHOUT QUESTION.",0,3},
 {"SEED-SCATTERING","WHEREVER IT WALKS, FLOWERS FOLLOW.",5,2},    {"SOFT GLOW","SHINES SOFTLY, LIKE A SMALL LAMP.",3,1},
 {"ECHO-STEP","VANISHES, THEN APPEARS A SHORT WAY OFF.",1,4},     {"GOLD-SNIFFING","ALWAYS FINDS THE NEAREST COIN.",4,2},
 {"LUCK-BRINGING","PEOPLE SAY IT BRINGS LUCK. IT AGREES.",5,2},    {"FIRE-TAMING","UNAFRAID OF FLAMES, SOMETIMES FOND OF THEM.",2,3},
};
static const char *const colour_nm[N_COLOUR] = {"MOSS-GREEN","EMBER-RED","SLATE-BLUE","BONE-WHITE","SOOT-BLACK","HONEY-GOLD","ROSE-PINK","PLUM-PURPLE","SAND-TAN","TEAL-STRIPED","DAPPLED GREY","SILVER","COPPER","OPAL","MIDNIGHT","SUNSET-ORANGE"};
static const struct { const char *name; uint8_t pct; int8_t wit, life; } rank_t[N_RANK] = {
 {"HATCHLING",40,0,1},{"YOUNG",70,0,2},{"ADULT",100,0,3},{"VETERAN",115,1,3},{"ELDER",100,2,4},{"WARDEN",130,1,3},{"CHAMPION",160,0,3},{"ANCIENT",150,3,5},
};
static const struct { const char *name; int8_t st, amt; } mut_t[N_MUT] = {
 {"NONE",0,0},{"EXTRA EYES",4,1},{"GLASS SKIN",3,-1},{"LONG LIMBS",1,2},{"FEATHERY FUR",3,1},{"BIG EARS",4,1},{"SHINING HORN",2,2},{"SPLIT TAIL",1,1},
 {"HOLLOW BONES",1,3},{"STRIPED WINGS",1,2},{"TINY CROWN",5,2},{"LOOP TAIL",3,1},{"DOUBLE VOICE",5,1},{"FROST BREATH",2,2},{"MOSS BEARD",0,2},{"SPARKLING TEETH",2,3},
};
int creature_plan_count(void){ return N_PLAN; }
int creature_kind_count(void){ return N_KIND; }
const char *creature_stat_name(int s){ return s >= 0 && s < N_CSTAT ? cst_nm[s] : "?"; }
const char *creature_class_name(int c){ return c >= 0 && c < 6 ? cls_nm[c] : "?"; }
const char *rarity_name(int r){ return r >= 0 && r < 6 ? rar_nm[r] : "?"; }
void creature_kind(int id, Creature *o){
    memset(o, 0, sizeof *o); if(id < 0 || id >= N_KIND){ snprintf(o->name, sizeof o->name, "?"); return; }
    int pl = id % N_PLAN, v = id / N_PLAN, beh = v % 4, hab = (v / 4) % 4, diet = (v / 16) % 3, size = v / 48;
    const char *s = plan[pl].s;
    snprintf(o->name, sizeof o->name, "%s %s %s", adj1[size][diet], adj2[hab][beh], plan[pl].name);
    snprintf(o->about1, sizeof o->about1, "%s", plan[pl].about);
    snprintf(o->about2, sizeof o->about2, "A %s %s OF THE %s. %s.", size_nm[size], diet_nm[diet], hab_nm[hab], beh_ph[beh]);
    int h = s[0] - '0', sp = s[1] - '0', b = s[2] - '0', g = s[3] - '0', w = s[4] - '0', l = s[5] - '0';
    static const int hm[4] = {2, 5, 10, 20};
    int health = h * hm[size], speed = sp + 2 - size, bite = b * (2 + size) / 2, guard = g + size / 2, wit = w, tame, life = l * (size + 1) * 2;
    if(diet == 0){ guard++; bite--; } else if(diet == 1){ bite += 2; speed++; guard--; } else { wit++; health += 3; }
    if(beh == 0){ speed += 2; bite--; } else if(beh == 1){ bite += 2; health += h; } else if(beh == 2){ health -= h; bite++; } else { speed++; health += h / 2; }
    if(hab == 0) health += health / 10; else if(hab == 1) guard++; else if(hab == 2) wit++; else speed++;
    tame = 4 + (beh == 0 ? 3 : beh == 1 ? -3 : beh == 2 ? 0 : -1) + (w - 4) / 2 - (diet == 1 ? 2 : 0);
    o->stat[0] = (int16_t)clampi(health, 1, 999); o->stat[1] = (int16_t)clampi(speed, 1, 12); o->stat[2] = (int16_t)clampi(bite, 1, 40);
    o->stat[3] = (int16_t)clampi(guard, 0, 14); o->stat[4] = (int16_t)clampi(wit, 1, 12); o->stat[5] = (int16_t)clampi(tame, 0, 9); o->stat[6] = (int16_t)clampi(life, 1, 400);
    o->plan = (uint8_t)pl; o->cls = (uint8_t)(pl / 11); o->size = (uint8_t)size; o->diet = (uint8_t)diet; o->habitat = (uint8_t)hab; o->behav = (uint8_t)beh;
    { int pts = size + (o->cls >= 4) + (beh == 0 && hab == 2) + (o->stat[6] > 60) + (o->stat[4] >= 8); o->rarity = (uint8_t)clampi(pts - 1, 0, 3); }   // kinds reach EPIC; the top two tiers need a special individual
}
void creature_indiv(const Indiv *iv, CreatureInd *o){
    Creature k; memset(o, 0, sizeof *o);
    int g = iv->gift % N_GIFT, c = iv->colour % N_COLOUR, r = iv->rank % N_RANK, m = iv->mut % N_MUT;
    creature_kind(iv->kind % N_KIND, &k);
    snprintf(o->name, sizeof o->name, "%s %s %s OF %s", rank_t[r].name, colour_nm[c], k.name, gift[g].name);
    if(m) { size_t n = strlen(o->name); snprintf(o->name + n, sizeof o->name - n, " WITH %s", mut_t[m].name); }
    snprintf(o->about1, sizeof o->about1, "%s", k.about1); snprintf(o->about2, sizeof o->about2, "%s", k.about2);
    snprintf(o->about3, sizeof o->about3, "%s", gift[g].about);
    for(int i = 0; i < N_CSTAT; i++) o->stat[i] = k.stat[i];
    o->stat[0] = (int16_t)(o->stat[0] * rank_t[r].pct / 100); o->stat[2] = (int16_t)(o->stat[2] * rank_t[r].pct / 100); o->stat[4] += rank_t[r].wit; o->stat[6] = (int16_t)(o->stat[6] * rank_t[r].life / 3);
    o->stat[gift[g].st] += gift[g].amt; if(m) o->stat[mut_t[m].st] += mut_t[m].amt;
    o->stat[0] = (int16_t)clampi(o->stat[0], 1, 999); o->stat[1] = (int16_t)clampi(o->stat[1], 1, 14); o->stat[2] = (int16_t)clampi(o->stat[2], 1, 60);
    o->stat[3] = (int16_t)clampi(o->stat[3], 0, 18); o->stat[4] = (int16_t)clampi(o->stat[4], 1, 16); o->stat[5] = (int16_t)clampi(o->stat[5], 0, 9); o->stat[6] = (int16_t)clampi(o->stat[6], 1, 999);
    { int pts = k.rarity / 2 + (r >= 6) + (r == 7) + (m >= 12) + (g >= 14); o->rarity = (uint8_t)clampi(pts, 0, 5); }
}
void indiv_from_seed(uint64_t seed, Indiv *iv){
    Rng r = { seed ^ 0x494E444956ull };
    iv->kind = (uint16_t)rndn(&r, N_KIND); iv->gift = (uint8_t)rndn(&r, N_GIFT); iv->colour = (uint8_t)rndn(&r, N_COLOUR); iv->rank = (uint8_t)rndn(&r, N_RANK); iv->mut = (uint8_t)rndn(&r, N_MUT);
}
// the natives of a planet: creatures (more of the kinds the traits favour), peoples, maybe a city, and one famous individual
static void make_natives(PlanetInfo *p, Rng *r){
    int w[6] = {2, 2, 2, 2, 2, 2}, tot = 0, nb, k;
    for(k = 0; k < p->n; k++) for(int c = 0; c < 6; c++) w[c] += cboost[p->id[k] % TR_BASE][c];
    for(int c = 0; c < 6; c++) tot += w[c];
    nb = clampi(3 + p->life / 2 + (int)rndn(r, 3) - 1, 1, MAX_NATIVE);
    for(k = 0; k < nb; k++){
        int tries, kind = 0;
        for(tries = 0; tries < 6; tries++){
            int x = (int)rndn(r, tot), c = 0; while(x >= w[c]){ x -= w[c]; c++; }
            int pl = c * 11 + (int)rndn(r, 11), v = (int)rndn(r, N_VARIANT), dup = 0; kind = v * N_PLAN + pl;
            for(int j = 0; j < k; j++) if(p->being[j] % N_PLAN == pl) dup = 1;
            if(!dup) break;
        }
        p->being[k] = (uint16_t)kind;
    }
    p->nbeing = (uint8_t)nb;
    { int chance = p->life <= -2 ? 0 : 40 + p->life * 12;
      if((int)rndn(r, 100) < chance){ p->npeople = (uint8_t)clampi(1 + (int)rndn(r, 2) + (p->life >= 4), 1, MAX_PEOPLE);
          for(k = 0; k < p->npeople; k++) p->people[k] = (uint16_t)rndn(r, N_PEOPLE); }
      p->city = -1; if(p->npeople && (int)rndn(r, 100) < 55 + 5 * p->life) p->city = (int16_t)rndn(r, N_CITY); }
    { Indiv iv; iv.kind = p->being[0]; iv.gift = (uint8_t)rndn(r, N_GIFT); iv.colour = (uint8_t)rndn(r, N_COLOUR); iv.rank = (uint8_t)(3 + rndn(r, 5)); iv.mut = (uint8_t)rndn(r, N_MUT);
      p->legend_kind = iv.kind; p->legend_gift = iv.gift; p->legend_colour = iv.colour; p->legend_rank = iv.rank; p->legend_mut = iv.mut; }
}
