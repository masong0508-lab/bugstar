// pc_main.c: the PC program. Runs the GBA game's own code (generated into pc/gen by build.sh) on the fake GBA in pc_hw.c.
//   bugstar_pc [--script file] [--out prefix] [--sav file] [--frames N]          play the game headless from a key script
//   bugstar_pc --view [--universe HEX] [--cx HEX] [--cy HEX] [--star N] [--planet N] [--moon N] [--out prefix] [--scale N]
//                                                                               the seed viewer (pc_view.inc)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <ucontext.h>
#include "pc_hw.h"

int bugstar_main(void);
int pc_view_main(int argc, char **argv);

static int g_argc; static char **g_argv; static int g_view;
static ucontext_t uc_main, uc_game;
static void game_entry(void){ if(g_view) pc_view_main(g_argc,g_argv); else bugstar_main(); }

int main(int argc, char **argv){
    const char *script=0, *out="pc_out", *sav=0; long frames=-1;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--script")&&i+1<argc) script=argv[++i];
        else if(!strcmp(argv[i],"--out")&&i+1<argc) out=argv[++i];
        else if(!strcmp(argv[i],"--sav")&&i+1<argc) sav=argv[++i];
        else if(!strcmp(argv[i],"--frames")&&i+1<argc) frames=atol(argv[++i]);
        else if(!strcmp(argv[i],"--view")) g_view=1;
    }
    if(!pc_hw_init(sav)) return 2;
    pc_script(script,out,frames);
    g_argc=argc; g_argv=argv;
    // The game stores some addresses in 32-bit numbers (for DMA). Run it on a stack below 4 GB so those still point at the right place.
    size_t ss=16u<<20; void *stk=mmap(0,ss,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_32BIT,-1,0);
    if(stk==MAP_FAILED){ perror("stack"); return 2; }
    getcontext(&uc_game); uc_game.uc_stack.ss_sp=stk; uc_game.uc_stack.ss_size=ss; uc_game.uc_link=&uc_main;
    makecontext(&uc_game,game_entry,0);
    swapcontext(&uc_main,&uc_game);
    pc_hw_finish();
    return 0;
}
