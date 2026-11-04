# WARSHIP BALANCE (patch 17)

The balance pass from `pc/ROADMAP_TODO.md` section C. The measuring program is `bal.c` (plain C, no game needed; `bash pc/warship/bal.sh`, about 3 seconds).
It copies the encounter and aftermath rules of `pc/pc_missions.inc` into `bal_rules.h`, drives the real fight rules (`ws_fight.c`) with the bot pilot, and runs the real colony raid code (`pc/sim/colony.c`).
Build it with `-DBAL_OLD` (`bash pc/warship/bal.sh old`) to see the numbers from before patch 17.

## What it assumes (my guesses, not measured in play: change `profiles()` in bal.c if they are wrong)
- Three stages: EARLY (level 80), MID (level 800), LATE (level 20,000), each RAW (no wingmen, kits or shield spell, little armour) and PREP (wingmen, kits, shield spell, firefly friend, armour).
- The player is always somewhere a warship can drop in (planet or dungeon), so the rates are per hour of play, not per hour of the game being open.
- On a burning surface the player walks about 4 cells a second (the game says about 5 on flat ground), is up to 96 cells from the ship (the world is 128 wide), needs up to 15 s to read the warnings, and starts with 30 to 100 percent of their hearts.
- A buster is stopped when the bot brings every shield layer down inside the window (plus 60 s to get there).

## What was wrong (before)
- A LATE player met a planet-buster about every 48 minutes on NORMAL and every 36 minutes on COMMON; a MID player every 100 and 62 minutes. On RARE a LATE player still met a warship every 30 minutes. Cause: the chance per minute grew with danger and level without a limit, a new roll started the moment the last warship left, and one severe roll in three was a buster.
- The window of a planet-buster fell to 3 minutes for any class from NEBULA LEVIATHAN up.
- The hellscape ship roll used the ship armour upside down (`ship_armour()` is the percent of damage taken, the formula wanted armour points): every ship got the same 5 percent loss and 85 percent pod, so armour did nothing.
- A DEFEND mission raided the colony with 6 + 8 x class: 50 or more for a CITADEL. A big colony on NORMAL won only 42 percent of them, a small one lost every one and was ruined, and on HARSH every lost raid ended the world.

