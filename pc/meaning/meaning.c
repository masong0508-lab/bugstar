// meaning.c: the MEANING LAYER (see meaning.h). Pure functions, no state, no libm.
#include <string.h>
#include <stdio.h>
#include "meaning.h"
#include "../content/flora.h"
#include "../laws/laws.h"
#include "../sim/god.h"

static int clampi(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }
// ------------------------------------------------------------------ channels and tags
static const char *const ch_nm[MN_NCH] = {"GRAVITY","LIGHT","HEAT","WEATHER","FOOD","WATER","ORE","DANGER","SPEED","TRADE","KNOWLEDGE","FAITH","HEALTH","LUCK","MAGIC",
  "GROWTH","SHELTER","FUEL","CRAFT","DEFENCE","CROWD","TAMING","STEALTH","AIR","TIME","BEAUTY","SOUND","TINT","MOOD","GLOW","SCENT","LORE"};
static const char *const ch_up[MN_NCH] = {"HEAVIER PULL","BRIGHTER DAYS","HOTTER","HARSHER WEATHER","MORE FOOD","MORE WATER","RICHER ORE","MORE DANGER","FASTER TRAVEL","BETTER TRADE","MORE LEARNING","STRONGER FAITH",
  "HEALTHIER","LUCKIER","MORE MAGIC","FASTER GROWTH","SAFER HOMES","MORE FUEL","BETTER CRAFTS","STRONGER WALLS","MORE PEOPLE","EASIER TAMING","EASIER HIDING","CLEANER AIR","LONGER DAYS",
  "PRETTIER","MORE SONG","WARMER COLOURS","CHEERFUL MOOD","MORE GLOW","SWEETER SMELL","MORE STORIES"};
static const char *const ch_dn[MN_NCH] = {"LIGHTER PULL","DARKER DAYS","COLDER","MILDER WEATHER","LESS FOOD","LESS WATER","POORER ORE","SAFER","SLOWER TRAVEL","WORSE TRADE","LESS LEARNING","WEAKER FAITH",
  "SICKLIER","UNLUCKIER","LESS MAGIC","SLOWER GROWTH","POORER HOMES","LESS FUEL","WORSE CRAFTS","WEAKER WALLS","FEWER PEOPLE","HARDER TAMING","HARDER HIDING","FOULER AIR","SHORTER DAYS",
  "PLAINER","MORE SILENCE","COOLER COLOURS","GLOOMY MOOD","LESS GLOW","FOULER SMELL","FEWER STORIES"};
static const char *const look_up[7] = {"LOVELY","FULL OF SONG","WARM COLOURED","CHEERFUL","GLOWING","SWEET SMELLING","RICH IN STORIES"};
static const char *const look_dn[7] = {"PLAIN","HUSHED","COOL COLOURED","GLOOMY","DARK","FOUL SMELLING","UNSTORIED"};
static const char *const tag_nm[MN_NTAG] = {"SEA","FOREST","DESERT","ICE","FIRE","SKY","CAVE","MARSH","STONE","STORM","NIGHT","SUN","MAGIC","OLD","LUSH","BARREN","HEAVY","AIRY","TRADE","WILD","CALM","SONG","CRAFT","FAITH"};
static const char SIG[] = "fpjqrigmtdwbokegaxjhmAsl";             // the rule channel a tag strengthens when items on it agree
static const char COSC[] = "AEBBDCCEFADBDFzzCCFACAzF";            // the look channel a tag colours
static const char COSS[] = "+++-++--++++++--++++++++";
static const char *const opp_pairs[] = {"ac","ae","de","bc","hc","kl","op","qr","tu","ju","fg","sp","he"};
const char *mn_ch_name(int c){ return c >= 0 && c < MN_NCH ? ch_nm[c] : "?"; }
const char *mn_tag_name(int t){ return t >= 0 && t < MN_NTAG ? tag_nm[t] : "?"; }
int mn_ch_is_rule(int c){ return c >= 0 && c < MN_RULE_CH; }
const char *mn_role_name(int r){ return r == MN_PURPOSE ? "PURPOSE" : r == MN_COSMETIC ? "COSMETIC" : "BOTH"; }
static int chl(char c){ return c >= 'a' && c <= 'y' ? c - 'a' : c == 'z' ? 25 : c >= 'A' && c <= 'F' ? 26 + c - 'A' : -1; }
static int tgl(char c){ return c >= 'a' && c <= 'x' ? c - 'a' : -1; }
static uint32_t tgm(char c){ int t = tgl(c); return t < 0 ? 0 : 1u << t; }
static int rolel(char c){ return c == 'P' ? MN_PURPOSE : c == 'C' ? MN_COSMETIC : MN_BOTH; }

// ------------------------------------------------------------------ part tables: roles, two tags, one channel, one amount digit ('5' = 0, '6' = +1, '8' = +3, '2' = -3)
typedef struct { int n; const char *role, *t1, *t2, *ch, *am; } PT;
#define U6 "6666666666666666666666666666666666"
static const PT pt_gift = {16, "PPPPBPPPCPPBPPBP", "kwaivrujvtbkmsme", "----u-o----m--xl", "bsftvwmdAhpbignr", "6666666266666666"};
static const PT pt_ccol = {16, "BCCBBCCCCCBCCCCC", "beadklomcabdemkl", "----------------", "wBBwwBBBBBwBBBBB", "6666666666666666"};
static const PT pt_rank = {8, "BPPPPPPP", "uootnqtn", "----u--x", "vvhhkthk", "66666666"};
static const PT pt_mut  = {16, "CPBPCPBCPCBCBPCB", "ukmfrkmwffxwvdbm", "----------------", "zktizwhzizlzvczg", "6666666666666266"};
static const PT pt_plan = {66, "PPCBPPPCPPP" "CPBPCCCPPBP" "PBCPPCCPPPP" "CPPPPBCBCPP" "PCCBPCCPBPP" "PPPPBPCPPPP",
  "ihklcgbhuwl" "mtwjnhvjekr" "tkuoqsmhtko" "vwhtbklwubg" "evvkejajkba" "bwihhwiqkvg", "", "", ""};
