// ================================================================= THE DEBUG SYSTEM
// One menu, two doors: the Konami code (UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A) on the TITLE screen, or the same code on the SYSTEM page of the
// pause screen (after that, SELECT on the SYSTEM page opens it). Pages: GAME, CHEAT, ITEMS, WORLD, SHIP, GO, INFO, TOOLS. Rows that make no sense
// where you are (a dungeon row on a planet, a title row in a game) are hidden or greyed with the reason. LIVE cheats are plain flags read by the game
// every frame (see dbg_tick); instant actions (heal, give, kill, teleport) run once. Nothing here is saved except by the normal save.
// (The old description follows.)
// Putting the code in unlocks every mode at once (Kart World, HOW BIG IS IT?, INSANE-SECT, all of Kart World's classes) and opens this menu:
// start a game with cheats, or jump straight to any part of the game: a zone's planet, any dungeon floor, a boss, space, a fleet battle,
// the Technobeast, a far world of any type with any weird trait, the galaxy map, a kart race, the makers, the credits, a sound test.
static void kill_mon(DObj *o); static void arrive_on_planet(void); static void landing_screen(int welcome);
typedef struct { int slot, maxst, lvl, gold, form, body, zone, floor, fleet, beast, kart, laps, mus, sfxn, go, ga, xa, gk, gr, gl, cons, spdi; } Dbg;
static Dbg dbg={.slot=2,.floor=1,.fleet=8,.beast=2,.laps=3,.mus=3,.sfxn=2,.ga=1,.xa=1,.gr=2,.gl=20};
enum { DG_NONE, DG_NEW, DG_CONT, DG_PLANET, DG_FLOOR, DG_BOSS, DG_SPACE, DG_FLEET, DG_BEAST, DG_FAR, DG_MAP, DG_KART, DG_GARAGE, DG_CREDITS };
static const char *const dbg_lvl_nm[5]={"KEEP","10","25","50","99"}; static const s64 dbg_lvl_v[5]={0,10,25,50,99};
static const char *const dbg_gold_nm[5]={"KEEP","10,000","1 MILLION","1 BILLION","THE MOST"}; static const s64 dbg_gold_v[5]={0,10000,1000000,1000000000LL,S64_MAX};
static const char *const dbg_form_nm[3]={"BUG","HUMANOID","CREATURE"}; static const u8 dbg_form_v[3]={FORM_BUG,FORM_HUMAN,FORM_CREATURE};
static const char *const dbg_beast_nm[3]={"HATCHLING","GROWN","COMPLETE"};
static const char *const dbg_mus_nm[8]={"PLANET","DUNGEON","TITLE","SPACE","BOSS","ENDING","KART RACE","FINAL LAP"};
static const char *const dbg_asis[1]={"AS IS"};
static void dbg_unlock_modes(void){                            // the modes: open for every save, right now
    kw_unlock(); opt_insane=1; settings_save();
    KWSave keep=kw; kw_load(); kw.cls_open=5; for(int p=0;p<NKPART;p++) kw.own[p]=0xFFFF; if(kw.coins<999999) kw.coins=999999; kw_store(); kw=keep;
}
static void dbg_unlock_game(void){                             // everything in the game you are in: stars, forms, tools, the drive, the friends, the relics
    ant_king=1; zones_open=6; cleared_mask=63; forms_open=7; tools_owned=0xFF; warp_lv=8; fuel_tank=8; fuel=fuel_max();
    relic_bits=0xFFFF; data_cores=8; wing_n=MAXWING; laser_lv=4; shield_lv=8; cargo_cap=2000; hull=100;
    for(int i=0;i<6;i++) have_bug[i]=1; for(int i=0;i<NSPELL;i++) spell_known[i]=1; beast_stage = beast_stage>2 ? beast_stage : 2;
    recalc(); hearts=maxh; mp=maxmp;
}
static void dbg_cheats(void){                                  // the menu's cheats, applied to the game just started or loaded
    if(dbg.maxst){ st_str=st_dex=st_vit=st_mag=300; }
    if(dbg.lvl) level=dbg_lvl_v[dbg.lvl];
    if(dbg.gold) gold=dbg_gold_v[dbg.gold];
    if(dbg.form){ form=dbg_form_v[dbg.form]; forms_open=7; tools_owned=0xFF; tool_held=1; form_body=dbg.body; form_col=rgb15(200,80,200); form_palette(); }
    recalc(); hearts=maxh; mp=maxmp;
}
static void dbg_story(int z){ zones_open=6; cleared_mask=(1<<z)-1; ant_king=0; cur_sys=z; }       // the story as it stands when you reach zone z
static int dbg_start(void){                                    // called by start_game after the game is set up: the GO targets that need their own set-up. 1 = done
    int go=dbg.go; if(!go) return 0;
    dbg_unlock_game(); dbg_cheats();
    if(go==DG_PLANET || go==DG_FLOOR || go==DG_BOSS || go==DG_KART || go==DG_GARAGE || go==DG_CREDITS){ dbg_story(dbg.zone); land_on_current(); planet_tables(); return 0; }   // (the normal landing follows)
    if(go==DG_SPACE){ dbg.go=0; fade(0); scene=2; page_back=1; space_enter(1); music(3); return 1; }
    if(go==DG_FLEET){ dbg.go=0; fade(0); scene=2; page_back=1; wing_n=MAXWING; no_ambush=1; space_enter(0); no_ambush=0; start_battle(dbg.fleet); music(3); return 1; }
    if(go==DG_BEAST){ dbg.go=0; { volatile u16 *v=(volatile u16*)0x06000000; for(int i=0;i<0x14000/2;i++) v[i]=0; } fade(0); scene=2; page_back=1; beast_defaults(); beast_stage=dbg.beast; tech_lv=dbg.beast==2?3:0; data_cores=8; lair_enter(); return 1; }
    if(go==DG_FAR || go==DG_MAP){ dbg.go=0; galaxy_no=4000000000u; u32 s=rnd(); for(int w=0;w<NCW;w++) clus[w]=hmix(w*77+s); u_settle(); gen_cluster(); cur_sys=7+(int)(s%20u); fade(0); arrive_on_planet();
        if(go==DG_MAP){ scene=2; page_back=1; space_enter(1); shx=PR*3; shy=0; shz=0; galaxy_map(); } return 1; }
    return 0;
}
static void dbg_start2(void){                                  // ...and the targets that start on the planet (after the landing)
    int go=dbg.go; if(!go) return; dbg.go=0;
    if(go==DG_NEW || go==DG_CONT){ dbg_unlock_game(); dbg_cheats(); save_game(); return; }
    if(go==DG_PLANET) return;
    if(go==DG_FLOOR){ portal_depth=dbg.floor; enter_dungeon_flag=1; }
    if(go==DG_BOSS){ portal_depth=zone_floors(); enter_dungeon_flag=1; }
    if(go==DG_KART){ k_cls=1; k_type=0; k_trk=dbg.kart%kt_count(); k_rec=-1; kt_official(k_trk,&kt); kt.laps=dbg.laps; kart_go(NRC,-1,0,1); }
    if(go==DG_GARAGE){ if(gold<99999) gold=99999; kgarage(VISPG); planet_hud(); }
    if(go==DG_CREDITS){ fade(1); credits(); planet_enter(); fade(0); }
}

