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

## Version 0.40 touch interface

- Options on touch devices are now a directly tappable list instead of a desktop panel behind the gamepad. Large text, paginated settings, clear values, dedicated Back/Prev/Next controls and wrapped help text fit tall, wide and small windows. The gamepad remains available during gameplay; the layout editor retains its own controls.
- Tap the left/right half of a setting to decrease/increase; tap a command to execute. No duplicate game input after leaving the options, and stray finger-motion events after rotation cannot recreate a released touch.
- Host geometry tests cover the whole paginated touch menu and the emulator smoke test exercises a real touch dismissal of the first-start guide and opens Video via a tap.

## Version 0.41 reliability

- The Android launch test now runs in one checked shell, preserving coordinate variables, and uses the actual SDL surface bounds to translate real taps even with navigation bars or display scaling. It dismisses the tutorial, opens options from the on-screen MODE control, opens Video, and verifies pause/resume across the Home button.
- The first-start guide teaches touch actions and the new settings interaction. SRAM autosave writes and flushes a temporary file, then replaces the old save without first deleting it. Failed writes or replacements preserve the previous file. Windows uses its native replace-existing operation. No save format changes.

## Version 0.42 low-end phone profile

- Video > PHONE PERFORMANCE: AUTO selects the battery profile on low-memory/32-bit phones; BATTERY uses classic rendering, 240x160 internal drawing and alternate-frame presentation (~30 pictures/second), while preserving fixed GBA-speed simulation, input and audio. FULL restores the normal rendering budget within hard memory limits. Existing visual preferences are retained separately.
- Touch Pad > PAD STYLE: SIMPLE uses direct geometry and game action labels; DETAILED retains the original controller. AUTO selects simple controls in the battery profile. Both share the same customizable touch regions. If a decorative texture cannot allocate/upload, the complete simple pad remains visible.
- Detailed mobile pad textures are limited to 256 pixels per side and 6 MiB total. Memory warnings release existing frame/pad textures and force a low-budget session. Default Android composition workers are bounded to two total on 32-bit and four on 64-bit.

## Version 0.43 touch responsiveness

- Brief taps now survive a complete down/up pair between game-input samples. A tap is delivered once, while held fingers still support normal multi-touch and continuous movement. Opposing directions cancel. Options, editor, background, rotation and controller changes clear pending input.
- Resizing refreshes the drawable dimensions immediately and invalidates old option geometry. Disconnecting the last physical controller restores touch controls. No new save format or gameplay timing changes.
- FPS overlay distinguishes actual game-frame presentations from fixed simulation ticks, including the battery profile; paused-menu redraws no longer count toward gameplay FPS.

## Version 0.44 menu rendering

- The opaque Android settings/help UI is now drawn directly: no hidden world composition, game texture allocation, upload, or contrast pass. The touch layout editor and gameplay captures retain the full scene. Menu redraws are scheduled independently of the input poll, approximately 30 per second with 8 ms polling, and menu feedback is visible above the footer.
- A host regression exhausts the menu-only policy and tests the redraw scheduler. The Android emulator test additionally requires the actual composition-bypass diagnostic before continuing through touch navigation and pause/resume. No save format or simulation changes.

## Version 0.45 adaptive performance

- AUTO on phones outside the existing low-memory profile now measures rendering work before VSync. After at least 30 slow renders in a 60-render sample window (13.5 ms threshold), it lowers the internal draw budget to 153,600 pixels; a further slow window selects the existing battery profile. The governor does not change game tick speed or saved user preferences, and ignores launch frames, menus, screenshots and long suspended intervals. Explicit Battery/Full Detail are unchanged; reselect AUTO to reset the session governor. The effective profile appears in Video settings.
- Native scene dimensions now follow live windows even when resized to 240x160, preventing stale high-resolution work after a downshift. Headless diagnostic semantics stay unchanged.
- Host regression exercises normal, reduced, battery, mixed, excluded and long-stall render samples; hardware performance remains device-dependent.

## Version 0.46 screenshot reliability

- Options > SCREENSHOT now requests an actual game capture, even when battery mode would skip that frame. Screenshots remain in the existing screenshots folder, use collision-resistant names, and show success or storage failure feedback.
- PNG encoding uses a 32 KiB streaming buffer rather than allocating two complete images. RGB and RGBA output is preserved, including large pictures, and write errors are returned. The host test checks chunk CRCs, zlib data, all pixels and alpha across block boundaries, invalid dimensions and a full-device write failure.

## Version 0.47 lightweight Android downloads

