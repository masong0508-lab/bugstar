#!/bin/sh
# Regenerates source/itemrom.h (all item sprites, stored in ROM). Run after changing item art. Needs a C compiler: pkg install clang (Termux) or gcc.
cd "$(dirname "$0")" || exit 1
CC=${CC:-$(command -v cc || command -v gcc || command -v clang)}
[ -n "$CC" ] || { echo "no C compiler found (Termux: pkg install clang)"; exit 1; }
$CC -O1 -o /tmp/bake_items bake_items.c && /tmp/bake_items ../source/itemrom.h && echo "wrote source/itemrom.h"
