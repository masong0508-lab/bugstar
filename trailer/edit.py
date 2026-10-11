# edit.py: cuts the recorded clips and the text cards into the trailer (1920x1080, 30 fps), muxes the music, writes bugstar_trailer.mp4
import numpy as np, os, sys, subprocess, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter
from tl import *
T=os.path.dirname(os.path.abspath(__file__)); C=os.path.join(T,'clips')
W,H,FPS=1920,1080,30
OUT=sys.argv[1] if len(sys.argv)>1 else os.path.join(T,'bugstar_trailer.mp4')
PREVIEW=os.environ.get('PREVIEW')          # "a,b": only render seconds a..b (to a quick file)
# ---- the clips
_clips={}
def clip(name):
    if name not in _clips:
        a=np.memmap(os.path.join(C,name+'.rgb'),dtype=np.uint8,mode='r'); n=a.size//115200; _clips[name]=a[:n*115200].reshape(n,160,240,3)
    return _clips[name]
def frame(name, i):
    c=clip(name); i=int(i); i=max(0,min(len(c)-1,i)); return np.asarray(c[i])
# ---- framing: the GBA picture filled into 1080p
def up(a, s): return np.repeat(np.repeat(a,s,0),s,1)
def frame_world(f):  return up(f[18:153],8)                                  # the view under the HUD bar: 240x135 -> 1920x1080
def frame_space(f):  o=np.zeros((H,W,3),np.uint8); o[28:28+1024]=up(f[0:128],8); return o
def frame_launch(f): return up(f[12:147],8)
_bgcache={}
def frame_full(f, key=None):                                                 # a whole screen (menus): 6x, over a blurred, darkened copy of itself
    big=up(f,6); o=np.zeros((H,W,3),np.uint8)
    bg=Image.fromarray(f).resize((W,H),Image.BILINEAR).filter(ImageFilter.GaussianBlur(40)); bg=(np.asarray(bg)*0.35).astype(np.uint8)
    o[:]=bg; y0=(H-960)//2; x0=(W-1440)//2; o[y0:y0+960,x0:x0+1440]=big; return o
