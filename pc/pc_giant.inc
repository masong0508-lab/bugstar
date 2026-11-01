// pc_giant.inc: the PC-only GIANT NUMBERS, inside the PC LAB (pause, WORLD page, B, GIANT NUMBERS). #included at the end of the generated main.c (like pc_laws.inc).
// The numbers and the names live in pc/giant (plain C, tested against a Python reference); this file only draws them.
//   * 2 TO THE POWER 2^32 (1,292,913,987 digits) and THE COUNTING NUMBER (1,000,000,008 digits): a viewer that scrolls by line and page (hold = faster and faster),
//     jumps to any digit, shows the digit-group commas, a counter, and writes the whole number to a text file (streamed, with progress, B cancels).
//   * BUGSTARILLION V3: the complete written-out name (about 6 x 10^18 letters) as a stream of letters, with the same viewer.
//   * NAME ANY NUMBER: type up to 128 digits (or a power of ten) and read its -illion name, group by group.
//   * SCALE LADDER: from a thousand to the V3 name.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "giant/giant.h"
#include <time.h>
typedef unsigned long long gull;
enum { GV_POW = 0, GV_COUNT = 1, GV_V3 = 2 };
static void gv_comma(char *b, gull v){ char t[32]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while(v); int k = textlen(b); for(int i = n - 1; i >= 0; i--){ b[k++] = t[i]; if(i && i % 3 == 0) b[k++] = ','; } b[k] = 0; }
static gull gv_total(int w){ return w == GV_V3 ? gv3_chars() : gi_digits(w); }
static int gv_fetch(int w, gull pos, int n, char *o){ return w == GV_V3 ? gv3_read(pos, n, o) : gi_read(w, pos, n, o); }
static double gv_now(void){ struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static const char *gv_title(int w){ return w == GV_POW ? "2 TO THE POWER 2^32" : w == GV_COUNT ? "THE COUNTING NUMBER" : "BUGSTARILLION V3: THE NAME"; }

// ---- is the number there? (2^(2^32) and V3 need the file from pc/giant/mkpow.py)
static int gv_ready(int w){ if(w == GV_COUNT) return 1; if(w == GV_POW) return gi_open(GI_POW) == 0; return gv3_ready() || gv3_open() == 0; }
static int gv_building;                                         // the file is being made by a program in the background
static void gv_start_build(void){
    char exe[600], cmd[1500]; int n = (int)readlink("/proc/self/exe", exe, sizeof exe - 1); if(n <= 0) return; exe[n] = 0; char *sl = strrchr(exe, '/'); if(!sl) return; *sl = 0;
    const char *d = gi_dir(); char dd[600]; if(!d[0]){ snprintf(dd, sizeof dd, "%s/giant/cache", exe); d = dd; }
    snprintf(cmd, sizeof cmd, "mkdir -p '%s' && (nohup python3 '%s/giant/mkpow.py' '%s/%s.part' > '%s/build.log' 2>&1 && mv '%s/%s.part' '%s/%s') > /dev/null 2>&1 &", d, exe, d, gi_file(GI_POW), d, d, gi_file(GI_POW), d, gi_file(GI_POW));
    if(system(cmd) == 0){ gv_building = 1; gi_set_dir(d); }
}
static void gv_missing_screen(volatile u16 *pg, int w){
    int dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143); ui_head(pg, 9, 8, 222, gv_title(w), C_GOLD);
            const char *L[] = { "THE DIGITS OF THIS NUMBER ARE NOT BUILT YET.", "", "THEY ARE WORKED OUT ONCE, EXACTLY, BY A PYTHON PROGRAM", "(PC/GIANT/MKPOW.PY) AND KEPT IN ONE FILE OF 646 MB:", gi_file(GI_POW), "",
                "IT NEEDS ABOUT 5 GB OF MEMORY AND 15 TO 30 MINUTES.", "THE GAME THEN JUMPS INTO THE FILE: IT NEVER HOLDS THE", "WHOLE NUMBER.", "" , 0 };
            for(int i = 0; L[i]; i++) text(pg, 9, 20 + i * 7, L[i], i == 4 ? C_CYAN : C_WHITE, 1);
            if(gv_building) text(pg, 9, 96, "BUILDING NOW IN THE BACKGROUND. COME BACK LATER.", C_GREEN, 1);
            else text(pg, 9, 96, "A: BUILD IT NOW (STARTS PYTHON IN THE BACKGROUND)", C_GOLD, 1);
            { char b[640]; scpy(b, "FOLDER: "); scat(b, gi_dir()[0] ? gi_dir() : "(NONE YET)"); b[56] = 0; text(pg, 9, 110, b, C_GREY, 1); }
            text(pg, 9, 118, "OR SET BUGSTAR_GIANT_DIR TO A FOLDER WITH THE FILE.", C_GREY, 1);
            ui_foot(pg, "A: BUILD IT NOW", "B: BACK"); }
        vsync(); read_keys();
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_A) && !gv_building){ gv_start_build(); dirty = 1; sfx(2); }
    }
    wait_release();
}

// ---- typing a number: UP/DOWN change the digit, LEFT/RIGHT move, A done, B cancel, SELECT = biggest, START = smallest. returns 1 when done
static int gv_enter(volatile u16 *pg, const char *label, gull *val, gull lo, gull hi){
    int nd = 1; { gull t = hi; while(t >= 10){ t /= 10; nd++; } } int cur = nd - 1; int d[20]; gull v = *val; for(int i = nd - 1; i >= 0; i--){ d[i] = (int)(v % 10); v /= 10; }
    int dirty = 1, hu = 0; wait_release();
    while(1){
        gull x = 0; for(int i = 0; i < nd; i++) x = x * 10 + (gull)d[i];
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); ui_bg(pg, 0, 160); ui_panel(pg, 8, 30, 224, 88); ui_head(pg, 14, 34, 212, label, C_GOLD);
            char b[40]; b[0] = 0; for(int i = 0; i < nd; i++){ if(i && (nd - i) % 3 == 0) scat(b, ","); int k = textlen(b); b[k] = (char)('0' + d[i]); b[k + 1] = 0; }
            int tw = textlen(b) * 8, x0 = 120 - tw / 2; text(pg, x0, 56, b, C_WHITE, 2);
            { int ci = 0, pos = 0; for(int i = 0; i < cur; i++){ pos += 8; if((nd - i - 1) % 3 == 0 && i < nd - 1) pos += 8; } (void)ci; rect(pg, x0 + pos, 68, 6, 2, C_GOLD); }
            char r[64]; scpy(r, "FROM "); gv_comma(r, lo); scat(r, " TO "); gv_comma(r, hi); ctext(pg, 120, 82, r, C_GREY);
            if(x < lo || x > hi) ctext(pg, 120, 92, "OUT OF RANGE: IT WILL BE CLAMPED", C_RED);
            ui_foot(pg, "UP DN: DIGIT  L R: MOVE", "A: GO  SEL: MAX  B: BACK"); }
        vsync(); read_keys();
        if(HIT(K_B)){ wait_release(); return 0; }
        if(HIT(K_A) || HIT(K_START)){ if(x < lo) x = lo; if(x > hi) x = hi; *val = x; wait_release(); return 1; }
        if(HIT(K_SEL)){ gull t = hi; for(int i = nd - 1; i >= 0; i--){ d[i] = (int)(t % 10); t /= 10; } dirty = 1; }
        if(HIT(K_LEFT) || HIT(K_L)){ if(cur > 0) cur--; dirty = 1; sfx(7); } if(HIT(K_RIGHT) || HIT(K_R)){ if(cur < nd - 1) cur++; dirty = 1; sfx(7); }
        int up = HELD(K_UP), dn = HELD(K_DOWN);
        if(up || dn){ int h = hu++; if(h == 0 || (h > 16 && h % 3 == 0)){ d[cur] = (d[cur] + (up ? 1 : 9)) % 10; dirty = 1; } } else hu = 0;
    }
}

