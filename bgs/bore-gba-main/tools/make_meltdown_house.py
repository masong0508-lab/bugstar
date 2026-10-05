#!/usr/bin/env python3
"""Builds tools/meltdown_in_mars_house.xm : a 90s house / techno rework of "Meltdown in Mars" (about 5:59).

usage:  python3 tools/make_meltdown_house.py        (run from the project root; needs numpy + scipy)

The notes (bass line G-Bb-F, the lead hooks, the chromatic riff, the breakdown chords Bb-C-Db-Eb) come from the original
tracker song; the sounds are all synthesised here (kick, clap, hats, bass, stabs, pads, leads, acid, risers) so the XM is
self-contained.  It only uses what the jukebox player supports: 10 channels, one-shot samples, note + volume column, no effects.
Channels (0-based): 0 kick | 1 clap / snare roll | 2 closed hat | 3 open hat | 4 bass | 5 chord stab | 6 pad | 7 lead | 8 acid / arp | 9 fx
126 BPM, 188 bars of 4/4 (47 four-bar phrases) + a short tail.  Re-running gives byte-identical output.
"""
import struct, os, sys
import numpy as np
from scipy import signal

SR = 16726          # sample rate of a "relative note +12" XM sample (plays at its natural pitch on note C-4)
SR2 = 8363 * 2 ** (5 / 12)    # relative note +5: 11163 Hz, for sounds with no energy above ~5 kHz (stabs, lead, crash) - 1/3 smaller than SR
SR3 = 8363 / 2                 # relative note -12: 4182 Hz, for the riser (a dark noise sweep)
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "meltdown_in_mars_house.xm")
rng = np.random.default_rng(1990)
BPM, SPEED, NCH, ROWS = 126, 6, 10, 64

# ---------------------------------------------------------------- synthesis helpers
def tt(n, sr=SR): return np.arange(n) / sr
def mid2f(m): return 440.0 * 2 ** ((m - 69) / 12)
def lp(x, fc, order=2, sr=SR): b, a = signal.butter(order, min(fc, sr * .45) / (sr / 2), 'low'); return signal.lfilter(b, a, x)
def hp(x, fc, order=2, sr=SR): b, a = signal.butter(order, fc / (sr / 2), 'high'); return signal.lfilter(b, a, x)
def bp(x, f1, f2, order=2, sr=SR): b, a = signal.butter(order, [f1 / (sr / 2), min(f2, sr * .45) / (sr / 2)], 'band'); return signal.lfilter(b, a, x)
def noise(n): return rng.uniform(-1, 1, n)
def saw(f, n, sr=SR, ph=0.0):
    t = tt(n, sr); y = np.zeros(n)
    for k in range(1, int(min(sr * .45 / f, 30)) + 1): y += np.sin(2 * np.pi * k * f * t + ph * k) / k
    return y * (2 / np.pi)
def square(f, n, sr=SR):
    t = tt(n, sr); y = np.zeros(n)
    for k in range(1, int(min(sr * .45 / f, 30)) + 1, 2): y += np.sin(2 * np.pi * k * f * t) / k
    return y * (4 / np.pi)
def supersaw(f, n, cents=(-14, -7, 0, 7, 14), sr=SR):
    return sum(saw(f * 2 ** (c / 1200), n, sr, ph=rng.uniform(0, 6.28)) for c in cents) / len(cents)
def tv_lp(x, fcs, q, sr=SR, blk=64):               # resonant low-pass with a moving cutoff (the acid filter sweep)
    y = np.zeros_like(x); zi = np.zeros(2)
    for i in range(0, len(x), blk):
        fc = min(max(fcs[i], 40), sr * .45); w0 = 2 * np.pi * fc / sr; al = np.sin(w0) / (2 * q); c = np.cos(w0)
        b = np.array([(1 - c) / 2, 1 - c, (1 - c) / 2]) / (1 + al); a = np.array([1, -2 * c / (1 + al), (1 - al) / (1 + al)])
        y[i:i + blk], zi = signal.lfilter(b, a, x[i:i + blk], zi=zi)
    return y
