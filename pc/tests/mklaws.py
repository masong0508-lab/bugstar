# makes tests/laws.txt: a key script that opens the PC LAB, edits laws, uses a preset, searches, reads HOW THIS WORKS and tries CHEATS (see laws.sh)
import os
D=os.path.dirname(os.path.abspath(__file__))
base=open(D+'/worldfile.txt').read().split('\n')
out=[l for l in base if l and not l.startswith('shot') and int(l.split()[0])<=4900]
t=5000; shots=[]
def key(k,gap=24):
    global t
    out.append("%d %d %s"%(t,t+4,k)); t+=gap
def shot(name):
    global t
    out.append("shot %d"%(t+6)); shots.append((t+6,name)); t+=14
def rep(k,n,gap=10):
    for i in range(n): key(k,gap)
key('B',40); shot('hub')
key('D'); key('A',40); shot('presets')
rep('D',4); shot('pick'); key('A',30); shot('used')
key('R',30); rep('D',2); rep('r',6); shot('universe')
for i in range(50): out.append("%d %d r"%(t,t+1)); t+=2
t+=10; shot('held')
key('S',30); shot('menu'); key('D',16); key('A',30); shot('advanced')
key('S',30); key('D',16); key('D',16); key('A',30)            # search by word
rep('U',9); key('r',16); rep('D',2); key('r',16); rep('U',7); key('r',16); shot('typing'); key('A',30); shot('found')
key('B',30); key('B',40); shot('hub2')
key('D'); key('A',40); shot('how1')
for i in range(8): key('R',16); shot('how%d'%(i+2))
key('B',40); key('D'); key('A',40); shot('cheats_off')
key('D',16); key('A',40); shot('ask')                         # tries SET ANY LAW (greyed) while off
key('U',16); key('A',40); shot('confirm'); key('A',40); shot('cheats_on')
rep('D',2); rep('r',2); key('A',30); shot('size'); rep('D',1); rep('r',3); shot('speed')
key('B',40); key('B',40)
open(D+'/laws.txt','w').write('\n'.join(out)+'\n'); open(D+'/laws.shots','w').write('\n'.join('%d %s'%s for s in shots)+'\n'); print(t)
