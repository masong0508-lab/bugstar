// god.h: the god powers of the side game, and when the character level unlocks them (DEPTH_PLAN section 13).
//   Power number n (1-based) opens when level >= GOD_STEP_LV x n. Sandbox mode opens all of them.
//   This file is plain C with no game types. The effect of a power on the side-game world is in sim_core.c (sim_cast).
#ifndef GOD_H
#define GOD_H
#include <stdint.h>
#include <stddef.h>
#include "lv.h"

#define GOD_STEP_LV 1000          // one power for every 1,000 levels. Change this one number to change the whole ladder.
#define GOD_MAX     128           // most powers the save format can hold

// What a power does to the side-game world (sim_cast in sim_core.c reads these). amt is the size of the effect.
enum { GE_FERTILE, GE_CLEAR, GE_RAIN, GE_IDEA, GE_HILL, GE_FOREST, GE_SPECIES, GE_FEED, GE_REST, GE_SOCIAL, GE_FUN, GE_WATER, GE_SHELTER,
       GE_COMFORT, GE_HEALTH, GE_LUCK, GE_SKILL, GE_PESTS, GE_BIRDS, GE_FOOD, GE_PEOPLE, GE_DAY, GE_LEGEND, GE_MOOD, GE_ORE, GE_NEFFECT };

typedef struct {
    const char *name;             // ALL CAPS, up to 22 letters
    const char *line;             // one plain line, up to 46 letters
    uint8_t eff, amt, cost;       // effect kind, size, spirit cost
} GodPower;

int  god_count(void);                                  // how many powers the table has (at least 40)
const GodPower *god_power(int n);                      // 1-based; 0 when n is out of range
void god_need_level(int n, Lv *out);                   // the level that opens power n: GOD_STEP_LV x n
int  god_unlocked(const Lv *level, int n, int sandbox);        // 1 if power n is open at this level
int  god_unlocked_count(const Lv *level, int sandbox);         // how many are open (never more than god_count)

// Which unlocks the player has already been told about (saved in the .sim file).
typedef struct { uint8_t seen[GOD_MAX / 8]; uint8_t sandbox; } GodState;
void god_state_init(GodState *g);
// Looks at the level and returns the numbers of powers that opened since the last call (ascending), up to max. Marks them as told.
// In sandbox mode every power is open: they are all marked as told at once and 0 is returned (the caller shows one sandbox message).
int  god_check(GodState *g, const Lv *level, int *out, int max);
void god_unlock_text(int n, char *buf, size_t sz);             // "GOD POWER 3 IS OPEN: CALL RAIN"
void god_locked_text(int n, const Lv *level, char *buf, size_t sz);   // "LOCKED. NEEDS LEVEL 5000" (or "OPEN" in the list)
#define GOD_STATE_BYTES (GOD_MAX / 8 + 1)
void god_state_to_bytes(const GodState *g, uint8_t *out);
void god_state_from_bytes(GodState *g, const uint8_t *in);     // bits past the table are cleared

#endif