def finish(x, peak, tail=0.012, sr=SR):              # remove DC, normalise to the wanted loudness, tiny fade-out
    x = x - x.mean() * 0.0
    x = x / np.abs(x).max() * peak
    k = int(tail * sr); x[-k:] *= np.linspace(1, 0, k) ** 2
    return x

def chord_tones(f, minor, octave_root=True):
    iv = [0, 3 if minor else 4, 7] + ([-12] if octave_root else [])
    return [f * 2 ** (i / 12) for i in iv]

# ---------------------------------------------------------------- the instruments  (name, data, rel-note, gen_midi, svol)
I = {}
def add(k, name, x, gen=None, rel=12): I[k] = dict(name=name, x=x, gen=gen, rel=rel)

n = int(.30 * SR); t = tt(n)
ph = 2 * np.pi * np.cumsum(46 + 150 * np.exp(-t / 0.032)) / SR
k = np.sin(ph) * np.exp(-t / 0.115); k[:int(.003 * SR)] += lp(noise(int(.003 * SR)), 3000) * .5
add('kick', 'kick', finish(np.tanh(1.7 * k), .99))
n = int(.24 * SR); t = tt(n); c = np.zeros(n)
for off, dec, a in ((0, .005, .8), (.009, .005, .8), (.018, .005, .9), (.028, .06, 1.0)):
    i = int(off * SR); m = n - i; c[i:] += noise(m) * np.exp(-tt(m) / dec) * a
add('clap', 'clap', finish(bp(c, 1000, 3600, 2), .80))
n = int(.075 * SR); add('chat', 'closed hat', finish(hp(noise(n) * np.exp(-tt(n) / .016), 5200, 4), .50))
n = int(.30 * SR); add('ohat', 'open hat', finish(hp(noise(n) * np.exp(-tt(n) / .085), 4600, 4), .55))
n = int(.13 * SR); t = tt(n); add('snare', 'snare', finish(bp(noise(n), 1400, 5500, 2) * np.exp(-t / .03) + np.sin(2 * np.pi * 190 * t) * np.exp(-t / .03) * .7, .85))
# bass: rolling pluck (G2 = midi 43) and a long sine sub
n = int(.36 * SR); t = tt(n); f = mid2f(43)
b = saw(f, n) * .8 + square(f, n) * .5 + np.sin(2 * np.pi * f * t) * .9
b = tv_lp(b, 140 + 1100 * np.exp(-t / .07), 2.5); b = np.tanh(1.8 * b) * np.exp(-t / .26)
add('bass', 'house bass', finish(b, .95), 43)
n = int(1.1 * SR); t = tt(n); f = mid2f(43)
s = (np.sin(2 * np.pi * f * t) + .25 * np.sin(4 * np.pi * f * t)) * np.minimum(1, t / .01) * np.exp(-t / .75)
add('sub', 'sub bass', finish(s, .9), 43)
# chord stabs (root = C4, played transposed) and pads (root = C3)
def stab(minor):
    n = int(.42 * SR2); t = tt(n, SR2)
    x = sum(supersaw(f, n, (-9, 9), SR2) for f in chord_tones(mid2f(60), minor))
    x = tv_lp(x, 300 + 4200 * np.exp(-t / .09), 1.2, SR2) * np.minimum(1, t / .003) * np.exp(-t / .16)
    return finish(x, .75)
add('stab_m', 'chord stab m', stab(True), 60, rel=5); add('stab_M', 'chord stab M', stab(False), 60, rel=5)
def pad():                     # one pad for every chord: root + fifth + octaves (the thirds come from the stabs, arp and lead), so one sample instead of two
    n = int(2.4 * SR); t = tt(n)
    x = sum(supersaw(f, n, (-10, 0, 10)) for f in (mid2f(48), mid2f(55), mid2f(60), mid2f(36)))
    x = lp(x, 1250, 4) * (1 - np.exp(-t / .22)) * np.minimum(1, (n / SR - t) / .35) * (1 + .12 * np.sin(2 * np.pi * 4.2 * t))
    return finish(x, .70)
