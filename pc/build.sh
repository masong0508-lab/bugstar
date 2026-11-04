#!/bin/bash
# pc/build.sh: build the PC (native, headless) version of the GBA game's own code.
#   ./pc/build.sh            -> pc/bugstar_pc
# It copies the game's sources into pc/gen (not committed), patches the GBA-only parts with sed (list below), and compiles them
# with the fake GBA in pc_hw.c. The game's source in the repo root is never edited by this.
set -e
cd "$(dirname "$0")"; PCD=$(pwd); ROOT=$(dirname "$PCD")
if [ -n "$PC_ANDROID_PREP" ]; then export PC_PIE=1 PC_GBA_TOP=${PC_GBA_TOP:-0x30000000}; fi   # patch 30: Android prep (see below), the same flags the APK build uses
# assets.h and facts.bin come from the same generators as the GBA build, run in a scratch copy (pc/genroot, not committed) with BUGSTAR_PC=1:
# that adds the PC version's own HOW BIG IS IT? pages (pc/facts/*.py). The repo root's assets.h and facts.bin are never touched.
rm -rf genroot; mkdir -p genroot/pc; cp "$ROOT"/*.py genroot/; cp -r facts genroot/pc/facts; rm -rf genroot/pc/facts/__pycache__; mkdir -p genroot/pc/laws; cp laws/laws_def.h laws/laws.c genroot/pc/laws/; mkdir -p genroot/pc/meaning; cp meaning/meaning.c meaning/meaning.h genroot/pc/meaning/     # (the laws pages read the law table)
( cd genroot && BUGSTAR_PC=1 python3 gen.py >/dev/null )
mkdir -p gen; rm -f gen/*.inc gen/*.c gen/*.h
cp "$ROOT"/*.inc "$ROOT"/main.c "$ROOT"/logo.h gen/; cp genroot/assets.h genroot/facts.bin gen/
cd gen
# --- the patches -------------------------------------------------------------------------------------------------------
# the game has a global array named sinf (save slot info); it shadows libm sinf() for the PC code that calls it (planet creator crashed). Rename it.
sed -i "s/\\bsinf\\b/pc_slotinfo/g" debug.inc save.inc main.c
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
# Windows (COFF) GNU as has no .pushsection/.popsection (ELF only): embed facts.bin in a COFF read-only section instead
if [ -n "$PC_WIN" ]; then sed -i -e 's|\.pushsection \.rodata|.section .rdata,\\"dr\\"|' -e 's|\.popsection|.text|' sizes.inc; fi
sed -i -e 's|^#define SZ_SCR .*|#define SZ_SCR pc_szscr|' -e '/^#define NSIZE /a static u32 pc_szscr[4*NSIZE+64];' sizes.inc
sed -i '1i #include "pc_hw.h"\nstatic void pc_creator_screen(volatile unsigned short *pg, int from_game); static void pc_cr_apply(void); static int pc_cr_water(int w);' main.c
# the PC content library is a separate translation unit (pc/content/pcx.c): the game's own code has names like trait_name, so it only sees pcx.h
sed -i -e 's|^static void world_page(volatile u16 \*pg, int y); |static void pc_world_screen(volatile u16 *pg), pc_lab_screen(volatile u16 *pg); &|' \
       -e 's|^\( *\)if(HIT(K_SEL) \&\& page==7){|\1if(HIT(K_B) \&\& page==7){ pc_lab_screen(pg); dirty=1; if(sys_req) break; }\n&|' \
       -e 's|foot = big_unlocked() ? "A: FAVOURITE  SEL: HOW BIG IS IT?" : "A: SAVE AS A FAVOURITE";|foot = big_unlocked() ? "A: FAV  SEL: HOW BIG  B: PC LAB" : "A: FAVOURITE  B: PC LAB";|' ui.inc
# traits really change the endless galaxy's planets (gravity, light, weather); the six story worlds keep their own rules
sed -i '1i static void pc_apply_traits(void), pc_mn_dna(void); static int pc_applied, pc_gold_pct=100; static int pc_gold(int g){ return pc_gold_pct==100 ? g : g*pc_gold_pct/100; }' game.inc
sed -i 's|^\( *\)// a weird world: one planet in 111, only out in the galaxy.*|\1pc_applied=0; if(gal \&\& !hellw() \&\& !cur_moon) pc_apply_traits();\n&|' game.inc
# --- the MEANING LAYER (pc/meaning, pc_info.inc pc_mn_dna): the planet's whole make-up adds up to channels; food/hunger, enemies, gold, ore, speed, light, weather and sky colour follow (endless-galaxy planets only)
sed -i 's/^    pc_laws_dna(0);$/    pc_laws_dna(0); pc_mn_dna();/' game.inc
sed -i -e 's/gold+=5; show_msg("SQUASH/gold+=pc_gold(5); show_msg("SQUASH/' -e 's/gold+=12+dlv()\*3; gain_xp(8+dlv()\*2); st_kills\[2\]++;/gold+=pc_gold(12+dlv()*3); gain_xp(8+dlv()*2); st_kills[2]++;/' -e 's/else { gold+=2+dlv(); show_msg("A SHINY PEBBLE/else { gold+=pc_gold(2+dlv()); show_msg("A SHINY PEBBLE/' planet.inc
sed -i 's|^    if(dbg_hour>=0) wd.hour=dbg_hour; if(dbg_weather>=0) wd.weather=dbg_weather;|    pc_cr_apply();\n&|' game.inc
sed -i -e 's|enum { TI_CONT, TI_NEW, TI_LOAD, TI_HELP, TI_OPT, TI_EXTRA };|enum { TI_CONT, TI_NEW, TI_LOAD, TI_HELP, TI_OPT, TI_EXTRA, TI_CREATE };|' -e 's|static const char \*const tn\[7\]={"CONTINUE","NEW GAME","LOAD GAME","HOW TO PLAY","OPTIONS","EXTRAS"};|static const char *const tn[8]={"CONTINUE","NEW GAME","LOAD GAME","HOW TO PLAY","OPTIONS","EXTRAS","PLANET CREATOR"};|' -e 's|int it\[7\]; int kon=0;|int it[8]; int kon=0;|' -e 's|it\[n++\]=TI_OPT; it\[n++\]=TI_EXTRA;|it[n++]=TI_OPT; it[n++]=TI_EXTRA; it[n++]=TI_CREATE;|' -e 's|else if(it\[sel\]==TI_HELP) scpy(b,"THE BUTTONS, AND TIPS");|else if(it[sel]==TI_CREATE) scpy(b,"MAKE YOUR OWN PLANET AND PEOPLE");\n&|' -e 's|            if(a==TI_HELP){ oam_hide_all|            if(a==TI_CREATE){ oam_hide_all(); oam_flush(); pc_creator_screen(pg,0); full=1; }\n&|' main.c
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
       -e 's/^    if(WL(MW_FLOOD)){ water_lv += /    water_lv=pc_cr_water(pc_water(water_lv));\n&/' \
       -e 's/+wd\.flora\[t\]-3; if(n<1) n=1;/+wd.flora[t]-3+pc_node[t]; if(n<1) n=1;/' \
       -e 's/s32 sp = 0x1600 + (wd\.grav-1)\*0x200;/s32 sp = 0x1600 + (wd.grav-1)*0x200 - pc_slow;/' \
       -e 's/ppx=64<<16; ppy=64<<16; cam_h = ground_h(ppx,ppy)+CAM_UP; facing=1; prompt_k=-2;/& pc_tiny_fix();/' \
       -e 's/int sun_az = (int)((s\&127) + T\/64);/int sun_az = (int)((s\&127) + (u32)((u64)T*(u32)pc_day_pct\/6400u));/' planet.inc
sed -i -e 's/^static void slot_erase(int i){/static void slot_erase(int i){ pc_laws_slot_erase(i);/' -e 's/^static void slot_copy(int from, int to){/static void slot_copy(int from, int to){ pc_laws_slot_copy(from,to);/' \
       -e 's/xp=s->xp; gold=s->gold; galaxy_seed=s->galaxy_seed; uni_load();/& pc_laws_slot_load(cur_slot);/' \
       -e 's/^\(    sram_write(slot_base(cur_slot), s, sizeof(Save));\)$/\1 pc_laws_slot_save(cur_slot);/' save.inc
sed -i 's/if(s2>=0){ cur_slot=s2; oam_hide_all(); oam_flush(); if(make_bug/if(s2>=0){ cur_slot=s2; pc_laws_slot_new(s2); oam_hide_all(); oam_flush(); if(make_bug/' main.c
# --- the PC ship renderer (pc/shipr, pc/pc_ships.inc): the space ships go to it when it is on (pc_ships_mode >= 0, the window build's SHIPS option)
sed -i -e 's|^    int crowd=0; for(int i=0;i<NPIR;i++) if(pir\[i\].alive) crowd++;$|    if(pc_sr_on()){ pc_sr_space(pg); goto pc_sr_done; }\n&|' \
       -e 's|^    for(int k=0;k<4;k++) if(cans\[k\].alive){ int sx,sy; s32 z; if(sproj|    pc_sr_done: pc_sr_mark(pg); pc_so_native(pg);\n&|' space.inc
sed -i 's|ship_draw(pg, scx, scy, scs, syw, spt, 0,0, 0,0,240,128); sh_flame=0;|if(pc_sr_on() \&\& pc_ships_mode>0) pc_sr_player(scx,scy,scs,syw,spt); else ship_draw(pg, scx, scy, scs, syw, spt, 0,0, 0,0,240,128); sh_flame=0;|' space.inc
sed -i '1i static int pc_sr_on(void); static void pc_sr_space(volatile u16 *pg), pc_sr_mark(volatile u16 *pg), pc_sr_player(int cx, int cy, int sc, int yaw_, int pitch_); extern int pc_ships_mode;' space.inc
echo '#include "../pc_ships.inc"' >> main.c
# --- the native dungeon (pc/pc_dnative.inc): after the game's own 3D pass, a second picture at the window's size; the compositor shows it where the page is unchanged
sed -i 's|^    if(terr_on) render_terrace(pg,dirx,diry,plx,ply); else render_view(pg,dirx,diry,plx,ply);|&\n    pc_dn_native(pg,dirx,diry,plx,ply);|' main.c
sed -i 's|^    for(int i=0;i<NPROJ;i++){ Proj \*p=&proj\[i\]; if(!p->alive) continue;|    pc_dn_sprites_done(pg);\n&|' main.c
sed -i 's|^    spr_bar(pg,o,scx,top,wcols,tY);|    pc_dn_spr(art,aw,ah,x0,wcols,top,bot,tY,tint,dark,o->flash);\n&|' dungeon.inc
sed -i '1i static void pc_dn_spr(const unsigned char *art, int aw, int ah, int x0, int wcols, int top, int bot, int tY, int tint, int dark, int flash);' dungeon.inc
sed -i '1i static void pc_dn_sprites_done(volatile unsigned short *pg);' main.c
sed -i '1i static void pc_dn_native(volatile unsigned short *pg, int dirx, int diry, int plx, int ply);' main.c
echo '#include "../pc_dnative.inc"' >> main.c
# --- the native planet (pc/pc_pnative.inc, pc_pncomp.inc in pc_hw.c)
sed -i 's|else render_planet(pg); u32 t_r1=TMR();|else { render_planet(pg); pc_pn_native(pg); } u32 t_r1=TMR();|' planet.inc
sed -i 's|^    int ad=dd<0?-dd:dd, t=ad\*2\*rc/60-rc;|    if(pc_pn_disc(cx,cy,rc,dd,lit,dark,sunmode)) return;\n&|' planet.inc
sed -i '1i static void pc_pn_native(volatile unsigned short *pg); static int pc_pn_disc(int cx, int cy, int rc, int dd, int lit, int dark, int sunmode);' main.c
sed -i '1i #define BUGSTAR_PC_SHADOW_HOOK 1\nstatic int pc_pn_shadow(volatile unsigned short *pg, int cx, int cy, int rx0, int ry0, int tx, int ty, int rx1, int ry1, int hour);' main.c
echo '#include "../pc_pnative.inc"' >> main.c
# --- the native space view (pc/pc_snative.inc, pc_sncomp.inc in pc_hw.c)
sed -i 's|^IWRAM static void planet_fill(\(.*\)const u8 \*noise){|&\n    pc_sn_rec(cx,cy,r,Lx,Ly,Lz,rot,noise);|' space.inc
sed -i 's|^    orbit_draw(pg,0); |    orbit_draw(pg,0); pc_sn_native(pg,vpx,vpy,stk); |' space.inc
sed -i 's|int vpx=120, vpy=VCY, vdrift=0, stk=0;|pc_sn_begin(); &|' space.inc
grep -c "pc_sn_rec(cx" space.inc | grep -qx 1 && grep -c "pc_sn_native(pg" space.inc | grep -qx 1 && grep -c "pc_sn_begin()" space.inc | grep -qx 1 || { echo "build: native space hooks did not apply"; exit 1; }
sed -i '1i static void pc_sn_begin(void), pc_so_native(volatile unsigned short *pg), pc_sn_native(volatile unsigned short *pg, int vpx, int vpy, int stk), pc_sn_rec(int cx,int cy,int r,int Lx,int Ly,int Lz,int rot,const unsigned char *noise);' main.c
echo '#include "../pc_snative.inc"' >> main.c
echo '#include "../pc_laws.inc"' >> main.c
# --- the kart race NATIVE extras (pc/pc_kartnat.inc, pc_kartnat_r.inc in pc_hw.c): banking, hills, soft shadows. The game's own dithered shadows are skipped when the window picture makes its own.
sed -i 's|^static void m7_present(void){|static void pc_kn_snapshot(void);\nstatic void m7_present(void){ pc_kn_snapshot();|' m7.inc
sed -i -e 's|^    for(int i=0;i<ksh_n;i++){ for(int y=-1;y<=1;y++){|    if(!pc_kn_on()) for(int i=0;i<ksh_n;i++){ for(int y=-1;y<=1;y++){|' -e 's|^    if(!cam_fp \&\& !kair \&\& !kloop_on){ for(int y=-1;y<=1;y++){ int w=19|    if(!pc_kn_on() \&\& !cam_fp \&\& !kair \&\& !kloop_on){ for(int y=-1;y<=1;y++){ int w=19|' kart.inc
sed -i 's|if(!kart_glow() \&\& !kloop_on \&\& slot<120){ shadow_oam\[slot\].a0=((bug_sy-6)|if(!pc_kn_on() \&\& !kart_glow() \&\& !kloop_on \&\& slot<120){ shadow_oam[slot].a0=((bug_sy-6)|' planet.inc
sed -i 's|^\(        \)s32 sc=8+m\*4; |\1s32 sc=8+m*4; if(kr_mode==1 \&\& ((o->kind>=O_PLANT \&\& o->kind<=O_FLOWER) \|\| o->kind==O_FOSSIL \|\| o->kind==O_TUFT)){ extern void pc_kn_obj(int,int,int,int); pc_kn_obj(sx,sy,w_of[o->kind],(int)sc); }  |' planet.inc
grep -q 'pc_kn_obj' planet.inc || { echo "build.sh: pc_kn_obj hook anchor not found in planet.inc"; exit 1; }
echo '#include "../pc_kartnat.inc"' >> main.c
# --- the side game (pc/sim, DEPTH_PLAN): hooks into the copies in pc/gen, never the repo root. Each hook must find its anchor exactly once or the build fails.
H() { python3 ../sim/hook.py "$@"; }
H main.c 'typedef long long s64;' $'\n#include "sim/sim_proto.h"' after
H main.c 'XO_PVIS, NXOPT };' 'XO_PVIS, XO_SIDE, NXOPT };'
H main.c '  [XO_MSGT]   =XR("MESSAGE TIME","HOW LONG POP-UP MESSAGES STAY ON SCREEN",xl_msgt,1,XG_EXTRA),' $'\n  [XO_SIDE]   =XR("SIDE GAME","PC ONLY: NAMED TOWNSFOLK, A JOURNAL AND GOD POWERS. OFF GIVES THE PLAIN GAME",xl_yn,1,XG_EXTRA),' after
H main.c 'XO_PVIS, XO_SIDE, NXOPT };' 'XO_PVIS, XO_SIDE, XO_SIDEDIF, NXOPT };'
H main.c 'XL(xl_pvis,"100%","50%","25%")' $'XL(xl_sdif,"CALM","NORMAL","HARSH")      XV(xl_sdif,0,1,2)\nXL(xl_pvis,"100%","50%","25%")'
H main.c '  [XO_SIDE]   =XR(' $'  [XO_SIDEDIF]=XR("SIDE GAME DIFFICULTY","CALM: NOBODY DIES, RAIDS ALWAYS FAIL. NORMAL: HUNGER, SICKNESS AND RAIDS KILL. HARSH: A LOST BIG RAID CAN BURN THE PLANET",xl_sdif,1,XG_EXTRA),\n  [XO_SIDE]   =XR('
H planet.inc 'life_tick(n); race_tick(n); kart_tick(n);' ' sim_frame(n);' after
H planet.inc 'if(best<0) show_msg("NOTHING HERE");' 'if(best<0){ if(sim_quick()){ planet_hud(); p_last=vbl_count; return; } show_msg("NOTHING HERE"); }'
H planet.inc 'oam_flush(); local_talk(&po[best]);' 'oam_flush(); sim_talk(&po[best]); local_talk(&po[best]);'
H planet.inc 'if(opt_minimap && !FILM && !kr_mode) planet_oremap(pg);' ' sim_hud(pg);' after
H save.inc 'save_ext(); uni_save(); grind_save(); ore_save();' ' sim_save();' after
H main.c 'if(!(cont && load_game(1))){ cont=0; new_game();' 'if(!(cont && load_game(1) && (sim_load(),1))){ cont=0; new_game(); sim_reset();'
H game.inc 'scpy(lv, level<1000 ? "LV" : "L"); ncatx(lv,level,1);' ' sim_lv_fix(lv);' after
H game.inc 'int xb = ratio(xp,xp_next(),16);' 'int xb = ratio(sim_xpv(xp),sim_xpm(xp_next()),16);'
H game.inc 'while(level<LEVEL_CAP && xp>=xp_next() && ups<64){' 'while(!sim_big_xp() && level<LEVEL_CAP && xp>=xp_next() && ups<64){'
H ui.inc 'scpy(b,"LEVEL "); ncat(b,level); scat(b,"   BUGS FREED ");' 'scpy(b,"LEVEL "); sim_ncat_level(b); scat(b,"   BUGS FREED ");'
H ui.inc 'char lv[12]="LV "; ncat(lv,level);' 'char lv[16]="LV "; sim_ncat_level(lv);'
H ui.inc 'vbar(pg,30,75,90,xp,xp_next(),C_GOLD,"XP")' 'vbar(pg,30,75,90,sim_xpv(xp),sim_xpm(xp_next()),C_GOLD,"XP")'
H ui.inc 'static const u8 shop_tab_n[4]={3,5,5,4};' 'static u8 shop_tab_n[4]={3,5,5,4};'
H ui.inc 'shop_tab_id[4][5]={{0,14,13,0,0},' 'shop_tab_id[4][5]={{0,14,13,17,18},'
H ui.inc 'static const char *const shop_nm[17]={' 'static const char *const shop_nm[19]={'
H ui.inc '"KART CLUB","SPELL SHOP","MINING"};' '"KART CLUB","SPELL SHOP","MINING","COLONY","MISSIONS"};'
H ui.inc 'static const char *const shop_ds[17]={' 'static const char *const shop_ds[19]={'
H ui.inc '"SELL ORE, UPGRADE YOUR TOOL, SEE YOUR ORE BAG"};' '"SELL ORE, UPGRADE YOUR TOOL, SEE YOUR ORE BAG","YOUR SETTLERS: SEE THEM AND TAKE WHAT THEY MADE","WARSHIP JOBS: BOARD AND JOURNAL"};'
# --- patch 15: colony calls in the dungeon (the colony's trouble picks a floor's goal; the floor event code does the rest)
H dungeon.inc 'fl_ev=FE_NONE; fl_need=fl_have=0; fl_bonus=100; fl_done=0;' ' sim_dn_reset();' after
H dungeon.inc '(rndi(100)<65 || cur_rock==5 || (ddna.on && ddna.haz))){' '(rndi(100)<65 || cur_rock==5 || (ddna.on && ddna.haz) || sim_dn_call())){'
H dungeon.inc '        switch(fl_ev){' '        sim_dn_pick(); switch(fl_ev){'
H dungeon.inc '    if(endless && cur_moon && !mboss){ char mb[40];' '    sim_dn_mark(); if(endless && cur_moon && !mboss){ char mb[40];'
H dungeon.inc 'fl_done=1; int g=200+dlv()*40+depth_no*10;' 'fl_done=1; sim_dn_done(FE_ELITE); int g=200+dlv()*40+depth_no*10;'
H dungeon.inc 'static void fl_leave(void){' 'static void fl_leave(void){ sim_dn_leave();'
H dungeon.inc 'static void fl_draw(volatile u16 *pg){' ' sim_dn_draw(pg);' after
H main.c 'dob[i].alive=0; free_bug(dob[i].sub); break;' '{ int cc_=dob[i].champ==3; dob[i].alive=0; free_bug(dob[i].sub); if(cc_) sim_dn_done(FE_QUIET); } break;'
H main.c 'dob[near_cage].alive=0; free_bug(dob[near_cage].sub);' '{ int cc_=dob[near_cage].champ==3; dob[near_cage].alive=0; free_bug(dob[near_cage].sub); if(cc_) sim_dn_done(FE_QUIET); }'
H ui.inc 'in_menu++; menu_msg[0]=0; ship_build_model();' 'in_menu++; menu_msg[0]=0; ship_build_model(); shop_tab_n[0]=sim_is_on()?5:4; shop_tab_id[0][3]=sim_is_on()?17:18; shop_tab_id[0][4]=18;'
H ui.inc 'if(sel==15){ spell_shop(pg); continue; }' 'if(sel==17){ sim_ship_colony(pg); continue; } if(sel==15){ spell_shop(pg); continue; }'
# --- the PC SHIP EDITOR (pc/shipyard): the hangar opens it, and your design gives the ship's numbers (no design file for the slot = the game's own numbers)
for f in armour top_speed turn damp gun hold_bonus fuel_bonus accel16; do H ship.inc "static int ship_$f(void){" "static int ship_${f}_g(void){"; done
H ship.inc '#define SV_MAX 128' $'static int pc_sy_stat(int k, int v);\nstatic int ship_armour(void){ return pc_sy_stat(0, ship_armour_g()); } static int ship_top_speed(void){ return pc_sy_stat(1, ship_top_speed_g()); } static int ship_turn(void){ return pc_sy_stat(2, ship_turn_g()); } static int ship_damp(void){ return pc_sy_stat(3, ship_damp_g()); }\nstatic int ship_gun(void){ return pc_sy_stat(4, ship_gun_g()); } static int ship_hold_bonus(void){ return pc_sy_stat(5, ship_hold_bonus_g()); } static int ship_fuel_bonus(void){ return pc_sy_stat(6, ship_fuel_bonus_g()); } static int ship_accel16(void){ return pc_sy_stat(7, ship_accel16_g()); }\n#define SV_MAX 128'
H hangar.inc 'static int make_ship(volatile u16 *pg, int first){' $'static int pc_sy_screen(volatile u16 *pg, int first);\nstatic int make_ship(volatile u16 *pg, int first){ return pc_sy_screen(pg, first); }\nstatic int make_ship_g(volatile u16 *pg, int first){'
H main.c 'typedef long long s64;' $'\nstatic void pc_sy_slot_erase(int s), pc_sy_slot_copy(int a, int b), pc_sy_autotest(void);' after
H save.inc 'pc_laws_slot_erase(i);' ' pc_sy_slot_erase(i);' after
H save.inc 'pc_laws_slot_copy(from,to);' ' pc_sy_slot_copy(from,to);' after
echo '#include "../sim/sim_glue.inc"' >> main.c
echo '#include "../pc_info.inc"' >> main.c
echo '#include "../creator/creator_ui.inc"' >> main.c
echo '#include "../shipyard/shipyard_ui.inc"' >> main.c
echo '#include "../pc_giant.inc"' >> main.c
# --- COLOSSAL WARSHIPS (pc/warship, pc/pc_warship.inc): the fight uses the lair code path (in_lair = 1, pc_ws_on = 1); every hook is a no-op when no warship fight is on
sed -i '1i static int pc_ws_on; static int pc_ws_lim(int d), pc_ws_danger(void), pc_ws_sel(void), pc_ws_scan(void); static void pc_ws_shot(void), pc_ws_update(void), pc_ws_lost(void), pc_ws_space_enter(void), pcw_draw(volatile unsigned short *pg), pcw_hud(volatile unsigned short *pg), pc_ws_autotest(void), pc_ws_slot_load(int s), pc_ws_slot_save(int s), pc_ws_slot_new(int s);' main.c
H space.inc '        if(in_lair) beast_shot();' '        if(in_lair){ if(pc_ws_on) pc_ws_shot(); else beast_shot(); }'
H space.inc '    if(in_lair) beast_update();' '    if(in_lair){ if(pc_ws_on) pc_ws_update(); else beast_update(); }'
H space.inc '    if(in_lair) beast_draw(pg);' '    if(in_lair){ if(pc_ws_on) pcw_draw(pg); else beast_draw(pg); }'
H space.inc '    if(in_lair) beast_hud(pg);' '    if(in_lair){ if(pc_ws_on) pcw_hud(pg); else beast_hud(pg); }'
H space.inc 'if(HIT(K_SEL) && !coil_sel_eaten){ if(in_lair){ lair_leave(); return 0; } return 2; }' 'if(HIT(K_SEL) && !coil_sel_eaten){ if(pc_ws_sel()) return 0; if(HELD(K_R) && pc_ws_scan()) return 0; if(in_lair){ lair_leave(); return 0; } return 2; }'
H space.inc 's32 lim = in_lair ? 2400 : PR+300;' 's32 lim = in_lair ? pc_ws_lim(2400) : PR+300;'
H space.inc 'if(hull<=0){ if(in_lair){ in_lair=0;' 'if(hull<=0){ pc_ws_lost(); if(in_lair){ in_lair=0;'
H space.inc 'static int space_danger(void){ return in_lair ?' 'static int space_danger(void){ if(pc_ws_on) return pc_ws_danger(); return in_lair ?'
H space.inc '    if(hive_card && !no_ambush) show_hive_card();' ' pc_ws_space_enter();' after
H space.inc 'space_say(in_lair?"YOU RAMMED THE BEAST!":' 'space_say(in_lair?(pc_ws_on?"YOU RAMMED ITS HULL!":"YOU RAMMED THE BEAST!"):'
H main.c '        rng_s += keys;' ' pc_ws_autotest();' after
H main.c '        rng_s += keys; pc_ws_autotest();' ' pc_sy_autotest();' after
H save.inc 'pc_laws_slot_load(cur_slot);' ' pc_ws_slot_load(cur_slot);' after
H save.inc 'sram_write(slot_base(cur_slot), s, sizeof(Save)); pc_laws_slot_save(cur_slot);' ' pc_ws_slot_save(cur_slot);' after
H main.c 'pc_laws_slot_new(s2);' ' pc_ws_slot_new(s2);' after
H ui.inc 'static const u8 shop_tab_id[4][5]' 'static u8 shop_tab_id[4][5]'
H ui.inc 'if(sel==16){ mining_menu(pg); continue; }' 'if(sel==18){ pc_ms_board(pg,0); box(pg,12,20,216,136); continue; } if(sel==16){ mining_menu(pg); continue; }'
H space.inc 'if(HIT(K_START) || (tab==1 && HIT(K_A) && sel==10)) break;' 'if(HIT(K_SEL)){ pc_ms_board(pg,1); dirty=1; wait_release(); continue; } if(HIT(K_START) || (tab==1 && HIT(K_A) && sel==10)) break;'
H space.inc '"A: BUY  L/R: MARKET  START: LEAVE"' '"A: BUY  L/R: MARKET  SEL: MISSIONS  START: LEAVE"'
H space.inc 'for(int i=0;i<NSYS;i++) if(has_base(i) && (i<6 ? i<zones_open : ant_king)){ int x=sys[i].x, y=sys[i].y; rect(pg,x-3,y-3,2,2,C_RED);' 'pc_ms_map(pg); for(int i=0;i<NSYS;i++) if(has_base(i) && (i<6 ? i<zones_open : ant_king)){ int x=sys[i].x, y=sys[i].y; rect(pg,x-3,y-3,2,2,C_RED);'
H planet.inc 'bounty_office(VISPG); planet_hud(); p_last=vbl_count; return; }' 'bounty_office(VISPG); pc_ms_board(VISPG,2); planet_hud(); p_last=vbl_count; return; }'
sed -i '1i static void pc_ms_board(volatile unsigned short *pg, int place), pc_ms_map(volatile unsigned short *pg);' main.c
# --- AFTERMATH of a severe warship (pc/pc_missions.inc): burnt-world look, the sealed entrance, troops holding the stairs, the burning-surface survival line
sed -i '1i static void pcm_hud(volatile unsigned short *pg); static int pcm_up_blocked(int surface), pcm_burnt_here(void), pcm_burnt_cls(void);' main.c
H game.inc '    if(dbg_cls>=0) wd.cls=dbg_cls;' ' if(pcm_burnt_here()) wd.cls=PC_INFERNO+pcm_burnt_cls();' after
H planet.inc 'sim_hud(pg);' ' pcm_hud(pg);' after
H main.c '            if(!(!endless && !forge_on && !base_mode && depth_no>=2)) return 1;' '            if(pcm_up_blocked(!(!endless && !forge_on && !base_mode && depth_no>=2))) return 0; if(!(!endless && !forge_on && !base_mode && depth_no>=2)) return 1;'
H main.c '            if(to==0) return 1;' '            if(to>=0 && to<=depth_no && pcm_up_blocked(to==0)) return 0; if(to==0) return 1;'
echo '#include "../pc_warship.inc"' >> main.c
echo '#include "../pc_view.inc"' >> main.c
echo '#include "../coil_test.inc"' >> main.c
case "$PC_CFLAGS" in *TEST_KFUZZ*) echo '#include "../tests/kart_fuzz.inc"' >> main.c;; esac   # only the fuzz test build (-DTEST_KFUZZ=n) includes the driver
# --- 64-bit clean DMA (patch 25): the game writes DMA source / destination as "(u32)&something". Above 4 GB that loses the top half, so the two writes
# become calls that keep the full pointer (pc_hw.c pc_dma_sad / pc_dma_dad). The (u32) cast right after the "=" is dropped.
sed -i -e 's/\*(volatile u32\*)0x040000D4=(u32)\([^;]*\);/pc_dma_sad((uintptr_t)(\1));/g' -e 's/\*(volatile u32\*)0x040000D8=(u32)\([^;]*\);/pc_dma_dad((uintptr_t)(\1));/g' *.inc main.c
# --- the fake GBA's address range (pc/platform/pc_gbaaddr.h). PC_GBA_TOP=0x30000000 moves every GBA address up by that much (Android: the Java heap sits on 02000000-12000000).
# Every 0x0[2-8E]xxxxxx in the game's own code is a GBA address (RAM, I/O, palette, VRAM, OAM, ROM, SRAM): wrap it in GBAA(). With PC_GBA_TOP unset that is +0: the same program.
# Two numbers look like addresses but are not (a random "memory error" screen, and a bit mask): hidden from the rewrite and put back.
sed -i -e 's/0x02000000u+((fx_rnd/@@KEEP_A@@/g' -e 's/0x07FFFFFFu/@@KEEP_B@@/g' *.inc main.c
python3 -c 'import re,sys
r=re.compile(r"(?<![0-9A-Za-z_@])(0[xX]0[2-8eE][0-9A-Fa-f]{6}u?)(?![0-9A-Za-z_])")
for f in sys.argv[1:]:
    s=open(f,encoding="latin-1",newline="").read(); t=r.sub(r"GBAA(\1)",s)
    if t!=s: open(f,"w",encoding="latin-1",newline="").write(t)' *.inc main.c
sed -i -e 's/@@KEEP_A@@/0x02000000u+((fx_rnd/g' -e 's/@@KEEP_B@@/0x07FFFFFFu/g' *.inc main.c
# the sound-DMA register stores in m7.inc (nothing on the fake GBA reads them) and the debug stack probe keep only 32 bits on purpose: say so, so the PC_PIE warning list stays clean
sed -i -e 's/=(u32)&\([fdt]\)\[1\];/=(u32)(uintptr_t)\&\1[1];/g' m7.inc
sed -i 's/(u32)((u32\*)GBAA(0x03000000))/(u32)(uintptr_t)GBAA(0x03000000)/' debug.inc
if [ -n "$PC_GBA_TOP" ]; then PC_CFLAGS="$PC_CFLAGS -DPC_GBA_TOP=$PC_GBA_TOP"; fi
# --- compile -----------------------------------------------------------------------------------------------------------
# -funsigned-char: the GBA compiler's char is unsigned. -fwrapv: ARM integers wrap. -no-pie: all addresses stay below 4 GB.
# SDL: with the window front end (default) the game links the system SDL2 (libSDL2-2.0.so.0); PC_SDL=0 builds the headless-only program.
OPT=${PC_OPT:--O2}
SDLF=""; SDLL=""
CC=${PC_CC:-gcc}; ARCH=${PC_ARCH:--m64 -no-pie}; OUT=${PC_OUT:-bugstar_pc}
# PC_PIE=1 (patch 25, Android step 2): a normal position-independent build, as an APK has it. No -no-pie, no low stack (pc_main.c runs the game on a pthread),
# -ffp-contract=off so arm64 and x86 round the same, and the compiler's pointer-to-int warnings on (every one left that is not a fixed GBA address is a place to fix).
WARN="-w"
if [ -n "$PC_PIE" ]; then ARCH=${PC_ARCH-}; PC_CFLAGS="$PC_CFLAGS -DPC_PIE -ffp-contract=off -fPIE"; WARN="-Wpointer-to-int-cast -Wint-to-pointer-cast"; PC_OBJ="${PC_OBJ}_pie"; fi
if [ -n "$PC_ANDROID_PREP" ]; then SDLF="-DPC_SDL -DPC_ANDROID -I$PCD/platform/sdl2/include"; SDLL=""; WARN="-w"
elif [ -n "$PC_WIN" ]; then   # Windows 32-bit cross build (llvm-mingw): 32-bit pointers make the game's "address in a u32" code just work
  if [ -n "$PC_WIN64" ]; then   # real 64-bit Windows exe (MSYS2 mingw64). The game keeps addresses in 32-bit numbers, so the exe is NOT large-address-aware and sits at a low base (the flag is cleared after linking by tools/pe_lowaddr.py):
    # Windows then keeps its stack, heap and DLLs below 2 GB, which is the same trick the 32-bit build used.
    ARCH="-m64"; CC=${PC_CC:-gcc}
    SDLF="-DPC_SDL -D_WIN32_CROSS -I../platform/sdl2/include"; SDLL="-L$PC_WIN/lib -lmingw32 -lSDL2main -lSDL2 -mwindows -Wl,--image-base=0x10000000 -Wl,--nxcompat -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic"
  else
  SDLF="-DPC_SDL -D_WIN32_CROSS -I../platform/sdl2/include"; SDLL="-L$PC_WIN/lib -lmingw32 -lSDL2main -lSDL2 -mwindows -Wl,--disable-dynamicbase -Wl,--nxcompat -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic"
  fi
elif [ "${PC_SDL:-1}" != "0" ]; then
  SDLF="-DPC_SDL -I../platform/sdl2/include -Wl,-rpath,\$ORIGIN/lib -Wl,-rpath,\$ORIGIN"
  if [ -n "$SDL_LIB" ]; then SDLL="$SDL_LIB"; else SDLL=$(ldconfig -p 2>/dev/null | awk '/libSDL2-2.0.so.0 /{print $NF; exit}'); fi
  [ -z "$SDLL" ] && { echo "SDL2 runtime (libSDL2-2.0.so.0) not found: install it, set SDL_LIB=/path/to/libSDL2-2.0.so.0, or PC_SDL=0 for the headless build"; exit 1; }
fi
# the level cap: the PC build lets the character level go far past the GBA's 32-bit limit (the sim worker sets the final value)
# newer GCC (14+, e.g. MSYS2 GCC 16) makes a call to an undeclared function an error: the game .inc files rely on the libc/libm/pc_plat declarations, so pull them in for every file
CF="-std=gnu11 $OPT $WARN -include string.h -include stdio.h -include stdlib.h -include math.h -include pc_plat.h -funsigned-char -fwrapv -fno-strict-aliasing $ARCH -DNO_LOGO -DLEVEL_CAP=0x7fffffffffffffffLL ${PC_CFLAGS} $SDLF -I. -I.. -I../platform -Wa,-I,."
# the game itself (main.c + the .inc files) is the slow part (about 35 s): it is compiled again only when it, or the flags, changed
# --- patch 30 (Android step 3): PC_ANDROID_PREP=1 bash pc/build.sh does NOT compile. It writes pc/gen/sources.txt (every .c file, relative to pc/) and
# pc/gen/cflags.txt (the flags used here, with absolute include paths) for pc/android/app/jni/src/Android.mk, so the APK build and this script never drift apart.
SRCS="gen/main.c ../pc_main.c ../pc_hw.c ../shipr/shipr.c ../platform/pc_psg.c ../content/pc_content.c ../content/pcx.c ../content/flora.c ../creator/cr_recipe.c ../creator/cr_bio.c ../creator/cr_files.c ../shipyard/sy_design.c ../shipyard/sy_files.c ../meaning/meaning.c ../giant/giant.c ../warship/ws.c ../warship/ws_fight.c ../warship/ws_deck.c ../warship/ws_view.c ../warship/ms.c ../sim/god.c ../sim/lv.c ../sim/lvxp.c ../sim/sim_core.c ../sim/colony.c ../sim/sim_io.c ../laws/laws.c ../laws/lawmodel.c ../laws/lawtext.c ../laws/lawx.c $( [ "${PC_SDL:-1}" != "0" ] && echo ../platform/pc_sdl.c )"
if [ -n "$PC_ANDROID_PREP" ]; then
  sed -i 's|facts\.bin\\"|'"$PCD"'/gen/facts.bin\\"|' sizes.inc          # the NDK compiles from another folder: give .incbin the full path
  echo "$SRCS" | tr ' ' '\n' | grep -v '^$' | sed -e 's|^\.\./||' > sources.txt
  echo "$CF" | sed -e "s| -I\. | -I$PCD/gen |" -e "s| -I\.\. | -I$PCD |" -e "s| -I\.\./platform | -I$PCD/platform |" -e "s|-Wa,-I,\.|-Wa,-I,$PCD/gen|" > cflags.txt
  echo "prepared pc/gen: $(wc -l < sources.txt) sources, flags in pc/gen/cflags.txt"; exit 0
fi
mkdir -p ../obj${PC_WIN:+win}${PC_OBJ}; OBJD=../obj${PC_WIN:+win}${PC_OBJ}
STAMP=$(cat *.inc main.c ../coil_test.inc ../tests/cn_check.inc ../tests/kart_fuzz.inc assets.h facts.bin ../pc_hw.h ../platform/pc_plat.h ../pc_info.inc ../pc_view.inc ../pc_laws.inc ../pc_ships.inc ../pc_dnative.inc ../pc_kartnat.inc ../pc_pnative.inc ../pc_pncomp.inc ../pc_snative.inc ../pc_sncomp.inc ../shipr/shipr.h ../pc_giant.inc ../pc_warship.inc ../pc_missions.inc ../warship/*.h ../giant/giant.h ../sim/sim_glue.inc ../sim/sim_ui.inc ../sim/*.h ../laws/lawx.h ../content/pcx.h ../meaning/meaning.h ../creator/*.inc ../creator/*.h ../shipyard/*.inc ../shipyard/*.h | md5sum | cut -c1-32)-$(echo "$CF" | md5sum | cut -c1-8)
if [ "$(cat $OBJD/game.stamp 2>/dev/null)" != "$STAMP" ] || [ ! -f $OBJD/game.o ]; then rm -f $OBJD/game.stamp; $CC $CF -c main.c -o $OBJD/game.o || exit 1; echo "$STAMP" > $OBJD/game.stamp; fi
OBJS="$OBJD/game.o"
for f in $SRCS; do [ "$f" = gen/main.c ] && continue; o=$OBJD/$(basename $f .c).o; $CC $CF -c $f -o $o; OBJS="$OBJS $o"; done
$CC $ARCH ${PC_PIE:+-pie} $OBJS $SDLL -lm -lpthread -o ../$OUT
if [ -n "$PC_WIN64" ]; then python3 "$PCD/tools/pe_lowaddr.py" ../$OUT || exit 1; fi
echo "built pc/$OUT ($([ "${PC_SDL:-1}" != "0" ] && echo "with window" || echo "headless only"))"
