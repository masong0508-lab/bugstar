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
// ---- phase 4 (basic): the smelter. Ore becomes bars (a smelt takes a little play time), bars become parts, parts become tools.
// Recipes are ROM data (15 of them). The stock (7 bars, 5 parts) and one smelt in progress are saved in a block of their own,
// SRAM slot + 0xBA4 .. 0xBCC (audited: the ore block ends at 0xBA4 and the play-time block starts at 0xC00).
// A save made before this block existed has none here: the smelter then starts empty. The time is play time (the same clock as the PLAYED screen).
#define NMAT 12
#define NREC 21
#define SM_OFF 0xBA4
#define SM_SECS 15              // play seconds a single smelt takes
#define SM_MAXB 5               // the most smelts you can queue at once
static const char *const sm_name[NMAT]={"IRON BAR","COPPER BAR","GLASS BAR","BLACK STEEL","CHALK BAR","ALLOY BAR","VOID BAR", "PLATING","DRILL BIT","LENS","ION CELL","BONE PLATE"};
static const u16 sm_price[NMAT]={24,28,50,95,85,160,380, 90,150,160,520,260};        // gold each (bars 0..6, parts 7..11); VOID BAR is for later phases, it sells high
typedef struct { u8 kind, out, a, na, b, nb; } Recipe;   // kind 0 smelt (a, b = ores), 1 make a part, 2 make a tool (out = the tool tier); for kinds 1 and 2, a and b are stock items (0..11). nb = 0: one input only
static const Recipe sm_rec[NREC]={
    {0,0, 4,2, 0,0},   {0,1, 5,2, 0,0},   {0,2, 6,2, 0,0},   {0,3, 8,1, 7,1},   {0,4, 9,2, 0,0},   {0,5, 10,2, 0,0},   {0,6, 11,2, 0,0},      // IRON, COPPER, GLASS (ice quartz), BLACK STEEL (obsidian + sulfur), CHALK (calcite), ALLOY (scrap alloy), VOID (voidsteel)
    {1,7, 0,2, 1,1},   {1,8, 0,3, 0,0},   {1,9, 2,2, 1,1},   {1,10, 5,2, 3,1},  {1,11, 4,2, 7,1},                                          // PLATING, DRILL BIT, LENS, ION CELL, BONE PLATE
    {2,1, 0,3, 1,1},   {2,2, 8,2, 7,1},   {2,3, 9,1, 10,1},                                                                              // PICKAXE, DRILL, LASER CUTTER
    {1,10, 6,1, 3,1},  {1,11, 6,1, 4,1},
    {3,0, 6,1, 7,2},
    {4,0, 10,2, 3,2}, {5,0, 9,2, 6,1}, {6,0, 11,2, 5,1} };   // kinds 4..6: ship parts like the plate: ENGINE TUNE (2 ion cell + 2 black steel), LASER LENS (2 lens + void bar), SHIELD CELL (2 bone plate + alloy bar). Up to SP_MAX each
          // kind 3: a HULL PLATE for the ship (void bar + 2 plating). It is not stocked: it is fitted at once, up to HP_MAX                                                                                              // void bar shortcuts: ION CELL (void bar + black steel), BONE PLATE (void bar + chalk bar)                                                                              // PICKAXE, DRILL (made from iron and copper alone), LASER CUTTER (no voidsteel: voidsteel needs a laser cutter to mine)
static u16 sm_n[NMAT]; static u8 sm_job, sm_jn; static u32 sm_done;       // what you hold; the smelt in progress (recipe + 1, how many, play second it finishes)
typedef struct { u32 magic; u8 ver, job, jn, pad; u32 done; u16 n[NMAT]; u32 sum; } SmSave;     // 40 bytes
_Static_assert(SM_OFF >= ORE_OFF+sizeof(OreSave), "the smelter block must start after the ore block");
_Static_assert(SM_OFF+sizeof(SmSave) <= META_OFF, "the smelter block must end before the play-time block");
static const char *const sp_name[4]={"HULL PLATE","ENGINE TUNE","LASER LENS","SHIELD CELL"};
static const char *const sp_what[4]={"3% LESS DAMAGE TAKEN EACH","MORE ENGINE THRUST EACH","MORE LASER DAMAGE EACH","+10 SHIELD EACH"};
static int sp_have(int k){ return k==3 ? hull_plates : sp_n[k-4]; }          // k is the recipe kind, 3..6
static int sp_cap(int k){ return k==3 ? HP_MAX : SP_MAX; }
static void sp_add(int k){ if(k==3) hull_plates++; else sp_n[k-4]++; }
static void sm_reset(void){ hull_plates=0; sp_n[0]=sp_n[1]=sp_n[2]=0; for(int i=0;i<NMAT;i++) sm_n[i]=0; sm_job=0; sm_jn=0; sm_done=0; }
static void sm_save(void){ SmSave s; memset(&s,0,sizeof s); s.magic=0x31544C4Du; s.ver=1; s.job=sm_job; s.jn=sm_jn; s.done=sm_done; s.pad=hull_plates; for(int i=0;i<NMAT;i++) s.n[i]=sm_n[i];
    s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+SM_OFF,&s,sizeof s); }