// ---------------------------------------------------------------- live cheats: the flags the game reads (declared in main.c), applied once a frame
#define DBG_GOLD_FLOOR 1000000000LL
static const char *const dbg_spd_nm[5]={"NORMAL","+50%","+100%","+200%","+400%"}; static const u16 dbg_spd_v[5]={0,50,100,200,400};
static int dbg_ncheats(void){ return (dbg_god||xo_i(XO_GOD))+dbg_mp+dbg_food+dbg_gold+dbg_fuel+dbg_ohk+dbg_noclip+(dbg_spd>0)+(dbg_coil>0)+(dbg_sm>0)+dbg_force+dbg_qdone; }
static void dbg_hex(char *b, u32 v){ int n=textlen(b); for(int i=7;i>=0;i--){ int x=(v>>(i*4))&15; b[n++]=(char)(x<10?'0'+x:'A'+x-10); } b[n]=0; }
static void dbg_overlay(void){                                 // INFO > OVERLAY: a line along the bottom of the picture
    if(!dbg_info) return; char b[56]; b[0]=0; volatile u16 *pg=VISPG;
    if(scene==1){ scpy(b,"B"); ncat(b,depth_no); scat(b," X"); ncat(b,px_>>16); scat(b," Y"); ncat(b,py_>>16); }
    else if(scene==0){ scpy(b,"Z"); ncat(b,zone); scat(b," W"); ncat(b,world_no); scat(b," X"); ncat(b,ppx>>16); scat(b," Y"); ncat(b,ppy>>16); }
    else { scpy(b,"SPACE SYS "); ncat(b,cur_sys); }
    if(dbg_info>=2){ scat(b," R"); dbg_hex(b,rng_s); }
    if(dbg_ncheats()){ scat(b," CHEAT "); ncat(b,dbg_ncheats()); }
    rect(pg,0,150,textlen(b)*4+4,9,0); text(pg,2,152,b,dbg_ncheats()?C_GOLD:C_WHITE,1);
}
static void dbg_tick(void){                                    // every frame of the game (run_game)
    if(dbg_god || xo_i(XO_GOD)){ hearts=maxh; food=100; mp=maxmp; hull=100; shield=shield_max(); pet_hp=pet_maxhp; }
    if(dbg_mp) mp=maxmp;
    if(dbg_food) food=100;
    if(dbg_gold && gold<DBG_GOLD_FLOOR) gold=DBG_GOLD_FLOOR;
    if(dbg_fuel){ fuel=fuel_max(); hull=100; shield=shield_max(); }
    dbg_overlay();
}
static void dbg_all_off(void){ dbg_god=dbg_mp=dbg_food=dbg_gold=dbg_fuel=dbg_ohk=dbg_noclip=dbg_spd=0; dbg.spdi=0; dbg_info=0; dbg_sm=dbg_sshow=dbg_force=dbg_qdone=0; if(dbg_coil){ dbg_coil=0; if(!coil_own && shpx.module==MD_COIL){ shpx.module=0; ship_build_model(); coil_cur=cw_u64(0); coil_tier=0; } } }

// ---------------------------------------------------------------- the menu: pages, rows, what each row shows and does
enum { DPG_GAME, DPG_CHEAT, DPG_ITEMS, DPG_WORLD, DPG_SHIP, DPG_GO, DPG_INFO, DPG_TOOLS, NDPG };
static const char *const dm_pgn[NDPG]={"GAME","CHEAT","ITEMS","WORLD","SHIP","GO","INFO","TOOLS"};
enum { DR_NEW, DR_CONT, DR_UNLOCK, DR_SLOT, DR_SAVENOW, DR_UNLOCKALL, DR_DIFF, DR_SLVL, DR_SGOLD, DR_SMAXST, DR_FORM, DR_BODY,
       DR_GOD, DR_INFMP, DR_NOFOOD, DR_INFGOLD, DR_INFFUEL, DR_OHK, DR_NOCLIP, DR_SPEED, DR_ALLOFF, DR_HEAL, DR_LEVEL, DR_ADDGOLD, DR_ADDXP, DR_STATS, DR_POINTS,
       DR_GKIND, DR_GRAR, DR_GLVL, DR_GIVE, DR_GIVE5, DR_GIVEUQ, DR_CONS, DR_GIVEC, DR_CLEARBAG, DR_SPELLS, DR_BUDDIES, DR_RELICS, DR_TOOLS, DR_CORES, DR_SCRAP, DR_ORE,
       DR_HOUR, DR_WEATHER, DR_CLASS, DR_WEIRD, DR_WLV, DR_KILLALL, DR_KILLBOSS, DR_REVEAL, DR_STAIRS,
       DR_COIL, DR_SPDX, DR_SSHOW, DR_FORCE, DR_QDONE, DR_FUZZ, DR_FUEL, DR_REPAIR, DR_FTANK, DR_WARP, DR_FOLD, DR_MAPC, DR_LASER, DR_SHIELD, DR_HOLD, DR_WING, DR_TECH,
       DR_ZONE, DR_FLOOR, DR_GOPLANET, DR_GOFLOOR, DR_GOBOSS, DR_GOSPACE, DR_FLEET, DR_GOFLEET, DR_BEAST, DR_GOBEAST, DR_GOFAR, DR_GOMAP, DR_KART, DR_LAPS, DR_GOKART, DR_GOKW, DR_GARAGE, DR_CREDITS,
       DR_INFO, DR_FPS, DR_HUD, DR_REPORT,
       DR_BUG, DR_MIRROR, DR_HANGAR, DR_SIZE, DR_MUS, DR_SFX, DR_QUIET, NDR };
