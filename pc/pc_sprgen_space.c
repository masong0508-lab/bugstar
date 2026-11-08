// pc_sprgen_space.c: the sky. Planet discs (12 classes: continents, bands, clouds, ice caps, lava cracks, night-side city lights, atmosphere rim, rings, moons),
// stars by spectral class, nebulae, galaxies of 8 shapes and weather sprites. All made from 3D value noise sampled on a sphere (no seams, no pole pinch), so a planet can spin.
#include "pc_sprgen_i.h"

const char *const sg_planet_class_name[SG_NCLS] = { "EARTHLIKE", "DESERT", "ICE", "LAVA", "GAS GIANT", "OCEAN", "BARREN", "TOXIC", "CITY WORLD", "JUNGLE", "ICE GIANT", "RUSTED" };
const char *const sg_galaxy_name[SG_NGAL] = { "SPIRAL", "BARRED SPIRAL", "ELLIPTICAL", "LENTICULAR", "RING", "IRREGULAR", "EDGE-ON", "STARBURST" };
const char *const sg_weather_name[SG_NWEATHER] = { "CLOUD", "SNOW", "RAIN", "DUST", "EMBERS", "ASH", "LIGHTNING", "AURORA" };

// ---- 3D value noise
static float h3(int x, int y, int z, uint32_t s){ uint32_t h = sg_h2(sg_h2((uint32_t)x * 73856093u ^ s, (uint32_t)y * 19349663u + 7), (uint32_t)z * 83492791u + 3); return (float)(h & 0xFFFF) * (1.0f / 65535.0f); }
static float n3(float x, float y, float z, uint32_t s){
    float fx = floorf(x), fy = floorf(y), fz = floorf(z); int ix = (int)fx, iy = (int)fy, iz = (int)fz; float tx = sg_smooth(x - fx), ty = sg_smooth(y - fy), tz = sg_smooth(z - fz);
    float a = sg_lerp(sg_lerp(h3(ix, iy, iz, s), h3(ix + 1, iy, iz, s), tx), sg_lerp(h3(ix, iy + 1, iz, s), h3(ix + 1, iy + 1, iz, s), tx), ty);
    float b = sg_lerp(sg_lerp(h3(ix, iy, iz + 1, s), h3(ix + 1, iy, iz + 1, s), tx), sg_lerp(h3(ix, iy + 1, iz + 1, s), h3(ix + 1, iy + 1, iz + 1, s), tx), ty); return sg_lerp(a, b, tz); }
static float f3(float x, float y, float z, uint32_t s, int oct){ float v = 0, a = 0.5f, tot = 0; for(int i = 0; i < oct; i++){ v += a * n3(x, y, z, s + i * 131u); tot += a; a *= 0.5f; x *= 2.03f; y *= 2.03f; z *= 2.03f; } return v / tot; }
static float cell3(float x, float y, float z, uint32_t s){ int ix = (int)floorf(x), iy = (int)floorf(y), iz = (int)floorf(z); float best = 9; for(int dz = -1; dz <= 1; dz++) for(int dy = -1; dy <= 1; dy++) for(int dx = -1; dx <= 1; dx++){ float px = ix + dx + h3(ix + dx, iy + dy, iz + dz, s), py = iy + dy + h3(ix + dx, iy + dy, iz + dz, s + 1), pz = iz + dz + h3(ix + dx, iy + dy, iz + dz, s + 2); float d = (px - x) * (px - x) + (py - y) * (py - y) + (pz - z) * (pz - z); if(d < best) best = d; } return sqrtf(best); }

