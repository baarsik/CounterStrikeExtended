#!/usr/bin/env python3
"""Actual-body value admission against the immutable original Win32 method output."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ORIGINAL_MANIFEST = '4b479bc64e734149cc59ca39f579a79e8f81ad1753d15a2419a647838846503b'
parser = argparse.ArgumentParser()
parser.add_argument('--original', type=Path, required=True)
parser.add_argument('--candidate', type=Path, required=True)
parser.add_argument('--result', type=Path, required=True)
args = parser.parse_args()
original = args.original.resolve()
candidate = args.candidate.resolve()
manifest = original / 'FINAL_SHA256.json'
assert hashlib.sha256(manifest.read_bytes()).hexdigest() == ORIGINAL_MANIFEST
files = json.loads(manifest.read_text())['files']

def original_rows(path):
    assert hashlib.sha256(path.read_bytes()).hexdigest() == files[str(path.relative_to(original))]
    return list(map(json.loads, path.read_text().splitlines()))

results = []
failures = 0
for cpu in ['x86', 'x64', 'arm64']:
    for mode in ['Release', 'Debug']:
        context = cpu + '-' + mode
        differences = []
        fp_differences = 0
        count = 0
        for reference in sorted((original / 'windows').glob('x86-Release-*.jsonl')):
            tag = reference.name[len('x86-Release-'):]
            target = candidate / (context + '-' + tag)
            expected_rows = original_rows(reference)
            prior_rows = original_rows(original / 'windows' / (context + '-' + tag))
            actual_rows = list(map(json.loads, target.read_text().splitlines()))
            assert len(expected_rows) == len(actual_rows) == len(prior_rows), (target, 'row count')
            for expected, actual, prior in zip(expected_rows, actual_rows, prior_rows):
                for key in ['row', 'counter', 'seconds_bits', 'millis']:
                    if expected[key] != actual[key]:
                        differences.append({'scenario': tag, 'row': expected['row'], 'key': key,
                            'expected': expected[key], 'actual': actual[key]})
                fp_keys = ['seconds_flags', 'millis_flags', 'seconds_status', 'millis_status']
                fp_differences += any(prior[key] != actual[key] for key in fp_keys)
                assert all(prior[key] == actual[key] for key in ['control', 'simd_control', 'rounding']), target
                count += 1
        failures += len(differences)
        results.append({'context': context, 'rows': count, 'value_differences_from_original_Win32': differences,
            'status_differences_from_original_same_CPU_profile': fp_differences,
            'observed_control_fields_match_original': True})
        print(context, 'rows=' + str(count), 'value_differences=' + str(len(differences)),
            'status_differences=' + str(fp_differences))
args.result.write_text(json.dumps({'original_manifest_sha256': ORIGINAL_MANIFEST,
    'value_gate_passed': failures == 0, 'full_FP_state_equivalence': False, 'results': results}, indent=2) + '\n')
sys.exit(1 if failures else 0)
