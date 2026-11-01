// ================================================================= THE STORMCLOCK COIL: wide numbers (no float, no 128-bit, nothing here can overflow)
// Speeds are written as multiples of c. Below 10^19 the figure is an exact whole number (CW with e = 0). From 10^19 up it is a 10-digit mantissa and a
// decimal exponent: value = m x 10^e with 1,000,000,000 <= m < 10,000,000,000 (and e >= 10), so it holds any speed the game can reach and every sum,
// product or quotient it needs, always with the same 10 digits. The STORM tier (2.3759 x 10^43 c) is exactly 23759 x 10^39.
// CS is the same idea for any positive number (also below 1): m (10 digits) x 10^e, e may be negative, or m = 0. Every derived figure uses it: km per
// second, stars passed per second, trip times down to the Planck time. The text makers at the end round half up, trim trailing zeros and are the ones
// pc/tests/coilref.py (a Python reference with exact decimal maths) is compared with.
typedef struct { u64 m; int e; } CW;                                   // a speed in whole c: exact below 10^19, else 10 digits x 10^e
typedef struct { u64 m; int e; } CS;                                   // any positive number: 10 digits x 10^e, or zero
static const u64 cn_p10[20]={1ULL,10ULL,100ULL,1000ULL,10000ULL,100000ULL,1000000ULL,10000000ULL,100000000ULL,1000000000ULL,10000000000ULL,100000000000ULL,
    1000000000000ULL,10000000000000ULL,100000000000000ULL,1000000000000000ULL,10000000000000000ULL,100000000000000000ULL,1000000000000000000ULL,10000000000000000000ULL};
static const char *illion(int g);                                      // (space.inc: MILLION, BILLION... for any size)
static int cn_dig(u64 m){ int n=0; while(m){ n++; m/=10; } return n; }
// ---------------------------------------------------------------- speeds (CW)
static CW cw_make(u64 m, int e){                                       // the one canonical form, so equal speeds always compare equal
    CW r; if(m==0){ r.m=0; r.e=0; return r; }
    int d=cn_dig(m);
    if(d+e<=19){ while(e>0){ m*=10; e--; } r.m=m; r.e=0; return r; }
    while(d>10){ m/=10; d--; e++; } while(d<10){ m*=10; d++; e--; }
    r.m=m; r.e=e; return r; }
static CW cw_u64(u64 v){ return cw_make(v,0); }
static int cw_zero(CW a){ return a.m==0; }
static int cw_cmp(CW a, CW b){ if(a.e!=b.e) return a.e<b.e ? -1 : 1; return a.m<b.m ? -1 : a.m>b.m ? 1 : 0; }
static CW cw_mulu(CW a, u32 k){                                        // k up to 1,000,000,000
    if(!k || !a.m) return cw_make(0,0);
    if(a.e==0){ if(a.m <= 0xFFFFFFFFFFFFFFFFULL/k) return cw_make(a.m*k,0);
        u64 m=a.m; int d=cn_dig(m), e=0; while(d>10){ m/=10; d--; e++; } return cw_make(m*k,e); }
    return cw_make(a.m*k,a.e); }
static CW cw_divu(CW a, u32 k){                                        // k from 1 to 1,000,000,000
    if(!k) k=1;
    if(a.e==0) return cw_make(a.m/k,0);
    return cw_make(a.m*1000000000ULL/k, a.e-9); }
static CW cw_add(CW a, CW b){
    if(a.e<b.e){ CW t=a; a=b; b=t; }
    if(a.e==0){ u64 s=a.m+b.m; if(s>=a.m) return cw_make(s,0);
        return cw_make(a.m/10+b.m/10+(a.m%10+b.m%10)/10, 1); }
    int d=a.e-b.e; u64 bm = d>19 ? 0 : b.m/cn_p10[d];
    return cw_make(a.m+bm,a.e); }