// ---- a little pop-up list. returns the item or -1
static int gv_menu(volatile u16 *pg, const char *title, const char *const *it, int n){
    int sel = 0, dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; int h = 20 + n * 10; int y0 = 80 - h / 2; panel(pg, 30, y0, 180, h, title);
            for(int i = 0; i < n; i++){ int y = y0 + 17 + i * 10; if(i == sel) rect(pg, 34, y - 1, 172, 10, 6); text(pg, 40, y, it[i], i == sel ? C_WHITE : C_GREY, 1); } }
        vsync(); read_keys();
        if(HIT(K_B)){ wait_release(); return -1; }
        if(HIT(K_A)){ wait_release(); return sel; }
        if(HIT(K_UP)){ sel = (sel + n - 1) % n; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ sel = (sel + 1) % n; dirty = 1; sfx(7); }
    }
}

// ---- the compare table: what the number is bigger than (digits count, i.e. 10^x). label: 0 REAL (exact), 1 ESTIMATE, 2 INVENTED
typedef struct { const char *nm; const char *e; long long ex; int lab; const char *note; } GvCmp;
static const GvCmp gv_cmp[] = {
    { "RUBIK'S CUBE POSITIONS", "4.3 X 10^19", 19, 0, "COUNTED EXACTLY" },
    { "WAYS TO ORDER 52 CARDS", "8.07 X 10^67", 67, 0, "52 FACTORIAL, EXACT" },
    { "ATOMS IN THE UNIVERSE", "ABOUT 10^80", 80, 1, "PHYSICS ESTIMATE" },
    { "SHANNON NUMBER (CHESS)", "ABOUT 10^120", 120, 1, "1950 ESTIMATE OF CHESS GAMES" },
    { "LEGAL GO POSITIONS (19X19)", "2.08 X 10^170", 170, 0, "COUNTED EXACTLY" },
    { "PLANCK VOLUMES IN THE UNIVERSE", "ABOUT 10^185", 185, 1, "THE V2 BUGSTARILLION'S ANCHOR" },
    { "A GOOGOL", "10^100", 100, 0, "A DEFINITION: 1 AND 100 ZEROS" },
    { "A GBA'S 256 KIB OF MAIN RAM", "2^(2^21): 631,306 DIGITS", 631305, 0, "EVERY STATE IT CAN HOLD" },
    { "BIGGEST KNOWN PRIME (2024)", "2^136279841-1: 41,024,320 DIGITS", 41024319, 0, "FOUND IN 2024" },
};
static const int gv_ncmp = 9;
// log10 of the number on show, as a whole number of orders of magnitude: digits - 1
static void gv_cmp_lines(int w, char L[][64], int *C, int *n){
    int k = 0; gull D = gv_total(w == GV_V3 ? GV_POW : w); char b[64];
    scpy(L[k], w == GV_V3 ? "V3 IS ABOUT 10 TO THE POWER (3 X 2^(2^32)): MORE" : "THIS NUMBER IS ABOUT 10 TO THE POWER"); C[k++] = C_GOLD;
    if(w == GV_V3){ scpy(L[k], "ORDERS OF MAGNITUDE THAN ITS EXPONENT HAS DIGITS."); C[k++] = C_GOLD; }
    else { b[0] = 0; gv_comma(b, D - 1); scpy(L[k], b); C[k++] = C_WHITE; }
    scpy(L[k], "BIGGER THAN (REAL = EXACT, EST = ESTIMATE):"); C[k++] = C_GREY;
    for(int i = 0; i < gv_ncmp; i++){
        const GvCmp *c = &gv_cmp[i];
        scpy(L[k], c->nm); scat(L[k], c->lab == 0 ? " [REAL]" : " [EST]"); C[k++] = C_WHITE;
        scpy(L[k], "  "); scat(L[k], c->e); C[k++] = C_CYAN;
        if(w == GV_V3){ scpy(L[k], "  V3 IS FAR, FAR BEYOND IT"); }
        else { gull ex = (gull)c->ex; if(ex < D - 1){ scpy(L[k], "  OURS HAS "); gv_comma(L[k], D - 1 - ex); scat(L[k], " MORE ZEROS"); } else scpy(L[k], "  SMALLER THAN OURS? NO"); }
        C[k++] = C_GREEN;
    }
    scpy(L[k], "A GOOGOLPLEX (10^(10^100)) HAS A GOOGOL DIGITS."); C[k++] = C_RED;
    scpy(L[k], "OURS HAS ONLY ABOUT A BILLION: IT IS FAR SMALLER."); C[k++] = C_RED;
    scpy(L[k], "GRAHAM'S NUMBER IS BEYOND ALL OF THESE (NOT EXACT)."); C[k++] = C_RED;
    *n = k;
}

// ================================================================= THE VIEWER
static int gv_big;                                              // big type (fewer digits, easier to read, good on a phone)
typedef struct { int rw, pitch, rows, y0, gw; } GvLay;
static GvLay gv_lay(int w){
    GvLay l; int hdr = w == GV_V3 ? 21 : 13;
    if(gv_big){ l.rw = w == GV_V3 ? 25 : 18; l.pitch = 12; l.gw = 32; } else { l.rw = w == GV_V3 ? 52 : 36; l.pitch = 7; l.gw = 16; }
    l.y0 = hdr + 2; l.rows = (148 - l.y0) / l.pitch; return l;
}
static int gv_off(int w){ return w == GV_V3 ? 0 : (int)((3 - gi_digits(w) % 3) % 3); }      // virtual zeros in front so the commas fall in the right places
static gull gv_nrows(int w){ gull t = gv_total(w); GvLay l = gv_lay(w); return (t + (gull)gv_off(w) + (gull)l.rw - 1) / (gull)l.rw; }

