// rec.c: like run.c, but also records frames for video. Script lines: "<start> <end> <keys>", "shot <frame>",
// "rec <start> <end> <step>" (append frames start..end-1, every step-th, as raw RGB24 240x160 to OUTPREFIX.rgb)
#include <mgba/core/core.h>
#include <mgba/core/config.h>
#include <mgba-util/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
typedef struct { int a,b; unsigned k; } Ev;
static Ev ev[16384]; static int nev; static int shots[8192], nshot; static int ra[256], rb[256], rs[256], nrec; static int fa=-1, fb=-1; static unsigned faddr=0;
int main(int c, char **v){
  if(c<5){ fprintf(stderr,"usage: rec ROM SCRIPT OUTPREFIX SAVEFILE\n"); return 1; }
  struct mCore *core=mCoreFind(v[1]); if(!core){ fprintf(stderr,"no core\n"); return 1; }
  core->init(core); mCoreInitConfig(core,NULL);
  unsigned w,h; core->baseVideoSize(core,&w,&h);
  mColor *buf=malloc(w*h*sizeof(mColor)); core->setVideoBuffer(core,buf,w);
  if(!mCoreLoadFile(core,v[1])){ fprintf(stderr,"load fail\n"); return 1; }
  struct VFile *sv=VFileOpen(v[4],O_CREAT|O_RDWR); if(sv) core->loadSave(core,sv);
  core->reset(core);
  FILE*f=fopen(v[2],"r"); char line[256]; int last=0;
  while(fgets(line,256,f)){ int a,b,s; char ks[64];
    if(sscanf(line,"rec %d %d %d",&a,&b,&s)==3){ ra[nrec]=a; rb[nrec]=b; rs[nrec]=s<1?1:s; nrec++; if(b>last) last=b; continue; }
    { unsigned ad; if(sscanf(line,"recf %d %d %x",&a,&b,&ad)==3){ fa=a; fb=b; faddr=ad; if(b>last) last=b; continue; } }
    if(sscanf(line,"shot %d",&a)==1){shots[nshot++]=a; if(a>last)last=a; continue;}
    if(sscanf(line,"%d %d %63s",&a,&b,ks)==3){unsigned k=0; for(char*p=ks;*p;p++){switch(*p){
      case 'A':k|=1;break;case 'B':k|=2;break;case 's':k|=4;break;case 'S':k|=8;break;case 'r':k|=16;break;case 'l':k|=32;break;case 'U':k|=64;break;case 'D':k|=128;break;case 'R':k|=256;break;case 'L':k|=512;break;}}
      ev[nev].a=a;ev[nev].b=b;ev[nev].k=k;nev++; if(b>last)last=b;}}
  fclose(f);
  char rn[512]; sprintf(rn,"%s.rgb",v[3]); FILE *ro = (nrec||fa>=0) ? fopen(rn,"wb") : NULL; unsigned flast=0; unsigned char *line3=malloc(w*h*3); int nframes=0;
  for(int fr=0;fr<=last;fr++){ unsigned k=0; for(int i=0;i<nev;i++) if(fr>=ev[i].a&&fr<ev[i].b) k|=ev[i].k;
    core->setKeys(core,k); core->runFrame(core);
    int want=0; for(int i=0;i<nrec;i++) if(fr>=ra[i] && fr<rb[i] && ((fr-ra[i])%rs[i])==0) want=1;
    if(fa>=0 && fr>=fa && fr<fb){ unsigned fv=core->busRead32(core,faddr); if(fv!=flast){ flast=fv; want=1; } }   // a new picture from the game
    if(want && ro){ for(unsigned j=0;j<w*h;j++){ unsigned px=buf[j]; line3[j*3]=px&255; line3[j*3+1]=(px>>8)&255; line3[j*3+2]=(px>>16)&255; } fwrite(line3,1,w*h*3,ro); nframes++; }
    for(int i=0;i<nshot;i++) if(shots[i]==fr){ char n[512]; sprintf(n,"%s_%06d.ppm",v[3],fr); FILE*o=fopen(n,"wb"); fprintf(o,"P6 %u %u 255\n",w,h);
      for(unsigned j=0;j<w*h;j++){ unsigned px=buf[j]; unsigned char rgb[3]={px&255,(px>>8)&255,(px>>16)&255}; fwrite(rgb,1,3,o);} fclose(o);} }
  if(ro) fclose(ro); if(nrec||fa>=0) fprintf(stderr,"%d frames recorded\n",nframes);
  core->deinit(core); return 0; }
