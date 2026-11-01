// ================================================================= THE STORMCLOCK COIL, part 3: what you fly THROUGH, and the speed readout
// (the state and the speed maths are in coil.inc and coilnum.inc, the glitch show and the save block in coil2.inc)
//
// PASS-THROUGH. The flight view is a small box round one star, but the coil's speed is the real one, so at these speeds the ship would cut straight
// through whatever lies on the way: planets, stars, dust and gas, black holes, whole galaxies and the voids between them. This file keeps a model of that.
// It is a GAME MODEL, not physics: nothing travels faster than light in the real world, so nobody knows what it would feel like.
//   * How often something comes, and how hard, depends on the speed (decades = log10 of the speed in c): the faster, the more often and the harder.
//   * What comes depends on the scale you are at: planets and dust among the stars, stars and nebulae across a galaxy, cores and galaxies beyond it.
//   * Each thing has an effect, named on screen while it lasts ("STAR PASS: RED GIANT  HEAT 40%"): HEAT (a meter 0 to 100; at 100 the coil drops out
//     by itself: COIL OFF: OVERHEATED), SHOCKWAVE (shield, then hull), DRAG (fuel), TIDES (shield and a stretched picture), LENSING (curved light),
//     BLUESHIFT (the background glow ahead burns hotter). Near a black hole or a neutron star time also slows (named; the clock is not touched).
//   * FAIR BY RULE: the hull is never taken below 10 by a pass-through, the effects never touch the save, and they stop at once when you slow down
//     (coil_cut resets the model). Options (EXTRAS): PASS-THROUGH EFFECTS FULL / REDUCED / VISUAL ONLY / OFF, damage strength and screen strength.
enum { PK_NONE, PK_PLANET, PK_STAR, PK_DUST, PK_HOLE, PK_GALAXY, PK_VOID, PK_N };
static const char *const pk_head[PK_N]={"CLEAR SPACE","PLANET PASS","STAR PASS","GAS CLOUD","BLACK HOLE PASS","GALAXY PASS","VOID"};
static const char *const pk_eff[PK_N]={"","SHOCKWAVE","HEAT","DRAG","TIDES","LENSING","BLUESHIFT"};
static const char *const pk_sub[PK_N][5]={{"","","","",""},{"MOON","ROCK WORLD","ICE GIANT","GAS GIANT",""},{"RED DWARF","SUN-LIKE STAR","BLUE STAR","RED GIANT","NEUTRON STAR"},
    {"DUST CLOUD","GAS NEBULA","STAR NURSERY","",""},{"STELLAR HOLE","GALAXY CORE","","",""},{"GALAXY DISC","GALAXY CLUSTER","","",""},{"BETWEEN GALAXIES","","","",""}};
