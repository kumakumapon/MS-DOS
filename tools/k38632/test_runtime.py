"""Check the actual packaged runner's loader, privilege and allocation contracts."""
from pathlib import Path
import struct
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent
EXE = (ROOT / 'build/K32MEM.EXE').read_bytes()
S = {}
for line in subprocess.check_output(['nm', '-n', str(ROOT / 'build/runtime.elf')], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        S[fields[2]] = int(fields[0], 16)
HEADER = struct.unpack_from('<14H', EXE)
PAYLOAD = EXE[HEADER[4] * 16:]


class RuntimeContracts(unittest.TestCase):
    def test_real_dos_mz_allocation(self):
        self.assertEqual(HEADER[0], 0x5a4d)
        self.assertEqual((HEADER[2] - 1) * 512 + (HEADER[1] or 512), len(EXE))
        self.assertEqual(HEADER[3], 0)
        self.assertEqual(HEADER[5], HEADER[6])
        self.assertEqual(((len(PAYLOAD) + 15) // 16 + HEADER[5]) * 16, 0x10000)
        self.assertEqual(HEADER[7:9], (0, 0xfff0))
        self.assertEqual(HEADER[10:12], (S['_start'], 0))
        self.assertEqual(len(PAYLOAD), S['__mz_image_end'])

    def test_ring3_gates_and_descriptor_width(self):
        gdt = PAYLOAD[S['gdt']:S['gdt'] + 72]
        self.assertEqual(gdt[7 * 8 + 5:7 * 8 + 7], bytes((0xfa, 0xcf)))
        self.assertEqual(gdt[8 * 8 + 5:8 * 8 + 7], bytes((0xf2, 0xcf)))
        for index, label in ((8, 'irq_or_df'), (13, 'vector_13'), (14, 'vector_14'), (48, 'user_exit')):
            off, selector, zero, access, high = struct.unpack_from('<HHBBH', PAYLOAD, S['idt'] + index * 8)
            self.assertEqual((off, selector, zero, access, high),
                             (S[label], 8, 0, 0xee if index == 48 else 0x8e, 0))
        for modrm, offset in ((6, 116), (6, 122), (14, 128), (14, 134), (22, 116), (30, 128)):
            self.assertIn(bytes((0x66, 0x0f, 0x01, modrm)) + struct.pack('<H', S['results'] + offset), PAYLOAD)

    def test_records_and_private_stacks_are_inside_allocation(self):
        self.assertLess(S['__bss_end'], 0xd000)
        self.assertLess(S['context'] + 76, S['pages'])
        self.assertGreaterEqual(S['__bss_end'] - S['pages'], 3 * 4096 + 4095)
        self.assertLessEqual(S['user_end'] - S['user_code'], 4096)
        r = PAYLOAD[S['results']:S['results'] + 1024]
        self.assertEqual(r[:12], b'K32MEM01' + struct.pack('<HH', 256, 1024))
        self.assertEqual(struct.unpack_from('<I', r, 36)[0], 0x401000)
        self.assertEqual(struct.unpack_from('<I', r, 20)[0], 0)
        self.assertEqual(struct.unpack_from('<I', r, 488)[0], 0x200000)
        self.assertEqual(struct.unpack_from('<I', r, 504)[0], 0)


if __name__ == '__main__':
    unittest.main()
