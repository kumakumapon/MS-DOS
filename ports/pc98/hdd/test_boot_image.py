import hashlib
import json
from pathlib import Path
import struct
import subprocess
import unittest

from boot_image import compose, HEADER, PHYSICAL, START, BPS, FAT, ROOT, DATA, CLUSTERS

HERE = Path(__file__).resolve().parent


def base_volume():
    image = bytearray(HEADER+PHYSICAL*256)
    struct.pack_into('<H', image, 0, 615)
    image[HEADER:HEADER+8] = b'\xeb\x0a\x90\x90IPL1'
    image[HEADER+254:HEADER+256] = b'\x55\xaa'
    partition = b'\xa1\x91\0\0\0\0\x01\0\0\0\x01\0\0\0\x66\x02KHDPROOF        '
    image[HEADER+256:HEADER+288] = partition
    image[START+3:START+36] = b'KHDPRF01'+struct.pack('<HBHBHHBHHHII',1024,4,1,2,512,40524,0xf8,20,33,8,66,0)
    image[START+43:START+62] = b'KHDPROOF   FAT16   '
    image[START+510:START+512] = b'\x55\xaa'
    for at in (FAT, FAT+20*BPS):
        struct.pack_into('<HHH', image, at, 0xfff8, 0xffff, 0xffff)
    image[ROOT:ROOT+11] = b'HDD     ID '
    image[ROOT+11] = 32
    struct.pack_into('<HI', image, ROOT+26, 2, 8)
    image[DATA:DATA+8] = b'KHDPROOF'
    return image


class BootImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ipl = (HERE/'build/boot1.bin').read_bytes()
        cls.loader = (HERE/'build/boot2.bin').read_bytes()
        cls.files = {'IO.SYS':b'I'*17001, 'MSDOS.SYS':b'D'*37376,
                     'COMMAND.COM':b'C'*99, 'EMPTY.TXT':b''}

    def test_native_sizes_and_copy_only_system_composition(self):
        base = base_volume()
        original = hashlib.sha256(base).digest()
        image, info = compose(base, self.files, self.ipl, self.loader)
        self.assertEqual(hashlib.sha256(base).digest(), original)
        self.assertEqual(len(image), len(base))
        self.assertEqual(image[HEADER:HEADER+256], self.ipl)
        self.assertEqual(image[FAT:FAT+20*BPS], image[FAT+20*BPS:FAT+40*BPS])
        self.assertEqual(image[DATA:DATA+4096], base[DATA:DATA+4096])
        descriptor = HEADER+info['descriptor_physical_offset']
        self.assertEqual(image[descriptor:descriptor+12], b'KHDBOOT1'+struct.pack('<HH',1,32))
        for i,name in enumerate(('IO.SYS','MSDOS.SYS')):
            e = next(e for e in info['files'] if e['name']==name)
            logical,count,size,cluster = struct.unpack_from('<HHIH',image,descriptor+12+i*10)
            self.assertEqual((size,cluster),(len(self.files[name]),e['cluster']))
            self.assertEqual((logical,count),(57+(cluster-2)*4,(size+1023)//1024))
            self.assertEqual(image[DATA+(cluster-2)*4096:DATA+(cluster-2)*4096+size],self.files[name])
            for c in range(cluster,cluster+(size+4095)//4096):
                actual = struct.unpack_from('<H',image,FAT+c*2)[0]
                self.assertEqual(actual,c+1 if c<cluster+(size+4095)//4096-1 else 0xffff)
        self.assertFalse(info['fd_required'])
        self.assertEqual(info['dos_boot_drive'],'A:')

    def test_wrong_geometry_marker_or_fat_is_refused_without_source_changes(self):
        for offset in (0, HEADER+256, START+3, START+13, FAT+20*BPS+4):
            base = base_volume();base[offset] ^= 1
            before = bytes(base)
            with self.assertRaises(ValueError):compose(base,self.files,self.ipl,self.loader)
            self.assertEqual(base,before)

    def test_payload_bounds_and_required_shell(self):
        for name,size in [('IO.SYS',0),('IO.SYS',0xf401),('MSDOS.SYS',0xa001),('COMMAND.COM',0)]:
            files = {**self.files,name:b'X'*size}
            with self.assertRaises(ValueError):compose(base_volume(),files,self.ipl,self.loader)
        with self.assertRaises(ValueError):compose(base_volume(),self.files,self.ipl[:-1],self.loader)
        with self.assertRaises(ValueError):compose(base_volume(),self.files,self.ipl,self.loader[:-1])

    def test_existing_names_and_noncanonical_paths_are_not_overwritten(self):
        for name in ['HDD.ID','../X.TXT','lower.txt','COM1.TXT']:
            files = {**self.files,name:b'x'}
            with self.assertRaises(ValueError):compose(base_volume(),files,self.ipl,self.loader)

    def test_full_or_fragmented_allocation_is_refused(self):
        for fragmented in (False,True):
            base = base_volume()
            for c in range(3,CLUSTERS+2):
                value = 0xffff if not fragmented or c%2==0 else 0
                for offset in (FAT,FAT+20*BPS):struct.pack_into('<H',base,offset+c*2,value)
            with self.assertRaises(ValueError):compose(base,self.files,self.ipl,self.loader)

    def test_built_oem_reuses_exact_driver_and_reserves_bss(self):
        path = HERE/'build/oem/IO-HDD.SYS'
        if not path.exists():self.skipTest('build HDD OEM profile first')
        io = path.read_bytes();meta = json.loads((path.parent/'hdd-oem.json').read_text())
        driver = (HERE/'build/KHD98.SYS').read_bytes()
        symbols = subprocess.check_output(['nm',str(HERE/'build/driver.elf')],text=True)
        resident = next(int(line.split()[0],16) for line in symbols.splitlines() if line.split()[-1]=='resident_end')
        at = meta['driver_offset']
        self.assertEqual(at%16,0)
        self.assertEqual(io[at:at+len(driver)],driver)
        self.assertEqual(io[at+len(driver):at+resident],bytes(resident-len(driver)))
        self.assertLessEqual(len(io),0xf400)


if __name__ == '__main__':unittest.main()
