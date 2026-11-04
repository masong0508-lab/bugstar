# BUGSTAR on Android (arm64 APK) - plan, started 3 Nov 2026

Goal: a playable APK, **arm64-v8a only** (no 32-bit build), that runs what `pc/bugstar_pc` runs: the game's own C code on the fake GBA
(`pc_hw.c`), the SDL2 window / audio / gamepad front end (`pc/platform/pc_sdl.c`), and new touch controls.

Rough size: 6 steps, one patch each, about 15 to 35 hours of work in total. These are my guesses, not measurements.
Two things can swing them: step 1 (a go / no-go test) and step 5 (speed on a real phone).

## Why it is not just "compile it for Android"

1. **Addresses in 32 bits.** The game keeps some addresses in `u32` (for DMA). The Linux build gets away with it by linking `-no-pie` and running the
   game on a stack below 4 GB (`pc_main.c`); the Windows build by sitting at a low base. In an APK the game is a shared library loaded high, so
   statics and the stack are above 4 GB and those `u32`s lose their top half. Found so far:
   - DMA-3 fills and copies that write the source / destination as `(u32)&something`: `film.inc:229`, `m7.inc:142` and `187-189`, `sizes.inc:222`, `space.inc:787`
   - the three sound-DMA lines `m7.inc:72-76` (probably harmless, check) and the stack probe `debug.inc:352`
   - step 2 finds the rest with the compiler's own warnings.
2. **Fixed addresses.** The fake GBA maps the real GBA ranges (`02000000` to `0E010000`, `pc_hw.c`, `MAP_FIXED_NOREPLACE`). An Android app
   process already has ART (the Java runtime) and other things in the low 4 GB, so one of the ranges may be taken. Windows had the same
   trouble (patch 01). This is the go / no-go: step 1.
3. **No `makecontext` in Android's libc.** `pc_main.c` uses ucontext to run the game on a low stack. It goes away once step 2 is done.
4. **No keyboard.** Needs touch controls. The game paces itself at 60 fps: check it on 90 / 120 Hz screens.
5. **gcc to clang.** The NDK builds with clang, `build.sh` uses gcc. Expect a few small fixes (gcc-only syntax, the `.incbin` of `facts.bin`).

## Step 0 - facts about the phone (5 minutes, Termux)

    getprop ro.product.model; getprop ro.build.version.release; getprop ro.product.cpu.abilist; getconf PAGE_SIZE

Needs `arm64-v8a` in the list. PAGE_SIZE 4096 or 16384 are both fine (step 3 builds for both).

## Step 1 - go / no-go test: can an app map the GBA addresses? (patch 21, 2 to 4 h)

- `pc/android/`: a Gradle project made from SDL2's own `android-project` template (latest SDL2 2.x release; get it in CI, do not commit it), `arm64-v8a` only.
- Its `SDL_main` only tries to map the 8 ranges (the same list as `win_launch` in `pc_main.c`) at their exact addresses and shows PASS or FAIL
  with the first bad address on screen. On a FAIL it also shows the lines of `/proc/self/maps` around it.
- `.github/workflows/android.yml`: on every push, build the debug APK on the GitHub runner and upload it as the artifact `bugstar-apk`.
  (Building the Android SDK + NDK inside Termux is painful; the runner already has them.)
- The loop on the phone, one command after each `git push`:

      sleep 10; id=$(gh run list -L1 --json databaseId -q '.[0].databaseId') && gh run watch $id --exit-status && gh run download $id -n bugstar-apk -D ~/storage/downloads/bugstar-apk && termux-open ~/storage/downloads/bugstar-apk/app-debug.apk

  (first time: allow Termux to install apps; debug-signed APKs install fine.)
- **Pass** = all 8 ranges map. Go on.
- **Fail** = move the fake GBA to other low addresses. The game's code uses the literal addresses (`0x06000000` ...), so `build.sh` would
  rewrite them with `sed`, as it already rewrites many other things. Invasive but possible. Only do it if the test fails.

## Step 2 - make the game 64-bit clean, test it in Termux (patch 22, 2 to 4 h)

- `build.sh`: `sed` turns every `*(volatile u32*)0x040000D4=(u32)X;` (and `D8`) into `pc_dma_sad((uintptr_t)(X));` / `pc_dma_dad(...)`.
  `pc_hw.c`: `pc_dma3` uses those full-width values (fake-GBA addresses are the same numbers as before).
