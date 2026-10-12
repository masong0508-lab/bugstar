// ================================================================= KART RACING, part two: the menus, the garage, your tracks, saving
static void kbuild_menu(void); static void kgarage(volatile u16 *pg);
// ---------------------------------------------------------------- saving: your kart and record (per slot), best times (per slot), your tracks (shared)
#define KSAVE_OFF 0xD00
#define KREC_OFF 0xE00
#define NKBEST 32
typedef struct { u32 magic; KLook look; u16 own[NKPART]; u16 races, wins, pods, built, tt, cups[4]; u8 cls_open, pad[3]; u8 cam[8]; u32 sum; } KartSave;
_Static_assert(__builtin_offsetof(KartSave,sum)==sizeof(KartSave)-4, "KartSave: the sum must be the last 4 bytes");
_Static_assert(KSAVE_OFF+sizeof(KartSave) <= KREC_OFF && KREC_OFF+NKBEST*8 <= TV2_OFF && META_OFF+16 <= KSAVE_OFF, "the kart blocks must fit in the slot");
static void kw_keep(void);
static void kart_save(void){
    if(kw_mode){ kw_keep(); return; }
    KartSave s; memset(&s,0,sizeof s); s.magic=0x3254524Bu; s.look=kl; for(int p=0;p<NKPART;p++) s.own[p]=kown[p];
    for(int i=0;i<8;i++) s.cam[i]=kcam[i]; s.races=kst_races; s.wins=kst_wins; s.pods=kst_pods; s.built=kst_built; s.tt=kst_tt; for(int c=0;c<4;c++) s.cups[c]=kst_cups[c]; s.cls_open=kcls_open;
    s.sum=fnv(&s,sizeof s-4); sram_write(slot_base(cur_slot)+KSAVE_OFF,&s,sizeof s); }
static void kart_load(void){
    KartSave s; sram_read(slot_base(cur_slot)+KSAVE_OFF,&s,sizeof s);
    if(s.magic!=0x3254524Bu || s.sum!=fnv(&s,sizeof s-4)){ kart_defaults(); return; }
    kl=s.look; for(int p=0;p<NKPART;p++){ kown[p]=s.own[p]; if(kl.part[p]>=kpart_n[p]) kl.part[p]=0; } if(kl.part[KP_SIZE]>2) kl.part[KP_SIZE]=1; kl.number%=100; kl.name[11]=0;
    for(int i=0;i<8;i++) kcam[i]=s.cam[i]; kcam[1]%=5; kcam[2]%=5; kcam[3]&=3; kcam[4]&=3; kst_races=s.races; kst_wins=s.wins; kst_pods=s.pods; kst_built=s.built; kst_tt=s.tt; for(int c=0;c<4;c++) kst_cups[c]=s.cups[c]; kcls_open=s.cls_open<1?1:s.cls_open>4?4:s.cls_open; }
typedef struct { u32 key; u16 best; u8 medal, chk; } KBest;
static u8 kbest_chk(const KBest *b){ return (u8)(0x5A ^ b->key ^ (b->key>>8) ^ (b->key>>16) ^ (b->key>>24) ^ b->best ^ (b->best>>8) ^ b->medal); }
static int kbest_find(u32 key, KBest *out){ for(int i=0;i<NKBEST;i++){ KBest b; sram_read(slot_base(cur_slot)+KREC_OFF+i*8,&b,8); if(b.key==key && b.chk==kbest_chk(&b)){ if(out) *out=b; return i; } } return -1; }
static void kbest_put(u32 key, int t, int medal){
    KBest b; int i=kbest_find(key,&b); if(i<0){ i=(int)(key%NKBEST); for(int j=0;j<NKBEST;j++){ KBest q; int k=(int)((key+j)%NKBEST); sram_read(slot_base(cur_slot)+KREC_OFF+k*8,&q,8); if(q.chk!=kbest_chk(&q) || !q.key){ i=k; break; } } b.key=key; b.best=0xFFFF; b.medal=0; }
    if(t<b.best) b.best=(u16)(t>65000?65000:t); if(medal>b.medal) b.medal=(u8)medal; b.chk=kbest_chk(&b); sram_write(slot_base(cur_slot)+KREC_OFF+i*8,&b,8); }
static u32 kt_key(int idx){ return hmix(wd.tseed^((u32)idx*0x9E3779B1u)^0x5245434Fu)|1; }
static int kw_best_get(int trk, KBest *b); static void kw_best_set(int trk, int t, int medal);
static int kbest_get(int trk, KBest *b){ if(kw_mode) return kw_best_get(trk,b); return kbest_find(kt_key(trk),b)>=0; }
static void kbest_set(int trk, int t, int medal){ if(kw_mode) kw_best_set(trk,t,medal); else kbest_put(kt_key(trk),t,medal); }
// your tracks: 30 just below the options block, 20 in the spare room after the favourites
typedef struct { Fav at; u8 n, laps, riv, cls, flags, surf, medal, shape; u16 best, runs; u8 gx[KG_MAX], gy[KG_MAX]; char name[16]; u32 sum; } KRec;
_Static_assert(sizeof(KRec)==128, "a saved track is 128 bytes");
_Static_assert(0x7000+30*128 <= SET_OFF && 0x6010+20*128 <= FG_OFF && FAV_HDR+4 <= 0x6010, "your tracks must fit in the spare cartridge memory");
static int krec_off(int i){ return i<30 ? 0x7000+i*128 : 0x6010+(i-30)*128; }
static int krec_get(int i, KRec *r){ sram_read(krec_off(i),r,128); if(!((r->at.used&0xE0)==0xA0 && r->sum==fnv(r,124))) return 0; r->at.used^=0xA5; return 1; }   // (on the cartridge 'used' is 0xA5 ^ the universe's table number, so old tracks, in universe 0-2^32, still read)
static void krec_put(int i, KRec *r){ KRec t=*r; if(t.at.used>UT_N) t.at.used=0; t.at.used^=0xA5; t.at.chk=fav_chk(&t.at); t.sum=fnv(&t,124); sram_write(krec_off(i),&t,128); }
static int uni_alloc(const u32 *u){                                    // the table number for universe u (adds it if new); -1 if the 31 slots are all in use by favourites and tracks
    UTab t; ut_read(&t); int k=ut_find(&t,u); if(k>=0) return k;
    int free_=-1; for(int i=0;i<UT_N;i++) if(!(t.h[i][0]|t.h[i][1]|t.h[i][2])){ free_=i; break; }
    if(free_<0){ u8 used[UT_N+1]; for(int i=0;i<=UT_N;i++) used[i]=0; Fav f; KRec r;
        for(int i=0;i<NFAV;i++) if(fav_get(i,&f)) used[f.used]=1;
        for(int i=0;i<NKREC;i++) if(krec_get(i,&r)) used[r.at.used]=1;
        used[0]=1; for(int i=0;i<UT_N;i++) if(!used[i+1]){ free_=i; break; } }
    if(free_<0) return -1;
    t.h[free_][0]=u[1]; t.h[free_][1]=u[2]; t.h[free_][2]=u[3]; ut_write(&t); return free_+1; }
static void krec_clear(int i){ for(int k=0;k<128;k++) SRAM[krec_off(i)+k]=0; }
static void kw_fav(Fav *f);
static int krec_here(const KRec *r){ Fav h; if(kw_mode) kw_fav(&h); else fav_here(&h); return fav_eq(&r->at,&h); }
static void krec_to_kt(const KRec *r){ memset(&kt,0,sizeof kt); kt.n=r->n; kt.laps=r->laps; kt.riv=r->riv; kt.cls=r->cls; kt.flags=r->flags; kt.surf=r->surf; kt.shape=r->shape;
    for(int i=0;i<KG_MAX;i++){ kt.gx[i]=r->gx[i]; kt.gy[i]=r->gy[i]; } memcpy(kt.name,r->name,16); kt.name[15]=0; }

// ---------------------------------------------------------------- a name box (up to max letters)
static void kname_edit(volatile u16 *pg, const char *title, char *nm, int max){
    char b[20]; scpy(b,nm); int n=textlen(b), cur=0, t=0; if(n>max){ n=max; b[n]=0; }
    oam_hide_all(); oam_flush(); wait_release();
    while(1){
        box(pg,16,52,208,56); rect(pg,17,53,206,12,6); ctext(pg,120,56,title,C_GOLD);
        int x0=120-max*4; for(int k=0;k<max;k++){ char ch[2]={ k<n?b[k]:'_',0 }; int c = k==cur ? C_GOLD : k<n ? C_WHITE : C_DGREY; rect(pg,x0+k*8,70,8,12,0); text(pg,x0+k*8,72,ch,c,2); if(k==cur && (t&16)) rect(pg,x0+k*8,83,6,1,C_GOLD); }
        ctext(pg,120,94,"UP/DOWN: LETTER   LEFT/RIGHT: MOVE   A: OK",C_GREY);
        vsync(); read_keys(); t++;
        if(HIT(K_UP)||HIT(K_DOWN)){ if(cur>=n){ b[cur]='A'; n=cur+1; b[n]=0; } else { char c=b[cur]; c = HIT(K_UP) ? (c=='Z'?' ':c==' '?'0':c=='9'?'A':(c>='A'&&c<'Z')||(c>='0'&&c<'9')?c+1:'A') : (c=='A'?'9':c=='0'?' ':c==' '?'Z':(c>'A'&&c<='Z')||(c>'0'&&c<='9')?c-1:'Z'); b[cur]=c; } sfx(7); }
        if(HIT(K_RIGHT) && cur<max-1){ cur++; if(cur>=n){ b[cur]='A'; n=cur+1; b[n]=0; } }
        if(HIT(K_LEFT) && cur>0) cur--;
        if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ while(n>0 && b[n-1]==' ') n--; b[n]=0; if(n>0) scpy(nm,b); sfx(2); wait_release(); return; } } }

