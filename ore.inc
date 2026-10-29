// ================================================================= ORES (roadmap step 1: foundations)
// The ore catalog (in ROM, free), the player's ore bag, the tool tier, and a small "recently mined" list, saved in a block of its own.
// SRAM: slot + 0xB60 .. 0xBA4. That range was empty (audited: the humanoid block ends at 0xB5C, the play-time block starts at 0xC00).
// Slots are copied and erased whole (slot_copy / slot_erase), so the block follows its slot. A save made before this block existed
// simply has no block here: ore_load() then starts you with an empty bag, and nothing else changes.
// Nothing in the game calls ore_add() yet: the mining loop that uses it is step 2 (Phase 1 of the roadmap).
#define NORE 12
#define ORE_RING 16
#define ORE_OFF 0xB60
typedef struct { const char *name; u8 tier, hard; u16 price; } OreDef;      // tier 1..6, hard = hits a vein takes with the plain tool, price = gold each at a neutral market
static const OreDef ore_def[NORE]={
    {"AMBER",1,3,4},     {"CRYSTAL",1,3,5},   {"SILK",1,2,3},      {"NECTAR",1,2,3},          // tier 1: what every world already grows
    {"IRON",2,5,10},     {"COPPER",2,5,12},                                                      // tier 2: rock and mountain worlds
    {"ICE QUARTZ",3,7,22},                                                                       // tier 3: ice worlds and moons
    {"SULFUR",4,8,35},   {"OBSIDIAN",4,10,45}, {"CALCITE",4,8,38},                               // tier 4: hell, glass and bone worlds
    {"SCRAP ALLOY",5,12,70},                                                                     // tier 5: cities and drone outposts
    {"VOIDSTEEL",6,16,160} };                                                                    // tier 6: rare, anywhere deep
static u16 ore_n[NORE];                    // how many of each you carry
static u16 ore_ring[ORE_RING]; static u8 ore_ring_i;   // veins mined lately (a hash of world and spot), so they stay empty after a save and load
static u8 ore_tool;                        // 0 bite, 1 pickaxe, 2 drill, 3 laser cutter
typedef struct { u32 magic; u8 ver, tool, ring_i, pad; u16 n[NORE]; u16 ring[ORE_RING]; u32 sum; } OreSave;     // 68 bytes
_Static_assert(ORE_OFF+sizeof(OreSave) <= META_OFF, "the ore block must end before the play-time block");
_Static_assert(ORE_OFF >= 0xB5C, "the ore block must start after the humanoid block");
static void ore_reset(void){ for(int i=0;i<NORE;i++) ore_n[i]=0; for(int i=0;i<ORE_RING;i++) ore_ring[i]=0; ore_ring_i=0; ore_tool=0; }
static void ore_save(void){ OreSave s; memset(&s,0,sizeof s); s.magic=0x3145524Fu; s.ver=1; s.tool=ore_tool; s.ring_i=ore_ring_i;
    for(int i=0;i<NORE;i++) s.n[i]=ore_n[i]; for(int i=0;i<ORE_RING;i++) s.ring[i]=ore_ring[i];
    s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+ORE_OFF,&s,sizeof s); }
static void ore_load(void){ OreSave s; ore_reset(); sram_read(slot_base(cur_slot)+ORE_OFF,&s,sizeof s);
    if(s.magic!=0x3145524Fu || s.sum!=fnv(&s,sizeof s-4) || s.ver!=1) return;           // no block (an old save) or a damaged one: an empty bag
    ore_tool = s.tool<=3 ? s.tool : 0; ore_ring_i = s.ring_i<ORE_RING ? s.ring_i : 0;
    for(int i=0;i<NORE;i++) ore_n[i]=s.n[i]; for(int i=0;i<ORE_RING;i++) ore_ring[i]=s.ring[i]; }
