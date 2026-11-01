#!/bin/bash
# builds and runs the content tests and the sampler: ./test.sh [sampler count]
cd "$(dirname "$0")" || exit 1
gcc -O2 -Wall -Wextra -Wno-misleading-indentation -o /tmp/pc_content_test content_test.c pc_content.c pcx.c flora.c ../meaning/meaning.c ../laws/laws.c ../sim/god.c ../sim/lv.c || exit 1
/tmp/pc_content_test
gcc -O2 -Wall -Wextra -o /tmp/pc_sampler sampler.c pc_content.c && /tmp/pc_sampler "${1:-0}"
