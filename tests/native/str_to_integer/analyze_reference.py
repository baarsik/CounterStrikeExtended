"""Compare actual CPU/CRT references; optionally retain a nonzero LP64 libc RED."""
import argparse
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--windows', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--native', type=Path)
a = p.parse_args()
profiles = ('linked-C', 'locale-C', 'locale-UTF8', 'locale-1250')
configs = tuple(x + '-' + c for x in ('x86', 'x64', 'arm64') for c in ('Release', 'Debug'))
def load(path):
    rows = {}
    for line in path.read_text().splitlines():
        name, kind, seed, has_end, bits, end, error, full = line.split('\t')
        key = (name, kind, int(seed), int(has_end))
        assert key not in rows
        rows[key] = (int(bits, 16), int(end), int(error), int(full, 16))
    return rows
baseline = {profile: load(a.windows / configs[0] / (profile + '.tsv')) for profile in profiles}
summary = {'reference': {}, 'profile_spaces': {}, 'native_mismatches': []}
for profile, rows in baseline.items():
    for config in configs:
        path = a.windows / config / (profile + '.tsv')
        assert load(path) == rows, (profile, config)
        summary['reference'][config + '/' + profile] = {
            'rows': len(rows), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'equal_to_x86_release': True}
    spaces = [byte for byte in range(256)
              if rows[(f'prefix-{byte:02x}-positive', 's', 12345, 1)][:3] == (7, 3, 12345)]
    expected = list(range(9, 14)) + [32] + ([160] if profile == 'locale-1250' else [])
    assert spaces == expected, (profile, spaces)
    summary['profile_spaces'][profile] = spaces
    for (name, kind, seed, has_end), row in rows.items():
        if has_end:
            other = rows[(name, kind, seed, 0)]
            assert (other[0], other[2], other[3]) == (row[0], row[2], row[3]) and other[1] == -1
        if seed == 0:
            other = rows[(name, kind, 12345, has_end)]
            assert other[0:2] == row[0:2] and other[3] == row[3]
            assert other[2] == (34 if row[2] == 34 else 12345), (profile, name, row, other)
    for name in ('empty', 'space', 'whitespace-only', 'plus', 'minus', 'space-plus', 'space-minus',
                 'double-plus', 'double-minus', 'mixed-sign', 'sign-space', 'huge-whitespace-only'):
        for kind in ('s', 'u'):
            assert rows[(name, kind, 12345, 1)][:3] == (0, 0, 12345)
assert baseline['linked-C'] == baseline['locale-C'] == baseline['locale-UTF8']
if a.native:
    native = load(a.native)
    expected = baseline['linked-C']
    assert native.keys() == expected.keys()
    for key, golden in expected.items():
        actual = native[key]
        # Returned low32, byte endpoint and errno are the consumer contract.
        if actual[:3] != golden[:3]:
            summary['native_mismatches'].append({'case': key, 'ucrt': golden, 'native_libc': actual})
a.output.write_text(json.dumps(summary, indent=2) + '\n')
print('24 actual Windows profile tables agree across CPUs/configs; exhaustive byte classification checked')
print('LP64 consumer-observable mismatches', len(summary['native_mismatches']))
raise SystemExit(int(bool(summary['native_mismatches'])))
