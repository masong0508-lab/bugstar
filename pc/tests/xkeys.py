#!/usr/bin/env python3
# xkeys.py: send key presses to the X display (XTest), so the window can be driven without a keyboard. Test helper (Linux/X11 only).
#   xkeys.py Escape Down Down Return          each name is an X keysym name; "sleep:0.5" waits; "hold:Left:1.0" holds a key
import ctypes, sys, time, os
x11=ctypes.CDLL("libX11.so.6"); xt=ctypes.CDLL("libXtst.so.6")
x11.XOpenDisplay.restype=ctypes.c_void_p; x11.XKeysymToKeycode.restype=ctypes.c_ubyte; x11.XStringToKeysym.restype=ctypes.c_ulong
d=x11.XOpenDisplay(None)
if not d: sys.exit("no display")
def kc(n):
    k=x11.XStringToKeysym(n.encode()); c=x11.XKeysymToKeycode(ctypes.c_void_p(d),ctypes.c_ulong(k))
    if not c: sys.exit("unknown key "+n)
    return c
def key(n,down): xt.XTestFakeKeyEvent(ctypes.c_void_p(d),ctypes.c_uint(kc(n)),ctypes.c_int(down),ctypes.c_ulong(0)); x11.XFlush(ctypes.c_void_p(d))
for a in sys.argv[1:]:
    if a.startswith("sleep:"): time.sleep(float(a[6:])); continue
    if a.startswith("hold:"): _,n,t=a.split(":"); key(n,1); time.sleep(float(t)); key(n,0); continue
    key(a,1); time.sleep(0.08); key(a,0); time.sleep(0.15)
