// ================================================================= THE RPG: zones, stats, gear, magic, pet, quests
// zone 0..5: The Garden .. The Anthill. Zone z has z+3 dungeon floors; the last one has a boss.
static int zone=0, zone_cleared=0, maxh=30, food=100, bugs_freed=0, scene=0;
static int level, xp;
static void ach_check(void);
static void scpy(char *d, const char *s); static void scat(char *d, const char *s); static void gain_xp(int n);
static void hud_now(void); static void say(const char *s); static void show_msg(const char *s); static void sfx(int n);
static int have_bug[6];
enum { BUG_PILL, BUG_LADY, BUG_FIRE, BUG_BEE, BUG_CRICKET, BUG_ANT };
static const char *const friend_name[6]={"PILL BUG","LADYBUG","FIREFLY","BEE","CRICKET","RUNAWAY ANT"};
static const char *const bug_power[6]={"+10 GOLD","+5 HEALTH","LIGHTS THE DARK","STINGS ATTACKERS","FASTER LEGS","POINTS TO STAIRS"};
// ---- endless galaxy (after the Ant King)
static int endless=0, world_no=0, loot=0, best_depth=0, w_dlv=6; static u32 galaxy_seed=1;
// ---- the grind: zone tiers, counters, achievements, Buddy's level (saved in their own block, see save.inc)
static u8 tiers[6];                                  // how many extra times each zone's boss has been beaten: each tier is harder and richer
static int st_kills[11], st_fish, st_barrels, st_mined, st_dodges, st_planets, st_uniq, st_tiers, st_floors;
static u32 ach_done;                                 // one bit per achievement
static int pet_xp, pet_lv;
// ---- the drone outposts and Bolt's memory: one bit per star system with its outpost destroyed, how many data cores you hold
static u32 base_mask; static int data_cores, outposts_done, base_mode;   // base_mode: 1 while you are inside an outpost
static int tier_lock=-1;                                                           // the tier this dungeon run started at: beating the boss must not change the floor count under your feet
static int tier_now(void){ if(scene==1 && tier_lock>=0) return tier_lock; return (!endless && zone_cleared) ? tiers[zone]+1 : 0; }     // 0 on a first run, 1+ when you go back down a cleared zone
static int dlv(void){ int d = endless ? w_dlv : zone + tier_now()*2; return d>30?30:d; }
static int zone_floors(void){ return endless ? 9999 : zone+3+tier_now(); }
static int gold_find=100;
// ---- FLOOR EVENTS: most dungeon floors roll one, with a twist or a goal
enum { FE_NONE, FE_VAULT, FE_NEST, FE_ELITE, FE_DARK, FE_SPEED, FE_CURSED, FE_RUSH, FE_BLESSED, FE_AMBUSH, FE_QUIET, NFE };
static const char *const fe_name[NFE]={"","TREASURE VAULT","MONSTER NEST","ELITE HUNT","DARK FLOOR","SPEED RUN","CURSED FLOOR","GOLD RUSH","BLESSED SHRINE","AMBUSH","QUIET FLOOR"};
static const char *const fe_what[NFE]={"","CHESTS EVERYWHERE. SO ARE THE GUARDS","EXTRA MONSTERS. EXTRA GOLD","SLAY THE ELITES FOR A BIG REWARD","LESS LIGHT. DOUBLE LOOT","REACH THE STAIRS IN 60 SECONDS",
    "TOUGHER MONSTERS. BETTER LOOT","GOLD, GOLD, GOLD","A SHRINE BLESSES YOU","THEY ARE WAITING AT THE STAIRS","NOTHING HERE WANTS TO FIGHT. BREATHE"};
static int fl_ev=0, fl_need=0, fl_have=0, fl_bonus=100, fl_done=0; static u32 fl_end=0;
static void earn(int g){ g = g*gold_find/100; if(fl_bonus!=100) g=g*fl_bonus/100; if(endless && scene==1) loot+=g; else gold+=g; }
static u16 w_tint[6], w_mon[8]; static u8 w_styles[3]; static char w_name[24];
static u32 wseed;
static int wr(int n){ wseed = wseed*1103515245u + 12345u; return (int)((wseed>>16)%(u32)n); }
static u16 rgb15(int r,int g,int b){ if(r>255)r=255; if(g>255)g=255; if(b>255)b=255; return (r>>3)|((g>>3)<<5)|((b>>3)<<10); }
// ---- your bug's look: built in-game from parts, like a creature creator. Its sprite is drawn at run time
// and uses sprite palette bank 3 all for itself: 1 outline, 2-4 shell, 5 shine, 6-7 head, 9 eye, 10 antenna, 11 legs, 12 markings, 13 horn
typedef struct { char name[9]; u8 shell, ant, horn, eye, legs, pat; u16 col[8]; } BugLook;      // col: shell, head, eye, antenna, legs, markings, horn, (spare)
static BugLook look;
#define bug_name look.name
enum { LC_SHELL, LC_HEAD, LC_EYE, LC_ANT, LC_LEGS, LC_MARK, LC_HORN };
#define NPRESET 16
#define C15(r,g,b) ((u16)(((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10)))
static const u16 preset_col[NPRESET]={ C15(92,108,148), C15(190,50,60), C15(50,140,60), C15(70,150,230), C15(230,100,80), C15(60,90,40), C15(120,60,180), C15(120,80,40), C15(220,170,40), C15(200,200,215), C15(245,245,255), C15(20,20,28), C15(250,220,60), C15(110,40,110), C15(240,130,180), C15(170,80,30) };
static const char *const preset_name[NPRESET]={"STEEL","RUBY","LEAF","SKY","CORAL","MOSS","VIOLET","OAK","GOLD","PEARL","WHITE","BLACK","SUN","PLUM","ROSE","RUST"};
static void bug_look_default(void){ scpy(look.name,"PIP"); look.shell=1; look.ant=0; look.horn=0; look.eye=0; look.legs=1; look.pat=0;
    look.col[LC_SHELL]=preset_col[0]; look.col[LC_HEAD]=rgb15(70,84,120); look.col[LC_EYE]=rgb15(240,240,250); look.col[LC_ANT]=rgb15(14,16,28);
    look.col[LC_LEGS]=rgb15(14,16,28); look.col[LC_MARK]=rgb15(30,36,60); look.col[LC_HORN]=rgb15(200,170,80); }
