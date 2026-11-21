/* BUGSTAR experimental LAN presence: one remote ship, no authority over combat.
 * This file must also compile in the Android NDK and Windows MinGW builds.
 * Internet exposure is intentionally unsupported until auth/rollback exists.
 */
#include "pc_net.h"
#include "../../net/session.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <winsock2.h>
# include <ws2tcpip.h>
typedef SOCKET NetSock;
typedef int NetAddrLen; /* WinSock APIs take int* lengths, not POSIX socklen_t* */
# define NET_INVALID INVALID_SOCKET
# define net_close_socket closesocket
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <fcntl.h>
typedef int NetSock;
typedef socklen_t NetAddrLen;
# define NET_INVALID (-1)
# define net_close_socket close
#endif

#define NET_PORT_DEFAULT 33777u
#define NET_TIMEOUT_TICKS 180u
#define NET_SNAPSHOT_TICKS 90u
static NetSock net_fd(const PcNetSession *s){ return (NetSock)s->sock; }
static void net_free(PcNetSession *s){
    if(s->sock!=(uintptr_t)NET_INVALID) net_close_socket(net_fd(s));
    s->sock=(uintptr_t)NET_INVALID;
#ifdef _WIN32
    WSACleanup();
#endif
}
static int net_open(PcNetSession *s, int host, const char *ip, uint16_t port){
    struct sockaddr_in a;
    NetSock fd;
    if(!s || (!host && (!ip || !port))) return 0;
    memset(s,0,sizeof *s);
    s->sock=(uintptr_t)NET_INVALID;
#ifdef _WIN32
    { WSADATA w; if(WSAStartup(MAKEWORD(2,2),&w)) return 0; }
#endif
    fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(fd==NET_INVALID){ net_free(s); return 0; }
    s->sock=(uintptr_t)fd;
#ifdef _WIN32
    { u_long yes=1; if(ioctlsocket(fd,FIONBIO,&yes)){ net_free(s); return 0; } }
#else
    { int flags=fcntl(fd,F_GETFL,0);
      if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){ net_free(s); return 0; } }
#endif
    memset(&a,0,sizeof a); a.sin_family=AF_INET;
    a.sin_port=htons(host?port:0);
    a.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,(struct sockaddr*)&a,(int)sizeof a)<0){ net_free(s); return 0; }
    { NetAddrLen n=(NetAddrLen)sizeof a;
      if(getsockname(fd,(struct sockaddr*)&a,&n)<0){ net_free(s); return 0; }
      s->local_port=ntohs(a.sin_port); }
    if(!host){
        /* IPv4 literals only: avoid DNS stalls on the simulation thread. */
        unsigned long addr=inet_addr(ip);
        if(addr==INADDR_NONE || addr==htonl(INADDR_ANY)){
            net_free(s); return 0;
        }
        s->peer_addr=(uint32_t)addr;
        s->peer_port=htons(port);
    }
    s->role=(uint8_t)(host?1:2);
    pc_duel_reset(&s->duel);
    return 1;
}
int pcnet_host(PcNetSession *s,uint16_t port){ return net_open(s,1,0,port); }
int pcnet_join(PcNetSession *s,const char *ip,uint16_t port){ return net_open(s,0,ip,port); }

