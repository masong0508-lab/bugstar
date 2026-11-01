// cr_bio.c: the creatures of the creator and the BIOFOOTPRINT. See cr_bio.h.
// The model is small and honest, not a full simulation: every number is a plain rule of thumb you can read below.
//   plants make food and air from sunlight (net primary production); plant eaters take a share; hunters take a share of the plant eaters' yearly growth
//   (about one tenth of what is eaten becomes body); your people eat, burn wood, clear land for farms and cities and make smoke.
//   Then 120 years are stepped forward to see whether anything dies out. The same recipe always gives the same report.
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "cr_bio.h"
#include "../content/flora.h"
#include "../content/pc_content.h"

// ------------------------------------------------------------------ creatures
static const struct { const char *name, *eats; uint8_t level; } roles[CR_NROLE] = {
 {"GRAZER","LEAVES AND GRASS",1},{"BROWSER","TREE LEAVES AND BARK",1},{"SEED-EATER","SEEDS AND FRUIT",1},{"NECTAR-SIPPER","FLOWERS",1},{"FILTER-FEEDER","WATER PLANTS",1},{"SCAVENGER","THE DEAD",4},
 {"HUNTER","PLANT EATERS",2},{"FISHER","WATER ANIMALS",2},{"AMBUSHER","PLANT EATERS AND HUNTERS",2},{"PARASITE","BLOOD OF OTHERS",2},{"ALLROUNDER","PLANTS AND BEASTS",2},{"APEX HUNTER","HUNTERS",3} };
int cr_species_count(void){ return creature_kind_count(); }
const char *cr_role_name(int role){ return roles[role % CR_NROLE].name; }
int cr_kind(int plan, int size, int diet, int habitat, int behav){ return (size * 48 + diet * 16 + habitat * 4 + behav) * 66 + plan; }
void cr_species(int i, CrSpecies *s){
    static const float mass[4] = {0.05f, 2.0f, 60.0f, 1500.0f};
    Creature c; creature_kind(i % creature_kind_count(), &c); int ro;
    if(c.diet == 0){ if(c.cls == 2) ro = ROLE_FILTER; else if(c.cls == 1) ro = c.size == 0 ? ROLE_NECTAR : ROLE_SEEDEATER; else if(c.cls == 4) ro = ROLE_NECTAR; else { int k = (c.plan + c.size) % 3; ro = k == 0 ? ROLE_GRAZER : k == 1 ? ROLE_BROWSER : ROLE_SEEDEATER; } }
    else if(c.diet == 1){ if(c.size == 3) ro = ROLE_APEX; else if(c.cls == 2) ro = ROLE_FISHER; else if(c.behav == 2) ro = ROLE_PARASITE; else if(c.behav == 1) ro = ROLE_AMBUSHER; else if(c.cls == 5 && c.size == 2) ro = ROLE_ALLROUNDER; else ro = ROLE_HUNTER; }
    else ro = ROLE_SCAVENGER;
    strncpy(s->name, c.name, sizeof s->name - 1); s->name[sizeof s->name - 1] = 0;
    s->level = roles[ro].level; s->aquatic = (uint8_t)(c.cls == 2); s->mass_kg = mass[c.size]; s->eats = roles[ro].eats;
    s->plan = c.plan; s->cls = c.cls; s->size = c.size; s->diet = c.diet; s->habitat = c.habitat; s->behav = c.behav; s->role = (uint8_t)ro;
}
int cr_kind_find(int role, int salt){
    int n = creature_kind_count(), start = (int)(((uint32_t)salt * 2654435761u) % (uint32_t)n);
    for(int k = 0; k < n; k++){ int id = (start + k * 7919) % n; CrSpecies s; cr_species(id, &s); if(s.role == role) return id; }
    return 0;
}

// ------------------------------------------------------------------ small helpers
static double clampd(double v, double lo, double hi){ return v < lo ? lo : v > hi ? hi : v; }
static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }
static double p075(double x){ return x <= 0 ? 0 : sqrt(x * sqrt(x)); }          // x to the 0.75
static int richness(int n){ static const int xs[10] = {0, 1, 3, 6, 12, 24, 48, 96, 200, 100000}, ys[10] = {0, 10, 25, 40, 55, 70, 82, 92, 100, 100};
    for(int i = 1; i < 10; i++) if(n <= xs[i]) return ys[i - 1] + (ys[i] - ys[i - 1]) * (n - xs[i - 1]) / (xs[i] - xs[i - 1]); return 100; }