static u16 shade15(u16 c, int k){ int r=(c&31)*k/16, g=((c>>5)&31)*k/16, b=((c>>10)&31)*k/16; if(r>31)r=31; if(g>31)g=31; if(b>31)b=31; return r|(g<<5)|(b<<10); }
static u16 lift15(u16 c, int k){ int r=(c&31), g=((c>>5)&31), b=((c>>10)&31); r+=(31-r)*k/16; g+=(31-g)*k/16; b+=(31-b)*k/16; return r|(g<<5)|(b<<10); }
static void obj_palette(void){
    for(int i=0;i<16;i++) OBJ_PAL[i]=obj_pal[i];
    u16 sh=look.col[LC_SHELL];
    OBJ_PAL[2]=shade15(sh,8); OBJ_PAL[3]=sh; OBJ_PAL[4]=lift15(sh,7);              // the jaws in first person take the shell colour
    for(int i=0;i<16;i++){ u16 c=OBJ_PAL[i]; OBJ_PAL[16+i]=shade15(c,11); OBJ_PAL[32+i]=shade15(c,7); }
    volatile u16 *P=OBJ_PAL+48; for(int i=0;i<16;i++) P[i]=0;
    P[1]=rgb15(14,16,28); P[2]=shade15(sh,8); P[3]=sh; P[4]=lift15(sh,7); P[5]=lift15(sh,13);
    P[6]=shade15(look.col[LC_HEAD],9); P[7]=look.col[LC_HEAD]; P[8]=lift15(look.col[LC_HEAD],6);
    P[9]=look.col[LC_EYE]; P[10]=look.col[LC_ANT]; P[11]=look.col[LC_LEGS]; P[12]=look.col[LC_MARK]; P[13]=look.col[LC_HORN]; P[14]=shade15(look.col[LC_HORN],9);
}
// draw one walking frame of the bug (28x14 art in a 32x16 box, facing left) from the parts
EW static u8 bug_px[16*32];
static void bug_draw(int frame){
    for(int i=0;i<16*32;i++) bug_px[i]=0;
    #define BP(x,y,v) do{ int xx_=(x), yy_=(y); if(xx_>=0&&xx_<28&&yy_>=0&&yy_<14) bug_px[(yy_+2)*32 + (2+27-xx_)]=(v); }while(0)
    int a = 9+look.shell, cx=11+look.shell, hx=21+look.shell*2;        // shell half-width, shell centre, where the head sits
    s32 cx8=cx*256, cy8=(int)(10.6*256), a8=a*256, b8=(int)(8.6*256);
    for(int y=0;y<=10;y++) for(int x=0;x<28;x++){
        s32 nx=((x*256+128)-cx8)*256/a8, ny=((y*256+128)-cy8)*256/b8;             // 8.8
        s32 r2=(nx*nx+ny*ny)>>8;                                                   // 8.8, 256 = the rim
        if(r2>256) continue;
        if(r2>205 || y==10){ BP(x,y,1); continue; }
        s32 lit=(-nx*115 - ny*243)>>8;                                             // light from the top and the front
        int c = lit>192?5 : lit>107?4 : lit>13?3 : 2;
        if(((x+1)%4==0) && y>2 && c>2) c--;                                        // the plates
        int mark=0;
        if(look.pat==1 && (x/3)%2==0 && y>2 && y<10) mark=1;                        // stripes
        if(look.pat==2 && ((x*5+y*3)%7)==0 && y>1 && y<9 && x>2 && x<cx+a-3) mark=1; // spots
        if(look.pat==3 && y>=6 && y<10) mark=1;                                     // a band
        if(look.pat==4 && ((x+y)&3)==0 && y>1 && y<9) mark=1;                       // checks
        BP(x,y, mark?12:c);
    }
    if(look.horn){ int bx=cx-2; BP(bx,2,13); BP(bx,1,13); BP(bx+1,0,14); BP(bx-1,2,13); BP(bx+1,1,13); }
    for(int y=0;y<14;y++) for(int x=0;x<28;x++){                                   // the head: a small ellipse in front
        s32 dx=(x*256+128)-(hx*256+128), dy=(y*256+128)-(int)(8.5*256);
        s32 r2=((dx*dx)/(int)(3.2*3.2*256) + (dy*dy)/(int)(2.6*2.6*256));         // 8.8-ish
        if(r2>256) continue; int cur=bug_px[(y+2)*32+(2+27-x)]; if(cur>1) continue;
        BP(x,y, r2>158 ? 1 : (y<8?7:6)); }
    if(look.eye==0) BP(hx+1,8,9); else { BP(hx,7,9); BP(hx+1,7,9); BP(hx,8,9); BP(hx+1,8,9); BP(hx+2,8,1); }
    { static const s8 antA[3][9][2]={{{2,5},{3,4},{4,3},{5,2},{6,2},{1,5},{1,4},{2,3},{2,2}},{{2,5},{2,4},{2,3},{2,2},{2,1},{3,5},{3,4},{3,3},{3,2}},{{2,5},{3,5},{4,5},{5,6},{6,7},{1,5},{2,4},{3,4},{4,4}}};
      for(int k=0;k<9;k++) BP(hx+antA[look.ant][k][0], antA[look.ant][k][1], 10); }
    { static const u8 legx[3][8]={{3,8,13,18,0,0,0,0},{3,6,9,12,15,18,0,0},{2,4,7,9,12,14,16,18}}; static const u8 legn[3]={4,6,8};
      for(int k=0;k<legn[look.legs];k++){ int lx=legx[look.legs][k]+look.shell; int fwd=((k+frame)&1)==0; int d=fwd?1:-1; BP(lx,11,11); BP(lx+d,12,11); BP(lx+d,13,11); } }
    #undef BP
}
static void bug_tiles_to_vram(int frame, int tile){                                  // pack the 32x16 picture into 8 4bpp tiles
    bug_draw(frame); volatile u16 *v = OBJ_VRAM + (tile-512)*16;
    for(int ty=0;ty<2;ty++) for(int tx=0;tx<4;tx++) for(int y=0;y<8;y++) for(int x=0;x<8;x+=4){
        const u8 *p=&bug_px[(ty*8+y)*32 + tx*8+x]; *v++ = (p[0]|(p[1]<<4)) | ((p[2]|(p[3]<<4))<<8); }
}
static void bug_build(void){ obj_palette(); bug_tiles_to_vram(0,T_BUGSIDE); bug_tiles_to_vram(1,T_BUGSIDE2); }
static int bug_tile(int frame){ return frame&1 ? T_BUGSIDE2 : T_BUGSIDE; }
#define BUG_BANK (3<<12)
static void name_of(u32 wn, char *b){
    static const char *const syl[32]={"ZA","KOR","MI","LUX","PO","BRA","NU","TIK","VO","RAN","SPI","GLO","QUA","FEN","DOR","ULM","OB","TEL","YAR","SNU","GRIB","OLO","PEM","VAX","HU","LIM","CRO","DIZ","WOB","ESK","TAN","JIB"};
    static const char *const suf[8]={""," PRIME"," II"," MAJOR"," MINOR"," X"," IX"," B"};
    wseed = galaxy_seed ^ (wn*2654435761u) ^ (wn>>7)*0x85EBCA6Bu;
    b[0]=0; int n=2+wr(2); for(int i=0;i<n;i++) scat(b,syl[wr(32)]); scat(b,suf[wr(8)]);
}
// ---- planet DNA. Every world is grown from its own code: the shape of the land, the sea, the sky, the hour, the weather,
// what grows there, whether a city stands on it, what it is known for. The parts multiply out to more than an unvigintillion
// (10 to the 66th) different planets, and the galaxy keeps making new clusters of them for as long as you fly.
typedef struct { u8 style, rough, hour, weather, grav, city, csize, snow, sand, tagline, fauna, ruins, fossils, beetles, cliffs, twotone; u16 bld, sky, road, alt; u8 flora[4]; u8 weird, ntr; u16 tr[4]; u8 cls, mood, snowl, hs; u32 tseed, cseed; } WorldDNA;
static WorldDNA wd;
// ---- PLANET CLASS: one choice that ties a whole world together - its land, sea, colours, weather, plants, life, city and temper.
enum { PC_TEMPERATE, PC_MOUNTAIN, PC_MEADOW, PC_OCEAN, PC_DESERT, PC_ICE, PC_JUNGLE, PC_DEATH, PC_ECUMENOPOLIS, PC_PARADISE, NPC };
static const char *const pc_name[NPC]={"TEMPERATE WORLD","MOUNTAIN WORLD","MEADOW WORLD","OCEAN WORLD","DESERT WORLD","ICE WORLD","JUNGLE WORLD","DEATH PLANET","CITY PLANET","PARADISE"};
enum { MOOD_LOVE, MOOD_MEH, MOOD_HATE };
static const char *const mood_name[3]={"THE LOCALS ADORE YOU.","THE LOCALS DO NOT CARE.","THE LOCALS HATE YOU."};
static int deadly(void){ return wd.cls==PC_DEATH; }
// ---- WEIRD TRAITS: 40 kinds of weirdness x 25 strengths = 1000 traits, each with its own name. A weird world (one in 111, out in the
// galaxy) stacks 1 to 4 of them. wlv[kind] is how strong that kind is on this world (0 = not here, 1..25), wcol[kind] its colour.
enum { MW_GLITCH, MW_RAINBOW, MW_TINY, MW_SPIKES, MW_GIANTS, MW_ZEROG, MW_METAL, MW_VOID, MW_METEOR, MW_MIRROR, MW_DISCO, MW_WOBBLE, MW_SHRINK, MW_JUNGLE,
       MW_SHY, MW_ANGRY, MW_STICKY, MW_ICE, MW_SING, MW_SUNS, MW_RINGS, MW_STRIPES, MW_CHECKER, MW_FOG, MW_GHOSTS, MW_SWARM, MW_HEAT, MW_FROST, MW_FLOOD,
       MW_DROUGHT, MW_NEGATIVE, MW_SEPIA, MW_MOONS, MW_AURORA, MW_LUCK, MW_HEALING, MW_BADAIR, MW_MAGNET, MW_SHYORE, MW_TERRACES, NMW };
#define NTRAIT (NMW*25)
static u8 wlv[NMW]; static u16 wcol[NMW];
#define WL(m) (wlv[m])
static const char *const mw_name[NMW]={"GLITCH","RAINBOW","TINY PLANET","SPIKES","GIANTS","ZERO-G","METAL GROUND","VOID","METEORS","MIRROR SKY","DISCO","WOBBLE","SHRINK RAY","JUNGLE",
    "SHY LIFE","ANGRY LIFE","STICKY GROUND","ICE","SINGING ROCKS","SUNS","RINGS","STRIPES","CHECKERS","FOG","GHOSTS","SLUG SWARM","HEAT","FROST","FLOOD",
    "DROUGHT","NEGATIVE","OLD PHOTO","MOONS","AURORA","LUCK","HEALING AIR","BAD AIR","MAGNET","SHY ORE","TERRACES"};
static const char *const mw_adj[25]={"FAINT","MILD","SOFT","LAZY","GENTLE","ODD","RESTLESS","BUSY","STRONG","WILD","LOUD","GRUMPY","FIERCE","MAD","FURIOUS",
    "FERAL","CRAZY","SAVAGE","EXTREME","INSANE","CHAOTIC","ABSURD","MONSTROUS","IMPOSSIBLE","FORBIDDEN"};
