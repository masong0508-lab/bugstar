# clip.py NAME "FLAGS|release" SCRIPTFILE [nsheet]  -> clips/NAME.rgb (+ .gba, sheet)
import sys, os, subprocess, shutil, numpy as np
from PIL import Image, ImageDraw
T=os.path.dirname(os.path.abspath(__file__)); C=os.path.join(T,'clips')
name, flags, script = sys.argv[1], sys.argv[2], sys.argv[3]; ns = int(sys.argv[4]) if len(sys.argv)>4 else 16
rom=os.path.join(C,name+'.gba')
if flags=='release': shutil.copy('/home/claude/bugstar/bugstar.gba',rom)
elif flags.startswith('rom:'): shutil.copy(os.path.join(C,flags[4:]+'.gba'),rom)
else:
    r=subprocess.run(['/home/claude/tools/build.sh',flags],capture_output=True,text=True)
    if 'error' in (r.stdout+r.stderr): print(r.stdout[-2000:], r.stderr[-2000:]); sys.exit(1)
    shutil.copy('/home/claude/tools/test.gba',rom)
sav=os.path.join(C,name+'.sav')
if os.path.exists(sav): os.remove(sav)
r=subprocess.run(['/home/claude/tools/harness/rec',rom,script,os.path.join(C,name),sav],capture_output=True,text=True)
print(r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'no output')
raw=os.path.join(C,name+'.rgb')
if os.path.exists(raw):
    a=np.fromfile(raw,dtype=np.uint8); n=a.size//(240*160*3); a=a[:n*240*160*3].reshape(n,160,240,3)
    idx=np.linspace(0,n-1,min(ns,n)).astype(int); cols=4; rows=(len(idx)+cols-1)//cols
    sh=Image.new('RGB',(cols*244,rows*174),(255,0,255)); d=ImageDraw.Draw(sh)
    for k,i in enumerate(idx):
        sh.paste(Image.fromarray(a[i]),((k%cols)*244,(k//cols)*174)); d.text(((k%cols)*244+2,(k//cols)*174+161),str(i),fill=(255,255,0))
    sh.save(os.path.join(C,name+'_sheet.png')); print(name, n, 'frames')