// ---------------------------------------------------------------- picking a track and a class
static int ktp_n, ktp_nc; EW static u8 ktp_cust[NKREC]; EW static KTrack ktp_tmp;
static int kt_rough_len(const KTrack *t){ int s=0; for(int i=0;i<t->n;i++){ int j=(i+1)%t->n, dx=kwrap8(t->gx[j]-t->gx[i]), dy=kwrap8(t->gy[j]-t->gy[i]); s+=isqrt((u32)(dx*dx+dy*dy)); } return s/2; }
static void ktp_row(int i, char *b, int *c){
    if(i<ktp_n){ kt_official(i,&ktp_tmp); scpy(b,ktp_tmp.name); while(textlen(b)<17) scat(b," "); scat(b,kshape_name[ktp_tmp.shape]);
        KBest kb; if(kbest_get(i,&kb) && kb.best<0xFFFF){ char t[12]; kfmt_time(t,kb.best); while(textlen(b)<31) scat(b," "); scat(b,t); if(kb.medal) *c = kb.medal==3?C_GOLD:kb.medal==2?C_WHITE:C_RED; } return; }
    KRec r; krec_get(ktp_cust[i-ktp_n],&r); scpy(b,"* "); scat(b,r.name); while(textlen(b)<19) scat(b," "); scat(b,"YOUR TRACK"); *c=C_CYAN;
    if(r.best && r.best<0xFFFF){ char t[12]; kfmt_time(t,r.best); while(textlen(b)<31) scat(b," "); scat(b,t); } }
static void ktp_desc(int i, char *b, int *c){
    if(i<0){ i=-1-i; if(i<ktp_n){ scpy(b,"SURFACE: "); kt_official(i,&ktp_tmp); scat(b,ksurf_name[ktp_tmp.surf]); if(ktp_tmp.flags&KF_PADS) scat(b,"   BOOST PADS"); } else scpy(b,"A TRACK YOU BUILT HERE"); return; }
    if(i<ktp_n){ kt_official(i,&ktp_tmp); scpy(b,"ABOUT "); ncat(b,kt_rough_len(&ktp_tmp)); scat(b," CELLS ROUND, "); ncat(b,ktp_tmp.n); scat(b," GATES"); }
    else { KRec r; krec_get(ktp_cust[i-ktp_n],&r); scpy(b,"BY YOU. "); ncat(b,r.n); scat(b," GATES, "); ncat(b,r.laps); scat(b," LAPS, "); ncat(b,r.riv); scat(b," RIVALS"); } (void)c; }
static int kpick_track(volatile u16 *pg, const char *title, int customs){   // >=0: this planet's track; <=-2: your saved track -2-i; -1: back
    ktp_n=kt_count(); ktp_nc=0; if(customs){ KRec r; for(int i=0;i<NKREC;i++) if(krec_get(i,&r) && krec_here(&r)) ktp_cust[ktp_nc++]=i; }
    int sel=0; list_desc=ktp_desc; int k=list_pick(pg,title,ktp_n+ktp_nc,ktp_row,&sel,"A: PICK   B: BACK"); list_desc=0;
    if(k<0) return -1; return k<ktp_n ? k : -2-ktp_cust[k-ktp_n]; }
static void kcls_row(int i, char *b, int *c){ static const char *const d[5]={"  (EASY)","  (NORMAL)","  (FAST)","  (VERY FAST)","  (LUDICROUS)"}; scpy(b,kcls_name[i]); scat(b,d[i]);
    if(i>=kcls_open){ static const char *const need[5]={"","","   LOCKED: WIN A PUPA CUP","   LOCKED: WIN AN IMAGO CUP","   LOCKED: WIN A STARBUG TOUR"}; scat(b,need[i]); *c=C_DGREY; } }
static int kpick_class(volatile u16 *pg){ int sel=k_cls<kcls_open?k_cls:0; list_desc=0;
    while(1){ int k=list_pick(pg,"PICK A CLASS",kw_mode?5:4,kcls_row,&sel,"A: PICK   B: BACK"); if(k<0) return -1; if(k<kcls_open) return k; say("WIN A CUP TO OPEN THIS CLASS"); } }

// ---------------------------------------------------------------- the results
static int k_par(void){ return kt_len*kt.laps*6*100/kcls_pct[k_cls]; }
static void kart_cup_next(void); static void kw_after_race(int me); static void kw_endless_next(int me); static void kw_tt_gold(void); static void kw_tour_race(int i); static void kw_tour_done(int me);
static void kres_order(u8 *ord){                                    // finishers first, in order; then the rest by how far they got
    int n=0; for(int p=1;p<=NRC;p++) for(int r=0;r<nrc;r++) if(rc[r].fin && rc[r].place==p) ord[n++]=(u8)r;
    while(n<nrc){ int best=-1, bs=-1; for(int r=0;r<nrc;r++){ int in=0; for(int j=0;j<n;j++) if(ord[j]==r) in=1; if(in) continue; int s=kscore(r); if(s>bs){ bs=s; best=r; } } ord[n++]=(u8)best; } }
