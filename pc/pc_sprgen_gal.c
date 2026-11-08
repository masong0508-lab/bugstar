// pc_sprgen_gal.c: the registry of generators (the SPRITE GALLERY and the tests walk it) and the little maps from a plain number to each generator's own parameters.
#include "pc_sprgen_i.h"
#include "content/pc_content.h"
#include "content/flora.h"
#include <stdio.h>

void sg_town_name(uint32_t seed, char *out, int n){
    static const char *const a[24] = { "NORTH", "ASH", "BRIAR", "CLOVER", "DUN", "ELM", "FALCON", "GREY", "HOLLOW", "IRON", "JUNIPER", "KEL", "LARK", "MILL", "NEW", "OAK", "PINE", "QUILL", "RAVEN", "STONE", "TOR", "UP", "WIND", "YEW" };
    static const char *const b[16] = { "FORD", "HAVEN", "BURY", "WICK", "MERE", "DALE", "STEAD", "PORT", "HOLM", "CROFT", "GATE", "BRIDGE", "FELL", "WOOD", "MOUTH", "TON" };
    SgRng r = sg_rng(seed, 0x70D1); snprintf(out, (size_t)n, "%s%s", a[sg_ri(&r, 24)], b[sg_ri(&r, 16)]);
}
static void bstyle_of(SgBStyle *st, uint32_t id){ uint64_t h = sg_m64(id + 0x5151); int city = (int)(id & 255), people = (int)((h >> 8) & 511); sg_bstyle(st, city, people, (int)((h >> 20) % 6), (int)((h >> 28) % 6), (uint32_t)(h >> 36)); }
static void g_tree(SgImg *im, uint32_t id){ sg_flora(im, (uint32_t)sg_flora_tree(id), 0); }
static void g_plant(SgImg *im, uint32_t id){ sg_flora(im, (uint32_t)(sg_m64(id * 77 + 1) >> 35), 0); }
static void g_bld(SgImg *im, uint32_t id){ SgBStyle st; bstyle_of(&st, id); sg_building(im, &st); }
static void g_city(SgImg *im, uint32_t id){ SgBStyle st; bstyle_of(&st, id); char nm[32]; sg_town_name(id * 31 + 7, nm, 32); sg_skyline(im, &st, nm); }
static void g_ppl(SgImg *im, uint32_t id){ sg_person(im, (int)(id % 512), id / 512 * 7 + 1); }
static void g_crit(SgImg *im, uint32_t id){ sg_creature_kind(im, (int)(id % 12672), id); }
static void g_ship(SgImg *im, uint32_t id){ sg_ship(im, id / 6, (int)(id % 6), id * 13 + 1); }
static void g_item(SgImg *im, uint32_t id){ sg_item(im, (int)(id % 20), (int)((id / 20) % 12), id / 240); }
static void g_prop(SgImg *im, uint32_t id){ sg_prop(im, (int)(id % SGP_N), id, (int)((id / SGP_N) % 8)); }
static void g_planet(SgImg *im, uint32_t id){ SgPlanetSpec p; sg_planet_spec(&p, (uint64_t)id * 7919u + 5, id < 12 ? (int)id : -1); sg_planet(im, &p, 0.1f, -0.65f, 0.4f, 0.65f); }
static void g_star(SgImg *im, uint32_t id){ sg_star(im, (int)(id % 7), id); }
static void g_neb(SgImg *im, uint32_t id){ sg_nebula(im, id); }
static void g_gal(SgImg *im, uint32_t id){ sg_galaxy(im, (int)(id % 8), id / 8 * 13 + id); }
static void g_wea(SgImg *im, uint32_t id){ sg_weather(im, (int)(id % 8), id / 8); }
static void l_tree(uint32_t id, char *o, int n){ sg_flora_label((uint32_t)sg_flora_tree(id), o, n); }
static void l_plant(uint32_t id, char *o, int n){ sg_flora_label((uint32_t)(sg_m64(id * 77 + 1) >> 35), o, n); }
static void l_bld(uint32_t id, char *o, int n){ SgBStyle st; bstyle_of(&st, id); sg_city_label((int)(id & 255), st.people_stock + st.people_way * 32, o, n); }
static void l_city(uint32_t id, char *o, int n){ sg_town_name(id * 31 + 7, o, n); }
static void l_ppl(uint32_t id, char *o, int n){ People p; people_get((int)(id % 512), &p); snprintf(o, (size_t)n, "%s", p.name); }
static void l_crit(uint32_t id, char *o, int n){ Creature c; creature_kind((int)(id % 12672), &c); snprintf(o, (size_t)n, "%s", c.name); }
static void l_ship(uint32_t id, char *o, int n){ static const char *const hn[6] = { "FIGHTER", "FREIGHTER", "CRUISER", "SCOUT", "CARRIER", "HAULER" }; snprintf(o, (size_t)n, "FACTION %u %s", id / 6, hn[id % 6]); }
static void l_item(uint32_t id, char *o, int n){ snprintf(o, (size_t)n, "%s %s", sg_mat_name[(id / 20) % 12], sg_item_name[id % 20]); }
static void l_prop(uint32_t id, char *o, int n){ static const char *const ln[8] = { "GRASS", "SAND", "ICE", "ROCK", "ASH", "SWAMP", "JUNGLE", "CRYSTAL" }; snprintf(o, (size_t)n, "%s ON %s", sg_prop_name[id % SGP_N], ln[(id / SGP_N) % 8]); }
static void l_planet(uint32_t id, char *o, int n){ SgPlanetSpec p; sg_planet_spec(&p, (uint64_t)id * 7919u + 5, id < 12 ? (int)id : -1); snprintf(o, (size_t)n, "%s%s%s", sg_planet_class_name[p.cls], p.ring ? " RINGED" : "", p.moons ? " MOONS" : ""); }
static void l_star(uint32_t id, char *o, int n){ static const char *const sn[7] = { "O BLUE", "B BLUE-WHITE", "A WHITE", "F YELLOW-WHITE", "G YELLOW", "K ORANGE", "M RED" }; snprintf(o, (size_t)n, "STAR %s", sn[id % 7]); }
static void l_neb(uint32_t id, char *o, int n){ snprintf(o, (size_t)n, "NEBULA %u", id); }
static void l_gal(uint32_t id, char *o, int n){ snprintf(o, (size_t)n, "%s %u", sg_galaxy_name[id % 8], id / 8); }
static void l_wea(uint32_t id, char *o, int n){ snprintf(o, (size_t)n, "%s %u", sg_weather_name[id % 8], id / 8); }
const SgGen sg_gens[] = {
    { "TREES", 64, 64, 100000, g_tree, l_tree }, { "PLANTS", 64, 64, 100000, g_plant, l_plant }, { "BUILDINGS", 64, 64, 4096, g_bld, l_bld }, { "CITIES", 128, 64, 4096, g_city, l_city },
    { "PEOPLE", 64, 64, 512, g_ppl, l_ppl }, { "CREATURES", 64, 64, 12672, g_crit, l_crit }, { "SHIPS", 64, 64, 6000, g_ship, l_ship }, { "ITEMS", 64, 64, 240, g_item, l_item },
    { "PROPS", 64, 64, 120, g_prop, l_prop }, { "PLANETS", 96, 96, 1000, g_planet, l_planet }, { "STARS", 64, 64, 700, g_star, l_star }, { "NEBULAE", 96, 96, 1000, g_neb, l_neb },
    { "GALAXIES", 96, 96, 800, g_gal, l_gal }, { "WEATHER", 64, 64, 80, g_wea, l_wea } };
const int sg_ngens = (int)(sizeof sg_gens / sizeof sg_gens[0]);
