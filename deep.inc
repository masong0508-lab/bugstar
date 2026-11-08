// ================================================================= DEEP SPACE: the long-range chart, the fold drive and the map computer
// After the story the warp drive can reach every cluster of a universe, and the multiverse beyond. The FOLD DRIVE goes further: it folds space to a
// LANDMARK, a place named by how far it is from home. Andromeda, the edge of the seen sky, a googol light years, the googolplex and, at the very end,
// the Bugstarillion (the biggest number in the game, as a distance). The MAP COMPUTER shows more of the chart, and more about each place.
// A landmark is a real place: its stars come from its own number, so you can land, mine and fight there, and fold back to where you left.
// The map says how far things are in four ways (map_units): NAMES, POWERS OF TEN, METRES and PLANCK LENGTHS.

#define DP_N 17
typedef struct { u32 val; u16 lead, nd; u8 exact; const char *ds; } Ex;   // an exponent: its exact value, or (when it has more than 9 digits) its first 3 digits (lead), how many digits it has (nd) and the digits (ds)
typedef struct { const char *nm, *ln; u16 m100; u8 kind; u32 e; } DpL;    // a landmark: name, a line about it, and its distance in light years: (m100/100) x 10^E, where E is e (kind 0), 10^e (kind 1) or the Bugstarillion's (kind 2)
static const char big_e_s[]=BIG_E;                                         // the Bugstarillion's exponent, every digit (186 of them)
static const DpL dpl[DP_N+1]={
    {"HOME","WHERE YOU CAME FROM",100,0,0},
    {"ANDROMEDA","THE NEAREST BIG GALAXY. IT IS COMING OUR WAY",250,0,6},
    {"THE VIRGO CLUSTER","ABOUT 1,300 GALAXIES IN ONE BIG CROWD",540,0,7},
    {"THE GREAT WALL","A SHEET OF GALAXIES 1.4 BILLION LY LONG",140,0,9},
    {"EDGE OF THE SEEN SKY","NO TELESCOPE CAN EVER LOOK PAST HERE",460,0,10},
    {"THE UNSEEN UNIVERSE","AT LEAST 250 TIMES WIDER THAN ALL WE SEE",230,0,13},
    {"UNIVERSE'S EDGE","WHERE A BUGSTAR UNIVERSE WRAPS ROUND",114,0,48},
    {"ONE GOOGOL LY","1 AND 100 ZEROS. SPELT WRONG ON PURPOSE",100,0,100},
    {"MULTIVERSE EDGE","THE FAR WALL OF ALL 2^256 UNIVERSES",132,0,125},
    {"THE STRING LANDSCAPE","WHERE STRING THEORY KEEPS ITS SPARE UNIVERSES",100,0,500},
    {"THE EMPTY BETWEEN","SO FAR OUT THERE IS NOTHING TO MEASURE FROM",100,0,5000},
    {"THE BIGGEST GUESS","A REAL PAPER'S GUESS: 10^272,000 UNIVERSES",100,0,272000},
    {"A MILLION ZEROS OUT","A 1 WITH A MILLION ZEROS AFTER IT",100,0,1000000},
    {"THE SCREEN SECOND","EVERY SECOND OF THE SCREEN, AS A DISTANCE",446,0,10403596},
    {"A BILLION ZEROS OUT","BRING SNACKS. A LOT OF SNACKS",100,0,1000000000},
    {"LINDE'S LAST ONE","10 TO THE 10 TO THE 16 UNIVERSES, END TO END",100,1,16},
    {"THE GOOGOLPLEX","1 AND A GOOGOL OF ZEROS. THE PEN GAVE UP",100,1,100},
    {"THE BUGSTARILLION","THE BIGGEST NUMBER HERE. SEE HOW BIG IS IT?",BIG_SIG,2,0}};
static const char *const unit_nm[4]={"NAMES","POWERS OF TEN","METRES","PLANCK LENGTHS"};
static const u8 dp_reach_t[7]={0,3,6,9,12,15,17};                         // how far the fold drive reaches at each level (the last landmark it can fold to)
static const u8 dp_reveal_t[6]={0,5,8,11,14,17};                          // how much of the chart the map computer shows at each level

// ---- saying a distance
static void dp_ex(const DpL *L, Ex *x){
    x->val=0; x->lead=x->nd=0; x->exact=1; x->ds=0;
    if(L->kind==0){ x->val=L->e; return; }
    if(L->kind==1){ if(L->e<=8){ u32 v=1; for(u32 i=0;i<L->e;i++) v*=10; x->val=v; return; } x->exact=0; x->lead=100; x->nd=(u16)(L->e+1); x->ds="1000"; return; }
    x->exact=0; x->lead=(u16)((big_e_s[0]-'0')*100+(big_e_s[1]-'0')*10+(big_e_s[2]-'0')); x->nd=(u16)(sizeof big_e_s-1); x->ds=big_e_s; }
