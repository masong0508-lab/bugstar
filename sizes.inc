// ================================================================= HOW BIG IS IT? The game's huge numbers, every digit, said out loud
// It opens once the story is beaten (by any save: the same flag as Kart World). Each page is one real number from the game, worked out
// exactly: the smaller ones here with big whole numbers (24 words: up to 231 digits), the giants (up to 78,914 digits) by bignums.py
// when the game is built. The number on show is always packed decimal digits, two to a byte, so any part of it can be drawn at once.
// Three views: IN SHORT (ABOUT 4.3 BILLION), EVERY DIGIT (with commas, in big type, scrolling) and BY NAME (each group of three digits
// with its name: 4 BILLION, 294 MILLION...). Names past the common ones follow the Conway-Wechsler rules (see illion in space.inc).
#define BNW 24
static void bn_set(u32 *b, u32 v){ b[0]=v; for(int i=1;i<BNW;i++) b[i]=0; }
static void bn_pow2(u32 *b, int e){ bn_set(b,0); b[e>>5]=1u<<(e&31); }
static void bn_mul(u32 *b, u32 k){ big_mul(b,BNW,k); }
static int big_unlocked(void){ if(ant_king) return 1; KWSave keep=kw; int u=kw_unlocked(); kw=keep; return u; }   // beaten the story in this save, or in any

// ---- the number on show
static const u8 *nb_d; static int nb_n, nb_nz, nb_about;         // its packed digits, how many digits, how many groups are not 000, and whether it is an estimate
static int nb_ngv, nb_leadv;                                     // how many groups of three digits it has, and how many digits the first group has
EW static u8 nb_ram[128];                                        // (the numbers worked out here are packed into this)
static inline int nb_dig(int k){ u8 b=nb_d[k>>1]; return (k&1) ? (b&15) : (b>>4); }
static int nb_ng(void){ return nb_ngv; }                         // groups of three digits (the first can be shorter)
static int nb_lead(void){ return nb_leadv; }                     // how many digits the first group has: 1, 2 or 3
static int nb_bign;                                              // BY NAME uses big type for this number's names (its biggest name fits)
static void nb_shape(void){ nb_ngv=(nb_n+2)/3; nb_leadv=nb_n-3*(nb_ngv-1); nb_bign = textlen(illion(nb_ngv-1))<=24; }
static int nb_grp(int j){ int a = j ? nb_lead()+3*(j-1) : 0, e = j ? a+3 : nb_lead(), v=0; for(int k=a;k<e;k++) v=v*10+nb_dig(k); return v; }   // group j, from the left
static int nz_next(int j){ int ng=nb_ng(); do j++; while(j<ng && !nb_grp(j)); return j; }   // the next group that is not 000 (the first group never is)
static int nz_prev(int j){ do j--; while(j>0 && !nb_grp(j)); return j<0?0:j; }
static void nb_rom(const u8 *d, int nd, int nz){ nb_d=d; nb_n=nd; nb_nz=nz; nb_about=0; nb_shape(); }
static void nb_from(const u32 *v, int about){                    // put a big number on show (an estimate is rounded to 3 figures)
    EW static char tmp[248], d[248]; u32 t[BNW]; for(int i=0;i<BNW;i++) t[i]=v[i]; int n=0;
    if(big_zero(t,BNW)) tmp[n++]='0';
    while(!big_zero(t,BNW) && n<234){ u32 r=big_div(t,BNW,1000000000u); for(int k=0;k<9;k++){ tmp[n++]=(char)('0'+r%10); r/=10; } }
    while(n>1 && tmp[n-1]=='0') n--;
    for(int i=0;i<n;i++) d[i]=tmp[n-1-i];
    if(about && n>3){ int up=d[3]>='5'; for(int k=3;k<n;k++) d[k]='0';
        if(up){ int k=2; while(k>=0 && d[k]=='9'){ d[k]='0'; k--; } if(k>=0) d[k]++; else { for(int i=n;i>0;i--) d[i]=d[i-1]; d[0]='1'; n++; } } }
    for(int i=0;i<(n+1)/2;i++){ int hi=d[2*i]-'0', lo = 2*i+1<n ? d[2*i+1]-'0' : 0; nb_ram[i]=(u8)(hi*16+lo); }
    nb_d=nb_ram; nb_n=n; nb_about=about; nb_shape(); nb_nz=0; for(int j=0;j<nb_ng();j++) if(nb_grp(j)) nb_nz++; if(!nb_nz) nb_nz=1; }   // (0 is said as 0)

// ---- the pages, smallest first (after your own travels)
enum { SZ_TRAVEL, SZ_COLOURS, SZ_DOTS, SZ_RELICS, SZ_NAMES, SZ_KWPLAN, SZ_UNIS, SZ_WEIRD, SZ_BUGNAMES, SZ_BEAST, SZ_KWTRK, SZ_BEASTMAX, SZ_GOLD, SZ_SHIPS,
       SZ_TRKNAMES, SZ_RACES, SZ_KARTS, SZ_DNA, SZ_TROPHIES, SZ_TOUR, SZ_FAR, SZ_WIDE, SZ_HUMS, SZ_BUGS, SZ_WIDEALL, SZ_CLUS, SZ_STARS, SZ_WORLDS,
       SZ_STARSALL, SZ_WORLDSALL, SZ_LOOKS, SZ_ORDER, SZ_TRIPS, SZ_FAVS, SZ_MAPS, SZ_SAVES, SZ_SCREENS, NSOLD_E };
