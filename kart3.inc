// ================================================================= KART WORLD: the racing game on its own. It opens once you have beaten the game.
// From the title screen. Any of 4 billion planets (each has its own 4 to 9 tracks, so tens of billions of tracks), or let a
// tour pick them: ten tours of four races, in five classes, for 50 trophies. Quick races with every option there is, random
// races, time attack, elimination, an endless run, the track maker, your saved tracks from anywhere, the garage, 18 drivers.
// Coins, parts, trophies and records are kept in their own block on the cartridge (shared by everyone who plays).
static void landing_screen(int welcome);
#define KW_OFF 0x6A10
#define NKWT 10                                    // tours
#define NKWD 18                                    // drivers
#define KW_TRACKS 1000000u                         // tracks on every Kart World planet
typedef struct { u32 magic; u8 unlocked, driver, cls, laps, riv, items, skill, catchup, mirror, reverse, hour, weather, ftype, fsize, weird, stunts;
    u32 seed, track, coins; KLook look; u16 own[NKPART]; u8 troph[NKWT][5]; u16 races, wins, endless_best, elim_wins, tt_gold, cls_open; u8 cam[8]; KBest best[16]; u32 sum; } KWSave;
_Static_assert(__builtin_offsetof(KWSave,sum)==sizeof(KWSave)-4, "KWSave: the sum must be the last 4 bytes");
_Static_assert(KW_OFF+sizeof(KWSave) <= FG_OFF, "Kart World's block must end before the Dungeon Forge's");
EW static KWSave kw;
static int kw_quit=0, kw_tour=0, kw_endless_n=0;
static void kw_defaults(int unlocked){ memset(&kw,0,sizeof kw); kw.magic=0x444C524Bu; kw.unlocked=(u8)unlocked; kw.cls=1; kw.riv=11; kw.items=1; kw.skill=1; kw.catchup=1; kw.seed=1000; kw.coins=500; kw.cls_open=2;
    kw.look.part[KP_SIZE]=1; kw.look.part[KP_EXH]=1; kw.look.number=1; kw.look.col[0]=C15(60,140,255); kw.look.col[1]=C15(250,250,250); kw.look.col[2]=C15(255,200,40); scpy(kw.look.name,"STARKART"); { static const u8 dc[8]={0,1,1,1,1,0,1,1}; for(int i=0;i<8;i++) kw.cam[i]=dc[i]; } }
static void kw_load(void){ sram_read(KW_OFF,&kw,sizeof kw); if(kw.magic!=0x444C524Bu || kw.sum!=fnv(&kw,sizeof kw-4)) kw_defaults(0); }
static void kw_store(void){ kw.sum=fnv(&kw,sizeof kw-4); sram_write(KW_OFF,&kw,sizeof kw); }
static int kw_unlocked(void){ kw_load(); return kw.unlocked; }
static void kw_unlock(void){ if(kw_mode) return; KWSave keep=kw; kw_load(); if(!kw.unlocked){ kw.unlocked=1; kw_store(); } kw=keep; }
static int kw_trophies(int gold_only){ int n=0; for(int t=0;t<NKWT;t++) for(int c=0;c<5;c++) if(kw.troph[t][c]>=(gold_only?3:1)) n++; return n; }

// ---------------------------------------------------------------- the drivers: your bug, or nearly anything that lives
static const char *const kwd_name[NKWD]={"","SIR CRUNCH","SLIMOTHY","ZORP","WIBBLE","HOPSY","MOOCOW","FLITTER","MR BONES","OAKLEY","ROCKY","PETALINA","BOLT","THE SHOP","WEBSLEY","OLD STONES","LIL SHIP","EVIL TWIN"};
static const char *const kwd_kind[NKWD]={"YOUR BUG","BEETLE","SLUG","LOCAL","LOCAL","HOPPER","GRAZER","FLYER","FOSSIL","TREE","ROCK","FLOWER","DROID","MARKET STALL","WEB","RUIN","TINY SHIP","YOUR EVIL TWIN"};
static const u8 kwd_need[NKWD]={0,0,0,0,0,0,0,0,0,0,0,0,3,5,7,9,12,16};           // trophies (any medal) a driver needs
static int kwd_tile(int d, int *shp, int *siz, int *w, int *h, int *pb){
    static const u16 tl[NKWD]={0,T_SBEETLE,T_SLUG,T_LOCAL1,T_LOCAL2,T_HOPPER,T_GRAZER,T_FLYER,T_FOSSIL,T_TREE,T_ROCK,T_FLOWER,T_BOLT,T_STALL,T_WEB,T_RUIN,T_SHIP,T_BUGSIDE};
    *shp=0; *siz=1; *w=16; *h=16; *pb = (d>=5&&d<=7) ? 6 : d==17 ? 15 : 0;
    if(d==2||d==7){ *shp=1; *siz=0; *h=8; } if(d==17){ *shp=0; *siz=2; *w=32; *h=32; } return tl[d]; }
static void kwd_palette(void){                                   // the creature drivers wear your trim colour; the evil twin is your bug, in red
    volatile u16 *P=OBJ_PAL+6*16; for(int i=0;i<16;i++) P[i]=obj_pal[i]; u16 c=kl.col[1]; P[6]=shade15(c,7); P[7]=c; P[8]=lift15(c,8); P[5]=C15(250,250,250);
    volatile u16 *Q=OBJ_PAL+15*16; for(int i=0;i<16;i++) Q[i]=OBJ_PAL[48+i]; Q[2]=C15(60,10,20); Q[3]=C15(150,20,40); Q[4]=C15(230,60,60); Q[5]=C15(255,120,120); Q[9]=C15(255,230,60); }
static int kw_draw_driver(int slot, int cx, int feet, int inv, int flip){   // a Kart World driver in your seat (0: the bug's own drawing is used)
    int d=kw.driver; if(!d) return -1; int shp,siz,w,h,pb, tl=kwd_tile(d,&shp,&siz,&w,&h,&pb);
    set_affine(31, flip?-inv:inv, 0, 0, inv); int dh=h*256/inv;
    shadow_oam[slot].a0=((feet-dh/2-h)&255)|(1<<8)|(1<<9)|(shp<<14); shadow_oam[slot].a1=((cx-w)&511)|(31<<9)|(siz<<14); shadow_oam[slot].a2=tl|(pb<<12); return slot+1; }

