// ================================================================= THE DUNGEON FORGE
// At your ship, once the Ant King is down. Pick a seed and tune the dungeon (layout, walls, creatures, density, hazard, boss, floors, level),
// play the result, and keep your best designs on the cartridge: 8 slots in spare SRAM at 0x6C00 (same seed + settings = the same dungeon).
static const char *const fg_ly[NLY]={"ROOMS","GRAND HALLS","CAVERNS","FLOODED TEMPLE","CATACOMBS","LABYRINTH"};
static const char *const fg_wl[7]={"BRICK","STONE","EARTH","MOSS","CRYSTAL","MARBLE","AUTO"};
static const char *const fg_mx[4]={"SPIDER SWARM","MIXED","BEETLE HERDS","WASP NESTS"};
static const char *const fg_dn[3]={"SPARSE","NORMAL","CROWDED"};
static const char *const fg_hz[5]={"NONE","DARKNESS","CURSED","MONSTER NESTS","AMBUSHES"};
static const char *const fg_bs[3]={"RARE LABYRINTH","LABYRINTH BOSS","NORMAL BOSS"};
static const char *const fg_rn[13]={"SEED","LAYOUT","WALLS","CREATURES","DENSITY","HAZARD","BOSS FLOOR","FLOORS","MONSTER LEVEL","DESIGN SLOT","LOAD THE SLOT","SAVE TO THE SLOT","START THE RUN"};

