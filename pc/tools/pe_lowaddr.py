#!/usr/bin/env python3
"""Make a 64-bit Windows exe keep everything below 2 GB (the game stores addresses in 32-bit numbers).
Clears LARGE_ADDRESS_AWARE, HIGH_ENTROPY_VA and DYNAMIC_BASE in the PE header and checks the image base is below 2 GB.
Done on the finished file so it does not depend on which linker options this binutils knows."""
import struct, sys
p = sys.argv[1]
d = bytearray(open(p, 'rb').read())
pe = struct.unpack_from('<I', d, 0x3c)[0]
assert d[pe:pe+4] == b'PE\0\0', 'not a PE file'
ch = struct.unpack_from('<H', d, pe+22)[0]
opt = pe + 24
magic = struct.unpack_from('<H', d, opt)[0]
assert magic == 0x20b, 'not a 64-bit (PE32+) exe'
base = struct.unpack_from('<Q', d, opt+24)[0]
soi = struct.unpack_from('<I', d, opt+56)[0]
dc = struct.unpack_from('<H', d, opt+70)[0]
print('before: characteristics=%04x dllchar=%04x imagebase=%x size=%x' % (ch, dc, base, soi))
ch &= ~0x0020              # IMAGE_FILE_LARGE_ADDRESS_AWARE
dc &= ~(0x0020 | 0x0040)   # HIGH_ENTROPY_VA, DYNAMIC_BASE
struct.pack_into('<H', d, pe+22, ch)
struct.pack_into('<H', d, opt+70, dc)
open(p, 'wb').write(d)
print('after:  characteristics=%04x dllchar=%04x' % (ch, dc))
if base + soi >= 0x7fff0000:
    sys.exit('image base %x + size %x is not below 2 GB' % (base, soi))
# the fake GBA needs these free: 02000000-0203FFFF 03000000-03007FFF 04000000.. 05000000.. 06000000-0601FFFF 07000000.. 08000000-0800FFFF 0E000000-0E00FFFF
if base < 0x0F000000 and base + soi > 0x02000000:
    sys.exit('image overlaps the fake GBA memory')
print('ok')