static void kart_results(void){
    volatile u16 *pg=VISPG; u8 ord[NRC]; kres_order(ord); int me=0; for(int i=0;i<nrc;i++){ rc[ord[i]].place=(u8)(i+1); if(ord[i]==0) me=i+1; }
    oam_hide_all(); oam_flush(); in_menu++;
    static const u8 cpts[8]={15,12,10,8,6,4,2,1};
    int prize=0, medal=0; char l1[64], l2[64];
    int base=(30+dlv()*6)*(k_cls==0?100:k_cls==1?160:k_cls==2?240:350)/100;
    if(nrc>1){ kst_races++; if(me==1) kst_wins++; if(me<=3) kst_pods++;
        if(k_type!=1){ static const u8 pc[8]={100,60,40,20,5,5,5,5}; prize=base*pc[me-1]/100; }
        if(k_type==1) for(int i=0;i<nrc;i++) rc[ord[i]].pts+=cpts[i];
        if(me==1 && pq_type==8 && !pq_done){ kart_won=1; job_tick(); }
        if(kst_wins>=10) kown[KP_TOP]|=1u<<15; }
    else { int t=rc[0].ftime, par=k_par(); medal = t<=par ? 3 : t<=par*112/100 ? 2 : t<=par*130/100 ? 1 : 0; if(medal==3) kst_tt++; prize = medal ? base*medal/3 : 0; }
    if(rc[0].fin && k_type!=3){ if(k_rec>=0){ KRec r; if(krec_get(k_rec,&r)){ if(!r.best || rc[0].ftime<r.best) r.best=rc[0].ftime; if(medal>r.medal) r.medal=(u8)medal; r.runs++; krec_put(k_rec,&r); } }
        else if(k_trk>=0) kbest_set(k_trk, rc[0].ftime, medal); }
    gold+=prize; fame+=me==1?6:me<=3?2:0;
    // the card
    box(pg,8,20,224,132); rect(pg,9,21,222,12,6);
    { char t[40]; scpy(t,k_type==1?"CUP RACE ":"RESULTS: "); if(k_type==1){ ncat(t,k_cup_i+1); scat(t," OF 4: "); } scat(t,kt.name); ctext(pg,120,24,t,C_GOLD); }
    for(int i=0;i<nrc;i++){ Racer *k=&rc[ord[i]]; int y=38+i*9, c = ord[i]==0 ? C_GOLD : C_WHITE; char t[48];
        scpy(t,kplace_s(i+1)); text(pg,16,y,t,i==0?C_GOLD:C_GREY,1);
        scpy(t, ord[i]==0 ? "YOU" : k->name); t[19]=0; text(pg,36,y,t,c,1);
        if(ord[i] && (k->being==KB_CRIT0||k->being==KB_CRIT1)) scpy(t,"CREATURE"); else scpy(t, ord[i]==0 ? (form==FORM_HUMAN?"HUMANOID":form==FORM_CREATURE?"CREATURE":"BUG") : kbeing_kind[k->being]); text(pg,120,y,t,C_DGREY,1);
        if(k->fin) kfmt_time(t,k->ftime); else { int sc=kscore(ord[i]), tot=(kt.laps+1)*knp; if(sc>knp/4 && nrc>1){ int est=k_clock+(int)((s32)(tot-sc)*k_clock/(sc>1?sc:1)); kfmt_time(t,est); } else scpy(t,"--"); } text(pg,172,y,t,c,1);   // the ones still going: about when they will finish
        if(k_type==1){ scpy(t,"+"); ncat(t,cpts[i]); text(pg,206,y,t,C_CYAN,1); } }
    if(nrc>1){ if(ord[0]){ scpy(l1,rc[ord[0]].name); scat(l1,": \""); scat(l1, rc[ord[0]].being==KB_CRIT0||rc[ord[0]].being==KB_CRIT1 ? "SQUEAK! SQUEAK!" : kbeing_brag[rc[ord[0]].being]); scat(l1,"\""); l1[54]=0; ctext(pg,120,112,l1,C_GREY); }
        else ctext(pg,120,112,"FIRST PLACE! THE CROWD GOES WILD. MOSTLY BUGS.",C_GREEN); }
    else { static const char *const md[4]={"NO MEDAL. KEEP AT IT","BRONZE MEDAL","SILVER MEDAL","GOLD MEDAL!"}; char t[12]; kfmt_time(t,k_par()); scpy(l1,md[medal]); scat(l1,"   GOLD TIME: "); scat(l1,t); ctext(pg,120,112,l1,medal==3?C_GOLD:C_WHITE); }
    l2[0]=0; if(prize){ scpy(l2,"PRIZE: +"); ncat(l2,prize); scat(l2,kw_mode?" COINS   ":"G   "); } if(k_best_lap){ char t[12]; kfmt_time(t,k_best_lap); scat(l2,"YOUR BEST LAP "); scat(l2,t); } if(l2[0]) ctext(pg,120,122,l2,C_GOLD);
    wait_release(); for(int t=0;;t++){ vsync(); read_keys(); if(HIT(K_A)||HIT(K_START)) break; ctext(pg,120,140,"A: GO ON",(t&32)?C_CYAN:C_DGREY); }
    sfx(2); wait_release(); in_menu--;
    if(kw_mode){ if(nrc==1 && medal==3) kw_tt_gold(); kw_after_race(me); } else { kart_save(); save_game(); }
    if(k_type==1){ kart_cup_next(); return; }
    if(k_type==5){ kw_endless_next(me); return; }
    // what next
    static const char *const qn[3]={"RACE AGAIN","PICK ANOTHER TRACK","LEAVE"}; static const char *const tn[3]={"TRY AGAIN","BACK TO THE TRACK MENU","LEAVE"};
    const char *const *opt = k_type==3 ? tn : qn; int sel=0;
    while(1){ in_menu++; box(pg,40,50,160,60); rect(pg,41,51,158,12,6); ctext(pg,120,54,"WHAT NOW?",C_GOLD);
        for(int i=0;i<3;i++){ if(i==sel) text(pg,52,70+i*10,">",C_CYAN,1); text(pg,60,70+i*10,opt[i],i==sel?C_WHITE:C_GREY,1); } in_menu--;
        vsync(); read_keys(); if(HIT(K_UP)) sel=(sel+2)%3; if(HIT(K_DOWN)) sel=(sel+1)%3; if(HIT(K_A)) break; if(HIT(K_B)){ sel=2; break; } }
    wait_release();
    if(sel==0){ kart_go(nrc, nrc>1?-1:0, 0, 0); return; }
    if(sel==1 && k_type==3){ kgive_back(); kr_mode=2; ks_state=0; nrc=1; ppx=kret_x; ppy=kret_y; heading=kret_h; heading_rays(); kbuild_menu(); return; }
    if(sel==1){ int t=kpick_track(VISPG,"PICK A TRACK",1); if(t==-1){ kart_stop(); return; } if(t>=0){ k_trk=t; k_rec=-1; kt_official(t,&kt); } else { KRec r; k_rec=-2-t; k_trk=-1; krec_get(k_rec,&r); krec_to_kt(&r); }
        kart_go(nrc, nrc>1?-1:0, 0, 1); return; }
    kart_stop(); }

// ---------------------------------------------------------------- the cup: four races, points for every place
static void kart_cup_start(void){
    int n=kt_count(); u32 h=hmix(wd.tseed^0x43555021u^(u32)k_cls); for(int i=0;i<4;i++){ int t; do { h=hmix(h+i); t=(int)(h%(u32)n); int dup=0; for(int j=0;j<i;j++) if(kcup_trk[j]==t) dup=1; if(!dup) break; } while(1); kcup_trk[i]=(u8)t; }
    const char *ln[6]; char a[48], tn[4][24]; scpy(a,"THE "); { char w[24]; scpy(w,world_name()); w[14]=0; scat(a,w); } scat(a," CUP: "); scat(a,kcls_name[k_cls]); ln[0]=a;
    for(int i=0;i<4;i++){ kt_official(kcup_trk[i],&ktp_tmp); scpy(tn[i],""); ncat(tn[i],i+1); scat(tn[i],". "); scat(tn[i],ktp_tmp.name); ln[1+i]=tn[i]; } ln[5]="POINTS: 15 12 10 8 6 4 2 1";
    card(VISPG,"GRAND PRIX",ln,6);
    k_type=1; k_cup_i=0; k_trk=kcup_trk[0]; k_rec=-1; kt_official(k_trk,&kt); rc[0].pts=0; kart_go(NRC,-1,0,1); for(int r=0;r<NRC;r++) rc[r].pts=0; }
static void kart_cup_next(void){
    volatile u16 *pg=VISPG; u8 ord[NRC]; for(int i=0;i<nrc;i++) ord[i]=(u8)i;
    for(int i=0;i<nrc;i++) for(int j=i+1;j<nrc;j++) if(rc[ord[j]].pts>rc[ord[i]].pts){ u8 t=ord[i]; ord[i]=ord[j]; ord[j]=t; }
    int me=0; for(int i=0;i<nrc;i++) if(ord[i]==0) me=i+1;
    in_menu++; box(pg,8,20,224,132); rect(pg,9,21,222,12,6); ctext(pg,120,24, k_cup_i<3 ? "CUP STANDINGS" : "FINAL STANDINGS", C_GOLD);
    for(int i=0;i<nrc;i++){ Racer *k=&rc[ord[i]]; char t[40]; int y=38+i*9, c = ord[i]==0 ? C_GOLD : C_WHITE;
        text(pg,16,y,kplace_s(i+1),C_GREY,1); scpy(t, ord[i]==0 ? "YOU" : k->name); t[22]=0; text(pg,40,y,t,c,1); scpy(t,""); ncat(t,k->pts); scat(t," PTS"); text(pg,170,y,t,c,1); }
    if(k_cup_i>=3){ int prize=0; char t[48];
        if(me<=3){ int base=(30+dlv()*6)*(k_cls==0?100:k_cls==1?160:k_cls==2?240:350)/100; static const u8 mul[3]={8,4,2}; prize=base*mul[me-1]; gold+=prize; fame+=me==1?30:10;
            if(me==1){ kst_cups[k_cls]++; kown[KP_TOP]|=1u<<6; if(k_cls==3) kown[KP_BODY]|=1u<<11; if(kcls_open<k_cls+2 && k_cls<3){ kcls_open=(u8)(k_cls+2); } } }
        static const char *const tr[4]={"","GOLD TROPHY! YOU WON THE CUP!","SILVER TROPHY!","BRONZE TROPHY!"}; ctext(pg,120,112, me<=3 ? tr[me] : "NO TROPHY THIS TIME", me==1?C_GOLD:C_WHITE);
        if(prize){ scpy(t,"PRIZE: +"); ncat(t,prize); scat(t,"G"); ctext(pg,120,122,t,C_GOLD); }
        if(me==1 && k_cls<3 && kcls_open==k_cls+2) ctext(pg,120,131,k_cls==1?"NEW CLASS OPEN: IMAGO":k_cls==2?"NEW CLASS OPEN: STARBUG":"",C_CYAN);
        else if(me==1) ctext(pg,120,131,k_cls==3?"YOU WON THE SAUCER AND THE CROWN!":"YOU WON THE CROWN TOPPER!",C_CYAN); }
    in_menu--; wait_release(); for(int t=0;;t++){ vsync(); read_keys(); if(HIT(K_A)||HIT(K_START)) break; ctext(pg,120,142,"A: GO ON",(t&32)?C_CYAN:C_DGREY); }
    sfx(2); wait_release(); kart_save();
    if(k_cup_i>=3){ if(kw_mode) kw_tour_done(me); kart_stop(); return; }
    k_cup_i++; if(kw_mode) kw_tour_race(k_cup_i); else { k_trk=kcup_trk[k_cup_i]; kt_official(k_trk,&kt); }
    int grid=nrc-1; for(int i=0;i<nrc;i++) if(ord[i]==0) grid=nrc-1-i; if(grid<0) grid=0;     // the leader starts at the back
    kart_go(NRC,grid,1,1); if(kw_mode) fade(0); }

