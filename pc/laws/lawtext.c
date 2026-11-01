// lawtext.c: HOW THIS WORKS. Plain explanations of where the numbers of this place come from, with the real numbers filled in.
// Font rule: capitals, digits, space and . , ' : ( ) - / % ? ! only. Lines are cut at 57 letters (the screen holds 58).
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "laws.h"

#define MAXC 57
static void line(LxPage *p, int col, const char *s){ if(p->n < LX_LINES){ snprintf(p->t[p->n], LX_W, "%.*s", MAXC, s); p->col[p->n++] = (uint8_t)col; } }
static void wrap(LxPage *p, int col, const char *s){          // word wrap at 57 letters
    char buf[400]; snprintf(buf, sizeof buf, "%s", s); char cur[LX_W]; cur[0] = 0; int L = 0;
    for(char *w = strtok(buf, " "); w; w = strtok(0, " ")){
        int wl = (int)strlen(w);
        if(L && L + 1 + wl > MAXC){ line(p, col, cur); cur[0] = 0; L = 0; }
        if(L){ cur[L++] = ' '; cur[L] = 0; } memcpy(cur + L, w, wl + 1); L += wl;
    }
    if(L) line(p, col, cur);
}
static void fw(LxPage *p, int col, const char *fmt, ...){ char b[400]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap); wrap(p, col, b); }
static void page(LxPage *p, const char *title){ memset(p, 0, sizeof *p); snprintf(p->title, sizeof p->title, "%s", title); }
static const char *yesno(int v){ return v ? "YES" : "NO"; }
static const char *fate_text(int f){
    static const char *const t[4] = { "A SLOW FADE: STARS BURN OUT AND EVERYTHING GROWS COLD AND DARK, FOR EVER.", "THE BIG RIP: THE PUSH WINS AND TEARS GALAXIES, STARS AND ATOMS APART.", "THE BIG CRUNCH: GRAVITY WINS, SPACE FALLS BACK AND EVERYTHING IS SQUASHED TOGETHER.", "STILL: SPACE DOES NOT STRETCH AT ALL. NOTHING MOVES APART, NOTHING CHANGES." };
    return t[f < 0 ? 0 : f > 3 ? 3 : f];
}
static const char *lawn(int id){ return lw_defs[id].name; }