static const int div_n[10] = {0, 6, 12, 24, 48, 96, 160, 240, 320, 400};
static const double dem1[10] = {0, .3, .6, 1.2, 2.4, 4, 6, 9, 13, 18};         // percent of the plants' yearly growth a plant eater takes, by amount
static const double dem2[10] = {0, 2, 4, 8, 14, 22, 32, 45, 60, 80};           // percent of the plant eaters' growth a hunter takes
// people: by tech step
static const double farm_ha[8] = {0.9, 0.7, 0.55, 0.45, 0.35, 0.22, 0.15, 0.08};
static const double fuel_kg[8] = {1200, 1000, 900, 1100, 2200, 2800, 1500, 500};
static const double smoke_t[8] = {0.9, 0.75, 1.2, 1.8, 5.4, 7.8, 4.2, 0.9};
static const double urban[8] = {0.10, 0.15, 0.25, 0.35, 0.50, 0.65, 0.80, 0.90};
// city styles: people per square km, people per city, wood use, smoke
static const struct { double dens, size, wood, smoke; } cs[16] = {
 {3000,3000,0.6,0.8},{600,800,1.6,0.9},{9000,20000,0.5,1.1},{1500,1200,1.3,0.8},{5000,8000,1.0,0.9},{4000,15000,0.4,0.3},{6000,30000,0.7,1.0},{3000,2500,0.5,0.8},
 {200,300,0.8,0.7},{4000,6000,0.9,1.0},{15000,90000,0.4,1.3},{3000,5000,1.2,0.9},{2500,4000,0.7,0.7},{7000,20000,0.4,0.8},{3500,2500,0.5,0.7},{6000,12000,0.8,1.1} };
// gifts: farm yield, smoke, fuel, city area (multipliers)
static void gift_fx(int g, double *yield, double *smoke, double *fuel, double *city){
    switch(g){ case 16: *yield *= 1.30; break; case 5: *yield *= 1.15; break; case 15: *yield *= 1.10; break; case 3: *fuel *= 0.85; *smoke *= 1.1; break; case 18: *city *= 0.5; break;
        case 7: *city *= 0.85; break; case 12: *yield *= 1.05; break; case 14: *smoke *= 0.9; break; case 1: *fuel *= 0.95; break; case 13: *yield *= 1.05; break; case 22: *smoke *= 0.85; break; default: break; }
}
// calling: farm, smoke, fuel, city, protect
static const struct { double farm, smoke, fuel, city, protect; } cl[16] = {
 {1.3,1,1,1,1},{1.1,1,1,1,1},{0.5,0.9,0.9,1,1},{0.4,0.8,0.8,0.8,1},{1,1.1,1.2,1.3,1},{1,1.4,1.4,1,1},{1,1,1,1.2,1},{0.9,0.9,1,1,1},{1,0.8,1,1,1},{0.9,0.9,0.9,1,1},{0.8,0.8,0.8,0.9,1},{0.5,0.8,0.8,0.5,1},{1,0.9,0.7,1,0.4},{1,1.4,1.5,1,1},{0.8,1,1,1,1},{0.9,0.8,0.8,0.9,1} };

