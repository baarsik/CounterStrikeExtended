#!/usr/bin/env python3
"""Inventory retained direct smoke calls, preserving complete original argument text."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]


def arguments(text, opening):
    depth = 1
    quote = None
    escaped = False
    parts = []
    start = opening + 1
    for offset in range(start, len(text)):
        char = text[offset]
        if quote:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in ('"', "'"):
            quote = char
        elif char in '([{':
            depth += 1
        elif char in ')]}':
            depth -= 1
            if depth == 0:
                parts.append(text[start:offset].strip())
                return parts, offset + 1
        elif char == ',' and depth == 1:
            parts.append(text[start:offset].strip())
            start = offset + 1
    raise RuntimeError('Truncated retained call')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    records = []
    declarations = []
    source_hashes = {}
    for path in sorted((ROOT / 'src').rglob('*')):
        if path.suffix not in ('.h', '.cpp', '.c', '.def') or not path.is_file():
            continue
        raw = path.read_bytes()
        if b'EV_HLDM_CreateSmoke' not in raw:
            continue
        text = raw.decode('cp1252')
        source_hashes[str(path.relative_to(ROOT))] = hashlib.sha256(raw).hexdigest()
        for match in re.finditer(r'\bEV_HLDM_CreateSmoke\s*\(', text):
            values, end = arguments(text, text.index('(', match.start()))
            row = {'path': str(path.relative_to(ROOT)), 'line': text.count('\n', 0, match.start()) + 1,
                   'arguments': values}
            if text[end:].lstrip().startswith('{') or values[0].startswith('float*'):
                declarations.append(row)
                continue
            owners = list(re.finditer(r'^(?:extern "C" )?void (\w+)\s*\(', text[:match.start()], re.M))
            row['function'] = owners[-1][1] if owners else None
            assert len(values) == 11
            row['smoke_type'] = int(values[7])
            records.append(row)
    result = {'direct_calls': records, 'declarations': declarations,
              'type_counts': dict(Counter(row['smoke_type'] for row in records)),
              'function_counts': dict(Counter(row['function'] for row in records)), 'source_sha256': source_hashes,
              'limit': 'Retained lexical calls only; does not prove arbitrary external callers absent at runtime'}
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(result, indent=2))
    print('calls', len(records), result['type_counts'], 'functions', len(result['function_counts']))


if __name__ == '__main__':
    main()