int lw_explain(const LawSet *s0, const LwEnv *e, LxPage *P){
    LawSet eff; lw_effective(s0, e->useed, &eff); const LawSet *s = &eff;
    LwResult r, f; lw_eval(s0, e, &r); { LawSet d; lw_default(&d); lw_eval(&d, e, &f); }
    const double *o = r.o; char a[64], b[64], c[64], d[64]; int n = 0; LxPage *p;
    // ---- 1: the universe
    p = &P[n++]; page(p, "THE UNIVERSE");
    lw_fmt_time(o[LO_U_AGE] * 1e9 * 3.156e7, a, sizeof a); fw(p, 0, "THIS UNIVERSE IS %s OLD.", a);
    lw_fmt_big(o[LO_U_HUBBLE], a, sizeof a); fw(p, 0, "SPACE STRETCHES: EVERY MEGAPARSEC (3.26 MILLION LIGHT YEARS) OF EMPTY SPACE GROWS %s KM EVERY SECOND.", a);
    fw(p, 2, "THE STRETCH IS SET BY %s (%d) AND %s (%d). DARK ENERGY PUSHES SPACE APART, MATTER PULLS IT BACK.", lawn(LW_U_EXPAND), s->v[LW_U_EXPAND], lawn(LW_U_DARKE), s->v[LW_U_DARKE]);
    lw_fmt_big(o[LO_U_OBS] * 1e9, a, sizeof a); fw(p, 0, "THE LIGHT HAS HAD TIME TO REACH YOU FROM %s LIGHT YEARS AWAY. FARTHER OUT IS STILL DARK.", a);
    fw(p, 1, "THE SPEED OF LIGHT IS %d%% OF NORMAL, SO A TRIP THAT USED TO TAKE 1 YEAR TAKES %s.", s->v[LW_U_LIGHT], (lw_fmt_time(3.156e7 / (s->v[LW_U_LIGHT] / 100.0), a, sizeof a), a));
    fw(p, 4, "HOW IT ENDS: %s", fate_text(r.fate));
    fw(p, 2, "DARK MATTER IS %d%% OF ALL MATTER. IT IS THE GLUE THAT KEEPS GALAXIES FROM FLYING APART.", (int)(o[LO_U_DM] + 0.5));
    // ---- 2: the galaxy
    p = &P[n++]; page(p, "THE GALAXY");
    fw(p, 1, "SHAPE OF THIS GALAXY: %s%s.", lw_gtype_name[r.gtype], e->gal == 0 ? " (THE BUG WAY, YOUR HOME)" : "");
    lw_fmt_big(o[LO_G_WIDTH] * 1000, a, sizeof a); lw_fmt_big(o[LO_G_STARS] * 1e9, b, sizeof b); fw(p, 0, "IT IS %s LIGHT YEARS WIDE AND HOLDS ABOUT %s STARS.", a, b);
    if(o[LO_G_ARMS] >= 1 && r.gtype <= GT_BARRED) fw(p, 0, "IT HAS %d SPIRAL ARMS. THE ARMS ARE WHERE GAS IS SQUEEZED INTO NEW STARS, SO THEY GLOW BLUE.", (int)o[LO_G_ARMS]);
    else if(r.gtype == GT_RING) fw(p, 0, "ITS STARS FORM A RING AROUND A HOLE, LEFT BY ANOTHER GALAXY THAT CRASHED STRAIGHT THROUGH.");
    else fw(p, 0, "IT HAS NO SPIRAL ARMS. ITS STARS MOVE IN ALL DIRECTIONS.");
    lw_fmt_big(o[LO_G_SPIN], a, sizeof a); lw_fmt_big(o[LO_G_PERIOD], b, sizeof b);
    fw(p, 3, "WHY GALAXIES SPIN: GAS FELL TOGETHER AND TURNED, LIKE WATER DOWN A DRAIN. AT THE SUN'S DISTANCE STARS MOVE %s KM/S, ONE TURN TAKES %s MILLION YEARS.", a, b);
    fw(p, 2, "GRAVITY (%d) AND DARK MATTER (%d) SET THE SPEED. WITHOUT DARK MATTER THE OUTER STARS WOULD FLY AWAY.", s->v[LW_U_GRAV], s->v[LW_U_DARKM]);
    lw_fmt_big(o[LO_G_BH], a, sizeof a); fw(p, 0, "AT THE MIDDLE SITS A BLACK HOLE OF %s MILLION SUNS. THIS STAR IS %d%% OF THE WAY OUT.", a, (int)(o[LO_G_RFRAC] + 0.5));
    lw_fmt_big(o[LO_G_SFR], a, sizeof a); fw(p, 0, "NEW STARS: %s SUNS A YEAR. METALS HERE: %d%% OF NORMAL.", a, (int)(o[LO_G_METAL] * 100 + 0.5));
    // ---- 3: the star
    p = &P[n++]; page(p, "THE STAR");
    fw(p, 1, "THE STAR IS %s%s.", lw_sclass_name[r.sclass], r.nsuns > 1 ? (r.nsuns == 2 ? ", WITH A PARTNER (TWO SUNS)" : ", WITH TWO PARTNERS (THREE SUNS)") : "");
    lw_fmt_big(o[LO_S_MASS], a, sizeof a); lw_fmt_big(o[LO_S_LUM], b, sizeof b); lw_fmt_big(o[LO_S_TEMP], c, sizeof c);
    fw(p, 0, "MASS %s SUNS. BRIGHTNESS %s SUNS. SURFACE %s KELVIN.", a, b, c);
    lw_fmt_time(o[LO_S_AGE] * 1e9 * 3.156e7, a, sizeof a); lw_fmt_time(o[LO_S_LIFE] * 1e9 * 3.156e7, b, sizeof b); fw(p, 0, "AGE %s. A STAR LIKE THIS LIVES ABOUT %s.", a, b);
    fw(p, 3, "WHAT MAKES THE SUN HOT: ITS OWN WEIGHT SQUEEZES THE CORE, AND AT THAT HEAT ATOMS OF HYDROGEN FUSE INTO HELIUM AND LET GO OF ENERGY. FUSION STRENGTH IS %d.", s->v[LW_U_FUSION]);
    fw(p, 3, "HEAVIER STARS SQUEEZE HARDER, BURN FASTER AND DIE YOUNG. SMALL RED DWARFS SIP THEIR FUEL FOR TRILLIONS OF YEARS.");
    fw(p, 3, "BIRTH AND DEATH: CLOUDS OF GAS COLLAPSE INTO STARS (%d%% OF NORMAL). WHEN THE FUEL RUNS OUT A STAR SWELLS INTO A RED GIANT, THEN SHRINKS TO A WHITE DWARF. THE BIGGEST EXPLODE AND LEAVE A NEUTRON STAR.", s->v[LW_S_SFR]);
    lw_fmt_big(o[LO_S_HZIN], a, sizeof a); lw_fmt_big(o[LO_S_HZOUT], b, sizeof b); fw(p, 4, "WATER CAN BE LIQUID BETWEEN %s AND %s AU FROM THE STAR (1 AU IS THE EARTH-SUN DISTANCE).", a, b);
    // ---- 4: planet size
    p = &P[n++]; page(p, "HOW BIG IS THIS PLANET");
    lw_fmt_dist(o[LO_P_RADIUS], a, sizeof a); lw_fmt_dist(o[LO_P_RADIUS] * 2 * 3.14159265, b, sizeof b); fw(p, 1, "RADIUS %s. ONCE ROUND IS %s.", a, b);
    lw_fmt_big(o[LO_P_CIRC], a, sizeof a); fw(p, 0, "THAT IS %s OF YOUR BUG'S STEPS (A STEP IS 5 CM).", a);
    lw_fmt_time(o[LO_P_WALK], a, sizeof a); fw(p, 0, "WALKING ROUND AT 1 METRE A SECOND TAKES %s.", a);
    lw_fmt_dist(o[LO_P_HORIZON], a, sizeof a); fw(p, 0, "FROM BUG EYE HEIGHT THE HORIZON IS %s AWAY.", a);
    { double rel = o[LO_P_RADIUS] / 6.371e6; lw_fmt_big(rel, a, sizeof a);
      if(rel >= 1) fw(p, 0, "THE EARTH'S RADIUS IS 6,371 KM. THIS PLANET'S IS %s TIMES THAT.", a); else { lw_fmt_big(1.0 / rel, b, sizeof b); fw(p, 0, "THE EARTH'S RADIUS IS %s TIMES BIGGER THAN THIS PLANET'S.", b); } }
    fw(p, 3, "WHY THE HORIZON BENDS: THE SMALLER A PLANET, THE FASTER THE GROUND CURVES AWAY, SO THE HORIZON COMES CLOSE. ON A HUGE PLANET THE LAND LOOKS FLAT FOR AS FAR AS YOU CAN SEE.");
    fw(p, 2, "PLANET SIZE LAW: %s. SIZE VARIETY: %d%%.", (lw_value_text(s0, LW_P_SIZE, 0, a, sizeof a), a), s->v[LW_P_SPREAD]);
    { LwApply ap; lw_apply(s0, e, &ap); if(ap.tw < 128) fw(p, 4, "SO SMALL THAT THE WORLD REPEATS EVERY %d CELLS: WALK %d CELLS AND YOU ARE HOME.", ap.tw, ap.tw); }
    // ---- 5: gravity and air
    p = &P[n++]; page(p, "GRAVITY AND AIR");
    lw_fmt_big(o[LO_P_G], a, sizeof a); lw_fmt_big(o[LO_P_GREL], b, sizeof b); fw(p, 1, "GRAVITY %s M/S2, WHICH IS %s TIMES THE EARTH'S.", a, b);
    fw(p, 3, "WHERE IT COMES FROM: GRAVITY GROWS WITH THE PLANET'S SIZE AND HOW PACKED IT IS. THE UNIVERSE'S GRAVITY STRENGTH (%d%%) AND THE DENSITY (%d%%) SCALE IT.", s->v[LW_U_GRAV], s->v[LW_P_DENS]);
    lw_fmt_big(o[LO_P_VESC], a, sizeof a); fw(p, 0, "TO FLY AWAY YOU NEED %s M/S (THE EARTH NEEDS 11,186).", a);
    lw_fmt_big(o[LO_P_PRESS], a, sizeof a); fw(p, 0, "AIR PRESSURE IS %s TIMES THE EARTH'S.", a);
    fw(p, 3, "SMALL PLANETS CANNOT HOLD ON TO AIR: IT DRIFTS OFF INTO SPACE. BIG, HEAVY ONES KEEP A THICK BLANKET. THE STAR WIND BLOWS ON IT TOO (%d%%).", s->v[LW_S_WIND]);
    if(o[LO_P_GREL] < 0.2) fw(p, 4, "SO LITTLE GRAVITY THAT YOU BOUNCE ALONG LIKE ON THE MOON.");
    else if(o[LO_P_GREL] > 1.4) fw(p, 5, "HEAVY! YOU WALK SLOWER AND GET HUNGRIER.");
    // ---- 6: day and year
    p = &P[n++]; page(p, "DAY, YEAR AND SEASONS");
    lw_fmt_time(o[LO_P_DAY] * 3600, a, sizeof a); lw_fmt_time(o[LO_P_YEAR] * 86400, b, sizeof b); fw(p, 1, "A DAY LASTS %s. A YEAR LASTS %s.", a, b);
    lw_fmt_big(o[LO_P_ORBIT], a, sizeof a); fw(p, 0, "THE PLANET ORBITS %s AU FROM ITS STAR.", a);
    fw(p, 3, "THE DAY IS ONE TURN OF THE PLANET ON ITS AXIS. THE YEAR IS ONE TRIP AROUND THE STAR. FARTHER OUT AND ORBIT SPEED %d%% GIVE LONGER YEARS, STRONGER GRAVITY (%d%%) GIVES SHORTER ONES.", s->v[LW_T_ORBIT], s->v[LW_U_GRAV]);
    if(r.locked) fw(p, 5, "TIDALLY LOCKED: IT SHOWS ONE FACE TO ITS STAR. DAY AND YEAR ARE THE SAME LENGTH, ONE SIDE ROASTS AND THE OTHER FREEZES.");
    if(r.rogue) fw(p, 5, "NO STAR AT ALL: THIS LONELY PLANET DRIFTS IN THE DARK.");
    fw(p, 0, "AXIS TILT %d DEGREES.", (int)(o[LO_P_TILT] + 0.5));
    if(o[LO_P_SEASON] > 0){ lw_fmt_time(o[LO_P_SEASON] * 86400, a, sizeof a); fw(p, 3, "SEASONS HAPPEN BECAUSE THE PLANET LEANS: THE LEANING HALF GETS MORE SUN. EACH SEASON LASTS ABOUT %s.", a); }
    else fw(p, 3, "NO TILT AND A ROUND ORBIT MEANS NO SEASONS AT ALL.");
    lw_fmt_time(o[LO_P_MONTH] * 86400, a, sizeof a); fw(p, 0, "A MOON TAKES ABOUT %s TO GO ROUND.", a);
    // ---- 7: climate
    p = &P[n++]; page(p, "HEAT AND WEATHER");
    fw(p, 1, "AVERAGE %d C. COLDEST %d C, HOTTEST %d C.", (int)(o[LO_P_TEMP] + (o[LO_P_TEMP] < 0 ? -0.5 : 0.5)), (int)(o[LO_P_TMIN] + (o[LO_P_TMIN] < 0 ? -0.5 : 0.5)), (int)(o[LO_P_TMAX] + 0.5));
    lw_fmt_big(o[LO_P_LIGHT], a, sizeof a); fw(p, 0, "THE STAR GIVES THIS PLANET %s TIMES THE LIGHT THE EARTH GETS.", a);
    fw(p, 3, "THE HEAT COMES FROM STARLIGHT (BRIGHTER AND CLOSER IS WARMER), MINUS THE SHARE THE PLANET BOUNCES BACK, PLUS THE GREENHOUSE: AIR TRAPS HEAT LIKE A BLANKET (%d%%).", s->v[LW_P_GREEN]);
    fw(p, 0, "WATER COVERS ABOUT %d%% OF THE PLANET.", (int)(o[LO_P_WATER] + 0.5));
    fw(p, 3, "WEATHER NEEDS AIR AND WATER: HEAT LIFTS WATER INTO CLOUDS, WIND MOVES THEM, AND RAIN OR SNOW FALLS. STORMS %d%%, CLOUDS %d%%.", s->v[LW_P_STORM], s->v[LW_P_CLOUD]);
    lw_fmt_big(o[LO_P_RAD], a, sizeof a); fw(p, 0, "RADIATION AT THE GROUND IS %s TIMES NORMAL. A MAGNETIC FIELD AND THICK AIR PROTECT YOU.", a);
    lw_fmt_big(o[LO_P_QUAKE], a, sizeof a); fw(p, 0, "QUAKES AND VOLCANOES: %s TIMES NORMAL.", a);
    // ---- 8: life
    p = &P[n++]; page(p, "LIFE");
    fw(p, 1, "LIFE INDEX %d%% (100 IS AS COMFORTABLE AS A GOOD NORMAL PLANET).", (int)(o[LO_P_LIFE] + 0.5));
    fw(p, 3, "LIFE NEEDS A COMFORTABLE TEMPERATURE (WITHIN %d DEGREES), LIQUID WATER, AIR OF THE RIGHT THICKNESS WITH OXYGEN, LITTLE RADIATION AND TIME TO GROW.", s->v[LW_L_TOLER]);
    lw_fmt_time(o[LO_S_AGE] * 1e9 * 3.156e7, a, sizeof a); fw(p, 0, "THE STAR IS %s OLD: %s", a, o[LO_S_AGE] < 0.8 ? "TOO YOUNG FOR MUCH LIFE YET." : "OLD ENOUGH FOR LIFE TO HAVE GROWN.");
    fw(p, 0, "STARS LIKE THIS ONE: %s.", (r.sclass == SC_NS || r.sclass == SC_WD) ? "DEAD. THEY GIVE LITTLE LIGHT AND LIFE STRUGGLES" : r.sclass <= SC_B ? "TOO SHORT-LIVED FOR LIFE TO GET STARTED" : "GOOD, STEADY LIGHT");
    fw(p, 0, "CITIES: %s. RUINS: %s. WEIRD WORLD: %s.", yesno(e->city), yesno(e->ruins), yesno(0));
    fw(p, 2, "CHANCE OF LIFE LAW: %d. SMART LIFE: %d. EVOLUTION SPEED: %d.", s->v[LW_L_CHANCE], s->v[LW_L_SMART], s->v[LW_L_EVOLVE]);
    // ---- 9: trips
    p = &P[n++]; page(p, "TRIPS AND DISTANCES");
    fw(p, 1, "ONE STAR CLUSTER IS %d LIGHT YEARS WIDE. A GALAXY IS A BLOCK OF 2^%d BY 2^%d CLUSTERS.", s->v[LW_U_SPACING], s->v[LW_U_GALBITS], s->v[LW_U_GALBITS]);
    lw_fmt_big(o[LO_U_GALBLOCK], a, sizeof a); fw(p, 0, "SO A GALAXY BLOCK IS %s LIGHT YEARS ACROSS.", a);
    lw_fmt_time(s->v[LW_U_SPACING] * 3.156e7 / (s->v[LW_U_LIGHT] / 100.0), a, sizeof a); fw(p, 0, "LIGHT TAKES %s TO CROSS ONE CLUSTER, AND SO DOES A SHIP AT LIGHT SPEED.", a);
    fw(p, 3, "NOTHING GOES FASTER THAN LIGHT. A LOWER SPEED OF LIGHT STRETCHES EVERY TRIP. A HIGHER ONE SHRINKS IT.");
    lw_fmt_big(o[LO_U_FUELX], a, sizeof a); fw(p, 0, "FUEL FOR A TRIP IS %s TIMES NORMAL (FAR TRIPS COST MORE WHEN SPACE STRETCHES FASTER).", a);
    lw_fmt_big(o[LO_U_TRIPX], a, sizeof a); fw(p, 2, "TRIP TIME: %s TIMES NORMAL. SPEED OF LIGHT LAW: %d.", a, s->v[LW_U_LIGHT]);
    (void)b; (void)c; (void)d; (void)f;
    return n;
}
