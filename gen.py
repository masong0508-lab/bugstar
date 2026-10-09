# Makes assets.h for BUGSTAR: planet picture, palettes, sprites, font, tables.
import numpy as np, math
W,H=240,160
B4=np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]])/16.0
def quant(v,yy,xx):
    v=np.clip(v,0,4); lo=np.floor(v); fr=v-lo
    return np.clip(lo+(fr>B4[yy%4,xx%4]),0,4).astype(int)
def c15(r,g,b): return (r>>3)|((g>>3)<<5)|((b>>3)<<10)
out=[]
def arr(name,typ,vals,per=24):
    out.append("static const %s %s[%d]={"%(typ,name,len(vals)))
    for i in range(0,len(vals),per): out.append(",".join(str(int(v)) for v in vals[i:i+per])+",")
    out.append("};")
# ---------------- BG palette (256)
pal=[(0,0,0)]*256
SPACE=[(6,6,16),(18,16,38),(40,34,70),(120,110,160),(240,236,255)]
for i,c in enumerate(SPACE): pal[1+i]=c
pal[6]=(46,56,86)          # HUD line
HUD={'white':(232,230,222),'red':(232,72,80),'dred':(70,30,40),'green':(120,200,110),'gold':(255,210,90),'grey':(120,130,150),'dgrey':(60,64,80),'cyan':(140,220,255)}
for i,k in enumerate(HUD): pal[30+i]=HUD[k]
out.append("enum{C_WHITE=30,C_RED,C_DRED,C_GREEN,C_GOLD,C_GREY,C_DGREY,C_CYAN,C_UNIQ};")
pal[38]=(255,140,50)       # unique items
TINTS=[("THE GARDEN",[(10,22,18),(30,62,40),(68,118,62),(132,176,92),(214,232,160)],(60,110,90)),
       ("THE POND",[(6,14,28),(20,46,86),(44,96,150),(110,170,214),(210,240,255)],(70,130,190)),
       ("THE PICNIC",[(26,6,10),(82,16,26),(150,36,48),(214,90,96),(255,200,190)],(150,60,70)),
       ("THE NIGHT LAWN",[(10,6,20),(34,20,60),(70,46,110),(124,96,170),(200,180,240)],(90,60,150)),
       ("THE BEEHIVE",[(26,16,4),(86,52,10),(160,104,24),(222,170,60),(255,236,150)],(160,120,50)),
       ("THE ANTHILL",[(20,10,6),(64,30,16),(120,60,30),(180,104,60),(236,170,120)],(130,70,40))]
MON=[[(8,14,8),(30,60,24),(70,120,40),(140,190,70),(255,240,120)],
     [(6,8,20),(20,40,90),(40,90,160),(110,170,230),(255,230,120)],
     [(16,6,8),(80,20,30),(160,40,50),(230,110,100),(255,240,140)],
     [(14,8,16),(70,22,58),(146,44,92),(222,98,120),(255,226,110)],
     [(10,8,4),(60,50,10),(200,160,30),(250,220,90),(255,255,220)],
     [(20,6,4),(90,24,10),(170,50,20),(230,110,60),(255,220,120)]]
tv=[]
for name,ramp,atm in TINTS:
    for c in ramp+[atm]: tv.append(c15(*c))
arr("tint_pal","unsigned short",tv)
out.append("#define NTINT %d"%len(TINTS))
out.append("static const char* const tint_name[]={%s};"%",".join('"%s"'%t[0] for t in TINTS))
for i,c in enumerate(TINTS[0][1]): pal[16+i]=c
pal[21]=TINTS[0][2]
# dungeon ramps, 8 shades each, from 5 key colours
def ramp8(keys):
    keys=np.array(keys,float); r=[]
    for k in range(8):
        t=k/7*4; i=min(3,int(t)); f=t-i; r.append(tuple(int(x) for x in keys[i]*(1-f)+keys[i+1]*f))
    return r
RAMPS={'WALL':[(20,14,18),(62,38,34),(118,72,52),(182,124,80),(240,200,142)],
       'COOL':[(12,14,28),(34,46,80),(66,96,142),(120,164,204),(206,234,246)],
       'PILLAR':[(22,14,12),(78,48,30),(146,100,54),(212,166,94),(252,228,166)],
       'FLOORA':[(12,12,12),(44,42,40),(84,80,74),(132,126,116),(196,190,176)],
       'FLOORB':[(12,12,14),(40,40,44),(78,78,84),(124,124,132),(186,186,196)],
       'CEIL':[(8,8,10),(30,28,30),(62,58,58),(104,98,94),(160,152,144)],
       'UP':[(8,20,30),(20,70,96),(50,140,180),(120,210,240),(230,250,255)],
       'DOWN':[(14,6,20),(40,14,50),(80,30,90),(140,60,150),(210,130,220)],
       'SPIDER':[(14,8,16),(70,22,58),(146,44,92),(222,98,120),(255,226,110)],
       'CHEST':[(18,10,8),(86,48,24),(150,92,40),(214,150,62),(255,226,120)],
       'STONE':[(14,16,22),(46,50,64),(92,98,116),(150,156,172),(214,220,232)],
       'EARTH':[(16,10,6),(52,34,20),(98,66,38),(150,104,60),(206,160,104)],
       'MOSS':[(8,16,10),(26,52,30),(54,98,48),(104,150,70),(180,214,120)],
       'WOOD':[(20,10,4),(66,36,14),(122,72,30),(178,114,52),(232,170,92)],
       'CRYSTAL':[(6,10,24),(20,40,90),(40,110,180),(100,200,240),(220,250,255)],
       'FLAME':[(90,24,0),(170,60,0),(240,130,20),(255,200,60),(255,250,210)],
       'DIRT':[(14,10,6),(46,32,20),(86,62,40),(130,98,64),(186,150,104)],
       'TILEF':[(10,12,18),(34,38,52),(70,76,96),(120,128,150),(190,198,214)],
       'WATER':[(4,10,24),(10,34,70),(24,70,130),(70,140,200),(180,230,255)]}
