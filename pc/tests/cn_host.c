// pc/tests/cn_host.c: the wide-number code on its own, compared with the Python reference. Build and run: see coil.sh (cc cn_host.c -o cn_host && ./cn_host ref.txt)
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint64_t u64; typedef uint32_t u32; typedef int64_t s64;
static void scpy(char *d, const char *s){ while(*s) *d++=*s++; *d=0; }
static void scat(char *d, const char *s){ while(*d) d++; scpy(d,s); }
static int textlen(const char *s){ return (int)strlen(s); }
static const char *const big_nm[22]={"","THOUSAND","MILLION","BILLION","TRILLION","QUADRILLION","QUINTILLION","SEXTILLION","SEPTILLION","OCTILLION","NONILLION","DECILLION",
    "UNDECILLION","DUODECILLION","TREDECILLION","QUATTUORDECILLION","QUINDECILLION","SEXDECILLION","SEPTENDECILLION","OCTODECILLION","NOVEMDECILLION","VIGINTILLION"};
static const char *illion(int g){ return big_nm[g<0?0:g>21?21:g]; }
#include "../../coilnum.inc"
#include "cn_check.inc"
int main(int argc, char **argv){ int total=0; int bad=cnc_run(argc>1?argv[1]:"ref.txt",&total); printf("coilnum vs Python: %d cases, %d differ\n",total,bad); return bad!=0; }