static const u8 pk_nsub[PK_N]={1,4,5,3,2,2,1};
//                                   base strength 0..100 per sub     heat  shield fuel   (per pass, before speed and strength)
static const u8 pk_rel[PK_N][5]={{0},{20,40,60,80,0},{20,35,60,75,65},{30,50,70,0,0},{80,100,0,0,0},{40,60,0,0,0},{25,0,0,0,0}};
static const u8 pk_heat[PK_N][5]={{0},{0,0,1,1,0},{2,4,7,9,6},{1,1,2,0,0},{3,5,0,0,0},{1,2,0,0,0},{1,0,0,0,0}};
static const u8 pk_hurt[PK_N][5]={{0},{1,2,3,4,0},{0,0,1,2,1},{0,0,0,0,0},{3,4,0,0,0},{0,0,0,0,0},{0,0,0,0,0}};
static const u8 pk_fuel[PK_N][5]={{0},{0,0,0,0,0},{0,0,0,0,0},{1,2,2,0,0},{0,1,0,0,0},{1,1,0,0,0},{0,0,0,0,0}};
static const u8 pk_len[PK_N]={0,40,70,100,90,140,110};
//                            planet star dust hole galaxy void   (weights by scale: among the stars / across a galaxy / beyond it)
static const u8 pk_wt[4][6]={{6,2,3,0,0,0},{2,5,4,0,0,0},{1,5,4,1,1,0},{0,3,3,3,5,3}};
static u32 pt_rnd(void){ pt_seed^=pt_seed<<13; pt_seed^=pt_seed>>17; pt_seed^=pt_seed<<5; return pt_seed; }    // (its own numbers: a pass-through must not change the world)
static int pt_mode(void){ return xo_i(XO_PASS); }                // 0 FULL, 1 REDUCED, 2 VISUAL ONLY, 3 OFF
static int pt_dmg(void){ static const u8 m[4]={100,50,0,0}, st[4]={100,50,25,10}; return m[pt_mode()]*st[xo_i(XO_PDMG)%4]/100; }     // percent of damage, heat and fuel
static int pt_vis(void){ static const u8 m[4]={100,50,100,0}, st[3]={100,50,25}; return m[pt_mode()]*st[xo_i(XO_PVIS)%3]/100; }        // percent of the screen effects
static void pt_reset(void){ pt_kind=PK_NONE; pt_t=pt_len=0; pt_peak=0; pt_heat=0; pt_hit=0; pt_next=90; pt_stretch=256; if(!pt_seed) pt_seed=0x2545F491u; }
static int pt_decades(CW w){ return cw_zero(w) ? 0 : cw_decades(w); }
static int pt_env(void){ int h=pt_len/2; if(h<1) h=1; int d = pt_t<h ? pt_t : (int)pt_len-pt_t; return d<0 ? 0 : d*100/h; }   // 0..100..0 over the pass
static int pt_pct(void){ return pt_kind ? pt_peak*pt_env()/100 : 0; }
static void pt_hurt(int dm){                                      // damage: shield first, then hull, but the hull never goes below 10
    if(dm<=0 || dbg_god || xo_i(XO_GOD)) return;
    while(dm>0){ if(shield>0) shield--; else if(hull>10) hull--; else break; dm--; } }
static void pt_start(int d){
    int reg = d<3 ? 0 : d<9 ? 1 : d<13 ? 2 : 3; if(d>=30) reg=3;
    int tot=0; for(int k=0;k<6;k++) tot+=pk_wt[reg][k]; int r=(int)(pt_rnd()%(u32)tot), k=0; while(r>=pk_wt[reg][k]){ r-=pk_wt[reg][k]; k++; }
    pt_kind=(u8)(k+1); pt_sub=(u8)(pt_rnd()%pk_nsub[pt_kind]);
    int rel=pk_rel[pt_kind][pt_sub]*(100+2*d)/100; if(rel>100) rel=100; if(rel<10) rel=10; pt_peak=(u8)rel;
    pt_len=(u16)(pk_len[pt_kind]*(100-d)/100); if(pt_len<30) pt_len=30; pt_t=0; pt_hit=0;
    if(coil_passes<0xFFFFFFF0u) coil_passes++; if(pt_kind==PK_STAR && coil_pstars<0xFFFFFFF0u) coil_pstars++; if(pt_kind==PK_PLANET && coil_pplanets<0xFFFFFFF0u) coil_pplanets++;
}
static void pt_dose(int d){                                       // four doses over a pass, each a quarter of the pass's whole effect
    int I=100+5*d, dm=pt_dmg(); if(dm<=0) return;
    int hh=(pk_heat[pt_kind][pt_sub]*I*dm+19999)/20000, hu=(pk_hurt[pt_kind][pt_sub]*I*dm+39999)/40000, fu=(pk_fuel[pt_kind][pt_sub]*I*dm+39999)/40000;   // (a quarter of the pass's total each, rounded up; heat counts double)
    if(hh){ int h=pt_heat+hh; pt_heat=(u8)(h>100?100:h); }
    pt_hurt(hu); if(hu) { sshake=6; pt_hit=1; }
    if(fu && !dbg_fuel){ fuel-=xo_fuel(fu); if(fuel<0) fuel=0; }
    if(pt_kind==PK_PLANET && pt_t>=pt_len/2-1) sshake=8; }