static const char *const mw_quip[NMW]={"MY SENSORS SAY... ERROR. HELLO?","I FEEL SICK. I HAVE NO STOMACH.","I CAN SEE MY OWN BACK FROM HERE.","DO NOT SIT DOWN.","I FEEL SMALL. SMALLER. HELP.",
    "WHEEEEE. SORRY. WHEEEEE.","WE ARE RICH. OR SHINY. OR BOTH.","I AM NOT SCARED. YOU ARE SCARED.","HEADS UP. NO, REALLY. UP.","THE SKY IS THE GROUND. GOOD LUCK.",
    "I CANNOT DANCE. I AM DANCING.","THE HILLS ARE BREATHING. NORMAL.","YOU LOOK... COMPACT.","THE PLANTS ARE WINNING.","EVERYTHING RUNS FROM US. RUDE.",
    "EVERYTHING HATES US. ALSO RUDE.","MY WHEELS ARE STUCK. SO ARE YOU.","WE ARE NOT WALKING. WE ARE SLIDING.","THE ROCKS ARE SINGING. OFF KEY.","TOO MANY SUNS. PICK ONE.",
    "NICE RINGS. VERY FANCY PLANET.","WHO PAINTED THE GROUND?","IT IS A GAME BOARD. WE ARE THE PIECES.","I CANNOT SEE. THAT IS FINE. PROBABLY.","THE CREATURES BLINK OUT. SPOOKY.",
    "SO. MANY. SLUGS.","IT IS HOT. I AM MELTING. A BIT.","MY BOLTS ARE FROZEN.","HOPE YOU CAN SWIM.","WHERE DID THE WATER GO?",
    "THE COLOURS ARE BACKWARDS. OR WE ARE.","THIS PLANET LOOKS A HUNDRED YEARS OLD.","COUNT THE MOONS. I GOT BORED.","THE SKY IS DANCING. SHOW OFF.","I FEEL LUCKY. DO YOU FEEL LUCKY?",
    "THE AIR HEALS. BREATHE DEEP.","DO NOT BREATHE. OR DO. QUICKLY.","THE ORE LIKES YOU.","THE ORE DOES NOT LIKE YOU.","STAIRS. GIANT STAIRS. EVERYWHERE."};
static void trait_name(int id, char *b){ scpy(b,mw_adj[(id/NMW)%25]); scat(b," "); scat(b,mw_name[id%NMW]); }
static u32 hmix(u32 h);
static void traits_apply(void){ for(int m=0;m<NMW;m++){ wlv[m]=0; wcol[m]=0; }
    for(int k=0;k<wd.ntr;k++){ int id=wd.tr[k]%NTRAIT, m=id%NMW, lv=id/NMW+1; if(lv>wlv[m]) wlv[m]=lv; u32 h=hmix(id*2654435761u+wd.tseed); wcol[m]=rgb15(60+(h&127)+((h>>21)&63),60+((h>>7)&127)+((h>>27)&31),60+((h>>14)&127)); } }
// ---- MARKS: what a weird world can do to YOU. Some last a few minutes, a few last forever. Most of them help.
enum { MK_SWIFT, MK_TOUGH, MK_LUCKY, MK_STRONG, MK_SHARP, MK_REGEN, MK_FULL, MK_THICK, MK_SLUGGISH, MK_FRAIL, MK_UNLUCKY, MK_WEAK, MK_HUNGRY, MK_DULL, NMK };
#define MK_GOOD 8
static const char *const mk_name[NMK]={"SWIFT","TOUGH","LUCKY","STRONG","SHARP","REGROWING","WELL FED","THICK SHELL","SLUGGISH","FRAIL","UNLUCKY","WEAK","HUNGRY","DULL"};
static const char *const mk_what[NMK]={"+15% SPEED","+8 MAX HEALTH","+15% GOLD","+3 DAMAGE","+5% CRIT","HEALS OVER TIME","HUNGER IS SLOWER","+3 DEFENCE","-15% SPEED","-6 MAX HEALTH","-10% GOLD","-2 DAMAGE","HUNGER IS FASTER","-4% CRIT"};
static u32 mark_perm=0; static int mark_t[NMK];
// ---- SHAPES: a weird world can turn you into one of its creatures, or into a humanoid that holds tools.
enum { FORM_BUG, FORM_CREATURE, FORM_HUMAN };
// ---- the humanoid's look (made in the character creator when a weird world turns you into one; saved in its own block, see ui.inc)
enum { HC_SKIN, HC_HAIR, HC_ACC, HC_SHIRT, HC_PANTS, HC_BELT, HC_EYES, NHC };     // skin or fur, hair or ears+tail, shoes or muzzle, shirt, pants, belt, eyes
typedef struct { u8 style, height, build, made; u16 col[NHC]; } Hum;               // style 0 human, 1 furry. height 0 tiny .. 4 towering. build 0 slim .. 2 broad
static Hum hum={0,3,1,0}; static int hum_req=0;
static const u16 hum_wd[3]={164,140,128};                                          // sprite width (affine, 128 = the 2x drawing size)
static const u16 hum_ht[5]={200,176,158,142,128};                                  // sprite height: tiny 41px .. towering 64px
static void hum_from_look(void);
static int form=0, form_t=0, form_body=0, forms_open=1, tool_held=0; static u16 form_col=0; static u32 tools_owned=0;   // form_t: seconds left (0 = it stays)
static const char *const form_name[3]={"BUG","CREATURE","HUMANOID"};
static const char *const body_name[3]={"HOPPER","GRAZER","FLYER"};
#define NTOOL 8
enum { TL_PICK, TL_SWORD, TL_BLASTER, TL_LANTERN, TL_JETPACK, TL_SHIELD, TL_SCANNER, TL_GLOVE };
static const char *const tool_name[NTOOL]={"PICKAXE","SWORD","BLASTER","LANTERN","JETPACK","SHIELD","SCANNER","GOLDEN GLOVE"};
static const char *const tool_what[NTOOL]={"MINE IN ONE HIT, MORE ORE","+6 DAMAGE","+4 DAMAGE, +10% CRIT","LIGHTS DUNGEONS","+30% SPEED","+4 DEFENCE","RELICS ARE EASIER TO FIND","+20% GOLD"};
static const u16 tool_cost[NTOOL]={150,260,340,120,300,220,500,600};
static void form_palette(void);
static int holding(int t){ return form==FORM_HUMAN && ((tools_owned>>t)&1) && tool_held==t; }
static int mark_on(int k){ return ((mark_perm>>k)&1) || mark_t[k]>0; }
static const char *const style_name[8]={"ROLLING HILLS","MESAS","DUNES","ISLANDS","CRATERS","CANYONS","RIDGES","PLAINS"};
static const char *const hour_name[3]={"DAY","DUSK","NIGHT"};
static const char *const weather_name[4]={"CLEAR SKIES","RAIN","SNOW","DUST"};
#define NTAG 48
static const char *const tagline[NTAG]={"ITS SOUP","A VERY LOUD FROG","THE WORLD'S FLATTEST ROCK","EXCELLENT NAPS","BEING SLIGHTLY DAMP","ONE REALLY GOOD TREE","A FESTIVAL OF BEANS","MUD. ALL THE MUD.",
    "ITS HAT MUSEUM","THE BIG SNAIL RACE","A SMELL NOBODY CAN PLACE","POLITE WEATHER","THE GREAT PEBBLE SHORTAGE","VERY STRONG TEA","A HOLE THAT HUMS","GRUMPY SUNSETS",
    "THE ANNUAL LEAF COUNT","A STATUE OF A SLUG","FREE HUGS (FROM BEETLES)","ITS NINE TINY MOONS","A LIBRARY WITH ONE BOOK","CHEESE SHAPED CLOUDS","THE FASTEST ANT ALIVE","DOING NOTHING, LOUDLY",
    "ITS BOTTOMLESS PUDDLE","A MAYOR WHO IS A MOTH","PANCAKE ROCKS","THE SONG EVERYONE HUMS","LOST SOCKS","A VERY TALL FENCE","ITS BUG BALLET","WIND THAT WHISTLES TUNES",
    "ANTS WHO SAY SORRY","A PIE CONTEST NOBODY WINS","A SHINY THING ON A HILL","THE LONGEST YAWN","MOSS YOU CAN SIT ON","SOUP (AGAIN)","THE DANCING STONES","TWELVE KINDS OF DEW",
    "A BRIDGE TO NOWHERE","ITS SLEEPY VOLCANO","A COLD THAT IS KIND","AN ECHO THAT TALKS BACK","GOLD THAT IS JUST YELLOW","CROWS WHO PLAY CARDS","RAIN THAT FALLS UP","BEING FAMOUS FOR NOTHING"};
