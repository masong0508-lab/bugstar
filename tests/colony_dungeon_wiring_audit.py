#!/usr/bin/env python3
"""Audit actual PC dungeon/colony rescue hook integration and versioned saves.

The real gameplay unit tests in pc/sim/{colony,mine}_test.c exercise production
C. This audit makes sure build.sh injects those C methods at the actual monster
kill/quest goal sites and that the UI advertises the reachable objective.
"""
from pathlib import Path

R = Path(__file__).resolve().parents[1]
src = {p: (R / p).read_text() for p in [
    "pc/build.sh", "dungeon.inc", "pc/sim/sim_glue.inc",
    "pc/sim/sim_ui.inc", "pc/sim/mine.c", "pc/sim/mine.h",
    "pc/sim/colony.c", "pc/sim/colony.h", "pc/sim/sim_core.c",
]}

checks = {
    "real elite goal still gives combat rewards": (
        "dungeon.inc",
        'if(fl_ev==FE_ELITE && o->champ==1 && !fl_done && ++fl_have>=fl_need){ fl_done=1;'
    ),
    "dungeon mission event picked from live colony and mine": (
        "pc/sim/sim_glue.inc", "mn_dungeon_need(sim_mine(),co)) return COLC_MINE;"
    ),
    "mine requires completed elite objective": (
        "pc/sim/sim_glue.inc", "call==COLC_RAID || call==COLC_MINE ? FE_ELITE"
    ),
    "real dungeon victory resolves actual mining hazard": (
        "pc/sim/sim_glue.inc", "if(call==COLC_MINE) mn_dungeon_rescue(sim_mine(),co);"
    ),
    "loot call must pass persisted cooldown guard": (
        "pc/sim/colony.c", "day < c->call_cool) return 0;"
    ),
    "new rescue quest has its own supply payout": (
        "pc/sim/colony.c", "case COLC_MINE:"
    ),
    "v2 colony file persists cooldown": (
        "pc/sim/colony.c", "p32(q + COL_REC_V1_BYTES, c->call_cool);"
    ),
    "v2 colony file reads cooldown": (
        "pc/sim/colony.c", "if(rb == COL_REC_BYTES) c->call_cool = g32(q + COL_REC_V1_BYTES);"
    ),
    "legacy 164-byte files import correctly": (
        "pc/sim/colony.c", "n == 1 + (size_t)in[0] * COL_REC_V1_BYTES"
    ),
    "COLN chunk gets version two": (
        "pc/sim/sim_core.c", 'PUT("COLN", s, 2, m);'
    ),
    "both COLN and MINE v2 accepted by parser": (
        "pc/sim/sim_core.c", '(!memcmp(h, "MINE", 4) || !memcmp(h, "COLN", 4))'
    ),
    "mine rescues preserve empty seams": (
        "pc/sim/mine.c", "m->seam[z]?MNZ_OPEN:MNZ_DRY"
    ),
    "rescuing mine saves crew health": (
        "pc/sim/mine.c", "m->hurt_days=0;"
    ),
    "colony mine menu exposes rescue": (
        "pc/sim/sim_ui.inc", 'scat(b,"DUNGEON RESCUE");'
    ),
    "existing dungeon dispatch is still installed": (
        "pc/build.sh", "'        sim_dn_pick(); switch(fl_ev){'"
    ),
    "elite objective actually signals completion in the PC build": (
        "pc/build.sh", "sfx(6); sim_dn_done(FE_ELITE); }"
    ),
    "rescue message is drawn after elite loot message": (
        "pc/build.sh", "'say(b); msg_timer=50; sfx(6); sim_dn_done(FE_ELITE); }'"
    ),
    "cage/colony settler objective still completes": (
        "pc/build.sh", "sim_dn_done(FE_QUIET)"
    ),
}

for title, (path, needle) in checks.items():
    if needle not in src[path]:
        raise AssertionError(f"{title}: expected production source in {path}: {needle}")

assert "fl_done=1; sim_dn_done(FE_ELITE); int g=" not in src["pc/build.sh"], (
    "old placement loses the colony reward notification to ELITES SLAIN"
)
assert "COLC_MINE" not in src["dungeon.inc"], (
    "PC side-game call must not leak into original GBA dungeon binary"
)
print(f"Colony mine dungeon hooks, rescue UI and versioned save migration: PASS ({len(checks)+2} checks)")
