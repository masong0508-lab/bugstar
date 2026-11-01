#!/bin/bash
# Seed test: the same address must make the same world, every time (facts + a hash of each rendered picture), and match tests/seeds.golden.
# (The golden file was made by this PC build. It guards against changes; it is not a comparison with a real GBA.)
cd "$(dirname "$0")/.."; mkdir -p out
run(){ ./bugstar_pc --view --scale 1 --out out/seedtest "$@"; echo; }
all(){
  run --universe 1 --star 7
  run --universe 1 --star 12 --planet 0 --moon 1
  run --universe 1 --cx 123456789ABCDEF --cy FEDCBA987654321 --star 9
  run --universe 2A --cx 100000000000000000000000000 --cy 3 --star 20 --planet 1
  run --universe 7FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF --cx 7FFFFFF00000000FFFFFFFFABCDEF --cy 5A5A5A5A5A5A --star 31 --planet 2 --moon 2
  run --universe DEADBEEF00000000CAFEBABE --cx 1 --cy 1 --star 6
}
all > out/seeds.a; all > out/seeds.b
cmp -s out/seeds.a out/seeds.b || { echo "seeds: DIFFERENT between two runs"; diff out/seeds.a out/seeds.b | head; exit 1; }
if [ "$1" = "--make-golden" ]; then cp out/seeds.a tests/seeds.golden; echo "golden written"; exit 0; fi
if cmp -s out/seeds.a tests/seeds.golden; then echo "seeds: SAME as golden, and SAME on two runs ($(grep -c '^SEEDVIEW' out/seeds.a) addresses)"; else echo "seeds: DIFFERENT from golden"; diff out/seeds.a tests/seeds.golden | head -20; exit 1; fi
