// pc/tests/cn_check.inc: compares the game's own number code (coilnum.inc) with the Python reference (pc/tests/coilref.py FILE). Used by the host test
// (pc/tests/cn_host.c) and by the game's PC self-test (pc/coil_test.inc). cnc_run(file) returns how many lines differ (or -1 if the file is missing).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int cnc_parse(const char *s, u64 *m, int *e){               // "12345e6", "5.391247e-44", "0d42" (d is the decimal point in names), "1e-60"
    u64 v=0; int fr=0, seen=0, ex=0, neg=0; const char *p=s;
    for(;*p && *p!='e';p++){ if(*p=='.'||*p=='d'){ seen=1; continue; } if(*p>='0'&&*p<='9'){ v=v*10+(u64)(*p-'0'); if(seen) fr++; } }
    if(*p=='e'){ p++; if(*p=='-'){ neg=1; p++; } while(*p>='0'&&*p<='9'){ ex=ex*10+(*p-'0'); p++; } }
    *m=v; *e=(neg?-ex:ex)-fr; return 1; }
static CS cnc_cs(const char *s){ u64 m; int e; cnc_parse(s,&m,&e); return cs_make(m,e); }
static int cnc_idx(const char *s, const char *const *names, int n){ for(int i=0;i<n;i++) if(!strcmp(s,names[i])) return i; return 0; }
static int cnc_run(const char *path, int *total){
    FILE *f=fopen(path,"r"); if(!f) return -1;
    static const char *const rn[CR_N]={"EARTH","STAR","MW","GAL","UNI"}, *const tn[CT_N]={"MOON","SUN","PROX","MW","AND","UNI","GAME"};
    char ln[300]; int n=0, bad=0;
    while(fgets(ln,sizeof ln,f)){
        char *sep=strstr(ln," => "); if(!sep) continue; *sep=0; char *want=sep+4; want[strcspn(want,"\r\n")]=0;
        char k[16], a1[48], a2[48]; a1[0]=a2[0]=0; sscanf(ln,"%15s %47s %47s",k,a1,a2);
        char got[200]; got[0]=0; u64 m; int e;
        if(!strcmp(k,"WFMT")){ cnc_parse(a1,&m,&e); cn_wfmt(got,cw_make(m,e)); }
        else if(!strcmp(k,"WORDS")){ cnc_parse(a1,&m,&e); cn_words(got,cs_from_cw(cw_make(m,e))); }
        else if(!strcmp(k,"KMPS")){ cnc_parse(a1,&m,&e); CS c = strchr(a1,'d') ? cs_make(m,e) : cs_from_cw(cw_make(m,e)); cn_sn(got,cn_kmps(c)); }
        else if(!strcmp(k,"RATE")||!strcmp(k,"TRIP")){ cnc_parse(a2,&m,&e); CS c = strchr(a2,'d') ? cs_make(m,e) : cs_from_cw(cw_make(m,e));
            if(k[0]=='R') cn_rate_text(got,cnc_idx(a1,rn,CR_N),c); else cn_trip_text(got,cnc_idx(a1,tn,CT_N),c); }
        else if(!strcmp(k,"TIME")) cn_time(got,cnc_cs(a1));
        else if(k[0]=='O' && k[1]=='P'){                                  // arithmetic: the answer's first 8 digits must agree (the game keeps 10)
            u64 m2; int e2; CW a, b, r; cnc_parse(a1,&m,&e); a=cw_make(m,e);
            if(!strcmp(k,"OPADD")||!strcmp(k,"OPSUB")){ cnc_parse(a2,&m2,&e2); b=cw_make(m2,e2); r = k[2]=='A' ? cw_add(a,b) : cw_sub(a,b); }
            else { u32 q=(u32)strtoul(a2,0,10); r = k[2]=='M' ? cw_mulu(a,q) : cw_divu(a,q); }
            cn_wfmt(got,r);
            { u64 mm=0; int nd=(int)strlen(want), use = nd<19?nd:19; for(int i=0;i<use;i++) mm=mm*10+(u64)(want[i]-'0');
              CS sx=cs_from_cw(cw_make(mm,nd-use)), sr=cs_from_cw(r); u64 x8=sx.m/100, r8=sr.m/100;
              int ok = (sx.m==0 && sr.m==0) || (sx.e==sr.e && (x8==r8 || x8+1==r8 || r8+1==x8)) || (sx.e+1==sr.e && x8>=99999999 && r8<=10000001) || (sr.e+1==sx.e && r8>=99999999 && x8<=10000001);
              if(ok) scpy(got,want); } }
        else continue;
        n++; if(strcmp(got,want)){ if(bad<12) printf("MISMATCH %s %s %s\n   want: %s\n   got:  %s\n",k,a1,a2,want,got); bad++; } }
    fclose(f); if(total) *total=n; return bad; }
