#!/bin/bash
# pc/build.sh: build the PC (native, headless) version of the GBA game's own code.
#   ./pc/build.sh            -> pc/bugstar_pc
# It copies the game's sources into pc/gen (not committed), patches the GBA-only parts with sed (list below), and compiles them
# with the fake GBA in pc_hw.c. The game's source in the repo root is never edited by this.
set -e
cd "$(dirname "$0")"; PCD=$(pwd); ROOT=$(dirname "$PCD")
# assets.h and facts.bin come from the same generators as the GBA build, run in a scratch copy (pc/genroot, not committed) with BUGSTAR_PC=1:
# that adds the PC version's own HOW BIG IS IT? pages (pc/facts/*.py). The repo root's assets.h and facts.bin are never touched.
rm -rf genroot; mkdir -p genroot/pc; cp "$ROOT"/*.py genroot/; cp -r facts genroot/pc/facts; rm -rf genroot/pc/facts/__pycache__
( cd genroot && BUGSTAR_PC=1 python3 gen.py >/dev/null )
mkdir -p gen; rm -f gen/*.inc gen/*.c gen/*.h
cp "$ROOT"/*.inc "$ROOT"/main.c "$ROOT"/logo.h gen/; cp genroot/assets.h genroot/facts.bin gen/
cd gen
# --- the patches -------------------------------------------------------------------------------------------------------
sed -i \
 -e 's/__attribute__((section("\.[ie]wram")))//g' \
 -e 's/__attribute__((section("\.iwram"), long_call, target("arm"), noinline))/__attribute__((noinline))/' \
 -e 's|#define REG_VCOUNT   (\*(volatile u16\*)0x04000006)|#define REG_VCOUNT   pc_vcount()|' \
 -e '/^void \*memset(void \*d, int c, unsigned n)/d' -e '/^void \*memcpy(void \*d, const void \*s, unsigned n)/d' \
 -e 's/__asm__ volatile("mov %0, sp":"=r"(\([a-z_]*\)))/\1=pc_sp()/g' \
 -e 's/extern u32 __bss_end; //g' -e 's/&__bss_end/((u32*)0x03000000)/g' \
 -e 's/\*(volatile u32\*)0x03007FFC=(u32)irq_handler;/pc_irq_fn=irq_handler;/' \
 -e 's/\*(volatile u32\*)0x040000DC=\([^;]*\);/pc_dma3(\1);/g' \
 -e 's/^int main(void){/int bugstar_main(void){/' \
 *.inc main.c
# the HOW BIG screen sorts every page in a scratch area: the GBA keeps it in the world builder's 32 KB; the PC has far more pages, so it gets its own
sed -i -e 's|^#define SZ_SCR .*|#define SZ_SCR pc_szscr|' -e '/^#define NSIZE /a static u32 pc_szscr[4*NSIZE+64];' sizes.inc
sed -i '1i #include "pc_hw.h"' main.c
# the PC content library is a separate translation unit (pc/content/pcx.c): the game's own code has names like trait_name, so it only sees pcx.h
sed -i -e 's|^static void world_page(volatile u16 \*pg, int y); |static void pc_world_screen(volatile u16 *pg); &|' \
       -e 's|^\( *\)if(HIT(K_SEL) \&\& page==7){|\1if(HIT(K_B) \&\& page==7){ pc_world_screen(pg); dirty=1; }\n&|' \
       -e 's|foot = big_unlocked() ? "A: FAVOURITE  SEL: HOW BIG IS IT?" : "A: SAVE AS A FAVOURITE";|foot = big_unlocked() ? "A: FAV  SEL: HOW BIG  B: PC FILE" : "A: FAVOURITE  B: PC FILE";|' ui.inc
# traits really change the endless galaxy's planets (gravity, light, weather); the six story worlds keep their own rules
sed -i '1i static void pc_apply_traits(void); static int pc_applied;' game.inc
sed -i 's|^\( *\)// a weird world: one planet in 111, only out in the galaxy.*|\1pc_applied=0; if(gal \&\& !hellw() \&\& !cur_moon) pc_apply_traits();\n&|' game.inc
echo '#include "../pc_info.inc"' >> main.c
echo '#include "../pc_view.inc"' >> main.c
# --- compile -----------------------------------------------------------------------------------------------------------
# -funsigned-char: the GBA compiler's char is unsigned. -fwrapv: ARM integers wrap. -no-pie: all addresses stay below 4 GB.
gcc -std=gnu11 -O1 -w -funsigned-char -fwrapv -fno-strict-aliasing -no-pie -m64 -DNO_LOGO ${PC_CFLAGS} \
    -I. -I.. -Wa,-I,. ../pc_main.c ../pc_hw.c ../content/pc_content.c ../content/pcx.c main.c -lm -o ../bugstar_pc
echo "built pc/bugstar_pc"
