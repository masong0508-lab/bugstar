// ================================================================= THE HANGAR: the ship creator screen
// Four tabs (BODY, WINGS, GEAR, STYLE), a live 3D preview you can turn and tilt, a line that says what the chosen part does, and bars that show
// how each choice changes the way the ship flies (compared with the ship you walked in with).
enum { R_NAME, R_PRESET, R_HULL, R_LEN, R_WID, R_NOSE, R_WINGS, R_WSIZE, R_WPOS, R_DIHED, R_FINS, R_ENG, R_ESIZE, R_GUN, R_PODS, R_MODULE, R_TOPPER, R_COCK, R_DECAL, R_SCHEME, R_FINISH, R_PAINT, R_TRIM, R_GLOW, NROWS };
static const char *const hg_row_nm[NROWS]={"NAME","PRESET","HULL","LENGTH","WIDTH","NOSE","WINGS","WING SIZE","WING SPOT","WING TILT","FINS","ENGINES","ENGINE SIZE","CANNON","PODS","MODULE","TOPPER","COCKPIT","DECAL","SCHEME","FINISH","PAINT","TRIM","GLOW"};
static const char *const hg_tab_nm[4]={"BODY","WINGS","GEAR","STYLE"};
static const u8 hg_tab_n[4]={6,5,6,7};
static const u8 hg_tab_row[4][7]={{R_NAME,R_PRESET,R_HULL,R_LEN,R_WID,R_NOSE},{R_WINGS,R_WSIZE,R_WPOS,R_DIHED,R_FINS},{R_ENG,R_ESIZE,R_GUN,R_PODS,R_MODULE,R_TOPPER},{R_COCK,R_DECAL,R_SCHEME,R_FINISH,R_PAINT,R_TRIM,R_GLOW}};
static const char *const hg_st_nm[6]={"SPEED","TURN","ARMOUR","LASER","HOLD","FUEL"};
static const u8 hg_st_max[6]={90,150,150,12,30,50};
static const u8 hg_st_col[6]={C_CYAN,C_GREEN,C_GOLD,C_RED,C_GOLD,C_CYAN};
// what each part does, in a few words (the first line is for the whole row, the lists are for the choices that change how the ship flies)
static const char *const hg_row_ds[NROWS]={"TAP A TO TYPE A NAME. UP TO 8 LETTERS","A READY-MADE SHIP. IT KEEPS YOUR NAME","","LONGER IS ROOMIER BUT HEAVIER","WIDER IS ROOMIER BUT HEAVIER","HOW THE FRONT OF THE HULL LOOKS","","BIGGER WINGS TURN BETTER","WHERE THE WINGS SIT ALONG THE BODY","WING TILT: DOWN, FLAT OR UP","FINS SETTLE THE TURNS FASTER","MORE ENGINES: MORE THRUST AND WEIGHT","BIG ENGINES PUSH HARDER","","","","A LOOK-ONLY FITTING ON THE ROOF","THE CANOPY. IT ONLY CHANGES THE LOOK","PAINTED MARKINGS. LOOK ONLY","WHICH PARTS WEAR THE TRIM COLOUR","HOW THE PAINT CATCHES THE LIGHT","THE MAIN COLOUR","THE SECOND COLOUR","ENGINE FLAMES AND CANOPY GLASS"};
static const char *const hg_hull_ds[NHULL]={"A BALANCED ALL-ROUNDER","A WIDE FLAT DISC WITH EXTRA HOLD","HEAVY AND TOUGH WITH A HUGE HOLD","LIGHT AND QUICK","THE FASTEST, BUT FRAGILE","FLAT AND AGILE WITH A SMALL HOLD","TWO HULLS: THE BIGGEST HOLD","TOUGH, WITH A BIG FUEL TANK"};
static const char *const hg_wing_ds[NWING]={"NO WINGS: SLOW TO TURN","STEADY AND EASY TO FLY","SWEPT BACK: FAST AND SHARP","DELTA: BEST OF THE CLASSIC WINGS","FOUR WINGS: QUICK AND LIGHT","FORWARD SWEPT: QUICKEST TURNS","BENT WINGS: GOOD, SMOOTH TURNS"};
static const char *const hg_gun_ds[NCANNON]={"NO EXTRA LASER DAMAGE","+2 LASER DAMAGE","+4 LASER DAMAGE FROM THE WINGS","+8 LASER DAMAGE BUT HEAVY","+5 LASER DAMAGE FROM THE NOSE","+3 LASER DAMAGE. A LIGHT TURRET","+9 LASER DAMAGE BUT VERY HEAVY"};
static const char *const hg_pod_ds[4]={"NO PODS","MISSILE PODS. THEY ONLY LOOK FIERCE","SCOOPS ON THE SIDES. LOOK ONLY","FUEL TANKS: +30 FUEL"};
static const char *const hg_mod_ds[NMODULE]={"NOTHING EXTRA FITTED","ARMOUR: 15% LESS DAMAGE. HEAVY","CARGO RACK: +10 HOLD","GYRO RING: SHARPER TURNS","TARGET DISH: +3 LASER DAMAGE","BOOSTERS: MORE THRUST","STORMCLOCK COIL: FASTER THAN LIGHT"};
static u8 *hg_field(int r, int *m){ *m=3;       // which byte a row changes, and how many choices it has
    switch(r){ case R_HULL: *m=NHULL; return &ship.hull; case R_LEN: return &ship.len; case R_WID: return &ship.wid; case R_NOSE: return &ship.nose; case R_WINGS: *m=NWING; return &ship.wings; case R_WSIZE: return &ship.wsize; case R_WPOS: return &ship.wpos; case R_DIHED: return &ship.dihed;
        case R_FINS: return &ship.fins; case R_ENG: *m=4; return &ship.engines; case R_ESIZE: return &ship.esize; case R_GUN: *m=NCANNON; return &ship.cannon; case R_PODS: *m=4; return &ship.pods; case R_MODULE: *m=coil_avail()?NMODULE:NMODULE-1; return &shpx.module; case R_TOPPER: *m=NTOPPER; return &shpx.topper;
        case R_COCK: *m=NCOCK; return &ship.cockpit; case R_DECAL: *m=NDECAL; return &ship.decal; case R_SCHEME: *m=NSCHEME; return &shpx.scheme; case R_FINISH: *m=NFINISH; return &shpx.finish; default: break; }
    return 0; }
