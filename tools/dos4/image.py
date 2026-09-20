"""Deterministic 1.44 MB FAT12 image using the freshly built DOS boot sector."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

SECTOR = 512
ROOT_START = 19 * SECTOR
DATA_START = 33 * SECTOR
IMAGE_SIZE = 2880 * SECTOR


def short_name(name):
    if not re.fullmatch(r"[A-Z0-9_]{1,8}(\.[A-Z0-9_]{1,3})?", name):
        raise ValueError(f"Invalid DOS filename: {name}")
    stem, _, ext = name.partition(".")
    return (stem.ljust(8) + ext.ljust(3)).encode("ascii")


def make_image(boot, files):
    if len(boot) != 512 or boot[510:] != b"\x55\xaa":
        raise ValueError("Boot sector must be 512 bytes with 55AA signature")
    if [name for name, _ in files[:2]] != ["IO.SYS", "MSDOS.SYS"]:
        raise ValueError("System files must be the first two root entries")
    if len(files) > 224 or len({name for name, _ in files}) != len(files):
        raise ValueError("Too many files or duplicate names")
    image = bytearray(IMAGE_SIZE)
    image[:512] = boot
    struct.pack_into("<HBHBHHBHHHII", image, 11,
                     512, 1, 1, 2, 224, 2880, 0xF0, 9, 18, 2, 0, 0)
    struct.pack_into("<BBBI", image, 36, 0, 0, 0x29, 0x19880927)
    image[43:62] = b"MSDOS4BOOT " + b"FAT12   "
    fat = bytearray(9 * SECTOR)
    fat[:3] = b"\xf0\xff\xff"
    cluster = 2
    for index, (name, data) in enumerate(files):
        encoded = short_name(name)
        count = (len(data) + 511) // 512
        if cluster + count > 2849:
            raise ValueError("Files exceed 1.44 MB capacity")
        entry = ROOT_START + index * 32
        image[entry:entry + 11] = encoded
        image[entry + 11] = 7 if index < 2 else 0x20
        struct.pack_into("<HHHI", image, entry + 22, 0, 0x113B,
                         cluster if count else 0, len(data))
        offset = DATA_START + (cluster - 2) * SECTOR
        image[offset:offset + len(data)] = data
        for c in range(cluster, cluster + count):
            value = 0xFFF if c == cluster + count - 1 else c + 1
            pos = c + c // 2
            pair = int.from_bytes(fat[pos:pos + 2], "little")
            pair = (pair & 0x000F) | (value << 4) if c & 1 else (pair & 0xF000) | value
            fat[pos:pos + 2] = pair.to_bytes(2, "little")
        cluster += count
    image[512:5120] = fat
    image[5120:9728] = fat
    return bytes(image)


def read_files(image):
    """Read the fixed FAT12 format, following chains (also after DOS writes)."""
    if len(image) != IMAGE_SIZE or image[512:5120] != image[5120:9728]:
        raise ValueError("Invalid image size or mismatched FAT copies")
    result = {}
    for offset in range(ROOT_START, DATA_START, 32):
        entry = image[offset:offset + 32]
        if entry[0] == 0:
            break
        if entry[0] == 0xE5 or entry[11] & 0x18:
            continue
        name = entry[:8].decode("ascii").rstrip()
        ext = entry[8:11].decode("ascii").rstrip()
        if ext:
            name += "." + ext
        cluster, size = struct.unpack_from("<HI", entry, 26)
        data = bytearray()
        seen = set()
        while cluster < 0xFF8 and size:
            if cluster < 2 or cluster > 2848 or cluster in seen:
                raise ValueError("Invalid FAT chain")
            seen.add(cluster)
            start = DATA_START + (cluster - 2) * 512
            data.extend(image[start:start + 512])
            pos = 512 + cluster + cluster // 2
            pair = int.from_bytes(image[pos:pos + 2], "little")
            cluster = (pair >> 4) if cluster & 1 else (pair & 0xFFF)
        if len(data) < size:
            raise ValueError("Truncated FAT chain")
        result[name] = bytes(data[:size])
    return result


def create(work):
    if json.loads((work / "checkpoint.json").read_text())["stage"] != "built":
        raise ValueError("A successful build is required")
    src = work / "v4.0/src"
    files = [(name, (src / path).read_bytes()) for name, path in [
        ("IO.SYS", "BIOS/IO.SYS"), ("MSDOS.SYS", "DOS/MSDOS.SYS"),
        ("COMMAND.COM", "CMD/COMMAND/COMMAND.COM")]]
    # Include the complete CPY.BAT collection, preserving system-file ordering.
    for path in re.findall(r"^copy\s+\.\\(\S+)\s+%1", (src / "CPY.BAT").read_text(), re.M | re.I):
        path = Path(path.replace("\\", "/"))
        name = path.name.upper()
        if name not in {n for n, _ in files}:
            files.append((name, (src / path).read_bytes()))
    files.append(("AUTOEXEC.BAT", b"@echo off\r\nver > VER.TXT\r\ndir > DIR.TXT\r\necho DOS4-WRITE-READ> PROBE.TXT\r\ntype PROBE.TXT > READ.TXT\r\ndir\r\nver\r\ntype READ.TXT\r\necho COMPLETE> DONE.TXT\r\n"))
    # MSBOOT.ASM uses ORG 7C00h; EXE2BIN preserves that zero-filled prefix.
    binary = (src / "BOOT/MSBOOT.BIN").read_bytes()
    if len(binary) != 0x7E00 or any(binary[:0x7C00]):
        raise ValueError("Unexpected EXE2BIN boot output layout")
    image = make_image(binary[0x7C00:], files)
    if read_files(image) != dict(files):
        raise ValueError("FAT image round-trip validation failed")
    output = work / "msdos4-boot.img"
    output.write_bytes(image)
    manifest = {name: {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()} for name, data in files}
    manifest["image"] = {"sha256": hashlib.sha256(image).hexdigest()}
    (work / "image-manifest.json").write_text(json.dumps(manifest, indent=2))
    print(f"Created {output}: {len(files)} files, SHA256 {manifest['image']['sha256']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, default=Path(__file__).resolve().parents[2] / ".work/build")
    create(parser.parse_args().work.resolve())
