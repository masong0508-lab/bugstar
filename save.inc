// ================================================================= SAVE SYSTEM: three slots on the cartridge, options, and the menus for them
// SRAM map (32 KB):  slot n lives at n*0x1000:  +0x000 Save (the bug)  +0x800 SaveX (the ship, see space.inc)  +0xC00 Meta (play time)
//                    options live at 0x7F00.  Slot 1 sits exactly where the old single save did, so old saves still load.
__attribute__((used, aligned(4))) const char sram_tag[]="SRAM_V114";
#define SRAM ((volatile u8*)0x0E000000)
#define NSLOT 3
#define SLOT_STRIDE 0x1000
#define SG_OFF 0x400
#define SX_OFF 0x800
#define META_OFF 0xC00
#define SET_OFF 0x7F00
static int slot_base(int i){ return i*SLOT_STRIDE; }
#define XT_BASE 0x3000                  // spare SRAM above the three slots: the 256 favourites live here now (see space.inc)
#define XT_STRIDE 0x1400
static int xt_base(int i){ return XT_BASE+i*XT_STRIDE; }
static u32 fnv(const void *d, unsigned n){ const u8 *p=d; u32 h=2166136261u; for(unsigned i=0;i<n;i++){ h^=p[i]; h*=16777619u; } return h; }
static void sram_read(int off, void *d, unsigned n){ u8 *p=d; for(unsigned i=0;i<n;i++) p[i]=SRAM[off+i]; }
static void sram_write(int off, const void *s, unsigned n){ const u8 *p=s; for(unsigned i=0;i<n;i++) SRAM[off+i]=p[i]; }
// ---------------------------------------------------------------- the universe number: 128 bits (4 words, low word first)
// uni_w[0] is the old 32-bit number, so every universe you could reach before is still the same universe with the same name. galaxy_seed is the 32-bit
// value the generators mix: uni_w[0] ^ a fold of the upper 96 bits (0 when they are all zero). Always change a universe with uni_set(), never galaxy_seed.
static u32 uni_fold(const u32 *u){ u32 hi=0; for(int i=1;i<UW;i++) hi|=u[i]; if(!hi) return 0; int top=UW-1; while(top>3 && !u[top]) top--;
    u32 h=0x5A17C0DEu; for(int i=top;i>=1;i--) h=hmix(u[i]^h); return h; }
static void uni_set(const u32 *u){ for(int i=0;i<UW;i++) uni_w[i]=u[i]; galaxy_seed=u[0]^uni_fold(u); }
static void uni_set32(u32 s){ u32 u[UW]={0}; u[0]=s; uni_set(u); }
static int uni_eq(const u32 *a, const u32 *b){ for(int i=0;i<UW;i++) if(a[i]!=b[i]) return 0; return 1; }
typedef struct { u32 magic, w[UW], eff, sum; } UniSave;                      // the slot's universe number: all 256 bits, and the seed they made
typedef struct { u32 magic, w[4], eff, sum; } UniSave4;                      // (the last version kept 128 bits)
#define UH_OFF 0xFC0
_Static_assert(UH_OFF+sizeof(UniSave) <= SLOT_STRIDE, "the universe number must end inside the slot");
static void uni_save(void){ UniSave s; s.magic=0x32494E55u; for(int i=0;i<UW;i++) s.w[i]=uni_w[i]; s.eff=galaxy_seed; s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+UH_OFF,&s,sizeof s); }
static void uni_load(void){ UniSave s; sram_read(slot_base(cur_slot)+UH_OFF,&s,sizeof s);   // (galaxy_seed is already loaded: old saves have no block, so they stay in universe galaxy_seed)
    if(s.magic==0x32494E55u && s.sum==fnv(&s,sizeof s-4) && s.eff==galaxy_seed && (s.w[0]^uni_fold(s.w))==galaxy_seed){ uni_set(s.w); return; }
    UniSave4 o; sram_read(slot_base(cur_slot)+UH_OFF,&o,sizeof o);                  // a save from the 128-bit version
    if(o.magic==0x494E5555u && o.sum==fnv(&o,sizeof o-4) && o.eff==galaxy_seed){ u32 u[UW]={0}; for(int i=0;i<4;i++) u[i]=o.w[i]; if((u[0]^uni_fold(u))==galaxy_seed){ uni_set(u); return; } }
    uni_set32(galaxy_seed); }

static void wait_release(void){ do { vsync(); read_keys(); } while(keys); }

// ---------------------------------------------------------------- the bug's save block (same layout as before)
typedef struct {
    u32 magic; u16 ver;
    s16 zone, zone_cleared, endless, world_no, best_depth, food, bugs, fame, portal, spell_sel;
    s64 level, st[4], stat_pts, hp, mp, xp, gold; u32 galaxy_seed;      // 64-bit: level, stats, gold and xp can go to the limit (save format v2: old saves are not read)
    u8 have_bug[6], berry_map[8], seed_map[6], berry_known[8], seed_known[6], spell_known[4];
    Gear eq[3]; u8 bag_n; Gear bag[BAGMAX];
    s16 q_type, q_need, q_have, q_floor, q_done, q_gold, q_fame, q_zone, q_world; char q_name[24];
    s16 pet_form, pet_form_t, pet_maxhp, pet_hp;
    u32 sum;
} Save;
_Static_assert(sizeof(Save) < SG_OFF, "Save must fit before the grind block");
typedef struct { u32 magic; u8 tiers[12]; u16 kills[11]; u16 fish, barrels, mined, dodges, planets, uniq, tier_n, floors; u32 ach; u16 pet_xp, pet_lv; BugLook look; u8 pad; ShipLook ship; u32 base_mask; u16 data_cores, outposts; u32 sum; } SaveG;
_Static_assert(sizeof(SaveG) <= SX_OFF-SG_OFF, "the grind block must end before the ship block");
_Static_assert(__builtin_offsetof(SaveG,sum)==sizeof(SaveG)-4, "SaveG: the sum must be the last 4 bytes");
static u32 sg_sum(const SaveG *g){ return fnv(g,sizeof(SaveG)-4); }
#define LX_OFF 0xA80                                                                   // the newer parts of your bug and your humanoid (after the travel log, before the trophies)
typedef struct { u32 magic; BugX b; HumX h; u32 sum; } LookSave;
_Static_assert(LX_OFF+sizeof(LookSave) <= 0xB00, "the look block must end before the trophies");
#define SLK_OFF (LX_OFF + ((sizeof(LookSave)+3)&~3u))                                  // three saved humanoid looks (the creator's MY LOOKS), right after the look block
static void lookx_save(void){ LookSave l; memset(&l,0,sizeof l); l.magic=0x584B4F4Cu; l.b=bugx; l.h=humx; l.sum=fnv(&l,sizeof l-4); sram_write(slot_base(cur_slot)+LX_OFF,&l,sizeof l); }
static void lookx_load(void){ LookSave l; sram_read(slot_base(cur_slot)+LX_OFF,&l,sizeof l); if(l.magic!=0x584B4F4Cu || l.sum!=fnv(&l,sizeof l-4)){ bugx_default(); humx_default(); return; } bugx=l.b; humx=l.h; }
#define SHX_OFF 0xC40                                                                  // the newer ship parts (module, topper, paint scheme, finish): the gap between the play-time block and the kart blocks
typedef struct { u32 magic; ShipX x; u32 sum; } ShipXSave;
_Static_assert(META_OFF+16 <= SHX_OFF && SHX_OFF+sizeof(ShipXSave) <= 0xD00, "the ship parts block must sit between the play-time block and the kart blocks");
static void shx_save(void){ ShipXSave s; memset(&s,0,sizeof s); s.magic=0x58504853u; s.x=shpx; s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+SHX_OFF,&s,sizeof s); }
static void shx_load(void){ ShipXSave s; sram_read(slot_base(cur_slot)+SHX_OFF,&s,sizeof s); memset(&shpx,0,sizeof shpx);      // (an old save has no block: classic paint, gloss, no module, no topper)
    if(s.magic!=0x58504853u || s.sum!=fnv(&s,sizeof s-4)) return; shpx=s.x; shpx.scheme%=NSCHEME; shpx.finish%=NFINISH; shpx.module%=NMODULE; shpx.topper%=NTOPPER; }
