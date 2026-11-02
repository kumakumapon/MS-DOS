"""Inspect the linked diagnostic's privilege gates and actual allocation bounds."""
from pathlib import Path
import struct
import subprocess
import unittest

ROOT = Path(__file__).resolve().parent
IMAGE = (ROOT / 'build/K32CPU.COM').read_bytes()
SYMBOLS = {}
for line in subprocess.check_output(['nm', '-n', str(ROOT / 'build/probe.elf')], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        SYMBOLS[fields[2]] = int(fields[0], 16)


def data(name, size):
    offset = SYMBOLS[name] - 0x100
    return IMAGE[offset:offset + size]


class ProbeLayout(unittest.TestCase):
    def test_privilege_descriptors(self):
        gdt = data('gdt', 72)
        for index, access in ((1, 0x9a), (2, 0x92), (3, 0x92), (4, 0x9a), (5, 0x92), (6, 0x89), (7, 0xfa), (8, 0xf2)):
            self.assertEqual(gdt[index * 8 + 5], access)
        self.assertEqual(gdt[7 * 8 + 6], 0xcf)
        self.assertEqual(gdt[8 * 8 + 6], 0xcf)
        self.assertLess((7 * 8) | 3, 0x200)  # #DF saved CS must not look like IF.
        self.assertLess((8 * 8) | 3, 0x200)
        self.assertEqual(struct.unpack_from('<H', gdt, 6 * 8)[0], 103)

    def test_hardware_fault_and_user_gates(self):
        idt = data('idt', 49 * 8)
        for index in (8, 13, 14, 48):
            off, selector, zero, access, high = struct.unpack_from('<HHBBH', idt, index * 8)
            self.assertEqual(selector, 8)
            self.assertEqual(zero, 0)
            self.assertEqual(high, 0)
            self.assertEqual(access, 0xee if index == 48 else 0x8e)
            self.assertEqual(off, SYMBOLS[{8: 'irq_or_df', 13: 'vector_13', 14: 'vector_14', 48: 'user_exit'}[index]])

    def test_code_pages_records_and_stacks(self):
        self.assertLess(SYMBOLS['__bss_end'], 0xc000)
        self.assertLessEqual(SYMBOLS['user_end'] - SYMBOLS['user_code'], 4096)
        self.assertGreaterEqual(SYMBOLS['__bss_end'] - SYMBOLS['pages'], 6 * 4096 + 4095)
        record = data('results', 576)
        self.assertEqual(record[:8], b'K32CPU01')
        self.assertEqual(struct.unpack_from('<HH', record, 8), (0x100, 576))
        self.assertEqual(struct.unpack_from('<I', record, 80)[0], 5)
        self.assertEqual(struct.unpack_from('<I', record, 84)[0], SYMBOLS['results'])
        self.assertEqual(record[96:], bytes(5 * 96))
        self.assertLess(len(IMAGE) + 0x100, SYMBOLS['tss'])


if __name__ == '__main__':
    unittest.main()
