// creator_ui.inc: the PLANET CREATOR screen (PC only). #included at the end of the generated main.c, so it can call the game's own drawing functions.
// Reach it from the title menu (PLANET CREATOR) or from the PC LAB. Tabs: WORLD TRAITS PEOPLE LIFE FLORA BIO FILE. L R change tab, UP DOWN pick a row,
// LEFT RIGHT change it, A opens, SELECT rolls the dice for this tab, START undoes. A planet made here can be landed on (FILE > LAND ON IT).
#include "cr_recipe.h"
#include "cr_bio.h"
#include "cr_files.h"

enum { TB_WORLD, TB_TRAITS, TB_PEOPLE, TB_LIFE, TB_FLORA, TB_BIO, TB_FILE, TB_N };
static const char *const cu_tabname[TB_N] = {"WORLD","TRAITS","PEOPLE","LIFE","FLORA","BIO","FILE"};
enum { UR_CLASS, UR_SIZE, UR_XSIZE, UR_SEA, UR_LAND, UR_ROUGH, UR_HOUR, UR_WEATHER, UR_SKY, UR_GROUND, UR_CLIMATE, UR_SALT, UR_NAMEDICE,
       UR_TR0, UR_TH0 = UR_TR0 + 9, UR_TH1,
       UR_STOCK, UR_BODY, UR_LIMBS, UR_HEIGHT, UR_COL0, UR_COL1, UR_COL2, UR_GIFT0, UR_GIFT1, UR_GIFT2, UR_CALLING, UR_FORM, UR_CHAR, UR_DIET, UR_TEMPER, UR_TECH, UR_POP,
       UR_COVER, UR_DIV, UR_CRE0, UR_ADDCRE = UR_CRE0 + CR_MAXCRE, UR_PIN0, UR_ADDPIN = UR_PIN0 + CR_MAXPIN, UR_BOOK,
       UR_DICE, UR_UNDO, UR_REDO, UR_SAVE, UR_LOAD, UR_CODE, UR_IMPORT, UR_EX, UR_LANDON, UR_LEAVE };
