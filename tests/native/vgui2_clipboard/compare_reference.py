#!/usr/bin/env python3
"""Compare admitted byte/unit views to immutable actual Win32-method output."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--original', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
original = args.original.resolve()
art = args.art.resolve()
manifest = original / 'FINAL_SHA256.json'
assert hashlib.sha256(manifest.read_bytes()).hexdigest() == 'a571fa19866ca70e6bbeb725c72e7529ff3e1a147c21737716775ff311081d06'
files = json.loads(manifest.read_text())['files']
reference_path = original / 'windows/x86-Release.jsonl'
assert hashlib.sha256(reference_path.read_bytes()).hexdigest() == files['windows/x86-Release.jsonl']
reference = list(map(json.loads, reference_path.read_text().splitlines()))
results = []
paths = list((art / 'mac').glob('*-reference_rows.stdout'))
paths += list((art / 'windows').glob('*-reference_rows.stdout'))
assert len(paths) == 9, paths
for path in sorted(paths):
    matched = 0
    scalars = 0
    rows = list(map(json.loads, path.read_text().splitlines()))
    for row in rows:
        if row['kind'] == 'scalar':
            expected = chr(row['value']).encode('utf-16le') + b'\0\0'
            assert bytes.fromhex(row['bytes']) == expected, (path, row)
            scalars += 1
            continue
        expected = reference[row['reference_row']]
        if row['kind'] in ['raw', 'pack']:
            assert row['bytes'] == expected['bytes'], (path, row, expected)
            key = 'requested' if row['kind'] == 'pack' else 'result'
            assert row[key] == expected[key], (path, row, expected)
        elif row['kind'] == 'wide':
            raw = bytes.fromhex(expected['bytes'])
            cells = [int.from_bytes(raw[index:index + 2], 'little') for index in range(0, len(raw), 2)]
            assert row['cells'] == cells and row['result'] == expected['result'], (path, row, expected)
        elif row['kind'] == 'count':
            assert row['result'] == expected['result'], (path, row, expected)
        else:
            raise AssertionError(row)
        matched += 1
    assert matched == 14 and scalars in [0, 520], (path, matched, scalars)
    results.append({'context': str(path.relative_to(art)), 'actual_method_rows_matched': matched,
        'authored_native32_scalar_oracle_rows': scalars, 'physical_wchar_byte_parity_claimed': False})
(art / 'comparison.json').write_text(json.dumps({'results': results, 'admission':
    'Only valid normalized complete-cell/byte/explicit-pack cases; OS publication, acquisition and Windows defects excluded',
    'actual_clipboard_calls': 0, 'provider_or_lease_implemented': False}, indent=2) + '\n')
print('PASS 9 contexts x14 admitted actual-method rows; 3 Mac contexts x520 independent scalar UTF16LE oracles')
