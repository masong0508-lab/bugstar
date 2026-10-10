// ================================================================= LIFE ON THE PLANET: cities, locals, creatures, jobs, races, ruins
// Cities are built straight into the heightmap, so the renderer draws them as real blocks with lit windows. Everything that
// lives on a planet is grown from the planet's DNA (see make_world): the species, their tempers, their numbers, the town's trade.
static int city_cx, city_cy, city_r;                        // the city, in cells (r = 0: no city)
EW static s16 cvx[NPO], cvy[NPO]; EW static u8 ctim[NPO];  // creatures: where each is heading, and for how long
typedef struct { u8 body, temper, size, pop, maxpop; u16 col; char name[20]; } Species;   // temper 0 timid, 1 curious, 2 hostile
static Species spc[2];
static int pq_type=0, pq_need, pq_have, pq_done, pq_gold;   // the job from the bounty office (0 none, 1 slugs, 2 beetles, 3 fossils, 4 race, 5 census, 6 hunt, 7 ore)
static int race_on=0, race_t=0, race_next=0, race_par=0, race_best=0; static s32 race_x[5], race_y[5];
static int weather_t=0;
static const char *const adj[16]={"MOSS","ROSY","GLASS","SOOT","HONEY","STRIPED","GRUMPY","SLEEPY","POLKA","AZURE","RUST","PEARL","GIANT","PYGMY","VELVET","SPARK"};
static const char *const noun[3][4]={{"HOPPER","SKIPPER","FLEA","BOUNCER"},{"GRAZER","SNUFFLER","LUMP","MOOCOW"},{"FLITTER","MOTH","WINGLET","BUZZER"}};
static void make_species(void){
    u32 keep=wseed; wseed=wd.cseed;                                                  // the species come from their own seed
    for(int s=0;s<2;s++){ Species *p=&spc[s];
        p->body = s==1 && wr(2) ? 2 : wr(2); p->temper=wr(3); if(wd.fauna==5) p->temper=2; p->size=wr(3);
        p->maxpop = endless ? 4+wr(10) : 5+zone; p->pop=p->maxpop;
        p->col=rgb15(40+wr(216),40+wr(216),40+wr(216));
        scpy(p->name,adj[wr(16)]); scat(p->name," "); scat(p->name,noun[p->body][wr(4)]); }
    if(!endless && zone==0){ spc[0].temper=0; spc[1].temper=1; }                      // the first planet's creatures are gentle
    if(wd.mood==MOOD_LOVE){ for(int s=0;s<2;s++) if(spc[s].temper==2) spc[s].temper=1; }                    // a loving world: nothing bites
    if(wd.mood==MOOD_HATE){ spc[0].temper=2; }                                                               // a hateful one: at least one kind does
    if(deadly()){ spc[0].temper=2; spc[1].temper=2; }
    if(wd.cls==PC_ECUMENOPOLIS){ spc[0].maxpop=spc[0].pop=0; spc[1].maxpop=spc[1].pop=0; }                    // no wild things in a city planet
    if(wd.cls==PC_OCEAN){ spc[1].body=2; }
    if(WL(MW_SHY)){ spc[0].temper=0; spc[1].temper=0; } if(WL(MW_ANGRY)){ spc[0].temper=2; spc[1].temper=2; }
    wseed=keep;
}
static void form_palette(void){ volatile u16 *P=OBJ_PAL+6*16; for(int i=0;i<16;i++) P[i]=obj_pal[i]; u16 c=form_col?form_col:look.col[LC_SHELL]; P[6]=shade15(c,7); P[7]=c; P[8]=lift15(c,8); P[5]=look.col[LC_EYE]; }
static void species_palette(void){                                                    // sprite banks 4 and 5: the two species' colours
    for(int s=0;s<2;s++){ volatile u16 *P=OBJ_PAL+(4+s)*16; for(int i=0;i<16;i++) P[i]=obj_pal[i];
        u16 c=spc[s].col; P[6]=shade15(c,7); P[7]=c; P[8]=lift15(c,8); P[5]=rgb15(250,250,250); }
    form_palette();
}
// ---------------------------------------------------------------- building the city into the land
static u32 hsh(u32 a, u32 b){ u32 h=a*0x9E3779B1u ^ (b+0x7F4A7C15u)*0x85EBCA6Bu; h^=h>>15; h*=0xC2B2AE35u; h^=h>>13; return h; }
static int in_city_cell(int x, int y){ if(!city_r) return 0; int dx=((x-city_cx+64)&TMASK)-64, dy=((y-city_cy+64)&TMASK)-64; return dx*dx+dy*dy < city_r*city_r; }
static void city_flatten(void){                                                       // before smoothing: a level ground for the town
    if(!wd.city){ city_r=0; return; }
    city_r = wd.cls==PC_ECUMENOPOLIS ? 62 : 9 + wd.csize*6;                          // a city planet: the city is the whole world
    int a=wr(256), d = wd.cls==PC_ECUMENOPOLIS ? 0 : city_r+14+wr(6); city_cx=(64+((cosx(a)*d)>>14))&TMASK; city_cy=(64+((sinx(a)*d)>>14))&TMASK;
    int sum=0, n=0; for(int y=-city_r;y<=city_r;y+=3) for(int x=-city_r;x<=city_r;x+=3){ sum+=hmap[((city_cy+y)&TMASK)*TW+((city_cx+x)&TMASK)]; n++; }
    int L=sum/n; if(L<water_lv+14) L=water_lv+14; if(L>150) L=150;
    for(int y=-city_r-4;y<=city_r+4;y++) for(int x=-city_r-4;x<=city_r+4;x++){ int d2=x*x+y*y, r2=city_r*city_r, r3=(city_r+4)*(city_r+4);
        int t = d2<r2 ? 256 : d2>r3 ? 0 : 256-(d2-r2)*256/(r3-r2+1); int i=((city_cy+y)&TMASK)*TW+((city_cx+x)&TMASK); hmap[i]=(hmap[i]*(256-t)+L*t)>>8; }
}
static void city_stamp(void){                                                         // after smoothing: streets, blocks, towers, a plaza
    if(!city_r) return;
    int L = hmap[city_cy*TW+city_cx]; int tx0=city_cx*2, ty0=city_cy*2, R=city_r*2;
    for(int y=-R;y<=R;y++) for(int x=-R;x<=R;x++){ if(x*x+y*y>=R*R) continue;
        int tx=(tx0+x)&HMASK, ty=(ty0+y)&HMASK, i=ty*HW+tx;
        int gx=x+R*4, gy=y+R*4;                                                       // positive grid coordinates
        int sx=gx&7, sy=gy&7;                                                         // where in the 8-texel block (4 cells)
        hfine[i]=L;
        if(x*x+y*y < 36){ cmap[i]=80; continue; }                                     // the plaza in the middle
        { int lx=((tx-128+128)&HMASK)-128, ly=((ty-128+128)&HMASK)-128; if(lx*lx+ly*ly < 14*14){ cmap[i]=80; continue; } }   // the landing field stays clear
        if(sx<2 || sy<2){ cmap[i] = ((sx==0&&(gy&3)==1)||(sy==0&&(gx&3)==1)) ? 75 : 74; continue; }   // streets with a dotted line
        int bx=gx>>3, by=gy>>3; u32 h=hsh(bx+wd.tseed, by); int kind=h&15;
        if(kind<3){ cmap[i]=80; continue; }                                            // an empty lot: a little square
        int fx=2+((h>>4)&3), fy=2+((h>>6)&3);                                          // footprint, from the block's corner
        if(sx-2>=fx || sy-2>=fy){ cmap[i]=80+((h>>8)&1); continue; }                  // the yard around it
        int cen=R*R-(x*x+y*y); int tall = 60 + ((h>>10)&127)*cen/(R*R+1);             // taller near the middle
        if(kind>=14) tall=100+((h>>10)&127);                                           // a tower
        int hh=L+tall; if(hh>255) hh=255; hfine[i]=hh;
        cmap[i] = BLD_B + ((h>>17)&3)*2 + ((tx^ty)&1); }
}
static void city_people(int *k){                                                      // doors, locals and stalls on the streets
    if(!city_r) return;
    int R=city_r*2, tx0=city_cx*2, ty0=city_cy*2, placed=0;
    static const u8 want[14]={O_MARKET,O_CANTINA,O_OFFICE,O_RACE,O_HOME,O_HOME,O_MARKET,O_CANTINA,O_HOME,O_OFFICE,O_HOME,O_HOME,O_CANTINA,O_HOME};
    int nwant = wd.cls==PC_ECUMENOPOLIS ? 14 : 4 + wd.csize;
    for(int t=0;t<900 && placed<nwant && *k<NPO-20;t++){                               // a door needs a building behind it and a street in front
        int x=wr(2*R+1)-R, y=wr(2*R+1)-R; if(x*x+y*y>=(R-4)*(R-4)) continue;
        int tx=(tx0+x)&HMASK, ty=(ty0+y)&HMASK; if(cmap[ty*HW+tx]!=74 && cmap[ty*HW+tx]!=75) continue;
        int ok=0; for(int d=1;d<=4 && !ok;d++){ int c=cmap[((ty-d)&HMASK)*HW+tx]; if(c>=BLD_B && c<BLD_B+8) ok=1; } if(!ok) continue;   // a building just behind
        PObj *o=&po[(*k)++]; o->kind=want[placed<14?placed:13]; o->alive=1; o->hp=1; o->var=wr(2); o->x=(tx<<15)+(1<<14); o->y=(ty<<15)+(1<<14)+(1<<13); placed++; }
    int nloc = wd.cls==PC_ECUMENOPOLIS ? 26 : 3+wd.csize*3;
    for(int t=0;t<900 && nloc && *k<NPO-20;t++){ int x=wr(2*R+1)-R, y=wr(2*R+1)-R; if(x*x+y*y>=R*R) continue;
        int tx=(tx0+x)&HMASK, ty=(ty0+y)&HMASK; int c=cmap[ty*HW+tx]; if(c!=74 && c!=75 && c!=80) continue;
        PObj *o=&po[(*k)++]; o->kind=O_LOCAL; o->alive=1; o->hp=wr(3)==0; o->var=wr(2); o->x=(tx<<15)+(1<<14); o->y=(ty<<15)+(1<<14); nloc--; }
    if(wd.csize>=2){ PObj *o=&po[(*k)++]; o->kind=O_STALL; o->alive=1; o->hp=1; o->x=(tx0<<15)+(3<<16); o->y=(ty0<<15)+(1<<16); }
}
// ---------------------------------------------------------------- creatures, fossils, beetles, ruins
static void spawn_crit(int s, s32 nx, s32 ny, int *k){ int i=*k; if(i>=NPO) return; po[i].kind=O_CRIT; po[i].alive=1; po[i].hp=2+spc[s].size; po[i].var=s; po[i].x=nx&((TW<<16)-1); po[i].y=ny&((TW<<16)-1); cvx[i]=cvy[i]=0; ctim[i]=wr(60); (*k)++; }
// ---------------------------------------------------------------- legendary relics: sixteen of them in the whole universe
static const char *const relic_name[16]={"THE COSMIC SPOON","A STAR IN A JAR","THE LAST SOCK","THE ANT KING'S TOENAIL","THE HUMMING EGG","A MAP OF EVERYWHERE","THE GOLDEN CRUMB","BOLT'S FIRST BOLT",
    "THE INFINITE PEBBLE","A SNEEZE IN A BOTTLE","THE TINY CROWN","THE MOON'S LOST BUTTON","THE SINGING SHELL","A SLICE OF NOTHING","THE WEIRD KEY","THE BUG STAR"};
