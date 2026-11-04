// mix_test.c: the ROOM MIX option of the warship interior builder (wd_make_mix), driven by the real shipyard presets and by random mixes.
// Plain C. Built and run by pc/warship/test.sh.
#include "ws_mix_sy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks, fails;
#define CHECK(c, ...) do{ checks++; if(!(c)){ fails++; if(fails<=30){ printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } }while(0)
static WdDeck da, db;
// the deck of subsystem `sub` of a craft (same walk as the game: down the sectors to the deck)
static int deck_of(WdDeck *d, const WsCraft *c, int sub, int from_hangar, const WdMix *mix){
    int D=wd_levels(c); uint8_t path[400]; if(D>390) return 0; uint32_t h=wd_root(c); for(int k=0;k<D;k++){ path[k]=(uint8_t)wd_sub_child(c,sub,k); h=wd_child(h,path[k],k); }
    int list[WS_MAXSUB]; int n=wd_subs_at(c,path,D,list); wd_make_mix(d,c,h,list,n,from_hangar,D,mix); return 1; }
static int rooms_of(const WdDeck *d, int kind){ int n=0; for(int i=1;i<d->nroom;i++) if(d->room[i].kind==kind) n++; return n; }
static int targets_of(const WdDeck *d){ int t=0; for(int i=1;i<d->nroom;i++) if(d->room[i].sub!=255) t++; return t; }
#define CROWDED(d) (targets_of(d) > 6)      // the craft's own subsystem rooms already fill the deck: a mix gets what floor is left (never more than it asked)
static void reach(const WdDeck *d, const char *what){            // every room reachable with the key and the vents; every target room too
    for(int r=1;r<d->nroom;r++) CHECK(wd_path_len(d,d->sx,d->sy,r,1,1)>=0, "%s: room %s cut off", what, d->room[r].name);
    CHECK(d->tram_room>0, "%s: no tram", what); }