static void gv_draw(volatile u16 *pg, int w, gull top, int msg_t, const char *msg){
    GvLay l = gv_lay(w); gull T = gv_total(w); int off = gv_off(w); char b[96], row[80];
    ui_bg(pg, 0, 160);
    gull p0 = top * (gull)l.rw; p0 = p0 >= (gull)off ? p0 - (gull)off : 0; if(top == 0 && off) p0 = 0;     // real position of the first digit on the screen
    scpy(b, w == GV_V3 ? "LETTER " : "DIGIT "); gv_comma(b, p0 + 1); scat(b, " OF "); gv_comma(b, T); text(pg, 3, 3, b, C_GOLD, 1);
    { char pc[16]; gull pm = (gull)((long double)p0 * 1000.0L / (long double)T); scpy(pc, ""); ncat(pc, (s64)(pm / 10)); scat(pc, "."); ncat(pc, (s64)(pm % 10)); scat(pc, "%"); rtext(pg, 236, 3, pc, C_CYAN); }
    if(w == GV_V3){ V3Info vi; if(gv3_info(p0, &vi) == 0){ scpy(b, "TERM "); gv_comma(b, vi.j); scat(b, " OF "); gv_comma(b, gv3_parts()); scat(b, "  VALUE "); ncat(b, vi.coef); text(pg, 3, 11, b, C_GREEN, 1); } }
    static char buf[40 * 64 + 8]; int want = l.rows * l.rw; if(want > (int)sizeof buf - 8) want = (int)sizeof buf - 8;
    gv_fetch(w, p0, want, buf);
    int k = 0;                                                   // k = index in buf
    for(int r = 0; r < l.rows; r++){
        int y = l.y0 + r * l.pitch; gull vstart = (top + (gull)r) * (gull)l.rw;                  // virtual index of this row's first digit
        if(vstart >= T + (gull)off) break;
        int first = (r == 0 && top == 0) ? off : 0;              // virtual blanks at the very start
        int cwid = gv_big ? 8 : 4;
        if(w == GV_V3){
            int n = l.rw; for(int i = 0; i < n; i++) row[i] = buf[k + i] ? buf[k + i] : ' '; row[n] = 0; k += n;
            text(pg, 6, y, row, C_WHITE, gv_big ? 2 : 1);
            for(int i = 0; i < n; i++) if(row[i] == ',' || row[i] == '.'){ char ch[2] = { row[i], 0 }; text(pg, 6 + i * cwid, y, ch, C_GOLD, gv_big ? 2 : 1); }
        } else {
            int groups = l.rw / 3, x0 = gv_big ? 20 : 22, cw = gv_big ? 8 : 4;
            for(int g = 0; g < groups; g++){
                char dg[4]; int gl = 0;
                for(int i = 0; i < 3; i++){ gull v = vstart + (gull)(g * 3 + i); if(v >= (gull)off && v < T + (gull)off) dg[gl++] = buf[k++]; }
                dg[gl] = 0; if(!gl) continue;
                int x = x0 + g * l.gw + (3 - gl) * cw;
                text(pg, x, y, dg, (g & 1) ? C_CYAN : C_WHITE, gv_big ? 2 : 1);
                if(g < groups - 1 && vstart + (gull)(g * 3 + 3) < T + (gull)off) text(pg, x + gl * cw, y, ",", C_DGREY, gv_big ? 2 : 1);
            }
        }
    }
    { gull nr = gv_nrows(w); int th = 12; int ty = l.y0 + (nr > 1 ? (int)((long double)top / (long double)(nr - 1) * (long double)(148 - l.y0 - th)) : 0); rect(pg, 236, l.y0, 2, 148 - l.y0, U_PANEL); rect(pg, 236, ty, 2, th, C_GOLD); }
    if(msg_t > 0){ int wd = textlen(msg) * 4 + 8; rect(pg, 120 - wd / 2, 74, wd, 12, U_BG0); rect(pg, 120 - wd / 2, 74, wd, 1, C_GOLD); text(pg, 124 - wd / 2, 77, msg, C_WHITE, 1); }
    ui_foot(pg, "UP DN LINE  L R PAGE", "A JUMP  START MENU");
}

// ---- exporting. progress callback: draws a bar, B cancels
typedef struct { volatile u16 *pg; int w; double t0, tlast; const char *path; } GvEx;
static int gv_ex_cb(gull done, gull total, void *ud){
    GvEx *e = ud; double t = gv_now(); if(t - e->tlast < 0.04 && done < total) return 0; e->tlast = t;
    volatile u16 *pg = e->pg; ui_bg(pg, 0, 160); ui_panel(pg, 8, 24, 224, 100); ui_head(pg, 14, 28, 212, "WRITING THE FILE", C_GOLD);
    { char b[600]; scpy(b, "TO "); scat(b, e->path); if(textlen(b) > 54){ int n = textlen(b); scpy(b, "TO ..."); scat(b, e->path + n - 3 - 46 + 4 - 4 > e->path ? e->path + (textlen(e->path) - 46) : e->path); } text(pg, 14, 42, b, C_CYAN, 1); }
    rect(pg, 14, 58, 212, 10, U_PANEL); int wd = (int)((long double)done / (long double)total * 210.0L); rect(pg, 15, 59, wd, 8, C_GREEN);
    { char b[96]; gull pm = (gull)((long double)done * 1000.0L / (long double)total); scpy(b, ""); ncat(b, (s64)(pm / 10)); scat(b, "."); ncat(b, (s64)(pm % 10)); scat(b, "%   "); gv_comma(b, done); scat(b, " OF "); gv_comma(b, total); ctext(pg, 120, 74, b, C_WHITE); }
    { char b[64]; scpy(b, "TIME "); ncat(b, (s64)(t - e->t0)); scat(b, " S"); ctext(pg, 120, 88, b, C_GREY); }
    ui_foot(pg, "", "B: CANCEL"); vsync(); read_keys(); return HIT(K_B) ? 1 : 0;
}
static void gv_export_dir(char *d, int n){ const char *sv = pc_sav_path(); d[0] = 0; if(sv){ scpy(d, sv); char *sl = strrchr(d, '/'); if(sl) *sl = 0; else scpy(d, "."); } else scpy(d, "."); (void)n; }
static void gv_do_export(volatile u16 *pg, int w, gull start, gull n){
    char dir[600], path[700]; gv_export_dir(dir, sizeof dir);
    if(w == GV_V3) snprintf(path, sizeof path, "%s/bugstar-v3-name-from-%llu.txt", dir, start + 1);
    else snprintf(path, sizeof path, "%s/bugstar-giant-%s.txt", dir, w == GV_POW ? "2-to-the-2-to-the-32" : "counting-number");
    char hx[65]; gull by = 0; GvEx e; e.pg = pg; e.w = w; e.t0 = e.tlast = gv_now(); e.path = path; int rc;
    wait_release();
    if(w == GV_V3) rc = gv3_export(path, start, n, gv_ex_cb, &e, hx); else rc = gi_export(w, path, gv_ex_cb, &e, hx, &by);
    if(w == GV_V3) by = n;
    int dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; ui_bg(pg, 0, 160); ui_panel(pg, 8, 24, 224, 110); ui_head(pg, 14, 28, 212, rc == 0 ? "FILE WRITTEN" : rc == 1 ? "CANCELLED" : "COULD NOT WRITE", rc == 0 ? C_GREEN : C_RED);
            char b[700]; scpy(b, path); char ls[3][60]; int k = pc_lw_wrap(b, 52, ls, 3); for(int i = 0; i < k; i++) text(pg, 14, 42 + i * 7, ls[i], C_CYAN, 1);
            if(rc == 0){ scpy(b, ""); gv_comma(b, by); scat(b, " BYTES"); text(pg, 14, 66, b, C_WHITE, 1);
                text(pg, 14, 76, "SHA-256 OF THE FILE:", C_GREY, 1); char h1[40], h2[40]; for(int i = 0; i < 32; i++){ h1[i] = hx[i]; h2[i] = hx[32 + i]; } h1[32] = h2[32] = 0; text(pg, 14, 84, h1, C_WHITE, 1); text(pg, 14, 91, h2, C_WHITE, 1);
                text(pg, 14, 102, w == GV_V3 ? "LETTERS OF THE NAME, NOTHING ELSE." : "100 DIGITS TO A LINE, NO COMMAS.", C_GREY, 1); }
            else if(rc == 1) text(pg, 14, 66, "THE FILE IS NOT COMPLETE. IT WAS LEFT ON THE DISK.", C_GREY, 1);
            ui_foot(pg, "", "A: OK"); }
        vsync(); read_keys(); if(HIT(K_A) || HIT(K_B) || HIT(K_START)) break;
    }
    wait_release();
}