static const char *const relic_joke[16]={"IT STIRS THE SOUP OF SPACE.","IT IS WARM. IT IS ALSO A STAR.","ITS PARTNER WAS NEVER FOUND.","STILL A LITTLE SMELLY.","IT HUMS IN B FLAT. ALWAYS.","IT IS MOSTLY BLANK. SPACE IS BIG.",
    "IT HAS NEVER BEEN EATEN. THE ONLY ONE.","HE WANTS IT BACK. HE WILL NOT SAY SO.","PUT IT DOWN. IT IS STILL THERE.","DO NOT OPEN. SERIOUSLY.","IT FITS NOBODY. IT IS PERFECT.","THE MOON HAS BEEN COLD SINCE.",
    "IT SINGS YOUR NAME. BADLY.","IT WEIGHS LESS THAN ZERO.","IT OPENS NOTHING. OR EVERYTHING.","THE GAME IS NAMED AFTER IT. PROBABLY."};
static void relic_found(void){
    volatile u16 *pg=VISPG; const char *ln[5]; char a[48], b[48], c[48]; int left=-1;
    for(int tries=0;tries<16;tries++){ int i=rndi(16); if(!((relic_bits>>i)&1)){ left=i; break; } }
    if(left<0) for(int i=0;i<16;i++) if(!((relic_bits>>i)&1)){ left=i; break; }
    int g = 1000 + dlv()*200; gold+=g; fame+=100;
    if(left<0){ g+=1000; gold+=1000; relic_gold+=g; scpy(a,"ANOTHER COPY OF A RELIC YOU OWN."); ln[0]=a; scpy(b,"A COLLECTOR PAYS "); ncat(b,g); scat(b,"G FOR IT."); ln[1]=b; card(pg,"A LEGENDARY RELIC",ln,2); sfx(6); return; }
    relic_bits|=1u<<left; recalc(); st_uniq++; ach_check();
    int n=0; for(u32 r=relic_bits;r;r>>=1) n+=r&1;
    ln[0]=relic_name[left]; ln[1]=relic_joke[left]; scpy(a,"+"); ncat(a,g); scat(a,"G  +100 FAME"); ln[2]=a; ln[3]="FOREVER: +5% GOLD, +2 MAX HEALTH";
    scpy(c,"RELICS: "); ncat(c,n); scat(c,"/16  (THERE ARE ONLY 16 IN THE UNIVERSE)"); ln[4]=c;
    sfx(6); card(pg,"!!! A LEGENDARY RELIC !!!",ln,5); save_game();
}
static int relic_roll(int one_in){                                                     // a very rare find. Weird worlds are far luckier
    if(wd.weird) one_in/=12; if(holding(TL_SCANNER)) one_in/=2; if(WL(MW_METAL)) one_in/=2; if(WL(MW_LUCK)) one_in/=1+WL(MW_LUCK)/4; if(one_in<1) one_in=1;
    if(rndi(one_in)) return 0; relic_found(); return 1;
}
static void planet_life(int *k){
    make_species();
    if(WL(MW_GIANTS)) for(int s=0;s<2;s++){ spc[s].size=2; spc[s].maxpop=spc[s].pop=10+WL(MW_GIANTS)/2; }    // GIANTS: huge, and lots of them
    if(WL(MW_SWARM)) for(int i=0;i<2+WL(MW_SWARM)/2 && *k<NPO-16;i++) place(&po[(*k)++],O_SLUG,6,40);          // SLUG SWARM
    if(WL(MW_JUNGLE)) for(int i=0;i<2+WL(MW_JUNGLE)/2 && *k<NPO-16;i++) place(&po[(*k)++],(i&1)?O_PLANT:O_TREE,4,60);   // JUNGLE
    if(wd.weird && *k<NPO-10){ place(&po[*k],O_SHRINE,10,30); po[*k].hp=1; (*k)++; }                // every weird world hides one relic shrine
    if(WL(MW_METAL)) for(int i=0;i<4+WL(MW_METAL)/3 && *k<NPO-12;i++) place(&po[(*k)++],O_ROCK,6,60);   // METAL GROUND: ore everywhere
    for(int s=0;s<2;s++){ int a=wr(256), d=12+wr(40); s32 hx=(64<<16)+cosx(a)*d*4, hy=(64<<16)+sinx(a)*d*4;    // a herd around a home spot
        for(int i=0;i<spc[s].pop && *k<NPO-10;i++) spawn_crit(s, hx+(wr(9)-4)*(1<<16), hy+(wr(9)-4)*(1<<16), k); }
    for(int i=0;i<wd.fossils && *k<NPO-8;i++) place(&po[(*k)++],O_FOSSIL,8,60);
    for(int i=0;i<wd.beetles && *k<NPO-8;i++){ place(&po[*k],O_BEETLE,16,50); po[*k].hp=4; (*k)++; }
    if(wd.ruins && *k<NPO-8){ place(&po[*k],O_RUIN,18,56); po[*k].hp=1; (*k)++; }
}
static int crit_count(int s){ int n=0; for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==O_CRIT && po[i].var==s) n++; return n; }
static void life_tick(int n){                                                         // every frame: creatures move, beetles hunt, herds breed
    for(int i=0;i<NPO;i++){ PObj *o=&po[i]; if(!o->alive) continue;
        if(o->kind==O_CRIT){ Species *p=&spc[o->var]; s32 dx=wrapd(ppx-o->x), dy=wrapd(ppy-o->y); s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy);
            if(ctim[i]>n) ctim[i]-=n; else { ctim[i]=40+wr(80); int a=wr(256); int sp = p->body==2 ? 900 : 500;
                cvx[i]=(cosx(a)*sp)>>14; cvy[i]=(sinx(a)*sp)>>14; if(wr(3)==0) cvx[i]=cvy[i]=0; }
            s32 mx=cvx[i]*n, my=cvy[i]*n;
            if(p->temper==0 && ad<(7<<16)){ mx=-(dx>>6)*n; my=-(dy>>6)*n; }          // timid: run from you
            else if(p->temper==2 && WL(MW_ANGRY) && ad<(10<<16)){ mx=(dx>>6)*n*(8+WL(MW_ANGRY))/8; my=(dy>>6)*n*(8+WL(MW_ANGRY))/8; if(ad<0x16000 && !invuln){ hurt(4+dlv()*2+WL(MW_ANGRY)/5); show_msg("OUCH! IT IS FURIOUS"); pknock(dx,dy,40); } }
            else if(p->temper==1 && ad<(9<<16) && ad>(2<<16)){ mx=(dx>>7)*n; my=(dy>>7)*n; }   // curious: come and look
            else if(p->temper==2 && ad<(10<<16)){ mx=(dx>>6)*n; my=(dy>>6)*n; if(ad<0x16000 && !invuln){ hurt((4+dlv()*2)*(deadly()?2:1)); show_msg(deadly()?"IT BIT YOU. HARD. THIS PLANET WANTS YOU DEAD":"OUCH! IT BIT YOU"); pknock(dx,dy,40); } }
            s32 nx=o->x+mx, ny=o->y+my; if(p->body!=2 && (hm_at(nx,ny)<water_lv || in_city_cell(nx>>16,ny>>16))){ ctim[i]=0; continue; }
            o->x=nx&((TW<<16)-1); o->y=ny&((TW<<16)-1); }
        else if(o->kind==O_BEETLE){ s32 dx=wrapd(ppx-o->x), dy=wrapd(ppy-o->y); s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy);
            if(ad<(12<<16) && ad>0x8000){ int sg=WL(MW_SHY)?-1:1, ml=8+WL(MW_ANGRY); o->x=(o->x+sg*((dx>>6)*n)*ml/8)&((TW<<16)-1); o->y=(o->y+sg*((dy>>6)*n)*ml/8)&((TW<<16)-1); }
            if(ad<0x14000 && !invuln){ hurt((5+dlv()*2)*(deadly()?2:1)); show_msg("A BEETLE RAMMED YOU"); pknock(dx,dy,48); } }
        else if(o->kind==O_LOCAL){ if(ctim[i]>n) ctim[i]-=n; else { ctim[i]=60+wr(120); int a=wr(256); cvx[i]=(cosx(a)*120)>>14; cvy[i]=(sinx(a)*120)>>14; if(wr(2)) cvx[i]=cvy[i]=0; }
            s32 nx=o->x+cvx[i]*n, ny=o->y+cvy[i]*n; int c=cmap[((ny>>15)&HMASK)*HW+((nx>>15)&HMASK)]; if(c==74||c==75||c==80||c==81){ o->x=nx; o->y=ny; } else ctim[i]=0; }
    }
    if(WL(MW_MAGNET)||WL(MW_SHYORE)) for(int i=0;i<NPO;i++){ PObj *o=&po[i]; if(!o->alive || o->kind<O_TREE || o->kind>O_FLOWER) continue;   // MAGNET / SHY ORE
        s32 dx=wrapd(ppx-o->x), dy=wrapd(ppy-o->y); s32 ad=(dx<0?-dx:dx)+(dy<0?-dy:dy); if(ad>(10<<16)) continue;
        int lv = WL(MW_MAGNET) ? WL(MW_MAGNET) : -WL(MW_SHYORE); if(lv>0 && ad<0x18000) continue;
        o->x=(o->x+((dx>>9)*n*lv)/4)&((TW<<16)-1); o->y=(o->y+((dy>>9)*n*lv)/4)&((TW<<16)-1); }
    if(frames%900==0){ for(int s=0;s<2;s++){ int c=crit_count(s); if(c>0 && c<spc[s].maxpop){ for(int i=0;i<NPO;i++) if(po[i].alive && po[i].kind==O_CRIT && po[i].var==s){
        for(int j=0;j<NPO;j++) if(!po[j].alive){ int k=j; spawn_crit(s, po[i].x+(1<<16), po[i].y, &k); break; } break; } } } }
}
// ---------------------------------------------------------------- talking: locals, gossip, Bolt's quips
#define NTALK 40
static const char *const talk_ln[NTALK]={"I HAVE SEEN THINGS. MOSTLY MUD.","DO NOT LICK THE ROCKS. I DID.","MY COUSIN IS A SLUG. WE DO NOT TALK.","THE MAYOR IS A MOTH. SHE IS FINE.","YOU ARE NOT FROM HERE. YOUR LEGS ARE WRONG.",
    "I SELL NOTHING. IT IS GOING WELL.","THE WEATHER? IT HAPPENS.","I COUNTED THE STARS. I LOST COUNT AT NINE.","NICE SHIP. DOES IT COME IN BEIGE?","BEETLES ARE JUST ROCKS WITH OPINIONS.",
    "WE HAD A FESTIVAL ONCE. NOBODY CAME.","THE SOUP IS GOOD. DO NOT ASK WHAT IS IN IT.","I AM WAITING FOR A BUS. THERE IS NO BUS.","I ONCE SAW A HILL MOVE. IT WAS A GRAZER.","YOUR DROID LOOKS SAD. GIVE HIM A HAT.",
    "DIG WHERE THE BONES ARE. THEY ARE OLD BONES.","THE RACE IS RIGGED. BY GRAVITY.","I AM A LOCAL. THAT IS MY WHOLE JOB.","THE RUINS HUM AT NIGHT. OR THAT IS ME.","I LOST MY HAT IN THE CRATER. GOOD HAT.",
    "I HEAR THE ANT KING SNORES.","FUN FACT: I AM NOT FUN.","THE CANTINA HAS CHAIRS. REAL ONES.","MY GRANDMA FLEW A SHIP. INTO A TREE.","NO TAXES HERE. ALSO NO ROADS. OH WAIT.",
    "THE BOUNTY OFFICE PAYS LATE. BUT IT PAYS.","I DREAMED I WAS A PEBBLE. IT WAS RESTFUL.","SELL YOUR SILK HERE. WE LOVE SILK. WE ARE SPIDERS. KIDDING.","EVERYONE HERE IS FAMOUS FOR SOMETHING.","THE PLAZA IS FOR STANDING. YOU ARE DOING IT RIGHT.",
    "I HAVE NEVER LEFT. THE VIEW IS THE SAME.","THE FLYERS STEAL BUTTONS. GUARD YOUR BUTTONS.","THE TOWER IS TALL SO WE CAN SEE THE OTHER TOWER.","THERE IS A SECRET. I FORGOT IT.","I AM TWELVE. IN BUG YEARS. SO, OLD.",
    "WELCOME! PLEASE DO NOT EAT THE MAYOR.","OUR MOTTO: WE ARE HERE.","THE TECHNOBEAST? NEVER HEARD OF IT. GULP.","BOLT? I KNEW A BOLT. HE WAS SHINY.","GO AWAY. KIDDING. STAY. KIDDING. GO."};