static int kw_draw_driver_rot(int slot, int cx, int cy, int qa, int qb, int qc, int qd){   // the same, turned (in a loop-de-loop); cx,cy is the middle of the picture
    int d=kw.driver; if(!d) return -1; int shp,siz,w,h,pb, tl=kwd_tile(d,&shp,&siz,&w,&h,&pb);
    set_affine(31,qa,qb,qc,qd); shadow_oam[slot].a0=((cy-h)&255)|(1<<8)|(1<<9)|(shp<<14); shadow_oam[slot].a1=((cx-w)&511)|(31<<9)|(siz<<14); shadow_oam[slot].a2=tl|(pb<<12); return slot+1; }

// ---------------------------------------------------------------- planets by number
static const char *const kw_type_name[NPC+1]={"ANY","TEMPERATE","MOUNTAIN","MEADOW","OCEAN","DESERT","ICE","JUNGLE","DEATH PLANET","CITY PLANET","PARADISE","INFERNO","BRIMSTONE","OBSIDIAN","BONE WORLD","BLOOD SEA","FROZEN HELL"};
static void kw_dna(u32 seed, int force){ gen_cluster(); cur_sys=0; cur_pl=cur_moon=0; endless=1; world_no=(int)seed; w_dlv=8; mw_force=force; make_world(); mw_force=0; }
static int kw_force(int ftype){ return (kw.weird?1:0) | (ftype>=PC_INFERNO+1 ? 2 : 0); }
static int kw_match(int ftype, int fsize){ return (!ftype || wd.cls==ftype-1) && (!fsize || wd.psz==fsize-1) && (!kw.weird || wd.weird); }
static u32 kw_find(u32 seed, int ftype, int fsize, int dir){      // the next planet number (going dir) that fits the filters
    for(int t=0;t<6000;t++){ kw_dna(seed,kw_force(ftype)); if(kw_match(ftype,fsize)) return seed; seed+=(u32)dir; }
    return seed; }
static void kw_world(u32 seed, int force){                       // build the planet: its land, its life, then the sky you asked for
    kw_dna(seed,force);
    if(kw.hour) wd.hour=(u8)(kw.hour-1); if(kw.weather) wd.weather=(u8)(kw.weather==1?0:kw.weather-1);
    fade(1); landing_screen(0); new_planet(); planet_enter(); scene=0; msg_timer=0; }
static void kw_fav(Fav *f){ memset(f,0,sizeof *f); f->star=0xFF; f->seed=(u32)world_no; f->body=(u8)(kw_force(kw.ftype)); }   // where a Kart World track is: a planet number
static void kw_rec_where(const KRec *r, char *b){ if(r->at.star==0xFF){ scpy(b,"KART WORLD #"); ncat(b,r->at.seed); } else fav_name(&r->at,b); }
static void kt_mirror_reverse(int mir, int rev){
    if(mir){ int x0=kt.gx[0]; for(int i=0;i<kt.n;i++) kt.gx[i]=(u8)((2*x0-kt.gx[i])&255); }
    if(rev){ for(int i=1,j=kt.n-1;i<j;i++,j--){ u8 t=kt.gx[i]; kt.gx[i]=kt.gx[j]; kt.gx[j]=t; t=kt.gy[i]; kt.gy[i]=kt.gy[j]; kt.gy[j]=t; } } }
static void kt_peek(int idx, KTrack *t){                          // a track's name and shape without building its planet
    u32 h=hmix(wd.tseed^0x4B415254u^((u32)idx*0x9E3779B1u)); memset(t,0,sizeof *t); int ps=wd.psz;
    int sh=(int)(h%15u); if(sh>=11) sh++; if(idx==0) sh=(int)((h>>5)%3u); if(ps<=2 && ((h>>7)%(ps==0?3:ps==1?6:12))==0 && idx) sh=11; t->shape=(u8)sh; kt_name(t,h); }
static u32 kw_key(u32 seed, u32 trk){ return hmix(seed*0x9E3779B1u ^ (u32)trk*0x85EBCA6Bu ^ (u32)(kw.mirror|kw.reverse<<1|(kw.stunts%3)<<2)*0x27D4EB2Fu ^ 0x4B57u)|1; }
static int kw_best_find(u32 key, KBest *out){ for(int i=0;i<16;i++) if(kw.best[i].key==key){ if(out) *out=kw.best[i]; return i; } return -1; }
static void kw_best_put(u32 key, int t, int medal){ int i=kw_best_find(key,0); if(i<0){ i=0; for(int j=0;j<16;j++){ if(!kw.best[j].key){ i=j; break; } if(kw.best[j].best>kw.best[i].best) i=j; } kw.best[i].key=key; kw.best[i].best=0xFFFF; kw.best[i].medal=0; }
    if(t<kw.best[i].best) kw.best[i].best=(u16)(t>65000?65000:t); if(medal>kw.best[i].medal) kw.best[i].medal=(u8)medal; }

// ---------------------------------------------------------------- starting each kind of race
static void kw_race_setup(void){ k_skill_adj = kw.skill==0 ? -8 : kw.skill==2 ? 5 : kw.skill==3 ? 10 : 0; k_items=kw.items; k_catch=kw.catchup; k_stunts=kw.stunts%3; kwd_palette(); }
static void kw_start_planet_race(u32 seed, u32 trk, int riders, int type){
    kw_world(seed,kw_force(kw.ftype)); kw_race_setup(); k_type=type; k_trk=(int)(trk%KW_TRACKS); k_rec=-1; kt_official(k_trk,&kt); kt_mirror_reverse(kw.mirror,kw.reverse);
    if(kw.laps) kt.laps=(u8)kw.laps; else kt.laps=0; if(type==5) kt.laps=2; if(type==6) kt.laps=50;
    kart_go(riders,riders>1?-1:0,0,1); kt.laps = kt.laps ? kt.laps : 3;
#ifdef TEST_TOPMAP
    { volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10); for(int y=0;y<160;y++) for(int x=0;x<120;x++){ int tx=(x*2*256/240+((ppx>>15)&255)-60)&255, ty=(y+((ppy>>15)&255)-80)&255; int c=cmap[ty*HW+tx]; pg[y*120+x]=c|(c<<8); }
      char b[48]; scpy(b,"KNP "); ncat(b,knp); scat(b," LEN "); ncat(b,kt_len); scat(b," N "); ncat(b,kt.n); scat(b," WL "); ncat(b,water_lv); scat(b," HS "); ncat(b,g_hs); text(pg,4,150,b,C_WHITE,1); while(1) vsync(); }
