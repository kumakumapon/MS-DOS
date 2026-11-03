#!/usr/bin/env python3
"""Package a PM32 ELF as a bounded DOS MZ stub plus a versioned ELF overlay."""
import argparse
from pathlib import Path
import struct

def package(elf,abi=256,features=0):
    if abi not in (256,257) or features&~15 or (abi==256 and features):raise ValueError("unsupported ABI/features")
    header=bytearray(96)
    # DOS loads only this short stub, never the ELF overlay into conventional RAM.
    code=bytes.fromhex('0e1fba1000b409cd21b8014ccd219090')+b'Use K32RUN APP32.EXE\r\n$'
    load=96+len(code)
    struct.pack_into('<14H',header,0,0x5a4d,load%512,(load+511)//512,0,6,16,16,0,0x100,0,0,0,96,0)
    header[28:72]=b'K386APP1'+struct.pack('<HH',256,3)+b'K38632 application'.ljust(32,b'\0')
    at=(load+15)&~15
    header[72:96]=struct.pack('<8sHHIII',b'K32EXE01',abi,24,at,len(elf),features)
    return bytes(header)+code+bytes(at-load)+elf

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('elf',type=Path);p.add_argument('output',type=Path);p.add_argument('--abi',type=lambda s:int(s,0),default=256);p.add_argument('--features',type=lambda s:int(s,0),default=0);a=p.parse_args()
    a.output.write_bytes(package(a.elf.read_bytes(),a.abi,a.features))
if __name__=='__main__':main()
