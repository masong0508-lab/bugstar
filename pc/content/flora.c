// flora.c: the FLORA catalogue (see flora.h). Pure functions: same number, same plant, always. No table of plants exists; only the word lists below.
#include <string.h>
#include <stdio.h>
#include "flora.h"

static uint64_t mix64(uint64_t z){ z += 0x9E3779B97F4A7C15ull; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
#define RGB(r,g,b) ((uint16_t)(((r) >> 3) | (((g) >> 3) << 5) | (((b) >> 3) << 10)))
#define H(x) (1u << FH_##x)
#define ALLH 0x3FFFu

// ---- growth forms: name, smallest size, biggest size, habitats it can live in, woody, aquatic, weight of its stuff (4 light .. 14 dense)
typedef struct { const char *name; uint8_t smin, smax, woody, aquatic, dens; uint16_t hab; } Form;
static const Form forms[FL_NFORM] = {
 {"MOSS",0,0,0,0,4,ALLH},{"LICHEN",0,0,0,0,4,ALLH},
 {"GRASS",0,2,0,0,5,H(MEADOW)|H(DUNE)|H(SLOPE)|H(CANYON)|H(VOLCANIC)|H(MARSH)|H(SHORE)},
 {"REED",1,3,0,0,5,H(MARSH)|H(SHORE)|H(TIDAL)|H(RAIN)},
 {"FERN",1,3,0,0,6,H(FOREST)|H(RAIN)|H(CAVE)|H(MARSH)|H(SLOPE)},
 {"VINE",1,3,0,0,6,H(FOREST)|H(RAIN)|H(CANYON)|H(CAVE)|H(ROCK)},
 {"CREEPER",0,2,0,0,5,H(FOREST)|H(RAIN)|H(ROCK)|H(SLOPE)|H(CAVE)|H(ASH)},
 {"SHRUB",1,3,1,0,9,H(MEADOW)|H(CANYON)|H(SLOPE)|H(DUNE)|H(ASH)|H(VOLCANIC)|H(FOREST)},
 {"BUSH",2,3,1,0,9,H(MEADOW)|H(FOREST)|H(SLOPE)|H(SHORE)|H(CANYON)},
 {"BRAMBLE",1,3,1,0,8,H(MEADOW)|H(FOREST)|H(SLOPE)|H(ASH)},
 {"TREE",3,5,1,0,12,H(FOREST)|H(MEADOW)|H(RAIN)|H(SLOPE)|H(MARSH)|H(VOLCANIC)},
 {"PALM",3,4,1,0,10,H(SHORE)|H(DUNE)|H(RAIN)|H(TIDAL)},
 {"CACTUS",1,4,0,0,11,H(DUNE)|H(CANYON)|H(ROCK)|H(ASH)},
 {"SUCCULENT",0,2,0,0,9,H(DUNE)|H(CANYON)|H(ROCK)|H(SLOPE)},
 {"TOADSTOOL",0,3,0,0,5,H(FOREST)|H(CAVE)|H(RAIN)|H(MARSH)|H(ASH)},
 {"KELP",2,5,0,1,5,H(SHORE)|H(TIDAL)},
 {"LILY",0,1,0,1,5,H(MARSH)|H(SHORE)|H(RAIN)},
 {"ORCHID",0,1,0,0,5,H(RAIN)|H(FOREST)|H(CAVE)},
 {"HERB",0,1,0,0,5,H(MEADOW)|H(FOREST)|H(SLOPE)|H(SHORE)|H(CANYON)},
 {"BULB",0,1,0,0,6,H(MEADOW)|H(SLOPE)|H(ICE)|H(ROCK)},
 {"MAT",0,1,0,0,5,H(ICE)|H(ROCK)|H(ASH)|H(DUNE)|H(SLOPE)|H(VOLCANIC)},
 {"TUFT",0,1,0,0,5,H(MEADOW)|H(SLOPE)|H(ICE)|H(DUNE)|H(ROCK)},
 {"THORNBUSH",1,3,1,0,9,H(DUNE)|H(CANYON)|H(ASH)|H(MEADOW)},
 {"CANOPY",4,5,1,0,13,H(FOREST)|H(RAIN)},
 {"SPIRE",3,5,1,0,13,H(ROCK)|H(CANYON)|H(VOLCANIC)|H(ICE)|H(SLOPE)},
 {"STILT-TREE",3,4,1,0,11,H(MARSH)|H(TIDAL)|H(SHORE)|H(RAIN)},
 {"FLOATER",1,3,0,0,4,H(MEADOW)|H(SLOPE)|H(CANYON)|H(RAIN)},
 {"GLASSWORT",0,1,0,0,6,H(SHORE)|H(TIDAL)|H(DUNE)|H(ICE)},
 {"SEAGRASS",0,1,0,1,5,H(SHORE)|H(TIDAL)},
 {"CLIMBER",2,4,1,0,8,H(FOREST)|H(RAIN)|H(CANYON)},
 {"BAMBOO",2,4,1,0,9,H(FOREST)|H(RAIN)|H(MARSH)|H(SLOPE)},
 {"WILLOW",3,4,1,0,10,H(MARSH)|H(SHORE)|H(MEADOW)},
};
// ---- leaves: name, how well they make air (0..15)
static const char *const leaf_nm[FL_NLEAF] = {"NEEDLE","BROAD","RIBBON","FAN","HEART","LOBED","SCALE","GLASSY","FUZZY","SPIRAL","PAPER","SPINE","FROND","CUP","COIN","LACE"};
static const uint8_t leaf_o2[FL_NLEAF] = {6,12,8,12,11,10,5,7,8,8,10,3,11,7,9,9};
static const char *const flower_nm[FL_NFLOWER] = {"BELL","STAR","CUP","SPIKE","PLUME","CROSS","WHEEL","SPORE","TASSEL","ORB","TRUMPET","LANTERN","RIBBON","DOUBLE","MOON","FEATHER"};
static const char *const fruit_nm[FL_NFRUIT] = {"BERRIES","NUTS","PODS","FLUFFY SEEDS","POMES","GOURDS","STONE FRUITS","CONES","BEANS","GRAIN","FIGS","HUSKS","BULBILS","GLOW-FRUIT","MELONS","NO FRUIT"};
static const uint8_t fruit_ed[FL_NFRUIT] = {10,9,6,1,11,8,10,3,8,12,10,4,6,5,11,0};
static const char *const bark_nm[FL_NBARK] = {"SMOOTH","RIDGED","SCALY","PAPERY","CORKY","SILVERY","MOSSY","THORNED","GLOSSY","CRACKED","BANDED","WOOLLY"};
static const uint8_t bark_hard[FL_NBARK] = {6,8,9,3,5,7,4,6,8,10,7,3};
static const char *const quirk_nm[FL_NQUIRK] = {"GLOWS AT NIGHT","HUMS IN THE WIND","CLOSES WHEN TOUCHED","SMELLS OF RAIN","TURNS TO FOLLOW THE SUN","STORES WATER","DRAWS BUGS","WARMS THE AIR AROUND IT",
 "CHANGES COLOUR BY DAY","WILL NOT BURN","SPARKLES IN FROST","HEALS ITSELF","IS QUIETLY POISONOUS","CLEANS THE AIR","SINGS AT DAWN","DROPS ALL ITS LEAVES AT ONCE"};
enum { Q_GLOW, Q_HUM, Q_CLOSE, Q_RAIN, Q_SUN, Q_WATER, Q_BUGS, Q_WARM, Q_COLOUR, Q_NOBURN, Q_FROST, Q_HEAL, Q_POISON, Q_CLEAN, Q_SING, Q_DROP };
static const char *const use_nm[FL_NUSE] = {"FOOD","MEDICINE","FUEL","TIMBER","FIBRE","DYE","SPICE","CLEAN AIR","SOIL BUILDER","ORNAMENT"};
static const char *const use_txt[FL_NUSE] = {"EATEN BY PEOPLE AND BEASTS","BREWED INTO MEDICINE","BURNED FOR HEAT","CUT FOR BUILDING","SPUN INTO CLOTH AND ROPE","BOILED FOR DYE","GROUND AS A SPICE","KEPT TO CLEAN THE AIR","SOWN TO RESTORE TIRED SOIL","GROWN FOR LOOKS ALONE"};
static const char *const hab_nm[FL_NHAB] = {"MEADOW","FOREST FLOOR","MARSH","SHORE","DUNES","BARE ROCK","ICE FIELD","CANYON","ASH FIELD","CAVE MOUTH","HIGH SLOPE","TIDAL FLAT","RAINFOREST","VOLCANIC SOIL"};
static const char *const size_nm[FL_NSIZE] = {"MICRO","SMALL","MEDIUM","LARGE","HUGE","TITAN"};
static const uint16_t size_cm[FL_NSIZE] = {3,25,150,800,2500,7000};
static const uint16_t size_mass[FL_NSIZE] = {2,8,30,120,300,700};         // tenths of kg per square metre of cover
// ---- colours: 16 colours, each with 4 words (so 64 colour words)
static const char *const cword[FL_NCOLOUR][4] = {
 {"RUBY","SCARLET","CRIMSON","POPPY"},{"AMBER","TANGERINE","RUST","MARIGOLD"},{"GOLDEN","LEMON","SAFFRON","STRAW"},{"LIME","CHARTREUSE","SPRING","FRESH"},
 {"EMERALD","FOREST","JADE","VERDANT"},{"TEAL","SEAFOAM","LAGOON","PEACOCK"},{"AZURE","GLACIER","ICE-BLUE","SKYLIT"},{"COBALT","SAPPHIRE","INDIGO","MIDNIGHT"},
 {"VIOLET","AMETHYST","LILAC","PLUM"},{"FUCHSIA","ROSE","BLUSH","MAGENTA"},{"PINK","PEACH","CORAL","SALMON"},{"UMBER","CHESTNUT","COCOA","TAWNY"},
 {"PEARL","IVORY","SNOWY","MILKY"},{"SILVER","ASHEN","SLATE","MISTY"},{"INKY","RAVEN","SOOTY","ONYX"},{"OPAL","PRISM","SHIMMER","MOONLIT"} };
static const uint16_t crgb[FL_NCOLOUR] = { RGB(200,40,50),RGB(230,120,30),RGB(235,200,50),RGB(150,220,50),RGB(40,160,70),RGB(30,150,140),RGB(70,200,225),RGB(50,80,200),
 RGB(130,70,190),RGB(200,50,160),RGB(240,150,180),RGB(130,85,50),RGB(235,235,225),RGB(140,145,150),RGB(35,30,45),RGB(190,170,235) };
static const char syl_c[13] = {'B','D','F','G','K','L','M','N','P','R','S','T','V'};
static const char syl_v[5] = {'A','E','I','O','U'};       // 13 x 5 = 65 syllables; the 64 used are all but the last (VU)

// ---- habitats: coldest and hottest C, driest and wettest %, and the sea % it needs
typedef struct { int16_t t0, t1; uint8_t w0, w1, sea; } HabRule;
static const HabRule hrule[FL_NHAB] = {
 {5,32,30,75,0},{0,30,45,95,0},{5,38,70,100,0},{0,42,55,100,12},{15,70,0,30,0},{-60,90,0,100,0},{-90,4,0,100,0},{5,55,5,50,0},
 {10,100,0,65,0},{-30,45,25,100,0},{-35,22,20,85,0},{0,42,60,100,30},{22,48,75,100,0},{10,120,10,85,0} };

static int hash_pick(uint64_t h, int n){ return (int)((h >> 20) % (uint64_t)n); }
static uint16_t shade(uint16_t c, int pct){ int r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31; r = r * pct / 100; g = g * pct / 100; b = b * pct / 100; if(r > 31) r = 31; if(g > 31) g = 31; if(b > 31) b = 31; return (uint16_t)(r | (g << 5) | (b << 10)); }
static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }

const char *flora_form_name(int i){ return forms[i & 31].name; }
const char *flora_use_name(int i){ return use_nm[i % FL_NUSE]; }
const char *flora_habitat_name(int i){ return hab_nm[i % FL_NHAB]; }
const char *flora_size_name(int i){ return size_nm[i % FL_NSIZE]; }
const char *flora_colour_name(int i){ return cword[i & 15][0]; }
uint16_t flora_colour_rgb(int i){ return crgb[i & 15]; }

void flora_make(uint32_t id, Flora *f){
    memset(f, 0, sizeof *f);
    id &= FL_TOTAL - 1; f->id = id;
    int adj = id & 63, form = (id >> 6) & 31, giv = id >> 11;
    f->colour = (uint8_t)(adj >> 2); f->word = (uint8_t)(adj & 3); f->form = (uint8_t)form;
    const Form *F = &forms[form];
    uint64_t h = mix64((uint64_t)id * 0x9E3779B97F4A7C15ull + 0x464C4F5241ull);
    uint64_t h2 = mix64(h ^ 0x1111), h3 = mix64(h ^ 0x2222), h4 = mix64(h ^ 0x3333);
    f->leaf = (uint8_t)hash_pick(h, FL_NLEAF); f->flower = (uint8_t)hash_pick(h2, FL_NFLOWER);
    f->fruit = (uint8_t)hash_pick(h3, FL_NFRUIT); f->bark = (uint8_t)hash_pick(h4, FL_NBARK);
    if(form <= 1 || form == 14) { f->flower = 7; if(form != 14) f->fruit = 15; }              // moss, lichen and toadstools have spores, not flowers
    if(!F->woody && form != 14 && (f->bark == 7 || f->bark == 9) && (h4 & 1)) f->bark = 0;       // soft plants rarely have thorny, cracked bark
    // size inside the form's range
    int span = F->smax - F->smin + 1; f->size = (uint8_t)(F->smin + hash_pick(h >> 7, span));
    // habitat among the ones this form can live in
    { int cnt = 0, k; for(k = 0; k < FL_NHAB; k++) if(F->hab >> k & 1) cnt++;
      int pick = hash_pick(h2 >> 9, cnt); for(k = 0; k < FL_NHAB; k++) if(F->hab >> k & 1){ if(!pick--) break; } f->habitat = (uint8_t)k; }
    f->quirk = (uint8_t)hash_pick(h3 >> 5, FL_NQUIRK);
    f->use = (uint8_t)hash_pick(h4 >> 5, FL_NUSE);
    f->woody = F->woody; f->aquatic = F->aquatic;
    f->height_cm = (uint16_t)(size_cm[f->size] * (60 + hash_pick(h >> 13, 81)) / 100);
    f->biomass = (uint16_t)(size_mass[f->size] * F->dens * (70 + hash_pick(h2 >> 13, 61)) / 800 + 1);
    // colours
    f->leaf_rgb = crgb[f->colour];
    { int fc = (f->colour + 1 + hash_pick(h3 >> 11, 14)) & 15; f->flower_rgb = crgb[fc]; if(f->flower == 7) f->flower_rgb = shade(f->leaf_rgb, 70); }
    // what it gives (0..15)
    int o2 = leaf_o2[f->leaf] + (f->aquatic ? 2 : 0) + (f->use == FU_AIR ? 3 : 0) + (f->quirk == Q_CLEAN ? 3 : 0) + (form == 14 ? -6 : 0) + (f->size >= 3 ? 1 : 0);
    int food = fruit_ed[f->fruit] / 2 + (f->use == FU_FOOD ? 6 : 0) + (f->use == FU_SPICE ? 2 : 0) + ((f->leaf == 1 || f->leaf == 4 || f->leaf == 14) ? 2 : 0) + (form == 14 ? 3 : 0) - (f->quirk == Q_POISON ? 7 : 0);
    int shelter = F->woody ? f->size * 2 + bark_hard[f->bark] / 3 : (f->size >= 2 ? f->size : 0);
    shelter += f->use == FU_TIMBER ? 4 : f->use == FU_FIBRE ? 2 : 0;
    int med = (f->use == FU_MEDICINE ? 9 : 0) + (f->quirk == Q_HEAL ? 3 : 0) + (f->quirk == Q_POISON ? 2 : 0) + hash_pick(h >> 25, 4);
    int fuel = (F->woody ? 2 + bark_hard[f->bark] / 2 + f->size / 2 : f->size / 2) + (f->use == FU_FUEL ? 5 : 0) - (f->quirk == Q_NOBURN ? 20 : 0) - (f->aquatic ? 4 : 0) - (f->quirk == Q_WATER ? 3 : 0);
    f->o2 = (uint8_t)clampi(o2, 0, 15); f->food = (uint8_t)clampi(food, 0, 15); f->shelter = (uint8_t)clampi(shelter, 0, 15); f->medicine = (uint8_t)clampi(med, 0, 15); f->fuel = (uint8_t)clampi(fuel, 0, 15);
    // the name: COLOUR-WORD GIVEN-NAME FORM. The given name is three 2-letter syllables (6 bits each), so names never collide.
    char g[7]; for(int k = 0; k < 3; k++){ int s = (giv >> (k * 6)) & 63; g[k * 2] = syl_c[s / 5]; g[k * 2 + 1] = syl_v[s % 5]; } g[6] = 0;
    snprintf(f->name, sizeof f->name, "%s %s %s", cword[adj >> 2][adj & 3], g, F->name);
}