static const char *const bolt_quip[12]={"I DID NOT PACK A TOWEL.","MY SENSORS SAY: HILLS. GOOD WORK, SENSORS.","A CITY. TRY NOT TO GET US ARRESTED.","IT IS NIGHT. MY NIGHT MODE IS JUST ME, DARKER.",
    "IT RAINS HERE. I AM TIN. THIS IS A PROBLEM.","SNOW. I HAVE DECIDED I LIKE SNOW.","DUST. IN MY GEARS. LOVELY.","THE LOCALS LOOK NICE. THE LOCALS LOOK AT US.",
    "I SMELL SOUP. I CANNOT SMELL.","GRAVITY FEELS WRONG. OR I AM TIRED.","THE CREATURES HERE HAVE OPINIONS ABOUT US.","HOME WAS LIKE THIS. BEFORE THE FALL. I THINK."};
static void scan_text(char *b){                                                         // SCAN: gravity, temperature, day length (silly on weird worlds)
    int gr=35+wd.grav*35; if(WL(MW_ZEROG)) gr=0; if(WL(MW_GIANTS)) gr*=2; if(WL(MW_SPIKES)) gr+=100+WL(MW_SPIKES)*10;
    int tc = wd.sand ? 38 : wd.snow ? -12 : 18; if(wd.hour==2) tc-=14; else if(wd.hour==1) tc-=5; if(wd.weather==2) tc-=20;
    tc += WL(MW_HEAT)*40 - WL(MW_FROST)*12 + (int)(wd.tseed%11) - 5; if(WL(MW_VOID)) tc=-270;
    int day = 6+(int)((wd.tseed>>8)%70);
    scpy(b,"SCAN: ");
    if(WL(MW_GLITCH)) scat(b,"GRAVITY NAN  TEMP ERR  DAY ???");
    else { scat(b,"GRAVITY "); ncat(b,gr/100); scat(b,"."); if(gr%100<10) scat(b,"0"); ncat(b,gr%100); scat(b,"G  TEMP "); ncat(b,tc); scat(b,"C  DAY ");
        if(WL(MW_TINY)){ ncat(b,60-WL(MW_TINY)*2); scat(b," MIN"); } else { ncat(b,day); scat(b,"H"); } }
}
static void give_marks(char *out){                                                      // a weird world may change you: for a while, or (rarely) forever
    out[0]=0; int given=0;
    for(int k=0;k<wd.ntr && given<2;k++){ if(rndi(2)) continue; int lv=wd.tr[k]/NMW+1;
        int good = rndi(100)<70; int mk = good ? rndi(MK_GOOD) : MK_GOOD+rndi(NMK-MK_GOOD);
        int perm = rndi(100) < 6+lv && (good || rndi(3)==0);
        if(perm){ if((mark_perm>>mk)&1) continue; mark_perm|=1u<<mk; } else mark_t[mk]=60+lv*15;
        if(out[0]) scat(out,". "); scat(out, perm?"FOREVER ":""); scat(out,mk_name[mk]); if(!perm){ scat(out," "); ncat(out,(60+lv*15)/60); scat(out,"M"); } given++; }
    if(rndi(100) < 30){                                                                 // ...or reshape you
        int lv=wd.tr[0]/NMW+1; int nf = rndi(100)<60 ? FORM_CREATURE : FORM_HUMAN;
        form=nf; forms_open|=1<<nf; form_t = rndi(100) < 6+lv/2 ? 0 : 180+lv*12; form_body=spc[0].body; form_col=spc[0].col;
        if(out[0]) scat(out,". "); scat(out,"YOU ARE NOW A "); scat(out, nf==FORM_HUMAN ? "HUMANOID" : body_name[form_body]); if(!form_t) scat(out," FOREVER");
        if(nf==FORM_HUMAN && !tools_owned){ tools_owned|=1u<<TL_PICK; tool_held=TL_PICK; }      // you wake up holding a pickaxe
        form_palette(); given=1; }
    if(given){ recalc(); save_game(); }
}
static void planet_card(volatile u16 *pg){                                              // on arrival: name, what it is known for, its DNA, a scan, Bolt's two cents
    char l1[48], l2[48], l3[48], sc[48]; const char *ln[8];
    scpy(l1,"KNOWN FOR: "); scat(l1,tagline[wd.tagline]);
    scpy(l2,style_name[wd.style]); scat(l2,". "); scat(l2,hour_name[wd.hour]); scat(l2,". "); scat(l2,weather_name[wd.weather]); scat(l2,".");
    if(wd.city) scat(l2, wd.csize>=2 ? " A CITY." : " A TOWN.");
    scpy(l3,"LIFE: "); ncat(l3,spc[0].pop); scat(l3," "); scat(l3,spc[0].name); scat(l3,"S, "); ncat(l3,spc[1].pop); scat(l3," "); scat(l3,spc[1].name); scat(l3,"S");
    scan_text(sc);
    int q = wd.weather==1 ? 4 : wd.weather==2 ? 5 : wd.weather==3 ? 6 : wd.hour==2 ? 3 : wd.city ? 2 : wd.grav!=1 ? 9 : (wd.tseed%3)==0 ? 0 : (wd.tseed%3)==1 ? 1 : 10;
    if(!endless && zone==0) q=11;
    if(wd.weird){
        static char ta[56], tb[56], mk[56], q2[48], tl[32]; char nb[28]; ta[0]=tb[0]=0;
        for(int k=0;k<wd.ntr;k++){ trait_name(wd.tr[k],nb); char *d = k<2 ? ta : tb; if(d[0]) scat(d," + "); scat(d,nb); }
        weird_seen++; give_marks(mk);
        scpy(tl,"??? "); scat(tl,world_name()); scat(tl," ???");
        scpy(q2,"BOLT: "); scat(q2,mw_quip[wd.tr[0]%NMW]);
        static char cm2[56]; scpy(cm2,pc_name[wd.cls]); scat(cm2,". "); scat(cm2,mood_name[wd.mood]);
        int n=0; ln[n++]=ta; if(tb[0]) ln[n++]=tb; ln[n++]=cm2; ln[n++]=sc;
        static char mk2[60]; if(mk[0]){ scpy(mk2,"IT CHANGED YOU: "); scat(mk2,mk); mk2[52]=0; ln[n++]=mk2; }
        ln[n++]="A RELIC SHRINE IS HIDDEN HERE. FIND IT!"; ln[n++]=q2;
        sfx(6); card(pg,tl,ln,n); return; }
    static char cm[56]; scpy(cm,pc_name[wd.cls]); scat(cm,". "); scat(cm,mood_name[wd.mood]);
    static const char *const pq[NPC]={0,"BOLT: MOUNTAINS. I DO NOT HAVE KNEES.","BOLT: SO FLAT. SO NICE. I COULD ROLL FOREVER.","BOLT: WATER EVERYWHERE. I RUST. HURRY.","BOLT: SAND IN MY GEARS. AGAIN.",
        "BOLT: BRR. MY BOLTS ARE CLICKING.","BOLT: THE PLANTS ARE TALLER THAN THE SHIP.","BOLT: THIS IS A DEATH PLANET. I WOULD LIKE TO LEAVE.","BOLT: A WHOLE PLANET OF CITY. WHERE DO THEY PARK?","BOLT: IT IS SO NICE HERE. I AM SUSPICIOUS."};
    ln[0]=cm; ln[1]=l1; ln[2]=l2; ln[3]=sc; ln[4]=l3; ln[5]= (endless && wd.cls) ? pq[wd.cls] : bolt_quip[q];
    card(pg,world_name(),ln,6);
}
static void local_talk(PObj *o){
    volatile u16 *pg=VISPG; const char *ln[2]; char g[40];
    static const char *const love_ln[12]={"YOU CAME! WE BAKED YOU A PIE!","EVERYONE, LOOK! IT IS THE BUG!","CAN I HAVE YOUR AUTOGRAPH? ON MY SHELL?","WE NAMED A HILL AFTER YOU.",
        "STAY FOREVER. WE HAVE SOUP.","YOU ARE THE BEST THING THAT EVER LANDED.","MY KIDS HAVE YOUR POSTER.","TAKE THIS. TAKE EVERYTHING.","WE LOVE YOUR LITTLE LEGS.","A HERO! A REAL ONE! HI!",
        "THE MAYOR WANTS TO HUG YOU. SHE IS A MOTH.","WE SING SONGS ABOUT YOU. BAD ONES. WITH LOVE."};
    static const char *const hate_ln[12]={"GO AWAY.","WE DO NOT LIKE BUGS. OR YOU.","YOUR SHIP IS UGLY.","THE LAST BUG HERE NEVER LEFT. JUST SAYING.","NO.",
        "PRICES DOUBLE WHEN YOU LOOK AT THEM.","DO NOT TOUCH ANYTHING.","I AM CALLING THE MAYOR. SHE BITES.","YOU SMELL LIKE SPACE.","LEAVE. NOW. PLEASE. NO, NOT PLEASE. LEAVE.",
        "WE HAD A NICE DAY UNTIL YOU CAME.","EVERYTHING HERE BITES. ME TOO."};
    int li=(int)((o->x>>16)*7+(o->y>>16)*13+wd.tseed)%NTALK; ln[0] = wd.mood==MOOD_LOVE ? love_ln[li%12] : wd.mood==MOOD_HATE ? hate_ln[li%12] : talk_ln[li];
    if(wd.mood==MOOD_HATE) o->hp=0; else if(wd.mood==MOOD_LOVE && rndi(3)==0) o->hp=1;
    if(o->hp){ o->hp=0; int r=wr(4);
        if(r==0){ gold+=5+dlv()*3; scpy(g,"HERE, TAKE "); ncat(g,5+dlv()*3); scat(g,"G. FOR THE LEGS."); }
        else if(r==1){ Gear gg={IT_CRUMB,0,0,0,0,0}; add_gear(gg); scpy(g,"HAVE A CRUMB. I HAVE TOO MANY."); }
        else if(r==2){ Gear gg={IT_BERRY,(u8)rndi(8),0,0,0,0}; add_gear(gg); scpy(g,"A BERRY. DO NOT ASK WHERE IT GREW."); }
        else if(form==FORM_HUMAN && (tools_owned|0xFF)!=tools_owned && rndi(2)){ int tl; do tl=rndi(NTOOL); while((tools_owned>>tl)&1); tools_owned|=1u<<tl; scpy(g,"YOU HAVE HANDS! TAKE THIS: "); scat(g,tool_name[tl]); }
        else { fame+=3; scpy(g,"I WILL TELL EVERYONE YOU ARE NICE. +FAME"); }
        ln[1]=g; card(pg,"A LOCAL",ln,2); sfx(7); }
    else { char w[40]; scpy(w,"KNOWN FOR: "); scat(w,tagline[wd.tagline]); ln[1]=w; card(pg,"A LOCAL",ln,2); }
    if(pq_type==5 && !pq_done){ pq_have++; }
}
// ---------------------------------------------------------------- the bounty office: a job per planet
static void job_text(char *b){
    static const char *const jt[8]={"","SQUASH ","BEAT ","DIG UP ","WIN THE RACE","TALK TO ","HUNT ","MINE "};
    scpy(b,jt[pq_type]); if(pq_type!=4){ ncat(b,pq_need); scat(b," "); }
    switch(pq_type){ case 1: scat(b,"SLUGS"); break; case 2: scat(b,"BEETLES"); break; case 3: scat(b,"FOSSILS"); break; case 5: scat(b,"LOCALS"); break; case 6: scat(b,spc[0].name); scat(b,"S"); break; case 7: scat(b,"ORE NODES"); break; }
}
static void job_tick(void){                                                           // called when a counter moves: pays out the moment the job is done
    if(!pq_type || pq_done) return;
    if(pq_type==4 ? race_best>0 : pq_have>=pq_need){ pq_done=1; gold+=pq_gold; fame+=8; gain_xp(10+dlv()*3); st_uniq++; ach_check();
        char b[40]; scpy(b,"JOB DONE! THE OFFICE WIRED "); ncat(b,pq_gold); scat(b,"G. BEEP."); show_msg(b); sfx(6); }
}
static void bounty_office(volatile u16 *pg){
    const char *ln[4]; char j[48], p[40];
    if(pq_type && !pq_done){ job_text(j); scpy(p,"SO FAR: "); if(pq_type==4) scat(p, race_best?"WON":"NOT YET"); else { ncat(p,pq_have); scat(p,"/"); ncat(p,pq_need); }
        ln[0]="YOUR JOB IS STILL OPEN:"; ln[1]=j; ln[2]=p; card(pg,"BOUNTY OFFICE",ln,3); return; }
    // a new job that this planet can actually offer
    int tries=0; do { pq_type=1+wr(7); tries++;
        if(pq_type==2 && !wd.beetles) pq_type=0; if(pq_type==3 && !wd.fossils) pq_type=0; if(pq_type==5 && !city_r) pq_type=0; if(pq_type==6 && spc[0].temper==0 && wr(2)) pq_type=0;
    } while(!pq_type && tries<10);
    if(!pq_type) pq_type=1;
    static const u8 need[8]={0,3,1,1,1,3,2,3}; pq_need=need[pq_type]+(pq_type==1||pq_type==5||pq_type==7 ? wr(3) : 0); pq_have=0;
    { static const u8 kk[8]={0,O_SLUG,O_BEETLE,O_FOSSIL,0,O_LOCAL,O_CRIT,O_TREE}; int have=0;                    // never ask for more than the planet has
      if(pq_type!=4) for(int i=0;i<NPO;i++) if(po[i].alive && (po[i].kind==kk[pq_type] || (pq_type==7 && po[i].kind>=O_TREE && po[i].kind<=O_FLOWER)) && (pq_type!=6 || po[i].var==0)) have++;
      if(pq_type!=4 && pq_need>have) pq_need = have>0 ? have : 1; } pq_done=0; pq_gold=20+dlv()*8+pq_need*6+(pq_type==4?30:0);
    job_text(j); scpy(p,"PAYS "); ncat(p,pq_gold); scat(p,"G AND FAME. WE WIRE IT. BEEP.");
    ln[0]="THE CLERK SLIDES YOU A NOTE:"; ln[1]=j; ln[2]=p; ln[3]="(IT IS PAID WHEN IT IS DONE. NO TRIPS BACK)"; card(pg,"BOUNTY OFFICE",ln,4); sfx(2);
}
// ---------------------------------------------------------------- the market and the cantina
static int mk_sell[8], mk_n; static int mk_buy[3];
static int local_price(int g){ int p=good_price(g); if(city_r) p=p*(85+(wd.csize>3?3:wd.csize)*5)/100; if(wd.mood==MOOD_LOVE) p=p*115/100; if(wd.mood==MOOD_HATE) p=p*75/100; return p<1?1:p; }   // they pay you more if they love you
static int mk_tool[NTOOL], mk_nt;
static void market_row(int i, char *b, int *c){
    if(i>=mk_n+3){ int tl=mk_tool[i-mk_n-3]; scpy(b,"TOOL: "); scat(b,tool_name[tl]); scat(b,"  "); ncat(b,tool_cost[tl]+dlv()*10); scat(b,"G"); *c = gold>=tool_cost[tl]+dlv()*10 ? C_CYAN : C_DGREY; return; }
    if(i<mk_n){ int g=mk_sell[i]; scpy(b,"SELL ALL "); scat(b,goods[g]); scat(b," ("); ncat(b,cargo[g]); scat(b,")  "); ncat(b,local_price(g)*cargo[g]); scat(b,"G"); *c=C_GREEN; }
    else { int g=mk_buy[i-mk_n]; scpy(b,"BUY 1 "); scat(b,goods[g]); scat(b,"  "); ncat(b,local_price(g)*120/100); scat(b,"G"); *c = gold>=local_price(g)*120/100 ? C_GREY : C_DGREY; }
}
static void market(volatile u16 *pg){
    int sel=0, r; list_desc=0;
    while(1){
        mk_n=0; for(int g=0;g<8;g++) if(cargo[g]) mk_sell[mk_n++]=g;
        int e=sys[cur_sys].eco; mk_buy[0]=e==0?0:e==1?4:e==2?5:1; mk_buy[1]=e==0?1:e==1?3:e==2?6:2; mk_buy[2]=7;
        mk_nt=0; if(form==FORM_HUMAN) for(int k=0;k<NTOOL;k++) if(!((tools_owned>>k)&1) && ((wd.tseed>>k)&1 || k<2)) mk_tool[mk_nt++]=k;   // humanoids can buy tools
        if((r=list_pick(pg,"THE MARKET",mk_n+3+mk_nt,market_row,&sel,"A: TRADE  B: LEAVE"))<0) break;
        if(r>=mk_n+3){ int tl=mk_tool[r-mk_n-3], c=tool_cost[tl]+dlv()*10; if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; tools_owned|=1u<<tl; tool_held=tl; recalc(); char b[40]; scpy(b,"NOW HOLDING: "); scat(b,tool_name[tl]); say(b); sfx(6); } sel=0; continue; }
        if(r<mk_n){ int g=mk_sell[r]; int got=local_price(g)*cargo[g]; gold+=got; cargo[g]=0; char b[40]; scpy(b,"SOLD. +"); ncat(b,got); scat(b,"G"); say(b); sfx(7); if(sel>=mk_n+2) sel=0; }
        else { int g=mk_buy[r-mk_n]; int cost=local_price(g)*120/100; if(gold<cost) say("NOT ENOUGH GOLD"); else if(cargo_n()>=cargo_cap+ship_hold_bonus()) say("YOUR HOLD IS FULL");
            else { gold-=cost; cargo[g]++; say("BOUGHT. SELL IT WHERE IT IS RARE"); sfx(7); } }
    }
}
static const char *const gossip[16]={"THEY SAY THE FOSSILS ARE SOUTH OF TOWN. OR NORTH.","A PIRATE DOCKED LAST WEEK. HE TIPPED WELL.","THE RUINS OPEN FOR A RIDDLE. MOST GET IT WRONG.","RELICS SELL BEST AT TECH STATIONS.",
    "THE BEETLES HATE SIDESTEPS. JUST SAYING.","THE OFFICE WIRES GOLD THE SECOND A JOB IS DONE.","CURIOUS CREATURES WALK UP TO YOU. TIMID ONES RUN.","HOSTILE ONES BITE. BITE THEM BACK.",
    "DRONE OUTPOSTS HOLD THE DROID'S MEMORY.","THE RACE PAR IS BEATABLE. BARELY.","HIVES FEED THE BEAST. SMASH THEM.","THE BARTENDER IS A BEETLE. ARM WRESTLE HIM.",
    "SELL SILK TO FARMERS. SELL FOOD TO MINERS.","THE NEXT CLUSTER IS A JUMP AWAY, WHEN THE KING FALLS.","SOMEONE SAW A SHIP IN THE SKY. IT WAS YOURS.","THERE IS SOUP. THERE IS ALWAYS SOUP."};
