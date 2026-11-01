// cr_addr.h: see cr_addr.c
#ifndef CR_ADDR_H
#define CR_ADDR_H
#include "cr_recipe.h"
#include "../content/pc_content.h"
void cr_addr(const CrRecipe *r, Addr *a);          // the address of a created planet: universe words 0..3 hold a marker and the recipe seed
int  cr_addr_is_created(const Addr *a);          // 1 when an address is one of these (addr_seed() then returns the recipe seed itself)
#endif