static void grind_save(void){ SaveG g; memset(&g,0,sizeof g); g.magic=0x444E5247; for(int i=0;i<12;i++) g.tiers[i]=tiers[i]; for(int i=0;i<11;i++) g.kills[i]=st_kills[i];
    g.fish=st_fish; g.barrels=st_barrels; g.mined=st_mined; g.dodges=st_dodges; g.planets=st_planets; g.uniq=st_uniq; g.tier_n=st_tiers; g.floors=st_floors; g.ach=ach_done; g.pet_xp=pet_xp; g.pet_lv=pet_lv; g.look=look; g.ship=ship; g.base_mask=base_mask; g.data_cores=data_cores; g.outposts=outposts_done;
    g.sum=sg_sum(&g); sram_write(slot_base(cur_slot)+SG_OFF,&g,sizeof g); lookx_save(); shx_save(); }
static void grind_load(void){ SaveG g; sram_read(slot_base(cur_slot)+SG_OFF,&g,sizeof g);
    if(g.magic!=0x444E5247 || g.sum!=sg_sum(&g)){ for(int i=0;i<12;i++) tiers[i]=0; for(int i=0;i<11;i++) st_kills[i]=0; st_fish=st_barrels=st_mined=st_dodges=st_planets=st_uniq=st_tiers=st_floors=0; ach_done=0; pet_xp=0; pet_lv=0; bug_look_default(); humx_default(); ship_default(); ship_build_model(); base_mask=0; data_cores=0; outposts_done=0; return; }
    for(int i=0;i<12;i++) tiers[i]=g.tiers[i]; for(int i=0;i<11;i++) st_kills[i]=g.kills[i];
    st_fish=g.fish; st_barrels=g.barrels; st_mined=g.mined; st_dodges=g.dodges; st_planets=g.planets; st_uniq=g.uniq; st_tiers=g.tier_n; st_floors=g.floors; ach_done=g.ach; pet_xp=g.pet_xp; pet_lv=g.pet_lv; look=g.look; look.name[8]=0; ship=g.ship; ship.name[8]=0; base_mask=g.base_mask; data_cores=g.data_cores; outposts_done=g.outposts; ship.hull%=NHULL; ship.wings%=NWING; ship.wsize%=3; ship.engines&=3; ship.fins%=3; ship.cannon%=NCANNON; ship.cockpit%=NCOCK; ship.len%=3; ship.wid%=3; ship.nose%=3; ship.wpos%=3; ship.dihed%=3; ship.esize%=3; ship.pods&=3; ship.decal%=NDECAL; shx_load(); ship_build_model(); look.shell%=3; look.ant%=8; look.eye%=8; look.legs%=3; look.pat%=12; look.horn%=6; if(!look.col[LC_WING]) look.col[LC_WING]=rgb15(190,225,255); lookx_load(); }
EW static Save sv;
static u32 save_sum(const Save *s){ return fnv(s, __builtin_offsetof(Save,sum)); }   // up to the sum itself (the struct ends in 4 bytes of padding, so sizeof-4 would hash the sum too)
static int save_sum_old(Save *s){ u32 k=s->sum; s->sum=0; u32 h=fnv(s, sizeof(Save)-4); s->sum=k; return h==k; }   // how older builds summed it (sum zeroed): those saves still load

// ---------------------------------------------------------------- play time (kept beside each slot)
typedef struct { u32 magic, secs, saves, sum; } Meta;
static u32 meta_sum(const Meta *m){ return fnv(m, 12); }
static u32 play_base=0, play_t0=0;
static u32 play_secs(void){ return play_base + ((u32)vbl_count - play_t0)/60; }
static void play_reset(u32 base){ play_base=base; play_t0=(u32)vbl_count; }
static int meta_read(int slot, Meta *m){
    sram_read(slot_base(slot)+META_OFF, m, sizeof(Meta));
    if(m->magic!=0x4154454D || m->sum!=meta_sum(m)){ memset(m,0,sizeof(Meta)); return 0; }
    return 1;
}

