// gba_test.c - step 1 of pc/ANDROID_PLAN.md: the go / no-go test (patch 21), now with the address search (patch 24).
// Patch 21 showed that Android's Java heap sits on 02000000-12000000, so the fake GBA cannot live at its real addresses.
// The game runs with every GBA address moved up by a fixed number (pc/platform/pc_gbaaddr.h, build.sh PC_GBA_TOP=...).
// So the question is now: which TOP values (each GBA range at TOP+02000000 ... TOP+0E010000) can this phone map?
// It tries TOP = 30000000, 40000000 ... 70000000 first (clear of the Java heap), then 20000000, 10000000 and 0 (the real addresses), at the earliest moment we control (a constructor: it runs
// when the Java side loads libmain.so). The first TOP that maps all 8 ranges is kept and reported; the later ones are listed as not tried.
// The text is shown on screen, copied to the clipboard and written to the log. Not the game: no game code in here.
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

// TOP values to try (every address stays below 80000000: the game keeps addresses in 32-bit numbers)
// order matters: offsets well clear of the Java heap (02000000-12000000) first, the risky ones (touching or inside it) last
static const uintptr_t TOPS[] = { 0x30000000, 0x40000000, 0x50000000, 0x60000000, 0x70000000, 0x20000000, 0x10000000, 0x00000000 };
#define NT 8
static int top_ok[NT], top_bad[NT], top_err[NT];     // per TOP: all mapped / index of the first range that failed / its errno (-1 = kernel gave another address)
static int chosen = -1;                              // index into TOPS of the TOP that was kept
static int res_ok[NR], res_err[NR];
static void *res_got[NR];
static int ran_early;

static int try_top(int t)
{
    int mapped = 0;
    top_ok[t] = 0; top_bad[t] = -1; top_err[t] = 0;
    for (int i = 0; i < NR; i++) {
        res_ok[i] = 0; res_err[i] = 0; res_got[i] = NULL;
        uintptr_t a = R[i].a + TOPS[t];
        void *p = mmap((void *)a, R[i].n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (p == MAP_FAILED) res_err[i] = errno;
        else if ((uintptr_t)p != a) { res_err[i] = -1; res_got[i] = p; munmap(p, R[i].n); }       // kernel put it somewhere else
        else {
            *(volatile uint8_t *)p = 0xA5;
            if (*(volatile uint8_t *)p != 0xA5) { res_err[i] = -2; munmap(p, R[i].n); }
            else { res_ok[i] = 1; mapped++; continue; }
        }
        top_bad[t] = i; top_err[t] = res_err[i];
        break;
    }
    if (mapped == NR) { top_ok[t] = 1; return 1; }
    for (int i = 0; i < NR; i++) if (res_ok[i]) munmap((void *)(R[i].a + TOPS[t]), R[i].n);     // give back what was mapped: this TOP is not usable
    return 0;
}

__attribute__((constructor)) static void map_early(void)
{
    for (int t = 0; t < NT; t++) top_bad[t] = -1;
    for (int t = 0; t < NT; t++) if (try_top(t)) { chosen = t; break; }     // the first TOP that works is kept mapped
    ran_early = 1;
}

static int all_ok(void) { return ran_early && chosen >= 0; }

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
    add(b, n, "GBA address search, run when libmain.so loaded (TOP = offset added to every GBA address):\n");
    for (int t = 0; t < NT; t++) {
        unsigned long T = (unsigned long)TOPS[t];
        if (top_ok[t]) { add(b, n, "  TOP %08lx: OK (all 8 ranges mapped)\n", T); continue; }
        int i = top_bad[t];
        if (i < 0) { add(b, n, "  TOP %08lx: not tried\n", T); continue; }
        unsigned long A = (unsigned long)(R[i].a + TOPS[t]);
        if (top_err[t] == -1) add(b, n, "  TOP %08lx: FAIL at %08lx %s (kernel gave another address)\n", T, A, R[i].name);
        else if (top_err[t] == -2) add(b, n, "  TOP %08lx: FAIL at %08lx %s (mapped but not usable)\n", T, A, R[i].name);
        else add(b, n, "  TOP %08lx: FAIL at %08lx %s, errno %d (%s)\n", T, A, R[i].name, top_err[t], strerror(top_err[t]));
    }
    if (chosen >= 0) add(b, n, "RESULT: PASS. Build the game with PC_GBA_TOP=0x%08lx\n", (unsigned long)TOPS[chosen]);
    else add(b, n, "RESULT: FAIL (none of the 8 TOP values works)\n");
    add(b, n, "where things live (step 2 needs these above 4 GB to be handled): stack %p, static %p, malloc %p, code %p\n",
        (void *)&local, (void *)&sdata, h, (void *)report);
    add(b, n, "page size %ld, pointer size %zu\n", sysconf(_SC_PAGESIZE), sizeof(void *));
#if defined(__ANDROID__) && !defined(HOST_TEST)
    add(b, n, "android sdk %d\n", SDL_GetAndroidSDKVersion());
#endif
    // who else is in the low 2 GB (from /proc/self/maps)? (our own ranges are the anonymous rw-p lines)
    add(b, n, "mappings below 80000000:\n");
    FILE *f = fopen("/proc/self/maps", "r");
    if (f) {
        char line[300]; int shown = 0;
        while (fgets(line, sizeof line, f) && shown < 40) {
            unsigned long s = 0, e = 0;
            if (sscanf(line, "%lx-%lx", &s, &e) == 2 && s < 0x80000000UL) {
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
int main(void) { char t[12000]; report(t, sizeof t); fputs(t, stdout); return all_ok() ? 0 : 1; }
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
    static char txt[12000];
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
