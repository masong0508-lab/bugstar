// pc_sprgen_game.inc: the glue between the procedural sprites (pc_sprgen*.c) and the game's views. Included at the end of the generated main.c (build.sh), after
// pc_snative.inc, so it can read the game's own state (wd, po[], sys[], clus[]) and the native views' buffers. Everything here only runs in NATIVE / ULTRA: in CLASSIC
// (or at 240 x 160) the hooks return 0 and the game draws its own sprites, so the CLASSIC frames are not touched. Off for tests: BUGSTAR_SPRGEN=0.
//   1. the planet's objects (trees, plants, rocks, ruins, houses, people, creatures) are drawn into the native picture instead of the game's tile sprites;
//   2. the planets and moons of the space view get the generated discs (bands, continents, rings, city lights);
//   3. the SPRITE GALLERY (PC LAB): every generator, browsed by its number.
#include "pc_sprgen.h"
#include "content/pcx.h"

static int pcg_off = -1;
static int pcg_disabled(void){ if(pcg_off < 0){ const char *e = getenv("BUGSTAR_SPRGEN"); pcg_off = (e && e[0] == '0') ? 1 : 0; } return pcg_off; }

// ---- what this world is made of (plants, peoples, city, creatures), worked out once per planet
typedef struct { signed char temp, wet, sea; unsigned char clim, style, pcls; } PcgCls;
static const PcgCls pcg_cls[16] = {                                 // by the game's planet class (PC_TEMPERATE ...): temperature, wetness, sea, building climate, ground style, disc class
    {15,55,30,0,0,0}, {3,45,20,2,3,0}, {18,60,20,0,0,0}, {22,80,80,5,1,5}, {35,10,5,1,1,1}, {-20,30,10,2,2,2}, {28,90,30,3,6,9}, {10,20,10,4,4,7},
    {15,40,20,0,3,8}, {24,70,40,5,0,0}, {60,5,0,4,4,3}, {55,5,0,4,4,3}, {30,10,0,4,4,6}, {5,10,0,1,3,6}, {20,40,70,4,5,11}, {-60,20,10,2,2,2} };
static PcxVis pcg_v; static u32 pcg_vkey; static int pcg_vok;
static u32 pcg_tree[4], pcg_plant[4]; static int pcg_ntree, pcg_nplant, pcg_cc;
static u32 pcg_world_key(void){
    u32 h = hmix(wd.tseed ^ 0x51ED270Bu); h = hmix(h ^ wd.cseed ^ (u32)wd.cls * 0x9E3779B1u ^ (u32)(cur_sys + 1) * 0x85EBCA6Bu ^ (u32)(cur_pl + 1) * 0xC2B2AE35u ^ (u32)(cur_moon + 7) * 0x27D4EB2Fu);
    for(int i = 0; i < 5; i++) h = hmix(h ^ clus[i] ^ clus[UAX + i] * 31u); h = hmix(h ^ uni_w[0]); return h ? h : 1;
}
static void pcg_world(void){
    u32 key = pcg_world_key(); if(pcg_vok && key == pcg_vkey) return;
    int c = wd.cls < 16 ? wd.cls : 0; pcg_cc = c; memset(&pcg_v, 0, sizeof pcg_v);
    int story = !endless || cur_sys < 0 || cur_sys >= NSYS || sys[cur_sys].kind < 6;
    if(!story){ u32 cx[5], cy[5]; for(int i = 0; i < 5; i++){ cx[i] = clus[i]; cy[i] = clus[UAX + i]; } pcx_vis(uni_w, cx, cy, cur_sys, cur_pl, cur_moon, &pcg_v); }
    else pcx_vis_seed(((uint64_t)hmix(wd.tseed) << 32) | hmix(wd.cseed ^ 0x77u), pcg_cls[c].temp, pcg_cls[c].wet, pcg_cls[c].sea, &pcg_v);
    pcg_ntree = pcg_nplant = 0; uint32_t sd = hmix(key);
    for(int i = 0; i < pcg_v.nflora; i++){ uint32_t id = pcg_v.flora[i]; if(sg_flora_woody(id)){ if(pcg_ntree < 4) pcg_tree[pcg_ntree++] = id; } else if(pcg_nplant < 4) pcg_plant[pcg_nplant++] = id; }
    if(!pcg_ntree) pcg_tree[pcg_ntree++] = sg_flora_tree(sd);
    if(!pcg_nplant) pcg_plant[pcg_nplant++] = ((sd >> 3) << 11) | ((sd % 7u) << 6) | (sd >> 20 & 63u);       // a low plant (forms 0..6 are not trees)
    pcg_vkey = key; pcg_vok = 1;
}

