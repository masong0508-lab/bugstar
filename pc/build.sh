#!/bin/bash
# pc/build.sh: build the PC (native, headless) version of the GBA game's own code.
#   ./pc/build.sh            -> pc/bugstar_pc
# It copies the game's sources into pc/gen (not committed), patches the GBA-only parts with sed (list below), and compiles them
# with the fake GBA in pc_hw.c. The game's source in the repo root is never edited by this.
set -e
cd "$(dirname "$0")"; PCD=$(pwd); ROOT=$(dirname "$PCD")
# assets.h and facts.bin come from the same generators as the GBA build, run in a scratch copy (pc/genroot, not committed) with BUGSTAR_PC=1:
# that adds the PC version's own HOW BIG IS IT? pages (pc/facts/*.py). The repo root's assets.h and facts.bin are never touched.
rm -rf genroot; mkdir -p genroot/pc; cp "$ROOT"/*.py genroot/; cp -r facts genroot/pc/facts; rm -rf genroot/pc/facts/__pycache__; mkdir -p genroot/pc/laws; cp laws/laws_def.h laws/laws.c genroot/pc/laws/     # (the laws pages read the law table)
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
 -e 's/SND(\([^)]*\))=\([^;]*\);/pc_snd(\1,\2);/g' \
 -e 's|\*(volatile u16\*)(0x04000090+i\*2)=wave\[i\];|pc_snd(0x30+i*2,wave[i]);|' \
 *.inc main.c
# the HOW BIG screen sorts every page in a scratch area: the GBA keeps it in the world builder's 32 KB; the PC has far more pages, so it gets its own
sed -i -e 's|^extern const u8 fact_bin\[\];|extern const u8 fact_bin[] __asm__("fact_bin");|' sizes.inc
sed -i -e 's|^#define SZ_SCR .*|#define SZ_SCR pc_szscr|' -e '/^#define NSIZE /a static u32 pc_szscr[4*NSIZE+64];' sizes.inc
sed -i '1i #include "pc_hw.h"' main.c
# the PC content library is a separate translation unit (pc/content/pcx.c): the game's own code has names like trait_name, so it only sees pcx.h
sed -i -e 's|^static void world_page(volatile u16 \*pg, int y); |static void pc_world_screen(volatile u16 *pg), pc_lab_screen(volatile u16 *pg); &|' \
       -e 's|^\( *\)if(HIT(K_SEL) \&\& page==7){|\1if(HIT(K_B) \&\& page==7){ pc_lab_screen(pg); dirty=1; if(sys_req) break; }\n&|' \
       -e 's|foot = big_unlocked() ? "A: FAVOURITE  SEL: HOW BIG IS IT?" : "A: SAVE AS A FAVOURITE";|foot = big_unlocked() ? "A: FAV  SEL: HOW BIG  B: PC LAB" : "A: FAVOURITE  B: PC LAB";|' ui.inc
# traits really change the endless galaxy's planets (gravity, light, weather); the six story worlds keep their own rules
sed -i '1i static void pc_apply_traits(void); static int pc_applied;' game.inc
sed -i 's|^\( *\)// a weird world: one planet in 111, only out in the galaxy.*|\1pc_applied=0; if(gal \&\& !hellw() \&\& !cur_moon) pc_apply_traits();\n&|' game.inc
# --- the UNIVERSE LAWS (pc/laws, pc/pc_laws.inc): hooks. At the default laws every hook returns the game's own number (tests/seeds.golden proves it).
# the world wraps every pc_tw cells (128; 64, 32, 16 or 8 on tiny worlds), the horizon bend is in sixteenths (pc_k16 = -1: the size class decides)
sed -i -e 's/(TW<<16)-1/(pc_tw<<16)-1/g' *.inc
sed -i 's|^static s32 wrapd(s32 d){ return ((d + (64<<16)) \& ((pc_tw<<16)-1)) - (64<<16); }|static s32 wrapd(s32 d){ s32 hf=(pc_tw/2)<<16; return ((d + hf) \& ((pc_tw<<16)-1)) - hf; }|' planet.inc
sed -i '1i static int pc_tw=128, pc_k16=-1, pc_slow=0, pc_day_pct=100, pc_hun_pct=100, pc_node[4], pc_water_pct=100;\nstatic int pc_nplan(int i,int o), pc_nmoon(int i,int p,int o), pc_dense(u32 r,int odds), pc_hell(int r,int hp), pc_weird(u32 f), pc_uly(void), pc_galbits(void), pc_warp(int c), pc_danger(int i);\nstatic u32 pc_hours(u32 h);\nstatic void pc_laws_dna(int story), pc_tiny_hmap(void), pc_tiny_fix(void), pc_laws_slot_load(int s), pc_laws_slot_save(int s), pc_laws_slot_new(int s), pc_laws_slot_erase(int s), pc_laws_slot_copy(int a,int b); static int pc_water(int w);' game.inc
sed -i -e 's/^    wd\.psz=2;$/    wd.psz=2; pc_laws_dna(1);/' \
       -e 's/^    if(cur_rock){ wlv\[MW_ZEROG\]=6; wd\.hour=2; }/    pc_laws_dna(0);\n&/' \
       -e 's/(int)(hh%100u)<hp/pc_hell((int)(hh%100u),hp)/' \
       -e 's/((F%111u)==0 \&\& gal)/(pc_weird(F) \&\& gal)/' \
       -e 's/^    every=every\*100\/xo_v(XO_HUNGER); if(every<1) every=1;/&\n    if(pc_hun_pct!=100){ if(pc_hun_pct<=0) return; every=every*100\/pc_hun_pct; if(every<1) every=1; }/' game.inc
sed -i -e 's/^#define ULY 25 .*/#define ULY pc_uly()/' -e 's/^#define GAL_SHIFT 20 .*/#define GAL_SHIFT pc_galbits()/' \
       -e 's/{ return 20+3\*digits; }/{ return pc_warp(20+3*digits); }/' \
       -e 's/return hmix(h^0xD3A5E000u^gsk(110))%DENSE_ODDS==0; }/return pc_dense(hmix(h^0xD3A5E000u^gsk(110)),DENSE_ODDS); }/' \
       -e 's/^static int sys_nplan(int i){/static int sys_nplan_g(int i){/' -e 's/^static int sys_nmoon(int i, int p){/static int sys_nmoon_g(int i, int p){/' \
       -e 's/^static int body_count(int i){/static int sys_nplan(int i){ return pc_nplan(i,sys_nplan_g(i)); }\nstatic int sys_nmoon(int i, int p){ return pc_nmoon(i,p,sys_nmoon_g(i,p)); }\n&/' \
       -e 's/^static int sys_dlv(int i){ return \(.*\)$/static int sys_dlv_g(int i){ return \1\nstatic int sys_dlv(int i){ return sys_dlv_g(i)+pc_danger(i); }/' \
       -e 's/^\(static void voyage(const char \*from, const char \*to, const u32 \*t10, u32 hours, int kind){\)/\1 hours=pc_hours(hours);/' \
       -e 's/hours_fmt(hb,(u32)d\*24\/10+1)/hours_fmt(hb,pc_hours((u32)d*24\/10+1))/' space.inc
