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

int pc_motor(void);                 // pc_hw.c: is the (cartridge) rumble motor on right now
int pc_script_keys(uint16_t *k);    // pc_hw.c: keys from a --script file, if one was given (for tests)
long pc_frames(void);

#define NB 10
static const char *bname[NB]={"A","B","SELECT","START","RIGHT","LEFT","UP","DOWN","R","L"};
// ------------------------------------------------------------------------------------------------ settings
typedef struct {
    int fullscreen, scale_mode, vsync, show_fps, volume, mute, pause_unfocus, rumble, deadzone, max_mp;
    int kb[NB][2]; int pad[NB]; int win_w, win_h;
} Cfg;
enum { SM_CRISP, SM_SMOOTH, SM_COUNT };
static Cfg C;
static char g_pref[1024], g_cfgpath[1100], g_savepath[1100], g_shotdir[1100];
static void cfg_defaults(void){
    memset(&C,0,sizeof C);
    C.fullscreen=0; C.scale_mode=SM_CRISP; C.vsync=1; C.show_fps=0; C.volume=80; C.mute=0; C.pause_unfocus=1; C.rumble=1; C.deadzone=30; C.max_mp=0;
    C.win_w=960; C.win_h=640;
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
    fprintf(f,"fullscreen=%d\nscale_mode=%d\nvsync=%d\nshow_fps=%d\nvolume=%d\nmute=%d\npause_unfocus=%d\nrumble=%d\ndeadzone=%d\nmax_mp=%d\nwin_w=%d\nwin_h=%d\n",
        C.fullscreen,C.scale_mode,C.vsync,C.show_fps,C.volume,C.mute,C.pause_unfocus,C.rumble,C.deadzone,C.max_mp,C.win_w,C.win_h);
    for(int i=0;i<NB;i++){ fprintf(f,"key_%s=%s,%s\n",bname[i],C.kb[i][0]?SDL_GetScancodeName(C.kb[i][0]):"",C.kb[i][1]?SDL_GetScancodeName(C.kb[i][1]):"");
        fprintf(f,"pad_%s=%s\n",bname[i],SDL_GameControllerGetStringForButton(C.pad[i])?SDL_GameControllerGetStringForButton(C.pad[i]):""); }
    fclose(f);
}
static void cfg_load(void){
    FILE *f=fopen(g_cfgpath,"r"); if(!f) return; char ln[256];
    while(fgets(ln,sizeof ln,f)){
        char *e=strchr(ln,'='); if(!e||ln[0]=='#') continue; *e++=0; size_t n=strlen(e); while(n&&(e[n-1]=='\n'||e[n-1]=='\r')) e[--n]=0;
        int v=atoi(e);
        #define I(nm,fld,lo,hi) if(!strcmp(ln,nm)){ C.fld = v<lo?lo:v>hi?hi:v; continue; }
        I("fullscreen",fullscreen,0,1) I("scale_mode",scale_mode,0,SM_COUNT-1) I("vsync",vsync,0,1) I("show_fps",show_fps,0,1) I("volume",volume,0,100) I("mute",mute,0,1)
        I("pause_unfocus",pause_unfocus,0,1) I("rumble",rumble,0,1) I("deadzone",deadzone,5,80) I("max_mp",max_mp,0,40) I("win_w",win_w,240,16384) I("win_h",win_h,160,16384)
        if(!strncmp(ln,"key_",4)){ for(int i=0;i<NB;i++) if(!strcmp(ln+4,bname[i])){ char *c=strchr(e,','); char a[64]="",b[64]=""; if(c){ *c=0; snprintf(b,sizeof b,"%s",c+1); } snprintf(a,sizeof a,"%s",e);
                C.kb[i][0]=a[0]?SDL_GetScancodeFromName(a):0; C.kb[i][1]=b[0]?SDL_GetScancodeFromName(b):0; } }
        if(!strncmp(ln,"pad_",4)){ for(int i=0;i<NB;i++) if(!strcmp(ln+4,bname[i])){ int b=SDL_GameControllerGetButtonFromString(e); if(b!=SDL_CONTROLLER_BUTTON_INVALID) C.pad[i]=b; } }
    }
    fclose(f);
}
// ------------------------------------------------------------------------------------------------ state
static SDL_Window *win; static SDL_Renderer *ren; static SDL_Texture *tex, *fonttex;
static int tex_w, tex_h, tex_linear; static uint32_t *pix; static size_t pix_cap;
static int menu_open, menu_page, menu_sel, menu_wait;      // menu_wait: 1 = waiting for a key to bind, 2 = for a pad button
static int menu_slot;                                      // which key slot (0/1) a bind goes into
static uint64_t t_next, t_freq; static double fps_show; static int fps_n; static uint64_t fps_t0;
static char toast[80]; static uint64_t toast_until;
static SDL_GameController *pad; static int pad_rumble_ok;
static SDL_AudioDeviceID adev; static const char *g_autoshot_path; static long g_autoshot_frame=-1; static int g_headless_audio;
static int g_no_audio; static int dw_, dh_;                // drawable size in real pixels
static long g_stop_after=-1; static const char *g_wav_path; static FILE *g_wav; static long g_wav_n;
static int g_paused_focus, g_fast;

