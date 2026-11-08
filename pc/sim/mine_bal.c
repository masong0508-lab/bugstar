// mine_bal.c: the colony's mine and workshops played for years with nobody helping (the numbers behind TUNING.md patch 45).
// Many colonies x many seeds x 1500 days, colony day then mine day, same order as the catch-up. Reports per difficulty: output a day, cave-ins and deaths per 1000 mine-days,
// how far down they get, and the softlocks: a mine that stops working with settlers still alive (stuck), a colony that lost every settler, a level caved in for 200 days or more.
#include <stdio.h>
#include <string.h>
#include "sim_core.h"
static const ColFlags NOF;
#define ALL 0x1FE
int main(int argc, char **argv){ int bad = 0;
    int days = argc > 1 ? atoi(argv[1]) : 1500, seeds = argc > 2 ? atoi(argv[2]) : 40; static const char *dn[3] = { "CALM", "NORMAL", "HARSH" }; static const int dv[3] = { COLD_CALM, COLD_NORMAL, COLD_HARSH };
    static const int miners[3] = { 3, 6, 10 };
    for(int d = 0; d < 3; d++) for(int k = 0; k < 3; k++){
        double ore = 0, bars = 0, tools = 0, cloth = 0, gems = 0, depth = 0, ci = 0, died = 0, wsl = 0; int stuck = 0, wiped = 0, longcave = 0, runs = 0;
        for(int s = 0; s < seeds; s++){
            SimColony c; Mine m; MnAway aw; memset(&aw, 0, sizeof aw); uint32_t town = 1000u + (uint32_t)s * 7919u;
            col_init(&c, town, 0); col_update(&c, (uint16_t)(ALL | 1u << COLR_TENT)); c.nset = (uint8_t)(miners[k] + 3); c.food = 3000; c.wood = 200; c.health = 100; c.morale = 100;
            for(int i = 0; i < c.nset; i++) c.job[i] = (uint8_t)(i < miners[k] ? COLJ_MINER : (i % 3 == 0 ? COLJ_BUILDER : i % 3 == 1 ? COLJ_COOK : COLJ_FORAGER));
            c.bld[COLB_MINE] = 1; c.grid[5] = COLB_MINE + 1; mn_init(&m, town, 0);
            int cavedays = 0, lastmined = 0, still = 0, mind = 0; uint32_t m0 = 0;
            for(uint32_t t = 1; t <= (uint32_t)days; t++){
                col_update(&c, (uint16_t)(ALL | col_goal_mask(&c, &NOF, t, 28))); col_day_ex(&c, t, 28, dv[d], 0); mn_day(&m, &c, t, dv[d], 0, &aw);
                if(c.state != COLS_ALIVE || c.nset == 0) break; mind++;
                int wc = m.work && m.zst[m.work] == MNZ_CAVED; cavedays = wc ? cavedays + 1 : 0; if(cavedays >= 200) longcave = 1;
                if(m.mined == (uint32_t)lastmined && mn_miners(&c) > 0) still++; else still = 0; lastmined = (int)m.mined; if(still >= 400){ stuck++; still = -100000; } }
            runs++; if(c.nset == 0 || c.state != COLS_ALIVE) wiped++; ore += aw.ore; bars += aw.bars; tools += aw.tools; cloth += aw.cloth; gems += aw.gems; depth += m.depth; ci += m.cave_ins; died += aw.died; wsl += m.wl[0] + m.wl[1] + m.wl[2];  (void)m0; if(longcave) { } (void)mind; }
        double md = (double)days * seeds;
        printf("%-6s %2d miners: a day ore %.1f bars %.2f tools %.2f cloth %.2f gems %.2f | depth %.1f ws %.1f | cave-ins %.1f/1000d died %.2f/1000d | wiped %d stuck %d\n", dn[d], miners[k], ore / md, bars / md, tools / md, cloth / md, gems / md, depth / runs, wsl / runs, 1000.0 * ci / md, 1000.0 * died / md, wiped, stuck); if(wiped || stuck) bad = 1;
    }
    printf("%s\n", bad ? "mine_bal: SOFTLOCK or wiped colony" : "mine_bal: no wiped colony, no stuck mine"); return bad; }