// ---------------------------------------------------------------- options (saved at the far end of SRAM)
typedef struct { u32 magic; u8 music, sfx, autosave, shake, last, pad[3]; u32 sum; } Settings;
_Static_assert(__builtin_offsetof(Settings,sum)==sizeof(Settings)-4, "Settings: the sum must be the last 4 bytes");
// ---------------------------------------------------------------- the MORE OPTIONS settings (saved just after the options, at the far end of SRAM)
#define XS_OFF 0x7F40
typedef struct { u32 magic; u8 v[NXO]; u32 sum; } XSet;
_Static_assert(XS_OFF >= SET_OFF+20 && XS_OFF+sizeof(XSet) <= 0x8000, "the extra options must fit between the options and the end of SRAM");
static void xo_apply(void){ SND(0x20)=xo_snd(); }
static void xo_save(void){ XSet s; memset(&s,0,sizeof s); s.magic=0x584F5053; for(int i=0;i<NXO;i++) s.v[i]=xo[i]; s.sum=fnv(&s,sizeof s-4); sram_write(XS_OFF,&s,sizeof s); }
static void xo_load(void){
    XSet s; sram_read(XS_OFF,&s,sizeof s);
    if(s.magic!=0x584F5053 || s.sum!=fnv(&s,sizeof s-4)) return;                                  // never saved: everything stays normal
    for(int i=0;i<NXO;i++) xo[i] = (i<NXOPT && (s.v[i]^XOT[i].def)<XOT[i].n) ? s.v[i] : 0;
}
static void settings_save(void){
    Settings s; memset(&s,0,sizeof(s)); s.magic=0x53544553;
    s.music=opt_music; s.sfx=opt_sfx; s.autosave=opt_autosave; s.shake=opt_shake; s.last=last_slot; s.pad[0]=opt_diff; s.pad[1]=(opt_minimap)|(opt_nums<<1)|(opt_hints<<2)|(opt_arrow<<3)|(opt_insane<<4); s.pad[2]=opt_cam|(opt_baby<<1)|((!opt_hunger)<<2)|((!opt_faintloss)<<3)|(opt_xpm<<4)|(opt_goldm<<6);
    s.sum=fnv(&s,sizeof(s)-4); sram_write(SET_OFF,&s,sizeof(s)); xo_save();
}
static void settings_load(void){
    xo_load(); xo_apply();
    Settings s; sram_read(SET_OFF,&s,sizeof(s));
    if(s.magic!=0x53544553 || s.sum!=fnv(&s,sizeof(s)-4) || s.music>2 || s.last>=NSLOT) return;   // first run: keep the defaults
    opt_music=s.music; opt_sfx=s.sfx&1; opt_autosave=s.autosave&1; opt_shake=s.shake&1; last_slot=s.last; opt_insane=(s.pad[1]>>4)&1; opt_diff=s.pad[0]%6; if(opt_diff==5 && !opt_insane) opt_diff=4; opt_easy=(opt_diff==0); opt_minimap=s.pad[1]&1; opt_nums=(s.pad[1]>>1)&1; opt_hints=(s.pad[1]>>2)&1; opt_arrow=(s.pad[1]>>3)&1; opt_cam=s.pad[2]&1; opt_baby=(s.pad[2]>>1)&1; opt_hunger=!((s.pad[2]>>2)&1); opt_faintloss=!((s.pad[2]>>3)&1); opt_xpm=(s.pad[2]>>4)&3; if(opt_xpm>2) opt_xpm=2; opt_goldm=(s.pad[2]>>6)&3; if(opt_goldm>2) opt_goldm=2;
}
static void music_apply(void){ if(opt_music==0){ SND(0x08)=0; SND(0x12)=0; } }

// ---------------------------------------------------------------- saving and loading
static void save_ext(void); static int load_ext(void);
static int forge_on;
static void do_save(void){        // always saves, into the slot you are playing
    if(forge_on || kw_mode) return;                              // a Dungeon Forge run borrows the world: never save it half-changed. Kart World has no slot
    Save *s=&sv; memset(s,0,sizeof(Save));
    s->magic=0x53475542; s->ver=2;
    s->zone=zone; s->zone_cleared=zone_cleared; s->endless=endless; s->world_no=world_no; s->best_depth=best_depth; s->level=level;
    s->st[0]=st_str; s->st[1]=st_dex; s->st[2]=st_vit; s->st[3]=st_mag; s->stat_pts=stat_pts; s->hp=hearts; s->mp=mp; s->food=food; s->bugs=bugs_freed; s->fame=fame; s->portal=portal_depth; s->spell_sel=spell_sel;
    s->xp=xp; s->gold=gold; s->galaxy_seed=galaxy_seed;
    for(int i=0;i<6;i++) s->have_bug[i]=have_bug[i];
    memcpy(s->berry_map,berry_map,8); memcpy(s->seed_map,seed_map,6); memcpy(s->berry_known,berry_known,8); memcpy(s->seed_known,seed_known,6); { u32 m_=0x80000000u; for(int i=0;i<NSPELL;i++) if(spell_known[i]) m_|=1u<<i; memcpy(s->spell_known,&m_,4); }
    s->eq[0]=eqw; s->eq[1]=eqa; s->eq[2]=eqc; s->bag_n=bag_n; memcpy(s->bag,bag,sizeof(bag));
    s->q_type=q_type<0?0:q_type; s->q_need=q_need; s->q_have=q_have; s->q_floor=q_floor; s->q_done=q_done; s->q_gold=q_gold; s->q_fame=q_fame; s->q_zone=q_zone; s->q_world=q_world; memcpy(s->q_name,q_name,24);
    s->pet_form=pet_form; s->pet_form_t=pet_form_t; s->pet_maxhp=pet_maxhp; s->pet_hp=pet_hp;
    s->sum=save_sum(s);
    sram_write(slot_base(cur_slot), s, sizeof(Save));
    save_ext(); uni_save(); grind_save(); ore_save();
    Meta m, old; meta_read(cur_slot,&old);
    m.magic=0x4154454D; m.secs=play_secs(); m.saves=old.saves+1; m.sum=meta_sum(&m);
    sram_write(slot_base(cur_slot)+META_OFF, &m, sizeof(Meta));
    if(last_slot!=cur_slot){ last_slot=cur_slot; settings_save(); }
}
static void save_game(void){ if(opt_autosave) do_save(); }     // the quiet saves the game does by itself