void flora_about(const Flora *f, char *out, int cap){
    char sz[64], hb[48]; const Form *F = &forms[f->form & 31];
    snprintf(sz, sizeof sz, "%s", size_nm[f->size]);
    snprintf(hb, sizeof hb, "%s", hab_nm[f->habitat]);
    int cm = f->height_cm; char ht[24];
    if(cm >= 100) snprintf(ht, sizeof ht, "%d.%d M", cm / 100, (cm % 100) / 10); else snprintf(ht, sizeof ht, "%d CM", cm);
    char fl[48]; if(f->flower == 7) snprintf(fl, sizeof fl, "SPORES INSTEAD OF FLOWERS"); else snprintf(fl, sizeof fl, "%s FLOWERS", flower_nm[f->flower]);
    char bk[24]; bk[0] = 0; if(f->woody) snprintf(bk, sizeof bk, ", %s BARK", bark_nm[f->bark]);
    snprintf(out, cap, "A %s %s OF THE %s, ABOUT %s TALL. %s LEAVES, %s, %s%s. %s. %s.",
        sz, F->name, hb, ht, leaf_nm[f->leaf], fl, fruit_nm[f->fruit], bk, use_txt[f->use], quirk_nm[f->quirk]);
}

// ---- the n-th plant of a catalogue: a bijection of the 29-bit ids, keyed by the seed (add, multiply by an odd number, xor-shift: all reversible)
uint32_t flora_catalogue(uint64_t seed, uint32_t n){
    const uint32_t M = FL_TOTAL - 1;
    uint64_t a = mix64(seed ^ 0xCA7A10600Dull), b = mix64(a), c = mix64(b);
    uint32_t k1 = (uint32_t)a | 1, k2 = (uint32_t)(a >> 32) | 1, k3 = (uint32_t)b | 1, c1 = (uint32_t)(b >> 32), c2 = (uint32_t)c, c3 = (uint32_t)(c >> 32);
    uint32_t x = n & M;
    x = (x * k1 + c1) & M; x ^= x >> 14; x = (x * k2) & M; x ^= x >> 11; x = (x + c2) & M; x = (x * k3 + c3) & M; x ^= x >> 15; x = (x * ((k1 ^ k3) | 1)) & M; x ^= x >> 13;
    return x & M;
}
uint32_t flora_find(const char *name){
    char b[48], *t[4]; int n = 0; snprintf(b, sizeof b, "%s", name);
    for(char *p = strtok(b, " "); p && n < 4; p = strtok(0, " ")) t[n++] = p;
    if(n != 3 || strlen(t[1]) != 6) return 0xFFFFFFFFu;
    int adj = -1, form = -1, giv = 0;
    for(int c = 0; c < FL_NCOLOUR; c++) for(int w = 0; w < 4; w++) if(!strcmp(cword[c][w], t[0])) adj = c * 4 + w;
    for(int i = 0; i < FL_NFORM; i++) if(!strcmp(forms[i].name, t[2])) form = i;
    if(adj < 0 || form < 0) return 0xFFFFFFFFu;
    for(int k = 0; k < 3; k++){ int ci = -1, vi = -1; for(int i = 0; i < 13; i++) if(syl_c[i] == t[1][k * 2]) ci = i; for(int i = 0; i < 5; i++) if(syl_v[i] == t[1][k * 2 + 1]) vi = i;
        if(ci < 0 || vi < 0 || ci * 5 + vi > 63) return 0xFFFFFFFFu;
        giv |= (ci * 5 + vi) << (k * 6); }
    return (uint32_t)adj | (uint32_t)form << 6 | (uint32_t)giv << 11;
}

