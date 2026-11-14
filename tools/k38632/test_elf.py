"""Exercise the same C parser used in the guest against actual packaged files."""
import ctypes as c
import importlib.util
from pathlib import Path
import struct
import subprocess
import unittest
ROOT=Path(__file__).resolve().parent
subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',str(ROOT/'elf.c'),'-o',str(ROOT/'build/elf-test.so')],check=True)
class Segment(c.Structure):
    _fields_=[(k,c.c_uint32) for k in ('offset','address','filesz','memsz','flags')]
class Plan(c.Structure):
    _fields_=[(k,c.c_uint32) for k in ('entry','elf_offset','elf_size','count','rwbase','rwbytes')]+[('segments',Segment*8)]+[(k,c.c_uint32) for k in ('abi','features','stack_base','stack_top','heap_limit','min_heap')]
READ=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_uint32,c.c_void_p,c.c_uint32)
LIB=c.CDLL(str(ROOT/'build/elf-test.so'));LIB.k32_plan.argtypes=[READ,c.c_void_p,c.c_uint32,c.POINTER(Plan)]
def parse(data):
    def read(_,at,dest,n):
        if at>len(data) or n>len(data)-at:return 0
        c.memmove(dest,bytes(data[at:at+n]),n);return 1
    plan=Plan();error=LIB.k32_plan(READ(read),None,len(data),c.byref(plan));return error,plan