#define NSIZE (NSOLD+NFACT)                                       // the game's own numbers and the facts from facts.py, in sz_ord's order
static const char *const sz_otxt[NSOLD][3]={
    {"LIGHT YEARS YOU HAVE FLOWN","IN THIS SAVE, SO FAR",0},
    {"COLOURS YOU CAN MIX","32 REDS, 32 GREENS AND 32 BLUES","MIX THEM IN THE MIRROR, THE HANGAR OR THE GARAGE"},
    {"DOTS ON THE SCREEN","240 ACROSS AND 160 DOWN.","EVERY PICTURE IN THE GAME IS MADE OF THESE"},
    {"RELIC COLLECTIONS","THERE ARE 16 LEGENDARY RELICS.","EVERY WAY TO HAVE SOME OF THEM"},
    {"STAR AND PLANET NAMES","2 OR 3 OF 32 SOUNDS, THEN ONE OF 8 ENDINGS","LIKE ZANIGLO MAJOR OR PODOR PRIME"},
    {"PLANETS IN KART WORLD","EVERY PLANET NUMBER IS A WHOLE WORLD TO RACE ON",0},
    {"UNIVERSES","AFTER THE STORY YOUR SHIP CAN JUMP TO ANY OF THEM",0},
    {"WEIRD WORLD MIXES","1 TO 4 OF THE 1,000 WEIRD TRAITS,","NEVER TWO OF THE SAME KIND"},
    {"BUG NAMES YOU CAN TYPE","UP TO 8 LETTERS: A TO Z, AND SPACES","(SHIP NAMES HAVE THE SAME RULES)"},
    {"THE TECHNOBEAST'S HEALTH","IN ITS LAST FORM, THE FIRST TIME YOU MEET IT.","EACH TIME IT FALLS IT COMES BACK 10 TIMES STRONGER"},
    {"TRACKS IN KART WORLD","1,000,000 TRACKS ON EVERY ONE OF THOSE PLANETS",0},
    {"THE STRONGEST TECHNOBEAST","ITS HEALTH AFTER IT FALLS 6 TIMES.","IT NEVER GETS STRONGER THAN THIS"},
    {"THE MOST GOLD YOU CAN HAVE","THE BIGGEST NUMBER THE GAME'S 64-BIT COUNTERS HOLD.","YOUR LEVEL AND XP STOP THERE TOO"},
    {"SHIPS YOU CAN BUILD","EVERY HANGAR PART AND 3 COLOURS","(NOT EVEN COUNTING NAMES)"},
    {"TRACK NAMES YOU CAN TYPE","UP TO 15 LETTERS AND NUMBERS",0},
    {"KART RACES YOU CAN SET UP","EVERY WAY TO SET THE 16 OPTIONS","ON KART WORLD'S QUICK RACE SCREEN"},
    {"KARTS YOU CAN BUILD","EVERY GARAGE PART, 3 COLOURS AND A NUMBER",0},
    {"PLANET DNA CODES","EVERY PLANET GROWS FROM 3 CODES OF 32 BITS:","ONE FOR ITS DNA, ONE FOR ITS LAND, ONE FOR ITS LIFE"},
    {"KART WORLD TROPHY SHELVES","10 TOURS X 5 CLASSES = 50 TROPHIES.","EACH ONE: NONE, BRONZE, SILVER OR GOLD"},
    {"WAYS TO TOUR A CLUSTER","EVERY ORDER TO VISIT ALL 32 STARS IN A CLUSTER","(32 X 31 X 30 X ... X 2 X 1)"},
    {"THE FARTHEST FROM HOME","A UNIVERSE WRAPS ROUND, SO NOWHERE IS FURTHER.","IN LIGHT YEARS, THE WAY THE GAME MEASURES IT"},
    {"HOW WIDE A UNIVERSE IS","IN LIGHT YEARS, FROM ONE SIDE TO THE OTHER",0},
    {"HUMANOIDS YOU CAN MAKE","BODIES, HAIR, CLOTHES, HATS AND 9 COLOURS",0},
    {"BUGS YOU CAN MAKE","EVERY PART AND 10 COLOURS","(NOT EVEN COUNTING NAMES)"},
    {"HOW WIDE THEY ALL ARE","ALL 4,294,967,296 UNIVERSES SIDE BY SIDE,","IN LIGHT YEARS"},
    {"STAR CLUSTERS IN A UNIVERSE","IT WRAPS ROUND: FLY FAR ENOUGH AND YOU COME HOME",0},
    {"STARS IN A UNIVERSE","32 STARS IN EVERY CLUSTER",0},
    {"WORLDS IN A UNIVERSE","PLANETS AND MOONS TO LAND ON:","ABOUT 5.6 FOR EVERY STAR"},
    {"STARS IN EVERY UNIVERSE","ALL 4,294,967,296 UNIVERSES TOGETHER",0},
    {"WORLDS IN EVERY UNIVERSE","EVERY PLANET AND MOON YOU COULD LAND ON",0},
    {"EVERY LOOK YOU COULD HAVE","YOUR BUG, HUMANOID, SHIP AND KART, ALL TOGETHER",0},
    {"WAYS TO VISIT 256 FAVOURITES","A FULL FAVOURITES LIST, VISITED IN EVERY ORDER","(256 X 255 X 254 X ... X 2 X 1)"},
    {"TRIPS OF 1,000 JUMPS","EACH JUMP GOES TO ONE OF THE OTHER 31 STARS","IN YOUR CLUSTER"},
    {"FAVOURITES LISTS","256 SLOTS. EACH ONE EMPTY, OR ANY STAR","IN ANY OF THE 4,294,967,296 UNIVERSES"},
    {"PLANET MAPS","128 X 128 SPOTS, EACH ONE OF 256 HEIGHTS:","THIS MANY DIFFERENT MAPS COULD BE MADE"},
    {"SAVE FILES","THE SAVE CHIP HOLDS 262,144 BITS, EACH ON OR OFF.","EVERY SAVE THERE COULD EVER BE IS ONE OF THESE"},
    {"PICTURES THE SCREEN CAN SHOW","240 X 160 DOTS, EACH ONE OF 32,768 COLOURS:","EVERY GAME BOY ADVANCE PICTURE THERE COULD BE"}};