static void story_dna(int z){                                                            // the six story worlds have fixed DNA (they are Bolt's garden)
    static const u8 sd[6][8]={{0,1,0,0,1,1,1,0},{3,1,0,1,0,1,0,0},{1,2,1,0,1,1,2,1},{6,2,2,0,2,1,2,0},{4,1,0,3,1,0,0,0},{5,3,1,0,1,1,3,1}};  // style rough hour weather grav city csize ruins
    memset(&wd,0,sizeof wd);
    wd.style=sd[z][0]; wd.rough=sd[z][1]; wd.hour=sd[z][2]; wd.weather=sd[z][3]; wd.grav=sd[z][4]; wd.city=sd[z][5]; wd.csize=sd[z][6]; wd.ruins=sd[z][7];
    wd.fossils=1+z/2; wd.beetles=z>=2 ? 1+z/2 : 0; wd.tagline=(z*7+3)%NTAG; wd.fauna=z%6; for(int m=0;m<NMW;m++) wlv[m]=0; { static const u8 sc[6]={PC_PARADISE,PC_OCEAN,PC_TEMPERATE,PC_MEADOW,PC_DESERT,PC_MOUNTAIN}; wd.cls=sc[z]; wd.mood = z<2 ? MOOD_LOVE : z<5 ? MOOD_MEH : MOOD_HATE; wd.snowl=200; wd.hs=z==5?4:3; } wd.tseed=(u32)z*977+13; wd.cseed=(u32)z*31+5;
    static const u16 bl[6]={C15(150,140,120),C15(90,120,160),C15(170,80,80),C15(90,70,140),C15(190,150,70),C15(150,90,60)};
    wd.bld=bl[z]; wd.sky=C15(120,170,230); wd.road=C15(60,56,60); wd.snow=z==3; wd.sand=z==1||z==2; wd.alt=0;
    for(int i=0;i<4;i++) wd.flora[i]=3;
}
static u32 sys_id(int i); static u32 amix(u32 k, int star);
static int cur_pl=0, cur_moon=0;                              // which body of this star you are on: planet cur_pl (0 = the first), cur_moon 0 = the planet itself, 1.. = its moons
static u32 hmix(u32 h);
static u32 body_salt(void){ int b=cur_pl*8+cur_moon; return b ? hmix((u32)b*0x9E3779B1u+0x5EEDu) : 0; }   // 0 for the first planet, so every old world stays exactly as it was
static void make_world(void){                                   // three independent 32-bit seeds from the full address: 96 bits, so every planet is its own
    int gal = sys[cur_sys].kind==6; u32 base = gal ? sys_id(cur_sys) : (u32)world_no;
    u32 bsalt = gal ? body_salt() : 0; name_of(base^bsalt,w_name); if(gal && cur_moon) scat(w_name," MOON");
    u32 A = gal ? amix(galaxy_seed^0xA1u^bsalt,cur_sys) : base*0x9E3779B1u, B = gal ? amix(galaxy_seed^0xB2u^bsalt,cur_sys) : base*0x85EBCA6Bu+7, C = gal ? amix(galaxy_seed^0xC3u^bsalt,cur_sys) : base*0xC2B2AE35u+3;
    wseed = A ^ 0x7F4A7C15u; wr(2);
    int r=40+wr(216), g=40+wr(216), b=40+wr(216); int k=wr(3); if(k==0) r=230; else if(k==1) g=230; else b=230;
    { int c=wr(100); wd.cls = c<20?PC_TEMPERATE : c<32?PC_MOUNTAIN : c<42?PC_MEADOW : c<54?PC_OCEAN : c<64?PC_DESERT : c<72?PC_ICE : c<82?PC_JUNGLE : c<89?PC_DEATH : c<92?PC_ECUMENOPOLIS : PC_PARADISE; if(gal && cur_moon){ static const u8 mc[3]={PC_MOUNTAIN,PC_DESERT,PC_ICE}; wd.cls=mc[c%3]; } }   // a moon is bare rock, dust or ice

#ifdef TEST_CLASS
    wd.cls=TEST_CLASS;
#endif
    { static const u8 base[NPC][3]={{0,0,0},{150,125,105},{140,215,95},{70,180,130},{235,195,110},{215,232,255},{40,160,50},{130,30,22},{150,150,165},{235,150,205}};
      if(wd.cls!=PC_TEMPERATE){ r=base[wd.cls][0]+wr(30)-15; g=base[wd.cls][1]+wr(30)-15; b=base[wd.cls][2]+wr(30)-15; if(r<10)r=10; if(g<10)g=10; if(b<10)b=10; } }
    static const u8 mul[5]={8,30,55,80,100};
    for(int i=0;i<5;i++){ int m=mul[i]; int w = i==4 ? 120 : (i==3?30:0); w_tint[i]=rgb15(r*m/100+w, g*m/100+w, b*m/100+w); }
    w_tint[5]=rgb15(r*45/100,g*45/100,b*45/100);
    int mr=40+wr(216), mg=40+wr(216), mb=40+wr(216);
    for(int i=0;i<7;i++){ int m=6+i*16; w_mon[i]=rgb15(mr*m/100+(i>4?(i-4)*40:0), mg*m/100+(i>4?(i-4)*40:0), mb*m/100+(i>4?(i-4)*40:0)); }
    w_mon[7] = wr(2) ? rgb15(255,240,110) : rgb15(120,255,230);
    for(int i=0;i<3;i++) w_styles[i]=wr(5);
    // the DNA
    wd.style=wr(8); wd.rough=wr(4); { int h=wr(8); wd.hour = h<5?0 : h<6?1 : 2; }
    { int w=wr(10); wd.weather = w<6?0 : w<8?1 : w<9?2 : 3; }
    wd.grav=wr(4); wd.city = wr(3)==0; wd.csize = wd.city ? wr(4) : 0; wd.snow=wr(3)==0; wd.sand=wr(2); wd.twotone=wr(3)==0; wd.cliffs=wr(4)==0;
    wd.tagline=wr(NTAG); wd.fauna=wr(6); wd.ruins=wr(3)==0; wd.fossils=wr(4); wd.beetles=wr(4); wd.alt=wr(2) ? rgb15(40+wr(216),40+wr(216),40+wr(216)) : 0;
    wd.bld=rgb15(60+wr(160),60+wr(160),60+wr(160)); wd.sky=rgb15(40+wr(200),60+wr(180),100+wr(156)); wd.road=rgb15(30+wr(60),30+wr(60),30+wr(60));
    for(int i=0;i<4;i++) wd.flora[i]=wr(8);
    wd.tseed = B; wd.cseed = C;
    // the class bends the rest of the DNA so the world hangs together
    wd.snowl=200; wd.hs=3; { int m=wr(10); wd.mood = m<3 ? MOOD_LOVE : m<8 ? MOOD_MEH : MOOD_HATE; }
    switch(wd.cls){
        case PC_MOUNTAIN: wd.style = wr(2) ? 6 : 0; wd.rough=3; wd.snow=1; wd.snowl=150; wd.hs=7; wd.city = wr(5)==0; wd.csize=0; break;
        case PC_MEADOW:   wd.style=7; wd.rough=0; wd.hs=2; wd.snow=0; wd.weather = wr(4)==0 ? 1 : 0; break;
        case PC_OCEAN:    wd.style=3; wd.sand=1; wd.snow=0; wd.weather = wr(3)==0 ? 1 : 0; wd.city = wr(4)==0; wd.csize=0; break;
        case PC_DESERT:   wd.style=2; wd.sand=1; wd.snow=0; wd.weather = wr(3)==0 ? 3 : 0; wd.hour = wr(3)==0 ? 1 : 0; break;
        case PC_ICE:      wd.style = wr(2) ? 0 : 4; wd.snow=1; wd.snowl=60; wd.weather = wr(2) ? 2 : 0; wd.sand=0; break;
        case PC_JUNGLE:   wd.style=0; wd.rough=2; wd.snow=0; wd.weather = wr(2) ? 1 : 0; for(int i=0;i<4;i++) wd.flora[i]=6+wr(2); break;
        case PC_DEATH:    wd.style=4; wd.rough=3; wd.hour=2; wd.weather = wr(2) ? 3 : 0; wd.snow=0; wd.sand=0; wd.mood=MOOD_HATE; wd.city=0; wd.fauna=5; wd.beetles=3; wd.hs=4; break;
        case PC_ECUMENOPOLIS: wd.style=7; wd.rough=0; wd.hs=3; wd.city=1; wd.csize=9; wd.snow=0; break;
        case PC_PARADISE: wd.style = wr(2) ? 3 : 0; wd.rough=1; wd.hour=wr(3)==0; wd.weather=0; wd.mood=MOOD_LOVE; wd.sand=1; break;
    }
    if(gal && cur_moon){ wd.city=0; wd.csize=0; wd.weather=0; wd.mood=MOOD_MEH; wd.ruins=0; }      // no cities, no weather: moons are for mining
    // a weird world: one planet in 111, only out in the galaxy. It stacks 1 to 4 of the 1000 traits
    { u32 F = gal ? amix(galaxy_seed^0xF6u^bsalt,cur_sys) : base*0x2545F491u; wd.ntr=0;
      if((F%111u)==0 && gal){ int n = 1 + ((F>>8)%3u==0) + ((F>>12)%5u==0) + ((F>>20)%9u==0); u32 h=F;
          for(int t=0;t<40 && wd.ntr<n;t++){ h=hmix(h+0x9E3779B9u); int id=(int)(h%NTRAIT), m=id%NMW, ok=1;
              for(int j=0;j<wd.ntr;j++){ int mj=wd.tr[j]%NMW; if(mj==m || (mj==MW_SHY&&m==MW_ANGRY) || (mj==MW_ANGRY&&m==MW_SHY) || (mj==MW_FLOOD&&m==MW_DROUGHT) || (mj==MW_DROUGHT&&m==MW_FLOOD) || (mj==MW_MAGNET&&m==MW_SHYORE) || (mj==MW_SHYORE&&m==MW_MAGNET)) ok=0; }
              if(ok) wd.tr[wd.ntr++]=(u16)id; } }
#ifdef TEST_WEIRD
      wd.ntr=1; wd.tr[0]=TEST_WEIRD;
#endif
#ifdef TEST_WEIRD2
      wd.ntr=2; wd.tr[1]=TEST_WEIRD2;
#endif
      wd.weird=wd.ntr; traits_apply();
      if(WL(MW_GLITCH)){ static const char gl[8]={'#','%','&','@','?','!','*','$'}; for(int i=0;w_name[i];i++) if(w_name[i]!=' ' && (i%3)==1) w_name[i]=gl[(F>>i)&7]; }   // GLITCH: even its name is broken
      if(WL(MW_SPIKES)){ wd.style=6; wd.rough=3; } if(WL(MW_VOID)){ wd.hour=2; wd.weather=0; } if(WL(MW_GIANTS)) wd.city=0;
      if(WL(MW_FROST)){ wd.snow=1; if(WL(MW_FROST)>12) wd.weather=2; } if(WL(MW_HEAT)>15) wd.weather=3; }
}
static const char *world_name(void){ return endless ? w_name : tint_name[zone]; }

