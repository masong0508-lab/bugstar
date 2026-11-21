#!/usr/bin/env python3
"""Compile Bugstar's actual 64-bit division helper with UndefinedBehaviorSanitizer.

Checks signed-minimum division, a zero divisor, and deterministic arithmetic
cases without extra game buffers, emulator dependencies, or heap allocations.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'main.c').read_text()
SIGNATURE = 'static s64 sdiv(s64 n, u32 d)'
start = SOURCE.find(SIGNATURE)
if start < 0:
    raise SystemExit('Missing sdiv() in main.c')
opening = SOURCE.find('{', start + len(SIGNATURE))
if opening < 0:
    raise SystemExit('Missing sdiv() opening brace')
depth = 0
closing = None
for pos in range(opening, len(SOURCE)):
    if SOURCE[pos] == '{':
        depth += 1
    elif SOURCE[pos] == '}':
        depth -= 1
        if depth == 0:
            closing = pos + 1
            break
if closing is None:
    raise SystemExit('Missing sdiv() closing brace')
actual_function = SOURCE[start:closing]

harness = r'''
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;
#define S64_MAX 0x7FFFFFFFFFFFFFFFLL
''' + actual_function + r'''
static int check(s64 n, u32 d) {
    s64 expected = d == 0 ? 0 : n / (s64)d;
    s64 result = sdiv(n, d);
    if (expected != result) {
        fprintf(stderr, "sdiv(%lld,%u) = %lld; expected %lld\n", n, d, result, expected);
        return 1;
    }
    return 0;
}
int main(void) {
    static const s64 numbers[] = {
        LLONG_MIN, LLONG_MIN + 1, -(1LL << 62), -(1LL << 32), -1000000, -1,
        0, 1, 2, 1000000, (1LL << 32), (1LL << 62), LLONG_MAX - 1, LLONG_MAX
    };
    static const u32 divisors[] = {
        1, 0, 2, 3, 7, 255, 65535, 65536, 1000000, 0x7fffffffU, 0x80000000U, 0xffffffffU
    };
    unsigned long checks = 0;
    for (unsigned i = 0; i < sizeof(numbers)/sizeof(numbers[0]); ++i)
        for (unsigned j = 0; j < sizeof(divisors)/sizeof(divisors[0]); ++j) {
            if (check(numbers[i], divisors[j])) return 1;
            ++checks;
        }
    u64 state = 0x9e3779b97f4a7c15ULL;
    for (int i = 0; i < 20000; ++i) {
        state ^= state << 13; state ^= state >> 7; state ^= state << 17;
        s64 n = (s64)(state & (u64)LLONG_MAX);
        if (state & 1) n = -n;
        u32 d = (u32)(state >> 24);
        if (check(n, d)) return 1;
        ++checks;
    }
    printf("Shared signed division: %lu UBSan cases PASS\n", checks);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bugstar-math-') as td:
    binary = Path(td) / 'math-host'
    subprocess.run(['cc', '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-x', 'c', '-', '-o', str(binary)], input=harness, text=True, check=True)
    subprocess.run([str(binary)], check=True)
