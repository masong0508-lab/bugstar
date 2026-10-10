// ================================================================= SAVE SYSTEM: three slots on the cartridge, options, and the menus for them
// SRAM map (32 KB):  slot n lives at n*0x1000:  +0x000 Save (the bug)  +0x800 SaveX (the ship, see space.inc)  +0xC00 Meta (play time)
//                    options live at 0x7F00.  Slot 1 sits exactly where the old single save did, so old saves still load.
__attribute__((used, aligned(4))) const char sram_tag[]="SRAM_V113";
#define SRAM ((volatile u8*)0x0E000000)
#define NSLOT 3
#define SLOT_STRIDE 0x1000
#define SG_OFF 0x400
#define SX_OFF 0x800
#define META_OFF 0xC00
#define SET_OFF 0x7F00
static int slot_base(int i){ return i*SLOT_STRIDE; }
#define XT_BASE 0x3000                  // spare SRAM above the three slots: each slot also owns 0x1400 bytes here for the upper cluster words
#define XT_STRIDE 0x1400
static int xt_base(int i){ return XT_BASE+i*XT_STRIDE; }
static u32 fnv(const void *d, unsigned n){ const u8 *p=d; u32 h=2166136261u; for(unsigned i=0;i<n;i++){ h^=p[i]; h*=16777619u; } return h; }
static void sram_read(int off, void *d, unsigned n){ u8 *p=d; for(unsigned i=0;i<n;i++) p[i]=SRAM[off+i]; }
static void sram_write(int off, const void *s, unsigned n){ const u8 *p=s; for(unsigned i=0;i<n;i++) SRAM[off+i]=p[i]; }

static void wait_release(void){ do { vsync(); read_keys(); } while(keys); }

// ---------------------------------------------------------------- the bug's save block (same layout as before)
typedef struct {
    u32 magic; u16 ver;
    s16 zone, zone_cleared, endless, world_no, best_depth, level, st[4], stat_pts, hp, mp, food, bugs, fame, portal, spell_sel;
    s32 xp, gold; u32 galaxy_seed;
    u8 have_bug[6], berry_map[8], seed_map[6], berry_known[8], seed_known[6], spell_known[NSPELL];
    Gear eq[3]; u8 bag_n; Gear bag[BAGMAX];
    s16 q_type, q_need, q_have, q_floor, q_done, q_gold, q_fame, q_zone, q_world; char q_name[24];
    s16 pet_form, pet_form_t, pet_maxhp, pet_hp;
    u32 sum;
} Save;
_Static_assert(sizeof(Save) < SG_OFF, "Save must fit before the grind block");
typedef struct { u32 magic; u8 tiers[6]; u16 kills[11]; u16 fish, barrels, mined, dodges, planets, uniq, tier_n, floors; u32 ach; u16 pet_xp, pet_lv; BugLook look; u8 pad; ShipLook ship; u32 base_mask; u16 data_cores, outposts; u32 sum; } SaveG;
static u32 sg_sum(const SaveG *g){ return fnv(g,sizeof(SaveG)-4); }
static void grind_save(void){ SaveG g; memset(&g,0,sizeof g); g.magic=0x444E5247; for(int i=0;i<6;i++) g.tiers[i]=tiers[i]; for(int i=0;i<11;i++) g.kills[i]=st_kills[i];
    g.fish=st_fish; g.barrels=st_barrels; g.mined=st_mined; g.dodges=st_dodges; g.planets=st_planets; g.uniq=st_uniq; g.tier_n=st_tiers; g.floors=st_floors; g.ach=ach_done; g.pet_xp=pet_xp; g.pet_lv=pet_lv; g.look=look; g.ship=ship; g.base_mask=base_mask; g.data_cores=data_cores; g.outposts=outposts_done;
    g.sum=sg_sum(&g); sram_write(slot_base(cur_slot)+SG_OFF,&g,sizeof g); }
