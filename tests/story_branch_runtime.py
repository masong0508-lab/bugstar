#!/usr/bin/env python3
"""Exercise the game's actual Bolt story handler with rescued-crew combinations.

The GBA C scene is extracted and compiled unmodified into a minimal native
host harness. Graphics and controls are substituted, but every line choice
and card transition is evaluated by the original scene code. No global save
format, image asset, gameplay-loop or framebuffer modifications.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
ui = (root / "ui.inc").read_text()
signature = "static void bolt_story(volatile u16 *pg, int beat)"
start = ui.index(signature + "{")
begin = ui.index("{", start)
depth = 0
end = None
for i in range(begin, len(ui)):
    if ui[i] == "{":
        depth += 1
    elif ui[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break
assert end is not None, "unbalanced bolt_story C handler"
scene = ui[start:end]
# The scene now correctly references the boss reward ROM table. Include that
# real source table in this standalone C harness instead of using a mock.
reward_start = ui.index("static const char *const story_boss_reward[6]={")
reward_end = ui.index("};", reward_start) + 2
rewards = ui[reward_start:reward_end]
assert "PRISON INFIRMARY: HEALTH RESTORED." in rewards

prelude = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint16_t u16;
enum {BUG_PILL,BUG_LADY,BUG_FIRE,BUG_BEE,BUG_CRICKET,BUG_ANT};
static int have_bug[6], bugs_freed, skip_all;
static char cards[4][8][64];
static int card_sizes[4], card_count, bad_card;
#define CHECK(e) do { if(!(e)){fprintf(stderr,"story branch line %d: %s\n",__LINE__,#e);return 1;} } while(0)

static int bolt_chapter_open(volatile u16 *pg,int beat){
    (void)pg;(void)beat;return skip_all;
}
static void bolt_card(volatile u16 *pg,const char *const *ln,int n){
    (void)pg;
    if(card_count >= 4 || n<1 || n>8){bad_card++;return;}
    card_sizes[card_count]=n;
    for(int j=0;j<n;j++){
        if(strlen(ln[j])>54){bad_card++;continue;}
        snprintf(cards[card_count][j],sizeof(cards[0][0]),"%s",ln[j]);
    }
    card_count++;
}
static void reset_story(void){
    memset(cards,0,sizeof cards);
    memset(card_sizes,0,sizeof card_sizes);
    memset(have_bug,0,sizeof have_bug);
    card_count=bad_card=bugs_freed=skip_all=0;
}
static int contains(const char *needle){
    for(int c=0;c<card_count;c++) for(int l=0;l<card_sizes[c];l++)
        if(strstr(cards[c][l],needle)) return 1;
    return 0;
}
"""
checks = r"""
int main(void){
    // Opening works without rescuing anyone and preserves its stakes.
    reset_story(); bolt_story(0,0);
    CHECK(!bad_card && card_count==1 && contains("YOUR FRIENDS ARE DOWN THERE."));

    // All rescued allies are optional. No cameo for an unrescued bug.
    reset_story(); bolt_story(0,1);
    CHECK(!bad_card && card_count==1);
    CHECK(contains("EVERY ARROW POINTS TOWARD THE ANTHILL."));
    CHECK(!contains("RUNAWAY ANT:"));
    have_bug[BUG_ANT]=1; card_count=0; bolt_story(0,1);
    CHECK(contains("RUNAWAY ANT: THE WORKERS HID A PASSAGE."));
    CHECK(!contains("EVERY ARROW POINTS TOWARD THE ANTHILL."));

    reset_story(); bolt_story(0,2);
    CHECK(card_count==1 && !contains("LADYBUG:"));
    have_bug[BUG_LADY]=1; card_count=0; bolt_story(0,2);
    CHECK(card_count==2 && card_sizes[1]==2);
    CHECK(contains("LADYBUG: REMEMBERING CAN HURT TOO."));

    reset_story(); bolt_story(0,3);
    CHECK(contains("ITS METAL SKIN SHIFTS"));
    have_bug[BUG_FIRE]=1; card_count=0; bolt_story(0,3);
    CHECK(contains("THE FIREFLY'S LIGHT GOES OUT"));
    CHECK(!contains("ITS METAL SKIN SHIFTS"));

    reset_story(); bolt_story(0,4);
    CHECK(contains("THEY BUILD OUTPOSTS"));
    have_bug[BUG_ANT]=1; card_count=0; bolt_story(0,4);
    CHECK(contains("RUNAWAY ANT: I SAW THEM SEAL THE HATCHES."));
    have_bug[BUG_CRICKET]=1; card_count=0; bolt_story(0,4);
    CHECK(contains("CRICKET: THEY MADE US MARCH TO THAT TUNE."));
    CHECK(!contains("THE SAME TUNE I HUM WHEN YOU SLEEP."));

    reset_story(); bolt_story(0,5);
    CHECK(contains("BOLT STOPS ROLLING. HE KNOWS THE TUNE."));
    have_bug[BUG_BEE]=1; card_count=0; bolt_story(0,5);
    CHECK(contains("THE RESCUED BEE SINGS. BOLT STOPS ROLLING."));
    CHECK(!contains("BOLT STOPS ROLLING. HE KNOWS THE TUNE."));

    // The King's culpability is constant; the rebel ant responds only when saved.
    reset_story(); bolt_story(0,6);
    CHECK(card_count==2 && contains("OUR EGGS WERE FREEZING."));
    CHECK(contains("I MADE YOUR FRIENDS DIG FOR BURIED ENGINES."));
    CHECK(contains("IT LEARNED MY HUNGER."));
    CHECK(contains("BOLT WATCHES THE EMPTY THRONE."));
    CHECK(!contains("RUNAWAY ANT:"));
    have_bug[BUG_ANT]=1; card_count=0; bolt_story(0,6);
    CHECK(card_count==2 && contains("RUNAWAY ANT: WE NEVER ASKED FOR THIS."));
    CHECK(!contains("BOLT WATCHES THE EMPTY THRONE."));

    // True ending responds to actual rescue progress, not a universal ending.
    reset_story(); bolt_story(0,10);
    CHECK(card_count==2 && card_sizes[1]==6);
    CHECK(contains("OPEN CAGES WAIT FOR FRIENDS STILL MISSING."));
    CHECK(!contains("THE BUGS YOU FREED WATCH THE SUN RISE."));
    CHECK(contains("THE LAST ROYAL BANNERS COME DOWN."));
    bugs_freed=3; have_bug[BUG_ANT]=1; card_count=0; bolt_story(0,10);
    CHECK(contains("THE BUGS YOU FREED WATCH THE SUN RISE."));
    CHECK(contains("THE RUNAWAY ANT WALKS HOME WITHOUT FEAR."));
    CHECK(!contains("OPEN CAGES WAIT FOR FRIENDS STILL MISSING."));

    // All seven pre-finale scenes must avoid naming Dawn early.
    for(int beat=0;beat<7;beat++){
        reset_story(); bolt_story(0,beat);
        CHECK(!contains("THE DAWN") && !bad_card);
    }

    // START must skip the complete chapter dialogue, not change story flags.
    reset_story(); skip_all=1; have_bug[BUG_ANT]=1; bugs_freed=6;
    bolt_story(0,6); bolt_story(0,10);
    CHECK(card_count==0 && !bad_card && have_bug[BUG_ANT]==1 && bugs_freed==6);

    puts("Story branch runtime: rescue cameos, King accountability, true ending, scene width, START skip PASS");
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-branch-runtime-") as tmp:
    source = Path(tmp) / "story_branch_runtime.c"
    executable = Path(tmp) / "story_branch_runtime"
    source.write_text(prelude + "\n" + rewards + "\n" + scene + "\n" + checks)
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    str(source), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