// ------------------------------------------------------------------ the specification
void sg_planet_spec(SgPlanetSpec *p, uint64_t seed, int cls){
    SgRng r = sg_rng(seed, 0x91A7); memset(p, 0, sizeof *p); p->seed = (uint32_t)(sg_m64(seed) >> 8);
    if(cls < 0){ static const uint8_t pick[24] = { 0, 0, 0, 1, 1, 2, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9, 9, 10, 10, 11, 0, 4, 1 }; cls = pick[sg_ri(&r, 24)]; } p->cls = (uint8_t)(cls % SG_NCLS);
    float h = sg_rf(&r), j = sg_rs(&r) * 0.04f; (void)h;
    switch(p->cls){
    case 0: p->c[0] = sg_hsv(0.58f + j, 0.75f, 0.55f); p->c[1] = sg_hsv(0.27f + j * 2, 0.55f, 0.5f); p->c[2] = sg_hsv(0.09f, 0.35f, 0.62f); p->c[3] = 0xEEF4FF; p->clouds = 150 + sg_ri(&r, 80); p->ice = 120; p->atmo = 150; p->lights = sg_ri(&r, 3) ? 0 : 200; break;
    case 1: p->c[0] = sg_hsv(0.07f, 0.55f, 0.45f); p->c[1] = sg_hsv(0.10f + j, 0.5f, 0.82f); p->c[2] = sg_hsv(0.06f, 0.6f, 0.62f); p->c[3] = 0xF8E0B8; p->clouds = 30; p->atmo = 90; p->ice = 20; break;
    case 2: p->c[0] = sg_hsv(0.58f, 0.4f, 0.55f); p->c[1] = sg_hsv(0.56f + j, 0.12f, 0.96f); p->c[2] = sg_hsv(0.6f, 0.45f, 0.78f); p->c[3] = 0xE8F4FF; p->clouds = 60; p->ice = 255; p->atmo = 120; break;
    case 3: p->c[0] = sg_hsv(0.02f, 0.5f, 0.12f); p->c[1] = sg_hsv(0.05f + j, 0.95f, 1.0f); p->c[2] = sg_hsv(0.13f, 0.8f, 1.0f); p->c[3] = 0xFF8040; p->clouds = 20; p->atmo = 140; break;
    case 4: p->c[0] = sg_hsv(sg_rf(&r), 0.45f, 0.78f); p->c[1] = sg_hsv(sg_rf(&r) * 0.2f + 0.04f, 0.55f, 0.9f); p->c[2] = sg_hsv(sg_rf(&r), 0.5f, 0.6f); p->c[3] = sg_hsv(0.1f, 0.3f, 1); p->bands = (uint8_t)(6 + sg_ri(&r, 9)); p->atmo = 110; break;
    case 5: p->c[0] = sg_hsv(0.6f + j, 0.8f, 0.5f); p->c[1] = sg_hsv(0.3f, 0.5f, 0.55f); p->c[2] = sg_hsv(0.12f, 0.3f, 0.75f); p->c[3] = 0xFFFFFF; p->clouds = 190; p->ice = 90; p->atmo = 170; break;
    case 6: p->c[0] = sg_hsv(0.08f, 0.1f, 0.3f); p->c[1] = sg_hsv(0.09f, 0.12f, 0.58f); p->c[2] = sg_hsv(0.1f, 0.08f, 0.78f); p->c[3] = 0xB0B0B8; p->atmo = 0; break;
    case 7: p->c[0] = sg_hsv(0.25f, 0.8f, 0.45f); p->c[1] = sg_hsv(0.16f + j, 0.85f, 0.85f); p->c[2] = sg_hsv(0.78f, 0.55f, 0.6f); p->c[3] = 0xD8F080; p->clouds = 170; p->atmo = 190; break;
    case 8: p->c[0] = sg_hsv(0.6f, 0.15f, 0.3f); p->c[1] = sg_hsv(0.58f, 0.1f, 0.55f); p->c[2] = sg_hsv(0.1f, 0.25f, 0.7f); p->c[3] = 0xC8D8F0; p->clouds = 80; p->lights = 255; p->atmo = 120; break;
    case 9: p->c[0] = sg_hsv(0.52f, 0.7f, 0.4f); p->c[1] = sg_hsv(0.33f + j, 0.7f, 0.38f); p->c[2] = sg_hsv(0.2f, 0.6f, 0.5f); p->c[3] = 0xE8FFE8; p->clouds = 200; p->atmo = 170; p->ice = 40; break;
    case 10: p->c[0] = sg_hsv(0.52f + j, 0.5f, 0.7f); p->c[1] = sg_hsv(0.56f, 0.5f, 0.88f); p->c[2] = sg_hsv(0.6f, 0.55f, 0.62f); p->c[3] = 0xE0F8FF; p->bands = (uint8_t)(4 + sg_ri(&r, 4)); p->atmo = 140; break;
    default: p->c[0] = sg_hsv(0.02f, 0.65f, 0.38f); p->c[1] = sg_hsv(0.045f + j, 0.7f, 0.74f); p->c[2] = sg_hsv(0.08f, 0.55f, 0.55f); p->c[3] = 0xF0B890; p->clouds = 60; p->atmo = 120; break; }
    int rr = (int)sg_ri(&r, 100); int ringchance = p->cls == 4 || p->cls == 10 ? 55 : p->cls == 2 ? 14 : 8; if(rr < ringchance){ p->ring = (uint8_t)(1 + sg_ri(&r, 3)); p->ringtilt = sg_rr(&r, -0.5f, 0.5f); p->ringin = sg_rr(&r, 1.25f, 1.5f); p->ringout = p->ringin + sg_rr(&r, 0.3f, 0.8f); p->ringc[0] = sg_mixc(p->c[1], 0xE8DCC0, 0.5f); p->ringc[1] = sg_mixc(p->c[2], 0x8A7A6A, 0.4f); if(p->ring == 2){ p->ringc[0] = sg_hsv(sg_rf(&r), 0.3f, 0.9f); p->ringc[1] = sg_hsv(sg_rf(&r), 0.3f, 0.6f); } if(p->ring == 3){ p->ringc[0] = 0xE8F4FF; p->ringc[1] = 0x9AB4D0; } }
    p->moons = (uint8_t)(p->cls == 4 || p->cls == 10 ? sg_ri(&r, 5) : sg_ri(&r, 3) == 0 ? 0 : sg_ri(&r, 3));
}
// ------------------------------------------------------------------ the surface (a longitude-latitude map, alpha = light the surface gives itself)
#define TW 256
#define TH 128
static void surf_px(const SgPlanetSpec *p, float x, float y, float z, uint32_t *out, float *cloud){
    uint32_t s = p->seed; float lat = y, h = f3(x * 2.2f + 3, y * 2.2f + 5, z * 2.2f + 9, s, 5), em = 0; uint32_t col; float lit;
    float sea = p->cls == 0 ? 0.52f : p->cls == 5 ? 0.66f : p->cls == 9 ? 0.40f : p->cls == 1 || p->cls == 11 ? 0.0f : -1;
    switch(p->cls){
    case 0: case 5: case 9: {
        if(h < sea){ float d = h / sea; col = sg_mixc(sg_scale(p->c[0], 0.55f), sg_mixc(p->c[0], 0xA8E0F0, 0.28f), d * d); lit = 0; }
        else { float t = (h - sea) / (1 - sea); col = t < 0.08f ? sg_mixc(0xD8C890, p->c[1], t / 0.08f) : sg_mixc(p->c[1], p->c[2], sg_smooth((t - 0.2f) * 1.6f)); float tm = f3(x * 5, y * 5, z * 5, s + 40, 3); if(p->cls == 9) col = sg_scale(col, 0.7f + 0.5f * tm); else col = sg_mixc(col, sg_scale(col, 0.7f), tm * 0.6f); if(t > 0.72f) col = sg_mixc(col, 0xF4F4F8, (t - 0.72f) * 3); lit = 1; }
        if(p->lights && h >= sea) em = f3(x * 14, y * 14, z * 14, s + 90, 2) > 0.62f ? 1.0f : 0; if(p->cls == 0) em *= p->lights / 255.0f; break; }
    case 1: { float d = f3(x * 3.0f + y * 2, y * 7, z * 3.0f, s, 5); col = sg_mixc(p->c[1], p->c[2], d); float k = f3(x * 1.4f, y * 1.4f, z * 1.4f, s + 8, 3); col = sg_mixc(col, p->c[0], sg_smooth((k - 0.55f) * 4)); float rip = f3(x * 18, y * 4, z * 18, s + 12, 2); col = sg_scale(col, 0.88f + 0.24f * rip); lit = 1; break; }
    case 11: { float d = f3(x * 4, y * 4, z * 4, s, 5); col = sg_mixc(p->c[1], p->c[2], d); float c = cell3(x * 4, y * 4, z * 4, s + 4); float c2 = cell3(x * 4 + 0.3f, y * 4, z * 4, s + 4); if(fabsf(c - c2) < 0.012f) col = sg_scale(col, 0.45f); float pit = cell3(x * 11, y * 11, z * 11, s + 8); if(pit < 0.2f) col = sg_mixc(col, p->c[0], 0.6f); if(f3(x * 7, y * 7, z * 7, s + 30, 3) > 0.7f) col = sg_mixc(col, 0xE8A060, 0.5f); lit = 1; break; }
    case 2: { float a = f3(x * 2.5f, y * 2.5f, z * 2.5f, s, 5); float c = fabsf(f3(x * 6, y * 6, z * 6, s + 5, 3) - 0.5f); col = sg_mixc(p->c[1], p->c[2], a); if(c < 0.03f) col = sg_mixc(col, p->c[0], 0.7f); lit = 1; break; }
    case 3: { float c = fabsf(f3(x * 3.2f, y * 3.2f, z * 3.2f, s + 5, 4) - 0.5f); float rock = f3(x * 6, y * 6, z * 6, s + 11, 3); col = sg_mixc(p->c[0], sg_scale(p->c[0], 2.2f), rock); if(c < 0.045f){ float t = 1 - c / 0.045f; col = sg_mixc(col, p->c[1], t); em = t * 0.95f; if(t > 0.65f) col = sg_mixc(col, p->c[2], (t - 0.65f) * 2.5f); } lit = 1; break; }
    case 4: case 10: { int nb = p->bands ? p->bands : 8; float w = f3(x * 2, y * 2.5f, z * 2, s + 3, 4) * 0.22f; float b = (lat + w) * nb * 0.5f; float t = 0.5f + 0.5f * sinf(b * SG_PI * 2); float t2 = f3(x * 1.2f + b * 0.3f, y * 14 * (p->cls == 10 ? 0.5f : 1), z * 1.2f, s + 17, 4);
        col = sg_mixc(sg_mixc(p->c[0], p->c[1], t), p->c[2], sg_smooth((t2 - 0.5f) * (p->cls == 10 ? 1.2f : 2.4f))); if(p->cls == 4){ float sx = x - 0.62f, sy = y + 0.30f, sz = z - 0.72f; float d = sqrtf(sx * sx * 3 + sy * sy * 9 + sz * sz * 3); if(d < 0.55f) col = sg_mixc(col, sg_hsv(0.03f, 0.6f, 0.78f), sg_smooth(1 - d / 0.55f)); } lit = 1; break; }
    case 6: { float c = cell3(x * 5, y * 5, z * 5, s), m = f3(x * 3, y * 3, z * 3, s + 2, 5); col = sg_mixc(p->c[0], p->c[2], m); float rim = c < 0.28f ? (c > 0.2f ? 1.0f : 0.0f) : 0.0f; if(c < 0.2f) col = sg_scale(col, 0.82f); col = sg_mixc(col, p->c[2], rim * 0.55f); float c2 = cell3(x * 13, y * 13, z * 13, s + 6); if(c2 < 0.14f) col = sg_scale(col, 0.85f); lit = 1; break; }
    case 7: { float wx = f3(x * 2, y * 2, z * 2, s, 3) * 1.6f, wy = f3(x * 2 + 5, y * 2, z * 2, s + 1, 3) * 1.6f, wz = f3(x * 2, y * 2 + 7, z * 2, s + 2, 3) * 1.6f; float d = f3(x * 2.5f + wx, y * 2.5f + wy, z * 2.5f + wz, s + 3, 4); col = sg_mixc(sg_mixc(p->c[0], p->c[1], sg_smooth(d * 2.2f - 0.3f)), p->c[2], sg_smooth((d - 0.6f) * 5)); lit = 1; break; }
    case 8: { float c = cell3(x * 9, y * 9, z * 9, s); col = sg_mixc(p->c[1], p->c[0], c < 0.22f ? 1 : 0); float g = f3(x * 3, y * 3, z * 3, s + 7, 3); col = sg_mixc(col, p->c[2], g * 0.4f); float dens = f3(x * 3, y * 3, z * 3, s + 9, 3); em = (dens > 0.4f && c > 0.2f && f3(x * 32, y * 32, z * 32, s + 4, 1) > 0.45f) ? 1.0f : 0; lit = 1; break; }
    default: col = p->c[1]; lit = 1; }
    // polar caps
    if(p->ice && p->cls != 4 && p->cls != 10 && p->cls != 3){ float a = fabsf(lat); float edge = 1.0f - p->ice / 255.0f * 0.38f + (f3(x * 6, y * 6, z * 6, s + 33, 3) - 0.5f) * 0.14f; if(a > edge && (p->cls != 0 || lit || a > edge + 0.04f)){ col = sg_mixc(col, 0xF2F8FF, sg_smooth((a - edge) * 14)); em = 0; } }
    // clouds
    float cl = 0; if(p->clouds){ float c1 = f3(x * 3.2f + 11, y * 5.2f, z * 3.2f + 5, s + 77, 5); float th = 0.62f - p->clouds / 255.0f * 0.2f; cl = sg_smooth((c1 - th) * 5.5f); }
    *cloud = cl; *out = (uint32_t)(sg_clampf(em, 0, 1) * 255.0f + 0.5f) << 24 | (col & 0xFFFFFF);
}
static void tex_make(SgImg *im, void *arg){      // top 64 rows surface, bottom 64 rows: alpha = cloud density
    const SgPlanetSpec *p = (const SgPlanetSpec *)arg;
    for(int v = 0; v < TH; v++) for(int u = 0; u < TW; u++){ float lon = (u + 0.5f) / TW * SG_TAU, lat = ((v + 0.5f) / TH - 0.5f) * SG_PI; float cy = cosf(lat); float x = cosf(lon) * cy, z = sinf(lon) * cy, y = -sinf(lat); uint32_t px; float cl; surf_px(p, x, y, z, &px, &cl); im->px[v * TW + u] = px; im->px[(TH + v) * TW + u] = (uint32_t)(sg_clampf(cl, 0, 1) * 255.0f + 0.5f) << 24 | 0xFFFFFF; }
}
void sg_planet_tex(uint32_t *tex, int tw, int th, const SgPlanetSpec *p){
    for(int v = 0; v < th; v++) for(int u = 0; u < tw; u++){ float lon = (u + 0.5f) / tw * SG_TAU, lat = ((v + 0.5f) / th - 0.5f) * SG_PI; float cy = cosf(lat); uint32_t px; float cl; surf_px(p, cosf(lon) * cy, -sinf(lat), sinf(lon) * cy, &px, &cl); tex[v * tw + u] = px & 0xFFFFFF; } }
