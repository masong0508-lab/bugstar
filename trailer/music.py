# music.py: the trailer's soundtrack, synthesized from scratch (numpy/scipy). Writes music.wav (stereo 44.1 kHz).
# It quotes the game's own chiptune themes (title fanfare, space arpeggio, boss riff, kart race tune) from sound.inc.
import numpy as np, scipy.signal as ss, scipy.io.wavfile as wavf, sys, os
from tl import *
SR=44100
rng=np.random.default_rng(7)
H=1; R=0          # hold, rest (as in sound.inc)
# ---- the game's tunes (64 steps each)
mel_title=[72,H,76,H,79,H,84,H, H,H,83,H,79,H,76,H, 77,H,81,H,84,H,89,H, H,H,88,H,84,H,81,H, 79,H,H,H,77,H,76,H, 74,H,H,H,72,H,74,H, 76,H,H,H,79,H,H,H, 72,H,H,H,H,H,R,R]
bas_title=[48,H,H,H,H,H,H,H, 52,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 57,H,H,H,H,H,H,H, 55,H,H,H,H,H,H,H, 53,H,H,H,H,H,H,H, 55,H,H,H,H,H,H,H, 48,H,H,H,H,H,H,H]
mel_space=[69,H,72,H,76,H,72,H, 69,H,72,H,76,H,79,H, 77,H,74,H,69,H,74,H, 77,H,H,H,76,H,74,H, 72,H,76,H,79,H,76,H, 72,H,76,H,79,H,84,H, 83,H,79,H,76,H,74,H, 76,H,H,H,H,H,R,R]
bas_space=[45,H,H,H,H,H,H,H, 45,H,H,H,52,H,H,H, 50,H,H,H,H,H,H,H, 50,H,H,H,57,H,H,H, 48,H,H,H,H,H,H,H, 48,H,H,H,55,H,H,H, 43,H,H,H,H,H,H,H, 40,H,H,H,47,H,H,H]
mel_boss=[64,H,67,R,64,H,71,R, 70,H,67,R,64,H,62,H, 64,H,67,R,64,H,72,R, 71,H,67,R,66,H,H,R, 76,H,74,H,72,H,71,H, 72,H,71,H,69,H,67,H, 69,H,71,H,72,H,74,H, 75,H,H,H,71,H,H,R]
bas_boss=[40,R,40,R,52,R,40,R, 40,R,40,R,52,R,40,R, 36,R,36,R,48,R,36,R, 35,R,35,R,47,R,35,R, 40,R,40,R,52,R,40,R, 38,R,38,R,50,R,38,R, 36,R,36,R,48,R,36,R, 35,R,35,R,47,R,47,R]
mel_race=[72,H,76,H,79,H,76,H, 74,H,77,H,81,H,77,H, 76,H,79,H,84,H,83,81, 79,H,H,H,R,R,79,81, 84,H,83,H,81,H,79,H, 77,H,76,H,74,H,72,H, 74,H,76,H,77,H,79,H, 76,H,74,H,72,H,H,R]
bas_race=[48,R,48,R,55,R,48,R, 50,R,50,R,57,R,50,R, 52,R,52,R,59,R,52,R, 55,R,55,R,43,R,55,R, 53,R,53,R,60,R,53,R, 52,R,52,R,59,R,52,R, 50,R,50,R,57,R,50,R, 55,R,55,R,43,R,47,R]
def notes(seq, step):          # (start, length, midi) from a game sequence, 'step' seconds per step
    out=[]; i=0
    while i<len(seq):
        if seq[i]>1:
            j=i+1
            while j<len(seq) and seq[j]==H: j+=1
            out.append((i*step,(j-i)*step,seq[i])); i=j
        else: i+=1
    return out
def f_of(m): return 440.0*2**((m-69)/12)
def n_(sec): return int(sec*SR)
def tt(n): return np.arange(n)/SR
# ---- building blocks
def env(n, a, d=0.0, s=1.0, r=0.1):   # attack, decay to sustain, release at the end (all seconds)
    e=np.ones(n); na=max(1,n_(a)); nd=max(1,n_(d)); nr=max(1,n_(r))
    e[:na]*=np.linspace(0,1,na)[:n]
    if nd>0 and s<1: k=min(n-na,nd);
    if d>0 and s<1 and n>na: k=min(n-na,nd); e[na:na+k]*=np.linspace(1,s,k); e[na+k:]*=s
    if n>nr: e[-nr:]*=np.linspace(1,0,nr)
    else: e*=np.linspace(1,0,n)
    return e
def saw(f, n, ph=0.0, drift=0.0):
    t=tt(n); fi = f*(1+drift*np.sin(2*np.pi*0.7*t)) if drift else f
    p=(np.cumsum(np.full(n,1.0)*fi)/SR+ph) % 1.0 if np.ndim(fi) else (f*t+ph)%1.0
    return 2*p-1
def square(f, n, duty=0.5): return np.where(((tt(n)*f)%1.0)<duty,1.0,-1.0)
def tri(f, n): p=(tt(n)*f)%1.0; return 4*np.abs(p-0.5)-1
def sine(f, n, ph=0.0): return np.sin(2*np.pi*(f*tt(n)+ph))
def noise(n): return rng.standard_normal(n)
def lp(x, fc, order=2):
    fc=min(fc,SR*0.45); sos=ss.butter(order,fc/(SR/2),'low',output='sos'); return ss.sosfilt(sos,x)
