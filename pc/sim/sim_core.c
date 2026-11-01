// sim_core.c: see sim_core.h
#include "sim_core.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------- random numbers: stateless, so order can never change a result
uint64_t sim_mix64(uint64_t x){
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
uint64_t sim_rng(uint64_t seed, uint32_t id, uint32_t purpose, uint64_t tick){
    uint64_t h = sim_mix64(seed ^ ((uint64_t)id * 0xD6E8FEB86659FD93ull));
    h = sim_mix64(h ^ ((uint64_t)purpose * 0xA0761D6478BD642Full));
    return sim_mix64(h + tick * 0xE7037ED1A0B428DBull);
}
uint32_t sim_rngn(uint64_t seed, uint32_t id, uint32_t purpose, uint64_t tick, uint32_t n){
    return n ? (uint32_t)((sim_rng(seed, id, purpose, tick) >> 16) % n) : 0;
}

// purposes (any distinct numbers)
enum { RP_NAME = 1, RP_JOB, RP_SHIFT, RP_NEED, RP_LAND, RP_WEATHER, RP_CHAT, RP_SPECIES, RP_CAST, RP_NEWFOLK, RP_IDEA, RP_HOME, RP_FRIEND };

// ---------------------------------------------------------------- names
static const char *const ON[] = { "P","M","T","B","K","L","N","R","S","V","W","Z","D","G","F","H","J","CH","SH","TH","BR","TR","FL","GR","SN","PL" };
static const char *const VO[] = { "A","E","I","O","U","OO","EE","AU","IA","Y" };
static const char *const CO[] = { "","","N","M","L","R","X","K","P","T","S","B" };
#define NONS ((int)(sizeof ON / sizeof *ON))
#define NVOW ((int)(sizeof VO / sizeof *VO))
#define NCOD ((int)(sizeof CO / sizeof *CO))
void sim_name(uint32_t seed32, uint32_t id, char *out){
    uint64_t s = seed32; char b[24]; b[0] = 0;
    int syl = 1 + (int)sim_rngn(s, id, RP_NAME, 0, 3) % 2;                       // 1 or 2 syllables
    for(int k = 0; k < syl; k++){
        strcat(b, ON[sim_rngn(s, id, RP_NAME, 10 + k * 3, NONS)]);
        strcat(b, VO[sim_rngn(s, id, RP_NAME, 11 + k * 3, NVOW)]);
        strcat(b, CO[sim_rngn(s, id, RP_NAME, 12 + k * 3, NCOD)]);
    }
    if(strlen(b) < 3) strcat(b, "O");
    b[8] = 0; strcpy(out, b);
}
void sim_species_name(uint32_t seed, char *out){
    static const char *const A[] = { "GLIM","MOSS","DUSK","SPORE","PEBBLE","REED","FERN","SALT","EMBER","DEW","LICHEN","AMBER" };
    static const char *const B[] = { "BACK","WING","HOPPER","TAIL","SNOUT","CRAWLER","MOTH","BEAK","FOOT","GRAZER","DRIFTER","SKIPPER" };
    uint64_t s = seed;
    strcpy(out, A[sim_rngn(s, 1, RP_SPECIES, 1, 12)]); strcat(out, B[sim_rngn(s, 2, RP_SPECIES, 2, 12)]);
}

static const char *const JOBN[SIMJ_N] = { "STALL KEEPER", "COOK", "CLERK", "RACER", "FORAGER", "MINER", "GUARD" };
static const char *const TASKN[SIMT_N] = { "SLEEPING", "EATING", "WORKING", "RESTING", "CHATTING" };
static const char *const WEATHN[SIMW_N] = { "CLEAR", "CLOUDY", "RAIN", "STORM", "SNOW" };
const char *sim_task_name(int t){ return t >= 0 && t < SIMT_N ? TASKN[t] : "?"; }
const char *sim_job_name(int j){ return j >= 0 && j < SIMJ_N ? JOBN[j] : "?"; }
const char *sim_weather_name(int w){ return w >= 0 && w < SIMW_N ? WEATHN[w] : "?"; }

// ---------------------------------------------------------------- clock
int sim_hour(const Sim *s){ return (int)((s->tick % s->day_ticks) * 24 / s->day_ticks); }
int sim_season(const Sim *s){ return (int)((s->tick / s->day_ticks / SIM_SEASON_DAYS) % 4); }
const char *sim_season_name(int se){ static const char *const n[4] = { "SPRING", "SUMMER", "AUTUMN", "WINTER" }; return n[se & 3]; }
int sim_sun_hour(const Sim *s){ return s->locked ? 12 : sim_hour(s); }
void sim_set_env(Sim *s, int day_hours, int temp_c){
    s->env_on = 1; s->temp_c = (int16_t)temp_c; s->locked = day_hours <= 0;
    int dt = day_hours <= 0 ? SIM_DAY_TICKS : (int)((long long)SIM_DAY_TICKS * (day_hours > 1000 ? 1000 : day_hours) / 24);
    s->day_ticks = (uint32_t)(dt < 1200 ? 1200 : dt > 12000 ? 12000 : dt);
    s->day_seen = (uint32_t)sim_day(s);
}
int sim_day(const Sim *s){ return (int)(s->tick / s->day_ticks); }

// ---------------------------------------------------------------- event log
void sim_log(Sim *s, int kind, int who, int a, int b){
    SimEvent *e = &s->log[s->log_head % SIM_LOGN];
    e->tick = (uint32_t)s->tick; e->kind = (uint16_t)kind; e->who = (uint16_t)who; e->a = (uint16_t)a; e->b = (uint16_t)b; e->seed32 = (uint32_t)sim_mix64(s->seed);
    s->log_head = (s->log_head + 1) % SIM_LOGN; s->log_n++;
}
uint32_t sim_log_count(const Sim *s){ return s->log_n < SIM_LOGN ? s->log_n : SIM_LOGN; }
const SimEvent *sim_log_at(const Sim *s, uint32_t back){
    if(back >= sim_log_count(s)) return 0;
    return &s->log[(s->log_head + SIM_LOGN - 1 - back) % SIM_LOGN];
}
static size_t cat(char *o, size_t n, size_t at, const char *t){ while(*t && at + 1 < n) o[at++] = *t++; if(n) o[at < n ? at : n - 1] = 0; return at; }
static size_t catn(char *o, size_t n, size_t at, long v){ char t[24]; snprintf(t, sizeof t, "%ld", v); return cat(o, n, at, t); }
int sim_event_text(const Sim *s, const SimEvent *e, char *o, size_t n){
    char nm[SIM_NAMELEN + 1], nm2[SIM_NAMELEN + 1], sp[24]; size_t at = 0; (void)s;
    if(!n) return 0;
    o[0] = 0;
    sim_name(e->seed32, e->who, nm); sim_name(e->seed32, e->b, nm2);
    switch(e->kind){
    case SIME_WAKE:   at = cat(o, n, at, nm); at = cat(o, n, at, " WOKE UP"); break;
    case SIME_SLEEP:  at = cat(o, n, at, nm); at = cat(o, n, at, " WENT TO BED"); break;
    case SIME_WORK:   at = cat(o, n, at, nm);
        switch(e->a){ case SIMJ_STALL: at = cat(o, n, at, " OPENED THE STALL"); break; case SIMJ_COOK: at = cat(o, n, at, " LIT THE STOVE"); break;
            case SIMJ_CLERK: at = cat(o, n, at, " OPENED THE OFFICE"); break; case SIMJ_RACER: at = cat(o, n, at, " WARMED UP A KART"); break;
            case SIMJ_FORAGER: at = cat(o, n, at, " WENT FORAGING"); break; case SIMJ_MINER: at = cat(o, n, at, " WENT TO DIG"); break;
            default: at = cat(o, n, at, " TOOK THE WATCH"); break; } break;
    case SIME_EAT:    at = cat(o, n, at, nm); at = cat(o, n, at, " SAT DOWN TO EAT"); break;
    case SIME_CHAT:   at = cat(o, n, at, nm); at = cat(o, n, at, " CHATTED WITH "); at = cat(o, n, at, nm2); break;
    case SIME_HUNGRY: at = cat(o, n, at, nm); at = cat(o, n, at, " IS HUNGRY"); break;
    case SIME_TIRED:  at = cat(o, n, at, nm); at = cat(o, n, at, " IS TIRED"); break;
    case SIME_WEATHER: at = cat(o, n, at, e->a == SIMW_RAIN ? "RAIN BEGAN TO FALL" : e->a == SIMW_SNOW ? "SNOW BEGAN TO FALL" : e->a == SIMW_STORM ? "A STORM CAME IN" : e->a == SIMW_CLOUD ? "CLOUDS ROLLED IN" : "THE SKY CLEARED"); break;
    case SIME_DAWN:   at = cat(o, n, at, "DAY "); at = catn(o, n, at, (long)e->a + 1); at = cat(o, n, at, " BEGAN"); break;
    case SIME_CAST:   { const GodPower *p = god_power(e->a); at = cat(o, n, at, "YOU CAST: "); at = cat(o, n, at, p ? p->name : "?"); } break;
    case SIME_UNLOCK: god_unlock_text(e->a, o, n); at = strlen(o); break;
    case SIME_EMPTY:  at = cat(o, n, at, "THE LARDER IS EMPTY"); break;
    case SIME_HARVEST: at = cat(o, n, at, nm); at = cat(o, n, at, " BROUGHT IN A GOOD HARVEST"); break;
    case SIME_NEWFOLK: at = cat(o, n, at, nm); at = cat(o, n, at, " SETTLED IN"); break;
    case SIME_SPECIES: sim_species_name(((uint32_t)e->a << 16) | e->b, sp); at = cat(o, n, at, "THE "); at = cat(o, n, at, sp); at = cat(o, n, at, " APPEARED"); break;
    case SIME_LEGEND: at = cat(o, n, at, "A LEGEND WAS BORN"); break;
    case SIME_IDEA:   at = cat(o, n, at, nm); at = cat(o, n, at, " HAD A GOOD IDEA"); break;
    case SIME_COLONY: { ColEvent ce = { (uint8_t)e->a, e->b, e->seed32, e->tick }; char t[52]; col_event_text(&ce, t, sizeof t); at = cat(o, n, at, t); } break;
    case SIME_FRIEND: at = cat(o, n, at, nm); at = cat(o, n, at, e->a >= 100 ? " IS YOUR BEST FRIEND" : e->a >= 50 ? " IS YOUR FRIEND" : " KNOWS YOU NOW"); break;
    case SIME_LETTER: { char t[48]; sim_letter_text(s, e->who, e->a, t, sizeof t); at = cat(o, n, at, "LETTER FROM "); at = cat(o, n, at, nm); at = cat(o, n, at, ": "); at = cat(o, n, at, t); } break;
    case SIME_SANDBOX: at = cat(o, n, at, e->a ? "SANDBOX: ALL GOD POWERS ARE OPEN" : "SANDBOX IS OFF"); break;
    default: at = cat(o, n, at, "..."); break;
    }
    return (int)at;
}

// ---------------------------------------------------------------- the town
static int outdoor(int job){ return job != SIMJ_COOK && job != SIMJ_CLERK; }
static int sched(int h){                       // what a bug does at hour h of its own day
    if(h >= 22 || h < 6) return SIMT_SLEEP;
    if(h == 6 || h == 12 || h == 17) return SIMT_EAT;
    if(h >= 7 && h < 17) return SIMT_WORK;
    if(h < 20) return SIMT_SOCIAL;
    return SIMT_LEISURE;
}
static int place_of(const SimFolk *f, int task){
    switch(task){ case SIMT_SLEEP: return SIMP_HOME; case SIMT_EAT: return SIMP_EATERY; case SIMT_WORK: return SIMP_WORK; default: return SIMP_PLAZA; }
    (void)f;
}
int sim_mood(const SimFolk *f){ int t = 0; for(int i = 0; i < SIMN_N; i++) t += f->need[i]; return t / SIMN_N; }
void sim_status_text(const SimFolk *f, char *o, size_t n){
    size_t at = 0; int any = 0; if(!n) return; o[0] = 0;
    if(f->need[SIMN_REST] < 70){ at = cat(o, n, at, "TIRED. "); any = 1; }
    if(f->need[SIMN_HUNGER] < 70){ at = cat(o, n, at, "HUNGRY. "); any = 1; }
    if(f->need[SIMN_SOCIAL] < 70){ at = cat(o, n, at, "LONELY. "); any = 1; }
    if(f->need[SIMN_FUN] < 70){ at = cat(o, n, at, "BORED. "); any = 1; }
    if(!any) cat(o, n, at, sim_mood(f) > 190 ? "HAPPY." : "FINE.");
}

static void new_folk(Sim *s, int i, uint32_t jobmask, uint32_t purpose){
    SimFolk *f = &s->folk[i]; memset(f, 0, sizeof *f);
    uint32_t seed32 = (uint32_t)sim_mix64(s->seed);
    sim_name(seed32, (uint32_t)i, f->name);
    int allowed[SIMJ_N], na = 0;
    for(int j = 0; j < SIMJ_N; j++) if(!jobmask || (jobmask >> j) & 1) allowed[na++] = j;
    f->job = (uint8_t)(na ? allowed[sim_rngn(s->seed, (uint32_t)i, RP_JOB, purpose, (uint32_t)na)] : SIMJ_FORAGER);
    f->home = (uint8_t)i;
    f->shift = (uint8_t)sim_rngn(s->seed, (uint32_t)i, RP_SHIFT, 0, 3);
    for(int k = 0; k < SIMN_N; k++) f->need[k] = (uint8_t)(150 + sim_rngn(s->seed, (uint32_t)i, RP_NEED, (uint64_t)k, 100));
    f->pal = 0xFFFF;
}
static int folk_hour(const Sim *s, const SimFolk *f){ return (sim_hour(s) + 24 - f->shift) % 24; }

static void new_day(Sim *s, int first){
    int day = sim_day(s); s->day_seen = (uint32_t)day;
    uint32_t r = sim_rngn(s->seed, 0, RP_WEATHER, (uint64_t)day, 100);
    int w = r < 55 ? SIMW_CLEAR : r < 75 ? SIMW_CLOUD : r < 95 ? SIMW_RAIN : SIMW_STORM;
    if(s->env_on){                                                                  // Phase 2: the planet's climate and the season shape the weather
        int se = sim_season(s), t = s->temp_c + (se == 3 ? -12 : se == 1 ? 8 : 0) * (s->temp_c > -40 ? 1 : 0);
        if(t <= 0 && (w == SIMW_RAIN || w == SIMW_STORM)) w = SIMW_SNOW;
        else if(t > 45 && w == SIMW_RAIN) w = SIMW_CLEAR;                           // too hot for rain
        else if(se == 2 && w == SIMW_CLEAR && r < 20) w = SIMW_CLOUD;
    }
    if(w != s->weather || first){ s->weather = (uint8_t)w; if(!first && (w == SIMW_RAIN || w == SIMW_STORM || w == SIMW_CLEAR || w == SIMW_SNOW)) sim_log(s, SIME_WEATHER, 0, w, 0); }
    s->weather_left = (uint16_t)(s->day_ticks / 2 + sim_rngn(s->seed, 0, RP_WEATHER, (uint64_t)day + 100000, s->day_ticks / 2));
    int target = 30 + s->forest * 4 + (s->weather == SIMW_RAIN ? 10 : 0); if(target > 90) target = 90;     // the soil drifts back to what the land can hold
    if(s->fertility < target) s->fertility = (uint8_t)(s->fertility + (target - s->fertility + 3) / 4);
    else if(s->fertility > target) s->fertility = (uint8_t)(s->fertility - (s->fertility - target + 3) / 4);
    if(s->pests < 100 && s->weather != SIMW_STORM) s->pests++;
    s->larder -= s->larder / 10 + s->pests * s->larder / 2000;                       // food spoils, and pests eat it
    int happy = 0; for(int i = 0; i < s->nfolk; i++) if(sim_mood(&s->folk[i]) >= 180) happy++;
    if(!first){ sim_log(s, SIME_DAWN, 0, day, 0); s->spirit = (uint16_t)(s->spirit + happy / 2 > SIM_SPIRIT_MAX ? SIM_SPIRIT_MAX : s->spirit + happy / 2); }
}

void sim_init(Sim *s, uint64_t seed, int nfolk, uint32_t jobmask, uint64_t start_tick){
    memset(s, 0, sizeof *s);
    s->seed = seed; s->tick = start_tick; s->day_ticks = SIM_DAY_TICKS;
    if(nfolk < 0) nfolk = 0;
    if(nfolk > SIM_MAXFOLK) nfolk = SIM_MAXFOLK;
    s->nfolk = nfolk;
    for(int i = 0; i < nfolk; i++) new_folk(s, i, jobmask, 0);
    { int has = 0; for(int i = 0; i < nfolk; i++) if(s->folk[i].job == SIMJ_FORAGER) has = 1;           // every town has somebody who brings in food
      if(!has && nfolk >= 3 && (!jobmask || (jobmask >> SIMJ_FORAGER) & 1)) s->folk[nfolk - 1].job = SIMJ_FORAGER; }
    s->fertility = (uint8_t)(40 + sim_rngn(seed, 0, RP_LAND, 1, 30)); s->water = (uint8_t)(40 + sim_rngn(seed, 0, RP_LAND, 2, 40));
    s->shelter = (uint8_t)(40 + sim_rngn(seed, 0, RP_LAND, 3, 30)); s->comfort = (uint8_t)(30 + sim_rngn(seed, 0, RP_LAND, 4, 30));
    s->health = 80; s->luck = (uint8_t)sim_rngn(seed, 0, RP_LAND, 5, 30); s->pests = (uint8_t)(10 + sim_rngn(seed, 0, RP_LAND, 6, 30));
    s->larder = 40u + 6u * (uint32_t)nfolk; s->spirit = 30;
    new_day(s, 1);
    for(int i = 0; i < nfolk; i++){ SimFolk *f = &s->folk[i]; f->task = (uint8_t)sched(folk_hour(s, f)); f->place = (uint8_t)place_of(f, f->task); }
    god_state_init(&s->god);
}

static uint8_t sat8(int v){ return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); }
static void add_need(SimFolk *f, int k, int d){ f->need[k] = sat8(f->need[k] + d); }

