#!/usr/bin/env bash
# Real consecutive display blanks: a native frame must remain complete between
# 15 Hz flight updates. Use an existing binary; never rebuild shared pc/gen.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - "${1:-./bugstar_pc}" <<'PY'
from pathlib import Path
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import zlib

game = Path(sys.argv[1]).resolve()
out = Path(os.environ.get('PC_LIFETIME_OUT', 'out/native_space_lifetime')).resolve()
out.mkdir(parents=True, exist_ok=True)
scale = int(os.environ.get('PC_LIFETIME_SCALE', '2'))
assert scale >= 2
report = {'size': [240 * scale, 160 * scale], 'cases': {}, 'checks': []}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def png(ppm, dest):
    header, pixels = ppm.split(b'\n', 1)
    magic, w, h, maximum = header.split()
    w, h = int(w), int(h)
    assert magic == b'P6' and maximum == b'255' and len(pixels) == w * h * 3

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

    raw = b''.join(b'\0' + pixels[y*w*3:(y+1)*w*3] for y in range(h))
    dest.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def run(name, ships, view, threads, keys='', shots=range(282, 346), keep=(), end=None, transition=False):
    shots = list(shots)
    prefix = out / name
    for path in out.glob(name + '*.sav*'):
        path.unlink()
    for path in out.glob(name + '_*.ppm'):
        path.unlink()
    script = prefix.with_suffix('.keys')
    script.write_text(keys + ''.join(f'shot {frame}\n' for frame in shots))
    env = os.environ | {
        'BUGSTAR_LAN_BOOT': '1', 'BUGSTAR_SY_TEST': '9,0', 'BUGSTAR_SY_AT': '100',
        'BUGSTAR_SY_PIRATES': '0', 'BUGSTAR_SIDE': '0', 'PC_NATIVE': '2',
        'PC_SCALE': str(scale), 'PC_SHIPS': str(ships), 'PC_V3D': str(view),
        'PC_THREADS': str(threads), 'SR_DBG': '1',
    }
    # Keep a caller's sanitizer settings, but not unrelated render diagnostics.
    for key in ('PC_NBENCH', 'PC_SN_BENCH', 'PC_SN_DIRTY_LOG', 'PC_TIMING'):
        env.pop(key, None)
    log = prefix.with_suffix('.log')
    with log.open('w') as stream:
        subprocess.run([str(game), '--headless', '--script', str(script),
                        '--out', str(prefix), '--sav', str(prefix.with_suffix('.sav')),
                        '--frames', str(end if end is not None else max(shots) + 2)],
                       env=env, stdout=stream, stderr=stream, timeout=150, check=True)
    hashes = {}
    for frame in shots:
        path = out / f'{name}_{frame:06d}.ppm'
        data = path.read_bytes()
        hashes[frame] = digest(data)
        if frame in keep:
            png(data, path.with_suffix('.png'))
        path.unlink()  # CI retains compact logs, hashes and selected PNGs.
    rows = {}
    for page, tag, f0, f1, frame in re.findall(
        r'sr: page (\d+) tag (-?\d+) snapf (-?\d+)/(-?\d+) frame (\d+)', log.read_text()
    ):
        page, tag, f0, f1, frame = map(int, (page, tag, f0, f1, frame))
        rows[frame] = {'page': page, 'tag': tag, 'age': frame - (f1 if page else f0),
                       'newest_age': frame - max(f0, f1)}
    missing = set(shots) - set(rows)
    # The compositor intentionally skips the ship pass at >=75% black during
    # a scene fade. Observe all pixels, but require ship logs only outside it.
    assert not missing or (transition and all(1360 <= f < 1400 for f in missing)), \
        f'{name}: missing actual renderer observations {sorted(missing)}'
    report['cases'][name] = {'ships': ships, 'view': view, 'threads': threads,
                             'frames': hashes, 'renderer': rows,
                             'save': digest(prefix.with_suffix('.sav').read_bytes())}
    print(name + ': captured ' + str(len(shots)) + ' actual display blanks', flush=True)
    return hashes, rows


def complete(name, hashes, rows, start=282, end=346):
    for frame in range(start, end):
        row = rows[frame]
        assert 1 <= row['age'] <= 4, f'{name}: expired native snapshot at blank {frame}: {row}'
        assert row['page'] == row['tag'], f'{name}: bitmap and ship batch disagree at {frame}: {row}'
    for first in range(start, end, 4):
        group = [hashes[frame] for frame in range(first, min(first + 4, end))]
        assert len(set(group)) == 1, f'{name}: native picture changes between logic steps at {first}..{first+3}'
    report['checks'].append(name + ': all four phases use the same complete native frame')