def hp(x, fc, order=2):
    fc=max(fc,20); sos=ss.butter(order,fc/(SR/2),'high',output='sos'); return ss.sosfilt(sos,x)
def bp(x, lo, hi, order=2):
    sos=ss.butter(order,[max(lo,20)/(SR/2),min(hi,SR*0.45)/(SR/2)],'band',output='sos'); return ss.sosfilt(sos,x)
def sweep_lp(x, f0, f1, curve=1.0, chunk=512):   # a low-pass whose cutoff slides from f0 to f1 over the signal
    n=len(x); out=np.empty(n); zi=None; k=0
    while k<n:
        e=min(n,k+chunk); u=(k/n)**curve; fc=f0*(f1/f0)**u
        sos=ss.butter(2,min(fc,SR*0.45)/(SR/2),'low',output='sos')
        if zi is None: zi=ss.sosfilt_zi(sos)*x[0]
        out[k:e],zi=ss.sosfilt(sos,x[k:e],zi=zi); k=e
    return out
def sweep_bp(x, f0, f1, q=1.0, chunk=512):
    n=len(x); out=np.empty(n); zi=None; k=0
    while k<n:
        e=min(n,k+chunk); u=k/n; fc=f0*(f1/f0)**u; lo=fc/(1+0.5/q); hi=fc*(1+0.5/q)
        sos=ss.butter(2,[max(lo,20)/(SR/2),min(hi,SR*0.45)/(SR/2)],'band',output='sos')
        if zi is None: zi=ss.sosfilt_zi(sos)*0
        out[k:e],zi=ss.sosfilt(sos,x[k:e],zi=zi); k=e
    return out
def dist(x, k=2.5): return np.tanh(x*k)/np.tanh(k)
def stereo(x, pan=0.0, width=0.0):   # mono -> (n,2); width adds a decorrelated copy
    l=x*min(1,1-pan); r=x*min(1,1+pan)
    if width: d=int(SR*0.011); r=r.copy(); r[d:]=r[d:]*(1-width)+x[:-d]*width*min(1,1+pan)
    return np.stack([l,r],1)
# ---- instruments (each returns a mono or stereo array)
def kick(dur=0.6, lowf=42, hif=190, punch=1.0):
    n=n_(dur); t=tt(n); f=lowf+(hif-lowf)*np.exp(-t*28); ph=np.cumsum(f)/SR
    x=np.sin(2*np.pi*ph)*np.exp(-t*5.5); x+=punch*0.5*hp(noise(n),2000)*np.exp(-t*300); return dist(x,1.6)
def taiko(dur=1.6, f0=50, f1=140):
    n=n_(dur); t=tt(n); f=f0+(f1-f0)*np.exp(-t*18); x=np.sin(2*np.pi*np.cumsum(f)/SR)*np.exp(-t*3.2)
    x+=lp(noise(n),300)*np.exp(-t*20)*1.4; x+=bp(noise(n),800,3000)*np.exp(-t*90)*0.5; return dist(x,2.0)
def snare(dur=0.25):
    n=n_(dur); t=tt(n); x=bp(noise(n),900,6500)*np.exp(-t*22)+sine(185,n)*np.exp(-t*40)*0.8; return dist(x,1.5)
def clap(dur=0.35):
    n=n_(dur); t=tt(n); x=np.zeros(n)
    for k in range(4): d=n_(0.011*k); x[d:]+=bp(noise(n-d),1200,5000)*np.exp(-tt(n-d)*(60 if k<3 else 14))*(0.7 if k<3 else 1.0)
    return x
def hat(dur=0.06, open_=False):
    n=n_(0.35 if open_ else dur); t=tt(n); return hp(noise(n),8500)*np.exp(-t*(9 if open_ else 70))
def crash(dur=2.8):
    n=n_(dur); t=tt(n); return hp(noise(n),4500)*np.exp(-t*1.6)*0.8+bp(noise(n),2500,9000)*np.exp(-t*4)*0.4
def ride(dur=0.5):
    n=n_(dur); t=tt(n); return bp(noise(n),4000,10000)*np.exp(-t*9)*0.5
def braam(f, dur, dark=False):
    n=n_(dur); x=np.zeros(n)
    for dt in (-0.012,-0.006,0,0.006,0.012): x+=saw(f*(1+dt),n,rng.random())
    x+=1.2*saw(f/2,n,0.3)+0.8*saw(f/2*1.005,n,0.7)+1.5*sine(f/2,n)
    x=sweep_lp(x,(120 if dark else 220),(1400 if dark else 3200),0.6)
    x=dist(x,3.0)*env(n,0.18,0,1,min(0.8,dur*0.4)); return x/6
def sub(f, dur):
    n=n_(dur); return sine(f,n)*env(n,0.01,0,1,0.1)
def strings(f, dur, bright=3200, a=0.03, r=0.12, drift=0.004):
    n=n_(dur); x=np.zeros(n)
    for dt,ph in ((-0.004,0.1),(0.0,0.5),(0.004,0.8)): x+=saw(f*(1+dt),n,ph,drift)
    x=lp(x,bright)*env(n,a,0,1,r); return x/3
