// cr_recipe.c: see cr_recipe.h. Pure functions of the recipe; no game code.
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "cr_recipe.h"
#include "../content/pc_content.h"
#include "../content/flora.h"
#include "cr_bio.h"

// ------------------------------------------------------------------ small helpers
static uint64_t mix64(uint64_t z){ z += 0x9E3779B97F4A7C15ull; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
static uint32_t rn(uint64_t *s, uint32_t n){ *s = mix64(*s); return (uint32_t)((*s >> 20) % n); }
static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }
#define RGB(r,g,b) ((uint16_t)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))

// ------------------------------------------------------------------ names
const char *const cr_cls_name[16] = {"TEMPERATE","MOUNTAIN","MEADOW","OCEAN","DESERT","ICE","JUNGLE","DEATH","CITY","PARADISE","INFERNO","BRIMSTONE","OBSIDIAN","BONE","BLOOD SEA","FROZEN HELL"};
const char *const cr_size_name[6] = {"TINY","SMALL","MEDIUM","LARGE","HUGE","GIANT"};
const char *const cr_xsize_name[9] = {"AS CHOSEN","PEBBLE","ROCK","ASTEROID","DWARF","X4 BIGGER","X16 BIGGER","X100 BIGGER","X1000 BIGGER"};
const char *const cr_land_name[8] = {"HILLS","MESAS","DUNES","ISLANDS","CRATERS","CANYONS","RIDGES","PLAINS"};
const char *const cr_hour_name[3] = {"DAY","DUSK","NIGHT"};
const char *const cr_weather_name[4] = {"CLEAR","RAIN","SNOW","DUST"};
const char *const cr_body_name[16] = {"ROUND","TALL","LONG","FLAT","WINGED","FURRY","SHELLED","MANY-LEGGED","FOUR-ARMED","ANTENNAED","TAILED","SLEEK","STOCKY","SPIKY","GLOWING","FEATHERED"};
const char *const cr_limb_name[6] = {"2 LIMBS","4 LIMBS","6 LIMBS","8 LIMBS","10 LIMBS","12 LIMBS"};
const char *const cr_gift_name[24] = {"NIGHT SIGHT","STRONG BACKS","SWEET SINGING","FIRE TAMING","WATER SENSE","SEED LORE","WIND READING","STONE SHAPING",
 "QUICK FEET","LONG LIVES","SILK SPINNING","BEE FRIENDS","STAR MAPS","HEALING HANDS","CALM MINDS","WEATHER SENSE",
 "GREEN THUMBS","TRADE LUCK","TUNNEL SENSE","HIGH JUMPS","WATER BREATH","LIGHT WINGS","FAST LEARNING","MEMORY KEEPING"};
const char *const cr_calling_name[16] = {"FARMERS","HERDERS","FISHERS","FORAGERS","BUILDERS","MINERS","TRADERS","WEAVERS","HEALERS","SCHOLARS","SINGERS","WANDERERS","WARDENS","SMITHS","SAILORS","STARGAZERS"};
const char *const cr_cstyle_name[16] = {"BURROW TOWN","TREE VILLAGE","STONE STACKS","REED HUTS","HANGING HIVES","DOME GARDENS","CANAL CITY","CLIFF NESTS","TENT CAMPS","MUD TERRACES","GLASS SPIRES","STILT PORTS","ROOT HALLS","FLOATING PADS","SHELL HOMES","LANTERN STREETS"};
const char *const cr_diet_name[6] = {"PLANTS","FRUIT","MEAT","MIXED","SCAVENGE","SUN AND SALT"};
const char *const cr_temper_name[8] = {"GENTLE","CURIOUS","PROUD","FIERCE","DREAMY","CAREFUL","WILD","WISE"};
const char *const cr_tech_name[8] = {"FIRE","STONE","BRONZE","IRON","STEAM","ENGINES","ELECTRIC","STAR"};
const char *const cr_div_name[10] = {"NONE","6 KINDS","12 KINDS","24 KINDS","48 KINDS","96 KINDS","160 KINDS","240 KINDS","320 KINDS","400 KINDS"};
static const uint16_t sw[CR_NSWATCH] = {                 // 25 swatches (the palette has 25 free slots, 39..63)
 RGB(30,30,42),RGB(100,100,112),RGB(170,170,178),RGB(245,245,245),RGB(110,70,40),RGB(170,120,70),RGB(225,185,130),RGB(250,215,170),
 RGB(200,40,50),RGB(235,100,40),RGB(240,200,60),RGB(170,220,60),RGB(60,170,70),RGB(30,120,90),RGB(40,170,170),RGB(80,200,240),
 RGB(50,100,220),RGB(40,50,150),RGB(120,70,200),RGB(190,90,220),RGB(230,90,170),RGB(250,160,190),RGB(140,40,60),RGB(90,100,40),RGB(180,230,210) };
uint16_t cr_swatch(int i){ return sw[i % CR_NSWATCH]; }
int cr_height_cm(int h){ static const int hs[8] = {6, 12, 25, 50, 90, 140, 180, 240}; return hs[h & 7]; }
double cr_pop_count(int p){ double v = 1000.0; for(int i = 0; i < p; i++) v *= 3.4; return v; }       // 1,000 .. about 4e10
void cr_people_name(const CrRecipe *r, char *out){
    static const char *const a[] = {"KO","VA","ZE","MI","TOR","SYL","BRA","NU","QUI","DRA","ELO","FAR","GOR","HAL","IX","JUN"};
    static const char *const b[] = {"RA","LU","NE","SA","KO","VI","MON","TAR"};
    static const char *const c[] = {"I","ANS","ENE","IN","OLK","ARI","UM","ESH"};
    uint64_t s = mix64(((uint64_t)r->body << 40) ^ ((uint64_t)r->gift[0] << 32) ^ ((uint64_t)r->col[0] << 24) ^ ((uint64_t)r->calling << 16) ^ r->salt);
    snprintf(out, 24, "%s%s%s", a[rn(&s, 16)], b[rn(&s, 8)], c[rn(&s, 8)]);
}