// ---------------------------------------------------------------- the track maker: drive, drop gates, close the loop
EW static u8 kbeac[KG_MAX];
static void kbuild_begin(void){
    const char *ln[5]={"DRIVE ANYWHERE ON THIS PLANET.","A: DROP A GATE WHERE YOU ARE.","SELECT: TAKE THE LAST ONE BACK.","START: CLOSE THE LOOP AND FINISH.","4 TO 24 GATES. THE ROAD JOINS THEM UP."};
    card(VISPG,"BUILD A TRACK",ln,5);
    kret_x=ppx; kret_y=ppy; kret_h=heading; kgive_back(); kt_unpaint(); memset(&kt,0,sizeof kt); kt.riv=7; kt.cls=(u8)(k_cls<kcls_open?k_cls:0); kt.flags=KF_ITEMS|KF_PADS;
    kt.flags|=KF_JUMPS|KF_LOOPS; kt.surf = wd.cls==PC_ICE ? 2 : wd.cls==PC_DESERT ? 1 : 0; memset(&rc[0],0,sizeof(Racer)); rc[0].look=kl; nrc=1; kart_build(0);
    kfp_keep=cam_fp; kr_mode=2; kcam_apply(); khd=heading; kv=0; kdrift=kcharge=kboost_t=0; kart_boost=100; kart_on=1; music(6); show_msg("DRIVE. A DROPS A GATE"); p_last=vbl_count; }
static void kbuild_drop(void){
    int tx=(ppx>>15)&255, ty=(ppy>>15)&255;
    if(kt.n>=KG_MAX){ show_msg("24 GATES! PRESS START TO FINISH"); return; }
    if(kt.n){ int dx=kwrap8(tx-kt.gx[kt.n-1]), dy=kwrap8(ty-kt.gy[kt.n-1]), d=dx*dx+dy*dy; if(d<64){ show_msg("TOO CLOSE TO THE LAST GATE"); sfx(4); return; } if(d>96*96){ show_msg("TOO FAR FROM THE LAST GATE"); sfx(4); return; } }
    kt.gx[kt.n]=(u8)tx; kt.gy[kt.n]=(u8)ty; int p=kgrab(O_BEACON,ppx,ppy); kbeac[kt.n]=(u8)(p<0?255:p); kt.n++;
    char b[32]; scpy(b,"GATE "); ncat(b,kt.n); scat(b," DOWN"); if(kt.n>=4) scat(b,". START: FINISH"); show_msg(b); sfx(7); }
static void kbuild_undo(void){ if(!kt.n){ show_msg("NO GATES YET"); return; } kt.n--; if(kbeac[kt.n]!=255) kpo_release(kbeac[kt.n]); show_msg("GATE TAKEN BACK"); sfx(2); }
static void kt_random_name(KTrack *t){ u32 h=rnd(); kt_name(t,h); }
static void kbuild_save(volatile u16 *pg);
static void kbuild_menu(void){                                            // the finished loop: name it, tune it, test it, keep it
    volatile u16 *pg=VISPG; int sel=0, dirty=1, arm=0;
    enum { B_NAME, B_LAPS, B_RIV, B_CLS, B_SURF, B_ITEMS, B_PADS, B_RAMPS, B_LOOPS, B_TEST, B_SAVE, B_MORE, B_BIN, NB };
    static const char *const bl[NB]={"NAME","LAPS","RIVALS","CLASS","SURFACE","ITEMS","BOOST PADS","RAMPS","LOOP-DE-LOOPS","TEST DRIVE IT","SAVE THIS TRACK","KEEP BUILDING","THROW IT AWAY"};
    oam_hide_all(); oam_flush(); wait_release(); in_menu++;
    while(1){
        if(dirty){ dirty=0; char b[48];
            box(pg,12,20,216,136); rect(pg,13,21,214,12,6); ctext(pg,120,24,"YOUR TRACK",C_GOLD);
            scpy(b,""); ncat(b,kt.n); scat(b," GATES, "); ncat(b,kt_len); scat(b," CELLS ROUND ON A "); scat(b,kpsize_name[wd.psz]); scat(b," PLANET"); ctext(pg,120,35,b,C_GREY);
            for(int i=0;i<NB;i++){ int y=44+i*7; if(i==sel) rect(pg,16,y-1,208,8,6); text(pg,22,y,bl[i],i==sel?C_WHITE:i>=B_TEST?C_CYAN:C_GREY,1);
                b[0]=0; switch(i){ case B_NAME: scpy(b,kt.name); break; case B_LAPS: scpy(b,"< "); ncat(b,kt.laps); scat(b," >"); break; case B_RIV: scpy(b,"< "); ncat(b,kt.riv); scat(b," >"); break;
                    case B_CLS: scpy(b,"< "); scat(b,kcls_name[kt.cls%5]); scat(b," >"); break; case B_SURF: scpy(b,"< "); scat(b,ksurf_name[kt.surf%6]); scat(b," >"); break;
                    case B_ITEMS: scpy(b,(kt.flags&KF_ITEMS)?"< ON >":"< OFF >"); break; case B_PADS: scpy(b,(kt.flags&KF_PADS)?"< ON >":"< OFF >"); break; case B_RAMPS: scpy(b,(kt.flags&KF_JUMPS)?"< ON >":"< OFF >"); break; case B_LOOPS: scpy(b,(kt.flags&KF_LOOPS)?"< ON >":"< OFF >"); if((kt.flags&KF_LOOPS) && !nkl) scat(b,"  (NO STRAIGHT BIT FOR ONE)"); else if(nkl){ scat(b,"  "); ncat(b,nkl); scat(b,nkl>1?" LOOPS":" LOOP"); } break; }
                text(pg,110,y,b,i==sel?C_GOLD:C_GREY,1); }
            ctext(pg,120,139,sel==B_BIN&&arm?"A AGAIN TO THROW IT AWAY":"LEFT/RIGHT: CHANGE   A: PICK   SELECT: RANDOM NAME",arm?C_RED:C_CYAN);
            if(menu_msg[0]) ctext(pg,120,147,menu_msg,C_GREEN); }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NB-1)%NB; dirty=1; arm=0; menu_msg[0]=0; } if(HIT(K_DOWN)){ sel=(sel+1)%NB; dirty=1; arm=0; menu_msg[0]=0; }
        int d = HIT(K_RIGHT) ? 1 : HIT(K_LEFT) ? -1 : 0;
        if(d){ dirty=1; switch(sel){ case B_LAPS: kt.laps=(u8)(1+(kt.laps-1+d+9)%9); break; case B_RIV: kt.riv=(u8)((kt.riv+d+8)%8); break; case B_CLS: kt.cls=(u8)((kt.cls+d+kcls_open)%kcls_open); break;
            case B_SURF: kt.surf=(u8)((kt.surf+d+6)%6); kart_palette(); break; case B_ITEMS: kt.flags^=KF_ITEMS; break; case B_PADS: kt.flags^=KF_PADS; kt_unpaint(); kt_paint(); khide_road(); break;
            case B_RAMPS: kt.flags^=KF_JUMPS; kt_unpaint(); kt_paint(); khide_road(); break; case B_LOOPS: kt.flags^=KF_LOOPS; kt_unpaint(); kt_paint(); khide_road(); break; } sfx(7); }
        if(HIT(K_SEL)){ kt_random_name(&kt); dirty=1; sfx(6); }
        if(HIT(K_A)){ dirty=1;
            if(sel==B_NAME){ kname_edit(pg,"NAME YOUR TRACK",kt.name,15); }
            else if(sel<B_TEST){ int dd=1; switch(sel){ case B_LAPS: kt.laps=(u8)(1+(kt.laps-1+dd)%9); break; case B_RIV: kt.riv=(u8)((kt.riv+dd)%8); break; case B_CLS: kt.cls=(u8)((kt.cls+dd)%kcls_open); break;
                case B_SURF: kt.surf=(u8)((kt.surf+dd)%6); kart_palette(); break; case B_ITEMS: kt.flags^=KF_ITEMS; break; case B_PADS: kt.flags^=KF_PADS; kt_unpaint(); kt_paint(); khide_road(); break;
                case B_RAMPS: kt.flags^=KF_JUMPS; kt_unpaint(); kt_paint(); khide_road(); break; case B_LOOPS: kt.flags^=KF_LOOPS; kt_unpaint(); kt_paint(); khide_road(); break; } sfx(7); }
            else if(sel==B_TEST){ in_menu--; k_type=3; k_cls=kt.cls; k_trk=-1; k_rec=-1; for(int i=0;i<kt.n;i++) if(kbeac[i]!=255) kpo_release(kbeac[i]); kart_go(kt.riv+1,-1,0,0); return; }
            else if(sel==B_SAVE){ kbuild_save(pg); }
            else if(sel==B_MORE){ in_menu--; kt_unpaint(); kgive_back(); for(int i=0;i<kt.n;i++){ s32 x=((s32)kt.gx[i]<<15)+(1<<14), y=((s32)kt.gy[i]<<15)+(1<<14); int p=kgrab(O_BEACON,x,y); kbeac[i]=(u8)(p<0?255:p); }
                kr_mode=2; music(6); planet_hud(); p_last=vbl_count; return; }
            else if(sel==B_BIN){ if(!arm){ arm=1; sfx(4); } else { in_menu--; kart_stop(); show_msg("TRACK THROWN AWAY"); return; } } }
        if(HIT(K_B)){ sel=B_MORE; dirty=1; } }
}
static void kbuild_finish(void){
    if(kt.n<4){ show_msg("NEED AT LEAST 4 GATES"); sfx(4); return; }
    { int dx=kwrap8(kt.gx[0]-kt.gx[kt.n-1]), dy=kwrap8(kt.gy[0]-kt.gy[kt.n-1]); if(dx*dx+dy*dy>96*96){ show_msg("DRIVE BACK NEARER THE FIRST GATE"); sfx(4); return; } }
    show_msg("PAINTING YOUR ROAD..."); kt.laps=0; kt.shape=255; if(!kt.name[0]) kt_random_name(&kt);
    for(int i=0;i<kt.n;i++) if(kbeac[i]!=255) kpo_release(kbeac[i]);
    katan_init(); kt_build(); if(kt_len<20){ show_msg("TOO SHORT. MAKE A BIGGER LOOP"); for(int i=0;i<kt.n;i++){ s32 x=((s32)kt.gx[i]<<15)+(1<<14), y=((s32)kt.gy[i]<<15)+(1<<14); int p=kgrab(O_BEACON,x,y); kbeac[i]=(u8)(p<0?255:p); } return; }
    kt_paint(); khide_road(); kart_palette(); kst_built++; kbuild_menu(); }