static int slot_read(int slot){   // loads a slot into sv, and says if it is a good save
    sram_read(slot_base(slot), &sv, sizeof(Save));
    return sv.magic==0x53475542 && sv.ver==2 && (sv.sum==save_sum(&sv) || save_sum_old(&sv));
}
static int load_game(int apply){
    if(!slot_read(cur_slot)) return 0;
    if(!apply) return 1;
    Save *s=&sv;
    zone=s->zone; zone_cleared=s->zone_cleared; endless=s->endless; world_no=s->world_no; best_depth=s->best_depth; level=s->level;
    st_str=s->st[0]; st_dex=s->st[1]; st_vit=s->st[2]; st_mag=s->st[3]; stat_pts=s->stat_pts; food=s->food; bugs_freed=s->bugs; fame=s->fame; portal_depth=s->portal; spell_sel=s->spell_sel;
    xp=s->xp; gold=s->gold; galaxy_seed=s->galaxy_seed; uni_load();
    for(int i=0;i<6;i++) have_bug[i]=s->have_bug[i];
    memcpy(berry_map,s->berry_map,8); memcpy(seed_map,s->seed_map,6); memcpy(berry_known,s->berry_known,8); memcpy(seed_known,s->seed_known,6); { u32 m_; memcpy(&m_,s->spell_known,4); for(int i=0;i<NSPELL;i++) spell_known[i]=0; if(m_>>31){ for(int i=0;i<NSPELL;i++) spell_known[i]=(m_>>i)&1; } else { for(int i=0;i<4;i++) spell_known[i]=s->spell_known[i]; } }
    eqw=s->eq[0]; eqa=s->eq[1]; eqc=s->eq[2]; bag_n=s->bag_n; memcpy(bag,s->bag,sizeof(bag)); bag_sel=0;
    q_type=s->q_type; q_need=s->q_need; q_have=s->q_have; q_floor=s->q_floor; q_done=s->q_done; q_gold=s->q_gold; q_fame=s->q_fame; q_zone=s->q_zone; q_world=s->q_world; memcpy(q_name,s->q_name,24);
    pet_form=s->pet_form; pet_form_t=s->pet_form_t; pet_maxhp=s->pet_maxhp; pet_hp=s->pet_hp; pet_away_t=0; pet_down_t=0;
    load_ext(); grind_load();
    if(endless) make_world(); else story_dna(zone);
    recalc(); hearts=s->hp; mp=s->mp; if(hearts<1) hearts=maxh;
    Meta m; meta_read(cur_slot,&m); play_reset(m.secs);
    last_slot=cur_slot; settings_save();
    return 1;
}
static void slot_erase(int i){ for(int k=0;k<SLOT_STRIDE;k++) SRAM[slot_base(i)+k]=0; }          // (the favourites past the slots are shared: erasing a slot keeps them)
static void slot_copy(int from, int to){ for(int k=0;k<SLOT_STRIDE;k++) SRAM[slot_base(to)+k]=SRAM[slot_base(from)+k]; }

// ---------------------------------------------------------------- a peek at each slot, for the cards
typedef struct { u8 ok; s16 zone, endless, world_no; s64 level, gold, xp; s32 fame; u32 secs; } SlotInfo;
static SlotInfo sinf[NSLOT];
static void slots_refresh(void){
    for(int i=0;i<NSLOT;i++){
        SlotInfo *o=&sinf[i]; memset(o,0,sizeof(SlotInfo));
        if(!slot_read(i)) continue;
        o->ok=1; o->zone=sv.zone; o->endless=sv.endless; o->world_no=sv.world_no; o->level=sv.level; o->gold=sv.gold; o->fame=sv.fame; o->xp=sv.xp;
        if(o->zone<0||o->zone>5) o->zone=0;
        Meta m; meta_read(i,&m); o->secs=m.secs;
    }
}
static int first_good_slot(void){ if(sinf[last_slot].ok) return last_slot; for(int i=0;i<NSLOT;i++) if(sinf[i].ok) return i; return -1; }
static void slot_where(const SlotInfo *o, char *b){ if(o->endless){ scpy(b,"ENDLESS WORLD "); ncat(b,o->world_no); } else scpy(b,tint_name[o->zone]); }
static void fmt_time(char *b, u32 secs){ u32 m=secs/60, h=m/60; b[0]=0; if(h){ ncat(b,(int)h); scat(b,"H "); ncat(b,(int)(m%60)); scat(b,"M"); } else { ncat(b,(int)m); scat(b,"M"); } }

// ---------------------------------------------------------------- little UI parts
static void ctext(volatile u16 *pg, int cx, int y, const char *s, int c){ text(pg,cx-textlen(s)*2,y,s,c,1); }
static void rtext(volatile u16 *pg, int rx, int y, const char *s, int c){ text(pg,rx-textlen(s)*4,y,s,c,1); }
static void stext(volatile u16 *pg, int x, int y, const char *s, int c){ text(pg,x+1,y+1,s,0,1); text(pg,x,y,s,c,1); }    // text with a drop shadow
static void frame1(volatile u16 *pg, int x, int y, int w, int h, int c){ rect(pg,x,y,w,1,c); rect(pg,x,y+h-1,w,1,c); rect(pg,x,y,1,h,c); rect(pg,x+w-1,y,1,h,c); }
// ---------------------------------------------------------------- THE INTERFACE KIT: every menu is built from these
// A soft dithered backdrop, raised panels with rounded corners and a lit top edge, section headings with an accent tick,
// glossy rounded bars, a selection bar with an accent edge, and button chips for the hints ("A: USE" draws an A key).
static void ui_bg(volatile u16 *pg, int y0, int y1){              // the backdrop: three blues, dithered into each other, with a faint dot grid
    int h=y1-y0; if(h<1) return;
    for(int y=y0;y<y1;y++){ int L=(y-y0)*8/h; int ca = L<4 ? U_BG2 : U_BG1, cb = L<4 ? U_BG1 : U_BG0, lv=(L&3);   // lv: how far into the next colour (0..3 quarters)
        static const u8 bay[2][2]={{0,2},{3,1}}; int p0 = bay[y&1][0]<lv ? cb : ca, p1 = bay[y&1][1]<lv ? cb : ca;
        u16 cc=(u16)(p0|(p1<<8)); volatile u16 *row=pg+y*120; for(int k=0;k<120;k++) row[k]=cc;
        if((y&15)==8) for(int x=8;x<240;x+=16){ volatile u16 *p=row+(x>>1); *p=(*p&0xFF00)|U_PANEL; } } }
static void ui_panel(volatile u16 *pg, int x, int y, int w, int h){   // a raised card: dark rim with cut corners, lit top edge, soft inner shadow
    rect(pg,x+1,y+1,w-2,h-2,U_PANEL);
    rect(pg,x+2,y,w-4,1,U_EDGE); rect(pg,x+2,y+h-1,w-4,1,U_EDGE); rect(pg,x,y+2,1,h-4,U_EDGE); rect(pg,x+w-1,y+2,1,h-4,U_EDGE);
    pix(pg,x+1,y+1,U_EDGE); pix(pg,x+w-2,y+1,U_EDGE); pix(pg,x+1,y+h-2,U_EDGE); pix(pg,x+w-2,y+h-2,U_EDGE);
    rect(pg,x+2,y+1,w-4,1,U_SHINE); rect(pg,x+1,y+2,1,h-4,U_RAISE); rect(pg,x+2,y+h-2,w-4,1,U_BG1); }