static void net_send(PcNetSession *s,const BsNetPacket *p){
    uint8_t data[BSNET_MAX_FRAME];
    int len=bsnet_pack(p,data,sizeof data);
    struct sockaddr_in dst;
    if(len<=0 || !s->peer_port || !s->peer_addr) return;
    memset(&dst,0,sizeof dst); dst.sin_family=AF_INET;
    dst.sin_port=s->peer_port; dst.sin_addr.s_addr=s->peer_addr;
    (void)sendto(net_fd(s),(const char*)data,len,0,(struct sockaddr*)&dst,(int)sizeof dst);
}
static void net_hello(PcNetSession *s,uint32_t world,uint32_t frame){
    BsNetPacket p;
    memset(&p,0,sizeof p); p.type=BSNET_HELLO; p.sequence=s->seq++;
    p.tick=(uint16_t)frame; p.length=5;
    for(int i=0;i<4;i++)p.payload[i]=(uint8_t)(world>>(i*8));
    p.payload[4]=1; /* version-1 space-presence capability */
    net_send(s,&p);
}
static void net_ready(PcNetSession *s,uint32_t frame){
    BsNetPacket p;
    memset(&p,0,sizeof p); p.type=BSNET_READY; p.sequence=s->seq++;
    p.tick=(uint16_t)frame; p.length=1; p.payload[0]=1;
    net_send(s,&p);
}
static int16_t qpos(int32_t p){
    int32_t x=p/16;
    if(x>32767)x=32767;
    if(x<-32768)x=-32768;
    return (int16_t)x;
}
static void net_state(PcNetSession *s,const PcNetShip *local,uint32_t frame){
    BsNetPacket p;
    int16_t q[3]={qpos(local->x),qpos(local->y),qpos(local->z)};
    int pitch=local->pitch/4;
    if(pitch>127)pitch=127;
    if(pitch<-128)pitch=-128;
    memset(&p,0,sizeof p);p.type=BSNET_STATE;p.sequence=s->seq++;
    p.tick=(uint16_t)frame;p.length=8;
    for(int i=0;i<3;i++){
        p.payload[i*2]=(uint8_t)((uint16_t)q[i]&255u);
        p.payload[i*2+1]=(uint8_t)((uint16_t)q[i]>>8);
    }
    p.payload[6]=(uint8_t)(local->yaw>>2);
    p.payload[7]=(uint8_t)(int8_t)pitch;
    net_send(s,&p);
}
/* Three bytes of input: cumulative fired-laser counter, held buttons, reserved.
 * Counter survives a lost datagram; the host validates aim and cooldown.
 */