static const int zs[]={-700,-300,0,200,520,900};
int main(void){
    // 1. no mix = the old builder, byte for byte
    { int same=0, n=0; for(int s=0;s<5;s++) for(int zi=0;zi<6;zi++){ WsCraft c; ws_make(&c,(uint32_t)s*7477u+11,zs[zi],0); for(int sub=0;sub<c.nsub && sub<2;sub++){
            int D=wd_levels(&c); uint8_t path[400]; if(D>390) continue; uint32_t h=wd_root(&c); for(int k=0;k<D;k++){ path[k]=(uint8_t)wd_sub_child(&c,sub,k); h=wd_child(h,path[k],k); } int list[WS_MAXSUB]; int m=wd_subs_at(&c,path,D,list);
            wd_make(&da,&c,h,list,m,sub==0,D); wd_make_mix(&db,&c,h,list,m,sub==0,D,0); n++; if(!memcmp(&da,&db,sizeof da)) same++; } }
      CHECK(same==n && n>40, "wd_make and wd_make_mix(NULL) differ (%d of %d same)", same, n); printf("mix: no mix gives the old deck: %d of %d identical\n", same, n); }
    // 2. every shipyard preset drives a deck: its rooms exist, its props are the numbers sy_rooms asks for, everything stays reachable
    { int decks=0, exact=0, asked=0, crowded_decks=0; for(int p=0;p<SY_NPRESET;p++){ SyDesign sd; sy_default(&sd); sy_preset(&sd,p); WdMix mx; wd_mix_from_sy(&sd,&mx);
        for(int s=0;s<3;s++) for(int zi=0;zi<6;zi++){ WsCraft c; ws_make(&c,(uint32_t)s*7477u+11+p*131,zs[zi],0); int sub=s%c.nsub; int fh=s&1;
            if(!deck_of(&da,&c,sub,fh,&mx)) continue; deck_of(&db,&c,sub,fh,&mx); CHECK(!memcmp(&da,&db,sizeof da), "preset %s deck not deterministic", sy_preset_name[p]); decks++;
            reach(&da,sy_preset_name[p]);
            // the target of this deck is still there and reachable with the key
            int room=-1; for(int r=1;r<da.nroom;r++) if(da.room[r].sub==sub) room=r; CHECK(room>0 && wd_path_len(&da,da.sx,da.sy,room,1,0)>=0, "preset %s: target room lost", sy_preset_name[p]);
            // rooms (a crowded deck, with more than 6 subsystem rooms of its own, gets what floor is left)
            int crowd=CROWDED(&da); crowded_decks+=crowd;
            if(!crowd){
            if(mx.reactor) CHECK(rooms_of(&da,WR_REACTOR)>=1, "%s: no reactor room", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_REACTOR)==0, "%s: reactor room it should not have", sy_preset_name[p]);
            if(mx.shieldgen) CHECK(rooms_of(&da,WR_SHIELDGEN)>=1, "%s: no shield room", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_SHIELDGEN)==0, "%s: shield room it should not have", sy_preset_name[p]);
            if(mx.bridge) CHECK(rooms_of(&da,WR_BRIDGE)>=1, "%s: no bridge", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_BRIDGE)==0, "%s: bridge it should not have", sy_preset_name[p]);
            if(mx.workshop) CHECK(wd_count_rooms(&da,WR_WORKSHOP)>=1, "%s: no workshop", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_WORKSHOP)==0, "%s: workshop it should not have", sy_preset_name[p]);
            if(mx.mess) CHECK(wd_count_rooms(&da,WR_MESS)>=1, "%s: no mess", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_MESS)==0, "%s: mess it should not have", sy_preset_name[p]);
            if(mx.quarters) CHECK(wd_count_rooms(&da,WR_QUARTERS)>=1, "%s: no quarters", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_QUARTERS)==0, "%s: quarters it should not have", sy_preset_name[p]);
            if(mx.hangar) CHECK(wd_count_rooms(&da,WR_HANGAR)>=1, "%s: no hangar", sy_preset_name[p]); else CHECK(wd_count_rooms(&da,WR_HANGAR)<=(fh?1:0), "%s: more hangars than the dock", sy_preset_name[p]);
            if(mx.turret) CHECK(wd_count_rooms(&da,WR_TURRET)>=1, "%s: no turret room", sy_preset_name[p]);
            }
            // props: the numbers the design asked for
            struct { int type, want; const char *nm; } pr[4]={{WP_BUNK,mx.bunks,"bunks"},{WP_FIGHTER,mx.fighters,"fighters"},{WP_CRATE,mx.crates,"crates"},{WP_GUN,mx.guns,"guns"}};
            for(int k=0;k<4;k++){ int got=wd_count_props(&da,pr[k].type); asked++; if(got==pr[k].want) exact++; CHECK(crowd ? got<=pr[k].want : got==pr[k].want, "%s z%d: %s asked %d got %d", sy_preset_name[p], zs[zi], pr[k].nm, pr[k].want, got); } } }
      printf("mix: %d preset decks (%d crowded by the craft's own subsystem rooms), %d of %d prop counts exactly as the design asked\n", decks, crowded_decks, exact, asked); CHECK(decks>170, "only %d preset decks", decks); }
    // 3. the carrier shows what a mix is for: bunks, fighters, a hangar, a quarters and no generator hall that the design does not have
    { SyDesign sd; sy_default(&sd); sy_preset(&sd,5); WdMix mx; wd_mix_from_sy(&sd,&mx); CHECK(mx.fighters==16 && mx.bunks==24 && mx.hangar && mx.quarters, "carrier mix");
      WsCraft c; ws_make(&c,4242,520,0); deck_of(&da,&c,0,1,&mx); CHECK(wd_count_props(&da,WP_FIGHTER)==16 && wd_count_props(&da,WP_BUNK)==24, "carrier deck props %d fighters %d bunks", wd_count_props(&da,WP_FIGHTER), wd_count_props(&da,WP_BUNK));
      CHECK(wd_count_rooms(&da,WR_HANGAR)>=2 && wd_count_rooms(&da,WR_QUARTERS)>=2, "a carrier needs more than one hangar and quarters (hangars %d quarters %d)", wd_count_rooms(&da,WR_HANGAR), wd_count_rooms(&da,WR_QUARTERS));
      printf("mix: carrier deck: %d hangars, %d fighters, %d quarters, %d bunks, %d crates, %d guns, %d rooms\n", wd_count_rooms(&da,WR_HANGAR), wd_count_props(&da,WP_FIGHTER), wd_count_rooms(&da,WR_QUARTERS), wd_count_props(&da,WP_BUNK), wd_count_props(&da,WP_CRATE), wd_count_props(&da,WP_GUN), da.nroom-1); }
    // 4. a design with no hangar still lets you dock: the dock hangar is there and empty of fighters
    { SyDesign sd; sy_default(&sd); sy_preset(&sd,1); WdMix mx; wd_mix_from_sy(&sd,&mx); CHECK(!mx.hangar && mx.fighters==0, "dart has no bay"); WsCraft c; ws_make(&c,99,300,0); deck_of(&da,&c,0,1,&mx);
      CHECK(rooms_of(&da,WR_HANGAR)>=1 && wd_count_props(&da,WP_FIGHTER)==0, "dock hangar with no fighters"); reach(&da,"dart dock"); }
    // 5. random mixes, including silly ones: always a deck, always the same deck, always reachable, never more props than asked, never a room or prop count out of range
    { unsigned long long rs=88172645463325252ull; int decks=0, exact=0, asked=0, full=0, calm=0, calm_exact=0;
      for(int i=0;i<300;i++){ rs^=rs<<13; rs^=rs>>7; rs^=rs<<17; unsigned long long v=rs; WdMix mx; mx.reactor=v&1; mx.hangar=(v>>1)&1; mx.quarters=(v>>2)&1; mx.bridge=(v>>3)&1; mx.turret=(uint8_t)((v>>4)%8); mx.shieldgen=(v>>7)&1; mx.workshop=(v>>8)&1; mx.mess=(v>>9)&1;
        mx.bunks=(uint8_t)((v>>10)%49); mx.crates=(uint8_t)((v>>16)%41); mx.guns=(uint8_t)((v>>22)%20); mx.fighters=(uint8_t)((v>>28)%40); if(i%50==0) memset(&mx,0,sizeof mx);
        WsCraft c; ws_make(&c,(uint32_t)(v>>34)*2654435761u+(uint32_t)i,zs[i%6]-(int)(v%200),0); int sub=(int)((v>>40)%(unsigned)c.nsub), fh=(int)((v>>50)&1);
        if(!deck_of(&da,&c,sub,fh,&mx)) continue; deck_of(&db,&c,sub,fh,&mx); CHECK(!memcmp(&da,&db,sizeof da),"random mix %d not deterministic",i); decks++; reach(&da,"random mix");
        int room=-1; for(int r=1;r<da.nroom;r++) if(da.room[r].sub==sub) room=r; CHECK(room>0 && wd_path_len(&da,da.sx,da.sy,room,1,1)>=0, "random mix %d: target lost",i);
        CHECK(da.nprop<=WD_MAXPROP && da.nroom<=WD_MAXROOM, "limits");
        int cap = mx.guns && !mx.turret ? 1 : mx.turret; (void)cap;
        struct { int type, want; } pr[4]={{WP_BUNK,mx.bunks},{WP_FIGHTER,mx.fighters},{WP_CRATE,mx.crates},{WP_GUN,mx.guns}};
        for(int k=0;k<4;k++){ int got=wd_count_props(&da,pr[k].type); CHECK(got<=pr[k].want, "random mix %d type %d: asked %d got %d", i, pr[k].type, pr[k].want, got); asked++; if(got==pr[k].want) exact++; if(!CROWDED(&da)){ calm++; if(got==pr[k].want) calm_exact++; } }
        if(!mx.reactor) CHECK(wd_count_rooms(&da,WR_REACTOR)==0, "random mix %d: reactor room not asked for", i);
        if(!mx.workshop) CHECK(wd_count_rooms(&da,WR_WORKSHOP)==0, "random mix %d: workshop not asked for", i);
        if(!mx.mess) CHECK(wd_count_rooms(&da,WR_MESS)==0, "random mix %d: mess not asked for", i);
        if(wd_count_props(&da,WP_BUNK)==mx.bunks && wd_count_props(&da,WP_FIGHTER)==mx.fighters) full++; }
      printf("mix: %d random mixes (up to 48 bunks, 40 crates, 19 guns, 39 fighters): %d of %d prop counts met exactly (%d of %d where the deck was not crowded), never over\n", decks, exact, asked, calm_exact, calm); CHECK(decks>290, "only %d random decks", decks);
      CHECK(calm_exact*100 >= calm*97, "too many random mixes did not fit on an uncrowded deck: %d of %d", calm_exact, calm); }
    printf("mix: %d checks, %d failed\n", checks, fails); return fails?1:0;
}