static void say_toast(const char *s){ snprintf(toast,sizeof toast,"%s",s); toast_until=SDL_GetTicks64()+2000; }

// ------------------------------------------------------------------------------------------------ PNG (stored, no compression library needed)
static uint32_t crc_t[256]; static void crc_init(void){ for(uint32_t n=0;n<256;n++){ uint32_t c=n; for(int k=0;k<8;k++) c=c&1?0xEDB88320u^(c>>1):c>>1; crc_t[n]=c; } }
static uint32_t crc_upd(uint32_t c,const uint8_t*b,size_t n){ for(size_t i=0;i<n;i++) c=crc_t[(c^b[i])&255]^(c>>8); return c; }
static void be32(uint8_t*p,uint32_t v){ p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void chunk(FILE*f,const char*t,const uint8_t*d,uint32_t n){ uint8_t h[8]; be32(h,n); memcpy(h+4,t,4); fwrite(h,1,8,f);
    uint32_t c=crc_upd(0xFFFFFFFFu,(uint8_t*)t,4); if(n){ fwrite(d,1,n,f); c=crc_upd(c,d,n); } uint8_t e[4]; be32(e,~c); fwrite(e,1,4,f); }
int plat_write_png(const char *path, const uint32_t *xrgb, int w, int h){
    if(!crc_t[1]) crc_init();
    FILE *f=fopen(path,"wb"); if(!f) return 0;
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10}; fwrite(sig,1,8,f);
    uint8_t ih[13]; be32(ih,w); be32(ih+4,h); ih[8]=8; ih[9]=2; ih[10]=0; ih[11]=0; ih[12]=0; chunk(f,"IHDR",ih,13);
    size_t rowb=(size_t)w*3+1, raw=rowb*h; uint8_t *rb=malloc(raw); 
    for(int y=0;y<h;y++){ uint8_t *r=rb+y*rowb; r[0]=0; for(int x=0;x<w;x++){ uint32_t p=xrgb[(size_t)y*w+x]; r[1+x*3]=p>>16; r[2+x*3]=p>>8; r[3+x*3]=p; } }
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
static void audio_frame(void){          // make this frame's sound and queue it
    int fill=(int)ring_fill(); int n=PC_AUDIO_RATE/60;
    if(adev){ int target=1000; int adj=(target-fill)/32; if(adj>24)adj=24; if(adj<-24)adj=-24; n+=adj; if(fill>6000) n=0; }
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
static void make_icon(void){            // the window icon, drawn here: a yellow star over a dark disc, a green bug in front
    const int S=64; SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,S,S,32,SDL_PIXELFORMAT_ARGB8888); if(!s) return;
    uint32_t *p=s->pixels;
    for(int y=0;y<S;y++) for(int x=0;x<S;x++){
        double dx=x-31.5, dy=y-31.5, r=sqrt(dx*dx+dy*dy); uint32_t c=0;
        if(r<30){ c=0xFF141B3C; if(r>27) c=0xFF2C3A7A; }
        // 5-point star
        double ang=atan2(dy,dx)+M_PI/2, k=fmod(ang+M_PI*2,2*M_PI/5)-M_PI/5; double rr=r*cos(k); if(rr<0)rr=-rr;
        double edge=(cos(M_PI/5)*14.0+0.0)/cos(k>0?k:-k); (void)rr;
        double sr = 27.0*cos(2*M_PI/5*0)/(1.0); (void)sr;
        double a2=fabs(k); double lim = 9.5/ (cos(a2)*0.75 + 0.0) ; (void)lim;
        double radial = 26.0 - 15.0*(a2/(M_PI/5)); if(r<radial && r<28) c=0xFFFFD23C;
        // the bug: body, head, legs
        double bx=dx, by=dy+6; if((bx*bx)/110+(by*by)/190<1) c=0xFF3DB04A;
        if((bx*bx)/110+((by-0)*(by-0))/190<1 && fabs(bx)<1.0) c=0xFF1F7A30;
        double hx=dx, hy=dy+19; if(hx*hx+hy*hy<30) c=0xFF2A8A38;
        if(fabs(bx)<7 && fabs(by+1)<2.5 && bx*bx/110+by*by/190<1) c=0xFF1F7A30;
        for(int l=-1;l<=1;l++){ double ly=by-l*7; if(fabs(ly)<1.2 && fabs(bx)>9 && fabs(bx)<17) c=0xFF1F7A30; }
        if(hx*hx+hy*hy<30 && ((fabs(hx)>2 && fabs(hx)<4.5) && fabs(hy+1)<1.6)) c=0xFFFFFFFF;
        p[y*S+x]=c; }
    SDL_SetWindowIcon(win,s); SDL_FreeSurface(s);
}
static void tex_ensure(int w, int h, int linear){
    if(tex && tex_w==w && tex_h==h && tex_linear==linear) return;
    if(tex) SDL_DestroyTexture(tex);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear?"linear":"nearest");
    tex=SDL_CreateTexture(ren,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
    tex_w=w; tex_h=h; tex_linear=linear;
    if((size_t)w*h>pix_cap){ free(pix); pix=malloc((size_t)w*h*4); pix_cap=(size_t)w*h; }
}
static int cap_pixels(void){ return C.max_mp ? C.max_mp*1000000 : 2400000; }
static void present_game(int with_overlay);
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
enum { P_MAIN, P_VIDEO, P_AUDIO, P_CONTROLS, P_PAD, P_ABOUT, P_COUNT };
typedef struct { int kind; const char *label; int *val; int lo, hi, step; const char *const *names; int action; } Item;   // kind 0 action, 1 choice/toggle, 2 slider
enum { A_RESUME=1, A_VIDEO, A_AUDIO, A_CONTROLS, A_PAD, A_ABOUT, A_QUIT, A_BACK, A_FULLSCREEN, A_VSYNC, A_RESETKEYS, A_RESETPAD, A_WINSIZE, A_SHOT, A_REBIND, A_MAINBACK };
static const char *const onoff[]={"OFF","ON"}; static const char *const scalenames[]={"CRISP (WHOLE NUMBERS)","SMOOTH (FITS THE WINDOW)"};
static const char *const mpnames[]={"AUTO","1 MEGAPIXEL","2","3","4","6","8"}; static int mp_choice;
static int mp_val(int i){ static const int v[]={0,1,2,3,4,6,8}; return v[i]; }
static void menu_build(Item *it, int *n){
    int k=0;
    switch(menu_page){
    case P_MAIN: it[k++]=(Item){0,"RESUME",0,0,0,0,0,A_RESUME}; it[k++]=(Item){0,"VIDEO",0,0,0,0,0,A_VIDEO}; it[k++]=(Item){0,"AUDIO",0,0,0,0,0,A_AUDIO};
        it[k++]=(Item){0,"KEYBOARD",0,0,0,0,0,A_CONTROLS}; it[k++]=(Item){0,"GAMEPAD",0,0,0,0,0,A_PAD}; it[k++]=(Item){0,"SCREENSHOT",0,0,0,0,0,A_SHOT};
        it[k++]=(Item){0,"ABOUT AND FOLDERS",0,0,0,0,0,A_ABOUT}; it[k++]=(Item){0,"QUIT GAME",0,0,0,0,0,A_QUIT}; break;
    case P_VIDEO: it[k++]=(Item){1,"FULLSCREEN",&C.fullscreen,0,1,1,onoff,A_FULLSCREEN}; it[k++]=(Item){1,"SCALING",&C.scale_mode,0,SM_COUNT-1,1,scalenames,0};
        it[k++]=(Item){1,"VSYNC",&C.vsync,0,1,1,onoff,A_VSYNC}; it[k++]=(Item){1,"MAX DRAW SIZE",&mp_choice,0,6,1,mpnames,0}; it[k++]=(Item){1,"SHOW FPS",&C.show_fps,0,1,1,onoff,0};
        it[k++]=(Item){1,"PAUSE WHEN UNFOCUSED",&C.pause_unfocus,0,1,1,onoff,0}; it[k++]=(Item){0,"WINDOW SIZE: NEXT",0,0,0,0,0,A_WINSIZE}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_AUDIO: it[k++]=(Item){2,"VOLUME",&C.volume,0,100,5,0,0}; it[k++]=(Item){1,"MUTE",&C.mute,0,1,1,onoff,0}; it[k++]=(Item){1,"RUMBLE (GAMEPAD)",&C.rumble,0,1,1,onoff,0}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_CONTROLS: for(int i=0;i<NB;i++) it[k++]=(Item){0,bname[i],0,0,0,0,0,A_REBIND+100*i}; it[k++]=(Item){0,"RESET KEYS",0,0,0,0,0,A_RESETKEYS}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
    case P_PAD: for(int i=0;i<NB;i++) it[k++]=(Item){0,bname[i],0,0,0,0,0,A_REBIND+100*i}; it[k++]=(Item){2,"STICK DEAD ZONE",&C.deadzone,5,80,5,0,0}; it[k++]=(Item){0,"RESET PAD",0,0,0,0,0,A_RESETPAD}; it[k++]=(Item){0,"BACK",0,0,0,0,0,A_MAINBACK}; break;
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
    SDL_RenderSetVSync(ren,C.vsync);
}
static void key_name_of(int i, char *o, size_t n){ const char *a=C.kb[i][0]?SDL_GetScancodeName(C.kb[i][0]):"-", *b=C.kb[i][1]?SDL_GetScancodeName(C.kb[i][1]):"-"; snprintf(o,n,"%s  /  %s",a,b); }
static void pad_name_of(int i, char *o, size_t n){ const char *a=SDL_GameControllerGetStringForButton(C.pad[i]); snprintf(o,n,"%s",a?a:"-"); }
static void menu_do(int act){
    int idx=act>=A_REBIND?act/100:0; if(act>=A_REBIND) act=A_REBIND;
    switch(act){
    case A_RESUME: menu_open=0; cfg_save(); break;
    case A_VIDEO: menu_page=P_VIDEO; menu_sel=0; break; case A_AUDIO: menu_page=P_AUDIO; menu_sel=0; break;
    case A_CONTROLS: menu_page=P_CONTROLS; menu_sel=0; break; case A_PAD: menu_page=P_PAD; menu_sel=0; break; case A_ABOUT: menu_page=P_ABOUT; menu_sel=0; break;
    case A_MAINBACK: menu_page=P_MAIN; menu_sel=0; cfg_save(); break;
    case A_QUIT: cfg_save(); pc_quit_now(); break;
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
static void menu_draw(void){
    int dw=dw_, dh=dh_; gs_=dh/180; if(gs_<2) gs_=2; if(gs_>10) gs_=10;
    rect_fill(0,0,dw,dh,0x000000,150);
    static const char *const titles[]={"BUGSTAR PC","VIDEO","AUDIO","KEYBOARD","GAMEPAD","ABOUT"};
    Item it[24]; int n; menu_build(it,&n);
    int lh=9*gs_, pw=text_w("MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM"); if(pw>dw-4*gs_) pw=dw-4*gs_; int ph=(n+3)*lh+2*gs_*4; if(menu_page==P_ABOUT) ph=14*lh;
    int x0=(dw-pw)/2, y0=(dh-ph)/2; rect_fill(x0,y0,pw,ph,0x0B1030,235); rect_fill(x0,y0,pw,gs_,0xFFD23C,255); rect_fill(x0,y0+ph-gs_,pw,gs_,0xFFD23C,255);
    text_at(x0+4*gs_,y0+3*gs_,titles[menu_page],0xFFD23C);
    int y=y0+3*gs_+lh*3/2;
    if(menu_page==P_ABOUT){
        char b[1200]; const char *ln[]={"BUGSTAR FOR PC","THE GAME'S OWN GBA CODE RUNS AS NATIVE PC CODE.","","SAVES AND SETTINGS ARE IN:",g_pref,"","KEYS: F11 FULLSCREEN   F12 SCREENSHOT","F3 FPS   ESC THIS MENU","GAMEPAD: BACK+START OPENS THE MENU",0};
        for(int i=0;ln[i];i++){ snprintf(b,sizeof b,"%s",ln[i]); int maxc=(pw-8*gs_)/(4*gs_); if((int)strlen(b)>maxc){ b[maxc]=0; } text_at(x0+4*gs_,y,b,0xCCD4FF); y+=lh; }
        y+=lh; it[0].label="BACK"; text_at(x0+4*gs_,y,"> BACK",0xFFFFFF); return; }
    for(int i=0;i<n;i++){
        uint32_t col = i==menu_sel ? 0xFFFFFF : 0xA8B4F0; char b[120]="";
        if(i==menu_sel) rect_fill(x0+2*gs_,y-gs_,pw-4*gs_,lh,0x2A3A9A,255);
        text_at(x0+4*gs_,y,it[i].label,col);
        if(it[i].kind==1) snprintf(b,sizeof b,"< %s >",it[i].names[*it[i].val - (it[i].val==&mp_choice?0:0)]);
        else if(it[i].kind==2) snprintf(b,sizeof b,"< %d >",*it[i].val);
        else if(it[i].action>=A_REBIND){ int bi=it[i].action/100; if(menu_wait && menu_sel==i) snprintf(b,sizeof b,"%s",menu_wait==1?"PRESS A KEY (ESC CANCELS)":"PRESS A BUTTON"); else if(menu_page==P_CONTROLS) key_name_of(bi,b,sizeof b); else pad_name_of(bi,b,sizeof b); }
        if(b[0]) text_at(x0+pw-4*gs_-text_w(b),y,b,i==menu_sel?0xFFD23C:0xE8D890);
        y+=lh;
    }
    if(menu_page==P_CONTROLS){ text_at(x0+4*gs_,y0+ph-lh-gs_,"ENTER REBINDS THE FIRST KEY   RIGHT: THE SECOND",0x8890C8); }
    if(menu_page==P_PAD){ text_at(x0+4*gs_,y0+ph-lh-gs_,"ENTER THEN PRESS A BUTTON ON THE PAD",0x8890C8); }
}
static void menu_key(SDL_Keycode k, SDL_Scancode sc){
    if(menu_wait==1){ if(sc==SDL_SCANCODE_ESCAPE){ menu_wait=0; return; } Item it[24]; int n; menu_build(it,&n); int bi=it[menu_sel].action/100; if(bi>=0&&bi<NB){ C.kb[bi][menu_slot]=sc; } menu_wait=0; menu_slot=0; return; }
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
    if(menu_wait==2){ Item it[24]; int n; menu_build(it,&n); int bi=it[menu_sel].action/100; if(bi>=0&&bi<NB && btn!=SDL_CONTROLLER_BUTTON_GUIDE) C.pad[bi]=btn; menu_wait=0; return; }
    switch(btn){ case SDL_CONTROLLER_BUTTON_DPAD_UP: menu_key(SDLK_UP,0); break; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: menu_key(SDLK_DOWN,0); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: menu_key(SDLK_LEFT,0); break; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: menu_key(SDLK_RIGHT,0); break;
        case SDL_CONTROLLER_BUTTON_A: menu_key(SDLK_RETURN,0); break; case SDL_CONTROLLER_BUTTON_B: case SDL_CONTROLLER_BUTTON_START: case SDL_CONTROLLER_BUTTON_BACK: menu_key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE); break; }
}
// ---- one picture
static int shot_pending;
static void present_game(int with_overlay){
    SDL_GetRendererOutputSize(ren,&dw_,&dh_);
    int W,H,linear,smooth; SDL_Rect dst; frame_geometry(&W,&H,&dst,&linear,&smooth);
    tex_ensure(W,H,linear);
    pc_compose_native(pix,W,H,smooth);
    SDL_UpdateTexture(tex,NULL,pix,W*4);
    SDL_SetRenderDrawColor(ren,0,0,0,255); SDL_RenderClear(ren);
    SDL_RenderCopy(ren,tex,NULL,&dst);
    gs_=dh_/180; if(gs_<2) gs_=2; if(gs_>10) gs_=10;
    if(with_overlay){
        if(C.show_fps){ char b[40]; snprintf(b,sizeof b,"%.0f FPS  %dX%d",fps_show,W,H); rect_fill(gs_,gs_,text_w(b)+2*gs_,8*gs_,0,160); text_at(2*gs_,2*gs_,b,0x60FF80); }
        if(toast[0] && SDL_GetTicks64()<toast_until){ int w=text_w(toast)+6*gs_; rect_fill((dw_-w)/2,dh_-12*gs_,w,9*gs_,0,170); text_at((dw_-w)/2+3*gs_,dh_-12*gs_+gs_*3/2,toast,0xFFFFFF); }
        if(menu_open) menu_draw();
        if(g_paused_focus && !menu_open){ rect_fill(0,0,dw_,dh_,0,120); gs_*=2; const char *s="PAUSED"; text_at((dw_-text_w(s))/2,dh_/2-3*gs_,s,0xFFFFFF); }
    }
    if(shot_pending){ shot_pending=0; char p[1200]; time_t t=time(0); struct tm *tm=localtime(&t); char ts[64]; strftime(ts,sizeof ts,"%Y%m%d_%H%M%S",tm);
        snprintf(p,sizeof p,"%sbugstar_%s.png",g_shotdir,ts); if(plat_write_png(p,pix,W,H)){ char m[200]; snprintf(m,sizeof m,"SAVED %s",p); say_toast("SCREENSHOT SAVED"); fprintf(stderr,"%s\n",m); } }
    if(g_autoshot_path && g_autoshot_frame>=0 && pc_frames()>=g_autoshot_frame){
        SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,dw_,dh_,32,SDL_PIXELFORMAT_ARGB8888);
        if(s && SDL_RenderReadPixels(ren,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch)==0){ plat_write_png(g_autoshot_path,s->pixels,dw_,dh_); fprintf(stderr,"window shot %dx%d (draw %dx%d) -> %s\n",dw_,dh_,W,H,g_autoshot_path); }
        if(s) SDL_FreeSurface(s); g_autoshot_frame=-1; g_autoshot_path=0; if(g_stop_after>=0 && 0) pc_quit_now(); }
    SDL_RenderPresent(ren);
}
// ------------------------------------------------------------------------------------------------ input
static void pad_open(void){
    if(pad) return;
    for(int i=0;i<SDL_NumJoysticks();i++) if(SDL_IsGameController(i)){ pad=SDL_GameControllerOpen(i); if(pad){ pad_rumble_ok=SDL_GameControllerHasRumble(pad); fprintf(stderr,"gamepad: %s\n",SDL_GameControllerName(pad)); say_toast("GAMEPAD CONNECTED"); break; } }
}
static uint16_t read_keys(void){
    uint16_t down=0; const Uint8 *ks=SDL_GetKeyboardState(NULL);
    for(int b=0;b<NB;b++){ if((C.kb[b][0]&&ks[C.kb[b][0]])||(C.kb[b][1]&&ks[C.kb[b][1]])) down|=1<<b; }
    if(pad){
        for(int b=0;b<NB;b++) if(SDL_GameControllerGetButton(pad,C.pad[b])) down|=1<<b;
        int dz=C.deadzone*32767/100; int lx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX), ly=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);
        if(lx>dz) down|=1<<4; if(lx<-dz) down|=1<<5; if(ly<-dz) down|=1<<6; if(ly>dz) down|=1<<7;
        if(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>16000) down|=1<<8; if(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>16000) down|=1<<9;
    }
    if((down&(1<<4))&&(down&(1<<5))) down&=~((1<<4)|(1<<5)); if((down&(1<<6))&&(down&(1<<7))) down&=~((1<<6)|(1<<7));
    return down;
}
static void rumble_frame(void){
    static int hist[8], hi; if(!pad||!pad_rumble_ok||!C.rumble){ return; }
    hist[hi++&7]=pc_motor(); int s=0; for(int i=0;i<8;i++) s+=hist[i];
    if(s>0 && !menu_open){ int lo=s*8000, hi2=s*5000; if(lo>65535)lo=65535; SDL_GameControllerRumble(pad,(Uint16)lo,(Uint16)hi2,60); }
}
static void open_menu(void){ menu_open=1; menu_page=P_MAIN; menu_sel=0; menu_wait=0; mp_choice=0; for(int i=0;i<7;i++) if(mp_val(i)==C.max_mp) mp_choice=i; }
static void pump_events(void){
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        switch(e.type){
        case SDL_QUIT: cfg_save(); pc_quit_now(); break;
        case SDL_WINDOWEVENT:
            if(e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED && !C.fullscreen && !(SDL_GetWindowFlags(win)&SDL_WINDOW_MINIMIZED)){ int w,h; SDL_GetWindowSize(win,&w,&h); if(w>=240&&h>=160){ C.win_w=w; C.win_h=h; } }
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST && C.pause_unfocus) g_paused_focus=1;
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) g_paused_focus=0;
            break;
        case SDL_CONTROLLERDEVICEADDED: pad_open(); break;
        case SDL_CONTROLLERDEVICEREMOVED: if(pad && e.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))){ SDL_GameControllerClose(pad); pad=0; say_toast("GAMEPAD REMOVED"); pad_open(); } break;
        case SDL_CONTROLLERBUTTONDOWN:
            if(menu_open) menu_pad(e.cbutton.button);
            else if(e.cbutton.button==SDL_CONTROLLER_BUTTON_GUIDE) open_menu();
            else if((e.cbutton.button==SDL_CONTROLLER_BUTTON_START && SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_BACK)) || (e.cbutton.button==SDL_CONTROLLER_BUTTON_BACK && SDL_GameControllerGetButton(pad,SDL_CONTROLLER_BUTTON_START))) open_menu();
            break;
        case SDL_KEYDOWN:
            if(e.key.repeat && !menu_open) break;
            if(e.key.keysym.sym==SDLK_F11 || (e.key.keysym.sym==SDLK_RETURN && (e.key.keysym.mod&KMOD_ALT))){ toggle_fullscreen(); break; }
            if(e.key.keysym.sym==SDLK_F12){ shot_pending=1; break; }
            if(e.key.keysym.sym==SDLK_F3){ C.show_fps=!C.show_fps; break; }
            if(menu_open){ menu_key(e.key.keysym.sym,e.key.keysym.scancode); break; }
            if(e.key.keysym.sym==SDLK_ESCAPE || e.key.keysym.sym==SDLK_F1){ open_menu(); break; }
            break;
        }
    }
}
// ------------------------------------------------------------------------------------------------ the frame
static void wait_until(uint64_t t){
    for(;;){ uint64_t n=SDL_GetPerformanceCounter(); if(n>=t) return; double ms=(double)(t-n)*1000.0/t_freq; if(ms>2.5) SDL_Delay((Uint32)(ms-2.0)); else if(ms>1.0) SDL_Delay(1); }
}
void plat_frame(void){
    long fr=pc_frames();
    audio_frame();
    uint64_t now=SDL_GetPerformanceCounter();
    int late = now > t_next + t_freq/50;                        // more than a frame behind: skip showing this one, catch up
    if(g_fast ? (fr&15)==0 : (!late || (fr&7)==0)) present_game(1);
    fps_n++; if(now-fps_t0>=t_freq){ fps_show=fps_n*(double)t_freq/(now-fps_t0); fps_n=0; fps_t0=now; }
    // ---- pace: the game steps at exactly 60 a second, whatever the screen's refresh rate is
    t_next += t_freq/60;
    now=SDL_GetPerformanceCounter();
    if(now > t_next + t_freq/8) t_next = now;                  // way behind (the window was dragged, a long load): do not try to catch up
    else if(!g_fast) wait_until(t_next);
    pump_events();
    while(menu_open || g_paused_focus){                        // paused: the game does not step; the window keeps being drawn
        present_game(1); pump_events();
        if(!C.vsync) SDL_Delay(16);
        else if(SDL_GetPerformanceCounter()-now<t_freq/200) SDL_Delay(8);
        if(!menu_open && !g_paused_focus) break;
        audio_frame();
    }
    if(!menu_open && now){ t_next=SDL_GetPerformanceCounter(); }
    uint16_t k;
    if(pc_script_keys(&k)) pc_set_keys(k); else pc_set_keys(read_keys());
    rumble_frame();
    { static long saved_at; if(fr-saved_at>=120){ saved_at=fr; pc_save_flush(0); } }
    if(g_stop_after>=0 && fr>=g_stop_after) pc_quit_now();
}
void plat_close(void){ pc_save_flush(1); cfg_save(); wav_close(); if(adev) SDL_CloseAudioDevice(adev); if(pad) SDL_GameControllerClose(pad); if(ren) SDL_DestroyRenderer(ren); if(win) SDL_DestroyWindow(win); SDL_Quit(); }

