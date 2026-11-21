// PC test-only real dungeon/warden capture setup. Included at the END of
// pc/gen/main.c by build.sh. The retail GBA/Android source never includes it.
// BUGSTAR_BOSS_CAPTURE="zone:phase", 0..5 and 0/1. The normal game is
// completely unchanged when this variable is absent.
static void pc_boss_capture_setup(void){
    const char *env=getenv("BUGSTAR_BOSS_CAPTURE");
    if(!env || !*env) return;
    int z=atoi(env), phase=0;
    const char *sep=strchr(env,':');
    if(sep) phase=atoi(sep+1);
    if(z<0 || z>=6 || phase<0 || phase>1){
        fprintf(stderr,"boss-capture: invalid zone or phase: %s\n",env);
        exit(3);
    }
    // Start from the normal newly initialized game, then switch to the
    // requested story planet and build the actual bottom dungeon floor.
    cur_sys=z; cur_pl=0; cur_moon=0; cur_rock=0;
    zones_open=6; cleared_mask=0; base_mode=0; forge_on=0;
    land_on_current();
    if(endless || zone!=z){
        fprintf(stderr,"boss-capture: wrong story world (wanted %d got %d)\n",z,zone);
        exit(4);
    }
    fm_reset();
    depth_no=zone_floors()-1; fl_max=depth_no; skip_taken=0;
    arrive_dn=0; arrive_sk=0; tier_lock=0; portal_depth=0;
    scene=1; dungeon_enter();
    // First-clear boss spawns through the unmodified dungeon generator.
    // Move the CAMERA (not the world layout) into the same boss chamber,
    // ensuring a clear 2-tile view across its authentic rendered silhouette.
    int boss=-1, count=0;
    for(int i=0;i<ndo;i++) if(dob[i].alive && dob[i].kind==K_BOSS){
        if(boss<0) boss=i;
        count++;
    }
    if(boss<0){
        fprintf(stderr,"boss-capture: zone %d spawned no warden\n",z);
        exit(5);
    }
    DObj *b=&dob[boss];
    const int step[4][3]={{2,0,512},{-2,0,0},{0,2,768},{0,-2,256}};
    int view=0;
    for(int i=0;i<4;i++){
        int x=(b->x>>16)+step[i][0], y=(b->y>>16)+step[i][1];
        int bx1=(b->x>>16)+step[i][0]/2, by1=(b->y>>16)+step[i][1]/2;
        if(x<1||y<1||x>=MW-1||y>=MW-1)continue;
        if(solid((x<<16)+0x8000,(y<<16)+0x8000) ||
           solid((bx1<<16)+0x8000,(by1<<16)+0x8000))continue;
        px_=(x<<16)+0x8000; py_=(y<<16)+0x8000;
        pang=step[i][2]; view=1; break;
    }
    if(!view){
        fprintf(stderr,"boss-capture: zone %d has no clear boss viewing position\n",z);
        exit(6);
    }
    if(phase && b->maxhp>3) b->hp=b->maxhp/2-1;
    if(hearts<10000) hearts=10000;
    if(maxh<10000) maxh=10000;
    invuln=30000; food=100; opt_autosave=0;
    music(4);
    fprintf(stderr,"boss-capture: zone=%d phase=%d monsters=%d hp=%d/%d camera=%d,%d boss=%d,%d\n",
        z,phase,count,b->hp,b->maxhp,(int)(px_>>16),(int)(py_>>16),
        (int)(b->x>>16),(int)(b->y>>16));
}