typedef struct { u8 pg, ctx; char kind; const char *name, *help; } DRow;      // ctx: 1 = title screen, 2 = in a game, 3 = both. kind: A action, V value, B value and action
#define DT 1
#define DL 2
#define DB 3
static const DRow dm_row[NDR]={
 [DR_NEW]={DPG_GAME,DT,'A',"NEW GAME, EVERYTHING UNLOCKED","START A NEW GAME IN THE SLOT BELOW WITH THE CHEATS ON. IT SAVES"},
 [DR_CONT]={DPG_GAME,DT,'A',"CONTINUE THAT SAVE, UNLOCKED","LOAD THE SLOT AND PLAY IT WITH THE CHEATS ON"},
 [DR_UNLOCK]={DPG_GAME,DT,'A',"UNLOCK EVERYTHING IN THAT SAVE","OPEN ALL STARS, FORMS, TOOLS, DRIVES AND RELICS IN THE SLOT. SAVES"},
 [DR_SLOT]={DPG_GAME,DT,'V',"SAVE SLOT","THE SLOT THE ROWS ABOVE USE. A NEW GAME OVERWRITES IT"},
 [DR_SAVENOW]={DPG_GAME,DL,'A',"SAVE NOW","SAVE THE GAME INTO YOUR SLOT, RIGHT NOW"},
 [DR_UNLOCKALL]={DPG_GAME,DL,'A',"UNLOCK EVERYTHING NOW","ALL STARS, FORMS, TOOLS, DRIVES, RELICS, FRIENDS, SPELLS AND THE MODES"},
 [DR_DIFF]={DPG_GAME,DB,'V',"DIFFICULTY","THE SAME SETTING AS IN OPTIONS"},
 [DR_SLVL]={DPG_GAME,DT,'V',"START LEVEL","YOUR LEVEL WHEN THE GAME STARTS"},
 [DR_SGOLD]={DPG_GAME,DT,'V',"START GOLD","YOUR GOLD WHEN THE GAME STARTS"},
 [DR_SMAXST]={DPG_GAME,DT,'V',"START WITH MAX STATS","STRENGTH, SPEED, LIFE AND MAGIC AT 300 WHEN THE GAME STARTS"},
 [DR_FORM]={DPG_GAME,DT,'V',"START AS","A BUG, A HUMANOID OR A CREATURE"},
 [DR_BODY]={DPG_GAME,DT,'V',"CREATURE BODY","WHICH CREATURE, IF YOU START AS ONE"},
 [DR_GOD]={DPG_CHEAT,DB,'V',"GOD MODE","NEVER HURT, NEVER HUNGRY, THE SHIP NEVER BREAKS"},
 [DR_INFMP]={DPG_CHEAT,DB,'V',"INFINITE MANA","SPELLS COST NOTHING: MANA IS ALWAYS FULL"},
 [DR_NOFOOD]={DPG_CHEAT,DB,'V',"NEVER HUNGRY","THE FOOD BAR STAYS FULL"},
 [DR_INFGOLD]={DPG_CHEAT,DB,'V',"INFINITE GOLD","GOLD IS REFILLED TO 1 BILLION WHENEVER IT FALLS BELOW"},
 [DR_INFFUEL]={DPG_CHEAT,DB,'V',"INFINITE FUEL AND HULL","FUEL, HULL AND SHIELD ARE ALWAYS FULL"},
 [DR_OHK]={DPG_CHEAT,DB,'V',"ONE-HIT KILLS","EVERY BITE DOES THE MOST DAMAGE THE GAME ALLOWS"},
 [DR_NOCLIP]={DPG_CHEAT,DB,'V',"NO-CLIP (DUNGEONS)","WALK THROUGH WALLS (NOT OUT OF THE MAP). THE VIEW CAN LOOK ODD"},
 [DR_SPEED]={DPG_CHEAT,DB,'V',"MOVE SPEED","HOW FAST YOU WALK, ON PLANETS AND IN DUNGEONS"},
 [DR_ALLOFF]={DPG_CHEAT,DB,'A',"ALL CHEATS OFF","TURN EVERY LIVE CHEAT ON THIS PAGE AND THE INFO OVERLAY OFF"},
 [DR_HEAL]={DPG_CHEAT,DL,'A',"FULL RESTORE","HEARTS, MANA, FOOD AND BUDDY BACK TO FULL"},
 [DR_LEVEL]={DPG_CHEAT,DL,'V',"LEVEL","SET YOUR LEVEL. HOLD LEFT/RIGHT TO GO FAST. +5 STAT POINTS A LEVEL UP"},
 [DR_ADDGOLD]={DPG_CHEAT,DL,'B',"ADD GOLD","LEFT/RIGHT: THE AMOUNT. A: ADD IT"},
 [DR_ADDXP]={DPG_CHEAT,DL,'B',"ADD XP","LEFT/RIGHT: THE AMOUNT. A: ADD IT (LEVEL UPS HAPPEN FOR REAL)"},
 [DR_STATS]={DPG_CHEAT,DL,'A',"MAX STATS","STRENGTH, SPEED, LIFE AND MAGIC TO 300"},
 [DR_POINTS]={DPG_CHEAT,DL,'A',"+10 STAT POINTS","TEN POINTS TO SPEND ON THE BUG PAGE"},
 [DR_GKIND]={DPG_ITEMS,DL,'V',"GEAR TYPE","WHAT KIND OF GEAR THE GIVE ROWS MAKE"},
 [DR_GRAR]={DPG_ITEMS,DL,'V',"GEAR RARITY","NORMAL, MAGIC, RARE (TWO AFFIXES) OR UNIQUE (A NAMED ONE)"},
 [DR_GLVL]={DPG_ITEMS,DL,'V',"GEAR LEVEL","THE LEVEL OF THE GEAR (1 TO 99). HOLD LEFT/RIGHT TO GO FAST"},
 [DR_GIVE]={DPG_ITEMS,DL,'A',"GIVE THAT GEAR","ONE PIECE OF GEAR WITH THE TYPE, RARITY AND LEVEL ABOVE"},
 [DR_GIVE5]={DPG_ITEMS,DL,'A',"GIVE FIVE OF THEM","FIVE ROLLS OF THE SAME (A DIFFERENT AFFIX EACH TIME)"},
 [DR_GIVEUQ]={DPG_ITEMS,DL,'A',"GIVE ALL 12 UNIQUES","EVERY NAMED UNIQUE: 4 WEAPONS, 4 ARMOURS, 4 CHARMS"},
 [DR_CONS]={DPG_ITEMS,DL,'V',"CONSUMABLE","POTIONS, CRUMBS AND GEMS FOR THE ROW BELOW"},
 [DR_GIVEC]={DPG_ITEMS,DL,'A',"GIVE FIVE CONSUMABLES","FIVE OF THE ONE ABOVE INTO YOUR BAG"},
 [DR_CLEARBAG]={DPG_ITEMS,DL,'A',"EMPTY THE BAG","THROW EVERYTHING IN THE BAG AWAY (ASKS FIRST)"},
 [DR_SPELLS]={DPG_ITEMS,DL,'A',"LEARN ALL SPELLS","FIREBALL, SPARK, HEAL, SHIELD AND PORTAL"},
 [DR_BUDDIES]={DPG_ITEMS,DL,'A',"ALL BUG FRIENDS","FREE EVERY CAGED FRIEND AT ONCE"},
 [DR_RELICS]={DPG_ITEMS,DL,'A',"ALL RELICS","EVERY LEGENDARY RELIC"},
 [DR_TOOLS]={DPG_ITEMS,DL,'A',"ALL TOOLS","EVERY HUMANOID TOOL (YOU STILL NEED TO BE A HUMANOID)"},
 [DR_CORES]={DPG_ITEMS,DL,'A',"ALL 8 DATA CORES","BOLT GETS ALL HIS MEMORIES BACK (8X DAMAGE ON THE BEAST'S CORE)"},
 [DR_SCRAP]={DPG_ITEMS,DL,'A',"+1000 TECH SCRAP","SCRAP FOR THE SHIPYARD AND THE FORGE"},
 [DR_ORE]={DPG_ITEMS,DL,'A',"FILL THE ORE BAG","9999 OF EVERY ORE"},
 [DR_HOUR]={DPG_WORLD,DB,'V',"TIME OF DAY","EVERY NEW WORLD AT THIS HOUR (YOUR WORLD CHANGES WHEN YOU TRAVEL)"},
 [DR_WEATHER]={DPG_WORLD,DB,'V',"WEATHER","EVERY NEW WORLD WITH THIS WEATHER"},
 [DR_CLASS]={DPG_WORLD,DB,'V',"WORLD TYPE","THE WORLD TYPE OF EVERY NEW WORLD"},
 [DR_WEIRD]={DPG_WORLD,DB,'V',"WEIRD TRAIT","A WEIRD TRAIT FOR EVERY NEW WORLD"},
 [DR_WLV]={DPG_WORLD,DB,'V',"TRAIT STRENGTH","HOW STRONG THE TRAIT IS, 1 TO 25"},
 [DR_KILLALL]={DPG_WORLD,DL,'A',"KILL ALL MONSTERS","EVERY MONSTER ON THIS FLOOR DIES (LOOT AND XP COUNT). NOT THE BOSS"},
 [DR_KILLBOSS]={DPG_WORLD,DL,'A',"KILL THE BOSS","THE BOSS ON THIS FLOOR DIES, THE STORY MOVES ON"},
 [DR_REVEAL]={DPG_WORLD,DL,'A',"REVEAL THE FLOOR MAP","THE WHOLE FLOOR GOES ON YOUR MAP"},
 [DR_STAIRS]={DPG_WORLD,DL,'A',"TELEPORT TO THE STAIRS","STAND ON THE STAIRS YOU ARE LOOKING FOR. PRESS A THERE"},
 [DR_COIL]={DPG_SHIP,DL,'V',"CHEAT: STORMCLOCK COIL","LEND YOURSELF THE COIL FOR THIS SESSION ONLY. FIT IT AT THE HANGAR. NEVER SAVED"},
 [DR_SPDX]={DPG_SHIP,DB,'V',"CHEAT: SHIP SPEED","MULTIPLIES YOUR SPEED, WITH OR WITHOUT THE COIL: X2 TO X1 MILLION (PAST STORM AT THE TOP). MARKED CHEAT"},
 [DR_SSHOW]={DPG_SHIP,DB,'V',"CHEAT: SPEED READOUT","SHOW YOUR SPEED AS TIMES THE SPEED OF LIGHT, WITH TRAVEL TIMES"},
 [DR_FORCE]={DPG_SHIP,DL,'V',"CHEAT: FORCE THE RARE ROLL","THE COIL DROP IS CERTAIN, BUT STILL ONLY WHEN THE QUEST IS DONE AND ALL SIX CONDITIONS HOLD"},
 [DR_QDONE]={DPG_SHIP,DL,'V',"CHEAT: QUEST DONE","COUNT THE COIL QUEST AS DONE FOR THIS SESSION. NEVER SAVED"},
 [DR_FUZZ]={DPG_SHIP,DL,'A',"TEST: GLITCH FUZZ","FLY ON RANDOM KEYS THROUGH EVERY SPEED TIER. ONLY IN SPACE"},
 [DR_FUEL]={DPG_SHIP,DL,'A',"REFUEL","FUEL TO THE TOP"},
 [DR_REPAIR]={DPG_SHIP,DL,'A',"REPAIR THE SHIP","HULL AND SHIELD TO FULL"},
 [DR_FTANK]={DPG_SHIP,DL,'V',"FUEL TANK","SHIPYARD LEVEL, 0 TO 8"},
 [DR_WARP]={DPG_SHIP,DL,'V',"WARP DRIVE","SHIPYARD LEVEL, 0 TO 8 (MK 8 REACHES ANYWHERE)"},
 [DR_FOLD]={DPG_SHIP,DL,'V',"FOLD DRIVE","SHIPYARD LEVEL, 0 TO 6"},
 [DR_MAPC]={DPG_SHIP,DL,'V',"MAP COMPUTER","SHIPYARD LEVEL, 0 TO 5"},
 [DR_LASER]={DPG_SHIP,DL,'V',"LASER","SHIPYARD LEVEL, 1 TO 4"},
 [DR_SHIELD]={DPG_SHIP,DL,'V',"SHIELD","SHIPYARD LEVEL, 1 TO 8"},
 [DR_HOLD]={DPG_SHIP,DL,'V',"CARGO HOLD","HOW MUCH THE HOLD CARRIES"},
 [DR_WING]={DPG_SHIP,DL,'V',"WINGMEN","HOW MANY SHIPS FLY WITH YOU"},
 [DR_TECH]={DPG_SHIP,DL,'V',"TECH CORES","EACH ONE MAKES YOUR LASER 4X. 0 TO 3"},
 [DR_ZONE]={DPG_GO,DT,'V',"ZONE","THE ZONE FOR THE GO ROWS BELOW"},
 [DR_FLOOR]={DPG_GO,DB,'V',"FLOOR","THE DUNGEON FLOOR TO GO TO (B1 = 1)"},
 [DR_GOPLANET]={DPG_GO,DT,'A',"GO: THAT ZONE'S PLANET","LAND ON THE ZONE'S PLANET, THE STORY AS IT WAS THEN"},
 [DR_GOFLOOR]={DPG_GO,DB,'A',"GO: THAT DUNGEON FLOOR","STRAIGHT DOWN TO THAT FLOOR (FROM A PLANET)"},
 [DR_GOBOSS]={DPG_GO,DB,'A',"GO: THE BOSS FLOOR","STRAIGHT TO THE BOSS AT THE BOTTOM (FROM A PLANET)"},
 [DR_GOSPACE]={DPG_GO,DB,'A',"GO: SPACE","UP IN YOUR SHIP (FROM A PLANET)"},
 [DR_FLEET]={DPG_GO,DT,'V',"ENEMY SHIPS","HOW MANY ENEMY SHIPS, 1 TO 50"},
 [DR_GOFLEET]={DPG_GO,DT,'A',"GO: A FLEET BATTLE","A FLEET BATTLE WITH A FULL WING"},
 [DR_BEAST]={DPG_GO,DT,'V',"BEAST STAGE","HATCHLING, GROWN, OR THE COMPLETE 1 TRILLION HP BEAST"},
 [DR_GOBEAST]={DPG_GO,DT,'A',"GO: THE TECHNOBEAST","INTO ITS LAIR"},
 [DR_GOFAR]={DPG_GO,DT,'A',"GO: A FAR WORLD","A RANDOM WORLD FAR OUT IN THE GALAXY"},
 [DR_GOMAP]={DPG_GO,DT,'A',"GO: THE GALAXY MAP","DEEP SPACE AT THE MAP. START: CLUSTER AND UNIVERSE JUMPS"},
 [DR_KART]={DPG_GO,DT,'V',"KART TRACK","WHICH OF THE PLANET'S TRACKS"},
 [DR_LAPS]={DPG_GO,DT,'V',"LAPS","HOW MANY LAPS"},
 [DR_GOKART]={DPG_GO,DT,'A',"GO: A KART RACE","A RACE ON THE GARDEN"},
 [DR_GOKW]={DPG_GO,DT,'A',"GO: KART WORLD","THE KART RACING GAME"},
 [DR_GARAGE]={DPG_GO,DT,'A',"GO: THE KART GARAGE","BUILD A KART (ON THE PLANET)"},
 [DR_CREDITS]={DPG_GO,DT,'A',"GO: THE CREDITS","ROLL THE CREDITS"},
 [DR_INFO]={DPG_INFO,DB,'V',"INFO OVERLAY","A LINE ALONG THE BOTTOM: WHERE YOU ARE. ENGINE ADDS THE RANDOM SEED"},
 [DR_FPS]={DPG_INFO,DB,'V',"SHOW FPS","A FRAME COUNTER IN THE CORNER"},
 [DR_HUD]={DPG_INFO,DB,'V',"HIDE THE HUD","NO HUD: ONLY THE WORLD (FOR SCREENSHOTS)"},
 [DR_REPORT]={DPG_INFO,DB,'A',"STATE REPORT","A LIST OF WHAT THE GAME IS DOING: SCENE, SEEDS, MEMORY, TIME"},
 [DR_BUG]={DPG_TOOLS,DT,'A',"THE BUG MAKER","A: MAKE A BUG"},
 [DR_MIRROR]={DPG_TOOLS,DT,'A',"THE MIRROR (HUMANOID)","A: MAKE A HUMANOID"},
 [DR_HANGAR]={DPG_TOOLS,DT,'A',"THE SHIP HANGAR","A: BUILD A SHIP"},
 [DR_SIZE]={DPG_TOOLS,DT,'A',"HOW BIG IS IT?","THE GAME'S HUGE NUMBERS"},
 [DR_MUS]={DPG_TOOLS,DB,'B',"MUSIC","LEFT/RIGHT: PICK A TUNE. A: PLAY IT"},
 [DR_SFX]={DPG_TOOLS,DB,'B',"SOUND EFFECT","LEFT/RIGHT: PICK A SOUND. A: PLAY IT"},
 [DR_QUIET]={DPG_TOOLS,DB,'A',"STOP THE MUSIC","QUIET"},
};
static int dm_wrap(int v, int d, int lo, int hi){ v+=d; if(v<lo) return hi; if(v>hi) return lo; return v; }
static int dm_saveable(void){ return !(scene==2 || (scene==1 && (forge_on || base_mode))); }
static const char *dm_need(int id){                            // why a row cannot be used right now (0 = it can)
    switch(id){
    case DR_FUZZ: return scene==2 ? 0 : "ONLY IN SPACE";
    case DR_KILLALL: case DR_KILLBOSS: case DR_REVEAL: case DR_STAIRS: return scene==1 ? 0 : "ONLY DOWN IN A DUNGEON";
    case DR_GOFLOOR: case DR_GOBOSS: case DR_GOSPACE: return dbg_live_ctx && scene!=0 ? "ONLY ON A PLANET" : 0;
    case DR_SAVENOW: return dm_saveable() ? 0 : "YOU CAN'T SAVE HERE. TRY ON A PLANET";
    default: return 0; }
}
static const char *const dbg_cons_nm[7]={"HEALTH POTION","MANA POTION","CRUMB","RUBY","SAPPHIRE","EMERALD","DIAMOND"};
static const char *const dbg_gk_nm[3]={"THORN (WEAPON)","SHELL (ARMOUR)","CHARM"}; static const u8 dbg_gk_v[3]={IT_THORN,IT_SHELL,IT_CHARM};
static const char *const dbg_gr_nm[4]={"NORMAL","MAGIC","RARE","UNIQUE"};
static const char *const dbg_ga_nm[6]={"1,000","1 MILLION","1 BILLION","1 TRILLION","1 QUADRILLION","1 QUINTILLION"};
static const s64 dbg_ga_v[6]={1000LL,1000000LL,1000000000LL,1000000000000LL,1000000000000000LL,1000000000000000000LL};
static const char *const dbg_xa_nm[5]={"100","10,000","1 MILLION","100 MILLION","1 BILLION"}; static const int dbg_xa_v[5]={100,10000,1000000,100000000,1000000000};
static const u16 dbg_hold_v[5]={10,50,200,500,2000};
static Gear dbg_make_gear(void){
    int kind=dbg_gk_v[dbg.gk]; Gear g=roll_gear(kind,dbg.gl,0); g.rar=(u8)dbg.gr; g.pre=g.suf=0;
    if(dbg.gr==R_MAGIC){ if(rndi(2)) g.pre=1+rndi(6); else g.suf=1+rndi(7); }
    else if(dbg.gr==R_RARE){ g.pre=1+rndi(6); g.suf=1+rndi(7); }
    else if(dbg.gr==R_UNIQUE){ int n=0, pick[NUNIQ]; for(int i=0;i<NUNIQ;i++) if(uq_it[i]==kind) pick[n++]=i; g.sub=(u8)pick[rndi(n)]; }
    return g; }
