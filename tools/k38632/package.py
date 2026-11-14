#!/usr/bin/env python3
"""Package a PM32 ELF as a bounded DOS MZ stub plus a versioned ELF overlay."""
import argparse
from pathlib import Path
import struct

def compact_elf(elf):
    # Keep virtual page layout and every loaded byte; pack file payloads at
    # 16-byte alignment and omit symbols/sections. The loader accepts both
    # 16-byte and original 4096-byte FILE alignment; VAs stay page-aligned.
    if len(elf)<52 or elf[:7]!=b'\x7fELF\x01\x01\x01':raise ValueError('not ELF32')
    ph,ent,count=struct.unpack_from('<I',elf,28)[0],*struct.unpack_from('<HH',elf,42)
    if ent!=32 or not 1<=count<=8 or ph<52 or ph+ent*count>len(elf):raise ValueError('invalid program headers')
    out=bytearray(elf[:ph+ent*count]);struct.pack_into('<I',out,32,0);struct.pack_into('<HHH',out,46,0,0,0)
    for i in range(count):
        p=list(struct.unpack_from('<8I',elf,ph+i*ent))
        kind,offset,size=p[0],p[1],p[4]
        if kind not in (0,1) or offset>len(elf) or size>len(elf)-offset:raise ValueError('invalid load range')
        if kind==1:
            aligned=(len(out)+15)&~15;out.extend(bytes(aligned-len(out)));p[1]=aligned;p[7]=16
            out.extend(elf[offset:offset+size]);struct.pack_into('<8I',out,ph+i*ent,*p)
    return bytes(out)

def package(elf,abi=256,features=0,gui=False,client=False,stack_bytes=None,min_heap=0):
    if abi not in (256,257,258) or features&~(31 if gui else 239 if client else 15) or (features&128 and features&96!=96) or (abi==256 and features) or (gui and not features&16) or (client and (gui or not features&32)):raise ValueError("unsupported ABI/features")
    if abi!=258 and (stack_bytes is not None or min_heap):raise ValueError('resources require ABI 258')
    resource=b''
    if abi==258:
        stack_bytes=65536 if stack_bytes is None else stack_bytes
        if stack_bytes<4096 or stack_bytes>262144 or stack_bytes%4096 or not 0<=min_heap<=0x400000:raise ValueError('invalid resource budget')
        resource=struct.pack('<8sHHIII',b'K32RES01',1,24,stack_bytes,min_heap,0)
    header=bytearray(96)
    # DOS loads only this short stub, never the ELF overlay into conventional RAM.
    code=bytes.fromhex('0e1fba1000b409cd21b8014ccd219090')+(b'Use KGUILDR KGUI32.EXE\r\n$' if gui else b'Start this app in KGUI32\r\n$' if client else b'Use K32RUN APP32.EXE\r\n$')
    load=96+len(code)
    struct.pack_into('<14H',header,0,0x5a4d,load%512,(load+511)//512,0,6,16,16,0,0x100,0,0,0,96,0)
    header[28:72]=b'K386APP1'+struct.pack('<HH',256,3)+b'K38632 application'.ljust(32,b'\0')
    at=(load+len(resource)+15)&~15
    header[72:96]=struct.pack('<8sHHIII',b'K32EXE01',abi,24,at,len(elf),features)
    return bytes(header)+code+bytes(at-load-len(resource))+resource+elf

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('elf',type=Path);p.add_argument('output',type=Path);p.add_argument('--abi',type=lambda s:int(s,0),default=256);p.add_argument('--features',type=lambda s:int(s,0),default=0);p.add_argument('--gui',action='store_true');p.add_argument('--client',action='store_true');p.add_argument('--compact',action='store_true');p.add_argument('--stack-bytes',type=lambda s:int(s,0));p.add_argument('--min-heap',type=lambda s:int(s,0),default=0);p.add_argument('--debug-map',type=Path);a=p.parse_args()
    elf=a.elf.read_bytes();output=package(compact_elf(elf) if a.compact else elf,a.abi,a.features,a.gui,a.client,a.stack_bytes,a.min_heap);a.output.write_bytes(output)
    if a.debug_map:
        import hashlib,json
        a.debug_map.write_text(json.dumps({'version':1,'elf':a.elf.name,'elf_sha256':hashlib.sha256(elf).hexdigest(),'exe':a.output.name,'exe_sha256':hashlib.sha256(output).hexdigest()},indent=2)+'\n')
if __name__=='__main__':main()
