// lawmodel.c: the model. A LawSet and a place go in; physical numbers and the game's changes come out.
// Rule: every number is worked out under the default laws too (the "reference"), and what the game changes is the DIFFERENCE. So with the
// default laws the game's own planet is left exactly as it was made, and every law moves something you can see.
// Only + - * / and comparisons are used (no libm), so the same laws and place give the same numbers on every machine.
#include <string.h>
#include "laws.h"

const char *const lw_out_name[LO_N] = {
#define LO(id,name,unit) name,
#include "laws_out.h"
#undef LO
};
const char *const lw_out_unit[LO_N] = {
#define LO(id,name,unit) unit,
#include "laws_out.h"
#undef LO
};

#define PI 3.14159265358979323846
static double lsqrt(double x){
    if(!(x > 0)) return 0; int e = 0; double m = x; while(m >= 4){ m *= 0.25; e++; } while(m < 1){ m *= 4; e--; }
    double y = 0.5 * (1 + m); for(int i = 0; i < 6; i++) y = 0.5 * (y + m / y);
    while(e > 0){ y *= 2; e--; } while(e < 0){ y *= 0.5; e++; } return y;
}
static double lpow(double x, double y){ if(x <= 0) return 0; return lw_pw2(y * lw_lg2(x)); }
static double clampd(double v, double lo, double hi){ return v < lo ? lo : v > hi ? hi : v; }
static double lsin_deg(double d){ d = clampd(d, 0, 180); double p = d * (180 - d); return 4 * p / (40500 - p); }       // Bhaskara's sine, 0 to 180 degrees
static double u01(uint64_t a, uint64_t salt){ return (double)(lw_mix(a * 0x9E3779B97F4A7C15ull ^ lw_mix(salt + 0x51ull)) >> 11) / 9007199254740992.0; }
#define RR(s,id) ((double)(s)->v[id] / (double)(lw_defs[id].def ? lw_defs[id].def : 1))
#define VV(s,id) ((double)(s)->v[id])

static const LawSet *defaults(void){ static LawSet d; static int init; if(!init){ lw_default(&d); init = 1; } return &d; }

// ---- tables
static const double G_WIDTH[GT_N]  = { 100, 110, 60, 10, 200, 70, 15, 120 };                     // thousand light years
static const double G_STARSB[GT_N] = { 200, 250, 40, 0.5, 1500, 80, 3, 300 };                    // billions of stars
static const double G_SFRT[GT_N]   = { 1.5, 1.2, 4, 0.08, 0.03, 0.15, 0.6, 40 };                 // suns per year
static const double G_METALT[GT_N] = { 1, 1.15, 0.8, 0.25, 1.5, 1.2, 0.4, 1.3 };
static const double G_BHT[GT_N]    = { 4, 4.3, 1, 0.3, 1500, 50, 0.2, 40 };                       // million suns
static const double G_PLANT[GT_N]  = { 1, 1.05, 0.95, 0.6, 1.25, 1, 0.7, 0.9 };
static const double G_DANGT[GT_N]  = { 0, 0, 1, 0, 0, 0, 1, 3 };
static const double G_SPINT[GT_N]  = { 1, 1, 0.9, 0.5, 0.25, 0.8, 0.4, 1.2 };
static const double SC_MASS[SC_N]  = { 25, 5, 2, 1.35, 1, 0.72, 0.25, 0.6, 1.2, 1.4 };
static const double SC_LUM[SC_N]   = { 80000, 800, 20, 3.8, 1, 0.25, 0.012, 0.003, 150, 0.0002 };
static const double SC_TEMP[SC_N]  = { 38000, 16000, 8800, 6700, 5772, 4600, 3300, 11000, 3900, 600000 };
static const double SC_LIFE[SC_N]  = { 0.005, 0.1, 1.0, 3.5, 10, 25, 200, 1000, 1000, 1000 };    // billion years on the main sequence
static const double SC_FLARE[SC_N] = { 0.5, 0.5, 0.6, 0.8, 1, 1.4, 3, 0.2, 0.5, 10 };
static const double R_TAB[6]  = { 0.27, 0.5, 1.0, 1.6, 2.5, 4.0 };                                // the DNA's size classes, in earth radii
static const double R_MOON[6] = { 0.04, 0.10, 0.2, 0.27, 0.35, 0.5 };
static const double G_TAB[4]  = { 0.35, 0.70, 1.05, 1.40 };
static const double T_CLS[16] = { 15, 4, 18, 17, 38, -28, 28, 55, 20, 22, 180, 90, 60, 30, 70, -120 };  // average temperature of each of the game's world classes, celsius
static const double REARTH = 6.371e6, G0 = 9.81, STEP_M = 0.05;                                   // a bug's step is 5 cm

