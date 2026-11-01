// ws.h: COLOSSAL WARSHIPS for the BUGSTAR PC port (plain C, no game code). See pc/warship/README.md.
//   * WN: a wide number (mantissa x 10^exponent) for sizes, masses, HP, costs and rewards from 10^-60 to 10^(10^15).
//   * A craft: size (log scale, 1 nm to far past a universe), class, hull grammar (shape, symmetry, segments, add-ons, surface, culture,
//     material), its original name, sections and subsystems, HP, and its COST BREAKDOWN.
//   * ONE REWARD FORMULA: reward = cost x attack-power factor x method factor x weapon factor (ws_pay).
//   * THREAT and recommended preparation (ws_threat).
// Deterministic: the same seed and size give the same craft every time (hashing only; the numbers use the C maths library).
#ifndef WS_H
#define WS_H
#include <stdint.h>

// ---------------------------------------------------------------- WN: value = m x 10^e, 1 <= m < 10 (or m = 0)
typedef struct { double m; int64_t e; } WN;
#define WN_EMAX 1000000000000000LL       // the type's top: 10^(10^15). Anything bigger saturates there (wn_sat says so)
WN   wn(double v);                       // from a plain (non-negative) double
WN   wn_p10(double x);                   // 10^x for any x (x may be huge: 1e12 is fine)
WN   wn_mul(WN a, WN b);
WN   wn_div(WN a, WN b);
WN   wn_add(WN a, WN b);
WN   wn_sub(WN a, WN b);                 // saturates at 0
WN   wn_muld(WN a, double k);
WN   wn_pow(WN a, double p);             // a^p
double wn_log10(WN a);                   // -1e300 for zero
double wn_to_d(WN a);                    // the value as a double (inf above 1e308, 0 below 1e-308)
int  wn_cmp(WN a, WN b);
int  wn_is_zero(WN a);
int  wn_sat(WN a);                       // 1 if a is at the type's top
// Text, always fits cap (>= 24). style 0: "407 SEXTILLION" / "12.5" / "40.7 SEPTILLIONTHS"; style 1: "4.07E23"; style 2: both when long.
// The -illion names come from pc/giant (Conway-Wechsler, any size). Names longer than namecap letters are cut with "..".
void wn_text(WN a, char *out, int cap, int style);
void wn_sci(WN a, char *out, int cap);   // "4.07 X 10^23"

// ---------------------------------------------------------------- the size parameter
// z = 100 x log10(length in metres): -900 is 1 nanometre, 0 is 1 metre, 520 is the 160 km reference station, 2695 is the observable universe.
#define WS_ZMIN   (-900)
#define WS_ZMAX_FAIR 3000                // biggest size the game rolls without cheats (10^30 m: a MULTIVERSE HULK, very rare endgame)
#define WS_ZMAX   300000                 // biggest size the cheats allow (10^3000 m)
#define WS_ZREF   520                    // 10^5.204 = 160 km: the reference battle station (Z_REF in the README)
#define WS_UNIVERSE_M 8.8e26             // the observable universe, across (m)

enum { WC_NANO, WC_MICRO, WC_MITE, WC_SKIFF, WC_CUTTER, WC_FRIGATE, WC_DREAD, WC_CITADEL, WC_MOON, WC_WORLD, WC_STAR, WC_SYSTEM,
       WC_NEBULA, WC_CLUSTER, WC_GALAXY, WC_SUPER, WC_UNIVERSE, WC_MULTI, WC_BEYOND, WC_N };
const char *ws_class_name(int c);        // "NANO SWARM" ... "UNIVERSE HULK"
const char *ws_class_about(int c);       // one line
int  ws_class_of(int z);
int  ws_class_lo(int c);                 // smallest z of the class
void ws_len_text(int z, char *out, int cap);   // "160 KM", "3.2 NM", "1.5 AU", "12,000 LY", "2.3 UNIVERSES"

// ---------------------------------------------------------------- the hull grammar
enum { WS_SPHERE, WS_RING, WS_SPINDLE, WS_WEDGE, WS_HIVE, WS_SHELL, WS_CITY, WS_FLEET, WS_TORUS, WS_NEEDLE, WS_CRESCENT, WS_LATTICE,
       WS_DISC, WS_CROSS, WS_FORK, WS_SPIRAL, WS_NSHAPE };
#define WS_NSURF 8
#define WS_NCULT 12
#define WS_NMAT 10
#define WS_NADD 6                        // add-on bits: spines, fins, pods, outer rings, towers, engines
const char *ws_shape_name(int s);
const char *ws_surf_name(int s);
const char *ws_cult_name(int c);         // the builders (original cultures)
const char *ws_mat_name(int m);
const char *ws_add_name(int bit);
typedef struct { double rho, price, hard, structf; } WsMat;   // kg per m^3, credits per kg, HP per tonne factor, solid share factor
const WsMat *ws_mat(int m);

