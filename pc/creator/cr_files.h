// cr_files.h: recipes and share codes as files next to the .sav (plain C: stdio and dirent only)
//   <sav without .sav>.<NAME>.crp          a recipe you saved (spaces and odd marks in the name become _ and -)
//   <sav without .sav>.seed_<HEX16>.crp    the recipe of a planet you landed on (the game finds it again from the planet seed)
//   <sav without .sav>.code                one line: a share code to import (and the one that was last shown)
#ifndef CR_FILES_H
#define CR_FILES_H
#include "cr_recipe.h"
#define CR_LIBMAX 40
void cr_lib_base(const char *sav, char *out, int n);                       // the part of the path before .sav (sav may be NULL: "bugstar")
int  cr_lib_list(const char *sav, char names[][CR_NAME + 1], int max);     // file names found (as stored), sorted
int  cr_lib_save(const char *sav, const CrRecipe *r);
int  cr_lib_load(const char *sav, const char *name, CrRecipe *r);
int  cr_lib_save_seed(const char *sav, const CrRecipe *r);
int  cr_lib_find_seed(const char *sav, uint64_t seed, CrRecipe *r);
int  cr_code_write(const char *sav, const char *code);
int  cr_code_read(const char *sav, char *code, int n);                     // from the .code file, or from the environment variable PC_CREATOR_CODE
#endif