static void folk_tick(Sim *s, int i){
    SimFolk *f = &s->folk[i]; uint64_t t = s->tick; int h = folk_hour(s, f);
    // 1. what should I be doing?
    int want = sched(h);
    if(want != SIMT_SLEEP){
        if(f->need[SIMN_HUNGER] < 50 && s->larder > 0 && want != SIMT_EAT) want = SIMT_EAT;
        if(f->need[SIMN_REST] < 20) want = SIMT_SLEEP;
        if(want == SIMT_WORK && s->weather == SIMW_STORM && outdoor(f->job)) want = SIMT_LEISURE;
    }
    if(want != f->task){
        int old = f->task; f->task = (uint8_t)want; f->place = (uint8_t)place_of(f, want);
        if(old == SIMT_SLEEP) sim_log(s, SIME_WAKE, i, 0, 0);
        if(want == SIMT_SLEEP) sim_log(s, SIME_SLEEP, i, 0, 0);
        else if(want == SIMT_WORK) sim_log(s, SIME_WORK, i, f->job, 0);
        else if(want == SIMT_EAT) sim_log(s, SIME_EAT, i, 0, 0);
    }
    // 2. needs fall and are met (each rule fires on a fixed rhythm, offset by the bug's number)
    uint64_t ph = t + (uint64_t)i * 7;
    int asleep = f->task == SIMT_SLEEP;
    if(ph % (asleep ? 60 : 30) == 0) add_need(f, SIMN_HUNGER, -1);
    if(!asleep && ph % 20 == 0) add_need(f, SIMN_REST, -1);
    if(ph % 40 == 0){ add_need(f, SIMN_SOCIAL, -1); add_need(f, SIMN_FUN, -1); }
    switch(f->task){
    case SIMT_SLEEP: if(ph % 6 == 0) add_need(f, SIMN_REST, 1 + s->comfort / 60); break;
    case SIMT_EAT:
        if(s->larder > 0){ if(ph % 5 == 0) add_need(f, SIMN_HUNGER, 4); if(ph % 100 == 0) s->larder--; if(ph % 20 == 0) add_need(f, SIMN_SOCIAL, 1); }
        else if(!(f->warned & 4)){ f->warned |= 4; sim_log(s, SIME_EMPTY, i, 0, 0); }
        if(s->larder > 0) f->warned &= (uint8_t)~4;
        break;
    case SIMT_WORK:
        if(f->job == SIMJ_FORAGER && ph % 60 == 0){
            uint32_t gain = 1u + s->fertility / 50u + (s->weather == SIMW_RAIN ? 1u : 0u) + f->skill / 64u;
            s->larder += gain; if(s->larder > 9999) s->larder = 9999;
            if(gain >= 4 && ph % 600 == 0) sim_log(s, SIME_HARVEST, i, 0, 0);
        }
        if(ph % 100 == 0 && f->skill < 60 && f->job != SIMJ_FORAGER) f->skill++;
        break;
    case SIMT_LEISURE: if(ph % 5 == 0) add_need(f, SIMN_FUN, 1); if(ph % 20 == 0) add_need(f, SIMN_SOCIAL, 1); break;
    case SIMT_SOCIAL:
        if(ph % 4 == 0) add_need(f, SIMN_SOCIAL, 1);
        if(ph % 20 == 0) add_need(f, SIMN_FUN, 1);
        if(ph % 150 == 0){
            int off = (int)sim_rngn(s->seed, (uint32_t)i, RP_CHAT, t, (uint32_t)s->nfolk);
            for(int k = 0; k < s->nfolk; k++){ int j = (off + k) % s->nfolk; if(j != i && s->folk[j].task == SIMT_SOCIAL){
                sim_log(s, SIME_CHAT, i, 0, j); f->pal = (uint16_t)j; add_need(f, SIMN_SOCIAL, 12); add_need(&s->folk[j], SIMN_SOCIAL, 12); break; } }
        }
        break;
    }
    // 3. complaints go in the log once
    if(f->need[SIMN_HUNGER] < 50 && !(f->warned & 1)){ f->warned |= 1; sim_log(s, SIME_HUNGRY, i, 0, 0); } else if(f->need[SIMN_HUNGER] > 120) f->warned &= (uint8_t)~1;
    if(f->need[SIMN_REST] < 40 && !(f->warned & 2)){ f->warned |= 2; sim_log(s, SIME_TIRED, i, 0, 0); } else if(f->need[SIMN_REST] > 120) f->warned &= (uint8_t)~2;
}