static int ore_add(int o, int n){ if(o<0||o>=NORE||n<=0) return 0; int v=ore_n[o]+n; if(v>9999) v=9999; int got=v-ore_n[o]; ore_n[o]=(u16)v; return got; }   // returns how many fit
static int ore_take(int o, int n){ if(o<0||o>=NORE||n<0||ore_n[o]<n) return 0; ore_n[o]-=(u16)n; return 1; }
static int ore_value(void){ int g=0; for(int i=0;i<NORE;i++) g+=ore_n[i]*ore_def[i].price; return g; }
static int ore_mined_recently(u16 id){ if(!id) id=1; for(int i=0;i<ORE_RING;i++) if(ore_ring[i]==id) return 1; return 0; }
static void ore_mark_mined(u16 id){ if(!id) id=1; ore_ring[ore_ring_i]=id; ore_ring_i=(u8)((ore_ring_i+1)%ORE_RING); }

// ---- phase 1: veins, tool tiers and what each world holds
// Tier-1 ores (0..3) are the four goods every world already grows: they stay goods in the ship's hold. Ores 4..11 are new and live in the ore bag.
static const char *const ore_tool_name[4]={"JAWS","PICKAXE","DRILL","LASER CUTTER"};
static const u8 ore_hab[NORE]={0,0,0,0, 1,1, 2, 4,4,8, 16, 32};                  // where each ore is found: 1 rock and dry, 2 ice, 4 hell, 8 bone and death, 16 city, 32 anywhere (rare)
static int ore_tool_eff(void){ int t=ore_tool; if(holding(TL_PICK) && t<1) t=1; return t; }          // your best mining tool right now
static int ore_need_tool(int o){ int t=ore_def[o].tier/2; if(cur_rock==2 && t<3) t++; return t; }      // (a metallic rock is dense: one tool tier more)                                     // tier 2-3 need a pickaxe, 4-5 a drill, 6 a laser cutter
static int ore_hits(int o){ int p=1<<ore_tool_eff(); int h=(ore_def[o].hard+p-1)/p; if(cur_rock==2) h+=(h+1)/2; return h; }          // bites it takes with the tool you hold: a tier doubles your power
static const u8 rock_hab[5]={1, 1|16|32, 2|8, 2|4|8|32, 1|2};                    // what each kind of belt rock holds (the same bits as ore_hab)
static int ore_habitat(void){ if(cur_rock) return rock_hab[cur_rock-1]; int h=1; if(wd.cls==PC_ICE||wd.cls==PC_COCYTUS||cur_moon==2) h|=2; if(hellw()) h|=4; if(wd.cls==PC_DEATH||wd.cls==PC_BONE) h|=8; if(wd.cls==PC_ECUMENOPOLIS) h|=16; return h|32; }       // (bit 32, the rare ore, was never set before: voidsteel could not be found)
static u16 ore_id(s32 ox, s32 oy){ return (u16)(hmix(wd.tseed ^ ((u32)(ox>>10)*0x9E3779B1u) ^ ((u32)(oy>>10)*0x85EBCA6Bu)) & 0xFFFF); }     // which vein it is (a hash of the world and the spot)
static int ore_of(s32 ox, s32 oy){      // -1: a plain crystal rock (goods, as before). Otherwise the ore this rock is a vein of
    u32 h=ore_id(ox,oy)*2654435761u+wd.tseed; if((int)((h>>16)%100)<(cur_rock ? 15 : 35)) return -1;      // (a belt rock is mostly vein)
    int maxt=2+dlv()/6+(cur_moon?1:0)+(hellw()?2:0)+(cur_rock==2?2:0); if(maxt>6) maxt=6; int hab=ore_habitat(), cand[NORE], n=0;
    for(int i=4;i<NORE;i++) if((ore_hab[i]&hab) && ore_def[i].tier<=maxt && (ore_hab[i]!=32 || (h&7)==0)) cand[n++]=i;       // (the rare one turns up in one vein in eight)
    return n ? cand[(h>>8)%n] : 4; }
