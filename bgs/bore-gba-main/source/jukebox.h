// jukebox.h - the playlist side of the jukebox. No hardware in here (the player and screen live in main.c).
// Needs before it: u8 / u32, rnd8(), SRAM_BASE.
//
// The shuffled order is kept in SRAM, so the song set stays in the SAME shuffled order every time the game starts.
// It is only re-rolled when you press SELECT in the jukebox, or when the number of songs changes (songs added / removed).
#define JB_MAX 32          // most songs the jukebox can hold
#define JB_OFF 12288       // SRAM block: 'J' 'B' '1', song count, current slot, then the shuffled order
static u8 sJb;             // jukebox mode (a setting, saved with the other settings): 0 SHUFFLE, 1 IN ORDER, 2 REPEAT ONE
static u8 jbMap[JB_MAX];   // visible song number -> index into songs[] (hides the placeholder tunes)
static u8 jbOrd[JB_MAX];   // the shuffled order: playlist slot -> song number
static int jbN, jbPos;     // number of songs, playlist slot of the current song
static const char* const jbModeNm[3]={"SHUFFLE","IN ORDER","REPEAT ONE"};

static int jbIdx(int slot){ return sJb==0 ? jbOrd[slot] : slot; }   // which visible song sits in a playlist slot
static int jbSong(int slot){ return jbMap[jbIdx(slot)]; }          // the songs[] entry for that slot
static int jbSlotOf(int song){ for(int i=0;i<jbN;i++) if(jbSong(i)==song) return i; return 0; }
static void jbSave(void){
    volatile u8*m=SRAM_BASE+JB_OFF; m[0]='J'; m[1]='B'; m[2]='1'; m[3]=(u8)jbN; m[4]=(u8)jbPos;
    for(int i=0;i<jbN;i++) m[5+i]=jbOrd[i];
}
static int jbLoad(void){   // 1 = a saved order for exactly this many songs was found
    volatile u8*m=SRAM_BASE+JB_OFF; u32 seen=0;
    if(m[0]!='J'||m[1]!='B'||m[2]!='1'||m[3]!=(u8)jbN||m[4]>=jbN) return 0;
    for(int i=0;i<jbN;i++){ u8 v=m[5+i]; if(v>=jbN||((seen>>v)&1u)) return 0; seen|=1u<<v; }   // must be a real permutation
    for(int i=0;i<jbN;i++) jbOrd[i]=m[5+i];
    jbPos=m[4]; return 1;
}
static void jbShuffle(void){   // Fisher-Yates
    for(int i=0;i<jbN;i++) jbOrd[i]=(u8)i;
    for(int i=jbN-1;i>0;i--){ int j=(rnd8()*(i+1))>>8; u8 t=jbOrd[i]; jbOrd[i]=jbOrd[j]; jbOrd[j]=t; }
}
static void jbReshuffle(void){   // new random order, saved. The song that is playing stays first, the rest follow it.
    int s=jbIdx(jbPos); sJb=0; jbShuffle();
    for(int i=0;i<jbN;i++) if(jbOrd[i]==s){ u8 t=jbOrd[0]; jbOrd[0]=jbOrd[i]; jbOrd[i]=t; break; }
    jbPos=0; jbSave();
}
static void jbSetMode(int m){   // change mode but keep the current song current
    int s=jbSong(jbPos); sJb=(u8)m; jbPos=jbSlotOf(s); jbSave();
}
static void jbInit(int n){   // call once at boot, after the settings are loaded and the random seed has been stirred
    jbN=n>JB_MAX?JB_MAX:n;
    if(!jbLoad()){ jbShuffle(); jbPos=0; jbSave(); }
}
