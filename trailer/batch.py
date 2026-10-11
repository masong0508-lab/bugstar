# batch.py [names...]: build and record trailer clips (two at a time). Each clip: its own ROM, script, raw frames and a contact sheet.
import sys, os, subprocess, numpy as np
from concurrent.futures import ThreadPoolExecutor
from PIL import Image, ImageDraw
T=os.path.dirname(os.path.abspath(__file__)); C=os.path.join(T,'clips'); B='/home/claude/bugstar'
TC='/home/claude/tools/xpack-arm-none-eabi-gcc-13.3.1-1.1/bin'
ENV=dict(os.environ, PATH='/home/claude/tools/bin:'+TC+':'+os.environ['PATH'])
REC='/home/claude/tools/harness/rec'

NEW=['400 404 A','500 504 A','600 604 S','700 704 S']          # title -> slot -> bug maker -> hangar
CARDS=['900 904 A','1000 1004 A','1100 1104 A']                 # landing cards

def every(a,b,step,key,hold=4): return ['%d %d %s'%(f,f+hold,key) for f in range(a,b,step)]

J={}
def clip(name, flags, script, n=16): J[name]=(flags,script,n)

F='-DTEST_FILM '
# ---- planet surfaces: the start garden, then far worlds (weird traits at strength 22, and the world classes)
clip('garden', F+'-DTEST_ORBIT=2', NEW+CARDS+['rec 1060 1560 2'])
clip('gardenwalk', F, NEW+CARDS+['1100 1700 U','1300 1360 R','rec 1100 1600 2'])
W={'disco':890,'rainbow':881,'giants':884,'suns':899,'rings':900,'moons':912,'aurora':913,'meteors':888,'checker':902,
   'negative':910,'glitch':880,'mirror':889,'spikes':883,'ghosts':904,'tiny':882,'wobble':891,'terraces':919,'jungle':893}
for k,(nm,w) in enumerate(W.items()):
    clip('w_'+nm, F+'-DTEST_ORBIT=2 -DTEST_DEEP=%d -DTEST_WEIRD=%d'%(3+k,w), NEW+['900 904 A','rec 960 1400 2'])
PCN={'mountain':1,'ocean':3,'desert':4,'ice':5,'jungle':6,'death':7,'city':8,'paradise':9,'inferno':10,'brimstone':11,'obsidian':12,'bone':13,'bloodsea':14,'cocytus':15}
for k,(nm,c) in enumerate(PCN.items()):
    clip('c_'+nm, F+'-DTEST_ORBIT=2 -DTEST_DEEP=%d -DTEST_CLASS=%d'%(30+k,c), NEW+['900 904 A','rec 960 1400 2'])
# ---- dungeons
clip('dexp', F+'-DTEST_DUNGEON -DTEST_AIM=1400 -DTEST_AUTOWALK', NEW+CARDS+every(1300,3300,22,'A')+['rec 1250 3300 2'])
for z in (0,2,5):
    clip('boss%d'%z, F+'-DTEST_DUNGEON -DTEST_BOSS=%d -DTEST_BSTR=1 -DTEST_AIM=4000'%z, NEW+CARDS+every(1250,2600,18,'A')+['rec 1180 2600 2'])
# ---- space
clip('space', F+'-DTEST_SPACE', NEW+CARDS+['1100 1500 B','1180 1260 L','1400 1480 R','1500 1700 B','rec 1080 1800 2'])
clip('fleet', F+'-DTEST_FLEET=8 -DTEST_SAUTO=1', NEW+['900 904 A','rec 920 2400 2'])
clip('beast', F+'-DTEST_BEAST -DTEST_SAUTO=1', NEW+['900 904 A','1000 1004 A','1100 1104 A','1200 1204 A','rec 1000 2600 2'])
clip('beasthud', '-DTEST_BEAST -DTEST_SAUTO=1', NEW+['900 904 A','1000 1004 A','1100 1104 A','1200 1204 A','rec 1000 2600 2'])
# ---- makers: random bugs and ships (SELECT = surprise me)
clip('makers', F, ['400 404 A','500 504 A']+every(560,760,14,'s')+['780 784 S']+every(840,1080,14,'s')+['1100 1104 S','rec 520 1120 2'])
clip('human', F+'-DTEST_CC=1', NEW+['900 904 A','1000 1004 A']+every(1150,1500,16,'s')+['rec 1080 1520 2'])
clip('c_city2', F+'-DTEST_ORBIT=2 -DTEST_DEEP=36 -DTEST_CLASS=8', NEW+['900 904 A','1000 1004 A','1100 1104 A','rec 1100 1540 2'])
for z in (1,3,4,5):
    clip('zone%d'%z, F+'-DTEST_DUNGEON -DTEST_ZONE=%d -DTEST_AIM=1400 -DTEST_AUTOWALK'%z, NEW+CARDS+every(1300,2800,22,'A')+['rec 1250 2800 2'])