base=64
for i,(k,keys) in enumerate(RAMPS.items()):
    for j,c in enumerate(ramp8(keys)): pal[base+i*8+j]=c
    out.append("#define R_%s %d"%(k,base+i*8))
mv=[]
for keys in MON: mv+= [c15(*c) for c in ramp8(keys)]
arr("zone_mon_pal","unsigned short",mv)
BERRY=[(220,40,50),(60,90,230),(250,200,50),(40,30,50),(240,240,250),(250,130,190),(80,200,90),(150,60,200)]
BUGC=[(110,120,160),(230,50,50),(255,240,120),(250,200,40),(90,200,80),(200,70,40)]
for i,c in enumerate(BERRY): pal[216+i]=c
for i,c in enumerate(BUGC): pal[224+i]=c
pal[230]=(250,250,250)
POT={'POTR':(231,[(30,4,8),(110,16,30),(200,40,50),(250,110,100),(255,220,200)]),
     'POTY':(239,[(26,20,4),(100,80,10),(200,170,30),(250,230,90),(255,255,210)]),
     'POTP':(247,[(20,6,30),(80,20,110),(160,60,200),(220,140,240),(255,230,255)])}
for k,(b0,keys) in POT.items():
    for j,c in enumerate(ramp8(keys)): pal[b0+j]=c
arr("bg_pal","unsigned short",[c15(*c) for c in pal])
# ---------------- planet picture (palette indices)
rng=np.random.default_rng(11)
yy,xx=np.mgrid[0:H,0:W]
cx,cy,R=128,292,232
dx,dy=(xx-cx)/R,(yy-cy)/R; d2=dx*dx+dy*dy; inside=d2<1
nz=np.sqrt(np.clip(1-d2,0,1)); L=np.array([-.5,-.65,.57]); L/=np.linalg.norm(L)
lam=np.clip(dx*L[0]+dy*L[1]+nz*L[2],0,1)
pat=(np.sin(dx*9+dy*3)+np.sin(dy*11-nz*5)+np.sin(nz*13+dx*4))/3
v=0.35+3.4*lam+0.35*pat
v=np.where(d2>0.86,v-0.9*(d2-0.86)/0.14,v)
img=1+quant(np.clip((yy/H)*1.2,0,1),yy,xx)
for _ in range(90):
    x,y=rng.integers(0,W),rng.integers(18,H); img[y,x]=5 if rng.random()<.35 else 4
img[(d2>=1)&(d2<1.03)]=21
glow1=(d2>=1.03)&(d2<1.075)&(((xx+yy)&1)==0); img[glow1]=21
glow2=(d2>=1.075)&(d2<1.12)&(((xx+yy)&3)==0); img[glow2]=21
img[inside]=16+quant(v,yy,xx)[inside]
img[0:17,:]=0; img[17,:]=6
arr("planet_img","unsigned char",img.flatten().tolist(),per=40)
# ---------------- OBJ palette + sprites (4bpp)
OP=[(0,0,0),(14,16,28),(46,56,86),(92,108,148),(152,172,210),(232,236,248),(70,52,40),(128,100,76),(190,160,120),(232,80,72),(110,200,255),(40,90,40),(90,170,70),(250,210,90),(196,170,80),(150,156,176)]
arr("obj_pal","unsigned short",[c15(*c) for c in OP])
# chars: . clear; 1 black 2-4 bug blues 5 white 6-8 browns 9 red a blue b-c greens d yellow e slug f grey
def art(rows,w,h,scale=1):
    g=np.zeros((h,w),int)
    hh=len(rows)*scale; ww=len(rows[0])*scale
    oy=h-hh; ox=(w-ww)//2
    for j,r in enumerate(rows):
        for i,c in enumerate(r):
            if c=='.': continue
            v=int(c,16)
            g[oy+j*scale:oy+j*scale+scale, ox+i*scale:ox+i*scale+scale]=v
    return g
