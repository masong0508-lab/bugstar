#!/usr/bin/env python3
"""Turn songs (mp3 / ogg / wav / flac / m4a - anything ffmpeg reads) into streamed 4-bit IMA-ADPCM for the GBA jukebox.
Spec: mono, 18157 Hz (the game's music rate), about 9 KB per second.

usage:  python3 tools/encode_song.py "my song.mp3" [other.wav ...]
        -> writes source/music/<id>.adp and appends a SONG_ADP line to source/songs.h (skipped if it is already there)

The song title on screen is made from the file name (capitals / digits / spaces only, 30 characters max).
Edit the title in source/songs.h if you want it different. Run from the project root.
Needs:  ffmpeg and numpy   (Termux: pkg install ffmpeg python-numpy)"""
import sys, os, re, subprocess
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from encode_sfx import encode, decode          # same IMA-ADPCM coder the sound effects use

RATE = 18157        # = MUS_RATE in source/musicdata.h
MAX_SONGS = 32      # JB_MAX in source/jukebox.h
SONGS_H = "source/songs.h"

def load(path):
    af = "loudnorm=I=-16:TP=-1.5,lowpass=f=7500"      # even loudness, nothing above what 18 kHz can hold
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-ac", "1", "-ar", str(RATE), "-af", af,
                          "-f", "s16le", "-"], capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype="<i2").astype(np.int32)

def make_id(stem):
    i = re.sub(r"[^a-z0-9]+", "_", stem.lower()).strip("_") or "song"
    return ("s_" + i) if i[0].isdigit() else i

def make_title(stem):
    t = re.sub(r"[^A-Z0-9]+", " ", stem.upper()).strip()
    return (t or "SONG")[:30].strip()

def add_line(sid, title, path):
    text = open(SONGS_H).read() if os.path.exists(SONGS_H) else ""
    if re.search(r"SONG_ADP\(\s*%s\s*," % re.escape(sid), text):
        print(f"  {SONGS_H} already lists '{sid}' - file replaced, list unchanged"); return
    n = len(re.findall(r"^\s*SONG_(?:XM|ADP)\(", text, re.M))
    if n >= MAX_SONGS: print(f"  WARNING: {n} songs already, the jukebox holds {MAX_SONGS}"); return
    with open(SONGS_H, "a") as f:
        if text and not text.endswith("\n"): f.write("\n")
        f.write(f'SONG_ADP({sid},"{title}","{path}")\n')
    print(f"  added to {SONGS_H}: {title}")

if __name__ == "__main__":
    if len(sys.argv) < 2: sys.exit(__doc__)
    os.makedirs("source/music", exist_ok=True)
    total = 0
    for p in sys.argv[1:]:
        stem = os.path.splitext(os.path.basename(p))[0]
        sid, title = make_id(stem), make_title(stem)
        out = f"source/music/{sid}.adp"
        x = load(p); b = encode(x); open(out, "wb").write(b); total += len(b)
        y = decode(b); snr = 10 * np.log10(np.sum(x.astype(float) ** 2) / max(1, np.sum((x - y).astype(float) ** 2)))
        print(f"{title}: {len(x)/RATE:.0f}s  {len(b)/1024:.0f} KB  SNR {snr:.1f} dB  -> {out}")
        add_line(sid, title, out)
    print(f"total {total/1024:.0f} KB (the whole ROM can be 32 MB)")
