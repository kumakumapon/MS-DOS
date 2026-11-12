"""Build the separate HDD OEM IO.SYS from pinned SYSINIT and native KHD98 code."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from dos4 import core, build_port
from image import ROOT, flatten_exe


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dosbox', required=True, type=Path)
    p.add_argument('--jwasm', required=True, type=Path)
    p.add_argument('--work', type=Path, default=ROOT/'.work/pc98-hdd-oem')
    p.add_argument('--output', type=Path, default=Path(__file__).parent/'build/oem')
    a = p.parse_args()
    a.work = a.work.resolve()
    a.output = a.output.resolve()
    if not a.work.exists():
        core.prepare(a.work)
    checkpoint = json.loads((a.work/'checkpoint.json').read_text())
    if checkpoint.get('base') != core.BASE:
        raise ValueError('Unexpected SYSINIT source base')
    a.output.mkdir(parents=True, exist_ok=True)
    build_port(a.work, a.output, str(a.dosbox.resolve()), str(a.jwasm.resolve()),
               hdd_boot=True, oem_only=True)
    io = flatten_exe((a.output/'bios.exe').read_bytes())
    if not 0 < len(io) <= 0xf400:
        raise ValueError('HDD IO.SYS exceeds the boot load region')
    driver = (Path(__file__).parent/'build/KHD98.SYS').read_bytes()
    at = io.index(b'KHDDIOP1')-18
    if at % 16 or io[at:at+len(driver)] != driver:
        raise ValueError('Built-in HDD transport is not paragraph-aligned/exact')
    (a.output/'IO-HDD.SYS').write_bytes(io)
    (a.output/'hdd-oem.json').write_text(json.dumps({
        'sysinit_source_base':core.BASE, 'io_bytes':len(io),
        'io_sha256':hashlib.sha256(io).hexdigest(),
        'driver_offset':at, 'driver_sha256':hashlib.sha256(driver).hexdigest(),
        'dos_boot_drive':'A:', 'boot_device':'SASI unit 0',
    }, indent=2)+'\n')
    print('Built separate HDD IO.SYS:', len(io), 'bytes')


if __name__ == '__main__':
    main()