sed -i -e 's/^static int curv_k(void){ static const u8 ks\[6\]={12,7,4,3,2,1}; int k=ks\[wd\.psz%6\]; if(WL(MW_TINY)){ int t=8<<(WL(MW_TINY)\/7); if(t>k) k=t; }/static int curv_k(void){ static const u8 ks[6]={12,7,4,3,2,1}; int k=pc_k16>=0 ? pc_k16 : ks[wd.psz%6]*16; if(WL(MW_TINY)){ int t=(8<<(WL(MW_TINY)\/7))*16; if(t>k) k=t; }/' \
       -e 's/^static int curv_sh(void){ return kr_mode==1 \&\& !kloop_on ? 5 : 3; }/static int curv_sh(void){ return (kr_mode==1 \&\& !kloop_on ? 5 : 3)+4; }/' \
       -e 's/return curv_k()>4 ? curv(cam_back()>>8) : 0; }/return curv_k()>64 ? curv(cam_back()>>8) : 0; }/' \
       -e 's/^    land_smooth();$/    pc_tiny_hmap();\n    land_smooth();/' \
       -e 's/^    if(WL(MW_FLOOD)){ water_lv += /    water_lv=pc_water(water_lv);\n&/' \
       -e 's/+wd\.flora\[t\]-3; if(n<1) n=1;/+wd.flora[t]-3+pc_node[t]; if(n<1) n=1;/' \
       -e 's/s32 sp = 0x1600 + (wd\.grav-1)\*0x200;/s32 sp = 0x1600 + (wd.grav-1)*0x200 - pc_slow;/' \
       -e 's/ppx=64<<16; ppy=64<<16; cam_h = ground_h(ppx,ppy)+CAM_UP; facing=1; prompt_k=-2;/& pc_tiny_fix();/' \
       -e 's/int sun_az = (int)((s\&127) + T\/64);/int sun_az = (int)((s\&127) + (u32)((u64)T*(u32)pc_day_pct\/6400u));/' planet.inc
