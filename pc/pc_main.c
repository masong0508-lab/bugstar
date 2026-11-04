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
#if defined(PC_PIE) || defined(__ANDROID__)
#include <pthread.h>          // patch 25: no makecontext on Android's libc; the game runs on a pthread with a big stack instead
#define PC_PTHREAD_STACK 1
#else
#include <ucontext.h>
#endif
#endif
#include "pc_hw.h"
#include "platform/pc_plat.h"
#ifdef _WIN32
// Windows: the exe is a window program (no console), so every error used to vanish. Now: a log file next to the exe and an error box.
#include <windows.h>
extern unsigned pc_map_fail_addr; extern unsigned long pc_map_fail_err;
static char g_logpath[MAX_PATH];
static void win_fatal(const char *what){
    char tail[800]; tail[0]=0; fflush(stderr);
    { FILE *f=fopen(g_logpath,"rb"); if(f){ fseek(f,0,SEEK_END); long sz=ftell(f); long st=sz>799?sz-799:0; fseek(f,st,SEEK_SET); size_t r=fread(tail,1,799,f); tail[r]=0; fclose(f); } }
    char msg[1800]; snprintf(msg,sizeof msg,"%s\n\nLog file: %s\n\n%s",what,g_logpath,tail);
    MessageBoxA(0,msg,"BUGSTAR",MB_OK|MB_ICONERROR);
}
static LONG WINAPI win_crash(EXCEPTION_POINTERS *e){
    fprintf(stderr,"CRASH code=%08lx at %p (touched %p)\n",(unsigned long)e->ExceptionRecord->ExceptionCode,e->ExceptionRecord->ExceptionAddress,e->ExceptionRecord->NumberParameters>1?(void*)e->ExceptionRecord->ExceptionInformation[1]:(void*)0);
    win_fatal("BUGSTAR crashed.");
    return EXCEPTION_EXECUTE_HANDLER;
}

// The fake GBA needs fixed low addresses (02000000 ... 0E010000). Windows loads DLLs (random order and place, below 2 GB for this exe) BEFORE main() runs,
// and one of them often lands on 02000000 (windows error 487). So the first copy of the program is only a launcher: it starts a second copy of itself
// SUSPENDED (only ntdll and the exe are loaded then), reserves the GBA ranges in it, and resumes it. The loader then places every DLL around them.
static int win_launch(int *rc){
    static const struct { unsigned a, n; } R[]={{GBAA(0x02000000),0x40000},{GBAA(0x03000000),0x10000},{GBAA(0x04000000),0x10000},{GBAA(0x05000000),0x10000},{GBAA(0x06000000),0x20000},{GBAA(0x07000000),0x10000},{GBAA(0x08000000),0x10000},{GBAA(0x0E000000),0x10000}};
    char tmp[8];
    if(GetEnvironmentVariableA("PC_CHILD",tmp,sizeof tmp)) return 0;          // we are the child: run the game
    SetEnvironmentVariableA("PC_CHILD","1");
    STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si,0,sizeof si); si.cb=sizeof si; memset(&pi,0,sizeof pi);
    si.dwFlags=STARTF_USESTDHANDLES; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
    if(!CreateProcessA(0,GetCommandLineA(),0,0,TRUE,CREATE_SUSPENDED,0,0,&si,&pi)) return 0;   // cannot launch: just run here, as before
    int ok=1;
    for(unsigned i=0;i<sizeof R/sizeof R[0];i++) if(!VirtualAllocEx(pi.hProcess,(void*)(uintptr_t)R[i].a,R[i].n,MEM_RESERVE,PAGE_READWRITE)) ok=0;
    if(!ok){ TerminateProcess(pi.hProcess,1); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); SetEnvironmentVariableA("PC_CHILD",0); return 0; }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess,INFINITE);
    DWORD code=0; GetExitCodeProcess(pi.hProcess,&code); *rc=(int)code;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}