static const PT pt_size = {4, "PPPP", "rotq", "u--t", "wvhh", "6668"};
static const PT pt_diet = {3, "PPP", "otn", "u-p", "ehm", "666"};
static const PT pt_hab  = {4, "PPPP", "higa", "ofks", "mtgf", "6666"};
static const PT pt_beh  = {4, "PPPP", "utor", "-q--", "vhui", "6766"};
static const PT pt_stock= {32, "BPPPPPCPPCBPCPPPBPPCPBBCCPPPBPPC", "", "", "", ""};
static const char *const stock_tags = "bl" "wn" "ah" "ek" "bu" "hf" "wm" "cs" "gu" "fi" "av" "gi" "km" "tb" "dq" "tw" "wf" "ni" "ot" "au" "cl" "ko" "sv" "iu" "bu" "ca" "ob" "ah" "kn" "wo" "wn" "km";
static const PT pt_way  = {16, "PCPPPPPCPPPPPPCC", "", "", "skseejtFgemqihAw", "6666666766666676"};
static const char *const way_tags = "hw" "kn" "we" "ou" "ao" "sc" "qi" "vn" "gi" "ob" "bu" "fi" "af" "tb" "vx" "uc";
static const PT pt_form = {16, "PPCPBPPPCBPCPBPC", "iaagbfiscfaawwcx", "oshifiq-uwsrolsn", "", ""};
static const PT pt_char = {16, "CBPPCPPCPPCBPPPC", "vkfsusnoxpvkwtgo", "-ljl----q-unmiku", "AbdjCukpteCFshwz", "6666666662666666"};
static const PT pt_fuse = {10, "PPPPBCBPPC", "ouebwwsrou", "-m-q------", "emrqsBjxpz", "6666666666"};
static const PT pt_fhab = {14, "PPPPPPPPPPPPPP", "obhacidcpgfabe", "--------------", "epmfctcgrwiefr", "66666666666666"};
static const PT pt_fq   = {16, "CCCCCPPPCPCPBPCC", "kvualctemidutrvb", "----------------", "DACEBfhcBqDmhxAF", "6666666666666666"};
static const PT pt_ffm  = {32, "PPPPPPPPPPPPPPPPPCPPPPPPPPPPPPPP", "hiohbbbcotbaccgaaouodotbihfaabbh", "--------------------------------", "ppppppppppppppppp" "z" "pppppppppppppp", ""};
static const PT pt_fleaf= {16, "PPPPCPPCCCPPPPCC", "dbafubcmumncahsw", "----------------", "xxxxzxxzzzxxxxzz", ""};
static const PT pt_ffl  = {16, "CCCCCCCCCCCCCCCC", "vkhtfxwgumvkwokf", "----------------", "zDzzzFzEzDAzzzDz", ""};
static const PT pt_ffr  = {16, "PPPCPPPPPPPPPBPC", "obbfoocdoocpokap", "----------------", "eeezeeeeeeeeeeez", ""};
static const PT pt_fbk  = {12, "PPPCPCPPCPCP", "ricnedbtwcwd", "------------", "qqqzqzqqzqzq", ""};
static const PT pt_fcol = {16, "CCCCCCCCCCCCCCCC", "eelobaakmoucddkm", "----------------", "BBBBBBBBBBBBBBBB", ""};
static const PT pt_fsz  = {6, "PPPPPP", "roubbq", "------", "pppqqq", "556678"};
// trait flavours: 9 categories x 15 words, a tag each
static const char *const flav_tag[9] = {"urrifgauqmmivfa","jutlkcdiklljswu","ohsedcwnbavmtub","ujbjhdellkjmuaq","mabmigithocwcur","umafklplufhqrea","utvuwkrqutvguwo","pgwbavgefmisgnr","pmjufdevhlbgugj"};
// 65 base traits: role, two tags
static const char tb_role[] = "PPCPP" "PPPBP" "PPCBPPC" "BPPPPPPB" "PPBBBPPPBPPP" "CCBPPCC" "PBPPPPPP" "PPPCBC" "PBPPPPC";
// (tags written below as two-letter pairs per trait so the table reads in order)
static const char *const tb_tags = "r-" "r-" "u-" "q-" "qi" "kl" "jf" "ul" "ln" "kd" "fp" "qh" "lh" "km" "jh" "rf" "cj" "lu" "j-" "ep" "jf" "jh" "d-" "cj" "hk"
  "a-" "af" "cl" "im" "bh" "et" "d-" "ci" "fm" "cp" "bq" "cp" "fm" "kf" "le" "el" "kp" "fj" "mf" "to" "kp" "ou" "tq" "or" "t-" "uo" "o-" "ni" "nw" "ns" "vi" "xn" "p-" "m-" "mj" "pn" "ei" "eh" "jm" "u-";
// ------------------------------------------------------------------ themes
typedef struct { char role, t1, t2, c1, a1, c2, a2; } Th;       // amounts are x2: '7' = +4
static const Th th_t[24] = {
 {'P','f','-','a','2','i','7'},{'P','q','-','a','8','i','3'},{'P','p','-','o','2','h','6'},{'P','m','j','o','7','n','3'},{'P','t','q','h','7','i','3'},{'P','t','r','h','6','u','7'},
 {'B','k','m','b','2','D','7'},{'B','l','e','b','7','c','7'},{'P','s','-','j','7','g','7'},{'P','p','-','j','3','g','3'},{'P','t','w','h','8','m','3'},{'P','o','-','e','8','m','7'},
 {'P','p','-','e','2','m','3'},{'C','m','-','F','7','B','7'},{'P','l','-','y','3','b','7'},{'P','f','s','i','3','r','8'},{'P','f','s','i','7','r','3'},{'P','n','f','g','8','k','7'},
 {'B','u','-','h','3','j','3'},{'P','t','s','h','8','j','7'},{'P','m','-','n','8','m','6'},{'P','t','q','h','7','k','7'},{'B','u','-','h','3','k','3'},{'B','o','u','m','7','v','7'}};
// ------------------------------------------------------------------ gods: effect kind -> role, tag, channel, sign
static const struct { char role, tag, ch, sg; } ge_t[GE_NEFFECT] = {
 {'P','o','p','+'},{'P','l','d','-'},{'P','a','f','+'},{'P','n','k','+'},{'P','i','t','+'},{'P','b','p','+'},{'P','o','v','+'},{'P','o','e','+'},{'P','u','m','+'},
 {'P','s','j','+'},{'C','v','C','+'},{'P','a','f','+'},{'P','i','q','+'},{'B','u','m','+'},{'P','u','m','+'},{'P','m','n','+'},{'P','w','s','+'},{'P','u','h','-'},
 {'C','f','A','+'},{'P','o','e','+'},{'P','o','u','+'},{'P','l','b','+'},{'C','n','F','+'},{'C','u','C','+'},{'P','i','g','+'}};
