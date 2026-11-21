#!/usr/bin/env python3
"""Guard the main game's quest contract without needing hardware input.

This is a source-level integrity audit, NOT a substitute for an emulator
playthrough. The ROM/PC builds and visual/gameplay tests run separately.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
game = (ROOT / "game.inc").read_text()
ui = (ROOT / "ui.inc").read_text()
dungeon = (ROOT / "dungeon.inc").read_text()
main = (ROOT / "main.c").read_text()


def require(src, phrase, explanation):
    assert phrase in src, explanation


# Quest targets may not complete in an unrelated zone or alternate world.
require(game, "q_zone==zone && (s16)q_world==(s16)world_no", "quest-world comparison must match signed 16-bit SRAM field")
require(game, "q_type==Q_RESCUE && !q_done && !forge_on && !base_mode && quest_home()",
        "rescued bugs can finish a contract in an unrelated place")
require(main, "!q_done && quest_home() && q_type==Q_HUNT",
        "hunt kills must count only on the posting world")
require(main, "o->champ==2 && q_type==Q_CHAMP && depth_no==q_floor",
        "champion target must be on the contract floor")
require(main, "boss && !mboss && nbo==0 && q_type==Q_BOSS",
        "bonus labyrinth bosses must not replace the campaign contract boss")
require(main, "!q_done && quest_home() && depth_no==q_floor",
        "boss contract must check the correct floor")

# A named champion must be the SAME named target on the board and in combat.
name_def = re.search(r'quest_champ_names\[6\]=\{([^}]+)\}', game)
assert name_def, "missing shared enemy names"
names = re.findall(r'"([^"]+)"', name_def.group(1))
assert len(names) == len(set(names)) == 6
require(ui, "q_need=1+rndi(6)", "champion bounty lacks a persistent target identity")
require(ui, "quest_champ_names[q_need-1]", "board doesn't use the target identity")
require(main, "quest_champ_id()", "combat HUD doesn't recover the advertised target identity")
require(game, "const char *a=q_name+5", "older champion saves do not recover their recorded name")

# Shards now appear only once at their *next* advertised dungeon depth.
require(dungeon, "depth_no==q_floor-q_need+q_have+1",
        "a star shard could appear on arbitrary deeper/revisited floors")
require(game, "depth_no==q_floor-q_need+q_have+1",
        "collecting a stray shard could skip the listed floors")
require(ui, "if(q_floor<q_need) q_floor=q_need",
        "multi-floor shard contract could begin before the first dungeon floor")
for highest in range(2, 12):
    for wanted in (2, 3):
        need = min(wanted, highest)
        for proposed in range(1, highest + 1):
            terminal = max(proposed, need)
            floors = [terminal - need + have + 1 for have in range(need)]
            assert floors == list(range(floors[0], terminal+1))
            assert floors[0] >= 1 and floors[-1] <= highest

# Keep rescue quests achievable in one trip; never overwrite critical quest
# objects if a crowded dungeon hits the 160-object cap.
require(dungeon, "cages_here<remaining && ndo<NDO",
        "rescue floor lacks a guaranteed cage supply or exceeds the object budget")
require(dungeon, "remaining=q_need-q_have", "rescue ignores completed progress")
require(dungeon, "dob[j].kind==K_CAGE", "existing cages weren't counted")
require(dungeon, "dob[k]=dob[--ndo]", "no decorative-object spare-slot recovery")
require(dungeon, "if(qhere && q_type==Q_FETCH && ndo<NDO)",
        "loot spawning can modify the last object when the floor is full")
require(dungeon, "if(qshard && ndo<NDO)",
        "shard spawning can modify the last object when the floor is full")

# Picking up an obsolete relic must not complete a different mission.
require(game, "q_type==Q_FETCH && depth_no==q_floor",
        "old quest relics can complete unrelated contracts")
require(game, 'say("THAT RELIC IS NOT YOUR QUEST TARGET")',
        "obsolete relic handling is missing")

# Do not consume quest goods or a completed job before verifying that the
# guaranteed equipment reward fits. Confirm an accidental GIVE UP.
start = ui.index('if(q_done){\n                        // Claim is atomic')
end = ui.index('say("REWARD CLAIMED!")', start)
claim = ui[start:end]
assert claim.index("bag_n>=BAGMAX") < claim.index("add_gear(pz)") < claim.index("quest_take()") < claim.index("q_type=Q_NONE"), (
    "quest claim can lose the prize or consume goods before the reward fits"
)
require(ui, 'confirm(pg,"ABANDON THIS QUEST?"',
        "quest abandonment lacks confirmation")
require(ui, "EXTRA CAGES ON B", "player doesn't know where to find rescue targets")
require(ui, "NEXT SHARD: B", "player doesn't know which shard floor is next")
require(ui, 'scpy(b,"RETURN TO ")', "player gets no clear world-restricted destination")
require(ui, "tint_name[q_zone]", "quest hints must show story world names")
require(ui, "ncat(b,(u16)q_world)", "endless-world number is absent from quest directions")
require(ui, 'A NAME WAS PAINTED ON MY HULL.', "first recovered memory must foreshadow rather than reveal Bolt's real name")
assert 'I WAS CALLED THE DAWN.' not in ui, "the final name is spoiled by the first recovered memory"
require(ui, "char qhint[48]", "Bolt needs a bounded local quest hint without consuming static GBA RAM")
require(ui, "if(q_type>0 && n<8)", "Bolt's extra line could overflow the eight-line story card")
require(ui, "QUEST DONE. CLAIM YOUR REWARD AT THE SHIP.", "Bolt omits quest turn-in guidance")

print("Quest integrity: location gates, champion identity, exact shard floors,",
      "guaranteed rescues, inventory-safe rewards and navigation hints: PASS")