def choir(f, dur, a=0.5, r=0.6):
    n=n_(dur); t=tt(n); x=np.zeros(n); amp=[1,0.45,0.3,0.28,0.18,0.1,0.07,0.05]
    for v,(dt,vr) in enumerate(((-0.006,4.7),(0.0,5.3),(0.006,5.9))):
        fv=f*(1+dt)*(1+0.003*np.sin(2*np.pi*vr*t+v)); ph=np.cumsum(fv)/SR
        for k,a_ in enumerate(amp): x+=a_*np.sin(2*np.pi*ph*(k+1)+v*0.7)
    form=bp(x,600,1100)*1.4+bp(x,1800,2600)*0.6+lp(x,900)*0.9
    return form*env(n,a,0,1,r)/4
def brass(f, dur, a=0.04, r=0.15):
    n=n_(dur); t=tt(n); vib=1+0.004*np.sin(2*np.pi*5.5*t)*np.clip((t-0.15)*4,0,1)
    ph=np.cumsum(f*vib)/SR; x=(2*(ph%1.0)-1)*0.8+np.where((ph%1.0)<0.5,1,-1)*0.3+(2*((ph*1.003)%1.0)-1)*0.5
    x=sweep_lp(x,700,4500,0.35); return dist(x,1.8)*env(n,a,0.08,0.85,r)/1.6
def pluck(f, dur=1.8):     # a celesta / music box
    n=n_(dur); t=tt(n); x=sine(f,n)*np.exp(-t*2.2)+0.5*sine(f*2,n)*np.exp(-t*5)+0.25*sine(f*3,n)*np.exp(-t*9)+0.15*sine(f*4.01,n)*np.exp(-t*14)
    x+=0.3*hp(noise(n),3000)*np.exp(-t*600); return x*env(n,0.002,0,1,0.05)
def glock(f, dur=1.4):
    n=n_(dur); t=tt(n); return (sine(f,n)+0.4*sine(f*2.76,n)*np.exp(-t*7)+0.2*sine(f*5.4,n)*np.exp(-t*12))*np.exp(-t*3)*env(n,0.001,0,1,0.02)
def chip_sq(f, dur, duty=0.5):
    n=n_(dur); return square(f,n,duty)*env(n,0.002,0.05,0.8,0.01)
def chip_tri(f, dur):
    n=n_(dur); x=tri(f,n); x=np.round(x*7.5)/7.5; return x*env(n,0.002,0,1,0.01)   # 4-bit steps, like the GBA's wave channel
def chip_noise(dur, hi=True):
    n=n_(dur); t=tt(n); x=np.sign(noise(n)); x=x[::2].repeat(2)[:n] if hi else x[::6].repeat(6)[:n]; return x*np.exp(-t*(40 if hi else 14))
def chip_kick(dur=0.2):
    n=n_(dur); t=tt(n); f=200*np.exp(-t*30)+45; return square(1,n)*0+np.sign(np.sin(2*np.pi*np.cumsum(f)/SR))*np.exp(-t*18)
def riser(dur, f0=250, f1=9000):
    n=n_(dur); t=tt(n); x=sweep_bp(noise(n),f0,f1,0.8)*(t/dur)**1.6*3
    x+=0.5*sweep_lp(saw(55,n)+saw(55.4,n),100,4000,1.5)*(t/dur)**2
    return x*env(n,0.1,0,1,0.02)
def impact(dur=2.5, f=46):
    n=n_(dur); t=tt(n); x=sine(f,n)*np.exp(-t*2.2)*1.3+lp(noise(n),180)*np.exp(-t*4)*1.6+lp(noise(n),2500)*np.exp(-t*1.4)*0.5
    return dist(x,2.2)
def whoosh(dur=1.6):
    n=n_(dur); t=tt(n); u=t/dur; fc=200*(6000/200)**np.sin(np.pi*u); x=np.empty(n); zi=None; k=0
    while k<n:
        e=min(n,k+512); c=fc[k]; sos=ss.butter(2,[c/(1.6)/(SR/2),min(c*1.6,SR*0.45)/(SR/2)],'band',output='sos')
        if zi is None: zi=ss.sosfilt_zi(sos)*0
        x[k:e],zi=ss.sosfilt(sos,noise(e-k),zi=zi); k=e
    x*=np.sin(np.pi*u)**0.7*4; pan=np.linspace(-0.9,0.9,n); return np.stack([x*(1-pan)/2,x*(1+pan)/2],1)
def rumble(dur, build=True):
    n=n_(dur); t=tt(n); x=lp(noise(n),90,3)*(1+0.5*np.sin(2*np.pi*(13+8*t/dur)*t)); x*= (t/dur)**1.5 if build else 1
    return x*22
def crackle(dur):
    n=n_(dur); t=tt(n); x=lp(noise(n),1800); g=(rng.random(n)<0.0015).astype(float); g=np.convolve(g,np.exp(-np.arange(200)/40))[:n]
    return x*g*8*env(n,0.3,0,1,0.5)
def duck_env(n, times, depth=0.5, rate=7.0):   # a gain that dips after each time (a sidechain pump)
    g=np.ones(n)
    for t0 in times:
        i=n_(t0); m=min(n-i,n_(0.6))
        if m>0: g[i:i+m]=np.minimum(g[i:i+m],1-depth*np.exp(-tt(m)*rate))
    return g