// ------------------------------------------------------------------ laws: explicit rows, category defaults for the rest
static const struct { const char *key; char role, tag, ch, sg; } lw_t[] = {
 {"U_GRAV",'P','q','a','+'},{"U_LIGHT",'P','f','i','+'},{"U_EXPAND",'P','f','i','-'},{"U_DARKM",'C','m','F','+'},{"U_DARKE",'P','f','r','-'},{"U_AGE",'P','n','k','+'},{"U_FUSION",'P','l','b','+'},
 {"U_ELEC",'P','w','s','+'},{"U_RADIO",'P','e','m','-'},{"U_CMB",'P','d','c','+'},{"U_COSMIC",'P','m','m','-'},{"U_SPACING",'B','f','i','-'},{"U_CLUMP",'C','i','z','+'},{"U_MATTER",'P','i','g','+'},
 {"U_GALBITS",'C','f','F','+'},{"U_VARIETY",'B','m','n','+'},{"U_STRONG",'P','i','t','+'},{"G_BH",'B','k','h','+'},{"G_BHJET",'B','k','h','+'},{"G_DUST",'B','k','b','-'},{"G_GAS",'B','o','p','+'},
 {"G_COLLIDE",'B','j','h','+'},{"G_SUPERN",'B','e','m','-'},{"G_METAL",'P','i','g','+'},{"G_GRAD",'P','i','g','+'},{"G_CROWD",'P','s','u','+'},{"G_HABZONE",'P','u','m','+'},
 {"S_O",'P','l','b','+'},{"S_B",'P','l','b','+'},{"S_A",'P','l','b','+'},{"S_F",'P','l','b','+'},{"S_G",'P','u','m','+'},{"S_K",'P','k','b','-'},{"S_M",'P','k','b','-'},{"S_WD",'P','k','b','-'},
 {"S_GIANT",'P','e','c','+'},{"S_NEUT",'P','t','h','+'},{"S_SFR",'C','n','F','+'},{"S_MASS",'C','n','F','+'},{"S_TEMP",'P','e','c','+'},{"S_LIFE",'P','u','y','+'},{"S_FLARE",'P','e','h','+'},
 {"S_FLICK",'B','k','b','-'},{"S_BIN",'B','l','b','+'},{"S_MAGNET",'P','m','o','+'},{"S_WIND",'P','j','d','+'},{"S_HZ",'P','u','m','+'},{"S_HZPOS",'P','d','c','-'},{"S_YOUNG",'C','n','F','+'},
 {"S_HOME",'C','n','F','+'},{"S_TRIPLE",'C','l','z','+'},{"S_ECL",'C','k','D','+'},
 {"P_SIZE",'P','q','a','+'},{"P_SPREAD",'C','i','F','+'},{"P_DENS",'P','q','a','+'},{"P_PERSTAR",'P','s','u','+'},{"P_MAXPL",'C','f','F','+'},{"P_MOONS",'C','k','D','+'},{"P_MAXMOON",'C','f','F','+'},
 {"P_ORBSP",'C','f','F','+'},{"P_ORB0",'P','d','c','-'},{"P_ECC",'P','j','d','+'},{"P_TILT",'P','j','d','+'},{"P_ATMO",'P','r','x','+'},{"P_GREEN",'P','e','c','+'},{"P_ALBEDO",'B','l','b','+'},
 {"P_OXY",'P','r','x','+'},{"P_WATER",'P','a','f','+'},{"P_ICE",'P','d','c','-'},{"P_CLOUD",'P','h','b','-'},{"P_WIND",'P','j','d','+'},{"P_STORM",'P','j','d','+'},{"P_MOUNT",'P','i','t','+'},
 {"P_ROUGH",'P','i','i','-'},{"P_VOLC",'B','e','r','+'},{"P_QUAKE",'P','i','q','-'},{"P_MAGNET",'P','m','o','+'},{"P_CORE",'P','e','c','+'},{"P_NEEDMET",'C','i','F','+'},{"P_ORE",'P','i','g','+'},
 {"P_GEMS",'B','i','j','+'},{"P_ROGUE",'C','k','F','+'},{"P_TIDAL",'P','k','y','-'},{"P_MOONSZ",'C','k','D','+'},{"P_GAS",'C','r','z','+'},{"P_AURORA",'B','k','D','+'},{"P_DUNE",'C','c','z','+'},
 {"P_SNOWLN",'P','d','c','-'},{"P_HELL",'P','t','h','+'},
 {"L_CHANCE",'P','o','e','+'},{"L_SMART",'P','n','k','+'},{"L_FAUNA",'P','u','v','+'},{"L_FLORA",'P','b','p','+'},{"L_BUGS",'P','t','h','+'},{"L_CSIZE",'P','q','h','+'},{"L_ANGER",'P','t','h','+'},
 {"L_RARE",'B','m','n','+'},{"L_LIFESPAN",'P','u','m','+'},{"L_FRIEND",'P','s','j','+'},{"L_WEIRD",'B','m','n','+'},{"L_CITY",'P','s','u','+'},{"L_RUINS",'P','n','g','+'},{"L_FOSSIL",'C','n','F','+'},
 {"L_HUNGER",'P','p','e','-'},{"L_DISEASE",'P','h','m','-'},{"L_EVOLVE",'C','n','F','+'},{"L_BREATH",'P','r','x','-'},{"L_TOLER",'P','u','m','+'},{"L_NIGHT",'B','k','w','+'},{"L_SEA",'P','a','e','+'},
 {"T_DAY",'P','l','y','+'},{"T_ORBIT",'C','f','F','+'},{"T_SPINDN",'C','f','F','+'},{"T_WOBBLE",'C','f','F','+'},{"T_SEASON",'P','j','d','+'},{"T_SWING",'P','e','c','+'},{"T_ICEAGE",'P','d','c','-'},
 {"T_NIGHT",'P','k','b','-'},{"T_DUSK",'C','k','B','+'},{"T_PLAGE",'C','n','F','+'},{"T_MOONORB",'C','k','D','+'},{"T_WEATHER",'P','j','d','+'},{"T_METEOR",'P','j','h','+'},{"T_SOLSTICE",'C','l','F','+'},
};
#define NLWT ((int)(sizeof lw_t / sizeof lw_t[0]))
static const struct { char role, tag, ch; } lw_cat[LC_N] = {{'P','n','k'},{'C','f','z'},{'P','l','b'},{'P','i','p'},{'P','o','e'},{'P','u','y'}};
// ------------------------------------------------------------------ stats
static const uint16_t st_p[7] = {0x36E7, 0x35E7, 0x15EF, 0x3EE7, 0x36E7, 0x9107, 0x18EF}, st_b[7] = {0xC000, 0x0808, 0x2800, 0x4100, 0x0110, 0x0600, 0xE010};
static const char st_zt[] = "oiaaadcbsnghoefr", st_famcos[] = "CBDAFEz", st_fhi[] = "qeltnso", st_flo[] = "rdkupp" "p";
static const uint8_t st_w[16] = {4,3,3,2,1,1,2,3,2,2,2,1,4,1,1,2};
static int stat_role(int f, int z){ return (st_p[f] >> z & 1) ? MN_PURPOSE : (st_b[f] >> z & 1) ? MN_BOTH : MN_COSMETIC; }
// ------------------------------------------------------------------ item building
typedef struct { int Rc[MN_NCH], Rn[MN_NCH], K[MN_NCH]; uint32_t tags; } Acc;
static void addR(Acc *a, int ch, int amt, int counted){ if(ch < 0) return; if(ch >= MN_RULE_CH) a->K[ch] += amt; else if(counted) a->Rc[ch] += amt; else a->Rn[ch] += amt; }
static void add_tags(Acc *a, uint32_t t, int pct){ a->tags |= t; for(int i = 0; i < MN_NTAG; i++) if(t >> i & 1) a->K[chl(COSC[i])] += (COSS[i] == '+' ? 2 : -2) * pct / 100; }
static int sigch(int tagbit){ int c = chl(SIG[tagbit]); return c >= MN_RULE_CH ? CH_TAMING : c; }       // (a song calms: the SONG family has no rule channel of its own)
static void sigfall(Acc *a, int tagbit, int amt){ if(tagbit >= 0) a->Rc[sigch(tagbit)] += amt; }
static const PT *ptab(int cls){
    switch(cls){ case MC_GIFT: return &pt_gift; case MC_CCOL: return &pt_ccol; case MC_RANK: return &pt_rank; case MC_MUT: return &pt_mut; case MC_PLAN: return &pt_plan;
      case MC_SIZE: return &pt_size; case MC_DIET: return &pt_diet; case MC_HAB: return &pt_hab; case MC_BEHAV: return &pt_beh; case MC_STOCK: return &pt_stock; case MC_WAY: return &pt_way;
      case MC_FORM: return &pt_form; case MC_CHAR: return &pt_char; case MC_FUSE: return &pt_fuse; case MC_FHAB: return &pt_fhab; case MC_FQUIRK: return &pt_fq; case MC_FFORM: return &pt_ffm;
      case MC_FLEAF: return &pt_fleaf; case MC_FFLOWER: return &pt_ffl; case MC_FFRUIT: return &pt_ffr; case MC_FBARK: return &pt_fbk; case MC_FCOL: return &pt_fcol; case MC_FSIZE: return &pt_fsz; }
    return 0;
}
// ------------------------------------------------------------------ parts
typedef struct { int role; uint32_t tags; int ch, amt; } Pi;
static const int fch[7] = {CH_GRAVITY, CH_HEAT, CH_LIGHT, CH_DANGER, CH_ORE, CH_TRADE, CH_FOOD};
static int part_n(int cls){
    switch(cls){ case MC_TBASE: return TR_BASE; case MC_TFLAV: return 9 * 15; case MC_STOCK: return N_STOCK; case MC_WAY: return N_WAY; }
    const PT *p = ptab(cls); return p ? p->n : 0;
}
static int part_get(int cls, int id, Pi *o){
    memset(o, 0, sizeof *o); o->ch = -1; if(id < 0 || id >= part_n(cls)) return 0;
    if(cls == MC_TBASE){ o->role = rolel(tb_role[id]); o->tags = tgm(tb_tags[2 * id]) | tgm(tb_tags[2 * id + 1]); return 1; }
    if(cls == MC_TFLAV){ int c = id / 15, k = id % 15; o->role = MN_BOTH; o->tags = tgm(flav_tag[c][k]); o->ch = fch[(k + 1 + c) % 7]; o->amt = (((k + 1) ^ c) & 1) ? 2 : -2; return 1; }
    const PT *p = ptab(cls); o->role = rolel(p->role[id]);
    if(cls == MC_STOCK) o->tags = tgm(stock_tags[2 * id]) | tgm(stock_tags[2 * id + 1]);
    else if(cls == MC_WAY) o->tags = tgm(way_tags[2 * id]) | tgm(way_tags[2 * id + 1]);
    else { o->tags = tgm(p->t1[id]); if(p->t2 && p->t2[0]) o->tags |= tgm(p->t2[id]); }
    if(p->ch && p->ch[0]){ o->ch = chl(p->ch[id]); o->amt = (p->am && p->am[0] ? p->am[id] : '6') - '5'; }
    return 1;
}
static void use_part(Acc *a, int cls, int id, int pct){            // adds a part's tags, look and (counted) rule effect
    Pi p; if(!part_get(cls, id, &p)) return;
    add_tags(a, p.tags, pct); if(p.ch >= 0) addR(a, p.ch, p.amt * pct / 100 + (p.amt && pct < 100 ? 0 : 0), 1);
}
static int first_tag(uint32_t t){ for(int i = 0; i < MN_NTAG; i++) if(t >> i & 1) return i; return -1; }
static void finish(MnItem *o, int role, Acc *a){
    int tot[MN_NCH], i, k; memset(o, 0, sizeof *o); o->role = (uint8_t)role; o->tags = a->tags;
    int ft = first_tag(a->tags);
    if(role != MN_COSMETIC){ int any = 0; for(i = 0; i < MN_RULE_CH; i++) if(a->Rc[i] + a->Rn[i]) any = 1; if(!any && ft >= 0) a->Rc[sigch(ft)] += 1; }
    if(role != MN_PURPOSE){ int any = 0; for(i = MN_RULE_CH; i < MN_NCH; i++) if(a->K[i]) any = 1; if(!any) a->K[CH_BEAUTY] = 1; }
    int nr = role == MN_PURPOSE ? MN_EFF : role == MN_BOTH ? 3 : 0, nk = role == MN_COSMETIC ? MN_EFF : role == MN_BOTH ? 3 : 0;
    for(i = 0; i < MN_NCH; i++) tot[i] = i < MN_RULE_CH ? a->Rc[i] + a->Rn[i] : a->K[i];
    for(int pass = 0; pass < 2; pass++){
        int want = pass == 0 ? nr : nk, lo = pass == 0 ? 0 : MN_RULE_CH, hi = pass == 0 ? MN_RULE_CH : MN_NCH;
        for(k = 0; k < want; k++){
            int best = -1, bv = 0; for(i = lo; i < hi; i++){ int v = tot[i] < 0 ? -tot[i] : tot[i]; if(v > bv){ bv = v; best = i; } }
            if(best < 0) break;
            o->ch[o->n] = (uint8_t)best; o->amt[o->n] = (int8_t)clampi(tot[best], -100, 100); o->cnt[o->n] = (int8_t)clampi(best < MN_RULE_CH ? a->Rc[best] : a->K[best], -100, 100); o->n++; tot[best] = 0;
        }
    }
}
static int cdiv(int a, int b){ return a >= 0 ? a / b : -((-a) / b); }

