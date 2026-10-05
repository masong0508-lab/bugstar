// itemids.h - sprite size and sprite numbers, shared by the GBA game (items.h) and the PC baker (tools/bake_items.c).
#define IW 21
#define IH 28
#define IOX 10   // sprite column of the tile centre
#define IOY 22   // sprite row of the tile centre
#define IKEY 0x8000
// Sprite numbers. EVERY item sprite is pre-baked in ROM (itemrom.h, generated), so new items cost no RAM, only ROM.
// NEW ITEM: add its art to itembake.h (or an art header it includes), add its V_ number here BEFORE NIV, add one bakeOne(...) line
// in bakeAll() in itembake.h, run tools/bake_items.sh, then draw it with blitItem(V_xxx,..) in items.h. NIV is the sprite count.
enum { V_CRATE, V_FRIDGE, V_TOILET=V_FRIDGE+4, V_RAILU=V_TOILET+4, V_RAILV, V_DOOR, V_BOARD,
       V_KICKER, V_QPIPE=V_KICKER+4, V_LEDGEU=V_QPIPE+4, V_LEDGEV, V_BENCHU, V_BENCHV, V_BED, V_SHOWER=V_BED+4, V_SOFA=V_SHOWER+4,
       V_LAUNCH=V_SOFA+4, V_FUNBOX=V_LAUNCH+4, V_BARREL, V_TRASH, V_PLANTER, V_PICNIC, V_JERSEYU, V_JERSEYV, V_MPAD, NIV };
