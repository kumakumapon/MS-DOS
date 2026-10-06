"""Verify the v4.0 OEM adaptation and packaged original kernel/shell."""
import hashlib
import json
import re
import struct
import tempfile
import unittest
from pathlib import Path
from dos4 import patch_init, core
from image import ROOT, flatten_exe
from test_port import read_root

HERE = Path(__file__).resolve().parent


class Dos4Tests(unittest.TestCase):
    def test_no_ibm_init_services_or_stack_hooks(self):
        bios = ROOT/'v4.0/src/BIOS'
        for name in ('SYSINIT1.ASM', 'SYSINIT2.ASM', 'SYSCONF.ASM'):
            text = patch_init(name, (bios/name).read_bytes()).decode('ascii')
            self.assertRegex(text, r'STACKSW\s+EQU\s+FALSE')
            if name == 'SYSINIT1.ASM':
                instructions = '\n'.join(s.split(';')[0] for s in text.splitlines())
                self.assertNotRegex(instructions, r'(?i)\bint\s+1[15]h\b')
                self.assertNotRegex(instructions, r'(?i)\bout\s+dx,al\b')
            if name == 'SYSCONF.ASM':
                self.assertIn('IF NOT STACKSW\r\nDo_TryK:\r\n\tjmp BadOp', text)
        with self.assertRaises(ValueError):
            patch_init('SYSINIT1.ASM', b'changed upstream')

    def test_dos_names_on_case_sensitive_host(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            (root/'CMD').mkdir()
            (root/'CMD/TEST.COM').write_bytes(b'test')
            self.assertEqual(core.dos_path(root, 'cmd\\test.com').read_bytes(), b'test')
            with self.assertRaises(FileNotFoundError):
                core.dos_path(root, 'cmd/missing.com')

    def test_source_built_kernel_shell_and_contiguous_ipl_payloads(self):
        build = HERE/'build/dos4'
        if not (build/'msdos4-pc98.xdf').exists():
            self.skipTest('run dos4.py first')
        disk = (build/'msdos4-pc98.xdf').read_bytes()
        reference = json.loads((ROOT/'docs/validation/dos4/image-manifest.json').read_text())
        for name in ('MSDOS.SYS', 'COMMAND.COM'):
            content = read_root(disk, name)
            self.assertEqual(content, (build/name).read_bytes())
            self.assertEqual(hashlib.sha256(content).hexdigest(), reference[name]['sha256'])
        io = flatten_exe((build/'bios.exe').read_bytes())
        self.assertEqual(read_root(disk, 'IO.SYS'), io)
        bios_sectors, dos_sectors = struct.unpack_from('<HH', disk, disk.index(b'COUNTS')+6)
        self.assertEqual(bios_sectors, (len(io)+1023)//1024)
        self.assertEqual(dos_sectors, 37)
        start = (11+bios_sectors)*1024
        self.assertEqual(disk[start:start+37376], read_root(disk, 'MSDOS.SYS'))
        self.assertEqual(read_root(disk, 'CONFIG.SYS'),
                         b'FILES=20\r\nBUFFERS=8\r\nLASTDRIVE=A\r\n'
                         b'DEVICE=FDXMS286.SYS\r\nDEVICE=EMM386.EXE EMM=8192\r\n')
        self.assertEqual(disk[1024:3072], disk[3072:5120])


if __name__ == '__main__':
    unittest.main()
