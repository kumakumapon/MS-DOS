"""Boot the source-built DOS 4.0 IBM image and drive FD.COM through QEMU keys."""
import argparse
import importlib.util
import json
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('dos_image', ROOT/'tools/dos4/image.py')
image = importlib.util.module_from_spec(spec)
spec.loader.exec_module(image)
build_spec = importlib.util.spec_from_file_location('dos_build', ROOT/'tools/dos4/build.py')
build = importlib.util.module_from_spec(build_spec)
build_spec.loader.exec_module(build)


def verify(work, output, qemu):
    output.mkdir(parents=True, exist_ok=True)
    if json.loads((work/'checkpoint.json').read_text())['stage'] != 'built':
        raise ValueError('A successful DOS 4.0 build is required')
    src = work/'v4.0/src'
    files = {name: build.dos_path(src, source).read_bytes() for name, source in [
        ('IO.SYS', 'BIOS/IO.SYS'), ('MSDOS.SYS', 'DOS/MSDOS.SYS'),
        ('COMMAND.COM', 'CMD/COMMAND/COMMAND.COM')]}
    boot = build.dos_path(src, 'BOOT/MSBOOT.BIN').read_bytes()
    if len(boot) != 0x7e00 or any(boot[:0x7c00]):
        raise ValueError('Unexpected source-built IBM boot sector')
    files['AUTOEXEC.BAT'] = b'@ECHO OFF\r\nECHO FILER-GUEST-DATA>SOURCE.TXT\r\nVER\r\n'
    for name in ('FD.COM', 'FILER.TXT', 'FILERLIC.TXT'):
        files[name] = (ROOT/'tools/filer/build'/name).read_bytes()
    files['P98TEST.COM'] = (ROOT/'ports/pc98/build/dos4/P98TEST.COM').read_bytes()
    disk = output/'msdos4-filer.img'
    disk.write_bytes(image.make_image(boot[0x7c00:], list(files.items())))
    with tempfile.TemporaryDirectory(prefix='filer-qmp-') as directory:
        endpoint = Path(directory)/'qmp.sock'
        video = Path(directory)/'vram.bin'
        args = [qemu, '-machine', 'pc', '-accel', 'tcg', '-m', '16',
                '-drive', f'file={disk},format=raw,if=floppy,index=0,cache=directsync',
                '-boot', 'order=a', '-nic', 'none', '-display', 'none',
                '-monitor', 'none', '-serial', 'none', '-no-reboot',
                '-qmp', f'unix:{endpoint},server=on,wait=off']
        with (output/'qemu.log').open('wb') as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
            stream = None
            connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                deadline = time.monotonic() + 15
                while not endpoint.exists():
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError('QEMU failed to start')
                    time.sleep(.1)
                connection.settimeout(5)
                while True:
                    try:
                        connection.connect(str(endpoint))
                        break
                    except ConnectionRefusedError:
                        if process.poll() is not None or time.monotonic() > deadline:
                            raise RuntimeError('QEMU QMP socket did not become ready')
                        time.sleep(.1)
                stream = connection.makefile('rwb')
                stream.readline()

                def command(name, arguments=None):
                    request = {'execute': name}
                    if arguments is not None:
                        request['arguments'] = arguments
                    stream.write(json.dumps(request).encode() + b'\n')
                    stream.flush()
                    while True:
                        response = json.loads(stream.readline())
                        if 'error' in response:
                            raise RuntimeError(response)
                        if 'return' in response:
                            return response['return']

                command('qmp_capabilities')

                def screen():
                    command('pmemsave', {'val': 0xb8000, 'size': 4000, 'filename': str(video)})
                    cells = video.read_bytes()[::2].decode('cp437')
                    return '\n'.join(cells[i:i+80].rstrip() for i in range(0, 2000, 80))

                def wait(needle):
                    deadline = time.monotonic() + 30
                    while time.monotonic() < deadline:
                        text = screen()
                        if needle in text:
                            return text
                        time.sleep(.1)
                    command('screendump', {'filename': str(output/'timeout.ppm')})
                    raise AssertionError(f'Missing {needle!r}:\n{text}')

                def key(value):
                    command('send-key', {'keys': [{'type': 'qcode', 'data': part}
                                                   for part in value.split('+')], 'hold-time': 35})
                    time.sleep(.09)

                def text(value):
                    for char in value:
                        if char.isalpha():
                            key(('shift+' if char.isupper() else '') + char.lower())
                        else:
                            key({'\r': 'ret', '.': 'dot', '-': 'minus', '\\': 'backslash',
                                 ':': 'shift+semicolon', ' ': 'spc'}.get(char, char))

                def select(name):
                    key('home')
                    for _ in range(256):
                        if screen().split('\n')[22].endswith('Selected: ' + name):
                            return
                        key('down')
                    raise AssertionError('Cannot select ' + name)

                def prompt(label, value):
                    wait(label); key('ctrl+u'); text(value + '\r')

                wait('MS-DOS Version 4.00')
                text('FD\r'); wait('FD Filer 1.0')
                key('tab'); wait('Active: RIGHT'); key('tab'); wait('Active: LEFT')
                select('SOURCE.TXT')
                key('f3'); wait('FD Filer - Viewer'); wait('FILER-GUEST-DATA'); key('esc')
                key('f5'); prompt('Copy to:', 'COPY.TXT'); wait('Copied')
                key('f5'); prompt('Copy to:', 'COPY.TXT'); wait('Destination exists')
                select('COPY.TXT'); key('f2'); prompt('Rename to:', 'RENAME.TXT'); wait('Renamed')
                select('RENAME.TXT')
                key('f6'); prompt('Move to:', 'MOVED.TXT'); wait('Moved')
                key('f7'); prompt('Create directory:', 'WORK'); wait('Directory created')
                select('WORK'); key('ret'); wait('A:\\WORK'); key('backspace')
                select('WORK'); key('f8'); wait('Delete selected'); key('n'); wait('Cancelled')
                key('f8'); wait('Delete selected'); key('y'); wait('Deleted')
                select('P98TEST.COM'); key('f9')
                wait('DOS4 EXEC OK'); wait('Program returned. Press any key')
                key('spc'); wait('Program returned (exit 0)')
                command('screendump', {'filename': str(output/'filer.ppm')})
                key('f10'); wait('FD Filer exited.')
                text('ECHO IBM-FILER-RETURN-OK\r'); wait('IBM-FILER-RETURN-OK')
                command('stop')
                command('quit'); process.wait(timeout=10)
            finally:
                if stream:
                    stream.close()
                connection.close()
                if process.poll() is None:
                    process.terminate(); process.wait(timeout=10)
    result = image.read_files(disk.read_bytes())
    assert result['SOURCE.TXT'] == b'FILER-GUEST-DATA\r\n'
    assert result['MOVED.TXT'] == result['SOURCE.TXT']
    assert result['FD.COM'] == files['FD.COM']
    assert 'COPY.TXT' not in result and 'RENAME.TXT' not in result
    evidence = {
        'qemu': subprocess.check_output([qemu, '--version'], text=True).splitlines()[0],
        'status': 'passed', 'dos_version': '4.00',
        'results': ['IBM BIOS text/key UI', 'two panes and text viewer',
                    'copy/no overwrite, rename, move, directory create/navigate/delete/cancel',
                    'child COM EXEC/return and filer exit', 'exact guest FAT12 file bytes'],
    }
    (output/'verification.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, default=ROOT/'.work/pc98-dos4')
    parser.add_argument('--output', type=Path, default=ROOT/'tools/filer/build/ibm-validation')
    parser.add_argument('--qemu', default='qemu-system-i386')
    args = parser.parse_args()
    verify(args.work.resolve(), args.output.resolve(), args.qemu)