static void gv_info(volatile u16 *pg, int w);
static gull gv_pos_of_row(int w, gull top){ int off = gv_off(w); GvLay l = gv_lay(w); gull p = top * (gull)l.rw; return p >= (gull)off ? p - (gull)off : 0; }
static gull gv_row_of_pos(int w, gull p){ GvLay l = gv_lay(w); return (p + (gull)gv_off(w)) / (gull)l.rw; }
static void gv_view(volatile u16 *pg, int w, gull start_pos){
    if(!gv_ready(w)){ gv_missing_screen(pg, w); if(!gv_ready(w)) return; }
    gull top = gv_row_of_pos(w, start_pos), nrows = gv_nrows(w), T = gv_total(w); int dirty = 1, hold = 0, msg_t = 0; char msg[48]; msg[0] = 0;
    wait_release();
    while(1){
        GvLay l = gv_lay(w); nrows = gv_nrows(w); gull maxtop = nrows > (gull)l.rows ? nrows - (gull)l.rows : 0; if(top > maxtop) top = maxtop;
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); gv_draw(pg, w, top, msg_t, msg); }
        vsync(); read_keys(); pc_lw_clock++; if(msg_t > 0){ msg_t--; if(!msg_t) dirty = 1; }
        if(HIT(K_B)) break;
        // scroll: a press moves one row (or page); holding it waits a moment and then goes faster and faster
        int dn = HELD(K_DOWN) || HELD(K_R), up = HELD(K_UP) || HELD(K_L); gull unit = (HELD(K_UP) || HELD(K_DOWN)) ? 1 : (gull)(l.rows > 1 ? l.rows - 1 : 1);
        if(dn || up){
            int h = hold++; gull st = 0;
            if(h == 0) st = 1; else if(h >= 14 && h < 40) st = (h % 3 == 0) ? 1 : 0; else if(h >= 40){ int e = (h - 40) / 7; if(e > 30) e = 30; st = (gull)1 << e; }
            if(st){ gull d = st * unit; if(dn){ top = (top + d > maxtop || top + d < top) ? maxtop : top + d; } else top = top > d ? top - d : 0; dirty = 1; }
        } else hold = 0;
        if(HIT(K_SEL)){ gv_big = !gv_big; dirty = 1; sfx(7); gull p = gv_pos_of_row(w, top); top = gv_row_of_pos(w, p); }
        if(HIT(K_A)){ gull v = gv_pos_of_row(w, top) + 1;
            if(gv_enter(pg, w == GV_V3 ? "JUMP TO LETTER NUMBER" : "JUMP TO DIGIT NUMBER", &v, 1, T)){ top = gv_row_of_pos(w, v - 1); } dirty = 1; }
        if(HIT(K_START)){
            static const char *const m1[] = { "JUMP TO DIGIT NUMBER", "GO TO THE START", "GO TO THE END", "ABOUT THIS NUMBER", "WRITE THE WHOLE NUMBER TO A FILE", "BIG / SMALL TYPE", "BACK TO THE VIEWER" };
            static const char *const m3[] = { "JUMP TO LETTER NUMBER", "JUMP TO TERM NUMBER", "GO TO THE START", "GO TO THE END", "ABOUT THE NAME", "WRITE 1 MB FROM HERE TO A FILE", "WRITE 100 MB FROM HERE", "BIG / SMALL TYPE", "BACK TO THE VIEWER" };
            int c = w == GV_V3 ? gv_menu(pg, "MENU", m3, 9) : gv_menu(pg, "MENU", m1, 7); dirty = 1;
            if(c >= 0){
                if(w == GV_V3){
                    if(c == 0){ gull v = gv_pos_of_row(w, top) + 1; if(gv_enter(pg, "JUMP TO LETTER NUMBER", &v, 1, T)) top = gv_row_of_pos(w, v - 1); }
                    else if(c == 1){ gull v = 1; V3Info vi; if(gv3_info(gv_pos_of_row(w, top), &vi) == 0) v = vi.j; if(gv_enter(pg, "JUMP TO TERM NUMBER", &v, 1, gv3_parts())) top = gv_row_of_pos(w, gv3_term_start(v)); }
                    else if(c == 2) top = 0; else if(c == 3) top = maxtop; else if(c == 4) gv_info(pg, w);
                    else if(c == 5){ gull p = gv_pos_of_row(w, top); gv_do_export(pg, w, p, 1ull << 20); }
                    else if(c == 6){ gull p = gv_pos_of_row(w, top); gv_do_export(pg, w, p, 100ull << 20); }
                    else if(c == 7){ gv_big = !gv_big; gull p = gv_pos_of_row(w, top); top = gv_row_of_pos(w, p); }
                } else {
                    if(c == 0){ gull v = gv_pos_of_row(w, top) + 1; if(gv_enter(pg, "JUMP TO DIGIT NUMBER", &v, 1, T)) top = gv_row_of_pos(w, v - 1); }
                    else if(c == 1) top = 0; else if(c == 2) top = maxtop; else if(c == 3) gv_info(pg, w); else if(c == 4) gv_do_export(pg, w, 0, T);
                    else if(c == 5){ gv_big = !gv_big; gull p = gv_pos_of_row(w, top); top = gv_row_of_pos(w, p); }
                }
            }
            dirty = 1; wait_release();
        }
    }
    wait_release();
}

