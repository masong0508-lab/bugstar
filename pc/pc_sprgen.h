// pc_sprgen.h: PROCEDURAL SPRITES for the PC port (NATIVE / ULTRA views and the SPRITE GALLERY in the PC LAB).
// Nothing is drawn by hand and nothing is stored: every sprite is a pure function of an id (and a few small numbers), made from shape grammars
// (L-system trees, a modular building kit, mirrored hull grammars, body-plan recipes), colour ramps, symmetry and noise. The same id always gives the
// same picture (the generators use only integer hashing, float maths and no global state), two different ids differ in shape, colour or both.
// All generators draw into an SgImg (0xAARRGGBB, straight alpha) of ANY size: the geometry is worked out on a 64 x 64 design grid and scaled, so the same
// id at 32 px or 128 px is the same picture. No platform code: plain C99 and libm.
//   ids: flora = the 29-bit plant id of pc/content/flora.h;  city = character*16 + form (256);  people = way*32 + stock (512);
//        creature kind = variant*66 + plan (12,672), an individual adds gift (16), colouring (16), rank (8), mutation (16).
#ifndef PC_SPRGEN_H
#define PC_SPRGEN_H
#include <stdint.h>

typedef struct { int w, h; uint32_t *px; } SgImg;           // px[y*w+x] = 0xAARRGGBB (alpha 0 = nothing)
SgImg *sg_img_new(int w, int h);                            // cleared (all zero)
void   sg_img_free(SgImg *im);
void   sg_img_clear(SgImg *im);
uint64_t sg_img_hash(const SgImg *im);                      // FNV-1a over the pixels (the tests compare these)
int    sg_img_diff(const SgImg *a, const SgImg *b);         // how different two same-size pictures are, 0..1000 (mean absolute colour and alpha difference, per mille)
int    sg_img_cover(const SgImg *im);                       // how many pixels are drawn (alpha >= 128)

// ---- plants and trees (pc/content/flora.h ids)
void sg_flora(SgImg *im, uint32_t flora_id, uint32_t var);   // the plant with this id; var 0 = the plant itself, other numbers = another individual of the same kind
int  sg_flora_woody(uint32_t id);                            // 1 when this plant is a tree or a bush (the game puts these where it has trees)
int  sg_flora_tree(uint32_t id);                             // a plant id of a tree-like form for the number id (the gallery's TREES list)
void sg_flora_label(uint32_t flora_id, char *out, int n);    // its name

// ---- buildings and cities
typedef struct { uint8_t form, chr, people_stock, people_way, climate, tech, floors, hour, flags; uint32_t seed; } SgBStyle;     // flags bit 0: drawn as one of many in a skyline (no ground scene)
enum { SGC_TEMPERATE, SGC_ARID, SGC_COLD, SGC_WET, SGC_VOLCANIC, SGC_SHORE, SGC_N };
void sg_bstyle(SgBStyle *st, int city_id, int people_id, int climate, int tech, uint32_t seed);    // city_id 0..255, people_id 0..511, tech 0..5
void sg_building(SgImg *im, const SgBStyle *st);              // one building (or one hut, tent, tower) of this city's architecture, 64 x 64 design
void sg_skyline(SgImg *im, const SgBStyle *st, const char *name);   // the whole town seen from afar, sky and ground, with its name on a sign (name may be NULL)
void sg_sign(SgImg *im, const SgBStyle *st, const char *text);      // the town's gate sign alone
void sg_city_label(int city_id, int people_id, char *out, int n);
void sg_town_name(uint32_t seed, char *out, int n);             // a made-up town name (NORTHFORD, ASHBURY ...)

// ---- people
void sg_person(SgImg *im, int people_id, uint32_t seed);      // an inhabitant: body shape by stock (32), clothes and tools by way of life (16), colours by seed
int  sg_people_stock_features(int stock);                     // (tests) a number that is different for every stock

// ---- creatures
typedef struct { uint16_t kind; uint8_t gift, colour, rank, mut, scene; uint32_t seed; } SgCrit;   // gift >= 16: none; scene 1: with the ground of its habitat
void sg_creature(SgImg *im, const SgCrit *c);                 // body plan (66) x size/diet/habitat/behaviour (192) x individual (gift, colouring, rank, mutation)
void sg_creature_kind(SgImg *im, int kind, uint32_t seed);    // a plain adult of the kind (colouring from the seed)

