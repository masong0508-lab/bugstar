// pc_info.inc: the PC-only WORLD FILE. It is #included at the end of the generated main.c (like pc_view.inc), so it can call the game's own functions.
// It shows what the planet you stand on is made of, from the PC content library (pc/content/pcx.c): the universe's two rule changes, the planet's
// 3 to 6 traits (of 1,040), its 112 stats, its peoples and city, its native creatures and its most famous creature.
// Open it: pause (START), the WORLD page, press B. L and R turn the pages. B or START closes it.
#include "content/pcx.h"
static PcxWorld pcw;
static void pc_world_make(void){
    u32 cx[5], cy[5]; for(int i = 0; i < 5; i++){ cx[i] = clus[i]; cy[i] = clus[UAX + i]; }
    pcx_gods(sim_open_gods());
    pcx_world(uni_w, cx, cy, cur_sys, cur_pl, cur_moon, pc_applied, &pcw);
}
// called by make_world (build.sh puts the call in) on the planets of the endless galaxy: the PC traits set the gravity, the light and the weather
static void pc_apply_traits(void){
    u32 cx[5], cy[5]; int o[3]; for(int i = 0; i < 5; i++){ cx[i] = clus[i]; cy[i] = clus[UAX + i]; }
    pcx_effects(uni_w, cx, cy, cur_sys, cur_pl, cur_moon, o);
    wd.grav = (u8)o[0]; if(o[1] >= 0) wd.hour = (u8)o[1]; if(o[2] >= 0) wd.weather = (u8)o[2];
    pc_applied = 1;
}
static int pc_col(int c){ return c == PX_GOLD ? C_GOLD : c == PX_GREY ? C_GREY : c == PX_CYAN ? C_CYAN : c == PX_GREEN ? C_GREEN : c == PX_RED ? C_RED : C_WHITE; }
static void pc_world_draw(volatile u16 *pg, int page){
    PcxPage *p = &pcw.page[page]; char b[48];
    ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143);
    scpy(b, "PC WORLD FILE: "); scat(b, p->title); ui_head(pg, 9, 8, 222, b, C_GOLD);
    { char pn[16]; scpy(pn, ""); ncat(pn, page + 1); scat(pn, "/"); ncat(pn, pcw.npages); rtext(pg, 231, 8, pn, U_SOFT); }
    for(int i = 0; i < p->n; i++) text(pg, 9, 19 + i * 7, p->t[i], pc_col(p->col[i]), 1);
    ui_foot(pg, "L R: PAGE", "B: BACK");
}
static void pc_world_screen(volatile u16 *pg){
    int page = 0, dirty = 1; pc_world_make(); wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); pc_world_draw(pg, page); }
        vsync(); read_keys();
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_L)){ page = (page + pcw.npages - 1) % pcw.npages; dirty = 1; sfx(7); }
        if(HIT(K_R)){ page = (page + 1) % pcw.npages; dirty = 1; sfx(7); }
    }
    wait_release();
}

// ---- the MEANING LAYER in play. Called at the end of make_world (build.sh): on endless-galaxy planets the planet's whole make-up (traits, stats, universe themes, peoples, city,
// creatures, plants, edited universe laws) is added up by pc/meaning into channels, and these follow: hunger (FOOD), enemy count (DANGER), gold (TRADE, LUCK), resource nodes
// (ORE, GROWTH, CRAFT), walking speed (SPEED), light (LIGHT), weather (WEATHER) and the sky and building colours (TINT, GLOW, MOOD, BEAUTY). Off: BUGSTAR_MEANING=0 in the environment.
extern char *getenv(const char *);
static int pc_mn_cl(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }
static u16 pc_mn_tint(u16 c, int dr, int dg, int db){ int r = (c & 31) + dr / 8, g = ((c >> 5) & 31) + dg / 8, b = ((c >> 10) & 31) + db / 8; return (u16)(pc_mn_cl(r, 0, 31) | (pc_mn_cl(g, 0, 31) << 5) | (pc_mn_cl(b, 0, 31) << 10)); }
static int pc_land_pend; static char pc_land_txt[48];
static void pc_mn_dna(void){
    pc_gold_pct = 100; pc_land_pend = 0;
    if(!pc_applied) return;
    { const char *e = getenv("BUGSTAR_MEANING"); if(e && e[0] == '0') return; }
    u32 cx[5], cy[5]; for(int i = 0; i < 5; i++){ cx[i] = clus[i]; cy[i] = clus[UAX + i]; }
    pcx_law_clear(); for(int i = 0; i < plw_count(); i++) if(plw_is_edited(i)) pcx_law_set(i, plw_value(i));
    pcx_gods(sim_open_gods());
    { PcxMeaning m; (void)m; }
    pcx_means_line(uni_w, cx, cy, cur_sys, cur_pl, cur_moon, pc_land_txt, sizeof pc_land_txt); pc_land_pend = 1;
    PcxMeaning m; pcx_meaning(uni_w, cx, cy, cur_sys, cur_pl, cur_moon, &m);
    wd.fauna = (u8)pc_mn_cl(wd.fauna + m.fauna_add, 0, 5); wd.beetles = (u8)pc_mn_cl(wd.beetles + m.beetle_add, 0, 3);
    pc_hun_pct = pc_hun_pct * m.hunger_pct / 100; pc_gold_pct = m.gold_pct;
    pc_slow = pc_mn_cl(pc_slow - m.speed_add, -0x600, 0xE00);
    if(m.hour >= 0) wd.hour = (u8)m.hour; if(m.weather >= 0) wd.weather = (u8)m.weather;
    for(int i = 0; i < 4; i++) pc_node[i] += m.node[i];
    wd.sky = pc_mn_tint(wd.sky, m.sky_r, m.sky_g, m.sky_b);
    if(m.lift > 0) wd.bld = lift15(wd.bld, m.lift); else if(m.lift < 0) wd.bld = shade15(wd.bld, 16 + m.lift);
}

// once, a moment after landing on an endless-galaxy planet: the planet's look in a few words (called from sim_frame, so only with the planet running)
static void pc_land_note(void){ if(pc_land_pend && frames > 90){ pc_land_pend = 0; say(pc_land_txt); } }