class Song:
    def __init__(self, dur):
        self.n=n_(dur); self.bus={k:np.zeros((self.n,2)) for k in ('drums','bass','pad','lead','fx','chip')}
    def add(self, bus, t, x, g=1.0, pan=0.0, width=0.0):
        i=n_(t)
        if i>=self.n or i<0: return
        s = x if x.ndim==2 else stereo(x,pan,width)
        m=min(len(s),self.n-i); self.bus[bus][i:i+m]+=s[:m]*g
def reverb_ir(dur=2.6, tone=2200):
    n=n_(dur); t=tt(n); ir=np.stack([noise(n),noise(n)],1)*np.exp(-t*2.6)[:,None]*(1-np.exp(-t*300))[:,None]
    for c in range(2): ir[:,c]=lp(ir[:,c],tone)
    ir[:n_(0.02)]*=0.2; return ir/np.sqrt((ir**2).sum(0).max())*0.9
def conv(x, ir):
    out=np.zeros_like(x)
    for c in range(2): out[:,c]=ss.fftconvolve(x[:,c],ir[:,c])[:len(x)]
    return out
# =============================================================== the song
S=Song(T_END+1.0)
ADD=S.add
ost=lambda root: [root,root,root+12,root,root+7,root,root+12,root]           # a bar of 8th notes
EPIC=[57,53,48,55]      # Am F C G (bass notes: A2 F2 C3 G2)
def epic_bar(t0, root, strength=1.0, hats=True, snares=True, taikos=True, choir_on=False):
    for k,m in enumerate(ost(root)): ADD('lead',t0+k*BEAT/2,strings(f_of(m+12),0.27,bright=2600+1200*strength),0.16*strength,pan=0.25*((k&1)*2-1),width=0.3)
    ADD('bass',t0,strings(f_of(root-12),BAR*0.98,bright=600,a=0.02,r=0.2),0.5*strength)
    if taikos:
        for b,g in ((0,1.0),(1.5,0.6),(2,0.9),(3.5,0.5)): ADD('drums',t0+b*BEAT,taiko(),0.7*g*strength)
    for b in range(4): ADD('drums',t0+b*BEAT,kick(),0.9*strength)
    if snares:
        for b in (1,3): ADD('drums',t0+b*BEAT,snare(),0.55*strength); ADD('drums',t0+b*BEAT,clap(),0.35*strength)
    if hats:
        for k in range(16): ADD('drums',t0+k*BEAT/4,hat(open_=(k%8==6)),(0.22 if k%2==0 else 0.12)*strength,pan=0.3)
    if choir_on:
        for m in (root+12,root+16 if root in (53,48,55) else root+15,root+19): ADD('pad',t0,choir(f_of(m+12),BAR*1.05,a=0.3,r=0.5),0.11*strength,width=0.5)
# ---- 0:00 the quiet start: a pad, the title theme's first notes on a celesta, a slow heartbeat
ADD('pad',0.0,choir(f_of(45),T_RISER+2,a=2.5,r=2.0),0.16,width=0.5); ADD('pad',0.0,choir(f_of(52),T_RISER+2,a=3.5,r=2.0),0.10,width=0.5)
ADD('bass',0.0,lp(saw(f_of(33),n_(T_DUNGEON)),300)*env(n_(T_DUNGEON),3,0,1,1.0),0.25)
for k,(t,m) in enumerate(((1.0,72),(2.0,76),(3.0,79),(4.0,84),(6.5,83),(7.5,79),(8.5,76),(11.0,72))): ADD('lead',t,pluck(f_of(m),2.2),0.42,pan=0.15*((k&1)*2-1))
for b in range(0,int(T_RISER)): ADD('drums',b*1.0+0.5,kick(0.5,38,80,0.0),0.35); ADD('drums',b*1.0+0.78,kick(0.4,38,70,0.0),0.2)
ADD('fx',T_RISER,riser(T_DUNGEON-T_RISER),0.6,width=0.6)
for k in range(8): ADD('drums',T_RISER+k*BEAT,taiko(),0.35+0.08*k)
for k in range(8,16): ADD('drums',T_RISER+2+ (k-8)*BEAT/4,taiko(1.0),0.5+0.06*(k-8))
ADD('drums',T_DUNGEON,impact(),1.0); ADD('drums',T_DUNGEON,crash(),0.5,width=0.6)
# ---- 0:16 dungeons: half time, braams, the ostinato grows
for bar in range(8):
    t0=T_DUNGEON+bar*BAR; root=EPIC[bar%4]; st=0.75+0.03*bar
    for k,m in enumerate(ost(root)): ADD('lead',t0+k*BEAT/2,strings(f_of(m+12),0.27,bright=2200),0.14*st,pan=0.25*((k&1)*2-1),width=0.3)
    ADD('bass',t0,strings(f_of(root-12),BAR*0.98,bright=500,a=0.02,r=0.2),0.45)
    for b,g in ((0,1.0),(2.5,0.7),(3,0.9)): ADD('drums',t0+b*BEAT,taiko(),0.8*g)
    ADD('drums',t0,kick(),0.8); ADD('drums',t0+2*BEAT,kick(),0.7)
    if bar%2==0: ADD('bass',t0,braam(f_of(root-12),3.6),0.7,width=0.4)
    if bar>=4:
        for b in (1,3): ADD('drums',t0+b*BEAT,snare(),0.45)
        for k in range(8): ADD('drums',t0+k*BEAT/2,hat(),0.15,pan=0.3)