#endif
}
static const char *const kw_tour_name[NKWT]={"SUNNY TOUR","SNOW TOUR","SAND TOUR","SPLASH TOUR","PEAK TOUR","JUNGLE TOUR","NEON CITY TOUR","DOOM TOUR","TINY WORLDS TOUR","WEIRD TOUR"};
static const u8 kw_tour_type[NKWT]={1+PC_TEMPERATE,1+PC_ICE,1+PC_DESERT,1+PC_OCEAN,1+PC_MOUNTAIN,1+PC_JUNGLE,1+PC_ECUMENOPOLIS,1+PC_DEATH,0,0};
static u32 kw_tour_seed(int t, int i){ u32 s=hmix(0x70A2u+(u32)t*1000u+(u32)i*77u)%900000000u; int w=kw.weird; kw.weird = t==9; int ft = kw_tour_type[t]; if(t==0 && (i&1)) ft=1+PC_MEADOW; if(t==3 && i==3) ft=1+PC_PARADISE;
    s=kw_find(s,ft,t==8?1:0,1); kw.weird=(u8)w; return s; }
static void kw_tour_race(int i){                                  // race i of the tour: its own planet, same rivals
    int w=kw.weird, mir=kw.mirror, rev=kw.reverse; kw.weird = kw_tour==9; kw.mirror=0; kw.reverse=0;
    u32 seed=kw_tour_seed(kw_tour,i); kw_world(seed,kw_force(kw_tour_type[kw_tour])); kw.weird=(u8)w; kw.mirror=(u8)mir; kw.reverse=(u8)rev;
    k_type=1; k_cup_i=i; k_trk=(int)(hmix(seed^0x51u)%(u32)kt_count()); k_rec=-1; kt_official(k_trk,&kt); kt.laps=0; kw_race_setup(); }
static void kw_tour_start(void){
    kw_race_setup(); kw_tour_race(0); rc[0].pts=0; kart_go(NRC,-1,0,1); for(int r=0;r<NRC;r++) rc[r].pts=0; }
static void kw_endless_race(void){
    u32 seed=rnd(); int c = kw_endless_n/3; if(c>4) c=4; if(c>=kw.cls_open) c=kw.cls_open-1; k_cls=c;
    k_skill_adj = kw_endless_n*2; if(k_skill_adj>16) k_skill_adj=16; k_items=1; k_catch=1; k_stunts=0; kwd_palette();
    kw_world(seed,0); k_type=5; k_trk=(int)(rnd()%KW_TRACKS); k_rec=-1; kt_official(k_trk,&kt); kt.laps=2;
    char b[40]; scpy(b,"ENDLESS RUN: PLANET "); ncat(b,kw_endless_n+1); show_msg(b); kart_go(NRC,-1,0,1); }

// ---------------------------------------------------------------- after each Kart World race
static void kw_after_race(int me){                                 // returns through the race's end: what Kart World does next
    kw.coins=(u32)(gold>0?gold:0); kw.races++; if(me==1) kw.wins++; kw.look=kl; for(int p=0;p<NKPART;p++) kw.own[p]=kown[p]; kw_store(); }
static void kw_endless_next(int me){
    volatile u16 *pg=VISPG; const char *ln[3]; char a[48], b[48];
    if(me<=3){ kw_endless_n++; int bonus=40+kw_endless_n*15; gold+=bonus; scpy(a,"YOU MADE IT! PLANETS SURVIVED: "); ncat(a,kw_endless_n); ln[0]=a; scpy(b,"BONUS +"); ncat(b,bonus); scat(b," COINS. NEXT PLANET..."); ln[1]=b;
        ln[2]= kw_endless_n%3==0 ? "THE NEXT CLASS IS FASTER. HOLD ON." : "FINISH IN THE TOP 3 TO KEEP GOING."; card(pg,"ENDLESS RUN",ln,3); kw_after_race(me); kw_endless_race(); return; }
    scpy(a,"YOU SURVIVED "); ncat(a,kw_endless_n); scat(a," PLANET"); if(kw_endless_n!=1) scat(a,"S"); ln[0]=a;
    if(kw_endless_n>kw.endless_best){ kw.endless_best=(u16)kw_endless_n; ln[1]="A NEW BEST! IT IS IN YOUR RECORDS."; } else { scpy(b,"YOUR BEST IS "); ncat(b,kw.endless_best); ln[1]=b; }
    ln[2]="THE RUN IS OVER."; card(pg,"ENDLESS RUN",ln,3); kw_after_race(me); kart_stop(); }

// ---------------------------------------------------------------- elimination: every 20 seconds the kart in last place is out
static void kw_elim_tick(int n){
    if(k_type!=6 || ks_state!=KS_RACE) return; k_elim_t+=n; if(k_elim_t<1200) return; k_elim_t=0;
    int last=-1, lp=0; for(int r=0;r<nrc;r++){ if(rc[r].fin || rc[r].out) continue; if(rc[r].place>lp){ lp=rc[r].place; last=r; } } if(last<0) return;
    int left=0; for(int r=0;r<nrc;r++) if(!rc[r].fin && !rc[r].out) left++;
    Racer *k=&rc[last]; k->out=1; k->place=(u8)left; k_elim_out++; k->v=0; if(last && k->po!=255) po[k->po].alive=0;
    char b[40]; if(last==0){ scpy(b,"YOU ARE OUT! "); scat(b,kplace_s(left)); show_msg(b); sfx(4); ks_state=KS_DONE; k_end_t=0; return; }
    scpy(b,k->name); b[16]=0; scat(b," IS OUT!"); show_msg(b); sfx(9);
    if(left-1==1){ kfinish(0); kw.elim_wins++; } }