static void bn_set64(u32 *b, u64 x){ bn_set(b,(u32)x); b[1]=(u32)(x>>32); }
static u64 nck(int n, int k){ if(k<0 || k>n) return 0; u64 r=1; for(int i=1;i<=k;i++) r=r*(u64)(n-k+i)/(u64)i; return r; }   // n choose k
__asm__(".pushsection .rodata\n.balign 4\n.global fact_bin\nfact_bin:\n.incbin \"facts.bin\"\n.popsection");   // every fact's digits, packed
extern const u8 fact_bin[];
static const char *szt(int pi, int k){ int id=sz_ord[pi]; return id<NSOLD ? sz_otxt[id][k] : fact_txt[id-NSOLD][k]; }   // page pi's text
static void size_set(int i){                                     // put page i's number on show
    i=sz_ord[i]; if(i>=NSOLD){ int f=i-NSOLD; nb_rom(fact_bin+fact_off[f],(int)fact_nd[f],(int)fact_nz[f]); nb_about=fact_ab[f]; return; }
    static const u8 bug_n[13]={3,5,4,12,8,6,8,3,4,6,6,5,12}, hum_n[11]={2,5,3,10,8,6,10,6,5,12,6}, ship_n[15]={NHULL,3,3,3,NWING,3,3,3,4,3,3,4,4,2,4};   // the options in each maker
    static const u8 race_n[14]={NPC+1,7,2,5,10,8,3,3,4,2,2,2,4,6};   // Kart World's quick race: world type, size, weird, class, laps, rivals, items, stunts, skill, catch-up, mirror, reverse, time, weather
    EW static u32 v[BNW]; int about=0;
    switch(i){
    case SZ_TRAVEL: { u32 t[LYW]; for(int k=0;k<LYW;k++) t[k]=tv_ly[k]; big_div(t,LYW,10); bn_set(v,0); for(int k=0;k<LYW;k++) v[k]=t[k]; } break;   // (the log keeps tenths)
    case SZ_COLOURS: bn_set(v,32*32*32); break;                                   // the colour mixer's three sliders (see pick_colour)
    case SZ_DOTS: bn_set(v,240*160); break;
    case SZ_RELICS: bn_pow2(v,16); break;                                         // each of the 16 relics: found or not
    case SZ_NAMES: bn_set(v,(32*32+32*32*32)*8); break;                           // name_of: 2 or 3 of its 32 sounds, then one of 8 endings (no two make the same name)
    case SZ_KWPLAN: case SZ_UNIS: bn_pow2(v,32); break;
    case SZ_WEIRD: { u64 t=0, p=1; for(int k=1;k<=4;k++){ p*=25; u64 s=0;          // k kinds (never two of one kind, nor SHY+ANGRY, FLOOD+DROUGHT, MAGNET+SHY ORE), 25 strengths each
            for(int j=0;j<=3 && j<=k;j++) s += nck(3,j)*((u64)1<<j)*nck(NMW-6,k-j); t+=p*s; } bn_set64(v,t); } break;
    case SZ_BUGNAMES: { u64 t=1; for(int k=0;k<8;k++) t*=27; bn_set64(v,t-1); } break;   // 27 signs in each of up to 8 places, not ending in a space
    case SZ_BEAST: bn_set(v,1000000); bn_mul(v,1000000); break;
    case SZ_KWTRK: bn_pow2(v,32); bn_mul(v,KW_TRACKS); break;
    case SZ_BEASTMAX: bn_set64(v,1000000000000000000ULL); break;                  // b3_max stops growing here (see the beast in space.inc)
    case SZ_GOLD: bn_set64(v,(u64)S64_MAX); break;
    case SZ_TRKNAMES: bn_set(v,1); for(int k=0;k<15;k++) bn_mul(v,37); for(int k=0;k<BNW;k++){ u32 o=v[k]--; if(o) break; } break;   // 37 signs (A-Z, 0-9, space) in up to 15 places
    case SZ_RACES: bn_pow2(v,32); bn_mul(v,KW_TRACKS); for(int k=0;k<14;k++) bn_mul(v,race_n[k]); break;   // every planet number and track, times every other option
    case SZ_DNA: bn_pow2(v,96); break;                                            // make_world's seeds A, B and C
    case SZ_TROPHIES: bn_set(v,1); for(int k=0;k<NKWT*5;k++) bn_mul(v,4); break;
    case SZ_TOUR: bn_set(v,1); for(int k=2;k<=NSYS;k++) bn_mul(v,(u32)k); break;  // 32 x 31 x ... x 1
    case SZ_FAR: { u32 a[UAX]={0}, ly[LYW]; a[154>>5]=1u<<(154&31); ly_between(ly,a,a); bn_set(v,0); for(int k=0;k<LYW;k++) v[k]=ly[k]; } break;   // half way round both ways, by the game's own sum
    case SZ_WIDE: bn_pow2(v,155); bn_mul(v,ULY); break;                           // 2^155 clusters of 25 light years
    case SZ_WIDEALL: bn_pow2(v,32+155); bn_mul(v,ULY); break;
    case SZ_CLUS: bn_pow2(v,2*155); break;
    case SZ_STARS: bn_pow2(v,2*155+5); break;
    case SZ_WORLDS: bn_pow2(v,2*155+5); bn_mul(v,5627); big_div(v,BNW,1000); about=1; break;   // 5.627 worlds a star on average (see sys_nplan, sys_nmoon)
    case SZ_STARSALL: bn_pow2(v,32+2*155+5); break;
    case SZ_WORLDSALL: bn_pow2(v,32+2*155+5); bn_mul(v,5627); big_div(v,BNW,1000); about=1; break;
    case SZ_ORDER: nb_rom(giant_order,GIANT_ORDER_ND,GIANT_ORDER_NZ); return;         // the giants, from bignums.py
    case SZ_TRIPS: nb_rom(giant_trips,GIANT_TRIPS_ND,GIANT_TRIPS_NZ); return;
    case SZ_FAVS: nb_rom(giant_favs,GIANT_FAVS_ND,GIANT_FAVS_NZ); return;
    case SZ_MAPS: nb_rom(giant_maps,GIANT_MAPS_ND,GIANT_MAPS_NZ); return;
    case SZ_SAVES: nb_rom(giant_saves,GIANT_SAVES_ND,GIANT_SAVES_NZ); return;
    case SZ_SCREENS: nb_rom(giant_screens,GIANT_SCREENS_ND,GIANT_SCREENS_NZ); return;
    default: { int all=i==SZ_LOOKS, cols=0;                                        // the makers: every option times every colour (32,768 each)
        bn_set(v, (i==SZ_KARTS||all) ? 100 : 1);                                        // (a kart also has a number, 0 to 99)
        if(i==SZ_SHIPS||all){ for(int k=0;k<15;k++) bn_mul(v,ship_n[k]); cols+=3; }
        if(i==SZ_KARTS||all){ for(int k=0;k<NKPART;k++) bn_mul(v,kpart_n[k]); cols+=3; }
        if(i==SZ_HUMS||all){ for(int k=0;k<11;k++) bn_mul(v,hum_n[k]); cols+=9; }
        if(i==SZ_BUGS||all){ for(int k=0;k<13;k++) bn_mul(v,bug_n[k]); cols+=10; }
        while(cols--) bn_mul(v,32768); } }
    nb_from(v,about); }