def tiles(g):
    h,w=g.shape; b=[]
    for ty in range(h//8):
        for tx in range(w//8):
            for y in range(8):
                for x in range(0,8,2):
                    b.append(g[ty*8+y,tx*8+x] | (g[ty*8+y,tx*8+x+1]<<4))
    return b
BUG=["..1.........1........",
     "...1.......1.........",
     "....1111111111.......",
     "..11455444444411.....",
     ".1453344333443341....",
     "14332233322332231....",
     "1322213322213222311..",
     "13222132221322213511.",
     "1222212222212222121.1",
     ".11111111111111111...",
     "..1.1..1.1..1.1..1...",
     ".1..1.1..1.1..1..1...",]
SHIP=["......11......",
      ".....1f51.....",
      "....1f5ff1....",
      "....1aaff1....",
      "...1fafff1....",
      "...1f5fff1....",
      "..1f5ffff31...",
      ".191f5fff319..",
      "1991333333199.",
      "191.111111.191"]
ARCH=["....11111111....",
      "..11788888711..",
      ".17881111118871.",
      "1788100000018871",
      "1781000000001871",
      "17810000a0001871",
      "1781000000001871",
      "1771000000001771",
      "1661000000001661",
      "1111000000001111"]
CAGE=["8888888888",
      "8.d.d.d.d8",
      "8.d99d.d.8",
      "8.d1991d.8",
      "8.d99dd.d8",
      "8888888888"]
SLUG=["..........1..1",
      "...........11.",
      "....1111111e81",
      "..11ee88888e1.",
      ".1d7eeeeeeeee1",
      "117777777777711"[:14]]
PLANT=[".c.d.","c.b.c",".bbb.","..b..","..1.."]
PEBBLE=["..ff.",".f8f7","17777"]
TUFT=["c.c.c",".cbc.","1bbb1"]
BACK=["......1........1......",
      ".......1......1.......",
      ".....111111111111.....",
      "...1144444444444411...",
      "..143333333333333341..",
      ".14322222222222222341.",
      ".11111111111111111111.",
      "1433333333333333333341",
      "1322222222222222222231",
      "1111111111111111111111",
      "1433333333333333333341",
      "1322222222222222222231",
      ".11111111111111111111.",
      ".13222222222222222231.",
      "..112222222222222211..",
      "....11111111111111....",
      "...1..1..1..1..1..1...",]

# ---- the player, redrawn: your jaws in first person (one jaw; the other is the same art mirrored), and a side view with 2 walking frames
def gen_jaw():
    import math
    W=32; g=[['.']*W for _ in range(W)]
    P=[(-3,36),(5,19),(15,4),(28,9)]
    def bez(t):
        a=(1-t)**3; b=3*(1-t)**2*t; c=3*(1-t)*t*t; d=t**3
        return (a*P[0][0]+b*P[1][0]+c*P[2][0]+d*P[3][0], a*P[0][1]+b*P[1][1]+c*P[2][1]+d*P[3][1])
    pts=[bez(i/200) for i in range(201)]
    for y in range(W):
        for x in range(W):
            best=1e9; bi=0
            for i,(px,py) in enumerate(pts):
                d=(x+0.5-px)**2+(y+0.5-py)**2
                if d<best: best=d; bi=i
            t=bi/200; px,py=pts[bi]
            tx,ty = (pts[min(bi+1,200)][0]-pts[max(bi-1,0)][0], pts[min(bi+1,200)][1]-pts[max(bi-1,0)][1])
            l=math.hypot(tx,ty) or 1; nx,ny = ty/l, -tx/l        # normal pointing up-left (the outside)
            off=((x+0.5-px)*nx+(y+0.5-py)*ny)                    # + outside, - inside (toward the middle of the screen)
            w = 6.6 if t<0.38 else 6.6-(t-0.38)/0.62*5.9
            tooth = 1.5 if (0.50<t<0.85 and (t*15)%1<0.45) else 0
            lim_in = w + tooth
            if off>w or -off>lim_in: continue
            if t<0.02: continue
            edge = off>w-1.0 or -off>lim_in-1.0
            if edge: g[y][x]='1'; continue
            if -off>w: g[y][x]='8'; continue                    # teeth
            sh = 0.5 + off/(2*w)
            tipc = t>0.66
            if t<0.45 and (t*30)%3<0.35: g[y][x]='2' if sh>0.4 else '1'; continue     # joints in the arm
            if tipc: c = '8' if sh>0.6 else '7' if sh>0.3 else '6'
            else: c = '5' if sh>0.86 else '4' if sh>0.62 else '3' if sh>0.32 else '2'
            g[y][x]=c
    return [''.join(r) for r in g]
JAW=gen_jaw()
def gen_side(frame,pattern=0):
    import math
    W,H=28,14; g=[['.']*W for _ in range(H)]
    cx,cy,a,b=11.0,10.6,10.0,8.6
    for y in range(H):
        for x in range(W):
            nx=(x+0.5-cx)/a; ny=(y+0.5-cy)/b
            if y<=10 and nx*nx+ny*ny<=1.0:
                e = nx*nx+ny*ny > 0.80 or y==10
                if e: g[y][x]='1'; continue
                lit = -nx*0.45 - ny*0.95
                band = int((x+0.5-cx + 1.5*ny*ny*3)//3)
                seam = ((x+1)%4==0) and y>2
                c = 5 if lit>0.75 else 4 if lit>0.42 else 3 if lit>0.05 else 2
                if seam: c=max(2,c-1)
                if pattern==1 and (x//3)%2==0 and 2<y<10: c=max(2,c-2)                     # stripes
                if pattern==2 and ((x*5+y*3)%7)==0 and 1<y<9 and 2<x<19: c=max(2,c-2)      # spots
                if pattern==3 and y>=6 and y<10: c=min(5,c+1)                               # a pale belly band
                g[y][x]=str(c)
    for y in range(H):                                   # the head
        for x in range(W):
            if (x+0.5-21.5)**2/(3.2**2)+(y+0.5-8.5)**2/(2.6**2)<=1 and g[y][x] in '.1':
                e=(x+0.5-21.5)**2/(3.2**2)+(y+0.5-8.5)**2/(2.6**2)>0.62
                g[y][x]='1' if e else ('3' if y<8 else '2')
    g[8][22]='5'; g[7][22]='1'
    for i,(x,y) in enumerate([(23,5),(24,4),(25,3),(26,2),(27,2)]): g[y][x]='1' if i<4 else '3'      # antenna
    for i,(x,y) in enumerate([(22,5),(22,4),(23,3),(23,2)]): g[y][x]='1'
    for k,lx in enumerate([3,6,9,12,15,18]):            # legs: they swap between the two frames
        fwd = (k+frame)%2==0
        g[11][lx]='1'; g[12][lx+(1 if fwd else -1)]='1'; g[13][lx+(1 if fwd else -1)]='1'
    return [''.join(r) for r in g]
BUG=[r[::-1] for r in gen_side(0)]; BUG2=[r[::-1] for r in gen_side(1)]      # drawn facing right; the game mirrors it
BUGP=[[ [r[::-1] for r in gen_side(f,p)] for f in (0,1)] for p in (1,2,3)]      # the other shell patterns
ARROW=["................",
       "................",
       "................",
       "........1.......",
       "........11......",
       "........1d1.....",
       ".111111.1dd1....",
       ".1ddddd11ddd1...",
       ".1ddddddddddd1..",
       ".1ddddd11ddd1...",
       ".111111.1dd1....",
       "........1d1.....",
       "........11......",
       "........1.......",
       "................",
       "................"]
SHADOW=["....1.1.1.1.....",
        "..1.1.1.1.1.1...",
        ".1.1.1.1.1.1.1..",
        "..1.1.1.1.1.1...",
        "....1.1.1.1.....",
        "................",
        "................",
        "................"]
CLOUD=["..........5.5.5.................",
       "......5.5.5.5.5.5.5.5.5.........",
       "...5.5.5.5.5.5.5.5.5.5.5.5.5....",
       ".5.5.5.5.5.5.5.5.5.5.5.5.5.5.5..",
       "5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.",
       ".4.4.5.5.5.5.5.5.5.5.5.5.5.5.4.4",
       "...4.4.4.4.4.4.4.4.4.4.4.4.4....",
       "......4.4.4.4.4.4.4.4.4........."]
HILL=["................................................................",
      "...............................5.5..............................",
      "......................5.5.5.5.5.5.5.5.5.5.5.....................",
      ".................5.5.5.5.5.5.5.5.5.5.5.5.5.5.5..................",
      "..............5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5...............",
      ".............5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5................",
      "..........5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.5.....................",
      ".........5.5.5.5.5.5.5.5.5.5.5.5.5.5............................",
      "........5.5.5.5.5.5.5.5.5.5.5...................................",
      ".......5.5.5.5.5.5.5.5.5........................................",
      "......5.5.5.5.5.5...............................................",
      ".....5.5.5.5....................................................",
      "....5...........................................................",
      "................................................................",
      "..........................................................b.b...",
      "...................................................b.b.b.b.b.b..",
      "..............................................b.b.b.b.b.b.b.b...",
      ".......................................b.b.b.b.b.b.b.b.b.b.b....",
      ".11...............................b.b.b.b.b.b.b.b.b.b.b.b.b.b11.",
      ".11........................b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.bbbb11.",
      "..11................b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.bbbbbbbb11..",
      "...11..........b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.bbbbbbbbbbbbbb11...",
      "...111..b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.b.bbbbbbbbbbbbbbbbbb111...",
      "....111b.b.b.b.b.b.b.b.b.b.b.b.b.bbbbbbbbbbbbbbbbbbbbbbbb111....",
      "......11b.b.b.b.b.b.b.b.b.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb11......",
      ".......111.b.b.b.b.b.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb111.......",
      ".........111b.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb111.........",
      "...........111bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb111...........",
      ".............1111bbbbbbbbbbbbbbbbbbbbbbbbbbbbbb1111.............",
      ".................1111bbbbbbbbbbbbbbbbbbbbbb1111.................",
      ".....................111111111bbbb111111111.....................",
      "................................................................"]
# resource nodes you can mine on planets: an amber tree, a crystal rock, a silk web, a nectar flower
TREE=["......1d1.......",
      ".....1ddd1......",
      "....1d8dd81.....",
      "...1dd8ddd81....",
      "...1d8dddd81....",
      "....1ddd8d1.....",
      ".....1ddd1......",
      "......171.......",
      "......171.......",
      ".....1771.......",
      "....117711......",
      "...1.1661.1.....",
      "........1......."]
ROCK=["......1a1.......",
      ".....1a5a1......",
      "....1aa5aa1.....",
      "...1aaa5aaa1....",
      "..1aa5aaaaaa1...",
      ".1aaaa5aaa5aa1..",
      ".1ffaaaaaaaff1..",
      "..1ffffffff1....",
      "...11111111....."]
WEB=["1.......1.......",
     ".1.....1........",
     "..1.5.1.........",
     "...555..........",
     "..5.5.5.........",
     ".5..5..5........",
     "5...5...5.......",
     "....5...........",
     "....5...........",
     "....5..........."]
FLOWER=["......e5e.......",
        ".....e5f5e......",
        ".....5fdf5......",
        ".....e5f5e......",
        "......e5e.......",
        "......1b1.......",
        ".....1bbb1......",
        "......1b1.......",
        "...b..1b1..b....",
        "....bb1b1bb.....",
        "......1b1......."]
HATCH=["......1111......",
       "....11ffff11....",
       "...1ffffffff1...",
       "..1fff9999fff1..",
       "..1ff999999ff1..",
       ".1fff999999fff1.",
       ".1ffff9999ffff1.",
       ".1ffffffffffff1.",
       "1ff1ffffffff1ff1",
       "1f1111111111 1f1".replace(" ","1"),
       "1111111111111111",
       "................"]
BOLT=["....11111.......",
      "...1fffff1......",
      "..1f5f5fff1.....",
      "..1fffffff1.....",
      "..1f1111ff1.....",
      "...1a1a1f1......",
      "..1fffffff1.....",
      ".1fffafffff1....",
      ".1ff1fff1ff1....",
      ".1ff1fff1ff1....",
      "..1fffffff1.....",
      "...1111111......",
      "..1f1...1f1.....",
      "..111...111....."]
# city and planet life: a shop doorway with a sign, two townsfolk, a race flag, a fossil, a surface beetle, a market stall, an old ruin
DOORWAY=["...1111111111...",
         "..1dddddddddd1..",
         "..1d999999991d..",
         "..1dddddddddd1..",
         "...1111111111...",
         "...1ff1111ff1...",
         "...1f166661f1...",
         "...1f166661f1...",
         "...1f166661f1...",
         "...1f166d61f1...",
         "...1f166661f1...",
         "...1f166661f1...",
         "...1f166661f1...",
         "..11111111111111"]
LOCAL1=["....11..........",
        "...1dd1.1.1.....",
        "...1dd1.1.1.....",
        "....11..1.1.....",
        "...1a1..1.1.....",
        "..1aaa111.1.....",
        "..1a5a1.........",
        "..1aaa1.........",
        ".1aaaaa1........",
        ".1a999a1........",
        ".1a999a1........",
        "..1aaa1.........",
        "..1.1.1.........",
        ".1..1..1........"]
LOCAL2=["................",
        "....1.1.........",
        "...1.1..........",
        "...11111........",
        "..1c5c5c1.......",
        "..1ccccc1.......",
        "...11111........",
        "..1bbbbb1.......",
        ".1bbdbdbb1......",
        ".1bbbbbbb1......",
        ".1bbdbdbb1......",
        "..1bbbbb1.......",
        ".1.1...1.1......",
        "1..1...1..1....."]
BEACON=["1dd1............",
        "1dddd1..........",
        "1dd9dd1.........",
        "1dddd1..........",
        "1dd1............",
        "1...............",
        "1...............",
        "1...............",
        "1...............",
        "1...............",
        "1...............",
        "1...............",
        "1...............",
        "11..............",
        "111.............",
        "1111............"]
FOSSIL=["................",
        "......1.........",
        ".....151.1......",
        "..1..1551551....",
        ".151.15555551...",
        ".155115555551...",
        ".1555555115551..",
        ".15515551.1551..",
        "..11.1551..11...",
        ".....1551.......",
        "..1111111111....",
        ".18887777777881.",
        "..111111111111.."]
SBEETLE=["......1111......",
         ".....166661.....",
         "....16966961....",
         "...1666666661...",
         "..1.1.1661.1.1..",
         ".1.166666666.1..",
         "..1766666667....",
         "..1766666667....",
         "...1.166661.1...",
         "..1..1....1..1..",
         ".1..1......1..1."]
STALL=["..111111111111..",
       ".19d9d9d9d9d9d1.",
       "1dddddddddddddd1",
       "1..1........1..1",
       "...1..1111..1...",
       "...1.1dc5c1.1...",
       "...1.1cd5d1.1...",
       "...1111111111...",
       "...1777777771...",
       "...1777777771...",
       "...1..1..1..1...",
       "...1..1..1..1..."]
RUIN=["................",
      "....1ff1........",
      "...1ffff1.......",
      "...1f5ff1.......",
      "...1ffff1..11...",
      "...1f5ff1.1ff1..",
      "...1ffff1.1f51..",
      "..1ffffff11ff1..",
      ".1ffff5fffffff1.",
      ".1ffffffff5fff1.",
      "..1bb1bbb1bbb1..",
      "...11..11..11..."]
# creatures: three body plans, tinted per planet (colours 6,7,8 become the species' colours)
HOPPER=["................",
        "......1.1.......",
        ".....1.1........",
        "....11111.......",
        "...1777771......",
        "..17751777111...",
        "..1777777776661.",
        "..1777777776661.",
        "...1777777111...",
        "....1177771.....",
        "...1.1..1.1.....",
        "..1..1..1..1...."]
GRAZER=["................",
        "..............1.",
        ".1111........1..",
        "1677771111111...",
        "167577766666661.",
        "1677777666666661",
        ".1177777666666661",
        "..11777766666661",
        "....1661.1661...",
        "....1661.1661...",
        "....1661.1661...",
        "....111..111...."]
FLYER=["..1111....1111..",
       ".18888111888881.",
       "1888866666688881",
       ".1111677776111..",
       ".....1675761....",
       "......16661.....",
       ".......111......",
       "................"]
sprs=[("HOPPER",HOPPER,16,16,1),("GRAZER",GRAZER,16,16,1),("FLYER",FLYER,16,8,1),("DOORWAY",DOORWAY,16,16,1),("LOCAL1",LOCAL1,16,16,1),("LOCAL2",LOCAL2,16,16,1),("BEACON",BEACON,16,16,1),("FOSSIL",FOSSIL,16,16,1),("SBEETLE",SBEETLE,16,16,1),("STALL",STALL,16,16,1),("RUIN",RUIN,16,16,1),("ARROW",ARROW,16,16,1),("HATCH",HATCH,16,16,1),("BOLT",BOLT,16,16,1),("TREE",TREE,16,16,1),("ROCK",ROCK,16,16,1),("WEB",WEB,16,16,1),("FLOWER",FLOWER,16,16,1),("HILL",HILL,64,32,1),("SHADOW",SHADOW,16,8,1),("CLOUD",CLOUD,32,8,1),("BUGSIDE",BUG,32,16,1),("BUGSIDE2",BUG2,32,16,1),("BUGP1A",BUGP[0][0],32,16,1),("BUGP1B",BUGP[0][1],32,16,1),("BUGP2A",BUGP[1][0],32,16,1),("BUGP2B",BUGP[1][1],32,16,1),("BUGP3A",BUGP[2][0],32,16,1),("BUGP3B",BUGP[2][1],32,16,1),("JAW",JAW,64,64,2),("SHIP",SHIP,16,16,1),("DOOR",ARCH,16,16,1),("CAGE",CAGE,16,8,1),("SLUG",SLUG,16,8,1),
      ("PLANT",PLANT,8,8,1),("PEBBLE",PEBBLE,8,8,1),("TUFT",TUFT,8,8,1),("BACK",BACK,64,64,2)]
allt=[]; t=0
for name,rows,w,h,s in sprs:
    g=art(rows,w,h,s); out.append("#define T_%s %d"%(name,512+t)); bts=tiles(g); allt+=bts; t+=len(bts)//32
words=[allt[i]|(allt[i+1]<<8) for i in range(0,len(allt),2)]
arr("obj_tiles","unsigned short",words)
# ---------------- software sprites for the dungeon (8bpp palette indices, 0 = clear)
SPIDER=["......1......1......",
        ".....1........1.....",
        "....1..122221..1....",
        "...1..12333321..1...",
        "..1..1233443321..1..",
        ".1..123344443321..1.",
        "1..12333333333321..1",
        "...12322333322321...",
        "..1.123253352321.1..",
        ".1..122333333221..1.",
        "1....1222222221....1",
        "....1..1....1..1....",
        "...1..1......1..1...",
        "..1..1........1..1.."]
CHEST=["..111111111111..",
       ".14444444444441.",
       "1433333333333341",
       "1433333333333341",
       "1111111111111111",
       "1322222552222231",
       "1322222112222231",
       "1322222222222231",
       "1322222222222231",
       "1111111111111111"]
def soft(rows,rampbase):
    m={'1':0,'2':2,'3':4,'4':6,'5':7}
    return [0 if c=='.' else rampbase+m[c] for r in rows for c in r]
rb={k:base+i*8 for i,k in enumerate(RAMPS)}
arr("spider_art","unsigned char",soft(SPIDER,rb['SPIDER'])); out.append("#define SPIDER_W 20\n#define SPIDER_H 14")
def legs_step(rows):          # second walking frame: legs swap forward and back
    out_=[]
    for j,r in enumerate(rows):
        r=list(r); n=len(r); new=['.']*n
        for i,c in enumerate(r):
            if c=='.': continue
            body = 4<=i<=15 and 2<=j<=10
            if body: new[i]=c; continue
            d = (1 if i<n//2 else -1) * (1 if j%2 else -1)
            k=min(n-1,max(0,i+d)); new[k]=c
        out_.append(''.join(new))
    return out_
arr("spider_art2","unsigned char",soft(legs_step(SPIDER),rb['SPIDER']))
BEETLE=[".........11.........",
        "........1221........",
        "........1341........",
        "....111114411111....",
        "...12222222222221...",
        "..1233333333333321..",
        ".123344444444443321.",
        ".123345344443543321.",
        "12333333333333333321",
        ".1.1222222222221.1..",
        "1.1..1.1...1.1..1.1.",
        ".1..1...1.1...1..1.."]
BEETLE=[r.ljust(20,'.') for r in BEETLE]
arr("beetle_art","unsigned char",soft(BEETLE,rb['SPIDER'])); arr("beetle_art2","unsigned char",soft(legs_step(BEETLE),rb['SPIDER']))
out.append("#define BEETLE_W 20\n#define BEETLE_H 12")
arr("chest_art","unsigned char",soft(CHEST,rb['CHEST'])); out.append("#define CHEST_W 16\n#define CHEST_H 10")
def soft2(rows,cmap):
    return [cmap.get(c,0) for r in rows for c in r]
rb2={k:base+i*8 for i,k in enumerate(RAMPS)}
CM={'.':0,'1':1,'k':rb2['CEIL'],'w':230,'g':rb2['MOSS']+5,'G':rb2['MOSS']+3,
    'e':rb2['EARTH']+3,'E':rb2['EARTH']+5,'d':rb2['EARTH']+1,
    's':rb2['STONE']+4,'S':rb2['STONE']+6,'t':rb2['STONE']+2,
    'o':rb2['WOOD']+3,'O':rb2['WOOD']+5,'x':rb2['WOOD']+1,
    'c':rb2['DIRT']+5,'C':rb2['DIRT']+7,'b':rb2['DIRT']+3,
    'q':rb2['CRYSTAL']+4,'Q':rb2['CRYSTAL']+7,'n':rb2['CRYSTAL']+2,
    'r':216,'y':218,'Y':rb2['CHEST']+6,'f':rb2['FLAME']+5,'F':rb2['FLAME']+7,'h':rb2['STONE']+3,'H':rb2['STONE']+5}
ART={
 'BERRY':["...gG...","..g.....",".k1111k.","k1w1111k","k111111k","k111111k",".k1111k.","..kkkk.."],
 'SEED':["...E....","..EEe...",".EeeEe..",".eeeee..",".deeed..","..ddd..."],
 'SHELL':["...kkkk...","..kSSssk..",".kSsssstk.",".ksssstttk","kssttttttk","kkkkkkkkkk"],
 'THORN':["...w..","...S..","..sS..","..ss..",".kst..",".kst..","kxoox.","kxxxx."],
 'CRUMB':["..kkkk..",".kCCCck.","kCccccbk","kcccbbbk",".kkkkkk."],
 'WARP':["...Q...","..QqQ..",".QqqnQ.","QqqnnnQ",".qqnnn.","..qnn..","...n..."],
 'GEM':["..kkkk..",".k1w11k.","k1w1111k","k111111k",".k1111k.","..k11k..","...kk..."],
 'BAT':["..x..............x..",".xox............xox.","xoOox..........xoOox","xoOOox..xxxx..xoOOox",".xoOOoxxoooooxxoOOox.".rstrip('.')+'.',"..xoOOoxroorxoOOox..","...xxooooooooooxx...","....xx.oooooo.xx....","......xxo..oxx......",".......x.oo.x.......","..........x........."],
 'BAT2':["....................","....................","....................","...xxxxx..xxxx..xxxx".replace('x','.'),".xxoOOoxxoooooxxoOOox","xoOOOoxroorxoOOOox..","xoOOooooooooooOOox..","xoOox..oooooo.xoOox.","xox....xo..oxx..xox.",".x.....x.oo.x....x..","..........x........."],
 'WASP':[".SS............SS.","SSSS..........SSSS",".SSSS..kkkk..SSSS.","...SSS.krrk.SSS...",".......kkkk.......","......YYYYYY......","......kkkkkk......","......YYYYYY......",".......kkkk.......",".......YYYY.......","........kk........","........k........."],
 'WASP2':["..................","..................",".......kkkk.......",".SS....krrk....SS.","SSSSSSSkkkkSSSSSSS",".SSSS.YYYYYY.SSSS.","......kkkkkk......","......YYYYYY......",".......kkkk.......",".......YYYY.......","........kk........","........k........."],
 'PET':["....k......k....",".....k....k.....","......kkkk......",".....kwkkwk.....","....kkkkkkkk....","...1111k11111...","..11k11k11k111..",".111111k1111111.",".11k111k111k111.",".111111k1111111.","..1111111111111.","..k.k..k..k.k..."],
 'LAMP':["....h....","....H....","....h....","....H....","....h....","...hHh...","..hHHHh..",".hfFFFfh.",".hfFFFfh.",".hffFffh.","..hhhhh..","...hHh..."],
 'BARREL':["...xxxxxx...","..xOOOOOOx..",".xhhhhhhhhx.",".xoOOoOOoox.","xooOOoOOooox","xoOOOoOOooox","xhhhhhhhhhhx","xoOOOoOOooox","xooOOoOOooox",".xoOOoOOoox.",".xhhhhhhhhx.","..xoooooox..","...xxxxxx..."],
 'CRATE':["xxxxxxxxxxxx","xOOOOOOOOOOx","xOxooooooxOx","xOoxooooxoOx","xOooxooxooOx","xOoooxxoooOx","xOoooxxoooOx","xOooxooxooOx","xOoxooooxoOx","xOxooooooxOx","xOOOOOOOOOOx","xxxxxxxxxxxx"],
 'CAGE':["kxxxxxxxxxxk","kOOOOOOOOOOk",".o.o.o.o.o..",".o.o.o.o.o..",".o.o11.o.o..",".o.1111o.o..",".o11k11.o.o.",".o.1111o.o..",".o.o11.o.o..",".o.o.o.o.o..","kOOOOOOOOOOk","kxxxxxxxxxxk"]}
for k,rows in ART.items():
    w=max(len(r) for r in rows); rows=[r.ljust(w,'.') for r in rows]
    arr("art_"+k.lower(),"unsigned char",soft2(rows,CM)); out.append("#define %s_W %d\n#define %s_H %d"%(k,w,k,len(rows)))
# ---------------- font 3x5
F={'A':"111101111101101",'B':"110101110101110",'C':"111100100100111",'D':"110101101101110",'E':"111100110100111",'F':"111100110100100",
'G':"111100101101111",'H':"101101111101101",'I':"111010010010111",'J':"001001001101111",'K':"101101110101101",'L':"100100100100111",
'M':"101111111101101",'N':"110101101101101",'O':"111101101101111",'P':"111101111100100",'Q':"111101101111001",'R':"110101110101101",
'S':"111100111001111",'T':"111010010010010",'U':"101101101101111",'V':"101101101101010",'W':"101101101111101",'X':"101101010101101",
'Y':"101101111010010",'Z':"111001010100111",'0':"111101101101111",'1':"010110010010111",'2':"111001111100111",'3':"111001111001111",
'4':"101101111001001",'5':"111100111001111",'6':"111100111101111",'7':"111001001001001",'8':"111101111101111",'9':"111101111001111",
':':"000010000010000",'!':"010010010000010",'?':"111001011000010",'+':"000010111010000",'-':"000000111000000",'.':"000000000000010",
' ':"000000000000000","'":"010010000000000",'/':"001001010100100",'<':"001010100010001",'>':"100010001010100",'^':"010101000000000",'*':"101010111010101",',':"000000000010100",'(':"010100100100010",')':"010001001001010",'%':"101001010100101"}
fv=[0]*128
for ch,bits in F.items(): fv[ord(ch)]=int(bits,2)
arr("font","unsigned short",fv)
# ---------------- tables
arr("sin_tab","short",[round(math.sin(i*2*math.pi/256)*16384) for i in range(256)])
# lantern light by distance (index = dist in 1/16 tiles), result 0..127 shade units
lt=[]
for i in range(512):
    d=i/16; L=max(0.03,min(1.0,1.05-d/6.0)); lt.append(int(L*110))
arr("light_tab","unsigned char",lt)
arr("fog_tab","unsigned char",[int(max(0.12,1-(i/16)/15)*64) for i in range(512)])
sq=[];wv=[]
for m in range(128):
    f=440*2**((m-69)/12)
    sq.append(max(0,min(2047,round(2048-131072/f)))); wv.append(max(0,min(2047,round(2048-65536/f))))
arr("sq_freq","unsigned short",sq); arr("wv_freq","unsigned short",wv)
# ---- space: a lookup that turns a point on a disc into longitude/latitude on a sphere, and planet surface noise
su=[];sv=[];sz=[]
for j in range(128):
    for i in range(128):
        x=(i-63.5)/64; y=(j-63.5)/64; d=x*x+y*y
        if d>=1: su.append(0); sv.append(0); sz.append(0); continue
        z=math.sqrt(1-d); lon=math.atan2(x,z); lat=math.asin(y)
        su.append(int((lon/math.pi+1)*64)&127); sv.append(int((lat/math.pi+0.5)*63)&63); sz.append(int(z*255))
arr("sph_u","unsigned char",su,per=128); arr("sph_v","unsigned char",sv,per=128); arr("sph_z","unsigned char",sz,per=128)
def pnoise(seed,w=128,h=64):
    # smooth value noise that wraps around in longitude; ranked so half of it is "land" (>=8)
    r=np.random.default_rng(seed); acc=np.zeros((h,w))
    for cx,cy,amp in ((8,4,1.0),(16,8,.5),(32,16,.25),(64,32,.12)):
        g=r.random((cy+1,cx))
        xs=np.arange(w)*cx/w; ys=np.arange(h)*cy/h
        x0=xs.astype(int); fx=xs-x0; y0=ys.astype(int); fy=ys-y0
        fx=fx*fx*(3-2*fx); fy=fy*fy*(3-2*fy); x1=(x0+1)%cx
        a=g[y0][:,x0]; b=g[y0][:,x1]; c=g[y0+1][:,x0]; d=g[y0+1][:,x1]
        top=a*(1-fx)+b*fx; bot=c*(1-fx)+d*fx
        acc+=(top*(1-fy)[:,None]+bot*fy[:,None])*amp
    rank=acc.flatten().argsort().argsort()
    return [int(v*16//(w*h)) for v in rank]
# two planet surfaces, then one cloud layer (each 128 wide x 64 tall)
arr("pl_noise","unsigned char",pnoise(5)+pnoise(6)+pnoise(7),per=128)
import texgen
R=dict(rb2); R.update({k:v[0] for k,v in POT.items()})
TX=texgen.make_all(R)
for grp in ('wall','floor','ceil'):
    texels, ramps = TX[grp]
    arr(grp+"_tex","unsigned char",texels,per=48); arr(grp+"_ramp","unsigned short",ramps)
out.append("enum { %s };" % ", ".join("WT_"+n for n in texgen.WALLS))
open("assets.h","w").write("// made by gen.py\n"+"\n".join(out)+"\n")
print("tiles",t,"bytes",len(allt))
