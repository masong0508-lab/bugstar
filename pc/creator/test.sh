#!/bin/bash
# builds and runs the planet creator's tests (recipe, seed, share code, files, undo, footprint) and the flora tests: pc/creator/test.sh
cd "$(dirname "$0")" || exit 1
C=../content
gcc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation -I. -o /tmp/cr_test cr_test.c cr_recipe.c cr_bio.c cr_addr.c cr_files.c $C/pc_content.c $C/flora.c -lm || exit 1
/tmp/cr_test || exit 1
gcc -O2 -Wall -Wextra -Wno-misleading-indentation -I$C -o /tmp/flora_test $C/flora_test.c $C/flora.c || exit 1
/tmp/flora_test
