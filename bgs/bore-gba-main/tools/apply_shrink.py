#!/usr/bin/env python3
"""One-time patch for tools/xm2gba.py that makes the ROM smaller (safe to run twice):
  1. every sample's inaudible tail (trailing values of -1/0/+1 after the fade) is cut off
  2. identical samples are stored ONCE for the whole ROM, even when several songs use the same instrument
usage: python3 tools/apply_shrink.py   (from the project root), then python3 tools/xm2gba.py"""
import os, sys
p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "xm2gba.py")
s = open(p).read()
edits = [
 ("        q = np.append(q, 0)                                       # guard sample for interpolation\n",
  "        k = len(q)\n        while k > 16 and abs(q[k - 1]) <= 1: k -= 1               # cut the inaudible tail (values -1..1 after the fade)\n        q = np.append(q[:k], 0)                                   # guard sample for interpolation\n"),
 ('SONGS_H = "source/songs.h"\n',
  'SONGS_H = "source/songs.h"\nSHARED = {}                       # sample data already written for an earlier song: identical samples are stored once in the whole ROM\n'),
 ("    seen = {}; names = []\n", "    seen = SHARED; names = []\n"),
]
if "SHARED = {}" in s and "cut the inaudible tail" in s and "seen = SHARED" in s:
    print("already patched"); sys.exit(0)
for a, b in edits:
    if s.count(a) != 1: sys.exit("ERROR: could not find the expected text in xm2gba.py:\n" + a)
    s = s.replace(a, b)
open(p, "w").write(s); print("xm2gba.py patched")