static void sm_load(void){ SmSave s; sm_reset(); sram_read(slot_base(cur_slot)+SM_OFF,&s,sizeof s);
    if(s.magic!=0x31544C4Du || s.sum!=fnv(&s,sizeof s-4) || s.ver!=1) return;          // no block (an old save) or a damaged one: an empty smelter
    for(int i=0;i<NMAT;i++) sm_n[i]= s.n[i]>9999 ? 9999 : s.n[i]; hull_plates = s.pad>HP_MAX ? HP_MAX : s.pad;       // (the spare byte: an old save has 0 there)
    if(s.job>=1 && s.job<=NREC && sm_rec[s.job-1].kind==0 && s.jn>=1 && s.jn<=SM_MAXB){ sm_job=s.job; sm_jn=s.jn; sm_done=s.done; } }
static int sm_have(int kind, int i){ return kind==0 ? ore_n[i] : sm_n[i]; }           // kind 0 reads the ore bag, the rest read the stock
static int sm_batches(int r){ const Recipe *q=&sm_rec[r]; int k=q->kind?1:0; int m=sm_have(k,q->a)/q->na; if(q->nb){ int m2=sm_have(k,q->b)/q->nb; if(m2<m) m=m2; } return m; }   // how many times you could make it now
static int sm_ready(void){ return sm_job && (s32)(play_secs()-sm_done)>=0; }           // a smelt has finished
static int sm_collect(void){ if(!sm_ready()) return 0; int out=sm_rec[sm_job-1].out, got=sm_jn; int v=sm_n[out]+got; sm_n[out]= v>9999?9999:(u16)v; sm_job=0; sm_jn=0; return got; }
static int sm_left(void){ if(!sm_job) return 0; s32 d=(s32)(sm_done-play_secs()); return d>0?d:0; }
// ---- price drift: every 5 play-minutes each ore and bar moves its price by up to 20% either way (a hash of the time slot, so nothing needs saving)
static int px_drift(int id, int base){ u32 h=hmix((u32)(play_secs()/300)*2654435761u ^ (u32)id*0x9E3779B1u); int pc=80+(int)((h>>8)%41); int v=base*pc/100; return v<1?1:v; }
static int ore_px(int o){ return px_drift(o,ore_def[o].price); }
static int sm_px(int m){ return px_drift(100+m,sm_price[m]); }
static int sm_value(void){ int g=0; for(int i=0;i<NMAT;i++) g+=sm_n[i]*sm_px(i); return g; }

// ---- the mining log: how much of each ore you have ever mined. SRAM slot + 0xBCC .. 0xBEC (the smelter block ends at 0xBCC, the play-time block starts at 0xC00).
// A save made before this block existed has none here: the log starts at zero and nothing else changes.
#define ML_OFF 0xBCC
typedef struct { u32 magic; u8 ver, pad[3]; u32 total;   /* pad[0..2]: engine tunes, laser lenses, shield cells */ u16 n[8]; u32 sum; } MlSave;     // 32 bytes
_Static_assert(ML_OFF >= SM_OFF+sizeof(SmSave), "the log block must start after the smelter block");
_Static_assert(ML_OFF+sizeof(MlSave) <= META_OFF, "the log block must end before the play-time block");
static void ml_reset(void){ ml_total=0; for(int i=0;i<8;i++) ml_n[i]=0; }
static void ml_add(int o, int n){ if(o<4||o>=NORE||n<=0) return; int v=ml_n[o-4]+n; ml_n[o-4]=(u16)(v>65535?65535:v); ml_total+=(u32)n; if(ml_total>999999) ml_total=999999; }
static void ml_save(void){ MlSave s; memset(&s,0,sizeof s); s.magic=0x314C4D42u; s.ver=1; s.pad[0]=sp_n[0]; s.pad[1]=sp_n[1]; s.pad[2]=sp_n[2]; s.total=ml_total; for(int i=0;i<8;i++) s.n[i]=ml_n[i]; s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+ML_OFF,&s,sizeof s); }
static void ml_load(void){ MlSave s; ml_reset(); sram_read(slot_base(cur_slot)+ML_OFF,&s,sizeof s);
    if(s.magic!=0x314C4D42u || s.sum!=fnv(&s,sizeof s-4) || s.ver!=1) return; for(int i=0;i<3;i++) sp_n[i]= s.pad[i]>SP_MAX ? SP_MAX : s.pad[i]; ml_total=s.total>999999?999999:s.total; for(int i=0;i<8;i++) ml_n[i]=s.n[i]; }

