// pc_sdl.c: the window front end of BUGSTAR PC (SDL2). Everything the player sees and touches comes through here:
//   window (resizable, fullscreen, high-DPI, vsync, icon), the picture at the window's real size, keyboard and gamepad (rebindable),
//   sound (the GBA sound chip synth in pc_psg.c -> SDL audio), the per-OS save folder, an options screen, screenshots, rumble.
// The game itself (the C code from the GBA) does not know any of this. It still thinks it is a GBA; pc_hw.c calls plat_frame() once per game frame (60 a second).
#define _GNU_SOURCE
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "pc_plat.h"
extern int pc_mouse_x, pc_mouse_y, pc_mouse_b, pc_mouse_wheel;

extern int pc_vid3d, pc_ships_mode, pc_armada_n;
int pc_motor(void);                 // pc_hw.c: is the (cartridge) rumble motor on right now
int pc_script_keys(uint16_t *k);    // pc_hw.c: keys from a --script file, if one was given (for tests)
long pc_frames(void);

#define NB 10
static const char *bname[NB]={"A","B","SELECT","START","RIGHT","LEFT","UP","DOWN","R","L"};
// ------------------------------------------------------------------------------------------------ settings
typedef struct {
    int ships, armada, textsize, contrast, mouse, video3d, fullscreen, scale_mode, vsync, show_fps, volume, mute, pause_unfocus, rumble, deadzone, max_mp;
    int kb[NB][2]; int pad[NB]; int win_w, win_h;
} Cfg;
enum { SM_CRISP, SM_SMOOTH, SM_COUNT };
static Cfg C;
static char g_pref[1024], g_cfgpath[1100], g_savepath[1100], g_shotdir[1100];
static int g_a38;      // patch 38: set when the settings file already carries the Android marker
#ifdef PC_ANDROID
// patch 39: touch pad layout data (landscape [0] and portrait [1] each keep their own), saved in settings.ini as tl0_*/tl1_* and talpha
enum { TC_DPAD, TC_A, TC_B, TC_L, TC_R, TC_SEL, TC_STA, TC_MENU, TC_N };
typedef struct { int set, x, y, s; } TPos;      // set: the player moved it; x,y: centre in 1/1000 of the window width/height; s: size in percent (0 = 100)
static TPos g_tl[2][TC_N]; static int g_talpha;      // g_talpha: opacity in percent (0 = 20)
static const char *const tl_key[TC_N]={"dpad","a","b","l","r","sel","sta","menu"};
static void tl_save(FILE *f){
    fprintf(f,"talpha=%d\n",g_talpha);
    for(int o=0;o<2;o++) for(int i=0;i<TC_N;i++){ TPos *p=&g_tl[o][i]; if(p->set||p->s) fprintf(f,"tl%d_%s=%d,%d,%d\n",o,tl_key[i],p->set?p->x:-1,p->set?p->y:-1,p->s?p->s:100); }
}
static int tl_parse(const char *k, const char *v){
    if(!strcmp(k,"talpha")){ int a=atoi(v); g_talpha=a<5?5:a>100?100:a; return 1; }
    if(k[0]=='t'&&k[1]=='l'&&(k[2]=='0'||k[2]=='1')&&k[3]=='_'){ int o=k[2]-'0';
        for(int i=0;i<TC_N;i++) if(!strcmp(k+4,tl_key[i])){ int x,y,s; if(sscanf(v,"%d,%d,%d",&x,&y,&s)==3){ TPos *p=&g_tl[o][i]; p->set=(x>=0&&y>=0); p->x=x<0?0:x>1000?1000:x; p->y=y<0?0:y>1000?1000:y; p->s=s<50?50:s>200?200:s; } return 1; } }
    return 0;
}
#endif
static void cfg_defaults(void){
    memset(&C,0,sizeof C);
    C.mouse=1; C.ships=1; C.armada=1; C.fullscreen=0; C.scale_mode=SM_CRISP; C.vsync=1; C.show_fps=0; C.volume=80; C.mute=0; C.pause_unfocus=1; C.rumble=1; C.deadzone=30; C.max_mp=0;
    C.win_w=960; C.win_h=640;
#ifdef PC_ANDROID
    C.video3d=2;      // patch 38: phones start on CLASSIC (GBA) 3D views; the native renderer is too heavy for them
#endif
    static const int kb[NB][2]={{SDL_SCANCODE_X,SDL_SCANCODE_SPACE},{SDL_SCANCODE_Z,SDL_SCANCODE_K},{SDL_SCANCODE_BACKSPACE,SDL_SCANCODE_TAB},{SDL_SCANCODE_RETURN,SDL_SCANCODE_KP_ENTER},
        {SDL_SCANCODE_RIGHT,SDL_SCANCODE_D},{SDL_SCANCODE_LEFT,SDL_SCANCODE_A},{SDL_SCANCODE_UP,SDL_SCANCODE_W},{SDL_SCANCODE_DOWN,SDL_SCANCODE_S},
        {SDL_SCANCODE_E,SDL_SCANCODE_PAGEDOWN},{SDL_SCANCODE_Q,SDL_SCANCODE_PAGEUP}};
    memcpy(C.kb,kb,sizeof kb);
    static const int pad[NB]={SDL_CONTROLLER_BUTTON_A,SDL_CONTROLLER_BUTTON_B,SDL_CONTROLLER_BUTTON_BACK,SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,SDL_CONTROLLER_BUTTON_LEFTSHOULDER};
    memcpy(C.pad,pad,sizeof pad);
}
static void cfg_save(void){
    FILE *f=fopen(g_cfgpath,"w"); if(!f) return;
    fprintf(f,"# BUGSTAR PC settings (edit in the game: press Esc). scale_mode: 0 crisp, 1 smooth\n");
#ifdef PC_ANDROID
    tl_save(f);      // patch 39
#endif
#ifdef PC_ANDROID
    fprintf(f,"android38=1\n");      // patch 38
#endif
    fprintf(f,"ships=%d\narmada=%d\ntextsize=%d\ncontrast=%d\nmouse=%d\nvideo3d=%d\nfullscreen=%d\nscale_mode=%d\nvsync=%d\nshow_fps=%d\nvolume=%d\nmute=%d\npause_unfocus=%d\nrumble=%d\ndeadzone=%d\nmax_mp=%d\nwin_w=%d\nwin_h=%d\n",
        C.ships,C.armada,C.textsize,C.contrast,C.mouse,C.video3d,C.fullscreen,C.scale_mode,C.vsync,C.show_fps,C.volume,C.mute,C.pause_unfocus,C.rumble,C.deadzone,C.max_mp,C.win_w,C.win_h);
    for(int i=0;i<NB;i++){ fprintf(f,"key_%s=%s,%s\n",bname[i],C.kb[i][0]?SDL_GetScancodeName(C.kb[i][0]):"",C.kb[i][1]?SDL_GetScancodeName(C.kb[i][1]):"");
        fprintf(f,"pad_%s=%s\n",bname[i],SDL_GameControllerGetStringForButton(C.pad[i])?SDL_GameControllerGetStringForButton(C.pad[i]):""); }
    fclose(f);
}
static void cfg_load(void){
    FILE *f=fopen(g_cfgpath,"r"); if(!f) return; char ln[256];
    while(fgets(ln,sizeof ln,f)){
        char *e=strchr(ln,'='); if(!e||ln[0]=='#') continue; *e++=0; size_t n=strlen(e); while(n&&(e[n-1]=='\n'||e[n-1]=='\r')) e[--n]=0;
        if(!strcmp(ln,"android38")){ g_a38=1; continue; }      // patch 38
#ifdef PC_ANDROID
        if(tl_parse(ln,e)) continue;      // patch 39
#endif
        int v=atoi(e);
        #define I(nm,fld,lo,hi) if(!strcmp(ln,nm)){ C.fld = v<lo?lo:v>hi?hi:v; continue; }
        I("ships",ships,0,2) I("armada",armada,0,3) I("textsize",textsize,0,2) I("contrast",contrast,0,1) I("mouse",mouse,0,1) I("video3d",video3d,0,2) I("fullscreen",fullscreen,0,1) I("scale_mode",scale_mode,0,SM_COUNT-1) I("vsync",vsync,0,1) I("show_fps",show_fps,0,1) I("volume",volume,0,100) I("mute",mute,0,1)
        I("pause_unfocus",pause_unfocus,0,1) I("rumble",rumble,0,1) I("deadzone",deadzone,5,80) I("max_mp",max_mp,0,40) I("win_w",win_w,240,16384) I("win_h",win_h,160,16384)
        if(!strncmp(ln,"key_",4)){ for(int i=0;i<NB;i++) if(!strcmp(ln+4,bname[i])){ char *c=strchr(e,','); char a[64]="",b[64]=""; if(c){ *c=0; snprintf(b,sizeof b,"%s",c+1); } snprintf(a,sizeof a,"%s",e);
                C.kb[i][0]=a[0]?SDL_GetScancodeFromName(a):0; C.kb[i][1]=b[0]?SDL_GetScancodeFromName(b):0; } }
        if(!strncmp(ln,"pad_",4)){ for(int i=0;i<NB;i++) if(!strcmp(ln+4,bname[i])){ int b=SDL_GameControllerGetButtonFromString(e); if(b!=SDL_CONTROLLER_BUTTON_INVALID) C.pad[i]=b; } }
    }
    fclose(f);
#ifdef PC_ANDROID
    if(!g_a38) C.video3d=2;      // patch 38: once, for a settings file saved before this patch: back to CLASSIC (you can switch it again in VIDEO > 3D VIEWS)
#endif
}
// ------------------------------------------------------------------------------------------------ state
static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex, *fonttex;
static int tex_w, tex_h, tex_linear; static uint32_t *pix; static size_t pix_cap;
static int menu_open, menu_page, menu_sel, menu_wait;      // menu_wait: 1 = waiting for a key to bind, 2 = for a pad button
static int menu_slot;                                      // which key slot (0/1) a bind goes into
static uint64_t t_freq; static double fps_show; static int fps_n; static uint64_t fps_t0;      // (pacing state: see 'the frame' below)
static char toast[80]; static uint64_t toast_until;
static SDL_GameController *pad; static int pad_rumble_ok;
static SDL_AudioDeviceID adev; static const char *g_autoshot_path; static long g_autoshot_frame=-1; static int g_headless_audio;
static int g_no_audio; static int dw_, dh_;                // drawable size in real pixels
static long g_stop_after=-1; static const char *g_wav_path; static FILE *g_wav; static long g_wav_n;
static int g_paused_focus, g_fast;
static volatile int g_bg, g_app_save, g_app_resume, g_pace_reset;      // app in the background / save wanted / back in the foreground / start a fresh clock

static int g_first_run;
static void say_toast_ms(const char *s,int ms){ snprintf(toast,sizeof toast,"%s",s); toast_until=SDL_GetTicks64()+ms; }
static void say_toast(const char *s){ say_toast_ms(s,2000); }