static void grind_load(void){ SaveG g; sram_read(slot_base(cur_slot)+SG_OFF,&g,sizeof g);
    if(g.magic!=0x444E5247 || g.sum!=sg_sum(&g)){ for(int i=0;i<6;i++) tiers[i]=0; for(int i=0;i<11;i++) st_kills[i]=0; st_fish=st_barrels=st_mined=st_dodges=st_planets=st_uniq=st_tiers=st_floors=0; ach_done=0; pet_xp=0; pet_lv=0; bug_look_default(); ship_default(); ship_build_model(); base_mask=0; data_cores=0; outposts_done=0; return; }
    for(int i=0;i<6;i++) tiers[i]=g.tiers[i]; for(int i=0;i<11;i++) st_kills[i]=g.kills[i];
    st_fish=g.fish; st_barrels=g.barrels; st_mined=g.mined; st_dodges=g.dodges; st_planets=g.planets; st_uniq=g.uniq; st_tiers=g.tier_n; st_floors=g.floors; ach_done=g.ach; pet_xp=g.pet_xp; pet_lv=g.pet_lv; look=g.look; look.name[8]=0; ship=g.ship; ship.name[8]=0; base_mask=g.base_mask; data_cores=g.data_cores; outposts_done=g.outposts; ship.hull%=NHULL; ship.wings%=NWING; ship.wsize%=3; ship.engines&=3; ship.fins%=3; ship.cannon&=3; ship.cockpit&=1; ship_build_model(); look.shell%=3; look.ant%=3; look.eye&=1; look.legs%=3; look.pat%=5; look.horn&=1; }
EW static Save sv;
static u32 save_sum(const Save *s){ return fnv(s, sizeof(Save)-4); }

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
static void settings_save(void){
    Settings s; memset(&s,0,sizeof(s)); s.magic=0x53544553;
    s.music=opt_music; s.sfx=opt_sfx; s.autosave=opt_autosave; s.shake=opt_shake; s.last=last_slot; s.pad[0]=opt_diff; s.pad[1]=(opt_minimap)|(opt_nums<<1)|(opt_hints<<2)|(opt_arrow<<3); s.pad[2]=opt_cam;
    s.sum=fnv(&s,sizeof(s)-4); sram_write(SET_OFF,&s,sizeof(s));
}
static void settings_load(void){
    Settings s; sram_read(SET_OFF,&s,sizeof(s));
    if(s.magic!=0x53544553 || s.sum!=fnv(&s,sizeof(s)-4) || s.music>2 || s.last>=NSLOT) return;   // first run: keep the defaults
    opt_music=s.music; opt_sfx=s.sfx&1; opt_autosave=s.autosave&1; opt_shake=s.shake&1; last_slot=s.last; opt_diff=s.pad[0]%3; opt_easy=(opt_diff==0); opt_minimap=s.pad[1]&1; opt_nums=(s.pad[1]>>1)&1; opt_hints=(s.pad[1]>>2)&1; opt_arrow=(s.pad[1]>>3)&1; opt_cam=s.pad[2]&1;
}
static void music_apply(void){ if(opt_music==0){ SND(0x08)=0; SND(0x12)=0; } }

// ---------------------------------------------------------------- saving and loading
static void save_ext(void); static int load_ext(void);
static void do_save(void){        // always saves, into the slot you are playing
    Save *s=&sv; memset(s,0,sizeof(Save));
    s->magic=0x53475542; s->ver=1;
    s->zone=zone; s->zone_cleared=zone_cleared; s->endless=endless; s->world_no=world_no; s->best_depth=best_depth; s->level=level;
    s->st[0]=st_str; s->st[1]=st_dex; s->st[2]=st_vit; s->st[3]=st_mag; s->stat_pts=stat_pts; s->hp=hearts; s->mp=mp; s->food=food; s->bugs=bugs_freed; s->fame=fame; s->portal=portal_depth; s->spell_sel=spell_sel;
    s->xp=xp; s->gold=gold; s->galaxy_seed=galaxy_seed;
    for(int i=0;i<6;i++) s->have_bug[i]=have_bug[i];
    memcpy(s->berry_map,berry_map,8); memcpy(s->seed_map,seed_map,6); memcpy(s->berry_known,berry_known,8); memcpy(s->seed_known,seed_known,6); memcpy(s->spell_known,spell_known,NSPELL);
    s->eq[0]=eqw; s->eq[1]=eqa; s->eq[2]=eqc; s->bag_n=bag_n; memcpy(s->bag,bag,sizeof(bag));
    s->q_type=q_type<0?0:q_type; s->q_need=q_need; s->q_have=q_have; s->q_floor=q_floor; s->q_done=q_done; s->q_gold=q_gold; s->q_fame=q_fame; s->q_zone=q_zone; s->q_world=q_world; memcpy(s->q_name,q_name,24);
    s->pet_form=pet_form; s->pet_form_t=pet_form_t; s->pet_maxhp=pet_maxhp; s->pet_hp=pet_hp;
    s->sum=save_sum(s);
    sram_write(slot_base(cur_slot), s, sizeof(Save));
    save_ext(); grind_save();
    Meta m, old; meta_read(cur_slot,&old);
    m.magic=0x4154454D; m.secs=play_secs(); m.saves=old.saves+1; m.sum=meta_sum(&m);
    sram_write(slot_base(cur_slot)+META_OFF, &m, sizeof(Meta));
    if(last_slot!=cur_slot){ last_slot=cur_slot; settings_save(); }
}
static void save_game(void){ if(opt_autosave) do_save(); }     // the quiet saves the game does by itself

