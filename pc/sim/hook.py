#!/usr/bin/env python3
# hook.py FILE ANCHOR NEW [APPEND]: in FILE replace the text ANCHOR (which must occur exactly once) by NEW.
# If APPEND is "after", NEW is added after the anchor instead. Fails loudly if the anchor is not found exactly once:
# that protects us when the game's code changes. Used by pc/build.sh on the copies in pc/gen only.
import sys
f, anchor, new = sys.argv[1], sys.argv[2], sys.argv[3]
mode = sys.argv[4] if len(sys.argv) > 4 else "replace"
s = open(f).read()
n = s.count(anchor)
if n != 1:
    sys.stderr.write("HOOK FAILED in %s: anchor found %d times (need exactly 1):\n  %s\n" % (f, n, anchor[:120]))
    sys.exit(1)
s = s.replace(anchor, anchor + new if mode == "after" else new)
open(f, "w").write(s)