// ------------------------------------------------------------------------------------------------ PNG (stored, no compression library needed)
static uint32_t crc_t[256]; static void crc_init(void){ for(uint32_t n=0;n<256;n++){ uint32_t c=n; for(int k=0;k<8;k++) c=c&1?0xEDB88320u^(c>>1):c>>1; crc_t[n]=c; } }
static uint32_t crc_upd(uint32_t c,const uint8_t*b,size_t n){ for(size_t i=0;i<n;i++) c=crc_t[(c^b[i])&255]^(c>>8); return c; }
static void be32(uint8_t*p,uint32_t v){ p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void chunk(FILE*f,const char*t,const uint8_t*d,uint32_t n){ uint8_t h[8]; be32(h,n); memcpy(h+4,t,4); fwrite(h,1,8,f);
    uint32_t c=crc_upd(0xFFFFFFFFu,(uint8_t*)t,4); if(n){ fwrite(d,1,n,f); c=crc_upd(c,d,n); } uint8_t e[4]; be32(e,~c); fwrite(e,1,4,f); }
static int png_write(const char *path, const uint32_t *px, int w, int h, int alpha);
int plat_write_png(const char *path, const uint32_t *xrgb, int w, int h){ return png_write(path,xrgb,w,h,0); }
int plat_write_png_rgba(const char *path, const uint32_t *argb, int w, int h){ return png_write(path,argb,w,h,1); }
static int png_write(const char *path, const uint32_t *xrgb, int w, int h, int alpha){
    if(!crc_t[1]) crc_init(); int bpp=alpha?4:3;
    FILE *f=fopen(path,"wb"); if(!f) return 0;
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10}; fwrite(sig,1,8,f);
    uint8_t ih[13]; be32(ih,w); be32(ih+4,h); ih[8]=8; ih[9]=alpha?6:2; ih[10]=0; ih[11]=0; ih[12]=0; chunk(f,"IHDR",ih,13);
    size_t rowb=(size_t)w*bpp+1, raw=rowb*h; uint8_t *rb=malloc(raw); 
    for(int y=0;y<h;y++){ uint8_t *r=rb+y*rowb; r[0]=0; for(int x=0;x<w;x++){ uint32_t p=xrgb[(size_t)y*w+x]; r[1+x*bpp]=p>>16; r[2+x*bpp]=p>>8; r[3+x*bpp]=p; if(alpha) r[4+x*bpp]=p>>24; } }
    // zlib stream of stored blocks
    size_t nblk=(raw+65534)/65535; uint8_t *z=malloc(raw+nblk*5+6+16); size_t zn=0; z[zn++]=0x78; z[zn++]=0x01;
    uint32_t a=1,b=0; for(size_t i=0;i<raw;i++){ a=(a+rb[i])%65521; b=(b+a)%65521; }
    for(size_t o=0;o<raw;){ size_t n=raw-o>65535?65535:raw-o; z[zn++]=(o+n>=raw); z[zn++]=n&255; z[zn++]=n>>8; z[zn++]=~n&255; z[zn++]=(~n>>8)&255; memcpy(z+zn,rb+o,n); zn+=n; o+=n; }
    be32(z+zn,(b<<16)|a); zn+=4; chunk(f,"IDAT",z,(uint32_t)zn); chunk(f,"IEND",0,0); fclose(f); free(rb); free(z); return 1;
}
// ------------------------------------------------------------------------------------------------ audio
#define RING 16384
static int16_t ring[RING*2]; static volatile unsigned rd_i, wr_i; static unsigned long underruns;
static unsigned ring_fill(void){ return __atomic_load_n(&wr_i,__ATOMIC_ACQUIRE)-__atomic_load_n(&rd_i,__ATOMIC_ACQUIRE); }
static void audio_cb(void *u, Uint8 *stream, int len){
    int16_t *o=(int16_t*)stream; int n=len/4; unsigned r=__atomic_load_n(&rd_i,__ATOMIC_RELAXED), w=__atomic_load_n(&wr_i,__ATOMIC_ACQUIRE);
    for(int i=0;i<n;i++){ if(r!=w){ o[i*2]=ring[(r%RING)*2]; o[i*2+1]=ring[(r%RING)*2+1]; r++; } else { o[i*2]=o[i*2+1]=0; underruns++; } }
    __atomic_store_n(&rd_i,r,__ATOMIC_RELEASE);
}
static void audio_open(void){
    if(g_no_audio) return;
    SDL_AudioSpec want; SDL_zero(want); want.freq=PC_AUDIO_RATE; want.format=AUDIO_S16SYS; want.channels=2; want.samples=512; want.callback=audio_cb;
    SDL_AudioSpec got; adev=SDL_OpenAudioDevice(NULL,0,&want,&got,0);
    if(!adev){ fprintf(stderr,"audio: %s (the game runs without sound)\n",SDL_GetError()); return; }
    if(got.freq!=PC_AUDIO_RATE||got.channels!=2||got.format!=AUDIO_S16SYS){ fprintf(stderr,"audio: unexpected format\n"); SDL_CloseAudioDevice(adev); adev=0; return; }
    SDL_PauseAudioDevice(adev,0);
}
// patch 43: one game frame lasts 280896 / 16777216 s = 1/59.7275 s, which is 48000 * 280896 / 16777216 = 803.64 samples. The sound is made at exactly that rate (the fraction is carried
// over), so the music and the effects run at the game's own speed. The audio device has its own clock, a few parts per million off: the ring's fill is steered back to AUDIO_TARGET by
// at most AUDIO_TRIM samples a frame (1 percent), and when the ring is far too full / empty a frame of sound is skipped / padded. The sound never decides when the game steps.
#define AUDIO_TARGET 1200
#define AUDIO_TRIM 8
#define AUDIO_HIGH 4000
static uint64_t a_acc; static unsigned long a_skips, a_pads;
static void audio_frame(void){          // make this frame's sound and queue it
    int fill=(int)ring_fill();
    { static long dbg=0; static int mn=1<<30,mx=0; if(getenv("BUGSTAR_AUDLOG")){ if(fill<mn)mn=fill; if(fill>mx)mx=fill; if(++dbg%100==0){ fprintf(stderr,"audfill min %d max %d underruns %lu pads %lu\n",mn,mx,underruns,a_pads); mn=1<<30; mx=0; } } }
    a_acc += (uint64_t)PC_AUDIO_RATE*280896u; int n=(int)(a_acc>>24); a_acc &= 0xFFFFFFu;      // 16777216 = 2^24
    if(adev){ int adj=(AUDIO_TARGET-fill)/48; if(adj>AUDIO_TRIM)adj=AUDIO_TRIM; if(adj<-AUDIO_TRIM)adj=-AUDIO_TRIM; n+=adj;
              if(fill>AUDIO_HIGH){ n=0; a_skips++; } else if(fill<96){ n+=64; a_pads++; } }
    static int16_t buf[2*1024]; if(n>1024) n=1024;
    if(menu_open || g_paused_focus) { memset(buf,0,sizeof buf); }
    else if(n>0) pc_psg_render(buf,n);
    if(n>0 && !menu_open && !g_paused_focus && !C.mute && C.volume<100){ int g=C.volume*C.volume*256/10000; for(int i=0;i<n*2;i++) buf[i]=(int16_t)((buf[i]*g)>>8); }
    if(C.mute) memset(buf,0,sizeof buf);
    if(adev){ unsigned w=__atomic_load_n(&wr_i,__ATOMIC_RELAXED); for(int i=0;i<n;i++){ ring[(w%RING)*2]=buf[i*2]; ring[(w%RING)*2+1]=buf[i*2+1]; w++; } __atomic_store_n(&wr_i,w,__ATOMIC_RELEASE); }
    if(g_wav && n>0){ fwrite(buf,4,n,g_wav); g_wav_n+=n; }
}
static void wav_close(void){
    if(!g_wav) return; uint32_t dl=(uint32_t)(g_wav_n*4), v; fseek(g_wav,0,SEEK_SET); fwrite("RIFF",1,4,g_wav); v=36+dl; fwrite(&v,4,1,g_wav); fwrite("WAVEfmt ",1,8,g_wav); v=16; fwrite(&v,4,1,g_wav);
    uint16_t s=1; fwrite(&s,2,1,g_wav); s=2; fwrite(&s,2,1,g_wav); v=PC_AUDIO_RATE; fwrite(&v,4,1,g_wav); v=PC_AUDIO_RATE*4; fwrite(&v,4,1,g_wav); s=4; fwrite(&s,2,1,g_wav); s=16; fwrite(&s,2,1,g_wav);
    fwrite("data",1,4,g_wav); fwrite(&dl,4,1,g_wav); fclose(g_wav); g_wav=0;
}
// ------------------------------------------------------------------------------------------------ window and picture
void plat_icon_pixels(uint32_t *p, int S){        // the icon: a yellow star over a dark disc, a green bug in front. Drawn from shapes, so any size works.
    for(int y=0;y<S;y++) for(int x=0;x<S;x++){
        double dx=(x+0.5)*64.0/S-32, dy=(y+0.5)*64.0/S-32, r=sqrt(dx*dx+dy*dy); uint32_t c=0;
        if(r<30){ c=0xFF141B3C; if(r>27.5) c=0xFF2C3A7A; }
        double ang=atan2(dy,dx)+M_PI/2; double seg=2*M_PI/5; double k=ang-seg*floor(ang/seg+0.5); double a2=fabs(k);   // angle from the nearest star tip
        double radial=26.0-15.0*(a2/(seg/2)); if(r<radial && r<28) c=0xFFFFD23C;
        double bx=dx, by=dy+6;                                    // the bug: body, wing split, head, eyes, legs
        if((bx*bx)/110+(by*by)/190<1) c=0xFF3DB04A;
        if((bx*bx)/110+(by*by)/190<1 && fabs(bx)<0.9) c=0xFF1F7A30;
        double hy=dy+19; if(dx*dx+hy*hy<30) c=0xFF2A8A38;
        for(int l=-1;l<=1;l++){ double ly=by-l*7; if(fabs(ly)<1.2 && fabs(bx)>9 && fabs(bx)<17) c=0xFF1F7A30; }
        if(dx*dx+hy*hy<30 && fabs(dx)>2 && fabs(dx)<4.5 && fabs(hy+1)<1.6) c=0xFFFFFFFF;
        p[y*S+x]=c; }
}
static void make_icon(void){
    const int S=64; SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,S,S,32,SDL_PIXELFORMAT_ARGB8888); if(!s) return;
    plat_icon_pixels(s->pixels,S); SDL_SetWindowIcon(win,s); SDL_FreeSurface(s);
}
static void tex_ensure(int w, int h, int linear){
    if(tex && tex_w==w && tex_h==h && tex_linear==linear) return;
    if(tex) SDL_DestroyTexture(tex);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear?"linear":"nearest");
    tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
    tex_w=w; tex_h=h; tex_linear=linear;
    if((size_t)w*h>pix_cap){ free(pix); pix=malloc((size_t)w*h*4); pix_cap=(size_t)w*h; }
}
#ifdef PC_ANDROID
static int cap_pixels(void){ return C.max_mp ? C.max_mp*1000000 : 450000; }      // patch 37: phone CPUs cannot do the native 3D at 2.4 MP
#else
static int cap_pixels(void){ return C.max_mp ? C.max_mp*1000000 : 2400000; }
#endif
static void present_game(int with_overlay);
static void touch_draw(void);          // patch 30.1: defined further down (clang refuses an implicit declaration)
static void frame_geometry(int *W, int *H, SDL_Rect *dst, int *linear, int *smooth){
    int dw=dw_, dh=dh_; if(dw<240)dw=240; if(dh<160)dh=160;
    int cap=cap_pixels();
    if(C.scale_mode==SM_CRISP){
        int k=dw/240<dh/160?dw/240:dh/160; if(k<1)k=1;
        int r=k; while(r>1 && ((k%r)!=0 || (long)r*r*38400>cap)) r--;      // render at r, upscale by k/r (a whole number): still sharp
        *W=240*r; *H=160*r; dst->w=240*k; dst->h=160*k; *linear=0; *smooth=0;
    } else {
        double s=fmin(dw/240.0,dh/160.0); int w=(int)lround(240*s), h=(int)lround(160*s); dst->w=w; dst->h=h; *smooth=1; *linear=0;
        if((long)w*h>cap){ double f=sqrt((double)cap/((double)w*h)); *W=(int)(w*f)&~1; *H=(int)(h*f)&~1; if(*W<240)*W=240; if(*H<160)*H=160; *linear=1; }
        else { *W=w; *H=h; }
    }
    dst->x=(dw-dst->w)/2; dst->y=(dh-dst->h)/2;
}
// ---- font / text overlay
static void font_init(void){
    const int cw=4, ch=6; SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,cw*16,ch*8,32,SDL_PIXELFORMAT_ARGB8888); if(!s) return; SDL_FillRect(s,0,0);
    uint32_t *p=s->pixels;
    for(int c=32;c<128;c++){ uint16_t g=pc_font_glyph(c); int cx=(c&15)*cw, cy=(c>>4)*ch; for(int k=0;k<15;k++) if(g&(1<<(14-k))) p[(cy+k/3)*(cw*16)+cx+k%3]=0xFFFFFFFF; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"nearest"); fonttex=SDL_CreateTextureFromSurface(ren,s); SDL_SetTextureBlendMode(fonttex,SDL_BLENDMODE_BLEND); SDL_FreeSurface(s);
}
static int gs_;                                                  // glyph scale in real pixels
static int text_w(const char *s){ return (int)strlen(s)*4*gs_; }
static void text_at(int x, int y, const char *s, uint32_t col){
    if(!fonttex) return; SDL_SetTextureColorMod(fonttex,(col>>16)&255,(col>>8)&255,col&255);
    for(;*s;s++){ int c=(unsigned char)*s; if(c>='a'&&c<='z') c-=32; if(c<32||c>127) c='?'; SDL_Rect a={(c&15)*4,(c>>4)*6,4,6}, b={x,y,4*gs_,6*gs_}; if(c!=' ') SDL_RenderCopy(ren,fonttex,&a,&b); x+=4*gs_; }
}
static void rect_fill(int x,int y,int w,int h,uint32_t col,int a){ SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(ren,(col>>16)&255,(col>>8)&255,col&255,a); SDL_Rect r={x,y,w,h}; SDL_RenderFillRect(ren,&r); }

