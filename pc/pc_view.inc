// pc_view.inc: the SEED VIEWER. It is #included at the end of the generated main.c, so it can call the game's own functions.
//   bugstar_pc --view --universe HEX --cx HEX --cy HEX --star N --planet N --moon N [--out prefix] [--scale N]
// The address is the game's own: universe (256 bits: uni_w), the cluster east/north of home (5 words each: clus[0..9], 155 bits each),
// a star in that cluster (0..31), a planet of that star, a moon of that planet. It sets the game up exactly as a landing does
// (land_on_current -> new_planet -> planet_enter), then writes: <prefix>_view0..3.ppm (north/east/south/west, CLASSIC look, scaled with
// sharp pixels), <prefix>_card.ppm (the facts, in the game's own font) and the same facts as text on stdout.
#define UNI_W_FULL 64                                              // 2048 bits
#define AX_W_FULL 39                                               // 1240 bits
#define UTOP_PC 0x7FFFFFFu                                         // the game keeps 27 bits in the top word of an axis (155 = 4 x 32 + 27)
static void pv_hex(const char *s, u32 *w, int nw){
    memset(w,0,nw*4); int n=(int)strlen(s);
    for(int i=0;i<n;i++){ char c=s[n-1-i]; int v = c>='0'&&c<='9' ? c-'0' : (c|32)>='a'&&(c|32)<='f' ? (c|32)-'a'+10 : -1; if(v<0) continue; int bit=i*4; if(bit/32<nw) w[bit/32]|=(u32)v<<(bit%32); }
}
static void pv_hexs(char *o, const u32 *w, int nw){ int top=nw-1; while(top>0 && !w[top]) top--; char *p=o; p+=sprintf(p,"%X",w[top]); for(int i=top-1;i>=0;i--) p+=sprintf(p,"%08X",w[i]); }
int pc_view_main(int argc, char **argv){
    const char *crcode=0, *universe="1", *cx="0", *cy="0", *out="seed"; int star=6, planet=0, moon=0, scale=4;
    for(int i=1;i<argc;i++){ const char *a=argv[i], *v=i+1<argc?argv[i+1]:"";
        if(!strcmp(a,"--universe")){ universe=v; i++; } else if(!strcmp(a,"--cx")){ cx=v; i++; } else if(!strcmp(a,"--cy")){ cy=v; i++; }
        else if(!strcmp(a,"--star")){ star=atoi(v); i++; } else if(!strcmp(a,"--planet")){ planet=atoi(v); i++; } else if(!strcmp(a,"--moon")){ moon=atoi(v); i++; }
        else if(!strcmp(a,"--laws")){ plw_load(v); i++; } else if(!strcmp(a,"--law")){ plw_setk(v,1); i++; } else if(!strcmp(a,"--preset")){ plw_preset_apply(atoi(v)); i++; }
        else if(!strcmp(a,"--crcode")){ crcode=v; i++; }
        else if(!strcmp(a,"--out")){ out=v; i++; } else if(!strcmp(a,"--scale")){ scale=atoi(v); i++; } }
    if(scale<1) scale=1; if(scale>8) scale=8;
    settings_load(); music_apply(); opt_music=0; opt_sfx=0;                 // (no sound in the viewer)
    new_game(); bug_look_default(); humx_default(); ship_default(); ship_build_model();
    in_game=1; scene=0; cam_fp=0;
    // ---- the address
    // The PC content library reads a 2048-bit universe and two 1240-bit sides; the game's own address is 256 and 155 bits. The bits above the game's are
    // handed to pcx (pcx_hi_set), which also folds them into the game's words, so a high bit changes the world too. With no high bits nothing changes.
    u32 ax[UAX], ucont[UW], cxo[UAX], cyo[UAX]; { static u32 uf[UNI_W_FULL], xf[AX_W_FULL], yf[AX_W_FULL];
        pv_hex(universe,uf,UNI_W_FULL); if(crcode){ CrRecipe cr0; if(!cr_decode(crcode,&cr0)){ printf("bad creator code\n"); return 1; } pc_cr_rec=cr0; uint32_t w4[4]; cr_uni4(&cr0,w4); for(int i=0;i<UNI_W_FULL;i++) uf[i]= i<4 ? w4[i] : 0; } pv_hex(cx,xf,AX_W_FULL); pv_hex(cy,yf,AX_W_FULL); xf[AX_W_FULL-1]&=0xFFu; xf[4]&=UTOP_PC; yf[4]&=UTOP_PC;   // (1240 bits: the last word keeps 24 bits)
        for(int i=0;i<UW;i++) uni_w[i]=ucont[i]=uf[i]; for(int i=0;i<UAX;i++){ cxo[i]=xf[i]; cyo[i]=yf[i]; }
        pcx_hi_set(uf+UW,xf+UAX,yf+UAX);
        { u32 fu[UW], fx[UAX], fy[UAX]; for(int i=0;i<UW;i++) fu[i]=uni_w[i]; for(int i=0;i<UAX;i++){ fx[i]=cxo[i]; fy[i]=cyo[i]; }
          pcx_fold(fu,fx,fy); for(int i=0;i<UW;i++) uni_w[i]=fu[i]; for(int i=0;i<UAX;i++){ cxo[i]=fx[i]; cyo[i]=fy[i]; } }
        for(int i=0;i<UAX;i++) ax[i]=cxo[i]; ax_put(0,ax); for(int i=0;i<UAX;i++) ax[i]=cyo[i]; ax_put(1,ax); }
    galaxy_no=0; memset(galaxy_hi,0,sizeof galaxy_hi); for(int i=2*UAX;i<NCW;i++) clus[i]=0;
    u_settle(); gen_cluster();
    ant_king=1; zones_open=6;
    if(star<0||star>=NSYS) star=NSYS-1;
    cur_sys=star; cur_pl=planet; cur_moon=moon; cur_rock=0;
    land_on_current(); new_planet(); planet_enter();
    // ---- the facts
    char nm[40], sn[40], gn[48], hx[80], ux[80], cxs[40], cys[40], dist[96];
    scpy(nm,world_name()); sys_name(cur_sys,sn); gal_name(gn);
    ax_get(0,ax); pv_hexs(cxs,ax,UAX); ax_get(1,ax); pv_hexs(cys,ax,UAX); pv_hexs(ux,uni_w,UW); home_ly(dist);
    int np=sys_nplan(cur_sys), nmo=sys_nmoon(cur_sys,cur_pl);
    printf("SEEDVIEW universe=%s cluster_x=%s cluster_y=%s star=%d planet=%d moon=%d\n",ux,cxs,cys,cur_sys,cur_pl,cur_moon);
    printf("galaxy: %s%s\nstar: %s (id %08X)\nworld: %s\nclass: %s  mood: %s\nsize: %d  gravity: %d  water: %d%%  hour: %s  weather: %s\n",
        gn,galaxy_dense()?" (CROWDED)":"",sn,sys_id(cur_sys),nm,pc_name[wd.cls],mood_name[wd.mood],wd.psz,wd.grav,water_lv,hour_name[wd.hour],weather_name[wd.weather]);
    printf("land: %s  city: %d  snow: %d  ruins: %d  weird: %d\nplanets at this star: %d  moons of this planet: %d  danger level: %d\nfrom home: %s\nterrain seed: %08X  city seed: %08X\n",
        style_name[wd.style],wd.city,wd.snow,wd.ruins,wd.weird,np,nmo,w_dlv,dist,wd.tseed,wd.cseed);
    // ---- the PC content library's pages for this place (same text as the in-game WORLD FILE), from the ORIGINAL address (before folding)
    { u32 cxa[5], cya[5], uo[UW]; { static u32 uf2[UNI_W_FULL], xf2[AX_W_FULL], yf2[AX_W_FULL]; pv_hex(universe,uf2,UNI_W_FULL); pv_hex(cx,xf2,AX_W_FULL); pv_hex(cy,yf2,AX_W_FULL); xf2[4]&=UTOP_PC; yf2[4]&=UTOP_PC;
          for(int i=0;i<UW;i++) uo[i]=uf2[i]; for(int i=0;i<5;i++){ cxa[i]=xf2[i]; cya[i]=yf2[i]; } }
      pcx_world(uo,cxa,cya,cur_sys,cur_pl,cur_moon,pc_applied,&pcw);
      printf("PCX planet=%s seed=%016llx pages=%d\n",pcw.planet,(unsigned long long)pcw.seed,pcw.npages);
      for(int pg2=0;pg2<pcw.npages;pg2++){ printf("PCX page %d: %s\n",pg2+1,pcw.page[pg2].title); for(int i=0;i<pcw.page[pg2].n;i++) printf("PCX   %s\n",pcw.page[pg2].t[i]); } }
    // ---- the four views
    for(int k=0;k<4;k++){ heading=k*256; heading_rays(); planet_frame(); planet_frame(); planet_frame();
        char fn[600]; snprintf(fn,sizeof fn,"%s_view%d.ppm",out,k); pc_write_ppm(fn,scale);
        printf("view%d screen=%016llx\n",k,(unsigned long long)pc_hash_screen()); }
    // ---- the card, in the game's own font
    { volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10); rect(pg,0,0,240,160,0); char b[96]; int y=6;
      text(pg,6,y,nm,C_WHITE,2); y+=18;
      scpy(b,pc_name[wd.cls]); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"STAR "); scat(b,sn); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,gn); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,style_name[wd.style]); scat(b,". "); scat(b,hour_name[wd.hour]); scat(b,". "); scat(b,weather_name[wd.weather]); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"WATER "); ncat(b,water_lv); scat(b,"%  GRAVITY "); ncat(b,wd.grav); scat(b,"  SIZE "); ncat(b,wd.psz); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"PLANETS "); ncat(b,np); scat(b,"  MOONS "); ncat(b,nmo); scat(b,"  DANGER "); ncat(b,w_dlv); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"FROM HOME: "); scat(b,dist); text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"UNIVERSE "); scat(b,ux); b[40]=0; text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"CLUSTER E "); scat(b,cxs); b[40]=0; text(pg,6,y,b,C_WHITE,1); y+=10;
      scpy(b,"CLUSTER N "); scat(b,cys); b[40]=0; text(pg,6,y,b,C_WHITE,1);
      char fn[600]; snprintf(fn,sizeof fn,"%s_card.ppm",out); pc_write_ppm(fn,scale); }
    { volatile u16 *pg=(volatile u16*)0x06000000; REG_DISPCNT=4|(1<<10);
      for(int k=0;k<pcw.npages;k++){ pc_world_draw(pg,k); char fn[600]; snprintf(fn,sizeof fn,"%s_pcfile%d.ppm",out,k+1); pc_write_ppm(fn,scale); } }
    pc_hw_finish();
    return 0;
}

uint16_t pc_font_glyph(int ch){ return font[ch&127]; }      // the game's own 3x5 font, for the PC options screen