// ---- saying it
static void nb_short(char *o){                                   // "ABOUT 4.3 BILLION", "1 TRILLION", "120,000"
    int n=nb_n; o[0]=0;
    if(n<=6){ if(nb_about) scpy(o,"ABOUT "); for(int k=0;k<n;k++){ char c[2]={(char)('0'+nb_dig(k)),0}; scat(o,c); if(k<n-1 && (n-1-k)%3==0) scat(o,","); } return; }
    int g=(n-1)/3, lead=n-3*g, s = lead==3 ? 3 : lead+1;          // 2 or 3 figures: 4.3, 12.3, 123
    u32 x=0; for(int k=0;k<s;k++) x=x*10+(u32)nb_dig(k);
    int rest=0; if(nb_nz>1) for(int k=s;k<n && !rest;k++) if(nb_dig(k)) rest=1;  // anything left over? (it stops at the first digit that is not 0)
    if(nb_dig(s)>=5) x++;
    u32 p10=1; for(int k=0;k<s;k++) p10*=10;
    if(x>=p10){ n++; g=(n-1)/3; lead=n-3*g; s = lead==3 ? 3 : lead+1; x=1; for(int k=1;k<s;k++) x*=10; }   // 999.7 rounds up to the next name: 1.0
    char xs[8]; for(int k=s-1;k>=0;k--){ xs[k]=(char)('0'+x%10); x/=10; } xs[s]=0;
    int exact = !nb_about && !rest; if(!exact) scpy(o,"ABOUT ");
    char t[8]; int j=0; for(int k=0;k<lead;k++) t[j++]=xs[k]; if(s>lead && xs[lead]!='0'){ t[j++]='.'; t[j++]=xs[lead]; } t[j]=0;   // (no ".0": ABOUT 3 SEXTILLION)
    scat(o,t); scat(o," "); scat(o,illion(g)); }