// ---------------------------------------------------------------- the options screen (quick race, time attack, elimination, the track maker)
enum { KO_PLANET, KO_TYPE, KO_SIZE, KO_WEIRD, KO_TRACK, KO_CLASS, KO_LAPS, KO_RIV, KO_ITEMS, KO_STUNTS, KO_SKILL, KO_CATCH, KO_MIRROR, KO_REV, KO_TIME, KO_WEATHER, KO_GO, NKO };
static const char *const ko_name[NKO]={"PLANET","WORLD TYPE","SIZE","WEIRD WORLDS","TRACK","CLASS","LAPS","RIVALS","ITEMS","STUNTS","RIVAL SKILL","CATCH-UP","MIRROR","REVERSE","TIME OF DAY","WEATHER",""};
static void kcomma(char *b, u32 v){ char t[16]; int n=0; do { t[n++]=(char)('0'+v%10); v/=10; if(v && n%4==3) t[n++]=','; } while(v); char r[16]; for(int i=0;i<n;i++) r[i]=t[n-1-i]; r[n]=0; scat(b,r); }   // 1,234,567
static u32 kw_num_edit(volatile u16 *pg, const char *title, u32 v, int nd, u32 lo, u32 hi){   // type a big number, digit by digit
    char d[10]; for(int i=nd-1;i>=0;i--){ d[i]=(char)('0'+v%10); v/=10; } int cur=nd-1, t=0, x0=120-nd*6; wait_release();
    while(1){ box(pg,24,56,192,46); rect(pg,25,57,190,12,6); ctext(pg,120,60,title,C_GOLD);
        for(int i=0;i<nd;i++){ char c[2]={d[i],0}; rect(pg,x0+i*12,74,10,12,0); text(pg,x0+i*12,76,c,i==cur?C_GOLD:C_WHITE,2); if(i==cur && (t&16)) rect(pg,x0+i*12,87,6,1,C_GOLD); }
        ui_chint(pg,120,93,"UP/DOWN: DIGIT  LEFT/RIGHT: MOVE  A: OK",C_GREY); vsync(); read_keys(); t++;
        if(HIT(K_UP)) d[cur]=(char)(d[cur]=='9'?'0':d[cur]+1); if(HIT(K_DOWN)) d[cur]=(char)(d[cur]=='0'?'9':d[cur]-1);
        if(HIT(K_LEFT) && cur>0) cur--; if(HIT(K_RIGHT) && cur<nd-1) cur++;
        if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ u32 s=0; for(int i=0;i<nd;i++){ u32 dg=(u32)(d[i]-'0'); if(s>(0xFFFFFFFFu-dg)/10u){ s=0xFFFFFFFFu; break; } s=s*10u+dg; }
            if(s<lo) s=lo; if(s>hi) s=hi; wait_release(); return s; } } }