// ---- one sprite to make: the generator and its numbers. The cache key is a hash of all of them.
typedef struct { int gen; uint32_t a, b, c, d; } PcgArg;
enum { PG_FLORA, PG_BUILD, PG_PERSON, PG_CRIT, PG_PROP };
static void pcg_make(SgImg *im, void *vp){
    PcgArg *g = (PcgArg *)vp;
    switch(g->gen){
    case PG_FLORA: sg_flora(im, g->a, g->b); break;
    case PG_BUILD: { SgBStyle st; sg_bstyle(&st, (int)g->a, (int)g->b, (int)(g->c & 15), (int)(g->c >> 4 & 7), g->d); st.floors = (uint8_t)(g->c >> 8 & 7); st.hour = (uint8_t)(g->c >> 12 & 3); st.flags = 1; sg_building(im, &st); } break;
    case PG_PERSON: sg_person(im, (int)g->a, g->b); break;
    case PG_CRIT: { SgCrit cr; memset(&cr, 0, sizeof cr); cr.kind = (uint16_t)g->a; cr.gift = 255; cr.colour = (uint8_t)(g->b & 15); cr.rank = (uint8_t)(g->b >> 4 & 7); cr.mut = (uint8_t)(g->b >> 8 & 15); cr.scene = 0; cr.seed = g->d; sg_creature(im, &cr); } break;
    case PG_PROP: sg_prop(im, (int)g->a, g->b, (int)g->c); break;
    }
}
static int pcg_bucket(int n){ static const short bk[] = { 16, 20, 26, 32, 40, 50, 64, 80, 100, 128, 160, 200, 256 }; for(unsigned i = 0; i < sizeof bk / sizeof bk[0]; i++) if(n <= bk[i]) return bk[i]; return 256; }
static const SgImg *pcg_get(const PcgArg *a, int px){
    int bsz = pcg_bucket(px); uint32_t k = hmix((uint32_t)a->gen * 0x9E3779B1u ^ a->a); k = hmix(k ^ a->b * 0x85EBCA6Bu); k = hmix(k ^ a->c * 0xC2B2AE35u); k = hmix(k ^ a->d * 0x27D4EB2Fu);
    PcgArg t = *a; return sg_cached_ex(k, bsz, bsz, pcg_make, &t);
}

