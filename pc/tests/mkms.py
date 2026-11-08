#!/usr/bin/env python3
# mkms.py SYNC CLASS [BOARD_FRAMES]: the key script for tests/mission.sh (the hook BUGSTAR_WS_TEST=mplay in pc_missions.inc plays the rest). Times are relative to SYNC (a script frame):
#   +30 A (open the first job) +60 A (accept) +90 B +120 B (board closed) ... the scanner opens about +125: +230 D (second row = the job's craft) +260 A (look) +290 START (engage)
#   out: the job is done from outside, the hook shoots; A closes the cost breakdown.
#   in:  fly (B thrust, R brake), board (R held + A), the hook walks home when the objective is done; A closes the breakdown.   sab: the same, A held in the room plants the charge.
import sys
S = int(sys.argv[1]); cls = sys.argv[2]; bt = int(sys.argv[3]) if len(sys.argv) > 3 else 100
resume = cls.startswith("r-")            # r-out, r-in, r-sab: CONTINUE from a save that already has the job in its journal (one A at the title), no board keys
if resume: cls = cls[2:]; L = ["300 304 A"]
elif cls == "accept": L = ["300 304 A", "600 604 A", "900 904 A", "1200 1204 S", "1500 1504 S", "2000 2004 S", "2300 2304 A", "2700 2704 A"]
else: L = ["300 304 A", "600 604 A", "900 904 A", "1200 1204 S", "1500 1504 S", "2000 2004 S", "2300 2304 A", "2700 2704 A"]
def k(t, key, n=4): L.append("%d %d %s" % (S + t, S + t + n, key))
def shot(t): L.append("shot %d" % (S + t))
if not resume: k(30, "A"); shot(45); k(60, "A"); shot(75); k(90, "B"); k(120, "B")
if cls == "accept": print("\n".join(L)); sys.exit(0)
k(230, "D"); shot(245); k(260, "A"); shot(275); k(290, "S")
if cls == "out":
    shot(315); k(330, "A"); k(370, "A"); shot(420)
else:
    L.append("%d %d B" % (S + 320, S + 320 + bt)); L.append("%d %d R" % (S + 330 + bt, S + 700)); k(430 + bt, "A")
    shot(380); shot(480)
    if cls == "sab":
        L.append("%d %d A" % (S + 580 + bt, S + 700 + bt))             # hold A in the room: the charge goes in after 75 frames and blows 5 seconds later
        for t in (640, 720, 800, 900, 1000): shot(t + bt - 100)
        for t in (1020, 1060, 1100, 1140): k(t, "A")
    else:
        for t in (600, 680, 760, 840): shot(t + bt - 100)
        for t in (800, 840, 880, 920): k(t, "A")
print("\n".join(L))