// ---- the options screen
enum { P_MAIN, P_VIDEO, P_AUDIO, P_CONTROLS, P_PAD, P_ABOUT, P_ACCESS, P_COUNT };
typedef struct { int kind; const char *label; int *val; int lo, hi, step; const char *const *names; int action; } Item;   // kind 0 action, 1 choice/toggle, 2 slider
enum { A_RESUME=1, A_VIDEO, A_AUDIO, A_CONTROLS, A_PAD, A_ABOUT, A_QUIT, A_BACK, A_FULLSCREEN, A_VSYNC, A_RESETKEYS, A_RESETPAD, A_WINSIZE, A_SHOT, A_MAINBACK, A_ACCESS, A_TOUCHEDIT, A_REBIND=1000 };
static const char *const onoff[]={"OFF","ON"}; static const char *const scalenames[]={"CRISP (WHOLE NUMBERS)","SMOOTH (FITS THE WINDOW)"};
static const char *const tsnames[]={"NORMAL","LARGE","HUGE"}; static const char *const hcnames[]={"OFF","ON"};
static const char *const shnames[]={"CLASSIC (GBA)","NATIVE","ULTRA"}; static const char *const arnames[]={"OFF","500 SHIPS","2,000 SHIPS","10,000 SHIPS"};
static const char *const v3names[]={"AUTO","NATIVE (SMOOTH)","CLASSIC (GBA)"};
static const char *const mpnames[]={"AUTO","1 MEGAPIXEL","2","3","4","6","8"}; static int mp_choice;
static int mp_val(int i){ static const int v[]={0,1,2,3,4,6,8}; return v[i]; }
#ifdef PC_ANDROID
static int ted_on; static void ted_open(void); static void ted_draw(void); static void ted_key(SDL_Keycode k);      // patch 39: the touch pad layout editor
#define TL_MENU_ITEM it[k++]=(Item){0,"TOUCH PAD LAYOUT",0,0,0,0,0,A_TOUCHEDIT};
#define TL_MENU_CASE case A_TOUCHEDIT: ted_open(); break;
#else
#define TL_MENU_ITEM
#define TL_MENU_CASE
#endif
static void menu_build(Item *it, int *n){
    int k=0;
    switch(menu_page){
    case P_MAIN: it[k++]=(Item){0,"RESUME",0,0,0,0,0,A_RESUME}; it[k++]=(Item){0,"VIDEO",0,0,0,0,0,A_VIDEO}; it[k++]=(Item){0,"AUDIO",0,0,0,0,0,A_AUDIO}; it[k++]=(Item){0,"ACCESSIBILITY AND MOUSE",0,0,0,0,0,A_ACCESS};
        it[k++]=(Item){0,"KEYBOARD",0,0,0,0,0,A_CONTROLS}; it[k++]=(Item){0,"GAMEPAD",0,0,0,0,0,A_PAD}; TL_MENU_ITEM it[k++]=(Item){0,"SCREENSHOT",0,0,0,0,0,A_SHOT};
        it[k++]=(Item){0,"ABOUT AND FOLDERS",0,0,0,0,0,A_ABOUT}; it[k++]=(Item){0,"QUIT GAME",0,0,0,0,0,A_QUIT}; break;
    case P_VIDEO: it[k++]=(Item){1,"FULLSCREEN",&C.fullscreen,0,1,1,onoff,A_FULLSCREEN}; it[k++]=(Item){1,"SCALING",&C.scale_mode,0,SM_COUNT-1,1,scalenames,0}; it[k++]=(Item){1,"3D VIEWS",&C.video3d,0,2,1,v3names,0}; it[k++]=(Item){1,"SHIPS",&C.ships,0,2,1,shnames,0}; it[k++]=(Item){1,"FAR BATTLE",&C.armada,0,3,1,arnames,0};
        it[k++]=(Item){1,"VSYNC",&C.vsync,0,1,1,onoff,A_VSYNC}; it[k++]=(Item){1,"MAX DRAW SIZE",&mp_choice,0,6,1,mpnames,0}; it[k++]=(Item){1,"SHOW FPS",&C.show_fps,0,1,1,onoff,0};
        it[k++]=(Item){1,"PAUSE WHEN UNFOCUSED",&C.pause_unfocus,0,1,1,onoff,0}; it[k++]=(Item){0,"WINDOW SIZE: NEXT",0,0,0,0,0,A_WINSIZE}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_ACCESS: it[k++]=(Item){1,"MENU TEXT SIZE",&C.textsize,0,2,1,tsnames,0}; it[k++]=(Item){1,"HIGH CONTRAST",&C.contrast,0,1,1,hcnames,0}; it[k++]=(Item){1,"MOUSE PLAYS THE GAME",&C.mouse,0,1,1,onoff,0}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_AUDIO: it[k++]=(Item){2,"VOLUME",&C.volume,0,100,5,0,0}; it[k++]=(Item){1,"MUTE",&C.mute,0,1,1,onoff,0}; it[k++]=(Item){1,"RUMBLE (GAMEPAD)",&C.rumble,0,1,1,onoff,0}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_CONTROLS: for(int i=0;i<NB;i++) it[k++]=(Item){0,bname[i],0,0,0,0,0,A_REBIND+i}; it[k++]=(Item){0,"RESET KEYS",0,0,0,0,0,A_RESETKEYS}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_PAD: for(int i=0;i<NB;i++) it[k++]=(Item){0,bname[i],0,0,0,0,0,A_REBIND+i}; it[k++]=(Item){2,"STICK DEAD ZONE",&C.deadzone,5,80,5,0,0}; it[k++]=(Item){0,"RESET PAD",0,0,0,0,0,A_RESETPAD}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_ABOUT: it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    }
    *n=k;
}
static void apply_video(void);
static void toggle_fullscreen(void){ C.fullscreen=!C.fullscreen; apply_video(); }
static void apply_video(void){
    if(!win) return;
    SDL_SetWindowFullscreen(win, C.fullscreen? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if(!C.fullscreen) SDL_SetWindowSize(win,C.win_w,C.win_h);
    SDL_RenderSetVSync(ren,C.vsync); SDL_ShowCursor(C.fullscreen?SDL_DISABLE:SDL_ENABLE);
}
static void key_name_of(int i, char *o, size_t n){ const char *a=C.kb[i][0]?SDL_GetScancodeName(C.kb[i][0]):"-", *b=C.kb[i][1]?SDL_GetScancodeName(C.kb[i][1]):"-"; snprintf(o,n,"%s  /  %s",a,b); }
static void pad_name_of(int i, char *o, size_t n){ const char *a=SDL_GameControllerGetStringForButton(C.pad[i]); snprintf(o,n,"%s",a?a:"-"); }
static void menu_do(int act){
    int idx=act>=A_REBIND?act-A_REBIND:0; if(act>=A_REBIND) act=A_REBIND;
    switch(act){
    case A_RESUME: menu_open=0; cfg_save(); break;
    case A_VIDEO: menu_page=P_VIDEO; menu_sel=0; break; case A_AUDIO: menu_page=P_AUDIO; menu_sel=0; break; case A_ACCESS: menu_page=P_ACCESS; menu_sel=0; break;
    case A_CONTROLS: menu_page=P_CONTROLS; menu_sel=0; break; case A_PAD: menu_page=P_PAD; menu_sel=0; break; case A_ABOUT: menu_page=P_ABOUT; menu_sel=0; break;
    case A_MAINBACK: menu_page=P_MAIN; menu_sel=0; cfg_save(); break;
    TL_MENU_CASE case A_QUIT: cfg_save(); pc_quit_now(); break;
    case A_FULLSCREEN: apply_video(); break;
    case A_VSYNC: SDL_RenderSetVSync(ren,C.vsync); break;
    case A_WINSIZE: { static const int sz[][2]={{720,480},{960,640},{1200,800},{1440,960},{1920,1280}}; int cur=0; for(int i=0;i<5;i++) if(C.win_w==sz[i][0]) cur=i; cur=(cur+1)%5; C.win_w=sz[cur][0]; C.win_h=sz[cur][1];
        if(C.fullscreen){ C.fullscreen=0; } apply_video(); } break;
    case A_RESETKEYS: { Cfg d; Cfg s=C; cfg_defaults(); d=C; C=s; memcpy(C.kb,d.kb,sizeof C.kb); say_toast("KEYS RESET"); } break;
    case A_RESETPAD: { Cfg d; Cfg s=C; cfg_defaults(); d=C; C=s; memcpy(C.pad,d.pad,sizeof C.pad); say_toast("PAD RESET"); } break;
    case A_SHOT: menu_open=0; g_autoshot_frame=-2; break;
    case A_REBIND: menu_wait = menu_page==P_PAD ? 2 : 1; menu_sel=idx; break;
    }
}
static int m_x0, m_pw, m_lh, m_y1, m_n, m_gs;
static void menu_draw(void){
#ifdef PC_ANDROID
    if(ted_on){ ted_draw(); return; }      // patch 39
#endif
    int dw=dw_, dh=dh_; gs_=(dw<dh?dw:dh)/140;      /* patch 39: portrait sizes the text by the narrow side */ if(gs_<2) gs_=2; if(gs_>12) gs_=12; if(C.textsize){ int g2=(dw<dh?dw:dh)/(C.textsize==1?100:75); if(g2>gs_) gs_=g2; if(gs_>16) gs_=16; }
    int hc=C.contrast; rect_fill(0,0,dw,dh,0x000000,hc?200:150);
    static const char *const titles[]={"BUGSTAR PC","VIDEO","AUDIO","KEYBOARD","GAMEPAD","ABOUT","ACCESSIBILITY"};
    Item it[24]; int n; menu_build(it,&n);
    int lh=9*gs_, pw=text_w(C.textsize?"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM":"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"); if(pw>dw-4*gs_) pw=dw-4*gs_; int ph=(n+3)*lh+2*gs_*4; if(menu_page==P_ABOUT) ph=14*lh;
    int x0=(dw-pw)/2, y0=(dh-ph)/2; rect_fill(x0,y0,pw,ph,hc?0x000000:0x0B1030,hc?255:235); rect_fill(x0,y0,pw,gs_,0xFFD23C,255); rect_fill(x0,y0+ph-gs_,pw,gs_,0xFFD23C,255);
    text_at(x0+4*gs_,y0+3*gs_,titles[menu_page],0xFFD23C);
    int y=y0+3*gs_+lh*3/2; m_x0=x0; m_pw=pw; m_lh=lh; m_y1=y; m_n=menu_page==P_ABOUT?0:n; m_gs=gs_;
    if(menu_page==P_ABOUT){
        char b[1200]; const char *ln[]={"BUGSTAR FOR PC","THE GAME'S OWN GBA CODE RUNS AS NATIVE PC CODE.","","SAVES AND SETTINGS ARE IN:",g_pref,"","KEYS: F11 FULLSCREEN   F12 SCREENSHOT","F3 FPS   ESC THIS MENU","GAMEPAD: BACK+START OPENS THE MENU",0};
        for(int i=0;ln[i];i++){ snprintf(b,sizeof b,"%s",ln[i]); int maxc=(pw-8*gs_)/(4*gs_); if((int)strlen(b)>maxc){ b[maxc]=0; } text_at(x0+4*gs_,y,b,0xCCD4FF); y+=lh; }
        y+=lh; it[0].label="BACK"; text_at(x0+4*gs_,y,"> BACK",0xFFFFFF); return; }
    for(int i=0;i<n;i++){
        uint32_t col = i==menu_sel ? (hc?0x000000:0xFFFFFF) : (hc?0xFFFFFF:0xA8B4F0); char b[120]="";
        if(i==menu_sel) rect_fill(x0+2*gs_,y-gs_,pw-4*gs_,lh,hc?0xFFE000:0x2A3A9A,255);
        text_at(x0+4*gs_,y,it[i].label,col);
        if(it[i].kind==1) snprintf(b,sizeof b,"< %s >",it[i].names[*it[i].val - (it[i].val==&mp_choice?0:0)]);
        else if(it[i].kind==2) snprintf(b,sizeof b,"< %d >",*it[i].val);
        else if(it[i].action>=A_REBIND){ int bi=it[i].action-A_REBIND; if(menu_wait && menu_sel==i) snprintf(b,sizeof b,"%s",menu_wait==1?"PRESS A KEY (ESC CANCELS)":"PRESS A BUTTON"); else if(menu_page==P_CONTROLS) key_name_of(bi,b,sizeof b); else pad_name_of(bi,b,sizeof b); }
        if(b[0]) text_at(x0+pw-4*gs_-text_w(b),y,b,i==menu_sel?(hc?0x000000:0xFFD23C):(hc?0xFFFF60:0xE8D890));
        y+=lh;
    }
    if(menu_page==P_CONTROLS){ text_at(x0+4*gs_,y0+ph-lh-gs_,"ENTER REBINDS THE FIRST KEY   RIGHT: THE SECOND",0x8890C8); }
    if(menu_page==P_PAD){ text_at(x0+4*gs_,y0+ph-lh-gs_,"ENTER THEN PRESS A BUTTON ON THE PAD",0x8890C8); }
}
static void menu_key(SDL_Keycode k, SDL_Scancode sc){
#ifdef PC_ANDROID
    if(ted_on){ ted_key(k); return; }      // patch 39
#endif
    if(menu_wait==1){ if(sc==SDL_SCANCODE_ESCAPE){ menu_wait=0; return; } Item it[24]; int n; menu_build(it,&n); int bi=it[menu_sel].action-A_REBIND; if(bi>=0&&bi<NB){ C.kb[bi][menu_slot]=sc; } menu_wait=0; menu_slot=0; return; }
    if(menu_wait==2) return;
    Item it[24]; int n; menu_build(it,&n);
    if(k==SDLK_ESCAPE||k==SDLK_F1){ if(menu_page==P_MAIN){ menu_open=0; cfg_save(); } else { menu_page=P_MAIN; menu_sel=0; cfg_save(); } return; }
    if(k==SDLK_UP){ menu_sel=(menu_sel+n-1)%n; return; } if(k==SDLK_DOWN){ menu_sel=(menu_sel+1)%n; return; }
    Item *s=&it[menu_sel];
    if(k==SDLK_LEFT||k==SDLK_RIGHT){ int d=k==SDLK_LEFT?-1:1;
        if(s->kind==1||s->kind==2){ int v=*s->val+d*s->step; if(v<s->lo) v=s->lo; if(v>s->hi) v=s->hi; if(s->kind==1&&s->hi>s->lo) { v=*s->val+d; if(v<s->lo) v=s->hi; if(v>s->hi) v=s->lo; }
            *s->val=v; if(s->val==&mp_choice) C.max_mp=mp_val(mp_choice); if(s->action) menu_do(s->action); return; }
        if(s->kind==0 && s->action>=A_REBIND && menu_page==P_CONTROLS && d>0){ menu_slot=1; menu_do(s->action); } return; }
    if(k==SDLK_RETURN||k==SDLK_KP_ENTER||k==SDLK_SPACE){
        if(s->kind==1){ int v=*s->val+1; if(v>s->hi) v=s->lo; *s->val=v; if(s->val==&mp_choice) C.max_mp=mp_val(mp_choice); if(s->action) menu_do(s->action); }
        else if(s->kind==0){ if(s->action>=A_REBIND) menu_slot=0; menu_do(s->action); } }
}
static void menu_pad(int btn){
    if(menu_wait==2){ Item it[24]; int n; menu_build(it,&n); int bi=it[menu_sel].action-A_REBIND; if(bi>=0&&bi<NB && btn!=SDL_CONTROLLER_BUTTON_GUIDE) C.pad[bi]=btn; menu_wait=0; return; }
    switch(btn){ case SDL_CONTROLLER_BUTTON_DPAD_UP: menu_key(SDLK_UP,0); break; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: menu_key(SDLK_DOWN,0); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: menu_key(SDLK_LEFT,0); break; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: menu_key(SDLK_RIGHT,0); break;
        case SDL_CONTROLLER_BUTTON_A: menu_key(SDLK_RETURN,0); break; case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_START: case SDL_CONTROLLER_BUTTON_BACK: menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE); break; }
}
// ---- one picture
static int shot_pending;
static void sim_vsync(void);          // test hook, defined with the pacing below
static void present_game(int with_overlay){
    SDL_GetRendererOutputSize(ren,&dw_,&dh_);
    int W,H,linear,smooth; SDL_Rect dst; frame_geometry(&W,&H,&dst,&linear,&smooth);
    tex_ensure(W,H,linear);
    if(!pix || (size_t)W*H>pix_cap){ free(pix); pix=malloc((size_t)W*H*4); pix_cap=(size_t)W*H; if(!pix){ pix_cap=0; return; } }      // (never write a picture into a buffer made for another size)
    pc_vid3d=C.video3d; pc_compose_native(pix,W,H,smooth);
    if(C.contrast){ for(size_t i=0,n=(size_t)W*H;i<n;i++){ uint32_t c=pix[i]; int r=c>>16&255,g=c>>8&255,b=c&255; int m=(r+g+b)/3; r=m+(r-m)*3/2; g=m+(g-m)*3/2; b=m+(b-m)*3/2;
            r=(r-128)*3/2+128; g=(g-128)*3/2+128; b=(b-128)*3/2+128; r=r<0?0:r>255?255:r; g=g<0?0:g>255?255:g; b=b<0?0:b>255?255:b; pix[i]=(uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b; } }
    SDL_UpdateTexture(tex,NULL,pix,W*4);
    SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
    SDL_RenderCopy(ren,tex,NULL,&dst);
    gs_=dh_/180; if(gs_<2) gs_=2; if(gs_>10) gs_=10;
    if(with_overlay){
        if(C.show_fps){ char b[40]; snprintf(b,sizeof b,"%.0f FPS  %dX%d",fps_show,W,H); rect_fill(gs_,gs_,text_w(b)+2*gs_,8*gs_,0,160); text_at(2*gs_,2*gs_,b,0x60FF80); }
        if(toast[0] && SDL_GetTicks64()<toast_until){ int w=text_w(toast)+6*gs_; rect_fill((dw_-w)/2,dh_-12*gs_,w,9*gs_,0,170); text_at((dw_-w)/2+3*gs_,dh_-12*gs_+gs_*3/2,toast,0xFFFFFF); }
        if(menu_open) menu_draw();
        touch_draw();
        if(g_paused_focus && !menu_open){ rect_fill(0,0,dw_,dh_,0,120); gs_*=2; const char *s="PAUSED"; text_at((dw_-text_w(s))/2,dh_/2-3*gs_,s,0xFFFFFF); }
    }
    if(shot_pending){ shot_pending=0; char p[1200]; time_t t=time(0); struct tm *tm=localtime(&t); char ts[64]; strftime(ts,sizeof ts,"%Y%m%d_%H%M%S",tm);
        snprintf(p,sizeof p,"%sbugstar_%s.png",g_shotdir,ts); if(plat_write_png(p,pix,W,H)){ char m[200]; snprintf(m,sizeof m,"SAVED %s",p); say_toast("SCREENSHOT SAVED"); fprintf(stderr,"%s\n",m); } }
    if(g_autoshot_path && g_autoshot_frame>=0 && pc_frames()>=g_autoshot_frame){
        SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,dw_,dh_,32,SDL_PIXELFORMAT_ARGB8888);
        if(s && SDL_RenderReadPixels(ren,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0){ plat_write_png(g_autoshot_path,s->pixels,dw_,dh_); fprintf(stderr,"window shot %dx%d (draw %dx%d) -> %s\n",dw_,dh_,W,H,g_autoshot_path); }
        if(s) SDL_FreeSurface(s); g_autoshot_frame=-1; g_autoshot_path=0; if(g_stop_after>=0 && (menu_open || pc_frames()>=g_stop_after)) pc_quit_now(); }
    SDL_RenderPresent(ren);
    sim_vsync();
}
// ------------------------------------------------------------------------------------------------ input
static void pad_open(void){
    if(pad) return;
    for(int i=0;i<SDL_NumJoysticks();i++) if(SDL_IsGameController(i)){ pad=SDL_GameControllerOpen(i); if(pad){ pad_rumble_ok=SDL_GameControllerHasRumble(pad); fprintf(stderr,"gamepad: %s\n",SDL_GameControllerName(pad)); say_toast("GAMEPAD CONNECTED"); break; } }
}
static uint8_t kst[512]; static uint16_t g_lastkeys;

// ---- patch 30: on-screen touch pad (Android only). Fingers are tracked here; read_keys ORs touch_keys() in; touch-to-mouse is switched off (hint in plat_open).
#ifdef PC_ANDROID
static void open_menu(void);
#if 0      /* patch 39: the old touch pad, replaced below (it was fixed to the landscape layout) */
typedef struct { SDL_FingerID id; float x, y; int on; } TFing;
static TFing tfing[10];
typedef struct { int cx, cy, r, bit; const char *lab; } TBtn;
static int t_u(void){ int u=dh_/6; return u<40?40:u; }
static int t_btns(TBtn *b){ int u=t_u(), n=0;
    b[n++]=(TBtn){dw_-u,       dh_-u*5/2, u*6/10, 1<<0, "A"};
    b[n++]=(TBtn){dw_-u*5/2,   dh_-u*3/2, u*6/10, 1<<1, "B"};
    b[n++]=(TBtn){u,           u*7/10,    u*55/100, 1<<9, "L"};
    b[n++]=(TBtn){dw_-u,       u*7/10,    u*55/100, 1<<8, "R"};
    b[n++]=(TBtn){dw_/2-u*6/5, dh_-u/2,   u*4/10, 1<<2, "SEL"};
    b[n++]=(TBtn){dw_/2+u*6/5, dh_-u/2,   u*4/10, 1<<3, "STA"};
    b[n++]=(TBtn){dw_/2,       u/2,       u*4/10, 1<<15, "MENU"};
    return n; }
static uint16_t t_hit(int px, int py){        // what a finger at (px,py) presses: GBA key bits, plus bit 15 = the MENU button
    int u=t_u(); uint16_t m=0; TBtn b[8]; int n=t_btns(b);
    int cx=u*2, cy=dh_-u*2, dx=px-cx, dy=py-cy, dz=u/3;
    if(dx*dx+dy*dy < (long)u*2*u*2 && (abs(dx)>dz || abs(dy)>dz)){
        if(abs(dx)*10 > abs(dy)*4) m|= dx>0 ? 1<<4 : 1<<5;
        if(abs(dy)*10 > abs(dx)*4) m|= dy>0 ? 1<<7 : 1<<6; }
    for(int i=0;i<n;i++){ long ex=px-b[i].cx, ey=py-b[i].cy, r=b[i].r*13/10; if(ex*ex+ey*ey<r*r) m|=b[i].bit; }
    return m; }
static uint16_t touch_keys(void){ uint16_t m=0; for(int i=0;i<10;i++) if(tfing[i].on) m|=t_hit((int)(tfing[i].x*dw_),(int)(tfing[i].y*dh_)); return m&0x7FFF; }
static void touch_event(const SDL_Event *e){
    int k=-1, fr=-1; for(int i=0;i<10;i++){ if(tfing[i].on && tfing[i].id==e->tfinger.fingerId) k=i; if(!tfing[i].on && fr<0) fr=i; }
    if(e->type==SDL_FINGERUP){ if(k>=0) tfing[k].on=0; return; }
    if(k<0){ if(fr<0) return; k=fr; tfing[k].id=e->tfinger.fingerId; tfing[k].on=1; }
    tfing[k].x=e->tfinger.x; tfing[k].y=e->tfinger.y;
    if(e->type!=SDL_FINGERDOWN) return;
    uint16_t m=t_hit((int)(e->tfinger.x*dw_),(int)(e->tfinger.y*dh_));
    if(menu_open){          // in the options the pad steers the menu: arrows, A = enter, B / MENU = back
        if(m&(1<<6)) menu_key(SDLK_UP,0); else if(m&(1<<7)) menu_key(SDLK_DOWN,0); else if(m&(1<<5)) menu_key(SDLK_LEFT,0); else if(m&(1<<4)) menu_key(SDLK_RIGHT,0);
        else if(m&1) menu_key(SDLK_RETURN,0); else if(m&((1<<1)|(1<<15))) menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    } else if(m&(1<<15)) open_menu(); }
static void touch_draw(void){
    int u=t_u(); uint16_t on=touch_keys(); TBtn b[8]; int n=t_btns(b);
    static const struct { int dx, dy, bit; const char *l; } D[4]={{0,-1,1<<6,"^"},{0,1,1<<7,"V"},{-1,0,1<<5,"<"},{1,0,1<<4,">"}};
    for(int i=0;i<4;i++){ int x=u*2+D[i].dx*u*7/10, y=dh_-u*2+D[i].dy*u*7/10, h=u*3/10; rect_fill(x-h,y-h,2*h,2*h,0xFFFFFF,(on&D[i].bit)?130:55); text_at(x-text_w(D[i].l)/2,y-5*gs_/2,D[i].l,0xFFFFFF); }
    for(int i=0;i<n;i++){ int r=b[i].r; int p=b[i].bit<(1<<15) && (on&b[i].bit); rect_fill(b[i].cx-r,b[i].cy-r,2*r,2*r,0xFFFFFF,p?130:55); text_at(b[i].cx-text_w(b[i].lab)/2,b[i].cy-5*gs_/2,b[i].lab,0xFFFFFF); } }
#endif      /* patch 39 */
// ---- patch 39: the on-screen touch pad, laid out from g_tl (landscape and portrait each keep their own) and the layout editor (menu: TOUCH PAD LAYOUT)
typedef struct { SDL_FingerID id; float x, y; int on; } TFing;
static TFing tfing[10];
typedef struct { int cx, cy, r; } TGeo;      // centre and radius in pixels (the pad: half the size of its touch area)
static const int tl_base[TC_N]={200,60,60,55,55,40,40,40};                       // radius in 1/100 of a unit
static const int tl_bit[TC_N]={0,1<<0,1<<1,1<<9,1<<8,1<<2,1<<3,1<<15};           // GBA key bits; bit 15 = the MENU button
static const char *const tl_lab[TC_N]={"","A","B","L","R","SEL","STA","MENU"};
static const char *const tl_nm[TC_N]={"PAD","A","B","L","R","SELECT","START","MENU"};
static int tl_o(void){ return dw_>=dh_ ? 0 : 1; }        // 0 landscape, 1 portrait
static int t_u(void){ int m=dw_<dh_?dw_:dh_; int u=tl_o()?m/7:m/6; return u<40?40:u; }
static int tl_alpha(void){ return g_talpha?g_talpha:20; }
static void tl_default(int o, int i, int *x, int *y){
    int u=t_u(), w=dw_, h=dh_;
    if(!o){ switch(i){
        case TC_DPAD: *x=u*2; *y=h-u*2; break;            case TC_A: *x=w-u; *y=h-u*5/2; break;
        case TC_B: *x=w-u*5/2; *y=h-u*3/2; break;         case TC_L: *x=u; *y=u*7/10; break;
        case TC_R: *x=w-u; *y=u*7/10; break;              case TC_SEL: *x=w/2-u*6/5; *y=h-u/2; break;
        case TC_STA: *x=w/2+u*6/5; *y=h-u/2; break;       default: *x=w/2; *y=u/2; break; } }
    else { switch(i){
        case TC_DPAD: *x=u*18/10; *y=h-u*26/10; break;    case TC_A: *x=w-u*9/10; *y=h-u*36/10; break;
        case TC_B: *x=w-u*23/10; *y=h-u*24/10; break;     case TC_L: *x=u; *y=h-u*54/10; break;
        case TC_R: *x=w-u; *y=h-u*54/10; break;           case TC_SEL: *x=w/2-u*6/5; *y=h-u/2; break;
        case TC_STA: *x=w/2+u*6/5; *y=h-u/2; break;       default: *x=w/2; *y=u/2; break; } }
}
static void tl_geo(TGeo *g){
    int o=tl_o(), u=t_u();
    for(int i=0;i<TC_N;i++){ TPos *p=&g_tl[o][i]; int x,y;
        if(p->set){ x=(int)((long)p->x*dw_/1000); y=(int)((long)p->y*dh_/1000); } else tl_default(o,i,&x,&y);
        int s=p->s?p->s:100; g[i].cx=x; g[i].cy=y; g[i].r=(int)((long)u*tl_base[i]/100*s/100); }
}
static uint16_t t_hit(int px, int py){        // what a finger at (px,py) presses: GBA key bits, plus bit 15 = the MENU button
    TGeo g[TC_N]; tl_geo(g); uint16_t m=0;
    int dx=px-g[TC_DPAD].cx, dy=py-g[TC_DPAD].cy, R=g[TC_DPAD].r, dz=R/6;
    if((long)dx*dx+(long)dy*dy < (long)R*R && (abs(dx)>dz || abs(dy)>dz)){
        if(abs(dx)*10 > abs(dy)*4) m|= dx>0 ? 1<<4 : 1<<5;
        if(abs(dy)*10 > abs(dx)*4) m|= dy>0 ? 1<<7 : 1<<6; }
    for(int i=1;i<TC_N;i++){ long ex=px-g[i].cx, ey=py-g[i].cy, r=g[i].r*13/10; if(ex*ex+ey*ey<r*r) m|=tl_bit[i]; }
    return m; }
static uint16_t touch_keys(void){ if(ted_on) return 0; uint16_t m=0; for(int i=0;i<10;i++) if(tfing[i].on) m|=t_hit((int)(tfing[i].x*dw_),(int)(tfing[i].y*dh_)); return m&0x7FFF; }
static void ted_touch(const SDL_Event *e);
static void touch_event(const SDL_Event *e){
    if(ted_on){ ted_touch(e); return; }
    int k=-1, fr=-1; for(int i=0;i<10;i++){ if(tfing[i].on && tfing[i].id==e->tfinger.fingerId) k=i; if(!tfing[i].on && fr<0) fr=i; }
    if(e->type==SDL_FINGERUP){ if(k>=0) tfing[k].on=0; return; }
    if(k<0){ if(fr<0) return; k=fr; tfing[k].id=e->tfinger.fingerId; tfing[k].on=1; }
    tfing[k].x=e->tfinger.x; tfing[k].y=e->tfinger.y;
    if(e->type!=SDL_FINGERDOWN) return;
    uint16_t m=t_hit((int)(e->tfinger.x*dw_),(int)(e->tfinger.y*dh_));
    if(menu_open){          // in the options the pad steers the menu: arrows, A = enter, B / MENU = back
        if(m&(1<<6)) menu_key(SDLK_UP,0); else if(m&(1<<7)) menu_key(SDLK_DOWN,0); else if(m&(1<<5)) menu_key(SDLK_LEFT,0); else if(m&(1<<4)) menu_key(SDLK_RIGHT,0);
        else if(m&1) menu_key(SDLK_RETURN,0); else if(m&((1<<1)|(1<<15))) menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
    } else if(m&(1<<15)) open_menu(); }
static void tl_outline(int x, int y, int w, int h, int t, uint32_t col){ rect_fill(x,y,w,t,col,255); rect_fill(x,y+h-t,w,t,col,255); rect_fill(x,y,t,h,col,255); rect_fill(x+w-t,y,t,h,col,255); }
static void tl_draw(uint16_t on, int sel, int edit){        // edit: brighter (the editor shows every button even at a low opacity) and the picked one is outlined
    TGeo g[TC_N]; tl_geo(g); int a=tl_alpha()*255/100, ap=a*12/5; if(ap>255) ap=255; if(edit&&a<90) a=90; if(ap<a) ap=a;
    static const struct { int dx, dy, bit; const char *l; } D[4]={{0,-1,1<<6,"^"},{0,1,1<<7,"V"},{-1,0,1<<5,"<"},{1,0,1<<4,">"}};
    int sv=gs_, m=dw_<dh_?dw_:dh_; gs_=m/150; if(gs_<2) gs_=2; if(gs_>8) gs_=8;
    for(int i=0;i<TC_N;i++){ int cx=g[i].cx, cy=g[i].cy, r=g[i].r;
        if(i==TC_DPAD){ for(int k=0;k<4;k++){ int x=cx+D[k].dx*r*7/20, y=cy+D[k].dy*r*7/20, h=r*3/20; rect_fill(x-h,y-h,2*h,2*h,0xFFFFFF,(on&D[k].bit)?ap:a); text_at(x-text_w(D[k].l)/2,y-5*gs_/2,D[k].l,0xFFFFFF); }
            if(edit) tl_outline(cx-r,cy-r,2*r,2*r,1,0x8890C8); }
        else { int p=tl_bit[i]<(1<<15) && (on&tl_bit[i]); rect_fill(cx-r,cy-r,2*r,2*r,0xFFFFFF,p?ap:a); text_at(cx-text_w(tl_lab[i])/2,cy-5*gs_/2,tl_lab[i],0xFFFFFF); }
        if(i==sel) tl_outline(cx-r-gs_,cy-r-gs_,2*r+2*gs_,2*r+2*gs_,gs_,0xFFD23C); }
    gs_=sv; }
static void touch_draw(void){ if(ted_on) return; tl_draw(touch_keys(),-1,0); }
static void touch_release_all(void){ memset(tfing,0,sizeof tfing); }      // (patch 43) back from the background: no finger is still 'down'

// ---- the layout editor: drag a button to move it, tap to pick it, then the panel changes its size; opacity is shared; landscape and portrait are separate layouts
static int ted_sel=-1, ted_drag=-1, ted_ox, ted_oy, ted_py=-1; static SDL_FingerID ted_fid;
enum { TA_SMALLER=1, TA_BIGGER, TA_LESS, TA_MORE, TA_RESETONE, TA_RESETALL, TA_DONE };
typedef struct { int x, y, w, h, act; const char *t; } TedB;
typedef struct { int x, y, w, h, gs, n; TedB b[8]; } TedP;
static int ted_hits(const TGeo *g, int x, int y, int w, int h){ int r=g->r*13/10; return g->cx+r>x && g->cx-r<x+w && g->cy+r>y && g->cy-r<y+h; }
static void ted_panel(TedP *p, const TGeo *g){
    int m=dw_<dh_?dw_:dh_, gs=m/150, gmax=dw_/108; if(gs>gmax) gs=gmax; if(gs<2) gs=2; if(gs>8) gs=8;
    int pw=100*gs; if(pw>dw_-8*gs) pw=dw_-8*gs; int rh=12*gs, ph=6*rh+2*gs, px=(dw_-pw)/2;
    int lo=2*gs, hi=dh_-ph-2*gs; if(hi<lo) hi=lo;
    int pref = tl_o()==0 ? (dh_-ph)/2 : dh_/12; if(pref<lo) pref=lo; if(pref>hi) pref=hi;
    int best=-1; long bestc=1L<<60;
    if(ted_py>=lo && ted_py<=hi){ long c=0; for(int i=0;i<TC_N;i++) if(ted_hits(&g[i],px,ted_py,pw,ph)) c+= i==ted_sel?1000:1; if(c==0) best=ted_py; }
    if(best<0){ for(int y=lo;y<=hi;y+=gs*2){ long c=0; for(int i=0;i<TC_N;i++) if(ted_hits(&g[i],px,y,pw,ph)) c+= i==ted_sel?1000:1; c=c*100000+labs((long)y-pref); if(c<bestc){ bestc=c; best=y; } } }
    if(best<0) best=pref; ted_py=best;
    p->x=px; p->y=best; p->w=pw; p->h=ph; p->gs=gs; int n=0, y0=best+gs+rh*2;
    int bw=10*gs, bx2=px+pw-2*gs-bw, bx1=px+pw-14*gs-bw;       // [ - ] value [ + ] at the right of a row
    p->b[n++]=(TedB){bx1,y0+gs,bw,rh-2*gs,TA_SMALLER,"-"}; p->b[n++]=(TedB){bx2,y0+gs,bw,rh-2*gs,TA_BIGGER,"+"};
    p->b[n++]=(TedB){bx1,y0+rh+gs,bw,rh-2*gs,TA_LESS,"-"}; p->b[n++]=(TedB){bx2,y0+rh+gs,bw,rh-2*gs,TA_MORE,"+"};
    int hw=(pw-6*gs)/2;
    p->b[n++]=(TedB){px+2*gs,y0+rh*2+gs,hw,rh-2*gs,TA_RESETONE,"RESET THIS"}; p->b[n++]=(TedB){px+4*gs+hw,y0+rh*2+gs,hw,rh-2*gs,TA_RESETALL,"RESET ALL"};
    p->b[n++]=(TedB){px+2*gs,y0+rh*3+gs,pw-4*gs,rh-2*gs,TA_DONE,"DONE"}; p->n=n;
}
static void ted_pin(int i){ TPos *p=&g_tl[tl_o()][i]; if(p->set) return; TGeo g[TC_N]; tl_geo(g); p->x=g[i].cx*1000/dw_; p->y=g[i].cy*1000/dh_; p->set=1; if(!p->s) p->s=100; }
static void ted_clamp(int i){ TGeo g[TC_N]; tl_geo(g); int r=g[i].r, x=g[i].cx, y=g[i].cy; TPos *p=&g_tl[tl_o()][i];
    if(2*r<dw_){ if(x<r) x=r; if(x>dw_-r) x=dw_-r; } if(2*r<dh_){ if(y<r) y=r; if(y>dh_-r) y=dh_-r; }
    p->x=x*1000/dw_; p->y=y*1000/dh_; }
static void ted_open(void){ memset(tfing,0,sizeof tfing); ted_on=1; ted_sel=-1; ted_drag=-1; ted_py=-1; say_toast("DRAG THE BUTTONS"); }
static void ted_close(void){ ted_on=0; ted_sel=-1; ted_drag=-1; cfg_save(); say_toast("TOUCH PAD SAVED"); }
static void ted_key(SDL_Keycode k){ if(k==SDLK_ESCAPE||k==SDLK_AC_BACK) ted_close(); }
static void ted_act(int act){
    int o=tl_o(); TPos *p = ted_sel>=0 ? &g_tl[o][ted_sel] : 0;
    switch(act){
    case TA_SMALLER: case TA_BIGGER: if(p){ ted_pin(ted_sel); int s=(p->s?p->s:100)+(act==TA_BIGGER?10:-10); p->s=s<50?50:s>200?200:s; ted_clamp(ted_sel); } break;
    case TA_LESS: case TA_MORE: { int a=tl_alpha()+(act==TA_MORE?5:-5); g_talpha=a<5?5:a>100?100:a; } break;
    case TA_RESETONE: if(p) memset(p,0,sizeof *p); break;
    case TA_RESETALL: memset(g_tl[o],0,sizeof g_tl[o]); g_talpha=0; ted_sel=-1; break;
    case TA_DONE: ted_close(); break; }
}
static void ted_touch(const SDL_Event *e){
    int x=(int)(e->tfinger.x*dw_), y=(int)(e->tfinger.y*dh_), o=tl_o(); TGeo g[TC_N]; tl_geo(g);
    if(e->type==SDL_FINGERUP){ if(ted_drag>=0 && e->tfinger.fingerId==ted_fid) ted_drag=-1; return; }
    if(e->type==SDL_FINGERMOTION){ if(ted_drag>=0 && e->tfinger.fingerId==ted_fid){ int r=g[ted_drag].r, cx=x+ted_ox, cy=y+ted_oy;
            if(2*r<dw_){ if(cx<r) cx=r; if(cx>dw_-r) cx=dw_-r; } if(2*r<dh_){ if(cy<r) cy=r; if(cy>dh_-r) cy=dh_-r; }
            TPos *p=&g_tl[o][ted_drag]; p->set=1; p->x=cx*1000/dw_; p->y=cy*1000/dh_; if(!p->s) p->s=100; } return; }
    if(e->type!=SDL_FINGERDOWN || ted_drag>=0) return;
    TedP pn; ted_panel(&pn,g);
    if(x>=pn.x && x<pn.x+pn.w && y>=pn.y && y<pn.y+pn.h){ for(int i=0;i<pn.n;i++){ TedB *b=&pn.b[i]; if(x>=b->x && x<b->x+b->w && y>=b->y && y<b->y+b->h){ ted_act(b->act); break; } } return; }
    int hit=-1; for(int i=TC_N-1;i>=1 && hit<0;i--){ long ex=x-g[i].cx, ey=y-g[i].cy, r=g[i].r*13/10; if(ex*ex+ey*ey<r*r) hit=i; }
    if(hit<0){ long ex=x-g[0].cx, ey=y-g[0].cy; if(ex*ex+ey*ey<(long)g[0].r*g[0].r) hit=0; }
    if(hit>=0){ ted_sel=hit; ted_drag=hit; ted_fid=e->tfinger.fingerId; ted_ox=g[hit].cx-x; ted_oy=g[hit].cy-y; ted_pin(hit); }
}
static void ted_draw(void){
    TGeo g[TC_N]; tl_geo(g); TedP pn; ted_panel(&pn,g); int gs=pn.gs, saved=gs_;
    rect_fill(0,0,dw_,dh_,0x000000,120); tl_draw(0,ted_sel,1);
    gs_=gs; rect_fill(pn.x,pn.y,pn.w,pn.h,0x0B1030,242); rect_fill(pn.x,pn.y,pn.w,gs,0xFFD23C,255); rect_fill(pn.x,pn.y+pn.h-gs,pn.w,gs,0xFFD23C,255);
    int rh=12*gs, y=pn.y+gs; char b[48]; int land=tl_o()==0;
    text_at(pn.x+2*gs,y+rh/2-3*gs,land?"TOUCH PAD: LANDSCAPE":"TOUCH PAD: PORTRAIT",0xFFD23C); y+=rh;
    text_at(pn.x+2*gs,y+rh/2-3*gs,"DRAG TO MOVE. TAP TO PICK",0xA8B4F0); y+=rh;
    snprintf(b,sizeof b,"SIZE: %s",ted_sel>=0?tl_nm[ted_sel]:"NONE"); text_at(pn.x+2*gs,y+rh/2-3*gs,b,ted_sel>=0?0xFFFFFF:0x8890C8);
    int ssz=ted_sel>=0?(g_tl[tl_o()][ted_sel].s?g_tl[tl_o()][ted_sel].s:100):0;
    if(ted_sel>=0){ snprintf(b,sizeof b,"%d%%",ssz); text_at(pn.b[0].x+pn.b[0].w+(pn.b[1].x-pn.b[0].x-pn.b[0].w-text_w(b))/2,y+rh/2-3*gs,b,0xFFFFFF); } y+=rh;
    text_at(pn.x+2*gs,y+rh/2-3*gs,"OPACITY",0xFFFFFF); snprintf(b,sizeof b,"%d%%",tl_alpha()); text_at(pn.b[2].x+pn.b[2].w+(pn.b[3].x-pn.b[2].x-pn.b[2].w-text_w(b))/2,y+rh/2-3*gs,b,0xFFFFFF);
    for(int i=0;i<pn.n;i++){ TedB *t=&pn.b[i]; int dis=(ted_sel<0 && (t->act==TA_SMALLER||t->act==TA_BIGGER||t->act==TA_RESETONE));
        rect_fill(t->x,t->y,t->w,t->h,dis?0x252C50:(t->act==TA_DONE?0x2E8B3A:0x2A3A9A),255); text_at(t->x+(t->w-text_w(t->t))/2,t->y+(t->h-6*gs)/2,t->t,dis?0x6E78A8:0xFFFFFF); }
    gs_=saved; }
#else
static uint16_t touch_keys(void){ return 0; }
static void touch_draw(void){}
static void touch_release_all(void){}
#endif
static uint16_t read_keys(void){
    uint16_t down=0; const Uint8 *ks=SDL_GetKeyboardState(NULL);
    for(int b=0;b<NB;b++){ for(int sl=0;sl<2;sl++){ int c=C.kb[b][sl]; if(c&&c<512&&(ks[c]||kst[c])) down|=1<<b; else if(c>=512&&ks[c]) down|=1<<b; } }
    if(pad){
        for(int b=0;b<NB;b++) if(SDL_GameControllerGetButton(pad,C.pad[b])) down|=1<<b;
        int dz=C.deadzone*32767/100; int lx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX), ly=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);
        if(lx>dz) down|=1<<4; if(lx<-dz) down|=1<<5; if(ly<-dz) down|=1<<6; if(ly>dz) down|=1<<7;
        if(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000) down|=1<<8; if(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000) down|=1<<9;
    }
    if(C.mouse && !menu_open && pc_mouse_x>=0){ if(pc_mouse_b&1) down|=1; if(pc_mouse_b&2) down|=2; }
    if(C.mouse && !menu_open && pc_mouse_wheel){ if(pc_mouse_wheel>0){ down|=1<<6; pc_mouse_wheel--; } else { down|=1<<7; pc_mouse_wheel++; } }
    down|=touch_keys();
    if((down&(1<<4))&&(down&(1<<5))) down&=~((1<<4)|(1<<5)); if((down&(1<<6))&&(down&(1<<7))) down&=~((1<<6)|(1<<7));
    return down;
}
static void rumble_frame(void){
    static int hist[8], hi; if(!pad||!pad_rumble_ok||!C.rumble){ return; }
    hist[hi++&7]=pc_motor(); int s=0; for(int i=0;i<8;i++) s+=hist[i];
    if(s>0 && !menu_open){ int lo=s*8000, hi2=s*5000; if(lo>65535)lo=65535; SDL_GameControllerRumble(pad,(Uint16)lo,(Uint16)hi2,60); }
}
// ---- the app's life cycle (Android: home button, another app, screen off; also SDL_APP_TERMINATING). In the background the game does not step and makes no sound; coming back
// starts a fresh clock (g_pace_reset): the minutes it was away are not played catch-up. The save is flushed on the game's own thread (g_app_save, in pump_events).
static void touch_release_all(void);
static void app_background(void){ g_bg=1; g_app_save=1; if(adev) SDL_PauseAudioDevice(adev,1); }
static void app_foreground(void){
    if(!g_bg && !g_app_resume) return;
    g_bg=0; g_app_resume=0; g_pace_reset=1; memset(kst,0,sizeof kst); touch_release_all();
    if(adev){ __atomic_store_n(&rd_i,__atomic_load_n(&wr_i,__ATOMIC_ACQUIRE),__ATOMIC_RELEASE); SDL_PauseAudioDevice(adev,0); }      // the old queued sound is dropped
}
static int SDLCALL app_watch(void *u, SDL_Event *e){        // called at once on SDL's own thread (the game thread may already be asleep inside SDL): only sets flags
    (void)u;
    if(e->type==SDL_APP_WILLENTERBACKGROUND || e->type==SDL_APP_DIDENTERBACKGROUND || e->type==SDL_APP_TERMINATING){ g_bg=1; g_app_save=1; }
    else if(e->type==SDL_APP_DIDENTERFOREGROUND || e->type==SDL_APP_WILLENTERFOREGROUND){ g_app_resume=1; }
    return 1;
}
static void open_menu(void){ menu_open=1; menu_page=P_MAIN; menu_sel=0; menu_wait=0; mp_choice=0; for(int i=0;i<7;i++) if(mp_val(i)==C.max_mp) mp_choice=i; }
static void inject_step(void);
static void pump_events(void){
    SDL_Event e; inject_step();
    if(g_app_save){ g_app_save=0; pc_save_flush(1); cfg_save(); }          // the app is going to the background (Android): the save is on disk before the system may kill the process
    while(SDL_PollEvent(&e)){
        switch(e.type){
        case SDL_QUIT: cfg_save(); pc_quit_now(); break;
        case SDL_DISPLAYEVENT: g_pace_reset=1; break;                 // the monitor changed (refresh rate, plugged in/out): the game's clock does not care
        case SDL_APP_WILLENTERBACKGROUND: case SDL_APP_DIDENTERBACKGROUND: case SDL_APP_TERMINATING: app_background(); break;
        case SDL_APP_WILLENTERFOREGROUND: case SDL_APP_DIDENTERFOREGROUND: app_foreground(); break;
        case SDL_WINDOWEVENT:
            g_pace_reset=1;      // any window event (move, size, minimise, restore, focus): the next frame starts a fresh clock, no burst of catch-up frames
            if(e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED && !C.fullscreen && !(SDL_GetWindowFlags(win)&SDL_WINDOW_MINIMIZED)){ int w,h; SDL_GetWindowSize(win,&w,&h); if(w>=240&&h>=160){ C.win_w=w; C.win_h=h; } }
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) memset(kst,0,sizeof kst);
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST && C.pause_unfocus) g_paused_focus=1;
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) g_paused_focus=0;
            break;
        case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            if(menu_open && !menu_wait){ int ex = e.type==SDL_MOUSEMOTION ? e.motion.x : e.button.x, ey = e.type==SDL_MOUSEMOTION ? e.motion.y : e.button.y;
                int hit = m_lh>0 && ex>=m_x0 && ex<m_x0+m_pw ? (ey-(m_y1-m_gs))/m_lh : -1; if(ey<m_y1-m_gs) hit=-1; if(hit>=m_n) hit=-1;
                if(e.type==SDL_MOUSEMOTION){ if(hit>=0) menu_sel=hit; }
                else if(e.type==SDL_MOUSEBUTTONDOWN){
                    if(e.button.button==SDL_BUTTON_RIGHT) menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                    else if(e.button.button==SDL_BUTTON_LEFT){ if(menu_page==P_ABOUT) menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);
                        else if(hit>=0){ menu_sel=hit; Item it[24]; int n; menu_build(it,&n); Item *q=&it[hit];
                            if(q->kind==0) menu_key(SDLK_RETURN,0); else if(ex < m_x0+m_pw/2 + (q->kind==2?0:m_pw/4)) menu_key(SDLK_LEFT,0); else menu_key(SDLK_RIGHT,0); } } }
                break; }
            if(menu_open) break;
            { int W,H,linear,smooth; SDL_Rect dst; frame_geometry(&W,&H,&dst,&linear,&smooth); int ex = e.type==SDL_MOUSEMOTION ? e.motion.x : e.button.x, ey = e.type==SDL_MOUSEMOTION ? e.motion.y : e.button.y;
            int mx=(ex-dst.x)*240/(dst.w?dst.w:1), my=(ey-dst.y)*160/(dst.h?dst.h:1); if(mx<0||my<0||mx>=240||my>=160){ pc_mouse_x=-1; pc_mouse_y=-1; } else { pc_mouse_x=mx; pc_mouse_y=my; }
            if(e.type!=SDL_MOUSEMOTION){ int bit = e.button.button==SDL_BUTTON_LEFT ? 1 : e.button.button==SDL_BUTTON_RIGHT ? 2 : 0; if(e.type==SDL_MOUSEBUTTONDOWN) pc_mouse_b|=bit; else pc_mouse_b&=~bit; } break; }
        case SDL_MOUSEWHEEL: if(menu_open){ if(e.wheel.y>0) menu_key(SDLK_UP,0); else if(e.wheel.y<0) menu_key(SDLK_DOWN,0); } else pc_mouse_wheel += e.wheel.y; break;