// ---- the planet's objects
static void pcg_shadow(double cx, double cy, double rx, double ry, int a256){        // a soft dark patch on the ground (window pixels; cy counts from the picture's top row)
    int W = pc_pn.w, Hb = pc_pn.h - pc_pn.ya; int X0 = (int)(cx - rx) - 1, X1 = (int)(cx + rx) + 2, Y0 = (int)(cy - ry) - 1, Y1 = (int)(cy + ry) + 2; if(X0 < 0) X0 = 0; if(X1 > W) X1 = W; if(Y0 < 0) Y0 = 0; if(Y1 > Hb) Y1 = Hb;
    for(int Y = Y0; Y < Y1; Y++){ double dy = (Y + 0.5 - cy) / ry; for(int X = X0; X < X1; X++){ double dx = (X + 0.5 - cx) / rx, q = dx * dx + dy * dy; if(q >= 1.0) continue;
            int a = (int)((1.0 - q) * (1.0 - q) * a256); uint32_t *p = &pc_pn.buf[(size_t)Y * W + X], d = *p; int r = (int)(d >> 16 & 255), g = (int)(d >> 8 & 255), b = (int)(d & 255);
            r -= r * a >> 8; g -= g * a >> 8; b -= b * a >> 8; *p = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; } }
}
// what this object looks like: fills the generator numbers, returns its box side in cells (0: leave it to the game). lift: how far above the ground it floats (fraction of the box)
static double pcg_pick(const PObj *o, int idx, PcgArg *a, double *lift, int *shadow){
    pcg_world(); uint32_t h = hmix((uint32_t)idx * 0x9E3779B1u ^ (uint32_t)o->kind * 0x85EBCA6Bu ^ wd.tseed), v = o->var; double vr = 0.85 + 0.3 * ((h >> 8 & 255) / 255.0);
    int np = pcg_v.npeople, nb = pcg_v.nbeing; int pe = np ? pcg_v.people[(o->kind + v) % (unsigned)np] : (int)(wd.cseed % 512u); int city = pcg_v.city;
    int tech = wd.csize * 6 / 10; if(tech > 5) tech = 5; int clim = pcg_cls[pcg_cc].clim, style = pcg_cls[pcg_cc].style; *lift = 0; *shadow = 1; memset(a, 0, sizeof *a);
    switch(o->kind){
    case O_TREE: a->gen = PG_FLORA; a->a = pcg_tree[v % (unsigned)pcg_ntree]; a->b = h & 3; return 2.6 * vr;
    case O_PLANT: a->gen = PG_FLORA; a->a = pcg_plant[v % (unsigned)pcg_nplant]; a->b = h & 3; return 1.5 * vr;
    case O_FLOWER: a->gen = PG_FLORA; a->a = pcg_plant[(v + 1) % (unsigned)pcg_nplant]; a->b = h & 3; return 1.1 * vr;
    case O_TUFT: a->gen = PG_FLORA; a->a = pcg_plant[(v + 2) % (unsigned)pcg_nplant]; a->b = h & 3; *shadow = 0; return 0.9 * vr;
    case O_ROCK: a->gen = PG_PROP; a->a = (v & 1) ? SGP_BOULDER : SGP_ROCK; a->b = h & 7; a->c = (uint32_t)style; return 1.5 * vr;
    case O_PEBBLE: a->gen = PG_PROP; a->a = SGP_PEBBLES; a->b = h & 7; a->c = (uint32_t)style; *shadow = 0; return 0.8;
    case O_FOSSIL: a->gen = PG_PROP; a->a = SGP_FOSSIL; a->b = h & 7; a->c = (uint32_t)style; return 1.7;
    case O_RUIN: { static const uint8_t rk[4] = { SGP_RUIN, SGP_COLUMN, SGP_OBELISK, SGP_RUIN }; a->gen = PG_PROP; a->a = rk[(v + (h >> 4)) & 3]; a->b = h & 7; a->c = (uint32_t)style; return 2.3 * vr; }
    case O_SHRINE: a->gen = PG_PROP; a->a = SGP_SHRINE; a->b = h & 7; a->c = (uint32_t)style; return 2.4;
    case O_BEACON: a->gen = PG_PROP; a->a = SGP_BEACON; a->b = h & 7; a->c = (uint32_t)style; return 2.2;
    case O_HOME: case O_BASE: case O_MARKET: case O_CANTINA: case O_OFFICE: case O_RACE: case O_STALL: {
        static const uint8_t fl[29] = { 0,0,0,0,0,0,0,0,0, 4,2,2,5,2,1, 0,0,0,0,0,0,0, 1, 0,0,0,0,0,0 }; static const float sz[29] = { 0,0,0,0,0,0,0,0,0, 3.4f,3.1f,2.9f,3.5f,3.1f,2.7f, 0,0,0,0,0,0,0, 1.7f, 0,0,0,0,0,0 };
        int floors = fl[o->kind]; if(o->kind == O_HOME) floors = 1 + (int)(h >> 5 & 1); if(o->kind == O_BASE) floors = 3;
        a->gen = PG_BUILD; a->a = (uint32_t)city; a->b = (uint32_t)pe; a->c = (uint32_t)(clim | tech << 4 | floors << 8 | (int)wd.hour << 12); a->d = (h & 7) + o->kind * 17u; return sz[o->kind]; }
    case O_LOCAL: a->gen = PG_PERSON; a->a = (uint32_t)pe; a->b = h; return 1.8;
    case O_CRIT: { if(!nb) return 0; int kind = pcg_v.being[v % (unsigned)nb]; int plan = kind % 66, size = (kind / 66) / 48; a->gen = PG_CRIT; a->a = (uint32_t)kind; a->b = (h >> 3 & 15) | (h >> 7 & 7) << 4; a->d = h;
        if(plan >= 11 && plan < 22){ *lift = 0.28; } return 1.0 + 0.33 * size; }
    case O_BEETLE: case O_SLUG: { int plan = 33 + (int)(h >> 5) % 11, variant = (int)(h >> 12) % 48; a->gen = PG_CRIT; a->a = (uint32_t)(variant * 66 + plan); a->b = h >> 3 & 15; a->d = h; return o->kind == O_SLUG ? 1.2 : 0.9; }
    default: return 0;
    }
}
// called in the planet's sprite loop for every object that is in view (planet.inc, build.sh puts the call in). 1: drawn here, the game's own tile sprite is skipped.
static int pc_sg_obj(const void *vo, int idx, int sx, int sy, int f, int zc){
    if(!pn_active || pc_pn.w <= 0 || kr_mode || pcg_disabled()) return 0; const PObj *o = (const PObj *)vo;
    PcgArg a; double lift; int shadow; double cells = pcg_pick(o, idx, &a, &lift, &shadow); if(cells <= 0) return 0;
    double kx = pc_pn.w / 240.0, ky = pc_pn.h / 160.0, S = cells * f / 256.0; double wpx = S * kx, hpx = S * ky; if(wpx < 3.0) return 1;
    int W = pc_pn.w, ya = pc_pn.ya, Hb = pc_pn.h - ya; double fx = (sx + 0.5) * kx, fy = (sy + 0.5) * ky - ya;       // the feet, in the picture's pixels
    if(fx + wpx < 0 || fx - wpx > W || fy - hpx * 1.2 > Hb || fy + hpx * 0.3 < 0) return 1;
    const SgImg *im = pcg_get(&a, (int)(wpx > hpx ? wpx : hpx) + 1); if(!im) return 0;
    int zcells = zc >> 8; int fog = (zcells - 6) * 7; if(fog < 0) fog = 0; if(fog > 190) fog = 190; uint32_t hz = pn_pal[SKY_B + 7], fogc = hz; int hour = wd.hour;
    if(hour == 2){ fogc = ((hz >> 16 & 255) * 3 >> 3) << 16 | ((hz >> 8 & 255) * 3 >> 3) << 8 | ((hz & 255) * 4 >> 3); if(fog < 105) fog = 105; }
    else if(hour == 1){ fogc = (((hz >> 16 & 255) + 255) >> 1) << 16 | (((hz >> 8 & 255) + 140) >> 1) << 8 | (((hz & 255) + 70) >> 1); if(fog < 40) fog = 40; }
    if(shadow){ double sw = wpx * 0.36, sh = hpx * 0.07 + 1.0; pcg_shadow(fx + (hour == 1 ? wpx * 0.08 : 0), fy, sw, sh, hour == 2 ? 70 : 105); }
    SgBlit b; memset(&b, 0, sizeof b); b.w = (float)wpx; b.h = (float)hpx; b.x = (float)(fx - wpx * 0.5); b.y = (float)(fy - hpx * (0.94 + lift)); b.flags = (idx & 1) ? 1 : 0; b.fog = fog; b.fogc = fogc;
    sg_blit(pc_pn.buf, W, Hb, im, &b); return 1;
}

