"""Build the v4.0 kernel/shell and a PC-98 OEM IO.SYS in an isolated tree."""
import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
from pathlib import Path
from image import ROOT, flatten_exe, make_image

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('dos4_core', ROOT/'tools/dos4/build.py')
core = importlib.util.module_from_spec(spec)
spec.loader.exec_module(core)


def patch_init(name, data):
    text = data.decode('ascii').replace('\r\n', '\n')
    # IBM IRQ numbers, NMI ports and interrupt stack dispatch are incompatible
    # with NEC BIOS. Keep the generic CONFIG parser and DOS memory management.
    if name in ('SYSINIT1.ASM', 'SYSINIT2.ASM', 'SYSCONF.ASM'):
        text, count = re.subn(r'(STACKSW\s+EQU\s+)TRUE', r'\1FALSE', text)
        if count != 1:
            raise ValueError('STACKSW source changed: '+name)
    if name == 'SYSINIT1.ASM':
        start = text.index('\nGOINIT:')
        end = text.index('\nMove_Myself:', start)
        text = text[:start] + '\nGOINIT:\n; PC-98: memory size and boot drive supplied by OEM BIOS.\n' + text[end:]
        # No IBM INT 15h extended-memory query on PC-98.
        old = '\tmov\tah,88h\n\tint\t15h'
        if text.count(old) != 1:
            raise ValueError('extended memory source changed')
        text = text.replace(old, '\tstc\n\tnop\t\t; PC-98: no IBM extended memory service')
        start = text.index('\n\tcmp\t[sys_model_byte], 0FDh')
        end = text.index('\nSet_Sysinit_Base:', start)
        text = text[:start] + '\n; PC-98: no IBM global interrupt rearm ports.\n' + text[end:]
    if name == 'SYSCONF.ASM':
        old = '\tJE\tDo_TryK\n\tjmp\tTRYS\n'
        if text.count(old) != 1:
            raise ValueError('STACKS parser source changed')
        text = text.replace(old, old + '\nIF NOT STACKSW\nDo_TryK:\n\tjmp BadOp\nENDIF\n')
    return text.replace('\n', '\r\n').encode('ascii')


def build_port(work, output, dosbox, jwasm):
    src = work/'v4.0/src'
    init = src/'P98INIT'
    init.mkdir(exist_ok=True)
    for path in init.glob('*.LOG'):
        path.unlink()
    hashes = {}
    for name in ('SYSINIT1.ASM', 'SYSCONF.ASM', 'SYSINIT2.ASM', 'SYSIMES.ASM'):
        original = (src/'BIOS'/name).read_bytes()
        patched = patch_init(name, original)
        (init/name).write_bytes(patched)
        hashes[name] = {'source': hashlib.sha256(original).hexdigest(), 'patched': hashlib.sha256(patched).hexdigest()}
    subprocess.run([jwasm, '-DDOS4', '-Fo'+str(init/'P98BIO.OBJ'), str(HERE/'bios.asm')], check=True)
    subprocess.run([jwasm, '-bin', '-Fo'+str(output/'ipl.bin'), str(HERE/'ipl.asm')], check=True)
    subprocess.run([jwasm, '-bin', '-Fo'+str(output/'P98TEST.COM'), str(HERE/'probe4.asm')], check=True)
    commands = ['@echo off', 'call ..\\SETENV.BAT']
    for name in ('SYSINIT1', 'SYSCONF', 'SYSINIT2', 'SYSIMES'):
        commands += [f'masm -Mx -t -I. -I..\\BIOS -I..\\INC -I..\\DOS {name}.ASM,{name}.OBJ; > {name}.LOG', 'if errorlevel 1 goto failed']
    commands += ['link P98BIO+SYSINIT1+SYSCONF+SYSINIT2+SYSIMES,P98BIO,P98BIO/M; > LINK.LOG',
                 'if errorlevel 1 goto failed', 'echo SUCCESS>PORT.OK', 'goto end',
                 ':failed', 'echo FAILED>PORT.ERR', ':end', 'exit', '']
    for name in ('PORT.OK', 'PORT.ERR'):
        (init/name).unlink(missing_ok=True)
    (init/'PORT.BAT').write_bytes('\r\n'.join(commands).encode('ascii'))
    conf = work/'pc98.conf'
    conf.write_text('[sdl]\noutput=surface\n[cpu]\ncycles=max\n[autoexec]\n'
                    f'mount d "{work / "v4.0"}"\nd:\ncd \\src\\P98INIT\ncall PORT.BAT\n')
    with (output/'port-host.log').open('wb') as log:
        subprocess.run([dosbox, '-conf', str(conf), '-exit'], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=300)
    log = b'\r\n'.join(f.read_bytes() for f in sorted(init.glob('*.LOG')))
    (output/'port.log').write_bytes(log)
    if not (init/'PORT.OK').exists() or re.search(rb'fatal error|error [A-Z]+\d+|unresolved', log, re.I):
        raise RuntimeError('PC-98 SYSINIT build failed; see '+str(output/'port.log'))
    (output/'bios.exe').write_bytes((init/'P98BIO.EXE').read_bytes())
    (output/'bios.map').write_bytes((init/'P98BIO.MAP').read_bytes())
    (output/'MSDOS.SYS').write_bytes(core.dos_path(src, 'DOS/MSDOS.SYS').read_bytes())
    (output/'COMMAND.COM').write_bytes(core.dos_path(src, 'CMD/COMMAND/COMMAND.COM').read_bytes())
    (output/'LICENSE.TXT').write_bytes((ROOT/'LICENSE').read_bytes())
    (output/'source-manifest.json').write_text(json.dumps({'base': core.BASE, 'sysinit': hashes}, indent=2)+'\n')