// ================================================================= ABOUT PAGES (what it is, how many digits, name parts, compare, how it was checked)
typedef struct { char t[26][64]; int col[26]; int n; char title[28]; } GvPage;
static void gv_ln(GvPage *p, const char *s, int c){ if(p->n < 26){ scpy(p->t[p->n], s); p->t[p->n][58] = 0; p->col[p->n] = c; p->n++; } }
static void gv_para(GvPage *p, const char *s, int c){ char ls[8][60]; int k = pc_lw_wrap(s, 55, ls, 8); for(int i = 0; i < k; i++) gv_ln(p, ls[i], c); }
static void gv_chunk_line(GvPage *p, int v, int c){                          // "327 = SEPTEM + VIGINTI + TRECENTI" and what each piece means
    char pc[3][24], dd[3][28]; int k = gcw_parts(v, pc, dd); char b[100]; b[0] = 0; ncat(b, v); scat(b, " = ");
    for(int i = 0; i < k; i++){ if(i) scat(b, " + "); scat(b, pc[i]); }
    gv_ln(p, b, c); b[0] = 0; scat(b, "    "); for(int i = 0; i < k; i++){ if(i) scat(b, ", "); scat(b, dd[i]); } gv_ln(p, b, C_GREY);
}
static int gv_pages(int w){ return 5; }
static void gv_lead_trail(GvPage *p, int w){                                 // the first and last digits, read live
    char s[48], b[64]; gull D = gi_digits(w); gv_fetch(w, 0, 40, s); s[40] = 0;
    scpy(b, "FIRST: "); scat(b, s); gv_ln(p, b, C_CYAN);
    gv_fetch(w, D - 40, 40, s); s[40] = 0; scpy(b, "LAST:  "); scat(b, s); gv_ln(p, b, C_CYAN);
}
static void gv_digitcount_name(GvPage *p, int w){
    gull D = gi_digits(w), G = (D + 2) / 3; char b[200], m[40];
    scpy(b, "DIGITS: "); gv_comma(b, D); gv_ln(p, b, C_WHITE);
    char wd[400]; gen_u64(D, wd, sizeof wd); gv_para(p, wd, C_GOLD);
    scpy(b, "IN GROUPS OF THREE: "); gv_comma(b, G); scat(b, " GROUPS"); gv_ln(p, b, C_WHITE);
    scpy(m, ""); { gull t = G - 2; char r[24]; int n = 0; if(!t) r[n++] = '0'; while(t){ r[n++] = (char)('0' + t % 10); t /= 10; } for(int i = 0; i < n; i++) m[i] = r[n - 1 - i]; m[n] = 0; }
    int lead = (int)(D - 3 * (G - 1));
    scpy(b, "THE FIRST GROUP HAS "); ncat(b, lead); scat(b, lead == 1 ? " DIGIT. IT SITS ON TOP OF 1000^" : " DIGITS. IT SITS ON TOP OF 1000^"); gv_comma(b, G - 1); gv_ln(p, b, C_GREY);
    scpy(b, "1000^"); gv_comma(b, G - 1); scat(b, " IS THE"); gv_ln(p, b, C_GREY);
    scpy(b, m); { char c2[60]; c2[0] = 0; gv_comma(c2, (gull)strtoull(m, 0, 10)); scpy(b, c2); } scat(b, "-ILLION, SAID:"); gv_ln(p, b, C_GREY);
    { char nm[200]; gcw_place(m, nm, sizeof nm); gv_para(p, nm, C_GOLD); }
}
static void gv_page_about(GvPage *p, int w, int pg){
    p->n = 0; char b[200];
    if(w != GV_V3){
        gull D = gi_digits(w); (void)D;
        if(pg == 0){ scpy(p->title, "WHAT IT IS");
            if(w == GV_POW){
                gv_ln(p, "2 TO THE POWER 2^32  =  2^4,294,967,296", C_GOLD);
                gv_para(p, "THE NUMBER OF DIFFERENT THINGS THAT 512 MIB (4,294,967,296 BITS) OF MEMORY CAN HOLD: EACH BIT IS 0 OR 1, SO IT IS 2 X 2 X 2 ... WITH 4.29 BILLION TWOS.", C_WHITE);
                gv_para(p, "REAL: YES. A TRUE MATHEMATICAL NUMBER, NOT INVENTED. EVERY DIGIT IS EXACT.", C_GREEN);
                gv_para(p, "HOW THE DIGITS WERE MADE: PYTHON'S DECIMAL MODULE SQUARED 2 THIRTY-TWO TIMES (2, 4, 16, 256 ...), WITH NO ROUNDING AT ALL, AND WROTE THE DIGITS TO ONE FILE. THE GAME NEVER KEEPS THE WHOLE NUMBER IN MEMORY: IT JUMPS INTO THE FILE.", C_GREY);
            } else {
                gv_ln(p, "1 2 3 4 5 ... 123,456,789 123,456,790 IN A ROW", C_GOLD);
                gv_para(p, "THE WHOLE NUMBERS WRITTEN ONE AFTER ANOTHER: 123456789101112... UP TO 123,456,790. IT IS CHAMPERNOWNE'S CONSTANT (1933), CUT OFF AFTER 123,456,790, AS A WHOLE NUMBER.", C_WHITE);
                gv_para(p, "REAL: YES, A WELL KNOWN MATHEMATICAL OBJECT. EVERY DIGIT IS EXACT AND WORKED OUT ON THE SPOT BY ARITHMETIC, SO IT NEEDS NO FILE.", C_GREEN);
                gv_para(p, "THE CUT-OFF WAS CHOSEN SO THAT IT HAS JUST OVER A BILLION DIGITS: 1,000,000,008.", C_GREY);
            } }
        else if(pg == 1){ scpy(p->title, "HOW MANY DIGITS"); gv_digitcount_name(p, w);
            if(w == GV_POW){ gv_ln(p, "WORKED OUT AS FLOOR(2^32 X LOG10(2)) + 1.", C_GREY); } else gv_ln(p, "9 + 180 + 2,700 + ... + 9 X 23,456,791 (9-DIGIT NUMBERS).", C_GREY);
            gv_lead_trail(p, w); }
        else if(pg == 2){ scpy(p->title, "THE PARTS OF THE NAME");
            gull G = (D + 2) / 3; gull m = G - 2; int c[3]; c[0] = (int)(m / 1000000); c[1] = (int)((m / 1000) % 1000); c[2] = (int)(m % 1000);
            gv_ln(p, "A NUMBER IS NAMED BY ITS POWER OF A THOUSAND:", C_WHITE);
            gv_ln(p, "10^(3N+3) IS THE N-ILLION (CONWAY-WECHSLER RULES).", C_WHITE);
            gv_ln(p, "N IS CUT IN GROUPS OF THREE DIGITS; EACH GROUP GETS", C_GREY); gv_ln(p, "A LATIN PIECE AND ENDS IN -ILLI; THE LAST ENDS -ILLION.", C_GREY);
            b[0] = 0; gv_comma(b, m); scat(b, "-ILLION HAS 3 GROUPS:"); gv_ln(p, b, C_GOLD);
            for(int i = 0; i < 3; i++) gv_chunk_line(p, c[i], i == 2 ? C_CYAN : C_WHITE);
            gv_ln(p, "UNITS FIRST, THEN TENS, THEN HUNDREDS. A LETTER (S X M N)", C_GREY); gv_ln(p, "IS ADDED TO THE UNIT SO THE WORD SOUNDS RIGHT.", C_GREY); }
        else if(pg == 3){ scpy(p->title, "COMPARE"); static char L[40][64]; static int C[40]; int n; gv_cmp_lines(w, L, C, &n); for(int i = 0; i < n && p->n < 26; i++) gv_ln(p, L[i], C[i]); }
        else { scpy(p->title, "HOW IT WAS CHECKED");
            if(w == GV_POW){
                gv_para(p, "1. EVERY DIGIT, MODULO 8 PRIMES, AGAINST PYTHON'S POW(2, 2^32, P): THE FILE'S WHOLE VALUE MUST MATCH.", C_WHITE);
                gv_para(p, "2. THE LAST 2,000 DIGITS FROM POW(2, 2^32, 10^2000) AND THE FIRST 300 FROM A SEPARATE LOGARITHM CALCULATION.", C_WHITE);
                gv_para(p, "3. THOUSANDS OF RANDOM WINDOWS READ BY THE GAME'S CODE AGAINST A SEPARATE PYTHON READER OF THE FILE.", C_WHITE);
                gv_para(p, "4. THE EXPORT'S SHA-256 AGAINST PYTHON'S. RUN PC/GIANT/TEST.SH", C_WHITE);
            } else {
                gv_para(p, "1. THOUSANDS OF RANDOM WINDOWS, AT EVERY CHANGE FROM 1-DIGIT TO 2-DIGIT ... TO 9-DIGIT NUMBERS, AND AT THE END, AGAINST A PYTHON READER THAT FINDS THE NUMBER BY BISECTION.", C_WHITE);
                gv_para(p, "2. THE EXPORT'S SIZE AND SHA-256 AGAINST A PYTHON PROGRAM THAT WRITES 1 TO 123,456,790 ITSELF. RUN PC/GIANT/TEST.SH", C_WHITE);
            } }
    } else {
        gull L = gv3_parts(), TC = gv3_chars();
        if(pg == 0){ scpy(p->title, "WHAT V3 IS");
            gv_ln(p, "BUGSTARILLION V3  =  SUM OF C(J) X 1000^(N(J)+1)", C_GOLD);
            b[0] = 0; scat(b, "J = 1 TO "); gv_comma(b, L); gv_ln(p, b, C_WHITE);
            gv_para(p, "N(J) IS THE NUMBER MADE BY THE FIRST J DIGITS OF 2^(2^32). C(J) IS A THREE-DIGIT NUMBER, 100 TO 999, MADE FROM J BY A FIXED HASH.", C_WHITE);
            gv_para(p, "EACH TERM IS SAID: THE WORDS OF C(J), THEN THE N(J)-ILLION. THE FIRST TERM IS 2-ILLION (BILLION), THE NEXT 20-ILLION (VIGINTILLION), THEN 200-ILLION ... UP TO THE 2^(2^32)-ILLION.", C_WHITE);
            gv_para(p, "THE NAME GROWS ONE DIGIT OF N(J) AT A TIME, IN ORDER, LIKE V2 DID. IT IS SAID BIGGEST TERM FIRST.", C_GREY); }
        else if(pg == 1){ scpy(p->title, "HOW BIG");
            b[0] = 0; scat(b, "TERMS (PARTS): "); gv_comma(b, L); gv_ln(p, b, C_WHITE);
            b[0] = 0; scat(b, "LETTERS IN THE WHOLE NAME: "); gv_comma(b, TC); gv_ln(p, b, C_GOLD);
            { char wd[400]; gen_u64(TC, wd, sizeof wd); gv_para(p, wd, C_GREY); }
            { V3Info vi; if(gv3_info(0, &vi) == 0){ b[0] = 0; scat(b, "BIGGEST TERM'S -ILLION NAME: "); gv_comma(b, vi.placelen); scat(b, " LETTERS"); gv_ln(p, b, C_CYAN); } }
            gv_para(p, "THE NUMBER ITSELF IS ABOUT 10 TO THE POWER (3 X 2^(2^32)), I.E. A 1 WITH ABOUT 3 X 2^(2^32) ZEROS: THE COUNT OF ITS DIGITS ITSELF HAS 1,292,913,987 DIGITS.", C_WHITE);
            gv_ln(p, "SAID ALOUD AT ONE LETTER A SECOND IT WOULD TAKE:", C_GREY);
            { gull yrs = TC / 31557600ull; b[0] = 0; scat(b, "THAT IS "); gv_comma(b, yrs); scat(b, " YEARS."); gv_ln(p, b, C_GREEN); } }
        else if(pg == 2){ scpy(p->title, "THE PARTS");
            gv_ln(p, "THE FIRST TERMS, N(J) AND ITS NAME:", C_WHITE);
            for(int j = 1; j <= 7; j++){ char s[16]; gi_read(GI_POW, 0, j, s); s[j] = 0; char nm[80]; gcw_place(s, nm, sizeof nm); b[0] = 0; ncat(b, j); scat(b, ". N="); scat(b, s); scat(b, " IS THE "); scat(b, nm); b[58] = 0; gv_ln(p, b, j & 1 ? C_CYAN : C_WHITE); }
            gv_ln(p, "TERM J HAS J/3 PIECES; THE LAST TERM HAS 430,971,329.", C_GREY);
            gv_ln(p, "A TERM IS SAID: <C(J) IN WORDS> <N(J)-ILLION>, AND A", C_GREY); gv_ln(p, "COMMA. THE LAST TERM ENDS WITH A FULL STOP.", C_GREY); }
        else if(pg == 3){ scpy(p->title, "THE RECORD");
            gv_ln(p, "WHAT I FOUND, AND WHAT THIS BEATS:", C_WHITE);
            gv_ln(p, "CONWAY-GUY GOOGOLPLEX NAME: ABOUT 769 LETTERS [REAL]", C_GREY);
            gv_ln(p, "NOLL'S GOOGOLPLEX WORD: 3,871 LETTERS [REAL]", C_GREY);
            gv_ln(p, "BUGSTARILLION V2 (GAME): 11,896,480 PARTS, ABOUT", C_GREY); gv_ln(p, "  8.9 BILLION LETTERS OF -ILLION NAMES [INVENTED]", C_GREY);
            b[0] = 0; scat(b, "BUGSTARILLION V3: "); gv_comma(b, L); scat(b, " PARTS,"); gv_ln(p, b, C_GOLD);
            b[0] = 0; scat(b, "  "); gv_comma(b, TC); scat(b, " LETTERS [INVENTED]"); gv_ln(p, b, C_GOLD);
            gv_para(p, "THE MEASURE: LENGTH OF ONE COMPLETE, WRITTEN-OUT, RULE-MADE NAME OF A NUMBER THAT YOU CAN READ ANYWHERE IN AND CHECK. NO OFFICIAL RECORD EXISTS FOR IT. THE -ILLION RULES CAN NAME ANY NUMBER, AND BOWERS' NAMES REACH FAR BIGGER NUMBERS IN FEWER LETTERS: V3 DOES NOT BEAT THOSE.", C_RED); }
        else { scpy(p->title, "REAL OR INVENTED");
            gv_ln(p, "REAL (CHECKED):", C_GREEN);
            gv_para(p, "THE -ILLION RULES (CONWAY, GUY, WECHSLER). OUR CODE GIVES A GOOGOLPLEX NAME OF 766 LETTERS (+ TEN) MATCHING THE PUBLISHED 769. THE DIGITS OF 2^(2^32).", C_WHITE);
            gv_ln(p, "INVENTED (THIS GAME):", C_RED);
            gv_para(p, "THE BUGSTARILLION ITSELF: THE SUM, THE HASH THAT MAKES C(J), THE ORDER THE NAME IS SAID IN, AND THE CLAIM THAT IT IS A RECORD. IT IS A RECORD ONLY FOR THE MEASURE ABOVE, AND ONLY AS FAR AS I COULD FIND.", C_WHITE);
            gv_para(p, "CHECKED BY PC/GIANT/TEST.SH AGAINST SEPARATE PYTHON CODE: TOTAL LETTERS, TERM STARTS, THOUSANDS OF WINDOWS, EXPORT SHA-256.", C_GREY); }
    }
}
static void gv_info(volatile u16 *pg, int w){
    static GvPage P; int page = 0, dirty = 1, scroll = 0; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); gv_page_about(&P, w, page);
            ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143); char b[64]; scpy(b, gv_title(w)); if(textlen(b) > 30) b[30] = 0; scat(b, ": "); scat(b, P.title); b[56] = 0; ui_head(pg, 9, 8, 222, b, C_GOLD);
            { char pn[16]; pn[0] = 0; ncat(pn, page + 1); scat(pn, "/"); ncat(pn, gv_pages(w)); rtext(pg, 231, 8, pn, U_SOFT); }
            int rows = 17; if(scroll > P.n - rows) scroll = P.n - rows; if(scroll < 0) scroll = 0;
            for(int i = 0; i < rows && scroll + i < P.n; i++) text(pg, 9, 19 + i * 7, P.t[scroll + i], P.col[scroll + i], 1);
            if(scroll > 0) text(pg, 229, 19, "^", C_CYAN, 1); if(scroll + rows < P.n) text(pg, 229, 130, "v", C_CYAN, 1);
            ui_foot(pg, "L R: PAGE  UP DN: SCROLL", "B: BACK"); }
        vsync(); read_keys();
        if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_L) || HIT(K_LEFT)){ page = (page + gv_pages(w) - 1) % gv_pages(w); scroll = 0; dirty = 1; sfx(7); }
        if(HIT(K_R) || HIT(K_RIGHT) || HIT(K_A)){ page = (page + 1) % gv_pages(w); scroll = 0; dirty = 1; sfx(7); }
        if(HIT(K_DOWN)){ scroll++; dirty = 1; } if(HIT(K_UP) && scroll > 0){ scroll--; dirty = 1; }
    }
    wait_release();
}