// ---- the space view: the generated disc over the game's own planet and moons
typedef struct { uint32_t id; uint32_t sig; int n; SgImg *im; uint32_t stamp; } PcgPl;
static PcgPl pcg_pl[6]; static uint32_t pcg_plclock; static int pcg_main = -1;
static void pc_sg_mark(void){ pcg_main = sn_nc; }                                 // (space.inc: the next recorded disc is the planet you are at)
static void pc_sg_space_planet(uint32_t *buf, int W, int hn, double sx, double sy, void *vc, int ci){
    SnCall *c = (SnCall *)vc;
    if(pcg_disabled()) return; double Rx = (c->r + 0.5) * sx, Ry = (c->r + 0.5) * sy; if(Rx < 6.0) return;
    int ismain = (ci == pcg_main); uint64_t seed; int cls; SgPlanetSpec sp; pcg_world();
    uint32_t sid = hmix(sys_id(cur_sys) ^ (u32)(cur_pl + 1) * 0x9E3779B1u ^ (u32)(cur_moon + 3) * 0x85EBCA6Bu ^ (pcg_vkey & 0xFFFFu));
    if(ismain){ seed = ((uint64_t)sid << 32) | hmix(sid ^ 0x1234567u); cls = pcg_cls[pcg_cc].pcls; }
    else { int mid = (c->rot - (int)(frames >> 3)) & 127; seed = ((uint64_t)hmix(sid ^ (u32)mid * 0x632BE5ABu) << 32) | (uint32_t)(mid * 2654435761u + 9); cls = (mid & 3) == 0 ? 2 : 6; }
    sg_planet_spec(&sp, seed, cls); sp.moons = 0; if(!ismain) sp.ring = 0;
    double frac = sg_planet_frac(&sp); if(frac < 0.05) frac = 0.05; int N = (int)(2.0 * Rx / frac) + 1; if(N > 320) N = 320; N = (N + 15) & ~15; if(N < 32) N = 32;
    float Lx = (float)c->Lx, Ly = (float)c->Ly, Lz = -(float)c->Lz, L = sqrtf(Lx * Lx + Ly * Ly + Lz * Lz); if(L < 1) L = 1; Lx /= L; Ly /= L; Lz /= L;
    int qx = (int)floorf(Lx * 48 + 0.5f), qy = (int)floorf(Ly * 48 + 0.5f), qz = (int)floorf(Lz * 48 + 0.5f), rot = c->rot & 127;
    uint32_t id = hmix((uint32_t)(seed >> 32) ^ (uint32_t)seed) | 1u, sig = hmix((uint32_t)rot | (uint32_t)(qx + 64) << 8 | (uint32_t)(qy + 64) << 16 | (uint32_t)(qz + 64) << 24) ^ (uint32_t)N;
    PcgPl *s = 0; int old = 0; pcg_plclock++; for(int i = 0; i < 6; i++){ if(pcg_pl[i].id == id){ s = &pcg_pl[i]; break; } if(pcg_pl[i].stamp < pcg_pl[old].stamp) old = i; }
    if(!s){ s = &pcg_pl[old]; sg_img_free(s->im); s->im = 0; s->id = id; s->sig = 0; s->n = 0; }
    s->stamp = pcg_plclock;
    if(!s->im || s->n != N || s->sig != sig){ if(!s->im || s->n != N){ sg_img_free(s->im); s->im = sg_img_new(N, N); s->n = N; } if(!s->im) return;
        sg_img_clear(s->im); sg_planet(s->im, &sp, rot / 128.0f, qx / 48.0f, qy / 48.0f, qz / 48.0f); s->sig = sig; }
    double cxp = (c->cx + 0.5) * sx, cyp = (c->cy + 0.5) * sy; SgBlit b; memset(&b, 0, sizeof b);
    double scx = Rx * 1.006 / (frac * N), scy = Ry * 1.006 / (frac * N);                                              // window px per image px
    b.w = (float)(N * scx); b.h = (float)(N * scy); b.x = (float)(cxp - b.w * 0.5); b.y = (float)(cyp - b.h * 0.5); b.clip_y0 = 0; b.clip_y1 = hn;
    sg_blit(buf, W, hn, s->im, &b);
}

