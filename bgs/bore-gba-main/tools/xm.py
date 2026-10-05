import struct
def parse(path):
    d=open(path,'rb').read()
    hs,=struct.unpack('<I',d[60:64])
    songlen,restart,nch,npat,ninst,flags,tempo,bpm=struct.unpack('<8H',d[64:80])
    order=list(d[80:80+songlen]); p=60+hs
    pats=[]
    for i in range(npat):
        hl,pt,nrows,psz=struct.unpack('<IBHH',d[p:p+9]); p+=hl
        rows=[];q=p
        for r in range(nrows):
            row=[]
            for c in range(nch):
                b=d[q];q+=1
                n=i_=v=e=ep=0
                if b&0x80:
                    if b&1:n=d[q];q+=1
                    if b&2:i_=d[q];q+=1
                    if b&4:v=d[q];q+=1
                    if b&8:e=d[q];q+=1
                    if b&16:ep=d[q];q+=1
                else:
                    n=b;i_=d[q];v=d[q+1];e=d[q+2];ep=d[q+3];q+=4
                row.append((n,i_,v,e,ep))
            rows.append(row)
        p+=psz; pats.append(rows)
    insts=[]
    for i in range(ninst):
        isz,=struct.unpack('<I',d[p:p+4]); name=d[p+4:p+26].split(b'\0')[0].decode('latin1')
        ns,=struct.unpack('<H',d[p+27:p+29])
        info=dict(name=name,samples=[],map=None)
        if ns>0:
            shs,=struct.unpack('<I',d[p+29:p+33])
            info['map']=list(d[p+33:p+33+96])
            volenv=list(struct.unpack('<24H',d[p+129:p+129+48])); 
            nvp=d[p+225]; vt=d[p+233]; vs,ve,vl=d[p+235],d[p+236],d[p+237]
            info['venv']=(volenv[:nvp*2],vt,vs,ve,vl); info['fade']=struct.unpack('<H',d[p+239:p+241])[0]
            p+=isz; sh=[]
            for s in range(ns):
                ln,ls,ll,vol,fine,typ,pan,rel=struct.unpack('<IIIBbBBb',d[p:p+17]); nm=d[p+18:p+40].split(b'\0')[0].decode('latin1'); p+=shs
                sh.append((ln,ls,ll,vol,fine,typ,pan,rel,nm))
            for (ln,ls,ll,vol,fine,typ,pan,rel,nm) in sh:
                raw=d[p:p+ln];p+=ln
                if typ&16:
                    v=list(struct.unpack('<%dh'%(ln//2),raw));a=0;sm=[]
                    for x in v:a=(a+x+32768)%65536-32768;sm.append(a)
                    sm=[x/32768 for x in sm];ls//=2;ll//=2
                else:
                    v=list(struct.unpack('<%db'%ln,raw));a=0;sm=[]
                    for x in v:a=(a+x+128)%256-128;sm.append(a)
                    sm=[x/128 for x in sm]
                info['samples'].append(dict(data=sm,ls=ls,ll=ll,vol=vol,fine=fine,type=typ&3,pan=pan,rel=rel,name=nm))
        else: p+=isz
        insts.append(info)
    return dict(order=order,restart=restart,nch=nch,pats=pats,insts=insts,tempo=tempo,bpm=bpm)