def fixture():return bytearray((ROOT/'build/MEM32.EXE').read_bytes())
class ElfValidation(unittest.TestCase):
    def test_resource_abi_stack_guard_and_heap_reservation(self):
        spec=importlib.util.spec_from_file_location('package',ROOT/'package.py');pkg=importlib.util.module_from_spec(spec);spec.loader.exec_module(pkg)
        elf=(ROOT/'build/practical.ext.elf').read_bytes()
        for stack in (4096,65536,262144):
            b=pkg.package(elf,258,15,stack_bytes=stack,min_heap=1048576)
            e,p=parse(b);self.assertEqual(e,0);self.assertEqual(p.stack_top,0xbfd000);self.assertEqual(p.stack_base,0xbfd000-stack);self.assertEqual(p.heap_limit,p.stack_base-4096);self.assertEqual(p.min_heap,1048576)
            resource=struct.unpack_from('<I',b,84)[0]-24
            for offset,value in ((0,0),(8,2),(10,23),(12,0),(12,4097),(12,266240),(16,0x400000),(20,1)):
                changed=bytearray(b);struct.pack_into('<I' if offset not in (8,10) else '<H',changed,resource+offset,value)
                self.assertNotEqual(parse(changed)[0],0,(stack,offset,value))
        for stack in (0,4095,4097,266240):
            with self.assertRaises(ValueError):pkg.package(elf,258,15,stack_bytes=stack)
        with self.assertRaises(ValueError):pkg.package(elf,257,15,stack_bytes=4096)
    def test_real_sdk_applications(self):
        for name in ('HELLO32','MEM32','BADPTR32','SPIN32','FAULT32'):
            with self.subTest(name=name):self.assertEqual(parse((ROOT/f'build/{name}.EXE').read_bytes())[0],0)
        data=fixture();error,p=parse(data);self.assertEqual(error,0)
        self.assertGreater(len(data),65536);self.assertGreater(p.segments[0].filesz,8192)
        self.assertGreaterEqual(p.rwbytes,2*1024*1024)
    def test_extended_abi_and_feature_requirements(self):
        for name in ('ARGS32','HEAP32','INPUT32','FILE32','VIEW32'):
            b=(ROOT/f'build/{name}.EXE').read_bytes();error,p=parse(b)
            self.assertEqual(error,0);self.assertEqual(p.abi,257);self.assertEqual(p.features,15)
        b=bytearray(b);struct.pack_into('<I',b,92,16);self.assertNotEqual(parse(b)[0],0)
        struct.pack_into('<I',b,92,0);self.assertEqual(parse(b)[0],0)
        struct.pack_into('<H',b,80,258);self.assertNotEqual(parse(b)[0],0)
    def test_xarray32_abi_and_cpu_record(self):
        b=(ROOT/'build/XARRAY32.EXE').read_bytes();error,p=parse(b)
        self.assertEqual(error,0);self.assertEqual((p.abi,p.features),(257,7))
        symbols={line.split()[3]:(int(line.split()[0],16),int(line.split()[1],16)) for line in subprocess.check_output(['nm','-S',str(ROOT/'build/xarray.ext.elf')],text=True).splitlines() if len(line.split())==4}
        self.assertEqual(symbols['xarray_result'],(p.rwbase,64))
    def test_existing_v86_classifier_rejects_pm32_kind(self):
        subprocess.run(['cc','-shared','-fPIC',str(ROOT/'../k386/app.c'),'-o',str(ROOT/'build/old-app-test.so')],check=True)
        lib=c.CDLL(str(ROOT/'build/old-app-test.so'));lib.kapp_header.argtypes=[c.c_void_p,c.c_uint];lib.kapp_pm32.argtypes=[c.c_void_p,c.c_uint]
        for name in ('HELLO32','MEM32','SPIN32','BADPTR32','FAULT32','ARGS32','HEAP32','INPUT32','FILE32','VIEW32','XARRAY32'):
            b=(ROOT/f'build/{name}.EXE').read_bytes();self.assertEqual(lib.kapp_header(c.create_string_buffer(b[:96]),96),-1)
            self.assertEqual(lib.kapp_pm32(c.create_string_buffer(b[:96]),96),1)
        b=bytearray(b[:96]);b[40:72]=b'A'*32;self.assertEqual(lib.kapp_pm32(c.create_string_buffer(bytes(b)),96),0)
    def test_runner_allocation_and_private_metadata(self):
        b=(ROOT/'build/K32RUN.EXE').read_bytes();h=struct.unpack_from('<14H',b);module=b[h[4]*16:]
        self.assertEqual(h[0],0x5a4d);self.assertEqual(h[5],h[6]);self.assertEqual(((len(module)+15)//16+h[5])*16,65536)
        symbols={}
        for line in subprocess.check_output(['nm','-n',str(ROOT/'build/run.elf')],text=True).splitlines():
            fields=line.split()
            if len(fields)==3:symbols[fields[2]]=int(fields[0],16)
        self.assertLess(symbols['__bss_end'],0xd000)
        self.assertEqual(module[symbols['results']:symbols['results']+8],b'K32RUN01')
        self.assertLessEqual(symbols['loader_map']+1024,symbols['__bss_end'])
        self.assertLessEqual(symbols['loader_buffer']+4096,symbols['__bss_end'])
    def test_host_private_state_and_stack_bounds(self):
        symbols={line.split()[2]:int(line.split()[0],16) for line in subprocess.check_output(['nm','-n',str(ROOT/'build/host.elf')],text=True).splitlines() if len(line.split())==3}
        self.assertLessEqual(symbols['__task_end']-symbols['__task_start'],7168)
        self.assertLess(symbols['__bss_end'],0xd000)
        for name in ('results','context','initialized','loader_map','loader_plan'):
            self.assertGreaterEqual(symbols[name],symbols['__task_start']);self.assertLess(symbols[name],symbols['__task_end'])
        b=(ROOT/'build/K32HOST.EXE').read_bytes();h=struct.unpack_from('<14H',b)
        self.assertEqual(((len(b)-h[4]*16+15)//16+h[5])*16,65536)
        self.assertEqual(b[28:36],b'K386APP1');self.assertEqual(struct.unpack_from('<H',b,38)[0],2)
    def test_program_headers_do_not_require_section_headers(self):
        b=fixture();base=struct.unpack_from('<I',b,84)[0]
        struct.pack_into('<I',b,base+32,0);struct.pack_into('<HHH',b,base+46,0,0,0)
        self.assertEqual(parse(b)[0],0)
    def test_container_rejections(self):
        for offset,value in ((0,0),(36,2),(38,1),(80,2),(82,0),(84,0xffffffff),(88,0xffffffff),(92,1)):
            with self.subTest(offset=offset):
                b=fixture();struct.pack_into('<I' if offset>=84 else '<H',b,offset,value);self.assertNotEqual(parse(b)[0],0)
        for n in (0,95,4096,len(fixture())-1):self.assertNotEqual(parse(fixture()[:n])[0],0)
    def test_elf_headers_and_entry(self):
        for offset,value,size in ((4,2,1),(5,2,1),(16,3,2),(18,62,2),(24,0x10000,4),(28,0xfffffff0,4),(42,31,2),(44,65535,2)):
            b=fixture();base=struct.unpack_from('<I',b,84)[0];struct.pack_into({1:'<B',2:'<H',4:'<I'}[size],b,base+offset,value)
            with self.subTest(offset=offset):self.assertNotEqual(parse(b)[0],0)
    def test_segments_and_integer_overflow(self):
        for offset,value in ((0,2),(4,0xfffff000),(8,0x10000),(8,0xfffff000),(16,0xffffffff),(20,0xffffffff),(24,7),(28,3)):
            b=fixture();base=struct.unpack_from('<I',b,84)[0];ph=base+struct.unpack_from('<I',b,base+28)[0];struct.pack_into('<I',b,ph+offset,value)
            with self.subTest(offset=offset,value=value):self.assertNotEqual(parse(b)[0],0)
    def test_page_overlap_and_truncated_reader(self):
        b=fixture();base=struct.unpack_from('<I',b,84)[0];ph=base+struct.unpack_from('<I',b,base+28)[0]
        first=struct.unpack_from('<I',b,ph+8)[0];struct.pack_into('<I',b,ph+32+8,first);self.assertNotEqual(parse(b)[0],0)
        p=Plan();self.assertNotEqual(LIB.k32_plan(READ(lambda *_:0),None,10000,c.byref(p)),0)
if __name__=='__main__':unittest.main()