static Gear dbg_cons_item(int c){ Gear g={0,0,0,0,0,0}; if(c<2){ g.it=IT_POTION; g.sub=(u8)c; } else if(c==2) g.it=IT_CRUMB; else { g.it=IT_GEM; g.sub=(u8)(c-3); } return g; }
// what a row shows on its right: returns the colour (0 = nothing to show)
static int dm_val(int id, char *b){
    static const char *const dif[6]={"EASY","NORMAL","HARD","BRUTAL","NIGHTMARE","INSANE-SECT"}, *const bd[3]={"SLUG","FROG","BAT"}, *const hr[3]={"DAY","DUSK","NIGHT"}, *const ov[3]={"OFF","POSITION","ENGINE"};
    int on=-1; b[0]=0;
    switch(id){
    case DR_SLOT: scpy(b,"SLOT "); ncat(b,dbg.slot+1); scat(b, sinf[dbg.slot].ok ? " (USED)" : " (EMPTY)"); return C_GOLD;
    case DR_DIFF: scpy(b,dif[opt_diff]); return C_GOLD;
    case DR_SLVL: scpy(b,dbg_lvl_nm[dbg.lvl]); return C_GOLD;   case DR_SGOLD: scpy(b,dbg_gold_nm[dbg.gold]); return C_GOLD;
    case DR_SMAXST: on=dbg.maxst; break;   case DR_FORM: scpy(b,dbg_form_nm[dbg.form]); return C_GOLD;   case DR_BODY: scpy(b,bd[dbg.body]); return C_GOLD;
    case DR_GOD: on=dbg_god; break;   case DR_INFMP: on=dbg_mp; break;   case DR_NOFOOD: on=dbg_food; break;   case DR_INFGOLD: on=dbg_gold; break;
    case DR_INFFUEL: on=dbg_fuel; break;   case DR_OHK: on=dbg_ohk; break;   case DR_NOCLIP: on=dbg_noclip; break;
    case DR_SPEED: scpy(b,dbg_spd_nm[dbg.spdi]); return dbg.spdi?C_GREEN:C_GREY;
    case DR_COIL: on=dbg_coil; break;   case DR_SPDX: scpy(b,dbg_sm_nm[dbg_sm]); return dbg_sm?C_GOLD:C_GREY;   case DR_SSHOW: on=dbg_sshow; break;   case DR_FORCE: on=dbg_force; break;   case DR_QDONE: on=dbg_qdone; break;
    case DR_LEVEL: ncat(b,level); return C_GOLD;
    case DR_ADDGOLD: scpy(b,dbg_ga_nm[dbg.ga]); return C_GOLD;   case DR_ADDXP: scpy(b,dbg_xa_nm[dbg.xa]); return C_GOLD;
    case DR_GKIND: scpy(b,dbg_gk_nm[dbg.gk]); return C_GOLD;   case DR_GRAR: scpy(b,dbg_gr_nm[dbg.gr]); return C_GOLD;   case DR_GLVL: ncat(b,dbg.gl); return C_GOLD;
    case DR_CONS: scpy(b,dbg_cons_nm[dbg.cons]); return C_GOLD;
    case DR_HOUR: scpy(b, dbg_hour<0 ? "AS IS" : hr[dbg_hour]); return C_GOLD;   case DR_WEATHER: scpy(b, dbg_weather<0 ? "AS IS" : weather_name[dbg_weather]); return C_GOLD;
    case DR_CLASS: scpy(b, dbg_cls<0 ? "ANY" : pc_name[dbg_cls]); return C_GOLD;   case DR_WEIRD: scpy(b, dbg_weird<0 ? "OFF" : mw_name[dbg_weird%NMW]); return C_GOLD;
    case DR_WLV: if(dbg_weird>=0){ ncat(b,dbg_weird/NMW+1); scat(b," "); scat(b,mw_adj[dbg_weird/NMW]); } else scpy(b,"-"); return C_GOLD;
    case DR_FTANK: ncat(b,fuel_tank); return C_GOLD;   case DR_WARP: ncat(b,warp_lv); return C_GOLD;   case DR_FOLD: ncat(b,fold_lv); return C_GOLD;
    case DR_MAPC: ncat(b,map_lv); return C_GOLD;   case DR_LASER: ncat(b,laser_lv); return C_GOLD;   case DR_SHIELD: ncat(b,shield_lv); return C_GOLD;
    case DR_HOLD: ncat(b,cargo_cap); return C_GOLD;   case DR_WING: ncat(b,wing_n); return C_GOLD;   case DR_TECH: ncat(b,tech_lv); return C_GOLD;
    case DR_ZONE: scpy(b,tint_name[dbg.zone]); return C_GOLD;   case DR_FLOOR: scpy(b,"B"); ncat(b,dbg.floor); return C_GOLD;
    case DR_FLEET: ncat(b,dbg.fleet); return C_GOLD;   case DR_BEAST: scpy(b,dbg_beast_nm[dbg.beast]); return C_GOLD;
    case DR_KART: ncat(b,dbg.kart); return C_GOLD;   case DR_LAPS: ncat(b,dbg.laps); return C_GOLD;
    case DR_INFO: scpy(b,ov[dbg_info]); return dbg_info?C_GREEN:C_GREY;
    case DR_FPS: on=dbg_fps; break;   case DR_HUD: on=film_mode; break;
    case DR_MUS: scpy(b,dbg_mus_nm[dbg.mus]); return C_GOLD;   case DR_SFX: ncat(b,dbg.sfxn); return C_GOLD;
    default: return 0; }
    scpy(b, on?"ON":"OFF"); return on?C_GREEN:C_GREY;
}
// LEFT/RIGHT (dd = -1/+1, mul = how big a step on numbers) or A on a value row. Returns 1 if the row's value changed
static void dm_change(int id, int dd, int mul){
    switch(id){
    case DR_SLOT: dbg.slot=(dbg.slot+dd+NSLOT)%NSLOT; break;
    case DR_DIFF: opt_diff=(opt_diff+dd+6)%6; opt_easy=(opt_diff==0); break;
    case DR_SLVL: dbg.lvl=(dbg.lvl+dd+5)%5; break;   case DR_SGOLD: dbg.gold=(dbg.gold+dd+5)%5; break;
    case DR_SMAXST: dbg.maxst^=1; break;   case DR_FORM: dbg.form=(dbg.form+dd+3)%3; break;   case DR_BODY: dbg.body=(dbg.body+dd+3)%3; break;
    case DR_GOD: dbg_god^=1; break;   case DR_INFMP: dbg_mp^=1; break;   case DR_NOFOOD: dbg_food^=1; break;   case DR_INFGOLD: dbg_gold^=1; break;
    case DR_INFFUEL: dbg_fuel^=1; break;   case DR_OHK: dbg_ohk^=1; break;   case DR_NOCLIP: dbg_noclip^=1; break;
    case DR_SPEED: dbg.spdi=(dbg.spdi+dd+5)%5; dbg_spd=dbg_spd_v[dbg.spdi]; break;
    case DR_COIL: dbg_coil^=1; if(dbg_coil){ shpx.module=MD_COIL; ship_build_model(); } else if(!coil_own && shpx.module==MD_COIL){ shpx.module=0; ship_build_model(); coil_cur=cw_u64(0); coil_tier=0; } break;
    case DR_SPDX: dbg_sm=(dbg_sm+dd+6)%6; break;   case DR_SSHOW: dbg_sshow^=1; break;   case DR_FORCE: dbg_force^=1; break;   case DR_QDONE: dbg_qdone^=1; break;
    case DR_LEVEL: { s64 nl=level+(s64)dd*mul; if(nl<1) nl=1; if(nl>99999) nl=99999; if(nl>level) stat_pts=sadd(stat_pts,(nl-level)*5);
        level=nl; xp=0; recalc(); hearts=maxh; mp=maxmp; hud_now(); } break;
    case DR_ADDGOLD: dbg.ga=(dbg.ga+dd+6)%6; break;   case DR_ADDXP: dbg.xa=(dbg.xa+dd+5)%5; break;
    case DR_GKIND: dbg.gk=(dbg.gk+dd+3)%3; break;   case DR_GRAR: dbg.gr=(dbg.gr+dd+4)%4; break;
    case DR_GLVL: dbg.gl+=dd*mul; if(dbg.gl<1) dbg.gl=1; if(dbg.gl>99) dbg.gl=99; break;
    case DR_CONS: dbg.cons=(dbg.cons+dd+7)%7; break;
    case DR_HOUR: dbg_hour = dbg_hour+dd<-1 ? 2 : dbg_hour+dd>2 ? -1 : dbg_hour+dd; break;
    case DR_WEATHER: dbg_weather = dbg_weather+dd<-1 ? 5 : dbg_weather+dd>5 ? -1 : dbg_weather+dd; break;
    case DR_CLASS: dbg_cls = dbg_cls+dd<-1 ? NPC-1 : dbg_cls+dd>=NPC ? -1 : dbg_cls+dd; break;
    case DR_WEIRD: { int k = dbg_weird<0 ? -1 : dbg_weird%NMW, lv = dbg_weird<0 ? 21 : dbg_weird/NMW; k += dd; if(k<-1) k=NMW-1; if(k>=NMW) k=-1; dbg_weird = k<0 ? -1 : lv*NMW+k; } break;
    case DR_WLV: if(dbg_weird>=0){ int k=dbg_weird%NMW, lv=(dbg_weird/NMW+dd+25)%25; dbg_weird=lv*NMW+k; } break;
    case DR_FTANK: fuel_tank=dm_wrap(fuel_tank,dd,0,8); if(fuel>fuel_max()) fuel=fuel_max(); break;
    case DR_WARP: warp_lv=dm_wrap(warp_lv,dd,0,8); break;   case DR_FOLD: fold_lv=dm_wrap(fold_lv,dd,0,6); break;   case DR_MAPC: map_lv=dm_wrap(map_lv,dd,0,5); break;
    case DR_LASER: laser_lv=dm_wrap(laser_lv,dd,1,4); break;   case DR_SHIELD: shield_lv=dm_wrap(shield_lv,dd,1,8); if(shield>shield_max()) shield=shield_max(); break;
    case DR_HOLD: { int k=0; for(int i=0;i<5;i++) if(dbg_hold_v[i]<=cargo_cap) k=i; cargo_cap=dbg_hold_v[(k+dd+5)%5]; } break;
    case DR_WING: wing_n=dm_wrap(wing_n,dd,0,MAXWING); break;   case DR_TECH: tech_lv=dm_wrap(tech_lv,dd,0,3); break;
    case DR_ZONE: dbg.zone=(dbg.zone+dd+6)%6; break;   case DR_FLOOR: dbg.floor=dm_wrap(dbg.floor,dd*mul,1,30); break;
    case DR_FLEET: dbg.fleet=dm_wrap(dbg.fleet,dd*mul,1,50); break;   case DR_BEAST: dbg.beast=(dbg.beast+dd+3)%3; break;
    case DR_KART: dbg.kart=(dbg.kart+dd+10)%10; break;   case DR_LAPS: dbg.laps=dm_wrap(dbg.laps,dd,1,9); break;
    case DR_INFO: dbg_info=(dbg_info+dd+3)%3; break;   case DR_FPS: dbg_fps^=1; break;   case DR_HUD: film_mode^=1; break;
    case DR_MUS: dbg.mus=(dbg.mus+dd+8)%8; break;   case DR_SFX: dbg.sfxn=(dbg.sfxn+dd+15)%15; break;
    default: break; }
}

