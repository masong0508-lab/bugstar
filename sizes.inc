// ================================================================= HOW BIG IS IT? The game's huge numbers, said out loud
// Each page is one number, written in full with commas, then said group by group by name: 4 BILLION, 294 MILLION, 967 THOUSAND, 296.
// The numbers are worked out here with big whole numbers (16 words: up to 154 digits), so they are exact, not made up.
#define BNW 16
static const char *const bn_name[34]={"","THOUSAND","MILLION","BILLION","TRILLION","QUADRILLION","QUINTILLION","SEXTILLION","SEPTILLION","OCTILLION","NONILLION","DECILLION",
    "UNDECILLION","DUODECILLION","TREDECILLION","QUATTUORDECILLION","QUINDECILLION","SEXDECILLION","SEPTENDECILLION","OCTODECILLION","NOVEMDECILLION","VIGINTILLION",
    "UNVIGINTILLION","DUOVIGINTILLION","TRESVIGINTILLION","QUATTUORVIGINTILLION","QUINVIGINTILLION","SEXVIGINTILLION","SEPTENVIGINTILLION","OCTOVIGINTILLION","NOVEMVIGINTILLION",
    "TRIGINTILLION","UNTRIGINTILLION","DUOTRIGINTILLION"};            // a thousand times apart, the short scale (a billion is a thousand million)