static void cant_row(int i, char *b, int *c){ static const char *const r[4]={"A BOWL OF SOUP  8G  (FOOD +40)","HEAR THE GOSSIP  (FREE)","BUY A ROUND  20G  (+FAME)","ARM WRESTLE THE BARTENDER  10G"}; scpy(b,r[i]); }
static void cantina(volatile u16 *pg){
    int sel=0, r; list_desc=0;
    while((r=list_pick(pg,"THE CANTINA",4,cant_row,&sel,"A: PICK  B: LEAVE"))>=0){
        if(r==0){ if(gold<8) say("NOT ENOUGH GOLD"); else { gold-=8; eat(40); say("SLURP. DO NOT ASK WHAT WAS IN IT"); sfx(11); } }
        if(r==1){ say(gossip[wr(16)]); }
        if(r==2){ if(gold<20) say("NOT ENOUGH GOLD"); else { gold-=20; fame+=6; say("CHEERS! EVERYONE LIKES YOU NOW. +FAME"); sfx(6); } }
        if(r==3){ if(gold<10) say("NOT ENOUGH GOLD"); else { gold-=10; int win = rndi(100) < 35+st_str*3; if(win){ gold+=30; say("SLAM! HE CRIES A LITTLE. +30G"); sfx(6); } else say("HE WINS. HE ALWAYS WINS. HE IS A BEETLE"); } }
    }
}
// ---------------------------------------------------------------- races, ruins, fossils, beetles
static void race_start(void){
    race_on=1; race_t=0; race_next=0; int a=wr(256);
    for(int i=0;i<5;i++){ int d=5+i*4; race_x[i]=(ppx+cosx((a+i*40)&255)*d*4)&((TW<<16)-1); race_y[i]=(ppy+sinx((a+i*40)&255)*d*4)&((TW<<16)-1);
        if(hm_at(race_x[i],race_y[i])<water_lv) race_x[i]+=3<<16; }
    race_par = 60*(14+wd.grav*2);
    for(int i=0;i<NPO;i++) if(po[i].kind==O_BEACON) po[i].alive=0;
    for(int j=0;j<NPO;j++) if(!po[j].alive){ po[j].kind=O_BEACON; po[j].alive=1; po[j].x=race_x[0]; po[j].y=race_y[0]; break; }
    show_msg("GO! TOUCH THE FLAGS IN ORDER"); sfx(6);
}
static void race_tick(int n){
    if(!race_on) return; race_t+=n;
    s32 dx=wrapd(race_x[race_next]-ppx), dy=wrapd(race_y[race_next]-ppy);
    if((dx<0?-dx:dx)+(dy<0?-dy:dy) < 0x18000){ sfx(7); race_next++;
        for(int i=0;i<NPO;i++) if(po[i].kind==O_BEACON){ if(race_next<5){ po[i].x=race_x[race_next]; po[i].y=race_y[race_next]; } else po[i].alive=0; }
        if(race_next>=5){ race_on=0; int sec=race_t/60; char b[40];
            if(race_t<=race_par){ int g=25+dlv()*5; gold+=g; fame+=5; race_best=race_t; scpy(b,"UNDER PAR! "); ncat(b,sec); scat(b,"S. +"); ncat(b,g); scat(b,"G"); sfx(6); job_tick(); }
            else { scpy(b,"DONE IN "); ncat(b,sec); scat(b,"S. PAR WAS "); ncat(b,race_par/60); scat(b,"S. AGAIN?"); }
            show_msg(b); }
        else { char b[40]; scpy(b,"FLAG "); ncat(b,race_next); scat(b,"/5  "); ncat(b,race_t/60); scat(b,"S"); show_msg(b); } }
}
typedef struct { const char *q; const char *a[3]; u8 ok; } Riddle;
static const Riddle riddles[10]={
    {"WHAT HAS LEGS BUT NEVER WALKS?",{"A CHAIR","A SLUG","A BUS"},0},{"WHAT GETS WETTER THE MORE IT DRIES?",{"A ROCK","A TOWEL","BOLT"},1},{"WHAT HAS A HEAD AND A TAIL BUT NO BODY?",{"A COIN","A BEETLE","THE MAYOR"},0},
    {"THE MORE YOU TAKE, THE MORE YOU LEAVE BEHIND.",{"GOLD","FOOTSTEPS","SOUP"},1},{"WHAT CAN YOU CATCH BUT NOT THROW?",{"A FLEA","A COLD","A FLAG"},1},{"WHAT HAS MANY KEYS BUT OPENS NO DOORS?",{"A DRONE","A PIANO","A BAG"},1},
    {"WHAT GOES UP AND NEVER COMES DOWN?",{"YOUR AGE","A SHIP","RAIN HERE"},0},{"I HAVE CITIES BUT NO HOUSES. WHAT AM I?",{"A PLANET","A MAP","A DREAM"},1},{"WHAT RUNS BUT HAS NO LEGS?",{"A HOPPER","A RIVER","A RACE"},1},
    {"WHAT IS FULL OF HOLES BUT HOLDS WATER?",{"A SPONGE","A HIVE","A SOCK"},0}};
