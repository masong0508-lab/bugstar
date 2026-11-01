// cr_files.c: see cr_files.h
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include "cr_files.h"
void cr_lib_base(const char *sav, char *out, int n){
    snprintf(out, n, "%s", sav && sav[0] ? sav : "bugstar"); int L = (int)strlen(out);
    if(L > 4 && !strcmp(out + L - 4, ".sav")) out[L - 4] = 0;
}
static void clean(const char *name, char *out, int n){ int k = 0; for(; name[k] && k < n - 1; k++){ char c = name[k]; out[k] = c == ' ' ? '_' : (c == '.' || c == '\'') ? '-' : c; } out[k] = 0; }
static void path_of(const char *sav, const char *stem, char *out, int n){ char b[400], c[64]; cr_lib_base(sav, b, sizeof b); clean(stem, c, sizeof c); snprintf(out, n, "%s.%s.crp", b, c); }
int cr_lib_save(const char *sav, const CrRecipe *r){ char p[500]; path_of(sav, r->name[0] ? r->name : "NEW_WORLD", p, sizeof p); return cr_save(r, p); }
int cr_lib_load(const char *sav, const char *name, CrRecipe *r){ char p[500]; path_of(sav, name, p, sizeof p); return cr_load(r, p); }
int cr_lib_save_seed(const char *sav, const CrRecipe *r){ char p[500], s[40]; snprintf(s, sizeof s, "seed_%016llX", (unsigned long long)cr_seed(r)); path_of(sav, s, p, sizeof p); return cr_save(r, p); }
int cr_lib_find_seed(const char *sav, uint64_t seed, CrRecipe *r){
    char p[500], s[40]; snprintf(s, sizeof s, "seed_%016llX", (unsigned long long)seed); path_of(sav, s, p, sizeof p);
    return cr_load(r, p) && cr_seed(r) == seed;
}
static int cmp(const void *a, const void *b){ return strcmp((const char*)a, (const char*)b); }
int cr_lib_list(const char *sav, char names[][CR_NAME + 1], int max){
    char base[400], dir[400], pre[200]; cr_lib_base(sav, base, sizeof base);
    const char *slash = strrchr(base, '/'); if(slash){ int L = (int)(slash - base); memcpy(dir, base, L); dir[L] = 0; snprintf(pre, sizeof pre, "%s.", slash + 1); } else { snprintf(dir, sizeof dir, "."); snprintf(pre, sizeof pre, "%s.", base); }
    DIR *d = opendir(dir); if(!d) return 0; int n = 0; struct dirent *e; size_t pl = strlen(pre);
    while((e = readdir(d)) && n < max){ const char *f = e->d_name; size_t L = strlen(f); if(L < pl + 5 || strncmp(f, pre, pl) || strcmp(f + L - 4, ".crp")) continue;
        char stem[200]; snprintf(stem, sizeof stem, "%.*s", (int)(L - pl - 4), f + pl); if(!strncmp(stem, "seed_", 5)) continue;
        for(char *c = stem; *c; c++) if(*c == '_') *c = ' ';
        snprintf(names[n++], CR_NAME + 1, "%.16s", stem); }
    closedir(d); qsort(names, n, CR_NAME + 1, cmp); return n;
}
int cr_code_write(const char *sav, const char *code){ char b[400], p[420]; cr_lib_base(sav, b, sizeof b); snprintf(p, sizeof p, "%s.code", b); FILE *f = fopen(p, "w"); if(!f) return 0; fprintf(f, "%s\n", code); fclose(f); return 1; }
int cr_code_read(const char *sav, char *code, int n){
    char b[400], p[420]; cr_lib_base(sav, b, sizeof b); snprintf(p, sizeof p, "%s.code", b); FILE *f = fopen(p, "r");
    if(f){ if(fgets(code, n, f)){ fclose(f); size_t L = strlen(code); while(L && (code[L - 1] == '\n' || code[L - 1] == '\r' || code[L - 1] == ' ')) code[--L] = 0; return L > 0; } fclose(f); }
    const char *e = getenv("PC_CREATOR_CODE"); if(e && e[0]){ snprintf(code, n, "%s", e); return 1; }
    return 0;
}