// ---- the model
static void core(const LawSet *s, const LwEnv *e, LwResult *r, const LwResult *ref){
    double *o = r->o; memset(r, 0, sizeof *r);
    const double G = RR(s, LW_U_GRAV), c = RR(s, LW_U_LIGHT), ex = RR(s, LW_U_EXPAND), dm = RR(s, LW_U_DARKM), de = RR(s, LW_U_DARKE);
    const double fus = RR(s, LW_U_FUSION), chem = RR(s, LW_U_ELEC), cosm = RR(s, LW_U_COSMIC), rad0 = RR(s, LW_U_RADIO), strong = RR(s, LW_U_STRONG);
    const double clump = RR(s, LW_U_CLUMP), mat = RR(s, LW_U_MATTER);
    const double age = VV(s, LW_U_AGE) / 10.0, cmb = VV(s, LW_U_CMB) / 10.0;
    const uint64_t gal = e->gal, st = e->star;
    // ================================================================ universe
    double om_m = 0.31 * mat * (1 + 0.3 * (dm - 1)), om_l = 0.69 * de;
    o[LO_U_AGE] = age; o[LO_U_HUBBLE] = 70.0 * ex * (0.75 + 0.25 * (om_l / 0.69 + 0.001)) * (1 + 0.08 * (om_m / 0.31 - 1));
    o[LO_U_OBS] = 46.5 * c * (age / 13.8) / (0.5 + 0.5 * ex);
    r->fate = ex < 0.05 ? 3 : om_l > 1.8 ? 1 : (om_l < 0.2 && om_m > 1.0) ? 2 : 0;
    o[LO_U_FATE] = r->fate; o[LO_U_CLUSTER] = VV(s, LW_U_SPACING); o[LO_U_GALBLOCK] = VV(s, LW_U_SPACING) * lw_pw2(VV(s, LW_U_GALBITS));
    o[LO_U_TRIPX] = 1.0 / c; o[LO_U_FUELX] = clampd(1 + 0.5 * (ex - 1) + 0.25 * (de - 1), 0.25, 50) * (VV(s, LW_U_SPACING) / 25.0);
    o[LO_U_DM] = clampd(27.0 * dm, 0, 95);
    // ================================================================ galaxy
    double w[GT_N]; for(int i = 0; i < GT_N; i++) w[i] = VV(s, LW_G_SPIRAL + i);
    w[GT_MERGE] *= RR(s, LW_G_COLLIDE) + 0.001; w[GT_RING] *= 0.5 + 0.5 * RR(s, LW_G_COLLIDE); w[GT_ELLIP] *= 0.5 + 0.5 * clump;
    double tot = 0; for(int i = 0; i < GT_N; i++) tot += w[i];
    int gt = 0;
    if(gal == 0) gt = s->v[LW_G_HOME] < 0 ? 0 : s->v[LW_G_HOME] > 7 ? 7 : s->v[LW_G_HOME];
    else if(tot > 0){ double ug = u01(gal, 11) * tot, acc = 0; gt = GT_N - 1; for(int i = 0; i < GT_N; i++){ acc += w[i]; if(ug < acc){ gt = i; break; } } }
    r->gtype = gt;
    double wk = G_WIDTH[gt] * RR(s, LW_G_SIZE) / lpow(clump, 0.3);
    double gage = age; if(gal) gage = age * (1 - VV(s, LW_G_AGESPR) / 100.0 * u01(gal, 12) * 0.7); if(gage < 0.3) gage = 0.3;
    double sfr = G_SFRT[gt] * RR(s, LW_S_SFR) * RR(s, LW_G_GAS) * (1 + 0.3 * (RR(s, LW_G_COLLIDE) - 1)) * (1 + 0.1 * (RR(s, LW_G_DUST) - 1));
    if(gt <= GT_BARRED) sfr *= lpow(12.0 / VV(s, LW_G_TWIST), 0.3);
    if(gt == GT_BARRED) sfr *= 1 + 0.2 * (RR(s, LW_G_BARLEN) - 1);
    if(gt == GT_RING) sfr *= lpow(RR(s, LW_G_RINGW), -0.3);
    double bulge = RR(s, LW_G_BULGE), halo = RR(s, LW_G_HALO);
    double stars = G_STARSB[gt] * RR(s, LW_G_STARS) * lpow(wk / G_WIDTH[gt], 2) * lpow(clump, 0.7) * lpow(mat, 0.3) * (0.8 + 0.2 * bulge) * (0.95 + 0.05 * halo);
    double bh = G_BHT[gt] * RR(s, LW_G_BH) * lpow(clump, 1.5) * (0.7 + 0.3 * bulge) * (gt == GT_BARRED ? 1 + 0.15 * (RR(s, LW_G_BARLEN) - 1) : 1);
    double rsun_ly = 0.27 * wk * 1000.0;
    double vspin = 220.0 * lsqrt(G * (0.4 + 0.6 * dm)) * lsqrt(RR(s, LW_G_SPIN)) * G_SPINT[gt] * (0.8 + 0.2 * lpow(wk / 100.0, 0.25));
    double agef = lsqrt(clampd(gage / 13.8, 0.01, 4));
    double metal = G_METALT[gt] * RR(s, LW_G_METAL) * lsqrt(strong) * lpow(chem, 0.3) * agef;
    double rfrac = gal == 0 ? 0.55 : clampd(0.05 + 0.95 * u01(gal ^ st, 13), 0.05, 1.0);   // where in the galaxy this star lies (0 centre, 1 edge)
    if(e->story) rfrac = 0.55;
    double mloc = metal * (1 + 0.8 * RR(s, LW_G_GRAD) * (1 - rfrac) * (1 - rfrac));
    double hc = 0.6, hw = 0.25 * RR(s, LW_G_HABZONE), din = rfrac < hc - hw ? hc - hw - rfrac : rfrac > hc + hw ? rfrac - hc - hw : 0;
    double hab = 1.0 / (1 + 6 * din);
    double jet = RR(s, LW_G_BHJET), grad = 0.1 / (rfrac + 0.1);
    double grad_rad = 0.5 + 0.5 * RR(s, LW_G_SUPERN) * (1 - rfrac) + 0.4 * RR(s, LW_G_BH) * lpow(clump, 1.5) * jet * grad * grad + 0.3 * (gt == GT_MERGE) * RR(s, LW_G_COLLIDE);
    o[LO_G_WIDTH] = wk; o[LO_G_STARS] = stars;
    o[LO_G_ARMS] = (gt == GT_SPIRAL || gt == GT_BARRED) ? VV(s, LW_G_ARMS) : gt == GT_RING ? 1 : 0;
    o[LO_G_BH] = bh; o[LO_G_SPIN] = vspin; o[LO_G_PERIOD] = 2 * PI * rsun_ly * 9.461e12 * c / (vspin + 1e-9) / 3.156e7 / 1e6;
    o[LO_G_SFR] = sfr; o[LO_G_METAL] = mloc; o[LO_G_AGE] = gage; o[LO_G_RFRAC] = rfrac * 100;
    o[LO_G_PITCH] = (gt <= GT_BARRED) ? VV(s, LW_G_TWIST) : 0; o[LO_G_BAR] = gt == GT_BARRED ? 0.25 * wk * RR(s, LW_G_BARLEN) : 0; o[LO_G_RINGW] = gt == GT_RING ? 0.12 * wk * RR(s, LW_G_RINGW) : 0;
    double needm = 0.5 * RR(s, LW_P_NEEDMET) + 0.2;
    o[LO_G_PLANETX] = G_PLANT[gt] * clampd(mloc / needm, 0, 2.5) * hab * (1 + 0.1 * (RR(s, LW_G_DUST) - 1));
    o[LO_G_RAD] = grad_rad; o[LO_G_DANGER] = G_DANGT[gt] + 2 * (grad_rad - 1);
    // ================================================================ star
    double sw[SC_N]; for(int i = 0; i < SC_N; i++) sw[i] = VV(s, LW_S_O + i);
    { double sf = lpow(RR(s, LW_S_SFR) + 0.01, 0.7); for(int i = 0; i <= SC_F; i++) sw[i] *= sf; double ag = clampd(gage / 13.8, 0.05, 3); sw[SC_WD] *= ag; sw[SC_NS] *= ag; }
    double stot = 0; for(int i = 0; i < SC_N; i++) stot += sw[i];
    int sc = SC_G; if(stot > 0){ double us = u01(st, 21) * stot, acc = 0; sc = SC_M; for(int i = 0; i < SC_N; i++){ acc += sw[i]; if(us < acc){ sc = i; break; } } }
    if(e->story) sc = s->v[LW_S_HOME] < 0 ? 0 : s->v[LW_S_HOME] > 9 ? 9 : s->v[LW_S_HOME];
    double young = VV(s, LW_S_YOUNG) / 100.0, sage = gage * lpow(u01(st, 22), 0.4 + 2.4 * young); if(e->story) sage = 4.6;
    double bright = RR(s, LW_S_BRIGHT);
    double life = SC_LIFE[sc] * RR(s, LW_S_LIFE) / (lpow(fus, 1.2) * lpow(bright, 0.6) * lpow(G, 0.5));
    double um = u01(st, 23);
    if(sc <= SC_K && sage > life){ if(sage < life * 1.12) sc = SC_GIANT; else if(sc <= SC_B && um < 0.6) sc = SC_NS; else sc = SC_WD; }
    r->sclass = sc;
    double lumf = lw_pw2((um - 0.5) * 0.6 * RR(s, LW_S_MASS));
    double L = SC_LUM[sc] * lumf * bright * fus * fus * lpow(G, 1.5), M = SC_MASS[sc], Tcol = SC_TEMP[sc] * RR(s, LW_S_TEMP);
    double Rs = lsqrt(L) / ((Tcol / 5772.0) * (Tcol / 5772.0) + 1e-30) * RR(s, LW_S_RADIUS); if(sc == SC_NS) Rs = 1.4e-5; if(sc == SC_WD) Rs = 0.012;
    double Tsurf = sc == SC_NS ? Tcol : 5772.0 * lpow(L / (Rs * Rs + 1e-30), 0.25);
    int nsuns = 1; if(u01(st, 24) < VV(s, LW_S_BIN) / 100.0){ nsuns = 2; if(u01(st, 25) < VV(s, LW_S_TRIPLE) / 100.0) nsuns = 3; }
    r->nsuns = nsuns; double lightx = 1 + 0.4 * (nsuns - 1);
    if(nsuns > 1) lightx *= 1 - 0.04 * VV(s, LW_S_ECL) / 100.0;
    double Ltot = L * lightx;
    double sqL = lsqrt(Ltot), hzc = 1.15 * sqL * RR(s, LW_S_HZPOS), hzw = RR(s, LW_S_HZ);
    double flare = RR(s, LW_S_FLARE) * RR(s, LW_S_MAGNET) * SC_FLARE[sc];
    o[LO_S_MASS] = M; o[LO_S_LUM] = Ltot; o[LO_S_TEMP] = Tsurf; o[LO_S_RADIUS] = Rs; o[LO_S_AGE] = sage; o[LO_S_LIFE] = life;
    o[LO_S_HZIN] = hzc * (1 - 0.2 * hzw) > 0.2 * hzc ? hzc * (1 - 0.2 * hzw) : 0.2 * hzc; o[LO_S_HZOUT] = hzc * (1 + 0.35 * hzw); o[LO_S_FLARE] = flare;
    // ================================================================ planet
    const int moon = e->moon > 0;
    r->rogue = !e->story && u01(e->seed_a, 31) < VV(s, LW_P_ROGUE) / 100.0;
    r->gas = !e->story && !moon && e->psz >= 3 && u01(e->seed_b, 32) < VV(s, LW_P_GAS) / 100.0;
    int psz = e->psz < 0 ? 0 : e->psz > 5 ? 5 : e->psz; int grv = e->grav < 0 ? 0 : e->grav > 3 ? 3 : e->grav; int cls = e->cls < 0 ? 0 : e->cls > 15 ? 15 : e->cls;
    double rdna = moon ? R_MOON[psz] : R_TAB[psz], dev = lw_lg2(rdna);
    double rratio = lw_pw2(dev * (RR(s, LW_P_SPREAD) - 1) + VV(s, LW_P_SIZE) / 4.0) * (r->gas ? 3.0 : 1.0);
    double Rm = REARTH * rdna * rratio, Rrel = Rm / REARTH;
    double dens = RR(s, LW_P_DENS) * (r->gas ? 0.25 : 1.0), dens_roll = 0.8 + 0.4 * u01(e->seed_c, 33);
    double gdna = G_TAB[grv] * G0, g = gdna * G * (dens) * rratio;
    double vesc = lsqrt(2 * g * Rm);
    double core_h = RR(s, LW_P_CORE), radio = rad0 * lpow(strong, 0.5), mag = RR(s, LW_P_MAGNET);
    double page = clampd(lpow(RR(s, LW_T_PLAGE), 1.0) * (sage > 0.1 ? (sage < age ? sage : age) : 0.1), 0.05, 100);
    if(e->story) page = 4.6;
    // orbit: where the planet is, worked out from how warm the game's own planet is (so the default laws keep it as it was)
    double Tdna = T_CLS[cls] + 273.15; double flux, a, press, dTg, dTi, alb, Teq;
    double ice = RR(s, LW_P_ICE), albk = 0.3 * RR(s, LW_P_ALBEDO) + 0.05 * (RR(s, LW_P_CLOUD) - 1);
    double wind = RR(s, LW_S_WIND), atmo = RR(s, LW_P_ATMO);
    dTi = clampd(1.5 * core_h * radio * lsqrt(Rrel / page + 0.001), 0, 400);
    a = ref ? ref->hid[0] * RR(s, LW_P_ORB0) * lpow(RR(s, LW_P_ORBSP), e->planet) : 1.0;
    for(int pass = 0; pass < (ref ? 1 : 6); pass++){                         // (the reference solves its orbit in a few passes: the air depends on the light and the light on the orbit)
        flux = Ltot / (a * a);
        double strip = 0.05 * wind * flux * lsqrt(flare + 0.001) / (0.3 + mag * lsqrt(core_h + 0.01));
        double ret = 2.0 / (1 + lpow(11186.0 / (vesc + 1e-3), 3)) / (1 + strip); ret = clampd(ret, 0, 1.5);
        press = atmo * lsqrt(g / G0) * ret; if(r->gas) press = atmo * 90;
        press = clampd(press, 0, 1e6);
        dTg = clampd(33.0 * RR(s, LW_P_GREEN) * (lsqrt(press) < 3 ? lsqrt(press) : 3.0) * (1 + 0.4 * (chem - 1)), 0, 1500);
        double tn = 278.0 * lpow(Ltot, 0.25) / lsqrt(a) * lpow((1 - clampd(albk, 0, 0.95)) / 0.7, 0.25);
        double icef = clampd((273.0 - (tn + dTg)) / 60.0, 0, 1); alb = clampd(albk * (1 + 0.6 * ice * icef), 0, 0.95);
        Teq = 278.0 * lpow(Ltot, 0.25) / lsqrt(a) * lpow((1 - alb) / 0.7, 0.25);
        if(!ref && pass < 5){                                                // solve the orbit that makes the planet exactly as warm as the game made it
            double need = Tdna - dTg - dTi; if(need < 25) need = 25;
            double Tn = 278.0 * lpow(Ltot, 0.25) * lpow((1 - alb) / 0.7, 0.25); a = (Tn / need) * (Tn / need); a = clampd(a, 0.005, 1e5);
        }
    }
    if(!ref) r->hid[0] = a;
    double Tk = Teq + dTg + dTi;
    if(r->rogue){ Tk = cmb + dTi + 0.3 * dTg; flux = 0; }
    Tk = clampd(Tk, cmb > 1 ? cmb : 1, 6000);
    double Mtot = M * (1 + 0.5 * (nsuns - 1));
    double year = lsqrt(a * a * a / (Mtot * G + 1e-12)) / RR(s, LW_T_ORBIT); double year_d = year * 365.25;
    if(r->rogue) year_d = 1e9;
    double alock = 0.2 * lpow(Mtot, 1.0 / 3) * lpow(RR(s, LW_P_TIDAL) + 0.001, 1.0 / 3) * lpow(RR(s, LW_T_SPINDN) + 0.001, 1.0 / 6) * lpow(RR(s, LW_P_MOONSZ), 0.1);
    r->locked = !r->rogue && a < alock && !e->story;
    double dayh = 24.0 * lw_pw2((u01(e->seed_a, 34) - 0.5) * 2.6) * RR(s, LW_T_DAY);
    if(r->locked) dayh = year_d * 24.0;
    double tilt = clampd(2.0 * VV(s, LW_P_TILT) * u01(e->seed_b, 35), 0, 180), ecc = clampd(0.35 * RR(s, LW_P_ECC) * u01(e->seed_c, 36) * u01(e->seed_c, 37), 0, 0.9);
    double seas = RR(s, LW_T_SEASON), so = clampd(Tk * 0.12 * lsin_deg(tilt) * seas / (1 + 0.5 * lsqrt(press)) + Tk * ecc * 0.25 * seas + Tk * 0.04 * RR(s, LW_S_FLICK), 0, 2000);
    double di = Tk * 0.1 * RR(s, LW_T_SWING) * lsqrt(dayh / 24.0) / (1 + lpow(press, 0.7)); if(r->locked) di = Tk * 0.35 * RR(s, LW_T_SWING) / (1 + press); di = clampd(di, 0, 3000);
    double Tc = Tk - 273.15;
    o[LO_P_TEMP] = Tc; o[LO_P_TMIN] = clampd(Tk - so - di, cmb > 1 ? cmb : 1, 6000) - 273.15; o[LO_P_TMAX] = Tk + so + di - 273.15;
    double wdna = (double)(e->water < 0 ? 0 : e->water > 100 ? 100 : e->water) / 100.0; if(e->water == 0) wdna = 0.45;
    double wfac = (Tc > 100 ? 1.0 / (1 + (Tc - 100) / 40.0) : Tc < -20 ? 0.5 : 1.0) * (press < 0.006 ? 0.0 : 1.0);
    double water = clampd(100.0 * wdna * RR(s, LW_P_WATER) * wfac, 0, 100);
    double radn = (0.5 * cosm + 0.5 * grad_rad + 0.4 * flux * flare + (sc == SC_NS ? 8 : 0) * (r->rogue ? 0 : 1)) / (0.35 + 0.65 * mag * lpow(core_h + 0.01, 0.3) + 0.4 * lsqrt(press));
    double fT = 1, dT = Tc > 15 ? Tc - 15 : 15 - Tc, tol = VV(s, LW_L_TOLER); if(dT > 10) fT = 1.0 / (1 + ((dT - 10) / tol) * ((dT - 10) / tol));
    double fw = clampd(water / 40.0, 0.15, 1), fp = press < 0.05 ? press / 0.05 : press > 30 ? 30 / press : 1;
    double fo = clampd(RR(s, LW_P_OXY) * 100.0 / (VV(s, LW_L_BREATH) + 1.0), 0, 1), fr = 1.0 / (1 + 0.3 * radn * radn);
    double fa = clampd(page / 0.8, 0, 1), fs = (sc == SC_NS || sc == SC_WD) ? 0.05 : (sc <= SC_B ? 0.1 : 1.0); if(r->rogue) fs = 0.02;
    double ev = RR(s, LW_L_EVOLVE);
    double lifeidx = 100.0 * RR(s, LW_L_CHANCE) * fT * fw * fp * fo * fr * fa * fs * lsqrt(ev) * (e->story ? 1 : 1);
    o[LO_P_RADIUS] = Rm; o[LO_P_CIRC] = 2 * PI * Rm / STEP_M; o[LO_P_G] = g; o[LO_P_GREL] = g / G0; o[LO_P_VESC] = vesc;
    o[LO_P_HORIZON] = lsqrt(2 * Rm * 0.03 + 0.0009); o[LO_P_WALK] = 2 * PI * Rm / 1.0;
    o[LO_P_ORBIT] = r->rogue ? 0 : a; o[LO_P_YEAR] = year_d; o[LO_P_DAY] = dayh; o[LO_P_TILT] = tilt;
    o[LO_P_PRESS] = press; o[LO_P_WATER] = water; o[LO_P_RAD] = radn; o[LO_P_LIFE] = lifeidx;
    o[LO_P_MONTH] = 27.3 / (lsqrt(G * dens * dens_roll) + 1e-9) / RR(s, LW_T_MOONORB);
    o[LO_P_SEASON] = (tilt < 2 && ecc < 0.02) ? 0 : year_d / 4.0 * RR(s, LW_T_SOLSTICE);
    o[LO_P_ORE] = RR(s, LW_P_ORE) * lsqrt(mloc) * lpow(strong, 0.5) * (0.8 + 0.2 * RR(s, LW_P_GEMS));
    o[LO_P_QUAKE] = 0.5 * (RR(s, LW_P_QUAKE) + RR(s, LW_P_VOLC)) * lsqrt(core_h * radio + 0.001) * lpow(Rrel + 1e-9, 0.3);
    o[LO_P_DENS] = dens * dens_roll; o[LO_P_LIGHT] = flux;
    { double grel = g / G0 + 1e-12, ro = RR(s, LW_P_ROUGH), mt = RR(s, LW_P_MOUNT);
      o[LO_P_WIND] = 20.0 * RR(s, LW_P_WIND) * (0.3 + lsqrt(press)) * lpow(24.0 / (dayh + 1e-6), 0.3) * (1 + 0.1 * (RR(s, LW_P_CLOUD) - 1));
      o[LO_P_STORMS] = 40.0 * RR(s, LW_P_STORM) * RR(s, LW_P_CLOUD) * lpow(RR(s, LW_T_WEATHER), 0.3) * clampd(lsqrt(press), 0, 3) * (1 + 0.1 * (RR(s, LW_P_WIND) - 1));
      o[LO_P_MOUNT] = 9.0 * mt * ro / (grel * (dens * dens_roll) + 1e-9) ; o[LO_P_ROUGH] = ro * (1 + 0.1 * RR(s, LW_P_QUAKE)) * lpow(mt, 0.5);
      o[LO_P_AURORA] = RR(s, LW_P_AURORA) * lsqrt(mag * wind) * lpow(flare + 0.001, 0.3) * (press > 0.01 ? 1 : 0.1);
      o[LO_P_SAND] = clampd(25.0 * RR(s, LW_P_DUNE) * (1 - water / 100.0) * (1 + 0.1 * (RR(s, LW_P_WIND) - 1)), 0, 100);
      o[LO_P_SNOWLINE] = (Tc > 0 ? Tc / 6.5 : 0) * RR(s, LW_P_SNOWLN) / lpow(ice + 0.01, 0.3);
      double evo = 0.5 * (ev + RR(s, LW_L_CHANCE)); double li = lifeidx / 100.0;
      o[LO_P_MIND] = clampd(100.0 * (1.0 - 1.0 / (1 + 0.5 * RR(s, LW_L_SMART) * li * evo * RR(s, LW_L_LIFESPAN))), 0, 100);
      o[LO_P_PLANTS] = clampd(60.0 * RR(s, LW_L_FLORA) * lsqrt(li) * (water > 5 ? 1 : 0.3), 0, 100);
      o[LO_P_BUGS] = RR(s, LW_L_BUGS) * lsqrt(li) * (1 + 0.1 * (RR(s, LW_L_NIGHT) - 1));
      o[LO_P_CREATURE] = 30.0 * RR(s, LW_L_CSIZE) / lpow(grel, 0.7) * lpow(dens * dens_roll + 1e-9, -0.2);
      o[LO_P_RARE] = clampd(3.0 * RR(s, LW_L_RARE) * (1 + 0.2 * (cosm - 1)), 0, 100);
      o[LO_P_LIFESPAN] = 20.0 * RR(s, LW_L_LIFESPAN) / lpow(grel, 0.2) * (1 + 0.1 * (RR(s, LW_L_EVOLVE) - 1));
      o[LO_P_FRIEND] = clampd(30.0 * RR(s, LW_L_FRIEND) / (0.5 + 0.5 * RR(s, LW_L_ANGER)), 0, 100);
      o[LO_P_RUINS] = clampd(33.0 * RR(s, LW_L_RUINS) * lpow(RR(s, LW_T_PLAGE), 0.5) * (0.5 + 0.5 * RR(s, LW_L_SMART)), 0, 100);
      o[LO_P_FOSSILS] = RR(s, LW_L_FOSSIL) * lsqrt(li + 0.01) * lpow(RR(s, LW_T_PLAGE), 0.7);
      o[LO_P_POISON] = clampd(8.0 * RR(s, LW_L_DISEASE) * (1 + 0.2 * (cosm - 1)), 0, 100);
      o[LO_P_NIGHTLIFE] = RR(s, LW_L_NIGHT) * (1 + 0.2 * (RR(s, LW_T_NIGHT) - 1)); o[LO_P_SEALIFE] = RR(s, LW_L_SEA) * lsqrt(water / 50.0 + 0.01);
      o[LO_P_WOBBLE] = 3.0 * RR(s, LW_T_WOBBLE) * lpow(RR(s, LW_P_MOONSZ), -0.4) * (1 + 0.3 * (RR(s, LW_P_TILT) > 0 ? 0 : 1));
      o[LO_P_ICEAGE] = VV(s, LW_T_ICEAGE) / (1 + 0.3 * (RR(s, LW_T_WOBBLE) - 1)) / lpow(ice + 0.01, 0.2);
      o[LO_P_WEATHER] = 3.0 / RR(s, LW_T_WEATHER) * lpow(dayh / 24.0 + 0.01, 0.5); }
    r->hid[1] = rratio; r->hid[2] = Tc; r->hid[3] = flux; r->hid[4] = press; r->hid[5] = water;
}