static uint32_t spec_key(const SgPlanetSpec *p){ uint32_t h = 0x811C9DC5u; const uint8_t *b = (const uint8_t *)p; (void)b; h = (h ^ p->seed) * 16777619u; h = (h ^ p->cls) * 16777619u; for(int i = 0; i < 4; i++) h = (h ^ p->c[i]) * 16777619u; h = (h ^ p->clouds) * 16777619u; h = (h ^ p->ice) * 16777619u; h = (h ^ p->lights) * 16777619u; h = (h ^ p->bands) * 16777619u; return h; }
static uint32_t tx_sample(const SgImg *t, int row0, float u, float v){      // bilinear, wraps in u
    u = u * TW - 0.5f; v = sg_clampf(v * TH - 0.5f, 0, TH - 1.001f); int u0 = (int)floorf(u), v0 = (int)v; float fu = u - floorf(u), fv = v - v0; int u1 = u0 + 1; u0 = ((u0 % TW) + TW) % TW; u1 = ((u1 % TW) + TW) % TW; uint32_t a = t->px[(row0 + v0) * TW + u0], b = t->px[(row0 + v0) * TW + u1], c = t->px[(row0 + v0 + 1) * TW + u0], d = t->px[(row0 + v0 + 1) * TW + u1];
    uint32_t o = 0; for(int s = 0; s <= 24; s += 8){ float va = (float)(a >> s & 255), vb = (float)(b >> s & 255), vc = (float)(c >> s & 255), vd = (float)(d >> s & 255); float q = sg_lerp(sg_lerp(va, vb, fu), sg_lerp(vc, vd, fu), fv); o |= (uint32_t)(q + 0.5f) << s; } return o; }