static int seq(const char *a, const char *b){ while(*a && *a==*b){ a++; b++; } return *a==*b; }   // the same text?
static int illi_cut(const char *s, int most){                    // where to break a long name: after its last ILLI that leaves at most 'most' letters (not the final ILLION)
    int cut=0; for(int k=1;s[k] && s[k+1] && s[k+2] && s[k+3] && k+4<=most;k++)
        if(s[k]=='I' && s[k+1]=='L' && s[k+2]=='L' && s[k+3]=='I' && s[k+4] && !(s[k+4]=='O' && s[k+5]=='N' && !s[k+6])) cut=k+4;
    return cut; }
static int sz_wrap(const char *s, char ln[3][32]){               // break a line of big type (28 letters at most) between words, or after an ILLI with a hyphen
    int n=0; ln[0][0]=0;
    for(const char *p=s; *p; ){
        while(*p==' ') p++; if(!*p) break;
        char w[64]; int L=0; while(*p && *p!=' ' && L<63) w[L++]=*p++; w[L]=0; const char *wp=w;
        while(textlen(wp)>28){ int cut=illi_cut(wp,27); if(!cut) cut=27;
            if(ln[n][0]){ if(n==2) return 3; n++; ln[n][0]=0; }
            for(int k=0;k<cut;k++) ln[n][k]=wp[k]; ln[n][cut]='-'; ln[n][cut+1]=0; wp+=cut;
            if(n==2) return 3; n++; ln[n][0]=0; }
        int cur=textlen(ln[n]);
        if(cur && cur+1+textlen(wp)>28){ if(n==2) return 3; n++; ln[n][0]=0; cur=0; }
        if(cur) scat(ln[n]," "); scat(ln[n],wp); }
    return ln[n][0] ? n+1 : n; }

// ---- drawing: each frame goes on the hidden page and is then shown, so scrolling is smooth and never tears.
// While you scroll only the rows (and the count in the bar) are drawn again: the title and the help line stay put on both pages.
#define SZ_TOP 37                                                // the first row of the scrolling views; rows are 13 high
#define SZ_Y0 36                                                 // the scrolling rows show in lines 36 to 147
#define SZ_Y1 148
static void sz_fill(volatile u16 *pg, int y, int lines){ static u32 zero=0; *(volatile u32*)0x040000D4=(u32)&zero; *(volatile u32*)0x040000D8=(u32)(pg+y*120); *(volatile u32*)0x040000DC=(u32)(lines*60)|(2<<23)|(1<<26)|(1u<<31); }   // a DMA fill: black
IWRAM static void clip_text(volatile u16 *pg, int x, int y, const char *s, int c, int sc, int y0, int y1){   // text cut off outside lines y0..y1-1 (big text needs an even x).
    u16 cc=(u16)(c|(c<<8)); int whole = y>=y0 && y+5*sc<=y1;                                               // ARM code in IWRAM: long numbers scroll at full speed
    for(;*s;s++, x+=4*sc){ u32 g=font[(u8)*s & 127]; if(!g) continue;
        if(sc==2){ volatile u16 *p=pg+y*120+(x>>1);
            for(int r=0;r<5;r++, p+=240){ u32 bits=(g>>(12-r*3))&7; if(!bits) continue; int ly=y+2*r;
                if(whole || (ly>=y0 && ly<y1)){ if(bits&4) p[0]=cc; if(bits&2) p[1]=cc; if(bits&1) p[2]=cc; }
                if(whole || (ly+1>=y0 && ly+1<y1)){ if(bits&4) p[120]=cc; if(bits&2) p[121]=cc; if(bits&1) p[122]=cc; } } }
        else for(int r=0;r<5;r++){ u32 bits=(g>>(12-r*3))&7; if(!bits) continue; int ly=y+r; if(!whole && (ly<y0 || ly>=y1)) continue; volatile u16 *row=pg+ly*120;
                for(int k=0;k<3;k++) if(bits&(4>>k)){ int xx=x+k; volatile u16 *p=row+(xx>>1); if(xx&1) *p=(u16)((*p&0x00FF)|(c<<8)); else *p=(u16)((*p&0xFF00)|c); } } } }
static void sz_row(volatile u16 *pg, int r, int y, int c, int centre, int y0, int y1){   // row r of the digits: seven groups with commas, set right so the commas line up
    int ng=nb_ngv, j0=r*7, j1=j0+7; if(j1>ng) j1=ng; if(j0>=ng) return;
    char b[40]; int L=0, k = j0 ? nb_leadv+3*(j0-1) : 0;                       // (k: the first digit of group j0)
    for(int j=j0;j<j1;j++){ for(int e = j ? 3 : nb_leadv; e>0; e--, k++) b[L++]=(char)('0'+nb_dig(k)); if(j<ng-1) b[L++]=','; } b[L]=0;
    clip_text(pg, centre ? 120-L*4 : 232-(L+(j1==ng))*8, y, b, c, 2, y0, y1); }
