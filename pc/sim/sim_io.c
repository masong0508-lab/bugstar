// sim_io.c: file and environment access for the side game. See sim_io.h.
#include "sim_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int path(char *out, size_t n, const char *suffix){
    const char *s = pc_sav_path(); if(!s || strlen(s) + strlen(suffix) + 1 > n) return 0;
    strcpy(out, s); strcat(out, suffix); return 1;
}
size_t sim_io_load(uint8_t *buf, size_t cap){
    char p[1024]; if(!path(p, sizeof p, ".sim")) return 0;
    FILE *f = fopen(p, "rb"); if(!f) return 0;
    size_t n = fread(buf, 1, cap, f); fclose(f); return n;
}
int sim_io_store(const uint8_t *buf, size_t n){
    char p[1024], t[1040]; if(!path(p, sizeof p, ".sim")) return -1;
    strcpy(t, p); strcat(t, ".tmp");
    FILE *f = fopen(t, "wb"); if(!f) return -1;
    size_t w = fwrite(buf, 1, n, f); int bad = fclose(f) || w != n;
    if(bad){ remove(t); return -1; }
    return rename(t, p);
}
const char *sim_io_env(const char *name){ return getenv(name); }
void sim_io_append(const char *p, const char *text){ FILE *f = fopen(p, "ab"); if(!f) return; fputs(text, f); fclose(f); }