static void ui_well(volatile u16 *pg, int x, int y, int w, int h){    // a sunken box inside a panel (for pictures and lists)
    rect(pg,x,y,w,h,U_INK); rect(pg,x,y,w,1,U_EDGE); rect(pg,x,y,1,h,U_EDGE); rect(pg,x,y+h-1,w,1,U_RAISE); rect(pg,x+w-1,y,1,h,U_RAISE); }
static void ui_head(volatile u16 *pg, int x, int y, int w, const char *t, int acc){   // a section heading: accent tick, gold title, a rule to the edge
    rect(pg,x,y,2,5,acc); stext(pg,x+5,y,t,C_GOLD); int rx=x+9+textlen(t)*4; if(rx<x+w) rect(pg,rx,y+2,x+w-rx,1,U_RAISE); }
static int ui_glint(int c){ return c==C_RED ? U_GLRED : c==C_CYAN ? U_GLCYAN : c==C_GREEN ? U_GLGREEN : c==C_GOLD ? U_GLGOLD : C_WHITE; }
static void ui_bar(volatile u16 *pg, int x, int y, int w, int h, s64 val, s64 max, int col){   // a rounded glossy bar
    rect(pg,x+1,y,w-2,h,U_TROUGH); rect(pg,x,y+1,1,h-2,U_TROUGH); rect(pg,x+w-1,y+1,1,h-2,U_TROUGH);
    int f=ratio(val,max,w-2); if(f>0){ rect(pg,x+1,y+1,f,h-2,col); rect(pg,x+1,y+1,f,1,ui_glint(col)); } }
static void ui_sel(volatile u16 *pg, int x, int y, int w, int h, int c){   // the selection bar: rounded, with a bright edge on the left
    rect(pg,x+1,y,w-2,h,c); rect(pg,x,y+1,1,h-2,c); rect(pg,x+w-1,y+1,1,h-2,c); rect(pg,x,y+1,2,h-2,U_ACCENT); }
static int ui_chip(volatile u16 *pg, int x, int y, const char *k, int draw){   // a button key: a little raised cap with its name
    int w=textlen(k)*4+5; if(draw){ rect(pg,x+1,y,w-2,9,U_RAISE); rect(pg,x,y+1,1,7,U_RAISE); rect(pg,x+w-1,y+1,1,7,U_RAISE); rect(pg,x+1,y,w-2,1,U_SHINE); rect(pg,x+1,y+8,w-2,1,U_EDGE);
        text(pg,x+3,y+2,k,C_WHITE,1); } return w; }
static int ui_iskey(const char *s, int n){                         // is s[0..n) the name of a button? (only those become key chips)
    static const char *const k[]={"A","B","L","R","SEL","SELECT","START","UP/DOWN","LEFT/RIGHT","L R","L/R","SEL+L/R","HOLD A"};
    for(unsigned i=0;i<sizeof k/sizeof k[0];i++){ const char *p=k[i]; int j=0; while(j<n && p[j] && p[j]==s[j]) j++; if(j==n && !p[j]) return 1; } return 0; }
static int ui_hints(volatile u16 *pg, int x, int y, const char *s, int col, int draw){   // "A: USE  B: DROP" as key chips and words. Returns where it ends
    char seg[48]; int i=0;
    while(s[i]){ int n=0; while(s[i] && !(s[i]==' ' && s[i+1]==' ') && n<47) seg[n++]=s[i++]; seg[n]=0; while(s[i]==' ') i++;
        int c=-1; for(int k=0;k<n;k++) if(seg[k]==':'){ c=k; break; }
        if(c>0 && c<12 && ui_iskey(seg,c)){ seg[c]=0; x+=ui_chip(pg,x,y-2,seg,draw)+3; const char *lab=seg+c+1; while(*lab==' ') lab++; if(draw) text(pg,x,y,lab,col,1); x+=textlen(lab)*4+7; }
        else { if(draw) text(pg,x,y,seg,col,1); x+=n*4+7; } }
    return x; }
static void ui_chint(volatile u16 *pg, int cx, int y, const char *s, int c){   // a hint line, centred, with key chips (plain text if it would not fit)
    int w=ui_hints(pg,0,y,s,c,0)-7; if(w>236){ ctext(pg,cx,y,s,c); return; } int x=cx-w/2; if(x<2) x=2; if(x+w>238) x=238-w; ui_hints(pg,x,y,s,c,1); }
static void ui_lhint(volatile u16 *pg, int x, int y, const char *s, int c){ int w=ui_hints(pg,0,y,s,c,0)-7; if(x+w>238){ text(pg,x,y,s,c,1); return; } ui_hints(pg,x,y,s,c,1); }
static void ui_foot(volatile u16 *pg, const char *left, const char *right){   // the hint strip along the bottom
    rect(pg,0,149,240,11,U_BG0); rect(pg,0,149,240,1,U_RAISE);
    int e = left && left[0] ? ui_hints(pg,4,152,left,U_SOFT,1) : 4;
    if(right){ int w=ui_hints(pg,0,152,right,C_GREY,0); if(e+4<236-w) ui_hints(pg,236-w,152,right,C_GREY,1); } }
static void panel(volatile u16 *pg, int x, int y, int w, int h, const char *title){   // a titled window
    ui_panel(pg,x,y,w,h); rect(pg,x+2,y+2,w-4,11,U_RAISE); rect(pg,x+2,y+2,w-4,1,U_SHINE); rect(pg,x+2,y+13,w-4,1,U_EDGE);
    rect(pg,x+5,y+5,2,5,U_ACCENT); stext(pg,x+10,y+5,title,C_GOLD);
}
static void hbar(volatile u16 *pg, int x, int y, int w, s64 val, s64 max, int col){
    rect(pg,x,y,w,3,U_TROUGH); int f = ratio(val,max,w); if(f){ rect(pg,x,y,f,3,col); rect(pg,x,y,f,1,ui_glint(col)); }
}
static int confirm(volatile u16 *pg, const char *l1, const char *l2){
    panel(pg,30,60,180,58,"ARE YOU SURE?");
    ctext(pg,120,80,l1,C_WHITE); if(l2) ctext(pg,120,89,l2,U_SOFT);
    { int w=ui_hints(pg,0,104,"A: YES  B: NO",C_WHITE,0); ui_hints(pg,120-w/2,104,"A: YES  B: NO",C_WHITE,1); }
    wait_release();
    while(1){ vsync(); read_keys(); if(HIT(K_A)){ sfx(2); wait_release(); return 1; } if(HIT(K_B)){ wait_release(); return 0; } }
}

