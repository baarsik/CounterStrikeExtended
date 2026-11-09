"""Compare native helper rows to actual linked UCRT; no value or pixel tolerances."""
import argparse
import errno
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--reference', type=Path, required=True)
p.add_argument('--helper', type=Path, required=True)
p.add_argument('--numeric-helper', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
def load_wide(path):
    rows = {}
    for line in path.read_text().splitlines():
        name, api, profile, cap, seed, returned, error, invalid, count, cells = line.split('\t')
        rows[(name, api, profile, int(cap), int(seed))] = (int(returned), int(error), int(count), cells)
    return rows
baseline = load_wide(a.reference / 'results.tsv')
helper = load_wide(a.helper / 'results.tsv')
mismatches = []
for key, row in helper.items():
    expected = baseline[(key[0], 'legacy-va', key[2], key[3], key[4])]
    def normalized(value, windows):
        return (value[0], 'EILSEQ' if value[1] == (42 if windows else errno.EILSEQ) else value[1], *value[2:])
    if normalized(expected, True) != normalized(row, False):
        mismatches.append({'case': key, 'ucrt': expected, 'helper': row})
# Numeric rows also assert actual UCRT wide UTF16 and narrow fields match before
# emitting each reference record. Sequence comparison preserves duplicate inputs.
reference_numeric = (a.reference / 'numeric.tsv').read_text().splitlines()
helper_numeric = (a.numeric_helper / 'numeric.tsv').read_text().splitlines()
assert len(reference_numeric) == len(helper_numeric)
numeric_bad = [{'ucrt': expected, 'helper': actual} for expected, actual in zip(reference_numeric, helper_numeric)
               if expected != actual]
reference_bytes = (a.reference / 'bytes.tsv').read_text().splitlines()
helper_bytes = (a.helper / 'bytes.tsv').read_text().splitlines()
assert len(reference_bytes) == len(helper_bytes)
byte_bad = []
for expected, actual in zip(reference_bytes, helper_bytes):
    e = expected.split('\t')
    v = actual.split('\t')
    e[5] = 'EILSEQ' if e[5] == '42' else e[5]
    v[5] = 'EILSEQ' if v[5] == str(errno.EILSEQ) else v[5]
    if e != v:
        byte_bad.append({'ucrt': e, 'helper': v})
a.output.write_text(json.dumps({'byte_rows': len(helper_bytes), 'byte_mismatches': byte_bad, 'wide_rows': len(helper), 'wide_mismatches': mismatches,
    'numeric_rows': len(helper_numeric), 'numeric_mismatches': numeric_bad,
    'scope': 'exact output/status/errno and count target; Windows CRT invalid-handler callbacks are not a native API'},
    indent=2) + '\n')
print('bytes', len(helper_bytes), 'mismatches', len(byte_bad))
print('wide', len(helper), 'mismatches', len(mismatches), 'numeric', len(helper_numeric), 'mismatches', len(numeric_bad))
raise SystemExit(int(bool(mismatches or numeric_bad or byte_bad)))
