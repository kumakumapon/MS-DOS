#!/usr/bin/env python3
"""Enable HTTPS in APT sources and file-backed mirror lists before apt update."""
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit
from urllib.request import urlopen


UBUNTU_MIRRORS = (
    "https://archive.ubuntu.com/ubuntu/",
    "https://mirror.math.princeton.edu/pub/ubuntu/",
    "https://mirrors.mit.edu/ubuntu/",
)


def available_mirror(suite: str, open_url=urlopen) -> str:
    if not re.fullmatch(r"[a-z]+", suite):
        raise ValueError("Expected an Ubuntu suite name")
    for mirror in UBUNTU_MIRRORS:
        try:
            with open_url(mirror + "dists/" + suite + "/InRelease", timeout=5) as response:
                if response.status == 200 and response.read(64).startswith(
                    b"-----BEGIN PGP SIGNED MESSAGE-----"
                ):
                    return mirror
        except (OSError, ValueError):
            continue
    raise RuntimeError("No Ubuntu HTTPS mirror is reachable; refusing stale indexes")


def configure(root: Path, ubuntu_mirror: str = UBUNTU_MIRRORS[0]) -> None:
    if ubuntu_mirror not in UBUNTU_MIRRORS:
        raise ValueError("Expected a configured Ubuntu HTTPS mirror")
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
        # Keep suites, components and Signed-By intact. APT still authenticates
        # InRelease and package hashes with the runner's Ubuntu archive keys.
        for host in ("archive.ubuntu.com", "azure.archive.ubuntu.com", "security.ubuntu.com"):
            updated = updated.replace(
                ("https://" + host + "/ubuntu/").encode(), ubuntu_mirror.encode()
            ).replace(
                ("https://" + host + "/ubuntu").encode(), ubuntu_mirror.rstrip("/").encode()
            )
        if updated != original:
            source.write_bytes(updated)
            print(f"Enabled HTTPS: {source}")


if __name__ == "__main__":
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/etc/apt")
    mirror = UBUNTU_MIRRORS[0]
    if root == Path("/etc/apt"):
        release = dict(line.split("=", 1) for line in Path("/etc/os-release").read_text().splitlines() if "=" in line)
        mirror = available_mirror(release["VERSION_CODENAME"].strip('"'))
        print("Selected Ubuntu HTTPS mirror:", mirror)
    configure(root, mirror)
