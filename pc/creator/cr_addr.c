// cr_addr.c: a created planet as an address in the game's address space (needs pc_content.h, so it lives apart from cr_recipe.c)
#include <string.h>
#include "cr_addr.h"
#include "../content/pc_content.h"
void cr_addr(const CrRecipe *r, Addr *a){
    memset(a, 0, sizeof *a); cr_uni4(r, a->uni);
}
int cr_addr_is_created(const Addr *a){ return cr_uni4_is(a->uni); }