static void bn_set(u32 *b, u32 v){ b[0]=v; for(int i=1;i<BNW;i++) b[i]=0; }
static void bn_pow2(u32 *b, int e){ bn_set(b,0); b[e>>5]=1u<<(e&31); }
static void bn_mul(u32 *b, u32 k){ big_mul(b,BNW,k); }
static int bn_groups(const u32 *v, u16 *g){ u32 t[BNW]; for(int i=0;i<BNW;i++) t[i]=v[i]; int n=0; do { g[n++]=(u16)big_div(t,BNW,1000); } while(!big_zero(t,BNW) && n<52); return n; }   // groups of three digits, the smallest first
typedef struct { const char *title, *note; u32 v[BNW]; char extra[44], extra2[44]; } SizePage;
#define NSIZE 12
static void size_page(int i, SizePage *p){                          // page i, worked out when you turn to it
    static const u8 bug_n[13]={3,5,4,12,8,6,8,3,4,6,6,5,12}, hum_n[11]={2,5,3,10,8,6,10,6,5,12,6};
    p->extra[0]=p->extra2[0]=0;
    switch(i){
    case 0: p->title="PLANETS IN KART WORLD"; p->note="EVERY PLANET NUMBER IS A WHOLE WORLD TO RACE ON"; bn_pow2(p->v,32); break;
    case 1: p->title="TRACKS IN KART WORLD"; p->note="1,000,000 TRACKS ON EVERY ONE OF THOSE PLANETS"; bn_pow2(p->v,32); bn_mul(p->v,1000000); break;
    case 2: p->title="UNIVERSES"; p->note="EVERY NEW GAME GETS A UNIVERSE OF ITS OWN"; bn_pow2(p->v,32); break;
    case 3: p->title="STAR CLUSTERS IN A UNIVERSE"; p->note="IT WRAPS ROUND: FLY FAR ENOUGH AND YOU COME HOME"; bn_pow2(p->v,2*155); break;
    case 4: p->title="STARS IN A UNIVERSE"; p->note="32 STARS IN EVERY CLUSTER"; bn_pow2(p->v,2*155); bn_mul(p->v,32); break;
    case 5: p->title="WORLDS TO LAND ON"; p->note="PLANETS AND MOONS, ABOUT 5.6 FOR EVERY STAR"; bn_pow2(p->v,2*155); bn_mul(p->v,32*563); big_div(p->v,BNW,100); break;
    case 6: p->title="LIGHT YEARS ACROSS IT"; p->note="FROM ONE SIDE OF A UNIVERSE TO THE OTHER"; bn_pow2(p->v,155); bn_mul(p->v,ULY); break;
    case 7: p->title="BUGS YOU CAN MAKE"; p->note="EVERY PART TIMES EVERY COLOUR (NOT EVEN COUNTING NAMES)"; bn_set(p->v,1); for(int k=0;k<13;k++) bn_mul(p->v,bug_n[k]); for(int k=0;k<10;k++) bn_mul(p->v,32768); break;
    case 8: p->title="HUMANOIDS YOU CAN MAKE"; p->note="BODIES, HAIR, CLOTHES, HATS AND 9 COLOURS"; bn_set(p->v,1); for(int k=0;k<11;k++) bn_mul(p->v,hum_n[k]); for(int k=0;k<9;k++) bn_mul(p->v,32768); break;
    case 9: p->title="KARTS YOU CAN BUILD"; p->note="EVERY GARAGE PART, 3 COLOURS AND A NUMBER"; bn_set(p->v,100); for(int k=0;k<NKPART;k++) bn_mul(p->v,kpart_n[k]); for(int k=0;k<3;k++) bn_mul(p->v,32768); break;
    case 10: p->title="THE TECHNOBEAST'S HEALTH"; p->note="AND ITS WOUNDS NEVER HEAL"; bn_set(p->v,1000000); bn_mul(p->v,1000000); break;
    default: p->title="LIGHT YEARS YOU HAVE FLOWN"; p->note="IN THIS SAVE, SO FAR"; { u32 t[6]; for(int k=0;k<6;k++) t[k]=tv_ly[k]; big_div(t,6,10); bn_set(p->v,0); for(int k=0;k<6;k++) p->v[k]=t[k]; }
        scpy(p->extra,""); ncat(p->extra,(int)tv_trips); scat(p->extra,tv_trips==1?" TRIP. LONGEST: ":" TRIPS. LONGEST: "); { char d[40]; ly_fmt(d,tv_best); d[24]=0; scat(p->extra,d); }
        { char h[24]; hours_fmt(h,tv_hours); scpy(p->extra2,"TIME BETWEEN THE STARS: "); scat(p->extra2,h); } }
}
static void size_screen(volatile u16 *pg, int from_title){          // LEFT/RIGHT: the next number. UP/DOWN: scroll a long one. B: back
    if(from_title) tv_load();                                          // (in a game the travel log in memory is the newest)
    int pi=0, top=0, dirty=1, last=-1, ng=0; SizePage pgd; u16 g[52]; wait_release();
    while(1){
        SizePage *p=&pgd; if(pi!=last){ size_page(pi,p); ng=bn_groups(p->v,g); last=pi; }   // (worked out once per page: the long division is slow)
        int nz=0; for(int k=ng-1;k>=0;k--) if(g[k]) nz++;                                   // the groups that are not zero: one line each
        int rows_vis=11-(p->extra[0]?2:0), cols = nz>rows_vis ? 2 : 1, per=rows_vis*cols, maxtop = nz>per ? nz-per : 0; if(top>maxtop) top=maxtop;
        if(dirty){ dirty=0; char b[64];
            rect(pg,0,0,240,160,0); for(int k=0;k<40;k++) pix(pg,(k*53+11)%240,(k*29+7)%160,(k&3)?C_DGREY:C_GREY);
            rect(pg,0,0,240,16,1); rect(pg,0,16,240,1,6); text(pg,4,4,"HOW BIG IS IT?",C_GOLD,1); scpy(b,""); ncat(b,pi+1); scat(b," / "); ncat(b,NSIZE); text(pg,236-textlen(b)*4,4,b,C_GREY,1);
            int L=textlen(p->title); if(L<=28) text(pg,120-L*4,22,p->title,C_GOLD,2); else ctext(pg,120,24,p->title,C_GOLD);
            ctext(pg,120,34,p->note,C_GREY);
            { char line[64]; int ll=0, row=0, digits=0; line[0]=0;                              // the whole number with commas, broken between groups
              for(int k=ng-1;k>=0;k--){ char t[8]; int v=g[k]; if(k==ng-1){ scpy(t,""); ncat(t,v); } else { t[0]=(char)('0'+v/100); t[1]=(char)('0'+v/10%10); t[2]=(char)('0'+v%10); t[3]=0; }
                  digits+=textlen(t); if(k) scat(t,",");
                  if(ll+textlen(t)>56 && row<2){ ctext(pg,120,44+row*8,line,C_WHITE); row++; ll=0; line[0]=0; }
                  if(ll+textlen(t)<=60){ scat(line,t); ll+=textlen(t); } }
              ctext(pg,120,44+row*8,line,C_WHITE);
              { char b2[32]; scpy(b2,""); ncat(b2,digits); scat(b2,digits==1?" DIGIT":" DIGITS"); ctext(pg,132,4,b2,C_CYAN); } }   // how long the number is, up in the bar
            int y0=72, k=0, shown=0; char ln[44];
            for(int gi=ng-1;gi>=0;gi--){ if(!g[gi]) continue; if(k++<top) continue; if(shown>=per) break;            // said out loud: a group per line
                scpy(ln,""); ncat(ln,g[gi]); if(gi){ scat(ln," "); scat(ln,bn_name[gi<34?gi:33]); }
                int col=shown/rows_vis, row=shown%rows_vis, x = cols==1 ? 120-textlen(ln)*2 : 6+col*118;
                text(pg,x,y0+row*7,ln,(shown&1)?C_CYAN:C_WHITE,1); shown++; }
            if(top>0) text(pg,228,70,"^",C_CYAN,1); if(top<maxtop) text(pg,228,y0+rows_vis*7-6,"V",C_CYAN,1);
            if(p->extra[0]){ ctext(pg,120,y0+rows_vis*7+2,p->extra,C_GOLD); ctext(pg,120,y0+rows_vis*7+10,p->extra2,C_GOLD); }
            rect(pg,0,150,240,10,1); ctext(pg,120,152, maxtop ? "LEFT/RIGHT: MORE   UP/DOWN: SCROLL   B: BACK" : "LEFT/RIGHT: MORE   B: BACK",C_CYAN); }
        vsync(); read_keys();
        if(HIT(K_RIGHT)||HIT(K_A)){ pi=(pi+1)%NSIZE; top=0; dirty=1; sfx(7); }
        if(HIT(K_LEFT)){ pi=(pi+NSIZE-1)%NSIZE; top=0; dirty=1; sfx(7); }
        if(HIT(K_DOWN) && top<maxtop){ top+=cols; if(top>maxtop) top=maxtop; dirty=1; sfx(7); }
        if(HIT(K_UP) && top>0){ top-=cols; if(top<0) top=0; dirty=1; sfx(7); }
        if(HIT(K_B)||HIT(K_START)) break; }
    wait_release(); }
