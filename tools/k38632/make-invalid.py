from pathlib import Path
import struct,sys
root=Path(sys.argv[1]);source=(root/'HELLO32.EXE').read_bytes();base=struct.unpack_from('<I',source,84)[0];ph=base+struct.unpack_from('<I',source,base+28)[0]
for name,at,value,fmt in [('BADABI',80,2,'<H'),('BADENT',base+24,0x10000,'<I'),('BADOVR',ph+32+8,0x800000,'<I'),('BADSIZE',ph+20,0xffffffff,'<I'),('BADELF',base+4,2,'<B')]:
 b=bytearray(source);struct.pack_into(fmt,b,at,value);(root/(name+'.EXE')).write_bytes(b)
