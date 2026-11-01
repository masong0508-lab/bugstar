// sim_io.h: the side file and the test hooks, in their own C file (so the game's code never sees stdio)
#ifndef SIM_IO_H
#define SIM_IO_H
#include <stdint.h>
#include <stddef.h>
const char *pc_sav_path(void);                                           // pc_hw.c: the .sav file name (0 if none)
size_t sim_io_load(uint8_t *buf, size_t cap);                            // reads <sav>.sim; 0 if there is none
int    sim_io_store(const uint8_t *buf, size_t n);                       // writes <sav>.sim (through a temp file); 0 ok
const char *sim_io_env(const char *name);                                // getenv
void   sim_io_append(const char *path, const char *text);                // appends text to a file (test dumps)
#endif