// ---------------------------------------------------------------- the slot cards
enum { SP_LOAD, SP_NEW, SP_SAVE, SP_COPY };
static void slot_card(volatile u16 *pg, int i, int y, int sel, int mode){
    const SlotInfo *o=&sinf[i]; const int cx=18, w=204, h=33; char b[40];
    rect(pg,cx,y,w,h,0);
    frame1(pg,cx,y,w,h, sel?C_GOLD:C_DGREY); if(sel) frame1(pg,cx+1,y+1,w-2,h-2,C_DRED);
    scpy(b,"SLOT "); ncat(b,i+1); text(pg,cx+8,y+5,b,sel?C_GOLD:C_GREY,1);
    if(in_game && i==cur_slot) text(pg,cx+44,y+5,"IN USE",C_CYAN,1);
    if(!o->ok){
        ctext(pg,cx+w/2,y+17,"- EMPTY -",sel?C_WHITE:C_DGREY);
        if(sel && mode!=SP_LOAD) ui_chint(pg,cx+w/2,y+25,"A: USE THIS SLOT",C_CYAN);
        return;
    }
    fmt_time(b,o->secs); rtext(pg,cx+w-8,y+5,b,C_GREY);
    slot_where(o,b); text(pg,cx+8,y+15,b,sel?C_WHITE:C_GREY,1);
    scpy(b,"LV "); ncat(b,o->level); rtext(pg,cx+w-8,y+15,b,C_GOLD);
    scpy(b,"G "); ncat(b,o->gold); scat(b,"  FAME "); ncat(b,o->fame); text(pg,cx+8,y+24,b,sel?C_GREY:C_DGREY,1);
    s64 need=sadd(smul(smul(o->level,o->level),18),12); hbar(pg,cx+w-8-48,y+25,48,o->xp,need,C_GOLD);
}
static int slot_picker(volatile u16 *pg, int mode, int src){
    static const char *const ttl[4]={"LOAD GAME","NEW GAME","SAVE GAME","COPY TO WHICH SLOT?"};
    static const char *const hlp[4]={"A: LOAD  SELECT: ERASE  R: COPY  B: BACK","A: PLAY HERE  SELECT: ERASE  B: BACK","A: SAVE  SELECT: ERASE  R: COPY  B: BACK","A: COPY HERE  B: BACK"};
    slots_refresh(); oam_hide_all(); oam_flush(); if(in_game) rect(pg,0,18,240,142,0);
    int sel = mode==SP_COPY ? (src+1)%NSLOT : cur_slot;
    if(mode==SP_LOAD && !sinf[sel].ok){ for(int k=0;k<NSLOT;k++) if(sinf[k].ok){ sel=k; break; } }
    int dirty=1; wait_release();
    while(1){
        if(dirty){ dirty=0;
            panel(pg,12,20,216,136,ttl[mode]);
            for(int i=0;i<NSLOT;i++) slot_card(pg,i,35+i*36,i==sel,mode);
            ctext(pg,120,146,hlp[mode],C_CYAN);
        }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NSLOT-1)%NSLOT; sfx(7); dirty=1; }
        if(HIT(K_DOWN)){ sel=(sel+1)%NSLOT; sfx(7); dirty=1; }
        if(HIT(K_B)){ wait_release(); return -1; }
        if(HIT(K_A)){
            int ok=sinf[sel].ok; dirty=1;
            if(mode==SP_LOAD){
                if(!ok) sfx(0);
                else if(!in_game || confirm(pg,"LOAD THIS GAME?","UNSAVED PROGRESS IS LOST")){ sfx(2); wait_release(); return sel; }
            } else if(mode==SP_COPY){
                if(sel==src) sfx(0);
                else if(!ok || confirm(pg,"OVERWRITE THIS SLOT?","THE OLD GAME IS GONE")){ wait_release(); return sel; }
            } else {
                if(!ok || confirm(pg,"OVERWRITE THIS SLOT?","THE OLD GAME IS GONE")){ sfx(2); wait_release(); return sel; }
            }
        }
        if(mode!=SP_COPY && HIT(K_SEL) && sinf[sel].ok){
            if(confirm(pg,"ERASE THIS SLOT?", (in_game && sel==cur_slot) ? "YOU ARE PLAYING IT RIGHT NOW" : "THIS CANNOT BE UNDONE")){ slot_erase(sel); slots_refresh(); sfx(4); }
            dirty=1;
        }
        if((mode==SP_LOAD||mode==SP_SAVE) && HIT(K_R) && sinf[sel].ok){
            int t=slot_picker(pg,SP_COPY,sel);
            if(t>=0){ slot_copy(sel,t); sfx(6); }
            slots_refresh(); dirty=1;
        }
    }
}