static void net_input(PcNetSession *s,uint32_t frame){
    BsNetPacket p;
    memset(&p,0,sizeof p);p.type=BSNET_INPUT;p.sequence=s->seq++;
    p.tick=(uint16_t)frame;p.length=3;p.payload[0]=s->local_shots;
    net_send(s,&p);
}
static void net_score(PcNetSession *s,uint32_t frame){
    BsNetPacket p;
    memset(&p,0,sizeof p);p.type=BSNET_EVENT;p.sequence=s->seq++;
    p.tick=(uint16_t)frame;p.length=4;
    p.payload[0]=s->duel.host_hp;p.payload[1]=s->duel.guest_hp;
    p.payload[2]=s->duel.host_wins;p.payload[3]=s->duel.guest_wins;
    net_send(s,&p);
}
static void net_decode(PcNetShip *v,const BsNetPacket *p,uint32_t world){
    int16_t q[3];
    for(int i=0;i<3;i++)q[i]=(int16_t)((uint16_t)p->payload[2*i]|
                                                   ((uint16_t)p->payload[2*i+1]<<8));
    v->x=(int32_t)q[0]*16;
    v->y=(int32_t)q[1]*16;
    v->z=(int32_t)q[2]*16;
    v->yaw=(uint16_t)(p->payload[6]<<2);
    v->pitch=(int16_t)((int8_t)p->payload[7])*4;
    v->world=world;
}
void pcnet_step(PcNetSession *s,uint32_t frame,const PcNetShip *local){
    if(!s || !s->role || !local) return;
    if(s->world!=local->world){
        s->world=local->world;
        s->connected=0;s->received=0;s->event_seen=0;s->state_seen=0;
        s->last_hello=0;s->last_send=0;
        s->host_seen_shots=s->local_shots;s->have_guest_fire=0;
        s->last_host_shot_tick=0;s->last_guest_shot_tick=0;
        pc_duel_reset(&s->duel);
        if(s->role==1){s->peer_addr=0;s->peer_port=0;}
    }
    for(int i=0;i<12;i++){
        uint8_t buffer[BSNET_MAX_FRAME+1];
        struct sockaddr_in peer;
        NetAddrLen n=(NetAddrLen)sizeof peer;
        int count=(int)recvfrom(net_fd(s),(char*)buffer,sizeof buffer,0,
                                (struct sockaddr*)&peer,&n);
        BsNetPacket p;
        if(count<=0) break;
        if(bsnet_unpack(&p,buffer,(uint8_t)count)!=0) continue;
        /* CRC validates transport integrity, not the message schema:
         * zero/short STATE or EVENT packets must never teleport a peer
         * or overwrite the authoritative duel score. */
        switch(p.type){
          case BSNET_HELLO: if(p.length!=5) continue; break;
          case BSNET_READY: if(p.length!=1) continue; break;
          case BSNET_STATE: if(p.length!=8) continue; break;
          case BSNET_INPUT: if(p.length!=3) continue; break;
          case BSNET_EVENT: if(p.length!=4) continue; break;
          case BSNET_BYE: if(p.length!=0) continue; break;
          default: continue;
        }
        if(s->role==2 && (peer.sin_addr.s_addr!=s->peer_addr ||
                          peer.sin_port!=s->peer_port)) continue;
        if(s->role==1 && s->connected &&
           (peer.sin_addr.s_addr!=s->peer_addr ||
            peer.sin_port!=s->peer_port)) continue;
        if(p.type==BSNET_HELLO && s->role==1){
            uint32_t their_world=0;
            for(int k=0;k<4;k++)their_world|=(uint32_t)p.payload[k]<<(8*k);
            if(their_world!=s->world || p.payload[4]!=1)continue;
            if(!s->connected){
                s->peer_addr=peer.sin_addr.s_addr;s->peer_port=peer.sin_port;
                s->connected=1;s->last_rx=frame;s->received=0;
                s->event_seen=0;s->state_seen=0;
                s->host_seen_shots=s->local_shots;s->have_guest_fire=0;
                s->last_host_shot_tick=0;s->last_guest_shot_tick=0;
                pc_duel_reset(&s->duel);
            }
            net_ready(s,frame);
        }else if(p.type==BSNET_READY && s->role==2 && p.payload[0]==1){
            if(!s->connected){
                pc_duel_reset(&s->duel);s->event_seen=0;s->state_seen=0;
            }
            s->connected=1;s->last_rx=frame;
        }else if(p.type==BSNET_STATE && s->connected){
            if(s->state_seen && (int16_t)(p.tick-s->last_state_tick)<=0)
                continue; /* reordered or duplicated unreliable UDP state */
            s->last_state_tick=p.tick;s->state_seen=1;
            net_decode(&s->other,&p,s->world);
            s->last_rx=frame;s->received=1;
        }else if(p.type==BSNET_INPUT && s->role==1 && s->connected){
            uint8_t shots=p.payload[0];
            if(!s->have_guest_fire){
                s->guest_seen_shots=shots;s->have_guest_fire=1;
            }else{
                uint8_t delta=(uint8_t)(shots-s->guest_seen_shots);
                if(delta>0 && delta<128u){
                    s->guest_seen_shots=shots;
                    if(s->received &&
                       (!s->last_guest_shot_tick ||
                        frame-s->last_guest_shot_tick>=6u)){
                        s->last_guest_shot_tick=frame;
                        (void)pc_duel_fire(&s->duel,1,&s->other,local,frame);
                    }
                }
            }
        }else if(p.type==BSNET_EVENT && s->role==2 && s->connected){
            if(s->event_seen && (int16_t)(p.tick-s->last_event_tick)<=0)
                continue; /* old scoreboard must not revive a destroyed ship */
            s->last_event_tick=p.tick;
            s->duel.host_hp=p.payload[0];s->duel.guest_hp=p.payload[1];
            s->duel.host_wins=p.payload[2];s->duel.guest_wins=p.payload[3];
            s->event_seen=1;s->last_rx=frame;
        }else if(p.type==BSNET_BYE && s->connected){
            s->connected=0;s->received=0;s->event_seen=0;s->state_seen=0;
            if(s->role==1){s->peer_addr=0;s->peer_port=0;}
        }
    }
    if(s->connected && frame-s->last_rx>NET_TIMEOUT_TICKS){
        s->connected=0;s->received=0;s->event_seen=0;s->state_seen=0;
        if(s->role==1){s->peer_addr=0;s->peer_port=0;}
    }
    if(!s->connected && s->role==2 && (!s->last_hello ||
       frame-s->last_hello>=30u)){
        net_hello(s,s->world,frame);
        s->last_hello=frame;
    }
    if(s->role==1 && s->connected && s->received){
        pc_duel_step(&s->duel,frame);
        if(s->local_shots!=s->host_seen_shots){
            s->host_seen_shots=s->local_shots;
            if(!s->last_host_shot_tick ||
               frame-s->last_host_shot_tick>=6u){
                s->last_host_shot_tick=frame;
                (void)pc_duel_fire(&s->duel,0,local,&s->other,frame);
            }
        }
    }else if(s->role==1){ s->host_seen_shots=s->local_shots; }
    if(s->connected && (!s->last_send || frame-s->last_send>=3u)){
        net_state(s,local,frame);
        if(s->role==1) net_score(s,frame);
        else net_input(s,frame);
        s->last_send=frame;
    }
}
void pcnet_fire(PcNetSession *s){ if(s && s->role) s->local_shots++; }
int pcnet_duel(const PcNetSession *s,uint8_t *host_hp,uint8_t *guest_hp,
               uint8_t *host_wins,uint8_t *guest_wins){
    if(!s || !s->connected || !s->received ||
       (s->role==2 && !s->event_seen)) return 0;
    if(host_hp)*host_hp=s->duel.host_hp;
    if(guest_hp)*guest_hp=s->duel.guest_hp;
    if(host_wins)*host_wins=s->duel.host_wins;
    if(guest_wins)*guest_wins=s->duel.guest_wins;
    return 1;
}
int pcnet_remote(const PcNetSession *s,uint32_t frame,PcNetShip *out){
    if(!s || !out || !s->connected || !s->received ||
       frame-s->last_rx>NET_SNAPSHOT_TICKS)return 0;
    *out=s->other;
    return 1;
}
void pcnet_close(PcNetSession *s){
    if(!s || !s->role)return;
    if(s->connected){
        BsNetPacket p;
        memset(&p,0,sizeof p);p.type=BSNET_BYE;
        net_send(s,&p);
    }
    net_free(s);memset(s,0,sizeof *s);
}
/* Singleton game adapter: submit only when space.inc actually draws a frame. */
static PcNetSession g_peer;
static PcNetShip g_local;
static uint8_t g_active,g_submitted;
int pc_mp_host(uint16_t port){
    if(g_active)return 0;
    g_active=(uint8_t)pcnet_host(&g_peer,port);
    return g_active;
}
int pc_mp_join(const char *ip,uint16_t port){
    if(g_active)return 0;
    g_active=(uint8_t)pcnet_join(&g_peer,ip,port);
    return g_active;
}
void pc_mp_submit(const PcNetShip *local){
    if(g_active && local){g_local=*local;g_submitted=1;}
}
void pc_mp_frame(uint32_t frame){
    /* The opt-in trace distinguishes transport failure from a game scene
     * that never entered 3D space. It is silent in normal play. */
    if(g_active && getenv("BUGSTAR_LAN_TRACE") && frame%300u==0u)
        fprintf(stderr,"lan: status frame %u role %u space %u world %u connected %u remote %u port %u\n",
                (unsigned)frame,(unsigned)g_peer.role,(unsigned)g_submitted,
                (unsigned)g_local.world,(unsigned)g_peer.connected,
                (unsigned)g_peer.received,(unsigned)g_peer.local_port);
    if(g_active && g_submitted){
        uint8_t before=g_peer.connected;
        uint8_t hp0=g_peer.duel.host_hp,hp1=g_peer.duel.guest_hp;
        pcnet_step(&g_peer,frame,&g_local);
        if(getenv("BUGSTAR_LAN_TRACE")){
            if(before!=g_peer.connected)
                fprintf(stderr,"lan: peer %s frame %u world %u role %u\n",
                        g_peer.connected?"CONNECTED":"DISCONNECTED",
                        (unsigned)frame,(unsigned)g_peer.world,
                        (unsigned)g_peer.role);
            if(g_peer.connected &&
               (hp0!=g_peer.duel.host_hp || hp1!=g_peer.duel.guest_hp))
                fprintf(stderr,"lan: duel P1=%u P2=%u wins=%u/%u frame %u\n",
                        (unsigned)g_peer.duel.host_hp,
                        (unsigned)g_peer.duel.guest_hp,
                        (unsigned)g_peer.duel.host_wins,
                        (unsigned)g_peer.duel.guest_wins,(unsigned)frame);
        }
    }
    g_submitted=0;
}
int pc_mp_remote(uint32_t frame,PcNetShip *out){
    return g_active && pcnet_remote(&g_peer,frame,out);
}
void pc_mp_fired(void){ if(g_active)pcnet_fire(&g_peer); }
int pc_mp_role(void){ return g_active ? g_peer.role : 0; }
int pc_mp_duel(uint8_t *host_hp,uint8_t *guest_hp,uint8_t *host_wins,uint8_t *guest_wins){
    return g_active && pcnet_duel(&g_peer,host_hp,guest_hp,host_wins,guest_wins);
}
void pc_mp_close(void){
    if(g_active)pcnet_close(&g_peer);
    g_active=0;g_submitted=0;
}