static CrRecipe cu_r; static CrHist cu_h; static BioReport cu_b; static int cu_init, cu_tab, cu_row[TB_N], cu_top[TB_N], cu_sub, cu_from_game;
static char cu_note[60]; static int cu_note_t; static u32 cu_clock; static uint64_t cu_rng = 0x5EED5EEDull; static int cu_ex;
static uint16_t cu_savepal[256]; static int cu_hold[4];
static const char *const cu_rough[4] = {"FLAT","GENTLE","ROUGH","WILD"};
static const char *const cu_cat[9] = {"GRAVITY","DAY","AIR","WEATHER","SURFACE","SKY","LIFE","ANCIENT","POWER"};
static void cu_say(const char *s){ snprintf(cu_note, sizeof cu_note, "%s", s); cu_note_t = 150; }
static void cu_commit(void){ cr_fix(&cu_r); cr_hist_push(&cu_h, &cu_r); cr_bio(&cu_r, &cu_b); }
static int cu_cyc(int v, int d, int n){ return ((v + d) % n + n) % n; }
static int cu_rows(int tab, int *ids){
    int n = 0;
    static const u8 w[] = {UR_NAMEDICE,UR_CLASS,UR_SIZE,UR_XSIZE,UR_SEA,UR_LAND,UR_ROUGH,UR_HOUR,UR_WEATHER,UR_SKY,UR_GROUND,UR_CLIMATE,UR_SALT,UR_DICE};
    static const u8 p[] = {UR_STOCK,UR_BODY,UR_LIMBS,UR_HEIGHT,UR_COL0,UR_COL1,UR_COL2,UR_GIFT0,UR_GIFT1,UR_GIFT2,UR_CALLING,UR_FORM,UR_CHAR,UR_DIET,UR_TEMPER,UR_TECH,UR_POP,UR_DICE};
    static const u8 f[] = {UR_UNDO,UR_REDO,UR_SAVE,UR_LOAD,UR_CODE,UR_IMPORT,UR_EX,UR_LANDON,UR_DICE,UR_LEAVE};
    if(tab == TB_WORLD) for(unsigned i = 0; i < sizeof w; i++) ids[n++] = w[i];
    else if(tab == TB_TRAITS){ for(int i = 0; i < 9; i++) ids[n++] = UR_TR0 + i; ids[n++] = UR_TH0; ids[n++] = UR_TH1; ids[n++] = UR_DICE; }
    else if(tab == TB_PEOPLE) for(unsigned i = 0; i < sizeof p; i++) ids[n++] = p[i];
    else if(tab == TB_LIFE){ ids[n++] = UR_COVER; ids[n++] = UR_DIV; for(int i = 0; i < cu_r.ncre; i++) ids[n++] = UR_CRE0 + i; if(cu_r.ncre < CR_MAXCRE) ids[n++] = UR_ADDCRE; ids[n++] = UR_DICE; }
    else if(tab == TB_FLORA){ ids[n++] = UR_BOOK; for(int i = 0; i < cu_r.npin; i++) ids[n++] = UR_PIN0 + i; if(cu_r.npin < CR_MAXPIN) ids[n++] = UR_ADDPIN; }
    else if(tab == TB_FILE) for(unsigned i = 0; i < sizeof f; i++) ids[n++] = f[i];
    return n;
}
static const char *cu_label(int id){
    static const char *const L[] = {"CLASS","SIZE","EXTREME","SEAS","LAND","ROUGHNESS","LIGHT","WEATHER","SKY","GROUND","CLIMATE","NEW LAND","NAME"};
    if(id <= UR_NAMEDICE) return L[id];
    if(id >= UR_TR0 && id < UR_TR0 + 9) return cu_cat[id - UR_TR0];
    switch(id){ case UR_TH0: return "THEME 1"; case UR_TH1: return "THEME 2"; case UR_STOCK: return "STOCK"; case UR_BODY: return "BODY"; case UR_LIMBS: return "LIMBS"; case UR_HEIGHT: return "HEIGHT";
      case UR_COL0: return "SKIN"; case UR_COL1: return "MARKS"; case UR_COL2: return "CLOTHES"; case UR_GIFT0: return "GIFT 1"; case UR_GIFT1: return "GIFT 2"; case UR_GIFT2: return "GIFT 3";
      case UR_CALLING: return "CALLING"; case UR_FORM: return "CITY FORM"; case UR_CHAR: return "CITY MOOD"; case UR_DIET: return "DIET"; case UR_TEMPER: return "TEMPER"; case UR_TECH: return "TECH"; case UR_POP: return "PEOPLE";
      case UR_COVER: return "PLANT COVER"; case UR_DIV: return "PLANT KINDS"; case UR_ADDCRE: return "ADD ANIMAL"; case UR_ADDPIN: return "PIN A PLANT"; case UR_BOOK: return "PLANT BOOK";
      case UR_DICE: return "ROLL DICE"; case UR_UNDO: return "UNDO"; case UR_REDO: return "REDO"; case UR_SAVE: return "SAVE RECIPE"; case UR_LOAD: return "LOAD RECIPE"; case UR_CODE: return "SHARE CODE";
      case UR_IMPORT: return "IMPORT CODE"; case UR_EX: return "EXAMPLE"; case UR_LANDON: return "LAND ON IT"; case UR_LEAVE: return "LEAVE"; }
    return "";
}
static const char *cu_help(int id){
    if(id >= UR_TR0 && id < UR_TR0 + 9) return "A: PICK FROM THE LIST OF THIS KIND OF TRAIT. LEFT RIGHT: NEXT ONE. 1,040 TRAITS IN ALL.";
    if(id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE) return "LEFT RIGHT: HOW MANY. A: CHANGE THE KIND. SELECT: REMOVE IT. 12,672 KINDS OF ANIMAL.";
    if(id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN) return "A: CHANGE THE PLANT. LEFT RIGHT: HOW MUCH. SELECT: REMOVE IT.";
    switch(id){
    case UR_CLASS: return "WHAT KIND OF WORLD: COLOURS, WATER AND MOOD. 16 CLASSES, FROM PARADISE TO FROZEN HELL.";
    case UR_SIZE: return "HOW BIG THE PLANET IS. BIGGER MEANS MORE LAND FOR PLANTS AND PEOPLE.";
    case UR_XSIZE: return "EXTREME SIZES: A PEBBLE YOU WALK ROUND IN SECONDS, OR A WORLD BIGGER THAN A STAR.";
    case UR_SEA: return "HOW MUCH OF THE SURFACE IS UNDER WATER. 0 IS A DRY WORLD.";
    case UR_LAND: return "THE SHAPE OF THE LAND: HILLS, MESAS, DUNES, ISLANDS, CRATERS, CANYONS, RIDGES, PLAINS.";
    case UR_ROUGH: return "HOW STEEP AND BROKEN THE GROUND IS."; case UR_HOUR: return "THE LIGHT YOU LAND IN: DAY, DUSK OR NIGHT.";
    case UR_WEATHER: return "THE WEATHER: CLEAR, RAIN, SNOW OR DUST."; case UR_SKY: return "THE COLOUR OF THE SKY. A OR LEFT RIGHT TO CHANGE.";
    case UR_GROUND: return "THE COLOUR OF THE GROUND. CLASS USES THE COLOUR OF THE WORLD'S CLASS.";
    case UR_CLIMATE: return "WARMER OR COLDER, FIVE DEGREES A STEP. CHANGES ICE, PLANTS AND LIFE.";
    case UR_SALT: return "A DIFFERENT LAND FROM THE SAME RECIPE. ROLL IT UNTIL YOU LIKE THE SHAPE.";
    case UR_NAMEDICE: return "A: TYPE A NAME FOR YOUR PLANET WITH THE LETTER GRID. SELECT OR LEFT RIGHT: A RANDOM NAME.";
    case UR_TH0: case UR_TH1: return "UNIVERSE THEMES: RULE CHANGES FOR THE WHOLE UNIVERSE OF YOUR PLANET.";
    case UR_STOCK: return "WHICH STOCK YOUR PEOPLE COME FROM. WITH THE CALLING IT MAKES ONE OF 512 KINDS OF PEOPLE.";
    case UR_BODY: return "THE SHAPE OF THEIR BODY."; case UR_LIMBS: return "HOW MANY ARMS AND LEGS."; case UR_HEIGHT: return "HOW TALL THEY ARE. TALL FOLK NEED MORE FOOD.";
    case UR_COL0: case UR_COL1: case UR_COL2: return "COLOURS OF THE PEOPLE: SKIN, MARKS AND CLOTHES (THE CITY'S COLOURS TOO).";
    case UR_GIFT0: case UR_GIFT1: case UR_GIFT2: return "A GIFT THEY ARE BORN WITH. SOME HELP THE PLANET, SOME HURT IT. ALL THREE DIFFER.";
    case UR_CALLING: return "WHAT THEY LIVE FOR: FARMING, BUILDING, WANDERING, WAR... IT SHAPES THEIR FOOTPRINT.";
    case UR_FORM: case UR_CHAR: return "THEIR CITY: A FORM AND A MOOD MAKE ONE OF 256 KINDS OF CITY.";
    case UR_DIET: return "WHAT THEY EAT."; case UR_TEMPER: return "THEIR NATURE."; case UR_TECH: return "HOW ADVANCED THEY ARE. MORE TECH, MORE SMOKE.";
    case UR_POP: return "HOW MANY OF THEM THERE ARE. WATCH THE FOOTPRINT.";
    case UR_COVER: return "HOW MUCH OF THE LAND THE PLANTS TRY TO COVER."; case UR_DIV: return "HOW MANY KINDS OF PLANT GROW HERE (OF 536 MILLION NAMED PLANTS).";
    case UR_ADDCRE: return "A: ADD AN ANIMAL. PICK ITS BODY PLAN, SIZE, DIET, HOME AND NATURE (12,672 KINDS)."; case UR_ADDPIN: return "A: PIN A PLANT OF YOUR OWN CHOICE FROM THE PLANT BOOK.";
    case UR_BOOK: return "A: PAGE THROUGH THE NAMED PLANTS THAT GROW ON YOUR PLANET.";
    case UR_DICE: return "A OR SELECT: ROLL NEW RANDOM CHOICES FOR THIS TAB ONLY. UNDO BRINGS THEM BACK.";
    case UR_UNDO: return "TAKE BACK THE LAST CHANGE."; case UR_REDO: return "PUT BACK WHAT YOU TOOK BACK.";
    case UR_SAVE: return "SAVE THIS RECIPE AS A FILE NEXT TO YOUR SAVE (.CRP)."; case UR_LOAD: return "OPEN A RECIPE YOU SAVED BEFORE.";
    case UR_CODE: return "A: MAKE A SHARE CODE AND WRITE IT NEXT TO YOUR SAVE (.CODE FILE).";
    case UR_IMPORT: return "A: READ A SHARE CODE FROM THE .CODE FILE (OR PC_CREATOR_CODE) AND USE IT.";
    case UR_EX: return "A OR LEFT RIGHT: START FROM ONE OF FOUR EXAMPLE PLANETS.";
    case UR_LANDON: return "A: SAVE AND LAND ON THIS PLANET IN THE GAME (OPEN FROM THE PC LAB WHILE YOU PLAY).";
    case UR_LEAVE: return "A: GO BACK. YOUR RECIPE IS KEPT."; }
    return "";
}
static void cu_pal_name(char *o, int sw){ snprintf(o, 20, "COLOUR %d", sw + 1); }
static void cu_big(double v, char *o){ if(v >= 1e12) snprintf(o, 20, "%.1fT", v / 1e12); else if(v >= 1e9) snprintf(o, 20, "%.1fB", v / 1e9); else if(v >= 1e6) snprintf(o, 20, "%.1fM", v / 1e6); else if(v >= 1e4) snprintf(o, 20, "%.0fK", v / 1e3); else snprintf(o, 20, "%.0f", v); }
static void cu_trait_str(int cat, char *o, int n){ if(cu_r.trait[cat] == CR_NONE) snprintf(o, n, "NONE"); else snprintf(o, n, "%s", cr_trait_name(cu_r.trait[cat])); }
// value text of a row; *sw = swatch to draw (or -1); *bar = 0..100 or -1
static void cu_val(int id, char *o, int *sw, int *bar){
    CrRecipe *r = &cu_r; *sw = -1; *bar = -1; o[0] = 0;
    if(id >= UR_TR0 && id < UR_TR0 + 9){ cu_trait_str(id - UR_TR0, o, 24); return; }
    if(id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE){ CrSpecies s; cr_species(r->cre[id - UR_CRE0].species, &s); snprintf(o, 26, "%d %s", r->cre[id - UR_CRE0].amount, s.name); return; }
    if(id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN){ CrPlant p; cr_plant(r->pin[id - UR_PIN0].id, &p); snprintf(o, 26, "%d %s", r->pin[id - UR_PIN0].amount, p.name); return; }
    switch(id){
    case UR_CLASS: snprintf(o, 24, "%s", cr_cls_name[r->cls]); break; case UR_SIZE: snprintf(o, 24, "%s", cr_size_name[r->size]); break;
    case UR_XSIZE: snprintf(o, 24, "%s", cr_xsize_name[r->xsize]); break;
    case UR_SEA: snprintf(o, 24, "%d PCT", r->sea); *bar = r->sea; break; case UR_LAND: snprintf(o, 24, "%s", cr_land_name[r->land]); break;
    case UR_ROUGH: snprintf(o, 24, "%s", cu_rough[r->rough]); break; case UR_HOUR: snprintf(o, 24, "%s", cr_hour_name[r->hour]); break;
    case UR_WEATHER: snprintf(o, 24, "%s", cr_weather_name[r->weather]); break; case UR_SKY: *sw = r->sky; cu_pal_name(o + 4, r->sky); o[0] = o[1] = o[2] = o[3] = ' '; break;
    case UR_GROUND: if(r->ground){ *sw = r->ground; cu_pal_name(o + 4, r->ground); o[0] = o[1] = o[2] = o[3] = ' '; } else snprintf(o, 24, "CLASS"); break;
    case UR_CLIMATE: snprintf(o, 24, "%+d STEPS", r->climate); break; case UR_SALT: snprintf(o, 24, "SEED %05X", r->salt & 0xFFFFF); break;
    case UR_NAMEDICE: snprintf(o, 24, "%s", r->name[0] ? r->name : "NEW WORLD"); break;
    case UR_TH0: case UR_TH1: { int t = r->theme[id - UR_TH0]; if(t == 255) snprintf(o, 26, "NONE"); else snprintf(o, 26, "%s", cr_theme_name(t)); break; }
    case UR_STOCK: { char a[56], b[72], c[72]; cr_people_info(r->stock, r->calling, a, b, c); snprintf(o, 24, "%d %s", r->stock + 1, a); break; }
    case UR_BODY: snprintf(o, 24, "%s", cr_body_name[r->body]); break; case UR_LIMBS: snprintf(o, 24, "%s", cr_limb_name[r->limbs]); break;
    case UR_HEIGHT: snprintf(o, 24, "%d CM", cr_height_cm(r->height)); break;
    case UR_COL0: case UR_COL1: case UR_COL2: *sw = r->col[id - UR_COL0]; cu_pal_name(o + 4, *sw); o[0] = o[1] = o[2] = o[3] = ' '; break;
    case UR_GIFT0: case UR_GIFT1: case UR_GIFT2: snprintf(o, 24, "%s", cr_gift_name[r->gift[id - UR_GIFT0]]); break;
    case UR_CALLING: snprintf(o, 24, "%s", cr_calling_name[r->calling]); break;
    case UR_FORM: snprintf(o, 24, "%s", cr_cstyle_name[r->cstyle & 15]); break; case UR_CHAR: { char a[56], b[72]; cr_city_info(r->cstyle & 15, r->cstyle >> 4, a, b); snprintf(o, 24, "%d OF 16", (r->cstyle >> 4) + 1); break; }
    case UR_DIET: snprintf(o, 24, "%s", cr_diet_name[r->diet]); break; case UR_TEMPER: snprintf(o, 24, "%s", cr_temper_name[r->temper]); break;
    case UR_TECH: snprintf(o, 24, "%s", cr_tech_name[r->tech]); break; case UR_POP: cu_big(cr_pop_count(r->pop), o); break;
    case UR_COVER: snprintf(o, 24, "%d PCT", r->cover); *bar = r->cover; break; case UR_DIV: snprintf(o, 24, "%s", cr_div_name[r->div]); break;
    case UR_BOOK: snprintf(o, 24, "%d PLANTS", cu_b.n_flora); break;
    case UR_EX: snprintf(o, 24, "NUMBER %d", cu_ex + 1); break;
    case UR_CODE: snprintf(o, 24, "WRITE FILE"); break; case UR_LANDON: snprintf(o, 24, "%s", cu_from_game ? "READY" : "IN GAME ONLY"); break;
    default: break; }
}
static void cu_trait_step(int cat, int d){
    int nt = cr_trait_count(), cur = cu_r.trait[cat]; if(cur == CR_NONE) cur = d > 0 ? -1 : nt;
    for(int k = 0; k < nt; k++){ cur += d; if(cur < 0 || cur >= nt){ cu_r.trait[cat] = CR_NONE; return; } if(cr_trait_cat(cur) == cat && cr_trait_ok(&cu_r, cat, cur)){ cu_r.trait[cat] = (uint16_t)cur; return; } }
}
static void cu_theme_step(int slot, int d){
    int nt = cr_theme_count(), cur = cu_r.theme[slot] == 255 ? -1 : cu_r.theme[slot]; if(cu_r.theme[slot] == 255 && d < 0) cur = nt;
    for(int k = 0; k <= nt; k++){ cur += d; if(cur < 0 || cur >= nt){ cu_r.theme[slot] = 255; return; } if(cr_theme_ok(&cu_r, slot, cur)){ cu_r.theme[slot] = (uint8_t)cur; return; } }
}
static void cu_name_dice(void){ CrRecipe t = cu_r; cr_randomize(&t, CRS_NAME, &cu_rng); memcpy(cu_r.name, t.name, sizeof cu_r.name); }
static void cu_dice(void){ if(cu_tab == TB_FILE || cu_tab == TB_FLORA){ cr_randomize(&cu_r, -1, &cu_rng); return; } int sec = cu_tab == TB_WORLD ? CRS_WORLD : cu_tab == TB_TRAITS ? CRS_TRAITS : cu_tab == TB_PEOPLE ? CRS_PEOPLE : CRS_LIFE; cr_randomize(&cu_r, sec, &cu_rng); }
// change a row by d (-1 or +1; big = a larger step). Returns 1 when the recipe changed.
static int cu_change(int id, int d, int big){
    CrRecipe *r = &cu_r; int st = big ? 10 : 1;
    if(id >= UR_TR0 && id < UR_TR0 + 9){ cu_trait_step(id - UR_TR0, d); return 1; }
    if(id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE){ CrCre *c = &r->cre[id - UR_CRE0]; int a = c->amount + d; if(a >= 1 && a <= 9) c->amount = (uint8_t)a; return 1; }
    if(id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN){ CrPin *c = &r->pin[id - UR_PIN0]; int a = c->amount + d; if(a >= 1 && a <= 9) c->amount = (uint8_t)a; return 1; }
    switch(id){
    case UR_CLASS: r->cls = (uint8_t)cu_cyc(r->cls, d, 16); break; case UR_SIZE: r->size = (uint8_t)cu_cyc(r->size, d, 6); break; case UR_XSIZE: r->xsize = (uint8_t)cu_cyc(r->xsize, d, 9); break;
    case UR_SEA: r->sea = (uint8_t)cu_cyc(r->sea + (d > 0 ? 0 : 0), 0, 101); { int v = r->sea + d * (big ? 10 : 5); r->sea = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v); } break;
    case UR_LAND: r->land = (uint8_t)cu_cyc(r->land, d, 8); break; case UR_ROUGH: r->rough = (uint8_t)cu_cyc(r->rough, d, 4); break; case UR_HOUR: r->hour = (uint8_t)cu_cyc(r->hour, d, 3); break;
    case UR_WEATHER: r->weather = (uint8_t)cu_cyc(r->weather, d, 4); break; case UR_SKY: r->sky = (uint8_t)cu_cyc(r->sky, d, CR_NSWATCH); break; case UR_GROUND: r->ground = (uint8_t)cu_cyc(r->ground, d, CR_NSWATCH); break;
    case UR_CLIMATE: { int v = r->climate + d; r->climate = (int8_t)(v < -8 ? -8 : v > 8 ? 8 : v); break; }
    case UR_SALT: r->salt = (uint32_t)(r->salt * 2654435761u + 0x9E3779B9u + (uint32_t)d); break; case UR_NAMEDICE: cu_name_dice(); break;
    case UR_TH0: case UR_TH1: cu_theme_step(id - UR_TH0, d); break;
    case UR_STOCK: r->stock = (uint8_t)cu_cyc(r->stock, d * st, 32); break; case UR_BODY: r->body = (uint8_t)cu_cyc(r->body, d, 16); break; case UR_LIMBS: r->limbs = (uint8_t)cu_cyc(r->limbs, d, 6); break;
    case UR_HEIGHT: r->height = (uint8_t)cu_cyc(r->height, d, 8); break;
    case UR_COL0: case UR_COL1: case UR_COL2: r->col[id - UR_COL0] = (uint8_t)cu_cyc(r->col[id - UR_COL0], d, CR_NSWATCH); break;
    case UR_GIFT0: case UR_GIFT1: case UR_GIFT2: r->gift[id - UR_GIFT0] = (uint8_t)cu_cyc(r->gift[id - UR_GIFT0], d, 24); break;
    case UR_CALLING: r->calling = (uint8_t)cu_cyc(r->calling, d, 16); break; case UR_FORM: r->cstyle = (uint8_t)((r->cstyle & 0xF0) | cu_cyc(r->cstyle & 15, d, 16)); break;
    case UR_CHAR: r->cstyle = (uint8_t)((cu_cyc(r->cstyle >> 4, d, 16) << 4) | (r->cstyle & 15)); break;
    case UR_DIET: r->diet = (uint8_t)cu_cyc(r->diet, d, 6); break; case UR_TEMPER: r->temper = (uint8_t)cu_cyc(r->temper, d, 8); break; case UR_TECH: r->tech = (uint8_t)cu_cyc(r->tech, d, 8); break;
    case UR_POP: { int v = r->pop + d; r->pop = (uint8_t)(v < 0 ? 0 : v > 15 ? 15 : v); break; }
    case UR_COVER: { int v = r->cover + d * (big ? 10 : 5); r->cover = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v); break; }
    case UR_DIV: { int v = r->div + d; r->div = (uint8_t)(v < 0 ? 0 : v > 9 ? 9 : v); break; }
    case UR_EX: cu_ex = cu_cyc(cu_ex, d, 4); cr_default(r, (uint32_t)cu_ex); break;
    default: return 0; }
    return 1;
}
// ================================================================= the live planet
enum { CP_GND = 128, CP_WAT = 144, CP_CLD = 152, CP_ICE = 156, CP_CITY = 160, CP_RING = 162, CP_STAR = 166, CP_SW = 224 };
static u16 cu_mix(u16 a, u16 b, int t){ int ra = a & 31, ga = (a >> 5) & 31, ba = (a >> 10) & 31, rb = b & 31, gb = (b >> 5) & 31, bb = (b >> 10) & 31;
    return (u16)((ra + (rb - ra) * t / 256) | ((ga + (gb - ga) * t / 256) << 5) | ((ba + (bb - ba) * t / 256) << 10)); }