// ---- the SPRITE GALLERY (PC LAB): every generator, browsed by its number
static int pcg_gen, pcg_id[32], pcg_hold; static uint32_t pcg_clock; static int pcg_planets = -1;
static uint32_t pcg_mix(uint32_t a, uint32_t b, int f){ uint32_t rb = ((a & 0xFF00FF) * (256 - f) + (b & 0xFF00FF) * f) >> 8, g = ((a & 0xFF00) * (256 - f) + (b & 0xFF00) * f) >> 8; return (rb & 0xFF00FF) | (g & 0xFF00); }
static void pcg_box(uint32_t *out, int w, int h, double gx, double gy, double gw, double gh, uint32_t fill, uint32_t edge){
    double kx = w / 240.0, ky = h / 160.0; int x0 = (int)(gx * kx), x1 = (int)((gx + gw) * kx), y0 = (int)(gy * ky), y1 = (int)((gy + gh) * ky); if(x0 < 0) x0 = 0; if(y0 < 0) y0 = 0; if(x1 > w) x1 = w; if(y1 > h) y1 = h;
    for(int y = y0; y < y1; y++) for(int x = x0; x < x1; x++) out[(size_t)y * w + x] = (x == x0 || x == x1 - 1 || y == y0 || y == y1 - 1) ? edge : fill;
}
static void pcg_show(uint32_t *out, int w, int h, int gen, uint32_t id, double gx, double gy, double gw, double gh, double pad, int spin){
    const SgGen *g = &sg_gens[gen]; double kx = w / 240.0, ky = h / 160.0; double bw = (gw - pad * 2) * kx, bh = (gh - pad * 2) * ky; double asp = (double)g->w / g->h; double dw = bw, dh = bw / asp; if(dh > bh){ dh = bh; dw = bh * asp; }
    int px = (int)(dw > dh ? dw : dh) + 1; SgBlit b; memset(&b, 0, sizeof b); b.w = (float)dw; b.h = (float)dh; b.x = (float)(gx * kx + (gw * kx - dw) * 0.5); b.y = (float)(gy * ky + (gh * ky - dh) * 0.5);
    if(spin && gen == pcg_planets){ static SgImg *pim; static int psz; int N = px > 256 ? 256 : px < 32 ? 32 : px; if(!pim || psz != N){ sg_img_free(pim); pim = sg_img_new(N, N); psz = N; } if(!pim) return; sg_img_clear(pim);
        SgPlanetSpec sp; sg_planet_spec(&sp, (uint64_t)id * 7919u + 5, id < 12 ? (int)id : -1); float a = pcg_clock * 0.0035f; sg_planet(pim, &sp, a - floorf(a), -0.65f, 0.4f, 0.65f); sg_blit(out, w, h, pim, &b); return; }
    const SgImg *im = sg_cached(gen, id, px > 256 ? 256 : px); if(im) sg_blit(out, w, h, im, &b);
}
static void pcg_ovl(uint32_t *out, int w, int h){
    for(int y = 0; y < h; y++){ uint32_t c = pcg_mix(0x0E131C, 0x1B2433, y * 256 / h); uint32_t *r = out + (size_t)y * w; for(int x = 0; x < w; x++) r[x] = c; }
    int gen = pcg_gen; uint32_t id = (uint32_t)pcg_id[gen]; const SgGen *g = &sg_gens[gen];
    pcg_box(out, w, h, 5, 16, 108, 108, 0x0A0E15, 0x46587A); pcg_show(out, w, h, gen, id, 5, 16, 108, 108, 2, 1);
    uint32_t base = id / 12 * 12;
    for(int i = 0; i < 12; i++){ uint32_t tid = base + (uint32_t)i; if(tid >= g->nids) break; double gx = 118 + (i & 3) * 30.5, gy = 16 + (i >> 2) * 36.5; int sel = tid == id;
        pcg_box(out, w, h, gx, gy, 29.5, 35, sel ? 0x24324A : 0x111823, sel ? 0xF0C040 : 0x2C3A52); pcg_show(out, w, h, gen, tid, gx, gy, 29.5, 35, 1.5, 0); }
    pcg_box(out, w, h, 5, 127, 230, 9, 0x0A0E15, 0x0A0E15); pcg_box(out, w, h, 5, 137, 230, 8, 0x0A0E15, 0x0A0E15);
}
static void pcg_text(volatile u16 *pg){
    char b[64], l[48]; const SgGen *g = &sg_gens[pcg_gen]; uint32_t id = (uint32_t)pcg_id[pcg_gen];
    rect(pg, 0, 0, 240, 160, 0);
    scpy(b, "SPRITE GALLERY: "); scat(b, g->name); scat(b, "  "); ncat(b, pcg_gen + 1); scat(b, "/"); ncat(b, sg_ngens); text(pg, 6, 4, b, C_GOLD, 1);
    scpy(b, "NUMBER "); ncat(b, (int)id); scat(b, " OF "); ncat(b, (int)g->nids); rtext(pg, 234, 4, b, C_GREY);
    l[0] = 0; g->label(id, l, sizeof l); l[40] = 0; text(pg, 7, 128, l, C_WHITE, 1);
    scpy(b, "THE SAME NUMBER ALWAYS MAKES THE SAME PICTURE"); text(pg, 7, 138, b, C_DGREY, 1);
    text(pg, 6, 148, "L R: KIND  LEFT RIGHT: NUMBER  UP DOWN: ROW  A: RANDOM", C_CYAN, 1); text(pg, 6, 154, "SEL: +100  B: BACK", C_GREEN, 1);
}
static void pc_sg_gallery(volatile u16 *pg){
    oam_hide_all(); oam_flush(); wait_release(); pcg_planets = -1; for(int i = 0; i < sg_ngens; i++) if(!strcmp(sg_gens[i].name, "PLANETS")) pcg_planets = i;
    { const char *e = getenv("BUGSTAR_SG"); if(e){ int gi = 0, id = 0; if(sscanf(e, "%d:%d", &gi, &id) >= 1){ if(gi >= 0 && gi < sg_ngens) pcg_gen = gi; if(id >= 0 && (uint32_t)id < sg_gens[pcg_gen].nids) pcg_id[pcg_gen] = id; } } }
    u16 s0 = BG_PAL[0]; pc_ovl_fn = pcg_ovl; int dirty = 1, hold = 0, last = -1;
    while(1){
        if(dirty){ dirty = 0; pcg_text(pg); }
        vsync(); read_keys(); pcg_clock++; const SgGen *g = &sg_gens[pcg_gen]; int step = 0, pg_ = 0, rep = 0;
        if(keys & (K_LEFT | K_RIGHT | K_UP | K_DOWN)){ int kk = keys & (K_LEFT | K_RIGHT | K_UP | K_DOWN); if(kk != last){ hold = 0; last = kk; } else hold++; rep = hold > 20 && (hold & 3) == 0; } else { hold = 0; last = -1; }
        if(HIT(K_LEFT) || (rep && (keys & K_LEFT))) step = -1; if(HIT(K_RIGHT) || (rep && (keys & K_RIGHT))) step = 1;
        if(HIT(K_UP) || (rep && (keys & K_UP))) step = -4; if(HIT(K_DOWN) || (rep && (keys & K_DOWN))) step = 4;
        if(HIT(K_SEL)) step = 100; (void)pg_;
        if(step){ long v = (long)pcg_id[pcg_gen] + step; long n = (long)g->nids; v = ((v % n) + n) % n; pcg_id[pcg_gen] = (int)v; dirty = 1; sfx(7); }
        if(HIT(K_A)){ pcg_id[pcg_gen] = (int)(hmix(pcg_clock * 2654435761u ^ (u32)pcg_gen * 77u) % g->nids); dirty = 1; sfx(7); }
        if(HIT(K_L)){ pcg_gen = (pcg_gen + sg_ngens - 1) % sg_ngens; dirty = 1; sfx(7); } if(HIT(K_R)){ pcg_gen = (pcg_gen + 1) % sg_ngens; dirty = 1; sfx(7); }
        if(HIT(K_B) || HIT(K_START)) break;
    }
    pc_ovl_fn = 0; BG_PAL[0] = s0; wait_release();
}

