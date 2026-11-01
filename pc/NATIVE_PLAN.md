# BUGSTAR PC: NATIVE PLAN

Goal: a real PC game, not "a GBA emulator in a window". The game's own C code (rules, worlds, dungeons, music data) stays and runs as native
64-bit code. Everything that only exists because of GBA hardware is a seam we cut, one at a time, and replace with a PC thing.
Short sentences on purpose. Status is at the end of each section (DONE / PART / NOT YET).

## 1. What is still GBA-shaped (the inventory)

| # | Thing | Where | Why it is GBA-shaped |
|---|---|---|---|
| 1 | Memory map | `pc_hw.c` `map_at()` | EWRAM, IWRAM, I/O, palette, VRAM, OAM, SRAM live at the real GBA addresses (`0x02000000`...), because the game writes `*(volatile u16*)0x06000000`. |
| 2 | Time | `pc_vcount()` | The beam line is a counter: 228 reads of VCOUNT = one frame. The game runs on a perfect machine at exactly 60 steps a second of *game* time. |
| 3 | The picture | VRAM + OAM + palette | A 240x160 8-bit bitmap page (mode 4) plus 128 hardware sprites plus a blend unit. |
| 4 | The 3D views | `terrace.inc`, `planet.inc`, `m7.inc`, `space.inc`, `kart*.inc` | Made in 120 columns (each drawn two pixels wide) with tables sized for 120 and 160. Palette-index dithering (Bayer) is part of the look. |
| 5 | Input | `REG_KEYINPUT` | 10 buttons, active low. |
| 6 | Sound | `SND(...)` writes | Four voices (2 square, 1 wave, 1 noise). The music is made by the game's own code and played by writing chip registers. |
| 7 | Save | 32 KB SRAM | Same layout as the cartridge. |
| 8 | Numbers | `s32` fixed point everywhere | This is what makes worlds repeatable. It is NOT removed: determinism is a feature. |
| 9 | Stack and pointers | `pc_main.c` | The game stores addresses in 32-bit numbers (for DMA), so the PC build runs it on a stack below 4 GB. |
| 10 | Font and UI | 3x5 font, 240x160 layout | The game's screens are laid out in GBA pixels. |

## 2. The seams (what we cut, in order)

### Seam 1: the platform seam (window, input, pacing). DONE
`pc/platform/pc_sdl.c`. The fake GBA calls `plat_frame()` once per game frame. That function: shows the picture, queues the sound, waits for the next
60th of a second, reads keyboard and gamepad, and writes the GBA key register. The game does not know. Game time is still 60 steps a second, always;
the screen can run at any refresh rate (60, 144, 240) and any window size, and a slow frame never changes game speed.
- window: resizable, fullscreen (borderless desktop, F11 or Alt+Enter), high-DPI aware, vsync on/off, icon, title, minimum size
- pacing: timer driven at 60.00 Hz, display independent; if the display is slow, pictures are skipped, game steps are not
- focus loss pauses the game (option)
- Headless script mode (`--script`, `--view`) is untouched and is what the tests use.

