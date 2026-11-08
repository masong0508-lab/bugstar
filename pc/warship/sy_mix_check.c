// sy_mix_check.c: the rooms and props a ship design asks of a deck, worked out on their own, for tests/walk.sh.
//   sy_mix_check foe SEED TEAM LEVEL   the design sy_foe makes for this craft seed      sy_mix_check preset N   the ship editor's preset N      sy_mix_check default
// prints: bunks=B fighters=F crates=C guns=G hangar=H quarters=Q turret=T
#include "../shipyard/sy_design.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv){
    SyDesign d; sy_default(&d);
    if(argc >= 5 && !strcmp(argv[1], "foe")) sy_foe(&d, (uint32_t)strtoul(argv[2], 0, 10), atoi(argv[3]), atoi(argv[4]));
    else if(argc >= 3 && !strcmp(argv[1], "preset")) sy_preset(&d, atoi(argv[2]));
    SyRooms r; sy_rooms(&d, &r);
    printf("bunks=%d fighters=%d crates=%d guns=%d hangar=%d quarters=%d turret=%d\n", r.bunks, r.fighters, r.crates, r.guns, r.hangar, r.quarters, r.turret); return 0;
}