void sim_step(Sim *s){
    if((uint32_t)sim_day(s) != s->day_seen) new_day(s, 0);
    if(s->weather_left && --s->weather_left == 0 && s->weather != SIMW_CLEAR){ s->weather = SIMW_CLEAR; sim_log(s, SIME_WEATHER, 0, SIMW_CLEAR, 0); }
    for(int i = 0; i < s->nfolk; i++) folk_tick(s, i);                       // by number, always in the same order
    if(s->tick % 100 == 0){ if(s->spirit < SIM_SPIRIT_MAX) s->spirit++; s->larder += 1u + (uint32_t)(s->nfolk / 4); }      // (trade brings a little food too)
    s->tick++; s->steps_done++;
}
int sim_feed(Sim *s, int vbl, int maxsteps){
    int n = 0; if(vbl < 0) vbl = 0;
    int acc = s->acc + vbl;
    while(acc >= SIM_TICK_VBL && n < maxsteps){ acc -= SIM_TICK_VBL; sim_step(s); n++; }
    if(acc >= SIM_TICK_VBL) acc = SIM_TICK_VBL - 1;                          // too far behind: drop the rest (menus, long frames)
    s->acc = (uint8_t)acc; return n;
}
void sim_run(Sim *s, uint64_t ticks){ while(ticks--) sim_step(s); }