#ifdef PC_ANDROID
        case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP: touch_event(&e); break;
#endif
        case SDL_CONTROLLERDEVICEADDED: pad_open(); break;
        case SDL_CONTROLLERDEVICEREMOVED: if(pad && e.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))){ SDL_GameControllerClose(pad); pad=0; say_toast("GAMEPAD REMOVED"); pad_open(); } break;
        case SDL_CONTROLLERBUTTONDOWN:
            if(menu_open) menu_pad(e.cbutton.button);
            else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_GUIDE) open_menu();
            else if((e.cbutton.button==SDL_CONTROLLER_BUTTON_START && SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_BACK)) || (e.cbutton.button==SDL_CONTROLLER_BUTTON_BACK && SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_START))) open_menu();
            break;
        case SDL_KEYUP: if(e.key.keysym.scancode<512) kst[e.key.keysym.scancode]=0; break;
        case SDL_KEYDOWN:
            if(e.key.keysym.scancode<512) kst[e.key.keysym.scancode]=1;
            if(e.key.repeat && !menu_open) break;
            if(e.key.keysym.sym==SDLK_F11 || (e.key.keysym.sym==SDLK_RETURN && (e.key.keysym.mod&KMOD_ALT))){ toggle_fullscreen(); break; }
            if(e.key.keysym.sym==SDLK_F12){ shot_pending=1; break; }
            if(e.key.keysym.sym==SDLK_F3){ C.show_fps=!C.show_fps; break; }
            if(menu_open){ if(e.key.keysym.sym==SDLK_AC_BACK) menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE); else menu_key(e.key.keysym.sym,e.key.keysym.scancode); break; }
            if(e.key.keysym.sym==SDLK_ESCAPE || e.key.keysym.sym==SDLK_F1 || e.key.keysym.sym==SDLK_AC_BACK){ open_menu(); break; }
            break;
        }
    }
}


