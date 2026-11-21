/* Independent UDP sockets: preflight Ready, no premature combat,
 * explicit dual confirmation, BYE and clean reconnect/Ready reset.
 * Build on POSIX with pc_net.c, pc_duel.c, pc_coop.c and net/session.c.
 */
#include "../pc/platform/pc_net.h"
#include <assert.h>
#include <stdio.h>

static void step_both(PcNetSession *host,PcNetSession *guest,
                      const PcNetShip *h,const PcNetShip *g,unsigned start,
                      unsigned finish){
    for(unsigned t=start;t<=finish;t++){
        pcnet_step(guest,t,g);
        pcnet_step(host,t,h);
        pcnet_step(guest,t,g);
    }
}
int main(void){
    PcNetSession h,g;
    PcNetShip hs={0,0,0,0xA987u,0,0};
    PcNetShip gs={128,0,1720,0xA987u,0,0};
    assert(pcnet_host(&h,0));
    pcnet_set_coop(&h,1);
    pcnet_set_ready(&h,0);
    assert(pcnet_join(&g,"127.0.0.1",h.local_port));
    pcnet_set_coop(&g,1);
    pcnet_set_ready(&g,0);
    step_both(&h,&g,&hs,&gs,1,90);
    assert(h.connected && g.connected && h.received && g.received);
    assert(!pcnet_match_ready(&h) && !pcnet_match_ready(&g));
    pcnet_fire(&h);pcnet_fire(&g);
    step_both(&h,&g,&hs,&gs,91,110);
    assert(h.coop_enemy.phase==0 && h.local_shots==0 && g.local_shots==0);
    /* One side can confirm first. Neither engine advances combat yet. */
    pcnet_set_ready(&h,1);
    step_both(&h,&g,&hs,&gs,111,150);
    assert(g.peer_ready && !h.peer_ready);
    assert(!pcnet_match_ready(&h) && !pcnet_match_ready(&g));
    assert(h.coop_enemy.phase==0);
    pcnet_set_ready(&g,1);
    step_both(&h,&g,&hs,&gs,151,190);
    assert(pcnet_match_ready(&h) && pcnet_match_ready(&g));
    assert(h.coop_enemy.phase==1 && h.coop_enemy.hp==100);
    assert(g.coop_enemy.phase==1 && g.coop_enemy.hp==100);
    /* Leave is never a save write and must reopen the host for new peers.
     * A reconnection must not re-arm either manually confirmed pilot. */
    uint16_t port=h.local_port;
    pcnet_close(&g);
    pcnet_step(&h,191,&hs);
    assert(!h.connected && h.reconnecting && !h.local_ready && !h.peer_ready);
    assert(pcnet_join(&g,"127.0.0.1",port));
    pcnet_set_coop(&g,1);
    pcnet_set_ready(&g,0);
    step_both(&h,&g,&hs,&gs,192,280);
    assert(h.connected && g.connected && h.received && g.received);
    assert(!pcnet_match_ready(&h) && !pcnet_match_ready(&g));
    assert(h.coop_enemy.phase==0);
    pcnet_set_ready(&g,1);
    step_both(&h,&g,&hs,&gs,281,310);
    assert(!pcnet_match_ready(&h) && !pcnet_match_ready(&g));
    pcnet_set_ready(&h,1);
    step_both(&h,&g,&hs,&gs,311,340);
    assert(pcnet_match_ready(&h) && pcnet_match_ready(&g));
    assert(h.coop_enemy.phase==1);
    pcnet_close(&g);pcnet_close(&h);
    puts("LAN ready: two confirmations gate combat; BYE + reconnect requires Ready again PASS");
    return 0;
}