static void pt_frame(CW w, int gt){                               // every flying frame while the coil is engaged (called from coil_fx_post)
    (void)gt; if(pt_mode()==3 || cw_zero(w)){ if(pt_kind||pt_heat) pt_reset(); return; }
    int d=pt_decades(w);
    if(pt_kind){
        if(pt_len>=4 && pt_t>0 && (pt_t%(pt_len/4))==0 && pt_t<pt_len-1) pt_dose(d);
        if(++pt_t>=pt_len){ pt_kind=PK_NONE; pt_stretch=256; int per=300-6*d; if(per<40) per=40; pt_next=(u16)(per+(int)(pt_rnd()%(u32)(per/2+1))); } }
    else if(pt_next>0) pt_next--; else if(!cw_zero(coil_cur)) pt_start(d);
    if(pt_heat>0 && (fx_t&15)==0 && !(pt_kind==PK_STAR || pt_kind==PK_HOLE)) pt_heat--;                  // it cools between stars
    if(pt_heat>=70 && (fx_t%45)==0){ pt_hurt((pt_dmg()+49)/50); sshake=3; }                            // the hull glows: a little shield burns
    if(pt_heat>=100 && pt_dmg()>0 && !cw_zero(coil_cur)){ pt_heat=60; coil_cut("COIL OFF: OVERHEATED. SLOWING DOWN"); } }
// ---------------------------------------------------------------- what the screen shows
static void pt_ring(volatile u16 *pg, int cx, int cy, int r, int c, int a0, int a1){        // an arc of a circle (a0..a1 in 1/256ths of a turn), squashed to the view's shape
    int px=cx+(cosx(a0)*r>>14), py=cy+(sinx(a0)*r>>14)*3/4;
    for(int a=a0+8;a<=a1;a+=8){ int x=cx+(cosx(a)*r>>14), y=cy+(sinx(a)*r>>14)*3/4; line2(pg,px,py,x,y,c); px=x; py=y; } }
static void pt_draw(volatile u16 *pg, int gt, int S){            // drawn over the world, before the glitch show; S = flash-safe (nothing flickers: shapes move slowly, no random noise)
    (void)gt; pt_stretch=256;
    int vis=pt_vis(); if(!pt_kind || vis<=0 || fx_off()) return;
    int v=pt_pct()*vis/100; if(v<=0) return;                       // 0..100
    int t=pt_t, cx=120, cy=VCY;
    switch(pt_kind){
    case PK_STAR: {                                                // a glare from the sides, white-hot in the middle of the view
        int wd=v*70/100; u32 sd=pt_seed;
        for(int y=0;y<128;y+=2){ int ww = S ? wd : wd*(3+(int)((sd>>(y&15))&3))/5; rect(pg,0,y,ww,1,pt_sub>=2?55:51); rect(pg,240-ww,y,ww,1,pt_sub>=2?55:51); }
        { int rr=2+v/10; rect(pg,cx-rr*2,cy-rr,rr*4,rr*2,54); rect(pg,cx-rr,cy-rr*2,rr*2,rr*4,49); } break; }
    case PK_PLANET: {                                              // a shockwave ring spreading from the middle
        int rr=6+t*110/(pt_len?pt_len:1); pt_ring(pg,cx,cy,rr,54,0,256); if(v>30) pt_ring(pg,cx,cy,rr*3/4,50,0,256);
        if(v>50 && !S) rect(pg,0,cy+rr*3/4-1,240,1,54); break; }
    case PK_DUST: {                                                // blue-shifted streaks racing past in a haze
        int n=v/3; u32 sd=pt_seed^0x5A5A5A5Au;
        for(int i=0;i<n;i++){ sd=sd*1664525u+1013904223u; int y=(int)((sd>>8)%128), x=(int)((sd>>16)%200)+(S?0:(int)(fx_rnd()%24)); rect(pg,x,y,16+(int)(sd&15),1,(sd&4)?55:53); } break; }
    case PK_HOLE: {                                                // tides stretch the picture, the edges darken, light bends into rings
        int mx = S ? 25 : 60; pt_stretch=256+v*mx/100;
        int bar=v*20/100; rect(pg,0,0,240,bar,U_BG0); rect(pg,0,128-bar,240,bar,U_BG0);
        pt_ring(pg,cx,cy,30+v/5,54,0,256); pt_ring(pg,cx,cy,12+v/8,53,(t*2)&255,((t*2)&255)+170); break; }
    case PK_GALAXY: {                                              // lensing: arcs of light curved round something huge
        int a=(t*(S?1:3))&255; pt_ring(pg,cx,cy,40+v/3,54,a,a+90); pt_ring(pg,cx,cy,52+v/3,55,a+128,a+210); pt_ring(pg,cx,cy,64+v/3,54,a+60,a+110); break; }
    default: {                                                     // between galaxies: the background glow ahead is blueshifted into a dim blue rim
        int e=v/4; for(int i=0;i<e;i++){ rect(pg,i,0,1,128,58); rect(pg,239-i,0,1,128,58); } break; } } }