static void belt_reset(void); static void belt_save(void); static void belt_load(void);       // the belt respawn clock (space.inc)
static void ore_reset(void){ sm_reset(); ml_reset(); belt_reset();  for(int i=0;i<NORE;i++) ore_n[i]=0; for(int i=0;i<ORE_RING;i++) ore_ring[i]=0; ore_ring_i=0; ore_tool=0; }
static void ore_save(void){ OreSave s; memset(&s,0,sizeof s); s.magic=0x3145524Fu; s.ver=1; s.tool=ore_tool; s.ring_i=ore_ring_i;
    for(int i=0;i<NORE;i++) s.n[i]=ore_n[i]; for(int i=0;i<ORE_RING;i++) s.ring[i]=ore_ring[i];
    s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+ORE_OFF,&s,sizeof s); sm_save(); ml_save(); belt_save(); }
static void ore_load(void){ OreSave s; ore_reset(); sm_load(); ml_load(); belt_load(); sram_read(slot_base(cur_slot)+ORE_OFF,&s,sizeof s);
    if(s.magic!=0x3145524Fu || s.sum!=fnv(&s,sizeof s-4) || s.ver!=1) return;           // no block (an old save) or a damaged one: an empty bag
    ore_tool = s.tool<=3 ? s.tool : 0; ore_ring_i = s.ring_i<ORE_RING ? s.ring_i : 0;
    for(int i=0;i<NORE;i++) ore_n[i]=s.n[i]; for(int i=0;i<ORE_RING;i++) ore_ring[i]=s.ring[i]; }
static int ore_add(int o, int n){ if(o<0||o>=NORE||n<=0) return 0; int v=ore_n[o]+n; if(v>9999) v=9999; int got=v-ore_n[o]; ore_n[o]=(u16)v; return got; }   // returns how many fit
static int ore_take(int o, int n){ if(o<0||o>=NORE||n<0||ore_n[o]<n) return 0; ore_n[o]-=(u16)n; return 1; }
static int ore_value(void){ int g=0; for(int i=0;i<NORE;i++) g+=ore_n[i]*ore_px(i); return g; }
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
// ---- ore quality: each vein is POOR, plain, RICH or PRISTINE (set by the vein's own hash, so nothing is stored). It changes how much a vein gives.
static const char *const oq_name[4]={"POOR ","","RICH ","PRISTINE "};
static int ore_quality(s32 ox, s32 oy){ int r=(int)((hmix(ore_id(ox,oy)*0x2545F491u+wd.tseed)>>12)%100); return r<25?0 : r<80?1 : r<95?2 : 3; }
static int ore_qn(int n, int q){ if(q==0) return n>1?n-1:1; if(q==2) return n+1; if(q==3) return n*2; return n; }
static int ore_of(s32 ox, s32 oy){      // -1: a plain crystal rock (goods, as before). Otherwise the ore this rock is a vein of
    u32 h=ore_id(ox,oy)*2654435761u+wd.tseed; if((int)((h>>16)%100)<(cur_rock ? 15 : 35)) return -1;      // (a belt rock is mostly vein)
    int maxt=2+dlv()/6+(cur_moon?1:0)+(hellw()?2:0)+(cur_rock==2?2:0); if(maxt>6) maxt=6; int hab=ore_habitat(), cand[NORE], n=0;
    for(int i=4;i<NORE;i++) if((ore_hab[i]&hab) && ore_def[i].tier<=maxt && (ore_hab[i]!=32 || (h&7)==0)) cand[n++]=i;       // (the rare one turns up in one vein in eight)
    return n ? cand[(h>>8)%n] : 4; }
