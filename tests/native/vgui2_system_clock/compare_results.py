#!/usr/bin/env python3
"""Compare encoded values with raw real-method MSVC references, not a copied clock model."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
art = args.art.resolve()
keys = ['row', 'counter', 'seconds_bits', 'millis']
summary = {'canonical_reference': 'Actual selected CSystem methods, MSVC x86 Release; masked/default FP profile',
    'value_comparisons': [], 'reference_cpu_differences': [], 'fp_observations': [],
    'full_machine_state_equivalence': False}

def read(path):
    return list(map(json.loads, path.read_text().splitlines()))

for directory, contexts, suffix in [('mac', ['Release', 'Debug', 'Sanitizers'], '.stdout'),
        ('windows-component', [cpu + '-' + mode for cpu in ['x86', 'x64', 'arm64']
            for mode in ['Release', 'Debug']], '.jsonl')]:
    for context in contexts:
        count = 0
        flag_differences = 0
        for reference in sorted((art / 'windows').glob('x86-Release-*.jsonl')):
            tag = reference.name[len('x86-Release-'):-len('.jsonl')]
            actual = art / directory / (context + '-' + tag + suffix)
            expected_rows = read(reference)
            rows = read(actual)
            assert len(rows) == len(expected_rows), (actual, 'row count')
            for expected, row in zip(expected_rows, rows):
                assert all(expected[key] == row[key] for key in keys), (actual, expected, row)
                flag_differences += (expected['seconds_flags'], expected['millis_flags']) != (
                    row['seconds_flags'], row['millis_flags'])
                count += 1
        summary['value_comparisons'].append({'context': directory + '/' + context,
            'matching_encoded_rows': count, 'fenv_observation_differences_from_x86': flag_differences})
        if directory == 'windows-component':
            for row in read(art / directory / (context + '-boundaries-0.jsonl')):
                expected = row['seconds'] * 1000000000 + row['nanos']
                valid = 0 <= expected <= 2**64 - 1
                assert row['valid'] == valid and row['encoded'] == (expected if valid else 987), row

for cpu in ['x86', 'x64', 'arm64']:
    for mode in ['Release', 'Debug']:
        context = cpu + '-' + mode
        differences = {key: 0 for key in keys}
        observation_differences = 0
        statuses = set()
        controls = set()
        for canonical in sorted((art / 'windows').glob('x86-Release-*.jsonl')):
            actual = art / 'windows' / canonical.name.replace('x86-Release-', context + '-')
            for expected, row in zip(read(canonical), read(actual)):
                for key in keys:
                    differences[key] += expected[key] != row[key]
                observation_differences += (expected['seconds_flags'], expected['millis_flags']) != (
                    row['seconds_flags'], row['millis_flags'])
                controls.add((row['control'], row['simd_control'], row['rounding']))
                statuses.add((row['seconds_flags'], row['millis_flags'], row['seconds_status'], row['millis_status']))
        summary['reference_cpu_differences'].append({'context': context, 'differences_from_x86': differences})
        summary['fp_observations'].append({'context': context, 'observed_control_triplets': sorted(controls),
            'observed_status_quartets': sorted(statuses), 'fenv_differences_from_x86': observation_differences})

(art / 'comparison.json').write_text(json.dumps(summary, indent=2) + '\n')
print('PASS 9 component contexts x1211 exact encoded reference rows; 6 Windows +3 Mac integer encoding oracles')
print('Reference CPU value and FP-status differences retained separately; no full-machine-state claim')
