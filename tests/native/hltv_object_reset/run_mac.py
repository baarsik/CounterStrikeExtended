#!/usr/bin/env python3
"""Run exact source-region reset fixtures using actual SDK types and implementations, without networking."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

from extract_regions import extract

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
CASES = ['constructor', 'address', 'allocation', 'frame-failure', 'delta-failure', 'both-failure', 'lifecycle']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--recipes', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--before', action='store_true')
    parser.add_argument('--pilot', action='store_true')
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    extract(options.source_root, options.output / 'generated_regions.h')
    recipe = next(row for row in json.loads(options.recipes.read_text()) if row['label'] == 'World-release')
    prefix = recipe['args'][:recipe['args'].index('-o')]
    profiles = [('arm64-release', 'arm64', ['-O2', '-DNDEBUG']),
                ('arm64-debug', 'arm64', ['-O0', '-g', '-D_DEBUG']),
                ('arm64-sanitizers', 'arm64', ['-O1', '-g', '-fsanitize=address,undefined',
                 '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']),
                ('x86_64-release', 'x86_64', ['-O2', '-DNDEBUG']),
                ('x86_64-debug', 'x86_64', ['-O0', '-g', '-D_DEBUG'])]
    receipts = {'compiles': [], 'cases': [], 'before_runtime_warning_policy':
                'Original strict whole-TU failures are retained separately. Runtime negative control omits global -Werror; warnings remain visible.' if options.before else 'actual target warnings retained'}
    for name, architecture, flags in profiles[:1] if options.pilot else profiles:
        output = options.output / name
        output.mkdir(exist_ok=True)
        common = [value for value in prefix if not value.startswith('-std=') and value not in ['-O2', '-O0']]
        common[common.index('-arch') + 1] = architecture
        if options.before:
            common.remove('-Werror')
        common += ['-std=c++14', '-I' + str(options.output), *flags]
        objects = []
        for label, source in [('buffer', options.source_root / 'src/HLTV/common/BitBuffer.cpp'),
                              ('address', options.source_root / 'src/HLTV/common/NetAddress.cpp'),
                              ('byteorder', ROOT / 'src/HLTV/common/byteorder.cpp'),
                              ('dispatch', HERE / 'virtual_calls.cpp'), ('fixture', HERE / 'reset_tests.cpp')]:
            obj = output / (label + '.o')
            extras = ['-include', str(HERE / 'allocation_hooks.h')] if label == 'buffer' else []
            command = common + extras + ['-c', str(source), '-o', str(obj)]
            result = subprocess.run(command, capture_output=True, timeout=120)
            (output / (label + '-compile.log')).write_bytes(shlex.join(command).encode() + b'\n\n' + result.stdout + result.stderr)
            receipts['compiles'].append({'profile': name, 'label': label, 'args': command,
                'exit': result.returncode, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
            assert result.returncode == 0, (output / (label + '-compile.log')).read_text()
            objects.append(str(obj))
        binary = output / 'reset-tests'
        command = ['/usr/bin/clang++', '-arch', architecture, *flags, *objects, '-o', str(binary)]
        result = subprocess.run(command, capture_output=True, timeout=120)
        (output / 'link.log').write_bytes(shlex.join(command).encode() + b'\n\n' + result.stdout + result.stderr)
        assert result.returncode == 0, (output / 'link.log').read_text()
        for case in CASES:
            run = subprocess.run([str(binary), case], capture_output=True, timeout=10)
            (output / (case + '.log')).write_bytes(run.stdout + run.stderr)
            expected_success = not options.before or case in ['constructor', 'delta-failure', 'both-failure']
            matched = (run.returncode == 0) == expected_success
            receipts['cases'].append({'profile': name, 'case': case, 'exit': run.returncode,
                                      'expected_success': expected_success, 'matched': matched})
            print(name, case, run.returncode, 'matched', matched, flush=True)
        (options.output / 'receipt.json').write_text(json.dumps(receipts, indent=2))
    assert all(row['matched'] for row in receipts['cases']), 'unexpected actual-type outcome; retain all logs'


if __name__ == '__main__':
    main()
