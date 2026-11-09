#!/usr/bin/env python3
"""Compare actual Windows-selected lexical tokens, independently of native runtime components."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|[^\s]')


def tokens(data, target):
    source = re.sub(r'^\s*#\s*include[^\n]*', '', data.decode(), flags=re.M)
    result = subprocess.run(['/usr/bin/clang++', '-E', '-P', '-x', 'c++', '--target=' + target,
                             '-D_WIN32', '-D_MSC_VER=1951', '-'], input=source.encode(), capture_output=True, check=True)
    return TOKEN.findall(result.stdout.decode())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    source = ROOT / 'GoldSrc-SDK/HLTV/Core/NetSocket.cpp'
    before = options.before.read_bytes()
    after = source.read_bytes()
    rows = []
    for target in ['i686-pc-windows-msvc', 'x86_64-pc-windows-msvc', 'aarch64-pc-windows-msvc']:
        equal = tokens(before, target) == tokens(after, target)
        rows.append({'target': target, 'selected_tokens_identical': equal})
        assert equal, target + ' Windows selected tokens changed'
    options.output.write_text(json.dumps({'before_sha256': hashlib.sha256(before).hexdigest(),
        'after_sha256': hashlib.sha256(after).hexdigest(), 'targets': rows}, indent=2))
    print('PASS Windows selected tokens for all three CPU targets')


if __name__ == '__main__':
    main()