// ---------------------------------------------------------------- items
enum { IT_NONE, IT_BERRY, IT_SEED, IT_SHELL, IT_THORN, IT_CRUMB, IT_WARP, IT_GEM, IT_CHARM, IT_BOOK, IT_FISH, IT_POTION, IT_QUEST, IT_KEY };
enum { R_NORMAL, R_MAGIC, R_RARE, R_UNIQUE };
// unique items: one of a kind, each with its own power. Mostly from bosses. Worth keeping.
#define NUNIQ 12
static const char *const uq_name[NUNIQ]={"QUEEN'S FANG","STAG KING'S HORN","EMPRESS STING","EMBER MANDIBLE","MOON SHELL","ANT KING'S PLATE","MIRROR CARAPACE","WIND WINGS","STAR PEBBLE","LUCKY CLOVER","OLD LANTERN","TECHNO SHARD"};
static const u8 uq_it[NUNIQ]={4,4,4,4,3,3,3,3,8,8,8,8};       // IT_THORN, IT_SHELL, IT_CHARM
static const char *const uq_desc[NUNIQ]={"HUGE BITE. POISON","HUGE CRITS","STINGS. FAST BITES","BITES BURN HOT","TOUGH. LOTS OF MANA","TOUGHEST. MORE HEALTH","HURTS WHAT HITS YOU","VERY FAST LEGS","MANA. HEALS YOU SLOWLY","MUCH MORE GOLD. CRITS","LIGHT. MORE HEALTH","2X DAMAGE ON THE TECHNOBEAST"};
// the trophy case on your ship: what you KEEP stays forever (even if you faint) and makes you stronger.
// what you SCRAP turns into tech scrap, which builds Tech Cores.
static int tr_n[4], tr_uq, scrap;
static int trophy_points(void){ return tr_n[0] + tr_n[1]*3 + tr_n[2]*10 + tr_n[3]*50; }
static int keep_points(int rar){ static const u8 p[4]={1,3,10,50}; return p[rar&3]; }
static int scrap_of(int rar){ static const u8 p[4]={1,3,6,12}; return p[rar&3]; }
static const char *const gem_name[4]={"RUBY","SAPPHIRE","EMERALD","DIAMOND"};
static const char *const gem_use[4]={"HEALS ALL","+XP","FULL BELLY","+2 STRENGTH"};
static const char *const berry_col[8]={"RED","BLUE","GOLD","BLACK","WHITE","PINK","GREEN","PURPLE"};
enum { BE_FOOD, BE_HEAL, BE_POISON, BE_SPEED, BE_SIGHT, BE_DIZZY, BE_STRONG, BE_GOLD };
static const char *const berry_eff[8]={"FOOD","HEAL","POISON","SPEED","SIGHT","DIZZY","STRONG","GOLD"};
static const char *const seed_look[6]={"ROUND","LONG","SPIKY","FLAT","TINY","HAIRY"};
enum { SE_STUN, SE_FIND, SE_ESCAPE, SE_CALL, SE_BLAST, SE_HEAL };
static const char *const seed_eff[6]={"STUN","FINDER","ESCAPE","CALLER","BLAST","HEALER"};
static const char *const shell_name[5]={"ACORN SHELL","BEETLE SHELL","CRYSTAL SHELL","STICKY SHELL","GOLDEN SHELL"};
static const char *const thorn_name[5]={"ROSE THORN","WASP STING","GLASS NEEDLE","DARK THORN","KING FANG"};
static const char *const charm_name[4]={"AMBER CHARM","JADE CHARM","BONE CHARM","OPAL CHARM"};
static const char *const fish_name[5]={"SILVER FISH","FIRE FISH","STONE FISH","KING FISH","GOLD FISH"};
static const char *const potion_name[2]={"HEALTH POTION","MANA POTION"};
enum { SP_FIRE, SP_SPARK, SP_HEAL, SP_SHIELD, SP_PORTAL, NSPELL };
static const char *const spell_name[NSPELL]={"FIREBALL","SPARK","HEAL","SHIELD","PORTAL"};
static const u8 spell_cost[NSPELL]={4,6,8,10,12};
// prefixes and suffixes, like the magic items in Fate
static const char *const wpre[7]={"","SHARP","CRUEL","FLAMING","VENOM","LUCKY","VAMPIRE"};
static const char *const apre[7]={"","STURDY","THICK","SPIKY","SWIFT","BLESSED","GLOWING"};
static const char *const suff[8]={"","OF LIFE","OF MAGIC","OF POWER","OF THE FOX","OF GREED","OF HASTE","OF THE BEE"};

typedef struct { u8 it, sub, rar, pre, suf, lvl; } Gear;
#define BAGMAX 12
static Gear bag[BAGMAX]; static int bag_n=0, bag_sel=0;
static Gear eqw, eqa, eqc;           // what you bite with, what you wear, your charm
static u8 berry_map[8], seed_map[6], berry_known[8], seed_known[6];

// ---------------------------------------------------------------- your bug: stats and everything worked out from them
static int st_str, st_dex, st_vit, st_mag, stat_pts;
static int mp, maxmp, fame;
static int d_dmg, d_def, d_crit, d_spd, d_regen, d_reflect, d_vamp, d_haste, d_sting, d_fire, d_light, d_venom;
static u8 spell_known[NSPELL]; static int spell_sel=0;
static int shake_t=0, bob_ph=0;
static int speed_t=0, strong_t=0, dizzy_t=0, stun_t=0, sight=0, hunger_t=0, shield_t=0, regen_t=0;
static int escape_flag=0, call_flag=0, blast_flag=0, cast_flag=-1;
static int portal_depth=0;

static void scpy(char *d, const char *s){ while(*s) *d++=*s++; *d=0; }
static void scat(char *d, const char *s){ while(*d) d++; scpy(d,s); }
static void unum(char *d, u32 v){ char t[12]; int n=0; do { t[n++]='0'+v%10; v/=10; } while(v); char r[12]; for(int i=0;i<n;i++) r[i]=t[n-1-i]; r[n]=0; scat(d,r); }
static void ncat(char *d, int v){ char t[8]; int n=0; if(v<0){ scat(d,"-"); v=-v; } do { t[n++]='0'+v%10; v/=10; } while(v && n<7); char r[8]; for(int i=0;i<n;i++) r[i]=t[n-1-i]; r[n]=0; scat(d,r); }

