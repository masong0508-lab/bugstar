// pc_kartnat.inc: the kart race's state, read once a frame (from m7_present) and handed to the window picture (pc_kartnat_r.inc in pc_hw.c).
// It only READS the game. Banking comes from your steering and the bend ahead, hills from the track's seed and where you are on it, shadows from
// where the karts stand on the ground this frame.
static void pc_kn_snapshot(void){
    int act = kr_mode==1 && m7_on && !kloop_on && !cam_fp;
    if(!act){ pc_kn_frame(m7_rowz,0,0,0,0,0,0,0,0,0,0,0,0,0,0); return; }
    int pi=rc[0].pi, nx=(pi+1)%knp; s32 ax=wrapd(ppx-kp_cx(pi))>>8, ay=wrapd(ppy-kp_cy(pi))>>8, bx=wrapd(kp_cx(nx)-kp_cx(pi))>>8, by=wrapd(kp_cy(nx)-kp_cy(pi))>>8;
    int fr=0; { s32 dd=bx*bx+by*by; if(dd>0){ fr=(int)(((long long)(ax*bx+ay*by)*256)/dd); if(fr<0) fr=0; if(fr>255) fr=255; } }
    int kcv=((kp_dir((pi+10)%knp)-kp_dir(pi)+512)&1023)-512;
    int elev=(int)(cam_h-ground_h(ppx,ppy)); if(elev<256) elev=256;
    s32 dz=cam_back(); s32 f=(FOCP<<16)/(dz>>8); s32 gh=ground_h(ppx,ppy)+kbr_cur; s32 curve=curv(dz>>8)-tiny_sub(); int sy=PHZ+(((cam_h+curve-gh)*f)>>16);
    int pw = !(nkb && kunder(ppx,ppy,rc[0].pi)) ? 16*256/ (kl.part[KP_SIZE]==0?232:kl.part[KP_SIZE]==2?160:190) + 4 : 0;
    pc_kn_frame(m7_rowz,1,pi*256+fr,heading,kcv,elev,(unsigned)(wd.tseed*2654435761u)^(unsigned)(k_trk*40503u+1),knp,ksh_n,ksh_x,ksh_y,ksh_w,120,sy,pw);
}