// ================================================================= NAME ANY NUMBER
static int gv_wrap_long(const char *s, int w, char lines[][60], int max){        // wrap at spaces; a word longer than a line is cut after an ILLI, or at the width
    int n = 0, i = 0, L = (int)strlen(s);
    while(i < L && n < max){
        int e = i + w; if(e >= L){ int k = L - i; memcpy(lines[n], s + i, k); lines[n][k] = 0; n++; break; }
        int br = -1; for(int j = e; j > i + w / 2; j--){ if(s[j] == ' '){ br = j; break; } }
        if(br < 0) for(int j = e; j > i + w / 2; j--){ if(j >= 4 && s[j-4] == 'I' && s[j-3] == 'L' && s[j-2] == 'L' && s[j-1] == 'I'){ br = j - 1; break; } }
        int cut; if(br < 0){ cut = e; } else if(s[br] == ' '){ cut = br; } else cut = br + 1;
        int k = cut - i; memcpy(lines[n], s + i, k); lines[n][k] = 0; n++; i = cut; if(i < L && s[i] == ' ') i++;
    }
    return n;
}
static void gv_say_number(const char *s, char *out, int cap){                      // 123456 -> "ONE HUNDRED TWENTY-THREE THOUSAND, FOUR HUNDRED FIFTY-SIX"
    while(*s == '0' && s[1]) s++;
    int d = (int)strlen(s); out[0] = 0;
    if(d == 1 && s[0] == '0'){ scpy(out, "ZERO"); return; }
    int G = (d + 2) / 3, lead = d - 3 * (G - 1), p = 0;
    for(int g = 0; g < G; g++){
        int len = g == 0 ? lead : 3, v = 0; for(int i = 0; i < len; i++) v = v * 10 + (s[p + i] - '0'); p += len;
        if(!v) continue;
        int k = G - 1 - g; char b[64]; gen_chunk(v, b);
        if(textlen(out) + 80 > cap) { scat(out, " ..."); return; }
        if(out[0]) scat(out, ", "); scat(out, b);
        if(k == 1) scat(out, " THOUSAND"); else if(k >= 2){ char m[16]; m[0] = 0; ncat(m, k - 1); char pl[300]; gcw_place(m, pl, sizeof pl); scat(out, " "); scat(out, pl); }
    }
}
static void gv_say_ten(const char *e, char *out, int cap, char *plname, gull *pllen){          // 10^E: ONE / TEN / ONE HUNDRED, then the power of a thousand
    char q[200]; int r; gds_div3(e, q, &r); scpy(out, r == 0 ? "ONE" : r == 1 ? "TEN" : "ONE HUNDRED"); plname[0] = 0; *pllen = 0;
    if(q[0] == '0' && !q[1]) return;
    if(q[0] == '1' && !q[1]){ scat(out, " THOUSAND"); *pllen = 8; scpy(plname, "THOUSAND"); return; }
    char m[200]; gds_sub1(q, m); *pllen = gcw_place(m, plname, 4000); scat(out, " "); { int L = textlen(out); int room = cap - L - 2; int k = 0; while(plname[k] && k < room){ out[L + k] = plname[k]; k++; } out[L + k] = 0; }
}
static void gv_namer(volatile u16 *pg){
    enum { ND = 128, RW = 32 };
    static char dg[ND + 1]; static int init; if(!init){ for(int i = 0; i < ND; i++) dg[i] = '0'; dg[ND] = 0; init = 1; dg[ND - 1] = '3'; dg[ND - 2] = '2'; dg[ND - 3] = '1'; }
    static char out[6000], pl[4100]; static char lines[100][60];
    int cur = ND - 1, mode = 0, view = 0, dirty = 1, scroll = 0, hu = 0, nl = 0; gull pllen = 0; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); const char *s = dg; while(*s == '0' && s[1]) s++;
            if(mode == 0) gv_say_number(s, out, sizeof out); else gv_say_ten(s, out, sizeof out, pl, &pllen);
            nl = gv_wrap_long(out, 54, lines, 100);
            ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143); ui_head(pg, 9, 8, 222, mode == 0 ? "NAME ANY NUMBER" : "NAME 10 TO THE POWER OF ...", C_GOLD);
            { char b[64]; scpy(b, "DIGITS: "); ncat(b, (s64)strlen(s)); rtext(pg, 231, 8, b, U_SOFT); }
            if(!view){ int rows = 128 / RW; for(int r = 0; r < rows; r++){ char rowt[48]; int k = 0; for(int i = 0; i < RW; i++){ int idx = r * RW + i; if(i && i % 4 == 0) rowt[k++] = ' '; rowt[k++] = dg[idx]; } rowt[k] = 0;
                    text(pg, 9, 19 + r * 9, rowt, C_GREY, 1);
                    if(cur / RW == r){ int i = cur % RW; int x = 9 + (i + i / 4) * 4; rect(pg, x, 26 + r * 9, 3, 1, C_GOLD); char ch[2] = { dg[cur], 0 }; text(pg, x, 19 + r * 9, ch, C_WHITE, 1); } } }
            else { text(pg, 9, 19, s, C_CYAN, 1); }
            int y0 = view ? 28 : 59; int rows = view ? 16 : 12; if(scroll > nl - rows) scroll = nl - rows; if(scroll < 0) scroll = 0;
            if(!view) rect(pg, 8, 57, 224, 1, U_EDGE);
            for(int i = 0; i < rows && scroll + i < nl; i++) text(pg, 9, y0 + i * 7, lines[scroll + i], C_WHITE, 1);
            if(view) ui_foot(pg, "UP DN: SCROLL", "A OR B: EDIT"); else ui_foot(pg, "UP DN: DIGIT  L R: MOVE  A: READ", "SEL: 10^ OR N  START: EXAMPLE");
            if(scroll > 0) text(pg, 229, y0, "^", C_CYAN, 1); if(scroll + rows < nl) text(pg, 229, y0 + (rows - 1) * 7, "v", C_CYAN, 1); }
        vsync(); read_keys();
        if(view){ if(HIT(K_A) || HIT(K_B)){ view = 0; dirty = 1; } if(HELD(K_DOWN)){ if(hu == 0 || (hu > 14 && hu % 2 == 0)){ scroll++; dirty = 1; } hu++; } else if(HELD(K_UP)){ if(hu == 0 || (hu > 14 && hu % 2 == 0)){ if(scroll > 0) scroll--; dirty = 1; } hu++; } else hu = 0; continue; }
        if(HIT(K_B)) break;
        if(HIT(K_A)){ view = 1; scroll = 0; dirty = 1; sfx(2); }
        if(HIT(K_SEL)){ mode = !mode; dirty = 1; sfx(7); }
        if(HIT(K_START)){ static const char *const ex[] = { "1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", "80658175170943878571660636856403766975289505440883277824000000000000", "340282366920938463463374607431768211456", "3003", "1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001" };
            static int ei; for(int i = 0; i < ND; i++) dg[i] = '0'; const char *e = ex[ei++ % 5]; int L = (int)strlen(e); for(int i = 0; i < L; i++) dg[ND - L + i] = e[i]; cur = ND - 1; dirty = 1; sfx(7); }
        if(HIT(K_LEFT)){ if(cur > 0) cur--; dirty = 1; } if(HIT(K_RIGHT)){ if(cur < ND - 1) cur++; dirty = 1; }
        if(HIT(K_L)){ cur = cur >= 8 ? cur - 8 : 0; dirty = 1; } if(HIT(K_R)){ cur = cur + 8 < ND ? cur + 8 : ND - 1; dirty = 1; }
        int up = HELD(K_UP), dn = HELD(K_DOWN);
        if(up || dn){ int h = hu++; if(h == 0 || (h > 16 && h % 3 == 0)){ int v = dg[cur] - '0'; v = (v + (up ? 1 : 9)) % 10; dg[cur] = (char)('0' + v); dirty = 1; } } else hu = 0;
    }
    wait_release();
}