# ---- karts (the autopilot drives; no HUD)
clip('kart', F+'-DTEST_KART=2 -DTEST_KAUTO', NEW+CARDS+['rec 1060 2200 2'])
clip('kart2', F+'-DTEST_KART=5 -DTEST_KAUTO', NEW+CARDS+['rec 1060 2200 2'])
clip('kjump', F+'-DTEST_KART=3 -DTEST_KJUMP=0 -DTEST_KAUTO', NEW+CARDS+['rec 1060 2000 2'])
# ---- HOW BIG IS IT? (the real screens)
TB=['400 404 D','430 434 D','500 504 A']
clip('hb_screens', '-DTEST_UNLOCK -DTEST_SIZE=36', TB+['640 644 A','700 704 s','1000 1004 A','1060 1064 s','rec 540 1400 2'])
clip('hb_list', '-DTEST_UNLOCK -DTEST_SIZE=0', TB+['620 624 s','700 1000 D','rec 560 1060 2'])
clip('hb_stars', '-DTEST_UNLOCK -DTEST_SIZE=26', TB+['660 664 A','rec 540 900 2'])
clip('hb_beast', '-DTEST_UNLOCK -DTEST_SIZE=9', TB+['rec 540 760 2'])
clip('hb_unis', '-DTEST_UNLOCK -DTEST_SIZE=6', TB+['rec 540 760 2'])
# ---- the galaxy map, a universe jump, a blind warp
clip('unijump', '-DTEST_DEEP=1 -DTEST_WARP=8 -DTEST_UNI', NEW+['900 904 A','1000 1004 S','1060 1064 A','1120 1124 s','1180 1184 S','rec 940 2600 2'])
clip('unijumpf', F+'-DTEST_DEEP=1 -DTEST_WARP=8 -DTEST_UNI', NEW+['900 904 A','1000 1004 S','1060 1064 A','1120 1124 s','1180 1184 S','rec 1180 2600 2'])
clip('warp', F+'-DTEST_DEEP=2 -DTEST_WARP=8 -DTEST_UNI', NEW+['900 904 A','1000 1004 S','1060 1064 r','rec 1060 2400 2'])
clip('title', F, ['rec 280 440 2'])

def build(name, flags):
    elf=os.path.join(C,name+'.elf'); gba=os.path.join(C,name+'.gba')
    cmd=('arm-none-eabi-gcc -mcpu=arm7tdmi -mthumb -mthumb-interwork -O2 -Wall -ffreestanding -fno-strict-aliasing -DNO_LOGO %s -nostdlib -nostartfiles -T gba.ld crt0.s main.c logo.c -lgcc -o %s'%(flags,elf))
    r=subprocess.run(cmd,shell=True,cwd=B,env=ENV,capture_output=True,text=True)
    errs=[l for l in (r.stdout+r.stderr).splitlines() if 'error' in l]
    if errs: return 'BUILD ERROR '+name+': '+' | '.join(errs[:4])
    subprocess.run('arm-none-eabi-objcopy -O binary %s %s && gbafix %s -tBUGSTAR -cBUGS -mCL >/dev/null'%(elf,gba,gba),shell=True,cwd=B,env=ENV)
    return None

def sheet(name, n):
    raw=os.path.join(C,name+'.rgb'); a=np.memmap(raw,dtype=np.uint8,mode='r'); N=a.size//(240*160*3)
    if not N: return 0
    a=a[:N*240*160*3].reshape(N,160,240,3); idx=np.linspace(0,N-1,min(n,N)).astype(int); cols=4; rows=(len(idx)+cols-1)//cols
    sh=Image.new('RGB',(cols*244,rows*174),(255,0,255)); d=ImageDraw.Draw(sh)
    for k,i in enumerate(idx):
        sh.paste(Image.fromarray(np.array(a[i])),((k%cols)*244,(k//cols)*174)); d.text(((k%cols)*244+2,(k//cols)*174+161),str(i),fill=(255,255,0))
    sh.save(os.path.join(C,name+'_sheet.png')); return N

def run(name):
    flags,script,n=J[name]
    e=build(name,flags)
    if e: return e
    sp=os.path.join(C,name+'.txt'); open(sp,'w').write('\n'.join(script)+'\n')
    sav=os.path.join(C,name+'.sav')
    if os.path.exists(sav): os.remove(sav)
    r=subprocess.run([REC,os.path.join(C,name+'.gba'),sp,os.path.join(C,name),sav],capture_output=True,text=True)
    N=sheet(name,n); os.remove(os.path.join(C,name+'.elf'))
    return '%s: %d frames'%(name,N)

if __name__=='__main__':
    subprocess.run('python3 gen.py >/dev/null',shell=True,cwd=B,env=ENV)
    names=sys.argv[1:] or list(J)
    if names==['list']: print(' '.join(J)); sys.exit()
    with ThreadPoolExecutor(2) as ex:
        for r in ex.map(run,names): print(r, flush=True)