// ------------------------------------------------------------------ the disc
float sg_planet_frac(const SgPlanetSpec *p){ float ext = 1.08f; if(p->ring) ext = p->ringout * 1.06f; if(p->moons && ext < 1.9f) ext = 1.9f; return 0.5f / ext; }
void sg_planet(SgImg *im, const SgPlanetSpec *p, float rot, float lx, float ly, float lz){
    int W = im->w, H = im->h; float ln = sqrtf(lx * lx + ly * ly + lz * lz); if(ln < 1e-4f){ lx = -0.6f; ly = 0.4f; lz = 0.7f; ln = 1; } lx /= ln; ly /= ln; lz /= ln;
    float ext = 1.08f; if(p->ring) ext = p->ringout * 1.06f; if(p->moons) if(ext < 1.9f) ext = 1.9f; float cx = W * 0.5f, cy = H * 0.5f, R = (W < H ? W : H) * 0.5f / ext;
    SgPlanetSpec q = *p; const SgImg *tex = sg_cached_ex(0x50000000u ^ spec_key(p), TW, TH * 2, tex_make, &q); if(!tex) return;
    float tilt = p->ringtilt * 0.5f, ct = cosf(tilt), st = sinf(tilt); uint32_t atm = p->c[3]; float rc = cosf(p->ringtilt), rs = sinf(p->ringtilt), kview = 0.3f;
    uint32_t litc[3]; (void)litc;
    // rings and moons are drawn in two passes: behind the disc, then in front
    for(int pass = 0; pass < 2; pass++){
        if(p->ring) for(int y = 0; y < H; y++) for(int x = 0; x < W; x++){
            float dx = (x + 0.5f - cx) / R, dy = (y + 0.5f - cy) / R, u = dx * rc + dy * rs, v = -dx * rs + dy * rc; float rho = sqrtf(u * u + (v / kview) * (v / kview)); if(rho < p->ringin || rho > p->ringout) continue; int front = v > 0; if(front != pass) continue;
            if(!front && dx * dx + dy * dy < 1.0f) continue;
            float t = (rho - p->ringin) / (p->ringout - p->ringin), n = n3(rho * 25.0f, 0.5f, 0.5f, p->seed + 5), gap = (t > 0.55f && t < 0.62f) ? 0.1f : 1.0f; float a = (0.55f + 0.4f * n) * gap; if(p->ring == 1) a *= 0.7f + 0.3f * sinf(t * 40); if(p->ring == 3) a *= 0.6f + 0.4f * n3(rho * 70, 1, 2, p->seed);
            uint32_t col = sg_mixc(p->ringc[0], p->ringc[1], sg_clampf(n * 1.4f - 0.2f + t * 0.4f, 0, 1)); float lit = 0.45f + 0.55f * sg_clampf(0.5f + (u * lx + (v / kview) * ly * 0.3f) * 0.5f, 0, 1);
            // the planet's shadow falls on the ring behind it
            if(!front){ float shx = u + lx * 0.0f; (void)shx; }
            col = sg_scale(col, lit); if(front){ float sd = (u * lx * 1.2f + (v / kview) * -0.0f); (void)sd; }
            float prior = (im->px[y * W + x] >> 24) / 255.0f; float al = sg_clampf(a, 0, 1) * 0.92f; int idx = y * W + x; uint32_t oc = im->px[idx]; float oa = prior; float na = al + oa * (1 - al);
            if(na > 0){ int r_ = (int)(((col >> 16 & 255) * al + (oc >> 16 & 255) * oa * (1 - al)) / na), g_ = (int)(((col >> 8 & 255) * al + (oc >> 8 & 255) * oa * (1 - al)) / na), b_ = (int)(((col & 255) * al + (oc & 255) * oa * (1 - al)) / na); im->px[idx] = (uint32_t)(na * 255.0f + 0.5f) << 24 | sg_rgb(r_, g_, b_); } }
        if(pass == 0){
            for(int y = 0; y < H; y++) for(int x = 0; x < W; x++){
                float dx = (x + 0.5f - cx) / R, dy = (y + 0.5f - cy) / R, r2 = dx * dx + dy * dy; if(r2 > 1.0f + 3.0f / R) continue;
                float rr = sqrtf(r2); float edge = rr <= 1 ? 1.0f : sg_clampf(1 - (rr - 1) * R, 0, 1); float zz = rr < 1 ? sqrtf(1 - r2) : 0; float nx = dx, ny = dy, nz = zz; if(rr >= 1){ nx = dx / rr; ny = dy / rr; nz = 0; }
                float ux = nx * ct - ny * st, uy = nx * st + ny * ct, uz = nz; float lat = uy, lon = atan2f(ux, uz) / SG_TAU + 0.5f + rot; float v = asinf(sg_clampf(-lat, -1, 1)) / SG_PI + 0.5f;
                uint32_t sf = tx_sample(tex, 0, lon - floorf(lon), v), cl = tx_sample(tex, TH, (lon + rot * 0.18f) - floorf(lon + rot * 0.18f), v); float cd = (cl >> 24) / 255.0f; uint32_t col = sf & 0xFFFFFF; float em = (sf >> 24) / 255.0f;
                float dl = nx * lx + ny * ly + nz * lz; float dif = sg_clampf(dl * 1.15f + 0.08f, 0, 1); float day = sg_smooth(dl * 3.0f + 0.5f);
                if(p->cls == 5 || (p->cls == 0 && 0)){ /* ocean glint */ float hx = lx, hy = ly, hz = lz + 1; float hn = sqrtf(hx * hx + hy * hy + hz * hz); float sp = sg_clampf((nx * hx + ny * hy + nz * hz) / hn, 0, 1); sp = powf(sp, 40.0f); col = sg_mixc(col, 0xFFFFFF, sp * 0.6f * (cd < 0.3f ? 1 : 0)); }
                if(cd > 0) col = sg_mixc(col, p->cls == 3 ? sg_scale(p->c[3], 0.5f) : p->c[3], cd * 0.9f); em *= (1 - cd * 0.8f);
                uint32_t night = sg_mixc(sg_scale(col, 0.12f), 0x101830, 0.35f); uint32_t dayc = sg_mixc(sg_scale(col, 0.25f + 0.9f * dif), 0xFFF4E0, sg_clampf((dif - 0.8f) * 0.8f, 0, 0.2f)); uint32_t sh = sg_mixc(night, dayc, day);
                if(em > 0){ uint32_t ec = p->cls == 3 ? sg_mixc(p->c[1], p->c[2], em) : 0xFFD890; float eg = p->cls == 3 ? em : em * (1 - day) * (0.5f + 0.5f * (1 - day)); if(p->cls == 3) sh = sg_mixc(sh, ec, eg * (0.65f + 0.35f * (1 - day))); else sh = sg_mixc(sh, ec, eg); }
                // atmosphere rim
                if(p->atmo){ float rim = powf(1 - nz, 3.0f) * (p->atmo / 255.0f); float lit = sg_clampf(dl * 2 + 0.6f, 0, 1); uint32_t ac = p->cls == 3 ? 0xFF6A30 : p->cls == 1 || p->cls == 11 ? 0xFFB070 : p->cls == 7 ? 0xC8F060 : 0x80B8FF; sh = sg_mixc(sh, ac, sg_clampf(rim * 1.4f * lit, 0, 0.85f)); }
                if(rr < 1){ im->px[y * W + x] = 0xFF000000u | sh; }
                else if(p->atmo){ float glow = (1 - (rr - 1) * R / 3.0f); if(glow > 0){ float lit = sg_clampf(dl * 2 + 0.5f, 0, 1); uint32_t ac = p->cls == 3 ? 0xFF6A30 : p->cls == 1 || p->cls == 11 ? 0xFFB070 : p->cls == 7 ? 0xC8F060 : 0x80B8FF; float a = glow * glow * 0.5f * lit * (p->atmo / 255.0f); uint32_t o = im->px[y * W + x]; if((o >> 24) < (uint32_t)(a * 255)) im->px[y * W + x] = (uint32_t)(a * 255) << 24 | ac; } }
                (void)edge; (void)atm; }
            // moons orbit in the ring plane, some behind the disc, some in front
            for(int m = 0; m < p->moons; m++){ SgRng mr = sg_rng(p->seed + 3, m + 1); float orb = (1.45f + m * 0.16f + sg_rf(&mr) * 0.1f), ph = sg_rf(&mr) * SG_TAU + rot * (4.0f / (m + 2)) * SG_TAU * 0.25f; float mu = cosf(ph) * orb, mv = sinf(ph) * orb * kview; float mx = cx + (mu * rc - mv * rs) * R, my = cy + (mu * rs + mv * rc) * R; float mrad = R * (0.07f + sg_rf(&mr) * 0.1f); int behind = sinf(ph) < 0; if(behind && sqrtf((mx - cx) * (mx - cx) + (my - cy) * (my - cy)) < R + mrad) continue; (void)behind;
                SgRamp mrp; sg_ramp(&mrp, sg_mixc(sg_hsv(sg_rf(&mr), 0.2f, 0.7f), 0x909090, 0.4f)); for(int y = (int)(my - mrad - 1); y <= (int)(my + mrad + 1); y++) for(int x = (int)(mx - mrad - 1); x <= (int)(mx + mrad + 1); x++){ if(x < 0 || y < 0 || x >= W || y >= H) continue; float ddx = (x + 0.5f - mx) / mrad, ddy = (y + 0.5f - my) / mrad, d2 = ddx * ddx + ddy * ddy; if(d2 > 1) continue; float zz = sqrtf(1 - d2); float li = sg_clampf(ddx * lx + ddy * ly + zz * lz, 0, 1); float cr = 1.0f - 0.25f * (n3(ddx * 4, ddy * 4, 1, p->seed + m) > 0.65f); im->px[y * W + x] = 0xFF000000u | sg_rampc(&mrp, (0.1f + 0.9f * li) * cr, 4); } }
        } }
}

