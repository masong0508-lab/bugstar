#!/usr/bin/env python3
"""Guard the real Kart World fuzz driver's fade-in against black race captures.

The PC fuzz test skips the usual Kart World menu, but it must still drive
kg_fadein exactly as kw_run() does. Otherwise the GBA-style scanline path
keeps the boot BLDY=16 darkening effect and all classic frames go black.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fuzz = (root / "pc/tests/kart_fuzz.inc").read_text()
kart = (root / "kart3.inc").read_text()
main = (root / "main.c").read_text()
kart_core = (root / "kart.inc").read_text()
fade_step = "if(kg_fadein && !--kg_fadein) fade(0);"
assert fade_step in kart, "The real Kart World race fade has changed"
assert fuzz.count(fade_step) == 1, "The fuzz loop must run the same fade-in once"
assert fuzz.index("planet_frame(); vsync();") < fuzz.index(fade_step) < fuzz.index(
    "if(kr_mode!=1 || nrc<1) continue;"
), "The fuzz fade must follow rendering and precede race-state checks"
assert "kg_fadein=4;" in kart_core, "Kart race startup no longer primes the fade"
assert "REG_BLDCNT=0xFF; REG_BLDY=16;" in main, "Boot black fade assumption changed"
assert "if(((*(volatile u16*)0x04000050>>6)&3)!=3)" in (root / "m7.inc").read_text(), (
    "Mode-4 renderer fade compatibility changed; reconsider this regression"
)

m = re.search(r"static void fade\(int out\)\s*\{", main)
if not m:
    raise SystemExit("Missing production fade(int) function")
depth = 0
for i in range(main.index("{", m.start()), len(main)):
    if main[i] == "{":
        depth += 1
    elif main[i] == "}":
        depth -= 1
        if depth == 0:
            real_fade = main[m.start():i+1]
            break
else:
    raise SystemExit("Unclosed fade(int) function")

program = r"""
#include <stdio.h>
#include <stdint.h>
static int scr_black, kg_fadein, fades, blanks;
static unsigned REG_BLDCNT, REG_BLDY;
static void vsync(void) { ++blanks; }
""" + real_fade + r"""
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fade assertion line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static void test_frame(void) {
    if(kg_fadein && !--kg_fadein) fade(0);
}
int main(void) {
    for (int delay=1; delay<=8; ++delay) {
        REG_BLDCNT=0xFF; REG_BLDY=16; scr_black=1; kg_fadein=delay;
        int before=blanks;
        for (int f=1; f<delay; ++f) {
            test_frame();
            CHECK(REG_BLDY==16);
            CHECK(scr_black);
        }
        test_frame();
        CHECK(REG_BLDCNT==0 && REG_BLDY==0 && !scr_black && !kg_fadein);
        CHECK(blanks-before==18);
        for (int f=0; f<20; ++f) {
            int old=blanks;
            test_frame();
            CHECK(blanks==old && !scr_black);
        }
        // A subsequent race also starts faded to black: same cycle again.
        fade(1);
        CHECK(scr_black && REG_BLDY==16);
        kg_fadein=delay;
        for (int f=0; f<delay; ++f) test_frame();
        CHECK(REG_BLDCNT==0 && REG_BLDY==0 && !scr_black && !kg_fadein);
    }
    CHECK(fades==0);
    printf("Kart fuzz/real-race fade parity: 16 race transitions PASS\n");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="bugstar-fade-") as td:
    exe = str(Path(td) / "kart-fade")
    subprocess.run(
        ["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=undefined", "-fno-sanitize-recover=all",
         "-x", "c", "-", "-o", exe],
        input=program, text=True, check=True,
    )
    subprocess.run([exe], check=True)