static void kw_seed_edit(volatile u16 *pg){ kw.seed=kw_num_edit(pg,"PLANET NUMBER (0 TO 4,294,967,295)",kw.seed,10,0,0xFFFFFFFFu); }
static int kw_options_(volatile u16 *pg, int mode);
static int kw_options(volatile u16 *pg, int mode){ int sh=shd_begin(); int r=kw_options_(sh?VISPG:pg, mode); shd_end(sh); return r; }
static int kw_options_(volatile u16 *pg, int mode){                 // mode 0 quick race, 1 time attack, 2 elimination, 3 the track maker. 1 = go
    u8 show[NKO]; int ns=0; for(int i=0;i<NKO;i++){ int ok=1;
        if(mode==1 && (i==KO_RIV||i==KO_ITEMS||i==KO_SKILL||i==KO_CATCH||i==KO_CLASS)) ok=0;
        if(mode==2 && (i==KO_RIV||i==KO_LAPS)) ok=0;
        if(mode==3 && (i==KO_TRACK||i==KO_CLASS||i==KO_LAPS||i==KO_RIV||i==KO_ITEMS||i==KO_STUNTS||i==KO_SKILL||i==KO_CATCH||i==KO_MIRROR||i==KO_REV)) ok=0;
        if(ok) show[ns++]=(u8)i; }
    int sel=0, top=0, dirty=1, rep=0; KTrack tp; kw_dna(kw.seed,kw_force(kw.ftype)); if(kw.track>=KW_TRACKS) kw.track=0;
    static const char *const mt[4]={"QUICK RACE","TIME ATTACK","ELIMINATION","TRACK MAKER"};
    wait_release();
    while(1){
        if(dirty){ dirty=0; char b[48];
            ui_panel(pg,4,20,232,136); ui_head(pg,9,24,150,mt[mode],C_GOLD); { char g[20]="COINS "; ncat(g,kw.coins); text(pg,232-textlen(g)*4,24,g,C_GOLD,1); }
            if(sel<top) top=sel; if(sel>=top+11) top=sel-10;
            for(int j=0;j<11 && top+j<ns;j++){ int i=show[top+j], y=36+j*9; if(top+j==sel) ui_sel(pg,7,y-2,226,10,U_SEL);
                if(i==KO_GO){ ctext(pg,120,y, mode==3?"> GO AND BUILD <":"> START <", top+j==sel?C_GREEN:C_CYAN); continue; }
                text(pg,12,y,ko_name[i],top+j==sel?C_WHITE:C_GREY,1); b[0]=0;
                switch(i){ case KO_PLANET: scpy(b,"#"); kcomma(b,kw.seed); scat(b,"  "); scat(b,w_name); break; case KO_TYPE: scpy(b,kw_type_name[kw.ftype]); if(!kw.ftype){ scat(b," ("); scat(b,pc_name[wd.cls]); scat(b,")"); } break;
                    case KO_SIZE: scpy(b,kw.fsize?kpsize_name[kw.fsize-1]:"ANY"); if(!kw.fsize){ scat(b," ("); scat(b,kpsize_name[wd.psz]); scat(b,")"); } break; case KO_WEIRD: scpy(b,kw.weird?"ALWAYS":"NORMAL"); break;
                    case KO_TRACK: kt_peek((int)kw.track,&tp); kcomma(b,kw.track+1); scat(b,": "); scat(b,tp.name); break; case KO_CLASS: scpy(b,kcls_name[kw.cls]); break;
                    case KO_LAPS: if(kw.laps) ncat(b,kw.laps); else scpy(b,"AUTO"); break; case KO_RIV: ncat(b,kw.riv); break;
                    case KO_ITEMS: scpy(b,kw.items==0?"OFF":kw.items==1?"ON":"CRAZY"); break; case KO_STUNTS: scpy(b,kw.stunts==1?"LOTS! JUMPS AND LOOPS":kw.stunts==2?"NONE":"AS THE TRACK COMES"); break; case KO_SKILL: { static const char *const s[4]={"EASY","NORMAL","HARD","BRUTAL"}; scpy(b,s[kw.skill&3]); } break;
                    case KO_CATCH: scpy(b,kw.catchup?"ON":"OFF"); break; case KO_MIRROR: scpy(b,kw.mirror?"ON":"OFF"); break; case KO_REV: scpy(b,kw.reverse?"ON":"OFF"); break;
                    case KO_TIME: { static const char *const s[4]={"AS IT IS","DAY","DUSK","NIGHT"}; scpy(b,s[kw.hour&3]); } break; case KO_WEATHER: { static const char *const s[6]={"AS IT IS","CLEAR","RAIN","SNOW","DUST","EMBERS"}; scpy(b,s[kw.weather%6]); } break; }
                b[38]=0; text(pg,76,y,b,top+j==sel?C_GOLD:C_GREY,1); }
            if(top>0) text(pg,228,36,"^",C_WHITE,1); if(top+11<ns) text(pg,228,126,"v",C_WHITE,1);
            int i=show[sel]; const char *h = i==KO_PLANET ? "LEFT/RIGHT: NEXT PLANET THAT FITS   A: TYPE A NUMBER" : i==KO_TRACK ? "1,000,000 TRACKS ON EVERY PLANET. A: TYPE ONE" : i==KO_STUNTS ? "RAMPS, WATER JUMPS AND LOOP-DE-LOOPS" : i==KO_GO ? "A: GO!" : "LEFT/RIGHT: CHANGE IT";
            ctext(pg,120,136,h,C_CYAN); ui_chint(pg,120,145,"B: BACK   SELECT: RANDOM PLANET",C_DGREY); }
        vsync(); read_keys();
        int i=show[sel];
        if(HIT(K_UP)){ sel=(sel+ns-1)%ns; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%ns; dirty=1; sfx(7); }
        if(HIT(K_B)){ kw_store(); return 0; }
        int d=0, big=1; if(HELD(K_LEFT)||HELD(K_RIGHT)){ rep++; if(rep==1 || (rep>16 && !(rep&3) && (i==KO_PLANET||i==KO_TRACK))) d=HELD(K_RIGHT)?1:-1; if(i==KO_TRACK) big = rep>120 ? 10000 : rep>80 ? 1000 : rep>40 ? 100 : rep>24 ? 10 : 1; } else rep=0;
        if(HIT(K_SEL)){ kw.seed=kw_find(rnd(),kw.ftype,kw.fsize,1); kw.track=rnd()%KW_TRACKS; dirty=1; sfx(6); }
        if(d){ dirty=1; sfx(2);
            switch(i){ case KO_PLANET: kw.seed=kw_find(kw.seed+(u32)d,kw.ftype,kw.fsize,d); kw.track=0; break;
                case KO_TYPE: kw.ftype=(u8)((kw.ftype+d+NPC+1)%(NPC+1)); kw.seed=kw_find(kw.seed,kw.ftype,kw.fsize,1); kw.track=0; break;
                case KO_SIZE: kw.fsize=(u8)((kw.fsize+d+7)%7); kw.seed=kw_find(kw.seed,kw.ftype,kw.fsize,1); kw.track=0; break;
                case KO_WEIRD: kw.weird^=1; kw.seed=kw_find(kw.seed,kw.ftype,kw.fsize,1); kw.track=0; break;
                case KO_TRACK: kw.track=(u32)(((s32)kw.track+d*big+(s32)KW_TRACKS)%(s32)KW_TRACKS); break; case KO_CLASS: kw.cls=(u8)((kw.cls+d+kw.cls_open)%kw.cls_open); break;
                case KO_LAPS: kw.laps=(u8)((kw.laps+d+10)%10); break; case KO_RIV: kw.riv=(u8)((kw.riv+d+NRC)%NRC); break; case KO_ITEMS: kw.items=(u8)((kw.items+d+3)%3); break; case KO_STUNTS: kw.stunts=(u8)((kw.stunts+d+3)%3); break;
                case KO_SKILL: kw.skill=(u8)((kw.skill+d+4)%4); break; case KO_CATCH: kw.catchup^=1; break; case KO_MIRROR: kw.mirror^=1; break; case KO_REV: kw.reverse^=1; break;
                case KO_TIME: kw.hour=(u8)((kw.hour+d+4)%4); break; case KO_WEATHER: kw.weather=(u8)((kw.weather+d+6)%6); break; }
            kw_dna(kw.seed,kw_force(kw.ftype)); }
        if(HIT(K_A)){ if(i==KO_PLANET){ kw_seed_edit(pg); kw_dna(kw.seed,kw_force(kw.ftype)); dirty=1; }
            else if(i==KO_TRACK){ kw.track=kw_num_edit(pg,"TRACK NUMBER (1 TO 1,000,000)",kw.track+1,7,1,KW_TRACKS)-1; dirty=1; }
            else if(i==KO_GO){ kw_store(); sfx(6); return 1; } } } }

// ---------------------------------------------------------------- picking a tour, a driver; your trophies and records
static int kwt_sel; static void kwt_row(int i, char *b, int *c){ scpy(b,kw_tour_name[i]); while(textlen(b)<20) scat(b," ");
    for(int k=0;k<5;k++){ int m=kw.troph[i][k]; scat(b, m==3?"G ":m==2?"S ":m==1?"B ":"- "); } if(kw.troph[i][0]==3 && kw.troph[i][1]==3 && kw.troph[i][2]==3 && kw.troph[i][3]==3 && kw.troph[i][4]==3) *c=C_GOLD; }
static void kwt_desc(int i, char *b, int *c){ if(i<0){ scpy(b,"TROPHIES BY CLASS: LARVA PUPA IMAGO STARBUG HYPER"); return; } static const char *const d[NKWT]={"GREEN HILLS AND MEADOWS. A GENTLE START.","ICE WORLDS. THE ROADS ARE SLIPPERY.","DESERTS. DIRT ROADS AND DUNES.","OCEAN WORLDS. BRIDGES OVER THE SEA.",
    "MOUNTAIN WORLDS. UP AND DOWN AND UP.","JUNGLE WORLDS. THICK AND GREEN.","CITY PLANETS. NEON IN THE NIGHT.","DEATH PLANETS. EVERYTHING WANTS YOU GONE.","FOUR TINY WORLDS. TRACKS RIGHT ROUND THEM.","FOUR WEIRD WORLDS. ANYTHING CAN HAPPEN."}; scpy(b,d[i]); (void)c; }
static void kwd_row(int i, char *b, int *c){ int need=kwd_need[i], have=kw_trophies(0); if(i==0){ scpy(b,"YOUR BUG ("); scat(b,look.name); scat(b,")"); } else { scpy(b,kwd_name[i]); while(textlen(b)<14) scat(b," "); scat(b,kwd_kind[i]); }
    if(have<need){ scat(b,"   LOCKED: "); ncat(b,need); scat(b," TROPHIES"); *c=C_DGREY; } else if(i==kw.driver) *c=C_GOLD; }
