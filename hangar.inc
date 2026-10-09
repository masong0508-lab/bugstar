// ================================================================= THE HANGAR: the ship creator screen
static int make_ship(volatile u16 *pg, int first){
    enum { R_NAME, R_HULL, R_LEN, R_WID, R_NOSE, R_WINGS, R_WSIZE, R_WPOS, R_DIHED, R_ENG, R_ESIZE, R_FINS, R_GUN, R_PODS, R_COCK, R_DECAL, R_PAINT, R_TRIM, R_GLOW, NROWS };
    static const char *const rows[NROWS]={"NAME","HULL","LENGTH","WIDTH","NOSE","WINGS","WING SIZE","WING SPOT","WING TILT","ENGINES","ENGINE SIZE","FINS","CANNON","PODS","COCKPIT","DECAL","PAINT","TRIM","GLOW"};
    #define NSHOW 11
    int sel=0, top=0, dirty=1, editing=0, cur=0, t=0, spin=0; char nm[9]; scpy(nm,ship.name); int n=textlen(nm); if(n<1){ scpy(nm,"MOTH"); n=4; }
    ShipLook before=ship; ship_build_model();
    oam_hide_all(); oam_flush(); wait_release();
    while(1){
        if(dirty){ dirty=0; char b[40];
            panel(pg,4,20,232,136, first ? "BUILD YOUR SHIP" : "THE HANGAR");
            if(top>0) text(pg,60,33,"^",C_DGREY,1); if(top+NSHOW<NROWS) text(pg,60,33+NSHOW*9+2,"v",C_DGREY,1);
            for(int k=0;k<NSHOW;k++){ int i=top+k; if(i>=NROWS) break; int y=36+k*9; if(i==sel) rect(pg,8,y-2,112,9,6);
                text(pg,11,y,rows[i], i==sel?C_WHITE:C_GREY,1); const char *opt=0;
                switch(i){ case R_HULL: opt=hull_name[ship.hull]; break; case R_LEN: opt=len_name[ship.len]; break; case R_WID: opt=wid_name[ship.wid]; break; case R_NOSE: opt=nose_name[ship.nose]; break;
                           case R_WINGS: opt=wing_name[ship.wings]; break; case R_WSIZE: opt=wsize_name[ship.wsize]; break; case R_WPOS: opt=wpos_name[ship.wpos]; break; case R_DIHED: opt=dihed_name[ship.dihed]; break;
                           case R_ENG: opt= ship.engines==0?"ONE":ship.engines==1?"TWO":ship.engines==2?"THREE":"FOUR"; break; case R_ESIZE: opt=esize_name[ship.esize]; break; case R_FINS: opt=fins_name[ship.fins]; break;
                           case R_GUN: opt=cannon_name[ship.cannon]; break; case R_PODS: opt=pods_name[ship.pods]; break; case R_COCK: opt=cockpit_name[ship.cockpit]; break; case R_DECAL: opt=decal_name[ship.decal]; break; default: break; }
                if(i==R_NAME){ for(int kk=0;kk<8;kk++){ char ch[2]={ kk<n?nm[kk]:'_',0 }; int c=(editing&&kk==cur)?C_GOLD : kk<n?C_WHITE:C_DGREY; text(pg,60+kk*6,y,ch,c,1); if(editing&&kk==cur&&(t&16)) rect(pg,60+kk*6,y+6,4,1,C_GOLD); } }
                else if(opt){ scpy(b,"<"); scat(b,opt); scat(b,">"); b[15]=0; text(pg,60,y,b,i==sel?C_GOLD:C_GREY,1); }
                else { u16 c = i==R_PAINT?ship.paint : i==R_TRIM?ship.trim : ship.glow; BG_PAL[224+k]=c; rect(pg,60,y-1,44,8,224+k); frame1(pg,60,y-1,44,8,i==sel?C_WHITE:C_DGREY); } }
            int y=136; scpy(b,"SPEED "); ncat(b,ship_top_speed()); scat(b," TURN "); ncat(b,ship_turn()); scat(b,"% ARMOUR "); ncat(b,200-ship_armour()); scat(b,"% MASS "); ncat(b,ship_mass()); text(pg,11,y,b,C_CYAN,1);
            scpy(b,"THRUST "); ncat(b,ship_thrust()); scat(b," GUN +"); ncat(b,ship_gun()); scat(b," HOLD +"); ncat(b,ship_hold_bonus()); scat(b," FUEL +"); ncat(b,ship_fuel_bonus()/10); text(pg,11,y+8,b,C_CYAN,1);
            ctext(pg,120,151, editing ? "UP/DOWN: LETTER  LEFT/RIGHT: MOVE  A: DONE" : "L/R: SPIN   A: COLOUR   SELECT: SURPRISE ME   START: DONE", C_CYAN);
            { char nb[12]; scpy(nb,nm); ctext(pg,178,127,nb,C_GOLD); }
            if(t){ volatile u16 *bp=(volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0:0xA000)); for(int y=36;y<124;y++){ volatile u16 *src=bp+y*120+62, *dst=pg+y*120+62; for(int x=0;x<54;x++) dst[x]=src[x]; } }   // keep the last preview while the list repaints
        }
        t++; if(HELD(K_L)) spin-=12; else if(HELD(K_R)) spin+=12; else spin+=3; spin&=1023;
        // the preview is drawn on the hidden page, then copied over after the vertical blank, so it never tears
        { volatile u16 *bp=(volatile u16*)(0x06000000 + ((REG_DISPCNT&0x10)?0:0xA000));
          rect(bp,124,36,108,88,1); frame1(bp,124,36,108,88,C_DGREY);
          for(int k=0;k<16;k++){ int x=126+((k*37+t/4)%104), yy=38+((k*53)%84); pix(bp,x,yy, (k&3)?2:4); }
          ship_draw(bp,178,80,105,spin,150+(sinx((t*2)&255)>>10),0,0,125,37,231,123);
          vsync();
          for(int y=36;y<124;y++){ volatile u16 *src=bp+y*120+62, *dst=pg+y*120+62; for(int x=0;x<54;x++) dst[x]=src[x]; } }
        read_keys(); if(editing && (t&15)==0) dirty=1;
        if(editing){
            if(HIT(K_UP)||HIT(K_DOWN)){ if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } else { char c=nm[cur]; c = HIT(K_UP) ? (c=='Z'?' ':c==' '?'A':c+1) : (c=='A'?' ':c==' '?'Z':c-1); nm[cur]=c; } sfx(7); dirty=1; }
            if(HIT(K_RIGHT) && cur<7){ cur++; if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } dirty=1; }
            if(HIT(K_LEFT) && cur>0){ cur--; dirty=1; }
            if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ while(n>0 && nm[n-1]==' ') n--; nm[n]=0; if(n==0){ scpy(nm,"MOTH"); n=4; } editing=0; dirty=1; sfx(2); }
            continue; }
        if(HIT(K_UP)){ sel=(sel+NROWS-1)%NROWS; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%NROWS; dirty=1; sfx(7); }
        if(sel<top) top=sel; if(sel>=top+NSHOW) top=sel-NSHOW+1;
        int d = HIT(K_RIGHT)?1 : HIT(K_LEFT)?-1 : (HIT(K_A) && sel>=R_HULL && sel<=R_DECAL) ? 1 : 0;
        if(d){ int ch=1; u8 *f=0; int m=3;
               switch(sel){ case R_HULL: f=&ship.hull; m=NHULL; break; case R_LEN: f=&ship.len; break; case R_WID: f=&ship.wid; break; case R_NOSE: f=&ship.nose; break; case R_WINGS: f=&ship.wings; m=NWING; break;
                 case R_WSIZE: f=&ship.wsize; break; case R_WPOS: f=&ship.wpos; break; case R_DIHED: f=&ship.dihed; break; case R_ENG: f=&ship.engines; m=4; break; case R_ESIZE: f=&ship.esize; break;
                 case R_FINS: f=&ship.fins; break; case R_GUN: f=&ship.cannon; m=4; break; case R_PODS: f=&ship.pods; m=4; break; case R_COCK: f=&ship.cockpit; m=2; break; case R_DECAL: f=&ship.decal; m=4; break; default: ch=0; }
               if(ch && f){ *f=(u8)((*f+d+m)%m); ship_build_model(); dirty=1; sfx(2); } }
        if(HIT(K_A)){ if(sel==R_NAME){ editing=1; cur=0; dirty=1; }
            else if(sel==R_PAINT){ ship.paint=pick_colour(pg,"PAINT",ship.paint); dirty=1; } else if(sel==R_TRIM){ ship.trim=pick_colour(pg,"TRIM",ship.trim); dirty=1; } else if(sel==R_GLOW){ ship.glow=pick_colour(pg,"ENGINE GLOW",ship.glow); dirty=1; } }
        if(HIT(K_SEL)){ ship.hull=rndi(NHULL); ship.len=rndi(3); ship.wid=rndi(3); ship.nose=rndi(3); ship.wings=rndi(NWING); ship.wsize=rndi(3); ship.wpos=rndi(3); ship.dihed=rndi(3); ship.engines=rndi(4); ship.esize=rndi(3); ship.fins=rndi(3); ship.cannon=rndi(4); ship.pods=rndi(4); ship.cockpit=rndi(2); ship.decal=rndi(4);
            ship.paint=preset_col[rndi(NPRESET)]; ship.trim=preset_col[rndi(NPRESET)]; ship.glow=preset_col[rndi(NPRESET)]; ship_build_model(); dirty=1; sfx(6); }
        if(HIT(K_START)) break;
        if(HIT(K_B)){ if(first){ ship=before; ship_build_model(); wait_release(); return 0; } ship=before; scpy(nm,before.name); n=textlen(nm); ship_build_model(); dirty=1; sfx(4); }
    }
    scpy(ship.name,nm); sfx(6); wait_release(); return 1;
}