## What changed (all in `pc/pc_missions.inc` and `pc/sim/sim_glue.inc`, same names as in `bal_rules.h`)
| constant | before | now |
|---|---|---|
| `PCM_P_CAP` (most a minute can roll, thousandths: RARE / NORMAL / COMMON) | none | 20 / 45 / 70 |
| `PCM_QUIET_MIN` (no new roll after a severe threat or a boarding party) | 10 (the warship's own wait) | 25 |
| severe types (buster / strikes / troops) | 1 : 1 : 1 | 1 : 2 : 3 |
| `PCM_BUSTER_MIN` (shortest window) | 3 minutes | 5 minutes |
| hellscape ship lost / pod waits | 5 / 85 percent for everyone | 5 to 20 / 85 to 65 percent by armour (more armour is better) |
| `PCM_DEFEND_K` (DEFEND raid = 6 + K x class + bonus/5) | 8 | 4 |
| DEFEND raid on NORMAL | full size | at most 6/5 of the colony's defence + 2 (CALM always holds; HARSH keeps the full size) |

## What was left alone, and why
- **The 1/5 loss.** It only happens on death (20 percent of gold and the cargo). After the change a raw player who never leaves and never fights dies to a buster about once per 100 to 120 hours of play (LATE, NORMAL or COMMON), and a prepared one practically never. That is a penalty people can see coming, not a trap.
- **The 10 minute board window** (`ENC_EXPIRE_MIN`). Nothing in the numbers says it is wrong, and it cannot be judged without play: it is how long you have to find the craft on the scanner.
- **HARSH.** A lost DEFEND raid can still end the world there (it is meant to be the extreme path; story worlds never end). A small colony on NORMAL still loses a DEFEND raid, but it is no longer ruined by it (it loses a few settlers).

## Still to check in play
- A LATE player cannot stop a buster in its window (class 12 or more, 0 percent): the only answer is to leave, or to wait it out in a dungeon (the fire only hurts on the surface). If that feels wrong, the buster window or the bot's idea of a LATE ship is the place to look.
- The profiles above.

## The report: BEFORE patch 17 (`bash pc/warship/bal.sh old`)
```
WARSHIP BALANCE REPORT (full run: 4000 game hours per cell, 200 busters per cell)

1. HOW OFTEN (per hour of play, the player is always somewhere a warship can drop in)
player         mode    threats/h  severe/h  buster/h boarder/h minutes/buster
EARLY RAW      RARE         0.60      0.00     0.000     0.000          0
EARLY RAW      NORMAL       1.41      0.10     0.034     0.141       1765
EARLY RAW      COMMON       2.73      0.17     0.054     0.261       1116
EARLY PREP     RARE         0.59      0.00     0.000     0.000          0
EARLY PREP     NORMAL       1.42      0.10     0.037     0.131       1611
EARLY PREP     COMMON       2.74      0.18     0.059     0.257       1013
MID RAW        RARE         1.14      0.00     0.000     0.000          0
MID RAW        NORMAL       2.42      1.79     0.586     0.065        102
MID RAW        COMMON       3.93      2.91     0.962     0.097         62
MID PREP       RARE         1.15      0.00     0.000     0.000          0
MID PREP       NORMAL       2.47      1.82     0.601     0.065        100
MID PREP       COMMON       3.92      2.89     0.953     0.104         63
LATE RAW       RARE         2.06      0.00     0.000     0.000          0
LATE RAW       NORMAL       3.67      3.67     1.245     0.000         48
LATE RAW       COMMON       5.03      5.03     1.667     0.000         36
LATE PREP      RARE         2.05      0.00     0.000     0.000          0
LATE PREP      NORMAL       3.67      3.67     1.201     0.000         50
LATE PREP      COMMON       5.03      5.03     1.696     0.000         35

2. PLANET-BUSTER (NORMAL and COMMON only): stop it in the window, or stay on the surface and burn
player         avg class   window      stop it survive the fire (stays on the surface)
EARLY RAW              7      8 m          90%            99%
EARLY PREP             7      8 m          92%           100%
MID RAW                7      8 m          46%            99%
MID PREP               7      8 m          49%           100%
LATE RAW              12      3 m           0%            99%
LATE PREP             12      3 m           0%           100%

3. WHAT A LOST BUSTER COSTS (only the death penalty: 1/5 of the gold and the cargo hold; the save is otherwise untouched)
   a player with 10,000 gold loses 2000 gold per death; busters that end in death per hour of play, stay-and-burn:
   EARLY RAW      NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 19318.1 hours
   EARLY RAW      COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 12219.8 hours
   EARLY PREP     NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   EARLY PREP     COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   MID RAW        NORMAL  deaths per hour (never leaves, never fights) 0.003 = one per 371.8 hours
   MID RAW        COMMON  deaths per hour (never leaves, never fights) 0.004 = one per 226.5 hours
   MID PREP       NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   MID PREP       COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   LATE RAW       NORMAL  deaths per hour (never leaves, never fights) 0.011 = one per 89.2 hours
   LATE RAW       COMMON  deaths per hour (never leaves, never fights) 0.015 = one per 66.6 hours
   LATE PREP      NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   LATE PREP      COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours

4. SIDE GAME DIFFICULTY: a DEFEND mission brings a real raid on the colony (size 6 + 8 x class + bonus/5). Same colonies and raids on all three.
colony       def | CALM  won lost ruin gone | NORMAL won lost ruin gone | HARSH  won lost ruin gone
SMALL          4 |       100%    0%    0%    0% |         0%  100%  100%    0% |         0%  100%    0%  100%
MIDDLE        85 |       100%    0%    0%    0% |        44%   55%    1%    0% |        44%   55%    0%   55%
BIG           84 |       100%    0%    0%    0% |        42%   57%    1%    0% |        42%   57%    0%   57%

TARGETS
FAIL: NORMAL busters 0.586 an hour for MID (limit one per 3 hours)
FAIL: COMMON busters 0.962 an hour for MID (limit one per 85 minutes)
FAIL: NORMAL severe threats 1.79 an hour for MID (limit 1.6)
FAIL: NORMAL busters 0.601 an hour for MID (limit one per 3 hours)
FAIL: COMMON busters 0.953 an hour for MID (limit one per 85 minutes)
FAIL: NORMAL severe threats 1.82 an hour for MID (limit 1.6)
FAIL: RARE fires 2.06 threats an hour for LATE (limit 1.2)
FAIL: NORMAL busters 1.245 an hour for LATE (limit one per 3 hours)
FAIL: COMMON busters 1.667 an hour for LATE (limit one per 85 minutes)
FAIL: NORMAL severe threats 3.67 an hour for LATE (limit 1.6)
FAIL: RARE fires 2.05 threats an hour for LATE (limit 1.2)
FAIL: NORMAL busters 1.201 an hour for LATE (limit one per 3 hours)
FAIL: COMMON busters 1.696 an hour for LATE (limit one per 85 minutes)
FAIL: NORMAL severe threats 3.67 an hour for LATE (limit 1.6)
FAIL: NORMAL: a big colony wins only 42% of DEFEND raids
FAIL: NORMAL: a small colony is ruined by 100% of DEFEND raids
65 checks, 16 failed
```

## The report: NOW (`bash pc/warship/bal.sh`)
```
WARSHIP BALANCE REPORT (full run: 4000 game hours per cell, 200 busters per cell)

1. HOW OFTEN (per hour of play, the player is always somewhere a warship can drop in)
player         mode    threats/h  severe/h  buster/h boarder/h minutes/buster
EARLY RAW      RARE         0.60      0.00     0.000     0.000          0
EARLY RAW      NORMAL       1.33      0.10     0.017     0.133       3582
EARLY RAW      COMMON       2.35      0.14     0.022     0.226       2759
EARLY PREP     RARE         0.59      0.00     0.000     0.000          0
EARLY PREP     NORMAL       1.34      0.09     0.017     0.133       3529
EARLY PREP     COMMON       2.35      0.15     0.025     0.215       2400
MID RAW        RARE         1.02      0.00     0.000     0.000          0
MID RAW        NORMAL       1.40      1.04     0.177     0.039        338
MID RAW        COMMON       1.72      1.27     0.213     0.045        281
MID PREP       RARE         1.03      0.00     0.000     0.000          0
MID PREP       NORMAL       1.42      1.05     0.177     0.036        338
MID PREP       COMMON       1.74      1.28     0.218     0.042        275
LATE RAW       RARE         1.04      0.00     0.000     0.000          0
LATE RAW       NORMAL       1.30      1.30     0.226     0.000        265
LATE RAW       COMMON       1.57      1.57     0.264     0.000        228
LATE PREP      RARE         1.00      0.00     0.000     0.000          0
LATE PREP      NORMAL       1.30      1.30     0.216     0.000        277
LATE PREP      COMMON       1.56      1.56     0.263     0.000        229

2. PLANET-BUSTER (NORMAL and COMMON only): stop it in the window, or stay on the surface and burn
player         avg class   window      stop it survive the fire (stays on the surface)
EARLY RAW              7      8 m          90%            90%
EARLY PREP             7      8 m          92%           100%
MID RAW                7      8 m          46%            94%
MID PREP               7      8 m          49%           100%
LATE RAW              12      5 m           0%            96%
LATE PREP             12      5 m           0%           100%

3. WHAT A LOST BUSTER COSTS (only the death penalty: 1/5 of the gold and the cargo hold; the save is otherwise untouched)
   a player with 10,000 gold loses 2000 gold per death; busters that end in death per hour of play, stay-and-burn:
   EARLY RAW      NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 5415.1 hours
   EARLY RAW      COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 4170.3 hours
   EARLY PREP     NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   EARLY PREP     COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   MID RAW        NORMAL  deaths per hour (never leaves, never fights) 0.006 = one per 181.4 hours
   MID RAW        COMMON  deaths per hour (never leaves, never fights) 0.007 = one per 151.0 hours
   MID PREP       NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   MID PREP       COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   LATE RAW       NORMAL  deaths per hour (never leaves, never fights) 0.008 = one per 118.7 hours
   LATE RAW       COMMON  deaths per hour (never leaves, never fights) 0.010 = one per 101.9 hours
   LATE PREP      NORMAL  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours
   LATE PREP      COMMON  deaths per hour (never leaves, never fights) 0.000 = one per 999.0 hours

4. SIDE GAME DIFFICULTY: a DEFEND mission brings a real raid on the colony (size 6 + 4 x class + bonus/5). Same colonies and raids on all three.
colony       def | CALM  won lost ruin gone | NORMAL won lost ruin gone | HARSH  won lost ruin gone
SMALL          4 |       100%    0%    0%    0% |         0%  100%    0%    0% |         0%  100%   83%   17%
MIDDLE        85 |       100%    0%    0%    0% |       100%    0%    0%    0% |       100%    0%    0%    0%
BIG           84 |       100%    0%    0%    0% |       100%    0%    0%    0% |       100%    0%    0%    0%

TARGETS
65 checks, 0 failed
```
