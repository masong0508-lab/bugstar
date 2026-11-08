#!/usr/bin/env python3
# tr.py ROM SAV NAME F0 F1 "<script lines with \n>" [sheetframes comma list]
# runs the script, single-steps frames F0..F1 (tear detector), grabs every frame, reports: torn frames (display memory changed while the beam drew), isolated blank frames, long blanks
import sys,subprocess,os,glob,shutil
rom,sav,name,f0,f1,pre=sys.argv[1:7]; f0=int(f0); f1=int(f1); sheet=sys.argv[7].split(',') if len(sys.argv)>7 and sys.argv[7] else []
d='/home/claude/tools/harness'; os.chdir(d)
sc='tr/%s.txt'%name
import re
elf=rom[:-4]+'.elf'
nm=subprocess.run(['/home/claude/tools/xpack-arm-none-eabi-gcc-13.3.1-1.1/bin/arm-none-eabi-nm',elf],capture_output=True,text=True).stdout
syms={l.split()[2]:int(l.split()[0],16) for l in nm.splitlines() if len(l.split())==3}
def sub(m):
    n=m.group(1); off=int(m.group(2) or 0); return '%08x'%(syms[n]+off)
pre=re.sub(r'@(\w+)(?:\+(\d+))?',sub,pre)
open(sc,'w').write(pre.replace('\\n','\n')+'\nwatch %d %d\nshots %d %d\n'%(f0,f1,f0,f1))
for f in glob.glob('tr/%s_*.ppm'%name): os.remove(f)
s2='/tmp/claude-0/tr_%s.sav'%name; shutil.copy(sav,s2)
r=subprocess.run(['./runs',rom,sc,'tr/'+name,s2],capture_output=True,text=True)
torn=[];ctrl=[];oam=0;palc=[]
for l in r.stdout.splitlines():
    if not l.startswith('W '): continue
    p=l.split(); fr=int(p[2]); vr=int(p[4]); rows=p[6]; fv=int(p[8]); ct=int(p[11]); rg=p[13]; pal=int(p[16]); om=int(p[19])
    if vr: torn.append((fr,vr,rows,fv))
    if ct: ctrl.append((fr,ct,rg))
    if pal: palc.append(fr)
    oam+=om
def load(f):
    b=open(f,'rb').read(); i=b.index(b'255\n')+4; return b[i:]
fs=sorted(glob.glob('tr/%s_*.ppm'%name)); blank=[]
for f in fs:
    im=load(f); n=int(f[-10:-4]); sub=im[::53]
    top=max(set(sub),key=sub.count) if False else None
    from collections import Counter
    c=Counter(sub).most_common(1)[0][1]/len(sub); blank.append((n,c))
iso=[];runs=[];cur=[]
for n,c in blank:
    if c>0.985: cur.append(n)
    else:
        if cur: runs.append(cur); cur=[]
if cur: runs.append(cur)
print('%s: frames %d..%d  torn-frames %d  ctrl-changes-while-drawing %d  palette-writes-while-drawing %d  blank-runs %s'%(name,f0,f1,len(torn),len(ctrl),len(palc),[(r[0],len(r)) for r in runs]))
for t in torn[:8]: print('   TORN frame %s lines-changed %s rows %s first-beam-line %s'%t)
for t in ctrl[:8]: print('   CTRL frame %s writes %s regs-mask %s'%t)
if sheet:
    from PIL import Image
    ims=[Image.open('tr/%s_%06d.ppm'%(name,int(x))) for x in sheet]; cols=4; rows=(len(ims)+cols-1)//cols
    S=Image.new('RGB',(cols*244,rows*164),(255,0,255))
    for i,im in enumerate(ims): S.paste(im,((i%cols)*244,(i//cols)*164))
    S.save('/tmp/claude-0/%s_sheet.png'%name)
