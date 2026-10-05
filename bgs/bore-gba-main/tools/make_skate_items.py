#!/usr/bin/env python3
"""BORE skate-object sprite art: kicker ramp, quarter pipe, ledge, bench, grind rail.
Run:  python3 tools/make_skate_items.py   then   sh tools/bake_items.sh   (re-bakes the ROM sprites)
     (needs Pillow for the preview PNG only)
Writes source/skateart.h (the C data items.h bakes into sprites), source/rampdata.h (matching physics heights) and assets/preview/skate_items.png.
Art is authored here in the same 'textured boxes' model as items.h (8 units per tile, z in px, a = across, b = front).
Ramps are 8 one-unit slices stepping up 1 px at a time; each riser is painted in the colour of the top so the slope reads smooth.
"""
import os, sys
ROOT=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0,os.path.join(ROOT,'tools'))
IW,IH,IOX,IOY=21,28,10,22
KEY=None
def shade(c,n): return (c[0]*n//16,c[1]*n//16,c[2]*n//16)
def lift(c,num,den): return tuple(min(31,v*num//den) for v in c)

class Mat:
    def __init__(s,name,pal,rows): s.name,s.pal,s.rows=name,pal,rows; s.h=len(rows); s.w=len(rows[0])
    def px(s,col,row,n):
        col=max(0,min(s.w-1,col)); row=max(0,min(s.h-1,row)); ch=s.rows[row][col]
        return KEY if ch=='.' else shade(s.pal[ord(ch)-97],n)
class Box:
    def __init__(s,a0,b0,a1,b1,z0,z1,m): s.a0,s.b0,s.a1,s.b1,s.z0,s.z1,s.m=a0,b0,a1,b1,z0,z1,m
MATS=[]; PALS=[]
def pal(name,cols): PALS.append((name,cols)); return cols
def mat(name,p,rows):   # same name = same material (shared between the U and V variants of an object)
    for o in MATS:
        if o.name==name:
            assert o.rows==rows and o.pal is p, 'material %s redefined differently'%name
            return o
    m=Mat(name,p,rows); MATS.append(m); return m
OBJS=[]   # (name, boxes, outline shade, note)

# ------------------------------------------------------------------ kicker ramp (plywood, rises away from the viewer at rotation 0)
pKi =pal('pKi', [(7,4,2),(24,17,9),(20,13,6),(28,21,12),(12,7,3)])
pKiR=pal('pKiR',[lift(c,4,3) for c in pKi])        # risers get shade 12, so pre-lift 4/3 -> same as the top
def kicker():
    boxes=[]; hs=[8,7,6,5,4,3,2,1]                  # slice k: b=k..k+1, back (high) to front (low)
    for k,h in enumerate(hs):
        seam=(k%3==0)
        top=mat('mKiT%d'%k,pKi,['cccccccc' if seam else 'bbbbbbbb'])
        ris=mat('mKiR%d'%k,pKiR,['c' if seam else 'b'])
        side=mat('mKiS%d'%k,pKi,['e'])
        boxes.append(Box(0,k,8,k+1,0,h,[ris,side,side,side,top]))
    return boxes
OBJS.append(('Kicker',kicker(),11,'wedge, 8 px lip'))

# ------------------------------------------------------------------ quarter pipe (concave, steel coping on the lip)
pQp =pal('pQp', [(6,4,3),(22,15,8),(17,11,5),(27,19,11),(11,7,3),(21,22,25),(30,30,31)])
pQpR=pal('pQpR',[lift(c,4,3) for c in pQp])
QPH=[14,10,7,5,3,2,1,1]                              # slice k, back to front (physics uses the same table)
def qpipe():
    boxes=[]
    for k,h in enumerate(QPH):
        if k==0:
            top=mat('mQpT0',pQp,['ffffffff'])
            ris=mat('mQpR0',pQpR,['g','f']+['b','b','c','b','b','b','c','b','b','b','b','b'])    # coping lip, then panel
        else:
            seam=(k%2==0)
            top=mat('mQpT%d'%k,pQp,['cccccccc' if seam else 'bbbbbbbb'])
            ris=mat('mQpR%d'%k,pQpR,['c' if seam else 'b'])
        side=mat('mQpS%d'%k,pQp,['e'])
        boxes.append(Box(0,k,8,k+1,0,h,[ris,side,side,side,top]))
    return boxes
OBJS.append(('QuarterPipe',qpipe(),11,'concave, 14 px'))

# ------------------------------------------------------------------ ledge (concrete box, steel edges). Two variants: run along a (U) / along b (V)
pLe=pal('pLe',[(6,6,8),(16,16,18),(12,12,14),(27,28,30),(21,22,25)])
def ledge(alongU):
    side=mat('mLeSide',pLe,['eeeeeeee','bbbbbbbb','bbbcbbbb','bbbbbbbb','bcbbbbbb','bbbbbbbb'])
    if alongU: topr=['dddddddd']+['bbbbbbbb','bbcbbbbb','bbbbbbcb','bcbbbbbb','bbbbbbbb','bbbcbbbb']+['dddddddd']
    else:      topr=['dbbbbbbd','dbbcbbbd','dbbbbcbd','dcbbbbbd','dbbbbbbd','dbbbcbbd','dbbbbbbd','dbcbbbbd']
    topm=mat('mLeTop'+('U' if alongU else 'V'),pLe,topr)
    return [Box(0,0,8,8,0,6,[side,side,side,side,topm])]
OBJS.append(('LedgeU',ledge(True),12,'grind ledge, runs along a'))
OBJS.append(('LedgeV',ledge(False),12,'grind ledge, runs along b'))

# ------------------------------------------------------------------ bench (slatted seat on steel legs)
pBe=pal('pBe',[(6,4,2),(25,18,9),(13,8,4),(18,19,23),(29,22,12)])
def bench(alongU):
    leg=mat('mBeLeg',pBe,['d']); edge=mat('mBeEdge',pBe,['bbbbbbbb','cccccccc'])
    if alongU:
        topm=mat('mBeTopU',pBe,['bbbbbbbb','bbbbbbbb','cccccccc','bbbbbbbb'])
        return [Box(1,2,2,6,0,4,[leg]*5),Box(6,2,7,6,0,4,[leg]*5),Box(0,2,8,6,4,6,[edge,edge,edge,edge,topm])]
    topm=mat('mBeTopV',pBe,['bbcb']*8)
    return [Box(2,1,6,2,0,4,[leg]*5),Box(2,6,6,7,0,4,[leg]*5),Box(2,0,6,8,4,6,[edge,edge,edge,edge,topm])]
OBJS.append(('BenchU',bench(True),12,'bench, runs along a'))
OBJS.append(('BenchV',bench(False),12,'bench, runs along b'))

# ------------------------------------------------------------------ rail v2: base plate + post + bar (same grind height, 6 px)
pRl=pal('pRl',[(7,7,10),(19,20,24),(29,30,31),(30,26,5),(12,12,15)])
def rail(alongU):
    plate=mat('mRlPlate',pRl,['e']); post=mat('mRlPost',pRl,['a'])
    if alongU:
        lng=mat('mRlLong',pRl,['cccccccc','bbbbbbbb']); sh=mat('mRlShort',pRl,['cc','bb'])
        top=mat('mRlTopL',pRl,['cccccccc','cdcdcdcd'])
        return [Box(2,2,6,6,0,1,[plate]*5),Box(3,3,5,5,1,4,[post]*5),Box(0,3,8,5,4,6,[lng,sh,lng,sh,top])]
    lng=mat('mRlLongV',pRl,['cccccccc','bbbbbbbb']); sh=mat('mRlShortV',pRl,['cc','bb'])
    top=mat('mRlTopS',pRl,['cc','cd','cc','cd','cc','cd','cc','cd'])
    return [Box(2,2,6,6,0,1,[plate]*5),Box(3,3,5,5,1,4,[post]*5),Box(3,0,5,8,4,6,[sh,lng,sh,lng,top])]
OBJS.append(('RailU',rail(True),16,'grind rail along a'))
OBJS.append(('RailV',rail(False),16,'grind rail along b'))

# ================================================================== PACK 2: launch ramp, funbox, barrel, trash can, planter, picnic table, jersey barrier, manual pad
# ------------------------------------------------------------------ launch ramp (taller blue wedge with a white centre stripe, 12 px lip)
pLa =pal('pLa', [(3,5,10),(8,14,24),(14,22,31),(24,29,31),(5,9,17)])
pLaR=pal('pLaR',[lift(c,4,3) for c in pLa])
LAH=[12,11,9,7,6,4,2,1]                               # slice k, back (lip) to front; physics is a straight line up to LAUNCH_H
def launch():
    boxes=[]
    for k,h in enumerate(LAH):
        seam=(k%3==0)
        top=mat('mLaT%d'%k,pLa,['bbbddbbb'])
        ris=mat('mLaR%d'%k,pLaR,['c' if seam else 'b'])
        side=mat('mLaS%d'%k,pLa,['e'])
        boxes.append(Box(0,k,8,k+1,0,h,[ris,side,side,side,top]))
    return boxes
OBJS.append(('Launch',launch(),11,'launch ramp, 12 px lip'))

# ------------------------------------------------------------------ funbox (flat concrete platform, yellow band, steel coping, 10 px)
pFb=pal('pFb',[(5,5,9),(14,13,20),(20,18,27),(28,26,31),(10,9,15),(31,27,6)])
def funbox():
    side=mat('mFbSide',pFb,['dddddddd','eeeeeeee','bbbbbbbb','bbcbbbbb','ffffffff','ffffffff','bbbbbbbb','bbbbbcbb','bbbbbbbb','eeeeeeee'])
    top=mat('mFbTop',pFb,['dddddddd','dbbbbbbd','dbbcbbbd','dbbbbbcd','dbcbbbbd','dbbbbbbd','dbbbcbbd','dddddddd'])
    return [Box(0,0,8,8,0,10,[side,side,side,side,top])]
OBJS.append(('Funbox',funbox(),12,'platform, 10 px, solid (ollie onto it)'))

# ------------------------------------------------------------------ barrel (red oil drum with ribs, 8 px)
pBa=pal('pBa',[(8,2,2),(24,6,5),(30,12,9),(14,3,3),(31,24,20)])
def barrel():
    side=mat('mBaSide',pBa,['bbcbbbbb','dddddddd','bbcbbbbb','bbcbbbbb','bbcbbbbb','bbcbbbbb','dddddddd','bbcbbbbb'])
    top=mat('mBaTop',pBa,['dddddd','dbbbbd','dbeebd','dbeebd','dbbbbd','dddddd'])
    return [Box(1,1,7,7,0,8,[side,side,side,side,top])]
OBJS.append(('Barrel',barrel(),11,'oil drum, 8 px, solid'))

# ------------------------------------------------------------------ trash can (grey-green, lid, 10 px)
pTr=pal('pTr',[(3,5,4),(12,18,14),(17,24,19),(22,28,24),(7,11,8)])
def trashcan():
    body=mat('mTrBody',pTr,['bcbcbcbc']*9)
    lidS=mat('mTrLidS',pTr,['d']); lidT=mat('mTrLidT',pTr,['dddddddd','dcccccd d'.replace(' ',''),'dcdddcdd','dcccccdd','dddddddd','dcccccdd','dcdddcdd','dddddddd'][:8])
    return [Box(1,1,7,7,0,9,[body,body,body,body,body]),Box(0,0,8,8,9,10,[lidS,lidS,lidS,lidS,lidT])]
OBJS.append(('TrashCan',trashcan(),11,'trash can, 10 px, solid'))

# ------------------------------------------------------------------ planter (brick box with soil and plants, grind height 6)
pPl=pal('pPl',[(6,3,2),(20,10,6),(26,15,9),(14,7,4),(9,6,3),(8,22,6),(14,28,9),(30,10,16)])
def planter():
    side=mat('mPlSide',pPl,['cbbbcbbb','dddddddd','bbcbbbcb','dddddddd','cbbbcbbb','dddddddd'])
    top=mat('mPlTop',pPl,['cccccccc','cefeefec','cffefeec','cefhfefc','cfeefhec','cefefeec','cfehefec','cccccccc'])
    return [Box(0,0,8,8,0,6,[side,side,side,side,top])]
OBJS.append(('Planter',planter(),11,'planter box, grind 6 px'))

# ------------------------------------------------------------------ picnic table (top at 6 px so it grinds; seats both sides)
pPt=pal('pPt',[(6,4,2),(25,18,9),(18,12,6),(29,22,12),(13,8,4)])
def picnic():
    leg=mat('mPtLeg',pPt,['e']); seatE=mat('mPtSeatE',pPt,['bbbbbbbb']); seatT=mat('mPtSeatT',pPt,['bbbbbbbb','bbcbbbbb'])
    topE=mat('mPtTopE',pPt,['dddddddd','cccccccc']); topT=mat('mPtTopT',pPt,['dddddddd','cccccccc']*3)
    return [Box(1,0,2,8,0,2,[leg]*5),Box(6,0,7,8,0,2,[leg]*5),Box(0,0,8,2,2,3,[seatE,seatE,seatE,seatE,seatT]),Box(0,6,8,8,2,3,[seatE,seatE,seatE,seatE,seatT]),
            Box(1,1,7,7,5,6,[topE,topE,topE,topE,topT]),Box(2,1,3,7,3,5,[leg]*5),Box(5,1,6,7,3,5,[leg]*5)]
OBJS.append(('PicnicTable',picnic(),12,'picnic table, grind 6 px'))

# ------------------------------------------------------------------ jersey barrier (concrete, red/white stripes on the top, grind 6 px). U / V variants like the ledge
pJe=pal('pJe',[(6,6,8),(17,17,19),(22,22,24),(27,28,30),(12,12,14),(27,6,5),(30,30,30)])
def jersey(alongU):
    base=mat('mJeBase',pJe,['bbbbbbbb','eeeeeeee']); mid=mat('mJeMid',pJe,['cccccccc','bbbbbbbb'])
    stp =mat('mJeStripe',pJe,['ffggffgg','ggffggff'])
    topm=mat('mJeTop'+('U' if alongU else 'V'),pJe,['dddd']*8 if alongU else ['dddddddd']*4)
    if alongU: return [Box(0,1,8,7,0,2,[base]*4+[mid]),Box(0,2,8,6,2,4,[mid,mid,mid,mid,mid]),Box(0,3,8,5,4,6,[stp,stp,stp,stp,topm])]
    return [Box(1,0,7,8,0,2,[base]*4+[mid]),Box(2,0,6,8,2,4,[mid,mid,mid,mid,mid]),Box(3,0,5,8,4,6,[stp,stp,stp,stp,topm])]
OBJS.append(('JerseyU',jersey(True),12,'jersey barrier, runs along a'))
OBJS.append(('JerseyV',jersey(False),12,'jersey barrier, runs along b'))

# ------------------------------------------------------------------ manual pad (low painted platform, 3 px: rolls straight on, the spot for manuals)
pPd=pal('pPd',[(6,6,8),(16,16,18),(21,22,25),(30,26,5),(24,20,3)])
def mpad():
    side=mat('mPdSide',pPd,['bbb','eee'.replace('e','a')][:1]+['cccccccc','bbbbbbbb'])
    top=mat('mPdTop',pPd,['dddddddd','dbbbbbbd','dbcbbbcd','dbbcbcbd','dbbbcbbd','dbbcbcbd','dbcbbbcd','dddddddd'])
    return [Box(0,0,8,8,0,3,[side,side,side,side,top])]
OBJS.append(('ManualPad',mpad(),12,'manual pad, 3 px, rides on without a jump'))

# ------------------------------------------------------------------ renderer (port of items.h)
def rotPt(r,a,b): return [(a,b),(b,8-a),(8-a,8-b),(8-b,a)][r]
def drawBox(d,q,r):
    bx=q['s']; u0,u1,v0,v1,z1=q['u0'],q['u1'],q['v0'],q['v1'],q['z1']; HT=bx.z1-bx.z0
    ml=bx.m[(4-r)&3]; mr=bx.m[(5-r)&3]; mt=bx.m[4]
    for u in range(u0,u1):
        X=IOX+u-v1; yh=IOY-4+((u+v1)>>1)-z1
        for j in range(HT):
            y=yh+1+j; c=ml.px(u-u0,j,12)
            if c is not KEY and 0<=X<IW and 0<=y<IH: d[y][X]=c
    for v in range(v1,v0-1,-1):
        X=IOX+u1-v; yh=IOY-4+((u1+v)>>1)-z1
        for j in range(HT):
            y=yh+1+j; c=mr.px(v1-v,j,9)
            if c is not KEY and 0<=X<IW and 0<=y<IH: d[y][X]=c
    for X in range(u0-v1,u1-v0+1):
        vlo=max(v0,u0-X); vhi=min(v1,u1-X); ylo=(2*vlo+X+1)>>1; yhi=(2*vhi+X)>>1
        for Y in range(ylo,yhi+1):
            v2=2*Y-X; cv=v2>>1; cu=(v2+2*X)>>1
            cu=max(u0,min(u1-1,cu)); cv=max(v0,min(v1-1,cv))
            la,lb=[(cu,cv),(7-cv,cu),(7-cu,7-cv),(cv,7-cu)][r]
            c=mt.px(la-bx.a0,lb-bx.b0,16); x=IOX+X; y=IOY-4+Y-z1
            if c is not KEY and 0<=x<IW and 0<=y<IH: d[y][x]=c
def behind(A,B): return A['u1']<=B['u0'] or A['v1']<=B['v0'] or A['z1']<=B['z0']
def drawObj(d,boxes,r):
    q=[]
    for b in boxes:
        ua,va=rotPt(r,b.a0,b.b0); ub,vb=rotPt(r,b.a1,b.b1)
        q.append(dict(u0=min(ua,ub),u1=max(ua,ub),v0=min(va,vb),v1=max(va,vb),z0=b.z0,z1=b.z1,s=b))
    done=0
    while done<len(q):
        pick=-1
        for i in range(len(q)):
            if q[i]['s'] is None: continue
            if all(j==i or q[j]['s'] is None or not(behind(q[j],q[i]) and not behind(q[i],q[j])) for j in range(len(q))): pick=i; break
        if pick<0: pick=[i for i in range(len(q)) if q[i]['s'] is not None][0]
        drawBox(d,q[pick],r); q[pick]['s']=None; done+=1
def outline(d,nsh):
    t=[row[:] for row in d]
    for y in range(IH):
        for x in range(IW):
            if t[y][x] is KEY: continue
            e=(x==0 or t[y][x-1] is KEY) or (x==IW-1 or t[y][x+1] is KEY) or (y==0 or t[y-1][x] is KEY) or (y==IH-1 or t[y+1][x] is KEY)
            if e: d[y][x]=shade(t[y][x],nsh)
def bake(boxes,r,nsh):
    d=[[KEY]*IW for _ in range(IH)]; drawObj(d,boxes,r)
    if nsh<16: outline(d,nsh)
    return d

# ------------------------------------------------------------------ C emitter
def cpal(n,cols): return 'static const u16 %s[%d]={%s};'%(n,len(cols),','.join('RGB(%d,%d,%d)'%c for c in cols))
def emit():
    L=['// skateart.h - GENERATED by tools/make_skate_items.py (edit the art there, then re-run). Included by itembake.h (host-only; the baked result is source/itemrom.h).',
       '// Kicker ramp, quarter pipe, ledge, bench and the v2 grind rail, in the same textured-box model as items.h.']
    for n,c in PALS: L.append(cpal(n,c))
    for m in MATS:
        L.append('MAT(%s,%s,%d,%d,%s)'%(m.name,[n for n,c in PALS if c is m.pal][0],m.w,m.h,','.join('"%s"'%r for r in m.rows)))
    for name,boxes,nsh,note in OBJS:
        L.append('static const IBox bx%s[%d]={ // %s'%(name,len(boxes),note))
        for i,b in enumerate(boxes):
            L.append(' {%d,%d,%d,%d,%d,%d,{%s}}%s'%(b.a0,b.b0,b.a1,b.b1,b.z0,b.z1,','.join('&'+m.name for m in b.m),',' if i<len(boxes)-1 else ' };'))
    return '\n'.join(L)+'\n'

def preview(path,scale=6):
    from PIL import Image
    rows=[('Kicker',[bake(OBJS[0][1],r,11) for r in range(4)]),('QuarterPipe',[bake(OBJS[1][1],r,11) for r in range(4)]),
          ('Ledge U / V',[bake(OBJS[2][1],0,12),bake(OBJS[3][1],0,12)]),('Bench U / V',[bake(OBJS[4][1],0,12),bake(OBJS[5][1],0,12)]),
          ('Rail U / V',[bake(OBJS[6][1],0,16),bake(OBJS[7][1],0,16)])]
    cols=4; pad=8; W=cols*(IW*scale+pad)+pad; H=len(rows)*(IH*scale+pad)+pad
    im=Image.new('RGB',(W,H),(52,54,62))
    for ri,(lab,sp) in enumerate(rows):
        for ci,s in enumerate(sp):
            ox=pad+ci*(IW*scale+pad); oy=pad+ri*(IH*scale+pad)
            # floor diamond so the footprint reads
            for y in range(IH):
                for x in range(IW):
                    dx=abs(x-IOX); dy=abs(y-(IOY-4)) 
                    if dx/8.0+dy/4.0<=1.0:
                        for yy in range(scale):
                            for xx in range(scale): im.putpixel((ox+x*scale+xx,oy+y*scale+yy),(70,72,80))
            for y in range(IH):
                for x in range(IW):
                    c=s[y][x]
                    if c is KEY: continue
                    c8=tuple(min(255,v*255//31) for v in c)
                    for yy in range(scale):
                        for xx in range(scale): im.putpixel((ox+x*scale+xx,oy+y*scale+yy),c8)
    im.save(path)

def preview2(path,scale=6):
    from PIL import Image
    by={n:b for n,b,_,_ in OBJS}; sh={n:x for n,_,x,_ in OBJS}
    rows=[('Launch',[bake(by['Launch'],r,11) for r in range(4)]),
          ('Funbox / Barrel / Trash can / Planter',[bake(by[n],0,sh[n]) for n in ('Funbox','Barrel','TrashCan','Planter')]),
          ('Picnic table / Jersey U / Jersey V / Manual pad',[bake(by[n],0,sh[n]) for n in ('PicnicTable','JerseyU','JerseyV','ManualPad')])]
    cols=4; pad=8; W=cols*(IW*scale+pad)+pad; H=len(rows)*(IH*scale+pad)+pad
    im=Image.new('RGB',(W,H),(52,54,62))
    for ri,(lab,sp) in enumerate(rows):
        for ci,s2 in enumerate(sp):
            ox=pad+ci*(IW*scale+pad); oy=pad+ri*(IH*scale+pad)
            for y in range(IH):
                for x in range(IW):
                    if abs(x-IOX)/8.0+abs(y-(IOY-4))/4.0<=1.0:
                        for yy in range(scale):
                            for xx in range(scale): im.putpixel((ox+x*scale+xx,oy+y*scale+yy),(70,72,80))
            for y in range(IH):
                for x in range(IW):
                    c=s2[y][x]
                    if c is KEY: continue
                    c8=tuple(min(255,v*255//31) for v in c)
                    for yy in range(scale):
                        for xx in range(scale): im.putpixel((ox+x*scale+xx,oy+y*scale+yy),c8)
    im.save(path)

def emit_ramps():
    return ('// rampdata.h - GENERATED by tools/make_skate_items.py. Surface heights (px) that match the ramp sprites, used by ramps.h.\n'
            'static const u8 qpH[8]={%s};   // quarter pipe, per eighth of the tile from the low edge to the lip\n'
            '#define KICKER_H %d   // kicker height at the lip\n'
            '#define LAUNCH_H %d   // launch ramp height at the lip (pack 2)\n')%(','.join(str(h) for h in reversed(QPH)),8,LAH[0])

if __name__=='__main__':
    open(os.path.join(ROOT,'source','skateart.h'),'w').write(emit())
    open(os.path.join(ROOT,'source','rampdata.h'),'w').write(emit_ramps())
    try: preview(os.path.join(ROOT,'assets','preview','skate_items.png')); preview2(os.path.join(ROOT,'assets','preview','skate_items2.png'))
    except ImportError: print('Pillow missing: preview skipped')
    print('wrote source/skateart.h')
