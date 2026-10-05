#!/usr/bin/env python3
"""Turn mp3/wav sound effects into tiny 4-bit IMA-ADPCM blobs for the GBA (6554 Hz mono).
usage: python3 tools/encode_sfx.py name=path.mp3 [name=path.mp3 ...]   -> source/sfx/<name>.adp
Identical input files are only encoded once (the game points several effects at the same blob)."""
import sys, subprocess, struct, hashlib, os
import numpy as np
RATE = 6554   # = 16777216 / 2560, the GBA timer period used by the game
STEP = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767]
IDX = [-1,-1,-1,-1,2,4,6,8]

def load(path):
    af = ("silenceremove=start_periods=1:start_threshold=-50dB,areverse,"
          "silenceremove=start_periods=1:start_threshold=-45dB,areverse,"
          "loudnorm=I=-16:TP=-1.5,lowpass=f=3000,volume=-1dB")
    raw = subprocess.run(["ffmpeg","-v","error","-i",path,"-ac","1","-ar",str(RATE),"-af",af,"-f","s16le","-"],
                         capture_output=True, check=True).stdout
    x = np.frombuffer(raw, dtype="<i2").astype(np.int32)
    win = RATE // 10                                   # drop trailing quiet, keep 0.2 s tail, fade the last 0.1 s
    last = 0
    for i in range(0, len(x), win):
        seg = x[i:i+win].astype(np.float64)
        if np.sqrt(np.mean(seg**2)) > 1536: last = i + win
    x = x[:min(len(x), last + 2*win)].copy()
    f = min(win, len(x))
    x[-f:] = (x[-f:] * np.linspace(1, 0, f)).astype(np.int32)
    return x

def encode(x):
    pred, idx, out = 0, 0, []
    for s in x:
        step = STEP[idx]; d = int(s) - pred; n = 0
        if d < 0: n = 8; d = -d
        if d >= step: n |= 4; d -= step
        if d >= step >> 1: n |= 2; d -= step >> 1
        if d >= step >> 2: n |= 1
        diff = step >> 3
        if n & 1: diff += step >> 2
        if n & 2: diff += step >> 1
        if n & 4: diff += step
        pred += -diff if n & 8 else diff
        pred = max(-32768, min(32767, pred))
        idx = max(0, min(88, idx + IDX[n & 7]))
        out.append(n)
    if len(out) & 1: out.append(0)
    b = bytes((out[i] | (out[i+1] << 4)) for i in range(0, len(out), 2))
    b = struct.pack("<I", len(x)) + b
    return b + bytes((-len(b)) % 4)

def decode(b):
    n = struct.unpack("<I", b[:4])[0]; pred, idx, y = 0, 0, []
    for i in range(n):
        v = b[4 + (i >> 1)]; v = (v >> 4) if (i & 1) else (v & 15)
        step = STEP[idx]; diff = step >> 3
        if v & 1: diff += step >> 2
        if v & 2: diff += step >> 1
        if v & 4: diff += step
        pred += -diff if v & 8 else diff
        pred = max(-32768, min(32767, pred)); idx = max(0, min(88, idx + IDX[v & 7]))
        y.append(pred)
    return np.array(y)

if __name__ == "__main__":
    seen, total = {}, 0
    os.makedirs("source/sfx", exist_ok=True)
    for a in sys.argv[1:]:
        name, path = a.split("=", 1)
        h = hashlib.md5(open(path, "rb").read()).hexdigest()
        if h in seen: print(f"{name}: same file as {seen[h]} -> reuse {seen[h]}.adp"); continue
        seen[h] = name; x = load(path); b = encode(x); open(f"source/sfx/{name}.adp", "wb").write(b); total += len(b)
        y = decode(b); snr = 10*np.log10(np.sum(x.astype(float)**2)/max(1,np.sum((x-y).astype(float)**2)))
        print(f"{name}: {len(x)/RATE:.1f}s  {len(b)} bytes  SNR {snr:.1f} dB")
    print("total", total, "bytes")