static float cu_h3(int x, int y, int z, u32 s){ u32 h = (u32)x * 73856093u ^ (u32)y * 19349663u ^ (u32)z * 83492791u ^ s * 2654435761u; h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15; return (h & 0xFFFF) / 65535.0f; }
static float cu_n3(float x, float y, float z, u32 s){
    int ix = (int)__builtin_floorf(x), iy = (int)__builtin_floorf(y), iz = (int)__builtin_floorf(z); float fx = x - ix, fy = y - iy, fz = z - iz; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy); fz = fz * fz * (3 - 2 * fz);
    float c[2][2][2]; for(int a = 0; a < 2; a++) for(int b = 0; b < 2; b++) for(int d = 0; d < 2; d++) c[a][b][d] = cu_h3(ix + a, iy + b, iz + d, s);
    float x0 = c[0][0][0] + (c[1][0][0] - c[0][0][0]) * fx, x1 = c[0][1][0] + (c[1][1][0] - c[0][1][0]) * fx, x2 = c[0][0][1] + (c[1][0][1] - c[0][0][1]) * fx, x3 = c[0][1][1] + (c[1][1][1] - c[0][1][1]) * fx;
    float y0 = x0 + (x1 - x0) * fy, y1 = x2 + (x3 - x2) * fy; return y0 + (y1 - y0) * fz;
}
static float cu_fbm(float x, float y, float z, u32 s){ return cu_n3(x, y, z, s) * 0.5f + cu_n3(x * 2.1f, y * 2.1f, z * 2.1f, s + 1) * 0.28f + cu_n3(x * 4.3f, y * 4.3f, z * 4.3f, s + 2) * 0.14f + cu_n3(x * 8.7f, y * 8.7f, z * 8.7f, s + 3) * 0.08f; }
static int cu_has(const char *w){ for(int c = 0; c < 9; c++) if(cu_r.trait[c] != CR_NONE && strstr(cr_trait_name(cu_r.trait[c]), w)) return 1; return 0; }
static void cu_pal(void){
    CrWorld w; cr_world(&cu_r, &w); CrEnv e; cr_env(&cu_r, &e);
    for(int i = 0; i < 16; i++){ int t = i * 4 * 16 / 15; int a = t >> 4, f = (t & 15) * 16; if(a > 3){ a = 3; f = 256; } BG_PAL[CP_GND + i] = cu_mix(w.tint[a], w.tint[a + 1], f > 256 ? 256 : f); }
    static const u8 wc[16][3] = {{20,80,170},{30,90,170},{30,100,170},{15,60,150},{60,120,170},{150,190,230},{20,100,120},{190,50,10},{50,110,170},{30,130,200},{230,90,10},{120,170,40},{20,200,200},{90,100,110},{170,20,30},{110,170,230}};
    for(int i = 0; i < 8; i++) BG_PAL[CP_WAT + i] = rgb15(wc[cu_r.cls][0] * (i + 2) / 9, wc[cu_r.cls][1] * (i + 2) / 9, wc[cu_r.cls][2] * (i + 2) / 9);
    int dust = cu_r.weather == 3; for(int i = 0; i < 4; i++){ int v = 90 + i * 55; BG_PAL[CP_CLD + i] = dust ? rgb15(v, v * 85 / 100, v * 60 / 100) : rgb15(v, v, v + 8); }
    for(int i = 0; i < 4; i++) BG_PAL[CP_ICE + i] = rgb15(120 + i * 45, 150 + i * 32, 190 + i * 22);
    BG_PAL[CP_CITY] = rgb15(255, 220, 110); BG_PAL[CP_CITY + 1] = rgb15(255, 160, 60);
    for(int i = 0; i < 4; i++) BG_PAL[CP_RING + i] = rgb15(120 + i * 30, 105 + i * 25, 80 + i * 20);
    BG_PAL[CP_STAR] = rgb15(255, 255, 255); BG_PAL[CP_STAR + 1] = rgb15(150, 150, 190); BG_PAL[CP_STAR + 2] = rgb15(70, 70, 110);
    for(int i = 0; i < CR_NSWATCH; i++) BG_PAL[CP_SW + i] = cr_swatch(i);
    (void)e;
}
static void cu_planet(volatile u16 *pg, int bx, int by, int bw, int bh){
    int cx = bx + bw / 2, cy = by + bh / 2; float ang = cu_clock * 0.012f;
    static const int Rs[6] = {18, 22, 26, 30, 34, 38}; int R = Rs[cu_r.size]; const int xs = cu_r.xsize;
    if(xs >= 1 && xs <= 4){ static const int rr[5] = {0, 5, 8, 11, 15}; R = rr[xs]; } else if(xs == 5) R = 44; else if(xs == 6) R = 60; else if(xs >= 7) R = 90;
    ui_well(pg, bx, by, bw, bh); u32 salt = cu_r.salt & 0xFFFF;
    CrEnv e; cr_env(&cu_r, &e);
    for(int y = by + 1; y < by + bh - 1; y++) for(int x = bx + 1; x < bx + bw - 1; x++){
        int dx = x - cx, dy = y - cy, c;
        float nx = (float)dx / R, ny = (float)dy / R, rr2 = nx * nx + ny * ny;
        if(rr2 > 1.0f){ u32 h = (u32)(x * 7919 + y * 104729) * 2654435761u; h ^= h >> 13; c = (h % 97) == 0 ? CP_STAR : (h % 211) == 1 ? CP_STAR + 1 : 0; if(!c) c = U_INK;
            if(rr2 < 1.06f && cu_r.xsize < 5) c = CP_SW + cu_r.sky;   // atmosphere rim
            if(cu_has("RINGS") && R < 40 && abs(dy * 3 + dx / 2 - 0) < 40 && __builtin_fabsf((float)dy - dx * 0.25f) < 2.2f && rr2 > 1.1f && rr2 < 2.4f) c = CP_RING + ((x + y) & 3);
            pix(pg, x, y, c); continue; }
        float nz = __builtin_sqrtf(1.0f - rr2), ca = __builtin_cosf(ang), sa = __builtin_sinf(ang), wx = nx * ca + nz * sa, wz = -nx * sa + nz * ca;
        float hgt = cu_fbm(wx * 2.2f + 5, ny * 2.2f, wz * 2.2f + 9, salt);
        switch(cu_r.land){ case 1: hgt = __builtin_floorf(hgt * 5) / 5; break; case 2: hgt = 0.5f + 0.4f * __builtin_sinf((wx + wz) * 9 + hgt * 5) * hgt; break; case 3: hgt = cu_fbm(wx * 4.4f, ny * 4.4f, wz * 4.4f, salt + 7) * 0.9f; break;
            case 4: hgt -= 0.35f * (cu_n3(wx * 6, ny * 6, wz * 6, salt + 3) > 0.7f); break; case 5: hgt = __builtin_fabsf(hgt - 0.5f) * 1.6f + 0.2f; break; case 6: hgt = 1.0f - __builtin_fabsf(hgt - 0.5f) * 2.0f; break; case 7: hgt = 0.5f + (hgt - 0.5f) * 0.35f; break; }
        float thr = 0.22f + 0.58f * cu_r.sea / 100.0f; float lit = nx * 0.55f - ny * 0.35f + nz * 0.75f; if(cu_r.hour == 1) lit = nx * 0.9f + nz * 0.2f + 0.05f; if(cu_r.hour == 2) lit = -nx * 0.2f + nz * 0.1f;
        if(lit < 0) lit = 0; if(lit > 1) lit = 1; lit = 0.12f + lit * 0.88f;
        float polar = (e.temp_c < 25) ? (float)(25 - e.temp_c) / 90.0f : 0; if(cu_r.cls == 5 || cu_r.cls == 15) polar = 0.9f; if(polar > 0.95f) polar = 0.95f;
        if(__builtin_fabsf(ny) > 1.0f - polar * 0.9f) c = CP_ICE + (int)(lit * 3.99f);
        else if(hgt < thr) c = CP_WAT + (int)(lit * 7.99f * (0.7f + 0.3f * hgt / thr));
        else { float up = (hgt - thr) / (1.0f - thr + 0.01f); int g = (int)(3 + lit * (5 + up * 8.0f)); if(g > 15) g = 15; c = CP_GND + g; }
        if(cu_r.weather || cu_r.cls == 6 || cu_r.cls == 2){ float cl = cu_fbm(wx * 3.1f + 20, ny * 3.1f, wz * 3.1f + 3, salt + 99); float cth = cu_r.weather ? 0.52f : 0.66f; if(cl > cth) c = CP_CLD + (int)(lit * 3.99f); }
        if(cu_r.hour == 2 && lit < 0.3f && cu_r.pop > 3 && cu_h3((int)(wx * 40), (int)(ny * 40), (int)(wz * 40), salt) > 0.985f && hgt >= thr) c = CP_CITY;
        pix(pg, x, y, c);
    }
}
// the people: a little figure from the recipe
static void cu_figure(volatile u16 *pg, int bx, int by, int bw, int bh){
    ui_well(pg, bx, by, bw, bh); CrRecipe *r = &cu_r; int cx = bx + bw / 2, base = by + bh - 8; int sk = CP_SW + r->col[0], mk = CP_SW + r->col[1], cl = CP_SW + r->col[2];
    int hh = 12 + r->height * 3, bw2 = 8 + (r->body == 0 || r->body == 12 ? 6 : r->body == 1 ? -2 : r->body == 3 ? 8 : r->body == 2 ? 4 : 0), bh2 = hh;
    if(r->body == 2 || r->body == 3) bh2 = hh * 2 / 3;
    int ty = base - 8 - bh2; rect(pg, cx - bw2 / 2, ty, bw2, bh2, cl); rect(pg, cx - bw2 / 2, ty, bw2, 2, mk);
    int hr = 5 + r->height / 3; rect(pg, cx - hr, ty - hr * 2 + 1, hr * 2, hr * 2, sk); rect(pg, cx - hr + 1, ty - hr - 1, 2, 2, C_WHITE); rect(pg, cx + hr - 3, ty - hr - 1, 2, 2, C_WHITE); rect(pg, cx - hr + 1, ty - hr * 2 + 1, hr * 2 - 2, 1, mk);
    int pairs = r->limbs + 1; for(int i = 0; i < pairs && i < 6; i++){ int ly = ty + 2 + i * (bh2 - 2) / (pairs > 3 ? pairs : 3); if(i >= pairs - 1) ly = ty + bh2; int len = 5 + (i & 1) * 2;
        if(i < pairs - 1 || pairs < 2){ rect(pg, cx - bw2 / 2 - len, ly, len, 2, sk); rect(pg, cx + bw2 / 2, ly, len, 2, sk); } else { rect(pg, cx - bw2 / 2 + 1, ty + bh2, 2, base - ty - bh2 + 6, sk); rect(pg, cx + bw2 / 2 - 3, ty + bh2, 2, base - ty - bh2 + 6, sk); } }
    if(r->body == 4 || r->body == 15){ for(int i = 0; i < 8; i++){ rect(pg, cx - bw2 / 2 - 4 - i, ty + 3 - i / 2, 1, 6, mk); rect(pg, cx + bw2 / 2 + 3 + i, ty + 3 - i / 2, 1, 6, mk); } }
    if(r->body == 9) { rect(pg, cx - 4, ty - hr * 2 - 6, 1, 6, mk); rect(pg, cx + 3, ty - hr * 2 - 6, 1, 6, mk); }
    if(r->body == 10) rect(pg, cx + bw2 / 2, base - 6, 10, 2, mk);
    if(r->body == 6) rect(pg, cx - bw2 / 2 - 2, ty + 2, bw2 + 4, bh2 - 4, mk);
    if(r->body == 13) for(int i = 0; i < 4; i++) rect(pg, cx - 6 + i * 4, ty - hr * 2 - 3, 1, 3, mk);
    if(r->body == 5) for(int i = 0; i < bh2; i += 3) rect(pg, cx - bw2 / 2 - 1, ty + i, bw2 + 2, 1, mk);
    if(r->body == 14) frame1(pg, cx - bw2 / 2 - 2, ty - hr * 2 - 2, bw2 + 4, bh2 + hr * 2 + 6, C_GOLD);
    char a[56], b[72], c[72]; cr_people_info(r->stock, r->calling, a, b, c); a[24] = 0; ctext(pg, bx + bw / 2, by + 3, a, C_WHITE);
    rect(pg, bx + 3, base + 6, bw - 6, 1, U_RAISE);
}
// ================================================================= drawing
static int cu_grade_col(char g){ return g <= 'A' ? C_GREEN : g == 'B' ? C_GREEN : g == 'C' ? C_GOLD : g == 'D' ? C_GOLD : C_RED; }
static void cu_mini(volatile u16 *pg, int x, int y, int w, int h){
    BioReport *b = &cu_b; char s[40], t[24]; ui_panel(pg, x, y, w, h);
    char g[2] = {b->grade, 0}; text(pg, x + 6, y + 4, g, cu_grade_col(b->grade), 4);
    snprintf(s, sizeof s, "HEALTH %d", b->health); text(pg, x + 28, y + 4, s, C_WHITE, 1); ui_bar(pg, x + 28, y + 12, w - 34, 5, b->health, 100, cu_grade_col(b->grade));
    snprintf(s, sizeof s, "PLANTS %d  ANIMALS %d", b->n_flora, b->n_fauna); text(pg, x + 5, y + 23, s, U_SOFT, 1);
    cu_big(b->people, t); snprintf(s, sizeof s, "PEOPLE %s  CITIES %d", t, b->cities > 999 ? 999 : b->cities); s[24] = 0; text(pg, x + 5, y + 30, s, U_SOFT, 1);
    text(pg, x + 5, y + 37, "SMOKE", U_SOFT, 1); ui_bar(pg, x + 30, y + 38, 30, 4, b->smoke, 100, b->smoke > 60 ? C_RED : C_GOLD);
    snprintf(s, sizeof s, "LOST %d%%", b->forest_lost_pct); text(pg, x + 64, y + 37, s, b->forest_lost_pct > 50 ? C_RED : U_SOFT, 1);
    if(b->nwarn){ char wt[28]; snprintf(wt, sizeof wt, "%s", b->warn[0]); text(pg, x + 5, y + 44, wt, C_RED, 1); } else text(pg, x + 5, y + 44, "NO WARNINGS", C_GREEN, 1);
}
static void cu_row_draw(volatile u16 *pg, int id, int y, int sel, int x0, int w){
    char v[40]; int sw, bar; cu_val(id, v, &sw, &bar); const char *lab = cu_label(id);
    int act = id >= UR_DICE || id == UR_ADDCRE || id == UR_ADDPIN || id == UR_BOOK;
    if(sel) ui_sel(pg, x0, y - 1, w, 9, U_SEL);
    if(id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE) lab = ""; if(id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN) lab = "";
    text(pg, x0 + 4, y, lab, sel ? C_GOLD : act ? C_CYAN : U_SOFT, 1);
    if(id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE || id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN){ char am[4] = {v[0], 0, 0, 0}; text(pg, x0 + 4, y, am, C_CYAN, 1); text(pg, x0 + 10, y, "X", U_SOFT, 1); v[27] = 0; text(pg, x0 + 18, y, v + 2, sel ? C_WHITE : C_GREY, 1); return; }
    if(bar >= 0) ui_bar(pg, x0 + 56, y + 1, 24, 5, bar, 100, C_CYAN);
    if(sw >= 0) rect(pg, x0 + 62, y, 10, 7, CP_SW + sw);
    v[18] = 0; if(!act || v[0]){ int vl = textlen(v) * 4; text(pg, x0 + w - 5 - vl, y, v, sel ? C_WHITE : C_GREY, 1); }
    if(sel && !act){ text(pg, x0 + w - 12 - textlen(v) * 4, y, "<", U_SOFT, 1); text(pg, x0 + w - 4, y, ">", U_SOFT, 1); }
}
static void cu_bio_page(volatile u16 *pg){
    BioReport *b = &cu_b; char s[64], t[24]; ui_panel(pg, 2, 14, 236, 134); static const char *const pn[5] = {"HEALTH","FOOD WEB","ENERGY","PEOPLE'S FOOTPRINT","NEXT 120 YEARS"};
    snprintf(s, sizeof s, "BIOFOOTPRINT: %s  %d/5", pn[cu_sub], cu_sub + 1); ui_head(pg, 8, 18, 224, s, C_GREEN);
    int y = 30;
    if(cu_sub == 0){
        char g[2] = {b->grade, 0}; text(pg, 10, y, g, cu_grade_col(b->grade), 6); snprintf(s, sizeof s, "HEALTH %d OF 100", b->health); text(pg, 44, y + 2, s, C_WHITE, 1);
        ui_bar(pg, 44, y + 11, 80, 6, b->health, 100, cu_grade_col(b->grade));
        static const char *const sn[5] = {"AIR","PLANTS","ANIMALS","PEOPLE","STEADY"};
        for(int i = 0; i < 5; i++){ text(pg, 10, y + 34 + i * 9, sn[i], U_SOFT, 1); ui_bar(pg, 50, y + 35 + i * 9, 70, 5, b->sub[i], 100, b->sub[i] < 40 ? C_RED : b->sub[i] < 70 ? C_GOLD : C_GREEN); }
        cu_big(b->biomass_gt, t); snprintf(s, sizeof s, "BIOMASS %s GT", t); text(pg, 130, y, s, C_WHITE, 1);
        cu_big(b->npp_gt, t); snprintf(s, sizeof s, "GROWTH %s GT A YEAR", t); text(pg, 130, y + 9, s, U_SOFT, 1);
        snprintf(s, sizeof s, "PLANT KINDS %d", b->n_flora); text(pg, 130, y + 18, s, C_GREEN, 1); snprintf(s, sizeof s, "ANIMAL KINDS %d", b->n_fauna); text(pg, 130, y + 27, s, C_GOLD, 1);
        snprintf(s, sizeof s, "ALL SPECIES %d", b->n_species); text(pg, 130, y + 36, s, C_WHITE, 1);
        snprintf(s, sizeof s, "AIR %d PCT OXYGEN", b->o2_pct); text(pg, 130, y + 45, s, U_SOFT, 1); snprintf(s, sizeof s, "%d C  LIGHT %d  SOIL %d", b->temp_c, b->light, b->fertility); text(pg, 130, y + 54, s, U_SOFT, 1);
        if(!b->nwarn) text(pg, 10, y + 82, "NO COLLAPSE WARNINGS IN 120 YEARS", C_GREEN, 1);
        for(int i = 0; i < b->nwarn && i < 3; i++) text(pg, 10, y + 82 + i * 8, b->warn[i], C_RED, 1);
    } else if(cu_sub == 1){
        static const char *const ln[4] = {"PLANTS","PLANT EATERS","HUNTERS","TOP HUNTERS"};
        for(int i = 0; i < 4; i++){ snprintf(s, sizeof s, "%s (%d)", ln[i], b->lvl_n[i]); text(pg, 10, y + i * 10, s, i ? C_WHITE : C_GREEN, 1);
            ui_bar(pg, 110, y + 1 + i * 10, 80, 5, b->lvl_use[i] > 100 ? 100 : b->lvl_use[i], 100, b->lvl_use[i] > 100 ? C_RED : C_CYAN); snprintf(s, sizeof s, "%d PCT EATEN", b->lvl_use[i]); text(pg, 194, y + i * 10, s, b->lvl_use[i] > 100 ? C_RED : U_SOFT, 1); }
        text(pg, 10, y + 44, "WHO EATS WHAT", C_GOLD, 1);
        for(int i = 0; i < b->n_links && i < 9; i++){ char w[28]; snprintf(w, sizeof w, "%s", b->link[i].who); w[26] = 0; snprintf(s, sizeof s, "%s", w); text(pg, 10, y + 54 + i * 7, s, C_GREY, 1);
            snprintf(w, sizeof w, "%s %d", b->link[i].what, b->link[i].pct); text(pg, 230 - textlen(w) * 4, y + 54 + i * 7, w, b->link[i].pct > 100 ? C_RED : U_SOFT, 1); }
    } else if(cu_sub == 2){
        int xs = 10, wd = 216; int seg[4] = {b->e_plants, b->e_eaten, b->e_people, b->e_lost}; int cols[4] = {C_GREEN, C_GOLD, C_CYAN, C_DGREY}; static const char *const sn[4] = {"KEPT BY PLANTS","EATEN BY ANIMALS","USED BY YOUR PEOPLE","LOST AS HEAT"};
        text(pg, 10, y, "WHERE THE SUNLIGHT GOES (PERCENT OF 100)", U_SOFT, 1); int tot = seg[0] + seg[1] + seg[2] + seg[3]; if(tot < 1) tot = 1; int x = xs;
        for(int i = 0; i < 4; i++){ int w = wd * seg[i] / tot; if(w > 0) rect(pg, x, y + 12, w, 10, cols[i]); x += w; }
        for(int i = 0; i < 4; i++){ rect(pg, 10, y + 32 + i * 9, 6, 6, cols[i]); snprintf(s, sizeof s, "%s %d", sn[i], seg[i]); text(pg, 20, y + 32 + i * 9, s, C_WHITE, 1); }
        snprintf(s, sizeof s, "SUNLIGHT CAUGHT BY PLANTS %d PCT", b->e_sun); text(pg, 10, y + 72, s, U_SOFT, 1); snprintf(s, sizeof s, "OXYGEN IN THE AIR %d PCT (EARTH 21)", b->o2_pct); text(pg, 10, y + 81, s, b->o2_pct < 14 || b->o2_pct > 30 ? C_RED : C_GREEN, 1);
    } else if(cu_sub == 3){
        cu_big(b->people, t); snprintf(s, sizeof s, "PEOPLE %s", t); text(pg, 10, y, s, C_WHITE, 1); snprintf(s, sizeof s, "CITIES %d OF %d PEOPLE", b->cities, b->city_size); text(pg, 10, y + 9, s, C_WHITE, 1);
        struct { const char *n; int v; int bad; } it[4] = {{"FARMS (LAND)", b->farm_pct, 40}, {"FOREST LOST", b->forest_lost_pct, 40}, {"SMOKE", b->smoke, 50}, {"WOOD BURNED", b->fuel_pct > 100 ? 100 : b->fuel_pct, 60}};
        for(int i = 0; i < 4; i++){ text(pg, 10, y + 26 + i * 12, it[i].n, U_SOFT, 1); ui_bar(pg, 90, y + 27 + i * 12, 90, 6, it[i].v, 100, it[i].v > it[i].bad ? C_RED : C_GOLD); snprintf(s, sizeof s, "%d PCT", it[i].v); text(pg, 186, y + 26 + i * 12, s, C_WHITE, 1); }
        for(int i = 0; i < b->nwarn && i < 3; i++) text(pg, 10, y + 80 + i * 8, b->warn[i], C_RED, 1);
    } else {
        static const char *const ln[4] = {"PLANTS","PLANT EATERS","HUNTERS","PEOPLE"}; static const int cc[4] = {C_GREEN, C_CYAN, C_GOLD, C_WHITE};
        rect(pg, 20, y, 200, 76, U_INK); frame1(pg, 20, y, 200, 76, U_RAISE);
        for(int k = 0; k < 4; k++) for(int i = 1; i < BIO_YEARS / 4; i++){ int x0 = 20 + (i - 1) * 200 / 29, x1 = 20 + i * 200 / 29, y0 = y + 74 - b->traj[k][i - 1] * 72 / 100, y1 = y + 74 - b->traj[k][i] * 72 / 100;
            for(int s2 = 0; s2 <= x1 - x0; s2++) pix(pg, x0 + s2, y0 + (y1 - y0) * s2 / (x1 - x0 ? x1 - x0 : 1), cc[k]); }
        for(int k = 0; k < 4; k++){ rect(pg, 20 + k * 52, y + 80, 5, 5, cc[k]); text(pg, 27 + k * 52, y + 80, ln[k], U_SOFT, 1); }
        if(b->collapse_year) { snprintf(s, sizeof s, "SOMETHING DIES OUT IN YEAR %d", b->collapse_year); text(pg, 10, y + 92, s, C_RED, 1); } else text(pg, 10, y + 92, "STEADY FOR 120 YEARS", C_GREEN, 1);
    }
}
static void cu_draw(volatile u16 *pg, int *rows, int nrows){
    ui_bg(pg, 0, 160);
    for(int i = 0; i < TB_N; i++){ int x = 1 + i * 34; int on = i == cu_tab; rect(pg, x, 1, 33, 11, on ? U_SEL : U_BG0); if(on) rect(pg, x, 11, 33, 1, U_ACCENT);
        text(pg, x + 17 - textlen(cu_tabname[i]) * 2, 4, cu_tabname[i], on ? C_GOLD : U_SOFT, 1); }
    if(cu_tab == TB_BIO){ cu_bio_page(pg); }
    else {
        ui_panel(pg, 2, 14, 130, 116); int top = cu_top[cu_tab], sel = cu_row[cu_tab];
        for(int i = 0; i < 12 && top + i < nrows; i++) cu_row_draw(pg, rows[top + i], 17 + i * 9, top + i == sel, 4, 126);
        if(nrows > 12){ int bh = 108 * 12 / nrows; rect(pg, 129, 17 + 100 * top / nrows * 108 / 100, 2, bh, U_ACCENT); }
        if(cu_tab == TB_PEOPLE) cu_figure(pg, 134, 14, 104, 64); else cu_planet(pg, 134, 14, 104, 64);
        cu_mini(pg, 134, 80, 104, 50);
    }
    ui_panel(pg, 2, 132, 236, 16);
    if(cu_note_t > 0) text(pg, 6, 137, cu_note, C_GOLD, 1);
    else { int id = cu_tab == TB_BIO ? -1 : rows[cu_row[cu_tab]]; const char *h = id >= 0 ? cu_help(id) : "LEFT RIGHT: NEXT PAGE. THE FOOTPRINT UPDATES AS YOU CHANGE THINGS."; char ls[2][60]; int k = pc_lw_wrap(h, 56, ls, 2); for(int i = 0; i < k; i++) text(pg, 6, 134 + i * 7, ls[i], U_SOFT, 1); }
    ui_foot(pg, "A: OPEN  SEL: DICE  START: UNDO", "L R: TAB  B: BACK");
}
// ================================================================= lists and pickers
typedef void (*CuNameFn)(int i, char *buf, int *col);
static int cu_list(volatile u16 *pg, const char *title, int n, CuNameFn fn, int cur){
    int sel = cur < 0 ? 0 : cur, dirty = 1, hu = 0, hd = 0; wait_release();
    while(1){
        if(dirty){ dirty = 0; ui_bg(pg, 0, 160); ui_panel(pg, 10, 6, 220, 140); char b[48]; snprintf(b, sizeof b, "%s", title); ui_head(pg, 16, 10, 208, b, C_GOLD); snprintf(b, sizeof b, "%d/%d", sel + 1, n); rtext(pg, 222, 10, b, U_SOFT);
            int top = sel - 6; if(top < 0) top = 0; if(top > n - 13) top = n - 13; if(top < 0) top = 0;
            for(int i = 0; i < 13 && top + i < n; i++){ char nm[60]; int col = 0; fn(top + i, nm, &col); nm[48] = 0; int y = 22 + i * 9; if(top + i == sel) ui_sel(pg, 14, y - 1, 212, 9, U_SEL);
                text(pg, 20, y, nm, col == 1 ? C_DGREY : top + i == sel ? C_GOLD : col == 2 ? C_CYAN : C_WHITE, 1); } 
            ui_foot(pg, "A: PICK  L R: PAGE", "B: CANCEL"); }
        vsync(); read_keys(); int u = pc_lw_rep(K_UP, &hu), d = pc_lw_rep(K_DOWN, &hd);
        if(u){ sel = (sel + n - 1) % n; dirty = 1; sfx(7); } if(d){ sel = (sel + 1) % n; dirty = 1; sfx(7); }
        if(HIT(K_L)){ sel = sel - 13 < 0 ? 0 : sel - 13; dirty = 1; sfx(7); } if(HIT(K_R)){ sel = sel + 13 >= n ? n - 1 : sel + 13; dirty = 1; sfx(7); }
        if(HIT(K_A)){ char nm[60]; int col = 0; fn(sel, nm, &col); if(col != 1){ sfx(2); wait_release(); return sel; } sfx(0); }
        if(HIT(K_B)){ wait_release(); return -1; }
    }
}
static int cu_pcat; static int cu_tmap[1100];
static void cu_fn_trait(int i, char *buf, int *col){ if(i == 0){ snprintf(buf, 60, "NONE"); *col = cu_r.trait[cu_pcat] == CR_NONE ? 2 : 0; return; } int id = cu_tmap[i]; snprintf(buf, 60, "%s", cr_trait_name(id)); *col = id == cu_r.trait[cu_pcat] ? 2 : cr_trait_ok(&cu_r, cu_pcat, id) ? 0 : 1; }
static int cu_pick_trait(volatile u16 *pg, int cat){
    int n = 1; cu_pcat = cat; cu_tmap[0] = -1; int cur = 0; for(int id = 0; id < cr_trait_count(); id++) if(cr_trait_cat(id) == cat){ cu_tmap[n] = id; if(id == cu_r.trait[cat]) cur = n; n++; }
    int r = cu_list(pg, cu_cat[cat], n, cu_fn_trait, cur); if(r < 0) return 0; cu_r.trait[cat] = r == 0 ? CR_NONE : (uint16_t)cu_tmap[r]; return 1;
}
static const char **cu_names; static void cu_fn_names(int i, char *buf, int *col){ snprintf(buf, 60, "%s", cu_names[i]); *col = 0; }
static int cu_pick_names(volatile u16 *pg, const char *title, const char *const *nm, int n, int cur){ cu_names = (const char **)nm; return cu_list(pg, title, n, cu_fn_names, cur); }
static void cu_fn_theme(int i, char *buf, int *col){ if(i == 0){ snprintf(buf, 60, "NONE"); *col = 0; return; } snprintf(buf, 60, "%s", cr_theme_name(i - 1)); *col = 0; }
static char cu_files[CR_LIBMAX][CR_NAME + 1]; static void cu_fn_file(int i, char *buf, int *col){ snprintf(buf, 60, "%s", cu_files[i]); *col = 0; }
// the animal builder: body plan x size x diet x home x nature
static int cu_animal(volatile u16 *pg, int slot){
    static const char *const sz[4] = {"TINY","SMALL","LARGE","HUGE"}, *const dt[3] = {"GRAZER","HUNTER","SCAVENGER"}, *const hb[4] = {"MARSH","HILLS","CAVES","SHORES"}, *const bh[4] = {"SHY","FIERCE","SWARMS","ALONE"};
    int k = slot >= 0 ? cu_r.cre[slot].species : (int)(cu_rng >> 33) % cr_species_count(); int amount = slot >= 0 ? cu_r.cre[slot].amount : 3;
    int plan = k % 66, v = k / 66, f[5] = {plan, v / 48, (v / 16) % 3, (v / 4) % 4, v % 4}; static const int lim[5] = {66, 4, 3, 4, 4}; int row = 0, dirty = 1, hr = 0, hl = 0; wait_release();
    while(1){
        if(dirty){ dirty = 0; k = cr_kind(f[0], f[1], f[2], f[3], f[4]); CrSpecies s; cr_species(k, &s); ui_bg(pg, 0, 160); ui_panel(pg, 10, 6, 220, 140); ui_head(pg, 16, 10, 208, slot >= 0 ? "CHANGE THIS ANIMAL" : "ADD AN ANIMAL", C_GOLD);
            char b[60]; const char *lab[5] = {"BODY PLAN", "SIZE", "DIET", "HOME", "NATURE"}; for(int i = 0; i < 6; i++){ int y = 24 + i * 10; if(i == row) ui_sel(pg, 14, y - 1, 212, 9, U_SEL); text(pg, 20, y, i < 5 ? lab[i] : "HOW MANY", i == row ? C_GOLD : U_SOFT, 1);
                if(i == 0) snprintf(b, sizeof b, "%d OF 66", f[0] + 1); else if(i == 1) snprintf(b, sizeof b, "%s", sz[f[1]]); else if(i == 2) snprintf(b, sizeof b, "%s", dt[f[2]]); else if(i == 3) snprintf(b, sizeof b, "%s", hb[f[3]]); else if(i == 4) snprintf(b, sizeof b, "%s", bh[f[4]]); else snprintf(b, sizeof b, "%d", amount);
                rtext(pg, 220, y, b, i == row ? C_WHITE : C_GREY); }
            char ls[3][60]; int n = pc_lw_wrap(s.name, 50, ls, 3); for(int i = 0; i < n; i++) text(pg, 20, 90 + i * 8, ls[i], C_GOLD, 1);
            snprintf(b, sizeof b, "EATS %s", s.eats); text(pg, 20, 116, b, C_CYAN, 1); snprintf(b, sizeof b, "KIND %d OF %d", k + 1, cr_species_count()); text(pg, 20, 125, b, U_SOFT, 1);
            ui_foot(pg, "A: OK  SEL: RANDOM", "B: CANCEL"); }
        vsync(); read_keys(); int r = pc_lw_rep(K_RIGHT, &hr), l = pc_lw_rep(K_LEFT, &hl);
        if(HIT(K_UP)){ row = (row + 5) % 6; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ row = (row + 1) % 6; dirty = 1; sfx(7); }
        if(r || l){ int d = r ? 1 : -1; if(row < 5) f[row] = cu_cyc(f[row], d, lim[row]); else { amount += d; if(amount < 1) amount = 1; if(amount > 9) amount = 9; } dirty = 1; sfx(7); }
        if(HIT(K_SEL)){ cu_rng = cu_rng * 6364136223846793005ull + 1442695040888963407ull; int q = (int)((cu_rng >> 33) % cr_species_count()); f[0] = q % 66; f[1] = q / 66 / 48; f[2] = (q / 66 / 16) % 3; f[3] = (q / 66 / 4) % 4; f[4] = q / 66 % 4; dirty = 1; sfx(6); }
        if(HIT(K_A)){ k = cr_kind(f[0], f[1], f[2], f[3], f[4]); if(slot >= 0){ cu_r.cre[slot].species = (uint16_t)k; cu_r.cre[slot].amount = (uint8_t)amount; } else { cu_r.cre[cu_r.ncre].species = (uint16_t)k; cu_r.cre[cu_r.ncre].amount = (uint8_t)amount; cu_r.ncre++; } sfx(2); wait_release(); return 1; }
        if(HIT(K_B)){ wait_release(); return 0; }
    }
}
// the plant book: n-th plant of this planet's catalogue (every plant once), or of the whole catalogue when pinning
static int cu_plants(volatile u16 *pg, int pin){
    uint64_t seed = pin ? 0xB00C0FFEEull + cu_r.salt : cr_seed(&cu_r) ^ 0xF10FAull; uint32_t n = 0; int dirty = 1, hr = 0, hl = 0; wait_release();
    while(1){
        if(dirty){ dirty = 0; uint32_t id = pin ? cr_plant_nth(seed, n) : (n < (uint32_t)cu_b.nlist ? cu_b.list[n] : (cu_b.nlist ? cu_b.list[0] : 0)); CrPlant p; cr_plant(id, &p);
            ui_bg(pg, 0, 160); ui_panel(pg, 10, 6, 220, 140); ui_head(pg, 16, 10, 208, pin ? "PICK A PLANT TO PIN" : "PLANT BOOK OF YOUR PLANET", C_GOLD); char b[64];
            snprintf(b, sizeof b, "%u OF %s", n + 1, pin ? "536,870,912" : "THIS PLANET"); rtext(pg, 222, 10, b, U_SOFT);
            text(pg, 18, 24, p.name, C_GREEN, 1); rect(pg, 18, 33, 8, 6, CP_SW + p.sw_leaf); rect(pg, 28, 33, 8, 6, CP_SW + p.sw_flower);
            snprintf(b, sizeof b, "%s  %s  %s  %s", p.form, p.habitat, p.size, p.use); text(pg, 40, 33, b, C_CYAN, 1);
            char ls[8][60]; int k = pc_lw_wrap(p.about, 52, ls, 8); for(int i = 0; i < k; i++) text(pg, 18, 46 + i * 8, ls[i], C_WHITE, 1);
            const char *gn[5] = {"AIR","FOOD","SHELTER","MEDICINE","FUEL"}; int gv[5] = {p.o2, p.food, p.shelter, p.medicine, p.fuel}; for(int i = 0; i < 5; i++){ text(pg, 18, 98 + i * 8, gn[i], U_SOFT, 1); ui_bar(pg, 66, 99 + i * 8, 50, 5, gv[i], 15, C_GREEN); }
            snprintf(b, sizeof b, "HEIGHT %d CM", p.height_cm); text(pg, 130, 98, b, U_SOFT, 1); snprintf(b, sizeof b, "NUMBER %u", id); text(pg, 130, 106, b, U_SOFT, 1);
            ui_foot(pg, pin ? "A: PIN  LEFT RIGHT: NEXT  UP DOWN: +20" : "LEFT RIGHT: NEXT  UP DOWN: +10", pin ? "L R: +1000  SEL: JUMP  B: BACK" : "B: BACK"); }
        vsync(); read_keys(); int r = pc_lw_rep(K_RIGHT, &hr), l = pc_lw_rep(K_LEFT, &hl); uint32_t lim = pin ? 536870912u : (uint32_t)(cu_b.nlist ? cu_b.nlist : 1); int st = pin ? 20 : 10;
        if(r){ n = (n + 1) % lim; dirty = 1; sfx(7); } if(l){ n = (n + lim - 1) % lim; dirty = 1; sfx(7); }
        if(HIT(K_DOWN)){ n = (n + st) % lim; dirty = 1; sfx(7); } if(HIT(K_UP)){ n = (n + lim - st % lim) % lim; dirty = 1; sfx(7); }
        if(pin && HIT(K_R)){ n = (n + 1000) % lim; dirty = 1; } if(pin && HIT(K_L)){ n = (n + lim - 1000) % lim; dirty = 1; }
        if(pin && HIT(K_SEL)){ cu_rng = cu_rng * 6364136223846793005ull + 1442695040888963407ull; n = (uint32_t)(cu_rng >> 33) % lim; dirty = 1; sfx(6); }
        if(pin && HIT(K_A)){ uint32_t id = cr_plant_nth(seed, n); cu_r.pin[cu_r.npin].id = id; cu_r.pin[cu_r.npin].amount = 5; cu_r.npin++; sfx(2); wait_release(); return 1; }
        if(HIT(K_B)){ wait_release(); return 0; }
    }
}
// the name: a grid of letters (A types, B deletes, START is done)
static int cu_name_edit(volatile u16 *pg){
    static const char chs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.'"; char nm[CR_NAME + 1]; snprintf(nm, sizeof nm, "%s", cu_r.name); int cx = 0, cy = 0, dirty = 1, hh[4] = {0, 0, 0, 0}; wait_release();
    while(1){
        if(dirty){ dirty = 0; ui_bg(pg, 0, 160); ui_panel(pg, 6, 6, 228, 140); ui_head(pg, 12, 10, 216, "NAME YOUR PLANET", C_GOLD);
            ui_well(pg, 14, 22, 212, 14); int L = (int)strlen(nm); text(pg, 20, 26, nm, C_WHITE, 1); if((cu_clock / 20) & 1) rect(pg, 20 + L * 4, 26, 3, 5, C_GOLD);
            char c2[8]; snprintf(c2, sizeof c2, "%d/%d", L, CR_NAME); rtext(pg, 222, 26, c2, U_SOFT);
            for(int i = 0; i < 44; i++){ int gx = i % 11, gy = i / 11, x = 14 + gx * 19, y = 42 + gy * 14, on = gx == cx && gy == cy; if(on) ui_sel(pg, x, y - 1, 18, 12, U_SEL); else rect(pg, x, y - 1, 18, 12, U_RAISE);
                char t[8] = {0}; const char *tx = t; if(i < 40) t[0] = chs[i]; else tx = i == 40 ? "DEL" : i == 41 ? "OK" : i == 42 ? "DICE" : "ESC"; if(i == 32) tx = "SP"; text(pg, x + 9 - textlen(tx) * 2, y + 2, tx, on ? C_GOLD : C_WHITE, 1); }
            ui_foot(pg, "A: TYPE  B: DELETE", "START: DONE  SEL: DICE"); }
        vsync(); read_keys(); cu_clock++; if((cu_clock % 20) == 0) dirty = 1;
        if(pc_lw_rep(K_RIGHT, &hh[0])){ cx = (cx + 1) % 11; dirty = 1; sfx(7); } if(pc_lw_rep(K_LEFT, &hh[1])){ cx = (cx + 10) % 11; dirty = 1; sfx(7); }
        if(pc_lw_rep(K_DOWN, &hh[2])){ cy = (cy + 1) % 4; dirty = 1; sfx(7); } if(pc_lw_rep(K_UP, &hh[3])){ cy = (cy + 3) % 4; dirty = 1; sfx(7); }
        int L = (int)strlen(nm), i = cy * 11 + cx;
        if(HIT(K_A)){ sfx(2); dirty = 1; if(i < 40){ if(L < CR_NAME){ nm[L] = chs[i]; nm[L + 1] = 0; } } else if(i == 40){ if(L) nm[L - 1] = 0; } else if(i == 41){ snprintf(cu_r.name, sizeof cu_r.name, "%s", nm); wait_release(); return 1; }
            else if(i == 42){ CrRecipe t = cu_r; cr_randomize(&t, CRS_NAME, &cu_rng); snprintf(nm, sizeof nm, "%s", t.name); } else { wait_release(); return 0; } }
        if(HIT(K_B) && L){ nm[L - 1] = 0; dirty = 1; sfx(4); }
        if(HIT(K_SEL)){ CrRecipe t = cu_r; cr_randomize(&t, CRS_NAME, &cu_rng); snprintf(nm, sizeof nm, "%s", t.name); dirty = 1; sfx(6); }
        if(HIT(K_START)){ snprintf(cu_r.name, sizeof cu_r.name, "%s", nm); wait_release(); return 1; }
    }
}
// ================================================================= actions, landing, the screen
static int cu_land(void){
    if(!cu_from_game || !endless || scene != 0){ cu_say(cu_from_game ? "LAND FROM A PLANET OF THE ENDLESS GALAXY" : "OPEN THIS FROM THE PC LAB WHILE YOU PLAY"); return 0; }
    const char *sv = pc_sav_path(); cr_fix(&cu_r); cr_lib_save(sv, &cu_r); cr_lib_save_seed(sv, &cu_r);
    uint32_t w4[4]; cr_uni4(&cu_r, w4); u32 u[UW]; for(int i = 0; i < UW; i++) u[i] = i < 4 ? w4[i] : 0; u32 z[UAX] = {0, 0, 0, 0, 0};
    plw_set_cheated(1); uni_set(u); ax_put(0, z); ax_put(1, z); dp_ix = 0; u_settle(); gen_cluster(); cur_sys = 6; cur_pl = cur_moon = 0; do_save(); sys_req = 1; return 1;
}
static int cu_open(volatile u16 *pg, int id, int *leave){
    CrRecipe *r = &cu_r; int ch = 0;
    if(id >= UR_TR0 && id < UR_TR0 + 9) return cu_pick_trait(pg, id - UR_TR0);
    if(id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE) return cu_animal(pg, id - UR_CRE0);
    if(id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN){ int sl = id - UR_PIN0; CrPin old = r->pin[sl]; if(r->npin < CR_MAXPIN && cu_plants(pg, 1)){ r->pin[sl] = r->pin[r->npin - 1]; r->npin--; (void)old; return 1; } if(r->npin >= CR_MAXPIN) cu_say("DELETE ONE FIRST (SELECT)"); return 0; }
    switch(id){
    case UR_CLASS: { int v = cu_pick_names(pg, "CLASS", cr_cls_name, 16, r->cls); if(v >= 0){ r->cls = (uint8_t)v; ch = 1; } break; }
    case UR_BODY: { int v = cu_pick_names(pg, "BODY", cr_body_name, 16, r->body); if(v >= 0){ r->body = (uint8_t)v; ch = 1; } break; }
    case UR_GIFT0: case UR_GIFT1: case UR_GIFT2: { int v = cu_pick_names(pg, "GIFT", cr_gift_name, 24, r->gift[id - UR_GIFT0]); if(v >= 0){ r->gift[id - UR_GIFT0] = (uint8_t)v; ch = 1; } break; }
    case UR_CALLING: { int v = cu_pick_names(pg, "CALLING", cr_calling_name, 16, r->calling); if(v >= 0){ r->calling = (uint8_t)v; ch = 1; } break; }
    case UR_FORM: { int v = cu_pick_names(pg, "CITY FORM", cr_cstyle_name, 16, r->cstyle & 15); if(v >= 0){ r->cstyle = (uint8_t)((r->cstyle & 0xF0) | v); ch = 1; } break; }
    case UR_TH0: case UR_TH1: { int slot = id - UR_TH0, v = cu_list(pg, "THEME", cr_theme_count() + 1, cu_fn_theme, r->theme[slot] == 255 ? 0 : r->theme[slot] + 1);
        if(v >= 0){ int t = v == 0 ? 255 : v - 1; if(t == 255 || cr_theme_ok(r, slot, t)){ r->theme[slot] = (uint8_t)t; ch = 1; } else cu_say("THAT THEME CLASHES WITH THE OTHER ONE"); } break; }
    case UR_SKY: case UR_GROUND: case UR_COL0: case UR_COL1: case UR_COL2: ch = cu_change(id, 1, 0); break;
    case UR_ADDCRE: ch = cu_animal(pg, -1); break;
    case UR_ADDPIN: ch = cu_plants(pg, 1); break;
    case UR_BOOK: cu_plants(pg, 0); break;
    case UR_DICE: cu_dice(); ch = 1; cu_say("NEW RANDOM CHOICES. START TAKES THEM BACK."); break;
    case UR_UNDO: if(cr_hist_undo(&cu_h, r)){ cr_bio(r, &cu_b); cu_say("UNDONE"); } else cu_say("NOTHING TO UNDO"); break;
    case UR_REDO: if(cr_hist_redo(&cu_h, r)){ cr_bio(r, &cu_b); cu_say("REDONE"); } else cu_say("NOTHING TO REDO"); break;
    case UR_SAVE: cr_fix(r); if(cr_lib_save(pc_sav_path(), r)) cu_say("SAVED NEXT TO YOUR SAVE FILE"); else cu_say("COULD NOT SAVE (NO SAVE FOLDER)"); break;
    case UR_LOAD: { int n = cr_lib_list(pc_sav_path(), cu_files, CR_LIBMAX); if(!n){ cu_say("NO SAVED RECIPES YET"); break; } int v = cu_list(pg, "SAVED RECIPES", n, cu_fn_file, 0); if(v >= 0){ CrRecipe t; if(cr_lib_load(pc_sav_path(), cu_files[v], &t)){ *r = t; ch = 1; cu_say("LOADED"); } else cu_say("COULD NOT READ IT"); } break; }
    case UR_CODE: { char code[400]; if(cr_encode(r, code, sizeof code)){ cr_code_write(pc_sav_path(), code); char m[60]; snprintf(m, sizeof m, "%s", code); cu_say("CODE WRITTEN TO THE .CODE FILE"); } break; }
    case UR_IMPORT: { char code[400]; CrRecipe t; if(cr_code_read(pc_sav_path(), code, sizeof code) && cr_decode(code, &t)){ *r = t; ch = 1; cu_say("CODE READ. THIS IS THAT PLANET"); } else cu_say("NO GOOD CODE IN THE .CODE FILE"); break; }
    case UR_EX: ch = cu_change(id, 1, 0); break;
    case UR_LANDON: if(cu_land()) *leave = 2; break;
    case UR_LEAVE: *leave = 1; break;
    case UR_NAMEDICE: ch = cu_name_edit(pg); break;
    default: ch = cu_change(id, 1, 0); }
    return ch;
}
static void pc_creator_screen(volatile u16 *pg, int from_game){
    int rows[40], nrows, leave = 0, dirty = 1; cu_from_game = from_game;
    for(int i = 0; i < 256; i++) cu_savepal[i] = BG_PAL[i];
    if(!cu_init){ cr_default(&cu_r, 0); cr_hist_init(&cu_h, &cu_r); cr_bio(&cu_r, &cu_b); cu_init = 1; }
    REG_DISPCNT = 4 | (1 << 10); oam_hide_all(); oam_flush(); cu_pal(); wait_release();
    while(!leave){
        nrows = cu_rows(cu_tab, rows); if(cu_row[cu_tab] >= nrows) cu_row[cu_tab] = nrows - 1; if(cu_row[cu_tab] < 0) cu_row[cu_tab] = 0;
        { int s = cu_row[cu_tab], *t = &cu_top[cu_tab]; if(s < *t) *t = s; if(s > *t + 11) *t = s - 11; if(*t < 0) *t = 0; }
        if(dirty){ dirty = 0; cu_pal(); cu_draw(pg, rows, nrows); }
        else if(cu_tab != TB_BIO && (cu_clock % 3) == 0){ if(cu_tab == TB_PEOPLE) ; else cu_planet(pg, 134, 14, 104, 64); }
        vsync(); read_keys(); cu_clock++; if(cu_note_t > 0 && --cu_note_t == 0) dirty = 1;
        int hr = pc_lw_rep(K_RIGHT, &cu_hold[0]), hl = pc_lw_rep(K_LEFT, &cu_hold[1]), hu = pc_lw_rep(K_UP, &cu_hold[2]), hd = pc_lw_rep(K_DOWN, &cu_hold[3]);
        int changed = 0, id = cu_tab == TB_BIO ? -1 : rows[cu_row[cu_tab]];
        if(HIT(K_L)){ cu_tab = (cu_tab + TB_N - 1) % TB_N; dirty = 1; sfx(7); } if(HIT(K_R)){ cu_tab = (cu_tab + 1) % TB_N; dirty = 1; sfx(7); }
        if(cu_tab == TB_BIO){ if(hr){ cu_sub = (cu_sub + 1) % 5; dirty = 1; sfx(7); } if(hl){ cu_sub = (cu_sub + 4) % 5; dirty = 1; sfx(7); } }
        else {
            if(hu){ cu_row[cu_tab] = (cu_row[cu_tab] + nrows - 1) % nrows; dirty = 1; sfx(7); } if(hd){ cu_row[cu_tab] = (cu_row[cu_tab] + 1) % nrows; dirty = 1; sfx(7); }
            if((hr || hl) && id < UR_DICE){ changed = cu_change(id, hr ? 1 : -1, hr == 2 || hl == 2); dirty = 1; if(changed) sfx(7); }
            else if((hr || hl) && id == UR_EX){ changed = cu_change(id, hr ? 1 : -1, 0); dirty = 1; }
            if(HIT(K_A)){ int lv = 0; changed = cu_open(pg, id, &lv); if(lv) leave = lv; dirty = 1; sfx(2); wait_release(); }
            if(HIT(K_SEL)){ if((id >= UR_CRE0 && id < UR_CRE0 + CR_MAXCRE)){ int s = id - UR_CRE0; for(int i = s; i < cu_r.ncre - 1; i++) cu_r.cre[i] = cu_r.cre[i + 1]; cu_r.ncre--; changed = 1; }
                else if(id >= UR_PIN0 && id < UR_PIN0 + CR_MAXPIN){ int s = id - UR_PIN0; for(int i = s; i < cu_r.npin - 1; i++) cu_r.pin[i] = cu_r.pin[i + 1]; cu_r.npin--; changed = 1; }
                else if(id == UR_NAMEDICE){ cu_name_dice(); changed = 1; sfx(6); }
                else if(cu_tab != TB_FILE && cu_tab != TB_FLORA){ cu_dice(); changed = 1; sfx(6); cu_say("DICE ROLLED. START TAKES IT BACK."); } dirty = 1; }
        }
        if(HIT(K_START)){ if(cr_hist_undo(&cu_h, &cu_r)){ cr_bio(&cu_r, &cu_b); cu_say("UNDONE (FILE TAB: REDO)"); } else cu_say("NOTHING TO UNDO"); dirty = 1; sfx(4); }
        if(changed){ cu_commit(); dirty = 1; }
        if(HIT(K_B)) leave = 1;
    }
    cr_fix(&cu_r); if(pc_sav_path()) cr_lib_save(pc_sav_path(), &cu_r);
    for(int i = 0; i < 256; i++) BG_PAL[i] = cu_savepal[i]; wait_release();
}
// ---- the landing: a created planet's world (called at the end of make_world; the recipe comes from memory or from the file next to the save)
static int pc_cr_on; static CrRecipe pc_cr_rec;
static void pc_cr_apply(void){
    pc_cr_on = 0; if(!(sys[cur_sys].kind == 6 && cur_sys == 6 && !cur_pl && !cur_moon && home_cluster() && cr_uni4_is(uni_w))) return;
    uint64_t seed = cr_uni4_seed(uni_w); if(!(cu_init && cr_seed(&cu_r) == seed)){ if(!(pc_cr_rec.name[0] && cr_seed(&pc_cr_rec) == seed) && !cr_lib_find_seed(pc_sav_path(), seed, &pc_cr_rec)) return; } else pc_cr_rec = cu_r;
    CrRecipe *r = &pc_cr_rec; CrWorld w; cr_world(r, &w); pc_cr_on = 1;
    scpy(w_name, w.name); wd.cls = w.cls; wd.psz = w.psz; wd.style = w.style; wd.rough = w.rough; wd.hour = w.hour; wd.weather = w.weather; wd.grav = w.grav; wd.snow = w.snow; wd.sand = w.sand;
    wd.city = w.city; wd.csize = w.csize; wd.ruins = w.ruins; wd.fauna = w.fauna; for(int i = 0; i < 4; i++) wd.flora[i] = w.flora[i]; wd.cliffs = w.cliffs; wd.twotone = w.twotone;
    wd.sky = w.sky; wd.bld = w.bld; wd.road = w.road; wd.tseed = w.tseed; wd.cseed = w.cseed; wd.weird = 0; wd.ntr = 0; wd.mood = MOOD_LOVE;
    for(int i = 0; i < 6; i++) w_tint[i] = w.tint[i];
    pc_tw = cr_world_tw(r); pc_k16 = cr_world_k16(r); if(pc_tw < 128){ wd.city = 0; wd.csize = 0; wd.ruins = 0; }
}
static int pc_cr_water(int w){ return pc_cr_on ? (pc_cr_rec.sea > 95 ? 95 : pc_cr_rec.sea) : w; }