static void sz_name_row(volatile u16 *pg, int j, int y, int c){  // group j said by name: 294 MILLION
    char b[8]; scpy(b,""); ncat(b,nb_grp(j)); clip_text(pg,30-textlen(b)*8,y,b,C_GOLD,2,SZ_Y0,SZ_Y1);
    int pw=nb_ng()-1-j; if(!pw) return; const char *nm=illion(pw); int n=textlen(nm);
    if(nb_bign && n<=24) clip_text(pg,36,y,nm,c,2,SZ_Y0,SZ_Y1);      // big when they all fit, small when they are long,
    else if(n<=49) clip_text(pg,36,y+3,nm,c,1,SZ_Y0,SZ_Y1);
    else { int cut=illi_cut(nm,49); if(!cut) cut=49; char a2[52]; for(int k=0;k<cut;k++) a2[k]=nm[k]; a2[cut]=0;    // and on two lines when it is very long
        clip_text(pg,36,y,a2,c,1,SZ_Y0,SZ_Y1); clip_text(pg,36,y+6,nm+cut,c,1,SZ_Y0,SZ_Y1); } }
static void sz_head(volatile u16 *pg, int pi, int view){         // view 3 is the list of them all
    rect(pg,0,0,240,12,1); text(pg,4,3,"HOW BIG IS IT?",C_GOLD,1);
    char b[16]; scpy(b,""); if(view==3){ ncat(b,NSIZE); scat(b," NUMBERS"); } else { ncat(b,pi+1); scat(b," / "); ncat(b,NSIZE); } text(pg,236-textlen(b)*4,3,b,C_GREY,1);
    const char *t = view==3 ? "ALL THE NUMBERS" : szt(pi,0); text(pg,120-textlen(t)*4,14,t,C_GOLD,2);
    static const char *const vn[3]={"IN SHORT","EVERY DIGIT","BY NAME"}; static const u8 vx[3]={52,100,160};
    if(view==3) ctext(pg,120,27,"YOURS FIRST, THEN SMALLEST TO BIGGEST",C_GREY);
    else for(int k=0;k<3;k++){ if(k==view){ rect(pg,vx[k]-3,26,textlen(vn[k])*4+5,8,6); text(pg,vx[k],27,vn[k],C_WHITE,1); } else text(pg,vx[k],27,vn[k],C_DGREY,1); }
    rect(pg,0,35,240,1,6);
    rect(pg,0,148,240,1,6); rect(pg,0,149,240,11,1);
    static const char *const help[4]={"LEFT/RIGHT: MORE  SELECT: LIST  A: EVERY DIGIT  B: BACK","UP/DOWN: SCROLL  L/R: FAST  SEL: ROLL  A: NAMES  B: BACK",
        "UP/DOWN: SCROLL  L/R: FAST  SEL: ROLL  A: SHORT  B: BACK","UP/DOWN: PICK   L/R: FAST   A: SEE IT   B: BACK"};
    ctext(pg,120,152,help[view],C_CYAN); }
static void sz_short_view(volatile u16 *pg, int pi){
    for(int k=0;k<24;k++) pix(pg,(k*53+11)%240,40+(k*29+7)%104,C_DGREY);
    int y=40; if(szt(pi,1)){ ctext(pg,120,y,szt(pi,1),C_GREY); y+=7; } if(szt(pi,2)){ ctext(pg,120,y,szt(pi,2),C_GREY); y+=7; }
    y+=6; { char s[96], ln[3][32]; nb_short(s); int nl=sz_wrap(s,ln); for(int k=0;k<nl;k++){ text(pg,120-textlen(ln[k])*4,y,ln[k],C_WHITE,2); y+=13; } }
    y+=2; { char b[64]; scpy(b,""); kcomma(b,(u32)nb_n); scat(b, nb_n==1 ? " DIGIT" : " DIGITS");
        if(nb_n>=1000){ u32 m=(u32)nb_n/60; scat(b,". READING ONE A SECOND: "); if(m<120){ ncat(b,(int)m); scat(b," MINUTES"); } else { ncat(b,(int)(m/60)); scat(b," HOURS"); } }
        ctext(pg,120,y,b,C_CYAN); y+=11; }
    if(nb_n>6){ int R=(nb_ng()+6)/7, room=((pi==SZ_TRAVEL?128:148)-y)/13;   // the number itself, in full if it fits
        if(R<=room) for(int k=0;k<R;k++) sz_row(pg,k,y+k*13,(k&1)?C_CYAN:C_WHITE,R==1,0,160);
        else { int sh=room-1; if(sh<1) sh=1; for(int k=0;k<sh;k++) sz_row(pg,k,y+k*13,(k&1)?C_CYAN:C_WHITE,0,0,160);
            int seen=nb_lead()+21*sh-3; char b[64]; scpy(b,"...AND "); kcomma(b,(u32)(nb_n-seen)); scat(b," MORE. PRESS A TO SEE THEM ALL"); ctext(pg,120,y+sh*13+2,b,C_GREY); } }
    if(pi==SZ_TRAVEL){ char b[64], d[40];
        scpy(b,""); kcomma(b,tv_trips); scat(b,tv_trips==1?" TRIP. LONGEST: ":" TRIPS. LONGEST: "); ly_fmt(d,tv_best); d[26]=0; scat(b,d); ctext(pg,120,130,b,C_GOLD);
        hours_fmt(d,tv_hours); scpy(b,"TIME BETWEEN THE STARS: "); scat(b,d); ctext(pg,120,138,b,C_GOLD); } }
