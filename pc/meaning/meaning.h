// meaning.h: the MEANING LAYER of the PC port. Every parameter (planet trait, stat, universe theme, people, city, creature and its parts, plant, universe law,
// god power) gets a ROLE and a set of EFFECTS on 32 named CHANNELS, and a set of TAGS (its synergy families). Nothing is stored per item: it is all worked out
// from the parts (65 base traits x 16 flavours, 66 body plans x 192 variants ...). A RESOLVER adds any set of items into one PLANET PROFILE.
//   PURPOSE  = changes rule numbers through the 25 rule channels.   COSMETIC = look, sound, name, colour, flavour only (the 7 look channels).   BOTH = both.
//   The role of a made-up item is the role of its LEAD part (base trait, stock, form, body plan, plant use); the other parts add effects of that kind.
#ifndef PC_MEANING_H
#define PC_MEANING_H
#include <stdint.h>
#include "../content/pc_content.h"

enum { CH_GRAVITY, CH_LIGHT, CH_HEAT, CH_WEATHER, CH_FOOD, CH_WATER, CH_ORE, CH_DANGER, CH_SPEED, CH_TRADE, CH_KNOW, CH_FAITH, CH_HEALTH, CH_LUCK, CH_MAGIC,
       CH_GROWTH, CH_SHELTER, CH_FUEL, CH_CRAFT, CH_DEFENCE, CH_CROWD, CH_TAMING, CH_STEALTH, CH_AIR, CH_TIME,
       CH_BEAUTY, CH_SOUND, CH_TINT, CH_MOOD, CH_GLOW, CH_SCENT, CH_LORE, MN_NCH };
#define MN_RULE_CH 25            // channels 0..24 change rule numbers, 25..31 are look channels
#define MN_NTAG 24
enum { MN_PURPOSE, MN_COSMETIC, MN_BOTH };
// item classes. The first nine are "whole" items; the rest are the parts they are made of (also items: each has a role, effects and tags)
enum { MC_TRAIT, MC_STAT, MC_THEME, MC_PEOPLE, MC_CITY, MC_CREATURE, MC_FLORA, MC_LAW, MC_GOD,
       MC_GIFT, MC_CCOL, MC_RANK, MC_MUT, MC_PLAN, MC_SIZE, MC_DIET, MC_HAB, MC_BEHAV, MC_STOCK, MC_WAY, MC_FORM, MC_CHAR, MC_TBASE, MC_TFLAV,
       MC_FUSE, MC_FHAB, MC_FQUIRK, MC_FFORM, MC_FLEAF, MC_FFLOWER, MC_FFRUIT, MC_FBARK, MC_FCOL, MC_FSIZE, MC_N };
#define MN_EFF 6
typedef struct { uint8_t cls; uint32_t id; int32_t val; } MnRef;      // val: a stat's number, or a law's value
typedef struct { uint8_t role, n; uint8_t ch[MN_EFF]; int8_t amt[MN_EFF], cnt[MN_EFF]; uint32_t tags; } MnItem;   // cnt: the part of amt the resolver counts (the rest is already in the stats)
int  mn_class_size(int cls);                       // how many ids the class has (flora: 536,870,912)
const char *mn_class_name(int cls);
int  mn_item(const MnRef *r, MnItem *o);           // 1 when the id exists
const char *mn_ch_name(int ch);
const char *mn_tag_name(int t);
int  mn_ch_is_rule(int ch);
void mn_means(const MnItem *it, char *out, int n); // "MORE FOOD, SLOWER TRAVEL" or "ONLY LOOKS: WARMER COLOURS"
const char *mn_role_name(int role);

#define MN_MAXPAIRS 12
typedef struct { uint8_t a, b, clash, tag; int8_t score; } MnPair;     // item indexes a and b share (or clash on) a tag
typedef struct {
    int32_t raw[MN_NCH]; int8_t lvl[MN_NCH];       // lvl: -100..100 (soft saturation)
    int n_items, role_n[3], syn_n, clash_n, npairs; MnPair pair[MN_MAXPAIRS]; int16_t tag_syn[MN_NTAG], tag_clash[MN_NTAG]; int16_t tag_n[MN_NTAG];
} MnProfile;
void mn_resolve(const MnRef *refs, int n, MnProfile *p, int want_pairs);   // deterministic; saturating; no overflow for any input
uint64_t mn_profile_hash(const MnProfile *p);

typedef struct {                                   // what the PC game does with a profile (endless-galaxy planets only)
    int hunger_pct, gold_pct, fauna_add, beetle_add, speed_add, hour, weather, node[4], sky_r, sky_g, sky_b, lift;
} MnHooks;
void mn_hooks(const MnProfile *p, MnHooks *h);
void mn_flavour(const MnProfile *p, char *out, int n);     // one plain sentence from the look channels
void mn_summary(const MnProfile *p, char *out, int n);     // the main rule channels in words

// building a planet's item list
typedef struct { int lawn; int law_id[160], law_val[160]; int gods; } MnExtra;
int  mn_planet_refs(const PlanetInfo *pi, const int themes[2], const uint32_t *flora, int nflora, const MnExtra *x, MnRef *out, int max);
int  mn_god_count(void);
int  mn_selfcheck(void);                           // 0 when the tables are consistent
#endif