- The universal APK still contains every common ABI, while four optional standalone APKs contain just the native code for ARMv7, ARM64, x86 or x86-64. They require no split-package installer. CI verifies the native libraries and Java program match the universal build, page alignment, and identical signing certificates within the same build. The x86 and x86-64 emulator runs also install the matching standalone APK over the universal build without clearing data and verify boot. Android debug signing keys can vary between separate CI runs; this does not guarantee cross-run APK upgrades. Keep a backup of existing game saves before changing builds.

## Version 0.48 audio and settings reliability

- Audio synthesis advances by the exact emulated GBA frame duration, independent of speaker queue fullness, trim, mute, or output availability. Only the playback queue is resampled for device drift; diagnostic WAV captures preserve the source sample count. Paused menus/background suspend the callback, and resume discards old audio and primes a short silent buffer instead of synthesizing extra paused frames.
- Android settings now use a temporary file, checked flush/sync and atomic rename. Storage errors leave the prior settings file intact. No campaign save format change. Host tests cover exact long-run sample counts, stereo resampling, queue bounds, and forced settings write failures.

## Version 0.49 small-window and 32-bit input fixes

- Touch layout editor dimensions are bounded by both window axes; normal windows keep all twelve editing commands, including Done. Extremely small split-screen windows display a safe exit instead of a clipped panel. Selection placement uses a fixed-width 64-bit score rather than an invalid long shift on 32-bit architectures.
- Rotation/background surface loss is rejected before layout-editor input, and physical keyboard reads are bounded by SDL's reported key-state length and the local key cache. Host tests exercise portrait, landscape, tiny, tall and large displays, all edit buttons, and invalid keyboard scancodes. No save format changes.

## Version 0.50 cartridge save transfer

- Options > EXPORT CARTRIDGE SAVE opens Android's document picker and writes a checksummed 32 KiB cartridge backup (.bgsave) from a snapshot of SRAM. RESTORE CARTRIDGE SAVE validates a backup, asks for confirmation, and stages it for the next fresh game launch. Quit via Options and reopen to apply. The previous raw cartridge file is retained as bugstar.sav.before-import. A failed restore leaves the existing save intact; a pending restore never overwrites live SRAM during play. Export includes the three cartridge slots and cartridge settings, not PC-specific sidecar content or external settings.ini. Files can be stored outside the app before an uninstall or moved to another device. No storage permission needed; Android 5.0+ document picker.

## Version 0.51 save-transfer verification

- The PNG host test now extracts just the real encoder. Java and C save codec tests cover matching CRC32, bad headers, wrong sizes, and rollback; the emulator additionally visits and cancels the real export picker, stages a valid backup in the private debug sandbox, restarts the actual game, and verifies recovery bytes and one-time restore. The activity uses singleTop so the system document picker can return a result in the same task.
- The emulator's staged restore uses an isolated test save, not player data. Physical ARM devices and a complete manual document-provider write/import flow still need device testing.

## Version 0.52 recovery exports

- Options > EXPORT PREVIOUS SAVE now opens the document picker with the cartridge from the last pre-import recovery file. It does not touch current progress or the recovery copy. If no complete cartridge exists, it shows a message. The recovery copy is replaced by the next successful import; export it first if it is needed long term.
- Android 5.0's document picker can have a navigation drawer and keyboard open simultaneously. The smoke test waits for the window, dismisses those layers while the picker is foregrounded, and requires a real cancellation result. Startup restore now also checks that a sentinel byte in the previous file survives in recovery but not in the restored live cartridge.

## Version 0.53 flight, dungeon and economy pass

- Brake before planet landing, station docking, or rock landing. L+A explicitly fires even in approach zones; ordinary laser pulses hit only the closest valid enemy or asteroid. A brighter reticle confirms an impact. Station purchases honor the ship's cargo bonus. Fuel burn cannot underflow.
- Dungeon speed-run time is retained when revisiting the same remembered floor during a trip; final fractional seconds remain visible. Gas and cave-in countdowns provide warnings, completed floor goals remain visible, and full bags give context-sensitive feedback. Long messages fit the GBA view. Field guide updated to explain flight and hazards. No persistent save schema changed.

## Version 0.54 exploration and trading

- Fixed mine-only event indexing and bounded event labels. Added stable cockpit direction/range guidance for the station and world, widened distant scanner arithmetic, and removed undefined negative signed shifts from line rasterization. Station trade hints use the real next-visit price formula, accessible stars and fuel limits; the estimate is a gross unit premium, not net profit. Transactions display local feedback and the effective hold capacity. The scrollable ship field guide has ten pages, with separate hazard and navigation/trading sections.