static int weapon_base(const Gear *g){ static const u8 b[5]={2,4,6,5,10}; return g->it!=IT_THORN ? 0 : g->rar==R_UNIQUE ? 12+g->lvl/2 : b[g->sub%5]+g->lvl/2; }
static int armor_base(const Gear *g){ static const u8 b[5]={2,4,6,5,10}; return g->it!=IT_SHELL ? 0 : g->rar==R_UNIQUE ? 12+g->lvl/3 : b[g->sub%5]+g->lvl/3; }
static void gear_affix(const Gear *g){
    if(!g->it) return;
    if(g->rar==R_UNIQUE){ int v=2+g->lvl/4;
        switch(g->sub){
            case 0: d_dmg+=6*v; d_venom+=3*v; break;            case 1: d_dmg+=3*v; d_crit+=25; break;
            case 2: d_sting+=5*v; d_haste+=4; break;            case 3: d_fire+=5*v; d_dmg+=3*v; break;
            case 4: d_def+=5*v; maxmp+=6*v; break;              case 5: d_def+=6*v; maxh+=8*v; break;
            case 6: d_reflect+=6*v; d_def+=3*v; break;          case 7: d_spd+=25; d_haste+=3; break;
            case 8: maxmp+=8*v; d_regen+=1; break;              case 9: gold_find+=80; d_crit+=10; break;
            case 10: d_light+=30; maxh+=5*v; break;             default: d_dmg+=3*v; break; }
        return; }
    int wpn = g->it==IT_THORN, arm = g->it==IT_SHELL, v = 1 + g->lvl/4 + (g->rar==R_RARE);
    if(wpn) switch(g->pre){ case 1: d_dmg+=2*v; break; case 2: d_dmg+=4*v; break; case 3: d_fire+=3*v; break; case 4: d_venom+=2*v; break; case 5: gold_find+=25; break; case 6: d_vamp+=1; break; }
    else if(arm) switch(g->pre){ case 1: d_def+=2*v; break; case 2: d_def+=4*v; break; case 3: d_reflect+=3*v; break; case 4: d_spd+=12; break; case 5: d_regen+=1; break; case 6: d_light+=12; break; }
    else switch(g->pre){ case 1: d_dmg+=v; break; case 2: d_dmg+=2*v; break; case 3: d_fire+=v*2; break; case 4: d_venom+=v; break; case 5: gold_find+=20; break; case 6: d_vamp+=1; break; }
    switch(g->suf){ case 1: maxh+=6*v; break; case 2: maxmp+=5*v; break; case 3: d_dmg+=2*v; break; case 4: d_crit+=8; break; case 5: gold_find+=40; break; case 6: d_haste+=3; break; case 7: d_sting+=3*v; break; }
}
static void recalc(void){
    gold_find=100; d_dmg=0; d_def=0; d_crit=0; d_spd=0; d_regen=0; d_reflect=0; d_vamp=0; d_haste=0; d_sting=0; d_fire=0; d_light=0; d_venom=0;
    maxh = 20 + st_vit*3 + level*2 + have_bug[BUG_LADY]*5;
    { int rn=0; for(u32 r=relic_bits;r;r>>=1) rn+=r&1; gold_find+=rn*5; maxh+=rn*2; }     // every legendary relic: +5% gold, +2 max health, forever
    if(mark_on(MK_TOUGH)) maxh+=8; if(mark_on(MK_FRAIL)) maxh-=6; if(mark_on(MK_LUCKY)) gold_find+=15; if(mark_on(MK_UNLUCKY)) gold_find-=10;
    maxmp = 5 + st_mag*3;
    gear_affix(&eqw); gear_affix(&eqa); gear_affix(&eqc);
    d_dmg += 2 + st_str/2 + weapon_base(&eqw) + (mark_on(MK_STRONG)?3:0) - (mark_on(MK_WEAK)?2:0);
    d_def += st_vit/4 + armor_base(&eqa) + (mark_on(MK_THICK)?3:0);
    d_crit += st_dex + (mark_on(MK_SHARP)?5:0) - (mark_on(MK_DULL)?4:0);
    if(mark_on(MK_SWIFT)) d_spd+=15; if(mark_on(MK_SLUGGISH)) d_spd-=15; if(mark_on(MK_REGEN)) d_regen+=1;
    if(form==FORM_CREATURE){ if(form_body==0) d_spd+=25; else if(form_body==1){ maxh+=15; d_spd-=10; d_def+=2; } else { d_spd+=35; maxh-=5; } d_dmg-=1; }   // a creature: hoppers are quick, grazers tough, flyers fastest
    if(form==FORM_HUMAN){ d_dmg+=2;                                                                                 // a humanoid: what you hold matters
        if(holding(TL_SWORD)) d_dmg+=6; if(holding(TL_BLASTER)){ d_dmg+=4; d_crit+=10; } if(holding(TL_LANTERN)) d_light+=1;
        if(holding(TL_JETPACK)) d_spd+=30; if(holding(TL_SHIELD)) d_def+=4; if(holding(TL_GLOVE)) gold_find+=20; }
    d_haste += st_dex/6;
    d_sting += have_bug[BUG_BEE]*2;
    if(eqa.it==IT_SHELL && eqa.sub==3 && eqa.rar!=R_UNIQUE) d_spd -= 15;        // the sticky shell slows you
    { int tp=trophy_points(); d_dmg += tp/15; gold_find += tp/5; maxh += tp/10; d_def += tp/30; }   // your trophy case
    if(opt_diff==2) gold_find+=25;                                                   // hard mode pays better
    { int r = fame>=1200?5: fame>=700?4: fame>=350?3: fame>=150?2: fame>=50?1:0; gold_find += r*5; maxh += r*3; d_crit += r; }   // fame has its perks
    if(hearts>maxh) hearts=maxh; if(mp>maxmp) mp=maxmp;
}
static void shuffle(u8 *a, int n){ for(int i=0;i<n;i++) a[i]=i; for(int i=n-1;i>0;i--){ int j=rndi(i+1); u8 t=a[i]; a[i]=a[j]; a[j]=t; } }

// ---------------------------------------------------------------- the pet (like Fate's dog): it follows you, fights, and changes when fed fish
static int pet_hp, pet_maxhp, pet_form, pet_form_t, pet_away_t, pet_down_t, pet_cool, pet_sold_n, pet_sold_g;
static const char *const pet_form_name[4]={"BUDDY","FIRE BEETLE","STONE BEETLE","STAG KING"};
static int pet_dmg(void){ static const u8 m[4]={2,4,3,6}; return (2 + (level+pet_lv)/2 + pet_lv/2) * m[pet_form] / 2; }
static int pet_xp_next(void){ return 40+pet_lv*pet_lv*12; }

// ---------------------------------------------------------------- quests and fame
enum { Q_NONE, Q_HUNT, Q_CHAMP, Q_FETCH, Q_RESCUE, Q_BOSS, Q_SHARDS, Q_BRING, Q_GOODS, NQ };
static int q_type, q_need, q_have, q_floor, q_done, q_gold, q_fame, q_zone, q_world; static char q_name[24];
static const char *fame_rank(void){
    return fame>=1200?"BUG LEGEND": fame>=700?"CHAMPION": fame>=350?"HERO": fame>=150?"BRAVE BUG": fame>=50?"KNOWN BUG":"NOBODY";
}

#define NACH 13
typedef struct { const char *name, *what; u16 goal, gold, fame; } Ach;
static const Ach ach[NACH]={
    {"SPIDER SQUASHER","SLAY 50 SPIDERS",50,150,20},{"BEETLE BREAKER","SLAY 25 BEETLES",25,200,25},{"WASP SWATTER","SLAY 25 WASPS",25,200,25},
    {"FRIEND OF BUGS","FREE 10 CAGED BUGS",10,120,60},{"ANGLER","CATCH 10 FISH",10,100,15},{"SMASHER","BREAK 30 BARRELS",30,100,10},
    {"MINER","MINE 40 THINGS ON PLANETS",40,250,20},{"DODGER","DODGE 25 BITES",25,150,20},{"EXPLORER","LAND ON 6 WORLDS",6,200,30},
    {"DEEP DIVER","GO DOWN 30 FLOORS",30,300,40},{"COLLECTOR","FIND 3 UNIQUE ITEMS",3,400,50},{"CHAMPION","CLEAR 3 ZONE TIERS",3,500,80},
    {"BUG LEGEND","REACH LEVEL 20",20,999,150}};
static int ach_have(int i){ switch(i){ case 0: return st_kills[0]; case 1: return st_kills[7]; case 2: return st_kills[8]; case 3: return bugs_freed; case 4: return st_fish;
    case 5: return st_barrels; case 6: return st_mined; case 7: return st_dodges; case 8: return st_planets; case 9: return st_floors; case 10: return st_uniq; case 11: return st_tiers; default: return level; } }
static void ach_check(void){
    for(int i=0;i<NACH;i++) if(!(ach_done>>i&1) && ach_have(i)>=ach[i].goal){ ach_done|=1u<<i; gold+=ach[i].gold; fame+=ach[i].fame;
        char b[40]; scpy(b,"FEAT: "); scat(b,ach[i].name); scat(b," +"); ncat(b,ach[i].gold); scat(b,"G"); say(b); sfx(6); hud_now(); }
}
static void new_game(void){
    for(int i=0;i<6;i++) tiers[i]=0; for(int i=0;i<11;i++) st_kills[i]=0; st_fish=st_barrels=st_mined=st_dodges=st_planets=st_uniq=st_tiers=st_floors=0; ach_done=0; pet_xp=0; pet_lv=0; base_mask=0; data_cores=0; outposts_done=0; base_mode=0;
    shuffle(berry_map,8); shuffle(seed_map,6);
    for(int i=0;i<8;i++) berry_known[i]=0; for(int i=0;i<6;i++) seed_known[i]=0;
    for(int i=0;i<6;i++) have_bug[i]=0;
    for(int i=0;i<NSPELL;i++) spell_known[i]=0; spell_known[SP_FIRE]=1; spell_known[SP_PORTAL]=1; spell_sel=0;
    hum.style=0; hum.height=3; hum.build=1; hum.made=0;
    zone=0; zone_cleared=0; level=1; xp=0; food=100; gold=0; bugs_freed=0; bag_n=0; bag_sel=0; fame=0; portal_depth=0;
    endless=0; world_no=0; loot=0; best_depth=0;
    st_str=6; st_dex=5; st_vit=6; st_mag=5; stat_pts=0;
    eqw=(Gear){IT_THORN,0,R_NORMAL,0,0,1}; eqa=(Gear){0,0,0,0,0,0}; eqc=(Gear){0,0,0,0,0,0};
    recalc(); hearts=maxh; mp=maxmp;
    pet_form=0; pet_form_t=0; pet_away_t=0; pet_down_t=0; pet_maxhp=30; pet_hp=pet_maxhp;
    q_type=Q_NONE;
    bag[bag_n++]=(Gear){IT_CRUMB,0,0,0,0,0};
    bag[bag_n++]=(Gear){IT_POTION,0,0,0,0,0};
}