add('pad', 'pad', pad(), 48)
# leads (C5 = midi 72)
n = int(.60 * SR2); t = tt(n, SR2); f = mid2f(72)
x = supersaw(f, n, (-16, -8, 0, 8, 16), SR2) + .35 * square(f / 2, n, SR2)
x = tv_lp(x, 1400 + 3200 * np.exp(-t / .10), 1.3, SR2) * np.minimum(1, t / .004) * (.5 + .5 * np.exp(-t / .22)) * np.minimum(1, (n / SR2 - t) / .1)
add('lead', 'house lead', finish(x, .80), 72, rel=5)
n = int(.26 * SR); t = tt(n)
x = tv_lp(saw(f, n) + .5 * square(f, n), 600 + 3000 * np.exp(-t / .045), 2.0) * np.exp(-t / .09)
add('pluck', 'arp pluck', finish(x, .75), 72)
# acid (C3 = midi 48), three cutoff settings = the filter knob
f = mid2f(48)
for key, base, peak in (('acid0', 220, 800), ('acid1', 380, 1900), ('acid2', 650, 3600)):
    n = int(.28 * SR); t = tt(n)
    x = tv_lp(saw(f, n), base + peak * np.exp(-t / .075), 7.0) * np.minimum(1, t / .002) * np.exp(-t / .22)
    add(key, 'acid ' + key[-1], finish(np.tanh(1.5 * x), .85), 48)
# fx
n = int(.90 * SR2); add('crash', 'crash', finish(hp(noise(n), 3300, 3, SR2) * np.exp(-tt(n, SR2) / .36), .65), None, rel=5)
sr0 = SR3; n = int(3.8 * sr0); t = tt(n, sr0); fc = 180 * 2 ** (t / t[-1] * 3.3)
x = tv_lp(noise(n), fc, 1.8, sr0) * (t / t[-1]) ** 1.6
add('riser', 'riser', finish(x, .75), None, rel=-12)
n = int(1.0 * SR); t = tt(n); ph = 2 * np.pi * np.cumsum(30 + 70 * np.exp(-t / .12)) / SR
x = np.sin(ph) * np.exp(-t / .38) + lp(noise(n), 700) * np.exp(-t / .12) * .5
add('impact', 'impact', finish(np.tanh(1.4 * x), .95))
ORDER_KEYS = ['kick', 'clap', 'chat', 'ohat', 'bass', 'sub', 'stab_m', 'stab_M', 'pad', 'lead', 'pluck', 'acid0', 'acid1', 'acid2', 'crash', 'riser', 'snare', 'impact']
INST = {k: i + 1 for i, k in enumerate(ORDER_KEYS)}

# ---------------------------------------------------------------- music
NOTE = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}
def nm(s): return NOTE[s[:-1]] + 12 * (int(s[-1]) + 1)
def xmnote(m, key):                   # midi pitch -> XM note number for an instrument whose waveform sits at gen_midi
    g = I[key]['gen']; return 49 + (m - g) if g else 49
def near(pc, c): return min((pc + 12 * o for o in range(0, 9)), key=lambda m: abs(m - c))   # pitch class -> midi note nearest to c

PROG = {'A': [(0, 'G', 1), (16, 'G', 1), (32 - 8, 'F', 0), (16 + 0, 'Bb', 0)], 'B': []}
PROG['A'] = [(0, 'G', 1), (16, 'Bb', 0), (24, 'F', 0), (32, 'G', 1), (48, 'Bb', 0), (56, 'F', 0)]     # Gm | Bb F | Gm | Bb F
PROG['B'] = [(0, 'Bb', 0), (24, 'C', 0), (48, 'Db', 0), (56, 'Eb', 0)]                                # Bb ... C | C | Db Eb   (the original breakdown)
def chords(prog):                      # [(start_row, end_row, pc, minor)]
    p = PROG[prog]; return [(s, (p[i + 1][0] if i + 1 < len(p) else ROWS), NOTE[r], q) for i, (s, r, q) in enumerate(p)]

