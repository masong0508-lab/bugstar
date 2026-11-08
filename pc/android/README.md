# pc/android - the Android build (work in progress)

Plan and steps: `pc/ANDROID_PLAN.md`. arm64-v8a only.

State (patch 24, step 1b): a **test app**, not the game yet. Patch 21 found that Android's Java heap covers 02000000-12000000, so the fake GBA cannot sit at its
real addresses. `app/jni/src/gba_test.c` now looks for a free offset TOP (every GBA address + TOP, see `pc/platform/pc_gbaaddr.h`, build with `PC_GBA_TOP=...`).
It shows a green (PASS) or red (FAIL) screen and a text box with the result per TOP; the text is also on the clipboard.

How it builds (GitHub Actions, `.github/workflows/android.yml`): the workflow clones SDL2 into `app/jni/SDL` (not committed), then
`gradle -p pc/android assembleDebug` builds `app-debug.apk` (SDL's Java files + `libSDL2.so` + `libmain.so`, ndk-build, arm64-v8a, minSdk 24, targetSdk 34).
The artifact is called `bugstar-apk`.

Install on the phone, from Termux, after a push (one line):

    sleep 15; id=$(gh run list -w android.yml -L1 --json databaseId -q '.[0].databaseId') && gh run watch $id --exit-status && gh run download $id -n bugstar-apk -D ~/storage/downloads/bugstar-apk && termux-open ~/storage/downloads/bugstar-apk/app-debug.apk

(First time: Android asks to let Termux install apps. If `git push` refuses the workflow file, run `gh auth refresh -s workflow`.)

If the app closes or shows a black screen (patch 35): start it once more, then in Termux `cat ~/storage/downloads/bugstar_log.txt`. It holds the crash backtrace (`pc <offset> libmain.so`),
the game's own log and the app's logcat. Download the artifact `bugstar-symbols` from the same CI run and run `llvm-addr2line -f -e libmain.so <offset>` (Termux: `pkg install llvm`) to name the line.

Speed (patch 43): the game steps at the GBA's own 59.7275 Hz from a fixed-timestep clock (`plat_frame` in `pc/platform/pc_sdl.c`), never from the screen, so 60 / 90 / 120 Hz phones all run it at the same speed
(vsync only paces the picture's show). `BugstarActivity` sets `Surface.setFrameRate(59.73)` as a hint (API 30+). Home button / screen off: the game and the sound stop, the save is flushed, and a fresh clock starts on return.
`pc/tests/speed.sh` proves the pacing on the PC.
