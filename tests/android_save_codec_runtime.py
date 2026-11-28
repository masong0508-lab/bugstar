#!/usr/bin/env python3
"""Compile the production Java cartridge codec without an Android runtime."""
from pathlib import Path
import subprocess,tempfile,struct,zlib
root=Path(__file__).resolve().parents[1]
s=(root/"pc/android/app/src/main/java/org/bugstar/app/BugstarActivity.java").read_text()
def extract(mark):
    a=s.index(mark);b=s.index("{",a)+1;d=1
    while d:
        d+=(s[b]=="{")-(s[b]=="}");b+=1
    return s[a:b]
magic=s[s.index("private static final byte[] SAVE_MAGIC"):s.index(";",s.index("private static final byte[] SAVE_MAGIC"))+1]
body=r"""
    static void reject(byte[] data)throws Exception {
        try {readSave(new ByteArrayInputStream(data));}
        catch(IOException e){return;}
        throw new AssertionError("Accepted malformed backup");
    }
    public static void main(String[] args)throws Exception {
        byte[] raw=new byte[SRAM_SIZE];for(int i=0;i<raw.length;i++)raw[i]=(byte)(i*37+11);
        ByteArrayOutputStream bytes=new ByteArrayOutputStream();
        try(DataOutputStream out=new DataOutputStream(bytes)){writeSave(out,raw);}
        byte[] result=bytes.toByteArray();
        if(!Arrays.equals(raw,readSave(new ByteArrayInputStream(result))))throw new AssertionError("roundtrip");
        for(int off:new int[]{0,8,11,12,result.length-1}){
            byte[] bad=result.clone();bad[off]^=1;reject(bad);
        }
        reject(Arrays.copyOf(result,result.length-1));reject(Arrays.copyOf(result,result.length+1));reject(new byte[0]);
        Files.write(Paths.get("java.bgsave"),result);
        byte[] reference=Files.readAllBytes(Paths.get("reference.bgsave"));
        if(!Arrays.equals(raw,readSave(new ByteArrayInputStream(reference))))throw new AssertionError("reference decode");
    }
}
"""
with tempfile.TemporaryDirectory() as td:
    p=Path(td);raw=bytes((i*37+11)&255 for i in range(32768))
    reference=b"BGSTAR1\n"+struct.pack(">I",zlib.crc32(raw))+raw
    (p/"reference.bgsave").write_bytes(reference)
    java="import java.io.*;import java.util.*;import java.util.zip.*;import java.nio.file.*; public class Codec { private static final int SRAM_SIZE=32768;\n"+magic+"\n"+extract("private static void writeSave(")+"\n"+extract("private static byte[] readSave(")+body
    (p/"Codec.java").write_text(java)
    subprocess.run(["javac","Codec.java"],cwd=p,check=True)
    subprocess.run(["java","Codec"],cwd=p,check=True)
    assert (p/"java.bgsave").read_bytes()==reference
    print("Java cartridge codec: exact cross-language backup and malformed-file rejection PASS")