static void dp_mant(char *o, int m){ char t[8]; int n=0; t[n++]=(char)('0'+m/100); int f=m%100; if(f){ t[n++]='.'; t[n++]=(char)('0'+f/10); if(f%10) t[n++]=(char)('0'+f%10); } t[n]=0; scat(o,t); }   // 446 -> "4.46", 250 -> "2.5", 100 -> "1"
static void dp_expw(char *o, const Ex *x, int compact){                  // the exponent as written after the 10^
    if(x->exact){ if(compact) ncat(o,(int)x->val); else kcomma(o,x->val); return; }
    scat(o,"(");
    if(x->lead==100){ scat(o, compact ? "1E" : "10^"); ncat(o,x->nd-1); }
    else { dp_mant(o,x->lead); scat(o, compact ? "E" : " X 10^"); ncat(o,x->nd-1); }
    scat(o,")"); }
static void dp_strip0(char *t){ for(int i=0;t[i];i++) if(t[i]=='.' && t[i+1]=='0' && t[i+2]==' '){ int j=i; while((t[j]=t[j+2])) j++; return; } }   // "10.0 BILLION" -> "10 BILLION"
static int dist_fmt(char *o, int m100, Ex x, int units, int compact, int cap){   // a distance of (m100/100) x 10^x light years, said as units says, in at most cap letters (cap must hold the short forms, 30). 1 if it fit
    const char *suf=" LY"; o[0]=0;
    if(units>=2){ u32 p=(u32)m100*(units==2?94607u:58535u)/10000u, sh=units==2?15u:50u; if(p>=1000){ p/=10; sh++; } m100=(int)p; if(x.exact) x.val+=sh; suf=units==2?" M":" PLANCKS"; }   // 1 LY = 9.4607 x 10^15 M = 5.8535 x 10^50 Planck lengths
    if(units==0){
        if(x.exact){ u32 g=x.val/3, r=x.val%3, v=(u32)m100; for(u32 i=0;i<r;i++) v*=10;
            ncat(o,(int)(v/100)); if(v%100){ scat(o,"."); ncat(o,(int)(v%100/10)); if(v%10) ncat(o,(int)(v%10)); }
            if(g){ const char *nm=illion((int)g); if(textlen(o)+1+textlen(nm)+3>=cap){ o[0]=0; return 0; } scat(o," "); scat(o,nm); } }
        else { char t[48]; dp_mant(o,m100); scat(o," X 10 TO THE "); digits_named(t,x.ds,x.nd); dp_strip0(t); if(textlen(o)+textlen(t)+3>=cap){ o[0]=0; return 0; } scat(o,t); } }
    else { dp_mant(o,m100); scat(o, compact ? "E" : " X 10^"); dp_expw(o,&x,compact); }
    scat(o,suf); if(textlen(o)>=cap){ o[0]=0; return 0; } return 1; }
static void dist_units(char *out, const char *d, int n){                  // a distance whose decimal digits are d (n of them), said in the chosen units
    Ex x; x.exact=1; x.val=(u32)(n-1); x.lead=x.nd=0; x.ds=0; int m=0; for(int i=0;i<3;i++) m=m*10+(i<n?d[i]-'0':0); dist_fmt(out,m,x,map_units,0,40); }
static void dp_dist_text(char *o, int ix, int units, int maxlen){         // landmark ix's distance, in at most maxlen letters (o holds maxlen+1): in these units if it fits, else shorter
    const DpL *L=&dpl[ix]; Ex x; dp_ex(L,&x); int cap=maxlen+1;
    if(units==0 && dist_fmt(o,L->m100,x,0,0,cap)) return;
    if(units<=1 && dist_fmt(o,L->m100,x,1,0,cap)) return;
    if(units>=2 && dist_fmt(o,L->m100,x,units,0,cap)) return;
    if(!dist_fmt(o,L->m100,x,units?units:1,1,cap)){ o[0]=0; } }                // (the short forms are 30 letters at most, so maxlen is never under 30 in a tight spot)