// ---- test hook (BUGSTAR_EVENTS=file): events pushed into SDL's own queue (dummy video driver works), to test keyboard, mouse, gamepad, Esc options, F11, F12, rebinding.
//   each line: "<pumps to wait> <command>"; commands: key down|up NAME | pad attach | pad button NAME 0|1 | pad axis NAME VALUE | mouse X Y | click L|R 0|1 | wheel N | dump | quit
static FILE *inj_f; static int inj_wait=-1, inj_mx, inj_my; static char inj_cmd[200]; static int inj_vid=-1; static int inj_init;
static void inj_dump(void){ fprintf(stderr,"EV dump: menu=%d page=%d sel=%d wait=%d fs=%d keys=%03x mouse=%d,%d b=%d pad=%s vol=%d shots=%d kb_A=%s/%s pad_A=%s\n",menu_open,menu_page,menu_sel,menu_wait,C.fullscreen,g_lastkeys,pc_mouse_x,pc_mouse_y,pc_mouse_b,pad?"yes":"no",C.volume,
    (int)(shot_pending),C.kb[0][0]?SDL_GetScancodeName(C.kb[0][0]):"-",C.kb[0][1]?SDL_GetScancodeName(C.kb[0][1]):"-",SDL_GameControllerGetStringForButton(C.pad[0])); }
