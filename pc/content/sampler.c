// sampler.c: print a few random worlds from the huge address space: universe themes, planet names and traits. ./sampler [count] [seed]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc_content.h"
static uint64_t rs;
static uint32_t r32(void){ rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return (uint32_t)(rs >> 8); }
int main(int argc, char **argv){
    int n = argc > 1 ? atoi(argv[1]) : 5; rs = argc > 2 ? strtoull(argv[2], 0, 10) | 1 : 88172645463325252ull;
    for(int i = 0; i < n; i++){
        Addr a; PlanetInfo p; char nm[32]; int th[2];
        memset(&a, 0, sizeof a);
        for(int k = 0; k < UNI_WORDS; k++) a.uni[k] = r32();
        for(int k = 0; k < AX_WORDS; k++){ a.cx[k] = r32(); a.cy[k] = r32(); }
        a.star = r32() % 32; a.planet = r32() % 16;
        uint64_t seed = addr_seed(&a); universe_themes(uni_seed(a.uni), th); planet_make(seed, &p); planet_name(seed, nm);
        printf("universe ...%08X%08X  themes: %s + %s\n", a.uni[1], a.uni[0], theme_get(th[0])->name, theme_get(th[1])->name);
        printf("  planet %s  gravity %d%%  day %dh  temp %dC  danger %+d  loot %+d  life %+d\n", nm, p.gravity_pct, p.day_hours, p.temp_c, p.danger, p.loot, p.life);
        printf("  traits:"); for(int k = 0; k < p.n; k++) printf(" %s%s", trait_name(p.id[k]), k + 1 < p.n ? "," : "\n");
    }
    return 0;
}
