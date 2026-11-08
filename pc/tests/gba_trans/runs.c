// runs.c: runq + scripting + beam-level tear detector.
// usage: runs ROM SCRIPT OUTPREFIX SAVEFILE
// script lines: "<a> <b> <keys>" hold keys; "shot F"; "shots A B" (every frame); "poke F HEXADDR VALUE SIZE" (SIZE 1/2/4, set at start of frame F);
//               "watch A B" (frames A..B single-stepped: report display changes while the beam is drawing)
// watch output lines:  W frame <f> vram <n> rows <lo>-<hi> first_v <v> | ctrl <n> regs <list> | pal <n> | oam <n>
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba-util/vfs.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
typedef struct { int a,b; unsigned k; } Ev; typedef struct { int f; unsigned a,v; int sz; } Pk;
static Ev ev[16384]; static int nev; static int shots[16384], nshot; static Pk pk[256]; static int npk; static int wa[64],wb[64],nw;
static unsigned char *vram; 
int main(int c, char **v){
  if(c<5){ fprintf(stderr,"usage\n"); return 1; }
  struct mCore *core=mCoreFind(v[1]); if(!core){ fprintf(stderr,"no core\n"); return 1; }
  core->init(core); mCoreInitConfig(core,NULL);
  unsigned w,h; core->baseVideoSize(core,&w,&h);
  mColor *buf=malloc(w*h*sizeof(mColor)); core->setVideoBuffer(core,buf,w);
  if(!mCoreLoadFile(core,v[1])){ fprintf(stderr,"load fail\n"); return 1; }
  struct VFile *sv=VFileOpen(v[4],O_CREAT|O_RDWR); if(sv) core->loadSave(core,sv);
  core->reset(core);
  FILE*f=fopen(v[2],"r"); char line[256]; int last=0;
  while(fgets(line,256,f)){ int a,b; char ks[64]; unsigned ua,uv; int sz;
    if(sscanf(line,"shots %d %d",&a,&b)==2){ for(int i=a;i<=b;i++) shots[nshot++]=i; if(b>last)last=b; continue; }
    if(sscanf(line,"shot %d",&a)==1){shots[nshot++]=a; if(a>last)last=a; continue;}
    if(sscanf(line,"poke %d %x %i %d",&a,&ua,&uv,&sz)==4){ pk[npk].f=a; pk[npk].a=ua; pk[npk].v=uv; pk[npk].sz=sz; npk++; continue; }
    if(sscanf(line,"watch %d %d",&a,&b)==2){ wa[nw]=a; wb[nw]=b; nw++; if(b>last)last=b; continue; }
    if(sscanf(line,"%d %d %63s",&a,&b,ks)==3){unsigned k=0; for(char*p=ks;*p;p++){switch(*p){
      case 'A':k|=1;break;case 'B':k|=2;break;case 's':k|=4;break;case 'S':k|=8;break;case 'r':k|=16;break;case 'l':k|=32;break;case 'U':k|=64;break;case 'D':k|=128;break;case 'R':k|=256;break;case 'L':k|=512;break;}}
      ev[nev].a=a;ev[nev].b=b;ev[nev].k=k;nev++; if(b>last)last=b;}}
  fclose(f);
  struct GBA *gba=(struct GBA*)core->board;
  static unsigned char snap[0x18000]; static unsigned short psnap[512], osnap[512], isnap[0x30];
  for(int fr=0;fr<=last;fr++){
    for(int i=0;i<npk;i++) if(pk[i].f==fr){ if(pk[i].sz==1) core->rawWrite8(core,pk[i].a,-1,pk[i].v); else if(pk[i].sz==2) core->rawWrite16(core,pk[i].a,-1,pk[i].v); else core->rawWrite32(core,pk[i].a,-1,pk[i].v); }
    unsigned k=0; for(int i=0;i<nev;i++) if(fr>=ev[i].a&&fr<ev[i].b) k|=ev[i].k;
    core->setKeys(core,k);
    int watching=0; for(int i=0;i<nw;i++) if(fr>=wa[i]&&fr<=wb[i]) watching=1;
    if(!watching) core->runFrame(core);
    else {
      unsigned fc=core->frameCounter(core); int pv=-1; int nvram=0,lo=999,hi=-1,firstv=-1,nctrl=0,npal=0,noam=0; unsigned regs=0;
      vram=(unsigned char*)gba->video.vram; int init=0;
      // run until the frame counter ticks
      while(core->frameCounter(core)==fc){ core->step(core); int vc=gba->video.vcount;
        if(vc==pv) continue;
        unsigned dc=gba->memory.io[0]; int mode=dc&7; unsigned base=0, len;
        if(mode==3){ base=0; len=0x12C00; } else if(mode==4||mode==5){ base=(dc&0x10)?0xA000:0; len=mode==4?38400:40*256*2; if(mode==5) len=0xA000; } else { base=0; len=0x10000; }
        int blanked = init && ((isnap[0]&0x80) || ((isnap[0x28]&0xC0)==0xC0 && (isnap[0x28]&0x3F)==0x3F && (isnap[0x2A]&31)>=16));   // (forced blank, or faded all the way to black: nothing shows)
        if(init && pv>=0 && pv<160 && !blanked){   // the line pv has just been drawn; what changed during it?
          // io control regs
          unsigned ch=0; static const int cr[]={0,4,5,6,7,0x28,0x29,0x2A}; (void)cr;
          unsigned short *io=gba->memory.io; static const int ri[]={0x00,0x04,0x05,0x06,0x07,0x24,0x25,0x26,0x27,0x28,0x29,0x2A};
          for(int q=0;q<12;q++) if(io[ri[q]]!=isnap[ri[q]]) ch|=1u<<q;
          if(ch){ nctrl++; regs|=ch; }
          if(memcmp(psnap,gba->video.palette,1024)) npal++;
          if(memcmp(osnap,gba->video.oam.raw,1024)) noam++;
          if(mode==4 && ((isnap[0]^dc)&0x10)==0){
            // rows: mode 4 is 240 bytes per row
            for(int r=0;r<160;r++) if(memcmp(snap+base+r*240,vram+base+r*240,240)){ nvram++; if(r<lo)lo=r; if(r>hi)hi=r; if(firstv<0)firstv=pv; }
          } else if(mode!=4) { if(memcmp(snap+base,vram+base,len)){ nvram++; if(firstv<0)firstv=pv; lo=0; hi=159; } }
        }
        if(vc<160){ memcpy(snap,vram,0x18000); memcpy(psnap,gba->video.palette,1024); memcpy(osnap,gba->video.oam.raw,1024); for(int q=0;q<0x30;q++) isnap[q]=gba->memory.io[q]; init=1; }
        pv=vc; }
      printf("W frame %d vram %d rows %d-%d first_v %d | ctrl %d regs %x | pal %d | oam %d\n",fr,nvram,lo==999?-1:lo,hi,firstv,nctrl,regs,npal,noam);
    }
    for(int i=0;i<nshot;i++) if(shots[i]==fr){ char n[512]; sprintf(n,"%s_%06d.ppm",v[3],fr); FILE*o=fopen(n,"wb"); fprintf(o,"P6 %u %u 255\n",w,h);
      for(unsigned j=0;j<w*h;j++){ unsigned px=buf[j]; unsigned char rgb[3]={px&255,(px>>8)&255,(px>>16)&255}; fwrite(rgb,1,3,o);} fclose(o);} }
  core->deinit(core); return 0; }