static const char *hg_opt(int r){
    switch(r){ case R_HULL: return hull_name[ship.hull]; case R_LEN: return len_name[ship.len]; case R_WID: return wid_name[ship.wid]; case R_NOSE: return nose_name[ship.nose]; case R_WINGS: return wing_name[ship.wings]; case R_WSIZE: return wsize_name[ship.wsize]; case R_WPOS: return wpos_name[ship.wpos]; case R_DIHED: return dihed_name[ship.dihed];
        case R_FINS: return fins_name[ship.fins]; case R_ENG: return ship.engines==0?"ONE":ship.engines==1?"TWO":ship.engines==2?"THREE":"FOUR"; case R_ESIZE: return esize_name[ship.esize]; case R_GUN: return cannon_name[ship.cannon]; case R_PODS: return pods_name[ship.pods]; case R_MODULE: return module_name[shpx.module]; case R_TOPPER: return topper_name[shpx.topper];
        case R_COCK: return cockpit_name[ship.cockpit]; case R_DECAL: return decal_name[ship.decal]; case R_SCHEME: return scheme_name[shpx.scheme]; case R_FINISH: return finish_name[shpx.finish]; default: break; }
    return 0; }
static const char *hg_desc(int r){
    switch(r){ case R_HULL: return hg_hull_ds[ship.hull]; case R_WINGS: return hg_wing_ds[ship.wings]; case R_GUN: return hg_gun_ds[ship.cannon]; case R_PODS: return hg_pod_ds[ship.pods&3]; case R_MODULE: return hg_mod_ds[shpx.module]; default: break; }
    return hg_row_ds[r]; }
static void hg_bars(volatile u16 *pg, const int *v, const int *o){       // the six bars: how good the ship is at each thing, and (in green or red) how that differs from when you came in
    for(int i=0;i<6;i++){ int y=103+i*7; char b[8]; text(pg,126,y,hg_st_nm[i],C_GREY,1); ui_bar(pg,154,y+1,44,5,v[i],hg_st_max[i],hg_st_col[i]);
        b[0]=0; ncat(b,v[i]); text(pg,202,y,b,C_WHITE,1); int d=v[i]-o[i]; if(d){ b[0]=0; scat(b,d>0?"+":"-"); ncat(b,d>0?d:-d); text(pg,218,y,b,d>0?C_GREEN:C_RED,1); } } }
