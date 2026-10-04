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
out.append("enum{C_WHITE=30,C_RED,C_DRED,C_GREEN,C_GOLD,C_GREY,C_DGREY,C_CYAN};")
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
v=np.where(d2>0.93,v-0.6*(d2-0.93)/0.07,v)
img=1+quant(np.clip((yy/H)*1.2,0,1),yy,xx)
for _ in range(90):
    x,y=rng.integers(0,W),rng.integers(18,H); img[y,x]=5 if rng.random()<.35 else 4
img[(d2>=1)&(d2<1.03)]=21
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
sprs=[("BUGSIDE",BUG,32,16,1),("SHIP",SHIP,16,16,1),("DOOR",ARCH,16,16,1),("CAGE",CAGE,16,8,1),("SLUG",SLUG,16,8,1),
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
    'r':216,'y':218,'f':rb2['FLAME']+5,'F':rb2['FLAME']+7,'h':rb2['STONE']+3,'H':rb2['STONE']+5}
ART={
 'BERRY':["...gG...","..g.....",".k1111k.","k1w1111k","k111111k","k111111k",".k1111k.","..kkkk.."],
 'SEED':["...E....","..EEe...",".EeeEe..",".eeeee..",".deeed..","..ddd..."],
 'SHELL':["...kkkk...","..kSSssk..",".kSsssstk.",".ksssstttk","kssttttttk","kkkkkkkkkk"],
 'THORN':["...w..","...S..","..sS..","..ss..",".kst..",".kst..","kxoox.","kxxxx."],
 'CRUMB':["..kkkk..",".kCCCck.","kCccccbk","kcccbbbk",".kkkkkk."],
 'WARP':["...Q...","..QqQ..",".QqqnQ.","QqqnnnQ",".qqnnn.","..qnn..","...n..."],
 'GEM':["..kkkk..",".k1w11k.","k1w1111k","k111111k",".k1111k.","..k11k..","...kk..."],
 'BAT':["..x..............x..",".xox............xox.","xoOox..........xoOox","xoOOox..xxxx..xoOOox",".xoOOoxxoooooxxoOOox.".rstrip('.')+'.',"..xoOOoxroorxoOOox..","...xxooooooooooxx...","....xx.oooooo.xx....","......xxo..oxx......",".......x.oo.x.......","..........x........."],
 'LAMP':["....h....","....H....","....h....","....H....","....h....","...hHh...","..hHHHh..",".hfFFFfh.",".hfFFFfh.",".hffFffh.","..hhhhh..","...hHh..."],
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
' ':"000000000000000","'":"010010000000000",'/':"001001010100100",'<':"001010100010001",'>':"100010001010100",'^':"010101000000000",'*':"101010111010101"}
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
open("assets.h","w").write("// made by gen.py\n"+"\n".join(out)+"\n")
print("tiles",t,"bytes",len(allt))