// ---------------------------------------------------------------- the MORE OPTIONS screen (press SELECT on the options screen)
static void options_more(volatile u16 *pg){
    int sel=0, top=0, dirty=1; oam_hide_all(); oam_flush(); if(in_game) rect(pg,0,18,240,142,0); wait_release();
    while(1){
        if(sel<top) top=sel; if(sel>=top+10) top=sel-9;
        if(dirty){ dirty=0; char b[24], t[32];
            scpy(t,"MORE: "); scat(t,xo_gname[XOT[sel].grp]); panel(pg,12,20,216,136,t);
            for(int r=0;r<10 && top+r<NXOPT;r++){ int i=top+r, y=37+r*9, v=xo_i(i);
                if(i==sel) rect(pg,18,y-2,204,10,6);
                text(pg,26,y,XOT[i].name,i==sel?C_WHITE:C_GREY,1);
                scpy(b,"< "); scat(b,XOT[i].lab[v]); scat(b," >");
                rtext(pg,206,y,b, i==sel?C_GOLD : v!=XOT[i].def ? C_CYAN : C_GREY);
            }
            if(top>0) text(pg,216,37,"^",C_CYAN,1); if(top+10<NXOPT) text(pg,216,118,"v",C_CYAN,1);
            rect(pg,18,128,204,1,C_DGREY);
            { const char *h=XOT[sel].help; int L=textlen(h); if(L>52){ char h1[56]; int k=0; while(k<52 && h[k]) k++; while(k>0 && h[k]!=' ') k--; for(int j=0;j<k;j++) h1[j]=h[j]; h1[k]=0; ctext(pg,120,132,h1,C_GREY); ctext(pg,120,140,h+k+1,C_GREY); } else ctext(pg,120,134,h,C_GREY); }
            ctext(pg,120,148,"<>: CHANGE  L/R: GROUP  SEL: RESET  B: BACK",C_CYAN);
        }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NXOPT-1)%NXOPT; sfx(7); dirty=1; }
        if(HIT(K_DOWN)){ sel=(sel+1)%NXOPT; sfx(7); dirty=1; }
        if(HIT(K_R)){ int g=XOT[sel].grp; while(sel<NXOPT && XOT[sel].grp==g) sel++; if(sel>=NXOPT) sel=0; sfx(7); dirty=1; }
        if(HIT(K_L)){ int g=XOT[sel].grp; while(sel>0 && XOT[sel].grp==g) sel--; g=XOT[sel].grp; while(sel>0 && XOT[sel-1].grp==g) sel--; sfx(7); dirty=1; }
        int d = HIT(K_RIGHT)||HIT(K_A) ? 1 : HIT(K_LEFT) ? -1 : 0;
        if(d){ int n=XOT[sel].n; xo[sel]=(u8)(((xo_i(sel)+d+n)%n)^XOT[sel].def); xo_apply(); sfx(2); if(sel==XO_RUMBLE||sel==XO_RUMEV||sel==XO_RUMHUM) rumble(RP_TEST); dirty=1; }   // (a test buzz when you change a rumble setting)
        if(HIT(K_SEL)){ if(confirm(pg,"RESET MORE OPTIONS?","BACK TO THE NORMAL SETTINGS")){ for(int i=0;i<NXO;i++) xo[i]=0; xo_apply(); sfx(4); } dirty=1; }
        if(HIT(K_B)||HIT(K_START)){ settings_save(); wait_release(); return; }
    }
}