// ---------------------------------------------------------------- god powers in the side game
static void all_need(Sim *s, int k, int d){ for(int i = 0; i < s->nfolk; i++) add_need(&s->folk[i], k, d); }
static uint8_t cap100(int v){ return (uint8_t)(v > 100 ? 100 : v < 0 ? 0 : v); }
int sim_cast(Sim *s, const Lv *level, int n){
    const GodPower *p = god_power(n);
    if(!p) return -3;
    if(!god_unlocked(level, n, s->god.sandbox)) return -1;
    if(!s->god.sandbox && s->spirit < p->cost) return -2;
    if(!s->god.sandbox) s->spirit = (uint16_t)(s->spirit - p->cost);
    int a = p->amt;
    uint32_t r = sim_rngn(s->seed, 0, RP_CAST, s->tick + (uint64_t)n * 977, 1u << 30);
    switch(p->eff){
    case GE_FERTILE: s->fertility = cap100(s->fertility + a); break;
    case GE_CLEAR:   s->weather = SIMW_CLEAR; s->weather_left = 0; sim_log(s, SIME_WEATHER, 0, SIMW_CLEAR, 0); break;
    case GE_RAIN:    s->weather = SIMW_RAIN; s->weather_left = (uint16_t)(s->day_ticks / 3); s->water = cap100(s->water + 5); sim_log(s, SIME_WEATHER, 0, SIMW_RAIN, 0); break;
    case GE_IDEA:    s->ideas = cap100(s->ideas + a); if(s->nfolk){ int w = (int)(r % (uint32_t)s->nfolk); if(s->folk[w].skill < 250) s->folk[w].skill = (uint8_t)(s->folk[w].skill + 8); sim_log(s, SIME_IDEA, w, 0, 0); } break;
    case GE_HILL:    s->hills = cap100(s->hills + a); s->shelter = cap100(s->shelter + 3 * a); break;
    case GE_FOREST:  s->forest = cap100(s->forest + a); s->fertility = cap100(s->fertility + 2 * a); break;
    case GE_SPECIES: if(s->nspecies < SIM_MAXSPECIES){ uint32_t sd = (uint32_t)(sim_rng(s->seed, 0, RP_SPECIES, (uint64_t)s->nspecies) >> 8); s->species_seed[s->nspecies++] = sd; sim_log(s, SIME_SPECIES, 0, (int)(sd >> 16), (int)(sd & 0xFFFF)); } break;
    case GE_FEED:    all_need(s, SIMN_HUNGER, a); break;
    case GE_REST:    all_need(s, SIMN_REST, a); break;
    case GE_SOCIAL:  all_need(s, SIMN_SOCIAL, a); break;
    case GE_FUN:     all_need(s, SIMN_FUN, a); break;
    case GE_WATER:   s->water = cap100(s->water + a); s->fertility = cap100(s->fertility + a / 4); break;
    case GE_SHELTER: s->shelter = cap100(s->shelter + a); break;
    case GE_COMFORT: s->comfort = cap100(s->comfort + a); break;
    case GE_HEALTH:  s->health = cap100(s->health + a); break;
    case GE_LUCK:    s->luck = cap100(s->luck + a); break;
    case GE_SKILL:   for(int i = 0; i < s->nfolk; i++) if(s->folk[i].skill < 250) s->folk[i].skill = (uint8_t)(s->folk[i].skill + 4 * a); break;
    case GE_PESTS:   s->pests = (uint8_t)(s->pests > a ? s->pests - a : 0); break;
    case GE_BIRDS:   s->birds = cap100(s->birds + a); all_need(s, SIMN_FUN, 20); break;
    case GE_FOOD:    s->larder += (uint32_t)a; break;
    case GE_PEOPLE:  if(s->nfolk < SIM_MAXFOLK){ int i = s->nfolk++; new_folk(s, i, 0, 1000 + (uint32_t)s->tick % 977); SimFolk *f = &s->folk[i]; f->task = (uint8_t)sched(folk_hour(s, f)); f->place = (uint8_t)place_of(f, f->task); sim_log(s, SIME_NEWFOLK, i, 0, 0); } break;
    case GE_DAY:     { uint64_t next = (s->tick / s->day_ticks + 1) * s->day_ticks; s->tick = next; all_need(s, SIMN_REST, 100); } break;
    case GE_LEGEND:  s->legends = cap100(s->legends + a); sim_log(s, SIME_LEGEND, 0, a, 0); all_need(s, SIMN_FUN, 10 * a); break;
    case GE_MOOD:    all_need(s, SIMN_SOCIAL, 3 * a); all_need(s, SIMN_FUN, 3 * a); break;
    case GE_ORE:     s->ore = cap100(s->ore + a); break;
    }
    sim_log(s, SIME_CAST, 0, n, 0);
    return 0;
}
void sim_check_unlocks(Sim *s, const Lv *level){
    int out[GOD_MAX];
    int c = god_check(&s->god, level, out, GOD_MAX);
    for(int i = 0; i < c; i++) sim_log(s, SIME_UNLOCK, 0, out[i], 0);
}
void sim_set_sandbox(Sim *s, int on, const Lv *level){
    int was = s->god.sandbox; s->god.sandbox = on ? 1 : 0;
    if(on != was) sim_log(s, SIME_SANDBOX, 0, on ? 1 : 0, 0);
    if(on) sim_check_unlocks(s, level);
}

