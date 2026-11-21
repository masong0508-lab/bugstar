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

# The OAM bit 14 on attribute 0 switches to 8bpp. Bolt's original sprites
# are 4bpp; the new chapter scene must preserve this rather than scrambling
# its pixels/palette on GBA hardware.
sprite_attributes = re.findall(r"shadow_oam\[0\]\.a0\s*=\s*([^;]+);", cinema)
assert len(sprite_attributes) >= 2, "chapter sprite has no initial/animated attr0"
assert all("(1<<14)" not in bits for bits in sprite_attributes), (
    "chapter scene incorrectly sets the 8bpp sprite bit for 4bpp Bolt artwork"
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
require(scenes, "I CUT OPEN A SHIP TO FEED THEM.", "King's final confession is lost")
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
require(scenes, '"WHERE SHOULD WE GO FIRST?"',
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
zone = section(main, "if(zone_card){ int z=", "planet_hud(); show_msg(\"GO TO YOUR SHIP\")")
assert zone.index("bolt_story(VISPG,1+z)") < zone.index(
    'card(VISPG,"NEXT STAR UNLOCKED"'), "travel directions interrupt emotional aftermath"
)
require(main, "FOLLOW THE YELLOW ARROW TO THE DOOR.",
        "opening cutscene must still teach the first objective")

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
require(space, 'text(pg,78,124,"A/START: CONTINUE",55,1)',
        "beast cutscenes must describe their confirm/skip inputs")
require(space, 'while(!HIT(K_A)&&!HIT(K_START))',
        "Technobeast story cards need START support as well as A")

for phrase in ("FIVE TECH HIVES", "WARP OUT WHEN NEEDED", "HOLD B TO THRUST",
               "HOLD L TOO FOR EXTRA BURN"):
    require(space, phrase, "boss encounter lacks real gameplay guidance: " + phrase)


# At least one in five neutral NPCs should acknowledge the journey, while
# love/hate moods retain the original distinctive mood-specific speeches.
rumors = literals(section(town, "static const char *const story_rumor[4][3]={",
                          "static const char *const bolt_quip[12]"))
assert len(rumors) == 12, "expected three grounded rumors for each of four story phases"
assert all(0 < len(line) <= 54 for line in rumors), "town dialog exceeds native card width"
for phrase in ("data_cores>=8?3:ant_king?2:bugs_freed>0?1:0",
               "wd.mood!=MOOD_LOVE && wd.mood!=MOOD_HATE",
               "ln[0]=story_rumor[stage][(li/5)%3]",
               "(u32)(rumor_seed%NTALK)"):
    if phrase == "(u32)(rumor_seed%NTALK)":
        continue
    require(town, phrase, "missing story-aware local dialogue: " + phrase)
require(town, "int li=(int)(rumor_seed%NTALK)", "town dialogue index must use unsigned modulo")
require(town, "wd.mood==MOOD_LOVE ? love_ln[li%12]", "love-mood speech overridden by rumor")
require(town, "wd.mood==MOOD_HATE ? hate_ln[li%12]", "hate-mood speech overridden by rumor")

print("Story/cutscene audit: 8 memories, 8 intertitles, conditional rescue voices,")
print("character reactions, reveal gating, scene order and action hints: PASS")
