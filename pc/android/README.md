# pc/android - the Android build (work in progress)

Plan and steps: `pc/ANDROID_PLAN.md`. arm64-v8a only.

State: the game runs (see patch 46 at the end). The first steps (patch 24, step 1b) were a test app. Patch 21 found that Android's Java heap covers 02000000-12000000, so the fake GBA cannot sit at its
real addresses. `app/jni/src/gba_test.c` now looks for a free offset TOP (every GBA address + TOP, see `pc/platform/pc_gbaaddr.h`, build with `PC_GBA_TOP=...`).
It shows a green (PASS) or red (FAIL) screen and a text box with the result per TOP; the text is also on the clipboard.

How it builds (GitHub Actions, `.github/workflows/android.yml`): the workflow clones SDL2 into `app/jni/SDL` (not committed), then
`gradle -p pc/android assembleDebug` builds `app-debug.apk` (SDL's Java files + `libSDL2.so` + `libmain.so`, ndk-build, arm64-v8a, minSdk 24, targetSdk 34).
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
`pc/tests/speed.sh` proves the pacing on the PC.

## Patch 46: what the app has now, and what CI checks

- The whole game as it is on PC: the procedural sprites (planet objects, space backdrops of nebulae and galaxies, generated planet discs, dungeon monsters; only in NATIVE / ULTRA, phones start on CLASSIC), the sim side game, laws lab, warships, shipyard, giant numbers. `pc/gen/sources.txt` lists 43 sources; the workflow fails if the sprite generators or the big modules fall out of it.
- Touch: every screen of the game is played with the GBA's ten keys, which the on-screen pad (and its layout editor) gives; the options menu is driven by the pad's A (Enter), B and MENU (Esc). Nothing new needs a keyboard or a mouse. The first-start tutorial (HOW TO PLAY, also in the options) is dismissed with A or B.
- Low memory: `BugstarActivity.onTrimMemory` (level RUNNING_LOW or worse) and SDL's `onLowMemory` send `SDL_APP_LOWMEMORY`; the game empties its sprite cache (`sg_cache_trim`) and writes the save. The sprite cache is capped at 12 MB on Android (32 MB on PC; `BUGSTAR_SG_CACHE_MB` changes it), and pictures that are not ready are made on one background thread, so the game thread never waits for a generator.
- After a push, check CI with `gh run list -R masong0508-lab/bugstar -w android.yml -L3` and, if it failed, `gh run view <id> --log-failed`.