// ---------------------------------------------------------------- hash and dump
typedef struct { uint64_t h; } H;
static void hb(H *x, uint8_t v){ x->h = (x->h ^ v) * 0x100000001B3ull; }
static void h16(H *x, uint16_t v){ hb(x, (uint8_t)v); hb(x, (uint8_t)(v >> 8)); }
static void h32(H *x, uint32_t v){ h16(x, (uint16_t)v); h16(x, (uint16_t)(v >> 16)); }
static void h64(H *x, uint64_t v){ h32(x, (uint32_t)v); h32(x, (uint32_t)(v >> 32)); }
uint64_t sim_hash(const Sim *s){
    H x = { 0xCBF29CE484222325ull };
    h64(&x, s->seed); h64(&x, s->tick); h32(&x, s->day_ticks); hb(&x, s->acc); h32(&x, (uint32_t)s->nfolk);
    for(int i = 0; i < s->nfolk; i++){ const SimFolk *f = &s->folk[i];
        for(int k = 0; f->name[k]; k++) hb(&x, (uint8_t)f->name[k]);
        hb(&x, f->job); hb(&x, f->home); hb(&x, f->task); hb(&x, f->place); hb(&x, f->shift); for(int k = 0; k < SIMN_N; k++) hb(&x, f->need[k]); hb(&x, f->warned); hb(&x, f->skill); h16(&x, f->pal); }
    hb(&x, s->fertility); hb(&x, s->water); hb(&x, s->shelter); hb(&x, s->comfort); hb(&x, s->health); hb(&x, s->luck); hb(&x, s->pests); hb(&x, s->forest);
    hb(&x, s->hills); hb(&x, s->birds); hb(&x, s->ideas); hb(&x, s->legends); hb(&x, s->nspecies); hb(&x, s->ore); hb(&x, s->weather); h16(&x, s->weather_left);
    h32(&x, s->larder); h16(&x, s->spirit); for(int k = 0; k < SIM_MAXSPECIES; k++) h32(&x, s->species_seed[k]);
    uint8_t gb[GOD_STATE_BYTES]; god_state_to_bytes(&s->god, gb); for(int k = 0; k < GOD_STATE_BYTES; k++) hb(&x, gb[k]);
    { int fs = 0; for(int k = 0; k < s->nfolk; k++) fs += s->friend[k]; if(fs){ for(int k = 0; k < s->nfolk; k++){ hb(&x, s->friend[k]); h32(&x, s->talked[k]); } } }
    if(s->env_on){ hb(&x, 1); hb(&x, s->locked); h16(&x, (uint16_t)s->temp_c); }
    h32(&x, s->day_seen); h32(&x, s->log_n); uint32_t cnt = sim_log_count(s);
    for(uint32_t b = 0; b < cnt; b++){ const SimEvent *e = sim_log_at(s, b); h32(&x, e->tick); h16(&x, e->kind); h16(&x, e->who); h16(&x, e->a); h16(&x, e->b); h32(&x, e->seed32); }
    return x.h;
}
size_t sim_dump(const Sim *s, char *o, size_t n){
    size_t at = 0; char t[160]; if(!n) return 0; o[0] = 0;
    snprintf(t, sizeof t, "sim tick %llu day %d hour %d weather %s larder %u spirit %u folk %d log %u\n", (unsigned long long)s->tick, sim_day(s), sim_hour(s), sim_weather_name(s->weather), s->larder, s->spirit, s->nfolk, s->log_n);
    at = cat(o, n, at, t);
    snprintf(t, sizeof t, "land fert %u water %u shelter %u comfort %u health %u luck %u pests %u forest %u hills %u birds %u ideas %u legends %u species %u ore %u\n", s->fertility, s->water, s->shelter, s->comfort, s->health, s->luck, s->pests, s->forest, s->hills, s->birds, s->ideas, s->legends, s->nspecies, s->ore);
    at = cat(o, n, at, t);
    for(int i = 0; i < s->nfolk; i++){ const SimFolk *f = &s->folk[i];
        snprintf(t, sizeof t, "folk %d %s job %s task %s place %d hunger %u rest %u social %u fun %u skill %u\n", i, f->name, sim_job_name(f->job), sim_task_name(f->task), f->place, f->need[0], f->need[1], f->need[2], f->need[3], f->skill);
        at = cat(o, n, at, t); }
    uint32_t cnt = sim_log_count(s); if(cnt > 5) cnt = 5;
    for(uint32_t b = 0; b < cnt; b++){ char tx[80]; sim_event_text(s, sim_log_at(s, b), tx, sizeof tx); at = cat(o, n, at, "event "); at = cat(o, n, at, tx); at = cat(o, n, at, "\n"); }
    return at;
}

