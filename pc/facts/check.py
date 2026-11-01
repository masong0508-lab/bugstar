#!/usr/bin/env python3
# Checks the PC pages: python3 pc/facts/check.py [file.py ...]   (no files = all pc/facts/NN_*.py plus the GBA pages for duplicates)
# Rules: A-Z 0-9 space and . , ' : ( ) - ^ only; title <= 28 letters; lines <= 58; value is an int > 0 or a function giving digits;
# no two pages with the same title; no page with a title or a line-1 that is empty; about is 0 or 1.
import sys, os, glob, importlib.util
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE); sys.path.insert(0, ROOT)
OK = set(" ',():-.^0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ")
def load(path):
    spec = importlib.util.spec_from_file_location(os.path.basename(path)[:-3], path); m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m); return m.PAGES
def digits(v):
    if callable(v): return v()
    return str(v)
def main():
    files = sys.argv[1:] or sorted(glob.glob(os.path.join(HERE, "[0-9]*.py")))
    os.environ.pop("BUGSTAR_PC", None)
    import facts                                                    # the GBA pages (for duplicate titles and values)
    seen = {t: v for (v, t, a, b, ab) in facts.F}
    seen_vals = {}
    bad = 0; total = 0
    for f in files:
        pages = load(f); n = 0
        for i, e in enumerate(pages):
            if len(e) != 5: print("%s #%d: not a 5-tuple: %r" % (os.path.basename(f), i, e[:2])); bad += 1; continue
            v, t, a, b, ab = e; where = "%s #%d %s" % (os.path.basename(f), i, t)
            for nm, s, lim in (("title", t, 28), ("line1", a, 58), ("line2", b, 58)):
                if s is None:
                    if nm != "line2": print(where, "-", nm, "missing"); bad += 1
                    continue
                if not s.strip() and nm != "line2": print(where, "-", nm, "empty"); bad += 1
                if len(s) > lim: print(where, "-", nm, "is", len(s), "long (max %d):" % lim, s); bad += 1
                if set(s) - OK: print(where, "-", nm, "has letters the font lacks:", sorted(set(s) - OK), s); bad += 1
            if ab not in (0, 1): print(where, "- about must be 0 or 1"); bad += 1
            try:
                d = digits(v)
                if not d.isdigit() or int(d[:5]) == 0: print(where, "- value is not a positive number"); bad += 1
            except Exception as ex:
                print(where, "- value fails:", ex); bad += 1; continue
            if t in seen: print(where, "- title already used (%s)" % ("GBA page" if t in [x[1] for x in facts.F] else "another PC page")); bad += 1
            seen[t] = v; n += 1; total += 1
        print("%s: %d pages" % (os.path.basename(f), n))
    print("TOTAL %d pages, %d problems" % (total, bad)); return 1 if bad else 0
sys.exit(main())
