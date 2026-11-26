#!/usr/bin/env python3
"""Check the actual universal APK, not only the Gradle settings."""
import struct, sys, zipfile
abis={"armeabi-v7a":(1,40),"arm64-v8a":(2,183),"x86":(1,3),"x86_64":(2,62)}
with zipfile.ZipFile(sys.argv[1]) as z:
    assert z.testzip() is None
    for abi,(cls,machine) in abis.items():
        for lib in ("libSDL2.so","libmain.so"):
            name=f"lib/{abi}/{lib}"; b=z.read(name)
            assert b[:4]==b"\x7fELF" and b[4]==cls and b[5]==1,name
            assert struct.unpack_from("<H",b,18)[0]==machine,name
            if cls==2:
                off=struct.unpack_from("<Q",b,32)[0]
                entsize,num=struct.unpack_from("<HH",b,54)
                loads=0
                for i in range(num):
                    p=struct.unpack_from("<IIQQQQQQ",b,off+i*entsize)
                    if p[0]==1:
                        loads+=1
                        assert p[7]>=16384 and (p[2]-p[3])%16384==0,(name,p)
                assert loads,name
            if lib=="libmain.so":
                for marker in (b"FIELD GUIDE",b"first rendered game frame",b"Android GBA map offset"):
                    assert marker in b,(name,marker)
    print("Universal APK: four ABIs, valid ELF architectures and 16 KiB 64-bit LOAD alignment PASS")