// a primitive of the hull model (unit space: the craft is 1 long, centred at 0): kind 0 sphere, 1 box, 2 ring (torus), 3 cone, 4 cylinder
typedef struct { uint8_t kind; float x, y, z, a, b, c; float yaw, pitch; } WsPrim;
#define WS_MAXPRIM 48

enum { WSS_GEN, WSS_REACTOR, WSS_HANGAR, WSS_TURRET, WSS_BRIDGE, WSS_NKIND };
const char *ws_sub_name(int k);          // "SHIELD GENERATOR" ...
typedef struct { uint8_t kind, sec, alive, inside_kill; WN hp, hp_max; } WsSub;
#define WS_MAXSUB 24
#define WS_MAXSEC 8
typedef struct { char name[24]; WN hp, hp_max; uint8_t nsub; } WsSec;

typedef struct {
    uint32_t seed; int z, cheat;
    int cls, shape, sym, segs, adds, surf, cult, mat;
    char name[28];                       // its own name (original syllables), e.g. "THE VOLLAR KESH"
    char title[44];                      // "VOLLAR KESH - RING MOON FORTRESS"
    double logL;                         // log10(length m)
    WN len, vol, mass, crew, air_n, wpn_n;  double air_len;   // m, m^3, kg, crew (fractions are AI minds), aircraft and their length (m), weapons
    WN power;                            // reactor output, watts
    WN hp_hull, hp_shield;               // total hull HP (all sections) and one shield layer's HP
    int layers;                          // shield layers
    int nsec, nsub; WsSec sec[WS_MAXSEC]; WsSub sub[WS_MAXSUB];
    int nprim; WsPrim prim[WS_MAXPRIM];
    double fs, fa;                       // envelope volume / L^3 and surface / L^2
    WN cost[6];                          // materials, crew, aircraft, weapons, power, total (gold, calibration K included)
    double dps;                          // what it fires at you per second (before your armour), and
    int wave;                            // fighters per wave
} WsCraft;
enum { WK_MAT, WK_CREW, WK_AIR, WK_WPN, WK_POW, WK_TOTAL };
void ws_make(WsCraft *c, uint32_t seed, int z, int cheat);
void ws_scale(WsCraft *c, int z, int cheat);   // the same design (shape, culture, material, name) at another size
void ws_make_ref(WsCraft *c);            // THE REFERENCE STATION: 160 km grey steel sphere, the calibration point
uint32_t ws_signature(const WsCraft *c); // a hash of the hull grammar choices (for variety counts)
double ws_K(void);                       // the calibration constant (gold per credit)

// ---------------------------------------------------------------- the reward
enum { WW_LASER, WW_RAIL, WW_MISSILE, WW_TORPEDO, WW_NOVA, WW_N };
const char *ws_wpn_name(int w);
double ws_wpn_mult(int w);               // damage per shot against the SIMPLE laser
typedef struct {
    double attack;                       // attack power in levels' worth (ws_attack)
    WN out_dmg[WW_N];                    // damage done from the outside, by weapon
    WN in_dmg;                           // damage done from the inside (sabotage, boarding)
    int sab_crit, sab_all;               // sabotage acts: on critical systems (reactor, shield generator, bridge) / all of them
} WsKill;
typedef struct { WN cost; double A, Fm, Fw, inside, precision; WN reward; int cheat; } WsPay;
double ws_attack(double level, int laser_lv, int gun, int tech_lv);   // levels' worth: level + 5 x laser + gun + 25 x tech (at least 1)
double ws_A(double attack);              // the attack-power factor: (attack)^-ALPHA, ALPHA = log10(4.07e23 / 1e11) / 3
double ws_Fm(double inside, double precision);   // method factor: 1 - 0.45 x inside + 0.15 x inside x precision
double ws_Fw(const WsKill *k);           // weapon factor: the damage-weighted mean of (weapon multiplier)^-0.4
void ws_pay(const WsCraft *c, const WsKill *k, WsPay *p);
#define WS_ALPHA 4.2032113
#define WS_CAL_GOLD 4.07e23

// ---------------------------------------------------------------- the fight numbers
double ws_shot(double attack, int w);    // damage of one shot: 10 x attack^3 x weapon multiplier
typedef struct { int tier; const char *word; double need_attack; int need_ehp, need_wing, need_kits, need_fuel; double ttk_s; } WsThreat;
// ehp = what your ship can take (hull + shield) x 100 / armour; your attack; wingmen; repair kits
void ws_threat(const WsCraft *c, double attack, int ehp, int wing, int kits, int fuel, WsThreat *t);
const char *ws_threat_word(int tier);    // TRIVIAL EASY FAIR HARD DEADLY HOPELESS
#endif
