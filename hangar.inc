// ================================================================= THE HANGAR: the ship creator screen
// ---------------------------------------------------------------- the hangar: design your ship
static int make_ship(volatile u16 *pg, int first){
    enum { R_NAME, R_HULL, R_WINGS, R_WSIZE, R_ENG, R_FINS, R_GUN, R_COCK, R_PAINT, R_TRIM, NROWS };
    static const char *const rows[NROWS]={"NAME","HULL","WINGS","WING SIZE","ENGINES","FINS","CANNON","COCKPIT","PAINT","TRIM"};
    int sel=0, dirty=1, editing=0, cur=0, t=0, spin=0, hold=0; char nm[9]; scpy(nm,ship.name); int n=textlen(nm); if(n<1){ scpy(nm,"MOTH"); n=4; }
    ShipLook before=ship; ship_build_model();
    oam_hide_all(); oam_flush(); wait_release();
    while(1){
        if(dirty){ dirty=0; char b[40];
            panel(pg,12,20,216,136, first ? "BUILD YOUR SHIP" : "THE HANGAR");
            for(int i=0;i<NROWS;i++){ int y=36+i*9; if(i==sel) rect(pg,16,y-2,118,9,6);
                text(pg,20,y,rows[i], i==sel?C_WHITE:C_GREY,1); const char *opt=0;
                switch(i){ case R_HULL: opt=hull_name[ship.hull]; break; case R_WINGS: opt=wing_name[ship.wings]; break; case R_WSIZE: opt=wsize_name[ship.wsize]; break;
                           case R_ENG: opt= ship.engines==0?"ONE":ship.engines==1?"TWO":ship.engines==2?"THREE":"FOUR"; break; case R_FINS: opt=fins_name[ship.fins]; break;
                           case R_GUN: opt=cannon_name[ship.cannon]; break; case R_COCK: opt=cockpit_name[ship.cockpit]; break; default: break; }
                if(i==R_NAME){ for(int k=0;k<8;k++){ char ch[2]={ k<n?nm[k]:'_',0 }; int c=(editing&&k==cur)?C_GOLD : k<n?C_WHITE:C_DGREY; text(pg,62+k*6,y,ch,c,1); if(editing&&k==cur&&(t&16)) rect(pg,62+k*6,y+6,4,1,C_GOLD); } }
                else if(opt){ scpy(b,"<"); scat(b,opt); scat(b,">"); text(pg,62,y,b,i==sel?C_GOLD:C_GREY,1); }
                else { BG_PAL[224+i]= i==R_PAINT?ship.paint:ship.trim; rect(pg,62,y-1,40,8,224+i); frame1(pg,62,y-1,40,8,i==sel?C_WHITE:C_DGREY); } }
            // the stats card
            int y=128; scpy(b,"SPEED "); ncat(b,ship_top_speed()); scat(b,"  TURN "); ncat(b,ship_turn()); scat(b,"%  ARMOUR "); ncat(b,200-ship_armour()); scat(b,"%"); text(pg,20,y,b,C_CYAN,1);
            scpy(b,"GUN +"); ncat(b,ship_gun()); scat(b,"  HOLD +"); ncat(b,ship_hold_bonus()); scat(b,"  SETTLE "); ncat(b,ship_damp()); text(pg,20,y+8,b,C_CYAN,1);
            ctext(pg,120,146, editing ? "UP/DOWN: LETTER  LEFT/RIGHT: MOVE  A: DONE" : first ? "L/R: SPIN   SELECT: SURPRISE ME   START: DONE" : "L/R: SPIN   SELECT: SURPRISE   START: DONE   B: UNDO", C_CYAN);
            { char nb[12]; scpy(nb,nm); ctext(pg,183,118,nb,C_GOLD); }
        }
        // the preview: the ship turns slowly, L/R spin it
        t++; if(HELD(K_L)) spin-=12; else if(HELD(K_R)) spin+=12; else if(!hold) spin+=3; spin&=1023;
        rect(pg,146,36,74,78,1); frame1(pg,146,36,74,78,C_DGREY);
        BG_PAL[236]=ship.paint; BG_PAL[237]=ship.trim;
        for(int k=0;k<12;k++){ int x=148+((k*37+t/4)%70), yy=38+((k*53)%74); pix(pg,x,yy, (k&3)?2:4); }
        ship_draw(pg,183,74,70,spin,140+(sinx((t*2)&255)>>10),236,237,147,37,219,113);
        vsync(); read_keys(); if(editing && (t&15)==0) dirty=1;
        if(editing){
            if(HIT(K_UP)||HIT(K_DOWN)){ if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } else { char c=nm[cur]; c = HIT(K_UP) ? (c=='Z'?' ':c==' '?'A':c+1) : (c=='A'?' ':c==' '?'Z':c-1); nm[cur]=c; } sfx(7); dirty=1; }
            if(HIT(K_RIGHT) && cur<7){ cur++; if(cur>=n){ nm[cur]='A'; n=cur+1; nm[n]=0; } dirty=1; }
            if(HIT(K_LEFT) && cur>0){ cur--; dirty=1; }
            if(HIT(K_A)||HIT(K_B)||HIT(K_START)){ while(n>0 && nm[n-1]==' ') n--; nm[n]=0; if(n==0){ scpy(nm,"MOTH"); n=4; } editing=0; dirty=1; sfx(2); }
            continue; }
        if(HIT(K_UP)){ sel=(sel+NROWS-1)%NROWS; dirty=1; sfx(7); } if(HIT(K_DOWN)){ sel=(sel+1)%NROWS; dirty=1; sfx(7); }
        int d = HIT(K_RIGHT)?1 : HIT(K_LEFT)?-1 : (HIT(K_A) && sel>=R_HULL && sel<=R_COCK) ? 1 : 0;
        if(d){ int ch=1; switch(sel){ case R_HULL: ship.hull=(ship.hull+d+NHULL)%NHULL; break; case R_WINGS: ship.wings=(ship.wings+d+NWING)%NWING; break; case R_WSIZE: ship.wsize=(ship.wsize+d+3)%3; break;
                 case R_ENG: ship.engines=(ship.engines+d+4)%4; break; case R_FINS: ship.fins=(ship.fins+d+3)%3; break; case R_GUN: ship.cannon=(ship.cannon+d+4)%4; break; case R_COCK: ship.cockpit^=1; break; default: ch=0; }
               if(ch){ ship_build_model(); dirty=1; sfx(2); } }
        if(HIT(K_A)){ if(sel==R_NAME){ editing=1; cur=0; dirty=1; }
            else if(sel==R_PAINT){ ship.paint=pick_colour(pg,"PAINT",ship.paint); dirty=1; } else if(sel==R_TRIM){ ship.trim=pick_colour(pg,"TRIM",ship.trim); dirty=1; } }
        if(HIT(K_SEL)){ ship.hull=rndi(NHULL); ship.wings=rndi(NWING); ship.wsize=rndi(3); ship.engines=rndi(4); ship.fins=rndi(3); ship.cannon=rndi(4); ship.cockpit=rndi(2); ship.paint=preset_col[rndi(NPRESET)]; ship.trim=preset_col[rndi(NPRESET)]; ship_build_model(); dirty=1; sfx(6); }
        if(HIT(K_START)) break;
        if(HIT(K_B)){ if(first){ ship=before; ship_build_model(); wait_release(); return 0; } ship=before; scpy(nm,before.name); n=textlen(nm); ship_build_model(); dirty=1; sfx(4); }
    }
    scpy(ship.name,nm); sfx(6); wait_release(); return 1;
}