static int sz_lsel, sz_ltop, sz_nd[NSIZE];                      // the list: which number is picked, the top row, and how many digits each one has
static void sz_list_view(volatile u16 *pg){
    for(int k=0;k<12;k++){ int i=sz_ltop+k, on=i==sz_lsel; if(i>=NSIZE) break; int y=SZ_TOP+2+k*9;
        if(on) rect(pg,4,y-2,229,9,6);
        char b[24]; scpy(b,""); ncat(b,i+1); text(pg,22-textlen(b)*4,y,b,on?C_GOLD:C_DGREY,1);
        text(pg,28,y,szt(i,0),on?C_WHITE:C_GREY,1);
        scpy(b,""); kcomma(b,(u32)sz_nd[i]); scat(b, sz_nd[i]==1 ? " DIGIT" : " DIGITS"); text(pg,232-textlen(b)*4,y,b,on?C_CYAN:C_DGREY,1); }
    { int th=110*12/NSIZE, ty=SZ_TOP+(110-th)*sz_ltop/(NSIZE-12); rect(pg,236,SZ_TOP,3,110,6); rect(pg,236,ty,3,th,C_GREY); } }
static void sz_draw(volatile u16 *pg, int full, int pi, int view, int pos, int ridx, int gcur, int R, int maxpos, char *was){   // full: the whole page, else just what scrolls
    if(full){ sz_fill(pg,0,160); sz_head(pg,pi,view); } else sz_fill(pg,SZ_Y0,SZ_Y1-SZ_Y0);
    if(view==0) sz_short_view(pg,pi);
    else if(view==3) sz_list_view(pg);
    else { int off=pos%13; char mid[40];
        for(int k=0;k<10;k++){ int r=ridx+k, y=SZ_TOP+13*k-off; if(r>=R || y>=SZ_Y1) break; int c=(r&1)?C_CYAN:C_WHITE;
            if(view==1) sz_row(pg,r,y,c,R==1,SZ_Y0,SZ_Y1); else { sz_name_row(pg,gcur,y,c); gcur=nz_next(gcur); } }
        if(maxpos){ int th=110*111/(13*R); if(th<6) th=6; if(th>110) th=110; int ty=SZ_TOP+(int)((s64)(110-th)*pos/maxpos);   // where you are in it
            rect(pg,236,SZ_TOP,3,110,6); rect(pg,236,ty,3,th,C_GREY); }
        if(view==1){ int before = ridx ? nb_lead()+3*(7*ridx-1) : 0; scpy(mid,"DIGIT "); kcomma(mid,(u32)before+1); }   // the count in the bar
        else { scpy(mid,"NAME "); kcomma(mid,(u32)ridx+1); }
        scat(mid," OF "); kcomma(mid,(u32)(view==1?nb_n:nb_nz));
        if(full || !seq(mid,was)){ rect(pg,62,0,144,12,1); ctext(pg,134,3,mid,C_CYAN); scpy(was,mid); } } }   // (was: what this page's bar says now)