static void dp_home_text(char *out){ dp_dist_text(out,dp_ix,map_units,24); }   // (out holds 40)
static void dp_here_name(char *b){ scpy(b,"DEEP SPACE: "); scat(b,dpl[dp_ix].nm); b[38]=0; }
static Ex ex_scale(Ex x, int q){                                           // the exponent q/256 of the way (for the counter that runs up on a voyage)
    if(x.exact){ x.val=(u32)((u64)x.val*(u32)q>>8); return x; }
    u32 l=(u32)x.lead*(u32)q>>8; int nd=x.nd; if(l==0){ x.exact=1; x.val=0; return x; } while(l<100){ l*=10; nd--; } x.lead=(u16)l; x.nd=(u16)nd; return x; }

// ---- the rules: what a fold costs, who can go where
static u32 dp_seen=0;                                                     // bit n: you have been to landmark n
EW static u32 dp_rc[2*UAX], dp_ru[UW]; static int dp_ret_ok=0, dp_rsys=0;   // where you were before the first fold (so you can fold back)
static int dp_cost(int ix){ return ix ? 60+8*ix : 40; }                   // fuel (tenths of a light year)
static u32 dp_bonus(int ix){ return 400u*(u32)ix*(u32)ix; }               // gold for the first visit
static u32 dp_hours(int ix){ return (u32)(20+ix*40)*24; }
static int dp_known(int ix){ return ix<=dp_reveal_t[map_lv]; }
static int dp_reach(int ix){ return ix<=dp_reach_t[fold_lv]; }
static int dp_map_cost(void){ return map_lv<5 ? 3000*(map_lv+1)*(map_lv+1) : 0; }
static int dp_fold_cost(void){ return fold_lv<6 ? 5000*(fold_lv+1)*(fold_lv+1) : 0; }
static int dp_why(int ix){                                                // why you cannot fold to ix just now: 0 you can, 1 nothing to fold back to, 2 here, 3 the chart cannot see it, 4 the drive cannot reach it, 5 the story, 6 too close to a planet, 7 fuel
    if(ix==0){ if(!dp_ret_ok) return 1; }
    else {
        if(ix==dp_ix) return 2;
        if(!dp_known(ix)) return 3;
        if(!dp_reach(ix)) return 4; }
    if(!ant_king) return 5;
    if(too_close_to_jump()) return 6;
    if(fuel<dp_cost(ix)) return 7;
    return 0; }
static void dp_why_text(int w, int ix, char *b){
    switch(w){
    case 1: scpy(b,"THERE IS NOTHING TO FOLD BACK TO"); break;
    case 2: scpy(b,"YOU ARE HERE"); break;
    case 3: { int k=1; while(k<5 && dp_reveal_t[k]<ix) k++; scpy(b,"THE CHART CANNOT SEE IT. MAP COMPUTER MK"); ncat(b,k); scat(b," SEES IT"); } break;
    case 4: { int k=1; while(k<6 && dp_reach_t[k]<ix) k++; scpy(b,"TOO FAR. IT NEEDS FOLD DRIVE MK"); ncat(b,k); } break;
    case 5: scpy(b,"BEAT THE ANT KING FIRST"); break;
    case 6: scpy(b,"TOO CLOSE TO THE PLANET TO FOLD"); break;
    default: { int c=dp_cost(ix); scpy(b,"NEED "); ncat(b,c/10); scat(b,"."); ncat(b,c%10); scat(b," LY OF FUEL. YOU HAVE "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10); } } }

#ifdef TEST_STACK
extern u32 __bss_end; static void stack_fill(void){ u32 *p=&__bss_end; u32 sp_now; __asm__ volatile("mov %0, sp":"=r"(sp_now)); while((u32)p<sp_now-0x40) *p++=0xA5A5A5A5u; }
static void stack_mark(volatile u16 *pg){ u32 *p=&__bss_end; while(p<(u32*)0x03007E00 && *p==0xA5A5A5A5u) p++; char b[32]; scpy(b,"STACK USED "); ncat(b,(int)(0x03007F00-(u32)p)); text(pg,4,120,b,C_RED,1); }
#endif
// ---- the voyage: the stars stream past while the number of zeros runs up
static const char *const dp_cap[5]={"PAST THE LAST KNOWN STARS","NOT EVEN DARK OUT HERE","THE NUMBERS ARE GETTING TIRED","THE DIGITS CANNOT KEEP UP","MATHS ITSELF IS FALLING BEHIND"};
static int dp_tier(int ix){ return ix<=4?0 : ix<=8?1 : ix<=12?2 : ix<=15?3 : 4; }
static void dp_log(int fx, u32 hours){                                    // the travel log: the distance if it fits in it, else only the trip and the time
    u32 t[LYW]; for(int i=0;i<LYW;i++) t[i]=0; const DpL *L=&dpl[fx];
    if(fx && !L->kind && L->e>=1 && L->e<=120){ t[0]=L->m100; for(u32 i=1;i<L->e;i++) big_mul(t,LYW,10); }
    tv_log(t,hours); }