// A on a row. note gets a line for the screen. Returns 0 = stay, 1 = close everything and play, 2 = Kart World, 3 = start the game the title menu set up
static int dm_act(volatile u16 *vis, int id, int live, char *note){
    int go=0; const char *nd=dm_need(id); if(nd){ scpy(note,nd); sfx(12); return 0; }
    switch(id){
    case DR_NEW: go=DG_NEW; break;
    case DR_CONT: if(sinf[dbg.slot].ok) go=DG_CONT; else scpy(note,"THAT SLOT IS EMPTY"); break;
    case DR_UNLOCK: if(!sinf[dbg.slot].ok) scpy(note,"THAT SLOT IS EMPTY"); else { cur_slot=dbg.slot; if(load_game(1)){ dbg_unlock_game(); do_save(); scpy(note,"DONE: THAT SAVE HAS EVERYTHING OPEN"); } else scpy(note,"THAT SAVE WOULD NOT LOAD"); new_game(); } break;
    case DR_SAVENOW: do_save(); scpy(note,"SAVED TO SLOT "); ncat(note,cur_slot+1); break;
    case DR_UNLOCKALL: dbg_unlock_game(); dbg_unlock_modes(); hud_now(); scpy(note,"EVERYTHING IS OPEN. SAVE NOW TO KEEP IT"); break;
    case DR_ALLOFF: dbg_all_off(); scpy(note,"ALL CHEATS OFF"); break;
    case DR_FUZZ: { int n=coil_fuzz(500); scpy(note,"FUZZ OK: "); ncat(note,n); scat(note," FRAMES"); } break;
    case DR_HEAL: recalc(); hearts=maxh; mp=maxmp; food=100; pet_hp=pet_maxhp; hud_now(); scpy(note,"FULLY RESTORED"); break;
    case DR_ADDGOLD: gold=sadd(gold,dbg_ga_v[dbg.ga]); hud_now(); scpy(note,"GOLD: "); ncat(note,gold); break;
    case DR_ADDXP: gain_xp(dbg_xa_v[dbg.xa]); scpy(note,"LEVEL "); ncat(note,level); break;
    case DR_STATS: st_str=st_dex=st_vit=st_mag=300; recalc(); hearts=maxh; mp=maxmp; hud_now(); scpy(note,"ALL STATS AT 300"); break;
    case DR_POINTS: stat_pts=sadd(stat_pts,10); scpy(note,"STAT POINTS: "); ncat(note,stat_pts); break;
    case DR_GIVE: case DR_GIVE5: { int n=id==DR_GIVE?1:5, got=0; for(int i=0;i<n;i++) got+=add_gear(dbg_make_gear()); scpy(note,"GAVE "); ncat(note,got); scat(note, got==n ? " PIECE" : " (BAG FULL)"); } break;
    case DR_GIVEUQ: { int got=0; for(int i=0;i<NUNIQ;i++){ Gear g={(u8)uq_it[i],(u8)i,R_UNIQUE,0,0,(u8)dbg.gl}; got+=add_gear(g); } scpy(note,"GAVE "); ncat(note,got); scat(note," UNIQUES"); } break;
    case DR_GIVEC: { int got=0; for(int i=0;i<5;i++) got+=add_gear(dbg_cons_item(dbg.cons)); scpy(note,"GAVE "); ncat(note,got); break; }
    case DR_CLEARBAG: if(confirm(vis,"EMPTY THE BAG?","EVERYTHING IN IT IS GONE")){ bag_n=0; bag_sel=0; hud_now(); scpy(note,"THE BAG IS EMPTY"); } break;
    case DR_SPELLS: for(int i=0;i<NSPELL;i++) spell_known[i]=1; scpy(note,"ALL SPELLS KNOWN"); break;
    case DR_BUDDIES: for(int i=0;i<6;i++) have_bug[i]=1; recalc(); hearts=maxh; hud_now(); scpy(note,"ALL FRIENDS ARE WITH YOU"); break;
    case DR_RELICS: relic_bits=0xFFFF; recalc(); scpy(note,"ALL RELICS"); break;
    case DR_TOOLS: tools_owned=0xFF; scpy(note,"ALL TOOLS"); break;
    case DR_CORES: data_cores=8; scpy(note,"8 DATA CORES"); break;
    case DR_SCRAP: scrap+=1000; scpy(note,"TECH SCRAP: "); ncat(note,scrap); break;
    case DR_ORE: for(int i=0;i<NORE;i++) ore_n[i]=9999; scpy(note,"THE ORE BAG IS FULL"); break;
    case DR_KILLALL: case DR_KILLBOSS: { int n=0, bo=id==DR_KILLBOSS; for(int i=0;i<ndo;i++){ DObj *o=&dob[i]; if(!o->alive || !is_mon(o->kind) || (o->kind==K_BOSS)!=bo) continue; o->hp=0; kill_mon(o); n++; }
        scpy(note,"KILLED "); ncat(note,n); } break;
    case DR_REVEAL: for(int i=0;i<MW*MW;i++) seen[i]=1; for(int i=0;i<10;i++) room_seen[i]=1; scpy(note,"THE WHOLE FLOOR IS ON YOUR MAP"); break;
    case DR_STAIRS: px_=tgt_x; py_=tgt_y; scpy(note,"YOU ARE ON THE STAIRS. CLOSE THIS AND PRESS A"); break;
    case DR_FUEL: fuel=fuel_max(); scpy(note,"FUEL FULL"); break;
    case DR_REPAIR: hull=100; shield=shield_max(); scpy(note,"THE SHIP IS LIKE NEW"); break;
    case DR_GOPLANET: go=DG_PLANET; break;   case DR_GOFLEET: go=DG_FLEET; break;   case DR_GOBEAST: go=DG_BEAST; break;
    case DR_GOFAR: go=DG_FAR; break;   case DR_GOMAP: go=DG_MAP; break;   case DR_GOKART: go=DG_KART; break;   case DR_GARAGE: go=DG_GARAGE; break;   case DR_CREDITS: go=DG_CREDITS; break;
    case DR_GOFLOOR: case DR_GOBOSS: case DR_GOSPACE:
        if(live){ if(id==DR_GOSPACE) launch_flag=1; else { portal_depth = id==DR_GOFLOOR ? dbg.floor : zone_floors(); enter_dungeon_flag=1; } return 1; }
        go = id==DR_GOFLOOR ? DG_FLOOR : id==DR_GOBOSS ? DG_BOSS : DG_SPACE; break;
    case DR_MUS: music(dbg.mus); break;   case DR_SFX: sfx(dbg.sfxn); break;   case DR_QUIET: music(-1); break;
    default: break; }
    if(go){ dbg.go=go; return 3; }
    return 0;
}
// ---------------------------------------------------------------- the state report: one line per fact, UP/DOWN to scroll
#define DM_NLINES 17
static int dm_line(int i, char *b){
    static const char *const sc[3]={"PLANET","DUNGEON","SPACE"}; b[0]=0;
    switch(i){
    case 0: scpy(b,"SCENE "); scat(b,sc[scene>2?0:scene]); scat(b,"  ZONE "); ncat(b,zone); scat(b,"  FLOOR "); ncat(b,depth_no); break;
    case 1: scpy(b,"WORLD "); ncat(b,world_no); scat(b, endless?"  ENDLESS":"  STORY"); break;
    case 2: scpy(b,"LEVEL "); ncat(b,level); scat(b,"  XP "); ncat(b,xp); scat(b,"/"); ncat(b,xp_next()); break;
    case 3: scpy(b,"HEARTS "); ncat(b,hearts); scat(b,"/"); ncat(b,maxh); break;
    case 4: scpy(b,"MANA "); ncat(b,mp); scat(b,"/"); ncat(b,maxmp); scat(b,"  FOOD "); ncat(b,food); break;
    case 5: scpy(b,"GOLD "); ncat(b,gold); break;
    case 6: scpy(b,"BAG "); ncat(b,bag_n); scat(b,"/"); ncat(b,BAGMAX); scat(b,"  STAT POINTS "); ncat(b,stat_pts); break;
    case 7: scpy(b,"SLOT "); ncat(b,cur_slot+1); scat(b,"  PLAYED "); { char t[16]; fmt_time(t,play_secs()); scat(b,t); } break;
    case 8: scpy(b,"FRAMES "); ncat(b,frames); scat(b,"  VBLANKS "); ncat(b,vbl_count); break;
    case 9: scpy(b,"GALAXY "); ncat(b,galaxy_no); scat(b,"  CLUSTER JUMPS "); ncat(b,cluster_no); break;
    case 10: scpy(b,"SYSTEM "); ncat(b,cur_sys); scat(b,"  PLANET "); ncat(b,cur_pl); scat(b,"  MOON "); ncat(b,cur_moon); break;
    case 11: scpy(b,"FUEL "); ncat(b,fuel); scat(b,"/"); ncat(b,fuel_max()); scat(b,"  HULL "); ncat(b,hull); break;
    case 12: scpy(b,"RANDOM SEED "); dbg_hex(b,rng_s); break;
    case 13: { extern u32 __bss_end; u32 sp; __asm__ volatile("mov %0, sp":"=r"(sp)); scpy(b,"STACK FREE "); ncat(b,(s64)(sp-(u32)&__bss_end)); scat(b," BYTES"); } break;
    case 14: scpy(b,"LIVE CHEATS ON: "); ncat(b,dbg_ncheats()); break;
    case 15: scpy(b,"DIFFICULTY "); ncat(b,opt_diff); scat(b,"  BUG "); scat(b,bug_name); break;
    case 16: scpy(b,"BUILD "); scat(b,__DATE__); scat(b," "); scat(b,__TIME__); break;
    default: return 0; }
    return 1;
}
// ---------------------------------------------------------------- the menu itself. live: opened in a game from the pause screen. 0: back. 1: close all and play. 2: Kart World. 3: start what the title menu set up
static int dm_run(volatile u16 *pg, int live){
    volatile u16 *pg0=(volatile u16*)0x06000000, *pg1=(volatile u16*)0x0600A000; (void)pg;
    int orig=(REG_DISPCNT>>4)&1, show=orig, sel=0, top=0, dirty=1, pgi=0, hold=0, rep=0, report=0, rtop=0; char note[64]; note[0]=0;
    dbg_live_ctx=live; dbg_on=1; sfx(6);
    if(!live){ slots_refresh(); if(dbg.slot<0){ dbg.slot=2; for(int i=0;i<NSLOT;i++) if(!sinf[i].ok){ dbg.slot=i; break; } } dbg_unlock_modes(); scpy(note,"OPEN: KART WORLD, HOW BIG IS IT?, INSANE-SECT"); REG_BLDCNT=0; REG_BLDY=0; }
    else { volatile u16 *s=show?pg1:pg0, *d=show?pg0:pg1; for(int i=0;i<19200;i++) d[i]=s[i]; in_menu++; }
    wait_release(); oam_hide_all(); oam_flush();
    #define DM_SHOW0() do{ for(int i_=0;i_<120*160;i_++) pg0[i_]=0; REG_DISPCNT&=~(1<<4); show=0; }while(0)   /* (the makers draw on page 0: clear it and show it) */
    int ctxm = live ? DL : DT;
    while(1){
        int vis[NDR], nv=0, pgs[NDPG], np=0;                    // the pages with something on them here, and this page's rows
        for(int p=0;p<NDPG;p++){ int any=0; for(int i=0;i<NDR;i++) if(dm_row[i].pg==p && (dm_row[i].ctx&ctxm)){ any=1; break; } if(any) pgs[np++]=p; }
        if(pgi>=np) pgi=np-1;
        for(int i=0;i<NDR;i++) if(dm_row[i].pg==pgs[pgi] && (dm_row[i].ctx&ctxm)) vis[nv++]=i;
        if(sel>=nv) sel=nv-1; if(sel<top) top=sel; if(sel>=top+8) top=sel-7;
        int id=vis[sel];
        if(dirty){ dirty=0; volatile u16 *hid = show ? pg0 : pg1; char b[64];
            if(!live) ui_bg(hid,0,160); else ui_bg(hid,18,149);
            if(!live){ rect(hid,0,0,240,17,U_BG0); rect(hid,0,17,240,1,U_RAISE); stext(hid,6,6,"DEBUG MENU",C_GOLD); rtext(hid,234,6,"TITLE SCREEN",U_SOFT); }
            rect(hid,0,18,240,17,U_BG0); rect(hid,0,35,240,1,U_RAISE);       // the tab bar
            for(int t=0;t<np;t++){ int x=t*240/np, w=240/np-1, on=t==pgi; if(on){ rect(hid,x,19,w,17,U_PANEL); rect(hid,x+1,19,w-2,2,C_CYAN); rect(hid,x+1,21,w-2,1,U_SHINE); }
                ctext(hid,x+w/2,25,dm_pgn[pgs[t]], on?C_GOLD:U_GLOW); }
            if(report){
                ui_panel(hid,4,38,232,108); ui_head(hid,9,41,222,"STATE REPORT",C_CYAN);
                int n=0, y=52; for(int i=rtop;i<DM_NLINES && n<10;i++,n++,y+=9){ dm_line(i,b); b[56]=0; text(hid,10,y,b,i&1?U_SOFT:C_WHITE,1); }
                if(rtop>0) text(hid,228,52,"^",C_WHITE,1); if(rtop+10<DM_NLINES) text(hid,228,133,"v",C_WHITE,1);
                ui_foot(hid,"UP/DOWN: SCROLL","B: BACK"); }
            else {
            ui_panel(hid,4,38,232,76);
            for(int r=0;r<8;r++){ int k=top+r, y=42+r*9; if(k>=nv) break; int rid=vis[k]; const DRow *R=&dm_row[rid]; int bad=dm_need(rid)!=0;
                if(k==sel) ui_sel(hid,7,y-2,226,10,U_SEL);
                text(hid,12,y,R->name, bad?C_DGREY : k==sel?C_WHITE : R->kind=='A'?C_CYAN : U_SOFT, 1);
                int c=dm_val(rid,b); if(c){ char v[40]; b[22]=0; scpy(v,"< "); scat(v,b); scat(v," >"); rtext(hid,228,y,v, bad?C_DGREY : k==sel?C_GOLD:c); } }
            if(top>0) text(hid,229,42,"^",C_WHITE,1); if(top+8<nv) text(hid,229,104,"v",C_WHITE,1);
            ui_panel(hid,4,116,232,31);
            { const char *h=dm_row[id].help; const char *nd=dm_need(id); if(nd) h=nd; int L=textlen(h);
              if(L>56){ char h1[60]; int k=0; while(k<56 && h[k]) k++; while(k>0 && h[k]!=' ') k--; for(int j=0;j<k;j++) h1[j]=h[j]; h1[k]=0; text(hid,10,119,h1,nd?C_GOLD:C_GREY,1); text(hid,10,127,h+k+1,nd?C_GOLD:C_GREY,1); }
              else text(hid,10,119,h,nd?C_GOLD:C_GREY,1); }
            if(note[0]){ char n2[60]; scpy(n2,note); n2[58]=0; text(hid,10,137,n2,C_GREEN,1); }
            else { scpy(b,"CHEATS ON: "); ncat(b,dbg_ncheats()); text(hid,10,137,b,dbg_ncheats()?C_GOLD:C_DGREY,1); }
            ui_foot(hid, dm_row[id].kind=='A' ? "A: DO IT" : dm_row[id].kind=='B' ? "A: DO IT  LEFT/RIGHT: CHANGE" : "LEFT/RIGHT: CHANGE", "L R: PAGE  B: BACK"); }
            vsync(); show^=1; REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(show<<4);
        }
        vsync(); read_keys(); volatile u16 *visp = show ? pg1 : pg0;
        if(report){ if(HIT(K_UP) && rtop>0){ rtop--; dirty=1; } if(HIT(K_DOWN) && rtop+10<DM_NLINES){ rtop++; dirty=1; } if(HIT(K_B)||HIT(K_A)){ report=0; dirty=1; } continue; }
        if(HIT(K_UP)){ sel=(sel+nv-1)%nv; sfx(7); dirty=1; note[0]=0; } if(HIT(K_DOWN)){ sel=(sel+1)%nv; sfx(7); dirty=1; note[0]=0; }
        if(HIT(K_L)){ pgi=(pgi+np-1)%np; sel=top=0; sfx(7); dirty=1; note[0]=0; } if(HIT(K_R)){ pgi=(pgi+1)%np; sel=top=0; sfx(7); dirty=1; note[0]=0; }
        if(HIT(K_B)){ if(live){ in_menu--; REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(orig<<4); menu_msg[0]=0; wait_release(); return 0; } settings_save(); DM_SHOW0(); wait_release(); return 0; }
        int d=0; if(HIT(K_RIGHT)){ d=1; hold=0; } else if(HIT(K_LEFT)){ d=-1; hold=0; } else if(HELD(K_RIGHT)||HELD(K_LEFT)){ hold++; if(hold>18 && (hold&3)==0) d=HELD(K_RIGHT)?1:-1; } else hold=0;
        int mul = hold>110 ? 100 : hold>60 ? 10 : 1; (void)rep;
        int a=HIT(K_A); char kd=dm_row[id].kind;
        if(d && kd!='A'){ dm_change(id,d,mul); dirty=1; note[0]=0; if(id!=DR_SFX) sfx(2); }
        if(a){ dirty=1; note[0]=0;
            if(kd=='V'){ dm_change(id,1,1); sfx(2); }
            else if(id==DR_REPORT){ if(dm_need(id)) ; report=1; rtop=0; }
            else if(id==DR_BUG || id==DR_MIRROR || id==DR_HANGAR || id==DR_SIZE){ DM_SHOW0(); if(id==DR_BUG) make_bug(pg0,1); else if(id==DR_MIRROR){ form=FORM_HUMAN; forms_open=7; human_creator(pg0,1); form=FORM_BUG; } else if(id==DR_HANGAR) make_ship(pg0,1); else { cur_slot=dbg.slot; size_screen(pg0,1); } }
            else if(id==DR_GOKW){ settings_save(); DM_SHOW0(); wait_release(); return 2; }
            else { int r=dm_act(visp,id,live,note); if(menu_msg[0]){ scpy(note,menu_msg); menu_msg[0]=0; }
                if(r==1){ in_menu--; REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(orig<<4); menu_msg[0]=0; wait_release(); return 1; }
                if(r==3){ cur_slot=dbg.slot; settings_save(); DM_SHOW0(); wait_release(); return 3; }
                if(id!=DR_SFX && id!=DR_MUS && !note[0]) sfx(2); }
            wait_release(); }
    }
    #undef DM_SHOW0
}
static int dbg_menu(volatile u16 *pg){ int on=in_menu&SH_ON; shd_off(); int r=dm_run(pg,0); if(on) shd_begin(); return r; }           // the title screen's door
static int dbg_live_menu(volatile u16 *pg){ int on=in_menu&SH_ON; shd_off(); int r=dm_run(pg,1); if(on && !r) shd_begin(); return r; }      // the pause screen's door
