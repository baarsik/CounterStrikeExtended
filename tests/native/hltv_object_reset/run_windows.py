#!/usr/bin/env python3
"""Headless actual-class reset fixture, supplied SDK closure and MSVC environment only."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

CASES = ['constructor', 'address', 'allocation', 'frame-failure', 'delta-failure', 'both-failure', 'lifecycle']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--arch', choices=['x86', 'x64', 'arm64'], required=True)
    parser.add_argument('--phase', choices=['before', 'after'], required=True)
    parser.add_argument('--pilot', action='store_true')
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    shutil.copytree(options.input, options.output / 'input', dirs_exist_ok=True)
    inputs = options.output / 'input'
    sdk = inputs / 'src'
    compiler = Path(shutil.which('cl.exe')).resolve()
    receipt = {'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
               'arch': options.arch, 'phase': options.phase, 'compiles': [], 'whole_tus': [], 'cases': []}
    include_paths = ['HLTV/Core/private', 'HLTV/Core/private/public', 'HLTV/Core/private/common',
                     'HLTV/common', 'launcher/public', '.']
    flags = ['/nologo', '/std:c++14', '/W4', '/WX', '/EHsc', '/GR', '/fp:precise', '/DWIN32', '/D_WINDOWS',
             '/D_USRDLL', '/DNOMINMAX', '/D_CRT_SECURE_NO_WARNINGS', '/D_WINSOCK_DEPRECATED_NO_WARNINGS']
    flags += ['/I' + str(sdk / value) for value in include_paths] + ['/I' + str(inputs)]
    configurations = [('release', ['/O2', '/MD', '/DNDEBUG']), ('debug', ['/Od', '/RTC1', '/Zi', '/MDd', '/D_DEBUG'])]
    for configuration, extra in configurations[:1] if options.pilot else configurations:
        build = options.output / configuration
        build.mkdir(exist_ok=True)
        objects = []
        for name, source in [('buffer', sdk / 'HLTV/common/BitBuffer.cpp'), ('address', sdk / 'HLTV/common/NetAddress.cpp'),
                             ('byteorder', sdk / 'HLTV/common/byteorder.cpp'),
                             ('dispatch', inputs / 'virtual_calls.cpp'), ('fixture', inputs / 'reset_tests.cpp')]:
            obj = build / (name + '.obj')
            hooks = ['/FI' + str(inputs / 'allocation_hooks.h')] if name == 'buffer' else []
            command = [str(compiler), *flags, *extra, *hooks, '/c', str(source), '/Fo' + str(obj)]
            result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
            (build / (name + '-compile.log')).write_bytes(result.stdout + result.stderr)
            receipt['compiles'].append({'configuration': configuration, 'label': name, 'args': command,
                'exit': result.returncode, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
            if result.returncode:
                (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
                raise RuntimeError('actual-class fixture compile failed: ' + str(build / (name + '-compile.log')))
            objects.append(str(obj))
        binary = build / 'reset-tests.exe'
        command = [str(compiler), '/nologo', *extra, *objects, '/Fe:' + str(binary), '/link', '/WX']
        result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
        (build / 'link.log').write_bytes(result.stdout + result.stderr)
        if result.returncode:
            (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
            raise RuntimeError('actual-class fixture link failed: ' + str(build / 'link.log'))
        for case in CASES:
            run = subprocess.run([str(binary), case], capture_output=True, timeout=15)
            (build / (case + '.log')).write_bytes(run.stdout + run.stderr)
            expected_success = options.phase == 'after' or case in ['delta-failure', 'both-failure']
            receipt['cases'].append({'configuration': configuration, 'case': case, 'exit': run.returncode,
                'expected_success': expected_success, 'matched': (run.returncode == 0) == expected_success})
        if options.phase == 'after':
            production_flags = [flag for flag in flags if flag != '/std:c++14'] + ['/std:c++17']
            for name, source in [('Network', sdk / 'HLTV/Core/Network.cpp'),
                                 ('World', sdk / 'HLTV/Core/World.cpp'),
                                 ('NetAddress', sdk / 'HLTV/common/NetAddress.cpp')]:
                command = [str(compiler), *production_flags, *extra, '/c', str(source),
                           '/Fo' + str(build / (name + '-whole.obj'))]
                result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
                (build / (name + '-whole.log')).write_bytes(result.stdout + result.stderr)
                receipt['whole_tus'].append({'configuration': configuration, 'name': name, 'args': command,
                    'exit': result.returncode, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                    'qualification': 'standalone C++17 owning hltv_core compile profile; full TU, no linking'})
    (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
    assert all(row['matched'] for row in receipt['cases']), 'unexpected actual component outcomes'
    assert all(row['exit'] == 0 for row in receipt['whole_tus']), 'actual whole production TU compile failed'
    print('PASS', options.arch, options.phase, len(receipt['compiles']), 'objects', len(receipt['cases']), 'outcomes')


if __name__ == '__main__':
    main()