FR={'world':frame_world,'space':frame_space,'full':frame_full,'launch':frame_launch}
# ---- text
FONT=lambda sz: ImageFont.truetype('/usr/share/fonts/opentype/inter/InterDisplay-ExtraBold.otf',sz)
exec(open(os.path.join(T,'pixfont.py')).read().split('fv=[0]*128')[0].replace('# ---------------- font 3x5',''))   # the game's 3x5 font: F
def pix_text(s, scale, col=(255,214,64), shadow=(60,40,0)):              # the game's own pixel font, big. Returns RGBA
    w=len(s)*4*scale; h=5*scale+scale; im=np.zeros((h+scale,w+scale,4),np.uint8)
    for k,ch in enumerate(s):
        bits=F.get(ch.upper(),'0'*15)
        for b in range(15):
            if bits[b]=='1':
                x=(k*4+b%3)*scale; y=(b//3)*scale
                im[y+scale:y+2*scale,x+scale:x+2*scale]=(*shadow,255)
    for k,ch in enumerate(s):
        bits=F.get(ch.upper(),'0'*15)
        for b in range(15):
            if bits[b]=='1':
                x=(k*4+b%3)*scale; y=(b//3)*scale; im[y:y+scale,x:x+scale]=(*col,255)
    return Image.fromarray(im)
def text_img(lines, size=96, col=(255,255,255), pix=False, pixcol=(255,214,64), gap=0.25):   # a card: lines of text as one RGBA image with a shadow
    ims=[]
    for ln in lines:
        if ln.startswith('#'):                                                           # '#...' lines are in the pixel font
            sc=max(2,min(size//6,1760//(4*len(ln)-3))); ims.append(pix_text(ln[1:],sc,pixcol)); continue
        f=FONT(size); bb=f.getbbox(ln); w=bb[2]-bb[0]+40; h=bb[3]-bb[1]+40
        im=Image.new('RGBA',(w+12,h+12),(0,0,0,0)); d=ImageDraw.Draw(im)
        for dx,dy in ((6,8),): d.text((20-bb[0]+dx,20-bb[1]+dy),ln,font=f,fill=(0,0,0,170))
        d.text((20-bb[0],20-bb[1]),ln,font=f,fill=(*col,255)); ims.append(im)
    gapp=int(size*gap); tw=max(i.width for i in ims); th=sum(i.height for i in ims)+gapp*(len(ims)-1)
    out=Image.new('RGBA',(tw,th),(0,0,0,0)); y=0
    for i in ims: out.alpha_composite(i,((tw-i.width)//2,y)); y+=i.height+gapp
    return out
def fit(im, maxw=1760):
    if im.width>maxw: im=im.resize((maxw,int(im.height*maxw/im.width)),Image.LANCZOS)
    return im
_cards={}
def card_img(key, lines, size, col, pixcol):
    if key not in _cards: _cards[key]=fit(text_img(lines,size,col,pixcol=pixcol))
    return _cards[key]
def blit(base, im, cx, cy, alpha=1.0, scale=1.0):                              # RGBA image onto the frame (uint8 array), centred at cx,cy
    if scale!=1.0: im=im.resize((max(1,int(im.width*scale)),max(1,int(im.height*scale))),Image.BILINEAR)
    a=np.asarray(im); h,w=a.shape[:2]; x0=int(cx-w/2); y0=int(cy-h/2)
    xs=max(0,x0); ys=max(0,y0); xe=min(W,x0+w); ye=min(H,y0+h)
    if xe<=xs or ye<=ys: return
    src=a[ys-y0:ye-y0, xs-x0:xe-x0]; al=(src[:,:,3:4].astype(np.float32)/255.0)*alpha
    dst=base[ys:ye,xs:xe]; base[ys:ye,xs:xe]=(dst*(1-al)+src[:,:,:3]*al).astype(np.uint8)
# ---- the timeline
SHOTS=[]   # (t0, t1, clip, f0, speed, framing, fx)   fx: dict with zoom, shake, hold, fade_in, fade_out, dim
CARDS=[]   # (t0, t1, lines, size, col, pixcol, y, style)   style: 'black' (its own black frame), 'over' (over the footage)
def shot(t0,t1,c,f0,speed=1.0,fr='world',**fx): SHOTS.append((t0,t1,c,f0,speed,fr,fx))
def card(t0,t1,lines,size=100,col=(255,255,255),pixcol=(255,214,64),y=0.5,style='over'):
    if isinstance(lines,str): lines=[lines]
    CARDS.append((t0,t1,lines,size,col,pixcol,y,style))
def black(t0,t1): SHOTS.append((t0,t1,None,0,1,'black',{}))
WHITE=(255,255,255); GOLD=(255,214,64); CYAN=(120,230,255); RED=(255,110,90); GREY=(190,190,200)
# 0:00 somewhere in a very big galaxy
black(0,4.0)
card(1.0,4.0,["SOMEWHERE IN A VERY,","VERY BIG GALAXY..."],size=92,style='black')
shot(4.0,8.5,'garden',40,1.0,zoom=(1.0,1.08))
card(4.6,8.3,["...LIVES A VERY,","VERY SMALL BUG."],size=92,y=0.78)
shot(8.5,11.5,'gardenwalk',60,1.0)
card(9.0,11.3,"HE HAS ONE JOB.",size=104,y=0.5)
black(11.5,16.0)
card(11.6,13.7,"GO DOWN.",size=150,style='black')
card(13.8,16.0,"BEAT THE BOSS.",size=150,style='black')
# 0:16 dungeons
shot(16.0,19.0,'dexp',0,1.0,flash=0.5,shake=(0.5,14))
card(16.3,18.8,"DUNGEONS.",size=170,col=GOLD)
shot(19.0,21.0,'zone1',300,1.0)
card(19.2,21.0,"SIX ZONES.",size=120)
shot(21.0,23.0,'zone3',300,1.0)
shot(23.0,25.0,'dexp',529,1.2)
card(23.3,25.0,"MONSTERS.",size=140,col=RED)
shot(25.0,27.0,'zone5',500,1.0)
shot(27.0,29.0,'zone4',300,1.0)
shot(29.0,32.0,'boss0',0,1.0,flash=0.4,shake=(0.4,10))
card(29.3,30.6,"BOSSES.",size=170,col=GOLD)
card(30.6,32.0,"SHE IS NOT HAPPY.",size=100,y=0.8)
shot(32.0,33.0,'boss5',0,1.0,shake=(0.3,8)); shot(33.0,34.0,'boss2',0,1.0,shake=(0.3,8)); shot(34.0,35.0,'boss0',94,1.0); shot(35.0,36.0,'boss5',47,1.0)
shot(36.0,38.0,'boss2',5,1.0); card(36.2,38.0,["YOU ARE A PILL BUG.","YOU HAVE 40 HP."],size=96)
shot(38.0,40.0,'boss0',141,1.0); card(38.2,40.0,"ROLL WITH IT.",size=130,col=GOLD)
# 0:40 space
shot(40.0,44.0,'space',0,1.0,fr='space',flash=0.5,zoom=(1.0,1.05))
card(40.5,43.5,"THEN GET IN YOUR SHIP.",size=100,y=0.5)
shot(44.0,47.0,'warp',44,1.0,fr='space')
card(44.3,47.0,"FLY TO OTHER STARS.",size=100)
shot(47.0,52.0,'fleet',0,1.0,fr='space',shake=(0.3,6))
card(47.5,49.8,"FIGHT WHOLE FLEETS.",size=110,col=RED)
card(50.0,52.0,"50 SHIPS AT ONCE.",size=100)
shot(52.0,56.0,'fleet',300,1.0,fr='space')
card(52.3,54.0,"HIRE WINGMEN.",size=110)
card(54.0,56.0,['WINGMAN: "THAT ONE WAS RUDE."'],size=72,col=CYAN,y=0.8)
# 0:56 the Technobeast
shot(56.0,60.0,'beast',0,1.0,fr='space',flash=0.6,shake=(0.6,16))
card(56.4,58.4,"THE TECHNOBEAST.",size=120,col=RED)
card(58.5,61.5,["#1,000,000,000,000 HP"],size=170)
shot(60.0,64.0,'beast',200,1.0,fr='space')
card(61.6,63.0,"(THAT'S ONE TRILLION.)",size=84,col=GREY)
card(63.0,64.0,"(YOU HAVE 40.)",size=84,col=GREY)
# 1:04 the worlds
shot(64.0,66.5,'unijumpf',60,1.0,fr='space',flash=0.4)
card(64.2,66.5,["THERE ARE","#4,294,967,296","UNIVERSES."],size=96,y=0.5)
shot(66.5,68.5,'unijump',221,1.0,fr='full')
card(67.0,68.5,"(ACTUAL SCREEN.)",size=60,col=GREY,y=0.9)
shot(68.5,70.5,'c_paradise',60,1.0)
card(68.6,70.5,["EACH ONE IS","1.1 QUINDECILLION","LIGHT YEARS WIDE."],size=88)
shot(70.5,71.5,'w_rings',60,1.0)
card(70.55,71.5,"WE CHECKED.",size=120,col=GOLD)
WORLDS=[('w_disco',"DISCO WORLDS."),('w_rainbow',"RAINBOW WORLDS."),('w_giants',"WORLDS WITH GIANTS."),('w_suns',"WORLDS WITH EXTRA SUNS."),('w_rings',"RING WORLDS."),('w_negative',"NEGATIVE WORLDS."),
        ('w_glitch',"GL!TCH W%RLDS."),('w_checker',"CHECKERBOARD WORLDS."),('w_mirror',"MIRROR-SKY WORLDS."),('c_inferno',"HELL."),('c_brimstone',"MORE HELL."),('c_obsidian',"FANCY HELL."),
        ('c_bone',"BONE HELL."),('c_bloodsea',"BLOOD HELL."),('c_cocytus',"FROZEN HELL.")]
t=71.5
for i,(c,label) in enumerate(WORLDS):
    shot(t,t+1.0,c,40+i*7,1.5); card(t+0.05,t+1.0,label,size=96 if len(label)<16 else 80,col=RED if 'HELL' in label else WHITE,y=0.5); t+=1.0
shot(86.5,88.0,'c_city2',120,1.0)
card(86.6,88.0,"...AND ONE NICE CITY.",size=92)
# 1:28 the record stop
shot(88.0,88.8,'c_city2',165,0.0,hold=True,dim=(1.0,0.2))
black(88.8,90.5)
card(88.85,89.5,"WAIT.",size=150,style='black')
card(89.55,90.5,"KART RACING?",size=130,style='black')
# 1:30 karts
shot(90.5,94.0,'kjump',150,1.0,flash=0.3)
card(90.8,92.8,"YES. KART RACING.",size=120,col=GOLD)
shot(94.0,97.0,'kart',200,1.0)
card(94.2,97.0,["#1,000,000 TRACKS","ON EVERY PLANET."],size=90)
shot(97.0,100.0,'kart2',150,1.0)
card(97.2,100.0,["ON ALL","#4,294,967,296","OF THEM."],size=90)
shot(100.0,104.0,'kjump',290,1.0)
card(100.2,102.0,"SOMEONE DID THE MATHS.",size=96)
card(102.1,104.0,"(IT WAS US. IT TOOK A WHILE.)",size=72,col=GREY,y=0.8)
# 1:44 makers, HOW BIG IS IT?
shot(104.0,107.0,'makers',0,1.3,fr='full',flash=0.4)
card(104.4,106.8,"MAKE YOUR BUG.",size=120,y=0.5)
shot(107.0,110.0,'human',0,1.3,fr='full')
card(107.2,110.0,["OR A HUMANOID.","IF YOU MUST."],size=100)
shot(110.0,113.0,'makers',139,1.3,fr='full')
card(110.3,112.8,"BUILD YOUR SHIP.",size=120)
shot(113.0,116.0,'hb_screens',0,1.0,fr='full')
card(113.3,116.0,["IT EVEN COUNTS","HOW BIG IT IS."],size=100)
shot(116.0,121.0,'hb_screens',57,1.3,fr='full')
card(116.2,118.0,"PICTURES THE SCREEN CAN SHOW:",size=80)
card(118.1,121.0,["#173,394 DIGITS."],size=150)
shot(121.0,124.0,'hb_screens',257,1.3,fr='full')
card(121.2,124.0,["READING THEM OUT LOUD","TAKES 48 HOURS."],size=96)
shot(124.0,128.0,'hb_list',20,1.2,fr='full')
card(124.2,125.9,"IT IS A MENU.",size=120,col=GOLD)
card(126.0,128.0,"THERE ARE 37 OF THESE.",size=96)
# 2:08 the climax
black(128.0,134.0)
card(128.1,130.0,"A LITTLE BUG.",size=140,style='black')
card(130.1,132.0,"A BIG GALAXY.",size=140,style='black')
card(132.1,134.0,["A REALLY, REALLY","BIG GALAXY."],size=120,col=GOLD,style='black')
M1=[('boss0',0,'world'),('fleet',120,'space'),('w_aurora',60,'world'),('kjump',200,'world'),('beast',60,'space'),('c_obsidian',100,'world'),('space',200,'space'),('w_moons',80,'world'),('zone3',400,'world'),('hb_screens',100,'full')]
t=134.0
for c,f0,fr in M1: shot(t,t+1.0,c,f0,1.0,fr=fr,flash=(0.1,0.5)); t+=1.0
M2=[('w_disco',100,'world'),('boss5',20,'world'),('warp',60,'space'),('c_inferno',120,'world'),('kart',300,'world'),('w_suns',100,'world'),('fleet',400,'space'),('w_glitch',100,'world'),
    ('beast',300,'space'),('w_rainbow',100,'world'),('boss2',30,'world'),('kjump',250,'world'),('c_bloodsea',100,'world'),('unijump',221,'full'),('w_giants',100,'world'),('dexp',700,'world')]
for c,f0,fr in M2: shot(t,t+0.5,c,f0,1.0,fr=fr,shake=(0.5,6)); t+=0.5
# 2:32 the small cartridge
black(152.0,156.0)
card(152.6,154.3,["ALL OF THIS FITS","IN LESS THAN 1 MB."],size=100,style='black')
card(154.4,156.0,["SMALLER THAN ONE PHOTO","ON YOUR PHONE."],size=88,col=GREY,style='black')
# 2:36 the launch: picture index from the animation time, at a speed that changes as it goes (slow at the end)
LK=[(T_LAUNCH,0),(T_IGNITE,56),(T_LIFT,80),(T_LIFT+2.5,110),(167.5,200),(168.8,240),(170.3,276),(T_FLASH,286)]
def launch_T(tv):
    for (ta,Ta),(tb,Tb) in zip(LK,LK[1:]):
        if ta<=tv<=tb: return Ta+(Tb-Ta)*(tv-ta)/(tb-ta)
    return 286
SHOTS.append((T_LAUNCH,T_FLASH,'launch4',0,0,'launch',{'launch':True,'fade_in':1.5}))
# 2:51.5 the flash and the title
SHOTS.append((T_FLASH,T_END,'launch4',0,0,'launch',{'title':True}))
# ---- rendering
def ease(x): return x*x*(3-2*x)
def render_frame(ti):
    tv=ti/FPS; base=None
    for (t0,t1,c,f0,sp,fr,fx) in SHOTS:
        if t0<=tv<t1:
            if fr=='black': base=np.zeros((H,W,3),np.uint8); break
            if fx.get('launch'):
                Tn=launch_T(tv); f=frame(c,round(Tn*4)); base=frame_launch(f)
                fi=fx.get('fade_in',0)
                if fi and tv-t0<fi: base=(base*((tv-t0)/fi)).astype(np.uint8)
                break
            if fx.get('title'):
                Tn=300+(tv-t0)*20; f=frame(c,round(Tn*4)); base=(frame_launch(f)*0.55).astype(np.uint8)
                # the flash
                u=tv-t0
                if u<0.1: base[:]=255
                elif u<0.9: a=1-(u-0.1)/0.8; base=(base*(1-a)+255*a).astype(np.uint8)
                # BUGSTAR
                if u>=0.25:
                    k=min(1,(u-0.25)/0.35); sc=1.35-0.35*ease(k); im=pix_text("BUGSTAR",32,GOLD,(70,45,0)); blit(base,im,W/2,H*0.40,min(1,k*2),sc)
                if u>=1.5:
                    k=min(1,(u-1.5)/0.4); im=pix_text("A LITTLE BUG. A BIG GALAXY.",7,WHITE,(0,0,0)); blit(base,im,W/2,H*0.60,k)
                if u>=3.0:
                    k=min(1,(u-3.0)/0.4); im=text_img(["GAME BOY ADVANCE"],44,GREY); blit(base,im,W/2,H*0.84,k*0.9)
                if tv>T_END-1.0: base=(base*max(0,(T_END-tv)/1.0)).astype(np.uint8)
                break
            idx = f0 if fx.get('hold') else f0+(tv-t0)*FPS*sp
            f=frame(c,idx); base=FR[fr](f)
            if 'zoom' in fx:
                z0,z1=fx['zoom']; z=z0+(z1-z0)*(tv-t0)/(t1-t0)
                if z!=1.0:
                    im=Image.fromarray(base); nw,nh=int(W*z),int(H*z); im=im.resize((nw,nh),Image.BILINEAR); x0=(nw-W)//2; y0=(nh-H)//2; base=np.asarray(im)[y0:y0+H,x0:x0+W].copy()
            if 'shake' in fx:
                d,amp=fx['shake']; u=tv-t0
                if u<d: k=(1-u/d)*amp; rs=np.random.default_rng(ti); dx,dy=int(rs.integers(-k,k+1)),int(rs.integers(-k,k+1)); base=np.roll(base,(dy,dx),(0,1))
            if 'dim' in fx:
                a,b=fx['dim']; k=a+(b-a)*(tv-t0)/(t1-t0); base=(base*k).astype(np.uint8)
            if 'flash' in fx:
                d=fx['flash']; am=0.9
                if isinstance(d,tuple): d,am=d
                u=tv-t0
                if u<d: a=(1-u/d)**2*am; base=(base*(1-a)+255*a).astype(np.uint8)
            break
    if base is None: base=np.zeros((H,W,3),np.uint8)
    base=np.ascontiguousarray(base)
    for (t0,t1,lines,size,col,pixcol,y,style) in CARDS:
        if t0<=tv<t1:
            key=(tuple(lines),size,col,pixcol); im=card_img(key,lines,size,col,pixcol)
            u=tv-t0; v=t1-tv; a=min(1,u/0.12,v/0.12); sc=1.0+0.08*max(0,1-u/0.12)
            if style=='over':                                                     # darken behind the words so they read
                h=im.height+80; cy=int(H*y); y0=max(0,cy-h//2); y1=min(H,cy+h//2); base[y0:y1]=(base[y0:y1]*(1-0.55*a)).astype(np.uint8)
            blit(base,im,W/2,H*y,a,sc)
    return base
def main():
    t0,t1=0,T_END
    out=OUT
    if PREVIEW: a,b=PREVIEW.split(','); t0,t1=float(a),float(b); out=os.path.join(T,'preview.mp4')
    n0,n1=int(t0*FPS),int(t1*FPS)
    cmd=['ffmpeg','-y','-loglevel','error','-f','rawvideo','-pix_fmt','rgb24','-s','%dx%d'%(W,H),'-r',str(FPS),'-i','-','-ss',str(t0),'-i',os.path.join(T,'music.wav'),
         '-c:v','libx264','-preset','medium','-crf','18','-pix_fmt','yuv420p','-c:a','aac','-b:a','192k','-shortest','-movflags','+faststart',out]
    p=subprocess.Popen(cmd,stdin=subprocess.PIPE)
    for ti in range(n0,n1):
        p.stdin.write(render_frame(ti).tobytes())
        if ti%300==0: print('%.1fs'%(ti/FPS),flush=True)
    p.stdin.close(); p.wait(); print('wrote',out)
if __name__=='__main__': main()
