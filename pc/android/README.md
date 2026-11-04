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