def package(output, extra=()):
    filer = ROOT/'tools/filer'
    subprocess.run(['make', '-C', str(filer), 'all'], check=True)
    bios = flatten_exe((output/'bios.exe').read_bytes())
    kernel = (output/'MSDOS.SYS').read_bytes()
    command = (output/'COMMAND.COM').read_bytes()
    autoexec = b'@ECHO OFF\r\nECHO MS-DOS 4.0 PC-98 / WebNP2\r\n'
    config = ('CONFIG.SYS', b'FILES=20\r\nBUFFERS=8\r\nLASTDRIVE=A\r\n')
    probe = ('P98TEST.COM', (output/'P98TEST.COM').read_bytes())
    apps = [(name, (filer/'build'/name).read_bytes()) for name in ('FD98.COM', 'FILER.TXT', 'FILERLIC.TXT')]
    disk, files = make_image((output/'ipl.bin').read_bytes(), bios, [config, probe, *apps, *extra],
                            kernel=kernel, command=command, autoexec=autoexec)
    (output/'IO.SYS').write_bytes(bios)
    path = output/'msdos4-pc98.xdf'
    path.write_bytes(disk)
    (output/'manifest.json').write_text(json.dumps({'image': path.name, 'dos_version': '4.00',
        'sha256': hashlib.sha256(disk).hexdigest(), 'files': files}, indent=2)+'\n')
    print(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--work', type=Path, default=ROOT/'.work/pc98-dos4')
    p.add_argument('--build', type=Path, default=HERE/'build/dos4')
    p.add_argument('--dosbox', required=True)
    p.add_argument('--jwasm', default='jwasm')
    p.add_argument('--add', type=Path, action='append', default=[])
    args = p.parse_args()
    args.work = args.work.resolve()
    args.build = args.build.resolve()
    args.build.mkdir(parents=True, exist_ok=True)
    if not args.work.exists():
        core.prepare(args.work)
    checkpoint = json.loads((args.work/'checkpoint.json').read_text())
    if checkpoint.get('base') != core.BASE:
        raise ValueError('unexpected source base')
    if checkpoint.get('stage') != 'built':
        core.build(args.work, Path(args.dosbox))
    build_port(args.work, args.build, args.dosbox, args.jwasm)
    package(args.build, [(f.name, f.read_bytes()) for f in args.add])


if __name__ == '__main__':
    main()