static void deep_voyage(const char *from, const char *to, int fx){
    shd_off();
    const DpL *L=&dpl[fx]; Ex ex; dp_ex(L,&ex); int T=180+fx*8; u32 hours=dp_hours(fx);
    char tot[48]; dp_dist_text(tot,fx,map_units,32); dp_log(fx,hours);
    s16 *sa=vsa, *sr=vsr, *sv=vsv; lseed=amix(gsk(114),(u32)frames); for(int i=0;i<40;i++){ sa[i]=(s16)lr(256); sr[i]=(s16)(lr(120)<<4); sv[i]=(s16)(10+lr(26)); }
    const char *cap=dp_cap[dp_tier(fx)]; int cap_t=140;
    sfx(3); int prog=0, rot=0; u32 last=vbl_count;
    while(prog<T*256){
        u32 nv=vbl_count; int dv=(int)(nv-last); last=nv; if(dv<1) dv=1; if(dv>4) dv=4; read_keys();
        prog += dv*256*(HELD(K_B)?3:1); if(prog>T*256) prog=T*256;
        int p=prog/T, sp = p<36 ? p*7 : p>214 ? (256-p)*6 : 256; if(sp<12) sp=12; if(sp>256) sp=256;
        int q=(int)((s32)p*p*(768-2*p)>>16);
        volatile u16 *pg=(volatile u16*)(0x06000000 + (page_back?0xA000:0));
        for(int i=0;i<120*128;i++) pg[i]=0;
        rot+=dv;
        for(int i=0;i<40;i++){ int r0=sr[i]; sr[i]=(s16)(sr[i]+sv[i]*sp*dv/64); if(sr[i]>(150<<4)){ sr[i]=(s16)(lr(24)<<4); sa[i]=(s16)lr(256); r0=sr[i]; }
            int len=sv[i]*sp/160+1; int ra=(sr[i]>>4)-len; if(ra<(r0>>4)) ra=r0>>4; int rb=sr[i]>>4; s32 ca=sinx((sa[i]+64)&255), sn=sinx(sa[i]&255);
            line2(pg,120+(ca*ra>>14),VCY+(sn*ra*3>>16),120+(ca*rb>>14),VCY+(sn*rb*3>>16), i%3==0?55:i%3==1?54:49); }
        if(fx>=2){ for(int g=0;g<3;g++){ int ga=(g*85+rot/3)&255, gr=((rot*(2+g)+g*60)%150); int gx=120+(sinx((ga+64)&255)*gr>>14), gy=VCY+(sinx(ga)*gr*3>>16), sz=2+gr/9;   // whole galaxies rush past
            for(int arm=0;arm<2;arm++) for(int t=1;t<15;t++){ int an=(rot*2+arm*128+t*12+g*40)&255, rr=t*sz/14; pix(pg,gx+(sinx((an+64)&255)*rr>>14),gy+(sinx(an)*rr>>15),t<4?49:(t&1)?50:54); } pix(pg,gx,gy,49); pix(pg,gx+1,gy,49); } }
        if(p>196){ int R=(p-196)*30/60; for(int y=-R;y<=R;y++){ int w=isqrt((u32)(R*R-y*y)); int yy=VCY+y; if(yy<0||yy>127) continue; int x0=120-w, x1=120+w; if(x0<0) x0=0; if(x1>239) x1=239; for(int x=x0;x<=x1;x++) pix(pg,x,yy,(x-120)*(x-120)+y*y < R*R/3 ? 49 : 50); } }   // the place ahead swells up
        if(p>250) for(int i=0;i<120*128;i++) pg[i]=54|(54<<8);
        { char b[48]; scpy(b,"FOLD: "); scat(b,tot); b[44]=0; text(pg,4,3,b,C_GOLD,1); ui_lhint(pg,236-9*4,11,"B: FASTER",C_DGREY); }
        if(p>190) cap_t=0;
        if(cap_t>0){ cap_t-=dv; ctext(pg,120,44,cap,C_WHITE); }
        else if(p<=190){ ctext(pg,120,44,L->ln,C_CYAN); }
        for(int i=128*120;i<160*120;i++) pg[i]=57|(57<<8); for(int i=128*120;i<129*120;i++) pg[i]=58|(58<<8);   // the dashboard: from, to, how far, how long
        { char b[48]; scpy(b,"FROM "); scat(b,from); b[28]=0; text(pg,4,131,b,C_GREY,1); scpy(b,"TO "); scat(b,to); b[28]=0; text(pg,236-textlen(b)*4,131,b,C_GOLD,1); }
        rect(pg,4,141,232,4,56); rect(pg,4,141,232*q/256,4,55); { int sx=4+232*q/256; rect(pg,sx-1,139,3,8,54); }
        { char b[72], c[64]; scpy(b,"GONE ");
            if(p>250 || q>=256) dp_dist_text(c,fx,map_units,36); else { Ex s=ex_scale(ex,q); scpy(c,"10^"); dp_expw(c,&s,0); scat(c," LY"); }     // a counter of zeros: 10^ and the exponent so far
            scat(b,c); b[48]=0; text(pg,4,150,b,C_WHITE,1);
            u32 hh=(u32)((u64)hours*(u32)q/256u); scpy(b,"CLOCK "); hours_fmt(c,hh); scat(b,c); text(pg,236-textlen(b)*4,150,b,C_CYAN,1); }
#ifdef TEST_STACK
        stack_mark(pg);
#endif
        vsync(); REG_DISPCNT = (REG_DISPCNT & ~(1<<4)) | (page_back?(1<<4):0); page_back^=1;
    }
}