// ---- test aid (BUGSTAR_SG_DEMO=1): on a planet, every frame lines up one of each kind of object in front of the camera, so a picture shows them all up close. Nothing happens without the variable.
static void pc_sg_tick(void){
    static int on = -1; if(on < 0){ const char *e = getenv("BUGSTAR_SG_DEMO"); on = e && e[0] != '0' ? atoi(e) : 0; } if(!on || !in_game || scene != 0) return;
    static const uint8_t ks[] = { O_TREE, O_TREE, O_PLANT, O_FLOWER, O_ROCK, O_HOME, O_MARKET, O_CANTINA, O_OFFICE, O_BASE, O_LOCAL, O_LOCAL, O_CRIT, O_CRIT, O_CRIT, O_BEETLE, O_RUIN, O_SHRINE, O_BEACON, O_FOSSIL, O_STALL, O_TUFT, O_PEBBLE, O_SLUG, O_RACE, O_TREE, O_TREE, O_PLANT, O_CRIT, O_LOCAL, O_HOME, O_ROCK };
    s32 cb = cam_back(), cam_x = ppx - ((hsn * cb) >> 8), cam_y = ppy - ((hcs * cb) >> 8);
    int n = (int)(sizeof ks / sizeof ks[0]); int shift = on > 1 ? (on - 1) * 7 : 0;
    for(int i = 0; i < n && i < NPO; i++){ PObj *o = &po[i]; int row = i / 8, col = i % 8; s32 dz = (s32)((6 + row * 3) << 16), dx = (s32)(((col - 3) * (2 + row)) << 15);
        s32 wx = (s32)(((long long)dx * hcs + (long long)dz * hsn) >> 8), wy = (s32)(((long long)dz * hcs - (long long)dx * hsn) >> 8);
        o->x = cam_x + wx; o->y = cam_y + wy; o->kind = ks[i]; o->var = (uint8_t)(i * 5 + shift); o->alive = 1; o->hp = 3; }
}