// ================================================================= THE SCALE LADDER
typedef struct { const char *nm; const char *e; } GvRung;
static void gv_ladder(volatile u16 *pg){
    static const GvRung R[] = {
        { "A THOUSAND", "3" }, { "A MILLION", "6" }, { "A BILLION", "9" }, { "A TRILLION", "12" }, { "A QUADRILLION", "15" },
        { "A DECILLION", "33" }, { "A GOOGOL", "100" }, { "A VIGINTILLION", "63" }, { "A CENTILLION", "303" }, { "A MILLINILLION", "3003" },
        { "A MILLION-ILLION", "3000003" }, { "10^(10^12)", "1000000000000" },
        { "A GOOGOLPLEX", "1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000" },
        { "BUGSTARILLION V2 (APPROX)", BIG_E },
    };
    const int NR = (int)(sizeof R / sizeof R[0]) + 1;
    static char out[6000], pl[4100]; static char lines[40][60];
    int sel = 0, dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); ui_bg(pg, 0, 160); ui_panel(pg, 4, 4, 232, 143); ui_head(pg, 9, 8, 222, "THE SCALE LADDER", C_GOLD);
            int top = sel - 4; if(top > NR - 9) top = NR - 9; if(top < 0) top = 0;
            for(int r = 0; r < 9 && top + r < NR; r++){ int i = top + r, y = 19 + r * 8; if(i == sel) rect(pg, 7, y - 1, 220, 9, 6);
                text(pg, 12, y, i < NR - 1 ? R[i].nm : "BUGSTARILLION V3 (GIANT)", i == sel ? C_WHITE : C_GREY, 1);
                char b[40]; if(i < NR - 1){ b[0] = 0; scat(b, "10^"); scat(b, textlen(R[i].e) > 12 ? "(...)" : R[i].e); } else scpy(b, "10^(3 X 2^(2^32))"); rtext(pg, 224, y, b, i == sel ? C_GOLD : C_DGREY); }
            rect(pg, 8, 93, 224, 1, U_EDGE);
            if(sel < NR - 1){ gull pll; gv_say_ten(R[sel].e, out, sizeof out, pl, &pll); int n = gv_wrap_long(out, 54, lines, 6); char b[64]; b[0] = 0; scat(b, "NAME: "); gv_comma(b, pll ? pll : 3); scat(b, " LETTERS"); text(pg, 9, 97, b, C_CYAN, 1);
                char dd[220]; scpy(dd, "DIGITS: "); { int L = (int)strlen(R[sel].e); if(L < 19){ gv_comma(dd, strtoull(R[sel].e, 0, 10) + 1); } else { scat(dd, "10^"); ncat(dd, L - 1); scat(dd, " (A "); ncat(dd, L); scat(dd, "-DIGIT NUMBER)"); } } text(pg, 9, 104, dd, C_WHITE, 1);
                for(int i = 0; i < n && i < 4; i++) text(pg, 9, 112 + i * 7, lines[i], C_GREY, 1); }
            else { text(pg, 9, 97, "SEE BUGSTARILLION V3 IN THE LAB: THE NAME IS", C_CYAN, 1); text(pg, 9, 104, "ABOUT 6 QUINTILLION LETTERS, READ LIVE.", C_WHITE, 1);
                if(gv_ready(GV_V3)){ char t[60]; int g = gv3_read(0, 54, t); t[g] = 0; text(pg, 9, 112, t, C_GOLD, 1); } else text(pg, 9, 112, "(NEEDS THE 2^(2^32) FILE: SEE GIANT NUMBERS)", C_RED, 1); }
            ui_foot(pg, "UP DN: RUNG", "B: BACK"); }
        vsync(); read_keys(); if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_UP)){ sel = (sel + NR - 1) % NR; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ sel = (sel + 1) % NR; dirty = 1; sfx(7); }
    }
    wait_release();
}

