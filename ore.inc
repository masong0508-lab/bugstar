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
