// pc_gbaaddr.h: where the fake GBA lives. GBAA(0x06000000) is the VRAM address.
// PC_GBA_TOP is added to every fake-GBA address. 0 (the default) = the real GBA addresses, as on Windows and Linux (nothing changes there).
// Android: the Java heap sits on 02000000-12000000, so the Android build moves everything up (build.sh: PC_GBA_TOP=0x30000000 or what the probe app reports).
// The offset must keep every address below 2 GB (the game keeps addresses in 32-bit numbers) and must not carry into another region.
#ifndef PC_GBAADDR_H
#define PC_GBAADDR_H
#ifndef PC_GBA_TOP
#define PC_GBA_TOP 0
#endif
#define GBAA(a) ((a) + PC_GBA_TOP)
#endif
