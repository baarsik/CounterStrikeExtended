#!/usr/bin/env python3
"""Freeze actual include/source closure for a controlled reference executable and current whole TU."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

from run_mac import SHARED

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
SOURCES = ['HLTV/Core/NetSocket.cpp', 'HLTV/Core/Network.cpp', 'HLTV/Core/private/common/BaseSystemModule.cpp',
           'HLTV/common/BitBuffer.cpp', 'HLTV/common/NetAddress.cpp', 'HLTV/common/byteorder.cpp',
           'HLTV/common/common.cpp', 'common/ObjectList.cpp', 'common/TokenLine.cpp']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    sdk = ROOT / 'GoldSrc-SDK'
    index = {str(Path(path)).casefold(): Path(path) for path in
             subprocess.check_output(['rg', '--files', str(sdk)], text=True).splitlines()}
    includes = [sdk / name for name in ['HLTV/Core/private', 'HLTV/Core/private/public',
                'HLTV/Core/private/common', 'HLTV/common', 'launcher/public', '.']]
    queue = [sdk / name for name in SOURCES]
    seen = set()
    while queue:
        source = queue.pop().resolve()
        if source in seen:
            continue
        seen.add(source)
        data = source.read_bytes()
        target = options.output / source.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        for name in re.findall(rb'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]', data, re.M):
            for candidate in [source.parent / name.decode(), *[path / name.decode() for path in includes]]:
                actual = index.get(str(candidate.resolve()).casefold())
                if actual:
                    queue.append(actual)
                    break
    (options.output / 'fixture-input.json').write_text(json.dumps({'sources': SOURCES, 'shared_cases': SHARED}, indent=2))
    shutil.copy2(options.original, options.output / 'NetSocket-original.cpp')
    for name in ['syscalls.h', 'fixture_system.h', 'fixture_channel.h', 'socket_tests.cpp']:
        shutil.copy2(HERE / name, options.output / name)
    rows = [{'path': str(path.relative_to(options.output)), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
            for path in sorted(options.output.rglob('*')) if path.is_file() and path.name != 'source-manifest.json']
    (options.output / 'source-manifest.json').write_text(json.dumps(rows, indent=2))
    print('staged', len(seen), 'real SDK files')


if __name__ == '__main__':
    main()
