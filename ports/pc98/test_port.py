import hashlib
import struct
import unittest
from pathlib import Path
from build import trim_omf, normalize_omf
from image import ROOT, make_image, flatten_exe

HERE = Path(__file__).parent


def records(data):
    offset = 0
    while offset < len(data):
        length = struct.unpack_from('<H', data, offset+1)[0]
        raw = data[offset:offset+3+length]
        if sum(raw) & 255:
            raise ValueError('checksum')
        yield raw[0], raw[3:-1]
        offset += 3 + length


def read_root(image, name):
    target = ''.join(part.ljust(size) for part, size in zip(name.split('.'), (8, 3))).encode()
    for offset in range(5120, 11264, 32):
        if image[offset:offset+11] == target:
            cluster = struct.unpack_from('<H', image, offset+26)[0]
            size = struct.unpack_from('<I', image, offset+28)[0]
            out = bytearray()
            seen = set()
            while size:
                if cluster in seen or not 2 <= cluster < 1223:
                    raise ValueError('FAT chain')
                seen.add(cluster)
                pos = (cluster+9)*1024
                count = min(size, 1024)
                out.extend(image[pos:pos+count])
                size -= count
                value = struct.unpack_from('<H', image, 1024+cluster*3//2)[0]
                cluster = value >> 4 if cluster & 1 else value & 0xFFF
            return bytes(out)
    raise FileNotFoundError(name)


class PortTests(unittest.TestCase):
    def test_original_omf_checksums_and_padding(self):
        for name in ('SYSINIT', 'SYSIMES'):
            original = (ROOT/'v2.0/bin'/f'{name}.OBJ').read_bytes()
            trimmed = trim_omf(original)
            self.assertIn(list(records(trimmed))[-1][0], (0x8A, 0x8B))
            self.assertTrue(all(b == 0 for b in original[len(trimmed):]))
        with self.assertRaises(ValueError):
            trim_omf(b'\x80\x01\x00\x00')

    def test_sysinit_exec_pointer_relocations(self):
        original = trim_omf((ROOT/'v2.0/bin/SYSINIT.OBJ').read_bytes())
        normalized = normalize_omf(original)
        values = list(records(normalized))
        self.assertNotIn(0xA2, [kind for kind, _ in values])
        for i, (kind, body) in enumerate(values):
            if kind == 0xA0 and body[:3] == b'\x01\x35\x00':
                self.assertEqual(body[3:], b'\0'*14)
                fixes = values[i+1][1]
                self.assertEqual([fixes[j+1] for j in range(0, 21, 7)], [2, 6, 10])
                break
        else:
            self.fail('EXEC parameter block missing')
        self.assertEqual(normalize_omf(normalized), normalized)

    def test_kernel_and_command_are_original(self):
        ipl = bytearray(1024)
        ipl[100:106] = b'COUNTS'
        image, manifest = make_image(ipl, b'BIOS'*800)
        self.assertEqual(len(image), 1261568)
        self.assertEqual(image[1024:3072], image[3072:5120])
        for name in ('MSDOS.SYS', 'COMMAND.COM'):
            self.assertEqual(read_root(image, name), (ROOT/'v2.0/bin'/name).read_bytes())
        self.assertEqual(read_root(image, 'IO.SYS'), b'BIOS'*800)
        self.assertIn(b'\r\n', read_root(image, 'AUTOEXEC.BAT'))
        self.assertEqual(struct.unpack_from('<HH', image, 106), (4, 17))
        self.assertEqual(hashlib.sha256(image).digest(), hashlib.sha256(make_image(ipl, b'BIOS'*800)[0]).digest())
        self.assertEqual(len(manifest), 5)
        self.assertEqual(read_root(image, 'DOSLIC.TXT'), (ROOT/'LICENSE').read_bytes())

    def test_file_safety(self):
        ipl = bytearray(1024)
        ipl[100:106] = b'COUNTS'
        with self.assertRaises(ValueError):
            make_image(ipl, b'BIOS', [('COMMAND.COM', b'bad')])
        with self.assertRaises(ValueError):
            make_image(ipl, b'BIOS', [('TOOLONGNAME.COM', b'bad')])
        with self.assertRaises(ValueError):
            make_image(ipl, b'BIOS', [('HUGE.BIN', bytes(1300000))])

    def test_real_linked_exec_block(self):
        exe = HERE/'build/bios.exe'
        if not exe.exists():
            self.skipTest('run build.py first')
        import re
        map_text = (HERE/'build/bios.map').read_text()
        segment = int(re.search(r'([0-9a-f]+):0000\s+SYSINIT\b', map_text).group(1), 16)
        bios = flatten_exe(exe.read_bytes())
        data = bios[segment*16+0x35:segment*16+0x43]
        self.assertEqual(struct.unpack('<7H', data), (0, 0x14, segment+0xC0, 0x11, segment+0xC0, 0x34, segment+0xC0))

    def test_dos4_image_bundles_xms_and_ems(self):
        image_path = HERE/'build/dos4/msdos4-pc98.xdf'
        if not image_path.exists():
            self.skipTest('run dos4.py first')
        image = image_path.read_bytes()
        config = read_root(image, 'CONFIG.SYS')
        self.assertIn(b'DEVICE=FDXMS286.SYS\r\n', config)
        self.assertIn(b'DEVICE=EMM386.EXE EMM=8192\r\n', config)
        for name in ('FDXMS286.SYS', 'EMM386.EXE', 'XMSLIC.TXT', 'EMM386L.TXT',
                     'XMSCHK.COM', 'EMSCHK.COM'):
            self.assertTrue(read_root(image, name), name)

if __name__ == '__main__':
    unittest.main()