// ------------------------------------------------------------------------------------------------ start-up
static void mkdirs(const char *p){ (void)p; }
const char *plat_save_path(void){ return g_savepath; }
int plat_paths(int argc, char **argv){            // finds the folders; call before the fake GBA starts (it loads the save)
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
    { char cmd[1300]; snprintf(cmd,sizeof cmd,"mkdir \"%s\" >nul 2>&1",g_pref); for(char*c=cmd;*c;c++) if(*c=='/') *c='\\'; system(cmd); }
#else
    { char cmd[1300]; snprintf(cmd,sizeof cmd,"mkdir -p '%s' '%sscreenshots' 2>/dev/null",g_pref,g_pref); if(system(cmd)){} }
#endif
    snprintf(g_savepath,sizeof g_savepath,"%sbugstar.sav",g_pref); snprintf(g_cfgpath,sizeof g_cfgpath,"%ssettings.ini",g_pref); snprintf(g_shotdir,sizeof g_shotdir,"%sscreenshots/",g_pref);
#ifdef _WIN32
    { char cmd[1300]; snprintf(cmd,sizeof cmd,"mkdir \"%sscreenshots\" >nul 2>&1",g_pref); for(char*c=cmd;*c;c++) if(*c=='/') *c='\\'; system(cmd); }
#endif
    (void)mkdirs;
    return 0;
}
int plat_open(int argc, char **argv){
    cfg_defaults(); cfg_load();
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
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_GAMECONTROLLER|SDL_INIT_TIMER)!=0){
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_TIMER)!=0){ fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; } g_no_audio=1; }
    win=SDL_CreateWindow("BUGSTAR",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,C.win_w,C.win_h,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|(C.fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0));
    if(!win){ fprintf(stderr,"window: %s\n",SDL_GetError()); return 1; }
    SDL_SetWindowMinimumSize(win,240,160);
    ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|(C.vsync?SDL_RENDERER_PRESENTVSYNC:0));
    if(!ren){ fprintf(stderr,"no accelerated renderer (%s): using the software one\n",SDL_GetError()); ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE); }
    if(!ren){ fprintf(stderr,"renderer: %s\n",SDL_GetError()); return 1; }
    { SDL_RendererInfo ri; if(SDL_GetRendererInfo(ren,&ri)==0) fprintf(stderr,"video: %s renderer, driver %s\n",ri.name,SDL_GetCurrentVideoDriver()); }
    make_icon(); font_init();
    SDL_ShowCursor(SDL_DISABLE);
    pad_open();
    t_freq=SDL_GetPerformanceFrequency(); t_next=SDL_GetPerformanceCounter(); fps_t0=t_next;
    if(g_wav_path){ g_wav=fopen(g_wav_path,"wb"); if(g_wav){ uint8_t h[44]={0}; fwrite(h,1,44,g_wav); } }
    audio_open();
    atexit(wav_close);
    return 0;
}