# lead lines (row -> note name); each is 32 rows = 2 bars
LINES = {
 'hook': {2: 'D5', 6: 'E5', 10: 'F5', 14: 'F5', 18: 'G5', 22: 'F5', 26: 'D#5', 30: 'C5'},                      # the verse tune (original pat 21), up an octave
 'hook2': {2: 'G5', 4: 'G5', 6: 'D6', 10: 'C6', 14: 'A#5', 18: 'C6', 20: 'C6', 22: 'F5', 24: 'F5', 27: 'G#5', 30: 'G5'},   # the shimmer hook (pat 10)
 'l1': {2: 'D5', 4: 'D5', 6: 'C5', 8: 'C5', 10: 'A#4', 12: 'C5', 14: 'D5', 16: 'F5', 20: 'F5', 24: 'F5', 27: 'D#5', 30: 'D5'},
 'l2': {2: 'G5', 4: 'G5', 6: 'F5', 8: 'F5', 10: 'E5', 12: 'E5', 14: 'D5', 16: 'C5', 19: 'D5', 22: 'E5', 24: 'F5', 25: 'D#5', 26: 'D5', 28: 'F5', 29: 'D#5', 30: 'D5'},
 'l3': {2: 'A#4', 14: 'G4', 15: 'F4', 16: 'G4', 18: 'A#4', 20: 'C5', 22: 'D5', 24: 'C5'},
 'l4': {2: 'G4', 14: 'D5', 15: 'C5', 16: 'D5', 18: 'C5', 19: 'A#4', 20: 'C5', 22: 'A#4', 23: 'A4', 24: 'A#4', 26: 'A4', 27: 'G4', 28: 'F4', 29: 'G4', 30: 'A#4', 31: 'D5'},
 'b1': {0: 'D5', 3: 'D5', 6: 'D5', 8: 'E5', 16: 'D5', 19: 'D5', 22: 'D5', 24: 'E5', 2: 'F5', 4: 'F5', 10: 'C#5', 12: 'C#5'},   # breakdown tunes (pat 8 / 9)
 'b2': {0: 'E5', 3: 'E5', 6: 'E5', 8: 'F#5', 16: 'G5', 19: 'G5', 22: 'G5', 24: 'A5', 2: 'G5', 4: 'G5', 6: 'F5', 10: 'D#5', 12: 'D#5', 14: 'F5', 18: 'G5', 22: 'A5'},
}
LINES['b1'] = {0: 'D5', 3: 'D5', 6: 'D5', 8: 'F5', 10: 'F5', 12: 'D5', 16: 'D5', 19: 'D5', 22: 'D5', 24: 'E5', 26: 'G5', 28: 'G5', 30: 'E5'}      # fitted to Bb | C
LINES['b2'] = {0: 'E5', 3: 'E5', 6: 'E5', 8: 'G5', 10: 'G5', 12: 'E5', 16: 'F5', 19: 'F5', 22: 'F5', 24: 'G5', 26: 'G5', 28: 'A#5', 30: 'G5'}    # fitted to C | Db | Eb
# the chromatic riff (original pat 10), moved down an octave and made to fit the chords: Bb Bb Bb Bb Bb C Db D | D . A . A Bb B C
RIFF = {0: 'A#3', 4: 'A#3', 8: 'A#3', 12: 'A#3', 13: 'A#3', 14: 'C4', 15: 'C#4', 16: 'D4', 20: 'D4', 24: 'A3', 28: 'A3', 29: 'A#3', 30: 'B3', 31: 'C4'}