# ---- 0:32 the boss riff (the game's boss tune, low) with the drums in full
for loop in range(2):
    t0=T_BOSS+loop*BAR*4
    for st,ln,m in notes(mel_boss,BEAT/4): ADD('lead',t0+st,brass(f_of(m-12),ln*0.95,a=0.01,r=0.04),0.42,width=0.2); ADD('lead',t0+st,strings(f_of(m),ln*0.95,bright=4000,a=0.01,r=0.05),0.2)
    for st,ln,m in notes(bas_boss,BEAT/4): ADD('bass',t0+st,strings(f_of(m-12),ln*0.9,bright=700,a=0.005,r=0.03),0.55); ADD('bass',t0+st,sub(f_of(m-24),ln*0.8),0.5)
    for bar in range(4):
        tb=t0+bar*BAR
        for b in range(4): ADD('drums',tb+b*BEAT,kick(),0.95)
        for b in (1,3): ADD('drums',tb+b*BEAT,snare(),0.6); ADD('drums',tb+b*BEAT,clap(),0.3)
        for b,g in ((0,1.0),(1.5,0.6),(2,0.9),(3.5,0.6)): ADD('drums',tb+b*BEAT,taiko(),0.6*g)
        for k in range(16): ADD('drums',tb+k*BEAT/4,hat(open_=(k%8==6)),0.2 if k%2==0 else 0.1,pan=0.3)
ADD('drums',T_BOSS,crash(),0.5,width=0.6); ADD('bass',T_BOSS,braam(f_of(40-12),3.0),0.8,width=0.4); ADD('bass',T_BOSS+BAR*4,braam(f_of(40-12),3.0),0.8,width=0.4)
ADD('fx',T_SPACE-2.0,riser(2.0,400,7000),0.5,width=0.6)
# ---- 0:40 space: it opens up. choir, the space arpeggio on a bright lead, four on the floor
ADD('drums',T_SPACE,impact(2.0,52),0.9); ADD('drums',T_SPACE,crash(),0.6,width=0.6)
for loop in range(2):
    t0=T_SPACE+loop*BAR*4
    for st,ln,m in notes(mel_space,BEAT/4):
        ADD('lead',t0+st,strings(f_of(m+12),ln*0.9,bright=6000,a=0.005,r=0.04),0.26,width=0.4); ADD('lead',t0+st+0.375,strings(f_of(m+12),ln*0.9,bright=5000,a=0.005,r=0.04),0.12,pan=0.5)   # with an echo
    for bar in range(4):
        tb=t0+bar*BAR; root=[45,45,50,50,48,48,43,40][loop*0+bar*2%8] if False else [45,50,48,43][bar]
        epic_bar(tb,root+12,strength=0.85,hats=True,snares=True,taikos=(bar>=2),choir_on=True)
        ADD('pad',tb,choir(f_of(root+24),BAR*1.05,a=0.4,r=0.5),0.09,width=0.6)
    ADD('bass',t0,braam(f_of(45-12),3.5),0.6,width=0.4)
# ---- 0:56 the Technobeast: dark, pulsing, a siren of detuned saws
ADD('drums',T_BEAST,impact(2.5,40),1.0); ADD('bass',T_BEAST,braam(f_of(28),5.0,dark=True),1.0,width=0.5); ADD('bass',T_BEAST+4,braam(f_of(27),4.0,dark=True),1.0,width=0.5)
for bar in range(4):
    tb=T_BEAST+bar*BAR
    for b in range(8): ADD('bass',tb+b*BEAT/2,sub(f_of(28),0.2),0.9); ADD('drums',tb+b*BEAT/2,kick(0.3,40,120),0.6)
    for b in (1,3): ADD('drums',tb+b*BEAT,snare(),0.5); ADD('drums',tb+b*BEAT,taiko(),0.7)
    for k in range(16): ADD('drums',tb+k*BEAT/4,hat(),0.12,pan=0.3)
    n=n_(BAR); ADD('pad',tb,sweep_lp(saw(f_of(52),n)+saw(f_of(52)*1.01,n)+saw(f_of(58),n),300,2500,1.0)*env(n,0.05,0,1,0.3),0.07,width=0.5)
ADD('fx',T_WORLDS-2.0,riser(2.0,300,8000),0.6,width=0.6)
# ---- 1:04 the worlds montage: full drive, the loop, choir; a build in the last four bars that gets cut off
ADD('drums',T_WORLDS,crash(),0.6,width=0.6)
for bar in range(12):
    tb=T_WORLDS+bar*BAR; root=EPIC[bar%4]
    epic_bar(tb,root,strength=0.9+0.02*bar,choir_on=(bar>=4))
    if bar%4==0: ADD('bass',tb,braam(f_of(root-12),3.2),0.6,width=0.4)
    if bar>=8:
        for k in range(16): ADD('lead',tb+k*BEAT/4,strings(f_of(root+24+(0 if k%4<2 else 7)),0.11,bright=5000,a=0.003,r=0.03),0.14,pan=0.4*np.sin(k))
    if bar==11:
        for k in range(16): ADD('drums',tb+k*BEAT/4,snare(0.15),0.3+0.03*k)
