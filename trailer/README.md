# The trailer

How the trailer was made, so it can be made again.

- `rec.c`: a headless mGBA recorder (built like `run.c` in the tools). Script lines: `rec <start> <end> <step>` records frames,
  `recf <start> <end> <hexaddr>` records one frame every time the game bumps the counter at that address (the launch shot), `shot N`, `<start> <end> <keys>`.
- `batch.py`: builds the filming ROMs (`-DTEST_FILM` and the autopilots) and records every clip into `clips/` as raw RGB, with a contact sheet each.
- `clip.py`: one clip at a time.
- `tl.py`: the timeline (seconds) the music and the edit share.
- `music.py`: the soundtrack, synthesized from scratch with numpy/scipy. It quotes the game's own tunes (title, space, boss, kart) from `sound.inc`.
- `edit.py`: the edit: crops, 8x pixel scaling, the text cards (Inter for words, the game's 3x5 font for the numbers and the title), the record stop,
  the launch shot's slow motion, the flash and the title. It writes `bugstar_trailer.mp4` (1920x1080, 30 fps, H.264 + AAC, 2:57).
- `pixfont.py`: the game's font, copied from `gen.py`.

The launch shot is drawn by the game itself (`film.inc`, a `-DTEST_LAUNCH` build): a sunrise over a planet, the ship lifts off and flies into the lens.