// ------------------------------------------------------------------ the whole items
static void creature_item(int id, Acc *a, int *role){
    int pl = id % N_PLAN, v = id / N_PLAN; Creature c; creature_kind(id, &c);
    Pi pp; part_get(MC_PLAN, pl, &pp); *role = pp.role;
    use_part(a, MC_PLAN, pl, 100); use_part(a, MC_SIZE, v / 48, 100); use_part(a, MC_DIET, (v / 16) % 3, 100); use_part(a, MC_HAB, (v / 4) % 4, 100); use_part(a, MC_BEHAV, v % 4, 100);
    static const int clsch[6] = {CH_FOOD, CH_SPEED, CH_WATER, CH_ORE, CH_MAGIC, CH_DANGER};
    addR(a, clsch[pl / 11], 2, 1);
    addR(a, CH_DANGER, cdiv(c.stat[2] + c.stat[3], 3) - 3, 1); addR(a, CH_SPEED, cdiv(c.stat[1] - 5, 2), 1); addR(a, CH_KNOW, cdiv(c.stat[4] - 5, 2), 1);
    addR(a, CH_TAMING, cdiv(c.stat[5] - 4, 2), 1); if(c.diet == 0) addR(a, CH_FOOD, c.stat[0] / 25, 1);
}
static void flora_item(uint32_t id, Acc *a, int *role){
    Flora f; flora_make(id, &f); Pi up; part_get(MC_FUSE, f.use, &up); *role = up.role;
    use_part(a, MC_FUSE, f.use, 100); use_part(a, MC_FFORM, f.form, 100); use_part(a, MC_FHAB, f.habitat, 100); use_part(a, MC_FQUIRK, f.quirk, 100); use_part(a, MC_FLEAF, f.leaf, 100);
    use_part(a, MC_FFLOWER, f.flower, 100); use_part(a, MC_FFRUIT, f.fruit, 100); use_part(a, MC_FBARK, f.bark, 100); use_part(a, MC_FCOL, f.colour, 100); use_part(a, MC_FSIZE, f.size, 100);
    addR(a, CH_FOOD, cdiv(f.food - 5, 3), 1); addR(a, CH_SHELTER, cdiv(f.shelter - 5, 3), 1); addR(a, CH_HEALTH, cdiv(f.medicine - 5, 3), 1); addR(a, CH_FUEL, cdiv(f.fuel - 5, 3), 1);
    addR(a, CH_AIR, cdiv(f.o2 - 7, 3), 1); addR(a, CH_GROWTH, cdiv((int)f.biomass - 30, 60), 1);
}
int mn_class_size(int cls){
    switch(cls){ case MC_TRAIT: return TR_BASE * TR_VARIANTS; case MC_STAT: return N_STAT; case MC_THEME: return 24; case MC_PEOPLE: return N_PEOPLE; case MC_CITY: return N_CITY; case MC_CREATURE: return N_KIND;
      case MC_FLORA: return 0x7fffffff; case MC_LAW: return lw_count(); case MC_GOD: return god_count(); }
    return part_n(cls);
}
static const char *const cls_nm[MC_N] = {"TRAIT","STAT","THEME","PEOPLE","CITY","CREATURE","PLANT","LAW","GOD POWER","GIFT","COLOURING","RANK","MUTATION","BODY PLAN","SIZE","DIET","HABITAT","BEHAVIOUR","STOCK","WAY OF LIFE","FORM","CHARACTER","BASE TRAIT","FLAVOUR","PLANT USE","PLANT HABITAT","PLANT QUIRK","PLANT FORM","LEAF","FLOWER","FRUIT","BARK","PLANT COLOUR","PLANT SIZE"};
const char *mn_class_name(int c){ return c >= 0 && c < MC_N ? cls_nm[c] : "?"; }
int mn_item(const MnRef *r, MnItem *o){
    Acc a; memset(&a, 0, sizeof a); int role = MN_PURPOSE, id = (int)r->id; uint32_t uid = r->id;
    if(r->cls >= MC_N) return 0;
    switch(r->cls){
    case MC_TRAIT: {
        if(id < 0 || id >= TR_BASE * TR_VARIANTS) return 0; int b = id % TR_BASE, v = id / TR_BASE, cat = trait_cat(id); Pi pb; part_get(MC_TBASE, b, &pb); role = pb.role; add_tags(&a, pb.tags, 100);
        int val, dt, dg, lt, pr, lf, lg, cb[6]; trait_num(id, &val, &dt, &dg, &lt, &pr, &lf, &lg, cb);
        if(cat == TC_GRAVITY){ addR(&a, CH_GRAVITY, cdiv(val - 100, 10), 0); addR(&a, CH_SPEED, -cdiv(val - 100, 40), 0); }
        if(cat == TC_DAY && val > 0) addR(&a, CH_TIME, clampi(cdiv(val - 24, val > 24 ? 25 : 6), -6, 12), 0);
        addR(&a, CH_HEAT, cdiv(dt, 5), 0); addR(&a, CH_DANGER, dg * 2, 0); addR(&a, CH_ORE, lt * 2, 0); addR(&a, CH_TRADE, pr * 2, 0); addR(&a, CH_FOOD, lf * 2, 0); addR(&a, CH_LIGHT, cdiv(lg, 5), 0);
        if(v){ Pi pf; part_get(MC_TFLAV, cat * 15 + v - 1, &pf); add_tags(&a, pf.tags, 100); if(!trait_look_only(id)) addR(&a, pf.ch, pf.amt, 0); sigfall(&a, first_tag(pf.tags), 1); }
        sigfall(&a, first_tag(pb.tags), 2); break; }
    case MC_STAT: {
        if(id < 0 || id >= N_STAT) return 0; int f = id / 16, z = id % 16; role = stat_role(f, z);
        add_tags(&a, tgm(st_zt[z]) | tgm(st_fhi[f]) | tgm(st_flo[f]), 100);
        a.Rc[f == 6 ? CH_FOOD : fch[f]] += 1; if(role != MN_PURPOSE) a.K[chl(st_famcos[f])] += 1; break; }
    case MC_THEME: { if(id < 0 || id >= 24) return 0; const Th *t = &th_t[id]; role = rolel(t->role); add_tags(&a, tgm(t->t1) | tgm(t->t2), 100);
        addR(&a, chl(t->c1), (t->a1 - '5') * 2, 1); addR(&a, chl(t->c2), (t->a2 - '5') * 2, 1); break; }
    case MC_PEOPLE: { if(id < 0 || id >= N_PEOPLE) return 0; People pe; people_get(id, &pe); int st = id % N_STOCK, w = id / N_STOCK; Pi ps; part_get(MC_STOCK, st, &ps); role = ps.role;
        use_part(&a, MC_STOCK, st, 100); use_part(&a, MC_WAY, w, 100);
        addR(&a, CH_DEFENCE, pe.might - 3, 1); addR(&a, CH_KNOW, pe.wit - 3, 1); addR(&a, CH_TRADE, pe.charm - 3, 1); addR(&a, CH_CRAFT, pe.craft - 3, 1);
        if(pe.temper >= 4) addR(&a, CH_DANGER, pe.temper - 3, 1); addR(&a, CH_HEALTH, cdiv(pe.lifespan - 60, 30), 1); sigfall(&a, first_tag(a.tags), 1); break; }
    case MC_CITY: { if(id < 0 || id >= N_CITY) return 0; CityKind c; city_get(id, &c); int fo = id % N_FORM, ch = id / N_FORM; Pi pf; part_get(MC_FORM, fo, &pf); role = pf.role;
        use_part(&a, MC_FORM, fo, 100); use_part(&a, MC_CHAR, ch, 100);
        addR(&a, CH_CROWD, cdiv(c.pop - 5, 2), 1); addR(&a, CH_DEFENCE, cdiv(c.walls - 4, 2), 1); addR(&a, CH_TRADE, cdiv(c.trade - 4, 2), 1); addR(&a, CH_FAITH, cdiv(c.wonder - 4, 2), 1); break; }
    case MC_CREATURE: if(id < 0 || id >= N_KIND) return 0; creature_item(id, &a, &role); break;
    case MC_FLORA: flora_item(uid & (FL_TOTAL - 1), &a, &role); break;
    case MC_LAW: { if(id < 0 || id >= lw_count()) return 0; int k = -1; for(int i = 0; i < NLWT; i++) if(!strcmp(lw_t[i].key, lw_defs[id].key)){ k = i; break; }
        char rl, tg, ch, sg; if(k >= 0){ rl = lw_t[k].role; tg = lw_t[k].tag; ch = lw_t[k].ch; sg = lw_t[k].sg; } else { int c = lw_defs[id].cat; rl = lw_cat[c].role; tg = lw_cat[c].tag; ch = lw_cat[c].ch; sg = '+'; }
        role = rolel(rl); add_tags(&a, tgm(tg), 100); addR(&a, chl(ch), sg == '+' ? 2 : -2, 1);
        if(role == MN_BOTH && chl(ch) < MN_RULE_CH) {} break; }
    case MC_GOD: { if(id < 0 || id >= god_count()) return 0; const GodPower *g = god_power(id + 1); int e = g->eff; role = rolel(ge_t[e].role); add_tags(&a, tgm(ge_t[e].tag), 100);
        addR(&a, chl(ge_t[e].ch), (ge_t[e].sg == '+' ? 1 : -1) * clampi(1 + g->amt / 12, 1, 6), 1); break; }
    default: { Pi p; if(!part_get(r->cls, id, &p)) return 0; role = p.role; a.tags = 0; add_tags(&a, p.tags, 100);
        if(p.ch >= 0) addR(&a, p.ch, p.amt, 1); else sigfall(&a, first_tag(p.tags), 1); if(p.ch >= 0 && p.ch < MN_RULE_CH) sigfall(&a, first_tag(p.tags), 0); }
    }
    finish(o, role, &a); return 1;
}
void mn_means(const MnItem *it, char *out, int n){
    char r[160] = "", k[160] = ""; int nr = 0, nk = 0;
    for(int i = 0; i < it->n; i++){
        int c = it->ch[i], up = it->amt[i] > 0; const char *w = up ? ch_up[c] : ch_dn[c];
        if(c < MN_RULE_CH){ if(nr < 3){ if(nr) strcat(r, ", "); strcat(r, w); nr++; } } else if(nk < 3){ if(nk) strcat(k, ", "); strcat(k, w); nk++; }
    }
    if(it->role == MN_PURPOSE) snprintf(out, n, "MEANS: %s", r);
    else if(it->role == MN_COSMETIC) snprintf(out, n, "MEANS: ONLY LOOKS, %s", k);
    else snprintf(out, n, "MEANS: %s. LOOKS: %s", r, k);
}
// ------------------------------------------------------------------ the resolver
#define MN_MAXITEMS 256
#define RAWMAX 100000
static int32_t sat(int32_t a, int d){ int64_t v = (int64_t)a + d; return v > RAWMAX ? RAWMAX : v < -RAWMAX ? -RAWMAX : (int32_t)v; }
static int grp(int cls){
    switch(cls){ case MC_TRAIT: case MC_TBASE: case MC_TFLAV: return 0; case MC_THEME: return 1; case MC_PEOPLE: case MC_STOCK: case MC_WAY: return 3; case MC_CITY: case MC_FORM: case MC_CHAR: return 4;
      case MC_FLORA: case MC_FUSE: case MC_FHAB: case MC_FQUIRK: case MC_FFORM: case MC_FLEAF: case MC_FFLOWER: case MC_FFRUIT: case MC_FBARK: case MC_FCOL: case MC_FSIZE: return 5;
      case MC_LAW: return 6; case MC_GOD: return 7; case MC_STAT: return 8; }
    return 2;
}
static int lwgt(int a, int b){
    if(a > b){ int t = a; a = b; b = t; }
    if(a == 8 || b == 8) return 2;
    if(a == 0 && b == 2) return 3; if(a == 2 && b == 5) return 3; if(a == 3 && b == 4) return 3; if(a == 0 && b == 1) return 3;
    if((a == 0 && (b == 3 || b == 4 || b == 5)) || (a == 2 && b == 3)) return 2;
    return 1;
}
static uint32_t opp_of(uint32_t t){ uint32_t o = 0; for(unsigned i = 0; i < sizeof opp_pairs / sizeof opp_pairs[0]; i++){ uint32_t x = tgm(opp_pairs[i][0]), y = tgm(opp_pairs[i][1]); if(t & x) o |= y; if(t & y) o |= x; } return o; }
static int scaled(int f, int v){ switch(f){ case 0: return cdiv(v - 100, 5); case 1: return cdiv(v - 15, 4); case 2: return cdiv(v - 50, 4); case 6: return v * 3; default: return v * 4; } }
static int stat_tags(int f, int z, int v, uint32_t *t){          // the tags a stat adds when it is strong
    int hi = 0, lo = 0;
    switch(f){ case 0: hi = v - 100 >= 30; lo = 100 - v >= 30; break; case 1: hi = v - 15 >= 25; lo = 15 - v >= 25; break; case 2: hi = v - 50 >= 25; lo = 50 - v >= 25; break;
      case 3: hi = v >= 4; lo = v <= -2; break; case 4: hi = v >= 3; lo = v <= -2; break; case 5: hi = v >= 3; lo = v <= -3; break; default: hi = v >= 3; lo = v <= -2; }
    if(!hi && !lo) { *t = 0; return 0; }
    *t = tgm(st_zt[z]) | tgm(hi ? st_fhi[f] : st_flo[f]); return 1;
}
void mn_resolve(const MnRef *refs, int n, MnProfile *p, int want){
    static const int8_t dummy = 0; (void)dummy;
    MnItem it[MN_MAXITEMS]; uint32_t tg[MN_MAXITEMS], op[MN_MAXITEMS]; uint8_t gr[MN_MAXITEMS], rl[MN_MAXITEMS], live[MN_MAXITEMS];
    int64_t fs[7]; int fw[7]; int m = 0, i, j;
    memset(p, 0, sizeof *p); memset(fs, 0, sizeof fs); memset(fw, 0, sizeof fw);
    if(n > MN_MAXITEMS) n = MN_MAXITEMS;
    for(i = 0; i < n; i++){
        if(!mn_item(&refs[i], &it[m])) continue;
        const MnItem *x = &it[m]; int cls = refs[i].cls; tg[m] = x->tags; gr[m] = (uint8_t)grp(cls); rl[m] = x->role; live[m] = 1; p->role_n[x->role]++;
        if(cls == MC_STAT){
            int f = (int)refs[i].id / 16, z = (int)refs[i].id % 16, v = refs[i].val;
            if(x->role != MN_COSMETIC){ fs[f] += (int64_t)st_w[z] * scaled(f, clampi(v, -2000, 2000)); fw[f] += st_w[z]; }
            if(x->role != MN_PURPOSE) p->raw[chl(st_famcos[f])] = sat(p->raw[chl(st_famcos[f])], clampi(scaled(f, clampi(v, -2000, 2000)) / 4, -3, 3));
            if(!stat_tags(f, z, v, &tg[m])) live[m] = 0;
        } else if(cls == MC_LAW){
            const LawDef *d = &lw_defs[refs[i].id]; int64_t rng = (int64_t)d->max - d->min; if(rng < 1) rng = 1;
            int dev = (int)(((int64_t)refs[i].val - d->def) * 8 / rng); dev = clampi(dev, -8, 8);
            if(!dev) live[m] = 0;
            for(j = 0; j < x->n; j++) p->raw[x->ch[j]] = sat(p->raw[x->ch[j]], x->cnt[j] * dev / 2);
        } else for(j = 0; j < x->n; j++) p->raw[x->ch[j]] = sat(p->raw[x->ch[j]], x->cnt[j]);
        op[m] = opp_of(tg[m]); m++;
    }
    p->n_items = m;
    for(i = 0; i < 7; i++) if(fw[i]){ int avg = (int)(fs[i] / fw[i]);
        if(i == 6){ p->raw[CH_FOOD] = sat(p->raw[CH_FOOD], avg); p->raw[CH_GROWTH] = sat(p->raw[CH_GROWTH], avg * 2 / 3); } else p->raw[fch[i]] = sat(p->raw[fch[i]], avg); }
    // synergy and clash
    int syn[MN_NTAG], cl[MN_NTAG], cosyn = 0; memset(syn, 0, sizeof syn); memset(cl, 0, sizeof cl);
    for(i = 0; i < m; i++) if(live[i]) for(int t = 0; t < MN_NTAG; t++) if(tg[i] >> t & 1) p->tag_n[t]++;
    for(i = 0; i < m; i++){ if(!live[i] || !tg[i]) continue;
        for(j = i + 1; j < m; j++){ if(!live[j] || !tg[j]) continue;
            int w = lwgt(gr[i], gr[j]); uint32_t sh = tg[i] & tg[j];
            if(sh){ int c = __builtin_popcount(sh), cosm = rl[i] == MN_COSMETIC || rl[j] == MN_COSMETIC;
                if(cosm) cosyn += w * c; else for(int t = 0; t < MN_NTAG; t++) if(sh >> t & 1) syn[t] += w;
                p->syn_n += w * c;
                if(want){ int sc = w * c, t0 = __builtin_ctz(sh), k = -1, lowest = 1000, cnt = 0;
                    for(int q = 0; q < p->npairs; q++) if(!p->pair[q].clash){ cnt++; if(p->pair[q].score < lowest){ lowest = p->pair[q].score; k = q; } }
                    if(cnt < 8 && p->npairs < MN_MAXPAIRS){ k = p->npairs++; } else if(k >= 0 && sc <= lowest) k = -1;
                    if(k >= 0){ p->pair[k].a = (uint8_t)i; p->pair[k].b = (uint8_t)j; p->pair[k].clash = 0; p->pair[k].tag = (uint8_t)t0; p->pair[k].score = (int8_t)clampi(sc, 0, 100); } } }
            uint32_t c1 = tg[i] & op[j], c2 = tg[j] & op[i];
            if(c1 | c2){ int c = __builtin_popcount(c1) + __builtin_popcount(c2); p->clash_n += w * c;
                for(int t = 0; t < MN_NTAG; t++) if(((c1 | c2) >> t) & 1) cl[t] += w;
                if(want){ int sc = w * c, t0 = __builtin_ctz(c1 | c2), k = -1, cnt = 0;
                    for(int q = 0; q < p->npairs; q++) if(p->pair[q].clash){ cnt++; }
                    if(p->npairs < MN_MAXPAIRS && cnt < 4){ k = p->npairs++; } else { int lowest = 1000; for(int q = 0; q < p->npairs; q++) if(p->pair[q].clash && p->pair[q].score < lowest){ lowest = p->pair[q].score; if(sc > lowest) k = q; } }
                    if(k >= 0){ p->pair[k].a = (uint8_t)i; p->pair[k].b = (uint8_t)j; p->pair[k].clash = 1; p->pair[k].tag = (uint8_t)t0; p->pair[k].score = (int8_t)clampi(sc, 0, 100); } } } } }
    for(int t = 0; t < MN_NTAG; t++){
        p->tag_syn[t] = (int16_t)clampi(syn[t], 0, 30000); p->tag_clash[t] = (int16_t)clampi(cl[t], 0, 30000);
        int b = (syn[t] > 12 ? 12 : syn[t]) * 2, pen = cl[t] > 8 ? 8 : cl[t], c = chl(SIG[t]);
        p->raw[c] = sat(p->raw[c], b - pen); p->raw[CH_MOOD] = sat(p->raw[CH_MOOD], -pen / 2);
    }
    p->raw[CH_BEAUTY] = sat(p->raw[CH_BEAUTY], (cosyn > 12 ? 12 : cosyn) * 2);
    for(i = 0; i < MN_NCH; i++){ int64_t r = p->raw[i], a = r < 0 ? -r : r; p->lvl[i] = (int8_t)(100 * r / (a + 60)); }
}
uint64_t mn_profile_hash(const MnProfile *p){
    uint64_t h = 0xCBF29CE484222325ull; for(int i = 0; i < MN_NCH; i++){ h ^= (uint32_t)p->raw[i]; h *= 0x100000001B3ull; h ^= (uint8_t)p->lvl[i]; h *= 0x100000001B3ull; }
    h ^= (uint64_t)p->syn_n << 20 ^ (uint64_t)p->clash_n << 40 ^ (uint64_t)p->n_items; h *= 0x100000001B3ull; return h;
}
// ------------------------------------------------------------------ what the game does
void mn_hooks(const MnProfile *p, MnHooks *h){
    const int8_t *l = p->lvl;
    h->hunger_pct = clampi(100 - l[CH_FOOD] * 45 / 100, 40, 160); h->gold_pct = clampi(100 + l[CH_TRADE] / 2 + l[CH_LUCK] / 4, 50, 200);
    h->fauna_add = clampi(l[CH_DANGER] / 30, -2, 2); h->beetle_add = clampi(l[CH_DANGER] / 40, -1, 2);
    h->speed_add = clampi(l[CH_SPEED] * 0x300 / 100, -0x600, 0x600);
    h->hour = l[CH_LIGHT] <= -45 ? 2 : l[CH_LIGHT] <= -20 ? 1 : l[CH_LIGHT] >= 45 ? 0 : -1;
    h->weather = -1;
    if(l[CH_WEATHER] <= -25) h->weather = 0;
    else if(l[CH_WEATHER] >= 35){ int w = 1; if(p->tag_n[3] > 0) w = 2; if(p->tag_n[2] > 0) w = 3; if(p->tag_n[4] > 0) w = 4; if(p->tag_n[15] > 1) w = 5; h->weather = w; }
    h->node[0] = clampi(l[CH_GROWTH] / 35, -2, 2); h->node[1] = clampi(l[CH_ORE] / 35, -2, 2); h->node[2] = clampi(l[CH_CRAFT] / 40, -2, 2); h->node[3] = clampi(l[CH_FOOD] / 40, -2, 2);
    h->sky_r = clampi(l[CH_TINT] * 24 / 100 + l[CH_GLOW] / 10, -24, 24); h->sky_g = clampi(l[CH_MOOD] / 10, -12, 12); h->sky_b = clampi(-l[CH_TINT] * 24 / 100 + l[CH_GLOW] / 10, -24, 24);
    h->lift = clampi(l[CH_BEAUTY] / 8, -10, 10);
}
void mn_flavour(const MnProfile *p, char *out, int n){
    int b1 = -1, b2 = -1, v1 = 0, v2 = 0;
    for(int c = MN_RULE_CH; c < MN_NCH; c++){ int v = p->lvl[c] < 0 ? -p->lvl[c] : p->lvl[c]; if(v > v1){ v2 = v1; b2 = b1; v1 = v; b1 = c; } else if(v > v2){ v2 = v; b2 = c; } }
    if(b1 < 0 || v1 < 8){ snprintf(out, n, "A PLACE WITH NOTHING SPECIAL TO SEE OR HEAR."); return; }
    #define LK(c) (p->lvl[c] > 0 ? look_up[(c) - MN_RULE_CH] : look_dn[(c) - MN_RULE_CH])
    if(b2 >= 0 && v2 >= 8) snprintf(out, n, "A PLACE THAT IS %s AND %s.", LK(b1), LK(b2)); else snprintf(out, n, "A PLACE THAT IS %s.", LK(b1));
}
void mn_summary(const MnProfile *p, char *out, int n){
    int used[3] = {-1, -1, -1}; out[0] = 0; size_t len = 0;
    for(int k = 0; k < 3; k++){ int best = -1, bv = 14; for(int c = 0; c < MN_RULE_CH; c++){ int v = p->lvl[c] < 0 ? -p->lvl[c] : p->lvl[c]; int dup = 0; for(int q = 0; q < k; q++) if(used[q] == c) dup = 1; if(!dup && v > bv){ bv = v; best = c; } }
        if(best < 0) break; used[k] = best; len += (size_t)snprintf(out + len, n - len, "%s%s", k ? ", " : "", p->lvl[best] > 0 ? ch_up[best] : ch_dn[best]); }
    if(!out[0]) snprintf(out, n, "NOTHING STRONG");
}
// ------------------------------------------------------------------ a planet's items
int mn_planet_refs(const PlanetInfo *pi, const int themes[2], const uint32_t *flora, int nflora, const MnExtra *x, MnRef *out, int max){
    int n = 0;
    #define PUSH(c, i, v) do{ if(n < max){ out[n].cls = (uint8_t)(c); out[n].id = (uint32_t)(i); out[n].val = (v); n++; } }while(0)
    for(int i = 0; i < pi->n; i++) PUSH(MC_TRAIT, pi->id[i], 0);
    for(int i = 0; i < N_STAT; i++) PUSH(MC_STAT, i, pi->stat[i]);
    if(themes){ PUSH(MC_THEME, themes[0], 0); PUSH(MC_THEME, themes[1], 0); }
    for(int i = 0; i < pi->npeople; i++) PUSH(MC_PEOPLE, pi->people[i], 0);
    if(pi->city >= 0) PUSH(MC_CITY, pi->city, 0);
    for(int i = 0; i < pi->nbeing; i++) PUSH(MC_CREATURE, pi->being[i], 0);
    PUSH(MC_GIFT, pi->legend_gift, 0); PUSH(MC_CCOL, pi->legend_colour, 0); PUSH(MC_RANK, pi->legend_rank, 0); PUSH(MC_MUT, pi->legend_mut, 0);
    for(int i = 0; i < nflora; i++) PUSH(MC_FLORA, flora[i], 0);
    if(x){ for(int i = 0; i < x->lawn; i++) if(x->law_id[i] >= 0 && x->law_id[i] < lw_count()) PUSH(MC_LAW, x->law_id[i], x->law_val[i]);
           for(int i = 0; i < x->gods && i < god_count(); i++) PUSH(MC_GOD, i, 0); }
    return n;
}
int mn_god_count(void){ return god_count(); }
// tables must be as long as their classes
int mn_selfcheck(void){
    int bad = 0;
    for(int c = 0; c < MC_N; c++){ const PT *p = ptab(c); if(!p) continue; int n = p->n;
        if((int)strlen(p->role) != n) { printf("selfcheck: roles of class %d: %d vs %d\n", c, (int)strlen(p->role), n); bad++; }
        if(p->t1 && p->t1[0] && (int)strlen(p->t1) != n){ printf("selfcheck: t1 of class %d\n", c); bad++; }
        if(p->t2 && p->t2[0] && (int)strlen(p->t2) != n){ printf("selfcheck: t2 of class %d\n", c); bad++; }
        if(p->ch && p->ch[0] && (int)strlen(p->ch) != n){ printf("selfcheck: ch of class %d\n", c); bad++; }
        if(p->am && p->am[0] && (int)strlen(p->am) != n){ printf("selfcheck: am of class %d\n", c); bad++; } }
    if(strlen(tb_role) != TR_BASE || strlen(tb_tags) != 2 * TR_BASE){ printf("selfcheck: trait base tables %d %d\n", (int)strlen(tb_role), (int)strlen(tb_tags)); bad++; }
    if(strlen(stock_tags) != 64 || strlen(way_tags) != 32){ printf("selfcheck: stock/way tags\n"); bad++; }
    for(int c = 0; c < 9; c++) if(strlen(flav_tag[c]) != 15){ printf("selfcheck: flavour tags %d\n", c); bad++; }
    if(strlen(SIG) != MN_NTAG || strlen(COSC) != MN_NTAG || strlen(COSS) != MN_NTAG){ printf("selfcheck: tag tables\n"); bad++; }
    return bad;
}
