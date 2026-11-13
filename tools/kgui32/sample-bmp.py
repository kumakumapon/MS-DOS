#!/usr/bin/env python3
"""Original deterministic BMP24 fixture, with saturated colours and fine edges."""
import struct
import sys
from pathlib import Path
w,h=64,48
rows=[]
for y in reversed(range(h)):
    row=bytearray()
    for x in range(w):
        if y<16:r,g,b=((255,0,0),(0,255,0),(0,0,255),(255,255,255))[x//16]
        elif y<32:r,g,b=x*255//63,(y-16)*255//15,(x^y)*255//63
        else:r,g,b=((255,255,0) if (x//4+y//4)&1 else (0,0,0))
        row.extend((b,g,r))
    rows.append(row)
pixels=b''.join(rows)
header=struct.pack('<2sIHHI',b'BM',54+len(pixels),0,0,54)+struct.pack('<IiiHHIIiiII',40,w,h,1,24,0,len(pixels),0,0,0,0)
Path(sys.argv[1]).write_bytes(header+pixels)