static void gear_name(const Gear *g, char *b){
    b[0]=0;
    if(g->rar==R_UNIQUE && (g->it==IT_THORN||g->it==IT_SHELL||g->it==IT_CHARM)){ scpy(b,uq_name[g->sub%NUNIQ]); return; }
    int it=g->it, sub=g->sub;
    switch(it){
        case IT_BERRY: scpy(b,berry_col[sub]); scat(b," BERRY"); if(berry_known[sub]){ scat(b," "); scat(b,berry_eff[berry_map[sub]]); } break;
        case IT_SEED:  scpy(b,seed_look[sub]); scat(b," SEED"); if(seed_known[sub]){ scat(b," "); scat(b,seed_eff[seed_map[sub]]); } break;
        case IT_SHELL: case IT_THORN: case IT_CHARM: {
            const char *pre = it==IT_THORN ? wpre[g->pre] : it==IT_SHELL ? apre[g->pre] : wpre[g->pre];
            if(g->pre){ scpy(b,pre); scat(b," "); }
            scat(b, it==IT_THORN ? thorn_name[sub] : it==IT_SHELL ? shell_name[sub] : charm_name[sub&3]);
            if(g->suf){ scat(b," "); scat(b,suff[g->suf]); }
            break; }
        case IT_WARP: scpy(b,"WARP CRYSTAL"); break;
        case IT_GEM: scpy(b,gem_name[sub&3]); break;
        case IT_CRUMB: scpy(b,"CRUMB"); break;
        case IT_BOOK: scpy(b,"BOOK: "); scat(b,spell_name[sub%NSPELL]); break;
        case IT_FISH: scpy(b,fish_name[sub%5]); break;
        case IT_POTION: scpy(b,potion_name[sub&1]); break;
        case IT_QUEST: scpy(b,"LOST TREASURE"); break;
        case IT_KEY: scpy(b,"DRONE KEYCARD"); break;
        default: scpy(b,"NOTHING");
    }
}
static int rar_color(const Gear *g){ return g->rar==R_UNIQUE ? C_UNIQ : g->rar==R_RARE ? C_GOLD : g->rar==R_MAGIC ? C_CYAN : C_WHITE; }
static int is_gear(const Gear *g){ return g->it==IT_THORN||g->it==IT_SHELL||g->it==IT_CHARM; }
static Gear roll_unique(int ilvl){ int id=rndi(NUNIQ); Gear g={uq_it[id],(u8)id,R_UNIQUE,0,0,(u8)(ilvl+5>99?99:ilvl+5)}; return g; }

// make a random piece of gear for this depth (drops, shops, rewards)
static Gear roll_gear(int kind, int ilvl, int luck){
    Gear g; g.it=kind; g.lvl=ilvl>99?99:ilvl; g.pre=0; g.suf=0;
    int r=rndi(100)+luck; g.rar = r>=95 ? R_RARE : r>=70 ? R_MAGIC : R_NORMAL;
    if(kind==IT_CHARM && g.rar==R_NORMAL) g.rar=R_MAGIC;
    if(kind==IT_THORN){ int t=rndi(100)+ilvl; g.sub = t>110?4 : t>75?2 : t>45?1 : (rndi(5)==0?3:0); }
    else if(kind==IT_SHELL){ int t=rndi(100)+ilvl; g.sub = t>110?4 : t>75?2 : t>45?1 : (rndi(5)==0?3:0); }
    else g.sub=rndi(4);
    if(g.rar==R_MAGIC){ if(rndi(2)) g.pre=1+rndi(6); else g.suf=1+rndi(7); }
    if(g.rar==R_RARE){ g.pre=1+rndi(6); g.suf=1+rndi(7); }
    return g;
}
static Gear roll_drop(int ilvl, int luck){
    int r=rndi(100);
    if(r<16) return roll_gear(IT_THORN,ilvl,luck);
    if(r<32) return roll_gear(IT_SHELL,ilvl,luck);
    if(r<40) return roll_gear(IT_CHARM,ilvl,luck);
    Gear g={0,0,0,0,0,0};
    if(r<55){ g.it=IT_BERRY; g.sub=rndi(8); }
    else if(r<66){ g.it=IT_POTION; g.sub=rndi(2); }
    else if(r<76){ g.it=IT_SEED; g.sub=rndi(6); }
    else if(r<84){ g.it=IT_CRUMB; }
    else if(r<88){ g.it=IT_BOOK; g.sub=rndi(SP_PORTAL); }
    else { g.it=IT_GEM; int t=rndi(10)+ilvl/6; g.sub = t<5?0 : t<8?1 : t<11?2 : 3; }
    return g;
}
static int item_value(const Gear *g){
    switch(g->it){
        case IT_GEM: { static const u8 v[4]={40,60,90,150}; return v[g->sub&3] + 5*dlv(); }
        case IT_SHELL: case IT_THORN: case IT_CHARM: { if(g->rar==R_UNIQUE) return 400+g->lvl*20; int v=12+g->lvl*3+(g->sub==4?150:0); return g->rar==R_RARE?v*4 : g->rar==R_MAGIC?v*2 : v; }
        case IT_WARP: return 60; case IT_SEED: return 8; case IT_BERRY: return 5; case IT_BOOK: return 80; case IT_POTION: return 12;
        case IT_FISH: { static const u8 v[5]={10,40,40,90,120}; return v[g->sub%5]; }
        default: return 2;
    }
}

// ---------------------------------------------------------------- the status bar
static void draw_hud(volatile u16 *pg, const char *l1){
    rect(pg,0,0,240,17,0); rect(pg,0,17,240,1,6);
    char b[32]; b[0]=0;
    scpy(b,"HP "); ncat(b,hearts<0?0:hearts); scat(b,"/"); ncat(b,maxh); text(pg,3,3,b,C_RED,1);
    b[0]=0; scpy(b,"MP "); ncat(b,mp); scat(b,"/"); ncat(b,maxmp); text(pg,3,10,b,C_CYAN,1);
    rect(pg,52,4,30,3,C_DGREY); rect(pg,52,4,(hearts>0?hearts:0)*30/(maxh?maxh:1),3,C_RED);
    rect(pg,52,11,30,3,C_DGREY); rect(pg,52,11,food*30/100,3, food>25?C_GREEN:C_RED);
    { char lv[8]="LV"; ncat(lv,level); text(pg,86,3,lv,C_GOLD,1); }
    { int xb = xp*16/((6*level*level+4)*3); if(xb>16) xb=16; rect(pg,86,11,16,2,C_DGREY); rect(pg,86,11,xb,2,C_GOLD); }
    text(pg,106,3,l1,C_WHITE,1);
    if(bag_n){ char nm[40]; gear_name(&bag[bag_sel],nm); b[0]='0'+(bag_sel+1)%10; b[1]=' '; b[2]=0; scat(b,nm); b[25]=0; text(pg,106,10,b,rar_color(&bag[bag_sel]),1); }
    else text(pg,106,10,"BAG EMPTY",C_GREY,1);
    if(endless && scene==1){ text(pg,206,3,"+",C_GOLD,1); num(b,loot,4); text(pg,212,3,b,C_GOLD,1); }
    else { text(pg,206,3,"G",C_GOLD,1); num(b,gold>9999?9999:gold,4); text(pg,212,3,b,C_WHITE,1); }
    text(pg,206,10,"*",C_RED,1); num(b,bugs_freed,2); text(pg,212,10,b,C_WHITE,1);
    if(stat_pts){ text(pg,226,10,"+",C_GREEN,1); }
}

static u32 hud_sig(void){      // changes whenever something on the status bar changes
    u32 h=2166136261u; int v[13]={hearts,mp,maxh,maxmp,food,level,xp,gold,loot,bag_n,bag_sel,bugs_freed,stat_pts};
    for(int i=0;i<13;i++){ h^=(u32)v[i]; h*=16777619u; } if(bag_n){ h^=bag[bag_sel].it*31+bag[bag_sel].sub; h*=16777619u; } return h; }

// ---------------------------------------------------------------- freeing bugs
static void free_bug(int t){
    bugs_freed++; have_bug[t]++;
    if(t==BUG_PILL) earn(10);
    recalc(); if(t==BUG_LADY) hearts=maxh;
    char b[40]; scpy(b,"FREED A "); scat(b,friend_name[t]); say(b); sfx(2);
    if(q_type==Q_RESCUE){ q_have++; if(q_have>=q_need) q_done=1; }
    ach_check(); hud_now();
}

// ---------------------------------------------------------------- getting hurt, hunger
static void hurt(int n){
    if(invuln) return;
    int def = d_def + (shield_t ? 5+st_mag/2 : 0);
    n -= def/2; if(opt_easy) n=(n+1)/2; else if(opt_diff==2) n=n*3/2; if(n<1) n=1;
    if(rndi(100) < def){ say("BLOCKED!"); invuln=15; return; }
    hearts-=n; invuln=30; shake_t=opt_shake?10:0; sfx(4); hud_now();
}
static void hunger_tick(int every){
    if(opt_easy) every+=every/2; else if(opt_diff==2) every=every*2/3;
    if(mark_on(MK_FULL)) every+=every/2; if(mark_on(MK_HUNGRY)) every=every*2/3;
    if(++hunger_t < every) return; hunger_t=0;
    if(food>0){ food--; if(food==20) say("YOU ARE HUNGRY"); if(food%5==0) hud_now(); }
    else { hearts-=1+maxh/20; say("STARVING! EAT SOMETHING"); hud_now(); }
}
static void regen_tick(int every){          // slow healing and mana
    if(++regen_t < every) return; regen_t=0;
    int ch=0;
    if(mp<maxmp){ mp+=1+st_mag/12; if(mp>maxmp) mp=maxmp; ch=1; }
    if(d_regen && hearts<maxh){ hearts+=d_regen; if(hearts>maxh) hearts=maxh; ch=1; }
    if(ch) hud_now();
}

