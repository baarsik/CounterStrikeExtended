"""Compare original CRT controls and retain mapped native-libc disagreements."""
import argparse
import errno
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--windows', type=Path, required=True)
p.add_argument('--native', type=Path)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
def load(path):
    result = {}
    for line in path.read_text().splitlines():
        name, api, profile, capacity, seed, returned, error, invalid, count, cells = line.split('\t')
        key = (name, api, profile, int(capacity), int(seed))
        assert key not in result
        result[key] = (int(returned), int(error), int(invalid), int(count), cells)
    return result
cases = tuple(x + '-' + c for x in ('x86', 'x64', 'arm64') for c in ('Release', 'Debug'))
baseline = load(a.windows / cases[0] / 'results.tsv')
summary = {'windows': {}, 'native_mismatches': []}
for case in cases:
    path = a.windows / case / 'results.tsv'
    rows = load(path)
    assert rows == baseline, case
    summary['windows'][case] = {'rows': len(rows), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                'equal_to_x86_release': True}
# On startup C, both retained legacy APIs produce exactly the same bytes/status.
for key, row in baseline.items():
    if key[1] == 'legacy-va' and key[2] == 'linked-C':
        direct = (key[0], 'legacy-direct', key[2], key[3], key[4])
        assert baseline[direct] == row
if a.native:
    native = load(a.native)
    def semantic_error(value, seed, windows):
        if value == seed:
            return 'preserved'
        if value == (42 if windows else errno.EILSEQ):
            return 'EILSEQ'
        if value == errno.EINVAL:
            return 'EINVAL'
        return str(value)
    for key, row in native.items():
        expected_key = (key[0], 'legacy-va', key[2], key[3], key[4])
        golden = baseline[expected_key]
        actual = (row[0], semantic_error(row[1], key[4], False), *row[2:])
        expected = (golden[0], semantic_error(golden[1], key[4], True), *golden[2:])
        if actual != expected:
            summary['native_mismatches'].append({'case': key, 'legacy_UCRT': expected, 'mapped_native': actual})
    summary['native_rows'] = len(native)
a.output.write_text(json.dumps(summary, indent=2) + '\n')
print('six CPU/configuration tables agree:', len(baseline), 'rows each')
print('mapped libc observable disagreements:', len(summary['native_mismatches']))
raise SystemExit(int(bool(summary['native_mismatches'])))