ADD('fx',T_STOP-3.0,riser(3.0,300,9000),0.7,width=0.6)
# ---- 1:30 kart racing: the game's own race tune on square waves, a chip bass and chip drums
ADD('chip',T_STOP+1.1,chip_sq(f_of(84),0.09,0.25),0.25); ADD('chip',T_STOP+1.2,chip_sq(f_of(79),0.09,0.25),0.25); ADD('chip',T_STOP+1.3,chip_sq(f_of(72),0.2,0.25),0.25)   # a little "huh?"
for loop in range(2):
    t0=T_KART+loop*BAR*4
    for st,ln,m in notes(mel_race,BEAT/4):
        if t0+st>=T_MAKERS: continue
        ADD('chip',t0+st,chip_sq(f_of(m),ln*0.9,0.25),0.22,pan=-0.2); ADD('chip',t0+st,chip_sq(f_of(m-12),ln*0.9,0.5),0.12,pan=0.2)
    for st,ln,m in notes(bas_race,BEAT/4):
        if t0+st>=T_MAKERS: continue
        ADD('chip',t0+st,chip_tri(f_of(m-12),ln*0.9),0.45)
    for bar in range(4):
        tb=t0+bar*BAR
        if tb>=T_MAKERS: continue
        for b in range(4): ADD('chip',tb+b*BEAT,chip_kick(),0.5)
        for b in (1,3): ADD('chip',tb+b*BEAT,chip_noise(0.18),0.3)
        for k in range(8): ADD('chip',tb+k*BEAT/2,chip_noise(0.05),0.08,pan=0.3)
ADD('fx',T_MAKERS-2.0,riser(2.0,300,9000),0.7,width=0.6)
for k in range(8): ADD('drums',T_MAKERS-2.0+k*BEAT/2,taiko(0.8),0.4+0.06*k)
# ---- 1:44 makers and HOW BIG IS IT?: epic again, a counting arpeggio that climbs
ADD('drums',T_MAKERS,impact(2.0,48),0.9); ADD('drums',T_MAKERS,crash(),0.6,width=0.6)
for bar in range(12):
    tb=T_MAKERS+bar*BAR; root=EPIC[bar%4]
    epic_bar(tb,root,strength=0.85+0.015*bar,choir_on=True,hats=(bar>=2))
    if bar%4==0: ADD('bass',tb,braam(f_of(root-12),3.2),0.6,width=0.4)
    arp=[0,7,12,19,24,19,12,7];
    for k in range(16):
        m=root+12+arp[k%8]+(12 if k>=8 and bar>=6 else 0); ADD('lead',tb+k*BEAT/4,pluck(f_of(m),0.5),0.2+0.01*bar,pan=0.5*np.sin(k*0.9))
    if bar>=8:
        for m in (root+24,root+31): ADD('pad',tb,strings(f_of(m),BAR,bright=5000,a=0.3,r=0.3),0.08,width=0.6)
for k in range(8): ADD('drums',T_CLIMAX-2.0+k*BEAT/4,snare(0.15),0.3+0.05*k); ADD('drums',T_CLIMAX-2.0+k*BEAT/4,taiko(0.6),0.4+0.05*k)
ADD('fx',T_CLIMAX-2.0,riser(2.0,400,9000),0.7,width=0.6)
# ---- 2:08 the climax: the title fanfare on brass, everything in
ADD('drums',T_CLIMAX,impact(2.0,50),1.0); ADD('drums',T_CLIMAX,crash(),0.7,width=0.6)
for st,ln,m in notes(mel_title,BEAT/2):
    ADD('lead',T_CLIMAX+st,brass(f_of(m),ln*0.95),0.5,width=0.3); ADD('lead',T_CLIMAX+st,brass(f_of(m-12),ln*0.95),0.25,width=0.3); ADD('lead',T_CLIMAX+st,strings(f_of(m+12),ln*0.95,bright=5000,a=0.02,r=0.08),0.15,width=0.5)
for st,ln,m in notes(bas_title,BEAT/2): ADD('bass',T_CLIMAX+st,strings(f_of(m-12),ln*0.98,bright=700,a=0.02,r=0.1),0.5); ADD('bass',T_CLIMAX+st,sub(f_of(m-24),ln*0.95),0.6)
for bar in range(8):
    tb=T_CLIMAX+bar*BAR; root=[48,43,53,57,55,53,48,48][bar]
    epic_bar(tb,root,strength=1.0,choir_on=True)
    ADD('bass',tb,braam(f_of(root-12),2.2),0.45,width=0.4)
for bar in range(8,12):       # four bars of hits, each one bigger
    tb=T_CLIMAX+bar*BAR; root=EPIC[bar%4]
    epic_bar(tb,root,strength=1.05,choir_on=True)
    for b in (0,2.5): ADD('lead',tb+b*BEAT,brass(f_of(root+12),0.45,a=0.01),0.5,width=0.4); ADD('lead',tb+b*BEAT,brass(f_of(root+19),0.45,a=0.01),0.35,width=0.4); ADD('drums',tb+b*BEAT,taiko(),0.9)
    ADD('bass',tb,braam(f_of(root-12),2.0),0.5,width=0.4)