uint32_t flora_habitats(const FloraEnv *e){
    uint32_t m = 0;
    for(int k = 0; k < FL_NHAB; k++){ const HabRule *r = &hrule[k];
        if(e->temp_c >= r->t0 && e->temp_c <= r->t1 && e->wet >= r->w0 && e->wet <= r->w1 && e->sea >= r->sea) m |= 1u << k; }
    if(e->rough >= 2 && e->temp_c < 25) m |= 1u << FH_SLOPE;
    if(e->temp_c < 4) m |= 1u << FH_ICE;
    m |= 1u << FH_ROCK;
    return m;
}
int flora_for_planet(uint64_t seed, const FloraEnv *e, int want, Flora *out){
    uint32_t mask = flora_habitats(e); int got = 0; uint32_t n = 0;
    uint32_t tries = (uint32_t)want * 400u + 4000u;
    while(got < want && n < tries){
        Flora f; flora_make(flora_catalogue(seed, n++), &f);
        if(!(mask >> f.habitat & 1)) continue;
        if(e->light < 25 && !(f.form == 0 || f.form == 1 || f.form == 14 || f.quirk == Q_GLOW)) continue;     // in the dark only fungus, moss and glowing plants grow
        if(e->light < 60 && f.size >= 4 && (n & 1)) continue;                                                // dim light: fewer giants
        if(e->temp_c > 60 && f.size >= 3) continue;                                                           // scorching: nothing tall
        out[got++] = f;
    }
    return got;
}
