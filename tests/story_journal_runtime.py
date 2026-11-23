#!/usr/bin/env python3
"""Run the actual Bugstar STORY SO FAR journal under ASan/UBSan.

The QUEST-page A toggle shows campaign progress without corrupting an
existing quest, revealing Bolt's identity before the right time or adding
persistent state. Use production C route, hive-name, text and status helpers.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
read=lambda path:(ROOT/path).read_text()
ui=read("ui.inc");space=read("space.inc");game=read("game.inc");main=read("main.c")

def function(source,needle):
    pat=re.search(re.escape(needle)+r"[^;{}]*\{",source)
    assert pat, f"missing definition of {needle}"
    beg=source.index("{",pat.start());depth=1;end=beg+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    return source[pat.start():end]

def array(source,needle):
    start=source.index(needle);beg=source.index("{",start);depth=1;end=beg+1
    while depth:
        depth+=(source[end]=="{")-(source[end]=="}")
        end+=1
    assert source[end]==";"
    return source[start:end+1]

titles=array(ui,"static const char *const story_chapter[8]")
lines=array(ui,"static const char *const story_journal_words[8][4]")
hive_names=array(space,"static const char *const hive_nm[5]")
hive_floors=array(space,"static const u8 hive_floors[5]")
route=function(space,"static int bolt_next_step(")
hive=function(space,"static void bolt_hive_hint(")
journal_line=function(ui,"static const char *story_journal_line(")
detail=function(ui,"static void story_journal_detail(")
texts="\n".join(function(game,f"static void {name}(")
                for name in ("txt_copy","txt_append","txt_number"))

# Ground each line in what the game can actually unlock at that phase.
all_titles=re.findall(r'"([^"\\]*)"',titles)
all_lines=re.findall(r'"([^"\\]*)"',lines)
assert len(all_titles)==8 and len(all_lines)==32
assert all(1<=len(x)<40 for x in all_titles)
assert all(10<=len(x)<=52 for x in all_lines), "240-wide journal line clipped"
assert all("DAWN" not in x for x in all_titles[:6]+all_lines[:24]), (
    "the early storyline reveals Bolt's eighth-core identity")
assert "THE MOUTH IS BROKEN. DAWN FLIES AGAIN." in all_lines[28:]
assert "THE ANT KING" in " ".join(all_lines[:12])
assert "FIVE TECH HIVES" in " ".join(all_lines[16:20])
assert "RED HATCHES" in " ".join(all_lines[20:24])
assert "GLOWING CORE" in " ".join(all_lines[24:28])

# This must be an existing QUEST tab's optional second view, not a new menu
# or an always-on card that adds frame/heap work to GBA's tight RAM budget.
pause=ui[ui.index("static void char_screen_("):ui.index("// ----------------------------------------------------------------",ui.index("static void char_screen_(")+5)]
assert 'if(page==3 || page==4){ sub^=1; sfx(2); }' in pause
assert 'if(sub){' in pause and 'story_chapter[stage]' in pause
assert 'story_journal_detail(b,sizeof b,stage)' in pause
assert 'story_journal_line(stage,i)' in pause
assert 'foot="A: STORY RECAP";' in pause
assert 'foot="A: FIELD NOTES";' in pause
assert 'WHAT YOU HAVE LEARNED' in pause
assert 'if(q_type==Q_NONE)' in pause and 'quest_refresh();' in pause
assert "static int bolt_next_step(void);" in main
assert "static void bolt_hive_hint(char *out, int cap);" in main

stub=r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
static int ant_king,endless,zone_cleared,zone,beast_stage,hive_done,data_cores,dawn_free;
static const char *const tint_name[6]={"GARDEN","POND","PICNIC","NIGHT LAWN","HONEY","ANTHILL"};
#define CHECK(x) do {if(!(x)){fprintf(stderr,"Story journal line %d: %s\n",__LINE__,#x);return 1;}}while(0)
"""
cases=r"""
int main(void){
    int nstates=0;
    // Invalid index must not read the line table.
    CHECK(!strcmp(story_journal_line(-1,0),""));
    CHECK(!strcmp(story_journal_line(8,0),""));
    CHECK(!strcmp(story_journal_line(0,4),""));
    for(int stage=0;stage<8;stage++){
        for(int row=0;row<4;row++){
            const char *line=story_journal_line(stage,row);
            CHECK(strlen(line)>=10 && strlen(line)<=52);
        }
    }
    // No spoilers when viewing the recap early in the game.
    for(int stage=0;stage<6;stage++)for(int row=0;row<4;row++)
        CHECK(!strstr(story_journal_line(stage,row),"DAWN"));

    for(int king=0;king<2;king++)
    for(int z=0;z<6;z++)
    for(int clear=0;clear<2;clear++)
    for(int end=0;end<2;end++)
    for(int beast=0;beast<4;beast++)
    for(int h=0;h<=5;h++)
    for(int cores=0;cores<=8;cores++)
    for(int freed=0;freed<2;freed++){
        ant_king=king;zone=z;zone_cleared=clear;endless=end;
        beast_stage=beast;hive_done=h;data_cores=cores;dawn_free=freed;
        int stage=bolt_next_step();
        CHECK(stage>=0 && stage<8);
        // Keep canaries on both sides of the existing menu's output.
        struct {u8 front;char buf[64];u8 back;} out;
        memset(&out,0xa5,sizeof out);out.front=0x44;out.back=0xdd;
        story_journal_detail(out.buf,64,stage);
        CHECK(out.front==0x44 && out.back==0xdd);
        CHECK(strlen(out.buf)<64 && strlen(out.buf)>0);
        if(stage==4){
            char target[16];snprintf(target,sizeof target,"HIVE %u/5",(unsigned)(h+1));
            CHECK(strstr(out.buf,target));
            CHECK(strstr(out.buf,hive_nm[h]));
            char floor[8];snprintf(floor,sizeof floor," B%u",(unsigned)hive_floors[h]);
            CHECK(strstr(out.buf,floor));
        }
        if(stage==5){
            char target[16];snprintf(target,sizeof target,"%u/8",(unsigned)cores);
            CHECK(strstr(out.buf,target));
        }
        if(stage==1)CHECK(strstr(out.buf,tint_name[z+1]));
        if(stage==0)CHECK(strstr(out.buf,tint_name[z]));
        nstates++;
    }
    // The bounded helper must terminate even a one-byte buffer and
    // never overwrite a second byte while displaying a long hive name.
    for(int stage=0;stage<8;stage++)for(int cap=1;cap<=12;cap++){
        struct {u8 l;char b[12];u8 r;} x;
        memset(&x,0x7e,sizeof x);
        x.l=0x22;x.r=0x33;
        story_journal_detail(x.b,cap,stage);
        CHECK(x.l==0x22 && x.r==0x33);
        CHECK(x.b[cap-1]==0 || memchr(x.b,0,cap)!=NULL);
        for(int i=cap;i<12;i++)CHECK((unsigned char)x.b[i]==0x7e);
    }
    printf("Story journal: %d narrative states, 32 spoiler-safe lines, 8 chapters, hive floors and guarded output PASS\n",nstates);
    return 0;
}
"""

with tempfile.TemporaryDirectory(prefix="bugstar-story-journal-") as td:
    source=Path(td)/"journal.c";exe=Path(td)/"journal"
    source.write_text(stub+"\n"+hive_names+"\n"+hive_floors+"\n"+
                      titles+"\n"+lines+"\n"+texts+"\n"+route+"\n"+hive+
                      "\n"+journal_line+"\n"+detail+"\n"+cases)
    subprocess.run(["cc","-std=c11","-O1","-Wall","-Wextra",
                    "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                    "-fno-omit-frame-pointer",str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