static void kw_records(volatile u16 *pg){
    const char *ln[6]; char a[48], b2[48], c2[48], d2[48], e2[48];
    scpy(a,"RACES "); ncat(a,kw.races); scat(a,"   WINS "); ncat(a,kw.wins); ln[0]=a;
    scpy(b2,"TROPHIES "); ncat(b2,kw_trophies(0)); scat(b2,"/50   GOLD "); ncat(b2,kw_trophies(1)); ln[1]=b2;
    scpy(c2,"ENDLESS RUN BEST: "); ncat(c2,kw.endless_best); scat(c2," PLANETS"); ln[2]=c2;
    scpy(d2,"ELIMINATION WINS "); ncat(d2,kw.elim_wins); scat(d2,"   TIME ATTACK GOLDS "); ncat(d2,kw.tt_gold); ln[3]=d2;
    { int n=0; for(int i=0;i<16;i++) if(kw.best[i].key) n++; scpy(e2,"TRACK RECORDS KEPT "); ncat(e2,n); scat(e2,"   COINS "); ncat(e2,kw.coins); ln[4]=e2; }
    ln[5]="CLASSES: "; { EW static char f[48]; scpy(f,"CLASSES: "); for(int i=0;i<kw.cls_open;i++){ if(i) scat(f,", "); scat(f,kcls_name[i]); } ln[5]=f; }
    card(pg,"KART WORLD RECORDS",ln,6); }

// ---------------------------------------------------------------- the Kart World menu
static void kw_strip(volatile u16 *pg, int ph){                      // the chequered strip under the logo: it scrolls
    int o=ph&7, c0=(ph>>3)&1;
    for(int i=0;i<41;i++){ int x=40+i*4-o, w=4; if(x<40){ w=x+4-40; x=40; } if(x+w>200) w=200-x; if(w<=0) continue;
        int k=(i+c0)&1; rect(pg,x,27,w,2,k?C_WHITE:C_DGREY); rect(pg,x,29,w,2,k?C_DGREY:C_WHITE); } }
static void kw_paint_bg(volatile u16 *pg){
    for(int i=0;i<120*160;i++) pg[i]=0; u32 r=0x51ED27u; for(int k=0;k<90;k++){ r=r*1664525u+1013904223u; int x=(r>>8)%240, y=(r>>20)%160; pix(pg,x,y,(k%9)==0?5:(k&1)?3:2); }
    rect(pg,0,128,240,32,1); for(int x=0;x<240;x+=8){ rect(pg,x,128,4,2,C_DGREY); }
    // the logo: KART in gold, WORLD in cyan, a drop shadow, a chequered strip underneath, a little kart and flag at the sides
    text(pg,61,5,"KART",C_DGREY,3); text(pg,60,4,"KART",C_GOLD,3); text(pg,121,5,"WORLD",C_DGREY,3); text(pg,120,4,"WORLD",C_CYAN,3);
    kw_strip(pg,0);
    ico_draw(pg,22,6,IC_KART); ico_draw(pg,202,6,IC_FLAG); }
