"""Original 256-index color chart, generated without external image assets."""
import struct
import sys
from pathlib import Path
colors = [0x234,0x256,0xeff,0xcde,0x27b,0x259,0xfc6,0xfff,0x123,0xbcd,0x678,0x467,0x367,0x7ab,0xeef,0xacd]
palette = [(c >> 8, (c >> 4) & 15, c & 15) for c in colors]
palette = [(r*17,g*17,b*17) for r,g,b in palette]
palette += [(r*51,g*51,b*51) for r in range(6) for g in range(6) for b in range(6)]
palette += [(24+t,64+t*2,88+t*2) for t in range(24)]
w,h = 160,112
pixels = bytes((x//10 + y//7*16) for y in range(h-1,-1,-1) for x in range(w))
header = b'BM'+struct.pack('<IHHI',1078+len(pixels),0,0,1078)
header += struct.pack('<IiiHHIIiiII',40,w,h,1,8,0,len(pixels),0,0,256,0)
Path(sys.argv[1]).write_bytes(header+b''.join(bytes((b,g,r,0)) for r,g,b in palette)+pixels)
