// crcode.c: prints the share code of an example planet with a few fields changed: crcode EXAMPLE CLASS SEA SIZE XSIZE   (used by tests/creator.sh)
#include <stdio.h>
#include <stdlib.h>
#include "../creator/cr_recipe.h"
int main(int argc, char **argv){
    CrRecipe r; cr_default(&r, (uint32_t)atoi(argc > 1 ? argv[1] : "0"));
    if(argc > 2) r.cls = (uint8_t)atoi(argv[2]); if(argc > 3) r.sea = (uint8_t)atoi(argv[3]); if(argc > 4) r.size = (uint8_t)atoi(argv[4]); if(argc > 5) r.xsize = (uint8_t)atoi(argv[5]);
    cr_fix(&r); char c[400]; if(!cr_encode(&r, c, sizeof c)) return 1; puts(c); return 0; }