// ---------------------------------------------------------------- the readout: four lines above the cockpit (what you pass, a rotating equivalent, the speed itself, a rotating trip time)
static const char *const hud_rate_nm[CR_N]={"EARTHS CROSSED: ","STARS PASSED: ","MILKY WAYS CROSSED: ","GALAXIES PASSED: ","UNIVERSES CROSSED: "};
static void coil_pass_line(char *l){                              // "STAR PASS: RED GIANT  HEAT 40%"
    l[0]=0; int engaged=!cw_zero(coil_cur);
    if(pt_mode()==3 || !engaged){ return; }
    if(!pt_kind){ scpy(l,"CLEAR SPACE: SKY SQUEEZED AHEAD"); }
    else { scpy(l,pk_head[pt_kind]); scat(l,": "); scat(l,pk_sub[pt_kind][pt_sub]); scat(l,"  "); scat(l,pk_eff[pt_kind]); scat(l," ");
        ncat(l, (pt_kind==PK_STAR && pt_dmg()>0) ? pt_heat : pt_pct()); scat(l,"%");
        if(pt_kind==PK_HOLE || (pt_kind==PK_STAR && pt_sub==4)) scat(l,"  TIME SLOWS"); }
    if(pt_heat>0 && pt_kind!=PK_STAR){ scat(l,"  HEAT "); ncat(l,pt_heat); scat(l,"%"); } }
// The glitch show tilts, mirrors and stretches the whole picture (the hardware's BG2 matrix hud_m), text and all. The readout must stay true and readable, so
// when the picture is warped the text is drawn already counter-warped: every lit pixel of the text goes to the bitmap place that the matrix sends to the
// pixel's screen place (a screen pixel (x,y) shows bitmap pixel ((X + pa x + pb y) / 256, (Y + pc x + pd y) / 256)). Cost: only the lit pixels.
static void hud_px(volatile u16 *pg, int sx, int sy, int c){
    if(!hud_on){ if(sx>=0 && sx<240 && sy>=0 && sy<160) pix(pg,sx,sy,c); return; }
    s32 u=(hud_m[4]+hud_m[0]*sx+hud_m[1]*sy)>>8, v=(hud_m[5]+hud_m[2]*sx+hud_m[3]*sy)>>8;
    if(u>=0 && u<240 && v>=0 && v<160) pix(pg,u,v,c); }
static void hud_text(volatile u16 *pg, int x, int y, const char *s, int c){
    if(!hud_on){ text(pg,x,y,s,c,1); return; }
    for(int cx=x;*s;s++,cx+=4){ u16 g=font[(u8)*s&127]; if(!g) continue;
        for(int k=0;k<15;k++) if(g&(1<<(14-k))) hud_px(pg,cx+k%3,y+k/3,c); } }