typedef struct { u16 seed; u8 layout, wall, mix, dens, haz, floors, lvl, boss, best, done, used, pad[3]; } FDes;     // 16 bytes
typedef struct { u32 magic; FDes d[8]; u32 sum; } FStore;
_Static_assert(__builtin_offsetof(FStore,sum)==sizeof(FStore)-4, "FStore: the sum must be the last 4 bytes");
#define FG_OFF 0x6C00
EW static FStore fst;
static DDna fg; static int fg_ready=0, fg_slot=0, fg_cur=-1;                 // fg_cur: the saved design the current run came from
static int fg_sv_end, fg_sv_dlv, fg_sv_portal; static s64 fg_sv_loot;
static void fst_load(void){ sram_read(FG_OFF,&fst,sizeof fst); if(fst.magic!=0x4746524Fu || fst.sum!=fnv(&fst,sizeof fst-4)){ memset(&fst,0,sizeof fst); fst.magic=0x4746524Fu; } }
static void fst_save(void){ fst.sum=fnv(&fst,sizeof fst-4); sram_write(FG_OFF,&fst,sizeof fst); }
static void fg_random(void){ u32 h=hmix(rng_s^vbl_count*2654435761u); fg.seed=h&0xFFFF; fg.layout=(h>>16)%NLY; fg.wall=(h>>19)%7; fg.mix=(h>>22)&3; fg.dens=(h>>24)%3; fg.haz=(h>>26)%5; fg.boss=0; }
static void fg_default(void){ fg.on=1; fg.layout=0; fg.wall=6; fg.mix=1; fg.dens=1; fg.haz=0; fg.boss=0; fg.floors=3; { int h=clampi(sdiv(level,2)); fg.lvl=h<1?1:h>30?30:h; } fg.seed=(u16)(rng_s>>8); }
static void fg_adj(int r, int d){
    int v;
    switch(r){
        case 0: fg.seed=(u16)(fg.seed+d); break;
        case 1: fg.layout=(fg.layout+d+NLY)%NLY; break;
        case 2: fg.wall=(fg.wall+d+7)%7; break;
        case 3: fg.mix=(fg.mix+d+4)%4; break;
        case 4: fg.dens=(fg.dens+d+3)%3; break;
        case 5: fg.haz=(fg.haz+d+5)%5; break;
        case 6: fg.boss=(fg.boss+d+3)%3; break;
        case 7: v=fg.floors+d; fg.floors = v<1?1 : v>9?9 : v; break;
        case 8: v=fg.lvl+d; fg.lvl = v<1?1 : v>30?30 : v; break;
        case 9: fg_slot=(fg_slot+d+8)%8; return;
        default: return;
    }
    fg_cur=-1;                                                               // changed: it is no longer the saved design
}
static void fg_val(int r, char *b){
    b[0]=0;
    switch(r){
        case 0: ncat(b,fg.seed); break;
        case 1: scpy(b,fg_ly[fg.layout]); break;
        case 2: scpy(b,fg_wl[fg.wall]); break;
        case 3: scpy(b,fg_mx[fg.mix]); break;
        case 4: scpy(b,fg_dn[fg.dens]); break;
        case 5: scpy(b,fg_hz[fg.haz]); break;
        case 6: scpy(b,fg_bs[fg.boss]); break;
        case 7: ncat(b,fg.floors); break;
        case 8: ncat(b,fg.lvl); break;
        case 9: ncat(b,fg_slot+1); if(fst.d[fg_slot].used){ scat(b,"  S"); ncat(b,fst.d[fg_slot].seed); if(fst.d[fg_slot].done) scat(b," *"); else if(fst.d[fg_slot].best){ scat(b," B"); ncat(b,fst.d[fg_slot].best); } } else scat(b,"  EMPTY"); break;
    }
}
static int forge_menu_(volatile u16 *pg);
static int forge_menu(volatile u16 *pg){ int sh=shd_begin(); int r=forge_menu_(sh?VISPG:pg); shd_end(sh); return r; }
static int forge_menu_(volatile u16 *pg){                                      // returns 1 to start a run
    fst_load(); if(!fg_ready){ fg_default(); fg_ready=1; }
    int sel=12, dirty=1; wait_release(); menu_msg[0]=0;
    while(1){
        if(dirty){ dirty=0; box(pg,12,20,216,136); text(pg,20,24,"DUNGEON FORGE",C_GOLD,1);
            for(int i=0;i<13;i++){ char b[32]; fg_val(i,b); if(i==sel) text(pg,16,34+i*7,">",C_CYAN,1);
                text(pg,22,34+i*7,fg_rn[i],i==sel?C_WHITE:C_GREY,1); if(i<10) rtext(pg,224,34+i*7,b,i==sel?C_GOLD:C_CYAN); else if(i==9||i==10) {} }
            { char b[32]; fg_val(9,b); rtext(pg,224,34+9*7,b,sel==9?C_GOLD:C_CYAN); }
            if(menu_msg[0]) text(pg,20,139,menu_msg,C_GREEN,1);
            ui_lhint(pg,20,148,"A: DO  B: BACK  SELECT: DICE  L/R: +100",C_DGREY); }
        vsync(); read_keys(); if(keys!=keys_old) dirty=1;
        if(HIT(K_UP)){ sel=(sel+12)%13; menu_msg[0]=0; } if(HIT(K_DOWN)){ sel=(sel+1)%13; menu_msg[0]=0; }
        if(HIT(K_LEFT)) fg_adj(sel,-1); if(HIT(K_RIGHT)) fg_adj(sel,1);
        if(sel==0){ if(HIT(K_L)) fg_adj(0,-100); if(HIT(K_R)) fg_adj(0,100); }
        if(HIT(K_SEL)){ fg_random(); fg_cur=-1; scpy(menu_msg,"A RANDOM DESIGN"); sfx(6); }
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HIT(K_A)){
            if(sel==10){ FDes *d=&fst.d[fg_slot]; if(!d->used) scpy(menu_msg,"THAT SLOT IS EMPTY");
                else { fg.seed=d->seed; fg.layout=d->layout%NLY; fg.wall=d->wall%7; fg.mix=d->mix&3; fg.dens=d->dens%3; fg.haz=d->haz%5; fg.floors=d->floors<1?1:d->floors>9?9:d->floors; fg.lvl=d->lvl<1?1:d->lvl>30?30:d->lvl; fg.boss=d->boss%3; fg_cur=fg_slot; scpy(menu_msg,"DESIGN LOADED"); sfx(7); } }
            else if(sel==11){ FDes *d=&fst.d[fg_slot]; memset(d,0,sizeof(FDes)); d->used=1; d->seed=fg.seed; d->layout=fg.layout; d->wall=fg.wall; d->mix=fg.mix; d->dens=fg.dens; d->haz=fg.haz; d->floors=fg.floors; d->lvl=fg.lvl; d->boss=fg.boss; fst_save(); fg_cur=fg_slot; scpy(menu_msg,"DESIGN SAVED TO THE CARTRIDGE"); sfx(7); }
            else if(sel==12){ wait_release(); return 1; }
        }
    }
}
static void forge_begin(void){                                                // the world becomes a temporary endless one for the run
    fg_sv_end=endless; fg_sv_dlv=w_dlv; fg_sv_portal=portal_depth; fg_sv_loot=loot;
    ddna=fg; ddna.on=1; endless=1; w_dlv=fg.lvl; portal_depth=0; loot=0; forge_on=1;
}
static void forge_end(int r, char *mb){                                       // back on the surface: pay out, record the design, restore the world
    int won = depth_no>=fg.floors && !boss_floor(); s64 bonus=0;
    if(r==2){ recalc(); hearts=maxh; food=60; scpy(mb,"FORGE: YOU FAINTED. NO HARM DONE"); }
    else { bonus = sdiv(smul(loot,100+depth_no*15),100); if(won) bonus=sadd(bonus,100+fg.lvl*20*fg.floors); gold=sadd(gold,bonus); scpy(mb, won?"FORGE CLEARED! +":"FORGE ESCAPED: +"); ncat(mb,bonus); scat(mb,"G"); }
    if(fg_cur>=0 && fst.d[fg_cur].used){ FDes *d=&fst.d[fg_cur]; if(depth_no>d->best) d->best=depth_no; if(won) d->done=1; fst_save(); }
    forge_on=0; endless=fg_sv_end; w_dlv=fg_sv_dlv; portal_depth=fg_sv_portal; loot=fg_sv_loot;
}
