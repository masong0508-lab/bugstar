// run.c: run a GBA ROM headless in mGBA, press keys from a script, save screenshots.
// usage: run ROM SCRIPT OUTPREFIX SAVEFILE
// script lines: <startframe> <endframe> <keys>   keys: A B s(select) S(start) R L U D l(left) r(right) ;  "shot <frame>"
#include <mgba/core/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/core/config.h>
#include <mgba-util/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
typedef struct { int a,b; unsigned k; } Ev;
unsigned get_pc(struct mCore*c){ return ((struct ARMCore*)c->cpu)->gprs[15]; }
static Ev ev[8192]; static int nev; static int shots[8192], nshot;
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
  while(fgets(line,256,f)){ int a,b; char ks[64];
    if(sscanf(line,"shot %d",&a)==1){shots[nshot++]=a; if(a>last)last=a; continue;}
    if(sscanf(line,"%d %d %63s",&a,&b,ks)==3){unsigned k=0; for(char*p=ks;*p;p++){switch(*p){
      case 'A':k|=1;break;case 'B':k|=2;break;case 's':k|=4;break;case 'S':k|=8;break;case 'r':k|=16;break;case 'l':k|=32;break;case 'U':k|=64;break;case 'D':k|=128;break;case 'R':k|=256;break;case 'L':k|=512;break;}}
      ev[nev].a=a;ev[nev].b=b;ev[nev].k=k;nev++; if(b>last)last=b;}}
  fclose(f);
  for(int fr=0;fr<=last;fr++){ if(fr==3) for(int i=5;i<c;i++) if(v[i][0]=='@'){ unsigned a=strtoul(v[i]+1,0,16); char*e=strchr(v[i],'='); core->rawWrite32(core,a,-1,strtoul(e+1,0,0)); } unsigned k=0; for(int i=0;i<nev;i++) if(fr>=ev[i].a&&fr<ev[i].b) k|=ev[i].k;
    core->setKeys(core,k);
    if(getenv("TR_A") && fr>=atoi(getenv("TR_A")) && fr<=atoi(getenv("TR_B"))){ static unsigned wa[32]; static int nw=-1; if(nw<0){ nw=0; char*p=getenv("TR_W"); while(p&&*p){ wa[nw++]=strtoul(p,&p,16)&~1u; while(*p==',')p++; } }
      unsigned fc=core->frameCounter(core); while(core->frameCounter(core)==fc){ core->step(core); unsigned cpc=0; { uint32_t regs; (void)regs; } cpc=core->busRead16 ? 0:0; 
        unsigned p2=get_pc(core)&~1u; for(int q=0;q<nw;q++) if(p2-wa[q]==4) printf("T f%d v%u dc%04x w%d\n",fr,core->busRead16(core,0x04000006),core->busRead16(core,0x04000000),q); } }
    else core->runFrame(core);
    for(int i=0;i<nshot;i++) if(shots[i]==fr){ char n[512]; sprintf(n,"%s_%06d.ppm",v[3],fr); FILE*o=fopen(n,"wb"); fprintf(o,"P6 %u %u 255\n",w,h);
      for(unsigned j=0;j<w*h;j++){ unsigned px=buf[j]; unsigned char rgb[3]={px&255,(px>>8)&255,(px>>16)&255}; fwrite(rgb,1,3,o);} fclose(o);} }
  for(int i=5;i<c;i++){ if(v[i][0]=='@') continue; unsigned a=strtoul(v[i],0,16); printf("%s:",v[i]); int nw=getenv("RQ_N")?atoi(getenv("RQ_N")):64; for(int k=0;k<nw;k++) printf(" %u",core->rawRead32(core,a+4*k,-1)); printf("\n"); }
  core->deinit(core); return 0; }
