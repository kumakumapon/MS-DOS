#!/usr/bin/env python3
"""Enable HTTPS in APT sources and file-backed mirror lists before apt update."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit


def configure(root: Path) -> None:
    sources = {root / "sources.list"}
    sources.update((root / "sources.list.d").glob("*.list"))
    sources.update((root / "sources.list.d").glob("*.sources"))
    # Ubuntu runners can use URIs: mirror+file:/etc/apt/mirrors/ubuntu.list.
    # Changing only the .sources URI leaves its actual HTTP mirrors untouched.
    sources.update((root / "mirrors").rglob("*"))
    pending = sorted(sources)
    visited = set()
    while pending:
        source = pending.pop(0)
        if source in visited:
            continue
        visited.add(source)
        if not source.is_file():
            continue
        original = source.read_bytes()
        # Current GitHub runners use /etc/apt/apt-mirrors.txt, rather than
        # /etc/apt/mirrors/*.list. Follow the URI instead of guessing its name.
        for token in re.findall(rb"mirror\+file:[^\s#]+", original):
            uri = urlsplit(token.decode("utf-8"))
            if uri.netloc not in ("", "localhost") or not uri.path.startswith("/"):
                raise ValueError(f"Expected a local absolute mirror file: {source}")
            mirror = Path(unquote(uri.path))
            if root != Path("/etc/apt") and mirror.is_relative_to("/etc/apt"):
                mirror = root / mirror.relative_to("/etc/apt")
            pending.append(mirror)
        updated = original.replace(b"http://", b"https://")
        # The Azure archive's HTTPS endpoint fails on some runners. Use the
        # official Ubuntu HTTPS archive for the same suites and signing keys.
        updated = updated.replace(
            b"https://azure.archive.ubuntu.com/", b"https://archive.ubuntu.com/"
        )
        if updated != original:
            source.write_bytes(updated)
            print(f"Enabled HTTPS: {source}")


if __name__ == "__main__":
    configure(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/etc/apt"))