// ---------------------------------------------------------------- the bag
static int add_gear(Gear g){
    if(g.it==IT_QUEST){ if(q_type==Q_SHARDS){ q_have++; if(q_have>=q_need){ q_done=1; say("ALL SHARDS FOUND! GO TO YOUR SHIP"); } else { char sb[32]; scpy(sb,"SHARD "); ncat(sb,q_have); scat(sb,"/"); ncat(sb,q_need); say(sb); } } else { q_done=1; say("FOUND THE LOST TREASURE!"); } sfx(2); return 1; }
    if(bag_n>=BAGMAX){ say("YOUR BAG IS FULL"); return 0; }
    bag[bag_n++]=g;
    char b[48], nm[40]; gear_name(&g,nm); scpy(b,"GOT "); scat(b,nm); b[39]=0; say(b); sfx(2); hud_now(); return 1;
}
static int add_item(int it, int sub){ Gear g={it,sub,0,0,0,0}; return add_gear(g); }
static void drop_at(int i){ for(int k=i;k<bag_n-1;k++) bag[k]=bag[k+1]; bag_n--; if(bag_sel>=bag_n) bag_sel=bag_n?bag_n-1:0; }
static void drop_sel(void){ drop_at(bag_sel); }
static void eat(int amount){ food+=amount; if(food>100) food=100; }
static int feed_pet(int f);
static int equip(int i){        // wear or wield bag item i; what you had goes back in the bag
    Gear g=bag[i], *slot = g.it==IT_THORN ? &eqw : g.it==IT_SHELL ? &eqa : &eqc;
    if(slot==&eqa && eqa.it==IT_SHELL && eqa.sub==3){ say("THE STICKY SHELL WON'T COME OFF"); return 0; }
    Gear old=*slot; *slot=g; drop_at(i);
    if(old.it && bag_n<BAGMAX) bag[bag_n++]=old;
    char b[48], nm[40]; gear_name(&g,nm); scpy(b,"NOW USING "); scat(b,nm); b[39]=0; say(b);
    recalc(); return 1;
}
static void use_index(int i){
    if(i<0 || i>=bag_n){ say("YOUR BAG IS EMPTY"); return; }
    Gear g=bag[i]; int it=g.it, sub=g.sub; char b[48];
    switch(it){
    case IT_CRUMB: eat(40); say("CRUNCH. FOOD +40"); break;
    case IT_POTION: if(sub==0){ hearts+=maxh/2+10; if(hearts>maxh) hearts=maxh; say("GLUG. HEALTH BACK"); } else { mp=maxmp; say("GLUG. MANA FULL"); } break;
    case IT_BERRY: { int e=berry_map[sub]; berry_known[sub]=1; scpy(b,berry_col[sub]); scat(b," BERRY: ");
        switch(e){
            case BE_FOOD: eat(60); scat(b,"FOOD!"); break;
            case BE_HEAL: hearts=maxh; scat(b,"HEALED!"); break;
            case BE_POISON: hearts-=maxh/5; scat(b,"POISON!"); break;
            case BE_SPEED: speed_t=240; scat(b,"SPEED!"); break;
            case BE_SIGHT: sight=1; scat(b,"YOU SEE THE WAY"); break;
            case BE_DIZZY: dizzy_t=200; scat(b,"DIZZY!"); break;
            case BE_STRONG: strong_t=300; scat(b,"STRONG BITE!"); break;
            default: earn(30); scat(b,"+30 GOLD"); break;
        }
        eat(5); say(b); break; }
    case IT_SEED: { int e=seed_map[sub]; seed_known[sub]=1; scpy(b,seed_look[sub]); scat(b," SEED: ");
        switch(e){
            case SE_STUN: stun_t=200; scat(b,"MONSTERS FREEZE"); break;
            case SE_FIND: sight=1; scat(b,"FINDS THE STAIRS"); break;
            case SE_ESCAPE: if(scene==0){ say("NOTHING TO ESCAPE"); return; } escape_flag=1; scat(b,"ESCAPE!"); break;
            case SE_CALL: call_flag=1; scat(b,"CALLS A BUG"); break;
            case SE_BLAST: blast_flag=1; scat(b,"BLAST!"); break;
            default: hearts+=maxh/3; if(hearts>maxh) hearts=maxh; scat(b,"HEALTH +"); break;
        }
        say(b); break; }
    case IT_SHELL: case IT_THORN: case IT_CHARM: equip(i); hud_now(); return;
    case IT_WARP:
        if(scene==0){ say("KEEP IT FOR A DUNGEON"); return; }
        escape_flag=1; portal_depth=depth_no; say("A PORTAL OPENS!"); break;
    case IT_GEM: { int t=sub&3; scpy(b,gem_name[t]); scat(b,": "); scat(b,gem_use[t]);
        if(t==0) hearts=maxh; else if(t==1) gain_xp(20+depth_no*3); else if(t==2) food=100; else { st_str+=2; recalc(); }
        say(b); break; }
    case IT_BOOK: if(spell_known[sub]){ say("YOU KNOW THAT SPELL"); return; } spell_known[sub]=1; spell_sel=sub; scpy(b,"LEARNED "); scat(b,spell_name[sub]); say(b); break;
    case IT_FISH: if(!feed_pet(sub)) return; break;
    case IT_KEY: say("WALK INTO THE LOCKED DOOR TO USE IT"); return;
    }
    drop_at(i); hud_now();
}
static void use_item(void){ use_index(bag_sel); }
static int sel_held=0, sel_used=0;
static void bag_input(void){
    if(HELD(K_SEL)){                         // hold SELECT + L/R to pick a spell
        if(HIT(K_SEL)){ sel_held=1; sel_used=0; }
        if(bag_n && (HIT(K_UP)||HIT(K_DOWN))){ sel_used=1; bag_sel=(bag_sel+(HIT(K_DOWN)?1:bag_n-1))%bag_n; hud_now(); }   // SELECT + up/down: pick an item
        if(scene!=0 && (HIT(K_L)||HIT(K_R))){ sel_used=1; for(int k=0;k<NSPELL;k++){ spell_sel=(spell_sel+(HIT(K_R)?1:NSPELL-1))%NSPELL; if(spell_known[spell_sel]) break; }
            char b[32]; scpy(b,"SPELL: "); scat(b,spell_name[spell_sel]); say(b); }
        return;
    }
    if(sel_held){ sel_held=0; if(!sel_used) cast_flag=spell_sel; }
    if(scene==0){                            // on a planet L/R pick an item; in a dungeon they strafe
        if(HIT(K_L) && bag_n){ bag_sel=(bag_sel+bag_n-1)%bag_n; hud_now(); }
        if(HIT(K_R) && bag_n){ bag_sel=(bag_sel+1)%bag_n; hud_now(); }
    }
    if(HIT(K_B)) use_item();
}
static int bite_power(int *crit){
    int p = d_dmg + d_fire + d_venom;
    if(strong_t) p*=2;
    *crit = rndi(100) < d_crit; if(*crit) p*=2;
    if(eqw.it==IT_THORN && eqw.sub==3 && rndi(6)==0){ say("THE DARK THORN BITES YOU"); hearts-=2; hud_now(); }
    if(d_vamp && hearts<maxh){ hearts+=d_vamp; }
    return p;
}
static void mark_tick(void){                                   // once a second: passing marks run down
    static int last=0; if((u32)(vbl_count-last)<60) return; last=vbl_count;
    for(int k=0;k<NMK;k++) if(mark_t[k]>0 && --mark_t[k]==0){ recalc(); char b[40]; scpy(b,mk_name[k]); scat(b," WORE OFF"); say(b); hud_now(); }
    if(form && form_t>0 && --form_t==0){ form=FORM_BUG; recalc(); say("YOU ARE A BUG AGAIN"); hud_now(); } }
static void tick_timers(void){ mark_tick(); if(speed_t) speed_t--; if(strong_t) strong_t--; if(dizzy_t) dizzy_t--; if(stun_t) stun_t--; if(shield_t) shield_t--;
    if(pet_form_t && --pet_form_t==0){ say("BUDDY CHANGES BACK"); pet_form=0; } }

// ---------------------------------------------------------------- experience and levels
static int xp_next(void){ return (6*level*level+4)*3; }
static void gain_xp(int n){
    xp+=n; pet_xp+=n/2+1; if(pet_xp>=pet_xp_next()){ pet_xp-=pet_xp_next(); pet_lv++; pet_maxhp+=3; pet_hp=pet_maxhp; char pb[32]; scpy(pb,"BUDDY IS LV "); ncat(pb,pet_lv+1); scat(pb,"!"); say(pb); }
    while(xp>=xp_next()){
        xp-=xp_next(); level++; stat_pts+=5;
        recalc(); hearts=maxh; mp=maxmp; pet_maxhp+=4; pet_hp=pet_maxhp;
        char b[40]; scpy(b,"LEVEL UP! LV "); ncat(b,level); scat(b,". START: SPEND POINTS"); say(b); sfx(6);
    }
    ach_check(); hud_now();
}

// ---------------------------------------------------------------- feeding Buddy
static int feed_pet(int f){
    if(f==0){ eat(40); say("YUM. FOOD +40"); return 1; }
    if(f==4){ say("TOO PRETTY TO EAT. SELL IT!"); return 0; }
    if(pet_away_t || pet_down_t){ say("BUDDY ISN'T HERE"); return 0; }
    pet_form=f; pet_form_t = f==3 ? 2400 : 1600; pet_hp=pet_maxhp*(f==2?2:1);
    char b[40]; scpy(b,"BUDDY BECOMES A "); scat(b,pet_form_name[f]); say(b); sfx(6);
    return 1;
}