static void sanitize(LwResult *r){ for(int i = 0; i < LO_N; i++){ double v = r->o[i]; if(!(v == v)) v = 0; if(v > 1e300) v = 1e300; if(v < -1e300) v = -1e300; r->o[i] = v; } }
void lw_eval(const LawSet *s, const LwEnv *e, LwResult *r){
    LawSet eff; lw_effective(s, e->useed, &eff); LwResult ref;
    core(defaults(), e, &ref, 0); core(&eff, e, r, &ref); r->hid[0] = ref.hid[0]; sanitize(r);
}

// ---- what the game changes
static int roundi(double x){ return (int)(x < 0 ? x - 0.5 : x + 0.5); }
static int ci(int v, int lo, int hi){ return v < lo ? lo : v > hi ? hi : v; }
void lw_apply(const LawSet *s, const LwEnv *e, LwApply *a){
    memset(a, 0, sizeof *a); a->tw = 128; a->psz = e->psz; a->grav = e->grav; a->hunger_pct = 100; a->water_pct = 100;
    a->hour = a->weather = a->snow = a->sand = a->cls = a->city = a->ruins = a->fauna = a->beetles = a->rough = a->hs = a->mood = a->snowl = -1;
    int ps = e->psz < 0 ? 0 : e->psz > 5 ? 5 : e->psz; a->k16 = 16 * (int[]){ 12, 7, 4, 3, 2, 1 }[ps];
    if(lw_is_default(s)) return;
    LawSet eff; lw_effective(s, e->useed, &eff); LwResult ref, cur; core(defaults(), e, &ref, 0); core(&eff, e, &cur, &ref); sanitize(&cur); sanitize(&ref);
    const double *c = cur.o, *f = ref.o;
    // ---- size, horizon, gravity (story worlds follow these too)
    double rr = cur.hid[1], lq = 4.0 * lw_lg2(rr); a->size_q = roundi(lq);
    double circ = c[LO_P_CIRC];
    if(a->size_q != 0){
        double k = 16.0 * (int[]){ 12, 7, 4, 3, 2, 1 }[ps] * lpow(rr, -0.8);
        if(circ < 128){ int P = circ <= 8 ? 8 : circ <= 16 ? 16 : circ <= 32 ? 32 : circ <= 64 ? 64 : 128; a->tw = P; k = 16.0 * 256.0 * 2 * PI / P; }
        else { double cap = a->k16 > 200 ? a->k16 : 200; if(k > cap) k = cap; }
        a->k16 = ci(roundi(k), 0, 4000);
        int pc = 0; double best = 1e9, lr = lw_lg2((e->moon > 0 ? R_MOON[ps] : R_TAB[ps]) * rr);
        for(int i = 0; i < 6; i++){ double d = lr - lw_lg2(e->moon > 0 ? R_MOON[i] * 3.5 : R_TAB[i]); if(d < 0) d = -d; if(d < best){ best = d; pc = i; } }
        a->psz = pc;
    }
    double grel = c[LO_P_GREL], gr0 = G_TAB[e->grav < 0 ? 0 : e->grav > 3 ? 3 : e->grav];
    if(grel / gr0 > 1.03 || grel / gr0 < 0.97){ a->grav = grel < 0.525 ? 0 : grel < 0.875 ? 1 : grel < 1.225 ? 2 : 3; }
    if(grel > 1.4) a->slow = ci(roundi(lw_lg2(grel / 1.4) * 768.0), 0, 0x1000);
    if(grel < 0.2) a->zerog = ci(1 + (int)(lw_lg2(0.2 / (grel + 1e-30)) * 0.8), 1, 8);
    a->changed = a->size_q != 0 || a->grav != e->grav || a->slow || a->zerog;
    if(e->story) return;
    // ---- the rest follows only on the endless galaxy's planets
    double Tc = c[LO_P_TEMP], Tr = f[LO_P_TEMP], lc = c[LO_P_LIFE], lr0 = f[LO_P_LIFE] > 1e-6 ? f[LO_P_LIFE] : 1e-6;
    double lratio = clampd(lc / lr0, 0, 20), lsq = lsqrt(lratio);
    // light and the length of the day
    { double nc = 0.25 * RR(&eff, LW_T_NIGHT), dc = 0.125 * RR(&eff, LW_T_DUSK), u = u01(e->seed_b, 41);
      int hc = u < nc ? 2 : u < nc + dc ? 1 : 0, hr = u < 0.25 ? 2 : u < 0.375 ? 1 : 0;
      int h = hc != hr ? hc : -1; double lx = c[LO_P_LIGHT] / (f[LO_P_LIGHT] + 1e-30);
      int base = h >= 0 ? h : e->hour;
      if(cur.rogue) h = 2; else if(cur.locked && !ref.locked) h = base < 1 ? 1 : base; else if(lx < 0.02) h = 2; else if(lx < 0.15 && base < 1) h = 1;
      if(h >= 0 && h != e->hour) a->hour = h; }
    // weather and snow
    { double storm = RR(&eff, LW_P_STORM) * lsqrt(RR(&eff, LW_P_CLOUD)) * lpow(RR(&eff, LW_T_WEATHER), 0.1) * lpow(RR(&eff, LW_P_WIND), 0.2), u = u01(e->seed_c, 42); int w = e->weather;
      if(storm < 0.5 && w != 0 && u < 1.0 - storm) w = 0;
      if(storm > 1.5 && w == 0 && u < clampd((storm - 1.0) * 0.35, 0, 0.85)) w = Tc < 0 ? 2 : Tc > 40 ? 3 : 1;
      if(Tc >= 2 && w == 2 && Tr < 2) w = 1; if(Tc < 0 && w == 1 && Tr >= 0) w = 2;
      if(c[LO_P_PRESS] < 0.02) w = 0;
      if(w != e->weather) a->weather = w;
      if(Tr >= 2 && Tc < 0 && !e->snow) a->snow = 1; else if(Tr < 2 && Tc >= 6 && e->snow) a->snow = 0;
      int sand = e->sand; double dn = RR(&eff, LW_P_DUNE);
      if(Tc > 35 && Tr <= 35 && c[LO_P_WATER] < 30) sand = 1; else if(Tc < 20 && Tr > 35) sand = 0;
      if(dn > 1.5 && u01(e->seed_a, 43) < clampd((dn - 1) * 0.3, 0, 0.9)) sand = 1; if(dn < 0.5 && u01(e->seed_a, 44) < 1.0 - dn) sand = 0;
      if(sand != e->sand) a->sand = sand;
      double sl = (double)e->snowl * RR(&eff, LW_P_SNOWLN) * lw_pw2((Tc - Tr) / 30.0) / lpow(RR(&eff, LW_P_ICE) + 0.01, 0.3); int snl = ci(roundi(sl), 20, 255); if(snl != e->snowl) a->snowl = snl; }
    // class (temperature): only a big change of temperature changes the kind of world
    if(!e->hell){
        int nc = e->cls;
        if(Tc - Tr >= 40 || Tr - Tc >= 40){
            if(Tc < -100) nc = 15; else if(Tc < -15) nc = 5; else if(Tc > 160) nc = 10; else if(Tc > 48) nc = 4;
            else if(e->cls == 5 || e->cls == 4 || e->cls >= 10) nc = 0;
        }
        if(nc != e->cls) a->cls = nc;
    }
    // water
    { double wr = (f[LO_P_WATER] > 1e-6) ? c[LO_P_WATER] / f[LO_P_WATER] : (c[LO_P_WATER] > 1e-6 ? 4.0 : 1.0); a->water_pct = ci(roundi(100.0 * wr), 0, 400); }
    // life, cities, creatures
    { double fa = RR(&eff, LW_L_FAUNA), bg = RR(&eff, LW_L_BUGS);
      int nf = ci(roundi(e->fauna * fa * lsq), 0, 5); if(nf != e->fauna) a->fauna = nf;
      int nb = ci(roundi(e->beetles * bg * lsq), 0, 3); if(nb != e->beetles) a->beetles = nb;
      double cp = RR(&eff, LW_L_CITY) * lpow(RR(&eff, LW_L_SMART), 0.5) * lratio, u = u01(e->seed_a, 45);
      if(e->city && cp < 1.0 && u > cp) a->city = 0; else if(!e->city && cp > 1.0 && !e->hell && !(e->moon > 0) && u < clampd((cp - 1.0) * 0.33, 0, 0.9)) a->city = 1;
      double rp = RR(&eff, LW_L_RUINS) * lpow(RR(&eff, LW_T_PLAGE), 0.5) * (0.5 + 0.5 * RR(&eff, LW_L_SMART)), u2 = u01(e->seed_b, 46);
      if(e->ruins && rp < 1.0 && u2 > rp) a->ruins = 0; else if(!e->ruins && rp > 1.0 && !e->hell && !(e->moon > 0) && u2 < clampd((rp - 1.0) * 0.33, 0, 0.9)) a->ruins = 1;
      double an = RR(&eff, LW_L_ANGER), fr = RR(&eff, LW_L_FRIEND), u3 = u01(e->seed_c, 47); int m = e->mood;
      if(an > 1.5 && m != 2 && u3 < 0.5) m = 2; if(an < 0.5 && m == 2 && u3 < 0.5) m = 1;
      if(fr < 0.5 && m == 0 && u3 < 0.6) m = 1; if(fr > 1.5 && m == 1 && u3 < 0.5) m = 0;
      if(e->hell) m = 2; if(m != e->mood) a->mood = m; }
    // ground
    { double gfac = lpow(clampd(grel, 0.01, 100.0) / gr0, -0.35); int hs = ci(roundi((e->hs ? e->hs : 3) * RR(&eff, LW_P_MOUNT) * gfac), 1, 12); if(hs != (e->hs ? e->hs : 3)) a->hs = hs;
      int ro = ci(roundi(e->rough * lpow(RR(&eff, LW_P_ROUGH), 0.7)), 0, 3); if(ro != e->rough) a->rough = ro; }
    // sky traits of the game
    { double au = RR(&eff, LW_P_AURORA) * lsqrt(RR(&eff, LW_P_MAGNET) * RR(&eff, LW_S_WIND)); if(au > 1.3) a->aurora = ci(roundi((au - 1.2) * 6.0), 1, 25);
      double me = RR(&eff, LW_T_METEOR); if(me > 1.3) a->meteor = ci(roundi((me - 1.2) * 5.0), 1, 25);
      double ox = RR(&eff, LW_P_OXY), di = RR(&eff, LW_L_DISEASE); double bad = (ox < 0.6 ? (0.6 - ox) * 25 : 0) + (di > 1.5 ? (di - 1.5) * 4 : 0); if(bad >= 1) a->badair = ci(roundi(bad), 1, 25); }
    // hunger, danger, nodes
    { double gf = 1 + 0.25 * (grel > 1 ? lw_lg2(grel) : 0), gf0 = 1 + 0.25 * (gr0 > 1 ? lw_lg2(gr0) : 0), tf = 1 + 0.004 * ((Tc > 20 ? Tc - 20 : 20 - Tc)), tf0 = 1 + 0.004 * ((Tr > 20 ? Tr - 20 : 20 - Tr));
      a->hunger_pct = ci(roundi(100.0 * RR(&eff, LW_L_HUNGER) * (gf / gf0) * (tf / tf0)), 0, 900);
      a->danger_add = ci(roundi((c[LO_G_DANGER] - f[LO_G_DANGER]) + 2 * (c[LO_P_RAD] - f[LO_P_RAD]) + 3 * (RR(&eff, LW_L_ANGER) - 1) + 2 * (RR(&eff, LW_T_METEOR) - 1) * 0.5), -4, 12);
      a->node_add[0] = ci(roundi(3 * (RR(&eff, LW_L_FLORA) * lsq - 1)), -3, 8);
      a->node_add[1] = ci(roundi(3 * (c[LO_P_ORE] / (f[LO_P_ORE] + 1e-9) - 1)), -3, 8);
      a->node_add[2] = ci(roundi(3 * (RR(&eff, LW_L_BUGS) - 1)), -3, 8);
      a->node_add[3] = ci(roundi(3 * (RR(&eff, LW_L_FLORA) * RR(&eff, LW_P_WATER) * lsq - 1)), -3, 8); }
    a->changed = a->changed || a->hour >= 0 || a->weather >= 0 || a->cls >= 0 || a->fauna >= 0 || a->city >= 0 || a->hs >= 0 || a->water_pct != 100;
}