static int ksv_sel; static void ksv_row(int i, char *b, int *c){ KRec r; if(krec_get(i,&r)){ scpy(b,""); ncat(b,i+1); scat(b,". "); scat(b,r.name); } else { scpy(b,""); ncat(b,i+1); scat(b,". (EMPTY)"); *c=C_DGREY; } }
static void kbuild_save(volatile u16 *pg){
    KRec r; int pick=-1; for(int i=0;i<NKREC;i++) if(!krec_get(i,&r)){ pick=i; break; }
    if(pick<0){ ksv_sel=0; list_desc=0; int k=list_pick(pg,"ALL 50 SPOTS ARE FULL. REPLACE WHICH?",NKREC>13?13:NKREC,ksv_row,&ksv_sel,"A: REPLACE   B: BACK"); if(k<0) return; pick=k; }
    memset(&r,0,sizeof r); if(kw_mode) kw_fav(&r.at); else { if(uni_alloc(uni_w)<0){ say("TOO MANY UNIVERSES SAVED. DELETE A FAVOURITE"); return; } fav_here(&r.at); } r.n=kt.n; r.laps=kt.laps; r.riv=kt.riv; r.cls=kt.cls; r.flags=kt.flags; r.surf=kt.surf; r.shape=255;
    for(int i=0;i<KG_MAX;i++){ r.gx[i]=kt.gx[i]; r.gy[i]=kt.gy[i]; } memcpy(r.name,kt.name,16); r.name[15]=0; krec_put(pick,&r); k_rec=pick;
    char b[40]; scpy(b,"SAVED AS TRACK "); ncat(b,pick+1); scat(b,". RACE IT ANYTIME"); say(b); sfx(6); kart_save(); }

// ---------------------------------------------------------------- your tracks, wherever they are
EW static u8 kmy_idx[NKREC]; static int kmy_n;
static void kmy_row(int i, char *b, int *c){ KRec r; krec_get(kmy_idx[i],&r); scpy(b,r.name); while(textlen(b)<17) scat(b," ");
    if(krec_here(&r)){ scat(b,"HERE"); *c=C_GREEN; } else if(r.at.star==0xFF){ scat(b,"IN KART WORLD"); *c=C_DGREY; } else if(!fav_in_uni(&r.at)){ char u[28]; u32 fu[4]; fav_uni(&r.at,fu); uni_name(u,fu); scat(b,u); *c = ant_king ? C_CYAN : C_DGREY; } else { char d[40]; here_ly(d,r.at.cx,r.at.cy); if(d[0]=='0'&&d[1]==' ') scpy(d,"THIS CLUSTER"); d[22]=0; scat(b,d); } }
static void kmy_desc(int i, char *b, int *c){ int k = i<0 ? -1-i : i; KRec r; krec_get(kmy_idx[k],&r);
    if(i<0){ char nm[32]; if(r.at.star==0xFF){ scpy(nm,"KART WORLD PLANET #"); ncat(nm,r.at.seed); } else fav_name(&r.at,nm); scpy(b,"ON "); scat(b,nm); return; }
    scpy(b,""); ncat(b,r.n); scat(b," GATES  "); ncat(b,r.laps); scat(b," LAPS  "); ncat(b,r.riv); scat(b," RIVALS  "); scat(b,ksurf_name[r.surf%6]);
    if(r.best){ char t[12]; kfmt_time(t,r.best); scat(b,"  BEST "); scat(b,t); } (void)c; }
static void kmy_tracks(volatile u16 *pg){
    int sel=0;
    while(1){ KRec r; kmy_n=0; for(int i=0;i<NKREC;i++) if(krec_get(i,&r)) kmy_idx[kmy_n++]=i;
        if(!kmy_n){ const char *ln[2]={"YOU HAVE NOT SAVED ANY TRACKS YET.","PICK BUILD A TRACK TO MAKE ONE."}; card(pg,"YOUR TRACKS",ln,2); return; }
        if(sel>=kmy_n) sel=kmy_n-1; list_desc=kmy_desc; int k=list_pick(pg,"YOUR TRACKS",kmy_n>11?11:kmy_n,kmy_row,&sel,"A: PICK   B: BACK"); list_desc=0; if(k<0) return;
        int ri=kmy_idx[k]; krec_get(ri,&r); int here=krec_here(&r);
        static const char *const oh[4]={"RACE IT","TIME TRIAL","DELETE IT","BACK"}, *const ot[4]={"SAVE THE PLANET AS A FAVOURITE","DELETE IT","BACK",""};
        const char *const *op = here ? oh : ot; int on = here ? 4 : 3;
        int s2=0; char ttl[40]; scpy(ttl,r.name);
        while(1){ in_menu++; box(pg,30,50,180,70); rect(pg,31,51,178,12,6); ctext(pg,120,54,ttl,C_GOLD); for(int i=0;i<on;i++){ if(i==s2) text(pg,40,70+i*10,">",C_CYAN,1); text(pg,48,70+i*10,op[i],i==s2?C_WHITE:C_GREY,1); }
            if(menu_msg[0]) ctext(pg,120,110,menu_msg,C_GREEN); in_menu--;
            vsync(); read_keys(); if(HIT(K_UP)) s2=(s2+on-1)%on; if(HIT(K_DOWN)) s2=(s2+1)%on; if(HIT(K_B)){ s2=on-1; break; } if(HIT(K_A)) break; }
        wait_release(); menu_msg[0]=0;
        if(here && (s2==0||s2==1)){ krec_to_kt(&r); k_rec=ri; k_trk=-1; k_cls = r.cls<kcls_open ? r.cls : 0; k_type = s2==0 ? 4 : 2; kart_go(s2==0 ? r.riv+1 : 1, s2==0?-1:0, 0, 1); return; }
        if((here && s2==2) || (!here && s2==1)){ if(confirm(pg,"DELETE THIS TRACK?","IT CANNOT BE UNDONE")){ krec_clear(ri); sfx(4); } continue; }
        if(!here && s2==0 && r.at.star==0xFF){ say("RACE IT FROM KART WORLD ON THE TITLE SCREEN"); continue; }
        if(!here && s2==0){ fav_init(); Fav f; int empty=-1, dup=0; for(int i=0;i<NFAV;i++){ if(fav_get(i,&f)){ if(fav_eq(&f,&r.at)) dup=1; } else if(empty<0) empty=i; }
            if(dup) say("IT IS ALREADY A FAVOURITE. FLY THERE FROM THE GALAXY MAP"); else if(empty<0) say("FAVOURITES ARE FULL"); else { fav_set(empty,&r.at); say("SAVED. GALAXY MAP, SELECT: FLY THERE"); sfx(6); } } } }

// ---------------------------------------------------------------- your racing record
static void kart_records(volatile u16 *pg){
    const char *ln[6]; char a[48], b2[48], c[48], d[48], e[48];
    scpy(a,"RACES "); ncat(a,kst_races); scat(a,"   WINS "); ncat(a,kst_wins); scat(a,"   PODIUMS "); ncat(a,kst_pods); ln[0]=a;
    scpy(b2,"CUPS: LARVA "); ncat(b2,kst_cups[0]); scat(b2,"  PUPA "); ncat(b2,kst_cups[1]); scat(b2,"  IMAGO "); ncat(b2,kst_cups[2]); scat(b2,"  STARBUG "); ncat(b2,kst_cups[3]); ln[1]=b2;
    scpy(c,"TIME TRIAL GOLDS "); ncat(c,kst_tt); scat(c,"   TRACKS BUILT "); ncat(c,kst_built); ln[2]=c;
    { int n=0; KRec r; for(int i=0;i<NKREC;i++) if(krec_get(i,&r)) n++; scpy(d,"TRACKS SAVED "); ncat(d,n); scat(d,"/50 (ALL SLOTS SHARE THEM)"); ln[3]=d; }
    scpy(e,"THIS PLANET: "); ncat(e,kt_count()); scat(e," TRACKS. "); scat(e,kpsize_name[wd.psz]); scat(e," WORLD"); ln[4]=e;
    ln[5]="CLASSES OPEN: "; { static char f[48]; scpy(f,"CLASSES OPEN: "); for(int i=0;i<kcls_open;i++){ if(i) scat(f,", "); scat(f,kcls_name[i]); } ln[5]=f; }
    card(pg,"YOUR KART RECORD",ln,6); }