// ---- folding
static void dp_place(int ix){                                             // a landmark's own place in the multiverse (the same one every time)
    u32 a[UAX], b[UAX], u[UW], h=hmix(0xDEE9F01Du^((u32)ix*0x9E3779B1u));
    for(int w=0;w<UAX;w++){ h=hmix(h+0x632BE5ABu); a[w]=h; h=hmix(h+0x1B873593u); b[w]=h; } for(int w=0;w<UW;w++){ h=hmix(h+0x7F4A7C15u); u[w]=h; }
    a[UAX-1]&=UTOP; b[UAX-1]&=UTOP; ax_put(0,a); ax_put(1,b); uni_set(u); }
static int dp_fold(int ix){                                               // fold to landmark ix (0: back to where you left). 1 if you went
    if(dp_why(ix)) return 0;
    char fn[28], tn[28]; int fx = ix>dp_ix ? ix : dp_ix; if(fx<1) fx=1;
    if(dp_ix) scpy(fn,dpl[dp_ix].nm); else sys_name(cur_sys,fn); fn[27]=0;
    if(ix) scpy(tn,dpl[ix].nm); else scpy(tn,"WHERE YOU LEFT");
    fuel-=xo_fuel(dp_cost(ix));
    if(ix){ if(!dp_ix && !dp_ret_ok){ for(int w=0;w<2*UAX;w++) dp_rc[w]=clus[w]; for(int w=0;w<UW;w++) dp_ru[w]=uni_w[w]; dp_rsys=cur_sys; dp_ret_ok=1; }
        dp_place(ix); dp_ix=ix; }
    else { for(int w=0;w<2*UAX;w++) clus[w]=dp_rc[w]; clus[UAX-1]&=UTOP; clus[2*UAX-1]&=UTOP; uni_set(dp_ru); dp_ix=0; dp_ret_ok=0; }
    u_settle(); gen_cluster(); cur_pl=cur_moon=0; visits++;
    cur_sys = ix ? 1+(int)(hmix(0x51A7u^(u32)ix*977u)%(u32)(NSYS-1)) : (dp_rsys<NSYS ? dp_rsys : 0);
    deep_voyage(fn,tn,fx);
    space_enter(0);
    if(ix && !((dp_seen>>ix)&1)){ dp_seen|=1u<<ix; u32 bo=dp_bonus(ix); gold+=bo; char b[48]; scpy(b,"FIRST VISIT! +G"); ncat(b,(int)bo); space_say(b); }
    else { char b[48]; scpy(b,ix ? "ARRIVED: " : "BACK: "); if(ix) scat(b,dpl[ix].nm); else { char nm[24]; sys_name(cur_sys,nm); scat(b,nm); } b[39]=0; space_say(b); }
    return 1; }