// ------------------------------------------------------------------ traits and themes (through pc_content)
int cr_trait_count(void){ return trait_count(); }
const char *cr_trait_name(int id){ return trait_name(id); }
int cr_trait_cat(int id){ return trait_cat(id); }
int cr_theme_count(void){ return theme_count(); }
const char *cr_theme_name(int id){ return theme_get(id)->name; }
int cr_trait_ok(const CrRecipe *r, int cat, int id){
    if(id < 0 || id >= trait_count() || trait_cat(id) != cat) return 0;
    for(int c = 0; c < CR_NCAT; c++) if(c != cat && r->trait[c] != CR_NONE && r->trait[c] < trait_count() && traits_conflict(id, r->trait[c]) && trait_cat(r->trait[c]) != cat) return 0;
    return 1;
}
int cr_theme_ok(const CrRecipe *r, int slot, int id){ int o = r->theme[slot ^ 1]; return id < theme_count() && (o == 255 || theme_pair_ok(id, o)); }
static int tid(const char *n){ for(int i = 0; i < trait_count(); i++) if(!strcmp(trait_name(i), n)) return i; return CR_NONE; }
static int thid(const char *n){ for(int i = 0; i < theme_count(); i++) if(!strcmp(theme_get(i)->name, n)) return i; return 255; }

