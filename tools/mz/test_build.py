import struct
import unittest

from build import package


class MZTests(unittest.TestCase):
    def symbols(self, size=64, **changes):
        return dict({'_start': 0, '__mz_cs': 0, '__mz_ss': 0,
                     '__mz_sp': 0xfff0, '__mz_image_end': size,
                     '__mz_memory_bytes': 0x10000}, **changes)

    def test_separate_segments_relocate_at_different_load_addresses(self):
        # A module larger than COM's limit: 40 KiB code, 24 KiB data + stack.
        payload = bytearray(0x11000)
        struct.pack_into('<H', payload, 7, 0xa00)
        symbols = self.symbols(len(payload), __mz_ss=0xa00,
                               __mz_memory_bytes=0x1a000, __mz_reloc_ds=7)
        exe = package(payload, symbols)
        h = struct.unpack_from('<14H', exe)
        self.assertEqual(h[0], 0x5a4d)
        self.assertEqual(h[3], 1)
        self.assertEqual(h[7:9], (0xa00, 0xfff0))
        self.assertEqual(h[5], h[6])
        self.assertEqual((h[2] - 1) * 512 + (h[1] or 512), len(exe))
        self.assertEqual((len(payload) + 15) // 16 + h[5], 0x1a00)
        offset, segment = struct.unpack_from('<HH', exe, h[12])
        for base in (0x1800, 0x4200):
            loaded = bytearray(exe[h[4] * 16:])
            at = segment * 16 + offset
            value = struct.unpack_from('<H', loaded, at)[0]
            struct.pack_into('<H', loaded, at, value + base)
            self.assertEqual(struct.unpack_from('<H', loaded, 7)[0], base + h[7])
            self.assertNotEqual(base + h[11], base + h[7])

    def test_exact_page_size_and_flat_resident_module(self):
        exe = package(bytes(480), self.symbols(480))
        h = struct.unpack_from('<14H', exe)
        self.assertEqual(len(exe), 512)
        self.assertEqual(h[1:5], (0, 1, 0, 2))
        self.assertEqual(h[5], 4096 - 30)

    def test_invalid_memory_stack_entry_and_relocations_are_rejected(self):
        for changes in ({'__mz_image_end': 63}, {'__mz_memory_bytes': 16},
                        {'__mz_ss': 0x1000}, {'__mz_sp': 0x10000},
                        {'_start': 64}, {'__mz_reloc_ds': 63},
                        {'__mz_reloc_ds': 2, '__mz_reloc_other': 2}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                package(bytes(64), self.symbols(**changes))
        with self.assertRaises(ValueError):
            package(b'\xff\xff' + bytes(62), self.symbols(__mz_reloc_ds=0))


if __name__ == '__main__':
    unittest.main()
