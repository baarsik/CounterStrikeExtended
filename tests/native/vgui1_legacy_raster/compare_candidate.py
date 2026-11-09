"""Exact comparison; a mismatch is expected evidence, never a passing parity gate."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=Path, required=True)
parser.add_argument('--candidate', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
rows = []
for order in ('ascending', 'descending', 'sequence'):
    name = order + '-metrics.tsv'
    before = (args.reference / name).read_text().splitlines()[0]
    after = (args.candidate / name).read_text().splitlines()[0]
    name = order + '-abc.tsv'
    gdi = [line.split('\t') for line in (args.reference / name).read_text().splitlines()]
    candidate = [line.split('\t') for line in (args.candidate / name).read_text().splitlines()]
    assert len(gdi) == len(candidate)
    mapping = [int(x[1]) for x, y in zip(gdi, candidate) if x[2:4] != y[2:4]]
    abc = [{'ordinal': i, 'byte': int(x[1]), 'gdi': x[4:], 'candidate': y[4:]}
           for i, (x, y) in enumerate(zip(gdi, candidate)) if x[4:] != y[4:]]
    name = order + '-rgba.bin'
    a = (args.reference / name).read_bytes()
    b = (args.candidate / name).read_bytes()
    differing = sum(x != y for x, y in zip(a, b)) + abs(len(a) - len(b))
    first = next((index for index, (x, y) in enumerate(zip(a, b)) if x != y), None)
    rows.append({'order': order, 'metrics_equal': before == after, 'metrics_gdi': before,
        'metrics_candidate': after, 'mapping_mismatch_bytes': mapping, 'abc_mismatches': abc,
        'rgba_differing_bytes': differing, 'rgba_reference_bytes': len(a), 'rgba_candidate_bytes': len(b),
        'first_difference': None if first is None else {'offset': first, 'gdi': a[first], 'candidate': b[first]}})
args.output.write_text(json.dumps(rows, indent=2) + '\n')
print(json.dumps([{'order': row['order'], 'metrics_equal': row['metrics_equal'],
    'mapping_mismatches': len(row['mapping_mismatch_bytes']), 'abc_mismatches': len(row['abc_mismatches']),
    'rgba_differing_bytes': row['rgba_differing_bytes']} for row in rows], indent=2))
raise SystemExit(int(any(not row['metrics_equal'] or row['mapping_mismatch_bytes'] or row['abc_mismatches']
                         or row['rgba_differing_bytes'] for row in rows)))
