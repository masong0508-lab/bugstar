#!/usr/bin/env python3
# fd.py ROM SAV NAME "<script lines>" F0 F1 [rows]   -> runs, shots every frame F0..F1, prints frames where the top `rows` rows differ from the previous frame, plus blank/total diffs
import sys,subprocess,os,glob
rom,sav,name,pre,f0,f1=sys.argv[1:7]; f0=int(f0); f1=int(f1)
d='/home/claude/tools/harness'; os.chdir(d)
sc='%s/out/fd_%s.txt'%(d,name)
open(sc,'w').write(pre.replace('\\n','\n')+'\n'+''.join('shot %d\n'%i for i in range(f0,f1+1)))
for f in glob.glob('out/fd_%s_*.ppm'%name): os.remove(f)
import shutil; s2='/tmp/claude-0/fd_%s.sav'%name; shutil.copy(sav,s2)
subprocess.run(['./run',rom,sc,'out/fd_'+name,s2],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
def load(f):
    b=open(f,'rb').read(); i=b.index(b'255\n')+4; return b[i:]
prev=None; fs=sorted(glob.glob('out/fd_%s_*.ppm'%name)); out=[]
for f in fs:
    im=load(f); n=int(f[-10:-4])
    black=im.count(0)==len(im)
    if prev is not None:
        top=im[:240*3*18]!=prev[:240*3*18]
        out.append((n,black,top,sum(1 for a,b in zip(im[::97],prev[::97]) if a!=b)))
    elif True: out.append((n,black,False,0))
    prev=im
for o in out: print(*o)