// ------------------------------------------------------------------ the report
void cr_bio(const Recipe *r0, BioReport *b){
    Recipe q = *r0; cr_fix(&q); const Recipe *r = &q; memset(b, 0, sizeof *b);
    CrEnv env; cr_env(r, &env);
    // ---- the world
    static const int8_t wet_cls[16] = {0,0,10,30,-50,10,35,-20,-10,20,-40,-30,-20,-45,20,0};
    static const int8_t wet_wx[4] = {0, 25, 10, -25};
    static const int radius[6] = {2000, 3500, 6371, 9000, 13000, 20000};
    int wet = clampi(r->sea / 2 + 20 + wet_cls[r->cls] + wet_wx[r->weather], 0, 100);
    b->temp_c = env.temp_c; b->wet = wet; b->light = clampi(env.light, 0, 200);
    double tf = 1.0 - ((env.temp_c - 22) / 45.0) * ((env.temp_c - 22) / 45.0); tf = clampd(tf, 0, 1); if(tf < 0.06 && env.temp_c > -70 && env.temp_c < 95) tf = 0.06;      // (lichen grows nearly anywhere)
    double wf = clampd(wet / 55.0, 0.1, 1); if(wet > 95) wf *= 0.9;
    double lf = clampd(b->light / 80.0, 0, 1);
    double lifef = clampd(1.0 + 0.1 * env.life, 0.3, 1.6);
    double fert = clampd(tf * wf * lf * lifef, 0, 1);
    b->fertility = (int)(fert * 100 + 0.5);
    double R = radius[r->size]; double area = 4.0 * 3.14159265 * R * R / 1e6;           // million km2
    double sea = r->sea > 95 ? 0.95 : r->sea / 100.0; b->land_mkm2 = area * (1.0 - sea);
    double land_km2 = b->land_mkm2 * 1e6;
    // ---- the plants
    FloraEnv fe = { env.temp_c, wet, r->sea, r->rough, b->light };
    Flora fl[420]; double wt[420]; int nf = 0;
    for(int i = 0; i < r->npin; i++){ flora_make(r->pin[i].id, &fl[nf]); wt[nf] = 3.0 * r->pin[i].amount; nf++; }
    int want = div_n[r->div]; if(want > 400 - nf) want = 400 - nf;
    { Flora tmp[400]; int g = want > 0 ? flora_for_planet(cr_seed(r) ^ 0xF10FAull, &fe, want, tmp) : 0;       // (a plant has the same name on every planet: the planet picks which ones live here)
      for(int i = 0; i < g; i++){ fl[nf] = tmp[i]; wt[nf] = 1.0 / (1.0 + i * 0.06); nf++; } }
    b->n_flora = nf; b->nlist = nf; for(int i = 0; i < nf; i++) b->list[i] = fl[i].id;
    double wsum = 0, bmass = 0, woody = 0, aqs = 0, fruit = 0, soft = 0, o2 = 0, fo = 0, sh = 0, md = 0, fu = 0, bsum = 0;
    for(int i = 0; i < nf; i++){ double w = wt[i], bm = fl[i].biomass / 10.0; wsum += w; bsum += w * bm; bmass += w * bm;
        if(fl[i].woody) woody += w * bm; else if(fl[i].aquatic) aqs += w * bm; else soft += w * bm; fruit += w * bm * fl[i].food / 15.0;
        o2 += w * fl[i].o2 / 15.0; fo += w * fl[i].food / 15.0; sh += w * fl[i].shelter / 15.0; md += w * fl[i].medicine / 15.0; fu += w * fl[i].fuel / 15.0; }
    double bbar = nf ? bmass / wsum : 0;                              // kg per square metre where plants cover the ground
    double sh_woody = bsum > 0 ? woody / bsum : 0, sh_aq = bsum > 0 ? aqs / bsum : 0, sh_soft = bsum > 0 ? soft / bsum : 0, sh_fruit = bsum > 0 ? fruit / bsum * 0.6 : 0;
    double eo2 = nf ? o2 / wsum : 0;
    b->u_o2 = nf ? (int)(100 * o2 / wsum) : 0; b->u_food = nf ? (int)(100 * fo / wsum) : 0; b->u_shelter = nf ? (int)(100 * sh / wsum) : 0; b->u_med = nf ? (int)(100 * md / wsum) : 0; b->u_fuel = nf ? (int)(100 * fu / wsum) : 0;
    b->woody_pct = (int)(sh_woody * 100 + 0.5);
    double cover = r->cover / 100.0 * (0.25 + 0.75 * fert); if(!nf) cover = 0;
    // ---- your people (they take land and wood first, so the plants left are worked out after)
    double P = cr_pop_count(r->pop), hcm = cr_height_cm(r->height), hm = hcm / 100.0, mass = 14.0 * hm * hm * hm; if(mass < 0.05) mass = 0.05;
    double mw = p075(mass / 65.0), msurf = clampd(sqrt(cbrt(mass / 65.0) * cbrt(mass / 65.0)), 0.05, 4.0);
    double yield = 1, smk = 1, fuelm = 1, citym = 1; for(int g = 0; g < 3; g++) gift_fx(r->gift[g], &yield, &smk, &fuelm, &citym);
    static const double dietm[6] = {1.0, 1.2, 2.5, 1.6, 0.3, 0.1};
    double farm_km2 = P * farm_ha[r->tech] * mw * dietm[r->diet] * cl[r->calling].farm / yield / 100.0;
    if(r->diet == 5) farm_km2 *= (b->light < 60 || r->sea < 20) ? 8.0 : 1.0;                 // sun-and-salt folk need light and a sea
    double ur = urban[r->tech], city_km2 = P * ur / cs[r->cstyle].dens * citym * cl[r->calling].city * msurf; if(city_km2 < 0) city_km2 = 0;
    b->cities = (int)(P * ur / cs[r->cstyle].size + 0.999); if(P < 1) b->cities = 0; b->city_size = (int)(cs[r->cstyle].size);
    if(b->cities < 1 && P >= 1000) b->cities = 1;
    // plants after the people
    double full_cover_km2 = land_km2 * cover;
    double fuelkg_pp = fuel_kg[r->tech] * fuelm * cl[r->calling].fuel * cs[r->cstyle].wood * mw;
    double stand0 = bbar * (full_cover_km2 / 1e6);                      // Gt (kg/m2 x 1e12 m2 per million km2 = 1e12 kg = Gt)
    double turn = 0.06 + 0.5 * (1.0 - sh_woody); double npp0 = stand0 * turn;
    // the sea has its own plants (plankton): about 0.13 kg of carbon per square metre a year at best (Earth's seas: roughly as much as the land)
    double seaf = clampd(1.0 - ((env.temp_c - 15) / 50.0) * ((env.temp_c - 15) / 50.0), 0.05, 1.0) * clampd(b->light / 80.0, 0, 1) * clampd(0.6 + 0.4 * lifef, 0.3, 1.2);
    if(r->sea < 8) seaf = 0;
    double ocean_npp = area * sea * 0.13 * seaf;
    double npp_nat = npp0 + ocean_npp;
    double fuel_frac = npp0 > 0 ? P * fuelkg_pp * 1e-12 / npp0 : (P > 0 ? 9 : 0);
    double cut_km2 = fuel_frac * full_cover_km2 * 0.5;
    double lost = full_cover_km2 > 0 ? (farm_km2 + city_km2 + cut_km2) / full_cover_km2 * cl[r->calling].protect : (P > 0 ? 1 : 0);
    lost = clampd(lost, 0, 0.98);
    double cover_left = cover * (1.0 - lost);
    b->biomass_gt = bbar * (land_km2 * cover_left / 1e6) + ocean_npp * 0.05; double npp_land = b->biomass_gt * turn; double npp = npp_land + ocean_npp; b->npp_gt = npp;
    b->forest_lost_pct = (int)(lost * 100 + 0.5);
    b->farm_pct = (int)(land_km2 > 0 ? clampd(farm_km2 / land_km2 * 100, 0, 100) + 0.5 : 0);
    b->people = P;
    // ---- air
    double extras = npp_nat > 0 ? P * fuelkg_pp * 1e-12 / npp_nat : (P > 0 ? 9 : 0);
    double need_kg = 280.0 * mw * dietm[r->diet]; double pfood = npp_nat > 0 ? P * need_kg * 1e-12 / npp_nat : (P > 0 ? 9 : 0);
    double smoke_gt = P * smoke_t[r->tech] * smk * cl[r->calling].smoke * cs[r->cstyle].smoke * clampd(mw, 0.05, 4) * 1e-9;
    double smoke_ratio = npp_nat > 0 ? smoke_gt / npp_nat : (P > 0 ? 5 : 0);
    b->smoke = clampi((int)(smoke_ratio * 100), 0, 100);
    b->fuel_pct = (int)clampd(extras * 100, 0, 999);
    double use = 0.93 + extras + 0.5 * smoke_ratio + 0.3 * pfood; double eo2s = nf ? eo2 : 0.5; double make = 0.70 + 0.40 * eo2s;
    b->o2_pct = npp_nat > 0.0005 ? clampi((int)(21.0 * make / use * sqrt(npp / npp_nat) + 0.5), 1, 40) : 1;
    // ---- animals
    int nfa = 0; double d1 = 0, d1_soft = 0, d1_wood = 0, d1_fruit = 0, d1_flower = 0, d1_aq = 0; double hp = 0;
    double d2 = 0, d2_fish = 0, d3 = 0; int n_l[5] = {0, 0, 0, 0, 0}; int scav = 0; double animals = 0;
    double mixsoft = 0;
    CrSpecies sp[CR_MAXCRE]; double dem[CR_MAXCRE]; memset(dem, 0, sizeof dem);
    for(int i = 0; i < r->ncre; i++){
        cr_species(r->cre[i].species, &sp[i]); int a = r->cre[i].amount;
        int ro = sp[i].role; n_l[sp[i].level]++; nfa++;
        double ai = a;
        switch(ro){
        case ROLE_GRAZER:   dem[i] = dem1[a] * 1.0 / 100; d1_soft += dem[i]; break;
        case ROLE_BROWSER:  dem[i] = dem1[a] * 0.9 / 100; d1_wood += dem[i]; break;
        case ROLE_SEEDEATER:dem[i] = dem1[a] * 0.5 / 100; d1_fruit += dem[i]; break;
        case ROLE_NECTAR:   dem[i] = dem1[a] * 0.12 / 100; d1_flower += dem[i]; break;
        case ROLE_FILTER:   dem[i] = dem1[a] * 1.0 / 100; d1_aq += dem[i]; break;
        case ROLE_ALLROUNDER: dem[i] = dem1[a] * 0.4 / 100; d1_soft += dem[i] * 0.6; d1_fruit += dem[i] * 0.4; mixsoft += dem2[a] * 0.5 / 100; break;
        default: break; }
        if(sp[i].level == 1) { d1 += dem[i]; if(sp[i].aquatic && ro != ROLE_FILTER) d1_aq += 0; }
        (void)ai;
    }
    // sea animals eat water plants only when there is a sea; a land planet without water starves them
    double sea_ok = r->sea >= 8 ? 1.0 : 0.0;
    double lsh = npp > 0 ? npp_land / npp : 0, osh = npp > 0 ? ocean_npp / npp : 0;           // land and sea shares of the yearly growth
    double pool_soft = sh_soft * lsh, pool_wood = sh_woody * lsh, pool_fruit = sh_fruit * lsh, pool_flower = 0.12 * lsh, pool_aq = (sh_aq * lsh + osh) * sea_ok;
    if(!nf){ pool_soft = pool_wood = pool_fruit = pool_flower = 0; }
    hp = d1 * 0.12;
    for(int i = 0; i < r->ncre; i++){ int ro = sp[i].role, a = r->cre[i].amount; double need = 0;
        if(ro == ROLE_HUNTER) { dem[i] = dem2[a] / 100.0; d2 += dem[i]; need = 1; }
        else if(ro == ROLE_AMBUSHER) { dem[i] = dem2[a] * 0.8 / 100.0; d2 += dem[i]; need = 1; }
        else if(ro == ROLE_PARASITE) { dem[i] = dem2[a] * 0.3 / 100.0; d2 += dem[i]; need = 1; }
        else if(ro == ROLE_FISHER) { dem[i] = dem2[a] / 100.0; d2_fish += dem[i]; need = 1; }
        else if(ro == ROLE_ALLROUNDER) { dem[i] = dem2[a] * 0.5 / 100.0; d2 += dem[i]; need = 1; }
        (void)need; }
    double uh = hp > 0 ? d2 / (hp / 0.12 * 0.0 + 1.0) : 0; (void)uh;
    // hunters take a share of the plant eaters' growth: demand d2 is a share (0..), supply is 1 (all of it); pool is the land plant-eaters' growth
    double herb_land = (d1 - d1_aq) * 0.12, herb_aq = d1_aq * 0.12;
    double u_land = herb_land > 0 ? d2 : (d2 > 0 ? 9 : 0);                          // fraction of the land plant eaters' growth that is hunted
    double u_fish = herb_aq > 0 && sea_ok ? d2_fish : (d2_fish > 0 ? 9 : 0);
    double cp = d2 * herb_land * 0.12 + d2_fish * herb_aq * 0.12 + 0.0;                  // hunters' own growth (part of the plant eaters' growth)
    for(int i = 0; i < r->ncre; i++) if(sp[i].role == ROLE_APEX){ dem[i] = dem2[r->cre[i].amount] / 100.0; d3 += dem[i]; }
    double u_apex = (cp > 0 || d3 == 0) ? (cp > 0 ? d3 : 0) : 9;
    double hunted_pool = (herb_land > 0 ? 1 : 0);
    (void)hunted_pool;
    for(int i = 0; i < r->ncre; i++) if(sp[i].role == ROLE_SCAVENGER) scav = 1;
    // pressure on each pool of plants
    double up_soft = pool_soft > 0 ? (d1_soft) / pool_soft : (d1_soft > 0 ? 9 : 0);
    double up_wood = pool_wood > 0 ? d1_wood / pool_wood : (d1_wood > 0 ? 9 : 0);
    double up_fruit = pool_fruit > 0 ? d1_fruit / pool_fruit : (d1_fruit > 0 ? 9 : 0);
    double up_flower = pool_flower > 0 ? d1_flower / pool_flower : (d1_flower > 0 ? 9 : 0);
    double up_aq = pool_aq > 0 ? d1_aq / pool_aq : (d1_aq > 0 ? 9 : 0);
    // animals' standing mass: yearly food x a few years of body mass
    for(int i = 0; i < r->ncre; i++){ double food = sp[i].level == 1 ? dem[i] * npp : sp[i].level == 2 ? dem[i] * herb_land * npp : sp[i].level == 3 ? dem[i] * cp * npp : dem[i] * npp * 0.05; animals += food * (0.5 + 0.2 * sqrt(sqrt(sp[i].mass_kg))); }
    b->animals_gt = animals; b->n_fauna = nfa; b->n_species = nf + nfa;
    // ---- the web, level by level
    b->lvl_n[0] = nf; b->lvl_n[1] = n_l[1]; b->lvl_n[2] = n_l[2]; b->lvl_n[3] = n_l[3];
    b->lvl_use[0] = (int)(d1 * 100 * 100 / 12.0 * 0.12 + 0.5);              // percent of the plants' growth eaten by animals
    b->lvl_use[0] = (int)(d1 * 100 + 0.5);
    b->lvl_use[1] = (int)clampd(u_land * 100 + 0.5, 0, 999);
    b->lvl_use[2] = (int)clampd(u_apex * 100 + 0.5, 0, 999);
    b->lvl_use[3] = 0;
    for(int i = 0; i < r->ncre && b->n_links < CR_MAXCRE + 4; i++){
        int ro = sp[i].role; double pr = 0; const char *what = sp[i].eats;
        switch(ro){ case ROLE_GRAZER: pr = up_soft; break; case ROLE_BROWSER: pr = up_wood; break; case ROLE_SEEDEATER: pr = up_fruit; break; case ROLE_NECTAR: pr = up_flower; break; case ROLE_FILTER: pr = up_aq; break;
            case ROLE_SCAVENGER: pr = 0.1; break; case ROLE_FISHER: pr = u_fish; break; case ROLE_APEX: pr = u_apex; break;
            case ROLE_ALLROUNDER: pr = (up_soft > u_land ? up_soft : u_land) * 0.9; break; default: pr = u_land; break; }
        snprintf(b->link[b->n_links].who, sizeof b->link[0].who, "%.43s", sp[i].name); snprintf(b->link[b->n_links].what, sizeof b->link[0].what, "%.27s", what);
        b->link[b->n_links].pct = (int)clampd(pr * 100 + 0.5, 0, 999); b->n_links++; }
    // ---- energy: where the plants' yearly growth goes
    b->e_sun = clampi(b->light, 0, 200);
    b->e_plants = clampi((int)(cover_left * 100 + 0.5), 0, 100);                          // how much of the land the plants hold now
    b->e_eaten = clampi((int)(d1 * 100 + 0.5), 0, 100);
    double ppl = pfood + extras; b->e_people = clampi((int)(ppl * 100 + 0.5), 0, 100);
    b->e_lost = clampi(100 - b->e_eaten - b->e_people, 0, 100);
    // ---- the next 120 years
    double f0 = clampd(1.0 - lost, 0.02, 1.0), rf = 0.06 + 0.22 * fert, f = f0, H = n_l[1] || n_l[2] > 0 ? 1.0 : 0, C = d2 + d2_fish > 0 ? 1.0 : 0, A3 = d3 > 0 ? 1.0 : 0, Pn = P >= 1000 ? 1.0 : 0, S = 0;
    double hnames = 0; (void)hnames;
    int has_h = n_l[1] > 0, has_c = (d2 + d2_fish) > 0, has_a = d3 > 0, has_p = P >= 1000;
    double rem_a = d1 * (nf ? 1 : 0), rem_p = ppl * 0.55;                                 // yearly removal as a share of growth
    double u2 = clampd(d2 + d2_fish, 0, 4), u3 = clampd(d3, 0, 4);
    b->collapse_year = 0; int which = 0; double smoke_idx = b->smoke / 100.0;
    for(int y = 0; y < BIO_YEARS; y++){
        double fr = f / f0;
        double removal = (rem_a * H + rem_p * Pn) * (1.0 - 0.3 * f) + 0.04 * S;
        f += rf * (1.0 - f) * 0.5 - removal * f * 0.8 + 0.0; f = clampd(f, 0, 1);
        if(has_h){ double Kh = (1.0 + clampd(0.4 * u2, 0, 0.45)) * (fr + 0.02); H += 0.45 * H * (1.0 - H / Kh) - 0.3 * clampd(u2, 0, 3) * C * H * 0.5; H = clampd(H, 0, 1.6); }
        if(has_c){ double sup = H / (u2 * (C > 0.05 ? C : 0.05) + 0.001); C += 0.35 * C * (clampd(sup, 0, 1.3) - 1.0); C = clampd(C, 0, 1.6); }
        if(has_a){ double sup = (C + 0.2 * H) / (u3 * (A3 > 0.05 ? A3 : 0.05) + 0.001); A3 += 0.3 * A3 * (clampd(sup, 0, 1.3) - 1.0); A3 = clampd(A3, 0, 1.6); }
        if(has_p){ double Kp = clampd(1.25 * fr - 0.25 * smoke_idx - 0.3 * clampd(rem_p - 0.4, 0, 1), 0, 1.6); Pn += 0.06 * Pn * (Kp - Pn); Pn = clampd(Pn, 0, 1.6); }
        S += 0.2 * smoke_idx * Pn - 0.1 * S * (0.5 + f); S = clampd(S, 0, 2);
        if(y % 4 == 0){ int k = y / 4; b->traj[0][k] = (uint8_t)clampd(f * 100, 0, 100); b->traj[1][k] = (uint8_t)clampd(H * 66, 0, 100); b->traj[2][k] = (uint8_t)clampd((has_a ? A3 : C) * 66, 0, 100); b->traj[3][k] = (uint8_t)clampd(Pn * 66, 0, 100); }
        if(!b->collapse_year){
            if(nf && f < 0.10){ b->collapse_year = y + 1; which = 1; }
            else if(has_h && H < 0.06){ b->collapse_year = y + 1; which = 2; }
            else if(has_c && C < 0.05){ b->collapse_year = y + 1; which = 3; }
            else if(has_a && A3 < 0.05){ b->collapse_year = y + 1; which = 4; }
            else if(has_p && Pn < 0.15){ b->collapse_year = y + 1; which = 5; }
        }
    }
    // ---- the scores
    double s_air = b->o2_pct >= 18 && b->o2_pct <= 25 ? 100 : b->o2_pct < 18 ? 100 - 11.0 * (18 - b->o2_pct) : 100 - 9.0 * (b->o2_pct - 25);
    b->sub[0] = clampi((int)s_air, 0, 100);
    int pl = (int)(0.45 * richness(nf) + 0.30 * (100 - b->forest_lost_pct) + 0.25 * clampd(b->e_plants * 1.4, 0, 100)); if(!nf) pl = 0; b->sub[1] = clampi(pl, 0, 100);
    int an = (int)(0.5 * richness(nfa * 6) + 25); if(nfa == 0) an = 50;          // (a planet with no animals is quiet, not dead)
    int starving = 0; for(int i = 0; i < b->n_links; i++) if(b->link[i].pct > 100) starving++;
    if(nfa > 0 && !starving && b->lvl_use[0] <= 30 && scav) an += 25;
    double over = 0; if(b->lvl_use[0] > 25) over += (b->lvl_use[0] - 25) * 0.8;
    if(b->lvl_use[1] > 100) over += (b->lvl_use[1] - 100) * 0.4; if(b->lvl_use[2] > 100) over += (b->lvl_use[2] - 100) * 0.3;
    for(int i = 0; i < b->n_links; i++) if(b->link[i].pct > 100) over += 6 + (b->link[i].pct > 300 ? 300 : b->link[i].pct - 100) * 0.1;
    an -= (int)over; if(nfa > 0 && !scav) an -= 6; if(n_l[2] + n_l[3] > 0 && n_l[1] == 0) an -= 30; b->sub[2] = clampi(an, 0, 100);
    int pe = 100; if(has_p){ pe = (int)(100 - 0.5 * b->smoke - 1.2 * (b->farm_pct > 15 ? b->farm_pct - 15 : 0) - 0.5 * b->forest_lost_pct - 25 * clampd(pfood + extras - 0.4, 0, 2)); } b->sub[3] = clampi(pe, 0, 100);
    int st = 100; if(b->collapse_year) st = 20 - (BIO_YEARS - b->collapse_year) / 8; else { int fe = b->traj[0][BIO_YEARS / 4 - 1]; if(nf && fe < 60) st -= (60 - fe) * 2; }
    if(!b->collapse_year && (b->o2_pct < 10 || lost > 0.9)){ b->collapse_year = 3; which = lost > 0.9 ? 1 : 5; st = 10; }
    b->sub[4] = clampi(st, 0, 100);
    b->health = (b->sub[0] * 2 + b->sub[1] * 2 + b->sub[2] * 2 + b->sub[3] * 2 + b->sub[4] * 2) / 10;
    if(b->collapse_year) b->health = clampi(b->health - 15, 0, 100);
    b->grade = b->health >= 85 ? 'A' : b->health >= 70 ? 'B' : b->health >= 55 ? 'C' : b->health >= 40 ? 'D' : b->health >= 25 ? 'E' : 'F';
    // ---- warnings
    int n = 0;
    if(b->collapse_year && n < BIO_WARN) b->warn[n++] = which == 1 ? "COLLAPSE: THE PLANTS DIE OUT" : which == 2 ? "COLLAPSE: PLANT EATERS DIE OUT" : which == 3 ? "COLLAPSE: HUNTERS DIE OUT" : which == 4 ? "COLLAPSE: TOP HUNTERS DIE OUT" : "COLLAPSE: YOUR PEOPLE DIE OUT";
    if(b->o2_pct < 15 && n < BIO_WARN) b->warn[n++] = "THIN AIR: TOO LITTLE OXYGEN";
    if(b->o2_pct > 28 && n < BIO_WARN) b->warn[n++] = "TOO MUCH OXYGEN: FIRES EVERYWHERE";
    if(!nf && ocean_npp < 0.01 && n < BIO_WARN) b->warn[n++] = "NO PLANTS: NOTHING CAN EAT";
    else if(b->fertility < 12 && n < BIO_WARN) b->warn[n++] = "TOO HARSH FOR MOST PLANTS";
    if(nf > 0 && nf < 8 && n < BIO_WARN) b->warn[n++] = "VERY FEW KINDS OF PLANT";
    if(b->lvl_use[0] > 35 && n < BIO_WARN) b->warn[n++] = "OVERGRAZED: PLANT EATERS TAKE TOO MUCH";
    if(n_l[2] + n_l[3] > 0 && n_l[1] == 0 && n < BIO_WARN) b->warn[n++] = "HUNTERS HAVE NO PLANT EATERS TO HUNT";
    else if(b->lvl_use[1] > 110 && n < BIO_WARN) b->warn[n++] = "HUNTERS TAKE MORE THAN GROWS BACK";
    if(b->lvl_use[2] > 110 && n < BIO_WARN) b->warn[n++] = "TOP HUNTERS ARE STARVING";
    if(has_p && b->forest_lost_pct >= 40 && n < BIO_WARN) b->warn[n++] = "FOREST LOST: MORE THAN 40 PERCENT CUT";
    if(has_p && b->smoke >= 60 && n < BIO_WARN) b->warn[n++] = "SMOKE IS CHOKING THE SKY";
    if(has_p && b->farm_pct >= 30 && n < BIO_WARN) b->warn[n++] = "FARMS COVER A THIRD OF THE LAND";
    if(has_p && pfood + extras > 0.7 && n < BIO_WARN) b->warn[n++] = "YOUR PEOPLE USE MORE THAN GROWS";
    if(nfa > 0 && !scav && n < BIO_WARN) b->warn[n++] = "NO ONE CLEARS THE DEAD";
    b->nwarn = n;
}