def build(sp):
    """one 64-row phrase from a spec dict -> list of ROWS rows, each NCH cells (note, inst, vol)"""
    P = [[(0, 0, 0)] * NCH for _ in range(ROWS)]
    def put(r, ch, key, m, lvl):
        if 0 <= r < ROWS and lvl > 0: P[r][ch] = (xmnote(m, key) if m is not None else 49, INST[key], 0x10 + max(1, min(64, int(round(64 * lvl)))))
    g = lambda k, d=0: sp.get(k, d)
    ch = chords(g('prog', 'A'))
    last = ROWS - 16
    for r in range(ROWS):
        s = r % 16; fillbar = r >= last and g('fill')
        if g('kick') and not (g('kick') == 2 and r >= ROWS - 2): put(r, 0, 'kick', None, 1.0 if s == 0 else .95) if s % 4 == 0 else None
        if g('clap') and s in (4, 12) and not fillbar: put(r, 1, 'clap', None, .9)
        if g('chat') == 1 and s % 2 == 0: put(r, 2, 'chat', None, .75 if s % 4 == 2 else .45)
        if g('chat') == 2: put(r, 2, 'chat', None, (.8 if s % 4 == 2 else .55 if s % 2 == 0 else .3) if True else 0)
        if g('ohat') and s % 4 == 2: put(r, 3, 'ohat', None, .75)
    if g('fill'):                                                       # snare roll through the last bar, getting faster and louder
        for r in range(last, ROWS):
            q = r - last; step = 2 if q < 8 else 1
            if q % step == 0: put(r, 1, 'snare', None, .25 + .7 * q / 16)
    for (a, b, pc, minor) in ch:
        # bass
        root = near(pc, 45); bm = g('bass')
        if bm in ('off', 'roll'):
            for r in range(a, b):
                s = r % 16
                if bm == 'off':
                    if s in (2, 6, 10, 14): put(r, 4, 'bass', root, .95 if s != 14 else .8)
                    if s == 15 and g('bassfill'): put(r, 4, 'bass', root + 12, .6)
                else:
                    if s % 4 == 2: put(r, 4, 'bass', root, 1.0)
                    elif s % 4 == 1: put(r, 4, 'bass', root, .55)
                    elif s % 4 == 3: put(r, 4, 'bass', root + 12 if s in (7, 15) else root, .6)
        elif bm == 'sub': put(a, 4, 'sub', root, .9)
        # pad, stabs
        if g('pad'): put(a, 6, 'pad', near(pc, 55) if pc != 0 else 60, g('pad') * .55)
        if g('stab'):
            sm = near(pc, 62); pat = (0, 3, 6, 10) if g('stab') == 'A' else (2, 6, 10, 14)
            for r in range(a, b):
                if r % 16 in pat: put(r, 5, 'stab_m' if minor else 'stab_M', sm, .8 if r % 16 == pat[0] else .6)
    # acid: the riff on chord set A, a 303-style pattern on set B; the cutoff variant steps up through a section
    if g('acid') is not None:
        key = 'acid%d' % g('acid')
        for r in range(ROWS):
            s32 = r % 32
            if g('prog', 'A') == 'A':
                if s32 in RIFF: put(r, 8, key, nm(RIFF[s32]), .95 if s32 % 4 == 0 else .75)
            else:
                pc = [c for c in ch if c[0] <= r < c[1]][0][2]; rt = near(pc, 50)
                iv = {0: 0, 2: 0, 3: 12, 5: 0, 6: 7, 8: 0, 10: 0, 11: 12, 13: 10, 14: 7}.get(r % 16)
                if iv is not None: put(r, 8, key, rt + iv, .95 if r % 16 in (0, 6, 11) else .7)
    # arp on the acid channel (breakdowns / builds)
    if g('arp'):
        for (a, b, pc, minor) in ch:
            rt = near(pc, 62); tones = [0, 7, 12, 3 if minor else 4, 19, 12 + (3 if minor else 4)]
            seq = [0, 1, 2, 3, 2, 1, 4, 3, 2, 1, 0, 1, 2, 5, 4, 3]
            for r in range(a, b):
                if (r % 16) % (1 if g('arp') == 2 else 2) == 0: put(r, 8, 'pluck', rt + tones[seq[r % 16]], .55 if r % 4 else .7)
    # lead
    if g('lead'):
        for half, line in enumerate(g('lead')):
            if line is None: continue
            for r, name in LINES[line].items():
                put(r + 32 * half, 7, 'lead', nm(name), .8 if g('leadvol') is None else g('leadvol'))
    # fx
    if g('impact'): put(0, 9, 'impact', None, 1.0)
    elif g('crash'): put(0, 9, 'crash', None, .8)
    if g('riser') == 2: put(32, 9, 'riser', None, .8)
    elif g('riser') == 1: put(48, 9, 'riser', 49 + 12 - 49 + 49, .8); P[48][9] = (49 + 12, INST['riser'], 0x10 + 51)
    return P