static CW cw_sub(CW a, CW b){                                          // a - b, never below zero
    if(cw_cmp(a,b)<=0) return cw_make(0,0);
    if(a.e==0) return cw_make(a.m-b.m,0);
    int d=a.e-b.e; u64 bm = d>19 ? 0 : b.m/cn_p10[d];
    return cw_make(a.m-bm,a.e); }
static int cw_decades(CW a){ return a.m==0 ? 0 : cn_dig(a.m)-1+a.e; }   // log10 rounded down (0 for zero and 1 to 9)
// ---------------------------------------------------------------- derived figures (CS)
static CS cs_make(u64 m, int e){
    CS r; if(m==0){ r.m=0; r.e=0; return r; }
    int d=cn_dig(m); while(d>10){ m/=10; d--; e++; } while(d<10){ m*=10; d++; e--; }
    r.m=m; r.e=e; return r; }
static CS cs_u(u64 v){ return cs_make(v,0); }
static CS cs_from_cw(CW a){ return cs_make(a.m,a.e); }
static int cs_cmp(CS a, CS b){
    if(!a.m || !b.m) return a.m==b.m ? 0 : a.m ? 1 : -1;
    if(a.e!=b.e) return a.e<b.e ? -1 : 1;
    return a.m<b.m ? -1 : a.m>b.m ? 1 : 0; }
static CS cs_mulu(CS a, u32 k){ return cs_make(a.m*k, a.e); }          // k up to 1,000,000,000
static CS cs_divu(CS a, u32 k){ if(!k) k=1; return cs_make(a.m*1000000000ULL/k, a.e-9); }
static CS cs_div(CS a, CS b){                                          // a / b with 9 or 10 digits (b must not be zero)
    if(!a.m || !b.m) return cs_make(0,0);
    u64 q1=a.m/b.m, rem=a.m%b.m; u64 q=q1*1000000000ULL + rem*1000000000ULL/b.m;
    return cs_make(q, a.e-b.e-9); }
static CS cs_mul(CS a, CS b){ if(!a.m || !b.m) return cs_make(0,0); return cs_make((a.m/10)*(b.m/10), a.e+b.e+2); }
static int cw_ratio256(CW cur, CW tg){                                 // cur / tg in 256ths (0 to 256)
    if(cw_cmp(cur,tg)>=0) return 256;
    if(cw_zero(cur) || cw_zero(tg)) return 0;
    CS q=cs_div(cs_from_cw(cur),cs_from_cw(tg)); if(!q.m || q.e>=0) return 0; if(-q.e>19) return 0;
    return (int)(q.m*256/cn_p10[-q.e]); }
// ---------------------------------------------------------------- text (all of it appends to b)
static void cn_u(char *b, u64 v){ char t[24]; int n=0; do { t[n++]=(char)('0'+v%10); v/=10; } while(v); char r[24]; for(int i=0;i<n;i++) r[i]=t[n-1-i]; r[n]=0; scat(b,r); }
static void cn_comma(char *b, u64 v){ char d[24]; d[0]=0; cn_u(d,v); int n=textlen(d); char o[32]; int k=0; for(int i=0;i<n;i++){ if(i && (n-i)%3==0) o[k++]=','; o[k++]=d[i]; } o[k]=0; scat(b,o); }
static void cs_round(CS v, int sig, u64 *r, int *X){                   // the first 'sig' digits, rounded half up; X = the decimal exponent of the first digit
    u64 d=cn_p10[10-sig], q=(v.m+d/2)/d; int x=v.e+9; if(q>=cn_p10[sig]){ q/=10; x++; } *r=q; *X=x; }
static void cn_sci(char *b, u64 r, int sig, int X){                    // "2.3759 X 10^43"
    char d[24]; d[0]=0; cn_u(d,r); int n=sig; while(n>1 && d[n-1]=='0') n--;
    char t[24]; int k=0; t[k++]=d[0]; if(n>1){ t[k++]='.'; for(int i=1;i<n;i++) t[k++]=d[i]; } t[k]=0; scat(b,t); scat(b," X 10^");
    if(X<0){ scat(b,"-"); cn_u(b,(u64)(-X)); } else cn_u(b,(u64)X); }