def parity(a, b):
    assert report['cases'][a]['frames'] == report['cases'][b]['frames'], f'{a}/{b}: thread pixels differ'
    assert report['cases'][a]['save'] == report['cases'][b]['save'], f'{a}/{b}: thread gameplay differs'
    report['checks'].append(a + '/' + b + ': every pixel and save byte match')


motion = '260 323 r\n268 339 A\n'
for label, ships, view, keys in (
    ('native_idle', 1, 1, ''), ('native_motion', 1, 1, motion),
    ('cockpit_motion', 1, 1, '160 162 S\n' + motion),
    ('ultra_motion', 2, 3, motion), ('classic_motion', 0, 2, motion),
):
    for threads in (1, 4):
        name = f'{label}_{threads}'
        hashes, rows = run(name, ships, view, threads, keys, keep=(286,) if threads == 1 else ())
        if ships > 0:
            complete(name, hashes, rows)
        # Turning and firing must genuinely change the completed picture.
        assert len(set(hashes.values())) >= 8, f'{name}: did not exercise changing flight frames'
    parity(f'{label}_1', f'{label}_4')

# SELECT opens a real galaxy menu inside scene 2; B returns to flight. The
# existing stale guards must still retire native content while that menu owns
# the pages, and fresh native frames must resume immediately afterwards.
menu_keys = '280 282 s\n320 322 B\n'
menu_shots = range(278, 348)
for name, ships, threads in (('menu_native_1', 1, 1), ('menu_native_4', 1, 4), ('menu_classic_1', 0, 1)):
    hashes, rows = run(name, ships, 1, threads, menu_keys, shots=menu_shots,
                       keep=(308,) if name == 'menu_native_1' else ())
    if ships > 0:
        complete(name, hashes, rows, 328, 348)
parity('menu_native_1', 'menu_native_4')
for frame in range(286, 324):
    assert report['cases']['menu_native_1']['frames'][frame] == report['cases']['menu_classic_1']['frames'][frame], \
        f'native content leaked into galaxy menu at blank {frame}'
report['checks'].append('galaxy menu: 38 consecutive native/classic frames match, then coherent native flight resumes')

# A real trip through landing, its welcome card, walking to the parked ship,
# the shop and its START launch action. This exercises the initial-launch
# caller as well as a genuine scene exit, without a special rendering hook.
roundtrip_keys = ('100 790 BL\n800 804 A\n1000 1004 A\n1100 1104 A\n1200 1204 A\n'
                  '1220 1250 l\n1220 1232 U\n1280 1284 A\n1340 1344 S\n')
roundtrip_shots = list(range(778, 798)) + list(range(1112, 1116)) + list(range(1292, 1296)) + list(range(1360, 1416))
for threads in (1, 4):
    name = f'landing_launch_{threads}'
    hashes, rows = run(name, 1, 1, threads, roundtrip_keys, shots=roundtrip_shots,
                       keep=(1112, 1292, 1400) if threads == 1 else (), transition=True)
    complete(name, hashes, rows, 778, 798)
    complete(name, hashes, rows, 1400, 1416)
    assert all(rows[f]['age'] > 250 for f in range(1112, 1116)), name + ': did not leave the space scene'
    assert all(rows[f]['age'] > 250 for f in range(1292, 1296)), name + ': old space snapshot survived into the ship shop'
    # The first regular flight image after the game's fade-in must already be
    # selected at its first blank. The runtime companion separately observes
    # the initial-launch publication while that first picture is still black.
    first = next(f for f in range(1360, 1400) if f in rows and rows[f]['newest_age'] == 1)
    assert rows[first]['age'] == 1 and rows[first]['page'] == rows[first]['tag'], \
        f'{name}: launch return presented an older page at blank {first}'
    report['checks'].append(name + ': real landing/shop/launch, first regular flight blank ' + str(first))
parity('landing_launch_1', 'landing_launch_4')

summary = out / 'summary.json'
summary.write_text(json.dumps(report, indent=2) + '\n')
print('Native space lifetime: all display phases, chase/cockpit, turning/firing, ULTRA, CLASSIC, menus, landing/launch and 1/4-thread parity PASS')
print('Evidence: ' + str(summary))
PY