// ---- the long-range chart
static void dp_text2(volatile u16 *pg, int x, int y, int y2, char *s, int w, int col){   // one line, or two if it is longer than w letters (broken at a space, or after an ILLI)
    int L=textlen(s); if(L<=w){ text(pg,x,y,s,col,1); return; }
    int cut=0; for(int i=w;i>0;i--) if(s[i]==' '){ cut=i; break; }
    if(cut){ char ch=s[cut]; s[cut]=0; text(pg,x,y,s,col,1); s[cut]=ch; text(pg,x,y2,s+cut+1,col,1); return; }
    cut=illi_cut(s,w-1); if(!cut) cut=w-1; char ch=s[cut]; s[cut]=0; text(pg,x,y,s,col,1); text(pg,x+cut*4,y,"-",col,1); s[cut]=ch; text(pg,x,y2,s+cut,col,1); }
static int deep_chart_(volatile u16 *pg);
static int deep_chart(volatile u16 *pg){ int sh=shd_begin(); int r=deep_chart_(sh?VISPG:pg); shd_end(sh); return r; }
static int deep_chart_(volatile u16 *pg){                                  // the chart: pick a place and fold there. 1 if you went
    u8 rows[DP_N+1]; int nr=0; if(dp_ret_ok) rows[nr++]=0; for(int i=1;i<=DP_N;i++) rows[nr++]=(u8)i;
    int sel=0, top=0, dirty=1;
    for(int r=0;r<nr;r++){ int ix=rows[r]; if(ix && !((dp_seen>>ix)&1) && ix!=dp_ix && dp_known(ix) && dp_reach(ix)){ sel=r; break; } }
    wait_release();
#ifdef TEST_STACK
    stack_fill();
#endif
    while(1){
        if(sel<top) top=sel; if(sel>=top+7) top=sel-6;
        if(dirty){ dirty=0; char c[100];
            for(int i=0;i<240*160/2;i++) pg[i]=0;
            text(pg,4,3,"LONG-RANGE CHART",C_GOLD,2);
            scpy(c,"MAP MK"); ncat(c,map_lv); scat(c,"  FOLD MK"); ncat(c,fold_lv); rtext(pg,236,5,c,C_CYAN);
            scpy(c,"FUEL "); ncat(c,fuel/10); scat(c,"."); ncat(c,fuel%10); scat(c," LY   DISTANCES IN "); scat(c,unit_nm[map_units]); text(pg,4,19,c,C_GREY,1);
            for(int r=top;r<nr && r<top+7;r++){ int ix=rows[r], y=29+(r-top)*9; if(r==sel) rect(pg,0,y-1,240,9,58);
                int known = ix==0 || dp_known(ix);
                int col = ix==0 ? C_GREEN : ix==dp_ix ? C_CYAN : !known ? C_DGREY : dp_reach(ix) ? C_WHITE : C_GREY;
                if(ix && ix==dp_ix) text(pg,3,y,">",C_CYAN,1); else if(ix && ((dp_seen>>ix)&1)) text(pg,3,y,"+",C_GREEN,1);
                if(ix==0){ text(pg,14,y,"FOLD BACK",col,1); rtext(pg,236,y,"TO WHERE YOU LEFT",C_GREY); }
                else if(!known){ text(pg,14,y,"? ? ?",col,1); }
                else { text(pg,14,y,dpl[ix].nm,col,1); dp_dist_text(c,ix,map_units,34); rtext(pg,236,y,c,dp_reach(ix)?C_GREY:C_DGREY); } }
            if(nr>7){ text(pg,232,28,"^",top>0?C_WHITE:C_DGREY,1); text(pg,232,86,"v",top+7<nr?C_WHITE:C_DGREY,1); }
            rect(pg,0,94,240,1,58);
            { int ix=rows[sel];
              if(ix==0){ ctext(pg,120,100,"YOUR OWN SPACE, WHERE YOU WERE BEFORE YOU FOLDED",C_WHITE); ctext(pg,120,110,"FOLDING BACK COSTS 4.0 LY OF FUEL",C_GREY); }
              else if(!dp_known(ix)){ ctext(pg,120,100,"THE CHART DOES NOT SHOW THIS PLACE YET",C_GREY); }
              else { dp_dist_text(c,ix,map_units,99); dp_text2(pg,4,97,105,c,56,C_WHITE);
                  text(pg,4,114,dpl[ix].ln,C_CYAN,1);
                  scpy(c,"FOLD "); ncat(c,dp_cost(ix)/10); scat(c,"."); ncat(c,dp_cost(ix)%10); scat(c," LY OF FUEL");
                  if(map_lv>=2){ char h[24]; hours_fmt(h,dp_hours(ix)); scat(c,"   CLOCK "); scat(c,h); }
                  if(map_lv>=3){ scat(c,"   DANGER "); ncat(c,46+ix); scat(c,"+"); }
                  text(pg,4,123,c,C_GREY,1);
                  if(map_lv>=4){ if((dp_seen>>ix)&1) scpy(c,"YOU HAVE BEEN HERE"); else { scpy(c,"FIRST VISIT PAYS G"); ncat(c,(int)dp_bonus(ix)); } text(pg,4,131,c,(dp_seen>>ix)&1?C_DGREY:C_GOLD,1); } } }
            { int w=dp_why(rows[sel]); if(w){ dp_why_text(w,rows[sel],c); text(pg,4,140,c,w==2?C_CYAN:C_RED,1); } else text(pg,4,140,rows[sel]?"A: FOLD THERE":"A: FOLD BACK",C_GREEN,1); }
            ui_chint(pg,120,150,"A: FOLD   SELECT: UNITS   B: BACK",C_CYAN);
#ifdef TEST_STACK
            stack_mark(pg);
#endif
        }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+nr-1)%nr; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%nr; dirty=1; sfx(7); }
        if(HIT(K_L)){ sel-=7; if(sel<0) sel=0; dirty=1; sfx(7); } if(HIT(K_R)){ sel+=7; if(sel>=nr) sel=nr-1; dirty=1; sfx(7); }
        if(HIT(K_SEL)){ map_units=(map_units+1)&3; dirty=1; sfx(7); }
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HIT(K_A)){ int ix=rows[sel]; if(dp_why(ix)){ sfx(12); } else if(dp_fold(ix)){ wait_release(); return 1; } }
    }
}