// ---- ships (by faction), items and terrain props
void sg_ship(SgImg *im, uint32_t faction, int hull, uint32_t seed);   // hull 0 fighter, 1 freighter, 2 cruiser, 3 scout, 4 carrier, 5 wreck-hauler; seen from above, nose up
void sg_item(SgImg *im, int kind, int material, uint32_t seed);
enum { SGP_ROCK, SGP_BOULDER, SGP_CRYSTAL, SGP_CORAL, SGP_FUNGUS, SGP_RUIN, SGP_COLUMN, SGP_OBELISK, SGP_FOSSIL, SGP_STUMP, SGP_GEYSER, SGP_ICE, SGP_SHRINE, SGP_BEACON, SGP_PEBBLES, SGP_N };
void sg_prop(SgImg *im, int kind, uint32_t seed, int style);   // style = the land (0..7: grass, sand, ice, rock, ash, swamp, jungle, crystal)
extern const char *const sg_prop_name[SGP_N];
#define SG_NITEM 20
#define SG_NMAT 12
extern const char *const sg_item_name[SG_NITEM];
extern const char *const sg_mat_name[SG_NMAT];

// ---- the sky: planets, stars, nebulae, galaxies, weather
typedef struct {
    uint8_t cls;                  // 0 earthlike, 1 desert, 2 ice, 3 lava, 4 gas giant, 5 ocean, 6 barren rock, 7 toxic, 8 city world, 9 jungle, 10 ice giant, 11 rusted
    uint8_t ring, moons, bands, clouds, ice, lights, atmo;    // ring 0 none, 1..3 kinds; moons 0..4; clouds/ice/lights/atmo 0..255; bands = band count
    uint32_t c[4];                // palette: sea or low, land or mid, high or peak, cloud/atmosphere
    float ringtilt, ringin, ringout; uint32_t ringc[2];
    uint32_t seed;
} SgPlanetSpec;
#define SG_NCLS 12
extern const char *const sg_planet_class_name[SG_NCLS];
void sg_planet_spec(SgPlanetSpec *p, uint64_t seed, int cls);          // cls < 0: the class comes from the seed too
void sg_planet(SgImg *im, const SgPlanetSpec *p, float rot, float lx, float ly, float lz);   // a lit disc (and its ring) in the image; rot: spin 0..1 turns; (lx,ly,lz) the sun direction (y up, z toward the viewer)
float sg_planet_frac(const SgPlanetSpec *p);                           // the disc's radius as a fraction of the picture's width (rings and moons need room around it)
void sg_planet_tex(uint32_t *tex, int tw, int th, const SgPlanetSpec *p);    // the surface as a tw x th longitude-latitude colour map (0x00RRGGBB)
void sg_star(SgImg *im, int spectral, uint32_t seed);                  // spectral 0..6 = O B A F G K M: glow, rays and a hot core
void sg_nebula(SgImg *im, uint32_t seed);                              // a cloud of gas and dust (alpha = density)
#define SG_NGAL 8
extern const char *const sg_galaxy_name[SG_NGAL];
void sg_galaxy(SgImg *im, int type, uint32_t seed);                    // 0 spiral, 1 barred spiral, 2 elliptical, 3 lenticular, 4 ring, 5 irregular, 6 edge-on, 7 starburst
#define SG_NWEATHER 8
extern const char *const sg_weather_name[SG_NWEATHER];
void sg_weather(SgImg *im, int kind, uint32_t seed);                   // 0 cloud, 1 snow crystal, 2 rain sheet, 3 dust, 4 embers, 5 ash, 6 lightning, 7 aurora

// ---- the gallery: every generator as a numbered list (used by the PC LAB screen and the tests)
typedef struct { const char *name; int w, h; uint32_t nids; void (*draw)(SgImg *im, uint32_t id); void (*label)(uint32_t id, char *out, int n); } SgGen;
extern const SgGen sg_gens[];
extern const int sg_ngens;

// ---- drawing a sprite into a window picture (0x00RRGGBB), scaled, with a small cache
// flags: 1 flip x, 2 dim (fog 0..255 in .fog mixes toward .fogc), 4 additive glow
typedef struct { float x, y, w, h; int flags; int fog; uint32_t fogc; int clip_y0, clip_y1; } SgBlit;
void sg_blit(uint32_t *dst, int dw, int dh, const SgImg *src, const SgBlit *b);   // (x,y) the top-left corner in dst pixels; bilinear (box when shrinking)
// A cache of finished sprites. sg_cached makes (or finds) the picture for (gen, id, size) with a least-recently-used list of about 192 pictures.
const SgImg *sg_cached(int gen, uint32_t id, int size);
const SgImg *sg_cached_ex(uint32_t key, int w, int h, void (*make)(SgImg *, void *), void *arg);   // the same for a caller's own generator (key must be unique for what make draws)
void sg_cache_stats(int *hits, int *misses, int *live);
void sg_cache_clear(void);
#endif
