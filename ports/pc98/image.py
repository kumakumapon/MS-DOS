"""Deterministic PC-98 2HD FAT12 image with the real bundled DOS 2.0 kernel."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def flatten_exe(data, segment=0xC0):
    if data[:2] != b'MZ':
        raise ValueError('expected linked MZ BIOS')
    last, pages, count, header = struct.unpack_from('<4H', data, 2)
    size = (pages - 1) * 512 + (last or 512)
    if size > len(data):
        raise ValueError('truncated EXE')
    offset = struct.unpack_from('<H', data, 24)[0]
    result = bytearray(data[header * 16:size])
    for i in range(count):
        off, seg = struct.unpack_from('<HH', data, offset + i * 4)
        pos = seg * 16 + off
        value = struct.unpack_from('<H', result, pos)[0]
        struct.pack_into('<H', result, pos, (value + segment) & 0xFFFF)
    return bytes(result)


def make_image(ipl, bios, extra=(), *, kernel=None, command=None, autoexec=None):
    # IO.SYS must stay below the kernel's final 1000h segment and the IPL
    # loads each payload without wrapping its 16-bit buffer offset.
    limit = 0xF400 if kernel is not None else 0x4000
    if len(ipl) != 1024 or len(bios) > limit:
        raise ValueError('invalid IPL size or oversized BIOS')
    image = bytearray(1232 * 1024)
    image[:1024] = ipl
    count_offset = image.index(b'COUNTS') + 6
    if kernel is None:
        kernel = (ROOT / 'v2.0/bin/MSDOS.SYS').read_bytes()
    if not kernel or len(kernel) > 0xA000:
        raise ValueError('invalid kernel load size')
    if command is None:
        command = (ROOT / 'v2.0/bin/COMMAND.COM').read_bytes()
    if autoexec is None:
        autoexec = (Path(__file__).parent / 'AUTOEXEC.BAT').read_text().replace('\n', '\r\n').encode('ascii')
    struct.pack_into('<HH', image, count_offset,
                     (len(bios) + 1023) // 1024, (len(kernel) + 1023) // 1024)
    files = [('IO.SYS', bios), ('MSDOS.SYS', kernel),
             ('COMMAND.COM', command),
             ('AUTOEXEC.BAT', autoexec),
             ('DOSLIC.TXT', (ROOT / 'LICENSE').read_bytes()),
             *extra]
    fat = bytearray(2048)
    def set_fat(cluster, value):
        p = cluster * 3 // 2
        old = struct.unpack_from('<H', fat, p)[0]
        value = (old & 0xF) | (value << 4) if cluster & 1 else (old & 0xF000) | value
        struct.pack_into('<H', fat, p, value)
    set_fat(0, 0xFFE)
    set_fat(1, 0xFFF)
    cluster = 2
    manifest = []
    seen = set()
    for i, (name, content) in enumerate(files):
        if not re.fullmatch(r'[A-Z0-9_]{1,8}\.[A-Z0-9_]{1,3}', name.upper()):
            raise ValueError('DOS 8.3 filename required')
        stem, ext = name.upper().split('.')
        if not (1 <= len(stem) <= 8 and 1 <= len(ext) <= 3):
            raise ValueError('DOS 8.3 filename required')
        key = stem.ljust(8) + ext.ljust(3)
        if key in seen:
            raise ValueError('duplicate DOS filename')
        seen.add(key)
        count = max(1, (len(content) + 1023) // 1024)
        if cluster + count > 1223 or i >= 192:
            raise ValueError('disk full')
        for c in range(cluster, cluster + count):
            set_fat(c, 0xFFF if c == cluster + count - 1 else c + 1)
        root = 5 * 1024 + i * 32
        image[root:root+11] = key.encode('ascii')
        image[root+11] = 6 if i < 2 else 0x20
        struct.pack_into('<HHHI', image, root+22, 0, 0x21, cluster, len(content))
        pos = (11 + cluster - 2) * 1024
        image[pos:pos+len(content)] = content
        manifest.append({'name': name.upper(), 'size': len(content),
                         'sha256': hashlib.sha256(content).hexdigest()})
        cluster += count
    image[1024:3072] = fat
    image[3072:5120] = fat
    return image, manifest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build', type=Path, default=Path(__file__).parent / 'build')
    parser.add_argument('--add', type=Path, action='append', default=[])
    args = parser.parse_args()
    bios = flatten_exe((args.build / 'bios.exe').read_bytes())
    (args.build / 'IO.SYS').write_bytes(bios)
    if len(bios) > 0x4000:
        raise ValueError('OEM BIOS/SYSINIT exceeds IPL load area')
    image, files = make_image((args.build / 'ipl.bin').read_bytes(), bios,
                             [(p.name, p.read_bytes()) for p in args.add])
    output = args.build / 'msdos2-pc98.xdf'
    output.write_bytes(image)
    (args.build / 'manifest.json').write_text(json.dumps({
        'image': output.name, 'sha256': hashlib.sha256(image).hexdigest(),
        'files': files}, indent=2) + '\n')
    print(output)

if __name__ == '__main__':
    main()
