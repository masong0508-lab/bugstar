// pc_main.c: the PC program. Runs the GBA game's own code (generated into pc/gen by build.sh) on the fake GBA in pc_hw.c.
//   bugstar_pc [--script file] [--out prefix] [--sav file] [--frames N]          play the game headless from a key script
//   bugstar_pc --view [--universe HEX] [--cx HEX] [--cy HEX] [--star N] [--planet N] [--moon N] [--out prefix] [--scale N]
//                                                                               the seed viewer (pc_view.inc)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/mman.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <ucontext.h>
#endif
#include "pc_hw.h"
#include "platform/pc_plat.h"
#ifdef PC_SDL
int plat_paths(int argc, char **argv); const char *plat_save_path(void);
#endif
void pc_set_window_mode(int on);

int bugstar_main(void);
int pc_view_main(int argc, char **argv);

static int g_argc; static char **g_argv; static int g_view;
#ifndef _WIN32
static ucontext_t uc_main, uc_game;
#endif
static void game_entry(void){ if(g_view) pc_view_main(g_argc,g_argv); else bugstar_main(); }

int main(int argc, char **argv){
    const char *script=0, *out="pc_out", *sav=0; long frames=-1; int want_window=-1;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--script")&&i+1<argc) script=argv[++i];
        else if(!strcmp(argv[i],"--out")&&i+1<argc) out=argv[++i];
        else if(!strcmp(argv[i],"--sav")&&i+1<argc) sav=argv[++i];
        else if(!strcmp(argv[i],"--frames")&&i+1<argc) frames=atol(argv[++i]);
        else if(!strcmp(argv[i],"--view")) g_view=1;
        else if(!strcmp(argv[i],"--window")) want_window=1;
        else if(!strcmp(argv[i],"--headless")) want_window=0;
        else if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){ puts("BUGSTAR PC\n  (no options)            play in a window\n  --fullscreen | --windowed | --size W H | --scale-mode 0|1 | --no-vsync | --no-audio\n  --data DIR | --portable  keep saves and settings in DIR (or next to the program)\n  --sav FILE              use this save file\n  --headless --script S --out PREFIX --frames N   run a key script and write pictures (tests)\n  --view ...              the seed viewer (see pc/README.md)"); return 0; }
    }
    if(want_window<0) want_window = !(script || g_view);          // keys from a script, or the seed viewer: headless. Otherwise: play.
#ifndef PC_SDL
    if(want_window){ fprintf(stderr,"this build has no window (built without SDL). Use --script / --view, or rebuild with SDL (pc/build.sh).\n"); return 2; }
#else
    if(want_window){ plat_paths(argc,argv); if(!sav) sav=plat_save_path(); }
#endif
    if(!pc_hw_init(sav)){
#ifndef _WIN32
        // rare: the system put a library or the stack on a low address the fake GBA needs; start over (the layout is random each time)
        { const char *r=getenv("PC_RETRY"); int n=r?atoi(r):0; if(n<8){ char b[8]; snprintf(b,8,"%d",n+1); setenv("PC_RETRY",b,1); execv("/proc/self/exe",argv); } }
#endif
        return 2; }
    pc_script(script,out,frames);
#ifdef PC_SDL
    if(want_window){ pc_set_window_mode(1); if(plat_open(argc,argv)) return 2; }
#endif
    g_argc=argc; g_argv=argv;
    // The game stores some addresses in 32-bit numbers (for DMA). Run it on a stack below 4 GB so those still point at the right place.
#ifdef _WIN32
    game_entry();      // 32-bit process: every stack address is already below 4 GB
#else
    size_t ss=16u<<20; void *stk=mmap(0,ss,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(stk==MAP_FAILED){ perror("stack"); return 2; }
    getcontext(&uc_game); uc_game.uc_stack.ss_sp=stk; uc_game.uc_stack.ss_size=ss; uc_game.uc_link=&uc_main;
    makecontext(&uc_game,game_entry,0);
    swapcontext(&uc_main,&uc_game);
#endif
#ifdef PC_SDL
    if(want_window) plat_close();
#endif
    pc_hw_finish();
    return 0;
}