static void size_screen(volatile u16 *pg, int from_title){       // LEFT/RIGHT: another number. A: another view. UP/DOWN, L/R, SELECT: scroll. B: back
    if(from_title) tv_load();                                    // (in a game the travel log in memory is the newest)
#ifdef TEST_TRAVELDEMO
    { u32 a[LYW]={32}, b[LYW]={250}, c[LYW]; uni_ly(c,1,3236620590u); tv_log(a,77); tv_log(b,600); tv_log(c,35520); }   // a short hop, the next cluster, a jump to another universe
#endif
    volatile u16 *pa=pg, *pb = pg==(volatile u16*)0x06000000 ? (volatile u16*)0x0600A000 : (volatile u16*)0x06000000;
    int bita = pg==(volatile u16*)0x06000000 ? 0 : 0x10, front=0;  // front 1: the other page is the one on screen
    int pi=0, view=0, set=-1, pos=0, ridx=0, gcur=0, hold=0, lrh=0, roll=0, dirty=1, R=0, maxpos=0, lastv=-1, ok[2]={0,0}, vlast=vbl_count; char was[2][40];   // ok[i]: page i has this number's title and help line
#ifdef TEST_SIZE
    pi=TEST_SIZE;
#endif
#ifdef TEST_SZVIEW
    view=TEST_SZVIEW;
#endif
    wait_release();
#ifdef TEST_SZCHECK
    sz_fill(pa,0,160); for(int i=0;i<NSIZE;i++){ size_set(i); u32 h=2166136261u; for(int k=0;k<nb_n;k++){ h^=(u32)nb_dig(k); h*=16777619u; }   // every digit of every page, as a fingerprint
        char b[40], x[12]; scpy(b,""); ncat(b,i+1); scat(b," "); ncat(b,nb_n); scat(b," "); for(int k=0;k<8;k++){ int q=(int)((h>>(28-4*k))&15); x[k]=(char)(q<10?'0'+q:'A'+q-10); } x[8]=0; scat(b,x);
        text(pa,4+(i/19)*120,4+(i%19)*8,b,C_WHITE,1); }
    REG_DISPCNT&=~0x10; while(1) vsync();
#endif
    while(1){
        if(set!=pi){ size_set(pi); set=pi; pos=ridx=gcur=0; roll=0; dirty=1; ok[0]=ok[1]=0; }
        if(view!=lastv || view==3){ lastv=view; ok[0]=ok[1]=0; }
        R = view==1 ? (nb_ng()+6)/7 : view==2 ? nb_nz : 0; maxpos = R*13>109 ? R*13-109 : 0;
        { int nr=pos/13; if(view==2){ while(ridx<nr){ gcur=nz_next(gcur); ridx++; } while(ridx>nr){ gcur=nz_prev(gcur); ridx--; } } else ridx=nr; }
        if(dirty){ dirty=0; int bi = front ? 0 : 1; volatile u16 *bk = bi ? pb : pa;
            sz_draw(bk,!ok[bi],pi,view,pos,ridx,gcur,R,maxpos,was[bi]); ok[bi]=1;
            vsync(); REG_DISPCNT=(REG_DISPCNT&~0x10)|(front ? bita : bita^0x10); front^=1; }
        else vsync();
        read_keys();
        if(HIT(K_START)) break;
        if(view==3){ int d=0;                                                          // the list: UP/DOWN picks (hold to keep going), L/R a screen, A shows it
            if(HELD(K_UP)||HELD(K_DOWN)){ if(HIT(K_UP)||HIT(K_DOWN)){ lrh=0; d=HELD(K_DOWN)?1:-1; } else if(++lrh>12 && !(lrh%3)) d=HELD(K_DOWN)?1:-1; } else lrh=0;
            if(HIT(K_L)) d=-12; if(HIT(K_R)) d=12;
            if(d){ int s=sz_lsel+d; if(s<0) s = d<-1 ? 0 : NSIZE-1; if(s>=NSIZE) s = d>1 ? NSIZE-1 : 0; sz_lsel=s;
                if(s<sz_ltop) sz_ltop=s; if(s>=sz_ltop+12) sz_ltop=s-11; dirty=1; sfx(7); }
            if(HIT(K_A)){ pi=sz_lsel; view=0; set=-1; dirty=1; sfx(2); }
            else if(HIT(K_B)||HIT(K_SEL)){ view=0; set=-1; dirty=1; sfx(4); }
            continue; }
        if(HIT(K_SEL) && !view){ for(int k=0;k<NSIZE;k++){ size_set(k); sz_nd[k]=nb_n; } set=-1;        // the list of them all (every number worked out again, for its length)
            sz_lsel=pi; sz_ltop = pi>5 ? pi-5 : 0; if(sz_ltop>NSIZE-12) sz_ltop=NSIZE-12; view=3; dirty=1; sfx(2); continue; }
        if(HIT(K_B)){ if(!view) break; view=0; pos=ridx=gcur=0; roll=0; dirty=1; sfx(4); continue; }
        if(HIT(K_A)){ view=(view+1)%3; pos=ridx=gcur=0; roll=0; dirty=1; sfx(2); continue; }
        if(HIT(K_RIGHT)){ pi=(pi+1)%NSIZE; sfx(7); continue; }
        if(HIT(K_LEFT)){ pi=(pi+NSIZE-1)%NSIZE; sfx(7); continue; }
        if(!view) continue;
        int d=0, dt=vbl_count-vlast; if(dt<1) dt=1; if(dt>8) dt=8; vlast=vbl_count;   // (speeds are per 60th of a second, however long a frame takes to draw)
        if(HELD(K_UP)||HELD(K_DOWN)){ int dir=HELD(K_DOWN)?1:-1; roll=0;            // a tap: one row. Held: faster and faster
            if(HIT(K_UP)||HIT(K_DOWN)){ hold=0; d=13*dir; } else if((hold+=dt)>12){ int sh=(hold-12)/20; d=(sh>10 ? 2048 : 2<<sh)*dt*dir; } } else hold=0;
        if(HELD(K_L)||HELD(K_R)){ int dir=HELD(K_R)?1:-1; roll=0;                    // a whole screen at a time
            if(HIT(K_L)||HIT(K_R)){ lrh=0; d=104*dir; } else { int was=lrh; lrh+=dt; if(lrh>15 && (lrh>>2)!=(was>>2)) d=104*dir; } } else lrh=0;
        if(HIT(K_SEL)){ roll=!roll; sfx(2); }                                          // let it roll by on its own
        if(roll && !d) d=dt;
        if(d){ int np=pos+d; if(np<0) np=0; if(np>maxpos) np=maxpos; if(np!=pos){ pos=np; dirty=1; } else roll=0; } }
    if(front){ sz_fill(pa,0,160); vsync(); REG_DISPCNT=(REG_DISPCNT&~0x10)|bita; }   // leave on the page we were given
    wait_release(); }
