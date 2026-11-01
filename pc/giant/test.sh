#!/bin/bash
# pc/giant/test.sh [quick|count|pow|v3|all]: builds giant_test and runs the tests against the independent Python reference (ref.py).
#   live   2^(2^32) worked out on the spot vs the reference digits (GIANT_FULL=1: also the whole export and the FAST CACHE)
#   quick  names (all 1000 chunks, 300 big names, googolplex), English, SHA-256, decimal strings      (seconds)
#   count  the counting number: 3,027 windows, the whole 1 GB export's size and SHA-256               (about 2 minutes)
#   pow    2^(2^32): needs the file (BUGSTAR_GIANT_DIR or pc/giant/cache, made by mkpow.py)            (about 10 minutes, 3 GB of RAM)
#   v3     BUGSTARILLION v3: totals, term starts, windows, export                                     (about 10 minutes, 3 GB of RAM)
cd "$(dirname "$0")"; W=${1:-quick}
BIN=${TMPDIR:-/tmp}/giant_test_$$; gcc -O2 -w -o $BIN giant_test.c giant.c -lpthread || exit 1
if [ -z "$BUGSTAR_GIANT_DIR" ] && [ -f cache/pow2_2e32.bcd ]; then export BUGSTAR_GIANT_DIR=$(pwd)/cache; fi
export BIN TMPD=${TMPDIR:-/tmp} POWFILE=${BUGSTAR_GIANT_DIR:+$BUGSTAR_GIANT_DIR/pow2_2e32.bcd}
python3 test_giant.py $W; rc=$?; rm -f $BIN; exit $rc