static void inject_step(void){
    if(!inj_init){ inj_init=1; const char *e=getenv("BUGSTAR_EVENTS"); if(e&&*e) inj_f=fopen(e,"r"); }
    if(!inj_f) return;
    if(inj_wait<0){ char ln[200]; if(!fgets(ln,sizeof ln,inj_f)){ fclose(inj_f); inj_f=0; return; } if(ln[0]=='#'||ln[0]=='\n'){ inj_wait=0; snprintf(inj_cmd,sizeof inj_cmd,"nop"); return; } int w=0,n=0; if(sscanf(ln,"%d %n",&w,&n)<1){ inj_wait=0; snprintf(inj_cmd,sizeof inj_cmd,"nop"); return; } inj_wait=w; snprintf(inj_cmd,sizeof inj_cmd,"%s",ln+n); }
    if(inj_wait>0){ inj_wait--; return; }
    inj_wait=-1; char a[64]="",b[64]="",c[64]=""; int x=0,y=0,k; SDL_Event e; SDL_zero(e);
    if(sscanf(inj_cmd,"key %63s %63s",a,b)==2){ SDL_Scancode sc=SDL_GetScancodeFromName(b); if(sc==SDL_SCANCODE_UNKNOWN){ fprintf(stderr,"EV bad key %s\n",b); return; } e.type=!strcmp(a,"down")?SDL_KEYDOWN:SDL_KEYUP; e.key.keysym.scancode=sc; e.key.keysym.sym=SDL_GetKeyFromScancode(sc); e.key.state=e.type==SDL_KEYDOWN; SDL_PushEvent(&e); }
    else if(!strncmp(inj_cmd,"pad attach",10)){
        #if SDL_VERSION_ATLEAST(2,0,16)
        SDL_VirtualJoystickDesc d; SDL_zero(d); d.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION; d.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER; d.naxes=SDL_CONTROLLER_AXIS_MAX; d.nbuttons=SDL_CONTROLLER_BUTTON_MAX; d.name="BUGSTAR TEST PAD";
        inj_vid=SDL_JoystickAttachVirtualEx(&d); fprintf(stderr,"EV pad attach -> %d\n",inj_vid); if(inj_vid>=0) SDL_JoystickOpen(inj_vid);
        #endif
    }
    else if(sscanf(inj_cmd,"pad button %63s %d",a,&k)==2){ SDL_Joystick *j=inj_vid>=0?SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(inj_vid)):0; int bt=SDL_GameControllerGetButtonFromString(a); if(j&&bt>=0) SDL_JoystickSetVirtualButton(j,bt,k); else fprintf(stderr,"EV pad button failed %s\n",a); }
    else if(sscanf(inj_cmd,"pad axis %63s %d",a,&k)==2){ SDL_Joystick *j=inj_vid>=0?SDL_JoystickFromInstanceID(SDL_JoystickGetDeviceInstanceID(inj_vid)):0; int ax=SDL_GameControllerGetAxisFromString(a); if(j&&ax>=0) SDL_JoystickSetVirtualAxis(j,ax,k); else fprintf(stderr,"EV pad axis failed %s\n",a); }
    else if(sscanf(inj_cmd,"mouse %d %d",&x,&y)==2){ inj_mx=x; inj_my=y; e.type=SDL_MOUSEMOTION; e.motion.x=x; e.motion.y=y; SDL_PushEvent(&e); }
    else if(sscanf(inj_cmd,"click %63s %d",a,&k)==2){ e.type=k?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP; e.button.button=a[0]=='R'?SDL_BUTTON_RIGHT:SDL_BUTTON_LEFT; e.button.x=inj_mx; e.button.y=inj_my; e.button.state=k; SDL_PushEvent(&e); }
    else if(sscanf(inj_cmd,"wheel %d",&k)==1){ e.type=SDL_MOUSEWHEEL; e.wheel.y=k; SDL_PushEvent(&e); }
    else if(!strncmp(inj_cmd,"dump",4)) inj_dump();
    else if(!strncmp(inj_cmd,"quit",4)){ pc_quit_now(); }
    (void)b; (void)c;
}
// ---- test hook (BUGSTAR_FAKEPAD=1): a virtual gamepad that is plugged in, pressed, unplugged and plugged in again, to test hot-plug, reading and rebinding without hardware
static int fake_id=-1, fake_idx=-1;
static void fakepad_frame(long fr){
    static int on=-2; if(on==-2){ const char *e=getenv("BUGSTAR_FAKEPAD"); on=e&&*e&&*e!='0'; } if(!on) return;
    #if SDL_VERSION_ATLEAST(2,0,16)
    SDL_VirtualJoystickDesc d; SDL_zero(d); d.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION; d.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER; d.naxes=SDL_CONTROLLER_AXIS_MAX; d.nbuttons=SDL_CONTROLLER_BUTTON_MAX; d.name="BUGSTAR FAKE PAD";
    SDL_Joystick *j = fake_id>=0 ? SDL_JoystickFromInstanceID(fake_id) : 0;
    if(fr==20||fr==200){ int i=SDL_JoystickAttachVirtualEx(&d); fprintf(stderr,"fakepad: attach -> device %d at frame %ld\n",i,fr); if(i>=0){ SDL_Joystick *jj=SDL_JoystickOpen(i); fake_id=SDL_JoystickInstanceID(jj); fake_idx=i; } }
    if(j){ if(fr==60) SDL_JoystickSetVirtualButton(j,SDL_CONTROLLER_BUTTON_A,1); if(fr==70) SDL_JoystickSetVirtualButton(j,SDL_CONTROLLER_BUTTON_A,0);
           if(fr==80) SDL_JoystickSetVirtualAxis(j,SDL_CONTROLLER_AXIS_LEFTX,32000); if(fr==90) SDL_JoystickSetVirtualAxis(j,SDL_CONTROLLER_AXIS_LEFTX,0);
           if(fr==110) SDL_JoystickSetVirtualButton(j,SDL_CONTROLLER_BUTTON_X,1); if(fr==120) SDL_JoystickSetVirtualButton(j,SDL_CONTROLLER_BUTTON_X,0); }
    if(fr==150 && j){ SDL_JoystickDetachVirtual(fake_idx); fprintf(stderr,"fakepad: detach at frame %ld\n",fr); fake_id=-1; }
    #endif
}
// ------------------------------------------------------------------------------------------------ the frame (patch 43: GBA-exact pacing)
// The game steps once per vblank of the real GBA: 16777216 Hz / 280896 cycles = 59.7275 frames a second. Nothing else may set that speed: not the monitor (60, 75, 90, 120, 144,
// 165 Hz), not vsync, not the window, not the audio buffer, not how fast the PC or the phone is. The game steps from a fixed-timestep accumulator on the monotonic clock:
//   g_due is when the current frame should end. Every frame it moves on by exactly one GBA frame (a fraction of a tick, carried), never "to now".
//   Early: wait for it (sleep, then spin the last bit, so a 1 ms timer does not make the frames uneven). Late by up to PACE_MAX_LAG frames: do not wait, catch up.
//   Later than that (a drag of the window, a long load, the phone's sleep): drop the lost time (resync: the late frame begins now). The game never runs at double speed to make it up.
// The picture is shown once per game frame; on a 144 Hz screen with vsync the show waits for the screen, but that wait is inside the frame budget, not added to it.
#define GBA_FRAME_SECONDS (280896.0/16777216.0)
#define PACE_MAX_LAG 4
static double g_due, g_slack_ms; static unsigned long g_resyncs, g_catchups, g_waits;
static inline double now_ticks(void){ return (double)SDL_GetPerformanceCounter(); }
static inline void cpu_relax(void){
#if defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("pause");
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#endif
}
static double pace_period(void){          // perf-counter ticks of one game frame at the current game speed (UNIVERSE LAWS: GAME SPEED, 100 = the GBA)
    extern int pc_time_pct; int tp = pc_time_pct < 10 ? 10 : pc_time_pct > 2000 ? 2000 : pc_time_pct;
    return (double)t_freq*GBA_FRAME_SECONDS*100.0/(double)tp;
}
static void wait_until(double due){        // sleep while there is plenty of time (SDL_Delay is 1 ms coarse, or 15 ms on an old Windows timer), spin the last g_slack_ms
    for(;;){
        double ms=(due-now_ticks())*1000.0/(double)t_freq; if(ms<=0) return;
        if(ms>g_slack_ms+0.5){
            double a=now_ticks(); Uint32 d=(Uint32)(ms-g_slack_ms); if(d<1) d=1; SDL_Delay(d);
            double over=(now_ticks()-a)*1000.0/(double)t_freq-(double)d;      // how late the sleep woke up: keep that much in reserve for the spin
            if(over>g_slack_ms-0.4){ g_slack_ms=over+0.6; if(g_slack_ms>8.0) g_slack_ms=8.0; } else if(g_slack_ms>1.0) g_slack_ms-=0.01;
        } else cpu_relax();
    }
}
static void sim_vsync(void){               // test hook BUGSTAR_SIMVSYNC=<hz>: when vsync is on, the picture's show waits for the next tick of a pretend monitor (tests/speed.sh: 144 Hz)
    static double hz=-1, t0; if(hz<0){ const char *e=getenv("BUGSTAR_SIMVSYNC"); hz=e?atof(e):0; t0=now_ticks(); }
    if(hz>0 && C.vsync){ double per=(double)t_freq/hz; double k=floor((now_ticks()-t0)/per)+1; wait_until(t0+k*per); }
}
static void sim_stall(long fr){            // test hook BUGSTAR_SIM_STALL=frame:ms,frame:ms: the window's thread stops for a while (a drag of the title bar, a minimise)
    static int init, n, at[8], ms[8]; if(!init){ init=1; const char *e=getenv("BUGSTAR_SIM_STALL"); while(e&&*e&&n<8){ int a=0,m=0,k=0; if(sscanf(e,"%d:%d%n",&a,&m,&k)<2) break; at[n]=a; ms[n]=m; n++; e+=k; if(*e==',') e++; } }
    for(int i=0;i<n;i++) if(at[i]==fr) SDL_Delay((Uint32)ms[i]);
}
static void pace_stats(void){ if(getenv("BUGSTAR_PACELOG")) fprintf(stderr,"pace: frames %ld resyncs %lu catchups %lu waits %lu audio skips %lu pads %lu slack %.2f ms underruns %lu\n",pc_frames(),g_resyncs,g_catchups,g_waits,a_skips,a_pads,g_slack_ms,underruns); }
void plat_frame(void){
    long fr=pc_frames(); { static const int an[4]={0,500,2000,10000}; if(!getenv("PC_SHIPS")) pc_ships_mode=C.ships; if(!getenv("PC_ARMADA")) pc_armada_n=an[C.armada]; }
    audio_frame();
    double period=pace_period(), now=now_ticks();
    // g_due is when THIS frame should have begun (= when the last one ended). A frame that arrives more than PACE_MAX_LAG frames late starts a fresh clock: the lost time is dropped.
    if(!g_fast && now-g_due > PACE_MAX_LAG*period){ g_due=now; g_resyncs++; }
    int late = now - g_due > period;                          // already more than a whole frame behind: do not spend time showing this one
    if(g_fast ? (fr&15)==0 : (!late || (fr&7)==0)) present_game(1);
    fps_n++; { uint64_t n=SDL_GetPerformanceCounter(); if(n-fps_t0>=t_freq){ fps_show=fps_n*(double)t_freq/(double)(n-fps_t0); fps_n=0; fps_t0=n; } }
    // ---- pace: the game steps at exactly 59.7275 a second, whatever the screen's refresh rate is
    g_due += period;                                           // when this frame ends: exactly one GBA frame after it began (the fraction of a tick is kept)
    now=now_ticks();
    if(g_fast) g_due=now;                                      // fast-forward (--fast): as quick as it goes, and nothing is owed afterwards
    else if(now>g_due) g_catchups++;                          // a little behind (less than PACE_MAX_LAG frames): the next frames do not wait until the clock is level again
    else { g_waits++; wait_until(g_due); }
    sim_stall(fr);
    pump_events();
    int was_paused=0;
    while(menu_open || g_paused_focus || g_bg){                // paused: the game does not step; the window keeps being drawn (not while the app is in the background)
        was_paused=1; uint64_t t0=SDL_GetPerformanceCounter();
        if(!g_bg) present_game(1);
        pump_events();
        if(g_bg) SDL_Delay(50);
        else if(!C.vsync) SDL_Delay(16);
        else if(SDL_GetPerformanceCounter()-t0<t_freq/200) SDL_Delay(8);
        if(!menu_open && !g_paused_focus && !g_bg) break;
        if(!g_bg) audio_frame();
    }
    if(was_paused || g_pace_reset || g_app_resume){ g_pace_reset=0; g_app_resume=0; g_due=now_ticks(); }      // a pause, a window move/resize/minimise, a monitor change: a fresh clock, no catch-up burst
    { const char *mp=getenv("BUGSTAR_MENU"); if(mp && fr==30 && !menu_open){ open_menu(); menu_page=atoi(mp); if(getenv("BUGSTAR_MENUSEL")) menu_sel=atoi(getenv("BUGSTAR_MENUSEL")); } }   // (test hook: open the options screen on a page)
    if(g_first_run && fr==45){ g_first_run=0; say_toast_ms("ARROWS/WASD MOVE  X/SPACE=A  Z/K=B  ENTER=START  ESC=OPTIONS",9000); }
    uint16_t k; fakepad_frame(fr);
    if(pc_script_keys(&k)) pc_set_keys(k); else { k=read_keys(); pc_set_keys(k); if(getenv("BUGSTAR_FAKEPAD") && k) fprintf(stderr,"fakepad: frame %ld keys=%03x pad=%s\n",fr,k,pad?SDL_GameControllerName(pad):"none"); }
    g_lastkeys=k; rumble_frame();
    { static long saved_at; if(fr-saved_at>=120){ saved_at=fr; pc_save_flush(0); } }
    if(g_stop_after>=0 && fr>=g_stop_after && !(g_autoshot_path && g_autoshot_frame>=0)){ pace_stats(); pc_quit_now(); }   // (a screenshot still waiting is taken first)
}
void plat_close(void){ pc_save_flush(1); cfg_save(); wav_close(); if(adev) SDL_CloseAudioDevice(adev); if(pad) SDL_GameControllerClose(pad); if(ren) SDL_DestroyRenderer(ren); if(win) SDL_DestroyWindow(win); SDL_Quit(); }