static int kw_menu(void){
    volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10)|(1<<12)|(1<<6); REG_BLDCNT=0;
    for(int i=0;i<256;i++) BG_PAL[i]=bg_pal[i]; obj_palette(); kart_build(0); kwd_palette();
    static const char *const it[14]={"GRAND TOUR","QUICK RACE","RANDOM RACE","TIME ATTACK","ELIMINATION","ENDLESS RUN","TRACK MAKER","YOUR TRACKS","GARAGE","DRIVER","RECORDS","HOW IT WORKS","BACK TO THE TITLE","CAMERA"};
    static const char *const dsc[14]={"10 TOURS OF 4 RACES ON 4 PLANETS. WIN TROPHIES.","ANY OF 4.3 QUADRILLION TRACKS. EVERY OPTION.","ONE BUTTON. A SURPRISE PLANET AND TRACK.","JUST YOU AND THE CLOCK.","EVERY 20 SECONDS THE LAST KART IS OUT.",
        "TOP 3 GOES ON TO A NEW PLANET. HOW FAR CAN YOU GET?","BUILD A TRACK ON ANY PLANET YOU LIKE.","EVERY TRACK YOU HAVE SAVED. RACE ANY OF THEM.","DRESS UP YOUR KART. COINS BUY PARTS.","WHO DRIVES? YOUR BUG OR NEARLY ANYONE.","TROPHIES, WINS AND BEST TIMES.","THE BUTTONS AND THE BASICS.","BACK TO BUGSTAR.","DISTANCE, HEIGHT, FIELD OF VIEW, SWING, COCKPIT."};
    int sel=0, top=0, dirty=1, t=0; kw_paint_bg(pg); int sh=shd_begin(); wait_release(); fade(0);
    while(1){
        if(dirty){ dirty=0; char b[48]; rect(pg,0,33,148,96,0);                                  // the list sits on a card, 7 rows at a time, and scrolls
            static const u8 ord[14]={0,1,2,3,4,5,6,7,8,9,13,10,11,12};
            if(sel<top) top=sel; if(sel>=top+7) top=sel-6;
            ui_panel(pg,3,33,142,95);
            for(int r=0;r<7;r++){ int j=top+r, i=ord[j], y=38+r*12;
                if(j==sel){ ui_sel(pg,7,y-2,134,11,U_SEL); rect(pg,10,y+1,3,3,C_GOLD); }
                text(pg,18,y,it[i],j==sel?C_GOLD:U_SOFT,1);
                if(i==9){ scpy(b,kw.driver?kwd_name[kw.driver]:look.name); b[9]=0; rtext(pg,136,y,b,C_CYAN); } }
            if(top>0) text(pg,136,36,"^",C_WHITE,1); if(top+7<14) text(pg,136,119,"v",C_WHITE,1);
            { static const u8 ord2[14]={0,1,2,3,4,5,6,7,8,9,13,10,11,12}; rect(pg,0,130,240,30,1); ctext(pg,120,134,dsc[ord2[sel]],C_CYAN); }
            scpy(b,"COINS "); ncat(b,kw.coins); scat(b,"    TROPHIES "); ncat(b,kw_trophies(0)); scat(b,"/50    WINS "); ncat(b,kw.wins); ctext(pg,120,146,b,C_GOLD); }
        // your kart on the right, turning slowly on its stand
        { int view=(t>>7)&1, cx=192, gy=110, inv=110, b2=kl.part[KP_BODY], bob=(t>>4)&1; const u8 *an=kb_anchor+b2*3; rect(pg,150,40,90,80,0); rect(pg,154,gy+2,76,2,C_DGREY);
          set_affine(29,inv,0,0,inv); int kh=16*256/inv, slot=0;
          shadow_oam[slot].a0=((gy-kh/2-bob-16)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((cx-32)&511)|(29<<9)|(2<<14); shadow_oam[slot].a2=(T_KDYN+view*8)|(7<<12); slot++;
          if(kl.part[KP_TOP]){ int tp = view ? an[2] : an[0], tx = view ? cx+(an[1]+3-16)*256/inv : cx, ty=gy-(16-tp)*256/inv-4*256/inv-bob;
              shadow_oam[slot].a0=((ty-8)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((tx-16)&511)|(29<<9); shadow_oam[slot].a2=(T_TOP1+(kl.part[KP_TOP]-1)*2); slot++; }
          int feet=gy-kb_seat[b2]*256/inv-bob, dx2 = view ? (12-16)*256/inv : 0;
          int s2=kw_draw_driver(slot,cx+dx2,feet+2,180,0); if(s2>=0) slot=s2;
          else if(view){ set_affine(31,-200,0,0,200); int dh=16*256/200; bug_obj(slot,cx+dx2,feet-dh/2+3,31,200); shadow_oam[slot].a2=bug_tile(0)|BUG_BANK; slot++; }
          else { set_affine(31,330,0,0,330); int cy2=feet-31*256/330; shadow_oam[slot].a0=((cy2-64)&255)|(1<<8)|(1<<9); shadow_oam[slot].a1=((cx-64)&511)|(31<<9)|(3<<14); shadow_oam[slot].a2=T_BACK|BUG_BANK; slot++; }
          for(;slot<128;slot++) shadow_oam[slot].a0=0x0200; }
        kw_strip(pg,t>>1); vsync(); oam_flush(); read_keys(); t++; rng_s+=t;
        if(HIT(K_UP)){ sel=(sel+13)%14; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%14; dirty=1; sfx(7); }
        if(HIT(K_B)){ sel=13; dirty=1; }
        if(HIT(K_A)||HIT(K_START)){ static const u8 ord3[14]={0,1,2,3,4,5,6,7,8,9,13,10,11,12}; sfx(2); oam_hide_all(); oam_flush(); wait_release(); shd_end(sh); return ord3[sel]; } } }
static void kw_help(volatile u16 *pg){
    const char *a[6]={"UP: GO.  DOWN: BRAKE.  LEFT/RIGHT: STEER.","B: BOOST (THE YELLOW BAR).  L: YOUR HORN.","HOLD R IN A TURN TO DRIFT. LET GO FOR A TURBO.","DRIVE THROUGH ? BOXES FOR ITEMS. A: USE ONE.","STAY ON THE ROAD. GRASS AND WATER SLOW YOU.","ORANGE ARROWS ON THE ROAD: A FREE BOOST."};
    card(pg,"HOW IT WORKS",a,6);
    const char *b[5]={"4,294,967,296 PLANETS. A MILLION TRACKS ON EACH.","THAT IS ABOUT 4.3 QUADRILLION TRACKS.","WIN TOURS FOR TROPHIES. TROPHIES OPEN","CLASSES AND DRIVERS. COINS BUY GARAGE PARTS.","JUMPS, BOOSTS, ITEMS. GO GO GO."};
    card(pg,"KART WORLD",b,5); }
static void kw_rmy_row(int i, char *b, int *c){ KRec r; krec_get(kmy_idx[i],&r); scpy(b,r.name); while(textlen(b)<17) scat(b," "); char w[32]; kw_rec_where(&r,w); w[24]=0; scat(b,w); if(r.at.star==0xFF) *c=C_CYAN; }
static int kw_my_tracks(volatile u16 *pg){                         // race any saved track: Kart World builds its planet, wherever it was
    KRec r; kmy_n=0; for(int i=0;i<NKREC;i++) if(krec_get(i,&r)) kmy_idx[kmy_n++]=(u8)i;
    if(!kmy_n){ const char *ln[2]={"NO SAVED TRACKS YET.","BUILD ONE IN THE TRACK MAKER."}; card(pg,"YOUR TRACKS",ln,2); return 0; }
    int sel=0; list_desc=0; int k=list_pick(pg,"YOUR TRACKS: RACE ANY OF THEM",kmy_n>11?11:kmy_n,kw_rmy_row,&sel,"A: RACE IT   B: BACK"); if(k<0) return 0;
    int ri=kmy_idx[k]; krec_get(ri,&r);
    if(r.at.star==0xFF){ kw_world(r.at.seed,r.at.body); }
    else { for(int w=0;w<UAX;w++){ clus[w]=r.at.cx[w]; clus[UAX+w]=r.at.cy[w]; } { u32 fu[UW]; fav_uni(&r.at,fu); uni_set(fu); } clus_refold(); gen_cluster(); cur_sys=r.at.star; cur_pl=r.at.body>>3; cur_moon=r.at.body&7;
        land_on_current(); fade(1); landing_screen(0); new_planet(); planet_enter(); scene=0; msg_timer=0; }
    kw_race_setup(); krec_to_kt(&r); k_rec=ri; k_trk=-1; k_cls = r.cls<kw.cls_open ? r.cls : 0; k_type=4; kart_go(r.riv+1,-1,0,1); return 1; }

// ---------------------------------------------------------------- the whole mode
static void kw_run(void){                                          // the planet, with a race on it, until the race says we are done
    while(!kw_quit){ read_keys(); frames++; if(invuln) invuln--; rng_s+=keys; planet_frame();
        vsync(); if(m7_on){ oam_flush(); m7_after_vsync(); } else { REG_DISPCNT=(REG_DISPCNT&~(1<<4))|(page_back?(1<<4):0); page_back^=1; oam_flush(); }
        if(kg_fadein && !--kg_fadein) fade(0); }                                                         // (a race that has just started: its first pictures are drawn in the dark, then it fades in)
    fade(1); oam_hide_all(); oam_flush(); music(2); }
static void kart_world(void){
    shd_off();
    kw_load(); if(!kw.unlocked) return;
    kw_mode=1; cur_slot=last_slot; grind_load(); hum_load(); form=0; obj_palette(); bug_build();
    galaxy_no=0; for(int w=0;w<NCW;w++) clus[w]=0; for(int k=0;k<NGH;k++) galaxy_hi[k]=0; clus_refold(); uni_set32(1); cluster_no=0; zone=0; zone_cleared=1; cur_sys=0; gen_cluster();
    gold=kw.coins; kl=kw.look; for(int p=0;p<NKPART;p++) kown[p]=kw.own[p]; for(int i=0;i<8;i++) kcam[i]=kw.cam[i]; kcam[1]%=5; kcam[2]%=5; kcls_open=(u8)kw.cls_open; pq_type=0; opt_hunger=0; music(2);
    { volatile u16 *ov=OBJ_VRAM; const u16 *t=obj_tiles; for(unsigned i=0;i<sizeof(obj_tiles)/2;i++) ov[i]=t[i]; } bug_build();
    while(1){
        int m=kw_menu(); volatile u16 *pg=(volatile u16*)0x06000000; kw_quit=0;
        if(m==12) break;
        if(m==11){ kw_help(pg); continue; }
        if(m==13){ kcam_menu(pg,0); kw_keep(); continue; }
        if(m==10){ kw_records(pg); continue; }
        if(m==8){ in_menu++; kgarage(pg); in_menu--; kw.look=kl; for(int p=0;p<NKPART;p++) kw.own[p]=kown[p]; kw.coins=(u32)gold; kw_store(); continue; }
        if(m==9){ int s=kw.driver; list_desc=0; while(1){ int k=list_pick(pg,"WHO DRIVES?",NKWD>13?13:NKWD,kwd_row,&s,"A: PICK   B: BACK"); if(k<0) break; if(kw_trophies(0)<kwd_need[k]){ say("WIN MORE TROPHIES FOR THIS DRIVER"); continue; } kw.driver=(u8)k; kw_store(); sfx(6); break; } continue; }
        if(m==0){ kwt_sel=0; list_desc=kwt_desc; int t=list_pick(pg,"GRAND TOUR: PICK A TOUR",NKWT,kwt_row,&kwt_sel,"A: PICK   B: BACK"); list_desc=0; if(t<0) continue;
            int c=kpick_class(pg); if(c<0) continue; k_cls=c; kw_tour=t; kw_tour_start(); kw_run(); continue; }
        if(m==1 || m==3 || m==4){ int mode = m==1?0 : m==3?1 : 2; if(!kw_options(pg,mode)) continue; k_cls = mode==1 ? 1 : kw.cls;
            kw_start_planet_race(kw.seed,kw.track, mode==1 ? 1 : mode==2 ? NRC : kw.riv+1, mode==1 ? 2 : mode==2 ? 6 : 0); k_elim_t=0; k_elim_out=0; kw_run(); continue; }
        if(m==2){ kw.seed=rnd(); kw_dna(kw.seed,0); u32 tr=rnd()%KW_TRACKS; k_cls=(int)(rnd()%(u32)kw.cls_open); int keep_m=kw.mirror, keep_r=kw.reverse; kw.mirror=(u8)(rnd()&1); kw.reverse=(u8)((rnd()>>3)&1);
            int fk=kw.ftype, fz=kw.fsize; kw.ftype=0; kw.fsize=0; kw_start_planet_race(kw.seed,tr,NRC,0); kw.ftype=(u8)fk; kw.fsize=(u8)fz; kw.mirror=(u8)keep_m; kw.reverse=(u8)keep_r; kw_run(); continue; }
        if(m==5){ kw_endless_n=0; kw_endless_race(); kw_run(); continue; }
        if(m==6){ if(!kw_options(pg,3)) continue; kw_world(kw.seed,kw_force(kw.ftype)); k_cls=kw.cls<kw.cls_open?kw.cls:0; fade(0); kbuild_begin(); kw_run(); continue; }
        if(m==7){ if(kw_my_tracks(pg)) kw_run(); continue; } }
    kw.coins=(u32)(gold>0?gold:0); kw.look=kl; for(int p=0;p<NKPART;p++) kw.own[p]=kown[p]; kw_store();
    kw_mode=0; kr_mode=0; kart_on=0; opt_hunger=1; k_skill_adj=0; k_items=1; k_catch=1; k_stunts=0; settings_load(); fade(1); }   // (story mode races are run the normal way)

// ---------------------------------------------------------------- the little things the race code asks Kart World for
static int kw_driver_on(void){ return kw.driver!=0; }
static void kw_keep(void){ for(int i=0;i<8;i++) kw.cam[i]=kcam[i]; kw.look=kl; for(int p=0;p<NKPART;p++) kw.own[p]=kown[p]; kw.coins=(u32)(gold>0?gold:0); kw_store(); }
static void kw_tt_gold(void){ kw.tt_gold++; }
static int kw_best_get(int trk, KBest *b){ return kw_best_find(kw_key(kw.seed,(u32)trk),b)>=0; }
static void kw_best_set(int trk, int t, int medal){ kw_best_put(kw_key(kw.seed,(u32)trk),t,medal); }
static void kw_tour_done(int me){                                  // the tour is over: a trophy, and maybe a faster class
    int m = me==1 ? 3 : me==2 ? 2 : me==3 ? 1 : 0; if(m>kw.troph[kw_tour][k_cls]) kw.troph[kw_tour][k_cls]=(u8)m;
    if(me==1 && k_cls<4 && kw.cls_open<k_cls+2) kw.cls_open=(u16)(k_cls+2); kcls_open=(u8)kw.cls_open; kw_after_race(me); }
static void kw_topbar(void){
    { volatile u16 *pg=hud_begin(); rect(pg,0,0,240,18,0); rect(pg,0,17,240,1,6); char b[48];
        scpy(b,"KART WORLD  #"); kcomma(b,(u32)world_no); text(pg,4,2,b,C_GOLD,1);
        scpy(b,w_name); scat(b,"  "); scat(b,pc_name[wd.cls]); b[40]=0; text(pg,4,10,b,C_GREY,1);
        scpy(b,"COINS "); ncat(b,gold); text(pg,236-textlen(b)*4,2,b,C_GOLD,1); scpy(b,kpsize_name[wd.psz]); scat(b," PLANET"); text(pg,236-textlen(b)*4,10,b,C_CYAN,1); hud_end(); } }