// ---------------------------------------------------------------------------------------------------- counts and chances
int lw_nplan(const LawSet *s, const LwEnv *e, int orig){
    if(e->story || lw_is_default(s)) return orig;
    LawSet eff; lw_effective(s, e->useed, &eff); LwResult ref, cur; core(defaults(), e, &ref, 0); core(&eff, e, &cur, &ref);
    double r = RR(&eff, LW_P_PERSTAR) * (cur.o[LO_G_PLANETX] / (ref.o[LO_G_PLANETX] + 1e-9)) * (1 - 0.1 * (cur.nsuns - ref.nsuns));
    r = clampd(r, 0, 100);
    int n = (int)(orig * r + u01(e->star, 51)); int cap = eff.v[LW_P_MAXPL] < 16 ? eff.v[LW_P_MAXPL] : 16;
    if(n > cap) n = cap; if(n > 16) n = 16; if(n < 1) n = 1; return n;
}
int lw_nmoon(const LawSet *s, const LwEnv *e, int orig){
    if(e->story || lw_is_default(s)) return orig;
    LawSet eff; lw_effective(s, e->useed, &eff);
    double r = RR(&eff, LW_P_MOONS) * lpow(RR(&eff, LW_P_MOONSZ), -0.2);
    int n = (int)(orig * r + u01(e->star ^ (uint64_t)(e->planet + 1) * 0x9E3779B1ull, 52)); int cap = eff.v[LW_P_MAXMOON] < 4 ? eff.v[LW_P_MAXMOON] : 4;
    if(n > cap) n = cap; if(n < 0) n = 0; return n;
}
int lw_dense(const LawSet *s, uint32_t roll, int odds){
    int base = odds > 0 && roll % (uint32_t)odds == 0, cr = s->v[LW_G_CROWD], d = lw_defs[LW_G_CROWD].def;
    if(cr == d) return base;
    if(cr < d) return base && ((roll >> 8) % (uint32_t)d) < (uint32_t)cr;
    return base || ((roll >> 8) % 1000u) < (uint32_t)((cr - d) * 1000 / (1000 - d));
}
int lw_hell_hit(const LawSet *s, int roll100, int hp){ return roll100 * 100 < hp * s->v[LW_P_HELL]; }
int lw_weird_hit(const LawSet *s, uint32_t f){
    int v = s->v[LW_L_WEIRD], base = (f % 111u) == 0;
    if(v == 100) return base; if(v < 100) return base && ((f >> 8) % 100u) < (uint32_t)v;
    return base || ((f >> 12) % 100000u) < (uint32_t)((v - 100) * 9);
}
int lw_uly(const LawSet *s){ int v = s->v[LW_U_SPACING]; return v < 1 ? 1 : v; }
int lw_galbits(const LawSet *s){ int v = s->v[LW_U_GALBITS]; return v < 4 ? 4 : v > 40 ? 40 : v; }
uint32_t lw_hours(const LawSet *s, uint32_t hours){
    int c = s->v[LW_U_LIGHT]; if(c == 100) return hours; if(c < 1) c = 1;
    uint64_t v = (uint64_t)hours * 100u / (uint64_t)c; if(v < 1) v = 1; if(v > 0xFFFFFFFFull) v = 0xFFFFFFFFull; return (uint32_t)v;
}
int lw_warp_cost(const LawSet *s, int cost){
    double ex = RR(s, LW_U_EXPAND), de = RR(s, LW_U_DARKE); double x = clampd(1 + 0.5 * (ex - 1) + 0.25 * (de - 1), 0.25, 50);
    if(x > 0.9995 && x < 1.0005) return cost; int v = roundi(cost * x); return v < 1 ? 1 : v;
}
int lw_time_pct(const LawSet *s){ return s->v[LW_T_SPEED]; }