for k in range(8): ADD('drums',T_HIT-2.0+k*BEAT/4,snare(0.15),0.35+0.05*k)
ADD('fx',T_HIT-2.0,riser(2.0,500,10000),0.6,width=0.6)
ADD('drums',T_HIT,impact(3.0,44),1.2); ADD('drums',T_HIT,crash(),0.5,width=0.6); ADD('lead',T_HIT,brass(f_of(57),1.2,a=0.01,r=0.9),0.6,width=0.4); ADD('lead',T_HIT,brass(f_of(45),1.2,a=0.01,r=0.9),0.4)
# ---- 2:32 almost silence: a hum. then the launch: rumble, ignition, lift-off, the fanfare slowly, the flyby, the title chord
ADD('pad',T_HIT+0.8,lp(saw(f_of(33),n_(T_LAUNCH-T_HIT+6)),200)*env(n_(T_LAUNCH-T_HIT+6),1.5,0,1,2.0),0.2)
ADD('pad',T_LAUNCH,choir(f_of(57),T_LIFT-T_LAUNCH+1,a=2.5,r=1.0),0.12,width=0.5); ADD('pad',T_LAUNCH,choir(f_of(64),T_LIFT-T_LAUNCH+1,a=3.5,r=1.0),0.08,width=0.5)
for b in range(int((T_IGNITE-T_LAUNCH)/1.0)): ADD('drums',T_LAUNCH+b*1.0,kick(0.5,38,80,0.0),0.4); ADD('drums',T_LAUNCH+b*1.0+0.28,kick(0.4,38,70,0.0),0.25)
ADD('fx',T_LAUNCH+1.5,rumble(T_FLYBY-T_LAUNCH-1.5),0.9,width=0.7)
ADD('drums',T_IGNITE,impact(2.0,40),0.8); ADD('fx',T_IGNITE,crackle(T_LIFT-T_IGNITE+3),0.8,width=0.6)
for k in range(8): ADD('drums',T_IGNITE+k*BEAT/2,taiko(1.2,45,110),0.4+0.06*k)
ADD('drums',T_LIFT,impact(2.5,42),1.0); ADD('drums',T_LIFT,crash(),0.5,width=0.6)
# the fanfare, half speed, swelling as the ship climbs
for st,ln,m in notes(mel_title[:48],BEAT):
    t=T_LIFT+st; g=0.25+0.45*min(1,(t-T_LIFT)/8.0)
    ADD('lead',t,brass(f_of(m),ln*0.97,a=0.08,r=0.2),g,width=0.4); ADD('lead',t,strings(f_of(m+12),ln*0.97,bright=5000,a=0.1,r=0.2),g*0.35,width=0.5); ADD('pad',t,choir(f_of(m),ln*1.1,a=0.3,r=0.4),g*0.3,width=0.6)
for st,ln,m in notes(bas_title[:48],BEAT): ADD('bass',T_LIFT+st,strings(f_of(m-12),ln*0.98,bright=600,a=0.05,r=0.2),0.5); ADD('bass',T_LIFT+st,sub(f_of(m-24),ln*0.95),0.5)
for bar in range(4):
    tb=T_LIFT+bar*BAR*1.0
    for b,g in ((0,1.0),(2,0.8)): ADD('drums',tb+b*BEAT,taiko(),0.6*g+0.1*bar)
    for b in range(4): ADD('drums',tb+b*BEAT,kick(),0.5+0.1*bar)
n=n_(T_FLYBY-T_LIFT-4.5); ADD('drums',T_LIFT+4.5,lp(noise(n),400)*(tt(n)/(T_FLYBY-T_LIFT-4.5))**2*np.abs(np.sin(2*np.pi*14*tt(n)))*2.5,0.6)   # a timpani roll
for k in range(12): ADD('drums',T_FLYBY-3.0+k*BEAT/4,snare(0.15),0.3+0.04*k)
ADD('fx',T_FLYBY-2.5,riser(2.5+0.2,300,10000),0.8,width=0.6)
ADD('fx',T_FLYBY-0.4,whoosh(1.4),1.0)
ADD('drums',T_FLASH,impact(3.5,36),1.3); ADD('drums',T_FLASH,crash(),0.9,width=0.7)
for m in (48,55,60,64,67,72,76,79,84): ADD('lead',T_FLASH,brass(f_of(m),4.5,a=0.02,r=2.5),0.26 if m<70 else 0.16,width=0.5)
for m in (60,64,67,72,76): ADD('pad',T_FLASH+0.1,choir(f_of(m),5.5,a=0.3,r=2.5),0.14,width=0.7)
ADD('bass',T_FLASH,sub(f_of(36),4.5),0.8); ADD('bass',T_FLASH,strings(f_of(36),4.5,bright=500,a=0.02,r=2.0),0.5)
for k,(t,m) in enumerate(((0.0,84),(0.25,88),(0.5,91),(0.75,96),(1.5,95),(1.75,91),(2.0,88))): ADD('lead',T_TITLE+1.0+t,glock(f_of(m)),0.3,pan=0.2*((k&1)*2-1))
ADD('fx',T_TITLE+2.5,crash(3.0),0.3,width=0.7)
# =============================================================== mix
ir=reverb_ir()
mix=np.zeros((S.n,2))
send={'drums':0.22,'bass':0.05,'pad':0.5,'lead':0.35,'fx':0.3,'chip':0.12}
# the sidechain pump in the driving sections: pads and leads dip on each beat
pump_times=[t for a,b in ((T_WORLDS,T_STOP),(T_MAKERS,T_HIT)) for t in np.arange(a,b,BEAT)]
g=duck_env(S.n,pump_times,0.35,9.0)
for k in ('pad','lead'): S.bus[k]*=g[:,None]
for k,b in S.bus.items():
    mix+=b
    if send[k]: mix+=conv(b,ir)*send[k]
