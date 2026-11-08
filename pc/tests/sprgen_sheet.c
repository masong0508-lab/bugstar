// sprgen_sheet.c: contact sheets of the procedural sprites (pc/pc_sprgen*.c). usage: sprgen_sheet GEN FIRST COLS ROWS SIZE out.ppm [bg]
// GEN is the number of the generator in sg_gens (see pc_sprgen_gal.c), ids FIRST, FIRST+1, ... Each cell is SIZE x SIZE (or the generator's own shape), on a pale checker.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../pc_sprgen.h"
int main(int argc, char **argv){
    if(argc < 7){ fprintf(stderr, "usage: %s GEN FIRST COLS ROWS SIZE out.ppm [bg 0xRRGGBB]\n", argv[0]); for(int i = 0; i < sg_ngens; i++) fprintf(stderr, "  %d %s (%u ids)\n", i, sg_gens[i].name, sg_gens[i].nids); return 1; }
    int gen = atoi(argv[1]); unsigned first = (unsigned)strtoul(argv[2], 0, 0); int cols = atoi(argv[3]), rows = atoi(argv[4]), size = atoi(argv[5]); uint32_t bg = argc > 7 ? (uint32_t)strtoul(argv[7], 0, 0) : 0x3A3F4A;
    if(gen < 0 || gen >= sg_ngens) return 2; const SgGen *g = &sg_gens[gen]; int cw = size, ch = size * g->h / g->w, pad = 2; int W = cols * (cw + pad) + pad, H = rows * (ch + pad) + pad;
    uint32_t *out = malloc((size_t)W * H * 4); for(int i = 0; i < W * H; i++) out[i] = bg;
    for(int i = 0; i < cols * rows; i++){ SgImg *im = sg_img_new(cw, ch); g->draw(im, first + i); int ox = pad + (i % cols) * (cw + pad), oy = pad + (i / cols) * (ch + pad);
        SgBlit b = { (float)ox, (float)oy, (float)cw, (float)ch, 0, 0, 0, 0, 0 }; sg_blit(out, W, H, im, &b); sg_img_free(im); }
    FILE *f = fopen(argv[6], "wb"); fprintf(f, "P6\n%d %d\n255\n", W, H); for(int i = 0; i < W * H; i++){ fputc(out[i] >> 16 & 255, f); fputc(out[i] >> 8 & 255, f); fputc(out[i] & 255, f); } fclose(f); free(out); return 0; }