### Seam 2: the save seam. DONE
- Save file lives in the per-OS user folder (`SDL_GetPrefPath`): Linux `~/.local/share/BUGSTAR/`, Windows `%APPDATA%\BUGSTAR\`, macOS `~/Library/Application Support/BUGSTAR/`. Or next to the program with `portable.txt` or `--portable`, or anywhere with `--data DIR`.
- Written when the game changes SRAM (checked every 2 seconds) and at exit. Atomic: temp file, then swap. A crash never leaves half a save.
- Settings (`settings.ini`) are next to it. The GBA save layout is unchanged, so a save from the cartridge still loads.

### Seam 3: the sound seam. DONE
`pc/platform/pc_psg.c`: the four-voice chip as a software synth (pulse with duty, sweep, envelope, length; wave RAM with volume steps; 7/15-bit noise).
`build.sh` turns the game's `SND(x)=v` writes into `pc_snd(x,v)` calls. The music code is the game's own. 48 kHz stereo, a small ring buffer, the number of
samples per frame is nudged by a few samples so audio and the 60 Hz clock never drift apart. Volume and mute in the options screen. `--wav FILE` writes the
sound to a file (this is how it is tested without a sound card).
- NOT YET: a better synth (band-limited squares). The squares are four-times oversampled and low-passed; good, not perfect.

### Seam 4: the input seam. DONE
10 GBA buttons, each with two keyboard keys and one gamepad button, all rebindable in the options screen (Esc). Left stick and d-pad both move. Triggers are L and R.
Gamepad hot-plug works. The cartridge rumble motor pin (the game has a full rumble engine) drives the gamepad's motors.

### Seam 5: the picture seam (native size). PART
`pc_compose_native(w, h)` in `pc_hw.c` makes the picture straight at the window's pixel size. Not "make 240x160, then stretch".
- The bitmap layer is sampled per output pixel through its affine matrix (so any zoom/rotation is sampled at real resolution).
- Sprites, including the rotating and scaling ones, are drawn from the sprite table per output pixel. Their edges are as sharp as the window.
- The blend unit (fades, alpha, brighten, darken) runs per output pixel.
- Two scaling modes: CRISP = a whole-number multiple of 240x160, black bars (sharp even pixels at any size); SMOOTH = exact fit to the window at any size, texel edges blended over one output pixel ("sharp bilinear": flat inside a texel, soft only at its edge).
- A cap on drawn pixels (auto about 2.4 megapixels) keeps 4K windows fast: it draws smaller and the GPU scales up by a whole number in crisp mode.
- Check: at 240x160 it gives the same picture as the old `compose()` (byte for byte, tested). At 960x640 it gives the same picture as the old one stretched 4x where there is no rotation (tested, 0 pixels differ).
- Honest limit: this seam stops at the game's drawing. The 3D views still make their picture at the game's own grid (see Seam 6).

### Seam 6: native-resolution 3D. NOT YET (the plan)
What is needed: the 3D views write 8-bit palette indices into the 240x160 page. To draw them at the window's resolution the renderers themselves must run at a size chosen at run time.
Steps, in order of payoff and risk:
1. `terrace.inc` (dungeon ray caster): make the column count `RW` a run-time number (today a literal 120 in loops, `camtab[]`, `rdx[]`, `zbuf[]`, `pg[y*120+c]`), write into a separate index buffer `hr3d[RH][RW]`, and let the compositor use it where the 240x160 page still holds what the 3D pass wrote (a snapshot compare says which pixels the HUD has covered). Textures are point-sampled, so walls get finer, not blurrier.
2. `planet.inc` (the planet ray march, 120 columns x N steps): same change; the step tables (`stp_f`, `stp_z`, `stp_base`) are per step, not per column, so they do not change; only the column loop and `rayvx/rayvy` do.
3. `space.inc` (3D space) and the ship model (`ship.inc`): triangles and lines are drawn with the game's own `m7_pix` style helpers; give those a scale factor.
4. `kart*.inc`: the road is drawn with a per-scanline DMA (HBlank). That needs the DMA timer to exist first (`pc_dma3` only does "start now" copies). Last.
Each step ships alone and falls back to 240x160 if the buffer is off. The sprite and HUD layers stay at native size from Seam 5.

### Seam 7: PC-only screens. PART
- Options screen (Esc, or Back+Start on a pad): video (fullscreen, scaling, vsync, max draw size, fps counter, window size, pause on focus loss), audio (volume, mute, rumble), keyboard rebinding (2 keys per button), gamepad rebinding, dead zone, about (shows the save folder). DONE
- Screenshots (F12) as PNG into the save folder. DONE
- NOT YET: mouse control, menu text at a nicer size than the game's 3x5 font, a language/accessibility page (high-contrast, bigger text).

### Seam 8: the stack seam (Windows). PART
Linux uses `makecontext` on a stack below 4 GB. Windows has no `ucontext`; the plan there: a small assembly context switch (or a fibre) on a stack allocated with `VirtualAlloc` at a low address, the program linked at a low image base with high-entropy ASLR off, and the GBA memory map from `VirtualAlloc` at the fixed addresses. See `pc/platform/README.md` for what was actually built and run.

## 3. What stays GBA-shaped on purpose
- Integer game rules (`s32`/`s64` fixed point): same address, same world, on every machine.
- The 32 KB save layout: your cartridge save loads, and the PC save loads in the GBA game.
- The 60 Hz game step.
- The game's palette-indexed look (dither, limited colours) unless a later seam says otherwise. We make it sharp, we do not repaint it.

## 4. Order of work
1. Plan (this file). 2. Platform, input, audio, saves, native picture, options (the first push). 3. Linux package and Xvfb checks. 4. Windows build and zip.
5. Seam 6 step 1 (dungeon). 6. Seam 6 step 2 (planet). 7. The rest.