def full(**kw):
    d = dict(kick=1, clap=1, chat=2, ohat=1, bass='off', stab='A', pad=1, acid=None, lead=None, prog='A'); d.update(kw); return d
S = []
# --- intro: DJ-friendly build, 16 bars
S += [dict(kick=1, chat=1, crash=0),
      dict(kick=1, chat=2, ohat=1, bass='off'),
      dict(kick=1, clap=1, chat=2, ohat=1, bass='off', pad=.6, acid=0),
      dict(kick=1, clap=1, chat=2, ohat=1, bass='off', pad=.8, acid=1, stab='B', fill=1, riser=1, kick_=0)]
# --- A1: first groove, 16 bars (the verse tune arrives on the 2nd phrase)
S += [full(bass='roll', acid=1, impact=1), full(bass='roll', acid=1, lead=('hook', 'hook')), full(bass='off', acid=1, stab='B', lead=('hook', 'l1')), full(bass='roll', acid=2, lead=('hook', 'l1'), fill=1, riser=2)]
# --- A2: the shimmer hook, 16 bars
S += [full(bass='roll', acid=2, lead=('hook2', 'hook2'), crash=1), full(bass='roll', acid=2, lead=('hook2', 'l2')), full(bass='off', acid=1, stab='B', lead=('l2', 'l1'), bassfill=1), full(bass='roll', acid=2, lead=('hook2', 'l2'), fill=1)]
# --- breakdown A: 12 bars, no kick, the original breakdown chords
S += [dict(prog='B', pad=1, bass='sub', arp=1, lead=('b1', 'b2'), leadvol=.7, crash=1),
      dict(prog='B', pad=1, bass='sub', arp=2, lead=('b2', 'b1'), leadvol=.75, chat=1),
      dict(prog='B', pad=1, bass='sub', arp=2, kick=2, fill=1, chat=1, stab='B', riser=2, leadvol=.7, lead=('b2', None))]
# --- B1/B2: second big drop (acid 303 pattern in the key of the breakdown first, then the main progression), 32 bars
S += [full(prog='B', bass='roll', acid=2, lead=('b1', 'b2'), impact=1), full(prog='B', bass='roll', acid=2, lead=('b2', 'b1')),
      full(bass='roll', acid=1, lead=('hook', 'hook2')), full(bass='off', acid=2, lead=('l1', 'l2'), stab='B', bassfill=1),
      full(bass='roll', acid=2, lead=('hook2', 'hook2'), crash=1), full(bass='roll', acid=1, lead=('l2', 'l2')),
      full(bass='off', acid=2, lead=('hook', 'l1'), stab='B'), full(bass='roll', acid=2, lead=('hook2', 'l2'), fill=1)]
# --- bridge: 8 bars, stripped to the groove + acid
S += [dict(kick=1, chat=2, ohat=1, bass='roll', acid=0, pad=.5, crash=1), dict(kick=1, clap=1, chat=2, ohat=1, bass='roll', acid=2, stab='B', fill=1, riser=2)]
# --- breakdown B: 12 bars, long build
S += [dict(prog='B', pad=1, bass='sub', arp=2, lead=('l3', 'l4'), leadvol=.7, crash=1, prog_='A'),
      dict(prog='B', pad=1, bass='sub', arp=2, lead=('b1', 'b2'), leadvol=.75, chat=1, ohat=1),
      dict(prog='B', pad=1, bass='sub', arp=2, kick=2, chat=2, ohat=1, clap=1, stab='A', fill=1, riser=2, leadvol=.8, lead=('b2', 'b2'))]