// ------------------------------------------------------------------------------------------------ start-up
#ifdef _WIN32
extern int __stdcall CreateDirectoryA(const char *, void *);
static void pc_mkdir_p(const char *path){ char t[1300]; size_t n=strlen(path); if(n>=sizeof t) return; memcpy(t,path,n+1);
    for(size_t i=1;i<=n;i++){ if(t[i]=='/'||t[i]=='\\'||t[i]==0){ char c=t[i]; t[i]=0; if(i>2||t[1]!=':') CreateDirectoryA(t,0); t[i]=c; if(!c) break; } } }
#endif
static void mkdirs(const char *p){ (void)p; }
#ifdef PC_ANDROID
#include <sys/stat.h>
static void android_mkdir_p(const char *path){
    char b[1300]; snprintf(b,sizeof b,"%s",path);
    for(char *q=b+1; *q; q++) if(*q=='/'){ *q=0; mkdir(b,0775); *q='/'; }
    mkdir(b,0775); }
#endif
const char *plat_save_path(void){ return g_savepath; }
int plat_paths(int argc, char **argv){            // finds the folders; call before the fake GBA starts (it loads the save)
    for(int i=1;i+1<argc;i++) if(!strcmp(argv[i],"--icon-png")){ int S=256; uint32_t *p=malloc((size_t)S*S*4); plat_icon_pixels(p,S); for(size_t k=0;k<(size_t)S*S;k++) if(!(p[k]>>24)) p[k]=0; plat_write_png_rgba(argv[i+1],p,S,S); exit(0); }
    const char *data=NULL; int portable=0;
    for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"--data")&&i+1<argc) data=argv[++i]; else if(!strcmp(argv[i],"--portable")) portable=1; }
    char *pp=NULL;
    if(data){ snprintf(g_pref,sizeof g_pref,"%s%s",data, (data[strlen(data)-1]=='/'||data[strlen(data)-1]=='\\')?"":"/"); }
    else {
        if(!portable){ char *bp=SDL_GetBasePath(); if(bp){ char t[1200]; snprintf(t,sizeof t,"%sportable.txt",bp); FILE *f=fopen(t,"r"); if(f){ fclose(f); portable=1; snprintf(g_pref,sizeof g_pref,"%sdata/",bp); } SDL_free(bp); } }
        else { char *bp=SDL_GetBasePath(); snprintf(g_pref,sizeof g_pref,"%sdata/",bp?bp:"./"); if(bp) SDL_free(bp); }
        if(!portable){ pp=SDL_GetPrefPath(NULL,"BUGSTAR"); if(!pp) pp=SDL_GetPrefPath("BUGSTAR","BUGSTAR"); snprintf(g_pref,sizeof g_pref,"%s",pp?pp:"./"); }
    }
    if(pp) SDL_free(pp);
