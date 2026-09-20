"""Prepare an isolated DOS source tree and run its bundled toolchain."""
import argparse
import io
import json
import re
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = "2d04cacc5322951f187bb17e017c12920ac8ebe2"


def prepare(work):
    if work.exists():
        raise ValueError(f"Refusing to overwrite existing build: {work}")
    archive = subprocess.check_output(
        ["git", "archive", "--format=zip", BASE, "v4.0"], cwd=ROOT)
    work.mkdir(parents=True)
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        z.extractall(work)
    # git apply outside the parent checkout must not discover its .git.
    subprocess.run(["git", "init", "--quiet", str(work)], check=True)
    subprocess.run(["git", "apply", "--whitespace=nowarn", "--check", str(Path(__file__).with_name("restoration.patch"))], cwd=work, check=True)
    subprocess.run(["git", "apply", "--whitespace=nowarn", str(Path(__file__).with_name("restoration.patch"))], cwd=work, check=True)
    src = work / "v4.0/src"
    text_extensions = {".ASM", ".INC", ".MAC", ".H", ".C", ".BAT", ".SKL", ".MSG", ".INF", ".CTL"}
    special = {"MAKEFILE", "LOCSCR", "ZERO.DAT", "GRAPHICS.PRO", "SELECT.PRT"}
    for p in src.rglob("*"):
        if p.is_file() and (p.suffix.upper() in text_extensions or p.name.upper() in special):
            data = p.read_bytes().replace(b"\r\n", b"\n")
            p.write_bytes(data.replace(b"\n", b"\r\n"))
    (work / "checkpoint.json").write_text(json.dumps({"base": BASE, "stage": "prepared"}, indent=2))


def build(work, dosbox):
    src = work / "v4.0/src"
    if not (work / "checkpoint.json").is_file():
        raise ValueError("Run prepare first")
    (work / "checkpoint.json").write_text(json.dumps({"base": BASE, "stage": "building"}, indent=2))
    for name in ("BUILD.OK", "BUILD.ERR", "BUILD.LOG"):
        (src / name).unlink(missing_ok=True)
    batch = "\r\n".join([
        "@echo off", "call setenv.bat", "nmake > BUILD.LOG",
        "if errorlevel 1 goto failed", "echo SUCCESS>BUILD.OK", "goto end",
        ":failed", "echo FAILED>BUILD.ERR", ":end", "exit", ""])
    (src / "BUILD.BAT").write_bytes(batch.encode("ascii"))
    conf = work / "dosbox.conf"
    conf.write_text("[sdl]\noutput=surface\n[cpu]\ncycles=max\n[dos]\nfile access tries=100\n[autoexec]\n"
                    f'mount d "{work / "v4.0"}"\nd:\ncd \\src\ncall BUILD.BAT\n', encoding="utf-8")
    with (work / "dosbox-host.log").open("wb") as log:
        subprocess.run([str(dosbox), "-conf", str(conf), "-fastlaunch", "-exit"],
                       cwd=work, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=3600)
    log = (src / "BUILD.LOG").read_text(encoding="cp437")
    if not (src / "BUILD.OK").exists() or re.search(r"fatal error|error [A-Z]+\d+|Stop\.", log, re.I):
        raise RuntimeError(f"Build failed; inspect {src / 'BUILD.LOG'}")
    files = re.findall(r"^copy\s+\.\\(\S+)\s+%1", (src / "CPY.BAT").read_text(), re.M | re.I)
    missing = [f for f in files if not (src / Path(f.replace('\\', '/'))).is_file()]
    if missing:
        raise RuntimeError(f"Missing CPY.BAT outputs: {missing}")
    (work / "checkpoint.json").write_text(json.dumps({"base": BASE, "stage": "built", "outputs": len(files)}, indent=2))
    print(f"Build passed: {len(files)} CPY.BAT outputs")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=["prepare", "build"])
    parser.add_argument("--work", type=Path, default=ROOT / ".work/build")
    parser.add_argument("--dosbox", type=Path)
    args = parser.parse_args()
    work = args.work.resolve()
    if args.stage == "prepare":
        prepare(work)
    else:
        if not args.dosbox:
            parser.error("build requires --dosbox")
        build(work, args.dosbox.resolve())