static void win_setup(void){
    char exe[MAX_PATH]; const char *mode=getenv("PC_RETRY")?"a":"w";
    GetModuleFileNameA(0,exe,MAX_PATH); { char *s=strrchr(exe,'\\'); if(s) *s=0; }
    snprintf(g_logpath,sizeof g_logpath,"%s\\bugstar_log.txt",exe);
    if(!freopen(g_logpath,mode,stderr)){ char t[MAX_PATH]; GetTempPathA(MAX_PATH,t); snprintf(g_logpath,sizeof g_logpath,"%sbugstar_log.txt",t); freopen(g_logpath,mode,stderr); }
    setvbuf(stderr,0,_IONBF,0);
    fprintf(stderr,"BUGSTAR PC log (retry=%s, launcher=%s)\n",getenv("PC_RETRY")?getenv("PC_RETRY"):"0",getenv("PC_CHILD")?"yes":"no");
    SetUnhandledExceptionFilter(win_crash);
}
#else
#define win_fatal(x) ((void)0)
#endif
#ifdef PC_SDL
int plat_paths(int argc, char **argv); const char *plat_save_path(void);
#endif
void pc_set_window_mode(int on);

int bugstar_main(void);
int pc_view_main(int argc, char **argv);

static int g_argc; static char **g_argv; static int g_view;
#if !defined(_WIN32) && !defined(PC_PTHREAD_STACK)
static ucontext_t uc_main, uc_game;
#endif
static void game_entry(void){ if(g_view) pc_view_main(g_argc,g_argv); else bugstar_main(); }
#ifdef PC_PTHREAD_STACK
static void *game_thread(void *unused){ (void)unused; game_entry(); return 0; }
#endif

int main(int argc, char **argv){
    const char *script=0, *out="pc_out", *sav=0; long frames=-1; int want_window=-1;
#ifdef _WIN32
    { int lrc=0; if(win_launch(&lrc)) return lrc; }
    win_setup();
#endif
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
#ifdef _WIN32
        // the system put something (a stack, the heap, a library) on an address the fake GBA needs: start the program again (the layout is random each time)
        { const char *r=getenv("PC_RETRY"); int n=r?atoi(r):0; fprintf(stderr,"cannot map the GBA memory at %08x (windows error %lu), attempt %d\n",pc_map_fail_addr,pc_map_fail_err,n);
          if(n<12){ char b[16]; snprintf(b,16,"%d",n+1); SetEnvironmentVariableA("PC_RETRY",b); SetEnvironmentVariableA("PC_CHILD",0);
            STARTUPINFOA si; PROCESS_INFORMATION pi; memset(&si,0,sizeof si); si.cb=sizeof si;
            if(CreateProcessA(0,GetCommandLineA(),0,0,FALSE,0,0,0,&si,&pi)){ CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return 0; } }
          win_fatal("BUGSTAR could not set up its memory."); }
#endif
        return 2; }
    pc_script(script,out,frames);
#ifdef PC_SDL
    if(want_window){ pc_set_window_mode(1); if(plat_open(argc,argv)){ win_fatal("BUGSTAR could not open its window."); return 2; } }
#endif
    g_argc=argc; g_argv=argv;
    // The game stores some addresses in 32-bit numbers (for DMA). Run it on a stack below 4 GB so those still point at the right place.
#ifdef _WIN32
    game_entry();      // 32-bit process: every stack address is already below 4 GB
#elif defined(PC_PTHREAD_STACK)
    // patch 25: the DMA code keeps full-width pointers now (pc_dma_sad / pc_dma_dad), so the stack can be anywhere. A big stack on its own thread.
    { pthread_attr_t at; pthread_t th; pthread_attr_init(&at); pthread_attr_setstacksize(&at,16u<<20);
      if(pthread_create(&th,&at,game_thread,0)){ perror("game thread"); return 2; }
      pthread_join(th,0); }
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