static int slot_read(int slot){   // loads a slot into sv, and says if it is a good save
    sram_read(slot_base(slot), &sv, sizeof(Save));
    return sv.magic==0x53475542 && sv.ver==1 && sv.sum==save_sum(&sv);
}
static int load_game(int apply){
    if(!slot_read(cur_slot)) return 0;
    if(!apply) return 1;
    Save *s=&sv;
    zone=s->zone; zone_cleared=s->zone_cleared; endless=s->endless; world_no=s->world_no; best_depth=s->best_depth; level=s->level;
    st_str=s->st[0]; st_dex=s->st[1]; st_vit=s->st[2]; st_mag=s->st[3]; stat_pts=s->stat_pts; food=s->food; bugs_freed=s->bugs; fame=s->fame; portal_depth=s->portal; spell_sel=s->spell_sel;
    xp=s->xp; gold=s->gold; galaxy_seed=s->galaxy_seed;
    for(int i=0;i<6;i++) have_bug[i]=s->have_bug[i];
    memcpy(berry_map,s->berry_map,8); memcpy(seed_map,s->seed_map,6); memcpy(berry_known,s->berry_known,8); memcpy(seed_known,s->seed_known,6); memcpy(spell_known,s->spell_known,NSPELL);
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
static void slot_erase(int i){ for(int k=0;k<SLOT_STRIDE;k++) SRAM[slot_base(i)+k]=0; for(int k=0;k<XT_STRIDE;k++) SRAM[xt_base(i)+k]=0; }
static void slot_copy(int from, int to){ for(int k=0;k<SLOT_STRIDE;k++) SRAM[slot_base(to)+k]=SRAM[slot_base(from)+k]; for(int k=0;k<XT_STRIDE;k++) SRAM[xt_base(to)+k]=SRAM[xt_base(from)+k]; }

// ---------------------------------------------------------------- a peek at each slot, for the cards
typedef struct { u8 ok; s16 zone, endless, world_no, level; s32 gold, fame, xp; u32 secs; } SlotInfo;
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
static void panel(volatile u16 *pg, int x, int y, int w, int h, const char *title){
    rect(pg,x,y,w,h,0); frame1(pg,x,y,w,h,6);
    rect(pg,x+1,y+1,w-2,11,C_DGREY); rect(pg,x+1,y+12,w-2,1,6);
    text(pg,x+7,y+4,title,0,1); text(pg,x+6,y+3,title,C_GOLD,1);
}
static void hbar(volatile u16 *pg, int x, int y, int w, int val, int max, int col){
    rect(pg,x,y,w,3,C_DGREY); int f = max>0 ? val*w/max : 0; if(f>w) f=w; if(f<0) f=0; if(f) rect(pg,x,y,f,3,col);
}
static int confirm(volatile u16 *pg, const char *l1, const char *l2){
    panel(pg,30,62,180,54,"ARE YOU SURE?");
    ctext(pg,120,80,l1,C_WHITE); if(l2) ctext(pg,120,89,l2,C_GREY);
    ctext(pg,120,103,"A: YES    B: NO",C_CYAN);
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
        if(sel && mode!=SP_LOAD) ctext(pg,cx+w/2,y+25,"A: USE THIS SLOT",C_CYAN);
        return;
    }
    fmt_time(b,o->secs); rtext(pg,cx+w-8,y+5,b,C_GREY);
    slot_where(o,b); text(pg,cx+8,y+15,b,sel?C_WHITE:C_GREY,1);
    scpy(b,"LV "); ncat(b,o->level); rtext(pg,cx+w-8,y+15,b,C_GOLD);
    scpy(b,"G "); ncat(b,o->gold); scat(b,"  FAME "); ncat(b,o->fame); text(pg,cx+8,y+24,b,sel?C_GREY:C_DGREY,1);
    int need=(6*o->level*o->level+4)*3; hbar(pg,cx+w-8-48,y+25,48,o->xp,need,C_GOLD);
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

// ---------------------------------------------------------------- the options screen
static void options_screen(volatile u16 *pg){
    enum { OP_DIFF, OP_CAM, OP_MUSIC, OP_SFX, OP_AUTO, OP_SHAKE, OP_MAP, OP_NUMS, OP_HINTS, OP_ARROW, NOPT };
    static const char *const nm[NOPT]={"DIFFICULTY","PLANET CAMERA","MUSIC","SOUND EFFECTS","AUTOSAVE","SCREEN SHAKE","MINIMAP","DAMAGE NUMBERS","BUTTON HINTS","GUIDE ARROW"};
    static const char *const hlp[NOPT]={"EASY: HALF DAMAGE, KEEP YOUR BAG. HARD: MORE DAMAGE, +25% GOLD","WHICH VIEW A NEW GAME STARTS WITH","THE CHIPTUNE TUNES","BITES, HITS AND PICKUPS","SAVES AT THE SHIP, AFTER DUNGEONS AND AT STATIONS","THE JOLT WHEN YOU GET HURT","THE LITTLE MAP IN THE DUNGEON CORNER","NUMBERS THAT FLY OFF THINGS YOU BITE","THE 'A: ...' LINES THAT SAY WHAT A DOES","THE YELLOW ARROW ON PLANETS"};
    static const char *const dif[3]={"EASY","NORMAL","HARD"}, *const cam[2]={"3RD PERSON","1ST PERSON"}, *const mv[3]={"OFF","LOW","FULL"}, *const yn[2]={"OFF","ON"};
    int sel=0, dirty=1; oam_hide_all(); oam_flush(); if(in_game) rect(pg,0,18,240,142,0); wait_release();
    while(1){
        if(dirty){ dirty=0; char b[20];
            panel(pg,12,20,216,136,"OPTIONS");
            for(int i=0;i<NOPT;i++){
                int y=37+i*9; int *pv = i==OP_DIFF?&opt_diff : i==OP_CAM?&opt_cam : i==OP_MUSIC?&opt_music : i==OP_SFX?&opt_sfx : i==OP_AUTO?&opt_autosave : i==OP_SHAKE?&opt_shake : i==OP_MAP?&opt_minimap : i==OP_NUMS?&opt_nums : i==OP_HINTS?&opt_hints : &opt_arrow;
                int v=*pv; if(i==sel) rect(pg,18,y-2,204,10,6);
                text(pg,26,y,nm[i],i==sel?C_WHITE:C_GREY,1);
                scpy(b,"< "); scat(b, i==OP_DIFF?dif[v] : i==OP_CAM?cam[v] : i==OP_MUSIC?mv[v] : yn[v]); scat(b," >");
                rtext(pg,214,y,b, i==sel?C_GOLD : (i==OP_DIFF ? (v==0?C_GREEN:v==2?C_RED:C_GREY) : C_GREY));
            }
            rect(pg,18,128,204,1,C_DGREY);
            { const char *h=hlp[sel]; int L=textlen(h); if(L>52){ char h1[56]; int k=0; while(k<52 && h[k]) k++; while(k>0 && h[k]!=' ') k--; for(int j=0;j<k;j++) h1[j]=h[j]; h1[k]=0; ctext(pg,120,132,h1,C_GREY); ctext(pg,120,140,h+k+1,C_GREY); } else ctext(pg,120,134,h,C_GREY); }
            ctext(pg,120,148,"LEFT/RIGHT: CHANGE  B: BACK",C_CYAN);
        }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NOPT-1)%NOPT; sfx(7); dirty=1; }
        if(HIT(K_DOWN)){ sel=(sel+1)%NOPT; sfx(7); dirty=1; }
        int d = HIT(K_RIGHT)||HIT(K_A) ? 1 : HIT(K_LEFT) ? -1 : 0;
        if(d){ dirty=1;
            if(sel==OP_DIFF){ opt_diff=(opt_diff+d+3)%3; opt_easy=(opt_diff==0); }
            else if(sel==OP_CAM) opt_cam^=1;
            else if(sel==OP_MUSIC){ opt_music=(opt_music+d+3)%3; music_apply(); }
            else if(sel==OP_SFX) opt_sfx^=1; else if(sel==OP_AUTO) opt_autosave^=1; else if(sel==OP_SHAKE) opt_shake^=1;
            else if(sel==OP_MAP) opt_minimap^=1; else if(sel==OP_NUMS) opt_nums^=1; else if(sel==OP_HINTS) opt_hints^=1; else opt_arrow^=1;
            sfx(2);
        }
        if(HIT(K_B)||HIT(K_START)){ settings_save(); wait_release(); return; }
    }
}
