#!/usr/bin/env python3
"""Check Mach-O game exports against a retained Win32 dumpbin export report."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import subprocess

from binary_identity import require_architecture


def read_reference(text):
    names = re.findall(r'^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(\S+)', text, re.MULTILINE)
    if not names:
        raise ValueError('No reference exports found in the dumpbin report')
    if len(names) != len(set(names)):
        raise ValueError('Duplicate names in the reference export report')
    return names


def check_exports(reference, symbols, demangled):
    symbols = set(symbols)
    missing_c = {'_' + name for name in reference if not name.startswith('?')} - symbols
    required_cpp = Counter()
    for name in reference:
        if name.startswith('?'):
            match = re.match(r'\?([^@]+)@([^@]+)@', name)
            if not match:
                raise ValueError('Unsupported reference C++ export: ' + name)
            required_cpp[match[1] + '@' + match[2]] += 1
    actual_cpp = Counter()
    for name in demangled:
        match = re.match(r'^(\w+)::(\w+)\(', name)
        if match:
            actual_cpp[match[2] + '@' + match[1]] += 1
    missing_cpp = required_cpp - actual_cpp
    if missing_c or missing_cpp:
        missing = sorted(missing_c) + [f'{name} ({count})' for name, count in sorted(missing_cpp.items())]
        raise ValueError('Missing reference exports: ' + ', '.join(missing))
    return {'retained_c': sum(not name.startswith('?') for name in reference),
            'retained_cpp': sum(required_cpp.values())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True, help='Win32 dumpbin /nologo /exports output')
    parser.add_argument('--library', type=Path, required=True, help='Owned rebuilt ARM64 Mach-O game library')
    arguments = parser.parse_args()
    data = arguments.library.read_bytes()
    identity = require_architecture(data, 'arm64', maximum_macos=(14, 0, 0))
    if identity['format'] != 'mach-o':
        raise ValueError('The game library must be a thin Mach-O image')
    reference = arguments.reference.read_bytes()
    exports = subprocess.check_output(['nm', '-gjU', str(arguments.library)], text=True)
    demangled = subprocess.check_output(['c++filt'], input=exports, text=True)
    result = check_exports(read_reference(reference.decode('utf-8-sig')), exports.splitlines(), demangled.splitlines())
    print(json.dumps({'scope': 'binary architecture and export inventory only', **identity, **result,
                      'library_sha256': hashlib.sha256(data).hexdigest(),
                      'reference_sha256': hashlib.sha256(reference).hexdigest()}, indent=2))


if __name__ == '__main__':
    main()
