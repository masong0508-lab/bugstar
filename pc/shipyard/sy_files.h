// sy_files.h: ship designs as small files next to the .sav (plain C: stdio and dirent only)
//   <sav without .sav>.slot<N>.ship      the design you fly in save slot N (a recipe of a few dozen bytes; no file = the game's own ship)
//   <sav without .sav>.<NAME>.shp        a design you saved in the editor's FILE tab
//   <sav without .sav>.shipcode          one line: a share code to import (and the last one made)
#ifndef SY_FILES_H
#define SY_FILES_H
#include "sy_design.h"
#define SY_LIBMAX 40
int  sy_write(const char *path, const SyDesign *d);
int  sy_read(const char *path, SyDesign *d);
int  sy_slot_save(const char *sav, int slot, const SyDesign *d);
int  sy_slot_load(const char *sav, int slot, SyDesign *d);                  // 0 = no design for this slot (old saves)
void sy_slot_erase(const char *sav, int slot);
void sy_slot_copy(const char *sav, int from, int to);
int  sy_lib_save(const char *sav, const SyDesign *d);
int  sy_lib_load(const char *sav, const char *name, SyDesign *d);
int  sy_lib_list(const char *sav, char names[][SY_NAME + 1], int max);
int  sy_code_write(const char *sav, const char *code);
int  sy_code_read(const char *sav, char *code, int n);                      // from the .shipcode file, or the environment variable PC_SHIP_CODE
#endif