// ================================================================= THE HUB (the PC LAB's GIANT NUMBERS)
static void pc_giant_hub(volatile u16 *pg){
    enum { H_POW, H_COUNT, H_V3, H_NAME, H_LADDER, H_N };
    static const char *const nm[H_N] = { "2 TO THE POWER 2^32", "THE COUNTING NUMBER", "BUGSTARILLION V3: THE NAME", "NAME ANY NUMBER", "THE SCALE LADDER" };
    static const char *const ds[H_N] = { "EVERY STATE OF 512 MIB OF MEMORY: 1,292,913,987 DIGITS, EVERY ONE READABLE. SCROLL, JUMP, WRITE IT TO A FILE.",
        "1 2 3 ... 123,456,790 WRITTEN IN A ROW: 1,000,000,008 DIGITS, WORKED OUT LIVE.",
        "THE WHOLE NAME OF THE BIGGEST NUMBER IN THE GAME, SAID LETTER BY LETTER: ABOUT SIX QUINTILLION LETTERS.",
        "TYPE ANY NUMBER, OR A POWER OF TEN, AND READ ITS NAME PIECE BY PIECE.", "FROM A THOUSAND UP TO THE V3 NAME, EACH WITH ITS NAME AND LENGTH." };
    int sel = 0, dirty = 1; wait_release();
    while(1){
        if(dirty){ dirty = 0; oam_hide_all(); oam_flush(); ui_bg(pg, 0, 160); ui_panel(pg, 14, 10, 212, 132); ui_head(pg, 20, 15, 200, "GIANT NUMBERS", C_GOLD);
            for(int i = 0; i < H_N; i++){ int y = 29 + i * 12; if(i == sel) rect(pg, 20, y - 2, 200, 11, 6); text(pg, 26, y, nm[i], i == sel ? C_WHITE : C_GREY, 1);
                if(i == H_POW || i == H_V3){ int ok = gi_open(GI_POW) == 0; rtext(pg, 214, y, ok ? "READY" : "NOT BUILT", ok ? C_GREEN : C_RED); } }
            { char ls[5][60]; int k = pc_lw_wrap(ds[sel], 50, ls, 5); for(int i = 0; i < k; i++) text(pg, 20, 94 + i * 7, ls[i], C_GREY, 1); }
            ui_foot(pg, "UP/DOWN: PICK  A: OPEN", "B: BACK"); }
        vsync(); read_keys(); if(HIT(K_B) || HIT(K_START)) break;
        if(HIT(K_UP)){ sel = (sel + H_N - 1) % H_N; dirty = 1; sfx(7); } if(HIT(K_DOWN)){ sel = (sel + 1) % H_N; dirty = 1; sfx(7); }
        if(HIT(K_A)){ sfx(2);
            if(sel == H_POW) gv_view(pg, GV_POW, 0); else if(sel == H_COUNT) gv_view(pg, GV_COUNT, 0); else if(sel == H_V3){ if(!gv_ready(GV_V3)) gv_missing_screen(pg, GV_V3); if(gv_ready(GV_V3)) gv_view(pg, GV_V3, 0); }
            else if(sel == H_NAME) gv_namer(pg); else gv_ladder(pg);
            dirty = 1; wait_release(); }
    }
    wait_release();
}
