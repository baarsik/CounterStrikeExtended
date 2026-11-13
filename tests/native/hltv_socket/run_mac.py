#!/usr/bin/env python3
"""Compile real NetSocket and dependencies with typed fake socket calls; never create a real socket."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
SHARED = ['send-zero-count', 'send', 'send-short', 'send-long', 'send-max',
          'send-error-wouldblock', 'send-error-reset', 'send-error-refused', 'send-error-unavailable',
          'send-error-large', 'send-error-interrupt', 'send-error-split-wouldblock',
          'receive-zero', 'receive-short', 'receive-drop', 'receive-wouldblock', 'receive-reset',
          'receive-refused', 'receive-large', 'receive-interrupt', 'queue', 'copy-flush', 'channel', 'split']
NATIVE = ['count-zero', 'count-max', 'count-overflow', 'count-wide', 'count-native-max', 'count-error', 'count-invalid-negative',
          'count-receive-overflow', 'count-direct-overflow', 'count-split-overflow',
          'failure-1', 'failure-2', 'failure-3', 'failure-4', 'failure-5', 'failure-6',
          'fd-zero', 'live-create', 'close', 'options', 'cleanup-close-error', 'close-error', 'ttl-nonsense', 'ttl-0', 'ttl-255', 'ttl-32', 'ttl-  +7tail',
          'ttl--1', 'ttl-256', 'ttl-4294967296', 'ttl-999999999999999999999999999999999']
BEFORE_PASSES = set(SHARED + ['count-zero', 'count-max', 'count-error'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=ROOT / 'src/HLTV/Core/NetSocket.cpp')
    parser.add_argument('--recipes', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--before', action='store_true')
    parser.add_argument('--pilot', action='store_true')
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    entry = next(row for row in json.loads(options.recipes.read_text()) if row['label'] == 'NetSocket-release')
    prefix = entry['args'][:entry['args'].index('-o')]
    profiles = [('arm64-release', 'arm64', ['-O2', '-DNDEBUG']),
                ('arm64-debug', 'arm64', ['-O0', '-g', '-D_DEBUG']),
                ('arm64-sanitizers', 'arm64', ['-O1', '-g', '-fsanitize=address,undefined',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']),
                ('x86_64-release', 'x86_64', ['-O2', '-DNDEBUG']),
                ('x86_64-debug', 'x86_64', ['-O0', '-g', '-D_DEBUG'])]
    receipt = {'compiles': [], 'cases': [], 'network': 'all socket/DNS entry points are typed controlled functions; no real endpoints'}
    sources = [('socket', options.source), ('network', ROOT / 'src/HLTV/Core/Network.cpp'),
               ('base', ROOT / 'src/HLTV/Core/private/common/BaseSystemModule.cpp'),
               ('buffer', ROOT / 'src/HLTV/common/BitBuffer.cpp'),
               ('address', ROOT / 'src/HLTV/common/NetAddress.cpp'),
               ('byteorder', ROOT / 'src/HLTV/common/byteorder.cpp'),
               ('common', ROOT / 'src/HLTV/common/common.cpp'),
               ('list', ROOT / 'src/common/ObjectList.cpp'),
               ('tokens', ROOT / 'src/common/TokenLine.cpp'), ('fixture', HERE / 'socket_tests.cpp')]
    for name, architecture, flags in profiles[:1] if options.pilot else profiles:
        output = options.output / name
        output.mkdir(exist_ok=True)
        common = [value for value in prefix if not value.startswith('-std=') and value not in ['-O2', '-O0']]
        common[common.index('-arch') + 1] = architecture
        if options.before:
            common = [value for value in common if not value.startswith('-Werror')]
        common += ['-std=c++14', '-I' + str(HERE), *flags]
        objects = []
        for label, source in sources:
            obj = output / (label + '.o')
            extra = ['-include', str(HERE / 'syscalls.h')] if label in ['socket', 'network'] else []
            command = common + extra + ['-c', str(source), '-o', str(obj)]
            result = subprocess.run(command, capture_output=True, timeout=120)
            (output / (label + '-compile.log')).write_bytes(shlex.join(command).encode() + b'\n\n' + result.stdout + result.stderr)
            receipt['compiles'].append({'profile': name, 'label': label, 'args': command, 'exit': result.returncode,
                                        'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
            assert result.returncode == 0, (output / (label + '-compile.log')).read_text()
            objects.append(str(obj))
        binary = output / 'socket-tests'
        command = ['/usr/bin/clang++', '-arch', architecture, *flags, *objects, '-o', str(binary)]
        result = subprocess.run(command, capture_output=True, timeout=120)
        (output / 'link.log').write_bytes(shlex.join(command).encode() + b'\n\n' + result.stdout + result.stderr)
        assert result.returncode == 0, (output / 'link.log').read_text()
        for case in SHARED + NATIVE:
            run = subprocess.run([str(binary), case], capture_output=True, timeout=10)
            (output / (str((SHARED + NATIVE).index(case)) + '.log')).write_bytes(run.stdout + run.stderr)
            expected = not options.before or case in BEFORE_PASSES
            receipt['cases'].append({'profile': name, 'case': case, 'exit': run.returncode,
                                     'expected_success': expected, 'matched': (run.returncode == 0) == expected})
            if options.pilot or (run.returncode == 0) != expected:
                print(name, case, run.returncode, 'matched', (run.returncode == 0) == expected, flush=True)
        print(name, len(SHARED) + len(NATIVE), 'case outcomes recorded', flush=True)
        (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
    assert all(row['matched'] for row in receipt['cases']), 'unexpected actual-source outcomes; logs retained'


if __name__ == '__main__':
    main()