static void hud_back(volatile u16 *pg, int x, int y, int w, int h){                  // a dark box behind a line (when warped: every screen pixel of the box, sent through the matrix)
    if(!hud_on){ rect(pg,x,y,w,h,U_BG0); return; }
    for(int j=0;j<h;j++){ int sy=y+j; if(sy<0||sy>=160) continue; s32 u=hud_m[4]+hud_m[0]*x+hud_m[1]*sy, v=hud_m[5]+hud_m[2]*x+hud_m[3]*sy;
        for(int i=0;i<w;i++,u+=hud_m[0],v+=hud_m[2]){ int bu=u>>8, bv=v>>8; if(bu>=0 && bu<240 && bv>=0 && bv<160) pix(pg,bu,bv,U_BG0); } } }
static void hud_line(volatile u16 *pg, int y, const char *s, int c, int back){      // centred
    int x=120-textlen(s)*2; if(back) hud_back(pg,x-2,y-2,textlen(s)*4+4,9); hud_text(pg,x,y,s,c); }
static void coil_hud(volatile u16 *pg, CW w, int milli, int gt, int minimal){
    char b[96]; int S=fx_safe();
    coil_fmt(b,w,milli);
    char l[100]; scpy(l,minimal ? "TRUE SPEED " : "SPEED "); scat(l,b);                 // the speed is always the true one: nothing glitches this line
    int c = gt>=6 ? C_RED : gt>=4 ? C_GOLD : gt>=2 ? C_CYAN : C_WHITE;
    if(minimal){ hud_line(pg,150,l,C_GOLD,1); return; }                             // (a full-screen fake error is up: only the true speed, at the bottom)
    hud_line(pg,113,l,c,1);
    CS sp=coil_speed_cs(w,milli);
    if(sp.m){
        char t[100];
        static const u8 ord[CT_N+1]={CT_N,CT_UNIT,CT_MOON,CT_SUN,CT_PROX,CT_MW,CT_AND,CT_UNI,CT_GAME,CT_FAR};
        int k=ord[(fx_t/130)%(CT_N+1)]; scpy(t,"TO "); scat(t,coil_trip_nm[k]); scat(t,": ");
        coil_trip(t+textlen(t), k, (u32)(coil_pd()<1?1:coil_pd()), sp);
        if(gt>=3 && !S){ for(int i=0;t[i];i++) if(t[i]>='0'&&t[i]<='9' && (fx_rnd()&7)<(unsigned)(gt-1)) t[i]=(char)('0'+fx_rnd()%10); }       // (the glitch scrambles this extra line, never the speed)
        hud_line(pg,121,t,C_GREY,1);
        int q=(fx_t/110)%(1+CR_N); scpy(t,"");
        if(q==0){ scpy(t,"= "); cn_sn(t,cn_kmps(sp)); scat(t," KM PER SECOND"); }
        else { scpy(t,hud_rate_nm[q-1]); cn_rate_text(t+textlen(t),q-1,sp); }
        hud_line(pg,104,t,C_GREY,1); }
    coil_pass_line(l);
    if(l[0]){ int pc = pt_kind==PK_STAR ? C_GOLD : pt_kind==PK_HOLE ? C_RED : pt_kind==PK_PLANET||pt_kind==PK_DUST ? C_CYAN : pt_kind ? C_WHITE : C_GREY;
        hud_line(pg,95,l,pc,1); }
#ifdef TEST_COILFPS
    { static u32 lv=0; char f[16]; scpy(f,"V"); ncat(f,(int)(vbl_count-lv)); lv=vbl_count; text(pg,200,30,f,C_WHITE,1); scpy(f,"T"); ncat(f,coil_tier); text(pg,200,38,f,C_WHITE,1); }
#endif
    if(!cw_zero(coil_cur)){ hud_text(pg,6,20,"R OR SELECT: SLOW DOWN",C_CYAN); hud_text(pg,6,28,"START: SPEED DETAILS",C_GREY); }
    else if(coil_equipped()) hud_text(pg,6,20,"L+R: ENGAGE THE COIL",C_CYAN);
    if(coil_cheated()) hud_text(pg,212,20,"CHEAT",C_GOLD);
}
// ---------------------------------------------------------------- SPEED DETAILS (START while the coil runs): five pages, a snapshot of the speed at the moment you open it
static void cd_row(volatile u16 *pg, int y, const char *lab, const char *val, int c){ text(pg,10,y,lab,C_GREY,1); rtext(pg,231,y,val,c); }
static void cd_draw(volatile u16 *pg, int pgn, CW w, int milli){
    CS sp=coil_speed_cs(w,milli); int gt=coil_gt(w);
    { { rect(pg,0,0,240,160,U_BG0); ui_bg(pg,0,160); ui_panel(pg,4,4,232,152);
            static const char *const hd[9]={"YOUR SPEED, EXACT","CROSSED EVERY SECOND","HOW LONG A TRIP TAKES","WHERE TO: DISTANCES","TRIPS IN SECONDS","TRIPS IN PLANCK TIMES","SPEED FOR ONE PLANCK TIME","ALL SEVEN SPEEDS","PASSING THROUGH"};
            ui_head(pg,9,8,200,hd[pgn],C_UNIQ); char n[8]; scpy(n,""); ncat(n,pgn+1); scat(n,"/9"); rtext(pg,231,8,n,C_GREY);
            char a[100], t[100]; int y=22;
            if(pgn==0){
                cd_row(pg,y,"TIER",coil_tier ? coil_tn[coil_tier] : "SHIP ENGINES",C_WHITE); y+=11;
                coil_fmt(a,w,milli); cd_row(pg,y,"SPEED",a,gt>=6?C_RED:C_GOLD); y+=11;
                if(!cw_zero(w) && cs_cmp(sp,cs_u(1000000))>=0){ a[0]=0; cn_words(a,sp); scat(a," X LIGHT"); cd_row(pg,y,"IN WORDS",a,C_WHITE); } y+=11;
                a[0]=0; cn_sn(a,cn_kmps(sp)); cd_row(pg,y,"KILOMETRES PER SECOND",a,C_WHITE); y+=11;
                a[0]=0; cn_sn(a,cn_lyps(sp)); cd_row(pg,y,"LIGHT YEARS PER SECOND",a,C_WHITE); y+=11;
                if(sp.m){ cn_time(a,cs_div(cs_u(31557600ULL),sp)); cd_row(pg,y,"ONE LIGHT YEAR TAKES",a,C_CYAN); } y+=15;
                text(pg,10,y,"LIGHT ITSELF: 299,792.458 KM PER SECOND, EXACTLY.",C_GREY,1); y+=9;
                text(pg,10,y,"A LIGHT YEAR IS 9.4607 X 10^12 KM. ABOVE 1 C IS NOT REAL",C_GREY,1); y+=9;
                text(pg,10,y,"PHYSICS: NOTHING KNOWN CAN DO IT. THIS IS A GAME MODEL.",C_GREY,1); }
            else if(pgn==1){
                for(int k=0;k<CR_N;k++){ scpy(a,hud_rate_nm[k]); a[textlen(a)-2]=0; if(sp.m) cn_rate_text(t,k,sp); else scpy(t,"NONE"); cd_row(pg,y,a,t,C_WHITE); y+=11; }
                y+=4; text(pg,10,y,"AN EARTH IS 12,742 KM ACROSS. STARS ARE ONE PER 4 LIGHT",C_GREY,1); y+=9;
                text(pg,10,y,"YEARS, GALAXIES ONE PER 3 MILLION. THE MILKY WAY IS",C_GREY,1); y+=9;
                text(pg,10,y,"100,000 LY WIDE, THE OBSERVABLE UNIVERSE 93 BILLION.",C_GREY,1); }
            else if(pgn==2){
                int tiny=0;
                for(int k=0;k<=CT_N;k++){ static const u8 ord[CT_N+1]={CT_N,CT_UNIT,CT_MOON,CT_SUN,CT_PROX,CT_MW,CT_AND,CT_UNI,CT_GAME,CT_FAR}; int o=ord[k];
                    scpy(a,coil_trip_nm[o]); coil_trip(t,o,(u32)(coil_pd()<1?1:coil_pd()),sp);
                    CS tl = o==CT_N ? cs_div(cs_u((u64)(coil_pd()<1?1:coil_pd())*2000000ULL),cs_u(299792458ULL)) : cn_trip_light(o); int sub = sp.m && cs_cmp(cs_div(tl,sp),CN_TP)<0; if(sub) tiny=1;
                    cd_row(pg,y,a,t,sub?C_RED:C_WHITE); y+=10; }
                y+=3; if(tiny){ text(pg,10,y,"RED: UNDER ONE PLANCK TIME (5.391247 X 10^-44 S): THE COUNT",C_RED,1); y+=9; text(pg,10,y,"IS A FRACTION OF ONE. PHYSICS HAS NO SHORTER TIME.",C_RED,1); }
                else { text(pg,10,y,"THE GAME MAP IS 25 X 2^155 LIGHT YEARS WIDE: BIGGER",C_GREY,1); y+=9; text(pg,10,y,"THAN THE REAL UNIVERSE. THE MOON IS 384,400 KM AWAY.",C_GREY,1); } }
            else if(pgn>=3 && pgn<=6){
                static const u8 ord[CT_N]={CT_UNIT,CT_MOON,CT_SUN,CT_PROX,CT_MW,CT_AND,CT_UNI,CT_GAME,CT_FAR};
                static const char *const nm[CT_N]={"A GAME UNIT (2,000 KM)","THE MOON","THE SUN (1 AU)","PROXIMA, NEAREST STAR","ACROSS THE MILKY WAY","ANDROMEDA, NEAREST GALAXY","THE OBSERVABLE UNIVERSE","EDGE OF THE GAME MAP","THE FURTHEST (CHEATS)"};
                int tiny=0;
                for(int k=0;k<CT_N;k++){ int o=ord[k], c=C_WHITE; t[0]=0;
                    if(pgn==3) cn_dist_text(t,o);
                    else if(pgn==4) cn_secs_text(t,o,sp);
                    else if(pgn==5){ cn_planck_text(t,o,sp); if(sp.m && cs_cmp(cs_div(cn_trip_light(o),sp),CN_TP)<0){ c=C_RED; tiny=1; } }
                    else { cn_onep_text(t,o); if(sp.m && cs_cmp(cs_div(cn_trip_light(o),CN_TP),sp)<=0) c=C_GREEN; }
                    cd_row(pg,y,nm[k],t,c); y+=10; }
                y+=4;
                if(pgn==3){ text(pg,10,y,"LIGHT YEAR = 9.4607 X 10^12 KM. THE FURTHEST IS A TRIP",C_GREY,1); y+=9; text(pg,10,y,"BETWEEN UNIVERSES: 2^256 OF THEM, EACH 25 X 2^155 LY.",C_GREY,1); }
                else if(pgn==4){ text(pg,10,y,"AT YOUR SPEED RIGHT NOW. SECONDS ARE THE EXACT FIGURE",C_GREY,1); y+=9; text(pg,10,y,"(LIGHT TIME OF THE DISTANCE DIVIDED BY YOUR SPEED).",C_GREY,1); }
                else if(pgn==5){ text(pg,10,y,"ONE PLANCK TIME = 5.391247 X 10^-44 SECONDS, THE REAL",tiny?C_RED:C_GREY,1); y+=9; text(pg,10,y,tiny?"VALUE. RED: LESS THAN ONE, SO A FRACTION OF IT.":"VALUE (CODATA 2018). BELOW IT TIME HAS NO MEANING.",tiny?C_RED:C_GREY,1); }
                else { text(pg,10,y,"THE SPEED (X LIGHT) WHERE THE TRIP TAKES EXACTLY ONE",C_GREY,1); y+=9; text(pg,10,y,"PLANCK TIME. GREEN: YOU ARE AT OR PAST IT.",C_GREY,1); } }
            else if(pgn==7){
                text(pg,10,y,"TIER",C_GREY,1); text(pg,52,y,"SPEED (X LIGHT)",C_GREY,1); rtext(pg,231,y,"ACROSS THE UNIVERSE",C_GREY); y+=10;
                for(int k=1;k<=NCT;k++){ CS ck=cs_from_cw(coil_mw[k]); text(pg,10,y,coil_tn[k],k==coil_tier?C_GOLD:C_WHITE,1); a[0]=0; cn_wfmt(a,coil_mw[k]); text(pg,52,y,a,k==coil_tier?C_GOLD:C_WHITE,1);
                    cn_trip_text(t,CT_UNI,ck); rtext(pg,231,y,t,C_CYAN); y+=10; }
                y+=3; if(coil_limit_s>=COIL_GATE_S || dbg_coil) text(pg,10,y,"STORM IS OPEN. IT IS THE LAST STEP: ONE MORE L+R.",C_GREEN,1);
                else { scpy(a,"STORM OPENS AFTER 30 SECONDS HELD AT LIMIT: "); ncat(a,coil_limit_s); scat(a,"/30"); text(pg,10,y,a,C_GOLD,1); }
                y+=9; text(pg,10,y,"EACH L+R STEPS UP ONE. R OR SELECT STOPS AT ONCE.",C_GREY,1); y+=9;
                scpy(a,"HIGHEST TIER YOU REACHED: "); scat(a,coil_maxtier ? coil_tn[coil_maxtier] : "NONE"); text(pg,10,y,a,C_GREY,1); }
            else {
                static const char *const md[4]={"FULL","REDUCED","VISUAL ONLY","OFF"};
                scpy(a,"THINGS PASSED THROUGH: "); ncat(a,coil_passes); scat(a," ("); ncat(a,coil_pstars); scat(a," STARS, "); ncat(a,coil_pplanets); scat(a," PLANETS)"); text(pg,10,y,a,C_WHITE,1); y+=11;
                scpy(a,"HEAT METER: "); ncat(a,pt_heat); scat(a,"%  (THE COIL DROPS OUT BY ITSELF AT 100%)"); text(pg,10,y,a,pt_heat>=70?C_RED:C_WHITE,1); y+=11;
                scpy(a,"EFFECTS: "); scat(a,md[pt_mode()]); scat(a,", DAMAGE "); ncat(a,pt_dmg()); scat(a,"%, SCREEN "); ncat(a,pt_vis()); scat(a,"%"); text(pg,10,y,a,C_CYAN,1); y+=11;
                static const char *const nt[6]={"PLANETS: A SHOCKWAVE BUMPS SHIELD, THEN HULL.","STARS: HEAT BUILDS. RED GIANTS AND BLUE STARS BURN MOST.","GAS AND DUST: DRAG, A LITTLE FUEL, BLUE STREAKS.","BLACK HOLES: TIDES STRETCH THE PICTURE, TIME SLOWS.","GALAXIES: LIGHT BENDS INTO RINGS (LENSING).","THE HULL NEVER FALLS BELOW 10 FROM ANY OF IT."};
                for(int i=0;i<6;i++){ text(pg,10,y,nt[i],i==5?C_GREEN:C_GREY,1); y+=9; }
                text(pg,10,y+2,"EXTRAS IN OPTIONS CAN REDUCE OR TURN ALL OF IT OFF.",C_GREY,1); }
            ui_lhint(pg,120,146,"A: NEXT PAGE   B: BACK",C_CYAN); } } }
static int coil_detail(volatile u16 *pg){
    int milli; CW w=coil_total_c(speed,ship_top_speed(),&milli);
    int pgn=0, dirty=1; const int NP=9; wait_release(); sfx(7);
    while(1){
        if(dirty){ dirty=0; cd_draw(pg,pgn,w,milli); }
        vsync(); read_keys();
        if(HIT(K_A)||HIT(K_RIGHT)){ pgn=(pgn+1)%NP; dirty=1; sfx(7); }
        if(HIT(K_LEFT)){ pgn=(pgn+NP-1)%NP; dirty=1; sfx(7); }
        if(HIT(K_B)||HIT(K_START)||HIT(K_SEL)) break; }
    wait_release(); return 0; }