// ---------------------------------------------------------------- the difficulty details: every number the chosen difficulty changes, read from the same tables the game uses
static void dif_line(volatile u16 *pg, int y, const char *lab, const char *val, int c){ text(pg,26,y,lab,C_GREY,1); rtext(pg,206,y,val,c); }
static void dif_pct(char *b, int v){ b[0]=0; ncat(b,v); scat(b,"%"); }
static void dif_details(volatile u16 *pg){
    char b[40]; int y=47, d=opt_diff;
    if(opt_baby){
        dif_line(pg,y,"DAMAGE YOU TAKE","1 AT MOST",C_GREEN); y+=7; dif_line(pg,y,"HUNGER","NEVER",C_GREEN); y+=7; dif_line(pg,y,"FAINTING","COSTS NOTHING",C_GREEN); y+=7;
        dif_line(pg,y,"GOLD AND XP","X2",C_GREEN); y+=7; dif_line(pg,y,"BUDDY","HEALS YOUR BOO-BOOS",C_GREEN); return; }
    static const char *const boss[6]={"NONE","1 PER 2 TIERS","1 PER 2 TIERS","3 PER 4 TIERS","1 PER TIER+","2+ PER TIER"};
    scpy(b,d==0?"HALF":""); if(d){ b[0]=0; ncat(b,dif_dmg[d]/2); scat(b,"X"); } dif_line(pg,y,"DAMAGE YOU TAKE",b,d==0?C_GREEN:d>=2?C_RED:C_WHITE); y+=7;
    dif_pct(b,dif_hp[d]); if(d){ scat(b," +"); ncat(b,d*6); scat(b,"% A ZONE"); } dif_line(pg,y,"FOE HEALTH",b,d>=2?C_RED:C_WHITE); y+=7;
    dif_pct(b,dif_spd[d]); dif_line(pg,y,"FOE SPEED",b,d>=2?C_RED:C_WHITE); y+=7;
    dif_pct(b,dif_aw[d]); dif_line(pg,y,"HOW FAR FOES SEE YOU",b,d>=2?C_RED:C_WHITE); y+=7;
    dif_pct(b,200-dif_cd[d]); dif_line(pg,y,"FOES ATTACK AGAIN AT",b,d>=2?C_RED:C_WHITE); y+=7;
    dif_line(pg,y,"PACKS FLANK YOU",d>=2?"YES":"NO",d>=2?C_RED:C_WHITE); y+=7;
    if(d){ int t=d*1000/60; b[0]=0; scat(b,"+"); ncat(b,t/10); scat(b,"."); ncat(b,t%10); scat(b,"% A LEVEL DEEP"); } else scpy(b,"NORMAL"); dif_line(pg,y,"FOE BITES GROW",b,d?C_RED:C_WHITE); y+=7;
    b[0]=0; scat(b,"+"); ncat(b,d>=2?(d-1)*25:0); scat(b,"% / "); ncat(b,8+d*3); scat(b,"% CHAMPS"); dif_line(pg,y,"MORE FOES / CHAMPIONS",b,d>=2?C_RED:C_WHITE); y+=7;
    dif_line(pg,y,"EXTRA BOSSES",boss[d],d>=3?C_RED:C_WHITE); y+=7;
    { int hx = d==0 ? 7 : 120/dif_hun[d]; b[0]=0; scat(b,"X"); ncat(b,hx/10); scat(b,"."); ncat(b,hx%10); dif_line(pg,y,"HUNGER",b,d==0?C_GREEN:C_RED); y+=7; }
    b[0]=0; scat(b,"+"); ncat(b,dif_gold[d]); scat(b,"%"); dif_line(pg,y,"GOLD PAID",b,C_GREEN);
}
// ---------------------------------------------------------------- the options screen
static void options_screen(volatile u16 *pg){
    enum { OP_DIFF, OP_HUNGER, OP_FAINT, OP_XP, OP_GOLD, OP_CAM, OP_MUSIC, OP_SFX, OP_AUTO, OP_SHAKE, OP_MAP, OP_NUMS, OP_HINTS, OP_ARROW, NOPT };
    static const char *const nm[NOPT]={"DIFFICULTY","HUNGER","LOSE STUFF WHEN YOU FAINT","XP SPEED","GOLD","PLANET CAMERA","MUSIC","SOUND EFFECTS","AUTOSAVE","SCREEN SHAKE","MINIMAP","DAMAGE NUMBERS","BUTTON HINTS","GUIDE ARROW"};
    static const char *const hlp[NOPT]={"BABY: 1 DAMAGE, NO HUNGER, NOTHING LOST, 2X GOLD AND XP. EASY: HALF DAMAGE. HARD UP: MORE DAMAGE, MORE GOLD","OFF: NOBODY EVER STARVES","OFF: FAINTING COSTS NO GOLD AND NO BAG","HOW FAST YOU LEVEL UP","HOW MUCH GOLD EVERYTHING PAYS","WHICH VIEW A NEW GAME STARTS WITH","THE CHIPTUNE TUNES","BITES, HITS AND PICKUPS","SAVES AT THE SHIP, AFTER DUNGEONS AND AT STATIONS","THE JOLT WHEN YOU GET HURT","THE LITTLE MAP IN THE DUNGEON CORNER","NUMBERS THAT FLY OFF THINGS YOU BITE","THE 'A: ...' LINES THAT SAY WHAT A DOES","THE YELLOW ARROW ON PLANETS"};
    static const char *const dif[7]={"BABY","EASY","NORMAL","HARD","BRUTAL","NIGHTMARE","INSANE-SECT"}, *const cam[2]={"3RD PERSON","1ST PERSON"}, *const mv[3]={"OFF","LOW","FULL"}, *const yn[2]={"OFF","ON"}, *const mx[3]={"X1","X2","X5"};
    int sel=0, top=0, dirty=1; oam_hide_all(); oam_flush(); if(in_game) rect(pg,0,18,240,142,0); wait_release();
    while(1){
        if(sel<top) top=sel; if(sel>=top+10) top=sel-9;
        if(dirty){ dirty=0; char b[20];
            panel(pg,12,20,216,136,"OPTIONS");
            for(int r=0;r<10;r++){ int i=top+r; if(sel==OP_DIFF && r>0) break;       // (on DIFFICULTY the rows under it give way to its details)
                int y=37+r*9; int dsel = opt_baby ? 0 : opt_diff+1; int *pv = i==OP_DIFF?&dsel : i==OP_HUNGER?&opt_hunger : i==OP_FAINT?&opt_faintloss : i==OP_XP?&opt_xpm : i==OP_GOLD?&opt_goldm : i==OP_CAM?&opt_cam : i==OP_MUSIC?&opt_music : i==OP_SFX?&opt_sfx : i==OP_AUTO?&opt_autosave : i==OP_SHAKE?&opt_shake : i==OP_MAP?&opt_minimap : i==OP_NUMS?&opt_nums : i==OP_HINTS?&opt_hints : &opt_arrow;
                int v=*pv; if(i==sel) rect(pg,18,y-2,204,10,6);
                text(pg,26,y,nm[i],i==sel?C_WHITE:C_GREY,1);
                scpy(b,"< "); scat(b, i==OP_DIFF?dif[v] : i==OP_CAM?cam[v] : i==OP_MUSIC?mv[v] : (i==OP_XP||i==OP_GOLD)?mx[v] : yn[v]); scat(b," >");
                rtext(pg,206,y,b, i==sel?C_GOLD : (i==OP_DIFF ? (v<=1?C_GREEN:v>=3?C_RED:C_GREY) : C_GREY));
            }
            if(sel==OP_DIFF) dif_details(pg); else { if(top>0) text(pg,216,37,"^",C_CYAN,1); if(top+10<NOPT) text(pg,216,118,"v",C_CYAN,1); }
            rect(pg,18,128,204,1,C_DGREY);
            { const char *h=hlp[sel]; char hb[112];
              if(sel==OP_DIFF){      // the tip for the difficulty you are ON
                  static const char *const tg[6]={"A GENTLE TRIP: HALF DAMAGE AND SLOWER HUNGER","THE INTENDED GAME: FOES HIT HARD AND HUNT YOU DOWN","FOES FLANK YOU IN PACKS AND RECOVER FASTER","TOUGH FAST FOES, MORE CHAMPIONS, MORE BOSSES","ONE MISTAKE IS ENOUGH. BEAT THE ANT KING HERE TO UNLOCK INSANE-SECT","54 TIERS AND A MOB OF BOSSES. GOOD LUCK, BUG"};
                  h = opt_baby ? "FOR BABIES, REALLY: NOTHING CAN HURT YOU MUCH" : tg[opt_diff]; }
              int L=textlen(h); if(L>52){ char h1[56]; int k=0; while(k<52 && h[k]) k++; while(k>0 && h[k]!=' ') k--; for(int j=0;j<k;j++) h1[j]=h[j]; h1[k]=0; ctext(pg,120,132,h1,C_GREY); ctext(pg,120,140,h+k+1,C_GREY); } else ctext(pg,120,134,h,C_GREY); }
            ui_chint(pg,120,148,"LEFT/RIGHT: CHANGE  SELECT: MORE  B: BACK",C_CYAN);
        }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NOPT-1)%NOPT; sfx(7); dirty=1; }
        if(HIT(K_DOWN)){ sel=(sel+1)%NOPT; sfx(7); dirty=1; }
        int d = HIT(K_RIGHT)||HIT(K_A) ? 1 : HIT(K_LEFT) ? -1 : 0;
        if(d){ dirty=1;
            if(sel==OP_DIFF){ int nd=opt_insane?7:6, ds=((opt_baby?0:opt_diff+1)+d+nd)%nd; opt_baby=(ds==0); opt_diff=ds?ds-1:0; opt_easy=(opt_diff==0); }
            else if(sel==OP_HUNGER) opt_hunger^=1; else if(sel==OP_FAINT) opt_faintloss^=1;
            else if(sel==OP_XP) opt_xpm=(opt_xpm+d+3)%3; else if(sel==OP_GOLD) opt_goldm=(opt_goldm+d+3)%3;
            else if(sel==OP_CAM) opt_cam^=1;
            else if(sel==OP_MUSIC){ opt_music=(opt_music+d+3)%3; music_apply(); }
            else if(sel==OP_SFX) opt_sfx^=1; else if(sel==OP_AUTO) opt_autosave^=1; else if(sel==OP_SHAKE) opt_shake^=1;
            else if(sel==OP_MAP) opt_minimap^=1; else if(sel==OP_NUMS) opt_nums^=1; else if(sel==OP_HINTS) opt_hints^=1; else opt_arrow^=1;
            sfx(2);
        }
        if(HIT(K_SEL)){ options_more(pg); dirty=1; }
        if(HIT(K_B)||HIT(K_START)){ settings_save(); wait_release(); return; }
    }
}