static int rid_i; static void rid_row(int i, char *b, int *c){ scpy(b,riddles[rid_i].a[i]); }
static void ruin_visit(volatile u16 *pg, PObj *o){
    const char *ln[3]; ln[0]="OLD STONES. A VOICE HUMS A RIDDLE:"; rid_i=(int)((wd.tseed>>5)%10); ln[1]=riddles[rid_i].q; ln[2]="(PICK WELL. THE STONES ARE PROUD)";
    card(pg,"THE RUIN",ln,3); int sel=0; list_desc=0; int r=list_pick(pg,"YOUR ANSWER",3,rid_row,&sel,"A: ANSWER  B: THINK");
    if(r<0) return;
    if(r==riddles[rid_i].ok){ o->hp=0; if(relic_roll(40)) return; int g=40+dlv()*10; gold+=g; cargo[7]+= (cargo_n()<cargo_cap+ship_hold_bonus()) ? 1 : 0; fame+=5; st_uniq++; ach_check();
        char b[40]; scpy(b,"THE STONES OPEN. +"); ncat(b,g); scat(b,"G AND A RELIC"); say(b); sfx(6); }
    else { hurt(5); say("WRONG. THE STONES BONK YOU. RUDE."); sfx(12); }
}
static void weather_draw(volatile u16 *pg){                                           // rain, snow and dust: a handful of pixels a frame, cheap
    if(!wd.weather) return; weather_t+=3; u32 s=wd.tseed+(frames>>1)*7;
    int n = wd.weather==3 ? 24 : 40;
    for(int k=0;k<n;k++){ s=s*1664525u+1013904223u; int x=(s>>8)%120, y=20+((s>>20)%138 + (wd.weather==2 ? (frames>>1) : frames*3)*(k+1))%138;
        int ix = wd.weather==1 ? 13 : wd.weather==2 ? C_WHITE : C_GOLD;
        if(wd.weather==1 && y<159){ pg[y*120+x]=ix|(ix<<8); pg[(y+1)*120+x]=ix|(ix<<8); } else pg[y*120+x]=(pg[y*120+x]&0xFF00)|ix; }
}