// ---- the station's NAV tab: buy the map computer and the fold drive
static void dp_nav_draw(volatile u16 *pg, int sel){
    static const char *const nav[4]={"MAP COMPUTER","FOLD DRIVE","LONG-RANGE CHART","DISTANCE UNITS"};
    char b[64]; int mc=dp_map_cost(), fc=dp_fold_cost();
    for(int i=0;i<4;i++){ int yy=26+i*10; if(i==sel) text(pg,6,yy,">",C_CYAN,1); scpy(b,nav[i]);
        if(i==0 && map_lv<5){ scat(b," MK"); ncat(b,map_lv+1); }
        if(i==1 && fold_lv<6){ scat(b," MK"); ncat(b,fold_lv+1); }
        while(textlen(b)<22) scat(b," ");
        if(i==0){ if(mc){ ncat(b,mc); scat(b,"G"); } else scat(b,"MAX"); }
        else if(i==1){ if(fc){ ncat(b,fc); scat(b,"G"); } else scat(b,"MAX"); }
        else if(i==2) scat(b,"OPEN");
        else scat(b,unit_nm[map_units]);
        text(pg,14,yy,b,i==sel?C_WHITE:C_GREY,1); }
    scpy(b,"MAP COMPUTER MK"); ncat(b,map_lv); scat(b," SEES UP TO "); scat(b,map_lv ? dpl[dp_reveal_t[map_lv]].nm : "NOTHING YET"); text(pg,14,70,b,C_CYAN,1);
    scpy(b,"FOLD DRIVE MK"); ncat(b,fold_lv); scat(b," FOLDS TO "); scat(b,fold_lv ? dpl[dp_reach_t[fold_lv]].nm : "NOWHERE YET"); text(pg,14,78,b,C_CYAN,1);
    scpy(b,"WARP DRIVE MK"); ncat(b,warp_lv); scat(b,"   FUEL TANK ");  ncat(b,fuel_tank); scat(b,"   FUEL "); ncat(b,fuel/10); scat(b,"."); ncat(b,fuel%10); scat(b,"/"); ncat(b,fuel_max()/10); text(pg,14,86,b,C_GREY,1);
    rect(pg,4,96,232,1,58);
    if(sel==0){
        if(map_lv<5){ scpy(b,"NEXT: THE CHART SEES UP TO "); scat(b,dpl[dp_reveal_t[map_lv+1]].nm); text(pg,14,102,b,C_WHITE,1);
            text(pg,14,110,map_lv<1 ? "WITHOUT ONE THE CHART IS BLANK" : map_lv<2 ? "MK2 ALSO SHOWS THE SHIP CLOCK" : map_lv<3 ? "MK3 ALSO SHOWS THE DANGER" : "MK4 ALSO SHOWS THE FIRST VISIT GOLD",C_GREY,1); }
        else text(pg,14,102,"THE CHART SHOWS EVERYTHING THERE IS",C_WHITE,1); }
    else if(sel==1){
        if(fold_lv<6){ scpy(b,"NEXT: FOLD AS FAR AS "); scat(b,dpl[dp_reach_t[fold_lv+1]].nm); text(pg,14,102,b,C_WHITE,1);
            scpy(b,"NEEDS WARP DRIVE MK"); ncat(b,fold_lv+1); text(pg,14,110,b,warp_lv>=fold_lv+1?C_GREEN:C_RED,1); }
        else text(pg,14,102,"YOUR FOLD DRIVE REACHES EVERYWHERE",C_WHITE,1);
        text(pg,14,118,"A FOLD COSTS 6.8 LY OF FUEL AND UP",C_GREY,1); }
    else if(sel==2){ text(pg,14,102,"SEE THE FAR PLACES AND FOLD THERE",C_WHITE,1); text(pg,14,110,"YOU CAN ALSO OPEN IT FROM THE MAP (START, THEN L)",C_GREY,1); }
    else { text(pg,14,102,"HOW BIG DISTANCES ARE SAID ON THE MAP:",C_WHITE,1); text(pg,14,110,"NAMES, POWERS OF TEN, METRES OR PLANCK LENGTHS",C_GREY,1); text(pg,14,118,"A: CHANGE IT",C_CYAN,1); }
    ui_lhint(pg,6,148,"A: BUY/OPEN  L/R: TABS  START: LEAVE",C_CYAN);
}
static int dp_nav_press(volatile u16 *pg, int sel){                       // A in the NAV tab. 1 if it folded you away
    if(sel==0){ int c=dp_map_cost(); if(!c) say("YOUR MAP COMPUTER IS AS GOOD AS IT GETS"); else if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; map_lv++; sfx(6); } }
    else if(sel==1){ int c=dp_fold_cost(); if(!c) say("YOUR FOLD DRIVE IS AS GOOD AS IT GETS"); else if(warp_lv<fold_lv+1) say("YOU NEED A BETTER WARP DRIVE FIRST"); else if(gold<c) say("NOT ENOUGH GOLD"); else { gold-=c; fold_lv++; sfx(6); } }
    else if(sel==2) return deep_chart(pg);
    else { map_units=(map_units+1)&3; sfx(7); }
    return 0; }

