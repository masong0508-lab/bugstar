#!/usr/bin/env python3
"""Verify Bugstar's actual kart HUD DMA palette packing under UBSan.

The hardware row DMA needs a 32-bit word of four identical 8-bit palette
indices. A signed shift by 24 may overflow for indices >= 128. Compile the
real game's helper and check its output, including negative and wide inputs.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "kart.inc").read_text()

# Ensure the live kart HUD painter is using the tested helper.
if not re.search(r"volatile\s+u32\s+v\s*=\s*krect_dma_word\s*\(\s*c\s*\)\s*;", source):
    raise SystemExit("kart HUD DMA fill is no longer wired to krect_dma_word")

match = re.search(r"static\s+u32\s+krect_dma_word\s*\(\s*int\s+c\s*\)\s*\{", source)
if not match:
    raise SystemExit("Cannot locate kart DMA palette helper")
start = match.start()
opening = source.index("{", match.start())
depth = 0
closing = None
for i in range(opening, len(source)):
    if source[i] == "{":
        depth += 1
    elif source[i] == "}":
        depth -= 1
        if depth == 0:
            closing = i + 1
            break
if closing is None:
    raise SystemExit("Incomplete kart DMA palette helper")
actual = source[start:closing]

program = r'''
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
typedef uint8_t u8;
typedef uint32_t u32;
''' + actual + r'''
static int check(int c) {
    u32 b = (u32)(uint8_t)c;
    u32 expected = b | (b << 8) | (b << 16) | (b << 24);
    u32 got = krect_dma_word(c);
    if (got != expected) {
        fprintf(stderr, "DMA palette word(%d)=%08x expected=%08x\n",
                c, (unsigned)got, (unsigned)expected);
        return 1;
    }
    return 0;
}
int main(void) {
    unsigned long checks = 0;
    static const int edge[] = {INT_MIN, INT_MIN + 1, -257, -256, -255,
        -129, -128, -1, 0, 1, 96, 127, 128, 129, 200, 254, 255, 256,
        257, 32767, 65535, INT_MAX - 1, INT_MAX};
    for (unsigned i = 0; i < sizeof(edge) / sizeof(edge[0]); ++i) {
        if (check(edge[i])) return 1;
        ++checks;
    }
    for (int c = -512; c <= 511; ++c) {
        if (check(c)) return 1;
        ++checks;
    }
    uint32_t state = 0x7919fa65u;
    for (int i = 0; i < 20000; ++i) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        if (check((int)(int32_t)state)) return 1;
        ++checks;
    }
    printf("Kart DMA palette packing: %lu UBSan cases PASS\n", checks);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="bugstar-kdma-") as td:
    exe = Path(td) / "dma-palette-test"
    subprocess.run(
        ["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=undefined", "-fno-sanitize-recover=all",
         "-x", "c", "-", "-o", str(exe)],
        input=program, text=True, check=True,
    )
    subprocess.run([str(exe)], check=True)
