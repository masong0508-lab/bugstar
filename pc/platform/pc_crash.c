// pc_crash.c (patch 34): Android only. When the game dies from a signal (SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE) this writes what happened (signal, address,
// where the code was, a backtrace with function names) to logcat and to <app files>/native_crash.txt. BugstarActivity (Java) finds that file on the next start
// and copies it to Downloads/bugstar_log.txt, so you can read it without adb or the data folder. On every other platform this file is empty.
#ifdef __ANDROID__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unwind.h>
#include <ucontext.h>
#include <android/log.h>

#define CR_PATH "/data/data/org.bugstar.app/files/native_crash.txt"
static int cr_fd=-1, cr_busy=0; static char cr_alt[65536] __attribute__((aligned(16)));
static struct sigaction cr_old[32];

static void cr_out(const char *s){ __android_log_write(ANDROID_LOG_FATAL,"BUGSTAR_CRASH",s); if(cr_fd>=0){ size_t n=strlen(s); if(write(cr_fd,s,n)<0){} if(write(cr_fd,"\n",1)<0){} } }
typedef struct { int n; } CrBt;
static _Unwind_Reason_Code cr_step(struct _Unwind_Context *ctx, void *arg){
    CrBt *b=(CrBt*)arg; uintptr_t pc=_Unwind_GetIP(ctx); if(!pc) return _URC_END_OF_STACK;
    Dl_info di; char line[300];
    if(dladdr((void*)pc,&di) && di.dli_fname){ const char *f=strrchr(di.dli_fname,'/'); f=f?f+1:di.dli_fname;
        snprintf(line,sizeof line,"#%02d pc %016llx  %s (%s+%lld)",b->n,(unsigned long long)(pc-(uintptr_t)di.dli_fbase),f,di.dli_sname?di.dli_sname:"?",di.dli_saddr?(long long)(pc-(uintptr_t)di.dli_saddr):0LL); }
    else snprintf(line,sizeof line,"#%02d pc %016llx  ?",b->n,(unsigned long long)pc);
    cr_out(line); return ++b->n>=40 ? _URC_END_OF_STACK : _URC_NO_REASON; }
static void cr_handler(int sig, siginfo_t *si, void *uc_){
    if(cr_busy){ _exit(128+sig); } cr_busy=1;
    cr_fd=open(CR_PATH,O_WRONLY|O_CREAT|O_TRUNC,0600);
    char line[300]; snprintf(line,sizeof line,"NATIVE CRASH: signal %d (%s), code %d, fault address %p",sig,strsignal(sig),si?si->si_code:0,si?si->si_addr:0); cr_out(line);
#if defined(__aarch64__)
    { ucontext_t *uc=(ucontext_t*)uc_; snprintf(line,sizeof line,"pc %016llx  lr %016llx  sp %016llx",(unsigned long long)uc->uc_mcontext.pc,(unsigned long long)uc->uc_mcontext.regs[30],(unsigned long long)uc->uc_mcontext.sp); cr_out(line);
      Dl_info di; if(dladdr((void*)uc->uc_mcontext.pc,&di) && di.dli_fname){ snprintf(line,sizeof line,"crashed in %s (%s+%lld)",di.dli_fname,di.dli_sname?di.dli_sname:"?",di.dli_saddr?(long long)(uc->uc_mcontext.pc-(uintptr_t)di.dli_saddr):0LL); cr_out(line); } }
#else
    (void)uc_;
#endif
    CrBt b={0}; _Unwind_Backtrace(cr_step,&b);
    if(cr_fd>=0) close(cr_fd);
    sigaction(sig,&cr_old[sig],0); raise(sig); }                                      // then Android's own crash dialog / tombstone as usual
// patch 35: an alternate signal stack belongs to ONE thread. The constructor below sets one for the thread that loads the library; the game runs on its own
// thread (pc_main.c), so that thread calls this too, or a stack overflow there would crash again inside the handler and leave no report.
void pc_crash_thread_init(void){
    stack_t ss; ss.ss_size=65536; ss.ss_sp=malloc(ss.ss_size); ss.ss_flags=0;
    if(ss.ss_sp && sigaltstack(&ss,0)!=0) free(ss.ss_sp); }
__attribute__((constructor)) static void pc_crash_install(void){
    stack_t ss; ss.ss_sp=cr_alt; ss.ss_size=sizeof cr_alt; ss.ss_flags=0; sigaltstack(&ss,0);
    struct sigaction sa; memset(&sa,0,sizeof sa); sa.sa_sigaction=cr_handler; sa.sa_flags=SA_SIGINFO|SA_ONSTACK; sigemptyset(&sa.sa_mask);
    static const int sigs[]={SIGSEGV,SIGABRT,SIGBUS,SIGILL,SIGFPE};
    for(unsigned i=0;i<sizeof sigs/sizeof sigs[0];i++) sigaction(sigs[i],&sa,&cr_old[sigs[i]]);
    __android_log_write(ANDROID_LOG_INFO,"BUGSTAR_CRASH","crash reporter installed"); }
#else
typedef int pc_crash_unused;
#endif
