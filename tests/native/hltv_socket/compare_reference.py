#!/usr/bin/env python3
"""Compare all controlled common-case bytes/traces with actual frozen Win32 Release behavior."""
import argparse
import hashlib
import json
from pathlib import Path

from run_mac import SHARED


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--packet', type=Path, required=True)
    options = parser.parse_args()
    art = options.packet
    golden = {}
    for index, case in enumerate(SHARED):
        data = (art / 'windows-results/x86/release' / (str(index) + '.log')).read_bytes().replace(b'\r\n', b'\n')
        assert data.endswith(('PASS ' + case + '\n').encode())
        golden[case] = {'stdout': data.decode(), 'sha256': hashlib.sha256(data).hexdigest()}
    (art / 'golden-win32.json').write_text(json.dumps(golden, indent=2))
    comparisons = []
    for arch in ['x86', 'x64', 'arm64']:
        for config in ['release', 'debug']:
            if arch == 'x86' and config == 'release':
                continue
            for index, case in enumerate(SHARED):
                data = (art / 'windows-results' / arch / config / (str(index) + '.log')).read_bytes().replace(b'\r\n', b'\n')
                assert hashlib.sha256(data).hexdigest() == golden[case]['sha256'], (arch, config, case)
                comparisons.append({'profile': 'windows-' + arch + '-' + config, 'case': case, 'identical': True})
    for folder in ['before-current-final', 'current-final']:
        receipt = json.loads((art / folder / 'receipt.json').read_text())
        for profile in sorted(set(row['profile'] for row in receipt['cases'])):
            for index, case in enumerate(SHARED):
                data = (art / folder / profile / (str(index) + '.log')).read_bytes()
                assert hashlib.sha256(data).hexdigest() == golden[case]['sha256'], (folder, profile, case)
                comparisons.append({'profile': folder + '-' + profile, 'case': case, 'identical': True})
    (art / 'reference-comparison.json').write_text(json.dumps({'comparisons': comparisons,
        'normalization': 'CRLF to LF only; result/payload bytes/RNG counts/warnings/stats/call order otherwise exact',
        'reference': 'actual frozen NetSocket.cpp Windows-selected branch, MSVC x86 Release; typed controlled OS/RNG/System boundaries'}, indent=2))
    print('PASS', len(comparisons), 'case transcript comparisons with frozen Win32 Release reference')


if __name__ == '__main__':
    main()