static void cn_dec(char *b, u64 r, int sig, int s){                    // r has 'sig' digits; the point goes after s+1 of them; trailing zeros of the fraction go
    char d[24]; d[0]=0; cn_u(d,r); int n=sig; char t[28]; int k=0;
    for(int i=0;i<=s;i++) t[k++] = i<n ? d[i] : '0';
    int f=n; while(f>s+1 && d[f-1]=='0') f--;
    if(f>s+1){ t[k++]='.'; for(int i=s+1;i<f;i++) t[k++]=d[i]; } t[k]=0; scat(b,t); }
static void cn_wfmt(char *b, CW w){                                    // a speed, exact: "950", "13,000,000,000,000", "2.3759 X 10^43"
    if(w.e==0) cn_comma(b,w.m); else cn_sci(b,w.m,10,w.e+9); }
static void cn_sn(char *b, CS v){                                      // a figure: 299,792,458  23.527  0.0165  3.8973 X 10^18
    if(!v.m){ scat(b,"0"); return; }
    u64 r; int x; cs_round(v,5,&r,&x); int X=v.e+9;
    if(x>5 || x<-3){ cn_sci(b,r,5,x); return; }
    if(x>=4){ u64 d=cn_p10[9-X]; cn_comma(b,(v.m+d/2)/d); return; }
    if(x>=0){ cn_dec(b,r,5,x); return; }
    scat(b,"0."); for(int i=0;i< -x-1;i++) scat(b,"0");
    char d[24]; d[0]=0; cn_u(d,r); int n=5; while(n>1 && d[n-1]=='0') n--; d[n]=0; scat(b,d); }
static void cn_words(char *b, CS v){                                   // "13 TRILLION", "23.759 TREDECILLION"; small figures as numbers
    u64 r; int x; cs_round(v,5,&r,&x);
    if(!v.m || x<6){ cn_sn(b,v); return; }
    cn_dec(b,r,5,x%3); scat(b," "); scat(b,illion(x/3)); }
// ---------------------------------------------------------------- time: seconds in, friendly words out, all the way down to the Planck time
static const char *const cn_su[9]={"SECOND","MILLISECOND","MICROSECOND","NANOSECOND","PICOSECOND","FEMTOSECOND","ATTOSECOND","ZEPTOSECOND","YOCTOSECOND"};
#define CN_TP cs_make(5391247000ULL,-53)                               // the Planck time, 5.391247 x 10^-44 s (CODATA 2018)
static void cn_unit(char *b, const char *u, int one){ scat(b," "); scat(b,u); if(!one) scat(b,"S"); }
static void cn_time(char *b, CS t){
    b[0]=0;
    if(!t.m){ scpy(b,"NO TIME AT ALL"); return; }
    CS tp=CN_TP;
    if(cs_cmp(t,tp)<0){ scpy(b,"LESS THAN ONE PLANCK TIME"); return; }
    int X=t.e+9; u64 r; int x;
    if(X<=-25){                                                         // shorter than a yoctosecond: in Planck times, or in seconds with a power
        CS q=cs_div(t,tp);
        if(q.e+9<3){ cs_round(q,3,&r,&x); cn_dec(b,r,3,x); cn_unit(b,"PLANCK TIME",r==100 && x==0); return; }
        cs_round(t,3,&r,&x); cn_sci(b,r,3,x); scat(b," SECONDS"); return; }
    if(X<0){                                                            // milliseconds ... yoctoseconds
        cs_round(t,3,&r,&x); int k=(-x+2)/3, s=x+3*k; cn_dec(b,r,3,s); cn_unit(b,cn_su[k],r==100 && s==0); return; }
    CS v=t; const char *u="SECOND";
    if(cs_cmp(t,cs_u(60))<0){ }
    else if(cs_cmp(t,cs_u(3600))<0){ v=cs_divu(t,60); u="MINUTE"; }
    else if(cs_cmp(t,cs_u(86400))<0){ v=cs_divu(t,3600); u="HOUR"; }
    else if(cs_cmp(t,cs_u(31557600ULL))<0){ v=cs_divu(t,86400); u="DAY"; }
    else { v=cs_divu(t,31557600ULL); u="YEAR"; }
    cs_round(v,3,&r,&x);
    if(u[0]!='Y' || x<3){ cn_dec(b,r,3,x); cn_unit(b,u,r==100 && x==0); return; }
    if(x<6){ cn_comma(b,r*cn_p10[x-2]); scat(b," YEARS"); return; }
    cn_dec(b,r,3,x%3); scat(b," "); scat(b,illion(x/3)); scat(b," YEARS"); }
