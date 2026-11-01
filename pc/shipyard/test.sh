#!/bin/bash
# the ship editor's own tests (recipe, share code, stats bounds, random-edit fuzz, files): pc/shipyard/test.sh
cd "$(dirname "$0")" || exit 1
gcc -O2 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-misleading-indentation -Wno-unused-result -fsanitize=address,undefined -fno-sanitize-recover=undefined -o /tmp/sy_test sy_test.c sy_design.c sy_files.c || exit 1
/tmp/sy_test