// ---- the save block (after the travel log): the levels, the setting, the places you have been, and the way back
typedef struct { u32 magic; u8 map_lv, fold_lv, units, has_ret, ix, rsys, pad[2]; u32 seen; u32 rc[2*UAX], ru[UW]; u32 sum; } SaveDp;
#define DP_OFF 0xF60
_Static_assert(__builtin_offsetof(SaveDp,sum)==sizeof(SaveDp)-4, "SaveDp: the sum must be the last 4 bytes");
_Static_assert(DP_OFF+sizeof(SaveDp) <= UH_OFF && DP_OFF >= TV2_OFF+sizeof(SaveTv2), "the far-travel block sits between the travel log and the universe number");
static void dp_defaults(void){ map_lv=fold_lv=map_units=dp_ix=0; dp_seen=0; dp_ret_ok=0; dp_rsys=0; for(int w=0;w<2*UAX;w++) dp_rc[w]=0; for(int w=0;w<UW;w++) dp_ru[w]=0; }
static void dp_save(void){ SaveDp d; memset(&d,0,sizeof d); d.magic=0x31504544u; d.map_lv=(u8)map_lv; d.fold_lv=(u8)fold_lv; d.units=(u8)map_units; d.has_ret=(u8)dp_ret_ok; d.ix=(u8)dp_ix; d.rsys=(u8)dp_rsys; d.seen=dp_seen;
    for(int w=0;w<2*UAX;w++) d.rc[w]=dp_rc[w]; for(int w=0;w<UW;w++) d.ru[w]=dp_ru[w]; d.sum=fnv(&d,sizeof d-4); sram_write(slot_base(cur_slot)+DP_OFF,&d,sizeof d); }
static void dp_load(void){ dp_defaults(); SaveDp d; sram_read(slot_base(cur_slot)+DP_OFF,&d,sizeof d);
    if(d.magic!=0x31504544u || d.sum!=fnv(&d,sizeof d-4)) return;
    map_lv=d.map_lv>5?5:d.map_lv; fold_lv=d.fold_lv>6?6:d.fold_lv; map_units=d.units&3; dp_ret_ok=d.has_ret?1:0; dp_ix=d.ix<=DP_N?d.ix:0; dp_rsys=d.rsys; dp_seen=d.seen&0x3FFFEu;
    for(int w=0;w<2*UAX;w++) dp_rc[w]=d.rc[w]; for(int w=0;w<UW;w++) dp_ru[w]=d.ru[w]; }
