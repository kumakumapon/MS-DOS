"""Policy used by the real guest broker, including fragmented allocation and bounds."""
import ctypes as c
from pathlib import Path
import random
import subprocess
import unittest
ROOT=Path(__file__).resolve().parent
subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',str(ROOT/'services.c'),'-o',str(ROOT/'build/services-test.so')],check=True)
U=c.c_uint32
class Args(c.Structure):_fields_=[('argc',U),('args',(c.c_char*64)*8),('budget',U)]
class Block(c.Structure):_fields_=[('first',U),('pages',U)]
class Heap(c.Structure):_fields_=[('base',U),('limit',U),('block',Block*64)]
L=c.CDLL(str(ROOT/'build/services-test.so'))
L.k32_arguments.argtypes=[c.c_char_p,c.c_uint,c.POINTER(Args)]
L.k32_filename.argtypes=[c.c_char_p,c.c_uint,c.c_char_p]
L.k32_heap_init.argtypes=[c.POINTER(Heap),U,U]
L.k32_heap_find.argtypes=[c.POINTER(Heap),U,c.POINTER(U),c.POINTER(U),c.POINTER(U)]
L.k32_heap_release.argtypes=[c.POINTER(Heap),U,c.POINTER(U)]
L.k32_user_range.argtypes=[c.POINTER(U),U,U,c.c_int]
L.k32_user_access.argtypes=[c.POINTER(c.c_uint8),U,U,c.c_int]
class Services(unittest.TestCase):
 def test_arguments(self):
  a=Args();s=b' /T ARGS32.EXE "two words" three'
  self.assertEqual(L.k32_arguments(s,len(s),c.byref(a)),0)
  self.assertEqual(a.argc,3);self.assertEqual(a.budget,64)
  self.assertEqual([bytes(a.args[i]).split(b'\0')[0] for i in range(3)],[b'ARGS32.EXE',b'two words',b'three'])
  for s in [b'',b' /T',b'A "unclosed',b'A '+b'x'*64,b'A '+b'b '*8,b'A\x00B',b'x'*127]:
   self.assertNotEqual(L.k32_arguments(s,len(s),c.byref(a)),0,s)
 def test_filename_policy(self):
  out=c.create_string_buffer(16)
  for s in [b'file.txt',b'A',b'12345678.123',b'A_$-0.X']:
   self.assertEqual(L.k32_filename(s,len(s),out),1);self.assertEqual(out.value,b'A:\\'+s.upper())
  for s in [b'',b'.',b'..',b'../X',b'A:FILE',b'\\FILE',b'CON',b'con.txt',b'COM1.X',b'LPT9',b'CLOCK$',b'A..B',b'A.',b'ABCDEFGHI',b'A.ABCD',b'A\x00X',b'A*',b'A B']:
   self.assertEqual(L.k32_filename(s,len(s),out),0,s)
 def test_fragmented_heap(self):
  h=Heap();L.k32_heap_init(c.byref(h),0x802000,0xbfd000);rng=random.Random(386)
  for _ in range(2000):
   live=[i for i,b in enumerate(h.block) if b.pages]
   if live and rng.random()<.45:
    b=h.block[rng.choice(live)];slot=U();self.assertEqual(L.k32_heap_release(c.byref(h),b.first,c.byref(slot)),0);h.block[slot.value].pages=0
   else:
    slot,at,pages=U(),U(),U();n=rng.randint(1,64)*4096-rng.randint(0,4095)
    err=L.k32_heap_find(c.byref(h),n,c.byref(slot),c.byref(at),c.byref(pages))
    if not err:
     self.assertEqual(at.value%4096,0);self.assertGreaterEqual(at.value,h.base);self.assertLessEqual(at.value+pages.value*4096,h.limit)
     for b in h.block:
      if b.pages:self.assertFalse(at.value<b.first+b.pages*4096 and b.first<at.value+pages.value*4096)
     h.block[slot.value]=Block(at.value,pages.value)
    else:self.assertEqual(err,-12)
  slot,at,pages=U(),U(),U()
  for n in [0,0xffffffff]:self.assertEqual(L.k32_heap_find(c.byref(h),n,c.byref(slot),c.byref(at),c.byref(pages)),-22)
  self.assertEqual(L.k32_heap_release(c.byref(h),0x800000,c.byref(slot)),-22)
 def test_range_permissions(self):
  m=(U*1024)();m[0]=5;m[1]=0x1007;m[1021]=0x3fd005;m[1023]=0x3ff007
  for at,n,write,ok in [(0x800000,4096,0,1),(0x800000,1,1,0),(0x800fff,2,0,1),(0x801fff,2,1,0),(0xbfd000,4096,0,1),(0xbfd000,1,1,0),(0xbfffff,2,0,0),(0xffffffff,2,1,0),(0x10000,1,0,0),(0xffffffff,0,1,1)]:
   self.assertEqual(L.k32_user_range(m,at,n,write),ok,(at,n,write))
 def test_compact_permissions_match_hardware_ptes(self):
  rng=random.Random(38632);pt=(U*1024)();flags=(c.c_uint8*1024)()
  for i in range(1024):
   access=rng.choice([0,0,5,7]);flags[i]=access;pt[i]=(i*4096|access) if access else 0
  cases=[(0x800000,1,0),(0xbfffff,2,1),(0xffffffff,2,1),(0x10000,1,0),(0xffffffff,0,1)]
  cases += [(rng.randrange(0x7ff000,0xc01000),rng.randrange(0,20000),rng.randrange(2)) for _ in range(10000)]
  for at,n,write in cases:self.assertEqual(L.k32_user_access(flags,at,n,write),L.k32_user_range(pt,at,n,write),(at,n,write))
if __name__=='__main__':unittest.main()