sed -i -e 's/^static void slot_erase(int i){/static void slot_erase(int i){ pc_laws_slot_erase(i);/' -e 's/^static void slot_copy(int from, int to){/static void slot_copy(int from, int to){ pc_laws_slot_copy(from,to);/' \
       -e 's/xp=s->xp; gold=s->gold; galaxy_seed=s->galaxy_seed; uni_load();/& pc_laws_slot_load(cur_slot);/' \
       -e 's/^\(    sram_write(slot_base(cur_slot), s, sizeof(Save));\)$/\1 pc_laws_slot_save(cur_slot);/' save.inc
sed -i 's/if(s2>=0){ cur_slot=s2; oam_hide_all(); oam_flush(); if(make_bug/if(s2>=0){ cur_slot=s2; pc_laws_slot_new(s2); oam_hide_all(); oam_flush(); if(make_bug/' main.c
echo '#include "../pc_laws.inc"' >> main.c
echo '#include "../pc_info.inc"' >> main.c
echo '#include "../pc_view.inc"' >> main.c
echo '#include "../coil_test.inc"' >> main.c
echo '#include "../tests/kart_fuzz.inc"' >> main.c
# --- compile -----------------------------------------------------------------------------------------------------------
# -funsigned-char: the GBA compiler's char is unsigned. -fwrapv: ARM integers wrap. -no-pie: all addresses stay below 4 GB.
# SDL: with the window front end (default) the game links the system SDL2 (libSDL2-2.0.so.0); PC_SDL=0 builds the headless-only program.
OPT=${PC_OPT:--O2}
SDLF=""; SDLL=""
CC=${PC_CC:-gcc}; ARCH=${PC_ARCH:--m64 -no-pie}; OUT=${PC_OUT:-bugstar_pc}
if [ -n "$PC_WIN" ]; then   # Windows 32-bit cross build (llvm-mingw): 32-bit pointers make the game's "address in a u32" code just work
  SDLF="-DPC_SDL -D_WIN32_CROSS -I../platform/sdl2/include"; SDLL="-L$PC_WIN/lib -lmingw32 -lSDL2main -lSDL2 -mwindows -Wl,--disable-dynamicbase -Wl,--nxcompat -static-libgcc"
elif [ "${PC_SDL:-1}" != "0" ]; then
  SDLF="-DPC_SDL -I../platform/sdl2/include -Wl,-rpath,\$ORIGIN/lib -Wl,-rpath,\$ORIGIN"
  if [ -n "$SDL_LIB" ]; then SDLL="$SDL_LIB"; else SDLL=$(ldconfig -p 2>/dev/null | awk '/libSDL2-2.0.so.0 /{print $NF; exit}'); fi
  [ -z "$SDLL" ] && { echo "SDL2 runtime (libSDL2-2.0.so.0) not found: install it, set SDL_LIB=/path/to/libSDL2-2.0.so.0, or PC_SDL=0 for the headless build"; exit 1; }
fi
# the level cap: the PC build lets the character level go far past the GBA's 32-bit limit (the sim worker sets the final value)
CF="-std=gnu11 $OPT -w -funsigned-char -fwrapv -fno-strict-aliasing $ARCH -DNO_LOGO -DLEVEL_CAP=0x7fffffffffffffffLL ${PC_CFLAGS} $SDLF -I. -I.. -I../platform -Wa,-I,."
# the game itself (main.c + the .inc files) is the slow part (about 35 s): it is compiled again only when it, or the flags, changed
mkdir -p ../obj${PC_WIN:+win}; OBJD=../obj${PC_WIN:+win}
STAMP=$(cat *.inc main.c ../coil_test.inc ../tests/cn_check.inc ../tests/kart_fuzz.inc assets.h facts.bin ../pc_hw.h ../platform/pc_plat.h ../pc_info.inc ../pc_view.inc ../pc_laws.inc ../laws/lawx.h ../content/pcx.h | md5sum | cut -c1-32)-$(echo "$CF" | md5sum | cut -c1-8)
if [ "$(cat $OBJD/game.stamp 2>/dev/null)" != "$STAMP" ] || [ ! -f $OBJD/game.o ]; then rm -f $OBJD/game.stamp; $CC $CF -c main.c -o $OBJD/game.o || exit 1; echo "$STAMP" > $OBJD/game.stamp; fi
OBJS="$OBJD/game.o"
for f in ../pc_main.c ../pc_hw.c ../platform/pc_psg.c ../content/pc_content.c ../content/pcx.c ../laws/laws.c ../laws/lawmodel.c ../laws/lawtext.c ../laws/lawx.c $( [ "${PC_SDL:-1}" != "0" ] && echo ../platform/pc_sdl.c ); do o=$OBJD/$(basename $f .c).o; $CC $CF -c $f -o $o; OBJS="$OBJS $o"; done
$CC $ARCH $OBJS $SDLL -lm -o ../$OUT
echo "built pc/$OUT ($([ "${PC_SDL:-1}" != "0" ] && echo "with window" || echo "headless only"))"