# the record stop: everything slows to a halt at T_STOP, then silence until the chip tune
i0=n_(T_STOP-0.25); src=mix[i0:n_(T_STOP+2.0)].copy(); L=n_(1.1); u=tt(L)/1.1; rate=1-u; pos=np.cumsum(rate)/SR*SR; pos=np.clip(pos,0,len(src)-2)
k=pos.astype(int); fr=pos-k; stopped=src[k]*(1-fr)[:,None]+src[k+1]*fr[:,None]; stopped*=np.linspace(1,0.15,L)[:,None]
mix[i0:i0+L]=stopped; mix[i0+L:n_(T_STOP+1.0)]=0; mix[n_(T_STOP+1.0):n_(T_KART)]*=1.0   # (the little chip "huh?" sits in that gap, added below)
for b in ('chip',):
    pass
# the chip "huh?" and the kart tune were in the mix before the stop was cut in: put the gap's chip notes back
# fades: the start, and the very end
mix[:n_(0.5)]*=np.linspace(0,1,n_(0.5))[:,None]
fe=n_(T_END-2.5); mix[fe:]*=np.linspace(1,0,S.n-fe)[:,None]**1.5
# ---- the balance: each section is brought to its own loudness (builds inside a section are kept), then a look-ahead limiter
from scipy.ndimage import maximum_filter1d
targets=[(T_INTRO,0.055),(T_RISER,0.13),(T_DUNGEON,0.17),(T_BOSS,0.21),(T_SPACE,0.19),(T_BEAST,0.2),(T_WORLDS,0.21),(T_STOP-3,0.25),(T_STOP,0.14),(T_KART,0.15),
         (T_MAKERS,0.19),(T_CLIMAX,0.25),(T_HIT,0.2),(T_HIT+1.0,0.09),(T_LAUNCH,0.07),(T_IGNITE,0.16),(T_LIFT,0.2),(T_LIFT+5,0.25),(T_FLYBY-2.5,0.3),(T_FLASH,0.33),(T_TITLE+1.5,0.24),(T_END,0.2)]
gain=np.ones(S.n)
for k,(t0,tg) in enumerate(targets[:-1]):
    t1=targets[k+1][0]; seg=mix[n_(t0):n_(t1)]; r=np.sqrt((seg**2).mean())+1e-6; g=tg/r
    i0,i1=n_(t0),n_(t1); gain[i0:i1]=g
    print('%7.1f-%6.1f raw rms %.3f gain %.2f'%(t0,t1,r,g))
# soft edges: a 0.4 s ramp at every change
k=n_(0.4); gain=np.convolve(gain,np.ones(k)/k,mode='same')
mix*=gain[:,None]
def limiter(x, ceil=0.89, look=0.004, rel=0.12):
    env=np.abs(x).max(1); la=max(2,n_(look)); peak=maximum_filter1d(env,size=la,mode='nearest')
    B=32; nb=len(x)//B+1; pk=np.zeros(nb); pk[:len(env)//B]=peak[:len(env)//B*B].reshape(-1,B).max(1)
    if len(env)%B: pk[len(env)//B]=peak[len(env)//B*B:].max()
    gt=np.minimum(1.0,ceil/np.maximum(pk,1e-9)); kr=np.exp(-B/(rel*SR)); g=np.empty(nb); cur=1.0
    for i in range(nb):
        cur = gt[i] if gt[i]<cur else cur*kr+gt[i]*(1-kr); g[i]=cur
    gs=np.interp(np.arange(len(x)),np.arange(nb)*B,g)
    return x*gs[:,None], gs
mix,gl=limiter(mix)
print('limiter: gain below 0.7 for %.1f%% of the time, min %.2f'%(100*(gl<0.7).mean(),gl.min()))
peak=np.abs(mix).max(); print('peak',peak)
wavf.write(os.path.join(os.path.dirname(os.path.abspath(__file__)),'music.wav'),SR,(mix*32767).astype(np.int16))
# a loudness report per section
secs=[('intro',T_INTRO),('dungeon',T_DUNGEON),('boss',T_BOSS),('space',T_SPACE),('beast',T_BEAST),('worlds',T_WORLDS),('stop',T_STOP),('kart',T_KART),('makers',T_MAKERS),('climax',T_CLIMAX),('hit',T_HIT),('launch',T_LAUNCH),('ignite',T_IGNITE),('lift',T_LIFT),('flash',T_FLASH),('title',T_TITLE),('end',T_END)]
for (a,ta),(b,tb) in zip(secs,secs[1:]):
    seg=mix[n_(ta):n_(tb)]; print('%-8s %6.1f-%6.1f rms %.3f peak %.2f'%(a,ta,tb,np.sqrt((seg**2).mean()),np.abs(seg).max()))
