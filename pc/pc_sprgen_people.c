// pc_sprgen_people.c: inhabitants. A people is a STOCK (32 body plans: height, build, head, eyes, ears, skin surface, arms, legs, horns, shell, tail) times a WAY OF LIFE
// (16 outfits and tools: hats, aprons, packs, staffs, shields, pickaxes ...); the colours of cloth and trim come from the seed. 32 x 16 = 512 peoples, none alike.
#include "pc_sprgen_i.h"
#include <stdio.h>

enum { X_HORN = 1, X_ANT = 2, X_CREST = 4, X_TAIL = 8, X_WING = 16, X_SHELL = 32, X_HANDS = 64, X_BEARD = 128, X_JEWEL = 256, X_MASK = 512, X_CROWD = 1024, X_FEET = 2048, X_BEAK = 4096 };
typedef struct { uint8_t h, w10, head, eyes, ear, feat, arms, leg; uint16_t ex; uint32_t skin, skin2; } Stock;
static const Stock stk[32] = {
 {30,10,0,2,0, 1,2,4, 0,                       0x6CAE48,0x3E7A2E}, {58, 9,1,2,1, 2,2,0, X_CREST,                0xC9A878,0x6A4A2A}, {48,10,0,2,4, 0,2,0, X_HANDS,                0x5A8EC8,0x2E5A94}, {50,13,5,2,0, 6,2,0, 0,                       0x3A3436,0xE04A20},
 {50,14,4,2,0, 7,2,0, X_BEARD,                 0x5E8A44,0x2E5A2A}, {62, 8,4,2,0, 0,2,1, X_BEAK,                  0x9AB0C8,0x5A7A98}, {50,10,0,2,0, 5,2,0, 0,                       0xA8E0F0,0xFFFFFF}, {52, 9,0,2,0, 0,2,0, X_FEET,                  0xD2A86A,0x8A6030},
 {36,10,3,2,3, 0,2,0, 0,                       0xD8D0E8,0x8A80A8}, {46, 8,4,2,0,10,2,0, X_WING | X_CREST | X_BEAK,0xB8C0D8,0x4A5A9A}, {54, 9,0,2,4, 0,2,3, 0,                       0x6CA8B8,0x305A78}, {38,15,5,2,3, 0,2,4, X_HANDS | X_BEARD,        0x8A7058,0x4A3A28},
 {50, 9,0,2,0,12,2,0, X_ANT,                   0xF4E8A0,0xFFB030}, {34,10,0,2,1,13,2,4, 0,                       0xA8583C,0x5A2A1A}, {44,14,4,2,3,14,2,0, 0,                       0xE6F0F8,0x9AB4D0}, {48,12,0,2,0, 0,2,0, X_SHELL | X_ANT,         0x4A5A3C,0x2A3A22},
 {54, 8,1,3,0, 0,6,0, 0,                       0xB4A8C8,0x6A5A8A}, {54,13,5,2,0, 6,2,0, X_BEARD,                 0x8E8A86,0x5A5650}, {56,10,1,2,1,11,2,2, X_TAIL | X_HORN,         0xB8865A,0x5A3A1A}, {44, 9,2,2,4, 0,2,0, 0,                       0x58A890,0x2A6A58},
 {50,10,2,2,0, 3,2,0, X_TAIL | X_CREST,        0xC88A3A,0x5A7A2A}, {56, 9,1,2,2, 0,2,0, X_HORN,                  0xE0DCEE,0x8A7AA8}, {50,10,0,2,1,11,2,0, X_TAIL | X_JEWEL,        0xC86A3C,0xF0C050}, {28,15,3,2,0, 6,2,5, 0,                       0x8C7C6C,0x5A4A3A},
 {60, 7,1,2,0, 1,2,0, 0,                       0x7A6A48,0x4A8A3A}, {40,13,2,2,0, 0,2,0, X_SHELL | X_HANDS,        0xC8B070,0x7A6030}, {50,10,0,2,1,13,2,0, X_HANDS,                 0x7A6038,0x3A5A2A}, {58, 8,4,2,0, 3,2,1, X_FEET,                  0x9CB4B8,0x4A6A7A},
 {54, 7,3,2,3, 0,2,0, 0,                       0x4A4A7A,0xFFE890}, {24,10,0,2,0, 0,2,0, X_CROWD,                 0xE8C050,0x8A6A20}, {56,13,6,1,0, 9,2,0, X_ANT,                   0x8A5A3A,0x50E0FF}, {52,10,0,0,0, 0,2,0, X_MASK,                  0xB89C84,0xE8E0D0} };
static const char *const way_nm[16] = { "REED", "STAR", "GLASS", "HERD", "TIDE", "ROAD", "SHIELD", "STORY", "DELVE", "FIELD", "HERB", "TOWER", "SAIL", "TRAIL", "CHOIR", "WANDER" };
int sg_people_stock_features(int stock){ const Stock *s = &stk[stock & 31]; return s->h * 7 + s->w10 * 13 + s->head * 31 + s->eyes * 53 + s->ear * 97 + s->feat * 193 + s->arms * 389 + s->leg * 769 + s->ex * 5 + (int)(s->skin & 0xFFFF); }