// ---------------------------------------------------------------- what a speed means, computed (c = 299,792.458 km/s exactly; a year = 365.25 days)
static CS cn_kmps(CS c){ return cs_divu(cs_mulu(c,299792458u),1000u); }            // km per second
static CS cn_lyps(CS c){ return cs_divu(c,31557600u); }                            // light years per second
enum { CR_EARTH, CR_STAR, CR_MW, CR_GAL, CR_UNI, CR_N };                           // Earths (12,742 km wide), stars (one per 4 ly), Milky Ways (100,000 ly), galaxies (one per 3 million ly), universes (93 billion ly)
static CS cn_len_light(int k){                                                      // how many seconds light takes to cross one of those things
    switch(k){ case CR_EARTH: return cs_div(cs_u(12742000ULL),cs_u(299792458ULL)); case CR_STAR: return cs_mulu(cs_u(4),31557600u);
        case CR_MW: return cs_mulu(cs_u(100000),31557600u); case CR_GAL: return cs_mulu(cs_u(3000000),31557600u); default: return cs_mulu(cs_make(93,9),31557600u); } }
static CS cn_rate(int k, CS c){ return cs_div(c,cn_len_light(k)); }                 // how many of them per second
static void cn_rate_text(char *b, int k, CS c){                                    // "102,986 PER SECOND", "ONE EVERY 7.28 SECONDS"
    b[0]=0; CS r=cn_rate(k,c); if(!r.m){ scpy(b,"NONE"); return; }
    if(cs_cmp(r,cs_u(1))>=0){ cn_sn(b,r); scat(b," PER SECOND"); return; }
    scpy(b,"ONE EVERY "); char t[48]; cn_time(t,cs_div(cn_len_light(k),c)); scat(b,t); }
enum { CT_MOON, CT_SUN, CT_PROX, CT_MW, CT_AND, CT_UNI, CT_GAME, CT_N };            // the Moon, 1 AU, Proxima (4.2465 ly), the Milky Way, Andromeda (2.537 million ly), the observable universe (93 billion ly), the game's own map (25 x 2^155 ly)
static CS cn_trip_light(int k){                                                    // how many seconds light itself takes
    switch(k){ case CT_MOON: return cs_div(cs_u(384400000ULL),cs_u(299792458ULL)); case CT_SUN: return cs_div(cs_u(149597870700ULL),cs_u(299792458ULL));
        case CT_PROX: return cs_mulu(cs_make(42465,-4),31557600u); case CT_MW: return cs_mulu(cs_u(100000),31557600u); case CT_AND: return cs_mulu(cs_make(2537,3),31557600u);
        case CT_UNI: return cs_mulu(cs_make(93,9),31557600u); default: return cs_mulu(cs_make(1141798154ULL,39),31557600u); } }
static void cn_trip_text(char *b, int k, CS c){ if(!c.m){ scpy(b,"NEVER"); return; } cn_time(b,cs_div(cn_trip_light(k),c)); }
