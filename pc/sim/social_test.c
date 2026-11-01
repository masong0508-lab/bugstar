// social_test.c: Phase 3 (friends and letters). Plain C, no game.
#include <stdio.h>
#include <string.h>
#include "sim_core.h"
static int checks, fails;
#define CHECK(c) do{ checks++; if(!(c)){ fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } }while(0)
static Sim A, B; static uint8_t buf[SIMFILE_MAX]; static SimFile F, G;
int main(void){
    sim_init(&A, 42, 5, 0, 0);
    CHECK(sim_talk_to(&A, -1) == -1 && sim_talk_to(&A, 5) == -1);
    int v = sim_talk_to(&A, 2); CHECK(v >= 8 && v <= 12); CHECK(sim_talk_to(&A, 2) == -1);            // once a day
    CHECK(A.friend[2] == v && A.friend[1] == 0);
    int letters = 0, last = v; for(int d = 1; d < 30; d++){ sim_run(&A, SIM_DAY_TICKS); int r = sim_talk_to(&A, 2); CHECK(r >= last || r == -1); if(r > 0) last = r; }
    CHECK(A.friend[2] == 100);
    for(uint32_t i = 0; i < sim_log_count(&A); i++) if(sim_log_at(&A, i)->kind == SIME_LETTER) letters++;
    CHECK(letters == 4);                                                                                // 25, 50, 75, 100: one letter each, never twice
    char t[64]; int l = 0;
    for(uint32_t i = 0; i < sim_log_count(&A); i++){ const SimEvent *e = sim_log_at(&A, i); if(e->kind == SIME_LETTER || e->kind == SIME_FRIEND){ l = sim_event_text(&A, e, t, sizeof t); CHECK(l > 0 && l <= 52); } }
    // determinism: same talks, same hash
    sim_init(&A, 42, 5, 0, 0); sim_init(&B, 42, 5, 0, 0);
    for(int d = 0; d < 10; d++){ sim_talk_to(&A, d % 5); sim_talk_to(&B, d % 5); sim_run(&A, 2000); sim_run(&B, 2000); }
    CHECK(sim_hash(&A) == sim_hash(&B)); sim_talk_to(&B, 4); sim_init(&A, 42, 5, 0, 0); CHECK(sim_hash(&A) != sim_hash(&B));
    // save and load: the file keeps friends per town; a different town does not see them
    sim_init(&A, 42, 5, 0, 0); for(int d = 0; d < 6; d++){ sim_talk_to(&A, 1); sim_talk_to(&A, 3); sim_run(&A, SIM_DAY_TICKS); }
    simfile_init(&F); F.slot[0].used = 1; sim_save_friends(&A, &F.slot[0]); CHECK(F.slot[0].nfr == 2);
    size_t n = simfile_write(&F, buf, sizeof buf); CHECK(n > 0); CHECK(simfile_read(&G, buf, n) == 0); CHECK(G.slot[0].nfr == 2);
    sim_init(&B, 42, 5, 0, 0); sim_load_friends(&B, &G.slot[0]); CHECK(B.friend[1] == A.friend[1] && B.friend[3] == A.friend[3] && B.friend[0] == 0);
    sim_init(&B, 43, 5, 0, 0); sim_load_friends(&B, &G.slot[0]); CHECK(B.friend[1] == 0);
    sim_save_friends(&A, &G.slot[0]); CHECK(G.slot[0].nfr == 2);                                     // saving again does not duplicate
    { static uint8_t bad[SIMFILE_MAX]; memcpy(bad, buf, n); size_t at = 8; while(at + 14 <= n){ uint32_t len = bad[at+6] | (bad[at+7]<<8) | (bad[at+8]<<16) | ((uint32_t)bad[at+9]<<24); if(!memcmp(bad + at, "FRND", 4)) bad[at + 14] = 0xFF; at += 14 + len; }
      CHECK(simfile_read(&G, bad, n) == 0); CHECK(G.slot[0].nfr == 0); }                             // a wrong friend count is refused, nothing else is lost
    printf("social_test: %d checks, %d failed\n", checks, fails); return fails != 0;
}
