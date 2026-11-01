// sy_files.c: see sy_files.h
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "sy_files.h"
static void base(const char *sav, char *out, int n){ snprintf(out, n, "%s", sav && sav[0] ? sav : "bugstar"); int L = (int)strlen(out); if(L > 4 && !strcmp(out + L - 4, ".sav")) out[L - 4] = 0; }
int sy_write(const char *path, const SyDesign *d){ uint8_t b[SY_RECIPE_MAX]; int n = sy_encode(d, b, sizeof b); if(!n) return 0; FILE *f = fopen(path, "wb"); if(!f) return 0; int ok = (int)fwrite(b, 1, n, f) == n; fclose(f); return ok; }
int sy_read(const char *path, SyDesign *d){ uint8_t b[SY_RECIPE_MAX + 1]; FILE *f = fopen(path, "rb"); if(!f) return 0; int n = (int)fread(b, 1, sizeof b, f); fclose(f); return n <= SY_RECIPE_MAX && sy_decode(b, n, d); }
static void slot_path(const char *sav, int slot, char *p, int n){ char b[400]; base(sav, b, sizeof b); snprintf(p, n, "%s.slot%d.ship", b, slot); }
int sy_slot_save(const char *sav, int slot, const SyDesign *d){ char p[500]; slot_path(sav, slot, p, sizeof p); return sy_write(p, d); }
int sy_slot_load(const char *sav, int slot, SyDesign *d){ char p[500]; slot_path(sav, slot, p, sizeof p); return sy_read(p, d); }
void sy_slot_erase(const char *sav, int slot){ char p[500]; slot_path(sav, slot, p, sizeof p); remove(p); }
void sy_slot_copy(const char *sav, int from, int to){ SyDesign d; if(sy_slot_load(sav, from, &d)) sy_slot_save(sav, to, &d); else sy_slot_erase(sav, to); }
static void clean(const char *name, char *out, int n){ int k = 0; for(; name[k] && k < n - 1; k++){ char c = name[k]; out[k] = c == ' ' ? '_' : (c == '.' || c == '\'') ? '-' : c; } out[k] = 0; }
static void lib_path(const char *sav, const char *name, char *p, int n){ char b[400], c[64]; base(sav, b, sizeof b); clean(name, c, sizeof c); snprintf(p, n, "%s.%s.shp", b, c); }
int sy_lib_save(const char *sav, const SyDesign *d){ char p[500]; lib_path(sav, d->name, p, sizeof p); return sy_write(p, d); }
int sy_lib_load(const char *sav, const char *name, SyDesign *d){ char p[500]; lib_path(sav, name, p, sizeof p); return sy_read(p, d); }
static int cmp(const void *a, const void *b){ return strcmp((const char*)a, (const char*)b); }
int sy_lib_list(const char *sav, char names[][SY_NAME + 1], int max){
    char b[400], dir[400], pre[200]; base(sav, b, sizeof b);
    const char *sl = strrchr(b, '/'); if(sl){ int L = (int)(sl - b); memcpy(dir, b, L); dir[L] = 0; snprintf(pre, sizeof pre, "%s.", sl + 1); } else { snprintf(dir, sizeof dir, "."); snprintf(pre, sizeof pre, "%s.", b); }
    DIR *d = opendir(dir); if(!d) return 0; int n = 0; struct dirent *e; size_t pl = strlen(pre);
    while((e = readdir(d)) && n < max){ const char *f = e->d_name; size_t L = strlen(f); if(L < pl + 5 || strncmp(f, pre, pl) || strcmp(f + L - 4, ".shp")) continue;
        char stem[200]; snprintf(stem, sizeof stem, "%.*s", (int)(L - pl - 4), f + pl); for(char *c = stem; *c; c++) if(*c == '_') *c = ' ';
        snprintf(names[n++], SY_NAME + 1, "%.12s", stem); }
    closedir(d); qsort(names, n, SY_NAME + 1, cmp); return n;
}
int sy_code_write(const char *sav, const char *code){ char b[400], p[420]; base(sav, b, sizeof b); snprintf(p, sizeof p, "%s.shipcode", b); FILE *f = fopen(p, "w"); if(!f) return 0; fprintf(f, "%s\n", code); fclose(f); return 1; }
int sy_code_read(const char *sav, char *code, int n){
    char b[400], p[420]; base(sav, b, sizeof b); snprintf(p, sizeof p, "%s.shipcode", b); FILE *f = fopen(p, "r");
    if(f){ if(fgets(code, n, f)){ fclose(f); size_t L = strlen(code); while(L && (code[L - 1] == '\n' || code[L - 1] == '\r' || code[L - 1] == ' ')) code[--L] = 0; if(L) return 1; } else fclose(f); }
    const char *e = getenv("PC_SHIP_CODE"); if(e && e[0]){ snprintf(code, n, "%s", e); return 1; }
    return 0;
}