// ------------------------------------------------------------------ stars, nebulae, galaxies
void sg_star(SgImg *im, int spectral, uint32_t seed){
    static const uint32_t sc[7] = { 0x9AB8FF, 0xB0C8FF, 0xE0E8FF, 0xFFF8E8, 0xFFE890, 0xFFB868, 0xFF7A48 }; int W = im->w, H = im->h; float cx = W * 0.5f, cy = H * 0.5f, R = W * 0.5f; uint32_t c = sc[(spectral % 7 + 7) % 7]; SgRng r = sg_rng(seed, 0x57A2); int rays = 4 + 2 * sg_ri(&r, 3); float rot = sg_rf(&r) * 3.14f, len = 0.55f + sg_rf(&r) * 0.4f;
    for(int y = 0; y < H; y++) for(int x = 0; x < W; x++){ float dx = (x + 0.5f - cx) / R, dy = (y + 0.5f - cy) / R, d = sqrtf(dx * dx + dy * dy); if(d > 1) continue; float a = atan2f(dy, dx); float ray = 0; for(int i = 0; i < rays; i++){ float an = rot + i * SG_PI / rays * 2; float da = fabsf(sinf(a - an)); ray += expf(-da * da * 400.0f) * (cosf(a - an) > 0 ? 1 : 0) * (1 - d / len) * (d < len ? 1 : 0); }
        float core = expf(-d * d * 90.0f), glow = expf(-d * 6.0f) * 0.45f, al = sg_clampf(core + glow + ray * 0.5f, 0, 1); uint32_t col = sg_mixc(c, 0xFFFFFF, sg_clampf(core * 1.4f, 0, 1)); im->px[y * W + x] = (uint32_t)(al * 255) << 24 | col; }
}
void sg_nebula(SgImg *im, uint32_t seed){
    int W = im->w, H = im->h; SgRng r = sg_rng(seed, 0x4EB0); float h0 = sg_rf(&r), h1 = h0 + 0.18f + sg_rf(&r) * 0.35f, h2 = h0 + 0.5f + sg_rf(&r) * 0.3f; uint32_t c0 = sg_hsv(h0, 0.7f, 0.55f), c1 = sg_hsv(h1, 0.65f, 0.95f), c2 = sg_hsv(h2, 0.4f, 1.0f); float sc = 2.0f + sg_rf(&r) * 2.5f; uint32_t s = (uint32_t)(sg_m64(seed) >> 16);
    for(int y = 0; y < H; y++) for(int x = 0; x < W; x++){ float u = (x + 0.5f) / W, v = (y + 0.5f) / H; float wx = f3(u * sc, v * sc, 1.5f, s, 4) * 1.4f, wy = f3(u * sc + 7, v * sc, 2.5f, s + 1, 4) * 1.4f; float d = f3(u * sc + wx, v * sc + wy, 3.5f, s + 2, 5); float ed = (1 - fabsf(u * 2 - 1)) * (1 - fabsf(v * 2 - 1)); ed = sqrtf(sg_clampf(ed, 0, 1)); float dens = sg_clampf((d - 0.38f) * 2.4f, 0, 1) * ed; float hot = f3(u * sc * 2 + wy, v * sc * 2 + wx, 9.5f, s + 3, 3); uint32_t col = sg_mixc(sg_mixc(c0, c1, sg_clampf(d * 1.6f - 0.3f, 0, 1)), c2, sg_clampf((hot - 0.55f) * 3, 0, 1) * dens); float st = (sg_h2((uint32_t)x * 7 + s, (uint32_t)y * 13) & 1023) < 3 ? 1.0f : 0; im->px[y * W + x] = (uint32_t)(sg_clampf(dens * 0.9f + st, 0, 1) * 255) << 24 | (st > 0 ? 0xFFFFFF : col); }
}
void sg_galaxy(SgImg *im, int type, uint32_t seed){
    int W = im->w, H = im->h; type = (type % SG_NGAL + SG_NGAL) % SG_NGAL; SgRng r = sg_rng(seed, 0x6A1A); float arms = 2 + (float)sg_ri(&r, 3), wind = 2.5f + sg_rf(&r) * 3.0f, tilt = sg_rf(&r) * SG_PI, inc = 0.45f + sg_rf(&r) * 0.5f; uint32_t s = (uint32_t)(sg_m64(seed) >> 16); uint32_t cc = sg_mixc(0xFFE0A0, sg_hsv(sg_rf(&r), 0.3f, 1), 0.3f), ca = sg_mixc(0x80A8FF, sg_hsv(sg_rf(&r), 0.5f, 1), 0.3f);
    if(type == 6) inc = 0.14f; if(type == 2) inc = 0.65f + sg_rf(&r) * 0.35f; float ct = cosf(tilt), sn = sinf(tilt);
    for(int y = 0; y < H; y++) for(int x = 0; x < W; x++){
        float dx = (x + 0.5f - W * 0.5f) / (W * 0.5f), dy = (y + 0.5f - H * 0.5f) / (H * 0.5f); float u = dx * ct + dy * sn, v = (-dx * sn + dy * ct) / inc; float rho = sqrtf(u * u + v * v), th = atan2f(v, u); if(rho > 1.0f && type != 5){ if(sg_h2(x * 3 + s, y * 5) % 997 > 2) continue; }
        float d = 0, bulge = expf(-rho * rho * 22.0f), disc = expf(-rho * 3.0f), warm = bulge; float noise = f3(u * 6, v * 6, 1.5f, s, 4);
        switch(type){
        case 0: case 1: { float a = th - wind * logf(rho + 0.06f); if(type == 1){ float bar = expf(-(v * v) / 0.012f) * (rho < 0.38f ? 1 : 0); d += bar * 0.8f; a = th - wind * logf(rho + 0.3f) + 0.8f; } float arm = 0.5f + 0.5f * cosf(a * arms); d += disc * powf(arm, 2.2f) * (0.5f + noise * 0.9f) + disc * 0.15f; break; }
        case 2: d = expf(-rho * rho * 4.5f) * (0.75f + 0.4f * noise); warm = 1; break;
        case 3: d = disc * 0.55f + bulge * 1.1f; d *= 0.8f + 0.3f * noise; warm = 0.9f; break;
        case 4: { float ring = expf(-powf((rho - 0.62f) * 7.0f, 2)); d = ring * (0.5f + noise * 0.9f) + bulge * 0.9f; break; }
        case 5: { float b1 = expf(-((u - 0.25f) * (u - 0.25f) + (v + 0.1f) * (v + 0.1f)) * 14), b2 = expf(-((u + 0.3f) * (u + 0.3f) + (v - 0.2f) * (v - 0.2f)) * 22), b3 = expf(-((u - 0.0f) * (u - 0.0f) + (v - 0.45f) * (v - 0.45f)) * 30); d = (b1 + b2 * 0.8f + b3 * 0.6f) * (0.4f + 1.2f * noise); warm = 0.15f; break; }
        case 6: { float dust = expf(-v * v * 900.0f) * (rho < 0.9f ? 1 : 0); d = (expf(-fabsf(v) * 60.0f) * expf(-rho * 2.2f) * 0.9f + bulge * 1.1f) * (1 - 0.65f * dust) * (0.8f + 0.4f * noise); break; }
        default: { float a = th - wind * logf(rho + 0.1f); float arm = 0.5f + 0.5f * cosf(a * 2); float knots = f3(u * 14, v * 14, 4.5f, s + 9, 3); d = disc * arm * 0.7f + powf(sg_clampf(knots - 0.52f, 0, 1) * 5, 1.5f) * expf(-rho * 2.2f) + bulge * 0.7f; warm = bulge * 0.4f; ca = 0xA0D0FF; break; } }
        float star = (sg_h2(x * 7 + s, y * 11) % 100) < 6 * d * 12 ? 1.0f : 0; float al = sg_clampf(d * 1.25f + star * 0.35f, 0, 1); uint32_t col = sg_mixc(ca, cc, sg_clampf(warm * 1.5f + (type == 2 ? 0.7f : 0), 0, 1)); col = sg_mixc(col, 0xFFFFFF, sg_clampf(bulge * 0.7f + star * 0.5f, 0, 1)); im->px[y * W + x] = (uint32_t)(al * 255) << 24 | col; }
}

