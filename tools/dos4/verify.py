"""Boot a disposable image in QEMU and verify files written by MS-DOS itself.

Run in WSL Ubuntu, or natively where qemu-system-i386 is installed.
"""
import argparse
import json
import shutil
import socket
import subprocess
import tempfile
import time
from pathlib import Path

from image import read_files


def verify(work, qemu, timeout):
    original = work / "msdos4-boot.img"
    tested = work / "qemu-test.img"
    shutil.copyfile(original, tested)
    version = subprocess.check_output([qemu, "--version"], text=True).splitlines()[0]
    # Use a Unix QMP socket in WSL's /tmp, not on a Windows-mounted filesystem.
    with tempfile.TemporaryDirectory(prefix="dos4-qmp-") as temp:
        endpoint = str(Path(temp) / "qmp.sock")
        args = [qemu, "-machine", "pc", "-accel", "tcg", "-m", "16",
                "-drive", f"file={tested},format=raw,if=floppy,index=0,cache=directsync",
                "-boot", "order=a", "-nic", "none", "-display", "none",
                "-monitor", "none", "-serial", "none", "-no-reboot",
                "-qmp", f"unix:{endpoint},server=on,wait=off"]
        with (work / "qemu.log").open("wb") as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
            qmp = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            stream = None
            try:
                deadline = time.monotonic() + timeout
                while not Path(endpoint).exists():
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("QEMU failed to start; inspect qemu.log")
                    time.sleep(0.1)
                qmp.settimeout(5)
                qmp.connect(endpoint)
                stream = qmp.makefile("rwb")
                stream.readline()

                def command(execute, arguments=None):
                    request = {"execute": execute}
                    if arguments is not None:
                        request["arguments"] = arguments
                    stream.write(json.dumps(request).encode() + b"\n")
                    stream.flush()
                    while True:
                        response = json.loads(stream.readline())
                        if "error" in response:
                            raise RuntimeError(response)
                        if "return" in response:
                            return response["return"]

                command("qmp_capabilities")
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        raise RuntimeError("QEMU exited before DOS completed")
                    try:
                        files = read_files(tested.read_bytes())
                    except ValueError:
                        files = {}
                    if files.get("DONE.TXT", b"").strip() == b"COMPLETE":
                        break
                    time.sleep(0.5)
                else:
                    command("screendump", {"filename": str(work / "qemu-timeout.ppm")})
                    raise TimeoutError("MS-DOS boot probe timed out")
                command("stop")
                command("screendump", {"filename": str(work / "qemu-screen.ppm")})
                command("quit")
                process.wait(timeout=10)
            finally:
                if stream:
                    stream.close()
                qmp.close()
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=10)
    files = read_files(tested.read_bytes())
    if b"4.00" not in files.get("VER.TXT", b""):
        raise AssertionError(f"Unexpected VER output: {files.get('VER.TXT')}")
    if b"COMMAND" not in files.get("DIR.TXT", b""):
        raise AssertionError("DIR did not list COMMAND.COM")
    if files.get("PROBE.TXT", b"").strip() != b"DOS4-WRITE-READ" or files.get("READ.TXT") != files.get("PROBE.TXT"):
        raise AssertionError("DOS file write/read probe failed")
    evidence = {"qemu": version, "status": "passed", "commands": {name: files[name].decode("cp437")
                for name in ["VER.TXT", "DIR.TXT", "PROBE.TXT", "READ.TXT", "DONE.TXT"]}}
    (work / "verification.json").write_text(json.dumps(evidence, indent=2))
    print(json.dumps(evidence, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=Path(__file__).resolve().parents[2] / ".work/build")
    parser.add_argument("--qemu", default="qemu-system-i386")
    parser.add_argument("--timeout", type=int, default=120)
    args = parser.parse_args()
    verify(args.work.resolve(), args.qemu, args.timeout)