// ------------------------------------------------------------------ fix: clamp everything, zero the unused, drop clashing traits
void cr_fix(CrRecipe *r){
    int i, n = (int)strlen(r->name);
    for(i = 0; i < CR_NAME; i++){ char c = i < n ? r->name[i] : 0; if(c >= 'a' && c <= 'z') c -= 32;
        if(c && !((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '.' || c == '\'')) c = ' ';
        r->name[i] = c; }
    r->name[CR_NAME] = 0; for(i = (int)strlen(r->name); i > 0 && r->name[i - 1] == ' '; i--) r->name[i - 1] = 0; n = (int)strlen(r->name); for(i = n; i <= CR_NAME; i++) r->name[i] = 0;
    r->cls %= 16; r->size = (uint8_t)clampi(r->size, 0, 5); r->xsize = (uint8_t)clampi(r->xsize, 0, 8); r->sea = (uint8_t)clampi(r->sea, 0, 100); r->land &= 7; r->rough &= 3; r->hour %= 3; r->weather &= 3;
    r->sky %= CR_NSWATCH; r->ground %= CR_NSWATCH; r->climate = (int8_t)clampi(r->climate, -8, 8);
    for(i = 0; i < CR_NCAT; i++){ if(r->trait[i] != CR_NONE && (r->trait[i] >= trait_count() || trait_cat(r->trait[i]) != i)) r->trait[i] = CR_NONE; }
    for(i = 0; i < CR_NCAT; i++) if(r->trait[i] != CR_NONE && !cr_trait_ok(r, i, r->trait[i])) r->trait[i] = CR_NONE;
    for(i = 0; i < 2; i++) if(r->theme[i] != 255 && r->theme[i] >= theme_count()) r->theme[i] = 255;
    if(r->theme[0] != 255 && r->theme[1] != 255 && !theme_pair_ok(r->theme[0], r->theme[1])) r->theme[1] = 255;
    r->body &= 15; r->limbs = (uint8_t)clampi(r->limbs, 0, 5); r->height &= 7;
    for(i = 0; i < 3; i++) r->col[i] %= CR_NSWATCH;
    for(i = 0; i < 3; i++) r->gift[i] %= 24;
    for(i = 1; i < 3; i++) for(int j = 0; j < i; j++) if(r->gift[i] == r->gift[j]){ r->gift[i] = (uint8_t)((r->gift[i] + 1) % 24); j = -1; }
    r->stock &= 31; r->calling &= 15; r->diet %= 6; r->temper &= 7; r->pop &= 15; r->tech &= 7;
    r->cover = (uint8_t)clampi(r->cover, 0, 100); r->div = (uint8_t)clampi(r->div, 0, 9);
    r->ncre = (uint8_t)clampi(r->ncre, 0, CR_MAXCRE);
    for(i = 0; i < CR_MAXCRE; i++){ if(i >= r->ncre){ r->cre[i].species = 0; r->cre[i].amount = 0; } else { r->cre[i].species = (uint16_t)(r->cre[i].species % cr_species_count()); r->cre[i].amount = (uint8_t)clampi(r->cre[i].amount, 1, 9); } }
    r->npin = (uint8_t)clampi(r->npin, 0, CR_MAXPIN);
    for(i = 0; i < CR_MAXPIN; i++){ if(i >= r->npin){ r->pin[i].id = 0; r->pin[i].amount = 0; } else { r->pin[i].id &= FL_TOTAL - 1; r->pin[i].amount = (uint8_t)clampi(r->pin[i].amount, 1, 9); } }
}

// ------------------------------------------------------------------ defaults
static void set_name(CrRecipe *r, const char *n){ memset(r->name, 0, sizeof r->name); strncpy(r->name, n, CR_NAME); }
static void add_cre(CrRecipe *r, int role, int salt, int amount){ if(r->ncre < CR_MAXCRE){ r->cre[r->ncre].species = (uint16_t)cr_kind_find(role, salt); r->cre[r->ncre].amount = (uint8_t)amount; r->ncre++; } }
void cr_default(CrRecipe *r, uint32_t n){
    memset(r, 0, sizeof *r); for(int i = 0; i < CR_NCAT; i++) r->trait[i] = CR_NONE; r->theme[0] = r->theme[1] = 255;
    r->salt = 0x5EED0000u + n * 7919u; r->stock = (uint8_t)(n * 7 % 32);
    switch(n % 4){
    default:
    case 0: set_name(r, "HOMEWORLD"); r->cls = 0; r->size = 2; r->sea = 68; r->land = 0; r->rough = 1; r->hour = 0; r->weather = 0; r->sky = 15; r->ground = 0; r->climate = 0;
        r->trait[TC_GRAVITY] = (uint16_t)tid("EARTHLIKE GRAVITY"); r->trait[TC_DAY] = (uint16_t)tid("EVEN DAYS"); r->trait[TC_AIR] = (uint16_t)tid("FRESH BREEZE"); r->trait[TC_WEATHER] = (uint16_t)tid("CLEAR SKIES");
        r->body = 1; r->limbs = 1; r->height = 4; r->col[0] = 7; r->col[1] = 12; r->col[2] = 16; r->gift[0] = 16; r->gift[1] = 14; r->gift[2] = 8; r->calling = 0; r->cstyle = 1; r->diet = 3; r->temper = 1; r->pop = 12; r->tech = 3;
        r->cover = 70; r->div = 5;
        add_cre(r, ROLE_GRAZER, 1, 6); add_cre(r, ROLE_BROWSER, 2, 4); add_cre(r, ROLE_SEEDEATER, 3, 5); add_cre(r, ROLE_HUNTER, 4, 3); add_cre(r, ROLE_APEX, 5, 1); add_cre(r, ROLE_SCAVENGER, 6, 3); add_cre(r, ROLE_FILTER, 7, 4); add_cre(r, ROLE_FISHER, 8, 2); break;
    case 1: set_name(r, "EMBER REACH"); r->cls = 4; r->size = 3; r->sea = 12; r->land = 2; r->rough = 2; r->hour = 1; r->weather = 3; r->sky = 9; r->ground = 6; r->climate = 0;
        r->trait[TC_GRAVITY] = (uint16_t)tid("LIGHT GRAVITY"); r->trait[TC_SKY] = (uint16_t)tid("TWIN SUNS"); r->trait[TC_SURFACE] = (uint16_t)tid("GLASS DUNES");
        r->body = 6; r->limbs = 2; r->height = 3; r->col[0] = 9; r->col[1] = 10; r->col[2] = 5; r->gift[0] = 5; r->gift[1] = 1; r->gift[2] = 18; r->calling = 5; r->cstyle = 0; r->diet = 1; r->temper = 5; r->pop = 9; r->tech = 2;
        r->cover = 35; r->div = 3; add_cre(r, ROLE_SEEDEATER, 11, 4); add_cre(r, ROLE_HUNTER, 12, 3); add_cre(r, ROLE_APEX, 13, 1); add_cre(r, ROLE_SCAVENGER, 14, 2); break;
    case 2: set_name(r, "DEEP GREEN"); r->cls = 6; r->size = 4; r->sea = 55; r->land = 7; r->rough = 1; r->hour = 0; r->weather = 1; r->sky = 13; r->ground = 12; r->climate = 2;
        r->trait[TC_GRAVITY] = (uint16_t)tid("EARTHLIKE GRAVITY"); r->trait[TC_SURFACE] = (uint16_t)tid("GIANT TREES"); r->trait[TC_LIFE] = (uint16_t)tid("BUG GARDEN"); r->trait[TC_WEATHER] = (uint16_t)tid("STORMY");
        r->body = 7; r->limbs = 2; r->height = 2; r->col[0] = 12; r->col[1] = 10; r->col[2] = 11; r->gift[0] = 11; r->gift[1] = 16; r->gift[2] = 4; r->calling = 3; r->cstyle = 4; r->diet = 3; r->temper = 0; r->pop = 10; r->tech = 1;
        r->cover = 95; r->div = 8; add_cre(r, ROLE_BROWSER, 21, 7); add_cre(r, ROLE_NECTAR, 22, 5); add_cre(r, ROLE_GRAZER, 23, 7); add_cre(r, ROLE_HUNTER, 24, 4); add_cre(r, ROLE_AMBUSHER, 25, 3); add_cre(r, ROLE_SCAVENGER, 26, 3); add_cre(r, ROLE_FILTER, 27, 5); break;
    case 3: set_name(r, "FROST CROWN"); r->cls = 5; r->size = 1; r->sea = 30; r->land = 6; r->rough = 3; r->hour = 2; r->weather = 2; r->sky = 17; r->ground = 3; r->climate = -4;
        r->trait[TC_GRAVITY] = (uint16_t)tid("LIGHT GRAVITY"); r->trait[TC_SKY] = (uint16_t)tid("RINGS"); r->trait[TC_AIR] = (uint16_t)tid("GLOW AURORA");
        r->body = 5; r->limbs = 1; r->height = 5; r->col[0] = 3; r->col[1] = 15; r->col[2] = 17; r->gift[0] = 2; r->gift[1] = 12; r->gift[2] = 21; r->calling = 9; r->cstyle = 5; r->diet = 2; r->temper = 7; r->pop = 8; r->tech = 4;
        r->cover = 25; r->div = 3; add_cre(r, ROLE_BROWSER, 31, 4); add_cre(r, ROLE_HUNTER, 32, 3); add_cre(r, ROLE_APEX, 33, 1); add_cre(r, ROLE_SCAVENGER, 34, 2); break;
    }
    r->name[CR_NAME] = 0; cr_fix(r);
}

// ------------------------------------------------------------------ env: the traits and the class added up
// extra light (percent of a normal sun) that a few sky and air traits give
static int trait_light(int id){ const char *n = trait_name(trait_base(id));
    static const struct { const char *n; int l; } t[] = { {"TWIN SUNS",150},{"RED GIANT SKY",120},{"DEAD SUN",8},{"ENDLESS FOG",60},{"DUST VEIL",75},{"AMBER HAZE",85},{"LONG NIGHT",45},{"NEARBY NEBULA",90},{"ACID CLOUDS",55},{"ASH FALL",65} };
    for(unsigned i = 0; i < sizeof t / sizeof t[0]; i++) if(!strcmp(n, t[i].n)) return t[i].l; return 100; }
void cr_env(const CrRecipe *r, CrEnv *e){
    static const int8_t ctemp[16] = {0,-8,0,-4,10,-35,10,5,3,0,45,30,8,8,18,-45};
    uint16_t ids[CR_NCAT]; int n = 0, light = 100;
    for(int c = 0; c < CR_NCAT; c++) if(r->trait[c] != CR_NONE){ ids[n++] = r->trait[c]; int l = trait_light(r->trait[c]); light = light * l / 100; }
    PlanetInfo p; planet_from_traits(ids, n, &p);
    e->gravity_pct = p.gravity_pct; e->day_hours = p.day_hours; e->temp_c = p.temp_c + ctemp[r->cls] + r->climate * 5; e->light = light; e->danger = p.danger; e->life = p.life;
}

// ------------------------------------------------------------------ world: what the game needs
static const uint8_t cbase[16][3] = {{110,150,70},{150,125,105},{140,215,95},{70,180,130},{235,195,110},{215,232,255},{40,160,50},{130,30,22},{150,150,165},{235,150,205},{200,70,25},{200,190,40},{40,35,60},{225,215,190},{150,15,35},{70,120,190}};
static uint16_t rgb15c(int r, int g, int b){ r = clampi(r, 0, 255); g = clampi(g, 0, 255); b = clampi(b, 0, 255); return RGB(r, g, b); }
void cr_world(const CrRecipe *r, CrWorld *w){
    CrEnv e; cr_env(r, &e); memset(w, 0, sizeof *w);
    w->cls = r->cls; w->psz = r->xsize >= 5 ? 5 : r->xsize >= 1 ? 0 : r->size; w->style = r->land; w->rough = r->rough; w->hour = r->hour; w->weather = r->weather;
    w->grav = e.gravity_pct <= 70 ? 0 : e.gravity_pct <= 130 ? 1 : e.gravity_pct <= 200 ? 2 : 3;
    int temp = e.temp_c;
    w->snow = temp < 2; w->sand = (temp > 28 && r->sea < 50) || r->land == 2;
    w->water = r->sea;
    w->city = r->pop >= 2; w->csize = (uint8_t)clampi(r->pop / 4, 0, 3);
    w->ruins = r->trait[TC_ANCIENT] != CR_NONE && trait_name(r->trait[TC_ANCIENT]) && strcmp(trait_name(r->trait[TC_ANCIENT]), "NOTHING OLD") != 0;
    w->fauna = (uint8_t)(r->ncre > 5 ? 5 : r->ncre); w->cliffs = r->rough == 3; w->twotone = (r->salt >> 3) % 3 == 0;
    int fl = 1 + r->cover * 6 / 100 + (r->div > 6); for(int t = 0; t < 4; t++) w->flora[t] = (uint8_t)clampi(fl + ((t & 1) ? 0 : 1) - (t == 3), 1, 7);
    // ground ramp: the class colour, or the swatch you picked
    int cr = cbase[r->cls][0], cg = cbase[r->cls][1], cb = cbase[r->cls][2];
    if(r->ground){ uint16_t c = sw[r->ground]; cr = (c & 31) << 3; cg = ((c >> 5) & 31) << 3; cb = ((c >> 10) & 31) << 3; }
    static const uint8_t mul[5] = {8, 30, 55, 80, 100};
    for(int i = 0; i < 5; i++){ int m = mul[i], add = i == 4 ? 120 : (i == 3 ? 30 : 0); w->tint[i] = rgb15c(cr * m / 100 + add, cg * m / 100 + add, cb * m / 100 + add); }
    w->tint[5] = rgb15c(cr * 45 / 100, cg * 45 / 100, cb * 45 / 100);
    w->sky = sw[r->sky];
    { uint16_t c = sw[r->col[2]]; int br = (c & 31) << 3, bg = ((c >> 5) & 31) << 3, bb = ((c >> 10) & 31) << 3; w->bld = rgb15c(60 + br * 160 / 255, 60 + bg * 160 / 255, 60 + bb * 160 / 255); }
    w->road = rgb15c(60, 56, 60);
    // the land comes from the salt (so editing the people never reshapes the land); the city from the people
    uint64_t t = mix64(((uint64_t)r->salt << 8) ^ 0x7E44A1ull); w->tseed = (uint32_t)(t >> 16);
    uint64_t c = mix64(((uint64_t)r->body << 56) ^ ((uint64_t)r->cstyle << 48) ^ ((uint64_t)r->col[2] << 40) ^ ((uint64_t)r->pop << 32) ^ r->salt ^ 0xC17Aull); w->cseed = (uint32_t)(c >> 16);
    snprintf(w->name, sizeof w->name, "%s", r->name[0] ? r->name : "NEW WORLD");
}

double cr_radius_km(const CrRecipe *r){
    static const double rk[6] = {2000, 3500, 6371, 9000, 13000, 20000}; static const double small[5] = {0, 0.5, 3, 30, 500}, big[9] = {0, 0, 0, 0, 0, 4, 16, 100, 1000};
    if(r->xsize >= 1 && r->xsize <= 4) return small[r->xsize]; if(r->xsize >= 5) return rk[r->size] * big[r->xsize]; return rk[r->size];
}
int cr_world_tw(const CrRecipe *r){ static const int t[5] = {128, 8, 16, 32, 64}; return r->xsize >= 1 && r->xsize <= 4 ? t[r->xsize] : 128; }
int cr_world_k16(const CrRecipe *r){
    static const int ks[6] = {12, 7, 4, 3, 2, 1};
    if(r->xsize >= 1 && r->xsize <= 4){ int P = cr_world_tw(r); double k = 16.0 * 256.0 * 2 * 3.14159265 / P; return k > 1024 ? 1024 : (int)(k + 0.5); }
    int k = 16 * ks[r->size % 6]; if(r->xsize >= 5){ static const int div[9] = {1, 1, 1, 1, 1, 3, 9, 30, 120}; k /= div[r->xsize]; if(k < 1) k = 1; } return k;
}
// ------------------------------------------------------------------ the created universe
void cr_uni4(const CrRecipe *r, uint32_t w[4]){ uint64_t s = cr_seed(r); w[0] = CR_UNI_MARK; w[1] = CR_UNI_MARK ^ 0x5EEDC0DEu ^ (uint32_t)(s >> 32) ^ (uint32_t)s; w[2] = (uint32_t)(s >> 32); w[3] = (uint32_t)s; }
int cr_uni4_is(const uint32_t *w){ return w[0] == CR_UNI_MARK && w[1] == (CR_UNI_MARK ^ 0x5EEDC0DEu ^ w[2] ^ w[3]); }
uint64_t cr_uni4_seed(const uint32_t *w){ return ((uint64_t)w[2] << 32) | w[3]; }

// ------------------------------------------------------------------ the planet seed: every field of the recipe, in a fixed order
uint64_t cr_seed(const CrRecipe *r){
    CrRecipe q = *r; cr_fix(&q);
    uint64_t h = 0xC4EA70A5EEDull;
#define MIXV(v) h = mix64(h ^ (uint64_t)(v) ^ ((uint64_t)__LINE__ << 50))
    for(int i = 0; i < CR_NAME; i++) MIXV(q.name[i]);
    MIXV(q.cls); MIXV(q.size); if(q.xsize) MIXV(q.xsize); MIXV(q.sea); MIXV(q.land); MIXV(q.rough); MIXV(q.hour); MIXV(q.weather); MIXV(q.sky); MIXV(q.ground); MIXV((uint8_t)q.climate); MIXV(q.salt);
    for(int i = 0; i < CR_NCAT; i++) MIXV(q.trait[i]);
    MIXV(q.theme[0]); MIXV(q.theme[1]);
    MIXV(q.body); MIXV(q.limbs); MIXV(q.height); for(int i = 0; i < 3; i++){ MIXV(q.col[i]); MIXV(q.gift[i]); }
    if(q.stock) MIXV(q.stock); MIXV(q.calling); MIXV(q.cstyle); MIXV(q.diet); MIXV(q.temper); MIXV(q.pop); MIXV(q.tech);
    MIXV(q.cover); MIXV(q.div); MIXV(q.ncre); for(int i = 0; i < q.ncre; i++){ MIXV(q.cre[i].species); MIXV(q.cre[i].amount); }
    MIXV(q.npin); for(int i = 0; i < q.npin; i++){ MIXV(q.pin[i].id); MIXV(q.pin[i].amount); }
#undef MIXV
    return mix64(h);
}

int cr_equal(const CrRecipe *a, const CrRecipe *b){ CrRecipe x = *a, y = *b; cr_fix(&x); cr_fix(&y); return !memcmp(&x, &y, sizeof x); }

// ------------------------------------------------------------------ randomise
static void rnd_world(CrRecipe *r, uint64_t *s){
    r->cls = (uint8_t)rn(s, 10); if(rn(s, 8) == 0) r->cls = (uint8_t)(10 + rn(s, 6));
    r->size = (uint8_t)rn(s, 6); r->xsize = (uint8_t)(rn(s, 6) == 0 ? 1 + rn(s, 8) : 0); r->sea = (uint8_t)(r->cls == 3 ? 78 + rn(s, 20) : r->cls == 4 ? rn(s, 15) : r->cls == 5 ? 10 + rn(s, 60) : 10 + rn(s, 85));
    r->land = (uint8_t)rn(s, 8); r->rough = (uint8_t)rn(s, 4); r->hour = (uint8_t)(rn(s, 3) == 0 ? 1 + rn(s, 2) : 0); r->weather = (uint8_t)rn(s, 4);
    r->sky = (uint8_t)rn(s, CR_NSWATCH); r->ground = (uint8_t)(rn(s, 3) ? 0 : 1 + rn(s, CR_NSWATCH - 1)); r->climate = (int8_t)((int)rn(s, 13) - 6); r->salt = (uint32_t)(mix64(*s) >> 11);
}
static void rnd_traits(CrRecipe *r, uint64_t *s){
    for(int c = 0; c < CR_NCAT; c++) r->trait[c] = CR_NONE;
    int nt = trait_count(), want = 3 + (int)rn(s, 4);
    for(int tries = 0, got = 0; tries < 200 && got < want; tries++){ int id = (int)rn(s, nt), cat = trait_cat(id); if(r->trait[cat] != CR_NONE) continue; if(!cr_trait_ok(r, cat, id)) continue; r->trait[cat] = (uint16_t)id; got++; }
    r->theme[0] = r->theme[1] = 255; if(rn(s, 2)) { r->theme[0] = (uint8_t)rn(s, theme_count()); if(rn(s, 2)) for(int t = 0; t < 20; t++){ int b = (int)rn(s, theme_count()); if(theme_pair_ok(b, r->theme[0])){ r->theme[1] = (uint8_t)b; break; } } }
}
static void rnd_people(CrRecipe *r, uint64_t *s){
    r->body = (uint8_t)rn(s, 16); r->limbs = (uint8_t)rn(s, 6); r->height = (uint8_t)rn(s, 8); for(int i = 0; i < 3; i++){ r->col[i] = (uint8_t)rn(s, CR_NSWATCH); r->gift[i] = (uint8_t)rn(s, 24); }
    r->stock = (uint8_t)rn(s, 32); r->calling = (uint8_t)rn(s, 16); r->cstyle = (uint8_t)rn(s, 256); r->diet = (uint8_t)rn(s, 6); r->temper = (uint8_t)rn(s, 8); r->pop = (uint8_t)(4 + rn(s, 11)); r->tech = (uint8_t)rn(s, 8);
}
static void rnd_life(CrRecipe *r, uint64_t *s){
    r->cover = (uint8_t)(20 + rn(s, 80)); r->div = (uint8_t)(2 + rn(s, 8)); r->ncre = (uint8_t)(3 + rn(s, 6));
    int ns = cr_species_count();
    for(int i = 0; i < CR_MAXCRE; i++){ if(i < r->ncre){ int sp = (int)rn(s, ns); r->cre[i].species = (uint16_t)sp; r->cre[i].amount = (uint8_t)(1 + rn(s, 8)); } else { r->cre[i].species = 0; r->cre[i].amount = 0; } }
    r->npin = (uint8_t)rn(s, 4); for(int i = 0; i < CR_MAXPIN; i++){ if(i < r->npin){ r->pin[i].id = flora_catalogue(mix64(*s), (uint32_t)rn(s, 100000)); r->pin[i].amount = (uint8_t)(1 + rn(s, 9)); } else { r->pin[i].id = 0; r->pin[i].amount = 0; } }
}
static void rnd_name(CrRecipe *r, uint64_t *s){
    static const char *const a[] = {"KO","VA","ZE","MI","TOR","SYL","BRA","NU","QUI","DRA","ELO","FAR","GOR","HAL","IX","JUN"};
    static const char *const b[] = {"RA","LU","NE","SA","KO","VI","MON","TAR","LEX","DAN","PHI","ZUN"};
    static const char *const c[] = {"X","N","S","TH","R","M","DE","K"};
    char buf[24]; snprintf(buf, sizeof buf, "%s%s%s", a[rn(s, 16)], b[rn(s, 12)], rn(s, 2) ? c[rn(s, 8)] : ""); set_name(r, buf);
}
void cr_randomize(CrRecipe *r, int section, uint64_t *rng){
    if(section < 0 || section == CRS_WORLD) rnd_world(r, rng);
    if(section < 0 || section == CRS_TRAITS) rnd_traits(r, rng);
    if(section < 0 || section == CRS_PEOPLE) rnd_people(r, rng);
    if(section < 0 || section == CRS_LIFE) rnd_life(r, rng);
    if(section < 0 || section == CRS_NAME) rnd_name(r, rng);
    cr_fix(r);
}

// ------------------------------------------------------------------ share code
typedef struct { uint8_t b[256]; int n; } Bits;
static void put(Bits *w, uint32_t v, int bits){ for(int i = bits - 1; i >= 0; i--){ if(w->n < 2040){ if((v >> i) & 1) w->b[w->n >> 3] |= 0x80 >> (w->n & 7); } w->n++; } }
static uint32_t get(Bits *r, int bits, int *bad){ uint32_t v = 0; for(int i = 0; i < bits; i++){ if(r->n >= 2040){ *bad = 1; return 0; } v = (v << 1) | ((r->b[r->n >> 3] >> (7 - (r->n & 7))) & 1); r->n++; } return v; }
static const char b32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
static const char chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.'";
static uint32_t sum12(const uint8_t *b, int nbits){ uint32_t h = 2166136261u; for(int i = 0; i < (nbits + 7) / 8; i++){ h ^= b[i]; h *= 16777619u; } h ^= (uint32_t)nbits; h *= 16777619u; return (h ^ (h >> 12) ^ (h >> 24)) & 0xFFF; }
int cr_encode(const CrRecipe *r0, char *out, int cap){
    CrRecipe q = *r0; cr_fix(&q); const CrRecipe *r = &q; Bits w; memset(&w, 0, sizeof w);
    put(&w, 2, 4);                                                    // version
    int nl = (int)strlen(r->name); put(&w, (uint32_t)nl, 5);
    for(int i = 0; i < nl; i++){ const char *p = strchr(chars, r->name[i]); put(&w, p ? (uint32_t)(p - chars) : 0, 6); }
    put(&w, r->cls, 4); put(&w, r->size, 3); put(&w, r->xsize, 4); put(&w, r->sea, 7); put(&w, r->land, 3); put(&w, r->rough, 2); put(&w, r->hour, 2); put(&w, r->weather, 2);
    put(&w, r->sky, 5); put(&w, r->ground, 5); put(&w, (uint32_t)(r->climate + 8), 5); put(&w, r->salt, 32);
    uint32_t mask = 0; for(int c = 0; c < CR_NCAT; c++) if(r->trait[c] != CR_NONE) mask |= 1u << c;
    put(&w, mask, 9); for(int c = 0; c < CR_NCAT; c++) if(mask >> c & 1) put(&w, r->trait[c], 11);
    for(int i = 0; i < 2; i++){ put(&w, r->theme[i] != 255, 1); if(r->theme[i] != 255) put(&w, r->theme[i], 8); }
    put(&w, r->body, 4); put(&w, r->limbs, 3); put(&w, r->height, 3); for(int i = 0; i < 3; i++) put(&w, r->col[i], 5); for(int i = 0; i < 3; i++) put(&w, r->gift[i], 5);
    put(&w, r->stock, 5); put(&w, r->calling, 4); put(&w, r->cstyle, 8); put(&w, r->diet, 3); put(&w, r->temper, 3); put(&w, r->pop, 4); put(&w, r->tech, 3);
    put(&w, r->cover, 7); put(&w, r->div, 4);
    put(&w, r->ncre, 4); for(int i = 0; i < r->ncre; i++){ put(&w, r->cre[i].species, 14); put(&w, r->cre[i].amount, 4); }
    put(&w, r->npin, 4); for(int i = 0; i < r->npin; i++){ put(&w, r->pin[i].id, 29); put(&w, r->pin[i].amount, 4); }
    put(&w, sum12(w.b, w.n), 12);
    if(w.n > 2040) return 0;
    int chs = (w.n + 4) / 5, len = 4 + chs + (chs - 1) / 5; if(len + 1 > cap) return 0;
    char *p = out; *p++ = 'P'; *p++ = 'L'; *p++ = '2';
    for(int i = 0; i < chs; i++){ if(i % 5 == 0) *p++ = '-'; uint32_t v = 0; for(int k = 0; k < 5; k++){ int bi = i * 5 + k; v = (v << 1) | (bi < w.n ? ((w.b[bi >> 3] >> (7 - (bi & 7))) & 1) : 0); } *p++ = b32[v]; }
    *p = 0; return (int)(p - out);
}
int cr_decode(const char *code, CrRecipe *r){
    Bits rd; memset(&rd, 0, sizeof rd); int n = 0, bad = 0;
    if(!code || (code[0] | 32) != 'p' || (code[1] | 32) != 'l' || code[2] != '2') return 0;
    for(const char *p = code + 3; *p; p++){ char c = *p; if(c == '-' || c == ' ') continue; if(c >= 'a' && c <= 'z') c -= 32; if(c == 'O') c = '0'; if(c == 'I' || c == 'L') c = '1';
        const char *q = strchr(b32, c); if(!q || !c) return 0; int v = (int)(q - b32); if(n + 5 > 2040) return 0; for(int k = 0; k < 5; k++, n++) if((v >> (4 - k)) & 1) rd.b[n >> 3] |= 0x80 >> (n & 7); }
    rd.n = 0; CrRecipe q; memset(&q, 0, sizeof q);
    if(get(&rd, 4, &bad) != 2) return 0;
    int nl = (int)get(&rd, 5, &bad); if(nl > CR_NAME) return 0;
    for(int i = 0; i < nl; i++){ uint32_t c = get(&rd, 6, &bad); if(c >= sizeof chars - 1) return 0; q.name[i] = chars[c]; }
    q.cls = (uint8_t)get(&rd, 4, &bad); q.size = (uint8_t)get(&rd, 3, &bad); q.xsize = (uint8_t)get(&rd, 4, &bad); q.sea = (uint8_t)get(&rd, 7, &bad); q.land = (uint8_t)get(&rd, 3, &bad); q.rough = (uint8_t)get(&rd, 2, &bad); q.hour = (uint8_t)get(&rd, 2, &bad); q.weather = (uint8_t)get(&rd, 2, &bad);
    q.sky = (uint8_t)get(&rd, 5, &bad); q.ground = (uint8_t)get(&rd, 5, &bad); q.climate = (int8_t)((int)get(&rd, 5, &bad) - 8); q.salt = get(&rd, 32, &bad);
    uint32_t mask = get(&rd, 9, &bad); for(int c = 0; c < CR_NCAT; c++) q.trait[c] = (mask >> c & 1) ? (uint16_t)get(&rd, 11, &bad) : CR_NONE;
    for(int i = 0; i < 2; i++) q.theme[i] = get(&rd, 1, &bad) ? (uint8_t)get(&rd, 8, &bad) : 255;
    q.body = (uint8_t)get(&rd, 4, &bad); q.limbs = (uint8_t)get(&rd, 3, &bad); q.height = (uint8_t)get(&rd, 3, &bad); for(int i = 0; i < 3; i++) q.col[i] = (uint8_t)get(&rd, 5, &bad); for(int i = 0; i < 3; i++) q.gift[i] = (uint8_t)get(&rd, 5, &bad);
    q.stock = (uint8_t)get(&rd, 5, &bad); q.calling = (uint8_t)get(&rd, 4, &bad); q.cstyle = (uint8_t)get(&rd, 8, &bad); q.diet = (uint8_t)get(&rd, 3, &bad); q.temper = (uint8_t)get(&rd, 3, &bad); q.pop = (uint8_t)get(&rd, 4, &bad); q.tech = (uint8_t)get(&rd, 3, &bad);
    q.cover = (uint8_t)get(&rd, 7, &bad); q.div = (uint8_t)get(&rd, 4, &bad);
    q.ncre = (uint8_t)get(&rd, 4, &bad); if(q.ncre > CR_MAXCRE) return 0; for(int i = 0; i < q.ncre; i++){ q.cre[i].species = (uint16_t)get(&rd, 14, &bad); q.cre[i].amount = (uint8_t)get(&rd, 4, &bad); }
    q.npin = (uint8_t)get(&rd, 4, &bad); if(q.npin > CR_MAXPIN) return 0; for(int i = 0; i < q.npin; i++){ q.pin[i].id = get(&rd, 29, &bad); q.pin[i].amount = (uint8_t)get(&rd, 4, &bad); }
    int used = rd.n; uint32_t chk = get(&rd, 12, &bad); if(bad) return 0;
    { Bits t = rd; (void)t; uint8_t tmp[256]; memset(tmp, 0, sizeof tmp); int nb = used; for(int i = 0; i < nb; i++) if((rd.b[i >> 3] >> (7 - (i & 7))) & 1) tmp[i >> 3] |= 0x80 >> (i & 7); if(sum12(tmp, nb) != chk) return 0; }
    for(int i = rd.n; i < n; i++) if((rd.b[i >> 3] >> (7 - (i & 7))) & 1) return 0;           // padding must be zero
    if(n - rd.n >= 5) return 0;
    cr_fix(&q); *r = q; return 1;
}

// ------------------------------------------------------------------ files
int cr_save(const CrRecipe *r0, const char *path){
    CrRecipe q = *r0; cr_fix(&q); const CrRecipe *r = &q; FILE *f = fopen(path, "w"); if(!f) return 0;
    fprintf(f, "crp 1\nname %s\ncls %d\nsize %d\nxsize %d\nsea %d\nland %d\nrough %d\nhour %d\nweather %d\nsky %d\nground %d\nclimate %d\nsalt %u\n", r->name, r->cls, r->size, r->xsize, r->sea, r->land, r->rough, r->hour, r->weather, r->sky, r->ground, r->climate, r->salt);
    for(int c = 0; c < CR_NCAT; c++) if(r->trait[c] != CR_NONE) fprintf(f, "trait %d %s\n", r->trait[c], trait_name(r->trait[c]));
    for(int i = 0; i < 2; i++) if(r->theme[i] != 255) fprintf(f, "theme %s\n", theme_get(r->theme[i])->name);
    fprintf(f, "body %d\nlimbs %d\nheight %d\ncol %d %d %d\ngift %d %d %d\nstock %d\ncalling %d\ncstyle %d\ndiet %d\ntemper %d\npop %d\ntech %d\ncover %d\ndiv %d\n",
        r->body, r->limbs, r->height, r->col[0], r->col[1], r->col[2], r->gift[0], r->gift[1], r->gift[2], r->stock, r->calling, r->cstyle, r->diet, r->temper, r->pop, r->tech, r->cover, r->div);
    for(int i = 0; i < r->ncre; i++) fprintf(f, "cre %d %d\n", r->cre[i].species, r->cre[i].amount);
    for(int i = 0; i < r->npin; i++){ Flora fl; flora_make(r->pin[i].id, &fl); fprintf(f, "pin %d %s\n", r->pin[i].amount, fl.name); }
    char code[400]; if(cr_encode(r, code, sizeof code)) fprintf(f, "code %s\n", code);
    fclose(f); return 1;
}
int cr_load(CrRecipe *r, const char *path){
    FILE *f = fopen(path, "r"); if(!f) return 0; char line[200]; CrRecipe q; cr_default(&q, 0); memset(q.cre, 0, sizeof q.cre); q.ncre = 0; q.npin = 0; memset(q.pin, 0, sizeof q.pin);
    for(int c = 0; c < CR_NCAT; c++) q.trait[c] = CR_NONE; q.theme[0] = q.theme[1] = 255; int ok = 0, nt = 0;
    while(fgets(line, sizeof line, f)){
        size_t L = strlen(line); while(L && (line[L - 1] == '\n' || line[L - 1] == '\r')) line[--L] = 0;
        char *sp = strchr(line, ' '); const char *val = sp ? sp + 1 : ""; if(sp) *sp = 0; const char *k = line; int a = 0, b = 0, c = 0;
        if(!strcmp(k, "crp")) ok = atoi(val) == 1;
        else if(!strcmp(k, "name")) set_name(&q, val);
        else if(!strcmp(k, "cls")) q.cls = (uint8_t)atoi(val); else if(!strcmp(k, "size")) q.size = (uint8_t)atoi(val); else if(!strcmp(k, "xsize")) q.xsize = (uint8_t)atoi(val); else if(!strcmp(k, "stock")) q.stock = (uint8_t)atoi(val); else if(!strcmp(k, "sea")) q.sea = (uint8_t)atoi(val);
        else if(!strcmp(k, "land")) q.land = (uint8_t)atoi(val); else if(!strcmp(k, "rough")) q.rough = (uint8_t)atoi(val); else if(!strcmp(k, "hour")) q.hour = (uint8_t)atoi(val);
        else if(!strcmp(k, "weather")) q.weather = (uint8_t)atoi(val); else if(!strcmp(k, "sky")) q.sky = (uint8_t)atoi(val); else if(!strcmp(k, "ground")) q.ground = (uint8_t)atoi(val);
        else if(!strcmp(k, "climate")) q.climate = (int8_t)atoi(val); else if(!strcmp(k, "salt")) q.salt = (uint32_t)strtoul(val, 0, 10);
        else if(!strcmp(k, "trait")){ int id = atoi(val); if(id >= 0 && id < trait_count() && nt < CR_NCAT) q.trait[trait_cat(id)] = (uint16_t)id; nt++; }
        else if(!strcmp(k, "theme")){ int id = thid(val); for(int i = 0; i < 2; i++) if(q.theme[i] == 255 && id != 255){ q.theme[i] = (uint8_t)id; break; } }
        else if(!strcmp(k, "body")) q.body = (uint8_t)atoi(val); else if(!strcmp(k, "limbs")) q.limbs = (uint8_t)atoi(val); else if(!strcmp(k, "height")) q.height = (uint8_t)atoi(val);
        else if(!strcmp(k, "col") && sscanf(val, "%d %d %d", &a, &b, &c) == 3){ q.col[0] = (uint8_t)a; q.col[1] = (uint8_t)b; q.col[2] = (uint8_t)c; }
        else if(!strcmp(k, "gift") && sscanf(val, "%d %d %d", &a, &b, &c) == 3){ q.gift[0] = (uint8_t)a; q.gift[1] = (uint8_t)b; q.gift[2] = (uint8_t)c; }
        else if(!strcmp(k, "calling")) q.calling = (uint8_t)atoi(val); else if(!strcmp(k, "cstyle")) q.cstyle = (uint8_t)atoi(val); else if(!strcmp(k, "diet")) q.diet = (uint8_t)atoi(val);
        else if(!strcmp(k, "temper")) q.temper = (uint8_t)atoi(val); else if(!strcmp(k, "pop")) q.pop = (uint8_t)atoi(val); else if(!strcmp(k, "tech")) q.tech = (uint8_t)atoi(val);
        else if(!strcmp(k, "cover")) q.cover = (uint8_t)atoi(val); else if(!strcmp(k, "div")) q.div = (uint8_t)atoi(val);
        else if(!strcmp(k, "cre") && sscanf(val, "%d %d", &a, &b) == 2 && q.ncre < CR_MAXCRE){ q.cre[q.ncre].species = (uint16_t)a; q.cre[q.ncre].amount = (uint8_t)b; q.ncre++; }
        else if(!strcmp(k, "pin") && q.npin < CR_MAXPIN){ char nm[48]; if(sscanf(val, "%d %47[^\n]", &a, nm) == 2){ uint32_t id = flora_find(nm); if(id != 0xFFFFFFFFu){ q.pin[q.npin].id = id; q.pin[q.npin].amount = (uint8_t)a; q.npin++; } } }
    }
    fclose(f); if(!ok) return 0; cr_fix(&q); *r = q; return 1;
}

// ------------------------------------------------------------------ undo and redo
void cr_hist_init(CrHist *h, const CrRecipe *r){ h->n = 1; h->at = 0; h->snap[0] = *r; }
void cr_hist_push(CrHist *h, const CrRecipe *r){
    if(cr_equal(&h->snap[h->at], r)) return;
    h->n = h->at + 1;                                                         // a new edit drops the redo list
    if(h->n == CR_HIST){ memmove(&h->snap[0], &h->snap[1], sizeof(CrRecipe) * (CR_HIST - 1)); h->n--; }
    h->at = h->n; h->snap[h->n++] = *r;
}
int cr_hist_undo(CrHist *h, CrRecipe *r){ if(h->at <= 0) return 0; h->at--; *r = h->snap[h->at]; return 1; }
int cr_hist_redo(CrHist *h, CrRecipe *r){ if(h->at + 1 >= h->n) return 0; h->at++; *r = h->snap[h->at]; return 1; }
