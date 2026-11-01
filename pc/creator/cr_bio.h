// cr_bio.h: creatures of the creator and the BIOFOOTPRINT of a recipe. Pure functions: the same recipe always gives the same report.
#ifndef CR_BIO_H
#define CR_BIO_H
#include "cr_recipe.h"

// ---- creatures: the PC content library's 12,672 kinds (66 body plans x 192 variants). Each kind eats like one of 12 ROLES; the footprint works with roles.
//      kind id = variant * 66 + plan; variant = size * 48 + diet * 16 + habitat * 4 + behaviour (the same numbers creature_kind() uses)
enum { ROLE_GRAZER, ROLE_BROWSER, ROLE_SEEDEATER, ROLE_NECTAR, ROLE_FILTER, ROLE_SCAVENGER, ROLE_HUNTER, ROLE_FISHER, ROLE_AMBUSHER, ROLE_PARASITE, ROLE_ALLROUNDER, ROLE_APEX, CR_NROLE };
typedef struct { char name[72]; uint8_t level;           // 1 plant eater, 2 hunter, 3 top hunter, 4 scavenger
                 uint8_t aquatic; float mass_kg; const char *eats; uint8_t plan, cls, size, diet, habitat, behav, role; } CrSpecies;
int  cr_species_count(void);
int  cr_kind(int plan, int size, int diet, int habitat, int behav);     // a kind id from its parts
int  cr_kind_find(int role, int salt);                                  // a kind that eats like this role (salt picks which one)
void cr_species(int i, CrSpecies *s);
const char *cr_role_name(int role);

// ---- the report
#define BIO_WARN 8
#define BIO_YEARS 120
typedef struct {
    // the world
    int temp_c, wet, light, fertility;              // C, 0..100, percent of a normal sun, 0..100
    double land_mkm2;                               // millions of square kilometres of land
    // plants
    int n_flora;                                    // kinds of plant on the planet (a pinned one counts)
    double biomass_gt, npp_gt;                      // plants standing, and what they make each year (gigatonnes)
    int woody_pct;                                  // share of the plants that are trees and bushes
    int u_o2, u_food, u_shelter, u_med, u_fuel;     // what the plants give, 0..100 each
    // animals
    int n_fauna, n_species;                         // kinds of animal; flora + fauna
    double animals_gt;                              // standing mass of animals
    // food web: four levels (plants, plant eaters, hunters, top hunters): supply and demand in percent of what the level below makes
    int lvl_use[4];                                 // how much of each level is eaten, percent (0..200)
    int lvl_n[4];                                   // kinds at each level
    int n_links; struct { char who[44], what[28]; int pct; } link[CR_MAXCRE + 4];     // who eats what, how hard (percent of the supply)
    // air and energy
    int o2_pct;                                     // oxygen in the air
    int e_sun, e_plants, e_eaten, e_people, e_lost; // percent of the sunlight: captured by plants, eaten by animals, used by your people, lost as heat
    // your people
    double people;                                  // how many
    int cities, city_size;                          // how many cities and how big each is (people)
    int farm_pct, forest_lost_pct;                  // percent of the land under farms; percent of the forest cut
    int smoke;                                      // 0..100 (100 = choking)
    int fuel_pct;                                   // percent of the plants' yearly growth burned or cut
    // the score
    int health;                                     // 0..100
    char grade;                                     // 'A'..'F'
    int sub[5];                                     // air, plants, animals, people, steadiness: 0..100
    int collapse_year;                              // 0 = no collapse in 120 years, else the year the first level dies
    uint8_t traj[4][BIO_YEARS / 4];                 // the next 120 years: plants, plant eaters, hunters, people (0..100, sampled every 4 years)
    int nwarn; const char *warn[BIO_WARN];
    // the flora list used (first 400 kinds), for the screen
    int nlist; uint32_t list[400];
} BioReport;
void cr_bio(const CrRecipe *r, BioReport *b);

// ---- plants, for the screen (the game's own code must not include flora.h)
typedef struct { char name[40], about[200], form[16], use[16], habitat[16], size[8]; uint8_t sw_leaf, sw_flower, o2, food, shelter, medicine, fuel, woody; uint16_t height_cm; } CrPlant;
void     cr_plant(uint32_t id, CrPlant *p);
uint32_t cr_plant_nth(uint64_t seed, uint32_t n);        // the n-th plant of a catalogue seed (every plant once)
uint32_t cr_plant_count(void);                           // 536,870,912
int      cr_swatch_near(uint16_t rgb);                   // the swatch that looks most like this colour
void cr_people_info(int stock, int calling, char *name56, char *about1_72, char *about2_72);   // one of the 512 kinds of people
void cr_city_info(int form, int character, char *name56, char *about1_72);                       // one of the 256 kinds of city
#endif
