// gba_test.c - patch 21, step 1 of pc/ANDROID_PLAN.md: the go / no-go test.
// Question: can an Android app process map the fake GBA's fixed addresses (02000000 ... 0E010000, as pc_hw.c does)?
// It tries at the earliest moment we control (a constructor: it runs when the Java side loads libmain.so), keeps the ranges, then
// shows PASS (green) or FAIL (red) plus a text with the details. The text is also copied to the clipboard and written to the log.
// Not the game: no game code in here.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#ifndef HOST_TEST
#include <SDL.h>
#endif

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000     // Linux 4.17+; older kernels ignore it, the address check below still catches a miss
#endif

typedef struct { uintptr_t a; size_t n; const char *name; } Rng;
// the same list as win_launch() in pc_main.c
static const Rng R[] = {
    {0x02000000, 0x40000, "EWRAM"}, {0x03000000, 0x10000, "IWRAM"}, {0x04000000, 0x10000, "IO"},   {0x05000000, 0x10000, "PAL"},
    {0x06000000, 0x20000, "VRAM"},  {0x07000000, 0x10000, "OAM"},   {0x08000000, 0x10000, "ROM"},  {0x0E000000, 0x10000, "SRAM"},
};
#define NR 8

static int res_ok[NR], res_err[NR];
static void *res_got[NR];
static int ran_early;

__attribute__((constructor)) static void map_early(void)
{
    for (int i = 0; i < NR; i++) {
        void *p = mmap((void *)R[i].a, R[i].n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == MAP_FAILED) { res_err[i] = errno; continue; }
        if ((uintptr_t)p != R[i].a) { res_err[i] = -1; res_got[i] = p; munmap(p, R[i].n); continue; }   // kernel put it somewhere else
        *(volatile uint8_t *)p = 0xA5;
        if (*(volatile uint8_t *)p != 0xA5) { res_err[i] = -2; continue; }
        res_ok[i] = 1;
    }
    ran_early = 1;
}

static int all_ok(void) { for (int i = 0; i < NR; i++) if (!res_ok[i]) return 0; return ran_early; }

static void add(char *b, size_t n, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void add(char *b, size_t n, const char *fmt, ...)
{
    size_t l = strlen(b); va_list ap; va_start(ap, fmt); vsnprintf(b + l, n - l, fmt, ap); va_end(ap);
}

static void report(char *b, size_t n)
{
    static int sdata;
    int local = 0; void *h = malloc(16);
    b[0] = 0;
    add(b, n, "GBA address ranges, mapped when libmain.so loaded:\n");
    for (int i = 0; i < NR; i++) {
        if (res_ok[i]) add(b, n, "  %08lx %-5s OK\n", (unsigned long)R[i].a, R[i].name);
        else if (res_err[i] == -1) add(b, n, "  %08lx %-5s FAIL: got %p instead\n", (unsigned long)R[i].a, R[i].name, res_got[i]);
        else if (res_err[i] == -2) add(b, n, "  %08lx %-5s FAIL: mapped but not usable\n", (unsigned long)R[i].a, R[i].name);
        else add(b, n, "  %08lx %-5s FAIL: errno %d (%s)\n", (unsigned long)R[i].a, R[i].name, res_err[i], strerror(res_err[i]));
    }
    add(b, n, "RESULT: %s\n", all_ok() ? "PASS" : "FAIL");
    add(b, n, "where things live (step 2 needs these above 4 GB to be handled): stack %p, static %p, malloc %p, code %p\n",
        (void *)&local, (void *)&sdata, h, (void *)report);
    add(b, n, "page size %ld, pointer size %zu\n", sysconf(_SC_PAGESIZE), sizeof(void *));
#if defined(__ANDROID__) && !defined(HOST_TEST)
    add(b, n, "android sdk %d\n", SDL_GetAndroidSDKVersion());
#endif
    // who else is in the low 512 MB (from /proc/self/maps)?
    add(b, n, "mappings below 20000000:\n");
    FILE *f = fopen("/proc/self/maps", "r");
    if (f) {
        char line[300]; int shown = 0;
        while (fgets(line, sizeof line, f) && shown < 28) {
            unsigned long s = 0, e = 0;
            if (sscanf(line, "%lx-%lx", &s, &e) == 2 && s < 0x20000000UL) {
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 96) line[96] = 0;
                add(b, n, "  %s\n", line); shown++;
            }
        }
        if (!shown) add(b, n, "  (none)\n");
        fclose(f);
    } else add(b, n, "  cannot read /proc/self/maps\n");
    free(h);
}

#ifdef HOST_TEST
int main(void) { char t[8000]; report(t, sizeof t); fputs(t, stdout); return all_ok() ? 0 : 1; }
#else
static void draw(SDL_Renderer *r, int pass)
{
    int W = 0, H = 0; SDL_GetRendererOutputSize(r, &W, &H);
    SDL_SetRenderDrawColor(r, pass ? 0 : 150, pass ? 110 : 0, 0, 255); SDL_RenderClear(r);
    for (int i = 0; i < NR; i++) {            // one stripe per range: bright green = mapped, bright red = not
        SDL_Rect q = { i * W / NR + 4, H / 4, W / NR - 8, H / 2 };
        if (res_ok[i]) SDL_SetRenderDrawColor(r, 60, 255, 60, 255); else SDL_SetRenderDrawColor(r, 255, 60, 60, 255);
        SDL_RenderFillRect(r, &q);
    }
    SDL_RenderPresent(r);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    static char txt[8000];
    report(txt, sizeof txt);
    SDL_Log("BUGSTAR address test:\n%s", txt);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_Window *w = SDL_CreateWindow("BUGSTAR address test", 0, 0, 0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
    SDL_Renderer *r = w ? SDL_CreateRenderer(w, -1, 0) : NULL;
    if (!r) { SDL_Log("no window/renderer: %s", SDL_GetError()); return 1; }
    int pass = all_ok();
    SDL_SetClipboardText(txt);                       // so it can be pasted into a chat
    draw(r, pass);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, pass ? "BUGSTAR address test: PASS" : "BUGSTAR address test: FAIL", txt, w);
    for (;;) {                                       // tap the screen to see the text again; Back quits
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, 250)) {
            if (e.type == SDL_QUIT) break;
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_AC_BACK) break;
            if (e.type == SDL_FINGERDOWN) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "BUGSTAR address test", txt, w);
        }
        draw(r, pass);
    }
    SDL_DestroyRenderer(r); SDL_DestroyWindow(w); SDL_Quit();
    return 0;
}
#endif
