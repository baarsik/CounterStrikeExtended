#!/usr/bin/env python3
"""Check compiler-reported physical slots; names identify typed schema selectors, not hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import re

METHODS = {
    'IEngineAPI': ['Run'],
    'IDedicatedServerAPI': ['Init', 'Shutdown', 'RunFrame', 'AddConsoleText', 'UpdateStatus'],
    'IDedicatedExports': ['Sys_Printf']}


def facts(text, microsoft):
    # MSVC can print an include trace inside its buffered class dump.
    text = re.sub(r'Note: including file:[^\n]*\n', '', text)
    found = []
    if microsoft:
        pattern = r'(?P<type>[\w:]+)::\$vftable@:\n(?P<body>.*?)(?=\n\n)'
    else:
        pattern = r"VTable indices for '(?P<type>[\w:]+)' \(\d+ entries\)\.\n(?P<body>.*?)(?=\n\n)"
    for match in re.finditer(pattern, text, re.S):
        qualified = match['type']
        name = qualified.split('::')[-1]
        if name not in METHODS:
            continue
        slots = []
        for line in match['body'].splitlines():
            item = re.match(r'\s*(\d+)\s*\|\s*(.*)', line)
            if not item:
                continue
            member = item[2]
            if microsoft:
                method = member.split('::')[-1].strip()
                if '{dtor}' in method:
                    method = 'destructor'
            else:
                method = member.split('::')[-1].split('(')[0]
                if method.startswith('~'):
                    method = 'destructor'
            slots.append({'slot': int(item[1]), 'method': method})
        found.append({'type': qualified, 'slots': slots})
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--profile', choices=['microsoft', 'itanium'], required=True)
    parser.add_argument('--expect-reject', action='store_true')
    parser.add_argument('--stamp', type=Path)
    options = parser.parse_args()
    destructor_slots = 1 if options.profile == 'microsoft' else 2
    if options.stamp:
        options.stamp.unlink(missing_ok=True)
    receipt = json.loads((options.directory / 'receipt.json').read_text())
    assert all(row['exit'] == 0 for row in receipt), 'A failed owning compile cannot supply slot evidence'
    expected_contexts = {
        'hw_engine': set(METHODS), 'swds': set(METHODS), 'producer': set(METHODS),
        'launcher': {'IEngineAPI'},
        'hlds': {'IDedicatedServerAPI', 'IDedicatedExports'},
        'dedicated': {'IDedicatedServerAPI', 'IDedicatedExports'}}
    result = []
    for entry in receipt:
        label = entry.get('target', entry.get('case'))
        path = options.directory / (label + '.log')
        current = facts(path.read_text(errors='replace'), options.profile == 'microsoft')
        names = [row['type'].split('::')[-1] for row in current]
        assert set(names) == expected_contexts[label] and len(names) == len(set(names)), (label, names)
        for row in current:
            name = row['type'].split('::')[-1]
            expected = ['destructor'] * destructor_slots + METHODS[name]
            row['source_log'] = str(path)
            row['expected_order'] = expected
            row['matches'] = row['slots'] == [{'slot': index, 'method': method}
                                              for index, method in enumerate(expected)]
            result.append(row)
    assert result, 'No actual interface vtables were reported'
    (options.directory / 'physical-slot-check.json').write_text(json.dumps(result, indent=2))
    mismatches = [row for row in result if not row['matches']]
    assert bool(mismatches) == options.expect_reject, mismatches
    if options.stamp and not mismatches:
        options.stamp.parent.mkdir(parents=True, exist_ok=True)
        options.stamp.write_text(json.dumps({
            'profile': options.profile, 'physical_slots': result,
            'compiler_receipt_sha256': hashlib.sha256((options.directory / 'receipt.json').read_bytes()).hexdigest(),
            'purpose': 'component prototype of fail-closed owning-build prerequisite; not production wiring'}, indent=2))
    print(('REJECTED negative control' if mismatches else 'MATCHED actual physical slots'),
          len(result), 'interface views')


if __name__ == '__main__':
    main()