// ------------------------------------------------------------------ weather
void sg_weather(SgImg *im, int kind, uint32_t seed){
    int W = im->w, H = im->h; SgCv cv; cv_init(&cv, im, 64); cv.post = 0; SgRng r = sg_rng(seed, 0x7EA7 + kind); kind = (kind % SG_NWEATHER + SG_NWEATHER) % SG_NWEATHER; (void)W; (void)H;
    switch(kind){
    case 0: { SgRamp c; sg_ramp(&c, 0xF0F4FA); int n = 5 + sg_ri(&r, 4); cv_ell(&cv, 32, 44, 24, 5, 0, &c, SH_VERT); for(int i = 0; i < n; i++){ float x = 12 + i * (40.0f / n) + sg_rr(&r, -2, 2); float rr = 6 + sg_rf(&r) * 8; cv_ell(&cv, x, 40 - rr * 0.6f - sg_rf(&r) * 5, rr, rr * 0.85f, 0, &c, SH_SPHERE); } break; }
    case 1: { int arms = 6; float len = 22 + sg_rf(&r) * 6; for(int i = 0; i < arms; i++){ float a = i * SG_TAU / arms; float ca = cosf(a), sa = sinf(a); cv_flat(&cv, 32, 32, 32 + ca * len, 32 + sa * len, 1.0f, 0xE8F4FF, 0.95f); for(int b = 1; b <= 2; b++){ float t = len * (0.35f + 0.28f * b), bl = 6 - b * 1.4f; cv_flat(&cv, 32 + ca * t, 32 + sa * t, 32 + ca * t + cosf(a + 0.9f) * bl, 32 + sa * t + sinf(a + 0.9f) * bl, 0.7f, 0xE8F4FF, 0.9f); cv_flat(&cv, 32 + ca * t, 32 + sa * t, 32 + ca * t + cosf(a - 0.9f) * bl, 32 + sa * t + sinf(a - 0.9f) * bl, 0.7f, 0xE8F4FF, 0.9f); } cv_dot(&cv, 32 + ca * len, 32 + sa * len, 1.4f, 0xFFFFFF, 1); } cv_dot(&cv, 32, 32, 3, 0xFFFFFF, 1); break; }
    case 2: for(int i = 0; i < 22; i++){ float x = sg_rf(&r) * 70 - 3, y = sg_rf(&r) * 64, l = 6 + sg_rf(&r) * 7; cv_flat(&cv, x, y, x - l * 0.25f, y + l, 0.5f, 0xA8C8F0, 0.55f + sg_rf(&r) * 0.35f); } break;
    case 3: for(int i = 0; i < 14; i++){ float x = sg_rf(&r) * 64, y = sg_rf(&r) * 64, s = 3 + sg_rf(&r) * 7; cv_glow(&cv, x, y, s, 0xC8A878, 0.35f); } for(int i = 0; i < 40; i++) cv_dot(&cv, sg_rf(&r) * 64, sg_rf(&r) * 64, 0.5f, 0xE0C898, 0.7f); break;
    case 4: for(int i = 0; i < 26; i++){ float x = sg_rf(&r) * 64, y = sg_rf(&r) * 64, s = 0.5f + sg_rf(&r) * 1.0f; cv_glow(&cv, x, y, s * 3.2f, 0xFF6A20, 0.5f); cv_dot(&cv, x, y, s, i & 1 ? 0xFFD060 : 0xFF9030, 1); } break;
    case 5: for(int i = 0; i < 24; i++){ float x = sg_rf(&r) * 64, y = sg_rf(&r) * 64, s = 0.9f + sg_rf(&r) * 1.6f, a = sg_rf(&r) * 3; cv_ell(&cv, x, y, s, s * 0.55f, a, &(SgRamp){ { 0x303034, 0x484850, 0x68686E, 0x909098, 0xB8B8C0 } }, SH_SPHERE); } break;
    case 6: { float x = 36, y = 2; for(int i = 0; i < 7; i++){ float nx = x + sg_rr(&r, -8, 8) + (i & 1 ? -5 : 5), ny = y + 9; cv_flat(&cv, x, y, nx, ny, 1.8f, 0xB0C0FF, 0.5f); cv_flat(&cv, x, y, nx, ny, 0.8f, 0xFFFFFF, 1); if(i == 3){ cv_flat(&cv, nx, ny, nx + 12, ny + 12, 0.5f, 0xFFFFFF, 0.9f); } x = nx; y = ny; } cv_glow(&cv, x, y, 8, 0xC8D8FF, 0.5f); break; }
    default: for(int i = 0; i < 56; i++){ float x = 2 + i * 1.0f, h0 = 8 + 18 * f3(i * 0.17f, 0.5f, seed * 0.01f, (uint32_t)seed, 3), y0 = 16 + 10 * sinf(i * 0.12f + sg_rf(&r) * 0.0f); (void)h0; float hh = 14 + 14 * f3(i * 0.13f, 1.5f, 2.5f, (uint32_t)seed + 3, 3); for(int j = 0; j < (int)hh; j++){ float t = j / hh; uint32_t col = sg_mixc(sg_mixc(0x40FF90, 0x30C0FF, t), 0xB050FF, t * t); cv_dot(&cv, x, y0 + j, 0.9f, col, (1 - t) * 0.55f); } } break; }
}