# --- C1/C2: the peak, 32 bars
S += [full(bass='roll', acid=2, lead=('hook2', 'hook'), impact=1), full(bass='roll', acid=2, lead=('hook2', 'l2'), stab='B'),
      full(bass='roll', acid=1, lead=('l2', 'l1')), full(bass='off', acid=2, lead=('hook', 'hook2'), stab='B', bassfill=1),
      full(prog='B', bass='roll', acid=2, lead=('b1', 'b2'), crash=1), full(prog='B', bass='roll', acid=1, lead=('b2', 'b1')),
      full(bass='roll', acid=2, lead=('hook2', 'l2')), full(bass='roll', acid=2, lead=('hook2', 'hook2'), fill=1)]
# --- breakdown C: 8 bars, short
S += [dict(prog='B', pad=1, bass='sub', arp=2, lead=('b1', 'b2'), leadvol=.8, crash=1, chat=1), dict(prog='B', pad=1, bass='sub', arp=2, kick=2, clap=1, chat=2, ohat=1, stab='A', fill=1, riser=2, lead=('b2', 'b1'), leadvol=.8)]
# --- final drop: 20 bars
S += [full(bass='roll', acid=2, lead=('hook2', 'hook2'), impact=1), full(bass='roll', acid=2, lead=('hook', 'l2'), stab='B'), full(bass='roll', acid=1, lead=('l2', 'hook2')),
      full(bass='off', acid=2, lead=('l1', 'l2'), stab='B', bassfill=1), full(bass='roll', acid=2, lead=('hook2', 'hook2'), crash=1)]
# --- outro: strip it back, 16 bars
S += [full(bass='roll', acid=2, stab='B', pad=.8, crash=1), dict(kick=1, clap=1, chat=2, ohat=1, bass='off', acid=1, pad=.5),
      dict(kick=1, chat=1, ohat=1, bass='off', acid=0), dict(kick=1, chat=1)]
assert len(S) == 47, len(S)

# ---------------------------------------------------------------- XM writer
def pat_bytes(P):
    b = bytearray()
    for row in P:
        for (n, i, v) in row: b += bytes([n, i, v, 0, 0])
    return struct.pack('<IBHH', 9, 0, len(P), len(b)) + bytes(b)
def inst_bytes(k):
    d = I[k]; q = np.clip(np.round(d['x'] * 127), -127, 127).astype(int)
    delta = np.diff(np.concatenate([[0], q])); dl = bytes((int(v) & 255) for v in delta)
    h = struct.pack('<I', 263) + d['name'].encode('latin1')[:22].ljust(22, b'\0') + b'\0' + struct.pack('<HI', 1, 40) + bytes(96) + bytes(48) + bytes(48)
    h += bytes(14) + struct.pack('<H', 0) + bytes(263 - len(h) - 14 - 2)
    sh = struct.pack('<IIIBbBBbB', len(q), 0, 0, 64, 0, 0, 128, d['rel'], 0) + b'\0' * 22
    assert len(h) == 263 and len(sh) == 40
    return h + sh + dl

def main():
    pats, order, seen = [], [], {}
    for sp in S:
        P = build(sp); key = pat_bytes(P)
        if key not in seen: seen[key] = len(pats); pats.append(P)
        order.append(seen[key])
    tail = [[(0, 0, 0)] * NCH for _ in range(8)]; pats.append(tail); order.append(len(pats) - 1)     # short silent tail so the last hit rings out
    hdr = b'Extended Module: ' + b'Meltdown in Mars (house)'[:20].ljust(20) + b'\x1a' + b'make_meltdown_house'.ljust(20, b'\0')[:20] + struct.pack('<H', 0x0104)
    hdr += struct.pack('<I', 276) + struct.pack('<8H', len(order), 0, NCH, len(pats), len(ORDER_KEYS), 1, SPEED, BPM) + bytes(order).ljust(256, b'\0')
    body = b''.join(pat_bytes(P) for P in pats) + b''.join(inst_bytes(k) for k in ORDER_KEYS)
    open(OUT, 'wb').write(hdr + body)
    rows = sum(len(pats[o]) for o in order); sec = rows * SPEED * 2.5 / BPM
    print("wrote %s: %d orders, %d patterns, %.1f s (%d:%02d), %d KB" % (OUT, len(order), len(pats), sec, sec // 60, sec % 60, os.path.getsize(OUT) // 1024))

if __name__ == '__main__': main()