// ---------------------------------------------------------------- the kart garage: a ludicrous amount of kart
// UP/DOWN pick a row. LEFT/RIGHT try things on (even ones you have not bought). A buys, mixes a colour, types the name or toots the horn.
// L turns the kart round. R matches the colours to the paint. SELECT surprises you. B undoes. START is done (anything not bought goes back).
static void kgarage(volatile u16 *pg){
    enum { G_NAME, G_BODY, G_SIZE, G_PAINT, G_TRIM, G_PAT, G_PATC, G_STICK, G_NUM, G_WHEEL, G_SPOIL, G_EXH, G_TOP, G_TRAIL, G_GLOW, G_HORN, NG };
    static const char *const gl[NG]={"NAME","BODY","SIZE","PAINT","TRIM","PATTERN","PATTERN COL","STICKER","NUMBER","WHEELS","SPOILER","EXHAUST","TOPPER","TRAIL","UNDERGLOW","HORN"};
    static const s8 gp[NG]={-1,KP_BODY,KP_SIZE,-1,-1,KP_PAT,-1,KP_STICK,-1,KP_WHEEL,KP_SPOIL,KP_EXH,KP_TOP,KP_TRAIL,KP_GLOW,KP_HORN};
    static const char *const *const gn[NKPART]={kbody_name,ksize_name,kpat_name,kstick_name,kwheel_name,kspoil_name,kexh_name,ktop_name,ktrail_name,kglow_name,khorn_name};
    KLook before=kl, hist[6]; int hn=0, hp=0, sel=1, top=0, dirty=1, t=0, view=1, arm=-1, rep=0, sch=-1, note_t=0; char note[44]; note[0]=0;
    #define G_PUSH() do{ hist[hp]=kl; hp=(hp+1)%6; if(hn<6) hn++; }while(0)
    kart_build(0); oam_hide_all(); oam_flush(); wait_release(); in_menu++;
    for(int i=0;i<24;i++) kpt[i].life=0;
    while(1){
        if(dirty){ dirty=0; char b[48];
            panel(pg,4,20,232,136,"THE KART GARAGE"); { char g2[20]="G"; ncat(g2,gold); text(pg,232-textlen(g2)*4,24,g2,C_GOLD,1); }
            if(sel<top) top=sel; if(sel>=top+11) top=sel-10;
            for(int j=0;j<11 && top+j<NG;j++){ int i=top+j, y=36+j*8, p=gp[i]; if(i==sel) rect(pg,7,y-2,128,9,6);
                text(pg,10,y,gl[i],i==sel?C_WHITE:C_GREY,1);
                if(i==G_NAME) text(pg,60,y,kl.name,i==sel?C_GOLD:C_GREY,1);
                else if(i==G_PAINT||i==G_TRIM||i==G_PATC){ u16 c=kl.col[i==G_PAINT?0:i==G_TRIM?1:2]; BG_PAL[224+j]=c; rect(pg,60,y-1,30,7,224+j); frame1(pg,60,y-1,30,7,i==sel?C_WHITE:C_DGREY); }
                else if(i==G_NUM){ scpy(b,"< "); if(kl.number<10) scat(b,"0"); ncat(b,kl.number); scat(b," >"); text(pg,60,y,b,i==sel?C_GOLD:C_GREY,1); }
                else { int v=kl.part[p]; scpy(b,gn[p][v]); b[13]=0; int own=kowns(p,v); text(pg,60,y,b,i==sel?(own?C_GOLD:C_RED):(own?C_GREY:C_DRED),1);
                    if(!own){ int pr=kprice(p,v); if(pr==9999) scpy(b,"WIN IT"); else { scpy(b,""); ncat(b,pr); scat(b,"G"); } text(pg,112,y,b,pr==9999?C_CYAN:C_RED,1); } } }
            if(top>0) text(pg,128,30,"^",C_CYAN,1); if(top+11<NG) text(pg,128,124,"V",C_CYAN,1);
            frame1(pg,140,34,92,62,C_DGREY);
            static const char *const sn[5]={"SPEED","ACCEL","GRIP","WEIGHT","BOOST"};
            for(int k=0;k<5;k++){ int y=100+k*7, v=kstat(&kl,k); text(pg,142,y,sn[k],C_GREY,1); for(int q=0;q<10;q++) rect(pg,170+q*6,y,5,4,q<v?(k==3?C_GOLD:C_CYAN):C_DGREY); }
            int p=gp[sel], own = p<0 || kowns(p,kl.part[p]);
            const char *h1 = sel==G_NAME ? "A: TYPE A NAME" : (sel==G_PAINT||sel==G_TRIM||sel==G_PATC) ? "LEFT/RIGHT: BROWSE   A: MIX YOUR OWN" : sel==G_HORN ? "LEFT/RIGHT: CHANGE   A: TOOT IT" : sel==G_NUM ? "LEFT/RIGHT: CHANGE THE NUMBER" :
                !own ? (kprice(p,kl.part[p])==9999 ? (p==KP_BODY?"WIN A STARBUG CUP FOR THIS":p==KP_TOP&&kl.part[p]==6?"WIN ANY CUP FOR THE CROWN":"WIN 10 RACES FOR THE HALO") : arm==sel ? "A AGAIN TO BUY IT" : "A: BUY IT") : "LEFT/RIGHT: TRY THINGS ON";
            ctext(pg,120,137,h1, (!own && p>=0) ? C_GOLD : C_CYAN);
            ctext(pg,120,145, hn ? "L: TURN  R: COLOURS  SEL: SURPRISE  B: UNDO  START: DONE" : "L: TURN  R: COLOURS  SEL: SURPRISE  B: BACK  START: DONE", C_DGREY);
            if(note_t) ctext(pg,186,90,note,C_GOLD); else { scpy(b,kl.name); ctext(pg,186,88,b,C_WHITE); }
        }
        // the preview: the kart on a turntable, with its trail and glow
        { int cx=186, gy=78; rect(pg,141,35,90,48,1); for(int x=146;x<226;x+=2) pix(pg,x+((t>>2)&1),gy+1,C_DGREY);
          if(kl.part[KP_GLOW]){ BG_PAL[KRC_GLOW]= kl.part[KP_GLOW]==7 ? kglow_col[1+((t>>3)%6)] : kglow_col[kl.part[KP_GLOW]]; for(int y=-2;y<=2;y++){ int w=26-y*y*3; for(int x=-w;x<=w;x+=2) pix(pg,cx+x+(y&1),gy+y,KRC_GLOW); } }
          int tr=kl.part[KP_TRAIL]; if(tr && (t&1)==0){ static const u8 tc[10][2]={{0,0},{C_DGREY,C_GREY},{C_GOLD,C_WHITE},{C_CYAN,C_WHITE},{C_RED,C_RED},{C_RED,C_GOLD},{C_GREY,C_DGREY},{C_GOLD,C_WHITE},{C_RED,C_GOLD},{C_GREEN,C_CYAN}};
              int c = tr==5 ? C_RED+((t>>1)%4==0?0:(t>>1)%4==1?3:(t>>1)%4==2?2:6) : tr==9 ? C_RED+rndi(8) : tc[tr][rndi(2)]; int ex = view ? cx-26 : cx; kpart_add(ex+rndi(5)-2, gy-3, view ? -(2+rndi(3)) : rndi(5)-2, view ? -rndi(3) : 3+rndi(3), 12+rndi(6), c); }
          for(int i=0;i<24;i++){ KPart *q=&kpt[i]; if(!q->life) continue; q->life--; q->x+=q->vx; q->y+=q->vy; int x=q->x>>4, y=q->y>>4; if(x<142||x>229||y<36||y>81){ q->life=0; continue; } pix(pg,x,y,q->c); if(tr==3||tr==4||tr==7){ pix(pg,x+1,y,q->c); pix(pg,x,y+1,q->c); } }
          int sz=kl.part[KP_SIZE], inv = sz==0 ? 150 : sz==2 ? 100 : 120, b=kl.part[KP_BODY], bob=(t>>4)&1; const u8 *an=kb_anchor+b*3;
          set_affine(29,inv,0,0,inv); int kh=16*256/inv, ky=gy-kh/2-bob, slot=0;
          shadow_oam[slot].a0=((ky-16)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((cx-32)&511)|(29<<9)|(2<<14); shadow_oam[slot].a2=(T_KDYN+view*8)|(7<<12); slot++;
          if(kl.part[KP_TOP]){ int tp = view ? an[2] : an[0], tx = view ? cx+(an[1]+3-16)*256/inv : cx, ty=gy-(16-tp)*256/inv-4*256/inv-bob;
              shadow_oam[slot].a0=((ty-8)&255)|(1<<8)|(1<<9)|(1<<14); shadow_oam[slot].a1=((tx-16)&511)|(29<<9); shadow_oam[slot].a2=(T_TOP1+(kl.part[KP_TOP]-1)*2); slot++; }
          int feet=gy-kb_seat[b]*256/inv-bob, dx2 = view ? (12-16)*256/inv : 0;
          if(form==FORM_HUMAN) slot=hum_oam(slot,cx+dx2,feet+4,1,1,0,0,0,0,-1);
          else if(form==FORM_CREATURE){ int fl=form_body==2; set_affine(31,-170,0,0,170); shadow_oam[slot].a0=((feet-(fl?8:16)-(fl?8:16))&255)|(1<<8)|(1<<9)|(fl?(1<<14):0); shadow_oam[slot].a1=((cx+dx2-16)&511)|(31<<9)|(fl?0:(1<<14)); shadow_oam[slot].a2=(form_body==0?T_HOPPER:form_body==1?T_GRAZER:T_FLYER)|(6<<12); slot++; }
          else if(view){ set_affine(31,-210,0,0,210); int dh=16*256/210; bug_obj(slot,cx+dx2,feet-dh/2+3,31,210); shadow_oam[slot].a2=bug_tile(0)|BUG_BANK; slot++; }
          else { set_affine(31,380,0,0,380); int dh=64*256/380; shadow_oam[slot].a0=((feet-dh/2+3-64)&255)|(1<<8)|(1<<9); shadow_oam[slot].a1=((cx-64)&511)|(31<<9)|(3<<14); shadow_oam[slot].a2=T_BACK|BUG_BANK; slot++; }
          for(;slot<128;slot++) shadow_oam[slot].a0=0x0200; }
        vsync(); oam_flush(); read_keys(); t++;
        if(note_t && --note_t==0) dirty=1;
        int p=gp[sel];
        if(HIT(K_UP)){ sel=(sel+NG-1)%NG; dirty=1; arm=-1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%NG; dirty=1; arm=-1; sfx(7); }
        int d=0, fresh=0; if(HELD(K_LEFT)||HELD(K_RIGHT)){ rep++; fresh=rep==1; if(fresh || (rep>14 && !(rep&3) && (sel==G_NUM||sel==G_PAINT||sel==G_TRIM||sel==G_PATC))) d=HELD(K_RIGHT)?1:-1; } else rep=0;
        if(d){ arm=-1; if(fresh) G_PUSH();
            if(p>=0){ kl.part[p]=(u8)((kl.part[p]+d+kpart_n[p])%kpart_n[p]); if(p==KP_HORN) khorn(kl.part[p]); }
            else if(sel==G_NUM) kl.number=(u8)((kl.number+d+100)%100);
            else if(sel==G_PAINT||sel==G_TRIM||sel==G_PATC){ u16 *c=&kl.col[sel==G_PAINT?0:sel==G_TRIM?1:2]; int pi=cc_pidx(*c); pi = pi<0 ? (d>0?0:NPRESET-1) : (pi+d+NPRESET)%NPRESET; *c=preset_col[pi]; }
            if(sel!=G_NAME){ kart_build(0); dirty=1; sfx(2); } }
        if(HIT(K_A)){ dirty=1;
            if(sel==G_NAME){ G_PUSH(); kname_edit(pg,"NAME YOUR KART",kl.name,11); }
            else if(sel==G_PAINT||sel==G_TRIM||sel==G_PATC){ G_PUSH(); oam_hide_all(); oam_flush(); u16 *c=&kl.col[sel==G_PAINT?0:sel==G_TRIM?1:2]; *c=pick_colour(pg,gl[sel],*c); kart_build(0); }
            else if(sel==G_HORN && kowns(p,kl.part[p])) khorn(kl.part[p]);
            else if(p>=0 && !kowns(p,kl.part[p])){ int pr=kprice(p,kl.part[p]);
                if(pr==9999){ scpy(note,"YOU HAVE TO WIN THIS ONE"); note_t=60; sfx(4); }
                else if(gold<pr){ scpy(note,"NOT ENOUGH GOLD"); note_t=60; sfx(4); }
                else if(arm!=sel) arm=sel;
                else { gold-=pr; kown[p]|=(u16)(1u<<kl.part[p]); arm=-1; scpy(note,"BOUGHT! IT IS YOURS"); note_t=70; sfx(6); kart_save(); } } }
        if(HIT(K_L)){ view^=1; for(int i=0;i<24;i++) kpt[i].life=0; sfx(7); }
        if(HIT(K_R)){ G_PUSH(); sch=(sch+1)%9; int h0,s0,v0; cc_hsv(kl.col[0],&h0,&s0,&v0);
            kl.col[1]=cc_rgb(sch>=7?cc_dh[sch][0]:h0+cc_dh[sch][0],cc_ss[sch][0],cc_vv[sch][0]); kl.col[2]=cc_rgb(sch>=7?cc_dh[sch][1]:h0+cc_dh[sch][1],cc_ss[sch][1],cc_vv[sch][1]);
            kart_build(0); scpy(note,"COLOURS: "); scat(note,cc_sch_nm[sch]); note_t=70; dirty=1; sfx(6); }
        if(HIT(K_SEL)){ G_PUSH(); for(int q=0;q<NKPART;q++){ if(q==KP_HORN) continue; int tries=0, v; do { v=rndi(kpart_n[q]); } while(!kowns(q,v) && ++tries<30); if(kowns(q,v)) kl.part[q]=(u8)v; }
            kl.col[0]=cc_rgb(rndi(360),140+rndi(116),150+rndi(106)); int s2=rndi(9); int h0,s0,v0; cc_hsv(kl.col[0],&h0,&s0,&v0);
            kl.col[1]=cc_rgb(s2>=7?cc_dh[s2][0]:h0+cc_dh[s2][0],cc_ss[s2][0],cc_vv[s2][0]); kl.col[2]=cc_rgb(s2>=7?cc_dh[s2][1]:h0+cc_dh[s2][1],cc_ss[s2][1],cc_vv[s2][1]); kl.number=(u8)rndi(100);
            kart_build(0); scpy(note,"SURPRISE!"); note_t=50; dirty=1; sfx(6); }
        if(HIT(K_B)){ if(hn){ hp=(hp+5)%6; hn--; kl=hist[hp]; kart_build(0); scpy(note,"UNDONE"); note_t=40; dirty=1; sfx(4); } else { kl=before; break; } }
        if(HIT(K_START)){ int back=0; for(int q=0;q<NKPART;q++) if(!kowns(q,kl.part[q])){ kl.part[q]=before.part[q]; if(!kowns(q,kl.part[q])) kl.part[q]=0; back++; }
            if(back){ const char *ln[2]={"THE PARTS YOU DID NOT BUY","WENT BACK ON THE SHELF."}; card(pg,"THE GARAGE",ln,2); }
            sfx(6); break; }
    }
    #undef G_PUSH
    in_menu--; kart_build(0); kart_save(); wait_release(); oam_hide_all(); oam_flush(); }
// ---------------------------------------------------------------- the kart camera: as you like it
static void kcam_menu(volatile u16 *pg, int live){
    enum { C_VIEW, C_DIST, C_HGT, C_FOV, C_SWING, C_TURNS, C_ZOOM, C_SHAKE, C_RESET, C_DONE, NC };
    static const char *const cl[NC]={"VIEW","DISTANCE","HEIGHT","FIELD OF VIEW","SWING","LOOK INTO TURNS","SPEED ZOOM","BUMP SHAKE","BACK TO NORMAL","DONE"};
    static const u8 cn[8]={2,5,5,4,4,2,2,2};
    static const char *const vn[2]={"CHASE","COCKPIT"}, *const dn[5]={"CLOSE","NORMAL","FAR","VERY FAR","SATELLITE"}, *const hn[5]={"LOW","NORMAL","HIGH","VERY HIGH","BIRD"},
        *const fn[4]={"NARROW","NORMAL","WIDE","FISHEYE"}, *const swn[4]={"STIFF","SOFT","NORMAL","LOOSE"}, *const on[2]={"OFF","ON"};
    int sel=0, dirty=1, y0 = live ? 19 : 28; in_menu++; wait_release();          // live: the menu sits at the top, your kart shows below
    while(1){
        if(dirty){ dirty=0; char b[40];
            if(live){ kcam_apply(); heading=khd; heading_rays(); cam_h=ground_h(ppx,ppy)+cam_up(); render_planet(pg); oam_hide_all();
                if(!cam_fp){ s32 dz=cam_back(), f=(FOCP<<16)/(dz>>8); int sy=PHZ+(((cam_h+curv(dz>>8)-tiny_sub()-ground_h(ppx,ppy))*f)>>16); kart_draw_me(0,sy); } else kart_cockpit(pg); oam_flush(); }
            box(pg,10,y0,220,live?78:118); rect(pg,11,y0+1,218,10,6); ctext(pg,120,y0+3,"KART CAMERA",C_GOLD);
            for(int i=0;i<NC;i++){ int y=y0+14+i*(live?6:9); if(i==sel) rect(pg,14,y-1,212,live?6:8,6); text(pg,20,y,cl[i],i==sel?C_WHITE:i>=C_RESET?C_CYAN:C_GREY,1);
                if(i<8){ int v=kcam[i]; const char *t = i==0?vn[v&1] : i==1?dn[v%5] : i==2?hn[v%5] : i==3?fn[v&3] : i==4?swn[v&3] : on[v&1]; scpy(b,"< "); scat(b,t); scat(b," >"); text(pg,120,y,b,i==sel?C_GOLD:C_GREY,1); } }
            if(!live) ctext(pg,120,y0+108,"IN A RACE: HOLD SELECT TO LOOK BEHIND",C_DGREY); }
        vsync(); read_keys();
        if(HIT(K_UP)){ sel=(sel+NC-1)%NC; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%NC; dirty=1; sfx(7); }
        int d = HIT(K_RIGHT) ? 1 : HIT(K_LEFT) ? -1 : 0; if(HIT(K_A) && sel<8) d=1;
        if(d && sel<8){ kcam[sel]=(u8)((kcam[sel]+d+cn[sel])%cn[sel]); dirty=1; sfx(2); }
        if(HIT(K_A) && sel==C_RESET){ static const u8 dc[8]={0,1,1,1,1,0,1,1}; for(int i=0;i<8;i++) kcam[i]=dc[i]; dirty=1; sfx(6); }
        if(HIT(K_B) || HIT(K_START) || (HIT(K_A) && sel==C_DONE)) break; }
    in_menu--; if(kr_mode) kcam_apply(); else kfovk=kcam_fov[kcam[3]&3]; kart_save(); wait_release(); }
// ---------------------------------------------------------------- the kart club (the race post, or your ship)
static u8 kc_opt[10]; static int kc_n, kc_post;
static void kc_row(int i, char *b, int *c){
    static const char *const nm[10]={"QUICK RACE","GRAND PRIX (A CUP OF 4 RACES)","TIME TRIAL","YOUR TRACKS","BUILD A TRACK HERE","KART GARAGE","FOOT RACE (FLAGS)","YOUR RECORD","LEAVE","KART CAMERA"};
    int o=kc_opt[i]; scpy(b,nm[o]); if(o==1 && !kc_post){ scat(b,"  (AT A RACE POST)"); *c=C_DGREY; } if(o==3){ int n=0; KRec r; for(int k=0;k<NKREC;k++) if(krec_get(k,&r)) n++; scat(b," ("); ncat(b,n); scat(b,")"); } }
static void kc_desc(int i, char *b, int *c){ int k = i<0 ? -1-i : i; int o=kc_opt[k];
    static const char *const d1[10]={"YOU AND 7 RIVALS ON ONE OF THIS PLANET'S TRACKS.","FOUR RACES. POINTS FOR EVERY PLACE. WIN A TROPHY.","JUST YOU AND THE CLOCK. GOLD, SILVER, BRONZE.","RACE YOUR OWN TRACKS. FLY BACK TO ONES FAR AWAY.",
        "DRIVE ROUND AND DROP GATES. THE ROAD JOINS THEM.","16 BODIES, ANY COLOURS, WHEELS, SPOILERS, TOPPERS...","THE OLD FLAG RACE, ON FOOT.","WINS, CUPS, MEDALS AND TRACKS.","BACK TO THE PLANET.","DISTANCE, HEIGHT, FIELD OF VIEW, SWING, COCKPIT..."};
    static const char *const d2[10]={"UP: GO  B: BOOST  R: DRIFT  A: ITEM  L: HORN","BEAT A CUP TO OPEN THE NEXT CLASS.","","THEY ARE KEPT ON THE CART. ALL SLOTS SHARE THEM.","4 TO 24 GATES, ON ANY PLANET OF ANY SIZE.","LEFT/RIGHT TO TRY THINGS ON. BUY WHAT YOU LOVE.","","","","IN A RACE, PAUSE AND PICK CAMERA TO SEE IT LIVE."};
    scpy(b, i<0 ? d2[o] : d1[o]); (void)c; }
static void kart_menu(int at_post){
    volatile u16 *pg=VISPG; kc_post=at_post; kc_n=0; static const u8 all[10]={0,1,2,3,4,5,9,6,7,8};
    for(int i=0;i<10;i++){ if(all[i]==6 && !at_post) continue; kc_opt[kc_n++]=all[i]; }
    oam_hide_all(); oam_flush(); in_menu++; int sel=0;
    while(1){
        list_desc=kc_desc; char ttl[40]; scpy(ttl,at_post?"THE RACE POST":"KART CLUB"); scat(ttl,"   "); scat(ttl,kpsize_name[wd.psz]); scat(ttl," PLANET");
        int k=list_pick(pg,ttl,kc_n,kc_row,&sel,"A: PICK   B: LEAVE"); list_desc=0; if(k<0) break; int o=kc_opt[k];
        if(o==8) break;
        if(o==0){ int t=kpick_track(pg,"QUICK RACE: PICK A TRACK",1); if(t==-1) continue; int c=kpick_class(pg); if(c<0) continue; k_cls=c; k_type=0;
            if(t>=0){ k_trk=t; k_rec=-1; kt_official(t,&kt); } else { KRec r; k_rec=-2-t; k_trk=-1; krec_get(k_rec,&r); krec_to_kt(&r); k_type=4; }
            in_menu--; kart_go(t>=0?NRC:kt.riv+1,-1,0,1); return; }
        if(o==1){ if(!at_post){ say("CUPS ARE RUN AT A RACE POST, IN A TOWN"); continue; } int c=kpick_class(pg); if(c<0) continue; k_cls=c; in_menu--; kart_cup_start(); return; }
        if(o==2){ int t=kpick_track(pg,"TIME TRIAL: PICK A TRACK",1); if(t==-1) continue; k_cls=1; k_type=2;
            if(t>=0){ k_trk=t; k_rec=-1; kt_official(t,&kt); } else { KRec r; k_rec=-2-t; k_trk=-1; krec_get(k_rec,&r); krec_to_kt(&r); }
            in_menu--; kart_go(1,0,0,1); return; }
        if(o==3){ in_menu--; kmy_tracks(pg); if(kr_mode) return; in_menu++; continue; }
        if(o==4){ in_menu--; kbuild_begin(); return; }
        if(o==5){ kgarage(pg); oam_hide_all(); oam_flush(); continue; }
        if(o==6){ in_menu--; race_start(0); planet_hud(); return; }
        if(o==7){ kart_records(pg); continue; }
        if(o==9){ kcam_menu(pg,0); continue; } }
    in_menu--; planet_hud(); p_last=vbl_count; }

// ---------------------------------------------------------------- buttons while driving
static int kart_buttons(void){                                     // A, SELECT and START in a kart; 1 = a menu was open (the frame starts over)
    if(kr_mode==2){
        if(HIT(K_A)) kbuild_drop(); if(HIT(K_SEL)) kbuild_undo();
        if(HIT(K_START)){ oam_hide_all(); oam_flush(); in_menu++; static const char *const o[4]={"FINISH THE TRACK","KEEP BUILDING","CAMERA","THROW IT AWAY"}; int sel=0; volatile u16 *pg=VISPG;
            while(1){ box(pg,50,46,140,62); rect(pg,51,47,138,12,6); ctext(pg,120,50,"BUILDING",C_GOLD); for(int i=0;i<4;i++){ if(i==sel) text(pg,60,64+i*10,">",C_CYAN,1); text(pg,68,64+i*10,o[i],i==sel?C_WHITE:C_GREY,1); }
                vsync(); read_keys(); if(HIT(K_UP)) sel=(sel+3)%4; if(HIT(K_DOWN)) sel=(sel+1)%4; if(HIT(K_B)){ sel=1; break; } if(HIT(K_A)) break; }
            wait_release(); in_menu--;
            if(sel==0) kbuild_finish(); else if(sel==2) kcam_menu(pg,1); else if(sel==3) kart_stop();
            planet_hud(); p_last=vbl_count; return 1; }
        return 0; }
    if(HIT(K_A) && rc[0].item && ks_state==KS_RACE && !rc[0].spin) kuse_item(0);
    if(HIT(K_START)){ oam_hide_all(); oam_flush(); in_menu++; static const char *const o[4]={"KEEP RACING","START AGAIN","CAMERA","QUIT THE RACE"}; int sel=0; volatile u16 *pg=VISPG;
        while(1){ box(pg,50,46,140,62); rect(pg,51,47,138,12,6); ctext(pg,120,50,"PAUSED",C_GOLD); for(int i=0;i<4;i++){ if(i==sel) text(pg,60,64+i*10,">",C_CYAN,1); text(pg,68,64+i*10,o[i],i==sel?C_WHITE:C_GREY,1); }
            vsync(); read_keys(); if(HIT(K_UP)) sel=(sel+3)%4; if(HIT(K_DOWN)) sel=(sel+1)%4; if(HIT(K_B)||HIT(K_START)){ sel=0; break; } if(HIT(K_A)) break; }
        wait_release(); in_menu--;
        if(sel==2){ kcam_menu(pg,1); sel=0; }
        if(sel==1) kart_go(nrc, nrc>1?-1:0, k_type==1, 0);
        else if(sel==3){ if(k_type==3){ kgive_back(); kr_mode=2; ks_state=0; nrc=1; ppx=kret_x; ppy=kret_y; heading=kret_h; heading_rays(); kbuild_menu(); } else kart_stop(); }
        planet_hud(); p_last=vbl_count; return 1; }
    return 0; }
