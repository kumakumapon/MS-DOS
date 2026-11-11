"""Offline boot composition for a NEW marked KHD98 volume; never mount a disk."""
import hashlib
import re
import struct

HEADER = 256
PHYSICAL = 162360
START = HEADER + 264 * 256
BPS = 1024
CLUSTER = 4096
FAT = START + BPS
ROOT = START + 41 * BPS
DATA = START + 57 * BPS
CLUSTERS = 10116


def compose(base, files, ipl, loader):
    """Return a separate immutable-source copy plus reproducible boot metadata."""
    if len(base) != HEADER + PHYSICAL * 256 or base[:2] != struct.pack('<H', 615):
        raise ValueError('Expected the KHD98 THD geometry')
    partition = b'\xa1\x91\0\0\0\0\x01\0\0\0\x01\0\0\0\x66\x02KHDPROOF        '
    if base[HEADER+256:HEADER+288] != partition or any(base[HEADER+288:HEADER+768]):
        raise ValueError('Expected one marked KHD98 partition')
    bpb = b'KHDPRF01' + struct.pack('<HBHBHHBHHHII', 1024, 4, 1, 2, 512, 40524, 0xf8, 20, 33, 8, 66, 0)
    if base[START+3:START+36] != bpb or base[START+43:START+62] != b'KHDPROOF   FAT16   ' or base[START+510:START+512] != b'\x55\xaa':
        raise ValueError('Unsupported BPB/volume marker')
    if base[FAT:FAT+20*BPS] != base[FAT+20*BPS:FAT+40*BPS]:
        raise ValueError('FAT copies disagree')
    if struct.unpack_from('<HH', base, FAT) != (0xfff8, 0xffff):
        raise ValueError('Invalid FAT16 reserved entries')
    if len(ipl) != 256 or ipl[4:8] != b'IPL1' or ipl[254:] != b'\x55\xaa':
        raise ValueError('Invalid native HDD IPL')
    if len(loader) != 4096 or loader[3:11] != b'KHDS2V1!' or loader.count(b'KHDBOOT1') != 1:
        raise ValueError('Invalid native HDD system loader')
    for name, ceiling in [('IO.SYS', 0xf400), ('MSDOS.SYS', 0xa000)]:
        if not 0 < len(files.get(name, b'')) <= ceiling:
            raise ValueError('System payload exceeds its load region: '+name)
    if not files.get('COMMAND.COM'):
        raise ValueError('COMMAND.COM is required')
    image = bytearray(base)
    fat = lambda c: struct.unpack_from('<H', image, FAT+c*2)[0]
    def link(c, value):
        for offset in (FAT, FAT+20*BPS):
            struct.pack_into('<H', image, offset+c*2, value)
    names = set()
    empty = []
    for at in range(ROOT, DATA, 32):
        if image[at] in (0, 0xe5):
            empty.append(at)
        else:
            names.add(bytes(image[at:at+11]))
    if len(empty) < len(files):
        raise ValueError('Root directory is full')
    entries = []
    ordered = ['IO.SYS', 'MSDOS.SYS'] + [n for n in files if n not in ('IO.SYS', 'MSDOS.SYS')]
    for index, name in enumerate(ordered):
        if not re.fullmatch(r'[A-Z0-9_]{1,8}\.[A-Z0-9_]{1,3}', name):
            raise ValueError('Canonical ASCII 8.3 name required: '+name)
        stem, ext = name.split('.')
        if stem in ('CON', 'PRN', 'AUX', 'NUL', 'CLOCK$') or re.fullmatch(r'(COM|LPT)[0-9]', stem):
            raise ValueError('Device names are not disk files: '+name)
        key = (stem.ljust(8)+ext.ljust(3)).encode('ascii')
        if key in names:
            raise ValueError('Existing root entry must not be replaced: '+name)
        names.add(key)
        content = files[name]
        count = (len(content)+CLUSTER-1)//CLUSTER
        free = [c for c in range(2, CLUSTERS+2) if not fat(c)]
        if len(free) < count:
            raise ValueError('Insufficient HDD capacity')
        chain = free[:count]
        if name in ('IO.SYS', 'MSDOS.SYS') and count:
            run = []
            for c in free:
                if run and c != run[-1]+1:
                    run = []
                run.append(c)
                if len(run) == count:
                    break
            if len(run) != count:
                raise ValueError('Contiguous system allocation is unavailable')
            chain = run
        for i, c in enumerate(chain):
            link(c, chain[i+1] if i+1<len(chain) else 0xffff)
            at = DATA+(c-2)*CLUSTER
            image[at:at+CLUSTER] = content[i*CLUSTER:(i+1)*CLUSTER].ljust(CLUSTER, b'\0')
        at = empty[index]
        image[at:at+32] = bytes(32)
        image[at:at+11] = key
        image[at+11] = 6 if name in ('IO.SYS', 'MSDOS.SYS') else 33
        struct.pack_into('<HHI', image, at+24, 0x21, chain[0] if chain else 0, len(content))
        entries.append({'name':name, 'bytes':len(content), 'cluster':chain[0] if chain else 0,
                        'sha256':hashlib.sha256(content).hexdigest()})
    second = bytearray(loader)
    descriptor = second.index(b'KHDBOOT1')
    for index, name in enumerate(('IO.SYS', 'MSDOS.SYS')):
        e = next(e for e in entries if e['name']==name)
        logical = 57+(e['cluster']-2)*4
        struct.pack_into('<HHIH', second, descriptor+12+index*10,
                         logical, (e['bytes']+1023)//1024, e['bytes'], e['cluster'])
    image[HEADER:HEADER+256] = ipl
    image[HEADER+4*256:HEADER+4*256+4096] = second
    return bytes(image), {'files':entries, 'descriptor_physical_offset':4*256+descriptor,
                         'boot_device':'SASI unit 0', 'dos_boot_drive':'A:',
                         'fd_required':False, 'system_loader_bytes':4096}
