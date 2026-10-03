"""Fetch/build pinned open-source tools into a new local directory."""
import argparse
import json
import subprocess
from pathlib import Path

TOOLS = {
    'JWasm': ('https://github.com/JWasm/JWasm.git', 'a5c4ea03cc0545a15d81a354251b5f534bef7a1b'),
    'JWlink': ('https://github.com/Baron-von-Riedesel/JWlink.git', 'cb7df1288619f7d3bf0290566bbe0b643fc05fd1'),
}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--output', type=Path, default=Path('.work/pc98-tools'))
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    for name, (url, commit) in TOOLS.items():
        dest = args.output / name
        subprocess.run(['git', 'init', str(dest)], check=True)
        subprocess.run(['git', '-C', str(dest), 'remote', 'add', 'origin', url], check=True)
        subprocess.run(['git', '-C', str(dest), 'fetch', '--depth', '1', 'origin', commit], check=True)
        subprocess.run(['git', '-C', str(dest), 'checkout', '--detach', commit], check=True)
        subprocess.run(['make', '-C', str(dest), '-f', 'GccUnix.mak', '-j2'], check=True)
    (args.output / 'versions.json').write_text(json.dumps(TOOLS, indent=2) + '\n')

if __name__ == '__main__':
    main()
