"""Validate real packaged GUI artifacts with both host parsers."""
import ctypes as c
import importlib.util
from pathlib import Path
import struct
import subprocess
import unittest

ROOT=Path(__file__).resolve().parent
K32=ROOT.parent/'k38632'
class Segment(c.Structure):
    _fields_=[(k,c.c_uint32) for k in ('offset','address','filesz','memsz','flags')]
class Plan(c.Structure):
    _fields_=[(k,c.c_uint32) for k in ('entry','elf_offset','elf_size','count','rwbase','rwbytes')]+[('segments',Segment*8)]+[(k,c.c_uint32) for k in ('abi','features','stack_base','stack_top','heap_limit','min_heap')]
READ=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_uint32,c.c_void_p,c.c_uint32)
def parser(gui,host=False):
    path=ROOT/'build'/('host-parser.so' if host else 'gui-parser.so' if gui else 'old-parser.so')
    subprocess.run(['cc','-shared','-fPIC',*(['-DK32_HOST'] if host else ['-DK32_GUI'] if gui else []),str(K32/'elf.c'),'-o',str(path)],check=True)
    lib=c.CDLL(str(path));lib.k32_plan.argtypes=[READ,c.c_void_p,c.c_uint32,c.POINTER(Plan)]
    def parse(data):
        def read(_,at,dest,n):
            if at>len(data) or n>len(data)-at:return 0
            c.memmove(dest,bytes(data[at:at+n]),n);return 1
        plan=Plan();error=lib.k32_plan(READ(read),None,len(data),c.byref(plan));return error,plan
    return parse
def symbols(name):
    return {line.split()[2]:int(line.split()[0],16) for line in subprocess.check_output(['nm','-n',str(ROOT/'build'/name)],text=True).splitlines() if len(line.split())==3}
class Image(unittest.TestCase):
    def test_host_contracts(self):
        data=(ROOT/'build/KGUI32.EXE').read_bytes()
        self.assertNotEqual(parser(False)(data)[0],0)
        error,p=parser(True)(data);self.assertEqual(error,0)
        self.assertEqual((p.abi,p.features),(257,23))
        self.assertGreater(p.rwbytes,1024*1024)
        self.assertLess(p.rwbase+p.rwbytes,0xbfd000)
        self.assertEqual(symbols('desktop.elf')['gui_state'],p.rwbase)
        b=bytearray(data);struct.pack_into('<I',b,92,32)
        self.assertNotEqual(parser(True)(b)[0],0)
    def test_desktop_stack_headroom(self):
        rows=(ROOT/'build/desktop.su').read_text().splitlines()
        self.assertTrue(rows)
        for row in rows:
            function,size,kind=row.split('\t')
            self.assertLessEqual(int(size),3072,function+' must leave headroom in the 4 KiB user stack')
    def test_graphical_client_packages(self):
        for name in ('KGCALC','KGCLOCK','KGFILES','KGDIRBAD','KGEDIT','KGRECNEG','KGRECNON','KGIMAGE'):
            data=(ROOT/f'build/{name}.EXE').read_bytes()
            self.assertNotEqual(parser(False)(data)[0],0)
            self.assertNotEqual(parser(True)(data)[0],0)
            error,p=parser(False,True)(data);self.assertEqual(error,0)
            self.assertEqual((p.abi,p.features),(257,231 if name in ('KGEDIT','KGFILES','KGRECNEG') else 103 if name in ('KGDIRBAD','KGRECNON') else 239 if name=='KGIMAGE' else 39))
            elf,stats={'KGCALC':('calculator.elf','calc_stats'),'KGCLOCK':('clock.elf','clock_stats'),'KGFILES':('files.elf','files_stats'),'KGDIRBAD':('directory-test.elf','directory_stats'),'KGEDIT':('editor.elf','editor_stats'),'KGRECNEG':('recovery-test.elf','recovery_stats'),'KGRECNON':('recovery-test.elf','recovery_stats'),'KGIMAGE':('image.elf','image_stats')}[name]
            self.assertEqual(symbols(elf)[stats],p.rwbase)
    def test_compact_packages_keep_all_loaded_bytes_and_page_alignment(self):
        for exe,elf in [('KGUI32','desktop'),('KGEDIT','editor'),('KGIMAGE','image')]:
            package=(ROOT/f'build/{exe}.EXE').read_bytes();at,size=struct.unpack_from('<II',package,84)
            compact=package[at:];original=(ROOT/f'build/{elf}.elf').read_bytes()
            self.assertEqual(size,len(compact));self.assertLess(len(compact),len(original))
            self.assertEqual(struct.unpack_from('<I',compact,32)[0],0)
            ph=struct.unpack_from('<I',original,28)[0];count=struct.unpack_from('<H',original,44)[0]
            for i in range(count):
                p=ph+i*32;kind,offset,_,_,n=struct.unpack_from('<5I',original,p)
                new=struct.unpack_from('<8I',compact,p);old=struct.unpack_from('<8I',original,p)
                self.assertEqual(new[2:7],old[2:7])
                if kind==1:
                    self.assertEqual(compact[new[1]:new[1]+n],original[offset:offset+n]);self.assertEqual(new[1]%16,0);self.assertEqual(new[2]%4096,0);self.assertEqual(new[7],16)
    def test_loader_bounds_and_explicit_allocation(self):
        s=symbols('driver.elf');self.assertLess(s['__bss_end'],0xd400)
        self.assertGreaterEqual(0xeff0-s['__bss_end'],7000)
        b=(ROOT/'build/KGUILDR.EXE').read_bytes();h=struct.unpack_from('<14H',b)
        self.assertEqual(h[0],0x5a4d);self.assertEqual(h[5],h[6])
        self.assertEqual(((len(b)-h[4]*16+15)//16+h[5])*16,65536)
        self.assertIn(b'KG32DRV1',b)
    def test_gui_requires_explicit_packaging(self):
        spec=importlib.util.spec_from_file_location('package',K32/'package.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with self.assertRaises(ValueError):module.package(b'',257,23)
        with self.assertRaises(ValueError):module.package(b'',257,7,True)
        with self.assertRaises(ValueError):module.package(b'',257,64)
        with self.assertRaises(ValueError):module.package(b'',257,87,True)
        with self.assertRaises(ValueError):module.package(b'',257,167,client=True)
        self.assertEqual(struct.unpack_from('<I',module.package(b'',257,103,client=True),92)[0],103)
        self.assertIn(b'KGUILDR',module.package(b'',257,23,True))
if __name__=='__main__':unittest.main()