// ---------------------------------------------------------------- the side file
uint32_t sim_fnv(const void *p, size_t n){ const uint8_t *b = p; uint32_t h = 2166136261u; while(n--){ h ^= *b++; h *= 16777619u; } return h; }
int sim_talk_to(Sim *s, int k){
    if(k < 0 || k >= s->nfolk) return -1;
    uint32_t today = (uint32_t)sim_day(s) + 1; if(s->talked[k] == today) return -1;
    s->talked[k] = today; int v = s->friend[k] + 8 + (int)sim_rngn(s->seed, (uint32_t)k, RP_FRIEND, today, 5); if(v > 100) v = 100;
    int before = s->friend[k]; s->friend[k] = (uint8_t)v;
    if(!before) sim_log(s, SIME_FRIEND, k, 0, 0);
    for(int st = 1; st <= 4; st++) if(before < st * 25 && v >= st * 25){ sim_log(s, SIME_LETTER, k, st, 0); if(st >= 2) sim_log(s, SIME_FRIEND, k, st * 25, 0); }
    return v;
}
int sim_letter_text(const Sim *s, int who, int step, char *out, size_t n){
    static const char *const L[4][3] = { { "I AM GLAD YOU STOPPED BY", "COME BACK ANY DAY", "THE TOWN LIKES SEEING YOU" },
        { "THERE IS A SEAT FOR YOU AT MY TABLE", "I SAVED YOU THE BEST FRUIT", "YOU MAKE THE DAYS BRIGHTER" },
        { "I TELL EVERYONE ABOUT YOU", "MY HOME IS YOUR HOME", "I WOULD WALK FAR FOR YOU" },
        { "YOU ARE MY DEAREST FRIEND", "NOTHING WILL EVER BREAK THIS", "THANK YOU FOR EVERYTHING" } };
    if(step < 1) step = 1; if(step > 4) step = 4; (void)n;
    const char *t = L[step - 1][sim_rngn(s->seed, (uint32_t)who, RP_FRIEND, 1000u + (uint32_t)step, 3)];
    size_t i = 0; while(t[i] && i + 1 < n){ out[i] = t[i]; i++; } out[i] = 0; return (int)i;
}
void sim_save_friends(const Sim *s, SimSlot *sl){
    uint32_t town = (uint32_t)sim_mix64(s->seed ^ 0xF00DF00Du);
    int m = 0; for(int i = 0; i < sl->nfr; i++) if(sl->fr[i].town != town){ if(m != i) sl->fr[m] = sl->fr[i]; m++; }
    for(int k = 0; k < s->nfolk && m < SIM_FRMAX; k++) if(s->friend[k]){ sl->fr[m].town = town; sl->fr[m].who = (uint8_t)k; sl->fr[m].val = s->friend[k]; m++; }
    sl->nfr = (uint8_t)m;
}
void sim_load_friends(Sim *s, const SimSlot *sl){
    uint32_t town = (uint32_t)sim_mix64(s->seed ^ 0xF00DF00Du);
    for(int i = 0; i < sl->nfr; i++) if(sl->fr[i].town == town && sl->fr[i].who < s->nfolk) s->friend[sl->fr[i].who] = sl->fr[i].val > 100 ? 100 : sl->fr[i].val;
}
void sim_save_tail(const Sim *s, SimSlot *sl){
    uint32_t cnt = sim_log_count(s); if(cnt > SIM_TAILN) cnt = SIM_TAILN;
    sl->ntail = (uint16_t)cnt;
    for(uint32_t i = 0; i < cnt; i++) sl->tail[i] = *sim_log_at(s, cnt - 1 - i);          // oldest first
}
void sim_load_tail(Sim *s, const SimSlot *sl){
    for(uint16_t i = 0; i < sl->ntail && i < SIM_TAILN; i++){ s->log[s->log_head] = sl->tail[i]; s->log_head = (s->log_head + 1) % SIM_LOGN; s->log_n++; }
}
void simfile_init(SimFile *f){
    memset(f, 0, sizeof *f); f->side_on = 1; f->difficulty = COLD_NORMAL;
    for(int i = 0; i < SIM_SLOTS; i++){ god_state_init(&f->slot[i].god); lv_zero(&f->slot[i].lv_true); lv_zero(&f->slot[i].lv_xp); }
}
static void p16(uint8_t *b, uint16_t v){ b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static void p32(uint8_t *b, uint32_t v){ p16(b, (uint16_t)v); p16(b + 2, (uint16_t)(v >> 16)); }
static void p64(uint8_t *b, uint64_t v){ p32(b, (uint32_t)v); p32(b + 4, (uint32_t)(v >> 32)); }
static uint16_t g16(const uint8_t *b){ return (uint16_t)(b[0] | (b[1] << 8)); }
static uint32_t g32(const uint8_t *b){ return g16(b) | ((uint32_t)g16(b + 2) << 16); }
static uint64_t g64(const uint8_t *b){ return g32(b) | ((uint64_t)g32(b + 4) << 32); }
#define CH_HDR 14
static size_t put_chunk(uint8_t *buf, size_t cap, size_t at, const char *tag, int slot, int ver, const uint8_t *d, size_t len){
    if(at + CH_HDR + len > cap) return (size_t)-1;
    memcpy(buf + at, tag, 4); buf[at + 4] = (uint8_t)slot; buf[at + 5] = (uint8_t)ver; p32(buf + at + 6, (uint32_t)len); p32(buf + at + 10, sim_fnv(d, len));
    memcpy(buf + at + CH_HDR, d, len); return at + CH_HDR + len;
}
size_t simfile_write(const SimFile *f, uint8_t *buf, size_t cap){
    size_t at = 0; uint8_t d[SIM_TAILN * 16 + 2 + 2 * LV_BYTES + 1];
    if(cap < 8) return 0;
    memcpy(buf, "BSIM", 4); p32(buf + 4, 1); at = 8;
#define PUT(tag, slot, ver, len) do{ at = put_chunk(buf, cap, at, tag, slot, ver, d, len); if(at == (size_t)-1) return 0; }while(0)
    d[0] = f->side_on; d[1] = f->sandbox; d[2] = f->difficulty; PUT("META", 0xFF, 1, 3);
    for(int s = 0; s < SIM_SLOTS; s++){ const SimSlot *sl = &f->slot[s]; if(!sl->used) continue;
        p64(d, sl->cal_ticks); PUT("CLCK", s, 1, 8);
        god_state_to_bytes(&sl->god, d); p16(d + GOD_STATE_BYTES, sl->spirit); PUT("GODS", s, 1, GOD_STATE_BYTES + 2);
        if(sl->has_lv){ d[0] = 1; lv_to_bytes(&sl->lv_true, d + 1); lv_to_bytes(&sl->lv_xp, d + 1 + LV_BYTES); PUT("LEVL", s, 1, 1 + 2 * LV_BYTES); }
        if(sl->nfr){ d[0] = sl->nfr; for(int i = 0; i < sl->nfr; i++){ p32(d + 1 + i * 6, sl->fr[i].town); d[5 + i * 6] = sl->fr[i].who; d[6 + i * 6] = sl->fr[i].val; } PUT("FRND", s, 1, 1 + (size_t)sl->nfr * 6); }
        if(sl->cols.n){ size_t m = col_list_write(&sl->cols, d, sizeof d); if(m) PUT("COLN", s, 1, m); }
        p16(d, sl->ntail); for(int i = 0; i < sl->ntail; i++){ const SimEvent *e = &sl->tail[i]; uint8_t *q = d + 2 + i * 16; p32(q, e->tick); p16(q + 4, e->kind); p16(q + 6, e->who); p16(q + 8, e->a); p16(q + 10, e->b); p32(q + 12, e->seed32); }
        PUT("TAIL", s, 1, 2 + (size_t)sl->ntail * 16);
    }
#undef PUT
    return at;
}
int simfile_read(SimFile *f, const uint8_t *buf, size_t n){
    simfile_init(f);
    if(n < 8 || memcmp(buf, "BSIM", 4) || g32(buf + 4) != 1) return -1;
    size_t at = 8;
    while(at + CH_HDR <= n){
        const uint8_t *h = buf + at; uint32_t len = g32(h + 6); int slot = h[4], ver = h[5];
        if(len > n - at - CH_HDR){ f->bad_chunks++; break; }                // cut off: stop here, keep what we have
        const uint8_t *d = h + CH_HDR; at += CH_HDR + len;
        if(sim_fnv(d, len) != g32(h + 10)){ f->bad_chunks++; continue; }
        if(ver != 1) continue;
        if(!memcmp(h, "META", 4)){ if(len >= 2){ f->side_on = d[0] ? 1 : 0; f->sandbox = d[1] ? 1 : 0; } if(len >= 3) f->difficulty = d[2] > COLD_HARSH ? COLD_NORMAL : d[2]; continue; }
        if(slot >= SIM_SLOTS) continue;
        SimSlot *sl = &f->slot[slot];
        if(!memcmp(h, "CLCK", 4) && len == 8){ sl->used = 1; sl->cal_ticks = g64(d); }
        else if(!memcmp(h, "GODS", 4) && len == GOD_STATE_BYTES + 2){ sl->used = 1; god_state_from_bytes(&sl->god, d); sl->spirit = g16(d + GOD_STATE_BYTES); if(sl->spirit > SIM_SPIRIT_MAX) sl->spirit = SIM_SPIRIT_MAX; }
        else if(!memcmp(h, "LEVL", 4) && len == 1 + 2 * LV_BYTES && d[0]){ sl->used = 1; sl->has_lv = 1; lv_from_bytes(&sl->lv_true, d + 1); lv_from_bytes(&sl->lv_xp, d + 1 + LV_BYTES); }
        else if(!memcmp(h, "FRND", 4) && len >= 1){ int c = d[0]; if(c > SIM_FRMAX || len != 1u + (uint32_t)c * 6) { f->bad_chunks++; continue; }
            sl->used = 1; sl->nfr = (uint8_t)c; for(int i = 0; i < c; i++){ sl->fr[i].town = g32(d + 1 + i * 6); sl->fr[i].who = d[5 + i * 6]; sl->fr[i].val = d[6 + i * 6] > 100 ? 100 : d[6 + i * 6]; } }
        else if(!memcmp(h, "COLN", 4)){ if(col_list_read(&sl->cols, d, len) < 0){ f->bad_chunks++; continue; } sl->used = 1; }
        else if(!memcmp(h, "TAIL", 4) && len >= 2){ uint16_t c = g16(d); if(c > SIM_TAILN || len != 2u + (uint32_t)c * 16) { f->bad_chunks++; continue; }
            sl->used = 1; sl->ntail = c; for(int i = 0; i < c; i++){ const uint8_t *q = d + 2 + i * 16; SimEvent *e = &sl->tail[i]; e->tick = g32(q); e->kind = g16(q + 4); e->who = g16(q + 6); e->a = g16(q + 8); e->b = g16(q + 10); e->seed32 = g32(q + 12); } }
    }
    return 0;
}