#ifdef _WIN32
    pc_mkdir_p(g_pref);
#elif defined(PC_ANDROID)
    // patch 35: no shell on the launch path (system() forks the whole app); mkdir -p done here
    { char d[1300]; android_mkdir_p(g_pref); snprintf(d,sizeof d,"%sscreenshots",g_pref); android_mkdir_p(d); }
#else
    { char cmd[1300]; snprintf(cmd,sizeof cmd,"mkdir -p '%s' '%sscreenshots' 2>/dev/null",g_pref,g_pref); if(system(cmd)){} }
#endif
    snprintf(g_savepath,sizeof g_savepath,"%sbugstar.sav",g_pref); snprintf(g_cfgpath,sizeof g_cfgpath,"%ssettings.ini",g_pref); snprintf(g_shotdir,sizeof g_shotdir,"%sscreenshots/",g_pref);
#ifdef _WIN32
    { char d[1300]; snprintf(d,sizeof d,"%sscreenshots",g_pref); pc_mkdir_p(d); }
#endif
    (void)mkdirs;
    return 0;
}
int plat_open(int argc, char **argv){
    cfg_defaults(); { FILE *tf=fopen(g_cfgpath,"r"); if(tf) fclose(tf); else g_first_run=1; } cfg_load();
    int force_fs=-1; const char *drv=NULL;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--fullscreen")) force_fs=1; else if(!strcmp(argv[i],"--windowed")) force_fs=0;
        else if(!strcmp(argv[i],"--size")&&i+2<argc){ C.win_w=atoi(argv[i+1]); C.win_h=atoi(argv[i+2]); i+=2; }
        else if(!strcmp(argv[i],"--scale-mode")&&i+1<argc) C.scale_mode=atoi(argv[++i])?SM_SMOOTH:SM_CRISP;
        else if(!strcmp(argv[i],"--no-vsync")) C.vsync=0; else if(!strcmp(argv[i],"--no-audio")) g_no_audio=1;
        else if(!strcmp(argv[i],"--video-driver")&&i+1<argc) drv=argv[++i];
        else if(!strcmp(argv[i],"--auto-shot")&&i+2<argc){ g_autoshot_frame=atol(argv[i+1]); g_autoshot_path=argv[i+2]; i+=2; }
        else if(!strcmp(argv[i],"--stop-after")&&i+1<argc) g_stop_after=atol(argv[++i]);
        else if(!strcmp(argv[i],"--wav")&&i+1<argc){ g_wav_path=argv[++i]; }
        else if(!strcmp(argv[i],"--fast")) g_fast=1;
    }
    if(force_fs>=0) C.fullscreen=force_fs;
    if(C.win_w<240) C.win_w=960; if(C.win_h<160) C.win_h=640;
    if(drv) SDL_setenv("SDL_VIDEODRIVER",drv,1);
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS,"permonitorv2"); SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING,"0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1"); SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER,"0");
    SDL_SetHint(SDL_HINT_APP_NAME,"BUGSTAR");
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION,"1");      // patch 43: Windows: a 1 ms timer (not 15.6 ms), so the frame waits are even (SDL then calls timeBeginPeriod(1))
#ifdef PC_ANDROID
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS,"0"); SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS,"0");   // the touch pad replaces touch-as-mouse
#endif
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_TIMER)!=0){
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_TIMER)!=0){ fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; } g_no_audio=1; }
    win=SDL_CreateWindow("BUGSTAR   ARROWS/WASD MOVE   X/SPACE=A   Z/K=B   ENTER=START   ESC=OPTIONS",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,C.win_w,C.win_h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(C.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
    if(!win){ fprintf(stderr,"window: %s\n",SDL_GetError()); return 1; }
    SDL_SetWindowMinimumSize(win,240,160);
    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|(C.vsync?SDL_RENDERER_PRESENTVSYNC:0));
    if(!ren){ fprintf(stderr,"no accelerated renderer (%s): using the software one\n",SDL_GetError()); ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE); }
    if(!ren){ fprintf(stderr,"renderer: %s\n",SDL_GetError()); return 1; }
    { SDL_RendererInfo ri; if(SDL_GetRendererInfo(ren,&ri)==0) fprintf(stderr,"video: %s renderer, driver %s\n",ri.name,SDL_GetCurrentVideoDriver()); }
    make_icon(); font_init();
    SDL_ShowCursor(C.fullscreen?SDL_DISABLE:SDL_ENABLE);
    pad_open();
    t_freq=SDL_GetPerformanceFrequency(); fps_t0=SDL_GetPerformanceCounter(); g_due=(double)fps_t0;
#ifdef _WIN32
    g_slack_ms=2.0;
#else
    g_slack_ms=1.2;
#endif
    SDL_AddEventWatch(app_watch,NULL);      // the app's life cycle events arrive at once, even while the game thread sleeps inside SDL (Android in the background)
    if(g_wav_path){ g_wav=fopen(g_wav_path,"wb"); if(g_wav){ uint8_t h[44]={0}; fwrite(h,1,44,g_wav); } }
    audio_open();
    atexit(wav_close);
    return 0;
}
