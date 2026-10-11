// ================================================================= THE DEBUG MENU: the Konami code on the title screen (UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A)
// Putting the code in unlocks every mode at once (Kart World, HOW BIG IS IT?, INSANE-SECT, all of Kart World's classes) and opens this menu:
// start a game with cheats, or jump straight to any part of the game: a zone's planet, any dungeon floor, a boss, space, a fleet battle,
// the Technobeast, a far world of any type with any weird trait, the galaxy map, a kart race, the makers, the credits, a sound test.
static void arrive_on_planet(void); static void landing_screen(int welcome);
typedef struct { int slot, god, maxst, lvl, gold, form, body, zone, floor, fleet, beast, kart, laps, mus, sfxn, go; } Dbg;
static Dbg dbg={2,0,0,0,0,0,0,0,1,8,2,0,3,3,2,0};
enum { DG_NONE, DG_NEW, DG_CONT, DG_PLANET, DG_FLOOR, DG_BOSS, DG_SPACE, DG_FLEET, DG_BEAST, DG_FAR, DG_MAP, DG_KART, DG_GARAGE, DG_CREDITS };
static const char *const dbg_lvl_nm[5]={"KEEP","10","25","50","99"}; static const s64 dbg_lvl_v[5]={0,10,25,50,99};
static const char *const dbg_gold_nm[5]={"KEEP","10,000","1 MILLION","1 BILLION","THE MOST"}; static const s64 dbg_gold_v[5]={0,10000,1000000,1000000000LL,S64_MAX};
static const char *const dbg_form_nm[3]={"BUG","HUMANOID","CREATURE"}; static const u8 dbg_form_v[3]={FORM_BUG,FORM_HUMAN,FORM_CREATURE};
static const char *const dbg_beast_nm[3]={"HATCHLING","GROWN","COMPLETE"};
static const char *const dbg_mus_nm[8]={"PLANET","DUNGEON","TITLE","SPACE","BOSS","ENDING","KART RACE","FINAL LAP"};
static const char *const dbg_asis[1]={"AS IS"};
static void dbg_unlock_modes(void){                            // the modes: open for every save, right now
    kw_unlock(); opt_insane=1; settings_save();
    KWSave keep=kw; kw_load(); kw.cls_open=5; for(int p=0;p<NKPART;p++) kw.own[p]=0xFFFF; if(kw.coins<999999) kw.coins=999999; kw_store(); kw=keep;
}
static void dbg_unlock_game(void){                             // everything in the game you are in: stars, forms, tools, the drive, the friends, the relics
    ant_king=1; zones_open=6; cleared_mask=63; forms_open=7; tools_owned=0xFF; warp_lv=8; fuel_tank=8; fuel=fuel_max();
    relic_bits=0xFFFF; data_cores=8; wing_n=MAXWING; laser_lv=4; shield_lv=8; cargo_cap=2000; hull=100;
    for(int i=0;i<6;i++) have_bug[i]=1; for(int i=0;i<NSPELL;i++) spell_known[i]=1; beast_stage = beast_stage>2 ? beast_stage : 2;
    recalc(); hearts=maxh; mp=maxmp;
}
static void dbg_cheats(void){                                  // the menu's cheats, applied to the game just started or loaded
    if(dbg.maxst){ st_str=st_dex=st_vit=st_mag=300; }
    if(dbg.lvl) level=dbg_lvl_v[dbg.lvl];
    if(dbg.gold) gold=dbg_gold_v[dbg.gold];
    if(dbg.form){ form=dbg_form_v[dbg.form]; forms_open=7; tools_owned=0xFF; tool_held=1; form_body=dbg.body; form_col=rgb15(200,80,200); form_palette(); }
    recalc(); hearts=maxh; mp=maxmp;
}
static void dbg_story(int z){ zones_open=6; cleared_mask=(1<<z)-1; ant_king=0; cur_sys=z; }       // the story as it stands when you reach zone z
static int dbg_start(void){                                    // called by start_game after the game is set up: the GO targets that need their own set-up. 1 = done
    int go=dbg.go; if(!go) return 0;
    dbg_unlock_game(); dbg_cheats();
    if(go==DG_PLANET || go==DG_FLOOR || go==DG_BOSS || go==DG_KART || go==DG_GARAGE || go==DG_CREDITS){ dbg_story(dbg.zone); land_on_current(); planet_tables(); return 0; }   // (the normal landing follows)
    if(go==DG_SPACE){ dbg.go=0; fade(0); scene=2; page_back=1; space_enter(1); music(3); return 1; }
    if(go==DG_FLEET){ dbg.go=0; fade(0); scene=2; page_back=1; wing_n=MAXWING; no_ambush=1; space_enter(0); no_ambush=0; start_battle(dbg.fleet); music(3); return 1; }
    if(go==DG_BEAST){ dbg.go=0; { volatile u16 *v=(volatile u16*)0x06000000; for(int i=0;i<0x14000/2;i++) v[i]=0; } fade(0); scene=2; page_back=1; beast_defaults(); beast_stage=dbg.beast; tech_lv=dbg.beast==2?3:0; data_cores=8; lair_enter(); return 1; }
    if(go==DG_FAR || go==DG_MAP){ dbg.go=0; galaxy_no=4000000000u; u32 s=rnd(); for(int w=0;w<NCW;w++) clus[w]=hmix(w*77+s); u_settle(); gen_cluster(); cur_sys=7+(int)(s%20u); fade(0); arrive_on_planet();
        if(go==DG_MAP){ scene=2; page_back=1; space_enter(1); shx=PR*3; shy=0; shz=0; galaxy_map(); } return 1; }
    return 0;
}
static void dbg_start2(void){                                  // ...and the targets that start on the planet (after the landing)
    int go=dbg.go; if(!go) return; dbg.go=0;
    if(go==DG_NEW || go==DG_CONT){ dbg_unlock_game(); dbg_cheats(); save_game(); return; }
    if(go==DG_PLANET) return;
    if(go==DG_FLOOR){ portal_depth=dbg.floor; enter_dungeon_flag=1; }
    if(go==DG_BOSS){ portal_depth=zone_floors(); enter_dungeon_flag=1; }
    if(go==DG_KART){ k_cls=1; k_type=0; k_trk=dbg.kart%kt_count(); k_rec=-1; kt_official(k_trk,&kt); kt.laps=dbg.laps; kart_go(NRC,-1,0,1); }
    if(go==DG_GARAGE){ if(gold<99999) gold=99999; kgarage(VISPG); planet_hud(); }
    if(go==DG_CREDITS){ fade(1); credits(); planet_enter(); fade(0); }
}
static int dbg_menu(volatile u16 *pg){                         // 0: back to the title. 2: Kart World. 3: start the game the menu set up
    enum { DM_NEW, DM_CONT, DM_UNLOCK, DM_SLOT, DM_GOD, DM_MAXST, DM_LVL, DM_GOLD, DM_DIFF, DM_HUD, DM_FPS, DM_FORM, DM_BODY, DM_HOUR, DM_WEATHER,
           DM_ZONE, DM_FLOOR, DM_GOPLANET, DM_GOFLOOR, DM_GOBOSS, DM_GOSPACE, DM_FLEET, DM_GOFLEET, DM_BEAST, DM_GOBEAST, DM_CLASS, DM_WEIRD, DM_WLV, DM_GOFAR, DM_GOMAP,
           DM_KART, DM_LAPS, DM_GOKART, DM_GOKW, DM_BUG, DM_MIRROR, DM_HANGAR, DM_GARAGE, DM_SIZE, DM_CREDITS, DM_MUS, DM_SFX, DM_QUIET, NDM };
    static const char *const nm[NDM]={"NEW GAME, EVERYTHING UNLOCKED","CONTINUE THAT SAVE, UNLOCKED","UNLOCK EVERYTHING IN THAT SAVE","SAVE SLOT","GOD MODE","MAX STATS","LEVEL","GOLD","DIFFICULTY","HIDE THE HUD","SHOW FPS","FORM","CREATURE BODY","TIME OF DAY","WEATHER",
        "ZONE","FLOOR","GO: THAT ZONE'S PLANET","GO: THAT DUNGEON FLOOR","GO: THAT ZONE'S BOSS","GO: SPACE","ENEMY SHIPS","GO: A FLEET BATTLE","BEAST STAGE","GO: THE TECHNOBEAST","WORLD TYPE","WEIRD TRAIT","TRAIT STRENGTH","GO: A FAR WORLD","GO: THE GALAXY MAP",
        "KART TRACK","LAPS","GO: A KART RACE","GO: KART WORLD","THE BUG MAKER","THE MIRROR (HUMANOID)","THE SHIP HANGAR","THE KART GARAGE","HOW BIG IS IT?","THE CREDITS","MUSIC","SOUND EFFECT","STOP THE MUSIC"};
    static const char *const hlp[NDM]={"A: START. THE CHEATS BELOW ARE ON. IT SAVES TO THE SLOT","A: LOAD THE SLOT AND PLAY IT WITH THE CHEATS BELOW","A: OPEN ALL STARS, FORMS, TOOLS, DRIVES, RELICS IN IT. SAVES","THE SLOT THE DEBUG GAME USES. IT GETS OVERWRITTEN",
        "NEVER HURT, NEVER HUNGRY, THE SHIP NEVER BREAKS","STRENGTH, SPEED, LIFE AND MAGIC AT 300","YOUR LEVEL WHEN THE GAME STARTS","YOUR GOLD WHEN THE GAME STARTS","THE SAME AS IN OPTIONS","NO HUD: ONLY THE WORLD (FOR SCREENSHOTS)","A FRAME COUNTER IN THE CORNER",
        "START AS A BUG, A HUMANOID OR A CREATURE","WHICH CREATURE, IF YOU ARE ONE","EVERY PLANET AT THIS HOUR","EVERY PLANET WITH THIS WEATHER",
        "THE ZONE FOR THE THREE GO LINES BELOW","THE FLOOR TO START ON (B1 = 1)","A: LAND ON THE ZONE'S PLANET, STORY AS IT WAS THEN","A: STRAIGHT DOWN TO THAT FLOOR","A: STRAIGHT TO THE BOSS AT THE BOTTOM","A: UP IN YOUR SHIP, BY THE PLANET",
        "HOW MANY ENEMY SHIPS (1 TO 50)","A: A FLEET BATTLE, WITH A FULL WING","HATCHLING, GROWN, OR THE COMPLETE 1 TRILLION HP BEAST","A: INTO ITS LAIR",
        "THE WORLD TYPE OF THE FAR WORLD (AND OF EVERY NEW WORLD)","A WEIRD TRAIT FOR THE FAR WORLD (AND EVERY NEW WORLD)","HOW STRONG THE TRAIT IS, 1 TO 25","A: A RANDOM WORLD FAR OUT IN THE GALAXY","A: IN DEEP SPACE AT THE MAP. START: CLUSTER AND UNIVERSE JUMPS",
        "WHICH OF THE PLANET'S TRACKS","HOW MANY LAPS","A: A RACE ON THE GARDEN","A: THE KART RACING GAME","A: MAKE A BUG","A: MAKE A HUMANOID","A: BUILD A SHIP","A: BUILD A KART (ON THE PLANET)","A: THE GAME'S HUGE NUMBERS","A: ROLL THE CREDITS","A: PLAY IT","A: PLAY IT","A: QUIET"};
    static const char *const dif[6]={"EASY","NORMAL","HARD","BRUTAL","NIGHTMARE","INSANE-SECT"}, *const yn[2]={"OFF","ON"}, *const hr[3]={"DAY","DUSK","NIGHT"}, *const bd[3]={"SLUG","FROG","BAT"};
    int sel=0, top=0, dirty=1, page=0; char note[64]; note[0]=0;
    volatile u16 *pg0=(volatile u16*)0x06000000, *pg1=(volatile u16*)0x0600A000; (void)pg;
    slots_refresh(); if(dbg.slot<0){ dbg.slot=2; for(int i=0;i<NSLOT;i++) if(!sinf[i].ok){ dbg.slot=i; break; } }
    dbg_unlock_modes(); scpy(note,"OPEN: KART WORLD, HOW BIG IS IT?, INSANE-SECT");
    REG_BLDCNT=0; REG_BLDY=0; wait_release();
    #define DBG_SHOW0() do{ for(int i_=0;i_<120*160;i_++) pg0[i_]=0; REG_DISPCNT&=~(1<<4); page=0; }while(0)        /* (the makers draw on page 0: clear it and show it) */
    while(1){
        if(sel<top) top=sel; if(sel>=top+10) top=sel-9;
        if(dirty){ dirty=0; char b[40]; volatile u16 *pg = page ? pg0 : pg1;     // drawn on the page you are not looking at, then shown: no flicker
            panel(pg,4,2,232,156,"DEBUG MENU");
            for(int r=0;r<10;r++){ int i=top+r, y=19+r*9; if(i>=NDM) break;
                if(i==sel) rect(pg,8,y-2,224,10,6);
                int act = nm[i][0]=='G' && nm[i][1]=='O' ? 1 : (i<=DM_UNLOCK || (i>=DM_BUG && i<=DM_CREDITS) || i==DM_QUIET);
                text(pg,12,y,nm[i],i==sel?C_WHITE:act?C_CYAN:C_GREY,1);
                b[0]=0;
                switch(i){
                    case DM_SLOT: scpy(b,"SLOT "); ncat(b,dbg.slot+1); scat(b, sinf[dbg.slot].ok ? " (USED)" : " (EMPTY)"); break;
                    case DM_GOD: scpy(b,yn[dbg.god]); break;  case DM_MAXST: scpy(b,yn[dbg.maxst]); break;
                    case DM_LVL: scpy(b,dbg_lvl_nm[dbg.lvl]); break;  case DM_GOLD: scpy(b,dbg_gold_nm[dbg.gold]); break;
                    case DM_DIFF: scpy(b,dif[opt_diff]); break;  case DM_HUD: scpy(b,yn[film_mode]); break;  case DM_FPS: scpy(b,yn[dbg_fps]); break;
                    case DM_FORM: scpy(b,dbg_form_nm[dbg.form]); break;  case DM_BODY: scpy(b,bd[dbg.body]); break;
                    case DM_HOUR: scpy(b, dbg_hour<0 ? dbg_asis[0] : hr[dbg_hour]); break;  case DM_WEATHER: scpy(b, dbg_weather<0 ? dbg_asis[0] : weather_name[dbg_weather]); break;
                    case DM_ZONE: scpy(b,tint_name[dbg.zone]); break;  case DM_FLOOR: scpy(b,"B"); ncat(b,dbg.floor); break;
                    case DM_FLEET: ncat(b,dbg.fleet); break;  case DM_BEAST: scpy(b,dbg_beast_nm[dbg.beast]); break;
                    case DM_CLASS: scpy(b, dbg_cls<0 ? "ANY" : pc_name[dbg_cls]); break;
                    case DM_WEIRD: scpy(b, dbg_weird<0 ? "OFF" : mw_name[dbg_weird%NMW]); break;
                    case DM_WLV: if(dbg_weird>=0){ ncat(b,dbg_weird/NMW+1); scat(b," "); scat(b,mw_adj[dbg_weird/NMW]); } else scpy(b,"-"); break;
                    case DM_KART: ncat(b,dbg.kart); break;  case DM_LAPS: ncat(b,dbg.laps); break;
                    case DM_MUS: scpy(b,dbg_mus_nm[dbg.mus]); break;  case DM_SFX: ncat(b,dbg.sfxn); break;
                }
                if(b[0]){ b[22]=0; char c[28]; scpy(c,"< "); scat(c,b); scat(c," >"); rtext(pg,228,y,c,i==sel?C_GOLD:C_GREY); } }
            if(top>0) text(pg,226,19,"^",C_CYAN,1); if(top+10<NDM) text(pg,226,100,"v",C_CYAN,1);
            rect(pg,8,110,224,1,C_DGREY);
            { const char *h=hlp[sel]; int L=textlen(h); if(L>54){ char h1[60]; int k=0; while(k<54 && h[k]) k++; while(k>0 && h[k]!=' ') k--; for(int j=0;j<k;j++) h1[j]=h[j]; h1[k]=0; ctext(pg,120,115,h1,C_GREY); ctext(pg,120,123,h+k+1,C_GREY); } else ctext(pg,120,118,h,C_GREY); }
            if(note[0]) ctext(pg,120,134,note,C_GOLD);
            ctext(pg,120,146,"LEFT/RIGHT: CHANGE   A: GO   B: BACK TO THE TITLE",C_CYAN);
            vsync(); page^=1; REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(page?(1<<4):0);
        }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NDM-1)%NDM; sfx(7); dirty=1; } if(HIT(K_DOWN)){ sel=(sel+1)%NDM; sfx(7); dirty=1; }
        if(HIT(K_L)){ sel=(sel+NDM-10)%NDM; sfx(7); dirty=1; } if(HIT(K_R)){ sel=(sel+10)%NDM; sfx(7); dirty=1; }
        if(HIT(K_B)){ settings_save(); DBG_SHOW0(); wait_release(); return 0; }
        int d = HIT(K_RIGHT) ? 1 : HIT(K_LEFT) ? -1 : 0, a=HIT(K_A);
        if(d || a){ dirty=1; int dd = d ? d : 1;
            switch(sel){
                case DM_SLOT: dbg.slot=(dbg.slot+dd+NSLOT)%NSLOT; break;
                case DM_GOD: dbg.god^=1; dbg_god=dbg.god; break;  case DM_MAXST: dbg.maxst^=1; break;
                case DM_LVL: dbg.lvl=(dbg.lvl+dd+5)%5; break;  case DM_GOLD: dbg.gold=(dbg.gold+dd+5)%5; break;
                case DM_DIFF: opt_diff=(opt_diff+dd+6)%6; opt_easy=(opt_diff==0); break;
                case DM_HUD: film_mode^=1; break;  case DM_FPS: dbg_fps^=1; break;
                case DM_FORM: dbg.form=(dbg.form+dd+3)%3; break;  case DM_BODY: dbg.body=(dbg.body+dd+3)%3; break;
                case DM_HOUR: dbg_hour = dbg_hour+dd<-1 ? 2 : dbg_hour+dd>2 ? -1 : dbg_hour+dd; break;
                case DM_WEATHER: dbg_weather = dbg_weather+dd<-1 ? 5 : dbg_weather+dd>5 ? -1 : dbg_weather+dd; break;
                case DM_ZONE: dbg.zone=(dbg.zone+dd+6)%6; break;  case DM_FLOOR: dbg.floor=dbg.floor+dd<1 ? 30 : dbg.floor+dd>30 ? 1 : dbg.floor+dd; break;
                case DM_FLEET: dbg.fleet=dbg.fleet+dd<1 ? 50 : dbg.fleet+dd>50 ? 1 : dbg.fleet+dd; break;  case DM_BEAST: dbg.beast=(dbg.beast+dd+3)%3; break;
                case DM_CLASS: dbg_cls = dbg_cls+dd<-1 ? NPC-1 : dbg_cls+dd>=NPC ? -1 : dbg_cls+dd; break;
                case DM_WEIRD: { int k = dbg_weird<0 ? -1 : dbg_weird%NMW, lv = dbg_weird<0 ? 21 : dbg_weird/NMW; k += dd; if(k<-1) k=NMW-1; if(k>=NMW) k=-1; dbg_weird = k<0 ? -1 : lv*NMW+k; } break;
                case DM_WLV: if(dbg_weird>=0){ int k=dbg_weird%NMW, lv=(dbg_weird/NMW+dd+25)%25; dbg_weird=lv*NMW+k; } break;
                case DM_KART: dbg.kart=(dbg.kart+dd+10)%10; break;  case DM_LAPS: dbg.laps=dbg.laps+dd<1 ? 9 : dbg.laps+dd>9 ? 1 : dbg.laps+dd; break;
                case DM_MUS: if(a) music(dbg.mus); else dbg.mus=(dbg.mus+dd+8)%8; break;
                case DM_SFX: if(a) sfx(dbg.sfxn); else dbg.sfxn=(dbg.sfxn+dd+15)%15; break;
                default: break;
            }
            if(sel!=DM_SFX) sfx(2);
        }
        if(a){ int go=0;
            switch(sel){
                case DM_NEW: go=DG_NEW; break;
                case DM_CONT: if(sinf[dbg.slot].ok) go=DG_CONT; else scpy(note,"THAT SLOT IS EMPTY"); break;
                case DM_UNLOCK: if(!sinf[dbg.slot].ok) scpy(note,"THAT SLOT IS EMPTY"); else { cur_slot=dbg.slot; if(load_game(1)){ dbg_unlock_game(); do_save(); scpy(note,"DONE: THAT SAVE HAS EVERYTHING OPEN"); } else scpy(note,"THAT SAVE WOULD NOT LOAD"); new_game(); } break;
                case DM_GOPLANET: go=DG_PLANET; break;  case DM_GOFLOOR: go=DG_FLOOR; break;  case DM_GOBOSS: go=DG_BOSS; break;
                case DM_GOSPACE: go=DG_SPACE; break;  case DM_GOFLEET: go=DG_FLEET; break;  case DM_GOBEAST: go=DG_BEAST; break;
                case DM_GOFAR: go=DG_FAR; break;  case DM_GOMAP: go=DG_MAP; break;  case DM_GOKART: go=DG_KART; break;  case DM_GARAGE: go=DG_GARAGE; break;  case DM_CREDITS: go=DG_CREDITS; break;
                case DM_GOKW: settings_save(); DBG_SHOW0(); wait_release(); return 2;
                case DM_BUG: DBG_SHOW0(); make_bug(pg0,1); break;
                case DM_MIRROR: DBG_SHOW0(); form=FORM_HUMAN; forms_open=7; human_creator(pg0,1); form=FORM_BUG; break;
                case DM_HANGAR: DBG_SHOW0(); make_ship(pg0,1); break;
                case DM_SIZE: DBG_SHOW0(); cur_slot=dbg.slot; size_screen(pg0,1); break;
                case DM_QUIET: music(-1); break;
                default: break;
            }
            if(go){ dbg.go=go; cur_slot=dbg.slot; dbg_god=dbg.god; settings_save(); DBG_SHOW0(); wait_release(); return 3; }
            dirty=1; wait_release();
        }
    }
    #undef DBG_SHOW0
}
