# pc/android - the Android build (work in progress)

Plan and steps: `pc/ANDROID_PLAN.md`. Version 0.39 targets Android 5.0 (API 21) or newer with ARMv7, ARM64, x86 and x86-64 native libraries. A working OpenGL ES 2.0 graphics stack is still required. These are target compatibility claims until actual device/CI testing passes.

State: the game runs (see patch 46 at the end). The first steps (patch 24, step 1b) were a test app. Patch 21 found that Android's Java heap covers 02000000-12000000, so the fake GBA cannot sit at its
real addresses. `app/jni/src/gba_test.c` now looks for a free offset TOP (every GBA address + TOP, see `pc/platform/pc_gbaaddr.h`, build with `PC_GBA_TOP=...`).
It shows a green (PASS) or red (FAIL) screen and a text box with the result per TOP; the text is also on the clipboard.

How it builds (GitHub Actions, `.github/workflows/android.yml`): the workflow clones SDL2 into `app/jni/SDL` (not committed), then
`gradle -p pc/android assembleDebug` builds `app-debug.apk` (SDL's Java files + `libSDL2.so` + `libmain.so`, ndk-build, four ABIs, minSdk 21, targetSdk 34).
The artifact is called `bugstar-apk`.

Install on the phone, from Termux, after a push (one line). The sandbox this was built in cannot download CI artifacts (no `gh run download` to the GitHub artifact store), so this is the
command to run on the phone. The file is now called `bugstar-<version>-debug.apk` (patch 46: version 0.36, versionCode 36; bump `versionCode` and `versionName` in `app/build.gradle` for every release, Android refuses to install over a higher versionCode):

    sleep 15; id=$(gh run list -w android.yml -L1 --json databaseId -q '.[0].databaseId') && gh run watch $id --exit-status && rm -rf ~/storage/downloads/bugstar-apk && gh run download $id -n bugstar-apk -D ~/storage/downloads/bugstar-apk && termux-open "$(ls -t ~/storage/downloads/bugstar-apk/bugstar-*-debug.apk | head -1)"

Only the latest run of a push that touched the game or the workflow exists; to get the build of a particular commit use `gh run list -w android.yml -L10` and pass that run's id instead of `$id`.

(First time: Android asks to let Termux install apps. If `git push` refuses the workflow file, run `gh auth refresh -s workflow`.)

If the app closes or shows a black screen (patch 35): start it once more, then in Termux `cat ~/storage/downloads/bugstar_log.txt`. It holds the crash backtrace (`pc <offset> libmain.so`),
the game's own log and the app's logcat. Download the artifact `bugstar-symbols` from the same CI run and run `llvm-addr2line -f -e libmain.so <offset>` (Termux: `pkg install llvm`) to name the line.

Speed (patch 43): the game steps at the GBA's own 59.7275 Hz from a fixed-timestep clock (`plat_frame` in `pc/platform/pc_sdl.c`), never from the screen, so 60 / 90 / 120 Hz phones all run it at the same speed
(vsync only paces the picture's show). `BugstarActivity` sets `Surface.setFrameRate(59.73)` as a hint (API 30+). Home button / screen off: the game and the sound stop, the save is flushed, and a fresh clock starts on return.
`pc/tests/speed.sh` and the new `pc/tests/speed_guard.sh` exercise the shared PC/Android pacing implementation on SDL's dummy video driver. A saved PC-only GAME SPEED law can no longer silently speed the APK up; normal play is locked to GBA speed unless the explicit `BUGSTAR_ALLOW_SPEED_LAW=1` developer opt-in is set. Long stalls drop time instead of racing to catch up.

## Patch 46: what the app has now, and what CI checks

- The whole game as it is on PC: the procedural sprites (planet objects, space backdrops of nebulae and galaxies, generated planet discs, dungeon monsters; only in NATIVE / ULTRA, phones start on CLASSIC), the sim side game, laws lab, warships, shipyard, giant numbers. `pc/gen/sources.txt` lists 43 sources; the workflow fails if the sprite generators or the big modules fall out of it.
- Touch: every screen of the game is played with the GBA's ten keys, which the on-screen pad (and its layout editor) gives; the options menu is driven by the pad's A (Enter), B and MENU (Esc). Nothing new needs a keyboard or a mouse. The first-start tutorial (HOW TO PLAY, also in the options) is dismissed with A or B.
- Low memory: the native render area is capped at 650,000 pixels even with unusually high settings; failed GPU texture allocation retries at 240x160; lost EGL textures are recreated after Android resumes. `BugstarActivity.onTrimMemory` (level RUNNING_LOW or worse) and SDL's `onLowMemory` send `SDL_APP_LOWMEMORY`; the game empties its sprite cache (`sg_cache_trim`) and writes the save. The sprite cache is capped at 12 MB on Android (32 MB on PC; `BUGSTAR_SG_CACHE_MB` changes it), and pictures that are not ready are made on one background thread, so the game thread never waits for a generator.
- After a push, check CI with `gh run list -R masong0508-lab/bugstar -w android.yml -L3` and, if it failed, `gh run view <id> --log-failed`.

## Version 0.39 compatibility

- All four common Android ABIs in one APK; 16 KiB ELF and ZIP alignment checks for modern 64-bit devices.
- Try safe alternate low addresses when the preferred fake GBA map is occupied, rolling back partial allocations. Never overwrite existing ART or GPU mappings.
- Low-RAM and 32-bit devices cap the render area at 480x320, use a 4 MiB sprite cache and start new settings in reduced-detail classic mode. Low-memory warnings reduce only the current session's rendering budget; existing saves and settings are retained.
- Tall portrait windows reserve space below the game for touch controls. Very small split-screen windows scale down without clipping. Resizing or loss of focus releases held touches; GPU reset recreates font and game textures. On-screen buttons remain bounded even with large saved layouts.
- Local multiplayer has the Android INTERNET permission. Device-specific drivers, optional controllers and vendor OS restrictions still require hardware testing.
