// cr_addr.c: a created planet as an address in the game's address space (needs pc_content.h, so it lives apart from cr_recipe.c)
#include <string.h>
#include "cr_addr.h"
#include "../content/pc_content.h"
void cr_addr(const CrRecipe *r, Addr *a){
    uint64_t s = cr_seed(r); memset(a, 0, sizeof *a);
    a->uni[0] = CR_UNI_MARK; a->uni[1] = CR_UNI_MARK ^ 0x5EEDC0DEu ^ (uint32_t)(s >> 32) ^ (uint32_t)s; a->uni[2] = (uint32_t)(s >> 32); a->uni[3] = (uint32_t)s;
}
int cr_addr_is_created(const Addr *a){ return a->uni[0] == CR_UNI_MARK && a->uni[1] == (CR_UNI_MARK ^ 0x5EEDC0DEu ^ a->uni[2] ^ a->uni[3]); }
