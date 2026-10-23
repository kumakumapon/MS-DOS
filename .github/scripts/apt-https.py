#!/usr/bin/env python3
"""Enable HTTPS in APT sources and file-backed mirror lists before apt update."""
from pathlib import Path
import sys


def configure(root: Path) -> None:
    sources = {root / "sources.list"}
    sources.update((root / "sources.list.d").glob("*.list"))
    sources.update((root / "sources.list.d").glob("*.sources"))
    # Ubuntu runners can use URIs: mirror+file:/etc/apt/mirrors/ubuntu.list.
    # Changing only the .sources URI leaves its actual HTTP mirrors untouched.
    sources.update((root / "mirrors").rglob("*"))
    for source in sorted(sources):
        if not source.is_file():
            continue
        original = source.read_bytes()
        updated = original.replace(b"http://", b"https://")
        if updated != original:
            source.write_bytes(updated)
            print(f"Enabled HTTPS: {source}")


if __name__ == "__main__":
    configure(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/etc/apt"))