typedef struct { SgCv cv; SgRng r; const Stock *s; int way; SgRamp skin, skin2, cloth, cloth2, trim, dark, leather, metal; float cx, feet, H, hx, hy, hr, shy, hipy, w; uint32_t glow; } Pp;
#define CV (&p->cv)
static void limb(Pp *p, float x0, float y0, float r0, float x1, float y1, float r1, const SgRamp *rp){ cv_cap(CV, x0, y0, r0, x1, y1, r1, rp, SH_SPHERE); }
static void head(Pp *p){
    const Stock *s = p->s; float x = p->hx, y = p->hy, r = p->hr; SgCv *cv = CV; const SgRamp *sk = &p->skin;
    // ears
    float ey = y - r * 0.1f;
    switch(s->ear){ case 1: cv_tri(cv, x - r * 0.8f, ey, x - r * 1.9f, ey - r * 0.9f, x - r * 0.7f, ey - r * 0.5f, sk, 0.7f, 0.4f); cv_tri(cv, x + r * 0.8f, ey, x + r * 1.9f, ey - r * 0.9f, x + r * 0.7f, ey - r * 0.5f, sk, 0.7f, 0.4f); break;
        case 2: cv_cap(cv, x - r * 0.85f, ey, r * 0.28f, x - r * 1.7f, ey + r * 1.4f, r * 0.2f, sk, SH_SPHERE); cv_cap(cv, x + r * 0.85f, ey, r * 0.28f, x + r * 1.7f, ey + r * 1.4f, r * 0.2f, sk, SH_SPHERE); break;
        case 3: cv_ell(cv, x - r * 1.0f, ey, r * 0.55f, r * 0.7f, 0, sk, SH_SPHERE); cv_ell(cv, x + r * 1.0f, ey, r * 0.55f, r * 0.7f, 0, sk, SH_SPHERE); break;
        case 4: for(int i = 0; i < 3; i++){ float a = -0.5f + i * 0.5f; cv_tri(cv, x - r * 0.8f, ey + i * r * 0.15f, x - r * (1.6f + i * 0.1f), ey + (i - 1) * r * 0.55f, x - r * 0.8f, ey + r * 0.3f + i * r * 0.15f, &p->skin2, 0.8f, 0.4f); cv_tri(cv, x + r * 0.8f, ey + i * r * 0.15f, x + r * (1.6f + i * 0.1f), ey + (i - 1) * r * 0.55f, x + r * 0.8f, ey + r * 0.3f + i * r * 0.15f, &p->skin2, 0.8f, 0.4f); (void)a; } break; default: break; }
    // surface behind the head: mane / leaf hair / quills / crest / horns / antennae
    if(s->feat == 11){ cv_ell(cv, x, y + r * 0.2f, r * 1.55f, r * 1.5f, 0, &p->skin2, SH_SPHERE); }
    if(s->feat == 2) for(int i = -3; i <= 3; i++){ float a = -SG_PI / 2 + i * 0.32f; cv_tri(cv, x + cosf(a - 0.12f) * r * 0.8f, y + sinf(a - 0.12f) * r * 0.8f, x + cosf(a + 0.12f) * r * 0.8f, y + sinf(a + 0.12f) * r * 0.8f, x + cosf(a) * r * 1.9f, y + sinf(a) * r * 1.9f, &p->skin2, 0.8f, 0.3f); }
    if(s->ex & X_HORN){ cv_curve(cv, x - r * 0.5f, y - r * 0.8f, x - r * 1.5f, y - r * 1.2f, x - r * 1.1f, y - r * 2.2f, r * 0.28f, r * 0.05f, &p->trim, SH_SPHERE); cv_curve(cv, x + r * 0.5f, y - r * 0.8f, x + r * 1.5f, y - r * 1.2f, x + r * 1.1f, y - r * 2.2f, r * 0.28f, r * 0.05f, &p->trim, SH_SPHERE); }
    if(s->ex & X_ANT){ for(int sd = -1; sd <= 1; sd += 2){ cv_curve(cv, x + sd * r * 0.4f, y - r * 0.85f, x + sd * r * 1.0f, y - r * 1.8f, x + sd * r * 1.5f, y - r * 2.1f, r * 0.11f, r * 0.08f, &p->dark, SH_FLAT); if(s->feat == 12 || s->feat == 9){ cv_glow(cv, x + sd * r * 1.5f, y - r * 2.1f, r * 0.8f, p->glow, 0.7f); cv_dot(cv, x + sd * r * 1.5f, y - r * 2.1f, r * 0.26f, 0xFFFFFF, 1); } else cv_dot(cv, x + sd * r * 1.5f, y - r * 2.1f, r * 0.2f, p->glow, 1); } }
    // the head itself
    switch(s->head){ case 1: cv_ell(cv, x, y, r * 0.85f, r * 1.25f, 0, sk, SH_SPHERE); break; case 2: cv_ell(cv, x, y + r * 0.1f, r * 1.3f, r * 0.85f, 0, sk, SH_SPHERE); break; case 3: cv_ell(cv, x, y - r * 0.1f, r * 1.25f, r * 1.2f, 0, sk, SH_SPHERE); break; case 4: cv_ell(cv, x, y, r * 0.82f, r * 0.82f, 0, sk, SH_SPHERE); break;
        case 5: { float xy[12]; for(int i = 0; i < 6; i++){ float a = i * SG_PI / 3 + 0.3f; xy[i * 2] = x + cosf(a) * r * 1.05f; xy[i * 2 + 1] = y + sinf(a) * r * 1.0f; } cv_poly(cv, xy, 6, sk, 0.85f, 0.3f); break; } case 6: cv_rrect(cv, x - r, y - r * 0.9f, x + r, y + r * 0.95f, r * 0.2f, sk, SH_SPHERE); break; default: cv_ell(cv, x, y, r, r, 0, sk, SH_SPHERE); }
    // hair / leaves / moss / feathers on top
    if(s->feat == 1){ for(int i = -3; i <= 3; i++){ float a = -SG_PI / 2 + i * 0.36f; cv_ell(cv, x + cosf(a) * r * 1.15f, y + sinf(a) * r * 1.15f - r * 0.1f, r * 0.62f, r * 0.26f, a, i & 1 ? &p->skin2 : &p->skin, SH_SPHERE); } }
    if(s->feat == 7) for(int i = -2; i <= 2; i++) cv_ell(cv, x + i * r * 0.45f, y - r * 0.85f, r * 0.45f, r * 0.34f, 0, &p->skin2, SH_SPHERE);
    if(s->ex & X_CREST) for(int i = -2; i <= 2; i++){ float a = -SG_PI / 2 + i * 0.32f; cv_tri(cv, x + cosf(a - 0.15f) * r * 0.7f, y + sinf(a - 0.15f) * r * 0.9f - r * 0.1f, x + cosf(a + 0.15f) * r * 0.7f, y + sinf(a + 0.15f) * r * 0.9f - r * 0.1f, x + cosf(a) * r * 1.7f, y + sinf(a) * r * 1.8f - r * 0.1f, i & 1 ? &p->trim : &p->skin2, 0.8f, 0.4f); }
    if(s->feat == 13) for(int i = -2; i <= 2; i++) cv_tri(cv, x + i * r * 0.4f - r * 0.1f, y - r * 0.9f, x + i * r * 0.4f + r * 0.1f, y - r * 0.9f, x + i * r * 0.4f, y - r * 1.5f, &p->skin2, 0.5f, 0.3f);
    if(s->feat == 14) for(int i = -2; i <= 2; i++) cv_ell(cv, x + i * r * 0.42f, y - r * 0.85f, r * 0.34f, r * 0.3f, 0, &p->skin, SH_SPHERE);
    if(s->feat == 3) cv_pattern(cv, 5, p->skin2.c[1], r * 0.4f, 0.45f, 3, x - r * 1.5f, y - r * 1.5f, x + r * 1.5f, y + r * 1.4f);
    if(s->feat == 4 || s->feat == 11) cv_pattern(cv, 3, p->skin2.c[1], r * 0.3f, 0.5f, 5, x - r, y - r, x + r, y + r);
    if(s->feat == 6) { for(int i = 0; i < 3; i++){ float a = i * 2.1f + 0.4f; cv_flat(cv, x + cosf(a) * r * 0.2f, y + sinf(a) * r * 0.2f, x + cosf(a) * r * 0.85f, y + sinf(a) * r * 0.8f + (i & 1) * 0.6f, r * 0.08f, s->skin2 == 0xE04A20 ? 0xE04A20 : 0x4A4642, 0.9f); } }
    if(s->feat == 12) for(int i = 0; i < 4; i++) cv_dot(cv, x + sinf(i * 2.3f) * r * 0.65f, y + cosf(i * 3.1f) * r * 0.55f, r * 0.1f, p->glow, 0.9f);
    // face
    float ex = r * (s->head == 2 ? 0.55f : 0.42f), eyy = y - r * 0.08f, er = r * (s->head == 3 ? 0.36f : 0.2f); if(s->head == 2) er = r * 0.27f;
    if(s->eyes == 0){ /* masked: a pale mask with two dark slits */ cv_ell(cv, x, y, r * 0.95f, r * 1.05f, 0, &p->metal, SH_SPHERE); cv_flat(cv, x - ex, eyy, x - ex * 0.3f, eyy + r * 0.1f, r * 0.12f, 0x101010, 1); cv_flat(cv, x + ex, eyy, x + ex * 0.3f, eyy + r * 0.1f, r * 0.12f, 0x101010, 1); cv_flat(cv, x - r * 0.3f, y + r * 0.45f, x + r * 0.3f, y + r * 0.45f, r * 0.08f, p->cloth.c[1], 1); }
    else if(s->eyes == 1){ cv_rect(cv, x - r * 0.8f, eyy - r * 0.16f, x + r * 0.8f, eyy + r * 0.16f, &p->dark, 0.5f, 0.3f); cv_rect(cv, x - r * 0.65f, eyy - r * 0.08f, x + r * 0.65f, eyy + r * 0.08f, &p->metal, 0.5f, 0.5f); cv_glow(cv, x, eyy, r * 0.9f, p->glow, 0.5f); cv_rect(cv, x - r * 0.55f, eyy - r * 0.07f, x + r * 0.55f, eyy + r * 0.07f, &p->metal, 0.95f, 0.95f); }
    else { int n = s->eyes; for(int i = 0; i < n; i++){ float t = n == 1 ? 0 : (i / (float)(n - 1)) * 2 - 1; float ex_ = x + t * ex * (n == 3 ? 1.2f : 1.0f), ey_ = eyy + (n == 3 && i == 1 ? -r * 0.32f : 0); if(n == 3 && i == 1) ex_ = x;
            cv_ell(cv, ex_, ey_, er * 1.2f, er * 1.3f, 0, &p->metal, SH_FLATLIT); cv_dot(cv, ex_ + er * 0.1f, ey_ + er * 0.1f, er * 0.62f, s->feat == 6 && s->skin2 == 0xE04A20 ? 0xFF4010 : s->feat == 12 ? 0xE08010 : 0x181818, 1); cv_dot(cv, ex_ - er * 0.15f, ey_ - er * 0.2f, er * 0.2f, 0xFFFFFF, 1); }
        if(s->head != 3) cv_flat(cv, x - r * 0.28f, y + r * 0.5f, x + r * 0.28f, y + r * 0.5f + (s->head == 2 ? 0.0f : 0.2f), r * 0.09f, 0x301818, 0.9f); else cv_dot(cv, x, y + r * 0.58f, r * 0.12f, 0x301818, 0.9f); if(s->head == 2) cv_flat(cv, x - r * 0.7f, y + r * 0.45f, x + r * 0.7f, y + r * 0.5f, r * 0.09f, 0x301818, 0.9f); }
    if(s->ex & X_BEAK) cv_tri(cv, x - r * 0.2f, y + r * 0.15f, x + r * 0.2f, y + r * 0.15f, x, y + r * 0.7f, &p->trim, 0.9f, 0.5f);
    if(s->ex & X_BEARD){ float xy[10] = { x - r * 0.8f, y + r * 0.2f, x + r * 0.8f, y + r * 0.2f, x + r * 0.55f, y + r * 1.15f, x, y + r * 1.6f, x - r * 0.55f, y + r * 1.15f }; cv_poly(cv, xy, 5, s->feat == 7 ? &p->skin2 : &p->dark, 0.8f, 0.3f); cv_flat(cv, x - r * 0.25f, y + r * 0.5f, x + r * 0.25f, y + r * 0.5f, r * 0.09f, 0x301818, 0.9f); }
}
static void hat(Pp *p){
    float x = p->hx, y = p->hy, r = p->hr; SgCv *cv = CV; const Stock *s = p->s; (void)s;
    switch(p->way){
    case 0: { float xy[6] = { x - r * 2.3f, y - r * 0.5f, x, y - r * 2.2f, x + r * 2.3f, y - r * 0.5f }; cv_poly(cv, xy, 3, &p->trim, 0.8f, 0.35f); cv_flat(cv, x - r * 1.4f, y - r * 1.1f, x + r * 0.2f, y - r * 1.9f, r * 0.08f, p->trim.c[0], 0.7f); cv_flat(cv, x - r * 0.4f, y - r * 0.8f, x + r * 1.4f, y - r * 1.1f, r * 0.08f, p->trim.c[0], 0.7f); break; }
    case 1: { float xy[6] = { x - r * 1.1f, y - r * 0.6f, x + r * 0.2f, y - r * 3.0f, x + r * 1.1f, y - r * 0.6f }; cv_poly(cv, xy, 3, &p->cloth, 0.8f, 0.3f); cv_ell(cv, x, y - r * 0.6f, r * 1.7f, r * 0.3f, 0, &p->cloth, SH_FLATLIT); cv_dot(cv, x + r * 0.05f, y - r * 1.5f, r * 0.2f, 0xFFE890, 1); cv_dot(cv, x - r * 0.3f, y - r * 1.0f, r * 0.12f, 0xFFE890, 1); break; }
    case 2: cv_rect(cv, x - r * 1.0f, y - r * 0.55f, x + r * 1.0f, y - r * 0.2f, &p->leather, 0.7f, 0.4f); cv_ell(cv, x - r * 0.45f, y - r * 0.42f, r * 0.4f, r * 0.34f, 0, &p->metal, SH_SPHERE); cv_ell(cv, x + r * 0.45f, y - r * 0.42f, r * 0.4f, r * 0.34f, 0, &p->metal, SH_SPHERE); break;
    case 3: cv_ell(cv, x, y - r * 0.55f, r * 2.1f, r * 0.45f, 0, &p->leather, SH_SPHERE); cv_ell(cv, x, y - r * 0.9f, r * 1.0f, r * 0.75f, 0, &p->leather, SH_SPHERE); cv_rect(cv, x - r * 1.0f, y - r * 0.8f, x + r * 1.0f, y - r * 0.62f, &p->trim, 0.7f, 0.4f); break;
    case 4: cv_ell(cv, x, y - r * 0.75f, r * 1.15f, r * 0.95f, 0, &p->trim, SH_SPHERE); cv_tri(cv, x - r * 1.0f, y - r * 0.4f, x - r * 1.9f, y + r * 0.3f, x - r * 0.4f, y - r * 0.5f, &p->trim, 0.8f, 0.4f); cv_ell(cv, x, y - r * 0.45f, r * 1.5f, r * 0.26f, 0, &p->trim, SH_FLATLIT); break;
    case 5: cv_ell(cv, x, y - r * 0.55f, r * 1.7f, r * 0.34f, 0, &p->leather, SH_SPHERE); cv_ell(cv, x, y - r * 0.95f, r * 0.95f, r * 0.7f, 0, &p->leather, SH_SPHERE); cv_dot(cv, x + r * 0.9f, y - r * 1.3f, r * 0.2f, p->glow, 1); cv_flat(cv, x + r * 0.9f, y - r * 1.3f, x + r * 1.1f, y - r * 2.0f, r * 0.12f, p->glow, 1); break;
    case 6: cv_ell(cv, x, y - r * 0.5f, r * 1.1f, r * 0.8f, 0, &p->metal, SH_SPHERE); cv_rect(cv, x - r * 1.1f, y - r * 0.4f, x + r * 1.1f, y - r * 0.15f, &p->metal, 0.8f, 0.4f); cv_rect(cv, x - r * 0.12f, y - r * 0.4f, x + r * 0.12f, y + r * 0.35f, &p->metal, 0.7f, 0.4f); cv_curve(cv, x, y - r * 1.2f, x + r * 1.0f, y - r * 2.1f, x + r * 1.8f, y - r * 1.0f, r * 0.28f, r * 0.05f, &p->trim, SH_SPHERE); break;
    case 7: cv_ell(cv, x - r * 0.1f, y - r * 0.7f, r * 1.2f, r * 0.5f, -0.15f, &p->cloth, SH_SPHERE); cv_curve(cv, x + r * 0.5f, y - r * 1.0f, x + r * 1.9f, y - r * 2.4f, x + r * 1.6f, y - r * 0.6f, r * 0.14f, r * 0.04f, &p->trim, SH_SPHERE); break;
    case 8: cv_ell(cv, x, y - r * 0.6f, r * 1.1f, r * 0.8f, 0, &p->metal, SH_SPHERE); cv_ell(cv, x, y - r * 0.4f, r * 1.3f, r * 0.2f, 0, &p->metal, SH_FLATLIT); cv_glow(cv, x, y - r * 0.9f, r * 1.2f, 0xFFE070, 0.55f); cv_ell(cv, x, y - r * 0.95f, r * 0.34f, r * 0.34f, 0, &p->trim, SH_SPHERE); cv_dot(cv, x, y - r * 0.95f, r * 0.2f, 0xFFF4B0, 1); break;
    case 9: cv_ell(cv, x, y - r * 0.6f, r * 2.2f, r * 0.42f, 0, &p->trim, SH_SPHERE); cv_ell(cv, x, y - r * 0.95f, r * 0.95f, r * 0.7f, 0, &p->trim, SH_SPHERE); cv_flat(cv, x - r * 0.9f, y - r * 0.9f, x + r * 0.9f, y - r * 0.9f, r * 0.14f, p->cloth.c[2], 0.9f); break;
    case 10: cv_ell(cv, x, y - r * 0.05f, r * 1.4f, r * 1.4f, 0, &p->cloth, SH_SPHERE); cv_ell(cv, x, y + r * 0.05f, r * 0.95f, r * 1.05f, 0, &p->skin, SH_SPHERE); cv_tri(cv, x - r * 1.3f, y - r * 0.3f, x - r * 0.4f, y - r * 1.7f, x + r * 0.4f, y - r * 1.45f, &p->cloth, 0.85f, 0.4f); break;
    case 11: cv_ell(cv, x, y - r * 0.7f, r * 1.1f, r * 0.8f, 0, &p->trim, SH_SPHERE); cv_ell(cv, x, y - r * 0.4f, r * 1.4f, r * 0.2f, 0, &p->trim, SH_FLATLIT); cv_rect(cv, x - r * 0.12f, y - r * 1.5f, x + r * 0.12f, y - r * 0.6f, &p->trim, 0.9f, 0.5f); break;
    case 12: cv_ell(cv, x, y - r * 0.55f, r * 1.1f, r * 0.7f, 0, &p->cloth, SH_SPHERE); for(int i = -2; i <= 2; i++) cv_flat(cv, x + i * r * 0.4f, y - r * 1.2f, x + i * r * 0.4f, y - r * 0.2f, r * 0.14f, p->trim.c[3], 0.8f); cv_tri(cv, x + r * 0.6f, y - r * 0.4f, x + r * 1.7f, y - r * 0.1f, x + r * 0.9f, y + r * 0.3f, &p->cloth, 0.8f, 0.4f); break;
    case 13: { float xy[8] = { x - r * 1.35f, y + r * 1.2f, x - r * 1.0f, y - r * 1.3f, x + r * 1.0f, y - r * 1.3f, x + r * 1.35f, y + r * 1.2f }; cv_poly(cv, xy, 4, &p->cloth, 0.7f, 0.3f); cv_ell(cv, x, y + r * 0.1f, r * 0.9f, r * 0.95f, 0, &p->dark, SH_SPHERE); break; }
    case 14: { float xy[8] = { x - r * 1.3f, y + r * 1.4f, x - r * 1.0f, y - r * 1.2f, x + r * 1.0f, y - r * 1.2f, x + r * 1.3f, y + r * 1.4f }; cv_poly(cv, xy, 4, &p->cloth, 0.75f, 0.3f); cv_ell(cv, x, y + r * 0.15f, r * 0.82f, r * 0.9f, 0, &p->skin, SH_SPHERE); cv_ell(cv, x, y + r * 0.55f, r * 0.26f, r * 0.2f, 0, &p->dark, SH_FLAT); break; }
    default: break; }
}
static void held(Pp *p, float hx, float hy){            // the tool in the hand at (hx, hy)
    SgCv *cv = CV; float r = p->hr;
    switch(p->way){
    case 0: for(int i = -2; i <= 2; i++) cv_cap(cv, hx + i * 0.8f, hy + 8, 0.5f, hx + i * 1.5f, hy - 12, 0.35f, &p->trim, SH_FLAT); cv_flat(cv, hx - 2, hy - 2, hx + 2, hy - 2, 0.7f, p->cloth.c[1], 1); break;
    case 1: cv_cap(cv, hx, hy + 10, 0.5f, hx + 1, hy - 14, 0.5f, &p->leather, SH_SPHERE); { float xy[20]; for(int i = 0; i < 10; i++){ float a = -SG_PI / 2 + i * SG_TAU / 10, rr = (i & 1) ? 1.3f : 3.0f; xy[i * 2] = hx + 1 + cosf(a) * rr; xy[i * 2 + 1] = hy - 15 + sinf(a) * rr; } cv_poly(cv, xy, 10, &p->trim, 0.9f, 0.4f); } break;
    case 2: cv_cap(cv, hx, hy, 0.5f, hx + 9, hy - 6, 0.5f, &p->metal, SH_FLAT); cv_glow(cv, hx + 10, hy - 6.5f, 5, 0xFF8030, 0.8f); cv_ell(cv, hx + 10, hy - 6.5f, 2.4f, 2.6f, 0, &p->trim, SH_SPHERE); cv_dot(cv, hx + 9.5f, hy - 7.2f, 0.9f, 0xFFF0B0, 1); break;
    case 3: cv_curve(cv, hx, hy + 12, hx - 0.5f, hy - 14, hx + 4, hy - 17, 0.6f, 0.5f, &p->leather, SH_SPHERE); break;
    case 4: cv_cap(cv, hx, hy, 0.4f, hx + 8, hy - 16, 0.3f, &p->leather, SH_FLAT); cv_flat(cv, hx + 8, hy - 16, hx + 8, hy - 4, 0.2f, 0xE0E0E0, 0.9f); cv_ell(cv, hx + 8, hy - 2, 2.6f, 1.3f, 0, &p->metal, SH_SPHERE); cv_tri(cv, hx + 10, hy - 2, hx + 12.5f, hy - 4, hx + 12.5f, hy, &p->metal, 0.7f, 0.4f); break;
    case 5: cv_ell(cv, hx, hy + 2, 2.2f, 2.8f, 0, &p->leather, SH_SPHERE); cv_dot(cv, hx, hy + 2, 0.8f, 0xF0D060, 1); break;
    case 6: cv_ell(cv, hx - 3, hy + 2, 8, 9, 0, &p->metal, SH_SPHERE); cv_ell(cv, hx - 3, hy + 2, 5, 5.6f, 0, &p->trim, SH_SPHERE); cv_dot(cv, hx - 3, hy + 2, 1.4f, 0xF0D060, 1); cv_cap(cv, hx + 4, hy + 13, 0.5f, hx + 4, hy - 20, 0.4f, &p->leather, SH_SPHERE); cv_tri(cv, hx + 3.2f, hy - 19, hx + 4.8f, hy - 19, hx + 4, hy - 25, &p->metal, 0.9f, 0.4f); break;
    case 7: cv_rect(cv, hx - 1, hy - 4, hx + 7, hy + 6, &p->metal, 0.95f, 0.8f); cv_flat(cv, hx + 1, hy - 2, hx + 6, hy - 2, 0.3f, 0x504030, 0.8f); cv_flat(cv, hx + 1, hy, hx + 6, hy, 0.3f, 0x504030, 0.8f); cv_flat(cv, hx + 1, hy + 2, hx + 5, hy + 2, 0.3f, 0x504030, 0.8f); break;
    case 8: cv_cap(cv, hx, hy + 10, 0.55f, hx + 1, hy - 12, 0.5f, &p->leather, SH_SPHERE); cv_tri(cv, hx - 5, hy - 12.5f, hx + 1, hy - 14, hx + 7, hy - 8, &p->metal, 0.9f, 0.4f); break;
    case 9: for(int i = 0; i < 5; i++){ cv_cap(cv, hx + (i - 2) * 1.1f, hy + 6, 0.35f, hx + (i - 2) * 1.6f, hy - 10, 0.25f, &p->trim, SH_FLAT); cv_ell(cv, hx + (i - 2) * 1.7f, hy - 11, 0.6f, 1.8f, 0, &p->trim, SH_SPHERE); } cv_flat(cv, hx - 3, hy, hx + 3, hy, 0.6f, p->cloth.c[1], 1); break;
    case 10: for(int i = 0; i < 3; i++){ cv_cap(cv, hx, hy, 0.25f, hx + (i - 1) * 2.5f, hy - 7, 0.15f, &p->cloth2, SH_FLAT); cv_ell(cv, hx + (i - 1) * 2.8f, hy - 8, 1.6f, 0.9f, 0.6f * (i - 1), &p->cloth2, SH_SPHERE); } break;
    case 11: cv_cap(cv, hx, hy + 9, 0.55f, hx, hy - 6, 0.5f, &p->leather, SH_SPHERE); cv_rect(cv, hx - 3.5f, hy - 10, hx + 3.5f, hy - 5.5f, &p->metal, 0.9f, 0.4f); break;
    case 12: cv_ell(cv, hx + 1, hy + 2, 3.6f, 3.2f, 0, &p->trim, SH_SPHERE); for(int i = 0; i < 3; i++) cv_ell(cv, hx + 1, hy + 2, 3.6f - i * 1.1f, 3.2f - i * 1.0f, 0, i & 1 ? &p->cloth : &p->trim, SH_FLAT); break;
    case 13: cv_curve(cv, hx, hy - 12, hx + 6, hy, hx, hy + 12, 0.6f, 0.6f, &p->leather, SH_SPHERE); cv_flat(cv, hx, hy - 12, hx, hy + 12, 0.18f, 0xE8E8E8, 0.9f); break;
    case 14: cv_rect(cv, hx - 1, hy - 5, hx + 7, hy + 4, &p->metal, 0.95f, 0.8f); for(int i = 0; i < 3; i++) cv_dot(cv, hx + 1 + i * 2.3f, hy - 2 + (i & 1) * 2.4f, 0.7f, 0x303030, 1); cv_flat(cv, hx + 1.5f, hy - 2, hx + 6, hy - 2.6f, 0.3f, 0x303030, 0.8f); break;
    default: cv_cap(cv, hx, hy + 12, 0.6f, hx, hy - 17, 0.5f, &p->leather, SH_SPHERE); cv_dot(cv, hx, hy - 17.5f, 1.0f, p->trim.c[2], 1); break; }
    (void)r;
}
static void figure(Pp *p){
    const Stock *s = p->s; SgCv *cv = CV; SgRng *r = &p->r; float H = p->H, cx = p->cx, w = p->w, feet = p->feet;
    float legL = H * (s->leg == 1 ? 0.52f : s->leg == 4 ? 0.2f : s->leg == 5 ? 0.0f : 0.31f), torsoH = H * 0.34f, hr = H * (s->head == 3 ? 0.12f : s->head == 4 ? 0.1f : 0.115f); if(s->head == 3) hr *= 1.1f; if(H < 40) hr = H * 0.15f;
    float hipy = feet - legL, shy = hipy - torsoH, hy = shy - hr * 0.8f, sw = H * 0.13f * w, hw = H * 0.1f * w; p->hr = hr; p->hx = cx; p->hy = hy; p->hipy = hipy;
    cv_shadow(cv, cx, feet + 0.8f, 11 * w, 2.4f, 0.28f);
    // ---- behind the body: cloak, shell, wings, tail, pack
    if(p->way == 13 || p->way == 15 || p->way == 7 || s->ex & X_MASK){ float xy[8] = { cx - sw * 1.2f, shy, cx + sw * 1.2f, shy, cx + sw * 1.8f, feet - 1, cx - sw * 1.8f, feet - 1 }; cv_poly(cv, xy, 4, &p->cloth2, 0.65f, 0.3f); }
    if(s->ex & X_SHELL){ cv_ell(cv, cx, shy + torsoH * 0.5f, sw * 1.45f, torsoH * 0.78f, 0, &p->skin2, SH_SPHERE); cv_pattern(cv, 5, p->skin2.c[0], 3.0f, 0.4f, 11, cx - 14, shy - 4, cx + 14, hipy); }
    if(s->ex & X_WING){ for(int sd = -1; sd <= 1; sd += 2){ for(int i = 0; i < 5; i++){ float a = -0.6f + i * 0.34f; cv_cap(cv, cx + sd * sw * 0.8f, shy + 2, 1.0f, cx + sd * (sw + 8 + (4 - i) * 1.8f), shy - 8 + i * 4.5f, 0.6f, i & 1 ? &p->skin2 : &p->skin, SH_FLAT); (void)a; } } }
    if(s->ex & X_TAIL){ cv_curve(cv, cx + hw * 0.5f, hipy, cx + hw * 4.0f, hipy + 3, cx + hw * 3.5f, hipy - legL * 0.5f - 4, hw * 0.5f, hw * 0.12f, &p->skin2, SH_SPHERE); if(s->feat == 11 || s->ear == 1) cv_ell(cv, cx + hw * 3.5f, hipy - legL * 0.5f - 5, hw * 0.55f, hw * 0.8f, 0.4f, &p->skin2, SH_SPHERE); }
    if(p->way == 5){ cv_rrect(cv, cx - sw * 1.4f, shy - 1, cx + sw * 1.4f, hipy + 3, 2.2f, &p->leather, SH_SPHERE); cv_ell(cv, cx, shy - 1.6f, sw * 1.2f, 2.0f, 0, &p->cloth2, SH_SPHERE); cv_flat(cv, cx - sw * 1.2f, hipy + 1, cx + sw * 1.2f, hipy + 1, 0.5f, p->leather.c[0], 0.9f); }
    // ---- legs
    if(s->leg == 3){ float tl = H * 0.34f; cv_curve(cv, cx, hipy - 2, cx - hw * 1.6f, hipy + tl * 0.7f, cx + hw * 0.6f, feet - 3, hw * 1.0f, hw * 0.5f, &p->skin, SH_SPHERE); cv_tri(cv, cx + hw * 0.6f, feet - 3, cx + hw * 3.4f, feet - 8, cx + hw * 3.2f, feet + 1, &p->skin2, 0.8f, 0.4f); cv_tri(cv, cx + hw * 0.6f, feet - 3, cx - hw * 1.8f, feet - 8, cx - hw * 2.0f, feet + 1, &p->skin2, 0.8f, 0.4f); }
    else if(s->leg == 5){ cv_ell(cv, cx, hipy - 3, hw * 1.7f, hw * 1.6f, 0, &p->skin, SH_SPHERE); }
    else for(int sd = -1; sd <= 1; sd += 2){ float lx = cx + sd * hw * 0.55f; float lr = hw * (s->leg == 1 ? 0.28f : 0.45f);
        if(s->leg == 2){ cv_cap(cv, lx, hipy, lr * 1.1f, lx + sd * hw * 0.3f, feet - legL * 0.45f, lr * 0.7f, &p->skin, SH_SPHERE); cv_cap(cv, lx + sd * hw * 0.3f, feet - legL * 0.45f, lr * 0.7f, lx, feet - 2, lr * 0.5f, &p->skin, SH_SPHERE); cv_tri(cv, lx - lr, feet, lx + lr, feet, lx, feet - 3, &p->dark, 0.5f, 0.3f); }
        else { cv_cap(cv, lx, hipy, lr, lx + sd * 0.4f, feet - 1.4f, lr * 0.8f, p->way == 4 ? &p->leather : (s->leg == 1 ? &p->skin : &p->cloth2), SH_SPHERE);
            float fw = (s->ex & X_FEET) ? hw * 1.3f : hw * 0.75f; cv_ell(cv, lx + sd * fw * 0.35f, feet - 0.9f, fw, 1.5f, 0, p->way == 4 ? &p->leather : &p->dark, SH_SPHERE); if(s->ex & X_FEET) cv_flat(cv, lx + sd * fw * 0.1f, feet - 0.6f, lx + sd * fw * 1.4f, feet - 0.6f, 0.4f, p->skin2.c[2], 0.9f); } }
    // ---- torso
    { float tw1 = sw * 1.05f, tw2 = hw * 1.05f; float xy[8] = { cx - tw1, shy, cx + tw1, shy, cx + tw2, hipy + 1.5f, cx - tw2, hipy + 1.5f };
      int robe = p->way == 1 || p->way == 14 || p->way == 10 || p->way == 7 || p->way == 15 || p->way == 13; const SgRamp *tr = (s->feat == 4 || s->feat == 14 || s->feat == 11) && !robe ? &p->skin : &p->cloth;
      if(robe){ float yb = feet - 1; if(p->way == 15 || p->way == 13) yb = hipy + legL * 0.55f; float rx[8] = { cx - tw1, shy, cx + tw1, shy, cx + tw1 * 1.5f, yb, cx - tw1 * 1.5f, yb }; cv_poly(cv, rx, 4, &p->cloth, 0.8f, 0.4f); cv_flat(cv, cx - tw1 * 1.5f, yb - 0.4f, cx + tw1 * 1.5f, yb - 0.4f, 0.7f, p->trim.c[2], 0.9f); }
      else { cv_poly(cv, xy, 4, tr, 0.8f, 0.45f); cv_rect(cv, cx - tw2 * 1.02f, hipy - 0.5f, cx + tw2 * 1.02f, hipy + 1.6f, &p->leather, 0.7f, 0.4f); }
      if(p->way == 2 || p->way == 9 || p->way == 11 || p->way == 8){ float xy2[4 * 2] = { cx - tw1 * 0.6f, shy + 1, cx + tw1 * 0.6f, shy + 1, cx + tw2 * 0.9f, hipy + 3, cx - tw2 * 0.9f, hipy + 3 }; cv_poly(cv, xy2, 4, &p->leather, 0.75f, 0.35f); if(p->way == 9 || p->way == 11) for(int sd = -1; sd <= 1; sd += 2) cv_flat(cv, cx + sd * tw1 * 0.6f, shy, cx + sd * tw1 * 0.4f, shy + 3, 0.7f, p->leather.c[1], 1); }
      if(p->way == 6 || p->way == 8 || p->way == 3){ cv_flat(cv, cx - tw1, shy + 1, cx + tw2, hipy, 0.9f, p->leather.c[1], 0.9f); }
      if(p->way == 1) cv_pattern(cv, 6, 0xFFE890, 1.8f, 0.9f, 7, cx - 12, shy, cx + 12, feet);
      if(p->way == 12 || p->way == 4) for(int i = 0; i < 4; i++) cv_flat(cv, cx - tw1, shy + 2 + i * 4, cx + tw1, shy + 2 + i * 4, 1.3f, p->trim.c[3], 0.65f);
      if(p->way == 14) cv_ell(cv, cx, shy + 1, tw1 * 0.9f, 1.8f, 0, &p->trim, SH_FLATLIT);
      if(s->feat == 6 && s->skin2 == 0xE04A20) { /* nothing on cloth */ }
      if(s->feat == 9) { cv_pattern(cv, 7, p->metal.c[0], 2.4f, 0.4f, 9, cx - 12, shy, cx + 12, hipy); for(int i = 0; i < 4; i++) cv_dot(cv, cx - tw1 * 0.7f + i * tw1 * 0.46f, shy + 2, 0.4f, p->metal.c[4], 1); }
      if(s->feat == 12){ for(int i = 0; i < 5; i++) cv_dot(cv, cx + sinf(i * 2.7f) * sw * 0.7f, shy + 4 + i * 2.2f, 0.7f, p->glow, 0.9f); cv_glow(cv, cx, shy + 8, 14, p->glow, 0.2f); } if(s->ex & X_JEWEL){ cv_curve(cv, cx - sw * 0.8f, shy, cx, shy + 7, cx + sw * 0.8f, shy, 0.4f, 0.4f, &p->trim, SH_FLAT); cv_dot(cv, cx, shy + 5.2f, 1.1f, 0xF0D060, 1); } }
    // ---- arms (two, four or six) and the held tool
    int na = s->arms; float handx = 0, handy = 0;
    for(int sd = -1; sd <= 1; sd += 2) for(int k = 0; k < na / 2; k++){ float sy = shy + 1.5f + k * 3.2f, ax = cx + sd * sw * 1.02f; float ex = ax + sd * (4.2f + (k == 0 ? 0 : k * 1.6f)), ey = sy + torsoH * (0.4f + 0.05f * k) + (k ? -1 : 0); float hx = ex + sd * (1.5f - k * 0.5f) - (sd > 0 && k == 0 ? 0 : 0), hy2 = ey + torsoH * (0.28f + 0.03f * k);
        if(sd > 0 && k == 0){ hx = ex + 3.0f; hy2 = ey + torsoH * 0.12f; handx = hx; handy = hy2; }
        int sleeve = (p->way != 6 && p->way != 4 && p->way != 10) || 1; (void)sleeve; const SgRamp *ar = (s->feat == 4 || s->feat == 14 || p->way == 8 || p->way == 15) && p->way != 8 ? &p->skin : &p->cloth;
        if(s->ex & X_WING && k == 0) ar = &p->skin2;
        limb(p, ax, sy, hw * 0.4f, ex, ey, hw * 0.34f, ar); limb(p, ex, ey, hw * 0.34f, hx, hy2, hw * 0.3f, k == 0 && p->way == 9 ? &p->cloth : &p->skin); float hr2 = (s->ex & X_HANDS) ? hw * 0.62f : hw * 0.33f; cv_ell(cv, hx, hy2 + 0.4f, hr2, hr2, 0, &p->skin, SH_SPHERE); if(s->feat == 13 || (s->ex & X_HANDS && p->s->feat == 13)) for(int t = -1; t <= 1; t++) cv_tri(cv, hx + t * hr2 * 0.7f - 0.4f, hy2 - hr2 * 0.6f, hx + t * hr2 * 0.7f + 0.4f, hy2 - hr2 * 0.6f, hx + t * hr2 * 0.7f, hy2 - hr2 * 1.6f, &p->skin2, 0.5f, 0.3f);
        if(s->ex & X_HANDS && s->feat == 0 && s->head == 0) for(int t = -1; t <= 1; t++) cv_dot(cv, hx + t * hr2 * 0.8f, hy2 + hr2 * 0.9f, 0.5f, p->skin2.c[2], 0.9f); }
    held(p, handx, handy);
    // ---- the head, its hat, its surface
    cv_cap(cv, cx, shy + 0.5f, hw * 0.4f, cx, hy + hr * 0.7f, hw * 0.4f, &p->skin, SH_SPHERE); head(p); hat(p);
    if(s->feat == 5){ for(int i = 0; i < 3; i++) cv_flat(cv, cx - sw * 0.7f, shy + 3 + i * 5, cx - sw * 0.1f, shy + 1 + i * 5, 0.5f, 0xFFFFFF, 0.5f); }
    (void)r;
}
static void crowd_fig(Pp *p, float ox, float oy, float k){ cv_view(CV, ox, oy, k); figure(p); }
void sg_person(SgImg *im, int people_id, uint32_t seed){
    int stock = (people_id & 511) % 32, way = (people_id & 511) / 32; Pp *p = (Pp *)calloc(1, sizeof *p); if(!p) return; p->s = &stk[stock]; p->way = way; p->r = sg_rng(seed + 5, people_id * 7919u); cv_init(CV, im, 64); CV->post = 6; SgRng *r = &p->r;
    const Stock *s = p->s; sg_ramp(&p->skin, s->skin); sg_ramp(&p->skin2, s->feat == 6 && s->skin2 == 0xE04A20 ? 0x3A3436 : s->skin2); sg_ramp(&p->metal, s->eyes == 0 ? 0xE8E0D0 : s->feat == 9 ? 0x9AA0A8 : 0xF4F4F0);
    sg_ramp(&p->dark, 0x2A2220); sg_ramp(&p->leather, sg_hsv(0.07f + sg_rs(r) * 0.02f, 0.5f, 0.38f)); p->glow = s->feat == 6 ? 0xFF5018 : s->feat == 9 ? 0x50E0FF : s->feat == 12 ? 0xFFD050 : 0xFFE890;
    static const float wh[16] = { 0.12f, 0.68f, 0.03f, 0.08f, 0.14f, 0.0f, 0.0f, 0.84f, 0.56f, 0.15f, 0.33f, 0.07f, 0.55f, 0.28f, 0.75f, 0.45f };
    float h0 = wh[way] + sg_rs(r) * 0.04f; static const float ws[16] = { 0.45f, 0.5f, 0.35f, 0.45f, 0.75f, 0.45f, 0.1f, 0.55f, 0.4f, 0.5f, 0.45f, 0.7f, 0.55f, 0.35f, 0.5f, 0.2f }, wv[16] = { 0.62f, 0.38f, 0.5f, 0.55f, 0.8f, 0.5f, 0.55f, 0.55f, 0.42f, 0.7f, 0.5f, 0.85f, 0.65f, 0.35f, 0.62f, 0.5f };
    sg_ramp(&p->cloth, sg_hsv(h0, ws[way], wv[way])); sg_ramp(&p->cloth2, sg_hsv(h0 + 0.06f, ws[way] * 0.9f, wv[way] * 0.75f)); sg_ramp(&p->trim, way == 6 || way == 8 ? 0xB8BCC4 : sg_hsv(h0 + 0.5f * (way & 1) + 0.12f, 0.6f, 0.82f));
    if(way == 4) sg_ramp(&p->trim, 0xE8C020); if(way == 11) sg_ramp(&p->trim, 0xF08020); if(way == 6 || way == 8) { sg_ramp(&p->metal, 0xB8BCC4); } if(way == 9) sg_ramp(&p->trim, 0xE0C060);
    p->H = s->h * 0.93f; p->w = s->w10 / 10.0f; p->cx = 32; p->feet = 61;
    if(s->ex & X_CROWD){ crowd_fig(p, -2, 5, 0.9f); p->r = sg_rng(seed + 6, people_id); crowd_fig(p, 14, 8, 0.8f); p->r = sg_rng(seed + 7, people_id); crowd_fig(p, 6, 12, 0.95f); cv_view(CV, 0, 0, 1); }
    else figure(p);
    cv_outline(CV, 0x1C1410, 0.85f); free(p); (void)way_nm;
}
