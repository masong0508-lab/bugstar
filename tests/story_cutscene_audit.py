#!/usr/bin/env python3
"""Protect Bugstar's real in-game story from lore, layout and progression regressions.

Reads the same C dialogue tables and event hooks compiled into GBA/PC/Android.
This is a source-level story and presentation audit, not a substitute for
native-resolution cutscene capture or a complete emulator playthrough.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
ui = (root / "ui.inc").read_text()
game = (root / "game.inc").read_text()
main = (root / "main.c").read_text()
space = (root / "space.inc").read_text()
town = (root / "town.inc").read_text()
dungeon = (root / "dungeon.inc").read_text()


def section(text, begin, end):
    assert begin in text and end in text, f"missing story section: {begin} -> {end}"
    return text.split(begin, 1)[1].split(end, 1)[0]


def literals(src):
    """Unescape only quoted C text, preserving checks against line widths."""
    strings = re.findall(r'"((?:\\.|[^"\\])*)"', src)
    return [s.replace(r'\"', '"').replace(r"\\", "\\") for s in strings]


def require(text, phrase, reason):
    assert phrase in text, reason


mem = section(ui, "static const char *const bolt_mem[8][4]={",
              "// One short, skippable cinematic chapter slate")
memlines = literals(mem)
assert len(memlines) == 32, f"expected 8 memories of 4 lines; got {len(memlines)}"
assert all(0 < len(s) <= 54 for s in memlines), "memory line does not fit 220-pixel card"
assert all(len(memlines[k*4]) <= 33 for k in range(8)), (
    "Bolt's ship-menu first-line recaps must fit the original 48-byte buffer"
)

assert "A NAME WAS PAINTED ON MY HULL." in memlines[:4], (
    "Bolt's original clue must precede the name itself"
)
assert all("THE DAWN" not in s for s in memlines[:28]), (
    "the real name must not be revealed by the first seven memories"
)
assert any("MY NAME" in s for s in memlines[28:]), (
    "final memory should pay off the identity mystery"
)
assert "I CARRIED SEEDS BETWEEN STARS." in memlines, (
    "Bolt's origin as the planter of the story worlds is missing"
)
assert "THE KING CUT OPEN MY HEART." in memlines, (
    "the King causing the Technobeast must remain explicit"
)

cinema = section(ui, "static int bolt_chapter_open(", "static void bolt_card_x(")
title = literals(section(cinema, "title[8]=", "sub[8]"))
sub = literals(section(cinema, "sub[8]=", "int k=beat"))
assert len(title) == len(sub) == 8, "missing a chapter title or subtitle"
assert all(len(t) <= 28 for t in title), "2x chapter title would exceed 240px"
assert all(len(t) <= 55 for t in sub), "chapter subtitle would exceed 240px"
for phrase in ("int k=beat==10?7:beat", "shd_begin()", "shd_end(sh)",
               "oam_hide_all()", "T_BOLT", "wait_release()", "vsync()",
               "HIT(K_A)", "HIT(K_START)", "for(int t=0;t<90;t++)"):
    require(cinema, phrase, "cutscene lacks safe 240x160 drawing, skip or page handling: " + phrase)
assert "malloc(" not in cinema and "calloc(" not in cinema, "chapter slates must not allocate"

# In GBA OBJ attribute 0, bit 14 changes the sprite SHAPE, not color depth.
# Color depth is bit 13. Keep bit 14 clear so Bolt remains square instead of
# being stretched into a horizontal rectangle; palette mode stays unchanged.
sprite_attributes = re.findall(r"shadow_oam\[0\]\.a0\s*=\s*([^;]+);", cinema)
assert len(sprite_attributes) >= 2, "chapter sprite has no initial/animated attr0"
assert all("(1<<14)" not in bits for bits in sprite_attributes), (
    "chapter scene incorrectly sets the horizontal-shape bit on square Bolt"
)
assert "switch(k){" in cinema and all(
    f"case {k}:" in cinema for k in range(8)
), "each chapter should have its own illustrated scene motif"
assert "if((t&7)==0)" in cinema, "chapter twinkle animation is missing"

assert "A: STORY  START: SKIP ALL" in cinema, "cutscene controls no longer explain full skip"
assert "if(HIT(K_START)){ skip=1; break; }" in cinema, "START should skip dialogue too"
assert "if(HIT(K_A)) break;" in cinema, "A should still continue into dialogue"
assert "return skip;" in cinema, "cutscene skip request is not returned to story handler"



scenes = section(ui, "static void bolt_story(", "// Three deliberate breathers")
for beat in [0, 1, 2, 3, 4, 5, 6, 10]:
    require(scenes, f"case {beat}:", f"missing story beat {beat}")
require(scenes, "if(bolt_chapter_open(pg,beat)) return;", "chapter skip not wired into scene progression")
require(ui, "int j=textlen(l2), cap=(int)sizeof l2-2;",
        "Bolt recap needs bounded text copy, not unsafe strcat")
require(ui, "while(*m && j<cap)", "Bolt recap could overflow its fixed 48-byte buffer")

require(scenes, "bugs_freed ? ", "rescued-bug consequence is not responsive to play")
assert all(len(s) <= 54 for s in literals(scenes)), (
    "story dialogue exceeds the existing text-card width"
)
require(scenes, "THE ANT KING FALLS. HIS CROWN BREAKS.", "King's defeat has no scene")
require(scenes, "story_boss_reward[5]", "King ending must acknowledge the liberated infirmary")
require(scenes, "bolt_card(pg,after,4);", "King's reward must appear in the actual ending scene")
for phrase in ("THE WINTER CAME. OUR EGGS WERE FREEZING.",
               "I MADE YOUR FRIENDS DIG FOR BURIED ENGINES.",
               "I CUT OPEN A SHIP TO HEAT THE NEST.",
               "THEN I FED THE FURNACE LIVING SHIPS.",
               "IT LEARNED MY HUNGER."):
    require(scenes, phrase, "Ant King's motive or responsibility is missing: " + phrase)
for companion in ("BUG_ANT", "BUG_LADY", "BUG_FIRE", "BUG_CRICKET", "BUG_BEE"):
    require(scenes, "have_bug[" + companion + "]",
            "saved rescue state must affect an actual chapter: " + companion)
require(scenes, "LADYBUG: REMEMBERING CAN HURT TOO.",
        "Bolt's memory distress needs a companion's response when rescued")
require(scenes, "RUNAWAY ANT: WE NEVER ASKED FOR THIS.",
        "a rescued rebel ant must challenge the King's claim to speak for all ants")
require(scenes, "OPEN CAGES WAIT FOR FRIENDS STILL MISSING.",
        "finale must not pretend the player rescued every captive")
require(scenes, "THE RUNAWAY ANT WALKS HOME WITHOUT FEAR.",
        "the liberated ant companion needs narrative closure")
require(ui, "have_bug[BUG_PILL] && gold<30",
        "the Pill Bug must be acknowledged through a relevant low-gold conversation")
require(ui, "else if(ant_king && data_cores>=8)",
        "recovered memories must guide the player even before the first beast encounter")
require(ui, "OPEN THE MAP. FOLLOW THE BEAST'S SIGNAL.",
        "the objective after recovery must be clear")
require(ui, "HE KNOWS WHERE THE CAPTIVES WERE TAKEN.",
        "Bolt must not promise every friend is in the King's chamber")
assert "THE DAWN" not in scenes.split("case 10:", 1)[0], (
    "King's chapter or earlier story dialogue reveals Bolt's final name prematurely"
)
credits = section(ui, "static void credits(void)", "// ---------------------------------------------------------------- the character screen")
assert "THE DAWN" not in credits, (
    "first credits appear after the Ant King, before the last memories"
)
bolt_zone = section(ui, "static const char *const bolt_zone[6][2]=", "static const char *const bolt_cls")
assert "THE DAWN" not in bolt_zone, (
    "anthill arrival dialogue reveals Bolt's final name prematurely"
)
require(scenes, "THE MOUTH FALLS APART.", "beast conclusion is missing")
require(scenes, "THE FIRST MORNING WITHOUT THE KING.",
        "true Dawn ending needs an epilogue beyond the fight")
require(scenes, 'bugs_freed ? "THE BUGS YOU FREED WATCH THE SUN RISE."',
        "epilogue must reflect actual rescues, not assume every bug was freed")
require(scenes, "WHERE SHOULD WE GO FIRST?",
        "finale should return player agency to the open galaxy")

reactions = section(ui, "static void bolt_memory_reaction(", "// what Bolt says when")
require(reactions, "cores==1", "first recovered memory has no emotional response")
require(reactions, "cores==4", "crew betrayal has no emotional response")
require(reactions, "cores>=8", "complete memory has no identity response")
assert "I AM THE DAWN" not in reactions.split("cores>=8", 1)[0], (
    "Bolt names himself before all eight cores are recovered"
)
require(reactions, "GALAXY MAP: PRESS L TO FIND THE BEAST.",
        "final reveal no longer helps the player continue")

require(main, "if(data_cores==1 || data_cores==4 || data_cores>=8)",
        "memory milestone reactions not connected to outpost clearing")
require(main, "bolt_memory_reaction(VISPG,data_cores)",
        "outpost exit does not play new story reaction")
require(main, "bolt_card(VISPG,bolt_mem[k],4)", "original four-line memory is missing")
require(main, "int new_memory=data_cores<8;",
        "extra outposts must distinguish new memories from post-eight rewards")
require(main, "base_card=new_memory?1:2;",
        "extra outposts must not trigger the eighth memory repeatedly")
require(main, "if(base_card){ int new_memory=base_card==1; base_card=0;",
        "outpost exit must branch on genuine versus repeat memory recovery")
require(main, "BOLT'S EIGHT MEMORIES ARE ALREADY WHOLE.",
        "extra outposts need their own short story rather than replaying the reveal")

zone = section(main, "if(zone_card){ int z=", "planet_hud(); show_msg(\"GO TO YOUR SHIP\")")
assert (
    zone.index("bolt_story(VISPG,1+z)") < zone.index('card(VISPG,"NEXT STAR UNLOCKED"')
), "travel directions interrupt emotional aftermath"
require(main, "FOLLOW THE YELLOW ARROW TO THE DOOR.",
        "opening cutscene must still teach the first objective")

# The central plot must be discovered in actual boss floors, before victory,
# and the resulting first-clear state must change resources and return dialogue.
boss_titles = literals(section(ui, "static const char *const story_boss_title[6]={",
                               "static const char *const story_boss_warning[6]"))
boss_warnings = literals(section(ui, "static const char *const story_boss_warning[6]={",
                                 "static const char *const story_boss_evidence[6][3]"))
boss_evidence = literals(section(ui, "static const char *const story_boss_evidence[6][3]={",
                                 "static const char *const story_boss_aftermath[6]"))
boss_aftermath = literals(section(ui, "static const char *const story_boss_aftermath[6]={",
                                  "static const char *const story_boss_reward[6]"))
boss_reward = literals(section(ui, "static const char *const story_boss_reward[6]={",
                               "static const char *const bolt_zone[6][2]"))
assert len(boss_titles) == len(boss_warnings) == len(boss_aftermath) == len(boss_reward) == 6, (
    "all six story bosses must have a unique title, warning, aftermath, and recovery"
)
assert len(boss_evidence) == 18, "six bosses need three discoverable clues each"
assert all(0 < len(line) <= 54 for line in boss_evidence), "boss intel overflows the story card"
assert all(0 < len(line) < 40 for line in boss_warnings), (
    "boss-floor warning overflows the 39-character gameplay message"
)
assert all(0 < len(line) <= 54 for line in boss_reward + boss_aftermath)
assert all(0 < len(t) <= 54 for t in boss_titles)
for hint in ("CAGE MANIFEST:", "WARM WATER DIVERTED", "RATIONS SENT",
             "SHIP METAL MOVED", "FURNACE REQUIRES A SONG",
             "THE KING LOCKED A HEART"):
    assert any(hint in line for line in boss_evidence), "one of the six warden ledgers is missing: " + hint
assert all("THE DAWN" not in line for line in boss_evidence), (
    "boss-floor intel must not spoil the eighth-memory identity reveal"
)
require(dungeon, "if(last && !endless && !base_mode && !forge_on && !zone_cleared && zone<6)",
        "first-time boss floors are not assigned searchable evidence")
require(dungeon, "if(evidence>=0) dob[evidence].sub=7;",
        "story evidence must reuse ordinary chests to respect NDO limits")
require(dungeon, "else if(ndo<NDO-4)", "evidence chest must leave space for quest objectives")
require(dungeon, "SPAWN(K_CHEST,room_floor_cell(&rooms[nrooms>1?1:0]),1,7);",
        "boss floor has no physical evidence chest fallback")
require(dungeon, "say(story_boss_warning[zone]);", "wardens need a pre-fight gameplay warning")
require(main, "int evidence=o->sub==7", "physical evidence must be distinguished on chest pickup")
require(main, "card(VISPG,story_boss_title[zone],story_boss_evidence[zone],3)",
        "opening evidence must reveal clues in-game instead of only awarding loot")
require(main, "story_boss_reward[z]", "player must be told what resource liberation recovered")
# Boss silhouettes must communicate which warden is present in the actual
# 3D gameplay, reusing existing art with the original collision geometry.
sprite_art = section(dungeon, "static SprArt sprite_art(DObj *o)",
                     "#define DIV9(d)")
boss_art = section(sprite_art, "case K_BOSS:", "case K_CHEST:")
for art in ("art_crab", "beetle_art", "art_ghost", "art_wasp",
            "art_ant", "spider_art"):
    require(boss_art, art, "one of the six warden silhouettes is missing: " + art)
require(boss_art, "if(!endless && !forge_on && !base_mode && zone>=0 && zone<6)",
        "story boss silhouettes must not alter forge or endless guardians")
require(boss_art, "hw=0x11000;", "all six boss silhouettes must preserve projected height")
require(main, "story_boss_title[zone]:\"THE QUEEN\"",
        "live boss HUD must show the correct warden name")

require(ui, "t[n++]=story_boss_aftermath[zone];",
        "returning to a liberated world should acknowledge the defeated warden")
require(space, "int first=!(cleared_mask&(1<<zone))",
        "boss supply recovery must be driven by legacy-compatible saved progression")
for reward in ("case 0: food=100", "case 1: { int fm=fuel_max()",
               "case 2: earn(150)", "case 3: if(hull<100)",
               "case 4: scrap+=24", "case 5: hearts=maxh"):
    require(space, reward, "boss first-clear reward is not wired to gameplay: " + reward)


first_rescue = section(game, "static const char *const first_rescue[6]={",
                       'char b[40]; scpy(b,"FREED A ");')
rescues = literals(first_rescue)
assert len(rescues) == 6, "every rescuable bug needs a first-meeting voice"
assert all(0 < len(s) <= 39 for s in rescues), "rescue feedback exceeds the in-game message"
for name in ("PILL BUG:", "LADYBUG:", "FIREFLY:", "BEE:", "CRICKET:", "RUNAWAY ANT:"):
    assert any(s.startswith(name) for s in rescues), f"{name} has no character voice"
require(game, "say(have_bug[t]==1?first_rescue[t]:b)", "first-rescue words not shown in gameplay")
require(game, "q_type==Q_RESCUE && !q_done", "narrative must not break rescue quest progress")

require(space, "if(beast_finale_ready(data_cores,dawn_free))",
        "true ending must depend on recovered cores and completion, not previous kills")
require(space, "bolt_story(VISPG,10); dawn_free=1;",
        "true ending must play and record completion")
require(space, "b.stage=(u16)(beast_stage|(dawn_free?0x8000u:0))",
        "true ending must persist through legacy-compatible save layout")
require(space, "dawn_free=(b.stage&0x8000u)!=0",
        "old and new save records must decode story completion")
require(ui, 'if(dawn_free){ t[n++]=', "post-finale Bolt must not demand another boss victory")
require(space, 'story_card("VICTORY",t,4)', "partial-victory ending must remain distinct")
require(space, "RECOVER ALL 8 OF BOLT'S MEMORIES.",
        "partial victory must explain how to unlock the real ending")
require(space, 'space_say(dawn_free ? "DAWN IS FREE. THE GALAXY IS OPEN."',
        "post-finale warp must not tell the player to return for a boss fight")
assert len("DAWN IS FREE. THE GALAXY IS OPEN.") < 40, (
    "post-finale warp banner must fit the existing 39-character message buffer"
)
require(space, "do_save(); }", "post-finale warp must save Dawn completion")

require(space, 'text(pg,78,124,"A/START: CONTINUE",55,1)',
        "beast cutscenes must describe their confirm/skip inputs")
require(space, 'while(!HIT(K_A)&&!HIT(K_START))',
        "Technobeast story cards need START support as well as A")

for phrase in ("FIVE TECH HIVES", "WARP OUT WHEN NEEDED", "HOLD B TO THRUST",
               "HOLD L TOO FOR EXTRA BURN"):
    require(space, phrase, "boss encounter lacks real gameplay guidance: " + phrase)


# At least one in five neutral NPCs should acknowledge the journey, while
# love/hate moods retain the original distinctive mood-specific speeches.
rumors = literals(section(town, "static const char *const story_rumor[5][3]={",
                          "static const char *const bolt_quip[12]"))
assert len(rumors) == 15, "expected three grounded rumors for five story phases"
assert all(0 < len(line) <= 54 for line in rumors), "town dialog exceeds native card width"

assert "THE ANT TUNNELS FROZE LAST WINTER." in rumors[:3], (
    "the King's winter fear must be foreshadowed before his confession"
)
assert "NOT EVERY ANT AGREED WITH THE KING." in rumors[3:6], (
    "the story should distinguish captive workers from the King"
)
assert "SOMEONE IN SPACE IS SINGING US HOME." not in rumors[:12], (
    "townsfolk must not announce Dawn's liberation before the true finale"
)
assert "THE OLD SHIP'S HEART IS STILL LOCKED AWAY." in rumors[9:12], (
    "eight-memory townsfolk need to acknowledge the remaining boss objective"
)
assert "SOMEONE IN SPACE IS SINGING US HOME." in rumors[12:], (
    "post-finale townsfolk should celebrate the ending"
)
for phrase in ("dawn_free?4:data_cores>=8?3:ant_king?2:bugs_freed>0?1:0",
               "wd.mood!=MOOD_LOVE && wd.mood!=MOOD_HATE",
               "ln[0]=story_rumor[stage][(li/5)%3]",
               "(u32)(rumor_seed%NTALK)"):
    if phrase == "(u32)(rumor_seed%NTALK)":
        continue
    require(town, phrase, "missing story-aware local dialogue: " + phrase)
require(town, "int li=(int)(rumor_seed%NTALK)", "town dialogue index must use unsigned modulo")
require(town, "wd.mood==MOOD_LOVE ? love_ln[li%12]", "love-mood speech overridden by rumor")
require(town, "wd.mood==MOOD_HATE ? hate_ln[li%12]", "hate-mood speech overridden by rumor")

print("Story/cutscene audit: 8 memories, 8 intertitles, rescued companion dialogue,")
print("character reactions, reveal gating, scene order and action hints: PASS")
