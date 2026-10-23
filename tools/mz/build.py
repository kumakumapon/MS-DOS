#!/usr/bin/env python3
"""Package an explicitly linked 16-bit DOS load module as an MZ executable.

The linker defines image/memory bounds and the entry/stack registers. Symbols
named __mz_reloc_* identify segment words that DOS must relocate at load time.
No COM startup assumptions or implicit allocation of all free memory are used.
"""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def package(payload, symbols, app_title=None):
    required = ('_start', '__mz_cs', '__mz_ss', '__mz_sp',
                '__mz_image_end', '__mz_memory_bytes')
    if any(name not in symbols for name in required):
        raise ValueError('missing MZ linker contract')
    if len(payload) != symbols['__mz_image_end'] or not payload:
        raise ValueError('load image does not match linker bounds')
    memory = symbols['__mz_memory_bytes']
    if memory < len(payload) or memory > 0x100000:
        raise ValueError('invalid MZ allocation')
    ip, cs, ss, sp = (symbols[name] for name in
                      ('_start', '__mz_cs', '__mz_ss', '__mz_sp'))
    if any(value < 0 or value > 0xffff for value in (ip, cs, ss, sp)):
        raise ValueError('entry or stack register exceeds 16 bits')
    if cs * 16 + ip >= len(payload) or ss * 16 + sp > memory or sp < 16:
        raise ValueError('entry or stack outside allocation')
    relocations = sorted(value for name, value in symbols.items()
                         if name.startswith('__mz_reloc_'))
    if len(set(relocations)) != len(relocations):
        raise ValueError('duplicate relocation')
    for at in relocations:
        if at < 0 or at + 2 > len(payload):
            raise ValueError('relocation outside image')
        if struct.unpack_from('<H', payload, at)[0] * 16 >= memory:
            raise ValueError('relocated segment outside allocation')
    table = 28
    descriptor = b''
    if app_title is not None:
        title = app_title.encode('ascii')
        if not 1 <= len(title) <= 31 or any(c < 32 or c > 126 for c in title):
            raise ValueError('KDESK/386 title must be 1..31 printable ASCII bytes')
        descriptor = b'K386APP1' + struct.pack('<HH', 0x0100, 1) + title.ljust(32, b'\0')
        table += len(descriptor)
    header_size = (table + 4 * len(relocations) + 15) & ~15
    size = header_size + len(payload)
    extra = (memory + 15) // 16 - (len(payload) + 15) // 16
    header = bytearray(header_size)
    struct.pack_into('<14H', header, 0, 0x5a4d, size % 512,
                     (size + 511) // 512, len(relocations), header_size // 16,
                     extra, extra, ss, sp, 0, ip, cs, table, 0)
    header[28:table] = descriptor
    for index, at in enumerate(relocations):
        struct.pack_into('<HH', header, table + index * 4, at & 15, at >> 4)
    return bytes(header) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--k386-app', metavar='TITLE', help='KDESK/386 text application descriptor')
    parser.add_argument('elf', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    symbols = {}
    for line in subprocess.check_output(['nm', '--defined-only', str(args.elf)], text=True).splitlines():
        value, _, name = line.split()
        symbols[name] = int(value, 16)
    with tempfile.TemporaryDirectory() as directory:
        raw = Path(directory) / 'module.bin'
        subprocess.run(['objcopy', '-O', 'binary', str(args.elf), str(raw)], check=True)
        executable = package(raw.read_bytes(), symbols, args.k386_app)
    args.output.write_bytes(executable)
    print(f'{args.output}: MZ {len(executable)} bytes, module allocation '
          f'{symbols["__mz_memory_bytes"]} bytes plus PSP')


if __name__ == '__main__':
    main()
