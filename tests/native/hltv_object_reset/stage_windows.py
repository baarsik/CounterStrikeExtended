#!/usr/bin/env python3
"""Freeze the real SDK literal-include closure for independent headless Windows compilation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

from extract_regions import extract

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    sdk = ROOT / 'src'
    index = {str(Path(name)).casefold(): Path(name) for name in
             subprocess.check_output(['rg', '--files', str(sdk)], text=True).splitlines()}
    includes = [sdk / name for name in ['HLTV/Core/private', 'HLTV/Core/private/public',
                'HLTV/Core/private/common', 'HLTV/common', 'launcher/public', '.']]
    queue = [sdk / name for name in ['HLTV/Core/Network.cpp', 'HLTV/Core/World.cpp',
             'HLTV/common/NetAddress.cpp', 'HLTV/common/BitBuffer.cpp', 'HLTV/common/byteorder.cpp']]
    seen = set()
    options.output.mkdir(parents=True, exist_ok=True)
    while queue:
        path = queue.pop().resolve()
        if path in seen:
            continue
        seen.add(path)
        relative = path.relative_to(ROOT)
        supplied = options.source_root / relative
        data = supplied.read_bytes() if supplied.exists() else path.read_bytes()
        target = options.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        for name in re.findall(rb'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]', data, re.M):
            name = name.decode()
            for candidate in [path.parent / name, *[include / name for include in includes]]:
                actual = index.get(str(candidate.resolve()).casefold())
                if actual:
                    queue.append(actual)
                    break
    extract(options.source_root, options.output / 'generated_regions.h')
    for name in ['allocation_hooks.h', 'virtual_calls.cpp', 'reset_tests.cpp']:
        shutil.copy2(HERE / name, options.output / name)
    rows = [{'path': str(path.relative_to(options.output)),
             'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
            for path in sorted(options.output.rglob('*')) if path.is_file() and path.name != 'source-manifest.json']
    (options.output / 'source-manifest.json').write_text(json.dumps(rows, indent=2))
    print('staged', len(seen), 'actual SDK files')


if __name__ == '__main__':
    main()
