"""Build on Linux with JWasm and JWlink (no DOSBox or commercial tools)."""
import argparse
import struct
import subprocess
from pathlib import Path
from image import ROOT

HERE = Path(__file__).resolve().parent


def trim_omf(data):
    offset = 0
    while offset + 3 <= len(data):
        kind = data[offset]
        length = struct.unpack_from('<H', data, offset + 1)[0]
        end = offset + 3 + length
        if end > len(data) or sum(data[offset:end]) & 255:
            raise ValueError('invalid OMF record')
        offset = end
        if kind in (0x8A, 0x8B):
            if any(data[end:]):
                raise ValueError('unexpected nonzero bytes after MODEND')
            return data[:end]
    raise ValueError('OMF MODEND missing')


def normalize_omf(data):
    """Expand OMF16 LIDATA and relocate its FIXUPP locations for JWlink.

    JWlink's LIDATA fixup path applies a fixup to unrelated literal blocks.
    Canonical LEDATA avoids corrupting SYSINIT's EXEC parameter block.
    The bundled object's LIDATA fixups use explicit segment frames/targets.
    """
    result = bytearray()
    mapping = None
    offset = 0
    def record(kind, body):
        raw = bytes([kind]) + struct.pack('<H', len(body) + 1) + body
        return raw + bytes([(-sum(raw)) & 255])
    while offset < len(data):
        kind = data[offset]
        size = struct.unpack_from('<H', data, offset + 1)[0]
        body = data[offset+3:offset+2+size]
        if kind == 0xA2:
            if body[0] & 0x80:
                raise ValueError('unsupported large LIDATA segment index')
            encoded = body[3:]
            expanded = bytearray()
            mapping = {}
            def block(pos):
                repeat, children = struct.unpack_from('<HH', encoded, pos)
                pos += 4
                start = len(expanded)
                if children:
                    for _ in range(children):
                        pos = block(pos)
                else:
                    count = encoded[pos]
                    pos += 1
                    for i in range(count):
                        mapping.setdefault(pos+i, []).append(len(expanded)+i)
                    expanded.extend(encoded[pos:pos+count])
                    pos += count
                content = expanded[start:]
                if repeat != 1:
                    # The only repeated bundled block is a zero-filled gap,
                    # with no fixups; reject fixups into repeated literals.
                    for positions in mapping.values():
                        if positions[-1] >= start:
                            positions.extend([-1] * (repeat-1))
                    expanded.extend(content * (repeat-1))
                return pos
            pos = 0
            while pos < len(encoded):
                pos = block(pos)
            body = body[:3] + expanded
            kind = 0xA0
        elif kind == 0x9C and mapping is not None:
            fixed = bytearray(body)
            if len(fixed) % 7:
                raise ValueError('unsupported LIDATA FIXUPP encoding')
            for i in range(0, len(fixed), 7):
                if fixed[i] & 0xFC != 0xCC or fixed[i+2:i+5] != b'\x00\x01\x01':
                    raise ValueError('unsupported LIDATA fixup type/frame/target')
                old = ((fixed[i] & 3) << 8) | fixed[i+1]
                positions = mapping.get(old)
                if not positions or len(positions) != 1 or positions[0] < 0:
                    raise ValueError('unsupported repeated LIDATA fixup')
                new = positions[0]
                fixed[i] = (fixed[i] & 0xFC) | (new >> 8)
                fixed[i+1] = new & 255
            body = fixed
        else:
            mapping = None
        result.extend(record(kind, body))
        offset += size + 3
    return bytes(result)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--jwasm', default='jwasm')
    p.add_argument('--jwlink', default='jwlink')
    p.add_argument('--build', type=Path, default=HERE / 'build')
    a = p.parse_args()
    a.build = a.build.resolve()
    a.build.mkdir(parents=True, exist_ok=True)
    (a.build / 'LICENSE.TXT').write_bytes((ROOT / 'LICENSE').read_bytes())
    for name in ('SYSINIT', 'SYSIMES'):
        (a.build / (name + '.OBJ')).write_bytes(normalize_omf(trim_omf(
            (ROOT / 'v2.0/bin' / (name + '.OBJ')).read_bytes())))
    subprocess.run([a.jwasm, '-Fo' + str(a.build / 'bios.obj'), str(HERE / 'bios.asm')], check=True)
    subprocess.run([a.jwasm, '-bin', '-Fo' + str(a.build / 'ipl.bin'), str(HERE / 'ipl.asm')], check=True)
    subprocess.run([a.jwlink, 'format', 'dos', 'name', str(a.build / 'bios.exe'),
                    'option', 'map=' + str(a.build / 'bios.map'), 'file',
                    ','.join(str(a.build / n) for n in ('bios.obj', 'SYSINIT.OBJ', 'SYSIMES.OBJ'))], check=True)

if __name__ == '__main__':
    main()
