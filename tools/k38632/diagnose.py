#!/usr/bin/env python3
"""Resolve a recorded PM32 EIP using a hash-matched, unstripped build ELF."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('map',type=Path);p.add_argument('--elf',type=Path,required=True)
    p.add_argument('--exe',type=Path,required=True);p.add_argument('--eip',type=lambda s:int(s,0),required=True)
    p.add_argument('--cr2',type=lambda s:int(s,0));a=p.parse_args()
    m=json.loads(a.map.read_text())
    for key,path in (('elf',a.elf),('exe',a.exe)):
        if m.get('version')!=1 or hashlib.sha256(path.read_bytes()).hexdigest()!=m.get(key+'_sha256'):p.error(key+' does not match the build map')
    print('EIP=0x%08x'%a.eip)
    if a.cr2 is not None:print('CR2=0x%08x'%a.cr2)
    subprocess.run(['addr2line','-f','-C','-e',str(a.elf),hex(a.eip)],check=True)
if __name__=='__main__':main()