- `pc_main.c`: drop the ucontext low stack; run the game on a pthread with a big stack (16 MB) instead.
- `PC_PIE=1` in `build.sh`: a normal PIE build (no `-no-pie`, no low stack) with `-ffp-contract=off` (so arm64 and x86 give the same numbers) and the
  warnings `-Wpointer-to-int-cast -Wint-to-pointer-cast` switched on for `pc/gen`. Every warning left that is not a fixed GBA address is a place to fix.
- Run it in Termux, which is arm64 + Android's libc + clang, the closest test before the APK:

      pkg install clang python git make
      PC_PIE=1 PC_CC=clang PC_SDL=0 ./pc/build.sh && for t in pc/tests/determinism.sh pc/tests/seeds.sh pc/tests/warship.sh pc/tests/aftermath.sh; do sh $t; done

- Pass = the same hashes / `seeds.golden` as on the PC. If `seeds.golden` differs on arm64, find out why before going on:
  a save should load the same on the phone and on the PC.
- Also run the old `-m64 -no-pie` PC build and the tests once more: the Linux and Windows packages must not change.

## Step 3 - the real game inside the APK: the title screen (patch 23, 3 to 6 h)

- `build.sh`: `PC_ANDROID_PREP=1` makes `pc/gen` and the assets and writes `pc/gen/sources.txt` (the list of .c files), without compiling.
  CMake reads that list, so the two never drift apart.
- CMake builds `libmain.so` from `pc/gen` + the `.c` files (same flags as `build.sh`: `-funsigned-char -fwrapv -fno-strict-aliasing -std=gnu11`,
  plus `-DPC_SDL`) and SDL2 itself. Link flag `-Wl,-z,max-page-size=16384` (phones with 16 KB pages). SDL2's real headers, not `pc/platform/sdl2/include`.
- `pc_main.c`: under `__ANDROID__`, `#include <SDL.h>` so `main` becomes `SDL_main`; open the window with no arguments.
- Saves: `SDL_GetPrefPath` already gives the app's private folder; check that the `.sav` really lands there.
- Goal (milestone 2): the title screen and the music on the phone. For now a Bluetooth pad or a USB keyboard plays it.

## Step 4 - touch controls and phone life cycle (patch 24, 4 to 8 h)

- Landscape lock, fullscreen, keep the screen on.
- On-screen controls drawn in `pc_sdl.c` under `__ANDROID__`: d-pad, A, B, L, R, Start, Select, a COIL button (= the Q+E combo), a menu button
  (= Esc, the options). Multi-touch with `SDL_FINGER*` events into `pc_set_keys`. Android's Back button opens the options too.
  Controls fade when a pad is connected, size and opacity are options.
- Life cycle: on `SDL_APP_WILLENTERBACKGROUND` / `TERMINATING` call `pc_save_flush(1)`; pause the sound.
- Goal (milestone 3): playable with fingers, a save survives swiping the app away.

## Step 5 - tuning on a real phone (patch 25 and on, 3 to 10 h, the big unknown)

- Speed: default to a light video mode on Android (the native 3D views cost 34 to 44 ms a frame at 1200x800 on a 2-core PC for the kart, see README);
  an fps overlay as a menu toggle (a `slowlog.on` file is hard to place on Android); raise the quality only when the numbers allow.
- Pacing: 60 fps game on 90 / 120 Hz screens (timer pacing, not vsync alone), no judder, no double speed.
- Audio: buffer size, crackle, pause on a phone call.
- Crash log: the same idea as the Windows `bugstar_log.txt` + error box (`SDL_ShowSimpleMessageBox`); stderr goes nowhere on Android.
  Optional: `pkg install android-tools`, then wireless debugging to the phone itself gives `adb logcat` inside Termux.
- Heat and battery: a 30 minute play test.
- Goal (milestone 4): steady 60 fps in the places that matter (surface, dungeon, space, kart, warship deck).

## Step 6 - release polish (patch 26, 2 to 4 h)

- Icon (the bug), app name, version number from the patch number, release keystore kept as a GitHub secret, signed APK on each tag.
- Save export / import (copy a `.sav` to the Downloads folder and back) so a phone and a PC can share a game.
- `pc/README.md` gets an Android section. Play Store (only if you want it): an AAB instead of an APK, the newest target API level, 16 KB pages (done in step 3).

## How to keep it short

- Step 1 first: it is the only step that can force a redesign, and it is cheap.
- Reuse the SDL front end; do not write a second renderer. First APK = debug-signed, light video mode, no Play Store.
- One patch per step; commit and let CI build after each one. Stop at any milestone: each one is a game you can already use.

## Open questions

- Is the repo public or private? (GitHub Actions minutes are free for public repos, limited for private ones.)
- Phone model and Android version (step 0).