#define HG_PX 124
#define HG_PY 36
#define HG_PW 108
#define HG_PH 64
static void hg_copy(volatile u16 *pg, volatile u16 *bp){ for(int y=HG_PY;y<HG_PY+HG_PH;y++){ volatile u16 *src=bp+y*120+HG_PX/2, *dst=pg+y*120+HG_PX/2; for(int x=0;x<HG_PW/2;x++) dst[x]=src[x]; } }
static int make_ship(volatile u16 *pg, int first){
    char nm[9]; scpy(nm,ship.name); int n=textlen(nm); if(n<1){ scpy(nm,"MOTH"); n=4; }
    ShipLook before=ship; ShipX beforex=shpx; ship_build_model();
    int tab=0, row[4]={0,0,0,0}, dirty=1, editing=0, cur=0, t=0, spin=0, tilt=26, pre=-1, selp=0, selused=0, old[6], now[6]; ship_stats(old);         // old: what the ship could do when you came in
    oam_hide_all(); oam_flush(); wait_release();
    while(1){
        int r=hg_tab_row[tab][row[tab]], selh = !editing && HELD(K_SEL);
        if(dirty){ dirty=0; char b[40];
            panel(pg,4,20,232,128, first ? "BUILD YOUR SHIP" : "THE HANGAR");
            for(int k=0;k<4;k++){ int x=8+k*28; if(k==tab) ui_sel(pg,x,36,27,11,U_SEL); ctext(pg,x+13,39,hg_tab_nm[k],k==tab?C_WHITE:C_GREY); }
            rect(pg,8,48,112,1,U_RAISE);
            for(int i=0;i<hg_tab_n[tab];i++){ int rr=hg_tab_row[tab][i], y=52+i*11, on=(i==row[tab]); if(on) ui_sel(pg,8,y-2,112,11,U_SEL);
                text(pg,14,y,hg_row_nm[rr], on?C_WHITE:C_GREY,1);
                if(rr==R_NAME){ for(int kk=0;kk<8;kk++){ char ch[2]={ kk<n?nm[kk]:'_',0 }; int c=(editing&&kk==cur)?C_GOLD : kk<n?(on?C_GOLD:C_GREY):C_DGREY; text(pg,58+kk*6,y,ch,c,1); if(editing&&kk==cur&&(t&16)) rect(pg,58+kk*6,y+6,4,1,C_GOLD); } }
                else if(rr>=R_PAINT){ u16 c = rr==R_PAINT?ship.paint : rr==R_TRIM?ship.trim : ship.glow; BG_PAL[224+i]=c; rect(pg,58,y-1,44,8,224+i); frame1(pg,58,y-1,44,8,on?C_WHITE:C_DGREY); }
                else { const char *opt = rr==R_PRESET ? (pre>=0 ? ship_pre[pre].nm : "CUSTOM") : hg_opt(rr); scpy(b,"<"); scat(b,opt); scat(b,">"); b[15]=0; text(pg,58,y,b,on?C_GOLD:C_GREY,1); } }
            ship_stats(now); hg_bars(pg,now,old);
            if(!editing){ const char *ds=hg_desc(r); if(ds[0]) wrap_text(pg,10,129,28,ds,C_GREY,2); }
            if(editing) ui_foot(pg,"UP/DOWN: LETTER  LEFT/RIGHT: MOVE","A: DONE");
            else if(selh) ui_foot(pg,"LEFT/RIGHT: TURN  UP/DOWN: TILT",0);
            else ui_foot(pg,"A: CHANGE  L/R: TAB","SEL: SURPRISE  START: DONE");
            if(t) hg_copy(pg,(volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0:0xA000))); }         // keep the last preview while the list repaints
        t++; if(!selh) spin+=3; spin&=1023;
        // the preview is drawn on the hidden page, then copied over after the vertical blank, so it never tears
        { volatile u16 *bp=(volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0:0xA000)); char b[20];
          ui_well(bp,HG_PX,HG_PY,HG_PW,HG_PH);
          for(int k=0;k<14;k++){ int x=127+((k*37+t/4)%102), yy=39+((k*53)%58); pix(bp,x,yy, (k&3)?U_RAISE:U_SOFT); }
          { int hs=38*256/sh_rad; if(hs<70) hs=70; if(hs>150) hs=150; frames++; sh_flame=1; ship_draw(bp,178,68,hs,spin,tilt+(sinx((t*2)&255)>>11),0,0,HG_PX+1,HG_PY+1,HG_PX+HG_PW-1,HG_PY+HG_PH-1); sh_flame=0; }
          { char sn[12]; scpy(sn,nm); text(bp,128,HG_PY+HG_PH-9,sn,C_GOLD,1); scpy(b,"MASS "); ncat(b,ship_mass()); rtext(bp,HG_PX+HG_PW-4,HG_PY+HG_PH-9,b,C_GREY); }
          if(selh) text(bp,128,HG_PY+3,"VIEW",C_CYAN,1); else text(bp,128,HG_PY+3,"HOLD SEL: VIEW",U_RAISE,1);
          vsync(); hg_copy(pg,bp); }
        read_keys(); if(editing && (t&15)==0) dirty=1;
        { int sh=!editing && HELD(K_SEL); if(sh && !selp){ selused=0; dirty=1; }
          if(!sh && selp){ dirty=1; if(!selused && !editing){                                                                                                  // a quick tap of SELECT: surprise me
              ship.hull=rndi(NHULL); ship.len=rndi(3); ship.wid=rndi(3); ship.nose=rndi(3); ship.wings=rndi(NWING); ship.wsize=rndi(3); ship.wpos=rndi(3); ship.dihed=rndi(3); ship.engines=rndi(4); ship.esize=rndi(3); ship.fins=rndi(3); ship.cannon=rndi(NCANNON); ship.pods=rndi(4); ship.cockpit=rndi(NCOCK); ship.decal=rndi(NDECAL);
              shpx.module=rndi(coil_avail()?NMODULE:NMODULE-1); shpx.topper=rndi(NTOPPER); shpx.scheme=rndi(NSCHEME); shpx.finish=rndi(NFINISH);
              ship.paint=preset_col[rndi(NPRESET)]; ship.trim=preset_col[rndi(NPRESET)]; ship.glow=preset_col[rndi(NPRESET)]; pre=-1; ship_build_model(); sfx(6); } }
          selp=sh; }
        if(editing){
            if(HIT(K_UP)||HIT(K_DOWN)){ if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } else { char c=nm[cur]; c = HIT(K_UP) ? (c=='Z'?' ':c==' '?'A':c+1) : (c=='A'?' ':c==' '?'Z':c-1); nm[cur]=c; } sfx(7); dirty=1; }
            if(HIT(K_RIGHT) && cur<7){ cur++; if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } dirty=1; }
            if(HIT(K_LEFT) && cur>0){ cur--; dirty=1; }
            if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ while(n>0 && nm[n-1]==' ') n--; nm[n]=0; if(n==0){ scpy(nm,"MOTH"); n=4; } editing=0; dirty=1; sfx(2); }
            continue; }
        if(selp){                                                                                         // holding SELECT: the arrows turn and tilt the view
            if(HELD(K_LEFT)){ spin-=14; selused=1; } if(HELD(K_RIGHT)){ spin+=14; selused=1; }
            if(HELD(K_UP)){ if(tilt<200) tilt+=3; selused=1; } if(HELD(K_DOWN)){ if(tilt>-90) tilt-=3; selused=1; }
            continue; }
        if(HIT(K_R)){ tab=(tab+1)&3; dirty=1; sfx(7); } if(HIT(K_L)){ tab=(tab+3)&3; dirty=1; sfx(7); }
        { int c=hg_tab_n[tab]; if(HIT(K_UP)){ row[tab]=(row[tab]+c-1)%c; dirty=1; sfx(7); } if(HIT(K_DOWN)){ row[tab]=(row[tab]+1)%c; dirty=1; sfx(7); } }
        r=hg_tab_row[tab][row[tab]];
        { int d = HIT(K_RIGHT)?1 : HIT(K_LEFT)?-1 : (HIT(K_A) && r!=R_NAME && r<R_PAINT) ? 1 : 0;
          if(d){ if(r==R_PRESET){ pre = pre<0 ? (d>0?0:NSHP-1) : (pre+d+NSHP)%NSHP; ship_apply_preset(pre); dirty=1; sfx(6); }
                 else if(r!=R_NAME && r<R_PAINT){ int m; u8 *f=hg_field(r,&m); if(f){ *f=(u8)((*f+d+m)%m); pre=-1; ship_build_model(); dirty=1; sfx(2); } } } }
        if(HIT(K_A)){ if(r==R_NAME){ editing=1; cur=0; dirty=1; }
            else if(r==R_PAINT){ ship.paint=pick_colour(pg,"PAINT",ship.paint); pre=-1; dirty=1; } else if(r==R_TRIM){ ship.trim=pick_colour(pg,"TRIM",ship.trim); pre=-1; dirty=1; } else if(r==R_GLOW){ ship.glow=pick_colour(pg,"ENGINE GLOW",ship.glow); pre=-1; dirty=1; } }
        if(HIT(K_START)) break;
        if(HIT(K_B)){ if(first){ ship=before; shpx=beforex; ship_build_model(); wait_release(); return 0; } ship=before; shpx=beforex; scpy(nm,before.name); n=textlen(nm); pre=-1; ship_build_model(); dirty=1; sfx(4); }
    }
    scpy(ship.name,nm); sfx(6); wait_release(); return 1;
}
